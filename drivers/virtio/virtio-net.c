#include "aarch64.h"
#include "log.h"
#include "allocpage.h"
#include "malloc.h"
#include "lib.h"
#include "virtio.h"
#include "virtio-mmio.h"
#include "virtio-net.h"
#include "net.h"
#include "node.h"
#include "ethernet.h"
#include "msg.h"
#include "irq.h"
#include "panic.h"

static struct virtio_net vtnet_dev;

static inline void virtio_net_get_mac(struct virtio_net *dev, u8 *buf) {
  memcpy(buf, dev->cfg->mac, sizeof(u8)*6);
}

static struct virtio_tx_hdr *virtio_tx_hdr_alloc(void *p) {
  struct virtio_tx_hdr *hdr = malloc(sizeof(*hdr));

  hdr->vh.flags = 0;
  hdr->vh.gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->vh.hdr_len = 0;
  hdr->vh.gso_size = 0;
  hdr->vh.csum_start = 0;
  hdr->vh.csum_offset = 0;

  hdr->packet = p;

  return hdr;
}

static void virtio_net_xmit(struct nic *nic, struct iobuf *iobuf) {
  struct virtio_net *dev = nic->device;
  struct virtio_tx_hdr *hdr;
  u64 flags = 0;
  int np;

  hdr = virtio_tx_hdr_alloc(iobuf);
  
  np = iobuf->body ? 3 : 2;

  struct qlist qs[3] = {
    { hdr, sizeof(struct virtio_net_hdr) },
    { iobuf->data, iobuf->len },
    { iobuf->body, iobuf->body_len },
  };

  spin_lock_irqsave(&dev->tx->lock, flags);

  virtq_enqueue_out(dev->tx, qs, np, hdr);

  virtq_kick(dev->tx);

  spin_unlock_irqrestore(&dev->tx->lock, flags);
}

static void txintr(struct virtq *txq) {
  struct virtio_tx_hdr *hdr;
  u32 len;

  while((hdr = virtq_dequeue(txq, &len)) != NULL) {
    struct iobuf *iobuf = hdr->packet;

    free(hdr);
    if(iobuf->body)
      free_page(iobuf->body);
    free_iobuf(iobuf);
  }
}

static void fill_recv_queue(struct virtq *rxq) {
  struct virtio_net *dev = rxq->dev->priv;
  struct qlist qs[2];
  u32 hdr_len = sizeof(struct virtio_net_hdr) + ETH_POCV2_MSG_HDR_SIZE;

  while(dev->n_rxbuf < NQUEUE/2) {
    struct iobuf *iobuf = alloc_iobuf(hdr_len);
    iobuf->body = alloc_page();
    iobuf->body_len = 4096;

    qs[0] = (struct qlist){ iobuf->data, iobuf->len };
    qs[1] = (struct qlist){ iobuf->body, iobuf->body_len };

    virtq_enqueue_in(rxq, qs, 2, iobuf);

    dev->n_rxbuf++;
  }
}

static void rxintr(struct virtq *rxq) {
  struct virtio_net *dev = rxq->dev->priv;
  struct iobuf *iobuf;
  u32 len;

  while((iobuf = virtq_dequeue(rxq, &len)) != NULL) {
    iobuf->body_len = len - iobuf->len;
    iobuf_pull(iobuf, sizeof(struct virtio_net_hdr));

    dev->n_rxbuf--;

    netdev_recv(iobuf);

    free_iobuf(iobuf);
  }

  fill_recv_queue(rxq);
}

static void virtio_net_set_recv_intr_callback(struct nic *nic, 
                                              void (*cb)(struct nic *, void **, int *, int)) {
  nic->ops->recv_intr_callback = cb;
}

static struct nic_ops virtio_net_ops = {
  .xmit = virtio_net_xmit,
  .set_recv_intr_callback = virtio_net_set_recv_intr_callback,
};

int virtio_net_probe(struct virtio_mmio_dev *dev) {
  vmm_log("virtio_net_probe\n");

  vtnet_dev.dev = dev;
  dev->priv = &vtnet_dev;
  vtnet_dev.cfg = (struct virtio_net_config *)(dev->base + VIRTIO_MMIO_CONFIG);

  vtnet_dev.mtu = vtnet_dev.cfg->mtu;
  vtnet_dev.n_rxbuf = 0;

  /* negotiate */
  u64 features = 0;
  features |= 1 << VIRTIO_NET_F_MAC;
  features |= 1 << VIRTIO_NET_F_STATUS;
  features |= 1 << VIRTIO_NET_F_MTU;

  if(vtmmio_negotiate(dev, features) < 0)
    panic("failed negotiate");

  vtnet_dev.rx = virtq_create(dev, 0, rxintr);
  vtnet_dev.tx = virtq_create(dev, 1, txintr);

  virtq_reg_to_dev(vtnet_dev.rx);
  virtq_reg_to_dev(vtnet_dev.tx);

  // vtnet_dev.tx->avail->flags |= VIRTQ_AVAIL_F_NO_INTERRUPT;

  fill_recv_queue(vtnet_dev.rx);

  /* initialize done */
  if(vtmmio_driver_ok(dev) < 0)
    panic("driver ok");

  vmm_log("virtio-net ready! irq: %d\n", dev->intid);

  u8 mac[6];
  virtio_net_get_mac(&vtnet_dev, mac);

  net_init("virtio-net", mac, vtnet_dev.mtu, &vtnet_dev, &virtio_net_ops);

  return 0;
}
