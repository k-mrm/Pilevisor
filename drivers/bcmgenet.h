#ifndef DRIVER_BCMGENET_H
#define DRIVER_BCMGENET_H

#include "types.h"
#include "lib.h"
#include "net.h"

#define GENET_DESC_INDEX 16 // max. 16 priority queues and 1 default queue

struct bcmgenet_cb {
  void *bd_addr;          // address of HW buffer descriptor
  struct iobuf *buffer;   // frame buffer
};

struct bcmgenet_tx_ring {
  u32 index;          // ring index
  u32 queue;          // queue index
  struct bcmgenet_cb *cbs; // tx ring buffer control block*/
  u32 size;           // size of each tx ring
  u32 clean_ptr;      // Tx ring clean pointer
  u32 c_index;        // last consumer index of each ring*/
  u32 free_bds;       // # of free bds for each ring
  u32 write_ptr;      // Tx ring write pointer SW copy
  u32 prod_index;     // Tx ring producer index SW copy
  u32 cb_ptr;         // Tx ring initial CB ptr
  u32 end_ptr;        // Tx ring end CB ptr
  void (*int_enable)(struct bcmgenet_tx_ring *);
};

struct bcmgenet_rx_ring {
  u32 index;          // Rx ring index
  struct bcmgenet_cb *cbs; // Rx ring buffer control block
  u32 size;           // Rx ring size
  u32 c_index;        // Rx last consumer index
  u32 read_ptr;       // Rx ring read pointer
  u32 cb_ptr;         // Rx ring initial CB ptr
  u32 end_ptr;        // Rx ring end CB ptr
  u32 old_discards;
  void (*int_enable)(struct bcmgenet_rx_ring *);
};

/* total number of Buffer Descriptors, same for Rx/Tx */
#define TOTAL_DESC 256

/* which ring is descriptor based */
#define DESC_INDEX 16

/* Body(1500) + EH_SIZE(14) + VLANTAG(4) + BRCMTAG(6) + FCS(4) = 1528.
 * 1536 is multiple of 256 bytes
 */
#define ENET_BRCM_TAG_LEN 6
#define ENET_PAD 8
/*
#define ENET_MAX_MTU_SIZE (ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN + \
                           ENET_BRCM_TAG_LEN + ETH_FCS_LEN + ENET_PAD)
                           */
#define DMA_MAX_BURST_LENGTH 0x08

/* misc. configuration */
#define CLEAR_ALL_HFB 0xFF
#define DMA_FC_THRESH_HI (TOTAL_DESC >> 4)
#define DMA_FC_THRESH_LO 5

/* Rx status bits */
#define STATUS_RX_EXT_MASK 0x1FFFFF
#define STATUS_RX_CSUM_MASK 0xFFFF
#define STATUS_RX_CSUM_OK 0x10000
#define STATUS_RX_CSUM_FR 0x20000
#define STATUS_RX_PROTO_TCP 0
#define STATUS_RX_PROTO_UDP 1
#define STATUS_RX_PROTO_ICMP 2
#define STATUS_RX_PROTO_OTHER 3
#define STATUS_RX_PROTO_MASK 3
#define STATUS_RX_PROTO_SHIFT 18
#define STATUS_FILTER_INDEX_MASK 0xFFFF
/* Tx status bits */
#define STATUS_TX_CSUM_START_MASK 0X7FFF
#define STATUS_TX_CSUM_START_SHIFT 16
#define STATUS_TX_CSUM_PROTO_UDP 0x8000
#define STATUS_TX_CSUM_OFFSET_MASK 0x7FFF
#define STATUS_TX_CSUM_LV 0x80000000

/* DMA Descriptor */
#define DMA_DESC_LENGTH_STATUS 0x00 /* in bytes of data in buffer */
#define DMA_DESC_ADDRESS_LO 0x04    /* lower bits of PA */
#define DMA_DESC_ADDRESS_HI 0x08    /* upper 32 bits of PA, GENETv4+ */

#define UMAC_HD_BKP_CTRL 0x004
#define HD_FC_EN (1 << 0)
#define HD_FC_BKOFF_OK (1 << 1)
#define IPG_CONFIG_RX_SHIFT 2
#define IPG_CONFIG_RX_MASK 0x1F

#define UMAC_CMD 0x008
#define CMD_TX_EN (1 << 0)
#define CMD_RX_EN (1 << 1)
#define UMAC_SPEED_10 0
#define UMAC_SPEED_100 1
#define UMAC_SPEED_1000 2
#define UMAC_SPEED_2500 3
#define CMD_SPEED_SHIFT 2
#define CMD_SPEED_MASK 3
#define CMD_PROMISC (1 << 4)
#define CMD_PAD_EN (1 << 5)
#define CMD_CRC_FWD (1 << 6)
#define CMD_PAUSE_FWD (1 << 7)
#define CMD_RX_PAUSE_IGNORE (1 << 8)
#define CMD_TX_ADDR_INS (1 << 9)
#define CMD_HD_EN (1 << 10)
#define CMD_SW_RESET (1 << 13)
#define CMD_LCL_LOOP_EN (1 << 15)
#define CMD_AUTO_CONFIG (1 << 22)
#define CMD_CNTL_FRM_EN (1 << 23)
#define CMD_NO_LEN_CHK (1 << 24)
#define CMD_RMT_LOOP_EN (1 << 25)
#define CMD_PRBL_EN (1 << 27)
#define CMD_TX_PAUSE_IGNORE (1 << 28)
#define CMD_TX_RX_EN (1 << 29)
#define CMD_RUNT_FILTER_DIS (1 << 30)

#define UMAC_MAC0 0x00C
#define UMAC_MAC1 0x010
#define UMAC_MAX_FRAME_LEN 0x014

#define UMAC_MODE 0x44
#define MODE_LINK_STATUS (1 << 5)

#define UMAC_EEE_CTRL 0x064
#define EN_LPI_RX_PAUSE (1 << 0)
#define EN_LPI_TX_PFC (1 << 1)
#define EN_LPI_TX_PAUSE (1 << 2)
#define EEE_EN (1 << 3)
#define RX_FIFO_CHECK (1 << 4)
#define EEE_TX_CLK_DIS (1 << 5)
#define DIS_EEE_10M (1 << 6)
#define LP_IDLE_PREDICTION_MODE (1 << 7)

#define UMAC_EEE_LPI_TIMER 0x068
#define UMAC_EEE_WAKE_TIMER 0x06C
#define UMAC_EEE_REF_COUNT 0x070
#define EEE_REFERENCE_COUNT_MASK 0xffff

#define UMAC_TX_FLUSH 0x334

#define UMAC_MIB_START 0x400

#define UMAC_MDIO_CMD 0x614 // same as
#define MDIO_START_BUSY (1 << 29)
#define MDIO_READ_FAIL (1 << 28)
#define MDIO_RD (2 << 26)
#define MDIO_WR (1 << 26)
#define MDIO_PMD_SHIFT 21
#define MDIO_PMD_MASK 0x1F
#define MDIO_REG_SHIFT 16
#define MDIO_REG_MASK 0x1F

#define UMAC_RBUF_OVFL_CNT_V1 0x61C
#define RBUF_OVFL_CNT_V2 0x80
#define RBUF_OVFL_CNT_V3PLUS 0x94

#define UMAC_MPD_CTRL 0x620
#define MPD_EN (1 << 0)
#define MPD_PW_EN (1 << 27)
#define MPD_MSEQ_LEN_SHIFT 16
#define MPD_MSEQ_LEN_MASK 0xFF

#define UMAC_MPD_PW_MS 0x624
#define UMAC_MPD_PW_LS 0x628
#define UMAC_RBUF_ERR_CNT_V1 0x634
#define RBUF_ERR_CNT_V2 0x84
#define RBUF_ERR_CNT_V3PLUS 0x98
#define UMAC_MDF_ERR_CNT 0x638
#define UMAC_MDF_CTRL 0x650
#define UMAC_MDF_ADDR 0x654
#define UMAC_MIB_CTRL 0x580
#define MIB_RESET_RX (1 << 0)
#define MIB_RESET_RUNT (1 << 1)
#define MIB_RESET_TX (1 << 2)

#define RBUF_CTRL 0x00
#define RBUF_64B_EN (1 << 0)
#define RBUF_ALIGN_2B (1 << 1)
#define RBUF_BAD_DIS (1 << 2)

#define RBUF_STATUS 0x0C
#define RBUF_STATUS_WOL (1 << 0)
#define RBUF_STATUS_MPD_INTR_ACTIVE (1 << 1)
#define RBUF_STATUS_ACPI_INTR_ACTIVE (1 << 2)

#define RBUF_CHK_CTRL 0x14
#define RBUF_RXCHK_EN (1 << 0)
#define RBUF_SKIP_FCS (1 << 4)

#define RBUF_ENERGY_CTRL 0x9c
#define RBUF_EEE_EN (1 << 0)
#define RBUF_PM_EN (1 << 1)

#define RBUF_TBUF_SIZE_CTRL 0xb4

#define RBUF_HFB_CTRL_V1 0x38
#define RBUF_HFB_FILTER_EN_SHIFT 16
#define RBUF_HFB_FILTER_EN_MASK 0xffff0000
#define RBUF_HFB_EN (1 << 0)
#define RBUF_HFB_256B (1 << 1)
#define RBUF_ACPI_EN (1 << 2)

#define RBUF_HFB_LEN_V1 0x3C
#define RBUF_FLTR_LEN_MASK 0xFF
#define RBUF_FLTR_LEN_SHIFT 8
#define TBUF_CTRL 0x00
#define TBUF_BP_MC 0x0C
#define TBUF_ENERGY_CTRL 0x14
#define TBUF_EEE_EN (1 << 0)
#define TBUF_PM_EN (1 << 1)

#define TBUF_CTRL_V1 0x80
#define TBUF_BP_MC_V1 0xA0

#define HFB_CTRL 0x00
#define HFB_FLT_ENABLE_V3PLUS 0x04
#define HFB_FLT_LEN_V2 0x04
#define HFB_FLT_LEN_V3PLUS 0x1C

#define INTRL2_CPU_STAT         0x00
#define INTRL2_CPU_SET          0x04
#define INTRL2_CPU_CLEAR        0x08
#define INTRL2_CPU_MASK_STATUS  0x0C
#define INTRL2_CPU_MASK_SET     0x10
#define INTRL2_CPU_MASK_CLEAR   0x14

/* SYS block offsets and register definitions */
#define SYS_REV_CTRL 0x00
#define SYS_PORT_CTRL 0x04
#define PORT_MODE_INT_EPHY 0
#define PORT_MODE_INT_GPHY 1
#define PORT_MODE_EXT_EPHY 2
#define PORT_MODE_EXT_GPHY 3
#define PORT_MODE_EXT_RVMII_25 (4 | BIT(4))
#define PORT_MODE_EXT_RVMII_50 4
#define LED_ACT_SOURCE_MAC (1 << 9)

#define SYS_RBUF_FLUSH_CTRL 0x08
#define SYS_TBUF_FLUSH_CTRL 0x0C
#define RBUF_FLUSH_CTRL_V1 0x04

/* Ext block register offsets and definitions */
#define EXT_EXT_PWR_MGMT 0x00
#define EXT_PWR_DOWN_BIAS (1 << 0)
#define EXT_PWR_DOWN_DLL (1 << 1)
#define EXT_PWR_DOWN_PHY (1 << 2)
#define EXT_PWR_DN_EN_LD (1 << 3)
#define EXT_ENERGY_DET (1 << 4)
#define EXT_IDDQ_FROM_PHY (1 << 5)
#define EXT_IDDQ_GLBL_PWR (1 << 7)
#define EXT_PHY_RESET (1 << 8)
#define EXT_ENERGY_DET_MASK (1 << 12)
#define EXT_PWR_DOWN_PHY_TX (1 << 16)
#define EXT_PWR_DOWN_PHY_RX (1 << 17)
#define EXT_PWR_DOWN_PHY_SD (1 << 18)
#define EXT_PWR_DOWN_PHY_RD (1 << 19)
#define EXT_PWR_DOWN_PHY_EN (1 << 20)

#define EXT_RGMII_OOB_CTRL 0x0C
#define RGMII_LINK (1 << 4)
#define OOB_DISABLE (1 << 5)
#define RGMII_MODE_EN (1 << 6)
#define ID_MODE_DIS (1 << 16)

#define EXT_GPHY_CTRL 0x1C
#define EXT_CFG_IDDQ_BIAS (1 << 0)
#define EXT_CFG_PWR_DOWN (1 << 1)
#define EXT_CK25_DIS (1 << 4)
#define EXT_GPHY_RESET (1 << 5)

/* INTRL2 instance 0 definitions */
#define UMAC_IRQ_SCB            (1 << 0)
#define UMAC_IRQ_EPHY           (1 << 1)
#define UMAC_IRQ_PHY_DET_R      (1 << 2)
#define UMAC_IRQ_PHY_DET_F      (1 << 3)
#define UMAC_IRQ_LINK_UP        (1 << 4)
#define UMAC_IRQ_LINK_DOWN      (1 << 5)
#define UMAC_IRQ_LINK_EVENT     (UMAC_IRQ_LINK_UP | UMAC_IRQ_LINK_DOWN)
#define UMAC_IRQ_UMAC           (1 << 6)
#define UMAC_IRQ_UMAC_TSV       (1 << 7)
#define UMAC_IRQ_TBUF_UNDERRUN  (1 << 8)
#define UMAC_IRQ_RBUF_OVERFLOW  (1 << 9)
#define UMAC_IRQ_HFB_SM         (1 << 10)
#define UMAC_IRQ_HFB_MM         (1 << 11)
#define UMAC_IRQ_MPD_R          (1 << 12)
#define UMAC_IRQ_RXDMA_MBDONE   (1 << 13)
#define UMAC_IRQ_RXDMA_PDONE    (1 << 14)
#define UMAC_IRQ_RXDMA_BDONE    (1 << 15)
#define UMAC_IRQ_RXDMA_DONE     UMAC_IRQ_RXDMA_MBDONE
#define UMAC_IRQ_TXDMA_MBDONE   (1 << 16)
#define UMAC_IRQ_TXDMA_PDONE    (1 << 17)
#define UMAC_IRQ_TXDMA_BDONE    (1 << 18)
#define UMAC_IRQ_TXDMA_DONE     UMAC_IRQ_TXDMA_MBDONE

#define RBUF_CTRL 0x00
#define RBUF_64B_EN (1 << 0)
#define RBUF_ALIGN_2B (1 << 1)
#define RBUF_BAD_DIS (1 << 2)

#define RBUF_STATUS 0x0C
#define RBUF_STATUS_WOL (1 << 0)
#define RBUF_STATUS_MPD_INTR_ACTIVE (1 << 1)
#define RBUF_STATUS_ACPI_INTR_ACTIVE (1 << 2)

#define RBUF_CHK_CTRL 0x14
#define RBUF_RXCHK_EN (1 << 0)
#define RBUF_SKIP_FCS (1 << 4)

#define RBUF_ENERGY_CTRL 0x9c
#define RBUF_EEE_EN (1 << 0)
#define RBUF_PM_EN (1 << 1)

#define RBUF_TBUF_SIZE_CTRL 0xb4

#define RBUF_HFB_CTRL_V1 0x38
#define RBUF_HFB_FILTER_EN_SHIFT 16
#define RBUF_HFB_FILTER_EN_MASK 0xffff0000
#define RBUF_HFB_EN (1 << 0)
#define RBUF_HFB_256B (1 << 1)
#define RBUF_ACPI_EN (1 << 2)

#define RBUF_HFB_LEN_V1 0x3C
#define RBUF_FLTR_LEN_MASK 0xFF
#define RBUF_FLTR_LEN_SHIFT 8

#define GENET_SYS_OFF           0x0000
#define GENET_GR_BRIDGE_OFF     0x0040
#define GENET_EXT_OFF           0x0080
#define GENET_INTRL2_0_OFF      0x0200
#define GENET_INTRL2_1_OFF      0x0240
#define GENET_RBUF_OFF          0x0300
#define GENET_UMAC_OFF          0x0800
#define GENET_HFB_OFF           0x8000
#define GENET_HFB_REG_OFF       0xFC00
#define GENET_TBUF_OFF          0x0600
#define GENET_RDMA_OFF          0x2000
#define GENET_TDMA_OFF          0x4000

/* DMA rings size */
#define DMA_RING_SIZE (0x40)
#define DMA_RINGS_SIZE (DMA_RING_SIZE * (DESC_INDEX + 1))

/* DMA registers common definitions */
#define DMA_RW_POINTER_MASK 0x1FF
#define DMA_P_INDEX_DISCARD_CNT_MASK 0xFFFF
#define DMA_P_INDEX_DISCARD_CNT_SHIFT 16
#define DMA_BUFFER_DONE_CNT_MASK 0xFFFF
#define DMA_BUFFER_DONE_CNT_SHIFT 16
#define DMA_P_INDEX_MASK 0xFFFF
#define DMA_C_INDEX_MASK 0xFFFF

/* DMA ring size register */
#define DMA_RING_SIZE_MASK 0xFFFF
#define DMA_RING_SIZE_SHIFT 16
#define DMA_RING_BUFFER_SIZE_MASK 0xFFFF

/* DMA interrupt threshold register */
#define DMA_INTR_THRESHOLD_MASK 0x01FF

/* DMA XON/XOFF register */
#define DMA_XON_THREHOLD_MASK 0xFFFF
#define DMA_XOFF_THRESHOLD_MASK 0xFFFF
#define DMA_XOFF_THRESHOLD_SHIFT 16
/* DMA flow period register */
#define DMA_FLOW_PERIOD_MASK 0xFFFF
#define DMA_MAX_PKT_SIZE_MASK 0xFFFF
#define DMA_MAX_PKT_SIZE_SHIFT 16

/* DMA control register */
#define DMA_EN (1 << 0)
#define DMA_RING_BUF_EN_SHIFT 0x01
#define DMA_RING_BUF_EN_MASK 0xFFFF
#define DMA_TSB_SWAP_EN (1 << 20)

/* DMA status register */
#define DMA_DISABLED (1 << 0)
#define DMA_DESC_RAM_INIT_BUSY (1 << 1)

/* DMA SCB burst size register */
#define DMA_SCB_BURST_SIZE_MASK 0x1F

/* DMA activity vector register */
#define DMA_ACTIVITY_VECTOR_MASK 0x1FFFF

/* DMA backpressure mask register */
#define DMA_BACKPRESSURE_MASK 0x1FFFF
#define DMA_PFC_ENABLE (1 << 31)

/* DMA backpressure status register */
#define DMA_BACKPRESSURE_STATUS_MASK 0x1FFFF

/* DMA override register */
#define DMA_LITTLE_ENDIAN_MODE (1 << 0)
#define DMA_REGISTER_MODE (1 << 1)

/* DMA timeout register */
#define DMA_TIMEOUT_MASK 0xFFFF
#define DMA_TIMEOUT_VAL 5000 /* micro seconds */

/* TDMA rate limiting control register */
#define DMA_RATE_LIMIT_EN_MASK 0xFFFF

/* TDMA arbitration control register */
#define DMA_ARBITER_MODE_MASK 0x03
#define DMA_RING_BUF_PRIORITY_MASK 0x1F
#define DMA_RING_BUF_PRIORITY_SHIFT 5
#define DMA_PRIO_REG_INDEX(q) ((q) / 6)
#define DMA_PRIO_REG_SHIFT(q) (((q) % 6) * DMA_RING_BUF_PRIORITY_SHIFT)
#define DMA_RATE_ADJ_MASK 0xFF

/* Tx/Rx Dma Descriptor common bits*/
#define DMA_BUFLENGTH_MASK 0x0fff
#define DMA_BUFLENGTH_SHIFT 16
#define DMA_OWN 0x8000
#define DMA_EOP 0x4000
#define DMA_SOP 0x2000
#define DMA_WRAP 0x1000
/* Tx specific Dma descriptor bits */
#define DMA_TX_UNDERRUN 0x0200
#define DMA_TX_APPEND_CRC 0x0040
#define DMA_TX_OW_CRC 0x0020
#define DMA_TX_DO_CSUM 0x0010
#define DMA_TX_QTAG_SHIFT 7

/* Rx Specific Dma descriptor bits */
#define DMA_RX_CHK_V3PLUS 0x8000
#define DMA_RX_CHK_V12 0x1000
#define DMA_RX_BRDCAST 0x0040
#define DMA_RX_MULT 0x0020
#define DMA_RX_LG 0x0010
#define DMA_RX_NO 0x0008
#define DMA_RX_RXER 0x0004
#define DMA_RX_CRC_ERROR 0x0002
#define DMA_RX_OV 0x0001
#define DMA_RX_FI_MASK 0x001F
#define DMA_RX_FI_SHIFT 0x0007
#define DMA_DESC_ALLOC_MASK 0x00FF

#define DMA_ARBITER_RR 0x00
#define DMA_ARBITER_WRR 0x01
#define DMA_ARBITER_SP 0x02

/* Maximum number of hardware queues, downsized if needed */
#define GENET_MAX_MQ_CNT 4

/* Default highest priority queue for multi queue support */
#define GENET_Q0_PRIORITY 0

#define GENET_Q16_RX_BD_CNT \
    (TOTAL_DESC - RX_QUEUES * RX_BDS_PER_Q)
#define GENET_Q16_TX_BD_CNT \
    (TOTAL_DESC - TX_QUEUES * TX_BDS_PER_Q)

#define WORDS_PER_BD        3
#define DMA_DESC_SIZE       (WORDS_PER_BD * sizeof(u32))

#define GENET_TDMA_REG_OFF  (GENET_TDMA_OFF + TOTAL_DESC * DMA_DESC_SIZE)
#define GENET_RDMA_REG_OFF  (GENET_RDMA_OFF + TOTAL_DESC * DMA_DESC_SIZE)

/* RDMA/TDMA ring registers and accessors
 * we merge the common fields and just prefix with T/D the registers
 * having different meaning depending on the direction
 */
enum dma_ring_reg {
  TDMA_READ_PTR         = 0,
  RDMA_WRITE_PTR        = TDMA_READ_PTR,
  TDMA_READ_PTR_HI,
  RDMA_WRITE_PTR_HI     = TDMA_READ_PTR_HI,
  TDMA_CONS_INDEX,
  RDMA_PROD_INDEX       = TDMA_CONS_INDEX,
  TDMA_PROD_INDEX,
  RDMA_CONS_INDEX       = TDMA_PROD_INDEX,
  DMA_RING_BUF_SIZE,
  DMA_START_ADDR,
  DMA_START_ADDR_HI,
  DMA_END_ADDR,
  DMA_END_ADDR_HI,
  DMA_MBUF_DONE_THRESH,
  TDMA_FLOW_PERIOD,
  RDMA_XON_XOFF_THRESH  = TDMA_FLOW_PERIOD,
  TDMA_WRITE_PTR,
  RDMA_READ_PTR         = TDMA_WRITE_PTR,
  TDMA_WRITE_PTR_HI,
  RDMA_READ_PTR_HI      = TDMA_WRITE_PTR_HI
};

/* RX/TX DMA register accessors */
enum dma_reg {
  DMA_RING_CFG = 0,
  DMA_CTRL,
  DMA_STATUS,
  DMA_SCB_BURST_SIZE,
  DMA_ARB_CTRL,
  DMA_PRIORITY_0,
  DMA_PRIORITY_1,
  DMA_PRIORITY_2,
  DMA_INDEX2RING_0,
  DMA_INDEX2RING_1,
  DMA_INDEX2RING_2,
  DMA_INDEX2RING_3,
  DMA_INDEX2RING_4,
  DMA_INDEX2RING_5,
  DMA_INDEX2RING_6,
  DMA_INDEX2RING_7,
  DMA_RING0_TIMEOUT,
  DMA_RING1_TIMEOUT,
  DMA_RING2_TIMEOUT,
  DMA_RING3_TIMEOUT,
  DMA_RING4_TIMEOUT,
  DMA_RING5_TIMEOUT,
  DMA_RING6_TIMEOUT,
  DMA_RING7_TIMEOUT,
  DMA_RING8_TIMEOUT,
  DMA_RING9_TIMEOUT,
  DMA_RING10_TIMEOUT,
  DMA_RING11_TIMEOUT,
  DMA_RING12_TIMEOUT,
  DMA_RING13_TIMEOUT,
  DMA_RING14_TIMEOUT,
  DMA_RING15_TIMEOUT,
  DMA_RING16_TIMEOUT,
};

static inline u32 readl(void *addr) {
  return *(volatile u32 *)addr;
}

static inline void writel(u32 val, void *addr) {
  *(volatile u32 *)addr = val;
}

static inline void dmadesc_set_length_status(void *d, u32 value) {
  writel(value, d + DMA_DESC_LENGTH_STATUS);
}

static inline u32 dmadesc_get_length_status(void *d) {
  return readl(d + DMA_DESC_LENGTH_STATUS);
}

#define lower_32_bits(n) ((u32)(n))

static inline void dmadesc_set_addr(void *d, dma_addr_t addr) {
  writel(lower_32_bits(addr), d + DMA_DESC_ADDRESS_LO);
}

/* Combined address + length/status setter */
static inline void dmadesc_set(void *d, dma_addr_t addr, u32 val) {
  dmadesc_set_addr(d, addr);
  dmadesc_set_length_status(d, val);
}

static inline dma_addr_t dmadesc_get_addr(void *d) {
  dma_addr_t addr;

  addr = readl(d + DMA_DESC_ADDRESS_LO);

  return addr;
}

/* Generic MII registers. */
#define MII_BMCR 0x00        /* Basic mode control register */
#define MII_BMSR 0x01        /* Basic mode status register  */
#define MII_PHYSID1 0x02     /* PHYS ID 1                   */
#define MII_PHYSID2 0x03     /* PHYS ID 2                   */
#define MII_ADVERTISE 0x04   /* Advertisement control reg   */
#define MII_LPA 0x05         /* Link partner ability reg    */
#define MII_EXPANSION 0x06   /* Expansion register          */
#define MII_CTRL1000 0x09    /* 1000BASE-T control          */
#define MII_STAT1000 0x0a    /* 1000BASE-T status           */
#define MII_MMD_CTRL 0x0d    /* MMD Access Control Register */
#define MII_MMD_DATA 0x0e    /* MMD Access Data Register */
#define MII_ESTATUS 0x0f     /* Extended Status             */
#define MII_DCOUNTER 0x12    /* Disconnect counter          */
#define MII_FCSCOUNTER 0x13  /* False carrier counter       */
#define MII_NWAYTEST 0x14    /* N-way auto-neg test reg     */
#define MII_RERRCOUNTER 0x15 /* Receive error counter       */
#define MII_SREVISION 0x16   /* Silicon revision            */
#define MII_RESV1 0x17       /* Reserved...                 */
#define MII_LBRERROR 0x18    /* Lpback, rx, bypass error    */
#define MII_PHYADDR 0x19     /* PHY address                 */
#define MII_RESV2 0x1a       /* Reserved...                 */
#define MII_TPISTATUS 0x1b   /* TPI status for 10mbps       */
#define MII_NCONFIG 0x1c     /* Network interface config    */

/* Basic mode control register. */
#define BMCR_RESV 0x003f      /* Unused...                   */
#define BMCR_SPEED1000 0x0040 /* MSB of Speed (1000)         */
#define BMCR_CTST 0x0080      /* Collision test              */
#define BMCR_FULLDPLX 0x0100  /* Full duplex                 */
#define BMCR_ANRESTART 0x0200 /* Auto negotiation restart    */
#define BMCR_ISOLATE 0x0400   /* Isolate data paths from MII */
#define BMCR_PDOWN 0x0800     /* Enable low power state      */
#define BMCR_ANENABLE 0x1000  /* Enable auto negotiation     */
#define BMCR_SPEED100 0x2000  /* Select 100Mbps              */
#define BMCR_LOOPBACK 0x4000  /* TXD loopback bits           */
#define BMCR_RESET 0x8000     /* Reset to default state      */
#define BMCR_SPEED10 0x0000   /* Select 10Mbps               */

/* Basic mode status register. */
#define BMSR_ERCAP 0x0001        /* Ext-reg capability          */
#define BMSR_JCD 0x0002          /* Jabber detected             */
#define BMSR_LSTATUS 0x0004      /* Link status                 */
#define BMSR_ANEGCAPABLE 0x0008  /* Able to do auto-negotiation */
#define BMSR_RFAULT 0x0010       /* Remote fault detected       */
#define BMSR_ANEGCOMPLETE 0x0020 /* Auto-negotiation complete   */
#define BMSR_RESV 0x00c0         /* Unused...                   */
#define BMSR_ESTATEN 0x0100      /* Extended Status in R15      */
#define BMSR_100HALF2 0x0200     /* Can do 100BASE-T2 HDX       */
#define BMSR_100FULL2 0x0400     /* Can do 100BASE-T2 FDX       */
#define BMSR_10HALF 0x0800       /* Can do 10mbps, half-duplex  */
#define BMSR_10FULL 0x1000       /* Can do 10mbps, full-duplex  */
#define BMSR_100HALF 0x2000      /* Can do 100mbps, half-duplex */
#define BMSR_100FULL 0x4000      /* Can do 100mbps, full-duplex */
#define BMSR_100BASE4 0x8000     /* Can do 100mbps, 4k packets  */

/* Advertisement control register. */
#define ADVERTISE_SLCT 0x001f          /* Selector bits               */
#define ADVERTISE_CSMA 0x0001          /* Only selector supported     */
#define ADVERTISE_10HALF 0x0020        /* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL 0x0020     /* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL 0x0040        /* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF 0x0040     /* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF 0x0080       /* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE 0x0080    /* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL 0x0100       /* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM 0x0100 /* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4 0x0200      /* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP 0x0400     /* Try for pause               */
#define ADVERTISE_PAUSE_ASYM 0x0800    /* Try for asymetric pause     */
#define ADVERTISE_RESV 0x1000          /* Unused...                   */
#define ADVERTISE_RFAULT 0x2000        /* Say we can detect faults    */
#define ADVERTISE_LPACK 0x4000         /* Ack link partners response  */
#define ADVERTISE_NPAGE 0x8000         /* Next page bit               */

#define ADVERTISE_FULL (ADVERTISE_100FULL | ADVERTISE_10FULL | \
                        ADVERTISE_CSMA)
#define ADVERTISE_ALL (ADVERTISE_10HALF | ADVERTISE_10FULL | \
                       ADVERTISE_100HALF | ADVERTISE_100FULL)

/* Link partner ability register. */
#define LPA_SLCT 0x001f            /* Same as advertise selector  */
#define LPA_10HALF 0x0020          /* Can do 10mbps half-duplex   */
#define LPA_1000XFULL 0x0020       /* Can do 1000BASE-X full-duplex */
#define LPA_10FULL 0x0040          /* Can do 10mbps full-duplex   */
#define LPA_1000XHALF 0x0040       /* Can do 1000BASE-X half-duplex */
#define LPA_100HALF 0x0080         /* Can do 100mbps half-duplex  */
#define LPA_1000XPAUSE 0x0080      /* Can do 1000BASE-X pause     */
#define LPA_100FULL 0x0100         /* Can do 100mbps full-duplex  */
#define LPA_1000XPAUSE_ASYM 0x0100 /* Can do 1000BASE-X pause asym*/
#define LPA_100BASE4 0x0200        /* Can do 100mbps 4k packets   */
#define LPA_PAUSE_CAP 0x0400       /* Can pause                   */
#define LPA_PAUSE_ASYM 0x0800      /* Can pause asymetrically     */
#define LPA_RESV 0x1000            /* Unused...                   */
#define LPA_RFAULT 0x2000          /* Link partner faulted        */
#define LPA_LPACK 0x4000           /* Link partner acked us       */
#define LPA_NPAGE 0x8000           /* Next page bit               */

#define LPA_DUPLEX (LPA_10FULL | LPA_100FULL)
#define LPA_100 (LPA_100FULL | LPA_100HALF | LPA_100BASE4)

/* Expansion register for auto-negotiation. */
#define EXPANSION_NWAY 0x0001        /* Can do N-way auto-nego      */
#define EXPANSION_LCWP 0x0002        /* Got new RX page code word   */
#define EXPANSION_ENABLENPAGE 0x0004 /* This enables npage words    */
#define EXPANSION_NPCAPABLE 0x0008   /* Link partner supports npage */
#define EXPANSION_MFAULTS 0x0010     /* Multiple faults detected    */
#define EXPANSION_RESV 0xffe0        /* Unused...                   */

#define ESTATUS_1000_XFULL 0x8000 /* Can do 1000BaseX Full       */
#define ESTATUS_1000_XHALF 0x4000 /* Can do 1000BaseX Half       */
#define ESTATUS_1000_TFULL 0x2000 /* Can do 1000BT Full          */
#define ESTATUS_1000_THALF 0x1000 /* Can do 1000BT Half          */

/* N-way test register. */
#define NWAYTEST_RESV1 0x00ff    /* Unused...                   */
#define NWAYTEST_LOOPBACK 0x0100 /* Enable loopback for N-way   */
#define NWAYTEST_RESV2 0xfe00    /* Unused...                   */

/* MAC and PHY tx_config_Reg[15:0] for SGMII in-band auto-negotiation.*/
#define ADVERTISE_SGMII 0x0001        /* MAC can do SGMII            */
#define LPA_SGMII 0x0001              /* PHY can do SGMII            */
#define LPA_SGMII_SPD_MASK 0x0c00     /* SGMII speed mask            */
#define LPA_SGMII_FULL_DUPLEX 0x1000  /* SGMII full duplex           */
#define LPA_SGMII_DPX_SPD_MASK 0x1C00 /* SGMII duplex and speed bits */
#define LPA_SGMII_10 0x0000           /* 10Mbps                      */
#define LPA_SGMII_10HALF 0x0000       /* Can do 10mbps half-duplex   */
#define LPA_SGMII_10FULL 0x1000       /* Can do 10mbps full-duplex   */
#define LPA_SGMII_100 0x0400          /* 100Mbps                     */
#define LPA_SGMII_100HALF 0x0400      /* Can do 100mbps half-duplex  */
#define LPA_SGMII_100FULL 0x1400      /* Can do 100mbps full-duplex  */
#define LPA_SGMII_1000 0x0800         /* 1000Mbps                    */
#define LPA_SGMII_1000HALF 0x0800     /* Can do 1000mbps half-duplex */
#define LPA_SGMII_1000FULL 0x1800     /* Can do 1000mbps full-duplex */
#define LPA_SGMII_LINK 0x8000         /* PHY link with copper-side partner */

/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL 0x0200    /* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF 0x0100    /* Advertise 1000BASE-T half duplex */
#define CTL1000_PREFER_MASTER 0x0400 /* prefer to operate as master */
#define CTL1000_AS_MASTER 0x0800
#define CTL1000_ENABLE_MASTER 0x1000

/* 1000BASE-T Status register */
#define LPA_1000MSFAIL 0x8000    /* Master/Slave resolution failure */
#define LPA_1000MSRES 0x4000     /* Master/Slave resolution status */
#define LPA_1000LOCALRXOK 0x2000 /* Link partner local receiver status */
#define LPA_1000REMRXOK 0x1000   /* Link partner remote receiver status */
#define LPA_1000FULL 0x0800      /* Link partner 1000BASE-T full duplex */
#define LPA_1000HALF 0x0400      /* Link partner 1000BASE-T half duplex */

/* Flow control flags */
#define FLOW_CTRL_TX 0x01
#define FLOW_CTRL_RX 0x02

/* MMD Access Control register fields */
#define MII_MMD_CTRL_DEVAD_MASK 0x1f   /* Mask MMD DEVAD*/
#define MII_MMD_CTRL_ADDR 0x0000       /* Address */
#define MII_MMD_CTRL_NOINCR 0x4000     /* no post increment */
#define MII_MMD_CTRL_INCR_RDWT 0x8000  /* post increment on reads & writes */
#define MII_MMD_CTRL_INCR_ON_WT 0xC000 /* post increment on writes only */

#endif
