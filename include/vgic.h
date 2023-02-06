#ifndef CORE_VGIC_H
#define CORE_VGIC_H

#include "types.h"
#include "param.h"
#include "gic.h"
#include "spinlock.h"
#include "irq.h"

struct vcpu;
struct mmio_access;

enum {
  CONFIG_LEVEL  = 0,
  CONFIG_EDGE   = 1,
};

struct vgic_irq {
  u16 intid;
  struct vcpu *target;    /* target vcpu: if NULL, target in remote node */

  union {
    u64 vcpuid;           /* for GICv3: target vmpidr */
    u8 targets;           /* for GICv2: targets bitmap */ 
  };

  u8 priority;

  u8 req_cpu;             /* for GICv2 */

  bool enabled: 1;
  bool hw: 1;
  u8 igroup: 1;
  u8 cfg: 1;

  int hwirq;

  spinlock_t lock;
};

struct vgic {
  int nspis;
  int version;
  struct vgic_irq *spis;
  bool enabled: 1;

  spinlock_t lock;
};

struct vgic_v2_cpu {

};

struct vgic_v3_cpu {
  
};

/* vgic cpu interface */
struct vgic_cpu {
  union {
    struct vgic_v2_cpu v2;
    struct vgic_v3_cpu v3;
  };

  struct vgic_irq sgis[GIC_NSGI];
  struct vgic_irq ppis[GIC_NPPI];
};

void vgic_cpu_init(struct vcpu *vcpu);
int vgic_inject_virq(struct vcpu *vcpu, u32 intid);
int vgic_emulate_sgi1r(struct vcpu *vcpu, int rt, int wr);

int vgic_emulate_sgi(struct vcpu *vcpu, struct gic_sgi *sgi);

void vgic_enable_irq(struct vcpu *vcpu, struct vgic_irq *irq);
void vgic_disable_irq(struct vcpu *vcpu, struct vgic_irq *irq);
void vgic_inject_pending_irqs(void);

struct vgic_irq *vgic_get_irq(struct vcpu *vcpu, int intid);
void vgic_connect_hwirq(int virq_no, int hwirq_no);

void vgic_iidr_read(struct vcpu *vcpu, struct mmio_access *mmio);
void vgic_igroup_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_igroup_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_ienable_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_isenable_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_icenable_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_ipend_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_ipriority_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_ipriority_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_icfg_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);
void vgic_icfg_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset);

void vgic_init(void);

#endif
