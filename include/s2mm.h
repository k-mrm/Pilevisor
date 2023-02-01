#ifndef CORE_S2MM_H
#define CORE_S2MM_H

#include "types.h"
#include "mm.h"

void map_guest_peripherals(u64 *pgt);
void vmiomap_passthrough(u64 *s2pgt, u64 pa, u64 size);

void s2mmu_init_core(void);
void s2mmu_init(void);

#define VTCR_T0SZ(n)  ((n) & 0x3f)
#define VTCR_SL0(n)   (((n) & 0x3) << 6)
#define VTCR_IRGN0(n) (((n) & 0x3) << 8)
#define VTCR_ORGN0(n) (((n) & 0x3) << 10)
#define VTCR_SH0(n)   (((n) & 0x3) << 12)
#define VTCR_TG0(n)   (((n) & 0x3) << 14)
#define VTCR_PS(n)    (((n) & 0x7) << 16)
#define VTCR_HA       (1 << 21)
#define VTCR_HD       (1 << 22)
#define VTCR_NSW      (1 << 29)
#define VTCR_NSA      (1 << 30)

#define VTCR_RES1     (1ul << 31)

#define VTCR_TG_4K    VTCR_TG0(0)
#define VTCR_TG_64K   VTCR_TG0(1)
#define VTCR_TG_16K   VTCR_TG0(2)

#define VTCR_NOSH     VTCR_SH0(0)
#define VTCR_OUTERSH  VTCR_SH0(2)
#define VTCR_INNERSH  VTCR_SH0(3)

#define VTCR_PS_4G    VTCR_PS(0)
#define VTCR_PS_64G   VTCR_PS(1)
#define VTCR_PS_1T    VTCR_PS(2)
#define VTCR_PS_4T    VTCR_PS(3)
#define VTCR_PS_16T   VTCR_PS(4)
#define VTCR_PS_256T  VTCR_PS(5)
#define VTCR_PS_4P    VTCR_PS(6)

#define VTCR_IRGN0_NC     VTCR_IRGN0(0)
#define VTCR_IRGN0_WBWA   VTCR_IRGN0(1)
#define VTCR_IRGN0_WT     VTCR_IRGN0(2)
#define VTCR_IRGN0_WB     VTCR_IRGN0(3)

#define VTCR_ORGN0_NC     VTCR_ORGN0(0)
#define VTCR_ORGN0_WBWA   VTCR_ORGN0(1)
#define VTCR_ORGN0_WT     VTCR_ORGN0(2)
#define VTCR_ORGN0_WB     VTCR_ORGN0(3)

/* stage 2 attribute */
#define S2PTE_S2AP(ap)    (((ap) & 3) << 6)

#define S2PTE_S2AP_MASK   (3ul << 6)
#define S2PTE_RO          S2PTE_S2AP(1ul)
#define S2PTE_WO          S2PTE_S2AP(2ul)
#define S2PTE_RW          S2PTE_S2AP(3ul)

#define S2PTE_DBM         (1ul << 51)

/* use bit[58:55] to keep page's copyset  */
#define S2PTE_COPYSET_SHIFT 55
#define S2PTE_COPYSET(c)    (((u64)(c) & 0xf) << S2PTE_COPYSET_SHIFT)
#define S2PTE_COPYSET_MASK  S2PTE_COPYSET(0xf)

void switch_vttbr(physaddr_t vttbr);
u64 faulting_ipa_page(void);

u64 *s2_readable_pte(u64 *s2pgt, u64 ipa);
void *ipa2hva(u64 *pgt, u64 ipa);

static inline int s2pte_perm(u64 *pte) {
  return (*pte & S2PTE_S2AP_MASK) >> 6;
}

static inline void s2pte_invalidate(u64 *pte) {
  *pte &= ~PTE_AF;
}

static inline void s2pte_ro(u64 *pte) {
  *pte &= ~S2PTE_S2AP_MASK;
  *pte |= S2PTE_RO;
}

static inline void s2pte_rw(u64 *pte) {
  *pte |= S2PTE_RW;
}

static inline int s2pte_copyset(u64 *pte) {
  return (*pte & S2PTE_COPYSET_MASK) >> S2PTE_COPYSET_SHIFT;
}

static inline void s2pte_add_copyset(u64 *pte, int nodeid) {
  *pte |= S2PTE_COPYSET(1 << nodeid);
}

static inline void s2pte_clear_copyset(u64 *pte) {
  *pte &= ~S2PTE_COPYSET_MASK;
}

#endif  /* CORE_S2MM_H */
