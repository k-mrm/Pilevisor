/*
 *  aarch64 arch-timer driver
 */

#include "arch-timer.h"
#include "aarch64.h"
#include "printf.h"
#include "irq.h"

#define CNTHP_CTL_EL2_ENABLE    (1ul << 0)
#define CNTHP_CTL_EL2_IMASK     (1ul << 1)
#define CNTHP_CTL_EL2_ISTATUS   (1ul << 2)

static u64 cpu_hz;

static void hyp_timer_intr(void *arg) {
  (void)arg;
}

void arch_timer_init_core() {
  u64 ctl = CNTHP_CTL_EL2_IMASK | CNTHP_CTL_EL2_ENABLE;

  write_sysreg(cnthp_ctl_el2, ctl);

  irq_register(26, hyp_timer_intr, NULL);
}

void arch_timer_init() {
  cpu_hz = read_sysreg(cntfrq_el0);
  printf("CPU %d Hz\n", cpu_hz);
}
