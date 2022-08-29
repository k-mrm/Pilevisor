#ifndef MVMM_TYPES_H
#define MVMM_TYPES_H

typedef unsigned long u64;
typedef long i64;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned char u8;
typedef signed char i8;

#define NULL ((void *)0)

typedef _Bool bool;

#define true 1
#define false 0

#define offsetof(st, m) ((u64)((char *)&((st *)0)->m - (char *)0))

#define container_of(ptr, st, m)  \
  ({ const typeof(((st *)0)->m) *_mptr = (ptr); \
     (st *)((char *)_mptr - offsetof(st, m)); })

#endif
