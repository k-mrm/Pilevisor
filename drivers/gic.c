#include "types.h"
#include "vcpu.h"
#include "localnode.h"
#include "gic.h"
#include "irq.h"
#include "log.h"
#include "spinlock.h"
#include "panic.h"

static void gic_inject_pending_irqs() {
  struct vcpu *vcpu = current;
  u64 flags;

  spin_lock_irqsave(&vcpu->pending.lock, flags);

  int head = vcpu->pending.head;

  while(head != vcpu->pending.tail) {
    struct gic_pending_irq *pendirq = vcpu->pending.irqs[head];
    if(localnode.irqchip->inject_guest_irq(pendirq) < 0)
      panic("inject pending irqs");

    head = (head + 1) % 4;

    free(pendirq);
  }

  vcpu->pending.head = head;

  spin_unlock_irqrestore(&vcpu->pending.lock, flags);

  dsb(ish);
}

static void gic_sgi_handler(enum gic_sgi sgi_id) {
  switch(sgi_id) {
    case SGI_INJECT:  /* inject guest pending interrupt */
      gic_inject_pending_irqs();
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

  all_implemented &= !!(irqchip->init);
  all_implemented &= !!(irqchip->initcore);
  all_implemented &= !!(irqchip->inject_guest_irq);
  all_implemented &= !!(irqchip->irq_pending);
  all_implemented &= !!(irqchip->read_iar);
  all_implemented &= !!(irqchip->host_eoi);
  all_implemented &= !!(irqchip->guest_eoi);
  all_implemented &= !!(irqchip->deactive_irq);
  all_implemented &= !!(irqchip->send_sgi);
  all_implemented &= !!(irqchip->irq_enabled);
  all_implemented &= !!(irqchip->enable_irq);
  all_implemented &= !!(irqchip->disable_irq);

  if(!all_implemented)
    panic("irqchip: features incomplete");
}

void gic_irq_handler(int from_guest) {
  while(1) {
    u32 iar = localnode.irqchip->read_iar();
    u32 pirq = iar & 0x3ff;

    if(pirq == 1023)    /* spurious interrupt */
      break;

    if(is_ppi_spi(pirq)) {
      isb();

      int handled = handle_irq(pirq);

      if(handled)
        localnode.irqchip->host_eoi(pirq);
    } else if(is_sgi(pirq)) {
      gic_sgi_handler(pirq);

      localnode.irqchip->host_eoi(pirq);
    } else {
      panic("???????");
    }
  }
}

void gic_init_cpu() {
  localnode.irqchip->initcore();
}

void gic_init() {
  if(!localnode.irqchip)
    panic("no irqchip");

  gic_irqchip_check(localnode.irqchip);

  localnode.irqchip->init();
}
