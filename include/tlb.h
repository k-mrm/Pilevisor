#ifndef CORE_TLB_H
#define CORE_TLB_H

#include "aarch64.h"
#include "mm.h"

static inline void tlb_vmm_flush_all() {
  dsb(ishst);
  asm volatile("tlbi  alle2" ::: "memory");
  dsb(ish);
  isb();
}

static inline void tlb_s2_flush_all() {
  dsb(ishst);
  asm volatile("tlbi  vmalls12e1" ::: "memory");
  dsb(ish);
  isb();
}

static inline void tlb_s2_flush_ipa(u64 ipa) {
  dsb(ishst);
  asm volatile("tlbi  ipas2e1, %0" :: "r"(ipa >> PAGESHIFT) : "memory");
  dsb(ish);
  isb();
}

#endif
