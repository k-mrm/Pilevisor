#include "power.h"
#include "psci.h"
#include "localnode.h"
#include "log.h"
#include "panic.h"
#include "compiler.h"

int cpu_power_wakeup(int cpuid, u64 entrypoint) {
  if(localnode.powerctl->wakeup) {
    return localnode.powerctl->wakeup(cpuid, entrypoint);
  } else {
    vmm_warn("%s: wakeup NULL\n", localnode.powerctl->name);
    return -1;
  }
}

void cpu_power_suspend(__unused int cpuid) {
  ;
}

void system_shutdown() {
  ;
}

void system_reboot() {
  ;
}

void powerctl_init() {
  localnode.powerctl = &psci;
  // localnode.powerctl = &rpi4power;
}
