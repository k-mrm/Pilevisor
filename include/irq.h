#ifndef CORE_IRQ_H
#define CORE_IRQ_H

#include "types.h"

struct irq_guest {
  int irq;
  bool guest_enabled;
};

struct irq_guest irq_guests[256];

int handle_irq(u32 pirq);

#endif
