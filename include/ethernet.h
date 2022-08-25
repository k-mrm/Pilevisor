#ifndef DRIVER_ETHERNET_H
#define DRIVER_ETHERNET_H

#include "types.h"

/* Ethernet II */
struct etherframe {
  u8 src[6];
  u8 dst[6];
  u16 type;
  u8 payload[0];
};

int ethernet_recv_intr(struct nic *nic, struct etherframe *eth, u64 len);
int ethernet_xmit(struct nic *nic, u8 *dst_mac, u16 type, u8 *payload);

#endif
