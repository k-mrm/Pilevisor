#ifndef NET_H
#define NET_H

#include "types.h"

struct nic;
struct nic_ops {
  void (*xmit)(struct nic *, void **, int *, int);
  void (*set_recv_intr_callback)(struct nic *, void (*cb)(struct nic *, void **, int *, int));
  // private
  void (*recv_intr_callback)(struct nic *, void **, int *, int);
};

struct nic {
  char *name;
  u8 mac[6];
  int irq;
  void *device;
  struct nic_ops *ops;
};

void net_init(char *name, u8 *mac, int irq, void *dev, struct nic_ops *ops);

#endif
