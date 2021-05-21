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
*                                               STR912
*
* Filename : net_dev_str912.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_STR912_MODULE
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_str912.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_TIMEOUT                              0x0004FFFF    /* MII rd/wr timeout.                                   */

#define  VLAN_FILTER_VLTAG                        0x00000000    /* VLAN filtering modes.                                */
#define  VLAN_FILTER_VLTAG_VLID                   0x00008000

#define  VLANID1                                      0x0000    /* VLAN 1                                               */
#define  VLANTAG1                                     0x8100

#define  VLANID2                                      0x0000    /* VLAN 2                                               */
#define  VLANTAG2                                     0x8100


/*
*********************************************************************************************************
*                                      REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

#define  ENET_MCR_RA                              0x80000000    /*  Received All.                                       */
#define  ENET_MCR_EN                              0x40000000    /*  Endianity.                                          */
#define  ENET_MCR_PS                              0x03000000    /*  Prescaler bits.                                     */
#define  ENET_MCR_DRO                             0x00800000    /*  Disable Receive Own.                                */
#define  ENET_MCR_LM                              0x00200000    /*  Loopback Mode.                                      */
#define  ENET_MCR_FDM                             0x00100000    /*  Full Duplex Mode.                                   */
#define  ENET_MCR_AFM                             0x000E0000    /*  Address Filtering Mode.                             */
#define  ENET_MCR_PWF                             0x00010000    /*  Pass Wrong Frame.                                   */
#define  ENET_MCR_VFM                             0x00008000    /*  VLAN Filtering Mode.                                */
#define  ENET_MCR_ELC                             0x00001000    /*  Enable Late Collision.                              */
#define  ENET_MCR_DBF                             0x00000800    /*  Disable Broadcast Frame.                            */
#define  ENET_MCR_DPR                             0x00000400    /*  Disable Packet Retry.                               */
#define  ENET_MCR_RVFF                            0x00000200    /*  VCI Rx Frame Filtering.                             */
#define  ENET_MCR_APR                             0x00000100    /*  Automatic Pad Removal.                              */
#define  ENET_MCR_BL                              0x000000C0    /*  Back-off Limit.                                     */
#define  ENET_MCR_DCE                             0x00000020    /*  Deferral Check Enable.                              */
#define  ENET_MCR_RVBE                            0x00000010    /*  Reception VCI Burst Enable.                         */
#define  ENET_MCR_TE                              0x00000008    /*  Transmission Enable.                                */
#define  ENET_MCR_RE                              0x00000004    /*  Reception Enable.                                   */
#define  ENET_MCR_RCFA                            0x00000001    /*  Reverse Control Frame Address.                      */

#define  ENET_MTS_PR                              0x80000000    /*  Packet Retry.                                       */
#define  ENET_MTS_BC                              0x7FFC0000    /*  Byte Count.                                         */
#define  ENET_MTS_CC                              0x00003C00    /*  Collision Count.                                    */
#define  ENET_MTS_LCO                             0x00000200    /*  Late Collision Observed.                            */
#define  ENET_MTS_DEF                             0x00000100    /*  Defferd.                                            */
#define  ENET_MTS_UR                              0x00000080    /*  Under Run.                                          */
#define  ENET_MTS_EC                              0x00000040    /*  Excessive Collision.                                */
#define  ENET_MTS_LC                              0x00000020    /*  Late Collision.                                     */
#define  ENET_MTS_ED                              0x00000010    /*  Excessive Deferral.                                 */
#define  ENET_MTS_LOC                             0x00000008    /*  Loss of Carrier.                                    */
#define  ENET_MTS_NC                              0x00000004    /*  No Carrier.                                         */
#define  ENET_MTS_FA                              0x00000001    /*  Frame Aborted.                                      */

#define  ENET_MRS_FA                              0x80000000    /*  Frame Aborted.                                      */
#define  ENET_MRS_PF                              0x40000000    /*  Packet Filter.                                      */
#define  ENET_MRS_FF                              0x20000000    /*  Filtering Fail.                                     */
#define  ENET_MRS_BF                              0x10000000    /*  Broadcast Frame.                                    */
#define  ENET_MRS_MCF                             0x08000000    /*  Multicast Frame.                                    */
#define  ENET_MRS_UCF                             0x04000000    /*  Unsupported control Frame.                          */
#define  ENET_MRS_CF                              0x02000000    /*  Control Frame.                                      */
#define  ENET_MRS_LE                              0x01000000    /*  Length Error.                                       */
#define  ENET_MRS_VL2                             0x00800000    /*  Vlan2 Tag.                                          */
#define  ENET_MRS_VL1                             0x00400000    /*  Vlan1 Tag.                                          */
#define  ENET_MRS_CE                              0x00200000    /*  CRC Error.                                          */
#define  ENET_MRS_EB                              0x00100000    /*  Extra Bit.                                          */
#define  ENET_MRS_ME                              0x00080000    /*  MII Error.                                          */
#define  ENET_MRS_FT                              0x00040000    /*  Frame Type.                                         */
#define  ENET_MRS_LC                              0x00020000    /*  Late Collision.                                     */
#define  ENET_MRS_OL                              0x00010000    /*  Over Length.                                        */
#define  ENET_MRS_RF                              0x00008000    /*  Runt Frame.                                         */
#define  ENET_MRS_WT                              0x00004000    /*  Watchdog Time-out.                                  */
#define  ENET_MRS_FCI                             0x00002000    /*  False Carrier Indication.                           */
#define  ENET_MRS_FL                              0x000007FF    /*  Frame Length.                                       */
#define  ENET_SCR_SRESET                          0x00000001    /* Soft Reset (ENET DMA_SCR_RESET).                     */
#define  ENET_SCR_LOOPB                           0x00000002    /* Loopback mode (ENET DMA_SCR_LOOPB).                  */
#define  ENET_SCR_RX_MBSIZE                       0x00000010    /* Max def'd burst len in RX mode.                      */
#define  ENET_SCR_TX_MBSIZE                       0x000000C0    /* Max def'd burst len in TX mode.                      */
#define  ENET_SCR_RX_MAX_BURST_SZ         ENET_SCR_RX_MBSIZE    /* Maximum value of defined burst len in RX mode.       */
#define  ENET_SCR_RX_MAX_BURST_SZ_VAL             0x00000000    /* Default value of burst len in RX mode.               */
#define  ENET_SCR_TX_MAX_BURST_SZ         ENET_SCR_TX_MBSIZE    /* Maximum value of defined burst len in TX mode.       */
#define  ENET_SCR_TX_MAX_BURST_SZ_VAL             0x000000C0    /* Default value of burst len in TX mode.               */

#define  ENET_RXSTR_DMA_EN                        0x00000001    /* set = 0 by sw force a ENET DMA abort.                */
#define  ENET_RXSTR_FETCH                         0x00000004    /* start fetching the 1st descriptor.                   */
#define  ENET_RXSTR_FILTER_FAIL                   0x00000020    /* if = 1 the address filtering failed cond.            */
#define  ENET_RXSTR_RUNT                          0x00000040    /* discard damaged RX frames from cpu charge.           */
#define  ENET_RXSTR_COLLS_SEEN                    0x00000080    /* Late Collision Seen Cond discard frame automat.      */
#define  ENET_RXSTR_DFETCH_DLY                    0x00FFFF00    /* Descriptor Fetch Delay.                              */
#define  ENET_RXSTR_DFETCH_DEFAULT                0x00000100    /* Descriptor Fetch Delay default value.                */

#define  ENET_TXSTR_DMA_EN                        0x00000001    /* set = 0 by sw force a ENET DMA abort.                */
#define  ENET_TXSTR_FETCH                         0x00000004    /* start fetching the 1st descriptor.                   */
#define  ENET_TXSTR_URUN                          0x00000020    /* Underrun Enable.                                     */
#define  ENET_TXSTR_DFETCH_DLY                    0x00FFFF00    /* Descriptor Fetch Delay.                              */
#define  ENET_TXSTR_DFETCH_DEFAULT                0x00000100    /* Descriptor Fetch Delay default value.                */

#define  ENET_TXCR_DLYEN                          0x00008000    /* ENET DMA trigger delay enable.                       */
#define  ENET_TXCR_NXTEN                          0x00004000    /* Next Descriptor Fetch Mode Enable.                   */
#define  ENET_TXCR_CONTEN                         0x00001000    /* Continuous Mode Enable.                              */
#define  ENET_TXCR_XFERCOUNT                      0x00000FFF    /* ENET DMA transfer count.                             */

#define  ENET_RXCR_DLYEN                          0x00008000    /* ENET DMA trigger delay enable.                       */
#define  ENET_RXCR_NXTEN                          0x00004000    /* Next Descriptor Fetch Mode Enable.                   */
#define  ENET_RXCR_CONTEN                         0x00001000    /* Continuous Mode Enable.                              */
#define  ENET_RXCR_XFERCOUNT                      0x00000FFF    /* ENET DMA transfer count.                             */

#define  ENET_TXSAR_TXADDR                        0xFFFFFFFC    /* for ENET DMA Start Address (32 bit Word Align).      */
#define  ENET_TXSAR_ADDR_FIX_ADDR                 0x00000002    /* Disable incrementing of ENET DMA_ADDR.               */
#define  ENET_TXSAR_WRAP_EN                       0x00000001

#define  ENET_RXSAR_RXADDR                        0xFFFFFFFC    /* for ENET DMA Start Address (32 bit Word Align).      */
#define  ENET_RXSAR_ADDR_FIX_ADDR                 0x00000002    /* Disable incrementing of ENET DMA_ADDR.               */
#define  ENET_RXSAR_WRAP_EN                       0x00000001

#define  ENET_TXNDAR_DSCRADDR                     0xFFFFFFFC    /* Points to Next descriptor starting address.          */
#define  ENET_TXNDAR_NPOL_EN                      0x00000001    /* Next Descriptor Polling Enable.                      */

#define  ENET_RXNDAR_DSCRADDR                     0xFFFFFFFC    /* Points to Next descriptor starting address.          */
#define  ENET_RXNDAR_NPOL_EN                      0x00000001    /* Next Descriptor Polling Enable.                      */

#define  ENET_DSCR_TX_STATUS_FA_MSK               0x00000001    /* Frame Aborted.                                       */
#define  ENET_DSCR_TX_STATUS_JTO_MSK              0x00000002    /* Jabber Timeout.                                      */
#define  ENET_DSCR_TX_STATUS_NOC_MSK              0x00000004    /* No Carrier.                                          */
#define  ENET_DSCR_TX_STATUS_LOC_MSK              0x00000008    /* Loss of Carrier.                                     */
#define  ENET_DSCR_TX_STATUS_EXCD_MSK             0x00000010    /* Excessive Deferral.                                  */
#define  ENET_DSCR_TX_STATUS_LCOLL_MSK            0x00000020    /* Late Collision.                                      */
#define  ENET_DSCR_TX_STATUS_ECOLL_MSK            0x00000040    /* Excessive Collisions.                                */
#define  ENET_DSCR_TX_STATUS_URUN_MSK             0x00000080    /* Under Run.                                           */
#define  ENET_DSCR_TX_STATUS_DEFER_MSK            0x00000100    /* Deferred.                                            */
#define  ENET_DSCR_TX_STATUS_LCOLLO_MSK           0x00000200    /* Late Collision Observed.                             */
#define  ENET_DSCR_TX_STATUS_CCNT_MSK             0x00003C00    /* Collision Count.                                     */
#define  ENET_DSCR_TX_STATUS_HBFAIL_MSK           0x00004000    /* Heart Beat Fail.                                     */
#define  ENET_DSCR_TX_STATUS_VALID_MSK            0x00010000    /* Valid bit indicator.                                 */
#define  ENET_DSCR_TX_STATUS_PKT_RTRY_MSK         0x80000000    /* Packet Retry.                                        */
#define  ENET_DSCR_TX_STATUS_ORED_ERR_MSK         0x000003D7    /* for total number of errors.                          */

#define  ENET_DSCR_RX_STATUS_FLEN_MSK             0x000007ff    /* 0x00003FFF * Frame Length (max 2047).                */
#define  ENET_DSCR_RX_STATUS_FTLONG_MSK           0x00001000    /* Over Lenght.                                         */
#define  ENET_DSCR_RX_STATUS_FCI_MSK              0x00002000    /* Frame too Long.                                      */
#define  ENET_DSCR_RX_STATUS_WDTO_MSK             0x00004000    /* Watchdog Timeout.                                    */
#define  ENET_DSCR_RX_STATUS_RUNTFR_MSK           0x00008000    /* Runt Frame.                                          */
#define  ENET_DSCR_RX_STATUS_VALID_MSK            0x00010000    /* Valid bit indicator.                                 */
#define  ENET_DSCR_RX_STATUS_COLLSEEN_MSK         0x00020000    /* Collision Seen.                                      */
#define  ENET_DSCR_RX_STATUS_FTYPE_MSK            0x00040000    /* Frame Type.                                          */
#define  ENET_DSCR_RX_STATUS_MII_ERR_MSK          0x00080000    /* MII Error.                                           */
#define  ENET_DSCR_RX_STATUS_DRBBIT_MSK           0x00100000    /* Dribbling Bit.                                       */
#define  ENET_DSCR_RX_STATUS_CRC_ERR_MSK          0x00200000    /* CRC Error.                                           */
#define  ENET_DSCR_RX_STATUS_VLAN1_FR_MSK         0x00400000    /* One-Level VLAN Frame.                                */
#define  ENET_DSCR_RX_STATUS_VLAN2_FR_MSK         0x00800000    /* Two-Level VLAN Frame.                                */
#define  ENET_DSCR_RX_STATUS_LEN_ERR_MSK          0x01000000    /* Length Error.                                        */
#define  ENET_DSCR_RX_STATUS_CTL_FR_MSK           0x02000000    /* Control Frame.                                       */
#define  ENET_DSCR_RX_STATUS_UCTRL_FR_MSK         0x04000000    /* Unsupported Control Frame.                           */
#define  ENET_DSCR_RX_STATUS_MCAST_FR_MSK         0x08000000    /* Multicast Frame.                                     */
#define  ENET_DSCR_RX_STATUS_BCAST_FR_MSK         0x10000000    /* BroadCast Frame.                                     */
#define  ENET_DSCR_RX_STATUS_FLT_FAIL_MSK         0x20000000    /* Filtering Fail.                                      */
#define  ENET_DSCR_RX_STATUS_PKT_FILT_MSK         0x40000000    /* Packet Filter.                                       */
#define  ENET_DSCR_RX_STATUS_FA_MSK               0x80000000    /* Frame Aborted.                                       */
#define  ENET_IT_All                              0xFFFFFFFF
#define  ENET_IT_TX_CURR_DONE                     0x80000000    /*  Tx Current Done interrupt.                          */
#define  ENET_IT_TX_MERR_INT                      0x02000000    /*  Tx Master Error interrupt.                          */
#define  ENET_IT_TX_DONE                          0x00800000    /*  Tx Done interrupt.                                  */
#define  ENET_IT_TX_NEXT                          0x00400000    /*  Tx Invalid Next Descriptor interrupt.               */
#define  ENET_IT_TX_TO                            0x00080000    /*  Tx timeout interrupt.                               */
#define  ENET_IT_TX_ENTRY                         0x00040000    /*  Tx FIFO Entry Error interrupt.                      */
#define  ENET_IT_TX_FULL                          0x00020000    /*  Tx FIFO Full interrupt.                             */
#define  ENET_IT_TX_EMPTY                         0x00010000    /*  Tx FIFO Empty interrupt.                            */
#define  ENET_IT_RX_CURR_DONE                     0x00008000    /*  Rx Current Done interrupt.                          */
#define  ENET_IT_RX_MERR_INT                      0x00000200    /*  Rx Master Error interrupt.                          */
#define  ENET_IT_RX_DONE                          0x00000080    /*  Rx Done interrupt.                                  */
#define  ENET_IT_RX_NEXT                          0x00000040    /*  Rx Invalid Next Descriptor interrupt.               */
#define  ENET_IT_PACKET_LOST                      0x00000020    /*  Packet Lost interrupt.                              */
#define  ENET_IT_RX_TO                            0x00000008    /*  Rx Timeout.                                         */
#define  ENET_IT_RX_ENTRY                         0x00000004    /*  Rx FIFO Entry Error interrupt.                      */
#define  ENET_IT_RX_FULL                          0x00000002    /*  Rx FIFO Full interrupt.                             */
#define  ENET_IT_RX_EMPTY                         0x00000001    /*  Rx FIFO Empty interrupt.                            */

#define  ENET_IT_ERR                    (ENET_IT_TX_MERR_INT | \
                                         ENET_IT_TX_TO       | \
                                         ENET_IT_RX_MERR_INT | \
                                         ENET_IT_PACKET_LOST | \
                                         ENET_IT_RX_TO       )

#define  ENET_MIIA_PADDR                          0x0000F800    /* Physical address mask.                               */
#define  ENET_MIIA_RADDR                          0x000007C0    /* Register address mask.                               */
#define  ENET_MIIA_PR                             0x00000004    /* MII Preamble removal.                                */
#define  ENET_MIIA_WR                             0x00000002    /* MII Write/Read.                                      */
#define  ENET_MIIA_BUSY                           0x00000001    /* MII Busy.                                            */

#define  ENET_MIID_RDATA                          0x0000FFFF    /* MII Data.                                            */

#define  MII_PRESCALER_1                          0x00000000    /* Prescaler for MDC clock when HCLK < 50 MHz.          */
#define  MII_PRESCAPER_2                          0x01000000    /* Prescaler for MDC when HCLK > = 50 MHz.              */

#define  MAC_PERFECT_MULTICAST_PERFECT            0x00000000    /* Address Filtering Modes.                             */
#define  MAC_PERFECT_MULTICAST_HASH               0x00020000
#define  MAC_HASH_MULTICAST_HASH                  0x00040000
#define  MAC_INVERSE_FILTING                      0x00060000
#define  MAC_PROMISCUOUS_FILTERING                0x00080000
#define  MAC_PERFECT_MULTICAST_ALL                0x000A0000
#define  MAC_HASH_MULTICAST_ALL                   0x000C0000


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

                                                                /* --------------- DESCRIPTOR DATA TYPE --------------- */
typedef  struct  dev_desc {
    CPU_REG32    DMA_Ctrl;                                      /* DMA Control Register.                                */
    CPU_REG32    DMA_Addr;                                      /* DMA Start Address Register.                          */
    CPU_REG32    DMA_Next;                                      /* DMA Next Descriptor Register.                        */
    CPU_REG32    DMA_Status;                                    /* DMA Packet Status and Control Register.              */
} DEV_DESC;

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR      StatRxPktCtr;
    NET_CTR      StatTxPktCtr;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR      ErrRxFrameAbortCtr;
    NET_CTR      ErrRxDataAreaAllocCtr;
    NET_CTR      ErrRxInvalidLenCtr;
    NET_CTR      ErrRxDescNotValidCtr;
    NET_CTR      ErrTxDescNotValidCtr;
#endif
    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;
    DEV_DESC    *RxBufDescPtrStart;
    DEV_DESC    *RxBufDescPtrCur;
    DEV_DESC    *RxBufDescPtrEnd;
    DEV_DESC    *TxBufDescPtrStart;
    DEV_DESC    *TxBufDescPtrCur;
    DEV_DESC    *TxBufDescCompPtr;
    DEV_DESC    *TxBufDescPtrEnd;
    CPU_INT16U   RxNRdyCtr;
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
    CPU_REG32  ENET_SCR;
    CPU_REG32  ENET_IER;
    CPU_REG32  ENET_ISR;
    CPU_REG32  ENET_CCR;
    CPU_REG32  ENET_RXSTR;
    CPU_REG32  ENET_RXCR;
    CPU_REG32  ENET_RXSAR;
    CPU_REG32  ENET_RXNDAR;
    CPU_REG32  ENET_RXCAR;
    CPU_REG32  ENET_RXCTCR;
    CPU_REG32  ENET_RXTOR;
    CPU_REG32  ENET_RXSR;
    CPU_REG32  ENET_TXSTR;
    CPU_REG32  ENET_TXCR;
    CPU_REG32  ENET_TXSAR;
    CPU_REG32  ENET_TXNDAR;
    CPU_REG32  ENET_TXCAR;
    CPU_REG32  ENET_TXCTCR;
    CPU_REG32  ENET_TXTOR;
    CPU_REG32  ENET_TXISR;
    CPU_REG32  RESERVED1[236];
    CPU_REG32  ENET_MCR;
    CPU_REG32  ENET_MAH;
    CPU_REG32  ENET_MAL;
    CPU_REG32  ENET_MCHA;
    CPU_REG32  ENET_MCLA;
    CPU_REG32  ENET_MIIA;
    CPU_REG32  ENET_MIID;
    CPU_REG32  ENET_MCF;
    CPU_REG32  ENET_VL1;
    CPU_REG32  ENET_VL2;
    CPU_REG32  ENET_MTS;
    CPU_REG32  ENET_MRS;
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

                                                                /* ------------ FNCT'S COMMON TO ALL DRV'S ------------ */
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
                                                                    /* --------- FNCT'S COMMON TO DMA BASED DRV'S --------- */
static  void  NetDev_RxDescInit         (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_RxDescPtrCurInc    (NET_IF             *pif);

static  void  NetDev_TxDescInit         (NET_IF             *pif,
                                         NET_ERR            *perr);


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
                                                                                    /* STR912 dev API fnct ptrs :       */
const  NET_DEV_API_ETHER  NetDev_API_STR912 = { NetDev_Init,                        /*   Init/add                       */
                                                NetDev_Start,                       /*   Start                          */
                                                NetDev_Stop,                        /*   Stop                           */
                                                NetDev_Rx,                          /*   Rx                             */
                                                NetDev_Tx,                          /*   Tx                             */
                                                NetDev_AddrMulticastAdd,            /*   Multicast addr add             */
                                                NetDev_AddrMulticastRemove,         /*   Multicast addr remove          */
                                                NetDev_ISR_Handler,                 /*   ISR handler                    */
                                                NetDev_IO_Ctrl,                     /*   I/O ctrl                       */
                                                NetDev_MII_Rd,                      /*   Phy reg rd                     */
                                                NetDev_MII_Wr                       /*   Phy reg wr                     */
                                              };

/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Perform Driver Layer OS initialization
*                   (b) Initialize driver status
*
*
* Argument(s) : pif         Pointer to the interface to initialize.
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    MEM_POOL           *pmem_pool;
    CPU_SIZE_T          reqd_octets;
    CPU_INT32U          nbytes;
    CPU_INT32U          reg_val;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;



                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
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



                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4,
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;




                                                                /* Enable module clks (GPIO & EMAC).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev->ENET_SCR &= ~ENET_SCR_SRESET;                         /* Release EMAC peripheral reset.                       */

    pdev->ENET_ISR  =  ENET_IT_All;                             /* Disable all interrupt sources.                       */
    pdev->ENET_IER  = ~ENET_IT_All;                             /* Clear pending interrupts.                            */


                                                                /* Configure ext int ctrl'r.                            */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure Ethernet Controller GPIO.                  */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* ------------------ DMA INIT ------------------------ */

    reg_val        =   pdev->ENET_SCR;                          /* Read the ENET DMA Status and Control Register.       */

    reg_val       &= ~(CPU_INT32U)ENET_SCR_TX_MAX_BURST_SZ;     /* Setup Tx Max burst size.                             */
    reg_val       |=  (CPU_INT32U)ENET_SCR_TX_MAX_BURST_SZ_VAL;

    reg_val       &= ~(CPU_INT32U)ENET_SCR_RX_MAX_BURST_SZ;     /* Setup Rx Max Burst size.                             */
    reg_val       |=  (CPU_INT32U)ENET_SCR_RX_MAX_BURST_SZ_VAL;

    pdev->ENET_SCR =   reg_val;                                 /* Write Tx & Rx burst size to status & ctrl reg.       */

                                                                /* ----------- INITIALIZE DESCRIPTOR LISTS ------------ */
    nbytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);            /* Allocate space for Rx descriptors.                   */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,                            /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) 1,                            /* Allocate one block                               ... */
                   (CPU_SIZE_T  ) nbytes,                       /* ... large enough to hold all EMAC descriptors.       */
                   (CPU_SIZE_T  ) pdev_cfg->RxBufAlignOctets,   /* Align block to specified alignment (32-bit).         */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
         return;
    }

    pmem_pool = &pdev_data->RxDescPool;
    pdev_data->RxBufDescPtrStart = (DEV_DESC *)Mem_PoolBlkGet(pmem_pool, nbytes, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
    }


    nbytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);            /* Allocate space for Tx descriptors.                   */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,
                   (void       *) 0,
                   (CPU_SIZE_T  ) 0,
                   (CPU_SIZE_T  ) 1,
                   (CPU_SIZE_T  ) nbytes,
                   (CPU_SIZE_T  ) pdev_cfg->TxBufAlignOctets,
                   (CPU_SIZE_T *)&reqd_octets,
                   (LIB_ERR    *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    pmem_pool = &pdev_data->TxDescPool;
    pdev_data->TxBufDescPtrStart = (DEV_DESC *)Mem_PoolBlkGet(pmem_pool, nbytes, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
    }

    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------------ REGISTER INIT ------------------- */

    pdev->ENET_MCR &=  ENET_MCR_PS;                             /* FIRST! - Clear ALL MCR bits except MII prescaler.    */
    pdev->ENET_MCR |=  ENET_MCR_DRO;                            /* Disable reception while transmission in progress.    */
    pdev->ENET_MCR &= ~ENET_MCR_LM;                             /* Normal (non loopback) mode.                          */
    pdev->ENET_MCR |=  MAC_PERFECT_MULTICAST_HASH;              /* Address filtering mode.                              */
    pdev->ENET_MCR |=  VLAN_FILTER_VLTAG;                       /* VLAN filtering mode.                                 */
    pdev->ENET_MCR |=  ENET_MCR_ELC;                            /* Enable late collission retransmission.               */
    pdev->ENET_MCR &= ~ENET_MCR_DBF;                            /* Enable broadcast frame reception.                    */
    pdev->ENET_MCR &= ~ENET_MCR_DPR;                            /* Set EC & FA error bits after 16 failed Tx attempts.  */
    pdev->ENET_MCR |=  ENET_MCR_RVFF;                           /* Enable receive frame filtering.                      */
    pdev->ENET_MCR |=  ENET_MCR_APR;                            /* Automatically remove frame padding.                  */
    pdev->ENET_MCR &= ~ENET_MCR_DCE;                            /* Disable Tx deferral counter, defer indefintely.      */

    pdev->ENET_MCR &= ~ENET_MCR_PS;                             /* Configure MII divider.                               */
    pdev->ENET_MCR |=  MII_PRESCALER_1;

    pdev->ENET_VL1  = (VLANID1 << 16) | VLANTAG1;               /* VLAN initialization.                                 */
    pdev->ENET_VL2  = (VLANID2 << 16) | VLANTAG2;

    pdev->ENET_MCHA =  0;                                       /* Configure mult-cast address.                         */
    pdev->ENET_MCLA =  0;

                                                                /* Setup Descriptor Fetch for Rx block.                 */
    reg_val           =  pdev->ENET_RXSTR;
    reg_val          &= ~ENET_RXSTR_DFETCH_DLY;
    reg_val          |=  ENET_RXSTR_DFETCH_DEFAULT;
    pdev->ENET_RXSTR  =  reg_val;

                                                                /* Setup Descriptor Fetch for Tx block.                 */
    reg_val           =  pdev->ENET_TXSTR;
    reg_val          &= ~ENET_TXSTR_DFETCH_DLY;
    reg_val          |=  ENET_TXSTR_DFETCH_DEFAULT;
    reg_val          |=  ENET_TXSTR_URUN;                       /* Set Tx underrun bit.                                 */
    pdev->ENET_TXSTR  =  reg_val;


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_Start()
*
* Description : Start network interface hardware.
*
* Argument(s) : pif         Pointer to a network interface.
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
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (1) The physical HW address should not be configured from NetDev_Init(). Instead, it
*                   should be configured from within NetDev_Start() to allow for the proper use of
*                   NetIF_AddrHW_SetHandler(), hard coded HW addresses from the device configuration
*                   structure, or auto-loading EEPROM's. Changes to the physical address only take
*                   effect when the device transitions from the DOWN to UP state.
*
*               (2) The device HW address is set from one of the data sources below.  Each source is
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
*
*               (3) Setting the maximum number of frames queued for transmission is optional.  By default,
*                   all network interfaces are configured to block until the previous frame has completed
*                   transmission.  However, DMA based interfaces may have several frames configured for
*                   transmission before blocking is required. The number of queued transmit frames depends
*                   on the number of configured transmit descriptors.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ---- INITIALIZE ALL DEVICE DATA AREA VARIABLES ----- */
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    pdev_data->ErrRxFrameAbortCtr   = 0;
    pdev_data->ErrRxFrameAbortCtr   = 0;
    pdev_data->ErrRxInvalidLenCtr   = 0;
    pdev_data->ErrRxDescNotValidCtr = 0;
    pdev_data->ErrTxDescNotValidCtr = 0;
#endif
    pdev_data->RxNRdyCtr            = 0;                        /* No pending frames to process                         */


    pdev->ENET_SCR &= ~ENET_SCR_SRESET;                         /* Deassert the EMAC reset bit.                         */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                         */
                            pdev_cfg->TxDescNbr,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }


                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #1 & #2.                                   */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #2a).                                  */
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
                                                                /* ... if any (see Note #2b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any (see Note #2c).           */
            hw_addr[0] = (pdev->ENET_MAL >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->ENET_MAL >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->ENET_MAL >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->ENET_MAL >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->ENET_MAH >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->ENET_MAH >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->ENET_MAL = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));

        pdev->ENET_MAH = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                          ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* ------------------ CFG MULTICAST ------------------- */
#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

    pdev->ENET_MCLA   = 0;                                      /* Clear all multicast hash bits.                       */
    pdev->ENET_MCHA   = 0;                                      /* Clear all multicast hash bits.                       */


                                                                /* ------------------ REGISTER INIT ------------------- */

    pdev->ENET_RXSTR &= ~ENET_RXSTR_DMA_EN;                     /* Force ENET abort by software for the Rx block.       */
    pdev->ENET_TXSTR &= ~ENET_TXSTR_DMA_EN;                     /* Force ENET abort by software for the Tx block.       */

    pdev->ENET_ISR    = ~ENET_IT_All;                           /* Clear all interrupt flags.                           */
    pdev->ENET_IER    = (ENET_IT_RX_CURR_DONE |                 /* Enable Rx cur Int. src.                              */
                         ENET_IT_TX_CURR_DONE |                 /* Enable Tx cur Int. src.                              */
                         ENET_IT_RX_MERR_INT  |                 /* Enable Rx Master AHB arbiter MERR Int. src.          */
                         ENET_IT_TX_MERR_INT);                  /* Enable Tx Master AHB arbiter MERR Int. src.          */

    pdev->ENET_MCR   |= (ENET_MCR_TE |                          /* Enable the transmitter.                              */
                         ENET_MCR_RE);                          /* Enable the receiver.                                 */

    pdev->ENET_RXSTR |=  ENET_RXSTR_FETCH;                      /* Start DMA Rx fetch.                                  */

   (void)&pdev_data;                                            /* Avoid compiler warning for var set and not used.     */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Stop()
*
* Description : (1) Shutdown network interface hardware :
*
*                   (a) Disable receive and transmit interrupts
*                   (b) Disable the receiver and transmitter
*                   (c) Clear pending interrupt requests
*
*
* Argument(s) : pif         Pointer to the interface to stop.
*               ---         Argument validated in NetIF_Stop().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                No Error.
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
*               (3) Receive descriptors and pointers MUST be re-initialized without calling STR912_RxDescInit().
*                   This is due to the fact that STR912_RxDescInit() obtains a buffer for each descriptor and
*                   the fact that there is no way to free those existing buffers after the interface has been
*                   stopped.  Therefore, the buffers that are currently configured within the Rx descriptors
*                   will be re-used when the interface is re-started and only the descriptor flag bits
*                   and STR912 descriptor pointer registers require re-initialization.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT08U          i;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------------- DISABLE THE EMAC ----------------- */
    pdev->ENET_MCR &= ~(ENET_MCR_RE |                           /* Disable the receiver and transmitter.                */
                        ENET_MCR_TE);

    pdev->ENET_ISR  =   ENET_IT_All;                            /* Disable all interrupt sources.                       */
    pdev->ENET_IER  =  ~ENET_IT_All;                            /* Clear pending interrupts.                            */


                                                                /* --------- PARTIAL RX DESC RE-INITIALIZATION -------- */
    pdev_data->RxBufDescPtrCur = pdev_data->RxBufDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* See note #3.                                         */
        pdesc              = &pdev_data->RxBufDescPtrStart[i];
        pdesc->DMA_Status  =  ENET_DSCR_RX_STATUS_VALID_MSK;
    }

                                                                /* Configure the DMA with the Rx desc start address.    */
    pdev->ENET_RXNDAR = ((CPU_INT32U)pdev_data->RxBufDescPtrStart) | ENET_RXNDAR_NPOL_EN;

                                                                /* ------------ FREE USED TX DESCRIPTORS -------------- */
    pdev_data->TxBufDescPtrCur =  pdev_data->TxBufDescPtrStart;
    pdesc                      = &pdev_data->TxBufDescPtrStart[0];
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {
        if ((pdesc->DMA_Status &   ENET_DSCR_TX_STATUS_VALID_MSK) > 0) {    /* If NOT yet       tx'd,  ...              */
             pdesc->DMA_Status &= ~ENET_DSCR_TX_STATUS_VALID_MSK;           /* ... mark desc as tx'd & ...              */
                                                                            /* ... dealloc tx buf (see Note #2a1).      */
            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->DMA_Addr, &err);
           (void)&err;                                          /* Ignore possible dealloc err (see Note #2b2).         */
        }
        pdesc++;
    }

                                                                /* ----------- RE-INITIALIZE TX DESCRIPTORS ----------- */
    NetDev_TxDescInit(pif, perr);

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*
*                   (a) Decrement frame counter
*                   (b) Determine the descriptor that caused the interrupt
*                   (c) Obtain a pointer to a Network Buffer Data Area for storing new data
*                   (d) Reconfigure the descriptor with the pointer to the new data area
*                   (e) Pass a pointer to the received data area back to the caller via p_data
*                   (f) Clear interrupts
*
*
* Argument(s) : pif         Pointer to the interface to receive packet(s).
*               ---         Argument validated in NetIF_RxHandler().
*
*               p_data      Pointer to pointer to received DMA data area.  The recevied data area address
*                           should be returned to the stack by dereferencing p_data as *p_data = (address
*                           of receive data area).
*
*               size        Pointer to size. The number of bytes received should be returned to the stack
*                           by dereferencing size as *size = (number of bytes).
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
* Note(s)     : none.
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
    CPU_INT16S          len;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdesc     = (DEV_DESC          *)pdev_data->RxBufDescPtrCur;


                                                                /* --------------- DECREMENT FRAME CNT ---------------- */
    CPU_CRITICAL_ENTER();                                       /* Disable interrupts to alter shared data.             */
    if (pdev_data->RxNRdyCtr > 0) {                             /* One less frame to process.                           */
        pdev_data->RxNRdyCtr--;
    }
    CPU_CRITICAL_EXIT();
                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
                                                                /* If desc not owned by software, or the frame was  ... */
                                                                /* ... abored, then Rx error.                           */
    if ((pdesc->DMA_Status & ENET_DSCR_RX_STATUS_VALID_MSK) > 0) {
        NET_CTR_ERR_INC(pdev_data->ErrRxDescNotValidCtr);
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

    if ((pdesc->DMA_Status & ENET_DSCR_RX_STATUS_FA_MSK) > 0) {
        NET_CTR_ERR_INC(pdev_data->ErrRxFrameAbortCtr);
        NetDev_RxDescPtrCurInc(pif);
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

                                                                /* Determine len value to return, minus 4 byte CRC      */
    len = (pdesc->DMA_Status & 0x07FF) - NET_IF_ETHER_FRAME_CRC_SIZE;
    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {                    /* If frame is a runt.                                  */
        NET_CTR_ERR_INC(pdev_data->ErrRxInvalidLenCtr);
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame.                              */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }
                                                                /* Request an empty buffer.                             */
    pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_TYPE  *)0,
                                 (NET_ERR       *)perr);


    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer.                           */
        NET_CTR_ERR_INC(pdev_data->ErrRxDataAreaAllocCtr);
        NetDev_RxDescPtrCurInc(pif);                            /* Free the current descriptor.                         */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
        return;
    }

   *size   =  len;
   *p_data = (CPU_INT08U *)pdesc->DMA_Addr;                     /* Return a pointer to the newly received data area.    */

    pdesc->DMA_Addr = (CPU_INT32U)pbuf_new;                     /* Update the descriptor to point to a new data area    */

    NetDev_RxDescPtrCurInc(pif);                                /* Free the current descriptor.                         */

    NET_CTR_STAT_INC(pdev_data->StatRxPktCtr);
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetDev_Tx()
*
* Description : This function transmits the specified data :
*
*                   (1) Check if the transmitter is ready.
*                   (2) Configure the next transmit descriptor for pointer to data and data size.
*                   (3) Issue the transmit command.
*
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
* Note(s)     : none.
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
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
    pdesc     = (DEV_DESC          *)pdev_data->TxBufDescPtrCur;

    CPU_CRITICAL_ENTER();                                       /* This routine alters shared data. Disable interrupts! */

                                                                /* If descriptor owned by software.                     */
    if ((pdesc->DMA_Status & ENET_DSCR_TX_STATUS_VALID_MSK) == 0) {
        CPU_CRITICAL_EXIT();                                    /* Re-enable interrupts.                                */
        pdesc->DMA_Addr    = (CPU_INT32U)p_data;                /* Configure the DMA to transmit the provided buffer.   */
        pdesc->DMA_Ctrl    = (size & 0x0FFF) | ENET_TXCR_NXTEN; /* Set size, enable automatic fetch of next descriptor. */
        pdesc->DMA_Status  =  ENET_DSCR_TX_STATUS_VALID_MSK;    /* Set ownership bit to DMA owned.                      */

        pdev->ENET_TXSTR  |=  ENET_TXSTR_FETCH;                 /* Tell the DMA to perform a descriptor fetch.          */

        pdev_data->TxBufDescPtrCur++;
                                                                /* Check if we need to wrap around.                     */
        if (pdev_data->TxBufDescPtrCur == pdev_data->TxBufDescPtrEnd) {
            pdev_data->TxBufDescPtrCur  = pdev_data->TxBufDescPtrStart;
        }

       *perr = NET_DEV_ERR_NONE;

    } else {
        CPU_CRITICAL_EXIT();                                    /* Re-enable interrupts.                                */
        NET_CTR_ERR_INC(pdev_data->ErrTxDescNotValidCtr);
       *perr = NET_DEV_ERR_TX_BUSY;
    }
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
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_BOOLEAN         reg_sel;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without compliment.                       */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    crc = NetUtil_32BitReflect(crc);                            /* Reflect CRC to produce equivalent hardware result.   */

    hash    = (crc >> 26) & 0x3F;                               /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26) & 0x1F;                               /* Obtain least 5 significant bits for for reg bit sel. */
    reg_sel = (crc >> 31) & 0x01;                               /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    if (reg_sel == 0) {                                         /* Set multicast hash reg bit.                          */
        pdev->ENET_MCLA |= (1 << bit_sel);
    } else {
        pdev->ENET_MCHA |= (1 << bit_sel);
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
*                       (a) Perfect        filtering of ONE multicast address.
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
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_BOOLEAN         reg_sel;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without compliment.                       */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    crc = NetUtil_32BitReflect(crc);                            /* Reflect CRC to produce equivalent hardware result.   */

    hash    = (crc >> 26) & 0x3F;                               /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26) & 0x1F;                               /* Obtain least 5 significant bits for for reg bit sel. */
    reg_sel = (crc >> 31) & 0x01;                               /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1) {                                 /* If hash bit reference ctr not zero.                  */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0;                                        /* Zero hash bit references remaining.                  */

    if (reg_sel == 0) {                                         /* Clr multicast hash reg bit.                          */
        pdev->ENET_MCLA &= ~(1 << bit_sel);
    } else {
        pdev->ENET_MCHA &= ~(1 << bit_sel);
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
*                                          NetDev_IO_Ctrl()
*
* Description : This function implements various hardware functions.
*
* Argument(s) : pif         Pointer to the interface to handle I/O controls.
*               ---         Argument validated in NetIF_IO_Ctrl().
*
*               opt         Option code representing desired function to perform. The network protocol suite
*                           specifies the option codes below. Additional option codes may be defined by the
*                           driver developer in the driver's header file.
*                               NET_DEV_LINK_STATE_GET
*                               NET_DEF_LINK_STATE_UPDATE
*
*                           Driver defined operation codes MUST be defined starting from 20 or higher
*                           to prevent clashing with the pre-defined operation code types. See net_dev_nnn.h
*                           for more details.
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
* Note(s)     : none.
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
    CPU_SR_ALLOC();


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
             CPU_CRITICAL_ENTER();                              /* Disable int's so duplex / spd update is autonomous.  */
             switch (plink_state->Duplex) {                     /* Update duplex setting on device.                     */
                case NET_PHY_DUPLEX_FULL:
                     pdev->ENET_MCR |=  ENET_MCR_FDM;           /* Full Duplex Mode.                                    */
                     break;

                case NET_PHY_DUPLEX_HALF:
                     pdev->ENET_MCR &= ~ENET_MCR_FDM;           /* Half Duplex Mode.                                    */
                     break;

                default:
                     break;
             }

             switch (plink_state->Spd) {                        /* Update speed setting on device.                      */
                case NET_PHY_SPD_10:
                case NET_PHY_SPD_100:
                case NET_PHY_SPD_1000:
                default:
                     break;
             }
             CPU_CRITICAL_EXIT();                               /* Re-enable interrupts.                                */
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
* Description : Write data over the (R)MII bus to the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface to read the MII bus.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                               NET_PHY_ERR_NONE                MII write completed successfully.
*                               NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
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
    CPU_INT32U          addr;
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


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    addr  = 0;
    addr |= ((phy_addr << 11) & ENET_MIIA_PADDR);               /* Set the PHY addr.                                    */
    addr |= ((reg_addr <<  6) & ENET_MIIA_RADDR);               /* Select the corresponding reg.                        */
    addr &= ~ENET_MIIA_WR;                                      /* Read cmd.                                            */
    addr |=  ENET_MIIA_BUSY;

    timeout = MII_TIMEOUT;
    while (timeout > 0) {                                       /* Wait for the MII bus controller to become ready.     */
        timeout--;
        if ((pdev->ENET_MIIA & ENET_MIIA_BUSY) == 0) {
            break;
        }
    }

    if (timeout == 0) {                                         /* Return an error if a timeout occured.                */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
       *p_data = 0;
        return;
    }

    pdev->ENET_MIIA = addr;                                     /* Issue rd cmd.                                        */

    timeout = MII_TIMEOUT;
    while (timeout > 0) {                                       /* Wait for the data to become ready.                   */
        timeout--;
        if ((pdev->ENET_MIIA & ENET_MIIA_BUSY) == 0) {
            break;
        }
    }

    if (timeout == 0) {                                         /* Return an error if a timeout occured.                */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
       *p_data = 0;
        return;
    }

   *p_data = pdev->ENET_MIID & ENET_MIID_RDATA;

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface to write to the MII bus.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               data        Data to write to the specified PHY register.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE                MII write completed successfully.
*                               NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_MII_Wr (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   reg_data,
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          addr;
    CPU_INT32U          timeout;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    addr  =   0;
    addr |= ((phy_addr << 11) & ENET_MIIA_PADDR);               /* Set the PHY addr.                                    */
    addr |= ((reg_addr <<  6) & ENET_MIIA_RADDR);               /* Select the corresponding reg.                        */
    addr |=   ENET_MIIA_WR;                                     /* Write cmd.                                           */
    addr |=   ENET_MIIA_BUSY;

    timeout = MII_TIMEOUT;
    while (timeout > 0) {                                       /* Wait for the MII bus controller to become ready.     */
        timeout--;
        if ((pdev->ENET_MIIA & ENET_MIIA_BUSY) == 0) {
            break;
        }
    }

    if (timeout == 0) {                                         /* Return an error if a timeout occured.                */
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

    pdev->ENET_MIID = (reg_data & ENET_MIID_RDATA);             /* Set write data.                                      */
    pdev->ENET_MIIA =  addr;                                    /* Set destination PHY register addr.                   */

    timeout = MII_TIMEOUT;
    while (timeout > 0) {                                       /* Wait for the MII bus controller to become ready.     */
        timeout--;
        if ((pdev->ENET_MIIA & ENET_MIIA_BUSY) == 0) {
            break;
        }
    }

    if (timeout == 0) {                                         /* Return an error if a timeout occured.                */
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetDev_RxDescInit()
*
* Description : This function initializes the Rx descriptor list for the specified interface.
*
* Argument(s) : pif         Pointer to the interface to initialize receive descriptors.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE            No error
*                               NET_IF_MGR_ERR_nnnn         Various Network Interface management error codes
*                               NET_BUF_ERR_nnn             Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) Memory allocation for the descriptors and receive buffers MUST be performed BEFORE
*                   calling this function. This ensures that multiple calls to this function do NOT
*                   allocate additional memory to the interface and that the Rx descriptors may be
*                   safely re-initialized by calling this function.
*********************************************************************************************************
*/

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    pdesc                      = pdev_data->RxBufDescPtrStart;
    pdev_data->RxBufDescPtrCur = pdesc;

    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Initialize Rx descriptor ring                        */
        pdesc->DMA_Ctrl   =   pdev_cfg->RxBufLargeSize | ENET_RXCR_NXTEN;
        pdesc->DMA_Status =   ENET_DSCR_RX_STATUS_VALID_MSK;
        pdesc->DMA_Next   = ((CPU_REG32)(pdesc + 1)) | ENET_RXNDAR_NPOL_EN;
        pdesc->DMA_Addr   =  (CPU_REG32)NetBuf_GetDataPtr((NET_IF        *)pif,
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

        pdesc++;
    }

    pdev_data->RxBufDescPtrEnd = pdesc;                         /* Remember the next to last buffer in the ring         */

    pdesc--;                                                    /* Point to the last descriptor in the ring.            */
                                                                /* Set the last desc's next pointer to the 1st desc.    */
    pdesc->DMA_Next   = ((CPU_INT32U)pdev_data->RxBufDescPtrStart) | ENET_RXNDAR_NPOL_EN;

                                                                /* Configure the DMA with the Rx desc start address.    */
    pdev->ENET_RXNDAR = ((CPU_INT32U)pdev_data->RxBufDescPtrStart) | ENET_RXNDAR_NPOL_EN;

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
*                   (c) Inform device of receive descriptor ownership status change.
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
    DEV_DESC      *pdesc;

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */

    pdesc             = pdev_data->RxBufDescPtrCur;             /* Obtain pointer to current Rx descriptor.             */

    pdesc->DMA_Status = ENET_DSCR_RX_STATUS_VALID_MSK;          /* Free the descriptor.                                 */
    pdev_data->RxBufDescPtrCur++;                               /* Point to next Buffer Descriptor                      */
    if (pdev_data->RxBufDescPtrCur == pdev_data->RxBufDescPtrEnd) {
                                                                /* Wrap around end of descriptor list if necessary      */
        pdev_data->RxBufDescPtrCur  = pdev_data->RxBufDescPtrStart;
    }
}


/*
*********************************************************************************************************
*                                         NetDev_TxDescInit()
*
* Description : This function initializes the Tx descriptor list for the specified interface.
*
* Argument(s) : pif         Pointer to the interface to initialize transmit descriptors.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE            No error
*                               NET_IF_MGR_ERR_nnnn         Various Network Interface management error codes
*                               NET_BUF_ERR_nnn             Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init(),
*               NetDev_Stop().
*
* Note(s)     : (1) This function ONLY initializes the descriptor control, status and next. Data area
*                   pointers are not assigned to the descriptors until transmission occurs.
*********************************************************************************************************
*/

static  void  NetDev_TxDescInit (NET_IF  *pif,
                                 NET_ERR *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    pdesc                       = pdev_data->TxBufDescPtrStart;
    pdev_data->TxBufDescPtrCur  = pdesc;
    pdev_data->TxBufDescCompPtr = pdesc;                        /* Initialize descriptor tx complete tracking pntr.     */

    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Initialize Tx descriptor ring                        */
        pdesc->DMA_Ctrl   =   0;
        pdesc->DMA_Status =   0;
        pdesc->DMA_Next   = ((CPU_INT32U)(pdesc + 1)) | ENET_TXNDAR_NPOL_EN;
        pdesc++;
    }

    pdev_data->TxBufDescPtrEnd = pdesc;                         /* Remember the next to last buffer in the ring         */

    pdesc--;                                                    /* Point to the last descriptor in the ring.            */
                                                                /* Set the last desc's next pointer to the 1st desc.    */
    pdesc->DMA_Next   = ((CPU_INT32U)pdev_data->TxBufDescPtrStart) | ENET_TXNDAR_NPOL_EN;

                                                                /* Configure the DMA with the Tx desc start address.    */
    pdev->ENET_TXNDAR = ((CPU_INT32U)pdev_data->TxBufDescPtrStart) | ENET_TXNDAR_NPOL_EN;

    *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_ISR_Handler()
*
* Description : This function serves as a generic Interrupt Service Routine handler. This ISR handler
*               will service all necessary ISR events for the network interface controller.
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
* Caller(s)   : NetBSP_ISR_Handler().     NET BSP code for handling a group of ISR vectors.
*
* Note(s)     : (1) This function is called from the context of an ISR.
*
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
    CPU_INT32U          isr_status;
    CPU_INT16U          n_rdy;
    CPU_INT16U          n_new;
    CPU_INT16U          i;
    CPU_BOOLEAN         done;
    NET_ERR             err;


   (void)&type;                                                 /* Prevent compiler warning if arg 'type' not req'd.    */


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */



    isr_status = pdev->ENET_ISR;

                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if ((isr_status & ENET_IT_RX_CURR_DONE) > 0) {              /* Receive interrupt.                                   */
        n_rdy = 0;
        pdesc = pdev_data->RxBufDescPtrStart;

        for (i = 0; i < pdev_cfg->RxDescNbr; i++) {             /* Loop throughout Rx descriptor ring                   */
            if ((pdesc->DMA_Status & ENET_DSCR_RX_STATUS_VALID_MSK) == 0) {
                n_rdy++;
            }
            pdesc++;
        }

        n_new = n_rdy - pdev_data->RxNRdyCtr;                   /* Determine how many NEW packets have been received    */
        while (n_new > 0) {
           NetIF_RxTaskSignal(pif->Nbr, &err);                  /* Signal Net IF RxQ Task of new frame                  */
            switch (err) {
                case NET_IF_ERR_NONE:
                     if (pdev_data->RxNRdyCtr < pdev_cfg->RxBufLargeNbr) {
                         pdev_data->RxNRdyCtr++;
                     }
                     break;

                case NET_IF_ERR_RX_Q_FULL:
                case NET_IF_ERR_RX_Q_SIGNAL_FAULT:
                default:
                     n_new = 1;                                 /* Break loop to prevent further posting when Q is full */
                     break;
            }
            n_new--;
        }

        pdev->ENET_ISR = ENET_IT_RX_CURR_DONE;                  /* Clear interrupt source.                              */
    }

                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if ((isr_status & ENET_IT_TX_CURR_DONE) > 0) {              /* Transmit complete interrupt.                         */
        done = DEF_NO;
        pdev->ENET_ISR = ENET_IT_TX_CURR_DONE;                  /* Clear interrupt source.                              */
        while (done != DEF_YES) {
            NET_CTR_STAT_INC(pdev_data->StatTxPktCtr);          /* Increment Tx Pkt ctr.                                */
            pdesc = pdev_data->TxBufDescCompPtr;
            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->DMA_Addr, &err);
            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available        */
            pdev_data->TxBufDescCompPtr++;
            if (pdev_data->TxBufDescCompPtr == pdev_data->TxBufDescPtrEnd) {
                pdev_data->TxBufDescCompPtr  = pdev_data->TxBufDescPtrStart;
            }

            pdesc = pdev_data->TxBufDescCompPtr;
            if ( (pdesc == pdev_data->TxBufDescPtrCur                    ) ||
                ((pdesc->DMA_Status & ENET_DSCR_TX_STATUS_VALID_MSK) != 0)) {
                done = DEF_YES;
            }
        }
    }

    if ((isr_status & ENET_IT_RX_MERR_INT) > 0) {
        pdev->ENET_ISR = ENET_IT_RX_MERR_INT;                   /* Clr Int. src.                                        */
    }

    if ((isr_status & ENET_IT_TX_MERR_INT) > 0) {
        pdev->ENET_ISR = ENET_IT_TX_MERR_INT;                   /* Clr Int. src.                                        */
    }
}


#endif /* NET_IF_ETHER_MODULE_EN */


