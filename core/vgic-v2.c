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

  spin_lock(&vgic->lock);

  if(vgic->enabled)
    val |= GICD_CTLR_EnableGrp1;

  mmio->val = val;

  spin_unlock(&vgic->lock);
}

static void vgicd_ctlr_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;

  spin_lock(&vgic->lock);

  if(mmio->val & GICD_CTLR_EnableGrp1)
    vgic->enabled = true;
  else
    vgic->enabled = false;

  spin_unlock(&vgic->lock);
}

static void vgicd_typer_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct vgic *vgic = localvm.vgic;
  u32 val;

  spin_lock(&vgic->lock);

  /* ITLinesNumber */
  val = ((vgic->nspis + 32) >> 5) - 1;
  /* CPUNumber */
  val |= (8 - 1) << GICD_TYPER_CPUNum_SHIFT;
  /* SecurityExtn, LSPI disabled */

  mmio->val = val;

  spin_unlock(&vgic->lock);
}

static void vgicd_itargetsr_read(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  int intid = offset / sizeof(u32) * 4;
  u8 targets;
  u32 itar = 0;

  for(int i = 0; i < 4; i++) {
    irq = vgic_get_irq(vcpu, intid + i);

    spin_lock(&irq->lock);
    targets = irq->targets;
    itar |= (u32)targets << (i * 8);
    spin_unlock(&irq->lock);
  }

  mmio->val = itar;
}

static void vgicd_itargetsr_write(struct vcpu *vcpu, struct mmio_access *mmio, u64 offset) {
  struct vgic_irq *irq;
  u32 val = mmio->val;
  int intid = offset / sizeof(u32) * 4;

  for(int i = 0; i < 4; i++) {
    irq = vgic_get_irq(vcpu, intid + i);
    
    spin_lock(&irq->lock);
    irq->targets = (u8)(val >> (i * 8));
    spin_unlock(&irq->lock);
  }
}

static void vgicd_icpidr2_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  /* GICv2 */
  mmio->val = 0x2 << GICD_ICPIDR2_ArchRev_SHIFT;
}

static int vgicv2_d_mmio_read(struct vcpu *vcpu, struct mmio_access *mmio) {
  ;
}

static int vgicv2_d_mmio_write(struct vcpu *vcpu, struct mmio_access *mmio) {
  ;
}

void vgic_v2_init(struct vgic *vgic) {
  vgic->archrev = 2;

  vmmio_reg_handler(0x08000000, 0x10000, vgicv2_d_mmio_read, vgicv2_d_mmio_write);
}
