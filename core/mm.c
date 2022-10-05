#include "aarch64.h"
#include "lib.h"
#include "mm.h"
#include "memmap.h"
#include "allocpage.h"
#include "printf.h"
#include "guest.h"

void copy_to_guest_alloc(u64 *pgt, u64 to_ipa, char *from, u64 len);

u64 *pagewalk(u64 *pgt, u64 va, int create) {
  for(int level = 0; level < 3; level++) {
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
    free_page((void *)pa);
    *pte = 0;
  }
}

void alloc_guestmem(u64 *pgt, u64 ipa, u64 size) {
  if(size % PAGESIZE)
    panic("invalid size");

  for(int i = 0; i < size; i += PAGESIZE) {
    char *p = alloc_page();
    if(!p)
      panic("p");

    pagemap(pgt, ipa+i, (u64)p, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  }
}

void map_guest_image(u64 *pgt, struct guest *img, u64 ipa) {
  copy_to_guest_alloc(pgt, ipa, (char *)img->start, img->size);
}

void map_peripherals(u64 *pgt) {
  pagemap(pgt, UARTBASE, UARTBASE, PAGESIZE, S2PTE_DEVICE|S2PTE_RW);
  pagemap(pgt, GPIOBASE, GPIOBASE, PAGESIZE, S2PTE_DEVICE|S2PTE_RW);
  pagemap(pgt, RTCBASE, RTCBASE, PAGESIZE, S2PTE_DEVICE|S2PTE_RW);
  pagemap(pgt, VIRTIO0, VIRTIO0, 0x4000, S2PTE_DEVICE|S2PTE_RW);
  pagemap(pgt, PCIE_ECAM_BASE, PCIE_ECAM_BASE, 256*1024*1024,
          S2PTE_DEVICE|S2PTE_RW);
  pagemap(pgt, PCIE_MMIO_BASE, PCIE_MMIO_BASE, 0x2eff0000, S2PTE_DEVICE|S2PTE_RW);
  pagemap(pgt, PCIE_HIGH_MMIO_BASE, PCIE_HIGH_MMIO_BASE, 0x100000/*XXX*/,
          S2PTE_DEVICE|S2PTE_RW);
}

void pageremap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr) {
  if(va % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pageremap");

  /* TODO: copy old page */

  pageunmap(pgt, va, size);
  pagemap(pgt, va, pa, size, attr);
}

void page_access_invalidate(u64 *pgt, u64 va) {
  if(!PAGE_ALIGNED(va))
    panic("page_invalidate");

  u64 *pte = pagewalk(pgt, va, 0);
  if(!pte)
    panic("no entry");

  *pte &= S2PTE_AF;
}

void page_access_ro(u64 *pgt, u64 va) {
  if(!PAGE_ALIGNED(va))
    panic("page_invalidate");

  u64 *pte = pagewalk(pgt, va, 0);
  if(!pte)
    panic("no entry");

  *pte &= ~S2PTE_S2AP_MASK;
  *pte |= S2PTE_RO;
}

void copy_to_guest_alloc(u64 *pgt, u64 to_ipa, char *from, u64 len) {
  while(len > 0) {
    u64 pa = ipa2pa(pgt, to_ipa);
    if(pa == 0) {
      char *page = alloc_page();
      pagemap(pgt, PAGE_ADDRESS(to_ipa), (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
      pa = ipa2pa(pgt, to_ipa);
      if(pa == 0)
        panic("copy_to_guest_alloc");
    }

    u64 poff = PAGE_OFFSET(to_ipa);
    u64 n = PAGESIZE - poff;
    if(n > len)
      n = len;

    memcpy((char *)pa, from, n);

    from += n;
    to_ipa += n;
    len -= n;
  }
}

void copy_to_guest(u64 *pgt, u64 to_ipa, char *from, u64 len) {
  while(len > 0) {
    u64 pa = ipa2pa(pgt, to_ipa);
    if(pa == 0)
      panic("copy_to_guest pa == 0 to_ipa: %p", to_ipa);
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
      panic("copy_from_guest pa == 0 from_ipa: %p", from_ipa);
    u64 poff = PAGE_OFFSET(from_ipa);
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
  u32 off = PAGE_OFFSET(ipa);

  return PTE_PA(*pte) + off;
}

u64 at_uva2pa(u64 uva) {
  u64 par;

  asm volatile("at s12e1r, %0" :: "r"(uva) : "memory");

  par = read_sysreg(par_el1);

  if(par & 1) {
    // dump_par_el1();
    return 0;
  } else {
    return (par & 0xfffffffff000) | PAGE_OFFSET(uva);
  }
}

u64 at_uva2ipa(u64 uva) {
  u64 par;

  asm volatile("at s1e1r, %0" :: "r"(uva) : "memory");

  par = read_sysreg(par_el1);

  if(par & 1) {
    // dump_par_el1();
    return 0;
  } else {
    return (par & 0xfffffffff000) | PAGE_OFFSET(uva);
  }
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

u64 faulting_ipa_page() {
  u64 hpfar = read_sysreg(hpfar_el2); 
  u64 ipa_page = (hpfar & HPFAR_FIPA_MASK) << 8;

  return ipa_page & ~(PAGESIZE-1);
}

void s2mmu_init(void) {
  u64 mmf = read_sysreg(id_aa64mmfr0_el1);
  printf("id_aa64mmfr0_el1.parange = %p\n", mmf & 0xf);

  u64 vtcr = VTCR_T0SZ(20) | VTCR_SH0(0) | VTCR_SL0(2) | VTCR_HA | VTCR_HD |
             VTCR_TG0(0) | VTCR_NSW | VTCR_NSA | VTCR_PS(4);
  write_sysreg(vtcr_el2, vtcr);

  u64 mair = (AI_DEVICE_nGnRnE << (8 * AI_DEVICE_nGnRnE_IDX)) | (AI_NORMAL_NC << (8 * AI_NORMAL_NC_IDX));
  write_sysreg(mair_el2, mair);

  isb();
}
