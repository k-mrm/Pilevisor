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

static int vsm_fetch_page(struct node *node, u8 dst_node, u64 page_ipa, char *buf) {
  if(page_ipa & PAGESIZE)
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

static int vsm_emulate_read() {
  /* unimpl */
  return -1;
}

static int vsm_emulate_write() {
  /* unimpl */
  return -1;
}

int vsm_access(struct node *node, u64 ipa, u64 *reg, enum maccsize accsz, bool wr) {
  struct vsmctl *vsm = &node->vsm;
  char *buf;

  /* FIXME */
  /* access remote memory */
  if(0x40000000+256*1024 <= ipa && ipa <= 0x40000000+256*1024+256*1024) {
    buf = kalloc();
    u64 page_ipa = ipa & ~(u64)(PAGESIZE-1);
    if(vsm_fetch_page(node, 1, page_ipa, buf) < 0)
      goto err;

    u32 offset = ipa & (PAGESIZE-1);

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

    vmm_log("read ipa %p : %p\n", ipa, *reg);
  } else {
    return -1;
  }

  kfree(buf);

  return 0;

err:
  kfree(buf);
  panic("fetch failed");
}

void vsm_init(struct vsmctl *vsm) {
  ;
}
