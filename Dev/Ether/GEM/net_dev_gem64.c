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
*                                 CADENCE GIGABIT ETHERNET MAC (GEM)
*                                               64-bit
*
* Filename : net_dev_gem64.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_GEM_MODULE

#include  <cpu.h>
#include  <lib_def.h>
#include  <cpu_cache.h>
#include  <Source/net.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_gem.h"

#ifdef NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  BUF_ALIGN_OCTETS                           4u                                  /* Minimum buffer alignment.                            */
#define  DEV_NUM_PRI_Q_PTRS                         2u                                  /* Two priority queues need termination.                */

                                                                                        /* ------------ REGISTER BIT FIELD DEFINES ------------ */
                                                                                        /* ------------------ RX DESCRIPTORS ------------------ */
#define  GEM_RXBUF_ADDR_MASK                       (0xFFFFFFFFFFFFFFFCu)                /* RX buffer address.                                   */
#define  GEM_RXBUF_ADDR_WRAP                        DEF_BIT_01                          /* Wrap flag.                                           */
#define  GEM_RXBUF_ADDR_OWN                         DEF_BIT_00                          /* Ownership flag.                                      */

#define  GEM_RXBUF_STATUS_GBC                       DEF_BIT_31                          /* Global broadcast detected.                           */
#define  GEM_RXBUF_STATUS_MULTI_MATCH               DEF_BIT_30                          /* Multicast hash match.                                */
#define  GEM_RXBUF_STATUS_UNI_MATCH                 DEF_BIT_29                          /* Unicast hash match.                                  */
#define  GEM_RXBUF_STATUS_EXT_MATCH                 DEF_BIT_28                          /* External address match.                              */

#define  GEM_RXBUF_STATUS_EOF                       DEF_BIT_15                          /* End of frame.                                        */
#define  GEM_RXBUF_STATUS_SOF                       DEF_BIT_14                          /* Start of frame.                                      */
#define  GEM_RXBUF_SIZE_MASK                       (0x1FFFu)                            /* Size of frame.                                       */


                                                                                        /* ------------------ TX DESCRIPTORS ------------------ */
#define  GEM_TXBUF_ADDR_MASK                       (0xFFFFFFFFFFFFFFFCu)                /* TX Buffer address.                                   */

#define  GEM_TXBUF_USED                             DEF_BIT_31                          /* Used flag.                                           */
#define  GEM_TXBUF_WRAP                             DEF_BIT_30                          /* Wrap flag.                                           */
#define  GEM_TXBUF_RETRY_EXCED                      DEF_BIT_29                          /* Retry limit exceeded.                                */
#define  GEM_TXBUF_AHB_ERR                          DEF_BIT_27                          /* Frame corruption due to AHB error.                   */
#define  GEM_TXBUF_LATE_COLL                        DEF_BIT_26                          /* Late collision.                                      */

#define  GEM_TXBUF_NO_CRC                           DEF_BIT_16                          /* No CRC to be appended.                               */
#define  GEM_TXBUF_LAST                             DEF_BIT_15                          /* Last buffer.                                         */
#define  GEM_TXBUF_LENGTH_MASK                     (0x3FFF)                             /* Buffer length.                                       */


                                                                                        /* ----------------- NETWORK CONTROL ------------------ */
#define  GEM_BIT_CTRL_FLUSH_NEXT_RX_DPRAM_PKT       DEF_BIT_18                          /* Flush next packet from the external DPRAM.           */
#define  GEM_BIT_CTRL_TX_PFC_PRI_PAUSE_FRAME        DEF_BIT_17                          /* Transmit PFC priority baste pause frame.             */
#define  GEM_BIT_CTRL_EN_PFC_PRI_PAUSE_RX           DEF_BIT_16                          /* Enable PFS priority based pause reception.           */
#define  GEM_BIT_CTRL_STR_RX_TIMESTAMP              DEF_BIT_15                          /* Store timestamps to memory.                          */
#define  GEM_BIT_CTRL_TX_ZEROQ_PAUSE_FRAME          DEF_BIT_12                          /* Transmit zero quantum pause frame.                   */
#define  GEM_BIT_CTRL_TX_PAUSE_FRAME                DEF_BIT_11                          /* Transmit pause frame.                                */
#define  GEM_BIT_CTRL_TX_HALT                       DEF_BIT_10                          /* Transmit halt.                                       */
#define  GEM_BIT_CTRL_START_TX                      DEF_BIT_09                          /* Start transmission.                                  */
#define  GEM_BIT_CTRL_BACK_PRESSURE                 DEF_BIT_08                          /* Back pressure.                                       */
#define  GEM_BIT_CTRL_WREN_STAT_REGS                DEF_BIT_07                          /* Write enable for stats registers.                    */
#define  GEM_BIT_CTRL_INCR_STATS_REGS               DEF_BIT_06                          /* Increment statistics registers.                      */
#define  GEM_BIT_CTRL_CLEAR_STATS_REGS              DEF_BIT_05                          /* Clear statistics registers.                          */
#define  GEM_BIT_CTRL_MGMT_PORT_EN                  DEF_BIT_04                          /* Management port enable.                              */
#define  GEM_BIT_CTRL_TX_EN                         DEF_BIT_03                          /* Tramsit enable.                                      */
#define  GEM_BIT_CTRL_RX_EN                         DEF_BIT_02                          /* Receive enable.                                      */
#define  GEM_BIT_CTRL_LOOPBACK_LOCAL                DEF_BIT_01                          /* Loop back local.                                     */


                                                                                        /* -------------- NETWORK CONFIGURATION --------------- */
#define  GEM_BIT_CFG_UNIDIR_EN                      DEF_BIT_31                          /* Uni-drection enable.                                 */
#define  GEM_BIT_CFG_IGNORE_IPG_RX_ER               DEF_BIT_30                          /* Ignore IPG rx_er.                                    */
#define  GEM_BIT_CFG_RX_BAD_PREAMBLE                DEF_BIT_29                          /* Receive bad preamble.                                */
#define  GEM_BIT_CFG_IPG_STRETCH_EN                 DEF_BIT_28                          /* IPG stretch enable.                                  */
#define  GEM_BIT_CFG_SGMII_EN                       DEF_BIT_27                          /* SGMII mode enable.                                   */
#define  GEM_BIT_CFG_IGNORE_RX_FCS                  DEF_BIT_26                          /* Ignore RX FCS.                                       */
#define  GEM_BIT_CFG_RX_HD_WHILE_TX                 DEF_BIT_25                          /* RX half duplex while TX.                             */
#define  GEM_BIT_CFG_RX_CHKSUM_OFFLD_EN             DEF_BIT_24                          /* Receive checksum offloading enable.                  */
#define  GEM_BIT_CFG_DIS_CP_PAUSE_FRAME             DEF_BIT_23                          /* Disable copy of pause frame.                         */

#define  GEM_BIT_CFG_DBUS_WIDTH_MSK                (DEF_BIT_FIELD(2, 21))               /* Data bus width.                                      */
#define  GEM_BIT_CFG_DBUS_WIDTH(cfg)               (DEF_BIT_MASK(cfg, 21) &\
                                                    GEM_BIT_CFG_DBUS_WIDTH_MSK)

#define  GEM_BIT_CFG_MDC_CLK_DIV_MSK               (DEF_BIT_FIELD(3, 18))               /* MDC clock division.                                  */
#define  GEM_BIT_CFG_MDC_CLK_DIV(cfg)              (DEF_BIT_MASK(cfg, 18) &\
                                                    GEM_BIT_CFG_MDC_CLK_DIV_MSK)

#define  GEM_BIT_CFG_FCS_REMOVE                     DEF_BIT_17                          /* FCS remove.                                          */
#define  GEM_BIT_CFG_LEN_ERR_FRAME_DISC             DEF_BIT_16                          /* Length field error frame discard.                    */

#define  GEM_BIT_CFG_RX_BUF_OFF_MSK                (DEF_BIT_FIELD(2, 14))               /* Receive buffer offset.                               */
#define  GEM_BIT_CFG_RX_BUF_OFF(cfg)               (DEF_BIT_MASK(cfg, 14) &\
                                                    GEM_BIT_CFG_RX_BUF_OFF_MSK)

#define  GEM_BIT_CFG_PAUSE_EN                       DEF_BIT_13                          /* Pause enable.                                        */
#define  GEM_BIT_CFG_RETRY_TEST                     DEF_BIT_12                          /* Retry test.                                          */
#define  GEM_BIT_CFG_PCS_SEL                        DEF_BIT_11                          /* PCS select.                                          */
#define  GEM_BIT_CFG_GIGE_EN                        DEF_BIT_10                          /* Gigabit mode enable.                                 */
#define  GEM_BIT_CFG_EXT_ADDR_MATCH_EN              DEF_BIT_09                          /* External address match enable.                       */
#define  GEM_BIT_CFG_UNI_HASH_EN                    DEF_BIT_07                          /* Unicast hash enable.                                 */
#define  GEM_BIT_CFG_MULTI_HASH_EN                  DEF_BIT_06                          /* Multicast hash enable.                               */
#define  GEM_BIT_CFG_NO_BROADCAST                   DEF_BIT_05                          /* No broadcast.                                        */
#define  GEM_BIT_CFG_COPY_ALL                       DEF_BIT_04                          /* Copy all frames.                                     */
#define  GEM_BIT_CFG_DISC_NON_VLAN                  DEF_BIT_02                          /* Discard non VLAN frames.                             */
#define  GEM_BIT_CFG_FULL_DUPLEX                    DEF_BIT_01                          /* Full duplex.                                         */
#define  GEM_BIT_CFG_SPEED                          DEF_BIT_00                          /* Speed.                                               */

                                                                                        /* ------------------ NETWORK STATUS ------------------ */
#define  GEM_BIT_STATUS_PFC_PRI_PAUSE_NEG           DEF_BIT_06                          /* PFC pause negociated.                                */
#define  GEM_BIT_STATUS_PCS_AUTONEG_TX_RES          DEF_BIT_05                          /* PCS pause tx resolution.                             */
#define  GEM_BIT_STATUS_PCS_AUTONEG_RX_RES          DEF_BIT_04                          /* PCS pause rx resultion.                              */
#define  GEM_BIT_STATUS_PCS_AUTONEG_DUP_RES         DEF_BIT_03                          /* PCS duplex resolution.                               */
#define  GEM_BIT_STATUS_PHY_MGMT_IDLE               DEF_BIT_02                          /* PHY MGMT logic idle.                                 */
#define  GEM_BIT_STATUS_MDIO_IN_PIN_STATUS          DEF_BIT_01                          /* MDIO in pin status.                                  */
#define  GEM_BIT_STATUS_PCS_LINK_STATE              DEF_BIT_00                          /* PCS link state.                                      */

                                                                                        /* ---------------- DMA CONFIGURATION ----------------- */
#define  GEM_BIT_DMACFG_ADDR_BUS_WIDTH              DEF_BIT_30                          /* DMA address bus width.                               */
#define  GEM_BIT_DMACFG_TX_EXT_BD_MODE              DEF_BIT_29                          /* Enable TX extended BD mode.                          */
#define  GEM_BIT_DMACFG_RX_EXT_BD_MODE              DEF_BIT_28                          /* Enable RX extended BD mode.                          */
#define  GEM_BIT_DMACFG_MAX_AMBA_BURST_TX           DEF_BIT_26                          /* Force maximum length bursts on TX.                   */
#define  GEM_BIT_DMACFG_MAX_AMBA_BURST_RX           DEF_BIT_25                          /* Force maximum length bursts on RX.                   */
#define  GEM_BIT_DMACFG_DISC_WHEN_NO_AHB            DEF_BIT_24                          /* Discard packet when no AHB resource                  */

#define  GEM_BIT_DMACFG_AHB_RX_SIZE_MSK            (DEF_BIT_FIELD(8, 16))               /* Receive buffer offset.                               */
#define  GEM_BIT_DMACFG_AHB_RX_SIZE(cfg)           (DEF_BIT_MASK(cfg, 16) &\
                                                    GEM_BIT_DMACFG_AHB_RX_SIZE_MSK)

#define  GEM_BIT_DMACFG_CSUM_GEN_OFFLOAD_EN         DEF_BIT_11                          /* Checksum generation offload enable.                  */
#define  GEM_BIT_DMACFG_TX_PKTBUF_MEMSZ_SEL         DEF_BIT_10                          /* Transmit packet buffer memory size.                  */

#define  GEM_BIT_DMACFG_RX_PKTBUF_SZ_MSK           (DEF_BIT_FIELD(2, 8))                /* Receive packet buffer memory size.                   */
#define  GEM_BIT_DMACFG_RX_PKTBUF_SZ(cfg)          (DEF_BIT_MASK(cfg, 8) &\
                                                    GEM_BIT_DMACFG_RX_PKTBUF_SZ_MSK)

#define  GEM_BIT_DMACFG_AHB_PKT_SWAP_EN             DEF_BIT_07                          /* AHB endian swap enable for packets.                  */
#define  GEM_BIT_DMACFG_AHC_MGMT_SWAP_EN            DEF_BIT_06                          /* AHB endian swap enable for management descriptors    */

#define  GEM_BIT_DMACFG_RX_AHB_BURST_MSK           (DEF_BIT_FIELD(5, 0))                /* AHB fixed burst length.                              */
#define  GEM_BIT_DMACFG_RX_AHB_BURST(cfg)          (DEF_BIT_MASK(cfg, 0) &\
                                                    GEM_BIT_DMACFG_RX_AHB_BURST_MSK)

                                                                                        /* ----------------- TRANSMIT STATUS ------------------ */
#define  GEM_BIT_TXSTATUS_HRESP_NOT_OK              DEF_BIT_08                          /* Hresp not OK.                                        */
#define  GEM_BIT_TXSTATUS_LATE_COLLISION            DEF_BIT_07                          /* Late collision occurred.                             */
#define  GEM_BIT_TXSTATUS_TX_UNDER_RUN              DEF_BIT_06                          /* Transmit under run.                                  */
#define  GEM_BIT_TXSTATUS_TX_COMPLETE               DEF_BIT_05                          /* Transmit complete.                                   */
#define  GEM_BIT_TXSTATUS_TX_CORR_AHB_ERR           DEF_BIT_04                          /* Transmit frame corruption due to AHB error.          */
#define  GEM_BIT_TXSTATUS_TX_GO                     DEF_BIT_03                          /* Transmit go.                                         */
#define  GEM_BIT_TXSTATUS_RETRY_EXCEEDED            DEF_BIT_02                          /* Retry limit exceeded.                                */
#define  GEM_BIT_TXSTATUS_COLLISION                 DEF_BIT_01                          /* Collision occurred.                                  */
#define  GEM_BIT_TXSTATUS_USED_BIT_READ             DEF_BIT_00                          /* Used bit read.                                       */

                                                                                        /* ------------------ RECEIVE STATUS ------------------ */
#define  GEM_BIT_RXSTATUS_HRESP_NOT_OK              DEF_BIT_03                          /* Hresp not OK.                                        */
#define  GEM_BIT_RXSTATUS_RX_OVERRUN                DEF_BIT_02                          /* Receive overrun.                                     */
#define  GEM_BIT_RXSTATUS_FRAME_RECD                DEF_BIT_01                          /* Frame received.                                      */
#define  GEM_BIT_RXSTATUS_BUFFER_NOT_AVAIL          DEF_BIT_00                          /* Buffer not available.                                */

                                                                                        /* ----------------- INTERRUPT STATUS ----------------- */
#define  GEM_BIT_INT_TSU_SEC_INCR                   DEF_BIT_26                          /* TSE seconds register increment.                      */
#define  GEM_BIT_INT_PDELAY_RESP_TX                 DEF_BIT_25                          /* PTP pdelay_resp frame transmitted.                   */
#define  GEM_BIT_INT_PDELAY_REQ_TX                  DEF_BIT_24                          /* PTP pdelay_req frame transmitted.                    */
#define  GEM_BIT_INT_PDELAY_RESP_RX                 DEF_BIT_23                          /* PTP pdelay_resp frame received.                      */
#define  GEM_BIT_INT_PDELAY_REQ_RX                  DEF_BIT_22                          /* PTP pdelay_req frame received.                       */
#define  GEM_BIT_INT_SYNC_TX                        DEF_BIT_21                          /* PTP sync frame transmitted.                          */
#define  GEM_BIT_INT_DELAY_REQ_TX                   DEF_BIT_20                          /* PTP delay_req frame transmitted.                     */
#define  GEM_BIT_INT_SYNC_RX                        DEF_BIT_19                          /* PTP sync frame received.                             */
#define  GEM_BIT_INT_DELAY_REQ_RX                   DEF_BIT_18                          /* PTP delay_req frame received.                        */
#define  GEM_BIT_INT_PARTNER_PG_RX                  DEF_BIT_17                          /* PCS link partner page received.                      */
#define  GEM_BIT_INT_AUTONEG_COMPLETE               DEF_BIT_16                          /* PCS auto-negotiation complete.                       */
#define  GEM_BIT_INT_EXT_INTR                       DEF_BIT_15                          /* External interrupt.                                  */
#define  GEM_BIT_INT_PAUSE_TX                       DEF_BIT_14                          /* Pause frame transmitted.                             */
#define  GEM_BIT_INT_PAUSE_ZERO                     DEF_BIT_13                          /* Pause time zero.                                     */
#define  GEM_BIT_INT_PAUSE_NONZERO_RX               DEF_BIT_12                          /* Pause frame with non-zero pause quantum received.    */
#define  GEM_BIT_INT_HRESP_NOT_OK                   DEF_BIT_11                          /* Hresp not OK.                                        */
#define  GEM_BIT_INT_RX_OVERRUN                     DEF_BIT_10                          /* Receive overrun.                                     */
#define  GEM_BIT_INT_LINK_CHNG                      DEF_BIT_09                          /* Link state change.                                   */
#define  GEM_BIT_INT_TX_COMPLETE                    DEF_BIT_07                          /* Transmit complete.                                   */
#define  GEM_BIT_INT_TX_CORRUPT_AHB                 DEF_BIT_06                          /* Transmit frame corruption due to AHB error.          */
#define  GEM_BIT_INT_RETRY_EX_LATE                  DEF_BIT_05                          /* Retry limit exceeded or late collision.              */
#define  GEM_BIT_INT_TX_USED_READ                   DEF_BIT_03                          /* TX used bit read.                                    */
#define  GEM_BIT_INT_RX_USED_READ                   DEF_BIT_02                          /* RX used bit read.                                    */
#define  GEM_BIT_INT_RX_COMPLETE                    DEF_BIT_01                          /* Receive complete.                                    */
#define  GEM_BIT_INT_MGMT_SENT                      DEF_BIT_00                          /* Management frame sent.                               */

                                                                                        /* ----------------- PHY MAINTENANCE ------------------ */
#define  GEM_BIT_PHYMGMT_CLAUSE_22                  DEF_BIT_30                          /* Clause 22 operation.                                 */

#define  GEM_BIT_PHYMGMT_OPERATION_MSK             (DEF_BIT_FIELD(2, 28))               /* Operation.                                           */
#define  GEM_BIT_PHYMGMT_OPERATION(cfg)            (DEF_BIT_MASK(cfg, 28) &\
                                                    GEM_BIT_PHYMGMT_OPERATION_MSK)

#define  GEM_BIT_PHYMGMT_PHYADDR_MSK               (DEF_BIT_FIELD(5, 23))               /* PHY address.                                         */
#define  GEM_BIT_PHYMGMT_PHYADDR(cfg)              (DEF_BIT_MASK(cfg, 23) &\
                                                    GEM_BIT_PHYMGMT_PHYADDR_MSK)

#define  GEM_BIT_PHYMGMT_REGADDR_MSK               (DEF_BIT_FIELD(5, 18))               /* Register address.                                    */
#define  GEM_BIT_PHYMGMT_REGADDR(cfg)              (DEF_BIT_MASK(cfg, 18) &\
                                                    GEM_BIT_PHYMGMT_REGADDR_MSK)

#define  GEM_BIT_PHYMGMT_MUST10_MSK                (DEF_BIT_FIELD(2, 16))               /* Must be 10.                                          */
#define  GEM_BIT_PHYMGMT_MUST10(cfg)               (DEF_BIT_MASK(cfg, 16) &\
                                                    GEM_BIT_PHYMGMT_MUST10_MSK)

#define  GEM_BIT_PHYMGMT_DATA_MSK                  (DEF_BIT_FIELD(16, 0))               /* Data.                                                */
#define  GEM_BIT_PHYMGMT_DATA(cfg)                 (DEF_BIT_MASK(cfg, 0) &\
                                                    GEM_BIT_PHYMGMT_DATA_MSK)

#define  INT_STATUS_MASK_ALL                       (0xFFFFFFFFu)
#define  INT_STATUS_MASK_SUPPORTED                 (GEM_BIT_INT_RX_COMPLETE |\
                                                    GEM_BIT_INT_TX_USED_READ)


/*
*********************************************************************************************************
*                                           GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                                MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL DATA TYPES
*********************************************************************************************************
*/

                                                                /* ------------- DMA DESCRIPTOR DATA TYPE ------------- */
typedef  struct  dev_desc {
    CPU_REG32    Addr;                                          /* Start Address Register.                              */
    CPU_REG32    Status;                                        /* Packet Status and Control Register.                  */
    CPU_REG32    AddrHigh;                                      /* Upper Address Register.                              */
    CPU_REG32    Reserved;
} DEV_DESC;

                                                                /* ------------------ SOFTWARE QUEUES ----------------- */
typedef  struct  dev_buf_q {                                    /* Struct used to track frames that are queued up.      */
    CPU_INT08U  *DataPtr;
    CPU_INT16U   Size;
} DEV_BUF_Q;

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
                                                                /* Memory used for DMA descriptors.                     */
    MEM_POOL      RxDescPool;
    MEM_POOL      TxDescPool;
    MEM_POOL      PriQDescPool;

                                                                /* Number of descriptors allocated in device cfg.       */
    NET_BUF_QTY   RxDescNbr;
    NET_BUF_QTY   TxDescNbr;
                                                                /* Rx/Tx descriptor ring management.                    */
    DEV_DESC     *RxBufDescPtrStart;
    DEV_DESC     *RxBufDescPtrCur;
    DEV_DESC     *RxBufDescPtrEnd;

    DEV_DESC     *TxRingHead;
    NET_BUF_QTY   TxRingSize;

    DEV_DESC     *PriQDescPtr;

    CPU_BOOLEAN   TxBusy;
                                                                /* Memory used for buffer queue.                        */
    MEM_POOL      TxQPool;
                                                                /* Tx buffer queue management.                          */
    DEV_BUF_Q    *TxQ;
    NET_BUF_QTY   TxQFront;
    NET_BUF_QTY   TxQBack;
    NET_BUF_QTY   TxQSize;

#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U    MulticastAddrHashBitCtr[64u];
#endif
} NET_DEV_DATA;

                                                                /* Error codes for queueing mechanism.                  */
typedef  enum  net_drv_err {
    NET_DRV_ERR_NONE,
    NET_DRV_ERR_Q_FULL,
    NET_DRV_ERR_Q_EMPTY
} NET_DRV_ERR;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                        /* -------- FNCT'S COMMON TO ALL DEV'S -------- */
static  void  NetDev_Init               (NET_IF             *pif,
                                         NET_ERR            *p_err);


static  void  NetDev_Start              (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_Stop               (NET_IF             *pif,
                                         NET_ERR            *p_err);


static  void  NetDev_Rx                 (NET_IF             *pif,
                                         CPU_INT08U        **p_data,
                                         CPU_INT16U         *size,
                                         NET_ERR            *p_err);

static  void  NetDev_Tx                 (NET_IF             *pif,
                                         CPU_INT08U         *p_data,
                                         CPU_INT16U          size,
                                         NET_ERR            *p_err);


static  void  NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *p_err);

static  void  NetDev_AddrMulticastRemove(NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *p_err);


static  void  NetDev_ISR_Handler        (NET_IF             *pif,
                                         NET_DEV_ISR_TYPE    type);


static  void  NetDev_IO_Ctrl            (NET_IF             *pif,
                                         CPU_INT08U          opt,
                                         void               *p_data,
                                         NET_ERR            *p_err);


static  void  NetDev_MII_Rd             (NET_IF             *pif,
                                         CPU_INT08U          phy_addr,
                                         CPU_INT08U          reg_addr,
                                         CPU_INT16U         *p_data,
                                         NET_ERR            *p_err);

static  void  NetDev_MII_Wr             (NET_IF             *pif,
                                         CPU_INT08U          phy_addr,
                                         CPU_INT08U          reg_addr,
                                         CPU_INT16U          data,
                                         NET_ERR            *p_err);


                                                                /* --------- FNCT'S COMMON TO DMA BASED DRV'S --------- */
static  void  NetDev_RxDescInit         (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_TxDescInit         (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_PriQDescInit       (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_RxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_TxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_PriQDescFreeAll    (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_RxDescPtrCurInc    (NET_IF             *pif);

                                                                /* ------------------ QUEUE FUNCTIONS ----------------- */
static  void  NetDev_TxEnqueue          (NET_DEV_DATA       *pdev_data,
                                         CPU_INT08U         *p_data,
                                         CPU_INT16U          size,
                                         NET_DRV_ERR        *p_err);

static  void  NetDev_TxDequeue          (NET_DEV_DATA       *pdev_data,
                                         DEV_BUF_Q          *buf_q,
                                         NET_DRV_ERR        *p_err);


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      NETWORK DEVICE DRIVER API
*********************************************************************************************************
*/

const  NET_DEV_API_ETHER  NetDev_API_GEM64 = {                                /* Ether DMA dev API fnct ptrs :*/
                                               &NetDev_Init,                  /*   Init/add                   */
                                               &NetDev_Start,                 /*   Start                      */
                                               &NetDev_Stop,                  /*   Stop                       */
                                               &NetDev_Rx,                    /*   Rx                         */
                                               &NetDev_Tx,                    /*   Tx                         */
                                               &NetDev_AddrMulticastAdd,      /*   Multicast addr add         */
                                               &NetDev_AddrMulticastRemove,   /*   Multicast addr remove      */
                                               &NetDev_ISR_Handler,           /*   ISR handler                */
                                               &NetDev_IO_Ctrl,               /*   I/O ctrl                   */
                                               &NetDev_MII_Rd,                /*   Phy reg rd                 */
                                               &NetDev_MII_Wr                 /*   Phy reg wr                 */
                                             };


/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*********************************************************************************************************
*/


typedef  struct  net_dev {
    CPU_REG32  NET_CTRL;                                        /* Network Control.                                     */
    CPU_REG32  NET_CFG;                                         /* Network Configuration.                               */
    CPU_REG32  NET_STATUS;                                      /* Network Status.                                      */
    CPU_REG32  USER_IO;                                         /* User Input/Output.                                   */
    CPU_REG32  DMA_CFG;                                         /* DMA configuration.                                   */
    CPU_REG32  TX_STATUS;                                       /* Transmit Status.                                     */
    CPU_REG32  RX_QBAR;                                         /* Receive Buffer Queue Base Address.                   */
    CPU_REG32  TX_QBAR;                                         /* Transmit Buffer Queue Base Address.                  */
    CPU_REG32  RX_STATUS;                                       /* Receive Status.                                      */
    CPU_REG32  INTR_STATUS;                                     /* Interrupt Status.                                    */
    CPU_REG32  INTR_EN;                                         /* Interrupt Enable.                                    */
    CPU_REG32  INTR_DIS;                                        /* Interrupt Disable.                                   */
    CPU_REG32  INTR_MASK;                                       /* Interrupt Mask Status.                               */
    CPU_REG32  PHY_MAINT;                                       /* PHY Maintenance.                                     */
    CPU_REG32  RX_PAUSEQ;                                       /* Receive Pause Quantum.                               */
    CPU_REG32  TX_PAUSEQ;                                       /* Transmit Pause Quantum.                              */
    CPU_REG32  PBUF_TX_CUTTHRU;                                 /* Transmit Partial Store and Forward.                  */
    CPU_REG32  PBUF_RX_CUTTHRU;                                 /* Receive Partial Store and Forward.                   */
    CPU_REG32  JUMBO_MAX_LENGTH;                                /* Maximum Jumbo Frame Size.                            */
    CPU_REG32  EXTERNAL_FIFO_INTERFACE;                         /* External FIFO Interface enable.                      */
    CPU_REG32  RESERVED1;
    CPU_REG32  AXI_MAX_PIPELINE;                                /* Size of the AXI bus pipeline.                        */
    CPU_REG32  RESERVED2[10];
    CPU_REG32  HASH_BOT;                                        /* Hash Register Bottom.                                */
    CPU_REG32  HASH_TOP;                                        /* Hash Register Top.                                   */
    CPU_REG32  SPEC_ADDR1_BOT;                                  /* Specific Address 1 Bottom.                           */
    CPU_REG32  SPEC_ADDR1_TOP;                                  /* Specific Address 1 Top.                              */
    CPU_REG32  SPEC_ADDR2_BOT;                                  /* Specific Address 2 Bottom.                           */
    CPU_REG32  SPEC_ADDR2_TOP;                                  /* Specific Address 2 Top.                              */
    CPU_REG32  SPEC_ADDR3_BOT;                                  /* Specific Address 3 Bottom.                           */
    CPU_REG32  SPEC_ADDR3_TOP;                                  /* Specific Address 3 Top.                              */
    CPU_REG32  SPEC_ADDR4_BOT;                                  /* Specific Address 4 Bottom.                           */
    CPU_REG32  SPEC_ADDR4_TOP;                                  /* Specific Address 4 Top.                              */
    CPU_REG32  TYPE_ID_MATCH1;                                  /* Type ID Match 1.                                     */
    CPU_REG32  TYPE_ID_MATCH2;                                  /* Type ID Match 2.                                     */
    CPU_REG32  TYPE_ID_MATCH3;                                  /* Type ID Match 3.                                     */
    CPU_REG32  TYPE_ID_MATCH4;                                  /* Type ID Match 4.                                     */
    CPU_REG32  WAKE_ON_LAN;                                     /* Wake on LAN.                                         */
    CPU_REG32  IPG_STRETCH;                                     /* IPG Stretch.                                         */
    CPU_REG32  STACKED_VLAN;                                    /* Stacked VLAN Register.                               */
    CPU_REG32  TX_PFC_PAUSE;                                    /* Transmit PFC Pause Register.                         */
    CPU_REG32  SPEC_ADDR1_MASK_BOT;                             /* Specific Address Mask 1 Bottom.                      */
    CPU_REG32  SPEC_ADDR1_MASK_TOP;                             /* Specific Address Mask 1 Top.                         */
    CPU_REG32  DMA_ADDR_OR_MASK;                                /* Receive DMA Data Buffer Address Mask.                */
    CPU_REG32  RX_PTP_UNICAST;                                  /* PTP RX unicast IP destination address.               */
    CPU_REG32  TX_PTP_UNICAST;                                  /* PTP TX unicast IP destination address.               */
    CPU_REG32  TSU_NSEC_CMP;                                    /* TSU timer comparison value nanoseconds.              */
    CPU_REG32  TSU_SEC_CMP;                                     /* TSU timer comparison value seconds 31:0.             */
    CPU_REG32  TSU_MSB_SEC_CMP;                                 /* TSU timer comparison value seconds 47:32.            */
    CPU_REG32  TSU_PTP_TX_MSB_SEC;                              /* PTP Event Frame Transmitted Seconds Register 47:32.  */
    CPU_REG32  TSU_PTP_RX_MSB_SEC;                              /* PTP Event Frame Received Seconds Register 47:32.     */
    CPU_REG32  TSU_PEER_TX_MSB_SEC;                             /* PTP Peer Event Frame Transmitted Seconds Register 47:32. */
    CPU_REG32  TSU_PEER_RX_MSB_SEC;                             /* PTP Peer Event Frame Received Seconds Register 47:32. */
    CPU_REG32  DPRAM_FILL_DBG;                                  /* TX/RX Packet buffer fill levels.                     */
    CPU_REG32  MODULE_ID;                                       /* Module ID.                                           */
    CPU_REG32  OCTETS_TX_BOT;                                   /* Octets transmitted bottom.                           */
    CPU_REG32  OCTETS_TX_TOP;                                   /* Octets transmitted top.                              */
    CPU_REG32  FRAMES_TX;                                       /* Frames Transmitted.                                  */
    CPU_REG32  BROADCAST_FRAMES_TX;                             /* Broadcast Frames Transmitted.                        */
    CPU_REG32  MULTI_FRAMES_TX;                                 /* Multicast Frames Transmitted.                        */
    CPU_REG32  PAUSE_FRAMES_TX;                                 /* Pause Frames Transmitted.                            */
    CPU_REG32  FRAMES_TXED_64;                                  /* Frames Transmitted. 64 bytes.                        */
    CPU_REG32  FRAMES_TXED_65;                                  /* Frames Transmitted. 65 to 127 bytes.                 */
    CPU_REG32  FRAMES_TXED_128;                                 /* Frames Transmitted. 128 to 255 bytes.                */
    CPU_REG32  FRAMES_TXED_256;                                 /* Frames Transmitted. 256 to 511 bytes.                */
    CPU_REG32  FRAMES_TXED_512;                                 /* Frames Transmitted. 512 to 1023 bytes.               */
    CPU_REG32  FRAMES_TXED_1024;                                /* Frames Transmitted. 1024 to 1518 bytes.              */
    CPU_REG32  FRAMES_TXED_1519;                                /* Frames Transmitted. Greater than 1518 bytes.         */
    CPU_REG32  TX_UNDER_RUNS;                                   /* Transmit under runs.                                 */
    CPU_REG32  SINGLE_COLLISN_FRAMES;                           /* Single Collision Frames.                             */
    CPU_REG32  MULTI_COLLISN_FRAMES;                            /* Multi Collision Frames.                              */
    CPU_REG32  EXCESSIVE_COLLISNS;                              /* Excesive Collisions.                                 */
    CPU_REG32  LATE_COLLISNS;                                   /* Late Collisions.                                     */
    CPU_REG32  DEFFERED_TX_FRAMES;                              /* Deffered Transmission Frames.                        */
    CPU_REG32  CARRIER_SENSE_ERRS;                              /* Carrier Sense Errors.                                */
    CPU_REG32  OCTETS_RX_BOT;                                   /* Octets Received Bottom.                              */
    CPU_REG32  OCTETS_RX_TOP;                                   /* Octets Received Top.                                 */
    CPU_REG32  FRAMES_RX;                                       /* Frames Received.                                     */
    CPU_REG32  BROADCAST_TXED;
    CPU_REG32  MULTICAST_TXED;
    CPU_REG32  PAUSE_FRAMES_TXED;
    CPU_REG32  FRAMES_RXED_64;
    CPU_REG32  FRAMES_RXED_65;
    CPU_REG32  FRAMES_RXED_128;
    CPU_REG32  FRAMES_RXED_256;
    CPU_REG32  FRAMES_RXED_512;
    CPU_REG32  FRAMES_RXED_1024;
    CPU_REG32  FRAMES_RXED_1519;
    CPU_REG32  UNDERSIZE_FRAMES;
    CPU_REG32  EXCESSIVE_RX_LENGTH;
    CPU_REG32  RX_JABBERS;
    CPU_REG32  FCS_ERRORS;
    CPU_REG32  RX_LENGTH_ERRORS;
    CPU_REG32  RX_SYMBOL_ERRORS;
    CPU_REG32  ALIGNMENT_ERRORS;
    CPU_REG32  RX_RESOURCE_ERRORS;
    CPU_REG32  RX_OVERRUNS;
    CPU_REG32  RX_IP_CK_ERRORS;
    CPU_REG32  RX_TCP_CK_ERRORS;
    CPU_REG32  RX_UDP_CK_ERRORS;
    CPU_REG32  AUTO_FLUSHED_PKTS;
    CPU_REG32  RESERVED3;
    CPU_REG32  TSU_TIMER_INCR_SUB_NSEC;
    CPU_REG32  TSU_TIMER_MSB_SEC;
    CPU_REG32  TSU_STROBE_MSB_SEC;
    CPU_REG32  TSU_STROBE_SEC;
    CPU_REG32  TSU_STROBE_NSEC;
    CPU_REG32  TSU_TIMER_SEC;
    CPU_REG32  TSU_TIMER_NSEC;
    CPU_REG32  TSU_TIMER_ADJUST;
    CPU_REG32  TSU_TIMER_INCR;
    CPU_REG32  TSU_PTP_TX_SEC;
    CPU_REG32  TSU_PTP_TX_NSEC;
    CPU_REG32  TSU_PTP_RX_SEC;
    CPU_REG32  TSU_PTP_RX_NSEC;
    CPU_REG32  TSU_PEER_TX_SEC;
    CPU_REG32  TSU_PEER_TX_NSEC;
    CPU_REG32  TSU_PEER_RX_SEC;
    CPU_REG32  TSU_PEER_RX_NSEC;
    CPU_REG32  PCS_CONTROL;
    CPU_REG32  PCS_STATUS;
    CPU_REG32  PCS_PHY_TOP_ID;
    CPU_REG32  PCS_PHY_BOT_ID;
    CPU_REG32  PCS_AN_ADV;
    CPU_REG32  PCS_AN_LP_BASE;
    CPU_REG32  PCS_AN_EXP;
    CPU_REG32  PCS_AN_NP_TX;
    CPU_REG32  PCS_AN_LP_NP;
    CPU_REG32  RESERVED4[6];
    CPU_REG32  PCS_AN_EXT_STATUS;
    CPU_REG32  RESERVED5[12];
    CPU_REG32  RX_LPI;
    CPU_REG32  RX_LPI_TIME;
    CPU_REG32  TX_LPI;
    CPU_REG32  TX_LPI_TIME;
    CPU_REG32  DESIGNCFG_DEBUG1;
    CPU_REG32  DESIGNCFG_DEBUG2;
    CPU_REG32  DESIGNCFG_DEBUG3;
    CPU_REG32  DESIGNCFG_DEBUG4;
    CPU_REG32  DESIGNCFG_DEBUG5;
    CPU_REG32  DESIGNCFG_DEBUG6;
    CPU_REG32  DESIGNCFG_DEBUG7;
    CPU_REG32  DESIGNCFG_DEBUG8;
    CPU_REG32  DESIGNCFG_DEBUG9;
    CPU_REG32  DESIGNCFG_DEBUG10;
    CPU_REG32  RESERVED6[86];
    CPU_REG32  INT_Q1_STATUS;
    CPU_REG32  RESERVED7[15];
    CPU_REG32  TRANSMIT_Q1_PTR;
    CPU_REG32  RESERVED8[15];
    CPU_REG32  RECEIVE_Q1_PTR;
    CPU_REG32  RESERVED9[7];
    CPU_REG32  DMA_RXBUF_SIZE_Q1;
    CPU_REG32  RESERVED10[6];
    CPU_REG32  CBS_CONTROL;
    CPU_REG32  CBS_IDLESLOPE_Q_A;
    CPU_REG32  CBS_IDLESLOPE_Q_B;
    CPU_REG32  UPPER_TX_Q_BASE_ADDR;
    CPU_REG32  TX_BD_CONTROL;
    CPU_REG32  RX_BD_CONTROL;
    CPU_REG32  UPPER_RX_Q_BASE_ADDR;
    CPU_REG32  RESERVED11[10];
    CPU_REG32  SCREENING_TYPE_1_REGISTER_0;
    CPU_REG32  SCREENING_TYPE_1_REGISTER_1;
    CPU_REG32  SCREENING_TYPE_1_REGISTER_2;
    CPU_REG32  SCREENING_TYPE_1_REGISTER_3;
    CPU_REG32  RESERVED12[12];
    CPU_REG32  SCREENING_TYPE_2_REGISTER_0;
    CPU_REG32  SCREENING_TYPE_2_REGISTER_1;
    CPU_REG32  SCREENING_TYPE_2_REGISTER_2;
    CPU_REG32  SCREENING_TYPE_2_REGISTER_3;
    CPU_REG32  RESERVED13[44];
    CPU_REG32  INT_Q1_ENABLE;
    CPU_REG32  RESERVED14[7];
    CPU_REG32  INT_Q1_DISABLE;
    CPU_REG32  RESERVED15[7];
    CPU_REG32  INT_Q1_MASK;
    CPU_REG32  RESERVED16[39];
    CPU_REG32  SCREENING_TYPE_2_ETHERTYPE_REG_0;
    CPU_REG32  SCREENING_TYPE_2_ETHERTYPE_REG_1;
    CPU_REG32  SCREENING_TYPE_2_ETHERTYPE_REG_2;
    CPU_REG32  SCREENING_TYPE_2_ETHERTYPE_REG_3;
    CPU_REG32  RESERVED17[4];
    CPU_REG32  TYPE_2_COMPARE_0_WORD_0;
    CPU_REG32  TYPE_2_COMPARE_0_WORD_1;
    CPU_REG32  TYPE_2_COMPARE_1_WORD_0;
    CPU_REG32  TYPE_2_COMPARE_1_WORD_1;
    CPU_REG32  TYPE_2_COMPARE_2_WORD_0;
    CPU_REG32  TYPE_2_COMPARE_2_WORD_1;
    CPU_REG32  TYPE_2_COMPARE_3_WORD_0;
    CPU_REG32  TYPE_2_COMPARE_3_WORD_1;
} NET_DEV;


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
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INVALID_CFG     Device configuration is invalid.
*                           NET_PHY_ERR_INVALID_CFG     PHY configuration is invalid.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *p_err)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbytes;
    LIB_ERR             lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets & (BUF_ALIGN_OCTETS - 1u)) != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf size.                                */
    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize(pif->Nbr,
                                     NET_TRANSACTION_RX,
                                     0,
                                     buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf alignment.                           */
    if ((pdev_cfg->TxBufAlignOctets & (BUF_ALIGN_OCTETS - 1u)) != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }


    if (pphy_cfg == (void *)0) {
       *p_err = NET_PHY_ERR_INVALID_CFG;
        return;
    }

                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    pif->Dev_Data = Mem_HeapAlloc( sizeof(NET_DEV_DATA),
                                   64u,
                                  &reqd_octets,
                                  &lib_err);
    if (pif->Dev_Data == (void *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;
                                                                /* ==================================================== */
                                                                /* =================== DEV_DATA INIT ================== */
                                                                /* ==================================================== */
    pdev_data->RxDescNbr = pdev_cfg->RxDescNbr;                 /* # Rx Descriptors: User configured.                   */
    pdev_data->TxDescNbr = pdev_cfg->TxDescNbr;                 /* # Tx Descriptors: User configured.                   */

                                                                /* ---------------- RX DMA DESCRIPTORS ---------------- */
    nbytes = pdev_data->RxDescNbr * sizeof(DEV_DESC);           /* Determine block size.                                */
    Mem_PoolCreate(       &pdev_data->RxDescPool,               /* Pass a pointer to the mem pool to create.            */
                   (void *)pdev_cfg->MemAddr,                   /* From the dedicated memory.                           */
                           pdev_cfg->MemSize,                   /* Dedicated area size.                                 */
                           1u,                                  /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold all Rx descriptors.  */
                           8u,                                  /* Block alignment.                                     */
                           0,                                   /* Optional, ptr to variable to store rem nbr bytes.    */
                          &lib_err);                            /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ---------------- TX DMA DESCRIPTORS ---------------- */
    nbytes = (pdev_data->TxDescNbr + 1u) * sizeof(DEV_DESC);    /* Determine block size. Extra desc to terminate list.  */
    Mem_PoolCreate(       &pdev_data->TxDescPool,               /* Pass a pointer to the mem pool to create.            */
                   (void *)pdev_cfg->MemAddr,                   /* From the dedicated memory.                           */
                           pdev_cfg->MemSize,                   /* Dedicated area size.                                 */
                           1u,                                  /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold all Tx descriptors.  */
                           8u,                                  /* Block alignment.                                     */
                           0,                                   /* Optional, ptr to variable to store rem nbr bytes.    */
                          &lib_err);                            /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------------------ TX BUFFER QUEUE ----------------- */
    nbytes = pdev_data->TxDescNbr * sizeof(DEV_BUF_Q);          /* Determine block size.                                */
    Mem_PoolCreate(       &pdev_data->TxQPool,                  /* Pass a pointer to the mem pool to create.            */
                   (void *)pdev_cfg->MemAddr,                   /* From the dedicated memory.                           */
                           pdev_cfg->MemSize,                   /* Dedicated area size.                                 */
                           1u,                                  /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold Tx buffer queue.     */
                           sizeof(DEV_BUF_Q *),                 /* Block alignment.                                     */
                           0,                                   /* Optional, ptr to variable to store rem nbr bytes.    */
                          &lib_err);                            /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------------- PRIORITY DMA DESCRIPTORS ------------- */
                                                                /* Dummy descriptors for priority queue.                */
    nbytes = DEV_NUM_PRI_Q_PTRS * sizeof(DEV_DESC);             /* Determine block size.                                */
    Mem_PoolCreate(       &pdev_data->PriQDescPool,             /* Pass a pointer to the mem pool to create.            */
                   (void *)pdev_cfg->MemAddr,                   /* From the dedicated memory.                           */
                           pdev_cfg->MemSize,                   /* Dedicated area size.                                 */
                           1u,                                  /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold dummy descriptors.   */
                           8u,                                  /* Block alignment.                                     */
                           0,                                   /* Optional, ptr to variable to store rem nbr bytes.    */
                          &lib_err);                            /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }
                                                                /* ==================================================== */
                                                                /* =================== HARDWARE CFG =================== */
                                                                /* ==================================================== */
                                                                /* ------------- ENABLE NECESSARY CLOCKS -------------- */
                                                                /* Enable module clks.                    				*/
    pdev_bsp->CfgClk(pif, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

                                                                /* ------- INITIALIZE EXTERNAL GPIO CONTROLLER -------- */
                                                                /* Configure Ethernet Controller GPIO.                  */
    pdev_bsp->CfgGPIO(pif, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
                                                                /* Configure ext int ctrl'r.                            */
    pdev_bsp->CfgIntCtrl(pif, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Start()
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
* Argument(s) : pif         Pointer to a network interface.
*               ---         Argument validated in NetIF_Start().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
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
*                                                               -- RETURNED BY NetIF_DevCfgTxRdySignal() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
*                                                               -------- RETURNED BY RxDescInit() : --------
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                               NET_BUF_ERR_INVALID_IX          Invalid index.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                               maximum network buffer data area size
*                                                               available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                               buffer's data area.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *p_err)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pphy_cfg  = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev_bsp  = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;


                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;

    NetASCII_Str_to_MAC( pdev_cfg->HW_AddrStr,                  /* Get configured HW MAC address string, if any.        */
                        &hw_addr[0u],
                        &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler( pif->Nbr,
                                &hw_addr[0u],
                                 sizeof(hw_addr),
                                &err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.    */
        hw_addr_cfg = DEF_YES;
    } else {                                                    /* Else get  app-configured IF layer HW MAC address, ...*/
                                                                /* ... if any.                                          */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0u], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0u], &err);
        } else {
            hw_addr_cfg = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any.                          */
            hw_addr[0u] = (pdev->SPEC_ADDR1_BOT >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1u] = (pdev->SPEC_ADDR1_BOT >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2u] = (pdev->SPEC_ADDR1_BOT >> (2u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3u] = (pdev->SPEC_ADDR1_BOT >> (3u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4u] = (pdev->SPEC_ADDR1_TOP >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5u] = (pdev->SPEC_ADDR1_TOP >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            NetIF_AddrHW_SetHandler( pif->Nbr,                  /* Configure IF layer to use automatically-loaded ...   */
                                    &hw_addr[0u],               /* ... HW MAC address.                                  */
                                     sizeof(hw_addr),
                                     p_err);
            if (*p_err != NET_IF_ERR_NONE) {                    /* No valid HW MAC address configured, return error.    */
                return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        pdev->SPEC_ADDR1_BOT = (((CPU_INT32U)hw_addr[0u] << (0u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[1u] << (1u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[2u] << (2u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[3u] << (3u * DEF_INT_08_NBR_BITS)));

        pdev->SPEC_ADDR1_TOP = (((CPU_INT32U)hw_addr[4u] << (0u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[5u] << (1u * DEF_INT_08_NBR_BITS)));
    }

                                                                /* ------------------ CFG MULTICAST ------------------- */
    pdev->HASH_BOT = 0u;
    pdev->HASH_TOP = 0u;

    pdev->NET_CFG |= GEM_BIT_CFG_UNI_HASH_EN   |                /* Enable Unicast/Multicast Hashing.                    */
                     GEM_BIT_CFG_MULTI_HASH_EN;
#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr(       pdev_data->MulticastAddrHashBitCtr,
            sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, p_err);                              /* Init Rx desc's.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_TxDescInit(pif, p_err);                              /* Init Tx desc's.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_PriQDescInit(pif, p_err);                            /* Initialize priority Q descriptors.                   */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,
                            pdev_cfg->TxDescNbr,
                            p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        return;
    }

                                                                /* ---------------- DMA ENGINE STATUS ----------------- */
    pdev_data->TxBusy = DEF_FALSE;

                                                                /* ----------------- START DMA ENGINE ----------------- */
    pdev->INTR_STATUS |= INT_STATUS_MASK_ALL;                   /* Clear all pending int. sources.                      */
    pdev->RX_STATUS    = 0xFFFFFFFF;
    pdev->TX_STATUS    = 0xFFFFFFFF;
    pdev->INTR_EN      = INT_STATUS_MASK_SUPPORTED;             /* Enable Rx, Tx and other supported int. sources.      */

    pdev->NET_CFG = GEM_BIT_CFG_DBUS_WIDTH(0x1)     |
                    GEM_BIT_CFG_MDC_CLK_DIV(0x5)    |
                    GEM_BIT_CFG_FCS_REMOVE          |
                    GEM_BIT_CFG_FULL_DUPLEX         |
                    GEM_BIT_CFG_SPEED;

    if (pphy_cfg->BusMode == NET_PHY_BUS_MODE_GMII) {           /* Configure for GMII or MII.                           */
        pdev->NET_CFG |=  GEM_BIT_CFG_GIGE_EN;
    } else {
        pdev->NET_CFG &= ~GEM_BIT_CFG_GIGE_EN;
    }


    pdev->DMA_CFG =
        GEM_BIT_DMACFG_ADDR_BUS_WIDTH       |
        GEM_BIT_DMACFG_DISC_WHEN_NO_AHB     |
        GEM_BIT_DMACFG_AHB_RX_SIZE(0x18u)   |
        GEM_BIT_DMACFG_CSUM_GEN_OFFLOAD_EN  |
        GEM_BIT_DMACFG_TX_PKTBUF_MEMSZ_SEL  |
        GEM_BIT_DMACFG_RX_PKTBUF_SZ(0x3)    |
        GEM_BIT_DMACFG_RX_AHB_BURST(0x1F);

    CPU_MB();
                                                                /* ------------------ ENABLE RX & TX ------------------ */
    pdev->NET_CTRL |= GEM_BIT_CTRL_TX_EN        |               /* Enable transmitter & receiver.                       */
                      GEM_BIT_CTRL_RX_EN        |
                      GEM_BIT_CTRL_MGMT_PORT_EN;

    CPU_MB();

    pdev_bsp->CfgClk(pif, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
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
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE    No Error
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Stop() via 'pdev_api->Stop()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    pdev->NET_CTRL &= ~(GEM_BIT_CTRL_TX_EN  |                   /* Disable transmitter & receiver.                      */
                        GEM_BIT_CTRL_RX_EN);

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    pdev->INTR_DIS    |= INT_STATUS_MASK_SUPPORTED;             /* Disable Rx, Tx and other supported int. sources.     */
    pdev->INTR_STATUS |= INT_STATUS_MASK_ALL;                   /* Clear all pending int. sources.                      */


    NetDev_RxDescFreeAll(pif, p_err);                           /* Free Rx desc's.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_TxDescFreeAll(pif, p_err);                           /* Free Tx desc's.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_PriQDescFreeAll(pif, p_err);                         /* Free PriQ desc's.                                    */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
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
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE                No Error
*                           NET_DEV_ERR_RX                  Generic Rx error.
*                           NET_DEV_ERR_INVALID_SIZE        Invalid Rx frame size.
*
*                                                           ----- RETURNED BY NetBuf_GetDataPtr() : -----
*                           NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                           NET_BUF_ERR_INVALID_IX          Invalid index.
*                           NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                           maximum network buffer data area size
*                                                           available.
*                           NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                           buffer's data area.
*                           NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
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
                         NET_ERR      *p_err)
{
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    CPU_INT08U    *pbuf_new;
    CPU_INT32U     rx_len;
    CPU_INT64U     addr;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;
    pdesc     = (DEV_DESC     *)pdev_data->RxBufDescPtrCur;

                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
    addr = ((CPU_INT64U)pdesc->AddrHigh << 32u) | pdesc->Addr;
    if ((addr & 1u) == 0) {
       *size   = 0;
       *p_data = 0;
       *p_err  = NET_DEV_ERR_RX;
        return;
    }
                                                                /* --------------- OBTAIN FRAME LENGTH ---------------- */
    rx_len = (pdesc->Status & GEM_RXBUF_SIZE_MASK);
    if (rx_len < NET_IF_ETHER_FRAME_MIN_SIZE) {                 /* If frame is a runt, ...                              */
        NetDev_RxDescPtrCurInc(pif);                            /* ... discard rx'd frame.                              */
       *size   = 0;
       *p_data = 0;
       *p_err  = NET_DEV_ERR_INVALID_SIZE;
        return;
    }

                                                                /* --------- OBTAIN PTR TO NEW DMA DATA AREA ---------- */
                                                                /* Request an empty buffer.                             */
    pbuf_new = NetBuf_GetDataPtr(pif,
                                 NET_TRANSACTION_RX,
                                 NET_IF_ETHER_FRAME_MAX_SIZE,
                                 NET_IF_IX_RX,
                                 0,
                                 0,
                                 0,
                                 p_err);
    if (*p_err != NET_BUF_ERR_NONE) {                           /* If unable to get a buffer.                           */
        NetDev_RxDescPtrCurInc(pif);                            /* Free the current descriptor.                         */
       *size   = 0;
       *p_data = 0;
        return;
    }

   *size   =                rx_len;                             /* Return the size of the received frame.               */
   *p_data = (CPU_INT08U *)(addr & GEM_RXBUF_ADDR_MASK);        /* Return a pointer to the newly received data area.    */

    CPU_DCACHE_RANGE_INV(*p_data, *size);                       /* Invalidate received buffer.                          */

    if(pdesc == pdev_data->RxBufDescPtrEnd) {                   /* Update the descriptor to point to a new data area    */
        pdesc->Addr = (CPU_INT32U)((CPU_INT64U)(void *)pbuf_new & GEM_RXBUF_ADDR_MASK) | GEM_RXBUF_ADDR_OWN | GEM_RXBUF_ADDR_WRAP;
    } else {
        pdesc->Addr = (CPU_INT32U)((CPU_INT64U)(void *)pbuf_new & GEM_RXBUF_ADDR_MASK) | GEM_RXBUF_ADDR_OWN;
    }
    pdesc->AddrHigh = (CPU_INT32U)((CPU_INT64U)(void *)pbuf_new >> 32u); /* Update the high-order address word.                  */

   *p_err = NET_DEV_ERR_NONE;
    NetDev_RxDescPtrCurInc(pif);                                /* Free the current descriptor.                         */
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
*               p_data  Pointer to data to transmit.
*
*               size    Size    of data to transmit.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE        No Error
*                           NET_DEV_ERR_TX_BUSY     No Tx descriptors available
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
                         NET_ERR     *p_err)
{
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DRV_ERR         drv_err;
    CPU_SR_ALLOC();

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;


    CPU_DCACHE_RANGE_FLUSH(p_data, size);                       /* Flush/Clean buffer to send.                          */

    CPU_CRITICAL_ENTER();
    if (pdev_data->TxBusy == DEF_TRUE) {                        /* Tx engine is already active.                         */
        NetDev_TxEnqueue(pdev_data, p_data, size, &drv_err);    /* Attempt to Queue the packet.                         */
        CPU_CRITICAL_EXIT();

       *p_err = (drv_err == NET_DRV_ERR_NONE) ? NET_DEV_ERR_NONE : NET_DEV_ERR_TX_BUSY;

        return;                                                 /* Return with TX_BUSY if the Queue is full.            */
    }
    CPU_CRITICAL_EXIT();
                                                                /* ---------------------------------------------------- */
                                                                /* ------------------ TX ENGINE IDLE ------------------ */
                                                                /* ---------------------------------------------------- */
    pdev_data->TxBusy     = DEF_TRUE;
    pdev_data->TxRingSize = 1u;


    pdesc           = pdev_data->TxRingHead;                    /* Insert frame into the descriptor list.               */
    pdesc->Addr     = (CPU_INT32U)((CPU_INT64U)(void *)p_data &  GEM_TXBUF_ADDR_MASK);
    pdesc->AddrHigh = (CPU_INT32U)((CPU_INT64U)(void *)p_data >>                 32u);
    pdesc->Status   = (((size) & GEM_TXBUF_LENGTH_MASK) | GEM_TXBUF_LAST);

    pdesc++;
    pdesc->Status = GEM_TXBUF_USED;                             /* Terminate the descriptor list.                       */

                                                                /* Point the DMA engine to our list.                    */
    pdev->TX_QBAR              = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->TxRingHead);
    pdev->UPPER_TX_Q_BASE_ADDR = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->TxRingHead >> 32u);

    CPU_MB();                                                   /* Force writes to buf & desc to be visible to the MAC. */
    pdev->NET_CTRL |= GEM_BIT_CTRL_START_TX;
    CPU_MB();

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_RxDescInit()
*
* Description : This function initializes the Rx descriptor list for the specified interface.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE                Rx Descriptors initialized successfully.
*                           NET_DEV_ERR_MEM_ALLOC           Failed to allocate block from Rx mem pool.
*
*                                                           ----- RETURNED BY NetBuf_GetDataPtr() : -----
*                           NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                           NET_BUF_ERR_INVALID_IX          Invalid index.
*                           NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                           maximum network buffer data area size
*                                                           available.
*                           NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                           buffer's data area.
*                           NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;
    void               *p_buf;
    MEM_POOL           *pmem_pool;
    CPU_SIZE_T          nbr_octets;
    LIB_ERR             lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
    pmem_pool  =            &pdev_data->RxDescPool;
    nbr_octets =             pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet( pmem_pool,
                                             nbr_octets,
                                            &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxBufDescPtrStart = pdesc;
    pdev_data->RxBufDescPtrCur   = pdesc;
    pdev_data->RxBufDescPtrEnd   = pdesc + (pdev_cfg->RxDescNbr - 1);

                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->Status = 0;
        p_buf         = NetBuf_GetDataPtr(pif,
                                          NET_TRANSACTION_RX,
                                          NET_IF_ETHER_FRAME_MAX_SIZE,
                                          NET_IF_IX_RX,
                                          0,
                                          0,
                                          0,
                                          p_err);
        if (*p_err != NET_BUF_ERR_NONE) {
            return;
        }

        CPU_DCACHE_RANGE_FLUSH(p_buf, pdev_cfg->RxBufLargeSize);

        pdesc->Addr     = (CPU_INT32U)((CPU_INT64U)p_buf &  ~GEM_RXBUF_ADDR_OWN);
        pdesc->AddrHigh = (CPU_INT32U)((CPU_INT64U)p_buf >>                 32u);

        if (pdesc == (pdev_data->RxBufDescPtrEnd)) {            /* Set WRAP bit on last descriptor in list.             */
            pdesc->Addr |= GEM_RXBUF_ADDR_WRAP;
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }
                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start address.    */
    pdev->RX_QBAR              = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->RxBufDescPtrStart);
    pdev->UPPER_RX_Q_BASE_ADDR = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->RxBufDescPtrStart >> 32u);

    *p_err = NET_DEV_ERR_NONE;
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
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_DEV_ERR_FAULT       Failed to free Rx memory blocks
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL receive buffers
*                   and the Rx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;
    CPU_INT08U         *pdesc_data;
    MEM_POOL           *pmem_pool;
    LIB_ERR             lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;

                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pdesc = pdev_data->RxBufDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */
        pdesc_data = (CPU_INT08U *)(((CPU_INT64U)pdesc->AddrHigh << 32u) | (pdesc->Addr & GEM_RXBUF_ADDR_MASK));
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

    pmem_pool = &pdev_data->RxDescPool;
    Mem_PoolBlkFree(pmem_pool, pdev_data->RxBufDescPtrStart, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_TxDescInit()
*
* Description : (1) This function initializes the Tx descriptor list for the specified interface :
*
*                   (a) Obtain reference to the Tx descriptor(s) memory block
*                   (b) Initialize Tx descriptor pointers
*                   (c) Obtain Tx descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE                Tx Descriptors initialized successfully.
*                           NET_DEV_ERR_MEM_ALLOC           Failed to allocate block from Tx mem pool.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_TxDescInit (NET_IF   *pif,
                                 NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    NET_BUF_QTY         i;
    CPU_INT32U          nbytes;
    LIB_ERR             lib_err;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;


                                                                /* ---------------- ALLOCATE TX MEMORY ---------------- */
    pdev_data->TxRingSize =             0u;
    nbytes                =            (pdev_data->TxDescNbr + 1u) * sizeof(DEV_DESC);
    pdev_data->TxRingHead = (DEV_DESC *)Mem_PoolBlkGet(&pdev_data->TxDescPool,
                                                        nbytes,
                                                       &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    nbytes              =              pdev_data->TxDescNbr * sizeof(DEV_BUF_Q);
    pdev_data->TxQFront =              0u;
    pdev_data->TxQBack  =              0u;
    pdev_data->TxQSize  =              0u;
    pdev_data->TxQ      = (DEV_BUF_Q *)Mem_PoolBlkGet(&pdev_data->TxQPool,
                                                       nbytes,
                                                      &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* ----------------- INIT DESCRIPTORS ----------------- */
    pdesc = pdev_data->TxRingHead;
    for (i = 0u; i < pdev_cfg->TxDescNbr; i++) {                /* Init Tx descriptor ring.                             */
        pdesc->Status = GEM_TXBUF_USED;
        pdesc++;
    }

    pdesc->Status              = GEM_TXBUF_USED;                /* Terminate the descriptor list.                       */

    pdev_data->TxRingSize      = 0u;                            /* Ring is empty until we have something to transmit.   */
                                                                /* Point the DMA engine to our list.                    */
    pdev->TX_QBAR              = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->TxRingHead);
    pdev->UPPER_TX_Q_BASE_ADDR = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->TxRingHead >> 32u);

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_TxDescFreeAll()
*
* Description : (1) This function returns the descriptor memory block and descriptor data area
*                   memory blocks back to their respective memory pools :
*
*                   (a) Free Tx descriptor data areas
*                   (b) Free Tx descriptor memory block
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_DEV_ERR_FAULT       Failed to free Tx memory blocks
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : (1) Tx interrupts should be stopped when this function is called
*********************************************************************************************************
*/

static  void  NetDev_TxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *p_err)
{
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    DEV_BUF_Q      tx_buf_q;
    NET_ERR        err;
    NET_DRV_ERR    drv_err;
    LIB_ERR        lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;

                                                                /* ------------- FLUSH THE SOFTWARE QUEUE ------------- */
    do {
        NetDev_TxDequeue(pdev_data, &tx_buf_q, &drv_err);
        if (drv_err != NET_DRV_ERR_Q_EMPTY) {
            NetIF_TxDeallocTaskPost(tx_buf_q.DataPtr, &err);
        } else {
            break;
        }
    } while (1);

                                                                /* --------------- FLUSH THE DMA ENGINE --------------- */
    pdesc = pdev_data->TxRingHead;
    while (pdev_data->TxRingSize > 0u) {
        NetIF_TxDeallocTaskPost((CPU_INT08U *)(((CPU_INT64U)pdesc->AddrHigh << 32u) | pdesc->Addr), &err);
        pdesc++;
        pdev_data->TxRingSize--;
    }

                                                                /* -------------- FREE TX MEMORY BLOCKS --------------- */
    Mem_PoolBlkFree(&pdev_data->TxDescPool,
                     pdev_data->TxRingHead,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

    Mem_PoolBlkFree(&pdev_data->TxQPool,
                     pdev_data->TxQ,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetDev_PriQDescInit()
*
* Description : (1) This function initializes the Priority Q descriptor lists for the specified interface :
*
*                   (a) Obtain reference to the PriQ descriptor(s) memory block
*                   (b) Initialize PriQ descriptor pointers
*                   (c) Obtain PriQ descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_DEV_ERR_MEM_ALLOC   Failed to allocate block from PriQ mem pool.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_PriQDescInit (NET_IF   *pif,
                                   NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;
    MEM_POOL           *pmem_pool;
    CPU_SIZE_T          nbr_octets;
    LIB_ERR             lib_err;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg   = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data  = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev       = (NET_DEV           *)pdev_cfg->BaseAddr;

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
    pmem_pool  = &pdev_data->PriQDescPool;
    nbr_octets =  DEV_NUM_PRI_Q_PTRS * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet( pmem_pool,
                                             nbr_octets,
                                            &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->PriQDescPtr = pdesc;

                                                                /* ------------- TERMINATE PRIORITY QUEUE ------------- */
    pdev->TRANSMIT_Q1_PTR = pdesc;
    pdesc->Status         = GEM_TXBUF_USED | GEM_TXBUF_WRAP;
    pdesc->Addr           = 0xFFFFFFFFu;
    pdesc->AddrHigh       = 0xFFFFFFFFu;
    pdesc++;

    pdev->RECEIVE_Q1_PTR = pdesc;
    pdesc->Status        = GEM_RXBUF_ADDR_OWN | GEM_RXBUF_ADDR_WRAP;
    pdesc->Addr          = 0xFFFFFFFFu;
    pdesc->AddrHigh      = 0xFFFFFFFFu;

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_PriQDescFreeAll()
*
* Description : (1) This function returns the PriQ descriptor memory block back to its memory pool.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_DEV_ERR_FAULT       Failed to free Tx memory blocks
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : (1) Tx interrupts should be stopped when this function is called
*********************************************************************************************************
*/

static  void  NetDev_PriQDescFreeAll(NET_IF   *pif,
                                     NET_ERR  *p_err)
{
    NET_DEV_DATA  *pdev_data;
    MEM_POOL      *pmem_pool;
    LIB_ERR        lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;

    pmem_pool = &pdev_data->PriQDescPool;
    Mem_PoolBlkFree(pmem_pool, pdev_data->PriQDescPtr, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
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

static  void  NetDev_RxDescPtrCurInc (NET_IF  *pif)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    NET_DEV            *pdev;
    NET_ERR             err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdesc     = (DEV_DESC          *)pdev_data->RxBufDescPtrCur;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;


    pdesc        =  pdev_data->RxBufDescPtrCur;                 /* Obtain pointer to current Rx descriptor.             */
    pdesc->Addr &= ~GEM_RXBUF_ADDR_OWN;

    if (pdev_data->RxBufDescPtrCur != pdev_data->RxBufDescPtrEnd) {
        pdev_data->RxBufDescPtrCur++;                           /* Point to next Buffer Descriptor.                     */
    } else {                                                    /* Wrap around end of descriptor list if necessary.     */
        pdev_data->RxBufDescPtrCur = pdev_data->RxBufDescPtrStart;
    }

    pdesc = pdev_data->RxBufDescPtrCur;
    if (pdesc->Addr & GEM_RXBUF_ADDR_OWN) {
        NetIF_RxTaskSignal(pif->Nbr, &err);
    } else {
        CPU_MB();
        pdev->INTR_EN = GEM_BIT_INT_RX_COMPLETE;
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
*               p_err       Pointer to variable that will receive the return error code from this function :
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U          bit_nbr;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_SR_ALLOC();

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;

                                                                /* Calculate the 6-bit hash value.                      */
    bit_nbr =   ((paddr_hw[0]       & 0x3F)                              & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[0] >> 6) & 0x3) | ((paddr_hw[1] & 0xF) << 2)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[1] >> 4) & 0xF) | ((paddr_hw[2] & 0x3) << 4)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[2] >> 2) & 0x3F))                             & DEF_BIT_FIELD(6, 0u))
            ^   ((paddr_hw[3]       & 0x3F)                              & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[3] >> 6) & 0x3) | ((paddr_hw[4] & 0xF) << 2)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[4] >> 4) & 0xF) | ((paddr_hw[5] & 0x3) << 4)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[5] >> 2) & 0x3F))                             & DEF_BIT_FIELD(6, 0u));


    CPU_CRITICAL_ENTER();
    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[bit_nbr];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    if (*paddr_hash_ctrs == 1u) {
        if (bit_nbr > 31u) {
            DEF_BIT_SET(pdev->HASH_TOP, DEF_BIT(bit_nbr - 32u));
        } else {
            DEF_BIT_SET(pdev->HASH_BOT, DEF_BIT(bit_nbr));
        }
    }
    CPU_CRITICAL_EXIT();

#else
   (void)pif;                                                   /* Prevent 'variable unused' compiler warnings.         */
   (void)paddr_hw;
#endif

   (void)addr_hw_len;                                           /* Prevent 'variable unused' compiler warning.          */

   *p_err = NET_DEV_ERR_NONE;
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
*               p_err       Pointer to variable that will receive the return error code from this function :
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U          bit_nbr;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_SR_ALLOC();

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;

    bit_nbr =   ((paddr_hw[0]       & 0x3F)                              & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[0] >> 6) & 0x3) | ((paddr_hw[1] & 0xF) << 2)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[1] >> 4) & 0xF) | ((paddr_hw[2] & 0x3) << 4)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[2] >> 2) & 0x3F))                             & DEF_BIT_FIELD(6, 0u))
            ^   ((paddr_hw[3]       & 0x3F)                              & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[3] >> 6) & 0x3) | ((paddr_hw[4] & 0xF) << 2)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[4] >> 4) & 0xF) | ((paddr_hw[5] & 0x3) << 4)) & DEF_BIT_FIELD(6, 0u))
            ^ ((((paddr_hw[5] >> 2) & 0x3F))                             & DEF_BIT_FIELD(6, 0u));

    CPU_CRITICAL_ENTER();
    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[bit_nbr];

    if (*paddr_hash_ctrs > 1u) {
      (*paddr_hash_ctrs)--;                                     /* Decrement hash bit reference ctr.                    */
        CPU_CRITICAL_EXIT();
       *p_err = NET_DEV_ERR_NONE;
        return;
    }

   *paddr_hash_ctrs = 0u;


    if (bit_nbr > 31u) {
        DEF_BIT_CLR(pdev->HASH_TOP, DEF_BIT(bit_nbr - 32u));
    } else {
        DEF_BIT_CLR(pdev->HASH_BOT, DEF_BIT(bit_nbr));
    }

    CPU_CRITICAL_EXIT();

#else
   (void)pif;                                                   /* Prevent 'variable unused' compiler warnings.         */
   (void)paddr_hw;
#endif

   (void)addr_hw_len;                                           /* Prevent 'variable unused' compiler warning.          */

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_ISR_Handler()
*
* Description : This function serves as the device Interrupt Service Routine Handler. This ISR
*               handler MUST service and clear all necessary and enabled interrupt events for
*               the device.
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
* Caller(s)   : Specific first- or second-level BSP ISR handler.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT08U         *p_data;
    DEV_BUF_Q           tx_buf_q;
    NET_ERR             err;
    NET_DRV_ERR         drv_err;
    CPU_INT32U          int_status;


   (void)type;                                                  /* Prevent 'variable unused' compiler warning.          */
                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;

    int_status        = pdev->INTR_STATUS;
    pdev->INTR_STATUS = int_status;                             /* Clr active int. src's.                               */


                                                                /* ==================================================== */
                                                                /* ================== HANDLE RX INTS ================== */
                                                                /* ==================================================== */
    if ((int_status & GEM_BIT_INT_RX_COMPLETE) > 0) {
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task.                              */
        pdev->INTR_DIS = GEM_BIT_INT_RX_COMPLETE;
    }
                                                                /* ==================================================== */
                                                                /* ================== HANDLE TX INTS ================== */
                                                                /* ==================================================== */
    if (int_status & GEM_BIT_INT_TX_USED_READ) {
        pdesc = pdev_data->TxRingHead;

        while (pdev_data->TxRingSize > 0u) {                    /* Signal Net IF that Tx resources are available.       */
            p_data = (CPU_INT08U *)(((CPU_INT64U)pdesc->AddrHigh << 32u) | pdesc->Addr);
            NetIF_TxDeallocTaskPost(p_data, &err);
            NetIF_DevTxRdySignal(pif);
            pdesc++;
            pdev_data->TxRingSize--;
        }

        pdesc = pdev_data->TxRingHead;
        while (pdev_data->TxRingSize < pdev_data->TxDescNbr) {
            NetDev_TxDequeue(pdev_data, &tx_buf_q, &drv_err);
            if (drv_err != NET_DRV_ERR_NONE) {
                break;
            }
            pdesc->Addr     = (CPU_INT32U)((CPU_INT64U)tx_buf_q.DataPtr & GEM_TXBUF_ADDR_MASK);
            pdesc->AddrHigh = (CPU_INT32U)((CPU_INT64U)tx_buf_q.DataPtr >> 32u);
            pdesc->Status   = (((tx_buf_q.Size) & GEM_TXBUF_LENGTH_MASK) | GEM_TXBUF_LAST);

            pdev_data->TxRingSize++;
            pdesc++;
        }

        if (pdev_data->TxRingSize == 0u) {
            pdev_data->TxBusy = DEF_FALSE;                      /* Tx is idle until we have something to transmit.      */
        } else {
            pdesc->Status = GEM_TXBUF_USED;                     /* Terminate the descriptor list.                       */

                                                                /* Start Tx if we had frames in the queue.              */
            pdev->TX_QBAR              = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->TxRingHead);
            pdev->UPPER_TX_Q_BASE_ADDR = (CPU_INT32U)((CPU_INT64U)(void *)pdev_data->TxRingHead >> 32u);
            CPU_MB();
            pdev->NET_CTRL |= GEM_BIT_CTRL_START_TX;
        }
    }

}


/*
*********************************************************************************************************
*                                            NetDev_IO_Ctrl()
*
* Description : This function provides a mechanism for the Phy driver to update the MAC link
*               and duplex settings, as well as a method for the application and link state
*               timer to obtain the current link status.  Additional user specified driver
*               functionality MAY be added if necessary.
*
* Argument(s) : pif     Pointer to interface requiring service.
*
*               opt     Option code representing desired function to perform. The Network Protocol Suite
*                       specifies the option codes below. Additional option codes may be defined by the
*                       driver developer in the driver's header file.
*                           NET_IF_IO_CTRL_LINK_STATE_GET
*                           NET_IF_IO_CTRL_LINK_STATE_UPDATE
*
*                       Driver defined operation codes MUST be defined starting from 20 or higher
*                       to prevent clashing with the pre-defined operation code types. See the
*                       device driver header file for more details.
*
*               data    Pointer to optional data for either sending or receiving additional function
*                       arguments or return data.
*
*               p_err   Pointer to return error code.
*                           NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid option number specified.
*                           NET_ERR_FAULT_NULL_FNCT             Null interface function pointer encountered.
*
*                           NET_DEV_ERR_NONE                    IO Ctrl operation completed successfully.
*                           NET_DEV_ERR_NULL_PTR                Null argument pointer passed.
*
*                           NET_ERR_FAULT_NULL_PTR              Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_TIMEOUT_REG_RD          Phy register read  time-out.
*                           NET_PHY_ERR_TIMEOUT_REG_WR          Phy register write time-out.
*                           NET_PHY_ERR_INVALID_CFG             Duplex or Speed setting is invalid
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
                              NET_ERR     *p_err)
{
    NET_DEV_BSP_ETHER   *pdev_bsp;
    NET_DEV_LINK_ETHER  *plink_state;
    NET_DEV_CFG_ETHER   *pdev_cfg;
    NET_DEV             *pdev;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;

                                                                /* ----------- PERFORM SPECIFIED OPERATION ------------ */
    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;
             if (plink_state == (NET_DEV_LINK_ETHER *)0) {
                *p_err = NET_DEV_ERR_NULL_PTR;
                 return;
             }
             pphy_api = (NET_PHY_API_ETHER *)pif->Ext_API;
             if (pphy_api == (void *)0) {
                *p_err = NET_ERR_FAULT_NULL_FNCT;
                 return;
             }
             pphy_api->LinkStateGet(pif, plink_state, p_err);
             if (*p_err != NET_PHY_ERR_NONE) {
                 return;
             }
            *p_err = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;

             duplex = NET_PHY_DUPLEX_UNKNOWN;
             if (plink_state->Duplex != duplex) {
                 switch (plink_state->Duplex) {
                    case NET_PHY_DUPLEX_FULL:
                         pdev->NET_CFG |=  GEM_BIT_CFG_FULL_DUPLEX;
                         break;


                    case NET_PHY_DUPLEX_HALF:
                         pdev->NET_CFG &=  ~GEM_BIT_CFG_FULL_DUPLEX;
                         break;


                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {
                     case NET_PHY_SPD_10:
                          pdev->NET_CFG &= ~GEM_BIT_CFG_SPEED;
                          pdev->NET_CFG &= ~GEM_BIT_CFG_GIGE_EN;
                          break;


                     case NET_PHY_SPD_100:
                          pdev->NET_CFG |=  GEM_BIT_CFG_SPEED;
                          pdev->NET_CFG &= ~GEM_BIT_CFG_GIGE_EN;
                          break;


                     case NET_PHY_SPD_1000:
                          pdev->NET_CFG &= ~GEM_BIT_CFG_SPEED;
                          pdev->NET_CFG |=  GEM_BIT_CFG_GIGE_EN;
                          break;


                     default:
                          break;
                 }
             }
            *p_err = NET_DEV_ERR_NONE;
             break;


        default:
            *p_err = NET_IF_ERR_INVALID_IO_CTRL_OPT;
             break;
    }

    if (opt == NET_IF_IO_CTRL_LINK_STATE_UPDATE) {
        pdev_bsp->CfgClk(pif, p_err);
        if (*p_err != NET_DEV_ERR_NONE) {
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Rd()
*
* Description : Read data over the (R)MII bus from the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               p_err   Pointer to return error code.
*                           NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various Phy functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_MII_Rd (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U  *p_data,
                             NET_ERR     *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          timeout;
    CPU_INT32U          phy_word;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *p_err = NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */

    phy_word = GEM_BIT_PHYMGMT_OPERATION(2u)     |
               GEM_BIT_PHYMGMT_CLAUSE_22         |
               GEM_BIT_PHYMGMT_MUST10(2u)        |
               GEM_BIT_PHYMGMT_PHYADDR(phy_addr) |
               GEM_BIT_PHYMGMT_REGADDR(reg_addr) |
               GEM_BIT_PHYMGMT_DATA(0u);

    pdev->PHY_MAINT = phy_word;

    for (timeout = 0; timeout < 10000u; timeout++) {
        if (DEF_BIT_IS_SET(pdev->NET_STATUS, GEM_BIT_STATUS_PHY_MGMT_IDLE)) {
            break;
        }
    }

   *p_data = pdev->PHY_MAINT & GEM_BIT_PHYMGMT_DATA_MSK;

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               data        Data to write to the specified Phy register.
*
*               p_err   Pointer to return error code.
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various Phy functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_MII_Wr (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   data,
                             NET_ERR     *p_err)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          timeout;
    CPU_INT32U          phy_word;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */

    phy_word = GEM_BIT_PHYMGMT_OPERATION(1u)     |
               GEM_BIT_PHYMGMT_CLAUSE_22         |
               GEM_BIT_PHYMGMT_MUST10(2u)        |
               GEM_BIT_PHYMGMT_PHYADDR(phy_addr) |
               GEM_BIT_PHYMGMT_REGADDR(reg_addr) |
               GEM_BIT_PHYMGMT_DATA(data);

    pdev->PHY_MAINT = phy_word;

    for (timeout = 0; timeout < 10000u; timeout++) {
        if (DEF_BIT_IS_SET(pdev->NET_STATUS, GEM_BIT_STATUS_PHY_MGMT_IDLE)) {
            break;
        }
    }

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_TxEnqueue()
*
* Description : Add buffer to the device's software transmit queue.
*
* Argument(s) : pdev_data   Pointer to the IF device data.
*
*               p_data      Pointer to the raw buffer to be queued.
*
*               size        Size of the buffer to be queued.
*
*               p_err       Pointer to return error code.
*                               NET_DRV_ERR_NONE            Buffer added to queue.
*                               NET_DRV_ERR_Q_FULL          Queue is full, could not add buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function may be called if one or more of the following constraints are met:
*
*                   (a) Interrupts are disabled.
*
*                   (b) It is called from the ISR.
*
*                   (c) The Tx DMA engine is idle.
*********************************************************************************************************
*/

static  void  NetDev_TxEnqueue (NET_DEV_DATA  *pdev_data,
                                CPU_INT08U    *p_data,
                                CPU_INT16U     size,
                                NET_DRV_ERR   *p_err)
{
    DEV_BUF_Q  *q_back;


    if (pdev_data->TxQSize == pdev_data->TxDescNbr) {           /* Is there room in the queue for a new entry?          */
       *p_err = NET_DRV_ERR_Q_FULL;
        return;
    }

    q_back          = pdev_data->TxQ + pdev_data->TxQBack;      /* q_back points to the free entry.                     */
    q_back->DataPtr = p_data;                                   /* Queue up the new buffer.                             */
    q_back->Size    = size;
                                                                /* Increment the back index to the next free entry.     */
    pdev_data->TxQBack = (pdev_data->TxQBack + 1u) % pdev_data->TxDescNbr;
    pdev_data->TxQSize++;                                       /* Update the queue size.                               */

   *p_err = NET_DRV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_TxDequeue()
*
* Description : Retrieve buffer from the device's software transmit queue.
*
* Argument(s) : pdev_data   Pointer to the IF device data.
*
*               buf_q       Pointer to structure which holds address and size of the retrieved buffer.
*
*               p_err       Pointer to return error code.
*                               NET_DRV_ERR_NONE            Buffer retrieved from queue.
*                               NET_DRV_ERR_Q_EMPTY         Queue is empty, could not retrieve buffer.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function may be called if one or more of the following constraints are met:
*
*                   (a) Interrupts are disabled.
*
*                   (b) It is called from the ISR.
*
*                   (c) The Tx DMA engine is idle.
*********************************************************************************************************
*/

static  void  NetDev_TxDequeue (NET_DEV_DATA  *pdev_data,
                                DEV_BUF_Q     *buf_q,
                                NET_DRV_ERR   *p_err)
{
    DEV_BUF_Q  *q_front;


    if (pdev_data->TxQSize == 0u) {                             /* Is there a buffer in the queue?                      */
       *p_err = NET_DRV_ERR_Q_EMPTY;
        return;
    }

    q_front        = pdev_data->TxQ + pdev_data->TxQFront;      /* q_front points to the first entry in the queue.      */
    buf_q->DataPtr = q_front->DataPtr;                          /* Give buffer back to the caller.                      */
    buf_q->Size    = q_front->Size;
                                                                /* Increment the Front index to free up space.          */
    pdev_data->TxQFront = (pdev_data->TxQFront + 1u) % pdev_data->TxDescNbr;
    pdev_data->TxQSize--;                                       /* Update the queue size.                               */

   *p_err = NET_DRV_ERR_NONE;
}

#endif  /* NET_IF_ETHER_MODULE_EN */

