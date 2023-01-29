#include "types.h"
#include "memory.h"
#include "printf.h"
#include "mm.h"

struct system_memory system_memory = {0};
static struct memblock reserved[NBLOCK_MAX] = {0};
static int nreserved = 0;

void system_memory_dump() {
  struct memblock *mem;
  int nslot = system_memory.nslot;

  printf("physical memory slots:\n");
  for(mem = system_memory.slots; mem < &system_memory.slots[nslot]; mem++) {
    u64 start = mem->phys_start;
    u64 size = mem->size;

    printf("\t[%p - %p) (area: %d MB)\n", start, start + size, size >> 20);
  }

  printf("reserved area:\n");
  for(mem = reserved; mem < &reserved[nreserved]; mem++) {
    u64 start = mem->phys_start;
    u64 size = mem->size;

    printf("\t%s: [%p - %p) (area: %d MB)\n", mem->name, start, start + size, size >> 20);
  }
}

void system_memory_reserve(u64 start, u64 end, const char *name) {
  struct memblock *mem = &reserved[nreserved++];

  start = ALIGN_DOWN(start, SZ_2MiB);
  end = ALIGN_UP(end, SZ_2MiB);

  mem->phys_start = start;
  mem->size = end - start;
  mem->name = name;
}

bool is_reserved(u64 p) {
  struct memblock *mem;

  for(mem = reserved; mem < &reserved[nreserved]; mem++) {
    u64 start = mem->phys_start;
    u64 end = start + mem->size;

    if(start <= p && p < end)
      return true;
  }

  return false;
}
