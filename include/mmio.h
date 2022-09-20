#ifndef MVMM_MMIO_H
#define MVMM_MMIO_H

#include "types.h"
#include "vcpu.h"
#include "memory.h"

struct mmio_access {
  u64 ipa;
  u64 offset;
  u64 pc;
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
int mmio_reg_handler(struct node *node, u64 ipa, u64 size,
                     int (*read)(struct vcpu *, struct mmio_access *),
                     int (*write)(struct vcpu *, struct mmio_access *));

#endif
