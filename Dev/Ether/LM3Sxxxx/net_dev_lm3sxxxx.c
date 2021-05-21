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
*                                       LUMINARY MICRO LM3Sxxxx
*
* Filename : net_dev_lm3sxxxx.c
* Version  : V3.06.01
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
#include  "net_dev_lm3sxxxx.h"

#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  MII_REG_RD_WR_TO               10000                   /* MII read write timeout.                              */


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    CPU_INT08U  *TxBufCompPtr;
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
                                                                                    /* LM3Sxxxx dev API fnct ptrs :     */
const  NET_DEV_API_ETHER  NetDev_API_LM3Sxxxx = { NetDev_Init,                      /*   Init/add                       */
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

/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*********************************************************************************************************
*/

#define  LM3SXXXX_BIT_MACIS_RXINT                 DEF_BIT_00
#define  LM3SXXXX_BIT_MACIS_TXER                  DEF_BIT_01
#define  LM3SXXXX_BIT_MACIS_TXEMP                 DEF_BIT_02
#define  LM3SXXXX_BIT_MACIS_FOV                   DEF_BIT_03
#define  LM3SXXXX_BIT_MACIS_RXER                  DEF_BIT_04
#define  LM3SXXXX_BIT_MACIS_MDINT                 DEF_BIT_05
#define  LM3SXXXX_BIT_MACIS_PHYINT                DEF_BIT_06

#define  LM3SXXXX_BIT_MACIM_RXINT                 DEF_BIT_00
#define  LM3SXXXX_BIT_MACIM_TXER                  DEF_BIT_01
#define  LM3SXXXX_BIT_MACIM_TXEMP                 DEF_BIT_02
#define  LM3SXXXX_BIT_MACIM_FOV                   DEF_BIT_03
#define  LM3SXXXX_BIT_MACIM_RXER                  DEF_BIT_04
#define  LM3SXXXX_BIT_MACIM_MDINT                 DEF_BIT_05
#define  LM3SXXXX_BIT_MACIM_PHYINT                DEF_BIT_06

#define  LM3SXXXX_BIT_MACRCTL_RXEN                DEF_BIT_00
#define  LM3SXXXX_BIT_MACRCTL_AMUL                DEF_BIT_01
#define  LM3SXXXX_BIT_MACRCTL_PRMS                DEF_BIT_02
#define  LM3SXXXX_BIT_MACRCTL_BADCRC              DEF_BIT_03
#define  LM3SXXXX_BIT_MACRCTL_RSTFIFO             DEF_BIT_04

#define  LM3SXXXX_BIT_MACTCTL_TXEN                DEF_BIT_00
#define  LM3SXXXX_BIT_MACTCTL_PADEN               DEF_BIT_01
#define  LM3SXXXX_BIT_MACTCTL_CRC                 DEF_BIT_02
#define  LM3SXXXX_BIT_MACTCTL_DUPLEX              DEF_BIT_04

#define  LM3SXXXX_BIT_MACIA0_MACOCT4              0xFF000000u
#define  LM3SXXXX_BIT_MACIA0_MACOCT3              0x00FF0000u
#define  LM3SXXXX_BIT_MACIA0_MACOCT2              0x0000FF00u
#define  LM3SXXXX_BIT_MACIA0_MACOCT1              0x000000FFu

#define  LM3SXXXX_BIT_MACIA1_MACOCT6              0x0000FF00u
#define  LM3SXXXX_BIT_MACIA1_MACOCT5              0x000000FFu

#define  LM3SXXXX_BIT_MACTHR_THRESH               0x0000003Fu

#define  LM3SXXXX_BIT_MACMCTL_START               DEF_BIT_00
#define  LM3SXXXX_BIT_MACMCTL_WRITE               DEF_BIT_01
#define  LM3SXXXX_BIT_MACMCTL_REGADR              0x000000F8u

#define  LM3SXXXX_BIT_MACMDV_DIV                  0x000000FFu

#define  LM3SXXXX_BIT_MACMTXD_MDTX                0x0000FFFFu

#define  LM3SXXXX_BIT_MACMRXD_MDRX                0x0000FFFFu

#define  LM3SXXXX_BIT_MACNP_NPR                   0x0000003Fu

#define  LM3SXXXX_BIT_MACTR_NEWTX                 DEF_BIT_00

#define  LM3SXXXX_BIT_MACTS_TSEN                  DEF_BIT_00


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
* Note(s)     : (2) The application developer SHOULD define NetDev_CfgClk() within net_bsp.c
*                   in order to properly enable clocks for specified network interface.
*
*               (3) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.
*
*               (4) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
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
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;


                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
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
                                                                /* Enable module clks (see Note #2).                    */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure Ethernet Controller GPIO (see Note #3).    */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure ext int ctrl'r (see Note #4).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


                                                                /* ------------------- INIT DEV REGS ------------------ */
    pdev->MACIM   = 0u;                                         /* Dis all ints.                                        */
    pdev->MACRCTL = 0u;                                         /* Dis rx'er.                                           */
    pdev->MACTCTL = 0u;                                         /* Dis tx'er.                                           */


                                                                /* --------------- INIT DEV DATA AREA ---------------- */
    pdev_data->TxBufCompPtr = (CPU_INT08U *)0;

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
*                                                                configure the HW address using
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
    CPU_INT32U          clk_freq;
    CPU_INT32U          div;
    CPU_INT32U          reg_val;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;



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
        pdev->MACIA0 = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                        ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                        ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                        ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));

        pdev->MACIA1 = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                        ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));
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

    div          = (clk_freq / 2u) / 2500000u;
    pdev->MACMDV = (div & LM3SXXXX_BIT_MACMDV_DIV);



                                                                /* ------------------ CFG TX'R & RX'R ----------------- */
    pdev->MACTCTL  = LM3SXXXX_BIT_MACTCTL_PADEN  |              /* Cfg rx'r.                                            */
                     LM3SXXXX_BIT_MACTCTL_DUPLEX |
                     LM3SXXXX_BIT_MACTCTL_CRC;
    pdev->MACRCTL  = LM3SXXXX_BIT_MACRCTL_BADCRC;               /* Cfg tx'r                                             */


                                                                /* ------------------ EN  TX'R & RX'R ----------------- */
    pdev->MACRCTL |= LM3SXXXX_BIT_MACRCTL_RSTFIFO;              /* Reset the EMAC FIFO.                                 */
    pdev->MACRCTL |= LM3SXXXX_BIT_MACRCTL_RXEN;                 /* En rx'r.                                             */
    pdev->MACTCTL |= LM3SXXXX_BIT_MACTCTL_TXEN;                 /* En tx'r.                                             */
    pdev->MACRCTL |= LM3SXXXX_BIT_MACRCTL_RSTFIFO;              /* Reset the EMAC FIFO.                                 */

    pdev->MACIM   |= LM3SXXXX_BIT_MACIM_RXINT |                 /* En rx & tx ints.                                     */
                     LM3SXXXX_BIT_MACIM_FOV   |
                     LM3SXXXX_BIT_MACIM_RXER  |
                     LM3SXXXX_BIT_MACIM_TXEMP;

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
    NET_DEV            *pdev;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------------- CLR DEV REGS ------------------- */
    pdev->MACIM   = 0u;                                         /* Dis all ints.                                        */
    pdev->MACRCTL = 0u;                                         /* Dis rx'er.                                           */
    pdev->MACTCTL = 0u;                                         /* Dis tx'er.                                           */


                                                                /* ------------------- FREE TX BUFS ------------------- */
    if (pdev_data->TxBufCompPtr != (CPU_INT08U *)0) {           /* If NOT yet  tx'd, ...                                */
        NetIF_TxDeallocTaskPost(pdev_data->TxBufCompPtr, &err); /* ... dealloc tx buf (see Note #2a1).                  */
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdev_data->TxBufCompPtr = (CPU_INT08U *)0;
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
    CPU_INT08U         *pbuf_new;
    CPU_INT16U         *pbuf_new_16;
    NET_ERR             err;
    CPU_INT16U          len;
    CPU_INT08U          pkt_cnt;
    CPU_INT16U          rd_cnt;
    CPU_INT32U          temp;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


                                                                /* ------------------- GET PKT SIZE ------------------- */
    pkt_cnt = (pdev->MACNP & LM3SXXXX_BIT_MACNP_NPR);
    if (pkt_cnt == 0) {                                         /* No pkt is in the FIFO.                               */
       *perr = NET_DEV_ERR_RX;
        pdev->MACIM |= LM3SXXXX_BIT_MACIM_RXINT | LM3SXXXX_BIT_MACIM_FOV | LM3SXXXX_BIT_MACIM_RXER;
        return;
    }

    temp =  pdev->MACDATA;                                      /* Rd word from FIFO.                                   */
    len  = (CPU_INT16U)(temp)                        & DEF_INT_16_MASK;
    temp = (CPU_INT16U)(temp >> DEF_INT_16_NBR_BITS) & DEF_INT_16_MASK;

    if (len < 6u) {                                             /* Insufficient  data was rx'd.                         */
        len = 0u;
    } else {                                                    /* If sufficient data was rx'd ...                      */
        len -= 6u;                                              /*  ... sub 6 = 4 (for FCS) + 2 (for size).             */
        if (len >= 2028u) {
            len  =    0u;
        }
    }

    if (len < 1u) {                                             /* Pkt size invalid.                                    */
       *perr = NET_DEV_ERR_RX;



    } else {


                                                                /* --------------------- GET BUF ---------------------- */
                                                                /* Get an empty buf.                                    */
        pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                     (NET_BUF_SIZE   )NET_IF_IX_RX,
                                     (NET_BUF_SIZE  *)0,
                                     (NET_BUF_SIZE  *)0,
                                     (NET_BUF_TYPE  *)0,
                                     (NET_ERR       *)perr);



        if (*perr != NET_BUF_ERR_NONE) {                        /* ----------------- DISCARD RX'D PKT ----------------- */
            *size   = (CPU_INT16U  )0;                          /* Clr size & buf ptr.                                  */
            *p_data = (CPU_INT08U *)0;

            len    -=  sizeof(CPU_INT16U);                      /* Rd pkt from FIFO.                                    */
            rd_cnt  = (len + sizeof(CPU_INT32U) - 1u) / sizeof(CPU_INT32U);
            while (rd_cnt > 0u) {
                temp = pdev->MACDATA;
               (void)&temp;
                rd_cnt--;
            }

            temp = pdev->MACDATA;                               /* Rd FCS.                                              */
           (void)&temp;



        } else {                                                /* ----------------------- RX PKT --------------------- */
           *size   = len;                                       /* Rtn the size of the rx'd frame.                      */
           *p_data = pbuf_new;                                  /* Rtn a ptr    to the rx'd data.                       */

            pbuf_new_16 = (CPU_INT16U *)pbuf_new;
           *pbuf_new_16 = (CPU_INT16U)temp;
            pbuf_new_16++;
            len        -= sizeof(CPU_INT16U);

            rd_cnt      = (len + sizeof(CPU_INT32U) - 1u) / sizeof(CPU_INT32U);
            while (rd_cnt > 0u) {
                temp        = pdev->MACDATA;
               *pbuf_new_16 = (CPU_INT16U)(temp)                        & DEF_INT_16_MASK;
                pbuf_new_16++;
               *pbuf_new_16 = (CPU_INT16U)(temp >> DEF_INT_16_NBR_BITS) & DEF_INT_16_MASK;
                pbuf_new_16++;
                rd_cnt--;
            }

            temp = pdev->MACDATA;                               /* Rd FCS.                                              */
           (void)&temp;

           *perr = NET_DEV_ERR_NONE;
        }
    }



                                                                /* --------------- SIG PKT / EN RX INTS --------------- */
    pkt_cnt = (pdev->MACNP & LM3SXXXX_BIT_MACNP_NPR);           /* Determine if pkts remain to be rd.                   */
    if (pkt_cnt > 0u) {                                         /* Pkts rx'd ... signal rx task.                        */
        NetIF_RxTaskSignal(pif->Nbr, &err);
    } else {                                                    /* No pkts rx'd ... en rx ints.                         */
        pdev->MACIM |= LM3SXXXX_BIT_MACIM_RXINT | LM3SXXXX_BIT_MACIM_FOV | LM3SXXXX_BIT_MACIM_RXER;
    }
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
    CPU_INT16U         *p_data_16;
    CPU_INT32U          temp;
    CPU_INT16U          wr_cnt;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
    p_data_16 = (CPU_INT16U        *)p_data;

                                                                /* ---------------------- TX PKT ---------------------- */
                                                                /* Form first word of FIFO data.                        */
    temp           = size - 14u;                                /* Length = size - 6 (for sender MAC)                   */
                                                                /*               - 6 (for dest   MAC)                   */
                                                                /*               - 2 (for len/type code).               */
    temp          |= (CPU_INT32U)*p_data_16 << DEF_INT_16_NBR_BITS;
    pdev->MACDATA  = temp;

    p_data_16++;
    size          -=  sizeof(CPU_INT16U);

    wr_cnt         = (size + sizeof(CPU_INT32U) - 1u) / sizeof(CPU_INT32U);
    while (wr_cnt > 0u) {
        temp           = (CPU_INT32U)*p_data_16;
        p_data_16++;
        temp          |= (CPU_INT32U)*p_data_16 << DEF_INT_16_NBR_BITS;
        p_data_16++;
        pdev->MACDATA  = temp;

        wr_cnt--;
    }

    CPU_CRITICAL_ENTER();
    pdev_data->TxBufCompPtr = p_data;                           /* See Note #2.                                         */
    pdev->MACTR             = LM3SXXXX_BIT_MACTR_NEWTX;         /* Start tx.                                            */
    CPU_CRITICAL_EXIT();

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
#else
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

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
#else
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
    NET_DEV            *pdev;
    CPU_INT08U         *pdata;
    NET_ERR             err;
    CPU_INT32U          status;


   (void)&type;                                                 /* Prevent compiler warning.                            */


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    status       =  pdev->MACIS;                                /* Rd  the int status.                                  */
    status      &=  pdev->MACIM;
    pdev->MACIS  =  status;                                     /* Clr the int status.                                  */



                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if (DEF_BIT_IS_SET(status, LM3SXXXX_BIT_MACIS_RXINT) == DEF_YES) {
        pdev->MACIM &= ~(LM3SXXXX_BIT_MACIM_RXINT | LM3SXXXX_BIT_MACIM_FOV | LM3SXXXX_BIT_MACIM_RXER);
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task for each new rdy pkt.         */
    }



                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if (DEF_BIT_IS_SET(status, LM3SXXXX_BIT_MACIS_TXEMP) == DEF_YES) {
         pdata                   =  pdev_data->TxBufCompPtr;
         pdev_data->TxBufCompPtr = (CPU_INT08U *)0;
         NetIF_TxDeallocTaskPost(pdata, &err);
         NetIF_DevTxRdySignal(pif);                             /* Signal Net IF that tx resources are available.       */
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

             switch (plink_state->Duplex) {                     /* Update duplex register setting in MAC.               */
                case NET_PHY_DUPLEX_FULL:
                     pdev->MACTCTL |=  LM3SXXXX_BIT_MACTCTL_DUPLEX;
                     break;

                case NET_PHY_DUPLEX_HALF:
                     pdev->MACTCTL &= ~LM3SXXXX_BIT_MACTCTL_DUPLEX;
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


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

   (void)&phy_addr;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&reg_addr;

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */

    while (DEF_BIT_IS_SET(pdev->MACMCTL, LM3SXXXX_BIT_MACMCTL_START) == DEF_YES) {
        ;
    }

    pdev->MACMCTL = (reg_addr << 3) | LM3SXXXX_BIT_MACMCTL_START;

    while (DEF_BIT_IS_SET(pdev->MACMCTL, LM3SXXXX_BIT_MACMCTL_START) == DEF_YES) {
        ;
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


   (void)&phy_addr;                                             /* Prevent 'variable unused' compiler warning.         */

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */

    while (DEF_BIT_IS_SET(pdev->MACMCTL, LM3SXXXX_BIT_MACMCTL_START) == DEF_YES) {
        ;
    }

    pdev->MACMTXD =  data;
    pdev->MACMCTL = (reg_addr << 3) | LM3SXXXX_BIT_MACMCTL_WRITE | LM3SXXXX_BIT_MACMCTL_START;

    while (DEF_BIT_IS_SET(pdev->MACMCTL, LM3SXXXX_BIT_MACMCTL_START) == DEF_YES) {
        ;
    }

   *perr = NET_PHY_ERR_NONE;
}


#endif /* NET_IF_ETHER_MODULE_EN */

