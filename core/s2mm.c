/*
 *  stage 2 pagetable
 */

#include "aarch64.h"
#include "lib.h"
#include "mm.h"
#include "s2mm.h"
#include "param.h"
#include "localnode.h"
#include "allocpage.h"
#include "memlayout.h"
#include "printf.h"
#include "guest.h"
#include "panic.h"
#include "tlb.h"

void copy_to_guest_alloc(u64 *pgt, u64 to_ipa, char *from, u64 len);

static int root_level;

static int parange_map[] = {
  32, 36, 40, 42, 44, 48, 52,
};

void pagemap(u64 *pgt, u64 va, physaddr_t pa, u64 size, u64 attr) {
  if(va % PAGESIZE != 0 || pa % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pagemap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE, pa += PAGESIZE) {
    u64 *pte = pagewalk(pgt, va, root_level, 1);
    if(*pte & PTE_AF)
      panic("this entry has been used: va %p", va);

    pte_set_entry(pte, pa, attr);
  }
}

void vmmemmap(u64 *s2pgt, u64 va, u64 pa, u64 size, u64 attr) {
  ;
}

/* make identity map */
void vmiomap_passthrough(u64 *s2pgt, u64 va, u64 size) {
  u64 pa = va;

  if(va % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("vmiomap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE, pa += PAGESIZE) {
    u64 *pte = pagewalk(s2pgt, va, root_level, 1);
    if(*pte & PTE_AF)
      panic("this entry has been used: va %p", va);

    pte_set_entry(pte, pa, S2PTE_RW | PTE_DEVICE_nGnRE | PTE_XN);
  }
}

void pageunmap(u64 *pgt, u64 va, u64 size) {
  if(va % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pageunmap");

  for(u64 p = 0; p < size; p += PAGESIZE, va += PAGESIZE) {
    u64 *pte = pagewalk(pgt, va, root_level, 0);
    if(*pte == 0)
      panic("unmapped");

    u64 pa = PTE_PA(*pte);
    free_page(P2V(pa));

    pte_clear(pte);
  }
}

void alloc_guestmem(u64 *pgt, u64 ipa, u64 size) {
  if(size % PAGESIZE)
    panic("invalid size");

  for(int i = 0; i < size; i += PAGESIZE) {
    char *p = alloc_page();
    if(!p)
      panic("p");

    pagemap(pgt, ipa+i, V2P(p), PAGESIZE, PTE_NORMAL|S2PTE_RW);
  }
}

void map_guest_image(u64 *pgt, struct guest *img, u64 ipa) {
  printf("map guest image %p - %p\n", ipa, ipa + img->size);

  copy_to_guest(pgt, ipa, (char *)img->start, img->size);
}

void map_guest_peripherals(u64 *s2pgt) {
  vmiomap_passthrough(s2pgt, 0x09000000, PAGESIZE);   // UART
  // vmiomap_passthrough(s2pgt, 0x09010000, PAGESIZE);   // RTC
  // vmiomap_passthrough(s2pgt, 0x09030000, PAGESIZE);   // GPIO

  // vmiomap_passthrough(s2pgt, 0x0a000000, 0x4000);   // VIRTIO0

  // vmiomap_passthrough(s2pgt, 0x4010000000ul, 256*1024*1024);    // PCIE ECAM
  // vmiomap_passthrough(s2pgt, 0x10000000, 0x2eff0000);           // PCIE MMIO
  // vmiomap_passthrough(s2pgt, 0x8000000000ul, 0x100000);         // PCIE HIGH MMIO
}

void pageremap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr) {
  if(va % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pageremap");

  /* TODO: copy old page */

  pageunmap(pgt, va, size);
  pagemap(pgt, va, pa, size, attr);
}

u64 *page_rwable_pte(u64 *pgt, u64 va) {
  if(!PAGE_ALIGNED(va))
    panic("page rwable pte");

  u64 *pte = pagewalk(pgt, va, root_level, 0);
  if(!pte)
    return NULL;

  if((*pte & (PTE_AF | S2PTE_S2AP_MASK)) == (PTE_AF | S2PTE_RW))
    return pte;
    
  return NULL;
}

u64 *s2_readable_pte(u64 *s2pgt, u64 ipa) {
  if(!PAGE_ALIGNED(ipa))
    panic("s2 readable pte");

  u64 *pte = pagewalk(s2pgt, ipa, root_level, 0);
  if(!pte)
    return NULL;

  if((*pte & PTE_AF) && (*pte & S2PTE_RO))
    return pte;
  
  return NULL;
}

u64 *page_ro_pte(u64 *pgt, u64 va) {
  if(!PAGE_ALIGNED(va))
    panic("page_invalidate");

  u64 *pte = pagewalk(pgt, va, root_level, 0);
  if(!pte)
    return NULL;

  if((*pte & (PTE_AF | S2PTE_S2AP_MASK)) == (PTE_AF | S2PTE_RO))
    return pte;
    
  return NULL;
}

bool page_accessible(u64 *pgt, u64 va) {
  return !!page_accessible_pte(pgt, va);
}

void page_access_invalidate(u64 *pgt, u64 va) {
  if(!PAGE_ALIGNED(va))
    panic("page_access_invalidate");

  u64 *pte = pagewalk(pgt, va, root_level, 0);
  if(!pte)
    panic("no entry");

  u64 pa = PTE_PA(*pte);

  s2pte_invalidate(pte);

  tlb_s2_flush_all();

  free_page(P2V(pa));
}

void page_access_ro(u64 *pgt, u64 va) {
  if(!PAGE_ALIGNED(va))
    panic("page_access_ro");

  u64 *pte = pagewalk(pgt, va, root_level, 0);
  if(!pte)
    panic("no entry");

  s2pte_ro(pte);

  tlb_s2_flush_all();
}

void copy_to_guest_alloc(u64 *pgt, u64 to_ipa, char *from, u64 len) {
  while(len > 0) {
    void *hva = ipa2hva(pgt, to_ipa);
    if(hva == 0) {
      char *page = alloc_page();
      pagemap(pgt, PAGE_ADDRESS(to_ipa), V2P(page), PAGESIZE, PTE_NORMAL|S2PTE_RW);
      hva = ipa2hva(pgt, to_ipa);
      if(hva == 0)
        panic("copy_to_guest_alloc");
    }

    u64 poff = PAGE_OFFSET(to_ipa);
    u64 n = PAGESIZE - poff;
    if(n > len)
      n = len;

    memcpy((char *)hva, from, n);

    from += n;
    to_ipa += n;
    len -= n;
  }
}

void copy_to_guest(u64 *pgt, u64 to_ipa, char *from, u64 len) {
  while(len > 0) {
    void *hva = ipa2hva(pgt, to_ipa);
    if(hva == 0)
      panic("copy_to_guest hva == 0 to_ipa: %p", to_ipa);
    u64 poff = to_ipa & (PAGESIZE-1);
    u64 n = PAGESIZE - poff;
    if(n > len)
      n = len;

    memcpy((char *)hva, from, n);

    from += n;
    to_ipa += n;
    len -= n;
  }
}

void copy_from_guest(u64 *pgt, char *to, u64 from_ipa, u64 len) {
  while(len > 0) {
    void *hva = ipa2hva(pgt, from_ipa);
    if(hva == 0)
      panic("copy_from_guest hva == 0 from_ipa: %p", from_ipa);
    u64 poff = PAGE_OFFSET(from_ipa);
    u64 n = PAGESIZE - poff;
    if(n > len)
      n = len;

    memcpy(to, (char *)hva, n);

    to += n;
    from_ipa += n;
    len -= n;
  }
}

u64 ipa2pa(u64 *pgt, u64 ipa) {
  u64 *pte = pagewalk(pgt, ipa, root_level, 0);
  if(!pte)
    return 0;
  u32 off = PAGE_OFFSET(ipa);

  return PTE_PA(*pte) + off;
}

void *ipa2hva(u64 *pgt, u64 ipa) {
  u64 *pte = pagewalk(pgt, ipa, root_level, 0);
  if(!pte)
    return 0;
  u32 off = PAGE_OFFSET(ipa);

  return P2V(PTE_PA(*pte) + off);
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

u64 faulting_ipa_page() {
  u64 hpfar = read_sysreg(hpfar_el2); 
  u64 ipa_page = (hpfar & HPFAR_FIPA_MASK) << 8;

  return ipa_page & ~(PAGESIZE-1);
}

static int root_level_sl0(int root) {
  switch(root) {
    case 3:
      return 3;
    case 2:
      return 0;
    case 1:
      return 1;
    case 0:
      return 2;
  }

  return -1;
}

void s2mmu_init() {
  u64 vtcr, mmf_parange = read_sysreg(id_aa64mmfr0_el1) & 0xf;
  int parange = parange_map[mmf_parange];
  int sl0;
  u32 ps;

  printf("id_aa64mmfr0_el1.parange = %d bit\n", parange);

  int min_t0sz = 64 - parange;

  vtcr = VTCR_INNERSH | VTCR_HA | VTCR_HD | VTCR_TG_4K |
         VTCR_ORGN0_WBRW | VTCR_IRGN0_WBRW | VTCR_NSW |
         VTCR_NSA | VTCR_RES1;

  /* PS = 16TB (44 bit) */
  int t0sz = 64 - 44;
  if(t0sz < min_t0sz)
    panic("t0sz %d < min t0sz %pd", t0sz, min_t0sz);

  root_level = 0;
  sl0 = root_level_sl0(root_level);

  vtcr |= VTCR_T0SZ(t0sz) | VTCR_PS_16T | VTCR_SL0(sl0);

  u64 *vttbr = alloc_page();
  if(!vttbr)
    panic("vttbr failed");

  localvm.vttbr = vttbr;
  localvm.vtcr = vtcr;

  printf("vtcr_el2: %p\n", vtcr);
  printf("mair_el2: %p\n", read_sysreg(mair_el2));
}
