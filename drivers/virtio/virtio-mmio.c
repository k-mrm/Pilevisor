#include "types.h"
#include "log.h"
#include "memmap.h"
#include "allocpage.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"

int virtio_net_init(void *base, int intid);

int virtio_mmio_dev_init(void *base, int intid) {
  if(vtmmio_read(base, VIRTIO_MMIO_MAGICVALUE) != VIRTIO_MAGIC ||
     vtmmio_read(base, VIRTIO_MMIO_VERSION) != 2) {
    vmm_warn("no device");
    return -1;
  }

  u8 status = 0;
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status);
  status = vtmmio_read(base, VIRTIO_MMIO_STATUS) | DEV_STATUS_ACKNOWLEDGE;
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status);
  status = vtmmio_read(base, VIRTIO_MMIO_STATUS) | DEV_STATUS_DRIVER;
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status);

  switch(vtmmio_read(base, VIRTIO_MMIO_DEVICE_ID)) {
    case VIRTIO_DEV_NET:
      return virtio_net_init(base, intid);
    default:
      return -1;
  }
}

int virtio_mmio_init(void) {
  virtio_mmio_dev_init((void *)VIRTIO0, 48);
}
