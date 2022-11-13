#include "types.h"
#include "log.h"
#include "allocpage.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "virtq.h"
#include "panic.h"

int virtq_reg_to_dev(struct virtq *vq) {
  return vtmmio_set_virtq(vq->dev, vq, vq->qsel);
}

void virtq_kick(struct virtq *vq) {
  if(!(vq->used->flags & VIRTQ_USED_F_NO_NOTIFY)) {
    vtmmio_notify(vq->dev, vq->qsel);
  }
}

/* TODO: chain? */
u16 virtq_alloc_desc(struct virtq *vq) {
  if(vq->nfree == 0)
    panic("virtq kokatu");

  u16 d = vq->free_head;
  if(vq->desc[d].flags & VIRTQ_DESC_F_NEXT)
    vq->free_head = vq->desc[d].next;
  
  vq->nfree--;

  return d;
}

void virtq_free_desc(struct virtq *vq, u16 n) {
  vq->desc[n].next = vq->free_head;
  vq->desc[n].flags = VIRTQ_DESC_F_NEXT;
  vq->free_head = n;
  vq->nfree++;  /* TODO: chain? */
}

struct virtq *virtq_create(struct virtio_mmio_dev *dev, int qsel, void (*intr_handler)(struct virtq *)) {
  struct virtq *vq = alloc_page();
  if(!vq)
    panic("vq");

  vq->desc = alloc_page();
  vq->avail = alloc_page();
  vq->used = alloc_page();

  vmm_log("virtq d %p a %p u %p\n", vq->desc, vq->avail, vq->used);

  for(int i = 0; i < NQUEUE - 1; i++) {
    vq->desc[i].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[i].next = i + 1;
  }

  vq->dev = dev;
  vq->qsel = qsel;
  vq->num = vq->nfree = NQUEUE;
  vq->free_head = 0;
  vq->last_used_idx = 0;

  return vq;
}
