#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "log.h"
#include "msg.h"

u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_recv_intr(struct nic *nic, struct receive_buf *buf) {
  struct etherheader *eth = recvbuf_pull(buf, sizeof(struct etherheader));
  int body_need_free = 1;

  vmm_log("ether: recv intr from %m %p\n", eth->src, eth->type);

  if(memcmp(eth->dst, broadcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19)
      body_need_free = msg_recv_intr(eth->src, buf);
  }

  if(body_need_free)
    free_page(buf->body);
}
