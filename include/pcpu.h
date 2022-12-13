#ifndef MVMM_PCPU_H
#define MVMM_PCPU_H

#include "types.h"
#include "param.h"
#include "mm.h"
#include "irq.h"
#include "msg.h"
#include "spinlock.h"
#include "compiler.h"

extern char _stack[PAGESIZE*NCPU_MAX] __aligned(PAGESIZE);

struct pcpu {
  void *stackbase;
  int mpidr;
  bool wakeup;
  
  struct pocv2_msg_queue recv_waitq;

  int irq_depth;
  bool lazyirq_enabled;

  union {
    struct {
      void *gicr_base;
    } v3;
    struct {
      ;
    } v2;
  };
};

extern struct pcpu pcpus[NCPU_MAX];

void pcpu_init_core(void);
void pcpu_init(void);

#define mycpu         (&pcpus[cpuid()])
#define localcpu(id)  (&pcpus[id]) 

#define local_lazyirq_enable()      (mycpu->lazyirq_enabled = true)
#define local_lazyirq_disable()     (mycpu->lazyirq_enabled = false)
#define local_lazyirq_enabled()     (mycpu->lazyirq_enabled)

#endif
