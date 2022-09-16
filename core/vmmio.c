#include "vmmio.h"
#include "vcpu.h"
#include "mmio.h"

int mmio_forward(struct vcpu *vcpu, struct mmio_access *mmio, int rn, int target_nodeid) {
  struct mmio_req req;
  req.wr = mmio->wnr;
  req.addr = mmio->ipa;
  req.accsz = mmio->accsz;
  if(mmio->wnr)
    req.val = vcpu_x(vcpu, rn);
}
