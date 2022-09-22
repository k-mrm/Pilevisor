#ifndef MVMM_VGIC_H
#define MVMM_VGIC_H

#include "types.h"
#include "param.h"
#include "gic.h"
#include "spinlock.h"

struct vcpu;
struct node;

struct vgic_irq {
  struct vcpu *target;
  u8 priority;  /* ipriorityr */
  u8 enabled: 1;
  u8 igroup: 1;
};

struct vgic {
  int spi_max;
  int nspis;
  u32 ctlr;     /* GICD_CTLR */
  struct vgic_irq *spis;

  spinlock_t lock;
};

/* vgic cpu interface */
struct vgic_cpu {
  u16 used_lr;
  struct vgic_irq sgis[GIC_NSGI];
  struct vgic_irq ppis[GIC_NPPI];
};

void vgic_irq_enter(struct vcpu *vcpu);
struct void vgic_cpu_init(struct vcpu *vcpu);
int vgic_inject_virq(struct vcpu *vcpu, u32 pirq, u32 virq, int grp);
void vgic_restore_state(struct vgic_cpu *vgic);

void vgic_init(void);

#endif
