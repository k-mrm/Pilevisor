#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "log.h"
#include "msg.h"

u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ether_packet(u8 *dst_mac, u8 *src_mac, u16 type) {
  ;
}

static void *eth_parse(void *packet, u8 **src, u8 **dst, u16 *ethtype) {
  struct etherheader *eth = (struct etherheader *)packet;

  *src = eth->src;
  *dst = eth->dst;
  *ethtype = eth->type;

  return (void *)((u8 *)packet + sizeof(struct etherheader));
}

void ethernet_recv_intr(struct nic *nic, void **packets, int *lens, int npackets) {
  u8 *src;
  u8 *dst;
  u16 ethtype;
  packets[0] = eth_parse(packets[0], &src, &dst, &ethtype);

  // printf("ether: recv intr from %m %p\n", src, ethtype);

  if(memcmp(dst, broadcast_mac, 6) == 0 || memcmp(dst, nic->mac, 6) == 0) {
    // if(ethtype == POCV2_MSG_ETH_PROTO)
    if((ethtype & 0xff) == 0x19)
      msg_recv_intr(src, packets, lens, npackets);
  }
}
