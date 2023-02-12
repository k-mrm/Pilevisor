#ifndef NET_H
#define NET_H

#include "types.h"

struct nic;
struct iobuf;
struct etherheader;

struct nic_ops {
  void (*xmit)(struct nic *, struct iobuf *);
  void (*set_recv_intr_callback)(struct nic *, void (*cb)(struct nic *, void **, int *, int));
  // private
  void (*recv_intr_callback)(struct nic *, void **, int *, int);
};

struct nic {
  char *name;
  u8 mac[6];
  int mtu;
  void *device;
  struct nic_ops *ops;
};

/* network iobuf is optimized for pocv2-msg */
struct iobuf {
  void *head;
  void *data;
  void *tail;

  struct etherheader *eth;

  /* page */
  void *body;
  u32 len;
  u32 body_len;
};

struct iobuf *alloc_iobuf_headsize(u32 size, u32 headsize);

static inline struct iobuf *alloc_iobuf(u32 size) {
  return alloc_iobuf_headsize(size, 0);
}

void free_iobuf(struct iobuf *buf);
void *iobuf_push(struct iobuf *buf, u32 size);
void *iobuf_pull(struct iobuf *buf, u32 size);
void iobuf_set_len(struct iobuf *buf, u32 len);

void netdev_recv(struct iobuf *buf);
void net_init(char *name, u8 *mac, int mtu, void *dev, struct nic_ops *ops);

#endif
