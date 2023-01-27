#include "pl011.h"
#include "earlycon.h"
#include "gpio.h"

/*
 *  early console pl011
 */

.global earlycon_init
earlycon_init:
  ldr x1, =EARLY_PL011_BASE

  /* disable UART */
  str wzr, [x1, #CR]
  str wzr, [x1, #LCRH]
  str wzr, [x1, #IFLS]
  str wzr, [x1, #IMSC]

  /* set baudrate to 115200 */
  mov w2, #26
  str w2, [x1, #IBRD]
  mov w2, #3
  str w2, [x1, #FBRD]

  mov w2, #LCRH_WLEN_8BIT
  str w2, [x1, #LCRH] 

  mov w2, #0x301       // RXE, TXE, UARTEN
  str w2, [x1, #CR]

  ret

.macro earlycon_tx, base, c
  ldr \base, =EARLY_PL011_BASE
  strb \c, [\base, #DR]
.endm
