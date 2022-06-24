#include "types.h"
#include "vpsci.h"
#include "log.h"

void _start(void);

static i32 vpsci_cpu_on(struct vcpu *vcpu, struct vpsci *vpsci) {
  u64 target_cpu = vpsci->x1;
  u64 ep_addr = vpsci->x2;
  u64 contextid = vpsci->x3;
  vmm_log("vcpu%d on: entrypoint %p\n", target_cpu, ep_addr);

  if(target_cpu >= vcpu->vm->nvcpu) {
    vmm_warn("vcpu%d wakeup failed\n", target_cpu);
    return -1;
  }

  struct vcpu *target = vcpu->vm->vcpus[target_cpu];
  target->reg.elr = ep_addr;

  vcpu_ready(target);

  /* wakeup pcpu */
  return psci_call(PSCI_SYSTEM_CPUON, target_cpu, (u64)_start, 0);
}

static u32 vpsci_version() {
  return psci_call(PSCI_VERSION, 0, 0, 0);
}

static i32 vpsci_migrate_info_type() {
  return psci_call(PSCI_MIGRATE_INFO_TYPE, 0, 0, 0);
}

u64 vpsci_emulate(struct vcpu *vcpu, struct vpsci *vpsci) {
  switch(vpsci->funcid) {
    case PSCI_VERSION:
      return vpsci_version();
    case PSCI_MIGRATE_INFO_TYPE:
      return (i64)vpsci_migrate_info_type();
    case PSCI_SYSTEM_OFF:
      /* TODO: shutdown vm */
      break;
    case PSCI_SYSTEM_RESET:
      /* TODO: reboot vm */
      break;
    case PSCI_SYSTEM_CPUON:
      return (i64)vpsci_cpu_on(vcpu, vpsci);
    default:
      panic("unknown funcid: %p\n", vpsci->funcid);
      return -1;
  }
}
