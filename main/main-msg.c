#include "msg.h"
#include "main-msg.h"
#include "node.h"
#include "ethernet.h"
#include "cluster.h"

static void node0_recv_init_ack_intr(struct pocv2_msg *msg) {
  struct init_ack_hdr *i = (struct init_ack_hdr *)msg->hdr;

  cluster_ack_node(pocv2_msg_src_mac(msg), i->nvcpu, i->allocated);
  vmm_log("Node 1: %d vcpus %p bytes\n", i->nvcpu, i->allocated);
}

static void node0_recv_sub_setup_done_notify_intr(struct pocv2_msg *msg) {
  struct setup_done_hdr *s = (struct setup_done_hdr *)msg->hdr;
  int src_nodeid = pocv2_msg_src_nodeid(msg);

  if(s->status == 0)
    vmm_log("Node %d: setup ran successfully\n", src_nodeid);
  else
    vmm_log("Node %d: setup failed\n", src_nodeid);

  cluster_node(src_nodeid)->status = NODE_ONLINE;

  vmm_log("node %d online\n", src_nodeid);
}

void broadcast_init_request() {
  printf("broadcast init request");
  struct pocv2_msg msg;
  struct init_req_hdr hdr;

  pocv2_broadcast_msg_init(&msg, MSG_INIT_REQUEST, &hdr, NULL, 0);

  send_msg(&msg);
}

void node0_msg_init() {
  msg_register_recv_handler(MSG_INIT_REPLY, node0_recv_init_ack_intr);
  msg_register_recv_handler(MSG_SETUP_DONE, node0_recv_sub_setup_done_notify_intr);
}
