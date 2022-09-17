#include "msg.h"
#include "sub-msg.h"
#include "log.h"
#include "node.h"
#include "cluster.h"

static void init_ack_reply(u8 *node0_mac, int nvcpu, u64 allocated) {
  vmm_log("send init ack\n");
  struct pocv2_msg msg;
  struct init_ack_hdr hdr;

  hdr.nvcpu = nvcpu;
  hdr.allocated = allocated;

  pocv2_msg_init(&msg, node0_mac, MSG_INIT_REPLY, &hdr, NULL, 0);

  send_msg(&msg);
}

void send_setup_done_notify(u8 status) {
  struct pocv2_msg msg;
  struct setup_done_hdr hdr;

  hdr.status = status;

  pocv2_msg_init2(&msg, 0, MSG_SETUP_DONE, &hdr, NULL, 0);

  send_msg(&msg);
}

static void recv_init_request_intr(struct pocv2_msg *msg) {
  u8 *node0_mac = pocv2_msg_src_mac(msg);
  vmm_log("node0 mac address: %m\n", node0_mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localnode.nvcpu, localnode.nalloc);

  init_ack_reply(node0_mac, localnode.nvcpu, localnode.nalloc);
}

void recv_cluster_info_intr(struct pocv2_msg *msg) {
  struct cluster_info_hdr *a = (struct cluster_info_hdr *)msg->hdr;
  struct cluster_info_body *b = msg->body;

  update_cluster_info(a->nnodes, b->cluster_info);
}

void sub_msg_init() {
  msg_register_recv_handler(MSG_INIT, recv_init_request_intr);
  msg_register_recv_handler(MSG_CLUSTER_INFO, recv_cluster_info_intr);
}
