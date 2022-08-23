#include "pci.h"
#include "aarch64.h"
#include "log.h"
#include "kalloc.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "virtio-net.h"
#include "net.h"
#include "node.h"

struct virtio_net vtnet_dev;

static void virtio_net_get_mac(struct virtio_net *dev, u8 *mac) {
  memcpy(mac, dev->cfg->mac, sizeof(u8)*6);
}

void virtio_net_xmit(struct nic *nic, u8 *buf, u64 size) {
  struct virtio_net *dev = nic->device;

  u16 d0 = virtq_alloc_desc(dev->tx);

  struct virtio_net_hdr *hdr = kalloc();

  hdr->flags = 0;
  hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->hdr_len = 0;
  hdr->gso_size = 0;
  hdr->csum_start = 0;
  hdr->csum_offset = 0;
  hdr->num_buffers = 1;

  u8 *packet = (u8 *)hdr + sizeof(struct virtio_net_hdr);
  // printf("ppppppppppppppppp %p", packet);
  memcpy(packet, buf, size);

  dev->tx->desc[d0].len = sizeof(struct virtio_net_hdr) + size;
  dev->tx->desc[d0].addr = (u64)hdr;
  dev->tx->desc[d0].flags = 0;

  dev->tx->avail->ring[dev->tx->avail->idx % NQUEUE] = d0;
  dsb(sy);
  dev->tx->avail->idx += 1;
  dsb(sy);

  vtmmio_write(dev->base, VIRTIO_MMIO_QUEUE_NOTIFY, dev->tx->qsel);
}

static void fill_recv_queue(struct virtq *rxq) {
  for(int i = 0; i < NQUEUE; i++) {
    u16 d = virtq_alloc_desc(rxq);
    rxq->desc[d].addr = (u64)kalloc();
    rxq->desc[d].len = 1564;    /* TODO: ??? */
    rxq->desc[d].flags = VIRTQ_DESC_F_WRITE;
    rxq->avail->ring[rxq->avail->idx] = d;
    dsb(sy);
    rxq->avail->idx += 1;
  }
}

static void rxintr(struct virtio_net *dev, u16 idx) {
  u16 d = dev->rx->used->ring[idx].id;
  u8 *buf = (u8 *)dev->rx->desc[d].addr + sizeof(struct virtio_net_hdr);
  u32 len = dev->rx->used->ring[idx].len;

  // printf("rxintr %p %d %d\n", buf, idx, len);

  msg_recv_intr(buf);

  dev->rx->avail->ring[dev->rx->avail->idx % NQUEUE] = d;
  dsb(sy);
  dev->rx->avail->idx += 1;
}

static void txintr(struct virtio_net *dev, u16 idx) {
  u16 d = dev->tx->avail->ring[idx];

  u8 *buf = dev->tx->desc[d].addr;

  kfree((void*)dev->tx->desc[d].addr);

  virtq_free_desc(dev->tx, d);
}

void virtio_net_intr() {
  struct virtio_net *dev = localnode.nic->device;

  if(!dev)
    panic("uninit vtnet dev");

  u32 status = vtmmio_read(dev->base, VIRTIO_MMIO_INTERRUPT_STATUS);
  vtmmio_write(dev->base, VIRTIO_MMIO_INTERRUPT_ACK, status);

  while(dev->rx->last_used_idx != dev->rx->used->idx) {
    rxintr(dev, dev->rx->last_used_idx % NQUEUE);
    dev->rx->last_used_idx++;
    dsb(sy);
  }

  while(dev->tx->last_used_idx != dev->tx->used->idx) {
    txintr(dev, dev->tx->last_used_idx % NQUEUE);
    dev->tx->last_used_idx++;
    dsb(sy);
  }
}

static struct nic netdev = {
  .xmit = virtio_net_xmit,
};

int virtio_net_init(void *base, int intid) {
  struct nic *nic = &netdev;

  vmm_log("virtio_net_init\n");

  vtnet_dev.base = base;
  vtnet_dev.cfg = (struct virtio_net_config *)(base + VIRTIO_MMIO_CONFIG);
  vtnet_dev.intid = intid;

  vtmmio_write(base, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
  vtmmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);

  /* negotiate */
  u32 feat = vtmmio_read(base, VIRTIO_MMIO_DEVICE_FEATURES);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_CSUM);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_GUEST_OFFLOADS);
  feat &= ~(1 << VIRTIO_NET_F_MTU);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_TSO4);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_TSO6);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_ECN);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_UFO);
  feat &= ~(1 << VIRTIO_NET_F_HOST_TSO4);
  feat &= ~(1 << VIRTIO_NET_F_HOST_TSO6);
  feat &= ~(1 << VIRTIO_NET_F_HOST_ECN);
  feat &= ~(1 << VIRTIO_NET_F_HOST_UFO);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_VQ);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_RX);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_VLAN);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_RX_EXTRA);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_ANNOUNCE);
  feat &= ~(1 << VIRTIO_NET_F_MQ);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_MAC_ADDR);
  vtmmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, feat & 0x1ffff);

  u32 status = vtmmio_read(base, VIRTIO_MMIO_STATUS);
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status | DEV_STATUS_FEATURES_OK);

  if(!(vtmmio_read(base, VIRTIO_MMIO_STATUS) & DEV_STATUS_FEATURES_OK)) {
    vmm_warn("features");
    return -1;
  }

  vtnet_dev.tx = virtq_create();
  vtnet_dev.rx = virtq_create();

  virtq_reg_to_dev(base, vtnet_dev.rx, 0);
  virtq_reg_to_dev(base, vtnet_dev.tx, 1);

  fill_recv_queue(vtnet_dev.rx);

  /* initialize done */
  status = vtmmio_read(base, VIRTIO_MMIO_STATUS);
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status | DEV_STATUS_DRIVER_OK);

  printf("virtio-net ready %p\n", status);

  virtio_net_get_mac(&vtnet_dev, nic->mac);
  nic->device = &vtnet_dev;
  nic->irq = intid;
  nic->name = "virtio-net";

  localnode.nic = &netdev;

  return 0;
}
