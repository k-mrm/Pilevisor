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

static int read_reply_send(u8 *dst_mac, u64 ipa, void *page);

void msg_recv_intr(struct etherframe *eth, u64 len) {
  enum msgtype m = (eth->type >> 8) & 0xff;
  struct recv_msg msg;
  msg.type = m;
  msg.src_mac = eth->src;
  msg.body = eth->body;
  msg.len = len;
  msg.data = eth;

  localnode.ctl->msg_recv_intr(&msg);
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

void init_reply_init(struct init_reply *rep, u8 *mac, u64 allocated) {
  rep->msg.type = MSG_INIT_REPLY;
  remote_macaddr(0, rep->msg.dst_mac);
  rep->msg.send = send_init_reply;

  memcpy(rep->body.me_mac, mac, 6);
  rep->body.allocated = allocated;
}

static int send_read_request(struct msg *msg) {
  struct read_req *req = (struct read_req *)msg;
  vmm_log("send read request\n");

  struct packet pk;
  packet_init(&pk, &req->body, sizeof(req->body));

  send_msg_core(msg, &pk);

  return 0;
}

void recv_read_request_intr(struct recv_msg *recvmsg) {
  vmm_log("recv read request\n");

  struct __read_req *rd = (struct __read_req *)recvmsg->body;

  /* TODO: use at instruction */
  u64 pa = ipa2pa(localnode.vttbr, rd->ipa);

  vmm_log("read ipa @%p -> pa %p\n", rd->ipa, pa);

  /* send read reply */
  read_reply_send(recvmsg->src_mac, rd->ipa, (void *)pa);
}

void read_req_init(struct read_req *rmsg, u8 dst, u64 ipa) {
  rmsg->msg.type = MSG_READ;
  remote_macaddr(dst, rmsg->msg.dst_mac);
  rmsg->msg.send = send_read_request;
  rmsg->body.ipa = ipa;
}

void recv_read_reply_intr(struct recv_msg *recvmsg) {
  struct __read_reply *rep = (struct __read_reply *)recvmsg->body;
  vmm_log("remote @%p\n", rep->ipa);
  bin_dump(rep->page, 1024);

  vsm_set_cache(rep->ipa, rep->page);
}

static int read_reply_send(u8 *dst_mac, u64 ipa, void *page) {
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

void msg_sysinit() {
  ;
}
