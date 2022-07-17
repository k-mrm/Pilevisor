/* virtual shared memory */
#include "types.h"
#include "aarch64.h"
#include "vsm.h"
#include "mm.h"
#include "kalloc.h"
#include "log.h"
#include "lib.h"
#include "node.h"
#include "msg.h"

static int vsm_fetch_page_dummy(struct node *node, u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  u64 pa = ipa2pa(vsm->dummypgt, page_ipa);

  memcpy(buf, (u8 *)pa, PAGESIZE);

  return 0;
}

static int vsm_writeback(struct node *node, u64 page_ipa, char *buf) {
  if(page_ipa % PAGESIZE)
    panic("align error");

  struct vsmctl *vsm = &node->vsm;

  u64 pa = ipa2pa(vsm->dummypgt, page_ipa);

  memcpy((u8 *)pa, buf, PAGESIZE);

  return 0;
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


int vsm_access(struct node *node, u64 ipa, u64 *reg, enum maccsize accsz, bool wr) {
  struct vsmctl *vsm = &node->vsm;
  char *buf;

  /* FIXME */
  /* access remote memory */
  if(0x40000000+128*1024*1024 <= ipa && ipa <= 0x40000000+128*1024*1024+128*1024*1024) {
    buf = kalloc();
    u64 page_ipa = ipa & ~(u64)(PAGESIZE-1);
    if(vsm_fetch_page_dummy(node, 1, page_ipa, buf) < 0)
      goto err;

    u32 offset = ipa & (PAGESIZE-1);

    if(wr) {
      switch(accsz) {
        case ACC_BYTE:
          *(u8 *)(buf + offset) = *reg;
          break;
        case ACC_HALFWORD:
          *(u16 *)(buf + offset) = *reg;
          break;
        case ACC_WORD:
          *(u32 *)(buf + offset) = *reg;
          break;
        case ACC_DOUBLEWORD:
          *(u64 *)(buf + offset) = *reg;
          break;
        default:
          goto err;
      }
      // vmm_log("write ipa %p : %p\n", ipa, *reg);
      vsm_writeback(node, page_ipa, buf);
    } else {
      switch(accsz) {
        case ACC_BYTE:
          *reg = *(u8 *)(buf + offset); 
          break;
        case ACC_HALFWORD:
          *reg = *(u16 *)(buf + offset);
          break;
        case ACC_WORD:
          *reg = *(u32 *)(buf + offset);
          break;
        case ACC_DOUBLEWORD:
          *reg = *(u64 *)(buf + offset);
          break;
        default:
          goto err;
      }
      // vmm_log("read ipa %p : %p\n", ipa, *reg);
    }

  } else {
    return -1;
  }

  kfree(buf);

  return 0;

err:
  kfree(buf);
  panic("fetch failed");
}

void vsm_init(struct node *node) {
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
  }

  u64 start = 0x40000000+128*1024*1024;
  for(u64 p = 0; p < 128*1024*1024; p += PAGESIZE) {
    char *page = kalloc();
    if(!page)
      panic("ram");

    pagemap(node->vttbr, start+p, (u64)page, PAGESIZE, S2PTE_NORMAL|S2PTE_RW);
  }
}
