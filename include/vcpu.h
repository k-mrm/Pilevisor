#ifndef MVMM_VCPU_H
#define MVMM_VCPU_H

#include "types.h"
#include "param.h"
#include "vgic.h"
#include "gic.h"
#include "aarch64.h"
#include "mm.h"

struct cpu_features {
  u64 pfr0;
};

struct vcpu {
  /* MUST be first field */
  struct {
    u64 x[31];
    u64 spsr;   /* spsr_el2 */
    u64 elr;    /* elr_el2 */
  } reg;
  struct {
    u64 spsr_el1;
    u64 elr_el1;
    u64 sp_el0;
    u64 sp_el1;
    u64 ttbr0_el1;
    u64 ttbr1_el1;
    u64 tcr_el1;
    u64 vbar_el1;
    u64 sctlr_el1;
    u64 cntv_ctl_el0;
    u64 cntv_tval_el0;
    u64 cntfrq_el0;
  } sys;

  /* vcpuid on cluster */
  int vcpuid;

  u64 vmpidr;

  struct cpu_features features;

  struct gic_state gic;
  struct vgic_cpu vgic;

  /* when dabort occurs on vCPU, informations will save here */
  struct dabort_info dabt;

  bool initialized;
  bool online;
  bool last;
};

extern int nr_cluster_online_vcpus;

void vcpu_entry(void);

void vcpuid_init(u32 *vcpuids, int nvcpu);

void vcpu_initstate(void);
void wait_for_current_vcpu_online(void);

void vcpu_dump(struct vcpu *vcpu);

#define current   ((struct vcpu *)read_sysreg(tpidr_el2))

static inline void set_current_vcpu(struct vcpu *vcpu) {
  write_sysreg(tpidr_el2, vcpu);
}

/* read general-purpose register */
static inline u64 vcpu_x(struct vcpu *vcpu, int r) {
  return r == 31 ? 0 : vcpu->reg.x[r];
}

static inline u32 vcpu_w(struct vcpu *vcpu, int r) {
  return r == 31 ? 0 : (u32)vcpu->reg.x[r];
}

/* write to general-purpose register */
static inline void vcpu_set_x(struct vcpu *vcpu, int r, u64 v) {
  vcpu->reg.x[r] = v;
}

static inline void vcpu_set_w(struct vcpu *vcpu, int r, u32 v) {
  vcpu->reg.x[r] = v;
}

#endif
