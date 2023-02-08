/*
 *  broadcom gigabit ethernet driver
 */

#include "types.h"
#include "log.h"
#include "net.h"
#include "device.h"
#include "malloc.h"
#include "printf.h"
#include "irq.h"
#include "panic.h"

static struct bcmgenet {
  void *base;
} bcmgenet_dev;

static void bcmgenet_xmit(struct nic *nic, struct iobuf *iobuf) {
  ;
}

static void bcmgenet_intr_irq0(void *arg) {
  // stub
}

static void bcmgenet_intr_irq1(void *arg) {
  // stub
}

static struct nic_ops bcmgenet_ops = {
  .xmit = bcmgenet_xmit,
};

static int bcmgenet_init() {
  // net_init("bcmgenet", mac, mtu, &bcmgenet_dev, &bcmgenet_ops);
  return -1;
}

static int bcmgenet_dt_init(struct device_node *dev) {
  u64 base, size; 
  int intr0, intr1;

  if(dt_node_prop_addr(dev, 0, &base, &size) < 0)
    return -1;

  bcmgenet_dev.base = iomap(base, size);
  if(!bcmgenet_dev.base)
    return -1;

  if(dt_node_prop_intr(dev, 0, &intr0, NULL) < 0)
    return -1;
  if(dt_node_prop_intr(dev, 1, &intr1, NULL) < 0)
    return -1;

  irq_register(intr0, bcmgenet_intr_irq0, NULL);
  irq_register(intr1, bcmgenet_intr_irq1, NULL);

  printf("bcmgenet: %p %p %d %d\n", base, size, intr0, intr1);

  return bcmgenet_init();
}

static const struct dt_compatible bcmgenet_compat[] = {
  { "brcm,bcm2711-genet-v5" },
  {}
};

DT_PERIPHERAL_INIT(bcmgenet, bcmgenet_compat, bcmgenet_dt_init);
