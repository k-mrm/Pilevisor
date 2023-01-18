#include "pl011.h"

#define EARLY_PL011_BASE    0x3f201000

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

  /* TODO: set baudrate */

  mov w2, #LCRH_WLEN_8BIT
  str w2, [x1, #LCRH] 

  mov w2, 0x301   /* RXE, TXE, UARTEN */
  str w2, [x1, #CR]

  ret

.macro earlycon_tx, base, c
  ldr \base, =EARLY_PL011_BASE
  strb \c, [\base, #DR]
.endm
