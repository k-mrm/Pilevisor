#ifndef VIRTIO_VIRTQ_H
#define VIRTIO_VIRTQ_H

#include "types.h"

/* virtqueue */
#define NQUEUE  8

#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2 
#define VIRTQ_DESC_F_INDIRECT 4
struct virtq_desc {
  u64 addr;
  u32 len;
  u16 flags;
  u16 next;
} __attribute__((packed, aligned(16)));

#define VIRTQ_AVAIL_F_NO_INTERRUPT  1
struct virtq_avail {
  u16 flags;
  u16 idx;
  u16 ring[NQUEUE];
} __attribute__((packed, aligned(2)));

struct virtq_used_elem {
  u32 id;
  u32 len;
} __attribute__((packed));

#define VIRTQ_USED_F_NO_NOTIFY  1
struct virtq_used {
  u16 flags;
  u16 idx;
  struct virtq_used_elem ring[NQUEUE];
} __attribute__((packed, aligned(4)));

struct virtq {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
  u16 free_head;
  u16 nfree;
  u16 last_used_idx;
};

/* device status */
#define DEV_STATUS_ACKNOWLEDGE  1
#define DEV_STATUS_DRIVER       2
#define DEV_STATUS_FAILED       128
#define DEV_STATUS_FEATURES_OK  8
#define DEV_STATUS_DRIVER_OK    4
#define DEV_STATUS_NEEDS_RESET  64

#define VIRTIO_VERSION    0x2

#define VIRTIO_DEV_NET    0x1
#define VIRTIO_DEV_BLK    0x2

int virtq_reg_to_dev(void *base, struct virtq *vq, int qsel);
int alloc_desc(struct virtq *vq);
void free_desc(struct virtq *vq, u16 n);
void virtq_init(struct virtq *vq);

#endif
