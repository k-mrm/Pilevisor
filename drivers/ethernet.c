#include "ethernet.h"
#include "net.h"
#include "mm.h"

u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* FIXME */
void free_etherframe(struct etherframe *eth) {
  kfree(PAGEROUNDDOWN(eth));
}

int ethernet_recv_intr(struct nic *nic, struct etherframe *eth, u64 len) {
  if(memcmp(eth->dst, broadcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if(((eth->type >> 8) & 0xff) == 0x19) {
      /* this is msg packet */
      msg_recv_intr(eth, len);
      return 0;
    }
  }

  free_etherframe(eth);
  return -1;
}

int ethernet_xmit(struct nic *nic, u8 *dst_mac, u16 type, u8 *payload) {
  ;
}
