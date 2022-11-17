#ifndef VMMIO_H
#define VMMIO_H

#include "types.h"
#include "msg.h"
#include "memory.h"
#include "mmio.h"


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
