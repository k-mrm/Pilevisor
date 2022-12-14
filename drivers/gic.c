#include "types.h"
#include "vcpu.h"
#include "localnode.h"
#include "gic.h"
#include "vgic.h"
#include "irq.h"
#include "log.h"
#include "spinlock.h"
#include "device.h"
#include "panic.h"

void gic_sgi_handler(enum gic_sgi_id sgi_id) {
  switch(sgi_id) {
    case SGI_INJECT:  /* inject guest pending interrupt */
      vgic_inject_pending_irqs();
      break;
    default:
      panic("unknown sgi %d", sgi_id);
  }
}

static void gic_irqchip_check(struct gic_irqchip *irqchip) {
  int version = irqchip->version;

  if(version != 2 && version != 3)
    panic("GIC?");

  printf("irqchip: GICv%d detected\n", version);

  bool all_implemented = true;

  all_implemented &= !!(irqchip->initcore);
  all_implemented &= !!(irqchip->inject_guest_irq);
  all_implemented &= !!(irqchip->irq_pending);
  all_implemented &= !!(irqchip->host_eoi);
  all_implemented &= !!(irqchip->guest_eoi);
  all_implemented &= !!(irqchip->deactive_irq);
  all_implemented &= !!(irqchip->send_sgi);
  all_implemented &= !!(irqchip->irq_enabled);
  all_implemented &= !!(irqchip->enable_irq);
  all_implemented &= !!(irqchip->disable_irq);
  all_implemented &= !!(irqchip->setup_irq);
  all_implemented &= !!(irqchip->irq_handler);

  if(!all_implemented)
    panic("irqchip: features incomplete");
}

void irqchip_init_core() {
  localnode.irqchip->initcore();
}

void irqchip_init() {
  /* support only GICv2 or GICv3 */
  struct device_node *n;

  foreach_device_node_child(n, localnode.device_tree) {
    if(!dt_node_propb(n, "interrupt-controller"))
      continue;

    const char *comp = dt_node_props(n, "compatible");
    if(!comp)
      continue;

    printf("intc: comp %s\n", comp);

    int rc = compat_dt_device_init(__dt_irqchip_device, n, comp);
    if(rc == 0)
      break;
  }

  if(!n)
    panic("no irqchip");

  if(!localnode.irqchip)
    panic("no irqchip");

  gic_irqchip_check(localnode.irqchip);
}
