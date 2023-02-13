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

extern struct msg_size_data __msg_size_data_start[];
extern struct msg_size_data __msg_size_data_end[];

extern struct msg_handler_data __msg_handler_data_start[];
extern struct msg_handler_data __msg_handler_data_end[];

static struct msg_data msg_data[NUM_MSG];

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

static inline u32 msg_hdr_size(struct msg *msg) {
  if(msg->hdr->type < NUM_MSG)
    return msg_data[msg->hdr->type].msg_hdr_size;
  else
    panic("msg_hdr_size");
}

static inline bool msg_type_is_reply(struct msg *msg) {
  switch(msg->hdr->type) {
    case MSG_CPU_WAKEUP_ACK:
    case MSG_FETCH_REPLY:
    case MSG_MMIO_REPLY:
      return true;
    default:
      return false;
  }
}

void msg_queue_init(struct msg_queue *q) {
  q->head = NULL;
  q->tail = NULL;
  spinlock_init(&q->lock);
}

static void msg_enqueue(struct msg_queue *q, struct msg *msg) {
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

static struct msg *msg_dequeue(struct msg_queue *q) {
  u64 flags;

  while(msg_queue_empty(q))
    wfi();

  spin_lock_irqsave(&q->lock, flags); 

  struct msg *msg = q->head;
  q->head = q->head->next;

  if(!q->head)
    q->tail = NULL;

  spin_unlock_irqrestore(&q->lock, flags); 

  return msg;
}

void msg_free(struct msg *msg) {
  assert(msg);

  free_iobuf(msg->data);

  free(msg);
}

static void set_reply_buf(struct msg *reply) {
  assert(!current->reply_buf);

  current->reply_buf = reply;
}

void do_recv_waitqueue() {
  struct msg *m, *m_next, *head;
  void (*handler)(struct msg *);

  struct msg_queue *recvq = &mycpu->recv_waitq;

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

    handler = msg_data[type].recv_handler;

    if(handler) {     // normal msg type
      vmm_log("msg handle %p %s %p\n", m, msmap[type], m->hdr->connectionid);
      handler(m);

      msg_free(m);
    } else {          // reply msg type
      set_reply_buf(m);
    }
  }

  local_irq_disable();

  /*
   *  handle enqueued msg during in this function
   */
  if(!msg_queue_empty(recvq))
    goto restart;

  lazyirq_exit();
}

/* called by hardware rx irq */
int msg_recv(u8 *src_mac, struct iobuf *buf) {
  struct msg *msg = malloc(sizeof(*msg));
  int rc = 0;
  u32 body_len = 0;
  void *body = NULL;

  /* Packet 1 */
  struct msg_header *hdr = buf->data;
  msg->hdr = hdr;
  msg->data = buf;

  // printf("msg recv %d %p\n", buf->len);
  // bin_dump(buf->data, 128);
  // bin_dump(buf->head, 128);

  if(buf->len > 64) {
    body = buf->data + 50;
    body_len = buf->len - 50;
  } else if(buf->body) {
    body = buf->body;
    body_len = buf->body_len;
  }

  /* Packet 2 */
  if(body) {
    msg->body_len = body_len;

    msg->body = alloc_page();
    memcpy(msg->body, body, body_len);
  }

  if(msg_type_is_reply(msg)) {
    int id = msg_cpu(msg);
    struct pcpu *cpu = get_cpu(id);

    msg_enqueue(&cpu->recv_waitq, msg);

    if(cpu != mycpu) {
      cpu_send_do_recvq_sgi(cpu);
    }
  } else {
    msg_enqueue(&mycpu->recv_waitq, msg);
  }

  return rc;
}

static u32 new_connection(int reqcpu) {
  static u32 conid = 0;
  u32 c;
  u64 flags;

  irqsave(flags);

  c = conid++;

  irqrestore(flags);

  return c << 3 | (reqcpu & 0x7);
}

static inline void __msginitcore(struct msg *msg, u16 dst_id, enum msgtype type,
                                 struct msg_header *hdr, void *body, int body_len,
                                 int cid) {
  assert(msg);
  assert(hdr);

  hdr->src_id = local_nodeid();
  hdr->type = type;
  hdr->connectionid = cid;

  msg->hdr = hdr;
  msg->dst_id = dst_id;
  msg->body = body;
  msg->body_len = body_len;
}

void __msg_init(struct msg *msg, u16 dst_id, enum msgtype type,
                struct msg_header *hdr, void *body, int body_len, int reqcpu) {
  __msginitcore(msg, dst_id, type, hdr, body, body_len, new_connection(reqcpu));
}

void __msg_reply(struct msg *msg, enum msgtype type,
                 struct msg_header *hdr, void *body, int body_len) {
  struct msg reply;

  __msginitcore(&reply, msg->hdr->src_id, type, hdr, body, body_len, msg->hdr->connectionid);

  send_msg(&reply);
}

void __send_msg(struct msg *msg, void (*reply_cb)(struct msg *, void *),
                void *cb_arg, int flags) {
  u8 *dst_mac;

  if(flags & M_BCAST) {
    dst_mac = bcast_mac;
  } else {
    dst_mac = node_macaddr(msg->dst_id);
  }

  struct iobuf *buf = alloc_iobuf_headsize(64, sizeof(struct etherheader));
  u16 type = POCV2_MSG_ETH_PROTO | (msg->hdr->type << 8);

  memcpy((u8 *)buf->data, msg->hdr, msg_hdr_size(msg));

  if(msg->body) {
    buf->body = alloc_page();
    memcpy(buf->body, msg->body, msg->body_len);
    buf->body_len = msg->body_len;
  }

  // printf("send msg %s\n", msmap[msg->hdr->type]);

  ether_send_packet(localnode.nic, dst_mac, type, buf);

  if(reply_cb) {
    struct msg *reply;
    
    while((reply = current->reply_buf) == NULL) {
      wfi();
    }
    current->reply_buf = NULL;

    reply_cb(reply, cb_arg);
    msg_free(reply);
  }
}

void msg_sysinit() {
  struct msg_size_data *sd;
  struct msg_handler_data *hd;

  for(sd = __msg_size_data_start; sd < __msg_size_data_end; sd++) {
    printf("pocv2-msg found: %s(%d) sizeof %d\n", msmap[sd->type], sd->type, sd->msg_hdr_size);
    msg_data[sd->type].type = sd->type;
    msg_data[sd->type].msg_hdr_size = sd->msg_hdr_size;
  }

  for(hd = __msg_handler_data_start; hd < __msg_handler_data_end; hd++) {
    printf("pocv2-msg func: %s(%d) %p\n", msmap[hd->type], hd->type, hd->recv_handler);
    msg_data[hd->type].recv_handler = hd->recv_handler;
  }
}
