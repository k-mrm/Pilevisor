#include "msg.h"
#include "lib.h"
#include "log.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"
#include "spinlock.h"
#include "ethernet.h"

static inline bool need_handle_now(enum msgtype ty) {
  return ty & NEED_HANDLE_IMMEDIATE;
}

void msg_recv_intr(struct etherframe *eth, u64 len) {
  enum msgtype m = eth->type & 0xff;
  struct recv_msg msg;
  msg.type = m;
  msg.src_mac = eth->src;
  msg.body = eth->body;
  msg.len = len;
  msg.data = eth;

  if(need_handle_now(m)) {
    localnode.ctl->msg_recv_intr(&msg);
    free_etherframe(eth);
  } else {
    /* add to waitqueue */
  }
}

static void send_msg_core(struct msg *msg, void *body, u32 len) {
  /* dst mac address */
  static u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  u8 mac_buf[6];
  u8 *mac;

  if(msg->dst_bits == 0xff) { /* broadcast */
    mac = broadcast_mac;
  } else {
    /* TODO: multicast? */
    u8 bits = msg->dst_bits;
    for(int i = 0; bits != 0; bits = bits >> 1, i++) {
      if(bits & 1) {
        memcpy(mac_buf, node->remotes[i].mac, sizeof(u8)*6);
        mac = mac_buf;
        break;
      }
    }
  }

  u16 type = 0x1900 | msg->type;

  len += sizeof(struct etherframe);
  len = len < 64 ? 64 : len;

  ethernet_xmit(localnode.nic, mac, type, body, len);
}

static int recv_init_request() {
  if(node->nodeid == 0)
    panic("node0 recv init msg");

  memcpy(node->remotes[0].mac, buf+6, sizeof(u8)*6);

  vmm_log("node0 mac address: %m\n", node->remotes[0].mac);
  vmm_log("me mac address: %m\n", node->nic->mac);

  struct init_reply msg;
  init_reply_init(&msg, node->nic->mac);
  msg.msg.send(node, (struct msg *)&msg);

  return 0;
}

static int send_init_request(struct msg *msg) {
  struct init_req *req = (struct init_req *)msg;

  send_msg_core(msg, &req->body, sizeof(req->body));

  return 0;
}

int recv_init_request(struct recv_msg *rmsg) {
  if(localnode.nodeid == 0)
    panic("node 0 recv init request");

  struct __init_req *body = (struct __init_req *)rmsg->body;

  memcpy(localnode.remotes[0].mac, body->me_mac, 6);
  vmm_log("node 0 mac address: %m\n", localnode.remotes[0].mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);

  /* send reply */
  struct init_reply reply;
  init_reply_init(&reply, localnode.nic->mac, 128 * 1024 * 1024);
  msg_send(reply);
  
  return 0;
}

static struct recv_msg *pop_from_waitqueue() {
  /* stub */
  return NULL;
}

static int recv_init_reply(struct msg *msg) {
  struct init_req *req = (struct init_req *)msg;

  /* TODO: pop from waitqueue */

  struct recv_msg *recvmsg = pop_from_waitqueue();
  while(!recvmsg) {
    // sleep?
    recvmsg = pop_from_waitqueue();
  }

  struct __init_reply *body = (struct __init_reply *)recvmsg->body;

  memcpy(&req->reply, body, sizeof(req->reply));

  return 0;
}

void init_req_init(strcut init_req *req, u8 *mac) {
  req->msg.type = MSG_INIT;
  req->msg.dst_bits = 0xff;
  req->msg.send = send_init_request;
  req->msg.recv_reply = recv_init_reply;

  memcpy(req->body.me_mac, mac, 6);
}

static int send_init_reply(struct msg *msg) {
  struct init_reply *rep = (struct init_reply *)msg;
  vmm_log("send_init_reply\n");

  send_msg_core(msg, &rep->body, sizeof(rep->body));

  return 0;
}

void init_reply_init(struct init_reply *rep, u8 *mac, u64 allocated) {
  rep->msg.type = MSG_INIT_REPLY;
  rep->msg.dst_bits = (1 << 0);   /* send to Node 0 */
  rep->msg.send = send_init_reply;
  rep->msg.recv_reply = NULL;

  memcpy(rep->body.me_mac, mac, 6);
  rep->body.allocated = allocated;
}

static int send_read_request(struct node *node, struct msg *msg) {
  struct read_msg *rmsg = (struct read_msg *)msg;
  vmm_log("send read request\n");

  u8 buf[64] = {0};
  u8 *body = msg_pack_eth_header(node, msg, buf);
  memcpy(body, &rmsg->ipa, sizeof(rmsg->ipa));

  node->nic->xmit(node->nic, buf, 64);

  return 0;
}

void recv_read_msg(struct recv_msg *msg) {
  vmm_log("recv read request\n");

  struct __read_msg *rd = (struct __read_msg *)msg->body;

  /* TODO: use at instruction */
  pa = ipa2pa(node->vttbr, rd->ipa);

  vmm_log("ipa: read @%p\n", rd->ipa);

  /* send read reply */
  struct read_reply reply;
  read_reply_init(&reply, 0, (u8 *)pa);
  reply.msg.send(node, &reply);
}

void read_msg_init(struct read_msg *rmsg, u8 dst, u64 ipa) {
  rmsg->msg.type = MSG_READ;
  rmsg->msg.dst_bits = 1 << dst;
  rmsg->msg.send = send_read_request;
  rmsg->rd.ipa = ipa;
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

  node->nic->xmit(node->nic, buf, 1088);

  return 0;
}

void read_reply_init(struct read_reply *rmsg, u8 dst, u8 *page) {
  rmsg->msg.type = MSG_READ_REPLY;
  rmsg->msg.dst_bits = 1 << dst;
  rmsg->msg.send = send_read_reply;
  rmsg->page = page;
}

void msg_sysinit() {
  ;
}
