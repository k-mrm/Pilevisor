#include "lib.h"
#include "log.h"

void *memcpy(void *dst, const void *src, u64 n) {
  return memmove(dst, src, n);
}

void *memmove(void *dst, const void *src, u64 n) {
  char *d = dst;
  const char *s = src;

  if(s > d) {
    while(n-- > 0)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while(n-- > 0)
      *--d = *--s;
  }

  return dst;
}

void *memset(void *dst, int c, u64 n) {
  char *d = dst;

  while(n-- > 0)
    *d++ = c;

  return dst;
}

char *strcpy(char *dst, const char *src) {
  char *r = dst;

  while((*dst++ = *src++) != 0)
    ;

  return r;
}

int strcmp(const char *s1, const char *s2) {
  while(*s1 && *s1 == *s2) {
    s1++;
    s2++;
  }

  return *s1 - *s2;
}

int memcmp(const void *b1, const void *b2, u64 count) {
  const char *p1 = b1;
  const char *p2 = b2;
  while(count > 0) {
    if(*p1 != *p2)
      return *p1 - *p2;
    p1++;
    p2++;
    count--;
  }

  return 0;
}

int strncmp(const char *s1, const char *s2, u64 len) {
  while(*s1 && *s1 == *s2 && len > 0) {
    s1++;
    s2++;
    len--;
  }
  if(len == 0)
    return 0;

  return *s1 - *s2;
}

u64 strlen(const char *s) {
  u64 i = 0;
  while(*s++)
    i++;

  return i;
}

void bin_dump(void *p, u64 size) {
  printf("bin dump %p %d\n", p, size);
  u8 *bin = p;
  for(u64 i = 0; i < size; i++) {
    printf("%02x ", bin[i]);
    if((i+1) % 8 == 0)
      printf("\n");
  }
}
