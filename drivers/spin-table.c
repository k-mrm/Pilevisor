#include "types.h"
#include "device.h"
#include "pcpu.h"
#include "param.h"
#include "mm.h"
#include "cache.h"

static u64 release_addr[NCPU_MAX];

static int spintable_boot(struct pcpu *cpu, physaddr_t entrypoint) {
  int id = pcpu_id(cpu);
  u64 addr = release_addr[id];

  volatile void *rel_addr = iomap(addr, sizeof(addr));
  if(!rel_addr)
    return -1;

  printf("spintable boot %d %p %p\n", id, addr, rel_addr);

  *(volatile u64 *)rel_addr = entrypoint;

  dsb(sy);

  dcache_flush_poc_range(rel_addr, sizeof(u64));

  sev();

  return 0;
}

static int spintable_init(struct pcpu *cpu) {
  struct device_node *cdev = cpu->device;
  if(!cdev)
    return -1;

  int id = pcpu_id(cpu);

  int rc = dt_node_propa64(cdev, "cpu-release-addr", &release_addr[id]);
  if(rc < 0)
    return -1;

  printf("cpu%d: release addr %p\n", id, release_addr[id]);

  return 0;
}

const struct cpu_enable_method spin_table = {
  .init = spintable_init,
  .boot = spintable_boot,
};
