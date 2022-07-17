#include "msg.h"
#include "lib.h"
#include "log.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"

static u8 *msg_pack_eth_header(struct node *node, struct msg *msg, u8 *buf) {
  /* dst mac address */
  if(msg->dst_bits == 0xff) { /* broadcast */
    memset(buf, 0xff, sizeof(u8)*6);
  } else {
    u8 bits = msg->dst_bits;
    for(int i = 0; bits != 0; bits = bits >> 1, i++) {
      if(bits & 1) {
        memcpy(buf, node->remote[i].mac, sizeof(u8)*6);
      }
    }
  }

  /* src mac address */
  memcpy(buf+6, node->nic->mac, sizeof(u8)*6);
  buf[12] = 0x19;
  buf[13] = msg->type;

  return buf + 14;  /* body */
}


static int send_init_request(struct node *node, struct msg *msg) {
  struct init_msg *imsg = (struct init_msg *)msg;
  vmm_log("send_init_request!!!!!!!\n");
  /* send code */
  u8 buf[64] = {0};

  msg_pack_eth_header(node, msg, buf);
  node->nic->ops->xmit(node->nic, buf, 64);

  return 0;
}

/* node must be sub-node */
static int recv_init_request(struct node *node, u8 *buf) {
  if(node->nodeid == 0)
    panic("node0 recv init msg");

  memcpy(node->remote[0].mac, buf+6, sizeof(u8)*6);

  vmm_log("node0 mac address: %m\n", node->remote[0].mac);
  vmm_log("me mac address: %m\n", node->nic->mac);

  struct init_reply msg;
  init_reply_init(&msg, node->nic->mac);
  msg.msg.send(node, (struct msg *)&msg);

  return 0;
}

void init_msg_init(struct init_msg *msg, u8 *mac) {
  msg->msg.type = MSG_INIT;
  msg->msg.dst_bits = 0xff;   /* broadcast */
  msg->msg.send = send_init_request;
  memcpy(msg->mac, mac, sizeof(msg->mac));
}


/* node is sub-node */
static int send_init_reply(struct node *node, struct msg *msg) {
  struct init_reply *rep = (struct init_reply *)msg;
  vmm_log("send_init_reply\n");

  u8 buf[64] = {0};
  u8 *body = msg_pack_eth_header(node, msg, buf);
  memcpy(body, rep->mac, sizeof(u8)*6);

  vmm_log("dst_bits: %d", msg->dst_bits);
  node->nic->ops->xmit(node->nic, buf, 64);

  return 0;
}

/* node is node0 */
static int recv_init_reply(struct node *node, u8 *buf) {
  vmm_log("recv_init_reply\n");

  u8 remote_mac[6];
  memcpy(remote_mac, buf+6, sizeof(u8)*6);

  node->ctl->register_remote_node(node, remote_mac);

  vmm_log("sub-node's mac %m\n", remote_mac);

  return 0;
}

static void unpack_init_reply(u8 *buf, struct init_reply *rep) {
  /* TODO */
}

/* mac: Node n's MAC address */
void init_reply_init(struct init_reply *msg, u8 *mac) {
  msg->msg.type = MSG_INIT_REPLY;
  msg->msg.dst_bits = (1 << 0);   /* send to Node0 */
  msg->msg.send = send_init_reply;
  memcpy(msg->mac, mac, sizeof(msg->mac));
}


static int send_read_request(struct node *node, struct msg *msg) {
  struct read_msg *rmsg = (struct read_msg *)msg;
  vmm_log("send read request\n");

  u8 buf[64] = {0};
  u8 *body = msg_pack_eth_header(node, msg, buf);
  memcpy(body, &rmsg->ipa, sizeof(rmsg->ipa));

  node->nic->ops->xmit(node->nic, buf, 64);

  return 0;
}

static int recv_read_request(struct node *node, u8 *buf) {
  vmm_log("recv read request\n");

  u8 *body = buf + 14;

  u64 ipa, pa;
  memcpy(&ipa, body, sizeof(ipa));

  /* TODO: use at instruction */
  pa = ipa2pa(node->vttbr, ipa);

  vmm_log("ipa: read @%p\n", ipa);

  struct read_reply reply;
  read_reply_init(&reply, 0, (u8 *)pa);
  reply.msg.send(node, &reply);

  return -1;
}

void read_msg_init(struct read_msg *rmsg, u8 dst, u64 ipa) {
  rmsg->msg.type = MSG_READ;
  rmsg->msg.dst_bits = 1 << dst;
  rmsg->msg.send = send_read_request;
  rmsg->ipa = ipa;
}

static int recv_read_reply(struct node *node, u8 *buf) {
  struct vsmctl *vsm = &node->vsm;

  u8 *body = buf + 14;
  memcpy(vsm->readbuf, body, 1024);
  vsm->finished = 1;

  return 0;
}

static int send_read_reply(struct node *node, struct msg *msg) {
  struct read_reply *rmsg = (struct read_reply *)msg;

  vmm_log("send read reply\n", rmsg->page);

  u8 buf[1088] = {0};
  u8 *body = msg_pack_eth_header(node, msg, buf);
  memcpy(body, rmsg->page, 1024);

  node->nic->ops->xmit(node->nic, buf, 1088);

  return 0;
}

void read_reply_init(struct read_reply *rmsg, u8 dst, u8 *page) {
  rmsg->msg.type = MSG_READ_REPLY;
  rmsg->msg.dst_bits = 1 << dst;
  rmsg->msg.send = send_read_reply;
  rmsg->page = page;
}

void msg_recv_intr(u8 *buf) {
  struct node *node = &global;

  if(buf[12] != 0x19)
    return;
  u8 type = buf[13];

  switch(type) {
    case MSG_INIT:
      recv_init_request(node, buf);
      break;
    case MSG_INIT_REPLY:
      recv_init_reply(node, buf);
      break;
    case MSG_READ:
      recv_read_request(node, buf);
      break;
    case MSG_READ_REPLY:
      recv_read_reply(node, buf);
      break;
    default:
      panic("?");
  }
}

