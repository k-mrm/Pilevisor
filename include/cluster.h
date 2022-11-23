#ifndef CLUSTER_H
#define CLUSTER_H

#include "types.h"
#include "param.h"
#include "memory.h"
#include "panic.h"

struct cluster_node {
  int nodeid;
  u8 mac[6];
  struct memrange mem;
  u32 vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;
};

extern struct cluster_node cluster[NODE_MAX];
extern int nr_cluster_nodes;
extern int nr_cluster_vcpus;
extern u64 node_online_map;
extern u64 node_active_map;

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

  return NULL;
}

static inline struct cluster_node *macaddr_to_node(u8 *mac) {
  struct cluster_node *node;
  foreach_cluster_node(node) {
    if(memcmp(node->mac, mac, 6) == 0)
      return node;
  }

  return NULL;
}

static inline int vcpuid_to_nodeid(int vcpuid) {
  return vcpuid_to_node(vcpuid)->nodeid;
}

static inline struct cluster_node *cluster_node(int nodeid) {
  if(nodeid >= NODE_MAX)
    panic("node");

  return &cluster[nodeid];
}

static inline bool vcpuid_in_cluster(struct cluster_node *c, int vcpuid) {
  for(int i = 0; i < c->nvcpu; i++) {
    if(c->vcpus[i] == vcpuid)
      return true;
  }

  return false;
}

static inline u8 *node_macaddr(int nodeid) {
  return cluster_node(nodeid)->mac;
}

static inline bool all_node_is_active() {
  u64 nodemask = (1 << nr_cluster_nodes) - 1;

  return (node_active_map & nodemask) == nodemask;
}

void cluster_node0_ack_node(u8 *mac, int nvcpu, u64 allocated);
void cluster_dump(void);
void cluster_node_me_init(void);

#endif
