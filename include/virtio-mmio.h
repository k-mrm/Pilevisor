#ifndef MVMM_VIRTIO_MMIO_H
#define MVMM_VIRTIO_MMIO_H

#include "types.h"
#include "spinlock.h"

struct vm;

#define VIRTIO_MMIO_MAGIC_VALUE		0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION		0x004 // version; 1 is legacy
#define VIRTIO_MMIO_DEVICE_ID		0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID		0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028 // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL		0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM		0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN		0x040 // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY		0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064 // write-only
#define VIRTIO_MMIO_STATUS		0x070 // read/write

#define NUM 8

struct virtq_desc {
  u64 addr;
  u32 len;
  u16 flags;
  u16 next;
};
#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

struct virtq_avail {
  u16 flags; // always zero
  u16 idx;   // driver will write ring[idx] next
  u16 ring[NUM]; // descriptor numbers of chain heads
  u16 unused;
};

struct virtq_used_elem {
  u32 id;   // index of start of completed descriptor chain
  u32 len;
};

struct virtq_used {
  u16 flags; // always zero
  u16 idx;   // device increments when it adds a ring[] entry
  struct virtq_used_elem ring[NUM];
};

struct virtio_blk_req {
  u32 type; // VIRTIO_BLK_T_IN or ..._OUT
  u32 reserved;
  u64 sector;
};

void virtio_mmio_init(struct vm *vm);

struct vtdev_desc {
  u64 ipa;
  u64 real_addr;
  u32 len;
  u16 next;
  bool across_page: 1;
  bool has_next: 1;
};

struct virtio_mmio_dev {
  int qnum;
  u16 last_used_idx;
  struct vtdev_desc ring[NUM];
  spinlock_t lock;
};

#endif
