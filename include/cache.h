#ifndef CACHE_H
#define CACHE_H

#include "types.h"

void dcache_flush_poc(u64 va_start, u64 va_end);
void cache_sync_pou(u64 va_start, u64 va_end);

#define dcache_flush_poc_range(v, s)  __dcache_flush_poc_range((u64)(v), (s))

static inline void __dcache_flush_poc_range(u64 va, u64 size) {
  u64 end = va + size;
  dcache_flush_poc(va, end);
}

static inline void cache_sync_pou_range(void *va, u64 size) {
  void *end = va + size;
  cache_sync_pou(va, end);
}

#endif
