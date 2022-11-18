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

struct receive_buf *alloc_recvbuf(u32 size) {
  struct receive_buf *buf = malloc(sizeof(*buf));

  buf->head = buf->data = malloc(size);
  buf->body = alloc_page();

  buf->len = size + PAGESIZE;

  return buf;
}

void free_recvbuf(struct receive_buf *buf) {
  free(buf);
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
  ethernet_recv_intr(&netdev, buf);
}
