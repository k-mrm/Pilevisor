/*
 *  broadcom gigabit ethernet driver
 */

#include "types.h"
#include "log.h"
#include "net.h"
#include "device.h"
#include "malloc.h"
#include "printf.h"
#include "irq.h"
#include "panic.h"
#include "spinlock.h"

#include "bcmgenet.h"

#define GENET_V5        5 // the only supported GENET version

// HW params for GENET_V5
#define TX_QUEUES       4
#define TX_BDS_PER_Q    32 // buffer descriptors per Tx queue
#define RX_QUEUES       0     // not implemented
#define RX_BDS_PER_Q    0  // buffer descriptors per Rx queue
#define HFB_FILTER_CNT  48
#define HFB_FILTER_SIZE 128
#define QTAG_MASK       0x3F
#define HFB_OFFSET      0x8000
#define HFB_REG_OFFSET  0xFC00
#define RDMA_OFFSET     0x2000
#define TDMA_OFFSET     0x4000
#define WORDS_PER_BD    3 // word per buffer descriptor

#define RX_BUF_LENGTH     2048

#define TX_RING_INDEX     1 // using highest TX priority queue

// Ethernet defaults
#define ETH_FCS_LEN       4
#define ETH_ZLEN          60
/// #define ENET_MAX_MTU_SIZE 1536 // with padding
#define ENET_MAX_MTU_SIZE 4536 // with padding // CHANGED

void *bcmgenet_base;

static struct bcmgenet_cb *m_tx_cbs;                             // Tx control blocks
static struct bcmgenet_tx_ring m_tx_rings[GENET_DESC_INDEX + 1]; // Tx rings

static spinlock_t m_tx_lock = SPINLOCK_INIT;

static struct bcmgenet_cb *m_rx_cbs;                             // Rx control blocks
static struct bcmgenet_rx_ring m_rx_rings[GENET_DESC_INDEX + 1]; // Rx rings

static spinlock_t m_rx_lock = SPINLOCK_INIT;

static bool m_crc_fwd_en; // has FCS to be removed?

// PHY status
static int m_phy_id; // probed address of this PHY

static int m_link;   // 1: link is up
static int m_speed;  // 10, 100, 1000
static int m_duplex; // 1: full duplex
static int m_pause;  // 1: pause capability

static int m_old_link;
static int m_old_speed;
static int m_old_duplex;
static int m_old_pause;

static void bcmgenet_xmit(struct nic *nic, struct iobuf *iobuf) {
  ;
}

static struct bcmgenet_cb *get_txcb(struct bcmgenet_tx_ring *ring) {
    struct bcmgenet_cb *tx_cb_ptr = ring->cbs;
    tx_cb_ptr += ring->write_ptr - ring->cb_ptr;

    // Advancing local write pointer
    if (ring->write_ptr == ring->end_ptr)
        ring->write_ptr = ring->cb_ptr;
    else
        ring->write_ptr++;

    return tx_cb_ptr;
}

static u32 tx_reclaim(struct bcmgenet_tx_ring *ring) {
  // Clear status before servicing to reduce spurious interrupts
  if (ring->index == GENET_DESC_INDEX)
    bcmgenet_intrl2_0_writel(UMAC_IRQ_TXDMA_DONE, INTRL2_CPU_CLEAR);
  else
    bcmgenet_intrl2_1_writel((1 << ring->index), INTRL2_CPU_CLEAR);

  // Compute how many buffers are transmitted since last xmit call
  u32 c_index = bcmgenet_tdma_ring_readl(ring->index, TDMA_CONS_INDEX) & DMA_C_INDEX_MASK;
  u32 txbds_ready = (c_index - ring->c_index) & DMA_C_INDEX_MASK;

  // Reclaim transmitted buffers
  u32 txbds_processed = 0;
  while (txbds_processed < txbds_ready) {
    free_tx_cb(&m_tx_cbs[ring->clean_ptr]);
    txbds_processed++;
    if (ring->clean_ptr < ring->end_ptr)
      ring->clean_ptr++;
    else
      ring->clean_ptr = ring->cb_ptr;
  }

  ring->free_bds += txbds_processed;
  ring->c_index = c_index;

  return txbds_processed;
}

static void bcmgenet_enable_dma(uint32_t dma_ctrl) {
    u32 reg = bcmgenet_rdma_readl(DMA_CTRL);
    reg |= dma_ctrl;
    bcmgenet_rdma_writel(reg, DMA_CTRL);

    reg = bcmgenet_tdma_readl(DMA_CTRL);
    reg |= dma_ctrl;
    bcmgenet_tdma_writel(reg, DMA_CTRL);
}

static void bcmgenet_rxintr() {
  ;
}

static void bcmgenet_intr_irq0(void *arg) {
  // Read irq status
  u32 status = bcmgenet_intrl2_0_readl(INTRL2_CPU_STAT) &
               ~bcmgenet_intrl2_0_readl(INTRL2_CPU_MASK_STATUS);

  // clear interrupts
  bcmgenet_intrl2_0_writel(INTRL2_CPU_CLEAR, status);

  if(status & UMAC_IRQ_TXDMA_DONE) {
    spin_lock(&m_tx_lock);

    struct bcmgenet_tx_ring *tx_ring = &m_tx_rings[GENET_DESC_INDEX];

    tx_reclaim(tx_ring);

    spin_unlock(&m_tx_lock);
  }

  if(status & UMAC_IRQ_RXDMA_DONE) {
    // bcmgenet_intrl2_0_writel(INTRL2_CPU_CLEAR, UMAC_IRQ_RXDMA_DONE);
    bcmgenet_rxintr(rcv, &len);
  }
}

static void bcmgenet_intr_irq1(void *arg) {
  // stub
}

static struct nic_ops bcmgenet_ops = {
  .xmit = bcmgenet_xmit,
};

static void umac_reset() {
  // 7358a0/7552a0: bad default in RBUF_FLUSH_CTRL.umac_sw_rst
  bcmgenet_rbuf_ctrl_set(0);
  usleep(10);

  // disable MAC while updating its registers
  bcmgenet_umac_writel(0, UMAC_CMD);

  // issue soft reset with (rg)mii loopback to ensure a stable rxclk
  bcmgenet_umac_writel(CMD_SW_RESET | CMD_LCL_LOOP_EN, UMAC_CMD);
  usleep(2);
  bcmgenet_umac_writel(0, UMAC_CMD);
}

static void umac_reset2() {
  u32 reg = bcmgenet_rbuf_ctrl_get();
  reg |= BIT(1);
  bcmgenet_rbuf_ctrl_set(reg);
  usleep(10);

  reg &= ~BIT(1);
  bcmgenet_rbuf_ctrl_set(reg);
  usleep(10);
}

static void init_umac(void) {
  umac_reset();
  umac_reset2();

  // clear tx/rx counter
  bcmgenet_umac_writel(MIB_RESET_RX | MIB_RESET_TX | MIB_RESET_RUNT, UMAC_MIB_CTRL);
  bcmgenet_umac_writel(0, UMAC_MIB_CTRL);

  bcmgenet_umac_writel(ENET_MAX_MTU_SIZE, UMAC_MAX_FRAME_LEN);

  // init rx registers, enable ip header optimization
  uint32_t reg = bcmgenet_rbuf_readl(RBUF_CTRL);
  reg |= RBUF_ALIGN_2B;
  bcmgenet_rbuf_writel(reg, RBUF_CTRL);

  bcmgenet_rbuf_writel(1, RBUF_TBUF_SIZE_CTRL);

  intr_disable();

  // Enable MDIO interrupts on GENET v3+
  // NOTE: MDIO interrupts do not work
  // bcmgenet_intrl2_0_writel(UMAC_IRQ_MDIO_DONE | UMAC_IRQ_MDIO_ERROR,
  // INTRL1_CPU_MASK_CLEAR);
}

static int bcmgenet_init() {
  u32 reg = bcmgenet_sys_readl(SYS_REV_CTRL); // read GENET HW version
  u8 major = (reg >> 24 & 0x0f);

  if(major == 6)
    major = 5;
  else if(major == 5)
    major = 4;
  else if(major == 0)
    major = 1;

  if(major != GENET_V5) {
    printf("GENET version mismatch, got: %d, configured for: %d", major, GENET_V5);
    return -1;
  }

  assert(((reg >> 16) & 0x0f) == 0); // minor version
  assert((reg & 0xffff) == 0);       // EPHY version

  init_umac();

  reg = bcmgenet_umac_readl(UMAC_CMD); // make sure we reflect the value of CRC_CMD_FWD
  m_crc_fwd_en = !!(reg & CMD_CRC_FWD);

  int ret = set_hw_addr();
  if (ret) {
    printf("Cannot set MAC address (%d)", ret);

    return false;
  }

  uint32_t dma_ctrl = dma_disable(); // disable Rx/Tx DMA and flush Tx queues

  ret = init_dma(); // reinitialize TDMA and RDMA and SW housekeeping
  if (ret) {
    printf("Failed to initialize DMA (%d)", ret);

    return false;
  }

  enable_dma(dma_ctrl); // always enable ring 16 - descriptor ring

  hfb_init();

  // net_init("bcmgenet", mac, mtu, &bcmgenet_dev, &bcmgenet_ops);
  return -1;
}

static int bcmgenet_dt_init(struct device_node *dev) {
  u64 base, size; 
  int intr0, intr1;

  if(dt_node_prop_addr(dev, 0, &base, &size) < 0)
    return -1;

  bcmgenet_dev.base = iomap(base, size);
  if(!bcmgenet_dev.base)
    return -1;

  if(dt_node_prop_intr(dev, 0, &intr0, NULL) < 0)
    return -1;
  if(dt_node_prop_intr(dev, 1, &intr1, NULL) < 0)
    return -1;

  irq_register(intr0, bcmgenet_intr_irq0, NULL);
  irq_register(intr1, bcmgenet_intr_irq1, NULL);

  printf("bcmgenet: %p %p %d %d\n", base, size, intr0, intr1);

  return bcmgenet_init();
}

static const struct dt_compatible bcmgenet_compat[] = {
  { "brcm,bcm2711-genet-v5" },
  {}
};

DT_PERIPHERAL_INIT(bcmgenet, bcmgenet_compat, bcmgenet_dt_init);
