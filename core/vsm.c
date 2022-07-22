/* virtual shared memory */
#include "types.h"
#include "aarch64.h"
#include "vsm.h"
#include "mm.h"
#include "kalloc.h"
#include "log.h"
#include "lib.h"
#include "node.h"
#include "vcpu.h"
#include "msg.h"

static u64 vsm_fetch_page_dummy(struct node *node, u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  u64 pa = ipa2pa(vsm->dummypgt, page_ipa);
  if(!pa)
    panic("non pa");

  memcpy(buf, (u8 *)pa, PAGESIZE);

  if(page_ipa == 0x4df75000) {
    u8 *cache = kalloc();
    memcpy(cache, buf, PAGESIZE);
    pagemap(node->vttbr, page_ipa, (u64)cache, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  }

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
  char *pgt = kalloc();

  vsm_fetch_page_dummy(node, 1, page_ipa, pgt);

  pagemap(node->vttbr, page_ipa, (u64)pgt, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);

  return 0;
}

static int vsm_write(struct node *node, u8 dst_node, u64 page_ipa, char *buf) {
  ;
}

static int vsm_fetch_page(struct node *node, u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  vmm_log("request remote fetch!!!!: %p\n", page_ipa);

  vsm->readbuf = buf;
  vsm->finished = 0;

  /* send read request */
  struct read_msg rmsg;
  read_msg_init(&rmsg, dst_node, page_ipa);
  rmsg.msg.send(node, (struct msg *)&rmsg);
  
  /* wait read reply */
  intr_enable();
  while(!vsm->finished)
    wfi();

  vsm->readbuf = NULL;
  vsm->finished = 0;

  return 0;
}

int vsm_access(struct vcpu *vcpu, char *buf, u64 ipa, u64 size, bool wr) {
  char *tmp;
  struct node *node = vcpu->node;

  /* FIXME */
  /* access remote memory */
  if(0x40000000+128*1024*1024 <= ipa && ipa <= 0x40000000+128*1024*1024+128*1024*1024) {
    tmp = kalloc();
    u64 page_ipa = ipa & ~(u64)(PAGESIZE-1);
    u64 pa = vsm_fetch_page_dummy(node, 1, page_ipa, tmp);

    u32 offset = ipa & (PAGESIZE-1);

    if(wr) {
      if(buf)
        memcpy(tmp+offset, buf, size);
      else
        memset(tmp+offset, 0, size);

      vsm_writeback(node, page_ipa, tmp);
    } else {
      if(!buf)
        panic("?");

      memcpy(buf, tmp+offset, size);
    }
  } else {
    return -1;
  }

  kfree(tmp);

  return 0;
}

void vsm_init(struct node *node) {
  /*
  if(node->nodeid == 0) {
    node->vsm.dummypgt = kalloc();
    u64 start = 0x40000000+128*1024*1024;
    for(u64 p = 0; p < 128*1024*1024; p += PAGESIZE) {
      char *page = kalloc();
      if(!page)
        panic("ram");

      pagemap(node->vsm.dummypgt, start+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
    }

    return;
  } */

  /*
  u64 start = 0x40000000+128*1024*1024;
  for(u64 p = 0; p < 128*1024*1024; p += PAGESIZE) {
    char *page = kalloc();
    if(!page)
      panic("ram");

    pagemap(node->vttbr, start+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  } */
}
