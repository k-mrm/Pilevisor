#include "types.h"
#include "guest.h"

extern char _binary_guest_linux_rootfs_img_start[];
extern char _binary_guest_linux_rootfs_img_size[];

struct guest rootfs_img = {
  .name = "initrd",
  .start = (u64)_binary_guest_linux_rootfs_img_start,
  .size = (u64)_binary_guest_linux_rootfs_img_size,
};
