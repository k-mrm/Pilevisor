#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "log.h"
#include "msg.h"

u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_recv_intr(struct nic *nic, void *data, u64 len) {
  struct etherframe *eth = (struct etherframe *)data;

  printf("eth len %d %m %x\n", len, eth->dst, eth->type);
  if(memcmp(eth->dst, broadcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19) {
      /* this is msg packet */
      msg_recv_intr(eth, len);
    }
  }
}

void ethernet_xmit(struct nic *nic, u8 *dst_mac, u16 type, struct packet *packet) {
  struct etherframe *eth = alloc_pages(1);
  memcpy(eth->dst, dst_mac, 6);
  memcpy(eth->src, nic->mac, 6);
  eth->type = type;
  /* FIXME: unnecessary copy */
  u32 len = 0;
  struct packet *p;
  foreach_packet(p, packet) {
    memcpy((char *)eth->body + len, p->data, p->len);
    len += p->len;
  }

  len = max(ETHER_PACKET_LENGTH_MIN, len + sizeof(struct etherframe));
  printf("tttttttt dst %m src %m %p %p %d\n", eth->dst, eth->src, type, packet->data, len);
  nic->ops->xmit(nic, eth, len);

  free_pages(eth, 1);
}
