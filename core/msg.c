#include "log.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"
#include "spinlock.h"
#include "ethernet.h"
#include "msg.h"
#include "lib.h"
#include "cluster.h"

extern struct pocv2_msg_data __pocv2_msg_data_start[], __pocv2_msg_data_end[];

static struct pocv2_msg_data msg_data[NUM_MSG];

static char *msmap[NUM_MSG] = {
  [MSG_NONE]          "msg:none",
  [MSG_INIT]          "msg:init",
  [MSG_INIT_ACK]      "msg:init_ack",
  [MSG_CLUSTER_INFO]  "msg:cluster_info",
  [MSG_SETUP_DONE]    "msg:setup_done",
  [MSG_CPU_WAKEUP]    "msg:cpu_wakeup",
  [MSG_SHUTDOWN]      "msg:shutdown",
  [MSG_READ]          "msg:read",
  [MSG_READ_REPLY]    "msg:read_reply",
  [MSG_INVALID_SNOOP] "msg:invalid_snoop",
  [MSG_INTERRUPT]     "msg:interrupt",
  [MSG_MMIO_REQUEST]  "msg:mmio_request",
  [MSG_MMIO_REPLY]    "msg:mmio_reply",
  [MSG_GIC_CONFIG]    "msg:gic_config",
};

void msg_recv_intr(u8 *src_mac, void **packets, int *lens, int npackets) {
  vmm_bug_on(npackets != 2 && npackets != 1, "npackets");
  struct pocv2_msg msg;

  /* Packet 1 */
  struct pocv2_msg_header *hdr = packets[0];
  msg.mac = src_mac;
  msg.hdr = hdr;

  /* Packet 2 */
  msg.body = npackets == 2 ? packets[1] : NULL;
  msg.body_len = npackets == 2 ? lens[1] : 0;

  if(msg.hdr->type < NUM_MSG && msg_data[msg.hdr->type].recv_handler)
    msg_data[msg.hdr->type].recv_handler(&msg);
  else
    panic("unknown msg received: %d\n", msg.hdr->type);
}

static u32 msg_hdr_size(struct pocv2_msg *msg) {
  if(msg->hdr->type < NUM_MSG)
    return msg_data[msg->hdr->type].msg_hdr_size;
  else
    panic("msg_hdr_size");
} 

void send_msg(struct pocv2_msg *msg) {
  /* build header */
  void *p[2];
  int ls[2], np;
  char header[ETH_POCV2_MSG_HDR_SIZE] = {0};

  struct etherheader *eth = (struct etherheader *)header;
  memcpy(eth->dst, pocv2_msg_dst_mac(msg), 6);
  memcpy(eth->src, localnode.nic->mac, 6);
  eth->type = POCV2_MSG_ETH_PROTO;
  memcpy(header+sizeof(struct etherheader), msg->hdr, msg_hdr_size(msg));

  p[0] = header;
  ls[0] = sizeof(header);
  np = 1;
  if(msg->body) {
    p[1] = msg->body;
    ls[1] = msg->body_len;
    np++;
  }

  localnode.nic->ops->xmit(localnode.nic, p, ls, np);

  intr_enable();
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

  hdr->src_nodeid = cluster_me_nodeid();
  hdr->type = type;

  msg->hdr = hdr;
  msg->mac = dst_mac;
  msg->body = body;
  msg->body_len = body_len;
}

static void unknown_msg_recv(struct pocv2_msg *msg) {
  panic("msg: unknown msg received: %d", pocv2_msg_type(msg));
}

void msg_sysinit() {
  struct pocv2_msg_data *d;

  for(d = __pocv2_msg_data_start; d < __pocv2_msg_data_end; d++) {
    vmm_log("pocv2-msg found: %s(%d) sizeof %d\n", msmap[d->type], d->type, d->msg_hdr_size);
    memcpy(&msg_data[d->type], d, sizeof(*d));
  }

  /* fill unused field */
  for(int i = 0; i < NUM_MSG; i++) {
    if(msg_data[i].recv_handler == NULL)
      msg_data[i].recv_handler = unknown_msg_recv;
  }
}
