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
*                                          Renesas RX-EtherC
*
* Filename : net_dev_rx_etherc.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.00.00 (or more recent version) is included in the project build.
*
*            (2) The following parts may use this 'net_dev_rx_etherc' device driver :
*
*                (a) RX62N
*                (b) RX63N
*                (c) RX64N
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_RX_ETHERC_MODULE
#include  <Source/net.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  <lib_mem.h>
#include  "net_dev_rx_etherc.h"

#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Receive buffers usually MUST be aligned to some octet boundary.  However, adjusting
*               receive buffer alignment MUST be performed from within 'net_dev_cfg.h'.  Do not adjust
*               the value below as it is used for configuration checking only.
*********************************************************************************************************
*/

#define  RX_BUF_SIZE_OCTETS                             16
#define  RX_BUF_ALIGN_OCTETS                            16      /* See Note #1.                                         */

#define  PHY_READ                                        2u
#define  PHY_WRITE                                       1u
#define  PHY_WRITE_TA                                    2u
#define  MII_DATA                               0x40000000u
#define  BIT_MASK                               0x00000001u



/*
*********************************************************************************************************
*                                      REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

#define  EDMAC_RESET                            DEF_BIT_00

#define  IPGR_IPG_96_BIT                              0x14u

#define  ECMR_DUPLEX_FULL                       DEF_BIT_01
#define  ECMR_SPD_100                           DEF_BIT_02
#define  ECMR_TX_EN                             DEF_BIT_05
#define  ECMR_RX_EN                             DEF_BIT_06

#define  ECSR_ICD                               DEF_BIT_00
#define  ECSR_MPD                               DEF_BIT_01
#define  ECSR_LCHNG                             DEF_BIT_02
#define  ECSR_PSRTO                             DEF_BIT_04
#define  ECSR_BFR                               DEF_BIT_05

#define  FDR_RX_FIFO_2048                             0x07u
#define  FDR_TX_FIFO_2048                      (0x07u << 8u)

#define  FCFTR_8_FRM                            0x00070000L

#define  EESIPR_RX                              DEF_BIT_18
#define  EESIPR_TX                              DEF_BIT_21
#define  EESIPR_RX_MULTI                        DEF_BIT_07
#define  EESIPR_RX_ERR                          0x0301001Fu

#define  RMCR_CONT_RX                           DEF_BIT_00

#define  EMDR_LITTLE_ENDIAN                     DEF_BIT_06

#define  TRSCER_RMAFCE                          DEF_BIT_07

#define  EESR_CLR                               0x47FF0F9Fu

#define  EPTPC_SWR_RESET                           0x00000001u

#define  DATA_FLASH_RD_EN_DFLRE1_REG_1          0x007FC442u

/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/

#define  BUFSIZE                                       256u   /* Must be 32-bit aligned.                              */
#define  ENTRY                                           8u   /* Number of RX and TX buffers.                         */

#define  ACT                                    0x80000000u
#define  DL                                     0x40000000u
#define  FP1                                    0x20000000u
#define  FP0                                    0x10000000u
#define  FE                                     0x08000000u

#define  RFOVER                                 0x00000200u
#define  RAD                                    0x00000100u
#define  RMAF                                   0x00000080u
#define  RRF                                    0x00000010u
#define  RTLF                                   0x00000008u
#define  RTSF                                   0x00000004u
#define  PRE                                    0x00000002u
#define  CERF                                   0x00000001u

#define  TAD                                    0x00000100u
#define  CND                                    0x00000008u
#define  DLC                                    0x00000004u
#define  CD                                     0x00000002u
#define  TRO                                    0x00000001u


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

typedef  struct  dev_desc  DEV_DESC;

struct dev_desc
{
     CPU_REG32   status;                            /* DMA status register.                                 */
#if CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE
     CPU_REG16   size;                              /* DMA size                                             */
     CPU_REG16   bufsize;                           /* DMA buffer size.                                     */
#else
     CPU_REG16   bufsize;                           /* DMA buffer size.                                     */
     CPU_REG16   size;                              /* DMA size                                             */
#endif
    CPU_INT08U  *p_buf;                             /* DMA buffer pointer                                   */
    DEV_DESC    *next;                              /* DMA next descriptor pointer.                         */
};


                                                                /* --------------- BUFFER LIST DATA TYPE -------------- */
typedef  struct  list  LIST;

struct  list {
    void        *Buffer;
    CPU_INT16U   Len;
    LIST        *Next;
};

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;
    DEV_DESC    *RxBufDescPtrStart;
    DEV_DESC    *RxBufDescPtrCur;
    DEV_DESC    *RxBufDescPtrEnd;
    DEV_DESC    *TxBufDescPtrStart;
    DEV_DESC    *TxBufDescPtrCur;
    DEV_DESC    *TxBufDescCompPtr;                              /* See Note #3.                                         */
    DEV_DESC    *TxBufDescPtrEnd;
    CPU_INT16U   TxNRdyCtr;
#ifdef NET_MCAST_MODULE_EN
    CPU_INT16U   MulticastAddrCtr;
#endif
    LIST        *RxReadyListPtr;
    LIST        *RxBufferListPtr;
    LIST        *RxFreeListPtr;
} NET_DEV_DATA;


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

static  void           NetDev_Init_RX64          (NET_IF              *pif,
                                                NET_ERR            *perr);

static  void         NetDev_Start              (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void           NetDev_Start_RX64         (NET_IF             *pif,
                                                  NET_ERR               *perr);


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

static  void         NetDev_MII_Rd_RX64        (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *perr);

static  void         NetDev_MII_Wr             (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);

static  void         NetDev_MII_Wr_RX64        (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);

static  CPU_INT08U   NetDev_PhyRegHandler      (NET_IF             *pif,
                                                CPU_INT08U          mdo,
                                                CPU_INT08U          mmd,
                                                CPU_INT08U          mdc);

static  CPU_INT08U   NetDev_PhyRegHandler_RX64 (NET_IF             *pif,
                                                CPU_INT08U          mdo,
                                                CPU_INT08U          mmd,
                                                CPU_INT08U          mdc);

static  void         NetDev_Dly                (CPU_INT32U         dly);
                                                                        /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
static  void         NetDev_RxDescInit         (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_RxDescFreeAll      (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_TxDescInit         (NET_IF             *pif,
                                                NET_ERR            *perr);

static  CPU_BOOLEAN  NetDev_ISR_Rx             (NET_IF             *pif,
                                                DEV_DESC           *pdesc);



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
    CPU_REG32  EDMAC_EDMR;
    CPU_REG32  RESERVED0[0x01];
    CPU_REG32  EDMAC_EDTRR;
    CPU_REG32  RESERVED1[0x01];
    CPU_REG32  EDMAC_EDRRR;
    CPU_REG32  RESERVED2[0x01];
    CPU_REG32  EDMAC_TDLAR;
    CPU_REG32  RESERVED3[0x01];
    CPU_REG32  EDMAC_RDLAR;
    CPU_REG32  RESERVED4[0x01];
    CPU_REG32  EDMAC_EESR;
    CPU_REG32  RESERVED5[0x01];
    CPU_REG32  EDMAC_EESIPR;
    CPU_REG32  RESERVED6[0x01];
    CPU_REG32  EDMAC_TRSCER;
    CPU_REG32  RESERVED7[0x01];
    CPU_REG32  EDMAC_RMFCR;
    CPU_REG32  RESERVED8[0x01];
    CPU_REG32  EDMAC_TFTR;
    CPU_REG32  RESERVED9[0x01];
    CPU_REG32  EDMAC_FDR;
    CPU_REG32  RESERVED10[0x01];
    CPU_REG32  EDMAC_RMCR;
    CPU_REG32  RESERVED11[0x02];
    CPU_REG32  EDMAC_TFUCR;
    CPU_REG32  EDMAC_RFOCR;
    CPU_REG32  EDMAC_IOSR;
    CPU_REG32  EDMAC_FCFTR;
    CPU_REG32  RESERVED12[0x01];
    CPU_REG32  EDMAC_RPADIR;
    CPU_REG32  EDMAC_TRIMD;
    CPU_REG32  RESERVED13[0x12];
    CPU_REG32  EDMAC_RBWAR;
    CPU_REG32  EDMAC_RDFAR;
    CPU_REG32  RESERVED14[0x01];
    CPU_REG32  EDMAC_TBRAR;
    CPU_REG32  EDMAC_TDFAR;
    CPU_REG32  RESERVED15[0x09];

    CPU_REG32  ETHERC_ECMR;
    CPU_REG32  RESERVED17[0x01];
    CPU_REG32  ETHERC_RFLR;
    CPU_REG32  RESERVED18[0x01];
    CPU_REG32  ETHERC_ECSR;
    CPU_REG32  RESERVED19[0x01];
    CPU_REG32  ETHERC_ECSIPR;
    CPU_REG32  RESERVED20[0x01];
    CPU_REG32  ETHERC_PIR;
    CPU_REG32  RESERVED21[0x01];
    CPU_REG32  ETHERC_PSR;
    CPU_REG32  RESERVED22[0x05];
    CPU_REG32  ETHERC_RDMLR;
    CPU_REG32  RESERVED23[0x03];
    CPU_REG32  ETHERC_IPGR;
    CPU_REG32  ETHERC_APR;
    CPU_REG32  ETHERC_MPR;
    CPU_REG32  RESERVED24[0x01];
    CPU_REG32  ETHERC_RFCF;
    CPU_REG32  ETHERC_TPAUSER;
    CPU_REG32  ETHERC_TPAUSECR;
    CPU_REG32  ETHERC_BCFRR;
    CPU_REG32  RESERVED25[0x14];
    CPU_REG32  ETHERC_MAHR;
    CPU_REG32  RESERVED26[0x01];
    CPU_REG32  ETHERC_MALR;
    CPU_REG32  RESERVED27[0x01];
    CPU_REG32  ETHERC_TROCR;
    CPU_REG32  ETHERC_CDCR;
    CPU_REG32  ETHERC_LCCR;
    CPU_REG32  ETHERC_CNDCR;
    CPU_REG32  RESERVED28[0x01];
    CPU_REG32  ETHERC_CEFCR;
    CPU_REG32  ETHERC_FRECR;
    CPU_REG32  ETHERC_TSFRCR;
    CPU_REG32  ETHERC_TLFRCR;
    CPU_REG32  ETHERC_RFCR;
    CPU_REG32  ETHERC_MAFCR;
} NET_DEV;

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

const  NET_DEV_API_ETHER  NetDev_API_RX_EtherC = {                                   /* RX-EtherC dev API fnct ptrs :   */
                                                   &NetDev_Init,                     /*   Init/add                      */
                                                   &NetDev_Start,                    /*   Start                         */
                                                   &NetDev_Stop,                     /*   Stop                          */
                                                   &NetDev_Rx,                       /*   Rx                            */
                                                   &NetDev_Tx,                       /*   Tx                            */
                                                   &NetDev_AddrMulticastAdd,         /*   Multicast addr add            */
                                                   &NetDev_AddrMulticastRemove,      /*   Multicast addr remove         */
                                                   &NetDev_ISR_Handler,              /*   ISR handler                   */
                                                   &NetDev_IO_Ctrl,                  /*   I/O ctrl                      */
                                                   &NetDev_MII_Rd,                   /*   Phy reg rd                    */
                                                   &NetDev_MII_Wr                    /*   Phy reg wr                    */
                                                 };



const  NET_DEV_API_ETHER  NetDev_API_RX64_EtherC = {                                 /* RX64-EtherC0 dev API fnct ptrs :*/
                                                     &NetDev_Init_RX64,              /*   Init/add                      */
                                                     &NetDev_Start_RX64,             /*   Start                         */
                                                     &NetDev_Stop,                   /*   Stop                          */
                                                     &NetDev_Rx,                     /*   Rx                            */
                                                     &NetDev_Tx,                     /*   Tx                            */
                                                     &NetDev_AddrMulticastAdd,       /*   Multicast addr add            */
                                                     &NetDev_AddrMulticastRemove,    /*   Multicast addr remove         */
                                                     &NetDev_ISR_Handler,            /*   ISR handler                   */
                                                     &NetDev_IO_Ctrl,                /*   I/O ctrl                      */
                                                     &NetDev_MII_Rd_RX64,            /*   Phy reg rd                    */
                                                     &NetDev_MII_Wr_RX64,            /*   Phy reg wr                    */
                                                   };


/*
*********************************************************************************************************
*                                      LOCAL GLOBAL VARIABLE
*********************************************************************************************************
*/

static     CPU_INT32U         EPTPc_ResetDone = DEF_NO;         /* Flag that prevent to re-initiate the register         */

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
*                   (e) Initialize driver statistics & error counters
*                   (f) Allocate memory for device DMA descriptors
*                   (g) Initialize additional device registers
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
*                           NET_DEV_ERR_MEM_ALLOC       Memory allocation failed.
*                           NET_DEV_ERR_INVALID_CFG     Invalid device configuration.
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
*                   (a) pdev_cfg->RxBufAlignOctets
*                   (b) pdev_cfg->RxBufLargeSize
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
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbr_octets;
    LIST               *plist;
    CPU_INT16U          free_cnt;
    CPU_INT16U          ix;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;

    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* See Note #7.                                         */
                                                                /* Validate Rx buf size.                                */
    if ((pdev_cfg->RxBufLargeSize   % RX_BUF_SIZE_OCTETS)  != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets % RX_BUF_ALIGN_OCTETS) != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
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



                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate phy bus mode.                               */
    if ((pphy_cfg->BusMode != NET_PHY_BUS_MODE_MII) &&
        (pphy_cfg->BusMode != NET_PHY_BUS_MODE_RMII)) {
       *perr = NET_PHY_ERR_INVALID_CFG;
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

                                                                /* ------------- ENABLE NECESSARY CLOCKS -------------- */
                                                                /* Enable module clks (see Note #2).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------- INITIALIZE EXTERNAL GPIO CONTROLLER -------- */
                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbr_octets = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all Rx descriptors.  */
                   (CPU_SIZE_T  ) 16,                           /* Block alignment (see Note #10).                      */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    nbr_octets = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all Tx descriptors.  */
                   (CPU_SIZE_T  ) 16,                           /* Block alignment (see Note #10).                      */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }


    pdev_data->RxReadyListPtr  = (LIST *)0;
    pdev_data->RxBufferListPtr = (LIST *)0;
    pdev_data->RxFreeListPtr   = (LIST *)0;

    free_cnt = pdev_cfg->RxBufLargeNbr - pdev_cfg->RxDescNbr;
    for (ix = 0; ix < free_cnt; ix++) {
        plist = (LIST *)Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(LIST),
                                      (CPU_SIZE_T  ) 4,
                                      (CPU_SIZE_T *)&reqd_octets,
                                      (LIB_ERR    *)&lib_err);
        if (plist == (LIST *)0) {
           *perr = NET_DEV_ERR_MEM_ALLOC;
            return;
        }

        plist->Buffer = (void *)0;
        plist->Len    =  0;
        plist->Next   =  pdev_data->RxFreeListPtr;

        pdev_data->RxFreeListPtr = plist;
    }

    pdev->EDMAC_EDMR    =   EDMAC_RESET;                        /* Reset EDMAC.                                         */
    KAL_Dly(5);                                                 /* Min delay of 64 internal bus cycles.                 */

    pdev->ETHERC_ECMR  &= ~(ECMR_RX_EN |                        /* Disable Rx and Tx.                                   */
                            ECMR_TX_EN);

    pdev->ETHERC_ECSR   =   ECSR_ICD   |                        /* Clear all ETHERC status BFR, PSRTO, LCHNG, MPD, ICD. */
                            ECSR_MPD   |
                            ECSR_LCHNG |
                            ECSR_PSRTO |
                            ECSR_BFR;

    pdev->ETHERC_ECSIPR =   0x00000000u;                        /* Disable ALL dev interrupts.                          */
    pdev->ETHERC_RFLR   =   pdev_cfg->RxBufLargeSize;           /* Set maximum packet size.                             */
    pdev->ETHERC_IPGR   =   IPGR_IPG_96_BIT;                    /* Set interpacket gap size.                            */

    pdev->EDMAC_EESR    =   EESR_CLR;                           /* Clear all pending interrupts.                        */

                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    if (pphy_cfg->Duplex == NET_PHY_DUPLEX_HALF) {
        pdev->ETHERC_ECMR &= ~(ECMR_DUPLEX_FULL);
    } else {
        pdev->ETHERC_ECMR |=   ECMR_DUPLEX_FULL;
    }

    if (pphy_cfg->Spd == NET_PHY_SPD_10) {
        pdev->ETHERC_ECMR &= ~(ECMR_SPD_100);
    } else {
        pdev->ETHERC_ECMR |=   ECMR_SPD_100;
    }

   *perr = NET_DEV_ERR_NONE;
}
/*
*********************************************************************************************************
*                                         NetDev_Init_RX64()
*
* Description : (1) Initialize Network Driver Layer for the RX64:
*
*                   (a) Initialize required clock sources
*                   (b) Initialize external interrupt controller
*                   (c) Initialize external GPIO controller
*                   (d) Initialize driver state variables
*                   (e) Initialize driver statistics & error counters
*                   (f) Allocate memory for device DMA descriptors
*                   (g) Initialize additional device registers
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
*                           NET_DEV_ERR_MEM_ALLOC       Memory allocation failed.
*                           NET_DEV_ERR_INVALID_CFG     Invalid device configuration.
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
*                   (a) pdev_cfg->RxBufAlignOctets
*                   (b) pdev_cfg->RxBufLargeSize
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

static  void  NetDev_Init_RX64 (NET_IF   *pif,
                                NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER          *pdev_bsp;
    NET_PHY_CFG_ETHER          *pphy_cfg;
    NET_DEV_CFG_ETHER_RX64  *pdev_cfg;
    NET_DEV_DATA               *pdev_data;
    NET_DEV                    *pdev;
    NET_BUF_SIZE             buf_size_max;
    NET_BUF_SIZE             buf_ix;
    CPU_SIZE_T               reqd_octets;
    CPU_SIZE_T               nbr_octets;
    LIST                       *plist;
    CPU_INT16U               free_cnt;
    CPU_INT16U                   ix;
    LIB_ERR                  lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER      *)pif->Ext_Cfg;

    pdev_cfg = (NET_DEV_CFG_ETHER_RX64 *)pif->Dev_Cfg;
    pdev     = (NET_DEV                *)pdev_cfg->BaseAddr;    /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER      *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* See Note #7.                                         */
                                                                /* Validate Rx buf size.                                */
    if ((pdev_cfg->RxBufLargeSize   % RX_BUF_SIZE_OCTETS)  != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets % RX_BUF_ALIGN_OCTETS) != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
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



                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate phy bus mode.                               */
    if ((pphy_cfg->BusMode != NET_PHY_BUS_MODE_MII) &&
        (pphy_cfg->BusMode != NET_PHY_BUS_MODE_RMII)) {
       *perr = NET_PHY_ERR_INVALID_CFG;
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

                                                                /* ------------- ENABLE NECESSARY CLOCKS -------------- */
                                                                /* Enable module clks (see Note #2).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------- INITIALIZE EXTERNAL GPIO CONTROLLER -------- */
                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbr_octets = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all Rx descriptors.  */
                   (CPU_SIZE_T  ) 16,                           /* Block alignment (see Note #10).                      */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    nbr_octets = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */

    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all Tx descriptors.  */
                   (CPU_SIZE_T  ) 16,                           /* Block alignment (see Note #10).                      */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }


    pdev_data->RxReadyListPtr  = (LIST *)0;
    pdev_data->RxBufferListPtr = (LIST *)0;
    pdev_data->RxFreeListPtr   = (LIST *)0;

    free_cnt = pdev_cfg->RxBufLargeNbr - pdev_cfg->RxDescNbr;
    for (ix = 0; ix < free_cnt; ix++) {
        plist = (LIST *)Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(LIST),
                                      (CPU_SIZE_T  ) 4,
                                      (CPU_SIZE_T *)&reqd_octets,
                                      (LIB_ERR    *)&lib_err);
        if (plist == (LIST *)0) {
           *perr = NET_DEV_ERR_MEM_ALLOC;
            return;
        }

        plist->Buffer = (void *)0;
        plist->Len    =  0;
        plist->Next   =  pdev_data->RxFreeListPtr;

        pdev_data->RxFreeListPtr = plist;
    }

    if(EPTPc_ResetDone == DEF_NO){
        pdev_cfg->PTPRSTRegister |=  EPTPC_SWR_RESET;           /* Software reset of EPTPC module.                      */
        KAL_Dly(5);
        pdev_cfg->PTPRSTRegister &= ~EPTPC_SWR_RESET;
        EPTPc_ResetDone = DEF_YES;                                /* Flag to prevent resetting the module more than once. */
    }

    pdev->EDMAC_EDMR    =   EDMAC_RESET;                        /* Reset EDMAC.                                         */
    KAL_Dly(5);                                                 /* Min delay of 64 internal bus cycles.                 */

    pdev->ETHERC_ECMR  &= ~(ECMR_RX_EN |                        /* Disable Rx and Tx.                                   */
                            ECMR_TX_EN);

    pdev->ETHERC_ECSR   =   ECSR_ICD   |                        /* Clear all ETHERC status BFR, PSRTO, LCHNG, MPD, ICD. */
                            ECSR_MPD   |
                            ECSR_LCHNG |
                            ECSR_PSRTO |
                            ECSR_BFR;

    pdev->ETHERC_ECSIPR =   0x00000000u;                        /* Disable ALL dev interrupts.                          */
    pdev->ETHERC_RFLR   =   pdev_cfg->RxBufLargeSize;           /* Set maximum packet size.                             */
    pdev->ETHERC_IPGR   =   IPGR_IPG_96_BIT;                    /* Set interpacket gap size.                            */

    pdev->EDMAC_EESR    =   EESR_CLR;                           /* Clear all pending interrupts.                        */

                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    if (pphy_cfg->Duplex == NET_PHY_DUPLEX_HALF) {
        pdev->ETHERC_ECMR &= ~(ECMR_DUPLEX_FULL);
    } else {
        pdev->ETHERC_ECMR |=   ECMR_DUPLEX_FULL;
    }

    if (pphy_cfg->Spd == NET_PHY_SPD_10) {
        pdev->ETHERC_ECMR &= ~(ECMR_SPD_100);
    } else {
        pdev->ETHERC_ECMR |=   ECMR_SPD_100;
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
    CPU_REG16          *pflash_en_reg;
    CPU_INT08U         *pflash_hw_addr;
                                                                /* Renesas YRDKRX62N MAC addr range.                    */
    CPU_INT08U          hw_addr_range[8] = { 0x00, 0x00, 0x00, 0x30, 0x55, 0x08, 0x00, 0x00 };
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    CPU_BOOLEAN         valid;
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


                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg    =  DEF_NO;                                   /* See Notes #4 & #5.                                   */
    pflash_en_reg  = (CPU_REG16 *)DATA_FLASH_RD_EN_DFLRE1_REG_1;
   *pflash_en_reg  =  0xD280;                                   /* En rd of data flash block 15.                        */
    pflash_hw_addr = (CPU_INT08U *)0x107FF0;
                                                                /* Validate flash MAC addr for proper range.            */
    valid             =  Mem_Cmp(pflash_hw_addr, hw_addr_range, 6);
    if (valid == DEF_YES) {
        Mem_Copy(&hw_addr[0], &pflash_hw_addr[2], NET_IF_ETHER_ADDR_SIZE);

    } else {
        NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,               /* Get configured HW MAC address string, if any ...     */
                           &hw_addr[0],                         /* ... (see Note #5a).                                  */
                           &err);
        valid = (err == NET_ASCII_ERR_NONE) ? DEF_YES : DEF_NO;
    }

    if (valid == DEF_YES) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
        valid = (err == NET_IF_ERR_NONE) ? DEF_YES : DEF_NO;
    }

    if (valid == DEF_YES) {                                     /* If no errors, configure device    HW MAC address.    */
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
            hw_addr[0] = (pdev->ETHERC_MAHR >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->ETHERC_MAHR >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->ETHERC_MAHR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->ETHERC_MAHR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->ETHERC_MALR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->ETHERC_MALR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->ETHERC_MAHR = (((CPU_INT32U)hw_addr[0] << (3 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[1] << (2 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[2] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[3] << (0 * DEF_INT_08_NBR_BITS)));

        pdev->ETHERC_MALR = (((CPU_INT32U)hw_addr[4] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[5] << (0 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev_data->TxNRdyCtr = 0;

#ifdef  NET_MCAST_MODULE_EN
    pdev_data->MulticastAddrCtr = 0u;                           /* Initialize Rx multicast group ctr.                   */
#endif

    pdev->EDMAC_TRSCER  = 0x00000000;                           /* Copy-back status is RFE & TFE only.                  */
    pdev->EDMAC_TFTR    = 0x00000000;                           /* Tx FIFO threshold.                                   */
    pdev->EDMAC_FDR     = FDR_RX_FIFO_2048 |                    /* Cfg int generation settings                          */
                          FDR_TX_FIFO_2048;
    pdev->EDMAC_RMCR    = RMCR_CONT_RX;                         /* Set controller for continuous receive.               */
    pdev->EDMAC_FCFTR   = FCFTR_8_FRM;
#if CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE
    pdev->EDMAC_EDMR   |= EMDR_LITTLE_ENDIAN;
#endif
                                                                /* -------------------- CFG INT'S --------------------- */
    pdev->EDMAC_EESIPR  =  EESIPR_RX     |                      /* Enable dev interrupts.                               */
                           EESIPR_TX     |
                           EESIPR_RX_ERR;

                                                                /* ------------------ ENABLE RX & TX ------------------ */
    pdev->ETHERC_ECMR  |=  ECMR_RX_EN |                         /* Enable transmitter & receiver.                       */
                           ECMR_TX_EN;

    pdev->EDMAC_EDRRR   =  DEF_ENABLED;                         /* Enable EDMAC rx.                                     */

                                                                /* ----------------- START DMA FETCH ------------------ */
    KAL_Dly(3);

   *perr = NET_DEV_ERR_NONE;
}
/*
*********************************************************************************************************
*                                           NetDev_Start_RX64()
*
* Description : (1) Start network interface hardware for the RX64 :
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
*********************************************************************************************************
*/

static  void  NetDev_Start_RX64 (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER_RX64  *pdev_cfg;
    NET_DEV_DATA            *pdev_data;
    NET_DEV                 *pdev;
    CPU_INT08U               hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U               hw_addr_len;
    CPU_BOOLEAN              hw_addr_cfg;
    CPU_BOOLEAN              valid;
    NET_ERR                  err;
    CPU_INT32U                *symacrur_reg;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER_RX64 *)pif->Dev_Cfg;         /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA           *)pif->Dev_Data;        /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV                *)pdev_cfg->BaseAddr;   /* Overlay dev reg struct on top of dev base addr.      */

    symacrur_reg = (CPU_INT32U *)pdev_cfg->SYMACRURegister;     /* Obtain ptr to the EPTPC MAC Frame filtering upper... */
                                                                /* ... register SYMACRU (Specific to the RX64)          */

                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal( pif,                               /* See Note #3.                                         */
                             pdev_cfg->TxDescNbr,
                             perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }
                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg    =  DEF_NO;                                   /* See Notes #4 & #5.   t                       */

    NetASCII_Str_to_MAC( pdev_cfg->HW_AddrStr,                  /* Get configured HW MAC address string, if any ...     */
                        &hw_addr[0],                            /* ... (see Note #5a).                                  */
                        &err);
    valid = (err == NET_ASCII_ERR_NONE) ? DEF_YES : DEF_NO;


    if (valid == DEF_YES) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
        valid = (err == NET_IF_ERR_NONE) ? DEF_YES : DEF_NO;
    }

    if (valid == DEF_YES) {                                     /* If no errors, configure device    HW MAC address.    */
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
            hw_addr[0] = (pdev->ETHERC_MAHR >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->ETHERC_MAHR >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->ETHERC_MAHR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->ETHERC_MAHR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->ETHERC_MALR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->ETHERC_MALR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->ETHERC_MAHR = (((CPU_INT32U)hw_addr[0] << (3 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[1] << (2 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[2] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[3] << (0 * DEF_INT_08_NBR_BITS)));

        pdev->ETHERC_MALR = (((CPU_INT32U)hw_addr[4] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[5] << (0 * DEF_INT_08_NBR_BITS)));

        symacrur_reg[0]   = (((CPU_INT32U)hw_addr[0] << (2 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[2] << (0 * DEF_INT_08_NBR_BITS)));

        symacrur_reg[1]   = (((CPU_INT32U)hw_addr[3] << (2 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[4] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[5] << (0 * DEF_INT_08_NBR_BITS)));
    }
                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev_data->TxNRdyCtr = 0;

#ifdef  NET_MCAST_MODULE_EN
    pdev_data->MulticastAddrCtr = 0u;                           /* Initialize Rx multicast group ctr.                   */
#endif

    pdev->EDMAC_TRSCER  = 0x00000000;                           /* Copy-back status is RFE & TFE only.                  */
    pdev->EDMAC_TFTR    = 0x00000000;                           /* Tx FIFO threshold.                                   */
    pdev->EDMAC_FDR     = FDR_RX_FIFO_2048 |                    /* Cfg int generation settings                          */
                          FDR_TX_FIFO_2048;
    pdev->EDMAC_RMCR    = RMCR_CONT_RX;                         /* Set controller for continuous receive.               */
    pdev->EDMAC_FCFTR   = FCFTR_8_FRM;
#if CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE
    pdev->EDMAC_EDMR   |= EMDR_LITTLE_ENDIAN;
#endif
                                                                /* -------------------- CFG INT'S --------------------- */
    pdev->EDMAC_EESIPR  =  EESIPR_RX     |                      /* Enable dev interrupts.                               */
                           EESIPR_TX     |
                           EESIPR_RX_ERR;

                                                                /* ------------------ ENABLE RX & TX ------------------ */
    pdev->ETHERC_ECMR  |=  ECMR_RX_EN |                         /* Enable transmitter & receiver.                       */
                           ECMR_TX_EN;

    pdev->EDMAC_EDRRR   =  DEF_ENABLED;                         /* Enable EDMAC rx.                                     */

                                                                /* ----------------- START DMA FETCH ------------------ */
    KAL_Dly(3);

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
*               (3) All functions that require device register access MUST obtain reference to the
*                   device hardware register space PRIOR to attempting to access any registers.
*                   Register definitions SHOULD NOT be absolute & SHOULD use the provided base
*                   address within the device configuration structure, as well as the device
*                   register definition structure in order to properly resolve register addresses
*                   during run-time.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    MEM_POOL           *pmem_pool;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT08U          i;
    NET_ERR             err;
    LIB_ERR             lib_err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    pdev->ETHERC_ECMR  &= ~(ECMR_RX_EN |                        /* Disable transmitter & receiver.                      */
                            ECMR_TX_EN);

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    pdev->EDMAC_EESIPR &= ~(EESIPR_RX    |
                            EESIPR_TX    |
                            EESIPR_RX_ERR);

    pdev->EDMAC_EESR    =   EESR_CLR;                           /* Clear all interrupt flags.                           */

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdesc = pdev_data->TxBufDescCompPtr;
    for (i = 0; i < pdev_data->TxNRdyCtr; i++) {                /* Dealloc ALL tx bufs (see Note #2a2).                 */
        NetIF_TxDeallocTaskPost(pdesc->p_buf, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdesc = pdesc->next;
    }
    pdev_data->TxNRdyCtr = 0u;

                                                                /* ---------------- FREE TX DESC BLOCK ---------------- */
    pmem_pool = &pdev_data->TxDescPool;
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Tx descriptor block to Tx descriptor pool.    */
                     pdev_data->TxBufDescPtrStart,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Determine which receive descriptor caused the interrupt
*                   (b) Obtain pointer to data area to replace existing data area
*                   (c) Reconfigure descriptor with pointer to new data area
*                   (d) Set return values.  Pointer to received data area and size
*                   (e) Update current receive descriptor pointer
*                   (f) Increment statistic counters
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
*               (4) Some devices optionally include each receive packet's CRC in the received
*                   packet data & size.
*
*                   (a) CRCs might optionally be included at run-time or at build time. Each
*                       driver doesn't necessarily need to conditionally include or exclude
*                       the CRC at build time.  Instead, a device may include/exclude the code
*                       to subtract the CRC size from the packet size.
*
*                   (b) The CRC size should be subtracted from the receive packet size ONLY if
*                       the CRC was included in the received packet data.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_DATA  *pdev_data;
    LIST          *plist;
    CPU_INT08U    *pbuf;
    CPU_BOOLEAN    valid;
    NET_ERR        net_err;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();                                       /* Disable interrupts to alter shared data.             */
    plist = pdev_data->RxReadyListPtr;                          /* Get next ready buffer.                               */
    if (plist != (LIST *)0) {
        pdev_data->RxReadyListPtr = plist->Next;

       *size   = plist->Len;                                    /* Return the size of the received frame.               */
       *p_data = (CPU_INT08U *)plist->Buffer;                   /* Return a pointer to the newly received data area.    */

        plist->Len    =  0;
        plist->Buffer = (void *)0;

        plist->Next              = pdev_data->RxFreeListPtr;    /* Move list header into free list.                     */
        pdev_data->RxFreeListPtr = plist;
        CPU_CRITICAL_EXIT();                                    /* Restore interrupts.                                  */

       *perr = NET_DEV_ERR_NONE;

    } else {

        CPU_CRITICAL_EXIT();                                    /* Restore interrupts.                                  */

       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;

       *perr = NET_DEV_ERR_RX;
    }

    valid = DEF_TRUE;
    while (valid == DEF_TRUE) {
        pbuf = NetBuf_GetDataPtr((NET_IF        *)pif,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_TYPE  *)0,
                                 (NET_ERR       *)&net_err);
        if (net_err != NET_BUF_ERR_NONE) {
            valid = DEF_FALSE;

        } else {

            CPU_CRITICAL_ENTER();                               /* Disable interrupts to alter shared data.             */
            plist = pdev_data->RxFreeListPtr;
            if (plist != (LIST *)0) {
                pdev_data->RxFreeListPtr   = plist->Next;
                plist->Buffer              = pbuf;
                plist->Next                = pdev_data->RxBufferListPtr;
                pdev_data->RxBufferListPtr = plist;

                CPU_CRITICAL_EXIT();                            /* Restore interrupts.                                  */
            } else {
                CPU_CRITICAL_EXIT();                            /* Restore interrupts.                                  */
                valid = DEF_FALSE;

                NetBuf_FreeBufDataAreaRx(pif->Nbr, pbuf);       /* Return data area to Rx data area pool.               */
            }
        }
    }
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
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to data to transmit.
*
*               size    Size    of data to transmit.
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
*
*               (4)  The ACT descriptor bit is reserved for software use ONLY. This bit is set by software
*                    during transmission to indicate that the data ready bit for the descriptor is being set.
*                    When the DMA hardware completes transmission of the descriptor data, the ready bit is
*                    cleared and the ACT bit remains set. The transmit complete ISR handler then detects the
*                    cleared ready bit and set ACT bit and determines that the descriptor has completed
*                    transmission. The ACT bit is then cleared by the ISR handler.
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
    pdesc     = (DEV_DESC          *)pdev_data->TxBufDescPtrCur;/* Obtain ptr to next available Tx descriptor.          */

    if ((pdesc->status & ACT) > 0) {                            /* Find next available Tx descriptor (see Note #3).     */
        *perr = NET_DEV_ERR_TX_BUSY;
         return;
    }

    pdesc->p_buf   = p_data;                                    /* Configure desc with Tx data area address.              */
    pdesc->bufsize = size;                                      /* Configure desc with Tx data size.                      */

    pdev_data->TxBufDescPtrCur = pdesc->next;

    CPU_CRITICAL_ENTER();
    pdev_data->TxNRdyCtr++;

    pdesc->status |= FP0 |                                      /* Hardware owns desc, enable tx comp int.              */
                     FP1 |
                     ACT;                                       /* See Note #4.                                         */
    CPU_CRITICAL_EXIT();

                                                                /* Update curr desc ptr to point to next desc.          */
    if (pdev->EDMAC_EDTRR == 0x00000000u) {
        pdev->EDMAC_EDTRR  = 0x00000001u;
    }


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
* Note(s)     : (1) The multicast group counter MUST NOT be incremented during the first call since
*                   the interface is joining the "all-hosts" group. However, it must be incremented
*                   during each subsequent call.
*
*                   See also 'net_igmp.c  NetIGMP_Init()  Note #3'.
*
*               (2) Prevent the RFE (Receive Frame Error) bit to go high when RFS7 (Receive Multicast
*                   Address Frame) event is detected.
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

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

    if (pdev_data->MulticastAddrCtr >= 1u) {                    /* See Note #1.                                         */
        pdev->EDMAC_TRSCER |= TRSCER_RMAFCE;                    /* See Note #2.                                         */
        pdev_data->MulticastAddrCtr++;                          /* Inc multicast group ctr.                             */
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warning.          */
#endif

   (void)&paddr_hw;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&addr_hw_len;

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
* Note(s)     : (1) Allow the RFE (Receive Frame Error) bit to go high when RFS7 (Receive Multicast
*                   Address Frame) event is detected.
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

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

    if (pdev_data->MulticastAddrCtr > 1) {                      /* If the last multicast group is removed ...           */
        pdev_data->MulticastAddrCtr--;                          /* ... dec     multicast group ctr.                     */
    } else {
        pdev->EDMAC_TRSCER          = 0x00000000;               /* See Note #1.                                         */
        pdev_data->MulticastAddrCtr = 0u;                       /* Rst multicast group ctr.                             */
    }
#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warning.          */
#endif

   (void)&paddr_hw;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&addr_hw_len;

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
             duplex      = (pdev->ETHERC_ECMR & ECMR_DUPLEX_FULL) ? NET_PHY_DUPLEX_FULL : NET_PHY_DUPLEX_HALF;

             if (plink_state->Duplex != duplex) {
                 switch (plink_state->Duplex) {
                    case NET_PHY_DUPLEX_FULL:
                         pdev->ETHERC_ECMR |=   ECMR_DUPLEX_FULL;
                         break;

                    case NET_PHY_DUPLEX_HALF:
                         pdev->ETHERC_ECMR &= ~(ECMR_DUPLEX_FULL);
                         break;

                    default:
                         break;
                 }
             }

             spd = (pdev->ETHERC_ECMR & ECMR_SPD_100) ? NET_PHY_SPD_100 : NET_PHY_SPD_10;

             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {
                    case NET_PHY_SPD_10:
                         pdev->ETHERC_ECMR &= ~(ECMR_SPD_100);
                         break;

                    case NET_PHY_SPD_100:
                         pdev->ETHERC_ECMR |=   ECMR_SPD_100;
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
    CPU_INT08U  i;
    CPU_INT08U  bit;
    CPU_INT08U  mdo;
    CPU_INT08U  mmd;
    CPU_INT08U  mdc;
    CPU_INT16U  val;
    CPU_INT32U  tmp;
    CPU_INT32U  data;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

    data  = MII_DATA;                                           /* Build the MII Management Frame.                      */
    tmp   = PHY_READ;
    tmp <<= 28;
    data |= tmp;
    tmp   = phy_addr;
    tmp <<= 23;
    data |= tmp;
    tmp   = reg_addr;
    tmp <<= 18;
    data |= tmp;

    mmd   = 1;                                                  /* Specify data write.                                  */
    mdo   = 1;                                                  /* Initiate by sending Preamble and 32 consecutive 1's. */

    for (i = 0; i < 32; i++) {                                  /* Transmits on rising edge.                            */
        mdc   = 0;                                              /* Bring clock line low.                                */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
        mdc   = 1;                                              /* Bring clock line high.                               */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    }

    bit   = 1;
                                                                /* Send ST, OP, phy addr, and reg addr bits.            */
    for (i = 0; i < 14; i++) {                                  /* Transmits on rising edge.                            */
        mdc   =  0;                                             /* Bring clock line low.                                */
        tmp   =  data;                                          /* Get the next bit to send.                            */
        tmp >>= (32 - bit++);
        tmp  &=  BIT_MASK;
        mdo   = (CPU_INT08U) tmp;                               /* Put the next bit onto the Data out line.             */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
        mdc   =  1;                                             /* Bring clock line high.                               */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    }

    mdo   = 0;
    mdc   = 0;
    mmd   = 0;                                                  /* Bus release (end of write).                          */
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    mdc   = 1;
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    mdc   = 0;
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);

    val   = 0;
    bit   = 1;

    for (i = 0; i < 16; i++) {                                  /* Reads on rising edge.                                */
        mdc   = 1;                                              /* Bring clock line high.                               */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);

        tmp   = NetDev_PhyRegHandler(pif, mdo, mmd, mdc);       /* Read input.                                          */
        tmp <<= (16 - bit++);                                   /* Data in MSB, shift to correct position.              */
        val  |= tmp;
        mdc   = 0;                                              /* Bring clock line low.                                */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    }

    mdc   = 1;                                                  /* Bring clock line low.                                */
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    mdc   = 0;                                                  /* Bring clock line high.                               */
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    mdc   = 1;                                                  /* Bring clock line low.                                */
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);

   *p_data  = val;

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Rd_RX64()
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

static  void  NetDev_MII_Rd_RX64 (NET_IF      *pif,
                                  CPU_INT08U   phy_addr,
                                  CPU_INT08U   reg_addr,
                                  CPU_INT16U  *p_data,
                                  NET_ERR     *perr)
{
    CPU_INT08U  i;
    CPU_INT08U  bit;
    CPU_INT08U  mdo;
    CPU_INT08U  mmd;
    CPU_INT08U  mdc;
    CPU_INT16U  val;
    CPU_INT32U  tmp;
    CPU_INT32U  data;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif
    data  = MII_DATA;                                           /* Build the MII Management Frame.                      */
    tmp   = PHY_READ;
    tmp <<= 28;
    data |= tmp;
    tmp   = phy_addr;
    tmp <<= 23;
    data |= tmp;
    tmp   = reg_addr;
    tmp <<= 18;
    data |= tmp;

    mmd   = 1;                                                  /* Specify data write.                                  */
    mdo   = 1;                                                  /* Initiate by sending Preamble and 32 consecutive 1's. */

    for (i = 0; i < 32; i++) {                                  /* Transmits on rising edge.                            */
        mdc   = 0;                                              /* Bring clock line low.                                */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
        mdc   = 1;                                              /* Bring clock line high.                               */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    }

    bit   = 1;
                                                                /* Send ST, OP, phy addr, and reg addr bits.            */
    for (i = 0; i < 14; i++) {                                  /* Transmits on rising edge.                            */
        mdc   =  0;                                             /* Bring clock line low.                                */
        tmp   =  data;                                          /* Get the next bit to send.                            */
        tmp >>= (32 - bit++);
        tmp  &=  BIT_MASK;
        mdo   = (CPU_INT08U) tmp;                               /* Put the next bit onto the Data out line.             */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
        mdc   =  1;                                             /* Bring clock line high.                               */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    }

    mdo   = 0;
    mdc   = 0;
    mmd   = 0;                                                  /* Bus release (end of write).                          */
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    mdc   = 1;
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    mdc   = 0;
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);

    val   = 0;
    bit   = 1;

    for (i = 0; i < 16; i++) {                                  /* Reads on rising edge.                                */
        mdc   = 1;                                              /* Bring clock line high.                               */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);

        tmp   = NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);       /* Read input.                                          */
        tmp <<= (16 - bit++);                                   /* Data in MSB, shift to correct position.              */
        val  |= tmp;
        mdc   = 0;                                              /* Bring clock line low.                                */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    }

    mdc   = 1;                                                  /* Bring clock line low.                                */
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    mdc   = 0;                                                  /* Bring clock line high.                               */
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    mdc   = 1;                                                  /* Bring clock line low.                                */
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);

   *p_data  = val;

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Wr()
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
    CPU_INT08U  i;
    CPU_INT08U  bit;
    CPU_INT08U  mdo;
    CPU_INT08U  mmd;
    CPU_INT08U  mdc;
    CPU_INT32U  tmp;
    CPU_INT32U  val;


    val   = MII_DATA;                                           /* Build the MII Management Frame.                      */
    tmp   = PHY_WRITE;
    tmp <<= 28;
    val  |= tmp;
    tmp   = phy_addr;
    tmp <<= 23;
    val  |= tmp;
    tmp   = reg_addr;
    tmp <<= 18;
    val  |= tmp;
    tmp   = PHY_WRITE_TA;
    tmp <<= 16;
    val  |= tmp;
    tmp   = data;
    val  |= tmp;

    mmd  = 1;                                                   /* Specify val write.                                   */
    mdo  = 1;                                                   /* Initiate by sending Preamble and 32 consecutive 1's. */

    for (i = 0; i < 32; i++) {                                  /* Transmits on rising edge.                            */
        mdc   = 0;                                              /* Bring clock line low.                                */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
        mdc   = 1;                                              /* Bring clock line high.                               */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    }
                                                                /* Send ST, OP, phy addr, and reg addr bits.            */
    bit  = 1;
                                                                /* Followed by TA bit and val.                          */
    for (i = 0; i < 32; i++) {                                  /* Transmits on rising edge.                            */
        mdc   =  0;                                             /* Bring clock line low.                                */
        tmp   =  val;                                           /* Get the next bit to send.                            */
        tmp >>= (32 - bit++);
        tmp  &=  BIT_MASK;
        mdo   =  tmp;
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
        mdc   =  1;                                             /* Bring clock line high.                               */
        NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    }

    mdo  = 0;
    mdc  = 0;
    mmd  = 0;                                                   /* Bus release (end of write).                          */
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);
    mdc  = 1;
    NetDev_PhyRegHandler(pif, mdo, mmd, mdc);

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_MII_Wr_RX64()
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

static  void  NetDev_MII_Wr_RX64 (NET_IF      *pif,
                                  CPU_INT08U   phy_addr,
                                  CPU_INT08U   reg_addr,
                                  CPU_INT16U   data,
                                  NET_ERR     *perr)
{
    CPU_INT08U  i;
    CPU_INT08U  bit;
    CPU_INT08U  mdo;
    CPU_INT08U  mmd;
    CPU_INT08U  mdc;
    CPU_INT32U  tmp;
    CPU_INT32U  val;


    val   = MII_DATA;                                           /* Build the MII Management Frame.                      */
    tmp   = PHY_WRITE;
    tmp <<= 28;
    val  |= tmp;
    tmp   = phy_addr;
    tmp <<= 23;
    val  |= tmp;
    tmp   = reg_addr;
    tmp <<= 18;
    val  |= tmp;
    tmp   = PHY_WRITE_TA;
    tmp <<= 16;
    val  |= tmp;
    tmp   = data;
    val  |= tmp;

    mmd  = 1;                                                   /* Specify val write.                                   */
    mdo  = 1;                                                   /* Initiate by sending Preamble and 32 consecutive 1's. */

    for (i = 0; i < 32; i++) {                                  /* Transmits on rising edge.                            */
        mdc   = 0;                                              /* Bring clock line low.                                */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
        mdc   = 1;                                              /* Bring clock line high.                               */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    }
                                                                /* Send ST, OP, phy addr, and reg addr bits.            */
    bit  = 1;
                                                                /* Followed by TA bit and val.                          */
    for (i = 0; i < 32; i++) {                                  /* Transmits on rising edge.                            */
        mdc   =  0;                                             /* Bring clock line low.                                */
        tmp   =  val;                                           /* Get the next bit to send.                            */
        tmp >>= (32 - bit++);
        tmp  &=  BIT_MASK;
        mdo   =  tmp;
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
        mdc   =  1;                                             /* Bring clock line high.                               */
        NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    }

    mdo  = 0;
    mdc  = 0;
    mmd  = 0;                                                   /* Bus release (end of write).                          */
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);
    mdc  = 1;
    NetDev_PhyRegHandler_RX64(pif, mdo, mmd, mdc);

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_PhyRegHandler()
*
* Description : Performs a register write and delay.
*
* Argument(s) : mdo         Value to output.
*               mmd         Read/Write (0 - Read, 1- Write).
*               mdc         Clock pulse value.
*
* Return(s)   : One bit data read from Phy (ignore on a write).
*
* Caller(s)   : NetDev_MII_Rd(),
*               NetDev_MII_Wr().
*********************************************************************************************************
*/

static  CPU_INT08U  NetDev_PhyRegHandler (NET_IF      *pif,
                                          CPU_INT08U   mdo,
                                          CPU_INT08U   mmd,
                                          CPU_INT08U   mdc)
{
    NET_DEV            *pdev;
    CPU_INT32U          pir;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev = (NET_DEV *)0x000C0000;                               /* Overlay dev reg struct on top of dev base addr.      */


    pir                = (mdo << 1);                            /* Arrange the arguments into the expected format       */
    pir               |=  mmd;
    pir              <<=  1;
    pir               |=  mdc;
    pir               &=  0x0000000F;

    pdev->ETHERC_PIR   =  pir;                                  /* Register write                                       */

    NetDev_Dly(10);                                             /* Perform a small delay (max clock rate is 25 MHz)     */

    pir                =  pdev->ETHERC_PIR;                     /* Read register                                        */
    pir              >>=  3;                                    /* Filter unwanted data                                 */
    pir               &=  0x00000001;

    return (CPU_INT08U) pir;                                    /* Return the input data bit value                      */
}


/*
*********************************************************************************************************
*                                       NetDev_PhyRegHandler_RX64()
*
* Description : Performs a register write and delay.
*
* Argument(s) : mdo         Value to output.
*               mmd         Read/Write (0 - Read, 1- Write).
*               mdc         Clock pulse value.
*
* Return(s)   : One bit data read from Phy (ignore on a write).
*
* Caller(s)   : NetDev_MII_Rd(),
*               NetDev_MII_Wr().
*********************************************************************************************************
*/

static  CPU_INT08U  NetDev_PhyRegHandler_RX64 (NET_IF      *pif,
                                                 CPU_INT08U   mdo,
                                                 CPU_INT08U   mmd,
                                                 CPU_INT08U   mdc)
{
    NET_DEV_CFG_ETHER_RX64  *pdev_cfg;
    NET_DEV                 *pdev;
    CPU_INT32U               pir;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER_RX64 *)pif->Dev_Cfg;          /* Obtain ptr to the dev cfg struct.                    */

    pdev     = (NET_DEV *)pdev_cfg->PHYCtrl_BaseAddr;           /* Overlay dev reg struct on top of dev base addr.      */

    pir                = (mdo << 1);                            /* Arrange the arguments into the expected format       */
    pir               |=  mmd;
    pir              <<=  1;
    pir               |=  mdc;
    pir               &=  0x0000000F;

    pdev->ETHERC_PIR   =  pir;                                  /* Register write                                       */

    NetDev_Dly(10);                                             /* Perform a small delay (max clock rate is 25 MHz)     */

    pir                =  pdev->ETHERC_PIR;                     /* Read register                                        */
    pir              >>=  3;                                    /* Filter unwanted data                                 */
    pir               &=  0x00000001;

    return (CPU_INT08U) pir;                                    /* Return the input data bit value                      */
}

/*
*********************************************************************************************************
*                                            NetDev_Dly()
*
* Description : Performs a  delay.
*
* Argument(s) : dly         Number of nop() to perform.
*
* Return(s)   : none.
*
* Caller(s)   : NetNIC_PhyRegHandler().
*
*********************************************************************************************************
*/

static void  NetDev_Dly (CPU_INT32U dly)
{
    while (dly-- > 0) {
        ;
    }
}


/*
*********************************************************************************************************
*                                         NetDev_RxDescInit()
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
    MEM_POOL           *pmem_pool;
    DEV_DESC           *pdesc;
    LIST               *plist;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbr_octets;
    CPU_INT16U          free_cnt;
    CPU_INT16U          i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS ---------------- */
                                                                /* See Note #3.                                         */
    pmem_pool  = &pdev_data->RxDescPool;
    nbr_octets =  pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                            (CPU_SIZE_T) nbr_octets,
                                            (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxBufDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrCur   = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrEnd   = (DEV_DESC *)pdesc + (pdev_cfg->RxDescNbr - 1);

                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->status  = ACT;
        pdesc->bufsize = pdev_cfg->RxBufLargeSize;
        pdesc->size    = 0;
        pdesc->p_buf   = NetBuf_GetDataPtr((NET_IF        *)pif,
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

        if (pdesc == (pdev_data->RxBufDescPtrEnd)) {            /* Set DL bit on last descriptor in list.               */
            pdesc->status |= DL;
            pdesc->next    = pdev_data->RxBufDescPtrStart;
        } else {
            pdesc->next    = pdesc + 1;                         /* Point to next descriptor in list.                    */
        }

        pdesc++;
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start address.    */
    pdev->EDMAC_RDLAR = (CPU_INT32U)pdev_data->RxBufDescPtrStart;

    free_cnt = pdev_cfg->RxBufLargeNbr - pdev_cfg->RxDescNbr;
    for (i = 0; i < free_cnt; i++) {
        plist                    = pdev_data->RxFreeListPtr;    /* Get list header from free list.                      */
        pdev_data->RxFreeListPtr = plist->Next;

        plist->Buffer = NetBuf_GetDataPtr((NET_IF        *)pif,
                                          (NET_TRANSACTION)NET_TRANSACTION_RX,
                                          (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                          (NET_BUF_SIZE   )NET_IF_IX_RX,
                                          (NET_BUF_SIZE  *)0,
                                          (NET_BUF_SIZE  *)0,
                                          (NET_BUF_TYPE  *)0,
                                          (NET_ERR       *)perr);

        if (*perr != NET_BUF_ERR_NONE) {
                                                                /* Return list header to free list.                     */
             plist->Next              = pdev_data->RxFreeListPtr;
             pdev_data->RxFreeListPtr = plist;
             break;
        }

                                                                /* Store on buffer list.                                */
        plist->Next                = pdev_data->RxBufferListPtr;
        pdev_data->RxBufferListPtr = plist;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_RxDescFreeAll()
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
    LIST               *plist;
    LIST               *plist_next;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pmem_pool = &pdev_data->RxDescPool;
    pdesc     =  pdev_data->RxBufDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */
        pdesc_data = pdesc->p_buf;
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

    plist = pdev_data->RxBufferListPtr;
    while (plist != (LIST *)0) {
        plist_next = plist->Next;
        pdesc_data = plist->Buffer;
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        plist->Buffer = (void *)0;
        plist->Len    =  0;

        plist->Next              = pdev_data->RxFreeListPtr;
        pdev_data->RxFreeListPtr = plist;

        plist = plist_next;
    }
    pdev_data->RxBufferListPtr = (LIST *)0;

    plist = pdev_data->RxReadyListPtr;
    while (plist != (LIST *)0) {
        plist_next = plist->Next;
        pdesc_data = plist->Buffer;
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        plist->Buffer = (void *)0;
        plist->Len    =  0;

        plist->Next              = pdev_data->RxFreeListPtr;
        pdev_data->RxFreeListPtr = plist;

        plist = plist_next;
    }
    pdev_data->RxReadyListPtr = (LIST *)0;

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
                     pdev_data->RxBufDescPtrStart,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_FAULT;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetDev_TxDescInit()
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

static  void  NetDev_TxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    MEM_POOL           *pmem_pool;
    CPU_SIZE_T          nbr_octets;
    CPU_INT16U          i;
    LIB_ERR             lib_err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS ---------------- */
                                                                /* See Note #3.                                         */
    pmem_pool  = &pdev_data->TxDescPool;
    nbr_octets =  pdev_cfg->TxDescNbr * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                            (CPU_SIZE_T) nbr_octets,
                                            (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_FAULT;
        return;
    }

                                                                /* -------------- INIT DESCRIPTOR PTRS ---------------- */
    pdev_data->TxBufDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->TxBufDescPtrCur   = (DEV_DESC *)pdesc;
    pdev_data->TxBufDescCompPtr  = (DEV_DESC *)pdesc;
    pdev_data->TxBufDescPtrEnd   = (DEV_DESC *)pdesc + (pdev_cfg->TxDescNbr - 1);

                                                                /* --------------- INIT TX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Initialize Tx descriptor ring.                       */
        pdesc->status  = 0;
        pdesc->bufsize = 0;
        pdesc->size    = 0;
        pdesc->p_buf   = 0;

        if (pdesc == (pdev_data->TxBufDescPtrEnd)) {            /* Set DL bit on last descriptor in list.               */
            pdesc->status |= DL;
            pdesc->next    = pdev_data->TxBufDescPtrStart;
        } else {
            pdesc->next    = pdesc + 1;                         /* Point to next descriptor in list.                    */
        }

        pdesc++;
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Tx desc start address.    */
    pdev->EDMAC_TDLAR = (CPU_INT32U)pdev_data->TxBufDescPtrStart;

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_ISR_Handler()
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
    void               *p_buf;
    CPU_DATA            reg_val;
    CPU_BOOLEAN         valid;
    NET_ERR             err;


                                                                /* --- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS -- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------- DETERMINE ISR TYPE IF UNKNOWN ---------- */
    reg_val          = pdev->EDMAC_EESR;                        /* Determine ISR type.                                  */
    pdev->EDMAC_EESR = reg_val;                                 /* Clear interrupts                                     */

                                                                /* ----------------- PROCESS INTERRUPT ---------------- */
    if (((reg_val & EESIPR_RX)     != 0) ||
        ((reg_val & EESIPR_RX_ERR) != 0)) {

        valid = DEF_TRUE;
        while (valid == DEF_TRUE) {
            pdesc = (DEV_DESC *)pdev_data->RxBufDescPtrCur;     /* Obtain ptr to next ready descriptor.                 */

            if ((pdesc->status & ACT) == 0) {                   /* Descriptor ready to receive.                         */
                valid = NetDev_ISR_Rx(pif, pdesc);

                pdev_data->RxBufDescPtrCur = pdesc->next;
            } else {
                valid = DEF_FALSE;
            }
        }

        if (pdev->EDMAC_EDRRR == 0x00000000u) {                 /* If the receiver has stopped, restart                 */
            pdev->EDMAC_EDRRR  = 0x00000001u;
        }
    }

    if ((reg_val & EESIPR_TX) != 0) {
        while (pdev_data->TxNRdyCtr > 0) {
            pdesc = pdev_data->TxBufDescCompPtr;
            if ((pdesc->status & ACT) != 0) {
                 break;
            }

            p_buf = pdesc->p_buf;

            pdev_data->TxBufDescCompPtr = pdesc->next;
            pdev_data->TxNRdyCtr--;

            NetIF_TxDeallocTaskPost(p_buf, &err);
            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available.       */
        }
    }
}


/*
*********************************************************************************************************
*                                           NetDev_ISR_Rx()
*
* Description : Interrupt Service Routine for Receive Handler.
*
* Argument(s) : pif         Pointer to interface requiring service.
*
* Return(s)   : DEF_YES, if packet received is valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetDev_ISR_Handler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetDev_ISR_Rx (NET_IF    *pif,
                                    DEV_DESC  *pdesc)
{
    NET_DEV_DATA  *pdev_data;
    LIST          *plist_buf;
    LIST          *plist_ready;
    void          *p_buf;
    CPU_BOOLEAN    valid;
    CPU_BOOLEAN    signal;
    NET_ERR        err;


    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    valid     =  DEF_YES;
    signal    =  DEF_FALSE;



    if ((pdesc->status & FE) == FE) {                           /* Chk for frame err .                                  */
        if((pdesc->status & RMAF) != RMAF){                     /* Chk if the error is only multi cast frame.           */
               valid = DEF_NO;
        }
    }
    if ((pdesc->status & (FP0 | FP1)) != (FP0 | FP1)) {         /* Chk frame split in multiple desc.                    */
        valid = DEF_NO;
    }

    if (pdev_data->RxBufferListPtr == (LIST *)0) {
        valid  = DEF_NO;
        signal = DEF_TRUE;
    }

    pdesc->status &= ~(FP1    |
                       FP0    |
                       FE     |
                       RFOVER |
                       RAD    |
                       RMAF   |
                       RRF    |
                       RTLF   |
                       RTSF   |
                       PRE    |
                       CERF);

    if (valid == DEF_YES) {
        plist_buf = pdev_data->RxBufferListPtr;
        pdev_data->RxBufferListPtr = plist_buf->Next;

        p_buf = plist_buf->Buffer;

        plist_buf->Buffer = pdesc->p_buf;
        plist_buf->Len    = pdesc->size;
        plist_buf->Next   = (LIST *)0;

        if (pdev_data->RxReadyListPtr == (LIST *)0) {
            pdev_data->RxReadyListPtr  = plist_buf;

        } else {

            plist_ready = pdev_data->RxReadyListPtr;
            while (plist_ready != (LIST *)0) {
                if (plist_ready->Next == (LIST *)0) {
                    break;
                }
                plist_ready = plist_ready->Next;
            }

            plist_ready->Next = plist_buf;
        }

        pdesc->p_buf = p_buf;
        pdesc->size  = 0;
    }

    if ((valid  == DEF_YES ) ||
        (signal == DEF_TRUE)) {
        NetIF_RxTaskSignal(pif->Nbr, &err);                          /* Signal Net IF RxQ Task of new frame.                     */
    }

    pdesc->status |= ACT;
    pdesc->size    = 0;

    return (valid);
}

#endif  /* NET_IF_ETHER_MODULE_EN */
