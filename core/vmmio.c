#include "vmmio.h"
#include "vcpu.h"
#include "msg.h"
#include "cluster.h"

int vmmio_forward(u32 target_vcpuid, struct mmio_access *mmio) {
  int target_nodeid = vcpuid_to_nodeid(target_vcpuid);

  struct pocv2_msg msg;
  struct mmio_req_hdr hdr;

  hdr.vcpuid = target_vcpuid;
  memcpy(&hdr.mmio, mmio, sizeof(*mmio));

  pocv2_msg_init2(&msg, target_nodeid, MSG_MMIO_REQUEST, &hdr, NULL, 0);

  send_msg(&msg);

  for(;;)
    wfi();
}

static void vmmio_reply(u8 *dst_mac, enum vmmio_status status, u64 addr, u64 val) {
  struct pocv2_msg msg;
  struct mmio_reply_hdr hdr;

  hdr.addr = addr;
  hdr.val = val;
  hdr.status = status;

  pocv2_msg_init(&msg, dst_mac, MSG_MMIO_REPLY, &hdr, NULL, 0);

  send_msg(&msg);
}

static void vmmio_req_recv_intr(struct pocv2_msg *msg) {
  struct mmio_req_hdr *hdr = (struct mmio_req_hdr *)msg->hdr;
  enum vmmio_status status = VMMIO_OK;

  struct vcpu *vcpu = vcpu_get(hdr->vcpuid);
  if(!vcpu)
    panic("mmio????????");

  if(mmio_emulate(vcpu, &hdr->mmio) < 0)
    status = VMMIO_FAILED;

  vmmio_reply(pocv2_msg_src_mac(msg), status, hdr->mmio.ipa, hdr->mmio.val);
}

static void vmmio_reply_recv_intr(struct pocv2_msg *msg) {
  struct mmio_reply_hdr *hdr = (struct mmio_reply_hdr *)msg->hdr;

  if(hdr->status == VMMIO_OK)
    vmm_log("vmmio recv ok: @%p : %p\n", hdr->addr, hdr->val);
  else
    vmm_log("vmmio failed\n");
}

DEFINE_POCV2_MSG(MSG_MMIO_REQUEST, struct mmio_req_hdr, vmmio_req_recv_intr);
DEFINE_POCV2_MSG(MSG_MMIO_REPLY, struct mmio_reply_hdr, vmmio_reply_recv_intr);
