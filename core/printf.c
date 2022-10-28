#include "types.h"
#include "uart.h"
#include "aarch64.h"
#include "pcpu.h"
#include "vcpu.h"
#include "lib.h"

#define va_list __builtin_va_list
#define va_start(v, l)  __builtin_va_start(v, l)
#define va_arg(v, l)  __builtin_va_arg(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_copy(d, s) __builtin_va_copy(d, s)

enum printopt {
  PR_0X     = 1 << 0,
  ZERO_PAD  = 1 << 1,
};

static void printiu64(i64 num, int base, bool sign, int digit, enum printopt opt) {
  char buf[sizeof(num) * 8 + 1] = {0};
  char *end = buf + sizeof(buf);
  char *cur = end - 1;
  u64 unum;
  bool neg = false;

  if(sign && num < 0) {
    unum = (u64)(-(num + 1)) + 1;
    neg = true;
  } else {
    unum = (u64)num;
  }

  do {
    *--cur = "0123456789abcdef"[unum % base];
  } while(unum /= base);

  if(opt & PR_0X) {
    *--cur = 'x';
    *--cur = '0';
  }

  if(neg)
    *--cur = '-';

  int len = strlen(cur);
  if(digit > 0) {
    while(digit-- > len)
      uart_putc(opt & ZERO_PAD ? '0' : ' '); 
  }
  uart_puts(cur);
  if(digit < 0) {
    digit = -digit;
    while(digit-- > len)
      uart_putc(' '); 
  }
}

static bool isdigit(char c) {
  return '0' <= c && c <= '9';
}

static const char *fetch_digit(const char *fmt, int *digit, enum printopt *opt) {
  int n = 0, neg = 0;
  *opt = 0;

  if(*fmt == '-') {
    fmt++;
    neg = 1;
  } else if(*fmt == '0') {
    fmt++;
    *opt |= ZERO_PAD;
  }

  while(isdigit(*fmt)) {
    n = n * 10 + *fmt++ - '0';
  }

  *digit = neg ? -n : n;

  return fmt;
}

static void printmacaddr(u8 *mac) {
  for(int i = 0; i < 6; i++) {
    printiu64(mac[i], 16, false, 2, ZERO_PAD);
    if(i != 5)
      uart_putc(':');
  }
}

static int vprintf(const char *fmt, va_list ap) {
  char *s;
  void *p;
  int digit = 0;
  enum printopt opt;

  for(; *fmt; fmt++) {
    char c = *fmt;
    if(c == '%') {
      fmt++;
      fmt = fetch_digit(fmt, &digit, &opt);

      switch(c = *fmt) {
        case 'd':
          printiu64(va_arg(ap, i32), 10, true, digit, opt);
          break;
        case 'u':
          printiu64(va_arg(ap, u32), 10, false, digit, opt);
          break;
        case 'x':
          printiu64(va_arg(ap, u64), 16, false, digit, opt);
          break;
        case 'p':
          p = va_arg(ap, void *);
          printiu64((u64)p, 16, false, digit, PR_0X);
          break;
        case 'c':
          uart_putc(va_arg(ap, int));
          break;
        case 's':
          s = va_arg(ap, char *);
          if(!s)
            s = "(null)";

          uart_puts(s);
          break;
        case 'm': /* print mac address */
          printmacaddr(va_arg(ap, u8 *));
          break;
        case '%':
          uart_putc('%');
          break;
        default:
          uart_putc('%');
          uart_putc(c);
          break;
      }
    } else {
      uart_putc(c);
    }
  }

  return 0;
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  vprintf(fmt, ap);

  va_end(ap);

  return 0;
}

static int __stacktrace(u64 sp, u64 bsp, u64 *nextsp) {
  if(sp >= (u64)mycpu->stackbase || bsp > sp)
    return -1;

  u64 x29 = *(u64 *)(sp);
  u64 x30 = *(u64 *)(sp + 8);

  printf("\tfrom: %p\n", x30 - 4);

  *nextsp = x29;

  return 0;
}

void panic(const char *fmt, ...) {
  intr_disable();

  va_list ap;
  va_start(ap, fmt);

  printf("!!!!!!vmm panic cpu%d: ", cpuid());
  vprintf(fmt, ap);
  printf("\n");

  printf("stack trace:\n");

  register const u64 current_sp asm("sp");
  u64 sp, bsp, next;
  sp = bsp = current_sp;

  while(1) {
    if(__stacktrace(sp, bsp, &next) < 0)
      break;

    sp = next;
  }

  printf("stack trace done\n");

  vcpu_dump(current);

  va_end(ap);

  for(;;)
    asm volatile("wfi");
}
