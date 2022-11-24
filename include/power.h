#ifndef DRIVER_POWER_H
#define DRIVER_POWER_H

#include "types.h"
#include "localnode.h"

struct powerctl {
  char *name;

  int (*wakeup)(int, u64);
  void (*suspend)(int);
  void (*system_shutdown)(void);
  void (*system_reboot)(void);
};

int cpu_power_wakeup(int cpuid, u64 entrypoint);
void cpu_power_suspend(int cpuid);
void system_shutdown(void);
void system_reboot(void);

void powerctl_init(void);

#endif
