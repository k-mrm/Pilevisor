#ifndef CORE_IRQ_H
#define CORE_IRQ_H

#include "types.h"
#include "param.h"
#include "spinlock.h"

#define NIRQ        256

struct irq {
  int count;
  void (*handler)(void *);
  void *arg;
  int nhandle[NCPU_MAX];

  spinlock_t lock;
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

void irqstats(void);

void irq_enable(struct irq *irq);
void irq_disable(struct irq *irq);

#endif
