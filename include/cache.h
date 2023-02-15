#ifndef CACHE_H
#define CACHE_H

#include "types.h"
#include "aarch64.h"

void dcache_flush_poc(void *va_start, void *va_end);
void cache_sync_pou(void *va_start, void *va_end);

static inline void dcache_flush_poc_range(void *va, u64 size) {
  void *end = va + size;
  dcache_flush_poc(va, end);
}

static inline void cache_sync_pou_range(void *va, u64 size) {
  void *end = va + size;
  cache_sync_pou(va, end);
}

static inline void icache_flush_all_pou() {
  asm volatile("ic ialluis");
  dsb(ish);
}

#endif
