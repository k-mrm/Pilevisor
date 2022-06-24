#ifndef MVMM_VIRTIO_PCI_H
#define MVMM_VIRTIO_PCI_H

#include "aarch64.h"
#include "types.h"
#include "pci.h"

struct virtio_pci_cap {
  u8 cap_vndr;
  u8 cap_next;
  u8 cap_len;
  u8 cfg_type;
  u8 bar;
  u8 padding[3];
  u32 offset;
  u32 length;
};

struct virtio_pci_cfg_cap {
  struct virtio_pci_cap cap;
  u8 pci_config_data[4];
};

struct virtio_pci_notify_cap {
  struct virtio_pci_cap cap;
  u32 notify_off_multiplier; /* Multiplier for queue_notify_off. */
};

struct virtio_pci_common_cfg {
  /* About the whole device. */
  u32 device_feature_select; /* read-write */
  u32 device_feature; /* read-only for driver */
  u32 driver_feature_select; /* read-write */
  u32 driver_feature; /* read-write */
  u16 msix_config; /* read-write */
  u16 num_queues; /* read-only for driver */
  u8 device_status; /* read-write */
  u8 config_generation; /* read-only for driver */
  /* About a specific virtqueue. */
  u16 queue_select; /* read-write */
  u16 queue_size; /* read-write */
  u16 queue_msix_vector; /* read-write */
  u16 queue_enable; /* read-write */
  u16 queue_notify_off; /* read-only for driver */
  u64 queue_desc; /* read-write */
  u64 queue_driver; /* read-write */
  u64 queue_device; /* read-write */
};

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

struct virtio_pci_dev {
  struct pci_dev *pci;
  struct virtio_pci_common_cfg *vtcfg;
  struct virtq virtq;
  void *notify_base;
  u32 notify_off_multiplier;
};

/* Common configuration */ 
#define VIRTIO_PCI_CAP_COMMON_CFG        1
/* Notifications */ 
#define VIRTIO_PCI_CAP_NOTIFY_CFG        2
/* ISR Status */ 
#define VIRTIO_PCI_CAP_ISR_CFG           3
/* Device specific configuration */ 
#define VIRTIO_PCI_CAP_DEVICE_CFG        4
/* PCI configuration access */ 
#define VIRTIO_PCI_CAP_PCI_CFG           5

/* device status */
#define DEV_STATUS_ACKNOWLEDGE  1
#define DEV_STATUS_DRIVER 2
#define DEV_STATUS_FAILED 128
#define DEV_STATUS_FEATURES_OK  8
#define DEV_STATUS_DRIVER_OK  4
#define DEV_STATUS_NEEDS_RESET  64

#endif
