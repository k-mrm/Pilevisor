#include "pci.h"
#include "log.h"
#include "kalloc.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "virtio-net.h"
#include "node.h"

struct virtio_net netdev;

void virtio_net_get_mac(struct virtio_net *nic, u8 *mac) {
  memcpy(mac, nic->cfg->mac, sizeof(u8)*6);
}

void virtio_net_tx(struct virtio_net *nic, u8 *buf, u64 size) {
  u32 d0 = virtq_alloc_desc(&nic->tx);

  struct virtio_net_hdr *hdr = kalloc();

  hdr->flags = 0;
  hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->hdr_len = 0;
  hdr->gso_size = 0;
  hdr->csum_start = 0;
  hdr->csum_offset = 0;
  hdr->num_buffers = 1;

  u8 *packet = (u8 *)hdr + sizeof(struct virtio_net_hdr);
  memcpy(packet, buf, size);

  nic->tx.desc[d0].len = sizeof(struct virtio_net_hdr) + size;
  nic->tx.desc[d0].addr = (u64)hdr;
  nic->tx.desc[d0].flags = 0;

  printf("cccc %d %d %p\n", nic->tx.qsel, nic->tx.desc[d0].len, nic->tx.desc[d0].addr);
  for(int i = 0; i < 90; i++) {
    printf("%2x ", ((u8 *)hdr)[i]);
  }

  nic->tx.avail->ring[nic->tx.avail->idx] = d0;
  dsb(sy);
  nic->tx.avail->idx += 1;
  dsb(sy);
  vtmmio_write(nic->base, VIRTIO_MMIO_QUEUE_NOTIFY, nic->tx.qsel);
}

static void rxintr(struct virtio_net *nic, u16 idx) {
  u32 d = nic->rx.used->ring[idx].id;
  u8 *buf = (u8 *)(nic->rx.desc[idx].addr + sizeof(struct virtio_net_hdr));
}

static void txintr(struct virtio_net *nic, u16 idx) {
  u32 d = nic->tx.avail->ring[idx];

  printf("tx intr!!\n");

  kfree((void*)nic->tx.desc[d].addr);

  virtq_free_desc(&nic->tx, d);
}

void virtio_net_intr() {
  printf("netintr");

  struct node *node = &global;
  struct virtio_net *nic = node->nic;

  u32 status = vtmmio_read(nic->base, VIRTIO_MMIO_INTERRUPT_STATUS);
  vtmmio_write(nic->base, VIRTIO_MMIO_INTERRUPT_ACK, status);

  while(nic->rx.last_used_idx != nic->rx.used->idx) {
    printf("netintr rx");
    rxintr(nic, nic->rx.last_used_idx);
    nic->rx.last_used_idx++;
  }

  while(nic->tx.last_used_idx != nic->tx.used->idx) {
    printf("netintr tx");
    txintr(nic, nic->tx.last_used_idx);
    nic->tx.last_used_idx++;
  }
}

int virtio_net_init(void *base, int intid) {
  vmm_log("virtio_net_init\n");

  netdev.base = base;
  netdev.cfg = (struct virtio_net_config *)(base + VIRTIO_MMIO_CONFIG);
  netdev.intid = intid;

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
  feat &= ~(1 << VIRTIO_NET_F_MRG_RXBUF);
  feat &= ~(1 << VIRTIO_NET_F_STATUS);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_VQ);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_RX);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_VLAN);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_RX_EXTRA);
  feat &= ~(1 << VIRTIO_NET_F_GUEST_ANNOUNCE);
  feat &= ~(1 << VIRTIO_NET_F_MQ);
  feat &= ~(1 << VIRTIO_NET_F_CTRL_MAC_ADDR);
  vtmmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, feat);

  u32 status = vtmmio_read(base, VIRTIO_MMIO_STATUS);
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status | DEV_STATUS_FEATURES_OK);

  if(!(vtmmio_read(base, VIRTIO_MMIO_STATUS) & DEV_STATUS_FEATURES_OK)) {
    vmm_warn("features");
    return -1;
  }

  virtq_init(&netdev.tx);
  virtq_init(&netdev.rx);

  virtq_reg_to_dev(base, &netdev.rx, 0);
  virtq_reg_to_dev(base, &netdev.tx, 1);

  /* initialized */
  status = vtmmio_read(base, VIRTIO_MMIO_STATUS);
  vtmmio_write(base, VIRTIO_MMIO_STATUS, status | DEV_STATUS_DRIVER_OK);

  return 0;
}

void virtio_net_send_test() {
  printf("sendtest\n");
  u8 buf[1000];

  buf[0] = 0xff;
  buf[1] = 0xff;
  buf[2] = 0xff;
  buf[3] = 0xff;
  buf[4] = 0xff;
  buf[5] = 0xff;

  /*
     buf[6] = netdev.regs->Config[0];
     buf[7] = netdev.regs->Config[1];
     buf[8] = netdev.regs->Config[2];
     buf[9] = netdev.regs->Config[3];
     buf[10] = netdev.regs->Config[4];
     buf[11] = netdev.regs->Config[5];
     */
  u8 mac[6];
  virtio_net_get_mac(&netdev, mac);

  buf[6] = mac[0];
  buf[7] = mac[1];
  buf[8] = mac[2];
  buf[9] = mac[3];
  buf[10] = mac[4];
  buf[11] = mac[5];
  buf[12] = 0;
  buf[13] = 0;

  for(int i = 20; i < 0xff; i++){
    buf[i] = 0xff - i;
  }

  virtio_net_tx(&netdev, buf, 80);
}
