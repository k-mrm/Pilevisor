#include "types.h"
#include "aarch64.h"
#include "vsm.h"
#include "mm.h"
#include "allocpage.h"
#include "log.h"
#include "lib.h"
#include "node.h"
#include "vcpu.h"
#include "msg.h"
#include "cluster.h"

static void send_read_request(u8 dst, u64 ipa);
static void send_read_reply(u8 *dst_mac, u64 ipa, void *page);

/* virtual shared memory */

/*
 *  memory read message
 *  read request: Node n1 ---> Node n2
 *    send
 *      - intermediate physical address(ipa)
 *
 *  read reply:   Node n1 <--- Node n2
 *    send
 *      - intermediate physical address(ipa)
 *      - 4KB page corresponding to ipa
 */

struct read_req_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 ipa;
};

struct read_reply_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 ipa;
};

struct read_reply_body {
  u8 page[4096];
};

/* TODO: determine dst_node by ipa */
static inline int page_owner(u64 ipa) {
  struct cluster_node *node;
  foreach_cluster_node(node) {
    if(in_memrange(&node->mem, ipa))
      return node->nodeid;
  }

  return -1;
}

/*
static u64 vsm_fetch_page_dummy(u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  u64 pa = ipa2pa(vsm->dummypgt, page_ipa);
  if(!pa)
    panic("non pa");

  memcpy(buf, (u8 *)pa, PAGESIZE);

  return pa;
}

int vsm_fetch_and_cache_dummy(u64 page_ipa) {
  char *page = alloc_page();
  if(!page)
    panic("mem");

  struct vcpu *vcpu = &node->vcpus[0];

  vsm_fetch_page_dummy(1, page_ipa, page);

  pagemap(node->vttbr, page_ipa, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  
  vmm_log("dummy cache %p elr %p va %p\n", page_ipa, vcpu->reg.elr, vcpu->dabt.fault_va);

  return 0;
} */

static void vsm_set_cache_fast(u64 ipa, u8 *page) {
  static int count = 0;
  u64 *vttbr = localnode.vttbr;

  vmm_bug_on(!PAGE_ALIGNED(ipa), "ipa align");

  vmm_log("vsm: cache @%p %d\n", ipa, ++count);
  pagemap(vttbr, ipa, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
}

/*
static void vsm_set_cache(u64 ipa, u8 *page, bool noncopy) {
  if(noncopy) {
    c = page;
  } else {
    c = alloc_page();
    if(!c)
      panic("cache");
    memcpy(c, page, PAGESIZE);
  }
}
*/

void *vsm_fetch_page(u64 page_ipa, bool wr) {
  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  int dst_node = page_owner(page_ipa);
  if(dst_node < 0)
    return NULL;

  u64 *vttbr = localnode.vttbr;
  vmm_log("request remote fetch!!!!: %p\n", page_ipa);

  /* send read request */
  send_read_request(dst_node, page_ipa);

  u64 pa;
  while(!(pa = ipa2pa(vttbr, page_ipa)))
    wfi();

  return (void *)pa;
}

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr) {
  if(!buf)
    panic("null buf");

  u64 page_ipa = PAGE_ADDRESS(ipa);
  char *pa_page = vsm_fetch_page(page_ipa, wr);
  if(!pa_page)
    return -1;

  u32 offset = PAGE_OFFSET(ipa);
  if(wr)
    memcpy(pa_page+offset, buf, size);
  else
    memcpy(buf, pa_page+offset, size);

  return 0;
}

static void send_read_request(u8 dst, u64 ipa) {
  struct pocv2_msg msg;
  struct read_req_hdr hdr;

  hdr.ipa = ipa;

  pocv2_msg_init2(&msg, dst, MSG_READ, &hdr, NULL, 0);

  send_msg(&msg);
}

static void send_read_reply(u8 *dst_mac, u64 ipa, void *page) {
  struct pocv2_msg msg;
  struct read_reply_hdr hdr;

  hdr.ipa = ipa;

  pocv2_msg_init(&msg, dst_mac, MSG_READ_REPLY, &hdr, page, PAGESIZE);

  send_msg(&msg);
}

static void recv_read_request_intr(struct pocv2_msg *msg) {
  struct read_req_hdr *a = (struct read_req_hdr *)msg->hdr;

  /* TODO: use at instruction */
  u64 pa = ipa2pa(localnode.vttbr, a->ipa);
  vmm_log("read ipa @%p -> pa %p\n", a->ipa, pa);

  send_read_reply(pocv2_msg_src_mac(msg), a->ipa, (void *)pa);
}

static void recv_read_reply_intr(struct pocv2_msg *msg) {
  struct read_reply_hdr *a = (struct read_reply_hdr *)msg->hdr;
  struct read_reply_body *b = msg->body;
  vmm_log("recv remote @%p page %p\n", a->ipa, b->page);

  vsm_set_cache_fast(a->ipa, b->page);
}

void vsm_node_init(struct memrange *mem) {
  u64 *vttbr = localnode.vttbr;
  u64 start = mem->start, size = mem->size;
  u64 p;

  for(p = 0; p < size; p += PAGESIZE) {
    u64 *pte = pagewalk(vttbr, start+p, 0);
    if(!pte || *pte == 0) {
      char *page = alloc_page();
      if(!page)
        panic("ram");

      pagemap(vttbr, start+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
    }
  }

  vmm_log("Node %d mapped: [%p - %p]\n", localnode.nodeid, start, start+p);
}

DEFINE_POCV2_MSG(MSG_READ, struct read_req_hdr, recv_read_request_intr);
DEFINE_POCV2_MSG(MSG_READ_REPLY, struct read_reply_hdr, recv_read_reply_intr);
