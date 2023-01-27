#include "mailbox.h"
#include "log.h"
#include "device.h"

static void *mbox_base;

static int bcm2835_mbox_init(struct device_node *dev) {
  const char *compat;
  u64 mboxbase, mboxlen;

  compat = dt_node_props(dev, "compatible");
  if(!compat)
    return -1;

  if(dt_node_prop_addr(dev, 0, &mboxbase, &mboxlen) < 0)
    return -1;

  printf("mailbox: %s %p %p\n", compat, mboxbase, mboxlen);

  mbox_base = iomap(mboxbase, mboxlen);
  if(!mbox_base) {
    vmm_warn("mbox: failed to map");
    return -1;
  }

  return 0;
}

void mailbox_init() {
  struct device_node *dev = dt_find_node_path("mailbox");

  bcm2835_mbox_init(dev);
}
