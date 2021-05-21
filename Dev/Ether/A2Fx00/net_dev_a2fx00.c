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
*                                      ACTEL SMART FUSION A2Fx00
*
* Filename : net_dev_a2fx00.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) The following parts use the 'net_dev_a2fx00' driver:
*                    Actel A2F200
*                    Actel A2F500
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_A2FX00
#include  <cpu_core.h>
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_a2fx00.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO                                10000  /* MII read write timeout.                              */
#define  A2FX00_VAL_REG_TO                              0xFFFF  /* Register timeout.                                    */
#define  A2FX00_VAL_CLK_TO                              0xFFFF  /* RMII clock generation timeout.                       */

#define  A2FX00_VAL_RX_DESC_ALIGN                CPU_WORD_SIZE_32
#define  A2FX00_VAL_SETUP_FRAME_SIZE                      192u
#define  A2FX00_VAL_SETUP_FRAME_ALIGN                     192u

#define  A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_CRC           255u
#define  A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_IX        (A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_CRC / 16u)
#define  A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_BIT       (A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_CRC % 16u)
#define  A2FX00_VAL_HASH_TBL_ADDR_UNICAST_IX             (156u / 4u)
#define  A2FX00_VAL_HASH_TBL_SIZE_BIT                     512u


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

                                                                /* ------------- BUS MODE REGISTER (CSR0) ------------- */
#define  A2FX00_BIT_CSR0_SPD                    DEF_BIT_21      /* Clock frequency selection                            */
#define  A2FX00_BIT_CSR0_DB0                    DEF_BIT_20      /* Descriptor byte order mode.                          */
                                                                /* Transmit automatic polling.                          */
#define  A2FX00_MSK_CSR0_TAP                    DEF_BIT_FIELD(3u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_DIS             DEF_BIT_NONE
#define  A2FX00_BIT_CSR0_TAP_10_825_6_uS        DEF_BIT_MASK(1u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_2_4768_mS       DEF_BIT_MASK(2u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_5_7792_mS       DEF_BIT_MASK(3u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_51_6_uS         DEF_BIT_MASK(4u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_103_2_uS        DEF_BIT_MASK(5u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_154_8_uS        DEF_BIT_MASK(6u, 17u)
#define  A2FX00_BIT_CSR0_TAP_10_412_8_uS        DEF_BIT_MASK(7u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_82_56_uS       DEF_BIT_MASK(1u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_247_68_mS      DEF_BIT_MASK(2u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_577_92_mS      DEF_BIT_MASK(3u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_5_16_uS        DEF_BIT_MASK(4u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_10_32_uS       DEF_BIT_MASK(5u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_15_48_uS       DEF_BIT_MASK(6u, 17u)
#define  A2FX00_BIT_CSR0_TAP_100_41_28_uS       DEF_BIT_MASK(7u, 17u)

                                                                /* Programable burst len.                               */
#define  A2FX00_MSK_CSR0_PBL                    DEF_BIT_MASK(6u, 8u)
#define  A2FX00_BIT_CSR0_BLE                    DEF_BIT_07      /* Big/Little endian.                                   */
                                                                /* Descriptor skip len.                                 */
#define  A2FX00_MSK_CSR0_DSL                    DEF_BIT_MASK(5u, 2u)

#define  A2FX00_BIT_CSR0_BAR                    DEF_BIT_01      /* Bus arbitration scheme.                              */
#define  A2FX00_BIT_CSR0_SWR                    DEF_BIT_00      /* Software reset.                                      */

                                                                /* -------- STATUS AND CONTROL REGISTER (CSR5) -------- */
                                                                /* Transmit process state.                              */
#define  A2FX00_MSK_CSR5_TS                     DEF_BIT_FIELD(3u, 20u)
#define  A2FX00_BIT_CSR5_TS_STOP                DEF_BIT_NONE
#define  A2FX00_BIT_CSR5_TS_RUN_FETCH           DEF_BIT_MASK(1u, 20u)
#define  A2FX00_BIT_CSR5_TS_RUN_WAIT            DEF_BIT_MASK(2u, 20u)
#define  A2FX00_BIT_CSR5_TS_RUN_XFER            DEF_BIT_MASK(3u, 20u)
#define  A2FX00_BIT_CSR5_TS_RUN_PKT             DEF_BIT_MASK(5u, 20u)
#define  A2FX00_BIT_CSR5_TS_SUSPENDED           DEF_BIT_MASK(6u, 20u)
#define  A2FX00_BIT_CSR5_TS_RUN_CLOSE           DEF_BIT_MASK(7u, 20u)

#define  A2FX00_MSK_CSR5_RS                     DEF_BIT_FIELD(3u, 17u)
#define  A2FX00_BIT_CSR5_RS_STOP                DEF_BIT_NONE
#define  A2FX00_BIT_CSR5_RS_RUN_FETCH           DEF_BIT_MASK(1u, 17u)
#define  A2FX00_BIT_CSR5_RS_RUN_WAIT_PKT_END    DEF_BIT_MASK(2u, 17u)
#define  A2FX00_BIT_CSR5_RS_RUN_PKT             DEF_BIT_MASK(3u, 17u)
#define  A2FX00_BIT_CSR5_RS_SUSPENDED           DEF_BIT_MASK(4u, 17u)
#define  A2FX00_BIT_CSR5_RS_RUN_XFER            DEF_BIT_MASK(7u, 17u)

#define  A2FX00_BIT_CSR5_NIS                    DEF_BIT_16      /* Normal Interrupt summary.                            */
#define  A2FX00_BIT_CSR5_AIS                    DEF_BIT_15      /* Abnormal interrupt summary.                          */
#define  A2FX00_BIT_CSR5_ERI                    DEF_BIT_14      /* Early receive interrupt.                             */
#define  A2FX00_BIT_CSR5_GTE                    DEF_BIT_11      /* General-purpose timer expiration.                    */
#define  A2FX00_BIT_CSR5_ETI                    DEF_BIT_10      /* Early transmit interrupt.                            */
#define  A2FX00_BIT_CSR5_RPS                    DEF_BIT_08      /* Received process stopped.                            */
#define  A2FX00_BIT_CSR5_RU                     DEF_BIT_07      /* Receive buffer unavailable.                          */
#define  A2FX00_BIT_CSR5_RI                     DEF_BIT_06      /* Receive interrupt.                                   */
#define  A2FX00_BIT_CSR5_UNF                    DEF_BIT_05      /* Transmit underflow.                                  */
#define  A2FX00_BIT_CSR5_TU                     DEF_BIT_02      /* Transmit buffer unavailable.                         */
#define  A2FX00_BIT_CSR5_TPS                    DEF_BIT_01      /* Transmit process stopped.                            */
#define  A2FX00_BIT_CSR5_TI                     DEF_BIT_00      /* Transmit process Interrupt.                          */
#define  A2Fx00_BIT_CSR5_STAT_ALL              (A2FX00_BIT_CSR5_NIS  | \
                                                A2FX00_BIT_CSR5_AIS  | \
                                                A2FX00_BIT_CSR5_AIS  | \
                                                A2FX00_BIT_CSR5_GTE  | \
                                                A2FX00_BIT_CSR5_ETI  | \
                                                A2FX00_BIT_CSR5_RPS  | \
                                                A2FX00_BIT_CSR5_RU   | \
                                                A2FX00_BIT_CSR5_RI   | \
                                                A2FX00_BIT_CSR5_UNF  | \
                                                A2FX00_BIT_CSR5_TU   | \
                                                A2FX00_BIT_CSR5_TPS  | \
                                                A2FX00_BIT_CSR5_TI)

                                                                /* --------- STATUS AND CONTROL REGISTER (CSR6) ------- */
#define  A2FX00_BIT_CSR6_TTM                    DEF_BIT_22      /* Transmit threshold mode.                             */
#define  A2FX00_BIT_CSR6_SF                     DEF_BIT_21      /* Store and forward.                                   */
                                                                /* Threshold control bits.                              */
#define  A2FX00_MSK_CSR6_TR                     DEF_BIT_FIELD(2u, 14u)
#define  A2FX00_BIT_CSR6_ST                     DEF_BIT_13      /* Start/Stop transmit command.                         */
#define  A2FX00_BIT_CSR6_FD                     DEF_BIT_09      /* Forcing full-duplex mode.                            */
#define  A2FX00_BIT_CSR6_PM                     DEF_BIT_07      /* Pass all multicast.                                  */
#define  A2FX00_BIT_CSR6_PR                     DEF_BIT_06      /* Pass all multicast.                                  */
#define  A2FX00_BIT_CSR6_IF                     DEF_BIT_04      /* Inverse filtering.                                   */
#define  A2FX00_BIT_CSR6_PB                     DEF_BIT_03      /* Pass bad frames.                                     */
#define  A2FX00_BIT_CSR6_HO                     DEF_BIT_02      /* Pass bad frames.                                     */
#define  A2FX00_BIT_CSR6_SR                     DEF_BIT_01      /* Start/Stop receive command.                          */
#define  A2FX00_BIT_CSR6_HP                     DEF_BIT_00      /* Hash/perfect receive filtering mode.                 */

                                                                /* ---------- INTERRUPT ENABLE REGISTER (CSR7) -------- */
#define  A2FX00_BIT_CSR7_NIE                    DEF_BIT_16      /* Normal Interrupt summary enable.                     */
#define  A2FX00_BIT_CSR7_AIE                    DEF_BIT_15      /* Abnormal interrupt summary enable.                   */
#define  A2FX00_BIT_CSR7_ERE                    DEF_BIT_14      /* Early receive interrupt enable.                      */
#define  A2FX00_BIT_CSR7_GTE                    DEF_BIT_11      /* General-purpose timer expiration enable.             */
#define  A2FX00_BIT_CSR7_ETE                    DEF_BIT_10      /* Early transmit interrupt enable.                     */
#define  A2FX00_BIT_CSR7_RSE                    DEF_BIT_08      /* Received process stopped enable.                     */
#define  A2FX00_BIT_CSR7_RUE                    DEF_BIT_07      /* Receive buffer unavailable enable.                   */
#define  A2FX00_BIT_CSR7_RIE                    DEF_BIT_06      /* Receive interrupt enable.                            */
#define  A2FX00_BIT_CSR7_UNE                    DEF_BIT_05      /* Underflow interrupt enable.                          */
#define  A2FX00_BIT_CSR7_TUE                    DEF_BIT_02      /* Transmit buffer unavailable enable.                  */
#define  A2FX00_BIT_CSR7_TSE                    DEF_BIT_01      /* Transmit process stopped enable.                     */
#define  A2FX00_BIT_CSR7_TIE                    DEF_BIT_00      /* Transmit process Interrupt enable.                   */
#define  A2Fx00_BIT_CSR7_INT_ALL               (A2FX00_BIT_CSR7_NIE |  \
                                                A2FX00_BIT_CSR7_AIE |  \
                                                A2FX00_BIT_CSR7_ERE |  \
                                                A2FX00_BIT_CSR7_GTE |  \
                                                A2FX00_BIT_CSR7_ETE |  \
                                                A2FX00_BIT_CSR7_RSE |  \
                                                A2FX00_BIT_CSR7_RUE |  \
                                                A2FX00_BIT_CSR7_RIE |  \
                                                A2FX00_BIT_CSR7_UNE |  \
                                                A2FX00_BIT_CSR7_TUE |  \
                                                A2FX00_BIT_CSR7_TSE |  \
                                                A2FX00_BIT_CSR7_TIE)

                                                                /* - MISSED FRAMES & OVERFLOW COUNTER REGISTER (CSR8) - */
#define  A2FX00_BIT_CSR8_OCO                    DEF_BIT_28      /* Overflow counter overflow.                           */
                                                                /* FIFO overflow counter.                               */
#define  A2FX00_MSK_CSR6_FPC                    DEF_BIT_FIELD(11u, 17u)
#define  A2FX00_BIT_CSR6_MFO                    DEF_BIT_16      /* Missed frame overflow                                */
                                                                /* Missed frame counter.                                */
#define  A2FX00_MSK_CSR6_MFC                    DEF_BIT_FIELD(16u, 0u)

                                                                /* ----- RMII Management interface register (CSR9) ---- */
#define  A2FX00_BIT_CSR9_MDI                    DEF_BIT_19      /* RMII managment data in signal.                       */
#define  A2FX00_BIT_CSR9_MDEN                   DEF_BIT_18      /* RMII managment data operation mode.                  */
#define  A2FX00_BIT_CSR9_MDO                    DEF_BIT_17      /* RMII managment write data.                           */
#define  A2FX00_BIT_CSR9_MDC                    DEF_BIT_16      /* RMII managment clock.                                */


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/
                                                                /* ---------- RECEIVE DESCRIPTORS BIT DEFINES --------- */
#define  A2FX00_BIT_RDES0_OWN                   DEF_BIT_31      /* Ownership bit.                                       */
#define  A2FX00_BIT_RDES0_FF                    DEF_BIT_30      /* Filtering fail.                                      */
                                                                /* Frame len.                                           */
#define  A2FX00_MSK_RDES0_FL                    DEF_BIT_FIELD(14u, 16u)
#define  A2FX00_BIT_RDES0_ES                    DEF_BIT_15      /* Error summary.                                       */
#define  A2FX00_BIT_RDES0_DE                    DEF_BIT_14      /* Descriptor error.                                    */
#define  A2FX00_BIT_RDES0_RF                    DEF_BIT_11      /* Runtframe.                                           */
#define  A2FX00_BIT_RDES0_MF                    DEF_BIT_10      /* Multicast frame.                                     */
#define  A2FX00_BIT_RDES0_FS                    DEF_BIT_09      /* First descriptor.                                    */
#define  A2FX00_BIT_RDES0_LS                    DEF_BIT_08      /* Last descriptor.                                     */
#define  A2FX00_BIT_RDES0_TL                    DEF_BIT_07      /* Frame too long.                                      */
#define  A2FX00_BIT_RDES0_CS                    DEF_BIT_06      /* Collision seen.                                      */
#define  A2FX00_BIT_RDES0_FT                    DEF_BIT_05      /* Frame type.                                          */
#define  A2FX00_BIT_RDES0_RE                    DEF_BIT_03      /* Report on RMII.                                      */
#define  A2FX00_BIT_RDES0_DB                    DEF_BIT_02      /* Dribbling bit.                                       */
#define  A2FX00_BIT_RDES0_CE                    DEF_BIT_01      /* CRC error.                                           */
#define  A2FX00_BIT_RDES0_ZERO                  DEF_BIT_00

#define  A2FX00_BIT_RDES1_RER                   DEF_BIT_25      /* Receive end of ring.                                 */
#define  A2FX00_BIT_RDES1_RCH                   DEF_BIT_24      /* Second address chained.                              */
                                                                /* Buffer 2 size.                                       */
#define  A2FX00_MSK_RDES1_RBS2                  DEF_BIT_FIELD(11u, 11u)
                                                                /* Buffer 2 size.                                       */
#define  A2FX00_MSK_RDES1_RBS1                  DEF_BIT_FIELD(11u,  0u)

                                                                /* ---------- TRANSMIT DESCRIPTORS BIT DEFINES -------- */
#define  A2FX00_BIT_TDES0_OWN                   DEF_BIT_31      /* Ownership bit.                                       */
#define  A2FX00_BIT_TDES0_ES                    DEF_BIT_15      /* Error summary.                                       */
#define  A2FX00_BIT_TDES0_LO                    DEF_BIT_11      /* Lost of carrier.                                     */
#define  A2FX00_BIT_TDES0_NC                    DEF_BIT_10      /* No carrier.                                          */
#define  A2FX00_BIT_TDES0_LC                    DEF_BIT_09      /* Late Collision.                                      */
#define  A2FX00_BIT_TDES0_EC                    DEF_BIT_08      /* Excessive collisions.                                */
                                                                /* Collision count.                                     */
#define  A2FX00_MSK_TDES0_CC                    DEF_BIT_FIELD(4u, 3u)

#define  A2FX00_BIT_TDES0_UF                    DEF_BIT_01      /* Underflow error.                                     */
#define  A2FX00_BIT_TDES0_DE                    DEF_BIT_00      /* Deferred.                                            */

#define  A2FX00_BIT_TDES1_IC                    DEF_BIT_31      /* Interrrupt on completion.                            */
#define  A2FX00_BIT_TDES1_LS                    DEF_BIT_30      /* Last descriptor.                                     */
#define  A2FX00_BIT_TDES1_FS                    DEF_BIT_29      /* First Descriptor.                                    */
#define  A2FX00_BIT_TDES1_FT1                   DEF_BIT_28      /* Filtering Type.                                      */
#define  A2FX00_BIT_TDES1_SET                   DEF_BIT_27      /* Setup packet.                                        */
#define  A2FX00_BIT_TDES1_AC                    DEF_BIT_26      /* Add CRC disable.                                     */
#define  A2FX00_BIT_TDES1_TER                   DEF_BIT_25      /* Transmit end of ring.                                */
#define  A2FX00_BIT_TDES1_TCH                   DEF_BIT_24      /* Second address chained.                              */
#define  A2FX00_BIT_TDES1_DPD                   DEF_BIT_23      /* Disabled padding.                                    */
#define  A2FX00_BIT_TDES1_FT0                   DEF_BIT_22      /* Filtering type.                                      */
                                                                /* Buffer 2 size.                                       */
#define  A2FX00_MSK_TDES1_TBS2                  DEF_BIT_FIELD(11u, 11u)
                                                                /* Buffer 1 size.                                       */
#define  A2FX00_MSK_TDES1_TBS1                  DEF_BIT_FIELD(11u, 0u)


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
#define  A2FX00_VAL_MDIO_MIN_FREQ              ((25u * DEF_TIME_NBR_uS_PER_SEC) / 10u)

#define  A2FX00_BIT_MDIO_START                  DEF_BIT_30
#define  A2FX00_BIT_MDIO_OP_RD                  DEF_BIT_29
#define  A2FX00_BIT_MDIO_OP_WR                  DEF_BIT_28
#define  A2FX00_BIT_MDIO_OP_TA_WR               DEF_BIT_17
#define  A2FX00_MSK_MDIO_PHY_ADDR               DEF_BIT_FIELD( 5u, 0u)
#define  A2FX00_MSK_MDIO_REG_ADDR               DEF_BIT_FIELD( 5u, 0u)
#define  A2FX00_MSK_MDIO_DATA                   DEF_BIT_FIELD(16u, 0u)


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
*********************************************************************************************************
*/

#if     (CPU_CFG_TS_64_EN == DEF_ENABLED)
typedef  CPU_TS64   NET_DEV_TS;
#elif   (CPU_CFG_TS_32_EN == DEF_ENABLED)
typedef  CPU_TS32   NET_DEV_TS;
#endif

                                                                /* ------------- DMA DESCRIPTOR DATA TYPE ------------- */
typedef  struct  dev_desc {                                     /* See Note #2.                                         */
    CPU_REG32      DES0;                                        /* Ownership & status word.                             */
    CPU_REG32      DES1;                                        /* Control word.                                        */
    CPU_REG32      DES2;                                        /* Next descriptor word.                                */
    CPU_REG32      DES3;                                        /* Descriptor data pointer.                             */
} DEV_DESC;
                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR        ErrRxDesc;
    NET_CTR        ErrRxRuntFrameCtr;
    NET_CTR        ErrRxCollisionCtr;
    NET_CTR        ErrRxRMII_Ctr;
    NET_CTR        ErrRxDataAreaAllocCtr;
    NET_CTR        ErrRxOwn;
    NET_CTR        ErrRxCRC;
#endif
    MEM_POOL       RxDescPool;
    MEM_POOL       TxDescPool;
    DEV_DESC      *RxDescPtrStart;
    DEV_DESC      *RxDescPtrCur;
    DEV_DESC      *TxDescPtrStart;
    DEV_DESC      *TxDescPtrCur;
    DEV_DESC      *TxDescPtrComp;                               /* See Note #3.                                         */
    CPU_INT32U    *SetupFramePtr;                               /* Setup frame buffer.                                  */
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U     MulticastAddrHashBitCtr[A2FX00_VAL_HASH_TBL_SIZE_BIT];
#endif
    CPU_BOOLEAN    PhyAccessLock;
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
*
*               An example device register structure is provided below.
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  CSR0;                                            /* Bus mode.                                            */
    CPU_REG32  RSVD0;
    CPU_REG32  CSR1;                                            /* Transmit poll demand.                                */
    CPU_REG32  RSVD1;
    CPU_REG32  CSR2;                                            /* Receive poll demand.                                 */
    CPU_REG32  RSVD2;
    CPU_REG32  CSR3;                                            /* Receive list base address.                           */
    CPU_REG32  RSVD3;
    CPU_REG32  CSR4;                                            /* Transmit list base address.                          */
    CPU_REG32  RSVD4;
    CPU_REG32  CSR5;                                            /* Status and control.                                  */
    CPU_REG32  RSVD5;
    CPU_REG32  CSR6;                                            /* Operation mode.                                      */
    CPU_REG32  RSVD6;
    CPU_REG32  CSR7;                                            /* Interrupt enable.                                    */
    CPU_REG32  RSVD7;
    CPU_REG32  CSR8;                                            /* Missed frames and overflow counters.                 */
    CPU_REG32  RSVD8;
    CPU_REG32  CSR9;                                            /* RMII management.                                     */
    CPU_REG32  RSVD9;
    CPU_REG32  CSF10;                                           /* Reserved.                                            */
    CPU_REG32  RSVD10;
    CPU_REG32  CSR11;                                           /* Timer and interrupt mitigation control.              */
} NET_DEV;


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) Global variables are highly discouraged and should only be used for storing NON-instance
*               specific data and the array of instance specific data.  Global variables, those that are
*               not declared within the NET_DEV_DATA area, are not multiple-instance safe and could lead
*               to incorrect driver operation if used to store device state information.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                              MACRO'S
*********************************************************************************************************
*/

#if   (CPU_CFG_TS_64_EN == DEF_ENABLED)
#define  NET_DEV_TS_GET()              CPU_TS_Get64();

#elif (CPU_CFG_TS_32_EN == DEF_ENABLED)
#define  NET_DEV_TS_GET()              CPU_TS_Get32();
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
static  void         NetDev_Init               (NET_IF             *pif,
                                                NET_ERR            *perr);


static  void         NetDev_Start              (NET_IF             *pif,
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


static  void         NetDev_MII_Rd             (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *perr);

static  void         NetDev_MII_Wr             (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);


                                                                        /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
static  void         NetDev_RxDescInit         (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_RxDescFreeAll      (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_RxDescPtrCurInc    (NET_IF             *pif);


static  void         NetDev_TxDescInit         (NET_IF             *pif,
                                                NET_ERR            *perr);

static  CPU_BOOLEAN  NetDev_MII_Clk            (NET_DEV            *pdev);



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
                                                                                    /* A2FX00 dev API fnct ptrs :       */
const  NET_DEV_API_ETHER  NetDev_API_A2FX00 = { NetDev_Init,                        /*   Init/add                       */
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
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
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
*                   NET_DEV_ERR_INVALID_CFG if unacceptible values have been specified. Fields
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
*                   descriptors MUST occur as a single contigous block of memory.  The driver may
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

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    NET_BUF_SIZE        buf_size_max;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbr_bytes;
    CPU_SIZE_T          reg_to;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;


                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf alignment (see Note #7).             */
    if (pdev_cfg->RxBufAlignOctets != CPU_WORD_SIZE_32) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate phy bus mode.                               */
    if (pphy_cfg->BusMode != NET_PHY_BUS_MODE_RMII) {           /* ... Only RMII interface is supported.                */
       *perr = NET_PHY_ERR_INVALID_CFG;
         return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf size.                                */
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )pif->Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )NET_IF_IX_RX);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }

                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 1u,
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;
    Mem_Clr((void        *)pdev_data,
            (CPU_SIZE_T   )sizeof(NET_DEV_DATA));

                                                                /* ---------- ALLOCATE FILTER TABLE DATA -------------- */
    pdev_data->SetupFramePtr = (CPU_INT32U *)Mem_HeapAlloc((CPU_SIZE_T  ) A2FX00_VAL_SETUP_FRAME_SIZE,
                                                           (CPU_SIZE_T  ) A2FX00_VAL_SETUP_FRAME_ALIGN,
                                                           (CPU_SIZE_T *)&reqd_octets,
                                                           (LIB_ERR    *)&lib_err);
    if (pdev_data->SetupFramePtr == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    Mem_Clr((void       *)pdev_data->SetupFramePtr,
            (CPU_SIZE_T  )A2FX00_VAL_SETUP_FRAME_SIZE);


    pdev_data->SetupFramePtr[A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_IX]            = DEF_BIT(A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_BIT);
#ifdef NET_MCAST_MODULE_EN
    pdev_data->MulticastAddrHashBitCtr[A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_BIT] = 1u;
#endif

                                                                /* ------------- ENABLE NECESSARY CLOCKS -------------- */
    pdev_bsp->CfgClk(pif, perr);                                /* Enable module clks (see Note #2).                    */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* -------- INITIALIZE EXTERNAL GPIO CONTROLLER ------- */
    pdev_bsp->CfgGPIO(pif, perr);                               /* Configure Ethernet Controller GPIO (see Note #4).    */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbr_bytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);         /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,                            /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) 1u,                           /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_bytes,                    /* Block size large enough to hold all Rx descriptors.  */
                   (CPU_SIZE_T  ) 4u,                           /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


    nbr_bytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);         /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0u,                           /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) 1u,                           /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_bytes,                    /* Block size large enough to hold all Tx descriptors.  */
                   (CPU_SIZE_T  ) 1u,                           /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */
                                                                /* See Note #9.                                         */
    DEF_BIT_SET(pdev->CSR0, A2FX00_BIT_CSR0_SWR);               /* Perform a software reset.                            */
    reg_to = A2FX00_VAL_REG_TO;
    while ((DEF_BIT_IS_CLR(pdev->CSR0, A2FX00_BIT_CSR0_SWR) == DEF_NO) &&
          (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
       *perr = NET_DEV_ERR_INIT;
        return;
    }

    DEF_BIT_CLR(pdev->CSR0, A2FX00_BIT_CSR0_SPD);
    DEF_BIT_CLR(pdev->CSR0, A2FX00_BIT_CSR0_DB0);
    DEF_BIT_CLR(pdev->CSR0, A2FX00_BIT_CSR0_BLE);
    DEF_BIT_CLR(pdev->CSR0, A2FX00_BIT_CSR0_BAR);
    DEF_BIT_CLR(pdev->CSR0, A2FX00_MSK_CSR0_TAP);
    DEF_BIT_SET(pdev->CSR0, A2FX00_BIT_CSR0_TAP_100_5_16_uS);

    DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SF);                /* Enable store & forward.                              */

    pdev->CSR11 = DEF_BIT_NONE;

    DEF_BIT_CLR(pdev->CSR7, A2Fx00_BIT_CSR7_INT_ALL);           /* Disable all interrupts.                              */
    pdev->CSR5 = A2Fx00_BIT_CSR5_STAT_ALL;                      /* Clear all status flags.                              */
    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST |                /* Stop Tx/Rx.                                          */
                            A2FX00_BIT_CSR6_SR);

    if (pphy_cfg->Duplex == NET_PHY_DUPLEX_HALF) {
        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_FD);            /* Force Half-duplex mode.                              */
    } else {
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_FD);            /* Force Full duplex as a default.                      */
    }

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
    pdev_bsp->CfgIntCtrl(pif, perr);                            /* Initialize ext int ctrl'r (see Note #3).             */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


   *perr = NET_DEV_ERR_NONE;
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
*                                                               ---- RETURNED BY NetDev_TxDescInit() : -----
*                          !!!!
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
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
*                                                                configure the HW address using using
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
*              (6) The setup frames define the address that are used for the receive address
*                  filtering process. These frames are never transmitted on the Ethernet Connection.
*                  They are used to fill the address filtering RAM.
*
*              (7) The device implementens the following filtering types.
*                       (a) Perfect filtering   mode of  16 unicast addresses.
*                       (b) Hash    filtering   mode of 512 multicast addresses.
*                       (d) Inverse filtering   mode of  16 unicast addresses.
*                       (3) Hash-only filtering mode of 512 multicast addresses plus ONE unicast address.
*                       (c) Promiscous non filtering.  Disable filtering of all received frames.
*
*               (2) This function implements the filtering mechanism described in 7b.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT32U         *psetup;
    DEV_DESC            setup_desc;
    CPU_SIZE_T          reg_to;
    CPU_INT32U          reg_val;
    CPU_INT08U          tbl_ix;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                         */
                            pdev_cfg->TxDescNbr,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }

    tbl_ix                   = A2FX00_VAL_HASH_TBL_ADDR_UNICAST_IX;
    pdev_data->PhyAccessLock = DEF_FALSE;
    psetup                   = pdev_data->SetupFramePtr;


                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #4 & #5.                                   */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #5a).                                  */
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
                                                                /* ... if any (see Note #5b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any (see Note #5c).           */
            hw_addr[0] = (psetup[tbl_ix     ]      ) & DEF_INT_08_MASK;
            hw_addr[1] = (psetup[tbl_ix     ] >> 8u) & DEF_INT_08_MASK;
            hw_addr[2] = (psetup[tbl_ix + 1u]      ) & DEF_INT_08_MASK;
            hw_addr[3] = (psetup[tbl_ix + 1u] >> 8u) & DEF_INT_08_MASK;
            hw_addr[4] = (psetup[tbl_ix + 2u]      ) & DEF_INT_08_MASK;
            hw_addr[5] = (psetup[tbl_ix + 2u] >> 8u) & DEF_INT_08_MASK;

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
                                                                /* Set physical address 0 in the setup frame.           */
        psetup[tbl_ix     ] = (CPU_INT32U)hw_addr[0] | ((CPU_INT32U)hw_addr[1] << 8u);
        psetup[tbl_ix + 1u] = (CPU_INT32U)hw_addr[2] | ((CPU_INT32U)hw_addr[3] << 8u);
        psetup[tbl_ix + 2u] = (CPU_INT32U)hw_addr[4] | ((CPU_INT32U)hw_addr[5] << 8u);
    }



    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);                /* Stop Tx.                                             */

    reg_to  = A2FX00_VAL_REG_TO;                                /* Wait utnil Tx is in stop state.                      */
    reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
    while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
           (reg_to   > 0u)) {
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr = NET_DEV_ERR_FAULT;
         return;
    }

    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);                /* Stop Rx.                                             */
    reg_to  = A2FX00_VAL_REG_TO;
    reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;                  /* Wait Until Rx is in stop or suspended state.         */
    while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
           (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
           (reg_to   > 0u)) {
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr = NET_DEV_ERR_FAULT;
        return;
    }

                                                                /* -------- CONFIGURE THE ADDRESS FILTERING RAM ------- */
    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_PM |                /* Set the filtering mode. One physical address and ... */
                            A2FX00_BIT_CSR6_PR);                /* ... 512-bit has table for multicast address.         */

    setup_desc.DES0 = A2FX00_BIT_TDES0_OWN;                     /* ... Ownership descriptor.                            */

    setup_desc.DES1 = A2FX00_BIT_TDES1_SET                      /* ... Setup packet.                                    */
                    | A2FX00_BIT_TDES1_TER
                    | A2FX00_BIT_TDES1_FT0                      /* ... Hash filtering mode.                             */
                    | ((A2FX00_VAL_SETUP_FRAME_SIZE) & A2FX00_MSK_TDES1_TBS1);

    setup_desc.DES2 = (CPU_INT32U )psetup;                      /* ... Set setup frame buffer address.                  */
    setup_desc.DES3 = (CPU_INT32U )0;
    pdev->CSR4      = (CPU_INT32U )&setup_desc;                 /* ... Set Tx descriptor list base address reister.     */
    DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);                /* ... Start the transmission.                          */

    reg_to          = A2FX00_VAL_REG_TO;

    reg_val         = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                                                /* Wait until the setup frame is sent.                  */
    while ((reg_val != A2FX00_BIT_CSR5_TS_SUSPENDED) &&
           (reg_to   > 0u)) {
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* Check that the current filter mode is set correctly. */
    reg_val  = pdev->CSR6;
    reg_val &= A2FX00_BIT_CSR6_PM
             | A2FX00_BIT_CSR6_PR
             | A2FX00_BIT_CSR6_IF
             | A2FX00_BIT_CSR6_HO
             | A2FX00_BIT_CSR6_HP;

    if (reg_val != A2FX00_BIT_CSR6_HP) {
        *perr = NET_DEV_ERR_INIT;
        return;
    }


    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);                /* Stop the transmission.                               */
    pdev->CSR5 = A2FX00_BIT_CSR5_TU                             /* Clear the transmit buffer unavailable bit.           */
               | A2FX00_BIT_CSR5_TPS;                           /* Clear the transmit buffer tranmit process stopped.   */

                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* -------------------- CFG INT'S --------------------- */
                                                                /* Enable interrupts:                                   */
    DEF_BIT_SET(pdev->CSR7, A2FX00_BIT_CSR7_NIE |               /* ... Normal Interrupt summary enable.                 */
                            A2FX00_BIT_CSR7_RIE |               /* ... Early receive interrupt enable.                  */
                            A2FX00_BIT_CSR7_TIE);               /* ... Early transmit interrupt enable.                 */
                                                                /* ------------------ ENABLE RX & TX ------------------ */
                                                                /* Start Rx.                                            */
    DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR | A2FX00_BIT_CSR6_ST);

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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT08U          desc_ix;
    NET_ERR             err;
    LIB_ERR             lib_err;
    CPU_INT32U          reg_val;
    CPU_SIZE_T          reg_to;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);                /* Stop Tx.                                             */

    reg_to  = A2FX00_VAL_REG_TO;                                /* Wait utnil Tx is in stop state.                      */
    reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
    while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
           (reg_to   > 0u)) {
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr = NET_DEV_ERR_FAULT;
         return;
    }

    DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);                /* Stop Rx.                                             */
    reg_to  = A2FX00_VAL_REG_TO;
    reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;                  /* Wait Until Rx is in stop or suspended state.         */
    while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
           (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
           (reg_to   > 0u)) {
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    DEF_BIT_CLR(pdev->CSR7, A2FX00_BIT_CSR7_NIE |               /* Disable interrupts.                                  */
                            A2FX00_BIT_CSR7_ERE |
                            A2FX00_BIT_CSR7_ETE);

    pdev->CSR5 = A2FX00_BIT_CSR5_NIS                            /* Clear all pending interrupts.                        */
               | A2FX00_BIT_CSR5_RI
               | A2FX00_BIT_CSR5_TI;

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    pdesc = pdev_data->TxDescPtrStart;
    for (desc_ix = 0u; desc_ix < pdev_cfg->TxDescNbr; desc_ix++) {
        if (pdesc->DES2 != 0u) {                                /* If NOT yet  tx'd, ...                                */
                                                                /* ... dealloc tx buf (see Note #2a1).                  */
            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->DES2, &err);
           (void)&err;                                          /* Ignore possible dealloc err (see Note #2b2).         */
        }
        pdesc = (DEV_DESC *)pdesc->DES3;
    }

                                                                /* ---------------- FREE TX DESC BLOCK ---------------- */
    Mem_PoolBlkFree(&pdev_data->TxDescPool,                     /* Return Tx descriptor block to Tx descriptor pool.    */
                     pdev_data->TxDescPtrStart,
                    &lib_err);

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
* Note(s)     : (2) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (3) If a receive error occurs and the descriptor is invalid then the function
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
*              (4) The collision seen seenf flag 'CS' (bit 6) does NOT seem to cause a critical error.
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
    CPU_INT08U         *pbuf_new;
    CPU_INT16U          len;
    CPU_INT32U          reg_val;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdesc     = (DEV_DESC          *)pdev_data->RxDescPtrCur;   /* Obtain ptr to next ready descriptor.                 */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

   *size      = (CPU_INT16U  )0u;
   *p_data    = (CPU_INT08U *)0;

                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
    reg_val   =  pdesc->DES0;                                   /* Get the status word.                                 */

    if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_OWN)) {
        DEF_BIT_SET(pdev->CSR7, A2FX00_BIT_CSR7_RIE);
        pdev->CSR2 = DEF_BIT_01;
        NET_CTR_ERR_INC(pdev_data->ErrRxOwn);
        *perr = (NET_ERR )NET_DEV_ERR_RX;
        return;
    }

    DEF_BIT_CLR(reg_val, A2FX00_BIT_RDES0_TL);                  /* Ignore the frame len error.                          */


    if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_ES) &&
        DEF_BIT_IS_CLR(reg_val, A2FX00_BIT_RDES0_CS)) {         /* Check for errrors. (see note #4)                     */
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)                        /* Increase all the errors counters.                    */
         if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_DE)) {    /* Descriptor error                                     */
             NET_CTR_ERR_INC(pdev_data->ErrRxDesc);
         }

         if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_RF)) {    /* Runt frame error.                                    */
             NET_CTR_ERR_INC(pdev_data->ErrRxRuntFrameCtr);
         }

#if 0
         if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_CS)) {    /* Collision seen error.                                */
             NET_CTR_ERR_INC(pdev_data->ErrRxCollisionCtr);
         }
#endif

         if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_RE)) {    /* RMII errror.                                          */
             NET_CTR_ERR_INC(pdev_data->ErrRxRMII_Ctr);
         }

         if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_RDES0_CE)) {    /* CRC errror.                                          */
             NET_CTR_ERR_INC(pdev_data->ErrRxCRC);
         }
#endif
         NetDev_RxDescPtrCurInc(pif);
         *perr = (NET_ERR )NET_DEV_ERR_RX;
          return;
    }

    len = ((reg_val & A2FX00_MSK_RDES0_FL) >> 16u);
    if (len < NET_IF_ETHER_FRAME_CRC_SIZE) {
        NetDev_RxDescPtrCurInc(pif);
       *perr = (NET_ERR )NET_DEV_ERR_INVALID_SIZE;
    }

    len -= NET_IF_ETHER_FRAME_CRC_SIZE;
    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {
        NetDev_RxDescPtrCurInc(pif);
       *perr = (NET_ERR )NET_DEV_ERR_INVALID_SIZE;
        return;
    }
                                                                /* --------- OBTAIN PTR TO NEW DMA DATA AREA ---------- */
                                                                /* Request an empty buffer.                             */
    pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_TYPE  *)0,
                                 (NET_ERR       *)perr);

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer (see Note #3c).            */
         NET_CTR_ERR_INC(pdev_data->ErrRxDataAreaAllocCtr);
         NetDev_RxDescPtrCurInc(pif);                           /* Free the current descriptor.                         */
         return;
    }

   *size        = (CPU_INT16U  )len;                            /* Return the size of the received frame.               */
   *p_data      = (CPU_INT08U *)pdesc->DES2;                    /* Return a pointer to the newly received data area.    */
    pdesc->DES2 = (CPU_INT32U  )pbuf_new;                       /* Set the receive buffer address in the descriptor.    */
    NetDev_RxDescPtrCurInc(pif);


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
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdesc     = (DEV_DESC          *)pdev_data->TxDescPtrCur;   /* Obtain ptr to next available Tx descriptor.          */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

    reg_val   =  pdesc->DES0;

    if (DEF_BIT_IS_CLR(reg_val, A2FX00_BIT_TDES0_OWN)) {
        pdesc->DES2 = (CPU_INT32U)p_data;
        DEF_BIT_CLR(pdesc->DES1, A2FX00_MSK_TDES1_TBS1);
        DEF_BIT_SET(pdesc->DES1, size & A2FX00_MSK_TDES1_TBS1);

        pdev_data->TxDescPtrCur = (DEV_DESC *)pdesc->DES3;
        pdesc->DES0             = (CPU_INT32U)A2FX00_BIT_TDES0_OWN;
       *perr                    = (NET_ERR   )NET_DEV_ERR_NONE;
        pdev->CSR1              = DEF_BIT_01;
    } else {
       *perr                    = (NET_ERR   )NET_DEV_ERR_TX_BUSY;
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
* Note(s)     : none.
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
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT32U         *psetup;
    DEV_DESC           *pdesc;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          tbl_ix;
    CPU_INT16U          hash;
    CPU_INT32U          reg_val;
    CPU_SIZE_T          reg_to;
    CPU_SR_ALLOC();

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without complement.                     */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    psetup          = pdev_data->SetupFramePtr;
    hash            = crc & DEF_BIT_FIELD(9u, 0u);
    tbl_ix          = hash / 16u;
    bit_sel         = hash % 16u;
    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    (*paddr_hash_ctrs)++;

    if ((*paddr_hash_ctrs) == 1u) {

        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;              /* Wait until all Tx buffer are sent (suspended state)  */
        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        pdev->CSR1 = DEF_BIT_01;
        while ((reg_val != A2FX00_BIT_CSR5_TS_SUSPENDED) &&
               (reg_to   > 0u)) {
            reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
            pdev->CSR1 = DEF_BIT_01;
            reg_to--;
        }

        if (reg_to == 0u) {
            *perr = NET_DEV_ERR_FAULT;
             return;
        }
        CPU_CRITICAL_ENTER();
        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);            /* Stop Tx.                                             */
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
               (reg_to   > 0u)) {
            reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
            reg_to--;
        }

        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);            /* Stop Rx.                                             */
        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;              /* Wait Until Rx is in stop or suspended state.         */
        while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
               (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
               (reg_to   > 0u)) {
            reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
            pdev->CSR2 = DEF_BIT_01;
            reg_to--;
        }

        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
            *perr = NET_DEV_ERR_FAULT;
            return;
        }

        pdesc   = pdev_data->TxDescPtrCur;
        reg_val = pdesc->DES0;

        if (DEF_BIT_IS_CLR(reg_val, A2FX00_BIT_TDES0_OWN)) {
            pdesc->DES0              = A2FX00_BIT_TDES0_OWN;
            pdesc->DES1              = A2FX00_BIT_TDES1_SET
                                     | A2FX00_BIT_TDES1_TER
                                     | A2FX00_BIT_TDES1_FT0
                                     | ((A2FX00_VAL_SETUP_FRAME_SIZE) & A2FX00_MSK_TDES1_TBS1);
            pdesc->DES2              = (CPU_INT32U)psetup;
        } else {
            CPU_CRITICAL_EXIT();
           *perr = NET_DEV_ERR_FAULT;
            return;
        }

        DEF_BIT_SET(psetup[tbl_ix], DEF_BIT(bit_sel));
                                                                /* -------- CONFIGURE THE ADDRESS FILTERING RAM ------- */
        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_PM |            /* Set the filtering mode. One physical address and ... */
                                A2FX00_BIT_CSR6_PR);            /* ... 512-bit has table for multicast address.         */
        DEF_BIT_CLR(pdev->CSR7, A2FX00_BIT_CSR7_TIE);           /* ... Early transmit interrupt disable.                */
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);            /* ... Start the transmission.                          */

        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                                                /* Wait until the setup frame is sent.                  */
        while ((reg_val != A2FX00_BIT_CSR5_TS_SUSPENDED)        &&
               (DEF_BIT_IS_CLR(pdev->CSR5, A2FX00_BIT_CSR5_TI)) &&
               (reg_to   > 0u)) {
            reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
            reg_to--;
        }

        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
            *perr = NET_DEV_ERR_FAULT;
            return;
        }

        pdesc->DES0 = DEF_BIT_NONE;
        pdesc->DES1 = A2FX00_BIT_TDES1_IC                       /* Interrupt on completion.                             */
                    | A2FX00_BIT_TDES1_TCH                      /* Second Adress chained.                               */
                    | A2FX00_BIT_TDES1_LS                       /* Last  descriptor of the frame.                       */
                    | A2FX00_BIT_TDES1_FS;                      /* First descriptor of the frame.                       */
        pdesc->DES2 = 0u;                                       /* Set the descriptor data buffer to 'NULL'             */
        pdev->CSR5  = A2FX00_BIT_CSR5_TI;                       /* Clear the interrupt.                                 */

        pdev_data->TxDescPtrCur  = pdev_data->TxDescPtrStart;   /* Re-start the descriptor list.                        */
        pdev_data->TxDescPtrComp = pdev_data->TxDescPtrStart;

        DEF_BIT_SET(pdev->CSR7, A2FX00_BIT_CSR7_TIE);           /* Early transmit interrupt enable.                     */
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR);
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);
        CPU_CRITICAL_EXIT();
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
* Note(s)     : none.
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
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT32U         *psetup;
    DEV_DESC           *pdesc;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          tbl_ix;
    CPU_INT16U          hash;
    CPU_INT32U          reg_val;
    CPU_SIZE_T          reg_to;
    CPU_SR_ALLOC();

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without complement.                     */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    psetup          = pdev_data->SetupFramePtr;
    hash            = crc & DEF_BIT_FIELD(9u, 0u);
    tbl_ix          = hash / 16u;
    bit_sel         = hash % 16u;
    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];

    if (((*paddr_hash_ctrs) == 1u                                    ) &&
        (  hash             == A2FX00_VAL_HASH_TBL_ADDR_BROADCAST_CRC)) {
        *perr = NET_DEV_ERR_NONE;
         return;
    }

    if ((*paddr_hash_ctrs) > 0u) {
        (*paddr_hash_ctrs)--;
    }

    if ((*paddr_hash_ctrs) == 0u) {
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;              /* Wait until all Tx buffer are sent (suspended state)  */
        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        pdev->CSR1 = DEF_BIT_01;
        while ((reg_val != A2FX00_BIT_CSR5_TS_SUSPENDED) &&
               (reg_to   > 0u)) {
            reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
            pdev->CSR1 = DEF_BIT_01;
            reg_to--;
        }

        if (reg_to == 0u) {
            *perr = NET_DEV_ERR_FAULT;
             return;
        }
        CPU_CRITICAL_ENTER();
        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);            /* Stop Tx.                                             */
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
        while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
               (reg_to   > 0u)) {
            reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
            reg_to--;
        }

        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);            /* Stop Rx.                                             */
        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;              /* Wait Until Rx is in stop or suspended state.         */
        while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
               (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
               (reg_to   > 0u)) {
            reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
            pdev->CSR2 = DEF_BIT_01;
            reg_to--;
        }

        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
            *perr = NET_DEV_ERR_FAULT;
            return;
        }

        pdesc   = pdev_data->TxDescPtrCur;
        reg_val = pdesc->DES0;

        if (DEF_BIT_IS_CLR(reg_val, A2FX00_BIT_TDES0_OWN)) {
            pdesc->DES0              = A2FX00_BIT_TDES0_OWN;
            pdesc->DES1              = A2FX00_BIT_TDES1_SET
                                     | A2FX00_BIT_TDES1_TER
                                     | A2FX00_BIT_TDES1_FT0
                                     | ((A2FX00_VAL_SETUP_FRAME_SIZE) & A2FX00_MSK_TDES1_TBS1);
            pdesc->DES2              = (CPU_INT32U)psetup;
        } else {
            CPU_CRITICAL_EXIT();
           *perr = NET_DEV_ERR_FAULT;
            return;
        }

        DEF_BIT_CLR(psetup[tbl_ix], DEF_BIT(bit_sel));
                                                                /* -------- CONFIGURE THE ADDRESS FILTERING RAM ------- */
        DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_PM |            /* Set the filtering mode. One physical address and ... */
                                A2FX00_BIT_CSR6_PR);            /* ... 512-bit has table for multicast address.         */
        DEF_BIT_CLR(pdev->CSR7, A2FX00_BIT_CSR7_TIE);           /* ... Early transmit interrupt disable.                */
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);            /* ... Start the transmission.                          */

        reg_to  = A2FX00_VAL_REG_TO;
        reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                                                /* Wait until the setup frame is sent.                  */
        while ((reg_val != A2FX00_BIT_CSR5_TS_SUSPENDED)        &&
               (DEF_BIT_IS_CLR(pdev->CSR5, A2FX00_BIT_CSR5_TI)) &&
               (reg_to   > 0u)) {
            reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
            reg_to--;
        }

        if (reg_to == 0u) {
            CPU_CRITICAL_EXIT();
            *perr = NET_DEV_ERR_FAULT;
            return;
        }

        pdesc->DES0 = DEF_BIT_NONE;
        pdesc->DES1 = A2FX00_BIT_TDES1_IC                       /* Interrupt on completion.                             */
                    | A2FX00_BIT_TDES1_TCH                      /* Second Adress chained.                               */
                    | A2FX00_BIT_TDES1_LS                       /* Last  descriptor of the frame.                       */
                    | A2FX00_BIT_TDES1_FS;                      /* First descriptor of the frame.                       */
        pdesc->DES2 = 0u;                                       /* Set the descriptor data buffer to 'NULL'             */
        pdev->CSR5  = A2FX00_BIT_CSR5_TI;                       /* Clear the interrupt.                                 */

        pdev_data->TxDescPtrCur  = pdev_data->TxDescPtrStart;   /* Re-start the descriptor list.                        */
        pdev_data->TxDescPtrComp = pdev_data->TxDescPtrStart;

        DEF_BIT_SET(pdev->CSR7, A2FX00_BIT_CSR7_TIE);           /* Early transmit interrupt enable.                     */
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR);
        DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);
        CPU_CRITICAL_EXIT();
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
* Note(s)     : (1) This function is called via function pointer from the context of an ISR.
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
    CPU_INT32U          reg_val;
    CPU_INT32U          reg_msk;
    CPU_INT08U         *p_data;
    NET_ERR             err;


   (void)&type;                                                 /* Prevent 'variable unused' compiler warning.          */

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* --------------- DETERMINE ISR TYPE ----------------- */
    reg_msk   =  pdev->CSR7;
    reg_msk  &=  A2FX00_BIT_CSR7_NIE
              |  A2FX00_BIT_CSR7_RIE
              |  A2FX00_BIT_CSR7_TIE;
    reg_val   =  pdev->CSR5;
    reg_val  &=  reg_msk;

                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_CSR5_RI)) {
        pdesc = pdev_data->RxDescPtrCur;
        if (DEF_BIT_IS_CLR(pdesc->DES0, A2FX00_BIT_RDES0_OWN)) {
            NetIF_RxTaskSignal(pif->Nbr, &err);                 /* Signal Net IF RxQ Task.                              */
            DEF_BIT_CLR(pdev->CSR7, A2FX00_BIT_CSR7_RIE);       /* Disable the early receive interrupt.                 */
        }
        pdev->CSR5 = A2FX00_BIT_CSR5_RI;                        /* Clear   the early receive interrupt.                 */
    }

                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if (DEF_BIT_IS_SET(reg_val, A2FX00_BIT_CSR5_TI)) {
        pdesc  = (DEV_DESC   *)pdev_data->TxDescPtrComp;        /* Get the current Tx descriptor.                       */
        p_data = (CPU_INT08U *)pdesc->DES2;                     /* Get the data buffer linked to the descriptor.        */
        NetIF_TxDeallocTaskPost(p_data, &err);                  /* De-allocate the Tx buffer.                           */
        NetIF_DevTxRdySignal(pif);                              /* Signal Net IF that Tx resources are available.       */

        pdesc->DES2              =  DEF_BIT_NONE;               /* Set the descriptor data buffer to 'NULL'             */
                                                                /* Move to the next available Tx descriptor.            */
        pdev_data->TxDescPtrComp = (DEV_DESC *)pdesc->DES3;
        pdev->CSR5               =  A2FX00_BIT_CSR5_TI;         /* Clear the interrupt.                                 */
        pdev->CSR1               = DEF_BIT_01;
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
    NET_DEV_LINK_ETHER  *plink_state;
    NET_DEV_CFG_ETHER   *pdev_cfg;
    NET_DEV             *pdev;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;
    CPU_REG16            reg_to;
    CPU_INT32U           reg_val;


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
                 switch (plink_state->Duplex) {                 /*  Update duplex register setting on device.           */
                    case NET_PHY_DUPLEX_FULL:
                         if (DEF_BIT_IS_CLR(pdev->CSR6, A2FX00_BIT_CSR6_FD)) {
                                                                /* Stop Rx and Tx.                                      */
                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             reg_to     = A2FX00_VAL_REG_TO;
                             reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                             pdev->CSR1 = DEF_BIT_01;
                             while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
                                    (reg_to   > 0u)) {
                                 reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                 pdev->CSR1 = DEF_BIT_01;
                                 reg_to--;
                             }

                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                             reg_to  = A2FX00_VAL_REG_TO;
                             reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                             while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
                                    (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
                                    (reg_to   > 0u)) {
                                 reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                                 reg_to--;
                             }
                                                                /* Set full-duplex mode.                              */
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_FD);
                                                                /* Re-enable Rx/Tx.                                   */
                             pdev->CSR5 = A2FX00_BIT_CSR5_RPS;
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                         }


                         break;

                    case NET_PHY_DUPLEX_HALF:
                         if (DEF_BIT_IS_SET(pdev->CSR6, A2FX00_BIT_CSR6_FD)) {
                                                                /* Stop Rx and Tx.                                      */
                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             reg_to     = A2FX00_VAL_REG_TO;
                             reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                             pdev->CSR1 = DEF_BIT_01;

                             while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
                                    (reg_to   > 0u)) {
                                 reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                 pdev->CSR1 = DEF_BIT_01;
                                 reg_to--;
                             }

                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                             reg_to  = A2FX00_VAL_REG_TO;
                             reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                             while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
                                    (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
                                    (reg_to   > 0u)) {
                                 reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                                 reg_to--;
                             }
                                                                /* Set half-duplex mode.                              */
                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_FD);
                                                                /* Re-enable Rx/Tx.                                   */
                             pdev->CSR5 = A2FX00_BIT_CSR5_RPS;
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                         }
                         break;

                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {                    /*  Update speed register setting on device.            */
                    case NET_PHY_SPD_10:
                         if (DEF_BIT_IS_SET(pdev->CSR6, A2FX00_BIT_CSR6_TTM)) {
                                                                /* Stop Rx and Tx.                                      */
                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             reg_to     = A2FX00_VAL_REG_TO;
                             reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                             pdev->CSR1 = DEF_BIT_01;

                             while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
                                    (reg_to   > 0u)) {
                                 reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                 pdev->CSR1 = DEF_BIT_01;
                                 reg_to--;
                             }

                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                             reg_to  = A2FX00_VAL_REG_TO;
                             reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                             while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
                                    (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
                                    (reg_to   > 0u)) {
                                 reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                                 reg_to--;
                             }
                                                                /* Transmit FIFO threshold set for 10Mbps.              */
                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_TTM);
                             DEF_BIT_CLR(pdev->CSR0, A2FX00_BIT_CSR0_SPD);

                                                                /* Re-enable Rx/Tx.                                   */
                             pdev->CSR5 = A2FX00_BIT_CSR5_RPS;
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                         }
                         break;

                    case NET_PHY_SPD_100:
                         if (DEF_BIT_IS_CLR(pdev->CSR6, A2FX00_BIT_CSR6_TTM)) {
                                                                /* Stop Rx and Tx.                                      */
                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             reg_to     = A2FX00_VAL_REG_TO;
                             reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                             pdev->CSR1 = DEF_BIT_01;

                             while ((reg_val != A2FX00_BIT_CSR5_TS_STOP) &&
                                    (reg_to   > 0u)) {
                                 reg_val    = pdev->CSR5 & A2FX00_MSK_CSR5_TS;
                                 pdev->CSR1 = DEF_BIT_01;
                                 reg_to--;
                             }

                             DEF_BIT_CLR(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                             reg_to  = A2FX00_VAL_REG_TO;
                             reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                             while ((reg_val != A2FX00_BIT_CSR5_RS_STOP     ) &&
                                    (reg_val != A2FX00_BIT_CSR5_RS_SUSPENDED) &&
                                    (reg_to   > 0u)) {
                                 reg_val = pdev->CSR5 & A2FX00_MSK_CSR5_RS;
                                 reg_to--;
                             }

                                                                /* Transmit FIFO threshold set for 100Mbps.            */
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_TTM);
                             DEF_BIT_SET(pdev->CSR0, A2FX00_BIT_CSR0_SPD);

                                                                /* Re-enable Rx/Tx.                                   */
                             pdev->CSR5 = A2FX00_BIT_CSR5_RPS;
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_ST);
                             DEF_BIT_SET(pdev->CSR6, A2FX00_BIT_CSR6_SR);
                         }
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
*                                            NetDev_MII_Rd()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
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
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        CPU_CRITICAL_EXIT();

        return;
    }

    pdev_data->PhyAccessLock = DEF_TRUE;                        /* Acquire the lock.                                    */
    CPU_CRITICAL_EXIT();

    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);              /* Idle cycle.                                          */
    status = NetDev_MII_Clk(pdev);

    if (status == DEF_FAIL) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;

        CPU_CRITICAL_ENTER();
        pdev_data->PhyAccessLock = DEF_FALSE;
        CPU_CRITICAL_EXIT();

        return;
    }


    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);              /* Perform the MDIO preamble for syncronization.        */

    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);          /* Output enable.                                       */
        DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDO);           /* Output = '1'.                                        */
        status  = NetDev_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_RD;

            CPU_CRITICAL_ENTER();
            pdev_data->PhyAccessLock = DEF_FALSE;
            CPU_CRITICAL_EXIT();

            return;
        }
    }


    mdio_frame = A2FX00_BIT_MDIO_START                          /* Build the MDIO frame.                                */
               | A2FX00_BIT_MDIO_OP_RD
               | ((phy_addr & A2FX00_MSK_MDIO_PHY_ADDR ) << 23u)
               | ((reg_addr & A2FX00_MSK_MDIO_REG_ADDR ) << 18u);

    mdio_frame_bit = 31u;

    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        if (mdio_frame_bit == 17u) {
            DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);      /* Start Read cycle.                                    */
        }

        if (mdio_frame_bit >= 16u) {
            if (DEF_BIT_IS_SET(mdio_frame, DEF_BIT(mdio_frame_bit))) {
                DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDO);
            } else {
                DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDO);
            }
        } else {
            if (DEF_BIT_IS_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDI)) {
                DEF_BIT_SET(mdio_frame, DEF_BIT(mdio_frame_bit));
            }
        }

        status  = NetDev_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_RD;

            CPU_CRITICAL_ENTER();
            pdev_data->PhyAccessLock = DEF_FALSE;
            CPU_CRITICAL_EXIT();

            return;
        }

        mdio_frame_bit--;
    }

    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDC);
   *p_data = (CPU_INT16U )mdio_frame & DEF_INT_16_MASK;

    CPU_CRITICAL_ENTER();
    pdev_data->PhyAccessLock = DEF_FALSE;                       /* Release the lock.                                    */
    CPU_CRITICAL_EXIT();


   *perr =  NET_PHY_ERR_NONE;
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
*               perr    Pointer to return error code.
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
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
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        CPU_CRITICAL_EXIT();
        return;
    }

    pdev_data->PhyAccessLock = DEF_TRUE;                        /* Acquire the lock.                                    */
    CPU_CRITICAL_EXIT();

    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);              /* Idle cycle.                                          */
    status  = NetDev_MII_Clk(pdev);

    if (status == DEF_FAIL) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }


    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);              /* Perform the MDIO preamble for syncronization.        */

    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDEN);          /* Output enable.                                       */
        DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDO);           /* Output = '1'.                                        */
        status = NetDev_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_WR;

            CPU_CRITICAL_ENTER();
            pdev_data->PhyAccessLock = DEF_FALSE;
            CPU_CRITICAL_EXIT();

            return;
        }
    }


    mdio_frame = A2FX00_BIT_MDIO_START                          /* Build the MDIO frame.                                */
               | A2FX00_BIT_MDIO_OP_WR
               | A2FX00_BIT_MDIO_OP_TA_WR
               | ((phy_addr & A2FX00_MSK_MDIO_PHY_ADDR) << 23u)
               | ((reg_addr & A2FX00_MSK_MDIO_REG_ADDR) << 18u)
               | ((data     & A2FX00_MSK_MDIO_DATA    )       );

    mdio_frame_bit = 31u;

    for (clk_cycles = 0u; clk_cycles < 32u; clk_cycles++) {
        if (DEF_BIT_IS_SET(mdio_frame, DEF_BIT(mdio_frame_bit))) {
            DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDO);
        } else {
            DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDO);
        }

        status = NetDev_MII_Clk(pdev);
        if (status == DEF_FAIL) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_WR;

            CPU_CRITICAL_ENTER();
            pdev_data->PhyAccessLock = DEF_FALSE;
            CPU_CRITICAL_EXIT();

            return;
        }

        mdio_frame_bit--;
    }

    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDC);

    CPU_CRITICAL_ENTER();
    pdev_data->PhyAccessLock = DEF_FALSE;                       /* Release the lock.                                    */
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
    DEV_DESC           *pdesc;
    NET_DEV            *pdev;
    MEM_POOL           *pmem_pool;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbr_bytes;
    CPU_INT16U          desc_nbr;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool = &pdev_data->RxDescPool;
    nbr_bytes =  pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdesc     = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                           (CPU_SIZE_T) nbr_bytes,
                                           (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->RxDescPtrCur   = (DEV_DESC *)pdesc;
                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (desc_nbr = 0u; desc_nbr < pdev_cfg->RxDescNbr; desc_nbr++) {
        pdesc->DES0 = A2FX00_BIT_RDES0_OWN;                     /* Gives ownership to the ethernet MAC.                 */
        pdesc->DES1 = A2FX00_BIT_RDES1_RCH                      /* Second address chained.                              */
                    | (pdev_cfg->RxBufLargeSize & A2FX00_MSK_RDES1_RBS1);

                                                                /* Obtain a Rx data buffer.                             */
        pdesc->DES2 = (CPU_REG32)NetBuf_GetDataPtr((NET_IF        *)pif,
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


        if (desc_nbr < (pdev_cfg->RxDescNbr - 1u)) {
            pdesc->DES3 = (CPU_INT32U)(pdesc + 1u);             /* Set the next descriptor pointer.                     */
        } else {
            pdesc->DES3 = (CPU_INT32U)(pdev_data->RxDescPtrStart);
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }
                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
    pdev->CSR3 = (CPU_INT32U)pdev_data->RxDescPtrStart;         /* Configure the DMA with the Rx desc start address.    */

   *perr       =  NET_DEV_ERR_NONE;
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

static  void  NetDev_RxDescFreeAll (NET_IF   *pif,
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
    pdesc     =  pdev_data->RxDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */
        pdesc_data = (CPU_INT08U *)(pdesc->DES2);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
                     pdev_data->RxDescPtrStart,
                    &lib_err);

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

static  void  NetDev_RxDescPtrCurInc (NET_IF *pif)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    NET_DEV            *pdev;
    NET_ERR             err;

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_cfg                = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data               = (NET_DEV_DATA      *)pif->Dev_Data;
    pdesc                   = (DEV_DESC          *)pdev_data->RxDescPtrCur;
    pdev                    = (NET_DEV           *)pdev_cfg->BaseAddr;

    pdev_data->RxDescPtrCur = (DEV_DESC          *)pdesc->DES3; /* Move to the next pointer available.                  */
                                                                /* Give the descriptor back to the MAC.                 */
    pdesc->DES0             = (CPU_INT32U         )A2FX00_BIT_RDES0_OWN;
    pdesc                   = (DEV_DESC          *)pdev_data->RxDescPtrCur;

                                                                /* If a packet has been received in the next ...        */
                                                                /* ... available descriptor.                            */
    if (DEF_BIT_IS_CLR(pdesc->DES0, A2FX00_BIT_RDES0_OWN)) {
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* ... signal Net IF RxQ Task, otherwise ...            */
    } else {
        DEF_BIT_SET(pdev->CSR7, A2FX00_BIT_CSR7_RIE);           /* ... enable the early receive interrupt.              */
    }

    pdev->CSR2 = DEF_BIT_01;

    (void)err;
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
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    MEM_POOL           *pmem_pool;
    CPU_SIZE_T          nbr_bytes;
    CPU_INT16U          desc_ix;
    LIB_ERR             lib_err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool = &pdev_data->TxDescPool;
    nbr_bytes =  pdev_cfg->TxDescNbr * sizeof(DEV_DESC);
    pdesc     = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                           (CPU_SIZE_T) nbr_bytes,
                                           (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }

                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->TxDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->TxDescPtrCur   = (DEV_DESC *)pdesc;
    pdev_data->TxDescPtrComp  = (DEV_DESC *)pdesc;
                                                                /* --------------- INIT TX DESCRIPTORS ---------------- */
                                                                /* Initialize Tx descriptor ring                        */
    for (desc_ix = 0u; desc_ix < pdev_cfg->TxDescNbr; desc_ix++) {
        pdesc->DES0 = DEF_BIT_NONE;
        pdesc->DES1 = A2FX00_BIT_TDES1_IC                       /* Interrupt on completion.                             */
                    | A2FX00_BIT_TDES1_TCH                      /* Second Adress chained.                               */
                    | A2FX00_BIT_TDES1_LS                       /* Last  descriptor of the frame.                       */
                    | A2FX00_BIT_TDES1_FS;                      /* First descriptor of the frame.                       */
        pdesc->DES2 = DEF_BIT_NONE;

        if (desc_ix < (pdev_cfg->TxDescNbr - 1u)) {
            pdesc->DES3 = (CPU_INT32U)(pdesc + 1u);             /* Point to next descriptor in list.                    */
        } else {
                                                                /* Point to the fist descriptor in the list.            */
            pdesc->DES3 = (CPU_INT32U)(pdev_data->TxDescPtrStart);
        }
        pdesc++;
    }
                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
    pdev->CSR4 = (CPU_INT32U)pdev_data->TxDescPtrStart;         /* Configure the DMA with the Tx desc start address.    */

   *perr       =  NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Clk()
*
* Description : Perform a MDIO clk cycle.
*
* Argument(s) : pdev    Pointer to the interface device registers.
*
* Return(s)   : DEF_OK,   If the clock cycle was performed correctly,
*               DEF_FAIL, Otherwise.
*
* Caller(s)   : NetDev_MII_Rd(),
*               NetDev_MII_Clr().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetDev_MII_Clk (NET_DEV  *pdev)
{
    CPU_TS           ts_start;
    CPU_TS           ts_cur;
    CPU_TS           ts_delta;
    CPU_TS           ts_clk_period;
    CPU_TS_TMR_FREQ  ts_tmr_freq;
    CPU_ERR          cpu_err;
    CPU_SIZE_T       reg_to;


    ts_tmr_freq = CPU_TS_TmrFreqGet(&cpu_err);                  /* Get the current TS timer frequency.                  */

    if (cpu_err != CPU_ERR_NONE) {
        return (DEF_FAIL);
    }

    ts_clk_period = (ts_tmr_freq / A2FX00_VAL_MDIO_MIN_FREQ);   /* Calculate the PHY period in TS units.                */

    if (ts_clk_period == 0u) {                                  /* If 'A2FX00_VAL_MDIO_MIN_FREQ > ts_tmr_freq' ...      */
        return (DEF_FAIL);                                      /* ... return an error.                                 */
    }

    reg_to   = A2FX00_VAL_CLK_TO;
    ts_start = NET_DEV_TS_GET();                                /* Get current TS value.                                */
    ts_cur   = NET_DEV_TS_GET();
    ts_delta = ts_cur - ts_start;

    DEF_BIT_CLR(pdev->CSR9, A2FX00_BIT_CSR9_MDC);               /* Set the Clk to '0'.                                  */

    while ((ts_delta < (ts_clk_period / 2u)) &&                 /* Wait for half-period.                                */
           (reg_to   > 0u                 )) {
        ts_cur   = NET_DEV_TS_GET();
        ts_delta = (ts_cur - ts_start);
        reg_to--;
    }

    if (reg_to == 0u) {
        return (DEF_FAIL);
    }

    DEF_BIT_SET(pdev->CSR9, A2FX00_BIT_CSR9_MDC);               /* Set the Clk to '1'.                                  */
                                                                /* Wait for half-period.                                */
    reg_to   = A2FX00_VAL_CLK_TO;
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


#endif  /* NET_IF_ETHER_MODULE_EN */
