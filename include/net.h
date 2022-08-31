#ifndef NET_H
#define NET_H

#include "types.h"

struct packet {
  int used;
  void *data;
  int len;
  struct packet *next;
};

#define foreach_packet(pk, pkhead)  \
  for((pk) = (pkhead); (pk); (pk) = (pk)->next)

struct packet *allocpacket(void);
void freepacket(struct packet *packet);

struct nic;
struct nic {
  char *name;
  u8 mac[6];
  void *device;
  int irq;

  void (*xmit)(struct nic *, u8 *, u64);
};

#endif
