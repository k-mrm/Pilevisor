#include "net.h"
#include "spinlock.h"
#include "ethernet.h"
#include "node.h"
#include "lib.h"
#include "mm.h"
#include "panic.h"
#include "allocpage.h"
#include "malloc.h"

static struct nic netdev;

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

struct iobuf *alloc_iobuf(u32 size) {
  struct iobuf *buf = malloc(sizeof(*buf));

  buf->head = buf->data = malloc(size);
  buf->body = alloc_page();

  buf->len = size;

  return buf;
}

void free_iobuf(struct iobuf *buf) {
  free(buf->head);
  free(buf);
}

void *iobuf_push(struct iobuf *buf, u32 size) {
  void *old = buf->data;

  buf->data = (u8 *)buf->data - size;
  buf->len += size;

  return old;
}

void *iobuf_pull(struct iobuf *buf, u32 size) {
  void *old = buf->data;

  buf->data = (u8 *)buf->data + size;
  buf->len -= size;

  return old;
}

void netdev_recv(struct iobuf *buf) {
  ethernet_recv_intr(&netdev, buf);
}
