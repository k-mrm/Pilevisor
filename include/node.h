#ifndef NODE_H
#define NODE_H

#include "types.h"
#include "param.h"
#include "localnode.h"
#include "nodectl.h"
#include "spinlock.h"
#include "guest.h"
#include "vsm.h"
#include "msg.h"
#include "lib.h"
#include "panic.h"
#include "compiler.h"

#define __node0     __section(".text.node0")
#define __subnode   __section(".text.subnode")

/* vm descriptor */
struct vm_desc {
  struct guest *os_img;
  struct guest *fdt_img;
  struct guest *initrd_img;
  int nvcpu;
  u64 ram_start;
  u64 nallocate;
  u64 entrypoint;
  u64 fdt_base;
  u64 initrd_base;
};

/* configuration per node */
struct nodeconfig {
  int nvcpu;
  u64 nallocate;
};

/* node information of the cluster */
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

static inline struct cluster_node *cluster_me() {
  return localnode.node;
}

static inline int cluster_me_nodeid() {
  struct cluster_node *me = cluster_me();

  if(me)
    return me->nodeid;
  else
    return 0;
}

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
    node_active_map |= mask;
  else
    node_active_map &= ~mask;
}

static inline bool node_online(int nodeid) {
  return !!(node_online_map & (1ul << nodeid));
}

static inline bool node_active(int nodeid) {
  return !!(node_active_map & (1ul << nodeid));
}

static inline bool all_node_is_online() {
  u64 nodemask = (1 << NR_NODE) - 1;

  return (node_online_map & nodemask) == nodemask;
}

static inline bool all_node_is_active() {
  u64 nodemask = (1 << nr_cluster_nodes) - 1;

  return (node_active_map & nodemask) == nodemask;
}

/*
 *  Node initialize message
 *  init request: Node 0 --broadcast--> Node n(n!=0)
 *    send:
 *      (nop)
 *
 *  init ack:   Node n ---> Node 0
 *    send-arg:
 *      num of vCPU allocated to VM
 *      allocated ram size to VM from Node n
 */

struct init_req_hdr {
  POCV2_MSG_HDR_STRUCT;
};

struct init_ack_hdr {
  POCV2_MSG_HDR_STRUCT;
  int nvcpu;
  u64 allocated;
};

/*
 *  setup_done_notify: Node n ---> Node 0
 *    send-arg:
 *      status (0 = success, else = failure)
 *
 */

struct setup_done_hdr {
  POCV2_MSG_HDR_STRUCT;
  u8 status;
};

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

void node_cluster_dump(void);
void __node0 cluster_init(void);
void __subnode subnode_cluster_init(void);

#endif
