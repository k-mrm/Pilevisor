#include "aarch64.h"

.section ".text.boot"

enable_boot_mmu:
  mrs x0, sctlr_el2
  orr x0, x0, #SCTLR_M
  msr sctlr_el2, x0

  ret

/*
 *  x0: dtb pointer
 */
.global _start
_start:
  INTR_DISABLE

  mrs x1, CurrentEL
  cmp x1, #0x8    // EL2
  b.eq startcore
  cmp x1, #0xc    // EL3
  b.eq switch_el2
  bl hang         // panic

switch_el2:
  mov x1, #(SCR_NS | SCR_SMD | SCR_HCE | SCR_RW | SCR_RES1) 
  msr scr_el3, x1
  adr x2, startcore
  msr elr_el3, x2
  mov x3, #0x3c9    // DAIF, EL2H
  msr spsr_el3, x3

  eret

startcore:
  /* in EL2 */
  adrp x1, _stack
  mov x2, #4096
  mrs x3, mpidr_el1
  and x3, x3, #0xf
  add x4, x3, 1
  mul x2, x2, x4
  add x1, x1, x2
  mov sp, x1
  cbz x3, vmm_init_cpu0
  bl vmm_init_secondary

  /* spin */
  bl hang

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
