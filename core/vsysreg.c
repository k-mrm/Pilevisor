/*
 *  aarch64 system register emulation
 */

#include "aarch64.h"
#include "types.h"
#include "vcpu.h"
#include "vsysreg.h"

#define ISS_SYSREG(op0, op1, crn, crm, op2) \
  ((op0 << 20) | (op2 << 17) | (op1 << 14) | (crn << 10) | (crm << 1))

#define ISS_ID_PFR0_EL1       ISS_SYSREG(3,0,0,1,0)
#define ISS_ID_PFR1_EL1       ISS_SYSREG(3,0,0,1,1)
#define ISS_ID_PFR2_EL1       ISS_SYSREG(3,0,0,3,4)
#define ISS_ID_DFR0_EL1       ISS_SYSREG(3,0,0,1,2)
#define ISS_ID_DFR1_EL1       ISS_SYSREG(3,0,0,3,5)
#define ISS_ID_AFR0_EL1       ISS_SYSREG(3,0,0,1,3)
#define ISS_ID_MMFR0_EL1      ISS_SYSREG(3,0,0,1,4)
#define ISS_ID_MMFR1_EL1      ISS_SYSREG(3,0,0,1,5)
#define ISS_ID_MMFR2_EL1      ISS_SYSREG(3,0,0,1,6)
#define ISS_ID_MMFR3_EL1      ISS_SYSREG(3,0,0,1,7)
#define ISS_ID_MMFR4_EL1      ISS_SYSREG(3,0,0,2,6)
#define ISS_ID_MMFR5_EL1      ISS_SYSREG(3,0,0,3,6)
#define ISS_ID_ISAR0_EL1      ISS_SYSREG(3,0,0,2,0)
#define ISS_ID_ISAR1_EL1      ISS_SYSREG(3,0,0,2,1)
#define ISS_ID_ISAR2_EL1      ISS_SYSREG(3,0,0,2,2)
#define ISS_ID_ISAR3_EL1      ISS_SYSREG(3,0,0,2,3)
#define ISS_ID_ISAR4_EL1      ISS_SYSREG(3,0,0,2,4)
#define ISS_ID_ISAR5_EL1      ISS_SYSREG(3,0,0,2,5)
#define ISS_ID_ISAR6_EL1      ISS_SYSREG(3,0,0,2,7)
#define ISS_MVFR0_EL1         ISS_SYSREG(3,0,0,3,0)
#define ISS_MVFR1_EL1         ISS_SYSREG(3,0,0,3,1)
#define ISS_MVFR2_EL1         ISS_SYSREG(3,0,0,3,2)

#define ISS_ID_AA64PFR0_EL1   ISS_SYSREG(3,0,0,4,0)
#define ISS_ID_AA64PFR1_EL1   ISS_SYSREG(3,0,0,4,1)
#define ISS_ID_AA64ZFR0_EL1   ISS_SYSREG(3,0,0,4,4)
#define ISS_ID_AA64DFR0_EL1   ISS_SYSREG(3,0,0,5,0)
#define ISS_ID_AA64DFR1_EL1   ISS_SYSREG(3,0,0,5,1)
#define ISS_ID_AA64ISAR0_EL1  ISS_SYSREG(3,0,0,6,0)
#define ISS_ID_AA64ISAR1_EL1  ISS_SYSREG(3,0,0,6,1)
#define ISS_ID_AA64MMFR0_EL1  ISS_SYSREG(3,0,0,7,0)
#define ISS_ID_AA64MMFR1_EL1  ISS_SYSREG(3,0,0,7,1)
#define ISS_ID_AA64MMFR2_EL1  ISS_SYSREG(3,0,0,7,2)

#define ISS_ICC_SGI1R_EL1     ISS_SYSREG(3,0,12,11,5)

/*
 *  https://developer.arm.com/documentation/ddi0601/2020-12/AArch64-Registers/ESR-EL2--Exception-Syndrome-Register--EL2-
 *  #ISS encoding for an exception from MSR, MRS, or System instruction execution in AArch64 state
 */

static void sysreg_iss_dump(u64 iss) {
  int wr = !(iss & 1);
  int crm = (iss>>1) & 0xf;
  int rt = (iss>>5) & 0x1f;
  int crn = (iss>>10) & 0xf;
  int op1 = (iss>>14) & 0x7;
  int op2 = (iss>>17) & 0x7;
  int op0 = (iss>>20) & 0x3;

  printf("%s ", wr ? "msr" : "mrs");
  printf("OP0 %d OP1 %d CRn c%d CRm c%d OP2 %d : Rt %d\n",
          op0, op1, crn, crm, op2, rt);
}

int vsysreg_emulate(struct vcpu *vcpu, u64 iss) {
  int wr = !(iss & 1);
  int rt = (iss >> 5) & 0x1f;

  iss = iss & ~1 & ~(0x1f << 5);

#define handle_sysreg_ro(sreg) __handle_sysreg_ro(sreg)
#define __handle_sysreg_ro(sreg) \
  case ISS_ ## sreg:  \
    if(wr)  \
      return -1;  \
    vcpu->reg.x[rt] = read_sysreg(sreg);  \
    return 0;

  switch(iss) {
    handle_sysreg_ro(ID_PFR1_EL1);
    handle_sysreg_ro(ID_DFR0_EL1);
    handle_sysreg_ro(ID_ISAR0_EL1);
    handle_sysreg_ro(ID_ISAR1_EL1);
    handle_sysreg_ro(ID_ISAR2_EL1);
    handle_sysreg_ro(ID_ISAR3_EL1);
    handle_sysreg_ro(ID_ISAR4_EL1);
    handle_sysreg_ro(ID_ISAR5_EL1);
    handle_sysreg_ro(ID_MMFR0_EL1);
    handle_sysreg_ro(ID_MMFR1_EL1);
    handle_sysreg_ro(ID_MMFR2_EL1);
    handle_sysreg_ro(ID_MMFR3_EL1);
    handle_sysreg_ro(ID_MMFR4_EL1);
    handle_sysreg_ro(MVFR0_EL1);
    handle_sysreg_ro(MVFR1_EL1);
    handle_sysreg_ro(MVFR2_EL1);
    handle_sysreg_ro(ID_AA64PFR0_EL1);
    handle_sysreg_ro(ID_AA64PFR1_EL1);
    handle_sysreg_ro(ID_AA64DFR0_EL1);
    handle_sysreg_ro(ID_AA64DFR1_EL1);
    handle_sysreg_ro(ID_AA64ISAR0_EL1);
    handle_sysreg_ro(ID_AA64ISAR1_EL1);
    handle_sysreg_ro(ID_AA64MMFR0_EL1);
    handle_sysreg_ro(ID_AA64MMFR1_EL1);
    // handle_sysreg_ro(ID_AA64MMFR2_EL1);
    // handle_sysreg_ro(ID_AA64ZFR0_EL1);

    case ISS_ID_PFR0_EL1:
      if(wr)
        return -1;
      vcpu->reg.x[rt] = vcpu->features.pfr0;
      return 0;

    case ISS_ICC_SGI1R_EL1:
      return vgic_emulate_sgi1r(vcpu, rt, wr);

    default:
      vmm_warn("unhandled system register access\n");
      sysreg_iss_dump(iss);
      return -1;
  }
}
