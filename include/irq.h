#ifndef CORE_IRQ_H
#define CORE_IRQ_H

#include "types.h"

#define NLOCALIRQ   32
#define NIRQ        256

struct irq {
  int irq;
  int count;
  void (*handler)(void *);
  void *arg;
};

struct irq *irq_get(u32 pirq);
int handle_irq(u32 pirq);
void irq_register(u32 pirq, void (*handler)(void *), void *arg);

#endif
