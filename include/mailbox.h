#ifndef DRIVER_MAILBOX_H
#define DRIVER_MAILBOX_H

#include "types.h"

#define MBOX0_READ      0
#define MBOX0_PEEK      0x10
#define MBOX0_SENDER    0x14
#define MBOX0_STATUS    0x18
#define MBOX0_CFG       0x1c
#define MBOX0_WRITE     0x20

#define MBOX_EMPTY      0x40000000
#define MBOX_FULL       0x80000000

#define MBOX_SUCCESS    0x80000000

enum mbox_proptag {
  PROP_END              = 0,
  PROP_FB_ALLOC_BUFFER  = 0x40001,
  PROP_FB_GET_PITCH     = 0x40008,
  PROP_FB_SET_PHY_WH    = 0x48003,
  PROP_FB_SET_VIRT_WH   = 0x48004,
  PROP_FB_SET_DEPTH     = 0x48005,
};

u32 mbox_read(int ch);
void mbox_write(int ch, u32 data);

#endif
