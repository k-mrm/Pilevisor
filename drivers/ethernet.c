#include "ethernet.h"
#include "net.h"
#include "mm.h"
#include "lib.h"
#include "allocpage.h"
#include "malloc.h"
#include "log.h"
#include "msg.h"
#include "aarch64.h"

u8 bcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void ethernet_recv_intr(struct nic *nic, struct iobuf *iobuf) {
  struct etherheader *eth = iobuf_pull(iobuf, sizeof(struct etherheader));
  int need_free_body = 1;

  iobuf->eth = eth;

  vmm_log("ether: recv intr from %m %p %p\n", eth->src, eth->type, read_sysreg(elr_el2));

  if(memcmp(eth->dst, bcast_mac, 6) == 0 || memcmp(eth->dst, nic->mac, 6) == 0) {
    if((eth->type & 0xff) == 0x19) {
      msg_recv_intr(eth->src, iobuf);

      return;
    }
  }

  free_iobuf(iobuf);
}
