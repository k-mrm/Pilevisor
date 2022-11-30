/*
 *  virtual GICv3 (generic interrupt controller)
 */

#include "aarch64.h"
#include "gic.h"
#include "gicv3.h"
#include "vgic.h"
#include "log.h"
#include "param.h"
#include "vcpu.h"
#include "mmio.h"
#include "vmmio.h"
#include "allocpage.h"
#include "malloc.h"
#include "lib.h"
#include "localnode.h"
#include "node.h"
#include "msg.h"
#include "irq.h"
#include "panic.h"

static struct vgic vgic_dist;

static struct vgic_irq *vgic_get_irq(struct vcpu *vcpu, int intid);

struct sgi_msg_hdr {
  POCV2_MSG_HDR_STRUCT;
  int target;
  int sgi_id;
};

static void vgic_enable_irq(struct vcpu *vcpu, struct vgic_irq *irq) {
  if(irq->enabled)
    return;

  irq->enabled = 1;
  u32 intid = irq->intid;

  if(valid_intid(intid))
    localnode.irqchip->enable_irq(intid);
  else
    panic("?");
}

static void vgic_disable_irq(struct vcpu *vcpu, struct vgic_irq *irq) {
  if(!irq->enabled)
    return;

  irq->enabled = 0;
  u32 intid = irq->intid;

  if(valid_intid(intid))
    localnode.irqchip->disable_irq(intid);
  else
    panic("?");
}

static void vgic_set_target(struct vcpu *vcpu, int vintid, u8 target) {
  if(is_sgi_ppi(vintid))
    return; /* ignored */
  else if(is_spi(vintid))
    localnode.irqchip->set_target(vintid, target);
  else
    panic("?");
}

void vgic_inject_pending_irqs() {
  struct vcpu *vcpu = current;
  u64 flags;

  spin_lock_irqsave(&vcpu->pending.lock, flags);

  int head = vcpu->pending.head;

  while(head != vcpu->pending.tail) {
    struct gic_pending_irq *pendirq = vcpu->pending.irqs[head];
    if(localnode.irqchip->inject_guest_irq(pendirq) < 0)
      panic("inject pending irqs");

    head = (head + 1) % 4;

    free(pendirq);
  }

  vcpu->pending.head = head;

  spin_unlock_irqrestore(&vcpu->pending.lock, flags);

  dsb(ish);
}

int vgic_inject_virq(struct vcpu *target, u32 virqno) {
  struct vgic_irq *irq = vgic_get_irq(target, virqno);
  printf("111VGIC inject: %d ", virqno);

  if(!irq->enabled)
    return -1;

  printf("222VGIC inject: %d\n", virqno);
  struct gic_pending_irq *pendirq = malloc(sizeof(*pendirq));

  pendirq->virq = virqno;
  pendirq->group = 1;
  pendirq->priority = irq->priority;

  if(is_sgi(virqno)) {
    pendirq->pirq = NULL;
    pendirq->req_cpu = 0;   // TODO
  } else {
    /* virq == pirq */
    pendirq->pirq = irq_get(virqno);
  }

  if(target == current) {
    if(localnode.irqchip->inject_guest_irq(pendirq) < 0)
      ;   /* TODO: do nothing? */
    free(pendirq);
  } else {
    u64 flags = 0;

    spin_lock_irqsave(&target->pending.lock, flags);

    int tail = (target->pending.tail + 1) % 4;

    if(tail != target->pending.head) {
      target->pending.irqs[target->pending.tail] = pendirq;

      target->pending.tail = tail;
    } else {
      panic("pending queue full");
    }

    spin_unlock_irqrestore(&target->pending.lock, flags);

    dsb(ish);

    struct gic_sgi sgi = {
      .sgi_id = SGI_INJECT,
      .targets = 1 << vcpu_localid(target),
      .mode = ROUTE_TARGETS,
    };

    localnode.irqchip->send_sgi(&sgi);
  }

  return 0;
}

static struct vgic_irq *vgic_get_irq(struct vcpu *vcpu, int intid) {
  if(is_sgi(intid))
    return &vcpu->vgic.sgis[intid];
  else if(is_ppi(intid))
    return &vcpu->vgic.ppis[intid - 16];
  else if(is_spi(intid))
    return &localvm.vgic->spis[intid - 32];

  panic("vgic_get_irq unknown %d", intid);
  return NULL;
}

static int vgic_emulate_sgir(struct vcpu *vcpu, u64 sgir) {
  u16 targets = ICC_SGI1R_TARGETS(sgir);
  u8 intid = ICC_SGI1R_INTID(sgir);
  int irm = ICC_SGI1R_IRM(sgir);

  if(irm == 1)
    panic("broadcast");

  struct cluster_node *node;
  foreach_cluster_node(node) {
    for(int i = 0; i < node->nvcpu; i++) {
      int vcpuid = node->vcpus[i];

      /* TODO: consider Affinity */
      if((1 << vcpuid) & targets) {
        // vmm_log("vgic: sgi to vcpu%d\n", vcpuid);
        struct vcpu *vcpu = node_vcpu(vcpuid);

        if(vcpu) {
          vmm_log("vgic injection!!!!!!!!!!!!!!!!!%d\n", intid);
          /* vcpu in localnode */
          if(vgic_inject_virq(vcpu, intid) < 0)
            panic("sgi failed");
        } else {
          vmm_log("vgic: route sgi(%d) to remote vcpu%d@%d (%p)\n", intid, vcpuid, node->nodeid, current->reg.elr);
          struct pocv2_msg msg;
          struct sgi_msg_hdr hdr;
          hdr.target = vcpuid;
          hdr.sgi_id = intid;

          pocv2_msg_init2(&msg, node->nodeid, MSG_SGI, &hdr, NULL, 0);

          send_msg(&msg);
        }
      }
    }
  }

  return 0;
}

static void recv_sgi_msg_intr(struct pocv2_msg *msg) {
  struct sgi_msg_hdr *h = (struct sgi_msg_hdr *)msg->hdr;
  struct vcpu *target = node_vcpu(h->target);
  if(!target)
    panic("oi");

  int virq = h->sgi_id;

  if(!is_sgi(virq))
    panic("invalid sgi");

  vmm_log("SGI: recv sgi(id=%d) request to vcpu%d\n", virq, target->vcpuid);

  if(vgic_inject_virq(target, virq) < 0)
    panic("sgi failed");
}

int vgic_emulate_sgi1r(struct vcpu *vcpu, int rt, int wr) {
  /* write only register */
  if(!wr)
    return -1;

  u64 sgir = rt == 31 ? 0 : vcpu->reg.x[rt];

  return vgic_emulate_sgir(vcpu, sgir);
}

static int vgicd_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid, status = 0;
  struct vgic_irq *irq;
  struct vgic *vgic = localvm.vgic;
  u64 offset = mmio->offset;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d", __func__, mmio->accsize*8);
    */

  spin_lock(&vgic->lock);

  switch(offset) {
    case GICD_CTLR: {
      u32 val = GICD_CTLR_SS_ARE | GICD_CTLR_DS;

      if(vgic->enabled)
        val |= GICD_CTLR_SS_ENGRP1;

      mmio->val = val;
      goto end;
    }
    case GICD_TYPER: {
      /* ITLinesNumber */
      u32 val = ((vgic->nspis + 32) >> 5) - 1;

      /* CPU Number */
      val |= (8 - 1) << GICD_TYPER_CPUNum_SHIFT;

      /* MBIS LPIS disabled */

      /* IDbits */
      val |= (10 - 1) << GICD_TYPER_IDbits_SHIFT;

      /* A3V disabled */

      mmio->val = val;
      goto end;
    }
    case GICD_IIDR:
      mmio->val = 0x19 << GICD_IIDR_ProductID_SHIFT |   /* pocv2 product id */
                  vgic->archrev << GICD_IIDR_Revision_SHIFT |
                  0x43b;   /* ARM implementer */
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
    case GICD_ISENABLER(0) ... GICD_ISENABLER(31)+3:
    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3: {
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
        /*
        irq = vgic_get_irq(vcpu, intid+i);
        u32 t = irq->target->vmpidr;
        itar |= (u32)t << (i * 8);
        */
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
      mmio->val = vgic->archrev << GICD_PIDR2_ArchRev_SHIFT;
      goto end;
  }

  vmm_warn("vgicd_mmio_read: unhandled %p\n", offset);
  mmio->val = 0;
  status = -1;
  goto end;

reserved:
  vmm_warn("read reserved\n");
  mmio->val = 0;
  goto end;

unimplemented:
  vmm_warn("vgicd_mmio_read: unimplemented %p\n", offset);
  mmio->val = 0;
  goto end;

end:
  spin_unlock(&vgic->lock);

  return status;
}

static int vgicd_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid, status = 0;
  struct vgic_irq *irq;
  struct vgic *vgic = localvm.vgic;
  u64 offset = mmio->offset;
  u64 val = mmio->val;

  /*
  if(!(mmio->accsize & ACC_WORD))
    panic("%s: unimplemented %d %p", __func__, mmio->accsize*8, offset);
    */

  spin_lock(&vgic->lock);

  switch(offset) {
    case GICD_CTLR:
      if(mmio->val & GICD_CTLR_SS_ENGRP1) {
        vgic->enabled = true;
      } else {
        vgic->enabled = false;
      }

      goto end;
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
          vgic_enable_irq(vcpu, irq);
        }
      }
      goto end;
    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3:
      intid = (offset - GICD_ICENABLER(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if((val >> i) & 0x1) {
          if(irq->enabled) {
            vgic_disable_irq(vcpu, irq);
          }
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
        /*  TODO: fix bug
        irq = vgic_get_irq(vcpu, intid+i);
        u32 t = (val >> (i * 8)) & 0xff;
        irq->target = node_vcpu(t);
        if(!irq->target)
          panic("target remote");
        vgic_set_target(vcpu, intid+i, t);
        */
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
  status = -1;
  goto end;

readonly:
  /* write ignored */
  vmm_warn("vgicd_mmio_write: readonly register %p\n", offset);
  goto end;

unimplemented:
  vmm_warn("vgicd_mmio_write: unimplemented %p %p %p %p\n", offset, val, current->reg.elr, current->reg.x[30]);
  goto end;

reserved:
  vmm_warn("vgicd_mmio_write: reserved %p\n", offset);

end:
  spin_unlock(&vgic->lock);
  return status;
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
      mmio->val = 0x19 << GICD_IIDR_ProductID_SHIFT |   /* pocv2 product id */
                  vgic->archrev << GICD_IIDR_Revision_SHIFT |
                  0x43b;   /* ARM implementer */
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
      mmio->val = vgic->archrev << GICD_PIDR2_ArchRev_SHIFT;
      return 0;
    case GICR_ISENABLER0:
    case GICR_ICENABLER0: {
      u32 iser = 0; 

      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, i);
        iser |= irq->enabled << i;
      }

      mmio->val = iser;
      return 0;
    }
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
          vgic_enable_irq(vcpu, irq);
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
      return -1;
  }
}

static int vgits_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  return -1;
}

static void load_new_vgic(void) {
  struct vgic *vgic = &vgic_dist;

  vgic->nspis = localnode.irqchip->nirqs - 32;
  vgic->enabled = false;
  vgic->spis = (struct vgic_irq *)alloc_page();
  if(!vgic->spis)
    panic("nomem");

  printf("nspis %d sizeof nspi %d\n", vgic->nspis, sizeof(struct vgic_irq) * vgic->nspis);

  vgic->archrev = 3;

  for(int i = 0; i < vgic->nspis; i++) {
    vgic->spis[i].intid = 32 + i;
  }

  vmmio_reg_handler(GICDBASE, 0x10000, vgicd_mmio_read, vgicd_mmio_write);
  vmmio_reg_handler(GICRBASE, 0xf60000, vgicr_mmio_read, vgicr_mmio_write);
  vmmio_reg_handler(GITSBASE, 0x20000, vgits_mmio_read, vgits_mmio_write);

  localvm.vgic = vgic;
}

void vgic_cpu_init(struct vcpu *vcpu) {
  struct vgic_irq *irq;
  struct vgic_cpu *vgic = &vcpu->vgic;

  for(int i = 0; i < GIC_NSGI; i++) {
    irq = &vgic->sgis[i];
    irq->intid = i;
    irq->enabled = true;
  }

  for(int i = 0; i < GIC_NPPI; i++) {
    irq = &vgic->ppis[i];
    irq->intid = i + 16;
    irq->enabled = false;
  }
}

void vgic_init() {
  load_new_vgic();
}

DEFINE_POCV2_MSG(MSG_SGI, struct sgi_msg_hdr, recv_sgi_msg_intr);
