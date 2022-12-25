#include "types.h"
#include "uart.h"
#include "lib.h"
#include "spinlock.h"
#include "printf.h"
#include "panic.h"
#include "log.h"

/*
 *  log level:
 *    
 *  0: flush
 *  1: warn
 *  2: log
 *
 */
struct log {
  int cpu;
  int level;
  char *msg;
};

#define PRINT_NBUF    (32 * 1024)
#define NLOG          1024

static spinlock_t loglock;
static char printbuf[PRINT_NBUF];   /* 32 KB */
static int pbuf_head = 0;
static int pbuf_tail = 0;
static struct log log[NLOG]; 
static int log_head = 0;
static int log_tail = 0;

static int __vprintf(const char *fmt, va_list ap, void (*putc)(char));
static int __printf(const char *fmt, ...);

enum printopt {
  PR_0X     = 1 << 0,
  ZERO_PAD  = 1 << 1,
};

/*
 *  must hold loglock
 */
static struct log *logpush() {
  int idx = log_tail;
  log_tail = (log_tail + 1) % NLOG;

  return &log[idx];
}

static void lputc(char c) {
  int idx = pbuf_tail;
  printbuf[idx] = c;

  pbuf_tail = (pbuf_tail + 1) % PRINT_NBUF;
}

static void printiu64(i64 num, int base, bool sign, int digit, enum printopt opt, void (*putc)(char)) {
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
      putc(opt & ZERO_PAD ? '0' : ' '); 
  }

  char c;
  while((c = *cur++))
    putc(c);

  if(digit < 0) {
    digit = -digit;
    while(digit-- > len)
      putc(' '); 
  }
}

static inline bool isdigit(char c) {
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

static void prmacaddr(u8 *mac, void (*putc)(char)) {
  for(int i = 0; i < 6; i++) {
    printiu64(mac[i], 16, false, 2, ZERO_PAD, putc);
    if(i != 5)
      putc(':');
  }
}

static int __vprintf(const char *fmt, va_list ap, void (*putc)(char)) {
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
          printiu64(va_arg(ap, i32), 10, true, digit, opt, putc);
          break;
        case 'u':
          printiu64(va_arg(ap, u32), 10, false, digit, opt, putc);
          break;
        case 'x':
          printiu64(va_arg(ap, u64), 16, false, digit, opt, putc);
          break;
        case 'p':
          p = va_arg(ap, void *);
          printiu64((u64)p, 16, false, digit, PR_0X, putc);
          break;
        case 'c':
          putc(va_arg(ap, int));
          break;
        case 's': {
          s = va_arg(ap, char *);
          if(!s)
            s = "(null)";

          char cc;
          while((cc = *s++))
            putc(cc);

          break;
        }
        case 'm': /* print mac address */
          prmacaddr(va_arg(ap, u8 *), putc);
          break;
        case '%':
          putc('%');
          break;
        default:
          putc('%');
          putc(c);
          break;
      }
    } else {
      putc(c);
    }
  }

  putc('\0');

  return 0;
}

void pflush() {
  ;
}

void log_dump_level(int level) {
  ;
}

int vprintf(const char *fmt, va_list ap) {
  u64 flags;

  spin_lock_irqsave(&loglock, flags);

  int rc = __vprintf(fmt, ap, uart_putc);

  spin_unlock_irqrestore(&loglock, flags);

  return rc;
}

static void levelprefix(int level) {
  switch(level) {
    case 1:     // WARN
      __printf("[warning]:cpu%d: ", cpuid());
      return;
    case 2:     // LOG
      __printf("[log]: cpu%d: ", cpuid());
      return;
  }
}

/*
 *  must hold loglock
 */
static int __printf(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  __vprintf(fmt, ap, uart_putc);
  va_end(ap);

  return 0;
}

int printf(const char *fmt, ...) {
  u64 flags;
  int level;
  va_list ap;
  void (*putcf)(char);

  if(*fmt == '\001') {
    level = *++fmt - '0';
    fmt++;
  } else {
    level = 0;      // default
  }

  // spin_lock_irqsave(&loglock, flags);

  // void (*putcf) = level == 0 ? uart_putc : lputc;
  putcf = uart_putc;

  if(level)
    levelprefix(level);

  if(putcf == lputc) {
    struct log *l = logpush();
    l->cpu = cpuid();
    l->level = level;
    l->msg = &printbuf[pbuf_tail];
  }

  va_start(ap, fmt);
  __vprintf(fmt, ap, putcf);
  va_end(ap);

  // spin_unlock_irqrestore(&loglock, flags);

  return 0;
}

void printf_init() {
  spinlock_init(&loglock);
}
