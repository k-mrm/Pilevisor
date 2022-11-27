#ifndef CORE_MMIO_H
#define CORE_MMIO_H

#include "types.h"
#include "memory.h"

#define define_mmio_func(size)  \
  static inline u##size mmio_read##size(void *addr) {   \
    return *(volatile u##size *)addr;   \
  }   \
  static inline void mmio_write##size(void *addr, u##size val) {   \
    *(volatile u##size *)addr = val;    \
  }

define_mmio_func(8)
define_mmio_func(16)
define_mmio_func(32)
define_mmio_func(64)

#endif
