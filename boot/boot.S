#include "aarch64.h"
#include "mm.h"

.section ".text.boot"

#include "earlyconsole.S.inc"


/*
 *  volatile x19, x20, x21
 */
.macro earlyputs, s
  adr x19, 2f
1:
  ldrb w20, [x19], #1
  cbz w20, 3f
  earlycon_tx x21, w20
  b 1b

  .pushsection .rodata.str, "aS"
2:.asciz "\s"
  .popsection  

3:
.endm

/*
 *  x0: dtb pointer
 */
.global _start
_start:
  INTR_DISABLE

  mrs x1, CurrentEL
  cmp x1, #0x8    // EL2
  b.eq prestartup
  cmp x1, #0xc    // EL3
  b.eq switch_el2
  bl hang         // panic

switch_el2:
  mov x1, #(SCR_NS | SCR_SMD | SCR_HCE | SCR_RW | SCR_RES1) 
  msr scr_el3, x1
  adr x2, prestartup
  msr elr_el3, x2
  mov x3, #0x3c9    // DAIF, EL2H
  msr spsr_el3, x3

  eret

prestartup:
  mrs x1, mpidr_el1
  and x1, x1, #0xf
  cbz x1, startup0    /* cpu0 */
  b startupothers     /* others */

startup0:
  bl bss_clear

  /*
   *  make early 1:1 mapping vmm_start to __earlymem_end
   */
  ldr x1, =vmm_start
  ldr x2, =__earlymem_end
  bl setup_boot_pagetable

  bl earlycon_init

  earlyputs "boot cpu init...\r\n"

  bl initcpu

  ldr x30, =vmm_init_cpu0
  b enable_boot_mmu   // return to x30: vmm_init_cpu0 (init.c)

startupothers:
  bl initcpu

  ldr x30, =vmm_init_secondary
  b enable_mmu_secondary   // return to x30: vmm_init_secondary (init.c)

.macro set_block, pgt, idx, addr, level, memtype
  adrp x8, \pgt
  add x8, x8, \idx, lsl #3
  bic x9, \addr, #((1 << (39 - \level * 9)) - 1)
  mov x10, #(PTE_AF | PTE_VALID | \memtype)
  orr x9, x9, x10
  str x9, [x8]
.endm 

.macro set_table, pgt, idx, table
  adrp x8, \pgt
  add x8, x8, \idx, lsl #3
  adrp x9, \table
  orr x9, x9, #(PTE_TABLE | PTE_VALID)
  str x9, [x8]
.endm

.macro pgt_index, idx, va, level
  ubfx  \idx, \va, #(39 - \level * 9), 9 
.endm 

/*
 *  make 1:1 mapping
 *  x1: start address
 *  x2: end address
 */
setup_boot_pagetable:
  /* map kernel */
  pgt_index x3, x1, 1     // x3 = PIDX(x1, 1)

  set_table __boot_pgt_l1, x3, __boot_pgt_l2_1

  pgt_index x4, x1, 2     // x4 = PIDX(x1, 2)
  pgt_index x5, x2, 2     // x5 = PIDX(x2, 2)

1:
  set_block __boot_pgt_l2_1, x4, x1, 2, PTE_NORMAL

  add x4, x4, #1
  add x1, x1, #0x200000   // next block
  cmp x4, x5
  b.ls 1b
 
  /* map earlyconsole */
  ldr x1, =EARLY_PL011_BASE

  pgt_index x2, x1, 1     // x2 = PIDX(x1, 1)
  set_table __boot_pgt_l1, x2, __boot_pgt_l2_2

  pgt_index x3, x1, 2     // x3 = PIDX(x1, 2)
  set_table __boot_pgt_l2_2, x3, __boot_pgt_l3

  pgt_index x4, x1, 3     // x4 = PIDX(x1, 3)
  set_block __boot_pgt_l3, x4, x1, 3, PTE_DEVICE_nGnRE

  ret

enable_boot_mmu:
  adrp x1, __boot_pgt_l1
  msr ttbr0_el2, x1

  tlbi alle2
  dsb sy
  
  /* enable MMU */
  mrs x1, sctlr_el2
  orr x1, x1, #(SCTLR_M)
  msr sctlr_el2, x1

  isb

  ret

enable_mmu_secondary:
  ldr x1, =vmm_pagetable
  ldr x1, [x1]
  msr ttbr0_el2, x1

  tlbi alle2
  dsb sy
  
  /* enable MMU */
  mrs x1, sctlr_el2
  orr x1, x1, #(SCTLR_M)
  msr sctlr_el2, x1

  isb

  ret

bss_clear:
  adrp x1, __bss_start
  ldr w2, =__bss_size
1:
  cbz w2, 2f
  str xzr, [x1], #8
  sub w2, w2, #1
  b 1b
2:
  ret

initcpu:
  /* setup vector table */
  ldr x1, =early_vectable
  msr vbar_el2, x1

  /* setup mair_el2 */
  ldr x1, =MAIR_VALUE
  msr mair_el2, x1

  /* setup tcr_el2 */
  ldr x1, =TCR_EL2_VALUE
  msr tcr_el2, x1

  /* setup sctlr_el2 */
  mrs x1, sctlr_el2
  mov x2, #(SCTLR_A | SCTLR_I | SCTLR_C)
  orr x1, x1, x2
  msr sctlr_el2, x1

  b initsp    /* return to caller of initcpu */

initsp:
  adrp x1, _stack
  mov x2, #4096
  mrs x3, mpidr_el1
  and x3, x3, #0xf
  add x4, x3, #1
  mul x2, x2, x4
  add x1, x1, x2
  mov sp, x1

  isb

  ret

.macro ventry, label
.balign 0x80
  b \label
.endm

.balign 0x800
early_vectable:
/* current EL with sp0 */
  ventry  .
  ventry  .
  ventry  .
  ventry  .

/* current EL with spx */
  ventry  early_panic
  ventry  .
  ventry  .
  ventry  .

/* lower EL using aarch64 */
  ventry  .
  ventry  .
  ventry  .
  ventry  .

/* lower EL using aarch32 */
  ventry  .
  ventry  .
  ventry  .
  ventry  .

early_panic:
  mrs x0, elr_el2
  mrs x1, far_el2
  mrs x2, esr_el2
  b .

.section ".text"

/*
 *  x0: ttbr0_el2
 */
.global set_ttbr0_el2
set_ttbr0_el2:
  dsb sy
  isb

  tlbi  alle2 

  msr ttbr0_el2, x0 

  tlbi  alle2 
  ic  iallu 
  dsb sy

  isb

  ret

.global psci_call
psci_call:
  smc #0
  ret

.global hang
hang:
  wfi
  wfe
  b hang
