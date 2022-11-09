#ifndef CLUSTER_H
#define CLUSTER_H

#include "types.h"
#include "node.h"
#include "param.h"
#include "memory.h"
#include "panic.h"

enum node_status {
  NODE_NULL,
  NODE_ACK,
  NODE_ONLINE,
  NODE_DEAD,
};

struct cluster_node {
  int nodeid;
  enum node_status status;
  u8 mac[6];
  struct memrange mem;
  u32 vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;
};

extern struct cluster_node cluster[NODE_MAX];
extern int nr_cluster_nodes;
extern int nr_cluster_vcpus;

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

static inline bool vcpu_in_localnode(int vcpuid) {
  return vcpuid_to_nodeid(vcpuid) == localnode.nodeid;
}

static inline struct cluster_node *cluster_node(int nodeid) {
  if(nodeid >= NODE_MAX)
    panic("node");

  return &cluster[nodeid];
}

static inline struct cluster_node *cluster_me() {
  return cluster_node(localnode.nodeid);
}

static inline int cluster_me_nodeid() {
  struct cluster_node *node = cluster_me();
  if(node)
    return node->nodeid;
  else
    return -1;
}

static inline u8 *node_macaddr(int nodeid) {
  return cluster_node(nodeid)->mac;
}

void cluster_node0_ack_node(u8 *mac, int nvcpu, u64 allocated);
void cluster_dump(void);
void cluster_node_me_init(void);

/*
 *  cluster_info_msg: Node 0 -broadcast-> Node n
 *    send argv:
 *      num of cluster nodes
 *    send body:
 *      struct cluster_node cluster[nremote];
 *
 */
struct cluster_info_hdr {
  POCV2_MSG_HDR_STRUCT;
  int nnodes;
  int nvcpus;
};

struct cluster_info_body {
  struct cluster_node cluster_info[NODE_MAX];
};

void node0_broadcast_cluster_info(void);

#endif
