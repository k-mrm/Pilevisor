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

static inline void packet_init(struct packet *p, void *data, int len) {
  p->data = data;
  p->len = len;
  p->next = NULL;
}

struct packet *allocpacket(void);
void freepacket(struct packet *packet);

struct nic;
struct nic_ops {
  void (*xmit)(struct nic *, void *, u64);
  void (*set_recv_intr_callback)(struct nic *, void (*cb)(struct nic *, void *, u64));
  // private
  void (*recv_intr_callback)(struct nic *, void *, u64);
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
