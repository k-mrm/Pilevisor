#ifndef MVMM_MMIO_H
#define MVMM_MMIO_H

#include "types.h"
#include "vcpu.h"
#include "memory.h"

struct mmio_access {
  u64 ipa;
  u64 offset;
  u64 val;
  enum maccsize accsize;
  bool wnr;
};

struct mmio_region {
  struct mmio_region *next;
  u64 base;
  u64 size;
  int (*read)(struct vcpu *, struct mmio_access *);
  int (*write)(struct vcpu *, struct mmio_access *);
};

int mmio_emulate(struct vcpu *vcpu, struct mmio_access *mmio);

int mmio_reg_handler(u64 ipa, u64 size,
                     int (*read_handler)(struct vcpu *, struct mmio_access *),
                     int (*write_handler)(struct vcpu *, struct mmio_access *));

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
