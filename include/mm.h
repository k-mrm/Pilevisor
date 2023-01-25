#ifndef CORE_MM_H
#define CORE_MM_H

#ifndef __ASSEMBLER__

#include "types.h"
#include "guest.h"
#include "allocpage.h"

#endif  /* __ASSEMBLER__ */

#define TCR_T0SZ(n)   ((n) & 0x3f)
#define TCR_IRGN0(n)  (((n) & 0x3) << 8)
#define TCR_ORGN0(n)  (((n) & 0x3) << 10)
#define TCR_SH0(n)    (((n) & 0x3) << 12)
#define TCR_TG0(n)    (((n) & 0x3) << 14)
#define TCR_PS(n)     (((n) & 0x7) << 16)

#define TCR_IRGN0_NC    TCR_IRGN0(0)
#define TCR_IRGN0_WBWA  TCR_IRGN0(1)
#define TCR_IRGN0_WT    TCR_IRGN0(2)
#define TCR_IRGN0_WB    TCR_IRGN0(3)

#define TCR_ORGN0_NC    TCR_ORGN0(0)
#define TCR_ORGN0_WBWA  TCR_ORGN0(1)
#define TCR_ORGN0_WT    TCR_ORGN0(2)
#define TCR_ORGN0_WB    TCR_ORGN0(3)

#define TCR_TG0_4K    TCR_TG0(0)
#define TCR_TG0_16K   TCR_TG0(1)
#define TCR_TG0_64K   TCR_TG0(2)

#define TCR_NONSH     TCR_SH0(0)
#define TCR_OUTERSH   TCR_SH0(2)
#define TCR_INNERSH   TCR_SH0(3)

#define TCR_PS_4G     TCR_PS(0)
#define TCR_PS_64G    TCR_PS(1)
#define TCR_PS_1T     TCR_PS(2)
#define TCR_PS_4T     TCR_PS(3)
#define TCR_PS_16T    TCR_PS(4)
#define TCR_PS_256T   TCR_PS(5)
#define TCR_PS_4P     TCR_PS(6)

#define TCR_RES1      ((1 << 21) | (1ul << 31))

/* 39 bit space */
#define TCR_EL2_VALUE \
  (TCR_T0SZ(25) | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA | TCR_TG0_4K | \
   TCR_INNERSH | TCR_RES1 | TCR_PS_1T)

#define VTCR_T0SZ(n)  ((n) & 0x3f)
#define VTCR_SL0(n)   (((n) & 0x3) << 6)
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
#define OFFSET(va)      ((va) & 0xfff)

/* lower attribute */
#define PTE_VALID     1   /* level 0,1,2 descriptor */
#define PTE_TABLE     2   /* level 0,1,2 descriptor */
#define PTE_V         3   /* level 3 descriptor */
#define PTE_INDX(idx) (((idx) & 7) << 2)
#define PTE_NS        (1 << 5)
#define PTE_AP(ap)    (((ap) & 3) << 6)
#define PTE_SH(sh)    (((sh) & 3) << 8)
#define PTE_AF        (1 << 10)
/* upper attribute */
/*
#define PTE_PXN       (1UL << 53)
#define PTE_UXN       (1UL << 54)
*/
#define PTE_XN        (1ul << 54)

#define PTE_RW        PTE_AP(0)
#define PTE_RO        PTE_AP(2)

#define PTE_SH_OUTER  PTE_SH(2)   // outer sharable
#define PTE_SH_INNER  PTE_SH(3)   // inner sharable

/* attr */
#define ATTR_DEVICE_nGnRnE      0x0ul
#define ATTR_DEVICE_nGnRE       0x4ul
#define ATTR_NORMAL_NC          0x44ul
#define ATTR_NORMAL             0xfful

/* attr index */
#define AI_DEVICE_nGnRnE_IDX    0
#define AI_DEVICE_nGnRE_IDX     1
#define AI_NORMAL_NC_IDX        2
#define AI_NORMAL_IDX           3

#define AI(attr, idx)           ((attr) << ((idx) * 8))

#define MAIR_VALUE        (AI(ATTR_DEVICE_nGnRnE, AI_DEVICE_nGnRnE_IDX) | \
                          AI(ATTR_DEVICE_nGnRE, AI_DEVICE_nGnRE_IDX) |    \
                          AI(ATTR_NORMAL_NC, AI_NORMAL_NC_IDX) |          \
                          AI(ATTR_NORMAL, AI_NORMAL_IDX))

#define PTE_DEVICE_nGnRnE   PTE_INDX(AI_DEVICE_nGnRnE_IDX)
#define PTE_DEVICE_nGnRE    PTE_INDX(AI_DEVICE_nGnRE_IDX)
#define PTE_NORMAL_NC       PTE_INDX(AI_NORMAL_NC_IDX)
#define PTE_NORMAL          PTE_INDX(AI_NORMAL_IDX)

#define PTE_PA(pte)         ((u64)(pte) & 0xfffffffff000)

#define PAGESIZE          4096  /* 4KB */
#define PAGESHIFT         12    /* 1 << 12 */

#define SZ_2MiB           0x200000
#define SZ_1GiB           0x40000000

#define BLOCKSIZE_L2      SZ_2MiB
#define BLOCKSIZE_L1      SZ_1GiB

#define PAGE_ADDRESS(p)   ((u64)(p) & ~(PAGESIZE-1))
#define PAGE_ALIGNED(p)   ((u64)(p) % PAGESIZE == 0)
#define PAGE_ALIGN(p)     (((u64)(p) + PAGESIZE-1) & ~(PAGESIZE-1))
#define PAGE_OFFSET(p)    ((u64)(p) & (PAGESIZE-1))

#define ALIGN_DOWN(p, align)  \
  ((u64)(p) & ~((align) - 1))

#define ALIGN_UP(p, align)  \
  (((u64)(p) + (align) - 1) & ~((align) - 1))

#ifndef __ASSEMBLER__

struct dabort_info {
  u64 fault_ipa;
  u64 fault_va;
  bool isv;
  bool write;   /* when isv == 1 */
  int reg;      /* when isv == 1 */
  int accbyte;  /* when isv == 1 */
};

u64 *pagewalk(u64 *pgt, u64 va, int root_level, int alloc);
void pagemap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr);
void pageunmap(u64 *pgt, u64 va, u64 size);

void *early_fdt_map(void *fdt_phys);
void early_map_earlymem(u64 pstart, u64 pend);

u64 ipa2pa(u64 *pgt, u64 ipa);
u64 at_uva2pa(u64 uva);
u64 at_uva2ipa(u64 uva);

void page_access_invalidate(u64 *pgt, u64 va);
void page_access_ro(u64 *pgt, u64 val);
bool page_accessible(u64 *pgt, u64 va);
u64 *page_accessible_pte(u64 *pgt, u64 va);
u64 *page_rwable_pte(u64 *pgt, u64 va);
u64 *page_ro_pte(u64 *pgt, u64 va);

void copy_to_guest(u64 *pgt, u64 to_ipa, char *from, u64 len);
void copy_from_guest(u64 *pgt, char *to, u64 from_ipa, u64 len);

void map_guest_image(u64 *pgt, struct guest *img, u64 ipa);
void alloc_guestmem(u64 *pgt, u64 ipa, u64 size);

void dump_par_el1(void);

u64 faulting_ipa_page(void);

void *iomap(u64 pa, u64 size);

void *setup_pagetable(u64 fdt_base);

#endif  /* __ASSEMBLER__ */

#endif  /* CORE_MM_H */
