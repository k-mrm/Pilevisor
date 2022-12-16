#include "pcpu.h"
#include "aarch64.h"
#include "log.h"
#include "spinlock.h"
#include "device.h"
#include "localnode.h"
#include "panic.h"
#include "gic.h"
#include "msg.h"

struct pcpu pcpus[NCPU_MAX];
char _stack[PAGESIZE*NCPU_MAX] __aligned(PAGESIZE);
int nr_online_pcpus;

void cpu_stop_all() {
  struct gic_sgi sgi = {
    .sgi_id = SGI_STOP,
    .mode = ROUTE_BROADCAST,
  };

  localnode.irqchip->send_sgi(&sgi);
}

void cpu_stop_local() {
  local_irq_disable();

  for(;;)
    wfi();
}

void pcpu_init_core() {
  if(!mycpu->online)
    panic("offline cpu?");

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
  u32 reg;

  do {
    cpu = dt_find_node_type_cont(cpus, "cpu", cpu);
    if(!cpu)
      break;

    if(nr_online_pcpus++ > NCPU_MAX)
      panic("too many cpu");

    int rc = dt_node_propa(cpu, "reg", &reg);
    if(rc < 0)
      panic("cpu? %s", cpu->name);

    const char *compat = dt_node_props(cpu, "compatible");

    printf("cpu: %s id: %d compat %s\n", cpu->name, reg, compat);

    pcpus[reg].online = true;
  } while(1);

  if(nr_online_pcpus == 0)
    panic("no pcpu?");
}
