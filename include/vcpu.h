#ifndef MVMM_VCPU_H
#define MVMM_VCPU_H

#include "types.h"
#include "param.h"
#include "vgic.h"
#include "gic.h"
#include "aarch64.h"
#include "mm.h"
#include "vsm.h"

struct cpu_features {
  u64 pfr0;
};

struct pending_queue {
  u32 irqs[4];
  int head;
  int tail;  

  spinlock_t lk;
};

/* aarch64 vcpu */
struct vcpu {
  /* !!! MUST be first field !!! */
  struct {
    u64 x[31];
    u64 spsr;   /* spsr_el2 */
    u64 elr;    /* elr_el2 */
    u64 sp;     /* stack pointer */
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
  /* pending irqs */
  struct pending_queue pending;

  /* when dabort occurs on vCPU, informations will save here */
  struct dabort_info dabt;

  struct vsm_waitqueue vsm_waitqueue;

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

#endif
