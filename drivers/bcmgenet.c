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
#include "arch-timer.h"
#include "memlayout.h"
#include "lib.h"

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

#define RX_BUF_LENGTH     8192

#define TX_RING_INDEX     1 // using highest TX priority queue

// Ethernet defaults
#define ETH_FCS_LEN       4
#define ETH_ZLEN          60
/// #define ENET_MAX_MTU_SIZE 1536 // with padding
#define ENET_MAX_MTU_SIZE 4536 // with padding // CHANGED
#define FRAME_BUFFER_SIZE 4500

#define MAX_MC_COUNT 16

static void *bcmgenet_base;
static void *mdio_base;

static struct bcmgenet_data {
  ;
} genet;

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
  return *(volatile u32 *)(bcmgenet_base + offset);
}

static inline void bcmgenet_writel(u32 val, u64 offset) {
  *(volatile u32 *)(bcmgenet_base + offset) = val;
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

static inline u32 bcmgenet_rbuf_ctrl_get() {
  return bcmgenet_sys_readl(SYS_RBUF_FLUSH_CTRL);
}

static inline void bcmgenet_rbuf_ctrl_set(u32 val) {
  bcmgenet_sys_writel(val, SYS_RBUF_FLUSH_CTRL);
}

/* These macros are defined to deal with register map change
 * between GENET1.1 and GENET2. Only those currently being used
 * by driver are defined.
 */
static inline u32 bcmgenet_tbuf_ctrl_get() {
  return bcmgenet_readl(GENET_TBUF_OFF + TBUF_CTRL);
}

static inline void bcmgenet_tbuf_ctrl_set(u32 val) {
  bcmgenet_writel(val, GENET_TBUF_OFF + TBUF_CTRL);
}

static inline u32 bcmgenet_bp_mc_get() {
  return bcmgenet_readl(GENET_TBUF_OFF + TBUF_BP_MC);
}

static inline void bcmgenet_bp_mc_set(u32 val) {
  bcmgenet_writel(val, GENET_TBUF_OFF + TBUF_BP_MC);
}

static struct bcmgenet_cb *get_txcb(struct bcmgenet_tx_ring *ring);
static void bcmgenet_intr_disable(void);
static struct iobuf *rx_refill(struct bcmgenet_cb *cb);

static void bcmgenet_xmit(struct nic *nic, struct iobuf *iobuf) {
  u64 flags;
  unsigned index;
  u32 length;

  assert(iobuf);

  // Mapping strategy:
  // index = 0, unclassified, packet xmited through ring16
  // index = 1, goes to ring 0. (highest priority queue)
  // index = 2, goes to ring 1.
  // index = 3, goes to ring 2.
  // index = 4, goes to ring 3.
  index = TX_RING_INDEX;
  if (index == 0)
    index = GENET_DESC_INDEX;
  else
    index -= 1;

  struct bcmgenet_tx_ring *ring = &m_tx_rings[index];

  spin_lock_irqsave(&m_tx_lock, flags);

  if (ring->free_bds < 2) { // is there room for this frame?
    printf("TX frame dropped!!!!!!!!\r\n");
    spin_unlock_irqrestore(&m_tx_lock, flags);
    return;
  }

  // uint8_t *pTxBuffer = malloc(ENET_MAX_MTU_SIZE);     // allocate and fill DMA

  int num_prod_descs = 1, dma_flag;

  void *tx_header_buffer = iobuf->data;
  length = iobuf->len;
  printf("bcmgenet: xmit: iobuf->len %d", length);

  struct bcmgenet_cb *tx_cb_ptr =
      get_txcb(ring);  // get Tx control block from ring
  assert(tx_cb_ptr != 0);

  // prepare for DMA
  dcache_flush_poc_range(tx_header_buffer, length);

  tx_cb_ptr->buffer = iobuf;  // set DMA buffer in Tx control block

  dma_flag = (length << DMA_BUFLENGTH_SHIFT) |
             (QTAG_MASK << DMA_TX_QTAG_SHIFT) | DMA_TX_APPEND_CRC | DMA_SOP;

  if (!iobuf->body) {
    dma_flag |= DMA_EOP;
  }
  // set DMA descriptor and start transfer
  dmadesc_set(tx_cb_ptr->bd_addr, V2P(tx_header_buffer), dma_flag);

  if (iobuf->body) {
    num_prod_descs = 2;

    void *tx_body_buffer = iobuf->body;
    length = iobuf->body_len;

    tx_cb_ptr = get_txcb(ring);

    dcache_flush_poc_range(tx_body_buffer, length);

    tx_cb_ptr->buffer = iobuf;  // set DMA buffer in Tx control block

    dmadesc_set(tx_cb_ptr->bd_addr, V2P(tx_body_buffer),
                (length << DMA_BUFLENGTH_SHIFT) |
                    (QTAG_MASK << DMA_TX_QTAG_SHIFT) | DMA_TX_APPEND_CRC |
                    DMA_SOP | DMA_EOP);
  }

  // decrement total BD count and advance our write pointer
  ring->free_bds-=num_prod_descs;
  ring->prod_index+=num_prod_descs;
  ring->prod_index &= DMA_P_INDEX_MASK;

  // packets are ready, update producer index
  bcmgenet_tdma_ring_writel(ring->index, ring->prod_index, TDMA_PROD_INDEX);

  spin_unlock_irqrestore(&m_tx_lock, flags);
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
  struct iobuf *buffer = cb->buffer;
  if (buffer) {
    cb->buffer = NULL;

    free_iobuf(buffer);
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


static int bcmgenet_rxintr(void) {
  int ret = -1;

  struct bcmgenet_rx_ring *ring = &m_rx_rings[GENET_DESC_INDEX]; // the only supported Rx queue

  printf("bcmgenet: rxintr\n");

  bcmgenet_intrl2_0_writel(UMAC_IRQ_RXDMA_DONE, INTRL2_CPU_CLEAR);

  unsigned p_index = bcmgenet_rdma_ring_readl(ring->index, RDMA_PROD_INDEX);

  unsigned discards = (p_index >> DMA_P_INDEX_DISCARD_CNT_SHIFT) &
                       DMA_P_INDEX_DISCARD_CNT_MASK;
  if (discards > ring->old_discards) {
    discards = discards - ring->old_discards;
    ring->old_discards += discards;

    // clear HW register when we reach 75% of maximum 0xFFFF
    if (ring->old_discards >= 0xC000) {
      ring->old_discards = 0;
      bcmgenet_rdma_ring_writel(ring->index, 0, RDMA_PROD_INDEX);
    }
  }

  p_index &= DMA_P_INDEX_MASK;

  unsigned rxpkttoprocess = (p_index - ring->c_index) & DMA_C_INDEX_MASK;
  if (rxpkttoprocess > 0) {
    u32 dma_length_status;
    u32 dma_flag;
    int nLength;

    struct bcmgenet_cb *cb = &m_rx_cbs[ring->read_ptr];

    struct iobuf *pRxBuffer = rx_refill(cb);
    if (!pRxBuffer) {
      printf("Missing RX buffer!");
      goto out;
    }

    dma_length_status = dmadesc_get_length_status(cb->bd_addr);
    dma_flag = dma_length_status & 0xFFFF;
    nLength = dma_length_status >> DMA_BUFLENGTH_SHIFT;

    if (!(dma_flag & DMA_EOP) || !(dma_flag & DMA_SOP)) {
      printf("Dropping fragmented RX packet!");
      free_iobuf(pRxBuffer);

      goto out;
    }

    // report errors
    if (dma_flag & (DMA_RX_CRC_ERROR | DMA_RX_OV | DMA_RX_NO | DMA_RX_LG | DMA_RX_RXER)) {
      printf("RX error (0x%x)", (unsigned)dma_flag);
      free_iobuf(pRxBuffer);

      goto out;
    }

    iobuf_set_len(pRxBuffer, nLength);

#define LEADING_PAD 2
    iobuf_pull(pRxBuffer, LEADING_PAD);   // remove HW 2 bytes added for IP alignment

    if (m_crc_fwd_en) {
      nLength -= ETH_FCS_LEN;
    }

    assert(nLength > 0);
    assert(nLength <= FRAME_BUFFER_SIZE);

  //printf("Received %d bytes\r\n", nLength);

//for(int i =0; i< nLength; i++){
//  printf("%02x", ((u8*)pRxBuffer->head)[i]);
//}
    
    netdev_recv(pRxBuffer);
    

    ret = 0;

out:
    if (ring->read_ptr < ring->end_ptr) {
      ring->read_ptr++;
    } else {
      ring->read_ptr = ring->cb_ptr;
    }

    ring->c_index = (ring->c_index + 1) & DMA_C_INDEX_MASK;
    bcmgenet_rdma_ring_writel(ring->index, ring->c_index, RDMA_CONS_INDEX);
  }

  return ret;
}

static void bcmgenet_intr_irq0(void *arg) {
  // Read irq status
  u32 status = bcmgenet_intrl2_0_readl(INTRL2_CPU_STAT) &
               ~bcmgenet_intrl2_0_readl(INTRL2_CPU_MASK_STATUS);

  // clear interrupts
  bcmgenet_intrl2_0_writel(status, INTRL2_CPU_CLEAR);

  if (status & UMAC_IRQ_TXDMA_DONE) {
    spin_lock(&m_tx_lock);

    struct bcmgenet_tx_ring *tx_ring = &m_tx_rings[GENET_DESC_INDEX];

    tx_reclaim(tx_ring);

    spin_unlock(&m_tx_lock);
  }

  if (status & UMAC_IRQ_RXDMA_DONE) {
    // bcmgenet_intrl2_0_writel(UMAC_IRQ_RXDMA_DONE, INTRL2_CPU_CLEAR);
    bcmgenet_rxintr();
  }
}

static void bcmgenet_intr_irq1(void *arg) {
  u32 status = bcmgenet_intrl2_1_readl(INTRL2_CPU_STAT) &
               ~bcmgenet_intrl2_1_readl(INTRL2_CPU_MASK_STATUS);

  // clear interrupts
  bcmgenet_intrl2_1_writel(status, INTRL2_CPU_CLEAR);

  // m_TxSpinLock.Acquire ();
  spin_lock(&m_tx_lock);

  // Check Tx priority queue interrupts
  for (unsigned index = 0; index < TX_QUEUES; index++) {
    if (!(status & BIT(index)))
      continue;

    struct bcmgenet_tx_ring *tx_ring = &m_tx_rings[index];

    tx_reclaim(tx_ring);
  }

  spin_unlock(&m_tx_lock);

  if (status & UMAC_IRQ_RXDMA_DONE) {
    printf("Received! #1\n");
  }
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
  u32 reg = bcmgenet_rbuf_readl(RBUF_CTRL);
  reg |= RBUF_ALIGN_2B;
  bcmgenet_rbuf_writel(reg, RBUF_CTRL);

  bcmgenet_rbuf_writel(1, RBUF_TBUF_SIZE_CTRL);

  bcmgenet_intr_disable();

  // Enable MDIO interrupts on GENET v3+
  // NOTE: MDIO interrupts do not work
  // bcmgenet_intrl2_0_writel(UMAC_IRQ_MDIO_DONE | UMAC_IRQ_MDIO_ERROR,
  // INTRL1_CPU_MASK_CLEAR);
}

static void bcmgenet_intr_disable(void) {
  // Mask all interrupts
  bcmgenet_intrl2_0_writel(0xFFFFFFFF, INTRL2_CPU_MASK_SET);
  bcmgenet_intrl2_0_writel(0xFFFFFFFF, INTRL2_CPU_CLEAR);
  bcmgenet_intrl2_1_writel(0xFFFFFFFF, INTRL2_CPU_MASK_SET);
  bcmgenet_intrl2_1_writel(0xFFFFFFFF, INTRL2_CPU_CLEAR);
}

static void enable_tx_intr(void) {
  struct bcmgenet_tx_ring *ring;
  for (unsigned i = 0; i < TX_QUEUES; ++i) {
    ring = &m_tx_rings[i];
    ring->int_enable(ring);
  }

  ring = &m_tx_rings[GENET_DESC_INDEX];
  ring->int_enable(ring);
}

static void enable_rx_intr(void) {
  struct bcmgenet_rx_ring *ring = &m_rx_rings[GENET_DESC_INDEX];
  ring->int_enable(ring);
}

static void link_intr_enable(void) {
  bcmgenet_intrl2_0_writel(UMAC_IRQ_LINK_EVENT, INTRL2_CPU_MASK_CLEAR);
}

static void tx_ring16_int_enable(struct bcmgenet_tx_ring *ring) {
  bcmgenet_intrl2_0_writel(UMAC_IRQ_TXDMA_DONE, INTRL2_CPU_MASK_CLEAR);
}

static void tx_ring_int_enable(struct bcmgenet_tx_ring *ring) {
  bcmgenet_intrl2_1_writel(1 << ring->index, INTRL2_CPU_MASK_CLEAR);
}

static void rx_ring16_int_enable(struct bcmgenet_rx_ring *ring) {
  bcmgenet_intrl2_0_writel(UMAC_IRQ_RXDMA_DONE, INTRL2_CPU_MASK_CLEAR);
}

static int set_hw_addr(u8 *maddr) {
  mbox_get_mac_address(maddr);

  printf("MAC address is %m\r\n", maddr);

  bcmgenet_umac_writel(
      (maddr[0] << 24) | (maddr[1] << 16) | (maddr[2] << 8) | maddr[3],
      UMAC_MAC0);
  bcmgenet_umac_writel((maddr[4] << 8) | maddr[5], UMAC_MAC1);

  return 0;
}

static void bcmgenet_dma_enable(u32 dma_ctrl) {
  u32 reg = bcmgenet_rdma_readl(DMA_CTRL);
  reg |= dma_ctrl;
  bcmgenet_rdma_writel(reg, DMA_CTRL);

  reg = bcmgenet_tdma_readl(DMA_CTRL);
  reg |= dma_ctrl;
  bcmgenet_tdma_writel(reg, DMA_CTRL);
}

static u32 bcmgenet_dma_disable() {
  // disable DMA
  u32 dma_ctrl = 1 << (GENET_DESC_INDEX + DMA_RING_BUF_EN_SHIFT) | DMA_EN;
  u32 reg = bcmgenet_tdma_readl(DMA_CTRL);
  reg &= ~dma_ctrl;
  bcmgenet_tdma_writel(reg, DMA_CTRL);

  reg = bcmgenet_rdma_readl(DMA_CTRL);
  reg &= ~dma_ctrl;
  bcmgenet_rdma_writel(reg, DMA_CTRL);

  bcmgenet_umac_writel(1, UMAC_TX_FLUSH);
  usleep(10);
  bcmgenet_umac_writel(0, UMAC_TX_FLUSH);

  return dma_ctrl;
}

static struct iobuf *free_rx_cb(struct bcmgenet_cb *cb) {
  struct iobuf *buffer = cb->buffer;
  cb->buffer = NULL;
  // printk("Free %x\r\n", &buffer);

  if(buffer) {
    dcache_flush_poc_range(buffer->data, RX_BUF_LENGTH);
  }

  return buffer;
}

static struct iobuf *rx_refill(struct bcmgenet_cb *cb) {
  dma_addr_t dmabuf;
  struct iobuf *rx_buffer;

  // Allocate a new Rx DMA buffer
  // u8 *buffer = new u8[RX_BUF_LENGTH];
  // u8 *buffer = malloc(RX_BUF_LENGTH);
  // u8 *buffer = alloc_page();
  struct iobuf *buffer = alloc_iobuf(RX_BUF_LENGTH);
  if (!buffer)
    return NULL;
  // printk("Refill %x\r\n", &buffer);
  //  prepare buffer for DMA
  dcache_flush_poc_range(buffer->data, RX_BUF_LENGTH);

  // Grab the current Rx buffer from the ring and DMA-unmap it
  rx_buffer = free_rx_cb(cb);

  // Put the new Rx buffer on the ring
  cb->buffer = buffer;
  dmabuf = V2P(buffer->data);
  dmadesc_set_addr(cb->bd_addr, dmabuf);

  // Return the current Rx buffer to caller
  return rx_buffer;
}

// Assign DMA buffer to Rx DMA descriptor
static int alloc_rx_buffers(struct bcmgenet_rx_ring *ring) {
  // loop here for each buffer needing assign
  for (unsigned i = 0; i < ring->size; i++) {
    struct bcmgenet_cb *cb = ring->cbs + i;
    rx_refill(cb);
    if (!cb->buffer)
      return -1;
  }

  return 0;
}

// Initialize a RDMA ring
static int init_rx_ring(unsigned int index, unsigned int size,
                        unsigned int start_ptr, unsigned int end_ptr) {
  struct bcmgenet_rx_ring *ring = &m_rx_rings[index];

  ring->index = index;

  assert(index == GENET_DESC_INDEX);
  ring->int_enable = rx_ring16_int_enable;

  ring->cbs = m_rx_cbs + start_ptr;
  ring->size = size;
  ring->c_index = 0;
  ring->read_ptr = start_ptr;
  ring->cb_ptr = start_ptr;
  ring->end_ptr = end_ptr - 1;
  ring->old_discards = 0;

  int ret = alloc_rx_buffers(ring);
  if (ret)
    return ret;

  bcmgenet_rdma_ring_writel(index, 0, RDMA_PROD_INDEX);
  bcmgenet_rdma_ring_writel(index, 0, RDMA_CONS_INDEX);
  bcmgenet_rdma_ring_writel(index, ((size << DMA_RING_SIZE_SHIFT) | RX_BUF_LENGTH),
      DMA_RING_BUF_SIZE);
  bcmgenet_rdma_ring_writel(
      index,
      (DMA_FC_THRESH_LO << DMA_XOFF_THRESHOLD_SHIFT) | DMA_FC_THRESH_HI,
      RDMA_XON_XOFF_THRESH);

  // Set start and end address, read and write pointers
  bcmgenet_rdma_ring_writel(index, start_ptr * WORDS_PER_BD, DMA_START_ADDR);
  bcmgenet_rdma_ring_writel(index, start_ptr * WORDS_PER_BD, RDMA_READ_PTR);
  bcmgenet_rdma_ring_writel(index, start_ptr * WORDS_PER_BD, RDMA_WRITE_PTR);
  bcmgenet_rdma_ring_writel(index, end_ptr * WORDS_PER_BD - 1, DMA_END_ADDR);

  return ret;
}

// Initialize a Tx ring along with corresponding hardware registers
static void init_tx_ring(unsigned int index, unsigned int size,
                         unsigned int start_ptr, unsigned int end_ptr) {
  struct bcmgenet_tx_ring *ring = &m_tx_rings[index];

  ring->index = index;
  if (index == GENET_DESC_INDEX) {
    ring->queue = 0;
    ring->int_enable = tx_ring16_int_enable;
  } else {
    ring->queue = index + 1;
    ring->int_enable = tx_ring_int_enable;
  }
  ring->cbs = m_tx_cbs + start_ptr;
  ring->size = size;
  ring->clean_ptr = start_ptr;
  ring->c_index = 0;
  ring->free_bds = size;
  ring->write_ptr = start_ptr;
  ring->cb_ptr = start_ptr;
  ring->end_ptr = end_ptr - 1;
  ring->prod_index = 0;

  // Set flow period for ring != 16
  u32 flow_period_val = 0;
  if (index != GENET_DESC_INDEX)
    flow_period_val = ENET_MAX_MTU_SIZE << 16;

  bcmgenet_tdma_ring_writel(index, 0, TDMA_PROD_INDEX);
  bcmgenet_tdma_ring_writel(index, 0, TDMA_CONS_INDEX);
  bcmgenet_tdma_ring_writel(index, 10, DMA_MBUF_DONE_THRESH);
  // Disable rate control for now
  bcmgenet_tdma_ring_writel(index, flow_period_val, TDMA_FLOW_PERIOD);
  bcmgenet_tdma_ring_writel(index, ((size << DMA_RING_SIZE_SHIFT) | RX_BUF_LENGTH),
      DMA_RING_BUF_SIZE);

  // Set start and end address, read and write pointers
  bcmgenet_tdma_ring_writel(index, start_ptr * WORDS_PER_BD, DMA_START_ADDR);
  bcmgenet_tdma_ring_writel(index, start_ptr * WORDS_PER_BD, TDMA_READ_PTR);
  bcmgenet_tdma_ring_writel(index, start_ptr * WORDS_PER_BD, TDMA_WRITE_PTR);
  bcmgenet_tdma_ring_writel(index, end_ptr * WORDS_PER_BD - 1, DMA_END_ADDR);
}

static int init_rx_queues() {
  u32 dma_ctrl = bcmgenet_rdma_readl(DMA_CTRL);
  u32 dma_enable = dma_ctrl & DMA_EN;
  dma_ctrl &= ~DMA_EN;
  bcmgenet_rdma_writel(dma_ctrl, DMA_CTRL);

  dma_ctrl = 0;
  u32 ring_cfg = 0;

  // Initialize Rx default queue 16
  int ret = init_rx_ring(GENET_DESC_INDEX, GENET_Q16_RX_BD_CNT,
                         RX_QUEUES * RX_BDS_PER_Q, TOTAL_DESC);
  if (ret)
    return ret;

  ring_cfg |= (1 << GENET_DESC_INDEX);
  dma_ctrl |= (1 << (GENET_DESC_INDEX + DMA_RING_BUF_EN_SHIFT));

  // Enable rings
  bcmgenet_rdma_writel(ring_cfg, DMA_RING_CFG);

  // Configure ring as descriptor ring and re-enable DMA if enabled
  if (dma_enable)
    dma_ctrl |= DMA_EN;
  bcmgenet_rdma_writel(dma_ctrl, DMA_CTRL);

  return 0;
}

static void init_tx_queues(bool enable) {
  u32 dma_ctrl = bcmgenet_tdma_readl(DMA_CTRL);
  u32 dma_enable = dma_ctrl & DMA_EN;
  dma_ctrl &= ~DMA_EN;
  bcmgenet_tdma_writel(dma_ctrl, DMA_CTRL);

  dma_ctrl = 0;
  u32 ring_cfg = 0;

  if (enable) {
    // Enable strict priority arbiter mode
    bcmgenet_tdma_writel(DMA_ARBITER_SP, DMA_ARB_CTRL);
  }

  u32 dma_priority[3] = {0, 0, 0};

  // Initialize Tx priority queues
  for (unsigned int i = 0; i < TX_QUEUES; i++) {
    init_tx_ring(i, TX_BDS_PER_Q, i * TX_BDS_PER_Q, (i + 1) * TX_BDS_PER_Q);
    ring_cfg |= (1 << i);
    dma_ctrl |= (1 << (i + DMA_RING_BUF_EN_SHIFT));
    dma_priority[DMA_PRIO_REG_INDEX(i)] |=
      ((GENET_Q0_PRIORITY + i) << DMA_PRIO_REG_SHIFT(i));
  }

  // Initialize Tx default queue 16
  init_tx_ring(GENET_DESC_INDEX, GENET_Q16_TX_BD_CNT,
      TX_QUEUES * TX_BDS_PER_Q, TOTAL_DESC);
  ring_cfg |= (1 << GENET_DESC_INDEX);
  dma_ctrl |= (1 << (GENET_DESC_INDEX + DMA_RING_BUF_EN_SHIFT));
  dma_priority[DMA_PRIO_REG_INDEX(GENET_DESC_INDEX)] |=
    ((GENET_Q0_PRIORITY + TX_QUEUES)
     << DMA_PRIO_REG_SHIFT(GENET_DESC_INDEX));

  if (enable) {
    // Set Tx queue priorities
    bcmgenet_tdma_writel(dma_priority[0], DMA_PRIORITY_0);
    bcmgenet_tdma_writel(dma_priority[1], DMA_PRIORITY_1);
    bcmgenet_tdma_writel(dma_priority[2], DMA_PRIORITY_2);

    // Enable Tx queues
    bcmgenet_tdma_writel(ring_cfg, DMA_RING_CFG);

    // Enable Tx DMA
    if (dma_enable)
      dma_ctrl |= DMA_EN;
    bcmgenet_tdma_writel(dma_ctrl, DMA_CTRL);
  } else {
    // Disable Tx queues
    bcmgenet_tdma_writel(0, DMA_RING_CFG);

    // Disable Tx DMA
    bcmgenet_tdma_writel(0, DMA_CTRL);
  }
}

static int init_dma() {
  int ret;

  // Initialize common Rx ring structures
  // m_rx_cbs = malloc(sizeof(struct bcmgenet_cb) * TOTAL_DESC);
  m_rx_cbs = alloc_page();
  if (!m_rx_cbs) {
    ret = -1;
    goto err_free;
  }

  memset(m_rx_cbs, 0, TOTAL_DESC * sizeof(struct bcmgenet_cb));

  struct bcmgenet_cb *cb;
  unsigned int i;
  for (i = 0; i < TOTAL_DESC; i++) {
    cb = m_rx_cbs + i;
    cb->bd_addr = bcmgenet_base + RDMA_OFFSET + i * DMA_DESC_SIZE;
  }

  // Initialize common TX ring structures
  // m_tx_cbs = malloc(sizeof(struct bcmgenet_cb) * TOTAL_DESC);
  m_tx_cbs = alloc_page();
  if (!m_tx_cbs) {
    ret = -1;
    goto err_free;
  }

  memset(m_tx_cbs, 0, TOTAL_DESC * sizeof(struct bcmgenet_cb));

  for (i = 0; i < TOTAL_DESC; i++) {
    cb = m_tx_cbs + i;
    cb->bd_addr = bcmgenet_base + TDMA_OFFSET + i * DMA_DESC_SIZE;
  }

  // Init rDma
  bcmgenet_rdma_writel(DMA_MAX_BURST_LENGTH, DMA_SCB_BURST_SIZE);

  // Initialize Rx queues
  ret = init_rx_queues();
  if (ret) {
    printf("Failed to initialize RX queues (%d)", ret);
    // free_rx_buffers();
    goto err_free;
  }

  // Init tDma
  bcmgenet_tdma_writel(DMA_MAX_BURST_LENGTH, DMA_SCB_BURST_SIZE);

  // Initialize Tx queues
  init_tx_queues(true);

  return 0;

err_free:
  if(m_rx_cbs) {
    free(m_rx_cbs);
    m_rx_cbs = NULL;
  }

  if(m_tx_cbs) {
    free(m_tx_cbs);
    m_tx_cbs = NULL;
  }

  return ret;
}


static void hfb_init() {
  // this has no function, but to suppress warnings from clang compiler >>>
  bcmgenet_hfb_reg_readl(HFB_CTRL);
  bcmgenet_hfb_readl(0);
  // <<<

  bcmgenet_hfb_reg_writel(0, HFB_CTRL);
  bcmgenet_hfb_reg_writel(0, HFB_FLT_ENABLE_V3PLUS);
  bcmgenet_hfb_reg_writel(0, HFB_FLT_ENABLE_V3PLUS + 4);

  u32 i;
  for (i = DMA_INDEX2RING_0; i <= DMA_INDEX2RING_7; i++)
    bcmgenet_rdma_writel(0, i);

  for (i = 0; i < (HFB_FILTER_CNT / 4); i++)
    bcmgenet_hfb_reg_writel(0, HFB_FLT_LEN_V3PLUS + i * sizeof(u32));

  for (i = 0; i < HFB_FILTER_CNT * HFB_FILTER_SIZE; i++)
    bcmgenet_hfb_writel(0, i * sizeof(u32));
}

//
// UniMAC MDIO
//

#define MDIO_CMD 0x00 // same register as UMAC_MDIO_CMD

static inline u32 mdio_readl(u32 offset) {
  return readl(mdio_base + offset);
}

static inline void mdio_writel(u32 val, u32 offset) {
  writel(val, mdio_base + offset);
}

static int bcmgenet_mdio_read(int reg);
static void bcmgenet_mdio_write(int reg, u16 val);

static inline void mdio_start(void) {
  u32 reg = mdio_readl(MDIO_CMD);
  reg |= MDIO_START_BUSY;
  mdio_writel(reg, MDIO_CMD);
}

// This is not the default wait_func from the UniMAC MDIO driver,
// but a private function assigned by the GENET bcmmii module.
static void mdio_wait(void) {
  // assert (m_pTimer != 0);
  // unsigned nStartTicks = m_pTimer->GetClockTicks ();

  do {
    // if (m_pTimer->GetClockTicks ()-nStartTicks >= CLOCKHZ / 100)
    // {
    //      break;
    // }
  } while (bcmgenet_umac_readl(UMAC_MDIO_CMD) & MDIO_START_BUSY);
}

// static inline unsigned mdio_busy(void)
// {
//      return mdio_readl(MDIO_CMD) & MDIO_START_BUSY;
// }

// Workaround for integrated BCM7xxx Gigabit PHYs which have a problem with
// their internal MDIO management controller making them fail to successfully
// be read from or written to for the first transaction.  We insert a dummy
// BMSR read here to make sure that phy_get_device() and get_phy_id() can
// correctly read the PHY MII_PHYSID1/2 registers and successfully register a
// PHY device for this peripheral.
static int mdio_reset(void) {
  int ret = bcmgenet_mdio_read(MII_BMSR);
  if (ret < 0)
    return ret;

  return 0;
}

static int bcmgenet_mdio_read(int reg) {
  // Prepare the read operation
  u32 cmd = MDIO_RD | (m_phy_id << MDIO_PMD_SHIFT) | (reg << MDIO_REG_SHIFT);
  mdio_writel(cmd, MDIO_CMD);

  mdio_start();
  mdio_wait();

  cmd = mdio_readl(MDIO_CMD);

  if (cmd & MDIO_READ_FAIL)
    return -1;

  return cmd & 0xFFFF;
}

static void bcmgenet_mdio_write(int reg, u16 val) {
  // Prepare the write operation
  u32 cmd = MDIO_WR | (m_phy_id << MDIO_PMD_SHIFT) |
    (reg << MDIO_REG_SHIFT) | (0xFFFF & val);
  mdio_writel(cmd, MDIO_CMD);

  mdio_start();
  mdio_wait();
}

static int phy_read_status() {
  // Update the link status, return if there was an error
  int bmsr = bcmgenet_mdio_read(MII_BMSR);
  if (bmsr < 0)
    return bmsr;

  if ((bmsr & BMSR_LSTATUS) == 0) {
    m_link = 0;
    return 0;
  } else
    m_link = 1;

  // Read autonegotiation status
  // NOTE: autonegotiation is enabled by firmware, not here

  int lpagb = bcmgenet_mdio_read(MII_STAT1000);
  if (lpagb < 0)
    return lpagb;

  int ctrl1000 = bcmgenet_mdio_read(MII_CTRL1000);
  if (ctrl1000 < 0)
    return ctrl1000;

  if (lpagb & LPA_1000MSFAIL) {
    printf("Master/Slave resolution failed (0x%X)", ctrl1000);
    return -1;
  }

  int common_adv_gb = lpagb & ctrl1000 << 2;

  int lpa = bcmgenet_mdio_read(MII_LPA);
  if (lpa < 0)
    return lpa;

  int adv = bcmgenet_mdio_read(MII_ADVERTISE);
  if (adv < 0)
    return adv;

  int common_adv = lpa & adv;

  m_speed = 10;
  m_duplex = 0;
  m_pause = 0;

  if (common_adv_gb & (LPA_1000FULL | LPA_1000HALF)) {
    m_speed = 1000;

    if (common_adv_gb & LPA_1000FULL)
      m_duplex = 1;
  } else if (common_adv & (LPA_100FULL | LPA_100HALF)) {
    m_speed = 100;

    if (common_adv & LPA_100FULL)
      m_duplex = 1;
  } else if (common_adv & LPA_10FULL) {
    m_duplex = 1;
  }

  if (m_duplex == 1)
    m_pause = lpa & LPA_PAUSE_CAP ? 1 : 0;

  return 0;
}

static void mii_setup() {
  // setup netdev link state when PHY link status change and
  // update UMAC and RGMII block when link up
  bool status_changed = false;

  if (m_old_link != m_link) {
    status_changed = true;
    m_old_link = m_link;
  }

  if (!m_link)
    return;

  // check speed/duplex/pause changes
  if (m_old_speed != m_speed) {
    status_changed = true;
    m_old_speed = m_speed;
  }

  if (m_old_duplex != m_duplex) {
    status_changed = true;
    m_old_duplex = m_duplex;
  }

  if (m_old_pause != m_pause) {
    status_changed = true;
    m_old_pause = m_pause;
  }

  // done if nothing has changed
  if (!status_changed)
    return;

  // speed
  u32 cmd_bits = 0;
  if (m_speed == 1000)
    cmd_bits = UMAC_SPEED_1000;
  else if (m_speed == 100)
    cmd_bits = UMAC_SPEED_100;
  else
    cmd_bits = UMAC_SPEED_10;
  cmd_bits <<= CMD_SPEED_SHIFT;

  // duplex
  if (!m_duplex)
    cmd_bits |= CMD_HD_EN;

  // pause capability
  if (!m_pause)
    cmd_bits |= CMD_RX_PAUSE_IGNORE | CMD_TX_PAUSE_IGNORE;

  // Program UMAC and RGMII block based on established
  // link speed, duplex, and pause. The speed set in
  // umac->cmd tell RGMII block which clock to use for
  // transmit -- 25MHz(100Mbps) or 125MHz(1Gbps).
  // Receive clock is provided by the PHY.
  u32 reg = bcmgenet_ext_readl(EXT_RGMII_OOB_CTRL);
  reg &= ~OOB_DISABLE;
  reg |= RGMII_LINK;
  bcmgenet_ext_writel(reg, EXT_RGMII_OOB_CTRL);

  reg = bcmgenet_umac_readl(UMAC_CMD);
  reg &= ~((CMD_SPEED_MASK << CMD_SPEED_SHIFT) | CMD_HD_EN |
      CMD_RX_PAUSE_IGNORE | CMD_TX_PAUSE_IGNORE);
  reg |= cmd_bits;
  bcmgenet_umac_writel(reg, UMAC_CMD);
}

static int mii_config(bool init) {
  // RGMII_NO_ID: TXC transitions at the same time as TXD
  //          (requires PCB or receiver-side delay)
  // RGMII:   Add 2ns delay on TXC (90 degree shift)
  //
  // ID is implicitly disabled for 100Mbps (RG)MII operation.
  u32 id_mode_dis = BIT(16);

  bcmgenet_sys_writel(PORT_MODE_EXT_GPHY, SYS_PORT_CTRL);

  // This is an external PHY (xMII), so we need to enable the RGMII
  // block for the interface to work
  u32 reg = bcmgenet_ext_readl(EXT_RGMII_OOB_CTRL);
  reg |= RGMII_MODE_EN | id_mode_dis;
  bcmgenet_ext_writel(reg, EXT_RGMII_OOB_CTRL);

  return 0;
}

static int mii_probe(void) {
  // Initialize link state variables that mii_setup() uses
  m_old_link = -1;
  m_old_speed = -1;
  m_old_duplex = -1;
  m_old_pause = -1;

  // probe PHY
  m_phy_id = 0x01;
  int ret = mdio_reset();
  if (ret) {
    m_phy_id = 0x00;
    ret = mdio_reset();
  }
  if (ret)
    return ret;

  ret = phy_read_status();
  if (ret)
    return ret;

  mii_setup();

  return mii_config(true); // Configure port multiplexer
}

static void set_mdf_addr(u8 *addr, int *i, int *mc) {
  bcmgenet_umac_writel(addr[0] << 8 | addr[1], UMAC_MDF_ADDR + (*i * 4));
  bcmgenet_umac_writel(addr[2] << 24 | addr[3] << 16 | addr[4] << 8 | addr[5],
                       UMAC_MDF_ADDR + ((*i + 1) * 4));

  u32 reg = bcmgenet_umac_readl(UMAC_MDF_CTRL);
  reg |= (1 << (MAX_MC_COUNT - *mc));
  bcmgenet_umac_writel(reg, UMAC_MDF_CTRL);

  *i += 2;
  (*mc)++;
}

static void set_rx_mode(u8 *mac) {
  // Promiscuous mode off
  u32 reg = bcmgenet_umac_readl(UMAC_CMD);
  reg &= ~CMD_PROMISC;
  bcmgenet_umac_writel(reg, UMAC_CMD);

  // update MDF filter
  int i = 0;
  int mc = 0;

  u8 Buffer_BR[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  set_mdf_addr(Buffer_BR, &i, &mc);

  // my own address
  set_mdf_addr(mac, &i, &mc);
}

static void umac_enable_set(u32 mask, bool enable) {
  u32 reg = bcmgenet_umac_readl(UMAC_CMD);
  if (enable)
    reg |= mask;
  else
    reg &= ~mask;

  bcmgenet_umac_writel(reg, UMAC_CMD);
  // UniMAC stops on a packet boundary, wait for a full-size packet to be
  // processed
  if (enable == 0)
    usleep(2000);
}

static void netif_start() {
  enable_rx_intr(); // NOTE: Rx interrupts are not needed
  umac_enable_set(CMD_TX_EN | CMD_RX_EN, true);
  enable_tx_intr();
  // link_intr_enable();              // NOTE: link interrupts do not work, must
  // be polled
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

  init_umac();

  reg = bcmgenet_umac_readl(UMAC_CMD); // make sure we reflect the value of CRC_CMD_FWD
  m_crc_fwd_en = !!(reg & CMD_CRC_FWD);

  int ret = set_hw_addr(mac);
  if(ret) {
    printf("Cannot set MAC address (%d)", ret);

    return -1;
  }

  u32 dma_ctrl = bcmgenet_dma_disable(); // disable Rx/Tx DMA and flush Tx queues

  ret = init_dma(); // reinitialize TDMA and RDMA and SW housekeeping
  if (ret) {
    printf("Failed to initialize DMA (%d)", ret);
    return -1;
  }

  bcmgenet_dma_enable(dma_ctrl); // always enable ring 16 - descriptor ring

  hfb_init();

  ret = mii_probe();
  if (ret) {
    printf("Failed to connect to PHY (%d)\r\n", ret);
    return -1;
  }

  netif_start();
  set_rx_mode(mac);

  net_init("bcmgenet", mac, ENET_MAX_MTU_SIZE, NULL, &bcmgenet_ops);

  return 0;
}

static int bcmgenet_dt_init(struct device_node *dev) {
  physaddr_t base;
  u64 size; 
  u64 mdio_offset, mdio_size;
  int intr0, intr1;
  struct device_node *mdio;

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

  mdio = dt_compatible_child(dev, "brcm,genet-mdio-v5");
  if(!mdio)
    panic("mdio?");

  if(dt_node_prop_addr(mdio, 0, &mdio_offset, &mdio_size) < 0)
    return -1;

  mdio_base = bcmgenet_base + mdio_offset;

  printf("bcmgenet: %p(%p) %p %d %d\n", base, bcmgenet_base, size, intr0, intr1);
  printf("mdio: %p %p %p\n", mdio_offset, mdio_size, mdio_base);

  return bcmgenet_init();
}

static const struct dt_compatible bcmgenet_compat[] = {
  { "brcm,bcm2711-genet-v5" },
  {}
};

DT_PERIPHERAL_INIT(bcmgenet, bcmgenet_compat, bcmgenet_dt_init);
