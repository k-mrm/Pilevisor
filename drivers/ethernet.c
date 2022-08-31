#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "log.h"
#include "msg.h"

/* Ethernet II */

static u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

int ethernet_recv_intr(struct nic *nic, struct etherframe *eth, u32 len) {
  printf("eth len %d %m %x\n", len, eth->dst, eth->type);
  if(memcmp(eth->dst, broadcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19) {
      /* this is msg packet */
      msg_recv_intr(eth, len);
      return 0;
    }
  }

  return -1;
}

int ethernet_xmit(struct nic *nic, u8 *dst_mac, u16 type, struct packet *packet) {
  struct etherframe *eth = alloc_page();
  memcpy(eth->dst, dst_mac, 6);
  memcpy(eth->src, nic->mac, 6);
  eth->type = type;
  printf("ttttttttttt dst %m src %m %p %p %d\n", eth->dst, eth->src, type, packet->data, packet->len);
  /* FIXME: unnecessary copy */
  memcpy(eth->body, packet->data, packet->len);

  /* Ethernet II packet */
  u32 len = max(64, packet->len);
  nic->xmit(nic, (u8 *)eth, len);

  free_page(eth);

  return 0;
}
