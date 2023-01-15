#include "pcpu.h"
#include "aarch64.h"
#include "log.h"
#include "spinlock.h"
#include "device.h"
#include "localnode.h"
#include "panic.h"
#include "gic.h"
#include "msg.h"
#include "lib.h"

struct pcpu pcpus[NCPU_MAX];
char _stack[PAGESIZE*NCPU_MAX] __aligned(PAGESIZE);
static int nr_online_pcpus;

extern const struct cpu_enable_method psci;
extern const struct cpu_enable_method spin_table;

enum sgi_id {
  SGI_INJECT,
  SGI_STOP,
  SGI_DO_RECVQ,
};

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

void cpu_send_inject_sgi(int cpu) {
  struct gic_sgi sgi = {
    .sgi_id = SGI_INJECT,
    .targets = 1u << cpu,
    .mode = ROUTE_TARGETS,
  };

  localnode.irqchip->send_sgi(&sgi);
}

void cpu_send_do_recvq_sgi(int cpu) {
  struct gic_sgi sgi = {
    .sgi_id = SGI_DO_RECVQ,
    .targets = 1u << cpu,
    .mode = ROUTE_TARGETS,
  };

  localnode.irqchip->send_sgi(&sgi);
}

void cpu_sgi_handler(int sgi_id) {
  switch(sgi_id) {
    case SGI_INJECT:  /* inject guest pending interrupt */
      vgic_inject_pending_irqs();
      break;
    case SGI_STOP:
      printf("\ncpu%d: sgi stop received %p\n", cpuid(), read_sysreg(elr_el2));
      cpu_stop_local();
      break;
    case SGI_DO_RECVQ:
      /* do_recv_waitqueue will be called when tail of interrupt handler in irq_entry() */
      break;
    default:
      panic("unknown sgi %d", sgi_id);
  }
}

void pcpu_init_core() {
  if(!mycpu->online)
    panic("offline cpu?");

  mycpu->stackbase = _stack + PAGESIZE * (cpuid() + 1);
  mycpu->mpidr = cpuid();    /* affinity? */
  mycpu->wakeup = true;
  msg_queue_init(&mycpu->recv_waitq);
  mycpu->waiting_reply = NULL;
  mycpu->irq_depth = 0;
  mycpu->lazyirq_depth = 0;
  mycpu->lazyirq_enabled = true;
}

int cpu_boot(int cpu, u64 entrypoint) {
  if(cpu >= nr_online_pcpus)
    panic("no cpu!");

  struct pcpu *c = get_cpu(cpu);

  if(!c->enable_method || !c->enable_method->boot)
    return -1;

  return c->enable_method->boot(cpu, entrypoint);
}

static int cpu_init_enable_method(int cpu, struct device_node *cpudev) {
  const char *enable_method = dt_node_props(cpudev, "enable-method");
  struct pcpu *c = get_cpu(cpu);

  if(!enable_method)
    return -1;

  if(strcmp(enable_method, "spin-table") == 0) {
    c->enable_method = &spin_table;
  } else if(strcmp(enable_method, "psci") == 0) {
    c->enable_method = &psci;
  } else {
    vmm_warn("enable-method");
    return -1;
  }

  return c->enable_method->init(cpu);
}

static int cpu_prepare(struct device_node *cpudev) {
  u32 reg;
  int rc = dt_node_propa(cpudev, "reg", &reg);
  if(rc < 0)
    return -1;

  struct pcpu *cpu = get_cpu(reg);

  cpu->online = true;
  cpu->device = cpudev;

  const char *compat = dt_node_props(cpudev, "compatible");
  printf("cpu: %s id: %d compat %s\n", cpudev->name, reg, compat);

  cpu_init_enable_method(reg, cpudev);

  return 0;
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

    if(nr_online_pcpus++ > NCPU_MAX)
      panic("too many cpu");

    int rc = cpu_prepare(cpu);
    if(rc < 0)
      panic("cpu? %s", cpu->name);
  } while(1);

  if(nr_online_pcpus == 0)
    panic("no pcpu?");
}
