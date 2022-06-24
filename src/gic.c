#include "aarch64.h"
#include "gic.h"
#include "log.h"
#include "memmap.h"

#define __fallthrough __attribute__((fallthrough))

/* gicv3 controller */

int gic_lr_max = 0;

u64 gic_read_lr(int n) {
  if(gic_lr_max <= n)
    panic("lr");

  u64 val;
  switch(n) {
    case 0:   read_sysreg(val, ich_lr0_el2); break;
    case 1:   read_sysreg(val, ich_lr1_el2); break;
    case 2:   read_sysreg(val, ich_lr2_el2); break;
    case 3:   read_sysreg(val, ich_lr3_el2); break;
    case 4:   read_sysreg(val, ich_lr4_el2); break;
    case 5:   read_sysreg(val, ich_lr5_el2); break;
    case 6:   read_sysreg(val, ich_lr6_el2); break;
    case 7:   read_sysreg(val, ich_lr7_el2); break;
    case 8:   read_sysreg(val, ich_lr8_el2); break;
    case 9:   read_sysreg(val, ich_lr9_el2); break;
    case 10:  read_sysreg(val, ich_lr10_el2); break;
    case 11:  read_sysreg(val, ich_lr11_el2); break;
    case 12:  read_sysreg(val, ich_lr12_el2); break;
    case 13:  read_sysreg(val, ich_lr13_el2); break;
    case 14:  read_sysreg(val, ich_lr14_el2); break;
    case 15:  read_sysreg(val, ich_lr15_el2); break;
    default:  panic("?");
  }

  return val;
}

void gic_write_lr(int n, u64 val) {
  if(gic_lr_max <= n)
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

static void gic_restore_lr(struct gic_state *gic) {
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

static void gic_save_lr(struct gic_state *gic) {
  switch(gic_lr_max-1) {
    case 15:  read_sysreg(gic->lr[15], ich_lr15_el2); __fallthrough;
    case 14:  read_sysreg(gic->lr[14], ich_lr14_el2); __fallthrough;
    case 13:  read_sysreg(gic->lr[13], ich_lr13_el2); __fallthrough;
    case 12:  read_sysreg(gic->lr[12], ich_lr12_el2); __fallthrough;
    case 11:  read_sysreg(gic->lr[11], ich_lr11_el2); __fallthrough;
    case 10:  read_sysreg(gic->lr[10], ich_lr10_el2); __fallthrough;
    case 9:   read_sysreg(gic->lr[9], ich_lr9_el2); __fallthrough;
    case 8:   read_sysreg(gic->lr[8], ich_lr8_el2); __fallthrough;
    case 7:   read_sysreg(gic->lr[7], ich_lr7_el2); __fallthrough;
    case 6:   read_sysreg(gic->lr[6], ich_lr6_el2); __fallthrough;
    case 5:   read_sysreg(gic->lr[5], ich_lr5_el2); __fallthrough;
    case 4:   read_sysreg(gic->lr[4], ich_lr4_el2); __fallthrough;
    case 3:   read_sysreg(gic->lr[3], ich_lr3_el2); __fallthrough;
    case 2:   read_sysreg(gic->lr[2], ich_lr2_el2); __fallthrough;
    case 1:   read_sysreg(gic->lr[1], ich_lr1_el2); __fallthrough;
    case 0:   read_sysreg(gic->lr[0], ich_lr0_el2);
  }
}

u64 gic_make_lr(u32 pirq, u32 virq, int grp) {
  return ICH_LR_STATE(LR_PENDING) | ICH_LR_HW | ICH_LR_GROUP(grp) | ICH_LR_PINTID(pirq) | ICH_LR_VINTID(virq);
}

static bool gic_irq_pending(u32 irq) {
  u32 is = gicd_r(GICD_ISPENDR(irq / 32));
  return (is & (1 << (irq % 32))) != 0;
}

u32 gic_read_iar() {
  u32 i;
  read_sysreg(i, icc_iar1_el1);
  return i;
}

static void gic_eoi(u32 iar, int grp) {
  if(grp == 0)
    write_sysreg(icc_eoir0_el1, iar);
  else if(grp == 1)
    write_sysreg(icc_eoir1_el1, iar);
  else
    panic("?");
}

void gic_deactive_irq(u32 irq) {
  write_sysreg(icc_dir_el1, irq);
}

void gic_host_eoi(u32 iar, int grp) {
  gic_eoi(iar, grp);
  gic_deactive_irq(iar);
}

void gic_guest_eoi(u32 iar, int grp) {
  gic_eoi(iar, grp);
}

void gic_set_sgi1r(u64 sgi1r) {
  write_sysreg(icc_sgi1r_el1, sgi1r);
}

void gic_irq_enable_redist(u32 cpuid, u32 irq) {
  u32 is = gicr_r32(cpuid, GICR_ISENABLER0);
  is |= 1 << (irq % 32);
  gicr_w32(cpuid, GICR_ISENABLER0, is);
}

void gic_irq_disable_redist(u32 cpuid, u32 irq) {
  u32 is = gicr_r32(cpuid, GICR_ICENABLER0);
  is |= 1 << (irq % 32);
  gicr_w32(cpuid, GICR_ICENABLER0, is);
}

void gic_irq_enable(u32 irq) {
  u32 is = gicd_r(GICD_ISENABLER(irq / 32));
  is |= 1 << (irq % 32);
  gicd_w(GICD_ISENABLER(irq / 32), is);
}

void gic_irq_disable(u32 irq) {
  u32 is = gicd_r(GICD_ICENABLER(irq / 32));
  is |= 1 << (irq % 32);
  gicd_w(GICD_ICENABLER(irq / 32), is);
}

bool gic_irq_enabled(u32 irq) {
  u32 is = gicd_r(GICD_ISENABLER(irq / 32));
  return (is & (1 << (irq % 32))) != 0;
}

void gic_set_igroup(u32 irq, u32 igrp) {
  ;
}

void gic_set_target(u32 irq, u8 target) {
  vmm_log("settttttttttarget %d %d\n", irq, target);

  u32 itargetsr = gicd_r(GICD_ITARGETSR(irq / 4));
  itargetsr &= ~((u32)0xff << (irq % 4 * 8));
  gicd_w(GICD_ITARGETSR(irq / 4), itargetsr | (target << (irq % 4 * 8)));
}

void gic_init_state(struct gic_state *gic) {
  read_sysreg(gic->vmcr, ich_vmcr_el2);
}

void gic_restore_state(struct gic_state *gic) {
  write_sysreg(ich_vmcr_el2, gic->vmcr);

  u32 sre;
  read_sysreg(sre, icc_sre_el1);
  write_sysreg(icc_sre_el1, sre | gic->sre_el1);

  gic_restore_lr(gic);
}

void gic_save_state(struct gic_state *gic) {
  read_sysreg(gic->vmcr, ich_vmcr_el2);
  read_sysreg(gic->sre_el1, icc_sre_el1);

  gic_save_lr(gic);
}

static int gic_max_listregs() {
  u64 i;
  read_sysreg(i, ich_vtr_el2);
  return (i & 0x1f) + 1;
}

int gic_max_spi() {
  u32 typer = gicd_r(GICD_TYPER);
  u32 lines = typer & 0x1f;
  u32 max_spi = 32 * (lines + 1) - 1;
  vmm_log("typer %p\n", typer);

  return max_spi < 1020? max_spi : 1019;
}

static void gic_setup_spi(u32 irq) {
  gic_set_target(irq, 0);
  gic_irq_enable(irq);
}

static void hyp_intr_setup() {
  // gic_setup_spi(33);
}

static void gicc_init(void) {
  write_sysreg(icc_igrpen0_el1, 0);
  write_sysreg(icc_igrpen1_el1, 0);

  write_sysreg(icc_pmr_el1, 0xff);
  write_sysreg(icc_ctlr_el1, ICC_CTLR_EOImode(1));

  write_sysreg(icc_igrpen1_el1, 1);

  isb();
}

static void gicd_init(void) {
  u32 typer = gicd_r(GICD_TYPER);
  u32 lines = typer & 0x1f;

  for(int i = 0; i < lines; i++)
    gicd_w(GICD_IGROUPR(i), ~0);

  gicd_w(GICD_CTLR, 3);

  isb();
}

static void gicr_init(int cpuid) {
  gicr_w32(cpuid, GICR_CTLR, 0);

  u32 sre;
  read_sysreg(sre, icc_sre_el2);
  write_sysreg(icc_sre_el2, sre | (1 << 3) | 1);

  isb();

  read_sysreg(sre, icc_sre_el1);
  write_sysreg(icc_sre_el1, sre | 1);

  gicr_w32(cpuid, GICR_IGROUPR0, ~0);
  gicr_w32(cpuid, GICR_IGRPMODR0, 0);

  u32 waker = gicr_r32(cpuid, GICR_WAKER);
  gicr_w32(cpuid, GICR_WAKER, waker & ~(1<<1));
  while(gicr_r32(cpuid, GICR_WAKER) & (1<<2))
    ;

  isb();
}

static void gich_init(void) {
  write_sysreg(ich_vmcr_el2, ICH_VMCR_VENG1);
  write_sysreg(ich_hcr_el2, ICH_HCR_EN);

  gic_lr_max = gic_max_listregs();

  isb();
}

void gic_init_cpu(int cpuid) {
  gicc_init();
  gicr_init(cpuid);
  gich_init();
}

void gic_init(void) {
  vmm_log("gic init...\n");

  gicd_init();
  hyp_intr_setup();
}
