#include "types.h"
#include "aarch64.h"
#include "vcpu.h"
#include "mm.h"
#include "allocpage.h"
#include "lib.h"
#include "memmap.h"
#include "printf.h"
#include "log.h"
#include "mmio.h"
#include "node.h"
#include "cluster.h"
#include "vsm.h"
#include "spinlock.h"
#include "panic.h"

struct cluster_node cluster[NODE_MAX];
int nr_cluster_nodes = 0;
int nr_cluster_vcpus = 0;

u64 node_ack_map = 0;
static spinlock_t ack_lk;

u64 node_online_map = 0;
static spinlock_t online_lk;

static int __node0 alloc_nodeid() {
  u64 flags = 0;

  irqsave(flags);

  int id = nr_cluster_nodes++;
  if(id >= NODE_MAX)
    panic("too many node");

  irqrestore(flags);

  return id;
}

static int __node0 alloc_vcpuid() {
  u64 flags = 0;

  irqsave(flags);

  int id = nr_cluster_vcpus++;
  if(id >= VCPU_MAX)
    panic("too many vcpu");

  irqrestore(flags);

  return id;
}

static void cluster_setup_vsm_memrange(struct memrange *m, u64 alloc) {
  static u64 ram_start = 0x40000000;
  u64 flags = 0;

  irqsave(flags);

  m->start = ram_start;
  m->size = alloc;

  ram_start += alloc;

  irqrestore(flags);
}

static inline void node_set_online(int nodeid, bool online) {
  u64 mask = 1ul << nodeid;

  if(online)
    node_online_map |= mask;
  else
    node_online_map &= ~mask;
}

static inline void node_set_active(int nodeid, bool active) {
  u64 mask = 1ul << nodeid;

  if(active) 
    node_active_map |= 1ul << nodeid;
  else
    node_active_map &= ~(1ul << nodeid);
}

static void __node0 node0_ack_node(u8 *mac, int nvcpus, u64 allocated) {
  int nodeid = alloc_nodeid();
  struct cluster_node *c = cluster_node(nodeid);

  vmm_log("node0 ack Node%d %m %d %p byte\n", nodeid, mac, nvcpus, allocated);
  
  node_set_online(nodeid, true);

  c->nodeid = nodeid;
  memcpy(c->mac, mac, 6);
  cluster_setup_vsm_memrange(&c->mem, allocated);
  c->nvcpu = nvcpus;
  for(int i = 0; i < nvcpus; i++)
    c->vcpus[i] = alloc_vcpuid();
}

void cluster_node0_init() {
  node_set_online(0, true);
  node_set_active(0, true);
}

static void update_cluster_info(int nnodes, int nvcpus, struct cluster_node *c) {
  nr_cluster_nodes = nnodes;
  nr_cluster_vcpus = nvcpus;
  memcpy(cluster, c, sizeof(cluster));
  cluster_dump();

  /* recognize me */
  for(int i = 0; i < nr_cluster_nodes; i++) {
    printf("cluster[%d] %d %m\n", i, cluster[i].status, cluster[i].mac);
    if(cluster[i].status == NODE_ACK && node_macaddr_is_me(cluster[i].mac)) {
      vmm_log("cluster info: I am Node %d\n", i);
      localnode.nodeid = i;
      localnode.node = &cluster[i];
      localnode.acked = true;
      return;
    }
  }

  panic("whoami??????");
}

int cluster_node_me_setup() {
  struct cluster_node *me = cluster_me();

  vmm_log("cluster node%p %d init\n", me, me->nodeid);
  cluster_dump();

  vsm_node_init(&me->mem);

  vcpuid_init(me->vcpus, me->nvcpu);

  return 0;
}

void node_cluster_dump() {
  struct cluster_node *node;
  printf("nr cluster: %d nvcpus: %d\n", nr_cluster_nodes, nr_cluster_vcpus);
  foreach_cluster_node(node) {
    printf("Node %d: %m nvcpu %d\n", node->nodeid, node->mac, node->nvcpu);
  }
}

void __node0 broadcast_init_request() {
  printf("broadcast init request");
  struct pocv2_msg msg;
  struct init_req_hdr hdr;

  pocv2_broadcast_msg_init(&msg, MSG_INIT, &hdr, NULL, 0);

  send_msg(&msg);
}

static void __node0 broadcast_cluster_info() {
  vmm_log("broadcast cluster info from Node0\n");
  cluster_dump();

  struct pocv2_msg msg;
  struct cluster_info_hdr hdr;

  hdr.nnodes = nr_cluster_nodes;
  hdr.nvcpus = nr_cluster_vcpus;

  pocv2_broadcast_msg_init(&msg, MSG_CLUSTER_INFO, &hdr, cluster, sizeof(cluster));

  send_msg(&msg);
}

static void __node0 recv_init_ack_intr(struct pocv2_msg *msg) {
  struct init_ack_hdr *i = (struct init_ack_hdr *)msg->hdr;

  node0_ack_node(pocv2_msg_src_mac(msg), i->nvcpu, i->allocated);
  vmm_log("Node 1: %d vcpus %p bytes\n", i->nvcpu, i->allocated);
}

static void __node0 recv_sub_setup_done_notify_intr(struct pocv2_msg *msg) {
  struct setup_done_hdr *s = (struct setup_done_hdr *)msg->hdr;
  int src_nodeid = pocv2_msg_src_nodeid(msg);

  if(s->status == 0)
    vmm_log("Node %d: setup ran successfully\n", src_nodeid);
  else
    vmm_log("Node %d: setup failed\n", src_nodeid);

  cluster_node(src_nodeid)->status = NODE_ONLINE;

  vmm_log("node %d online\n", src_nodeid);
}

static void init_ack_reply(u8 *node0_mac, int nvcpu, u64 allocated) {
  vmm_log("send init ack\n");
  struct pocv2_msg msg;
  struct init_ack_hdr hdr;

  hdr.nvcpu = nvcpu;
  hdr.allocated = allocated;

  pocv2_msg_init(&msg, node0_mac, MSG_INIT_ACK, &hdr, NULL, 0);

  send_msg(&msg);
}

static void __subnode recv_init_request_intr(struct pocv2_msg *msg) {
  u8 *node0_mac = pocv2_msg_src_mac(msg);
  vmm_log("node0 mac address: %m\n", node0_mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localnode.nvcpu, localnode.nalloc);

  init_ack_reply(node0_mac, localnode.nvcpu, localnode.nalloc);
}

void __subnode send_setup_done_notify(u8 status) {
  struct pocv2_msg msg;
  struct setup_done_hdr hdr;

  hdr.status = status;

  pocv2_msg_init2(&msg, 0, MSG_SETUP_DONE, &hdr, NULL, 0);

  send_msg(&msg);
}

static void __subnode recv_cluster_info_intr(struct pocv2_msg *msg) {
  struct cluster_info_hdr *h = (struct cluster_info_hdr *)msg->hdr;
  struct cluster_info_body *b = msg->body;

  update_cluster_info(h->nnodes, h->nvcpus, b->cluster_info);
}

DEFINE_POCV2_MSG(MSG_INIT, struct init_req_hdr, recv_init_request_intr);
DEFINE_POCV2_MSG(MSG_INIT_ACK, struct init_ack_hdr, recv_init_ack_intr);
DEFINE_POCV2_MSG(MSG_SETUP_DONE, struct setup_done_hdr, recv_sub_setup_done_notify_intr);
DEFINE_POCV2_MSG(MSG_CLUSTER_INFO, struct cluster_info_hdr, recv_cluster_info_intr);
