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
#include "msg.h"
#include "irq.h"
#include "panic.h"

static struct virtio_net vtnet_dev;

static inline void virtio_net_get_mac(struct virtio_net *dev, u8 *buf) {
  memcpy(buf, dev->cfg->mac, sizeof(u8)*6);
}

static struct virtio_tx_hdr *virtio_tx_hdr_alloc(void *p) {
  struct virtio_tx_hdr *hdr = alloc_page();    // FIXME: malloc(sizeof(*hdr))

  hdr->vh.flags = 0;
  hdr->vh.gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->vh.hdr_len = 0;
  hdr->vh.gso_size = 0;
  hdr->vh.csum_start = 0;
  hdr->vh.csum_offset = 0;

  hdr->packet = p;

  return hdr;
}

static void virtio_net_xmit(struct nic *nic, void **packets, int *lens, int npackets) {
  struct virtio_net *dev = nic->device;
  struct virtio_tx_hdr *hdr;
  u64 flags = 0;

  u8 *body = alloc_pages(1);
  u32 offset = 0;
  for(int i = 0; i < npackets; i++) {
    memcpy(body+offset, packets[i], lens[i]);
    offset += lens[i];
  }

  hdr = virtio_tx_hdr_alloc(body);

  struct qlist qs[2] = {
    { hdr, sizeof(struct virtio_net_hdr) },
    { body, offset },
  };

  spin_lock_irqsave(&dev->tx->lock, flags);

  virtq_enqueue_out(dev->tx, qs, 2, hdr);

  virtq_kick(dev->tx);

  spin_unlock_irqrestore(&dev->tx->lock, flags);
}

static void txintr(struct virtq *txq) {
  struct virtio_tx_hdr *hdr;
  u32 len;

  while((hdr = virtq_dequeue(txq, &len)) != NULL) {
    void *buf = hdr->packet;

    free_page(hdr);
    free_pages(buf, 1);
  }
}

static void fill_recv_queue(struct virtq *rxq) {
  struct virtio_net *dev = rxq->dev->priv;
  struct qlist qs[2];
  u32 hdr_len = sizeof(struct virtio_net_hdr) + ETH_POCV2_MSG_HDR_SIZE;

  while(dev->n_rxbuf < NQUEUE/2) {
    struct receive_buf *buf = alloc_recvbuf(hdr_len);

    qs[0] = (struct qlist){ buf->data, hdr_len };
    qs[1] = (struct qlist){ buf->body, 4096 };

    virtq_enqueue_in(rxq, qs, 2, buf);

    dev->n_rxbuf++;
  }
}

static void rxintr(struct virtq *rxq) {
  struct virtio_net *dev = rxq->dev->priv;
  struct receive_buf *buf;
  u32 len;

  while((buf = virtq_dequeue(rxq, &len)) != NULL) {
    recvbuf_pull(buf, sizeof(struct virtio_net_hdr));
    recvbuf_set_len(buf, len - sizeof(struct virtio_net_hdr));

    dev->n_rxbuf--;

    netdev_recv(buf);

    free_page(buf->head);
    free_recvbuf(buf);
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

  u8 mac[6] = {0};
  virtio_net_get_mac(&vtnet_dev, mac);

  net_init("virtio-net", mac, vtnet_dev.mtu, &vtnet_dev, &virtio_net_ops);

  return 0;
}
