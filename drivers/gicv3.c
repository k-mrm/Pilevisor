/*
 *  GICv3 (generic interrupt controller) driver
 */

#include "aarch64.h"
#include "gic.h"
#include "log.h"
#include "memmap.h"
#include "irq.h"
#include "vcpu.h"
#include "emul.h"
#include "compiler.h"
#include "localnode.h"
#include "panic.h"

/* gicv3 controller */

static u64 gicv3_read_lr(int n) {
  if(localnode.irqchip->max_lr <= n)
    panic("lr");

  switch(n) {
    case 0:   return read_sysreg(ich_lr0_el2);
    case 1:   return read_sysreg(ich_lr1_el2);
    case 2:   return read_sysreg(ich_lr2_el2);
    case 3:   return read_sysreg(ich_lr3_el2);
    case 4:   return read_sysreg(ich_lr4_el2);
    case 5:   return read_sysreg(ich_lr5_el2);
    case 6:   return read_sysreg(ich_lr6_el2);
    case 7:   return read_sysreg(ich_lr7_el2);
    case 8:   return read_sysreg(ich_lr8_el2);
    case 9:   return read_sysreg(ich_lr9_el2);
    case 10:  return read_sysreg(ich_lr10_el2);
    case 11:  return read_sysreg(ich_lr11_el2);
    case 12:  return read_sysreg(ich_lr12_el2);
    case 13:  return read_sysreg(ich_lr13_el2);
    case 14:  return read_sysreg(ich_lr14_el2);
    case 15:  return read_sysreg(ich_lr15_el2);
    default:  panic("?");
  }
}

static void gicv3_write_lr(int n, u64 val) {
  if(localnode.irqchip->max_lr <= n)
    panic("lr");

  switch(n) {
    case 0:   write_sysreg(ich_lr0_el2, val); break;
    case 1:   write_sysreg(ich_lr1_el2, val); break;
    case 2:   write_sysreg(ich_lr2_el2, val); break;
    case 3:   write_sysreg(ich_lr3_el2, val); break;
    case 4:   write_sysreg(ich_lr4_el2, val); break;
    case 5:   write_sysreg(ich_lr5_el2, val); break;
    case 6:   write_sysreg(ich_lr6_el2, val); break;
    case 7:   write_sysreg(ich_lr7_el2, val); break;
    case 8:   write_sysreg(ich_lr8_el2, val); break;
    case 9:   write_sysreg(ich_lr9_el2, val); break;
    case 10:  write_sysreg(ich_lr10_el2, val); break;
    case 11:  write_sysreg(ich_lr11_el2, val); break;
    case 12:  write_sysreg(ich_lr12_el2, val); break;
    case 13:  write_sysreg(ich_lr13_el2, val); break;
    case 14:  write_sysreg(ich_lr14_el2, val); break;
    case 15:  write_sysreg(ich_lr15_el2, val); break;
    default:  panic("?");
  }
}

/*
static void gicv3_restore_lr(struct gic_state *gic) {
  switch(gic_lr_max-1) {
    case 15:  write_sysreg(ich_lr15_el2, gic->lr[15]); __fallthrough;
    case 14:  write_sysreg(ich_lr14_el2, gic->lr[14]); __fallthrough;
    case 13:  write_sysreg(ich_lr13_el2, gic->lr[13]); __fallthrough;
    case 12:  write_sysreg(ich_lr12_el2, gic->lr[12]); __fallthrough;
    case 11:  write_sysreg(ich_lr11_el2, gic->lr[11]); __fallthrough;
    case 10:  write_sysreg(ich_lr10_el2, gic->lr[10]); __fallthrough;
    case 9:   write_sysreg(ich_lr9_el2, gic->lr[9]); __fallthrough;
    case 8:   write_sysreg(ich_lr8_el2, gic->lr[8]); __fallthrough;
    case 7:   write_sysreg(ich_lr7_el2, gic->lr[7]); __fallthrough;
    case 6:   write_sysreg(ich_lr6_el2, gic->lr[6]); __fallthrough;
    case 5:   write_sysreg(ich_lr5_el2, gic->lr[5]); __fallthrough;
    case 4:   write_sysreg(ich_lr4_el2, gic->lr[4]); __fallthrough;
    case 3:   write_sysreg(ich_lr3_el2, gic->lr[3]); __fallthrough;
    case 2:   write_sysreg(ich_lr2_el2, gic->lr[2]); __fallthrough;
    case 1:   write_sysreg(ich_lr1_el2, gic->lr[1]); __fallthrough;
    case 0:   write_sysreg(ich_lr0_el2, gic->lr[0]);
  }
}

static void gicv3_save_lr(struct gic_state *gic) {
  switch(gic_lr_max-1) {
    case 15:  gic->lr[15] = read_sysreg(ich_lr15_el2); __fallthrough;
    case 14:  gic->lr[14] = read_sysreg(ich_lr14_el2); __fallthrough;
    case 13:  gic->lr[13] = read_sysreg(ich_lr13_el2); __fallthrough;
    case 12:  gic->lr[12] = read_sysreg(ich_lr12_el2); __fallthrough;
    case 11:  gic->lr[11] = read_sysreg(ich_lr11_el2); __fallthrough;
    case 10:  gic->lr[10] = read_sysreg(ich_lr10_el2); __fallthrough;
    case 9:   gic->lr[9] = read_sysreg(ich_lr9_el2); __fallthrough;
    case 8:   gic->lr[8] = read_sysreg(ich_lr8_el2); __fallthrough;
    case 7:   gic->lr[7] = read_sysreg(ich_lr7_el2); __fallthrough;
    case 6:   gic->lr[6] = read_sysreg(ich_lr6_el2); __fallthrough;
    case 5:   gic->lr[5] = read_sysreg(ich_lr5_el2); __fallthrough;
    case 4:   gic->lr[4] = read_sysreg(ich_lr4_el2); __fallthrough;
    case 3:   gic->lr[3] = read_sysreg(ich_lr3_el2); __fallthrough;
    case 2:   gic->lr[2] = read_sysreg(ich_lr2_el2); __fallthrough;
    case 1:   gic->lr[1] = read_sysreg(ich_lr1_el2); __fallthrough;
    case 0:   gic->lr[0] = read_sysreg(ich_lr0_el2);
  }
}
*/

static u64 gicv3_pending_lr(u32 pirq, u32 virq, int grp) {
  u64 lr = ICH_LR_VINTID(virq) | ICH_LR_STATE(LR_PENDING) | ICH_LR_GROUP(grp);
  
  if(!is_sgi(pirq)) {
    /* this is hw irq */
    lr |= ICH_LR_HW;
    lr |= ICH_LR_PINTID(pirq);
  }

  return lr;
}

static int gicv3_inject_guest_irq(u32 pirq, u32 virq, int grp) {
  if(is_sgi(pirq)) {
    if(pirq == 2)
      panic("!? maybe kernel panicked");
  }

  u64 elsr = read_sysreg(ich_elsr_el2);
  int freelr = -1;
  u64 lr;

  for(int i = 0; i < localnode.irqchip->max_lr; i++) {
    if((elsr >> i) & 0x1) {   // free area in lr
      if(freelr < 0)
        freelr = i;

      continue;
    }

    if((u32)gicv3_read_lr(i) == pirq)
      return -1;    // busy
  }

  if(freelr < 0)
    return -1;

  lr = gicv3_pending_lr(pirq, virq, grp);

  gicv3_write_lr(freelr, lr);

  return 0;
}

static bool gicv3_irq_pending(u32 irq) {
  u32 is = gicd_r(GICD_ISPENDR(irq / 32));
  return (is & (1 << (irq % 32))) != 0;
}

static u32 gicv3_read_iar() {
  return read_sysreg(icc_iar1_el1);
}

static void gicv3_eoi(u32 iar, int grp) {
  if(grp == 0)
    write_sysreg(icc_eoir0_el1, iar);
  else if(grp == 1)
    write_sysreg(icc_eoir1_el1, iar);
  else
    panic("?");
}

static void gicv3_deactive_irq(u32 irq) {
  write_sysreg(icc_dir_el1, irq);
}

static void gicv3_host_eoi(u32 iar, int grp) {
  gicv3_eoi(iar, grp);
  gicv3_deactive_irq(iar);
}

static void gicv3_guest_eoi(u32 iar, int grp) {
  gicv3_eoi(iar, grp);
}

static void gicv3_send_sgi(int cpuid, int sgi_id) {
  u64 targets = 1 << cpuid;
  u64 sgir = (sgi_id & 0xf) << ICC_SGI1R_INTID_SHIFT;
  sgir |= targets & ICC_SGI1R_TARGETS_MASK;

  dsb(ish);

  write_sysreg(icc_sgi1r_el1, sgir);

  isb();
}

static bool gicv3_irq_enabled(u32 irq) {
  u32 is;

  if(is_sgi_ppi(irq)) {
    int id = cpuid();

    is = gicr_r32(id, GICR_ISENABLER0);
    return !!(is & (1 << irq));
  } else if(is_spi(irq)) {
    is = gicd_r(GICD_ISENABLER(irq / 32));
    return !!(is & (1 << (irq % 32)));
  }
}

static void gicv3_enable_irq(u32 irq) {
  u32 is;

  if(is_sgi_ppi(irq)) {
    int id = cpuid();

    is = gicr_r32(id, GICR_ISENABLER0);
    is |= 1 << irq;
    gicr_w32(id, GICR_ISENABLER0, is);
  } else if(is_spi(irq)) {
    is = gicd_r(GICD_ISENABLER(irq / 32));
    is |= 1 << (irq % 32);
    gicd_w(GICD_ISENABLER(irq / 32), is);
  }
}

static void gicv3_disable_irq(u32 irq) {
  u32 is;

  if(is_sgi_ppi(irq)) {
    int id = cpuid();

    is = 1 << irq;
    gicr_w32(cpuid, GICR_ICENABLER0, is);
  } else if(is_spi(irq)) {
    is = 1 << (irq % 32);
    gicd_w(GICD_ICENABLER(irq / 32), is);
  }
}

static void gic_set_igroup(u32 irq, u32 igrp) {
  panic("set igroup");
}

static void gicv3_set_target(u32 irq, u8 target) {
  vmm_log("settttttttttarget %d %d\n", irq, target);

  if(is_sgi_ppi(irq))
    panic("sgi_ppi set target?");

  u32 itargetsr = gicd_r(GICD_ITARGETSR(irq / 4));
  itargetsr &= ~((u32)0xff << (irq % 4 * 8));
  gicd_w(GICD_ITARGETSR(irq / 4), itargetsr | (target << (irq % 4 * 8)));
}

/*
void gic_init_state(struct gic_state *gic) {
  gic->vmcr = read_sysreg(ich_vmcr_el2);
}

void gic_restore_state(struct gic_state *gic) {
  write_sysreg(ich_vmcr_el2, gic->vmcr);

  u32 sre = read_sysreg(icc_sre_el1);
  write_sysreg(icc_sre_el1, sre | gic->sre_el1);

  gic_restore_lr(gic);
}

void gic_save_state(struct gic_state *gic) {
  gic->vmcr = read_sysreg(ich_vmcr_el2);
  gic->sre_el1 = read_sysreg(icc_sre_el1);

  gic_save_lr(gic);
}
*/

static void gicv3_setup_irq(u32 irq) {
  if(is_spi(irq))
    gicv3_set_target(irq, 0);

  gicv3_enable_irq(irq);
}

static void gicv3_c_init(void) {
  write_sysreg(icc_igrpen0_el1, 0);
  write_sysreg(icc_igrpen1_el1, 0);

  write_sysreg(icc_pmr_el1, 0xff);
  write_sysreg(icc_ctlr_el1, ICC_CTLR_EOImode(1));

  write_sysreg(icc_igrpen1_el1, 1);

  isb();
}

static void gicv3_d_init(void) {
  u32 typer = gicd_r(GICD_TYPER);
  u32 lines = typer & 0x1f;
  u32 pidr2 = gicd_r(GICD_PIDR2);
  u32 archrev = GICD_PIDR2_ARCHREV(pidr2);

  vmm_log("GICv%d found\n", archrev);

  if(archrev != 0x3)
    panic("gicv3?");

  for(int i = 0; i < lines; i++)
    gicd_w(GICD_IGROUPR(i), ~0);

  gicd_w(GICD_CTLR, 3);

  isb();
}

static void gicv3_r_init(int cpuid) {
  gicr_w32(cpuid, GICR_CTLR, 0);

  u32 sre = read_sysreg(icc_sre_el2);
  write_sysreg(icc_sre_el2, sre | (1 << 3) | 1);

  isb();

  sre = read_sysreg(icc_sre_el1);
  write_sysreg(icc_sre_el1, sre | 1);

  gicr_w32(cpuid, GICR_IGROUPR0, ~0);
  gicr_w32(cpuid, GICR_IGRPMODR0, 0);

  u32 waker = gicr_r32(cpuid, GICR_WAKER);
  gicr_w32(cpuid, GICR_WAKER, waker & ~(1<<1));
  while(gicr_r32(cpuid, GICR_WAKER) & (1<<2))
    ;

  isb();
}

static void gicv3_h_init(void) {
  write_sysreg(ich_vmcr_el2, ICH_VMCR_VENG1);
  write_sysreg(ich_hcr_el2, ICH_HCR_EN);

  isb();
}

static void gicv3_init_cpu() {
  gicv3_c_init();
  gicv3_r_init(cpuid());
  gicv3_h_init();
}

static int gic_max_spi() {
  u32 typer = gicd_r(GICD_TYPER);
  u32 lines = typer & 0x1f;
  u32 max_spi = 32 * (lines + 1) - 1;
  vmm_log("typer %p\n", typer);

  return max_spi < 1020 ? max_spi : 1019;
}

static int gicv3_max_listregs() {
  u64 i = read_sysreg(ich_vtr_el2);
  return (i & 0x1f) + 1;
}

static void gicv3_init(void);

static struct gic_irqchip gicv3_irqchip = {
  .version = 3,

  .init = gicv3_init,
  .initcore = gicv3_init_cpu,

  .read_lr = gicv3_read_lr,
  .write_lr = gicv3_write_lr,
  .inject_guest_irq = gicv3_inject_guest_irq,
  .read_iar = gicv3_read_iar,
  .host_eoi = gicv3_host_eoi,
  .guest_eoi = gicv3_guest_eoi,
  .deactive_irq = gicv3_deactive_irq,
  .send_sgi = gicv3_send_sgi,
  .irq_enabled = gicv3_irq_enabled,
  .enable_irq = gicv3_enable_irq,
  .disable_irq = gicv3_disable_irq,
  .setup_irq = gicv3_setup_irq,
  .set_target = gicv3_set_target,
};

static void gicv3_init(void) {
  gicv3_d_init();

  gicv3_irqchip.max_spi = gic_max_spi();
  gicv3_irqchip.max_lr = gicv3_max_listregs();

  printf("max_spi: %d max_lr: %d\n", gicv3_irqchip.max_spi, gicv3_irqchip.max_lr);
}

void gicv3_sysinit() {
  localnode.irqchip = &gicv3_irqchip;
}
