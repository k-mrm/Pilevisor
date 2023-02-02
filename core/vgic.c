/*
 *  virtual GIC (generic interrupt controller)
 */

#include "aarch64.h"
#include "gic.h"
#include "gicv3.h"
#include "vgic.h"
#include "log.h"
#include "param.h"
#include "vcpu.h"
#include "pcpu.h"
#include "vmmio.h"
#include "allocpage.h"
#include "malloc.h"
#include "lib.h"
#include "localnode.h"
#include "node.h"
#include "msg.h"
#include "irq.h"
#include "panic.h"
#include "assert.h"

static struct vgic vgic_dist;

struct sgi_msg_hdr {
  POCV2_MSG_HDR_STRUCT;
  int target;
  int sgi_id;
};

void vgic_enable_irq(struct vcpu *vcpu, struct vgic_irq *irq) {
  if(irq->enabled)
    return;

  irq->enabled = 1;
  u32 intid = irq->intid;

  vmm_warn("vcpu %d enable irq %d\n", vcpu->vcpuid, intid);

  if(valid_intid(intid))
    localnode.irqchip->enable_irq(intid);
  else
    panic("?");
}

void vgic_disable_irq(struct vcpu *vcpu, struct vgic_irq *irq) {
  if(!irq->enabled)
    return;

  irq->enabled = 0;
  u32 intid = irq->intid;

  vmm_warn("vcpu %d disable irq %d\n", vcpu->vcpuid, intid);

  if(valid_intid(intid))
    localnode.irqchip->disable_irq(intid);
  else
    panic("?");
}

void vgic_inject_pending_irqs() {
  u64 flags;
  struct vcpu *vcpu = current;

  spin_lock_irqsave(&vcpu->pending.lock, flags);

  int head = vcpu->pending.head;

  while(head != vcpu->pending.tail) {
    struct gic_pending_irq *pendirq = vcpu->pending.irqs[head];
    if(localnode.irqchip->inject_guest_irq(pendirq) < 0)
      ;   /* do nothing */

    head = (head + 1) % 4;

    free(pendirq);
  }

  vcpu->pending.head = head;

  spin_unlock_irqrestore(&vcpu->pending.lock, flags);

  dsb(ish);
}

bool vgic_irq_pending(struct vgic_irq *irq) {
  u32 intid = irq->intid;

  return localnode.irqchip->guest_irq_pending(intid);
}

void vgic_readonly(struct vcpu * __unused vcpu, struct mmio_access * __unused mmio) {
  /* do nothing */
  return;
}

static int vgic_inject_virq_local(struct vcpu *target, struct gic_pending_irq *pendirq) {
  if(target == current) {
    if(localnode.irqchip->inject_guest_irq(pendirq) < 0)
      ;   /* do nothing */

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

    cpu_send_inject_sgi(vcpu_localid(target));
  }

  return 0;
}

static int vgic_inject_virq_remote(struct vgic_irq *irq, struct gic_pending_irq *pendirq) {
  u64 vcpuid = irq->vcpuid;
  int nodeid = vcpuid_to_nodeid(vcpuid);
  if(nodeid < 0)
    return -1;

  // unimpl
  panic("inject remote Node %d", nodeid);

  return 0;
}

int vgic_inject_virq(struct vcpu *target, u32 virqno) {
  int rc;
  struct vgic_irq *irq = vgic_get_irq(target, virqno);
  if(!irq || !irq->enabled)
    return -1;

  struct gic_pending_irq *pendirq = malloc(sizeof(*pendirq));

  pendirq->virq = virqno;
  pendirq->group = 1;   /* irq->igroup */
  pendirq->priority = irq->priority;

  if(is_sgi(virqno)) {
    pendirq->pirq = NULL;
  } else if(is_ppi(virqno)) {
    /* virq == pirq */
    pendirq->pirq = irq_get(virqno);
  } else if(is_spi(virqno)) {
    pendirq->pirq = irq_get(virqno);
    target = irq->target;
  } else {
    vmm_warn("virq%d not exist\n", virqno);
    free(pendirq);
    return -1;
  }

  if(target)
    rc = vgic_inject_virq_local(target, pendirq);
  else
    rc = vgic_inject_virq_remote(irq, pendirq);

  if(rc < 0)
    free(pendirq);

  return rc;
}

struct vgic_irq *vgic_get_irq(struct vcpu *vcpu, int intid) {
  if(is_sgi(intid))
    return &vcpu->vgic.sgis[intid];
  else if(is_ppi(intid))
    return &vcpu->vgic.ppis[intid - 16];
  else if(is_spi(intid))
    return &localvm.vgic->spis[intid - 32];

  assert(0);

  return NULL;
}

static void recv_sgi_msg_intr(struct msg *msg) {
  struct sgi_msg_hdr *h = (struct sgi_msg_hdr *)msg->hdr;
  struct vcpu *target = node_vcpu(h->target);
  int virq = h->sgi_id;

  if(!target)
    panic("oi");

  if(!is_sgi(virq))
    panic("invalid sgi");

  vmm_log("SGI: recv sgi(id=%d) request to vcpu%d\n", virq, target->vcpuid);

  if(vgic_inject_virq(target, virq) < 0)
    panic("sgi failed");
}

static inline void virq_set_target(struct vgic_irq *virq, u64 vcpuid) {
  virq->vcpuid = vcpuid;
  virq->target = node_vcpu(vcpuid);
}

void vgicd_iidr_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;

  spin_lock(&vgic->lock);

  mmio->val = 0x19 << GICD_IIDR_ProductID_SHIFT |   /* pocv2 product id */
              vgic->archrev << GICD_IIDR_Revision_SHIFT |
              0x43b;   /* ARM implementer */

  spin_unlock(&vgic->lock);
}

static int vgicd_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  int intid, status = 0;
  struct vgic_irq *irq;
  struct vgic *vgic = localvm.vgic;
  u64 offset = mmio->offset;

  if(!(mmio->accsize & ACC_WORD))
    panic("unimplemented %d", mmio->accsize*8);

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
      /* linux's gicv3 driver accesses GICD_TYPER2 (offset 0xc) */
      mmio->val = 0;
      goto end;

    case GICD_IGROUPR(0) ... GICD_IGROUPR(31)+3: {
      u32 igrp = 0;

      intid = (offset - GICD_IGROUPR(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if(!irq)
          goto end;

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
        if(!irq)
          goto end;

        iser |= (u32)irq->enabled << i;
      }

      mmio->val = iser;
      goto end;
    }

    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3: {
      u32 iser = 0;

      intid = (offset - GICD_ICENABLER(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if(!irq)
          goto end;

        iser |= (u32)irq->enabled << i;
      }

      mmio->val = iser;
      goto end;
    }

    case GICD_ISPENDR(0) ... GICD_ISPENDR(31)+3: {
      u32 pendr = 0;

      intid = (offset - GICD_ISPENDR(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          goto end;

        pendr |= (u32)vgic_irq_pending(irq) << i;
      }

      printf("vgic ispendr read %p %p\n", offset, pendr);
      mmio->val = pendr;
      goto end;
    }

    case GICD_ICPENDR(0) ... GICD_ICPENDR(31)+3: {
      u32 pendr = 0;

      intid = (offset - GICD_ICPENDR(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          goto end;
        
        pendr |= (u32)vgic_irq_pending(irq) << i;
      }

      mmio->val = pendr;
      goto end;
    }

    case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31)+3:
    case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31)+3:
      mmio->val = 0;
      goto end;

    case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254)+3: {
      u32 ipr = 0;

      intid = offset - GICD_IPRIORITYR(0);
      for(int i = 0; i < 4; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          goto end;

        ipr |= (u32)irq->priority << (i * 8);
      }

      mmio->val = ipr;
      goto end;
    }

    case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254)+3: {
      goto end;
    }

    case GICD_ICFGR(0) ... GICD_ICFGR(63)+3: {
      u32 icfg = 0;
      intid = (offset - GICD_ICFGR(0)) / sizeof(u32) * 16;
      for(int i = 0; i < 16; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          goto end;

        if(irq->cfg == CONFIG_EDGE)
          icfg |= 0x2u << (i * 2);
      }

      mmio->val = icfg;
      goto end;
    }

    case GICD_IROUTER(0) ... GICD_IROUTER(31)+3:
      goto reserved;

    case GICD_IROUTER(32) ... GICD_IROUTER(1019)+3:

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
      if(mmio->val & GICD_CTLR_SS_ENGRP1)
        vgic->enabled = true;
      else
        vgic->enabled = false;

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
        if(!irq)
          goto end;

        if((val >> i) & 0x1) {
          vgic_enable_irq(vcpu, irq);
        }
      }
      goto end;

    case GICD_ICENABLER(0) ... GICD_ICENABLER(31)+3:
      intid = (offset - GICD_ICENABLER(0)) / sizeof(u32) * 32;
      for(int i = 0; i < 32; i++) {
        irq = vgic_get_irq(vcpu, intid+i);
        if(!irq)
          goto end;

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
        if(!irq)
          goto end;

        irq->priority = (val >> (i * 8)) & 0xff;
      }
      goto end;

    case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254)+3:

    case GICD_ICFGR(0) ... GICD_ICFGR(63)+3: {
      intid = (offset - GICD_ICFGR(0)) / sizeof(u32) * 16;
      for(int i = 0; i < 16; i++) {
        irq = vgic_get_irq(vcpu, intid + i);
        if(!irq)
          goto end;

        u8 c = (val >> (i * 2)) & 0x3;

        if(c >> 1 == CONFIG_LEVEL)
          irq->cfg = CONFIG_LEVEL;
        else
          irq->cfg = CONFIG_EDGE;
      }
      goto end;
    }

    case GICD_IROUTER(0) ... GICD_IROUTER(31)+7:
      goto reserved;

    case GICD_IROUTER(32) ... GICD_IROUTER(1019)+7:

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
  vmm_warn("vgicd_mmio_write: unimplemented %p %p %p %p\n",
           offset, val, current->reg.elr, current->reg.x[30]);
  goto end;

reserved:
  vmm_warn("vgicd_mmio_write: reserved %p\n", offset);

end:
  spin_unlock(&vgic->lock);
  return status;
}

void vgic_init(void) {
  struct vgic *vgic = &vgic_dist;

  vgic->nspis = localnode.irqchip->nirqs - 32;
  vgic->enabled = false;
  vgic->spis = (struct vgic_irq *)alloc_pages(1);   // allocate 8192 byte
  if(!vgic->spis)
    panic("nomem");

  printf("nspis %d sizeof nspi %d\n", vgic->nspis, sizeof(struct vgic_irq) * vgic->nspis);

  for(int i = 0; i < vgic->nspis; i++) {
    vgic->spis[i].intid = 32 + i;
  }

  spinlock_init(&vgic->lock);

  vgic_v2_init(vgic);

  localvm.vgic = vgic;
}

void vgic_cpu_init(struct vcpu *vcpu) {
  struct vgic_irq *irq;
  struct vgic_cpu *vgic = &vcpu->vgic;

  for(int i = 0; i < GIC_NSGI; i++) {
    irq = &vgic->sgis[i];
    irq->intid = i;
    irq->enabled = true;
    irq->cfg = CONFIG_EDGE;
    spinlock_init(&irq->lock);
  }

  for(int i = 0; i < GIC_NPPI; i++) {
    irq = &vgic->ppis[i];
    irq->intid = i + 16;
    irq->enabled = false;
    irq->cfg = CONFIG_LEVEL;
    virq_set_target(irq, vcpu->vcpuid);
    spinlock_init(&irq->lock);
  }
}

DEFINE_POCV2_MSG(MSG_SGI, struct sgi_msg_hdr, recv_sgi_msg_intr);
