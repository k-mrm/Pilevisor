#include "types.h"
#include "virtio.h"
#include "virtio-mmio.h"

/*
 *  TODO: now support MMIO only
 */

int virtio_device_features_ok(void *base) {
  u32 status = vtmmio_read(base, VIRTIO_MMIO_STATUS);

  vtmmio_write(base, VIRTIO_MMIO_STATUS, status | DEV_STATUS_FEATURES_OK);

  if(!(vtmmio_read(base, VIRTIO_MMIO_STATUS) & DEV_STATUS_FEATURES_OK))
    return -1;

  return 0;
}

int virtio_device_driver_ok(void *base) {
  u32 status = vtmmio_read(base, VIRTIO_MMIO_STATUS);

  vtmmio_write(base, VIRTIO_MMIO_STATUS, status | DEV_STATUS_DRIVER_OK);

  if(!(vtmmio_read(base, VIRTIO_MMIO_STATUS) & DEV_STATUS_DRIVER_OK))
    return -1;

  return 0;
}
