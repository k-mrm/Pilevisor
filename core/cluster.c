#include "cluster.h"
#include "vcpu.h"
#include "node.h"
#include "msg.h"

struct cluster_node cluster[NODE_MAX];
int nr_cluster_nodes = 0;
int nr_cluster_vcpus = 0;
bool cluster_initialized = false;

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
  broadcast_msg_header_init(&msg, MSG_CLUSTER_INFO);

  struct packet pk_nnodes, pk_c;
  packet_init(&pk_nnodes, &nr_cluster_nodes, sizeof(nr_cluster_nodes));
  packet_init(&pk_c, &cluster, sizeof(cluster));
  pk_nnodes.next = &pk_c;

  msg.pk = &pk_nnodes;
  send_msg(&msg);
}

void update_cluster_info(int nnodes, struct cluster_node *c) {
  nr_cluster_nodes = nnodes;
  memcpy(cluster, c, sizeof(cluster));

  /* recognize me */
  for(int i = 0; i < nr_cluster_nodes; i++) {
    if(node_macaddr_is_me(cluster[i].mac)) {
      vmm_log("cluster info: I am Node %d\n", i);
      localnode.nodeid = i;
      localnode.online = true;
      goto found;
    }
    panic("whoami??????");
  }

found:
  cluster_initialized = true;
}

void cluster_add_node(u8 *mac, int nvcpu, u64 allocated) {
  int nodeid = cluster_alloc_nodeid();

  struct cluster_node *c = &cluster[nodeid];

  c->nodeid = nodeid;
  memcpy(c->mac, mac, 6);
  node0_setup_vsm_memrange(&c->mem, allocated);
  c->nvcpu = nvcpu;
  for(int i = 0; i < nvcpu; i++)
    c->vcpus[i] = cluster_alloc_vcpuid();
}
