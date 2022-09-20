#ifndef MVMM_PCPU_H
#define MVMM_PCPU_H

#include "types.h"
#include "vcpu.h"
#include "param.h"
#include "mm.h"

extern char _stack[PAGESIZE] __attribute__((aligned(PAGESIZE)));

struct pcpu {
  void *stackbase;
  int mpidr;

  struct {
    void *base;
  } gicr;
};

extern struct pcpu pcpus[NCPU];

void pcpu_init(void);

#define mycpu     (&pcpus[cpuid()])

#endif
