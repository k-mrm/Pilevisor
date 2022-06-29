#include "types.h"
#include "log.h"
#include "kalloc.h"
#include "lib.h"
#include "virtio.h"

static void desc_init(struct virtq *vq) {
  for(int i = 0; i < NQUEUE; i++) {
    if(i != NQUEUE - 1) {
      vq->desc[i].flags = VIRTQ_DESC_F_NEXT;
      vq->desc[i].next = i + 1;
    }
  }
}

int alloc_desc(struct virtq *vq) {
  if(vq->nfree == 0)
    panic("virtq kokatu");

  u16 d = vq->free_head;
  if(vq->desc[d].flags & VIRTQ_DESC_F_NEXT)
    vq->free_head = vq->desc[d].next;
  
  vq->nfree--;

  return d;
}

void free_desc(struct virtq *vq, u16 n) {
  u16 head = n;
  int empty = 0;

  if(vq->nfree == 0)
    empty = 1;

  while(vq->nfree++, (vq->desc[n].flags & VIRTQ_DESC_F_NEXT)) {
    n = vq->desc[n].next;
  }

  vq->desc[n].flags = VIRTQ_DESC_F_NEXT;
  if(!empty)
    vq->desc[n].next = vq->free_head;
  vq->free_head = head;
}

void virtq_init(struct virtq *vq) {
  memset(vq, 0, sizeof(*vq));

  vq->desc = kalloc();
  vq->avail = kalloc();
  vq->used = kalloc();

  vq->nfree = NQUEUE;

  desc_init(vq);
}
