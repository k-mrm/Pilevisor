#include "uart.h"
#include "aarch64.h"
#include "guest.h"
#include "pcpu.h"
#include "allocpage.h"
#include "mm.h"
#include "log.h"
#include "vgic.h"
#include "vtimer.h"
#include "pci.h"
#include "log.h"
#include "psci.h"
#include "node.h"
#include "virtio-mmio.h"

#define KiB   (1024)
#define MiB   (1024 * 1024)
#define GiB   (1024 * 1024 * 1024)

extern char _binary_virt_dtb_start[];
extern char _binary_virt_dtb_size[];

struct guest virt_dtb = {
  .name = "virt dtb",
  .start = (u64)_binary_virt_dtb_start,
  .size = (u64)_binary_virt_dtb_size,
};

void _start(void);
void vectable();

volatile static int cpu0_ready = 0;

static void hcr_setup() {
  u64 hcr = HCR_VM | HCR_SWIO | HCR_FMO | HCR_IMO |
            HCR_RW | HCR_TSC | HCR_TDZ;

  write_sysreg(hcr_el2, hcr);

  isb();
}

int vmm_init_secondary() {
  vmm_log("cpu%d activated...\n", cpuid());
  pcpu_init();
  vcpu_init_core();
  write_sysreg(vbar_el2, (u64)vectable);
  gic_init_cpu();
  s2mmu_init();
  hcr_setup();
  write_sysreg(vttbr_el2, localnode.vttbr);

  localnode.ctl->start();

  panic("unreachable");
}

int vmm_init_cpu0() {
  uart_init();
  vmm_log("vmm booting...\n");
  pcpu_init();
  vcpu_init_core();
  pageallocator_init();
  write_sysreg(vbar_el2, (u64)vectable);
  hcr_setup();
  gic_init();
  gic_init_cpu();
  vtimer_init();
  s2mmu_init();
  // pci_init();
  virtio_mmio_init();

  nodectl_init();

  node_preinit(1, 128 * MiB, &virt_dtb);

  localnode.ctl->init();
  localnode.ctl->start();

  panic("unreachable");
}
