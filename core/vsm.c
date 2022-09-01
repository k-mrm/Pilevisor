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

static u64 vsm_fetch_page_dummy(struct node *node, u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  u64 pa = ipa2pa(vsm->dummypgt, page_ipa);
  if(!pa)
    panic("non pa");

  memcpy(buf, (u8 *)pa, PAGESIZE);

  return pa;
}

static int vsm_writeback(struct node *node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  u64 pa = ipa2pa(vsm->dummypgt, page_ipa);
  if(!pa)
    panic("non pa");

  memcpy((u8 *)pa, buf, PAGESIZE);

  return 0;
}

int vsm_fetch_pagetable(struct node *node, u64 page_ipa) {
  char *pgt = alloc_page();
  if(!pgt)
    panic("mem");

  vsm_fetch_page_dummy(node, 1, page_ipa, pgt);

  pagemap(node->vttbr, page_ipa, (u64)pgt, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);

  return 0;
}

int vsm_fetch_and_cache_dummy(struct node *node, u64 page_ipa) {
  char *page = alloc_page();
  if(!page)
    panic("mem");

  struct vcpu *vcpu = &node->vcpus[0];

  vsm_fetch_page_dummy(node, 1, page_ipa, page);

  pagemap(node->vttbr, page_ipa, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  
  vmm_log("dummy cache %p elr %p va %p\n", page_ipa, vcpu->reg.elr, vcpu->dabt.fault_va);

  return 0;
}

static int vsm_fetch_page(u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  vmm_log("request remote fetch!!!!: %p\n", page_ipa);

  /* send read request */
  struct read_req rreq;
  read_req_init(&rreq, dst_node, page_ipa);
  msg_send(rreq);

  intr_enable();

  for(;;)
    wfi();
  
  return 0;
}

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr) {
  char *tmp;
  struct node *node = &localnode;
  int zerofill = !buf;

  if(!wr && !buf)
    panic("null buf");

  /* FIXME */
  /* access remote memory */
  if(0x40000000+128*1024*1024 <= ipa && ipa <= 0x40000000+128*1024*1024+128*1024*1024) {
    tmp = alloc_page();
    u64 page_ipa = ipa & ~(u64)(PAGESIZE-1);
    u64 pa = vsm_fetch_page(1, page_ipa, tmp);

    u32 offset = ipa & (PAGESIZE-1);

    if(wr) {
      if(zerofill)
        memset(tmp+offset, 0, size);
      else
        memcpy(tmp+offset, buf, size);

      vsm_writeback(node, page_ipa, tmp);
    } else {
      memcpy(buf, tmp+offset, size);
    }
  } else {
    return -1;
  }

  free_page(tmp);

  return 0;
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
