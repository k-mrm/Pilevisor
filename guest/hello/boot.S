.section ".text.boot"

.global _start

_start:
  mrs x1, mpidr_el1
  and x1, x1, #3
  cbz x1, kernel_boot

kernel_boot:
  adrp x0, _stack
  add x0, x0, #4096
  mov sp, x0
  bl main
  wfi

halt:
  b halt
