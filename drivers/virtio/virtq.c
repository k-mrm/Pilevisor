#include "types.h"
#include "log.h"
#include "allocpage.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "panic.h"

static void desc_init(struct virtq *vq) {
  for(int i = 0; i < NQUEUE; i++) {
    if(i != NQUEUE - 1) {
      vq->desc[i].flags = VIRTQ_DESC_F_NEXT;
      vq->desc[i].next = i + 1;
    }
  }
}

#define LO(addr)  (u32)((u64)(addr) & 0xffffffff)
#define HI(addr)  (u32)(((u64)(addr) >> 32) & 0xffffffff)

/* mmio only */
int virtq_reg_to_dev(void *base, struct virtq *vq, int qsel) {
  vq->qsel = qsel;

  vtmmio_write(base, VIRTIO_MMIO_QUEUE_SEL, qsel);
  vtmmio_write(base, VIRTIO_MMIO_QUEUE_NUM, NQUEUE);
  int qmax = vtmmio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
  vmm_log("virtio: virtq max %d\n", qmax);
  if(qmax < NQUEUE)
    panic("queue?");

  vtmmio_write(base, VIRTIO_MMIO_QUEUE_DESC_LOW, LO(vq->desc));
  vtmmio_write(base, VIRTIO_MMIO_QUEUE_DESC_HIGH, HI(vq->desc));
  vtmmio_write(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW, LO(vq->avail));
  vtmmio_write(base, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, HI(vq->avail));
  vtmmio_write(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW, LO(vq->used));
  vtmmio_write(base, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, HI(vq->used));

  vtmmio_write(base, VIRTIO_MMIO_QUEUE_READY, 1);

  return 0;
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

struct virtq *virtq_create() {
  struct virtq *vq = alloc_page();

  vq->desc = alloc_page();
  vq->avail = alloc_page();
  vq->used = alloc_page();
  vmm_log("virtq d %p a %p u %p\n", vq->desc, vq->avail, vq->used);

  vq->nfree = NQUEUE;
  vq->free_head = 0;
  vq->last_used_idx = 0;
  vq->qsel = 0;

  desc_init(vq);

  return vq;
}
