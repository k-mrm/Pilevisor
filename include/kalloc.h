#ifndef MVMM_KALLOC_H
#define MVMM_KALLOC_H

void *kalloc(void);
void kfree(void *p);
void kalloc_init(void);

#endif
