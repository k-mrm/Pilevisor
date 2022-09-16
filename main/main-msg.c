#include "msg.h"
#include "main-msg.h"
#include "node.h"
#include "ethernet.h"
#include "cluster.h"

static void node0_recv_init_ack_intr(struct pocv2_msg *msg) {
  struct init_ack *i = pocv2_msg_argv(msg);

  cluster_ack_node(msg->mac, i->nvcpu, i->allocated);
  vmm_log("Node 1: %d vcpus %p bytes\n", i->nvcpu, i->allocated);
}

static void node0_recv_sub_setup_done_notify_intr(struct pocv2_msg *msg) {
  struct setup_done_notify *s = pocv2_msg_argv(msg);
  int nodeid = macaddr_to_node(recvmsg->src_mac)->nodeid;

  if(body->status == 0)
    vmm_log("Node %d: setup ran successfully\n", nodeid);
  else
    vmm_log("Node %d: setup failed\n", nodeid);

  cluster_node(nodeid)->status = NODE_ONLINE;

  vmm_log("node %d online\n", nodeid);
}

void broadcast_init_request() {
  printf("broadcast init request");
  struct msg_header head;
  msg_header_init(&head, MSG_INIT);

  send_msg(&msg);
}

void node0_msg_init() {
  msg_register_recv_handler(MSG_INIT_REPLY, node0_recv_init_ack_intr);
  msg_register_recv_handler(MSG_SETUP_DONE, node0_recv_sub_setup_done_notify_intr);
}
