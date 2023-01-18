#ifndef DRIVER_PL011_H
#define DRIVER_PL011_H

#define DR      0x00
#define FR      0x18
#define IBRD    0x24
#define FBRD    0x28
#define LCRH    0x2c
#define CR      0x30
#define IFLS    0x34
#define IMSC    0x38
#define MIS     0x40
#define ICR     0x44

#define FR_RXFE (1<<4)  // recieve fifo empty
#define FR_TXFF (1<<5)  // transmit fifo full
#define FR_RXFF (1<<6)  // recieve fifo full
#define FR_TXFE (1<<7)  // transmit fifo empty

#define INT_RX_ENABLE   (1<<4)
#define INT_TX_ENABLE   (1<<5)

#define LCRH_FEN        (1<<4)
#define LCRH_WLEN_8BIT  (3<<5)

#define UART_FREQ           48000000ull

#endif  /* DRIVER_PL011_H */
