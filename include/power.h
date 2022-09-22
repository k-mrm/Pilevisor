#ifndef DRIVER_POWER_H
#define DRIVER_POWER_H

struct cpu_power_ops {
  void (*wakeup)(int);
};

void set_cpu_power_ops();

#endif
