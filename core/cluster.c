#include "cluster.h"
#include "vcpu.h"
#include "node.h"
#include "msg.h"
#include "vsm.h"

struct cluster_node cluster[NODE_MAX];
int nr_cluster_nodes = 0;
int nr_cluster_vcpus = 0;

static int cluster_alloc_nodeid() {
  int id = nr_cluster_nodes++;
  if(id >= NODE_MAX)
    panic("too many node");

  return id;
}

static int cluster_alloc_vcpuid() {
  int id = nr_cluster_vcpus++;
  if(id >= VCPU_MAX)
    panic("too many node");

  return id;
}

static void cluster_setup_vsm_memrange(struct memrange *m, u64 alloc) {
  static u64 ram_start = 0x40000000;

  m->start = ram_start;
  m->size = alloc;

  ram_start += alloc;
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
      localnode.acked = true;
      return;
    }
  }
  panic("whoami??????");
}

void cluster_node0_ack_node(u8 *mac, int nvcpus, u64 allocated) {
  vmm_log("node0 ack node!! %m %d %p byte\n", mac, nvcpus, allocated);
  
  int nodeid = cluster_alloc_nodeid();

  struct cluster_node *c = &cluster[nodeid];

  c->nodeid = nodeid;
  c->status = NODE_ACK;
  memcpy(c->mac, mac, 6);
  cluster_setup_vsm_memrange(&c->mem, allocated);
  c->nvcpu = nvcpus;
  for(int i = 0; i < nvcpus; i++)
    c->vcpus[i] = cluster_alloc_vcpuid();
}

void cluster_node_me_init() {
  struct cluster_node *me = cluster_me();

  vmm_log("cluster node%p %d init\n", me, me->nodeid);
  cluster_dump();

  vsm_node_init(&me->mem);

  vcpuid_init(me->vcpus, me->nvcpu);
}

void node0_broadcast_cluster_info() {
  vmm_log("broadcast cluster info from Node0\n");
  cluster_dump();

  struct pocv2_msg msg;
  struct cluster_info_hdr hdr;

  hdr.nnodes = nr_cluster_nodes;
  hdr.nvcpus = nr_cluster_vcpus;

  pocv2_broadcast_msg_init(&msg, MSG_CLUSTER_INFO, &hdr, cluster, sizeof(cluster));

  send_msg(&msg);
}

static void recv_cluster_info_intr(struct pocv2_msg *msg) {
  struct cluster_info_hdr *h = (struct cluster_info_hdr *)msg->hdr;
  struct cluster_info_body *b = msg->body;

  update_cluster_info(h->nnodes, h->nvcpus, b->cluster_info);
}

void cluster_dump() {
  static char *states[] = {
    [NODE_NULL]   "null",
    [NODE_ACK]    "acked",
    [NODE_ONLINE] "online",
    [NODE_DEAD]   "dead",
  };

  struct cluster_node *node;
  printf("nr cluster: %d nvcpus: %d\n", nr_cluster_nodes, nr_cluster_vcpus);
  foreach_cluster_node(node) {
    printf("Node %d: %m %s nvcpu %d\n", node->nodeid, node->mac, states[node->status], node->nvcpu);
  }
}

DEFINE_POCV2_MSG(MSG_CLUSTER_INFO, struct cluster_info_hdr, recv_cluster_info_intr);
