#ifndef CACHE_H
#define CACHE_H

#include "types.h"

void dcache_clear_poc(u64 va_start, u64 va_end);

static inline void dcache_clear_range(u64 va, u64 size) {
  u64 end = va + size;
  dcache_clear_poc(va, end);
}

#endif
