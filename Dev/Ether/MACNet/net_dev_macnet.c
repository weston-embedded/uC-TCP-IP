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
*                                            KINETIS MACNET
*
* Filename : net_dev_macnet.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_MACNET_MODULE

#include  <KAL/kal.h>
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_macnet.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO                              65000    /* MII read write timeout.                              */
#define  MIB_REG_CTR_OFFSET_START                      0x200    /* MIB stat reg start offset.                           */
#define  MIB_REG_CTR_OFFSET_END                        0x2E0    /* MIB stat reg end   offset.                           */
#define  RX_BUF_SIZE_OCTETS                               16    /* Rx bufs MUST be multiple of 16 bytes.                */
#define  RX_BUF_ALIGN_OCTETS                              16    /* Rx bufs MUST be aligned  to 16 byte boundary.        */


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
*           (3) Various CPU's both large and small endian contain FEC's.  Since the DMA fetches
*               descriptors using 32-bit words, it is necessary to define the two 16 bit
*               fields in the order represented by the CPU data bus endianess.
*
*           (4) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*/

                                                                /* ------------- DMA DESCRIPTOR DATA TYPE ------------- */
                                                                /* See Note #2 & #3.                                    */
typedef  struct  dev_desc {
    CPU_REG16    status;
    CPU_REG16    len;
    CPU_INT08U  *p_data;
} DEV_DESC;

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    CPU_INT16U   RxNRdyCtr;
    DEV_DESC    *RxBufDescPtrCur;
    DEV_DESC    *RxBufDescPtrStart;
    DEV_DESC    *RxBufDescPtrEnd;
    DEV_DESC    *TxBufDescPtrCur;
    DEV_DESC    *TxBufDescPtrStart;
    DEV_DESC    *TxBufDescPtrEnd;
    DEV_DESC    *TxBufDescCompPtr;                              /* See Note #4.                                         */
    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U   MulticastAddrHashBitCtr[64];
#endif
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

static  void  NetDev_RxDescFreeAll      (NET_IF             *pif,
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
                                                                            /* MACNET dev API fnct ptrs :               */
const  NET_DEV_API_ETHER  NetDev_API_MACNET = { NetDev_Init,                /*   Init/add                               */
                                                NetDev_Start,               /*   Start                                  */
                                                NetDev_Stop,                /*   Stop                                   */
                                                NetDev_Rx,                  /*   Rx                                     */
                                                NetDev_Tx,                  /*   Tx                                     */
                                                NetDev_AddrMulticastAdd,    /*   Multicast addr add                     */
                                                NetDev_AddrMulticastRemove, /*   Multicast addr remove                  */
                                                NetDev_ISR_Handler,         /*   ISR handler                            */
                                                NetDev_IO_Ctrl,             /*   I/O ctrl                               */
                                                NetDev_MII_Rd,              /*   Phy reg rd                             */
                                                NetDev_MII_Wr               /*   Phy reg wr                             */
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
    CPU_REG32  RESERVED1;
    CPU_REG32  ENET_EIR;
    CPU_REG32  ENET_EIMR;
    CPU_REG32  RESERVED2;
    CPU_REG32  ENET_RDAR;
    CPU_REG32  ENET_TDAR;
    CPU_REG32  RESERVED3[3];
    CPU_REG32  ENET_ECR;
    CPU_REG32  RESERVED4[6];
    CPU_REG32  ENET_MMFR;
    CPU_REG32  ENET_MSCR;
    CPU_REG32  RESERVED5[7];
    CPU_REG32  ENET_MIBC;
    CPU_REG32  RESERVED6[7];
    CPU_REG32  ENET_RCR;
    CPU_REG32  RESERVED7[15];
    CPU_REG32  ENET_TCR;
    CPU_REG32  RESERVED8[7];
    CPU_REG32  ENET_PALR;
    CPU_REG32  ENET_PAUR;
    CPU_REG32  ENET_OPD;
    CPU_REG32  RESERVED9[10];
    CPU_REG32  ENET_IAUR;
    CPU_REG32  ENET_IALR;
    CPU_REG32  ENET_GAUR;
    CPU_REG32  ENET_GALR;
    CPU_REG32  RESERVED10[7];
    CPU_REG32  ENET_TFWR;
    CPU_REG32  RESERVED11[14];
    CPU_REG32  ENET_RDSR;
    CPU_REG32  ENET_TDSR;
    CPU_REG32  ENET_MRBR;
    CPU_REG32  RESERVED12;
    CPU_REG32  ENET_RSFL;
    CPU_REG32  ENET_RSEM;
    CPU_REG32  ENET_RAEM;
    CPU_REG32  ENET_RAFL;
    CPU_REG32  ENET_TSEM;
    CPU_REG32  ENET_TAEM;
    CPU_REG32  ENET_TAFL;
    CPU_REG32  ENET_TIPG;
    CPU_REG32  ENET_FTRL;
    CPU_REG32  RESERVED13[3];
    CPU_REG32  ENET_TACC;
    CPU_REG32  ENET_RACC;
    CPU_REG32  RESERVED14[142];
    CPU_REG32  ENET_ATCR;
    CPU_REG32  ENET_ATVR;
    CPU_REG32  ENET_ATOFF;
    CPU_REG32  ENET_ATPER;
    CPU_REG32  ENET_ATCOR;
    CPU_REG32  ENET_ATINC;
    CPU_REG32  ENET_ATSTMP;
    CPU_REG32  RESERVED15[122];
    CPU_REG32  ENET_TCSR0;
    CPU_REG32  ENET_TCCR0;
    CPU_REG32  ENET_TCSR1;
    CPU_REG32  ENET_TCCR1;
    CPU_REG32  ENET_TCSR2;
    CPU_REG32  ENET_TCCR2;
    CPU_REG32  ENET_TCSR3;
    CPU_REG32  ENET_TCCR3;
} NET_DEV;


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/
                                                                /* Bit definitions and macros for ENET_EIR.             */
#define  ENET_EIR_TS_TIMER  (0x00008000)
#define  ENET_EIR_TS_AVAIL  (0x00010000)
#define  ENET_EIR_WAKEUP    (0x00020000)
#define  ENET_EIR_PLR       (0x00040000)
#define  ENET_EIR_UN        (0x00080000)
#define  ENET_EIR_RL        (0x00100000)
#define  ENET_EIR_LC        (0x00200000)
#define  ENET_EIR_EBERR     (0x00400000)
#define  ENET_EIR_MII       (0x00800000)
#define  ENET_EIR_RXB       (0x01000000)
#define  ENET_EIR_RXF       (0x02000000)
#define  ENET_EIR_TXB       (0x04000000)
#define  ENET_EIR_TXF       (0x08000000)
#define  ENET_EIR_GRA       (0x10000000)
#define  ENET_EIR_BABT      (0x20000000)
#define  ENET_EIR_BABR      (0x40000000)

                                                                /* Bit definitions and macros for ENET_EIMR.            */
#define  ENET_EIMR_TS_TIMER (0x00008000)
#define  ENET_EIMR_TS_AVAIL (0x00010000)
#define  ENET_EIMR_WAKEUP   (0x00020000)
#define  ENET_EIMR_PLR      (0x00040000)
#define  ENET_EIMR_UN       (0x00080000)
#define  ENET_EIMR_RL       (0x00100000)
#define  ENET_EIMR_LC       (0x00200000)
#define  ENET_EIMR_EBERR    (0x00400000)
#define  ENET_EIMR_MII      (0x00800000)
#define  ENET_EIMR_RXB      (0x01000000)
#define  ENET_EIMR_RXF      (0x02000000)
#define  ENET_EIMR_TXB      (0x04000000)
#define  ENET_EIMR_TXF      (0x08000000)
#define  ENET_EIMR_GRA      (0x10000000)
#define  ENET_EIMR_BABT     (0x20000000)
#define  ENET_EIMR_BABR     (0x40000000)

                                                                /* Bit definitions and macros for ENET_RDAR.            */
#define  RDAR_R_DES_ACTIVE  (0x01000000)

                                                                /* Bit definitions and macros for ENET_TDAR.            */
#define  TDAR_X_DES_ACTIVE  (0x01000000)

                                                                /* Bit definitions and macros for ENET_ECR.              */
#define  ENET_ECR_RESET     (0x00000001)
#define  ENET_ECR_ETHER_EN  (0x00000002)
#define  ENET_ECR_MAGIC_EN  (0x00000004)
#define  ENET_ECR_SLEEP_EN  (0x00000008)
#define  ENET_ECR_EN1588_EN (0x00000010)
#define  ENET_ECR_DEBUG_EN  (0x00000040)
#define  ENET_ECR_STOPEN    (0x00000080)

                                                                /* Bit definitions and macros for ENET_MMFR.            */
#define  MMFR_DATA(x)       (((x) & 0x0000FFFF) <<  0)
#define  MMFR_TA(x)         (((x) & 0x00000003) << 16)
#define  MMFR_RA(x)         (((x) & 0x0000001F) << 18)
#define  MMFR_PA(x)         (((x) & 0x0000001F) << 23)
#define  MMFR_OP(x)         (((x) & 0x00000003) << 28)
#define  MMFR_ST(x)         (((x) & 0x00000003) << 30)
#define  MMFR_ST_01         (0x40000000)
#define  MMFR_OP_READ       (0x20000000)
#define  MMFR_OP_WRITE      (0x10000000)
#define  MMFR_TA_10         (0x00020000)

                                                                /* Bit definitions and macros for ENET_MSCR.            */
#define  MSCR_MII_SPEED(x)  (((x) & 0x0000003F) <<  1)
#define  MSCR_DIS_PREAMBLE  (0x00000080)

                                                                /* Bit definitions and macros for ENET_MIBC.            */
#define  MIBC_MIB_CLEAR     (0x20000000)
#define  MIBC_MIB_IDLE      (0x40000000)
#define  MIBC_MIB_DISABLE   (0x80000000)

                                                                /* Bit definitions and macros for ENET_RCR.             */
#define  ENET_RCR_LOOP      (0x00000001)
#define  ENET_RCR_DRT       (0x00000002)
#define  ENET_RCR_MII_MODE  (0x00000004)
#define  ENET_RCR_PROM      (0x00000008)
#define  ENET_RCR_BC_REJ    (0x00000010)
#define  ENET_RCR_FCE       (0x00000020)
#define  ENET_RCR_RMII_MODE (0x00000100)
#define  ENET_RCR_RMII_10T  (0x00000200)
#define  ENET_RCR_PADEN     (0x00001000)
#define  ENET_RCR_PAUFWD    (0x00002000)
#define  ENET_RCR_CRCFWD    (0x00004000)
#define  ENET_RCR_CFEN      (0x00008000)
#define  ENET_RCR_NLC       (0x40000000)
#define  ENET_RCR_GRS       (0x80000000)
#define  ENET_RCR_MAX_FL(x) (((x) & 0x000007FF) << 16)

                                                                /* Bit definitions and macros for ENET_TCR.             */
#define  ENET_TCR_GTS       (0x00000001)
#define  ENET_TCR_FDEN      (0x00000004)
#define  ENET_TCR_TFC_PAUSE (0x00000008)
#define  ENET_TCR_RFC_PAUSE (0x00000010)
#define  ENET_TCR_ADDINS    (0x00000100)
#define  ENET_TCR_CRCFWD    (0x00000200)
#define  ENET_TCR_ADDSEL(x) (((x) & 0x000000E0) << 5)


#define  ENET_TFWR_STRFWD           DEF_BIT_08
#define  ENET_TFWR_TFWR            (0x0000003F)

#define  ENET_TACC_PROCHK           DEF_BIT_04
#define  ENET_TACC_IPCHK            DEF_BIT_03

#define  ENET_RACC_LINEDIS          DEF_BIT_06
#define  ENET_RACC_PRODIS           DEF_BIT_02
#define  ENET_RACC_IPDIS            DEF_BIT_01


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/

#if (CPU_CFG_ENDIAN_TYPE == CPU_ENDIAN_TYPE_LITTLE)             /* ARM, PPC & other little endian CPU's.                */
    #define RX_BD_E                  0x0080
    #define RX_BD_R01                0x0040
    #define RX_BD_W                  0x0020
    #define RX_BD_R02                0x0010
    #define RX_BD_L                  0x0008
    #define RX_BD_M                  0x0001
    #define RX_BD_BC                 0x8000
    #define RX_BD_MC                 0x4000
    #define RX_BD_LG                 0x2000
    #define RX_BD_NO                 0x1000
    #define RX_BD_CR                 0x0400
    #define RX_BD_OV                 0x0200
    #define RX_BD_TR                 0x0100

    #define TX_BD_R                  0x0080
    #define TX_BD_TO1                0x0040
    #define TX_BD_W                  0x0020
    #define TX_BD_TO2                0x0010
    #define TX_BD_L                  0x0008
    #define TX_BD_TC                 0x0004
    #define TX_BD_ABC                0x0002
#else                                                           /* Coldfire & other big endian CPU's.                   */
    #define RX_BD_E                  0x8000
    #define RX_BD_R01                0x4000
    #define RX_BD_W                  0x2000
    #define RX_BD_R02                0x1000
    #define RX_BD_L                  0x0800
    #define RX_BD_M                  0x0100
    #define RX_BD_BC                 0x0080
    #define RX_BD_MC                 0x0040
    #define RX_BD_LG                 0x0020
    #define RX_BD_NO                 0x0010
    #define RX_BD_CR                 0x0004
    #define RX_BD_OV                 0x0002
    #define RX_BD_TR                 0x0001

    #define TX_BD_R                  0x8000
    #define TX_BD_TO1                0x4000
    #define TX_BD_W                  0x2000
    #define TX_BD_TO2                0x1000
    #define TX_BD_L                  0x0800
    #define TX_BD_TC                 0x0400
    #define TX_BD_ABC                0x0200
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
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
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
    CPU_SIZE_T          nbytes;
    CPU_INT32U          clk;
    CPU_INT08U          mii_speed;
    CPU_ADDR            addr = DEF_NULL;
    CPU_SIZE_T          size = 0;
    NET_CTR             ctr;
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


    if ((pphy_cfg       == (void *)0       ) ||
        (pphy_cfg->Type != NET_PHY_TYPE_EXT)) {                 /* MACNET must have ext Phy.                               */
                                                                /* Call to NetIF_Start() does not specify Phy API.      */
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

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
    pdev->ENET_EIMR = 0x00000000;                               /* Disable ALL MACNET interrupts.                       */
    pdev->ENET_EIR  = 0xFFFFFFFF;                               /* Clear all pending interrupts.                        */
                                                                /* Initialize ext int ctrl'r (see Note #3).             */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* -------- INITIALIZE EXTERNAL GPIO CONTROLLER ------- */
                                                                /* Configure Ethernet Controller GPIO (see Note #4).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


#if (CPU_CFG_CACHE_MGMT_EN == DEF_ENABLED)
    if (pdev_cfg->RxBufPoolType != NET_IF_MEM_TYPE_MAIN) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }

    if (pdev_cfg->TxBufPoolType != NET_IF_MEM_TYPE_MAIN) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }

    if (pdev_cfg->MemAddr == 0) {                               /* The dedicated address must contain descriptor's      */
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* location                                             */
         return;
    }

    addr = pdev_cfg->MemAddr;
    size = pdev_cfg->MemSize;

#else
    if (pdev_cfg->RxBufPoolType == NET_IF_MEM_TYPE_DEDICATED) {
        addr = pdev_cfg->MemAddr;
        size = pdev_cfg->MemSize;
    }
#endif

                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);            /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) addr,
                   (CPU_SIZE_T  ) size,
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbytes,                       /* Block size large enough to hold all Rx descriptors.  */
                   (CPU_SIZE_T  ) 16,                           /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


    nbytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);            /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) addr,
                   (CPU_SIZE_T  ) size,
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbytes,                       /* Block size large enough to hold all Tx descriptors.  */
                   (CPU_SIZE_T  ) 16,                           /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */
    pdev->ENET_TCR = ENET_TCR_GTS;                              /* Set the Graceful Transmit Stop bit.                  */


    pdev->ENET_EIR  =  ENET_EIR_GRA;                            /* Clear the GRA event.                                 */

    pdev->ENET_RCR = 0;
    pdev->ENET_TCR = 0;

    pdev->ENET_ECR  =  0;                                       /* Disable the FEC.                                     */
    pdev->ENET_TCR &= ~ENET_TCR_GTS;                            /* Clear GTS bit so frames can be tx'd when restarted.  */


    pdev->ENET_ECR = ENET_ECR_RESET;                            /* Perform soft reset of MACNET.                        */
    ctr = 0;
    while ((pdev->ENET_ECR & ENET_ECR_RESET) != 0) {
        KAL_Dly(1);
        ctr++;
        if (ctr >= 1000) {
           *perr = NET_DEV_ERR_FAULT;
            return;
        }
    }
                                                                /* Set maximum Rx frame len.                            */
    pdev->ENET_RCR = ENET_RCR_MAX_FL(pdev_cfg->RxBufLargeSize);


    pdev->ENET_TCR |= ENET_TCR_FDEN;                            /* !!!! Set full duplex despite link duplex unknown.    */


                                                                /* Retrieve MACNET module clk freq.                     */
    clk = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    mii_speed = (clk / (2 * 2500000)) - 1;                      /* Calculate MII_SPEED prescaler value                  */
    if (mii_speed == 0u) {
        mii_speed = 0x3Fu;                                      /* Set prescaler to max val if calculation fails.       */
    }

    pdev->ENET_RACC = 0;
#ifdef NET_IPV4_CHK_SUM_OFFLOAD_RX
    pdev->ENET_RACC |= DEF_BIT_01;
#endif
#if ((defined(NET_TCP_CHK_SUM_OFFLOAD_RX)) || \
     (defined(NET_UDP_CHK_SUM_OFFLOAD_RX)) || \
     (defined(NET_ICMP_CHK_SUM_OFFLOAD_RX)))
    pdev->ENET_RACC |= DEF_BIT_02;
#endif


    pdev->ENET_TACC = 0;
    pdev->ENET_TFWR = 0;
#ifdef NET_IPV4_CHK_SUM_OFFLOAD_TX
    pdev->ENET_TACC |= DEF_BIT_03;
    pdev->ENET_TFWR |= DEF_BIT_08;
#endif
#if ((defined(NET_TCP_CHK_SUM_OFFLOAD_TX)) || \
     (defined(NET_UDP_CHK_SUM_OFFLOAD_TX)) || \
     (defined(NET_ICMP_CHK_SUM_OFFLOAD_TX)))
    pdev->ENET_TACC |= DEF_BIT_04;
    pdev->ENET_TFWR |= DEF_BIT_08;
#endif

    pdev->ENET_MSCR = MSCR_MII_SPEED(mii_speed);                /* Set MII bus freq less than or equal to 2.5Mhz.       */

                                                                /* Initialize MIB counters to 0.                        */
    pdev->ENET_MIBC |= MIBC_MIB_CLEAR;                          /* Reset all statistics counters to 0.                  */
    pdev->ENET_MIBC  = 0;                                       /* Enable MIB counters.                                 */



    pdev->ENET_TFWR = ENET_TFWR_STRFWD;
    pdev->ENET_TACC = 0;

    pdev->ENET_RACC = ENET_RACC_LINEDIS;

#if ((defined(NET_ICMP_CHK_SUM_OFFLOAD_RX)) ||  \
     (defined(NET_TCP_CHK_SUM_OFFLOAD_RX))  ||  \
     (defined(NET_UDP_CHK_SUM_OFFLOAD_RX)))

    pdev->ENET_RACC  |= ENET_RACC_PRODIS;
#endif

#ifdef NET_IPv4_CHK_SUM_OFFLOAD_RX
    pdev->ENET_RACC  |= ENET_RACC_IPDIS;
#endif


#if ((defined(NET_ICMP_CHK_SUM_OFFLOAD_TX)) ||  \
     (defined(NET_TCP_CHK_SUM_OFFLOAD_TX))  ||  \
     (defined(NET_UDP_CHK_SUM_OFFLOAD_TX)))

    pdev->ENET_TACC  |= ENET_TACC_PROCHK;
#endif

#ifdef NET_IPv4_CHK_SUM_OFFLOAD_TX
    pdev->ENET_TACC  |= ENET_TACC_PROCHK;
#endif

                                                                /* -------------- CFG PHY OPERATING MODE -------------- */
    pdev->ENET_RCR |= ENET_RCR_MII_MODE;                        /* MII_MODE bit must always be set.                     */
    if (pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII) {
        pdev->ENET_RCR |= ENET_RCR_RMII_MODE;                   /* Conditionally enable RMII mode.                      */
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
            hw_addr[0] = (pdev->ENET_PALR >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (pdev->ENET_PALR >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (pdev->ENET_PALR >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (pdev->ENET_PALR >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (pdev->ENET_PAUR >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (pdev->ENET_PAUR >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
        pdev->ENET_PALR = (((CPU_INT32U)hw_addr[0] << (3 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[1] << (2 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[2] << (1 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[3] << (0 * DEF_INT_08_NBR_BITS)));

        pdev->ENET_PAUR = (((CPU_INT32U)hw_addr[4] << (3 * DEF_INT_08_NBR_BITS)) |
                           ((CPU_INT32U)hw_addr[5] << (2 * DEF_INT_08_NBR_BITS)));
    }


                                                                /* ------------------ CFG MULTICAST ------------------- */
#ifdef NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

                                                                /* Individual and Multicast addresses should be 0.      */
    pdev->ENET_IAUR = 0;                                        /* Individual address upper register.                   */
    pdev->ENET_IALR = 0;                                        /* Individual address lower register.                   */
    pdev->ENET_GAUR = 0;                                        /* Group (multicast) address upper register.            */
    pdev->ENET_GALR = 0;                                        /* Group (multicast) address lower register.            */


                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev_data->RxNRdyCtr = 0;                                   /* No pending frames to process (see Note #2).          */


    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }
                                                                /* -------------------- CFG INT'S --------------------- */
    pdev->ENET_EIR  = (ENET_EIR_RXF  | ENET_EIR_RXB);           /* Clear the Rx interrupt flags.                        */
    pdev->ENET_EIR  = (ENET_EIR_TXF  | ENET_EIR_TXB);           /* Clear the Tx interrupt flags.                        */
    pdev->ENET_EIMR = (ENET_EIMR_RXF | ENET_EIMR_TXF);          /* Enable MACNET interrupts.                            */


                                                                /* ------------------ ENABLE RX & TX ------------------ */
    pdev->ENET_ECR  = ENET_ECR_ETHER_EN;                        /* Enable the MACNET.                                   */



                                                                /* ----------------- START DMA FETCH ------------------ */
    pdev->ENET_RDAR = RDAR_R_DES_ACTIVE;                        /* Tell HW that there is an empty Rx buffer produced.   */



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
    MEM_POOL           *pmem_pool;
    CPU_INT08U          i;
    CPU_INT32U          data_addr;
    LIB_ERR             lib_err;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    pdev->ENET_ECR  &=  ~ENET_ECR_ETHER_EN;                 /* Disable the MACNET.                                  */


                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    pdev->ENET_EIMR &= ~(ENET_EIMR_RXF | ENET_EIMR_TXF);        /* Disable MACNET interrupts.                           */
    pdev->ENET_EIR  &= ~(ENET_EIR_RXF  | ENET_EIR_RXB);         /* Clear the Rx interrupt flags.                        */
    pdev->ENET_EIR  &= ~(ENET_EIR_TXF  | ENET_EIR_TXB);         /* Clear the Tx interrupt flags.                        */


                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    pdesc = &pdev_data->TxBufDescPtrStart[0];
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Dealloc ALL tx bufs (see Note #2a2).                 */
        data_addr = (CPU_INT32U)pdesc->p_data;
        MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data), data_addr);
        NetIF_TxDeallocTaskPost(pdesc->p_data, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */

        pdesc++;
    }

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    pmem_pool = &pdev_data->TxDescPool;
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
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
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Decrement frame counter
*                   (b) Determine which receive descriptor caused the interrupt
*                   (c) Obtain pointer to data area to replace existing data area
*                   (d) Reconfigure descriptor with pointer to new data area
*                   (e) Set return values.  Pointer to received data area and size
*                   (f) Update current receive descriptor pointer
*                   (g) Increment statistic counters.
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
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    CPU_INT08U    *pbuf_new;
    CPU_INT16S     len;
    CPU_INT32U     data_addr;
    CPU_INT16U     len_desc;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdesc     = (DEV_DESC          *)pdev_data->RxBufDescPtrCur;/* Obtain ptr to next ready descriptor.                 */


                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
    if (((pdesc->status & RX_BD_E ) != 0) ||                    /* Validate buf & check for over-run (see Note #3a).    */
        ((pdesc->status & RX_BD_OV) >  0)) {
        NetDev_RxDescPtrCurInc(pif);                            /* If overrun, drop packet.                             */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

    len_desc = (CPU_INT16U)pdesc->len;
    MEM_VAL_SET_INT16U_BIG(((CPU_INT32U)&pdesc->len), len_desc & 0xFFFF);
                                                                /* --------------- OBTAIN FRAME LENGTH ---------------- */
    len = pdesc->len - NET_IF_ETHER_FRAME_CRC_SIZE;             /* Determine len value to return, minus 4 byte CRC      */

    if (len < 0) {
        NetDev_RxDescPtrCurInc(pif);                            /* Invalid len, drop packet (see Note #3b).             */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
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
         NetDev_RxDescPtrCurInc(pif);                           /* Free the current descriptor.                         */
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
         return;
    }

    data_addr = (CPU_INT32U)pdesc->p_data;
        MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data)    ,  data_addr);

   *size   =  len;                                              /* Return the size of the received frame.               */
   *p_data = (CPU_INT08U *)pdesc->p_data;                       /* Return a pointer to the newly received data area.    */

                                                                /* Update the descriptor to point to a new data area.   */
    MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data)    ,  pbuf_new);



    NetDev_RxDescPtrCurInc(pif);                                /* Free the current descriptor.                         */
    CPU_DCACHE_RANGE_INV(*p_data, *size);
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
*
*               (4)  The T01 descriptor bit is reserved for software use ONLY. This bit is set by software
*                    during transmission to indicate that the data ready bit for the descriptor is being set.
*                    When the DMA hardware completes transmission of the descriptor data, the ready bit is
*                    cleared and the T01 bit remains set. The transmit complete ISR handler then detects the
*                    cleared ready bit and set T01 bit and determines that the descriptor has completed
*                    transmission. The T01 bit is then cleared by the ISR handler.
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
    CPU_INT16U          desc_status;
    CPU_INT16U          wrap;
    CPU_SR_ALLOC();
    CPU_INT32U          data_addr;
    CPU_INT16U          len_desc;


    CPU_DCACHE_RANGE_FLUSH(p_data, size);
                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
    pdesc     = (DEV_DESC          *)pdev_data->TxBufDescPtrCur;/* Obtain ptr to next available Tx descriptor.          */

    CPU_CRITICAL_ENTER();                                       /* This routine reads shared data. Disable interrupts!  */
    desc_status = pdesc->status & TX_BD_R;
    CPU_CRITICAL_EXIT();                                        /* Enable interrupts.                                   */

    if (desc_status == 0) {                                     /* Find next available Tx descriptor (see Note #3).     */
        wrap          = pdesc->status & TX_BD_W;                /* Obtain value of WRAP bit.                            */
        pdesc->p_data = p_data;                                 /* Configure descriptor with Tx data area address.      */

        data_addr = (CPU_INT32U)pdesc->p_data;
        MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data)    ,  data_addr);

        pdesc->len    = size;                                   /* Configure descriptor with Tx data size.              */

        len_desc = (CPU_INT16U)pdesc->len;
        MEM_VAL_SET_INT16U_BIG(((CPU_INT32U)&pdesc->len), len_desc & 0xFFFF);

        pdesc->status = (wrap     |                             /* Hardware ownes the descriptor, enable Tx Comp. Int's */
                         TX_BD_R  |
                         TX_BD_L  |
                         TX_BD_TC |
                         TX_BD_TO1);                            /* See note #4.                                         */

        pdev->ENET_TDAR = TDAR_X_DES_ACTIVE;                    /* Instruct DMA to transmit descriptor data.            */


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

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
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
                                (CPU_INT16U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    hash    = (crc >> 26) & 0x3F;                               /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26) & 0x1F;                               /* Obtain least 5 significant bits for reg bit sel.     */
    reg_sel = (crc >> 31) & 0x01;                               /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    if (reg_sel == 0) {                                         /* Set multicast hash reg bit.                          */
        pdev->ENET_GALR |= (1 << bit_sel);
    } else {
        pdev->ENET_GAUR |= (1 << bit_sel);
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
                                (CPU_INT16U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

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
        pdev->ENET_GALR &= ~(1 << bit_sel);
    } else {
        pdev->ENET_GAUR &= ~(1 << bit_sel);
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
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

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
             duplex      =  NET_PHY_DUPLEX_UNKNOWN;
             if (plink_state->Duplex != duplex) {
                 switch (plink_state->Duplex) {
                    case NET_PHY_DUPLEX_FULL:
                         pdev->ENET_TCR |=  ENET_TCR_FDEN;      /* !!!! ENET_ECR[ETHER_EN] must equal 0 ...             */
                         break;                                 /* ... before changing ENET_TCR.                        */

                    case NET_PHY_DUPLEX_HALF:
                         pdev->ENET_TCR &= ~ENET_TCR_FDEN;      /* !!!! ENET_ECR[ETHER_EN] must equal 0 ...             */
                         break;                                 /* ... before changing ENET_TCR.                        */

                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {                    /* !!!! Update speed register setting on device.        */
                    case NET_PHY_SPD_10:
                         break;

                    case NET_PHY_SPD_100:
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
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    pdev->ENET_EIR  = ENET_EIR_MII;                             /* Clear the MII interrupt bit.                         */

    pdev->ENET_MMFR = MMFR_ST_01        |                       /* Execute a Phy register read command.                 */
                      MMFR_OP_READ      |
                      MMFR_PA(phy_addr) |
                      MMFR_RA(reg_addr) |
                      MMFR_TA_10;

    timeout = MII_REG_RD_WR_TO;
    while (timeout > 0) {                                       /* Wait for the interrupt bit to become set.            */
        timeout--;
        if ((pdev->ENET_EIR & ENET_EIR_MII) > 0) {
            break;
        }
    }

    if (timeout == 0) {                                         /* Return an error if a timeout occured.                */
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
       *p_data = 0;
        return;
    }

    pdev->ENET_EIR = ENET_EIR_MII;                              /* Clear the interrupt bit.                             */

   *p_data = pdev->ENET_MMFR & 0xFFFF;

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
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    pdev->ENET_EIR  = ENET_EIR_MII;                             /* Clear the MII interrupt bit.                         */

    pdev->ENET_MMFR = MMFR_ST_01        |                       /* Write to the Phy register.                           */
                      MMFR_OP_WRITE     |
                      MMFR_PA(phy_addr) |
                      MMFR_RA(reg_addr) |
                      MMFR_TA_10        |
                      MMFR_DATA(data);


    timeout = MII_REG_RD_WR_TO;
    while (timeout > 0) {                                       /* Wait for the interrupt bit to become set.            */
        timeout--;
        if ((pdev->ENET_EIR & ENET_EIR_MII) > 0) {
            break;
        }
    }

    if(timeout == 0) {                                          /* Return an error if a timeout occured.                */
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

    pdev->ENET_EIR = ENET_EIR_MII;                              /* Clear the interrupt bit.                             */

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
    MEM_POOL           *pmem_pool;
    DEV_DESC           *pdesc;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbytes;
    CPU_INT16U          i;
    CPU_INT32U          data_addr;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool = &pdev_data->RxDescPool;
    nbytes    =  pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdesc     = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                           (CPU_SIZE_T) nbytes,
                                           (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxBufDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrCur   = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrEnd   = (DEV_DESC *)pdesc + (pdev_cfg->RxDescNbr - 1);

                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->status = RX_BD_E;
        pdesc->len    = 0;
        pdesc->p_data = NetBuf_GetDataPtr((NET_IF        *)pif,
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

        if (pdesc == (pdev_data->RxBufDescPtrEnd)) {            /* Set WRAP bit on last descriptor in list.             */
            pdesc->status |= RX_BD_W;
        }

        CPU_DCACHE_RANGE_INV(pdesc->p_data, NET_IF_ETHER_FRAME_MAX_SIZE);

        data_addr = (CPU_INT32U)pdesc->p_data;
        MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data)    ,  data_addr);

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start address.    */
    pdev->ENET_MRBR =  pdev_cfg->RxBufLargeSize;                /* Set maximum receive buffer size.                     */
                                                                /* Cfg DMA descriptor ring start address.               */
    pdev->ENET_RDSR = (CPU_INT32U)pdev_data->RxBufDescPtrStart;


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
    CPU_INT32U          data_addr;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pmem_pool = &pdev_data->RxDescPool;
    pdesc     =  pdev_data->RxBufDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */

        data_addr = (CPU_INT32U)pdesc->p_data;
        MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data), data_addr);

        pdesc_data = pdesc->p_data;
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
                     pdev_data->RxBufDescPtrStart,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_RxDescPtrCurInc()
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
    NET_DEV            *pdev;
    DEV_DESC           *pdesc;
    CPU_INT16U          wrap;
    CPU_SR_ALLOC();

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    pdesc = pdev_data->RxBufDescPtrCur;                         /* Obtain pointer to current Rx descriptor.             */
    wrap  = pdesc->status & RX_BD_W;                            /* Determine wrap bit status.                           */

                                                                /* --------------- FREE THE DESCRIPTOR ---------------- */
    CPU_CRITICAL_ENTER();                                       /* The counter and descriptor updates must be atomic.   */
    if (pdev_data->RxNRdyCtr > 0) {
        pdev_data->RxNRdyCtr--;                                 /* One less frame to process.                           */
    }
    pdesc->status = wrap | RX_BD_E;                             /* Return descriptor to the DMA engine.                 */
    CPU_CRITICAL_EXIT();                                        /* ---------------------------------------------------- */

    if (pdev_data->RxBufDescPtrCur != pdev_data->RxBufDescPtrEnd) {
        pdev_data->RxBufDescPtrCur++;                           /* Point to next Buffer Descriptor.                     */
    } else {                                                    /* Wrap around end of descriptor list if necessary.     */
        pdev_data->RxBufDescPtrCur  = pdev_data->RxBufDescPtrStart;
    }

    pdev->ENET_RDAR = RDAR_R_DES_ACTIVE;                        /* Tell HW that there is an empty Rx buffer produced.   */
}


/*
*********************************************************************************************************
*                                          NetDev_TxDescInit()
*
* Description : (1) This function initializes the Tx descriptor list for the specified interface :
*
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
    CPU_SIZE_T          nbytes;
    CPU_INT16U          i;
    LIB_ERR             lib_err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool = &pdev_data->TxDescPool;
    nbytes    =  pdev_cfg->TxDescNbr * sizeof(DEV_DESC);
    pdesc     = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                           (CPU_SIZE_T) nbytes,
                                           (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->TxBufDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->TxBufDescPtrCur   = (DEV_DESC *)pdesc;
    pdev_data->TxBufDescCompPtr  = (DEV_DESC *)pdesc;
    pdev_data->TxBufDescPtrEnd   = (DEV_DESC *)pdesc + (pdev_cfg->TxDescNbr - 1);

                                                                /* --------------- INIT TX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Initialize Tx descriptor ring.                       */
        pdesc->status = TX_BD_L | TX_BD_TC;
        pdesc->len    = 0;

        if (pdesc == (pdev_data->TxBufDescPtrEnd)) {            /* Set WRAP bit on last descriptor in list.             */
            pdesc->status |= TX_BD_W;
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
    pdev->ENET_TDSR = (CPU_INT32U)pdev_data->TxBufDescPtrStart; /* Configure the DMA with the Tx desc start address.    */


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
    NET_ERR             err;
    CPU_INT16U          desc_status;
    CPU_INT32U          data_addr;


   (void)&type;                                                 /* Prevent compiler warning if arg 'type' not req'd.    */


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    reg_val = pdev->ENET_EIR;                                   /* Read the interrupt flag register.                    */

                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if ((reg_val & (ENET_EIR_RXF | ENET_EIR_RXB)) > 0) {
        pdev->ENET_EIR = (ENET_EIR_RXF | ENET_EIR_RXB);         /* Clear the receive flags.                             */

        n_rdy = 0;
        pdesc = pdev_data->RxBufDescPtrStart;

        for (i = 0; i < pdev_cfg->RxDescNbr; i++) {             /* Loop throughout Rx descriptor ring.                  */
            if ((pdesc->status & RX_BD_E) == 0) {
                n_rdy++;
            }
            pdesc++;
        }

        n_new = n_rdy - pdev_data->RxNRdyCtr;                   /* Determine how many NEW packets have been received.   */
        while (n_new > 0) {
            NetIF_RxTaskSignal(pif->Nbr, &err);                 /* Signal Net IF RxQ Task of new frame.                 */
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
                      n_new = 0;                                /* Break loop to prevent further posting when Q full.   */
                      break;
            }
        }

    }

                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if ((reg_val & (ENET_EIR_TXF | ENET_EIR_TXB)) > 0) {
        pdev->ENET_EIR = (ENET_EIR_TXF | ENET_EIR_TXB);         /* Clear the transmit flags.                            */

        pdesc = pdev_data->TxBufDescCompPtr;                    /* Get the first Tx descriptor.                         */
        desc_status = pdesc->status;

        while (((desc_status & TX_BD_R) == 0) &&                /* Post ALL Tx'd buffers to dealloc task (see Note #4). */
               ((desc_status & TX_BD_TO1) > 0)) {

            pdesc->status  &= ~TX_BD_TO1;                       /* Return ownership of the descriptor to software.      */

            data_addr = (CPU_INT32U)pdesc->p_data;
            MEM_VAL_SET_INT32U_BIG(((CPU_INT32U)&pdesc->p_data), data_addr);

            NetIF_TxDeallocTaskPost(pdesc->p_data, &err);       /* Free the Tx buffer for reuse.                        */
            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available.       */

                                                                /* ------------ GET THE NEXT TX DESCRIPTOR ------------ */
            if (pdev_data->TxBufDescCompPtr != pdev_data->TxBufDescPtrEnd) {
                pdev_data->TxBufDescCompPtr++;
            } else {                                            /* Wrap around end of descriptor list if necessary.     */
                pdev_data->TxBufDescCompPtr = pdev_data->TxBufDescPtrStart;
            }

            pdesc = pdev_data->TxBufDescCompPtr;
            desc_status = pdesc->status;
        }
    }
}


#endif /* NET_IF_ETHER_MODULE_EN */

