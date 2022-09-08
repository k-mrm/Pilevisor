#include "msg.h"
#include "main-msg.h"
#include "node.h"

static void node0_recv_init_reply_intr(struct recv_msg *recvmsg) {
  struct __init_reply *body = (struct __init_reply *)recvmsg->body;

  node0_register_remote(body->me_mac);
  vmm_log("Node 1 is %m %p bytes\n", body->me_mac, body->allocated);
}

void node0_msg_init() {
  msg_register_recv_handler(MSG_INIT_REPLY, node0_recv_init_reply_intr);
}
