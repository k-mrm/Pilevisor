#include "aarch64.h"
#include "irq.h"
#include "gic.h"
#include "vgic.h"
#include "vcpu.h"
#include "pcpu.h"
#include "localnode.h"
#include "panic.h"

struct irq irqlist[NIRQ];

void irq_register(u32 pirq, void (*handler)(void *), void *arg) {
  if(pirq > NIRQ)
    panic("pirq %d", pirq);

  vmm_log("new interrupt: %d\n", pirq);

  struct irq *irq = irq_get(pirq);

  irq->count++;
  irq->handler = handler;
  irq->arg = arg;

  localnode.irqchip->setup_irq(pirq);
}

static inline void irq_enter() {
  mycpu->irq_depth++;
}

static inline void irq_exit() {
  mycpu->irq_depth--;
}

void irq_entry(int from_guest) {
  if(local_irq_enabled())  
    panic("local irq enabled?");

  irq_enter();

  localnode.irqchip->irq_handler(from_guest);

  irq_exit();

  if(!pocv2_msg_queue_empty(&mycpu->recv_waitq) && !in_interrupt() && local_lazyirq_enabled()) {
    handle_recv_waitqueue();
  }
}

void irqstats() {
  printf("irqstats\n");

  for(struct irq *irq = irqlist; irq < &irqlist[NIRQ]; irq++) {
    if(!irq->count)
      continue;

    printf("irq %d: ", irq_no(irq));
    for(int i = 0; i < NCPU_MAX; i++)
      printf("CPU%d: %d ", i, irq->nhandle[i]);
    printf("\n");
  }
}

int handle_irq(u32 irqno) {
  /* interrupt to vmm */
  struct irq *irq = irq_get(irqno);
  int irqret = 0;

  irq->nhandle[cpuid()]++;

  if(irq->handler) {
    irq->handler(irq->arg);
    irqret = 1;

    goto end;
  }

  /* inject irq to guest */
  localnode.irqchip->guest_eoi(irqno);

  /*
  if(irqno == 27)
    printf("cpu%d timerintr ", cpuid());
    */

  vgic_inject_virq(current, irqno);

end:
  return irqret;
}
