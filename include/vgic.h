#ifndef CORE_VGIC_H
#define CORE_VGIC_H

#include "types.h"
#include "param.h"
#include "gic.h"
#include "spinlock.h"

struct vcpu;

enum {
  CONFIG_LEVEL  = 0,
  CONFIG_EDGE   = 1,
};

struct vgic_irq {
  struct vcpu *target;  /* target vcpu: if NULL, target in remote node */
  u64 vcpuid;           /* target vcpuid: now Aff0 only */
  u16 intid;
  u8 priority;
  bool enabled: 1;
  u8 igroup: 1;
  u8 cfg: 1;
};

struct vgic {
  int nspis;
  int archrev;
  struct vgic_irq *spis;
  bool enabled: 1;

  spinlock_t lock;
};

/* vgic cpu interface */
struct vgic_cpu {
  struct vgic_irq sgis[GIC_NSGI];
  struct vgic_irq ppis[GIC_NPPI];
};

void vgic_cpu_init(struct vcpu *vcpu);
int vgic_inject_virq(struct vcpu *vcpu, u32 intid);
int vgic_emulate_sgi1r(struct vcpu *vcpu, int rt, int wr);

void vgic_inject_pending_irqs(void);

void vgic_init(void);

#endif
