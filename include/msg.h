#ifndef MSG_H
#define MSG_H

#include "types.h"

enum msgtype {
  MSG_NONE = 0,
  MSG_INIT,
  MSG_INIT_REPLY,
  MSG_WAKEUP,
  MSG_SHUTDOWN,
  MSG_READ,
  MSG_READ_REPLY,
  MSG_INVALID_SNOOP,
  MSG_INTERRUPT,
};

struct node;

struct msg {
  enum msgtype type;
  /* destination node id */
  u8 dst_bits;

  int (*send)(struct node *, struct msg *);
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

#endif
