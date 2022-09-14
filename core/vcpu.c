#include "aarch64.h"
#include "vcpu.h"
#include "printf.h"
#include "lib.h"
#include "pcpu.h"
#include "log.h"
#include "mm.h"
#include "spinlock.h"
#include "memmap.h"
#include "node.h"

static void save_sysreg(struct vcpu *vcpu);
static void restore_sysreg(struct vcpu *vcpu);

static void vcpu_features_init(struct vcpu *vcpu) {
  u64 pfr0 = read_sysreg(ID_PFR0_EL1);

  vmm_log("pfr0 %p\n", pfr0);
  /* test: Disable EL2 */
  pfr0 &= ~(0xf << 8); 

  vcpu->features.pfr0 = pfr0;
}

struct vcpu *vcpu_get_local(int localcpuid) {
  if(localcpuid >= localnode.nvcpu)
    panic("vcpu_get_local");

  struct vcpu *vcpu = &localnode.vcpus[localcpuid];
  return vcpu;
}

struct vcpu *vcpu_get(int vcpuid) {
  for(struct vcpu *v = localnode.vcpus; v < &localnode.vcpus[VCPU_PER_NODE_MAX]; v++) {
    if(v->vcpuid == vcpuid)
      return v;
  }

  /* vcpu in remote node */
  return NULL;
}

void vcpuid_init(u32 *vcpuids, int nvcpu) {
  for(int i = 0; i < nvcpu; i++) {
    struct vcpu *vcpu = &localnode.vcpus[i];
    vcpu->vcpuid = vcpuids[i];
    vmm_log("vcpuid is %d\n", vcpu->vcpuid);
  }
}

void load_new_local_vcpu(void) {
  int localcpuid = cpuid();
  if(localcpuid >= localnode.nvcpu)
    panic("too many vcpu");

  struct vcpu *vcpu = vcpu_get_local(localcpuid);

  set_current_vcpu(vcpu);

  /* TODO: determine vcpu->name from system register */
  vcpu->name = "cortex-a72";

  vcpu->vgic = new_vgic_cpu(localcpuid);
  gic_init_state(&vcpu->gic);

  vcpu->reg.spsr = 0x3c5;   /* EL1 */

  vcpu->sys.mpidr_el1 = vcpu->vcpuid;       /* TODO: affinity? */
  vcpu->sys.midr_el1 = read_sysreg(midr_el1);
  vcpu->sys.sctlr_el1 = 0xc50838;
  vcpu->sys.cntfrq_el0 = 62500000;

  vcpu_features_init(vcpu);

  vcpu->initialized = true;
}

void trapret(void);

void enter_vcpu() {
  vmm_log("entering vcpu%d\n", current->vcpuid);

  if(current->reg.elr == 0)
    panic("elr maybe uninitalized?");

  restore_sysreg(current);
  gic_restore_state(&current->gic);

  vcpu_dump(current);

  isb();

  /* enter vm */
  trapret();
}

static void save_sysreg(struct vcpu *vcpu) {
  vcpu->sys.spsr_el1 = read_sysreg(spsr_el1);
  vcpu->sys.elr_el1 = read_sysreg(elr_el1);
  vcpu->sys.mpidr_el1 = read_sysreg( mpidr_el1);
  vcpu->sys.midr_el1 = read_sysreg(midr_el1);
  vcpu->sys.sp_el0 = read_sysreg(sp_el0);
  vcpu->sys.sp_el1 = read_sysreg(sp_el1);
  vcpu->sys.ttbr0_el1 = read_sysreg(ttbr0_el1);
  vcpu->sys.ttbr1_el1 = read_sysreg(ttbr1_el1);
  vcpu->sys.tcr_el1 = read_sysreg(tcr_el1);
  vcpu->sys.vbar_el1 = read_sysreg(vbar_el1);
  vcpu->sys.sctlr_el1 = read_sysreg(sctlr_el1);
  vcpu->sys.cntv_ctl_el0 = read_sysreg(cntv_ctl_el0);
  vcpu->sys.cntv_tval_el0 = read_sysreg(cntv_tval_el0);
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

void vcpu_init() {
  ;
}
