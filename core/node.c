/*
 *  cluster node
 */

#include "types.h"
#include "aarch64.h"
#include "vcpu.h"
#include "mm.h"
#include "allocpage.h"
#include "lib.h"
#include "param.h"
#include "printf.h"
#include "log.h"
#include "mmio.h"
#include "localnode.h"
#include "node.h"
#include "vsm.h"
#include "spinlock.h"
#include "msg.h"
#include "panic.h"

static void __node0 broadcast_init_request();
static void __node0 broadcast_cluster_info();
static void __node0 recv_init_ack_intr(struct pocv2_msg *msg);
static void __node0 recv_sub_setup_done_notify_intr(struct pocv2_msg *msg);
static void __subnode init_ack_reply(u8 *node0_mac, int nvcpu, u64 allocated);
static void __subnode recv_init_request_intr(struct pocv2_msg *msg);
static void __subnode send_setup_done_notify(u8 status);
static void __subnode recv_cluster_info_intr(struct pocv2_msg *msg);

static int cluster_node_me_setup();

struct cluster_node cluster[NODE_MAX];
int nr_cluster_nodes = 0;
int nr_cluster_vcpus = 0;

u64 node_online_map = 0;
u64 node_active_map = 0;
u64 node_running_vcpu_map = 0;

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

static void setup_vsm_memrange(struct memrange *m, u64 alloc) {
  static u64 ram_start = 0x40000000;
  u64 flags = 0;

  irqsave(flags);

  m->start = ram_start;
  m->size = alloc;

  ram_start += alloc;

  irqrestore(flags);
}

static inline void wait_for_all_node_online() {
  while(!all_node_is_online())
    wfi();
}

static inline void wait_for_all_node_ready() {
  while(!all_node_is_active())
    wfi();
}

/*
 *  Node0 ack me or sub-node
 */
static void __node0 node0_ack_node(u8 *mac, int nvcpus, u64 allocated) {
  int nodeid = alloc_nodeid();
  struct cluster_node *c = cluster_node(nodeid);

  vmm_log("node0 ack Node%d %m %d %p byte\n", nodeid, mac, nvcpus, allocated);
  
  node_set_online(nodeid, true);
  printf("node online status: %p\n", node_online_map);

  c->nodeid = nodeid;
  memcpy(c->mac, mac, 6);
  setup_vsm_memrange(&c->mem, allocated);
  c->nvcpu = nvcpus;
  for(int i = 0; i < nvcpus; i++)
    c->vcpus[i] = alloc_vcpuid();
}

static void __node0 cluster_node0_init(u8 *mac, int nvcpu, u64 allocated) {
  /* Node0 acked Node0 */
  node0_ack_node(mac, nvcpu, allocated);
}

/*
 *
 *  Node discover protocol:
 *
 *          1         2'       3          4'
 *  Node0 --+---------+----+---+----------+---+----->
 *           \\       ^    ^    \\        ^   ^
 *            v\ 1'  /2   /      v\3'  4 /   /
 *  Node1 ----+-\---+----/-------+-\----+---/------->
 *               \      /           \      /
 *                v    /             v    /
 *  Node2 --------+---+--------------+---+---------->
 *
 */

void __node0 cluster_init() {
  cluster_node0_init(localnode.nic->mac, localvm.nvcpu, localvm.nalloc);

  intr_enable();

  /* 1. send initialization request to sub-node */
  broadcast_init_request();
  /* 2'. */
  wait_for_all_node_online();

  /* 3. broadcast cluster information to sub-node */
  broadcast_cluster_info();

  if(cluster_node_me_setup() < 0)
    panic("my node failed");

  wait_for_all_node_ready();
  /* 4'. receive setup done notify from sub-node! */
}

static void __subnode wait_for_acked_me() {
  while(!localnode.acked)
    wfi();

  isb();
}

void __subnode subnode_cluster_init() {
  vmm_log("Waiting for recognition from cluster...\n");

  intr_enable();

  /* 1' and 2 and 3' */
  wait_for_acked_me();

  vmm_log("Node %d initializing...\n", cluster_me()->nodeid);

  /* sub-node setup */
  int status = cluster_node_me_setup();

  /* 4. sub-node setup done! */
  send_setup_done_notify(status);
}

/*
 *  recv cluster info from Node0
 */
static void __subnode update_cluster_info(int nnodes, int nvcpus, struct cluster_node *c) {
  printf("receive cluster info from Node0\n");

  nr_cluster_nodes = nnodes;
  nr_cluster_vcpus = nvcpus;
  memcpy(cluster, c, sizeof(cluster));
  for(int i = 0; i < nr_cluster_nodes; i++) {
    printf("cluster[%d] %m\n", i, cluster[i].mac);

    node_set_online(i, true);
    node_set_active(i, true);

    if(node_macaddr_is_me(cluster[i].mac)) {
      /* recognize me */
      vmm_log("cluster info: I am Node %d\n", i);
      localnode.nodeid = i;
      localnode.node = &cluster[i];
      localnode.acked = true;
    }
  }

  node_cluster_dump();

  if(!localnode.acked)
    panic("whoami????");
}

static int cluster_node_me_setup() {
  struct cluster_node *me = cluster_me();

  vmm_log("cluster node%p %d init\n", me, me->nodeid);

  vsm_node_init(&me->mem);

  vcpuid_init(me->vcpus, me->nvcpu);

  node_set_active(me->nodeid, true);

  return 0;
}

void node_cluster_dump() {
  struct cluster_node *node;
  char *online, *active;

  printf("Cluster Info:\n");
  printf("\tnr_nodes: %d nr_vcpus: %d\n", nr_cluster_nodes, nr_cluster_vcpus);

  foreach_cluster_node(node) {
    int id = node->nodeid;
    online = node_online(id) ? "online" : "offline";
    active = node_active(id) ? "active" : "inactive";
    u64 memstart = node->mem.start;
    u64 memend = memstart + node->mem.size;

    printf("\tNode %d:\n"
           "\t\tmac address: %m\n"
           "\t\tstatus: %s %s\n"
           "\t\tvm memory: %p - %p\n"
           "\t\tnum of vcpus: %d\n",
           id, node->mac, online, active, memstart, memend, node->nvcpu);
  }
}

void node_send_msg(int dst_id, struct pocv2_msg *msg) {
  ;
}

void node_broadcast_msg(struct pocv2_msg *msg) {
  ;
}

static void __node0 broadcast_init_request() {
  printf("broadcast init request");
  struct pocv2_msg msg;
  struct init_req_hdr hdr;

  pocv2_broadcast_msg_init(&msg, MSG_INIT, &hdr, NULL, 0);

  send_msg(&msg);
}

static void __node0 broadcast_cluster_info() {
  vmm_log("broadcast cluster info from Node0\n");
  node_cluster_dump();

  struct pocv2_msg msg;
  struct cluster_info_hdr hdr;

  hdr.nnodes = nr_cluster_nodes;
  hdr.nvcpus = nr_cluster_vcpus;

  pocv2_broadcast_msg_init(&msg, MSG_CLUSTER_INFO, &hdr, cluster, sizeof(cluster));

  send_msg(&msg);
}

static void __subnode send_setup_done_notify(u8 status) {
  struct pocv2_msg msg;
  struct setup_done_hdr hdr;

  hdr.status = status;

  pocv2_msg_init2(&msg, 0, MSG_SETUP_DONE, &hdr, NULL, 0);

  send_msg(&msg);
}

static void __subnode init_ack_reply(u8 *node0_mac, int nvcpu, u64 allocated) {
  vmm_log("send init ack\n");
  struct pocv2_msg msg;
  struct init_ack_hdr hdr;

  hdr.nvcpu = nvcpu;
  hdr.allocated = allocated;

  pocv2_msg_init(&msg, node0_mac, MSG_INIT_ACK, &hdr, NULL, 0);

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

  node_set_active(src_nodeid, true);

  vmm_log("node %d READY!\n", src_nodeid);
}

static void __subnode recv_init_request_intr(struct pocv2_msg *msg) {
  u8 *node0_mac = pocv2_msg_src_mac(msg);
  vmm_log("node0 mac address: %m\n", node0_mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localvm.nvcpu, localvm.nalloc);

  init_ack_reply(node0_mac, localvm.nvcpu, localvm.nalloc);
}

static void __subnode recv_cluster_info_intr(struct pocv2_msg *msg) {
  struct cluster_info_hdr *h = (struct cluster_info_hdr *)msg->hdr;
  struct cluster_info_body *b = msg->body;

  update_cluster_info(h->nnodes, h->nvcpus, b->cluster_info);
}

DEFINE_POCV2_MSG_RECV_NODE0(MSG_INIT_ACK, struct init_ack_hdr, recv_init_ack_intr);
DEFINE_POCV2_MSG_RECV_NODE0(MSG_SETUP_DONE, struct setup_done_hdr, recv_sub_setup_done_notify_intr);
DEFINE_POCV2_MSG_RECV_SUBNODE(MSG_INIT, struct init_req_hdr, recv_init_request_intr);
DEFINE_POCV2_MSG_RECV_SUBNODE(MSG_CLUSTER_INFO, struct cluster_info_hdr, recv_cluster_info_intr);
