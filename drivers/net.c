#include "net.h"
#include "spinlock.h"
#include "ethernet.h"

static struct nic netdev;

static struct packet packets[256];
static spinlock_t plock;

struct packet *allocpacket() {
  acquire(&plock);
  for(struct packet *p = packets; p < &packets[256]; p++) {
    if(!p->used) {
      p->used = 1;
      release(&plock);
      return p;
    }
  }
  release(&plock);
}

void freepacket(struct packet *packet) {
  acquire(&plock);
  struct packet *p_next;
  for(struct packet *p = packet; p; p = p_next) {
    p_next = p->next;
    memset(p, 0, sizeof(*p));
  }
  release(&plock);
}

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
