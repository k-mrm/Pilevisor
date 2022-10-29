/*
 *  virtual shared memory
 */

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
#include "tlb.h"

#define ipa_to_pfn(ipa)  ((ipa - 0x40000000) >> PAGESHIFT)

void *vsm_read_fetch_page(u64 page_ipa);
void *vsm_write_fetch_page(u64 page_ipa);
static void send_fetch_request(u8 req, u8 dst, u64 ipa, bool wnr);

static int vsm_read_server_process(struct vsm_server_proc *proc);
static int vsm_write_server_process(struct vsm_server_proc *proc);
static int vsm_invalidate_server_process(struct vsm_server_proc *proc);

static struct cache_page pages[NR_CACHE_PAGES];
static struct vsm_server_proc ptable[128];

static u8 page_lock[256*1024*1024 / PAGESIZE];

static char *pte_state[4] = {
  [0]   "INV",
  [1]   " RO",
  [2]   " WO",
  [3]   " RW",
};

enum {
  READ_SERVER,
  WRITE_SERVER,
  INVALIDATE_SERVER,
};

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

static struct vsm_server_proc *allocvsp() {
  struct vsm_server_proc *p;

  for(p = ptable; p < &ptable[128]; p++) {
    if(!p->used) {
      p->used = 1;
      p->next = NULL;
      return p;
    }
  }

  panic("no process");
}

static struct vsm_server_proc *new_vsm_server_proc(u64 page_ipa, int req_nodeid, bool wnr) {
  struct vsm_server_proc *p = allocvsp();

  p->type = wnr ? WRITE_SERVER : READ_SERVER;
  p->page_ipa = page_ipa;
  p->req_nodeid = req_nodeid;
  p->do_process = wnr ? vsm_write_server_process : vsm_read_server_process;

  return p;
}

static struct vsm_server_proc *new_vsm_invalidate_server_proc(u64 page_ipa, u64 copyset) {
  struct vsm_server_proc *p = allocvsp();

  p->type = INVALIDATE_SERVER;
  p->page_ipa = page_ipa;
  p->copyset = copyset;
  p->do_process = vsm_invalidate_server_process;

  return p;
}

static void free_vsm_server_proc(struct vsm_server_proc *p) {
  if(!p)
    panic("free vsm server process");

  p->used = 0;
}

static void vsm_enqueue_proc(struct vsm_server_proc *p) {
  u64 flags;
  vmm_log("enquuuuuuuuuuuuuuueueueeueueue %p %d\n", p, p->type);

  spin_lock_irqsave(&current->vsm_waitqueue.lk, flags);

  if(current->vsm_waitqueue.head == NULL)
    current->vsm_waitqueue.head = p;

  if(current->vsm_waitqueue.tail)
    current->vsm_waitqueue.tail->next = p;

  current->vsm_waitqueue.tail = p;

  spin_unlock_irqrestore(&current->vsm_waitqueue.lk, flags);
}

static void vsm_process_waitqueue() {
  struct vsm_server_proc *p, *p_next, *head;

  if(!current->vsm_waitqueue.head)
    return;

  head = current->vsm_waitqueue.head;

  current->vsm_waitqueue.head = NULL;
  current->vsm_waitqueue.tail = NULL;

  for(p = head; p != NULL; p = p_next) {
    if(p->do_process(p) < 0)
      continue;
    p_next = p->next;
    free_vsm_server_proc(p);
  }

  /*
   *  process enqueued processes during in this function
   */
  vsm_process_waitqueue();
}

static inline struct cache_page *ipa_cache_page(u64 ipa) {
  if(!in_memrange(&cluster_me()->mem, ipa))
    panic("ipa_cache_page");

  return pages + ((ipa - cluster_me()->mem.start) >> PAGESHIFT);
}

static inline void cache_page_set_owner(struct cache_page *p, int owner) {
  p->flags &= ~((u64)CACHE_PAGE_OWNER_MASK << CACHE_PAGE_OWNER_SHIFT);
  p->flags |= ((u64)owner & CACHE_PAGE_OWNER_MASK) << CACHE_PAGE_OWNER_SHIFT;
}

/*
 *  success: return 0
 */
static inline int page_trylock(u64 ipa) {
  u8 *lock = &page_lock[ipa_to_pfn(ipa)];
  vmm_log("trylock %p (%p) %p\n", ipa, ipa_to_pfn(ipa), read_sysreg(elr_el2));
  u8 r, l = 1;
  u64 flag = read_sysreg(daif);

  local_irq_disable();

  asm volatile(
    "ldaxrb %w0, [%1]\n"
    "cbnz   %w0, 1f\n"
    "stxrb  %w0, %w2, [%1]\n"
    "1:\n"
    : "=&r"(r) : "r"(lock), "r"(l) : "memory"
  );

  write_sysreg(daif, flag);

  return r;
}

static inline void page_unlock(u64 ipa) {
  u8 *lock = &page_lock[ipa_to_pfn(ipa)];

  asm volatile("stlrb wzr, [%0]" :: "r"(lock) : "memory");
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

  vmm_log("vsm: cache @%p copyset: %p count%d\n", ipa_page, copyset, ++count);

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

  int node = 0;
  while(copyset != 0) {
    if(copyset & 1 && node != localnode.nodeid) {
      vmm_log("send invalidate msg to Node %d\n", node);

      pocv2_msg_init2(&msg, node, MSG_INVALIDATE, &hdr, NULL, 0);

      send_msg(&msg);
    }
    copyset >>= 1;
    node++;
  }
}

static int vsm_invalidate_server_process(struct vsm_server_proc *proc) {
  u64 ipa = proc->page_ipa;
  u64 copyset = proc->copyset;

  if(page_trylock(ipa)) {
    vsm_enqueue_proc(proc);
    return -1;
  }

  u64 *vttbr = localnode.vttbr;

  vmm_log("Node %d: access invalidate %p %d\n", localnode.nodeid, ipa, page_accessible(vttbr, ipa));

  page_access_invalidate(vttbr, ipa);

  page_unlock(ipa);

  return 0;
}

/* read fault handler */
void *vsm_read_fetch_page(u64 page_ipa) {
  u64 *vttbr = localnode.vttbr;
  u64 *pte;
  void *page_pa = NULL;
  u64 far = read_sysreg(far_el2);
  int manager = -1;

  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  if(page_trylock(page_ipa))
    panic("rf: locked %p", page_ipa);

  if(manager == localnode.nodeid) {   /* I am manager */
    /* receive page from owner of page */
    struct cache_page *p = ipa_cache_page(page_ipa);
    int owner = CACHE_PAGE_OWNER(p);

    vmm_log("rf: request remote read fetch!!!!: %p owner %d elr %p far %p\n", page_ipa, owner, current->reg.elr, far);
    send_fetch_request(localnode.nodeid, owner, page_ipa, 0);
  } else {
    /* ask manager for read access to page and a copy of page */
    vmm_log("rf: request remote read fetch!!!!: %p manager %d elr %p far %p\n", page_ipa, manager, current->reg.elr, far);
    send_fetch_request(localnode.nodeid, manager, page_ipa, 0);
  }

  while(!(pte = page_accessible_pte(vttbr, page_ipa)))
    wfi();

  s2pte_ro(pte);
  tlb_s2_flush_all();

  page_pa = (void *)PTE_PA(*pte);

  page_unlock(page_ipa);
  vsm_process_waitqueue();

  return page_pa;
}

/* write fault handler */
void *vsm_write_fetch_page(u64 page_ipa) {
  u64 *vttbr = localnode.vttbr;
  u64 *pte;
  void *pa_page = NULL;
  u64 far = read_sysreg(far_el2);
  int manager = -1;

  vmm_bug_on(!PAGE_ALIGNED(page_ipa), "page_ipa align");

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  if(page_trylock(page_ipa))
    panic("wf: locked %p", page_ipa);

  if((pte = page_ro_pte(vttbr, page_ipa)) != NULL) {
    if(s2pte_copyset(pte) != 0) {
      /* I am owner */
      vmm_log("wf: write to owner ro page %p elr %p far %p\n", page_ipa, current->reg.elr, far);
      goto inv_phase;
    }

    /*
     *  TODO: Maybe no need to fetch page from remote node
     */
    vmm_log("wf: write to ro page(copyset) %p elr %p far %p\n", page_ipa, current->reg.elr, far);
    s2pte_invalidate(pte);
    tlb_s2_flush_all();
  }

  if(manager == localnode.nodeid) {   /* I am manager */
    /* receive page from owner of page */
    struct cache_page *page = ipa_cache_page(page_ipa);
    int owner = CACHE_PAGE_OWNER(page);

    vmm_log("wf: request remote write fetch!!!!: %p owner %d elr %p far %p\n", page_ipa, owner, current->reg.elr, far);
    send_fetch_request(localnode.nodeid, owner, page_ipa, 1);
  } else {
    /* ask manager for write access to page and a copy of page */
    vmm_log("wf: request remote write fetch!!!!: %p manager %d elr %p far %p\n", page_ipa, manager, current->reg.elr, far);
    send_fetch_request(localnode.nodeid, manager, page_ipa, 1);
  }

  while(!(pte = page_accessible_pte(vttbr, page_ipa)))
    wfi();

  vmm_log("wf: get remote page @%p\n", page_ipa);

inv_phase:
  /* invalidate request to copyset */
  vsm_invalidate(page_ipa, s2pte_copyset(pte));
  s2pte_clear_copyset(pte);
  s2pte_rw(pte);

  pa_page = (void *)PTE_PA(*pte);

  page_unlock(page_ipa);
  vsm_process_waitqueue();

  return pa_page;
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
  
  vmm_log("vsm: read fetch reply %p\n", ipa);

  pocv2_msg_init2(&msg, dst_nodeid, MSG_FETCH_REPLY, &hdr, page, PAGESIZE);

  send_msg(&msg);
}

static void send_write_fetch_reply(u8 dst_nodeid, u64 ipa, void *page, u64 copyset) {
  struct pocv2_msg msg;
  struct fetch_reply_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = 1;
  hdr.copyset = copyset;

  vmm_log("vsm: write fetch reply %p\n", ipa);

  pocv2_msg_init2(&msg, dst_nodeid, MSG_FETCH_REPLY, &hdr, page, PAGESIZE);

  send_msg(&msg);
}

/* read server */
static int vsm_read_server_process(struct vsm_server_proc *proc) {
  u64 *vttbr = localnode.vttbr;
  u64 page_ipa = proc->page_ipa;
  int req_nodeid = proc->req_nodeid;
  u64 *pte;

  if(page_trylock(page_ipa)) {
    vsm_enqueue_proc(proc);
    return -1;
  }

  int manager = page_manager(page_ipa);
  if(manager < 0)
    panic("dare");

  if((pte = page_rwable_pte(vttbr, page_ipa)) != NULL ||
      (((pte = page_ro_pte(vttbr, page_ipa)) != NULL) && s2pte_copyset(pte) != 0)) {
    s2pte_ro(pte);
    tlb_s2_flush_all();

    /* copyset = copyset | request node */
    s2pte_add_copyset(pte, req_nodeid);

    /* I am owner */
    u64 pa = PTE_PA(*pte);

    vmm_log("read server: send read fetch reply: i am owner! c: %p\n", s2pte_copyset(pte));

    /* send p */
    send_read_fetch_reply(req_nodeid, page_ipa, (void *)pa);
  } else if(localnode.nodeid == manager) {  /* I am manager */
    struct cache_page *p = ipa_cache_page(page_ipa);
    int p_owner = CACHE_PAGE_OWNER(p);

    vmm_log("read server: forward read fetch request to %d (%p)\n", p_owner, page_ipa);

    if(req_nodeid == p_owner)
      panic("read server: req_nodeid(%d) == p_owner(%d)", req_nodeid, p_owner);

    /* forward request to p's owner */
    send_fetch_request(req_nodeid, p_owner, page_ipa, 0);
  } else {
    panic("read server: read %p %d unreachable", page_ipa, req_nodeid);
  }

  page_unlock(page_ipa);

  return 0;
}

/* write server */
static int vsm_write_server_process(struct vsm_server_proc *proc) {
  u64 *vttbr = localnode.vttbr;
  u64 page_ipa = proc->page_ipa;
  int req_nodeid = proc->req_nodeid;
  u64 *pte;

  if(page_trylock(page_ipa)) {
    vsm_enqueue_proc(proc);
    return -1;
  }

  int manager = page_manager(page_ipa);
  if(manager < 0)
    panic("dare w");

  if((pte = page_rwable_pte(vttbr, page_ipa)) != NULL ||
      (((pte = page_ro_pte(vttbr, page_ipa)) != NULL) && s2pte_copyset(pte) != 0)) {
    /* I am owner */
    u64 pa = PTE_PA(*pte);
    u64 copyset = s2pte_copyset(pte);

    vmm_log("write server: send write fetch reply: i am owner! %p\n", s2pte_copyset(pte));

    s2pte_invalidate(pte);
    tlb_s2_flush_all();

    // send p and copyset;
    send_write_fetch_reply(req_nodeid, page_ipa, (void *)pa, copyset);

    if(localnode.nodeid == manager) {
      struct cache_page *p = ipa_cache_page(page_ipa);
      cache_page_set_owner(p, req_nodeid);
    }
  } else if(localnode.nodeid == manager) {
    struct cache_page *p = ipa_cache_page(page_ipa);
    int p_owner = CACHE_PAGE_OWNER(p);

    vmm_log("write server: forward write fetch request to %d (%p)\n", p_owner, page_ipa);

    if(req_nodeid == p_owner)
      panic("write server: req_nodeid(%d) == p_owner(%d)", req_nodeid, p_owner);

    /* forward request to p's owner */
    send_fetch_request(req_nodeid, p_owner, page_ipa, 1);

    /* now owner is request node */
    cache_page_set_owner(p, req_nodeid);
  } else {
    panic("write server: %p %d unreachable", page_ipa, req_nodeid);
  }

  page_unlock(page_ipa);

  return 0;
}

static void recv_fetch_request_intr(struct pocv2_msg *msg) {
  struct fetch_req_hdr *a = (struct fetch_req_hdr *)msg->hdr;
  struct vsm_server_proc *p = new_vsm_server_proc(a->ipa, a->req_nodeid, a->wnr);

  if(p->do_process(p) < 0) {
    return;
  }

  free_vsm_server_proc(p);
  vsm_process_waitqueue();
}

static void recv_fetch_reply_intr(struct pocv2_msg *msg) {
  struct fetch_reply_hdr *a = (struct fetch_reply_hdr *)msg->hdr;
  struct fetch_reply_body *b = msg->body;
  vmm_log("recv remote ipa %p ----> pa %p\n", a->ipa, b->page);

  vsm_set_cache_fast(a->ipa, b->page, a->copyset);
}

static void recv_invalidate_intr(struct pocv2_msg *msg) {
  struct invalidate_hdr *h = (struct invalidate_hdr *)msg->hdr;
  struct vsm_server_proc *p = new_vsm_invalidate_server_proc(h->ipa, h->copyset);

  if(p->do_process(p) < 0)
    return;

  free_vsm_server_proc(p);
  vsm_process_waitqueue();
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
