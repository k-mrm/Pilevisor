#include "msg.h"
#include "main-msg.h"

void node0_msg_recv_intr(struct etherframe *eth, u64 len) {
  switch(type) {
    case MSG_INIT:
      recv_init_request(&localnode, buf);
      break;
    case MSG_INIT_REPLY:
      recv_init_reply(&localnode, buf);
      break;
    case MSG_READ:
      recv_read_request(&localnode, buf);
      break;
    case MSG_READ_REPLY:
      recv_read_reply(&localnode, buf);
      break;
    default:
      panic("unknown message");
  }
}
