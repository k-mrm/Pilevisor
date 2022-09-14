#include "msg.h"
#include "sub-msg.h"
#include "log.h"
#include "node.h"
#include "cluster.h"

static void init_ack_reply(u8 *node0_mac, int nvcpu, u64 allocated) {
  vmm_log("send init ack\n");
  struct msg msg;
  msg.type = MSG_INIT_REPLY;
  msg.dst_mac = node0_mac;
  struct __init_ack ack;
  ack.nvcpu = nvcpu;
  ack.allocated = allocated;

  struct packet pk;
  packet_init(&pk, &ack, sizeof(ack));
  msg.pk = &pk;

  send_msg(&msg);
}

void send_setup_done_notify(u8 status) {
  struct msg msg;
  msg.type = MSG_SETUP_DONE;
  msg.dst_mac = node_macaddr(0);

  struct setup_done_notify s;
  s.status = status;
  struct packet pk;
  packet_init(&pk, &s, sizeof(s));

  msg.pk = &pk;
  send_msg(&msg);
}

static void recv_init_request_intr(struct recv_msg *recvmsg) {
  u8 *node0_mac = recvmsg->src_mac;
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localnode.nvcpu, localnode.nalloc);

  init_ack_reply(node0_mac, localnode.nvcpu, localnode.nalloc);
}

void recv_cluster_info_intr(struct recv_msg *recvmsg) {
  struct cluster_info_msg *c = (struct cluster_info_msg *)recvmsg->body;

  update_cluster_info(c->nnodes, c->cluster_info);
}

void sub_msg_init() {
  msg_register_recv_handler(MSG_INIT, recv_init_request_intr);
  msg_register_recv_handler(MSG_CLUSTER_INFO, recv_cluster_info_intr);
}
