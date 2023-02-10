#ifndef MSG_H
#define MSG_H

#include "types.h"
#include "net.h"
#include "ethernet.h"
#include "spinlock.h"
#include "compiler.h"

enum msgtype {
  MSG_NONE            = 0x0,
  MSG_INIT            = 0x1,
  MSG_INIT_ACK        = 0x2,
  MSG_CLUSTER_INFO    = 0x3,
  MSG_SETUP_DONE      = 0x4,
  MSG_CPU_WAKEUP      = 0x5,
  MSG_CPU_WAKEUP_ACK  = 0x6,
  MSG_SHUTDOWN        = 0x7,
  MSG_FETCH           = 0x8,
  MSG_FETCH_REPLY     = 0x9,
  MSG_INVALIDATE      = 0xa,
  MSG_INVALIDATE_ACK  = 0xb,
  MSG_INTERRUPT       = 0xc,
  MSG_MMIO_REQUEST    = 0xd,
  MSG_MMIO_REPLY      = 0xe,
  MSG_GIC_CONFIG      = 0xf,
  MSG_SGI             = 0x10,
  MSG_PANIC           = 0x11,
  MSG_BOOT_SIG        = 0x12,
  NUM_MSG,
};

/*
 *  pocv2-msg protocol via Ethernet (64 - 4160 byte)
 *  +-------------+---------------------------+------------------+
 *  | etherheader | src | type | conid | argv |      (body)      |
 *  +-------------+---------------------------+------------------+
 *     (14 byte)            (50 byte)           (up to 4096 byte)
 */

struct msg_header {
  u16 src_id;         /* msg src */
  u16 type;           /* enum msgtype */
  u32 connectionid;   /* lower 3 bit is cpuid */
} __aligned(8);

#define POCV2_MSG_HDR_STRUCT      struct msg_header hdr

#define ETH_POCV2_MSG_HDR_SIZE    64

struct msg {
  u16 dst_id;
  struct msg_header *hdr;   /* must be 8 byte alignment */

  void *body;
  u32 body_len;

  /* private */
  struct msg *next;       /* msg_queue */
  struct iobuf *data;     /* raw data */
};

#define M_BCAST             (1 << 0)    /* broadcast msg */

#define msg_cpu(msg)        ((msg)->hdr->connectionid & 0x7)

#define POCV2_MSG_ETH_PROTO       0x0019

#define msg_eth(msg)        ((msg)->data->eth)

struct msg_queue {
  struct msg *head;
  struct msg *tail;
  spinlock_t lock;
};

void msg_queue_init(struct msg_queue *q);

static inline bool msg_queue_empty(struct msg_queue *q) {
  return q->head == NULL;
}

struct msg_size_data {
  enum msgtype type;
  u32 msg_hdr_size;
} __aligned(16);

struct msg_handler_data {
  enum msgtype type;
  void (*recv_handler)(struct msg *);
} __aligned(16);

struct msg_data {
  enum msgtype type;
  u32 msg_hdr_size;
  void (*recv_handler)(struct msg *);
};

#define DEFINE_POCV2_MSG(ty, hdr_struct, handler)       \
  static struct msg_size_data _msdata_##ty        \
  __used __section(".rodata.msg") = {             \
    .type = (ty),                                       \
    .msg_hdr_size = sizeof(hdr_struct),                 \
  };                                                    \
  static struct msg_handler_data _mhdata_##ty     \
  __used __section(".rodata.msg.common") = {      \
    .type = (ty),                                       \
    .recv_handler = handler,                            \
  };

#define DEFINE_POCV2_MSG_RECV_NODE0(ty, hdr_struct, handler)  \
  static struct msg_size_data _msdata_##ty              \
  __used __section(".rodata.msg") = {                   \
    .type = (ty),                                             \
    .msg_hdr_size = sizeof(hdr_struct),                       \
  };                                                          \
  static struct msg_handler_data _mhdata_##ty           \
  __used __section(".rodata.msg.node0") = {             \
    .type = (ty),                                             \
    .recv_handler = handler,                                  \
  };

#define DEFINE_POCV2_MSG_RECV_SUBNODE(ty, hdr_struct, handler)  \
  static struct msg_size_data _msdata_##ty                \
  __used __section(".rodata.msg") = {                     \
    .type = (ty),                                               \
    .msg_hdr_size = sizeof(hdr_struct),                         \
  };                                                            \
  static struct msg_handler_data _mhdata_##ty             \
  __used __section(".rodata.msg.subnode") = {             \
    .type = (ty),                                               \
    .recv_handler = handler,                                    \
  };

void __send_msg(struct msg *msg, void (*reply_cb)(struct msg *, void *),
                void *cb_arg, int flags);

#define send_msg(msg)         __send_msg((msg), NULL, NULL, 0)

#define send_msg_bcast(msg)   __send_msg((msg), NULL, NULL, M_BCAST)

#define send_msg_cb(msg, cb, arg) \
  __send_msg((msg), (cb), (arg), 0)

int msg_recv_intr(u8 *src_mac, struct iobuf *buf);

#define msg_init(msg, dst_id, type, hdr, body, body_len)   \
  __msg_init(msg, dst_id, type, (struct msg_header *)hdr, body, body_len)

void __msg_init(struct msg *msg, u16 dst_id, enum msgtype type,
                struct msg_header *hdr, void *body, int body_len);

#define msg_reply(msg, type, hdr, body, body_len)   \
  __msg_reply(msg, type, (struct msg_header *)hdr, body, body_len)

void __msg_reply(struct msg *msg, enum msgtype type,
                 struct msg_header *hdr, void *body, int body_len);

void msg_sysinit(void);

struct msg *pocv2_recv_reply(struct msg *msg);
void free_recv_msg(struct msg *msg);

void do_recv_waitqueue(void);

#endif
