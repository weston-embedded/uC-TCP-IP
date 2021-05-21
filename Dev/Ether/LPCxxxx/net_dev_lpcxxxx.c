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
*                                               LPCxxxx
*
* Filename : net_dev_lpcxxxx.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) The following parts may use this 'net_dev_lpcxxxx' device driver :
*
*                  * LPC23xx Series
*                  * LPC24xx Series
*                  * LPC32x0 Series
*                  * LPC17xx Series
*                  * LPC408x Series
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_LPCxxxx_MODULE

#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_lpcxxxx.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  NET_DEV_LPCxxxx                                0u
#define  NET_DEV_LPC1758                             1758u


#define  IPGT_DUPLEX_FULL_VAL                        0x15       /* IPGT recommended value in full duplex mode          */
#define  IPGT_DUPLEX_HALF_VAL                        0x12       /* IPGT recommended value in half duplex mode          */

#define  IPGR_VAL                                  0x0C12       /* IPGR recommended value                              */

#define  MII_TO                                    0xFFFF
#define  COMMAND_RST_TO_VAL                        0x00FF

#define  TX_TO                                     0x00FF


/*
*********************************************************************************************************
*                                       REGISTER BIT DEFINITIONS
*********************************************************************************************************
*/
                                                                /* Module control register                            */
#define  INT_FLAG_RX_OVERRUN                   DEF_BIT_00
#define  INT_FLAG_RX_ERR                       DEF_BIT_01
#define  INT_FLAG_RX_FINISHED                  DEF_BIT_02
#define  INT_FLAG_RX_DONE                      DEF_BIT_03
#define  INT_FLAG_TX_UNDERRUN                  DEF_BIT_04
#define  INT_FLAG_TX_ERR                       DEF_BIT_05
#define  INT_FLAG_TX_FINISHED                  DEF_BIT_06
#define  INT_FLAG_TX_DONE                      DEF_BIT_07
#define  INT_FLAG_SOFT                         DEF_BIT_12
#define  INT_FLAG_WAKEUP                       DEF_BIT_13
#define  INT_FLAG_ALL                         (DEF_BIT_FIELD(8u,  0u) | \
                                               DEF_BIT_FIELD(2u, 12u))

                                                                /* -------------- ETHERNET MAC REGISTERS ------------ */
#define  MAC1_RX_EN                            DEF_BIT_00       /* Rx frames will be received                         */
#define  MAC1_RX_PASS_ALL_FRAMES               DEF_BIT_01       /* MAC will pass all frames (normal vs control)       */
#define  MAC1_RX_FLOW_CTRL                     DEF_BIT_02       /* MAC acts upon Rx PAUSE flow control frames         */
#define  MAC1_TX_FLOW_CTRL                     DEF_BIT_03       /* Tx PAUSE flow control frames are permitted         */
#define  MAC1_LOOPBACK_EN                      DEF_BIT_04       /* MAC Tx interface is looped back to the MAC RX ...  */
                                                                /* ... interface                                      */
#define  MAC1_TX_RST                           DEF_BIT_08       /* Transmit function logic reset                      */
#define  MAC1_TX_MCS_RST                       DEF_BIT_09       /* MAC control sublayer/Transmit logic reset          */
#define  MAC1_RX_RST                           DEF_BIT_10       /* Ethernet receive logic reset                       */
#define  MAC1_RX_MCS_RST                       DEF_BIT_11       /* MAC Control sublayer/Receive logic reset           */
#define  MAC1_SIM_RST                          DEF_BIT_14       /* Reset the random number generator                  */
#define  MAC1_SOFT_RST                         DEF_BIT_15       /* Put all modules in reset state                     */
#define  MAC1_DIS_RST_ALL                      0x00000000       /* Clear all reset bits                               */

#define  MAC2_DUPLEX_FULL                      DEF_BIT_00       /* MAC Operates in Full-Duplex mode                   */
#define  MAC2_FRAME_LEN_CHK                    DEF_BIT_01       /* MAC Frame len checking enable                      */
#define  MAC2_HUGE_FRAME_EN                    DEF_BIT_02
#define  MAC2_CRC_EN                           DEF_BIT_04       /* Append a CRC to every frame                        */
#define  MAC2_PAD_CRC_EN                       DEF_BIT_05       /* MAC will pad all short frames                      */
#define  MAC2_VLAN_PAD_EN                      DEF_BIT_06       /* MAC will pad all short frames to 64 bytes and ...  */
                                                                /* ... append CRC                                     */
#define  MAC2_AUTO_DETECTE_PAD_EN              DEF_BIT_07       /* MAC will automatically detect the type of frame    */
#define  MAC2_PREAMBLE_CHK_EN                  DEF_BIT_08       /* MAC will verify the content of the preamble        */
#define  MAC2_EXCESS_DEFER                     DEF_BIT_14

#define  COMMAND_RX_EN                         DEF_BIT_00       /* Enable receive                                     */
#define  COMMAND_TX_EN                         DEF_BIT_01       /* Enable Transmit                                    */
#define  COMMAND_REG_RST                       DEF_BIT_03       /* All datapaths and the host registers are reset     */
#define  COMMAND_TX_RST                        DEF_BIT_04       /* Tx datapath is  reset                              */
#define  COMMAND_RX_RST                        DEF_BIT_05       /* Rx datapath is  reset                              */
#define  COMMAND_RST_ALL                       DEF_BIT_FIELD(3u, 3u)

#define  COMMAND_TX_FLOW_CTRL_EN               DEF_BIT_08       /* Enable IEEE 802.3 / Clause 21                      */
#define  COMMAND_RMII_EN                       DEF_BIT_09       /* RMII mode Enable                                   */
#define  COMMAND_DUPLEX_FULL_EN                DEF_BIT_10       /* PHY Full Duplex                                    */

#define  SUPP_PHY_RMII_SPD_100                 DEF_BIT_08       /* 100 Mbps in RMII mode                              */

#define  RX_FILTER_CTRL_UNICAST_EN             DEF_BIT_00       /* All unicast frames are accepted                    */
#define  RX_FILTER_CTRL_BROADCAST_EN           DEF_BIT_01       /* All broadcast frames are accepted                  */
#define  RX_FILTER_CTRL_MULTICAST_EN           DEF_BIT_02       /* All multicast frames are accepted                  */
#define  RX_FILTER_CTRL_UNICAST_HASH_EN        DEF_BIT_03       /* Frames that pass the imperfect hash filter are     */
                                                                /* ... accepted                                       */
#define  RX_FILTER_CTRL_MULTICAST_HASH_EN      DEF_BIT_04
#define  RX_FILTER_CTRL_PERFECT_EN             DEF_BIT_05       /* Frames with DestAdd = SrcAdd are accepted          */
#define  RX_FILTER_CTRL_ACCEPT_ALL             0x0000003F       /* All frames are accepted                            */

#define  RX_DESC_CTRL_INT_EN                   DEF_BIT_31       /* Generate a RxDone interrupt                        */

#define  MIND_BUSY                             DEF_BIT_00       /* MII Mgmt is currently performing a MII Mgmt Rd/Wr  */
#define  MIND_SCANNING                         DEF_BIT_01       /* MII Mgmt is currently performing a scan operation  */
#define  MIND_NOT_VALID                        DEF_BIT_02       /* MII Mgmt has not completed a read cycle            */
#define  MIND_MII_LINK_FAIL                    DEF_BIT_03       /* MII Mgmt link fail has occurred                    */

#define  MCFG_SCAN_INCREMENT                   DEF_BIT_00       /* MII Mgmt will scan across a range of PHYs          */
#define  MCFG_SUPRESS_PREAMBLE                 DEF_BIT_01       /* MII Mgmt will Rd and Wr without the 32 bit ...     */
                                                                /* ... preamble field                                 */

#define  MCFG_CLK_SEL_BIT_START                         2u      /* MII Mgmt clock select field start bit postition.   */
#define  MCFG_CLK_SEL_4                          ( 0 << 2)      /* MII Mgmt Clock (MDC) is divided by 4               */
#define  MCFG_CLK_SEL_6                          ( 2 << 2)      /* MII Mgmt Clock (MDC) is divided by 6               */
#define  MCFG_CLK_SEL_8                          ( 3 << 2)      /* MII Mgmt Clock (MDC) is divided by 8               */
#define  MCFG_CLK_SEL_10                         ( 4 << 2)      /* MII Mgmt Clock (MDC) is divided by 10              */
#define  MCFG_CLK_SEL_14                         ( 5 << 2)      /* MII Mgmt Clock (MDC) is divided by 14              */
#define  MCFG_CLK_SEL_20                         ( 6 << 2)      /* MII Mgmt Clock (MDC) is divided by 20              */
#define  MCFG_CLK_SEL_28                         ( 7 << 2)      /* MII Mgmt Clock (MDC) is divided by 28              */
#define  MCFG_CLK_SEL_36                         ( 8 << 2)      /* MII Mgmt Clock (MDC) is divided by 36              */
#define  MCFG_CLK_SEL_40                         ( 9 << 2)      /* MII Mgmt Clock (MDC) is divided by 40              */
#define  MCFG_CLK_SEL_44                         (10 << 2)      /* MII Mgmt Clock (MDC) is divided by 44              */
#define  MCFG_CLK_SEL_48                         (11 << 2)      /* MII Mgmt Clock (MDC) is divided by 48              */
#define  MCFG_CLK_SEL_52                         (12 << 2)      /* MII Mgmt Clock (MDC) is divided by 52              */
#define  MCFG_CLK_SEL_56                         (13 << 2)      /* MII Mgmt Clock (MDC) is divided by 56              */
#define  MCFG_CLK_SEL_60                         (14 << 2)      /* MII Mgmt Clock (MDC) is divided by 60              */
#define  MCFG_CLK_SEL_64                         (15 << 2)      /* MII Mgmt Clock (MDC) is divided by 64              */
#define  MCFG_CLK_SEL_BIT_END                          15u      /* MII Mgmt clock select field end bit position.      */

#define  MCFG_MII_MGMT_RST                     DEF_BIT_15       /* MII Mgmt reset                                     */

#define  RX_STATUS_INFO_ERR_CRC                DEF_BIT_23       /* The Rx frame had a CRC error                       */
#define  RX_STATUS_INFO_ERR_SYMBOL             DEF_BIT_24       /* The PHY repots a bit error over the MII reception  */
#define  RX_STATUS_INFO_ERR_LEN                DEF_BIT_25       /* The frame len field values doesn't match ...       */
                                                                /* ... with the current data len                      */
#define  RX_STATUS_INFO_ERR_RANGE              DEF_BIT_26       /* The Rx pkt exceed the maximum packet size          */
#define  RX_STATUS_INFO_ERR_ALIGN              DEF_BIT_27       /* An alignment error. IEEE std 802.3/ clause 4.3.2   */
#define  RX_STATUS_INFO_ERR_OVERRUN            DEF_BIT_28       /* Rx overrun                                         */
#define  RX_STATUS_INFO_ERR_NO_DESC            DEF_BIT_29       /* No new Rx descriptor is available                  */
#define  RX_STATUS_INFO_ERR_LAST_FLAG          DEF_BIT_30       /* Last fragment of a frame                           */
#define  RX_STATUS_INFO_ERR_OR_ERR             DEF_BIT_31       /* OR between all errors including range error        */

#define  RX_STATUS_INFO_ERR_ANY               (RX_STATUS_INFO_ERR_NO_DESC | \
                                               RX_STATUS_INFO_ERR_OVERRUN | \
                                               RX_STATUS_INFO_ERR_ALIGN   | \
                                               RX_STATUS_INFO_ERR_LEN     | \
                                               RX_STATUS_INFO_ERR_CRC     | \
                                               RX_STATUS_INFO_ERR_SYMBOL)

#define  TX_STATUS_INFO_ERR_EXC_DEFER          DEF_BIT_26       /* The packet incurred deferral beyond the ...        */
                                                                /* ... maximum deferral limit and was aborted.        */
#define  TX_STATUS_INFO_ERR_EXC_COL            DEF_BIT_27       /* The packet exceeded the maximum collision limit ...*/
                                                                /* ... and was aborted.                               */
#define  TX_STATUS_INFO_ERR_LATE_COL           DEF_BIT_28       /* Out of window Collision was seen, causing ...      */
                                                                /* ... packet abort                                   */
#define  TX_STATUS_INFO_ERR_UNDER              DEF_BIT_29       /* A Tx underrun occurred due to the adapter not ...  */
                                                                /* ... producing transmit data.                       */
#define  TX_STATUS_INFO_ERR_NO_DESC            DEF_BIT_30       /* The transmit stream was interrupted because a ...  */
                                                                /* ... descriptor was not available.                  */
#define  TX_STATUS_INFO_ERR_OR                 DEF_BIT_31       /* An error occurred during transmission. This is ... */
                                                                /* ... a logical OR of Underrun, LateCollision, ...   */
                                                                /* ... ExcessiveCollision, and ExcessiveDefer.        */

#define  RSV_MSK_BYTE_CNT                      DEF_BIT_FIELD(16u, 0u)
#define  RSV_BIT_PKT_IGNORED                   DEF_BIT_16
#define  RSV_BIT_RXDV                          DEF_BIT_17
#define  RSV_BIT_CARRIER                       DEF_BIT_18
#define  RSV_BIT_RX_CODE_VIOLATION             DEF_BIT_19
#define  RSV_BIT_CRC                           DEF_BIT_20
#define  RSV_BIT_LEN_CHK                       DEF_BIT_21
#define  RSV_BIT_LEN_RANGE                     DEF_BIT_22
#define  RSV_BIT_RX_OK                         DEF_BIT_23




#define  RX_STATUS_INFO_SIZE_MASK              0x000007FF

#define  RX_DESC_CTRL_SIZE_MASK                0x000007FF
#define  RX_DESC_CTRL_INT_EN                   DEF_BIT_31

#define  TX_DESC_CTRL_OVERRIDE                 DEF_BIT_26       /* Tx descriptor control field options                */
#define  TX_DESC_CTRL_PAD                      DEF_BIT_28
#define  TX_DESC_CTRL_CRC_EN                   DEF_BIT_29
#define  TX_DESC_CTRL_INT_EN                   DEF_BIT_31
#define  TX_DESC_CTRL_LAST                     DEF_BIT_30
#define  TX_DESC_CTRL_SIZE_MASK                0x000007FF

#define  MADR_PHY_ADDR_MASK                    0x00001F00
#define  MADR_REG_ADDR_MASK                    0x0000001F

#define  MCMD_RD                               DEF_BIT_00       /* RMII/MII read command                              */
#define  MCMD_RD_WITH_SCAN                     DEF_BIT_00 | \
                                               DEF_BIT_01
#define  MCMD_WR                               0x00000000       /* RMII/MII write command                             */


/*
*********************************************************************************************************
*                          GPIO REGISTER DEFINITION FRO MIIM SOFTWARE EMULATION
*********************************************************************************************************
*/

#define  LPC17XX_GPIO_ADDR_FIO                      (CPU_ADDR  )(0x2009C000)
#define  LPC17XX_GPIO_ADDR_PINSEL                   (CPU_ADDR  )(0x4002C000)
#define  LPC17XX_GPIO_ADDR_PINMODE                  (CPU_ADDR  )(0x4002C040)


#define  LPC17XX_GPIO_REG_FIODIRx(port_nbr)         (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_FIO     + (     (port_nbr) * 32u) + 0x00))
#define  LPC17XX_GPIO_REG_FIOMASKx(port_nbr)        (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_FIO     + (     (port_nbr) * 32u) + 0x10))
#define  LPC17XX_GPIO_REG_FIOPINx(port_nbr)         (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_FIO     + (     (port_nbr) * 32u) + 0x14))
#define  LPC17XX_GPIO_REG_FIOSETx(port_nbr)         (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_FIO     + (     (port_nbr) * 32u) + 0x18))
#define  LPC17XX_GPIO_REG_FIOCLRx(port_nbr)         (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_FIO     + (     (port_nbr) * 32u) + 0x1C))

#define  LPC17XX_GPIO_REG_PINSELLx(port_nbr)        (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_PINSEL  + (2u * (port_nbr) *  4u) + 0x00))
#define  LPC17XX_GPIO_REG_PINSELHx(port_nbr)        (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_PINSEL  + (2u * (port_nbr) *  4u) + 0x04))

#define  LPC17XX_GPIO_REG_PINMODELx(port_nbr)       (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_PINMODE + (2u * (port_nbr) *  4u) + 0x00))
#define  LPC17XX_GPIO_REG_PINMODEHx(port_nbr)       (*(CPU_REG32 *)(LPC17XX_GPIO_ADDR_PINMODE + (2u * (port_nbr) *  4u) + 0x04))

#define  LPC17XX_GPIO_PIN_MDC                        DEF_BIT_08
#define  LPC17XX_GPIO_PIN_MDIO                       DEF_BIT_09


/*
*********************************************************************************************************
*                                      MDIO FRAME FORMAT DEFINES
*
* Note(s) : (1) The MDIO standard specifies that the max clock frquency is 2.5 Mhz,
*               however some PHYs MAY used supports higher clocks.
*
*           (2) MDIO frame format:
*
*                  0    2    4          9         14   16         32
*                  +----|----|----------|----------|----|----------+
*                  | ST | OP | PHY ADDR | REG ADDR | TA |   DATA   |
*                  +----|----|----------|----------|----|----------|
*
*                  where,
*
*                      ST       : Start of frame. '01' (2-bits).
*                      OP       : Access type.    '01' Read.
*                                                 '10' Write.
*                      PHY ADDR : Phy      address. (5-bits).
*                      REG ADDR : Register address. (5-bits).
*                      TA       : Turnaround time.  (2-bits)
*                      DATA     : Register data.   (16-bits)
*                                 Driven by station for write.
*                                 Driven by station for read.

*********************************************************************************************************
*/

                                                                /* MDIO Max frequency (see note # 1).                   */
#define  LPC17XX_VAL_MDIO_MIN_FREQ              ((25u * DEF_TIME_NBR_uS_PER_SEC) / 10u)

#define  LPC17XX_BIT_MDIO_START                  DEF_BIT_30
#define  LPC17XX_BIT_MDIO_OP_RD                  DEF_BIT_29
#define  LPC17XX_BIT_MDIO_OP_WR                  DEF_BIT_28
#define  LPC17XX_BIT_MDIO_OP_TA_WR               DEF_BIT_17
#define  LPC17XX_MSK_MDIO_PHY_ADDR               DEF_BIT_FIELD( 5u, 0u)
#define  LPC17XX_MSK_MDIO_REG_ADDR               DEF_BIT_FIELD( 5u, 0u)
#define  LPC17XX_MSK_MDIO_DATA                   DEF_BIT_FIELD(16u, 0u)


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) Instance specific data area structures should be defined below.  The data area
*               structure typically includes error counters and variables used to track the
*               state of the device.  Variables required for correct operation of the device
*               MUST NOT be defined globally and should instead be included within the instance
*               specific data area structure and referenced as pif->Dev_Data structure members.
*
*           (2) DMA based devices may require more than one type of descriptor.  Each descriptor
*               type should be defined below.  An example descriptor has been provided.
*
*           (3) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*
*           (4) Since the MIIM signals needs to be emulated in software through GPIOs, 'PhyAccessLock'
*               is used to lock the MII_Rd() and MII_Wr() operations from multiple access.
*********************************************************************************************************
*/

                                                                /* --------------- DESCRIPTOR DATA TYPE ------------- */
typedef  struct  dev_desc {
  CPU_INT32U  PktAddr;                                          /* Base address of the data buffer for storing ...    */
                                                                /*  ... receive data.                                 */
  CPU_INT32U  Ctrl;                                             /* Control information                                */
} DEV_DESC;
                                                                /* ------------ RECEIVE STATUS DATA TYPE ------------ */
typedef  struct  dev_rx_status {
  CPU_INT32U  StatusInfo;                                       /* Receive status return flag.                        */
  CPU_INT32U  StatusHashCRC;                                    /* The concatenation of the destination address ...   */
                                                                /* ... hash CRC and the source address hash CRC       */
} DEV_RX_STATUS;
                                                                /* ------------ TRANSMIT STATUS DATA TYPE ----------- */
typedef  struct  dev_tx_status {
  CPU_INT32U  StatusInfo;                                       /* Transmit status return flag.                       */
} DEV_TX_STATUS;

                                                                /* -------------- DEVICE INSTANCE DATA -------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
                                                                /* ------------- RECEIVE ERROR COUNTERS ------------- */
    NET_CTR         ErrRxPktCRCCtr;                             /* CRC in the packet did NOT match the internally CRC */
    NET_CTR         ErrRxSymbolCtr;                             /* The PHY reports a bit error over the MII           */
    NET_CTR         ErrRxInvalidLenCtr;                         /* Frame len field value does NOT match actual len    */
    NET_CTR         ErrRxNotValidDescCtr;                       /* No new Rx descriptor is available,                 */
    NET_CTR         ErrRxAlignCtr;
    NET_CTR         ErrRxOverrunCtr;                            /* Receive overrun.                                   */
    NET_CTR         ErrRxPktDiscarder;                          /* Indicates that a packet was dropped.               */
    NET_CTR         ErrRxDataAreaAllocCtr;                      /* Rx buffer is not available.                        */

                                                                /* ------------ TRANSMIT ERROR COUNTERS ------------- */
    NET_CTR         ErrTxNotValidDescCtr;                       /* Not valid descriptor counter.                      */
    NET_CTR         ErrTxUnderrunCtr;                           /* Fatal underrun counter.                            */
    NET_CTR         ErrTxExcCollisionCtr;                       /* Excessive collision counter.                       */
    NET_CTR         ErrTxExcDeferCtr;                           /* Excessive deferall  counter.                       */
    NET_CTR         ErrTxLateColissionCtr;                      /* Late Collision counter.                            */
#endif

#if NET_CTR_CFG_STAT_EN
    NET_CTR         RxPktCtr;
    NET_CTR         TxPktCtr;
#endif

    MEM_POOL        RxDescPool;
    MEM_POOL        RxStatusPool;
    MEM_POOL        TxDescPool;
    MEM_POOL        TxStatusPool;
    DEV_DESC       *RxDescPtrStart;
    DEV_RX_STATUS  *RxStatusPtrStart;
    DEV_DESC       *TxDescPtrStart;
    DEV_TX_STATUS  *TxStatusPtrStart;
    CPU_INT32U      TxConsumeIx;
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U      MulticastAddrHashBitCtr[64];
#endif

    CPU_BOOLEAN     PhyAccessLock;

} NET_DEV_DATA;


                                                                /* ------------ DEVICE TIMESTAMP DATA TYPE ---------- */
#if     (CPU_CFG_TS_64_EN == DEF_ENABLED)
typedef  CPU_TS64   NET_DEV_TS;
#elif   (CPU_CFG_TS_32_EN == DEF_ENABLED)
typedef  CPU_TS32   NET_DEV_TS;
#endif


/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*
* Note(s) : (1) The device register definition structure MUST take into account appropriate
*               register offsets and apply reserved space as required.  The registers listed
*               within the register definition structure MUST reflect the exact ordering and
*               data sizes illustrated in the device user guide. An example device register
*               structure is provided below.
*
*           (2) The device register definition structure is mapped over the corresponding base
*               address provided within the device configuration structure.  The device
*               configuration structure is provided by the application developer within
*               net_dev_cfg.c.  Refer to the Network Protocol Suite User Manual for more
*               information related to declaring device configuration structures.
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  MAC1;                                            /* MAC registers                                      */
    CPU_REG32  MAC2;
    CPU_REG32  IPGT;
    CPU_REG32  IPGR;
    CPU_REG32  CLRT;
    CPU_REG32  MAXF;
    CPU_REG32  SUPP;
    CPU_REG32  TEST;
    CPU_REG32  MCFG;
    CPU_REG32  MCMD;
    CPU_REG32  MADR;
    CPU_REG32  MWTD;
    CPU_REG32  MRDD;
    CPU_REG32  MIND;
    CPU_REG32  RESERVED0[2];
    CPU_REG32  SA0;
    CPU_REG32  SA1;
    CPU_REG32  SA2;
    CPU_REG32  RESERVED1[45];
    CPU_REG32  COMMAND;                                         /* Control registers                                         */
    CPU_REG32  STAT;
    CPU_REG32  RX_DESC_ADDR;
    CPU_REG32  RX_STAT_ADDR;
    CPU_REG32  RX_DESC_NBR;
    CPU_REG32  RX_PRODUCE_IX;
    CPU_REG32  RX_CONSUME_IX;
    CPU_REG32  TX_DESC_ADDR;
    CPU_REG32  TX_STAT_ADDR;
    CPU_REG32  TX_DESC_NBR;
    CPU_REG32  TX_PRODUCE_IX;
    CPU_REG32  TX_CONSUME_IX;
    CPU_REG32  RESERVED2[10];
    CPU_REG32  TSV0;
    CPU_REG32  TSV1;
    CPU_REG32  RSV;
    CPU_REG32  RESERVED3[3];
    CPU_REG32  FLOW_CTRL_CNTR;
    CPU_REG32  FLOW_CTRL_STAT;
    CPU_REG32  RESERVED4[34];
    CPU_REG32  RX_FILTER_CTRL;                                  /* Rx filters registers                                      */
    CPU_REG32  RX_FILTER_WOL_STAT;
    CPU_REG32  RX_FILTER_WOL_CLR;
    CPU_REG32  RESERVED5;
    CPU_REG32  HASH_FILTERL;
    CPU_REG32  HASH_FILTERH;
    CPU_REG32  RESERVED6[882];
    CPU_REG32  INT_STAT;
    CPU_REG32  INT_EN;
    CPU_REG32  INT_CLR;
    CPU_REG32  INT_SET;
    CPU_REG32  RESERVED7;
    CPU_REG32  PWR_DOWN;
    CPU_REG32  RESEVED8;
} NET_DEV;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL MACRO'S
*********************************************************************************************************
*/

#if     (CPU_CFG_TS_64_EN == DEF_ENABLED)
#define  NET_DEV_TS_GET()                   CPU_TS_Get64();

#elif   (CPU_CFG_TS_32_EN == DEF_ENABLED)
#define  NET_DEV_TS_GET()                   CPU_TS_Get32();
#else
#define  NET_DEV_TS_GET()
#endif


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
static  void         NetDev_LPCxxxx_Init       (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_LPC1758_Init       (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_InitHandler        (NET_IF             *pif,
                                                CPU_INT16U          lpc_dev_type,
                                                NET_ERR            *perr);


static  void         NetDev_LPCxxxx_Start      (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_LPC1758_Start      (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_StartHandler       (NET_IF             *pif,
                                                CPU_INT16U          lpc_dev_type,
                                                NET_ERR            *perr);


static  void         NetDev_Stop               (NET_IF             *pif,
                                                NET_ERR            *perr);


static  void         NetDev_Rx                 (NET_IF             *pif,
                                                CPU_INT08U        **p_data,
                                                CPU_INT16U         *size,
                                                NET_ERR            *perr);

static  void         NetDev_Tx                 (NET_IF             *pif,
                                                CPU_INT08U         *p_data,
                                                CPU_INT16U          size,
                                                NET_ERR            *perr);


static  void         NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                                CPU_INT08U         *paddr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *perr);

static  void         NetDev_AddrMulticastRemove(NET_IF             *pif,
                                                CPU_INT08U         *paddr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *perr);


static  void         NetDev_ISR_Handler        (NET_IF             *pif,
                                                NET_DEV_ISR_TYPE    type);

static  void         NetDev_IO_Ctrl            (NET_IF             *pif,
                                                CPU_INT08U          opt,
                                                void               *p_data,
                                                NET_ERR            *perr);


static  void         NetDev_LPCxxxx_MII_Rd     (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *perr);

static  void         NetDev_LPCxxxx_MII_Wr     (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);


static  void         NetDev_LPC1758_MII_Rd     (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *perr);

static  void         NetDev_LPC1758_MII_Wr     (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);


                                                                        /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
static  void         NetDev_RxDescInit         (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_RxDescFreeAll      (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_RxConsumeIxInc     (NET_IF             *pif);


static  void         NetDev_TxDescInit         (NET_IF             *pif,
                                                NET_ERR            *perr);


static  CPU_BOOLEAN  NetDev_LPC1758_MII_Clk    (NET_DEV            *pdev);


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      NETWORK DEVICE DRIVER APIs
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
                                                                                    /* LPCxxxx dev API fnct ptrs :      */
const  NET_DEV_API_ETHER  NetDev_API_LPCxxxx = { NetDev_LPCxxxx_Init,               /*   Init/add                       */
                                                 NetDev_LPCxxxx_Start,              /*   Start                          */
                                                 NetDev_Stop,                       /*   Stop                           */
                                                 NetDev_Rx,                         /*   Rx                             */
                                                 NetDev_Tx,                         /*   Tx                             */
                                                 NetDev_AddrMulticastAdd,           /*   Multicast addr add             */
                                                 NetDev_AddrMulticastRemove,        /*   Multicast addr remove          */
                                                 NetDev_ISR_Handler,                /*   ISR handler                    */
                                                 NetDev_IO_Ctrl,                    /*   I/O ctrl                       */
                                                 NetDev_LPCxxxx_MII_Rd,             /*   Phy reg rd                     */
                                                 NetDev_LPCxxxx_MII_Wr              /*   Phy reg wr                     */
                                               };


                                                                                    /* LPC1758 dev API fnct ptrs :      */
const  NET_DEV_API_ETHER  NetDev_API_LPC1758 = { NetDev_LPC1758_Init,               /*   Init/add                       */
                                                 NetDev_LPC1758_Start,              /*   Start                          */
                                                 NetDev_Stop,                       /*   Stop                           */
                                                 NetDev_Rx,                         /*   Rx                             */
                                                 NetDev_Tx,                         /*   Tx                             */
                                                 NetDev_AddrMulticastAdd,           /*   Multicast addr add             */
                                                 NetDev_AddrMulticastRemove,        /*   Multicast addr remove          */
                                                 NetDev_ISR_Handler,                /*   ISR handler                    */
                                                 NetDev_IO_Ctrl,                    /*   I/O ctrl                       */
                                                 NetDev_LPC1758_MII_Rd,             /*   Phy reg rd                     */
                                                 NetDev_LPC1758_MII_Wr              /*   Phy reg wr                     */
                                               };


/*
*********************************************************************************************************
*                                        NetDev_LPCxxxx_Init()
*
* Description : Initialize generic LPCxxxx device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPCxxxx_Init (NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_LPCxxxx, perr);
}


/*
*********************************************************************************************************
*                                        NetDev_LPC1758_Init()
*
* Description : Initialize LPC1758 device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPC1758_Init (NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_LPC1758, perr);
}


/*
*********************************************************************************************************
*                                        NetDev_InitHandler()
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
* Argument(s) : pif             Pointer to the interface requiring service.
*
*               lpc_dev_type    LPCxxxx device type :
*
*                                   NET_DEV_LPCxxxx             Generic LPCxxxx device.
*                                   NET_DEV_LPC1758                     LPC1758 device.
*
*               perr            Pointer to return error code :
*
*                                   NET_DEV_ERR_NONE            No Error.
*                                   NET_DEV_ERR_INIT            General initialization error.
*                                   NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_LPCxxxx_Init(),
*               NetDev_LPC1758_Init().
*
* Note(s)     : (2) The application developer SHOULD define NetDev_CfgClk() within net_bsp.c
*                   in order to properly enable clocks for specified network interface.  In
*                   some cases, a device may require clocks to be enabled for BOTH the device
*                   and accessory peripheral modules such as GPIO.  A call to this function
*                   MAY need to occur BEFORE any device register accesses are made.  In the
*                   event that a device does NOT require any external clocks to be enabled,
*                   it is recommended that the device driver still call the NetBSP function
*                   which may in turn leave the section for the specific interface number
*                   empty.
*
*               (3) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.  Micrium recommends defining and calling this NetBSP
*                   function even if no additional GPIO initialization is required.
*
*               (4) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
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
*               (7) Drivers SHOULD validate device configuration values and set *perr to
*                   NET_DEV_ERR_INVALID_CFG if unacceptable values have been specified. Fields
*                   of interest generally include, but are not limited to :
*
*                   (a) pdev_cfg->RxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (b) pdev_cfg->TxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (c) pdev_cfg->RxBufAlignOctets
*                   (d) pdev_cfg->TxBufAlignOctets
*                   (e) pdev_cfg->RxBufDescNbr
*                   (f) pdev_cfg->TxBufDescnbr
*
*               (8) Descriptors are typically required to be contiguous in memory.  Allocation of
*                   descriptors MUST occur as a single contiguous block of memory.  The driver may
*                   use pointer arithmetic to sub-divide and traverse the descriptor list.
*
*               (9) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*
*              (10) Some drivers MAY require knowledge of the Phy configuration in order
*                   to properly configure the MAC with the correct Phy bus mode, speed and
*                   duplex settings.  If a driver requires access to the Phy configuration,
*                   then the driver MUST validate the pif->Phy_Cfg pointer by checking for
*                   a NULL pointer BEFORE attempting to access members of the Phy
*                   configuration structure.  Phy configuration fields of interest generally
*                   include, but are  not limited to :
*
*                   (a) pphy_cfg->Type :
*
*                       (1) NET_PHY_TYPE_INT            Phy integrated with MAC.
*                       (2) NET_PHY_TYPE_EXT            Phy externally attached to MAC.
*
*                   (b) pphy_cfg->BusMode :
*
*                       (1) NET_PHY_BUS_MODE_MII        Phy bus mode configured to MII.
*                       (2) NET_PHY_BUS_MODE_RMII       Phy bus mode configured to RMII.
*                       (3) NET_PHY_BUS_MODE_SMII       Phy bus mode configured to SMII.
*
*                   (c) pphy_cfg->Spd :
*
*                       (1) NET_PHY_SPD_0               Phy link speed unknown or NOT linked.
*                       (2) NET_PHY_SPD_10              Phy link speed configured to  10   mbit/s.
*                       (3) NET_PHY_SPD_100             Phy link speed configured to  100  mbit/s.
*                       (4) NET_PHY_SPD_1000            Phy link speed configured to  1000 mbit/s.
*                       (5) NET_PHY_SPD_AUTO            Phy link speed configured for auto-negotiation.
*
*                   (d) pphy_cfg->Duplex :
*
*                       (1) NET_PHY_DUPLEX_UNKNOWN      Phy link duplex unknown or link not established.
*                       (2) NET_PHY_DUPLEX_HALF         Phy link duplex configured to  half duplex.
*                       (3) NET_PHY_DUPLEX_FULL         Phy link duplex configured to  full duplex.
*                       (4) NET_PHY_DUPLEX_AUTO         Phy link duplex configured for auto-negotiation.
*********************************************************************************************************
*/

static  void  NetDev_InitHandler (NET_IF      *pif,
                                  CPU_INT16U   lpc_dev_type,
                                  NET_ERR     *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reg_to;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbr_bytes;
    CPU_ADDR            mem_addr;
    CPU_ADDR            mem_size;
    CPU_INT32U          mdc_ref_clk;
    CPU_INT32U          mdc_cfg;
    CPU_INT08U          bit;
    CPU_INT08U          bit_nbr;
    CPU_BOOLEAN         bit_found;
    LIB_ERR             lib_err;


                                                                /* ------- OBTAIN REFERENCE TO CFGs/REGs/BSP -------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;

    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.    */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
                                                        /* -------------- VALIDATE DEVICE CFG --------------- */
                                                                /* See Note #7.                                       */
                                                                /* Validate Rx buf alignment.                         */
    if (pdev_cfg->RxBufAlignOctets != CPU_WORD_SIZE_32) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf size.                              */
    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )pif->Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf alignment.                         */
    if (pdev_cfg->TxBufAlignOctets != CPU_WORD_SIZE_32) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf ix offset (see Note #2).             */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }

    if ((pphy_cfg->BusMode != NET_PHY_BUS_MODE_MII) &&          /* The LPCxxx Ethernet controller only accepts MII    */
        (pphy_cfg->BusMode != NET_PHY_BUS_MODE_RMII)) {         /* ... or RMII PHY bus mode                           */
        *perr = NET_PHY_ERR_INVALID_CFG;
         return;
    }

    if  (pphy_cfg->Type != NET_PHY_TYPE_EXT) {                  /* The LPCxxx Ether controller always interfaces ...  */
                                                                /* ... with an external PHY controller                */
        *perr = NET_PHY_ERR_INVALID_CFG;
         return;
    }

    if ((pphy_cfg->Spd != NET_PHY_SPD_0)   &&                   /* The LPCxxx Ethernet controller only works at ...   */
        (pphy_cfg->Spd != NET_PHY_SPD_10)  &&                   /* ... 100 Mbps or 10  Mbps                           */
        (pphy_cfg->Spd != NET_PHY_SPD_100) &&
        (pphy_cfg->Spd != NET_PHY_SPD_AUTO)) {
        *perr = NET_PHY_ERR_INVALID_CFG;
         return;
    }


                                                                /* ------------- ALLOCATE DEV DATA AREA ------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4u,
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;


                                                                /* ------------ ENABLE NECESSARY CLOCKS ------------- */
                                                                /* Enable module clks (see Note #2).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


                                                                /* ------ INITIALIZE EXTERNAL GPIO CONTROLLER ------- */
                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------ ALLOCATE MEMORY FOR DMA DESCRIPTORS ------- */
    if (pdev_cfg->RxBufPoolType == NET_IF_MEM_TYPE_MAIN) {
        mem_addr = 0u;
        mem_size = 0u;
    } else {
        mem_addr = pdev_cfg->MemAddr;
        mem_size = pdev_cfg->MemSize;
    }

                                                                /* ------ ALLOCATE MEMORY FOR RX DESCRIPTORS -------- */
    nbr_bytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);         /* Number of byte required                            */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.          */
                   (void       *) mem_addr,                     /* From the dedicated memory/ uC/LIB Mem generic pool */
                   (CPU_SIZE_T  ) mem_size,                     /* Dedicated Memory size    / 0 for  Mem generic pool */
                   (CPU_SIZE_T  ) 1u,                           /* Allocate one block                                 */
                   (CPU_SIZE_T  ) nbr_bytes,
                   (CPU_SIZE_T  ) 4u,                           /* Align block to specified alignment                 */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.  */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.           */

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }
                                                                /* ------ ALLOCATE MEMORY FOR RX STATUS ARRAY ------- */
    nbr_bytes = pdev_cfg->RxDescNbr * sizeof(DEV_RX_STATUS);    /* Number of byte required                            */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxStatusPool,      /* Pass a pointer to the mem pool to create.          */
                   (void       *) mem_addr,                     /* From the dedicated memory/ uC/LIB Mem generic pool */
                   (CPU_SIZE_T  ) mem_size,                     /* Dedicated Memory size    / 0 for  Mem generic pool */
                   (CPU_SIZE_T  ) 1u,
                   (CPU_SIZE_T  ) nbr_bytes,
                   (CPU_SIZE_T  ) 8u,                           /* Align block to specified alignment                 */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.  */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.           */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------ ALLOCATE MEMORY FOR TX DESCRIPTORS -------- */
    if (pdev_cfg->TxBufPoolType == NET_IF_MEM_TYPE_MAIN) {
        mem_addr = 0u;
        mem_size = 0u;
    } else {
        mem_addr = pdev_cfg->MemAddr;
        mem_size = pdev_cfg->MemSize;
    }

    nbr_bytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);         /* Number of byte required                            */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.          */
                   (void       *) mem_addr,                     /* From the dedicated memory/ uC/LIB Mem generic pool */
                   (CPU_SIZE_T  ) mem_size,                     /* Dedicated Memory size    / 0 for  Mem generic pool */
                   (CPU_SIZE_T  ) 1u,                           /* Allocate one block                                 */
                   (CPU_SIZE_T  ) nbr_bytes,
                   (CPU_SIZE_T  ) 4u,                           /* Align block to specified alignment                 */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.  */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.           */

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    nbr_bytes = pdev_cfg->TxDescNbr * sizeof(DEV_TX_STATUS);    /* Number of byte required                            */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxStatusPool,      /* Pass a pointer to the mem pool to create.          */
                   (void       *) mem_addr,                     /* From the dedicated memory/ uC/LIB Mem generic pool */
                   (CPU_SIZE_T  ) mem_size,                     /* Dedicated Memory size    / 0 for  Mem generic pool */
                   (CPU_SIZE_T  ) 1u,                           /* Allocate one block                                 */
                   (CPU_SIZE_T  ) nbr_bytes,
                   (CPU_SIZE_T  ) 4u,                           /* Align block to specified alignment                 */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.  */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.           */

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------------- INITIALIZE DEV REGISTERS ----------- */

    pdev->INT_EN         = DEF_BIT_NONE;                        /* Disable all interrupts                             */
    pdev->INT_CLR        = INT_FLAG_ALL;                        /* Clear all pending interrupts                       */

                                                                /* ---------------- MAC TX/RX RESET ----------------- */
    pdev->MAC1           = (MAC1_SOFT_RST
                         | MAC1_RX_MCS_RST
                         | MAC1_TX_MCS_RST
                         | MAC1_RX_RST
                         | MAC1_SIM_RST
                         | MAC1_TX_RST);
                                                                /* Perform a full soft reset of the ethernet block    */
    pdev->MAC1           = MAC1_SOFT_RST;                       /* ... Set the 'SOST RESET' bit in the MAC 1          */
    pdev->COMMAND        = COMMAND_RST_ALL;                     /* ... Reset All Registers                            */

    reg_to               = COMMAND_RST_TO_VAL;
                                                                /* ... Wait until the COMMAND_REG_RST bit is clear    */
    while ((DEF_BIT_IS_SET_ANY(pdev->COMMAND, COMMAND_RST_ALL)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {                                         /* If the Tx, Rx datapaths cannot be reset then ...   */
        *perr = NET_DEV_ERR_INIT;                               /* ... return with error                              */
    }

    pdev->MAC1           = MAC1_DIS_RST_ALL;                    /* De-assert all resets.                              */

                                                                /* ----------- MAC 1 REGISTER INITIALIZATION -------- */
    pdev->MAC1           = (MAC1_RX_PASS_ALL_FRAMES
                         |  MAC1_RX_FLOW_CTRL
                         |  MAC1_TX_FLOW_CTRL);

    pdev->MAC2           = (MAC2_FRAME_LEN_CHK                  /* MAC Frame len checking enable                      */
                         |  MAC2_CRC_EN                         /* Append a CRC to every frame                        */
                         |  MAC2_PAD_CRC_EN                     /* MAC will pad all short frames                      */
                         |  MAC2_VLAN_PAD_EN                    /* MAC will pad all short frames to 64 bytes + CRC    */
                         |  MAC2_EXCESS_DEFER
                         |  MAC2_AUTO_DETECTE_PAD_EN            /* MAC will automatically detect the type of frame    */
                         |  MAC2_PREAMBLE_CHK_EN);              /* MAC will verify the content of the preamble        */

    pdev->RX_FILTER_CTRL = (RX_FILTER_CTRL_MULTICAST_HASH_EN    /* Accept multicast hash match frames.                */
                         |  RX_FILTER_CTRL_BROADCAST_EN         /* Accept broadcast frames.                           */
                         |  RX_FILTER_CTRL_PERFECT_EN);         /* Accept perfect match frames.                       */

    if ((pphy_cfg->Duplex == NET_PHY_DUPLEX_FULL) ||            /* ------ NON/BACK-TO-BACK-INTER-PACKET-GAP CFG ----- */
        (pphy_cfg->Duplex == NET_PHY_DUPLEX_AUTO)) {
        DEF_BIT_SET(pdev->MAC2, MAC2_DUPLEX_FULL);
        pdev->IPGT  = IPGT_DUPLEX_FULL_VAL;
    } else {
        DEF_BIT_CLR(pdev->MAC2, MAC2_DUPLEX_FULL);
        pdev->IPGT  = IPGT_DUPLEX_HALF_VAL;
    }

    pdev->IPGR = IPGR_VAL;
                                                                /* --- SUPP (PHY SUPOPRT) REGISTER INITIALIZATION --- */
    if ( (pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII) &&
        ((pphy_cfg->Spd == NET_PHY_SPD_100) ||
         (pphy_cfg->Spd == NET_PHY_SPD_AUTO))) {
        pdev->SUPP = SUPP_PHY_RMII_SPD_100;
    }
                                                                /* ------ MII MANAGMENT REGISTER CONFIGURATION ------ */
    pdev->MCFG  = MCFG_MII_MGMT_RST;                            /* Reset the MII management hardware                  */
    pdev->MCFG  = DEF_BIT_NONE;
                                                                /* Set the clk divider in the MII Mgmt Cfg. register  */

    mdc_ref_clk = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    mdc_ref_clk /= DEF_TIME_NBR_mS_PER_SEC;

    bit       = MCFG_CLK_SEL_BIT_START;
    bit_found = DEF_NO;

    while ((bit       <  MCFG_CLK_SEL_BIT_END) &&
           (bit_found == DEF_NO             )) {
        DEF_BIT_SET(pdev->MCFG, DEF_BIT(bit));
        if (DEF_BIT_IS_CLR(pdev->MCFG, DEF_BIT(bit))) {
            bit_found = DEF_YES;
        } else {
            bit++;
        }
    }

    bit_nbr = bit - MCFG_CLK_SEL_BIT_START;

    switch (bit_nbr) {
        case 4u:
             if (mdc_ref_clk > 150000u) {
                 mdc_cfg = MCFG_CLK_SEL_64;
                 break;
             }
             if (mdc_ref_clk > 140000u) {
                 mdc_cfg = MCFG_CLK_SEL_60;
                 break;
             }
             if (mdc_ref_clk > 130000u) {
                 mdc_cfg = MCFG_CLK_SEL_56;
                 break;
             }
             if (mdc_ref_clk > 120000u) {
                 mdc_cfg = MCFG_CLK_SEL_52;
                 break;
             }
             if (mdc_ref_clk > 100000u) {
                 mdc_cfg = MCFG_CLK_SEL_48;
                 break;
             }
             if (mdc_ref_clk >  90000u) {
                 mdc_cfg = MCFG_CLK_SEL_44;
                 break;
             }
             if (mdc_ref_clk >  80000u) {
                 mdc_cfg = MCFG_CLK_SEL_40;
                 break;
             }
             if (mdc_ref_clk >  70000u) {
                 mdc_cfg = MCFG_CLK_SEL_36;
                 break;
             }
                                                                /* 'break' intentionally omitted.                     */
        case 3u:
             if (mdc_ref_clk >  50000u) {
                 mdc_cfg = MCFG_CLK_SEL_28;
                 break;
             }
             if (mdc_ref_clk >  35000u) {
                 mdc_cfg = MCFG_CLK_SEL_20;
                 break;
             }
             if (mdc_ref_clk >  25000u) {
                 mdc_cfg = MCFG_CLK_SEL_14;
                 break;
             }
             if (mdc_ref_clk >  20000u) {
                 mdc_cfg = MCFG_CLK_SEL_10;
                 break;
             }
                                                                /* 'break' intentionally omitted.                     */
        case 2u:
             if (mdc_ref_clk >  15000u) {
                 mdc_cfg = MCFG_CLK_SEL_8;
                 break;
             }
             if (mdc_ref_clk >  10000u) {
                 mdc_cfg = MCFG_CLK_SEL_6;
                 break;
             }
                                                                /* 'break' intentionally omitted.                     */
        case 1u:
             mdc_cfg = MCFG_CLK_SEL_4;
             break;

        default:
            *perr = NET_DEV_ERR_INVALID_CFG;
             return;
    }

    pdev->MCFG = mdc_cfg;


    pdev->MCMD = DEF_BIT_NONE;
    DEF_BIT_CLR(pdev->MAC1, MAC1_SOFT_RST);

    pdev->COMMAND = COMMAND_TX_FLOW_CTRL_EN;                    /* Enable IEEE 802.3/Clause 31                        */

    if (pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII) {           /* Set the PHY Bus Mode                               */
        DEF_BIT_SET(pdev->COMMAND, COMMAND_RMII_EN);
    }

    if ((pphy_cfg->Duplex == NET_PHY_DUPLEX_FULL) ||            /* Set the PHY duplex mode                            */
        (pphy_cfg->Duplex == NET_PHY_DUPLEX_AUTO)) {
        DEF_BIT_SET(pdev->COMMAND, COMMAND_DUPLEX_FULL_EN);
    }

                                                                /* Initialize ext int ctrl'r.                           */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


    switch (lpc_dev_type) {                                     /* Set the ENET_MDC & ENET_MDIO pins as controlled... */
                                                                /* ... GPIO controller.                               */
        case NET_DEV_LPC1758:
             DEF_BIT_CLR(LPC17XX_GPIO_REG_PINSELLx(2u),  (DEF_BIT_16 | DEF_BIT_17));
             DEF_BIT_CLR(LPC17XX_GPIO_REG_PINSELLx(2u),  (DEF_BIT_18 | DEF_BIT_19));
             DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u),    LPC17XX_GPIO_PIN_MDIO);
             DEF_BIT_SET(LPC17XX_GPIO_REG_FIODIRx(2u),    LPC17XX_GPIO_PIN_MDC);
             DEF_BIT_CLR(LPC17XX_GPIO_REG_PINMODELx(2u), (DEF_BIT_18 | DEF_BIT_19));
             break;

        case NET_DEV_LPCxxxx:
        default:
             break;
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_LPCxxxx_Start()
*
* Description : Start generic LPCxxxx interface hardware.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPCxxxx_Start (NET_IF   *pif,
                                    NET_ERR  *perr)
{
    NetDev_StartHandler(pif, NET_DEV_LPCxxxx, perr);
}


/*
*********************************************************************************************************
*                                        NetDev_LPC1758_Start()
*
* Description : Start LPC1758 interface hardware.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPC1758_Start (NET_IF   *pif,
                                    NET_ERR  *perr)
{
    NetDev_StartHandler(pif, NET_DEV_LPC1758, perr);
}


/*
*********************************************************************************************************
*                                        NetDev_StartHandler()
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
* Argument(s) : pif             Pointer to a network interface.
*               ---             Argument validated in NetIF_Start().
*
*               lpc_dev_type    LPCxxxx device type :
*
*                                   NET_DEV_LPCxxxx                 Generic LPCxxxx device.
*                                   NET_DEV_LPC1758                         LPC1758 device.
*
*               perr            Pointer to variable that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE                Ethernet device successfully started.
*
*                                                                   - RETURNED BY NetIF_AddrHW_SetHandler() : --
*                                   NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                                   NET_ERR_FAULT_NULL_FNCT         Invalid NULL function pointer.
*                                   NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                                   NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                                   NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                                   NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                                   - RETURNED BY NetIF_DevCfgTxRdySignal() : -
*                                   NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                                   NET_OS_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
*                                                                   ---- RETURNED BY NetDev_RxDescInit() : -----
*                              !!!!
*
*                                                                   ---- RETURNED BY NetDev_TxDescInit() : -----
*                              !!!!
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_LPCxxxx_Start(),
*               NetDev_LPC1758_Start().
*
* Note(s)     : (2) Many DMA devices may generate only one interrupt for several ready receive
*                   descriptors.  In order to accommodate this, it is recommended that all DMA
*                   based drivers count the number of ready receive descriptors during the
*                   receive event and signal the receive task accordingly ONLY for those
*                   NEW descriptors which have not yet been accounted for.  Each time a
*                   descriptor is processed (or discarded) the count for acknowledged and
*                   unprocessed frames should be decremented by 1.  This function initializes the
*                   acknowledged receive descriptor count to 0.

*               (3) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, DMA based interfaces may have several
*                   frames configured for transmission before blocking is required. The number
*                   of queued transmit frames depends on the number of configured transmit
*                   descriptors.
*
*               (4) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_Ether_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (5) The device hardware address is set from one of the data sources below. Each source
*                   is listed in the order of precedence.
*
*                   (a) Device Configuration Structure       Configure a valid HW address during
*                                                                compile time.
*
*                                                            Configure either "00:00:00:00:00:00" or
*                                                                an empty string, "", in order to
*                                                                configure the HW address using
*                                                                method (b).
*
*                   (b) NetIF_Ether_HW_AddrSet()             Call NetIF_Ether_HW_AddrSet() if the HW
*                                                                address needs to be configured via
*                                                                run-time from a different data
*                                                                source. E.g. Non auto-loading
*                                                                memory such as I2C or SPI EEPROM.
*                                                               (see Note #3).
*
*                   (c) Auto-Loading via EEPROM.             If neither options a) or b) are used,
*                                                                the IF layer will use the HW address
*                                                                obtained from the network hardware
*                                                                address registers.
*
*               (6) LPC24xx/LPC17xx errata:
*                   "The TxConsumeIndex register is not updated correctly (from 0 to 1) after the first frame is
*                    sent. After the next frame sent, the TxConsumeIndex register is updated by two (from 0 to 2).
*                    This only happens the very first time, so subsequent updates are correct (even those from 0 to 1,
*                    after wrapping the value to 0 once the value of TxDescriptorNumber has been reached)."
*
*                   (a) Work-around: send a a dummy frame after initialization.
*********************************************************************************************************
*/

static  void  NetDev_StartHandler (NET_IF      *pif,
                                   CPU_INT16U   lpc_dev_type,
                                   NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT08U         *pbuf;
    CPU_INT32U          int_stat;
    CPU_INT16U          reg_to;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


                                                                /* --------------- CFG TX RDY SIGNAL ---------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                       */
                            pdev_cfg->TxDescNbr,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ---- INITIALIZE DEVICE DATA AREA VARIABLES ------- */
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    pdev_data->ErrRxPktCRCCtr        = 0u;
    pdev_data->ErrRxSymbolCtr        = 0u;
    pdev_data->ErrRxNotValidDescCtr  = 0u;
    pdev_data->ErrRxInvalidLenCtr    = 0u;
    pdev_data->ErrRxAlignCtr         = 0u;
    pdev_data->ErrRxOverrunCtr       = 0u;
    pdev_data->ErrRxDataAreaAllocCtr = 0u;
    pdev_data->ErrRxPktDiscarder     = 0u;

    pdev_data->ErrTxNotValidDescCtr  = 0u;
    pdev_data->ErrTxUnderrunCtr      = 0u;
    pdev_data->ErrTxExcCollisionCtr  = 0u;
    pdev_data->ErrTxExcDeferCtr      = 0u;
    pdev_data->ErrTxLateColissionCtr = 0u;
#endif

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->RxPktCtr = 0u;
    pdev_data->TxPktCtr = 0u;
#endif

    pdev_data->TxConsumeIx = pdev->TX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);


                                                                /* ------------------ CFG HW ADDR ------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #4 & #5.                                 */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...   */
                       &hw_addr[0],                             /* ... (see Note #5a).                                */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.  */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Else get  app-configured IF layer HW MAC address,  */
                                                                /* ... if any (see Note #5b).                         */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* If NOT valid, attempt to get device's automatically*/
                                                                /* ... HW MAC address, if any (see Note #5c).         */
            hw_addr[0] = (CPU_INT08U)(pdev->SA2 >> 0);
            hw_addr[1] = (CPU_INT08U)(pdev->SA2 >> 8);
            hw_addr[2] = (CPU_INT08U)(pdev->SA1 >> 0);
            hw_addr[3] = (CPU_INT08U)(pdev->SA1 >> 8);
            hw_addr[4] = (CPU_INT08U)(pdev->SA0 >> 0);
            hw_addr[5] = (CPU_INT08U)(pdev->SA0 >> 8);

            NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,    /* Configure IF layer to use automatically-loaded ... */
                                    (CPU_INT08U *)&hw_addr[0],  /* ... HW MAC address.                                */
                                    (CPU_INT08U  ) sizeof(hw_addr),
                                    (NET_ERR    *) perr);
            if (*perr != NET_IF_ERR_NONE) {                     /* No valid HW MAC address configured, return error.  */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.           */
        pdev->SA2 = ((CPU_INT32U)hw_addr[1] << 8) | ((CPU_INT32U)hw_addr[0] << 0);
        pdev->SA1 = ((CPU_INT32U)hw_addr[3] << 8) | ((CPU_INT32U)hw_addr[2] << 0);
        pdev->SA0 = ((CPU_INT32U)hw_addr[5] << 8) | ((CPU_INT32U)hw_addr[4] << 0);
    }


                                                                /* ----------------- CFG MULTICAST ------------------ */
    pdev->HASH_FILTERL = 0;                                     /* Init hash filter bits to 0.                        */
    pdev->HASH_FILTERH = 0;

#ifdef NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                 */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif


                                                                /* --------------- INIT DMA DESCRIPTORS ------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                         */


    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                         */

    pdesc = (DEV_DESC *)pdev_data->TxDescPtrStart;

    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* ------------------- CFG INT'S -------------------- */
    DEF_BIT_SET(pdev->MAC1, MAC1_RX_EN);                        /* Rx frames will be received.                        */

    DEF_BIT_SET(pdev->COMMAND,(COMMAND_RX_EN |                  /* Enable Tx and Rx datapath                          */
                               COMMAND_TX_EN));

    pbuf = NetBuf_GetDataPtr((NET_IF        *)pif,
                             (NET_TRANSACTION)NET_TRANSACTION_RX,
                             (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                             (NET_BUF_SIZE   )NET_IF_IX_RX,
                             (NET_BUF_SIZE  *)0,
                             (NET_BUF_SIZE  *)0,
                             (NET_BUF_TYPE  *)0,
                             (NET_ERR       *)perr);

    Mem_Clr((void     *)pbuf,
            (CPU_SIZE_T)NET_IF_ETHER_FRAME_MIN_SIZE);


    pdesc[0].PktAddr    = (CPU_INT32U )pbuf;
    pdesc[0].Ctrl       = ((128u & TX_DESC_CTRL_SIZE_MASK) - 1u)
                        | TX_DESC_CTRL_INT_EN
                        | TX_DESC_CTRL_PAD
                        | TX_DESC_CTRL_CRC_EN
                        | TX_DESC_CTRL_LAST
                        | TX_DESC_CTRL_OVERRIDE;

    pdev->TX_PRODUCE_IX = (0u + 1u)
                        % pdev_cfg->TxDescNbr;

    int_stat  = pdev->INT_STAT;
    reg_to    = TX_TO;

    while ((DEF_BIT_IS_CLR(int_stat, INT_FLAG_TX_DONE)) &&
           (reg_to > 0u                              )) {
       int_stat  = pdev->INT_STAT;
       reg_to--;
    }

    pdev->INT_CLR          = DEF_BIT_FIELD(32u, 0u);
    pdev_data->TxConsumeIx = 1u;

    NetBuf_FreeBufDataAreaRx((NET_IF_NBR   )pif->Nbr,
                             (CPU_INT08U  *)pbuf);

    pdev_data->PhyAccessLock = DEF_FALSE;
    switch (lpc_dev_type) {
        case NET_DEV_LPC1758:
             DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);
             DEF_BIT_SET(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDC);
             break;

        case NET_DEV_LPCxxxx:
        default:
             break;
    }

    pdev->INT_EN = INT_FLAG_RX_DONE                             /* Enable Rx done interrupt                           */
                 | INT_FLAG_TX_DONE                             /* Enable Tx done interrupt                           */
                 | INT_FLAG_RX_OVERRUN                          /* Enable Rx overrun interrupt                        */
                 | INT_FLAG_RX_ERR                              /* Enable Rx error interrupt                          */
                 | INT_FLAG_TX_UNDERRUN                         /* Enable Tx overrun interrupt                        */
                 | INT_FLAG_TX_ERR;                             /* Enable Tx Error overrun.                           */

    *perr        = NET_DEV_ERR_NONE;
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
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE    No Error
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
    CPU_INT32U          produce_ix;
    CPU_INT32U          consume_ix;
    DEV_DESC           *pdesc;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    NET_ERR             err;
    LIB_ERR             err_lib;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


                                                                /* ---------------- DISABLE RX & TX ----------------- */
    DEF_BIT_CLR(pdev->MAC1, MAC1_RX_EN);                        /* Rx frames will not be received.                    */
    DEF_BIT_CLR(pdev->COMMAND, (COMMAND_RX_EN |                 /* Disable Tx and Rx datapath                         */
                                COMMAND_TX_EN));
                                                                /* ------------- DISABLE & CLEAR INT'S -------------- */
    DEF_BIT_CLR(pdev->INT_EN, (INT_FLAG_RX_DONE    |            /* Disable Tx and Rx datapath                         */
                               INT_FLAG_TX_DONE    |
                               INT_FLAG_RX_OVERRUN |
                               INT_FLAG_TX_UNDERRUN));
    pdev->INT_CLR  = INT_FLAG_ALL;                              /* Clear all pending interrupts                       */

                                                                /* -------------- FREE RX DESCRIPTORS --------------- */
    NetDev_RxDescFreeAll(pif, perr);

                                                                /* ------------ FREE USED TX DESCRIPTORS ------------ */
    produce_ix = (CPU_INT32U)(pdev->TX_PRODUCE_IX & DEF_BIT_FIELD(16u, 0u));
    consume_ix = (CPU_INT32U)(pdev->TX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u));
    pdesc      = (DEV_DESC *)pdev_data->TxDescPtrStart;

    while (consume_ix != produce_ix) {                          /* Dealloc ALL tx bufs (see Note #2a2).               */
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc[produce_ix].PktAddr, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).       */

        if (produce_ix > 0u) {
            produce_ix--;
        } else {
            produce_ix = pdev->TX_DESC_NBR;
        }
    }

    Mem_PoolBlkFree((MEM_POOL  *)&pdev_data->TxDescPool,
                    (void      *) pdev_data->TxDescPtrStart,
                    (LIB_ERR   *)&err_lib);

    pdev->TX_PRODUCE_IX    = pdev->TX_CONSUME_IX;
    pdev_data->TxConsumeIx = pdev->TX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Decrement frame counter
*                   (b) Determine which receive descriptor caused the interrupt
*                   (c) Obtain pointer to data area to replace existing data area
*                   (d) Reconfigure descriptor with pointer to new data area
*                   (e) Set return values.  Pointer to received data area and size
*                   (f) Update current receive descriptor pointer
*                   (g) Increment statistic counters
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to pointer to received DMA data area. The received data
*                       area address should be returned to the stack by dereferencing
*                       p_data as *p_data = (address of receive data area).
*
*               size    Pointer to size. The number of bytes received should be returned
*                       to the stack by dereferencing size as *size = (number of bytes).
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error
*                           NET_DEV_ERR_RX              Generic Rx error.
*                           NET_DEV_ERR_INVALID_SIZE    Invalid Rx frame size.
*                           NET_BUF error codes         Potential NET_BUF error codes
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pdev_api->Rx()'.
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (2) If a receive error occurs and the descriptor is invalid then the function
*                   SHOULD return 0 for the size, a NULL pointer to the data area AND an
*                   error equal to NET_DEV_ERR_RX.
*
*                   (a) If the next expected ready / valid descriptor is NOT owned by
*                       software, then there is descriptor pointer corruption and the
*                       driver should NOT increment the current receive descriptor
*                       pointer.
*                   (b) If the descriptor IS valid, but an error is indicated within
*                       the descriptor status bits, or length field, then the driver
*                       MUST increment the current receive descriptor pointer and discard
*                       the received frame.
*                   (c) If a new data area is unavailable, the driver MUST increment
*                       the current receive descriptor pointer and discard the received
*                       frame.  This will invoke the DMA to re-use the existing configured
*                       data area.
*
*                (3) The EMAC doesn't distinguish the frame type and frame length,
*                    e.g. when the IP(0x8000) or ARP(0x0806)  packets are received, it compares
*                    the frame type with the max length and gives the "Range" error.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    DEV_RX_STATUS      *pstatus;
    CPU_INT08U         *pbuf;
    CPU_INT32U          consume_ix;
    CPU_INT16U          len;
    CPU_INT32U          status_info;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


    NET_CTR_STAT_INC(pdev_data->RxPktCtr);

                                                                /* ----------- OBTAIN THE CURRENT RX STATUS  -------- */
    consume_ix      =  pdev->RX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);
    pstatus         = (DEV_RX_STATUS *)pdev_data->RxStatusPtrStart;
    pdesc           = (DEV_DESC      *)pdev_data->RxDescPtrStart;
    status_info     = (CPU_INT32U     )pstatus[consume_ix].StatusInfo;


                                                                /* ----------- CHECK FOR RECEIVE ERRORS ------------- */
    if (DEF_BIT_IS_SET_ANY(status_info, RX_STATUS_INFO_ERR_ANY)) {
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)                        /* Increase all the errors counters.                  */

         if (DEF_BIT_IS_SET(status_info, RX_STATUS_INFO_ERR_CRC)) {
             NET_CTR_ERR_INC(pdev_data->ErrRxPktCRCCtr);
         }

         if (DEF_BIT_IS_SET(status_info, RX_STATUS_INFO_ERR_SYMBOL)) {
             NET_CTR_ERR_INC(pdev_data->ErrRxSymbolCtr);
         }

         if (DEF_BIT_IS_SET(status_info, RX_STATUS_INFO_ERR_LEN)) {
             NET_CTR_ERR_INC(pdev_data->ErrRxInvalidLenCtr);
         }

         if (DEF_BIT_IS_SET(status_info, RX_STATUS_INFO_ERR_ALIGN)) {
             NET_CTR_ERR_INC(pdev_data->ErrRxAlignCtr);
         }

         if (DEF_BIT_IS_SET(status_info, RX_STATUS_INFO_ERR_OVERRUN)) {
             NET_CTR_ERR_INC(pdev_data->ErrRxAlignCtr);
         }

         if (DEF_BIT_IS_SET(status_info, RX_STATUS_INFO_ERR_NO_DESC)) {
             NET_CTR_ERR_INC(pdev_data->ErrRxNotValidDescCtr);
         }
#endif

        NetDev_RxConsumeIxInc(pif);

        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;

        *perr   =  NET_DEV_ERR_RX;
         return;
    }

                                                                /* -------------- OBTAIN FRAME LENGTH --------------- */
    len = (status_info & RX_STATUS_INFO_SIZE_MASK) + 1u;
    if (len >  NET_IF_ETHER_FRAME_CRC_SIZE) {
        len -= NET_IF_ETHER_FRAME_CRC_SIZE;
    } else {
        NetDev_RxConsumeIxInc(pif);

       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;

       *perr   =  NET_DEV_ERR_RX;
        return;
    }
                                                                /* -------- OBTAIN PTR TO NEW DMA DATA AREA --------- */
                                                                /* Request an empty buffer.                           */
    pbuf = NetBuf_GetDataPtr((NET_IF        *)pif,
                             (NET_TRANSACTION)NET_TRANSACTION_RX,
                             (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                             (NET_BUF_SIZE   )NET_IF_IX_RX,
                             (NET_BUF_SIZE  *)0,
                             (NET_BUF_SIZE  *)0,
                             (NET_BUF_TYPE  *)0,
                             (NET_ERR       *)perr);

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer (see Note #3c).          */
         NetDev_RxConsumeIxInc(pif);
         NET_CTR_ERR_INC(pdev_data->ErrRxDataAreaAllocCtr);
        *size    = (CPU_INT16U  )0;
        *p_data  = (CPU_INT08U *)0;
         return;
    }

    *size                             =  len;
    *p_data                           = (CPU_INT08U *)pdesc[consume_ix].PktAddr;

    pdesc[consume_ix].PktAddr         = (CPU_INT32U  )pbuf;

    pstatus[consume_ix].StatusInfo    = DEF_BIT_NONE;
    pstatus[consume_ix].StatusHashCRC = DEF_BIT_NONE;

    NetDev_RxConsumeIxInc(pif);

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Check if the transmitter is ready.
*                   (b) Configure the next transmit descriptor for pointer to data and data size.
*                   (c) Issue the transmit command.
*                   (d) Increment pointer to next transmit descriptor
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No Error
*                           NET_DEV_ERR_TX_BUSY     No Tx descriptors available
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
*               (3) Care should be taken to avoid skipping transmit descriptors while selecting
*                   the next available descriptor.  Software MUST track the descriptor which
*                   is expected to generate the next transmit complete interrupt.  Skipping
*                   descriptors, unless carefully accounted for, may make it difficult to
*                   know which descriptor will complete transmission next.  Some device
*                   drivers may find it useful to adjust pdev_data->TxBufDescCompPtr after
*                   having selected the next available transmit descriptor.
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
    DEV_DESC           *pdesc;
    CPU_INT32U          consume_ix;
    CPU_INT32U          produce_ix;
    CPU_INT32U          produce_next_ix;
    CPU_SR_ALLOC();


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */
    pdesc     = (DEV_DESC          *)pdev_data->TxDescPtrStart;

    CPU_CRITICAL_ENTER();
    consume_ix             = pdev->TX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);
    produce_ix             = pdev->TX_PRODUCE_IX & DEF_BIT_FIELD(16u, 0u);
    pdev_data->TxConsumeIx = consume_ix;                        /* Save the Consume index                             */
    CPU_CRITICAL_EXIT();

    produce_next_ix = (produce_ix + 1u)
                    % pdev_cfg->TxDescNbr;

    if (produce_next_ix == consume_ix) {
       *perr = NET_DEV_ERR_TX_BUSY;
        NET_CTR_ERR_INC(pdev_data->ErrTxNotValidDescCtr);
        return;
    }

    pdesc[produce_ix].PktAddr = (CPU_INT32U )p_data;
    pdesc[produce_ix].Ctrl    = ((size & TX_DESC_CTRL_SIZE_MASK) - 1u)
                              | TX_DESC_CTRL_INT_EN
                              | TX_DESC_CTRL_PAD
                              | TX_DESC_CTRL_CRC_EN
                              | TX_DESC_CTRL_LAST
                              | TX_DESC_CTRL_OVERRIDE;

    pdev->TX_PRODUCE_IX       = produce_next_ix;

    *perr                     = NET_DEV_ERR_NONE;

}


/*
*********************************************************************************************************
*                                       NetDev_AddrMulticastAdd()
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
*                   (a) Perfect        filtering of ONE multicast address.
*                   (b) Imperfect hash filtering of 64  multicast addresses.
*                   (c) Accept     all multicast.
*
*               (2) This function implements the filtering mechanism described in 1b.
*
*               (3) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U *)paddr_hw,         /* Obtain CRC without compliment.                     */
                                (CPU_INT32U  )addr_hw_len,
                                (NET_ERR    *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    crc = NetUtil_32BitReflect(crc);

    hash    = (crc >> 23u) & 0x3F;                              /* Obtain hash from CRC[28:23].                       */
    bit_sel = (hash % 32u);                                     /* Obtain hash reg bit.                               */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                  */

    if (hash <= 31u) {                                          /* Set multicast hash reg bit.                        */
        pdev->HASH_FILTERL |= (1 << bit_sel);
    } else {
        pdev->HASH_FILTERH |= (1 << bit_sel);
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
#endif

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetDev_AddrMulticastRemove()
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
*                   (a) Perfect        filtering of ONE multicast address.
*                   (b) Imperfect hash filtering of 64  multicast addresses.
*                   (c) Accept     all multicast.
*
*               (2) This function implements the filtering mechanism described in 1b.
*
*               (3) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U *)paddr_hw,         /* Obtain CRC without compliment.                     */
                                (CPU_INT32U  )addr_hw_len,
                                (NET_ERR    *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    crc = NetUtil_32BitReflect(crc);

    hash    = (crc >> 23u) & 0x3F;                              /* Obtain hash from CRC[28:23].                       */
    bit_sel = (hash % 32u);                                     /* Obtain hash reg bit.                               */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1u) {                                /* If hash bit reference ctr not zero.                */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                  */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0u;                                       /* Zero hash bit references remaining.                */

    if (hash <= 31u) {                                          /* Set multicast hash reg bit.                        */
        pdev->HASH_FILTERL &= ~(1 << bit_sel);
    } else {
        pdev->HASH_FILTERH &= ~(1 << bit_sel);
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
#endif

   *perr = NET_DEV_ERR_NONE;
}

/*
*********************************************************************************************************
*                                        NetDev_ISR_Handler()
*
* Description : This function serves as a generic Interrupt Service Routine handler. This ISR handler
*               will service all necessary ISR events for the network interface controller.
*
* Argument(s) : pif     Pointer to interface requiring service.
*               ---
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
* Caller(s)   : NetBSP_ISR_Handler().     NET BSP code for handling a group of ISR vectors.
*
* Note(s)     : (1) This function is called from the context of an ISR.
*               (2) The interface number is known at the time of BSP ISR code authoring and
*                   must be converted to a pointer to the interface from within the ISR handler
*                   instead of the lower layer BSP code that calls this ISR Handler.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE  type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    DEV_TX_STATUS      *pstatus;
    CPU_INT08U         *pdata;
    CPU_INT32U          reg_val;
    CPU_INT32U          int_stat;
    CPU_INT32U          consume_ix;
    CPU_INT32U          produce_ix;
    NET_ERR             err;


   (void)&type;                                                 /* Prevent 'variable unused' compiler warning.        */

                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */

    int_stat  = pdev->INT_STAT;
    int_stat &= pdev->INT_EN;
                                                                /* ----------------- HANDLE RX ISRs ----------------- */
    if (DEF_BIT_IS_SET(int_stat, INT_FLAG_RX_DONE)) {
        DEF_BIT_CLR(pdev->INT_EN, INT_FLAG_RX_DONE | INT_FLAG_RX_OVERRUN);
        pdev->INT_CLR = INT_FLAG_RX_DONE;
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task of new frame                */
    }

                                                                /* ----------------- HANDLE TX ISRs ----------------- */
    if (DEF_BIT_IS_SET_ANY(int_stat, INT_FLAG_TX_DONE | INT_FLAG_TX_ERR)) {
        pdev->INT_CLR = int_stat & (INT_FLAG_TX_DONE | INT_FLAG_TX_ERR);
        pdesc         = (DEV_DESC *)pdev_data->TxDescPtrStart;
        pstatus       = (DEV_TX_STATUS *)pdev_data->TxStatusPtrStart;
        produce_ix    = pdev->TX_PRODUCE_IX & DEF_BIT_FIELD(16u, 0u);
        consume_ix    = pdev->TX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);

        while (pdev_data->TxConsumeIx != consume_ix) {
            pdata                   = (CPU_INT08U    *)pdesc[pdev_data->TxConsumeIx].PktAddr;
            reg_val                 = (CPU_INT32U     )pstatus[pdev_data->TxConsumeIx].StatusInfo;
            pdev_data->TxConsumeIx  = (pdev_data->TxConsumeIx + 1u)
                                    % (pdev_cfg->TxDescNbr);

            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdata, &err);
            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available      */

#if (NET_CTR_CFG_STAT_EN  == DEF_ENABLED)
            if (DEF_BIT_IS_CLR(reg_val, TX_STATUS_INFO_ERR_OR)) {
                NET_CTR_STAT_INC(pdev_data->TxPktCtr);
            }
#endif

#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
            if (DEF_BIT_IS_SET(reg_val, TX_STATUS_INFO_ERR_OR)) {

                if (DEF_BIT_IS_SET(reg_val, TX_STATUS_INFO_ERR_EXC_DEFER)) {
                    NET_CTR_STAT_INC(pdev_data->ErrTxExcDeferCtr);
                }

                if (DEF_BIT_IS_SET(reg_val, TX_STATUS_INFO_ERR_EXC_COL)) {
                    NET_CTR_STAT_INC(pdev_data->ErrTxExcCollisionCtr);
                }

                if (DEF_BIT_IS_SET(reg_val, TX_STATUS_INFO_ERR_LATE_COL)) {
                    NET_CTR_STAT_INC(pdev_data->ErrTxLateColissionCtr);
                }

                if (DEF_BIT_IS_SET(reg_val, TX_STATUS_INFO_ERR_UNDER)) {
                    NET_CTR_STAT_INC(pdev_data->ErrTxUnderrunCtr);
                }

                if (DEF_BIT_IS_SET(reg_val, TX_STATUS_INFO_ERR_NO_DESC)) {
                    NET_CTR_STAT_INC(pdev_data->ErrTxNotValidDescCtr);
                }
            }
#endif
        }
    }

    if (DEF_BIT_IS_SET(int_stat, INT_FLAG_RX_ERR)) {
        pdev->INT_CLR = INT_FLAG_RX_ERR;
        reg_val       = pdev->RSV;
        if (DEF_BIT_IS_CLR(reg_val, RSV_BIT_RX_OK)) {
            NET_CTR_ERR_INC(pdev_data->ErrRxPktDiscarder);
        }
    }

                                                                /* ------------ HANDLE TX UNDERRUN ISRs ------------- */
    if (DEF_BIT_IS_SET(int_stat, INT_FLAG_TX_UNDERRUN)) {
        pdev->INT_CLR = INT_FLAG_TX_UNDERRUN;

        DEF_BIT_CLR(pdev->COMMAND, COMMAND_TX_EN);              /* Disable Tx datapath                                */

        pdesc         = (DEV_DESC *)pdev_data->TxDescPtrStart;
        produce_ix    = pdev->TX_PRODUCE_IX & DEF_BIT_FIELD(16u, 0u);
        consume_ix    = pdev->TX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);
                                                                /* Free all the Tx buffers                            */
        while (pdev_data->TxConsumeIx != produce_ix) {
            pdata                   = (CPU_INT08U *)pdesc[pdev_data->TxConsumeIx].PktAddr;

            pdev_data->TxConsumeIx  = (pdev_data->TxConsumeIx + 1u)
                                    % (pdev_cfg->TxDescNbr);

            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdata, &err);

            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available      */

            NET_CTR_STAT_INC(pdev_data->TxPktCtr);
        }

        DEF_BIT_SET(pdev->COMMAND, COMMAND_TX_RST);             /* Reset TX datapath                                    */

        NET_CTR_ERR_INC(pdev_data->ErrTxUnderrunCtr);           /* Increment the underrun counter                       */

        pdev->TX_DESC_ADDR  = ((CPU_INT32U)pdev_data->TxDescPtrStart);
        pdev->TX_STAT_ADDR  = ((CPU_INT32U)pdev_data->TxStatusPtrStart);
        pdev->TX_DESC_NBR   = ((CPU_INT32U)pdev_cfg->TxDescNbr) - 1;
        pdev->TX_PRODUCE_IX = 0;
        DEF_BIT_SET(pdev->COMMAND, COMMAND_TX_EN);              /* Enable Tx datapath                                   */
    }

                                                                /* ------------ HANDLE RX OVERRUN ISRs -------------- */
    if (DEF_BIT_IS_SET(int_stat, INT_FLAG_RX_OVERRUN)) {
        NetDev_RxDescFreeAll(pif, &err);                        /* Free all the Rx descriptors                        */
        NetDev_RxDescInit(pif, &err);                           /* Re-Initialize the Descriptors chain                */

        NET_CTR_ERR_INC(pdev_data->ErrRxOverrunCtr);            /* Increment Overrun counter                          */

        DEF_BIT_SET(pdev->COMMAND, COMMAND_RX_EN);              /* Enable the receive function                        */
        pdev->INT_CLR = INT_FLAG_RX_OVERRUN;                    /* Clear the overrun interrupt                        */
        DEF_BIT_SET(pdev->INT_EN, INT_FLAG_RX_DONE | INT_FLAG_RX_OVERRUN);
    }
}


/*
*********************************************************************************************************
*                                            NetDev_IO_Ctrl()
*
* Description : This function implements various hardware functions.
*
* Argument(s) : pif     Pointer to interface requiring service.
*
*               opt     Option code representing desired function to perform. The Network Protocol Suite
*                       specifies the option codes below. Additional option codes may be defined by the
*                       driver developer in the driver's header file.
*                           NET_DEV_LINK_STATE_GET
*                           NET_DEF_LINK_STATE_UPDATE
*
*                       Driver defined operation codes MUST be defined starting from 20 or higher
*                       to prevent clashing with the pre-defined operation code types. See net_dev_nnn.h
*                       for more details.
*
*               data    Pointer to optional data for either sending or receiving additional function
*                       arguments or return data.
*
*               perr    Pointer to return error code.
*                           NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid option number specified.
*                           NET_ERR_FAULT_NULL_FNCT             Null interface function pointer encountered.
*
*                           NET_DEV_ERR_NONE                    IO Ctrl operation completed successfully.
*                           NET_DEV_ERR_NULL_PTR                Null argument pointer passed.
*
*                           NET_PHY_ERR_NULL_PTR                Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_TIMEOUT_RESET           Phy reset          time-out.
*                           NET_PHY_ERR_TIMEOUT_AUTO_NEG        Auto-Negotiation   time-out.
*                           NET_PHY_ERR_TIMEOUT_REG_RD          Phy register read  time-out.
*                           NET_PHY_ERR_TIMEOUT_REG_WR          Phy register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IO_CtrlHandler() via 'pdev_api->IO_Ctrl()',
*               NetPhy_LinkStateGet()        via 'pdev_api->IO_Ctrl()'.
*
* Note(s)     : (1) In order to get the Phy's Link state the SPEED bit in the SUPP register has to set or clear
*                   according to the link speed otherwise the Phy link will never be up.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *pif,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *perr)
{


    NET_DEV_CFG_ETHER   *pdev_cfg;
    NET_DEV_LINK_ETHER  *plink_state;
    NET_PHY_API_ETHER   *pphy_api;
    NET_DEV             *pdev;

                                                                /* ------ OBTAIN REFERENCE TO DEVICE REGISTERS ------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                  */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.    */

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
             switch (plink_state->Duplex) {                     /* Update duplex setting on device.                   */
                case NET_PHY_DUPLEX_FULL:
                     pdev->MAC2    |= MAC2_DUPLEX_FULL;         /* Full Duplex Mode.                                  */
                     pdev->COMMAND |= COMMAND_DUPLEX_FULL_EN;
                     pdev->IPGT     = IPGT_DUPLEX_FULL_VAL;
                     break;

                case NET_PHY_DUPLEX_HALF:
                     pdev->MAC2    &= ~MAC2_DUPLEX_FULL;        /* Half Duplex Mode.                                  */
                     pdev->COMMAND &= ~COMMAND_DUPLEX_FULL_EN;
                     pdev->IPGT     = IPGT_DUPLEX_HALF_VAL;
                     break;

                default:
                     break;
             }

             switch (plink_state->Spd) {                        /* Update speed setting on device.                    */
                case NET_PHY_SPD_10:
                     pdev->SUPP  &= ~SUPP_PHY_RMII_SPD_100;
                     break;

                case NET_PHY_SPD_100:
                     pdev->SUPP  |= SUPP_PHY_RMII_SPD_100;
                     break;

                case NET_PHY_SPD_1000:
                default:
                     break;
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
*                                       NetDev_LPCxxxx_MII_Rd()
*
* Description : Read data over the (R)MII bus from the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr        Pointer to return error code :
*
*                           NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPCxxxx_MII_Rd (NET_IF      *pif,
                                     CPU_INT08U   phy_addr,
                                     CPU_INT08U   reg_addr,
                                     CPU_INT16U  *p_data,
                                     NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          reg_to;


                                                                /* ------ OBTAIN REFERENCE TO DEVICE REGISTERS ------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                  */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.    */

                                                                /* ----------- PERFORM MII READ OPERATION ----------- */
                                                                /* Wait for the MII bus controller to become ready.   */
    reg_to = MII_TO;

    while ((DEF_BIT_IS_SET(pdev->MIND, MIND_BUSY)) &&
           (reg_to > 0)) {
        reg_to--;
    }

    if (reg_to == 0) {                                          /* Return an error if a timeout occurred.             */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    pdev->MCFG &= ~MCFG_SCAN_INCREMENT;
    pdev->MADR  = ((phy_addr << 8) & MADR_PHY_ADDR_MASK)
                | ((reg_addr << 0) & MADR_REG_ADDR_MASK);
    pdev->MCMD  = MCMD_RD;

    reg_to  = MII_TO;
                                                                /* Wait for the MII bus controller to become ready.   */
    while ((DEF_BIT_IS_SET(pdev->MIND, MIND_BUSY)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {                                         /* Return an error if a timeout occurred.             */
       *p_data = 0u;
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    pdev->MCMD = DEF_BIT_NONE;
   *p_data     = pdev->MRDD;

   *perr       = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_LPCxxxx_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               data        Data to write to the specified PHY register.
*
*               perr        Pointer to return error code :
*
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPCxxxx_MII_Wr (NET_IF      *pif,
                                     CPU_INT08U   phy_addr,
                                     CPU_INT08U   reg_addr,
                                     CPU_INT16U   reg_data,
                                     NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          reg_to;


                                                                /* ------ OBTAIN REFERENCE TO DEVICE REGISTERS ------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                  */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.    */

                                                                /* ----------- PERFORM MII WRITE OPERATION ---------- */
    reg_to  = MII_TO;
                                                                /* Wait for the MII bus controller to become ready.   */
    while ((DEF_BIT_IS_SET(pdev->MIND, MIND_BUSY)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {                                         /* Return an error if a timeout occurred.             */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    pdev->MADR  = ((phy_addr << 8) & MADR_PHY_ADDR_MASK)
                | ((reg_addr << 0) & MADR_REG_ADDR_MASK);


    pdev->MCMD = MCMD_WR;
    pdev->MWTD = (CPU_INT32U)reg_data;

    reg_to     = MII_TO;
                                                                /* Wait for the MII bus controller to become ready.   */
    while ((DEF_BIT_IS_SET(pdev->MIND, MIND_BUSY)) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {                                         /* Return an error if a timeout occurred.             */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_LPC1758_MII_Rd()
*
* Description : Read data over the (R)MII bus from the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr        Pointer to return error code :
*
*                           NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPC1758_MII_Rd (NET_IF      *pif,
                                     CPU_INT08U   phy_addr,
                                     CPU_INT08U   reg_addr,
                                     CPU_INT16U  *p_data,
                                     NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          mdio_frame;
    CPU_INT08U          mdio_frame_bit;
    CPU_INT08U          clk_cycles;
    CPU_BOOLEAN         status;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg   = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;             /* Obtain ptr to the dev cfg struct.                    */
    pdev       = (NET_DEV           *)pdev_cfg->BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */
    pdev_data  = (NET_DEV_DATA      *)pif->Dev_Data;            /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();

    if (pdev_data->PhyAccessLock == DEF_TRUE) {
        CPU_CRITICAL_EXIT();
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    pdev_data->PhyAccessLock = DEF_TRUE;                        /* Lock MIIM access.                                    */
    CPU_CRITICAL_EXIT();

    DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);

    status = NetDev_LPC1758_MII_Clk(pdev);                      /* Perfrom IDLE cycle                                   */

    if (status == DEF_FAIL) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    DEF_BIT_SET(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);
    LPC17XX_GPIO_REG_FIOSETx(2u) = LPC17XX_GPIO_PIN_MDIO;

                                                                /* Perform the MDIO preamble for syncronization.        */
    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        status = NetDev_LPC1758_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
            return;
        }
    }

    mdio_frame     = LPC17XX_BIT_MDIO_START                     /* Build the MDIO frame.                                */
                   | LPC17XX_BIT_MDIO_OP_RD
                   | ((phy_addr & LPC17XX_MSK_MDIO_PHY_ADDR ) << 23u)
                   | ((reg_addr & LPC17XX_MSK_MDIO_REG_ADDR ) << 18u);

    mdio_frame_bit = 31u;

    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        if (mdio_frame_bit == 17u) {
                                                                /* Start Read cycle.                                    */
            DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);
        }

        if (mdio_frame_bit >= 16u) {
            if (DEF_BIT_IS_SET(mdio_frame, DEF_BIT(mdio_frame_bit))) {
                LPC17XX_GPIO_REG_FIOSETx(2u) = LPC17XX_GPIO_PIN_MDIO;
            } else {
                LPC17XX_GPIO_REG_FIOCLRx(2u) = LPC17XX_GPIO_PIN_MDIO;
            }
        } else {
            if (DEF_BIT_IS_SET(LPC17XX_GPIO_REG_FIOPINx(2u), LPC17XX_GPIO_PIN_MDIO)) {
                DEF_BIT_SET(mdio_frame, DEF_BIT(mdio_frame_bit));
            }
        }
        status = NetDev_LPC1758_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
            return;
        }
        mdio_frame_bit--;
    }

    DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);

    CPU_CRITICAL_ENTER();

    pdev_data->PhyAccessLock = DEF_FALSE;

    CPU_CRITICAL_EXIT();

    *p_data = (CPU_INT16U )mdio_frame & DEF_INT_16_MASK;
    *perr   = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_LPC1758_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               data        Data to write to the specified PHY register.
*
*               perr        Pointer to return error code :
*
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPC1758_MII_Wr (NET_IF      *pif,
                                     CPU_INT08U   phy_addr,
                                     CPU_INT08U   reg_addr,
                                     CPU_INT16U   reg_data,
                                     NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          mdio_frame;
    CPU_INT08U          mdio_frame_bit;
    CPU_INT08U          clk_cycles;
    CPU_BOOLEAN         status;
    CPU_SR_ALLOC();

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();

    if (pdev_data->PhyAccessLock == DEF_TRUE) {
        CPU_CRITICAL_EXIT();
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    pdev_data->PhyAccessLock = DEF_TRUE;                        /* Lock MIIM access.                                    */
    CPU_CRITICAL_EXIT();


    DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);

    status = NetDev_LPC1758_MII_Clk(pdev);                      /* Perfrom IDLE cycle                                   */

    if (status == DEF_FAIL) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }


    DEF_BIT_SET(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);
    LPC17XX_GPIO_REG_FIOSETx(2u) = LPC17XX_GPIO_PIN_MDIO;

                                                                /* Perform the MDIO preamble for syncronization.        */
    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        status = NetDev_LPC1758_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
            return;
        }
    }

    mdio_frame = LPC17XX_BIT_MDIO_START                         /* Build the MDIO frame.                                */
               | LPC17XX_BIT_MDIO_OP_WR
               | LPC17XX_BIT_MDIO_OP_TA_WR
               | ((phy_addr & LPC17XX_MSK_MDIO_PHY_ADDR) << 23u)
               | ((reg_addr & LPC17XX_MSK_MDIO_REG_ADDR) << 18u)
               | ((reg_data & LPC17XX_MSK_MDIO_DATA    )       );

    mdio_frame_bit = 31u;

    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        if (DEF_BIT_IS_SET(mdio_frame, DEF_BIT(mdio_frame_bit))) {
            LPC17XX_GPIO_REG_FIOSETx(2u) = LPC17XX_GPIO_PIN_MDIO;
        } else {
            LPC17XX_GPIO_REG_FIOCLRx(2u) = LPC17XX_GPIO_PIN_MDIO;
        }

        status = NetDev_LPC1758_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
            return;
        }

        mdio_frame_bit--;
    }

    DEF_BIT_CLR(LPC17XX_GPIO_REG_FIODIRx(2u), LPC17XX_GPIO_PIN_MDIO);

    CPU_CRITICAL_ENTER();

    pdev_data->PhyAccessLock = DEF_FALSE;

    CPU_CRITICAL_EXIT();

    *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_RxDescInit()
*
* Description : (1) This function initializes the Rx descriptor list for the specified interface :
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Rx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
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
    DEV_DESC           *pdesc;
    DEV_RX_STATUS      *pstatus;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbr_bytes;
    CPU_INT16U          i;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


                                                                /* ------------- ALLOCATE DESCRIPTORS  -------------- */
    nbr_bytes                   =  pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdev_data->RxDescPtrStart   = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL   *)&pdev_data->RxDescPool,
                                                             (CPU_SIZE_T  ) nbr_bytes,
                                                             (LIB_ERR    *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
    }

    nbr_bytes                   =  pdev_cfg->RxDescNbr * sizeof(DEV_RX_STATUS);
    pdev_data->RxStatusPtrStart = (DEV_RX_STATUS *)Mem_PoolBlkGet((MEM_POOL   *)&pdev_data->RxStatusPool,
                                                                  (CPU_SIZE_T  ) nbr_bytes,
                                                                  (LIB_ERR    *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
    }


    pdesc   =  pdev_data->RxDescPtrStart;
    pstatus =  pdev_data->RxStatusPtrStart;

    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Initialize Rx descriptor list                      */
        pdesc->PktAddr = (CPU_INT32U)NetBuf_GetDataPtr((NET_IF        *)pif,
                                                       (NET_TRANSACTION)NET_TRANSACTION_RX,
                                                       (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                                       (NET_BUF_SIZE   )NET_IF_IX_RX,
                                                       (NET_BUF_SIZE  *)0,
                                                       (NET_BUF_SIZE  *)0,
                                                       (NET_BUF_TYPE  *)0,
                                                       (NET_ERR       *)perr);
        if (*perr != NET_BUF_ERR_NONE) {
             return;
        }

        pdesc->Ctrl         = ((pdev_cfg->RxBufLargeSize & RX_DESC_CTRL_SIZE_MASK) - 1)
                            |  RX_DESC_CTRL_INT_EN;

        pstatus->StatusInfo = DEF_BIT_NONE;
        pdesc++;
        pstatus++;
    }

    DEF_BIT_SET(pdev->MAC1   , MAC1_RX_MCS_RST);
    DEF_BIT_CLR(pdev->COMMAND, COMMAND_RX_EN);
    DEF_BIT_SET(pdev->COMMAND, COMMAND_RX_RST);
    DEF_BIT_CLR(pdev->MAC1   , MAC1_RX_MCS_RST);

    pdev->RX_DESC_ADDR     = ((CPU_INT32U)pdev_data->RxDescPtrStart);
    pdev->RX_STAT_ADDR     = ((CPU_INT32U)pdev_data->RxStatusPtrStart);
    pdev->RX_DESC_NBR      = ((CPU_INT32U)pdev_cfg->RxDescNbr) - 1;
    pdev->RX_CONSUME_IX    = pdev->RX_PRODUCE_IX;

    *perr                  = NET_DEV_ERR_NONE;
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
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
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
    DEV_DESC           *pdesc;
    LIB_ERR             lib_err;
    CPU_INT16U          i;
    CPU_INT08U         *pdesc_data;


                                                                /* ------ OBTAIN REFERENCE TO DEVICE CFG/DATA ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */

                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pdesc = pdev_data->RxDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */
        pdesc_data = (CPU_INT08U *)(pdesc->PktAddr);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }
                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree(&pdev_data->RxDescPool,                     /* Return Rx descriptor block to Rx descriptor pool.    */
                     pdev_data->RxDescPtrStart,
                    &lib_err);

    Mem_PoolBlkFree(&pdev_data->RxStatusPool,                   /* Return Rx Status block to Rx status pool.            */
                     pdev_data->RxStatusPtrStart,
                    &lib_err);

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_RxConsumeIxInc()
*
* Description : (1) Increment consumer index :
*
*                   (a) Post to receive task
*                   (b) Re-enable receive interrupts
*
* Argument(s) : pif     Pointer to interface requiring service.
*               ---
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_RxConsumeIxInc (NET_IF  *pif)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          consume_ix;
    CPU_INT32U          consume_ix_next;
    CPU_INT32U          produce_ix;
    NET_ERR             err;

                                                                /* ------ OBTAIN REFERENCE TO DEVICE REGISTERS ------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                  */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.    */

                                                                /* ------------- DECREMENT FRAME CNT ---------------- */
                                                                /* Current consume index.                             */
    consume_ix      =  pdev->RX_CONSUME_IX & DEF_BIT_FIELD(16u, 0u);
    consume_ix_next = (consume_ix + 1u) % pdev_cfg->RxDescNbr;  /* Next consume index.                                */

    pdev->RX_CONSUME_IX = consume_ix_next;                      /* Update consume index.                              */
    pdev->INT_CLR       = INT_FLAG_RX_DONE;
    produce_ix          = pdev->RX_PRODUCE_IX & DEF_BIT_FIELD(16u, 0u);;

    if (consume_ix_next != produce_ix) {
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task of new frame                */
        if (err != NET_IF_ERR_NONE) {
            DEF_BIT_SET(pdev->INT_EN, INT_FLAG_RX_DONE);
        }
    } else {
        DEF_BIT_SET(pdev->INT_EN, INT_FLAG_RX_DONE | INT_FLAG_RX_OVERRUN);
    }
}


/*
*********************************************************************************************************
*                                          NetDev_TxDescInit()
*
* Description : (1) This function initializes the Tx descriptor list for the specified interface :
*
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Tx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start().
*
* Note(s)     : (2) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (3) All Tx descriptors are allocated as ONE memory block.  This removes the
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

static  void  NetDev_TxDescInit (NET_IF  *pif,
                                 NET_ERR *perr)

{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    NET_DEV            *pdev;
    LIB_ERR             lib_err;
    CPU_INT32U          nbr_bytes;
    CPU_INT32U          i;


                                                                /* - OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                  */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                       */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.    */


    nbr_bytes                 =  pdev_cfg->TxDescNbr * sizeof(DEV_DESC);
    pdev_data->TxDescPtrStart = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL   *)&pdev_data->TxDescPool,
                                                           (CPU_SIZE_T  ) nbr_bytes,
                                                           (LIB_ERR    *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    pdesc = pdev_data->TxDescPtrStart;

    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Initialize Tx descriptor array                     */
        pdesc->PktAddr = 0;
        pdesc->Ctrl    = 0;
        pdesc++;
    }

                                                                /* ------- ALLOCATE SPACE FOR TX STATUS ARRAY ------- */
    nbr_bytes                    =  pdev_cfg->TxDescNbr * sizeof(DEV_TX_STATUS);

    pdev_data->TxStatusPtrStart = (DEV_TX_STATUS *)Mem_PoolBlkGet((MEM_POOL   *)&pdev_data->TxStatusPool,
                                                                  (CPU_SIZE_T  ) nbr_bytes,
                                                                  (LIB_ERR    *)&lib_err);

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
    }

    DEF_BIT_SET(pdev->MAC1   , MAC1_TX_MCS_RST);
    DEF_BIT_CLR(pdev->COMMAND, COMMAND_TX_EN);
    DEF_BIT_SET(pdev->COMMAND, COMMAND_TX_RST);
    DEF_BIT_CLR(pdev->MAC1   , MAC1_TX_MCS_RST);

    pdev->TX_DESC_ADDR  = ((CPU_INT32U)pdev_data->TxDescPtrStart);
    pdev->TX_STAT_ADDR  = ((CPU_INT32U)pdev_data->TxStatusPtrStart);
    pdev->TX_DESC_NBR   = ((CPU_INT32U)pdev_cfg->TxDescNbr) - 1u;
    pdev->TX_PRODUCE_IX = 0u;

    *perr               = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetDev_LPC1758_MII_Clk()
*
* Description : Perform a MDIO clk cycle.
*
* Argument(s) : pdev    Pointer to the interface device registers.
*
* Return(s)   : DEF_OK,   If the clock cycle was performed correctly,
*               DEF_FAIL, Otherwise.
*
* Caller(s)   : NetDev_LPC1758_MII_Rd(),
*               NetDev_LPC1758_MII_Wr().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetDev_LPC1758_MII_Clk (NET_DEV  *pdev)
{
    NET_DEV_TS          ts_start;
    NET_DEV_TS          ts_cur;
    NET_DEV_TS          ts_delta;
    NET_DEV_TS          ts_clk_period;
    CPU_TS_TMR_FREQ     ts_tmr_freq;
    CPU_ERR             cpu_err;
    CPU_SIZE_T          reg_to;


   (void)&pdev;                                                 /* Prevent 'variable unused' compiler warning.          */

    ts_tmr_freq = CPU_TS_TmrFreqGet(&cpu_err);                  /* Get the current TS timer frequency.                  */

    if (cpu_err != CPU_ERR_NONE) {
        return (DEF_FAIL);
    }

    ts_clk_period = (ts_tmr_freq / LPC17XX_VAL_MDIO_MIN_FREQ);  /* Calculate the PHY period in TS units.                */

    if (ts_clk_period == 0u) {                                  /* If 'LPC17XX_VAL_MDIO_MIN_FREQ > ts_tmr_freq' ...     */
        return (DEF_FAIL);                                      /* ... return an error.                                 */
    }

    reg_to   = MII_TO;
    ts_start = NET_DEV_TS_GET();                                /* Get current TS value.                                */
    ts_cur   = NET_DEV_TS_GET();
    ts_delta = ts_cur - ts_start;

    LPC17XX_GPIO_REG_FIOCLRx(2u) = LPC17XX_GPIO_PIN_MDC;        /* Set the Clk to '0'.                                  */

    while ((ts_delta < (ts_clk_period / 2u)) &&                 /* Wait for half-period.                                */
           (reg_to   > 0u                 )) {
        ts_cur   = NET_DEV_TS_GET();
        ts_delta = (ts_cur - ts_start);
        reg_to--;
    }

    if (reg_to == 0u) {
        return (DEF_FAIL);
    }

    LPC17XX_GPIO_REG_FIOSETx(2u) = LPC17XX_GPIO_PIN_MDC;        /* Set the Clk to '1'.                                  */
                                                                /* Wait for half-period.                                */
    reg_to   = MII_TO;
    ts_start = NET_DEV_TS_GET();                                /* Get current TS value.                                */
    ts_cur   = NET_DEV_TS_GET();
    ts_delta = ts_cur - ts_start;

    while ((ts_delta < (ts_clk_period / 2u)) &&
           (reg_to   >  0u                )) {
        ts_cur   = NET_DEV_TS_GET();
        ts_delta = ts_cur - ts_start;
        reg_to--;
    }

    if (reg_to == 0u) {
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


#endif /* NET_IF_ETHER_MODULE_EN */

