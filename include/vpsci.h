#ifndef MVMM_VPSCI_H
#define MVMM_VPSCI_H

#include "types.h"
#include "vcpu.h"
#include "psci.h"

struct vpsci_argv {
  u32 funcid;
  u64 x1;
  u64 x2;
  u64 x3;
};

u64 vpsci_emulate(struct vcpu *vcpu, struct vpsci_argv *vpsci);

#endif
