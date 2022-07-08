#include "aarch64.h"
#include "vcpu.h"
#include "printf.h"
#include "lib.h"
#include "pcpu.h"
#include "log.h"
#include "mm.h"
#include "spinlock.h"
#include "memmap.h"

static struct vcpu vcpus[VCPU_MAX];
static spinlock_t vcpus_lk;

static void save_sysreg(struct vcpu *vcpu);
static void restore_sysreg(struct vcpu *vcpu);

void vcpu_init() {
  spinlock_init(&vcpus_lk);
}

static struct vcpu *allocvcpu() {
  acquire(&vcpus_lk);

  for(struct vcpu *vcpu = vcpus; vcpu < &vcpus[VCPU_MAX]; vcpu++) {
    if(vcpu->state == UNUSED) {
      vcpu->state = CREATED;
      release(&vcpus_lk);
      return vcpu;
    }
  }

  release(&vcpus_lk);

  return NULL;
}

static void vcpu_features_init(struct vcpu *vcpu) {
  u64 pfr0;
  read_sysreg(pfr0, ID_PFR0_EL1);

  vmm_log("pfr0 %p\n", pfr0);
  /* test: Disable EL2 */
  pfr0 &= ~(0xf << 8); 

  vcpu->features.pfr0 = pfr0;
}

struct vcpu *new_vcpu(struct node *node, int vcpuid) {
  struct vcpu *vcpu = allocvcpu();
  if(!vcpu)
    panic("vcpu kokatsu");

  vcpu->name = "cortex-a72";
  vcpu->node = node;
  vcpu->cpuid = vcpuid;

  vcpu->vgic = new_vgic_cpu(vcpuid);

  vcpu->reg.spsr = 0x3c5;   /* EL1 */

  vcpu->sys.mpidr_el1 = vcpuid; /* TODO: affinity */
  vcpu->sys.midr_el1 = 0x410fd081;  /* cortex-a72 */
  vcpu->sys.sctlr_el1 = 0xc50838;
  vcpu->sys.cntfrq_el0 = 62500000;

  if(vcpuid == 0) {
    /* linux https://www.kernel.org/doc/Documentation/arm64/booting.txt */
    vcpu->reg.x[0] = node->fdt_base;    /* fdt address */
    vcpu->reg.x[4] = node->entrypoint;  /* entry point */
  }

  vcpu_features_init(vcpu);

  gic_init_state(&vcpu->gic);

  return vcpu;
}

void vcpu_ready(struct vcpu *vcpu) {
  vmm_log("vcpu ready %d\n", vcpu->cpuid);

  vcpu->state = READY;
}

void trapret(void);

static void switch_vcpu(struct vcpu *vcpu) {
  write_sysreg(tpidr_el2, vcpu);

  if(vcpu->cpuid != cpuid())
    panic("cpu%d: illegal vcpu%d", cpuid(), vcpu->cpuid);

  vcpu->state = RUNNING;

  write_sysreg(vttbr_el2, vcpu->node->vttbr);
  tlb_flush();
  restore_sysreg(vcpu);
  gic_restore_state(&vcpu->gic);

  isb();

  vmm_log("enter vcpu%d enter %p\n", vcpu->cpuid, vcpu->reg.elr);

  vcpu_dump(vcpu);

  /* enter vm */
  trapret();

  panic("unreachable");
}

void enter_vcpu() {
  int id = cpuid();
  struct vcpu *vcpu = &vcpus[id];

  while(vcpu->state != READY)
    wfi();

  vmm_log("cpu%d: entering vcpu%d\n", id, vcpu->cpuid);

  vcpu->reg.elr = vcpu->node->entrypoint;

  switch_vcpu(vcpu);
}

static void save_sysreg(struct vcpu *vcpu) {
  read_sysreg(vcpu->sys.spsr_el1, spsr_el1);
  read_sysreg(vcpu->sys.elr_el1, elr_el1);
  read_sysreg(vcpu->sys.mpidr_el1, mpidr_el1);
  read_sysreg(vcpu->sys.midr_el1, midr_el1);
  read_sysreg(vcpu->sys.sp_el0, sp_el0);
  read_sysreg(vcpu->sys.sp_el1, sp_el1);
  read_sysreg(vcpu->sys.ttbr0_el1, ttbr0_el1);
  read_sysreg(vcpu->sys.ttbr1_el1, ttbr1_el1);
  read_sysreg(vcpu->sys.tcr_el1, tcr_el1);
  read_sysreg(vcpu->sys.vbar_el1, vbar_el1);
  read_sysreg(vcpu->sys.sctlr_el1, sctlr_el1);
  read_sysreg(vcpu->sys.cntv_ctl_el0, cntv_ctl_el0);
  read_sysreg(vcpu->sys.cntv_tval_el0, cntv_tval_el0);
}

static void restore_sysreg(struct vcpu *vcpu) {
  write_sysreg(spsr_el1, vcpu->sys.spsr_el1);
  write_sysreg(elr_el1, vcpu->sys.elr_el1);
  write_sysreg(vmpidr_el2, vcpu->sys.mpidr_el1);
  write_sysreg(vpidr_el2, vcpu->sys.midr_el1);
  write_sysreg(sp_el0, vcpu->sys.sp_el0);
  write_sysreg(sp_el1, vcpu->sys.sp_el1);
  write_sysreg(ttbr0_el1, vcpu->sys.ttbr0_el1);
  write_sysreg(ttbr1_el1, vcpu->sys.ttbr1_el1);
  write_sysreg(tcr_el1, vcpu->sys.tcr_el1);
  write_sysreg(vbar_el1, vcpu->sys.vbar_el1);
  write_sysreg(sctlr_el1, vcpu->sys.sctlr_el1);
  write_sysreg(cntv_ctl_el0, vcpu->sys.cntv_ctl_el0);
  write_sysreg(cntv_tval_el0, vcpu->sys.cntv_tval_el0);
  write_sysreg(cntfrq_el0, vcpu->sys.cntfrq_el0);
}

void vcpu_dump(struct vcpu *vcpu) {
  if(!vcpu)
    return;

  save_sysreg(vcpu);

  vmm_log("vcpu register dump %p\n", vcpu);
  for(int i = 0; i < 31; i++) {
    printf("x%-2d %18p  ", i, vcpu->reg.x[i]);
    if((i+1) % 4 == 0)
      printf("\n");
  }
  printf("\n");
  printf("spsr_el2  %18p  elr_el2   %18p\n", vcpu->reg.spsr, vcpu->reg.elr);
  printf("spsr_el1  %18p  elr_el1   %18p  mpdir_el1    %18p\n",
         vcpu->sys.spsr_el1, vcpu->sys.elr_el1, vcpu->sys.mpidr_el1);
  printf("midr_el1  %18p  sp_el0    %18p  sp_el1       %18p\n",
         vcpu->sys.midr_el1, vcpu->sys.sp_el0, vcpu->sys.sp_el1);
  printf("ttbr0_el1 %18p  ttbr1_el1 %18p  tcr_el1      %18p\n",
         vcpu->sys.ttbr0_el1, vcpu->sys.ttbr1_el1, vcpu->sys.tcr_el1);
  printf("vbar_el1  %18p  sctlr_el1 %18p  cntv_ctl_el0 %18p\n",
         vcpu->sys.vbar_el1, vcpu->sys.sctlr_el1, vcpu->sys.cntv_ctl_el0);
}
