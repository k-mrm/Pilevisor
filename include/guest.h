#ifndef MVMM_GUEST_H
#define MVMM_GUEST_H

#include "types.h"

struct guest {
  char *name;
  u64 start;
  u64 size;
};

#endif
