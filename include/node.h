#ifndef NODE_H
#define NODE_H

#include "types.h"
#include "param.h"
#include "vgic.h"
#include "spinlock.h"
#include "guest.h"
#include "net.h"
#include "vsm.h"
#include "nodectl.h"
#include "vcpu.h"
#include "lib.h"
#include "cluster.h"
#include "panic.h"
#include "compiler.h"

#define __node0     __section(".text.node0")
#define __subnode   __section(".text.subnode")

extern struct localnode localnode;

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

/* localnode */
struct localnode {
  struct vcpu vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;    /* nvcpu <= npcpu */
  u64 nalloc;
  int nodeid;
  /* Am I recognized by cluster? */
  bool acked;
  /* stage 2 pagetable */
  u64 *vttbr;
  /* interrupt controller */
  struct vgic *vgic;
  /* network interface card */
  struct nic *nic;
  /* mmio */
  spinlock_t lock;
  struct mmio_region *pmap;
  int npmap;
  /* node control dispatcher */
  struct nodectl *ctl;
  /* my node in the cluster */
  struct cluster_node *node;
};

#define local_nodeid()    (localnode.nodeid)

static inline struct vcpu *node_vcpu(int vcpuid) {
  for(struct vcpu *v = localnode.vcpus; v < &localnode.vcpus[localnode.nvcpu]; v++) {
    if(v->vcpuid == vcpuid)
      return v;
  }

  /* vcpu in remote node */
  return NULL;
}

static inline struct cluster_node *cluster_me() {
  if(!localnode.node)
    panic("oi");
  return localnode.node;
}

static inline int cluster_me_nodeid() {
  return cluster_me()->nodeid;
}

static inline int vcpu_localid(struct vcpu *v) {
  return (int)(v - localnode.vcpus);
}

static inline struct vcpu *node_vcpu_by_localid(int localcpuid) {
  return &localnode.vcpus[localcpuid];
}

void localnode_preinit(int nvcpu, u64 nalloc, struct guest *guest_fdt);

static inline bool node_macaddr_is_me(u8 *mac) {
  return memcmp(localnode.nic->mac, mac, 6) == 0;
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

void node0_broadcast_init_request(void);

#endif
