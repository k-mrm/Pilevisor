#include "types.h"
#include "device.h"
#include "pcpu.h"
#include "param.h"

static u64 release_addr[NCPU_MAX];

static int spintable_boot(int cpu, u64 entrypoint) {
  if(cpu >= NCPU_MAX) {
    vmm_warn("no cpu");
    return -1;
  }

  volatile void *rel_addr = (volatile void *)release_addr[cpu];

  *(volatile u64 *)rel_addr = entrypoint;

  isb();
  dsb(sy);

  // TODO: invalidate dcache point of coherency

  sev();

  return 0;
}

static int spintable_init(int cpu) {
  struct device_node *cdev = get_cpu(cpu)->device;
  if(!cdev)
    return -1;

  int rc = dt_node_propa64(cdev, "cpu-release-addr", &release_addr[cpu]);
  if(rc < 0)
    return -1;

  printf("cpu%d: release addr %p\n", cpu, release_addr[cpu]);

  return 0;
}

const struct cpu_enable_method spin_table = {
  .init = spintable_init,
  .boot = spintable_boot,
};
