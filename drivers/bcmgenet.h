#ifndef DRIVER_BCMGENET_H
#define DRIVER_BCMGENET_H

#include "types.h"

#define BIT(n)          (1u << (n))

#define GENET_DESC_INDEX 16 // max. 16 priority queues and 1 default queue

struct bcmgenet_cb {
  physaddr_t bd_addr; // address of HW buffer descriptor
  u8 *buffer;         // pointer to frame buffer (DMA address)
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

#endif
