#ifndef CORE_LOCALNODE_H
#define CORE_LOCALNODE_H

#include "types.h"
#include "vcpu.h"
#include "net.h"
#include "vgic.h"
#include "uart.h"
#include "lib.h"
#include "power.h"
#include "compiler.h"

/* localvm */
struct vm {
  struct vcpu vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;
  /* allocated RAM size to vm */
  u64 nalloc;
  /* stage 2 pagetable */
  u64 *vttbr;
  /* virtual interrupt controller */
  struct vgic *vgic;
  /* guest mmio */
  struct mmio_region *pmap;
  spinlock_t lock;
  int npmap;
  /* device tree blob */
  // struct guest_fdt *fdt;
};

/* localnode */
struct localnode {
  /* localvm */
  struct vm vm;
  /* Node id in the cluster */
  int nodeid;
  /* Am I recognized by cluster? */
  bool acked;
  /* network interface card */
  struct nic *nic;
  /* machine power controller */
  struct powerctl *powerctl;
  /* irqchip */
  struct gic_irqchip *irqchip;
  /* uartchip */
  struct uartchip *uart;
  /* node control dispatcher */
  struct nodectl *ctl;
  /* my node in the cluster */
  struct cluster_node *node;
};

extern struct localnode localnode;

#define local_nodeid()    (localnode.nodeid)

#define localvm           (localnode.vm)

void localvm_init(int nvcpu, u64 nalloc, struct guest *guest_fdt);

static inline struct vcpu *node_vcpu(int vcpuid) {
  for(struct vcpu *v = localvm.vcpus; v < &localvm.vcpus[localvm.nvcpu]; v++) {
    if(v->vcpuid == vcpuid)
      return v;
  }

  /* vcpu in remote node */
  return NULL;
}

static inline int vcpu_localid(struct vcpu *v) {
  return (int)(v - localvm.vcpus);
}

static inline struct vcpu *node_vcpu_by_localid(int localcpuid) {
  return &localvm.vcpus[localcpuid];
}

static inline bool node_macaddr_is_me(u8 *mac) {
  return memcmp(localnode.nic->mac, mac, 6) == 0;
}

#endif
