#ifndef DRIVER_ETHERNET_H
#define DRIVER_ETHERNET_H

#include "types.h"
#include "net.h"

/* Ethernet II header */
struct etherheader {
  u8 dst[6];
  u8 src[6];
  u16 type;
} __attribute__((packed));

void ethernet_recv_intr(struct nic *nic, void **packets, int *lens, int npackets);

#define ETHER_PACKET_LENGTH_MIN    64

extern u8 broadcast_mac[6];

#endif
