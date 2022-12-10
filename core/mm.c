#include "aarch64.h"
#include "tlb.h"
#include "mm.h"
#include "panic.h"

static u64 *vmm_pagetable;
static int root_level = 1;

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

void memmap(u64 pa, u64 va, u64 size) {
  ;
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
