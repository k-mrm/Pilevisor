#include "types.h"
#include "psci.h"
#include "power.h"
#include "log.h"

extern u64 psci_call(u32 func, u64 cpuid, u64 entry, u64 ctxid);

char *psci_status_map(int status) {
  switch(status) {
    case PSCI_SUCCESS:
      return "SUCCESS";
    case PSCI_NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case PSCI_INVALID_PARAMETERS:
      return "INVALID_PARAMETERS";
    case PSCI_DENIED:
      return "DENIED"; 
    case PSCI_ALREADY_ON:
      return "ALREADY_ON";
    case PSCI_ON_PENDING:
      return "ON_PENDING"; 
    case PSCI_INTERNAL_FAILURE:
      return "INTERNAL_FAILURE"; 
    case PSCI_NOT_PRESENT:
      return "NOT_PRESENT"; 
    case PSCI_DISABLED:
      return "DISABLED"; 
    case PSCI_INVALID_ADDRESS:
      return "INVALID_ADDRESS";
    default:
      return "???";
  }
}

static int psci_cpu_wakeup(int cpuid, u64 entrypoint) {
  i64 status = psci_call(PSCI_SYSTEM_CPUON, cpuid, entrypoint, 0);

  if(status == PSCI_SUCCESS) {
    return 0;
  } else {
    vmm_warn("cpu%d wakeup failed: %d(=%s)", cpuid, status, psci_status_map(status));
    return -1;
  }
}

struct powerctl psci = {
  .name = "psci",
  .wakeup = psci_cpu_wakeup,
  .suspend = NULL,
  .system_shutdown = NULL,
  .system_reboot = NULL,
};
