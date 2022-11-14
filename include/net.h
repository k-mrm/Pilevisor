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
  int mtu;
  void *device;
  struct nic_ops *ops;
};

/* receive_buf is optimized for pocv2-msg */
struct receive_buf {
  int used;
  void *head;
  void *data;
  /* page */
  void *body;
  u32 len;
};

struct receive_buf *alloc_recvbuf(u32 size);
void free_recvbuf(struct receive_buf *buf);
void recvbuf_pull(struct receive_buf *buf, u32 size);
void recvbuf_set_len(struct receive_buf *buf, u32 len);

void netdev_recv(struct receive_buf *buf);

void net_init(char *name, u8 *mac, int mtu, void *dev, struct nic_ops *ops);

#endif
