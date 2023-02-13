/*
 *  void dcache_flush_poc(u64 va_start, u64 va_end)
 */
.global dcache_flush_poc
dcache_flush_poc:
  mrs x2, ctr_el0
  ubfx x4, x2, #16, #4
  mov x3, #4
  lsl x3, x3, x4
  sub x4, x3, #1
  bic x4, x0, x4
dci:
  dc civac, x4
  add x4, x4, x3
  cmp x4, x1
  b.lt dci

  dsb ish
  isb

  ret

/*
 *  cache_sync_pou(void *va_start, void *va_end)
 */
.global cache_sync_pou
cache_sync_pou:
  // flush D
  mrs x2, ctr_el0
  ubfx x4, x2, #16, #4
  mov x3, #4
  lsl x3, x3, x4
  sub x4, x3, #1
  bic x4, x0, x4
cdc:
  dc cvau, x4
  add x4, x4, x3
  cmp x4, x1
  b.lt cdc

  dsb ish

  // flush I
  and x2, x2, #0xf
  mov x3, #4
  lsl x3, x3, x2
  sub x4, x3, #1
  bic x4, x0, x4
cic:
  ic ivau, x4
  add x4, x4, x3
  cmp x4, x1
  b.lt cic

  dsb ish
  isb

  ret
