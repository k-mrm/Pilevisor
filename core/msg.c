#include "lib.h"
#include "log.h"
#include "node.h"
#include "net.h"
#include "pcpu.h"
#include "mm.h"
#include "spinlock.h"
#include "ethernet.h"
#include "msg.h"

void msg_recv_intr(struct etherframe *eth, u64 len) {
  enum msgtype m = eth->type & 0xff;
  struct recv_msg msg;
  msg.type = m;
  msg.src_mac = eth->src;
  msg.body = eth->body;
  msg.len = len;
  msg.data = eth;

  localnode.ctl->msg_recv_intr(&msg);

  free_etherframe(eth);
}

static void send_msg_core(struct msg *msg, void *body, u32 len) {
  /* dst mac address */
  static u8 broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  u8 mac_buf[6];
  u8 *mac = NULL;

  if(msg->dst_bits == 0xff) { /* broadcast */
    mac = broadcast_mac;
  } else {
    /* TODO: multicast? */
    u8 bits = msg->dst_bits;
    for(int i = 0; bits != 0; bits = bits >> 1, i++) {
      if(bits & 1) {
        memcpy(mac_buf, localnode.remotes[i].mac, sizeof(u8)*6);
        mac = mac_buf;
        break;
      }
    }
  }
  if(!mac)
    panic("invalid");

  u16 type = (msg->type << 8) | 0x19;

  len += sizeof(struct etherframe);
  len = len < 64 ? 64 : len;

  ethernet_xmit(localnode.nic, mac, type, body, len);
}

static int send_init_request(struct msg *msg) {
  struct init_req *req = (struct init_req *)msg;
  printf("send_init_request");

  send_msg_core(msg, &req->body, sizeof(req->body));

  return 0;
}

void init_req_init(struct init_req *req, u8 *mac) {
  req->msg.type = MSG_INIT;
  req->msg.dst_bits = 0xff;
  req->msg.send = send_init_request;

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

  memcpy(rep->body.me_mac, mac, 6);
  rep->body.allocated = allocated;
}

static int send_read_request(struct msg *msg) {
  /*
  struct read_msg *rmsg = (struct read_msg *)msg;
  vmm_log("send read request\n");

  u8 buf[64] = {0};
  u8 *body = msg_pack_eth_header(node, msg, buf);
  memcpy(body, &rmsg->ipa, sizeof(rmsg->ipa));

  node->nic->xmit(node->nic, buf, 64);
  */

  return 0;
}

void recv_read_request_intr(struct recv_msg *msg) {
  vmm_log("recv read request\n");

  struct __read_req *rd = (struct __read_req *)msg->body;

  /* TODO: use at instruction */
  u64 pa = ipa2pa(localnode.vttbr, rd->ipa);

  vmm_log("ipa: read @%p\n", rd->ipa);

  /* send read reply */
  struct read_reply reply;
  read_reply_init(&reply, 0, (u8 *)pa);
  msg_send(reply);
}

void read_req_init(struct read_req *rmsg, u8 dst, u64 ipa) {
  rmsg->msg.type = MSG_READ;
  rmsg->msg.dst_bits = 1 << dst;
  rmsg->msg.send = send_read_request;
  rmsg->body.ipa = ipa;
}

int recv_read_reply_intr(struct recv_msg *recvmsg) {
  /*
  struct vsmctl *vsm = &node->vsm;

  u8 *body = buf + 14;
  memcpy(vsm->readbuf, body, 1024);
  vsm->finished = 1;
  */
  return -1;
}

static int send_read_reply(struct msg *msg) {
  /*
  struct read_reply *rmsg = (struct read_reply *)msg;

  vmm_log("send read reply\n", rmsg->page);

  u8 buf[1088] = {0};
  u8 *body = msg_pack_eth_header(node, msg, buf);
  memcpy(body, rmsg->page, 1024);

  node->nic->xmit(node->nic, buf, 1088);
  */

  return 0;
}

void read_reply_init(struct read_reply *rmsg, u8 dst, u8 *page) {
  ;
}

void msg_sysinit() {
  ;
}
