#include "hello.h"
#include "uart.h"

// armv8 generic timer driver

#define CNTV_CTL_ENABLE   (1<<0)
#define CNTV_CTL_IMASK    (1<<1)
#define CNTV_CTL_ISTATUS  (1<<2)

static void enable_timer(void);
static void disable_timer(void);
static void reload_timer(void);

void timerinit() {
  unsigned long f;
  read_sysreg(f, cntfrq_el0);
  uart_puts("\nfreq: ");
  uart_put64(f, 10);

  disable_timer();
  reload_timer();
  enable_timer();
}

static void enable_timer() {
  unsigned long c;
  read_sysreg(c, cntv_ctl_el0);
  c |= CNTV_CTL_ENABLE;
  c &= ~CNTV_CTL_IMASK;
  write_sysreg(cntv_ctl_el0, c);
}

static void disable_timer() {
  unsigned long c;
  read_sysreg(c, cntv_ctl_el0);
  c &= ~CNTV_CTL_ENABLE;
  c |= CNTV_CTL_IMASK;
  write_sysreg(cntv_ctl_el0, c);
}

static void reload_timer() {
  // interval 1000ms
  unsigned long interval = 1000000;
  unsigned long f;
  read_sysreg(f, cntfrq_el0);
  unsigned long interval_clk = interval * (f / 1000000);

  write_sysreg(cntv_tval_el0, interval_clk);
}

void timerintr() {
  uart_puts("timerintr\n");
  disable_timer();
  reload_timer();
  enable_timer();
}
