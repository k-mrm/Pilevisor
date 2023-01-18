#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

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

static inline bool in_memrange(struct memrange *mem, u64 addr) {
  return mem->start <= addr && addr < mem->start + mem->size;
}

#endif
