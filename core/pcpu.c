#include "pcpu.h"
#include "aarch64.h"
#include "log.h"
#include "spinlock.h"
#include "device.h"
#include "localnode.h"
#include "panic.h"
#include "msg.h"

struct pcpu pcpus[NCPU_MAX];
char _stack[PAGESIZE*NCPU_MAX] __aligned(PAGESIZE);
int nr_online_pcpus;

void pcpu_init_core() {
  mycpu->stackbase = _stack + PAGESIZE * (cpuid() + 1);
  mycpu->mpidr = cpuid();    /* affinity? */
  mycpu->wakeup = true;
  pocv2_msg_queue_init(&mycpu->recv_waitq);
  mycpu->irq_depth = 0;
  mycpu->lazyirq_enabled = true;
}

void pcpu_init() {
  struct device_node *cpus = dt_find_node_path(localnode.device_tree, "/cpus");
  if(!cpus)
    panic("cpu?");

  struct device_node *cpu = NULL;

  do {
    cpu = dt_find_node_type_cont(cpus, "cpu", cpu);
    if(!cpu)
      break;

    nr_online_pcpus++;
    printf("cpu: %s\n", cpu->name);
  } while(1);

  if(nr_online_pcpus == 0)
    panic("no pcpu?");
  if(nr_online_pcpus > NCPU_MAX)
    panic("too many cpu");
}
