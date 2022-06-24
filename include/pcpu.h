#ifndef MVMM_PCPU_H
#define MVMM_PCPU_H

#include "types.h"
#include "vcpu.h"
#include "param.h"

struct pcpu {
  int cpuid;
  struct vcpu *vcpu;  /* current vcpu */
  struct vcpu *ready;
};

extern struct pcpu pcpus[NCPU];

struct pcpu *cur_pcpu(void);
void pcpu_init(void);

#endif
