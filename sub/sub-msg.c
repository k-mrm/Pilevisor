#include "msg.h"
#include "sub-msg.h"
#include "log.h"

static int sub_recv_init_request_intr(struct recv_msg *recvmsg) {
  struct __init_req *body = (struct __init_req *)recvmsg->body;

  u8 *node0_mac = body->me_mac;
  sub_register_node0(node0_mac);
  vmm_log("node 0 mac address: %m\n", localnode.remotes[0].mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);

  /* send reply */
  struct init_reply reply;
  init_reply_init(&reply, localnode.nic->mac, 128 * 1024 * 1024);
  msg_send(reply);
  
  return 0;
}

void sub_msg_recv_intr(struct recv_msg *recvmsg) {
  switch(recvmsg->type) {
    case MSG_INIT:
      sub_recv_init_request_intr(recvmsg);
      break;
    case MSG_WAKEUP:
      panic("msg-wakeup");
    case MSG_SHUTDOWN:
      panic("msg-shutdown");
    case MSG_READ:
      recv_read_request_intr(recvmsg);
      break;
    case MSG_INVALID_SNOOP:
      panic("msg-invalid-snoop");
      break;
    case MSG_INTERRUPT:
      panic("msg-interrupt");
    default:
      panic("unknown message");
  }
}
