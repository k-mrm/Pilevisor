#ifndef DRIVER_BCMGENET_H
#define DRIVER_BCMGENET_H

#include "types.h"

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

#define INTRL2_CPU_STAT         0x00
#define INTRL2_CPU_SET          0x04
#define INTRL2_CPU_CLEAR        0x08
#define INTRL2_CPU_MASK_STATUS  0x0C
#define INTRL2_CPU_MASK_SET     0x10
#define INTRL2_CPU_MASK_CLEAR   0x14

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

#define GENET_SYS_OFF           0x0000
#define GENET_GR_BRIDGE_OFF     0x0040
#define GENET_EXT_OFF           0x0080
#define GENET_INTRL2_0_OFF      0x0200
#define GENET_INTRL2_1_OFF      0x0240
#define GENET_RBUF_OFF          0x0300
#define GENET_UMAC_OFF          0x0800

#endif
