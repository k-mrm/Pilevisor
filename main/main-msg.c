#include "msg.h"
#include "main-msg.h"
#include "node.h"
#include "ethernet.h"

static void node0_recv_init_reply_intr(struct recv_msg *recvmsg) {
  struct __init_reply *body = (struct __init_reply *)recvmsg->body;

  node0_register_remote(recvmsg->src_mac);
  vmm_log("Node 1: %d vcpus %p bytes\n", body->nvcpu, body->allocated);
}

static void node0_recv_sub_setup_done_notify_intr(struct recv_msg *recvmsg) {
  struct setup_done_notify *body = (struct setup_done_notify *)recvmsg->body;
  int nodeid = macaddr_to_nodeid(recvmsg->src_mac);

  if(body->status == 0)
    vmm_log("Node %d: setup ran successfully\n", nodeid);
  else
    vmm_log("Node %d: setup failed\n", nodeid);

  node_online(nodeid);

  vmm_log("node %d online\n", nodeid);
}

void send_init_broadcast(void) {
  printf("send_init_request");
  struct msg msg;
  msg.type = MSG_INIT;
  msg.dst_mac = broadcast_mac;
  msg.pk = NULL;

  send_msg(&msg);
}

void node0_msg_init() {
  msg_register_recv_handler(MSG_INIT_REPLY, node0_recv_init_reply_intr);
  msg_register_recv_handler(MSG_SETUP_DONE, node0_recv_sub_setup_done_notify_intr);
}
