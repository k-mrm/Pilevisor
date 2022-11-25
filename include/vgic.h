#ifndef MVMM_VGIC_H
#define MVMM_VGIC_H

#include "types.h"
#include "param.h"
#include "gic.h"
#include "spinlock.h"

struct vcpu;

struct vgic_irq {
  u16 intid;
  u8 priority;  /* ipriorityr */
  bool enabled: 1;
  u8 igroup: 1;
};

struct vgic {
  int nspis;
  u32 ctlr;     /* GICD_CTLR */
  struct vgic_irq *spis;

  spinlock_t lock;
};

/* vgic cpu interface */
struct vgic_cpu {
  struct vgic_irq sgis[GIC_NSGI];
  struct vgic_irq ppis[GIC_NPPI];
};

void vgic_cpu_init(struct vcpu *vcpu);
int vgic_inject_virq(struct vcpu *vcpu, u32 pirq, u32 virq, int grp);
void vgic_restore_state(struct vgic_cpu *vgic);
int vgic_emulate_sgi1r(struct vcpu *vcpu, int rt, int wr);

void vgic_init(void);

#endif
