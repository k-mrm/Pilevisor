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
#include "vgic-v2.h"
#include "vgic-v3.h"

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
      panic("pending queue full %d %d\n", target->vcpuid, tail);
    }

    spin_unlock_irqrestore(&target->pending.lock, flags);

    dsb(ish);

    cpu_send_inject_sgi(target->pcpu);
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

void vgic_connect_hwirq(int virq_no, int hwirq_no) {
  struct vgic_irq *virq = vgic_get_irq(current, virq_no);

  if(!virq)
    return;

  virq->hw = true;
  virq->hwirq = hwirq_no;
}

int vgic_inject_virq(struct vcpu *target, u32 virqno) {
  int rc;
  struct vgic_irq *irq = vgic_get_irq(target, virqno);
  if(!irq || !irq->enabled)
    return -1;

  struct gic_pending_irq *pendirq = malloc(sizeof(*pendirq));

  pendirq->virq = virqno;
  pendirq->group = irq->igroup;       /* irq->igroup */
  pendirq->priority = irq->priority;
  pendirq->req_cpu = irq->req_cpu;    /* for GICv2 SGI */

  if(is_sgi(virqno)) {
    pendirq->pirq = NULL;
  } else if(is_ppi(virqno)) {
    /* virq == pirq */
    pendirq->pirq = irq_get(virqno);
  } else if(is_spi(virqno)) {
    pendirq->pirq = irq_get(virqno);
    target = irq->target;

    if(!target)
      panic("target?");
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

void vgic_iidr_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u64 flags;

  spin_lock_irqsave(&vgic->lock, flags);

  mmio->val = 0x19 << GICD_IIDR_ProductID_SHIFT |   /* pocv2 product id */
              vgic->version << GICD_IIDR_Revision_SHIFT |
              0x43b;   /* ARM implementer */

  spin_unlock_irqrestore(&vgic->lock, flags);
}

void vgic_igroup_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 igrp = 0;
  int intid = offset / sizeof(u32) * 32;
  u64 flags;

  for(int i = 0; i < 32; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);
    igrp |= (u32)irq->igroup << i;
    spin_unlock_irqrestore(&irq->lock, flags);
  }

  mmio->val = igrp;
}

void vgic_igroup_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 32;
  u32 val = mmio->val;
  u64 flags;

  for(int i = 0; i < 32; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);

    irq->igroup = !!(val & (1u << i));

    spin_unlock_irqrestore(&irq->lock, flags);
  }
}

void vgic_ienable_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 iser = 0;
  int intid = offset / sizeof(u32) * 32;
  u64 flags;

  for(int i = 0; i < 32; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);
    iser |= (u32)irq->enabled << i;
    spin_unlock_irqrestore(&irq->lock, flags);
  }

  mmio->val = iser;
}

void vgic_isenable_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 32;
  u32 val = mmio->val;
  u64 flags;

  for(int i = 0; i < 32; i++) {
    irq = vgic_get_irq(vcpu, intid+i);
    if(!irq)
      return;

    if(val & (1 << i)) {
      spin_lock_irqsave(&irq->lock, flags);
      vgic_enable_irq(vcpu, irq);
      spin_unlock_irqrestore(&irq->lock, flags);
    }
  }
}

void vgic_icenable_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 32;
  u32 val = mmio->val;
  u64 flags;

  for(int i = 0; i < 32; i++) {
    irq = vgic_get_irq(vcpu, intid+i);
    if(!irq)
      return;

    if(val & (1 << i)) {
      spin_lock_irqsave(&irq->lock, flags);

      if(irq->enabled)
        vgic_disable_irq(vcpu, irq);

      spin_unlock_irqrestore(&irq->lock, flags);
    }
  }
}

void vgic_ipend_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 pendr = 0;
  int intid = offset / sizeof(u32) * 32;
  u64 flags;

  for(int i = 0; i < 32; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);
    pendr |= (u32)vgic_irq_pending(irq) << i;
    spin_unlock_irqrestore(&irq->lock, flags);
  }

  mmio->val = pendr;
}

void vgic_ipriority_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 ipr = 0;
  int intid = offset / sizeof(u32) * 4;
  u64 flags;

  for(int i = 0; i < 4; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);
    ipr |= (u32)irq->priority << (i * 8);
    spin_unlock_irqrestore(&irq->lock, flags);
  }

  mmio->val = ipr;
}

void vgic_ipriority_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 4;
  u32 val = mmio->val;
  u64 flags;

  for(int i = 0; i < 4; i++) {
    irq = vgic_get_irq(vcpu, intid+i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);
    irq->priority = (val >> (i * 8)) & 0xff;
    spin_unlock_irqrestore(&irq->lock, flags);
  }
}

void vgic_icfg_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 icfg = 0;
  int intid = offset / sizeof(u32) * 16;
  u64 flags;

  for(int i = 0; i < 16; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    spin_lock_irqsave(&irq->lock, flags);

    if(irq->cfg == CONFIG_EDGE)
      icfg |= 0x2u << (i * 2);

    spin_unlock_irqrestore(&irq->lock, flags);
  }

  mmio->val = icfg;
}

void vgic_icfg_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 16;
  u32 val = mmio->val;
  u64 flags;

  for(int i = 0; i < 16; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    if(!irq)
      return;

    u8 c = (val >> (i * 2)) & 0x3;

    spin_lock_irqsave(&irq->lock, flags);

    if(c >> 1 == CONFIG_LEVEL)
      irq->cfg = CONFIG_LEVEL;
    else
      irq->cfg = CONFIG_EDGE;

    spin_unlock_irqrestore(&irq->lock, flags);
  }
}

int vgic_emulate_sgi(struct vcpu *vcpu, struct gic_sgi *sgi) {
  int intid = sgi->sgi_id;
  u16 targets = sgi->targets;

  struct cluster_node *node;
  foreach_cluster_node(node) {
    for(int i = 0; i < node->nvcpu; i++) {
      int vcpuid = node->vcpus[i];

      /* TODO: consider Affinity */
      if((1u << vcpuid) & targets) {
        // vmm_log("vgic: sgi to vcpu%d\n", vcpuid);
        struct vcpu *vcpu = node_vcpu(vcpuid);

        if(vcpu) {
          vmm_log("vgic injection!!!!!!!!!!!!!!!!!%d\n", intid);
          /* vcpu in localnode */
          if(vgic_inject_virq(vcpu, intid) < 0)
            panic("sgi failed");
        } else {
          struct msg msg;
          struct sgi_msg_hdr hdr;

          vmm_log("vgic: route sgi(%d) to remote vcpu%d@%d (%p)\n",
                  intid, vcpuid, node->nodeid, current->reg.elr);

          hdr.target = vcpuid;
          hdr.sgi_id = intid;

          msg_init(&msg, node->nodeid, MSG_SGI, &hdr, NULL, 0, 0);

          send_msg(&msg);
        }
      }
    }
  }

  return 0;
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
    struct vgic_irq *irq = &vgic->spis[i];
    
    irq->intid = 32 + i;
    spinlock_init(&irq->lock);
  }

  spinlock_init(&vgic->lock);

  if(localnode.irqchip->version == 2)
    vgic_v2_init(vgic);
  else if(localnode.irqchip->version == 3)
    vgic_v3_init(vgic);

  localvm.vgic = vgic;
}

void vgic_cpu_init(struct vcpu *vcpu) {
  struct vgic_irq *irq;
  struct vgic_cpu *vgic = &vcpu->vgic;
  int version = localvm.vgic->version;

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
    irq->target = vcpu;

    if(version == 2)
      irq->targets = 1 << vcpu->vcpuid;
    else if(version == 3)
      irq->vcpuid = vcpu->vcpuid;

    spinlock_init(&irq->lock);
  }
}

DEFINE_POCV2_MSG(MSG_SGI, struct sgi_msg_hdr, recv_sgi_msg_intr);
