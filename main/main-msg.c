#include "msg.h"
#include "main-msg.h"

void node0_msg_recv_intr(struct recv_msg *msg) {
  switch(msg->type) {
    case MSG_INIT:
      recv_init_request(&localnode, msg);
      break;
    case MSG_WAKEUP:
      panic("msg-wakeup");
    case MSG_SHUTDOWN:
      panic("msg-shutdown");
    case MSG_READ:
      recv_read_request(&localnode, msg);
      break;
    case MSG_INVALID_SNOOP:
      recv_read_reply(&localnode, msg);
      break;
    case MSG_INTERRUPT:
      panic("msg-interrupt");
    default:
      panic("unknown message");
  }
}
