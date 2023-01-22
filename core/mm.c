#include "aarch64.h"
#include "tlb.h"
#include "mm.h"
#include "memory.h"
#include "param.h"
#include "memlayout.h"
#include "panic.h"
#include "lib.h"
#include "earlycon.h"

u64 *vmm_pagetable;
static int root_level;

u64 pvoffset;

extern char __boot_pgt_l1[];

void set_ttbr0_el2(u64 ttbr0_el2);

u64 *pagewalk(u64 *pgt, u64 va, int root, int create) {
  for(int level = root; level < 3; level++) {
    u64 *pte = &pgt[PIDX(level, va)];

    if((*pte & PTE_VALID) && (*pte & PTE_TABLE)) {
      pgt = (u64 *)PTE_PA(*pte);
    } else if(create) {
      pgt = alloc_page();
      if(!pgt)
        panic("nomem");

      *pte = PTE_PA(V2P(pgt)) | PTE_TABLE | PTE_VALID;
    } else {
      /* unmapped */
      return NULL;
    }
  }

  return &pgt[PIDX(3, va)];
}

static void __memmap(u64 pa, u64 va, u64 size, u64 memflags) {
  if(!PAGE_ALIGNED(pa) || !PAGE_ALIGNED(va) || size % PAGESIZE != 0)
    panic("__memmap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE, pa += PAGESIZE) {
    u64 *pte = pagewalk(vmm_pagetable, va, root_level, 1);

    if(*pte & PTE_AF)
      panic("this entry has been used: %p", va);

    *pte = PTE_PA(pa) | PTE_AF | PTE_V | memflags;
  }
}

static void __pagemap(u64 pa, u64 va, u64 memflags) {
  u64 *pte = pagewalk(vmm_pagetable, va, root_level, 1);
  if(*pte & PTE_AF)
      panic("this entry has been used: %p", va);

  *pte = PTE_PA(pa) | PTE_AF | PTE_V | memflags;
}

static inline int hugepage_level(u64 size) {
  switch(size) {
    case BLOCKSIZE_L2:    /* 2 MB */
      return 2;
    case BLOCKSIZE_L1:    /* 1 GB */
      return 1;
  }

  return 0;
}

static int memmap_huge(u64 pa, u64 va, u64 memflags, u64 size) {
  u64 *hugepte;
  u64 *pgt = vmm_pagetable;

  if(pa % size != 0 || va % size != 0)
    return -1;

  int level = hugepage_level(size);
  if(!level)
    return -1;

  for(int lv = root_level; lv < level; lv++) {
    u64 *pte = &pgt[PIDX(lv, va)];

    if((*pte & PTE_VALID) && (*pte & PTE_TABLE)) {
      pgt = (u64 *)PTE_PA(*pte);
    } else {
      pgt = alloc_page();
      if(!pgt)
        panic("nomem");

      *pte = PTE_PA(V2P(pgt)) | PTE_TABLE | PTE_VALID;
    }
  }

  hugepte = &pgt[PIDX(level, va)];

  if((*hugepte & PTE_TABLE) || (*hugepte & PTE_AF))
    return -1;

  *hugepte |= PTE_PA(pa) | PTE_AF | PTE_VALID | memflags;

  return 0;
}

/*
 *  mapping device memory
 */
void *iomap(u64 pa, u64 size) {
  u64 memflags = PTE_DEVICE_nGnRE | PTE_XN;

  size = PAGE_ALIGN(size);

  void *va = iomalloc(pa, size);

  for(u64 p = 0; p < size; p += PAGESIZE) {
    u64 *pte = pagewalk(vmm_pagetable, (u64)va + p, root_level, 1);
    if(*pte & PTE_AF) {
      panic("bad iomap");
    }
    
    *pte = PTE_PA(pa + p) | PTE_AF | PTE_V | memflags;
  }

  return (void *)va;
}

static void *map_fdt_early(u64 fdt_base) {
  u64 memflags = PTE_NORMAL | PTE_SH_INNER | PTE_RO | PTE_XN;
  u64 offset;
  int rc;

  offset = fdt_base & (BLOCKSIZE_L2 - 1);
  fdt_base = fdt_base & ~(BLOCKSIZE_L2 - 1);
  /* map 2MB */
  rc = memmap_huge(fdt_base, FDT_SECTION_BASE, memflags, 0x200000);
  if(rc < 0)
    return NULL;

  return (void *)(FDT_SECTION_BASE + offset);
}

static u64 hva2pa(u64 *pgt, u64 hva) {
  u64 *pte = pagewalk(pgt, hva, root_level, 0);
  if(!pte)
    return 0;

  return PTE_PA(*pte);
}

static void remap_kernel() {
  u64 start_phys = hva2pa((u64 *)__boot_pgt_l1, VMM_SECTION_BASE);
  u64 size = (u64)__earlymem_end - (u64)vmm_start;
  u64 memflags;

  pvoffset = VMM_SECTION_BASE - start_phys;

  for(u64 i = 0; i < size; i += PAGESIZE) {
    u64 p = start_phys + i;
    u64 v = VMM_SECTION_BASE + i;

    memflags = PTE_NORMAL | PTE_SH_INNER;

    if(!is_vmm_text(v))
      memflags |= PTE_XN;

    if(is_vmm_text(v) || is_vmm_rodata(v))
      memflags |= PTE_RO;

    __pagemap(p, v, memflags);
  }
}

static void remap_earlycon() {
  u64 flags = PTE_DEVICE_nGnRE | PTE_XN;

  __pagemap(EARLY_PL011_BASE, EARLY_PL011_BASE, flags);
}

static void map_memory(void *fdt) {
  return;
}

void *setup_pagetable(u64 fdt_base) {
  void *virt_fdt;

  root_level = 1;

  vmm_pagetable = alloc_page();
  memset(vmm_pagetable, 0, PAGESIZE);

  remap_kernel();
  remap_earlycon();

  virt_fdt = map_fdt_early(fdt_base);

  map_memory(virt_fdt);

  set_ttbr0_el2(V2P(vmm_pagetable));

  return virt_fdt;
}
