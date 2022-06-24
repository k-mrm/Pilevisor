.section ".text"

.macro hyp_save_reg
  sub sp, sp, #256
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
  str x30, [sp, #16 * 15]
.endm

.macro hyp_restore_reg
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
  add sp, sp, #256
.endm

#define VCPU_REG_OFFSET 0x0

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
  
  mrs x1, spsr_el2
  mrs x2, elr_el2
  ldp x3, x4, [sp], #16       /* x3: x0, x4: x1 */
  stp x30, x1, [x0, #16 * 15]
  str x2, [x0, #16 * 16]
  stp x3, x4, [x0, #16 * 0]
.endm

.macro vm_restore_reg
  mrs x0, tpidr_el2
  add x0, x0, #VCPU_REG_OFFSET  /* x0 = &vcpu->reg */
  
  ldp x30, x1, [x0, #16 * 15]  /* x1: spsr */
  ldr x2, [x0, #16 * 16]       /* x2: elr */
  ldp x3, x4, [x0, #16 * 0]    /* x3: x0, x4: x1 */
  stp x3, x4, [sp, #-16]!      /* save x0, x1 in stack */
  msr spsr_el2, x1
  msr elr_el2, x2
  
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

.global vectable
.balign 0x800
vectable:
/* current EL with sp0 */
  b .
.balign 0x80
  b .
.balign 0x80
  b .
.balign 0x80
  b .

/* current EL with spx */
.balign 0x80
  b el2_sync
.balign 0x80
  b el2_irq
.balign 0x80
  b .
.balign 0x80
  b .

/* lower EL using aarch64 */
.balign 0x80
  b el1_sync
.balign 0x80
  b el1_irq
.balign 0x80
  b .
.balign 0x80
  b .

/* lower EL using aarch32 */
.balign 0x80
  b .
.balign 0x80
  b .
.balign 0x80
  b .
.balign 0x80
  b .

el2_sync:
  bl hyp_sync_handler

el2_irq:
  hyp_save_reg

  bl hyp_irq_handler

  hyp_restore_reg

  eret

el1_sync:
  vm_save_reg

  bl vm_sync_handler

.global trapret
trapret:
  vm_restore_reg

  eret

el1_irq:
  vm_save_reg

  bl vm_irq_handler

  vm_restore_reg

  eret
