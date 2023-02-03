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

static void vgicd_ctlr_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u32 val = GICD_CTLR_SS_ARE | GICD_CTLR_DS;

  spin_lock(&vgic->lock);

  if(vgic->enabled)
    val |= GICD_CTLR_SS_ENGRP1;

  mmio->val = val;

  spin_unlock(&vgic->lock);
}

static void vgicd_ctlr_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;

  spin_lock(&vgic->lock);

  if(mmio->val & GICD_CTLR_SS_ENGRP1)
    vgic->enabled = true;
  else
    vgic->enabled = false;

  spin_unlock(&vgic->lock);
}

static void vgicd_pidr2_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;

  mmio->val = vgic->archrev << GICD_PIDR2_ArchRev_SHIFT;
}

static void vgicd_typer_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;

  spin_lock(&vgic->lock);

  /* ITLinesNumber */
  u32 val = ((vgic->nspis + 32) >> 5) - 1;
  /* CPU Number */
  val |= (8 - 1) << GICD_TYPER_CPUNum_SHIFT;
  /* MBIS LPIS disabled */
  /* IDbits */
  val |= (10 - 1) << GICD_TYPER_IDbits_SHIFT;
  /* A3V disabled */

  mmio->val = val;

  spin_unlock(&vgic->lock);
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
      mmio->val = 0x19 << GICD_IIDR_ProductID_SHIFT |   /* pocv2 product id = 0x19 */
                  vgic->archrev << GICD_IIDR_Revision_SHIFT |
                  0x43b;   /* ARM implementer */
      return 0;

    case GICR_TYPER: {
    }

    case GICR_PIDR2:
      mmio->val = vgic->archrev << GICD_PIDR2_ArchRev_SHIFT;
      return 0;

    case GICR_ISENABLER0:
    case GICR_ICENABLER0: {
      u32 iser = 0; 

      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, i);
        if(!irq)
          return 0;

        iser |= irq->enabled << i;
      }

      mmio->val = iser;
      return 0;
    }

    case GICR_ISPENDR0:
      panic("pa");
      return 0;

    case GICR_ICPENDR0:
      panic("pa");
      return 0;

    case GICR_ISACTIVER0:
    case GICR_ICACTIVER0:
      mmio->val = 0;
      return 0;

    case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7): {
      u32 ipr = 0;
      intid = (offset - GICR_IPRIORITYR(0)) / sizeof(u32) * 4;
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if(!irq)
          return 0;

        ipr |= irq->priority << (i * 8);
      }

      mmio->val = ipr;
      return 0;
    }

    case GICR_ICFGR0 ... GICR_ICFGR1: {
      u32 icfg = 0;
      intid = (offset - GICR_ICFGR0) / sizeof(u32) * 16;
      for(int i = 0; i < 16; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          return 0;

        if(irq->cfg == CONFIG_EDGE)
          icfg |= 0x2u << (i * 2);
      }

      mmio->val = icfg;
      return 0;
    }

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
  int intid;
  struct vgic_irq *irq;
  u64 offset = mmio->offset;
  u64 val = mmio->val;

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
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, i);
        if(!irq)
          return 0;

        if((val >> i) & 0x1) {
          vgic_enable_irq(vcpu, irq);
        }
      }

      return 0;

    case GICR_ICENABLER0:
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, i);
        if(!irq)
          return 0;

        if((val >> i) & 0x1) {
          vgic_disable_irq(vcpu, irq);
        }
      }

      return 0;

    case GICR_ICPENDR0:
      goto unimplemented;

    case GICR_ISACTIVER0:
    case GICR_ICACTIVER0:
      goto unimplemented;

    case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7):
      intid = (offset - GICR_IPRIORITYR(0)) / sizeof(u32) * 4;
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if(!irq)
          return 0;

        irq->priority = (val >> (i * 8)) & 0xff;
      }

      return 0;

    case GICR_ICFGR0 ... GICR_ICFGR1: {
      intid = (offset - GICR_ICFGR0) / sizeof(u32) * 16;
      for(int i = 0; i < 16; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          return 0;

        u8 c = (val >> (i * 2)) & 0x3;

        if(c >> 1 == CONFIG_LEVEL)
          irq->cfg = CONFIG_LEVEL;
        else
          irq->cfg = CONFIG_EDGE;
      }

      return 0;
    }

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
  int intid = (offset - GICD_IROUTER(0)) / sizeof(u64);

  irq = vgic_get_irq(vcpu, intid);
  if(!irq)
    return;

  spin_lock(&irq->lock);
  mmio->val = vcpuid_to_irouter(irq->vcpuid);
  spin_unlock(&irq->lock);
}

static void vgic_v3_irouter_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u64);

  irq = vgic_get_irq(vcpu, intid);
  if(!irq)
    return;

  spin_lock(&irq->lock);
  vgic_set_irouter(irq, mmio->val);
  spin_unlock(&irq->lock);
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
