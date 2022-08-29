#ifndef NET_H
#define NET_H

#include "types.h"

struct packet_body {
  struct packet_body *next;
  void *data;
  int len;
};

struct nic;
struct nic {
  char *name;
  u8 mac[6];
  void *device;
  int irq;

  void (*xmit)(struct nic *, u8 *, u64);
};

#endif
