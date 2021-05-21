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
*                                       LUMINARY MICRO LM3S9Bxx
*
* Filename : net_dev_lm3s9bxx.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) The driver support DMA using the Micro Direct Memory Access (uDMA) module.
*
*            (3) Only the following devices are supported by this driver:
*
*                    LMS9B90.
*                    LMS9B92.
*                    LMS9B95.
*                    LMS9B96.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_LM3Sxxxx_MODULE
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_lm3s9bxx.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO                              10000    /* MII read write timeout.                              */

#define  RX_BUF_IX_OFFSET_OCTETS                           2
#define  TX_BUF_IX_OFFSET_OCTETS                           2


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    CPU_INT08U  *TxBufCompPtr;                                  /* Tx buffer pointer.                                   */
    CPU_BOOLEAN  TxDMA_En;                                      /* Tx DMA enable flag.                                  */
    CPU_INT32U  *RxBufCompPtr;                                  /* Rx buffer pointer.                                   */
    CPU_INT32U   RxPktLen;                                      /* Rx Packet Length.                                    */
    CPU_BOOLEAN  RxDMA_En;                                      /* Rx DMA enable flag.                                  */
#ifdef NET_MCAST_MODULE_EN
    CPU_INT32U   RxMulticastCtr;                                /* Rx Multicast enable counter.                         */
#endif
} NET_DEV_DATA;


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

                                                                /* ------------ FNCT'S COMMON TO ALL DEV'S ------------ */
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

static  void  NetDev_ISR_Handler        (NET_IF             *pif,
                                         NET_DEV_ISR_TYPE    type);

static  void  NetDev_IO_Ctrl            (NET_IF             *pif,
                                         CPU_INT08U          opt,
                                         void               *p_data,
                                         NET_ERR            *perr);

static  void  NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *perr);

static  void  NetDev_AddrMulticastRemove(NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
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
                                                                                    /* LM3S9Bxx dev API fnct ptrs :     */
const  NET_DEV_API_ETHER  NetDev_API_LM3S9Bxx = { NetDev_Init,                      /*   Init/add                       */
                                                  NetDev_Start,                     /*   Start                          */
                                                  NetDev_Stop,                      /*   Stop                           */
                                                  NetDev_Rx,                        /*   Rx                             */
                                                  NetDev_Tx,                        /*   Tx                             */
                                                  NetDev_AddrMulticastAdd,          /*   Multicast addr add             */
                                                  NetDev_AddrMulticastRemove,       /*   Multicast addr remove          */
                                                  NetDev_ISR_Handler,               /*   ISR handler                    */
                                                  NetDev_IO_Ctrl,                   /*   I/O ctrl                       */
                                                  NetDev_MII_Rd,                    /*   Phy reg rd                     */
                                                  NetDev_MII_Wr                     /*   Phy reg wr                     */
                                                };

/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  MACIS;                                           /* Ethernet MAC Raw Interrupt Status/Acknowledge.       */
    CPU_REG32  MACIM;                                           /* Ethernet MAC Interrupt Mask.                         */
    CPU_REG32  MACRCTL;                                         /* Ethernet MAC Receive Control.                        */
    CPU_REG32  MACTCTL;                                         /* Ethernet MAC Transmit Control.                       */
    CPU_REG32  MACDATA;                                         /* Ethernet MAC Data.                                   */
    CPU_REG32  MACIA0;                                          /* Ethernet MAC Individual Address 0.                   */
    CPU_REG32  MACIA1;                                          /* Ethernet MAC Individual Address 1.                   */
    CPU_REG32  MACTHR;                                          /* Ethernet MAC Threshold.                              */
    CPU_REG32  MACMCTL;                                         /* Ethernet MAC Management Control.                     */
    CPU_REG32  MACMDV;                                          /* Ethernet MAC Management Divider.                     */
    CPU_REG32  Reserved0;                                       /* [Reserved].                                          */
    CPU_REG32  MACMTXD;                                         /* Ethernet MAC Management Transmit Data.               */
    CPU_REG32  MACMRXD;                                         /* Ethernet MAC Management Receive Data.                */
    CPU_REG32  MACNP;                                           /* Ethernet MAC Number of Packets.                      */
    CPU_REG32  MACTR;                                           /* Ethernet MAC Transmission Request.                   */
    CPU_REG32  Reserved1;                                       /* [Reserved].                                          */
    CPU_REG32  MACLED;                                          /* Ethernet MAC LED Encoding.                           */
    CPU_REG32  MACIX;                                           /* Ethernet PHY MDIX.                                   */
} NET_DEV;

                                                                /* ----------- MICRO DIRECT MEMORY ACCESS ------------- */
typedef  struct net_dev_dma {
    CPU_REG32  DMA_STAT;                                        /* DMA Status.                                          */
    CPU_REG32  DMA_CFG;                                         /* DMA Configuration.                                   */
    CPU_REG32  DMA_CTL_BASE;                                    /* DMA Channel Control Base Pointer.                    */
    CPU_REG32  DMA_ALT_BASE;                                    /* DMA Alternate Channel Control Base Pointer.          */
    CPU_REG32  DMA_WAIT_STAT;                                   /* DMA Channel Wait-on-Request Status.                  */
    CPU_REG32  DMA_SWREQ;                                       /* DMA Channel Software Request.                        */
    CPU_REG32  DMA_USE_BURST_SET;                               /* DMA Channel Useburst Set.                            */
    CPU_REG32  DMA_USE_BURST_CLR;                               /* DMA Channel Useburst Clear.                          */
    CPU_REG32  DMA_REQ_MSK_SET;                                 /* DMA Channel Request Mask Set.                        */
    CPU_REG32  DMA_REQ_MSK_CLR;                                 /* DMA Channel Request Mask Clr.                        */
    CPU_REG32  DMA_ENA_SET;                                     /* DMA Channel Enable Set.                              */
    CPU_REG32  DMA_ENA_CLR;                                     /* DMA Channel Enable Clr.                              */
    CPU_REG32  DMA_ALT_SET;                                     /* DMA Channel Primary Alternate Set.                   */
    CPU_REG32  DMA_ALT_CLR;                                     /* DMA Channel Primary Alternate Clr.                   */
    CPU_REG32  DMA_PRIO_SET;                                    /* DMA Channel Priority Set.                            */
    CPU_REG32  DMA_PRIO_CLR;                                    /* DMA Channel Priority Clr.                            */
    CPU_REG32  RSVD0[3];                                        /* Reserved.                                            */
    CPU_REG32  DMA_ERR_CLR;                                     /* DMA Channel Error Clear.                             */
    CPU_REG32  RSVD1[300];                                      /* Reserved.                                            */
    CPU_REG32  DMA_CH_ALT;                                      /* DMA Channel Alternate Select                         */
} NET_DEV_DMA;


typedef  struct net_dev_dma_ch {
    CPU_REG32  SrcEndPtr;                                       /* DMA Channel Source Address End Pointer.              */
    CPU_REG32  DestEndPtr;                                      /* DMA Channel Destination Address End Pointer.         */
    CPU_REG32  Ctrl;                                            /* DMA Channel Control Word.                            */
    CPU_REG32  RSVD0;                                           /* Reserved.                                            */
} NET_DEV_DMA_CH;


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*********************************************************************************************************
*/

#define  LM3S9BXX_BIT_MACIS_RXINT                 DEF_BIT_00
#define  LM3S9BXX_BIT_MACIS_TXER                  DEF_BIT_01
#define  LM3S9BXX_BIT_MACIS_TXEMP                 DEF_BIT_02
#define  LM3S9BXX_BIT_MACIS_FOV                   DEF_BIT_03
#define  LM3S9BXX_BIT_MACIS_RXER                  DEF_BIT_04
#define  LM3S9BXX_BIT_MACIS_MDINT                 DEF_BIT_05
#define  LM3S9BXX_BIT_MACIS_PHYINT                DEF_BIT_06

#define  LM3S9BXX_BIT_MACIM_RXINT                 DEF_BIT_00
#define  LM3S9BXX_BIT_MACIM_TXER                  DEF_BIT_01
#define  LM3S9BXX_BIT_MACIM_TXEMP                 DEF_BIT_02
#define  LM3S9BXX_BIT_MACIM_FOV                   DEF_BIT_03
#define  LM3S9BXX_BIT_MACIM_RXER                  DEF_BIT_04
#define  LM3S9BXX_BIT_MACIM_MDINT                 DEF_BIT_05
#define  LM3S9BXX_BIT_MACIM_PHYINT                DEF_BIT_06
#define  LM3S9BXX_BIT_MACIM_RX_ALL               (LM3S9BXX_BIT_MACIM_RXINT | \
                                                  LM3S9BXX_BIT_MACIM_FOV   | \
                                                  LM3S9BXX_BIT_MACIM_RXER)

#define  LM3S9BXX_BIT_MACRCTL_RXEN                DEF_BIT_00
#define  LM3S9BXX_BIT_MACRCTL_AMUL                DEF_BIT_01
#define  LM3S9BXX_BIT_MACRCTL_PRMS                DEF_BIT_02
#define  LM3S9BXX_BIT_MACRCTL_BADCRC              DEF_BIT_03
#define  LM3S9BXX_BIT_MACRCTL_RSTFIFO             DEF_BIT_04

#define  LM3S9BXX_BIT_MACTCTL_TXEN                DEF_BIT_00
#define  LM3S9BXX_BIT_MACTCTL_PADEN               DEF_BIT_01
#define  LM3S9BXX_BIT_MACTCTL_CRC                 DEF_BIT_02
#define  LM3S9BXX_BIT_MACTCTL_DUPLEX              DEF_BIT_04

#define  LM3S9BXX_BIT_MACIA0_MACOCT4              0xFF000000u
#define  LM3S9BXX_BIT_MACIA0_MACOCT3              0x00FF0000u
#define  LM3S9BXX_BIT_MACIA0_MACOCT2              0x0000FF00u
#define  LM3S9BXX_BIT_MACIA0_MACOCT1              0x000000FFu

#define  LM3S9BXX_BIT_MACIA1_MACOCT6              0x0000FF00u
#define  LM3S9BXX_BIT_MACIA1_MACOCT5              0x000000FFu

#define  LM3S9BXX_BIT_MACTHR_THRESH               0x0000003Fu

#define  LM3S9BXX_BIT_MACMCTL_START               DEF_BIT_00
#define  LM3S9BXX_BIT_MACMCTL_WRITE               DEF_BIT_01
#define  LM3S9BXX_BIT_MACMCTL_REGADR              0x000000F8u

#define  LM3S9BXX_BIT_MACMDV_DIV                  0x000000FFu

#define  LM3S9BXX_BIT_MACMTXD_MDTX                0x0000FFFFu

#define  LM3S9BXX_BIT_MACMRXD_MDRX                0x0000FFFFu

#define  LM3S9BXX_BIT_MACNP_NPR                   0x0000003Fu

#define  LM3S9BXX_BIT_MACTR_NEWTX                 DEF_BIT_00

#define  LM3S9BXX_BIT_MACTS_TSEN                  DEF_BIT_00

                                                                /* ------------- MICRO DIRECT MEMORY ACCESS ----------- */
#define  LM3S9BXX_BIT_DMA_CFG_MASTEN              DEF_BIT_00
#define  LM3S9BXX_BIT_DMACHCTL_XFERMODE_STOP      DEF_BIT_NONE
#define  LM3S9BXX_BIT_DMACHCTL_XFERMODE_BASIC     DEF_BIT_MASK(1u, 0u)
#define  LM3S9BXX_BIT_DMACHCTL_XFERMODE_AUTO      DEF_BIT_MASK(2u, 0u)

#define  LM3S9BXX_BIT_DMACHCTL_SIZE_BYTE          0u
#define  LM3S9BXX_BIT_DMACHCTL_SIZE_HALF          1u
#define  LM3S9BXX_BIT_DMACHCTL_SIZE_WORD          2u

#define  LM3S9BXX_BIT_DMACHCTL_INC_BYTE           0u
#define  LM3S9BXX_BIT_DMACHCTL_INC_HALF           1u
#define  LM3S9BXX_BIT_DMACHCTL_INC_WORD           2u
#define  LM3S9BXX_BIT_DMACHCTL_INC_NONE           3u


/*
*********************************************************************************************************
*                                              MACRO'S
*********************************************************************************************************
*/

#define  LM3S9BXX_DMA_CH_CTRL_CFG(mode,      \
                                  xfer_size, \
                                  src_size,  \
                                  src_inc,   \
                                  dest_size, \
                                  dest_inc)      ((((mode          ) & DEF_BIT_FIELD( 3u, 0u))       ) | \
                                                  (((xfer_size - 1u) & DEF_BIT_FIELD(10u, 0u)) <<  4u) | \
                                                  (((src_size      ) & DEF_BIT_FIELD( 2u, 0u)) << 24u) | \
                                                  (((src_inc       ) & DEF_BIT_FIELD( 2u, 0u)) << 26u) | \
                                                  (((dest_size     ) & DEF_BIT_FIELD( 2u, 0u)) << 28u) | \
                                                  (((dest_inc      ) & DEF_BIT_FIELD( 2u, 0u)) << 30u))

#define  LM3S9BXX_DMA_CH_CTRL_GET_SIZE(ctrl)      (((ctrl     >> 4u) & DEF_BIT_FIELD(10u, 0u)))
#define  LM3S9BXX_DMA_CH_CTRL_GET_MODE(ctrl)      (((ctrl          ) & DEF_BIT_FIELD( 3u, 0u)))


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
*                   (e) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
*                       (2) Disable device interrupts
*                       (3) Disable device receiver and transmitter
*                       (4) Other necessary device initialization
*
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE            No error.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : (2) The received packet is organized in the LM3S9Bxx ethernet controller's FIFO as :
*
*           +=================+======+==================+
*           | FIFO Word Rd/Wr | Byte |      RX FIFO     |
*           |=================|======+==================+                          +=========+          -----
*           |       1st       |  0   | Frame Length LSB |                          | xxxxxxx | + 0x00     ^
*           |                 |  1   | Frame Length MSB |                          | xxxxxxx | + 0x01     |
*           |                 |  2   |    DA Octet 1    |     -----                | Octet 1 | + 0x02     |
*           |                 |  3   |    DA Octet 2    |       ^                  | Octet 2 | + 0x03     |
*           +=================|======|==================+       |       -----      |=========|            |
*           |       2st       |  0   |    DA Octet 3    |       |         ^        | Octet 3 | + 0x04     |
*           |                 |  1   |    DA Octet 4    |       |         |        | Octet 4 | + 0x05     |
*           |                 |  2   |    DA Octet 5    |       |         |        | Octet 5 | + 0x06     |
*           |                 |  3   |    DA Octet 6    |       |         |        | Octet 6 | + 0x07     |
*           +=================|======|==================+       |         |        |=========|            |
*                                .                                                 |    .    |
*                                .                           Ethernet    DMA       |    .    |         Network
*                                .                            Frame    Transfer    |    .    |         Buffer
*                                .                                                 |    .    |
*           +=================|======|==================+       |         |        |    .    |            |
*           |       Last      |  0   |      FCS 1a      |       |         |        |    .    |            |
*           |                 |  1   |      FCS 2a      |       |         |        |    .    |            |
*           |                 |  2   |      FCS 3a      |       |         |        |    .    |            |
*           |                 |  3   |      FCS 4a      |       v         v        |    .    |            v
*           +=================|======|==================+     -----     -----      +=========+          -----
*
*                   because the size of a received packet is not known until the header is examined,
*                   the frame size must be determined first. Once the packet length is known, a
*                   DMA transfer can be set up to transfer the remaining received packet payload from
*                   the FIFO into a buffer.
*
*                   Since DMA transfer MUST be 32-bit aligned then the network buffer must be 32-bit
*                   aligned and the data section should start at index 2.
*
*               (3) The application developer SHOULD define NetDev_CfgClk() within net_bsp.c
*                   in order to properly enable clocks for specified network interface.
*
*               (4) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.
*
*               (5) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
*                   in order to properly enable interrupts on an external or CPU-integrated
*                   interrupt controller.
*
*                   (a) External interrupt sources are cleared within the NetBSP first level
*                       ISR handler either before or after the call to the device driver ISR
*                       handler function.  The device driver ISR handler function SHOULD only
*                       clear the device specific interrupts and NOT external or CPU interrupt
*                       controller interrupt sources.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    NET_DEV_DMA        *pdev_dma;
    NET_DEV_DMA_CH     *pdma_ch_tbl;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_dma = (NET_DEV_DMA       *)NET_DEV_LM3S9Bxx_DMA_BASE_ADDR;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf alignment.                           */
    if (pdev_cfg->RxBufAlignOctets != CPU_WORD_SIZE_32) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Rx buf ix offset (see Note #2).             */
    if (pdev_cfg->RxBufIxOffset != RX_BUF_IX_OFFSET_OCTETS) {
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


                                                                /* Validate Tx buf alignment.                           */
    if (pdev_cfg->TxBufAlignOctets != CPU_WORD_SIZE_32) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf ix offset (see Note #2).             */
    if (pdev_cfg->TxBufIxOffset != TX_BUF_IX_OFFSET_OCTETS) {
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
                                                                /* Enable module clks (see Note #3).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* ----------- DMA CONTROLLER INITIALIZATION ---------- */
                                                                /* Use the primary channel assignment for Rx & Tx.      */
    DEF_BIT_CLR(pdev_dma->DMA_CH_ALT, DEF_BIT(NET_DEV_LM3S9Bxx_DMA_RX_CH));
    DEF_BIT_CLR(pdev_dma->DMA_CH_ALT, DEF_BIT(NET_DEV_LM3S9Bxx_DMA_TX_CH));

                                                                /* Disable Rx & Tx channel                              */
    pdev_dma->DMA_ENA_CLR = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_RX_CH)
                          | DEF_BIT(NET_DEV_LM3S9Bxx_DMA_TX_CH);

    pdma_ch_tbl = (NET_DEV_DMA_CH *)pdev_dma->DMA_CTL_BASE;     /* Get DMA ch ctrl tbl addr; MUST get AFTER CfgClk().   */
    if (pdma_ch_tbl == (NET_DEV_DMA_CH *)0) {                   /* If the table has not been created ...                */
                                                                /* ... allocate the table from the heap.                */
        pdma_ch_tbl  = (NET_DEV_DMA_CH *)Mem_HeapAlloc((CPU_SIZE_T   )(NET_DEV_LM3S9Bxx_DMA_NBR_CH * sizeof(NET_DEV_DMA_CH)),
                                                       (CPU_SIZE_T   )1024u,
                                                       (CPU_SIZE_T  *)&reqd_octets,
                                                       (LIB_ERR     *)&lib_err);
        if (pdma_ch_tbl == (void *)0) {
           *perr = NET_DEV_ERR_MEM_ALLOC;
            return;
        }

        pdev_dma->DMA_CTL_BASE = (CPU_INT32U)pdma_ch_tbl;       /* Set the table address.                               */
    }

                                                                /* ------------------- INIT DEV REGS ------------------ */
    pdev->MACIM   = 0u;                                         /* Dis all ints.                                        */
    pdev->MACRCTL = 0u;                                         /* Dis rx'er.                                           */
    pdev->MACTCTL = 0u;                                         /* Dis tx'er.                                           */


                                                                /* Initialize ext int ctrl'r (see Note #5).             */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* --------------- INIT DEV DATA AREA ----------------- */
    pdev_data->TxBufCompPtr                            = (CPU_INT08U *)0;
    pdev_data->RxBufCompPtr                            = (CPU_INT32U *)0u;

                                                                /* Abort Rx DMA transfers.                              */
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_RX_CH].DestEndPtr = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_RX_CH].SrcEndPtr  = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_RX_CH].Ctrl       = DEF_BIT_NONE;
    pdev_data->RxDMA_En                                = DEF_DISABLED;
    pdev_data->RxPktLen                                = 0u;
                                                                /* Abort Tx DMA transfers.                              */
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_TX_CH].DestEndPtr = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_TX_CH].SrcEndPtr  = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_TX_CH].Ctrl       = DEF_BIT_NONE;
    pdev_data->TxDMA_En                                = DEF_DISABLED;

   *perr                                               = NET_DEV_ERR_NONE;
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
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (2) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, some devices can queue multiple frames for
*                   transmission before blocking is required.  The default semaphore value is one.
*
*               (3) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_Ether_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (4) The device hardware address is set from one of the data sources below. Each source
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

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_DMA        *pdev_dma;
    CPU_INT08U         *pbuf;
    CPU_INT32U          clk_freq;
    CPU_INT32U          div;
    CPU_INT32U          reg_val;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEV/PHY CFGs/DATA/REGs/BSP -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev_dma  = (NET_DEV_DMA       *)NET_DEV_LM3S9Bxx_DMA_BASE_ADDR;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp  = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;



                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #2.                                         */
                             1u,
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
            hw_addr[0] = (pdev->MACIA0 >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->MACIA0 >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->MACIA0 >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->MACIA0 >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->MACIA1 >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->MACIA1 >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->MACIA0 = ((((CPU_INT32U)hw_addr[0]) << (0 * DEF_INT_08_NBR_BITS)) |
                        (((CPU_INT32U)hw_addr[1]) << (1 * DEF_INT_08_NBR_BITS)) |
                        (((CPU_INT32U)hw_addr[2]) << (2 * DEF_INT_08_NBR_BITS)) |
                        (((CPU_INT32U)hw_addr[3]) << (3 * DEF_INT_08_NBR_BITS)));

        pdev->MACIA1 = ((((CPU_INT32U)hw_addr[4]) << (0 * DEF_INT_08_NBR_BITS)) |
                        (((CPU_INT32U)hw_addr[5]) << (1 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* --------------------- CFG MAC ---------------------- */
    pdev->MACIM = 0u;                                           /* Dis all ints.                                        */
    reg_val     = pdev->MACIS;                                  /* Clr all ints.                                        */
    pdev->MACIS = reg_val;


                                                                /* Init clk div.                                        */
    clk_freq = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }

    div             = (clk_freq / 2u) / 2500000u;
    pdev->MACMDV    = (div & LM3S9BXX_BIT_MACMDV_DIV);


                                                                /* ------------------ CFG TX'R & RX'R ----------------- */
    pdev->MACTCTL   = LM3S9BXX_BIT_MACTCTL_PADEN                /* Cfg rx'r.                                            */
                    | LM3S9BXX_BIT_MACTCTL_DUPLEX
                    | LM3S9BXX_BIT_MACTCTL_CRC;
    pdev->MACRCTL   = LM3S9BXX_BIT_MACRCTL_BADCRC;              /* Cfg tx'r                                             */



                                                                /* --------------------- CFG RX DMA ------------------- */
                                                                /* Get Rx buffer for DMA operations.                    */
    pbuf = NetBuf_GetDataPtr((NET_IF        *)pif,
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

    pdev_data->RxBufCompPtr = (CPU_INT32U *)pbuf;
    pdev_dma->DMA_CFG       = LM3S9BXX_BIT_DMA_CFG_MASTEN;      /* Enable DMA controller                                */
    pdev_data->RxPktLen     = 0u;
    pdev_data->RxDMA_En     = DEF_DISABLED;

                                                                /* ------------------ EN TX'R & RX'R ------------------ */
    pdev->MACRCTL  |= LM3S9BXX_BIT_MACRCTL_RSTFIFO;             /* Reset the EMAC FIFO.                                 */
    pdev->MACRCTL  |= LM3S9BXX_BIT_MACRCTL_RXEN;                /* En rx'r.                                             */
    pdev->MACTCTL  |= LM3S9BXX_BIT_MACTCTL_TXEN;                /* En tx'r.                                             */
    pdev->MACRCTL  |= LM3S9BXX_BIT_MACRCTL_RSTFIFO;             /* Reset the EMAC FIFO.                                 */

    pdev->MACIM    |= LM3S9BXX_BIT_MACIM_RXINT                  /* En rx & tx ints.                                     */
                    | LM3S9BXX_BIT_MACIM_FOV
                    | LM3S9BXX_BIT_MACIM_RXER
                    | LM3S9BXX_BIT_MACIM_TXER
                    | LM3S9BXX_BIT_MACIM_TXEMP;

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
*               perr    Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE    No error.
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
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_DMA        *pdev_dma;
    NET_DEV_DMA_CH     *pdma_ch_tbl;
    NET_DEV            *pdev;
    NET_ERR             err;


    pdev_cfg    = (NET_DEV_CFG_ETHER  *)pif->Dev_Cfg;
    pdev        = (NET_DEV            *)pdev_cfg->BaseAddr;
    pdev_data   = (NET_DEV_DATA       *)pif->Dev_Data;
    pdev_data   = (NET_DEV_DATA       *)pif->Dev_Data;
    pdev_dma    = (NET_DEV_DMA        *)NET_DEV_LM3S9Bxx_DMA_BASE_ADDR;

    pdma_ch_tbl = (NET_DEV_DMA_CH *)(pdev_dma->DMA_CTL_BASE);


                                                                /* ------------------- CLR DEV REGS ------------------- */
    pdev->MACIM   = 0u;                                         /* Dis all ints.                                        */
    pdev->MACRCTL = 0u;                                         /* Dis rx'er.                                           */
    pdev->MACTCTL = 0u;                                         /* Dis tx'er.                                           */

                                                                /* ---------------------- STOP DMA -------------------- */
                                                                /* Abort Rx DMA transfers.                              */
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_RX_CH].DestEndPtr = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_RX_CH].SrcEndPtr  = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_RX_CH].Ctrl       = DEF_BIT_NONE;
    pdev_data->RxDMA_En                                = DEF_DISABLED;
    pdev_data->RxPktLen                                = 0u;
                                                                /* Abort Tx DMA transfers.                              */
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_TX_CH].DestEndPtr = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_TX_CH].SrcEndPtr  = (CPU_REG32  )0u;
    pdma_ch_tbl[NET_DEV_LM3S9Bxx_DMA_TX_CH].Ctrl       = DEF_BIT_NONE;
    pdev_data->TxDMA_En                                = DEF_DISABLED;

                                                                /* Disable Rx/Tx Channels.                              */
    pdev_dma->DMA_ENA_CLR = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_RX_CH);
    pdev_dma->DMA_ENA_CLR = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_TX_CH);

                                                                /* ------------------ FREE RX/TX BUFS ----------------- */
    if (pdev_data->TxBufCompPtr != (CPU_INT08U *)0) {           /* If NOT yet  tx'd, ...                                */
        NetIF_TxDeallocTaskPost(pdev_data->TxBufCompPtr, &err); /* ... dealloc tx buf (see Note #2a1).                  */
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdev_data->TxBufCompPtr = (CPU_INT08U *)0;
    }

    if (pdev_data->RxBufCompPtr != (CPU_INT32U *)0) {
        NetBuf_FreeBufDataAreaRx((NET_IF_NBR  )pif->Nbr,
                                 (CPU_INT08U *)pdev_data->RxBufCompPtr);
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Determine packet size.
*                   (b) Obtain pointer to buffer.
*                   (c) Read packet from FIFO into buffer.
*                   (d) Check if another packet has been received.
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
*               perr    Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE            No Error
*                           NET_DEV_ERR_RX              Generic Rx error.
*                           NET_DEV_ERR_INVALID_SIZE    Invalid Rx frame size.
*                           NET_BUF error codes         Potential NET_BUF error codes
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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U         *pbuf_new;
    CPU_INT16U          len;


    pdev_cfg  = (NET_DEV_CFG_ETHER  *)pif->Dev_Cfg;
    pdev      = (NET_DEV            *)pdev_cfg->BaseAddr;
    pdev_data = (NET_DEV_DATA       *)pif->Dev_Data;

                                                                /* ------------------- GET PKT SIZE ------------------- */
    len = pdev_data->RxPktLen;

    if (len >= NET_IF_ETHER_FRAME_CRC_SIZE) {
        len -= NET_IF_ETHER_FRAME_CRC_SIZE;
        pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                     (NET_BUF_SIZE   )NET_IF_IX_RX,
                                     (NET_BUF_SIZE  *)0,
                                     (NET_BUF_SIZE  *)0,
                                     (NET_BUF_TYPE  *)0,
                                     (NET_ERR       *)perr);

        if (*perr != NET_BUF_ERR_NONE) {
            *size   = (CPU_INT16U  )0;                          /* Clr size & buf ptr.                                  */
            *p_data = (CPU_INT08U *)0;
        } else {
            *p_data                  = (CPU_INT08U *)pdev_data->RxBufCompPtr;
             pdev_data->RxBufCompPtr = (CPU_INT32U *)pbuf_new;
            *size                    = len;
            *perr                    = NET_DEV_ERR_NONE;
        }
    } else {
       *perr = NET_DEV_ERR_RX;
    }
                                                                /* Enable Rx interrupts.                                */
    DEF_BIT_SET(pdev->MACIM, LM3S9BXX_BIT_MACIM_RXINT | LM3S9BXX_BIT_MACIM_RXER);
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Check if the transmitter is ready.
*                   (b) Write packet from buffer into FIFO.
*                   (c) Issue the transmit command.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE        No error.
*                           NET_DEV_ERR_TX_BUSY     Transmitter is busy.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pdev_api->Tx()'.
*
* Note(s)     : (2) Software MUST track all transmit buffer addresses that are that are queued for
*                   transmission but have not received a transmit complete notification (interrupt).
*                   Once the frame has been transmitted, software must post the buffer address of
*                   the frame that has completed transmission to the transmit deallocation task.
*                   See NetDev_ISR_Handler() for more information.
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
    NET_DEV_DMA        *pdev_dma;
    NET_DEV_DMA_CH     *pdma_ch_tbl;
    CPU_INT32U         *p_data_32;
    CPU_INT16U          len;
    CPU_INT32U          reg_val;
    CPU_SR_ALLOC();


    pdev_cfg      = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev          = (NET_DEV           *)pdev_cfg->BaseAddr;
    pdev_data     = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev_dma      = (NET_DEV_DMA       *)NET_DEV_LM3S9Bxx_DMA_BASE_ADDR;
    pdma_ch_tbl   = (NET_DEV_DMA_CH    *)(pdev_dma->DMA_CTL_BASE + (NET_DEV_LM3S9Bxx_DMA_TX_CH * sizeof(NET_DEV_DMA_CH)));


                                                                /* ---------------------- TX PKT ---------------------- */
                                                                /* Form first word of FIFO data.                        */
    reg_val       = size                                        /* Length = size                                        */
                  - 2u                                          /*        - 2 (for len/type code)                       */
                  - NET_IF_ETHER_ADDR_SIZE                      /*        - 6 (for sender MAC)                          */
                  - NET_IF_ETHER_ADDR_SIZE;                     /*        - 6 (for dest.  MAC).                         */

                                                                /* Copy first 2 octets to the Tx FIFO.                  */
    reg_val      |= (CPU_INT32U)(MEM_VAL_GET_INT16U_LITTLE(p_data) << 16u);

    pdev->MACDATA = reg_val;
    len           = (((size - 2u) + 3u) / 4u);                  /* Get DMA transfer size in words                       */

    p_data_32     = (CPU_INT32U *)p_data;
    p_data_32     += len;

    CPU_CRITICAL_ENTER();

    pdev_data->TxBufCompPtr = (CPU_INT08U *)p_data;
    pdma_ch_tbl->SrcEndPtr  = (CPU_REG32   )p_data_32;
    pdma_ch_tbl->DestEndPtr = (CPU_REG32   )&pdev->MACDATA;
    pdma_ch_tbl->Ctrl       = LM3S9BXX_DMA_CH_CTRL_CFG(LM3S9BXX_BIT_DMACHCTL_XFERMODE_AUTO,
                                                       len,
                                                       LM3S9BXX_BIT_DMACHCTL_SIZE_WORD,
                                                       LM3S9BXX_BIT_DMACHCTL_INC_WORD,
                                                       LM3S9BXX_BIT_DMACHCTL_SIZE_WORD,
                                                       LM3S9BXX_BIT_DMACHCTL_INC_NONE);
    pdev_data->TxDMA_En   = DEF_ENABLED;                        /* Set DMA status variable to enabled.                  */

    CPU_CRITICAL_EXIT();
                                                                /* Enable DMA channel & perform SW request.             */
    pdev_dma->DMA_ENA_SET = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_TX_CH);
    pdev_dma->DMA_SWREQ   = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_TX_CH);
    pdev->MACTR           = LM3S9BXX_BIT_MACTR_NEWTX;           /* Start tx.                                            */


    *perr = NET_DEV_ERR_NONE;
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
    CPU_SR_ALLOC();


    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;

    CPU_CRITICAL_ENTER();
    pdev_data->RxMulticastCtr++;
    DEF_BIT_SET(pdev->MACRCTL, LM3S9BXX_BIT_MACRCTL_AMUL);      /* Enable multicast frames.                             */
    CPU_CRITICAL_EXIT();

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warning.          */
#endif

   (void)&paddr_hw;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&addr_hw_len;

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
    CPU_SR_ALLOC();


    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;

    CPU_CRITICAL_ENTER();
    if (pdev_data->RxMulticastCtr > 0u) {
        pdev_data->RxMulticastCtr--;
    }

    if (pdev_data->RxMulticastCtr == 0u) {
        DEF_BIT_CLR(pdev->MACRCTL, LM3S9BXX_BIT_MACRCTL_AMUL);  /* Disable multicast frames.                            */
    }
    CPU_CRITICAL_EXIT();

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warning.          */
#endif

   (void)&paddr_hw;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&addr_hw_len;

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
*
*                           NET_DEV_ISR_TYPE_RX
*                           NET_DEV_ISR_TYPE_TX_COMPLETE
*                           NET_DEV_ISR_TYPE_UNKNOWN
*
* Return(s)   : none.
*
* Caller(s)   : Specific first- or second-level BSP ISR handler.
*
* Note(s)     : (1) This function is called via function pointer from the context of an ISR.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_DMA        *pdev_dma;
    NET_DEV_DMA_CH     *pdma_ch_tbl;
    NET_DEV            *pdev;
    CPU_INT08U         *pdata;
    NET_ERR             err;
    CPU_INT32U          status;
    CPU_INT16U          len;
    CPU_INT32U          reg_val;


   (void)&type;                                                 /* Prevent compiler warning.                            */

    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev         = (NET_DEV           *)pdev_cfg->BaseAddr;     /* Overlay dev reg struct on top of dev base addr.      */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev_dma     = (NET_DEV_DMA       *)NET_DEV_LM3S9Bxx_DMA_BASE_ADDR;

    status       =  pdev->MACIS;                                /* Rd  the int status.                                  */
    status      &=  pdev->MACIM;
    pdev->MACIS  =  status;                                     /* Clr the int status.                                  */

                                                                /* Rx overrun FIFO.                                     */
    if (DEF_BIT_IS_SET(status, LM3S9BXX_BIT_MACIS_FOV) == DEF_YES) {
        pdev->MACRCTL  |= LM3S9BXX_BIT_MACRCTL_RSTFIFO;         /* Reset the EMAC FIFO.                                 */
        pdev->MACRCTL  |= LM3S9BXX_BIT_MACRCTL_RXEN;            /* En rx'r.                                             */
        pdev->MACTCTL  |= LM3S9BXX_BIT_MACTCTL_TXEN;            /* En tx'r.                                             */
        pdev->MACRCTL  |= LM3S9BXX_BIT_MACRCTL_RSTFIFO;         /* Reset the EMAC FIFO.                                 */
                                                                /* Abort any DMA pending transfer.                      */
        pdev_dma->DMA_ENA_CLR = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_RX_CH);
        pdev_data->RxPktLen   = 0u;
        pdev_data->RxDMA_En   = DEF_DISABLED;

        DEF_BIT_SET(pdev->MACIM, LM3S9BXX_BIT_MACIM_RX_ALL);
        DEF_BIT_CLR(status, LM3S9BXX_BIT_MACIM_RX_ALL);
    }
                                                                /* Tx error handler                                     */
    if (DEF_BIT_IS_SET(status, LM3S9BXX_BIT_MACIM_TXER) == DEF_YES) {
        DEF_BIT_CLR(pdev->MACTR, LM3S9BXX_BIT_MACTR_NEWTX);     /* Disable transmitter.                                 */

        if (pdev_data->TxBufCompPtr != (CPU_INT08U *)0) {       /* Deallocate Tx buffer.                                */
            NetIF_TxDeallocTaskPost(pdev_data->TxBufCompPtr, &err);
            pdev_data->TxBufCompPtr = (CPU_INT08U *)0;
        }

        NetIF_DevCfgTxRdySignal(pif,                            /* Configure the ready signal.                          */
                                 1u,
                                 &err);
        DEF_BIT_CLR(status, LM3S9BXX_BIT_MACIS_TXEMP);
    }


    if (DEF_BIT_IS_CLR(pdev->MACIM, (LM3S9BXX_BIT_MACIM_RXINT | LM3S9BXX_BIT_MACIM_RXER)) &&
       (pdev_data->RxDMA_En == DEF_ENABLED)) {

                                                                /* Get the DMA Rx channel control table entry.          */
        pdma_ch_tbl = (NET_DEV_DMA_CH *)(pdev_dma->DMA_CTL_BASE + (NET_DEV_LM3S9Bxx_DMA_RX_CH * sizeof(NET_DEV_DMA_CH)));

                                                                /* If the DMA transfer is completed ...                 */
        if ((LM3S9BXX_DMA_CH_CTRL_GET_SIZE(pdma_ch_tbl->Ctrl) == 0u                                 ) &&
            (LM3S9BXX_DMA_CH_CTRL_GET_MODE(pdma_ch_tbl->Ctrl) == LM3S9BXX_BIT_DMACHCTL_XFERMODE_STOP)) {

             pdev_data->RxDMA_En = DEF_DISABLED;                /* Set DMA status variable to disabled.                 */

             if (pdev_data->RxPktLen != 0u) {                   /* If a valid DMA packet transfer is completed          */
                                                                /* ... disable Rx interrupts.                           */
                DEF_BIT_CLR(pdev->MACIM, LM3S9BXX_BIT_MACIM_RXINT | LM3S9BXX_BIT_MACIM_RXER);
                NetIF_RxTaskSignal(pif->Nbr, &err);             /* Signal Net IF RxQ Task for new rdy pkt.              */
             } else {
                 DEF_BIT_SET(pdev->MACIM, LM3S9BXX_BIT_MACIM_RX_ALL);
             }
        }
    }

    if (DEF_BIT_IS_SET(status, LM3S9BXX_BIT_MACIS_RXINT) == DEF_YES) {

        if (pdev_data->RxDMA_En == DEF_DISABLED) {              /* If DMA transfer is not enabled ...                    */
            reg_val = pdev->MACDATA;                            /* Read first word in the Rx FIFO.                       */
            len     = (CPU_INT16U)(reg_val & DEF_INT_16_MASK);  /* Get the packet size.                                  */

            if (len >  4u) {                                    /* Decrement packet size by 4( 2 len + 2 Data).          */
                len -= 4u;
            }

            pdma_ch_tbl             = (NET_DEV_DMA_CH *)(pdev_dma->DMA_CTL_BASE + (NET_DEV_LM3S9Bxx_DMA_RX_CH * sizeof(NET_DEV_DMA_CH)));
            pdev_data->RxPktLen     = (CPU_INT16U      )  (len + 2u);
            len                     = (CPU_INT16U      )(((len + 3u) / 4u));


            if (len > pdev_cfg->RxBufLargeSize) {               /* If the packet size is bigger than the Rx large buffer */
                pdev_data->RxPktLen     = 0;
                                                                /* ... Perform a 'dummy' read to clear the FIFO          */
                                                                /* ... End Source      Ptr = Rx FIFO (MACDATA).          */
                                                                /* ... End Destination Ptr = RxBufCompPtr.               */
                                                                /* ... No increment in both.                             */
                pdma_ch_tbl->SrcEndPtr  = (CPU_REG32 )(&pdev->MACDATA);
                pdma_ch_tbl->DestEndPtr = (CPU_REG32 )(pdev_data->RxBufCompPtr);
                pdma_ch_tbl->Ctrl       = LM3S9BXX_DMA_CH_CTRL_CFG(LM3S9BXX_BIT_DMACHCTL_XFERMODE_AUTO,
                                                                   len,
                                                                   LM3S9BXX_BIT_DMACHCTL_SIZE_WORD,
                                                                   LM3S9BXX_BIT_DMACHCTL_INC_NONE,
                                                                   LM3S9BXX_BIT_DMACHCTL_SIZE_WORD,
                                                                   LM3S9BXX_BIT_DMACHCTL_INC_NONE);
            } else {
                                                                /* Copy first two bytes from the packet into the buffer  */
                pdev_data->RxBufCompPtr[0] = (CPU_INT32U )reg_val;

                                                                /* ... End Source      Ptr = Rx FIFO (MACDATA).          */
                                                                /* ... End Destination Ptr = RxBufCompPtr + 1 + len - 1  */
                                                                /* ... No increment in Rx FIFO.                          */
                pdma_ch_tbl->DestEndPtr    = (CPU_REG32  )(pdev_data->RxBufCompPtr + len);
                pdma_ch_tbl->SrcEndPtr     = (CPU_REG32  )(&pdev->MACDATA);
                pdma_ch_tbl->Ctrl          = LM3S9BXX_DMA_CH_CTRL_CFG(LM3S9BXX_BIT_DMACHCTL_XFERMODE_AUTO,
                                                                      len,
                                                                      LM3S9BXX_BIT_DMACHCTL_SIZE_WORD,
                                                                      LM3S9BXX_BIT_DMACHCTL_INC_NONE,
                                                                      LM3S9BXX_BIT_DMACHCTL_SIZE_WORD,
                                                                      LM3S9BXX_BIT_DMACHCTL_INC_WORD);
            }

            pdev_data->RxDMA_En   = DEF_ENABLED;                /* Set DMA status variable to enabled.                  */
                                                                /* Enable DMA channel & perform SW request.             */
            pdev_dma->DMA_ENA_SET = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_RX_CH);
            pdev_dma->DMA_SWREQ   = DEF_BIT(NET_DEV_LM3S9Bxx_DMA_RX_CH);
                                                                /* Disable Interrupts.                                  */
            DEF_BIT_CLR(pdev->MACIM, LM3S9BXX_BIT_MACIM_RXINT | LM3S9BXX_BIT_MACIM_RXER);
        }
    }
                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if (DEF_BIT_IS_SET(status, LM3S9BXX_BIT_MACIS_TXEMP) == DEF_YES) {
         pdma_ch_tbl = (NET_DEV_DMA_CH *)(pdev_dma->DMA_CTL_BASE + (NET_DEV_LM3S9Bxx_DMA_TX_CH * sizeof(NET_DEV_DMA_CH)));

                                                                /* If the DMA transfer is completed ...                 */
         if ((LM3S9BXX_DMA_CH_CTRL_GET_SIZE(pdma_ch_tbl->Ctrl) == 0u                                 ) &&
            (LM3S9BXX_DMA_CH_CTRL_GET_MODE(pdma_ch_tbl->Ctrl) == LM3S9BXX_BIT_DMACHCTL_XFERMODE_STOP)) {

             pdata                   =  pdev_data->TxBufCompPtr;
             pdev_data->TxBufCompPtr = (CPU_INT08U *)0;
             NetIF_TxDeallocTaskPost(pdata, &err);              /* Deallocate the transmit buffer.                      */
             NetIF_DevTxRdySignal(pif);                         /* Signal Net IF that tx resources are available.       */
             pdev_data->TxDMA_En   = DEF_DISABLED;
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
*               perr    Pointer to variable that will receive the return error code from this function :
*
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
* Note(s)     : none.
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


    pdev_cfg =  pif->Dev_Cfg;
    pdev     = (NET_DEV *)pdev_cfg->BaseAddr;                   /* Overlay dev reg struct on top of dev base addr.      */



                                                                /* ----------- PERFORM SPECIFIED OPERATION ------------ */
    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;
             if (plink_state == (NET_DEV_LINK_ETHER *)0) {
                *perr = NET_DEV_ERR_NULL_PTR;
                 return;
             }

             pphy_api = pif->Ext_API;
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

             switch (plink_state->Duplex) {                     /* Update duplex register setting in MAC.               */
                case NET_PHY_DUPLEX_FULL:
                     pdev->MACTCTL |=  LM3S9BXX_BIT_MACTCTL_DUPLEX;
                     break;

                case NET_PHY_DUPLEX_HALF:
                     pdev->MACTCTL &= ~LM3S9BXX_BIT_MACTCTL_DUPLEX;
                     break;

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
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                               NET_PHY_ERR_NONE                MII write completed successfully.
*                               NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
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
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          reg_to;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr = NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

   (void)&phy_addr;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&reg_addr;

    pdev_cfg =  pif->Dev_Cfg;
    pdev     = (NET_DEV *)pdev_cfg->BaseAddr;

    reg_to   = MII_REG_RD_WR_TO;
    while ((DEF_BIT_IS_SET(pdev->MACMCTL, LM3S9BXX_BIT_MACMCTL_START) == DEF_YES) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr =  NET_PHY_ERR_TIMEOUT_REG_WR;
         return;
    }

    pdev->MACMCTL = (reg_addr << 3) | LM3S9BXX_BIT_MACMCTL_START;

    reg_to   = MII_REG_RD_WR_TO;

    while ((DEF_BIT_IS_SET(pdev->MACMCTL, LM3S9BXX_BIT_MACMCTL_START) == DEF_YES) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr =  NET_PHY_ERR_TIMEOUT_REG_WR;
         return;
    }

   *p_data = pdev->MACMRXD & DEF_INT_16_MASK;
   *perr   = NET_PHY_ERR_NONE;
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
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE                MII write completed successfully.
*                               NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
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
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          reg_to;


   (void)&phy_addr;                                             /* Prevent 'variable unused' compiler warning.         */

    pdev_cfg =  pif->Dev_Cfg;
    pdev     = (NET_DEV *)pdev_cfg->BaseAddr;

    reg_to   = MII_REG_RD_WR_TO;
    while ((DEF_BIT_IS_SET(pdev->MACMCTL, LM3S9BXX_BIT_MACMCTL_START) == DEF_YES) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr =  NET_PHY_ERR_TIMEOUT_REG_WR;
         return;
    }

    pdev->MACMTXD =  data;
    pdev->MACMCTL = (reg_addr << 3) | LM3S9BXX_BIT_MACMCTL_WRITE | LM3S9BXX_BIT_MACMCTL_START;

    reg_to   = MII_REG_RD_WR_TO;
    while ((DEF_BIT_IS_SET(pdev->MACMCTL, LM3S9BXX_BIT_MACMCTL_START) == DEF_YES) &&
           (reg_to > 0u)) {
        reg_to--;
    }

    if (reg_to == 0u) {
        *perr =  NET_PHY_ERR_TIMEOUT_REG_WR;
         return;
    }


   *perr = NET_PHY_ERR_NONE;
}


#endif /* NET_IF_ETHER_MODULE_EN */

