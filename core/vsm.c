/* virtual shared memory */
#include "types.h"
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

  pfree(buf);

  return 0;

err:
  pfree(buf);
  panic("fetch failed");
}

void vsm_init(struct vsmctl *vsm, u64 entry, u64 ram_start_gpa, u64 ram_size, u64 img_start, u64 img_size) {
  u64 *vttbr = kalloc();
  if(!vttbr)
    panic("no mem");
  if(ram_size <= img_size)
    panic("ramsize");

  /* mapping code */
  u64 p, cpsize;
  for(p = 0; p < img_size; p += PAGESIZE) {
    char *page = kalloc();
    if(!page)
      panic("nomem");

    if(img_size - p > PAGESIZE)
      cpsize = PAGESIZE;
    else
      cpsize = img_size - p;

    memcpy(page, (char *)img_start+p, cpsize);
    pagemap(vttbr, entry+p, (u64)page, PAGESIZE, S2PTE_NORMAL);
  }

  if(entry + p <= ram_start_gpa)
    p = 0;

  /* mapping ram */
  for(int i = 0; p < ram_size; p += PAGESIZE, i++) {
    char *page = kalloc();
    if(!page)
      panic("nomem");
    if(i == 0) {
      for(int i = 0; i < PAGESIZE; i++)
        page[i] = i % 0x100;
    }

    // printf("%p:%p ", ram_start_gpa+p, page);
    pagemap(vttbr, ram_start_gpa+p, (u64)page, PAGESIZE, S2PTE_NORMAL);
  }

  vsm->local.start = ram_start_gpa;
  vsm->local.size = ram_size;

  vsm->vttbr = vttbr;
}
