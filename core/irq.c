#include "irq.h"
#include "gic.h"
#include "vgic.h"
#include "vcpu.h"

int handle_irq(u32 pirq) {
  /* interrupt to vmm */
  switch(pirq) {
    case 48:
      virtio_net_intr();
      return 1;
  }

  /* inject irq to guest */
  vgic_irq_enter(current);

  vgic_inject_virq(current, pirq, pirq, 1);

  return 0;
}
