#include "log.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"
#include "spinlock.h"
#include "ethernet.h"
#include "msg.h"
#include "lib.h"
#include "cluster.h"

void msg_recv_intr(void **packets, int *lens, int npackets) {
  vmm_bug_on(npackets != 2, "npackets");
  struct pocv2_msg msg;

  /* Packet 1 */
  struct raw_pocv2_msg_header *h = packets[0];
  vmm_bug_on(lens[0] != sizeof(struct raw_pocv2_msg_header), "raw_pocv2_msg_header");
  msg.hdr = &h->msghdr;

  /* Packet 2 */
  msg.body = packets[1];
  msg.len = lens[1];

  if(msg.hdr->type < NUM_MSG && localnode.ctl->msg_recv_handlers[msg.hdr->type])
    localnode.ctl->msg_recv_handlers[msg.hdr->type](&msg);
  else
    panic("unknown msg received: %d\n", msg.hdr->type);
}

void msg_register_recv_handler(enum msgtype type, void (*handler)(struct pocv2_msg *)) {
  if(type >= NUM_MSG)
    panic("invalid msg type");
  if(!localnode.ctl)
    panic("ctlr?");

  localnode.ctl->msg_recv_handlers[type] = handler;
}

void send_msg(struct pocv2_msg *msg) {
  ethernet_xmit(localnode.nic, msg->mac, type, msg->pk);
  intr_enable();
}

void pocv2_broadcast_msg_init(struct pocv2_msg *msg, enum msgtype type,
                                void *argv, int argv_size, void *body, int body_len) {
  pocv2_msg_init(msg, broadcast_mac, type, argv, argv_size, body, body_len);
}

void pocv2_msg_init2(struct pocv2_msg *msg, int dst_nodeid, enum msgtype type,
                       void *argv, int argv_size, void *body, int body_len) {
  struct cluster_node *node = cluster_node(dst_nodeid);
  vmm_bug_on(!node, "uninit cluster");

  pocv2_msg_init(msg, node->mac, type, argv, argv_size, body, body_len);
}

void pocv2_msg_init(struct pocv2_msg *msg, u8 *dst_mac, enum msgtype type,
                      void *argv, int argv_size, void *body, int body_len) {
  struct pocv2_msg_header hdr;
  hdr.src_nodeid = cluster_me_nodeid();
  hdr.type = type;
  memcpy(hdr.argv, argv, argv_size);

  msg->hdr = &hdr;
  msg->mac = dst_mac;
  msg->body = body;
  msg->body_len = body_len;
}

static void unknown_msg_recv(struct pocv2_msg *msg) {
  panic("msg: unknown msg received: %d", msg->hdr->type);
}

void msg_sysinit() {
  for(int i = 0; i < NUM_MSG; i++) {
    if(!localnode.ctl->msg_recv_handlers[i])
      localnode.ctl->msg_recv_handlers[i] = unknown_msg_recv;
  }
}
