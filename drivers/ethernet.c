#include "ethernet.h"
#include "aarch64.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "malloc.h"
#include "log.h"
#include "msg.h"
#include "localnode.h"

u8 bcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
u8 zeromac[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

void ether_send_packet(struct nic *nic, u8 *dst_mac, u16 type, struct iobuf *buf) {
  struct etherheader *eth = iobuf_push(buf, sizeof(struct etherheader));
  if(!eth)
    panic("eth");

  memcpy(eth->dst, dst_mac, 6);
  memcpy(eth->src, localnode.nic->mac, 6);
  eth->type = type;

  buf->eth = eth;

  nic->ops->xmit(nic, buf);
}

void ethernet_recv_intr(struct nic *nic, struct iobuf *iobuf) {
  struct etherheader *eth = iobuf_pull(iobuf, sizeof(struct etherheader));
  int need_free_body = 1;

  iobuf->eth = eth;

  vmm_log("ether: recv intr from %m %p %p\n", eth->src, eth->type, read_sysreg(elr_el2));

  if(memcmp(eth->dst, bcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19) {
      msg_recv(eth->src, iobuf);

      return;
    }
  } else if(memcmp(eth->dst, zeromac, 6) == 0) {
    panic("packet from hell!");
  }

  free_iobuf(iobuf);
}
