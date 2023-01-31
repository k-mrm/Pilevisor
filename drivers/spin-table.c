#include "types.h"
#include "device.h"
#include "pcpu.h"
#include "param.h"
#include "mm.h"
#include "cache.h"

static u64 release_addr[NCPU_MAX];

static int spintable_boot(int cpu, physaddr_t entrypoint) {
  if(cpu >= NCPU_MAX) {
    vmm_warn("no cpu");
    return -1;
  }

  u64 addr = release_addr[cpu];

  volatile void *rel_addr = iomap(addr, sizeof(addr));
  if(!rel_addr)
    return -1;

  printf("spintable boot %d %p\n", cpu, rel_addr);

  *(volatile u64 *)rel_addr = entrypoint;

  dsb(sy);

  dcache_flush_range((u64)rel_addr, sizeof(u64));

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
