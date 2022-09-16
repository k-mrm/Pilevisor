#include "net.h"
#include "spinlock.h"
#include "ethernet.h"
#include "node.h"
#include "lib.h"

static struct nic netdev;

void net_init(char *name, u8 *mac, int irq, void *dev, struct nic_ops *ops) {
  if(localnode.nic)
    vmm_warn("net: already initialized");

  netdev.name = name;
  memcpy(netdev.mac, mac, 6);
  netdev.irq = irq;
  netdev.device = dev;
  netdev.ops = ops;

  netdev.ops->set_recv_intr_callback(&netdev, ethernet_recv_intr);

  localnode.nic = &netdev;

  vmm_log("found nic: %s %m @%p\n", name, mac, &netdev);
}
