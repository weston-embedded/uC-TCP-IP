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
*                                          TEXAS INSTRUMENTS
*                                 ETHERNET MEDIA ACCESS CONTROLLER (EMAC)
*
* Filename : net_dev_ti_emac.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.00 (or more recent version) is included in the project build.
*
*            (2) The following parts may use this 'net_dev_ti_emac' device driver:
*
*                    TEXAS INSTRUMENTS   RMxx         series
*                    TEXAS INSTRUMENTS   TMS570xx     series
*                    TEXAS INSTRUMENTS   AM35xx       series
*                    TEXAS INSTRUMENTS   OMAP-L1x     series
*
*            (3) This driver only uses one channel, the default channel is defined using the
*                'NET_DEV_TI_EMAC_CH_DFLT' #define.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_TI_EMAC_MODULE
#include  <Source/net.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_ti_emac.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) This driver only use one transmit and receive DMA channel. the channel used by this driver
*               is specified by the 'NET_DEV_TI_EMAC_CH_DFLT' #define. All the four channel
*               'NET_DEV_TI_EMAC_CH_DFLT' interrupts are mapped to core 0 CPU interrupt. It is possible
*               that you application MAY use the other chanel to send and receive interrupts if different
*               channels if those channels are mapped to the core 1 or core 2 interrupts lines.
*
*           (2) This driver support Tx and Rx flow control. For the RX flow control, there is a threshold
*               value, that specifies the number of free buffers required to issue a flow control
*               on incoming frames for channel 'NET_DEV_TI_EMAC_CH_DFLT'. The threshold value is
*               APPLICATION specific. These value can be configured with the 'NET_DEV_TI_EMAC_CH_THRESHOLD_VAL'
*               #define.
*********************************************************************************************************
*/

#define  NET_DEV_TI_EMAC                                 0u
#define  NET_DEV_AM35xx_EMAC                             1u
#define  NET_DEV_OMAP_L1x_EMAC                           2u
#define  NET_DEV_TMS570LCxx_EMAC                         3u

#define  NET_DEV_TI_EMAC_MII_REG_RD_WR_TO          0xFFFFFu    /* MII read write timeout.                              */
#define  NET_DEV_TI_EMAC_SOFT_RST_TO                0xFFFFu    /* Soft reset timeout.                                  */
#define  NET_DEV_TI_EMAC_NBR_CH                          8u    /* Number of channels.                                  */
#define  NET_DEV_TI_EMAC_DESC_ALIGN                      4u    /* Descriptors are aligned on a 32-bit boundary.        */
#define  NET_DEV_TI_EMAC_VAL_PHY_FREQ_MIN          2500000u    /* Minimal PHY frequency 2.5 Mhz.                       */
#define  NET_DEV_TI_EMAC_CH_DFLT                         0u    /* Default channel. (see note #1)                       */
#define  NET_DEV_TI_EMAC_CH_THRESHOLD_VAL                2u    /* Application specific threshold value.(see note #2)   */

#define  NET_DEV_TI_EMAC_CTRL_REG_OFFSET        (CPU_INT32U )(  0x0800u)
#define  NET_DEV_TI_EMAC_MDIO_REG_OFFSET        (CPU_INT32U )(  0x0900u)

#define  NET_DEV_OMAP_L1x_EMAC_MOD_REG_OFFSET   (CPU_INT32U )(  0x1000u)
#define  NET_DEV_OMAP_L1x_EMAC_MDIO_REG_OFFSET  (CPU_INT32U )(  0x2000u)

#define  NET_DEV_AM35xx_EMAC_MOD_REG_OFFSET     (CPU_INT32U )(0x010000u)
#define  NET_DEV_AM35xx_EMAC_MDIO_REG_OFFSET    (CPU_INT32U )(0x030000u)


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

#define  TI_EMAC_BIT_SOFRESET_RST               DEF_BIT_00

#define  TI_EMAC_BIT_INTCTRL_C0TXPACEEN         DEF_BIT_17

#define  TI_EMAC_BIT_CONTROL_IDLE               DEF_BIT_31
#define  TI_EMAC_BIT_CONTROL_EN                 DEF_BIT_30
#define  TI_EMAC_MSK_CONTROL_HIGHEST_USER_CH    DEF_BIT_FIELD(5u,  24u)
#define  TI_EMAC_BIT_CONTROL_PREAMBLE           DEF_BIT_20
#define  TI_EMAC_BIT_CONTROL_FAULT              DEF_BIT_19
#define  TI_EMAC_BIT_CONTROL_FAULTENB           DEF_BIT_18
#define  TI_EMAC_MSK_CONTROL_CLKDIV             DEF_BIT_FIELD(15u,  0u)

#define  TI_EMAC_MSK_LINK_FLAG_USERPHY0         DEF_BIT_00
#define  TI_EMAC_MSK_LINK_FLAG_USERPHY1         DEF_BIT_01

#define  TI_EMAC_BIT_USERACCESS_GO              DEF_BIT_31
#define  TI_EMAC_BIT_USERACCESS_WRITE           DEF_BIT_30
#define  TI_EMAC_BIT_USERACCESS_ACK             DEF_BIT_29

#define  TI_EMAC_MSK_USERACCESS_REGADDR         DEF_BIT_FIELD(5u,  21u)
#define  TI_EMAC_MSK_USERACCESS_PHYADDR         DEF_BIT_FIELD(5u,  16u)
#define  TI_EMAC_MSK_USERACCESS_DATA            DEF_BIT_FIELD(16u,  0u)

#define  TI_EMAC_BIT_PHYSEL_LINKSEL             DEF_BIT_07
#define  TI_EMAC_BIT_PHYSEL_LINKINENB           DEF_BIT_06
#define  TI_EMAC_MSK_PHYSEL_PHYADRMON           DEF_BIT_FIELD(5u,   0u)

#define  TI_EMAC_BIT_TXCONTROL_TXEN             DEF_BIT_00
#define  TI_EMAC_BIT_RXCONTROL_RXEN             DEF_BIT_00

#define  TI_EMAC_MSK_MACINVECTOR_STATPEND       DEF_BIT_27
#define  TI_EMAC_MSK_MACINVECTOR_HOSTPEND       DEF_BIT_27
#define  TI_EMAC_MSK_MACINVECTOR_USERINT        DEF_BIT_27
#define  TI_EMAC_MSK_MACINVECTOR_TXPEND         DEF_BIT_FIELD(8u,  16u)
#define  TI_EMAC_MSK_MACINVECTOR_RXTHRESHPEND   DEF_BIT_FIELD(8u,   8u)
#define  TI_EMAC_MSK_MACINVECTOR_RXPEND         DEF_BIT_FIELD(8u,   0u)

#define  TI_EMAC_BIT_MACEOIVECTOR_C0RXTHRESH    DEF_BIT_MASK (0u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C0RX          DEF_BIT_MASK (1u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C0TX          DEF_BIT_MASK (2u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C0MISC        DEF_BIT_MASK (3u,   0u)

#define  TI_EMAC_BIT_MACEOIVECTOR_C1RXTHRESH    DEF_BIT_MASK (4u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C1RX          DEF_BIT_MASK (5u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C1TX          DEF_BIT_MASK (6u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C1MISC        DEF_BIT_MASK (7u,   0u)

#define  TI_EMAC_BIT_MACEOIVECTOR_C2RXTHRESH    DEF_BIT_MASK (8u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C2RX          DEF_BIT_MASK (9u,   0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C2TX          DEF_BIT_MASK (10u,  0u)
#define  TI_EMAC_BIT_MACEOIVECTOR_C2MISC        DEF_BIT_MASK (11u,  0u)

#define  TI_EMAC_BIT_MAC_INT_FLAG_HOSTPEND      DEF_BIT_00
#define  TI_EMAC_BIT_MAC_INT_FLAG_STATPEND      DEF_BIT_01

#define  TI_EMAC_BIT_RXMBPENABLE_RXPASSCRC      DEF_BIT_30
#define  TI_EMAC_BIT_RXMBPENABLE_RXQOSEN        DEF_BIT_29
#define  TI_EMAC_BIT_RXMBPENABLE_RXNOCHAIN      DEF_BIT_28
#define  TI_EMAC_BIT_RXMBPENABLE_RXCMFEN        DEF_BIT_24
#define  TI_EMAC_BIT_RXMBPENABLE_RXCSFEN        DEF_BIT_23
#define  TI_EMAC_BIT_RXMBPENABLE_RXCEFEN        DEF_BIT_22
#define  TI_EMAC_BIT_RXMBPENABLE_RXCAFEN        DEF_BIT_21
#define  TI_EMAC_MSK_RXMBPENABLE_RXPROMCH       DEF_BIT_FIELD(3u,  16u)
#define  TI_EMAC_BIT_RXMBPENABLE_RXBROADEN      DEF_BIT_13
#define  TI_EMAC_MSK_RXMBPENABLE_RXBROADCH      DEF_BIT_FIELD(3u,   8u)
#define  TI_EMAC_BIT_RXMBPENABLE_RXMULTEN       DEF_BIT_05
#define  TI_EMAC_MSK_RXMBPENABLE_RXMULTCH       DEF_BIT_FIELD(3u,   8u)

#define  TI_EMAC_BIT_MACCONTROL_RMIISPEED       DEF_BIT_15
#define  TI_EMAC_BIT_MACCONTROL_RXOFFLENBLOCK   DEF_BIT_14
#define  TI_EMAC_BIT_MACCONTROL_RXOWNERSHIP     DEF_BIT_13
#define  TI_EMAC_BIT_MACCONTROL_CMDIDLE         DEF_BIT_11
#define  TI_EMAC_BIT_MACCONTROL_TXSHORTGAPEN    DEF_BIT_10
#define  TI_EMAC_BIT_MACCONTROL_TXPTYPE         DEF_BIT_09
#define  TI_EMAC_BIT_MACCONTROL_TXPACE          DEF_BIT_06
#define  TI_EMAC_BIT_MACCONTROL_GMIIEN          DEF_BIT_05
#define  TI_EMAC_BIT_MACCONTROL_TXFLOWEN        DEF_BIT_04
#define  TI_EMAC_BIT_MACCONTROL_RXBUFFERFLOWEN  DEF_BIT_03
#define  TI_EMAC_BIT_MACCONTROL_LOOPBACK        DEF_BIT_01
#define  TI_EMAC_BIT_MACCONTROL_FULLDUPLEX      DEF_BIT_00

#define  TI_EMAC_BIT_MACSTATUS_IDLE             DEF_BIT_31
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_NONE      DEF_BIT_MASK (0u,  20u)
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_SOP       DEF_BIT_MASK (1u,  20u)
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_OWN       DEF_BIT_MASK (2u,  20u)
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_BUF_NEXT  DEF_BIT_MASK (3u,  20u)
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_BUF_PTR   DEF_BIT_MASK (4u,  20u)
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_BUF_LEN   DEF_BIT_MASK (5u,  20u)
#define  TI_EMAC_BIT_MACSTATUS_TX_ERR_PKT_LEN   DEF_BIT_MASK (6u,  20u)
#define  TI_EMAC_MSK_MACSTATUS_TX_ERRCODE       DEF_BIT_MASK (15u, 20u)
#define  TI_EMAC_MSK_MACSTATUS_TXERRCH          DEF_BIT_FIELD(3u,  16u)
#define  TI_EMAC_BIT_MACSTATUS_RX_ERR_NONE      DEF_BIT_MASK (0u,  12u)
#define  TI_EMAC_BIT_MACSTATUS_RX_ERR_OWN       DEF_BIT_MASK (2u,  12u)
#define  TI_EMAC_BIT_MACSTATUS_RX_ERR_BUF_PTR   DEF_BIT_MASK (4u,  12u)
#define  TI_EMAC_MSK_MACSTATUS_RX_ERRCODE       DEF_BIT_MASK (15u, 12u)
#define  TI_EMAC_MSK_MACSTATUS_RXERRCH          DEF_BIT_FIELD(3u,  12u)

#define  TI_EMAC_BIT_MACSTATUS_RXQOSACT         DEF_BIT_00
#define  TI_EMAC_BIT_MACSTATUS_RXFLOWACT        DEF_BIT_01
#define  TI_EMAC_BIT_MACSTATUS_TXFLOWACT        DEF_BIT_00

#define  TI_EMAC_BIT_EMCONTROL_SOFT             DEF_BIT_01
#define  TI_EMAC_BIT_EMCONTROL_FREE             DEF_BIT_00

#define  TI_EMAC_BIT_MACADDRLO_VALID            DEF_BIT_20
#define  TI_EMAC_BIT_MACADDRLO_MATCHFILT        DEF_BIT_19
#define  TI_EMAC_MSK_MACADDRLO_CH               DEF_BIT_MASK (3u,  16u)

#define  TI_EMAC_BIT_DESC_FLAG_SOP              DEF_BIT_31
#define  TI_EMAC_BIT_DESC_FLAG_EOP              DEF_BIT_30
#define  TI_EMAC_BIT_DESC_FLAG_OWNER            DEF_BIT_29
#define  TI_EMAC_BIT_DESC_FLAG_EOQ              DEF_BIT_28
#define  TI_EMAC_BIT_DESC_FLAG_TDOWNCMPLT       DEF_BIT_27
#define  TI_EMAC_BIT_DESC_FLAG_PASSCRC          DEF_BIT_26
#define  TI_EMAC_BIT_DESC_FLAG_JABBER           DEF_BIT_25
#define  TI_EMAC_BIT_DESC_FLAG_OVERSIZE         DEF_BIT_24

#define  TI_EMAC_BIT_DESC_FLAG_FRAGMENT         DEF_BIT_23
#define  TI_EMAC_BIT_DESC_FLAG_UNDERSIZED       DEF_BIT_22
#define  TI_EMAC_BIT_DESC_FLAG_CONTROL          DEF_BIT_21
#define  TI_EMAC_BIT_DESC_FLAG_OVERRUN          DEF_BIT_20
#define  TI_EMAC_BIT_DESC_FLAG_CODEERROR        DEF_BIT_19
#define  TI_EMAC_BIT_DESC_FLAG_ALIGNERROR       DEF_BIT_18
#define  TI_EMAC_BIT_DESC_FLAG_CRCERROR         DEF_BIT_17
#define  TI_EMAC_BIT_DESC_FLAG_NOMATCH          DEF_BIT_16
#define  TI_EMAC_MSK_DESC_PKT_LEN               DEF_BIT_FIELD(16u,  0u)


#define  TI_EMAC_BIT_DESC_FLAG_RX_ERR          (TI_EMAC_BIT_DESC_FLAG_JABBER     | \
                                                TI_EMAC_BIT_DESC_FLAG_OVERSIZE   | \
                                                TI_EMAC_BIT_DESC_FLAG_FRAGMENT   | \
                                                TI_EMAC_BIT_DESC_FLAG_UNDERSIZED | \
                                                TI_EMAC_BIT_DESC_FLAG_OVERRUN    | \
                                                TI_EMAC_BIT_DESC_FLAG_CODEERROR  | \
                                                TI_EMAC_BIT_DESC_FLAG_ALIGNERROR | \
                                                TI_EMAC_BIT_DESC_FLAG_CRCERROR   | \
                                                TI_EMAC_BIT_DESC_FLAG_NOMATCH)


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) Instance specific data area structures should be defined below.  The data area
*               structure typically includes error counters and variables used to track the
*               state of the device.  Variables required for correct operation of the device
*               MUST NOT be defined globally and should instead be included within the instance
*               specific data area structure and referenced as pif->Dev_Data structure members.
*********************************************************************************************************
*/

typedef  struct  dev_desc  DEV_DESC;
                                                                /* ------------- DMA DESCRIPTOR DATA TYPE ------------- */
struct  dev_desc {
    DEV_DESC    *NextPtr;                                       /* Pointer to next descriptor in chain.                 */
    CPU_INT08U  *BufPtr;                                        /* Pointer to data buffer.                              */
    CPU_REG32    BufLen;                                        /* Buffer offset (MSW) and length (LSW).                */
    CPU_REG32    PktFlagsLen;                                   /* Packet flags (MSW) and length (LSW).                 */
};

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR     StatRxCtrRx;
    NET_CTR     StatRxCtrRxISR;
    NET_CTR     StatTxCtrTxISR;
    NET_CTR     StatRxCtrTaskSignalSelf;
#endif

    MEM_POOL    RxDescPool;
    MEM_POOL    TxDescPool;
    DEV_DESC   *RxDescStartPtr;
    DEV_DESC   *RxDescCurPtr;
    DEV_DESC   *RxDescLastPtr;

    DEV_DESC   *TxDescStartPtr;
    DEV_DESC   *TxDescFreeHeadPtr;
    DEV_DESC   *TxDescFreeTailPtr;
    DEV_DESC   *TxDescEMACHeadPtr;
    DEV_DESC   *TxDescEMACTailPtr;
    DEV_DESC   *TxDescEOQPtr;
    DEV_DESC   *TxDescLastHDPPtr;

                                                                /* -------------- RECEIVE ERROR COUNTERS -------------- */
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR     ErrRxDataAreaAllocCtr;                          /* Rx buffer is not available.                          */
#endif
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U  MulticastAddrHashBitCtr[64];
#endif
    CPU_ADDR    EMAC_BaseAddr;
    CPU_ADDR    EMACCtrl_BaseAddr;
    CPU_ADDR    MDIO_BaseAddr;

    CPU_INT32U  FreeBuf;
    CPU_INT08U  ByteSwap;

    CPU_INT32U  ErrRxInvalidSizeCtr;

    CPU_INT32U  ErrTxCtr;

    CPU_INT32U  ErrRxCtr;

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

typedef  struct  net_dev_emac_ctrl {
                                                                /* ----------- EMAC CONTROL MODULE REGISTERS ---------- */
    CPU_REG32  REVID;                                           /* Revision ID.                                         */
    CPU_REG32  SOFTRESET;                                       /* Software reset.                                      */
    CPU_REG32  RSVD_00;
    CPU_REG32  INTCTRL;                                         /* Interrupt control.                                   */
    CPU_REG32  C0RXTHRESHEN;                                    /* Core 0 receive threshold interrupt enable.           */
    CPU_REG32  C0RXEN;                                          /* Core 0 receive interrupt enable.                     */
    CPU_REG32  C0TXEN;                                          /* Core 0 transmit interrupt enable.                    */
    CPU_REG32  C0MISCEN;                                        /* Core 0 miscellaneous interrupt enable.               */
    CPU_REG32  C1RXTHRESHEN;                                    /* Core 1 receive threshold interrupt enable.           */
    CPU_REG32  C1RXEN;                                          /* Core 1 receive interrupt enable.                     */
    CPU_REG32  C1TXEN;                                          /* Core 1 transmit interrupt enable.                    */
    CPU_REG32  C1MISCEN;                                        /* Core 1 miscellaneous interrupt enable.               */
    CPU_REG32  C2RXTHRESHEN;                                    /* Core 2 receive threshold interrupt enable.           */
    CPU_REG32  C2RXEN;                                          /* Core 2 receive interrupt enable.                     */
    CPU_REG32  C2TXEN;                                          /* Core 2 transmit interrupt enable.                    */
    CPU_REG32  C2MISCEN;                                        /* Core 2 miscellaneous interrupt enable.               */
    CPU_REG32  C0RXTHRESHSTAT;                                  /* Core 0 receive threshold interrupt status.           */
    CPU_REG32  C0RXSTAT;                                        /* Core 0 receive interrupt status.                     */
    CPU_REG32  C0TXSTAT;                                        /* Core 0 transmit interrupt status.                    */
    CPU_REG32  C0MISCSTAT;                                      /* Core 0 miscellaneous interrupt status.               */
    CPU_REG32  C1RXTHRESHSTAT;                                  /* Core 1 receive threshold interrupt status.           */
    CPU_REG32  C1RXSTAT;                                        /* Core 1 receive interrupt status.                     */
    CPU_REG32  C1TXSTAT;                                        /* Core 1 transmit interrupt status.                    */
    CPU_REG32  C1MISCSTAT;                                      /* Core 1 miscellaneous interrupt status.               */
    CPU_REG32  C2RXTHRESHSTAT;                                  /* Core 2 receive threshold interrupt status.           */
    CPU_REG32  C2RXSTAT;                                        /* Core 2 receive interrupt status.                     */
    CPU_REG32  C2TXSTAT;                                        /* Core 2 transmit interrupt status.                    */
    CPU_REG32  C2MISCSTAT;                                      /* Core 2 miscellaneous interrupt status.               */
    CPU_REG32  C0RXIMAX;                                        /* Core 0 receive interrupts per milliseconds.          */
    CPU_REG32  C0TXIMAX;                                        /* Core 0 trasmit interrupts per milliseconds.          */
    CPU_REG32  C1RXIMAX;                                        /* Core 1 receive interrupts per milliseconds.          */
    CPU_REG32  C1TXIMAX;                                        /* Core 1 trasmit interrupts per milliseconds.          */
    CPU_REG32  C2RXIMAX;                                        /* Core 2 receive interrupts per milliseconds.          */
    CPU_REG32  C2TXIMAX;                                        /* Core 2 trasmit interrupts per milliseconds.          */
} NET_DEV_EMAC_CTRL;


typedef  struct  net_dev_emac {
                                                                /* ------------------ EMAC REGISTERS ------------------ */
    CPU_REG32  TXREVID;                                         /* Transmit revision ID.                                */
    CPU_REG32  TXCONTROL;                                       /* Transmit control.                                    */
    CPU_REG32  TXTEARDOWN;                                      /* Transmit teardown.                                   */
    CPU_REG32  RSVD_02;

    CPU_REG32  RXREVID;                                         /* Receive  revision ID.                                */
    CPU_REG32  RXCONTROL;                                       /* Receive  control.                                    */
    CPU_REG32  RXTEARDOWN;                                      /* Receive  teardown.                                   */
    CPU_REG32  RSVD_03[25];

    CPU_REG32  TXINTSTATRAW;                                    /* Transmit Interrupt Status (Unmasked) Register.       */
    CPU_REG32  TXINTSTATMASKED;                                 /* Transmit Interrupt Status (Masked) Register.         */
    CPU_REG32  TXINTMASKSET;                                    /* Transmit Interrupt Mask Set Register.                */
    CPU_REG32  TXINTMASKCLEAR;                                  /* Transmit Interrupt Clear Register.                   */
    CPU_REG32  MACINVECTOR;                                     /* MAC Input Vector Register.                           */
    CPU_REG32  MACEOIVECTOR;                                    /* MAC End Of Interrupt Vector Register.                */
    CPU_REG32  RSVD_04[2];
    CPU_REG32  RXINTSTATRAW;                                    /* Receive Interrupt Status (Unmasked) Register.        */
    CPU_REG32  RXINTSTATMASKED;                                 /* Receive Interrupt Status (Masked) Register.          */
    CPU_REG32  RXINTMASKSET;                                    /* Receive Interrupt Mask Set Register.                 */
    CPU_REG32  RXINTMASKCLEAR;                                  /* Receive Interrupt Mask Clear Register.               */
    CPU_REG32  MACINTSTATRAW;                                   /* MAC Interrupt Status (Unmasked) Register.            */
    CPU_REG32  MACINTSTATMASKED;                                /* MAC Interrupt Status (Masked) Register.              */
    CPU_REG32  MACINTMASKSET;                                   /* MAC Interrupt Mask Set Register.                     */
    CPU_REG32  MACINTMASKCLEAR;                                 /* MAC Interrupt Mask Clear Register.                   */
    CPU_REG32  RSVD_05[16];
    CPU_REG32  RXMBPENABLE;                                     /* Receive Multi/Broad/Promiscuous Channel Enable Reg.  */
    CPU_REG32  RXUNICASTSET;                                    /* Receive Unicast Enable Set Register.                 */
    CPU_REG32  RXUNICASTCLEAR;                                  /* Receive Unicast Clear Register.                      */
    CPU_REG32  RXMAXLEN;                                        /* Receive Maximum Length Register.                     */
    CPU_REG32  RXBUFFEROFFSET;                                  /* Receive Buffer Offset Register.                      */
    CPU_REG32  RXFILTERLOWTHRESH;                               /* Receive Filter Low Priority Frame Threshold Register.*/

    CPU_REG32  RSVD_06[2];

    CPU_REG32  RXnFLOWTHRESH[NET_DEV_TI_EMAC_NBR_CH];           /* Receive Channel x Flow Control Threshold Register    */
    CPU_REG32  RXnFREEBUFFER[NET_DEV_TI_EMAC_NBR_CH];           /* Receive Channel x Free buffer Threshold Register     */

    CPU_REG32  MACCONTROL;                                      /* MAC Control Register.                                */
    CPU_REG32  MACSTATUS;                                       /* MAC Status Register.                                 */
    CPU_REG32  EMCONTROL;                                       /* Emulation Control Register.                          */
    CPU_REG32  FIFOCONTROL;                                     /* FIFO Control Register.                               */
    CPU_REG32  MACCONFIG;                                       /* MAC Configuration Register.                          */
    CPU_REG32  MACSOFTRESET;                                    /* Soft Reset Register.                                 */

    CPU_REG32  RSVD_07[22];

    CPU_REG32  MACSRCADDRLO;                                    /* MAC Source Address Low Bytes Register                */
    CPU_REG32  MACSRCADDRHI;                                    /* MAC Source Address High Bytes Register               */
    CPU_REG32  MACHASH1;                                        /* MAC  Hash Address Register 1                         */
    CPU_REG32  MACHASH2;                                        /* MAC Hash Address Register 2                          */
    CPU_REG32  BOFFTEST;                                        /* Back Off Test Register                               */
    CPU_REG32  TPACETEST;                                       /* Transmit Pacing Algorithm Test Register              */
    CPU_REG32  RXPAUSE;                                         /* Receive Pause Timer Register                         */
    CPU_REG32  TXPAUSE;                                         /* Transmit Pause Timer Register                        */
    CPU_REG32  RSVD_08[4];

                                                                /* ------------ EMAC STATISTICS REGISTERS ------------- */
    CPU_REG32  RXGOODFRAMES;                                    /* Good Receive Frames Register.                        */
    CPU_REG32  RXBCASTFRAMES;                                   /* Broadcast Receive Frames Register.                   */
    CPU_REG32  RXMCASTFRAMES;                                   /* Multicast Receive Frames Register.                   */
    CPU_REG32  RXPAUSEFRAMES;                                   /* Pause Receive Frames Register.                       */
    CPU_REG32  RXCRCERRORS;                                     /* Receive CRC Errors Register.                         */
    CPU_REG32  RXALIGNCODEERRORS;                               /* Receive Alignment/Code Errors Register.              */
    CPU_REG32  RXOVERSIZED;                                     /* Receive Oversized Frames Register.                   */
    CPU_REG32  RXJABBER;                                        /* Receive Jabber Frames Register.                      */
    CPU_REG32  RXUNDERSIZED;                                    /* Receive Undersized Frames Register.                  */
    CPU_REG32  RXFRAGMENTS;                                     /* Receive Frame Fragments Register.                    */
    CPU_REG32  RXFILTERED;                                      /* Filtered Receive Frames Register.                    */
    CPU_REG32  RXQOSFILTERED;                                   /* Received QOS Filtered Frames Register                */
    CPU_REG32  RXOCTETS;                                        /* Receive Octet Frames Register.                       */
    CPU_REG32  TXGOODFRAMES;                                    /* Good Transmit Frames Register.                       */
    CPU_REG32  TXBCASTFRAMES;                                   /* Broadcast Transmit Frames Register.                  */
    CPU_REG32  TXMCASTFRAMES;                                   /* Multicast Transmit Frames Register.                  */
    CPU_REG32  TXPAUSEFRAMES;                                   /* Pause Transmit Frames Register.                      */
    CPU_REG32  TXDEFERRED;                                      /* Deferred Transmit Frames Register.                   */
    CPU_REG32  TXCOLLISION;                                     /* Transmit Collision Frames Register.                  */
    CPU_REG32  TXSINGLECOLL;                                    /* Transmit Single Collision Frames Register.           */
    CPU_REG32  TXMULTICOLL;                                     /* Transmit Multiple Collision Frames Register.         */
    CPU_REG32  TXEXCESSIVECOLL;                                 /* Transmit Excessive Collision Frames Register.        */
    CPU_REG32  TXLATECOLL;                                      /* Transmit Late Collision Frames Register.             */
    CPU_REG32  TXUNDERRUN;                                      /* Transmit Underrun Error Register.                    */
    CPU_REG32  TXCARRIERSENSE;                                  /* Transmit Carrier Sense Errors Register.              */
    CPU_REG32  TXOCTETS;                                        /* Transmit Octet Frames Register.                      */
    CPU_REG32  FRAME64;                                         /* Transmit and Receive 64 Octet Frames Register        */
    CPU_REG32  FRAME65T127;                                     /* Transmit and Receive 65 to 127 Octet Frames          */
    CPU_REG32  FRAME128T255;                                    /* Transmit and Receive 128 to 255 Octet Frames         */
    CPU_REG32  FRAME256T511;                                    /* Transmit and Receive 256 to 511 Octet Frames reg.    */
    CPU_REG32  FRAME512T1023;                                   /* Transmit and Receive 512 to 1023 Octet Frames reg.   */
    CPU_REG32  FRAME1024TUP;                                    /* Transmit and Receive 1024 to 1518 Octet Frames reg.  */
    CPU_REG32  NETOCTETS;                                       /* Network Octet Frames Register.                       */
    CPU_REG32  RXSOFOVERRUNS;                                   /* Receive FIFO or DMA Start of Frame Overruns Reg.     */
    CPU_REG32  RXMOFOVERRUNS;                                   /* Receive FIFO or DMA Middle of Frame Overruns Reg.    */
    CPU_REG32  RXDMAOVERRUNS;                                   /* Receive DMA Start of Frame and Middle of Frame ...   */
                                                                /* ... overrun register.                                */
    CPU_REG32  RSVD_09[156];

    CPU_REG32  MACADDRLO;                                       /* MAC Address Low Bytes Register                       */
    CPU_REG32  MACADDRHI;                                       /* MAC Address High Bytes Register                      */
    CPU_REG32  MACINDEX;                                        /* MAC Index Register                                   */

    CPU_REG32  RSVD_10[61];

    CPU_REG32  TXnHDP[NET_DEV_TI_EMAC_NBR_CH];                  /* Transmit Channel n DMA Head Descriptor Pointer Reg.   */
    CPU_REG32  RXnHDP[NET_DEV_TI_EMAC_NBR_CH];                  /* Receive  Channel n DMA Head Descriptor Pointer Reg.   */

    CPU_REG32  TXnCP[NET_DEV_TI_EMAC_NBR_CH];                   /* Transmit Channel n Completion Pointer Register.       */
    CPU_REG32  RXnCP[NET_DEV_TI_EMAC_NBR_CH];                   /* Receive  Channel n Completion Pointer Register.       */

    CPU_REG32  RSVD_11[608];
} NET_DEV_EMAC;


typedef  struct  net_dev_mdio {
                                                                /* ------------------ MDIO REGISTERS ------------------ */
    CPU_REG32  MDIO_REVID;                                      /* MDIO Revision ID Register                            */
    CPU_REG32  CONTROL;                                         /* MDIO Control Register                                */
    CPU_REG32  ALIVE;                                           /* PHY Alive Status register                            */
    CPU_REG32  LINK;                                            /* PHY Link Status register                             */
    CPU_REG32  LINKINTRAW;                                      /* MDIO Link Status Change Interrupt (Unmasked)         */
    CPU_REG32  LINKINTMASKED;                                   /* MDIO Link Status Change Interrupt (Masked)           */

    CPU_REG32  RSVD_12[2];

    CPU_REG32  USERINTRAW;                                      /* MDIO User Command Complete Interrupt (Unmasked).     */
    CPU_REG32  USERINTMASKED;                                   /* MDIO User Command Complete Interrupt (Masked)        */
    CPU_REG32  USERINTMASKSET;                                  /* MDIO User Command Complete Interrupt Mask Set.       */
    CPU_REG32  USERINTMASKCLEAR;                                /* MDIO User Command Complete Interrupt Mask Clear.     */

    CPU_REG32  RSVD_13[20];

    CPU_REG32  USERACCESS0;                                     /* MDIO User Access Register.                           */
    CPU_REG32  USERPHYSEL0;                                     /* MDIO User PHY Select Register 0                      */
    CPU_REG32  USERACCESS1;                                     /* MDIO User Access Register 1                          */
    CPU_REG32  USERPHYSEL1;                                     /* MDIO User PHY Select Register 1                      */
} NET_DEV_MDIO;


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
static  void        NetDev_TI_EMAC_Init        (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_AM35xx_EMAC_Init    (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_OMAP_L1x_EMAC_Init  (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_TMS570LCxx_EMAC_Init(NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_InitHandler         (NET_IF             *pif,
                                                CPU_INT08U          emac_dev_type,
                                                NET_ERR            *perr);

static  void        NetDev_Start               (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_Stop                (NET_IF             *pif,
                                                NET_ERR            *perr);


static  void        NetDev_Rx                  (NET_IF             *pif,
                                                CPU_INT08U        **p_data,
                                                CPU_INT16U         *size,
                                                NET_ERR            *perr);

static  void        NetDev_Tx                  (NET_IF             *pif,
                                                CPU_INT08U         *p_data,
                                                CPU_INT16U          size,
                                                NET_ERR            *perr);


static  void        NetDev_AddrMulticastAdd    (NET_IF             *pif,
                                                CPU_INT08U         *paddr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *perr);

static  void        NetDev_AddrMulticastRemove (NET_IF             *pif,
                                                CPU_INT08U         *paddr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *perr);


static  void        NetDev_ISR_Handler         (NET_IF             *pif,
                                                NET_DEV_ISR_TYPE    type);


static  void        NetDev_IO_Ctrl             (NET_IF             *pif,
                                                CPU_INT08U          opt,
                                                void               *p_data,
                                                NET_ERR            *perr);


static  void        NetDev_MII_Rd              (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *perr);

static  void        NetDev_MII_Wr              (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);


                                                                       /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
static  void        NetDev_RxDescInit          (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_RxDescFreeAll       (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_RxDescPtrCurInc     (NET_IF             *pif);


static  void        NetDev_TxDescInit          (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void        NetDev_TxDescFreeAll       (NET_IF             *pif,
                                                NET_ERR            *perr);

static  CPU_INT32U  NetDev_SwapData            (CPU_INT32U          word,
                                                CPU_INT08U          byte_swap);


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
                                                                                       /* TI_EMAC dev API fnct ptrs :         */
const  NET_DEV_API_ETHER  NetDev_API_TI_EMAC         = { NetDev_TI_EMAC_Init,          /*   Init/add                          */
                                                         NetDev_Start,                 /*   Start                             */
                                                         NetDev_Stop,                  /*   Stop                              */
                                                         NetDev_Rx,                    /*   Rx                                */
                                                         NetDev_Tx,                    /*   Tx                                */
                                                         NetDev_AddrMulticastAdd,      /*   Multicast addr add                */
                                                         NetDev_AddrMulticastRemove,   /*   Multicast addr remove             */
                                                         NetDev_ISR_Handler,           /*   ISR handler                       */
                                                         NetDev_IO_Ctrl,               /*   I/O ctrl                          */
                                                         NetDev_MII_Rd,                /*   Phy reg rd                        */
                                                         NetDev_MII_Wr                 /*   Phy reg wr                        */
                                                       };


                                                                                       /* AM35xx_EMAC dev API fnct ptrs :     */
const  NET_DEV_API_ETHER  NetDev_API_AM35xx_EMAC     = { NetDev_AM35xx_EMAC_Init,      /*   Init/add                          */
                                                         NetDev_Start,                 /*   Start                             */
                                                         NetDev_Stop,                  /*   Stop                              */
                                                         NetDev_Rx,                    /*   Rx                                */
                                                         NetDev_Tx,                    /*   Tx                                */
                                                         NetDev_AddrMulticastAdd,      /*   Multicast addr add                */
                                                         NetDev_AddrMulticastRemove,   /*   Multicast addr remove             */
                                                         NetDev_ISR_Handler,           /*   ISR handler                       */
                                                         NetDev_IO_Ctrl,               /*   I/O ctrl                          */
                                                         NetDev_MII_Rd,                /*   Phy reg rd                        */
                                                         NetDev_MII_Wr                 /*   Phy reg wr                        */
                                                       };


                                                                                       /* OMAP-L1x_EMAC dev API fnct ptrs :   */
const  NET_DEV_API_ETHER  NetDev_API_OMAP_L1x_EMAC   = { NetDev_OMAP_L1x_EMAC_Init,    /*   Init/add                          */
                                                         NetDev_Start,                 /*   Start                             */
                                                         NetDev_Stop,                  /*   Stop                              */
                                                         NetDev_Rx,                    /*   Rx                                */
                                                         NetDev_Tx,                    /*   Tx                                */
                                                         NetDev_AddrMulticastAdd,      /*   Multicast addr add                */
                                                         NetDev_AddrMulticastRemove,   /*   Multicast addr remove             */
                                                         NetDev_ISR_Handler,           /*   ISR handler                       */
                                                         NetDev_IO_Ctrl,               /*   I/O ctrl                          */
                                                         NetDev_MII_Rd,                /*   Phy reg rd                        */
                                                         NetDev_MII_Wr                 /*   Phy reg wr                        */
                                                       };


                                                                                       /* TMS570LCxx_EMAC dev API fnct ptrs : */
const  NET_DEV_API_ETHER  NetDev_API_TMS570LCxx_EMAC = { NetDev_TMS570LCxx_EMAC_Init,  /*   Init/add                          */
                                                         NetDev_Start,                 /*   Start                             */
                                                         NetDev_Stop,                  /*   Stop                              */
                                                         NetDev_Rx,                    /*   Rx                                */
                                                         NetDev_Tx,                    /*   Tx                                */
                                                         NetDev_AddrMulticastAdd,      /*   Multicast addr add                */
                                                         NetDev_AddrMulticastRemove,   /*   Multicast addr remove             */
                                                         NetDev_ISR_Handler,           /*   ISR handler                       */
                                                         NetDev_IO_Ctrl,               /*   I/O ctrl                          */
                                                         NetDev_MII_Rd,                /*   Phy reg rd                        */
                                                         NetDev_MII_Wr                 /*   Phy reg wr                        */
                                                       };


/*
*********************************************************************************************************
*                                        NetDev_TI_EMAC_Init()
*
* Description : Initialize generic TI_EMAC devices.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_TI_EMAC_Init (NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_TI_EMAC, perr);
}


/*
*********************************************************************************************************
*                                      NetDev_AM35xx_EMAC_Init()
*
* Description : Initialize generic TI_EMAC AM35xx devices.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AM35xx_EMAC_Init (NET_IF   *pif,
                                       NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_AM35xx_EMAC, perr);
}


/*
*********************************************************************************************************
*                                     NetDev_OMAP_L1x_EMAC_Init()
*
* Description : Initialize generic TI_EMAC OMAP-L1x devices.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_OMAP_L1x_EMAC_Init (NET_IF   *pif,
                                         NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_OMAP_L1x_EMAC, perr);
}


/*
*********************************************************************************************************
*                                    NetDev_TMS570LCxx_EMAC_Init()
*
* Description : Initialize generic TI_EMAC TMS570LCxx device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_TMS570LCxx_EMAC_Init (NET_IF   *pif,
                                           NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_TMS570LCxx_EMAC, perr);
}


/*
*********************************************************************************************************
*                                        NetDev_InitHandler()
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
* Argument(s) : pif            Pointer to the interface requiring service.
*
*               mac_dev_type   TI_EMAC device type :
*
*                                   NET_DEV_TI_EMAC              Generic TI_EMAC         device.
*                                   NET_DEV_AM35xx_EMAC          Generic AM35xx_EMAC     device.
*                                   NET_DEV_OMAP_L1x_EMAC        Generic OMAP_L1x_EMAC   device.
*                                   NET_DEV_TMS570LCxx_EMAC      Generic TMS570LCxx_EMAC device.
*
*               perr           Pointer to return error code.
*                                   NET_DEV_ERR_NONE             No Error.
*                                   NET_DEV_ERR_INIT             General initialization error.
*                                   NET_BUF_ERR_POOL_MEM_ALLOC   Memory allocation failed.
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

static  void  NetDev_InitHandler (NET_IF      *pif,
                                  CPU_INT08U   emac_dev_type,
                                  NET_ERR     *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev_emac;
    NET_DEV_EMAC_CTRL  *pdev_emac_ctrl;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbr_octets;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */

    pphy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
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


    if (pphy_cfg == (void *)0) {
       *perr = NET_PHY_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate phy bus mode.                               */
    if ((pphy_cfg->BusMode != NET_PHY_BUS_MODE_RMII) &&
        (pphy_cfg->BusMode != NET_PHY_BUS_MODE_MII )) {
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

    pdev_data->FreeBuf = pdev_cfg->RxDescNbr;
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->StatRxCtrRxISR = 0u;
    pdev_data->StatRxCtrRx    = 0u;
#endif

                                                                /* Get base addr : EMAC reg., EMAC control, MDIO reg.   */
    switch (emac_dev_type) {
        case NET_DEV_OMAP_L1x_EMAC:
             pdev_emac                    = (NET_DEV_EMAC      *)(pdev_cfg->BaseAddr + NET_DEV_OMAP_L1x_EMAC_MOD_REG_OFFSET);
             pdev_emac_ctrl               = (NET_DEV_EMAC_CTRL *)(pdev_cfg->BaseAddr);

             pdev_data->EMAC_BaseAddr     = (pdev_cfg->BaseAddr + NET_DEV_OMAP_L1x_EMAC_MOD_REG_OFFSET);
             pdev_data->EMACCtrl_BaseAddr = (pdev_cfg->BaseAddr);
             pdev_data->MDIO_BaseAddr     = (pdev_cfg->BaseAddr + NET_DEV_OMAP_L1x_EMAC_MDIO_REG_OFFSET);
             pdev_data->ByteSwap          =  DEF_NO;
             break;


        case NET_DEV_AM35xx_EMAC:
             pdev_emac                    = (NET_DEV_EMAC      *)(pdev_cfg->BaseAddr + NET_DEV_AM35xx_EMAC_MOD_REG_OFFSET);
             pdev_emac_ctrl               = (NET_DEV_EMAC_CTRL *)(pdev_cfg->BaseAddr);

             pdev_data->EMAC_BaseAddr     = (pdev_cfg->BaseAddr + NET_DEV_AM35xx_EMAC_MOD_REG_OFFSET);
             pdev_data->EMACCtrl_BaseAddr = (pdev_cfg->BaseAddr);
             pdev_data->MDIO_BaseAddr     = (pdev_cfg->BaseAddr + NET_DEV_AM35xx_EMAC_MDIO_REG_OFFSET);
             pdev_data->ByteSwap          =  DEF_NO;
             break;


        case NET_DEV_TMS570LCxx_EMAC:
        case NET_DEV_TI_EMAC:
        default:
             pdev_emac                    = (NET_DEV_EMAC      *)(pdev_cfg->BaseAddr);
             pdev_emac_ctrl               = (NET_DEV_EMAC_CTRL *)(pdev_cfg->BaseAddr + NET_DEV_TI_EMAC_CTRL_REG_OFFSET);

             pdev_data->EMAC_BaseAddr     =  pdev_cfg->BaseAddr;
             pdev_data->EMACCtrl_BaseAddr = (pdev_cfg->BaseAddr + NET_DEV_TI_EMAC_CTRL_REG_OFFSET);
             pdev_data->MDIO_BaseAddr     = (pdev_cfg->BaseAddr + NET_DEV_TI_EMAC_MDIO_REG_OFFSET);

             if (emac_dev_type == NET_DEV_TMS570LCxx_EMAC) {
                 pdev_data->ByteSwap = DEF_YES;
             } else {
                 pdev_data->ByteSwap = DEF_NO;
             }
             break;
    }

#ifdef NET_MCAST_MODULE_EN
    Mem_Clr((void *)&pdev_data->MulticastAddrHashBitCtr[0], sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif

                                                                /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
    nbr_octets = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->RxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) pdev_cfg->MemAddr,            /* From dedicated memory.                               */
                   (CPU_SIZE_T  ) pdev_cfg->MemSize,            /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all Rx descriptors.  */
                   (CPU_SIZE_T  ) NET_DEV_TI_EMAC_DESC_ALIGN,  /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    nbr_octets = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);        /* Determine block size.                                */
    Mem_PoolCreate((MEM_POOL   *)&pdev_data->TxDescPool,        /* Pass a pointer to the mem pool to create.            */
                   (void       *) pdev_cfg->MemAddr,            /* From dedicated memory.                               */
                   (CPU_SIZE_T  ) pdev_cfg->MemSize,            /* Generic pool is of unknown size.                     */
                   (CPU_SIZE_T  ) 1,                            /* Allocate 1 block.                                    */
                   (CPU_SIZE_T  ) nbr_octets,                   /* Block size large enough to hold all Rx descriptors.  */
                   (CPU_SIZE_T  ) NET_DEV_TI_EMAC_DESC_ALIGN,  /* Block alignment (see Note #8).                       */
                   (CPU_SIZE_T *)&reqd_octets,                  /* Optional, ptr to variable to store rem nbr bytes.    */
                   (LIB_ERR    *)&lib_err);                     /* Ptr to variable to return an error code.             */
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

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

                                                                /* ------------- INITIALIZE DEV REGISTERS ------------- */
    DEF_BIT_SET(pdev_emac_ctrl->SOFTRESET, TI_EMAC_BIT_SOFRESET_RST);   /* Perform a software reset.                    */
    DEF_BIT_CLR(pdev_emac_ctrl->SOFTRESET, TI_EMAC_BIT_SOFRESET_RST);   /* Clear reset condition.                       */

    DEF_BIT_CLR(pdev_emac_ctrl->C0RXTHRESHEN, DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0TXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));

                                                                /* Disable interrupts.                                  */
    pdev_emac->TXINTMASKCLEAR = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);
    pdev_emac->RXINTMASKCLEAR = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);

    DEF_BIT_CLR(pdev_emac->TXCONTROL, TI_EMAC_BIT_TXCONTROL_TXEN);  /* Disable TX.                                      */
    DEF_BIT_CLR(pdev_emac->RXCONTROL, TI_EMAC_BIT_RXCONTROL_RXEN);  /* Disable RX.                                      */

    pdev_emac->MACHASH1 = 0u;                                   /* Clear MAC hash registers.                            */
    pdev_emac->MACHASH2 = 0u;

                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }
                                                                /* ------- INITIALIZE ERROR AND DEBUG COUNTERS -------- */
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    pdev_data->RxDescStartPtr    = (DEV_DESC *)0;
    pdev_data->RxDescCurPtr      = (DEV_DESC *)0;
    pdev_data->RxDescLastPtr     = (DEV_DESC *)0;

    pdev_data->TxDescStartPtr    = (DEV_DESC *)0;
    pdev_data->TxDescEMACHeadPtr = (DEV_DESC *)0;
    pdev_data->TxDescEMACTailPtr = (DEV_DESC *)0;
    pdev_data->TxDescFreeHeadPtr = (DEV_DESC *)0;
    pdev_data->TxDescFreeTailPtr = (DEV_DESC *)0;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    pdev_data->ErrRxDataAreaAllocCtr = 0u;
#endif
    pdev_data->ErrRxInvalidSizeCtr   = 0u;
    pdev_data->ErrTxCtr              = 0u;
    pdev_data->ErrRxCtr              = 0u;

    pdev_data->TxDescEOQPtr     = DEF_NULL;
    pdev_data->TxDescLastHDPPtr = DEF_NULL;

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
*                                                               - RETURNED BY NetIF_AddrHW_SetHandler() : -
*                               NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_IF_ERR_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               ---- RETURNED BY NetDev_RxDescInit() : ----
*                          !!!!
*
*                                                               ---- RETURNED BY NetDev_TxDescInit() : ----
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
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev_emac;
    NET_DEV_EMAC_CTRL  *pdev_emac_ctrl;
    NET_DEV_MDIO       *pdev_mdio;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_len;
    CPU_BOOLEAN         valid;
    CPU_INT32U          ch_ix;
    CPU_INT32U          reg_val;
    CPU_INT32U          clk_freq;
    CPU_INT32U          reg_to;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg       = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;         /* Obtain ptr to the dev cfg struct.                    */
    pdev_data      = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev_emac      = (NET_DEV_EMAC      *)pdev_data->EMAC_BaseAddr;
    pdev_emac_ctrl = (NET_DEV_EMAC_CTRL *)pdev_data->EMACCtrl_BaseAddr;
    pdev_mdio      = (NET_DEV_MDIO      *)pdev_data->MDIO_BaseAddr;
    pphy_cfg       = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;         /* Obtain ptr to the phy cfg struct.                    */


    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;
    clk_freq = pdev_bsp->ClkFreqGet(pif, perr);
    if (clk_freq < NET_DEV_TI_EMAC_VAL_PHY_FREQ_MIN) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                         */
                            pdev_cfg->TxDescNbr,
                            perr);

                                                                /* ---------- INITIALIZE HARDWARE REGISTERS ----------- */
                                                                /* If enable, clear the device interrupt enable bits.   */
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXTHRESHEN, DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0TXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));

    DEF_BIT_SET(pdev_emac_ctrl->SOFTRESET, TI_EMAC_BIT_SOFRESET_RST);   /* Perform a software reset.                    */
    DEF_BIT_CLR(pdev_emac_ctrl->SOFTRESET, TI_EMAC_BIT_SOFRESET_RST);

    DEF_BIT_SET(pdev_emac->MACSOFTRESET, TI_EMAC_BIT_SOFRESET_RST); /* Perform a software reset.                        */

    reg_to = NET_DEV_TI_EMAC_SOFT_RST_TO;

    do {
        reg_to--;
        reg_val = pdev_emac->MACSOFTRESET;
    } while (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_SOFRESET_RST));

    DEF_BIT_SET(pdev_emac->EMCONTROL, TI_EMAC_BIT_EMCONTROL_FREE);

                                                                /* Initialize all 16 header descriptors pointers to 0.  */
    for (ch_ix = 0u; ch_ix < NET_DEV_TI_EMAC_NBR_CH; ch_ix++) {
        pdev_emac->RXnHDP[ch_ix] = 0u;
        pdev_emac->TXnHDP[ch_ix] = 0u;
    }

    reg_to = NET_DEV_TI_EMAC_SOFT_RST_TO;
    while (reg_to != 0u) {                                      /* SAFETY WAIT for controller to settle after reset.    */
        reg_to--;
    }
                                                                /* --------------- CONFIGURE HW ADDRESS --------------- */
                                                                /* See Notes #4 & #5.                                   */
    NetASCII_Str_to_MAC( pdev_cfg->HW_AddrStr,
                        &hw_addr[0],
                         perr);

    NetIF_AddrHW_SetHandler((NET_IF_NBR  )pif->Nbr,             /* See Note #5a.                                        */
                            (CPU_INT08U *)hw_addr,
                            (CPU_INT08U  )sizeof(hw_addr),
                            (NET_ERR    *)perr);
    if (*perr == NET_IF_ERR_NONE) {
                                                                /* Set device MAC address registers.                    */
        pdev_emac->MACADDRHI    = ((CPU_INT32U)(hw_addr[0] <<  0u)
                                |  (CPU_INT32U)(hw_addr[1] <<  8u)
                                |  (CPU_INT32U)(hw_addr[2] << 16u)
                                |  (CPU_INT32U)(hw_addr[3] << 24u));
        pdev_emac->MACSRCADDRLO = ((CPU_INT32U)(hw_addr[5] <<  8u)
                                |  (CPU_INT32U)(hw_addr[4] <<  0u));
        pdev_emac->MACSRCADDRHI = ((CPU_INT32U)(hw_addr[0] <<  0u)
                                |  (CPU_INT32U)(hw_addr[1] <<  8u)
                                |  (CPU_INT32U)(hw_addr[2] << 16u)
                                |  (CPU_INT32U)(hw_addr[3] << 24u));

        for (ch_ix = 0u; ch_ix < NET_DEV_TI_EMAC_NBR_CH; ch_ix++) {
            pdev_emac->MACINDEX  = ch_ix;
            if (ch_ix == NET_DEV_TI_EMAC_CH_DFLT) {
                reg_val = ((CPU_INT32U)(TI_EMAC_BIT_MACADDRLO_VALID | TI_EMAC_BIT_MACADDRLO_MATCHFILT)
                        |  (CPU_INT32U)(hw_addr[5] << 8u)
                        |  (CPU_INT32U)(hw_addr[4] << 0u));
            } else {
                reg_val = ((CPU_INT32U)(hw_addr[5] << 8u)
                        |  (CPU_INT32U)(hw_addr[4] << 0u));
            }
            pdev_emac->MACADDRLO = reg_val;
        }
    } else {                                                    /* See Note #5b.                                        */
                                                                /* Obtain a copy of the current IF layer HW addr.       */
        hw_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_len, perr);
        if (*perr == NET_IF_ERR_NONE) {                         /* Check if the IF HW address has been user configured. */
            valid = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], perr);
        } else {
            valid = DEF_NO;
        }

        if (valid == DEF_YES) {
                                                                /* Set device MAC address registers.                    */
            pdev_emac->MACSRCADDRLO = ((CPU_INT32U)(hw_addr[5] <<  8u)
                                    |  (CPU_INT32U)(hw_addr[4] <<  0u));
            pdev_emac->MACSRCADDRHI = ((CPU_INT32U)(hw_addr[0] <<  0u)
                                    |  (CPU_INT32U)(hw_addr[1] <<  8u)
                                    |  (CPU_INT32U)(hw_addr[2] << 16u)
                                    |  (CPU_INT32U)(hw_addr[3] << 24u));
            pdev_emac->MACADDRHI    = ((CPU_INT32U)(hw_addr[0] <<  0u)
                                    |  (CPU_INT32U)(hw_addr[1] <<  8u)
                                    |  (CPU_INT32U)(hw_addr[2] << 16u)
                                    |  (CPU_INT32U)(hw_addr[3] << 24u));

            for (ch_ix = 0u; ch_ix < NET_DEV_TI_EMAC_NBR_CH; ch_ix++) {
                pdev_emac->MACINDEX  = ch_ix;
                if (ch_ix == NET_DEV_TI_EMAC_CH_DFLT) {
                    reg_val = (CPU_INT32U)(TI_EMAC_BIT_MACADDRLO_VALID | TI_EMAC_BIT_MACADDRLO_MATCHFILT)
                            | (CPU_INT32U)(hw_addr[5] << 8u)
                            | (CPU_INT32U)(hw_addr[4] << 0u);
                } else {
                    reg_val = (CPU_INT32U)(hw_addr[5] << 8u)
                            | (CPU_INT32U)(hw_addr[4] << 0u);
                }
                pdev_emac->MACADDRLO = reg_val;
            }
                                                                /* Set HW MAC address.                                  */
        } else {                                                /* See Note #5c.                                        */
            pdev_emac->MACINDEX     = NET_DEV_TI_EMAC_CH_DFLT;
            reg_val                 = pdev_emac->MACADDRLO;
            hw_addr[5]              = (reg_val >>  8u) & 0xFFu; /* Attempt to use automatically loaded HW address.      */
            hw_addr[4]              = (reg_val >>  0u) & 0xFFu;
            reg_val                 = pdev_emac->MACADDRHI;
            hw_addr[3]              = (reg_val >> 24u) & 0xFF;
            hw_addr[2]              = (reg_val >> 16u) & 0xFF;
            hw_addr[1]              = (reg_val >>  8u) & 0xFF;
            hw_addr[0]              = (reg_val >>  0u) & 0xFF;
            pdev_emac->MACSRCADDRLO = ((CPU_INT32U)(hw_addr[5] <<  8u)
                                    |  (CPU_INT32U)(hw_addr[4] <<  0u));
            pdev_emac->MACSRCADDRHI = ((CPU_INT32U)(hw_addr[3] <<  0u)
                                    |  (CPU_INT32U)(hw_addr[2] <<  8u)
                                    |  (CPU_INT32U)(hw_addr[1] << 16u)
                                    |  (CPU_INT32U)(hw_addr[0] << 24u));

                                                                /* Configure the IF to use the auto-loaded HW address.  */
            NetIF_AddrHW_SetHandler((NET_IF_NBR  )pif->Nbr,     /* See Note #5a.                                        */
                                    (CPU_INT08U *)hw_addr,
                                    (CPU_INT08U  )sizeof(hw_addr),
                                    (NET_ERR    *)perr);
            if (*perr != NET_IF_ERR_NONE) {                     /* No valid HW address specified, return with error     */
                return;
            }
        }
    }
                                                                /* Clear all unicast channels.                          */
    pdev_emac->RXUNICASTCLEAR = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);
    pdev_emac->RXUNICASTSET   = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
#ifdef NET_MCAST_MODULE_EN
    pdev_emac->RXMBPENABLE    = TI_EMAC_BIT_RXMBPENABLE_RXBROADEN
                              | DEF_BIT_MASK(NET_DEV_TI_EMAC_CH_DFLT, 8u)
                              | TI_EMAC_BIT_RXMBPENABLE_RXMULTEN
                              | DEF_BIT_MASK(NET_DEV_TI_EMAC_CH_DFLT, 0u);
#else
    pdev_emac->RXMBPENABLE    = TI_EMAC_BIT_RXMBPENABLE_RXBROADEN
                              | DEF_BIT_MASK(NET_DEV_TI_EMAC_CH_DFLT, 8u);
#endif

    reg_val = TI_EMAC_BIT_MACCONTROL_TXFLOWEN
            | TI_EMAC_BIT_MACCONTROL_RXBUFFERFLOWEN;

                                                                /* ------ OPTIONALLY OBTAIN REFERENCE TO PHY CFG ------ */
    if (pphy_cfg != (void *)0) {                                /* Cfg MAC w/ initial Phy settings.                     */
        if ( (pphy_cfg->BusMode == NET_PHY_BUS_MODE_RMII) &&
            ((pphy_cfg->Spd     == NET_PHY_SPD_100      ) ||
             (pphy_cfg->Spd     == NET_PHY_SPD_AUTO     ))) {
            DEF_BIT_SET(reg_val, TI_EMAC_BIT_MACCONTROL_RMIISPEED);
        }

        if ((pphy_cfg->Duplex == NET_PHY_DUPLEX_FULL) ||
            (pphy_cfg->Duplex == NET_PHY_DUPLEX_AUTO)) {
            DEF_BIT_SET(reg_val, TI_EMAC_BIT_MACCONTROL_FULLDUPLEX);
        }
    }

    pdev_emac->MACCONTROL      = reg_val;                       /* Set configuration bits in the MAC control register.  */
    pdev_emac->RXINTMASKCLEAR  = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);
    pdev_emac->TXINTMASKCLEAR  = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);
    pdev_emac->MACINTMASKCLEAR = TI_EMAC_BIT_MAC_INT_FLAG_HOSTPEND
                               | TI_EMAC_BIT_MAC_INT_FLAG_STATPEND;
    pdev_emac->RXINTMASKSET    = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac->TXINTMASKSET    = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac->MACINTMASKSET   = TI_EMAC_BIT_MAC_INT_FLAG_HOSTPEND
                               | TI_EMAC_BIT_MAC_INT_FLAG_STATPEND;

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
                                                                /* Enable receive and transmit DMA controllers.         */
    pdev_emac->RXCONTROL = TI_EMAC_BIT_RXCONTROL_RXEN;
    pdev_emac->TXCONTROL = TI_EMAC_BIT_TXCONTROL_TXEN;

    reg_val = ((((2u * clk_freq) / NET_DEV_TI_EMAC_VAL_PHY_FREQ_MIN) - 1u) / 2u);
    reg_val = DEF_MIN(reg_val, TI_EMAC_MSK_CONTROL_CLKDIV);

    DEF_BIT_CLR(pdev_mdio->CONTROL, TI_EMAC_MSK_CONTROL_CLKDIV);
    DEF_BIT_SET(pdev_mdio->CONTROL, reg_val);

    DEF_BIT_SET(pdev_mdio->CONTROL, TI_EMAC_BIT_CONTROL_EN);
    DEF_BIT_SET(pdev_emac->MACCONTROL, TI_EMAC_BIT_MACCONTROL_GMIIEN);

                                                                /* Enable the device interrupt in EMAC control ...      */
                                                                /* ... registers.                                       */
    pdev_emac_ctrl->C0RXTHRESHEN = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac_ctrl->C0RXEN       = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac_ctrl->C0TXEN       = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac_ctrl->C0MISCEN     = DEF_BIT_NONE;

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
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev_emac;
    NET_DEV_EMAC_CTRL  *pdev_emac_ctrl;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data      = (NET_DEV_DATA      *)pif->Dev_Data;
    pdev_emac      = (NET_DEV_EMAC      *)pdev_data->EMAC_BaseAddr;
    pdev_emac_ctrl = (NET_DEV_EMAC_CTRL *)pdev_data->EMACCtrl_BaseAddr;

                                                                /* ----------------- DISABLE RX & TX ------------------ */
    pdev_emac->RXCONTROL = DEF_BIT_NONE;
    pdev_emac->TXCONTROL = DEF_BIT_NONE;

                                                                /* -------------- DISABLE & CLEAR INT'S --------------- */
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXTHRESHEN, DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0TXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    pdev_emac->RXINTMASKCLEAR  = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);
    pdev_emac->TXINTMASKCLEAR  = DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u);
    pdev_emac->MACINTMASKCLEAR = TI_EMAC_BIT_MAC_INT_FLAG_HOSTPEND
                               | TI_EMAC_BIT_MAC_INT_FLAG_STATPEND;

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }
                                                                /* ------------- FREE USED TX DESCRIPTORS ------------- */
    NetDev_TxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }
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
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    CPU_INT08U    *pbuf_new;
    CPU_INT16U     len;
    CPU_INT32U     reg_val;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdesc     = (DEV_DESC     *)pdev_data->RxDescCurPtr;        /* Obtain ptr to next ready descriptor.                 */
                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->StatRxCtrRx++;
#endif

    reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                          pdev_data->ByteSwap);

    if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_OWNER)) {
       *size   = (CPU_INT16U  )0u;
       *p_data = (CPU_INT08U *)0u;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

    if (DEF_BIT_IS_SET_ANY(reg_val, TI_EMAC_BIT_DESC_FLAG_RX_ERR)) {
        NetDev_RxDescPtrCurInc(pif);
        pdev_data->ErrRxInvalidSizeCtr++;
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   =  NET_ERR_RX;
        return;
    }

    len = reg_val & TI_EMAC_MSK_DESC_PKT_LEN;

    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
        NetDev_RxDescPtrCurInc(pif);
       *perr = NET_DEV_ERR_INVALID_SIZE;
        return;
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
        NET_CTR_ERR_INC(pdev_data->ErrRxDataAreaAllocCtr);
        NetDev_RxDescPtrCurInc(pif);                           /* Free the current descriptor.                         */
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
        return;
    }

   *size          = len;                                       /* Return the size of the received frame.               */
                                                                /* Return a pointer to the newly received data area.    */
   *p_data        = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)pdesc->BufPtr,
                                                              pdev_data->ByteSwap);
                                                                /* Update the descriptor to point to a new data area    */
    pdesc->BufPtr = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)pbuf_new,
                                                              pdev_data->ByteSwap);
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

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev_emac;
    DEV_DESC           *pdesc;
    DEV_DESC           *pdesc_last;
    CPU_INT32U          reg_val;
    CPU_INT08U         *p_data_free;
    DEV_DESC           *pdesc_prev;
    NET_ERR             err;
    DEV_DESC           *pdesc_tmp    = DEF_NULL;
    CPU_INT32U          reg_val_tmp  = 0;
    CPU_INT32U          reg_val_next = 0;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC      *)pdev_data->EMAC_BaseAddr;  /* Overlay dev reg struct on top of dev base addr.      */


    pdev_emac->TXINTMASKCLEAR = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);   /* Disable Tx interrupts.                           */
                                                                /* Obtain ptr to next available Tx descriptor.          */

                                                                /* Free buffers and descriptors                         */
    pdesc = pdev_data->TxDescEMACHeadPtr;
    if (pdesc != DEF_NULL) {
        reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                              pdev_data->ByteSwap);

                                                                    /* Search for all the descriptors processed by the EMAC */
        while (DEF_BIT_IS_CLR(reg_val, TI_EMAC_BIT_DESC_FLAG_OWNER)) {
            p_data_free = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(pdesc->BufPtr),
                                                                     pdev_data->ByteSwap);

            NetIF_TxDeallocTaskPost(p_data_free, &err);         /* De-allocate Tx buffer.                               */
            NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available.       */

                                                                /* Write to the complete Tx descriptor register.        */
            pdev_emac->TXnCP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)pdesc;

                                                                /* Add the descriptor to the free list.                 */
            if (pdev_data->TxDescFreeHeadPtr == (DEV_DESC *)0) {
                pdev_data->TxDescFreeHeadPtr = pdesc;
            } else {
                pdesc_prev          = pdev_data->TxDescFreeTailPtr;
                pdesc_prev->NextPtr = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc),
                                                                               pdev_data->ByteSwap);
            }
            pdev_data->TxDescFreeTailPtr = pdesc;
            pdev_data->TxDescEMACHeadPtr = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc->NextPtr),
                                                                                    pdev_data->ByteSwap);
            pdesc->NextPtr               = (DEV_DESC *)0;

            if (pdev_data->TxDescEMACHeadPtr == (DEV_DESC *)0) {
                pdev_data->TxDescEMACTailPtr = (DEV_DESC *)0;
                break;
            }

            if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_EOQ)) {
                pdev_data->TxDescEOQPtr = pdev_data->TxDescEMACHeadPtr;
            }

            pdesc   = pdev_data->TxDescEMACHeadPtr;
            reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                                      pdev_data->ByteSwap);
        }
    }

    if (pdev_data->TxDescFreeHeadPtr == (DEV_DESC *) 0) {       /* If no descriptors are available.                     */
        pdev_emac->TXINTMASKSET = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT); /* ... Re-enable interrupts.                        */
       *perr = NET_DEV_ERR_TX_BUSY;                             /* ... Return error.                                    */
        return;
    }

                                                                /* Take first descriptor from the list.                 */
    pdesc                        = (DEV_DESC *)pdev_data->TxDescFreeHeadPtr;
                                                                /* Move the head pointer to the next descriptor.        */
    pdev_data->TxDescFreeHeadPtr = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc->NextPtr),
                                                                            pdev_data->ByteSwap);

                                                                /* Fill the descriptor.                                 */
    reg_val                      = ((size                    & DEF_BIT_FIELD(16u, 0u)) <<  0u)
                                 | ((pdev_cfg->TxBufIxOffset & DEF_BIT_FIELD(16u, 0u)) << 16u);
    pdesc->BufLen                = (CPU_REG32)NetDev_SwapData((CPU_INT32U)reg_val,
                                                                          pdev_data->ByteSwap);
                                                                /* Configure descriptor with Tx data area address.      */
    pdesc->BufPtr                = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(p_data),
                                                                              pdev_data->ByteSwap);
    reg_val                      = size                         /* Configure descriptor with Tx data size.              */
                                 | TI_EMAC_BIT_DESC_FLAG_OWNER
                                 | TI_EMAC_BIT_DESC_FLAG_SOP
                                 | TI_EMAC_BIT_DESC_FLAG_EOP;
    pdesc->PktFlagsLen           = (CPU_REG32)NetDev_SwapData((CPU_INT32U)reg_val,
                                                                          pdev_data->ByteSwap);
    pdesc->NextPtr               = (DEV_DESC *)0;
                                                                /* Insert descriptor in the EMAC descriptor list.       */
    if (pdev_data->TxDescEMACHeadPtr == (DEV_DESC *) 0) {       /* Is the EMAC descriptor list Empty?                   */
        pdev_data->TxDescEMACHeadPtr = pdesc;                   /* ... Add descriptor in the head of the list.          */
        pdev_data->TxDescEMACTailPtr = pdesc;
                                                                /* ... Write to the Tx head DMA channel.                */
        pdev_emac->TXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)pdesc;
        pdev_data->TxDescLastHDPPtr = pdesc;

    } else {
                                                                /* Insert the descriptor at the end of the list.        */
        pdesc_last                   = pdev_data->TxDescEMACTailPtr;
        pdesc_last->NextPtr          = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc),
                                                                                pdev_data->ByteSwap);
        pdev_data->TxDescEMACTailPtr = pdesc;

        if (pdev_data->TxDescEOQPtr != DEF_NULL) {
            reg_val_tmp = NetDev_SwapData((CPU_INT32U)pdev_data->TxDescEOQPtr->PktFlagsLen,
                                                      pdev_data->ByteSwap);
            if ((DEF_BIT_IS_SET(reg_val_tmp, TI_EMAC_BIT_DESC_FLAG_OWNER)) &&
                (pdev_data->TxDescEOQPtr != pdev_data->TxDescLastHDPPtr)) {
                pdev_emac->TXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)pdev_data->TxDescEOQPtr;
                pdev_data->TxDescLastHDPPtr = pdev_data->TxDescEOQPtr;
            }

        } else {
            pdesc_tmp = pdev_data->TxDescEMACHeadPtr;
            while (pdesc_tmp != DEF_NULL) {
                reg_val_tmp = NetDev_SwapData((CPU_INT32U)pdesc_tmp->PktFlagsLen,
                                                          pdev_data->ByteSwap);

                if (DEF_BIT_IS_SET(reg_val_tmp, TI_EMAC_BIT_DESC_FLAG_EOQ)) {
                    if (pdesc_tmp->NextPtr != DEF_NULL) {
                        reg_val_next = NetDev_SwapData((CPU_INT32U)pdesc_tmp->NextPtr->PktFlagsLen,
                                                                   pdev_data->ByteSwap);
                        if ((DEF_BIT_IS_SET(reg_val_next, TI_EMAC_BIT_DESC_FLAG_OWNER)) &&
                            (pdesc_tmp->NextPtr != pdev_data->TxDescLastHDPPtr)) {
                            pdev_emac->TXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)pdesc_tmp->NextPtr;
                            pdev_data->TxDescLastHDPPtr = pdesc_tmp->NextPtr;
                            break;
                        }
                    }
                }

                pdesc_tmp = pdesc_tmp->NextPtr;
            }
        }
    }

    pdev_data->TxDescEOQPtr = DEF_NULL;

    pdev_emac->TXINTMASKSET = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT); /* ... Re-enable Tx interrupts.                         */

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
* Note(s)     : (1) The hash funtion in the TI_EMAC MAC controllers creates a 6-bit data value from
*                   the 48-bit destination address (DA) as follows:
*
*                   HASH[0] = DA[0] XOR DA[6]  XOR DA[12] XOR DA[18] XOR DA[24] XOR DA[30] XOR DA[36] XOR DA[42]
*                   HASH[1] = DA[1] XOR DA[7]  XOR DA[13] XOR DA[19] XOR DA[25] XOR DA[31] XOR DA[37] XOR DA[43]
*                   HASH[2] = DA[2] XOR DA[8]  XOR DA[14] XOR DA[20] XOR DA[26] XOR DA[32] XOR DA[38] XOR DA[44]
*                   HASH[3] = DA[3] XOR DA[9]  XOR DA[15] XOR DA[21] XOR DA[27] XOR DA[33] XOR DA[39] XOR DA[45]
*                   HASH[4] = DA[4] XOR DA[10] XOR DA[16] XOR DA[22] XOR DA[28] XOR DA[34] XOR DA[40] XOR DA[46]
*                   HASH[5] = DA[5] XOR DA[11] XOR DA[17] XOR DA[23] XOR DA[29] XOR DA[35] XOR DA[41] XOR DA[48]
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_EMAC  *pdev_emac;
    NET_DEV_DATA  *pdev_data;
    CPU_INT08U     bit_nbr;
    CPU_INT08U    *paddr_hash_ctrs;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC *)pdev_data->EMAC_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* Calculate the 6-bit has value. (see note #1).        */
    bit_nbr   =  (paddr_hw[5]                             ) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[5] >> 6u) | (paddr_hw[4] << 2u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[4] >> 4u) | (paddr_hw[3] << 4u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[3] >> 2u)                      ) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[2])                            ) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[2] >> 6u) | (paddr_hw[1] << 2u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[1] >> 4u) | (paddr_hw[0] << 4u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[0] >> 2u)                      ) & DEF_BIT_FIELD(6, 0u);

    CPU_CRITICAL_ENTER();
    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[bit_nbr];
    (*paddr_hash_ctrs)++;                                       /* Increment hash bit reference ctr.                    */

    if (*paddr_hash_ctrs == 1u) {
        if (bit_nbr > 31u) {
            DEF_BIT_SET(pdev_emac->MACHASH2, DEF_BIT(bit_nbr - 32u));
        } else {
            DEF_BIT_SET(pdev_emac->MACHASH1, DEF_BIT(bit_nbr));
        }
    }
    CPU_CRITICAL_EXIT();

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
* Note(s)     : (1) See 'NetIF_Ether_AddrMulticastAdd()' note #1.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_EMAC  *pdev_emac;
    NET_DEV_DATA  *pdev_data;
    CPU_INT08U     bit_nbr;
    CPU_INT08U    *paddr_hash_ctrs;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC *)pdev_data->EMAC_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

    bit_nbr   =  (paddr_hw[5]                             ) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[5] >> 6u) | (paddr_hw[4] << 2u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[4] >> 4u) | (paddr_hw[3] << 4u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[3] >> 2u)                      ) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[2])                            ) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[2] >> 6u) | (paddr_hw[1] << 2u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[1] >> 4u) | (paddr_hw[0] << 4u)) & DEF_BIT_FIELD(6, 0u)
              ^ ((paddr_hw[0] >> 2u)                      ) & DEF_BIT_FIELD(6, 0u);

    CPU_CRITICAL_ENTER();
    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[bit_nbr];

    if (*paddr_hash_ctrs > 1u) {
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        CPU_CRITICAL_EXIT();
       *perr = NET_DEV_ERR_NONE;
    }

    *paddr_hash_ctrs = 0u;

    if (bit_nbr > 31u) {
        DEF_BIT_CLR(pdev_emac->MACHASH2, DEF_BIT(bit_nbr - 32u));
    } else {
        DEF_BIT_CLR(pdev_emac->MACHASH1, DEF_BIT(bit_nbr));
    }

    CPU_CRITICAL_EXIT();

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
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
    NET_DEV_EMAC       *pdev_emac;
    NET_DEV_EMAC_CTRL  *pdev_emac_ctrl;
    DEV_DESC           *pdesc;
    DEV_DESC           *pdesc_prev;
    CPU_INT08U         *p_data;
    CPU_DATA            reg_val;
    CPU_INT32U          int_stat;
    NET_ERR             err;
    CPU_INT32U          reg_macstatus;


   (void)&type;                                                 /* Prevent 'variable unused' compiler warning.          */

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA  *)pif->Dev_Data;                 /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC  *)pdev_data->EMAC_BaseAddr;      /* Overlay dev reg struct on top of dev base addr.      */
    pdev_emac_ctrl = (NET_DEV_EMAC_CTRL *)pdev_data->EMACCtrl_BaseAddr;

                                                                /* Disable Interrupts                                   */
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXTHRESHEN, DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0RXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));
    DEF_BIT_CLR(pdev_emac_ctrl->C0TXEN,       DEF_BIT_FIELD(NET_DEV_TI_EMAC_NBR_CH, 0u));

    reg_macstatus = 0u;                                         /* Initialize before checking for TX errors             */

                                                                /* --------------- DETERMINE ISR TYPE ----------------- */
    int_stat = pdev_emac->MACINVECTOR;                          /* Get the interrupt status register value.             */

    if (DEF_BIT_IS_SET(int_stat, DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT))) {
        pdesc   = pdev_data->RxDescCurPtr;
        reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                              pdev_data->ByteSwap);

        reg_macstatus = (pdev_emac->MACSTATUS & TI_EMAC_MSK_MACSTATUS_RX_ERRCODE);

        if (reg_macstatus > 0u) {
            pdev_data->ErrRxCtr++;                              /* Indicate that Rx error has occurred                  */
        }

        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task                               */

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
        pdev_data->StatRxCtrRxISR++;
#endif
                                                                /* Disable Rx interrupt.                                */
        pdev_emac->RXINTMASKCLEAR = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
                                                                /* Acknowledge Core 0 Rx interrupt.                     */
        pdev_emac->MACEOIVECTOR   = TI_EMAC_BIT_MACEOIVECTOR_C0RX;
    }

    if (DEF_BIT_IS_SET(int_stat, DEF_BIT_MASK(DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT), 16u))) {
        pdesc   = pdev_data->TxDescEMACHeadPtr;

        if (pdesc != DEF_NULL) {
            reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                                  pdev_data->ByteSwap);

            reg_macstatus = (pdev_emac->MACSTATUS & TI_EMAC_MSK_MACSTATUS_TX_ERRCODE);

                                                                    /* Search for all the descriptors processed by the EMAC */
            while (DEF_BIT_IS_CLR(reg_val, TI_EMAC_BIT_DESC_FLAG_OWNER)) {
                if (reg_macstatus > 0u) {
                    pdev_data->ErrTxCtr++;                          /* Indicate that Tx error has occurred                  */
                    break;
                }

                p_data = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(pdesc->BufPtr),
                                                                    pdev_data->ByteSwap);

                NetIF_TxDeallocTaskPost(p_data, &err);              /* De-allocate Tx buffer.                               */
                NetIF_DevTxRdySignal(pif);                          /* Signal Net IF that Tx resources are available.       */

                                                                    /* Write to the complete Tx descriptor register.        */
                pdev_emac->TXnCP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)pdesc;

                                                                    /* Add the descriptor to the free list.                 */
                if (pdev_data->TxDescFreeHeadPtr == (DEV_DESC *)0) {
                    pdev_data->TxDescFreeHeadPtr = pdesc;
                } else {
                    pdesc_prev          = pdev_data->TxDescFreeTailPtr;
                    pdesc_prev->NextPtr = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc),
                                                                                   pdev_data->ByteSwap);
                }
                pdev_data->TxDescFreeTailPtr = pdesc;
                pdev_data->TxDescEMACHeadPtr = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc->NextPtr),
                                                                                        pdev_data->ByteSwap);
                pdesc->NextPtr               = (DEV_DESC *)0;

                if (pdev_data->TxDescEMACHeadPtr == (DEV_DESC *)0) {
                    pdev_data->TxDescEMACTailPtr = (DEV_DESC *)0;
                    break;
                }

                if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_EOQ)) {
                    pdev_data->TxDescEOQPtr = pdev_data->TxDescEMACHeadPtr;
                }

                pdesc   = pdev_data->TxDescEMACHeadPtr;
                reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                                      pdev_data->ByteSwap);
            }
        }

        pdev_emac->MACEOIVECTOR = TI_EMAC_BIT_MACEOIVECTOR_C0TX;    /* Acknowledge Core 0 Tx interrupt.                     */
    }

    pdev_emac_ctrl->C0RXTHRESHEN = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac_ctrl->C0RXEN       = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
    pdev_emac_ctrl->C0TXEN       = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT);
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
*                           NET_IF_ERR_NULL_FNCT                Null interface function pointer encountered.
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
    NET_DEV_DATA        *pdev_data;
    NET_DEV_EMAC        *pdev_emac;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC *)pdev_data->EMAC_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

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
                         DEF_BIT_SET(pdev_emac->MACCONTROL, TI_EMAC_BIT_MACCONTROL_FULLDUPLEX);
                         break;


                    case NET_PHY_DUPLEX_HALF:
                         DEF_BIT_CLR(pdev_emac->MACCONTROL, TI_EMAC_BIT_MACCONTROL_FULLDUPLEX);
                         break;


                    default:
                         break;
                 }
             }

             spd = NET_PHY_SPD_0;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {
                    case NET_PHY_SPD_10:
                         DEF_BIT_CLR(pdev_emac->MACCONTROL, TI_EMAC_BIT_MACCONTROL_RMIISPEED);
                         break;


                    case NET_PHY_SPD_100:
                         DEF_BIT_SET(pdev_emac->MACCONTROL, TI_EMAC_BIT_MACCONTROL_RMIISPEED);
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
    NET_DEV_DATA  *pdev_data;
    NET_DEV_MDIO  *pdev_mdio;
    CPU_INT32U     reg_to;
    CPU_INT32U     reg_val;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr = NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_mdio = (NET_DEV_MDIO *)pdev_data->MDIO_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

    reg_to    = NET_DEV_TI_EMAC_MII_REG_RD_WR_TO;

    do {
        reg_val = pdev_mdio->USERACCESS0;
        reg_to--;
    } while ((reg_to > 0u) && (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_USERACCESS_GO)));

    if (reg_to == 0u) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    reg_val                = TI_EMAC_BIT_USERACCESS_GO
                           | (reg_addr << 21u)
                           | (phy_addr << 16u);

    pdev_mdio->USERACCESS0 = reg_val;
    reg_to                 = NET_DEV_TI_EMAC_MII_REG_RD_WR_TO;

    do {
        reg_val = pdev_mdio->USERACCESS0;
        reg_to--;
    } while ((reg_to > 0u) &&
             (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_USERACCESS_GO)));

    if (reg_to == 0u) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    reg_to = NET_DEV_TI_EMAC_MII_REG_RD_WR_TO;
    do {
        reg_val = pdev_mdio->USERACCESS0;
        reg_to--;
    } while ((reg_to > 0u) &&
             (DEF_BIT_IS_CLR(reg_val, TI_EMAC_BIT_USERACCESS_ACK)));
    if (reg_to == 0u) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    *p_data = reg_val & DEF_BIT_FIELD(16u, 0u);
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
    NET_DEV_DATA  *pdev_data;
    NET_DEV_MDIO  *pdev_mdio;
    CPU_INT32U     reg_to;
    CPU_INT32U     reg_val;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_mdio = (NET_DEV_MDIO *)pdev_data->MDIO_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

    reg_to    = NET_DEV_TI_EMAC_MII_REG_RD_WR_TO;

    do {
        reg_val = pdev_mdio->USERACCESS0;
        reg_to--;
    } while ((reg_to > 0u) && (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_USERACCESS_GO)));

    if (reg_to == 0u) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

    reg_val                = TI_EMAC_BIT_USERACCESS_GO
                           | TI_EMAC_BIT_USERACCESS_WRITE
                           | (reg_addr << 21u)
                           | (phy_addr << 16u)
                           | data;
    pdev_mdio->USERACCESS0 = reg_val;
    reg_to                 = NET_DEV_TI_EMAC_MII_REG_RD_WR_TO;
    do {
        reg_val = pdev_mdio->USERACCESS0;
        reg_to--;
    } while ((reg_to > 0u) && (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_USERACCESS_GO)));

    if (reg_to == 0u) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

    reg_to = NET_DEV_TI_EMAC_MII_REG_RD_WR_TO;
    do {
        reg_val = pdev_mdio->USERINTRAW;
        reg_to--;
    } while ((reg_to > 0u) && (DEF_BIT_IS_CLR(reg_val, TI_EMAC_MSK_LINK_FLAG_USERPHY0)));


    if (reg_to == 0u) {
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
    NET_DEV_EMAC       *pdev_emac;
    MEM_POOL           *pmem_pool;
    DEV_DESC           *pdesc;
    LIB_ERR             lib_err;
    CPU_SIZE_T          nbr_octets;
    CPU_INT16U          desc_ix;
    CPU_INT32U          reg_val;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC      *)pdev_data->EMAC_BaseAddr;  /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool  = &pdev_data->RxDescPool;
    nbr_octets =  pdev_cfg->RxDescNbr * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                            (CPU_SIZE_T) nbr_octets,
                                            (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }
                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdev_data->RxDescStartPtr = pdesc;
    pdev_data->RxDescCurPtr   = pdesc;
                                                                /* --------------- INIT RX DESCRIPTORS ---------------- */
    for (desc_ix = 0; desc_ix < pdev_cfg->RxDescNbr; desc_ix++) {
        pdesc->NextPtr = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc + 1u),
                                                                  pdev_data->ByteSwap);
        pdesc->BufPtr  = (CPU_INT08U *)NetBuf_GetDataPtr(pif,
                                                         NET_TRANSACTION_RX,
                                                         NET_IF_ETHER_FRAME_MAX_SIZE,
                                                         NET_IF_IX_RX,
                                                         0,
                                                         0,
                                                         DEF_NULL,
                                                         perr);
        pdesc->BufPtr  = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(pdesc->BufPtr),
                                                                    pdev_data->ByteSwap);
        if (*perr != NET_BUF_ERR_NONE) {
            return;
        }
        reg_val            = ((pdev_cfg->RxBufLargeSize & DEF_BIT_FIELD(16u, 0u)) <<  0u)
                           | ((pdev_cfg->RxBufIxOffset  & DEF_BIT_FIELD(16u, 0u)) << 16u);
        pdesc->BufLen      = (CPU_REG32)NetDev_SwapData((CPU_INT32U)reg_val,
                                                                   pdev_data->ByteSwap);
        pdesc->PktFlagsLen = (CPU_REG32)NetDev_SwapData((CPU_INT32U)TI_EMAC_BIT_DESC_FLAG_OWNER,
                                                                    pdev_data->ByteSwap);
        pdesc++;                                                /* Point to next descriptor in list.                    */
    }

    desc_ix                  = pdev_cfg->RxDescNbr - 1u;
    pdesc                    = &pdev_data->RxDescStartPtr[desc_ix];
    pdev_data->RxDescLastPtr = pdesc;
    pdesc->NextPtr           = (DEV_DESC *)0;
                                                                /* ------------- INIT HARDWARE REGISTERS -------------- */
                                                                /* Configure the DMA with the Rx desc start address.    */
    pdev_emac->RXnHDP[NET_DEV_TI_EMAC_CH_DFLT]        = (CPU_INT32U)pdev_data->RxDescCurPtr;
    pdev_emac->RXnFLOWTHRESH[NET_DEV_TI_EMAC_CH_DFLT] = NET_DEV_TI_EMAC_CH_THRESHOLD_VAL;
    pdev_emac->RXBUFFEROFFSET                         = pdev_cfg->RxBufIxOffset;
    pdev_emac->RXMAXLEN                               = NET_IF_ETHER_FRAME_MAX_CRC_SIZE;
    if (pdev_emac->RXnFREEBUFFER[NET_DEV_TI_EMAC_CH_DFLT] == 0u) {
        pdev_emac->RXnFREEBUFFER[NET_DEV_TI_EMAC_CH_DFLT] = pdev_cfg->RxDescNbr;
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
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL receive buffers
*                   and the Rx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_DATA  *pdev_data;
    NET_DEV_EMAC  *pdev_emac;
    MEM_POOL      *pmem_pool;
    DEV_DESC      *pdesc;
    LIB_ERR        lib_err;
    CPU_INT08U    *pdesc_data;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC *)pdev_data->EMAC_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

    pmem_pool = &pdev_data->RxDescPool;
                                                                /* ---------- FREE RX DESC DATA AREAS (FREE) ---------- */
    pdesc     = pdev_data->RxDescCurPtr;

    while (pdesc != (DEV_DESC *)0) {
        pdesc_data = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(pdesc->BufPtr),
                                                                pdev_data->ByteSwap);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdev_emac->RXnFREEBUFFER[NET_DEV_TI_EMAC_CH_DFLT] = 1u; /* Increment free buffer.                               */
        pdesc = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc->NextPtr),
                                                         pdev_data->ByteSwap);
    }

    pdev_emac->RXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = DEF_BIT_NONE;
                                                                /* ---------------- FREE RX DESC BLOCK ---------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.    */
                     pdev_data->RxDescStartPtr,
                    &lib_err);

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    pdev_data->RxDescStartPtr = (DEV_DESC *)0;
    pdev_data->RxDescCurPtr   = (DEV_DESC *)0;
    pdev_data->RxDescLastPtr  = (DEV_DESC *)0;
#endif

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
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
    NET_DEV_DATA      *pdev_data;
    DEV_DESC          *pdesc;
    DEV_DESC          *pdesc_last;
    NET_DEV_EMAC      *pdev_emac;
    NET_DEV_CFG_ETHER *pdev_cfg;
    CPU_INT16U         free_buf;
    CPU_INT32U         reg_val;
    NET_ERR            err;


    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC      *)pdev_data->EMAC_BaseAddr;  /* Overlay dev reg struct on top of dev base addr.      */

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
                                                                /* Get the current descriptor.                          */
    pdesc                                         = pdev_data->RxDescCurPtr;
    pdev_data->RxDescCurPtr                       = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)pdesc->NextPtr,
                                                                                            pdev_data->ByteSwap);
    pdev_emac->RXnFREEBUFFER[NET_DEV_TI_EMAC_CH_DFLT] = 1u;     /* Increment #freebuffers by 1.                         */
                                                                /* Write in the Rx complete descriptor register.        */
    pdesc->NextPtr                                = (DEV_DESC *)0;
    pdesc->PktFlagsLen                            = (CPU_REG32)NetDev_SwapData((CPU_INT32U)TI_EMAC_BIT_DESC_FLAG_OWNER,
                                                                                           pdev_data->ByteSwap);
    reg_val                                       = ((pdev_cfg->RxBufLargeSize & DEF_BIT_FIELD(16u, 0u)) <<  0u)
                                                  | ((pdev_cfg->RxBufIxOffset  & DEF_BIT_FIELD(16u, 0u)) << 16u);
    pdesc->BufLen                                 = (CPU_REG32)NetDev_SwapData((CPU_INT32U)reg_val,
                                                                                           pdev_data->ByteSwap);
    pdev_emac->RXnCP[NET_DEV_TI_EMAC_CH_DFLT]     = (CPU_INT32U)pdesc;
    pdesc_last                                    = pdev_data->RxDescLastPtr;
    reg_val                                       = (CPU_REG32)NetDev_SwapData((CPU_INT32U)pdesc_last->PktFlagsLen,
                                                                                           pdev_data->ByteSwap);
    pdesc_last->NextPtr                           = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)pdesc,
                                                                                            pdev_data->ByteSwap);
    pdev_data->RxDescLastPtr                      = pdesc;
    if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_EOQ)) {
                                                                /* Write the new list in the Rx descriptor list head.   */
        pdev_emac->RXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)pdesc;
    } else if (pdev_emac->RXnHDP[NET_DEV_TI_EMAC_CH_DFLT] == 0u) {
        pdesc   = (DEV_DESC *)pdev_emac->RXnCP[NET_DEV_TI_EMAC_CH_DFLT] ;
        reg_val = (CPU_REG32)NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                                        pdev_data->ByteSwap);
        if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_EOQ)) {
            pdev_emac->RXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = (CPU_INT32U)NetDev_SwapData((CPU_INT32U)pdesc->NextPtr,
                                                                                                 pdev_data->ByteSwap);
        }
    } else {
        ;
    }

    free_buf = pdev_emac->RXnFREEBUFFER[NET_DEV_TI_EMAC_CH_DFLT];   /* Get the number of free buffers.                      */

    if (pdev_data->FreeBuf > free_buf){
        pdev_data->FreeBuf = free_buf;
    }

    if (free_buf == 0u) {
        free_buf = 0;
    }

    if (free_buf < pdev_cfg->RxDescNbr) {                       /* If #available buffers < #Rx Descriptors              */
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task                               */

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
        pdev_data->StatRxCtrTaskSignalSelf++;
#endif
        (void)&err;
    } else if (free_buf > pdev_cfg->RxDescNbr) {
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task                               */

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
        pdev_data->StatRxCtrTaskSignalSelf++;
#endif
        (void)&err;
    } else {
        pdev_emac->RXINTMASKSET = DEF_BIT(NET_DEV_TI_EMAC_CH_DFLT); /* Enable Rx interrupt.                             */
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
    DEV_DESC           *pdesc;
    MEM_POOL           *pmem_pool;
    CPU_SIZE_T          nbr_octets;
    CPU_INT16U          desc_ix;
    CPU_INT32U          reg_val;
    LIB_ERR             lib_err;

                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */

                                                                /* -------------- ALLOCATE DESCRIPTORS  --------------- */
                                                                /* See Note #3.                                         */
    pmem_pool  = &pdev_data->TxDescPool;
    nbr_octets =  pdev_cfg->TxDescNbr * sizeof(DEV_DESC);
    pdesc      = (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                            (CPU_SIZE_T) nbr_octets,
                                            (LIB_ERR  *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data->TxDescStartPtr    = pdesc;
    pdev_data->TxDescEMACHeadPtr = (DEV_DESC *)0;
    pdev_data->TxDescEMACTailPtr = (DEV_DESC *)0;
    pdev_data->TxDescFreeHeadPtr = pdesc;

    for (desc_ix = 0; desc_ix < pdev_cfg->TxDescNbr; desc_ix++) {
        pdesc->NextPtr     = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc + 1u),
                                                                      pdev_data->ByteSwap);
        pdesc->BufPtr      = (CPU_INT08U *)0;
        pdesc->BufLen      = 0u;
        reg_val            = TI_EMAC_BIT_DESC_FLAG_SOP | TI_EMAC_BIT_DESC_FLAG_EOP;
        pdesc->PktFlagsLen = (CPU_REG32)NetDev_SwapData((CPU_INT32U)reg_val,
                                                                    pdev_data->ByteSwap);
        pdesc++;
    }
    desc_ix                      =  pdev_cfg->TxDescNbr - 1u;
    pdesc                        = &pdev_data->TxDescStartPtr [desc_ix];
    pdev_data->TxDescFreeTailPtr = pdesc;
    pdesc->NextPtr               = (DEV_DESC *)0;
    *perr                        = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_TxDescFreeAll()
*
* Description : (1) This function returns the descriptor memory block and descriptor data area
*                   memory blocks back to their respective memory pools :
*
*                   (a) Free Tx descriptor data areas
*                   (b) Free Tx descriptor memory block
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
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL transmit buffers
*                   and the Tx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_TxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_DATA  *pdev_data;
    NET_DEV_EMAC  *pdev_emac;
    MEM_POOL      *pmem_pool;
    DEV_DESC      *pdesc;
    CPU_INT08U    *p_buf;
    CPU_INT32U     reg_val;
    LIB_ERR        lib_err;
    NET_ERR        err;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_emac = (NET_DEV_EMAC *)pdev_data->EMAC_BaseAddr;       /* Overlay dev reg struct on top of dev base addr.      */

    pmem_pool = &pdev_data->TxDescPool;
    pdesc     =  pdev_data->TxDescFreeHeadPtr;

    while (pdesc != (DEV_DESC *)0) {
        reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                              pdev_data->ByteSwap);
        if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_OWNER)) {
                                                                /* ... dealloc tx buf (see Note #2a1).                  */
            p_buf = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(pdesc->BufPtr),
                                                               pdev_data->ByteSwap);
            NetIF_TxDeallocTaskPost(p_buf, &err);
           (void)&err;                                          /* Ignore possible dealloc err (see Note #2b2).         */
        }
        pdesc = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc->NextPtr),
                                                         pdev_data->ByteSwap);
    }

    pdesc = pdev_data->TxDescEMACHeadPtr;

    while (pdesc != (DEV_DESC *)0) {
        reg_val = NetDev_SwapData((CPU_INT32U)pdesc->PktFlagsLen,
                                              pdev_data->ByteSwap);
        if (DEF_BIT_IS_SET(reg_val, TI_EMAC_BIT_DESC_FLAG_OWNER)) {
                                                                /* ... dealloc tx buf (see Note #2a1).                  */
            p_buf = (CPU_INT08U *)NetDev_SwapData((CPU_INT32U)(pdesc->BufPtr),
                                                               pdev_data->ByteSwap);
            NetIF_TxDeallocTaskPost(p_buf, &err);
           (void)&err;                                          /* Ignore possible dealloc err (see Note #2b2).         */
        }
        pdesc = (DEV_DESC *)NetDev_SwapData((CPU_INT32U)(pdesc->NextPtr),
                                                         pdev_data->ByteSwap);
    }

    pdev_emac->TXnHDP[NET_DEV_TI_EMAC_CH_DFLT] = DEF_BIT_NONE;

    Mem_PoolBlkFree( pmem_pool,
                     pdev_data->TxDescStartPtr,
                    &lib_err);

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    pdev_data->TxDescStartPtr    = (DEV_DESC *)0;
    pdev_data->TxDescEMACHeadPtr = (DEV_DESC *)0;
    pdev_data->TxDescEMACTailPtr = (DEV_DESC *)0;
    pdev_data->TxDescFreeHeadPtr = (DEV_DESC *)0;
    pdev_data->TxDescFreeTailPtr = (DEV_DESC *)0;
#endif

    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_SwapData()
*
* Description : This function performs byte inversion of a 32-bit data.
*
* Argument(s) : word         32-bit data to be byte swapped.
*
*               byte_swap    Value to control the byte swapping.
*                                DEF_YES        word needs to be byte swapped.
*                                DEF_NO         word DOES NOT need to be byte swapped.
*
* Return(s)   : A 32-bit word.
*
* Caller(s)   : NetDev_Rx().
*               NetDev_RxDescInit().
*               NetDev_RxDescFreeAll().
*               NetDev_RxDescPtrCurInc().
*               NetDev_Tx().
*               NetDev_TxDescInit().
*               NetDev_TxDescFreeAll().
*               NetDev_ISR_Handler().
*
* Note(s)     : (1)This function is necessary for the TMS570LCxx family due to an Silicon ERRATA Rev A:
*                   (a) EXPEDTED BEHAVIOR: Writes and reads to EMAC CPPI packet buffer descriptors should
*                       be in the big endian 32(BE32) format of the device.
*
*                   (b) ISSUE : The EMAC controller needs the 32-bit words of the packet buffer descriptors
*                       in little endian(LE) format.
*
*                   (c) WORKAROUND : The software must apply byte swapping before writing data to the CPPI
*                       memory, or after reading the CPPI memory.
*
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_SwapData (CPU_INT32U  word,
                                     CPU_INT08U  byte_swap)
{
    CPU_INT32U  val;


    if (byte_swap == DEF_YES) {                                 /* See Note 1.                                          */
        val = (((word << 24u) & 0xFF000000u)  |
               ((word <<  8u) & 0x00FF0000u)  |
               ((word >>  8u) & 0x0000FF00u)  |
               ((word >> 24u) & 0x000000FFu));
    } else {
        val = word;
    }

    return (val);
}


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_ETHER_MODULE_EN */
