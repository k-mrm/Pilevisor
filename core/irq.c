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

  irq->handler = handler;
  irq->arg = arg;

  localnode.irqchip->setup_irq(pirq);
}

void irq_entry(int from_guest) {
  if(local_irq_enabled())  
    panic("local irq enabled?");

  localnode.irqchip->irq_handler(from_guest);

  if(mycpu->recv_waitq)
    handle_recv_waitqueue();
}

int handle_irq(u32 irqno) {
  /* interrupt to vmm */
  struct irq *irq = irq_get(irqno);
  int irqret = 0;

  if(irq->handler) {
    irq->handler(irq->arg);
    irqret = 1;

    goto end;
  }

  /* inject irq to guest */
  localnode.irqchip->guest_eoi(irqno);

  vgic_inject_virq(current, irqno);

end:
  return irqret;
}
