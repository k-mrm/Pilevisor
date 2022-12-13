#include "types.h"
#include "psci.h"
#include "power.h"
#include "log.h"
#include "device.h"

extern u64 psci_call(u32 func, u64 cpuid, u64 entry, u64 ctxid);

enum psci_method {
  PSCI_HVC,
  PSCI_SMC,
};

struct psci_info {
  u32 migrate;
  u32 cpu_on;
  u32 cpu_off;
  u32 cpu_suspend;
  enum psci_method method;
} psci;

static char *psci_status_map(int status) {
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
  i64 status = psci_call(psci.cpu_on, cpuid, entrypoint, 0);

  if(status == PSCI_SUCCESS) {
    return 0;
  } else {
    vmm_warn("cpu%d wakeup failed: %d(=%s)", cpuid, status, psci_status_map(status));
    return -1;
  }
}

static void psci_init(struct device_node *dev) {
  const char *meth = dt_node_props(dev, "method");

  if(strcmp(meth, "hvc") != 0)
    psci.method = PSCI_HVC;
  else if(strcmp(meth, "smc") != 0)
    psci.method = PSCI_SMC;
  else
    panic("psci method");

  int rc;

  dt_node_propa(dev, "migrate", &psci.migrate);
  dt_node_propa(dev, "cpu_suspend", &psci.cpu_suspend);

  rc = dt_node_propa(dev, "cpu_on", &psci.cpu_on);
  if(rc < 0)
    panic("cpu on");

  rc = dt_node_propa(dev, "cpu_off", &psci.cpu_off);
  if(rc < 0)
    panic("cpu off");
}

struct powerctl pscichip = {
  .name = "psci",
  .init = psci_init,
  .wakeup = psci_cpu_wakeup,
  .suspend = NULL,
  .system_shutdown = NULL,
  .system_reboot = NULL,
};
