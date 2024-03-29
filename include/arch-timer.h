#ifndef DRIVER_ARCH_TIMER_H
#define DRIVER_ARCH_TIMER_H

#include "types.h"
#include "aarch64.h"

void arch_timer_init_core(void);

void arch_timer_init(void);

void usleep(int us);

static inline u64 now_cycles() {
  return read_sysreg(cntpct_el0);
}

#endif
