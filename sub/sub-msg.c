#include "msg.h"
#include "sub-msg.h"
#include "log.h"
#include "node.h"

static void sub_recv_init_request_intr(struct recv_msg *recvmsg) {
  struct __init_req *body = (struct __init_req *)recvmsg->body;

  u8 *node0_mac = body->me_mac;
  sub_register_node0(node0_mac);
  vmm_log("node 0 mac address: %m\n", localnode.remotes[0].mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localnode.nvcpu, localnode.nalloc);

  /* send reply */
  struct init_reply reply;
  init_reply_init(&reply, localnode.nic->mac, localnode.nvcpu, localnode.nalloc);
  msg_send(reply);
}

void sub_msg_init() {
  msg_register_recv_handler(MSG_INIT, sub_recv_init_request_intr);
}
