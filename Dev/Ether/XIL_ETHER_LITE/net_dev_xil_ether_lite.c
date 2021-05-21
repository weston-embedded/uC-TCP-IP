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
*                                       XILINX AXI ETHERNET LITE
*
* Filename : net_dev_xil_ether_lite.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_TEMPLATE_ETHER_PIO_MODULE
#include  <lib_mem.h>
#include  <Source/net.h>
#include  <Source/net_ctr.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_xil_ether_lite.h"

#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Receive buffers usually MUST be aligned to some octet boundary.  However, adjusting
*               receive buffer alignment MUST be performed from within 'net_dev_cfg.h'.  Do not adjust
*               the value below as it is used for configuration checking only.
*********************************************************************************************************
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO                              10000    /* MII read write timeout.                              */
#define  RX_BUF_ALIGN_OCTETS                               4    /* See Note #1.                                         */

#define  TX_BUF_STATUS                                     1

#define  TX_PING_REQ                              DEF_BIT_00
#define  TX_PONG_REQ                              DEF_BIT_01


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

#define  GIE_INTEN        DEF_BIT_31                            /* Global Enable Bit                                    */

#define  TXCTRL_STAT      DEF_BIT_00                            /* TX Status                                            */
#define  TXCTRL_PROG      DEF_BIT_01                            /* Addr Program                                         */
#define  TXCTRL_INT_EN    DEF_BIT_03                            /* TX Interrupt Enable                                  */
#define  TXCTRL_LOOP_BACK DEF_BIT_04                            /* Internal Loopbak Enable                              */

#define  RXCTRL_STAT      DEF_BIT_00                            /* RX Status                                            */
#define  RXCTRL_INT_EN    DEF_BIT_03                            /* RX Interrupt Enable                                  */



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
*           (2) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*/

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR      StatRxPktCtr;
    NET_CTR      StatTxPktCtr;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR      ErrRxPktDiscardedCtr;
    NET_CTR      ErrTxPktDiscardedCtr;
#endif

    CPU_INT32U   TXStatus;
    CPU_INT32U   RXSig;
    CPU_INT32U   RXPong;

    CPU_INT08U  *TxPingPtr;
    CPU_INT08U  *TxPongPtr;
#ifdef  NET_MCAST_MODULE_EN
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

typedef  struct  net_dev {
    CPU_REG32  TxPingBuf[0x1F9];                                /* Transmit Ping Buffer                                 */
    CPU_REG32  MDIOADDR;                                        /* MDIO Address Register                                */
    CPU_REG32  MDIOWR;                                          /* MDIO Write Date Register                             */
    CPU_REG32  MDIORD;                                          /* MDIO Read Data Register                              */
    CPU_REG32  MDIOCTRL;                                        /* MDIO Control Register                                */
    CPU_REG32  TXPingLength;                                    /* Transmit Ping Length Register                        */
    CPU_REG32  GIE;                                             /* Global Interrupt Enable Register                     */
    CPU_REG32  TXPingCtrl;                                      /* Transmit Ping Control Register                       */
    CPU_REG32  TxPongBuf[0x1F9];                                /* Transmit Pong Buffer                                 */
    CPU_REG32  Reserved0[4];
    CPU_REG32  TXPongLength;                                    /* Transmit Pong Length                                 */
    CPU_REG32  Reserved1;
    CPU_REG32  TXPongCtrl;                                      /* Transmit Pong Control Register                       */
    CPU_REG32  RXPingBuf[0x1F9];                                /* Receive Ping Buffer                                  */
    CPU_REG32  Reserved2[6];
    CPU_REG32  RXPingCtrl;                                      /* Receive Ping Control Register                        */
    CPU_REG32  RXPongBuf[0x1F9];                                /* Receive Pong Buffer                                  */
    CPU_REG32  Reserved3[6];
    CPU_REG32  RXPongCtrl;                                      /* Receive Pong Control Register                        */
} NET_DEV;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) Global variables are highly discouraged and should only be used for storing NON-instance
*               specific data and the array of instance specific data.  Global variables, those that are
*               not declared within the NET_DEV_DATA area, are not multiple-instance safe and could lead
*               to incorrect driver operation if used to store device state information.
*********************************************************************************************************
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Device driver functions may be arbitrarily named.  However, it is recommended that device
*               driver functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_&&&.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*********************************************************************************************************
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


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
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

const  NET_DEV_API_ETHER  NetDev_API_XIL_ETHER_LITE = {                                 /* Ether PIO dev API fnct ptrs :*/
                                                          &NetDev_Init,                 /*   Init/add                   */
                                                          &NetDev_Start,                /*   Start                      */
                                                          &NetDev_Stop,                 /*   Stop                       */
                                                          &NetDev_Rx,                   /*   Rx                         */
                                                          &NetDev_Tx,                   /*   Tx                         */
                                                          &NetDev_AddrMulticastAdd,     /*   Multicast addr add         */
                                                          &NetDev_AddrMulticastRemove,  /*   Multicast addr remove      */
                                                          &NetDev_ISR_Handler,          /*   ISR handler                */
                                                          &NetDev_IO_Ctrl,              /*   I/O ctrl                   */
                                                          &NetDev_MII_Rd,               /*   Phy reg rd                 */
                                                          &NetDev_MII_Wr                /*   Phy reg wr                 */
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
*                   (e) pdev_cfg->DataBusSizeNbrBits
*
*               (8) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*
*               (9) Some drivers MAY require knowledge of the Phy configuration in order
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
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* See Note #7.                                         */
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

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ------ */
                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ---- INITIALIZE ALL DEVICE DATA AREA VARIABLES ----- */
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->StatRxPktCtr         = 0;
    pdev_data->StatTxPktCtr         = 0;
#endif

#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    pdev_data->ErrRxPktDiscardedCtr = 0;
    pdev_data->ErrTxPktDiscardedCtr = 0;
#endif

    pdev_data->RXSig    = DEF_NO;
    pdev_data->RXPong   = DEF_NO;
    pdev_data->TXStatus = 0u;


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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    pdev->MDIOCTRL = DEF_BIT_00;                                /* Enable MDIO module.                                  */

                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #2.                                         */
                            pdev_cfg->TxDescNbr,
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
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        pdev->TxPingBuf[0] = hw_addr[0];
        pdev->TxPingBuf[1] = hw_addr[1];
        pdev->TxPingBuf[2] = hw_addr[2];
        pdev->TxPingBuf[3] = hw_addr[3];
        pdev->TxPingBuf[4] = hw_addr[4];
        pdev->TxPingBuf[5] = hw_addr[5];

        pdev->TXPingCtrl = TXCTRL_STAT | TXCTRL_PROG;

        while (pdev->TXPingCtrl & (TXCTRL_STAT | TXCTRL_PROG)) {

        }
    }

    pdev->RXPingCtrl = RXCTRL_INT_EN;

    if (pdev_cfg->RxDescNbr == 2) {
        pdev->RXPongCtrl = RXCTRL_INT_EN;
    }

    pdev->GIE |= GIE_INTEN;

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
    NET_DEV            *pdev;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* Disable Interrupts.                                  */
    pdev->GIE &= ~GIE_INTEN;
    pdev->RXPingCtrl = 0u;
    if (pdev_cfg->RxDescNbr == 2) {
        pdev->RXPongCtrl = 0u;
    }

                                                                /* ------------------- FREE TX BUFS ------------------- */
    if ((pdev_data->TXStatus & TX_PING_REQ) != 0) {
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdev_data->TxPingPtr, &err);
        (void)&err;
    }

    if ((pdev_data->TXStatus & TX_PONG_REQ) != 0) {
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdev_data->TxPongPtr, &err);
        (void)&err;
    }

    pdev_data->TXStatus = 0u;

   *perr = NET_DEV_ERR_NONE;
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
*               (3) If a receive error occurs, the function SHOULD return 0 for the size, a
*                   NULL pointer to the data area AND an error code equal to NET_DEV_ERR_RX.
*                   Some devices may require that driver instruct the hardware to drop the
*                   frame if it has been commited to internal device memory.
*
*                   (a) If a new data area is unavailable, the driver MUST instruct hardware
*                       to discard the frame.
*
*               (4) Reading data from the device hardware may occur in various sized reads :
*
*                   (a) Device drivers that require read sizes equivalent to the size of the
*                       device data bus MAY examine pdev_cfg->DataBusSizeNbrBits in order to
*                       determine the number of required data reads.
*
*                   (b) Devices drivers that require read sizes equivalent to the size of the
*                       Rx FIFO width SHOULD use the known FIFO width to determine the number
*                       of required data reads.
*
*                   (c) It may be necessary to round the number of data reads up, OR perform
*                       the last data read outside of the loop.
*
*               (5) A pointer set equal to pbuf_new and sized according to the required data
*                   read size determined in (4) should be used to read data from the device
*                   into the receive buffer.
*
*               (6) Some devices may interrupt only ONCE for a recieved frame.  The driver MAY
*                   need check if additional frames have been received while processing the
*                   current received frame.  If additional frames have been received, the driver
*                   MAY need to signal the receive task before exiting NetDev_Rx().
*
*               (7) Some devices optionally include each receive packet's CRC in the received
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
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_INT08U         *pbuf_new;
    CPU_REG32          *rdy_buf_ptr;
    CPU_REG32          *rx_ctrl;
    CPU_BOOLEAN         rx_crc;
    CPU_INT16U          rx_len;
    CPU_INT16U          cnt;
    CPU_INT16U          i;
    NET_ERR             err;
    CPU_INT08U         *tx_data;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* --------------- OBTAIN FRAME LENGTH ---------------- */
    if (pdev_data->RXPong == DEF_NO) {
        if ((pdev->RXPingCtrl & RXCTRL_STAT) != 0u) {
            rdy_buf_ptr = pdev->RXPingBuf;
            rx_ctrl = &pdev->RXPingCtrl;
            pdev_data->RXPong = DEF_YES;
        } else if ((pdev_cfg->RxDescNbr == 2) && ((pdev->RXPongCtrl & RXCTRL_STAT) != 0u)) {
            rdy_buf_ptr = pdev->RXPongBuf;
            rx_ctrl = &pdev->RXPongCtrl;
        } else {
           *perr = NET_DEV_ERR_RX;
            return;
        }
    } else {
        if ((pdev_cfg->RxDescNbr == 2) && ((pdev->RXPongCtrl & RXCTRL_STAT) != 0u)) {
            rdy_buf_ptr = pdev->RXPongBuf;
            rx_ctrl = &pdev->RXPongCtrl;
            pdev_data->RXPong = DEF_NO;
        } else if ((pdev->RXPingCtrl & RXCTRL_STAT) != 0u) {
            rdy_buf_ptr = pdev->RXPingBuf;
            rx_ctrl = &pdev->RXPingCtrl;
        } else {
           *perr = NET_DEV_ERR_RX;
            return;
        }
    }


                                                                /* Extract frame length from received data.             */
    MEM_VAL_COPY_GET_INT16U_BIG(&rx_len, &((CPU_INT08U *)rdy_buf_ptr)[12]);

    if (rx_len > 1518) {                                        /* Only bother to decode ARP frames. Receive path with handle the rest even if size if wrong. */
        if (rx_len == 0x806) {
            rx_len = 64;
        } else {
            rx_len = 1514;
        }
    } else {
        rx_len = rx_len + 14;
    }

                                                                /* --------- OBTAIN PTR TO NEW DMA DATA AREA ---------- */
                                                                /* Request an empty buffer.                             */
    pbuf_new = NetBuf_GetDataPtr(pif,
                                 NET_TRANSACTION_RX,
                                 NET_IF_ETHER_FRAME_MAX_SIZE,
                                 NET_IF_IX_RX,
                                 0,
                                 0,
                                 DEF_NULL,
                                 perr);

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer (see Note #3c).            */
        *size   = 0u;
        *p_data = 0u;
         pdev->RXPingCtrl = RXCTRL_INT_EN;                      /* See Note #3.                                         */
         NET_CTR_ERR_INC(pdev_data->ErrRxPktDiscardedCtr);
         return;
    }

   *size = rx_len;                                              /* Return the size of the received frame.               */

                                                                /* Copy the received data.                              */
    for (i = 0; i < ((rx_len / 4) + 1); i++) {
        ((CPU_INT32U *)pbuf_new)[i] = rdy_buf_ptr[i];           /* This is device/perihperal memory Mem_Copy or memcpy cannot be used. */
    }

   *rx_ctrl = RXCTRL_INT_EN;

   *p_data = pbuf_new;                                          /* Return a pointer to the received data.               */

    NET_CTR_STAT_INC(pdev_data->StatRxPktCtr);


   CPU_CRITICAL_ENTER();                                        /* Check if something is ready to be received.          */
   if ((pdev->RXPingCtrl & RXCTRL_STAT) != 0u) {
       CPU_CRITICAL_EXIT();
       pdev_data->RXSig = DEF_YES;
       NetIF_RxTaskSignal(pif->Nbr, &err);
   } else if ((pdev_cfg->RxDescNbr == 2) && ((pdev->RXPongCtrl & RXCTRL_STAT) != 0u)) {
       CPU_CRITICAL_EXIT();
       pdev_data->RXSig = DEF_YES;
       NetIF_RxTaskSignal(pif->Nbr, &err);
   } else {
       pdev_data->RXSig = DEF_NO;
       CPU_CRITICAL_EXIT();
   }


   CPU_CRITICAL_ENTER();                                        /* Check transmit status since we can't differentiate between a tx and rx interrupt*/
   if (((pdev->TXPingCtrl & TXCTRL_STAT) == 0) && ((pdev_data->TXStatus & TX_PING_REQ) != 0)) {
        NET_CTR_STAT_INC(pdev_data->StatTxPktCtr);             /* Increment Tx Pkt ctr.                                */
        tx_data = (CPU_INT08U *)pdev_data->TxPingPtr;
        pdev_data->TXStatus &= ~TX_PING_REQ;
        CPU_CRITICAL_EXIT();
        NetIF_TxDeallocTaskPost(tx_data, &err);
        NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that Tx resources are available.       */
   } else {
       CPU_CRITICAL_EXIT();
   }

   CPU_CRITICAL_ENTER();
   if ((pdev_cfg->TxDescNbr == 2) && ((pdev->TXPongCtrl & TXCTRL_STAT) == 0) && ((pdev_data->TXStatus & TX_PONG_REQ) != 0)) {
        NET_CTR_STAT_INC(pdev_data->StatTxPktCtr);             /* Increment Tx Pkt ctr.                                */
        tx_data = (CPU_INT08U *)pdev_data->TxPongPtr;
        pdev_data->TXStatus &= ~TX_PONG_REQ;
        CPU_CRITICAL_EXIT();
        NetIF_TxDeallocTaskPost(tx_data, &err);
        NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that Tx resources are available.       */
   } else {
       CPU_CRITICAL_EXIT();
   }


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
*               (3) Software MUST track all transmit buffer addresses that are that are queued for
*                   transmission but have not received a transmit complete notification (interrupt).
*                   Once the frame has been transmitted, software must post the buffer address of
*                   the frame that has completed transmission to the transmit deallocation task.
*                   See NetDev_ISR_Handler() for more information.
*
*               (4) Writing data to the device hardware may occur in various sized writes :
*
*                   (a) Device drivers that require write sizes equivalent to the size of the
*                       device data bus MAY examine pdev_cfg->DataBusSizeNbrBits in order to
*                       determine the number of required data writes.
*
*                   (b) Devices drivers that require write sizes equivalent to the size of the
*                       Tx FIFO width SHOULD use the known FIFO width to determine the number
*                       of required data reads.
*
*                   (c) It may be necessary to round the number of data writes up, OR perform the
*                       last data write outside of the loop.
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
    CPU_INT16U          cnt;
    CPU_INT32U          i;
    CPU_INT32U          tx_status;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

    CPU_CRITICAL_ENTER();
    tx_status = pdev_data->TXStatus;
    CPU_CRITICAL_EXIT();

    if ((pdev_data->TXStatus & TX_PING_REQ) == 0u) {
        for (i = 0u; i < ((size / 4) + 1); i++) {               /* Copy transmit data.                                  */
            pdev->TxPingBuf[i] = ((CPU_INT32U *)p_data)[i];     /* This is device memory memcpy or Mem_Copy cannot be used. */
        }
        CPU_CRITICAL_ENTER();
        pdev->TXPingLength = size;
        pdev->TXPingCtrl |= TXCTRL_STAT;
        pdev_data->TxPingPtr = p_data;
        pdev_data->TXStatus |= TX_PING_REQ;
        CPU_CRITICAL_EXIT();
    } else if ((pdev_cfg->TxDescNbr == 2) && ((pdev_data->TXStatus & TX_PONG_REQ) == 0u)) {
        for (i = 0u; i < ((size / 4) + 1); i++) {
            pdev->TxPongBuf[i] = ((CPU_INT32U *)p_data)[i];
        }
        CPU_CRITICAL_ENTER();
        pdev->TXPongLength = size;
        pdev->TXPongCtrl |= TXCTRL_STAT;
        pdev_data->TxPongPtr = p_data;
        pdev_data->TXStatus |= TX_PONG_REQ;
        CPU_CRITICAL_EXIT();
    } else {
        *perr = NET_DEV_ERR_TX_BUSY;
    }

    CPU_MB();


   *perr = NET_DEV_ERR_NONE;

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
* Note(s)     : Multicast filtering not supported by the AXI ethernet lite.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
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
* Note(s)     : Multicast filtering not supported by the AXI ethernet lite.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
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
*
*                           NET_DEV_ISR_TYPE_RX
*                           NET_DEV_ISR_TYPE_TX_COMPLETE
*                           NET_DEV_ISR_TYPE_UNKNOWN
*
* Return(s)   : none.
*
* Caller(s)   : Specific first- or second-level BSP ISR handler.
*
* Note(s)     : (1) Due to race conditions possible with the AXI ethernet lite we have to assume an
*                   RX interrupt might be missed. As such signal the RX task anyways without checking.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV            *pdev;
    CPU_DATA            reg_val;
    CPU_INT08U         *p_data;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */




                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if (((pdev->TXPingCtrl & TXCTRL_STAT) == 0) && ((pdev_data->TXStatus & TX_PING_REQ) != 0)) {
         NET_CTR_STAT_INC(pdev_data->StatTxPktCtr);             /* Increment Tx Pkt ctr.                                */
         p_data = (CPU_INT08U *)pdev_data->TxPingPtr;
         NetIF_TxDeallocTaskPost(p_data, &err);
         NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that Tx resources are available.       */
         pdev_data->TXStatus &= ~TX_PING_REQ;
    }

    if ((pdev_cfg->TxDescNbr == 2) && ((pdev->TXPongCtrl & TXCTRL_STAT) == 0) && ((pdev_data->TXStatus & TX_PONG_REQ) != 0)) {
         NET_CTR_STAT_INC(pdev_data->StatTxPktCtr);             /* Increment Tx Pkt ctr.                                */
         p_data = (CPU_INT08U *)pdev_data->TxPongPtr;
         NetIF_TxDeallocTaskPost(p_data, &err);
         NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that Tx resources are available.       */
         pdev_data->TXStatus &= ~TX_PONG_REQ;
    }

                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if (pdev_data->RXSig == DEF_NO) {
        pdev_data->RXSig = DEF_YES;
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal RX, see note 1.                               */
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



                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */
    pdev->MDIOADDR = reg_addr | (phy_addr << 5u) | DEF_BIT_10;
    pdev->MDIOCTRL = DEF_BIT_00 | DEF_BIT_03;

    while (pdev->MDIOCTRL & DEF_BIT_00) {
        if (timeout-- == 0) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
            break;
        }
    }

   *p_data = pdev->MDIORD;


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
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------ PERFORM MII READ OPERATION ------------ */

    pdev->MDIOADDR = reg_addr | (phy_addr << 5u);
    pdev->MDIOWR = data;
    pdev->MDIOCTRL = DEF_BIT_00 | DEF_BIT_03;

    timeout = MII_REG_RD_WR_TO;
    while (pdev->MDIOCTRL & DEF_BIT_00) {
        if (timeout-- == 0) {
           *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
            break;
        }
    }

   *perr = NET_PHY_ERR_NONE;
}

#endif  /* NET_IF_ETHER_MODULE_EN */
