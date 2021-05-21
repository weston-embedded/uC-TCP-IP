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
*                                          Renesas RZT1-Ether
*
* Filename : net_dev_rzt1_ether.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_RZT1_ETHER_MODULE

#include  "net_dev_rzt1_ether.h"
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  <KAL/kal.h>
#include  <cpu_cache.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  NET_DEV_ETHSW_OFFSET           0x14
#define  NET_DEV_MAC_HDR_LEN            14u

#define  NET_DEV_RX_PKT                 1u
#define  NET_DEV_TX_PKT                 2u

#define  NET_DEV_HW_TASK                0x03
#define  NET_DEV_HW_READY               0x03
#define  NET_DEV_HW_INIT                0x8004

#define  NET_DEV_DATA_RAM_ADDR_START    0x20000000u
#define  NET_DEV_DATA_RAM_ADDR_END      0x2007FFFFu


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

#define  EMACRST_RESET               DEF_BIT_00

#define  MII_CTRL_MODE_MII           0x00000000
#define  MII_CTRL_MODE_RMII          0x00000005
#define  MII_CTRL_MODE_FULLD         DEF_BIT_08


#define  MACSEL_MODE_0               0x00000000;
#define  MACSEL_MODE_1               0x00000001;
#define  MACSEL_MODE_5               0x00000005;

#define  ETHSFTRST_CATRST            DEF_BIT_00
#define  ETHSFTRST_SWRST             DEF_BIT_01
#define  ETHSFTRST_PHYRST            DEF_BIT_02
#define  ETHSFTRST_PHYRST2           DEF_BIT_03
#define  ETHSFTRST_MIIRST            DEF_BIT_04


#define  HW_STATUS_READY             DEF_BIT_31

#define  GMAC_MODE_ETHMODE           DEF_BIT_31
#define  GMAC_MODE_DUPMODE           DEF_BIT_30

#define  GMAC_RESET_ALLRST           DEF_BIT_31

#define  ETHSWMD_P0HDMODE            DEF_BIT_01
#define  ETHSWMD_P1HDMODE            DEF_BIT_03


#define  ETHSW_PORT0                 DEF_BIT_00
#define  ETHSW_PORT1                 DEF_BIT_01
#define  ETHSW_PORT2                 DEF_BIT_02

#define  R0_SYSTEM_CALL              DEF_BIT_29

#define  SYS_CALL_ERROR             (DEF_BIT_00 | DEF_BIT_01)
#define  SYS_CALL_COMPLETED          DEF_BIT_29

#define  GMAC_MIIM_RWDV              DEF_BIT_26

#define  GMAC_BUFID_NOEMP            DEF_BIT_31
#define  GMAC_BUFID_VALID            DEF_BIT_28

#define  TX_DESC_AUTO_FREE_OPT       DEF_BIT_31


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

typedef struct  dev_desc {
    CPU_REG32    sizebuf;
    CPU_INT08U  *p_buf;
}DEV_DESC;


typedef struct net_dev_macdma_desc {
    CPU_INT32U adr;
    CPU_INT32U len;
}NET_DEV_MACDMA_DESC;


/*
*********************************************************************************************************
*                                          RING BUFFER DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_dev_rbuf {
    CPU_INT08U  *Buf_Ptr;
    CPU_INT16U   Element_Size;
    CPU_INT16U   Element_Nb;
    CPU_INT16U   Rd;
    CPU_INT16U   Wr;
    CPU_INT16U   Cnt;
    CPU_INT16U   MaxCnt;
} NET_DEV_RING_BUF;


/*
*********************************************************************************************************
*                                        DEVICE INSTANCE DATA
*
*           (1) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*/

typedef  struct  net_dev_data {

    NET_DEV_RING_BUF  RxDescRingBuf;
    NET_DEV_RING_BUF  RxRdyRingBuf;

} NET_DEV_DATA;



/*
*********************************************************************************************************
*                                        DEVICE REGISTER DATA TYPE
*********************************************************************************************************
*/

                                                                /* ------------ ETHERC REGISTER DATA TYPE ------------- */
typedef struct net_dev_ether {
    CPU_REG32  ETSPCMD;
    CPU_REG32  MACSEL;
    CPU_REG32  MII_CTRL0;
    CPU_REG32  MII_CTRL1;
    CPU_REG32  MII_CTRL2;
    CPU_REG08  rsvd0[260];
    CPU_REG32  ETHSFTRST;
    CPU_REG08  rsvd1a[134884];
    CPU_REG32  C0TYPE;
    CPU_REG08  rsvd1b[4];
    CPU_REG32  C0STAT;
    CPU_REG08  rsvd1c[61428];
    CPU_REG32  SYSC;
    CPU_REG32  R4;
    CPU_REG32  R5;
    CPU_REG32  R6;
    CPU_REG32  R7;
    CPU_REG32  CMD;
    CPU_REG08  rsvd2[8];
    CPU_REG32  R0;
    CPU_REG32  R1;
    CPU_REG08  rsvd3[4068];
    CPU_REG32  GMAC_TXID;
    CPU_REG32  GMAC_TXRESULT;
    CPU_REG08  rsvd4[12];
    CPU_REG32  GMAC_MODE;
    CPU_REG32  GMAC_RXMODE;
    CPU_REG32  GMAC_TXMODE;
    CPU_REG08  rsvd5[4];
    CPU_REG32  GMAC_RESET;
    CPU_REG08  rsvd6[76];
    CPU_REG32  GMAC_PAUSE1;
    CPU_REG32  GMAC_PAUSE2;
    CPU_REG32  GMAC_PAUSE3;
    CPU_REG32  GMAC_PAUSE4;
    CPU_REG32  GMAC_PAUSE5;
    CPU_REG08  rsvd7[4];
    CPU_REG32  GMAC_FLWCTL;
    CPU_REG32  GMAC_PAUSPKT;
    CPU_REG32  GMAC_MIIM;
    CPU_REG08  rsvd8[92];
    CPU_REG32  GMAC_ADR0A;
    CPU_REG32  GMAC_ADR0B;
    CPU_REG32  GMAC_ADR1A;
    CPU_REG32  GMAC_ADR1B;
    CPU_REG32  GMAC_ADR2A;
    CPU_REG32  GMAC_ADR2B;
    CPU_REG32  GMAC_ADR3A;
    CPU_REG32  GMAC_ADR3B;
    CPU_REG32  GMAC_ADR4A;
    CPU_REG32  GMAC_ADR4B;
    CPU_REG32  GMAC_ADR5A;
    CPU_REG32  GMAC_ADR5B;
    CPU_REG32  GMAC_ADR6A;
    CPU_REG32  GMAC_ADR6B;
    CPU_REG32  GMAC_ADR7A;
    CPU_REG32  GMAC_ADR7B;
    CPU_REG32  GMAC_ADR8A;
    CPU_REG32  GMAC_ADR8B;
    CPU_REG32  GMAC_ADR9A;
    CPU_REG32  GMAC_ADR9B;
    CPU_REG32  GMAC_ADR10A;
    CPU_REG32  GMAC_ADR10B;
    CPU_REG32  GMAC_ADR11A;
    CPU_REG32  GMAC_ADR11B;
    CPU_REG32  GMAC_ADR12A;
    CPU_REG32  GMAC_ADR12B;
    CPU_REG32  GMAC_ADR13A;
    CPU_REG32  GMAC_ADR13B;
    CPU_REG32  GMAC_ADR14A;
    CPU_REG32  GMAC_ADR14B;
    CPU_REG32  GMAC_ADR15A;
    CPU_REG32  GMAC_ADR15B;
    CPU_REG08  rsvd9[128];
    CPU_REG32  GMAC_RXFIFO;
    CPU_REG32  GMAC_TXFIFO;
    CPU_REG32  GMAC_ACC;
    CPU_REG08  rsvd10[20];
    CPU_REG32  GMAC_RXMAC_ENA;
    CPU_REG32  GMAC_LPI_MODE;
    CPU_REG32  GMAC_LPI_TIMING;
    CPU_REG08  rsvd11[3796];
    CPU_REG32  BUFID;
    CPU_REG08  rsvd12[4092];
    CPU_REG32  SPCMD;
    CPU_REG08  rsvd13[12];
    CPU_REG32  EMACRST;
} NET_DEV_ETHERC;


/*
*********************************************************************************************************
*                                        SYSTEM CALL DATA TYPE
*********************************************************************************************************
*/

typedef struct net_dev_sys_cmd {
    CPU_INT32U  Cmd;
    CPU_INT32U  R4;
    CPU_INT32U  R5;
    CPU_INT32U  R6;
    CPU_INT32U  R7;
}NET_DEV_SYS_CMD;;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

                                                                /* ------------ FNCT'S COMMON TO ALL DRV'S ------------ */
static  void         NetDev_Init                   (NET_IF             *p_if,
                                                    NET_ERR            *p_err);

static  void         NetDev_Start                  (NET_IF             *p_if,
                                                    NET_ERR            *p_err);

static  void         NetDev_Stop                   (NET_IF             *p_if,
                                                    NET_ERR            *p_err);

static  void         NetDev_Rx                     (NET_IF             *p_if,
                                                    CPU_INT08U        **p_data,
                                                    CPU_INT16U         *p_size,
                                                    NET_ERR            *p_err);

static  void         NetDev_Tx                     (NET_IF             *p_if,
                                                    CPU_INT08U         *p_data,
                                                    CPU_INT16U          size,
                                                    NET_ERR            *p_err);

static  void         NetDev_AddrMulticastAdd       (NET_IF             *p_if,
                                                    CPU_INT08U         *p_addr_hw,
                                                    CPU_INT08U          addr_hw_len,
                                                    NET_ERR            *p_err);

static  void         NetDev_AddrMulticastRemove    (NET_IF             *p_if,
                                                    CPU_INT08U         *p_addr_hw,
                                                    CPU_INT08U          addr_hw_len,
                                                    NET_ERR            *p_err);

static  void         NetDev_ISR_Handler            (NET_IF             *p_if,
                                                    NET_DEV_ISR_TYPE    type);

static  void         NetDev_IO_Ctrl                (NET_IF             *p_if,
                                                    CPU_INT08U          opt,
                                                    void               *p_data,
                                                    NET_ERR            *p_err);

static  void         NetDev_MII_Rd                 (NET_IF             *p_if,
                                                    CPU_INT08U          phy_addr,
                                                    CPU_INT08U          reg_addr,
                                                    CPU_INT16U         *p_data,
                                                    NET_ERR            *p_err);

static  void         NetDev_MII_Wr                 (NET_IF             *p_if,
                                                    CPU_INT08U          phy_addr,
                                                    CPU_INT08U          reg_addr,
                                                    CPU_INT16U          data,
                                                    NET_ERR            *p_err);

                                                                /* ------------------ LOCAL FUNCTIONs ----------------- */

static  void         NetDev_ISR_Rx_Handler         (NET_IF             *p_if);

                                                                /* System Call functions.                               */
static  CPU_INT32U   NetDev_SysCall                (NET_IF             *p_if,
                                                    NET_DEV_SYS_CMD    *p_cmd,
                                                    CPU_INT32U         *p_ret);

static  CPU_INT32U   NetDev_SysCall_DMA_Transfer   (NET_IF             *p_if,
                                                    CPU_ADDR            dst_addr,
                                                    CPU_ADDR            src_addr,
                                                    CPU_INT16U          len);

static  CPU_INT32U   NetDev_SysCall_PacketTransfer (NET_IF             *p_if,
                                                    CPU_INT08U          direction,
                                                    CPU_ADDR            dst_addr,
                                                    CPU_ADDR            src_addr,
                                                    CPU_INT16U          len);

static  CPU_ADDR     NetDev_SysCall_GetBuf         (NET_IF             *p_if,
                                                    CPU_INT16U          len);

static  CPU_INT32U   NetDev_SysCall_TxStart        (NET_IF             *p_if,
                                                    CPU_ADDR            desc_addr);

static  CPU_INT32U   NetDev_SysCall_FreeBuf        (NET_IF             *p_if,
                                                    CPU_ADDR            buf_addr);

static  CPU_INT32U   NetDev_SysCall_RxEnable       (NET_IF             *p_if);


static  CPU_INT32U   NetDev_SysCall_RxDisable      (NET_IF             *p_if);

                                                                /* Ring buffer function.                                */
static   void        NetDev_RingBuf_Create         (NET_DEV_RING_BUF   *ring_buf,
                                                    CPU_INT16U          element_Size,
                                                    CPU_INT16U          element_Nb,
                                                    NET_ERR            *p_err );

static  void         NetDev_RingBuf_Reset          (NET_DEV_RING_BUF   *p_ring_buf);

static  CPU_INT08U  *NetDev_RingBuf_Wr_Get         (NET_DEV_RING_BUF   *p_buf);

static  void         NetDev_RingBuf_Wr_Set         (NET_DEV_RING_BUF   *p_buf);

static  CPU_INT08U  *NetDev_RingBuf_Rd_Get         (NET_DEV_RING_BUF   *p_buf);

static  void         NetDev_RingBuf_Rd_Set         (NET_DEV_RING_BUF   *p_buf);


/*
*********************************************************************************************************
*                                                 MACROS
*********************************************************************************************************
*/

#define  NET_DEV_UNLOCK_SYSTEM_REGISTER(reg)          do {                        \
                                                          reg = 0x00a5;           \
                                                          reg = 0x0001;           \
                                                          reg = 0xfffe;           \
                                                          reg = 0x0001;           \
                                                      } while(0)

#define  NET_DEV_LOCK_SYSTEM_REGISTER(reg)            (reg) = 0u

#define  SYS_CALL_CHECK_STATUS(status)                (((DEF_BIT_IS_SET(status, SYS_CALL_ERROR    ) == DEF_YES) ||                     \
                                                        (DEF_BIT_IS_SET(status, SYS_CALL_COMPLETED) == DEF_NO )) ? DEF_FAIL : DEF_OK)

#define  NET_DEV_RING_BUF_WR_GET_NEXT(p_buf)          NetDev_RingBuf_Wr_Get((p_buf))
#define  NET_DEV_RING_BUF_WR_SET_NEXT(p_buf)          NetDev_RingBuf_Wr_Set((p_buf))
#define  NET_DEV_RING_BUF_RD_GET_NEXT(p_buf)          NetDev_RingBuf_Rd_Get((p_buf))
#define  NET_DEV_RING_BUF_RD_SET_NEXT(p_buf)          NetDev_RingBuf_Rd_Set((p_buf))

#define  NET_DEV_RING_BUF_IS_FULL(p_buf)              (((p_buf)->Cnt >= (p_buf)->Element_Nb - 1) ? DEF_YES : DEF_NO)
#define  NET_DEV_RING_BUF_IS_EMPTY(p_buf)             (((p_buf)->Cnt == 0) ? DEF_YES : DEF_NO)

#define  NET_DEV_COMPUTE_BUFFID_ADDR(buffid)          (((buffid) & 0xffff) << 11) | 0x08000000
#define  NET_DEV_COMPUTE_BUFFID_LEN(buffid)           ((buffid) & 0x0fff0000) >> 16

#define  NET_DEV_COMPUTE_RX_CTRL_LEN(rx_ctrl)         (((rx_ctrl) >> 16) & 0x7fff) - 9


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
                                                                                   /* RZ-Ether dev API fnct ptrs :      */
const  NET_DEV_API_ETHER  NetDev_API_RZT1_EtherC = {
                                                     NetDev_Init,                  /*   Init/add                        */
                                                     NetDev_Start,                 /*   Start                           */
                                                     NetDev_Stop,                  /*   Stop                            */
                                                     NetDev_Rx,                    /*   Rx                              */
                                                     NetDev_Tx,                    /*   Tx                              */
                                                     NetDev_AddrMulticastAdd,      /*   Multicast addr add              */
                                                     NetDev_AddrMulticastRemove,   /*   Multicast addr remove           */
                                                     NetDev_ISR_Handler,           /*   ISR handler                     */
                                                     NetDev_IO_Ctrl,               /*   I/O ctrl                        */
                                                     NetDev_MII_Rd,                /*   Phy reg rd                      */
                                                     NetDev_MII_Wr                 /*   Phy reg wr                      */
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
    NET_DEV_DATA       *p_dev_data;
    CPU_SIZE_T          reqd_octets;
    LIB_ERR             lib_err;

                                                                /* ------------ ALLOCATE DEV DATA AREA ---------------- */
    p_if->Dev_Data = Mem_HeapAlloc ((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                    (CPU_SIZE_T  ) 4,
                                    (CPU_SIZE_T *)&reqd_octets,
                                    (LIB_ERR    *)&lib_err);
    if (p_if->Dev_Data == (void *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        goto exit;
    }
                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/BSP--- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */
    p_dev_bsp  = (NET_DEV_BSP_ETHER *)p_if->Dev_BSP;            /* Obtain ptr to BSP API.                               */

                                                                /* ----------------- CHECK DEV CONFIG ----------------- */
    if ((p_dev_cfg->MemAddr < NET_DEV_DATA_RAM_ADDR_START) ||
        (p_dev_cfg->MemAddr > NET_DEV_DATA_RAM_ADDR_END  )) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

    if (p_dev_cfg->TxBufIxOffset < 32u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

    if (p_dev_cfg->TxDescNbr > 2u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

    if (p_dev_cfg->RxBufAlignOctets < sizeof(CPU_INT32U)) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

    if (p_dev_cfg->TxBufAlignOctets < sizeof(CPU_INT32U)) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }
                                                                /* ----------- ENABLE NECESSARY CLOCKS ---------------- */
                                                                /* Enable module clocks (see Note #3).                  */
    p_dev_bsp->CfgClk(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         goto exit;
    }
                                                                /* ----- INITIALIZE EXTERNAL GPIO CONTROLLER ---------- */
                                                                /* Configure Ethernet Controller GPIO (see Note #5).    */
    p_dev_bsp->CfgGPIO(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         goto exit;
    }
                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
                                                                /* Initialize ext. int. ctlr (see Note #4).             */
    p_dev_bsp->CfgIntCtrl(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         goto exit;
    }
                                                                /* -------- ALLOCATE MEMORY FOR RX RING BUFER --------- */
    NetDev_RingBuf_Create(&p_dev_data->RxDescRingBuf,
                           sizeof(DEV_DESC),
                           p_dev_cfg->RxDescNbr,
                           p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         goto exit;
    }

    NetDev_RingBuf_Create(&p_dev_data->RxRdyRingBuf,
                           sizeof(DEV_DESC),
                           p_dev_cfg->RxBufLargeNbr,
                           p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         goto exit;
    }

   *p_err = NET_DEV_ERR_NONE;

exit:
   return;
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
    NET_DEV_ETHERC     *p_etherc;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;
    NET_PHY_CFG_ETHER  *p_phy_cfg;
    CPU_REG32           status;

                                                                /* ----- OBTAIN REFERENCE TO DEVICE CFG/REGISTERS ----- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */
    p_phy_cfg  = (NET_PHY_CFG_ETHER *)p_if->Ext_Cfg;            /* Obtain ptr to the phy cfg struct.                    */

                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(p_if,                               /* See Note #3.                                         */
                            p_dev_cfg->TxDescNbr,
                            p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }
                                                                /* --------------- RESET ETHERC MODULE ---------------- */
    NET_DEV_UNLOCK_SYSTEM_REGISTER(p_etherc->SPCMD);

    p_etherc->EMACRST = 0u;
    while (p_etherc->EMACRST != 0u);
    p_etherc->EMACRST = EMACRST_RESET;

    NET_DEV_LOCK_SYSTEM_REGISTER(p_etherc->SPCMD);

                                                                /* -------------- RESET ETHERNET IF CTRL -------------- */
    NET_DEV_UNLOCK_SYSTEM_REGISTER(p_etherc->ETSPCMD);

    if (p_phy_cfg->BusMode == NET_PHY_BUS_MODE_MII) {           /* Configure the Phy bus mode.                          */
        p_etherc->MII_CTRL0 = MII_CTRL_MODE_MII ;
    } else {
        p_etherc->MII_CTRL0 = MII_CTRL_MODE_RMII;
    }

    p_etherc->MACSEL = 1u;

    DEF_BIT_SET(p_etherc->ETHSFTRST, ETHSFTRST_PHYRST  |
                                     ETHSFTRST_PHYRST2 |
                                     ETHSFTRST_MIIRST);

    NET_DEV_LOCK_SYSTEM_REGISTER(p_etherc->SPCMD);
                                                                /* ------------- SETUP HARDWARE FUNCTION -------------- */
    p_etherc->C0TYPE = NET_DEV_HW_TASK;
    p_etherc->C0STAT = NET_DEV_HW_READY;
    p_etherc->CMD    = NET_DEV_HW_INIT;

    while ((p_etherc->R0 & HW_STATUS_READY) == DEF_NULL);
    status = p_etherc->R1;                                      /* Dummy read R1 to clear the value.                    */
                                                                /* -------------------- RESET GMAC -------------------- */

    DEF_BIT_SET(p_etherc->GMAC_RESET, GMAC_RESET_ALLRST);
    while (DEF_BIT_IS_SET(p_etherc->GMAC_RESET, GMAC_RESET_ALLRST) == DEF_YES);

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
            hw_addr[0] = (p_etherc->GMAC_ADR0A >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1] = (p_etherc->GMAC_ADR0A >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2] = (p_etherc->GMAC_ADR0A >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3] = (p_etherc->GMAC_ADR0A >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4] = (p_etherc->GMAC_ADR0B>> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5] = (p_etherc->GMAC_ADR0B >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

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
     p_etherc->GMAC_ADR0A = (((CPU_INT32U)hw_addr[0] << (3 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[1] << (2 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[2] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[3] << (0 * DEF_INT_08_NBR_BITS)));

     p_etherc->GMAC_ADR0B = (((CPU_INT32U)hw_addr[4] << (1 * DEF_INT_08_NBR_BITS)) |
                             ((CPU_INT32U)hw_addr[5] << (0 * DEF_INT_08_NBR_BITS)));
    }

                                                                /* -------------------- ENABLE RX --------------------- */
    status = NetDev_SysCall_RxEnable(p_if);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        *p_err = NET_DEV_ERR_FAULT;
         goto exit;
    }
    *p_err = NET_DEV_ERR_NONE;
exit:
    return;
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
    NET_DEV_DATA       *p_dev_data;
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_ETHERC     *p_etherc;
    DEV_DESC           *p_desc;
    CPU_ADDR            hw_buf;
    CPU_INT32U          status;
    CPU_SR_ALLOC();

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */

                                                                /* -------------------- DISABLE RX -------------------- */
    status = NetDev_SysCall_RxDisable(p_if);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        *p_err = NET_DEV_ERR_FAULT;
         goto exit;
    }
                                                                /* ---------- FREE ALL REMAINING HW BUFFERS ----------- */
    while (NET_DEV_RING_BUF_IS_EMPTY(&p_dev_data->RxDescRingBuf) == DEF_NO) {

        CPU_CRITICAL_ENTER();

        p_desc = (DEV_DESC*)NET_DEV_RING_BUF_RD_GET_NEXT(&p_dev_data->RxDescRingBuf);
        hw_buf = (CPU_ADDR) p_desc->p_buf;
        NET_DEV_RING_BUF_RD_SET_NEXT(&p_dev_data->RxDescRingBuf);

        CPU_CRITICAL_EXIT();

        NetDev_SysCall_FreeBuf(p_if,hw_buf);
    }
                                                                /* ---------------- RESET RING BUFFER ----------------- */
    NetDev_RingBuf_Reset(&p_dev_data->RxDescRingBuf);
    NetDev_RingBuf_Reset(&p_dev_data->RxRdyRingBuf);

                                                                /* --------------- DISABLE ETHER MODULE --------------- */
    p_etherc->EMACRST = 0u;

   *p_err = NET_DEV_ERR_NONE;
exit:
    return;
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
    CPU_INT08U    *p_buf;
    NET_BUF_SIZE   rx_len;
    NET_ERR        net_err;
    DEV_DESC*      p_desc,p_desx_rdy;
    CPU_INT16U     len;
    CPU_ADDR       hw_buf;
    CPU_ADDR       buf_addr;
    CPU_INT32U     status;
    CPU_INT32U     ret;
    CPU_SR_ALLOC();

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */

                                                                /* ---------- COMPUTE ALL PENDING RX HW BUF ----------- */

    while (1) {
                                                                /* Check if there's no more pending buffers.            */
        if (NET_DEV_RING_BUF_IS_EMPTY(&p_dev_data->RxDescRingBuf) == DEF_YES) {
            break;
        }
                                                                /* Check if there's a rx buf available.                 */
        if (NET_DEV_RING_BUF_IS_FULL(&p_dev_data->RxRdyRingBuf) == DEF_YES) {
            break;
        }
                                                                /* Get a rx buffer.                                     */
        p_buf = NetBuf_GetDataPtr((NET_IF        *)p_if,
                                  (NET_TRANSACTION)NET_TRANSACTION_RX,
                                  (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                  (NET_BUF_SIZE   )NET_IF_IX_RX,
                                  (NET_BUF_SIZE  *)0,
                                  (NET_BUF_SIZE  *)&rx_len,
                                  (NET_BUF_TYPE  *) 0,
                                  (NET_ERR       *)&net_err);
        if (net_err != NET_BUF_ERR_NONE) {
             break;
        }
                                                                /* Pop the next available buffer from the ring buffer.  */
        CPU_CRITICAL_ENTER();

        p_desc = (DEV_DESC*)NET_DEV_RING_BUF_RD_GET_NEXT(&p_dev_data->RxDescRingBuf);
        hw_buf = (CPU_ADDR) p_desc->p_buf;
        len    = p_desc->sizebuf;
        NET_DEV_RING_BUF_RD_SET_NEXT(&p_dev_data->RxDescRingBuf);

        CPU_CRITICAL_EXIT();
                                                                /* Copy the hw buffer to software buffer.               */
        buf_addr = (CPU_ADDR)p_buf;
        status   = NetDev_SysCall_PacketTransfer(p_if,
                                                 NET_DEV_RX_PKT,
                                                 buf_addr,
                                                 hw_buf,
                                                 len);
        if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
            CPU_SW_EXCEPTION(;);
        }
                                                                /* Free hw buffer.                                      */
        status = NetDev_SysCall_FreeBuf(p_if, hw_buf);
        if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
            CPU_SW_EXCEPTION(;);
        }

                                                                /* Queue the new buffer in the rdy ring buffer.         */
        p_desc = (DEV_DESC*)NET_DEV_RING_BUF_WR_GET_NEXT(&p_dev_data->RxRdyRingBuf);
        p_desc->p_buf   = p_buf;
        p_desc->sizebuf = len;
        NET_DEV_RING_BUF_WR_SET_NEXT(&p_dev_data->RxRdyRingBuf);

    }
                                                                /* Enable Rx in case it was disabled by int handler.    */
    status = NetDev_SysCall_RxEnable(p_if);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        CPU_SW_EXCEPTION(;);
    }
                                                                /* Dequeue the next rdy buffer in the ring buffer.      */
    p_desc = (DEV_DESC*)NET_DEV_RING_BUF_RD_GET_NEXT(&p_dev_data->RxRdyRingBuf);
   *p_data = p_desc->p_buf;
   *p_size = p_desc->sizebuf;
    NET_DEV_RING_BUF_RD_SET_NEXT(&p_dev_data->RxRdyRingBuf);

    CPU_DCACHE_RANGE_INV(*p_data, *p_size);

    (void)&ret;
   *p_err = NET_DEV_ERR_NONE;
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
    CPU_ADDR              tx_ctrl_addr;
    CPU_ADDR              tx_desc_addr;
    CPU_ADDR              tx_ctrl_hw_addr;
    CPU_ADDR              tx_desc_hw_addr;
    CPU_ADDR              buf_addr;
    CPU_ADDR              data_addr;
    CPU_INT32U           *p_tx_ctrl_tmp;
    CPU_INT32U            status;
    NET_DEV_MACDMA_DESC  *p_tx_desc;


    CPU_DCACHE_RANGE_FLUSH(p_data, size);

    p_tx_desc     = (NET_DEV_MACDMA_DESC *) (p_data - 32);
    p_tx_ctrl_tmp = (CPU_INT32U          *) (p_data - 8);
                                                                /* Get Hw buffers.                                      */

    buf_addr = NetDev_SysCall_GetBuf(p_if, size + 4);
    if (buf_addr  == DEF_NULL) {
       *p_err = NET_DEV_ERR_FAULT;
    }
    tx_ctrl_hw_addr = NetDev_SysCall_GetBuf(p_if, 128);
    if (tx_ctrl_hw_addr == DEF_NULL) {
       *p_err = NET_DEV_ERR_FAULT;
    }
    tx_desc_hw_addr = NetDev_SysCall_GetBuf(p_if, 128);
    if (tx_desc_hw_addr == DEF_NULL) {
       *p_err = NET_DEV_ERR_FAULT;
    }
                                                                /* Copy packet in hw_buf.                               */
    data_addr = (CPU_ADDR)p_data;
    status    = NetDev_SysCall_PacketTransfer(p_if,
                                              NET_DEV_TX_PKT,
                                              buf_addr,
                                              data_addr,
                                              size);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        CPU_SW_EXCEPTION(;);
    }
                                                                /* Create frame control struct.                         */
    p_tx_ctrl_tmp[0] = ((size + 3) << 16);
    p_tx_ctrl_tmp[1] = tx_desc_hw_addr;

    CPU_DCACHE_RANGE_FLUSH(p_tx_ctrl_tmp,8);

                                                                /* Copy control struct in hw buf.                       */
    tx_ctrl_addr = (CPU_ADDR)p_tx_ctrl_tmp;
    status       = NetDev_SysCall_DMA_Transfer(p_if,
                                               tx_ctrl_hw_addr,
                                               tx_ctrl_addr,
                                               8);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        CPU_SW_EXCEPTION(;);
    }
                                                                /* Create tx descriptor.                                */
    p_tx_desc[0].adr = tx_ctrl_hw_addr;
    p_tx_desc[0].len = 8 | DEF_BIT_31;
    p_tx_desc[1].adr = buf_addr ;
    p_tx_desc[1].len = (size + 2) | DEF_BIT_31;
    p_tx_desc[2].adr = 0xffffffff;
    p_tx_desc[2].len = 0;

    CPU_DCACHE_RANGE_FLUSH(p_tx_desc,24);
                                                                /* Copy tx descriptor in hw buf.                        */
    tx_desc_addr = (CPU_ADDR)p_tx_desc;
    status       = NetDev_SysCall_DMA_Transfer(p_if,
                                               tx_desc_hw_addr,
                                               tx_desc_addr,
                                               24);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        CPU_SW_EXCEPTION(;);
    }
                                                                /* Start packet TX.                                     */
    status = NetDev_SysCall_TxStart(p_if, tx_desc_hw_addr);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        CPU_SW_EXCEPTION(;);
    }
                                                                /* Tx Buffer dealloc.                                   */
    NetIF_TxDeallocTaskPost((CPU_INT08U *)p_data, p_err);
    if (*p_err !=  NET_IF_ERR_NONE) {
        CPU_SW_EXCEPTION(;);
    }

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
   (void)&p_if;
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
   (void)&p_if;
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
    NET_DEV_ETHERC      *p_etherc;
    NET_PHY_API_ETHER   *p_phy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;

                                                                /* ----- OBTAIN REFERENCE TO DEVICE CFG/REGISTERS ----- */
    p_dev_cfg = (NET_DEV_CFG_ETHER *) p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc  = (NET_DEV_ETHERC    *) p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

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
             duplex       =  NET_PHY_DUPLEX_UNKNOWN;

             if (p_link_state->Duplex != duplex) {
                 switch (p_link_state->Duplex) {
                    case NET_PHY_DUPLEX_FULL:
                          NET_DEV_UNLOCK_SYSTEM_REGISTER(p_etherc->SPCMD);
                          DEF_BIT_SET(p_etherc->MII_CTRL0, MII_CTRL_MODE_FULLD);
                          NET_DEV_LOCK_SYSTEM_REGISTER(p_etherc->SPCMD);

                          DEF_BIT_SET(p_etherc->GMAC_MODE, GMAC_MODE_DUPMODE);
                          break;


                    case NET_PHY_DUPLEX_HALF:
                          NET_DEV_UNLOCK_SYSTEM_REGISTER(p_etherc->SPCMD);
                          DEF_BIT_CLR(p_etherc->MII_CTRL0, MII_CTRL_MODE_FULLD);
                          NET_DEV_LOCK_SYSTEM_REGISTER(p_etherc->SPCMD);

                          DEF_BIT_CLR(p_etherc->GMAC_MODE, GMAC_MODE_DUPMODE);
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
    CPU_INT32U          reg_tmp;
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_ETHERC     *p_etherc;

                                                                /* ----- OBTAIN REFERENCE TO DEVICE CFG/REGISTERS ----- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */


    p_etherc->GMAC_MIIM = ((CPU_INT32U)phy_addr << 21) | ((CPU_INT32U)reg_addr << 16);

    while (DEF_ON) {
        reg_tmp = p_etherc->GMAC_MIIM;
        if (DEF_BIT_IS_SET(reg_tmp , GMAC_MIIM_RWDV) == DEF_YES) {
            *p_data = (CPU_INT16U)(reg_tmp & DEF_INT_16_MASK);
             break;
        }
    }
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
    CPU_INT32U reg_tmp;
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_ETHERC     *p_etherc;


                                                                /* ----- OBTAIN REFERENCE TO DEVICE CFG/REGISTERS ----- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

    p_etherc->GMAC_MIIM = GMAC_MIIM_RWDV | ((CPU_INT32U)phy_addr << 21) | ((CPU_INT32U)reg_addr << 16) | data;

    while (DEF_ON) {
        reg_tmp = p_etherc->GMAC_MIIM;
        if (DEF_BIT_IS_SET(reg_tmp , GMAC_MIIM_RWDV) == DEF_YES) {
            break;
        }
    }

   *p_err = NET_PHY_ERR_NONE;
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
    CPU_INT32U          tx_id;
    CPU_INT32U          tx_result;
    NET_DEV_ETHERC     *p_etherc;

                                                                /* ----- OBTAIN REFERENCE TO DEVICE CFG/REGISTERS ----- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* Procces tranmission complete interrupt.              */
    if (type == NET_DEV_ISR_TYPE_TX_COMPLETE) {

        tx_id     = p_etherc->GMAC_TXID;                        /* Tx Id is the Tx Desc buffer addr to release.         */
        tx_result = p_etherc->GMAC_TXRESULT;                    /* Clear Tx Result.                                     */

        NetDev_SysCall_FreeBuf(p_if, tx_id);                    /* Free Tx Desc Buffer.                                 */

        NetIF_DevTxRdySignal(p_if);
       (void)&tx_result;
    }
                                                                /* Process available Rx buffer.                         */
    NetDev_ISR_Rx_Handler(p_if);
}


/*
*********************************************************************************************************
*                                        NetDev_ISR_Rx_Handler()
*
* Description : Handle the RX part of the ISR.
*
* Argument(s) : p_if    Pointer to interface requiring service.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ISR_Handler().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void  NetDev_ISR_Rx_Handler (NET_IF  *p_if)
{
    CPU_INT32U           buffid;
    CPU_INT32U           recv_adr;
    CPU_INT32U           recv_len;
    CPU_INT32U          *rx_frame_control;
    CPU_INT32U           len;
    NET_ERR              err;
    DEV_DESC            *p_desc;
    NET_DEV_DATA        *p_dev_data;
    NET_DEV_CFG_ETHER   *p_dev_cfg;
    NET_DEV_ETHERC      *p_etherc;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */
    p_dev_data = (NET_DEV_DATA      *)p_if->Dev_Data;           /* Obtain ptr to dev data area.                         */

    while (DEF_ON) {
                                                                /* Validate if there's still space in the ring buffer.  */
        if (NET_DEV_RING_BUF_IS_FULL(&p_dev_data->RxDescRingBuf) == DEF_YES) {
           (void)NetDev_SysCall_RxDisable(p_if);
            break;
        }
                                                                /* Get and compute the BUFFID register.                 */
        buffid   = p_etherc->BUFID;
        recv_adr = NET_DEV_COMPUTE_BUFFID_ADDR(buffid);
                                                                /* Check packet exist.                                  */
        if (DEF_BIT_IS_SET(buffid, GMAC_BUFID_NOEMP) == DEF_NO) {
            break;
        }
                                                                /* Check packet error.                                  */
        if (DEF_BIT_IS_SET(buffid, GMAC_BUFID_VALID) == DEF_NO) {
            NetDev_SysCall_FreeBuf(p_if,recv_adr);
            continue ;
        }
                                                                /* Get buffer len.                                      */
        recv_len = NET_DEV_COMPUTE_BUFFID_LEN(buffid);
                                                                /* Get rx control data.                                 */
        rx_frame_control = (CPU_INT32U *)recv_adr + recv_len - 2;

                                                                /* Frame validation.                                    */
        if (DEF_BIT_IS_SET(rx_frame_control[0], DEF_BIT_04 | DEF_BIT_03 | DEF_BIT_01 | DEF_BIT_00)) {
            NetDev_SysCall_FreeBuf(p_if,recv_adr);
            continue;
        }
                                                                /* Get Packet length.                                   */
        len = NET_DEV_COMPUTE_RX_CTRL_LEN(rx_frame_control[0]);

                                                                /* Queue packet in the ring buffer.                     */
        p_desc = (DEV_DESC *) NET_DEV_RING_BUF_WR_GET_NEXT(&p_dev_data->RxDescRingBuf);
        p_desc->p_buf   = (CPU_INT08U*)recv_adr;
        p_desc->sizebuf = len;
        NET_DEV_RING_BUF_WR_SET_NEXT(&p_dev_data->RxDescRingBuf);
                                                                /* Signal the RX task.                                   */
        NetIF_RxTaskSignal(p_if->Nbr, &err);
    }
}


/*
*********************************************************************************************************
*                                           NetDev_SysCall()
*
* Description : Perform a System Call and wait its completion.
*
* Argument(s) : p_if    Pointer to an Ethernet network interface.
*
*               p_cmd   Pointer to a system command object to call.
*
*               p_rtn   Pointer that will carry the exception value if used.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_SysCall_DMA_Transfer(),
*               NetDev_SysCall_FreeBuf(),
*               NetDev_SysCall_GetBuf(),
*               NetDev_SysCall_RxDisable(),
*               NetDev_SysCall_RxEnable(),
*               NetDev_SysCall_TxStart().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall(NET_IF           *p_if,
                                   NET_DEV_SYS_CMD  *p_cmd,
                                   CPU_INT32U       *p_rtn)
{

    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_ETHERC     *p_etherc;
    CPU_REG32           r0;
    CPU_SR_ALLOC();

                                                                /* ----- OBTAIN REFERENCE TO DEVICE CFG/REGISTERS ----- */
    p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;            /* Obtain ptr to the dev cfg struct.                    */
    p_etherc   = (NET_DEV_ETHERC    *)p_dev_cfg->BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */

    CPU_CRITICAL_ENTER();
                                                                /* Set parameters.                                      */
    p_etherc->R4  = p_cmd->R4;
    p_etherc->R5  = p_cmd->R5;
    p_etherc->R6  = p_cmd->R6;
    p_etherc->R7  = p_cmd->R7;
                                                                /* Set command to start the function.                   */
    p_etherc->SYSC = p_cmd->Cmd;
                                                                /* Wait completion.                                     */
    do {
        r0 = p_etherc->R0;
    } while ((r0 & R0_SYSTEM_CALL) != R0_SYSTEM_CALL);

    *p_rtn = p_etherc->R1;

    CPU_CRITICAL_EXIT();

    return r0;
}


/*
*********************************************************************************************************
*                                     NetDev_SysCall_DMA_Transfer()
*
* Description : Perform a Transfer from the DATA_RAM(Software Buffer) to the BUFFER_RAM (Hardware Buffer)
*               or vice versa.
*
* Argument(s) : p_if        Pointer to an Ethernet network interface.
*
*               dst_addr    Destination address.
*
*               src_addr    Source Address.
*
*               len         Transfer Length.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_SysCall_PacketTransfer(),
*               NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall_DMA_Transfer(NET_IF      *p_if,
                                                CPU_ADDR     dst_addr,
                                                CPU_ADDR     src_addr,
                                                CPU_INT16U   len)
{
    NET_DEV_SYS_CMD     cmd ;
    CPU_INT32U          status;
    CPU_INT32U          ret;


    cmd.Cmd = 0x5211;
    cmd.R4  = src_addr;
    cmd.R5  = dst_addr;
    cmd.R6  = len;
    cmd.R7  = 0u;

    status = NetDev_SysCall(p_if, &cmd, &ret);
    (void)&ret;

    return status;
}


/*
*********************************************************************************************************
*                                        NetDev_SysCall_GetBuf()
*
* Description : Get a hardware buffer from the BUFFER_RAM.
*
* Argument(s) : p_if    Pointer to an Ethernet network interface.
*
*               len     Length of the buffer to get.
*
* Return(s)   : Address of the Hardware buffer.
*
* Caller(s)   : NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_ADDR  NetDev_SysCall_GetBuf (NET_IF      *p_if,
                                         CPU_INT16U   len)
{
    NET_DEV_SYS_CMD     cmd ;
    CPU_INT32U          status;
    CPU_INT32U          ret;


    if (len > 512) {
       cmd.Cmd = 0x5000;                                        /* Small Buffer Cmd.                                    */
    } else  {
       cmd.Cmd = 0x5006;                                        /* Large Buffer Cmd.                                    */
    }

    cmd.R4  = len;
    cmd.R5  = 0;
    cmd.R6  = 0;
    cmd.R7  = 0u;

    status = NetDev_SysCall(p_if, &cmd, &ret);

    if ((status & DEF_BIT_01) == 0x2) {
        ret = DEF_NULL;
    }

    return ret;
}


/*
*********************************************************************************************************
*                                       NetDev_SysCall_FreeBuf()
*
* Description : Free an hardware buffer allocated from the BUFFER_RAM.
*
* Argument(s) : p_if        Pointer to an Ethernet network interface.
*
*               buf_addr    Address of the buffer to free.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_ISR_Handler(),
*               NetDev_ISR_Rx_Handler(),
*               NetDev_Rx(),
*               NetDev_Stop().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall_FreeBuf (NET_IF    *p_if,
                                            CPU_ADDR   buf_addr)
{
    NET_DEV_SYS_CMD     cmd  ;
    CPU_INT32U          status;
    CPU_INT32U          ret;


    cmd.Cmd = 0x5001;

    cmd.R4  = buf_addr;
    cmd.R5  = 0;
    cmd.R6  = 0;
    cmd.R7  = 0u;

    status = NetDev_SysCall( p_if, &cmd, &ret);
    (void)&ret;

    return status;
}


/*
*********************************************************************************************************
*                                    NetDev_SysCall_PacketTransfer()
*
* Description : Transfer a packet from/to the BUFFER_RAM  removing/adding the padding.
*
* Argument(s) : p_if        Pointer to an Ethernet network interface.
*
*               direction   Direction of the transfer, either  NET_DEV_RX_PKT or NET_DEV_TX_PKT.
*
*               dst_addr    Destination Address.
*
*               src_addr    Source Address
*
*               len         Length of the packet to transfer.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_Rx(),
*               NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall_PacketTransfer (NET_IF      *p_if,
                                                   CPU_INT08U   direction,
                                                   CPU_ADDR     dst_addr,
                                                   CPU_ADDR     src_addr,
                                                   CPU_INT16U   len)
{
    CPU_INT32U  status;

                                                                /* Copy the Mac Header.                                 */
    status = NetDev_SysCall_DMA_Transfer(p_if, dst_addr, src_addr, NET_DEV_MAC_HDR_LEN);
    if (SYS_CALL_CHECK_STATUS(status) == DEF_FAIL) {
        goto exit;
    }
                                                                /* Update Address and remaining length.                 */
    dst_addr +=  NET_DEV_MAC_HDR_LEN;
    src_addr +=  NET_DEV_MAC_HDR_LEN;
    len      -=  NET_DEV_MAC_HDR_LEN;
                                                                /* Add the proper offset according to the direction.    */
    if (direction == NET_DEV_RX_PKT) {
        src_addr += sizeof(CPU_INT16U);
    } else {
        dst_addr += sizeof(CPU_INT16U);
    }
                                                                /* Copy the rest of the packet.                         */
    status = NetDev_SysCall_DMA_Transfer(p_if, dst_addr, src_addr, len);

exit:
    return status;
}


/*
*********************************************************************************************************
*                                       NetDev_SysCall_RxEnable()
*
* Description : Enable Reception.
*
* Argument(s) : p_if    Pointer to an Ethernet network interface.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_Rx(),
*               NetDev_Start().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall_RxEnable (NET_IF  *p_if)
{
    NET_DEV_SYS_CMD     cmd ;
    CPU_INT32U          status;
    CPU_INT32U          ret;


    cmd.Cmd = 0x5101;
    cmd.R4  = 0u;
    cmd.R5  = 0u;
    cmd.R6  = 0u;
    cmd.R7  = 0u;

    status = NetDev_SysCall(p_if, &cmd, &ret);
    (void)&ret;

    return status;
}


/*
*********************************************************************************************************
*                                      NetDev_SysCall_RxDisable()
*
* Description : Disable Reception.
*
* Argument(s) : p_if    Pointer to an Ethernet network interface.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_ISR_Rx_Handler(),
*               NetDev_Stop().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall_RxDisable (NET_IF  *p_if)
{
    NET_DEV_SYS_CMD     cmd ;
    CPU_INT32U          status;
    CPU_INT32U          ret;


    cmd.Cmd = 0x5102;
    cmd.R4  = 0u;
    cmd.R5  = 0u;
    cmd.R6  = 0u;
    cmd.R7  = 0u;

    status = NetDev_SysCall(p_if, &cmd, &ret);
    (void)&ret;

    return status;
}


/*
*********************************************************************************************************
*                                       NetDev_SysCall_TxStart()
*
* Description : Initiate a packet transmission with the Transmission descriptor.
*
* Argument(s) : p_if    Pointer to an Ethernet network interface.
*
*               desc    Address to the Transmission descriptor.
*
* Return(s)   : Status of the system call.
*
* Caller(s)   : NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SysCall_TxStart (NET_IF    *p_if,
                                            CPU_ADDR   desc)
{
    NET_DEV_SYS_CMD     cmd ;
    CPU_INT32U          status;
    CPU_INT32U          ret;

    cmd.Cmd = 0x5100;
    cmd.R4  = desc;
    cmd.R5  = 0u;
    cmd.R6  = 0u;
    cmd.R7  = 0u;

    status = NetDev_SysCall(p_if, &cmd, &ret);
    (void)&ret;

    return status;

}


/*
*********************************************************************************************************
*                                        NetDev_RingBuf_Create()
*
* Description : Create/Allocate a Ring buffer.
*
* Argument(s) : p_ring_buf      Pointer to the ring buffer control structure.
*
*               element_size    Size of each element in the ring buffer.
*
*               element_nb      Number of element in the ring buffer.
*
*               p_err           Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_RingBuf_Create (NET_DEV_RING_BUF  *p_ring_buf,
                                     CPU_INT16U         element_size,
                                     CPU_INT16U         element_nb,
                                     NET_ERR           *p_err)
{
    LIB_ERR     lib_err;
    CPU_SIZE_T  reqd_octets;


    p_ring_buf->Buf_Ptr = (CPU_INT08U*)Mem_HeapAlloc(element_size * element_nb,
                                                     4,
                                                    &reqd_octets,
                                                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        goto exit;
    }

    p_ring_buf->Element_Size = element_size;
    p_ring_buf->Element_Nb   = element_nb;
    p_ring_buf->Rd           = 0;
    p_ring_buf->Wr           = 0;
    p_ring_buf->Cnt          = 0;
    p_ring_buf->MaxCnt       = 0;

    *p_err = NET_DEV_ERR_NONE;
exit:
    return;
}


/*
*********************************************************************************************************
*                                        NetDev_RingBuf_Reset()
*
* Description : Reset a Ring Buffer control structure.
*
* Argument(s) : p_ring_buf      Pointer to the ring buffer control structure.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_RingBuf_Reset (NET_DEV_RING_BUF  *p_ring_buf)
{

    p_ring_buf->Rd     = 0u;
    p_ring_buf->Wr     = 0u;
    p_ring_buf->Cnt    = 0u;
    p_ring_buf->MaxCnt = 0u;
}


/*
*********************************************************************************************************
*                                        NetDev_RingBuf_Wr_Get()
*
* Description : Get the next free element to write in the ring buffer.
*
* Argument(s) : p_buf   Pointer to the ring buffer control structure.
*
* Return(s)   : Pointer to the element.
*
* Caller(s)   : NetDev_ISR_Rx_Handler(),
*               NetDev_Rx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT08U  *NetDev_RingBuf_Wr_Get (NET_DEV_RING_BUF *p_buf)
{
    return p_buf->Buf_Ptr + (p_buf->Wr * p_buf->Element_Size);
}


/*
*********************************************************************************************************
*                                        NetDev_RingBuf_Wr_Set()
*
* Description : Set the next free element to write as ready in the ring buffer.
*
* Argument(s) : p_buf   Pointer to the ring buffer control structure.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ISR_Rx_Handler(),
*               NetDev_Rx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_RingBuf_Wr_Set (NET_DEV_RING_BUF *p_buf)
{
    p_buf->Wr++;
    if (p_buf->Wr == p_buf->Element_Nb) {
        p_buf->Wr = 0;
    }

    p_buf->Cnt++;
                                                                /* For debug purpose.                                   */
    if (p_buf->Cnt > p_buf->MaxCnt) {
        p_buf->MaxCnt = p_buf->Cnt;
    }
}


/*
*********************************************************************************************************
*                                        NetDev_RingBuf_Rd_Get()
*
* Description : Get the next ready element to read in the ring buffer.
*
* Argument(s) : p_buf   Pointer to the ring buffer control structure.
*
* Return(s)   : Pointer to the element.
*
* Caller(s)   : NetDev_Rx(),
*               NetDev_Stop().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT08U  *NetDev_RingBuf_Rd_Get (NET_DEV_RING_BUF *p_buf)
{
    return p_buf->Buf_Ptr + (p_buf->Rd * p_buf->Element_Size);
}


/*
*********************************************************************************************************
*                                        NetDev_RingBuf_Rd_Set()
*
* Description : Set the next element to read as read in the ring buffer.
*
* Argument(s) : p_buf   Pointer to the ring buffer control structure.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx(),
*               NetDev_Stop().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_RingBuf_Rd_Set (NET_DEV_RING_BUF *p_buf)
{
    p_buf->Rd++;

    if (p_buf->Rd == p_buf->Element_Nb) {
        p_buf->Rd = 0;
    }

    p_buf->Cnt--;
}


#endif  /* NET_IF_ETHER_MODULE_EN */

