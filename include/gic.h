#ifndef DRIVER_GIC_H
#define DRIVER_GIC_H

#include "types.h"
#include "aarch64.h"

#define GIC_NSGI            16
#define GIC_SGI_MAX         15
#define GIC_NPPI            16
#define GIC_PPI_MAX         31

#define is_sgi(intid)       (0 <= (intid) && (intid) < 16)
#define is_ppi(intid)       (16 <= (intid) && (intid) < 32)
#define is_sgi_ppi(intid)   (is_sgi(intid) || is_ppi(intid))
#define is_ppi_spi(intid)   (16 <= (intid) && (intid) < 1020)
#define is_spi(intid)       (32 <= (intid) && (intid) < 1020)
#define valid_intid(intid)  (0 <= (intid) && (intid) < 1020)

#define LR_INACTIVE         0L
#define LR_PENDING          1L
#define LR_ACTIVE           2L
#define LR_PENDACT          3L
#define LR_MASK             3L

#define GICD_CTLR           0x0
#define GICD_TYPER          0x4
#define GICD_IIDR           0x8
#define GICD_TYPER2         0xc
#define GICD_STATUSR        0x10
#define GICD_IGROUPR(n)     (0x080 + (u64)(n) * 4)
#define GICD_ISENABLER(n)   (0x100 + (u64)(n) * 4)
#define GICD_ICENABLER(n)   (0x180 + (u64)(n) * 4)
#define GICD_ISPENDR(n)     (0x200 + (u64)(n) * 4)
#define GICD_ICPENDR(n)     (0x280 + (u64)(n) * 4)
#define GICD_ISACTIVER(n)   (0x300 + (u64)(n) * 4)
#define GICD_ICACTIVER(n)   (0x380 + (u64)(n) * 4)
#define GICD_IPRIORITYR(n)  (0x400 + (u64)(n) * 4)
#define GICD_ITARGETSR(n)   (0x800 + (u64)(n) * 4)
#define GICD_ICFGR(n)       (0xc00 + (u64)(n) * 4)
#define GICD_SGIR           0xf00
#define GICD_IROUTER(n)     (0x6000 + (u64)(n) * 8)
#define GICD_PIDR2          0xffe8

#define GICD_TYPER_CPUNum_SHIFT   5
#define GICD_TYPER_IDbits_SHIFT   19

#define GICD_IIDR_Revision_SHIFT    12
#define GICD_IIDR_ProductID_SHIFT   24

#define GICD_PIDR2_ArchRev(pidr2)   (((pidr2)>>4) & 0xf)
#define GICD_PIDR2_ArchRev_SHIFT    4

enum sgi_mode {
  SGI_ROUTE_TARGETS   = 0,
  SGI_ROUTE_BROADCAST = 1,
  SGI_ROUTE_SELF      = 2,
};

struct gic_sgi {
  int sgi_id;
  /* TODO: Affinity? */
  u16 targets;
  enum sgi_mode mode;
};

struct gic_pending_irq {
  int virq;
  struct irq *pirq;   // hw irq
  int group;
  int priority;
  int req_cpu;        // only sgi
};

struct gic_irqchip {
  int version;      // 2 or 3
  int nirqs;
  int max_lr;

  void (*initcore)(void);
  int (*inject_guest_irq)(struct gic_pending_irq *irq);
  bool (*irq_pending)(u32 irq);
  bool (*guest_irq_pending)(u32 irq);
  void (*host_eoi)(u32 iar);
  void (*guest_eoi)(u32 iar);
  void (*deactive_irq)(u32 irq);
  void (*send_sgi)(struct gic_sgi *sgi);
  bool (*irq_enabled)(u32 irq);
  void (*enable_irq)(u32 irq);
  void (*disable_irq)(u32 irq);
  void (*setup_irq)(u32 irq);
  void (*set_targets)(u32 irq, u8 targets);
  void (*route_irq)(u32 irq, u64 mpidr);
  void (*irq_handler)(int from_guest);
};

extern struct gic_irqchip irqchip;

struct gic_state {
  u64 lr[16];
  u64 vmcr;
  u32 sre_el1;
};

void irqchip_init(void);
void irqchip_init_core(void);

#endif
