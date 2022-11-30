#ifndef CORE_IRQ_H
#define CORE_IRQ_H

#include "types.h"

#define NIRQ        256

struct irq {
  int count;
  void (*handler)(void *);
  void *arg;
};

extern struct irq irqlist[NIRQ];

static inline struct irq *irq_get(u32 pirq) {
  return pirq < NIRQ ? &irqlist[pirq] : NULL;
}

static inline unsigned int irq_no(struct irq *irq) {
  return irq - irqlist;
}

int handle_irq(u32 pirq);
void irq_register(u32 pirq, void (*handler)(void *), void *arg);

#endif
