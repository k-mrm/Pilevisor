#ifndef MVMM_GUEST_H
#define MVMM_GUEST_H

#include "types.h"

struct guest {
  char *name;
  u64 start;
  u64 size;
};

extern struct guest virt_dtb;
extern struct guest linux_img;
extern struct guest rootfs_img;

#endif
