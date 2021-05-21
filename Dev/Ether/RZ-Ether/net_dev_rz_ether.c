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
*                                           Renesas RZ-Ether
*
* Filename : net_dev_rz_ether.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_RZ_ETHER_MODULE
#include  "net_dev_rz_ether.h"
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>


#include  <KAL/kal.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>


#include  <bsp_int.h>
#include  <app_cfg.h>
#include  <bsp_cache.h>


#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  PHY_READ                                          2
#define  PHY_WRITE                                         1
#define  PHY_WRITE_TA                                      2
#define  MII_DATA                                 0x40800000
#define  BIT_MASK                                 0x00000001
#define  MDC_WAIT                                          6
#define  PHY_ADDR                                          0
#define  PHY_ST                                            1
#define  TSU_ADR_LEN                                      32


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) Instance specific data area structures should be defined below.  The data area
*               structure typically includes error counters and variables used to track the
*               state of the device.  Variables required for correct operation of the device
*               MUST NOT be defined globally and should instead be included within the instance
*               specific data area structure and referenced as p_if->Dev_Data structure members.
*
*           (2) DMA based devices may require more than one type of descriptor.  Each descriptor
*               type should be defined below.  An example descriptor has been provided.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          DESCRIPTOR DATA TYPE
*********************************************************************************************************
*/

typedef  struct  dev_desc  DEV_DESC;

struct  dev_desc {
    CPU_REG32    status;                                        /* DMA status register.                                 */
    CPU_REG32    sizebuf;                                       /* DMA buffer size.                                     */
    CPU_INT08U  *p_buf;                                         /* DMA buffer pointer                                   */
    DEV_DESC    *p_next;                                        /* DMA next descriptor pointer.                         */
    CPU_REG32    pad[4];
};


/*
*********************************************************************************************************
*                                        BUFFER LIST DATA TYPE
*********************************************************************************************************
*/

typedef  struct  list  LIST;

struct  list {
    void        *p_buf;
    CPU_INT16U   len;
    LIST        *p_next;
};


/*
*********************************************************************************************************
*                                        DEVICE INSTANCE DATA
*
*           (1) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*/

typedef  struct  net_dev_data {
    CPU_INT32U   MultiRxCtr;
    CPU_INT32U   MultiRxMax;
    CPU_INT16U   RxNRdyCtr;
    CPU_INT16U   TxNRdyCtr;

    DEV_DESC    *RxBufDescPtrCur;
    DEV_DESC    *RxBufDescPtrStart;
    DEV_DESC    *RxBufDescPtrEnd;

    DEV_DESC    *TxBufDescPtrCur;
    DEV_DESC    *TxBufDescPtrStart;
    DEV_DESC    *TxBufDescPtrEnd;
    DEV_DESC    *TxBufDescCompPtr;                              /* See Note #1.                                         */

    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;

    LIST        *RxReadyListPtr;
    LIST        *RxBufferListPtr;
    LIST        *RxFreeListPtr;
} NET_DEV_DATA;


/*
*********************************************************************************************************
*                                       DEVICE CAM ENTRY DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_dev_tsu {
    CPU_REG32  ADDRH;
    CPU_REG32  ADDRL;
} NET_DEV_TSU;

typedef  struct  net_dev_tsu_addr {
    NET_DEV_TSU  Addr[TSU_ADR_LEN];
} NET_DEV_TSU_ADDR;


/*
*********************************************************************************************************
*                                        DEVICE REGISTER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32         EDSR;                                     /* E-DMAC start register.                               */
    CPU_REG32         RESERVED0[3];
    CPU_REG32         TDLAR;                                    /* Transmit descriptor list start address register.     */
    CPU_REG32         TDFAR;                                    /* Transmit descriptor fetch address register.          */
    CPU_REG32         TDFXR;                                    /* Transmit descriptor finished address register.       */
    CPU_REG32         TDFFR;                                    /* Transmit descriptor final flag register.             */
    CPU_REG32         RESERVED1[4];
    CPU_REG32         RDLAR;                                    /* Receive descriptor list start address register.      */
    CPU_REG32         RDFAR;                                    /* Receive descriptor fetch address register.           */
    CPU_REG32         RDFXR;                                    /* Receive descriptor finished address register.        */
    CPU_REG32         RDFFR;                                    /* Receive descriptor final flag register.              */
    CPU_REG32         RESERVED2[240];
    CPU_REG32         EDMR;                                     /* E-DMAC mode register.                                */
    CPU_REG32         RESERVED3[1];
    CPU_REG32         EDTRR;                                    /* E-DMAC transmit request register.                    */
    CPU_REG32         RESERVED4[1];
    CPU_REG32         EDRRR;                                    /* E-DMAC receive request register.                     */
    CPU_REG32         RESERVED5[5];
    CPU_REG32         EESR;                                     /* E-MAC/E-DMAC status register.                        */
    CPU_REG32         RESERVED6[1];
    CPU_REG32         EESIPR;                                   /* E-MAC/E-DMAC status interrupt permission register.   */
    CPU_REG32         RESERVED7[1];
    CPU_REG32         TRSCER;                                   /* Transmit/receive status copy enable register.        */
    CPU_REG32         RESERVED8[1];
    CPU_REG32         RMFCR;                                    /* Receive missed-frame counter register.               */
    CPU_REG32         RESERVED9[1];
    CPU_REG32         TFTR;                                     /* Transmit FIFO threshold register.                    */
    CPU_REG32         RESERVED10[1];
    CPU_REG32         FDR;                                      /* FIFO depth register.                                 */
    CPU_REG32         RESERVED11[1];
    CPU_REG32         RMCR;                                     /* Receiving method control register.                   */
    CPU_REG32         RESERVED12[1];
    CPU_REG32         RPADIR;                                   /* Receive data padding insert register.                */
    CPU_REG32         RESERVED13[1];
    CPU_REG32         FCFTR;                                    /* Overflow alert FIFO threshold register.              */
    CPU_REG32         RESERVED14[30];
    CPU_REG32         CSMR;                                     /* Intelligent checksum mode register.                  */
    CPU_REG32         CSSBM;                                    /* Intelligent checksum skipped bytes monitor register. */
    CPU_REG32         CSSMR;                                    /* Intelligent checksum monitor register.               */
    CPU_REG32         RESERVED15[4];
    CPU_REG32         ECMR;                                     /* E-MAC mode register.                                 */
    CPU_REG32         RESERVED16[1];
    CPU_REG32         RFLR;                                     /* Receive frame length register.                       */
    CPU_REG32         RESERVED17[1];
    CPU_REG32         ECSR;                                     /* E-MAC status register.                               */
    CPU_REG32         RESERVED18[1];
    CPU_REG32         ECSIPR;                                   /* E-MAC interrupt permission register.                 */
    CPU_REG32         RESERVED19[1];
    CPU_REG32         PIR;                                      /* PHY interface register.                              */
    CPU_REG32         RESERVED20[12];
    CPU_REG32         APR;                                      /* Automatic PAUSE frame register.                      */
    CPU_REG32         MPR;                                      /* Manual PAUSE frame register.                         */
    CPU_REG32         PFTCR;                                    /* PAUSE frame transmit counter register.               */
    CPU_REG32         PFRCR;                                    /* PAUSE frame receive counter register.                */
    CPU_REG32         TPAUSER;                                  /* Automatic PAUSE frame retransmit count register.     */
    CPU_REG32         RESERVED21[22];
    CPU_REG32         MAHR;                                     /* MAC address high register.                           */
    CPU_REG32         RESERVED22[1];
    CPU_REG32         MALR;                                     /* MAC address low register.                            */
    CPU_REG32         RESERVED23[77];
    CPU_REG32         TROCR;                                    /* Transmit retry over counter register.                */
    CPU_REG32         RESERVED24[1];
    CPU_REG32         CDCR;                                     /* Delayed collision detect counter register.           */
    CPU_REG32         RESERVED25[1];
    CPU_REG32         LCCR;                                     /* Lost carrier counter register.                       */
    CPU_REG32         RESERVED26[11];
    CPU_REG32         CEFCR;                                    /* CRC error frame receive counter register.            */
    CPU_REG32         RESERVED27[1];
    CPU_REG32         FRECR;                                    /* Frame receive error counter register.                */
    CPU_REG32         RESERVED28[1];
    CPU_REG32         TSFRCR;                                   /* Too-short frame receive counter register.            */
    CPU_REG32         RESERVED29[1];
    CPU_REG32         TLFRCR;                                   /* Too-long frame receive counter register.             */
    CPU_REG32         RESERVED30[1];
    CPU_REG32         RFCR;                                     /* Residual-bit frame receive counter register.         */
    CPU_REG32         RESERVED31[1];
    CPU_REG32         CERCR;                                    /* Carrier extension loss counter register.             */
    CPU_REG32         RESERVED32[1];
    CPU_REG32         CEECR;                                    /* Carrier extension error counter register.            */
    CPU_REG32         RESERVED33[1];
    CPU_REG32         MAFCR;                                    /* Multicast address frame receive counter register.    */
    CPU_REG32         RESERVED34[1057];
    CPU_REG32         ARSTR;                                    /* Software reset register.                             */
    CPU_REG32         TSU_CTRST;                                /* TSU counter reset register.                          */
    CPU_REG32         RESERVED35[20];
    CPU_REG32         TSU_VTAG;                                 /* VLANtag set register.                                */
    CPU_REG32         RESERVED36[1];
    CPU_REG32         TSU_ADSBSY;                               /* CAM entry table busy register.                       */
    CPU_REG32         TSU_TEN;                                  /* CAM entry table enable register.                     */
    CPU_REG32         RESERVED37[6];
    CPU_REG32         TXNLCR;                                   /* TX frame counter register (normal TX only).          */
    CPU_REG32         TXALCR;                                   /* TX frame counter register (normal and erroneous TX). */
    CPU_REG32         RXNLCR;                                   /* RX frame counter register (normal RX only).          */
    CPU_REG32         RXALCR;                                   /* RX frame counter register (normal and erroneous RX). */
    CPU_REG32         RESERVED38[28];
    NET_DEV_TSU_ADDR  TSU_ADDR;                                 /* CAM entry table registers.                           */
} NET_DEV;


/*
*********************************************************************************************************
*                                                 MACROS
*********************************************************************************************************
*/

#define  NET_BUF_DESC_PTR_NEXT(ptr)         (ptr = ptr->p_next)


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* ------------ FNCT'S COMMON TO ALL DRV'S ------------ */
static  void         NetDev_Init               (NET_IF             *p_if,
                                                NET_ERR            *p_err);

static  void         NetDev_Start              (NET_IF             *p_if,
                                                NET_ERR            *p_err);


static  void         NetDev_Stop               (NET_IF             *p_if,
                                                NET_ERR            *p_err);

static  void         NetDev_Rx                 (NET_IF             *p_if,
                                                CPU_INT08U        **p_data,
                                                CPU_INT16U         *p_size,
                                                NET_ERR            *p_err);

static  void         NetDev_Tx                 (NET_IF             *p_if,
                                                CPU_INT08U         *p_data,
                                                CPU_INT16U          size,
                                                NET_ERR            *p_err);

static  void         NetDev_AddrMulticastAdd   (NET_IF             *p_if,
                                                CPU_INT08U         *p_addr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *p_err);

static  void         NetDev_AddrMulticastRemove(NET_IF             *p_if,
                                                CPU_INT08U         *p_addr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *p_err);

static  void         NetDev_ISR_Handler        (NET_IF             *p_if,
                                                NET_DEV_ISR_TYPE    type);

static  void         NetDev_IO_Ctrl            (NET_IF             *p_if,
                                                CPU_INT08U          opt,
                                                void               *p_data,
                                                NET_ERR            *p_err);

static  void         NetDev_MII_Rd             (NET_IF             *p_if,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *p_err);

static  void         NetDev_MII_Wr             (NET_IF             *p_if,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *p_err);

                                                                /* ------- FNCT'S COMMON TO DMA BASED DRV'S ----------- */
static  void         NetDev_RxDescInit         (NET_IF             *p_if,
                                                NET_ERR            *p_err);

static  void         NetDev_RxDescFreeAll      (NET_IF             *p_if,
                                                NET_ERR            *p_err);

static  void         NetDev_TxDescInit         (NET_IF             *p_if,
                                                NET_ERR            *p_err);

static  CPU_BOOLEAN  NetDev_ISR_Rx             (NET_IF             *p_if,
                                                DEV_DESC           *p_desc);

                                                                /* ----------------- MII CTRL FNCT'S ------------------ */
static  void         NetDev_MII_Preamble       (NET_IF             *p_if);

static  void         NetDev_MII_Cmd            (NET_IF             *p_if,
                                                CPU_INT16U          reg_addr,
                                                CPU_INT32S          option);

static  void         NetDev_MII_TurnAround     (NET_IF             *p_if);

static  void         NetDev_MII_RegRead        (NET_IF             *p_if,
                                                CPU_INT16U         *p_data);

static  void         NetDev_MII_RegWrite       (NET_IF             *p_if,
                                                CPU_INT16U          data);

static  void         NetDev_MII_Write1         (NET_IF             *p_if);

static  void         NetDev_MII_Write0         (NET_IF             *p_if);

static  void         NetDev_MII_Z              (NET_IF             *p_if);


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      NETWORK DEVICE DRIVER API
*
* Note(s) : (1) Device driver API structures are used by the application during the call to NetIF_Add().
*               This API structure allows higher layers to call specific device driver functions via
*               function pointer instead of by name.  This enables the network protocol suite to compile
*               & operate with multiple device drivers.
*
*           (2) Device driver function names may be arbitrarily chosen.  However, it is recommended that
*               device functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_???.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*
*           (3) In most cases, the API structure provided below SHOULD suffice for most device drivers
*               exactly as is with the exception that the API structure's name which MUST be unique &
*               SHOULD clearly identify the device being implemented.  For example, the Cirrus Logic
*               CS8900A Ethernet controller's API structure should be named NetDev_API_CS8900A[].
*
*               The API structure MUST also be externally declared in the device driver header file
*               ('net_dev_???.h') with the exact same name & type.
*********************************************************************************************************
*/
                                                                                /* RZ-Ether dev API fnct ptrs :         */
const  NET_DEV_API_ETHER  NetDev_API_RZ_Ether = {
                                                  NetDev_Init,                  /*   Init/add                           */
                                                  NetDev_Start,                 /*   Start                              */
                                                  NetDev_Stop,                  /*   Stop                               */
                                                  NetDev_Rx,                    /*   Rx                                 */
                                                  NetDev_Tx,                    /*   Tx                                 */
                                                  NetDev_AddrMulticastAdd,      /*   Multicast addr add                 */
                                                  NetDev_AddrMulticastRemove,   /*   Multicast addr remove              */
                                                  NetDev_ISR_Handler,           /*   ISR handler                        */
                                                  NetDev_IO_Ctrl,               /*   I/O ctrl                           */
                                                  NetDev_MII_Rd,                /*   Phy reg rd                         */
                                                  NetDev_MII_Wr                 /*   Phy reg wr                         */
                                                };


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

#define  RX_BUF_ALIGNMENT                                 32    /* Rx buff MUST be aligned to a 32 byte boundary.       */
#define  EDMAC_RESET                              DEF_BIT_00
#define  ECMR_DUPLEX_FULL                         DEF_BIT_01
#define  RFLR_MAX_PKT_LEN                              0x5F0
#define  IPGR_IPG_100_BIT                               0x14
#define  ECMR_TX_EN                               DEF_BIT_05
#define  ECMR_RX_EN                               DEF_BIT_06
#define  FCFTR_8_FRM                              0x00070000L
#define  FCFTR_24_FRM                             0x00170000L

#define  EMDR_LITTLE_ENDIAN                       DEF_BIT_06

#define  EDSR_ENT                                 DEF_BIT_01
#define  EDSR_ENR                                 DEF_BIT_00

#define  ECMR_TRCCM                               DEF_BIT_26
#define  ECMR_RCSC                                DEF_BIT_23
#define  ECMR_DPAD                                DEF_BIT_21
#define  ECMR_RZPF                                DEF_BIT_20
#define  ECMR_ZPF                                 DEF_BIT_19
#define  ECMR_PFR                                 DEF_BIT_18
#define  ECMR_RXF                                 DEF_BIT_17
#define  ECMR_TXF                                 DEF_BIT_16
#define  ECMR_MCT                                 DEF_BIT_13
#define  ECMR_MPDE                                DEF_BIT_09
#define  ECMR_RE                                  DEF_BIT_06
#define  ECMR_TE                                  DEF_BIT_05
#define  ECMR_DM                                  DEF_BIT_01
#define  ECMR_PRM                                 DEF_BIT_00

#define  FDR_RX_FIFO_256                          DEF_BIT_NONE
#define  FDR_RX_FIFO_512                          DEF_BIT_00
#define  FDR_RX_FIFO_768                          DEF_BIT_01
#define  FDR_RX_FIFO_1024                        (DEF_BIT_01 | DEF_BIT_00)
#define  FDR_RX_FIFO_1280                         DEF_BIT_02
#define  FDR_RX_FIFO_1536                        (DEF_BIT_02 | DEF_BIT_00)
#define  FDR_RX_FIFO_1792                        (DEF_BIT_02 | DEF_BIT_01)
#define  FDR_RX_FIFO_2048                        (DEF_BIT_02 | DEF_BIT_01 | DEF_BIT_00)
#define  FDR_RX_FIFO_2304                         DEF_BIT_03
#define  FDR_RX_FIFO_2560                        (DEF_BIT_03 | DEF_BIT_00)
#define  FDR_RX_FIFO_2816                        (DEF_BIT_03 | DEF_BIT_01)
#define  FDR_RX_FIFO_3072                        (DEF_BIT_03 | DEF_BIT_01 | DEF_BIT_00)
#define  FDR_RX_FIFO_3328                        (DEF_BIT_03 | DEF_BIT_02)
#define  FDR_RX_FIFO_3584                        (DEF_BIT_03 | DEF_BIT_02 | DEF_BIT_00)
#define  FDR_RX_FIFO_3840                        (DEF_BIT_03 | DEF_BIT_02 | DEF_BIT_01)
#define  FDR_RX_FIFO_4096                        (DEF_BIT_03 | DEF_BIT_02 | DEF_BIT_01 | DEF_BIT_00)

#define  FDR_TX_FIFO_256                          DEF_BIT_NONE
#define  FDR_TX_FIFO_512                          DEF_BIT_08
#define  FDR_TX_FIFO_768                          DEF_BIT_09
#define  FDR_TX_FIFO_1024                        (DEF_BIT_09 | DEF_BIT_08)
#define  FDR_TX_FIFO_1280                         DEF_BIT_10
#define  FDR_TX_FIFO_1536                        (DEF_BIT_10 | DEF_BIT_08)
#define  FDR_TX_FIFO_1792                        (DEF_BIT_10 | DEF_BIT_09)
#define  FDR_TX_FIFO_2048                        (DEF_BIT_10 | DEF_BIT_09 | DEF_BIT_08)

#define  RMCR_CONT_RX                             DEF_BIT_00

#define  EES_TWB1                                 DEF_BIT_31
#define  EES_TWB0                                 DEF_BIT_30
#define  EES_TC1                                  DEF_BIT_29
#define  EES_TUC                                  DEF_BIT_28
#define  EES_ROC                                  DEF_BIT_27
#define  EES_TABT                                 DEF_BIT_26
#define  EES_RABT                                 DEF_BIT_25
#define  EES_RFCOF                                DEF_BIT_24
#define  EES_ECI                                  DEF_BIT_22
#define  EES_TC0                                  DEF_BIT_21
#define  EES_TDE                                  DEF_BIT_20
#define  EES_TFUF                                 DEF_BIT_19
#define  EES_FR                                   DEF_BIT_18
#define  EES_RDE                                  DEF_BIT_17
#define  EES_RFOF                                 DEF_BIT_16
#define  EES_DLC                                  DEF_BIT_10
#define  EES_CD                                   DEF_BIT_09
#define  EES_TRO                                  DEF_BIT_08
#define  EES_RMAF                                 DEF_BIT_07
#define  EES_CEEF                                 DEF_BIT_06
#define  EES_CELF                                 DEF_BIT_05
#define  EES_RRF                                  DEF_BIT_04
#define  EES_RTLF                                 DEF_BIT_03
#define  EES_RTSF                                 DEF_BIT_02
#define  EES_PRE                                  DEF_BIT_01
#define  EES_CERF                                 DEF_BIT_00

#define  EES_TC                                  (EES_TC1 | EES_TC0)

#define  EES_RX_ERR                              (EES_ROC   | \
                                                  EES_RABT  | \
                                                  EES_RFCOF | \
                                                  EES_RFOF  | \
                                                  EES_RRF   | \
                                                  EES_RTLF  | \
                                                  EES_RTSF  | \
                                                  EES_PRE   | \
                                                  EES_CERF)

#define  EDMR_DE                                  DEF_BIT_06
#define  EDMR_DL1                                 DEF_BIT_05
#define  EDMR_DL0                                 DEF_BIT_04
#define  EDMR_SWRT                                DEF_BIT_01
#define  EDMR_SWRR                                DEF_BIT_00


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/

#define  ACT                                      0x80000000L
#define  DLE                                      0x40000000L
#define  FP1                                      0x20000000L
#define  FP0                                      0x10000000L
#define  FE                                       0x08000000L
#define  FEI                                      0x04000000L
#define  CSE                                      0x04000000L

#define  CLR                                      0x380003FFL
#define  ERR                                      0x080003FFL
#define  OVRFLOW                                  0x00000200L
#define  MULTI                                    0x00000080L


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
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'p_dev_api->Init()'.
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
*               (7) Drivers SHOULD validate device configuration values and set *p_err to
*                   NET_DEV_ERR_INVALID_CFG if unacceptible values have been specified. Fields
*                   of interest generally include, but are not limited to :
*
*                   (a) p_dev_cfg->RxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (b) p_dev_cfg->TxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (c) p_dev_cfg->RxBufAlignOctets
*                   (d) p_dev_cfg->TxBufAlignOctets
*                   (e) p_dev_cfg->RxBufDescNbr
*                   (f) p_dev_cfg->TxBufDescnbr
*
*               (8) Descriptors are typically required to be contiguous in memory.  Allocation of
*                   descriptors MUST occur as a single contigous block of memory.  The driver may
*                   use pointer arithmetic to sub-divide and traverse the descriptor list.
*
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
*                   then the driver MUST validate the p_if->Phy_Cfg pointer by checking for
*                   a NULL pointer BEFORE attempting to access members of the Phy
*                   configuration structure.  Phy configuration fields of interest generally
*                   include, but are  not limited to :
*
*                   (a) p_phy_cfg->Type :
*
*                       (1) NET_PHY_TYPE_INT            Phy integrated with MAC.
*                       (2) NET_PHY_TYPE_EXT            Phy externally attached to MAC.
*
*                   (b) p_phy_cfg->BusMode :
*
*                       (1) NET_PHY_BUS_MODE_MII        Phy bus mode configured to MII.
*                       (2) NET_PHY_BUS_MODE_RMII       Phy bus mode configured to RMII.
*                       (3) NET_PHY_BUS_MODE_SMII       Phy bus mode configured to SMII.
*
*                   (c) p_phy_cfg->Spd :
*
*                       (1) NET_PHY_SPD_0               Phy link speed unknown or NOT linked.
*                       (2) NET_PHY_SPD_10              Phy link speed configured to  10   mbit/s.
*                       (3) NET_PHY_SPD_100             Phy link speed configured to  100  mbit/s.
*                       (4) NET_PHY_SPD_1000            Phy link speed configured to  1000 mbit/s.
*                       (5) NET_PHY_SPD_AUTO            Phy link speed configured for auto-negotiation.
*
*                   (d) p_phy_cfg->Duplex :
*
*                       (1) NET_PHY_DUPLEX_UNKNOWN      Phy link duplex unknown or link not established.
*                       (2) NET_PHY_DUPLEX_HALF         Phy link duplex configured to  half duplex.
*                       (3) NET_PHY_DUPLEX_FULL         Phy link duplex configured to  full duplex.
*                       (4) NET_PHY_DUPLEX_AUTO         Phy link duplex configured for auto-negotiation.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NET_DEV_BSP_ETHER  *p_dev_bsp;
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_PHY_CFG_ETHER  *p_phy_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbytes;
    LIST               *p_list;
    CPU_INT16U          free_cnt;
    CPU_INT16U          ix;
    LIB_ERR             lib_err;


                                                                /* ------------ ALLOCATE DEV DATA AREA ---------------- */
    p_if->Dev_Data = Mem_HeapAlloc ((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                    (CPU_SIZE_T  ) 4,
                                    (CPU_SIZE_T *)&reqd_octets,
                                    (LIB_ERR    *)&lib_err);

    if (p_if->Dev_Data == (void *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */
    p_dev_bsp  = (NET_DEV_BSP_ETHER *)p_if->Dev_BSP;


                                                                /* ----------- ENABLE NECESSARY CLOCKS ---------------- */
                                                                /* Enable module clocks (see Note #3).                  */
    p_dev_bsp->CfgClk(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* ----- INITIALIZE EXTERNAL GPIO CONTROLLER ---------- */
                                                                /* Configure Ethernet Controller GPIO (see Note #5).    */
    p_dev_bsp->CfgGPIO(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    p_dev->ARSTR = EDMAC_RESET;                                 /* Reset EDMAC                                          */

                                                                /* Requires a dly of a min of 256 ext. Bck bus cycles.  */
    KAL_Dly(100u);


    p_dev->EDSR |= (EDSR_ENT |                                  /* Enable Rx and Tx                                     */
                    EDSR_ENR);

    p_dev->EDMR |= (EDMR_SWRT |                                 /* Reset RX & TX FIFO                                   */
                    EDMR_SWRR);

    while(p_dev->EDMR & (EDMR_SWRT |EDMR_SWRR)) {               /* Confirm cancellation of the software reset.          */
        ;                                                       /* NET_TODO: Provide a timeout.                         */
    }

    p_dev->EDSR &= ~(EDSR_ENT |                                 /* Disable Rx and Tx                                    */
                     EDSR_ENR);

    p_dev->TSU_TEN = 0u;                                        /* Disable every CAM entry.                             */

    for (ix = 0 ; ix < TSU_ADR_LEN ; ix++) {                    /* Clear each CAM entry.                                */
        p_dev->TSU_ADDR.Addr[ix].ADDRH = 0u;
        p_dev->TSU_ADDR.Addr[ix].ADDRL = 0u;
    }

    p_dev->ECMR |= ECMR_MCT;                                    /* Filter multicast frames to CAM content.              */

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
    BSP_IntSrcDis(RZ_INT_ID_ETH);                               /* NET_TODO: add BSP example to driver.                 */
    p_dev->EESIPR = 0x00000000L;                                /* Disable ALL dev interrupts.                          */
    p_dev->EESR   = 0xFFFFFFFFL;                                /* Clear all pending interrupts.                        */

                                                                /* Initialize ext. int. ctlr (see Note #4).             */
    p_dev_bsp->CfgIntCtrl(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* --------------- VALIDATE DRIVER CFG ---------------- */
                                                                /* See Note #8.                                         */
    if ((p_dev_cfg->RxBufAlignOctets % RX_BUF_ALIGNMENT) != 0) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if ((p_dev_cfg->RxBufLargeSize % RX_BUF_ALIGNMENT) != 0) {  /* Rx buf size MUST be divisible by 16.                 */
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* ----------- OBTAIN REFERENCE TO PHY CFG ------------ */
    p_phy_cfg = (NET_PHY_CFG_ETHER *)p_if->Ext_Cfg;             /* Obtain ptr to the phy cfg struct.                    */
    if (p_phy_cfg == (void *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;                         /* Call to NetIF_Start() does not specify Phy API.      */
        return;
    }

    if (p_phy_cfg->BusMode != NET_PHY_BUS_MODE_MII) {           /* Validate supported Phy bus types.                    */
        *p_err = NET_DEV_ERR_INVALID_CFG;
         return;
    }

    if (p_phy_cfg->Duplex == NET_PHY_DUPLEX_FULL) {             /* Set the device duplex.                               */
        p_dev->ECMR  =   ECMR_DM;
    } else {
        p_dev->ECMR &= ~(ECMR_DM);
    }

    p_dev->RFLR   =  p_dev_cfg->RxBufLargeSize;                 /* Set the maximum packet size                          */

    p_dev->FDR    =  (FDR_RX_FIFO_4096 |                        /* Configure the interrupt generation settings          */
                      FDR_TX_FIFO_2048);

    p_dev->RMCR   =   RMCR_CONT_RX;                             /* Set the controller for continuous receive            */
    p_dev->FCFTR  =   FCFTR_24_FRM;

    p_dev->EDMR  |=  (EDMR_DL0 | EDMR_DE);                      /* Set Frame Endianness to Little.                      */
    p_dev->EDMR  &= ~(EDMR_DL1);                                /* Set descriptor length to 32-bytes.                   */

    p_dev->TFTR   = (2048u >> 2);                               /* Set the FIFO threshold to the TX FIFO size.          */

                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbytes = p_dev_cfg->RxDescNbr * sizeof(DEV_DESC);           /* Determine block size.                                */

    Mem_PoolCreate ((MEM_POOL   *)&p_dev_data->RxDescPool,      /* Pass a pointer to the mem pool to create.            */
                    (void       *) DEF_NULL,                    /* From the onchip RAM.                                 */
                    (CPU_SIZE_T  ) 0,                           /* Allocated RAM size.                                  */
                    (CPU_SIZE_T  ) 1,                           /* Allocate 1 block.                                    */
                    (CPU_SIZE_T  ) nbytes,                      /* Block size large enough to hold all Rx descriptors.  */
                    (CPU_SIZE_T  ) 32,                          /* Block alignment (see Note #10).                      */
                    (CPU_SIZE_T *)&reqd_octets,                 /* Optional, ptr to variable to store rem nbr bytes.    */
                    (LIB_ERR    *)&lib_err);                    /* Ptr to variable to return an error code.             */

    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    nbytes = p_dev_cfg->TxDescNbr * sizeof(DEV_DESC);           /* Determine block size.                                */

    Mem_PoolCreate ((MEM_POOL   *)&p_dev_data->TxDescPool,      /* Pass a pointer to the mem pool to create.            */
                    (void       *) DEF_NULL,                    /* From the onchip RAM.                                 */
                    (CPU_SIZE_T  ) 0,                           /* Allocated RAM size.                                  */
                    (CPU_SIZE_T  ) 1,                           /* Allocate 1 block.                                    */
                    (CPU_SIZE_T  ) nbytes,                      /* Block size large enough to hold all Tx descriptors.  */
                    (CPU_SIZE_T  ) 32,                          /* Block alignment (see Note #10).                      */
                    (CPU_SIZE_T *)&reqd_octets,                 /* Optional, ptr to variable to store rem nbr bytes.    */
                    (LIB_ERR    *)&lib_err);                    /* Ptr to variable to return an error code.             */

    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    p_dev_data->RxReadyListPtr  = (LIST *)0;
    p_dev_data->RxBufferListPtr = (LIST *)0;
    p_dev_data->RxFreeListPtr   = (LIST *)0;

    free_cnt = p_dev_cfg->RxBufLargeNbr - p_dev_cfg->RxDescNbr;

    for (ix = 0; ix < free_cnt; ix++) {
        p_list = (LIST *)Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(LIST),
                                      (CPU_SIZE_T  ) 4,
                                      (CPU_SIZE_T *)&reqd_octets,
                                      (LIB_ERR    *)&lib_err);
        if (p_list == (LIST *)0) {
           *p_err = NET_DEV_ERR_MEM_ALLOC;
            return;
        }

        p_list->p_buf  = (void *)0;
        p_list->len    =  0;
        p_list->p_next =  p_dev_data->RxFreeListPtr;

        p_dev_data->RxFreeListPtr = p_list;
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
* Argument(s) : p_if         Pointer to a network interface.
*               ---         Argument validated in NetIF_Start().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Ethernet device successfully started.
*
*                                                               --- RETURNED BY NetIF_AddrHW_SetHandler() : ----
*                               NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT         Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               --- RETURNED BY NetIF_DevCfgTxRdySignal() : ---
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_OS_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
*                                                               ------ RETURNED BY NetDev_RxDescInit() : -------
*                               NET_DEV_ERR_NONE                No error
*                               NET_IF_MGR_ERR_nnnn             Various Network Interface management error codes
*                               NET_BUF_ERR_nnn                 Various Network buffer error codes
*
*                                                               ------ RETURNED BY NetDev_TxDescInit() : -------
*                               NET_DEV_ERR_NONE                No error
*                               NET_IF_MGR_ERR_nnnn             Various Network Interface management error codes
*                               NET_BUF_ERR_nnn                 Various Network buffer error codes
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'p_dev_api->Start()'.
*
* Note(s)     : (2) Many DMA devices may generate only one interrupt for several ready receive
*                   or transmit descriptors.  In order to accommodate this, it is recommended
*                   that all DMA based drivers count the number of ready receive and or transmit
*                   descriptors during the receive event and signal the receive or transmit
*                   deallocation task accordingly for those NEW descriptors which have not yet
*                   been accounted for.  Each time a descriptor is processed (or discarded) the
*                   count for acknowledged and unprocessed frames should be decremented by 1.
*                   This function initializes the acknowledged receive and transmit descriptor
*                   count to 0.
*
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
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *p_if,
                            NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */

    NetIF_DevCfgTxRdySignal(p_if,                               /* See Note #3.                                         */
                            p_dev_cfg->TxDescNbr,
                            p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #4 & #5.                                   */

    NetASCII_Str_to_MAC(p_dev_cfg->HW_AddrStr,                  /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #5a).                                  */
                       &err);

    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) p_if->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.    */
        hw_addr_cfg = DEF_YES;
    } else {                                                    /* Else get  app-configured IF layer HW MAC addr, ...   */
                                                                /* ... if any (see Note #5b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(p_if->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(p_if->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's auto. loaded ...        */
                                                                /* ... HW MAC address, if any (see Note #5c).           */
            hw_addr[0] = (p_dev->MAHR >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (p_dev->MAHR >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (p_dev->MAHR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (p_dev->MAHR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (p_dev->MALR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (p_dev->MALR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            NetIF_AddrHW_SetHandler((NET_IF_NBR  ) p_if->Nbr,   /* Configure IF layer to use automatically-loaded ...   */
                                    (CPU_INT08U *)&hw_addr[0],  /* ... HW MAC address.                                  */
                                    (CPU_INT08U  ) sizeof(hw_addr),
                                    (NET_ERR    *) p_err);
            if (*p_err != NET_IF_ERR_NONE) {                    /* No valid HW MAC address configured, return error.    */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        p_dev->MAHR = (((CPU_INT32U)hw_addr[0] << (3 * DEF_INT_08_NBR_BITS)) |
                       ((CPU_INT32U)hw_addr[1] << (2 * DEF_INT_08_NBR_BITS)) |
                       ((CPU_INT32U)hw_addr[2] << (1 * DEF_INT_08_NBR_BITS)) |
                       ((CPU_INT32U)hw_addr[3] << (0 * DEF_INT_08_NBR_BITS)));

        p_dev->MALR = (((CPU_INT32U)hw_addr[4] << (1 * DEF_INT_08_NBR_BITS)) |
                       ((CPU_INT32U)hw_addr[5] << (0 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    p_dev_data->MultiRxCtr = 0;
    p_dev_data->MultiRxMax = 0;
    p_dev_data->RxNRdyCtr  = 0;                                 /* No pending frames to process (see Note #2).          */
    p_dev_data->TxNRdyCtr  = 0;                                 /* No pending frames to process (see Note #2).          */

    NetDev_RxDescInit(p_if, p_err);                             /* Initialize Rx descriptors.                           */
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(p_if, p_err);                             /* Initialize Tx descriptors.                           */
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    p_dev->EDSR |= (EDSR_ENT |                                  /* Enable Rx and Tx                                     */
                    EDSR_ENR);

                                                                /* -------------------- CFG INT'S --------------------- */
    p_dev->EESIPR = (EES_FR      |                              /* Enable dev interrupts.                               */
                     EES_TC      |
                     EES_RX_ERR  |
                     EES_RMAF);

    BSP_IntSrcEn(RZ_INT_ID_ETH);

                                                                /* ------------------ ENABLE RX & TX ------------------ */
    p_dev->EDRRR  =  0x00000001L;                               /* Enable dev device.                                   */
    p_dev->EDTRR  =  0x00000000L;
    p_dev->ECMR  |= (ECMR_RX_EN |
                     ECMR_TX_EN);

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
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE    No Error
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Stop() via 'p_dev_api->Stop()'.
*
* Note(s)     : (2) Post all transmit buffers to the transmit deallocation task.  If the transmit
*                   buffer is NOT pending transmission, the transmit deallocation task will silently
*                   handle the error.  This mechanism allows device drivers to indiscriminately post
*                   to the transmit deallocation task thus preventing the need to determine exactly
*                   which descriptors have been marked for transmission and have NOT yet been
*                   transmitted.
*
*               (3) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    DEV_DESC           *p_desc;
    MEM_POOL           *p_mem_pool;
    CPU_INT08U          i;
    CPU_BOOLEAN         loop;
    LIB_ERR             lib_err;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    p_dev->ECMR   &= ~(ECMR_RX_EN |
                       ECMR_TX_EN);

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    p_dev->EESIPR &= ~(EES_FR     |                             /* Disable dev interrupts.                              */
                       EES_TC0    |
                       EES_RX_ERR);

    p_dev->EESR    =   0xFFFFFFFF;                              /* Clear all interrupt flags.                           */

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(p_if, p_err);

    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    for (i = 0; i < p_dev_cfg->TxDescNbr; i++) {                /* See Note #2.                                         */
        p_desc = &p_dev_data->TxBufDescPtrStart[i];

        BSP_DCache_InvalidateRange(p_desc, sizeof(DEV_DESC));

        do {
            NetIF_TxDeallocTaskPost(p_desc->p_buf, &err);
            switch (err) {
                case NET_IF_ERR_NONE:
                     loop = DEF_NO;
                     break;


                default:
                                                                /* Delay & retry if queue is full.                      */
                     KAL_Dly(2u);
                     loop = DEF_YES;
                     break;
            }
        } while (loop == DEF_YES);                              /* Loop until successfully queued for deallocation.     */
    }

                                                                /* --------------- FREE TX DESCRIPTORS ---------------- */
    p_mem_pool = &p_dev_data->TxDescPool;

    Mem_PoolBlkFree( p_mem_pool,                                /* Return Rx descriptor block to Rx descriptor pool.    */
                     p_dev_data->TxBufDescPtrStart,
                    &lib_err);

    if (lib_err != LIB_MEM_ERR_NONE) {
          *p_err = NET_DEV_ERR_MEM_ALLOC;
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
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_data  Pointer to pointer to received DMA data area. The received data
*                       area address should be returned to the stack by dereferencing
*                       p_data as *p_data = (address of receive data area).
*
*               size    Pointer to size. The number of bytes received should be returned
*                       to the stack by dereferencing size as *p_size = (number of bytes).
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error
*                           NET_DEV_ERR_RX              Generic Rx error.
*                           NET_DEV_ERR_INVALID_SIZE    Invalid Rx frame size.
*                           NET_BUF error codes         Potential NET_BUF error codes
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Rx().
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
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *p_if,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *p_size,
                         NET_ERR      *p_err)
{
    NET_DEV_DATA  *p_dev_data;
    LIST          *p_list;
    CPU_INT08U    *p_buf;
    CPU_BOOLEAN    valid;
    NET_BUF_SIZE   rx_len;
    NET_ERR        net_err;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();                                       /* Disable interrupts to alter shared data.             */
    p_list = p_dev_data->RxReadyListPtr;                        /* Get next ready buffer.                               */
    if (p_list != (LIST *)0) {
        p_dev_data->RxReadyListPtr = p_list->p_next;

       *p_size =               p_list->len;                     /* Return the size of the received frame.               */
       *p_data = (CPU_INT08U *)p_list->p_buf;                   /* Return a pointer to the newly received data area.    */

        p_list->len   =         0;
        p_list->p_buf = (void *)0;

        p_list->p_next            = p_dev_data->RxFreeListPtr;  /* Move list header into free list.                     */
        p_dev_data->RxFreeListPtr = p_list;
        CPU_CRITICAL_EXIT();                                    /* Restore interrupts.                                  */

       *p_err = NET_DEV_ERR_NONE;

    } else {

        CPU_CRITICAL_EXIT();                                    /* Restore interrupts.                                  */

       *p_size = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *p_err  =  NET_DEV_ERR_RX;
    }

    valid = DEF_TRUE;
    while (valid == DEF_TRUE) {
        p_buf = NetBuf_GetDataPtr((NET_IF        *)p_if,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_SIZE  *)&rx_len,
                                 (NET_BUF_TYPE  *) 0,
                                 (NET_ERR       *)&net_err);
        if (net_err != NET_BUF_ERR_NONE) {
            valid = DEF_FALSE;

        } else {

            CPU_CRITICAL_ENTER();                               /* Disable interrupts to alter shared data.             */
            p_list = p_dev_data->RxFreeListPtr;
            if (p_list != (LIST *)0) {
                p_dev_data->RxFreeListPtr   = p_list->p_next;
                p_list->p_buf               = p_buf;
                p_list->p_next              = p_dev_data->RxBufferListPtr;
                p_dev_data->RxBufferListPtr = p_list;

                CPU_CRITICAL_EXIT();                            /* Restore interrupts.                                  */
            } else {
                CPU_CRITICAL_EXIT();                            /* Restore interrupts.                                  */
                valid = DEF_FALSE;

                NetBuf_FreeBufDataAreaRx(p_if->Nbr, p_buf);     /* Return data area to Rx data area pool.               */
            }
        }
    }
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
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No Error
*                           NET_DEV_ERR_TX_BUSY     No Tx descriptors available
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Tx().
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
*                   drivers may find it useful to adjust p_dev_data->TxDescPtrComp after
*                   having selected the next available transmit descriptor.
*
*               (4)  The T01 descriptor bit is reserved for software use ONLY. This bit is set by software
*                    during transmission to indicate that the data ready bit for the descriptor is being set.
*                    When the DMA hardware completes transmission of the descriptor data, the ready bit is
*                    cleared and the T01 bit remains set. The transmit complete ISR handler then detects the
*                    cleared ready bit and set T01 bit and determines that the descriptor has completed
*                    transmission. The T01 bit is then cleared by the ISR handler.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *p_if,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    DEV_DESC           *p_desc;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* Obtain ptr to next available Tx descriptor.          */
    p_desc     = (DEV_DESC          *)p_dev_data->TxBufDescPtrCur;

    BSP_DCache_FlushRange(p_data, size);
    BSP_DCache_InvalidateRange(p_desc, sizeof(DEV_DESC));

    if (((p_desc->status) & ACT) > 0) {                         /* Find next available Tx descriptor (see Note #3).     */
       *p_err = NET_DEV_ERR_TX_BUSY;
        return;
    }

    CPU_CRITICAL_ENTER();
    if (p_dev_data->TxNRdyCtr < p_dev_cfg->TxDescNbr) {
        p_dev_data->TxNRdyCtr++;
    } else {
      *p_err = NET_DEV_ERR_TX_BUSY;
       CPU_CRITICAL_EXIT();
       return;
    }

    p_desc->p_buf    =  p_data;                                 /* Configure descriptor with Tx data area address.      */
    p_desc->sizebuf  = (size << 16);                            /* Configure descriptor with Tx data size.              */
    p_desc->status  |= (FP0 |                                   /* Hardware owns the descriptor, enable Tx Comp. Int's  */
                        FP1 |
                        ACT);                                   /* See note #4.                                         */

    BSP_DCache_FlushRange(p_desc, sizeof(DEV_DESC));

                                                                /* Update curr desc ptr to point to next desc.          */
    NET_BUF_DESC_PTR_NEXT(p_dev_data->TxBufDescPtrCur);

    p_dev->EDTRR  = 0x00000003L;

    CPU_CRITICAL_EXIT();

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_AddrMulticastAdd()
*
* Description : Configure hardware address filtering to accept specified hardware address.
*
* Argument(s) : p_if        Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               p_addr_hw   Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully configured.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *p_if,
                                       CPU_INT08U  *p_addr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    CPU_INT08U          ix;


    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;

    ix = 0u;
                                                                /* Check if MAC is not already present in table.        */
    while (ix < p_dev_data->MultiRxMax) {
        if ((p_dev->TSU_ADDR.Addr[ix].ADDRH == (p_addr_hw[0] << 24 |
                                                p_addr_hw[1] << 16 |
                                                p_addr_hw[2] <<  8 |
                                                p_addr_hw[3])) &&
            (p_dev->TSU_ADDR.Addr[ix].ADDRL == (p_addr_hw[4] <<  8 |
                                                p_addr_hw[5]))) {

                                                                /* Address is already present in the table.             */
           *p_err = NET_DEV_ERR_NONE;
            return;
        }

        ix++;
    }


    ix = 0u;

                                                                /* Find an available location to store the MAC.         */
    while (ix < TSU_ADR_LEN) {
        if ((p_dev->TSU_ADDR.Addr[ix].ADDRH == 0) &&
            (p_dev->TSU_ADDR.Addr[ix].ADDRL == 0)) {
            break;
        }

        ix++;
    }

    if (ix == TSU_ADR_LEN) {
       *p_err = NET_DEV_ERR_ADDR_MCAST_ADD;
        return;
    }

    p_dev->TSU_ADDR.Addr[ix].ADDRH = p_addr_hw[0] << 24 |
                                     p_addr_hw[1] << 16 |
                                     p_addr_hw[2] <<  8 |
                                     p_addr_hw[3];

    p_dev->TSU_ADDR.Addr[ix].ADDRL = p_addr_hw[4] <<  8 |
                                     p_addr_hw[5];

    p_dev->TSU_TEN |= (1 << ix);

    if ((ix + 1) > p_dev_data->MultiRxMax) {
        p_dev_data->MultiRxMax = ix + 1;
    }

    p_dev_data->MultiRxCtr++;
#else
   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
#endif

   (void)&p_addr_hw;                                            /* Prevent 'variable unused' compiler warnings.         */
   (void)&addr_hw_len;

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetDev_AddrMulticastRemove()
*
* Description : Configure hardware address filtering to reject specified hardware address.
*
* Argument(s) : p_if         Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               p_addr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully removed.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastRemove()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void   NetDev_AddrMulticastRemove (NET_IF      *p_if,
                                           CPU_INT08U  *p_addr_hw,
                                           CPU_INT08U   addr_hw_len,
                                           NET_ERR     *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    CPU_INT08S          last_entry;
    CPU_INT08U          ix;



    p_dev_cfg  = (NET_DEV_CFG_ETHER *) p_if->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    p_dev      = (NET_DEV           *) p_dev_cfg->BaseAddr;     /* Overlay dev reg struct on top of dev base addr.      */
    p_dev_data = (NET_DEV_DATA      *) p_if->Dev_Data;


                                                                /* Verify that an entry exists in the table.            */
    if (p_dev_data->MultiRxCtr <= 0) {
       *p_err = NET_DEV_ERR_ADDR_MCAST_REMOVE;
        return;
    }

    ix         =  0;
    last_entry = -1;

    while (ix < p_dev_data->MultiRxMax) {
        if ((p_dev->TSU_ADDR.Addr[ix].ADDRH == (p_addr_hw[0] << 24 |
                                                p_addr_hw[1] << 16 |
                                                p_addr_hw[2] <<  8 |
                                                p_addr_hw[3])) &&
            (p_dev->TSU_ADDR.Addr[ix].ADDRL == (p_addr_hw[4] <<  8 |
                                                p_addr_hw[5]))) {

            p_dev->TSU_ADDR.Addr[ix].ADDRH = 0;                 /* Addess is present in the table. Clear entry.         */
            p_dev->TSU_ADDR.Addr[ix].ADDRL = 0;

            p_dev->TSU_TEN &= ~(1 << ix);                       /* Clear CAM entry enable bit.                          */
            break;
        }

                                                                /* Keep track of the last CAM entry.                    */
        if ((p_dev->TSU_ADDR.Addr[ix].ADDRH != 0) &&
            (p_dev->TSU_ADDR.Addr[ix].ADDRL != 0)) {
            last_entry = ix;
        }

        ix++;
    }

    if (ix == TSU_ADR_LEN) {                                    /* If no entry was found. Return with error.            */
       *p_err = NET_DEV_ERR_ADDR_MCAST_REMOVE;
        return;
    }

    if ((ix         == p_dev_data->MultiRxMax) &&               /* If removed entry was the last one. Adjust max ...    */
        (last_entry >= 0)) {                                    /* ... CAM usage.                                       */
        p_dev_data->MultiRxMax = (CPU_INT32U)last_entry;
    }

    p_dev_data->MultiRxCtr--;

#else
   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
#endif

   (void)&p_addr_hw;                                            /* Prevent 'variable unused' compiler warnings.         */
   (void)&addr_hw_len;

   *p_err = NET_DEV_ERR_NONE;
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
* Argument(s) : p_if     Pointer to interface requiring service.
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
*               p_err    Pointer to return error code.
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
* Caller(s)   : NetIF_Ether_IO_Ctrl()
*               NetPhy_LinkStateGet()
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *p_if,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *p_err)
{
    NET_DEV_LINK_ETHER  *p_link_state;
    NET_DEV_CFG_ETHER   *p_dev_cfg;
    NET_DEV             *p_dev;
    NET_PHY_API_ETHER   *p_phy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    p_dev_cfg = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;             /* Obtain ptr to the dev cfg struct.                    */
    p_dev     = (NET_DEV           *)p_dev_cfg->BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* ----------- PERFORM SPECIFIED OPERATION ------------ */
    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             p_link_state = (NET_DEV_LINK_ETHER *)p_data;

             if (p_link_state == (NET_DEV_LINK_ETHER *)0) {
                *p_err = NET_DEV_ERR_NULL_PTR;
                 return;
             }

             p_phy_api = (NET_PHY_API_ETHER *)p_if->Ext_API;
             if (p_phy_api == (void *)0) {
                *p_err = NET_ERR_FAULT_NULL_FNCT;
                 return;
             }

             p_phy_api->LinkStateGet(p_if, p_link_state, p_err);

             if (*p_err != NET_PHY_ERR_NONE) {
                  return;
             }

            *p_err = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
             p_phy_api    = (NET_PHY_API_ETHER *)p_if->Ext_API;
             p_link_state = (NET_DEV_LINK_ETHER *)p_data;
             duplex      =  NET_PHY_DUPLEX_UNKNOWN;

             if (p_link_state->Duplex != duplex) {
                 switch (p_link_state->Duplex) {
                    case NET_PHY_DUPLEX_FULL:
                         p_dev->ECMR |=   ECMR_DUPLEX_FULL;
                         break;


                    case NET_PHY_DUPLEX_HALF:
                         p_dev->ECMR &= ~(ECMR_DUPLEX_FULL);
                         break;


                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;

             if (p_link_state->Spd != spd) {
                 switch (p_link_state->Spd) {                   /* Update speed register setting on device.             */
                    case NET_PHY_SPD_10:
                    case NET_PHY_SPD_100:
                         p_phy_api->LinkStateSet(p_if, p_link_state, p_err);
                         break;


                    case NET_PHY_SPD_1000:                      /* 1000 Mbps not supported.                             */
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
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Rd()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : p_if        Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               p_err    Pointer to return error code.
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

static  void  NetDev_MII_Rd (NET_IF      *p_if,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U  *p_data,
                             NET_ERR     *p_err)
{
   NetDev_MII_Preamble(p_if);

   NetDev_MII_Cmd     (p_if,
                       reg_addr,
                       PHY_READ);

   NetDev_MII_Z       (p_if);

   NetDev_MII_RegRead (p_if,
                       p_data);

   NetDev_MII_Z       (p_if);

  (void)&phy_addr;

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : p_if        Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               data        Data to write to the specified Phy register.
*
*               p_err    Pointer to return error code.
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

static  void  NetDev_MII_Wr (NET_IF      *p_if,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   data,
                             NET_ERR     *p_err)
{
    NetDev_MII_Preamble  (p_if);

    NetDev_MII_Cmd       (p_if,
                          reg_addr,
                          PHY_WRITE);

    NetDev_MII_TurnAround(p_if);

    NetDev_MII_RegWrite  (p_if,
                          data);

    NetDev_MII_Z         (p_if);

   (void)&phy_addr;

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_RxDescInit()
*
* Description : (1) This function initializes the Rx descriptor list for the specified interface :
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Rx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start()
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

static  void  NetDev_RxDescInit (NET_IF   *p_if,
                                 NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    MEM_POOL           *p_mem_pool;
    DEV_DESC           *p_desc;
    LIST               *p_list;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbr_octets;
    CPU_INT16U          free_cnt;
    CPU_INT16U          i;


                                                                /* --- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* ---------------- ALLOCATE DESCRIPTORS  ------------- */
                                                                /* See Note #3.                                         */
    p_mem_pool = &p_dev_data->RxDescPool;
    nbr_octets =  p_dev_cfg->RxDescNbr * sizeof(DEV_DESC);
    p_desc     = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) p_mem_pool,
                                            (CPU_SIZE_T) nbr_octets,
                                            (LIB_ERR  *)&lib_err);

    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }
                                                                /* ------------ INIT DESCRIPTOR PTRS  ----------------- */
    p_dev_data->RxBufDescPtrStart = (DEV_DESC *)p_desc;
    p_dev_data->RxBufDescPtrCur   = (DEV_DESC *)p_desc;
    p_dev_data->RxBufDescPtrEnd   = (DEV_DESC *)p_desc + (p_dev_cfg->RxDescNbr - 1);

                                                                /* ------------- INIT RX DESCRIPTORS ------------------ */
    for (i = 0; i < p_dev_cfg->RxDescNbr; i++) {
        p_desc->status  = ACT;
        p_desc->sizebuf = (p_dev_cfg->RxBufLargeSize << 16);
        p_desc->p_buf   = NetBuf_GetDataPtr((NET_IF        *)p_if,
                                            (NET_TRANSACTION)NET_TRANSACTION_RX,
                                            (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                            (NET_BUF_SIZE   )NET_IF_IX_RX,
                                            (NET_BUF_SIZE  *)0,
                                            (NET_BUF_SIZE  *)0,
                                            (NET_BUF_TYPE  *)0,
                                            (NET_ERR       *)p_err);
        if (*p_err != NET_BUF_ERR_NONE) {
             return;
        }

        if (p_desc == (p_dev_data->RxBufDescPtrEnd)) {          /* Set DLE bit on last descriptor in list.              */
            p_desc->status |= DLE;
            p_desc->p_next  = p_dev_data->RxBufDescPtrStart;
        } else {
            p_desc->p_next  = p_desc + 1;                       /* Point to next descriptor in list.                    */
        }

        BSP_DCache_FlushRange(p_desc, sizeof(DEV_DESC));

        p_desc++;
    }
                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start address.    */
    p_dev->RDLAR = (CPU_INT32U)p_dev_data->RxBufDescPtrStart;   /* Cfg DMA descriptor ring start address.               */
    p_dev->RDFAR = (CPU_INT32U)p_dev_data->RxBufDescPtrStart;
    p_dev->RDFXR = (CPU_INT32U)p_dev_data->RxBufDescPtrEnd;

    free_cnt = p_dev_cfg->RxBufLargeNbr - p_dev_cfg->RxDescNbr;
    for (i = 0; i < free_cnt; i++) {
        p_list                    = p_dev_data->RxFreeListPtr;  /* Get list header from free list.                      */
        p_dev_data->RxFreeListPtr = p_list->p_next;

        p_list->p_buf = NetBuf_GetDataPtr((NET_IF        *)p_if,
                                         (NET_TRANSACTION)NET_TRANSACTION_RX,
                                         (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                         (NET_BUF_SIZE   )NET_IF_IX_RX,
                                         (NET_BUF_SIZE  *)0,
                                         (NET_BUF_SIZE  *)0,
                                         (NET_BUF_TYPE  *)0,
                                         (NET_ERR       *)p_err);

        if (*p_err != NET_BUF_ERR_NONE) {
                                                                /* Return list header to free list.                     */
             p_list->p_next            = p_dev_data->RxFreeListPtr;
             p_dev_data->RxFreeListPtr = p_list;
             break;
        }

                                                                /* Store on buffer list.                                */
        p_list->p_next              = p_dev_data->RxBufferListPtr;
        p_dev_data->RxBufferListPtr = p_list;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_RxDescFreeAll()
*
* Description : (1) This function returns the descriptor memory block and descriptor data area
*                   memory blocks back to their respective memory pools :
*
*                   (a) Free Rx descriptor data areas
*                   (b) Free Rx descriptor memory block
*
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop()
*
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL receive buffers
*                   and the Rx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *p_if,
                                   NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    MEM_POOL           *p_mem_pool;
    DEV_DESC           *p_desc;
    LIB_ERR             lib_err;
    CPU_INT16U          i;
    CPU_INT08U         *p_desc_data;
    LIST               *p_list;
    LIST               *p_list_next;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    p_mem_pool = &p_dev_data->RxDescPool;
    p_desc     =  p_dev_data->RxBufDescPtrStart;

    for (i = 0; i < p_dev_cfg->RxDescNbr; i++) {                /* Free Rx descriptor ring.                             */
        p_desc_data = p_desc->p_buf;
        NetBuf_FreeBufDataAreaRx(p_if->Nbr, p_desc_data);       /* Return data area to Rx data area pool.               */
        p_desc++;
    }

    p_list = p_dev_data->RxBufferListPtr;
    while (p_list != (LIST *)0) {
        p_list_next = p_list->p_next;
        p_desc_data = p_list->p_buf;
        NetBuf_FreeBufDataAreaRx(p_if->Nbr, p_desc_data);       /* Return data area to Rx data area pool.               */
        p_list->p_buf = (void *)0;
        p_list->len   =  0;

        p_list->p_next            = p_dev_data->RxFreeListPtr;
        p_dev_data->RxFreeListPtr = p_list;

        p_list = p_list_next;
    }
    p_dev_data->RxBufferListPtr = (LIST *)0;

    p_list = p_dev_data->RxReadyListPtr;
    while (p_list != (LIST *)0) {
        p_list_next = p_list->p_next;
        p_desc_data = p_list->p_buf;
        NetBuf_FreeBufDataAreaRx(p_if->Nbr, p_desc_data);       /* Return data area to Rx data area pool.               */
        p_list->p_buf = (void *)0;
        p_list->len   =  0;

        p_list->p_next            = p_dev_data->RxFreeListPtr;
        p_dev_data->RxFreeListPtr = p_list;

        p_list = p_list_next;
    }
    p_dev_data->RxReadyListPtr = (LIST *)0;

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( p_mem_pool,                                /* Return Rx descriptor block to Rx descriptor pool.    */
                     p_dev_data->RxBufDescPtrStart,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
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
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Rx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : p_if     Pointer to the interface requiring service.
*
*               p_err    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start()
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

static  void  NetDev_TxDescInit (NET_IF   *p_if,
                                 NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    DEV_DESC           *p_desc;
    MEM_POOL           *p_mem_pool;
    CPU_SIZE_T          nbytes;
    CPU_INT16U          i;
    LIB_ERR             lib_err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    p_mem_pool = &p_dev_data->TxDescPool;
    nbytes     =  p_dev_cfg->TxDescNbr * sizeof(DEV_DESC);
    p_desc     = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) p_mem_pool,
                                            (CPU_SIZE_T) nbytes,
                                            (LIB_ERR  *)&lib_err);

    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    p_dev_data->TxBufDescPtrStart = (DEV_DESC *)p_desc;
    p_dev_data->TxBufDescPtrCur   = (DEV_DESC *)p_desc;
    p_dev_data->TxBufDescCompPtr  = (DEV_DESC *)p_desc;
    p_dev_data->TxBufDescPtrEnd   = (DEV_DESC *)p_desc + (p_dev_cfg->TxDescNbr - 1);

                                                                /* --------------- INIT TX DESCRIPTORS ---------------- */
    for (i = 0; i < p_dev_cfg->TxDescNbr; i++) {                /* Initialize Tx descriptor ring.                       */
        p_desc->status  = 0;
        p_desc->sizebuf = 0;
        p_desc->p_buf   = 0;

        if (p_desc == (p_dev_data->TxBufDescPtrEnd)) {          /* Set DLE bit on last descriptor in list.              */
            p_desc->status |= DLE;
            p_desc->p_next  = p_dev_data->TxBufDescPtrStart;
        } else {
            p_desc->p_next  = (p_desc + 1);                     /* Point to next descriptor in list.                    */
        }

        BSP_DCache_FlushRange(p_desc, sizeof(DEV_DESC));

        p_desc++;
    }
                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
    p_dev->TDLAR = (CPU_INT32U)p_dev_data->TxBufDescPtrStart;   /* Configure the DMA with the Tx desc start address.    */
    p_dev->TDFAR = (CPU_INT32U)p_dev_data->TxBufDescPtrStart;
    p_dev->TDFXR = (CPU_INT32U)p_dev_data->TxBufDescPtrEnd;

   *p_err        = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_ISR_Handler()
*
* Description : This function serves as the device Interrupt Service Routine Handler. This ISR
*               handler MUST service and clear all necessary and enabled interrupt events for
*               the device.
*
* Argument(s) : p_if     Pointer to interface requiring service.
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
* Caller(s)   : NetBSP_ISR_Handler().     Specific interface first or second level ISR handler.
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

static  void  NetDev_ISR_Handler (NET_IF            *p_if,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_DATA       *p_dev_data;
    NET_DEV            *p_dev;
    DEV_DESC           *p_desc;
    CPU_DATA            reg_val;
    CPU_BOOLEAN         valid;
    NET_ERR             err;


    (void)&type;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ---------- DETERMINE ISR TYPE IF UNKNOWN ----------- */
    reg_val    = p_dev->EESR;                                   /* Determine ISR type.                                  */
    p_dev->EESR = reg_val;                                      /* Clear interrupts                                     */

                                                                /* ---------------- PROCESS INTERRUPT ----------------- */
    if (((reg_val & EES_FR)     != 0) ||
        ((reg_val & EES_RX_ERR) != 0)) {

        valid = DEF_TRUE;
        while (valid == DEF_TRUE) {
            p_desc = (DEV_DESC *)p_dev_data->RxBufDescPtrCur;   /* Obtain ptr to next ready descriptor.                 */

            BSP_DCache_InvalidateRange(p_desc, sizeof(DEV_DESC));

            if ((p_desc->status & ACT) == 0) {                  /* Descriptor ready to receive.                         */
                valid = NetDev_ISR_Rx(p_if, p_desc);

                p_dev_data->RxBufDescPtrCur = p_desc->p_next;
            } else {
                valid = DEF_FALSE;
            }
        }

        if (p_dev->EDRRR == 0x00000000u) {                      /* If the receiver has stopped, restart                 */
            p_dev->EDRRR  = 0x00000001u;
        }
    }

    if ((reg_val & EES_TC) != 0) {
        if (p_dev_data->TxNRdyCtr > 0) {
            p_desc = p_dev_data->TxBufDescCompPtr;

            BSP_DCache_InvalidateRange(p_desc, sizeof(DEV_DESC));


            while (((p_dev_data->TxNRdyCtr)  > 0) &&
                  (((p_desc->status) & ACT) == 0)) {

                NetIF_TxDeallocTaskPost(p_desc->p_buf, &err);

                if (err != NET_IF_ERR_NONE) {
                    break;
                }

                NET_BUF_DESC_PTR_NEXT(p_dev_data->TxBufDescCompPtr);

                NetIF_DevTxRdySignal(p_if);                     /* Signal Net IF that Tx resources are available.       */

                p_dev_data->TxNRdyCtr--;

                NET_BUF_DESC_PTR_NEXT(p_desc);

                BSP_DCache_InvalidateRange(p_desc, sizeof(DEV_DESC));
            }
        }
    }
}


/*
*********************************************************************************************************
*                                           NetDev_ISR_Rx()
*
* Description : This function serves as the device Interrupt Service Routine Receive Handler.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
* Return(s)   : DEF_YES,     if the packet received is valid
*
*               DEF_NO,      if error occured.
*
* Caller(s)   : NetDev_ISR_Handler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetDev_ISR_Rx (NET_IF    *p_if,
                                    DEV_DESC  *p_desc)
{
    NET_DEV_DATA  *p_dev_data;
    LIST          *p_list_buf;
    LIST          *p_list_ready;
    void          *p_buf;
    CPU_BOOLEAN    valid;
    CPU_BOOLEAN    signal;
    NET_ERR        err;


    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */
    valid      =  DEF_TRUE;
    signal     =  DEF_FALSE;

    if ((p_desc->status & FE) != 0) {                           /* Frame error, must advance curr ptr.                  */
        valid = DEF_FALSE;
    }

    if ((p_desc->status & (FP0 | FP1)) != (FP0 | FP1)) {        /* Frame should be contained in a single buffer.        */
        valid = DEF_FALSE;
    }

    if (p_dev_data->RxBufferListPtr == (LIST *)0) {
        valid  = DEF_FALSE;
        signal = DEF_TRUE;
    }

    p_desc->status &= ~(FP1    |
                        FP0    |
                        FE);

    BSP_DCache_FlushRange(p_desc, sizeof(DEV_DESC));

    if (valid == DEF_TRUE) {
        p_list_buf = p_dev_data->RxBufferListPtr;
        p_dev_data->RxBufferListPtr = p_list_buf->p_next;

        p_buf = p_list_buf->p_buf;

        p_list_buf->p_buf  =  p_desc->p_buf;
        p_list_buf->len    = (p_desc->sizebuf & 0x0000FFFF);
        p_list_buf->p_next = (LIST *)0;

        BSP_DCache_InvalidateRange(p_list_buf->p_buf, p_list_buf->len);

        if (p_dev_data->RxReadyListPtr == (LIST *)0) {
            p_dev_data->RxReadyListPtr  = p_list_buf;
        } else {
            p_list_ready = p_dev_data->RxReadyListPtr;

            while (p_list_ready != (LIST *)0) {
                if (p_list_ready->p_next == (LIST *)0) {
                    break;
                }
                p_list_ready = p_list_ready->p_next;
            }

            p_list_ready->p_next = p_list_buf;
        }

        p_desc->p_buf    = p_buf;
        p_desc->sizebuf &= 0xFFFF0000;
    }

    if ((valid  == DEF_TRUE) ||
        (signal == DEF_TRUE)) {
        NetIF_RxTaskSignal(p_if->Nbr, &err);                    /* Signal Net IF RxQ Task of new frame.                 */
    }

    p_desc->sizebuf &= 0xFFFF0000;
    p_desc->status  |= ACT;

    BSP_DCache_FlushRange(p_desc, sizeof(DEV_DESC));

    return (valid);
}


/*
*********************************************************************************************************
*                                         NetDev_MII_Preamble()
*
* Description : Execute preambles.
*
* Argument(s) : p_if     Pointer to interface requiring service.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Rd(),
*               NetDev_MII_Wr().
*
* Note(s)     : 1 is output via the MII management interface.
*********************************************************************************************************
*/

static void NetDev_MII_Preamble (NET_IF  *p_if)
{
    CPU_INT16S   i;


    i = 32;

    while (i > 0) {
                                                                /* 1 is output via the MII block.                       */
        NetDev_MII_Write1(p_if);
        i--;
    }
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Cmd()
*
* Description : Execute commands.
*
* Argument(s) : p_if        Pointer to interface requiring service.
*
*               reg_addr    Register address on which the command is executed.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Rd(),
*               NetDev_MII_Wr().
*
* Note(s)     : Placing the PHY module register in read or write mode.
*********************************************************************************************************
*/

static  void  NetDev_MII_Cmd (NET_IF      *p_if,
                              CPU_INT16U   reg_addr,
                              CPU_INT32S   option)
{
    CPU_INT32S  i;
    CPU_INT16U  data;


    data = 0;
    data = (PHY_ST << 14);                                      /* ST code.                                             */

    if (PHY_READ == option) {
        data |= (PHY_READ  << 12);                              /* OP code(RD).                                         */
    } else {
        data |= (PHY_WRITE << 12);                              /* OP code(WT).                                         */
    }

    data |= (PHY_ADDR << 7);                                    /* PHY Address.                                         */
    data |= (CPU_INT16U)(reg_addr << 2);                        /* Reg Address.                                         */

    for (i = 14; i > 0; i--) {
        if ((data & 0x8000) == 0) {
            NetDev_MII_Write0(p_if);
        } else {
            NetDev_MII_Write1(p_if);
        }

        data <<= 1;
    }
}


/*
*********************************************************************************************************
*                                         NetDev_MII_RegRead()
*
* Description : Read bits.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
*               p_data  Pointer to receive the data read from the MII.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Rd().
*
* Note(s)     : Obtains the value of the PHY module register bit by bit.
*********************************************************************************************************
*/

static  void  NetDev_MII_RegRead (NET_IF      *p_if,
                                  CPU_INT16U  *p_data)
{
    volatile  CPU_INT32S          i;
    volatile  CPU_INT32S          j;
              CPU_INT16U          reg_data;
              NET_DEV_CFG_ETHER  *p_dev_cfg;
              NET_DEV            *p_dev;


    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

    reg_data   = 0;

    for (i = 16; i > 0; i--) {                                  /* Data are read in one bit at a time.                  */
        for (j = MDC_WAIT; j > 0; j--) {
            p_dev->PIR = 0x00000000;
        }

        for (j = MDC_WAIT; j > 0; j--) {
            p_dev->PIR = 0x00000001;
        }

        reg_data <<= 1;
                                                                /* MDI read.                                            */
        reg_data  |= (CPU_INT16U)((p_dev->PIR & 0x00000008) >> 3);

        for (j = MDC_WAIT; j > 0; j--) {
            p_dev->PIR = 0x00000001;
        }

        for (j = MDC_WAIT; j > 0; j--) {
            p_dev->PIR = 0x00000000;
        }
    }

    *p_data = reg_data;
}


/*
*********************************************************************************************************
*                                        NetDev_MII_RegWrite()
*
* Description : Wtite bits.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
*               data    Data to write to MII.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Wr().
*
* Note(s)     : The value of the PHY module register is set one bit at a time.
*********************************************************************************************************
*/

static  void  NetDev_MII_RegWrite (NET_IF      *p_if,
                                   CPU_INT16U   data)
{
    volatile  CPU_INT32S  i;


    for (i = 16; i > 0; i--) {                                  /* Data are written one bit at a time.                  */
        if ((data & 0x8000) == 0) {
            NetDev_MII_Write0(p_if);
        } else {
            NetDev_MII_Write1(p_if);
        }

        data <<= 1;
    }
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Z()
*
* Description : Execute one clock.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Rd(),
*               NetDev_MII_Wr().
*
* Note(s)     : Reading is selected as the direction of access to the PHY module.
*********************************************************************************************************
*/

static  void  NetDev_MII_Z (NET_IF  *p_if)
{
    volatile  CPU_INT32S          j;
              NET_DEV_CFG_ETHER  *p_dev_cfg;
              NET_DEV            *p_dev;


    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */


    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000000;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000001;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000001;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000000;
    }
}


/*
*********************************************************************************************************
*                                         NetDev_MII_TurnAround()
*
* Description : Execute a bus turn-around.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Wr().
*
* Note(s)     : Outputs 1 and 0 to the MII management interface of the PHY module.
*********************************************************************************************************
*/

static  void  NetDev_MII_TurnAround (NET_IF  *p_if)
{
    NetDev_MII_Write1(p_if);
    NetDev_MII_Write0(p_if);
}


/*
*********************************************************************************************************
*                                          NetDev_MII_Write1()
*
* Description : Output 1.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Preamble(),
*               NetDev_MII_Cmd(),
*               NetDev_MII_RegWrite(),
*               NetDev_MII_TurnAround().
*
* Note(s)     : 1 is output to the MII management interface of the PHY module.
*********************************************************************************************************
*/

static  void  NetDev_MII_Write1 (NET_IF  *p_if)
{
    volatile  CPU_INT32S          j;
              NET_DEV_CFG_ETHER  *p_dev_cfg;
              NET_DEV            *p_dev;


    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000006;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000007;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000007;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000006;
    }
}


/*
*********************************************************************************************************
*                                          NetDev_MII_Write0()
*
* Description : Output 0.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MII_Cmd(),
*               NetDev_MII_RegWrite(),
*               NetDev_MII_TurnAround().
*
* Note(s)     : 0 is output to the MII management interface of the PHY module.
*********************************************************************************************************
*/

static void NetDev_MII_Write0 (NET_IF  *p_if)
{
    volatile  CPU_INT32S          j;
              NET_DEV_CFG_ETHER  *p_dev_cfg;
              NET_DEV            *p_dev;


    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev      = (NET_DEV           *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000002;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000003;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000003;
    }

    for (j = MDC_WAIT; j > 0; j--) {
        p_dev->PIR = 0x00000002;
    }
}


#endif  /* NET_IF_ETHER_MODULE_EN */

