#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

#include "types.h"

#define MAX_CHUNK   16

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

struct system_memory {
  struct memrange slot[MAX_CHUNK];
  int nslot;
  u64 allsize;
};

static inline bool in_memrange(struct memrange *mem, u64 addr) {
  return mem->start <= addr && addr < mem->start + mem->size;
}

extern struct system_memory system_memory;

static inline void system_memory_reg(u64 start, u64 size) {
  struct memrange *mem = &system_memory.slot[system_memory.nslot++];

  mem->start = start;
  mem->size = size;

  system_memory.allsize += size;
}

#endif
