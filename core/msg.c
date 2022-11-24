/*
 *  inter node communication
 */

#include "log.h"
#include "aarch64.h"
#include "localnode.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"
#include "spinlock.h"
#include "ethernet.h"
#include "msg.h"
#include "lib.h"
#include "panic.h"

extern struct pocv2_msg_size_data __pocv2_msg_size_data_start[];
extern struct pocv2_msg_size_data __pocv2_msg_size_data_end[];

extern struct pocv2_msg_handler_data __pocv2_msg_handler_data_start[];
extern struct pocv2_msg_handler_data __pocv2_msg_handler_data_end[];

static struct pocv2_msg_data msg_data[NUM_MSG];

static enum msgtype reqrep[NUM_MSG] = {
  [MSG_INIT]          MSG_INIT_ACK,
  [MSG_CPU_WAKEUP]    MSG_CPU_WAKEUP_ACK,
  [MSG_FETCH]         MSG_FETCH_REPLY,
  [MSG_INVALIDATE]    MSG_INVALIDATE_ACK,
  [MSG_MMIO_REQUEST]  MSG_MMIO_REPLY,
};

static char *msmap[NUM_MSG] = {
  [MSG_NONE]            "msg:none",
  [MSG_INIT]            "msg:init",
  [MSG_INIT_ACK]        "msg:init_ack",
  [MSG_CLUSTER_INFO]    "msg:cluster_info",
  [MSG_SETUP_DONE]      "msg:setup_done",
  [MSG_CPU_WAKEUP]      "msg:cpu_wakeup",
  [MSG_CPU_WAKEUP_ACK]  "msg:cpu_wakeup_ack",
  [MSG_SHUTDOWN]        "msg:shutdown",
  [MSG_FETCH]           "msg:fetch",
  [MSG_FETCH_REPLY]     "msg:fetch_reply",
  [MSG_INVALIDATE]      "msg:invalidate",
  [MSG_INVALIDATE_ACK]  "msg:invalidate_ack",
  [MSG_INTERRUPT]       "msg:interrupt",
  [MSG_MMIO_REQUEST]    "msg:mmio_request",
  [MSG_MMIO_REPLY]      "msg:mmio_reply",
  [MSG_GIC_CONFIG]      "msg:gic_config",
  [MSG_SGI]             "msg:sgi",
};

/* msg ring queue */
static struct mq {
  char rq[16][64];
  int rear;
  int front;
  int nelem;
  spinlock_t lk;
} recvq[NUM_MSG];

static u32 msg_hdr_size(struct pocv2_msg *msg) {
  if(msg->hdr->type < NUM_MSG)
    return msg_data[msg->hdr->type].msg_hdr_size;
  else
    panic("msg_hdr_size");
}

static u32 msg_type_hdr_size(enum msgtype type) {
  if(type < NUM_MSG)
    return msg_data[type].msg_hdr_size;
  else
    panic("msg_hdr_size");
}

int msg_recv_intr(u8 *src_mac, struct iobuf *buf) {
  struct pocv2_msg msg;
  int rc = 1;

  /* Packet 1 */
  struct pocv2_msg_header *hdr = buf->data;
  msg.mac = src_mac;
  msg.hdr = hdr;

  /* Packet 2 */
  if(buf->body_len != 0) {
    msg.body = buf->body;
    msg.body_len = buf->body_len;
    rc = 0;
  }

  if(hdr->type < NUM_MSG && msg_data[hdr->type].recv_handler)
    msg_data[hdr->type].recv_handler(&msg);
  else
    panic("unknown msg received: %d\n", hdr->type);

  return rc;
}

void send_msg(struct pocv2_msg *msg) {
  if(memcmp(pocv2_msg_dst_mac(msg), cluster_me()->mac, 6) == 0)
    panic("send msg to me %m %m", pocv2_msg_dst_mac(msg), cluster_me()->mac);

  struct iobuf *buf = alloc_iobuf(64);

  struct etherheader *eth = (struct etherheader *)buf->data;
  memcpy(eth->dst, pocv2_msg_dst_mac(msg), 6);
  memcpy(eth->src, localnode.nic->mac, 6);
  eth->type = POCV2_MSG_ETH_PROTO | (msg->hdr->type << 8);

  memcpy((u8 *)buf->data + sizeof(struct etherheader), msg->hdr, msg_hdr_size(msg));

  buf->body = msg->body;
  buf->body_len = msg->body_len;

  localnode.nic->ops->xmit(localnode.nic, buf);
}

static inline bool mq_is_empty(struct mq *mq) {
  return mq->nelem == 0;
}

static inline bool mq_is_full(struct mq *mq) {
  return mq->nelem == 16;
}

void msgenqueue(struct pocv2_msg *msg) {
  struct mq *mq = &recvq[msg->hdr->type];

  if(mq_is_full(mq)) {
    panic("mq is full");
  }

  memcpy(mq->rq[mq->rear], msg->hdr, msg_hdr_size(msg));
  mq->rear = (mq->rear + 1) % 16;
  mq->nelem++;
}

static int msgdequeue(enum msgtype type, struct pocv2_msg_header *buf, int size) {
  struct mq *mq = &recvq[type];

  while(mq_is_empty(mq))
    return -1;

  struct pocv2_msg_header *h = (struct pocv2_msg_header *)mq->rq[mq->front];

  memcpy(buf, h, size);

  mq->front = (mq->front + 1) % 16;
  mq->nelem--;

  return 0;
}

int pocv2_recv_reply(struct pocv2_msg *msg, struct pocv2_msg_header *buf) {
  enum msgtype reptype = reqrep[msg->hdr->type];
  if(reptype == 0)
    return -1;

  int cpsize = msg_type_hdr_size(reptype);

  printf("waiting recv %s........... (%p)\n", msmap[reptype], read_sysreg(daif));
  while(msgdequeue(reptype, buf, cpsize) < 0)
    wfi();
  printf("recv %s(%p)!!!!!!!!!!!!!!!!!\n", msmap[reptype], read_sysreg(daif));

  return cpsize;
}

void _pocv2_broadcast_msg_init(struct pocv2_msg *msg, enum msgtype type,
                                struct pocv2_msg_header *hdr, void *body, int body_len) {
  _pocv2_msg_init(msg, broadcast_mac, type, hdr, body, body_len);
}

void _pocv2_msg_init2(struct pocv2_msg *msg, int dst_nodeid, enum msgtype type,
                       struct pocv2_msg_header *hdr, void *body, int body_len) {
  struct cluster_node *node = cluster_node(dst_nodeid);
  vmm_bug_on(!node, "uninit cluster");

  _pocv2_msg_init(msg, node->mac, type, hdr, body, body_len);
}

void _pocv2_msg_init(struct pocv2_msg *msg, u8 *dst_mac, enum msgtype type,
                      struct pocv2_msg_header *hdr, void *body, int body_len) {
  vmm_bug_on(!hdr, "hdr");

  hdr->src_nodeid = local_nodeid();
  hdr->type = type;

  msg->hdr = hdr;
  msg->mac = dst_mac;
  msg->body = body;
  msg->body_len = body_len;
}

void msg_sysinit() {
  struct pocv2_msg_size_data *sd;
  struct pocv2_msg_handler_data *hd;

  for(sd = __pocv2_msg_size_data_start; sd < __pocv2_msg_size_data_end; sd++) {
    printf("pocv2-msg found: %s(%d) sizeof %d\n", msmap[sd->type], sd->type, sd->msg_hdr_size);
    msg_data[sd->type].type = sd->type;
    msg_data[sd->type].msg_hdr_size = sd->msg_hdr_size;
  }

  for(hd = __pocv2_msg_handler_data_start; hd < __pocv2_msg_handler_data_end; hd++) {
    msg_data[hd->type].recv_handler = hd->recv_handler;
  }
}
