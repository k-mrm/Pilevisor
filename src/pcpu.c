#include "pcpu.h"
#include "aarch64.h"

struct pcpu pcpus[NCPU];

struct pcpu *cur_pcpu() {
  int id = cpuid();
  return &pcpus[id];
}

void pcpu_init() {
  for(int i = 0; i < NCPU; i++) {
    pcpus[i].cpuid = i;
  }
}
