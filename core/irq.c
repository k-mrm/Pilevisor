#include "irq.h"
#include "aarch64.h"
#include "gic.h"
#include "vgic.h"
#include "vcpu.h"
#include "pcpu.h"

#define NLOCALIRQ   32
#define NIRQ        256

static struct irq irqlist[NIRQ];

struct irq *irq_get(u32 pirq) {
  if(pirq < NLOCALIRQ)
    return &mycpu->local_irq[pirq];
  else if(pirq < NIRQ)
    return &irqlist[pirq];

  return NULL;
}

void irq_register(u32 pirq, void (*handler)(void)) {
  if(pirq > NIRQ)
    return;

  struct irq *irq = irq_get(pirq);

  gic_setup_spi(pirq);

  irq->handler = handler;
}

int handle_irq(u32 pirq) {
  /* interrupt to vmm */
  struct irq *irq = irq_get(pirq);

  if(irq->handler) {
    irq->handler();
    return 1;
  }

  /* inject irq to guest */
  vgic_irq_enter(current);

  gic_guest_eoi(pirq, 1);

  vgic_inject_virq(current, pirq, pirq, 1);

  return 0;
}
