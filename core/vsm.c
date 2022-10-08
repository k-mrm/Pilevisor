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

static void send_fetch_request(u8 dst, u64 ipa, bool wnr);
static void send_fetch_reply(u8 *dst_mac, u64 ipa, void *page, bool wnr);

/* virtual shared memory */

/*
 *  memory fetch message
 *  read request: Node n1 ---> Node n2
 *    send
 *      - intermediate physical address(ipa)
 *
 *  read reply:   Node n1 <--- Node n2
 *    send
 *      - intermediate physical address(ipa)
 *      - 4KB page corresponding to ipa
 */

struct fetch_req_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 ipa;
  bool wnr;     // 0 read 1 write fetch
};

struct fetch_reply_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 ipa;
  u64 copyset;
  bool wnr;     // 0 read 1 write fetch
};

struct fetch_reply_body {
  u8 page[PAGESIZE];
};

static inline struct cache_page *ipa_to_page(u64 ipa) {
  if(!in_memrange(&cluster_me()->mem, ipa))
    panic("ipa_to_page");

  return pages + ((ipa - cluster_me()->mem.start) >> PAGESHIFT);
}

static inline void cache_page_lock(struct cache_page *p) {
  /* TODO */
  ;
}

static inline void cache_page_unlock(struct cache_page *p) {
  /* TODO */
  ;
}

/* determine manager's node of page by ipa */
static inline int page_manager(u64 ipa) {
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

static void vsm_set_cache_fast(u64 ipa_page, u8 *page) {
  static int count = 0;
  u64 *vttbr = localnode.vttbr;

  vmm_bug_on(!PAGE_ALIGNED(ipa_page), "pagealign");

  vmm_log("vsm: cache @%p %d\n", ipa_page, ++count);

  /* set access permission later */
  pagemap(vttbr, ipa_page, (u64)page, PAGESIZE, S2PTE_NORMAL);
}

/* read fault handler */
void *vsm_read_fetch_page(u64 page_ipa) {
  u64 *vttbr = localnode.vttbr;
  int owner = -1;
  int manager = -1;

  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  if(manager == localnode.nodeid) {   /* I am manager */
    /* receive page from owner of page */
    struct cache_page *page = ipa_to_page(page_ipa);
    owner = (page->flags >> CACHE_PAGE_OWNER_SHIFT) & CACHE_PAGE_OWNER_MASK;

    vmm_log("request remote read fetch!!!!: %p owner %d\n", page_ipa, owner);
    send_fetch_request(owner, page_ipa, 0);
  } else {
    /* ask manager for read access to page and a copy of page */

    vmm_log("request remote read fetch!!!!: %p manager %d\n", page_ipa, manager);
    send_fetch_request(manager, page_ipa, 0);
  }

  u64 *pte;
  while(!(pte = page_accessible_pte(vttbr, page_ipa)))
    wfi();

  pte_ro(pte);

  return (void *)PTE_PA(*pte);
}

/* write fault handler */
void *vsm_write_fetch_page(u64 page_ipa) {
  u64 *vttbr = localnode.vttbr;
  int owner = -1;
  int manager = -1;

  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  if(page_accessible(vttbr, page_ipa)) {
    panic("oioioioio %p", page_ipa);
  }

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  if(manager == localnode.nodeid) {   /* I am manager */
    /* receive page from owner of page */
    struct cache_page *page = ipa_to_page(page_ipa);
    owner = (page->flags >> CACHE_PAGE_OWNER_SHIFT) & CACHE_PAGE_OWNER_MASK;

    vmm_log("request remote write fetch!!!!: %p owner %d\n", page_ipa, owner);
    send_fetch_request(owner, page_ipa, 1);
  } else {
    /* ask manager for write access to page and a copy of page */

    vmm_log("request remote write fetch!!!!: %p manager %d\n", page_ipa, manager);
    send_fetch_request(manager, page_ipa, 1);
  }

  u64 *pte;
  while(!(pte = page_accessible_pte(vttbr, page_ipa)))
    wfi();

  pte_rw(pte);

  return (void *)PTE_PA(*pte);
}

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr) {
  if(!buf)
    panic("null buf");

  u64 page_ipa = PAGE_ADDRESS(ipa);
  char *pa_page;

  if(wr)
    pa_page = vsm_write_fetch_page(page_ipa);
  else
    pa_page = vsm_read_fetch_page(page_ipa);

  if(!pa_page)
    return -1;

  u32 offset = PAGE_OFFSET(ipa);
  if(wr)
    memcpy(pa_page+offset, buf, size);
  else
    memcpy(buf, pa_page+offset, size);

  return 0;
}

static void send_fetch_request(u8 dst, u64 ipa, bool wnr) {
  struct pocv2_msg msg;
  struct fetch_req_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = wnr;

  pocv2_msg_init2(&msg, dst, MSG_READ, &hdr, NULL, 0);

  send_msg(&msg);
}

static void send_fetch_reply(u8 *dst_mac, u64 ipa, void *page, bool wnr) {
  struct pocv2_msg msg;
  struct fetch_reply_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = wnr;

  pocv2_msg_init(&msg, dst_mac, MSG_READ_REPLY, &hdr, page, PAGESIZE);

  send_msg(&msg);
}

/* read/write server */
static void recv_fetch_request_intr(struct pocv2_msg *msg) {
  struct fetch_req_hdr *a = (struct fetch_req_hdr *)msg->hdr;

  /* TODO: use at instruction */
  u64 pa = ipa2pa(localnode.vttbr, a->ipa);
  vmm_log("%s fetch ipa @%p -> pa %p\n", a->wnr ? "write" : "read", a->ipa, pa);

  if(a->wnr)
    page_access_invalidate(localnode.vttbr, a->ipa);
  else
    page_access_ro(localnode.vttbr, a->ipa);

  send_fetch_reply(pocv2_msg_src_mac(msg), a->ipa, (void *)pa, a->wnr);
}

static void recv_fetch_reply_intr(struct pocv2_msg *msg) {
  struct fetch_reply_hdr *a = (struct fetch_reply_hdr *)msg->hdr;
  struct fetch_reply_body *b = msg->body;
  vmm_log("recv remote ipa %p ----> pa %p\n", a->ipa, b->page);

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

DEFINE_POCV2_MSG(MSG_READ, struct fetch_req_hdr, recv_fetch_request_intr);
DEFINE_POCV2_MSG(MSG_READ_REPLY, struct fetch_reply_hdr, recv_fetch_reply_intr);
