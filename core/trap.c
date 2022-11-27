/*
 *  aarch64 trap handler
 */

#include "types.h"
#include "aarch64.h"
#include "param.h"
#include "printf.h"
#include "vcpu.h"
#include "log.h"
#include "mm.h"
#include "vmmio.h"
#include "vpsci.h"
#include "node.h"
#include "emul.h"
#include "vsysreg.h"
#include "compiler.h"
#include "panic.h"

static void dabort_iss_dump(u64 iss);
static void iabort_iss_dump(u64 iss);

struct hyp_context {
  u64 x[31];
  u64 spsr;
  u64 elr;
} __packed;

void hyp_sync_handler(struct hyp_context *ctx) {
  u64 esr = read_sysreg(esr_el2);
  u64 elr = read_sysreg(elr_el2);
  u64 far = read_sysreg(far_el2);
  u64 ec = (esr >> 26) & 0x3f;
  u64 iss = esr & 0x1ffffff;

  printf("ERROR: prohibited sync exception\n");
  printf("ec %p iss %p elr %p far %p\n", ec, iss, elr, far);

  printf("hypervisor context (%p):\n", ctx);
  printf("x0  %18p x1  %18p x2  %18p x3  %18p\n", ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);
  printf("x4  %18p x5  %18p x6  %18p x7  %18p\n", ctx->x[4], ctx->x[5], ctx->x[6], ctx->x[7]);
  printf("x8  %18p x9  %18p x10 %18p x11 %18p\n", ctx->x[8], ctx->x[9], ctx->x[10], ctx->x[11]);
  printf("x12 %18p x13 %18p x14 %18p x15 %18p\n", ctx->x[12], ctx->x[13], ctx->x[14], ctx->x[15]);
  printf("x16 %18p x17 %18p x18 %18p x19 %18p\n", ctx->x[16], ctx->x[17], ctx->x[18], ctx->x[19]);
  printf("x20 %18p x21 %18p x22 %18p x23 %18p\n", ctx->x[20], ctx->x[21], ctx->x[22], ctx->x[23]);
  printf("x24 %18p x25 %18p x26 %18p x27 %18p\n", ctx->x[24], ctx->x[25], ctx->x[26], ctx->x[27]);
  printf("x28 %18p x29 %18p x30 %18p\n", ctx->x[28], ctx->x[29], ctx->x[30]);
  printf("spsr  %18p  elr  %18p\n", ctx->spsr, ctx->elr);

  panic("sync el2");
}

void hyp_serror_handler(struct hyp_context *ctx) {
  printf("ERROR: prohibited SError exception\n");

  printf("hypervisor context (%p):\n", ctx);
  printf("x0  %18p x1  %18p x2  %18p x3  %18p\n", ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);
  printf("x4  %18p x5  %18p x6  %18p x7  %18p\n", ctx->x[4], ctx->x[5], ctx->x[6], ctx->x[7]);
  printf("x8  %18p x9  %18p x10 %18p x11 %18p\n", ctx->x[8], ctx->x[9], ctx->x[10], ctx->x[11]);
  printf("x12 %18p x13 %18p x14 %18p x15 %18p\n", ctx->x[12], ctx->x[13], ctx->x[14], ctx->x[15]);
  printf("x16 %18p x17 %18p x18 %18p x19 %18p\n", ctx->x[16], ctx->x[17], ctx->x[18], ctx->x[19]);
  printf("x20 %18p x21 %18p x22 %18p x23 %18p\n", ctx->x[20], ctx->x[21], ctx->x[22], ctx->x[23]);
  printf("x24 %18p x25 %18p x26 %18p x27 %18p\n", ctx->x[24], ctx->x[25], ctx->x[26], ctx->x[27]);
  printf("x28 %18p x29 %18p x30 %18p\n", ctx->x[28], ctx->x[29], ctx->x[30]);
  printf("spsr  %18p  elr  %18p\n", ctx->spsr, ctx->elr);

  panic("serror el2");
}

void fiq_handler() {
  panic("fiq");
}

static int vm_iabort(struct vcpu *vcpu, u64 iss, u64 far) {
  bool fnv = (iss >> 10) & 0x1;
  bool s1ptw = (iss >> 7) & 0x1;

  if(fnv)
    panic("fnv");

  u64 faultpage = faulting_ipa_page();

  if(vcpu->reg.elr == 0)
    panic("? %p %p %p", faultpage, far, vcpu->reg.elr);

  if(s1ptw) {
    /* fetch pagetable */
    vmm_log("\tiabort fetch pagetable ipa %p %p\n", faultpage, vcpu->reg.elr);
  }

  if(!vsm_read_fetch_page(faultpage))
    panic("no page %p %p %p", faultpage, far, vcpu->reg.elr);

  return 0;
}

static int vm_dabort(struct vcpu *vcpu, u64 iss, u64 far) {
  int isv = (iss >> 24) & 0x1;
  int sas = (iss >> 22) & 0x3;
  int r = (iss >> 16) & 0x1f;
  int ar = (iss >> 14) & 0x1;
  int fnv = (iss >> 10) & 0x1;
  bool s1ptw = (iss >> 7) & 0x1;
  bool wnr = (iss >> 6) & 0x1;

  if(fnv)
    panic("fnv");
  /*
  if(ar)
    vmm_warn("acqrel %p\n", vcpu->reg.elr);
    */
  u64 fipa_page = faulting_ipa_page();

  if(s1ptw) {
    /* fetch pagetable */
    vmm_log("\tdabort fetch pagetable ipa %p %p\n", fipa_page, vcpu->reg.elr);
    vsm_read_fetch_page(fipa_page);

    return 1;
  }

  u64 ipa = fipa_page | (far & (PAGESIZE-1));
  vcpu->dabt.fault_va = far;
  vcpu->dabt.fault_ipa = ipa;
  vcpu->dabt.isv = isv;
  vcpu->dabt.write = wnr;
  vcpu->dabt.reg = r;
  vcpu->dabt.accbyte = 1 << sas;

  void *pa;
  if(wnr)
    pa = vsm_write_fetch_page(fipa_page);
  else
    pa = vsm_read_fetch_page(fipa_page);

  if(pa)
    return 1;

  /*
  u32 op = *(u32 *)at_uva2pa(vcpu->reg.elr);

  int c = cpu_emulate(vcpu, op);

  if(c >= 0)
    return c;
  */

  enum maccsize accsz;
  switch(sas) {
    case 0: accsz = ACC_BYTE; break;
    case 1: accsz = ACC_HALFWORD; break;
    case 2: accsz = ACC_WORD; break;
    case 3: accsz = ACC_DOUBLEWORD; break;
    default: panic("unreachable");
  }

  struct mmio_access mmio = {
    .ipa = ipa,
    .val = r == 31 ? 0 : vcpu->reg.x[r],
    .accsize = accsz,
    .wnr = wnr,
  };

  if(vmmio_emulate(vcpu, &mmio) >= 0) {
    if(!mmio.wnr)   // mmio read
      vcpu->reg.x[r] = mmio.val;
    return 0;
  }

  printf("dabort ipa: %p va: %p elr: %p %s %d %d\n", ipa, far, vcpu->reg.elr, wnr ? "write" : "read", r, accsz);

  return -1;
}

static void vpsci_handler(struct vcpu *vcpu) {
  struct vpsci_argv argv = {
    .funcid = (u32)vcpu->reg.x[0],
    .x1 = vcpu->reg.x[1],
    .x2 = vcpu->reg.x[2],
    .x3 = vcpu->reg.x[3],
  };

  vcpu->reg.x[0] = vpsci_emulate(vcpu, &argv);
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

void vm_sync_handler() {
  local_irq_enable();

  // vmm_log("el0/1 sync!\n");
  u64 esr = read_sysreg(esr_el2);
  u64 elr = read_sysreg(elr_el2);
  u64 far = read_sysreg(far_el2);
  u64 ec = (esr >> 26) & 0x3f;
  u64 iss = esr & 0x1ffffff;

  switch(ec) {
    case 0x1:     /* trap WF* */
      // vmm_log("wf* trapped\n");
      current->reg.elr += 4;
      break;
    case 0x16:    /* trap hvc */
      if(hvc_handler(current, iss) < 0)
        panic("unknown hvc #%d", iss);

      break;
    case 0x17:    /* trap smc */
      if(hvc_handler(current, iss) < 0)
        panic("unknown smc #%d", iss);

      break;
    case 0x18:    /* trap system regsiter */
      if(vsysreg_emulate(current, iss) < 0)
        panic("unknown msr/mrs access %p", iss);

      current->reg.elr += 4;

      break;
    case 0x20:    /* instruction abort */
      if(vm_iabort(current, iss, far) < 0) {
        printf("ec %p iss %p elr %p far %p\n", ec, iss, elr, far);
        iabort_iss_dump(iss);
        panic("iabort");
      }

      break;
    case 0x24: {  /* trap EL0/1 data abort */
      int redo;
      if((redo = vm_dabort(current, iss, far)) < 0) {
        dabort_iss_dump(iss);
        panic("unexcepted dabort");
      }

      if(!redo)
        current->reg.elr += 4;

      break;
    }
    default:
      vmm_log("ec %p iss %p elr %p far %p\n", ec, iss, elr, far);
      panic("unknown sync");
  }
}
