#include "aarch64.h"
#include "vcpu.h"
#include "printf.h"
#include "lib.h"
#include "pcpu.h"
#include "log.h"
#include "mm.h"
#include "spinlock.h"
#include "param.h"
#include "localnode.h"
#include "node.h"
#include "panic.h"
#include "tlb.h"
#include "arch-timer.h"

/*
static void vcpu_features_init(struct vcpu *vcpu) {
  u64 pfr0 = read_sysreg(ID_PFR0_EL1);

  vmm_log("pfr0 %p\n", pfr0);
  // test: Disable EL2
  pfr0 &= ~(0xf << 8); 

  vcpu->features.pfr0 = pfr0;
}
*/

static inline u64 vcpuid_to_vaff(int vcpuid) {
  /* TODO */
  return vcpuid;
}
 
void vcpuid_init(u32 *vcpuids, int nvcpu) {
  for(int i = 0; i < nvcpu; i++) {
    struct vcpu *vcpu = &localvm.vcpus[i];
    vcpu->vcpuid = vcpuids[i];
    vcpu->vmpidr = vcpuid_to_vaff(vcpu->vcpuid);

    if(vcpu->vcpuid == nr_cluster_vcpus - 1)    // last vcpu
      vcpu->last = true;
    else
      vcpu->last = false;

    vmm_log("vcpuid is %d(%d/%d)\n", vcpu->vmpidr, i, nvcpu);
  }
}

void trapret(void);

void vcpu_entry() {
  vmm_log("entering vcpu%d\n", current->vmpidr);

  if(!current->initialized)
    panic("maybe current vcpu uninitalized");

  write_sysreg(vmpidr_el2, current->vmpidr);
  
  u64 vpidr = read_sysreg(midr_el1);
  write_sysreg(vpidr_el2, vpidr);

  printf("vmm_boot_clk: %d\n", current->vmm_boot_clk);
  write_sysreg(cntvoff_el2, current->vmm_boot_clk);

  write_sysreg(vtcr_el2, localvm.vtcr);

  write_sysreg(vttbr_el2, localvm.vttbr);
  tlb_s2_flush_all();
  dsb(sy);

  write_sysreg(sctlr_el1, current->sctlr_el1);

  isb();

  vcpu_dump(current);

  /* vmentry */
  trapret();
}

void vcpu_init_core() {
  struct vcpu *vcpu = node_vcpu_by_localid(cpuid());

  set_current_vcpu(vcpu);

  /* current = vcpu */

  vgic_cpu_init(vcpu);

  vcpu->reg.spsr = PSR_EL1H;     /* EL1h */
  vcpu->sctlr_el1 = 0xc50838;

  vcpu->initialized = true;
}

void vcpu_shutdown(struct vcpu *vcpu) {
  ;
}

void wait_for_current_vcpu_online() {
  vmm_log("cpu%d: current online: %d\n", cpuid(), current->online);

  while(!current->online)
    wfi();
}

void vcpu_preinit() {
  for(struct vcpu *v = localvm.vcpus; v < &localvm.vcpus[VCPU_PER_NODE_MAX]; v++) {
    spinlock_init(&v->lock);
    spinlock_init(&v->pending.lock);
  }
}

void vcpu_dump(struct vcpu *vcpu) {
  if(!vcpu) {
    printf("null vcpu\n");
    return;
  }

  printf("vcpu dump %p id: %d\n", vcpu, vcpu->vmpidr);
  printf("vcpu status: %s %s\n", vcpu->initialized ? "initialized" : "uninitialized", 
                                 vcpu->online ? "online" : "offline");

  for(int i = 0; i < 31; i++) {
    printf("x%-2d %18p  ", i, vcpu->reg.x[i]);
    if((i+1) % 4 == 0)
      printf("\n");
  }

  printf("\n");

  printf("spsr_el2  %18p  elr_el2   %18p\n", vcpu->reg.spsr, vcpu->reg.elr);
  printf("spsr_el1  %18p  elr_el1   %18p  mpdir_el1    %18p\n",
         read_sysreg(spsr_el1), read_sysreg(elr_el1), vcpu->vmpidr);

  u64 esr = read_sysreg(esr_el1);

  printf("esr_el1   %18p  (EC %p ISS %p)\n", esr, (esr >> 26) & 0x3f, esr & 0xffffff);
  printf("ttbr0_el1 %18p  ttbr1_el1 %18p  tcr_el1      %18p\n",
         read_sysreg(ttbr0_el1), read_sysreg(ttbr1_el1), read_sysreg(tcr_el1));
  printf("vbar_el1  %18p  sctlr_el1 %18p  cntv_ctl_el0 %18p\n",
         read_sysreg(vbar_el1), read_sysreg(sctlr_el1), read_sysreg(cntv_ctl_el0));
}
