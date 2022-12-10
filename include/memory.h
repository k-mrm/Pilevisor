#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

#include "types.h"

extern char vmm_start[], vmm_end[];
extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];

#define is_vmm_text(addr)     ((u64)__text_start <= (addr) && (addr) < (u64)__text_end)
#define is_vmm_rodata(addr)   ((u64)__rodata_start <= (addr) && (addr) < (u64)__rodata_end)

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
