#include "aarch64.h"
#include "tlb.h"
#include "mm.h"
#include "memory.h"
#include "param.h"
#include "memlayout.h"
#include "panic.h"
#include "lib.h"
#include "earlycon.h"
#include "iomem.h"
#include "device.h"
#include "printf.h"
#include "compiler.h"
#include "assert.h"
#include "fdt.h"

u64 vmm_pagetable[512] __aligned(4096);

extern u64 __boot_pgt_l1[];

static u64 earlymem_pgt_l2[512] __aligned(4096);
static u64 bootfdt_pgt_l2[512] __aligned(4096);
static u64 bootfdt_pgt_l2_2[512] __aligned(4096);

static int root_level;
static u64 early_phys_start, early_phys_end, early_memsize;

u64 pvoffset;

void set_ttbr0_el2(u64 ttbr0_el2);
static void pgt_set_block(u64 *pe, u64 phys, u64 memflags);

u64 *pagewalk(u64 *pgt, u64 va, int root, int create) {
  for(int level = root; level < 3; level++) {
    u64 *pte = &pgt[PIDX(level, va)];

    if((*pte & PTE_VALID) && (*pte & PTE_TABLE)) {
      pgt = P2V(PTE_PA(*pte));
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

static void __memmap(u64 va, u64 pa, u64 size, u64 memflags) {
  if(!PAGE_ALIGNED(pa) || !PAGE_ALIGNED(va) || size % PAGESIZE != 0)
    panic("__memmap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE, pa += PAGESIZE) {
    u64 *pte = pagewalk(vmm_pagetable, va, root_level, 1);

    if(*pte & PTE_AF)
      panic("this entry has been used: %p", va);

    *pte = PTE_PA(pa) | PTE_AF | PTE_V | memflags;
  }
}

static void __pagemap(u64 va, u64 pa, u64 memflags) {
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

static int memmap_huge(u64 va, u64 pa, u64 memflags, u64 size) {
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
      pgt = P2V(PTE_PA(*pte));
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

  pgt_set_block(hugepte, pa, memflags);

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

void dump_par_el1(void) {
  u64 par = read_sysreg(par_el1);

  if(par & 1) {
    printf("translation fault\n");
    printf("FST : %p\n", (par >> 1) & 0x3f);
    printf("PTW : %p\n", (par >> 8) & 1);
    printf("S   : %p\n", (par >> 9) & 1);
  } else {
    printf("address: %p\n", par);
  }
} 

u64 at_hva2pa(u64 hva) {
  u64 tmp = read_sysreg(par_el1);

  asm volatile ("at s1e2r, %0" :: "r"(hva));

  u64 par = read_sysreg(par_el1);

  write_sysreg(par_el1, tmp);

  if(par & 1) {
    return 0;
  } else {
    return (par & 0xfffffffff000ul) | PAGE_OFFSET(hva);
  }
}

static void *remap_fdt(u64 fdt_phys) {
  u64 memflags = PTE_NORMAL | PTE_SH_INNER | PTE_RO | PTE_XN;
  u64 offset, size, fdt_base;
  int rc;
  void *fdt;

  offset = fdt_phys & (BLOCKSIZE_L2 - 1);
  fdt_base = fdt_phys & ~(BLOCKSIZE_L2 - 1);
  /* map 2MB */
  rc = memmap_huge(FDT_SECTION_BASE, fdt_base, memflags, 0x200000);
  if(rc < 0)
    return NULL;

  fdt = (void *)(FDT_SECTION_BASE + offset);

  if(fdt_magic(fdt) != FDT_MAGIC)
    return NULL;

  if(fdt_version(fdt) != 17)
    return NULL;

  size = fdt_totalsize(fdt);

  return fdt;
}

static void pgt_set_table_entry(u64 *pe, u64 next_table_phys) {
  if(!pe)
    return;
  
  *pe = PTE_PA(next_table_phys) | PTE_TABLE | PTE_VALID;
}

static void pgt_set_block(u64 *pe, u64 phys, u64 memflags) {
  if(!pe)
    return;

  *pe = phys | memflags | PTE_AF | PTE_VALID;
}

void *early_fdt_map(void *fdt_phys) {
  // TODO
  return fdt_phys;
}

void early_map_earlymem(u64 pstart, u64 pend) {
  u64 size = pend - pstart;
  u64 *epmd, *epud;
  u64 vstart = pstart + VIRT_BASE;

  assert(pstart % SZ_2MiB == 0);
  assert(pend % SZ_2MiB == 0);
  assert(size % SZ_2MiB == 0);

  early_phys_start = pstart; 
  early_phys_end = pend;
  early_memsize = size;

  epud = &__boot_pgt_l1[PIDX(1, vstart)];
  pgt_set_table_entry(epud, V2P(earlymem_pgt_l2));

  /* map earlymem */
  for(u64 i = 0; i < size; i += SZ_2MiB) {
    epmd = &earlymem_pgt_l2[PIDX(2, vstart + i)];
    pgt_set_block(epmd, pstart + i, PTE_NORMAL | PTE_SH_INNER);
  }
}

static void remap_image() {
  u64 start_phys = at_hva2pa(VMM_SECTION_BASE);
  u64 size = (u64)vmm_end - (u64)vmm_start;
  u64 memflags, i;

  for(i = 0; i < size; i += PAGESIZE) {
    u64 p = start_phys + i;
    u64 v = VMM_SECTION_BASE + i;

    memflags = PTE_NORMAL | PTE_SH_INNER;

    if(!is_vmm_text(v))
      memflags |= PTE_XN;

    if(is_vmm_text(v) || is_vmm_rodata(v))
      memflags |= PTE_RO;

    __pagemap(v, p, memflags);
  }

  system_memory_reserve(start_phys, start_phys + i, "vmm image");
}

static void remap_earlymem() {
  u64 vstart = early_phys_start + VIRT_BASE;
  u64 size = early_memsize;
  u64 i, memflags = PTE_NORMAL | PTE_SH_INNER;

  for(i = 0; i < size; i += PAGESIZE) {
    __pagemap(vstart + i, early_phys_start + i, memflags);
  }

  system_memory_reserve(early_phys_start, early_phys_end, "earlymem");
}

static void remap_earlycon() {
  u64 flags = PTE_DEVICE_nGnRE | PTE_XN;

  __pagemap(EARLY_PL011_BASE, EARLY_PL011_BASE, flags);
}

static void map_memory() {
  /* system_memory available here */

  u64 memflags;
  struct memblock *mem;
  int nslot = system_memory.nslot;

  for(mem = system_memory.slots; mem < &system_memory.slots[nslot]; mem++) {
    u64 vbase = VIRT_BASE + mem->phys_start;
    u64 size = mem->size;

    for(u64 i = 0; i < size; i += PAGESIZE) {
      u64 p = mem->phys_start + i;
      u64 v = vbase + i;
      u64 kv = phys2kern(p);
      memflags = PTE_NORMAL | PTE_SH_INNER;

      if(early_phys_start <= p && p < early_phys_end)   // already mapped
        continue;

      if(!is_vmm_text(kv))
        memflags |= PTE_XN;

      if(is_vmm_text(kv) || is_vmm_rodata(kv))
        memflags |= PTE_RO;

      __pagemap(v, p, memflags);
    }
  }
}

void setup_pagetable_secondary() {
  set_ttbr0_el2(V2P(vmm_pagetable));
}

void *setup_pagetable(u64 fdt_base) {
  void *virt_fdt;

  root_level = 1;

  assert(PAGE_ALIGNED(vmm_pagetable));

  memset(vmm_pagetable, 0, PAGESIZE);

  remap_image();
  remap_earlymem();
  remap_earlycon();

  set_ttbr0_el2(V2P(vmm_pagetable));

  virt_fdt = remap_fdt(fdt_base);

  device_tree_init(virt_fdt);
  map_memory();

  return virt_fdt;
}
