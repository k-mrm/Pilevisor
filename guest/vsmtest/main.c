typedef unsigned long u64;
typedef long i64;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned char u8;
typedef signed char i8;

#define NULL ((void *)0)

typedef _Bool bool;

#define true 1
#define false 0

#define intr_enable() \
  asm volatile("msr daifclr, #0x2" ::: "memory");

#define PSCI_SYSTEM_OFF   0x84000008
#define PSCI_SYSTEM_RESET   0x84000009
#define PSCI_SYSTEM_CPUON   0xc4000003

__attribute__((aligned(16))) char _stack[4096*2];

void psci_call(u32 fn, u64 cpuid, u64 ep);

int main(void) {
  psci_call(PSCI_SYSTEM_CPUON, 1, 0x40000000);

  volatile u32 *p = (volatile u32 *)(0x40000000+256*1024);
  volatile u32 c;

  for(int i = 0; i < 10; i++)
    c = p[i];
  
  for(;;)
    ;
}
