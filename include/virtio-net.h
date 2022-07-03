#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include "aarch64.h"
#include "types.h"
#include "virtio.h"
#include "virtio-mmio.h"

#define VIRTIO_NET_F_CSUM       0
#define VIRTIO_NET_F_GUEST_CSUM 1
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS  2
#define VIRTIO_NET_F_MTU    3
#define VIRTIO_NET_F_MAC    5
#define VIRTIO_NET_F_GUEST_TSO4   7
#define VIRTIO_NET_F_GUEST_TSO6	  8
#define VIRTIO_NET_F_GUEST_ECN    9
#define VIRTIO_NET_F_GUEST_UFO    10
#define VIRTIO_NET_F_HOST_TSO4    11
#define VIRTIO_NET_F_HOST_TSO6    12
#define VIRTIO_NET_F_HOST_ECN     13
#define VIRTIO_NET_F_HOST_UFO     14
#define VIRTIO_NET_F_MRG_RXBUF    15
#define VIRTIO_NET_F_STATUS       16
#define VIRTIO_NET_F_CTRL_VQ      17
#define VIRTIO_NET_F_CTRL_RX      18
#define VIRTIO_NET_F_CTRL_VLAN    19 
#define VIRTIO_NET_F_CTRL_RX_EXTRA    20 
#define VIRTIO_NET_F_GUEST_ANNOUNCE   21 
#define VIRTIO_NET_F_MQ   22
#define VIRTIO_NET_F_CTRL_MAC_ADDR    23
#define VIRTIO_NET_F_STANDBY          62

struct virtio_net_config {
  u8 mac[6];
#define VIRTIO_NET_S_LINK_UP  1
#define VIRTIO_NET_S_ANNOUNCE 2
  u16 status;
  u16 max_virtqueue_pairs;
} __attribute__((packed));

struct virtio_net {
  void *base;
  struct virtio_net_config *cfg;
  int intid;
  struct virtq tx;
  struct virtq rx;
};

struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM    1    /* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID    2    /* Csum is valid */
  u8 flags;
#define VIRTIO_NET_HDR_GSO_NONE     0     /* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4    1     /* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP      3     /* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6    4     /* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN      0x80  /* TCP has ECN set */
  u8 gso_type;
  u16 hdr_len;      /* Ethernet + IP + tcp/udp hdrs */
  u16 gso_size;     /* Bytes to append to hdr_len per frame */
  u16 csum_start;   /* Position to start checksumming from */
  u16 csum_offset;  /* Offset after that to place checksum */
  u16 num_buffers;  /* Number of merged rx buffers */
};

extern struct virtio_net netdev;

#endif
