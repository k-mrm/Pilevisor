#ifndef VIRTIO_VIRTQ_H
#define VIRTIO_VIRTQ_H

#include "types.h"
#include "compiler.h"

struct virtio_mmio_dev;

/* virtqueue */
#define NQUEUE  256

#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2 
#define VIRTQ_DESC_F_INDIRECT 4
struct virtq_desc {
  u64 addr;
  u32 len;
  u16 flags;
  u16 next;
} __packed __aligned(16);

#define VIRTQ_AVAIL_F_NO_INTERRUPT  1
struct virtq_avail {
  u16 flags;
  u16 idx;
  u16 ring[NQUEUE];
} __packed __aligned(2);

struct virtq_used_elem {
  u32 id;
  u32 len;
} __packed;

#define VIRTQ_USED_F_NO_NOTIFY  1
struct virtq_used {
  u16 flags;
  u16 idx;
  struct virtq_used_elem ring[NQUEUE];
} __packed __aligned(4);

struct virtq {
  struct virtio_mmio_dev *dev;

  /* vring */
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
  u16 num;

  u16 free_head;
  u16 nfree;
  u16 last_used_idx;
  int qsel;

  void (*intr_handler)(struct virtq *);

  void *xdata[NQUEUE];

  struct virtq *next;
};

struct qlist {
  void *buf;
  u32 len;
};

int virtq_reg_to_dev(struct virtq *vq);
void virtq_kick(struct virtq *vq);
void virtq_enqueue(struct virtq *vq, struct qlist *qs, int nqs, void *x, bool in);
void *virtq_dequeue(struct virtq *vq, u32 *len);
struct virtq *virtq_create(struct virtio_mmio_dev *dev, int qsel,
                            void (*intr_handler)(struct virtq *));

static inline void virtq_enqueue_in(struct virtq *vq, struct qlist *qs, int nqs, void *x) {
  return virtq_enqueue(vq, qs, nqs, x, true);
}

static inline void virtq_enqueue_out(struct virtq *vq, struct qlist *qs, int nqs, void *x) {
  return virtq_enqueue(vq, qs, nqs, x, false);
}


#endif
