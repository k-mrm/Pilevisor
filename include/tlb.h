#ifndef CORE_TLB_H
#define CORE_TLB_H

#include "aarch64.h"

#define tlbi(ty)    asm volatile("tlbi " #ty ::: "memory")

static inline void tlb_vmm_flush_all() {
  dsb(ishst);
  tlbi(alle2);
  dsb(ish);
  isb();
}

static inline void tlb_s2_flush_all() {
  dsb(ishst);
  tlbi(vmalls12e1);
  dsb(ish);
  isb();
}

static inline void tlb_s2_flush_ipa(u64 ipa) {
  ;
}

#endif
