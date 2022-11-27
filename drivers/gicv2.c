/*
 *  GICv2 (generic interrupt controller) driver
 */

#include "aarch64.h"
#include "gic.h"
#include "gicv2.h"
#include "log.h"
#include "param.h"
#include "irq.h"
#include "vcpu.h"
#include "localnode.h"
#include "compiler.h"
#include "panic.h"

static struct gic_irqchip gicv2_irqchip;

static u64 gicc_base;
static u64 gicd_base;
static u64 gich_base;
static u64 gicv_base;

static inline u32 gicd_read(u32 offset) {
  return *(volatile u32 *)(gicd_base + offset);
}

static inline void gicd_write(u32 offset, u32 val) {
  *(volatile u32 *)(gicd_base + offset) = val;
}

static inline u32 gicc_read(u32 offset) {
  return *(volatile u32 *)(gicc_base + offset);
}

static inline void gicc_write(u32 offset, u32 val) {
  *(volatile u32 *)(gicc_base + offset) = val;
}

static u32 gicv2_read_lr(int n) {
  ;
}

static void gicv2_write_lr(int n, u32 val) {
  ;
}

static u32 gicv2_pending_lr(u32 pirq, u32 virq, int grp) {
  ;
}

static int gicv2_inject_guest_irq(u32 intid, int grp) {
  ;
}

static u32 gicv2_read_iar() {
  return gicc_read(GICC_IAR);
}

static void gicv2_eoi(u32 iar) {
  gicc_write(GICC_EOIR, iar);
}

static void gicv2_deactive_irq(u32 irq) {
  gicc_write(GICC_DIR, irq);
}

static void gicv2_host_eoi(u32 iar) {
  gicv2_eoi(iar);
  gicv2_deactive_irq(iar & 0x3ff);
}

static void gicv2_guest_eoi(u32 iar) {
  gicv2_eoi(iar);
}

static void gicv2_send_sgi(int cpuid, int sgi_id) {
  ;
}

static bool gicv2_irq_pending(u32 irq) {
  ;
}

static bool gicv2_irq_enabled(u32 irq) {
  ;
}

static void gicv2_enable_irq(u32 irq) {
  ;
}

static void gicv2_disable_irq(u32 irq) {
  ;
}

static void gicv2_set_target(u32 irq, u8 target) {
  if(is_sgi_ppi(irq))
    panic("sgi_ppi set target?");

  u32 itargetsr = gicd_read(GICD_ITARGETSR(irq / 4));
  itargetsr &= ~((u32)0xff << (irq % 4 * 8));
  gicd_write(GICD_ITARGETSR(irq / 4), itargetsr | (target << (irq % 4 * 8)));
}

static void gicv2_setup_irq(u32 irq) {
  ;
}

static void gicv2_h_init() {
  ;
}

static void gicv2_c_init() {
  ;
}

static void gicv2_d_init() {
  ;
}

static void gicv2_init_cpu(void) {
  ;
}

static int gicv2_max_spi() {
  u32 typer = gicd_read(GICD_TYPER);
  u32 lines = typer & 0x1f;
  u32 max_spi = 32 * (lines + 1) - 1;
  vmm_log("GICv2: typer %p\n", typer);

  return max_spi < 1020 ? max_spi : 1019;
}

static void gicv2_init(void) {
  gicd_base = GICDBASE;

  gicv2_d_init();

  gicv2_irqchip.max_spi = gicv2_max_spi();
  gicv2_irqchip.max_lr = gicv2_max_listregs();
}

static struct gic_irqchip gicv3_irqchip = {
  .version = 2,

  .init             = gicv2_init,
  .initcore         = gicv2_init_cpu,

  .inject_guest_irq = gicv2_inject_guest_irq,
  .irq_pending      = gicv2_irq_pending,
  .read_iar         = gicv2_read_iar,
  .host_eoi         = gicv2_host_eoi,
  .guest_eoi        = gicv2_guest_eoi,
  .deactive_irq     = gicv2_deactive_irq,
  .send_sgi         = gicv2_send_sgi,
  .irq_enabled      = gicv2_irq_enabled,
  .enable_irq       = gicv2_enable_irq,
  .disable_irq      = gicv2_disable_irq,
  .setup_irq        = gicv2_setup_irq,
  .set_target       = gicv2_set_target,
};

void gicv2_sysinit() {
  localnode.irqchip = &gicv2_irqchip;
}
