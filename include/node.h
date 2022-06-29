#ifndef NODE_H
#define NODE_H

#include "types.h"
#include "param.h"
#include "vgic.h"
#include "spinlock.h"
#include "guest.h"
#include "vm.h"

struct node {
  struct vcpu *vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;    /* nvcpu == npcpu */

  /* vsm */
  struct vsmctl vsm;

  /* vgic */
  struct vgic *vgic;

  /* nic */
  struct nic *nic;
  u8 mac[6];

  struct mmio_info *pmap;
  int npmap;

  spinlock_t lock;

  u64 fdt_base;

  /* remote node */
  struct rnode_desc {
    u8 mac[6];
    u8 enabled;
  } remote[NODE_MAX];
  int nremote;
};

void node_init(struct node *node, struct vmconfig *vmcfg);

#endif
