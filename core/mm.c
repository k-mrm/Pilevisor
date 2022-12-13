#include "aarch64.h"
#include "tlb.h"
#include "mm.h"
#include "memory.h"
#include "param.h"
#include "panic.h"

u64 *vmm_pagetable;
static int root_level;

void set_ttbr0_el2(u64 *ttbr0_el2);

u64 *pagewalk(u64 *pgt, u64 va, int root, int create) {
  for(int level = root; level < 3; level++) {
    u64 *pte = &pgt[PIDX(level, va)];

    if((*pte & PTE_VALID) && (*pte & PTE_TABLE)) {
      pgt = (u64 *)PTE_PA(*pte);
    } else if(create) {
      pgt = alloc_page();
      if(!pgt)
        panic("nomem");

      *pte = PTE_PA(pgt) | PTE_TABLE | PTE_VALID;
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

static void memmap_huge(u64 pa, u64 va, u64 memflags, u64 size) {
  u64 *hugepte;
  u64 *pgt = vmm_pagetable;

  if(pa % size != 0 || va % size != 0)
    panic("align");

  int level = hugepage_level(size);
  if(!level)
    return;

  for(int lv = root_level; lv < level; lv++) {
    u64 *pte = &pgt[PIDX(lv, va)];

    if((*pte & PTE_VALID) && (*pte & PTE_TABLE)) {
      pgt = (u64 *)PTE_PA(*pte);
    } else {
      pgt = alloc_page();
      if(!pgt)
        panic("nomem");

      *pte = PTE_PA(pgt) | PTE_TABLE | PTE_VALID;
    }
  }

  hugepte = &pgt[PIDX(level, va)];

  if((*hugepte & PTE_TABLE) || (*hugepte & PTE_AF))
    panic("already mapped: %p\n", va);

  *hugepte |= PTE_PA(pa) | PTE_AF | PTE_VALID | memflags;
}

/*
 *  mapping device memory
 */
void *iomap(u64 pa, u64 size) {
  u64 va = pa;
  u64 memflags = PTE_DEVICE_nGnRE | PTE_RW | PTE_XN;

  size = PAGE_ALIGN(size);

  for(u64 p = 0; p < size; p += PAGESIZE) {
    u64 *pte = pagewalk(vmm_pagetable, va + p, root_level, 1);

    if(*pte & PTE_AF) {
      if((*pte & PTE_DEVICE_nGnRE) == PTE_DEVICE_nGnRE) {
        continue;
      } else {
        panic("bad iomap");
      }
    } else {
      *pte = PTE_PA(pa + p) | PTE_AF | PTE_V | memflags;
    }
  }

  return (void *)va;
}

static void map_fdt_early(u64 fdt_base) {
  u64 memflags = PTE_NORMAL | PTE_SH_INNER | PTE_RO | PTE_XN;

  /* map 2MB */
  memmap_huge(fdt_base, fdt_base, memflags, 0x200000);
}

void setup_pagetable(u64 fdt_base) {
  root_level = 1;

  vmm_pagetable = alloc_page();

  u64 start = PAGE_ADDRESS(vmm_start);
  u64 end = (u64)PHYEND;
  u64 memflags;

  for(u64 p = start; p < end; p += PAGESIZE) {
    memflags = PTE_NORMAL | PTE_SH_INNER;

    if(!is_vmm_text(p))
      memflags |= PTE_XN;

    if(is_vmm_text(p) || is_vmm_rodata(p))
      memflags |= PTE_RO;

    /* make 1:1 map */
    __pagemap(p, p, memflags);
  }

  map_fdt_early(fdt_base);

  set_ttbr0_el2(vmm_pagetable);
}
