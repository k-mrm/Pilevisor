#include "pci.h"
#include "aarch64.h"
#include "log.h"
#include "allocpage.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "virtio-net.h"
#include "net.h"
#include "node.h"
#include "ethernet.h"

struct virtio_net vtnet_dev;

static void virtio_net_get_mac(struct virtio_net *dev, u8 *buf) {
  memcpy(buf, dev->cfg->mac, sizeof(u8)*6);
}

static void virtio_net_xmit(struct nic *nic, u8 *buf, u64 len) {
  struct virtio_net *dev = nic->device;
  struct virtio_net_hdr *hdr = alloc_page();

  hdr->flags = 0;
  hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->hdr_len = 0;
  hdr->gso_size = 0;
  hdr->csum_start = 0;
  hdr->csum_offset = 0;
  hdr->num_buffers = 0;

  u8 *body = alloc_pages(1);
  memcpy(body, buf, len);

  /* hdr */
  bin_dump(hdr, 12);
  u16 d0 = virtq_alloc_desc(dev->tx);

  dev->tx->desc[d0].len = sizeof(struct virtio_net_hdr);
  dev->tx->desc[d0].addr = (u64)hdr;
  dev->tx->desc[d0].flags = VIRTQ_DESC_F_NEXT;

  /* body */
  u16 d1 = virtq_alloc_desc(dev->tx);

  dev->tx->desc[d1].len = len;
  dev->tx->desc[d1].addr = (u64)body;
  dev->tx->desc[d1].flags = 0;

  dev->tx->desc[d0].next = d1;

  printf("d0 %d d1 %d\n", d0, d1);

  dev->tx->avail->ring[dev->tx->avail->idx % NQUEUE] = d0;
  dsb(sy);
  dev->tx->avail->idx += 1;
  dsb(sy);

  vtmmio_write(dev->base, VIRTIO_MMIO_QUEUE_NOTIFY, dev->tx->qsel);
}

static void fill_recv_queue(struct virtq *rxq) {
  static struct virtio_net_hdr hdrbuf[NQUEUE/2];

  for(int i = 0; i < NQUEUE; i += 2) {
    u16 d0 = virtq_alloc_desc(rxq);
    u16 d1 = virtq_alloc_desc(rxq);
    rxq->desc[d0].addr = (u64)&hdrbuf[i/2];
    rxq->desc[d0].len = sizeof(struct virtio_net_hdr);
    rxq->desc[d0].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    rxq->desc[d0].next = d1;
    rxq->desc[d1].addr = (u64)alloc_pages(1);
    rxq->desc[d1].len = 8192;
    rxq->desc[d1].flags = VIRTQ_DESC_F_WRITE;
    rxq->avail->ring[rxq->avail->idx] = d0;
    dsb(sy);
    rxq->avail->idx += 1;
  }
}

static void rxintr(struct nic *nic, u16 idx) {
  struct virtio_net *dev = nic->device;

  u16 d0 = dev->rx->used->ring[idx].id;
  u16 d1 = dev->rx->desc[d0].next;
  u32 len = dev->rx->used->ring[idx].len;

  struct etherframe *eth = (struct etherframe *)dev->rx->desc[d1].addr;
  ethernet_recv_intr(nic, eth, len - sizeof(struct virtio_net_hdr));

  dev->rx->avail->ring[dev->rx->avail->idx % NQUEUE] = d0;
  dsb(sy);
  dev->rx->avail->idx += 1;
}

static void txintr(struct nic *nic, u16 idx) {
  struct virtio_net *dev = nic->device;

  u16 d0 = dev->tx->avail->ring[idx];
  u16 d1 = dev->tx->desc[d0].next;
  struct virtio_net_hdr *h = (struct virtio_net_hdr *)dev->tx->desc[d0].addr;
  u8 *buf = (u8 *)dev->tx->desc[d1].addr;
  bin_dump(buf, 64);
  vmm_log("txintrrrrrrrrrrrrrr\n");

  free_page(h);
  free_pages(buf, 1);

  virtq_free_desc(dev->tx, d0);
  virtq_free_desc(dev->tx, d1);
}

void virtio_net_intr() {
  struct nic *nic = localnode.nic;
  struct virtio_net *dev = nic->device;

  u32 status = vtmmio_read(dev->base, VIRTIO_MMIO_INTERRUPT_STATUS);
  vtmmio_write(dev->base, VIRTIO_MMIO_INTERRUPT_ACK, status);

  while(dev->rx->last_used_idx != dev->rx->used->idx) {
    rxintr(nic, dev->rx->last_used_idx % NQUEUE);
    dev->rx->last_used_idx++;
    dsb(sy);
  }

  while(dev->tx->last_used_idx != dev->tx->used->idx) {
    txintr(nic, dev->tx->last_used_idx % NQUEUE);
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
  // feat &= ~(1 << VIRTIO_NET_F_MTU);
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

  vtnet_dev.mtu = vtnet_dev.cfg->mtu;

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
