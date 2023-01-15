#ifndef DRIVER_ETHERNET_H
#define DRIVER_ETHERNET_H

#include "types.h"
#include "net.h"
#include "compiler.h"

/* Ethernet II header */
struct etherheader {
  u8 dst[6];
  u8 src[6];
  u16 type;
} __packed;

void ethernet_recv_intr(struct nic *nic, struct iobuf *iobuf);

#define ETHER_PACKET_LENGTH_MIN    64

extern u8 bcast_mac[6];

#endif
