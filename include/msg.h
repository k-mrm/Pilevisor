#ifndef MSG_H
#define MSG_H

#include "types.h"
#include "net.h"
#include "ethernet.h"

enum msgtype {
  MSG_NONE          = 0x0,
  MSG_INIT          = 0x1,
  MSG_INIT_REPLY    = 0x2,
  MSG_CLUSTER_INFO  = 0x3,
  MSG_SETUP_DONE    = 0x4,
  MSG_CPU_WAKEUP    = 0x5,
  MSG_SHUTDOWN      = 0x6,
  MSG_READ          = 0x7,
  MSG_READ_REPLY    = 0x8,
  MSG_INVALID_SNOOP = 0x9,
  MSG_INTERRUPT     = 0xa,
  MSG_MMIO_REQUEST  = 0xb,
  MSG_MMIO_REPLY    = 0xc,
  MSG_GIC_CONFIG    = 0xd,
  NUM_MSG,
};

/*
 *  pocv2-msg protocol (64 - 4160 byte)
 *  +-------------+-------------------+------------------+
 *  | etherheader | src | type | argv |      (body)      |
 *  +-------------+-------------------+------------------+
 *     (14 byte)        (50 byte)      (up to 4096 byte)
 *                struct pocv2_msg_header
 *      struct raw_pocv2_msg_header
 */

struct pocv2_msg_header {
  int src_nodeid;
  enum msgtype type;
  u8 argv[42];    /* definition per message */
};

build_bug_on(sizeof(struct pocv2_msg_header) != 50);

/* sizeof(pocv2_frame_header) must be 64 */
struct raw_pocv2_msg_header {
  struct etherheader eth;
  struct pocv2_msg_header msghdr;
};

build_bug_on(sizeof(struct raw_pocv2_msg_header) != 64);

struct pocv2_msg {
  u8 *mac;
  struct pocv2_msg_header *hdr;
  void *body;
  u32 body_len;
};

#define pocv2_msg_src_mac(msg)    ((msg)->mac)
#define pocv2_msg_dst_mac(msg)    ((msg)->mac)

#define pocv2_msg_argv(msg)       ((void *)&((msg)->hdr->argv))
#define pocv2_msg_src_nodeid(msg) ((msg)->hdr->src_nodeid)
#define pocv2_msg_type(msg)       ((msg)->hdr->type)

void send_msg(struct pocv2_msg *msg);

void msg_recv_intr(void **packets, int *lens, int npackets);
void msg_register_recv_handler(enum msgtype type, void (*handler)(struct pocv2_msg *));

void pocv2_broadcast_msg_init(struct pocv2_msg *msg, enum msgtype type,
                               void *argv, int argv_size, void *body, int body_len);
void pocv2_msg_init2(struct pocv2_msg *msg, int dst_nodeid, enum msgtype type,
                       void *argv, int argv_size, void *body, int body_len);
void pocv2_msg_init(struct pocv2_msg *msg, u8 *dst_mac, enum msgtype type,
                      void *argv, int argv_size, void *body, int body_len);

void msg_sysinit(void);

#endif
