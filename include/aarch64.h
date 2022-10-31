#ifndef MVMM_AARCH64_H
#define MVMM_AARCH64_H

#include "types.h"

#define arm_sysreg(op1, crn, crm, op2)  \
  s3_ ## op1 ## _ ## crn ## _ ## crm ## _ ## op2

#define HFGITR_EL2    arm_sysreg(4, c1, c1, 6)

#define __read_sysreg(reg) \
  ({u64 val; asm volatile("mrs %0, " #reg : "=r"(val)); val;})
#define read_sysreg(reg)  __read_sysreg(reg)

#define __write_sysreg(reg, val)  \
  asm volatile("msr " #reg ", %0" : : "r"(val))
#define write_sysreg(reg, val)  \
  do { u64 x = (u64)(val); __write_sysreg(reg, x); } while(0)

#define intr_enable()   asm volatile("msr daifclr, #0xf" ::: "memory")
#define intr_disable()  asm volatile("msr daifset, #0xf" ::: "memory")

#define local_irq_enable()    asm volatile("msr daifclr, #0x2" ::: "memory")
#define local_irq_disable()   asm volatile("msr daifset, #0x2" ::: "memory")

#define isb()     asm volatile("isb");
#define dsb(ty)   asm volatile("dsb " #ty);

#define wfi()     asm volatile("wfi");

#define sev()     asm volatile("sev");
#define sevl()    asm volatile("sevl");

#define HCR_VM    (1<<0)
#define HCR_SWIO  (1<<1)
#define HCR_FMO   (1<<3)
#define HCR_IMO   (1<<4)
#define HCR_TWI   (1<<13)
#define HCR_TWE   (1<<14)
#define HCR_TID3  (1<<18)
#define HCR_TSC   (1<<19)
#define HCR_TGE   (1<<27)
#define HCR_TDZ   (1<<28)
#define HCR_RW    (1<<31)

#define HPFAR_FIPA_MASK   0xfffffffffff

#define MPIDR_AFFINITY_LEVEL0(m)    ((m) & 0xff)
#define MPIDR_AFFINITY_LEVEL1(m)    (((m) >> 8) & 0xff)
#define MPIDR_AFFINITY_LEVEL2(m)    (((m) >> 16) & 0xff)
#define MPIDR_AFFINITY_LEVEL3(m)    (((m) >> 32) & 0xff)

#define PSR_EL1H      (5)

#define SPSR_EL(spsr) (((spsr) & 0xf) >> 2)

static inline int cpuid() {
  int mpidr = read_sysreg(mpidr_el1);
  return mpidr & 0xf;
}

static inline u64 at_ipa2pa(u64 ipa) {
  asm volatile("at s12e1r, %0" :: "r"(ipa) : "memory");
  return read_sysreg(par_el1);
}

static inline bool local_irq_enabled() {
  return (read_sysreg(daif) >> 3) & 0x1;
}

static inline u64 r_sp() {
  u64 x;
  asm volatile("mov %0, sp" : "=r"(x));
  return x;
}

static inline void tlb_flush() {
  dsb(ishst);
  asm volatile("tlbi vmalls12e1");
  dsb(ish);
  isb();
}

#endif
