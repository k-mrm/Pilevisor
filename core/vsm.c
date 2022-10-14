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

void *vsm_read_fetch_page(u64 page_ipa);
void *vsm_write_fetch_page(u64 page_ipa);
static void send_fetch_request(u8 req, u8 dst, u64 ipa, bool wnr);

static struct cache_page pages[NR_CACHE_PAGES];


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
  u8 req_nodeid;
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

struct invalidate_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 ipa;
  u64 copyset;
};

static inline struct cache_page *ipa_cache_page(u64 ipa) {
  if(!in_memrange(&cluster_me()->mem, ipa))
    panic("ipa_cache_page");

  return pages + ((ipa - cluster_me()->mem.start) >> PAGESHIFT);
}

static inline void cache_page_set_owner(struct cache_page *p, int owner) {
  p->flags &= ~((u64)CACHE_PAGE_OWNER_MASK << CACHE_PAGE_OWNER_SHIFT);
  p->flags |= ((u64)owner & CACHE_PAGE_OWNER_MASK) << CACHE_PAGE_OWNER_SHIFT;
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
}
*/

static void vsm_set_cache_fast(u64 ipa_page, u8 *page, u64 copyset) {
  static int count = 0;
  u64 *vttbr = localnode.vttbr;

  vmm_bug_on(!PAGE_ALIGNED(ipa_page), "pagealign");

  vmm_log("vsm: cache @%p %d\n", ipa_page, ++count);

  /* set access permission later */
  pagemap(vttbr, ipa_page, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_COPYSET(copyset));
}

static void vsm_invalidate(u64 ipa, u64 copyset) {
  if(copyset == 0)
    return;

  struct pocv2_msg msg;
  struct invalidate_hdr hdr;

  hdr.ipa = ipa;
  hdr.copyset = copyset;

  pocv2_broadcast_msg_init(&msg, MSG_INVALIDATE, &hdr, NULL, 0);

  send_msg(&msg);
}

static void vsm_invalidate_server(u64 ipa, u64 copyset) {
  if(!(copyset & (1 << localnode.nodeid)))
    return;

  page_access_invalidate(localnode.vttbr, ipa);
}

/* read fault handler */
void *vsm_read_fetch_page(u64 page_ipa) {
  u64 *vttbr = localnode.vttbr;
  u64 *pte;
  int manager = -1;

  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  if(manager == localnode.nodeid) {   /* I am manager */
    /* receive page from owner of page */
    struct cache_page *p = ipa_cache_page(page_ipa);
    int owner = CACHE_PAGE_OWNER(p);

    vmm_log("request remote read fetch!!!!: %p owner %d\n", page_ipa, owner);
    send_fetch_request(localnode.nodeid, owner, page_ipa, 0);
  } else {
    /* ask manager for read access to page and a copy of page */
    vmm_log("request remote read fetch!!!!: %p manager %d\n", page_ipa, manager);
    send_fetch_request(localnode.nodeid, manager, page_ipa, 0);
  }

  while(!(pte = page_accessible_pte(vttbr, page_ipa)))
    wfi();

  s2pte_ro(pte);

  return (void *)PTE_PA(*pte);
}

/* write fault handler */
void *vsm_write_fetch_page(u64 page_ipa) {
  u64 *vttbr = localnode.vttbr;
  u64 *pte;
  int manager = -1;

  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  if((pte = page_accessible_pte(vttbr, page_ipa)) != NULL) {
    s2pte_invalidate(pte);
  }

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  if(manager == localnode.nodeid) {   /* I am manager */
    /* receive page from owner of page */
    struct cache_page *page = ipa_cache_page(page_ipa);
    int owner = CACHE_PAGE_OWNER(page);

    vmm_log("request remote write fetch!!!!: %p owner %d\n", page_ipa, owner);
    send_fetch_request(localnode.nodeid, owner, page_ipa, 1);
  } else {
    /* ask manager for write access to page and a copy of page */
    vmm_log("request remote write fetch!!!!: %p manager %d\n", page_ipa, manager);
    send_fetch_request(localnode.nodeid, manager, page_ipa, 1);
  }

  while(!(pte = page_accessible_pte(vttbr, page_ipa)))
    wfi();

  /* invalidate request to copyset */
  vsm_invalidate(page_ipa, s2pte_copyset(pte));
  s2pte_clear_copyset(pte);
  s2pte_rw(pte);

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

/*
 *  @req: request nodeid
 *  @dst: fetch request destination
 */
static void send_fetch_request(u8 req, u8 dst, u64 ipa, bool wnr) {
  struct pocv2_msg msg;
  struct fetch_req_hdr hdr;

  hdr.ipa = ipa;
  hdr.req_nodeid = req;
  hdr.wnr = wnr;

  pocv2_msg_init2(&msg, dst, MSG_FETCH, &hdr, NULL, 0);

  send_msg(&msg);
}

static void send_read_fetch_reply(u8 dst_nodeid, u64 ipa, void *page) {
  struct pocv2_msg msg;
  struct fetch_reply_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = 0;
  hdr.copyset = 0;

  pocv2_msg_init2(&msg, dst_nodeid, MSG_FETCH_REPLY, &hdr, page, PAGESIZE);

  send_msg(&msg);
}

static void send_write_fetch_reply(u8 dst_nodeid, u64 ipa, void *page, u64 copyset) {
  struct pocv2_msg msg;
  struct fetch_reply_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = 1;
  hdr.copyset = copyset;

  pocv2_msg_init2(&msg, dst_nodeid, MSG_FETCH_REPLY, &hdr, page, PAGESIZE);

  send_msg(&msg);
}

/* read server */
static void vsm_readpage_server(u64 ipa_page, int req_nodeid) {
  u64 *vttbr = localnode.vttbr;
  u64 *pte;
  int manager = page_manager(ipa_page);
  if(manager < 0)
    panic("dare");

  if((pte = page_rwable_pte(vttbr, ipa_page)) != NULL ||
      (((pte = page_ro_pte(vttbr, ipa_page)) != NULL) && s2pte_copyset(pte) != 0)) {
    /* I am owner */
    u64 pa = PTE_PA(*pte);
    /* copyset = copyset | request node */
    s2pte_add_copyset(pte, req_nodeid);

    s2pte_ro(pte);

    /* send p */
    send_read_fetch_reply(req_nodeid, ipa_page, (void *)pa);
  } else if(localnode.nodeid == manager) {  /* I am manager */
    struct cache_page *p = ipa_cache_page(ipa_page);
    int p_owner = CACHE_PAGE_OWNER(p);

    vmm_log("forward read fetch request to %d (%p)\n", p_owner, ipa_page);
    /* forward request to p's owner */
    send_fetch_request(req_nodeid, p_owner, ipa_page, 0);
  } else {
    panic("unreachable");
  }
}

/* write server */
static void vsm_writepage_server(u64 ipa_page, int req_nodeid) {
  u64 *vttbr = localnode.vttbr;
  u64 *pte;
  int manager = page_manager(ipa_page);
  if(manager < 0)
    panic("dare w");

  if((pte = page_rwable_pte(vttbr, ipa_page)) != NULL ||
      (((pte = page_ro_pte(vttbr, ipa_page)) != NULL) && s2pte_copyset(pte) != 0)) {
    /* I am owner */
    u64 pa = PTE_PA(*pte);

    // send p and copyset;
    send_write_fetch_reply(req_nodeid, ipa_page, (void *)pa, s2pte_copyset(pte));

    s2pte_invalidate(pte);

    if(localnode.nodeid == manager) {
      struct cache_page *p = ipa_cache_page(ipa_page);
      cache_page_set_owner(p, req_nodeid);
    }
  } else if(localnode.nodeid == manager) {
    struct cache_page *p = ipa_cache_page(ipa_page);
    int p_owner = CACHE_PAGE_OWNER(p);

    vmm_log("forward write fetch request to %d (%p)\n", p_owner, ipa_page);
    /* forward request to p's owner */
    send_fetch_request(req_nodeid, p_owner, ipa_page, 1);

    /* now owner is request node */
    cache_page_set_owner(p, req_nodeid);
  } else {
    panic("unreachable");
  }
}

static void recv_fetch_request_intr(struct pocv2_msg *msg) {
  struct fetch_req_hdr *a = (struct fetch_req_hdr *)msg->hdr;

  if(a->wnr)
    vsm_writepage_server(a->ipa, a->req_nodeid);
  else
    vsm_readpage_server(a->ipa, a->req_nodeid);
}

static void recv_fetch_reply_intr(struct pocv2_msg *msg) {
  struct fetch_reply_hdr *a = (struct fetch_reply_hdr *)msg->hdr;
  struct fetch_reply_body *b = msg->body;
  vmm_log("recv remote ipa %p ----> pa %p\n", a->ipa, b->page);

  vsm_set_cache_fast(a->ipa, b->page, a->copyset);
}

static void recv_invalidate_intr(struct pocv2_msg *msg) {
  struct invalidate_hdr *h = (struct invalidate_hdr *)msg->hdr;

  vsm_invalidate_server(h->ipa, h->copyset);
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

  struct cache_page *c;
  for(c = pages; c < &pages[NR_CACHE_PAGES]; c++) {
    /* now owner is me */
    c->flags = ((u64)localnode.nodeid & CACHE_PAGE_OWNER_MASK) << CACHE_PAGE_OWNER_SHIFT;
  }
}

DEFINE_POCV2_MSG(MSG_FETCH, struct fetch_req_hdr, recv_fetch_request_intr);
DEFINE_POCV2_MSG(MSG_FETCH_REPLY, struct fetch_reply_hdr, recv_fetch_reply_intr);
DEFINE_POCV2_MSG(MSG_INVALIDATE, struct invalidate_hdr, recv_invalidate_intr);
