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

struct mmio_access;

extern struct node localnode;

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

struct node {
  struct vcpu vcpus[VCPU_PER_NODE_MAX];
  int nvcpu;    /* nvcpu <= npcpu */
  u64 nalloc;
  int nodeid;
  /* Am I recognized by cluster? */
  bool acked;
  /* virtual shared memory */
  struct vsmctl vsm;
  /* stage 2 pagetable */
  u64 *vttbr;
  /* interrupt controller */
  struct vgic *vgic;
  /* network interface card */
  struct nic *nic;
  /* mmio */
  spinlock_t lock;
  struct mmio_info *pmap;
  int npmap;
  /* node control dispatcher */
  struct nodectl *ctl;
  /* vm's parameter */
  struct vm_desc *vm_desc;
};

void node_preinit(int nvcpu, u64 nalloc, struct vm_desc *vm_desc);

void pagetrap(struct node *node, u64 va, u64 size,
              int (*read_handler)(struct vcpu *, u64, u64 *, struct mmio_access *),
              int (*write_handler)(struct vcpu *, u64, u64, struct mmio_access *));

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

#endif
