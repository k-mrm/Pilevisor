#include "irq.h"
#include "aarch64.h"
#include "gic.h"
#include "vgic.h"
#include "vcpu.h"

int handle_irq(u32 pirq) {
  /* interrupt to vmm */
  switch(pirq) {
    case 33:
      uartintr();
      return 1;
    case 48:
      virtio_net_intr();
      return 1;
  }

  /* inject irq to guest */
  vgic_irq_enter(current);

  gic_guest_eoi(pirq, 1);

  vgic_inject_virq(current, pirq, pirq, 1);

  return 0;
}
