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
#include "mailbox.h"
#include "assert.h"
#include "mm.h"

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

static void *bcmgenet_base;

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

/* GENET v4 supports 40-bits pointer addressing
 * for obvious reasons the LO and HI word parts
 * are contiguous, but this offsets the other
 * registers.
 */
static const u8 genet_dma_ring_regs[] = {
  [TDMA_READ_PTR] = 0x00,
  [TDMA_READ_PTR_HI] = 0x04,
  [TDMA_CONS_INDEX] = 0x08,
  [TDMA_PROD_INDEX] = 0x0C,
  [DMA_RING_BUF_SIZE] = 0x10,
  [DMA_START_ADDR] = 0x14,
  [DMA_START_ADDR_HI] = 0x18,
  [DMA_END_ADDR] = 0x1C,
  [DMA_END_ADDR_HI] = 0x20,
  [DMA_MBUF_DONE_THRESH] = 0x24,
  [TDMA_FLOW_PERIOD] = 0x28,
  [TDMA_WRITE_PTR] = 0x2C,
  [TDMA_WRITE_PTR_HI] = 0x30,
};

static const u8 bcmgenet_dma_regs[] = {
  [DMA_RING_CFG] = 0x00,
  [DMA_CTRL] = 0x04,
  [DMA_STATUS] = 0x08,
  [DMA_SCB_BURST_SIZE] = 0x0C,
  [DMA_ARB_CTRL] = 0x2C,
  [DMA_PRIORITY_0] = 0x30,
  [DMA_PRIORITY_1] = 0x34,
  [DMA_PRIORITY_2] = 0x38,
  [DMA_RING0_TIMEOUT] = 0x2C,
  [DMA_RING1_TIMEOUT] = 0x30,
  [DMA_RING2_TIMEOUT] = 0x34,
  [DMA_RING3_TIMEOUT] = 0x38,
  [DMA_RING4_TIMEOUT] = 0x3c,
  [DMA_RING5_TIMEOUT] = 0x40,
  [DMA_RING6_TIMEOUT] = 0x44,
  [DMA_RING7_TIMEOUT] = 0x48,
  [DMA_RING8_TIMEOUT] = 0x4c,
  [DMA_RING9_TIMEOUT] = 0x50,
  [DMA_RING10_TIMEOUT] = 0x54,
  [DMA_RING11_TIMEOUT] = 0x58,
  [DMA_RING12_TIMEOUT] = 0x5c,
  [DMA_RING13_TIMEOUT] = 0x60,
  [DMA_RING14_TIMEOUT] = 0x64,
  [DMA_RING15_TIMEOUT] = 0x68,
  [DMA_RING16_TIMEOUT] = 0x6C,
  [DMA_INDEX2RING_0] = 0x70,
  [DMA_INDEX2RING_1] = 0x74,
  [DMA_INDEX2RING_2] = 0x78,
  [DMA_INDEX2RING_3] = 0x7C,
  [DMA_INDEX2RING_4] = 0x80,
  [DMA_INDEX2RING_5] = 0x84,
  [DMA_INDEX2RING_6] = 0x88,
  [DMA_INDEX2RING_7] = 0x8C,
};

static inline u32 bcmgenet_readl(u64 offset) {
  return *(volatile u32 *)((u64)bcmgenet_base + offset);
}

static inline void bcmgenet_writel(u32 val, u64 offset) {
  *(volatile u32 *)((u64)bcmgenet_base + offset) = val;
}

#define GENET_IO_MACRO(name, offset)                                \
  static inline u32 bcmgenet_##name##_readl(u64 reg) {              \
    return bcmgenet_readl(reg + (offset));                          \
  }                                                                 \
  static inline void bcmgenet_##name##_writel(u32 val, u64 reg) {   \
    bcmgenet_writel(val, reg + (offset));                           \
  }

GENET_IO_MACRO(ext, GENET_EXT_OFF);
GENET_IO_MACRO(umac, GENET_UMAC_OFF);
GENET_IO_MACRO(sys, GENET_SYS_OFF);

/* interrupt l2 registers accessors */
GENET_IO_MACRO(intrl2_0, GENET_INTRL2_0_OFF);
GENET_IO_MACRO(intrl2_1, GENET_INTRL2_1_OFF);

/* HFB register accessors  */
GENET_IO_MACRO(hfb, GENET_HFB_OFF);

/* GENET v2+ HFB control and filter len helpers */
GENET_IO_MACRO(hfb_reg, GENET_HFB_REG_OFF);

/* RBUF register accessors */
GENET_IO_MACRO(rbuf, GENET_RBUF_OFF);

static inline u32 bcmgenet_tdma_readl(enum dma_reg r) {
  return bcmgenet_readl(GENET_TDMA_REG_OFF +
      DMA_RINGS_SIZE + bcmgenet_dma_regs[r]);
}

static inline void bcmgenet_tdma_writel(u32 val, enum dma_reg r) {
  bcmgenet_writel(val, GENET_TDMA_REG_OFF +
      DMA_RINGS_SIZE + bcmgenet_dma_regs[r]);
}

static inline u32 bcmgenet_rdma_readl(enum dma_reg r) {
  return bcmgenet_readl(GENET_RDMA_REG_OFF +
      DMA_RINGS_SIZE + bcmgenet_dma_regs[r]);
}

static inline void bcmgenet_rdma_writel(u32 val, enum dma_reg r) {
  bcmgenet_writel(val, GENET_RDMA_REG_OFF +
      DMA_RINGS_SIZE + bcmgenet_dma_regs[r]);
}

static inline u32 bcmgenet_tdma_ring_readl(unsigned int ring,
                                           enum dma_ring_reg r) {
  return bcmgenet_readl(GENET_TDMA_REG_OFF + (DMA_RING_SIZE * ring) +
      genet_dma_ring_regs[r]);
}

static inline void bcmgenet_tdma_ring_writel(unsigned int ring, u32 val,
                                             enum dma_ring_reg r) {
  bcmgenet_writel(val, GENET_TDMA_REG_OFF + (DMA_RING_SIZE * ring) +
      genet_dma_ring_regs[r]);
}

static inline u32 bcmgenet_rdma_ring_readl(unsigned int ring,
                                           enum dma_ring_reg r) {
  return bcmgenet_readl(GENET_RDMA_REG_OFF + (DMA_RING_SIZE * ring) +
      genet_dma_ring_regs[r]);
}

static inline void bcmgenet_rdma_ring_writel(unsigned int ring, u32 val,
                                             enum dma_ring_reg r) {
  bcmgenet_writel(val, GENET_RDMA_REG_OFF + (DMA_RING_SIZE * ring) +
      genet_dma_ring_regs[r]);
}

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

static void free_tx_cb(struct bcmgenet_cb *cb) {
  u8 *buffer = cb->buffer;
  if (buffer) {
    cb->buffer = 0;

    free(buffer);
  }
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

static void bcmgenet_enable_dma(u32 dma_ctrl) {
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
    // bcmgenet_rxintr(rcv, &len);
  }
}

static void bcmgenet_intr_irq1(void *arg) {
  // stub
}

static struct nic_ops bcmgenet_ops = {
  .xmit = bcmgenet_xmit,
};

/*
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
  u32 reg = bcmgenet_rbuf_readl(RBUF_CTRL);
  reg |= RBUF_ALIGN_2B;
  bcmgenet_rbuf_writel(reg, RBUF_CTRL);

  bcmgenet_rbuf_writel(1, RBUF_TBUF_SIZE_CTRL);

  intr_disable();

  // Enable MDIO interrupts on GENET v3+
  // NOTE: MDIO interrupts do not work
  // bcmgenet_intrl2_0_writel(UMAC_IRQ_MDIO_DONE | UMAC_IRQ_MDIO_ERROR,
  // INTRL1_CPU_MASK_CLEAR);
}
*/

static int set_hw_addr(u8 *maddr) {
  mbox_get_mac_address(maddr);

  printf("MAC address is %m\r\n", maddr);

  bcmgenet_umac_writel(
      (maddr[0] << 24) | (maddr[1] << 16) | (maddr[2] << 8) | maddr[3],
      UMAC_MAC0);
  bcmgenet_umac_writel((maddr[4] << 8) | maddr[5], UMAC_MAC1);

  return 0;
}

static int bcmgenet_init() {
  u32 reg = bcmgenet_sys_readl(SYS_REV_CTRL); // read GENET HW version
  u8 major = (reg >> 24 & 0x0f);
  u8 mac[6];

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

  // init_umac();

  reg = bcmgenet_umac_readl(UMAC_CMD); // make sure we reflect the value of CRC_CMD_FWD
  m_crc_fwd_en = !!(reg & CMD_CRC_FWD);

  int ret = set_hw_addr(mac);
  if(ret) {
    printf("Cannot set MAC address (%d)", ret);

    return -1;
  }

  /*
  u32 dma_ctrl = dma_disable(); // disable Rx/Tx DMA and flush Tx queues

  ret = init_dma(); // reinitialize TDMA and RDMA and SW housekeeping
  if (ret) {
    printf("Failed to initialize DMA (%d)", ret);

    return false;
  }

  enable_dma(dma_ctrl); // always enable ring 16 - descriptor ring

  hfb_init();
  */

  // net_init("bcmgenet", mac, mtu, &bcmgenet_dev, &bcmgenet_ops);
  return 0;
}

static int bcmgenet_dt_init(struct device_node *dev) {
  u64 base, size; 
  int intr0, intr1;

  if(dt_node_prop_addr(dev, 0, &base, &size) < 0)
    return -1;

  bcmgenet_base = iomap(base, size);
  if(!bcmgenet_base)
    return -1;

  if(dt_node_prop_intr(dev, 0, &intr0, NULL) < 0)
    return -1;
  if(dt_node_prop_intr(dev, 1, &intr1, NULL) < 0)
    return -1;

  irq_register(intr0, bcmgenet_intr_irq0, NULL);
  irq_register(intr1, bcmgenet_intr_irq1, NULL);

  printf("bcmgenet: %p(%p) %p %d %d\n", base, bcmgenet_base, size, intr0, intr1);

  return bcmgenet_init();
}

static const struct dt_compatible bcmgenet_compat[] = {
  { "brcm,bcm2711-genet-v5" },
  {}
};

DT_PERIPHERAL_INIT(bcmgenet, bcmgenet_compat, bcmgenet_dt_init);
