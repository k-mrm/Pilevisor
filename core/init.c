#include "uart.h"
#include "aarch64.h"
#include "kalloc.h"
#include "guest.h"
#include "vm.h"
#include "pcpu.h"
#include "mm.h"
#include "log.h"
#include "vgic.h"
#include "vtimer.h"
#include "pci.h"
#include "log.h"
#include "psci.h"

extern struct guest xv6_img;
extern struct guest linux_img;
extern struct guest virt_dtb;
extern struct guest rootfs_img;

void _start(void);
void vectable();

__attribute__((aligned(16))) char _stack[4096*NCPU];

volatile static int cpu0_ready = 0;

static void hcr_setup() {
  u64 hcr = HCR_VM | HCR_SWIO | HCR_FMO | HCR_IMO |
            /*HCR_TWI | HCR_TWE |*/ HCR_RW | HCR_TSC | HCR_TID3;

  write_sysreg(hcr_el2, hcr);

  isb();
}

int vmm_init_secondary() {
  vmm_log("cpu%d activated\n", cpuid());
  write_sysreg(vbar_el2, (u64)vectable);
  gic_init_cpu(cpuid());
  s2mmu_init();
  hcr_setup();

  enter_vcpu();

  panic("unreachable");
}

int vmm_init_cpu0() {
  uart_init();
  vmm_log("vmm booting...\n");
  kalloc_init();
  pcpu_init();
  write_sysreg(vbar_el2, (u64)vectable);
  vgic_init();
  gic_init();
  gic_init_cpu(0);
  vtimer_init();
  vcpu_init();
  s2mmu_init();
  // pci_init();
  hcr_setup();

  struct vmconfig vmcfg = {
    .guest_img = &linux_img,
    .fdt_img = &virt_dtb,
    .initrd_img = &rootfs_img,
    .nvcpu = 8,
    .nallocate = 128 * 1024 * 1024,
    .entrypoint = 0x40080000,
  };

  create_vm(&vmcfg);

  enter_vcpu();

  panic("unreachable");
}
