#ifndef CORE_VSYSREG_H
#define CORE_VSYSREG_H

#include "types.h"
#include "vcpu.h"

int vsysreg_emulate(struct vcpu *vcpu, u64 iss);

#endif
