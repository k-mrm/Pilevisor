#include "types.h"
#include "spinlock.h"
#include "cnt.h"

/* cnt++ */
u32 cnt_inc(struct cnt *cnt) {
  u32 c = cnt->cnt;

  spin_lock(&cnt->lock);
  cnt->cnt++;
  spin_unlock(&cnt->lock);

  return c;
}

/* cnt-- */
u32 cnt_dec(struct cnt *cnt) {
  u32 c = cnt->cnt;

  spin_lock(&cnt->lock);
  cnt->cnt--;
  spin_unlock(&cnt->lock);

  return c;
}

void cnt_init(struct cnt *cnt, u32 init) {
  cnt->cnt = init;
  spinlock_init(&cnt->lock);
}
