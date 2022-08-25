#ifndef MSG_H
#define MSG_H

#include "types.h"

#define NEED_HANDLE_IMMEDIATE   0x80

enum msgtype {
  MSG_NONE          = 0x0,
  MSG_INIT          = 0x1 | NEED_HANDLE_IMMEDIATE,
  MSG_INIT_REPLY    = 0x2,
  MSG_WAKEUP        = 0x3 | NEED_HANDLE_IMMEDIATE,
  MSG_SHUTDOWN      = 0x4 | NEED_HANDLE_IMMEDIATE,
  MSG_READ          = 0x5 | NEED_HANDLE_IMMEDIATE,
  MSG_READ_REPLY    = 0x6,
  MSG_INVALID_SNOOP = 0x7 | NEED_HANDLE_IMMEDIATE,
  MSG_INTERRUPT     = 0x8 | NEED_HANDLE_IMMEDIATE,
};

#define send_msg(msg)   (((struct msg *)&msg)->send(&msg))

struct msg {
  enum msgtype type;
  /* destination node id */
  u8 dst_bits;

  int (*send)(struct node *, struct msg *);
};

struct msg_cb {
  bool used;
  enum msgtype type;
  char window[4096];
  int wlen;
};

/*
 *  init msg: Node 0 --broadcast--> Node n(n!=0)
 *
 *  send:
 *    Node0 MAC address 
 */
struct init_msg {
  struct msg msg;
  u8 mac[6];
};

void init_msg_init(struct init_msg *msg, u8 *mac);

/*
 *  init reply: Node n ---> Node 0
 *
 *  send:
 *    Node n MAC address
 */

struct init_reply {
  struct msg msg;
  u8 mac[6];
};

void init_reply_init(struct init_reply *msg, u8 *mac);

/*
 *  read msg: Node n ---> Node n
 *
 *  send:
 *    internal physical address
 */
struct read_msg {
  struct msg msg;
  u64 ipa;
};

void read_msg_init(struct read_msg *msg, u8 dst, u64 ipa);

/*
 *  read reply: Node n ---> Node n
 *
 *  send:
 *    requested page(4KB, but now 1KB)
 */
struct read_reply {
  struct msg msg;
  u8 *page;
};

void read_reply_init(struct read_reply *msg, u8 dst, u8 *page);

struct invalid_snoop_msg {
  struct msg msg;
};

struct wakeup_msg {
  struct msg msg;
};

struct shutdown_msg {
  struct msg msg;
};

void msg_sysinit(void);

#endif
