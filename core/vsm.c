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
#include "memlayout.h"
#include "cache.h"

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
  u8 r, l = cpuid() + 1;

  vmm_log("%p page trylock\n", page_desc_addr(page));

  asm volatile(
    "ldaxrb %w0, [%1]\n"
    "cbnz   %w0, 1f\n"
    "stxrb  %w0, %w2, [%1]\n"
    "1:\n"
    : "=&r"(r) : "r"(lock), "r"(l) : "memory"
  );

  return r;
}

static inline bool page_locked(struct page_desc *page) {
  return !!page->lock;
}

static inline void page_spinlock(struct page_desc *page) {
  u8 *lock = &page->lock;
  u8 r, l = cpuid() + 1;

  asm volatile(
    "sevl\n"
    "1: wfe\n"
    "2: ldaxrb %w0, [%1]\n"
    "cbnz   %w0, 1b\n"
    "stxrb  %w0, %w2, [%1]\n"
    "cbnz   %w0, 2b\n"
    : "=&r"(r) : "r"(lock), "r"(l) : "memory"
  );

  vmm_log("%p page spinlock\n", page_desc_addr(page));
}

/*
 *  cpu that locked page and cpu that unlocked page must be the same
 */
static inline void page_unlock(struct page_desc *page) {
  u16 *l = &page->ll;

  asm volatile("stlrh wzr, [%0]" :: "r"(l) : "memory");
  vmm_log("%p page unlock\n", page_desc_addr(page));
}

/*
 *  lock page and vsm_waitqueue
 */
static inline void page_vwq_lock(struct page_desc *page) {
  u16 tmp, l = 0x0100 | ((cpuid() + 1) & 0xff);

  vmm_log("page_vwq_lock %p %p\n", page_desc_addr(page), page->ll);

  asm volatile(
    "sevl\n"
    "1: wfe\n"
    "2: ldaxrh %w0, [%1]\n"
    "cbnz   %w0, 1b\n"
    "stxrh  %w0, %w2, [%1]\n"
    "cbnz   %w0, 2b\n"
    : "=&r"(tmp) : "r"(&page->ll), "r"(l) : "memory"
  );
}

/*
 *  lock vsm_waitqueue; if page is unlocked, re-lock page and return 1
 */
static inline bool vwq_lock(struct page_desc *page) {
  u16 tmp, lk, wlk, l = 0x0101;

  asm volatile(
    "sevl\n"
    "1: wfe\n"
    "2: ldaxrh %w0, [%3]\n"
    "and    %w1, %w0, #0x00ff\n"    // w1 = page->lock
    "and    %w2, %w0, #0xff00\n"    // w2 = page->wqlock
    "cbnz   %w2, 1b\n"
    "stxrh  %w0, %w4, [%3]\n"
    "cbnz   %w0, 2b\n"
    : "=&r"(tmp), "=&r"(lk), "=&r"(wlk) : "r"(&page->ll), "r"(l) : "memory"
  );

  return !lk;
}

static inline void vwq_unlock(struct page_desc *page) {
  asm volatile("stlrb wzr, [%0]" :: "r"(&page->wqlock) : "memory");
}

static inline bool vwq_locked(struct page_desc *page) {
  return !!page->wqlock;
}

static inline void vwqinit(struct vsm_waitqueue *wq) {
  memset(wq, 0, sizeof(*wq));
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
static bool vsm_enqueue_proc(struct vsm_server_proc *p) {
  u64 flags;
  struct page_desc *page = ipa_to_desc(p->page_ipa);

  if(!page->wq) {
    page->wq = malloc(sizeof(*page->wq));
    vwqinit(page->wq);
  }

  irqsave(flags);

  vmm_log("enquuuuuuuuuuuuu %p %p\n", p, page_desc_addr(page));

  bool punlocked = vwq_lock(page);
  
  if(page->wq->head == NULL)
    page->wq->head = p;

  if(page->wq->tail)
    page->wq->tail->next = p;

  page->wq->tail = p;

  vwq_unlock(page);

  irqrestore(flags);

  return punlocked;
}

static void vsm_process_wq_core(struct page_desc *page) {
  struct vsm_server_proc *p, *p_next, *head;
  assert(local_irq_disabled());
  assert(vwq_locked(page));

reprocess:
  head = page->wq->head;

  page->wq->head = NULL;
  page->wq->tail = NULL;

  vwq_unlock(page);

  local_irq_enable();

  for(p = head; p; p = p_next) {
    vmm_log("processing queue..... %p %p\n", p, page_desc_addr(page));
    invoke_vsm_process(p);

    p_next = p->next;
    free(p);
  }

  vmm_log("processing doneeeeeeeee..... %p\n", page_desc_addr(page));

  local_irq_disable();

  vwq_lock(page);

  /*
   *  process enqueued processes during in this function
   */
  if(page->wq->head)
    goto reprocess;
}

/*
 *  must be held page->lock
 */
static void vsm_process_waitqueue(struct page_desc *page) {
  u64 flags;

  assert(page_locked(page));

  irqsave(flags);

  vwq_lock(page);

  if(page->wq && page->wq->head) {
    vsm_process_wq_core(page);
  }

  /* unlock page lock and wqlock atomic */
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
  int timeout_us = 3000000;   // wait for 3s
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
  u64 *vttbr = localvm.vttbr;
  u64 page_phys = V2P(page);

  vmm_bug_on(!PAGE_ALIGNED(ipa_page), "pagealign");

  // printf("vsm: cache @%p(%p) copyset: %p count%d\n", ipa_page, page, copyset, ++count);

  /* set access permission later */
  pagemap(vttbr, ipa_page, page_phys, PAGESIZE, PTE_NORMAL|S2PTE_COPYSET(copyset));
}

/*
 *  already has ptable[ipa].lock
 */
static void vsm_invalidate(u64 ipa, u64 copyset) {
  if(copyset == 0)
    return;

  struct msg msg;
  struct invalidate_hdr hdr;

  hdr.ipa = ipa;
  hdr.copyset = copyset;
  hdr.from_nodeid = local_nodeid();

  int node = 0;
  do {
    if((copyset & 1) && (node != local_nodeid())) {
      vmm_log("invalidate request %p %d -> %d\n", ipa, local_nodeid(), node);
 
      msg_init(&msg, node, MSG_INVALIDATE, &hdr, NULL, 0, 0);

      send_msg(&msg);
    }

    copyset >>= 1;
    node++;
  } while(copyset);
}

static void send_invalidate_ack(int from_nodeid, u64 ipa) {
  struct msg msg;
  struct invalidate_ack_hdr hdr;

  hdr.ipa = ipa;
  hdr.from_nodeid = from_nodeid;

  msg_init(&msg, from_nodeid, MSG_INVALIDATE_ACK, &hdr, NULL, 0, 0);

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

  vmm_log("inv server %p: %d -> %d\n", ipa, from_nodeid, local_nodeid()); 

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
  u64 page_pa = 0;
  int manager = -1;
  u64 page_ipa = page_desc_addr(page);

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  page_spinlock(page);

  vmm_log("read request occured: %p %p\n", page_ipa, read_sysreg(elr_el2));

  /*
   * may other cpu has readable page already
   */
  if((pte = s2_readable_pte(vttbr, page_ipa)) != NULL) {
    page_pa = PTE_PA(*pte);
    goto end;
  }

  if(manager == local_nodeid()) {   /* I am manager */
    /* receive page from owner of page */
    struct manager_page *p = ipa_manager_page(page_ipa);
    int owner = p->owner;

    vmm_log("read req %p: %d -> %d request to owner\n", page_ipa, local_nodeid(), owner);

    send_fetch_request(local_nodeid(), owner, page_ipa, 0);
  } else {
    /* ask manager for read access to page and a copy of page */
    vmm_log("read req %p: %d -> %d request to owner\n", page_ipa, local_nodeid(), manager);

    send_fetch_request(local_nodeid(), manager, page_ipa, 0);
  }

  pte = vsm_wait_for_recv_timeout(vttbr, page_ipa);

  vmm_log("read req %p: get remote page!\n", page_ipa);

  page_pa = PTE_PA(*pte);

  /* read data */
  if(unlikely(d))
    memcpy(d->buf, P2V(page_pa + d->offset), d->size);

  s2pte_ro(pte);
  tlb_s2_flush_all();

end:
  vsm_process_waitqueue(page);

  return P2V(page_pa);
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
  u64 page_pa = 0;
  int manager = -1;
  u64 page_ipa = page_desc_addr(page);

  manager = page_manager(page_ipa);
  if(manager < 0)
    return NULL;

  page_spinlock(page);

  vmm_log("write request occured: %p %p\n", page_ipa, read_sysreg(elr_el2));

  /*
   * may other cpu has readable/writable page already
   */
  if((pte = page_rwable_pte(vttbr, page_ipa)) != NULL) {
    page_pa = PTE_PA(*pte);
    goto end;
  }

  assert(local_irq_enabled());

  if((pte = page_ro_pte(vttbr, page_ipa)) != NULL) {
    if(s2pte_copyset(pte) != 0) {
      /* I am owner */
      vmm_log("write request %p: write to owner ro page\n", page_ipa);
      goto inv_phase;
    }

    /*
     *  TODO: no need to fetch page from remote node
     */
    vmm_log("write request %p: write to copyset\n", page_ipa);

    u64 pa = PTE_PA(*pte);

    s2pte_invalidate(pte);
    tlb_s2_flush_all();

    free_page(P2V(pa));
  }

  if(manager == local_nodeid()) {   /* I am manager */
    /* receive page from owner of page */
    struct manager_page *page = ipa_manager_page(page_ipa);
    int owner = page->owner;

    vmm_log("write request %p: %d -> %d request to owner\n", page_ipa, local_nodeid(), owner);

    send_fetch_request(local_nodeid(), owner, page_ipa, 1);
  } else {
    /* ask manager for write access to page and a copy of page */
    vmm_log("write request %p: %d -> %d request to manager\n", page_ipa, local_nodeid(), manager);

    send_fetch_request(local_nodeid(), manager, page_ipa, 1);
  }

  pte = vsm_wait_for_recv_timeout(vttbr, page_ipa);

  vmm_log("write request %p: get remote page!\n", page_ipa);

inv_phase:
  /* invalidate request to copyset */
  vsm_invalidate(page_ipa, s2pte_copyset(pte));

  s2pte_clear_copyset(pte);

  page_pa = PTE_PA(*pte);

  vmm_log("write request: page_pa %p\n", page_pa);

  /* write data */
  if(unlikely(d))
    memcpy(P2V(page_pa + d->offset), d->buf, d->size);

  s2pte_rw(pte);
  // tlb_s2_flush_all();

end:
  vsm_process_waitqueue(page);

  return P2V(page_pa);
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
  struct msg msg;
  struct fetch_req_hdr hdr;

  hdr.ipa = ipa;
  hdr.req_nodeid = req;
  hdr.wnr = wnr;

  msg_init(&msg, dst, MSG_FETCH, &hdr, NULL, 0, 0);

  send_msg(&msg);
}

static void send_read_fetch_reply(u8 dst_nodeid, u64 ipa, void *page) {
  struct msg msg;
  struct fetch_reply_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = 0;
  hdr.copyset = 0;

  /*
  if(ipa == 0x406c2000) {
    printf("rsend;; %p ", read_sysreg(cntvct_el0));
    bin_dump((u8 *)page + 0x350, 0x40);
  }
  */

  msg_init(&msg, dst_nodeid, MSG_FETCH_REPLY, &hdr, page, PAGESIZE, 0);
  vmm_log("send read fetch reply %p\n", page);

  send_msg(&msg);
}

static void send_write_fetch_reply(u8 dst_nodeid, u64 ipa, void *page, u64 copyset) {
  struct msg msg;
  struct fetch_reply_hdr hdr;

  hdr.ipa = ipa;
  hdr.wnr = 1;
  hdr.copyset = copyset;

  /*
  if(ipa == 0x406c2000) {
    printf("wsend;; %p ", read_sysreg(cntvct_el0));
    bin_dump((u8 *)page + 0x350, 0x40);
  }
  */

  msg_init(&msg, dst_nodeid, MSG_FETCH_REPLY, &hdr, page, PAGESIZE, 0);

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

    vmm_log("read server %p: %d -> %d: I am owner!\n", page_ipa, req_nodeid, local_nodeid());

    /* send p */
    send_read_fetch_reply(req_nodeid, page_ipa, P2V(pa));
  } else if(local_nodeid() == manager) {  /* I am manager */
    struct manager_page *p = ipa_manager_page(page_ipa);
    int p_owner = p->owner;

    vmm_log("read server %p: %d -> %d: forward read request\n", page_ipa, req_nodeid, p_owner);

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
    u64 pa = PTE_PA(*pte);
    u64 copyset = s2pte_copyset(pte);

    vmm_log("write server %p %d -> %d I am owner!\n", page_ipa, req_nodeid, local_nodeid());

    s2pte_invalidate(pte);
    tlb_s2_flush_all();

    // send p and copyset;
    send_write_fetch_reply(req_nodeid, page_ipa, P2V(pa), copyset);

    free_page(P2V(pa));

    if(local_nodeid() == manager) {
      struct manager_page *p = ipa_manager_page(page_ipa);

      p->owner = req_nodeid;
    }
  } else if(local_nodeid() == manager) {
    struct manager_page *p = ipa_manager_page(page_ipa);
    int p_owner = p->owner;

    vmm_log("write server %p %d -> %d forward write request\n", page_ipa, req_nodeid, p_owner);

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

static void recv_fetch_request_intr(struct msg *msg) {
  struct fetch_req_hdr *a = (struct fetch_req_hdr *)msg->hdr;
  struct vsm_server_proc *p = new_vsm_server_proc(a->ipa, a->req_nodeid, a->wnr);

  struct page_desc *page = ipa_to_desc(a->ipa);

  if(page_trylock(page)) {
    bool proc_myself = vsm_enqueue_proc(p);
    if(proc_myself)
      vsm_process_waitqueue(page);
    
    return;
  }

  invoke_vsm_process(p);
  free(p);
  vsm_process_waitqueue(page);
}

static void recv_invalidate_intr(struct msg *msg) {
  struct invalidate_hdr *h = (struct invalidate_hdr *)msg->hdr;
  struct vsm_server_proc *p = new_vsm_inv_server_proc(h->ipa, h->from_nodeid, h->copyset);

  struct page_desc *page = ipa_to_desc(h->ipa);

  if(page_trylock(page)) {
    bool proc_myself = vsm_enqueue_proc(p);
    if(proc_myself)
      vsm_process_waitqueue(page);

    return;
  }

  invoke_vsm_process(p); 
  free(p);
  vsm_process_waitqueue(page);
}

static void recv_fetch_reply_intr(struct msg *msg) {
  struct fetch_reply_hdr *a = (struct fetch_reply_hdr *)msg->hdr;
  struct fetch_reply_body *b = msg->body;
  // vmm_log("recv remote ipa %p ----> pa %p\n", a->ipa, b->page);
  //

  /*
  if(a->ipa == 0x406c2000) {
    u64 c = read_sysreg(cntvct_el0);
    // bin_dump((u8 *)b->page + 0x350, 0x40);
    u64 cy = *(u64 *)((u8 *)b->page + 0x360);
    printf("recv;; %p %p\n", cy, c);
  }
  */

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

    guest_memmap(vttbr, start + p, V2P(page), PAGESIZE);

    pagemap(vttbr, start+p, V2P(page), PAGESIZE, PTE_NORMAL|S2PTE_RW);
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
