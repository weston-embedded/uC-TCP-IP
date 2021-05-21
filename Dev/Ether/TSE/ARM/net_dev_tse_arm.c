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
*                                    Altera Triple Speed Ethernet
*
* Filename : net_dev_tse.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This version of the TSE driver has only been tested on an ARM-Cortex-A9 using the ARM
*                DS-5 compiler.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_TSE_MODULE
#include  <lib_def.h>

#include  <bsp_net.h>
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_tse.h"
#include  <net_dev_tse_addr_cfg.h>


#include  <sys/alt_cache.h>
#include  <altera_avalon_tse.h>
#include  <altera_avalon_sgdma.h>
#include  <altera_avalon_sgdma_regs.h>
#include  <altera_avalon_sgdma_descriptor.h>
#include  <triple_speed_ethernet_regs.h>


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

#define  MII_REG_RD_WR_TO                              10000    /* MII read write timeout.                              */
#define  RX_BUF_ALIGN_OCTETS                               4    /* See Note #1.                                         */
#define  SGDMA_BUSY_TIMEOUT                          1000000
#define  SGDMA_INT_MASK                           0x00000004


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/
                                                                /* Command Config Register Bit Defines                  */
#define  TX_ENA                   DEF_BIT_00
#define  RX_ENA                   DEF_BIT_01
#define  XON_GEN                  DEF_BIT_02
#define  ETH_SPEED                DEF_BIT_03
#define  PROMIS_EN                DEF_BIT_04
#define  PAD_EN                   DEF_BIT_05
#define  CRC_FWD                  DEF_BIT_06
#define  PAUSE_FWD                DEF_BIT_07
#define  PAUSE_IGNORE             DEF_BIT_08
#define  TX_ADDR_INS              DEF_BIT_09
#define  HD_ENA                   DEF_BIT_10
#define  EXCESS_COL               DEF_BIT_11
#define  LATE_COL                 DEF_BIT_12
#define  SW_RESET_TSE             DEF_BIT_13
#define  MHASH_SEL                DEF_BIT_14
#define  LOOP_ENA                 DEF_BIT_15
#define  TX_ADDR_SEL0             DEF_BIT_16
#define  TX_ADDR_SEL1             DEF_BIT_17
#define  TX_ADDR_SEL2             DEF_BIT_18
#define  MAGIC_ENA                DEF_BIT_19
#define  SLEEP                    DEF_BIT_20
#define  WAKEUP                   DEF_BIT_21
#define  XOFF_GEN                 DEF_BIT_22
#define  CNTL_FRM_ENA             DEF_BIT_23
#define  NO_LGTH_CHECK            DEF_BIT_24
#define  ENA_10                   DEF_BIT_25
#define  RX_ERR_DISC              DEF_BIT_26
#define  DISABLE_RD_TIMEOUT       DEF_BIT_27
#define  CNT_RESET                DEF_BIT_31

                                                                /* SGDMA Control Register Bit Defines                   */
#define  IE_ERROR                 DEF_BIT_00
#define  IE_EOP_ENCOUNTERED       DEF_BIT_01
#define  IE_DESCRIPTOR_COMPLETED  DEF_BIT_02
#define  IE_CHAIN_COMPLETED       DEF_BIT_03
#define  IE_GLOBAL                DEF_BIT_04
#define  RUN                      DEF_BIT_05
#define  STOP_DMA_ER              DEF_BIT_06
#define  IE_MAX_DESC_PROCESSED    DEF_BIT_07
#define  MAX_DESC_PROCESSED00     DEF_BIT_08
#define  MAX_DESC_PROCESSED01     DEF_BIT_09
#define  MAX_DESC_PROCESSED02     DEF_BIT_10
#define  MAX_DESC_PROCESSED03     DEF_BIT_11
#define  MAX_DESC_PROCESSED04     DEF_BIT_12
#define  MAX_DESC_PROCESSED05     DEF_BIT_13
#define  MAX_DESC_PROCESSED06     DEF_BIT_14
#define  MAX_DESC_PROCESSED07     DEF_BIT_15
#define  SW_RESET_SGDMA           DEF_BIT_16
#define  PARK                     DEF_BIT_17
#define  CLEAR_INTERRUPT          DEF_BIT_31

                                                                /* SGDMA Status Register Bit Defines                    */
#define  ERROR                    DEF_BIT_00
#define  EOP_ENCOUNTERED          DEF_BIT_01
#define  DESCRIPTOR_COMPLETED     DEF_BIT_02
#define  CHAIN_COMPLETED          DEF_BIT_03
#define  BUSY                     DEF_BIT_04

                                                                /* SGDMA Descriptor Error Bit Defines                   */
#define  DESC_STATUS_ERROR0       DEF_BIT_00
#define  DESC_STATUS_ERROR1       DEF_BIT_01
#define  DESC_STATUS_ERROR2       DEF_BIT_02
#define  DESC_STATUS_ERROR3       DEF_BIT_03
#define  DESC_STATUS_ERROR4       DEF_BIT_04
#define  DESC_STATUS_ERROR5       DEF_BIT_05
#define  DESC_STATUS_ERROR6       DEF_BIT_06
#define  DESC_STATUS_ERROR7       DEF_BIT_07


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/
                                                                /* Descriptor Control Bit Defines                      */
#define  GENERATE_EOP             DEF_BIT_00
#define  READ_FIXED_ADDRESS       DEF_BIT_01
#define  WRITE_FIXED_ADDRESS      DEF_BIT_02
#define  OWNED_BY_HW              DEF_BIT_07


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
                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    MEM_POOL              RxDescPool;
    MEM_POOL              TxDescPool;
    alt_sgdma_descriptor *RxBufDescPtrStart;
    alt_sgdma_descriptor *RxBufDescPtrCur;
    alt_sgdma_descriptor *RxBufDescPtrEnd;
    alt_sgdma_descriptor *TxBufDescPtrStart;
    alt_sgdma_descriptor *TxBufDescPtrCur;
    alt_sgdma_descriptor *TxBufDescCompPtr;                     /* See Note #3.                                         */
    alt_sgdma_descriptor *TxBufDescPtrEnd;
    CPU_INT16U            RxNRdyCtr;
    alt_sgdma_dev        *RxSGDMA;
    alt_sgdma_dev        *TxSGDMA;
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U            MulticastAddrHashBitCtr[64];
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
*
*               An example device register structure is provided below.
*********************************************************************************************************
*/


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
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Device driver functions may be arbitrarily named.  However, it is recommended that device
*               driver functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_&&&.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*********************************************************************************************************
*/

                                                                        /* -------- FNCT'S COMMON TO ALL DEV'S -------- */
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

static  void  NetDev_SGDMA_ISR_Handler  (void               *context);

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


                                                                        /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
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
                                                                                  /* Ether DMA dev API fnct ptrs :*/
const  NET_DEV_API_ETHER  NetDev_API_ALTERA_TSE = { NetDev_Init,                  /*   Init/add                   */
                                                    NetDev_Start,                 /*   Start                      */
                                                    NetDev_Stop,                  /*   Stop                       */
                                                    NetDev_Rx,                    /*   Rx                         */
                                                    NetDev_Tx,                    /*   Tx                         */
                                                    NetDev_AddrMulticastAdd,      /*   Multicast addr add         */
                                                    NetDev_AddrMulticastRemove,   /*   Multicast addr remove      */
                                                    NetDev_ISR_Handler,           /*   ISR handler                */
                                                    NetDev_IO_Ctrl,               /*   I/O ctrl                   */
                                                    NetDev_MII_Rd,                /*   Phy reg rd                 */
                                                    NetDev_MII_Wr                 /*   Phy reg wr                 */
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
*                   then the driver MUST validate the pif->Phy_Cfg / pif->Ext_Cfg pointer
*                   by checking for a NULL pointer BEFORE attempting to access members of the
*                   Phy configuration structure.  Phy configuration fields of interest
*                   generally include, but are  not limited to :
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
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbytes;
    CPU_INT32U          reg_data;
    LIB_ERR             lib_err;
    NET_ERR             err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* See Note #7.                                         */
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets % RX_BUF_ALIGN_OCTETS) != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 2u) {                        /* Altera TSE stuffs Rx data with two bytes and must not*/
       *perr = NET_DEV_ERR_INVALID_CFG;                         /* be a part of the Rx data.                            */
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
    if (pdev_cfg->TxBufIxOffset != 2u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate phy bus mode.                               */
    if (pphy_cfg->BusMode != NET_PHY_BUS_MODE_RMII) {
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
                                                                /* Configure Ethernet Controller GPIO (see Note #3).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
                                                                /* Open altera API device register pointer.             */
    pdev_data->RxSGDMA = alt_avalon_sgdma_open(BSP_SGDMA_RX_NAME);
    pdev_data->TxSGDMA = alt_avalon_sgdma_open(BSP_SGDMA_TX_NAME);

                                                                /* Do a software reset of the Rx and Tx SGDMA by writing*/
                                                                /* to the reset bit twice.                              */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, SW_RESET_SGDMA);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, SW_RESET_SGDMA);

    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, SW_RESET_SGDMA);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, SW_RESET_SGDMA);

                                                                /* Disable all of the Rx and Tx SGDMA interrupts.       */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, 0x0);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, 0x0);

                                                                /* Clear all of the Rx and Tx pending interrupts.       */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, CLEAR_INTERRUPT);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, CLEAR_INTERRUPT);

                                                                /* Configure ext int ctrl'r (see Note #4).              */
    alt_avalon_sgdma_register_callback(pdev_data->RxSGDMA, NetDev_SGDMA_ISR_Handler, SGDMA_INT_MASK, (void *)pif);
    alt_avalon_sgdma_register_callback(pdev_data->TxSGDMA, NetDev_SGDMA_ISR_Handler, SGDMA_INT_MASK, (void *)pif);

    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* -------- ALLOCATE MEMORY FOR DMA DESCRIPTORS ------- */
    nbytes = pdev_cfg->RxDescNbr * sizeof(alt_sgdma_descriptor);/* Determine block size.                                */
    Mem_PoolCreate ((MEM_POOL   *)&pdev_data->RxDescPool,       /* Pass a pointer to the mem pool to create.            */
                    (void       *) pdev_cfg->MemAddr,           /* From the uC/LIB Mem generic pool.                    */
                    (CPU_SIZE_T  ) pdev_cfg->MemSize,           /* Generic pool is of unknown size.                     */
                    (CPU_SIZE_T  ) 1,                           /* Allocate 1 block.                                    */
                    (CPU_SIZE_T  ) nbytes,                      /* Block size large enough to hold all Rx descriptors.  */
                    (CPU_SIZE_T  ) 4,                           /* Block alignment (see Note #8).                       */
                    (CPU_SIZE_T *)&reqd_octets,                 /* Optional, ptr to variable to store rem nbr bytes.    */
                    (LIB_ERR    *)&lib_err);                    /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


    nbytes = pdev_cfg->TxDescNbr * sizeof(alt_sgdma_descriptor);/* Determine block size.                                */
    Mem_PoolCreate ((MEM_POOL   *)&pdev_data->TxDescPool,       /* Pass a pointer to the mem pool to create.            */
                    (void       *) pdev_cfg->MemAddr,           /* From the uC/LIB Mem generic pool.                    */
                    (CPU_SIZE_T  ) pdev_cfg->MemSize,           /* Generic pool is of unknown size.                     */
                    (CPU_SIZE_T  ) 1,                           /* Allocate 1 block.                                    */
                    (CPU_SIZE_T  ) nbytes,                      /* Block size large enough to hold all Tx descriptors.  */
                    (CPU_SIZE_T  ) 4,                           /* Block alignment (see Note #8).                       */
                    (CPU_SIZE_T *)&reqd_octets,                 /* Optional, ptr to variable to store rem nbr bytes.    */
                    (LIB_ERR    *)&lib_err);                    /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */
                                                                /* Perform a software reset of the TSE MAC              */
    reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
    reg_data |= SW_RESET_TSE;
    IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);

    while ((IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr) & SW_RESET_TSE) != 0) {
        KAL_Dly(1);
    }

                                                                /* ------ OPTIONALLY OBTAIN REFERENCE TO PHY CFG ------ */
                                                                /* Obtain ptr to the phy cfg struct.                    */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    if (pphy_cfg != (void *)0) {                                /* Phy bus configuration is done in the hardware design */
        ;                                                       /* level, not the software level                        */
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
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    CPU_INT32U          reg_data;
    np_tse_mac         *pmac;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pmac      = (np_tse_mac        *)(pdev_cfg->BaseAddr);
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
            hw_addr[0]  = (IORD_ALTERA_TSEMAC_MAC_0(pdev_cfg->BaseAddr) >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1]  = (IORD_ALTERA_TSEMAC_MAC_0(pdev_cfg->BaseAddr) >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2]  = (IORD_ALTERA_TSEMAC_MAC_0(pdev_cfg->BaseAddr) >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3]  = (IORD_ALTERA_TSEMAC_MAC_0(pdev_cfg->BaseAddr) >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4]  = (IORD_ALTERA_TSEMAC_MAC_1(pdev_cfg->BaseAddr) >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5]  = (IORD_ALTERA_TSEMAC_MAC_1(pdev_cfg->BaseAddr) >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
                                                                /* Select source MAC address selection on transmit to   */
                                                                /* ... MAC_0 & MAC_1.                                   */
        reg_data = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
        DEF_BIT_CLR(reg_data, (TX_ADDR_SEL0 |
                               TX_ADDR_SEL1 |
                               TX_ADDR_SEL2));
        IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);

                                                                /* Set device MAC address registers.                    */
        reg_data = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                    ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                    ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                    ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));

        IOWR_ALTERA_TSEMAC_MAC_0(pdev_cfg->BaseAddr, reg_data);

        reg_data = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                    ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));

        IOWR_ALTERA_TSEMAC_MAC_1(pdev_cfg->BaseAddr, reg_data);
    }

                                                                /* Configure Rx and Tx threshold values                 */
    IOWR_ALTERA_TSEMAC_FRM_LENGTH(pdev_cfg->BaseAddr, ALTERA_TSE_MAC_MAX_FRAME_LENGTH);
    IOWR_ALTERA_TSEMAC_RX_ALMOST_EMPTY(pdev_cfg->BaseAddr, 8);
    IOWR_ALTERA_TSEMAC_RX_ALMOST_FULL(pdev_cfg->BaseAddr, 8);
    IOWR_ALTERA_TSEMAC_TX_ALMOST_EMPTY(pdev_cfg->BaseAddr, 8);
    IOWR_ALTERA_TSEMAC_TX_ALMOST_FULL(pdev_cfg->BaseAddr,  3);
    IOWR_ALTERA_TSEMAC_TX_SECTION_EMPTY(pdev_cfg->BaseAddr, BSP_TSE_MAX_TRANSMIT_FIFO_DEPTH - 16);
    IOWR_ALTERA_TSEMAC_TX_SECTION_FULL(pdev_cfg->BaseAddr,  0);
    IOWR_ALTERA_TSEMAC_RX_SECTION_EMPTY(pdev_cfg->BaseAddr, BSP_TSE_MAX_RECEIVE_FIFO_DEPTH - 16);
    IOWR_ALTERA_TSEMAC_RX_SECTION_FULL(pdev_cfg->BaseAddr,  0);
                                                                /* --------------- INIT DMA DESCRIPTORS --------------- */
    NetDev_RxDescInit(pif, perr);                               /* Initialize Rx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev_data->RxNRdyCtr = 0;                                   /* No pending frames to process (see Note #3).          */


    NetDev_TxDescInit(pif, perr);                               /* Initialize Tx descriptors.                           */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------------ ENABLE RX & TX ------------------ */
                                                                /* Only call rgmii function is using Marvell Phy        */
    marvell_cfg_rgmii(pmac);
                                                                /* Enable Transmit and Receive in the TSE MAC.          */
    reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
    reg_data |=(TX_ENA | RX_ENA | TX_ADDR_INS | RX_ERR_DISC);
    IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);

                                                                /* ----------------- START DMA FETCH ------------------ */

                                                                /* Clear Run in Rx SGDMA.                               */
    reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
    reg_data &= ~RUN;
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(SGDMA_RX_BASEADDR, reg_data);

                                                                /* Clear any (previous) status register information that*/
                                                                /* might occlude our error checking later.              */
    IOWR_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_RX_BASEADDR, 0xFF);

                                                                /* Point the controller at the first descriptor.        */
    IOWR_ALTERA_AVALON_SGDMA_NEXT_DESC_POINTER(BSP_SGDMA_RX_BASEADDR, (alt_u32)pdev_data->RxBufDescPtrStart);

                                                                /* Enable the Rx SGDMA and its interrupts.              */
    reg_data  =  IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
    reg_data |= (RUN | STOP_DMA_ER | IE_DESCRIPTOR_COMPLETED | IE_GLOBAL);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, reg_data);

                                                                /* Clear all of the Rx and Tx pending interrupts.       */
    reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
    reg_data |= CLEAR_INTERRUPT;
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, reg_data);

    reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR);
    reg_data |= CLEAR_INTERRUPT;
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, reg_data);

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
    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    alt_sgdma_descriptor *pdesc;
    CPU_INT16U            bytes_txd;
    CPU_INT16U            bytes_to_tx;
    CPU_INT08U            i;
    CPU_INT32U            reg_data;
    NET_ERR               err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;          /* Obtain ptr to dev data area.                         */

                                                                /* ----------------- DISABLE RX & TX ------------------ */
                                                                /* Disable transmitter & receiver.                      */
    reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
    reg_data &= ~(TX_ENA | RX_ENA);
    IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
                                                                /* Disable all of the Rx and Tx SGDMA interrupts and    */
                                                                /* stop the sgdma core                                  */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, DEF_BIT_NONE);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, DEF_BIT_NONE);

                                                                /* Clear all of the Rx and Tx pending interrupts        */
    reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
    reg_data |= CLEAR_INTERRUPT;
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, reg_data);

    reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR);
    reg_data |= CLEAR_INTERRUPT;
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, reg_data);

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    pdesc = &pdev_data->TxBufDescPtrStart[0];
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {

       bytes_txd   = pdesc->actual_bytes_transferred;
       bytes_to_tx = pdesc->bytes_to_transfer;
                                                                /* If NOT yet  tx'd, ...                                */
       if (bytes_txd < bytes_to_tx) {
                                                                /* ... dealloc tx buf (see Note #2a1).                  */
            NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->read_addr, &err);
           (void)err;                                           /* Ignore possible dealloc err (see Note #2b2).         */
        }
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
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    alt_sgdma_descriptor *pdesc;
    CPU_INT08U           *pbuf_new;
    CPU_INT16S            length;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg     = (NET_DEV_CFG_ETHER    *)pif->Dev_Cfg;        /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA         *)pif->Dev_Data;       /* Obtain ptr to dev data area.                         */
                                                                /* Obtain ptr to next ready descriptor.                 */
    pdesc        = (alt_sgdma_descriptor *)pdev_data->RxBufDescPtrCur;

                                                                /* --------------- DECREMENT FRAME CNT ---------------- */
    CPU_CRITICAL_ENTER();                                       /* Disable interrupts to alter shared data.             */
    if (pdev_data->RxNRdyCtr > 0) {                             /* One less frame to process.                           */
        pdev_data->RxNRdyCtr--;
    }
    CPU_CRITICAL_EXIT();
                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
                                                                /* See Note #3a.                                        */
    if (DEF_BIT_IS_SET(pdesc->control, OWNED_BY_HW)) {
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
        *perr   = (NET_ERR     )NET_DEV_ERR_RX;
         return;
    }

    if ((pdesc->status) > 0) {                                  /* See Note #3b.                                        */
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;
        *perr   = (NET_ERR     )NET_DEV_ERR_RX;
         return;
    }

                                                                /* --------------- OBTAIN FRAME LENGTH ---------------- */
                                                                /* Altera TSE stuffs Rx data with two bytes and must not*/
                                                                /* have those bytes included in the length.             */
    length  = (pdesc->actual_bytes_transferred) - (pdev_cfg->RxBufIxOffset);

    if (length < NET_IF_ETHER_FRAME_MIN_SIZE) {                 /* If frame is a runt.                                  */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame (see Note #3b.                */
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

   *size     =  length;                                         /* Return the size of the received frame.               */
                                                                /* Return a pointer to the newly received data area.    */
   *p_data             = (CPU_INT08U *)alt_remap_cached(pdesc->write_addr, 4);
    pdesc->write_addr  = (alt_u32    *)pbuf_new;                /* Update the descriptor to point to a new data area    */
    pdesc->control     = (pdesc->control | OWNED_BY_HW);        /* Re-enable the descriptor for DMA transfer.           */

    NetDev_RxDescPtrCurInc(pif);                                /* Free the current descriptor.                         */

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
*********************************************************************************************************
*/

void  NetDev_Tx (NET_IF      *pif,
                 CPU_INT08U  *p_data,
                 CPU_INT16U   size,
                 NET_ERR     *perr)
{

    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    alt_sgdma_descriptor *pdesc;
    CPU_INT32U           *p_data_actual;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER    *)pif->Dev_Cfg;        /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA         *)pif->Dev_Data;       /* Obtain ptr to dev data area.                         */


                                                                /* Wait for any pending transfers to complete.          */
    while ((IORD_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_TX_BASEADDR) & BUSY));

                                                                /* Clear Run.                                           */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR,
                                    (IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR) & ~RUN));

                                                                /* Obtain ptr to next available Tx descriptor.          */
    pdesc        = (alt_sgdma_descriptor *)pdev_data->TxBufDescPtrCur;

                                                                /* Clear bit-31 before passing it to SGDMA Driver.      */
    p_data_actual = (CPU_INT32U *)alt_remap_cached ((volatile void*)p_data - 2, 4);

    alt_avalon_sgdma_construct_mem_to_stream_desc((alt_sgdma_descriptor *)pdesc,
                                                  (alt_sgdma_descriptor *)pdesc->next,
                                                  (alt_u32              *)p_data_actual,
                                                  (alt_u16               )size + 2,
                                                                          0,
                                                                          1,
                                                                          1,
                                                                          0);

                                                                /* Clear any (previous) status register information     */
                                                                /* that might occlude our error checking later.         */
    IOWR_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_TX_BASEADDR, 0xFF);

                                                                /* Point the controller at the descriptor.              */
    IOWR_ALTERA_AVALON_SGDMA_NEXT_DESC_POINTER(BSP_SGDMA_TX_BASEADDR, (alt_u32)pdesc);

                                                                /* Set up SGDMA controller to:                          */
                                                                /* - Run once a valid descriptor is written to          */
                                                                /*   controller.                                        */
                                                                /* - Enable interrupt generation.                       */
                                                                /* - Stop on an error with any particular descriptor.   */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR,
      (RUN | IE_DESCRIPTOR_COMPLETED | IE_GLOBAL | STOP_DMA_ER | IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR)));

                                                                /* Wait for the descriptor to complete.                 */
    while ((IORD_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_TX_BASEADDR) & BUSY));

                                                                /* Clear Run.                                           */
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR,
                                    (IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR) & ~RUN));

    if (pdev_data->TxBufDescPtrCur != pdev_data->TxBufDescPtrEnd) {
        pdev_data->TxBufDescPtrCur++;
    } else {
        pdev_data->TxBufDescPtrCur  = pdev_data->TxBufDescPtrStart;
    }

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
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : (1) This driver does NOT support the TSE's '10/100Mb Small MAC' variant. As such, it expects
*                   the presence of the hardware hash table in order to successfully translate the multicast
*                   address to its hardware (MAC) address counterpart. We do NOT implement a software hash
*                   table.
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
    CPU_INT32U          xor_bit;
    CPU_INT32U          hash_code;
    CPU_INT08U          mac_octet;
    CPU_INT32U          bit_shift;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT08U          cnt;
                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;          /* Obtain ptr to dev data area.                         */

    hash_code    = 0x00;

    for (cnt = 6; cnt > 0; cnt--) {
        xor_bit   = 0;
        mac_octet = paddr_hw[cnt - 1];

        for (bit_shift = 0; bit_shift < 8; bit_shift++) {
            xor_bit ^= (CPU_INT32U)((mac_octet >> bit_shift) & 0x01);
        }

        hash_code = (hash_code << 1) | xor_bit;
    }

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash_code];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    IOWR_ALTERA_TSEMAC_HASH_TABLE(pdev_cfg->BaseAddr, hash_code, 1);
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
    CPU_INT32U          xor_bit;
    CPU_INT32U          hash_code;
    CPU_INT32U          mac_octet;
    CPU_INT32U          bit_shift;
    CPU_INT08U         *paddr_hash_ctrs;
                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;          /* Obtain ptr to dev data area.                         */

    hash_code    = 0x00;

    for (mac_octet = 5; mac_octet > 0; mac_octet--) {
        xor_bit   = 0;
        mac_octet = paddr_hw[mac_octet];

        for (bit_shift = 0; bit_shift < 8; bit_shift++) {
            xor_bit ^= (CPU_INT32U)((mac_octet >> bit_shift) & 0x01);
        }

        hash_code = (hash_code << 1) | xor_bit;
    }

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash_code];
    if (*paddr_hash_ctrs > 1) {                                 /* If hash bit reference ctr not zero.                  */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0;                                        /* Zero hash bit references remaining.                  */
    IOWR_ALTERA_TSEMAC_HASH_TABLE(pdev_cfg->BaseAddr, hash_code, 0);
#endif

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_SGDMA_ISR_Handler()
*
* Description : This function serves as a general entry point for SG-DMA related interrupts. It calls
*               the device specific ISR like the BSP-level handler would.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Altera's HAL interrupt funnel.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_SGDMA_ISR_Handler (void *context)
{
    NET_IF  *pif;


    pif = (NET_IF *)context;

    NetDev_ISR_Handler(pif, NET_DEV_ISR_TYPE_UNKNOWN);
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
    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    alt_sgdma_descriptor *pdesc;
    CPU_DATA              reg_valRx;
    CPU_DATA              reg_valTx;
    CPU_INT16U            n_rdy;
    CPU_INT16U            n_new;
    CPU_INT16U            i;
    CPU_INT08U           *p_data;
    CPU_INT32U            reg_data;
    NET_ERR               err;


   (void)type;                                                  /* Prevent 'variable unused' compiler warning.          */

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;          /* Obtain ptr to dev data area.                         */

                                                                /* --------------- DETERMINE ISR TYPE ----------------- */
    reg_valRx    = IORD_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_RX_BASEADDR);
    reg_valTx    = IORD_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_TX_BASEADDR);

                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if (reg_valRx & DESCRIPTOR_COMPLETED) {
                                                                /* Clear the Rx interrupts                              */
        IOWR_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_RX_BASEADDR, DESCRIPTOR_COMPLETED | CHAIN_COMPLETED);

        reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
        reg_data |= CLEAR_INTERRUPT;
        IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, reg_data);

        pdesc = pdev_data->RxBufDescPtrStart;

                                                                /* Clear EOP status bit for receive descriptors         */
        DEF_BIT_CLR(pdev_data->RxBufDescPtrCur->status, DESC_STATUS_ERROR7);

        n_rdy = 0;

        for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
            if (DEF_BIT_IS_CLR(pdesc->control, OWNED_BY_HW)) {
                n_rdy++;
            }

            pdesc++;
        }

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

                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if (reg_valTx & DESCRIPTOR_COMPLETED) {
                                                                /* Clear & Disable the Tx interrupts.                   */
         IOWR_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_TX_BASEADDR, DESCRIPTOR_COMPLETED | CHAIN_COMPLETED);

         reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR);
         reg_data |= CLEAR_INTERRUPT;
         reg_data &= ~IE_DESCRIPTOR_COMPLETED;
         IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_TX_BASEADDR, reg_data);

         pdesc  = pdev_data->TxBufDescCompPtr;
         p_data = (CPU_INT08U *)pdesc->read_addr;
         p_data += 2;

         NetIF_TxDeallocTaskPost(p_data, &err);

         NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that Tx resources are available.       */

         if (pdev_data->TxBufDescCompPtr != pdev_data->TxBufDescPtrEnd) {
             pdev_data->TxBufDescCompPtr++;                     /* Point to next Buffer Descriptor.                     */
         } else {                                               /* Wrap around end of descriptor list if necessary.     */
             pdev_data->TxBufDescCompPtr  = pdev_data->TxBufDescPtrStart;
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
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;
    CPU_INT32U           reg_data;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */

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

             if (IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr) & HD_ENA) {
                 duplex = NET_PHY_DUPLEX_HALF;
             } else {
                 duplex = NET_PHY_DUPLEX_FULL;
             }

             if (plink_state->Duplex != duplex) {
                 switch (plink_state->Duplex) {                 /* !!!! Update duplex register setting on device.       */
                    case NET_PHY_DUPLEX_FULL:
                                                                /* Update link duplex to full.                          */
                         reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
                         reg_data &= ~HD_ENA;
                         IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);
                         break;

                    case NET_PHY_DUPLEX_HALF:
                                                                /* Update link duplex to half.                          */
                         reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
                         reg_data |= HD_ENA;
                         IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);
                         break;

                    default:
                         break;
                 }
             }

             switch (IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr) & (ETH_SPEED | ENA_10)) {
                 case ENA_10:
                     spd = NET_PHY_SPD_10;
                     break;

                 case DEF_BIT_NONE:
                     spd = NET_PHY_SPD_100;
                     break;

                 case ETH_SPEED:
                     spd = NET_PHY_SPD_1000;
                     break;

                 default:
                     spd = NET_PHY_SPD_0;
                     break;
             }

             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {                    /* !!!! Update speed register setting on device.        */
                    case NET_PHY_SPD_10:
                                                                /* Update MAC link spd to 10 Mbps.                      */
                         reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
                         reg_data &= ~ETH_SPEED;
                         reg_data |=  ENA_10;
                         IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);
                         break;

                    case NET_PHY_SPD_100:
                                                                /* Update MAC link spd to 100 Mbps.                     */
                         reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
                         reg_data &= ~ETH_SPEED;
                         reg_data &= ~ENA_10;
                         IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);
                         break;

                    case NET_PHY_SPD_1000:
                                                                /* Update MAC link spd to 1000 Mbps.                    */
                         reg_data  = IORD_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr);
                         reg_data |=  ETH_SPEED;
                         reg_data &= ~ENA_10;
                         IOWR_ALTERA_TSEMAC_CMD_CONFIG(pdev_cfg->BaseAddr, reg_data);
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
* Description : Read data over the (R)MII bus to the specified Phy register.
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
    CPU_INT32U          timeout;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

    if ((phy_addr > 31) || (reg_addr > 31)) {
       *perr = NET_PHY_ERR_INVALID_ADDR;
        return;
    }

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */

    IOWR_ALTERA_TSEMAC_MDIO_ADDR1(pdev_cfg->BaseAddr, phy_addr);
   *p_data = IORD_ALTERA_TSEMAC_MDIO(pdev_cfg->BaseAddr, 1, reg_addr);

                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
   (void)timeout;

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
    CPU_INT32U          timeout;

    if ((phy_addr > 31) || (reg_addr > 31)) {
       *perr = NET_PHY_ERR_INVALID_ADDR;
        return;
    }
                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */

    IOWR_ALTERA_TSEMAC_MDIO_ADDR1(pdev_cfg->BaseAddr, phy_addr);
    IOWR_ALTERA_TSEMAC_MDIO(pdev_cfg->BaseAddr, 1, reg_addr, data);

                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
   (void)timeout;

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
    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    MEM_POOL             *pmem_pool;
    alt_sgdma_descriptor *pdesc;
    alt_u32              *uncached_buf;
    LIB_ERR               lib_err;
    CPU_SIZE_T            nbytes;
    CPU_INT16U            i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;          /* Obtain ptr to dev data area.                         */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool = &pdev_data->RxDescPool;
    nbytes    =  pdev_cfg->RxDescNbr * sizeof(alt_sgdma_descriptor);
    pdesc     = (alt_sgdma_descriptor *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                                       (CPU_SIZE_T) nbytes,
                                                       (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = lib_err;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxBufDescPtrStart = (alt_sgdma_descriptor *)pdesc;
    pdev_data->RxBufDescPtrCur   = (alt_sgdma_descriptor *)pdesc;
    pdev_data->RxBufDescPtrEnd   = (alt_sgdma_descriptor *)pdesc + (pdev_cfg->RxDescNbr - 1);

                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->next = (alt_u32 *)((alt_sgdma_descriptor *)pdesc + 1);

        uncached_buf              = (alt_u32 *)NetBuf_GetDataPtr((NET_IF *)pif,
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

                                                                /* Ensure bit-31 of buffer is clear before passing to   */
                                                                /* SGDMA Driver.                                        */
        uncached_buf = (alt_u32 *)alt_remap_cached ((volatile void*)uncached_buf, 4);

        alt_avalon_sgdma_construct_stream_to_mem_desc(pdesc,    /* Descriptor to initialize.                            */
                                                                /* Next descriptor pointer.                             */
                                                     (pdesc + 1),
                                                                /* Descriptor's Rx buffer.                              */
                                                      uncached_buf,
                                                      0,        /* Read until EOP.                                      */
                                                      0);       /* Don't write to constant address.                     */

        if (pdesc == (pdev_data->RxBufDescPtrEnd)) {            /* Set next field for last descriptor to the first      */
                                                                /* descriptor to wrap the descriptor chain.             */
            pdesc->next = (alt_u32 *)pdev_data->RxBufDescPtrStart;
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start address.    */
     IOWR_ALTERA_AVALON_SGDMA_NEXT_DESC_POINTER(BSP_SGDMA_RX_BASEADDR, (CPU_INT32U)pdev_data->RxBufDescPtrStart);

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

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    MEM_POOL             *pmem_pool;
    alt_sgdma_descriptor *pdesc;
    LIB_ERR               lib_err;
    CPU_INT16U            i;
    CPU_INT08U           *pdesc_data;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pmem_pool = &pdev_data->RxDescPool;
    pdesc     =  pdev_data->RxBufDescPtrStart;

    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                             */
        pdesc_data = (CPU_INT08U *)(pdesc->write_addr);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree(pmem_pool,                                  /* Return Rx descriptor block to Rx descriptor pool.    */
                    pdev_data->RxBufDescPtrStart,
                   &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = lib_err;
        return;
    }

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
    NET_DEV_DATA         *pdev_data;
    CPU_INT32U            reg_data;
                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data    = (NET_DEV_DATA  *)pif->Dev_Data;              /* Obtain ptr to dev data area.                         */

    if ((alt_u32 *)pdev_data->RxBufDescPtrCur == (alt_u32 *)pdev_data->RxBufDescPtrEnd) {
        pdev_data->RxBufDescPtrCur = pdev_data->RxBufDescPtrStart;
    } else {
        pdev_data->RxBufDescPtrCur++;                           /* Point to next Buffer Descriptor.                     */
    }

    reg_data  = IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
    reg_data &= ~RUN;
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(SGDMA_RX_BASEADDR, reg_data);

                                                                /* Clear any (previous) status register information that*/
                                                                /* might occlude our error checking later.              */
    IOWR_ALTERA_AVALON_SGDMA_STATUS(BSP_SGDMA_RX_BASEADDR, 0xFF);

                                                                /* Point the controller at the first descriptor.        */
    IOWR_ALTERA_AVALON_SGDMA_NEXT_DESC_POINTER(BSP_SGDMA_RX_BASEADDR, (alt_u32)pdev_data->RxBufDescPtrCur);

                                                                /* Enable the Rx SGDMA and its interrupts.              */
    reg_data  =  IORD_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR);
    reg_data |= (RUN | STOP_DMA_ER | IE_DESCRIPTOR_COMPLETED | IE_GLOBAL);
    IOWR_ALTERA_AVALON_SGDMA_CONTROL(BSP_SGDMA_RX_BASEADDR, reg_data);
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
    NET_DEV_CFG_ETHER    *pdev_cfg;
    NET_DEV_DATA         *pdev_data;
    alt_sgdma_descriptor *pdesc;
    MEM_POOL             *pmem_pool;
    CPU_SIZE_T            nbytes;
    CPU_INT16U            i;
    LIB_ERR               lib_err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg     = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;           /* Obtain ptr to the dev cfg struct.                    */
    pdev_data    = (NET_DEV_DATA      *)pif->Dev_Data;          /* Obtain ptr to dev data area.                         */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool = &pdev_data->TxDescPool;
    nbytes    =  pdev_cfg->TxDescNbr * sizeof(alt_sgdma_descriptor);
    pdesc     = (alt_sgdma_descriptor *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                                       (CPU_SIZE_T) nbytes,
                                                       (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = lib_err;
        return;
    }

                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->TxBufDescPtrStart = (alt_sgdma_descriptor *)pdesc;
    pdev_data->TxBufDescPtrCur   = (alt_sgdma_descriptor *)pdesc;
    pdev_data->TxBufDescCompPtr  = (alt_sgdma_descriptor *)pdesc;
    pdev_data->TxBufDescPtrEnd   = (alt_sgdma_descriptor *)pdesc + (pdev_cfg->TxDescNbr - 1);


                                                                /* --------------- INIT TX DESCRIPTORS ---------------- */
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Initialize Tx descriptor ring                        */
        pdesc->read_addr                = 0x0;
        pdesc->read_addr_pad            = 0x0;
        pdesc->write_addr               = 0x0;
        pdesc->write_addr_pad           = 0x0;
        pdesc->next_pad                 = 0x0;
        pdesc->bytes_to_transfer        = 0x0;
        pdesc->read_burst               = 0x4;
        pdesc->write_burst              = 0x4;
        pdesc->actual_bytes_transferred = 0x0;
        pdesc->status                   = 0x0;
        pdesc->control                  = 0x0;

        if (pdesc == (pdev_data->TxBufDescPtrEnd)) {            /* Set next descriptor to the first in the list to wrap.*/
            pdesc->next = (alt_u32 *)pdev_data->TxBufDescPtrStart;
        } else {
            pdesc->next = (alt_u32 *)(pdesc + 1);
        }

        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Tx desc start address.    */
    IOWR_ALTERA_AVALON_SGDMA_NEXT_DESC_POINTER(BSP_SGDMA_TX_BASEADDR, (CPU_INT32U)pdev_data->TxBufDescPtrStart);

    *perr = NET_DEV_ERR_NONE;
}

#endif /* NET_IF_ETHER_MODULE_EN */
