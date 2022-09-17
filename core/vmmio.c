#include "vmmio.h"
#include "vcpu.h"
#include "mmio.h"

int mmio_forward(struct vcpu *vcpu, struct mmio_access *mmio, int rn, int target_nodeid) {
  struct mmio_req_hdr req;
  req.wr = mmio->wnr;
  req.addr = mmio->ipa;
  req.accsz = mmio->accsize;
  if(mmio->wnr)
    req.val = vcpu_x(vcpu, rn);
}
