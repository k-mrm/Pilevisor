#ifndef CLUSTER_H
#define CLUSTER_H

#include "types.h"
#include "node.h"
#include "param.h"
#include "memory.h"
#include "msg.h"

struct cluster_node {
  int nodeid;
  u8 mac[6];
  struct memrange mem;
  u32 vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;
};

extern struct cluster_node cluster[NODE_MAX];
extern int nr_cluster_nodes;

#define foreach_cluster_node(c)   \
  for(c = cluster; c < &cluster[nr_cluster_nodes]; c++)

static inline struct cluster_node *vcpuid_to_node(int vcpuid) {
  struct cluster_node *node;
  foreach_cluster_node(node) {
    for(int i = 0; i < node->nvcpu; i++) {
      if(node->vcpus[i] == vcpuid)
        return node;
    }
  }
}

static inline int vcpuid_to_nodeid(int vcpuid) {
  return vcpuid_to_node(vcpuid)->nodeid;
}

static inline bool vcpu_in_localnode(int vcpuid) {
  return vcpuid_to_nodeid(vcpuid) == localnode.nodeid;
}

void broadcast_cluster_info(void);
void update_cluster_info(int nnodes, struct cluster_node *c);
void cluster_add_node(u8 *mac, int nvcpu, u64 allocated);

/*
 *  cluster_info_msg: Node 0 -broadcast-> Node n
 *    send:
 *      num of cluster nodes
 *      struct cluster_node cluster[nremote];
 *
 */
struct cluster_info_msg {
  int nnodes;
  struct cluster_node cluster_info[NODE_MAX];
};

#endif
