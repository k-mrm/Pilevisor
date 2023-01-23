#ifndef MVMM_LIB_H
#define MVMM_LIB_H

#include "types.h"

void *memcpy(void *dst, const void *src, u64 n);
void *memmove(void *dst, const void *src, u64 n);
void *memset(void *dst, int c, u64 n);
int memcmp(const void *b1, const void *b2, u64 count);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, u64 len);
u64 strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strchr(const char *s, int c);
char *strtok(char *s1, const char *s2);

void bin_dump(void *p, u64 size);

#define BIT(n)          (1 << (n))
#define get_bit(x, n)   (((x) & BIT(n)) >> n)

#define max(x, y)       ((x) > (y) ? (x) : (y))
#define min(x, y)       ((x) < (y) ? (x) : (y))

#endif
