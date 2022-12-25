/*
 *  logger for vsm
 */

#include "vsm-log.h"
#include "spinlock.h"
#include "assert.h"

static struct vsmlog vsmlog[4096];
static int ring_idx = 0;
static bool loop = false;
static spinlock_t vloglock = SPINLOCK_INIT;

static const char *tyfmt[] = {
  [READ_SENDER]     "read sender",
  [READ_RECEIVER]   "read receiver",
  [WRITE_SENDER]    "write sender",
  [WRITE_RECEIVER]  "write receiver",
  [INV_SENDER]      "inv sender",
  [INV_RECEIVER]    "inv receiver",
};

void vsm_logdump(int n) {
  int start;
  const char *s;
  struct vsmlog *vl;

  assert(n < 4096);

  if(ring_idx < n) {
    if(loop) {
      start = ring_idx + 4096 - n;
    } else {
      start = 0;
      n = ring_idx;
    }
  } else {
    start = ring_idx - n;
  }

  while(n-- > 0) {
    vl = &vsmlog[start];

    if(!vl->msg)
      vl->msg = "";

    printf("%s:cpu%d from: Node %d to: Node %d "
           "@%p %s\n", tyfmt[vl->type], vl->cpu, vl->from_node, vl->to_node, vl->ipa, vl->msg);

    start = (start + 1) % 4096;
  }
}

void vsm_logging(int type, int from_node, int to_node, u64 ipa, const char *msg) {
  u64 flags;

  spin_lock_irqsave(&vloglock, flags);

  struct vsmlog *vl = &vsmlog[ring_idx];

  ring_idx = (ring_idx + 1) % 4096;
  if(ring_idx == 0)
    loop = true;

  vl->type = type;
  vl->from_node = from_node;
  vl->cpu = cpuid();
  vl->to_node = to_node;
  vl->ipa = ipa;
  vl->msg = msg;

  spin_unlock_irqrestore(&vloglock, flags);
}

void vsm_log_out(int type, int from_node, int to_node, u64 ipa, const char *msg) {
  if(!msg)
    msg = "";

  printf("%s:cpu%d from: Node %d to: Node %d "
         "@%p %s\n", tyfmt[type], cpuid(), from_node, to_node, ipa, msg);
}
