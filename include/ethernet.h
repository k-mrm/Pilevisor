#ifndef DRIVER_ETHERNET_H
#define DRIVER_ETHERNET_H

#include "types.h"
#include "net.h"

/* Ethernet II */
struct etherframe {
  u8 dst[6];
  u8 src[6];
  u16 type;
  u8 body[0];
} __attribute__((packed));

void ethernet_recv_intr(struct nic *nic, void *data, u64 len);
void ethernet_xmit(struct nic *nic, u8 *dst_mac, u16 type, struct packet *packet);

#define ETHER_PACKET_LENGTH_MIN    64

#endif
