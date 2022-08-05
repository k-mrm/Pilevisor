#ifndef MVMM_VCPU_H
#define MVMM_VCPU_H

#include "types.h"
#include "node.h"
#include "param.h"
#include "vgic.h"
#include "gic.h"
#include "aarch64.h"
#include "mm.h"

enum vcpu_state {
  UNUSED,
  CREATED,
  READY,
  RUNNING,
};

struct cpu_features {
  u64 pfr0;
};

struct vcpu {
  struct {
    u64 x[31];
    u64 spsr;
    u64 elr;
  } reg;
  struct {
    u64 spsr_el1;
    u64 elr_el1;
    u64 mpidr_el1;
    u64 midr_el1;
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

  struct cpu_features features;
  const char *name;
  enum vcpu_state state;

  struct gic_state gic;
  struct vgic_cpu *vgic;

  struct node *node;

  int cpuid;

  /* when dabort occurs on vCPU, informations will save here */
  struct dabort_info dabt;
};

struct vcpu *new_vcpu(struct node *node, int vcpuid);
void free_vcpu(struct vcpu *vcpu);

void vcpu_ready(struct vcpu *vcpu);

void enter_vcpu(void);

void vcpu_init(void);

void vcpu_dump(struct vcpu *vcpu);

static inline bool vcpu_running(struct vcpu *vcpu) {
  return vcpu->state == RUNNING;
}

static inline struct vcpu *cur_vcpu() {
  return (struct vcpu *)read_sysreg(tpidr_el2);
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
