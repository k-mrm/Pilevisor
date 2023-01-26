#include "net.h"
#include "spinlock.h"
#include "ethernet.h"
#include "node.h"
#include "lib.h"
#include "mm.h"
#include "panic.h"
#include "allocpage.h"
#include "malloc.h"
#include "assert.h"

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

  printf("found nic: %s %m @%p\n", name, mac, &netdev);
}

struct iobuf *alloc_iobuf_headsize(u32 size, u32 headsize) {
  if(size < headsize)
    return NULL;

  struct iobuf *buf = malloc(sizeof(*buf));

  buf->head = malloc(size);
  buf->data = (u8 *)buf->head + headsize;
  buf->tail = (u8 *)buf->head + size;

  buf->len = size - headsize;

  buf->body = NULL;
  buf->body_len = 0;

  return buf;
}

void free_iobuf(struct iobuf *buf) {
  assert(buf);

  free(buf->head);
  if(buf->body)
    free_page(buf->body);
  free(buf);
}

void *iobuf_push(struct iobuf *buf, u32 size) {
  void *old = buf->data;

  void *data = (u8 *)buf->data - size;
  if(data < buf->head)
    return NULL;

  buf->data = data;
  buf->len += size;

  return data;
}

void *iobuf_pull(struct iobuf *buf, u32 size) {
  void *old = buf->data;

  void *data = (u8 *)buf->data + size;
  if(data >= buf->tail)
    return NULL;

  buf->data = data;
  buf->len -= size;

  return old;
}

void netdev_recv(struct iobuf *buf) {
  ethernet_recv_intr(&netdev, buf);
}
