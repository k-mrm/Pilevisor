#include "msg.h"
#include "main-msg.h"

static void node0_recv_init_reply_intr(struct recv_msg *recvmsg) {
  struct __init_reply *body = (struct __init_reply *)recvmsg->body;

  node0_register_remote(body->me_mac);
  vmm_log("Node 1 is %m %p bytes\n", body->me_mac, body->allocated);
}

void node0_msg_recv_intr(struct recv_msg *recvmsg) {
  switch(recvmsg->type) {
    case MSG_INIT_REPLY:
      node0_recv_init_reply_intr(recvmsg);
      break;
    case MSG_WAKEUP:
      panic("msg-wakeup");
    case MSG_SHUTDOWN:
      panic("msg-shutdown");
    case MSG_READ:
      recv_read_request_intr(recvmsg);
      break;
    case MSG_READ_REPLY:
      recv_read_reply_intr(recvmsg);
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
