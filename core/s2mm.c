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
#include "assert.h"

static int root_level;

static int parange_map[] = {
  32, 36, 40, 42, 44, 48, 52,
};

void s2_pte_dump(ipa_t ipa) {
  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  if(!pte) {
    printf("unmapped\n");
    return;
  }

  u64 e = *pte;
  int v = (e & PTE_V) == PTE_V;
  int af = !!(e & PTE_AF);
  int ai = (e >> 2) & PTE_INDX_MASK;

  printf("pte@%p: %p\n"
         "\tphysical address: %p\n"
         "\tv: %d\n"
         "\taccess flag: %d\n"
         "\tattrindex: %d\n"
         "\t", ipa, e, PTE_PA(e), v, af, ai);
  printf("mair %p %p\n", read_sysreg(mair_el2), read_sysreg(mair_el1));
}

static inline u64 pageflag_to_s2pte_flags(enum pageflag flags) {
  u64 f = 0;

  if(flags & PAGE_NORMAL)
    f = S2PTE_NORMAL;
  else if(flags & PAGE_DEVICE)
    f = S2PTE_DEVICE_nGnRE | PTE_XN;

  if(flags & PAGE_RW)
    f |= S2PTE_RW;
  else if(flags & PAGE_RO)
    f |= S2PTE_RO;

  if(flags & PAGE_NOEXEC)
    f |= PTE_XN;

  return f;
}

static void __s2_map_pages(ipa_t ipa, physaddr_t pa, u64 size, enum pageflag flags) {
  u64 f = pageflag_to_s2pte_flags(flags);

  mappages(vttbr, ipa, pa, size, f);
}

void guest_map_page(ipa_t ipa, physaddr_t pa, enum pageflag flags) {
  __s2_map_pages(ipa, pa, PAGESIZE, flags);
}

/* make identity map */
void vmiomap_passthrough(ipa_t ipa, u64 size) {
  u64 pa = ipa;

  __s2_map_pages(ipa, pa, size, PAGE_RW | PAGE_DEVICE);
}

void s2pageunmap(ipa_t ipa, u64 size) {
  if(ipa % PAGESIZE != 0 || size % PAGESIZE != 0)
    panic("invalid pageunmap");

  for(u64 p = 0; p < size; p += PAGESIZE, ipa += PAGESIZE) {
    u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
    if(*pte == 0)
      panic("already unmapped");

    u64 pa = PTE_PA(*pte);
    free_page(P2V(pa));

    pte_clear(pte);
  }
}

void alloc_guestmem(u64 ipa, u64 size) {
  if(size % PAGESIZE)
    panic("invalid size");

  for(int i = 0; i < size; i += PAGESIZE) {
    char *p = alloc_page();
    if(!p)
      panic("p");

    guest_map_page(ipa + i, V2P(p), PAGE_NORMAL | PAGE_RW);
  }
}

void map_guest_image(struct guest *img, u64 ipa) {
  printf("map guest image %p - %p\n", ipa, ipa + img->size);

  copy_to_guest(ipa, (char *)img->start, img->size, false);
}

void map_guest_peripherals() {
  vmiomap_passthrough(0x09000000, PAGESIZE);   // UART
  // vmiomap_passthrough(vttbr, 0x09010000, PAGESIZE);   // RTC
  // vmiomap_passthrough(vttbr, 0x09030000, PAGESIZE);   // GPIO

  // vmiomap_passthrough(vttbr, 0x0a000000, 0x4000);   // VIRTIO0

  // vmiomap_passthrough(vttbr, 0x4010000000ul, 256*1024*1024);    // PCIE ECAM
  // vmiomap_passthrough(vttbr, 0x10000000, 0x2eff0000);           // PCIE MMIO
  // vmiomap_passthrough(vttbr, 0x8000000000ul, 0x100000);         // PCIE HIGH MMIO
}

void s2_map_page_copyset(ipa_t ipa, physaddr_t pa, u64 copyset) {
  u64 flags = S2PTE_NORMAL | S2PTE_COPYSET(copyset);

  return mappages(vttbr, ipa, pa, PAGESIZE, flags);
}

u64 *s2_rwable_pte(ipa_t ipa) {
  assert(PAGE_ALIGNED(ipa));

  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  if(!pte)
    return NULL;

  if((*pte & (PTE_AF | S2PTE_S2AP_MASK)) == (PTE_AF | S2PTE_RW))
    return pte;
    
  return NULL;
}

u64 *s2_readable_pte(ipa_t ipa) {
  assert(PAGE_ALIGNED(ipa));

  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  if(!pte)
    return NULL;

  if((*pte & PTE_AF) && (*pte & S2PTE_RO))
    return pte;
  
  return NULL;
}

u64 *s2_ro_pte(ipa_t ipa) {
  assert(PAGE_ALIGNED(ipa));

  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  if(!pte)
    return NULL;

  if((*pte & (PTE_AF | S2PTE_S2AP_MASK)) == (PTE_AF | S2PTE_RO))
    return pte;
    
  return NULL;
}

void s2_page_invalidate(ipa_t ipa) {
  assert(PAGE_ALIGNED(ipa));

  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  if(!pte)
    panic("no entry");

  u64 pa = PTE_PA(*pte);

  s2pte_invalidate(pte);
  tlb_s2_flush_ipa(ipa);

  free_page(P2V(pa));
}

void s2_page_ro(ipa_t ipa) {
  assert(PAGE_ALIGNED(ipa));

  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  if(!pte)
    panic("no entry");

  s2pte_ro(pte);
  tlb_s2_flush_ipa(ipa);
}

void copy_to_guest(ipa_t to_ipa, char *from, u64 len, bool alloc) {
  while(len > 0) {
    void *hva = ipa2hva(to_ipa);
    if(hva == 0) {
      if(!alloc)
        panic("copy_to_guest hva == 0 to_ipa: %p", to_ipa);

      char *page = alloc_page();
      if(!page)
        panic("page");

      guest_map_page(PAGE_ADDRESS(to_ipa), V2P(page), PAGE_NORMAL | PAGE_RW);

      hva = page;
    }

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

void copy_from_guest(char *to, ipa_t from_ipa, u64 len) {
  while(len > 0) {
    void *hva = ipa2hva(from_ipa);
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

physaddr_t ipa2pa(ipa_t ipa) {
  u64 *pte = pagewalk(vttbr, ipa, root_level, 0);
  u32 off;

  if(!pte)
    return 0;

  off = PAGE_OFFSET(ipa);

  return PTE_PA(*pte) + off;
}

void *ipa2hva(ipa_t ipa) {
  return P2V(ipa2pa(ipa));
}

u64 at_uva2pa(u64 uva) {
  u64 par;

  asm volatile("at s12e1r, %0" :: "r"(uva) : "memory");

  par = read_sysreg(par_el1);

  if(par & 1) {
    printf("uva2pa: %p ", uva);
    dump_par_el1(par);
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
    printf("uva2ipa: %p ", uva);
    dump_par_el1(par);
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

  printf("id_aa64mmfr0_el1.parange = %d bit\n", parange);

  int min_t0sz = 64 - parange;

  vtcr = VTCR_INNERSH | VTCR_HA | VTCR_HD | VTCR_TG_4K |
         VTCR_ORGN0_WBWA | VTCR_IRGN0_WBWA | VTCR_NSW |
         VTCR_NSA | VTCR_RES1;

  /* PS = 16TB (44 bit) */
  int t0sz = 64 - 44;
  if(t0sz < min_t0sz)
    panic("t0sz %d < min t0sz %pd", t0sz, min_t0sz);

  root_level = 0;
  sl0 = root_level_sl0(root_level);

  vtcr |= VTCR_T0SZ(t0sz) | VTCR_PS_16T | VTCR_SL0(sl0);

  vttbr = alloc_page();
  if(!vttbr)
    panic("vttbr failed");

  localvm.vttbr = vttbr;
  localvm.vtcr = vtcr;

  printf("vtcr_el2: %p\n", vtcr);
  printf("mair_el2: %p\n", read_sysreg(mair_el2));
}
