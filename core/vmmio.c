/*
 *  virtual mmio
 */

#include "types.h"
#include "vmmio.h"
#include "vcpu.h"
#include "msg.h"
#include "node.h"
#include "malloc.h"
#include "spinlock.h"
#include "log.h"
#include "panic.h"

static struct mmio_region *alloc_mmio_region(struct mmio_region *prev) {
  struct mmio_region *m = malloc(sizeof(*m));

  m->next = prev;

  return m;
}

int vmmio_emulate(struct vcpu *vcpu, struct mmio_access *mmio) {
  struct mmio_region *map = localvm.pmap;
  int c = -1;
  u64 ipa = mmio->ipa;

  for(struct mmio_region *m = map; m; m = m->next) {
    if(m->base <= ipa && ipa < m->base + m->size) {
      mmio->offset = ipa - m->base;
      if(mmio->wnr && m->write)
        c = m->write(vcpu, mmio);
      else if(m->read)
        c = m->read(vcpu, mmio);
      break;
    }
  }

  return c;
}

int vmmio_reg_handler(u64 ipa, u64 size,
                     int (*read)(struct vcpu *, struct mmio_access *),
                     int (*write)(struct vcpu *, struct mmio_access *)) {
  if(size == 0)
    return -1;

  spin_lock(&localvm.lock);

  struct mmio_region *new = alloc_mmio_region(localvm.pmap);
  localvm.pmap = new;

  new->base = ipa;
  new->size = size;
  new->read = read;
  new->write = write;

  spin_unlock(&localvm.lock);

  return 0;
}

int vmmio_forward(u32 target_vcpuid, struct mmio_access *mmio) {
  struct msg msg;
  struct msg *reply;
  struct mmio_req_hdr hdr;
  int status;

  int target_nodeid = vcpuid_to_nodeid(target_vcpuid);

  hdr.vcpuid = target_vcpuid;
  memcpy(&hdr.mmio, mmio, sizeof(*mmio));

  printf("vmmio forwarding to vcpu%d %p\n", target_vcpuid, mmio->ipa);

  msg_init(&msg, target_nodeid, MSG_MMIO_REQUEST, &hdr, NULL, 0, M_WAITREPLY);

  reply = send_msg(&msg);

  struct mmio_reply_hdr *rep = (struct mmio_reply_hdr *)reply->hdr;

  printf("rep!!!!!!!!!!!!!! %p %p %d\n", rep->addr, rep->val, rep->status);

  if(mmio->ipa != rep->addr)
    panic("vmmio? %p %p", mmio->ipa, rep->addr);

  mmio->val = rep->val;
  status = rep->status;

  free_recv_msg(reply);

  return status;
}

static void vmmio_req_recv_intr(struct msg *msg) {
  struct mmio_req_hdr *hdr = (struct mmio_req_hdr *)msg->hdr;
  enum vmmio_status status = VMMIO_OK;
  struct mmio_reply_hdr rephdr;

  struct vcpu *vcpu = node_vcpu(hdr->vcpuid);
  if(!vcpu)
    panic("mmio????????");

  if(vmmio_emulate(vcpu, &hdr->mmio) < 0)
    status = VMMIO_FAILED;

  printf("mmio access %s %p %p\n",
          hdr->mmio.wnr ? "write" : "read", hdr->mmio.ipa, hdr->mmio.val);

  rephdr.addr = hdr->mmio.ipa;
  rephdr.val = hdr->mmio.val;
  rephdr.status = status;

  msg_reply(msg, MSG_MMIO_REPLY, (struct msg_header *)&rephdr, NULL, 0);
}

void vmmio_init() {
  spinlock_init(&localvm.lock);
}

DEFINE_POCV2_MSG(MSG_MMIO_REQUEST, struct mmio_req_hdr, vmmio_req_recv_intr);
DEFINE_POCV2_MSG(MSG_MMIO_REPLY, struct mmio_reply_hdr, NULL);
