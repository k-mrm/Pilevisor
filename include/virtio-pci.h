#ifndef MVMM_VIRTIO_PCI_H
#define MVMM_VIRTIO_PCI_H

#include "aarch64.h"
#include "types.h"
#include "pci.h"
#include "virtio.h"
#include "virtq.h"

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

struct virtio_pci_dev {
  struct pci_dev *pci;
  struct virtio_pci_common_cfg *vtcfg;
  struct virtq virtq;
  void *notify_base;
  int dev_id;
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

int virtio_pci_dev_init(struct pci_dev *pci_dev);

#endif
