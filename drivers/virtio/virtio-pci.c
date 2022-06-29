#include "pci.h"
#include "virtio-pci.h"
#include "log.h"
#include "kalloc.h"
#include "lib.h"

static void virtio_pci_notify_queue(struct virtio_pci_dev *vdev, int queue) {
  u16 notify_off = vdev->vtcfg->queue_notify_off;
  u32 *addr = (u32 *)((u64)vdev->notify_base + notify_off * vdev->notify_off_multiplier);

  *addr = queue;
}

static void virtio_pci_rng_req(struct virtio_pci_dev *vdev, u64 *rnd) {
  struct virtq *vq = &vdev->virtq;
  *rnd = 0;

  int d = alloc_desc(vq);

  vq->desc[d].addr = (u64)rnd;
  vq->desc[d].len = sizeof(u64);
  vq->desc[d].flags = VIRTQ_DESC_F_WRITE;

  vq->avail->ring[vq->avail->idx % NQUEUE] = d;
  vq->avail->idx++;

  virtio_pci_notify_queue(vdev, 0);

  isb();

  while(*rnd == 0)
    ;

  free_desc(vq, d);
}

static int virtio_pci_rng_init(struct virtio_pci_dev *vdev) {
  vmm_log("virtio_rng_init\n");
  struct virtio_pci_common_cfg *vtcfg = vdev->vtcfg;

  vtcfg->device_status = 0;

  u8 status = DEV_STATUS_ACKNOWLEDGE;
  vtcfg->device_status = status;
  isb();

  status |= DEV_STATUS_DRIVER;
  vtcfg->device_status = status;
  isb();

  vtcfg->device_feature_select = 0;
  vtcfg->driver_feature_select = 0;

  status |= DEV_STATUS_FEATURES_OK;
  vtcfg->device_status = status;
  isb();

  virtq_init(&vdev->virtq);

  vtcfg->queue_select = 0;
  vtcfg->queue_size = 1; 

  vtcfg->queue_desc = (u64)vdev->virtq.desc;
  vtcfg->queue_driver = (u64)vdev->virtq.avail;
  vtcfg->queue_device = (u64)vdev->virtq.used;
  isb();

  vtcfg->queue_enable = 1;
  isb();

  status |= DEV_STATUS_DRIVER_OK;
  vtcfg->device_status = status;
  isb();

  if(!(vtcfg->device_status & DEV_STATUS_DRIVER_OK))
    return -1;

  return 0;
}

static void __virtio_pci_scan_cap(struct virtio_pci_dev *vdev, struct virtio_pci_cap *cap) {
  if(cap->cap_vndr != 0x9)
    vmm_warn("virtio-pci invalid vendor %p\n", cap->cap_vndr);

  struct pci_dev *pdev = vdev->pci;

  switch(cap->cfg_type) {
    case VIRTIO_PCI_CAP_COMMON_CFG: {
      u64 addr = pdev->reg_addr[cap->bar];
      struct virtio_pci_common_cfg *vtcfg = (struct virtio_pci_common_cfg *)addr;

      vdev->vtcfg = vtcfg;

      break;
    }
    case VIRTIO_PCI_CAP_NOTIFY_CFG: {
      u64 addr = pdev->reg_addr[cap->bar];
      struct virtio_pci_notify_cap *ntcap = (struct virtio_pci_notify_cap *)cap;

      vdev->notify_base = (void *)(addr + cap->offset);
      vdev->notify_off_multiplier = ntcap->notify_off_multiplier;

      break;
    }
    case VIRTIO_PCI_CAP_ISR_CFG:
    case VIRTIO_PCI_CAP_DEVICE_CFG:
    case VIRTIO_PCI_CAP_PCI_CFG:
    default:
      break;
  }

  if(cap->cap_next) {
    cap = (struct virtio_pci_cap *)((char *)(pdev->cfg) + cap->cap_next);
    __virtio_pci_scan_cap(vdev, cap);
  }
}

static void virtio_pci_scan_cap(struct virtio_pci_dev *vdev) {
  struct pci_config *cfg = vdev->pci->cfg;
  struct virtio_pci_cap *cap = (struct virtio_pci_cap *)((char *)cfg + cfg->cap_ptr);

  __virtio_pci_scan_cap(vdev, cap);
}

int virtio_pci_dev_init(struct pci_dev *pci_dev) {
  if(pci_dev->dev_id < 0x1040)
    return -1;

  struct virtio_pci_dev vdev;
  vdev.pci = pci_dev;
  virtio_pci_scan_cap(&vdev);

  switch(pci_dev->dev_id - 0x1040) {
    case 1:
      /* net */
      break;
    case 4:
      virtio_pci_rng_init(&vdev);
      break;
    default:
      return -1;
  }

  return 0;
}
