#include "types.h"
#include "psci.h"
#include "log.h"
#include "device.h"
#include "localnode.h"
#include "lib.h"
#include "pcpu.h"

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
} psci_info;

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

static int psci_cpu_boot(int cpuid, u64 entrypoint) {
  i64 status = psci_call(psci_info.cpu_on, cpuid, entrypoint, 0);

  if(status == PSCI_SUCCESS) {
    return 0;
  } else {
    vmm_warn("psci: cpu%d wakeup failed: %d(=%s)", cpuid, status, psci_status_map(status));
    return -1;
  }
}

static int em_psci_init(int __unused cpu) {
  return 0;
}

const struct cpu_enable_method psci = {
  .init = em_psci_init,
  .boot = psci_cpu_boot,
};

void psci_init() {
  struct device_node *dev = dt_find_node_path(localnode.device_tree, "/psci");
  if(!dev)
    return;

  printf("psci found\n");

  const char *method = dt_node_props(dev, "method");

  if(strcmp(method, "hvc") != 0)
    psci_info.method = PSCI_HVC;
  else if(strcmp(method, "smc") != 0)
    psci_info.method = PSCI_SMC;
  else
    panic("psci method");

  int rc;

  dt_node_propa(dev, "migrate", &psci_info.migrate);
  dt_node_propa(dev, "cpu_suspend", &psci_info.cpu_suspend);

  rc = dt_node_propa(dev, "cpu_on", &psci_info.cpu_on);
  if(rc < 0)
    panic("cpu on");

  rc = dt_node_propa(dev, "cpu_off", &psci_info.cpu_off);
  if(rc < 0)
    panic("cpu off");
}
