#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "kalloc.h"
#include "log.h"

static u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* FIXME */
void free_etherframe(struct etherframe *eth) {
  kfree((void *)PAGEROUNDDOWN(eth));
}

int ethernet_recv_intr(struct nic *nic, struct etherframe *eth, u32 len) {
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

int ethernet_xmit(struct nic *nic, u8 *dst_mac, u16 type, u8 *body, u32 len) {
  struct etherframe *eth = kalloc();
  memcpy(eth->src, nic->mac, 6);
  memcpy(eth->dst, dst_mac, 6);
  eth->type = type;
  /* FIXME: unnecessary copy */
  memcpy(eth->body, body, len - sizeof(struct etherframe));

  printf("ether header %d byte\n", sizeof(struct etherframe));

  nic->xmit(nic, (u8 *)eth, len);

  kfree(eth);
}
