/*
 *  PCI driver
 */

#include "pci.h"
#include "memmap.h"
#include "log.h"
#include "panic.h"

struct pci_config_space {
  u8 raw[256][32][8][4096];
};

static int pci_baseaddr_map(u32 size, bool mem64, u64 *addr) {
  static u64 pci_mmio_base32 = 0x10000000;
  static u64 pci_mmio_base64 = 0x8000000000;

  if(mem64) {
    *addr = pci_mmio_base64;
    pci_mmio_base64 += size;
  } else {
    *addr = pci_mmio_base32;
    pci_mmio_base32 += size;
  }

  return 0;
}

static void pci_func_enable(struct pci_dev *dev) {
  struct pci_config *cfg = dev->cfg;

  dev->vndr_id = cfg->vendor_id;
  dev->dev_id = cfg->device_id;

  vmm_log("PCI %x:%x.%x enable(%x:%x)\n", dev->bus, dev->device, dev->func, dev->vndr_id, dev->dev_id);

  cfg->command |= (1 << 0) | (1 << 1) | (1 << 2);

  /* only header_type 0 */
  for(int i = 0; i < 6; i++) {
    bool mem64 = false;
    u64 oldv = cfg->bar[i];
    cfg->bar[i] = 0xffffffff;
    isb();
    u32 rv = cfg->bar[i];

    if(rv == 0)
      continue;

    if(PCI_BAR_TYPE(oldv) == PCI_BAR_TYPE_MEM) {
      if(PCI_BAR_MEM_TYPE(oldv) == PCI_BAR_MEM_TYPE_64)
        mem64 = true;

      u64 addr = oldv & 0xfffffff0;
      u32 size = ~(rv & 0xfffffff0) + 1;

      if(addr == 0) {
        if(pci_baseaddr_map(size, mem64, &addr) < 0)
          panic("pci");
        oldv = addr | (oldv & 0xf);
      }

      dev->reg_addr[i] = addr;
      dev->reg_size[i] = size;

      vmm_log("happy typhoon %p %p %dbit\n", addr, size, mem64 ? 64 : 32);
    } else {  // PCI_BAR_TYPE(oldv) == PCI_BAR_TYPE_IO
      /* TODO */
    }

    if(mem64) {
      cfg->bar[i] = oldv & 0xffffffff;
      cfg->bar[++i] = (oldv >> 32) & 0xffffffff;
    } else {
      cfg->bar[i] = oldv;
    }
  }
}

static void pci_scan_bus() {
  struct pci_dev dev;
  struct pci_config *cfg;
  struct pci_config_space *space = (struct pci_config_space *)PCIE_ECAM_BASE;

  for(int bc = 0; bc < 256; bc++)
    for(int dc = 0; dc < 32; dc++)
      for(int fc = 0; fc < 8; fc++) {
        cfg = (struct pci_config *)&space->raw[bc][dc][fc];
        if(cfg->vendor_id == 0xffff)
          continue;
        if(cfg->header_type != 0) /* unsupport */
          continue;

        dev.bus = bc;
        dev.device = dc;
        dev.func = fc;
        dev.cfg = cfg;
        
        pci_func_enable(&dev);

        /* VIRTIO vendor id */
        if(dev.vndr_id == 0x1af4) {
          virtio_pci_dev_init(&dev);
        }
      }
}

void pci_init() {
  pci_scan_bus();
}
