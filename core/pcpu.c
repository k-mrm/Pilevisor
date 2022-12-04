#include "pcpu.h"
#include "aarch64.h"
#include "log.h"
#include "spinlock.h"
#include "msg.h"

struct pcpu pcpus[NCPU];
char _stack[PAGESIZE*NCPU] __aligned(PAGESIZE);

void pcpu_init() {
  mycpu->stackbase = _stack + PAGESIZE*(cpuid() + 1);
  mycpu->mpidr = cpuid();    /* affinity? */
  mycpu->wakeup = true;
  pocv2_msg_queue_init(&mycpu->recv_waitq);
  mycpu->irq_depth = 0;
  mycpu->lazyirq_enabled = true;
}
