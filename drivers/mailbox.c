#include "mailbox.h"
#include "log.h"
#include "device.h"
#include "mm.h"
#include "cache.h"
#include "lib.h"

static void *mbox_base; 

static inline u32 mbox_read32(u32 reg) {
  return *(volatile u32 *)((u64)mbox_base + reg);
}

static inline void mbox_write32(u32 reg, u32 val) {
  *(volatile u32 *)((u64)mbox_base + reg) = val;
}

static int mailbox_call(volatile u32 *mbox, enum mbox_ch ch) {
  // CleanAndInvalidateDataCacheRange(PERIPHERAL_BASE, 0x10000);

  dcache_flush_poc_range(mbox, 36);

  // 28-bit address (MSB) and 4-bit value (LSB)
  u32 r = ((u32)((u64)&mbox) & ~0xf) | (ch & 0xf);

  // Wait until we can write
  while (mbox_read32(MBOX_STATUS) & MBOX_FULL)
    ;

  // Write the address of our buffer to the mailbox with the channel appended
  mbox_write32(MBOX_WRITE, r);

  while (1) {
    // Is there a reply?
    while (mbox_read32(MBOX_STATUS) & MBOX_EMPTY)
      ;

    dcache_flush_poc_range(mbox, 36);

    // Is it a reply to our message?
    if (r == mbox_read32(MBOX_READ)) {
      return mbox[1] == MBOX_SUCCESS;  // Is it successful?
    }
  }
}

int mbox_get_mac_address(u8 *mac) {
  volatile u32 __attribute__((aligned(16))) mbox[36];

  mbox[0] = 8 * 4;         // length of the message
  mbox[1] = MBOX_REQUEST;  // this is a request message

  mbox[2] = RPI_FIRMWARE_GET_BOARD_MAC_ADDRESS;
  mbox[3] = 6;  // buffer size
  mbox[4] = 0;  // len

  mbox[5] = 0;
  mbox[6] = 0;

  mbox[7] = RPI_FIRMWARE_PROPERTY_END;

  int rc = mailbox_call(mbox, MBOX_CH_PROP);
  if(rc < 0) {
    vmm_warn("mailbox failed\n");
    return rc;
  }

  char *m = (char *)&mbox[5];
  memcpy(mac, m, 6);

  return 0;
}

static int bcm2835_mbox_init(struct device_node *dev) {
  const char *compat;
  u64 mboxbase, mboxlen;

  compat = dt_node_props(dev, "compatible");
  if(!compat)
    return -1;

  if(dt_node_prop_addr(dev, 0, &mboxbase, &mboxlen) < 0)
    return -1;

  printf("mailbox: %s %p %p\n", compat, mboxbase, mboxlen);

  mbox_base = iomap(mboxbase, mboxlen);
  if(!mbox_base) {
    vmm_warn("mbox: failed to map");
    return -1;
  }

  return 0;
}

void mailbox_init() {
  struct device_node *dev = dt_find_node_path("mailbox");
  if(!dev)
    return;

  if(bcm2835_mbox_init(dev) < 0)
    panic("mbox init fail");
}
