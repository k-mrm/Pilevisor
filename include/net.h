#ifndef NET_H
#define NET_H

#include "types.h"

struct nic;

struct nic {
  char *name;
  u8 mac[6];
  void *device;
  int irq;

  void (*xmit)(struct nic *, u8 *, u64);
};

#endif
