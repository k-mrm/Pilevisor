#include "aarch64.h"
#include "mm.h"

.section ".text.boot"

/*
 *  x0: dtb pointer
 */
.global _start
_start:
  INTR_DISABLE

  mrs x1, CurrentEL
  cmp x1, #0x8    // EL2
  b.eq startup
  cmp x1, #0xc    // EL3
  b.eq switch_el2
  bl hang         // panic

switch_el2:
  mov x1, #(SCR_NS | SCR_SMD | SCR_HCE | SCR_RW | SCR_RES1) 
  msr scr_el3, x1
  adr x2, startup
  msr elr_el3, x2
  mov x3, #0x3c9    // DAIF, EL2H
  msr spsr_el3, x3

  eret

startup:
  bl bss_clear
  bl initcpu
  // bl enable_boot_mmu

  b startcore

startcore:
  /* in EL2 */
  adrp x1, _stack
  mov x2, #4096
  mrs x3, mpidr_el1
  and x3, x3, #0xf
  add x4, x3, #1
  mul x2, x2, x4
  add x1, x1, x2
  mov sp, x1
  cbz x3, vmm_init_cpu0
  bl vmm_init_secondary

  /* spin */
  bl hang

enable_boot_mmu:
  tlbi alle2
  dsb sy

  /* enable MMU */
  mrs x1, sctlr_el2
  orr x1, x1, #(SCTLR_M)
  msr sctlr_el2, x1

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
  /* setup mair_el2 */
  ldr x1, =MAIR_VALUE
  msr mair_el2, x1

  /* setup tcr_el2 */
  mov x1, #0
  msr tcr_el2, x1

  /* setup sctlr_el2 */
  mrs x1, sctlr_el2
  mov x2, #(SCTLR_A | SCTLR_I | SCTLR_C)
  orr x1, x1, x2
  msr sctlr_el2, x1

  ret

.section ".text"

.global psci_call
psci_call:
  smc #0
  ret

.global hang
hang:
  wfi
  wfe
  b hang
