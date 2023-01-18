#ifndef VMMIO_H
#define VMMIO_H

#include "types.h"
#include "msg.h"
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

int vmmio_emulate(struct vcpu *vcpu, struct mmio_access *mmio);

int vmmio_reg_handler(u64 ipa, u64 size,
                     int (*read_handler)(struct vcpu *, struct mmio_access *),
                     int (*write_handler)(struct vcpu *, struct mmio_access *));

enum vmmio_status {
  VMMIO_OK,
  VMMIO_FAILED,
};

/*
 *  MMIO forward request
 *
 */
struct mmio_req_hdr {
  POCV2_MSG_HDR_STRUCT;
  struct mmio_access mmio;
  u32 vcpuid;
};

struct mmio_reply_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 addr;
  u64 val;
  enum vmmio_status status;
};

int vmmio_forward(u32 target_vcpuid, struct mmio_access *mmio);

#endif
