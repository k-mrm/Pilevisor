#ifndef CORE_EMUL_H
#define CORE_EMUL_H

#include "types.h"
#include "vcpu.h"

int cpu_emulate(struct vcpu *vcpu, u32 inst);

#endif
