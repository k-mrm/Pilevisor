/*
 *  virtual GICv3
 */

#include "types.h"
#include "vcpu.h"
#include "pcpu.h"
#include "gic.h"
#include "gicv3.h"
#include "vgic.h"
#include "log.h"
#include "vmmio.h"
#include "allocpage.h"
#include "malloc.h"
#include "lib.h"
#include "localnode.h"
#include "node.h"
#include "panic.h"
#include "assert.h"
#include "msg.h"

static inline void virq_set_target(struct vgic_irq *virq, u64 vcpuid) {
  virq->vcpuid = vcpuid;
  virq->target = node_vcpu(vcpuid);
}

static void vgicd_ctlr_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u32 val = GICD_CTLR_SS_ARE | GICD_CTLR_DS;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  if(vgic->enabled)
    val |= GICD_CTLR_SS_ENGRP1;

  mmio->val = val;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgicd_ctlr_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  if(mmio->val & GICD_CTLR_SS_ENGRP1)
    vgic->enabled = true;
  else
    vgic->enabled = false;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgicd_pidr2_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;

  mmio->val = vgic->archrev << GICD_PIDR2_ArchRev_SHIFT;
}

static void vgicd_typer_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  /* ITLinesNumber */
  u32 val = ((vgic->nspis + 32) >> 5) - 1;
  /* CPU Number */
  val |= (8 - 1) << GICD_TYPER_CPUNum_SHIFT;
  /* MBIS LPIS disabled */
  /* IDbits */
  val |= (10 - 1) << GICD_TYPER_IDbits_SHIFT;
  /* A3V disabled */

  mmio->val = val;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgicr_typer_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  u64 typer = gicr_typer_affinity(vcpu->vmpidr);

  typer |= GICR_TYPER_PROC_NUM(vcpu->vcpuid);

  if(vcpu->last)
    typer |= GICR_TYPER_LAST;

  mmio->val = typer;
}

static int __vgicr_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid;
  struct vgic_irq *irq;
  struct vgic *vgic = localvm.vgic;
  u64 offset = mmio->offset;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d %p", __func__, mmio->accsize*8, offset);
    */

  switch(offset) {
    case GICR_CTLR:
    case GICR_WAKER:
    case GICR_IGROUPR0:
      /* no op */
      mmio->val = 0;
      return 0;

    case GICR_IIDR:

    case GICR_TYPER:

    case GICR_PIDR2:
      return 0;

    case GICR_ISENABLER0:
    case GICR_ICENABLER0:

    case GICR_ISPENDR0:
      return 0;

    case GICR_ICPENDR0:
      return 0;

    case GICR_ISACTIVER0:
    case GICR_ICACTIVER0:
      mmio->val = 0;
      return 0;

    case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7):

    case GICR_ICFGR0 ... GICR_ICFGR1:

    case GICR_IGRPMODR0:
      mmio->val = 0;
      return 0;
  }

  vmm_warn("vgicr_mmio_read: unhandled %p\n", offset);
  return -1;

badwidth:
  vmm_warn("bad width: %d %p", mmio->accsize*8, offset);
  mmio->val = 0;
  return -1;
}

static int __vgicr_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  u64 offset = mmio->offset;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d", __func__, mmio->accsize*8);
    */

  switch(offset) {
    case GICR_CTLR:
    case GICR_WAKER:
    case GICR_IGROUPR0:
      /* no op */
      return 0;

    case GICR_TYPER:
    case GICR_PIDR2:
      goto readonly;

    case GICR_ISENABLER0:
      vgic_isenable_write(vcpu, mmio, offset - GICR_ISENABLER0);
      return 0;

    case GICR_ICENABLER0:
      vgic_icenable_write(vcpu, mmio, offset - GICR_ICENABLER0);
      return 0;

    case GICR_ICPENDR0:
      goto unimplemented;

    case GICR_ISACTIVER0:
    case GICR_ICACTIVER0:
      goto unimplemented;

    case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7):
      vgic_ipriority_write(vcpu, mmio, offset - GICR_IPRIORITYR(0));
      return 0;

    case GICR_ICFGR0 ... GICR_ICFGR1:
      vgic_icfg_write(vcpu, mmio, offset - GICR_ICFGR0);
      return 0;

    case GICR_IGRPMODR0:
      /* no op */
      return 0;
  }

  vmm_warn("vgicr_mmio_write: unhandled %p\n", offset);
  return -1;

unimplemented:
  vmm_warn("vgicr_mmio_write: unimplemented %p %p\n", offset, val);
  return 0;

readonly:
  vmm_log("vgicr_mmio_write: readonly %p\n", offset);
  return 0;
}

static int vgicr_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  u32 ridx = mmio->offset / 0x20000;
  u32 roffset = mmio->offset % 0x20000;
  mmio->offset = roffset;

  vcpu = node_vcpu(ridx);
  if(!vcpu)
    return vmmio_forward(ridx, mmio);

  return __vgicr_mmio_read(vcpu, mmio);
}

static int vgicr_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  u32 ridx = mmio->offset / 0x20000;
  u32 roffset = mmio->offset % 0x20000;
  mmio->offset = roffset;

  vcpu = node_vcpu(ridx);
  if(!vcpu)
    return vmmio_forward(ridx, mmio);

  return __vgicr_mmio_write(vcpu, mmio);
}

static int vgits_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  switch(mmio->offset) {
    case GITS_PIDR2:
      /* GITS unsupported */
      mmio->val = 0;
      return 0;
    default:
      vmm_log("vgits_mmio_read unknown\n");
      return -1;
  }
}

static int vgits_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  return -1;
}

static inline u64 irouter_to_vcpuid(u64 irouter) {
  return irouter & ((1 << 24) - 1);
}

static inline u64 vcpuid_to_irouter(u64 vcpuid) {
  return vcpuid & ((1 << 24) - 1);
}

static void vgic_set_irouter(struct vgic_irq *virq, u64 irouter) {
  u64 vcpuid;

  if(irouter & GICD_IROUTER_IRM) {
    vcpuid = local_vcpu(0)->vcpuid;
  else
    vcpuid = irouter_to_vcpuid(irouter);

  virq_set_target(virq, vcpuid);
}

static void vgic_v3_irouter_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u64);
  u64 flags;

  irq = vgic_get_irq(vcpu, intid);
  if(!irq)
    return;

  spin_lock_irqsave(&irq->lock, flags);
  mmio->val = vcpuid_to_irouter(irq->vcpuid);
  spin_unlock_irqrestore(&irq->lock, flags);
}

static void vgic_v3_irouter_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u64);
  u64 flags;

  irq = vgic_get_irq(vcpu, intid);
  if(!irq)
    return;

  spin_lock_irqsave(&irq->lock, flags);
  vgic_set_irouter(irq, mmio->val);
  spin_unlock_irqrestore(&irq->lock, flags);
}

int vgic_emulate_sgi1r(struct vcpu *vcpu, int rt, int wr) {
  /* write only register */
  if(!wr)
    return -1;

  u64 sgir = rt == 31 ? 0 : vcpu->reg.x[rt];

  return vgic_emulate_sgir(vcpu, sgir);
}

static int vgic_v3_d_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  case GICD_IROUTER(0) ... GICD_IROUTER(31)+3:
    goto reserved;
  case GICD_IROUTER(32) ... GICD_IROUTER(1019)+3:
  case GICD_PIDR2:
    goto end;
}

static int vgic_v3_d_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  case GICD_IROUTER(0) ... GICD_IROUTER(31)+7:
    goto reserved;

  case GICD_IROUTER(32) ... GICD_IROUTER(1019)+7:

  case GICD_PIDR2:
    goto readonly;
}

void vgic_v3_init(struct vgic *vgic) {
  vgic->archrev = 3;

  vmmio_reg_handler(0x08000000, 0x10000, vgic_v3_d_mmio_read, vgic_v3_d_mmio_write);
  vmmio_reg_handler(0x080a0000, 0xf60000, vgicr_mmio_read, vgicr_mmio_write);
  vmmio_reg_handler(0x08080000, 0x20000, vgits_mmio_read, vgits_mmio_write);
}
