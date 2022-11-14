#include "net.h"
#include "spinlock.h"
#include "ethernet.h"
#include "node.h"
#include "lib.h"
#include "mm.h"
#include "panic.h"
#include "allocpage.h"

static struct nic netdev;

static struct receive_buf rbufs[256];
static spinlock_t rbuf_lock;

void net_init(char *name, u8 *mac, int mtu, void *dev, struct nic_ops *ops) {
  if(localnode.nic)
    vmm_warn("net: already initialized");

  netdev.name = name;
  memcpy(netdev.mac, mac, 6);
  netdev.mtu = mtu;
  netdev.device = dev;
  netdev.ops = ops;

  localnode.nic = &netdev;

  vmm_log("found nic: %s %m @%p\n", name, mac, &netdev);
}

struct receive_buf *alloc_recvbuf(u32 size) {
  u64 flags = 0;

  spin_lock_irqsave(&rbuf_lock, flags);

  for(struct receive_buf *r = rbufs; r < &rbufs[256]; r++) {
    if(r->used == 0) {
      r->used = 1;

      r->head = r->data = alloc_page();   /* TODO: malloc(size) */
      r->body = alloc_page();

      r->len = size + PAGESIZE;

      spin_unlock_irqrestore(&rbuf_lock, flags);

      return r;
    }
  }

  spin_unlock_irqrestore(&rbuf_lock, flags);

  panic("recvbuf");
}

void free_recvbuf(struct receive_buf *buf) {
  u64 flags = 0;

  spin_lock_irqsave(&rbuf_lock, flags);

  buf->used = 0;

  spin_unlock_irqrestore(&rbuf_lock, flags);
}

void recvbuf_set_len(struct receive_buf *buf, u32 len) {
  buf->len = len;
}

void *recvbuf_pull(struct receive_buf *buf, u32 size) {
  void *old = buf->data;
  buf->data = (u8 *)buf->data + size;

  return old;
}

void netdev_recv(struct receive_buf *buf) {
  u32 len = buf->len;

  if(len < 64)
    panic("recvbuf len");

  void *p[2];
  int l[2], np = 1;

  p[0] = buf->data;
  l[0] = 64;

  if(len > 64) {
    p[1] = buf->body;
    l[1] = len - 64;
    np++;
  } else {
    free_page(buf->body);
  }

  ethernet_recv_intr(&netdev, p, l, np);
}
