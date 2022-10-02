#include "types.h"
#include "vcpu.h"
#include "mm.h"
#include "allocpage.h"
#include "lib.h"
#include "memmap.h"
#include "printf.h"
#include "log.h"
#include "mmio.h"
#include "node.h"
#include "cluster.h"

struct node localnode;    /* me */

void pagetrap(struct node *node, u64 ipa, u64 size,
              int (*read_handler)(struct vcpu *, struct mmio_access *),
              int (*write_handler)(struct vcpu *, struct mmio_access *)) {
  u64 *vttbr = node->vttbr;

  if(pagewalk(vttbr, ipa, 0))
    pageunmap(vttbr, ipa, size);

  if(mmio_reg_handler(node, ipa, size, read_handler, write_handler) < 0)
    panic("?");
}

void node_preinit(int nvcpu, u64 nalloc, struct guest *guest_fdt) {
  vmm_log("node n vCPU: %d total RAM: %p byte\n", nvcpu, nalloc);

  u64 *vttbr = alloc_page();
  if(!vttbr)
    panic("vttbr");

  localnode.vttbr = vttbr;
  write_sysreg(vttbr_el2, vttbr);

  map_peripherals(localnode.vttbr);

  localnode.nvcpu = nvcpu;
  localnode.nalloc = nalloc;

  localnode.pmap = NULL;
  spinlock_init(&localnode.lock);

  /* TODO: determines vm's device info from fdt file */
  (void)guest_fdt;

  vgic_init();

  msg_sysinit();
}

void node0_broadcast_init_request() {
  printf("broadcast init request");
  struct pocv2_msg msg;
  struct init_req_hdr hdr;

  pocv2_broadcast_msg_init(&msg, MSG_INIT, &hdr, NULL, 0);

  send_msg(&msg);
}

static void node0_recv_init_ack_intr(struct pocv2_msg *msg) {
  struct init_ack_hdr *i = (struct init_ack_hdr *)msg->hdr;

  cluster_node0_ack_node(pocv2_msg_src_mac(msg), i->nvcpu, i->allocated);
  vmm_log("Node 1: %d vcpus %p bytes\n", i->nvcpu, i->allocated);
}

static void node0_recv_sub_setup_done_notify_intr(struct pocv2_msg *msg) {
  struct setup_done_hdr *s = (struct setup_done_hdr *)msg->hdr;
  int src_nodeid = pocv2_msg_src_nodeid(msg);

  if(s->status == 0)
    vmm_log("Node %d: setup ran successfully\n", src_nodeid);
  else
    vmm_log("Node %d: setup failed\n", src_nodeid);

  cluster_node(src_nodeid)->status = NODE_ONLINE;

  vmm_log("node %d online\n", src_nodeid);
}

static void init_ack_reply(u8 *node0_mac, int nvcpu, u64 allocated) {
  vmm_log("send init ack\n");
  struct pocv2_msg msg;
  struct init_ack_hdr hdr;

  hdr.nvcpu = nvcpu;
  hdr.allocated = allocated;

  pocv2_msg_init(&msg, node0_mac, MSG_INIT_ACK, &hdr, NULL, 0);

  send_msg(&msg);
}

static void recv_init_request_intr(struct pocv2_msg *msg) {
  u8 *node0_mac = pocv2_msg_src_mac(msg);
  vmm_log("node0 mac address: %m\n", node0_mac);
  vmm_log("me mac address: %m\n", localnode.nic->mac);
  vmm_log("sub: %d vcpu %p byte RAM\n", localnode.nvcpu, localnode.nalloc);

  init_ack_reply(node0_mac, localnode.nvcpu, localnode.nalloc);
}

void send_setup_done_notify(u8 status) {
  struct pocv2_msg msg;
  struct setup_done_hdr hdr;

  hdr.status = status;

  pocv2_msg_init2(&msg, 0, MSG_SETUP_DONE, &hdr, NULL, 0);

  send_msg(&msg);
}

void nodedump(struct node *node) {
  printf("================== node  ================\n");
  printf("nvcpu %4d nodeid %4d\n", node->nvcpu, node->nodeid);
  printf("nic %p mac %m\n", node->nic, node->nic->mac);
  printf("ctl %p\n", node->ctl);
  printf("=========================================\n");
}

DEFINE_POCV2_MSG(MSG_INIT, struct init_req_hdr, recv_init_request_intr);
DEFINE_POCV2_MSG(MSG_INIT_ACK, struct init_ack_hdr, node0_recv_init_ack_intr);
DEFINE_POCV2_MSG(MSG_SETUP_DONE, struct setup_done_hdr, node0_recv_sub_setup_done_notify_intr);
