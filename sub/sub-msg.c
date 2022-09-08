#include "msg.h"
#include "sub-msg.h"
#include "log.h"
#include "node.h"

static void send_init_reply(int nvcpu, u64 allocated) {
  vmm_log("send_init_reply\n");
  struct msg msg;
  msg.type = MSG_INIT_REPLY;
  msg.dst_mac = remote_macaddr(0);
  struct __init_reply rep;
  rep.nvcpu = nvcpu;
  rep.allocated = allocated;

  struct packet pk;
  packet_init(&pk, &rep, sizeof(rep));
  msg.pk = &pk;

  send_msg(&msg);
}

void send_setup_done_notify(u8 status) {
  struct msg msg;
  msg.type = MSG_SETUP_DONE;
  msg.dst_mac = remote_macaddr(0);
  struct setup_done_notify s;
  s.status = status;
  struct packet pk;
  packet_init(&pk, &s, sizeof(s));
  msg.pk = &pk;

  send_msg(&msg);
}

static void sub_recv_init_request_intr(struct recv_msg *recvmsg) {
  u8 *node0_mac = recvmsg->src_mac;
  sub_register_node0(node0_mac);
  vmm_log("node 0 mac address: %m\n", localnode.remotes[0].mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localnode.nvcpu, localnode.nalloc);

  send_init_reply(localnode.nvcpu, localnode.nalloc);
}

void sub_msg_init() {
  msg_register_recv_handler(MSG_INIT, sub_recv_init_request_intr);
}
