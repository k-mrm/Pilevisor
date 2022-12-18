#ifndef CORE_S2MM_H
#define CORE_S2MM_H

#include "types.h"
#include "mm.h"

void map_guest_peripherals(u64 *pgt);
void vmiomap_passthrough(u64 *s2pgt, u64 pa, u64 size);

void s2mmu_init_core(void);
void s2mmu_init(void);

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

u64 *s2_readable_pte(u64 *s2pgt, u64 ipa);

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
