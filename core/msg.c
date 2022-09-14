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

void msg_recv_intr(struct etherframe *eth, u64 len) {
  enum msgtype m = (eth->type >> 8) & 0xff;
  struct recv_msg msg;
  msg.type = m;
  msg.src_mac = eth->src;
  msg.body = eth->body;
  msg.len = len;

  if(m < NUM_MSG && localnode.ctl->msg_recv_handlers[m])
    localnode.ctl->msg_recv_handlers[m](&msg);
  else
    panic("unknown msg received: %d\n", m);
}

void msg_register_recv_handler(enum msgtype type, void (*handler)(struct recv_msg *)) {
  if(type >= NUM_MSG)
    panic("invalid msg type");
  if(!localnode.ctl)
    panic("ctlr?");

  localnode.ctl->msg_recv_handlers[type] = handler;
}

void send_msg(struct msg *msg) {
  u16 type = (msg->type << 8) | 0x19;

  ethernet_xmit(localnode.nic, msg->dst_mac, type, msg->pk);
  intr_enable();
}

void broadcast_msg_header_init(struct msg *msg, enum msgtype type) {
  msg->type = type;
  msg->dst_mac = broadcast_mac;
}

void msg_header_init(struct msg *msg, enum msgtype type, int dst_node_id) {
  msg->type = type;
  msg->dst_mac = node_macaddr(dst_node_id);
}

static void unknown_msg_recv(struct recv_msg *recvmsg) {
  panic("msg: unknown msg received: %d", recvmsg->type);
}

void msg_sysinit() {
  for(int i = 0; i < NUM_MSG; i++) {
    if(!localnode.ctl->msg_recv_handlers[i])
      localnode.ctl->msg_recv_handlers[i] = unknown_msg_recv;
  }
}
