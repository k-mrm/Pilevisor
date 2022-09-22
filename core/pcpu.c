#include "pcpu.h"
#include "aarch64.h"
#include "log.h"

struct pcpu pcpus[NCPU];
char _stack[PAGESIZE*NCPU] __attribute__((aligned(PAGESIZE)));

void pcpu_init() {
  mycpu->stackbase = _stack + PAGESIZE*cpuid();
  mycpu->mpidr = cpuid();    /* affinity? */
  mycpu->wakeup = true;
}
