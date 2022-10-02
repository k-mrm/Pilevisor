#include "aarch64.h"
#include "gic.h"
#include "vgic.h"
#include "log.h"
#include "param.h"
#include "vcpu.h"
#include "mmio.h"
#include "allocpage.h"
#include "lib.h"
#include "node.h"

static struct vgic vgic_dist;

extern int gic_lr_max;

static int vgic_cpu_alloc_lr(struct vgic_cpu *vgic) {
  for(int i = 0; i < gic_lr_max; i++) {
    if((vgic->used_lr & BIT(i)) == 0) {
      vgic->used_lr |= BIT(i);
      return i;
    }
  }

  vmm_warn("lr :(");
  return -1;
}

void vgic_irq_enter(struct vcpu *vcpu) {
  struct vgic_cpu *vgic = &vcpu->vgic;

  for(int i = 0; i < gic_lr_max; i++) {
    if((vgic->used_lr & BIT(i)) != 0) {
      u64 lr = gic_read_lr(i);
      /* already handled by guest */
      if(lr_is_inactive(lr))
        vgic->used_lr &= ~(u16)BIT(i);
    }
  }
}

static void vgic_irq_enable(struct vcpu *vcpu, int vintid) {
  int cpu = cpuid();
  vmm_log("cpu%d: enabled %d irq\n", cpu, vintid);

  if(is_sgi_ppi(vintid))
    gic_irq_enable_redist(cpu, vintid);
  else if(is_spi(vintid))
    gic_irq_enable(vintid);
  else
    panic("?");
}

static void vgic_irq_disable(struct vcpu *vcpu, int vintid) {
  if(is_sgi_ppi(vintid))
    gic_irq_disable(vintid);
  else if(is_spi(vintid))
    gic_irq_disable(vintid);
  else
    panic("?");
}

static void vgic_set_target(struct vcpu *vcpu, int vintid, u8 target) {
  if(is_sgi_ppi(vintid))
    return; /* ignored */
  else if(is_spi(vintid))
    gic_set_target(vintid, target);
  else
    panic("?");
}

static void vgic_dump_irq_state(struct vcpu *vcpu, int intid);

int vgic_inject_virq(struct vcpu *vcpu, u32 pirq, u32 virq, int grp) {
  /*
  if(!(vcpu->vm->vgic->ctlr & GICD_CTLR_ENGRP(grp))) {
    vmm_warn("vgicd disabled\n");
    return -1;
  }*/

  struct vgic_cpu *vgic = &vcpu->vgic;

  u64 lr = gic_make_lr(pirq, virq, grp);

  int n = vgic_cpu_alloc_lr(vgic);
  if(n < 0)
    panic("no lr");

  gic_write_lr(n, lr);

  return 0;
}

static struct vgic_irq *vgic_get_irq(struct vcpu *vcpu, int intid) {
  if(is_sgi(intid))
    return &vcpu->vgic.sgis[intid];
  else if(is_ppi(intid))
    return &vcpu->vgic.ppis[intid - 16];
  else if(is_spi(intid))
    return &localnode.vgic->spis[intid - 32];

  panic("vgic_get_irq unknown %d", intid);
  return NULL;
}

static void vgic_dump_irq_state(struct vcpu *vcpu, int intid) {
  struct vgic_irq *virq = vgic_get_irq(vcpu, intid);
  vmm_log("vgic irq dump %d\n", intid);
  vmm_log("%d %d %d %d\n", virq->priority, virq->target, virq->enabled, virq->igroup);
}

static int vgic_inject_sgi(struct vcpu *vcpu, u64 sgir) {
  /*
  struct node *node = &localnode;

  u16 targetlist = ICC_SGI1R_TargetList(sgir); 
  u8 intid = ICC_SGI1R_INTID(sgir);
  bool broadcast = ICC_SGI1R_IRM(sgir);

  // TODO: support Affinity
  for(int i = 0; i < 16; i++) {
    if(targetlist & BIT(i)) {
      if(i >= node->nvcpu)
        continue;

      struct vcpu *target = &node->vcpus[i];

      // vgic_inject_virq(target, intid, intid, 1);
    }
  }

  gic_set_sgi1r(sgir);

  return 0
  */
  panic("unimplemented vgic_inject_sgi");

  return -1;
}

int vgic_emulate_sgi1r(struct vcpu *vcpu, int rt, int wr) {
  /* write only register */
  if(!wr)
    return -1;

  u64 sgir = vcpu_x(vcpu, rt);
  u16 targetlist = ICC_SGI1R_TargetList(sgir); 
  u8 intid = ICC_SGI1R_INTID(sgir);

  // vmm_log("vgic sgi1r %p %p", targetlist, intid);

  return vgic_inject_sgi(vcpu, sgir);
}

static int vgicd_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid, idx;
  struct vgic_irq *irq;
  struct vgic *vgic = localnode.vgic;
  u64 offset = mmio->offset;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d", __func__, mmio->accsize*8);
    */

  acquire(&vgic->lock);

  switch(offset) {
    case GICD_CTLR:
      mmio->val = gicd_r(GICD_CTLR);
      goto end;
    case GICD_TYPER:
      mmio->val = gicd_r(GICD_TYPER);
      goto end;
    case GICD_IIDR:
      mmio->val = gicd_r(GICD_IIDR);
      goto end;
    case GICD_TYPER2:
      /* linux's gicv3 driver accessed GICD_TYPER2 (offset 0xc) */
      mmio->val = 0;
      goto end;
    case GICD_IGROUPR(0) ... GICD_IGROUPR(31)+3: {
      u32 igrp = 0;

      intid = (offset - GICD_IGROUPR(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        igrp |= (u32)irq->igroup << i;
      }

      mmio->val = igrp;
      goto end;
    }
    case GICD_ISENABLER(0) ... GICD_ISENABLER(31)+3: {
      u32 iser = 0;

      intid = (offset - GICD_ISENABLER(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        iser |= (u32)irq->enabled << i;
      }

      mmio->val = iser;
      goto end;
    }
    case GICD_ISPENDR(0) ... GICD_ISPENDR(31)+3:
    case GICD_ICPENDR(0) ... GICD_ICPENDR(31)+3:
      goto unimplemented;
    case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31)+3:
    case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31)+3:
      goto unimplemented;
    case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254)+3: {
      u32 ipr = 0;

      intid = offset - GICD_IPRIORITYR(0);
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        ipr |= (u32)irq->priority << (i * 8);
      }

      mmio->val = ipr;
      goto end;
    }
    case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254)+3: {
      u32 itar = 0;

      intid = offset - GICD_ITARGETSR(0);
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        u32 t = irq->target->vmpidr;
        itar |= (u32)t << (i * 8);
      }

      mmio->val = itar;
      goto end;
    }
    case GICD_ICFGR(0) ... GICD_ICFGR(63)+3:
      goto unimplemented;
    case GICD_IROUTER(0) ... GICD_IROUTER(31)+3:
      goto reserved;
    case GICD_IROUTER(32) ... GICD_IROUTER(1019)+3:
      goto unimplemented;
    case GICD_PIDR2:
      mmio->val = gicd_r(GICD_PIDR2);
      goto end;
  }

  vmm_warn("vgicd_mmio_read: unhandled %p\n", offset);

  release(&vgic->lock);
  return -1;

reserved:
  vmm_warn("read reserved\n");
  mmio->val = 0;
  goto end;

unimplemented:
  vmm_warn("vgicd_mmio_read: unimplemented %p\n", offset);
  mmio->val = 0;
  goto end;

end:
  release(&vgic->lock);
  return 0;
}

static int vgicd_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid;
  struct vgic_irq *irq;
  struct vgic *vgic = localnode.vgic;
  u64 offset = mmio->offset;
  u64 val = mmio->val;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d %p", __func__, mmio->accsize*8, offset);
    */

  switch(offset) {
    case GICD_CTLR:
    case GICD_IIDR:
    case GICD_TYPER:
      goto readonly;
    case GICD_IGROUPR(0) ... GICD_IGROUPR(31)+3:
      /*
      intid = (offset - GICD_IGROUPR(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        irq->igroup = (val >> i) & 0x1;
      }
      */
      goto end;
    case GICD_ISENABLER(0) ... GICD_ISENABLER(31)+3:
      intid = (offset - GICD_ISENABLER(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if((val >> i) & 0x1) {
          irq->enabled = 1;
          vgic_irq_enable(vcpu, intid+i);
        }
      }
      goto end;
    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3:
      intid = (offset - GICD_ISENABLER(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if((val >> i) & 0x1) {
          irq->enabled = 0;
          vgic_irq_disable(vcpu, intid+i);
        }
      }
      goto end;
    case GICD_ISPENDR(0) ... GICD_ISPENDR(31)+3:
    case GICD_ICPENDR(0) ... GICD_ICPENDR(31)+3:
    case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31)+3:
      goto unimplemented;
    case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31)+3:
      goto end;
    case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254)+3:
      intid = (offset - GICD_IPRIORITYR(0)) / sizeof(u32) * 4;
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        irq->priority = (val >> (i * 8)) & 0xff;
      }
      goto end;
    case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254)+3:
      intid = (offset - GICD_ITARGETSR(0)) / sizeof(u32) * 4;
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        u32 t = (val >> (i * 8)) & 0xff;
        irq->target = node_vcpu(t);
        if(!irq->target)
          panic("target remote");
        vgic_set_target(vcpu, intid+i, t);
      }
      goto end;
    case GICD_ICFGR(0) ... GICD_ICFGR(63)+3:
      goto unimplemented;
    case GICD_IROUTER(0) ... GICD_IROUTER(31)+3:
      goto reserved;
    case GICD_IROUTER(32) ... GICD_IROUTER(1019)+3:
      goto unimplemented;
    case GICD_PIDR2:
      goto readonly;
  }

  vmm_warn("vgicd_mmio_write: unhandled %p\n", offset);
  return -1;

readonly:
  /* write ignored */
  vmm_warn("vgicd_mmio_write: readonly register %p\n", offset);
  goto end;

unimplemented:
  vmm_warn("vgicd_mmio_write: unimplemented %p %p\n", offset, val);
  goto end;

reserved:
  vmm_warn("vgicd_mmio_write: reserved %p\n", offset);

end:
  return 0;
}

static int __vgicr_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid;
  struct vgic_irq *irq;
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
      mmio->val = gicr_r32(vcpu->vmpidr, GICR_IIDR);
      return 0;
    case GICR_TYPER: {
      u64 typer;

      if(!(mmio->accsize & ACC_DOUBLEWORD))
        goto badwidth;

      typer = gicr_typer_affinity(vcpu->vmpidr);

      typer |= GICR_TYPER_PROC_NUM(vcpu->vcpuid);

      if(vcpu->last)
        typer |= GICR_TYPER_LAST;

      mmio->val = typer;

      return 0;
    }
    case GICR_PIDR2:
      mmio->val = gicr_r32(vcpu->vmpidr, GICR_PIDR2);
      return 0;
    case GICR_ISENABLER0: {
      u32 iser = 0; 

      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, i);
        iser |= irq->enabled << i;
      }

      mmio->val = iser;
      return 0;
    }
    case GICR_ICENABLER0:
      goto unimplemented;
    case GICR_ICPENDR0:
      mmio->val = 0;
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
        ipr |= irq->priority << (i * 8);
      }

      mmio->val = ipr;
      return 0;
    }
    case GICR_ICFGR0:
    case GICR_ICFGR1:
    case GICR_IGRPMODR0:
      mmio->val = 0;
      return 0;
  }

  vmm_warn("vgicr_mmio_read: unhandled %p\n", offset);
  return -1;

unimplemented:
  vmm_warn("vgicr_mmio_read: unimplemented %p\n", offset);
  mmio->val = 0;
  goto end;

badwidth:
  vmm_warn("bad width: %d %p", mmio->accsize*8, offset);
  mmio->val = 0;
  goto end;

end:
  return 0;
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
        if((val >> i) & 0x1) {
          irq->enabled = 1;
          vgic_irq_enable(vcpu, i);
        }
      }
      return 0;
    case GICR_ICENABLER0:
    case GICR_ICPENDR0:
      goto unimplemented;
    case GICR_ISACTIVER0:
    case GICR_ICACTIVER0:
      goto unimplemented;
    case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7):
      intid = (offset - GICR_IPRIORITYR(0)) / sizeof(u32) * 4;
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        irq->priority = (val >> (i * 8)) & 0xff;
      }
      return 0;
    case GICR_ICFGR0:
    case GICR_ICFGR1:
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
  vmm_warn("vgicr_mmio_write: readonly %p\n", offset);
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
  }

  return -1;
}

static int vgits_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  return -1;
}

static void load_new_vgic(void) {
  struct vgic *vgic = &vgic_dist;

  vgic->spi_max = gic_max_spi();
  vgic->nspis = vgic->spi_max - 31;
  vgic->ctlr = 0;
  vgic->spis = (struct vgic_irq *)alloc_page();
  if(!vgic->spis)
    panic("nomem");

  vmm_log("nspis %d sizeof nspi %d\n", vgic->nspis, sizeof(struct vgic_irq) * vgic->nspis);

  pagetrap(&localnode, GICDBASE, 0x10000, vgicd_mmio_read, vgicd_mmio_write);
  pagetrap(&localnode, GICRBASE, 0xf60000, vgicr_mmio_read, vgicr_mmio_write);
  pagetrap(&localnode, GITSBASE, 0x20000, vgits_mmio_read, vgits_mmio_write);

  localnode.vgic = vgic;
}

void vgic_cpu_init(struct vcpu *vcpu) {
  struct vgic_cpu *vgic = &vcpu->vgic;

  vgic->used_lr = 0;

  for(struct vgic_irq *v = vgic->sgis; v < &vgic->sgis[GIC_NSGI]; v++) {
    v->enabled = 1;
    v->target = vcpu;
  }

  for(struct vgic_irq *v = vgic->ppis; v < &vgic->ppis[GIC_NPPI]; v++) {
    v->enabled = 0;
    v->target = vcpu;
  }
}

void vgic_init() {
  load_new_vgic();
}
