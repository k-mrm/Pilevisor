#include "aarch64.h"
#include "mm.h"
#include "memlayout.h"

.section ".text.head"

_head:
  b _start

.section ".text.boot"

#ifdef RPI4
#include "earlypl011-rpi.S.inc"
#else   /* !RPI4 */
#include "earlypl011.S.inc"
#endif  /* RPI4 */

/*
 *  volatile x19, x20, x21, x22
 */
.macro earlyputs, s
  adr x19, 2f
1:
  ldrb w20, [x19], #1
  cbz w20, 3f
  earlycon_tx w20, x21, w22
  b 1b

  .pushsection .rodata.str, "aS"
2:.asciz "\s"
  .popsection  

3:
.endm

.macro earlyputn, n
  adr x19, hexstr
  mov x20, #60

1:
  mov x21, \n
  lsr x21, x21, x20
  and x21, x21, #0xf
  ldrb w22, [x19, x21]
  earlycon_tx w22, x23, w24
  cbz x20, 2f
  sub x20, x20, 4
  b 1b
2:
  mov w22, #10        // \n
  earlycon_tx w22, x23, w24
.endm

.pushsection .rodata.str, "aS"
hexstr: .asciz "0123456789abcdef"
.popsection

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
  bl clear_bss

  bl earlycon_init
  earlyputs "boot cpu init...\r\n"

  bl clear_pgts

  bl setup_idmap_pagetable
  bl setup_boot_pagetable

  bl initcpu

  ldr x30, =vmm_init_cpu0
  b enable_boot_mmu   // return to x30: vmm_init_cpu0 (init.c)

  /* unreachable */
  brk #0

startupothers:
  bl initcpu

  ldr x30, =vmm_init_secondary
  b enable_mmu_secondary   // return to x30: vmm_init_secondary (init.c)

  /* unreachable */
  brk #0

.macro set_block_r, xpgt, idx, addr, level, memtype
  bic x9, \addr, #((1 << (39 - \level * 9)) - 1)
  mov x10, #(PTE_AF | PTE_VALID | \memtype)
  orr x9, x9, x10
  str x9, [\xpgt, \idx, lsl #3]
.endm

.macro set_block, pgt, idx, addr, level, memtype
  adrp x8, \pgt
  set_block_r x8, \idx, \addr, \level, \memtype
.endm

.macro set_entry, pgt, idx, addr, memtype
  adrp x8, \pgt
  bic x9, \addr, #((1 << (39 - 3 * 9)) - 1)
  mov x10, #(PTE_AF | PTE_V | \memtype)
  orr x9, x9, x10
  str x9, [x8, \idx, lsl #3]
.endm 

.macro set_entry_r, xpgt, idx, addr, memtype, tmp1, tmp2
  bic \tmp1, \addr, #((1 << (39 - 3 * 9)) - 1)
  mov \tmp2, #(PTE_AF | PTE_V | \memtype)
  orr \tmp1, \tmp1, \tmp2
  str \tmp1, [\xpgt, \idx, lsl #3]
.endm

.macro get_slot, pgt, idx, ent, tmp
  adrp \tmp, \pgt
  ldr \ent, [\tmp, \idx, lsl #3]
.endm

.macro get_slot_r, xpgt, idx, ent
  ldr \ent, [\xpgt, \idx, lsl #3]
.endm

.macro get_slot_nl, pgt, idx, nl
  adrp x8, \pgt
  ldr \nl, [x8, \idx, lsl #3]
  ubfx \nl, \nl, #PAGESHIFT, 36
  lsl \nl, \nl, #PAGESHIFT
.endm

.macro set_table_r, xpgt, idx, table, tmp
  mov \tmp, \table
  orr \tmp, \tmp, #(PTE_TABLE | PTE_VALID)
  str \tmp, [\xpgt, \idx, lsl #3]
.endm

.macro set_table, pgt, idx, table, tmp1, tmp2
  adrp \tmp1, \pgt
  adrp \tmp2, \table
  orr \tmp2, \tmp2, #(PTE_TABLE | PTE_VALID)
  str \tmp2, [\tmp1, \idx, lsl #3]
.endm

.macro pgt_index, idx, va, level
  ubfx  \idx, \va, #(39 - \level * 9), 9 
.endm 

/*
 *  make identity mapping pa(__idmap_start) to pa(__idmap_end)
 *                        pa(EATLY_PL011_BASE)
 */
setup_idmap_pagetable:
  adrp x1, __idmap_start

  pgt_index x3, x1, 1     // x3 = PIDX(1, x1)
  set_table __boot_pgt_l1, x3, __idmap_pgt_l2_1, x9, x10

  pgt_index x4, x1, 2     // x4 = PIDX(2, x1)
  set_table __idmap_pgt_l2_1, x4, __idmap_pgt_l3_1, x9, x10

  pgt_index x5, x1, 3     // x5 = PIDX(3, x1)
  set_entry __idmap_pgt_l3_1, x5, x1, PTE_NORMAL

  earlyputs "setup idmap pagetable\r\n"

  /* map earlyconsole */
  ldr x1, =EARLY_PL011_BASE

  pgt_index x2, x1, 1                   // x2 = PIDX(1, x1)
  get_slot_nl  __boot_pgt_l1, x2, x3    // x3 = next_level_addr(__boot_pgt_l1[x2])
  cbnz x3, 2f

  set_table __boot_pgt_l1, x2, __idmap_pgt_l2_2, x9, x10
  adrp x3, __idmap_pgt_l2_2

2:
  pgt_index x4, x1, 2     // x4 = PIDX(2, x1)

  adrp x9, __idmap_pgt_l3_2
  set_table_r x3, x4, x9, x10

  pgt_index x5, x1, 3     // x5 = PIDX(3, x1)
  set_entry __idmap_pgt_l3_2, x5, x1, PTE_DEVICE_nGnRE

  ret

/*
 *  make linear mapping pa(vmm_start)-pa(vmm_end) to va(vmm_start)-va(vmm_end) 
 */
setup_boot_pagetable:
  earlyputs "setup boot pagetable...\r\n"

  adrp x1, vmm_start        // x1 = pa(vmm_start)
  ldr x2, =vmm_start        // x2 = va(vmm_start)
  ldr x3, =vmm_end          // x3 = va(vmm_end)

  pgt_index x4, x2, 1       // x4 = PIDX(1, x2)

  get_slot_nl __boot_pgt_l1, x4, x5    // x5 = next_level_addr(__boot_pgt_l1[x4])
  cbnz x5, 1f

  set_table __boot_pgt_l1, x4, __boot_pgt_l2, x9, x10
  adrp x5, __boot_pgt_l2

  /*
   *  setup level 2 table
   *  x5: level 2 page table address
   */
1:
  pgt_index x6, x2, 2       // x6 = PIDX(2, x2) 
  pgt_index x7, x3, 2       // x7 = PIDX(2, x3)

  adrp x8, __boot_pgt_l3

2:
  set_table_r x5, x6, x8, x9

  /* x8 - level 3 table */
  pgt_index x9, x2, 3       // x9 = PIDX(3, x2)

3:
  set_entry_r x8, x9, x1, PTE_NORMAL, x11, x12

  add x9, x9, #1
  add x1, x1, #PAGESIZE
  cmp x9, 511
  b.ls 3b

  add x6, x6, 1
  add x8, x8, #PAGESIZE
  cmp x6, x7
  b.ls 2b

  ret

pt_crashed:
  earlyputs "abort: crashed pagetable\r\n"
  brk #0    // goto earlypanic

enable_boot_mmu:
  earlyputs "enable boot mmu...\r\n"

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

clear_bss:
  adrp x1, __bss_start
  ldr w2, =__bss_size
1:
  cbz w2, 2f
  str xzr, [x1], #8
  sub w2, w2, #1
  b 1b
2:
  ret

clear_pgts:
  adrp x1, __boot_pagetable_start
  adrp x2, __boot_pagetable_end

  earlyputn x1
  earlyputn x2
1: 
  str xzr, [x1], #8
  cmp x1, x2
  b.eq 2f
  b 1b
2:
  ret

initcpu:
  earlyputs "initcpu...\r\n"

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
  
  /* init sp */
  ldr x1, =_stack
  mov x2, #4096
  mrs x3, mpidr_el1
  and x3, x3, #0xf
  add x4, x3, #1
  mul x2, x2, x4
  add x1, x1, x2
  mov sp, x1

  isb

  ret

.section ".text"

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
  mrs x12, elr_el2
  mrs x13, far_el2
  mrs x14, esr_el2

  earlyputs "earlypanic!\r\n"

  earlyputs "elr_el2: "
  earlyputn x12
  earlyputs "far_el2: "
  earlyputn x13
  earlyputs "esr_el2: "
  earlyputn x14
  earlyputs "x30(lr): "
  earlyputn x30

  b .

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
