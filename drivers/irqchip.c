#include "types.h"
#include "pcpu.h"
#include "vcpu.h"
#include "localnode.h"
#include "gic.h"
#include "vgic.h"
#include "irq.h"
#include "log.h"
#include "spinlock.h"
#include "device.h"
#include "panic.h"
#include "compiler.h"

static const struct dt_device sentinel
__used __section("__dt_irqchip_sentinel") __aligned(_Alignof(struct dt_device)) = {0};

static void gic_irqchip_check(struct gic_irqchip *irqchip) {
  int version = irqchip->version;

  if(version != 2 && version != 3)
    panic("GIC?");

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
  struct dt_device *dev;

  for(n = next_match_node(__dt_irqchip_device, &dev, NULL); n;
      n = next_match_node(__dt_irqchip_device, &dev, n)) {
    if(!dt_node_propb(n, "interrupt-controller")) {
      earlycon_puts("interrupt-controller?\n");
      continue;
    }

    if(!dev) {
      earlycon_puts("dev?\n");
      continue;
    }

    if(dev->init) {
      dev->init(n);
      break;
    }
  }

  if(!n)
    panic("no compatible irqchip");

  if(!localnode.irqchip)
    panic("no irqchip");

  gic_irqchip_check(localnode.irqchip);
}
