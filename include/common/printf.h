#ifndef MVMM_PRINTF_H
#define MVMM_PRINTF_H

int printf(const char *fmt, ...);
void panic(const char *fmt, ...) __attribute__((noreturn));

#endif
