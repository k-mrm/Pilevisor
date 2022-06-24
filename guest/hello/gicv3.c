#include "gicv3.h"
#include "uart.h"
#include "hello.h"

typedef _Bool bool;

#define true 1
#define false 0

#define GICBASE     0x08000000L

#define GICD_CTLR           (0x0)
#define GICD_TYPER          (0x4)
#define GICD_IGROUPR(n)     (0x80 + (u64)(n) * 4)
#define GICD_ISENABLER(n)   (0x100 + (u64)(n) * 4)
#define GICD_ICENABLER(n)   (0x180 + (u64)(n) * 4)
#define GICD_ISPENDR(n)     (0x200 + (u64)(n) * 4)
#define GICD_ICPENDR(n)     (0x280 + (u64)(n) * 4)
#define GICD_IPRIORITYR(n)  (0x400 + (u64)(n) * 4)
#define GICD_ITARGETSR(n)   (0x800 + (u64)(n) * 4)
#define GICD_ICFGR(n)       (0xc00 + (u64)(n) * 4)

#define GICR_CTLR           (0x0)
#define GICR_WAKER          (0x14)

#define SGI_BASE  0x10000
#define GICR_IGROUPR0       (SGI_BASE+0x80)
#define GICR_ISENABLER0     (SGI_BASE+0x100)
#define GICR_ICENABLER0     (SGI_BASE+0x180)
#define GICR_ICPENDR0       (SGI_BASE+0x280)
#define GICR_IPRIORITYR(n)  (SGI_BASE+0x400+(n)*4)
#define GICR_ICFGR0         (SGI_BASE+0xc00)
#define GICR_ICFGR1         (SGI_BASE+0xc04)
#define GICR_IGRPMODR0      (SGI_BASE+0xd00)

static bool is_sgi_ppi(u32 id);
bool gic_enabled(void);

static inline u32 icc_igrpen1_el1() {
  u32 x;
  asm volatile("mrs %0, S3_0_C12_C12_7" : "=r"(x));
  return x;
}

static inline void w_icc_igrpen1_el1(u32 x) {
  asm volatile("msr S3_0_C12_C12_7, %0" : : "r"(x));
}

static inline u32 icc_pmr_el1() {
  u32 x;
  asm volatile("mrs %0, S3_0_C4_C6_0" : "=r"(x));
  return x;
}

static inline void w_icc_pmr_el1(u32 x) {
  asm volatile("msr S3_0_C4_C6_0, %0" : : "r"(x));
}

static inline u32 icc_iar1_el1() {
  u32 x;
  asm volatile("mrs %0, S3_0_C12_C12_0" : "=r"(x));
  return x;
}

static inline void w_icc_eoir1_el1(u32 x) {
  asm volatile("msr S3_0_C12_C12_1, %0" : : "r"(x));
}

static inline u32 icc_sre_el1() {
  u32 x;
  asm volatile("mrs %0, S3_0_C12_C12_5" : "=r"(x));
  return x;
}

static inline void w_icc_sre_el1(u32 x) {
  asm volatile("msr S3_0_C12_C12_5, %0" : : "r"(x));
}

static struct {
  char *gicd;
  char *rdist_addrs[1];
} gicv3;

static void wd32(u32 off, u32 val) {
  *(volatile u32 *)(gicv3.gicd + off) = val;
}

static u32 rd32(u32 off) {
  return *(volatile u32 *)(gicv3.gicd + off);
}

static void wr32(u32 cpuid, u32 off, u32 val) {
  *(volatile u32 *)(gicv3.rdist_addrs[cpuid] + off) = val;
}

static u32 rr32(u32 cpuid, u32 off) {
  return *(volatile u32 *)(gicv3.rdist_addrs[cpuid] + off);
}

static void gic_enable_int(u32 intid) {
  u32 is = rd32(GICD_ISENABLER(intid / 32));
  is |= 1 << (intid % 32);
  wd32(GICD_ISENABLER(intid / 32), is);
}

static void gicr_enable_int(u32 cpuid, u32 intid) {
  u32 is = rr32(cpuid, GICR_ISENABLER0);
  is |= 1 << (intid % 32);
  wr32(cpuid, GICR_ISENABLER0, is);
}

static bool gicr_int_enabled(u32 cpuid, u32 intid) {
  u32 is = rr32(cpuid, GICR_ISENABLER0);
  return is & (1 << (intid % 32));
}

static void gic_clear_pending(u32 intid) {
  u32 ic = rd32(GICD_ICPENDR(intid / 32));
  ic |= 1 << (intid % 32);
  wd32(GICD_ICPENDR(intid / 32), ic);
}

static void gicr_clear_pending(u32 cpuid, u32 intid) {
  u32 ic = rr32(cpuid, GICR_ICPENDR0);
  ic |= 1 << (intid % 32);
  wr32(cpuid, GICR_ICPENDR0, ic);
}

static void gic_set_prio(u32 intid, u32 prio) {
  u32 p = rd32(GICD_IPRIORITYR(intid / 4));
  p &= ~((u32)0xff << (intid % 4 * 8)); // set prio 0
  wd32(GICD_IPRIORITYR(intid / 4), p);
}

static void gicr_set_prio(u32 cpuid, u32 intid, u32 prio) {
  u32 p = rr32(cpuid, GICR_IPRIORITYR(intid / 4));
  p &= ~((u32)0xff << (intid % 4 * 8)); // set prio 0
  wr32(cpuid, GICR_IPRIORITYR(intid / 4), p);
}

static void gic_set_target(u32 intid, u32 cpuid) {
  u32 itargetsr = rd32(GICD_ITARGETSR(intid / 4));
  itargetsr &= ~((u32)0xff << (intid % 4 * 8));
  wd32(GICD_ITARGETSR(intid / 4), itargetsr | ((u32)(1 << cpuid) << (intid % 4 * 8)));
}

static void gicr_wait_rwp(u32 cpuid) {
  u32 ctlr = rr32(cpuid, GICR_CTLR);
  while((ctlr >> 3) & 1)  // RWP
    ;
}

static void gic_setup_ppi(u32 cpu, u32 intid, int prio) {
  gicr_set_prio(cpu, intid, prio);
  gicr_clear_pending(cpu, intid);
  gicr_enable_int(cpu, intid);
}

static void gic_setup_spi(u32 intid, int prio) {
  gic_set_prio(intid, prio);
  gic_set_target(intid, 0);
  gic_clear_pending(intid);
  gic_enable_int(intid);
}

static void gic_cpu_init() {
  w_icc_igrpen1_el1(0);

  w_icc_pmr_el1(0xff);
}

static void gic_dist_init() {
  wd32(GICD_CTLR, 0);

  u32 typer = rd32(GICD_TYPER);
  u32 lines = typer & 0x1f;

  uart_put64(typer, 16);
  uart_puts(" nnnnnn ");
  uart_put64(32 * (lines + 1) - 1, 10);

  for(int i = 0; i < lines; i++)
    wd32(GICD_IGROUPR(i), ~0);
}

static void gic_redist_init(u32 cpuid) {
  wr32(cpuid, GICR_CTLR, 0);

  w_icc_sre_el1(icc_sre_el1() | 1);

  /* Non-secure Group1 */
  wr32(cpuid, GICR_IGROUPR0, ~0);
  wr32(cpuid, GICR_IGRPMODR0, 0);

  // wr32(cpuid, GICR_ICFGR1, 0);

  /* enable redist */
  u32 waker = rr32(cpuid, GICR_WAKER);
  wr32(cpuid, GICR_WAKER, waker & ~(1<<1));
  while(rr32(cpuid, GICR_WAKER) & (1<<2))
    ;
}

static void gic_enable() {
  /* enable Group0/Non-secure Group1 */
  wd32(GICD_CTLR, 3);

  w_icc_igrpen1_el1(1);
}

void gicv3_init_percpu() {
  gic_cpu_init();
  gic_redist_init(0);

  gic_setup_ppi(0, TIMER_IRQ, 0);

  gic_enable();
}

void gicv3_init() {
  uart_puts("gicv3 init...\n");
  gicv3.gicd = (char *)GICBASE;
  for(int i = 0; i < 1; i++) {
    gicv3.rdist_addrs[i] = (char *)(GICBASE + 0xa0000 + (i) * 0x20000);
  }

  gic_dist_init();

  gic_setup_spi(UART_IRQ, 0);
}

bool gic_enabled() {
  return (icc_igrpen1_el1() & 0x1) && (rd32(GICD_CTLR) & 0x1);
}

u32 gic_iar() {
  return icc_iar1_el1();
}

void gic_eoi(u32 iar) {
  w_icc_eoir1_el1(iar);
}

static bool is_sgi_ppi(u32 id) {
  if(0 <= id && id < 32)
    return true;
  else
    return false;
}
