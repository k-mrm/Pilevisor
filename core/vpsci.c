#include "types.h"
#include "vpsci.h"
#include "log.h"
#include "node.h"

void _start(void);

static i32 vpsci_cpu_on(struct vcpu *vcpu, struct vpsci_argv *argv) {
  u64 target_cpuid = argv->x1;
  u64 ep_addr = argv->x2;
  u64 contextid = argv->x3;
  vmm_log("vcpu%d on: entrypoint %p\n", target_cpuid, ep_addr);

  if(target_cpuid >= localnode.nvcpu) {
    vmm_warn("vcpu%d wakeup failed\n", target_cpuid);
    return -1;
  }

  struct vcpu *target = &localnode.vcpus[target_cpuid];
  target->reg.elr = ep_addr;

  /* wakeup pcpu */
  return psci_call(PSCI_SYSTEM_CPUON, target_cpuid, (u64)_start, 0);
}

static u32 vpsci_version() {
  return psci_call(PSCI_VERSION, 0, 0, 0);
}

static i32 vpsci_migrate_info_type() {
  return psci_call(PSCI_MIGRATE_INFO_TYPE, 0, 0, 0);
}

static i32 vpsci_system_features(struct vpsci_argv *argv) {
  u32 fid = argv->x1;
  return psci_call(PSCI_SYSTEM_FEATURES, fid, 0, 0);
}

u64 vpsci_emulate(struct vcpu *vcpu, struct vpsci_argv *argv) {
  switch(argv->funcid) {
    case PSCI_VERSION:
      return vpsci_version();
    case PSCI_MIGRATE_INFO_TYPE:
      return (i64)vpsci_migrate_info_type();
    case PSCI_SYSTEM_FEATURES:
      return (i64)vpsci_system_features(argv);
    case PSCI_SYSTEM_OFF:
      /* TODO: shutdown vm */
      break;
    case PSCI_SYSTEM_RESET:
      /* TODO: reboot vm */
      break;
    case PSCI_SYSTEM_CPUON:
      return (i64)vpsci_cpu_on(vcpu, argv);
    default:
      panic("unknown funcid: %p\n", argv->funcid);
  }
}
