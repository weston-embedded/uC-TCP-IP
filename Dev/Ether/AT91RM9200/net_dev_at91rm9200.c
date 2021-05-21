/*
*********************************************************************************************************
*                                              uC/TCP-IP
*                                      The Embedded TCP/IP Suite
*
*                    Copyright 2004-2021 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                        NETWORK DEVICE DRIVER
*
*                                             AT91RM9200
*
* Filename : net_dev_at91rm9200.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_AT91RM9200_MODULE
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_at91rm9200.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Receive buffers MUST be aligned to a multiple of a 4 octet boundary.  However,
*               aligning to a 64-byte boundary may improve performance AND help to reduce RBNA
*               buffer errata.  Adjusting receive buffer alignment MUST be performed from within
*               net_dev_cfg.h.  Do not adjust the value below as it is used for configuration
*               checking only.
*
*           (2) Receive descriptors MUST be aligned to a multiple of a 4 octet boundary.  However,
*               aligning the first descriptor to a 64-byte boundary may reduce RCNA buffer errata.
*               The value specifiec below MAY be changed in order to adjust the receive descriptor
*               pool alignment.
*
*           (3) An undocumented errata seems to occur when the number of receive descriptors is
*               greater than 8.  The datasheet says the maximum is 1024.
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO                              10000    /* MII read write timeout.                              */
#define  TX_Q_NBR_MAX                                      2    /* Max nbr of q'd Tx frames allowed by HW.              */
#define  RX_BUF_ALIGN_OCTETS                               4    /* See Note #1.                                         */
#define  RX_DESC_ALIGN_OCTETS                             64    /* See Note #2.                                         */
#define  RX_DESC_NBR_MAX                                   8    /* See Note #3.                                         */


/*
*********************************************************************************************************
*                                      REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

                                                                /* -------- EMAC CONTROL REGISTER (CTL) BITS ----------------- */
#define  AT91RM9200_REG_EMAC_CTL_LB_LOW         DEF_BIT_NONE    /* 0: Optional. Loopback signal LOW. Default.                  */
#define  AT91RM9200_REG_EMAC_CTL_LB_HIGH        DEF_BIT_00      /* 1: Optional. Loopback signal HIGH.                          */
#define  AT91RM9200_REG_EMAC_CTL_LBL_DIS        DEF_BIT_NONE    /* 0: Loopback Local DISABLE. Default.                         */
#define  AT91RM9200_REG_EMAC_CTL_LBL_EN         DEF_BIT_01      /* 1: Loopback Local ENABLE.                                   */
#define  AT91RM9200_REG_EMAC_CTL_RE_DIS         DEF_BIT_NONE    /* 0: Receive DISABLE. Default.                                */
#define  AT91RM9200_REG_EMAC_CTL_RE_EN          DEF_BIT_02      /* 1: Receive ENABLE.                                          */
#define  AT91RM9200_REG_EMAC_CTL_TE_DIS         DEF_BIT_NONE    /* 0: Transmit DISABLE. Default.                               */
#define  AT91RM9200_REG_EMAC_CTL_TE_EN          DEF_BIT_03      /* 1: Transmit ENABLE.                                         */
#define  AT91RM9200_REG_EMAC_CTL_MPE_DIS        DEF_BIT_NONE    /* 0: Management Port DISABLE. Default.                        */
#define  AT91RM9200_REG_EMAC_CTL_MPE_EN         DEF_BIT_04      /* 1: Management Port ENABLE.                                  */
#define  AT91RM9200_REG_EMAC_CTL_CSR            DEF_BIT_05      /* Write to clear Statistics Registers.                        */
#define  AT91RM9200_REG_EMAC_CTL_ISR            DEF_BIT_06      /* Write to Increment Statistics Registers FOR TEST USE.       */
#define  AT91RM9200_REG_EMAC_CTL_WES_DIS        DEF_BIT_NONE    /* 0: Normal operation of Statistics Registers. Default.       */
#define  AT91RM9200_REG_EMAC_CTL_WES_EN         DEF_BIT_07      /* 1: Enable Write to Statistics Registers FOR TEST USE.       */
#define  AT91RM9200_REG_EMAC_CTL_BP_DIS         DEF_BIT_NONE    /* 0: Normal operation. Default.                               */
#define  AT91RM9200_REG_EMAC_CTL_BP_EN          DEF_BIT_08      /* 1: Force collision on all RX frames in HDX mode.            */

                                                                /* -------- EMAC CONFIGURATION REGISTER (CFG) BITS ----------- */
#define  AT91RM9200_REG_EMAC_CFG_SPD_10         DEF_BIT_NONE    /* 0: Set 10  MBps Speed (no functional effect). Default.      */
#define  AT91RM9200_REG_EMAC_CFG_SPD_100        DEF_BIT_00      /* 1: Set 100 MBps Speed (no functional effect).               */
#define  AT91RM9200_REG_EMAC_CFG_HD             DEF_BIT_NONE    /* 0: Half Duplex Operation. Default.                          */
#define  AT91RM9200_REG_EMAC_CFG_FD             DEF_BIT_01      /* 1: Full Duplex Operation.                                   */
#define  AT91RM9200_REG_EMAC_CFG_BR             DEF_BIT_NONE    /* Bit Rate (optional: write 0).                               */
#define  AT91RM9200_REG_EMAC_CFG_CAF_DIS        DEF_BIT_NONE    /* 0: Normal operation. Default.                               */
#define  AT91RM9200_REG_EMAC_CFG_CAF_EN         DEF_BIT_04      /* 1: All valid frames are received.                           */
#define  AT91RM9200_REG_EMAC_CFG_NBC_DIS        DEF_BIT_NONE    /* 0: Normal operation. Default.                               */
#define  AT91RM9200_REG_EMAC_CFG_NBC_EN         DEF_BIT_05      /* 1: No broadcast frames are received.                        */
#define  AT91RM9200_REG_EMAC_CFG_MTI_DIS        DEF_BIT_NONE    /* 0: Multicast Hash DISABLE. Default.                         */
#define  AT91RM9200_REG_EMAC_CFG_MTI_EN         DEF_BIT_06      /* 1: Multicast Hash ENABLE.                                   */
#define  AT91RM9200_REG_EMAC_CFG_UNI_DIS        DEF_BIT_NONE    /* 0: Unicast Hash DISABLE. Default.                           */
#define  AT91RM9200_REG_EMAC_CFG_UNI_EN         DEF_BIT_07      /* 1: Unicast Hash ENABLE.                                     */
#define  AT91RM9200_REG_EMAC_CFG_NORMAL         DEF_BIT_NONE    /* 0: Reception of 802.3   1518 byte (NORMAL) frames. Default. */
#define  AT91RM9200_REG_EMAC_CFG_BIG            DEF_BIT_08      /* 1: Reception of 802.3ac 1522 byte (VLAN) frames.            */
#define  AT91RM9200_REG_EMAC_CFG_EAE            DEF_BIT_NONE    /* External Address Match Enable (optional: write 0).          */
#define  AT91RM9200_REG_EMAC_CFG_CLK_8          DEF_BIT_NONE    /* 00: MDC Clock = MCK divided by 8.                           */
#define  AT91RM9200_REG_EMAC_CFG_CLK_16         (0x01 << 10)    /* 01: MDC Clock = MCK divided by 16.                          */
#define  AT91RM9200_REG_EMAC_CFG_CLK_32         (0x02 << 10)    /* 10: MDC Clock = MCK divided by 32. Default.                 */
#define  AT91RM9200_REG_EMAC_CFG_CLK_64         (0x03 << 10)    /* 11: MDC Clock = MCK divided by 64.                          */
#define  AT91RM9200_REG_EMAC_CFG_RTY_DIS        DEF_BIT_NONE    /* 0: Retry Test DISABLE. Default.                             */
#define  AT91RM9200_REG_EMAC_CFG_RTY_EN         DEF_BIT_12      /* 1: Retry Test ENABLE. (FOR TESTS ONLY).                     */
#define  AT91RM9200_REG_EMAC_CFG_RMII_DIS       DEF_BIT_NONE    /* 0:         MII Mode. Default.                               */
#define  AT91RM9200_REG_EMAC_CFG_RMII_EN        DEF_BIT_13      /* 1: Reduced MII Mode.                                        */

                                                                /* -------- EMAC STATUS REGISTER (SR) BITS ------------------- */
#define  AT91RM9200_REG_EMAC_SR_LINK            DEF_BIT_00      /* Reserved.                                                   */
#define  AT91RM9200_REG_EMAC_SR_MDIO_UNSET      DEF_BIT_NONE    /* 0: MDIO pin NOT set.                                        */
#define  AT91RM9200_REG_EMAC_SR_MDIO_SET        DEF_BIT_01      /* 1: MDIO pin     set. Default.                               */
#define  AT91RM9200_REG_EMAC_SR_IDLE            DEF_BIT_NONE    /* 0: PHY logic is idle.                                       */
#define  AT91RM9200_REG_EMAC_SR_RUNNING         DEF_BIT_02      /* 1: PHY logic running. Default.                              */

                                                                /* -------- EMAC TRANSMIT CONTROL REGISTER (TCR) BITS -------- */
#define  AT91RM9200_REG_EMAC_TCR_LEN(_x_)       (_x_ & 0x07FF)  /* Transmit Frame Length minus CRC len (if any).               */
#define  AT91RM9200_REG_EMAC_TCR_CRC            DEF_BIT_NONE    /* 0:        Append CRC on Transmit. Default.                  */
#define  AT91RM9200_REG_EMAC_TCR_NCRC           DEF_BIT_15      /* 1: Do NOT append CRC on Transmit.                           */

                                                                /* -------- EMAC TRANSMIT STATUS REGISTER (TSR) BITS --------- */
                                                                /* 0: No event.                                                */
#define  AT91RM9200_REG_EMAC_TSR_OVR            DEF_BIT_00      /* 1: Ethernet Transmit Buffer Overrun.                        */
#define  AT91RM9200_REG_EMAC_TSR_OVR_CLR        DEF_BIT_00      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_TSR_COL            DEF_BIT_01      /* 1: Collision Occurred.                                      */
#define  AT91RM9200_REG_EMAC_TSR_COL_CLR        DEF_BIT_01      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_TSR_RLE            DEF_BIT_02      /* 1: Retry Limit Exceeded.                                    */
#define  AT91RM9200_REG_EMAC_TSR_RLE_CLR        DEF_BIT_02      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_TSR_TXIDLE         DEF_BIT_03      /* 1: Transmitter Idle.                                        */
#define  AT91RM9200_REG_EMAC_TSR_TXIDLE_CLR     DEF_BIT_03      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_TSR_BNQ            DEF_BIT_04      /* 1: Ethernet Transmit Buffer not Queued.                     */
#define  AT91RM9200_REG_EMAC_TSR_BNQ_CLR        DEF_BIT_04      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_TSR_COMP           DEF_BIT_05      /* 1: Transmit Complete.                                       */
#define  AT91RM9200_REG_EMAC_TSR_COMP_CLR       DEF_BIT_05      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_TSR_UND            DEF_BIT_06      /* 1: Transmit Underrun.                                       */
#define  AT91RM9200_REG_EMAC_TSR_UND_CLR        DEF_BIT_06      /* 1: Clear event.                                             */

                                                                /* -------- EMAC RECEIVE STATUS REGISTER (RSR) BITS ---------- */
                                                                /* 0: No event.                                                */
#define  AT91RM9200_REG_EMAC_RSR_BNA            DEF_BIT_00      /* 1: Buffer Not Available.                                    */
#define  AT91RM9200_REG_EMAC_RSR_BNA_CLR        DEF_BIT_00      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_RSR_REC            DEF_BIT_01      /* 1: Frame Received.                                          */
#define  AT91RM9200_REG_EMAC_RSR_REC_CLR        DEF_BIT_01      /* 1: Clear event.                                             */
#define  AT91RM9200_REG_EMAC_RSR_OVR            DEF_BIT_02      /* 1: RX Overrun.                                              */
#define  AT91RM9200_REG_EMAC_RSR_OVR_CLR        DEF_BIT_02      /* 1: Clear event.                                             */

                                                                /* -------- EMAC INTERRUPT STATUS REGISTER  (ISR) BITS ------- */
                                                                /* NOTE: Registers are cleared on read.                        */

                                                                /* -------- EMAC INTERRUPT ENABLE REGISTER  (IER) BITS ------- */
                                                                /* -------- EMAC INTERRUPT DISABLE REGISTER (IDR) BITS ------- */
                                                                /* -------- EMAC INTERRUPT MASK REGISTER    (IMR) BITS ------- */
                                                                /* 0: The corresponding interrupt is enabled.                  */
                                                                /* 1: The corresponding interrupt is disabled.                 */
                                                                /* NOTE: The interrupt is disabled when the corresponding bit  */
                                                                /* is set. This is non-standard for AT91 products as generally */
                                                                /* a mask bit set enables the interrupt.                       */

#define  AT91RM9200_REG_EMAC_INT_DONE           DEF_BIT_00      /* PHY Management Complete.                                    */
#define  AT91RM9200_REG_EMAC_INT_RCOM           DEF_BIT_01      /* Receive Complete.                                           */
#define  AT91RM9200_REG_EMAC_INT_RBNA           DEF_BIT_02      /* Receive Buffer Not Available.                               */
#define  AT91RM9200_REG_EMAC_INT_TOVR           DEF_BIT_03      /* Transmit Buffer Overrun.                                    */
#define  AT91RM9200_REG_EMAC_INT_TUND           DEF_BIT_04      /* Transmit Buffer Underrun.                                   */
#define  AT91RM9200_REG_EMAC_INT_RTRY           DEF_BIT_05      /* Retry Limit.                                                */
#define  AT91RM9200_REG_EMAC_INT_TBRE           DEF_BIT_06      /* Transmit Buffer Register Empty.                             */
#define  AT91RM9200_REG_EMAC_INT_TCOM           DEF_BIT_07      /* Transmit Complete.                                          */
#define  AT91RM9200_REG_EMAC_INT_TIDLE          DEF_BIT_08      /* Transmit Idle.                                              */
#define  AT91RM9200_REG_EMAC_INT_LINK           DEF_BIT_09      /* Link Pin Changed Value (optional).                          */
#define  AT91RM9200_REG_EMAC_INT_ROVR           DEF_BIT_10      /* RX Overrun.                                                 */
#define  AT91RM9200_REG_EMAC_INT_ABT            DEF_BIT_11      /* Abort on DMA transfer.                                      */

                                                                        /* ---- EMAC PHY MAINTENANCE REGISTER (MAN) BITS ----- */
#define  AT91RM9200_REG_EMAC_MAN_DATA(_x_)      (_x_ & 0xFFFF)          /* PHY Read/Write Data Register.                       */
#define  AT91RM9200_REG_EMAC_MAN_CODE           (0x02 << 16)            /* IEEE Code. MUST have value of 10.                   */
#define  AT91RM9200_REG_EMAC_MAN_REGA(_x_)      ((_x_ & 0x1F) << 18)    /* Specifies the register in the PHY to access.        */
#define  AT91RM9200_REG_EMAC_MAN_PHYA(_x_)      ((_x_ & 0x1F) << 23)    /* PHY address. Normally 0.                            */
#define  AT91RM9200_REG_EMAC_MAN_WRITE          (0x01 << 28)            /* 01: Transfer is a write.                            */
#define  AT91RM9200_REG_EMAC_MAN_READ           (0x02 << 28)            /* 10: Transfer is a read.                             */
#define  AT91RM9200_REG_EMAC_MAN_HIGH           DEF_BIT_30              /* MUST be set to 1.                                   */
#define  AT91RM9200_REG_EMAC_MAN_LOW            DEF_BIT_NONE            /* MUST be set to 0.                                   */


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/

                                                                /* -------- WORD 0 - ADDRESS --------------------------------- */
#define  AT91RM9200_EMAC_RXBUF_ADD_MAC_OWNED    DEF_BIT_NONE    /* 0: EMAC owns buffer.                                        */
#define  AT91RM9200_EMAC_RXBUF_ADD_SW_OWNED     DEF_BIT_00      /* 1: Software owns the buffer.                                */
#define  AT91RM9200_EMAC_RXBUF_ADD_NO_WRAP      DEF_BIT_NONE    /* 0: This is NOT the last buffer in the ring.                 */
#define  AT91RM9200_EMAC_RXBUF_ADD_WRAP         DEF_BIT_01      /* 1: This is     the last buffer in the ring.                 */
#define  AT91RM9200_EMAC_RXBUF_ADD_BASE_MASK    0xFFFFFFFC      /* Base address of the receive buffer                          */

                                                                /* -------- WORD 1 - STATUS ---------------------------------- */
#define  AT91RM9200_EMAC_RXBUF_STAT_LEN_MASK    0x07FF          /* Length of frame including FCS.                              */
#define  AT91RM9200_EMAC_RXBUF_STAT_LOC4        DEF_BIT_23      /* Local address match (Specific address 4 match).             */
#define  AT91RM9200_EMAC_RXBUF_STAT_LOC3        DEF_BIT_24      /* Local address match (Specific address 3 match).             */
#define  AT91RM9200_EMAC_RXBUF_STAT_LOC2        DEF_BIT_25      /* Local address match (Specific address 2 match).             */
#define  AT91RM9200_EMAC_RXBUF_STAT_LOC1        DEF_BIT_26      /* Local address match (Specific address 1 match).             */
#define  AT91RM9200_EMAC_RXBUF_STAT_UNK         DEF_BIT_27      /* Unknown source address (reserved for future use).           */
#define  AT91RM9200_EMAC_RXBUF_STAT_EXT         DEF_BIT_28      /* External address (optional).                                */
#define  AT91RM9200_EMAC_RXBUF_STAT_UNI         DEF_BIT_29      /* Unicast hash match.                                         */
#define  AT91RM9200_EMAC_RXBUF_STAT_MULTI       DEF_BIT_30      /* Multicast hash match.                                       */
#define  AT91RM9200_EMAC_RXBUF_STAT_BCAST       DEF_BIT_31      /* Global all ones broadcast address detected.                 */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

                                                                /* --------------- DESCRIPTOR DATA TYPE --------------- */
typedef  struct  dev_desc {
    CPU_REG32    RxBufAddr;                                     /* Address of Rx buffer.                                */
    CPU_REG32    RxBufStatus;                                   /* Status  of Rx buffer.                                */
} DEV_DESC;                                                     /* See 'AT91RM9200 EMAC RECEIVE BUFFER DESCRIPTOR'.     */

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    MEM_POOL     RxDescPool;
    DEV_DESC    *RxBufDescPtrStart;
    DEV_DESC    *RxBufDescPtrCur;
    DEV_DESC    *RxBufDescPtrEnd;
    CPU_INT32U   TxBufPtr0;
    CPU_INT32U   TxBufPtr1;
    CPU_INT16U   RxNRdyCtr;
    CPU_INT08U   TxBufPtrNext;

#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U   MulticastAddrHashBitCtr[64];
#endif
} NET_DEV_DATA;


/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*
* Note(s) : (1) Device register definitions SHOULD NOT be absolute & SHOULD use the provided base address
*               within the device configuration structure (see 'net_dev_cfg.c'), as well as each device's
*               register definition structure in order to properly resolve register addresses at run-time
*               by mapping the device register definition structure onto an interface's base address.
*
*           (2) The device register definition structure MUST take into account appropriate register
*               offsets & apply reserved space as required.  The registers listed within the register
*               definition structure MUST reflect the exact ordering and data sizes illustrated in the
*               device user guide.
*
*           (3) Device registers SHOULD be declared as volatile variables so that compilers do NOT cache
*               register values but MUST perform the steps to read or write each register value for every
*               register read or write access.
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  ETH_CTL;
    CPU_REG32  ETH_CFG;
    CPU_REG32  ETH_SR;
    CPU_REG32  ETH_TAR;
    CPU_REG32  ETH_TCR;
    CPU_REG32  ETH_TSR;
    CPU_REG32  ETH_RBQP;
    CPU_REG32  RESERVED1;
    CPU_REG32  ETH_RSR;
    CPU_REG32  ETH_ISR;
    CPU_REG32  ETH_IER;
    CPU_REG32  ETH_IDR;
    CPU_REG32  ETH_IMR;
    CPU_REG32  ETH_MAN;
    CPU_REG32  RESERVED2;
    CPU_REG32  RESERVED3;
    CPU_REG32  ETH_FRA;
    CPU_REG32  ETH_SCOL;
    CPU_REG32  ETH_MCOL;
    CPU_REG32  ETH_OK;
    CPU_REG32  ETH_SEQE;
    CPU_REG32  ETH_ALE;
    CPU_REG32  ETH_DTE;
    CPU_REG32  ETH_LCOL;
    CPU_REG32  ETH_ECOL;
    CPU_REG32  ETH_TUE;
    CPU_REG32  ETH_CSE;
    CPU_REG32  ETH_DRFC;
    CPU_REG32  ETH_ROV;
    CPU_REG32  ETH_CDE;
    CPU_REG32  ETH_ELR;
    CPU_REG32  ETH_RJB;
    CPU_REG32  ETH_USF;
    CPU_REG32  ETH_SQEE;
    CPU_REG32  RESERVED4;
    CPU_REG32  RESERVED5;
    CPU_REG32  ETH_HSL;
    CPU_REG32  ETH_HSH;
    CPU_REG32  ETH_SA1L;
    CPU_REG32  ETH_SA1H;
    CPU_REG32  ETH_SA2L;
    CPU_REG32  ETH_SA2H;
    CPU_REG32  ETH_SA3L;
    CPU_REG32  ETH_SA3H;
    CPU_REG32  ETH_SA4L;
    CPU_REG32  ETH_SA4H;
} NET_DEV;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Device driver functions may be arbitrarily named.  However, it is recommended that device
*               driver functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_&&&.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*********************************************************************************************************
*/

                                                                        /* -------- FNCT'S COMMON TO ALL DEV'S -------- */
static  void  NetDev_Init               (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_Start              (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_Stop               (NET_IF             *pif,
                                         NET_ERR            *perr);


static  void  NetDev_Rx                 (NET_IF             *pif,
                                         CPU_INT08U        **p_data,
                                         CPU_INT16U         *size,
                                         NET_ERR            *perr);

static  void  NetDev_Tx                 (NET_IF             *pif,
                                         CPU_INT08U         *p_data,
                                         CPU_INT16U          size,
                                         NET_ERR            *perr);


static  void  NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *perr);

static  void  NetDev_AddrMulticastRemove(NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *perr);


static  void  NetDev_ISR_Handler        (NET_IF             *pif,
                                         NET_DEV_ISR_TYPE    type);


static  void  NetDev_IO_Ctrl            (NET_IF             *pif,
                                         CPU_INT08U          opt,
                                         void               *p_data,
                                         NET_ERR            *perr);


static  void  NetDev_MII_Rd             (NET_IF             *pif,
                                         CPU_INT08U          phy_addr,
                                         CPU_INT08U          reg_addr,
                                         CPU_INT16U         *p_data,
                                         NET_ERR            *perr);

static  void  NetDev_MII_Wr             (NET_IF             *pif,
                                         CPU_INT08U          phy_addr,
                                         CPU_INT08U          reg_addr,
                                         CPU_INT16U          data,
                                         NET_ERR            *perr);

                                                                        /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
static  void  NetDev_RxDescInit         (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_RxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_RxDescPtrCurInc    (NET_IF             *pif);


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      NETWORK DEVICE DRIVER API
*
* Note(s) : (1) Device driver API structures are used by applications during calls to NetIF_Add().  This
*               API structure allows higher layers to call specific device driver functions via function
*               pointer instead of by name.  This enables the network protocol suite to compile & operate
*               with multiple device drivers.
*
*           (2) In most cases, the API structure provided below SHOULD suffice for most device drivers
*               exactly as is with the exception that the API structure's name which MUST be unique &
*               SHOULD clearly identify the device being implemented.  For example, the Cirrus Logic
*               CS8900A Ethernet controller's API structure should be named NetDev_API_CS8900A[].
*
*               The API structure MUST also be externally declared in the device driver header file
*               ('net_dev_&&&.h') with the exact same name & type.
*********************************************************************************************************
*/
                                                                                    /* AT91RM9200 dev API fnct ptrs :   */
const  NET_DEV_API_ETHER  NetDev_API_AT91RM9200 = { NetDev_Init,                    /*   Init/add                       */
                                                    NetDev_Start,                   /*   Start                          */
                                                    NetDev_Stop,                    /*   Stop                           */
                                                    NetDev_Rx,                      /*   Rx                             */
                                                    NetDev_Tx,                      /*   Tx                             */
                                                    NetDev_AddrMulticastAdd,        /*   Multicast addr add             */
                                                    NetDev_AddrMulticastRemove,     /*   Multicast addr remove          */
                                                    NetDev_ISR_Handler,             /*   ISR handler                    */
                                                    NetDev_IO_Ctrl,                 /*   I/O ctrl                       */
                                                    NetDev_MII_Rd,                  /*   Phy reg rd                     */
                                                    NetDev_MII_Wr                   /*   Phy reg wr                     */
                                                  };

/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if    ((RX_BUF_ALIGN_OCTETS % 4) != 0)
#error  "RX_BUF_ALIGN_OCTETS  illegally #define'd in 'net_dev_at91rm9200.c'               "
#error  "                     [RX_BUF_ALIGN_OCTETS  MUST be equally divisible by 4 octets]"
#endif


/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Initialize required clock sources
*                   (b) Initialize external interrupt controller
*                   (c) Initialize external GPIO controller
*                   (d) Initialize driver state variables
*                   (e) Allocate memory for device DMA descriptors
*                   (f) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
*                       (2) Disable device interrupts
*                       (3) Disable device receiver and transmitter
*                       (4) Other necessary device initialization
*
*
* Argument(s) : pif         Pointer to the interface to initialize.
*               ---         Argument validated in NetIF_Add().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                No Error.
*                               NET_DEV_ERR_INIT                General initialization error.
*                               NET_BUF_ERR_POOL_MEM_ALLOC      Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : (2) The application developer SHOULD define NetDev_CfgClk() within net_bsp.c
*                   in order to properly enable clocks for specified network interface.  In
*                   some cases, a device may require clocks to be enabled for BOTH the device
*                   and accessory peripheral modules such as GPIO.  A call to this function
*                   MAY need to occur BEFORE any device register accesses are made.  In the
*                   event that a device does NOT require any external clocks to be enabled,
*                   it is recommended that the device driver still call the NetBSP fuction
*                   which may in turn leave the section for the specific interface number
*                   empty.
*
*               (3) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
*                   in order to properly enable interrupts on an external or CPU integrated
*                   interrupt controller.  Interrupt sources that are specific to the DEVICE
*                   hardware MUST NOT be initialized from within NetDev_CfgIntCtrl() and
*                   SHOULD only be modified from within the device driver.
*
*                   (a) External interrupt sources are cleared within the NetBSP first level
*                       ISR handler either before or after the call to the device driver ISR
*                       handler function.  The device driver ISR handler function SHOULD only
*                       clear the device specific interrupts and NOT external or CPU interrupt
*                       controller interrupt sources.
*
*               (4) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.  Micrium recommends defining and calling this NetBSP
*                   function even if no additional GPIO initialization is required.
*
*               (5) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (6) All device drivers that store instance specific data MUST declare all
*                   instance specific variables within the device data area defined above.
*
*               (7) Many DMA devices may generate only one interrupt for several ready receive
*                   descriptors.  In order to accommodate this, it is recommended that all DMA
*                   based drivers count the number of ready receive descriptors during the
*                   receive event and signal the receive task accordingly ONLY for those
*                   NEW descriptors which have not yet been accounted for.  Each time a
*                   descriptor is processed (or discarded) the count for acknowledged and
*                   unprocessed frames should be decremented by 1.  This function initializes the
*                   acknowledged receive descriptor count to 0.
*
*               (8) Descriptors are typically required to be contiguous in memory.  Allocation of
*                   descriptors MUST occur as a single contigous block of memory.  The driver may
*                   use pointer arithmetic to sub-divide and traverse the descriptor list.
*
*               (9) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbr_octets;
    CPU_INT32U          reg_val;
    CPU_INT08U          phy_bus_mode;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;


    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate nbr Rx desc.                                */
    if (pdev_cfg->RxDescNbr > RX_DESC_NBR_MAX) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets % RX_BUF_ALIGN_OCTETS) != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if ((pdev_cfg->RxBufIxOffset % CPU_WORD_SIZE_32) != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }
                                                                /* Validate Rx buf size.                                */

    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )pif->Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Limit max nbr Tx desc.                               */
    if (pdev_cfg->TxDescNbr != TX_Q_NBR_MAX) {                  /* Force driver operation w/ max nbr queued Tx frames.  */
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4u,
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;


                                                                /* ---------------------- INIT HW --------------------- */
                                                                /* Enable module clks (see Note #2).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbr_octets = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) pdev_cfg->MemAddr,            /* Ptr  to dedicated memory address for Rx descriptors. */
                   (CPU_SIZE_T  ) pdev_cfg->MemSize,            /* Size of dedicated memory.                            */
                   (CPU_SIZE_T  ) 1u,                           /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all  Rx descriptors. */
                   (CPU_SIZE_T  ) RX_DESC_ALIGN_OCTETS,         /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


                                                                /* --------------- DISABLE ALL INTERRUPTS ------------- */
    pdev->ETH_IDR = (AT91RM9200_REG_EMAC_INT_DONE     |
                     AT91RM9200_REG_EMAC_INT_RCOM     |
                     AT91RM9200_REG_EMAC_INT_RBNA     |
                     AT91RM9200_REG_EMAC_INT_TOVR     |
                     AT91RM9200_REG_EMAC_INT_TUND     |
                     AT91RM9200_REG_EMAC_INT_RTRY     |
                     AT91RM9200_REG_EMAC_INT_TBRE     |
                     AT91RM9200_REG_EMAC_INT_TCOM     |
                     AT91RM9200_REG_EMAC_INT_TIDLE    |
                     AT91RM9200_REG_EMAC_INT_LINK     |
                     AT91RM9200_REG_EMAC_INT_ROVR     |
                     AT91RM9200_REG_EMAC_INT_ABT);

                                                                /* --------------------- CLEAR ISR -------------------- */
    reg_val = pdev->ETH_ISR;
   (void)&reg_val;

                                                                /* ------------------- INIT CTL REG ------------------- */
    pdev->ETH_CTL = (AT91RM9200_REG_EMAC_CTL_LB_LOW   |         /* Enable Received & Transmitter, reset Statistics.     */
                     AT91RM9200_REG_EMAC_CTL_LBL_DIS  |
                     AT91RM9200_REG_EMAC_CTL_RE_DIS   |         /* Enabled at the end of initialization.                */
                     AT91RM9200_REG_EMAC_CTL_TE_DIS   |         /* Enabled at the end of initialization.                */
                     AT91RM9200_REG_EMAC_CTL_MPE_EN   |
                     AT91RM9200_REG_EMAC_CTL_CSR      |
                     AT91RM9200_REG_EMAC_CTL_WES_DIS  |
                     AT91RM9200_REG_EMAC_CTL_BP_DIS);

                                                                /* ------------------- INIT CFG REG ------------------- */
    pdev->ETH_CFG = (AT91RM9200_REG_EMAC_CFG_SPD_100  |
                     AT91RM9200_REG_EMAC_CFG_FD       |
                     AT91RM9200_REG_EMAC_CFG_CAF_DIS  |
                     AT91RM9200_REG_EMAC_CFG_NBC_DIS  |
                     AT91RM9200_REG_EMAC_CFG_MTI_EN   |
                     AT91RM9200_REG_EMAC_CFG_UNI_DIS  |
                     AT91RM9200_REG_EMAC_CFG_NORMAL   |
                     AT91RM9200_REG_EMAC_CFG_RTY_DIS  |
                     AT91RM9200_REG_EMAC_CFG_RMII_DIS);

    pdev->ETH_CFG &= ~AT91RM9200_REG_EMAC_CFG_CLK_64;           /* Clear MDIO clk bits.                                 */
                                                                /* Get   MDC  clk input freq.                           */
    reg_val = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    reg_val /= 2500000L;                                        /* Determine MCLK div for 2.5MHz MDC CLK.               */
    if (reg_val <= 8) {
        reg_val = AT91RM9200_REG_EMAC_CFG_CLK_8;
    } else if (reg_val <= 16) {
        reg_val = AT91RM9200_REG_EMAC_CFG_CLK_16;
    } else if (reg_val <= 32) {
        reg_val = AT91RM9200_REG_EMAC_CFG_CLK_32;
    } else {
        reg_val = AT91RM9200_REG_EMAC_CFG_CLK_64;
    }

    pdev->ETH_CFG |= reg_val;                                   /* Cfg best match MDC clk div.                          */


                                                                /* --------------- CLEAR TX STATUS BITS --------------- */
    pdev->ETH_TSR = (AT91RM9200_REG_EMAC_TSR_OVR_CLR  |
                     AT91RM9200_REG_EMAC_TSR_COL_CLR  |
                     AT91RM9200_REG_EMAC_TSR_RLE_CLR  |
                     AT91RM9200_REG_EMAC_TSR_COMP_CLR |
                     AT91RM9200_REG_EMAC_TSR_UND_CLR);

                                                                /* --------------- CLEAR RX STATUS BITS --------------- */
    pdev->ETH_RSR = (AT91RM9200_REG_EMAC_RSR_BNA_CLR |
                     AT91RM9200_REG_EMAC_RSR_REC_CLR |
                     AT91RM9200_REG_EMAC_RSR_OVR_CLR);


                                                                /* -------------- INITIALIZE DEV REGISTERS ------------- */
    if (pphy_cfg != (void *)0) {
        phy_bus_mode = pphy_cfg->BusMode;
        if (phy_bus_mode == NET_PHY_BUS_MODE_RMII) {
            pdev->ETH_CFG |=  AT91RM9200_REG_EMAC_CFG_RMII_EN;
        } else {
            pdev->ETH_CFG &= ~AT91RM9200_REG_EMAC_CFG_RMII_EN;
        }
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_Start()
*
* Description : (1) Start network interface hardware :
*
*                   (a) Initialize transmit semaphore count
*                   (b) Initialize hardware address registers
*                   (c) Initialize receive and transmit descriptors
*                   (d) Clear all pending interrupt sources
*                   (e) Enable supported interrupts
*                   (f) Enable the transmitter and receiver
*                   (g) Start / Enable DMA if required
*
*
* Argument(s) : pif         Pointer to the interface to start.
*               ---         Argument validated in NetIF_Start().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Ethernet device successfully started.
*
*                                                               - RETURNED BY NetIF_AddrHW_SetHandler() : --
*                               NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT         Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               - RETURNED BY NetIF_DevCfgTxRdySignal() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_OS_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
*                                                               ---- RETURNED BY NetDev_RxDescInit() : -----
*                          !!!!
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (2) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, DMA based interfaces may have several
*                   frames configured for transmission before blocking is required. The number
*                   of queued transmit frames depends on the number of configured transmit
*                   descriptors.
*
*               (3) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_Ether_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (4) The device HW address is set from one of the data sources below.  Each source is
*                   listed in the order of precedence :
*
*                   (a) Device Configuration Structure      Configure a valid HW address at compile time.
*
*                                                           Configure either "00:00:00:00:00:00" or an
*                                                               empty string, "", in order to configure
*                                                               the HW using using method #2b.
*
*                   (b) NetIF_AddrHW_SetHandler()           Call NetIF_AddrHW_SetHandler() if the HW
*                                                               address needs to be configured via run-
*                                                               time from a different data source.  E.g.
*                                                               Non-auto-loading memory such as I2C or
*                                                               SPI EEPROM  (see Note #2c).
*
*                   (c) Auto-Loading via EEPROM             If neither options #2a or #2b are used, the
*                                                               IF layer's copy of the HW address will be
*                                                               obtained from the HW address registers.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT32U          reg_val;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #2.                                         */
                            pdev_cfg->TxDescNbr,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }


                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #3 & #4.                                   */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #4a).                                  */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.    */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Else get  app-configured IF layer HW MAC address, ...*/
                                                                /* ... if any (see Note #4b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any (see Note #4c).           */
            hw_addr[0] = (pdev->ETH_SA1L >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->ETH_SA1L >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->ETH_SA1L >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->ETH_SA1L >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->ETH_SA1H >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->ETH_SA1H >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,    /* Configure IF layer to use automatically-loaded ...   */
                                    (CPU_INT08U *)&hw_addr[0],  /* ... HW MAC address.                                  */
                                    (CPU_INT08U  ) sizeof(hw_addr),
                                    (NET_ERR    *) perr);
            if (*perr != NET_IF_ERR_NONE) {                     /* No valid HW MAC address configured, return error.    */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        pdev->ETH_SA1L = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));

        pdev->ETH_SA1H = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* ------------------ CFG MULTICAST ------------------- */
#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

    pdev->ETH_HSL = 0;                                          /* Clear all multicast hash bits.                       */
    pdev->ETH_HSH = 0;                                          /* Clear all multicast hash bits.                       */


                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev_data->RxNRdyCtr    = 0;                                /* No pending frames to process (see Note #7).          */
    pdev_data->TxBufPtrNext = 0;                                /* Next transmit complete frame is frame 0.             */



                                                                /* -------------------- CFG INT'S --------------------- */
    reg_val = pdev->ETH_ISR;                                    /* Clear all pending int. sources.                      */
   (void)&reg_val;

    pdev->ETH_IER  = (AT91RM9200_REG_EMAC_INT_RCOM |            /* Enable 'Reception complete' interrupt.               */
                      AT91RM9200_REG_EMAC_INT_RBNA |            /* Enable 'Reception buffer not available' interrupt.   */
                      AT91RM9200_REG_EMAC_INT_ROVR |            /* Enable 'Receiver overrun' interrupt.                 */
                      AT91RM9200_REG_EMAC_INT_TCOM);            /* Enable 'Transmission complete' interrupt.            */

                                                                /* ------------------ ENABLE RX & TX ------------------ */
    pdev->ETH_CTL |= (AT91RM9200_REG_EMAC_CTL_TE_EN |           /* Enable the transmitter and receiver.                 */
                      AT91RM9200_REG_EMAC_CTL_RE_EN);


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Stop()
*
* Description : (1) Shutdown network interface hardware :
*
*                   (a) Disable the receiver and transmitter
*                   (b) Disable receive and transmit interrupts
*                   (c) Clear pending interrupt requests
*                   (d) Free ALL receive descriptors (Return ownership to hardware)
*                   (e) Deallocate ALL transmit buffers
*
* Argument(s) : pif         Pointer to the interface to stop.
*               ---         Argument validated in NetIF_Stop().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Ethernet device successfully stopped.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Stop() via 'pdev_api->Stop()'.
*
* Note(s)     : (2) (a) (1) It is recommended that a device driver should only post all currently-used,
*                           i.e. not-fully-transmitted, transmit buffers to the network interface transmit
*                           deallocation queue.
*
*                       (2) However, a driver MAY attempt to post all queued &/or transmitted buffers.
*                           The network interface transmit deallocation task will silently ignore any
*                           unknown or duplicate transmit buffers.  This allows device drivers to
*                           indiscriminately & easily post all transmit buffers without determining
*                           which buffers have NOT yet been transmitted.
*
*                   (b) (1) Device drivers should assume that the network interface transmit deallocation
*                           queue is large enough to post all currently-used transmit buffers.
*
*                       (2) If the transmit deallocation queue is NOT large enough to post all transmit
*                           buffers, some transmit buffers may/will be leaked/lost.
*
*               (3) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U         *p_txdata;
    CPU_INT32U          reg_val;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    pdev->ETH_CTL &= ~(AT91RM9200_REG_EMAC_CTL_TE_EN  |         /* Disable the transmitter and receiver.                */
                       AT91RM9200_REG_EMAC_CTL_RE_EN);

                                                                /* ------------------ DISABLE INT'S ------------------- */
    pdev->ETH_IDR  =  (AT91RM9200_REG_EMAC_INT_RCOM   |         /* Disable 'Reception complete' interrupt.              */
                       AT91RM9200_REG_EMAC_INT_RBNA   |         /* Disable 'Reception buffer not available' interrupt.  */
                       AT91RM9200_REG_EMAC_INT_ROVR   |         /* Disable 'Receiver overrun' interrupt.                */
                       AT91RM9200_REG_EMAC_INT_TCOM);           /* Disable 'Transmission complete' interrupt.           */

    reg_val = pdev->ETH_ISR;                                    /* Clear all pending int. sources.                      */
   (void)&reg_val;

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    p_txdata = (CPU_INT08U *)pdev_data->TxBufPtr0;
    if (p_txdata != (CPU_INT08U *)0) {                          /* If NOT yet  tx'd, ...                                */
        NetIF_TxDeallocTaskPost(p_txdata, &err);                /* ... dealloc tx buf (see Note #2a1).                  */
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdev_data->TxBufPtr0 = (CPU_INT32U)0;
    }

    p_txdata = (CPU_INT08U *)pdev_data->TxBufPtr1;
    if (p_txdata != (CPU_INT08U *)0) {                          /* If NOT yet  tx'd, ...                                */
        NetIF_TxDeallocTaskPost(p_txdata, &err);                /* ... dealloc tx buf (see Note #2a1).                  */
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdev_data->TxBufPtr1 = (CPU_INT32U)0;
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*
*                   (a) Determine which receive descriptor caused the interrupt
*                   (b) Obtain pointer to data area to replace existing data area
*                   (c) Reconfigure descriptor with pointer to new data area
*                   (d) Set return values.  Pointer to received data area and size
*                   (e) Update current receive descriptor pointer
*                   (f) Increment counters.
*
*
* Argument(s) : pif         Pointer to the interface to receive packet(s).
*               ---         Argument validated in NetIF_RxHandler().
*
*               p_data      Pointer to pointer to received DMA data area. The received data
*                           area address should be returned to the stack by dereferencing
*                           p_data as *p_data = (address of receive data area).
*
*               size        Pointer to size. The number of bytes received should be returned
*                           to the stack by dereferencing size as *size = (number of bytes).
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                No Error
*                               NET_DEV_ERR_RX                  Generic Rx error.
*                               NET_DEV_ERR_INVALID_SIZE        Invalid Rx frame size.
*                               NET_BUF error codes             Potential NET_BUF error codes
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pdev_api->Rx()'.
*
* Note(s)     : (2) All functions that require device register access MUST obtain reference to the
*                   device hardware register space PRIOR to attempting to access any registers.  Register
*                   definitions SHOULD NOT be absolute and SHOULD use the provided base address within the
*                   device configuration structure, as well as the device register definition structure in
*                   order to properly resolve register addresses during run-time.
*
*               (3) If a receive error occurs and the descriptor is invalid then the function SHOULD return
*                   0 for the size, a NULL pointer to the data area AND an error equal to NET_DEV_ERR_RX.
*
*                   (a) If the next expected ready / valid descriptor is NOT owned by software, then there
*                       is descriptor pointer corruption and the driver should NOT increment the current
*                       receive descriptor pointer.
*                   (b) If the descriptor IS valid, but an error is indicated within the descriptor
*                       status bits, or length field, then the driver MUST increment the current receive
*                       descriptor pointer and discard the received frame.
*                   (c) If a new data area is unavailable, the driver MUST increment the current receive
*                       descriptor pointer and discard the received frame.  This will invoke the DMA to
*                       re-use the existing configured data area.
*
*               (4) Receive descriptors' variables MUST ALWAYS be accessed exclusively in critical sections.
*
*                   (a) However, since 32-bit access is atomically performed in a single instruction for
*                       both ARM & Thumb modes, it is NOT necessary to access receive descriptor variables
*                       in critical sections.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    CPU_INT08U         *pbuf_new;
    CPU_ADDR            pbuf_addr;
    CPU_ADDR            data_addr;
    CPU_INT32U          buf_owned;
    CPU_INT16S          len;
    NET_BUF_SIZE        ix_offset;
    CPU_SR_ALLOC();


                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ---------- CHECK FOR DESCRIPTOR RX ERRORS ---------- */
    pdesc     =  pdev_data->RxBufDescPtrCur;                    /* Obtain ptr to next ready descriptor.                 */
    buf_owned = (CPU_INT32U)(pdesc->RxBufAddr & AT91RM9200_EMAC_RXBUF_ADD_SW_OWNED);
    if (buf_owned == AT91RM9200_EMAC_RXBUF_ADD_MAC_OWNED) {
        CPU_CRITICAL_ENTER();
        if (pdev_data->RxNRdyCtr > 0) {                         /* Decrement spurious frame.                            */
            pdev_data->RxNRdyCtr--;
        }
        CPU_CRITICAL_EXIT();
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

                                                                /* --------------- OBTAIN FRAME LENGTH ---------------- */
    len  = (CPU_INT16S)(pdesc->RxBufStatus & AT91RM9200_EMAC_RXBUF_STAT_LEN_MASK);
    len -=    NET_IF_ETHER_FRAME_CRC_SIZE;
    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {                    /* If frame is a runt.                                  */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame.                              */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }


                                                                /* --------- OBTAIN PTR TO NEW DMA DATA AREA ---------- */
                                                                /* Request an empty buffer.                             */
    pbuf_new = NetBuf_GetDataPtr((NET_IF        *) pif,
                                 (NET_TRANSACTION) NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   ) NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   ) NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)&ix_offset,

                                 (NET_BUF_SIZE  *) 0,
                                 (NET_BUF_TYPE  *) 0,
                                 (NET_ERR       *) perr);

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer (see Note #3c).            */
         NetDev_RxDescPtrCurInc(pif);                           /* Free the current descriptor.                         */
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
         return;
    }

   *size      = (CPU_INT16U)len;                                /* Return the size of the received frame.               */


    ix_offset = (NET_BUF_SIZE) 0u;
    data_addr = (CPU_ADDR    )(pdesc->RxBufAddr & AT91RM9200_EMAC_RXBUF_ADD_BASE_MASK);
   *p_data    = (CPU_INT08U *) data_addr - ix_offset;           /* Return a pointer to the newly received data area.    */

    pbuf_addr = (CPU_ADDR    ) pbuf_new  + ix_offset;
    if (pdesc == pdev_data->RxBufDescPtrEnd) {                  /* If at end of descriptor list, ...                    */
                                                                /* ... set WRAP bit for last buffer in list.            */
        pbuf_addr    |= (CPU_ADDR )AT91RM9200_EMAC_RXBUF_ADD_WRAP;
    }
                                                                /* Keep as software owned until desc freed.             */
    pbuf_addr        |= (CPU_ADDR )AT91RM9200_EMAC_RXBUF_ADD_SW_OWNED;
    pdesc->RxBufAddr  = (CPU_REG32)pbuf_addr;                   /* Set desc data addr to new data area (see Note #4).   */


    NetDev_RxDescPtrCurInc(pif);                                /* Free the current descriptor.                         */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Check if the transmitter is ready.
*                   (b) Configure the next transmit descriptor for pointer to data and data size.
*                   (c) Issue the transmit command.
*                   (d) Increment pointer to next transmit descriptor
*
* Argument(s) : pif         Pointer to the interface to transmit packet(s).
*               ---         Argument validated in NetIF_TxHandler().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                No Error
*                               NET_DEV_ERR_TX_BUSY             No Tx descriptors available
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pdev_api->Tx()'.
*
* Note(s)     : (2) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (3) Software MUST track the address of the frame which is expected to generate
*                   the next transmit complete interrupt.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT32U          reg_val;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------- CHECK FOR DEVICE ERRORS -------------- */
    reg_val = pdev->ETH_TSR;
    if ((reg_val & AT91RM9200_REG_EMAC_TSR_BNQ) == 0) {
       *perr = NET_DEV_ERR_TX_BUSY;
        return;
    }

    pdev->ETH_TSR = (AT91RM9200_REG_EMAC_TSR_OVR_CLR  |         /* Clear all transmitter errors.                       */
                     AT91RM9200_REG_EMAC_TSR_COL_CLR  |
                     AT91RM9200_REG_EMAC_TSR_RLE_CLR  |
                     AT91RM9200_REG_EMAC_TSR_COMP_CLR |
                     AT91RM9200_REG_EMAC_TSR_UND_CLR);


                                                                /* ------------------ PREPARE TO TX ------------------- */
    pdev->ETH_TAR = (CPU_INT32U)p_data;                         /* Configure Tx frame address.                          */
    pdev->ETH_TCR =  size;                                      /* Configure Tx frame size.                             */

    CPU_CRITICAL_ENTER();                                       /* This routine reads & alters shared data. Dis. Int's. */
    if (pdev_data->TxBufPtrNext == 0) {                         /* Track Tx data addr for Tx Int. (see Note #3).        */
        pdev_data->TxBufPtr0 = (CPU_INT32U)p_data;
    } else {
        pdev_data->TxBufPtr1 = (CPU_INT32U)p_data;
    }
    CPU_CRITICAL_EXIT();                                        /* Enable interrupts.                                   */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetDev_AddrMulticastAdd()
*
* Description : Configure hardware address filtering to accept specified hardware address.
*
* Argument(s) : pif         Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               paddr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully configured.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : (1) The device is capable of the following multicast address filtering techniques :
*                       (a) Perfect        filtering of ONE multicast address.
*                       (b) Imperfect hash filtering of 64  multicast addresses.
*                       (c) Promiscous non filtering.  Disable filtering of all received frames.
*
*               (2) This function implements the filtering mechanism described in 1b.
*********************************************************************************************************
*/
static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT08U          bit_val;
    CPU_INT08U          bit_nbr;
    CPU_INT08U          bit;
    CPU_INT08U          octet_nbr;
    CPU_INT08U          octet;
    CPU_INT08U          hash;
    CPU_INT08U          i;
    CPU_INT08U          j;
    CPU_INT08U          reg_bit;
    CPU_INT08U          reg_sel;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------------- OBTAIN HASH -------------------- */
    hash = 0;
    for (i = 0; i < 6; i++) {                                   /* For row in the table above (see Note #3).            */
        bit_val = 0;                                            /* Set initial xor val for cur table row (see Note #3). */
        for (j = 0; j < 8; j++) {                               /* For each bit in each octet.                          */
            bit_nbr    = (j * 6) + i;                           /* Determine bit position in stream, 0-47.              */
            octet_nbr  =  bit_nbr / 8;                          /* Determine which octet the bit belongs within.        */
            octet      =  paddr_hw[octet_nbr];                  /* Obtain the octet.                                    */
            bit        =  octet & (1 << (bit_nbr % 8));         /* Check if the bit is set within the octet.            */
            bit_val   ^= (bit > 0) ? 1 : 0;                     /* Determine table row xor contribution to hash.        */
        }

        hash |= (bit_val << i);                                 /* Add row contribution to final hash.                  */
     }

    reg_bit = (hash >> 0) & 0x1F;                               /* Determine bit position within hash reg to cfg.       */
    reg_sel = (hash >> 5) & 0x01;                               /* Determine which hash reg to cfg.                     */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    if (reg_sel == 0) {                                         /* Set multicast hash reg bit.                          */
        pdev->ETH_HSL |= (1 << reg_bit);
    } else {
        pdev->ETH_HSH |= (1 << reg_bit);
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
#endif

   (void)&addr_hw_len;                                          /* Prevent 'variable unused' compiler warning.          */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetDev_AddrMulticastRemove()
*
* Description : Configure hardware address filtering to reject specified hardware address.
*
* Argument(s) : pif         Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               paddr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully removed.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastRemove()'.
*
* Note(s)     : (1) The device is capable of the following multicast address filtering techniques :
*                       (a) Perfect        filtering of 4 multicast addresses.
*                       (b) Imperfect hash filtering of 64  multicast addresses.
*                       (c) Promiscous non filtering.  Disable filtering of all received frames.
*
*               (2) This function implements the filtering mechanism described in 1b.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT08U          bit_val;
    CPU_INT08U          bit_nbr;
    CPU_INT08U          bit;
    CPU_INT08U          octet_nbr;
    CPU_INT08U          octet;
    CPU_INT08U          hash;
    CPU_INT08U          i;
    CPU_INT08U          j;
    CPU_INT08U          reg_bit;
    CPU_INT08U          reg_sel;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------------- OBTAIN HASH -------------------- */
    hash = 0;
    for (i = 0; i < 6; i++) {                                   /* For row in the table above (see Note #3).            */
        bit_val = 0;                                            /* Set initial xor val for cur table row (see Note #3). */
        for (j = 0; j < 8; j++) {                               /* For each bit in each octet.                          */
            bit_nbr    = (j * 6) + i;                           /* Determine bit position in stream, 0-47.              */
            octet_nbr  =  bit_nbr / 8;                          /* Determine which octet the bit belongs within.        */
            octet      =  paddr_hw[octet_nbr];                  /* Obtain the octet.                                    */
            bit        =  octet & (1 << (bit_nbr % 8));         /* Check if the bit is set within the octet.            */
            bit_val   ^= (bit > 0) ? 1 : 0;                     /* Determine table row xor contribution to hash.        */
        }

        hash |= (bit_val << i);                                 /* Add row contribution to final hash.                  */
     }

    reg_bit = (hash >> 0) & 0x1F;                               /* Determine bit position within hash reg to cfg.       */
    reg_sel = (hash >> 5) & 0x01;                               /* Determine which hash reg to cfg.                     */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1) {                                 /* If hash bit reference ctr not zero.                  */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0;                                        /* Zero hash bit references remaining.                  */

    if (reg_sel == 0) {                                         /* Clr multicast hash reg bit.                          */
        pdev->ETH_HSL &= ~(1 << reg_bit);
    } else {
        pdev->ETH_HSH &= ~(1 << reg_bit);
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
#endif

   (void)&addr_hw_len;                                          /* Prevent 'variable unused' compiler warning.          */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_IO_Ctrl()
*
* Description : This function provides a mechanism for the Phy driver to update the MAC link
*               and duplex settings, as well as a method for the application and link state
*               timer to obtain the current link status.  Additional user specified driver
*               functionality MAY be added if necessary.
*
* Argument(s) : pif         Pointer to the interface to handle I/O controls.
*               ---         Argument validated in NetIF_IO_Ctrl().
*
*               opt         Option code representing desired function to perform. The network protocol suite
*                           specifies the option codes below. Additional option codes may be defined by the
*                           driver developer in the driver's header file.
*                               NET_IF_IO_CTRL_LINK_STATE_GET
*                               NET_IF_IO_CTRL_LINK_STATE_UPDATE
*
*                           Driver defined operation codes MUST be defined starting from 20 or higher
*                           to prevent clashing with the pre-defined operation code types. See the
*                           device driver header file for more details.
*
*               data        Pointer to optional data for either sending or receiving additional function
*                           arguments or return data.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid option number specified.
*                               NET_ERR_FAULT_NULL_FNCT             Null interface function pointer encountered.
*
*                               NET_DEV_ERR_NONE                    IO Ctrl operation completed successfully.
*                               NET_DEV_ERR_NULL_PTR                Null argument pointer passed.
*
*                               NET_PHY_ERR_NULL_PTR                Pointer argument(s) passed NULL pointer(s).
*                               NET_PHY_ERR_TIMEOUT_RESET           Phy reset          time-out.
*                               NET_PHY_ERR_TIMEOUT_AUTO_NEG        Auto-Negotiation   time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_RD          Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR          Phy register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IO_CtrlHandler() via 'pdev_api->IO_Ctrl()',
*               NetPhy_LinkStateGet()        via 'pdev_api->IO_Ctrl()'.
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *pif,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER   *pdev_cfg;
    NET_DEV             *pdev;
    NET_DEV_LINK_ETHER  *plink_state;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* ----------- PERFORM SPECIFIED OPERATION ------------ */
    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;
             if (plink_state == (NET_DEV_LINK_ETHER *)0) {
                *perr = NET_DEV_ERR_NULL_PTR;
                 return;
             }

             pphy_api = (NET_PHY_API_ETHER *)pif->Ext_API;

             if (pphy_api == (void *)0) {
                *perr = NET_ERR_FAULT_NULL_FNCT;
                 return;
             }
             pphy_api->LinkStateGet(pif, plink_state, perr);
             if (*perr != NET_PHY_ERR_NONE) {
                  return;
             }
            *perr = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;

             duplex = NET_PHY_DUPLEX_UNKNOWN;
             if (plink_state->Duplex != duplex) {
                 switch (plink_state->Duplex) {
                    case NET_PHY_DUPLEX_FULL:
                         pdev->ETH_CFG |=  AT91RM9200_REG_EMAC_CFG_FD;
                         break;

                    case NET_PHY_DUPLEX_HALF:
                         pdev->ETH_CFG &= ~AT91RM9200_REG_EMAC_CFG_FD;
                         break;

                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {
                    case NET_PHY_SPD_10:
                         pdev->ETH_CFG &= ~AT91RM9200_REG_EMAC_CFG_SPD_100;
                         break;

                    case NET_PHY_SPD_100:
                         pdev->ETH_CFG |=  AT91RM9200_REG_EMAC_CFG_SPD_100;
                         break;

                    case NET_PHY_SPD_1000:
                         break;

                    default:
                         break;
                 }
             }

            *perr = NET_DEV_ERR_NONE;
             break;


        default:
            *perr = NET_IF_ERR_INVALID_IO_CTRL_OPT;
             break;
    }
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Rd()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface to read the MII bus.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NULL_PTR        Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_NONE            No error.
*                           NET_PHY_ERR_TIMEOUT_REG_RD  Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various Phy functions.
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_MII_Rd (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U  *p_data,
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          timeout;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */


    timeout  =  MII_REG_RD_WR_TO;
    while (timeout > 0) {
        timeout--;
        if ((pdev->ETH_SR & AT91RM9200_REG_EMAC_SR_RUNNING) > 0) {
            break;
        }
    }

    if (timeout == 0) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    pdev->ETH_MAN = (AT91RM9200_REG_EMAC_MAN_LOW            |
                     AT91RM9200_REG_EMAC_MAN_HIGH           |
                     AT91RM9200_REG_EMAC_MAN_READ           |
                     AT91RM9200_REG_EMAC_MAN_PHYA(phy_addr) |
                     AT91RM9200_REG_EMAC_MAN_REGA(reg_addr) |
                     AT91RM9200_REG_EMAC_MAN_CODE           |
                     AT91RM9200_REG_EMAC_MAN_DATA(0));


    timeout = MII_REG_RD_WR_TO;
    while (timeout > 0) {
        timeout--;
        if ((pdev->ETH_SR & AT91RM9200_REG_EMAC_SR_RUNNING) > 0) {
            break;
        }
    }

    if (timeout == 0) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

   *p_data  = pdev->ETH_MAN & 0xFFFF;
   *perr    = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface to write to the MII bus.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               data        Data to write to the specified Phy register.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                           NET_PHY_ERR_NONE            No error.
*                           NET_PHY_ERR_TIMEOUT_REG_WR  Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various Phy functions.
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_MII_Wr (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   data,
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          timeout;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */


    timeout  =  MII_REG_RD_WR_TO;
    while (timeout > 0) {
        timeout--;
        if ((pdev->ETH_SR & AT91RM9200_REG_EMAC_SR_RUNNING) > 0) {
            break;
        }
    }

    if (timeout == 0) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

    pdev->ETH_MAN = (AT91RM9200_REG_EMAC_MAN_LOW            |
                     AT91RM9200_REG_EMAC_MAN_HIGH           |
                     AT91RM9200_REG_EMAC_MAN_WRITE          |
                     AT91RM9200_REG_EMAC_MAN_PHYA(phy_addr) |
                     AT91RM9200_REG_EMAC_MAN_REGA(reg_addr) |
                     AT91RM9200_REG_EMAC_MAN_CODE           |
                     AT91RM9200_REG_EMAC_MAN_DATA(data));

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetDev_RxDescInit()
*
* Description : (1) This function initializes the Rx descriptor list for the specified interface :
*
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Rx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
*                   (d) Initialize hardware registers
*
*
* Argument(s) : pif         Pointer to the interface to initialize receive descriptors.
*               ---         Argument validated in NetDev_Start().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE            No error
*                               NET_IF_MGR_ERR_nnnn         Various Network Interface management error codes
*                               NET_BUF_ERR_nnn             Various Network buffer error codes
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start().
*
* Note(s)     : (2) Memory allocation for the descriptors MUST be performed BEFORE calling this
*                   function. This ensures that multiple calls to this function do NOT allocate
*                   additional memory to the interface and that the Rx descriptors may be safely
*                   re-initialized by calling this function.
*
*               (3) All Rx descriptors are allocated as ONE memory block.  This removes the
*                   necessity to ensure that descriptor blocks are returned to the descriptor
*                   pool in the opposite order in which they were allocated; doing so would
*                   ensure that each memory block address was contiguous to the one before
*                   and after it.  If the descriptors are NOT contiguous, then software
*                   MUST NOT assign a pointer to the pool start address and use pointer
*                   arithmetic to navigate the descriptor list.  Since pointer arithmetic
*                   is a convenient way to navigate the descriptor list, ONE block is allocated
*                   and the driver uses pointer arithmetic to slice the block into descriptor
*                   sized units.
*********************************************************************************************************
*/

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    MEM_POOL           *pmem_pool;
    DEV_DESC           *pdesc;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbr_octets;
    NET_BUF_SIZE        ix_offset;
    NET_BUF_QTY         i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg   = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;             /* Obtain ptr to the dev cfg struct.                    */
    pdev_data  = (NET_DEV_DATA      *)pif->Dev_Data;            /* Obtain ptr to dev data area.                         */
    pdev       = (NET_DEV           *)pdev_cfg->BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool  = &pdev_data->RxDescPool;
    nbr_octets =  pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                            (CPU_SIZE_T) nbr_octets,
                                            (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxBufDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrCur   = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrEnd   = (DEV_DESC *)pdesc + (pdev_cfg->RxDescNbr - 1);

                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */

    ix_offset = 0u;

    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->RxBufStatus =  0;
        pdesc->RxBufAddr   = (CPU_REG32)NetBuf_GetDataPtr((NET_IF        *) pif,
                                                          (NET_TRANSACTION) NET_TRANSACTION_RX,
                                                          (NET_BUF_SIZE   ) NET_IF_ETHER_FRAME_MAX_SIZE,
                                                          (NET_BUF_SIZE   ) NET_IF_IX_RX,
                                                          (NET_BUF_SIZE  *)&ix_offset,
                                                          (NET_BUF_SIZE  *) 0,
                                                          (NET_BUF_TYPE  *) 0,
                                                          (NET_ERR       *) perr);
        if (*perr != NET_BUF_ERR_NONE) {
             return;
        }

        pdesc->RxBufAddr += ix_offset;
        if (pdesc == pdev_data->RxBufDescPtrEnd) {              /* Set WRAP bit on last descriptor in list.             */
            pdesc->RxBufAddr |= AT91RM9200_EMAC_RXBUF_ADD_WRAP;
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
    pdev->ETH_RBQP = (CPU_INT32U)pdev_data->RxBufDescPtrStart;  /* Configure the DMA with the Rx desc start address.    */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_RxDescFreeAll()
*
* Description : (1) This function returns the descriptor memory block and descriptor data area
*                   memory blocks back to their respective memory pools :
*
*                   (a) Free Rx descriptor data areas
*                   (b) Free Rx descriptor memory block
*
*
* Argument(s) : pif         Pointer to the interface to free receive descriptors.
*               ---         Argument validated in NetDev_Stop().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE            No error
*                               NET_IF_MGR_ERR_nnnn         Various Network Interface management error codes
*                               NET_BUF_ERR_nnn             Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL receive buffers
*                   and the Rx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    MEM_POOL           *pmem_pool;
    DEV_DESC           *pdesc;
    LIB_ERR             lib_err;
    CPU_INT16U          i;
    CPU_INT08U         *pdesc_data;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pmem_pool = &pdev_data->RxDescPool;
    pdesc     =  pdev_data->RxBufDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */
        pdesc_data = (CPU_INT08U *)(pdesc->RxBufAddr & AT91RM9200_EMAC_RXBUF_ADD_BASE_MASK);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
                     pdev_data->RxBufDescPtrStart,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetDev_RxDescPtrCurInc()
*
* Description : (1) Increment current descriptor pointer to next receive descriptor :
*
*                   (a) Return ownership of current descriptor back to DMA.
*                   (b) Point to the next descriptor.
*
*
* Argument(s) : pif         Pointer to the interface to increment receive descriptors.
*               ---         Argument validated in NetDev_Rx().
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_RxDescPtrCurInc (NET_IF *pif)
{
    NET_DEV_DATA  *pdev_data;
    CPU_SR_ALLOC();


    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;

    pdev_data->RxBufDescPtrCur->RxBufStatus  =  0x00000000;         /* Free cur Rx descriptor.                          */
    pdev_data->RxBufDescPtrCur->RxBufAddr   &= ~AT91RM9200_EMAC_RXBUF_ADD_SW_OWNED;


    pdev_data->RxBufDescPtrCur++;                                   /* Advance to next Rx Buffer Descriptor.            */
    if (pdev_data->RxBufDescPtrCur > pdev_data->RxBufDescPtrEnd) {
        pdev_data->RxBufDescPtrCur = pdev_data->RxBufDescPtrStart;  /* Wrap around end of descriptor list if necessary. */
    }

    CPU_CRITICAL_ENTER();
    if (pdev_data->RxNRdyCtr > 0) {                             /* Decrement one less frame to process.                 */
        pdev_data->RxNRdyCtr--;
    }
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                        NetDev_ISR_Handler()
*
* Description : This function serves as the device Interrupt Service Routine Handler. This ISR
*               handler MUST service and clear all necessary and enabled interrupt events for
*               the device.
*
* Argument(s) : pif     Pointer to the interface to handle interrupts.
*
*               type    Network Interface defined argument representing the type of ISR in progress. Codes
*                       for Rx, Tx, Overrun, Jabber, etc... are defined within net_if.h and are passed
*                       into this function by the corresponding Net BSP ISR handler function. The Net
*                       BSP ISR handler function may be called by a specific ISR vector and therefore
*                       know which ISR type code to pass.  Otherwise, the Net BSP may pass
*                       NET_DEV_ISR_TYPE_UNKNOWN and the device driver MAY ignore the parameter when
*                       the ISR type can be deduced by reading an available interrupt status register.
*
*                       Type codes that are defined within net_if.c include but are not limited to :
*                           NET_DEV_ISR_TYPE_RX
*                           NET_DEV_ISR_TYPE_TX_COMPLETE
*                           NET_DEV_ISR_TYPE_UNKNOWN
*
* Return(s)   : none.
*
* Caller(s)   : Specific first- or second-level BSP ISR handler.
*
* Note(s)     : (1) This function is called by name from the context of an ISR.
*
*               (2) In the case of an interrupt occurring prior to Network Protocol Stack initialization,
*                   the device driver should ensure that the interrupt source is cleared in order
*                   to prevent the potential for an infinite interrupt loop during system initialization.
*
*               (3) Many DMA devices generate only one interrupt event for several ready receive
*                   descriptors.  In order to accommodate this, it is recommended that all DMA based
*                   drivers count the number of ready receive descriptors during the receive event
*                   and signal the receive task for ONLY newly received descriptors which have not
*                   yet been signaled for during the last receive interrupt event.
*
*               (4) Many DMA devices generate only one interrupt event for several transmit
*                   complete descriptors.  In this case, the driver MUST determine which descriptors
*                   have completed transmission and post each descriptor data area address to
*                   the transmit deallocation task.  The code provided below assumes one
*                   interrupt per transmit event which may not necessarily be the case for all
*                   devices.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_DATA            reg_val;
    CPU_INT16U          n_rdy;
    CPU_INT16U          n_new;
    CPU_INT16U          i;
    CPU_INT08U         *p_txdata;
    NET_ERR             err;


   (void)&type;                                                 /* Prevent 'variable unused' compiler warning.          */

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ---------------- HANDLE ISR TYPE(s) ---------------- */
    reg_val = pdev->ETH_ISR;                                    /* Read status reg, determine and clr all int src's.    */

                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if ((reg_val & (AT91RM9200_REG_EMAC_INT_RBNA |
                    AT91RM9200_REG_EMAC_INT_RCOM |
                    AT91RM9200_REG_EMAC_INT_ROVR)) > 0) {
                                                                /* Clear all receiver errors.                           */
        pdev->ETH_RSR = (AT91RM9200_REG_EMAC_RSR_BNA_CLR |
                         AT91RM9200_REG_EMAC_RSR_REC_CLR |
                         AT91RM9200_REG_EMAC_RSR_OVR_CLR);


                                                                /* Check for and handle RBNA device errata.             */
        if ((reg_val & AT91RM9200_REG_EMAC_INT_RBNA) > 0) {
             pdev->ETH_CTL &= ~AT91RM9200_REG_EMAC_CTL_RE_EN;
             pdev->ETH_CTL |=  AT91RM9200_REG_EMAC_CTL_RE_EN;
        }

                                                                /* Overrun condition, Rx desc automatically recovered.  */
        if ((reg_val & AT91RM9200_REG_EMAC_INT_ROVR) > 0) {
            ;
        }


        n_rdy = 0;                                              /* Frame(s) received. Cnt nbr of rdy Rx desc's.         */
        pdesc = pdev_data->RxBufDescPtrStart;
        for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
            if ((pdesc->RxBufAddr & AT91RM9200_EMAC_RXBUF_ADD_SW_OWNED) > 0) {
                n_rdy++;
            }

            pdesc++;
        }

        n_new = n_rdy - pdev_data->RxNRdyCtr;                   /* Determine how many descriptors have become ready ... */
                                                                /* ... since last count of ready descriptors        ... */
                                                                /* ... (see Note #3).                                   */

        while (n_new > 0) {
            NetIF_RxTaskSignal(pif->Nbr, &err);                 /* Signal Net IF RxQ Task for each new rdy descriptor.  */
            switch (err) {
                case NET_IF_ERR_NONE:
                     pdev_data->RxNRdyCtr++;
                     n_new--;
                     break;


                case NET_IF_ERR_RX_Q_FULL:
                case NET_IF_ERR_RX_Q_SIGNAL_FAULT:
                default:
                     n_new = 0;                                 /* Break loop to prevent further posting when Q full.   */
                     break;
            }
        }
    }


                                                                /* ------------- HANDLE TX COMPLETE ISRs -------------- */
    if ((reg_val & AT91RM9200_REG_EMAC_INT_TCOM) > 0) {
        if (pdev_data->TxBufPtrNext == 0) {
            p_txdata = (CPU_INT08U *)pdev_data->TxBufPtr0;
            pdev_data->TxBufPtrNext = 1;
        } else {
            p_txdata = (CPU_INT08U *)pdev_data->TxBufPtr1;
            pdev_data->TxBufPtrNext = 0;
        }

        if (p_txdata != (CPU_INT08U *)0) {
            NetIF_TxDeallocTaskPost(p_txdata, &err);
            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available        */
        }
    }
}


#endif  /* NET_IF_ETHER_MODULE_EN */

