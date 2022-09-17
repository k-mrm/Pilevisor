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

void ethernet_recv_intr(struct nic *nic, void **packets, int *lens, int npackets) {
  /*
  if(memcmp(eth->dst, broadcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19) {
      msg_recv_intr(packets, lens, npackets);
    }
  } */
}
