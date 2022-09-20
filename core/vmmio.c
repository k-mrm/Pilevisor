#include "vmmio.h"
#include "vcpu.h"
#include "mmio.h"
#include "msg.h"

int vmmio_forward(struct vcpu *vcpu, struct mmio_access *mmio, int rn, int target_nodeid) {
  struct mmio_req_hdr req;
  req.wr = mmio->wnr;
  req.addr = mmio->ipa;
  req.accsz = mmio->accsize;
  if(mmio->wnr)
    req.val = vcpu_x(vcpu, rn);
}

static void mmio_req_recv_intr(struct pocv2_msg *msg) {
  ;
}

DEFINE_POCV2_MSG(MSG_MMIO_REQUEST, struct mmio_req_hdr, mmio_req_recv_intr);
