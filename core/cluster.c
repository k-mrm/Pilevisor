#include "cluster.h"
#include "vcpu.h"
#include "node.h"
#include "msg.h"

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

void broadcast_cluster_info() {
  struct msg msg;

  vmm_log("broadcast cluster info from Node0\n");

  broadcast_msg_header_init(&msg, MSG_CLUSTER_INFO);

  struct packet pk_nnodes, pk_c;
  packet_init(&pk_nnodes, &nr_cluster_nodes, sizeof(nr_cluster_nodes));
  packet_init(&pk_c, cluster, sizeof(cluster));
  pk_nnodes.next = &pk_c;

  msg.pk = &pk_nnodes;
  send_msg(&msg);
}

void update_cluster_info(int nnodes, struct cluster_node *c) {
  nr_cluster_nodes = nnodes;
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

void cluster_ack_node(u8 *mac, int nvcpu, u64 allocated) {
  int nodeid = cluster_alloc_nodeid();

  struct cluster_node *c = &cluster[nodeid];

  c->nodeid = nodeid;
  c->status = NODE_ACK;
  memcpy(c->mac, mac, 6);
  cluster_setup_vsm_memrange(&c->mem, allocated);
  c->nvcpu = nvcpu;
  for(int i = 0; i < nvcpu; i++)
    c->vcpus[i] = cluster_alloc_vcpuid();
}

void cluster_dump() {
  static char *states[] = {
    [NODE_NULL]   "null",
    [NODE_ACK]    "acked",
    [NODE_ONLINE] "online",
    [NODE_DEAD]   "dead",
  };

  struct cluster_node *node;
  printf("nr cluster: %d\n", nr_cluster_nodes);
  foreach_cluster_node(node) {
    printf("Node %d: %m %s\n", node->nodeid, node->mac, states[node->status]);
  }
}
