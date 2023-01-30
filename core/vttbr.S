.section ".text"

/*
 *  switch_vttbr(physaddr_t vttbr)
 */
.global switch_vttbr
switch_vttbr:
  dsb sy
  isb

  msr vttbr_el2, x0

  dsb sy
  isb

  ret
