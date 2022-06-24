#include "mm.h"
#include "aarch64.h"
#include "lib.h"
#include "kalloc.h"
#include "printf.h"

u64 *pagewalk(u64 *pgt, u64 va, int alloc) {
  for(int level = 0; level < 3; level++) {
    u64 *pte = &pgt[PIDX(level, va)];

    if((*pte & PTE_VALID) && (*pte & PTE_TABLE)) {
      pgt = (u64 *)PTE_PA(*pte);
    } else if(alloc) {
      pgt = kalloc();
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

void pagemap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr) {
  if(va % PAGESIZE != 0 || pa % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pagemap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE, pa += PAGESIZE) {
    u64 *pte = pagewalk(pgt, va, 1);
    if(*pte & PTE_AF)
      panic("this entry has been used");

    *pte = PTE_PA(pa) | S2PTE_AF | attr | PTE_V;
  }
}

void pageunmap(u64 *pgt, u64 va, u64 size) {
  if(va % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pageunmap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE) {
    u64 *pte = pagewalk(pgt, va, 0);
    if(*pte == 0)
      panic("unmapped");

    u64 pa = PTE_PA(*pte);
    kfree((void *)pa);
    *pte = 0;
  }
}

void pageremap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr) {
  if(va % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pageremap");

  /* TODO: copy old page */

  pageunmap(pgt, va, size);
  pagemap(pgt, va, pa, size, attr);
}

void copy_to_guest(u64 *pgt, u64 to_ipa, char *from, u64 len) {
  while(len > 0) {
    u64 pa = ipa2pa(pgt, to_ipa);
    if(pa == 0)
      panic("copy_to_guest pa == 0");
    u64 poff = to_ipa & (PAGESIZE-1);
    u64 n = PAGESIZE - poff;
    if(n > len)
      n = len;

    memcpy((char *)pa, from, n);

    from += n;
    to_ipa += n;
    len -= n;
  }
}

void copy_from_guest(u64 *pgt, char *to, u64 from_ipa, u64 len) {
  while(len > 0) {
    u64 pa = ipa2pa(pgt, from_ipa);
    if(pa == 0)
      panic("copy_from_guest pa == 0");
    u64 poff = from_ipa & (PAGESIZE-1);
    u64 n = PAGESIZE - poff;
    if(n > len)
      n = len;

    memcpy(to, (char *)pa, n);

    to += n;
    from_ipa += n;
    len -= n;
  }
}

u64 ipa2pa(u64 *pgt, u64 ipa) {
  u64 *pte = pagewalk(pgt, ipa, 0);
  if(!pte)
    return 0;
  u32 off = ipa & (PAGESIZE-1);

  return PTE_PA(*pte) + off;
}

void dump_par_el1(u64 par) {
  if(par & 1) {
    printf("translation fault\n");
    printf("FST : %p\n", (par >> 1) & 0x3f);
    printf("PTW : %p\n", (par >> 8) & 1);
    printf("S   : %p\n", (par >> 9) & 1);
  } else {
    printf("address: %p\n", par);
  }
} 

void s2mmu_init(void) {
  u64 mmf;
  read_sysreg(mmf, id_aa64mmfr0_el1);
  printf("id_aa64mmfr0_el1.parange = %p\n", mmf & 0xf);

  u64 vtcr = VTCR_T0SZ(20) | VTCR_SH0(0) | VTCR_SL0(2) |
             VTCR_TG0(0) | VTCR_NSW | VTCR_NSA | VTCR_PS(4);
  write_sysreg(vtcr_el2, vtcr);

  u64 mair = (AI_DEVICE_nGnRnE << (8 * AI_DEVICE_nGnRnE_IDX)) | (AI_NORMAL_NC << (8 * AI_NORMAL_NC_IDX));
  write_sysreg(mair_el2, mair);

  isb();
}
