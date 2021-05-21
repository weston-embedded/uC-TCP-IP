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
*                                         PIC32 ETHERNET DMA
*
* Filename : net_dev_pic32.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_ETHER_PIC32
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_pic32.h"

#include  <plib.h>

#include  <bsp_peripherals.h>
#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO                            10000      /* MII read write timeout.                              */


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     DESCRIPTOR BIT DEFINITIONS
*********************************************************************************************************
*/


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
    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;
    NET_ERR      net_err;
    CPU_INT32U   rxPktCnt;
    eEthEvents   ethEnabledEvents;
    eEthEvents   ethMaskedEvents;

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

static  void  NetDev_TxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *perr);


static  void  NetDev_TxDescInit         (NET_IF             *pif,
                                         NET_ERR            *perr);


static void*  NetDev_AllocDcpt(CPU_SIZE_T nitems, CPU_SIZE_T size, void* param );

static void   NetDev_RxDeAllocDcpt      (void               *pDcpt,
                                         void               *param );

static void   NetDev_TxDeAllocDcpt      (void               *pDcpt,
                                         void               *param );

static void   NetDev_TxDeallocPost      (CPU_INT08U         *pTxBuff );

static  void  NetDev_TxAcknowledge      (void               *pTxBuff,
                                         int                buffIx );

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
                                                                                /* Ether DMA dev API fnct ptrs :        */
const  NET_DEV_API_ETHER  NetDev_API_EtherPIC32 = { NetDev_Init,                /*   Init/add                           */
                                                    NetDev_Start,               /*   Start                              */
                                                    NetDev_Stop,                /*   Stop                               */
                                                    NetDev_Rx,                  /*   Rx                                 */
                                                    NetDev_Tx,                  /*   Tx                                 */
                                                    NetDev_AddrMulticastAdd,    /*   Multicast addr add                 */
                                                    NetDev_AddrMulticastRemove, /*   Multicast addr remove              */
                                                    NetDev_ISR_Handler,         /*   ISR handler                        */
                                                    NetDev_IO_Ctrl,             /*   I/O ctrl                           */
                                                    NetDev_MII_Rd,              /*   Phy reg rd                         */
                                                    NetDev_MII_Wr               /*   Phy reg wr                         */
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
    NET_BUF_SIZE        buf_size_max;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbytes;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Phy_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* See Note #7.                                         */
                                                                /* Validate Rx buf size.                                */
    if ((pdev_cfg->RxBufLargeSize   % 16)  != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }
                                                                /* Validate Rx buf alignment.                           */
                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf size/ix.                             */
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

    if (pdev_cfg->RxBufPoolType != NET_IF_MEM_TYPE_MAIN || pdev_cfg->TxBufPoolType != NET_IF_MEM_TYPE_MAIN) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate phy type .                               */
    if (pphy_cfg->Type != NET_PHY_TYPE_EXT) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }

                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4u,
                                  (CPU_SIZE_T *)0,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;
    pdev_data->rxPktCnt=0;
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

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ------ */
                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbytes = EthDescriptorsGetSize(ETH_DCPT_TYPE_RX);           /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0,                            /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) (pdev_cfg->RxDescNbr+1),      /* Number of blocks.                                    */
                   (CPU_SIZE_T  ) nbytes,                       /* Block size large enough to hold a Rx descriptor.     */
                   (CPU_SIZE_T  ) 4u,                           /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


    nbytes = EthDescriptorsGetSize(ETH_DCPT_TYPE_TX);           /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) 0,                            /* From the uC/LIB Mem generic pool.                    */
                   (CPU_SIZE_T  ) 0u,                           /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) (pdev_cfg->TxDescNbr+1),      /* Number of blocks.                                    */
                   (CPU_SIZE_T  ) nbytes,                       /* Block size large enough to hold all Tx descriptors.  */
                   (CPU_SIZE_T  ) 4u,                           /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */
    EthInit();
                                                                /* TODO: when is the PHY initialization ...             */
                                                                /* ... pif->Phy_API->NetPhy_Init() called ?             */
    EthRxFiltersClr(ETH_FILT_ALL_FILTERS);
    EthRxFiltersSet(ETH_FILT_CRC_ERR_REJECT|ETH_FILT_RUNT_REJECT|ETH_FILT_ME_UCAST_ACCEPT|ETH_FILT_BCAST_ACCEPT);
                                                                /* TODO: how are the filters updated/changed?           */
                                                                /* they are not part of the NET_DEV_CFG_ETHER ???       */

    EthRxSetBufferSize(pdev_cfg->RxBufLargeSize);               /* set the RX buffer size                               */



                                                                /* ------ OPTIONALLY OBTAIN REFERENCE TO PHY CFG ------ */
    if ((pphy_cfg->Duplex != NET_PHY_DUPLEX_AUTO) &&
        (pphy_cfg->Spd    != NET_PHY_SPD_AUTO)) {
                                                                /* Cfg MAC w/ initial Phy settings.                     */
        eEthMacPauseType  pauseType;
        eEthOpenFlags     oFlags;

        if(pphy_cfg->Duplex == NET_PHY_DUPLEX_FULL)
        {
            pauseType = ETH_MAC_PAUSE_CPBL_MASK;
            oFlags = ETH_OPEN_FDUPLEX;

        }
        else
        {
            pauseType = ETH_MAC_PAUSE_TYPE_NONE;
            oFlags = ETH_OPEN_HDUPLEX;
        }

        if(pphy_cfg->Spd == NET_PHY_SPD_100)
        {
            oFlags |= ETH_OPEN_100;
        }
        else
        {
            oFlags |= ETH_OPEN_10;
        }

        if(pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII)
        {
            oFlags |= ETH_OPEN_RMII;
        }
        else
        {
            oFlags |= ETH_OPEN_MII;
        }

        EthMACOpen(oFlags, pauseType);                          /* no need of negotiation results; just update the MAC  */
    }
                                                                /* else the MAC should be updated with the ...          */
                                                                /* ...auto-negotiation results in NetDev_IO_Ctrl        */

    INTEnable(INT_ETHERNET, INT_DISABLED);                      /* disable any Eth ints                                 */
    INTClearFlag(INT_ETHERNET);
    INTSetVectorPriority(INT_ETH_VECTOR, INT_ETH_VECTOR_PRIORITY);
    INTSetVectorSubPriority(INT_ETH_VECTOR, INT_ETH_VECTOR_SUBPRIORITY);
                                            /* TODO: how is the interrupt priority/sub-priority passed to this level ??? */
                                            /* they are not part of the NET_DEV_CFG_ETHER ???                            */
                                            /* had to include bsp_peripherals.h !!!                                      */

    pdev_data->ethEnabledEvents =  ( ETH_EV_TXDONE|ETH_EV_RXDONE|                                                       /* TX and RX events                       */
                                     ETH_EV_RXOVFLOW|ETH_EV_RXBUFNA|ETH_EV_TXABORT|ETH_EV_RXBUSERR|ETH_EV_TXBUSERR );   /* error events                           */
    pdev_data->ethMaskedEvents =  ( ETH_EV_RXOVFLOW|ETH_EV_RXBUFNA|ETH_EV_TXABORT|ETH_EV_RXBUSERR|ETH_EV_TXBUSERR );    /* events that are masked once they occur */
                                                                                                                        /* they should be acknowledged in order   */
                                                                                                                        /* to be turned on again                  */
                                            /* TODO: how are the events updated/changed?      */
                                            /* they are not part of the NET_DEV_CFG_ETHER ??? */


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
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                          /* See Note #3.                                         */
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
            EthMACGetAddress(hw_addr);

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
        EthMACSetAddress(hw_addr);
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

                                                                /* -------------------- CFG INT'S --------------------- */
                                                                /* Clear all pending int. sources.                      */
                                                                /* Enable Rx, Tx and other supported int. sources.      */

    INTEnable(INT_ETHERNET, INT_DISABLED);                      /* disable any Eth ints                                 */
    INTClearFlag(INT_ETHERNET);
    EthEventsClr(ETH_EV_ALL);                                   /* clear old pending events                             */
    EthEventsEnableSet(pdev_data->ethEnabledEvents);
    INTEnable(INT_ETHERNET, INT_ENABLED);                       /* enable interrupts                                    */


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

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ----------------- DISABLE RX & TX ------------------ */

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    EthClose(0);


                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* --------------- FREE TX DESCRIPTORS ---------------- */
    NetDev_TxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
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
    NET_DEV_DATA       *pdev_data;
    void               *pbuf_rx;
    void               *pbuf_new;
    CPU_INT16S          length;
    const sEthRxPktStat *pRxPktStat;
    eEthRes             res;
    CPU_SR_ALLOC();



    *size   = (CPU_INT16U  )0;
    *p_data = (CPU_INT08U *)0;
    *perr   = (NET_ERR     )NET_DEV_ERR_RX;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */

    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */

                                                                /* --------------- RECEIVE PACKET      ---------------- */

    res = EthRxGetBuffer(&pbuf_rx, &pRxPktStat);

    if(res != ETH_RES_OK)
    {
        return;                                                 /* no packet available                                  */
    }


                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
    while(1)
    {                                                           /* available packet; minimum check                      */
        if(!pRxPktStat->rxOk || pRxPktStat->runtPkt || pRxPktStat->crcError) {   /*if invalid packet                    */
            *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
            break;
        }
                                                                /* --------------- OBTAIN FRAME LENGTH ---------------- */
        length = pRxPktStat->rxBytes - NET_IF_ETHER_FRAME_CRC_SIZE;


        if (length < NET_IF_ETHER_FRAME_MIN_SIZE) {                 /* If frame is a runt.                              */
            *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
            break;
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

        if (*perr != NET_BUF_ERR_NONE) {                        /* If unable to get a buffer (see Note #3c).            */
            break;
        }


        *size   =  length;                                           /* Return the size of the received frame.               */
        *p_data = (CPU_INT08U *)pbuf_rx;                             /* Return a pointer to the newly received data area.    */
        *perr = NET_DEV_ERR_NONE;

        break;
    }

    EthRxAckBuffer(pbuf_rx, 0);                              /* Acknowledge the received packet */
    CPU_CRITICAL_ENTER();
    EthAckRxDcpt();
    pdev_data->rxPktCnt--;
    CPU_CRITICAL_EXIT();

                                                                /* Update the descriptor to point to a new data area    */
    if(*perr == NET_DEV_ERR_NONE)
    {
        res = EthRxBuffersAppend(&pbuf_new, 1, ETH_BUFF_FLAG_RX_UNACK);
    }
    else
    {
        res = EthRxBuffersAppend(&pbuf_rx, 1, ETH_BUFF_FLAG_RX_UNACK);
    }

    if(res != ETH_RES_OK)
    {
        if(*perr == NET_DEV_ERR_NONE)
        {
            *perr = NET_DEV_ERR_FAULT;
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

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    eEthRes     res;


                                                                /* ---- Acknowledge the previously sent packets --- */
    EthTxAckBuffer(0, NetDev_TxAcknowledge);

                                                                /* Try to transmit the packet                       */
    res=EthTxSendBuffer(p_data, size);

    if(res == ETH_RES_OK) {                                     /* success                                          */
        *perr = NET_DEV_ERR_NONE;
    }
    else if(res == ETH_RES_NO_DESCRIPTORS) {                    /* ok, no more descriptors, we have to try later    */
       *perr = NET_DEV_ERR_TX_BUSY;
    }
    else
    {
       *perr = NET_DEV_ERR_TX;                                  /* shouldn't happen                                 */
    }


    return;
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
#ifdef  NET_MCAST_MODULE_EN
#endif

   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
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

static  void   NetDev_AddrMulticastRemove (NET_IF      *pif,
                                           CPU_INT08U  *paddr_hw,
                                           CPU_INT08U   addr_hw_len,
                                           NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
#endif

   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
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
    NET_DEV_DATA       *pdev_data;
    NET_ERR             err;
    eEthEvents          currEvents;
    CPU_INT32S          nUnack, toSignal, signalled;

   (void)&type;                                                 /* Prevent 'variable unused' compiler warning.          */


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* --------------- DETERMINE ISR TYPE ----------------- */

    currEvents=EthEventsGet()&pdev_data->ethEnabledEvents;      /* keep just those that are relevant                    */

    EthEventsEnableClr(pdev_data->ethMaskedEvents);             /* these will get reported; disable them until ack is   */
                                                                /* received back                                        */

    EthEventsClr(currEvents);                                   /* acknowledge the ETHC                                 */


                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if(currEvents & ETH_EV_RXDONE)
    {                                                           /* packet available                                     */
                                                                /* Determine how many descriptors have become ready ... */

        nUnack=EthDescriptorsGetRxUnack();
        toSignal=nUnack-pdev_data->rxPktCnt;
        signalled = 0;

        while(toSignal > 0)
        {
            NetIF_RxTaskSignal(pif->Nbr, &err);                 /* Signal Net IF RxQ Task for each new rdy descriptor.  */
            if(err == NET_IF_ERR_NONE) {                        /* signal succeeded                                     */
                signalled++;
                toSignal--;
            }
            else {                                              /* RxQ Task full...                                     */
                break;
            }
        }

        if(signalled) {
            pdev_data->rxPktCnt+=signalled;
        }
    }
                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if(currEvents & ETH_EV_TXDONE) {                            /* packet transmitted                                   */
         NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that Tx resources are available.       */
    }

    if(currEvents & pdev_data->ethMaskedEvents) {
                                                                /* TODO: how are the error events reported ???          */
                                                                /* To what thread ???                                   */
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
    NET_PHY_API_ETHER   *pphy_api;
    NET_PHY_CFG_ETHER   *pphy_cfg;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Phy_Cfg;               /* Obtain ptr to the phy cfg struct.                    */

                                                                /* ----------- PERFORM SPECIFIED OPERATION ------------ */
    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;
             if (plink_state == (NET_DEV_LINK_ETHER *)0) {
                *perr = NET_DEV_ERR_NULL_PTR;
                 return;
             }
             pphy_api = (NET_PHY_API_ETHER *)pif->Phy_API;
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

             if (plink_state->Duplex != NET_PHY_DUPLEX_UNKNOWN && plink_state->Spd != NET_PHY_SPD_0)
             {
                 eEthMacPauseType  pauseType;
                 eEthOpenFlags     oFlags;

                 if(plink_state->Duplex == NET_PHY_DUPLEX_FULL)
                 {
                     pauseType = ETH_MAC_PAUSE_CPBL_MASK;
                     oFlags = ETH_OPEN_FDUPLEX;

                 }
                 else
                 {
                     pauseType = ETH_MAC_PAUSE_TYPE_NONE;
                     oFlags = ETH_OPEN_HDUPLEX;
                 }

                 if(plink_state->Spd == NET_PHY_SPD_100)
                 {
                     oFlags |= ETH_OPEN_100;
                 }
                 else
                 {
                     oFlags |= ETH_OPEN_10;
                 }

                 if(pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII)
                 {
                     oFlags |= ETH_OPEN_RMII;
                 }
                 else
                 {
                     oFlags |= ETH_OPEN_MII;
                 }

                 EthMACOpen(oFlags, pauseType);       /* update the MAC with negotiation results */
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

   (void)&pif;                                                 /* Prevent possible 'variable unused' warnings.         */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

    CPU_SR_ALLOC();
                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    while(EthMIIMBusy());           /* make sure there no previous operation in progress */
    CPU_CRITICAL_ENTER();
    EthMIIMReadStart(reg_addr, phy_addr);                       /* Initiate the read operation */
    CPU_CRITICAL_EXIT();

    while(EthMIIMBusy());           /* wait read complete */

    *p_data=(CPU_INT16U)EthMIIMReadResult();

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

   (void)&pif;                                                 /* Prevent possible 'variable unused' warnings.         */
                                                                /* ------------ PERFORM MII WRITE OPERATION ----------- */
    CPU_SR_ALLOC();

   while(EthMIIMBusy());           /* make sure there no previous operation in progress */
   CPU_CRITICAL_ENTER();
   EthMIIMWriteReg(reg_addr, phy_addr, data);
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

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_SIZE_T          desc_nbr;
    CPU_SIZE_T          ix;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    desc_nbr=EthDescriptorsPoolAdd(pdev_cfg->RxDescNbr, ETH_DCPT_TYPE_RX, (pEthDcptAlloc)NetDev_AllocDcpt, &pdev_data->RxDescPool);

    if(desc_nbr != pdev_cfg->RxDescNbr)
    {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* -------------- APPEND RX BUFFERS  --------------- */

    for (ix = 0; ix < pdev_cfg->RxDescNbr; ix++)
    {
        eEthRes      ethRes;
        void*        pRxBuff;

        pRxBuff            =             NetBuf_GetDataPtr((NET_IF        *)pif,
                                                          (NET_TRANSACTION)NET_TRANSACTION_RX,
                                                          (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                                          (NET_BUF_SIZE   )NET_IF_IX_RX,
                                                          (NET_BUF_SIZE  *)0,
                                                          (NET_BUF_SIZE  *)0,
                                                          (NET_BUF_TYPE  *)0,
                                                          (NET_ERR       *)perr);

        if (*perr != NET_BUF_ERR_NONE)
        {
            return;
        }

        ethRes = EthRxBuffersAppend(&pRxBuff, 1, ETH_BUFF_FLAG_RX_UNACK);
        if(ethRes != ETH_RES_OK)
        {
            *perr = NET_DEV_ERR_FAULT;
            return;
        }


    }

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
* Caller(s)   : NetDev_Stop()
*
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL receive buffers
*                   and the Rx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_DATA       *pdev_data;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
                                                                /* Return data area to Rx data area pool.               */
                                                                /* Return Rx descriptor block to Rx descriptor pool.    */

    pdev_data->net_err = NET_DEV_ERR_NONE;
    EthDescriptorsPoolCleanUp ( ETH_DCPT_TYPE_RX, NetDev_RxDeAllocDcpt, pif );

    if(pdev_data->net_err != NET_DEV_ERR_NONE)
    {
        *perr = pdev_data->net_err;
    }
    else
    {
        *perr = NET_DEV_ERR_NONE;
    }

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

static  void  NetDev_TxDescInit (NET_IF  *pif,
                                 NET_ERR *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_SIZE_T          desc_nbr;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */


    desc_nbr=EthDescriptorsPoolAdd(pdev_cfg->TxDescNbr, ETH_DCPT_TYPE_TX, (pEthDcptAlloc)NetDev_AllocDcpt, &pdev_data->TxDescPool);

    if(desc_nbr != pdev_cfg->TxDescNbr)
    {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }


    *perr = NET_DEV_ERR_NONE;
}

/*
*********************************************************************************************************
*                                        NetDev_TxDescFreeAll()
*
* Description : (1) This function returns the descriptor memory blocks back
*                   to their respective memory pools
*
*                   (a) Free Rx descriptor memory block
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
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

static  void  NetDev_TxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_DATA       *pdev_data;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
                                                                /* Return data area to Rx data area pool.               */
                                                                /* Return Rx descriptor block to Rx descriptor pool.    */

    pdev_data->net_err = NET_DEV_ERR_NONE;
    EthDescriptorsPoolCleanUp ( ETH_DCPT_TYPE_TX, NetDev_TxDeAllocDcpt, pif );

    if(pdev_data->net_err != NET_DEV_ERR_NONE)
    {
        *perr = pdev_data->net_err;
    }
    else
    {
        *perr = NET_DEV_ERR_NONE;
    }

}


/* function returns memory for a descriptor */
static void* NetDev_AllocDcpt(CPU_SIZE_T nitems, CPU_SIZE_T size, void* param )
{
    MEM_POOL*   pmem_pool;
    LIB_ERR     lib_err;
    void*       pDcpt;

    pmem_pool = (MEM_POOL*)param;

    pDcpt = (void *)Mem_PoolBlkGet(pmem_pool, nitems*size, &lib_err);

    return pDcpt;

}

/* function deallocates memory for a RX descriptor and associated RX buffer */
static void NetDev_RxDeAllocDcpt(void* pDcpt, void* param )
{
    NET_IF             *pif;
    NET_DEV_DATA       *pdev_data;
    MEM_POOL           *pmem_pool;
    LIB_ERR             lib_err;
    CPU_INT08U         *pRxBuff;

    pif=(NET_IF*)param;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pmem_pool = &pdev_data->RxDescPool;
    pRxBuff = EthDescriptorGetBuffer(pDcpt);
    if(pRxBuff)
    {
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pRxBuff);           /* Return data area to Rx data area pool.               */
    }

                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
                     pDcpt,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
        pdev_data->net_err = lib_err;
        return;
    }

}


/* function deallocates memory for a TX descriptor and associated TX buffer */
static void NetDev_TxDeAllocDcpt(void* pDcpt, void* param )
{
    NET_IF             *pif;
    NET_DEV_DATA       *pdev_data;
    MEM_POOL           *pmem_pool;
    LIB_ERR             lib_err;
    CPU_INT08U         *pTxBuff;

    pif=(NET_IF*)param;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


    pTxBuff = EthDescriptorGetBuffer(pDcpt);
    NetDev_TxDeallocPost(pTxBuff);                              /* Deallocate TX buffers */


                                                                /* ------------- FREE TX DESC DATA AREAS -------------- */
    pmem_pool = &pdev_data->TxDescPool;
                                                                /* ---------------- FREE TX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Tx descriptor pool.    */
                     pDcpt,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
        pdev_data->net_err = lib_err;
        return;
    }

}

/* performs deallocation of a TX buffer */
static void NetDev_TxDeallocPost(CPU_INT08U* pTxBuff)
{
    if(pTxBuff)
    {
        NET_ERR     err;

        while(1)                                                /* Loop until successfully queued for deallocation.     */
        {
            NetIF_TxDeallocTaskPost(pTxBuff, &err);
            if (err == NET_IF_ERR_NONE)
            {
                break;
            }
            else
            {
                NetOS_TimeDly_ms(2u, &err);                     /* Delay & retry if queue is full.                      */
            }
        }
    }
}


/* acknowledge the transmitted packets  */
static  void  NetDev_TxAcknowledge (void *pTxBuff, int buffIx )
{

    (void)&buffIx;                                                  /* Prevent 'variable unused' compiler warnings.         */

    NetDev_TxDeallocPost(pTxBuff);
}


#endif /* NET_IF_ETHER_MODULE_EN */

