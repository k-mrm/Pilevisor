#ifndef VARM_POC_MEMORY_H
#define VARM_POC_MEMORY_H

#include "types.h"

enum maccsize {
  ACC_BYTE = 1 << 0,
  ACC_HALFWORD = 1 << 1,
  ACC_WORD = 1 << 2,
  ACC_DOUBLEWORD = 1 << 3,
};

struct memrange {
  u64 start;
  u64 size;
};

#endif
