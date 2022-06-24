#include "uart.h"
#include "hello.h"
#include "gicv3.h"

#define intr_enable() \
  asm volatile("msr daifclr, #0x2" ::: "memory");

__attribute__((aligned(16))) char _stack[4096];

int devintr(int iar) {
  int w;
  int irq = iar & 0x3ff;

  switch(irq) {
    case UART_IRQ:
      uartintr();
      w = 1;
      break;
    case TIMER_IRQ:
      timerintr();
      w = 1;
      break;
    default:
      uart_puts("????????\n");
      w = 0;
      break;
  }

  if(w)
    gic_eoi(iar);

  return w;
}

void el1trap() {
  int iar = gic_iar();
  if(!devintr(iar))
    uart_puts("sync!");
}

void vectable();

int main(void) {
  uart_init();
  uart_puts("hello, guest world!\n");
  gicv3_init();
  gicv3_init_percpu();
  timerinit();
  write_sysreg(vbar_el1, (unsigned long)vectable);

  unsigned int sctlr;
  read_sysreg(sctlr, sctlr_el1);
  uart_puts("\n");
  uart_put64(sctlr, 16);

  intr_enable();
  uart_puts("\nintr\n");
  int i = 0;

  for(;;) {
    /*
    for(int j = 0; j < 1000; i++, j++)
      uart_put64(i, 10);
    asm volatile ("hvc #10");
    */
  }

  return 0x199999;
}
