#define read_sysreg(val, reg) \
  asm volatile("mrs %0, " #reg : "=r"(val))
#define __write_sysreg(reg, val)  \
  asm volatile("msr " #reg ", %0" : : "r"(val))
#define write_sysreg(reg, val)  \
  do { unsigned long x = (unsigned long)(val); __write_sysreg(reg, x); } while(0)

void timerinit(void);
void timerintr(void);

#define UART_IRQ    33
#define TIMER_IRQ   27
