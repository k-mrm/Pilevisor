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

/* virtual shared memory */

/* TODO: determine dst_node by ipa */
static inline int page_owner(u64 ipa) {
  if(0x40000000+128*1024*1024 <= ipa && ipa <= 0x40000000+128*1024*1024+128*1024*1024)
    return 1;
  else
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

static void vsm_set_cache(u64 ipa, u8 *page) {
  static int count = 0;
  u64 *vttbr = localnode.vttbr;

  if(!PAGE_ALIGNED(ipa))
    panic("ipa align");

  void *c = alloc_page();
  if(!c)
    panic("cache");

  memcpy(c, page, PAGESIZE);

  vmm_log("vsm: cache @%p %d\n", ipa, ++count);

  pagemap(vttbr, ipa, (u64)c, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
}

void *vsm_fetch_page(u64 page_ipa, bool wr) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  int dst_node = page_owner(page_ipa);
  if(dst_node < 0)
    return NULL;

  u64 *vttbr = localnode.vttbr;
  vmm_log("request remote fetch!!!!: %p\n", page_ipa);

  /* send read request */
  struct read_req rreq;
  read_req_init(&rreq, dst_node, page_ipa);
  msg_send(rreq);

  intr_enable();

  u64 pa;
  while(!(pa = ipa2pa(vttbr, page_ipa)))
    wfi();

  return (void *)pa;
}

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr) {
  if(!buf)
    panic("null buf");

  u64 page_ipa = ipa & ~(u64)(PAGESIZE-1);
  char *pa_page = vsm_fetch_page(page_ipa, wr);
  if(!pa_page)
    return -1;

  u32 offset = ipa & (PAGESIZE-1);

  if(wr)
    memcpy(pa_page+offset, buf, size);
  else
    memcpy(buf, pa_page+offset, size);

  return 0;
}

static void recv_read_request_intr(struct recv_msg *recvmsg) {
  struct __read_req *rd = (struct __read_req *)recvmsg->body;

  /* TODO: use at instruction */
  u64 pa = ipa2pa(localnode.vttbr, rd->ipa);

  vmm_log("read ipa @%p -> pa %p\n", rd->ipa, pa);

  /* send read reply */
  read_reply_send(recvmsg->src_mac, rd->ipa, (void *)pa);
}

void recv_read_reply_intr(struct recv_msg *recvmsg) {
  struct __read_reply *rep = (struct __read_reply *)recvmsg->body;
  vmm_log("recv remote @%p\n", rep->ipa);

  vsm_set_cache(rep->ipa, rep->page);
}

void vsm_init() {
  msg_register_recv_handler(MSG_READ, recv_read_request_intr);
  msg_register_recv_handler(MSG_READ_REPLY, recv_read_reply_intr);
}

/* now Node 1 only */
void vsm_node_init() {
  u64 *vttbr = localnode.vttbr;
  u64 start = 0x40000000 + 128*1024*1024;
  u64 p;

  for(p = 0; p < localnode.nalloc; p += PAGESIZE) {
    char *page = alloc_page();
    if(!page)
      panic("ram");

    pagemap(vttbr, start+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  }

  vmm_log("Node 1 mapped: [%p - %p]\n", start, start+p);
}
