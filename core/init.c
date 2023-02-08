/*
 *  vmm initialization sequeunce
 */

#include "uart.h"
#include "aarch64.h"
#include "guest.h"
#include "pcpu.h"
#include "allocpage.h"
#include "mm.h"
#include "log.h"
#include "vgic.h"
#include "pci.h"
#include "log.h"
#include "psci.h"
#include "node.h"
#include "virtio-mmio.h"
#include "malloc.h"
#include "iomem.h"
#include "arch-timer.h"
#include "panic.h"

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

volatile static int cpu0_ready = 0;

static void hcr_setup() {
  u64 hcr = HCR_VM | HCR_SWIO | HCR_AMO | HCR_FMO | HCR_IMO |
            HCR_PTW | HCR_RW | HCR_TSC /* | HCR_TDZ */;

  write_sysreg(hcr_el2, hcr);

  isb();
}

int vmm_init_secondary() {
  setup_pagetable_secondary();

  trapinit();
  pcpu_init_core();

  irqchip_init_core();

  arch_timer_init_core();

  hcr_setup();

  localvm_initcore();

  localnode.ctl->startcore();

  panic("unreachable");
}

int vmm_init_cpu0(void *fdt_phys) {
  // void *fdt;
  printf("vmm init cpu0\n");

  early_allocator_init();
  // fdt = early_fdt_map(fdt_phys);

  iomem_init();
  setup_pagetable((u64)fdt_phys);

  trapinit();

  irqchip_init();
  irqchip_init_core();

  gpio_init();
  mailbox_init();

  uart_init();
  printf("vmm booting...\n");

  pageallocator_init();

  psci_init();

  pcpu_init();
  pcpu_init_core();

  arch_timer_init();
  arch_timer_init_core();

  // virtio_mmio_init();
  peripheral_device_init();

  hcr_setup();

  msg_sysinit();

  nodectl_init();

  localvm_init(1, 256 * MiB, &virt_dtb);
  localvm_initcore();

  localnode.ctl->init();
  localnode.ctl->startcore();

  panic("unreachable");
}
