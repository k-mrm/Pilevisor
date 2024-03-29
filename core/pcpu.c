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
static bool disallow_mp;

extern const struct cpu_enable_method psci;
extern const struct cpu_enable_method spin_table;

enum sgi_id {
  SGI_INJECT,
  SGI_STOP,
  SGI_DO_RECVQ,
};

static inline void __send_sgi(int sgi_id, int cpu) {
  struct gic_sgi sgi = {
    .sgi_id = sgi_id,
    .targets = 1u << cpu,
    .mode = SGI_ROUTE_TARGETS,
  };

  localnode.irqchip->send_sgi(&sgi);
}

static inline void __send_sgi_bcast(int sgi_id) {
  struct gic_sgi sgi = {
    .sgi_id = sgi_id,
    .mode = SGI_ROUTE_BROADCAST,
  };

  if(localnode.irqchip)
    localnode.irqchip->send_sgi(&sgi);
}

void cpu_stop_all() {
  __send_sgi_bcast(SGI_STOP);
}

void cpu_stop_local() {
  local_irq_disable();

  for(;;)
    wfi();
}

void cpu_send_inject_sgi(struct pcpu *cpu) {
  int id = pcpu_id(cpu);
  __send_sgi(SGI_INJECT, id);
}

void cpu_send_do_recvq_sgi(struct pcpu *cpu) {
  int id = pcpu_id(cpu);
  __send_sgi(SGI_DO_RECVQ, id);
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
  mycpu->wakeup = true;
  msg_queue_init(&mycpu->recv_waitq);
  mycpu->irq_depth = 0;
  mycpu->lazyirq_depth = 0;
  mycpu->lazyirq_enabled = true;
}

int cpu_boot(struct pcpu *cpu, u64 entrypoint) {
  if(!cpu->enable_method || !cpu->enable_method->boot)
    return -1;

  return cpu->enable_method->boot(cpu, entrypoint);
}

static int cpu_init_enable_method(struct pcpu *c, struct device_node *cpudev) {
  const char *enable_method = dt_node_props(cpudev, "enable-method");

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

  return c->enable_method->init(c);
}

static int cpu_prepare(struct device_node *cpudev) {
  u32 mpidr;
  int rc = dt_node_propa(cpudev, "reg", &mpidr);
  if(rc < 0)
    return -1;

  struct pcpu *cpu = get_cpu(mpidr);

  cpu->online = true;
  cpu->device = cpudev;
  cpu->mpidr = mpidr;

  const char *compat = dt_node_props(cpudev, "compatible");
  printf("cpu: %s mpidr: %d compat %s\n", cpudev->name, mpidr, compat);

  if(cpu_init_enable_method(cpu, cpudev) < 0) {
    disallow_mp = true;
    printf("enable-method?\n");
  }

  return 0;
}

void pcpu_init() {
  struct device_node *cpu;

  disallow_mp = false;

  for(cpu = dt_next_cpu_device(NULL); cpu;
      cpu = dt_next_cpu_device(cpu)) {
    if(nr_online_pcpus++ > NCPU_MAX)
      panic("too many cpu");

    if(cpu_prepare(cpu) < 0)
      panic("cpu? %s", cpu->name);
  }

  if(nr_online_pcpus == 0)
    panic("no pcpu?");

  if(nr_online_pcpus > 1 && disallow_mp)
    panic("mp %d", disallow_mp);
}
