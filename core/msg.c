#include "log.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"
#include "spinlock.h"
#include "ethernet.h"
#include "msg.h"
#include "lib.h"

static u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void msg_recv_intr(struct etherframe *eth, u64 len) {
  enum msgtype m = (eth->type >> 8) & 0xff;
  struct recv_msg msg;
  msg.type = m;
  msg.src_mac = eth->src;
  msg.body = eth->body;
  msg.len = len;

  if(m < NUM_MSG && localnode.ctl->msg_recv_handlers[m])
    localnode.ctl->msg_recv_handlers[m](&msg);
  else
    panic("unknown msg received: %d\n", m);
}

void msg_register_recv_handler(enum msgtype type, void (*handler)(struct recv_msg *)) {
  if(type >= NUM_MSG)
    panic("invalid msg type");
  if(!localnode.ctl)
    panic("ctlr?");

  localnode.ctl->msg_recv_handlers[type] = handler;
}

static inline void send_msg_core(struct msg *msg, struct packet *pk) {
  u16 type = (msg->type << 8) | 0x19;

  ethernet_xmit(localnode.nic, msg->dst_mac, type, pk);
}

static int send_init_request(struct msg *msg) {
  struct init_req *req = (struct init_req *)msg;
  printf("send_init_request");

  struct packet pk;
  packet_init(&pk, &req->body, sizeof(req->body));

  send_msg_core(msg, &pk);

  return 0;
}

void init_req_init(struct init_req *req, u8 *mac) {
  req->msg.type = MSG_INIT;
  memcpy(req->msg.dst_mac, broadcast_mac, 6);
  req->msg.send = send_init_request;

  memcpy(req->body.me_mac, mac, 6);
}

static int send_init_reply(struct msg *msg) {
  struct init_reply *rep = (struct init_reply *)msg;
  vmm_log("send_init_reply\n");

  struct packet pk;
  packet_init(&pk, &rep->body, sizeof(rep->body));

  send_msg_core(msg, &pk);

  return 0;
}

void init_reply_init(struct init_reply *rep, u8 *mac, int nvcpu, u64 allocated) {
  rep->msg.type = MSG_INIT_REPLY;
  remote_macaddr(0, rep->msg.dst_mac);
  rep->msg.send = send_init_reply;

  memcpy(rep->body.me_mac, mac, 6);
  rep->body.nvcpu = nvcpu;
  rep->body.allocated = allocated;
}

int send_read_request(struct msg *msg) {
  struct read_req *req = (struct read_req *)msg;
  vmm_log("send read request\n");

  struct packet pk;
  packet_init(&pk, &req->body, sizeof(req->body));

  send_msg_core(msg, &pk);

  return 0;
}

void read_req_init(struct read_req *rmsg, u8 dst, u64 ipa) {
  rmsg->msg.type = MSG_READ;
  remote_macaddr(dst, rmsg->msg.dst_mac);
  rmsg->msg.send = send_read_request;
  rmsg->body.ipa = ipa;
}

int read_reply_send(u8 *dst_mac, u64 ipa, void *page) {
  vmm_log("read reply send\n");

  struct msg msg;
  msg.type = MSG_READ_REPLY;
  memcpy(msg.dst_mac, dst_mac, 6);
  struct packet p_ipa;
  packet_init(&p_ipa, &ipa, sizeof(ipa));
  struct packet p_page;
  packet_init(&p_page, page, 4096);
  p_ipa.next = &p_page;

  send_msg_core(&msg, &p_ipa);

  return 0;
}

static void unknown_msg_recv(struct recv_msg *recvmsg) {
  panic("msg: unknown msg received: %d", recvmsg->type);
}

void msg_sysinit() {
  for(int i = 0; i < NUM_MSG; i++) {
    if(!localnode.ctl->msg_recv_handlers[i])
      localnode.ctl->msg_recv_handlers[i] = unknown_msg_recv;
  }
}
