#ifndef MSG_H
#define MSG_H

#include "types.h"
#include "net.h"

struct etherframe;

enum msgtype {
  MSG_NONE          = 0x0,
  MSG_INIT          = 0x1,
  MSG_INIT_REPLY    = 0x2,
  MSG_SETUP_DONE    = 0x3,
  MSG_CPU_WAKEUP    = 0x4,
  MSG_SHUTDOWN      = 0x5,
  MSG_READ          = 0x6,
  MSG_READ_REPLY    = 0x7,
  MSG_INVALID_SNOOP = 0x8,
  MSG_INTERRUPT     = 0x9,
  NUM_MSG,
};

struct recv_msg {
  enum msgtype type;
  u8 *src_mac;
  void *body;
  u32 len;
};

struct msg {
  enum msgtype type;
  u8 *dst_mac;
  struct packet *pk;
};

void send_msg(struct msg *msg);

void msg_recv_intr(struct etherframe *eth, u64 len);
void msg_register_recv_handler(enum msgtype type, void (*handler)(struct recv_msg *));

void msg_sysinit(void);

#endif
