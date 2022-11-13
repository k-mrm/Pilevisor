#include "aarch64.h"
#include "irq.h"
#include "gic.h"
#include "vgic.h"
#include "vcpu.h"
#include "pcpu.h"
#include "panic.h"

static struct irq irqlist[NIRQ];

struct irq *irq_get(u32 pirq) {
  if(pirq < NLOCALIRQ)
    return &mycpu->local_irq[pirq];
  else if(pirq < NIRQ)
    return &irqlist[pirq];

  return NULL;
}

void irq_register(u32 pirq, void (*handler)(void *), void *arg) {
  if(pirq > NIRQ)
    panic("pirq %d", pirq);

  vmm_log("new interrupt: %d\n", pirq);

  struct irq *irq = irq_get(pirq);

  irq->handler = handler;
  irq->arg = arg;

  gic_setup_spi(pirq);
}

int handle_irq(u32 pirq) {
  /* interrupt to vmm */
  struct irq *irq = irq_get(pirq);

  if(irq->handler) {
    irq->handler(irq->arg);
    return 1;
  }

  /* inject irq to guest */
  gic_guest_eoi(pirq, 1);

  vgic_inject_virq(current, pirq, pirq, 1);

  return 0;
}
