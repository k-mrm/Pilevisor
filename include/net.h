#ifndef NET_H
#define NET_H

#include "types.h"

struct nic;

struct nic_ops {
  void (*xmit)(struct nic *, u8 *, u64);
};

struct nic {
  u8 mac[6];
  void *device;
  int irq;
  struct nic_ops *ops;
  char *name;
};

extern struct nic netdev;

#endif
