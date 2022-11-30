#ifndef MVMM_PCPU_H
#define MVMM_PCPU_H

#include "types.h"
#include "vcpu.h"
#include "param.h"
#include "mm.h"
#include "irq.h"
#include "compiler.h"

extern char _stack[PAGESIZE] __aligned(PAGESIZE);

struct pcpu {
  void *stackbase;
  int mpidr;
  bool wakeup;

  union {
    struct {
      void *gicr_base;
    } v3;
    struct {
      ;
    } v2;
  };
};

extern struct pcpu pcpus[NCPU];

void pcpu_init(void);

#define mycpu         (&pcpus[cpuid()])
#define localcpu(id)  (&pcpus[id]) 

#endif
