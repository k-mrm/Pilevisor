#ifndef VMMIO_H
#define VMMIO_H

#include "types.h"
#include "msg.h"

/*
 *  MMIO forward request
 *
 */
struct mmio_req {
  POCV2_MSG_BODY_HEADER;
  bool wr;
  u64 addr;
  u64 val;    /* use if wr == 1 */
  enum maccsize accsz;
};

struct mmio_read_reply {
  POCV2_MSG_BODY_HEADER;
  u64 addr;
  u64 val;
};

struct mmio_write_reply {
  POCV2_MSG_BODY_HEADER;
  u64 addr;
};

#endif
