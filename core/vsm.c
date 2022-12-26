/*
 *  virtual shared memory
 */

#include "types.h"
#include "aarch64.h"
#include "arch-timer.h"
#include "pcpu.h"
#include "vsm.h"
#include "mm.h"
#include "s2mm.h"
#include "allocpage.h"
#include "malloc.h"
#include "log.h"
#include "lib.h"
#include "localnode.h"
#include "node.h"
#include "vcpu.h"
#include "msg.h"
#include "tlb.h"
#include "panic.h"
#include "assert.h"
#include "compiler.h"
#include "vsm-log.h"

#define ipa_to_pfn(ipa)       (((ipa) - 0x40000000) >> PAGESHIFT)
#define ipa_to_desc(ipa)      (&ptable[ipa_to_pfn(ipa)])

#define page_desc_addr(page)  ((((page) - ptable) << PAGESHIFT) + 0x40000000)

static struct manager_page manager[NR_MANAGER_PAGES];
static struct page_desc ptable[GVM_MEMORY / PAGESIZE];

static u64 w_copyset = 0;
static u64 w_roowner = 0;
static u64 w_inv = 0;

static const char *pte_state[4] = {
  [0]   "INV",
  [1]   " RO",
  [2]   " WO",
  [3]   " RW",
};

enum {
  READ_SERVER,
  WRITE_SERVER,
  INV_SERVER,
};

struct vsm_rw_data {
  u64 offset;
  char *buf;
  u64 size;
};

static void *__vsm_write_fetch_page(struct page_desc *page, struct vsm_rw_data *d);
static void *__vsm_read_fetch_page(struct page_desc *page, struct vsm_rw_data *d);
static void send_fetch_request(u8 req, u8 dst, u64 ipa, bool wnr);

static void vsm_read_server_process(struct vsm_server_proc *proc);
static void vsm_write_server_process(struct vsm_server_proc *proc);
static void vsm_invalidate_server_process(struct vsm_server_proc *proc);

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
  u8 from_nodeid;
};

struct invalidate_ack_hdr {
  POCV2_MSG_HDR_STRUCT;
  u64 ipa;
  u8 from_nodeid;
};

/*
 *  success: return 0
 *  else:    return 1 
 */
static inline int page_trylock(struct page_desc *page) {
  u8 *lock = &page->lock;
  u8 r, l = 1;

  asm volatile(
    "ldaxrb %w0, [%1]\n"
    "cbnz   %w0, 1f\n"
    "stxrb  %w0, %w2, [%1]\n"
    "1:\n"
    : "=&r"(r) : "r"(lock), "r"(l) : "memory"
  );

  return r;
}

static inline int page_locked(struct page_desc *page) {
  return page->lock;
}

static inline void page_spinlock(struct page_desc *page) {
  spin_lock(&page->lock);
}

static inline void page_unlock(struct page_desc *page) {
  u8 *lock = &page->lock;

  asm volatile("stlrb wzr, [%0]" :: "r"(lock) : "memory");
}

static struct vsm_server_proc *new_vsm_server_proc(u64 page_ipa, int req_nodeid, bool wnr) {
  struct vsm_server_proc *p = malloc(sizeof(*p));

  p->type = wnr ? WRITE_SERVER : READ_SERVER;
  p->page_ipa = page_ipa;
  p->req_nodeid = req_nodeid;
  p->do_process = wnr ? vsm_write_server_process : vsm_read_server_process;

  return p;
}

static struct vsm_server_proc *new_vsm_inv_server_proc(u64 page_ipa, int from_nodeid, u64 copyset) {
  struct vsm_server_proc *p = malloc(sizeof(*p));

  p->type = INV_SERVER;
  p->page_ipa = page_ipa;
  p->copyset = copyset;
  p->req_nodeid = from_nodeid;
  p->do_process = vsm_invalidate_server_process;

  return p;
}

static inline void invoke_vsm_process(struct vsm_server_proc *p) {
  p->do_process(p);
}

/*
 *  return value:
 *    0: nothing to do
 *    1: process enqueued server_proc myself
 */
static int vsm_enqueue_proc(struct vsm_server_proc *p) {
  int myself = 0;
  u64 flags;
  printf("%d enquuuuuuuuuuuuuuueueueeueueue %p %p %p\n", cpuid(), p, p->page_ipa, read_sysreg(elr_el2));

  struct page_desc *page = ipa_to_desc(p->page_ipa);

  if(!page->wq) {
    page->wq = malloc(sizeof(*page->wq));
    spinlock_init(&page->wq->lock);
  }

  spin_lock_irqsave(&page->wq->lock, flags);

  myself = page->wq->processing;

  if(page->wq->head == NULL)
    page->wq->head = p;

  if(page->wq->tail)
    page->wq->tail->next = p;

  page->wq->tail = p;

  spin_unlock_irqrestore(&page->wq->lock, flags);

  return myself;
}

static void vsm_process_wq_core(struct page_desc *page) {
  struct vsm_server_proc *p, *p_next, *head;
  assert(local_irq_disabled());

  spin_lock(&page->wq->lock);

restart:
  head = page->wq->head;

  page->wq->head = NULL;
  page->wq->tail = NULL;

  page->wq->processing = true;

  spin_unlock(&page->wq->lock);

  local_irq_enable();

  for(p = head; p; p = p_next) {
    printf("%d process %p(%d) %p................\n", cpuid(), p, p->type, p->page_ipa);
    invoke_vsm_process(p);

    p_next = p->next;
    free(p);
  }

  printf("%d processing doneeeeeeeeeeee\n", cpuid());

  local_irq_disable();

  spin_lock(&page->wq->lock);

  page->wq->processing = false;

  /*
   *  process enqueued processes during in this function
   */
  if(page->wq->head) {
    printf("restage!\n");
    goto restart;
  }

  spin_unlock(&page->wq->lock);
}

/*
 *  must be held ptable[ipa].lock
 */
static void vsm_process_waitqueue(struct page_desc *page) {
  u64 flags;

  assert(page_locked(page));

  irqsave(flags);

  if(page->wq && page->wq->head)
    vsm_process_wq_core(page);

  page_unlock(page);

  irqrestore(flags);
}

static void vsm_process_waitqueue_spinlock(struct page_desc *page) {
  u64 flags;

  page_spinlock(page);

  irqsave(flags);

  if(page->wq && page->wq->head)
    vsm_process_wq_core(page);

  page_unlock(page);

  irqrestore(flags);
}

static inline struct manager_page *ipa_manager_page(u64 ipa) {
  assert(in_memrange(&cluster_me()->mem, ipa));

  return manager + ((ipa - cluster_me()->mem.start) >> PAGESHIFT);
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

static inline u64 *vsm_wait_for_recv_timeout(u64 *vttbr, u64 page_ipa) {
  int timeout_us = 3000000;   // wait for 2s
  u64 *pte;

  while(!(pte = page_accessible_pte(vttbr, page_ipa)) && timeout_us--) {
    usleep(1);
  }

  if(unlikely(!pte))
    panic("vsm timeout: failed @%p", page_ipa);

  return pte;
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

  pagemap(node->vttbr, page_ipa, (u64)page, PAGESIZE, PTE_NORMAL|S2PTE_RW);
  
  vmm_log("dummy cache %p elr %p va %p\n", page_ipa, vcpu->reg.elr, vcpu->dabt.fault_va);

  return 0;
}
*/

static void vsm_set_cache_fast(u64 ipa_page, u8 *page, u64 copyset) {
  static int count = 0;
  u64 *vttbr = localvm.vttbr;

  vmm_bug_on(!PAGE_ALIGNED(ipa_page), "pagealign");

  // printf("vsm: cache @%p(%p) copyset: %p count%d\n", ipa_page, page, copyset, ++count);

  /* set access permission later */
  pagemap(vttbr, ipa_page, (u64)page, PAGESIZE, PTE_NORMAL|S2PTE_COPYSET(copyset));
}

/*
 *  already has ptable[ipa].lock
 */
static void vsm_invalidate(u64 ipa, u64 copyset) {
  if(copyset == 0)
    return;

  struct pocv2_msg msg;
  struct invalidate_hdr hdr;

  hdr.ipa = ipa;
  hdr.copyset = copyset;
  hdr.from_nodeid = local_nodeid();

  int node = 0;
  do {
    if((copyset & 1) && (node != local_nodeid())) {
      vsm_log(INV_SENDER, local_nodeid(), node, ipa, NULL);

      pocv2_msg_init2(&msg, node, MSG_INVALIDATE, &hdr, NULL, 0);

      send_msg(&msg);
    }

    copyset >>= 1;
    node++;
  } while(copyset);
}

static void send_invalidate_ack(int from_nodeid, u64 ipa) {
  struct pocv2_msg msg;
  struct invalidate_ack_hdr hdr;

  hdr.ipa = ipa;
  hdr.from_nodeid = from_nodeid;

  pocv2_msg_init2(&msg, from_nodeid, MSG_INVALIDATE_ACK, &hdr, NULL, 0);

  send_msg(&msg);
}

static void vsm_invalidate_server_process(struct vsm_server_proc *proc) {
  u64 ipa = proc->page_ipa;
  struct page_desc *page = ipa_to_desc(ipa);
  u64 copyset = proc->copyset;
  u64 from_nodeid = proc->req_nodeid;
  u64 *vttbr = localvm.vttbr;
  u64 *pte;

  assert(page_locked(page));

  if(!page_accessible(vttbr, ipa)) {
    // panic("invalidate already: %p", ipa);
    return;
  }

  if((pte = page_rwable_pte(vttbr, ipa)) != NULL ||
      (((pte = page_ro_pte(vttbr, ipa)) != NULL) && s2pte_copyset(pte) != 0)) {
    /* I'm already owner, ignore invalidate request */
    return;
  }

  vsm_log(INV_RECEIVER, from_nodeid, local_nodeid(), ipa, NULL);

  page_access_invalidate(vttbr, ipa);
}

void *vsm_read_fetch_page_imm(u64 page_ipa, u64 offset, char *buf, u64 size)  {
  struct page_desc *page = ipa_to_desc(page_ipa);

  struct vsm_rw_data d = {
    .offset = offset,
    .buf = buf,
    .size = size,
  };

  return __vsm_read_fetch_page(page, &d);
}

void *vsm_read_fetch_page(u64 page_ipa) {
  struct page_desc *page = ipa_to_desc(page_ipa);

  return __vsm_read_fetch_page(page, NULL);
}

/* read fault handler */
static void *__vsm_read_fetch_page(struct page_desc *page, struct vsm_rw_data *d) {
  u64 *vttbr = localvm.vttbr;
  u64 *pte;
  void *page_pa = NULL;
  u64 far = read_sysreg(far_el2);
  int manager = -1;
  u64 page_ipa = page_desc_addr(page);

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  page_spinlock(page);

  /*
   * may other cpu has readable page already
   */
  if((pte = s2_readable_pte(vttbr, page_ipa)) != NULL) {
    page_pa = (void *)PTE_PA(*pte);
    goto end;
  }

  if(manager == local_nodeid()) {   /* I am manager */
    /* receive page from owner of page */
    struct manager_page *p = ipa_manager_page(page_ipa);
    int owner = p->owner;

    vsm_log(READ_SENDER, local_nodeid(), owner, page_ipa, "request to owner");

    send_fetch_request(local_nodeid(), owner, page_ipa, 0);
  } else {
    /* ask manager for read access to page and a copy of page */
    vsm_log(READ_SENDER, local_nodeid(), manager, page_ipa, "request to manager");

    send_fetch_request(local_nodeid(), manager, page_ipa, 0);
  }

  pte = vsm_wait_for_recv_timeout(vttbr, page_ipa);

  page_pa = (void *)PTE_PA(*pte);

  /* read data */
  if(unlikely(d))
    memcpy(d->buf, (char *)page_pa + d->offset, d->size);

  s2pte_ro(pte);
  tlb_s2_flush_all();

end:
  vsm_process_waitqueue(page);

  return page_pa;
}

void *vsm_write_fetch_page_imm(u64 page_ipa, u64 offset, char *buf, u64 size) {
  struct page_desc *page = ipa_to_desc(page_ipa);

  struct vsm_rw_data d = {
    .offset = offset,
    .buf = buf,
    .size = size,
  };

  return __vsm_write_fetch_page(page, &d);
}

void *vsm_write_fetch_page(u64 page_ipa) {
  struct page_desc *page = ipa_to_desc(page_ipa);

  return __vsm_write_fetch_page(page, NULL);
}

/* write fault handler */
static void *__vsm_write_fetch_page(struct page_desc *page, struct vsm_rw_data *d) {
  u64 *vttbr = localvm.vttbr;
  u64 *pte;
  void *page_pa;
  u64 far = read_sysreg(far_el2);
  int manager = -1;
  u64 page_ipa = page_desc_addr(page);

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  page_spinlock(page);

  /*
   * may other cpu has readable/writable page already
   */
  if((pte = page_rwable_pte(vttbr, page_ipa)) != NULL) {
    page_pa = (void *)PTE_PA(*pte);
    goto end;
  }

  if(!local_irq_enabled())
    panic("bug: local irq disabled");

  if((pte = page_ro_pte(vttbr, page_ipa)) != NULL) {
    if(s2pte_copyset(pte) != 0) {
      /* I am owner */
      vsm_log(WRITE_SENDER, -1, -1, page_ipa, "write to owner ro page");
      goto inv_phase;
    }

    /*
     *  TODO: no need to fetch page from remote node
     */
    vsm_log(WRITE_SENDER, -1, -1, page_ipa, "write to copyset");

    void *pa = (void *)PTE_PA(*pte);

    s2pte_invalidate(pte);
    tlb_s2_flush_all();

    free_page(pa);
  }

  if(manager == local_nodeid()) {   /* I am manager */
    /* receive page from owner of page */
    struct manager_page *page = ipa_manager_page(page_ipa);
    int owner = page->owner;

    vsm_log(WRITE_SENDER, local_nodeid(), owner, page_ipa, "request to owner");

    send_fetch_request(local_nodeid(), owner, page_ipa, 1);
  } else {
    /* ask manager for write access to page and a copy of page */
    vsm_log(WRITE_SENDER, local_nodeid(), manager, page_ipa, "request to manager");

    send_fetch_request(local_nodeid(), manager, page_ipa, 1);
  }

  pte = vsm_wait_for_recv_timeout(vttbr, page_ipa);

  // vmm_log("wf: get remote page @%p\n", page_ipa);

inv_phase:
  /* invalidate request to copyset */
  vsm_invalidate(page_ipa, s2pte_copyset(pte));

  s2pte_clear_copyset(pte);

  page_pa = (void *)PTE_PA(*pte);

  /* write data */
  if(unlikely(d))
    memcpy((char *)page_pa + d->offset, d->buf, d->size);

  s2pte_rw(pte);
  // tlb_s2_flush_all();

end:
  vsm_process_waitqueue(page);

  return page_pa;
}

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr) {
  if(!buf)
    panic("null buf");

  u64 page_ipa = PAGE_ADDRESS(ipa);
  u64 offset = PAGE_OFFSET(ipa);
  char *pa_page;

  if(wr)
    pa_page = vsm_write_fetch_page_imm(page_ipa, offset, buf, size);
  else
    pa_page = vsm_read_fetch_page_imm(page_ipa, offset, buf, size);

  return pa_page ? 0 : -1;
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
static void vsm_read_server_process(struct vsm_server_proc *proc) {
  u64 *vttbr = localvm.vttbr;
  u64 page_ipa = proc->page_ipa;
  struct page_desc *page = ipa_to_desc(page_ipa);
  int req_nodeid = proc->req_nodeid;
  u64 *pte;

  assert(page_locked(page));

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

    vsm_log(READ_RECEIVER, req_nodeid, local_nodeid(), page_ipa, "I am owner");

    /* send p */
    send_read_fetch_reply(req_nodeid, page_ipa, (void *)pa);
  } else if(local_nodeid() == manager) {  /* I am manager */
    struct manager_page *p = ipa_manager_page(page_ipa);
    int p_owner = p->owner;

    vsm_log(READ_RECEIVER, req_nodeid, p_owner, page_ipa, "forward read request");

    if(req_nodeid == p_owner)
      panic("read server: req_nodeid(%d) == p_owner(%d)", req_nodeid, p_owner);

    /* forward request to p's owner */
    send_fetch_request(req_nodeid, p_owner, page_ipa, 0);
  } else {
    printf("read server: read %p (manager %d) from Node %d", page_ipa, manager, req_nodeid);
    panic("unreachable");
  }
}

/* write server */
static void vsm_write_server_process(struct vsm_server_proc *proc) {
  u64 *vttbr = localvm.vttbr;
  u64 page_ipa = proc->page_ipa;
  struct page_desc *page = ipa_to_desc(page_ipa);
  int req_nodeid = proc->req_nodeid;
  u64 *pte;

  assert(page_locked(page));

  int manager = page_manager(page_ipa);
  if(manager < 0)
    panic("dare w");

  if((pte = page_rwable_pte(vttbr, page_ipa)) != NULL ||
      (((pte = page_ro_pte(vttbr, page_ipa)) != NULL) && s2pte_copyset(pte) != 0)) {
    /* I am owner */
    void *pa = (void *)PTE_PA(*pte);
    u64 copyset = s2pte_copyset(pte);

    vsm_log(WRITE_RECEIVER, req_nodeid, local_nodeid(), page_ipa, "I am owner!");

    s2pte_invalidate(pte);
    tlb_s2_flush_all();

    // send p and copyset;
    send_write_fetch_reply(req_nodeid, page_ipa, pa, copyset);

    free_page(pa);

    if(local_nodeid() == manager) {
      struct manager_page *p = ipa_manager_page(page_ipa);

      p->owner = req_nodeid;
    }
  } else if(local_nodeid() == manager) {
    struct manager_page *p = ipa_manager_page(page_ipa);
    int p_owner = p->owner;

    vsm_log(WRITE_RECEIVER, req_nodeid, p_owner, page_ipa, "forward write request");

    if(req_nodeid == p_owner)
      panic("write server: req_nodeid(%d) == p_owner(%d) fetch request from owner!",
              req_nodeid, p_owner);

    /* forward request to p's owner */
    send_fetch_request(req_nodeid, p_owner, page_ipa, 1);

    /* now owner is request node */
    p->owner = req_nodeid;
  } else {
    panic("write server: %p (manager %d) %d unreachable", page_ipa, manager, req_nodeid);
  }
}

static void recv_fetch_request_intr(struct pocv2_msg *msg) {
  struct fetch_req_hdr *a = (struct fetch_req_hdr *)msg->hdr;
  struct vsm_server_proc *p = new_vsm_server_proc(a->ipa, a->req_nodeid, a->wnr);

  assert(!local_lazyirq_enabled());

  struct page_desc *page = ipa_to_desc(a->ipa);

  if(page_trylock(page)) {
    int proc_myself = vsm_enqueue_proc(p);

    if(proc_myself)
      goto end;
    else
      return;
  }

  invoke_vsm_process(p);
  free(p);

end:
  vsm_process_waitqueue(page);
}

static void recv_invalidate_intr(struct pocv2_msg *msg) {
  struct invalidate_hdr *h = (struct invalidate_hdr *)msg->hdr;
  struct vsm_server_proc *p = new_vsm_inv_server_proc(h->ipa, h->from_nodeid, h->copyset);

  assert(!local_lazyirq_enabled());

  struct page_desc *page = ipa_to_desc(h->ipa);

  if(page_trylock(page)) {
    int proc_myself = vsm_enqueue_proc(p);

    if(proc_myself)
      goto end;
    else
      return;
  }

  invoke_vsm_process(p); 
  free(p);

end:
  vsm_process_waitqueue(page);
}

static void recv_fetch_reply_intr(struct pocv2_msg *msg) {
  struct fetch_reply_hdr *a = (struct fetch_reply_hdr *)msg->hdr;
  struct fetch_reply_body *b = msg->body;
  // vmm_log("recv remote ipa %p ----> pa %p\n", a->ipa, b->page);

  vsm_set_cache_fast(a->ipa, b->page, a->copyset);
}

void vsm_node_init(struct memrange *mem) {
  u64 *vttbr = localvm.vttbr;
  u64 start = mem->start, size = mem->size;
  u64 p;

  for(p = 0; p < size; p += PAGESIZE) {
    char *page = alloc_page();
    if(!page)
      panic("ram");

    pagemap(vttbr, start+p, (u64)page, PAGESIZE, PTE_NORMAL|S2PTE_RW);
  }

  vmm_log("Node %d mapped: [%p - %p]\n", local_nodeid(), start, start+p);

  struct manager_page *page;
  for(page = manager; page < &manager[NR_MANAGER_PAGES]; page++) {
    /* now owner is me */
    page->owner = local_nodeid();
  }
}

DEFINE_POCV2_MSG(MSG_FETCH, struct fetch_req_hdr, recv_fetch_request_intr);
DEFINE_POCV2_MSG(MSG_FETCH_REPLY, struct fetch_reply_hdr, recv_fetch_reply_intr);
DEFINE_POCV2_MSG(MSG_INVALIDATE, struct invalidate_hdr, recv_invalidate_intr);
DEFINE_POCV2_MSG(MSG_INVALIDATE_ACK, struct invalidate_ack_hdr, NULL);
