#include "types.h"
#include "log.h"
#include "allocpage.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "virtq.h"
#include "panic.h"
#include "malloc.h"
#include "memlayout.h"

int virtq_reg_to_dev(struct virtq *vq) {
  return vtmmio_set_virtq(vq->dev, vq, vq->qsel);
}

void virtq_kick(struct virtq *vq) {
  if(!(vq->used->flags & VIRTQ_USED_F_NO_NOTIFY)) {
    vtmmio_notify(vq->dev, vq->qsel);
  }
}

static void virtq_free_chain(struct virtq *vq, u16 n) {
  u16 head = n;

  while(vq->desc[n].flags & VIRTQ_DESC_F_NEXT) {
    n = vq->desc[n].next;
  }

  vq->desc[n].next = vq->free_head;
  vq->free_head = head;
}

void virtq_enqueue(struct virtq *vq, struct qlist *qs, int nqs, void *x, bool in) {
  u16 head, idx;
  struct virtq_desc *desc;

  if(!x)
    panic("enqueue: xdata");

  head = idx = vq->free_head;

  for(int i = 0; i < nqs; i++, idx = desc->next) {
    if(idx == 0xffff)
      panic("no desc");

    desc = &vq->desc[idx];

    desc->addr = V2P(qs[i].buf);
    desc->len = qs[i].len;

    desc->flags = 0;
    if(i != nqs - 1)
      desc->flags |= VIRTQ_DESC_F_NEXT;
    if(in)
      desc->flags |= VIRTQ_DESC_F_WRITE;
  }

  vq->xdata[head] = x;

  vq->free_head = idx;

  vq->avail->ring[vq->avail->idx % vq->num] = head;
  dsb(sy);
  vq->avail->idx += 1;
  dsb(sy);
}

void *virtq_dequeue(struct virtq *vq, u32 *len) {
  u16 idx = vq->last_used_idx;

  if(idx == vq->used->idx)
    return NULL;

  u32 d = vq->used->ring[idx % vq->num].id;
  if(len)
    *len = vq->used->ring[idx % vq->num].len;

  void *x = vq->xdata[d];
  vq->xdata[d] = NULL;
  if(!x)
    panic("dequeue: xdata %d", d);

  vq->last_used_idx++;

  dsb(ishst);

  virtq_free_chain(vq, d);

  return x;
}

struct virtq *virtq_create(struct virtio_mmio_dev *dev, int qsel,
                            void (*intr_handler)(struct virtq *)) {
  struct virtq *vq = alloc_page();
  if(!vq)
    panic("vq");

  vq->desc = alloc_page();
  vq->avail = alloc_page();
  vq->used = alloc_page();

  vmm_log("virtq d %p a %p u %p\n", vq->desc, vq->avail, vq->used);

  int i;
  for(i = 0; i < NQUEUE - 1; i++)
    vq->desc[i].next = i + 1;
  vq->desc[i].next = 0xffff;    /* last entry */

  vq->dev = dev;
  vq->qsel = qsel;
  vq->num = vq->nfree = NQUEUE;
  vq->free_head = 0;
  vq->last_used_idx = 0;
  vq->intr_handler = intr_handler;

  spinlock_init(&vq->lock);

  return vq;
}
