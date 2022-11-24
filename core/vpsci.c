/*
 *  virtual psci (power state coordination interface)
 *  emulate psci 1.1
 */

#include "types.h"
#include "vpsci.h"
#include "log.h"
#include "localnode.h"
#include "node.h"
#include "msg.h"
#include "pcpu.h"
#include "power.h"
#include "panic.h"

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
  struct cpu_wakeup_ack_hdr ack;

  hdr.vcpuid = target_cpuid;
  hdr.entrypoint = ep_addr;
  hdr.contextid = contextid;

  int nodeid = vcpuid_to_nodeid(target_cpuid);
  vmm_log("wakeup vcpu%d@node%d %p\n", target_cpuid, nodeid, read_sysreg(daif));

  pocv2_msg_init2(&msg, nodeid, MSG_CPU_WAKEUP, &hdr, NULL, 0);

  send_msg(&msg);

  pocv2_recv_reply(&msg, (struct pocv2_msg_header *)&ack);

  vmm_log("remote vcpu wakeup status: %d(=%s)\n", ack.ret, psci_status_map(ack.ret));

  return ack.ret;
}

static int vcpu_wakeup_local(struct vcpu *vcpu, u64 ep) {
  if(!vcpu) {
    panic("no vcpu to wakeup in this node\n");
    return PSCI_DENIED;
  }

  int localid = vcpu_localid(vcpu);
  int status;

  vmm_log("wakeup vcpu%d(cpu%d)\n", vcpu->vmpidr, localid);

  if(vcpu->online) {
    status = PSCI_ALREADY_ON;
  } else {
    vcpu->reg.elr = ep;

    if(localcpu(localid)->wakeup) {  /* pcpu already wakeup */
      status = PSCI_SUCCESS;
    } else {    /* pcpu sleeping... */
      int c = cpu_power_wakeup(localid, (u64)_start);
      if(c < 0)
        panic("cpu%d wakeup failed", localid);
      
      status = PSCI_SUCCESS;
    }

    vcpu->online = true;
  }

  return status;
}

static void cpu_wakeup_recv_intr(struct pocv2_msg *msg) {
  struct cpu_wakeup_msg_hdr *hdr = (struct cpu_wakeup_msg_hdr *)msg->hdr;

  int vcpuid = hdr->vcpuid;
  u64 ep = hdr->entrypoint;

  struct vcpu *target = node_vcpu(vcpuid);

  int ret = vcpu_wakeup_local(target, ep);

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

  msgenqueue(msg);
}

static i32 vpsci_cpu_on(struct vcpu *vcpu, struct vpsci_argv *argv) {
  u64 vcpuid = argv->x1;
  u64 ep_addr = argv->x2;
  u64 contextid = argv->x3;
  vmm_log("vcpu%d on: entrypoint %p %p\n", vcpuid, ep_addr, contextid);

  struct vcpu *target = node_vcpu(vcpuid);
  if(!target)
    return vpsci_remote_cpu_wakeup(vcpuid, ep_addr, contextid);

  /* target in localnode! */

  /* set entrypoint to target vcpu */
  return vcpu_wakeup_local(target, ep_addr);
}

static i32 vpsci_features(struct vpsci_argv *argv) {
  u32 fid = argv->x1;

  switch(fid) {
    case PSCI_VERSION:
    case PSCI_FEATURES:
    case PSCI_SYSTEM_OFF:
    case PSCI_SYSTEM_RESET:
    case PSCI_SYSTEM_CPUON:
      return 0;
    default:
      return PSCI_NOT_SUPPORTED;
  }
}

u64 vpsci_emulate(struct vcpu *vcpu, struct vpsci_argv *argv) {
  switch(argv->funcid) {
    case PSCI_VERSION:
      return PSCI_VERSION_1_1;
    case PSCI_FEATURES:
      return (i64)vpsci_features(argv);
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

  return 0;
}

DEFINE_POCV2_MSG(MSG_CPU_WAKEUP, struct cpu_wakeup_msg_hdr, cpu_wakeup_recv_intr);
DEFINE_POCV2_MSG(MSG_CPU_WAKEUP_ACK, struct cpu_wakeup_ack_hdr, cpu_wakeup_ack_recv_intr);
