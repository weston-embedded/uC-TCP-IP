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
*                                           Atmel MACB on the
*                                              AVR AP7000
*
* Filename : net_dev_macb_ap7000.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_MACB_AP7000_MODULE

#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_macb_AP7000.h"

#ifdef  NET_IF_ETHER_MODULE_EN


#if defined(__ICCAVR32__)
#include  <intrinsics.h>
#endif


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO               10000                   /* MII read write timeout.                              */
#define  RX_DESC_BUF_SIZE                 128                   /* Rx descriptor   buffers are ALWAYS 128 bytes.        */
#define  RX_DESC_BUF_ALIGN                 32                   /* Rx descriptor & buffers are ALWAYS aligned 32 bytes. */
#define  TX_DESC_BUF_ALIGN                  8                   /* Tx descriptor & buffers SHOULD be aligned 8 bytes.   */

#define  CFG_DCACHE_LINESZ                 32                   /* Cfg d-cache line size.                               */
#define  USE_BARRIER                        1                   /* Use memory barriers. Applies to drv instances.       */
#define  USE_CACHE_FLUSH                    1                   /* Use cache flushing.  Applies to drv instances.       */

#define  P1SEG                     0x80000000
#define  P2SEG                     0xA0000000


/*
*********************************************************************************************************
*                                             LOCAL MACROS
*********************************************************************************************************
*/

#define  INLINE         inline

#if (USE_BARRIER > 0)
#define BARRIER()       Sync_Write_Buffer()
#else
#define BARRIER()
#endif

#define ADDR_PHYS(a) (((CPU_INT32U)(a)) & 0x1FFFFFFF)           /* Obtain phys addr of PnSEG addr.  Where n = 1, 2.     */

#define P1SEGADDR(a)   (ADDR_PHYS(a) | P1SEG)
#define P2SEGADDR(a)   (ADDR_PHYS(a) | P2SEG)

#define ADDR_CACHED(addr)   ((void *)P1SEGADDR(addr))
#define ADDR_UNCACHED(addr) ((void *)P2SEGADDR(addr))


/*
*********************************************************************************************************
*                                           LOCAL DATA TYPES
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

                                                                /* ------------- DMA DESCRIPTOR DATA TYPE ------------- */
typedef  struct  dev_desc {                                     /* See Note #2.                                         */
  CPU_INT32U  Addr;                                             /* Buffer address.                                      */
  CPU_INT32U  Status;                                           /* Buffer status.                                       */
} DEV_DESC;

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR      ErrRxPktOwner1;
    NET_CTR      ErrRxPktOwner2;
    NET_CTR      ErrRxPktNotSOF;
    NET_CTR      ErrRxPktInvalidSOF;
    NET_CTR      ErrRxPktGetDataPtr;
#endif

    DEV_DESC     *RxBufDescPtrStartRegAddr;                     /* Rx desc start addr used to cfg MACB hw.              */
    DEV_DESC     *TxBufDescPtrStartRegAddr;                     /* Tx desc start addr used to cfg MACB hw.              */

    DEV_DESC     *RxBufDescPtrStart;
    DEV_DESC     *RxBufDescPtrEnd;
    DEV_DESC     *RxBufDescPtrCur;
    DEV_DESC     *TxBufDescPtrStart;
    DEV_DESC     *TxBufDescPtrEnd;
    DEV_DESC     *TxBufDescPtrCur;
    DEV_DESC     *TxBufDescPtrComp;                             /* See Note #3.                                         */

    CPU_INT16U    RxNRdyCtr;
    CPU_INT08U   *RxDescBufs;                                   /* Pointer to local 128 byte buffers.                   */
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U    MulticastAddrHashBitCtr[64];
#endif
} NET_DEV_DATA;


enum dma_data_direction {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE     = 1,
    DMA_FROM_DEVICE   = 2
};


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) Global variables are highly discouraged and should only be used for storing NON
*               instance specific data and the array of instance specific data.  Global
*               variables, those that are not declared within the NET_DEV_DATA area, are not
*               multiple-instance safe and could lead to incorrect driver operation if used to
*               store device state information.
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
static          void  NetDev_Init               (NET_IF             *pif,
                                                 NET_ERR            *perr);

static          void  NetDev_Start              (NET_IF             *pif,
                                                 NET_ERR            *perr);


static          void  NetDev_Stop               (NET_IF             *pif,
                                                 NET_ERR            *perr);

static          void  NetDev_Rx                 (NET_IF             *pif,
                                                 CPU_INT08U        **p_data,
                                                 CPU_INT16U         *size,
                                                 NET_ERR            *perr);

static          void  NetDev_Tx                 (NET_IF             *pif,
                                                 CPU_INT08U         *p_data,
                                                 CPU_INT16U          size,
                                                 NET_ERR            *perr);

static          void  NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                                 CPU_INT08U         *paddr_hw,
                                                 CPU_INT08U          addr_hw_len,
                                                 NET_ERR            *perr);

static          void  NetDev_AddrMulticastRemove(NET_IF             *pif,
                                                 CPU_INT08U         *paddr_hw,
                                                 CPU_INT08U          addr_hw_len,
                                                 NET_ERR            *perr);

static          void  NetDev_ISR_Handler        (NET_IF             *pif,
                                                 NET_DEV_ISR_TYPE    type);

static          void  NetDev_IO_Ctrl            (NET_IF             *pif,
                                                 CPU_INT08U          opt,
                                                 void               *p_data,
                                                 NET_ERR            *perr);

static          void  NetDev_MII_Rd             (NET_IF             *pif,
                                                 CPU_INT08U          phy_addr,
                                                 CPU_INT08U          reg_addr,
                                                 CPU_INT16U         *p_data,
                                                 NET_ERR            *perr);

static          void  NetDev_MII_Wr             (NET_IF             *pif,
                                                 CPU_INT08U          phy_addr,
                                                 CPU_INT08U          reg_addr,
                                                 CPU_INT16U          data,
                                                 NET_ERR            *perr);
                                                                    /* --------- FNCT'S COMMON TO DMA BASED DRV'S --------- */
static          void  NetDev_RxDescInit         (NET_IF             *pif,
                                                 NET_ERR            *perr);

static          void  NetDev_TxDescInit         (NET_IF             *pif,
                                                 NET_ERR            *perr);

static          void   NetDev_DescAddrTranslate (NET_IF             *pif);

static          void   DCache_Invalidate_Range  (volatile  void     *pmem,
                                                 CPU_INT32U          size);

static  INLINE  void  *DMA_Map_Single           (volatile  void     *pvaddr,
                                                 CPU_INT32U          len,
                                                 CPU_INT08U          dir);

static  INLINE  void   DCache_Flush_Unlocked    (void);

static  INLINE  void   Sync_Write_Buffer        (void);


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
                                                                                    /* MACB-AP7000 dev API fnct ptrs :  */
const  NET_DEV_API_ETHER  NetDev_API_MACB_AP7000 = { NetDev_Init,                   /*   Init/add                       */
                                                     NetDev_Start,                  /*   Start                          */
                                                     NetDev_Stop,                   /*   Stop                           */
                                                     NetDev_Rx,                     /*   Rx                             */
                                                     NetDev_Tx,                     /*   Tx                             */
                                                     NetDev_AddrMulticastAdd,       /*   Multicast addr add             */
                                                     NetDev_AddrMulticastRemove,    /*   Multicast addr remove          */
                                                     NetDev_ISR_Handler,            /*   ISR handler                    */
                                                     NetDev_IO_Ctrl,                /*   I/O ctrl                       */
                                                     NetDev_MII_Rd,                 /*   Phy reg rd                     */
                                                     NetDev_MII_Wr                  /*   Phy reg wr                     */
                                                   };

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
                                                                /* -------------- CONFIGURATION REGISTERS ------------- */
    CPU_REG32  MACB_NCR;                                        /* EMAC Control Register.                               */
    CPU_REG32  MACB_NCFGR;                                      /* EMAC Configuration Register.                         */
    CPU_REG32  MACB_NSR;                                        /* EMAC Status Register.                                */
    CPU_REG32  RESERVED1;                                       /* RESERVED.                                            */
    CPU_REG32  RESERVED2;                                       /* RESERVED.                                            */
    CPU_REG32  MACB_TSR;                                        /* EMAC Transmit Status Register.                       */
    CPU_REG32  MACB_RBQP;                                       /* EMAC Receive  Buffer Queue Pointer.                  */
    CPU_REG32  MACB_TBQP;                                       /* EMAC Transmit Buffer Queue Pointer.                  */
    CPU_REG32  MACB_RSR;                                        /* EMAC Receive Status Register.                        */
    CPU_REG32  MACB_ISR;                                        /* EMAC Interrupt Status  Register.                     */
    CPU_REG32  MACB_IER;                                        /* EMAC Interrupt Enable  Register.                     */
    CPU_REG32  MACB_IDR;                                        /* EMAC Interrupt Disable Register.                     */
    CPU_REG32  MACB_IMR;                                        /* EMAC Interrupt Mask    Register.                     */
    CPU_REG32  MACB_MAN;                                        /* EMAC PHY Maintenance   Register.                     */
    CPU_REG32  MACB_PTR;                                        /* EMAC Pause Time        Register.                     */

                                                                /* ---------------- STATISTIC REGISTERS --------------- */
    CPU_REG32  MACB_PFR;                                        /* Pause Frames Received Register.                      */
    CPU_REG32  MACB_FTO;                                        /* Frames Transmitted OK Register.                      */
    CPU_REG32  MACB_SCF;                                        /* Single   Collision Frame Register.                   */
    CPU_REG32  MACB_MCF;                                        /* Multiple Collision Frame Register.                   */
    CPU_REG32  MACB_FRO;                                        /* Frames Received    OK Register.                      */
    CPU_REG32  MACB_FCSE;                                       /* Frame Check Sequence Error Register.                 */
    CPU_REG32  MACB_ALE;                                        /* Alignment Error Register.                            */
    CPU_REG32  MACB_DTF;                                        /* Deferred Transmission Frame Register.                */
    CPU_REG32  MACB_LCOL;                                       /* Late      Collision Register.                        */
    CPU_REG32  MACB_ECOL;                                       /* Excessive Collision Register.                        */
    CPU_REG32  MACB_TUND;                                       /* Transmit Underrun Error Register.                    */
    CPU_REG32  MACB_CSE;                                        /* Carrier Sense     Error Register.                    */
    CPU_REG32  MACB_RRE;                                        /* Receive Resource  Error Register.                    */
    CPU_REG32  MACB_ROV;                                        /* Receive Jabber Register.                             */
    CPU_REG32  MACB_RSE;                                        /* Undersize Frame Register.                            */
    CPU_REG32  MACB_ELE;                                        /* Excessive Length  Error Register.                    */
    CPU_REG32  MACB_RJA;                                        /* Receive Jabbers Register.                            */
    CPU_REG32  MACB_USF;                                        /* Undersize Frame Register.                            */
    CPU_REG32  MACB_STE;                                        /* SQE Test          Error Register.                    */
    CPU_REG32  MACB_RLE;                                        /* Received Length Field Mismatch Register.             */
    CPU_REG32  MACB_TPF;                                        /* Transmitted Pause Frames Register.                   */

                                                                /* ----------------- ADDRESS REGISTERS ---------------- */
    CPU_REG32  MACB_HRB;                                        /* Hash Register Bottom [31: 0] Register.               */
    CPU_REG32  MACB_HRT;                                        /* Hash Register Top    [63:32] Register.               */
    CPU_REG32  MACB_SA1L;                                       /* Specific Address 1 Bottom Register.                  */
    CPU_REG32  MACB_SA1H;                                       /* Specific Address 1 Top    Register.                  */
    CPU_REG32  MACB_SA2L;                                       /* Specific Address 2 Bottom Register.                  */
    CPU_REG32  MACB_SA2H;                                       /* Specific Address 2 Top    Register.                  */
    CPU_REG32  MACB_SA3L;                                       /* Specific Address 3 Bottom Register.                  */
    CPU_REG32  MACB_SA3H;                                       /* Specific Address 3 Top    Register.                  */
    CPU_REG32  MACB_SA4L;                                       /* Specific Address 4 Bottom Register.                  */
    CPU_REG32  MACB_SA4H;                                       /* Specific Address 4 Top    Register.                  */

                                                                /* -------- ADDITIONAL CONFIGURATION REGISTERS -------- */
    CPU_REG32  MACB_TID;                                        /* Type ID Checking Register.                           */
    CPU_REG32  MACB_TPQ;                                        /* Transmit Pause Quantum Register.                     */
    CPU_REG32  MACB_USRIO;                                      /* User Input/Output Register.                          */
    CPU_REG32  MACB_WOL;                                        /* Wake on LAN.                                         */
    CPU_REG32  RESERVED[13];                                    /* Reserved.                                            */
    CPU_REG32  MACB_REV;                                        /* Revision.                                            */
} NET_DEV;


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

                                                                /* ------------------ MACB_NCR bits ------------------- */
#define  MACB_NCR_LB                              DEF_BIT_00    /* Loopback. When set, loopback signal is at high level.*/
#define  MACB_NCR_LLB                             DEF_BIT_01    /* Loopback local.                                      */
#define  MACB_NCR_RE                              DEF_BIT_02    /* Receive enable.                                      */
#define  MACB_NCR_TE                              DEF_BIT_03    /* Transmit enable.                                     */
#define  MACB_NCR_MPE                             DEF_BIT_04    /* Management port enable.                              */
#define  MACB_NCR_CLRSTAT                         DEF_BIT_05    /* Clear statistics registers.                          */
#define  MACB_NCR_INCSTAT                         DEF_BIT_06    /* Increment statistics registers.                      */
#define  MACB_NCR_WESTAT                          DEF_BIT_07    /* Write enable for statistics registers.               */
#define  MACB_NCR_BP                              DEF_BIT_08    /* Back pressure.                                       */
#define  MACB_NCR_TSTART                          DEF_BIT_09    /* Start Transmission.                                  */
#define  MACB_NCR_THALT                           DEF_BIT_10    /* Transmission Halt.                                   */
#define  MACB_NCR_TPFR                            DEF_BIT_11    /* Transmit pause frame.                                */
#define  MACB_NCR_TZQ                             DEF_BIT_12    /* Transmit zero quantum pause frame.                   */

                                                                /* ------------------ MACB_NCFGR bits ----------------- */
#define  MACB_NCFGR_SPD                           DEF_BIT_00    /* Speed.                                               */
#define  MACB_NCFGR_FD                            DEF_BIT_01    /* Full duplex.                                         */
#define  MACB_NCFGR_JFRAME                        DEF_BIT_03    /* Jumbo Frames.                                        */
#define  MACB_NCFGR_CAF                           DEF_BIT_04    /* Copy all frames.                                     */
#define  MACB_NCFGR_NBC                           DEF_BIT_05    /* No broadcast.                                        */
#define  MACB_NCFGR_MTI                           DEF_BIT_06    /* Multicast hash event enable                          */
#define  MACB_NCFGR_UNI                           DEF_BIT_07    /* Unicast hash enable.                                 */
#define  MACB_NCFGR_BIG                           DEF_BIT_08    /* Receive 1522 bytes.                                  */
#define  MACB_NCFGR_EAE                           DEF_BIT_09    /* External address match enable.                       */
#define  MACB_NCFGR_CLK                ((CPU_INT32U) 3 << 10)
#define  MACB_NCFGR_CLK_HCLK_8         ((CPU_INT32U) 0 << 10)   /* HCLK divided by 8.                                   */
#define  MACB_NCFGR_CLK_HCLK_16        ((CPU_INT32U) 1 << 10)   /* HCLK divided by 16.                                  */
#define  MACB_NCFGR_CLK_HCLK_32        ((CPU_INT32U) 2 << 10)   /* HCLK divided by 32.                                  */
#define  MACB_NCFGR_CLK_HCLK_64        ((CPU_INT32U) 3 << 10)   /* HCLK divided by 64.                                  */
#define  MACB_NCFGR_RTY                           DEF_BIT_12
#define  MACB_NCFGR_PAE                           DEF_BIT_13
#define  MACB_NCFGR_RBOF               ((CPU_INT32U) 3 << 14)
#define  MACB_NCFGR_RBOF_OFFSET_0      ((CPU_INT32U) 0 << 14)
#define  MACB_NCFGR_RBOF_OFFSET_1      ((CPU_INT32U) 1 << 14)
#define  MACB_NCFGR_RBOF_OFFSET_2      ((CPU_INT32U) 2 << 14)
#define  MACB_NCFGR_RBOF_OFFSET_3      ((CPU_INT32U) 3 << 14)
#define  MACB_NCFGR_RLCE                          DEF_BIT_16    /* Receive Length field Checking Enable.                */
#define  MACB_NCFGR_DRFCS                         DEF_BIT_17    /* Discard Receive FCS.                                 */
#define  MACB_NCFGR_EFRHD                         DEF_BIT_18
#define  MACB_NCFGR_IRXFCS                        DEF_BIT_19    /* Ignore RX FCS.                                       */

                                                                /* ------------------ MACB_NSR bits ------------------- */
#define  MACB_NSR_LINKR                           DEF_BIT_00
#define  MACB_NSR_MDIO                            DEF_BIT_01
#define  MACB_NSR_IDLE                            DEF_BIT_02

                                                                /* ------------------ MACB_TSR bits ------------------- */
#define  MACB_TSR_UBR                             DEF_BIT_00
#define  MACB_TSR_COL                             DEF_BIT_01
#define  MACB_TSR_RLES                            DEF_BIT_02
#define  MACB_TSR_TGO                             DEF_BIT_03    /* Transmit Go.                                         */
#define  MACB_TSR_BEX                             DEF_BIT_04    /* Buffers exhausted mid frame.                         */
#define  MACB_TSR_COMP                            DEF_BIT_05
#define  MACB_TSR_UND                             DEF_BIT_06

                                                                /* ------------------ MACB_RSR bits ------------------- */
#define  MACB_RSR_BNA                             DEF_BIT_00
#define  MACB_RSR_REC                             DEF_BIT_01
#define  MACB_RSR_OVR                             DEF_BIT_02

                                                                /* ------------------ MACB_ISR bits ------------------- */
                                                                /* ------------------ MACB_IER bits ------------------- */
                                                                /* ------------------ MACB_IDR bits ------------------- */
                                                                /* ------------------ MACB_IMR bits ------------------- */
#define  MACB_ISR_MFD                             DEF_BIT_00
#define  MACB_ISR_RCOMP                           DEF_BIT_01
#define  MACB_ISR_RXUBR                           DEF_BIT_02
#define  MACB_ISR_TXUBR                           DEF_BIT_03
#define  MACB_ISR_TUNDR                           DEF_BIT_04
#define  MACB_ISR_RLEX                            DEF_BIT_05
#define  MACB_ISR_TXERR                           DEF_BIT_06
#define  MACB_ISR_TCOMP                           DEF_BIT_07
#define  MACB_ISR_LINK                            DEF_BIT_09
#define  MACB_ISR_ROVR                            DEF_BIT_10
#define  MACB_ISR_HRESP                           DEF_BIT_11
#define  MACB_ISR_PFRE                            DEF_BIT_12
#define  MACB_ISR_PTZ                             DEF_BIT_13

                                                                /* ------------------ MACB_MAN bits ------------------- */
#define  MACB_MAN_CODE            ((CPU_INT32U)   0x02 << 16)   /* IEEE Code. MUST have value of 10.                    */
#define  MACB_MAN_RW              ((CPU_INT32U)   0x03 << 28)
#define  MACB_MAN_WRITE           ((CPU_INT32U)   0x01 << 28)   /* Transfer is a write.                                 */
#define  MACB_MAN_READ            ((CPU_INT32U)   0x02 << 28)   /* Transfer is a read.                                  */
#define  MACB_MAN_SOF             ((CPU_INT32U)   0x03 << 30)
#define  MACB_MAN_REGA(_x_)             ((_x_)         << 18)   /* Specifies the register in the PHY to access.         */
#define  MACB_MAN_PHYA(_x_)            (((_x_) & 0x1F) << 23)   /* PHY address. Normally 0.                             */
#define  MACB_MAN_DATA(_x_)             ((_x_) & 0xFFFF)        /* PHY Read/Write Data Mask.                            */

                                                                /* ----------------- MACB_USRIO bits ------------------ */
#define  MACB_USRIO_RMII                  DEF_BIT_00            /* Reduce MII.                                          */
#define  MACB_USRIO_CLKEN                 DEF_BIT_01            /* Clock Enable.                                        */

                                                                /* ------------------ MACB_WOL bits ------------------- */
#define  MACB_WOL_IP              ((CPU_INT32U) 0xFFFF <<  0)   /* ARP request IP address.                              */
#define  MACB_WOL_MAG             ((CPU_INT32U)      1 << 16)   /* Magic packet event enable.                           */
#define  MACB_WOL_ARP             ((CPU_INT32U)      1 << 17)   /* ARP request event enable.                            */
#define  MACB_WOL_SA1             ((CPU_INT32U)      1 << 18)   /* Specific address register 1 event enable.            */


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/

#define  EMAC_RXBUF_ADDRESS_MASK                 (0xFFFFFFFC)   /* Addr of Rx descriptor data area.                     */
#define  EMAC_RXBUF_ADD_WRAP                      DEF_BIT_01    /* Wrap bit.                                            */
#define  EMAC_RXBUF_SW_OWNED                      DEF_BIT_00    /* Descriptor ownership bit.                            */
#define  EMAC_RXBUF_LEN_MASK                     (0x00000FFF)   /* Length of frame including FCS.                       */
#define  EMAC_RXBUF_SOF_MASK                      DEF_BIT_14    /* Start of frame mask.                                 */
#define  EMAC_RXBUF_EOF_MASK                      DEF_BIT_15    /* End of frame mask.                                   */
#define  EMAC_RXBUF_OFF_MASK                     (0x03 << 12)   /* Data offset mask.                                    */

#define  EMAC_TXBUF_ADD_WRAP                      DEF_BIT_30    /* Wrap bit.                                            */
#define  EMAC_TXBUF_TX_SIZE_MASK                 (0x000007FF)   /* Length of frame including FCS.                       */
#define  EMAC_TXBUF_ADD_LAST                      DEF_BIT_15    /* Last descriptor fragment in chain.                   */

#define  EMAC_TXBUF_STATUS_USED                   DEF_BIT_31    /* Descriptor consumed by hardware.                     */


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
*                   (e) Allocate memory for device DMA descriptors and buffers
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
    NET_BUF_SIZE        buf_ix;
    CPU_INT32U          input_freq;
    CPU_INT32U          mdc_div;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          rx_buf_size;
    CPU_SIZE_T          tx_buf_align;
    CPU_SIZE_T          nbytes;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;

    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* See Note #7.                                         */
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets % RX_DESC_BUF_ALIGN != 0)) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* Descriptors AND Rx buffers MUST be aligned on a ...  */
         return;                                                /* ... 32 BYTE boundary.                                */
    }

    rx_buf_size = RX_DESC_BUF_SIZE;
    if ((rx_buf_size % RX_DESC_BUF_ALIGN) != 0) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* Descriptors AND Rx buffers MUST be aligned on a ...  */
         return;                                                /* ... 32 BYTE boundary.                                */
    }

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

                                                                /* Validate nbr Tx desc.                                */
    if (pdev_cfg->TxDescNbr == 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* At least one Tx descriptor MUST be allocated.        */
         return;
    }
                                                                /* Validate Tx buf alignment.                           */
    tx_buf_align = TX_DESC_BUF_ALIGN;
    if ((tx_buf_align % 8u) != 0u) {
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


                                                                /* ---------------------- INIT HW --------------------- */
                                                                /* Enable module clks (see Note #2).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ---- ALLOCATE MEMORY FOR DMA DESCRIPTORS & BUFS ---- */

    nbytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);            /* Allocate Rx descriptors.                             */
    pdev_data->RxBufDescPtrStart = (DEV_DESC *)Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                             (CPU_SIZE_T  ) pdev_cfg->RxBufAlignOctets,
                                                             (CPU_SIZE_T *)&reqd_octets,
                                                             (LIB_ERR    *)&lib_err);
    if (pdev_data->RxBufDescPtrStart == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data->RxBufDescPtrStartRegAddr = pdev_data->RxBufDescPtrStart;


    nbytes = pdev_cfg->RxDescNbr * RX_DESC_BUF_SIZE;            /* Allocate Rx descriptor buf's.                        */
    pdev_data->RxDescBufs = (CPU_INT08U *)Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                        (CPU_SIZE_T  ) pdev_cfg->RxBufAlignOctets,
                                                        (CPU_SIZE_T *)&reqd_octets,
                                                        (LIB_ERR    *)&lib_err);
    if (pdev_data->RxDescBufs == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }


    nbytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);            /* Allocate Tx descriptors.                             */
    pdev_data->TxBufDescPtrStart = (DEV_DESC *)Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                             (CPU_SIZE_T  ) TX_DESC_BUF_ALIGN,
                                                             (CPU_SIZE_T *)&reqd_octets,
                                                             (LIB_ERR    *)&lib_err);
    if (pdev_data->TxBufDescPtrStart == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data->TxBufDescPtrStartRegAddr = pdev_data->TxBufDescPtrStart;


    NetDev_DescAddrTranslate(pif);                              /* Translate desc ptr addrs to uncached variant.        */


                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */
    pdev->MACB_NCR |= MACB_NCR_MPE;                             /* Enable the management port.                          */
                                                                /* Get MDC clk freq (in Hz).                            */
    input_freq      = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    input_freq /= 1000;                                         /* Convert to kHz.                                      */
                                                                /* Determine MDC clk divider.                           */
    if (input_freq        <= 20000) {
        mdc_div = MACB_NCFGR_CLK_HCLK_8;
    } else if (input_freq <= 40000) {
        mdc_div = MACB_NCFGR_CLK_HCLK_16;
    } else if (input_freq <= 80000) {
        mdc_div = MACB_NCFGR_CLK_HCLK_32;
    } else {
        mdc_div = MACB_NCFGR_CLK_HCLK_64;
    }

    pdev->MACB_NCFGR  =  mdc_div;                               /* Set the MDC clk divider, clear RBOF (offset) bits.   */
    pdev->MACB_NCFGR |= (MACB_NCFGR_DRFCS |                     /* Do NOT copy the FCS to memory during DMA transfer    */
                         MACB_NCFGR_SPD   |                     /* Initially assume 100Mbps.                            */
                         MACB_NCFGR_FD);                        /* Initially assume Full Duplex.                        */

    pdev->MACB_USRIO  = MACB_USRIO_CLKEN;                       /* Enaable tranceiver input clk (EAM on AVR32 MACB).    */

                                                                /* ----------- OBTAIN REFERENCE TO PHY CFG ------------ */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;

    if (pphy_cfg != (void *)0) {                                /* Cfg MAC w/ initial Phy settings.                     */
#ifdef EMAC_SEL_AVR32                                           /* If using the AVR32                                   */
        if (pphy_cfg->BusMode == NET_PHY_BUS_MODE_MII) {
            pdev->MACB_USRIO  |= MACB_USRIO_RMII;               /* Enable MII mode on AVR MABC.                         */
        }
#else
        if (pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII) {
            pdev->MACB_USRIO  |= MACB_USRIO_RMII;               /* Enable RMII mode on non AVR MACB.                    */
        }
#endif
    }

                                                                /* ----------- DISABLE RX, TX, & INT SRC'S. ----------- */
    pdev->MACB_NCR &= ~(MACB_NCR_RE |                           /* See Note #9.                                         */
                        MACB_NCR_TE);

    pdev->MACB_IDR  =   0x3FFF;                                 /* Disable all int. src's.                              */

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
*                   receive event and signal the receive task ONLY for NEW descriptors which
*                   have not yet been accounted for.  Each time a descriptor is processed
*                  (or discarded) the count for acknowledged and unprocessed frames should be
*                   decremented by 1.  This function initializes the acknowledged receive
*                   descriptor count to 0.
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
    pdev_data->ErrRxPktOwner1     = 0;
    pdev_data->ErrRxPktOwner2     = 0;
    pdev_data->ErrRxPktNotSOF     = 0;
    pdev_data->ErrRxPktInvalidSOF = 0;
    pdev_data->ErrRxPktGetDataPtr = 0;
#endif
    pdev_data->RxNRdyCtr          = 0;                          /* No pending frames to process (see Note #2).          */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                         */
                            pdev_cfg->TxDescNbr,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }


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
            hw_addr[0] = (pdev->MACB_SA1L >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->MACB_SA1L >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->MACB_SA1L >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->MACB_SA1L >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->MACB_SA1H >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->MACB_SA1H >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->MACB_SA1L = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));

        pdev->MACB_SA1H = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* ------------------ CFG MULTICAST ------------------- */
#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

    pdev->MACB_HRB = 0;                                         /* Clear all multicast hash bits.                       */
    pdev->MACB_HRT = 0;                                         /* Clear all multicast hash bits.                       */


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
    pdev->MACB_RSR = (MACB_RSR_OVR  |                           /* Clear pending Rx int. src's.                         */
                      MACB_RSR_REC  |
                      MACB_RSR_BNA);

    pdev->MACB_TSR = (MACB_TSR_UBR  |                           /* Clear pending Tx int. src's.                         */
                      MACB_TSR_COL  |
                      MACB_TSR_RLES |
                      MACB_TSR_BEX  |
                      MACB_TSR_COMP |
                      MACB_TSR_UND);

    pdev->MACB_IER = (MACB_ISR_RCOMP |                          /* Enable 'Reception complete' interrupt.               */
                      MACB_ISR_ROVR  |                          /* Enable 'Receiver overrun'   interrupt.               */
                      MACB_ISR_TCOMP |                          /* Enable Tx complete interrupt                         */
                      MACB_ISR_TXUBR |                          /* Enable Tx bit used interrupt                         */
                      MACB_ISR_TUNDR |                          /* Enable Tx underrun interrupt.                        */
                      MACB_TSR_COL   |                          /* Enable Tx collision interrupt.                       */
                      MACB_TSR_RLES  |                          /* Enable Retry Limit Exceeded interrupt.               */
                      MACB_TSR_BEX);                            /* Enable Buffer Exhasted Mid-Frame interrupt.          */

                                                                /* ------------------ ENABLE RX & TX ------------------ */
    pdev->MACB_NCR |= (MACB_NCR_CLRSTAT |                       /* Clear stat regs.                                     */
                       MACB_NCR_TE      |                       /* Tx enable.                                           */
                       MACB_NCR_RE);                            /* Rx enable.                                           */

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
*
*              (4)  The MACB uses locally allocated receive buffers for receive descriptor data
*                   areas.  Consequently, the data areas do NOT need to be freed when the
*                   interface is stopped.  Only the receive frame counter needs to be reset.
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


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    pdev->MACB_NCR &= ~(MACB_NCR_TE |
                        MACB_NCR_RE);

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    pdev->MACB_IDR  =   0x3FFF;                                 /* Disable all int. src's.                              */

    pdev->MACB_RSR  =  (MACB_RSR_OVR  |                         /* Clear pending Rx int. src's.                         */
                        MACB_RSR_REC  |
                        MACB_RSR_BNA);

    pdev->MACB_TSR  =  (MACB_TSR_UBR  |                         /* Clear pending Tx int. src's.                         */
                        MACB_TSR_COL  |
                        MACB_TSR_RLES |
                        MACB_TSR_BEX  |
                        MACB_TSR_COMP |
                        MACB_TSR_UND);


                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    pdesc = &pdev_data->TxBufDescPtrStart[0];
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Dealloc ALL tx bufs (see Note #2a2).                 */
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->Addr, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdesc++;
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Decrement frame counter
*                   (b) Validate descriptor start of frame bit
*                   (c) Obtain pointer to buffer
*                   (c) Copy frame data from descriptors in to buffer
*                   (d) Return size and pointer to received data
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to pointer to received data area. The received data
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
*
*                   (b) If the descriptor IS valid, but an error is indicated within
*                       the descriptor status bits, or length field, then the driver
*                       MUST free all descriptors associated with the current received frame
*                       and change the current receive descriptor pointer to the
*                       location of the next start of frame (SOF) descriptor.
*
*                   (c) If a new data area is unavailable, the driver NOT increment the current
*                       receive descriptor pointer.  This will ensure that the next receive
*                       interrupt attempts to process the missed frame.
*
*               (4) If the MACB hardware fails to fully commit a frame to memory AND fails to
*                   properly update all ownership bits as is common when a critical error occurs,
*                   then the best course of recovery is to stop and start the EMAC in order
*                   to ensure that pdev_data->RxBufDescPtr points to the exact location of the
*                   next receive descriptor.  When the above error occurs, this location cannot
*                   be precisely determined without a reset.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    CPU_INT32U          status;
    CPU_INT32U          src_addr;
    CPU_INT16U          frame_len;
    CPU_INT16U          src_offset;
    CPU_INT16U          dst_offset;
    CPU_INT08U          copy_len;
    CPU_INT08U         *dest_addr;
    CPU_SR_ALLOC();


                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */

    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to dev cfg struct.                        */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* --------------- DECREMENT FRAME CNT ---------------- */
    CPU_CRITICAL_ENTER();                                       /* Disable interrupts to alter shared data.             */
    if (pdev_data->RxNRdyCtr > 0) {                             /* One less frame to process.                           */
        pdev_data->RxNRdyCtr--;
    }
    CPU_CRITICAL_EXIT();


                                                                /* --------------- VALIDATE DESCRIPTOR ---------------- */
    pdesc = pdev_data->RxBufDescPtrCur;                         /* Point to SOF descriptor.                             */

    src_addr = pdesc->Addr;
    if ((src_addr & EMAC_RXBUF_SW_OWNED) == DEF_NO) {           /* Error.  Descriptor not yet owned by software.        */
         NET_CTR_ERR_INC(pdev_data->ErrRxPktOwner1);
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
        *perr   = (NET_ERR     )NET_DEV_ERR_RX;
         return;
    }

    status = pdesc->Status;
    if ((status & EMAC_RXBUF_SOF_MASK) == DEF_NO) {
         NET_CTR_ERR_INC(pdev_data->ErrRxPktNotSOF);
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
        *perr   = (NET_ERR     )NET_DEV_ERR_RX;
         return;
    }

                                                                /* --------- OBTAIN PTR TO NEW DMA DATA AREA ---------- */
                                                                /* Request an empty buffer.                             */
    dest_addr = NetBuf_GetDataPtr((NET_IF        *)pif,
                                  (NET_TRANSACTION)NET_TRANSACTION_RX,
                                  (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                  (NET_BUF_SIZE   )NET_IF_IX_RX,
                                  (NET_BUF_SIZE  *)0,
                                  (NET_BUF_SIZE  *)0,
                                  (NET_BUF_TYPE  *)0,
                                  (NET_ERR       *)perr);

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer (see Note #3c).            */
         NET_CTR_ERR_INC(pdev_data->ErrRxPktGetDataPtr);
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
         return;
    }

                                                                /* ----------------- COPY FRAME DATA ------------------ */

    dst_offset =  0;
    src_offset = (status & EMAC_RXBUF_OFF_MASK) >> 12;          /* Data alignment offset within 1st descriptor.         */

    while ((status & EMAC_RXBUF_EOF_MASK) == DEF_NO) {          /* Copy data from all descriptors except last.          */
        src_addr = pdesc->Addr & EMAC_RXBUF_ADDRESS_MASK;
        copy_len = RX_DESC_BUF_SIZE - src_offset;

        Mem_Copy((void     *)(dest_addr + dst_offset),
                 (void     *)(src_addr  + src_offset),
                 (CPU_SIZE_T)copy_len);

        dst_offset  +=  RX_DESC_BUF_SIZE;
        src_offset   =  0;                                      /* Only first descriptor has a data offset.             */

        BARRIER();
        pdesc->Addr &= ~EMAC_RXBUF_SW_OWNED;                    /* Free descriptor.                                     */
        BARRIER();

        if (pdesc != pdev_data->RxBufDescPtrEnd) {              /* Point to next descriptor.                            */
            pdesc++;
        } else {
            pdesc = pdev_data->RxBufDescPtrStart;
        }


        src_addr = pdesc->Addr;
        if ((src_addr & EMAC_RXBUF_SW_OWNED) == DEF_NO) {       /* Validate ownership (see Note #4).                    */
             NET_CTR_ERR_INC(pdev_data->ErrRxPktOwner2);
             NetBuf_FreeBufDataAreaRx(pif->Nbr, dest_addr);     /* Return data area to Rx data area pool.               */
             NetDev_Stop(pif, perr);
             NetDev_Start(pif, perr);
            *perr = NET_DEV_ERR_RX;
             return;
        }

        status = pdesc->Status;
        if ((status & EMAC_RXBUF_SOF_MASK) > 0) {               /* Premature SOF before EOF.  Incomplete frame found.   */
             NET_CTR_ERR_INC(pdev_data->ErrRxPktInvalidSOF);
             NetBuf_FreeBufDataAreaRx(pif->Nbr, dest_addr);     /* Return data area to Rx data area pool.               */
             pdev_data->RxBufDescPtrCur = pdesc;                /* Update pointer to next SOF descriptor.               */
            *size   = (CPU_INT16U  )0;
            *p_data = (CPU_INT08U *)0;
            *perr   = (NET_ERR     )NET_DEV_ERR_RX;
             return;
        }
    }

    frame_len = status & EMAC_RXBUF_LEN_MASK;                   /* Obtain frame len & copy data from EOF descriptor.    */
    copy_len  = frame_len - dst_offset;

    src_addr  = pdesc->Addr & EMAC_RXBUF_ADDRESS_MASK;
    src_addr  = (CPU_INT32U)(DMA_Map_Single((void volatile *)src_addr, RX_DESC_BUF_SIZE, DMA_FROM_DEVICE));

    Mem_Copy((void     *)(dest_addr + dst_offset),
             (void     *)(src_addr  + src_offset),
             (CPU_SIZE_T)copy_len);

    BARRIER();
    pdesc->Addr &= ~EMAC_RXBUF_SW_OWNED;                        /* Free descriptor.                                     */
    BARRIER();

    if (pdesc != pdev_data->RxBufDescPtrEnd) {                  /* Point to next descriptor.                            */
        pdesc++;
    } else {
        pdesc = pdev_data->RxBufDescPtrStart;
    }

    pdev_data->RxBufDescPtrCur = pdesc;                         /* Update pointer to next SOF descriptor.               */


                                                                /* --------------- CHECK FRAME LENGTH ----------------- */
    if (frame_len < NET_IF_ETHER_FRAME_MIN_SIZE) {              /* If frame is a runt.                                  */
        NetBuf_FreeBufDataAreaRx(pif->Nbr, dest_addr);          /* Return data area to Rx data area pool.               */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }

   *size   = frame_len;                                         /* Return the size of the received frame.               */
   *p_data = dest_addr;                                         /* Return a pointer to the newly received data area.    */

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
*                   drivers may find it useful to adjust pdev_data->TxBufDescPtrComp after
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
    CPU_INT32U          desc_status;
    CPU_INT32U          reg_val;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* -------------- CHECK IF HW RDY TO TX --------------- */
    reg_val = pdev->MACB_TSR;                                   /* Transmitter ready?                                   */
    if ((reg_val & MACB_TSR_TGO) == MACB_TSR_TGO) {
        *perr = NET_DEV_ERR_TX_BUSY;
         return;
    }

    pdesc = pdev_data->TxBufDescPtrCur;                         /* Obtain ptr to next available Tx descriptor.          */

    desc_status = pdesc->Status;
    if ((desc_status & EMAC_TXBUF_STATUS_USED) > 0) {           /* Check if descriptor is software owned (see Note #3). */
        p_data = (CPU_INT08U *)DMA_Map_Single(p_data, size, DMA_TO_DEVICE);
        if (((CPU_INT32U)p_data & 0xFF000000) == 0x24000000) {  /* Tx buf located in internal SRAM.                     */
            pdesc->Addr =  (CPU_INT32U)p_data;                  /* Cfg desc w/      Tx data area addr.                  */
        } else {
            pdesc->Addr =  (CPU_INT32U)ADDR_PHYS(p_data);       /* Cfg desc w/ phys Tx data area addr.                  */
        }

        if (pdev_data->TxBufDescPtrCur == pdev_data->TxBufDescPtrEnd) {
            pdesc->Status   =  EMAC_TXBUF_ADD_LAST |            /* The WRAP bit must be set on the last (& only) desc.  */
                               EMAC_TXBUF_ADD_WRAP |
                              (size & EMAC_TXBUF_TX_SIZE_MASK);
        } else {
            pdesc->Status   =  EMAC_TXBUF_ADD_LAST |            /* The WRAP bit must be set on the last (& only) desc.  */
                              (size & EMAC_TXBUF_TX_SIZE_MASK);
        }

        DMA_Map_Single(pdesc, sizeof(DEV_DESC), DMA_TO_DEVICE);
        pdev->MACB_NCR |=   MACB_NCR_TSTART;                    /* Start the transmission.                              */

                                                                /* Update curr desc ptr to point to next desc.          */
        if (pdev_data->TxBufDescPtrCur != pdev_data->TxBufDescPtrEnd) {
            pdev_data->TxBufDescPtrCur++;
        } else {
            pdev_data->TxBufDescPtrCur  = pdev_data->TxBufDescPtrStart;
        }

       *perr = NET_DEV_ERR_NONE;
    } else {
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
*                       (a) Imperfect hash filtering of 64  multicast addresses.
*                       (b) Promiscous non filtering.  Disable filtering of all received frames.
*
*               (2) This function implements the filtering mechanism described in 1a.
*
*               (3) The hash algorithm is the exlusive OR of every 6th bit of the destination address.
*                       hash[5] = da[5] ^ da[11] ^ da[17] ^ da[23] ^ da[29] ^ da[35] ^ da[41] ^ da[47]
*                       hash[4] = da[4] ^ da[10] ^ da[16] ^ da[22] ^ da[28] ^ da[34] ^ da[40] ^ da[46]
*                       hash[3] = da[3] ^ da[09] ^ da[15] ^ da[21] ^ da[27] ^ da[33] ^ da[39] ^ da[45]
*                       hash[2] = da[2] ^ da[08] ^ da[14] ^ da[20] ^ da[26] ^ da[32] ^ da[38] ^ da[44]
*                       hash[1] = da[1] ^ da[07] ^ da[13] ^ da[19] ^ da[25] ^ da[31] ^ da[37] ^ da[43]
*                       hash[0] = da[0] ^ da[06] ^ da[12] ^ da[18] ^ da[24] ^ da[30] ^ da[36] ^ da[42]
*
*                       (a) Where da0 represents the least significant bit of the first octet
*                           of the destination address received and where da47 represents the
*                           most significant bit of the last octet of the destination address
*                           received..
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
#ifdef NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
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
        pdev->MACB_HRB |= (1 << reg_bit);
    } else {
        pdev->MACB_HRT |= (1 << reg_bit);
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
#ifdef NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
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
        pdev->MACB_HRB &= ~(1 << reg_bit);
    } else {
        pdev->MACB_HRT &= ~(1 << reg_bit);
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
                         pdev->MACB_NCFGR |=  MACB_NCFGR_FD;    /* Cfg MAC to FULL duplex.                              */
                         break;


                    case NET_PHY_DUPLEX_HALF:
                         pdev->MACB_NCFGR  &= ~MACB_NCFGR_FD;   /* Cfg MAC to HALF duplex.                              */
                         break;


                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {
                    case NET_PHY_SPD_100:
                         pdev->MACB_NCFGR |=  MACB_NCFGR_SPD;   /* Cfg MAC to 100Mbit.                                  */
                         break;


                    case NET_PHY_SPD_10:
                         pdev->MACB_NCFGR &= ~MACB_NCFGR_SPD;   /* Cfg MAC to 10Mbit.                                   */
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
    pdev->MACB_MAN = (MACB_MAN_SOF & (1 << 30)) |               /* Start of frame code (must be 0x01                    */
                      MACB_MAN_READ             |               /* RW flags, must be 0x10 for Read                      */
                      MACB_MAN_PHYA(phy_addr)   |               /* Specify the address of the PHY to read from          */
                      MACB_MAN_REGA(reg_addr)   |               /* Supply the addres of the PHY register to read        */
                      MACB_MAN_CODE             |               /* IEEE code, must be 0x10                              */
                      MACB_MAN_DATA(0);                         /* Dummy data, 0x0000                                   */


    timeout = MII_REG_RD_WR_TO;
    while (timeout > 0) {                                       /* Wait for the MII bus to become idle.                 */
        timeout--;
        if ((pdev->MACB_NSR & MACB_NSR_IDLE) > 0) {
             break;
        }
    }

    if (timeout == 0) {                                         /* Return an error if a timeout occured.                */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
       *p_data = 0;
        return;
    }

   *p_data = (pdev->MACB_MAN & 0x0000FFFF);                     /* Read the management register data.                   */

   *perr = NET_PHY_ERR_NONE;
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
    CPU_INT32U          timeout;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    pdev->MACB_MAN = (MACB_MAN_SOF & (1 << 30)) |               /* Start of frame code (must be 0x01                    */
                      MACB_MAN_WRITE            |               /* RW flags, must be 0x01 for Read                      */
                      MACB_MAN_PHYA(phy_addr)   |               /* Specify the address of the PHY to read from          */
                      MACB_MAN_REGA(reg_addr)   |               /* Supply the addres of the PHY register to read        */
                      MACB_MAN_CODE             |               /* IEEE code, must be 0x10                              */
                      MACB_MAN_DATA(data);                      /* Specify the data to be written                       */

    timeout = MII_REG_RD_WR_TO;
    while (timeout > 0) {                                       /* Wait for the MII bus to become idle.                 */
        timeout--;
        if ((pdev->MACB_NSR & MACB_NSR_IDLE) > 0) {
            break;
        }
    }

    if(timeout == 0) {                                          /* Return an error if a timeout occured.                */
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_RxDescInit()
*
* Description : (1) This function initializes the Rx descriptor list for the specified interface :
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Rx descriptor pointers
*                   (c) Configure Rx descriptor data areas
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
*********************************************************************************************************
*/

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
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


                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdesc                      = (DEV_DESC *)pdev_data->RxBufDescPtrStart;
    pdev_data->RxBufDescPtrCur = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrEnd = (DEV_DESC *)pdesc + (pdev_cfg->RxDescNbr - 1);

                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->Status =  0;
        pdesc->Addr   = (CPU_INT32U)((pdev_data->RxDescBufs + (i * RX_DESC_BUF_SIZE))) & 0xFFFFFFE0;

        if (pdesc == (pdev_data->RxBufDescPtrEnd)) {            /* Set WRAP bit on last descriptor in list.             */
            pdesc->Addr |= EMAC_RXBUF_ADD_WRAP;
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start addr.       */
                                                                /* MUST use phys addr if desc's not in int. sram.       */
    BARRIER();
    pdev->MACB_RBQP = (CPU_INT32U)pdev_data->RxBufDescPtrStartRegAddr;

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_TxDescInit()
*
* Description : (1) This function initializes the Tx descriptor list for the specified interface :
*
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Tx descriptor pointers
*                   (c) Obtain Tx descriptor data areas
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
*               (3) This routine is called PRIOR to enabling interrupts and therefore does NOT require
*                   critical sections surrounding the shared data.  However, for consistency, the
*                   safe guard has been left in place.
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


                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdesc                       = (DEV_DESC *)pdev_data->TxBufDescPtrStart;
    pdev_data->TxBufDescPtrCur  = (DEV_DESC *)pdev_data->TxBufDescPtrStart;
    pdev_data->TxBufDescPtrComp = (DEV_DESC *)pdev_data->TxBufDescPtrStart;
    pdev_data->TxBufDescPtrEnd  = (DEV_DESC *)pdev_data->TxBufDescPtrStart + (pdev_cfg->TxDescNbr - 1);

                                                                /* --------------- INIT TX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Initialize Tx descriptor ring                        */
        pdesc->Status = (EMAC_TXBUF_STATUS_USED |               /* All descriptors initially software owned.            */
                         EMAC_TXBUF_ADD_LAST);                  /* One descriptor per transmit buffer.                  */

        if (pdesc == (pdev_data->TxBufDescPtrEnd)) {            /* Set WRAP bit on last descriptor in list.             */
            pdesc->Status |= EMAC_TXBUF_ADD_WRAP;
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Tx desc start address.    */
    pdev->MACB_TBQP = (CPU_INT32U)pdev_data->TxBufDescPtrStartRegAddr;

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
* Note(s)     : (1) This function via function pointer from the context of an ISR.
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
*                   the transmit deallocation task.
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
    CPU_INT16U          n_rdy;
    CPU_INT16U          n_new;
    CPU_INT16U          i;
    CPU_INT08U         *p_data;
    CPU_BOOLEAN         rx_event;
    NET_ERR             err;
    CPU_SR_ALLOC();


   (void)&type;                                                 /* Prevent compiler warning if arg 'type' not req'd.    */

    CPU_CRITICAL_ENTER();                                       /* Prohibit interrupt nesting.                          */


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------------- PROCESS ISR -------------------- */
    reg_val = pdev->MACB_ISR;                                   /* Clear int. status reg.                               */
   (void)&reg_val;

                                                                /* ----------------- HANDLE RX EVENT ------------------ */
    rx_event = DEF_NO;
    reg_val  = pdev->MACB_RSR;
    if ((reg_val & MACB_RSR_REC) == MACB_RSR_REC) {             /* Check if we have recieved a frame                    */
        pdev->MACB_RSR =  MACB_RSR_REC;                         /* Clear the Receive Status Register REC bit            */
        rx_event = DEF_YES;
    }

    if ((reg_val & (MACB_RSR_BNA | MACB_RSR_OVR)) > 0) {        /* Clear all receiver errors if they have occured       */
        pdev->MACB_RSR = (MACB_RSR_BNA | MACB_RSR_OVR);
        rx_event = DEF_YES;
    }

    if (rx_event == DEF_YES) {
        n_rdy = 0;
        pdesc = pdev_data->RxBufDescPtrStart;

        DCache_Flush_Unlocked();

        for (i = 0; i < pdev_cfg->RxDescNbr; i++) {             /* Count nbr of ready descriptors.                      */
            if (((pdesc->Addr   & EMAC_RXBUF_SW_OWNED) > 0) &&
                ((pdesc->Status & EMAC_RXBUF_EOF_MASK) > 0)) {
                n_rdy++;
            }

            pdesc++;
        }

        BARRIER();
        n_new = n_rdy - pdev_data->RxNRdyCtr;                   /* Determine how many descriptors have become ready ... */
                                                                /* ... since last count of ready descriptors        ... */
                                                                /* ... (see Note #3).                                   */

        while (n_new > 0) {
            NetIF_RxTaskSignal(pif->Nbr, &err);                 /* Signal Net IF RxQ Task for each new rdy descriptor.  */
            switch (err) {
                case NET_IF_ERR_NONE:
                     if (pdev_data->RxNRdyCtr < pdev_cfg->RxBufLargeNbr) {
                         pdev_data->RxNRdyCtr++;
                     }
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

                                                                /* ----------------- HANDLE TX EVENT ------------------ */
    reg_val = pdev->MACB_TSR;                                   /* Read the Transmit Status Register                    */
    if (((reg_val & MACB_TSR_COMP) > 0) ||                      /* Check if frame successfully completed Tx, or if  ... */
        ((reg_val & MACB_TSR_UND)  > 0) ||                      /* ... passive  error occured. ... In both cases,   ... */
        ((reg_val & MACB_TSR_UBR)  > 0)) {                      /* ... the Tx buffer MUST be de-allocated.              */

         pdesc   =   pdev_data->TxBufDescPtrComp;
         if ((pdesc->Status & EMAC_TXBUF_STATUS_USED) > 0) {
              p_data = (CPU_INT08U *)pdesc->Addr;
              NetIF_TxDeallocTaskPost(p_data, &err);
              NetIF_DevTxRdySignal(pif);                        /* Signal Net IF that Tx resources are available.       */
              if (pdesc != pdev_data->TxBufDescPtrEnd) {
                  pdesc++;
              } else {
                  pdesc   = pdev_data->TxBufDescPtrStart;
              }
         }

         pdev_data->TxBufDescPtrComp = pdesc;                   /* Set ptr to next expected descriptor to complete Tx.  */

         if ((reg_val & MACB_TSR_COMP) > 0) {
             pdev->MACB_TSR = MACB_TSR_COMP;                    /* Clr the Tx stat reg COMP bit.                        */
         }
    }


    if ((reg_val & MACB_TSR_UND) > 0) {                         /* Transmit underrun.  Tx halted, TBQP reset by HW.     */
         NetDev_TxDescInit(pif, &err);                          /* SW MUST re-initialize Tx descriptors.                */
         NetIF_DevCfgTxRdySignal((NET_IF_NBR) pif,              /* Re-cfg Tx Rdy sem count.                             */
                                  (CPU_INT16U) pdev_cfg->TxDescNbr,
                                  (NET_ERR  *)&err);

         pdev->MACB_TSR  = MACB_TSR_UND;                        /* Clr the Tx stat reg UND bit.                         */
    }

    pdev->MACB_TSR = (MACB_TSR_UBR  |                           /* Clear other int. status flags.                       */
                      MACB_TSR_COL  |
                      MACB_TSR_RLES |
                      MACB_TSR_BEX);

    CPU_CRITICAL_EXIT();                                        /* Re-enable interrupts.                                */
}


/*
*********************************************************************************************************
*                                      NetDev_DescAddrTranslate()
*
* Description : (1) This function translates external memory addresses to their non cached equivalent.
*
*                   (a) Translate Rx descriptor ring   start address   to its   physical address.
*                   (c) Translate Rx descriptor buffer start address   to its   uncached address.
*                   (b) Translate Rx descriptor ring         addresses to their uncached address.
*                   (d) Translate Tx descriptor ring         addresses to their uncached address.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (2) This routine MUST be called BEFORE :
*                   (a) NetDev_RxBufDescInit(),
*                   (b) NetDev_TxBufDescInit()
*********************************************************************************************************
*/

static  void  NetDev_DescAddrTranslate (NET_IF  *pif)
{
    NET_DEV_DATA  *pdev_data;

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */

                                                                /* --------- MEM ALLOC PHYS ADDR TRANSLATION ---------- */
                                                                /* If mem obj's NOT located in internal SRAM ...        */
    if (((CPU_INT32U)(pdev_data->RxBufDescPtrStart) & 0xFF000000) != 0x24000000) {
         pdev_data->RxBufDescPtrStartRegAddr = (DEV_DESC *)(ADDR_PHYS(pdev_data->RxBufDescPtrStart));
         pdev_data->RxBufDescPtrStart        = (DEV_DESC *)(ADDR_UNCACHED(pdev_data->RxBufDescPtrStart));
    }

    if (((CPU_INT32U)(pdev_data->RxDescBufs) & 0xFF000000) != 0x24000000) {
         pdev_data->RxDescBufs = (CPU_INT08U *)(ADDR_PHYS(pdev_data->RxDescBufs));
    }

    if (((CPU_INT32U)(pdev_data->TxBufDescPtrStart) & 0xFF000000) != 0x24000000) {
         pdev_data->TxBufDescPtrStartRegAddr = (DEV_DESC *)(ADDR_PHYS(pdev_data->TxBufDescPtrStartRegAddr));
         pdev_data->TxBufDescPtrStart        = (DEV_DESC *)(ADDR_UNCACHED(pdev_data->TxBufDescPtrStart));
    }
}


/*
*********************************************************************************************************
*                                          DCache_Flush()
*
* Description : (1) Finish all pending memory accesses, empty write buffers, and clean & invalidate cache lines.
*
* Argument(s) : pbuf        Pointer to memory buffer to clean & invalidate cache from.
*
*               size        Size of the cache to clean & invalidate.
*
* Return(s)   : none.
*
* Caller(s)   : MACB_TxPkt().
*********************************************************************************************************
*/

static  inline  void  DCache_Invalidate_Line (volatile  void  *pmem)
{
#ifdef __GNUC__
    __builtin_cache((void *)pmem, 0x0B);
#elif  __ICCAVR32__
    __cache_control((void *)pmem, 0x0B);
#endif
   (void)&pmem;                                                 /* Prevent possible 'variable unused' warning.          */
}


static  inline  void  DCache_Clean_Line (volatile  void  *pmem)
{
#ifdef __GNUC__
    __builtin_cache((void *)pmem, 0x0C);
#elif  __ICCAVR32__
    __cache_control((void *)pmem, 0x0C);
#endif
   (void)&pmem;                                                 /* Prevent possible 'variable unused' warning.          */
}


static  inline  void  DCache_Flush_Line (volatile  void  *pmem)
{
#ifdef __GNUC__
    __builtin_cache((void *)pmem, 0x0D);
#elif  __ICCAVR32__
    __cache_control((void *)pmem, 0x0D);
#endif
   (void)&pmem;                                                 /* Prevent possible 'variable unused' warning.          */
}

#if (USE_CACHE_FLUSH > 0)
inline  void  DCache_Flush_Unlocked (void)
{
#ifdef __GNUC__
    __builtin_cache((void *)5, 0x08);
#elif  __ICCAVR32__
    __cache_control((void *)5, 0x08);
#endif
}
#else
inline  void  DCache_Flush_Unlocked (void) {
}
#endif


inline  void  Sync_Write_Buffer (void)
{
#ifdef __GNUC__
    __builtin_sync(0);
#elif  __ICCAVR32__
    __synchronize_write_buffer(0);
#endif
}


void  DCache_Clean_Range (volatile void  *pmem,
                          CPU_INT32U      size)
{
    CPU_INT32U  i;
    CPU_INT32U  begin;
    CPU_INT32U  end;
    CPU_INT32U  linesz;


    linesz = CFG_DCACHE_LINESZ;
    begin  =  (CPU_INT32U)pmem & ~(linesz - 1);
    end    = ((CPU_INT32U)pmem + size + linesz - 1) & ~(linesz - 1);

    for (i = begin; i < end; i += linesz) {
        DCache_Clean_Line((void *)i);
    }

    Sync_Write_Buffer();
}


void  DCache_Invalidate_Range (volatile void  *pmem,
                               CPU_INT32U      size)
{
    CPU_INT32U  i;
    CPU_INT32U  begin;
    CPU_INT32U  end;
    CPU_INT32U  linesz;


    linesz = CFG_DCACHE_LINESZ;
    begin  =  (CPU_INT32U)pmem & ~(linesz - 1);
    end    = ((CPU_INT32U)pmem + size + linesz - 1) & ~(linesz - 1);

    for (i = begin; i < end; i += linesz) {
        DCache_Invalidate_Line((void *)i);
    }
}

void  DCache_Flush_Range (volatile void  *pmem,
                          CPU_INT32U      size)
{
    CPU_INT32U  i;
    CPU_INT32U  begin;
    CPU_INT32U  end;
    CPU_INT32U  linesz;


    linesz = CFG_DCACHE_LINESZ;
    begin  =  (CPU_INT32U)pmem & ~(linesz - 1);
    end    = ((CPU_INT32U)pmem + size + linesz - 1) & ~(linesz - 1);

    for (i = begin; i < end; i += linesz) {
        DCache_Flush_Line((void *)i);
    }

    Sync_Write_Buffer();
}


inline  void  *DMA_Map_Single (volatile void  *pvaddr,
                               CPU_INT32U      len,
                               CPU_INT08U      dir)
{
    switch (dir) {
        case DMA_BIDIRECTIONAL:
             DCache_Flush_Range(pvaddr, len);
             break;

            case DMA_TO_DEVICE:
                 DCache_Clean_Range(pvaddr, len);
                 break;

            case DMA_FROM_DEVICE:
                 DCache_Invalidate_Range(pvaddr, len);
                 break;

            default:
                return ((void *)0);
    }

    if (((CPU_INT32U)pvaddr & 0xFF000000) == 0x24000000) {              /* Mem is located in internal SRAM.                     */
        return ((void *)pvaddr);
    } else {
        return ((void *)ADDR_PHYS(pvaddr));
    }
}


#endif /* NET_IF_ETHER_MODULE_EN */

