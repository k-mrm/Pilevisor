#ifndef MVMM_UART_H
#define MVMM_UART_H

void uart_putc(char c);
void uart_puts(char *s);
int uart_getc(void);
void uart_init(void);

#endif
