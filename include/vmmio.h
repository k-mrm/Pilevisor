#ifndef VMMIO_H
#define VMMIO_H

#include "types.h"
#include "msg.h"
#include "memory.h"

/*
 *  MMIO forward request
 *
 */
struct mmio_req_hdr {
  POCV2_MSG_HDR_STRUCT;
  bool wr;
  u64 addr;
  u64 val;    /* use if wr == 1 */
  enum maccsize accsz;
};

struct mmio_read_reply_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 addr;
  u64 val;
};

struct mmio_write_reply_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 addr;
};

#endif
