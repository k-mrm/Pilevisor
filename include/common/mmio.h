#ifndef MVMM_MMIO_H
#define MVMM_MMIO_H

#include "types.h"
#include "vcpu.h"

enum mmio_accsize {
  ACC_BYTE = 1<<0,
  ACC_HALFWORD = 1<<1,
  ACC_WORD = 1<<2,
  ACC_DOUBLEWORD = 1<<3,
};

struct mmio_access {
  u64 ipa;
  u64 pc;
  enum mmio_accsize accsize;
  int wnr: 1;
};

struct mmio_info {
  struct mmio_info *next;
  u64 base;
  u64 size;
  int (*read)(struct vcpu *, u64, u64 *, struct mmio_access *);
  int (*write)(struct vcpu *, u64, u64, struct mmio_access *);
};

int mmio_emulate(struct vcpu *vcpu, int rn, struct mmio_access *mmio);
int mmio_reg_handler(struct vm *vm, u64 ipa, u64 size,
                     int (*read)(struct vcpu *, u64, u64 *, struct mmio_access *),
                     int (*write)(struct vcpu *, u64, u64, struct mmio_access *));

#endif
