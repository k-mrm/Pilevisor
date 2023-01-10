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
#include "malloc.h"
#include "panic.h"
#include "assert.h"

#define USE_SCATTER_GATHER

extern struct pocv2_msg_size_data __pocv2_msg_size_data_start[];
extern struct pocv2_msg_size_data __pocv2_msg_size_data_end[];

extern struct pocv2_msg_handler_data __pocv2_msg_handler_data_start[];
extern struct pocv2_msg_handler_data __pocv2_msg_handler_data_end[];

static struct pocv2_msg_data msg_data[NUM_MSG];

static spinlock_t connectlock = SPINLOCK_INIT;

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
  [MSG_PANIC]           "msg:panic",
  [MSG_BOOT_SIG]        "msg:boot_sig",
};

/* msg ring queue */
static struct pocv2_msg_queue replyq[NUM_MSG];

static void replyq_enqueue(struct pocv2_msg *msg);

static u32 msg_hdr_size(struct pocv2_msg *msg) {
  if(msg->hdr->type < NUM_MSG)
    return msg_data[msg->hdr->type].msg_hdr_size;
  else
    panic("msg_hdr_size");
}

void pocv2_msg_queue_init(struct pocv2_msg_queue *q) {
  q->head = NULL;
  q->tail = NULL;
  spinlock_init(&q->lock);
}

void pocv2_msg_enqueue(struct pocv2_msg_queue *q, struct pocv2_msg *msg) {
  u64 flags;

  msg->next = NULL;

  spin_lock_irqsave(&q->lock, flags); 

  if(q->head == NULL)
    q->head = msg;

  if(q->tail)
    q->tail->next = msg;

  q->tail = msg;

  spin_unlock_irqrestore(&q->lock, flags); 
}

struct pocv2_msg *pocv2_msg_dequeue(struct pocv2_msg_queue *q) {
  u64 flags;

  while(pocv2_msg_queue_empty(q)) {
    ;
    // wfi();
  }

  spin_lock_irqsave(&q->lock, flags); 

  struct pocv2_msg *msg = q->head;
  q->head = q->head->next;

  if(!q->head)
    q->tail = NULL;

  spin_unlock_irqrestore(&q->lock, flags); 

  return msg;
}

void free_recv_msg(struct pocv2_msg *msg) {
  free(msg->data);
  free(msg);
}

void handle_recv_waitqueue() {
  struct pocv2_msg *m, *m_next, *head;

  struct pocv2_msg_queue *recvq = &mycpu->recv_waitq;

  if(in_lazyirq())
    panic("nest lazyirq");
  if(in_interrupt())
    panic("in interrupt?");

  assert(local_irq_disabled());

  /* prevent nest handle_recv_waitqueue() */
  lazyirq_enter();

restart:
  spin_lock(&recvq->lock); 

  head = recvq->head;

  recvq->head = NULL;
  recvq->tail = NULL;

  spin_unlock(&recvq->lock); 

  local_irq_enable();

  for(m = head; m; m = m_next) {
    enum msgtype type = m->hdr->type;
    m_next = m->next;

    if(type >= NUM_MSG)
      panic("msg %d", type);

    if(msg_data[type].recv_handler) {
      vmm_log("msg handle %p %s %p\n", m, msmap[type], m->hdr->connectionid);
      msg_data[type].recv_handler(m);

      free_recv_msg(m);
    } else {
      /* enqueue msg to replyq */
      replyq_enqueue(m);
    }
  }

  local_irq_disable();

  /*
   *  handle enqueued msg during in this function
   */
  if(!pocv2_msg_queue_empty(recvq))
    goto restart;

  lazyirq_exit();
}

int msg_recv_intr(u8 *src_mac, struct iobuf *buf) {
  struct pocv2_msg *msg = malloc(sizeof(*msg));
  int rc = 1;

  /* Packet 1 */
  struct pocv2_msg_header *hdr = buf->data;
  msg->mac = src_mac;
  msg->hdr = hdr;

  /* Packet 2 */
  if(buf->body_len != 0) {
    msg->body_len = buf->body_len;

#ifdef USE_SCATTER_GATHER
    msg->body = buf->body;
    rc = 0;
#else   /* !USE_SCATTER_GATHER */
    msg->body = alloc_page();
    memcpy(msg->body, buf->body, msg->body_len);
#endif  /* USE_SCATTER_GATHER */
  }

  msg->data = buf->head;

  vmm_log("msg recv intr %p\n", msg->hdr->connectionid);
  pocv2_msg_enqueue(&mycpu->recv_waitq, msg);

  return rc;
}

void send_msg(struct pocv2_msg *msg) {
  if(memcmp(pocv2_msg_dst_mac(msg), localnode.nic->mac, 6) == 0)
    panic("send msg to me %m %m", pocv2_msg_dst_mac(msg), cluster_me()->mac);

  vmm_log("send msg to %m %s %p\n", pocv2_msg_dst_mac(msg),
            msmap[msg->hdr->type], msg->hdr->connectionid);

  struct iobuf *buf = alloc_iobuf(64);

  struct etherheader *eth = (struct etherheader *)buf->data;
  memcpy(eth->dst, pocv2_msg_dst_mac(msg), 6);
  memcpy(eth->src, localnode.nic->mac, 6);
  eth->type = POCV2_MSG_ETH_PROTO | (msg->hdr->type << 8);

  memcpy((u8 *)buf->data + sizeof(struct etherheader), msg->hdr, msg_hdr_size(msg));

  if(msg->body) {
    buf->body = alloc_page();
    memcpy(buf->body, msg->body, msg->body_len);
    buf->body_len = msg->body_len;
  }

  localnode.nic->ops->xmit(localnode.nic, buf);
}

static void replyq_enqueue(struct pocv2_msg *msg) {
  struct pocv2_msg_queue *q = &replyq[msg->hdr->type];

  printf("enqueueueeu %p %p %p\n", q, msg, msg->next);

  pocv2_msg_enqueue(q, msg);
}

static struct pocv2_msg *replyq_dequeue(enum msgtype type) {
  struct pocv2_msg_queue *q = &replyq[type];

  struct pocv2_msg *rep = pocv2_msg_dequeue(q);

  printf("dequeueueeu %p %p %p\n", q, rep, rep->next);

  return rep;
}

struct pocv2_msg *pocv2_recv_reply(struct pocv2_msg *msg) {
  struct pocv2_msg *reply;
  enum msgtype reptype = reqrep[msg->hdr->type];
  if(reptype == 0)
    return NULL;

  printf("%d waiting recv %s........... (%p)\n", cpuid(), msmap[reptype], read_sysreg(daif));

  reply = replyq_dequeue(reptype);

  printf("recv %s(%p)!!!!!!!!!!!!!!!!!\n", msmap[reptype], reply);

  return reply;
}

static u32 new_connection() {
  static u32 conid = 0;
  u32 c;
  u64 flags;

  spin_lock_irqsave(&connectlock, flags);

  c = conid++;

  spin_unlock_irqrestore(&connectlock, flags);

  return c;
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
  hdr->connectionid = new_connection();

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

  for(struct pocv2_msg_queue *q = replyq; q < &replyq[NUM_MSG]; q++) {
    pocv2_msg_queue_init(q);
  }
}
