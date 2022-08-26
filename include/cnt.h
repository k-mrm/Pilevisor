#ifndef CNT_H
#define CNT_H

#include "spinlock.h"

struct cnt {
  u32 cnt;
  spinlock_t lock;
};

void cnt_init(struct cnt *cnt, u32 init);
u32 cnt_inc(struct cnt *cnt);
u32 cnt_dec(struct cnt *cnt);

#endif
