#ifndef MVMM_PRINTF_H
#define MVMM_PRINTF_H

#define va_list __builtin_va_list
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_arg(v, l)  __builtin_va_arg(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_copy(d, s) __builtin_va_copy(d, s)

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);

#endif
