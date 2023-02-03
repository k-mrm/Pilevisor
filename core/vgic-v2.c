/*
 *  virtual GICv3
 */

#include "types.h"
#include "vcpu.h"
#include "pcpu.h"
#include "gic.h"
#include "gicv2.h"
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

static void vgicd_ctlr_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u32 val = 0;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  if(vgic->enabled)
    val |= GICD_CTLR_EnableGrp1;

  mmio->val = val;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgicd_ctlr_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  if(mmio->val & GICD_CTLR_EnableGrp1)
    vgic->enabled = true;
  else
    vgic->enabled = false;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgicd_typer_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u32 val;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  /* ITLinesNumber */
  val = ((vgic->nspis + 32) >> 5) - 1;
  /* CPUNumber */
  val |= (8 - 1) << GICD_TYPER_CPUNum_SHIFT;
  /* SecurityExtn, LSPI disabled */

  mmio->val = val;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgic_v2_itargets_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 4;
  u8 targets;
  u32 itar = 0;
  u64 flags;

  for(int i = 0; i < 4; i++) {
    irq = vgic_get_irq(vcpu, intid + i);

    spin_lock_irqsave(&irq->lock, flags);
    targets = irq->targets;
    itar |= (u32)targets << (i * 8);
    spin_unlock_irqrestore(&irq->lock, flags);
  }

  mmio->val = itar;
}

static void vgic_v2_itargets_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 val = mmio->val;
  u64 flags;
  int intid = offset / sizeof(u32) * 4;

  for(int i = 0; i < 4; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    
    spin_lock_irqsave(&irq->lock, flags);
    irq->targets = (u8)(val >> (i * 8));
    spin_unlock_irqrestore(&irq->lock, flags);
  }
}

static void vgicd_icpidr2_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  /* GICv2 */
  mmio->val = 0x2 << GICD_ICPIDR2_ArchRev_SHIFT;
}

static int vgic_v2_d_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  u64 offset = mmio->offset;

  if(!(mmio->accsize & ACC_WORD))
    panic("unimplemented %d", mmio->accsize*8);

  switch(offset) {
    case GICD_CTLR:
      vgicd_ctlr_read(vcpu, mmio);
      return 0;

    case GICD_TYPER:
      vgicd_typer_read(vcpu, mmio);
      return 0;

    case GICD_IIDR:
      vgicd_iidr_read(vcpu, mmio);
      return 0;

    case GICD_TYPER2:
      /* linux's gicv3 driver accesses GICD_TYPER2 (offset 0xc) */
      mmio->val = 0;
      return 0;

    case GICD_IGROUPR(0) ... GICD_IGROUPR(31)+3:
      vgic_igroup_read(vcpu, mmio, offset - GICD_IGROUPR(0));
      return 0;

    case GICD_ISENABLER(0) ... GICD_ISENABLER(31)+3:
      vgic_ienable_read(vcpu, mmio, offset - GICD_ISENABLER(0));
      return 0;

    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3:
      vgic_ienable_read(vcpu, mmio, offset - GICD_ICENABLER(0));
      return 0;

    case GICD_ISPENDR(0) ... GICD_ISPENDR(31)+3:
      vgic_ipend_read(vcpu, mmio, offset - GICD_ISPENDR(0));
      return 0;

    case GICD_ICPENDR(0) ... GICD_ICPENDR(31)+3:
      vgic_ipend_read(vcpu, mmio, offset - GICD_ICPENDR(0));
      return 0;

    case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31)+3:
    case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31)+3:
      mmio->val = 0;

    case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254)+3:
      vgic_ipriority_read(vcpu, mmio, offset - GICD_IPRIORITYR(0));
      return 0;

    case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254)+3:
      vgic_v2_itargets_read(vcpu, mmio, offset - GICD_ITARGETSR(0));
      return 0;

    case GICD_ICFGR(0) ... GICD_ICFGR(63)+3:
      vgic_icfg_read(vcpu, mmio, offset - GICD_ICFGR(0));
      return 0;
  }

  vmm_warn("vgicv2: dist mmio read: unhandled %p\n", offset);
  return -1;
}

static int vgic_v2_d_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  u64 offset = mmio->offset;

  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d %p", __func__, mmio->accsize*8, offset);

  switch(offset) {
    case GICD_CTLR:
      vgicd_ctlr_write(vcpu, mmio);
      return 0;

    case GICD_IIDR:
      vgicd_iidr_write(vcpu, mmio);
      return 0;

    case GICD_TYPER:
      goto readonly;

    case GICD_IGROUPR(0) ... GICD_IGROUPR(31)+3:
      goto readonly;

    case GICD_ISENABLER(0) ... GICD_ISENABLER(31)+3:
      vgicd_isenable_write(vcpu, mmio, offset - GICD_ISENABLER(0));
      return 0;

    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3:
      vgicd_icenable_write(vcpu, mmio, offset - GICD_ICENABLER(0));
      return 0;

    case GICD_ISPENDR(0) ... GICD_ISPENDR(31)+3:
    case GICD_ICPENDR(0) ... GICD_ICPENDR(31)+3:
    case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31)+3:
      goto unimplemented;

    case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31)+3:
      goto reserved;

    case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254)+3:
      vgic_ipriority_write(vcpu, mmio, offset - GICD_IPRIORITYR(0));
      return 0;

    case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254)+3:
      vgic_v2_itargets_write(vcpu, mmio, offset - GICD_ITARGETSR(0));
      return 0;

    case GICD_ICFGR(0) ... GICD_ICFGR(63)+3:
      vgic_icfg_write(vcpu, mmio, offset - GICD_ICFGR(0));
      return 0;
  }

  vmm_warn("vgicd_mmio_write: unhandled %p\n", offset);
  return -1;

readonly:
  /* write ignored */
  vmm_warn("vgicd_mmio_write: readonly register %p\n", offset);
  return 0;

unimplemented:
  vmm_warn("vgicd_mmio_write: unimplemented %p %p %p %p\n",
           offset, val, current->reg.elr, current->reg.x[30]);
  return 0;

reserved:
  vmm_warn("vgicd_mmio_write: reserved %p\n", offset);
  return 0;
}

void vgic_v2_init(struct vgic *vgic) {
  vgic->archrev = 2;

  vmmio_reg_handler(0x08000000, 0x10000, vgic_v2_d_mmio_read, vgic_v2_d_mmio_write);
}
