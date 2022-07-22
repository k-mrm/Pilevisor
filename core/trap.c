#include "types.h"
#include "aarch64.h"
#include "param.h"
#include "printf.h"
#include "vcpu.h"
#include "log.h"
#include "mm.h"
#include "mmio.h"
#include "vpsci.h"

static void dabort_iss_dump(u64 iss);
static void iabort_iss_dump(u64 iss);

void hyp_sync_handler() {
  u64 esr, elr, far;
  read_sysreg(esr, esr_el2);
  read_sysreg(elr, elr_el2);
  read_sysreg(far, far_el2);
  u64 ec = (esr >> 26) & 0x3f;
  u64 iss = esr & 0x1ffffff;

  vmm_log("ec %p iss %p elr %p far %p\n", ec, iss, elr, far);

  panic("sync el2");
}

void uartintr(void);
void hyp_irq_handler() {
  u32 iar = gic_read_iar();
  u32 irq = iar & 0x3ff;

  switch(irq) {
    case 33:
      uartintr();
      break;
    case 48:
      virtio_net_intr();
      break;
    case 1023:
      vmm_warn("sprious interrupt");
      return;
    default:
      panic("?");
  }

  gic_host_eoi(irq, 1);
}

void virtio_dev_intr(struct vcpu *vcpu);

void vm_irq_handler() {
  struct vcpu *vcpu = cur_vcpu();

  vgic_irq_enter(vcpu);

  u32 iar = gic_read_iar();
  u32 pirq = iar & 0x3ff;
  u32 virq = pirq;

  // vmm_log("cpu%d: vm_irq_handler %d\n", cpuid(), iar);
  
  if(pirq == 48) {
    /* virtio-net interrupt */
    virtio_net_intr();
  }

  gic_guest_eoi(pirq, 1);

  if(vgic_inject_virq(vcpu, pirq, virq, 1) < 0)
    gic_deactive_irq(pirq);

  isb();
}

static u64 faulting_ipa_page() {
  u64 hpfar;
  read_sysreg(hpfar, hpfar_el2);

  u64 ipa_page = (hpfar & HPFAR_FIPA_MASK) << 8;

  return ipa_page;
}

static int vm_iabort_handler(struct vcpu *vcpu, u64 iss, u64 far) {
  bool fnv = (iss >> 10) & 0x1;
  bool s1ptw = (iss >> 7) & 0x1;

  if(fnv)
    panic("fnv");

  if(s1ptw) {
    /* fetch pagetable */
    u64 pgt_ipa = faulting_ipa_page();
    vmm_log("iabort fetch pgt ipa %p %p\n", pgt_ipa, vcpu->reg.elr);
    vsm_fetch_pagetable(vcpu->node, pgt_ipa);

    return 0;
  }

  vcpu->reg.elr += 4;

  return -1;
}

static int vm_dabort_handler(struct vcpu *vcpu, u64 iss, u64 far) {
  int isv = (iss >> 24) & 0x1;
  int sas = (iss >> 22) & 0x3;
  int r = (iss >> 16) & 0x1f;
  int fnv = (iss >> 10) & 0x1;
  bool s1ptw = (iss >> 7) & 0x1;
  bool wnr = (iss >> 6) & 0x1;

  if(isv == 0) {
    u32 *pa = (u32 *)at_uva2pa(vcpu->reg.elr);
    u32 op = *pa;
    cpu_emulate(vcpu, op);
    dabort_iss_dump(iss);
  }

  if(s1ptw) {
    /* fetch pagetable */
    u64 pgt_ipa = faulting_ipa_page();
    vmm_log("dabort fetch pgt ipa %p %p\n", pgt_ipa, vcpu->reg.elr);
    // vcpu_dump(vcpu);
    vsm_fetch_pagetable(vcpu->node, pgt_ipa);

    u64 pa = at_uva2pa(0xffff00000df75dc8);
    u64 ipa = at_uva2ipa(0xffff00000df75dc8);
    printf("pa %p ipa %p\n", pa, ipa);

    if(ipa) {
      u64 rp = ipa2pa(vcpu->node->vsm.dummypgt, ipa);
      printf("rp pppp %p \n", rp);

      for(int i = 0; i < 0x200; i++) {
        printf("%02x", ((char*)rp)[i]);
        printf("%s", (i+1) % 4 == 0? " " : "");
      }
    }

    return 0;
  }

  u64 elr;
  read_sysreg(elr, elr_el2);

  enum maccsize accsz;
  switch(sas) {
    case 0: accsz = ACC_BYTE; break;
    case 1: accsz = ACC_HALFWORD; break;
    case 2: accsz = ACC_WORD; break;
    case 3: accsz = ACC_DOUBLEWORD; break;
    default: panic("?");
  }

  u64 ipa = faulting_ipa_page() | (far & (PAGESIZE-1));

  struct mmio_access mmio = {
    .ipa = ipa,
    .pc = elr,
    .accsize = accsz,
    .wnr = wnr,
  };

  vcpu->reg.elr += 4;

  if(elr == 0xffff800010471ef8)
    panic("dabort ipa: %p va: %p elr: %p %s\n", ipa, far, elr, wnr? "write" : "read");

  if(vsm_access(vcpu, vcpu->node, ipa, r, accsz, wnr) >= 0)
    return 0;
  if(mmio_emulate(vcpu, r, &mmio) >= 0)
    return 0;

  vmm_warn("dabort ipa: %p va: %p elr: %p\n", ipa, far, elr);
  return -1;
}

static void vpsci_handler(struct vcpu *vcpu) {
  struct vpsci vpsci = {
    .funcid = (u32)vcpu->reg.x[0],
    .x1 = vcpu->reg.x[1],
    .x2 = vcpu->reg.x[2],
    .x3 = vcpu->reg.x[3],
  };

  u64 ret = vpsci_emulate(vcpu, &vpsci);
  vmm_log("vpsci return %p\n", ret);

  vcpu->reg.x[0] = ret;
}

static int hvc_handler(struct vcpu *vcpu, int imm) {
  switch(imm) {
    case 0:
      vpsci_handler(vcpu);
      return 0;
    default:
      return -1;
  }
}

static void dabort_iss_dump(u64 iss) {
  printf("ISV  : %d\n", (iss >> 24) & 0x1);
  printf("SAS  : %d\n", (iss >> 22) & 0x3);
  printf("SSE  : %d\n", (iss >> 21) & 0x1);
  printf("SRT  : %d\n", (iss >> 16) & 0x1f);
  printf("SF   : %d\n", (iss >> 15) & 0x1);
  printf("AR   : %d\n", (iss >> 14) & 0x1);
  printf("VNCR : %d\n", (iss >> 13) & 0x1);
  printf("FnV  : %d\n", (iss >> 10) & 0x1);
  printf("EA   : %d\n", (iss >> 9) & 0x1);
  printf("CM   : %d\n", (iss >> 8) & 0x1);
  printf("S1PTW: %d\n", (iss >> 7) & 0x1);
  printf("WnR  : %d\n", (iss >> 6) & 0x1);
  printf("DFSC : %x\n", iss & 0x3f);
}

static void iabort_iss_dump(u64 iss) {
  printf("SET  : %d\n", (iss >> 11) & 0x3);
  printf("FnV  : %d\n", (iss >> 10) & 0x1);
  printf("EA   : %d\n", (iss >> 9) & 0x1);
  printf("S1PTW: %d\n", (iss >> 7) & 0x1);
  printf("IFSC : %x\n", iss & 0x3f);
}

int vsysreg_emulate(struct vcpu *vcpu, u64 iss);
void vm_sync_handler() {
  struct vcpu *vcpu;
  read_sysreg(vcpu, tpidr_el2);

  // vmm_log("el0/1 sync!\n");
  u64 esr, elr, far;
  read_sysreg(esr, esr_el2);
  read_sysreg(elr, elr_el2);
  read_sysreg(far, far_el2);
  u64 ec = (esr >> 26) & 0x3f;
  u64 iss = esr & 0x1ffffff;

  switch(ec) {
    case 0x1:     /* trap WF* */
      // vmm_log("wf* trapped\n");
      vcpu->reg.elr += 4;
      break;
    case 0x16:    /* trap hvc */
      if(hvc_handler(vcpu, iss) < 0)
        panic("unknown hvc #%d", iss);

      break;
    case 0x17:    /* trap smc */
      if(hvc_handler(vcpu, iss) < 0)
        panic("unknown smc #%d", iss);

      break;
    case 0x18:    /* trap system regsiter */
      if(vsysreg_emulate(vcpu, iss) < 0)
        panic("unknown msr/mrs access %p", iss);

      vcpu->reg.elr += 4;

      break;
    case 0x20:    /* instruction abort */
      if(vm_iabort_handler(vcpu, iss, far) < 0) {
        printf("ec %p iss %p elr %p far %p\n", ec, iss, elr, far);
        iabort_iss_dump(iss);
      }

      break;
    case 0x24:    /* trap EL0/1 data abort */
      if(vm_dabort_handler(vcpu, iss, far) < 0) {
        dabort_iss_dump(iss);
        panic("unexcepted dabort");
      }

      break;
    default:
      vmm_log("ec %p iss %p elr %p far %p\n", ec, iss, elr, far);
      panic("unknown sync");
  }
}
