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
*                                        Davicom DM9000 (A/B/E)
*
* Filename : net_dev_dm9000.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_DM9000_MODULE
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  <KAL/kal.h>
#include  "net_dev_dm9000.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  DM9000_VENDOR_INFO                       0x90000A46
#define  DM9000A_CHIP_REV                               0x19
#define  DM9000B_CHIP_REV                               0x1A
#define  DM9000E_CHIP_REV                               0x00

#define  DM9000_TX_Q_MAX_NBR                               2    /* DM9000 can queue up to 2 full frames for Tx.         */

#define  MII_REG_RD_WR_TO                              10000    /* MII read write timeout.                              */


/*
*********************************************************************************************************
*                                                MACRO'S
*********************************************************************************************************
*/

#define  SWAP_32(val)                   (((((CPU_INT32U)(val)) & 0xFF000000) >> 24) | \
                                         ((((CPU_INT32U)(val)) & 0x00FF0000) >>  8) | \
                                         ((((CPU_INT32U)(val)) & 0x0000FF00) <<  8) | \
                                         ((((CPU_INT32U)(val)) & 0x000000FF) << 24))

#define  SWAP_16(val)                   (((((CPU_INT16U)(val)) &     0xFF00) >>  8) | \
                                         ((((CPU_INT16U)(val)) &     0x00FF) <<  8))


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
    CPU_INT32U   VendorInfo;
    CPU_INT08U   ChipRev;
    CPU_INT08U   BusWidth;
    CPU_INT08U   ExtPhySupp;
    CPU_ADDR     CmdPinAddr;

    CPU_INT08U   TxBufPtrIxCur;                                 /* See Note #2.                                         */
    CPU_INT08U   TxBufPtrIxComp;                                /* See Note #2.                                         */
    CPU_INT08U  *TxBufPtrs[DM9000_TX_Q_MAX_NBR];                /* Store addr of queued Tx frames (see Note #2).        */
    CPU_INT16U   TxBufSize[DM9000_TX_Q_MAX_NBR];                /* Store size of queued Tx frame.                       */
    CPU_INT08U   TxBufNbrQueued;                                /* Nbr queued Tx frames in DM9000 SRAM.                 */

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
static  void        NetDev_Init               (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_Start              (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_Stop               (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_Rx                 (NET_IF             *pif,
                                               CPU_INT08U        **p_data,
                                               CPU_INT16U         *size,
                                               NET_ERR            *perr);

static  void        NetDev_Tx                 (NET_IF             *pif,
                                               CPU_INT08U         *p_data,
                                               CPU_INT16U          size,
                                               NET_ERR            *perr);


static  void        NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                               CPU_INT08U         *paddr_hw,
                                               CPU_INT08U          addr_hw_len,
                                               NET_ERR            *perr);

static  void        NetDev_AddrMulticastRemove(NET_IF             *pif,
                                               CPU_INT08U         *paddr_hw,
                                               CPU_INT08U          addr_hw_len,
                                               NET_ERR            *perr);

static  void        NetDev_ISR_Handler        (NET_IF             *pif,
                                               NET_DEV_ISR_TYPE    type);

static  void        NetDev_IO_Ctrl            (NET_IF             *pif,
                                               CPU_INT08U          opt,
                                               void               *p_data,
                                               NET_ERR            *perr);

static  void        NetDev_MII_Rd             (NET_IF             *pif,
                                               CPU_INT08U          phy_addr,
                                               CPU_INT08U          reg_addr,
                                               CPU_INT16U         *p_data,
                                               NET_ERR            *perr);

static  void        NetDev_MII_Wr             (NET_IF             *pif,
                                               CPU_INT08U          phy_addr,
                                               CPU_INT08U          reg_addr,
                                               CPU_INT16U          data,
                                               NET_ERR            *perr);

                                                                        /* ----------------- DM9000 FNCT'S --------------------- */
static  CPU_INT08U  NetDev_RegRd              (NET_IF             *pif,
                                               CPU_INT08U          nbr);

static  void        NetDev_RegWr              (NET_IF             *pif,
                                               CPU_INT08U          nbr,
                                               CPU_INT08U          val);

static  void        NetDev_CmdWr              (NET_IF             *pif,
                                               CPU_INT08U          cmd);

static  void        NetDev_DataRd08           (NET_DEV_CFG_ETHER  *pdev_cfg,
                                               NET_DEV_DATA       *pdev_data,
                                               CPU_INT08U         *p_data,
                                               CPU_INT16U          len);

static  void        NetDev_DataRd16           (NET_DEV_CFG_ETHER  *pdev_cfg,
                                               NET_DEV_DATA       *pdev_data,
                                               CPU_INT16U         *p_data,
                                               CPU_INT16U          len);

static  void        NetDev_DataRd32           (NET_DEV_CFG_ETHER  *pdev_cfg,
                                               NET_DEV_DATA       *pdev_data,
                                               CPU_INT32U         *p_data,
                                               CPU_INT16U          len);

static  void        NetDev_DataWr08           (NET_DEV_CFG_ETHER  *pdev_cfg,
                                               NET_DEV_DATA       *pdev_data,
                                               CPU_INT08U         *p_data,
                                               CPU_INT16U          len);

static  void        NetDev_DataWr16           (NET_DEV_CFG_ETHER  *pdev_cfg,
                                               NET_DEV_DATA       *pdev_data,
                                               CPU_INT16U         *p_data,
                                               CPU_INT16U          len);

static  void        NetDev_DataWr32           (NET_DEV_CFG_ETHER  *pdev_cfg,
                                               NET_DEV_DATA       *pdev_data,
                                               CPU_INT32U         *p_data,
                                               CPU_INT16U          len);

static  void        NetDev_Reset              (NET_IF             *pif);


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
                                                                                    /* DM9000 dev API fnct ptrs :       */
const  NET_DEV_API_ETHER  NetDev_API_DM9000 = { NetDev_Init,                        /*   Init/add                       */
                                                NetDev_Start,                       /*   Start                          */
                                                NetDev_Stop,                        /*   Stop                           */
                                                NetDev_Rx,                          /*   Rx                             */
                                                NetDev_Tx,                          /*   Tx                             */
                                                NetDev_AddrMulticastAdd,            /*   Multicast addr add             */
                                                NetDev_AddrMulticastRemove,         /*   Multicast addr remove          */
                                                NetDev_ISR_Handler,                 /*   ISR handler                    */
                                                NetDev_IO_Ctrl,                     /*   I/O ctrl                       */
                                                NetDev_MII_Rd,                      /*   Phy reg rd                     */
                                                NetDev_MII_Wr                       /*   Phy reg wr                     */
                                              };

/*
*********************************************************************************************************
*                                      DM9000 REGISTER DEFINITIONS
*********************************************************************************************************
*/

#define  REG_NCR                         0x00                   /* Network Control Register                             */
#define  REG_NSR                         0x01                   /* Network Status  Register                             */
#define  REG_TCR                         0x02                   /* TX Control Register                                  */
#define  REG_TSR1                        0x03                   /* TX Status  Register 1                                */
#define  REG_TSR2                        0x04                   /* TX Status  Register 2                                */
#define  REG_RCR                         0x05                   /* RX Control Register                                  */
#define  REG_RSR                         0x06                   /* RX Status  Register                                  */
#define  REG_ROCR                        0x07                   /* Receive Overflow Counter Register                    */
#define  REG_BPTR                        0x08                   /* Back Pressure Threshold Register                     */
#define  REG_FCTR                        0x09                   /* Flow Control  Threshold Resgister                    */
#define  REG_FCR                         0x0A                   /* RX Flow Control Register                             */

#define  REG_EPCR                        0x0B                   /* EEPROM & PHY Control Register                        */
#define  REG_EPAR                        0x0C                   /* EEPROM & PHY Address Register                        */
#define  REG_EPDRL                       0x0D                   /* EEPROM & PHY Low  Byte Data Register                 */
#define  REG_EPDRH                       0x0E                   /* EEPROM & PHY High Byte Data Register                 */

#define  REG_WCR                         0x0F                   /* Wake Up Control Register                    (8-bit)  */

#define  REG_PAR0                        0x10                   /* Physical  Address Register, Byte 0                   */
#define  REG_PAR1                        0x11                   /*    "         "        "   , Byte 1                   */
#define  REG_PAR2                        0x12                   /*    "         "        "   , Byte 1                   */
#define  REG_PAR3                        0x13                   /*    "         "        "   , Byte 1                   */
#define  REG_PAR4                        0x14                   /*    "         "        "   , Byte 1                   */
#define  REG_PAR5                        0x15                   /*    "         "        "   , Byte 1                   */
#define  REG_MAR0                        0x16                   /* Multicast Address Register, Byte 0                   */
#define  REG_MAR1                        0x17                   /*    "         "        "   , Byte 1                   */
#define  REG_MAR2                        0x18                   /*    "         "        "   , Byte 2                   */
#define  REG_MAR3                        0x19                   /*    "         "        "   , Byte 3                   */
#define  REG_MAR4                        0x1A                   /*    "         "        "   , Byte 4                   */
#define  REG_MAR5                        0x1B                   /*    "         "        "   , Byte 5                   */
#define  REG_MAR6                        0x1C                   /*    "         "        "   , Byte 6                   */
#define  REG_MAR7                        0x1D                   /*    "         "        "   , Byte 7                   */

#define  REG_GPCR                        0x1E                   /* General Purpose Control Register            (8-bit)  */
#define  REG_GPR                         0x1F                   /* General Purpose Register                             */
#define  REG_TRPAL                       0x22                   /* TX SRAM Read  Pointer Address Low  Byte              */
#define  REG_TRPAH                       0x23                   /* TX SRAM Read  Pointer Address High Byte              */
#define  REG_RWPAL                       0x24                   /* RX SRAM Write Pointer Address Low  Byte              */
#define  REG_RWPAH                       0x25                   /* RX SRAM Write Pointer Address High Byte              */

#define  REG_VIDL                        0x28                   /* Vendor  ID Low  Byte                                 */
#define  REG_VIDH                        0x29                   /* Vendor  ID High Byte                                 */
#define  REG_PIDL                        0x2A                   /* Product ID Low  Byte                                 */
#define  REG_PIDH                        0x2B                   /* Product ID High Byte                                 */
#define  REG_CHIPR                       0x2C                   /* Chip Revision                                        */

#define  REG_TCR2                        0x2D                   /* TX Control Register 2                                */
#define  REG_OCR                         0x2E                   /* Operation Control Register                           */

                                                                /* ----------------- DM9000A/B ONLY ------------------- */
#define  REG_SMCR                        0x2F                   /* Special Mode Control Register                        */
#define  REG_ETXCSR                      0x30                   /* Early Transmit Control/Status Register               */
#define  REG_TCSCR                       0x31                   /* Transmit Check Sum Control Register                  */
#define  REG_RCSCSR                      0x32                   /* Receive  Check Sum Control Status Register           */
#define  REG_MPAR                        0x33                   /* PHY_ PHY Address Register                            */
#define  REG_LEDCR                       0x34                   /* LED Pin Control Register                             */
#define  REG_BUSCR                       0x38                   /* Processor Bus Control Register                       */
#define  REG_INTCR                       0x39                   /* INT Pin Control Register                             */

#define  REG_SCCR                        0x50                   /* System Clock Turn ON Control Register                */
#define  REG_RSCCR                       0x51                   /* Resume System Clock  Control Register                */
                                                                /* ---------------------------------------------------- */

#define  REG_MRCMDX                      0xF0                   /* Memory Data Pre-Fetch Rd Cmd w/o Addr Inc Register   */
#define  REG_MRCMDX1                     0xF1                   /* Memory Data Read  Command with   Addr Inc Register   */
#define  REG_MRCMD                       0xF2                   /* Memory Data Read  Command with   Addr Inc Register   */
#define  REG_MRRH                        0xF4                   /* Memory Data Read  Address Register Low  Byte         */
#define  REG_MRRL                        0xF5                   /* Memory Data Read  Address Register High Byte         */
#define  REG_MWCMDX                      0xF6                   /* Memory Data Write Command w/o    Addr Inc Register   */
#define  REG_MWCMD                       0xF8                   /* Memory Data Write Command with   Addr Inc Register   */
#define  REG_MWRL                        0xFA                   /* Memory Data Write Address Register Low  Byte         */
#define  REG_MWRH                        0xFB                   /* Memory Data Write Address Register High Byte         */

#define  REG_TXPLL                       0xFC                   /* TX Packet Length Low  Byte Register                  */
#define  REG_TXPLH                       0xFD                   /* TX Packet Length High Byte Register                  */

#define  REG_ISR                         0xFE                   /* Interrupt Status Register                            */
#define  REG_IMR                         0xFF                   /* Interrupt Mask   Register                            */


/*
*********************************************************************************************************
*                                     DM9000 REGISTER VALUE DEFINES
*********************************************************************************************************
*/
                                                                /* ----------- NETWORK CONTROL REGISTER BITS ---------- */
#define  REG_NCR_EXT_PHY                  DEF_BIT_07            /* DM9000E only.                                        */
#define  REG_NCR_WAKEEN                   DEF_BIT_06
#define  REG_NCR_FCOL                     DEF_BIT_04
#define  REG_NCR_FDX                      DEF_BIT_03
#define  REG_NCR_LBK_PHY                  DEF_BIT_02
#define  REG_NCR_LBK_MAC                  DEF_BIT_01
#define  REG_NCR_LBK_NORMAL               DEF_BIT_NONE
#define  REG_NCR_RST                      DEF_BIT_00

                                                                /* ----------- NETWORK STATUS REGISTER BITS ----------- */
#define  REG_NSR_SPEED                    DEF_BIT_07
#define  REG_NSR_LINKST                   DEF_BIT_06
#define  REG_NSR_WAKEST                   DEF_BIT_05
#define  REG_NSR_TX2END                   DEF_BIT_03
#define  REG_NSR_TX1END                   DEF_BIT_02
#define  REG_NSR_RXOV                     DEF_BIT_01

                                                                /* ------------- TX CONTROL REGISTER BITS ------------- */
#define  REG_TCR_TJDIS                    DEF_BIT_06
#define  REG_TCR_EXCECM                   DEF_BIT_05
#define  REG_TCR_PAD_DIS2                 DEF_BIT_04
#define  REG_TCR_CRC_DIS2                 DEF_BIT_03
#define  REG_TCR_PAD_DIS1                 DEF_BIT_02
#define  REG_TCR_CRC_DIS1                 DEF_BIT_01
#define  REG_TCR_TXREQ                    DEF_BIT_00

                                                                /* ------------- TX STATUS 1 REGISTER BITS ------------ */
                                                                /* ------------- TX STATUS 2 REGISTER BITS ------------ */
#define  REG_TSR_TJTO                     DEF_BIT_07
#define  REG_TSR_LC                       DEF_BIT_06
#define  REG_TSR_NC                       DEF_BIT_05
#define  REG_TSR_LCOL                     DEF_BIT_04
#define  REG_TSR_COL                      DEF_BIT_03
#define  REG_TSR_EC                       DEF_BIT_02

                                                                /* ------------- RX CONTROL REGISTER BITS ------------- */
#define  REG_RCR_WTDIS                    DEF_BIT_06
#define  REG_RCR_DIS_LONG                 DEF_BIT_05
#define  REG_RCR_DIS_CRC                  DEF_BIT_04
#define  REG_RCR_ALL                      DEF_BIT_03
#define  REG_RCR_RUNT                     DEF_BIT_02
#define  REG_RCR_PRMSC                    DEF_BIT_01
#define  REG_RCR_RXEN                     DEF_BIT_00

                                                                /* -------------- RX STATUS REGISTER BITS ------------- */
#define  REG_RSR_RF                       DEF_BIT_07
#define  REG_RSR_MF                       DEF_BIT_06
#define  REG_RSR_LCS                      DEF_BIT_05
#define  REG_RSR_RWTO                     DEF_BIT_04
#define  REG_RSR_PLE                      DEF_BIT_03
#define  REG_RSR_AE                       DEF_BIT_02
#define  REG_RSR_CE                       DEF_BIT_01
#define  REG_RSR_FOE                      DEF_BIT_00

#define  REG_RSR_ERRORS                  (REG_RSR_PLE | \
                                          REG_RSR_AE  | \
                                          REG_RSR_CE  | \
                                          REG_RSR_FOE)

                                                                /* ----------- RX FLOW CONTROL REGISTER BITS ---------- */
#define  REG_FCR_TXP0                     DEF_BIT_07
#define  REG_FCR_TXPF                     DEF_BIT_06
#define  REG_FCR_TXPEN                    DEF_BIT_05
#define  REG_FCR_BKPA                     DEF_BIT_04
#define  REG_FCR_BKPM                     DEF_BIT_03
#define  REG_FCR_RXPS                     DEF_BIT_02
#define  REG_FCR_RXPCS                    DEF_BIT_01
#define  REG_FCR_FLCE                     DEF_BIT_00

                                                                /* -------- EEPROM & PHY CONTROL REGISTER BITS -------- */
#define  REG_EPCR_REEP                    DEF_BIT_05
#define  REG_EPCR_WEB                     DEF_BIT_04
#define  REG_EPCR_EPOS                    DEF_BIT_03
#define  REG_EPCR_ERPRR                   DEF_BIT_02
#define  REG_EPCR_ERPRW                   DEF_BIT_01
#define  REG_EPCR_ERRE                    DEF_BIT_00

                                                                /* ----------- GENERAL PURPOSE REGISTER BITS ---------- */
#define  REG_GPR_PHYPD                    DEF_BIT_00
#define  REG_GPR_PHYPU                    DEF_BIT_NONE

                                                                /* ----------- INT PIN CONTROL REGISTER BITS ---------- */
#define  REG_INTCR_INT_TYPE               DEF_BIT_01
#define  REG_INTCR_INT_POL                DEF_BIT_00

                                                                /* ---------- INTERRUPT STATUS REGISTER BITS ---------- */
#define  REG_IMR_PAR                      DEF_BIT_07
#define  REG_IMR_LNKCHGI                  DEF_BIT_05
#define  REG_IMR_UDRUNI                   DEF_BIT_04
#define  REG_IMR_ROOI                     DEF_BIT_03
#define  REG_IMR_ROI                      DEF_BIT_02
#define  REG_IMR_PTI                      DEF_BIT_01
#define  REG_IMR_PRI                      DEF_BIT_00

                                                                /* ----------- INTERRUPT MASK REGISTER BITS ----------- */
#define  REG_ISR_IOMODEH                  DEF_BIT_07
#define  REG_ISR_IOMODEL                  DEF_BIT_06            /* DM9000E ONLY.                                        */
#define  REG_ISR_LNKCHG                   DEF_BIT_05
#define  REG_ISR_UDRUN                    DEF_BIT_04
#define  REG_ISR_ROO                      DEF_BIT_03
#define  REG_ISR_ROS                      DEF_BIT_02
#define  REG_ISR_PT                       DEF_BIT_01
#define  REG_ISR_PR                       DEF_BIT_00

#define  REG_ISR_ALL                     (REG_ISR_LNKCHG | \
                                          REG_ISR_UDRUN  | \
                                          REG_ISR_ROO    | \
                                          REG_ISR_ROS    | \
                                          REG_ISR_PT     | \
                                          REG_ISR_PR)


/*
*********************************************************************************************************
*                                             LOCAL MACROS
*
* Note(s) : (1) DM9000_ENTER() and DM9000_EXIT() should be used to "protect" the DM9000 against access by
*               the ISR handler during the reading or transmitting of a packet.  Because these functions
*               use CPU_CRITICAL_ENTER() and CPU_CRITICAL_EXIT() to protect the necessary DM9000A register
*               writes, the declaration CPU_SR_ALLOC() should be included in each function in which these
*               are used.
*********************************************************************************************************
*/

#define  DM9000_ENTER()     {                                               \
                                CPU_CRITICAL_ENTER();                       \
                                NetDev_RegWr(pif, REG_IMR, REG_IMR_PAR);    \
                                CPU_CRITICAL_EXIT();                        \
                            }


#define  DM9000_EXIT()      {                                               \
                                CPU_CRITICAL_ENTER();                       \
                                NetDev_RegWr(pif,                           \
                                             REG_IMR,                       \
                                             REG_IMR_PTI  |                 \
                                             REG_IMR_PRI  |                 \
                                             REG_IMR_ROI  |                 \
                                             REG_IMR_ROOI);                 \
                                CPU_CRITICAL_EXIT();                        \
                            }

/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Allocate device data area
*                   (b) Initialize external clk gating
*                   (c) Initialize external GPIO controller
*                   (d) Initialize external interrupt controller
*                   (e) Obtain DM9000 Cmd pin address bit
*                   (f) Device software reset.
*                   (g) Obtain and configure DM9000 bus configuration
*                   (h) Validate bus configuration
*                   (i) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
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
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (2) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*
*               (3) Enabling the external Phy interface limits the bus width from 32-bit to 16-bit
*                   on the DM9000E.  Phy mode settings NOT affected by software reset.  DM9000E
*                   only supports external MII Phy, RMII not supported by hardware.
*
*               (4) Internal Phy MII bus address equal to 0x01.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    CPU_INT32U          vendor_info;
    CPU_INT08U          chip_rev;
    CPU_INT08U          bus_width;
    CPU_INT08U          phy_type;
    CPU_INT08U          reg_val;
    LIB_ERR             lib_err;


                                                                /* ----------- OBTAIN REFERENCE TO CFGs/BSP ----------- */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
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


    if (pphy_cfg == (void *)0) {                                /* Cfg MAC w/ initial Phy settings.                     */
                                                                /* Phy cfg required for int/ext phy knowledge.          */
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
                                                                /* Enable module clks.                                  */
    pdev_bsp->CfgClk(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* -------- INITIALIZE EXTERNAL GPIO CONTROLLER ------- */
                                                                /* Configure Ethernet Controller GPIO.                  */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------ OBTAIN ADDITIONAL ADDRESS INFORMATION ------- */
    pdev_data->CmdPinAddr = NetDev_CmdPinGet(pif->Nbr, perr);   /* Obtain addr bit value used to drive DM9000 Cmd pin.  */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_Reset(pif);                                          /* Reset dev to ensure int's disabled & normal mode.    */

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
                                                                /* Initialize ext int ctrl'r.                           */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


                                                                /* ---------------- OBTAIN VENDOR INFO ---------------- */
    vendor_info  = NetDev_RegRd(pif, REG_VIDL);
    vendor_info |= NetDev_RegRd(pif, REG_VIDH) << 8;
    vendor_info |= NetDev_RegRd(pif, REG_PIDL) << 16;
    vendor_info |= NetDev_RegRd(pif, REG_PIDH) << 24;
    pdev_data->VendorInfo = vendor_info;

    chip_rev = NetDev_RegRd(pif, REG_ISR);                      /* Obtain pin strapped bus width settings.              */
    pdev_data->ChipRev = chip_rev;

                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */

    switch (chip_rev) {                                         /* Decode bus width & ext phy capabilities.             */
        case DM9000A_CHIP_REV:
        case DM9000B_CHIP_REV:
             pdev_data->ExtPhySupp = DEF_NO;
             bus_width             = chip_rev & REG_ISR_IOMODEH;
             bus_width           >>= 7;                         /* REG_ISR_IOMODEH == bit [7].                          */
             if (bus_width > 0) {
                 pdev_data->BusWidth = 8;                       /* 8-bit.                                               */
             } else {
                 pdev_data->BusWidth = 16;                      /* 16-bit.                                              */
             }
             break;


        case DM9000E_CHIP_REV:
             pdev_data->ExtPhySupp = DEF_YES;
             phy_type              = pphy_cfg->Type;
             bus_width             = chip_rev & (REG_ISR_IOMODEH | REG_ISR_IOMODEL);
             bus_width           >>= 6;                         /* REG_ISR_IOMODE == bit [7:6].                         */
             switch (bus_width) {
                 case 0:                                        /* 16-bit.                                              */
                      pdev_data->BusWidth = 16;
                      break;


                 case 1:                                        /* 16-bit or 32-bit depending on Phy cfg (see Note #3). */
                      if (phy_type ==  NET_PHY_TYPE_INT) {
                          reg_val   =  NetDev_RegRd(pif, REG_NCR);
                          reg_val  &= ~REG_NCR_EXT_PHY;
                          NetDev_RegWr(pif, REG_NCR, reg_val);
                          pdev_data->BusWidth = 32;
                      }

                      if (phy_type ==  NET_PHY_TYPE_EXT) {
                          reg_val   =  NetDev_RegRd(pif, REG_NCR);
                          reg_val  |=  REG_NCR_EXT_PHY;
                          NetDev_RegWr(pif, REG_NCR, reg_val);
                          pdev_data->BusWidth = 16;
                      }
                      break;


                 case 2:                                        /* 8-bit.                                               */
                      pdev_data->BusWidth = 8;
                      break;
             }
             break;


        default:
            *perr = NET_DEV_ERR_UNKNOWN_CHIP_REV;               /* Unknown DM9000 derivative detected.                  */
             return;
    }

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
    if (pdev_cfg->DataBusSizeNbrBits != pdev_data->BusWidth) {  /* Ensure Phy cfg affect on bus width is consistent ... */
       *perr = NET_DEV_ERR_INVALID_CFG;                         /* ... with specified dev cfg.                          */
        return;
    }

    if (pphy_cfg->BusMode != NET_PHY_BUS_MODE_MII) {
       *perr = NET_DEV_ERR_INVALID_BUS_MODE;                    /* Only MII compliant ext Phy supported (see Note #3). */
        return;
    }

    if (pphy_cfg->BusAddr == 0x00) {
       *perr = NET_DEV_ERR_INVALID_PHY_ADDR;                    /* Invalid Phy addr cfg'd.                              */
        return;
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_Start()
*
* Description : (1) Start network interface hardware :
*
*                   (a) Reset device
*                   (b) Initialize transmit semaphore count
*                   (c) Initialize hardware address registers
*                   (d) Initialize SRAM pointer settings
*                   (e) Clear all pending interrupt sources
*                   (f) Enable supported interrupts
*                   (g) Enable the transmitter and receiver
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
*
*               (5) Set hash filter bit MAR7[7] in order to enable the reception of broadcast
*                   frames.  Broadcast frames are a special case of multicast frames since
*                   they have the least significant bit of the most significant byte set
*                   to '1'.
*
*                       E.g: FF-FF-FF-FF-FF-FF
*                             ^-- F == binary 111 '1'.
*
*                       Where multicast frames have physical addresses in the form of :
*                           0x01-00-5E-XX-YY-ZZ
*                              ^- 1 == binary 000 '1'
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U          reg_val;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */



    pdev_data->TxBufPtrIxCur  = 0;                              /* Init Tx comp buf ptr arr next ix.                    */
    pdev_data->TxBufPtrIxComp = 0;                              /* Init Tx comp buf ptr arr      ix.                    */
    pdev_data->TxBufNbrQueued = 0;                              /* Init Tx frame queue nbr.                             */


    NetDev_Reset(pif);                                          /* Reset device.  NCR ext Phy cfg bits not affected.    */


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #2.                                         */
                            DM9000_TX_Q_MAX_NBR,
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
            hw_addr[0] = NetDev_RegRd(pif, REG_PAR0);
            hw_addr[1] = NetDev_RegRd(pif, REG_PAR1);
            hw_addr[2] = NetDev_RegRd(pif, REG_PAR2);
            hw_addr[3] = NetDev_RegRd(pif, REG_PAR3);
            hw_addr[4] = NetDev_RegRd(pif, REG_PAR4);
            hw_addr[5] = NetDev_RegRd(pif, REG_PAR5);

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
        NetDev_RegWr(pif, REG_PAR0, hw_addr[0]);
        NetDev_RegWr(pif, REG_PAR1, hw_addr[1]);
        NetDev_RegWr(pif, REG_PAR2, hw_addr[2]);
        NetDev_RegWr(pif, REG_PAR3, hw_addr[3]);
        NetDev_RegWr(pif, REG_PAR4, hw_addr[4]);
        NetDev_RegWr(pif, REG_PAR5, hw_addr[5]);
    }


                                                                /* ------------------ CFG MULTICAST ------------------- */
#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

    NetDev_RegWr(pif, REG_MAR0, 0x00);                          /* Clr multicast hash reg bits [63:0].                  */
    NetDev_RegWr(pif, REG_MAR1, 0x00);
    NetDev_RegWr(pif, REG_MAR2, 0x00);
    NetDev_RegWr(pif, REG_MAR3, 0x00);
    NetDev_RegWr(pif, REG_MAR4, 0x00);
    NetDev_RegWr(pif, REG_MAR5, 0x00);
    NetDev_RegWr(pif, REG_MAR6, 0x00);
    NetDev_RegWr(pif, REG_MAR7, 0x80);                          /* Set MAR7 bit 7 to enable broadcast Rx (see Note #5). */


                                                                /* ----------- OBTAIN REFERENCE TO PHY CFG ------------ */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;               /* Obtain ptr to the phy cfg struct.                    */

    if ((pphy_cfg->Type        == NET_PHY_TYPE_INT) &&          /* If ext phy capable but internal Phy selected.        */
        (pdev_data->ExtPhySupp == DEF_YES)) {
        NetDev_RegWr(pif, REG_GPR, REG_GPR_PHYPU);              /* Power up the internal Phy.                           */
    }

                                                                /* -------------------- CFG MISC ---------------------- */
    NetDev_RegWr(pif, REG_IMR, REG_IMR_PAR);                    /* Internal SRAM rd wr ptr auto. returns to start.      */


                                                                /* -------------------- CFG INT'S --------------------- */
    NetDev_RegWr(pif, REG_ISR, REG_ISR_ALL);                    /* Clear Rx and Tx int. flags.                          */

    reg_val  = NetDev_RegRd(pif, REG_IMR);
    reg_val |= REG_IMR_PTI |                                    /* Enable packet transmitted int.                       */
               REG_IMR_PRI |                                    /* Enable packet received int.                          */
               REG_IMR_ROI |                                    /* Enable receive overflow int.                         */
               REG_IMR_ROOI;                                    /* Enable receive overflow counter overflow int.        */

    NetDev_RegWr(pif, REG_IMR, reg_val);
                                                                /* ------------------ ENABLE RX & TX ------------------ */
    reg_val = REG_RCR_DIS_LONG |                                /* Discard over sized frames.                           */
              REG_RCR_DIS_CRC  |                                /* Discard frames with CRC errors.                      */
              REG_RCR_RXEN;                                     /* Enable the receiver.                                 */
    NetDev_RegWr(pif, REG_RCR, reg_val);                        /* Enable Rx.  Tx enabled by dflt & cannot be disabled. */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Stop()
*
* Description : (1) Shutdown network interface hardware :
*
*                   (a) Disable the receiver
*                   (b) Disable receive and transmit interrupts
*                   (c) Clear pending interrupt requests
*                   (d) Deallocate pending transmit buffers
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
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U          i;
    NET_ERR             err;

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


                                                                /* ----------------- DISABLE RX & TX ------------------ */
    NetDev_RegWr(pif, REG_RCR, 0);                              /* Disable Rx. Tx enabled by dflt & cannot be disabled. */


                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    NetDev_RegWr(pif, REG_IMR, 0);                              /* Disable all int. src's. (SRAM ptr cfg also reset).   */
    NetDev_RegWr(pif, REG_ISR, REG_ISR_ALL);                    /* Clear all int. flags.                                */


                                                                /* ------------------- FREE TX BUFS ------------------- */
    for (i = 0; i < DM9000_TX_Q_MAX_NBR; i++) {                 /* Dealloc ALL tx bufs (see Note #2a2).                 */
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdev_data->TxBufPtrs[i], &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdev_data->TxBufPtrs[i]  = (CPU_INT08U *)0;
        pdev_data->TxBufSize[i]  = (CPU_INT16U  )0;
    }

                                                                /* ----------- OBTAIN REFERENCE TO PHY CFG ------------ */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;               /* Obtain ptr to the phy cfg struct.                    */

    if ((pphy_cfg->Type        == NET_PHY_TYPE_INT) &&          /* If ext phy capable but internal Phy selected.        */
        (pdev_data->ExtPhySupp == DEF_YES)) {
         NetDev_RegWr(pif, REG_GPR, REG_GPR_PHYPD);             /* Power down the internal Phy.                         */
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
*                           NET_DEV_ERR_FAULT           Generic Rx fault.
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
*                   (a) If a new data area is unavailable, the driver MUST perform the
*                       necessary action(s) in order to drop the frame.
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
*                   (c) It may be necessary to round the number of data reads up, OR perform the
*                       last data read outside of the loop.
*
*               (5) A pointer set equal to pbuf_new and sized according to the required data
*                   read size determined in (4) should be used to read data from the device
*                   into the receive buffer.
*
*               (6) Some devices may interrupt only ONCE for a recieved frame.  The driver MAY need
*                   check if additional frames have been received while processing the current
*                   received frame.  If additional frames have been received, the driver MAY need
*                   to signal the receive task before exiting NetDev_Rx().
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          rdy_word;
    CPU_INT32U          rdy_status_len;
    CPU_INT32U          len_32;
    CPU_INT16U          rdy_status;
    CPU_INT08U          rdy;
    CPU_INT08U          status;
    CPU_INT16U          len_lo;
    CPU_INT16U          len_hi;
    CPU_INT16U          len;
    CPU_INT08U         *pbuf_new;
    NET_ERR             err;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


    DM9000_ENTER();                                             /* Disable device int's.  Rx MUST NOT be interrupted.   */


   (void)NetDev_RegRd(pif, REG_MRCMDX);                         /* Dummy read of rdy byte without auto-increment.       */

    switch (pdev_data->BusWidth) {                              /* Obtain rdy flag.  0x00 no frame, 0x01 frame rdy, ... */
                                                                /* ... any other value MUST force a device reset.       */
        case 8:
             NetDev_DataRd08((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT08U        *)&rdy_word,
                             (CPU_INT16U         ) 1);
             break;


        case 16:
             NetDev_DataRd16((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT16U        *)&rdy_word,
                             (CPU_INT16U         ) 1);
             break;


        case 32:
             NetDev_DataRd32((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT32U        *)&rdy_word,
                             (CPU_INT16U         ) 1);
             break;
    }
    rdy = MEM_VAL_GET_INT08U_LITTLE(&rdy_word);

    switch (rdy) {
        case 0x00:                                              /* Frame not present.                                   */
             DM9000_EXIT();                                     /* Frame Rx complete, allow future int's.               */
            *size   = (CPU_INT08U  )0;
            *p_data = (CPU_INT08U *)0;
            *perr   =  NET_DEV_ERR_RX;
             return;


        case 0x01:                                              /* No error, frame rdy.                                 */
             break;


        default:                                                /* Force device reset.                                  */
             DM9000_EXIT();                                     /* Frame Rx complete, allow future int's.               */
            *size   = (CPU_INT08U  )0;
            *p_data = (CPU_INT08U *)0;
            *perr   =  NET_DEV_ERR_RX;
             NetDev_Stop(pif, &err);
             NetDev_Start(pif, &err);
             return;
    }

    NetDev_CmdWr(pif, REG_MRCMD);                               /* Prepare to read mem with auto-increment.             */

    switch (pdev_data->BusWidth) {                              /* BusWidth validated in NetDev_Init().                 */
        case 8:
             NetDev_DataRd08((NET_DEV_CFG_ETHER *) pdev_cfg,    /* Remove rdy flag    byte from FIFO.                   */
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT08U        *)&rdy,
                             (CPU_INT16U         ) 1);

             NetDev_DataRd08((NET_DEV_CFG_ETHER *) pdev_cfg,    /* Remove status      byte from FIFO.                   */
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT08U        *)&status,
                             (CPU_INT16U         ) 1);

             NetDev_DataRd08((NET_DEV_CFG_ETHER *) pdev_cfg,    /* Remove len low  byte from FIFO.                      */
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT08U        *)&len_lo,
                             (CPU_INT16U         ) 1);

             NetDev_DataRd08((NET_DEV_CFG_ETHER *) pdev_cfg,    /* Remove len high byte from FIFO.                      */
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT08U        *)&len_hi,
                             (CPU_INT16U         ) 1);

             len = (len_hi << 8) | len_lo;                      /* Determine frame len.                                 */
             break;


        case 16:
             NetDev_DataRd16((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT16U        *)&rdy_status,
                             (CPU_INT16U         ) 2);

             NetDev_DataRd16((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT16U        *)&len,
                             (CPU_INT16U         ) 2);

             status = (CPU_INT08U)MEM_VAL_GET_INT16U_BIG(&rdy_status);
             break;


        case 32:
             NetDev_DataRd32((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT32U        *)&rdy_status_len,
                             (CPU_INT16U         ) 4);

             status = (CPU_INT08U) MEM_VAL_GET_INT16U_BIG(&rdy_status_len);
             len_32 = (CPU_INT32U) MEM_VAL_GET_INT32U_LITTLE(&rdy_status_len);
             len    = (CPU_INT16U)(len_32 >> 16);
             break;
    }

   (void)&status;

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

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer (see Note #3a).            */
        *size   = (CPU_INT16U  )0;
        *p_data = (CPU_INT08U *)0;

        switch (pdev_data->BusWidth) {                          /* Discard frame.                                       */
            case 8:
                 NetDev_DataRd08((NET_DEV_CFG_ETHER *)pdev_cfg,
                                 (NET_DEV_DATA      *)pdev_data,
                                 (CPU_INT08U        *)0,
                                 (CPU_INT16U         )len);
                 break;


            case 16:
                 NetDev_DataRd16((NET_DEV_CFG_ETHER *)pdev_cfg,
                                 (NET_DEV_DATA      *)pdev_data,
                                 (CPU_INT16U        *)0,
                                 (CPU_INT16U         )len);
                 break;


            case 32:
                 NetDev_DataRd32((NET_DEV_CFG_ETHER *)pdev_cfg,
                                 (NET_DEV_DATA      *)pdev_data,
                                 (CPU_INT32U        *)0,
                                 (CPU_INT16U         )len);
                 break;
         }

         DM9000_EXIT();                                         /* Frame Rx complete, allow future int's.               */
        *perr = NET_DEV_ERR_RX;
         return;
    }

    switch (pdev_data->BusWidth) {                              /* Read frame data from dev.                            */
        case 8:
             NetDev_DataRd08((NET_DEV_CFG_ETHER *)pdev_cfg,
                             (NET_DEV_DATA      *)pdev_data,
                             (CPU_INT08U        *)pbuf_new,
                             (CPU_INT16U         )len);
             break;


        case 16:
             NetDev_DataRd16((NET_DEV_CFG_ETHER *)pdev_cfg,
                             (NET_DEV_DATA      *)pdev_data,
                             (CPU_INT16U        *)pbuf_new,
                             (CPU_INT16U         )len);
             break;


        case 32:
             NetDev_DataRd32((NET_DEV_CFG_ETHER *)pdev_cfg,
                             (NET_DEV_DATA      *)pdev_data,
                             (CPU_INT32U        *)pbuf_new,
                             (CPU_INT16U         )len);
             break;
    }

                                                                /* -------------- CHK FOR ADD'L RDY FRAMES ------------ */
   (void)NetDev_RegRd(pif, REG_MRCMDX);                         /* Dummy read of rdy byte without auto-increment.       */

    switch (pdev_data->BusWidth) {                              /* Obtain rdy flag.  0x00 no frame, 0x01 frame rdy, ... */
                                                                /* ... any other value MUST force a device reset.       */
        case 8:
             NetDev_DataRd08((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT08U        *)&rdy_word,
                             (CPU_INT16U         ) 1);
             break;


        case 16:
             NetDev_DataRd16((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT16U        *)&rdy_word,
                             (CPU_INT16U         ) 1);
             break;


        case 32:
             NetDev_DataRd32((NET_DEV_CFG_ETHER *) pdev_cfg,
                             (NET_DEV_DATA      *) pdev_data,
                             (CPU_INT32U        *)&rdy_word,
                             (CPU_INT16U         ) 1);
             break;
    }
    rdy = MEM_VAL_GET_INT08U_LITTLE(&rdy_word);


    DM9000_EXIT();                                              /* Frame Rx complete, allow future int's.               */

                                                                /* If first byte of status is 0x01, pkt rx'd.           */
    switch (rdy) {
        case 0x00:                                              /* Frame not present.                                   */
             break;


        case 0x01:                                              /* No error, frame rdy.                                 */
             NetIF_RxTaskSignal(pif->Nbr, &err);                /* Signal Net IF Rx Task.                               */
             break;


        default:                                                /* Force device reset.                                  */
             NetDev_Stop (pif, &err);
             NetDev_Start(pif, &err);
            *perr = NET_DEV_ERR_FAULT;
             return;
    }


   *size   = len;                                               /* Return the size of the recv'd frame.                 */
   *p_data = pbuf_new;                                          /* Return a ptr    to the recv'd frame.                 */

   *perr   = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Write frame to device SRAM.
*                   (b) Issue transmit command.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No Error
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
*
*               (3) TxBufQueuedNbr and TxBufCompPtrIxCur always < DM9000_TX_Q_MAX_NBR since
*                   Tx semaphore limits consecutive access to NetDev_Tx() to DM9000_TX_Q_MAX_NBR (also
*                   see NetDev_Start() Note #2).
*
*               (4) Interrupts MUST remain disabled until TxBufNbrQueued has been incremented.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


    DM9000_ENTER();                                             /* Disable device int's.  Tx MUST NOT be interrupted.   */

    NetDev_CmdWr(pif, REG_MWCMD);                               /* Prepare to read mem with auto-increment.             */

    switch (pdev_data->BusWidth) {                              /* BusWidth validated in NetDev_Init().                 */
        case 8:
             NetDev_DataWr08((NET_DEV_CFG_ETHER *)pdev_cfg,
                             (NET_DEV_DATA      *)pdev_data,
                             (CPU_INT08U        *)p_data,
                             (CPU_INT16U         )size);
             break;


        case 16:
             NetDev_DataWr16((NET_DEV_CFG_ETHER *)pdev_cfg,
                             (NET_DEV_DATA      *)pdev_data,
                             (CPU_INT16U        *)p_data,
                             (CPU_INT16U         )size);
             break;


        case 32:
             NetDev_DataWr32((NET_DEV_CFG_ETHER *)pdev_cfg,
                             (NET_DEV_DATA      *)pdev_data,
                             (CPU_INT32U        *)p_data,
                             (CPU_INT16U         )size);
             break;
    }

                                                                /* -------------------- TX FRAME ---------------------- */
    if (pdev_data->TxBufNbrQueued == 0) {                       /* No queued frames in mem.  Rdy to Tx.                 */
        NetDev_RegWr(pif, REG_TXPLL,  size       & 0xFF);       /* Write low  byte of len into reg.                     */
        NetDev_RegWr(pif, REG_TXPLH, (size >> 8) & 0xFF);       /* Write high byte of len into reg.                     */
        NetDev_RegWr(pif, REG_TCR,    REG_TCR_TXREQ);           /* Issue TX req cmd.                                    */
    }

                                                                /* ----------- STORE TX FRAME PTR AND SIZE ------------ */
    pdev_data->TxBufNbrQueued++;                                /* Frame queued (see Notes #2, #3, & #4).               */
    pdev_data->TxBufPtrs[pdev_data->TxBufPtrIxCur] =  p_data;
    pdev_data->TxBufSize[pdev_data->TxBufPtrIxCur] =  size;
    pdev_data->TxBufPtrIxCur                       = (pdev_data->TxBufPtrIxCur + 1) % DM9000_TX_Q_MAX_NBR;

    DM9000_EXIT();                                              /* Frame Tx complete, allow future int's.               */

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
                                       NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_DATA  *pdev_data;
    CPU_INT32U     crc;
    CPU_INT08U     bit_sel;
    CPU_INT08U     hash;
    CPU_INT08U    *paddr_hash_ctrs;
    CPU_INT08U     reg_val;
    CPU_BOOLEAN    reg_sel;
    CPU_SR_ALLOC();


                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without compliment.                       */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    hash     = (crc  >> 0) & 0x3F;                              /* Obtain hash from 6 least significant bits of CRC.    */
    bit_sel  = (hash >> 0) & 0x07;                              /* Obtain hash bits [2:0] for reg bit sel.              */
    bit_sel  = (1 << bit_sel);                                  /* Compute hash bit value.                              */
    reg_sel  = (hash >> 3) & 0x07;                              /* Obtain hash bits [5:3] for reg sel.                  */
    reg_sel +=  REG_MAR0;                                       /* DM9000 MAR base reg nbr + reg_sel offset.            */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */


    DM9000_ENTER();                                             /* Disable device int's.                                */
    reg_val  = NetDev_RegRd(pif, reg_sel);
    reg_val |= bit_sel;
    NetDev_RegWr(pif, reg_sel, reg_val);                        /* Set multicast hash reg bit.                          */
    DM9000_EXIT();                                              /* Enable device int's.                                 */

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
*               (3) Do NOT clear hash filter bit MAR7[7] in order to continue receiving broadcast
*                   frames.  Broadcast frames are a special case of multicast frames since
*                   they have the least significant bit of the most significant byte set
*                   to '1'.
*
*                       E.g: FF-FF-FF-FF-FF-FF
*                             ^-- F == binary 111 '1'.
*
*                       Where multicast frames have physical addresses in the form of :
*                           0x01-00-5E-XX-YY-ZZ
*                              ^- 1 == binary 000 '1'
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_DATA  *pdev_data;
    CPU_INT32U     crc;
    CPU_INT08U     bit_sel;
    CPU_INT08U     hash;
    CPU_INT08U    *paddr_hash_ctrs;
    CPU_INT08U     reg_val;
    CPU_BOOLEAN    reg_sel;
    CPU_SR_ALLOC();


                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without compliment.                       */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    hash     = (crc  >> 0) & 0x3F;                              /* Obtain hash from 6 least significant bits of CRC.    */
    bit_sel  = (hash >> 0) & 0x07;                              /* Obtain hash bits [2:0] for reg bit sel.              */
    bit_sel  = (1 << bit_sel);                                  /* Compute hash bit value.                              */
    reg_sel  = (hash >> 3) & 0x07;                              /* Obtain hash bits [5:3] for reg sel.                  */
    reg_sel +=  REG_MAR0;                                       /* DM9000 MAR base reg nbr + reg_sel offset.            */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1) {                                 /* If hash bit reference ctr not zero.                  */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0;                                        /* Zero hash bit references remaining.                  */

    if ((reg_sel == REG_MAR7) &&                                /* Do NOT clr the broadcast hash bit (see Note #3).     */
        (bit_sel == 7)) {
       *perr = NET_DEV_ERR_NONE;
        return;
    }

    DM9000_ENTER();                                             /* Disable device int's.                                */
    reg_val  =  NetDev_RegRd(pif, reg_sel);
    reg_val &= ~bit_sel;
    NetDev_RegWr(pif, reg_sel, reg_val);                        /* Set multicast hash reg bit.                          */
    DM9000_EXIT();                                              /* Enable device int's.                                 */

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
#endif

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
    NET_PHY_CFG_ETHER   *pphy_cfg;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;
    CPU_INT08U           reg_val;

                                                                /* ----------- OBTAIN REFERENCE TO PHY CFG ------------ */
    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;               /* Obtain ptr to the Phy cfg.                           */

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
             if (pphy_cfg->Type == NET_PHY_TYPE_EXT) {          /* Adjust NCR FDX bit for ext Phy ONLY.                 */
                 plink_state = (NET_DEV_LINK_ETHER *)p_data;

                 duplex = NET_PHY_DUPLEX_UNKNOWN;
                 if (plink_state->Duplex != duplex) {
                     switch (plink_state->Duplex) {             /* Update ext Phy MAC duplex setting.                   */
                        case NET_PHY_DUPLEX_FULL:
                             reg_val  = NetDev_RegRd(pif, REG_NCR);
                             reg_val |= REG_NCR_FDX;
                             NetDev_RegWr(pif, REG_NCR, reg_val);
                             break;

                        case NET_PHY_DUPLEX_HALF:
                             reg_val  =  NetDev_RegRd(pif, REG_NCR);
                             reg_val &= ~REG_NCR_FDX;
                             NetDev_RegWr(pif, REG_NCR, reg_val);
                             break;

                        default:
                             break;
                     }
                 }

                 spd = NET_PHY_SPD_0;
                 if (plink_state->Spd != spd) {
                     switch (plink_state->Spd) {                /* DM9000 does NOT have MAC setting for spd.            */
                        case NET_PHY_SPD_10:
                        case NET_PHY_SPD_100:
                        case NET_PHY_SPD_1000:
                             break;

                        default:
                             break;
                     }
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
* Note(s)     : (1) The MII control logic forces Phy address bits [4:2] to 0.  This implies that
*                   all external Phy devices MUST be physically wired such that the corresponding
*                   MII bus address is set to 1, 2 or 3. See Note #3.
*
*               (3) Phy address 0x00 corresponds to EEPROM instead of Phy register writes.
*********************************************************************************************************
*/

static  void  NetDev_MII_Rd (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U  *p_data,
                             NET_ERR     *perr)
{
    CPU_INT32U  timeout;
    CPU_INT08U  reg_val;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

    reg_val = reg_addr | ((phy_addr & 0x03) << 6);              /* Compute Phy reg addr & phy bus addr.                 */

                                                                /* --------------- PERFORM PHY REG RD ----------------- */
    DM9000_ENTER();                                             /* Disable device int's during Phy operation.           */

    NetDev_RegWr(pif, REG_EPAR,  reg_val);                      /* Cfg MII logic for Phy reg write.                     */

    NetDev_RegWr(pif,                                           /* Issue read cmd.                                      */
                 REG_EPCR,
                 REG_EPCR_EPOS |
                 REG_EPCR_ERPRR);

                                                                /* Wait for operation to complete.                      */
    timeout = MII_REG_RD_WR_TO;

    do {
        reg_val  = NetDev_RegRd(pif, REG_EPCR);
        reg_val &= REG_EPCR_ERRE;
        timeout--;
    } while ((reg_val > 0) && (timeout > 0));

    NetDev_RegWr(pif, REG_EPCR, REG_EPCR_EPOS);                 /* Write "8" into EPCR to clear read cmd.               */

   *p_data  = NetDev_RegRd(pif, REG_EPDRL);                     /* Read low  byte.                                      */
   *p_data |= NetDev_RegRd(pif, REG_EPDRH) << 8;                /* Read high byte.                                      */

    DM9000_EXIT();                                              /* Phy reg operation complete. Allow future int's.      */

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
* Note(s)     : (1) The MII control logic forces Phy address bits [4:2] to 0.  This implies that
*                   all external Phy devices MUST be physically wired such that the corresponding
*                   MII bus address is set to 1, 2 or 3. See Note #3.
*
*               (2) The device internal Phy address is 0x01 and cannot be changed.
*
*               (3) Phy address 0x00 corresponds to EEPROM instead of Phy register writes.
*********************************************************************************************************
*/

static  void  NetDev_MII_Wr (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   data,
                             NET_ERR     *perr)
{
    CPU_INT32U  timeout;
    CPU_INT08U  data_high;
    CPU_INT08U  data_low;
    CPU_INT08U  reg_val;
    CPU_SR_ALLOC();


    reg_val   =  reg_addr | ((phy_addr & 0x03) << 6);           /* Compute Phy reg addr & phy bus addr.                 */
    data_high = (CPU_INT08U)(data >>   8);
    data_low  = (CPU_INT08U)(data & 0x0F);

                                                                /* --------------- PERFORM PHY REG WR ----------------- */
    DM9000_ENTER();                                             /* Disable device int's during Phy operation.           */

    NetDev_RegWr(pif, REG_EPAR,  reg_val);                      /* Cfg MII logic for Phy reg write.                     */
    NetDev_RegWr(pif, REG_EPDRH, data_high);                    /* Write high byte to reg.                              */
    NetDev_RegWr(pif, REG_EPDRL, data_low);                     /* Write low  byte to reg.                              */

    NetDev_RegWr(pif,                                           /* Issue write cmd.                                     */
                 REG_EPCR,
                 REG_EPCR_EPOS |
                 REG_EPCR_ERPRW);

                                                                /* Wait for operation to complete.                      */
    timeout = MII_REG_RD_WR_TO;

    do {
        reg_val  = NetDev_RegRd(pif, REG_EPCR);
        reg_val &= REG_EPCR_ERRE;
        timeout--;
    } while ((reg_val > 0) && (timeout > 0));

    NetDev_RegWr(pif, REG_EPCR, REG_EPCR_EPOS);                 /* Write "8" into EPCR to clear write cmd.              */

    DM9000_EXIT();                                              /* Phy reg operation complete. Allow future int's.      */

   *perr = NET_PHY_ERR_NONE;
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
*
*               (2) In the case of an interrupt occurring prior to Network Protocol Stack initialization,
*                   the device driver should ensure that the interrupt source is cleared in order
*                   to prevent the potential for an infinite interrupt loop during system initialization.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_DATA  *pdev_data;
    CPU_INT16U     size;
    CPU_INT08U    *p_data;
    CPU_INT08U     int_status;
    NET_ERR        err;
    CPU_SR_ALLOC();


   (void)&type;                                                 /* Prevent compiler warning if arg 'type' not req'd.    */

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();                                       /* Prevent nested interrupts!                           */


                                                                /* --------------- DETERMINE ISR TYPE ----------------- */
    int_status    = NetDev_RegRd(pif, REG_ISR) & 0x3F;          /* Read int. status reg.                                */


                                                                /* ------------------ HANDLE RX ISRs ------------------ */
    if (DEF_BIT_IS_SET(int_status, REG_ISR_PR)) {               /* Check if packet recv'd.                              */
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF Rx Task.                               */
        NetDev_RegWr(pif, REG_ISR, REG_ISR_PR);                 /* Clear packet recv'd int. src.                        */
    }

                                                                /* ------------------ HANDLE TX ISRs ------------------ */
    if (DEF_BIT_IS_SET(int_status, REG_ISR_PT)) {               /* Check if Tx complete int.                            */
        p_data = (CPU_INT08U *)pdev_data->TxBufPtrs[pdev_data->TxBufPtrIxComp];
        pdev_data->TxBufPtrIxComp = (pdev_data->TxBufPtrIxComp + 1) % DM9000_TX_Q_MAX_NBR;
        if (pdev_data->TxBufNbrQueued > 1) {
            size = pdev_data->TxBufSize[pdev_data->TxBufPtrIxComp];
            NetDev_RegWr(pif, REG_TXPLL,  size       & 0xFF);   /* Write low  byte of len into reg.                     */
            NetDev_RegWr(pif, REG_TXPLH, (size >> 8) & 0xFF);   /* Write high byte of len into reg.                     */
            NetDev_RegWr(pif, REG_TCR,    REG_TCR_TXREQ);       /* Issue TX req cmd.                                    */
            pdev_data->TxBufNbrQueued--;
        } else {
            pdev_data->TxBufNbrQueued = 0;
        }
        NetIF_TxDeallocTaskPost(p_data, &err);                  /* Queue transmitted frame for deallocation.            */
        NetIF_DevTxRdySignal(pif);                              /* Signal Net IF that Tx rsrc's have become available.  */
        NetDev_RegWr(pif, REG_ISR, REG_ISR_PT);                 /* Clear packet transmitted int.                        */
    }

                                                                /* ---------------- HANDLE MISC ISRs ------------------ */
    if (DEF_BIT_IS_SET(int_status, REG_ISR_LNKCHG)) {           /* Check for link change int.                           */
        NetDev_RegWr(pif, REG_ISR, REG_ISR_LNKCHG);             /* Clear link change interrupt.                         */
    }

    if (DEF_BIT_IS_SET(int_status, REG_ISR_ROS)) {              /* Check for recv ctr overflow int.                     */
        NetDev_RegWr(pif, REG_ISR, REG_ISR_ROS);                /* Clear recv ctr overflow int.                         */
    }

    if (DEF_BIT_IS_SET(int_status, REG_ISR_ROO)) {              /* Check for recv overflow int.                         */
        NetDev_RegWr(pif, REG_ISR, REG_ISR_ROO);                /* Clear recv overflow int.                             */
    }

    CPU_CRITICAL_EXIT();                                        /* Restore previous CPU level int. mask.                */
}


/*
*********************************************************************************************************
*                                           NetDev_RegRd()
*
* Description : Read DM9000 register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               nbr         Register number.
*
* Return(s)   : 32-bit return value (see Note #1).
*
* Caller(s)   : Various dev drv fncts.
*
* Note(s)     : (1) DM9000 register reads and writes are alway 8 bit.
*********************************************************************************************************
*/

static  CPU_INT08U  NetDev_RegRd (NET_IF     *pif,
                                  CPU_INT08U  nbr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U         *port_ix;
    CPU_INT08U         *port_data;
    CPU_INT08U          ret_val;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------- OBTAIN REFERENCE TO DM9000 REGISTERS ------- */
    port_ix   = (CPU_INT08U *)pdev_cfg->BaseAddr;               /* Determine addr of DM9000 IX   port.                  */
    port_data =  port_ix + pdev_data->CmdPinAddr;               /* Determine addr of DM9000 DATA port.                  */


    CPU_CRITICAL_ENTER();

   *port_ix =  nbr;                                             /* Cfg    reg nbr by writing the DM9000 INDEX port      */
    ret_val = *port_data;                                       /* Obtain reg val by reading the DM9000 DATA  port.     */

    CPU_CRITICAL_EXIT();

    return (ret_val);
}


/*
*********************************************************************************************************
*                                           NetDev_RegWr()
*
* Description : Read DM9000 register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               nbr         Register number.
*
*               val         8-bit value to write to specified register.
*
* Return(s)   : none.
*
* Caller(s)   : Various dev drv fncts.
*
* Note(s)     : (1) DM9000 register reads and writes are alway 8 bit.
*********************************************************************************************************
*/

static  void  NetDev_RegWr (NET_IF     *pif,
                            CPU_INT08U  nbr,
                            CPU_INT08U  val)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U         *port_ix;
    CPU_INT08U         *port_data;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


                                                                /* ------- OBTAIN REFERENCE TO DM9000 REGISTERS ------- */
    port_ix   = (CPU_INT08U *)pdev_cfg->BaseAddr;               /* Determine addr of DM9000 IX   port.                  */
    port_data =  port_ix + pdev_data->CmdPinAddr;               /* Determine addr of DM9000 DATA port.                  */


    CPU_CRITICAL_ENTER();

   *port_ix   =  nbr;                                           /* Cfg   reg nbr by writing the DM9000 INDEX port       */
   *port_data =  val;                                           /* Write reg val by writing the DM9000 DATA  port.      */

    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                            NetDev_CmdWr()
*
* Description : Write a command to the DM9000 index register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               cmd         Command number.
*
* Return(s)   : none.
*
* Caller(s)   : Various dev drv fncts.
*
* Note(s)     : (1) DM9000 writes to the device index port are always 8-bit.
*********************************************************************************************************
*/

static  void  NetDev_CmdWr (NET_IF     *pif,
                            CPU_INT08U  cmd)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT08U         *port_ix;
    CPU_SR_ALLOC();


                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    CPU_CRITICAL_ENTER();
                                                                /* ------- OBTAIN REFERENCE TO DM9000 REGISTERS ------- */
    port_ix  = (CPU_INT08U *)pdev_cfg->BaseAddr;                /* Determine addr of DM9000 IX port.                    */


   *port_ix  =  cmd;                                            /* Issue cmd by writing DM9000 IX port.                 */

    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                          NetDev_DataRd08()
*
* Description : Read 8-bit frame data from the device.
*
* Argument(s) : pdev_cfg    Pointer to device configuration.
*               --------    Argument validated in NetDev_Rx().
*
*               pdev_data   Pointer to device data area.
*               ---------   Argument validated in NetDev_Rx().
*
*               p_data      8-bit pointer to storage area of sufficient size, OR
*                           NULL  pointer to discard frame data.
*
*               len         Number of octets to read.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_DataRd08 (NET_DEV_CFG_ETHER  *pdev_cfg,
                               NET_DEV_DATA       *pdev_data,
                               CPU_INT08U         *p_data,
                               CPU_INT16U          len)
{
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
              CPU_INT16U   nbr_rds;
              CPU_INT16U   i;
#else
              CPU_DATA     nbr_rds;
              CPU_DATA     i;
#endif
    volatile  CPU_INT08U  *pdata_port_08;
              CPU_INT08U   val;


    pdata_port_08 = (CPU_INT08U *)(pdev_cfg->BaseAddr | pdev_data->CmdPinAddr);

                                                                /* Calc nbr  8-bits rds to rd 'len' data.               */
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
    nbr_rds = (CPU_INT16U)len;
#else
    nbr_rds = (CPU_DATA  )len;
#endif

    if (p_data == (CPU_INT08U *)0) {                            /* If no rd data buf avail, ...                         */
        for (i = 0; i < nbr_rds; i++) {
            val = *pdata_port_08;                               /* ...   rd data out of dev ...                         */
           (void) &val;                                         /* ... & discard.                                       */
        }

    } else {                                                    /* Else  rd data into avail data buf.                   */
        for (i = 0; i < nbr_rds; i++) {
            val      = *pdata_port_08;
           *p_data++ =  val;
        }
    }
}


/*
*********************************************************************************************************
*                                          NetDev_DataRd16()
*
* Description : Read 16-bit frame data from the device.
*
* Argument(s) : pdev_cfg    Pointer to device configuration.
*               --------    Argument validated in NetDev_Rx().
*
*               pdev_data   Pointer to device data area.
*               ---------   Argument validated in NetDev_Rx().
*
*               p_data      16-bit pointer to storage area of sufficient size, OR
*                           NULL   pointer to discard frame data.
*
*               len         Number of octets to read.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_DataRd16 (NET_DEV_CFG_ETHER  *pdev_cfg,
                               NET_DEV_DATA       *pdev_data,
                               CPU_INT16U         *p_data,
                               CPU_INT16U          len)
{
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
              CPU_INT16U    nbr_rds;
              CPU_INT16U    i;
#else
              CPU_DATA      nbr_rds;
              CPU_DATA      i;
#endif
    volatile  CPU_INT16U   *pdata_port_16;
              CPU_INT16U    val;
              CPU_BOOLEAN   data_octet_swap;


    pdata_port_16 = (CPU_INT16U *)(pdev_cfg->BaseAddr | pdev_data->CmdPinAddr);

                                                                /* Calc nbr 16-bits rds to rd 'len' data.               */
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
    nbr_rds = (CPU_INT16U)((len + (CPU_WORD_SIZE_16 - 1)) / CPU_WORD_SIZE_16);
#else
    nbr_rds = (CPU_DATA  )((len + (CPU_WORD_SIZE_16 - 1)) / CPU_WORD_SIZE_16);
#endif

    if (p_data == (CPU_INT16U *)0) {                            /* If no rd data buf avail, ...                         */
        for (i = 0; i < nbr_rds; i++) {
            val = *pdata_port_16;                               /* ...   rd data out of dev ...                         */
           (void) &val;                                         /* ... & discard.                                       */
        }

    } else {                                                    /* Else  rd data into avail data buf.                   */
        data_octet_swap = DEF_BIT_IS_SET(pdev_cfg->Flags, NET_DEV_CFG_FLAG_SWAP_OCTETS);

        if (data_octet_swap == DEF_YES) {                       /* If req'd,         ...                                */
            for (i = 0; i < nbr_rds; i++) {
                val      = *pdata_port_16;
                val      =  SWAP_16(val);                       /* ... swap dev data ...                                */
               *p_data++ =  val;                                /* ... before wr'ing to data buf.                       */
            }
        } else {
            for (i = 0; i < nbr_rds; i++) {
                val      = *pdata_port_16;
               *p_data++ =  val;
            }
        }
    }
}


/*
*********************************************************************************************************
*                                          NetDev_DataRd32()
*
* Description : Read 32-bit frame data from the device.
*
* Argument(s) : pdev_cfg    Pointer to device configuration.
*               --------    Argument validated in NetDev_Rx().
*
*               pdev_data   Pointer to device data area.
*               ---------   Argument validated in NetDev_Rx().
*
*               p_data      16-bit pointer to storage area of sufficient size, OR
*                           NULL   pointer to discard frame data.
*
*               len         Number of octets to read.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_DataRd32 (NET_DEV_CFG_ETHER  *pdev_cfg,
                               NET_DEV_DATA       *pdev_data,
                               CPU_INT32U         *p_data,
                               CPU_INT16U          len)
{
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
              CPU_INT16U    nbr_rds;
              CPU_INT16U    i;
#else
              CPU_DATA      nbr_rds;
              CPU_DATA      i;
#endif
    volatile  CPU_INT32U   *pdata_port_32;
              CPU_INT32U    val;
              CPU_BOOLEAN   data_octet_swap;


    pdata_port_32 = (CPU_INT32U *)(pdev_cfg->BaseAddr | pdev_data->CmdPinAddr);

                                                                /* Calc nbr 32-bits rds to rd 'len' data.               */
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
    nbr_rds = (CPU_INT16U)((len + (CPU_WORD_SIZE_32 - 1)) / CPU_WORD_SIZE_32);
#else
    nbr_rds = (CPU_DATA  )((len + (CPU_WORD_SIZE_32 - 1)) / CPU_WORD_SIZE_32);
#endif

    if (p_data == (CPU_INT32U *)0) {                            /* If no rd data buf avail, ...                         */
        for (i = 0; i < nbr_rds; i++) {
            val = *pdata_port_32;                               /* ...   rd data out of dev ...                         */
           (void) &val;                                         /* ... & discard.                                       */
        }

    } else {                                                    /* Else  rd data into avail data buf.                   */
        data_octet_swap = DEF_BIT_IS_SET(pdev_cfg->Flags, NET_DEV_CFG_FLAG_SWAP_OCTETS);

        if (data_octet_swap == DEF_YES) {                       /* If req'd,         ...                                */
            for (i = 0; i < nbr_rds; i++) {
                val      = *pdata_port_32;
                val      =  SWAP_32(val);                       /* ... swap dev data ...                                */
               *p_data++ =  val;                                /* ... before wr'ing to data buf.                       */
            }
        } else {
            for (i = 0; i < nbr_rds; i++) {
                val      = *pdata_port_32;
               *p_data++ =  val;
            }
        }
    }
}


/*
*********************************************************************************************************
*                                          NetDev_DataWr08()
*
* Description : Write 8-bit frame data to the device.
*
* Argument(s) : pdev_cfg    Pointer to device configuration.
*               --------    Argument validated in NetDev_Tx().
*
*               pdev_data   Pointer to device data area.
*               ---------   Argument validated in NetDev_Tx().
*
*               p_data      8-bit pointer to data.
*
*               len         Number of octets to write.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_DataWr08 (NET_DEV_CFG_ETHER  *pdev_cfg,
                               NET_DEV_DATA       *pdev_data,
                               CPU_INT08U         *p_data,
                               CPU_INT16U          len)
{
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
              CPU_INT16U   nbr_wrs;
              CPU_INT16U   i;
#else
              CPU_DATA     nbr_wrs;
              CPU_DATA     i;
#endif
    volatile  CPU_INT08U  *pdata_port_08;
              CPU_INT08U   val;


    pdata_port_08 = (CPU_INT08U *)(pdev_cfg->BaseAddr | pdev_data->CmdPinAddr);

                                                                /* Calc nbr  8-bits wrs to wr 'len' data.               */
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
    nbr_wrs = (CPU_INT16U)len;
#else
    nbr_wrs = (CPU_DATA  )len;
#endif

    for (i = 0; i < nbr_wrs; i++) {
        val           = *p_data++;
       *pdata_port_08 =  val;
    }
}


/*
*********************************************************************************************************
*                                          NetDev_DataWr16()
*
* Description : Write 8-bit frame data to the device.
*
* Argument(s) : pdev_cfg    Pointer to device configuration.
*               --------    Argument validated in NetDev_Tx().
*
*               pdev_data   Pointer to device data area.
*               ---------   Argument validated in NetDev_Tx().
*
*               p_data      16-bit pointer to data.
*
*               len         Number of octets to write.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_DataWr16 (NET_DEV_CFG_ETHER  *pdev_cfg,
                               NET_DEV_DATA       *pdev_data,
                               CPU_INT16U         *p_data,
                               CPU_INT16U          len)
{
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
              CPU_INT16U    nbr_wrs;
              CPU_INT16U    i;
#else
              CPU_DATA      nbr_wrs;
              CPU_DATA      i;
#endif
    volatile  CPU_INT16U   *pdata_port_16;
              CPU_INT16U    val;
              CPU_BOOLEAN   data_octet_swap;


    pdata_port_16 = (CPU_INT16U *)(pdev_cfg->BaseAddr | pdev_data->CmdPinAddr);

                                                                /* Calc nbr 16-bits wrs to wr 'len' data.               */
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
    nbr_wrs = (CPU_INT16U)((len + (CPU_WORD_SIZE_16 - 1)) / CPU_WORD_SIZE_16);
#else
    nbr_wrs = (CPU_DATA  )((len + (CPU_WORD_SIZE_16 - 1)) / CPU_WORD_SIZE_16);
#endif

    data_octet_swap = DEF_BIT_IS_SET(pdev_cfg->Flags, NET_DEV_CFG_FLAG_SWAP_OCTETS);

    if (data_octet_swap == DEF_YES) {                           /* If req'd,     ...                                    */
        for (i = 0; i < nbr_wrs; i++) {
            val           = *p_data++;
            val           =  SWAP_16(val);                      /* ... swap data ...                                    */
           *pdata_port_16 =  val;                               /* ... before wr'ing to dev.                            */
        }
    } else {
        for (i = 0; i < nbr_wrs; i++) {
            val           = *p_data++;
           *pdata_port_16 =  val;
        }
    }
}


/*
*********************************************************************************************************
*                                          NetDev_DataWr32()
*
* Description : Write 32-bit frame data to the device.
*
* Argument(s) : pdev_cfg    Pointer to device configuration.
*               --------    Argument validated in NetDev_Tx().
*
*               pdev_data   Pointer to device data area.
*               ---------   Argument validated in NetDev_Tx().
*
*               p_data      32-bit pointer to data.
*
*               len         Number of octets to write.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_DataWr32 (NET_DEV_CFG_ETHER  *pdev_cfg,
                               NET_DEV_DATA       *pdev_data,
                               CPU_INT32U         *p_data,
                               CPU_INT16U          len)
{
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
              CPU_INT16U    nbr_wrs;
              CPU_INT16U    i;
#else
              CPU_DATA      nbr_wrs;
              CPU_DATA      i;
#endif
    volatile  CPU_INT32U   *pdata_port_32;
              CPU_INT32U    val;
              CPU_BOOLEAN   data_octet_swap;


    pdata_port_32 = (CPU_INT32U *)(pdev_cfg->BaseAddr | pdev_data->CmdPinAddr);

                                                                /* Calc nbr 32-bits wrs to wr 'len' data.               */
#if (CPU_CFG_DATA_SIZE == CPU_WORD_SIZE_08)
    nbr_wrs = (CPU_INT16U)((len + (CPU_WORD_SIZE_32 - 1)) / CPU_WORD_SIZE_32);
#else
    nbr_wrs = (CPU_DATA  )((len + (CPU_WORD_SIZE_32 - 1)) / CPU_WORD_SIZE_32);
#endif

    data_octet_swap = DEF_BIT_IS_SET(pdev_cfg->Flags, NET_DEV_CFG_FLAG_SWAP_OCTETS);
    if (data_octet_swap == DEF_YES) {                           /* If req'd,     ...                                    */
        for (i = 0; i < nbr_wrs; i++) {
            val           = *p_data++;
            val           =  SWAP_32(val);                      /* ... swap data ...                                    */
           *pdata_port_32 =  val;                               /* ... before wr'ing to dev.                            */
        }
    } else {
        for (i = 0; i < nbr_wrs; i++) {
            val           = *p_data++;
           *pdata_port_32 =  val;
        }
    }
}


/*
*********************************************************************************************************
*                                           NetDev_Reset()
*
* Description : Software reset the DM9000.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
* Return(s)   : none.
*
* Caller(s)   : Various dev drv fncts.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Reset (NET_IF  *pif)
{
    NetDev_RegWr(pif,
                 REG_NCR,
                 REG_NCR_RST |
                 REG_NCR_LBK_MAC);

    KAL_Dly(2);

    NetDev_RegWr(pif,
                 REG_NCR,
                 REG_NCR_RST |
                 REG_NCR_LBK_MAC);

    KAL_Dly(2);

    NetDev_RegWr(pif,
                 REG_NCR,
                 REG_NCR_LBK_NORMAL);
}


#endif /* NET_IF_ETHER_MODULE_EN */

