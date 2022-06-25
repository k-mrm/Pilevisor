#include "mmio.h"
#include "virtio-mmio.h"
#include "log.h"
#include "mm.h"
#include "memmap.h"
#include "vm.h"
#include "kalloc.h"
#include "spinlock.h"

#define R(r) ((volatile u32 *)(VIRTIO0 + (r)))

static struct virtio_mmio_dev vtdev = {0};

__attribute__((aligned(PAGESIZE)))
static char virtqueue[PAGESIZE*2] = {0};
static spinlock_t vq_lock;

static int virtq_read(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio) {
  u32 desc_size = sizeof(struct virtq_desc) * vtdev.qnum;

  if(offset >= desc_size)
    goto passthrough;

passthrough:
  // acquire(&vq_lock);
  switch(mmio->accsize) {
    case ACC_BYTE:        *val = *(u8 *)(&virtqueue[offset]); break;
    case ACC_HALFWORD:    *val = *(u16 *)(&virtqueue[offset]); break;
    case ACC_WORD:        *val = *(u32 *)(&virtqueue[offset]); break;
    case ACC_DOUBLEWORD:  *val = *(u64 *)(&virtqueue[offset]); break;
    default: panic("?");
  }
  // release(&vq_lock);

  return 0;
}

static int virtq_write(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio) {
  u32 desc_size = sizeof(struct virtq_desc) * vtdev.qnum;

  if(offset >= desc_size)
    goto passthrough;

  u64 descoff = offset % sizeof(struct virtq_desc); 
  u64 descn = offset / sizeof(struct virtq_desc); 

  switch(descoff) {
    case offsetof(struct virtq_desc, addr):
      acquire(&vtdev.lock);

      if(val) {
        // vmm_log("%d miiiiiii addr %p %p %p\n", vcpu->cpuid, val, ipa2pa(vcpu->vm->vttbr, val), mmio->accsize);
        vtdev.ring[descn].ipa = val;
        val = ipa2pa(vcpu->vm->vttbr, val);
        vtdev.ring[descn].real_addr = val;
      } else {
        vtdev.ring[descn].ipa = 0;
        vtdev.ring[descn].real_addr = 0;
      }

      release(&vtdev.lock);
      break;
    case offsetof(struct virtq_desc, len): {
      acquire(&vtdev.lock);

      u32 len = (u32)val;
      vtdev.ring[descn].len = len;

      u64 daddr = vtdev.ring[descn].real_addr;
      u64 iaddr = vtdev.ring[descn].ipa;
      /* check acrossing pages */
      if(((daddr+len)>>12) > (daddr>>12)) {
        char *real = kalloc();
        copy_from_guest(vcpu->vm->vttbr, real, vtdev.ring[descn].ipa, len);
        vtdev.ring[descn].real_addr = (u64)real;
        vtdev.ring[descn].across_page = true;

        /* rewrite virtqueue */
        u64 daddr_offset =
          descn * sizeof(struct virtq_desc) + offsetof(struct virtq_desc, addr);
        // acquire(&vq_lock);
        *(u64 *)(&virtqueue[daddr_offset]) = (u64)real;
        // release(&vq_lock);
      }

      release(&vtdev.lock);
      break;
    }
    case offsetof(struct virtq_desc, flags): {
      u16 flags = (u16)val;
      acquire(&vtdev.lock);
      vtdev.ring[descn].has_next = !!(flags & VRING_DESC_F_NEXT);
      release(&vtdev.lock);
      break;
    }
    case offsetof(struct virtq_desc, next): {
      acquire(&vtdev.lock);
      u16 next = (u16)val;
      vtdev.ring[descn].next = next;
      release(&vtdev.lock);
      break;
    }
  }

  // vmm_log("%d virtqwrite descn %p %d %d val %p %p %d\n", vcpu->cpuid, offset, descoff, descn, val, mmio->pc, mmio->accsize*8);
passthrough:
  // acquire(&vq_lock);
  switch(mmio->accsize) {
    case ACC_BYTE:        *(u8 *)(&virtqueue[offset]) = val; break;
    case ACC_HALFWORD:    *(u16 *)(&virtqueue[offset]) = val; break;
    case ACC_WORD:        *(u32 *)(&virtqueue[offset]) = val; break;
    case ACC_DOUBLEWORD:  *(u64 *)(&virtqueue[offset]) = val; break;
    default: panic("?");
  }
  // release(&vq_lock);

  return 0;
}

/* legacy */
static int virtio_mmio_read(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio) {
  volatile void *ipa = (void *)mmio->ipa;

  switch(mmio->accsize) {
    case ACC_BYTE:        *val = *(u8 *)ipa; break;
    case ACC_HALFWORD:    *val = *(u16 *)ipa; break;
    case ACC_WORD:        *val = *(u32 *)ipa; break;
    case ACC_DOUBLEWORD:  *val = *(u64 *)ipa; break;
    default: panic("?");
  }

  return 0;
}

/* legacy */
static int virtio_mmio_write(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio) {
  volatile void *ipa = (void *)mmio->ipa;

  switch(offset) {
    case VIRTIO_MMIO_QUEUE_NUM:
      acquire(&vtdev.lock);
      vtdev.qnum = val;
      release(&vtdev.lock);
      vmm_log("queuenum %d\n", val);
      break;
    case VIRTIO_MMIO_GUEST_PAGE_SIZE:
      if(val != PAGESIZE)
        panic("unsupported");
      vmm_log("guest pagesize: %d\n", val);
      break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:
      // vmm_log("%d queue notify val: %d\n", vcpu->cpuid, val);
      break;
    case VIRTIO_MMIO_QUEUE_PFN: {
      u64 pfn_ipa = val << 12;
      pagetrap(vcpu->vm, pfn_ipa, 0x2000, virtq_read, virtq_write);
      val = (u64)virtqueue >> 12;
      vmm_log("queuepfn %p -> %p(%p)\n", pfn_ipa, virtqueue, val);
      break;
    }
    case VIRTIO_MMIO_INTERRUPT_ACK:
      /* dummy */
      return 0;
  }

  switch(mmio->accsize) {
    case ACC_BYTE:        *(u8 *)ipa = val; break;
    case ACC_HALFWORD:    *(u16 *)ipa = val; break;
    case ACC_WORD:        *(u32 *)ipa = val; break;
    case ACC_DOUBLEWORD:  *(u64 *)ipa = val; break;
    default: panic("?");
  }

  return 0;
}

void virtio_dev_intr(struct vcpu *vcpu) {
  struct virtq_used *used = (struct virtq_used *)(virtqueue + PAGESIZE);

  acquire(&vtdev.lock);

  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  while(vtdev.last_used_idx != used->idx) {
    __sync_synchronize();
    int id = used->ring[vtdev.last_used_idx % NUM].id;
    for(;;) {
      struct vtdev_desc *d = &vtdev.ring[id];
      if(d->across_page) {
        copy_to_guest(vcpu->vm->vttbr, d->ipa, (char *)d->real_addr, d->len);

        kfree((char *)d->real_addr);
        d->across_page = false;
      }

      if(!d->has_next)
        break;
      id = d->next;
    }

    vtdev.last_used_idx++;
  }

  release(&vtdev.lock);
}

void virtio_mmio_init(struct vm *vm) {
  pagetrap(vm, VIRTIO0, 0x10000, virtio_mmio_read, virtio_mmio_write);

  spinlock_init(&vtdev.lock);
  spinlock_init(&vq_lock);
}
