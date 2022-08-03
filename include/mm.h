#ifndef MVMM_MM_H
#define MVMM_MM_H

#include "types.h"

#define TCR_T0SZ(n)   ((n) & 0x3f)
#define TCR_IRGN0(n)  (((n) & 0x3) << 8)
#define TCR_ORGN0(n)  (((n) & 0x3) << 10)
#define TCR_SH0(n)    (((n) & 0x3) << 12)
#define TCR_TG0(n)    (((n) & 0x3) << 14)
#define TCR_T1SZ(n)   (((n) & 0x3f) << 16)
#define TCR_A1(n)     (((n) & 0x1) << 22)
#define TCR_EPD1(n)   (((n) & 0x1) << 23)
#define TCR_IRGN1(n)  (((n) & 0x3) << 24)
#define TCR_ORGN1(n)  (((n) & 0x3) << 26)
#define TCR_SH1(n)    (((n) & 0x3) << 28)
#define TCR_TG1(n)    (((n) & 0x3) << 30)
#define TCR_IPS(n)    (((n) & 0x7) << 32)
#define TCR_AS(n)     (((n) & 0x1) << 36)
#define TCR_TBI0(n)   (((n) & 0x1) << 37)
#define TCR_TBI1(n)   (((n) & 0x1) << 38)

#define VTCR_T0SZ(n)  ((n) & 0x3f)
#define VTCR_SL0(n)   (((n) & 0x3) << 6)
#define VTCR_SH0(n)   (((n) & 0x3) << 12)
#define VTCR_TG0(n)   (((n) & 0x3) << 14)
#define VTCR_PS(n)    (((n) & 0x7) << 16)
#define VTCR_NSW      (1 << 29)
#define VTCR_NSA      (1 << 30)

/*
 *  48bit Virtual Address
 *
 *     48    39 38    30 29    21 20    12 11       0
 *    +--------+--------+--------+--------+----------+
 *    | level0 | level1 | level2 | level3 | page off |
 *    +--------+--------+--------+--------+----------+
 *
 *
 */

#define PIDX(level, va) (((va) >> (39 - (level) * 9)) & 0x1ff)
#define OFFSET(va)  ((va) & 0xfff)

#define PTE_PA(pte) ((u64)(pte) & 0xfffffffff000)

/* lower attribute */
#define PTE_VALID 1   /* level 0,1,2 descriptor */
#define PTE_TABLE 2   /* level 0,1,2 descriptor */
#define PTE_V 3       /* level 3 descriptor */
#define PTE_INDX(idx) (((idx) & 7) << 2)
#define PTE_NORMAL  PTE_INDX(AI_NORMAL_NC_IDX)
#define PTE_DEVICE  PTE_INDX(AI_DEVICE_nGnRnE_IDX)
#define PTE_NS  (1 << 5)
#define PTE_AP(ap)  (((ap) & 3) << 6)
#define PTE_U   PTE_AP(1)
#define PTE_RO  PTE_AP(2)
#define PTE_URO PTE_AP(3)
#define PTE_SH(sh)  (((sh) & 3) << 8)
#define PTE_AF  (1 << 10)
/* upper attribute */
#define PTE_PXN (1UL << 53)
#define PTE_UXN (1UL << 54)

/* stage 2 attribute */
#define S2PTE_AF  (1 << 10)
#define S2PTE_S2AP(ap)  (((ap) & 3) << 6)
#define S2PTE_RO  S2PTE_S2AP(1)
#define S2PTE_WO  S2PTE_S2AP(2)
#define S2PTE_RW  S2PTE_S2AP(3)
#define S2PTE_ATTR(attr)  (((attr) & 7) << 2)
#define S2PTE_NORMAL  S2PTE_ATTR(AI_NORMAL_NC_IDX)
#define S2PTE_DEVICE  S2PTE_ATTR(AI_DEVICE_nGnRnE_IDX)

#define PAGESIZE  4096    /* 4KB */

#define PAGEROUNDUP(p)  ((p + PAGESIZE - 1) & ~(PAGESIZE - 1))

/* attr index */
#define AI_DEVICE_nGnRnE_IDX  0x0
#define AI_NORMAL_NC_IDX      0x1

/* attr */
#define AI_DEVICE_nGnRnE  0x0
#define AI_NORMAL_NC      0x44

struct dabort_info {
  u64 fault_ipa;
  bool isv;
  bool write;
  int reg;
  int accbyte;
};

u64 *pagewalk(u64 *pgt, u64 va, int alloc);
void pagemap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr);
void pageunmap(u64 *pgt, u64 va, u64 size);

u64 ipa2pa(u64 *pgt, u64 ipa);
u64 at_uva2pa(u64 uva);
u64 at_uva2ipa(u64 uva);

void copy_to_guest(u64 *pgt, u64 to_ipa, char *from, u64 len);
void copy_from_guest(u64 *pgt, char *to, u64 from_ipa, u64 len);

void s2mmu_init(void);

u64 faulting_ipa_page(void);

#endif
