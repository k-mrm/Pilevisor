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

struct pending_queue {
  struct gic_pending_irq *irqs[4];
  int head;
  int tail;  

  spinlock_t lock;
};

struct vm;
struct vcpu {
  /* !!! MUST be first field !!! */
  struct {
    u64 x[31];
    u64 spsr;   /* spsr_el2 */
    u64 elr;    /* elr_el2 */
    u64 sp;     /* stack pointer */
  } reg;

  /* vcpuid on cluster */
  int vcpuid;
  u64 vmpidr;

  struct cpu_features features;

  u64 sctlr_el1;

  struct vgic_cpu vgic;
  /* pending irqs */
  struct pending_queue pending;

  /* when dabort occurs on vCPU, informations will save here */
  struct dabort_info dabt;

  spinlock_t lock;

  bool initialized;
  bool online;
  bool last;
} __cacheline_aligned;
  
extern struct vcpu *vcpu0;

extern int nr_cluster_online_vcpus;

void vcpu_entry(void);

void vcpuid_init(u32 *vcpuids, int nvcpu);

void vcpu_init_core(void);
void vcpu_preinit(void);
void wait_for_current_vcpu_online(void);

void vcpu_dump(struct vcpu *vcpu);

#define current   ((struct vcpu *)read_sysreg(tpidr_el2))

static inline void set_current_vcpu(struct vcpu *vcpu) {
  write_sysreg(tpidr_el2, vcpu);
}

#endif
