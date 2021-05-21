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
*
* The NET_DEV_ETHMAC data structure was provided by STMicroelectronics and is subject to the following
* conditions:
*
* ******************************************************************************
* * @attention
* *
* * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
* *
* * Redistribution and use in source and binary forms, with or without modification,
* * are permitted provided that the following conditions are met:
* *   1. Redistributions of source code must retain the above copyright notice,
* *      this list of conditions and the following disclaimer.
* *   2. Redistributions in binary form must reproduce the above copyright notice,
* *      this list of conditions and the following disclaimer in the documentation
* *      and/or other materials provided with the distribution.
* *   3. Neither the name of STMicroelectronics nor the names of its contributors
* *      may be used to endorse or promote products derived from this software
* *      without specific prior written permission.
* *
* * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* *
* ******************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                         NETWORK DEVICE DRIVER
*
*                                                ETHMAC
*
* Filename : net_dev_ethmac.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) With an appropriate BSP, this driver will support the following MCU series:
*
*                     STMicroelectronics  STM32H7xx  series
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_ETHMAC_MODULE
#include  <cpu_cache.h>
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_ethmac.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                             LOCAL DEFINES
*********************************************************************************************************
*/
                                                                /* -------------- PHY Read/write Timeouts ------------- */
#define  PHY_TIMEOUT                          (0x0004FFFFu)
#define  RST_TIMEOUT                          (0x0004FFFFu)
                                                                /* ------------ Register Bit Field Defines ------------ */
#define  DMAMR_SWR                             DEF_BIT_00
#define  DMACCR_DSL_SHIFT                     (18u)
#define  MTLTQOMR_TSF                          DEF_BIT_01
#define  MTLRQOMR_RSF                          DEF_BIT_05
#define  MTLRQDR_RWCSTS                        DEF_BIT_00
#define  MTLTQDR_PRXQ_MASK                    (0x3FFF0000u)
#define  MTLTQDR_PRXQ_SHIFT                   (16u)
#define  MTLTQDR_TRCSTS                        DEF_BIT_04
#define  MTLTQDR_TXQSTS_MASK                  (0x00000060u)
#define  MTLTQDR_TXQSTS_SHIFT                 (1u)
#define  MACMDIOAR_CR_124                     (5u << 8u)
#define  MACMDIOAR_CR_102                     (4u << 8u)
#define  MACMDIOAR_CR_62                      (1u << 8u)
#define  MACMDIOAR_CR_42                      (0u << 8u)
#define  MACMDIOAR_CR_26                      (3u << 8u)
#define  MACMDIOAR_CR_16                      (2u << 8u)
#define  MACMDIOAR_CR_MASK                    (0x00000F00u)
#define  MACPFR_HPF                            DEF_BIT_10
#define  MACPFR_HMC                            DEF_BIT_02
#define  MACPFR_HUC                            DEF_BIT_01
#define  DMACIER_NIE                           DEF_BIT_15
#define  DMACIER_RBUE                          DEF_BIT_07
#define  DMACIER_RIE                           DEF_BIT_06
#define  DMACIER_TBUE                          DEF_BIT_02
#define  DMACRCR_SR                            DEF_BIT_00
#define  MACCR_IPC                             DEF_BIT_27
#define  MACCR_TE                              DEF_BIT_01
#define  MACCR_RE                              DEF_BIT_00
#define  RX_DESC_DES3_OWN                      DEF_BIT_31
#define  RX_DESC_DES3_BUF1V                    DEF_BIT_24
#define  RX_DESC_DES3_IOC                      DEF_BIT_30
#define  RX_DESC_DES3_LEN_MASK                (0x00007FFFu)
#define  TX_DESC_DES2_LEN1_MASK               (0x00003FFFu)
#define  TX_DESC_DES3_OWN                      DEF_BIT_31
#define  TX_DESC_DES3_FD                       DEF_BIT_29
#define  TX_DESC_DES3_LD                       DEF_BIT_28
#define  TX_DESC_DES3_CIC                     (DEF_BIT_17 | DEF_BIT_16)
#define  DMACTCR_ST                            DEF_BIT_00
#define  DMACSR_TBU                            DEF_BIT_02
#define  DMACSR_TPS                            DEF_BIT_01
#define  DMACSR_RI                             DEF_BIT_06
#define  DMACSR_RBU                            DEF_BIT_07
#define  DMACSR_RPS                            DEF_BIT_08
#define  MACCR_DM                              DEF_BIT_13
#define  MACCR_FES                             DEF_BIT_14
#define  MACMDIOAR_PA_MASK                    (0x03E00000u)
#define  MACMDIOAR_RDA_MASK                   (0x001F0000u)
#define  MACMDIOAR_GOC_MASK                   (0x0000000Cu)
#define  MACMDIOAR_GOC_RD                     (3u << 2u)
#define  MACMDIOAR_GOC_WR                     (1u << 2u)
#define  MACMDIOAR_MB                          DEF_BIT_00


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
                                                                /* --------------- HARDWARE DESCRIPTORS --------------- */
typedef  struct  dev_desc {                                     /* Use of the descriptor fields differs for Tx and Rx.  */
    CPU_REG32  DES0;
    CPU_REG32  DES1;
    CPU_REG32  DES2;
    CPU_REG32  DES3;

    CPU_REG32  SoftAddr;                                        /* Not used by DMA. Keeps a copy of the buffer addr.    */
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
                                                                /* Number of descriptors allocated in device cfg.       */
    NET_BUF_QTY   RxDescNbr;
    NET_BUF_QTY   TxDescNbr;
                                                                /* Rx/Tx descriptor ring management.                    */
    DEV_DESC     *RxRingHead;
    DEV_DESC     *RxRingRxd;
    NET_BUF_QTY   RxRingSize;
    NET_BUF_QTY   RxRingRxdCnt;
    CPU_INT08U   *RxKeepAliveBuf;                               /* Keeps Rx alive in case we run out of bufs.           */
    CPU_BOOLEAN   RxKeepAlive;
    DEV_DESC     *TxRingHead;
    NET_BUF_QTY   TxRingSize;
    CPU_BOOLEAN   RxBusy;
    CPU_BOOLEAN   TxBusy;

                                                                /* Memory used for buffer queue.                        */
    MEM_POOL      TxQPool;

                                                                /* Tx buffer queue management.                          */
    DEV_BUF_Q    *TxQ;
    NET_BUF_QTY   TxQFront;
    NET_BUF_QTY   TxQBack;
    NET_BUF_QTY   TxQSize;

#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U    MulticastAddrHashBitCtr[64];
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

static  void  NetDev_RxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *p_err);

static  void  NetDev_TxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *p_err);

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

const  NET_DEV_API_ETHER  NetDev_API_ETHMAC = {                               /* ETHMAC dev API fnct ptrs :             */
                                                &NetDev_Init,                 /*   Init/add                             */
                                                &NetDev_Start,                /*   Start                                */
                                                &NetDev_Stop,                 /*   Stop                                 */
                                                &NetDev_Rx,                   /*   Rx                                   */
                                                &NetDev_Tx,                   /*   Tx                                   */
                                                &NetDev_AddrMulticastAdd,     /*   Multicast addr add                   */
                                                &NetDev_AddrMulticastRemove,  /*   Multicast addr remove                */
                                                &NetDev_ISR_Handler,          /*   ISR handler                          */
                                                &NetDev_IO_Ctrl,              /*   I/O ctrl                             */
                                                &NetDev_MII_Rd,               /*   Phy reg rd                           */
                                                &NetDev_MII_Wr                /*   Phy reg wr                           */
                                              };


/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  MACCR;
    CPU_REG32  MACECR;
    CPU_REG32  MACPFR;
    CPU_REG32  MACWTR;
    CPU_REG32  MACHT0R;
    CPU_REG32  MACHT1R;
    CPU_REG32  RESERVED1[14];
    CPU_REG32  MACVTR;
    CPU_REG32  RESERVED2;
    CPU_REG32  MACVHTR;
    CPU_REG32  RESERVED3;
    CPU_REG32  MACVIR;
    CPU_REG32  MACIVIR;
    CPU_REG32  RESERVED4[2];
    CPU_REG32  MACTFCR;
    CPU_REG32  RESERVED5[7];
    CPU_REG32  MACRFCR;
    CPU_REG32  RESERVED6[7];
    CPU_REG32  MACISR;
    CPU_REG32  MACIER;
    CPU_REG32  MACRXTXSR;
    CPU_REG32  RESERVED7;
    CPU_REG32  MACPCSR;
    CPU_REG32  MACRWKPFR;
    CPU_REG32  RESERVED8[2];
    CPU_REG32  MACLCSR;
    CPU_REG32  MACLTCR;
    CPU_REG32  MACLETR;
    CPU_REG32  MAC1USTCR;
    CPU_REG32  RESERVED9[12];
    CPU_REG32  MACVR;
    CPU_REG32  MACDR;
    CPU_REG32  RESERVED10;
    CPU_REG32  MACHWF0R;
    CPU_REG32  MACHWF1R;
    CPU_REG32  MACHWF2R;
    CPU_REG32  RESERVED11[54];
    CPU_REG32  MACMDIOAR;
    CPU_REG32  MACMDIODR;
    CPU_REG32  RESERVED12[2];
    CPU_REG32  MACARPAR;
    CPU_REG32  RESERVED13[59];
    CPU_REG32  MACA0HR;
    CPU_REG32  MACA0LR;
    CPU_REG32  MACA1HR;
    CPU_REG32  MACA1LR;
    CPU_REG32  MACA2HR;
    CPU_REG32  MACA2LR;
    CPU_REG32  MACA3HR;
    CPU_REG32  MACA3LR;
    CPU_REG32  RESERVED14[248];
    CPU_REG32  MMCCR;
    CPU_REG32  MMCRIR;
    CPU_REG32  MMCTIR;
    CPU_REG32  MMCRIMR;
    CPU_REG32  MMCTIMR;
    CPU_REG32  RESERVED15[14];
    CPU_REG32  MMCTSCGPR;
    CPU_REG32  MMCTMCGPR;
    CPU_REG32  RESERVED16[5];
    CPU_REG32  MMCTPCGR;
    CPU_REG32  RESERVED17[10];
    CPU_REG32  MMCRCRCEPR;
    CPU_REG32  MMCRAEPR;
    CPU_REG32  RESERVED18[10];
    CPU_REG32  MMCRUPGR;
    CPU_REG32  RESERVED19[9];
    CPU_REG32  MMCTLPIMSTR;
    CPU_REG32  MMCTLPITCR;
    CPU_REG32  MMCRLPIMSTR;
    CPU_REG32  MMCRLPITCR;
    CPU_REG32  RESERVED20[65];
    CPU_REG32  MACL3L4C0R;
    CPU_REG32  MACL4A0R;
    CPU_REG32  RESERVED21[2];
    CPU_REG32  MACL3A0R0R;
    CPU_REG32  MACL3A1R0R;
    CPU_REG32  MACL3A2R0R;
    CPU_REG32  MACL3A3R0R;
    CPU_REG32  RESERVED22[4];
    CPU_REG32  MACL3L4C1R;
    CPU_REG32  MACL4A1R;
    CPU_REG32  RESERVED23[2];
    CPU_REG32  MACL3A0R1R;
    CPU_REG32  MACL3A1R1R;
    CPU_REG32  MACL3A2R1R;
    CPU_REG32  MACL3A3R1R;
    CPU_REG32  RESERVED24[108];
    CPU_REG32  MACTSCR;
    CPU_REG32  MACSSIR;
    CPU_REG32  MACSTSR;
    CPU_REG32  MACSTNR;
    CPU_REG32  MACSTSUR;
    CPU_REG32  MACSTNUR;
    CPU_REG32  MACTSAR;
    CPU_REG32  RESERVED25;
    CPU_REG32  MACTSSR;
    CPU_REG32  RESERVED26[3];
    CPU_REG32  MACTTSSNR;
    CPU_REG32  MACTTSSSR;
    CPU_REG32  RESERVED27[2];
    CPU_REG32  MACACR;
    CPU_REG32  RESERVED28;
    CPU_REG32  MACATSNR;
    CPU_REG32  MACATSSR;
    CPU_REG32  MACTSIACR;
    CPU_REG32  MACTSEACR;
    CPU_REG32  MACTSICNR;
    CPU_REG32  MACTSECNR;
    CPU_REG32  RESERVED29[4];
    CPU_REG32  MACPPSCR;
    CPU_REG32  RESERVED30[3];
    CPU_REG32  MACPPSTTSR;
    CPU_REG32  MACPPSTTNR;
    CPU_REG32  MACPPSIR;
    CPU_REG32  MACPPSWR;
    CPU_REG32  RESERVED31[12];
    CPU_REG32  MACPOCR;
    CPU_REG32  MACSPI0R;
    CPU_REG32  MACSPI1R;
    CPU_REG32  MACSPI2R;
    CPU_REG32  MACLMIR;
    CPU_REG32  RESERVED32[11];
    CPU_REG32  MTLOMR;
    CPU_REG32  RESERVED33[7];
    CPU_REG32  MTLISR;
    CPU_REG32  RESERVED34[55];
    CPU_REG32  MTLTQOMR;
    CPU_REG32  MTLTQUR;
    CPU_REG32  MTLTQDR;
    CPU_REG32  RESERVED35[8];
    CPU_REG32  MTLQICSR;
    CPU_REG32  MTLRQOMR;
    CPU_REG32  MTLRQMPOCR;
    CPU_REG32  MTLRQDR;
    CPU_REG32  RESERVED36[177];
    CPU_REG32  DMAMR;
    CPU_REG32  DMASBMR;
    CPU_REG32  DMAISR;
    CPU_REG32  DMADSR;
    CPU_REG32  RESERVED37[60];
    CPU_REG32  DMACCR;
    CPU_REG32  DMACTCR;
    CPU_REG32  DMACRCR;
    CPU_REG32  RESERVED38[2];
    CPU_REG32  DMACTDLAR;
    CPU_REG32  RESERVED39;
    CPU_REG32  DMACRDLAR;
    CPU_REG32  DMACTDTPR;
    CPU_REG32  RESERVED40;
    CPU_REG32  DMACRDTPR;
    CPU_REG32  DMACTDRLR;
    CPU_REG32  DMACRDRLR;
    CPU_REG32  DMACIER;
    CPU_REG32  DMACRIWTR;
    CPU_REG32  DMACSFCSR;
    CPU_REG32  RESERVED41;
    CPU_REG32  DMACCATDR;
    CPU_REG32  RESERVED42;
    CPU_REG32  DMACCARDR;
    CPU_REG32  RESERVED43;
    CPU_REG32  DMACCATBR;
    CPU_REG32  RESERVED44;
    CPU_REG32  DMACCARBR;
    CPU_REG32  DMACSR;
    CPU_REG32  RESERVED45[2];
    CPU_REG32  DMACMFCR;
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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          input_freq;
    CPU_INT32U          mdc_div;
    CPU_SIZE_T          nbytes;
    CPU_REG32           timeout;
    LIB_ERR             lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
    if (pdev_cfg->RxBufLargeNbr == 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
    if ((pdev_cfg->TxBufLargeNbr + pdev_cfg->TxBufSmallNbr) == 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pdev_cfg->RxDescNbr == 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pdev_cfg->TxDescNbr == 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pdev_cfg->RxBufIxOffset != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pdev_cfg->TxBufIxOffset != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* ------------- ALLOCATE DEV DATA AREA --------------- */
    pif->Dev_Data = Mem_HeapAlloc( sizeof(NET_DEV_DATA),
                                   4u,
                                   0,
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
                           1,                                   /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold all Rx descriptors.  */
                           sizeof(CPU_CFG_DATA_SIZE),           /* Block alignment.                                     */
                           0,                                   /* Optional, ptr to variable to store rem nbr bytes.    */
                          &lib_err);                            /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }
                                                                /* ----------- ALLOCATE RX KEEPALIVE BUFFER ----------- */
    pdev_data->RxKeepAliveBuf = Mem_HeapAlloc( pdev_cfg->RxBufLargeSize,
                                               pdev_cfg->RxBufAlignOctets,
                                               0,
                                              &lib_err);
                                                                /* ---------------- TX DMA DESCRIPTORS ---------------- */
    nbytes = pdev_data->TxDescNbr * sizeof(DEV_DESC);           /* Determine block size.                                */
    Mem_PoolCreate(       &pdev_data->TxDescPool,               /* Pass a pointer to the mem pool to create.            */
                   (void *)pdev_cfg->MemAddr,                   /* From the dedicated memory.                           */
                           pdev_cfg->MemSize,                   /* Dedicated area size.                                 */
                           1,                                   /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold all Tx descriptors.  */
                           sizeof(CPU_CFG_DATA_SIZE),           /* Block alignment.                                     */
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
                           1,                                   /* Allocate 1 block.                                    */
                           nbytes,                              /* Block size large enough to hold Tx buffer queue.     */
                           sizeof(DEV_BUF_Q *),                 /* Block alignment.                                     */
                           0,                                   /* Optional, ptr to variable to store rem nbr bytes.    */
                          &lib_err);                            /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }
                                                                /* ==================================================== */
                                                                /* =================== HARDWARE CFG =================== */
                                                                /* ==================================================== */
                                                                /* -------------------- CFG SYSTEM -------------------- */
    pdev_bsp->CfgClk(pif, p_err);                               /* Enable module clks (GPIO & ETHMAC).                  */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    pdev_bsp->CfgGPIO(pif, p_err);                              /* Configure MAC / Phy GPIO.                            */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    pdev_bsp->CfgIntCtrl(pif, p_err);                           /* Configure ext int ctrl'r.                            */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }
                                                                /* --------------------- INIT DMA --------------------- */
    timeout      = RST_TIMEOUT;
    pdev->DMAMR |= DMAMR_SWR;                                   /* Software reset.                                      */
    while (pdev->DMAMR & DMAMR_SWR) {                           /* Wait for reset to complete.                          */
        if (timeout == 0u) {
           *p_err = NET_DEV_ERR_INIT;                           /* Return with an error if reset doesn't complete.      */
            return;
        }
        timeout--;
    }
    pdev->DMACCR = 1u << DMACCR_DSL_SHIFT;                      /* Skip 4 bytes between descriptors for SoftAddr.       */

                                                                /* --------------------- INIT MTL --------------------- */
    pdev->MTLTQOMR |= MTLTQOMR_TSF;                             /* Enable Tx Store and Forward mode.                    */
    pdev->MTLRQOMR |= MTLRQOMR_RSF;                             /* Enable Rx Store and Forward mode.                    */

                                                                /* --------------------- INIT MAC --------------------- */
                                                                /* Get MDC clk freq (in Hz).                            */
    input_freq = pdev_bsp->ClkFreqGet(pif, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    if (input_freq > 250000000UL) {                             /* Determine MDC clk div.                               */
        mdc_div = (CPU_INT32U)MACMDIOAR_CR_124;
    } else if (input_freq > 150000000UL) {
        mdc_div = (CPU_INT32U)MACMDIOAR_CR_102;
    } else if (input_freq > 100000000UL) {
        mdc_div = (CPU_INT32U)MACMDIOAR_CR_62;
    } else if (input_freq >  60000000UL) {
        mdc_div = (CPU_INT32U)MACMDIOAR_CR_42;
    } else if (input_freq >  35000000UL) {
        mdc_div = (CPU_INT32U)MACMDIOAR_CR_26;
    } else {
        mdc_div = (CPU_INT32U)MACMDIOAR_CR_16;
    }

    pdev->MACMDIOAR &= MACMDIOAR_CR_MASK;
    pdev->MACMDIOAR |= mdc_div;                                 /* Set MDC divider.                                     */

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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
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
            hw_addr[0u] = (pdev->MACA0LR >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1u] = (pdev->MACA0LR >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2u] = (pdev->MACA0LR >> (2u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3u] = (pdev->MACA0LR >> (3u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4u] = (pdev->MACA0HR >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5u] = (pdev->MACA0HR >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->MACA0HR = (((CPU_INT32U)hw_addr[4u] << (0u * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[5u] << (1u * DEF_INT_08_NBR_BITS)));

        pdev->MACA0LR = (((CPU_INT32U)hw_addr[0u] << (0u * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[1u] << (1u * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[2u] << (2u * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[3u] << (3u * DEF_INT_08_NBR_BITS)));
    }

                                                                /* ------------------ CFG MULTICAST ------------------- */
    pdev->MACHT0R = 0u;                                         /* Clr multicast hash filter hi bits.                   */
    pdev->MACHT1R = 0u;                                         /* Clr multicast hash filter lo bits.                   */
    pdev->MACPFR  = MACPFR_HPF |                                /* Enable Hash or Perfect filter.                       */
                    MACPFR_HMC |                                /* Enable Multicast Hashing.                            */
                    MACPFR_HUC;                                 /* Enable Unicast Hashing.                              */

#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr(       pdev_data->MulticastAddrHashBitCtr,
            sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif


    NetDev_RxDescInit(pif, p_err);                              /* Init Rx desc's.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_TxDescInit(pif, p_err);                              /* Init Tx desc'c.                                      */
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
    pdev_data->TxBusy      = DEF_FALSE;
    pdev_data->RxBusy      = DEF_TRUE;
    pdev_data->RxKeepAlive = DEF_FALSE;

                                                                /* ----------------- START DMA ENGINE ----------------- */
                                                                /* Enable Tx/Rx interrupts.                             */
    pdev->DMACIER  = DMACIER_NIE  |
                     DMACIER_RBUE |
                     DMACIER_RIE  |
                     DMACIER_TBUE;

    pdev->DMACRCR |= DMACRCR_SR;                                /* Start DMA Reception.                                 */

    pdev->MACCR   |= MACCR_IPC |                                /* Enable Rx checksum offloading.                       */
                     MACCR_TE  |                                /* Enable Transmitter and Receiver.                     */
                     MACCR_RE;

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

                                                                /* ---------------- STOP THE CONTROLLER --------------- */
    pdev->DMACIER  =   0u;                                      /* Disable interrupts.                                  */
    pdev->DMACTCR &= ~(DMACTCR_ST);                             /* Stop Tx DMA Engine.                                  */
                                                                /* Wait for Stop to complete.                           */
    while ( (pdev->MTLTQDR & MTLTQDR_TRCSTS                                  == 0x1) ||
           (((pdev->MTLTQDR & MTLTQDR_TXQSTS_MASK) >> MTLTQDR_TXQSTS_SHIFT ) !=   0));

    pdev->MACCR    = 0u;                                        /* Disable Transmitter and Receiver.                    */

    pdev->DMACRCR &= ~(DMACRCR_SR);                             /* Stop Rx DMA Engine.                                  */
                                                                /* Wait for Stop to complete.                           */
    while ( (pdev->MTLRQDR & MTLRQDR_RWCSTS                              != 0x0) ||
           (((pdev->MTLTQDR & MTLTQDR_PRXQ_MASK) >> MTLTQDR_PRXQ_SHIFT ) !=   0));

                                                                /* ---------------- FREE THE RESOURCES ---------------- */
    NetDev_RxDescFreeAll(pif, p_err);                           /* Free Rx desc's.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_TxDescFreeAll(pif, p_err);                           /* Free Tx desc'c.                                      */
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
*                   (a) Determine which receive descriptor caused the interrupt
*                   (b) Obtain pointer to data area to replace existing data area
*                   (c) Reconfigure descriptor with pointer to new data area
*                   (d) Set return values.  Pointer to received data area and size
*                   (e) Update current receive descriptor pointer
*                   (f) Increment counters.
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
*                           NET_DEV_ERR_RX_BUSY             Frame was dropped due to lack of resources.
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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    NET_DEV            *pdev;
    CPU_BOOLEAN         rx_busy;
    NET_BUF_QTY         i;
    NET_ERR             buf_err;
    CPU_SR_ALLOC();

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;

    CPU_CRITICAL_ENTER();
    rx_busy = pdev_data->RxBusy;                                /* Check busy signal from the ISR.                      */
    CPU_CRITICAL_EXIT();

    pdesc  =                pdev_data->RxRingRxd++;             /* pdesc points to the frame we received.               */
   *p_data = (CPU_INT08U *) pdesc->SoftAddr;
   *size   =               (pdesc->DES3 & RX_DESC_DES3_LEN_MASK);
    CPU_DCACHE_RANGE_INV(*p_data, *size);

   *p_err = NET_DEV_ERR_NONE;                                   /* Report Rx successful in case we return.              */
    if (rx_busy == DEF_TRUE) {
        return;
    }
                                                                /* ---------------------------------------------------- */
                                                                /* ------------------ RX ENGINE IDLE ------------------ */
                                                                /* ---------------------------------------------------- */
    if ((pdev_data->RxRingRxd - pdev_data->RxRingHead) != pdev_data->RxRingSize) {
        return;                                                 /* We have more Rx signals to process.                  */
    }

                                                                /* - ALL RX SIGNALS PROCESSED : RESTART THE RX ENGINE - */
    pdev_data->RxBusy = DEF_YES;

                                                                /* ------------ REPOPULATE THE DESCRIPTORS ------------ */
    pdev_data->RxRingRxd    = pdev_data->RxRingHead;
    pdev_data->RxRingRxdCnt = 0u;
    pdesc                   = pdev_data->RxRingHead;
    for (i = 0u; i < pdev_data->RxDescNbr; i++) {               /* Make a best effort to fill up all the Descriptors.   */
        pdesc->SoftAddr = (CPU_INT32U)NetBuf_GetDataPtr( pif,
                                                         NET_TRANSACTION_RX,
                                                         NET_IF_ETHER_FRAME_MAX_SIZE,
                                                         NET_IF_IX_RX,
                                                         0u,
                                                         0u,
                                                         0u,
                                                        &buf_err);
        if (buf_err != NET_BUF_ERR_NONE) {
            break;                                              /* Stop trying once we can't allocate any more bufs.    */
        }

        pdesc->DES0 = pdesc->SoftAddr;                          /* If we have a buf, place it in the descriptor.        */
        pdesc->DES1 = 0u;
        pdesc->DES2 = 0u;
        pdesc->DES3 = RX_DESC_DES3_OWN   |                      /* DMA owns the desc.                                   */
                      RX_DESC_DES3_BUF1V |                      /* Buffer 1 address is valid.                           */
                      RX_DESC_DES3_IOC;                         /* Interrupt on reception.                              */
        pdesc++;
    }

    pdev_data->RxRingSize = i;                                  /* Only count the descriptors we've populated.          */

                                                                /* ------ PROCESS FINAL FRAME FROM PREVIOUS LIST ------ */
    if (pdev_data->RxKeepAlive == DEF_FALSE) {                  /* Was this frame placed in the keep-alive buffer?      */
       *p_err = NET_DEV_ERR_NONE;                               /* NO - OK to pass the frame back up.                   */
    } else {
       *p_data = (CPU_INT08U *)0;                               /* YES - Drop the frame.                                */
       *size   =               0u;

        if (pdev_data->RxRingSize == 0u) {
           *p_err = buf_err;                                    /* Still no valid bufs: Return the Buf error.           */
        } else {
           *p_err = NET_DEV_ERR_RX_BUSY;                        /* Got some valid bufs, but frame is still invalid.     */
        }
    }
                                                                /* ------------ PREP LIST FOR NEXT RECEIVE ------------ */
    if (pdev_data->RxRingSize == 0) {
        pdesc->SoftAddr = (CPU_INT32U)pdev_data->RxKeepAliveBuf;/* Use keep-alive buffer until Rx bufs are available.   */
        pdev_data->RxRingSize++;                                /* We need at least one buffer to run DMA.              */

        pdesc->DES0 = pdesc->SoftAddr;
        pdesc->DES1 = 0u;
        pdesc->DES2 = 0u;
        pdesc->DES3 = RX_DESC_DES3_OWN   |
                      RX_DESC_DES3_BUF1V |
                      RX_DESC_DES3_IOC;

        pdev_data->RxKeepAlive = DEF_TRUE;                      /* Drop new frames until we get valid buffers.          */
    } else {
        pdev_data->RxKeepAlive = DEF_FALSE;                     /* We got fresh buffers. Don't drop new frames.         */
    }

                                                                /* Start Rx.                                            */
    pdev->DMACRDLAR = (CPU_INT32U) pdev_data->RxRingHead;
    pdev->DMACRDRLR =              pdev_data->RxRingSize;
    pdev->DMACRDTPR = (CPU_INT32U)(pdev_data->RxRingHead + pdev_data->RxRingSize);
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

    pdesc                  =              pdev_data->TxRingHead;
    pdesc->DES0            = (CPU_INT32U) p_data;
    pdesc->DES1            =              0u;
    pdesc->DES2            =             (size & TX_DESC_DES2_LEN1_MASK);
    pdesc->DES3            =             (TX_DESC_DES3_OWN | TX_DESC_DES3_FD | TX_DESC_DES3_LD | TX_DESC_DES3_CIC);
    pdesc->SoftAddr        = (CPU_INT32U) p_data;

    pdev->DMACTDLAR        = (CPU_INT32U) pdev_data->TxRingHead;
    pdev->DMACTDRLR        =              1u;
    pdev->DMACTDTPR        = (CPU_INT32U)(pdev_data->TxRingHead + 1u);

    pdev->DMACTCR         |=             (32u << 16u) | DMACTCR_ST; /* Start Tx DMA Engine.                             */

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
    NET_BUF_QTY         i;
    CPU_INT32U          nbytes;
    LIB_ERR             lib_err;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;


                                                                /* ---------------- ALLOCATE RX MEMORY ---------------- */
    nbytes                  =             pdev_data->RxDescNbr * sizeof(DEV_DESC);
    pdev_data->RxRingHead   = (DEV_DESC *)Mem_PoolBlkGet(&pdev_data->RxDescPool,
                                                          nbytes,
                                                         &lib_err);
    pdev_data->RxRingRxd    =             pdev_data->RxRingHead;
    pdev_data->RxRingRxdCnt =             0u;

                                                                /* ----------------- INIT DESCRIPTORS ----------------- */
    pdesc = pdev_data->RxRingHead;
    for (i = 0u; i < pdev_data->RxDescNbr; i++) {               /* Init Rx descriptor ring.                             */
        pdesc->SoftAddr = (CPU_INT32U)NetBuf_GetDataPtr(pif,
                                                        NET_TRANSACTION_RX,
                                                        NET_IF_ETHER_FRAME_MAX_SIZE,
                                                        NET_IF_IX_RX,
                                                        0u,
                                                        0u,
                                                        0u,
                                                        p_err);
        if (*p_err != NET_BUF_ERR_NONE) {
            break;
        }

        pdesc->DES0 = pdesc->SoftAddr;
        pdesc->DES1 = 0u;
        pdesc->DES2 = 0u;
        pdesc->DES3 = RX_DESC_DES3_OWN   |                      /* DMA owns the desc.                                   */
                      RX_DESC_DES3_BUF1V |                      /* Buffer 1 address is valid.                           */
                      RX_DESC_DES3_IOC;                         /* Interrupt on reception.                              */
        pdesc++;
    }

    pdev_data->RxRingSize = i;

    if (pdev_data->RxRingSize == 0) {                           /* We must have at least one buffer to operate.         */
        return;                                                 /* Only return the NET_BUF error for this case.         */
    }

    pdev->DMACRDLAR = (CPU_INT32U) pdev_data->RxRingHead;
    pdev->DMACRDRLR =              pdev_data->RxRingSize;
    pdev->DMACRDTPR = (CPU_INT32U)(pdev_data->RxRingHead + pdev_data->RxRingSize);

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
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : (1) Rx interrupts should be stopped when this function is called.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *p_err)
{
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    LIB_ERR        lib_err;

                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;

                                                                /* --------------- FLUSH THE DMA ENGINE --------------- */
    if (pdev_data->RxKeepAlive != DEF_YES) {                    /* No buffers to free if we're in the keep-alive state. */
        pdesc = pdev_data->RxRingHead;
        while (pdev_data->RxRingSize > 0u) {
            NetBuf_FreeBufDataAreaRx(pif->Nbr, (CPU_INT08U *)pdesc->SoftAddr);
            pdesc++;
            pdev_data->RxRingSize--;
        }
    }

                                                                /* -------------- FREE RX MEMORY BLOCKS --------------- */
    Mem_PoolBlkFree(&pdev_data->RxDescPool,
                     pdev_data->RxRingHead,
                    &lib_err);

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
    nbytes                =             pdev_data->TxDescNbr * sizeof(DEV_DESC);
    pdev_data->TxRingHead = (DEV_DESC *)Mem_PoolBlkGet(&pdev_data->TxDescPool,
                                                        nbytes,
                                                       &lib_err);

    nbytes              =              pdev_data->TxDescNbr * sizeof(DEV_BUF_Q);
    pdev_data->TxQFront =              0u;
    pdev_data->TxQBack  =              0u;
    pdev_data->TxQSize  =              0u;
    pdev_data->TxQ      = (DEV_BUF_Q *)Mem_PoolBlkGet(&pdev_data->TxQPool,
                                                       nbytes,
                                                      &lib_err);

                                                                /* ----------------- INIT DESCRIPTORS ----------------- */
    pdesc = pdev_data->TxRingHead;
    for (i = 0u; i < pdev_cfg->TxDescNbr; i++) {                /* Init Tx descriptor ring.                             */
        pdesc->DES0     = 0u;
        pdesc->DES1     = 0u;
        pdesc->DES2     = 0u;
        pdesc->DES3     = 0u;
        pdesc->SoftAddr = 0u;

        pdesc++;
    }

    pdev_data->TxRingSize =             0u;                     /* Ring is empty until we have something to transmit.   */
    pdev->DMACTDLAR       = (CPU_INT32U)pdev_data->TxRingHead;
    pdev->DMACTDRLR       =             0u;
    pdev->DMACTDTPR       = (CPU_INT32U)pdev_data->TxRingHead;

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
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->SoftAddr, &err);
        pdesc++;
        pdev_data->TxRingSize--;
    }

                                                                /* -------------- FREE TX MEMORY BLOCKS --------------- */
    Mem_PoolBlkFree(&pdev_data->TxDescPool,
                     pdev_data->TxRingHead,
                    &lib_err);

    Mem_PoolBlkFree(&pdev_data->TxQPool,
                     pdev_data->TxQ,
                    &lib_err);

   *p_err = NET_DEV_ERR_NONE;
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
                                       NET_ERR     *p_err)
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


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;


    crc = NetUtil_32BitCRC_CalcCpl(            paddr_hw,        /* Obtain CRC without compliment.                       */
                                   (CPU_INT32U)addr_hw_len,
                                               p_err);

    if (*p_err != NET_UTIL_ERR_NONE) {
        return;
    }

    crc     = NetUtil_32BitReflect(crc);                        /* Reflect CRC to produce equivalent hardware result.   */
    hash    = (crc >> 26u) & 0x3Fu;                             /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26u) & 0x1Fu;                             /* Obtain least 5 significant bits for for reg bit sel. */
    reg_sel = (crc >> 31u) & 0x01u;                             /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    if (reg_sel == 0u) {                                        /* Set multicast hash reg bit.                          */
        pdev->MACHT0R |= (1u << bit_sel);
    } else {
        pdev->MACHT1R |= (1u << bit_sel);
    }

#else
   (void)pif;                                                   /* Prevent 'variable unused' compiler warnings.         */
   (void)paddr_hw;
   (void)addr_hw_len;
#endif

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
                                          NET_ERR     *p_err)
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


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;

    crc = NetUtil_32BitCRC_CalcCpl(            paddr_hw,        /* Obtain CRC without compliment.                       */
                                   (CPU_INT32U)addr_hw_len,
                                               p_err);

    if (*p_err != NET_UTIL_ERR_NONE) {
        return;
    }

    crc     = NetUtil_32BitReflect(crc);                        /* Reflect CRC to produce equivalent hardware result.   */
    hash    = (crc >> 26u) & 0x3Fu;                             /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26u) & 0x1Fu;                             /* Obtain least 5 significant bits for for reg bit sel. */
    reg_sel = (crc >> 31u) & 0x01u;                             /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1u) {                                /* If hash bit reference ctr not zero.                  */
      (*paddr_hash_ctrs)--;                                     /* Decrement hash bit reference ctr.                    */
       *p_err = NET_DEV_ERR_NONE;
        return;
    }

   *paddr_hash_ctrs = 0u;                                       /* Zero hash bit references remaining.                  */

    if (reg_sel == 0u) {                                        /* Clr multicast hash reg bit.                          */
        pdev->MACHT0R &= ~(1u << bit_sel);
    } else {
        pdev->MACHT1R &= ~(1u << bit_sel);
    }

#else
   (void)pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)paddr_hw;
   (void)addr_hw_len;
#endif

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
    DEV_BUF_Q           tx_buf_q;
    NET_ERR             err;
    NET_DRV_ERR         drv_err;
    CPU_INT32U          int_status;


   (void)type;                                                  /* Prevent compiler warning for arg type unused.        */
                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev         = (NET_DEV           *)pdev_cfg->BaseAddr;

    int_status   = pdev->DMACSR;
    pdev->DMACSR = int_status;                                  /* Clr active int. src's.                               */

                                                                /* ==================================================== */
                                                                /* ================== HANDLE TX INTS ================== */
                                                                /* ==================================================== */
    if (int_status & DMACSR_TBU) {
        pdesc = pdev_data->TxRingHead;

        while (pdev_data->TxRingSize > 0u) {                    /* Signal Net IF that Tx resources are available.       */
            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->SoftAddr, &err);
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
            pdesc->DES0     = (CPU_INT32U) tx_buf_q.DataPtr;
            pdesc->DES1     =              0u;
            pdesc->DES2     =             (tx_buf_q.Size & TX_DESC_DES2_LEN1_MASK);
            pdesc->DES3     =             (TX_DESC_DES3_OWN | TX_DESC_DES3_FD | TX_DESC_DES3_LD | TX_DESC_DES3_CIC);
            pdesc->SoftAddr = (CPU_INT32U) tx_buf_q.DataPtr;

            pdev_data->TxRingSize++;
            pdesc++;
        }

        if (pdev_data->TxRingSize == 0u) {
            pdev_data->TxBusy  =  DEF_FALSE;                    /* Tx is idle until we have something to transmit.      */
            pdev->DMACTCR     &= ~DMACTCR_ST;                   /* Stop the Tx DMA engine.                              */
        } else {
            pdev->DMACTDLAR = (CPU_INT32U) pdev_data->TxRingHead;
            pdev->DMACTDRLR =              pdev_data->TxRingSize;
            pdev->DMACTDTPR = (CPU_INT32U)(pdev_data->TxRingHead + pdev_data->TxRingSize);
        }
    }
                                                                /* ==================================================== */
                                                                /* ================== HANDLE RX INTS ================== */
                                                                /* ==================================================== */
    if (int_status & DMACSR_RI) {
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF Rx Task for each buffer we receive.    */
        pdev_data->RxRingRxdCnt++;                              /* Track the number of signals we've sent.              */
    }

    if (int_status & DMACSR_RBU) {                              /* Finished receiving packets.                          */
                                                                /* Ensure no signals have been missed.                  */
        while (pdev_data->RxRingRxdCnt < pdev_data->RxRingSize) {
            NetIF_RxTaskSignal(pif->Nbr, &err);
            pdev_data->RxRingRxdCnt++;
        }
        pdev_data->RxBusy = DEF_FALSE;
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
    NET_DEV_CFG_ETHER   *pdev_cfg;
    NET_DEV_LINK_ETHER  *plink_state;
    NET_PHY_API_ETHER   *pphy_api;
    NET_DEV             *pdev;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;

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

             switch (plink_state->Duplex) {                     /* Update duplex setting on device.                     */
                case NET_PHY_DUPLEX_FULL:
                     pdev->MACCR |= MACCR_DM;                   /* Enable Full Duplex mode.                             */
                     break;


                case NET_PHY_DUPLEX_HALF:
                     pdev->MACCR &= ~(MACCR_DM);                /* Enable Half Duplex mode.                             */
                     break;


                default:
                    *p_err = NET_PHY_ERR_INVALID_CFG;
                     return;
             }

             switch (plink_state->Spd) {                        /* Update speed setting on device.                      */
                case NET_PHY_SPD_10:
                     pdev->MACCR &= ~(MACCR_FES);
                     break;


                case NET_PHY_SPD_100:
                     pdev->MACCR |= MACCR_FES;
                     break;


                default:
                    *p_err = NET_PHY_ERR_INVALID_CFG;
                     return;
             }

            *p_err = NET_DEV_ERR_NONE;
             break;


        default:
            *p_err = NET_IF_ERR_INVALID_IO_CTRL_OPT;
             break;
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
*               p_err    Pointer to return error code.
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
    CPU_INT32U          tmpreg;
    CPU_INT32U          timeout;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;


    tmpreg  =   pdev->MACMDIOAR;
    tmpreg &= ~(MACMDIOAR_PA_MASK  |
                MACMDIOAR_RDA_MASK |
                MACMDIOAR_GOC_MASK);
    tmpreg |= ((phy_addr & 0x1Fu) << 21u) |
              ((reg_addr & 0x1Fu) << 16u) |
                MACMDIOAR_GOC_RD          |
                MACMDIOAR_MB;

    timeout         = 0u;
    pdev->MACMDIOAR = tmpreg;
    do {                                                        /* Check for the Busy flag.                            */
        timeout++;
        tmpreg = pdev->MACMDIOAR;
    } while ((tmpreg & MACMDIOAR_MB) && (timeout < (CPU_INT32U)PHY_TIMEOUT));

    if (timeout == PHY_TIMEOUT) {                               /* Return ERROR in case of timeout.                    */
       *p_err = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    *p_data = (CPU_INT16U)(pdev->MACMDIODR & 0xFFFFu);          /* Return data reg val.                                */

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
    CPU_INT32U          tmpreg;


                                                                /* ----  OBTAIN REFERENCES FOR THE DEVICE INSTANCE ---- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;

    tmpreg  =   pdev->MACMDIOAR;
    tmpreg &= ~(MACMDIOAR_PA_MASK  |
                MACMDIOAR_RDA_MASK |
                MACMDIOAR_GOC_MASK);
    tmpreg |= ((phy_addr & 0x1Fu) << 21u) |
              ((reg_addr & 0x1Fu) << 16u) |
                MACMDIOAR_GOC_WR          |
                MACMDIOAR_MB;

    timeout         = 0u;
    pdev->MACMDIODR = data;
    pdev->MACMDIOAR = tmpreg;
    do {                                                        /* Check for the Busy flag.                            */
        timeout++;
        tmpreg = pdev->MACMDIOAR;
    } while ((tmpreg & MACMDIOAR_MB) && (timeout < (CPU_INT32U)PHY_TIMEOUT));

    if (timeout == PHY_TIMEOUT) {                               /* Return ERROR in case of timeout.                    */
       *p_err = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
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

