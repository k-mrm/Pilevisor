#include "pl011.h"
#include "earlycon.h"

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

  mov w2, 0x301   /* RXE, TXE, UARTEN */
  str w2, [x1, #CR]

  ret

.macro earlycon_tx, c, tmp1, tmp2
  ldr \tmp1, =EARLY_PL011_BASE

99:
  ldrh \tmp2, [\tmp1, #FR]
  tst  \tmp2, #0x8
  b.ne 99b

  strb \c, [\tmp1, #DR]
.endm
