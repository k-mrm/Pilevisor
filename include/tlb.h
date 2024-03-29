#ifndef CORE_TLB_H
#define CORE_TLB_H

#include "aarch64.h"
#include "mm.h"
#include "compiler.h"

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

#ifdef BUILD_QEMU

/* QEMU does not emulate tlbi ipas2e1 ;; */

static inline void tlb_s2_flush_ipa(u64 __unused ipa) {
  tlb_s2_flush_all();
}

#else   /* !BUILD_QEMU */

static inline void tlb_s2_flush_ipa(u64 ipa) {
  dsb(ishst);
  asm volatile("tlbi  ipas2e1, %0" :: "r"(ipa >> PAGESHIFT) : "memory");
  dsb(ish);
  isb();
}

#endif  /* BUILD_QEMU */

#endif  /* CORE_TLB_H */
