/*
 *  GICv3 (generic interrupt controller) driver
 */

#include "aarch64.h"
#include "gic.h"
#include "gicv3.h"
#include "log.h"
#include "param.h"
#include "irq.h"
#include "vcpu.h"
#include "pcpu.h"
#include "localnode.h"
#include "mm.h"
#include "compiler.h"
#include "device.h"
#include "panic.h"

static struct gic_irqchip gicv3_irqchip;

static void *gicd_base;
static void *gicr_base;

static inline u32 gicd_r(u32 offset) {
  return *(volatile u32 *)((u64)gicd_base + offset);
}

static inline void gicd_w(u32 offset, u32 val) {
  *(volatile u32 *)((u64)gicd_base + offset) = val;
}

static inline u32 gicr_r32(int cpuid, u32 offset) {
  return *(volatile u32 *)((u64)gicr_base + cpuid * 0x20000 + offset);
}

static inline void gicr_w32(int cpuid, u32 offset, u32 val) {
  *(volatile u32 *)((u64)gicr_base + cpuid * 0x20000 + offset) = val;
}

static inline u64 gicr_r64(int cpuid, u32 offset) {
  return *(volatile u64 *)((u64)gicr_base + cpuid * 0x20000 + offset);
}

static inline void gicr_w64(int cpuid, u32 offset, u32 val) {
  *(volatile u64 *)((u64)gicr_base + cpuid * 0x20000 + offset) = val;
}

static u64 gicv3_read_lr(int n) {
  if(n > gicv3_irqchip.max_lr)
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
  if(n > gicv3_irqchip.max_lr)
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

static u64 gicv3_pending_lr(struct gic_pending_irq *irq) {
  u64 lr = irq->virq;

  lr |= LR_PENDING << ICH_LR_STATE_SHIFT;

  if(irq->group == 1)
    lr |= ICH_LR_GROUP1;

  lr |= (u64)irq->priority << ICH_LR_Priority_SHIFT;
  
  if(irq->pirq) {
    /* this is hw irq */
    lr |= ICH_LR_HW;
    lr |= ((u64)irq_no(irq->pirq) & 0x3ff) << ICH_LR_PINTID_SHIFT;
  }

  return lr;
}

static int gicv3_inject_guest_irq(struct gic_pending_irq *irq) {
  u32 virq = irq->virq;

  if(virq == 2)
    panic("!? maybe Linux kernel panicked");

  u64 elsr = read_sysreg(ich_elsr_el2);
  int freelr = -1;
  u64 lr;

  for(int i = 0; i < gicv3_irqchip.max_lr; i++) {
    if((elsr >> i) & 0x1) {   // free area in lr
      if(freelr < 0)
        freelr = i;

      continue;
    }

    if((u32)gicv3_read_lr(i) == virq)
      return -1;
  }

  if(freelr < 0)
    panic("busy");

  lr = gicv3_pending_lr(irq);

  gicv3_write_lr(freelr, lr);

  return 0;
}

static u32 gicv3_read_iar() {
  return read_sysreg(icc_iar1_el1);
}

static void gicv3_eoi(u32 iar) {
  write_sysreg(icc_eoir1_el1, iar);
}

static void gicv3_deactive_irq(u32 irq) {
  write_sysreg(icc_dir_el1, irq);
}

static void gicv3_host_eoi(u32 iar) {
  gicv3_eoi(iar);
  gicv3_deactive_irq(iar & 0x3ff);
}

static void gicv3_guest_eoi(u32 iar) {
  gicv3_eoi(iar);
}

static void gicv3_send_sgi(struct gic_sgi *sgi) {
  u64 sgir = (sgi->sgi_id & 0xf) << ICC_SGI1R_INTID_SHIFT;

  if(sgi->mode == ROUTE_BROADCAST) {
    sgir |= 1ul << 40;      // IRM
  } else if(sgi->mode == ROUTE_TARGETS) {
    sgir |= sgi->targets & 0xffff;
  } else {
    panic("unknown sgi");
  }

  dsb(ish);

  write_sysreg(icc_sgi1r_el1, sgir);

  isb();
}

static bool gicv3_irq_pending(u32 irq) {
  u32 is;

  if(is_sgi_ppi(irq)) {
    int id = cpuid();

    is = gicr_r32(id, GICR_ISPENDR0);
    return !!(is & (1 << irq));
  } else if(is_spi(irq)) {
    is = gicd_r(GICD_ISPENDR(irq / 32));
    return !!(is & (1 << (irq % 32)));
  } else {
    panic("GICv3: unknown irq: %d", irq);
  }
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
  } else {
    panic("GICv3: unknown irq: %d", irq);
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
  } else {
    panic("GICv3: unknown irq: %d", irq);
  }
}

static void gicv3_disable_irq(u32 irq) {
  u32 is;

  if(is_sgi_ppi(irq)) {
    int id = cpuid();

    is = 1 << irq;
    gicr_w32(id, GICR_ICENABLER0, is);
  } else if(is_spi(irq)) {
    is = 1 << (irq % 32);
    gicd_w(GICD_ICENABLER(irq / 32), is);
  } else {
    panic("GICv3: unknown irq: %d", irq);
  }
}

static void gicv3_set_target(u32 irq, u8 target) {
  ;
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

static bool gicv3_guest_irq_pending(u32 virq) {
  for(int i = 0; i <= gicv3_irqchip.max_lr; i++) {
    u64 lr = gicv3_read_lr(i);

    if((u32)lr == virq) {
      if(lr_is_pending(lr))
        return true;
      else
        return false;
    }
  }

  return false;
}

static void gicv3_setup_irq(u32 irq) {
  if(is_spi(irq))
    gicv3_set_target(irq, 0);

  gicv3_enable_irq(irq);
}

static void gicv3_irq_handler(int from_guest) {
  do {
    u32 irq = gicv3_read_iar();

    if(irq == 1023)    /* spurious interrupt */
      break;

    if(likely(is_ppi_spi(irq))) {
      isb();

      local_irq_enable();

      int handled = handle_irq(irq);

      local_irq_disable();

      if(handled)
        gicv3_host_eoi(irq);
    } else if(is_sgi(irq)) {
      cpu_sgi_handler(irq);

      gicv3_host_eoi(irq);
    } else {
      panic("???????");
    }
  } while(1);
}

static void gicv3_c_init(void) {
  write_sysreg(icc_igrpen0_el1, 0);
  write_sysreg(icc_igrpen1_el1, 0);

  write_sysreg(icc_pmr_el1, 0xff);
  write_sysreg(icc_ctlr_el1, ICC_CTLR_EOImode(1));

  write_sysreg(icc_igrpen1_el1, 1);

  isb();
}

static void gicv3_d_wait_for_rwp() {
  while(gicd_r(GICD_CTLR) & GICD_CTLR_RWP)
    ;
}

static inline bool security_disabled() {
  return !!(gicd_r(GICD_CTLR) & GICD_CTLR_DS);
}

static void gicv3_d_init(void) {
  u32 ctlr;

  /* disabled gicd */
  gicd_w(GICD_CTLR, 0);
  gicv3_d_wait_for_rwp();

  u32 pidr2 = gicd_r(GICD_PIDR2);
  u32 archrev = GICD_PIDR2_ArchRev(pidr2);
  if(archrev != 0x3)
    panic("gicv3?");

  vmm_log("GICv3: security state: %s\n", security_disabled() ? "disabled" : "enabled");

  u32 lines = gicd_r(GICD_TYPER) & 0x1f;
  u32 nirqs = 32 * (lines + 1);

  gicv3_irqchip.nirqs = nirqs < 1020 ? nirqs : 1020;

  for(int i = 0; i < nirqs; i += 4)
    gicd_w(GICD_IGROUPR(i / 4), ~0);

  ctlr = GICD_CTLR_NS_ENGRP1 | GICD_CTLR_NS_ENGRP1A | GICD_CTLR_NS_ARE_NS;
  gicd_w(GICD_CTLR, ctlr);
  gicv3_d_wait_for_rwp();

  isb();
}

static void gicv3_r_init() {
  int id = cpuid();

  gicr_w32(id, GICR_CTLR, 0);

  u32 sre = read_sysreg(icc_sre_el2);
  write_sysreg(icc_sre_el2, sre | (1 << 3) | 1);

  isb();

  sre = read_sysreg(icc_sre_el1);
  write_sysreg(icc_sre_el1, sre | 1);

  gicr_w32(id, GICR_IGROUPR0, ~0);
  gicr_w32(id, GICR_IGRPMODR0, 0);

  gicr_w32(id, GICR_ICACTIVER0, 0xffffffff);
  /* disable PPI */
  gicr_w32(id, GICR_ICENABLER0, 0xffff0000);
  /* enable SGI */
  gicr_w32(id, GICR_ISENABLER0, 0x0000ffff);

  u32 waker = gicr_r32(id, GICR_WAKER);
  gicr_w32(id, GICR_WAKER, waker & ~(1<<1));
  while(gicr_r32(id, GICR_WAKER) & (1<<2))
    ;

  isb();
}

static void gicv3_h_init(void) {
  u64 i = read_sysreg(ich_vtr_el2);

  gicv3_irqchip.max_lr = i & 0x1f;

  write_sysreg(ich_vmcr_el2, ICH_VMCR_VENG1);
  write_sysreg(ich_hcr_el2, ICH_HCR_EN);

  isb();
}

static void gicv3_init_cpu() {
  gicv3_c_init();
  gicv3_r_init();
  gicv3_h_init();
}

static int gicv3_dt_init(struct device_node *dev) {
  u64 dbase, dsize, rbase, rsize;

  if(dt_node_prop_addr(dev, 0, &dbase, &dsize) < 0)
    return -1;

  if(dt_node_prop_addr(dev, 1, &rbase, &rsize) < 0)
    return -1;

  gicd_base = iomap(dbase, dsize);
  if(!gicd_base)
    return -1;

  gicr_base = iomap(rbase, rsize);
  if(!gicr_base)
    return -1;

  gicv3_d_init();
  gicv3_h_init();

  vmm_log("GICv3 detected:\n");
  vmm_log("GICv3: nirqs: %d max_lr: %d\n", gicv3_irqchip.nirqs, gicv3_irqchip.max_lr);
  vmm_log("GICv3: dist base %p\n"
          "       redist base %p\n", gicd_base, gicr_base);

  localnode.irqchip = &gicv3_irqchip;

  return 0;
}

static struct gic_irqchip gicv3_irqchip = {
  .version = 3,

  .initcore           = gicv3_init_cpu,
  .inject_guest_irq   = gicv3_inject_guest_irq,
  .irq_pending        = gicv3_irq_pending,
  .guest_irq_pending  = gicv3_guest_irq_pending,
  .host_eoi           = gicv3_host_eoi,
  .guest_eoi          = gicv3_guest_eoi,
  .deactive_irq       = gicv3_deactive_irq,
  .send_sgi           = gicv3_send_sgi,
  .irq_enabled        = gicv3_irq_enabled,
  .enable_irq         = gicv3_enable_irq,
  .disable_irq        = gicv3_disable_irq,
  .setup_irq          = gicv3_setup_irq,
  .set_target         = gicv3_set_target,
  .irq_handler        = gicv3_irq_handler,
};

static struct dt_compatible gicv3_compat[] = {
  { "arm,gic-v3" },
  {},
};

DT_IRQCHIP_INIT(gicv3, gicv3_compat, gicv3_dt_init);
