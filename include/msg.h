#ifndef MSG_H
#define MSG_H

#include "types.h"
#include "node.h"

struct etherframe;
void msg_recv_intr(struct etherframe *eth, u64 len);

enum msgtype {
  MSG_NONE          = 0x0,
  MSG_INIT          = 0x1,
  MSG_INIT_REPLY    = 0x2,
  MSG_WAKEUP        = 0x3,
  MSG_SHUTDOWN      = 0x4,
  MSG_READ          = 0x5,
  MSG_READ_REPLY    = 0x6,
  MSG_INVALID_SNOOP = 0x7,
  MSG_INTERRUPT     = 0x8,
};

struct recv_msg {
  enum msgtype type;
  u8 *src_mac;
  u8 *body;
  u32 len;
  void *data;   /* ethernet frame */
};

struct msg {
  enum msgtype type;
  /* destination node id */
  u8 dst_bits;

  int (*send)(struct msg *);
};

#define msg_send(m)     (((struct msg *)&(m))->send((struct msg *)&(m)))

/*
 *  Initialize message
 *  init request: Node 0 --broadcast--> Node n(n!=0)
 *    send:
 *      Node 0's MAC address 
 *
 *  init reply:   Node n ---> Node 0
 *    send:
 *      Node n's MAC address
 *      allocated ram size to VM from Node n
 */

struct __init_req {
  u8 me_mac[6];
};

struct __init_reply {
  u8 me_mac[6];
  u64 allocated;
};

struct init_req {
  struct msg msg;
  struct __init_req body;
};

struct init_reply {
  struct msg msg;
  struct __init_reply body;
};

void init_req_init(struct init_req *req, u8 *mac);
void init_reply_init(struct init_reply *rep, u8 *mac, u64 allocated);

/*
 *  Read message
 *  read request: Node n1 ---> Node n2
 *    send
 *      - intermediate physical address(ipa)
 *
 *  read reply:   Node n1 <--- Node n2
 *    send
 *      - intermediate physical address(ipa)
 *      - 4KB page corresponding to ipa
 */

struct __read_req {
  u64 ipa;
};

struct __read_reply {
  u8 page[4096];
};

struct read_req {
  struct msg msg;
  struct __read_req body;
};

struct read_reply {
  struct msg msg;
  struct __read_reply body;
};

void read_req_init(struct read_req *rmsg, u8 dst, u64 ipa);
void read_reply_init(struct read_reply *rmsg, u8 dst, u8 *page);

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
