#ifndef CORE_LOCALNODE_H
#define CORE_LOCALNODE_H

#include "types.h"
#include "vcpu.h"
#include "net.h"
#include "vgic.h"
#include "lib.h"
#include "compiler.h"

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

extern struct localnode localnode;

#define local_nodeid()    (localnode.nodeid)

void localnode_preinit(int nvcpu, u64 nalloc, struct guest *guest_fdt);

static inline struct vcpu *node_vcpu(int vcpuid) {
  for(struct vcpu *v = localnode.vcpus; v < &localnode.vcpus[localnode.nvcpu]; v++) {
    if(v->vcpuid == vcpuid)
      return v;
  }

  /* vcpu in remote node */
  return NULL;
}

static inline int vcpu_localid(struct vcpu *v) {
  return (int)(v - localnode.vcpus);
}

static inline struct vcpu *node_vcpu_by_localid(int localcpuid) {
  return &localnode.vcpus[localcpuid];
}

static inline bool node_macaddr_is_me(u8 *mac) {
  return memcmp(localnode.nic->mac, mac, 6) == 0;
}

#endif
