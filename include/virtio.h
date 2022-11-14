#ifndef VIRTIO_VIRTIO_H
#define VIRTIO_VIRTIO_H

/* device status */
#define DEV_STATUS_ACKNOWLEDGE  1
#define DEV_STATUS_DRIVER       2
#define DEV_STATUS_FAILED       128
#define DEV_STATUS_FEATURES_OK  8
#define DEV_STATUS_DRIVER_OK    4
#define DEV_STATUS_NEEDS_RESET  64

#define VIRTIO_VERSION      0x2

#define VIRTIO_DEV_NET      0x1
#define VIRTIO_DEV_BLK      0x2
#define VIRTIO_DEV_CONSOLE  0x3
#define VIRTIO_DEV_RNG      0x4

#endif
