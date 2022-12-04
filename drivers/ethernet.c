#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "malloc.h"
#include "log.h"
#include "msg.h"

u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_recv_intr(struct nic *nic, struct iobuf *iobuf) {
  struct etherheader *eth = iobuf_pull(iobuf, sizeof(struct etherheader));
  int need_free_body = 1;

  vmm_log("ether: recv intr from %m %p\n", eth->src, eth->type);

  if(memcmp(eth->dst, broadcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19) {
      need_free_body = msg_recv_intr(eth->src, iobuf);

      if(need_free_body)
        goto free_body;
      else
        return;   /* no need to free iobuf */
    }
  }

  free(iobuf->head);
  /* fall through */
free_body:
  free_page(iobuf->body);
}
