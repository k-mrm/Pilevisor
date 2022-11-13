/*
 *  pl011 uart driver
 */

#include "memmap.h"
#include "types.h"
#include "uart.h"
#include "vcpu.h"
#include "irq.h"
#include "compiler.h"

#define R(reg)  (volatile u32 *)(UARTBASE+reg)

#define DR  0x00
#define FR  0x18
#define FR_RXFE (1<<4)  // recieve fifo empty
#define FR_TXFF (1<<5)  // transmit fifo full
#define FR_RXFF (1<<6)  // recieve fifo full
#define FR_TXFE (1<<7)  // transmit fifo empty
#define IBRD  0x24
#define FBRD  0x28
#define LCRH  0x2c
#define LCRH_FEN  (1<<4)
#define LCRH_WLEN_8BIT  (3<<5)
#define CR    0x30
#define IMSC  0x38
#define INT_RX_ENABLE (1<<4)
#define INT_TX_ENABLE (1<<5)
#define MIS   0x40
#define ICR   0x44

void uart_putc(char c) {
  while(*R(FR) & FR_TXFF)
    ;
  *R(DR) = c;
}

void uart_puts(char *s) {
  char c;
  while((c = *s++))
    uart_putc(c);
}

int uart_getc() {
  if(*R(FR) & FR_RXFE)
    return -1;
  else
    return *R(DR);
}

void uartintr(__unused void *arg) {
  int status = *R(MIS);

  if(status & INT_RX_ENABLE) {
    for(;;) {
      int c = uart_getc();
      if(c < 0)
        break;

      printf("uartintr\n");
    }
  }

  *R(ICR) = (1<<4);
}

void uart_init() {
  *R(CR) = 0;
  *R(IMSC) = 0;
  *R(LCRH) = LCRH_FEN | LCRH_WLEN_8BIT;
  *R(CR) = 0x301;   /* RXE, TXE, UARTEN */
  *R(IMSC) = (1<<4);

  // irq_register(33, uartintr);
}
