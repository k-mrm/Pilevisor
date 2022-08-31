#include "net.h"
#include "spinlock.h"

static struct packet packets[256];
static spinlock_t plock;

struct packet *allocpacket() {
  acquire(&plock);
  for(struct packet *p = packets; p < &packets[256]; p++) {
    if(!p->used) {
      p->used = 1;
      release(&plock);
      return p;
    }
  }
  release(&plock);
}

void freepacket(struct packet *packet) {
  acquire(&plock);
  struct packet *p_next;
  for(struct packet *p = packet; p; p = p_next) {
    p_next = p->next;
    memset(p, 0, sizeof(*p));
  }
  release(&plock);
}
