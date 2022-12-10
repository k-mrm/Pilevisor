#include "aarch64.h"
#include "tlb.h"
#include "mm.h"

void set_vmm_pgt(u64 *pgt) {
  tlb_vmm_flush_all();
  dsb(sy);

  write_sysreg(ttbr0_el2, (u64)pgt);

  isb();
}
