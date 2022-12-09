#include "aarch64.h"

.section ".text"

.macro hyp_save_reg
  sub sp, sp, #272

  stp x0, x1, [sp, #16 * 0]
  stp x2, x3, [sp, #16 * 1]
  stp x4, x5, [sp, #16 * 2]
  stp x6, x7, [sp, #16 * 3]
  stp x8, x9, [sp, #16 * 4]
  stp x10, x11, [sp, #16 * 5]
  stp x12, x13, [sp, #16 * 6]
  stp x14, x15, [sp, #16 * 7]
  stp x16, x17, [sp, #16 * 8]
  stp x18, x19, [sp, #16 * 9]
  stp x20, x21, [sp, #16 * 10]
  stp x22, x23, [sp, #16 * 11]
  stp x24, x25, [sp, #16 * 12]
  stp x26, x27, [sp, #16 * 13]
  stp x28, x29, [sp, #16 * 14]

  mrs x0, spsr_el2
  mrs x1, elr_el2
  stp x30, x0, [sp, #16 * 15]
  str x1, [sp, #16 * 16]
.endm

.macro hyp_restore_reg
  ldr x0, [sp, #(16 * 15 + 8)]
  ldr x1, [sp, #(16 * 16)]

  msr spsr_el2, x0
  msr elr_el2, x1

  ldp x0, x1, [sp, #16 * 0]
  ldp x2, x3, [sp, #16 * 1]
  ldp x4, x5, [sp, #16 * 2]
  ldp x6, x7, [sp, #16 * 3]
  ldp x8, x9, [sp, #16 * 4]
  ldp x10, x11, [sp, #16 * 5]
  ldp x12, x13, [sp, #16 * 6]
  ldp x14, x15, [sp, #16 * 7]
  ldp x16, x17, [sp, #16 * 8]
  ldp x18, x19, [sp, #16 * 9]
  ldp x20, x21, [sp, #16 * 10]
  ldp x22, x23, [sp, #16 * 11]
  ldp x24, x25, [sp, #16 * 12]
  ldp x26, x27, [sp, #16 * 13]
  ldp x28, x29, [sp, #16 * 14]
  ldr x30, [sp, #16 * 15]

  add sp, sp, #272
.endm

#define VCPU_REG_OFFSET 0x0

.macro vm_save_sp_elx, el, savereg
  cmp \el, 0 
  b.eq 1f
  cmp \el, 1
  b.eq 2f
  b .     // panic
1:
  mrs \savereg, sp_el0
  b 3f
2:
  mrs \savereg, sp_el1
3:
.endm

.macro vm_restore_sp_elx, el, reg
  cmp \el, 0
  b.eq 4f
  cmp \el, 1
  b.eq 5f
  b .     // panic
4:
  msr sp_el0, \reg
  b 6f
5:
  msr sp_el1, \reg
  b 6f
6:
.endm

.macro vm_save_reg
  stp x0, x1, [sp, #-16]!
  mrs x0, tpidr_el2
  add x0, x0, #VCPU_REG_OFFSET   /* x0 = &vcpu->reg */
  
  stp x2, x3, [x0, #16 * 1]
  stp x4, x5, [x0, #16 * 2]
  stp x6, x7, [x0, #16 * 3]
  stp x8, x9, [x0, #16 * 4]
  stp x10, x11, [x0, #16 * 5]
  stp x12, x13, [x0, #16 * 6]
  stp x14, x15, [x0, #16 * 7]
  stp x16, x17, [x0, #16 * 8]
  stp x18, x19, [x0, #16 * 9]
  stp x20, x21, [x0, #16 * 10]
  stp x22, x23, [x0, #16 * 11]
  stp x24, x25, [x0, #16 * 12]
  stp x26, x27, [x0, #16 * 13]
  stp x28, x29, [x0, #16 * 14]
  
  mrs x19, spsr_el2
  mrs x20, elr_el2

  /* extract M[3:2](=el) from spsr_el2 */
  ubfx x21, x19, #2, #0x3

  vm_save_sp_elx x21, x22

  ldp x23, x24, [sp], #16       /* x23: x0, x24: x1 */
  stp x30, x19, [x0, #16 * 15]
  stp x20, x22, [x0, #16 * 16]
  stp x23, x24, [x0, #16 * 0]
.endm

.macro vm_restore_reg
  mrs x0, tpidr_el2
  add x0, x0, #VCPU_REG_OFFSET  /* x0 = &vcpu->reg */
  
  ldp x30, x1, [x0, #16 * 15]  /* x1: spsr */
  ldp x2, x3, [x0, #16 * 16]   /* x2: elr x3: sp */
  ldp x4, x5, [x0, #16 * 0]    /* x4: x0, x5: x1 */
  stp x4, x5, [sp, #-16]!      /* save x0, x1 in stack */
  msr spsr_el2, x1
  msr elr_el2, x2

  /* extract M[3:2](=el) from spsr_el2 */
  ubfx x6, x1, #2, #0x3

  vm_restore_sp_elx x6, x3
  
  ldp x2, x3, [x0, #16 * 1]
  ldp x4, x5, [x0, #16 * 2]
  ldp x6, x7, [x0, #16 * 3]
  ldp x8, x9, [x0, #16 * 4]
  ldp x10, x11, [x0, #16 * 5]
  ldp x12, x13, [x0, #16 * 6]
  ldp x14, x15, [x0, #16 * 7]
  ldp x16, x17, [x0, #16 * 8]
  ldp x18, x19, [x0, #16 * 9]
  ldp x20, x21, [x0, #16 * 10]
  ldp x22, x23, [x0, #16 * 11]
  ldp x24, x25, [x0, #16 * 12]
  ldp x26, x27, [x0, #16 * 13]
  ldp x28, x29, [x0, #16 * 14]
  
  ldp x0, x1, [sp], #16
.endm

.macro ventry, label
.balign 0x80
  b \label
.endm

.global vectable
.balign 0x800
vectable:
/* current EL with sp0 */
  ventry  .
  ventry  .
  ventry  .
  ventry  .

/* current EL with spx */
  ventry  el2_sync
  ventry  el2_irq
  ventry  el2_fiq
  ventry  el2_serror

/* lower EL using aarch64 */
  ventry  guest_sync
  ventry  guest_irq
  ventry  .
  ventry  .

/* lower EL using aarch32 */
  ventry  .
  ventry  .
  ventry  .
  ventry  .

el2_sync:
  hyp_save_reg

  mov x0, sp
  bl hyp_sync_handler

  /* never return here */

el2_irq:
  hyp_save_reg

  mov x0, 0
  bl irq_entry

  INTR_DISABLE

  hyp_restore_reg

  eret

el2_fiq:
  b fiq_handler

  /* never return here */

el2_serror:
  hyp_save_reg

  mov x0, sp
  bl hyp_serror_handler

  /* never return here */

guest_sync:
  vm_save_reg

  bl vm_sync_handler

  INTR_DISABLE

.global trapret
trapret:
  vm_restore_reg

  eret

guest_irq:
  vm_save_reg

  mov x0, 1
  bl irq_entry

  INTR_DISABLE

  vm_restore_reg

  eret
