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

static inline int fls(unsigned int n) {
  return sizeof(n) * 8 - __builtin_clz(n);
}

static inline int ilog2(int n) {
  return fls(n) - 1;
}

void net_init(char *name, u8 *mac, int mtu, void *dev, struct nic_ops *ops) {
  if(localnode.nic)
    vmm_warn("net: already initialized");

  netdev.name = name;
  memcpy(netdev.mac, mac, 6);
  netdev.mtu = mtu;
  netdev.device = dev;
  netdev.ops = ops;

  localnode.nic = &netdev;

  printf("found nic: %s @%m\n", name, netdev.mac);
}

static struct iobuf *alloc_iobuf_pages(u32 size, u32 headsize) {
  int npages = size >> PAGESHIFT;
  struct iobuf *buf = malloc(sizeof(*buf));
  if(!buf)
    return NULL;

  int order = ilog2(npages);
  buf->head = alloc_pages(order);
  if(!buf->head)
    return NULL;

  buf->data = buf->head + headsize;
  buf->tail = buf->head + size;

  buf->len = size - headsize;

  buf->body = NULL;
  buf->body_len = 0;
  buf->npages = npages;

  return buf;
}

struct iobuf *alloc_iobuf_headsize(u32 size, u32 headsize) {
  if(size < headsize)
    return NULL;

  if(size >= PAGESIZE)
    return alloc_iobuf_pages(size, headsize);

  struct iobuf *buf = malloc(sizeof(*buf));
  if(!buf)
    return NULL;

  buf->head = malloc(size);
  if(!buf->head)
    return NULL;

  buf->data = (u8 *)buf->head + headsize;
  buf->tail = (u8 *)buf->head + size;

  buf->len = size - headsize;

  buf->body = NULL;
  buf->body_len = 0;
  buf->npages = 0;

  return buf;
}

void free_iobuf(struct iobuf *buf) {
  assert(buf);

  if(buf->npages)
    free_pages(buf->head, ilog2(buf->npages));
  else
    free(buf->head);

  if(buf->body)
    free_page(buf->body);

  free(buf);
}

void iobuf_set_len(struct iobuf *buf, u32 len) {
  buf->len = len;
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
