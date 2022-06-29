#ifndef MVMM_VIRTIO_MMIO_H
#define MVMM_VIRTIO_MMIO_H

#include "aarch64.h"
#include "types.h"
#include "spinlock.h"
#include "virtio.h"

#define VIRTIO_MAGIC   0x74726976

#define VIRTIO_MMIO_MAGICVALUE           0x000
#define VIRTIO_MMIO_VERSION              0x004
#define VIRTIO_MMIO_DEVICE_ID            0x008
#define VIRTIO_MMIO_VENDOR_ID            0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES      0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL  0x014
#define VIRTIO_MMIO_DRIVER_FEATURES      0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL  0x024
#define VIRTIO_MMIO_QUEUE_SEL            0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX        0x034
#define VIRTIO_MMIO_QUEUE_NUM            0x038
#define VIRTIO_MMIO_QUEUE_READY          0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY         0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS     0x060
#define VIRTIO_MMIO_INTERRUPT_ACK        0x064
#define VIRTIO_MMIO_STATUS               0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW       0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH      0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW     0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH    0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW     0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH    0x0a4
#define VIRTIO_MMIO_CONFIG_GENERATION    0x0fc
#define VIRTIO_MMIO_CONFIG               0x100

static inline u32 vtmmio_read(void *base, u32 off) {
  return *(volatile u32 *)((volatile char *)base + off);
}

static inline void vtmmio_write(void *base, u32 off, u32 val) {
  *(volatile u32 *)((volatile char *)base + off) = val;
  dsb(sy);
}

struct virtio_mmio_dev {
  void *base;
};

#endif
