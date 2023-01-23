#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

#include "types.h"

#define NBLOCK_MAX    16

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

struct memblock {
  u64 phys_start;
  u64 size;

  u64 virt_start;
};

struct system_memory {
  struct memblock slots[NBLOCK_MAX];
  int nslot;
  u64 allsize;
};

static inline bool in_memrange(struct memrange *mem, u64 addr) {
  return mem->start <= addr && addr < mem->start + mem->size;
}

extern struct system_memory system_memory;

static inline void system_memory_reg(u64 start, u64 size) {
  struct memblock *mem = &system_memory.slots[system_memory.nslot++];

  mem->phys_start = start;
  mem->size = size;

  system_memory.allsize += size;
}

static inline u64 system_memory_base() {
  return system_memory.slots[0].phys_start;
}

static inline u64 system_memory_end() {
  int idx = system_memory.nslot;
  struct memblock *mem = &system_memory.slots[idx - 1];

  return mem->phys_start + mem->size;
}

#endif
