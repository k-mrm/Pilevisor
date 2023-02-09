/*
 *  virtual GICv2
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
#include "s2mm.h"

static physaddr_t host_vbase = 0;
static bool pre_initialized = 0;

static void vgic_v2_ctlr_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u32 val = 0;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  if(vgic->enabled)
    val |= GICD_CTLR_EnableGrp1;

  mmio->val = val;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgic_v2_ctlr_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  if(mmio->val & GICD_CTLR_EnableGrp1)
    vgic->enabled = true;
  else
    vgic->enabled = false;

  spin_unlock_irqrestore(&vgic->lock, flags);
}

static void vgic_v2_typer_read(struct vcpu *vcpu, struct mmio_access *mmio) {
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
  int intid = offset;
  u32 itar = 0;
  int len = mmio->accsize;
  u64 flags;

  for(int i = 0; i < len; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);
    itar |= (u32)irq->targets << (i * 8);
    spin_unlock_irqrestore(&irq->lock, flags);
  }

  vmm_warn("get targets %d %p\n", intid, itar);

  mmio->val = itar;
}

void vgic_v2_set_targets(struct vcpu *vcpu, int intid, int len, u32 val) {
  struct vgic_irq *irq;
  int target, targets;
  u64 flags;

  for(int i = 0; i < len; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);

    irq->targets = targets = (u8)(val >> (i * 8));

    vmm_warn("set targets %d %p\n", intid + i, targets);

    target = __builtin_ffs(targets);
    if(target) {
      irq->target = node_vcpu(target - 1);
    } else {
      irq->target = NULL;
    }

    if(irq->hw) {
      int t;

      if(irq->target)
        t = 1 << pcpu_id(irq->target->pcpu);
      else
        t = 1 << 0;     // route to 0

      localnode.irqchip->set_targets(irq->hwirq, t);
    }

    spin_unlock_irqrestore(&irq->lock, flags);
  }
}

static void vgic_v2_itargets_write(struct vcpu *vcpu, struct mmio_access *mmio,
                                   u64 offset) {
  u32 val = mmio->val;
  int intid = offset, len = mmio->accsize;
  
  vmm_warn("vgic itargets write %p %p\n", intid, val);

  vgic_v2_set_targets(vcpu, intid, len, val);

  /* if intid is spi, forward itargets value to other node */
  if(is_spi(intid)) {
    struct msg msg;
    struct gic_config_msg_hdr cfg;

    cfg.type = GIC_CONFIG_SET_TARGET_V2;
    cfg.len = len;
    cfg.intid = intid;
    cfg.value = val;

    msg_init(&msg, 0, MSG_GIC_CONFIG, &cfg, NULL, 0, M_BCAST);

    send_msg(&msg);
  }
}

static void vgic_v2_icpidr2_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  /* GICv2 */
  mmio->val = 0x2 << GICD_ICPIDR2_ArchRev_SHIFT;
}

static void vgic_v2_sgir_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct gic_sgi sgi;
  struct vgic_irq *irq;
  u32 sgir = mmio->val;

  int virq = sgir & 0xf;
  irq = vgic_get_irq(vcpu, virq);

  irq->req_cpu = vcpu->vcpuid;

  sgi.targets = (sgir >> GICD_SGIR_TargetList_SHIFT) & 0xff;
  sgi.sgi_id = virq;
  sgi.mode = (sgir >> GICD_SGIR_TargetListFilter_SHIFT) & 0x3;

  vgic_emulate_sgi(vcpu, &sgi);
}

static int vgic_v2_d_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  u64 offset = mmio->offset;

  if(!(mmio->accsize & ACC_WORD))
    panic("unimplemented %d", mmio->accsize*8);

  switch(offset) {
    case GICD_CTLR:
      vgic_v2_ctlr_read(vcpu, mmio);
      return 0;

    case GICD_TYPER:
      vgic_v2_typer_read(vcpu, mmio);
      return 0;

    case GICD_IIDR:
      vgic_iidr_read(vcpu, mmio);
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

    case GICD_ICPIDR2:
      vgic_v2_icpidr2_read(vcpu, mmio);
      return 0;
  }

  vmm_warn("vgicv2: dist mmio read: unhandled %p\n", offset);
  return -1;
}

static int vgic_v2_d_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  u64 offset = mmio->offset;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d %p", __func__, mmio->accsize*8, offset);
    */

  switch(offset) {
    case GICD_CTLR:
      vgic_v2_ctlr_write(vcpu, mmio);
      return 0;

    case GICD_IIDR:
    case GICD_TYPER:
      goto readonly;

    case GICD_SGIR:
      vgic_v2_sgir_write(vcpu, mmio);
      return 0;

    case GICD_IGROUPR(0) ... GICD_IGROUPR(31)+3:
      vgic_igroup_write(vcpu, mmio, offset - GICD_IGROUPR(0));
      return 0;

    case GICD_ISENABLER(0) ... GICD_ISENABLER(31)+3:
      vgic_isenable_write(vcpu, mmio, offset - GICD_ISENABLER(0));
      return 0;

    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3:
      vgic_icenable_write(vcpu, mmio, offset - GICD_ICENABLER(0));
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

    case GICD_ICPIDR2:
      goto readonly;
  }

  vmm_warn("vgicd_mmio_write: unhandled %p\n", offset);
  return -1;

readonly:
  /* write ignored */
  vmm_warn("vgicd_mmio_write: readonly register %p\n", offset);
  return 0;

unimplemented:
  vmm_warn("vgicd_mmio_write: unimplemented %p %p %p\n",
           offset, current->reg.elr, current->reg.x[30]);
  return 0;

reserved:
  vmm_warn("vgicd_mmio_write: reserved %p\n", offset);
  return 0;
}

void vgic_v2_pre_init(physaddr_t vbase) {
  host_vbase = vbase;

  pre_initialized = true;
}

void vgic_v2_init(struct vgic *vgic) {
  if(!pre_initialized) {
    vmm_warn("host gic?\n");
    return;
  }

  vgic->version = 2;

  vmmio_reg_handler(0x08000000, 0x10000, vgic_v2_d_mmio_read, vgic_v2_d_mmio_write);

  vmiomap(0x08010000, host_vbase, 0x1000);
}
