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

/*
 *  mapping device memory
 */
void *iomap(u64 pa, u64 size) {
  u64 va = pa;
  u64 memflags = PTE_DEVICE_nGnRE | PTE_RW | PTE_XN;

  __memmap(pa, va, size, memflags);

  return (void *)va;
}

void setup_pagetable(void) {
  root_level = 1;

  vmm_pagetable = alloc_page();

  u64 start = PAGE_ADDRESS(vmm_start);
  u64 end = early_alloc_end;
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
  
  set_ttbr0_el2(vmm_pagetable);
}
