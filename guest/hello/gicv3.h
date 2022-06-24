#ifndef MVMM_GUEST_GICV3_H
#define MVMM_GUEST_GICV3_H

typedef unsigned int u32;
typedef unsigned long u64;

void gicv3_init(void);
void gicv3_init_percpu(void);

u32 gic_iar(void);
void gic_eoi(u32 iar);

#endif
