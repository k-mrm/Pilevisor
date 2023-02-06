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
#include "spinlock.h"
#include "panic.h"
#include "assert.h"
#include "memlayout.h"

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

static i32 vpsci_remote_cpu_wakeup(u32 target_cpuid, u64 ep_addr, u64 contextid) {
  struct msg msg;
  struct msg *reply;
  struct cpu_wakeup_msg_hdr hdr;
  struct cpu_wakeup_ack_hdr *ack;

  hdr.vcpuid = target_cpuid;
  hdr.entrypoint = ep_addr;
  hdr.contextid = contextid;

  int nodeid = vcpuid_to_nodeid(target_cpuid);

  msg_init(&msg, nodeid, MSG_CPU_WAKEUP, &hdr, NULL, 0, M_WAITREPLY);

  printf("wakeup vcpu%d@node%d\n", target_cpuid, nodeid);

  reply = send_msg(&msg);

  ack = (struct cpu_wakeup_ack_hdr *)reply->hdr;
  int ret = ack->ret;

  printf("remote vcpu%d wakeup status: %d(=%s)\n", target_cpuid, ret, psci_status_map(ret));

  free_recv_msg(reply);

  return ret;
}

static int vpsci_vcpu_wakeup_local(struct vcpu *vcpu, u64 ep) {
  u64 flags;

  if(!vcpu) {
    panic("no vcpu to wakeup in this node\n");
    return PSCI_DENIED;
  }

  spin_lock_irqsave(&vcpu->lock, flags);

  int pcpuid = pcpu_id(vcpu->pcpu);
  int status;

  printf("wakeup vcpu%d(cpu%d)\n", vcpu->vmpidr, pcpuid);

  if(vcpu->online) {
    status = PSCI_ALREADY_ON;
  } else {
    vcpu->reg.elr = ep;

    if(vcpu->pcpu->wakeup) {  /* pcpu already wakeup */
      status = PSCI_SUCCESS;
    } else {    /* pcpu sleeping... */
      int c = cpu_boot(vcpu->pcpu, (u64)V2P(_start));
      if(c < 0) {
        vmm_warn("cpu%d wakeup failed", pcpuid);
        status = PSCI_DENIED;
      } else {
        status = PSCI_SUCCESS;
      }
    }

    if(status == PSCI_SUCCESS)
      vcpu->online = true;
  }

  spin_unlock_irqrestore(&vcpu->lock, flags);

  return status;
}

static void cpu_wakeup_recv_intr(struct msg *msg) {
  struct cpu_wakeup_ack_hdr ackhdr;
  int ret;
  struct cpu_wakeup_msg_hdr *hdr = (struct cpu_wakeup_msg_hdr *)msg->hdr;
  int vcpuid = hdr->vcpuid;
  u64 ep = hdr->entrypoint;
  struct vcpu *target = node_vcpu(vcpuid);

  assert(target);

  ret = vpsci_vcpu_wakeup_local(target, ep);

  /* reply ack */
  ackhdr.ret = ret;

  msg_reply(msg, MSG_CPU_WAKEUP_ACK, (struct msg_header *)&ackhdr, NULL, 0);
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
  return vpsci_vcpu_wakeup_local(target, ep_addr);
}

static i32 vpsci_features(struct vpsci_argv *argv) {
  u32 fid = argv->x1;

  switch(fid) {
    case PSCI_VERSION:
    case PSCI_FEATURES:
    case PSCI_SYSTEM_OFF:
    case PSCI_SYSTEM_RESET:
    case PSCI_SYSTEM_CPUON:
    case PSCI_MIGRATE_INFO_TYPE:
      return 0;
    default:
      return PSCI_NOT_SUPPORTED;
  }
}

u64 vpsci_emulate(struct vcpu *vcpu, struct vpsci_argv *argv) {
  switch(argv->funcid) {
    case PSCI_VERSION:
      return PSCI_VERSION_1_1;
    case PSCI_MIGRATE_INFO_TYPE:
      return PSCI_NOT_SUPPORTED;
    case PSCI_FEATURES:
      return (i64)vpsci_features(argv);
    case PSCI_SYSTEM_OFF:
      /* TODO: shutdown vm */
      panic("PSCI_SYSTEM_OFF");
    case PSCI_SYSTEM_RESET:
      /* TODO: reboot vm */
      panic("PSCI_SYSTEM_RESET");
    case PSCI_SYSTEM_CPUON:
      return (i64)vpsci_cpu_on(vcpu, argv);
    default:
      panic("unknown funcid: %p\n", argv->funcid);
  }

  return 0;
}

DEFINE_POCV2_MSG(MSG_CPU_WAKEUP, struct cpu_wakeup_msg_hdr, cpu_wakeup_recv_intr);
DEFINE_POCV2_MSG(MSG_CPU_WAKEUP_ACK, struct cpu_wakeup_ack_hdr, NULL);
