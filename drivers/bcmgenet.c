/*
 *  broadcom genet driver
 */

#include "types.h"
#include "log.h"
#include "net.h"
#include "device.h"
#include "malloc.h"
#include "printf.h"
#include "panic.h"

static void *bcmgenet_base;

static int bcmgenet_intr_stub(void *arg) {
  ;
}

static int bcmgenet_init() {
  // net_init;
  return -1;
}

static int bcmgenet_dt_init(struct device_node *dev) {
  u64 base, size; 
  int intr0, intr1;

  if(dt_node_prop_addr(dev, 0, &base, &size) < 0)
    return -1;

  bcmgenet_base = iomap(base, size);
  if(!bcmgenet_base)
    return -1;

  if(dt_node_prop_intr(dev, 0, &intr0, NULL) < 0)
    return -1;
  if(dt_node_prop_intr(dev, 1, &intr1, NULL) < 0)
    return -1;

  irq_register(intr0, bcmgenet_intr_stub, NULL);
  irq_register(intr1, bcmgenet_intr_stub, NULL);

  printf("bcmgenet: %p %p %d %d\n", base, size, intr0, intr1);

  return bcmgenet_init();
}

static const struct dt_compatible bcmgenet_compat[] = {
  { "brcm,bcm2711-genet-v5" },
  {}
};

DT_PERIPHERAL_INIT(bcmgenet, bcmgenet_compat, bcmgenet_dt_init);
