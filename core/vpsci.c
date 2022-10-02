#include "types.h"
#include "vpsci.h"
#include "log.h"
#include "node.h"
#include "msg.h"
#include "cluster.h"
#include "pcpu.h"

void _start(void);

struct cpu_wakeup_msg_hdr {
  POCV2_MSG_HDR_STRUCT;
  u32 vcpuid;
  u64 entrypoint;
  u64 contextid;
};

struct cpu_wakeup_ack_hdr {
  POCV2_MSG_HDR_STRUCT;
  i32 ret;
};

static i32 vpsci_remote_cpu_wakeup(u32 target_cpuid, u64 ep_addr, u64 contextid) {
  struct pocv2_msg msg;
  struct cpu_wakeup_msg_hdr hdr;

  hdr.vcpuid = target_cpuid;
  hdr.entrypoint = ep_addr;
  hdr.contextid = contextid;

  int nodeid = vcpuid_to_nodeid(target_cpuid);
  vmm_log("wakeup vcpu%d@node%d\n", target_cpuid, nodeid);

  pocv2_msg_init2(&msg, nodeid, MSG_CPU_WAKEUP, &hdr, NULL, 0);

  send_msg(&msg);

  for(;;)
    ;
}

static int vcpu_wakeup(struct vcpu *vcpu, u64 ep) {
  if(!vcpu) {
    vmm_log("no vcpu to wakeup\n");
    return PSCI_DENIED;
  }

  int localid = vcpu_localid(vcpu);

  vmm_log("wakeup vcpu%d(cpu%d)\n", vcpu->vmpidr, localid);

  int status;
  if(vcpu->online) {
    return PSCI_ALREADY_ON;
  } else {
    vcpu->reg.elr = ep;

    if(localcpu(localid)->wakeup) {  /* pcpu already wakeup */
      status = PSCI_SUCCESS;

      vcpu->online = true;
    } else {    /* pcpu sleeping... */
      status = psci_call(PSCI_SYSTEM_CPUON, localid, (u64)_start, 0);
    }
  }

  return status;
}

static void cpu_wakeup_recv_intr(struct pocv2_msg *msg) {
  struct cpu_wakeup_msg_hdr *hdr = (struct cpu_wakeup_msg_hdr *)msg->hdr;

  int vcpuid = hdr->vcpuid;
  u64 ep = hdr->entrypoint;

  struct vcpu *target = node_vcpu(vcpuid);

  int ret = vcpu_wakeup(target, ep);

  /* reply ack */
  struct pocv2_msg ack;
  struct cpu_wakeup_ack_hdr ackhdr;

  ackhdr.ret = ret;

  pocv2_msg_init(&ack, pocv2_msg_src_mac(msg), MSG_CPU_WAKEUP_ACK, &ackhdr, NULL, 0);

  send_msg(&ack);
}

static void cpu_wakeup_ack_recv_intr(struct pocv2_msg *msg) {
  struct cpu_wakeup_ack_hdr *hdr = (struct cpu_wakeup_ack_hdr *)msg->hdr;

  vmm_log("remote psci return %d\n", hdr->ret);
}

static i32 vpsci_cpu_on(struct vcpu *vcpu, struct vpsci_argv *argv) {
  u64 vcpuid = argv->x1;
  u64 ep_addr = argv->x2;
  u64 contextid = argv->x3;
  vmm_log("vcpu%d on: entrypoint %p\n", vcpuid, ep_addr);

  struct vcpu *target = node_vcpu(vcpuid);
  if(!target)
    return vpsci_remote_cpu_wakeup(vcpuid, ep_addr, contextid);

  /* target in localnode! */

  /* set entrypoint to target vcpu */
  return vcpu_wakeup(target, ep_addr);
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

DEFINE_POCV2_MSG(MSG_CPU_WAKEUP, struct cpu_wakeup_msg_hdr, cpu_wakeup_recv_intr);
DEFINE_POCV2_MSG(MSG_CPU_WAKEUP_ACK, struct cpu_wakeup_ack_hdr, cpu_wakeup_ack_recv_intr);
