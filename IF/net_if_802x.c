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
*                                       NETWORK INTERFACE LAYER
*
*                                                802x
*
* Filename : net_if_802x.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Ethernet as described in RFC #894; supports IEEE 802 as described in RFC #1042.
*
*            (2) Ethernet implementation conforms to RFC #1122, Section 2.3.3, bullets (a) & (b), but
*                does NOT implement bullet (c) :
*
*                RFC #1122                  LINK LAYER                  October 1989
*
*                2.3.3  ETHERNET (RFC-894) and IEEE 802 (RFC-1042) ENCAPSULATION
*
*                       Every Internet host connected to a 10Mbps Ethernet cable :
*
*                       (a) MUST be able to send and receive packets using RFC-894 encapsulation;
*
*                       (b) SHOULD be able to receive RFC-1042 packets, intermixed with RFC-894 packets; and
*
*                       (c) MAY be able to send packets using RFC-1042 encapsulation.
*
*            (3) REQUIREs the following network protocol files in network directories :
*
*                (a) Network Interface Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\IF\
*
*                (b) Address Resolution Protocol    Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\
*
*                    See also 'net_arp.h  Note #1'.
*
*                         where
*                                 <Network Protocol Suite>    directory path for network protocol suite
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_IF_MODULE_802x


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_if_802x.h"
#include  "../Source/net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#include  "../IP/IPv4/net_icmpv4.h"
#endif

#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ndp.h"
#include  "../IP/IPv6/net_ipv6.h"
#endif

#include  "net_if.h"
#include  "../Source/net.h"
#include  "../Source/net_util.h"
#include  "../Source/net_ctr.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IF_802x_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            802x DEFINES
*********************************************************************************************************
*/

                                                                /* ---------------- ETHER FRAME TYPES ----------------- */
#define  NET_IF_802x_FRAME_TYPE_IPv4                  0x0800u
#define  NET_IF_802x_FRAME_TYPE_IPv6                  0x86DDu
#define  NET_IF_802x_FRAME_TYPE_ARP                   0x0806u
#define  NET_IF_802x_FRAME_TYPE_RARP                  0x8035u   /* See 'net_def.h  NETWORK PROTOCOL TYPES  Note #1'.    */


/*
*********************************************************************************************************
*                                          IEEE 802 DEFINES
*
* Note(s) : (1) SNAP 'Organizational Unique Identifier' (OUI) abbreviated to 'SNAP' for some SNAP OUI
*               codes to enforce ANSI-compliance of 31-character symbol length uniqueness.
*
*           (2) Default SNAP 'Organizational Unique Identifier' (OUI) IEEE 802.2 frame type is ALWAYS
*               Ethernet frame type (see 'IEEE 802 HEADER / FRAME  Note #1').
*********************************************************************************************************
*/

#define  NET_IF_IEEE_802_FRAME_LEN_MAX                  (NET_IF_MTU_IEEE_802 + NET_IF_HDR_SIZE_BASE_IEEE_802)

                                                                /* ------- IEEE 802.2 LOGICAL LINK CONTROL (LLC) ------ */
#define  NET_IF_IEEE_802_LLC_DSAP                       0xAAu
#define  NET_IF_IEEE_802_LLC_SSAP                       0xAAu
#define  NET_IF_IEEE_802_LLC_CTRL                       0x03u

                                                                /* --- IEEE 802.2 SUB-NETWORK ACCESS PROTOCOL (SNAP) -- */
#define  NET_IF_IEEE_802_SNAP_CODE_ETHER            0x000000u   /*    Dflt  SNAP org code (Ether) [see Note #2].        */
#define  NET_IF_IEEE_802_SNAP_CODE_00                   0x00u   /*    Dflt  SNAP org code, octet #00.                   */
#define  NET_IF_IEEE_802_SNAP_CODE_01                   0x00u   /*    Dflt  SNAP org code, octet #01.                   */
#define  NET_IF_IEEE_802_SNAP_CODE_02                   0x00u   /*    Dflt  SNAP org code, octet #02.                   */

#define  NET_IF_IEEE_802_SNAP_TYPE_IPv4                  NET_IF_802x_FRAME_TYPE_IPv4
#define  NET_IF_IEEE_802_SNAP_TYPE_IPv6                  NET_IF_802x_FRAME_TYPE_IPv6
#define  NET_IF_IEEE_802_SNAP_TYPE_ARP                   NET_IF_802x_FRAME_TYPE_ARP
#define  NET_IF_IEEE_802_SNAP_TYPE_RARP                  NET_IF_802x_FRAME_TYPE_RARP



/*
*********************************************************************************************************
*                                  NETWORK INTERFACE HEADER DEFINES
*
* Note(s) : (1) NET_IF_HDR_SIZE_ETHER_MAX's ideal #define'tion :
*
*                   (A) max( Ether Hdr, IEEE 802 Hdr )
*
*               (a) However, since NET_IF_HDR_SIZE_ETHER_MAX is used ONLY for network transmit & IEEE 802
*                   is NEVER transmitted (see 'net_if_ether.h  Note #2'), NET_IF_HDR_SIZE_ETHER_MAX  MUST
*                   be #define'd with hard-coded knowledge that Ethernet is the only supported frame
*                   encapsulation for network transmit.
*
*           (2) The following network interface value MUST be pre-#define'd in 'net_def.h' PRIOR to
*               'net_cfg.h' so that the developer can configure the network interface for the correct
*               network interface link layer values (see 'net_def.h  NETWORK INTERFACE LAYER DEFINES'
*               & 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #4') :
*
*               (a) NET_IF_HDR_SIZE_ETHER                 14
*********************************************************************************************************
*/

#define  NET_IF_HDR_SIZE_BASE_ETHER                       14    /* Ethernet base hdr size.                              */
#define  NET_IF_HDR_SIZE_BASE_IEEE_802                     8    /* IEEE 802 base hdr size.                              */

#if 0                                                           /* See Note #2a.                                        */
#define  NET_IF_HDR_SIZE_ETHER                           NET_IF_HDR_SIZE_BASE_ETHER
#endif
#define  NET_IF_HDR_SIZE_IEEE_802                       (NET_IF_HDR_SIZE_BASE_ETHER + NET_IF_HDR_SIZE_BASE_IEEE_802)

#define  NET_IF_HDR_SIZE_ETHER_MIN              (DEF_MIN(NET_IF_HDR_SIZE_ETHER, NET_IF_HDR_SIZE_IEEE_802))
#define  NET_IF_HDR_SIZE_ETHER_MAX                       NET_IF_HDR_SIZE_ETHER




/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                                /* ------------ 802x ADDRs ------------ */
static  const  CPU_INT08U  NetIF_802x_AddrNull[NET_IF_802x_ADDR_SIZE] = {
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u
};

static  const  CPU_INT08U  NetIF_802x_AddrBroadcast[NET_IF_802x_ADDR_SIZE] = {
    0xFFu,
    0xFFu,
    0xFFu,
    0xFFu,
    0xFFu,
    0xFFu
};

#ifdef  NET_MCAST_MODULE_EN
static  const  CPU_INT08U  NetIF_802x_AddrMulticastMask[NET_IF_802x_ADDR_SIZE] = {
    0x01u,
    0x00u,
    0x00u,
    0x00u,
    0x00u,
    0x00u
};
#endif

#ifdef  NET_MCAST_TX_MODULE_EN
#ifdef  NET_IPv4_MODULE_EN
static  const  CPU_INT08U  NetIF_802x_AddrMulticastBaseIPv4[NET_IF_802x_ADDR_SIZE] = {
    0x01u,
    0x00u,
    0x5Eu,
    0x00u,
    0x00u,
    0x00u
};
#endif
#endif

#ifdef  NET_MCAST_MODULE_EN
#ifdef  NET_IPv6_MODULE_EN
static  const  CPU_INT08U  NetIF_802x_AddrMulticastBaseIPv6[NET_IF_802x_ADDR_SIZE] = {
    0x33u,
    0x33u,
    0x00u,
    0x00u,
    0x00u,
    0x00u
};
#endif
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                             NETWORK INTERFACE HEADER / FRAME DATA TYPES
*********************************************************************************************************
*/

                                                                /* ----------------- NET 802x IF DATA ----------------- */
typedef  struct  net_if_data_802x {
    CPU_INT08U  HW_Addr[NET_IF_802x_ADDR_SIZE];                 /* 802x IF's dev hw addr.                               */
} NET_IF_DATA_802x;


/*
*********************************************************************************************************
*                                 ETHERNET HEADER / FRAME DATA TYPES
*
* Note(s) : (1) Frame 'Data' buffer CANNOT be declared to force word-alignment.  'Data' buffer MUST immediately
*               follow frame 'Hdr' since Ethernet frames are contiguous, non-aligned data packets.
*
*           (2) 'Data' declared with 1 entry; prevents removal by compiler optimization.
*
*           (3) Frame CRC's are computed/validated by an Ethernet device.  NO software CRC is handled for
*               receive or transmit.
*********************************************************************************************************
*/

                                                                /* ----------------- NET IF ETHER HDR ----------------- */
typedef  struct  net_if_hdr_ether {
    CPU_INT08U        AddrDest[NET_IF_802x_ADDR_SIZE];          /* MAC dest addr.                                       */
    CPU_INT08U        AddrSrc[NET_IF_802x_ADDR_SIZE];           /* MAC src  addr.                                       */
    CPU_INT16U        FrameType;                                /* Frame type.                                          */
} NET_IF_HDR_ETHER;


/*
*********************************************************************************************************
*                                 IEEE 802 HEADER / FRAME DATA TYPES
*
* Note(s) : (1) Header 'SNAP_OrgCode' defines the SNAP 'Organizational Unique Identifier' (OUI).  The OUI
*               indicates the various organization/vendor/manufacturer with each organization then defining
*               their respective frame types.
*
*               However, the default SNAP OUI indicates Ethernet frame types & is ALWAYS used.  ALL other
*               OUI's are discarded as invalid.
*
*               See also 'IEEE 802 DEFINES  Notes #1 & #2'.
*
*           (2) Frame 'Data' buffer CANNOT be declared to force word-alignment.  'Data' buffer MUST immediately
*               follow frame 'Hdr' since Ethernet frames are contiguous, non-aligned data packets.
*
*           (3) 'Data' declared with 1 entry; prevents removal by compiler optimization.
*
*           (4) Frame CRC's are computed/validated by an Ethernet device.  NO software CRC is handled for
*               receive or transmit.
*********************************************************************************************************
*/

                                                                /* --------------- NET IF IEEE 802 HDR ---------------- */
typedef  struct  net_if_hdr_ieee_802 {
    CPU_INT08U           AddrDest[NET_IF_802x_ADDR_SIZE];       /* IEEE 802.3 dest  addr.                               */
    CPU_INT08U           AddrSrc[NET_IF_802x_ADDR_SIZE];        /* IEEE 802.3 src   addr.                               */
    CPU_INT16U           FrameLen;                              /* IEEE 802.3 frame len.                                */

                                                                /* ------ IEEE 802.2 LOGICAL LINK CONTROL (LLC) ------- */
    CPU_INT08U           LLC_DSAP;                              /* Dest Serv Access Pt.                                 */
    CPU_INT08U           LLC_SSAP;                              /* Src  Serv Access Pt.                                 */
    CPU_INT08U           LLC_Ctrl;                              /* Ctrl Field.                                          */

                                                                /* -- IEEE 802.2 SUB-NETWORK ACCESS PROTOCOL (SNAP) --- */
    CPU_INT08U           SNAP_OrgCode[NET_IF_IEEE_802_SNAP_CODE_SIZE];  /* Org code (see Note #1).                      */
    CPU_INT16U           SNAP_FrameType;                                /* IEEE 802.2 frame type.                       */
} NET_IF_HDR_IEEE_802;




/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                                    /* ----------- RX FNCTS ----------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIF_802x_RxPktValidateBuf      (NET_BUF_HDR            *p_buf_hdr,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);
#endif

static  void  NetIF_802x_RxPktFrameDemux       (NET_IF                 *p_if,
                                                NET_BUF                *p_buf,
                                                NET_BUF_HDR            *p_buf_hdr,
                                                NET_IF_HDR_802x        *p_if_hdr,
                                                NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);

static  void  NetIF_802x_RxPktFrameDemuxEther  (NET_IF                 *p_if,
                                                NET_BUF_HDR            *p_buf_hdr,
                                                NET_IF_HDR_802x        *p_if_hdr,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);

static  void  NetIF_802x_RxPktFrameDemuxIEEE802(NET_IF                 *p_if,
                                                NET_BUF_HDR            *p_buf_hdr,
                                                NET_IF_HDR_802x        *p_if_hdr,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);

static  void  NetIF_802x_RxPktDiscard          (NET_BUF                *p_buf,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);


                                                                                    /* ----------- TX FNCTS ----------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIF_802x_TxPktValidate         (NET_BUF_HDR            *p_buf_hdr,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);
#endif

static  void  NetIF_802x_TxPktPrepareFrame     (NET_IF                 *p_if,
                                                NET_BUF                *p_buf,
                                                NET_BUF_HDR            *p_buf_hdr,
                                                NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);

static  void  NetIF_802x_TxPktDiscard          (NET_BUF                *p_buf,
                                                NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                NET_ERR                *p_err);

static  void  NetIF_802x_TxIxDataGet           (CPU_INT16U             *p_ix,
                                                NET_ERR                *p_err);


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          NetIF_802x_Init()
*
* Description : (1) Initialize 802x Module :
*
*                   Module initialization NOT yet required/implemented
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     802x network interface module
*                                                                       successfully initialized.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_802x_Init (NET_ERR  *p_err)
{
   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetIF_802x_Rx()
*
* Description : (1) Process received data packets & forward to network protocol layers :
*
*                   (a) Update link status
*                   (b) Validate packet received
*                   (c) Demultiplex packet to higher-layer protocols
*
*
* Argument(s) : p_if            Pointer to an 802x network interface.
*               ----            Argument validated in NetIF_RxHandler().
*
*               p_buf           Pointer to a network buffer that received a packet.
*               -----           Argument checked   in NetIF_Ether_Rx(),
*                                                     NetIF_WiFi_Rx().
*
*               p_ctrs_stat     Pointer to an 802x network interface statistic counters.
*
*               p_ctrs_err      Pointer to an 802x network interface error     counters.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 802x packet successfully received & processed.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*
*                                                               -- RETURNED BY NetIF_802x_RxPktDiscard() : ---
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Rx(),
*               NetIF_WiFi_Rx().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) If a network interface receives a packet, its physical link must be 'UP' & the
*                   interface's physical link state is set accordingly.
*
*                   (a) An attempt to check for link state is made after an interface has been started.
*                       However, many physical layer devices, such as Ethernet physical layers require
*                       several seconds for Auto-Negotiation to complete before the link becomes
*                       established.  Thus the interface link flag is not updated until the link state
*                       timer expires & one or more attempts to check for link state have been completed.
*
*               (3) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

void  NetIF_802x_Rx (NET_IF                 *p_if,
                     NET_BUF                *p_buf,
                     NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                     NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                     NET_ERR                *p_err)
{
    NET_BUF_HDR      *p_buf_hdr;
    NET_IF_HDR_802x  *p_if_hdr;
    CPU_BOOLEAN       size_valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------ VALIDATE PTRS ------------------- */
#if (NET_CTR_CFG_STAT_EN        == DEF_ENABLED)
    if (p_ctrs_stat == (NET_CTR_IF_802x_STATS *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif
#if (NET_CTR_CFG_ERR_EN         == DEF_ENABLED)
    if (p_ctrs_err == (NET_CTR_IF_802x_ERRS *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif
#endif


                                                                /* ---------------- UPDATE LINK STATUS ---------------- */
    if (p_if->En != DEF_YES) {
        NetIF_802x_RxPktDiscard(p_buf, p_ctrs_err, p_err);
        return;
    }

    p_if->Link = NET_IF_LINK_UP;                                /* See Note #2.                                         */


                                                                /* ------------ VALIDATE RX'D 802x IF PKT ------------- */
    p_buf_hdr = &p_buf->Hdr;
    size_valid = NetIF_802x_PktSizeIsValid(p_buf_hdr->TotLen);
    if (size_valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
        NetIF_802x_RxPktDiscard(p_buf, p_ctrs_err, p_err);
        return;
    }

    NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.RxPktCtr);
    NET_CTR_STAT_INC(p_ctrs_stat->RxPktCtr);


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIF_802x_RxPktValidateBuf(p_buf_hdr, p_ctrs_err, p_err);  /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIF_802x_RxPktDiscard(p_buf, p_ctrs_err, p_err);
             return;
    }
#endif


                                                                /* -------------- DEMUX RX'D 802x IF PKT -------------- */
    p_if_hdr = (NET_IF_HDR_802x *)&p_buf->DataPtr[p_buf_hdr->IF_HdrIx];
    NetIF_802x_RxPktFrameDemux(p_if,                            /* Demux pkt to appropriate net protocol.               */
                               p_buf,
                               p_buf_hdr,
                               p_if_hdr,
                               p_ctrs_stat,
                               p_ctrs_err,
                               p_err);


                                                                /* ----------------- UPDATE RX STATS ------------------ */
    switch (*p_err) {
#ifdef  NET_ARP_MODULE_EN
        case NET_ARP_ERR_NONE:
#endif
#ifdef  NET_NDP_MODULE_EN
        case NET_NDP_ERR_NONE:
#endif
#ifdef  NET_IPv4_MODULE_EN
        case NET_IPv4_ERR_NONE:
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_IPv6_ERR_NONE:
#endif
            *p_err = NET_IF_ERR_NONE;
             break;


        case NET_ERR_RX:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxPktDisCtr);
             NET_CTR_ERR_INC(p_ctrs_err->RxPktDisCtr);
                                                                /* Rtn err from NetIF_802x_RxPktFrameDemux().           */
             return;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_IF_ERR_INVALID_ADDR_DEST:
        case NET_IF_ERR_INVALID_ADDR_SRC:
        case NET_IF_ERR_INVALID_LEN_FRAME:
        case NET_IF_ERR_INVALID_ETHER_TYPE:
        case NET_IF_ERR_INVALID_LLC_DSAP:
        case NET_IF_ERR_INVALID_LLC_SSAP:
        case NET_IF_ERR_INVALID_LLC_CTRL:
        case NET_IF_ERR_INVALID_SNAP_CODE:
        case NET_IF_ERR_INVALID_SNAP_TYPE:
        default:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.RxPktDisCtr);
             NET_CTR_STAT_INC(p_ctrs_stat->RxPktDisCtr);
             NetIF_802x_RxPktDiscard(p_buf, p_ctrs_err, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                           NetIF_802x_Tx()
*
* Description : (1) Prepare data packets from network protocol layers for transmit :
*
*                   (a) Validate packet to transmit
*                   (b) Prepare data packets with appropriate Ethernet frame format
*
*
* Argument(s) : p_if            Pointer to an 802x network interface.
*               ----            Argument validated in NetIF_TxHandler().
*
*               p_buf           Pointer to a network buffer with data packet to transmit.
*               -----           Argument checked   in NetIF_Ether_Tx(),
*                                                     NetIF_WiFi_Tx().
*
*               p_ctrs_stat     Pointer to an 802x network interface statistic counters.
*
*               p_ctrs_err      Pointer to an 802x network interface error     counters.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 802x Packet successfully prepared for transmission.
*                               NET_IF_ERR_TX_ADDR_PEND         802x Packet successfully prepared & queued for
*                                                                   later transmission.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*
*                                                               ----- RETURNED BY NetIF_802x_TxPktDiscard() : -----
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Tx(),
*               NetIF_WiFi_Tx().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_802x_Tx (NET_IF                 *p_if,
                     NET_BUF                *p_buf,
                     NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                     NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                     NET_ERR                *p_err)
{
    NET_IF_HDR_ETHER  *p_if_hdr_ether;
    NET_BUF_HDR       *p_buf_hdr;
    CPU_INT16U         frame_type;


    p_buf_hdr = &p_buf->Hdr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------ VALIDATE PTRS ------------------- */
#if (NET_CTR_CFG_STAT_EN        == DEF_ENABLED)
    if (p_ctrs_stat == (NET_CTR_IF_802x_STATS *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif
#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    if (p_ctrs_err == (NET_CTR_IF_802x_ERRS *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif
                                                                /* --------------- VALIDATE 802x TX PKT --------------- */
    NetIF_802x_TxPktValidate(p_buf_hdr, p_ctrs_err, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_IF_ERR_INVALID_LEN_DATA:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        case NET_ERR_INVALID_PROTOCOL:
        default:
             NetIF_802x_TxPktDiscard(p_buf, p_ctrs_err, p_err);
             return;
    }
#endif


                                                                /* -------------- PREPARE 802x TX FRAME --------------- */
    NetIF_802x_TxPktPrepareFrame(p_if,
                                 p_buf,
                                 p_buf_hdr,
                                 p_ctrs_stat,
                                 p_ctrs_err,
                                 p_err);
    switch (*p_err) {
        case NET_IF_ERR_TX_RDY:
        case NET_IF_ERR_TX_BROADCAST:
             break;


        case NET_IF_ERR_TX_ADDR_REQ:
        case NET_IF_ERR_TX_MULTICAST:
             p_if_hdr_ether = (NET_IF_HDR_ETHER *)&p_buf->DataPtr[p_buf_hdr->IF_HdrIx];
             frame_type     =  NET_UTIL_VAL_GET_NET_16(&p_if_hdr_ether->FrameType);
             switch (frame_type) {
#ifdef  NET_IPv4_MODULE_EN
                 case NET_IF_802x_FRAME_TYPE_ARP:
                 case NET_IF_802x_FRAME_TYPE_IPv4:
#ifdef  NET_ARP_MODULE_EN
                      NetARP_CacheHandler(p_buf, p_err);
#else
                     *p_err = NET_ERR_FAULT_FEATURE_DIS;
#endif
                      break;
#endif


#ifdef  NET_IPv6_MODULE_EN
                 case NET_IF_802x_FRAME_TYPE_IPv6:
#ifdef  NET_NDP_MODULE_EN
                      NetNDP_NeighborCacheHandler(p_buf, p_err);
#else
                     *p_err = NET_ERR_FAULT_FEATURE_DIS;
#endif
                      break;
#endif


                 default:
                     *p_err = NET_IF_ERR_INVALID_PROTOCOL;
                      return;
                    }
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_LEN:
        default:
             NetIF_802x_TxPktDiscard(p_buf, p_ctrs_err, p_err);
             return;
    }


                                                                /* ----------------- UPDATE TX STATS ------------------ */
    switch (*p_err) {                                           /* Chk err from                       ...               */
        case NET_IF_ERR_TX_RDY:                                 /* ... NetIF_802x_TxPktPrepareFrame() ...               */
        case NET_IF_ERR_TX_BROADCAST:
        case NET_ARP_ERR_CACHE_RESOLVED:                        /* ... or NetARP_CacheHandler().                        */
        case NET_NDP_ERR_NEIGHBOR_CACHE_STALE:
        case NET_NDP_ERR_NEIGHBOR_CACHE_RESOLVED:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.TxPktCtr);
             NET_CTR_STAT_INC(p_ctrs_stat->TxPktCtr);
            *p_err = NET_IF_ERR_NONE;
             break;


        case NET_ARP_ERR_CACHE_PEND:
        case NET_NDP_ERR_NEIGHBOR_CACHE_PEND:
            *p_err = NET_IF_ERR_TX_ADDR_PEND;                   /* Tx pending on hw addr; will tx when addr resolved.   */
             return;

        default:
             NetIF_802x_TxPktDiscard(p_buf, p_ctrs_err, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                       NetIF_802x_AddrHW_Get()
*
* Description : Get an 802x interface's hardware address.
*
* Argument(s) : p_if        Pointer to an 802x network interface.
*               ----        Argument validated in NetIF_AddrHW_GetHandler().
*
*               p_addr_hw   Pointer to variable that will receive the hardware address (see Note #1).
*               ---------   Argument checked   in NetIF_AddrHW_GetHandler().
*
*               p_addr_len  Pointer to a variable to ... :
*               ----------
*                               (a) Pass the length of the address buffer pointed to by 'paddr_hw'.
*                               (b) (1) Return the actual size of the protocol address, if NO error(s);
*                                   (2) Return 0,                                       otherwise.
*
*                           Argument checked in NetIF_AddrHW_GetHandler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 802x interface's hardware address
*                                                                   successfully returned.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrHW_GetHandler() via 'pif_api->AddrHW_Get()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) An 802x hardware address is also known as a MAC (Media Access Control) address.
*
*                   (b) The hardware address is returned in network-order; i.e. the pointer to the hardware
*                       address points to the highest-order octet.
*
*               (2) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

void  NetIF_802x_AddrHW_Get (NET_IF      *p_if,
                             CPU_INT08U  *p_addr_hw,
                             CPU_INT08U  *p_addr_len,
                             NET_ERR     *p_err)
{
    NET_IF_DATA_802x  *p_if_data;
    CPU_INT08U         addr_len;


      addr_len = *p_addr_len;
   *p_addr_len =  0u;                                           /* Init len for err (see Note #2).                      */

                                                                /* ------------ VALIDATE 802x HW ADDR LEN ------------- */
    if (addr_len < NET_IF_802x_ADDR_SIZE) {
       *p_err    = NET_IF_ERR_INVALID_ADDR_LEN;
        return;
    }

                                                                /* ----------------- GET 802x HW ADDR ----------------- */
    p_if_data = (NET_IF_DATA_802x *)p_if->IF_Data;
    NET_UTIL_VAL_COPY(p_addr_hw,
                     &p_if_data->HW_Addr[0],
                      NET_IF_802x_ADDR_SIZE);

   *p_addr_len = NET_IF_802x_ADDR_SIZE;
   *p_err      = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetIF_802x_AddrHW_Set()
*
* Description : Set an 802x interface's hardware address.
*
* Argument(s) : p_if        Pointer to an 802x network interface.
*               ----        Argument validated in NetIF_AddrHW_SetHandler().
*
*               p_addr_hw   Pointer to hardware address (see Note #1).
*               ---------   Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 802x interface's hardware address
*                                                                   successfully set.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'paddr_hw' passed a NULL pointer.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrHW_SetHandler() via 'pif_api->AddrHW_Set()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) An 802x hardware address is also known as a MAC (Media Access Control) address.
*
*                   (b) The hardware address MUST be in network-order; i.e. the pointer to the hardware
*                       address MUST point to the highest-order octet.
*
*               (2) The interface MUST be stopped BEFORE setting a new hardware address which does NOT
*                   take effect until the interface is re-started.
*********************************************************************************************************
*/

void  NetIF_802x_AddrHW_Set (NET_IF      *p_if,
                             CPU_INT08U  *p_addr_hw,
                             CPU_INT08U   addr_len,
                             NET_ERR     *p_err)
{
    NET_IF_DATA_802x  *p_if_data;
    CPU_BOOLEAN        valid;

                                                                /* -------------- VALIDATE 802x HW ADDR --------------- */
    if (addr_len != NET_IF_802x_ADDR_SIZE) {
       *p_err = NET_IF_ERR_INVALID_ADDR_LEN;
        return;
    }

    valid = NetIF_802x_AddrHW_IsValid(p_if, p_addr_hw);
    if (valid != DEF_YES) {
       *p_err = NET_IF_ERR_INVALID_ADDR;
        return;
    }

                                                                /* ----------------- SET 802x HW ADDR ----------------- */
    p_if_data = (NET_IF_DATA_802x *)p_if->IF_Data;
    NET_UTIL_VAL_COPY(&p_if_data->HW_Addr[0],                   /* Set new hw addr (see Note #2).                       */
                       p_addr_hw,
                       NET_IF_802x_ADDR_SIZE);

   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIF_802x_AddrHW_IsValid()
*
* Description : (1) Validate an 802x hardware address which MUST NOT be one of the following :
*
*                   (a) 802x broadcast address               See RFC #894, Section 'Address Mappings :
*                                                                   Broadcast Address'
*
*
* Argument(s) : p_if        Pointer to an 802x network interface.
*               ----        Argument validated in NetIF_AddrHW_IsValidHandler(),
*                                                 NetIF_Ether_AddrHW_Set(),
*                                                 NetIF_Ether_RxPktFrameDemux(),
*                                                 NetIF_WiFi_AddrHW_Set(),
*                                                 NetIF_WiFi_RxPktFrameDemux().
*
*               p_addr_hw   Pointer to an 802x hardware address (see Note #1).
*               ---------   Argument checked   in NetIF_AddrHW_IsValidHandler(),
*                                                 NetIF_Ether_AddrHW_Set(),
*                                                 NetIF_WiFi_AddrHW_Set();
*                                    validated in NetIF_802x_RxPktFrameDemux().
*
* Return(s)   : DEF_YES, if 802x hardware address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIF_AddrHW_IsValidHandler() via 'pif_api->AddrHW_IsValid()',
*               NetIF_Ether_AddrHW_Set(),
*               NetIF_Ether_RxPktFrameDemux(),
*               NetIF_WiFi_AddrHW_Set(),
*               NetIF_WiFi_RxPktFrameDemux().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) An 802x hardware address is also known as a MAC (Media Access Control) address.
*
*                   (b) The hardware address MUST be in network-order; i.e. the pointer to the hardware
*                       address MUST point to the highest-order octet.
*
*                   (c) The size of the memory buffer that contains the 802x hardware address MUST be
*                       greater than or equal to NET_IF_802x_ADDR_SIZE.
*
*                   (d) 802x hardware address memory buffer array accessed by octets.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_802x_AddrHW_IsValid (NET_IF      *p_if,
                                        CPU_INT08U  *p_addr_hw)
{
    CPU_BOOLEAN  addr_null;
    CPU_BOOLEAN  addr_broadcast;
    CPU_BOOLEAN  addr_valid;


   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

                                                                /* ------------------- VALIDATE PTR ------------------- */
    if (p_addr_hw == (CPU_INT08U *)0) {
        switch (p_if->Type) {
#ifdef  NET_IF_ETHER_MODULE_EN
            case NET_IF_TYPE_ETHER:
                 NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Ether.IF_802xCtrs.NullPtrCtr);
                 break;
#endif


#ifdef  NET_IF_WIFI_MODULE_EN
            case NET_IF_TYPE_WIFI:
                 NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.IF_802xCtrs.NullPtrCtr);
                 break;
#endif

            default:
                 break;
        }
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.NullPtrCtr);
        return (DEF_NO);
    }

                                                                /* -------------- VALIDATE 802x HW ADDR --------------- */
    addr_null      = Mem_Cmp((void     *) p_addr_hw,
                             (void     *)&NetIF_802x_AddrNull[0],
                             (CPU_SIZE_T) NET_IF_802x_ADDR_SIZE);
    addr_broadcast = Mem_Cmp((void     *) p_addr_hw,
                             (void     *)&NetIF_802x_AddrBroadcast[0],
                             (CPU_SIZE_T) NET_IF_802x_ADDR_SIZE);

    addr_valid     = ((addr_null      == DEF_YES) ||
                      (addr_broadcast == DEF_YES)) ? DEF_NO : DEF_YES;

    return (addr_valid);
}


/*
*********************************************************************************************************
*                                    NetIF_802x_AddrMulticastAdd()
*
* Description : Add a multicast address to an 802x interface.
*
* Argument(s) : p_if                    Pointer to an 802x network interface to add address.
*               ----                    Argument validated in NetIF_AddrMulticastAdd().
*
*               p_addr_protocol         Pointer to a multicast protocol address to add (see Note #1).
*               ---------------         Argument checked   in NetIF_AddrMulticastAdd().
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                              NET_IF_ERR_NONE                  Network hardware address successfully added.
*                              NET_ERR_FAULT_NULL_FNCT             Invalid NULL function pointer.*
*                              NET_ERR_FAULT_FEATURE_DIS               Disabled     API function.
*                              NET_ERR_FAULT_NULL_OBJ           Invalid/NULL API configuration.
*
*                                                               - RETURNED BY NetIF_802x_AddrMulticastProtocolToHW() : -
*                              NET_IF_ERR_INVALID_ADDR_LEN      Invalid hardware/protocol
*                                                                   address length.
*                              NET_IF_ERR_INVALID_PROTOCOL      Invalid network protocol.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrMulticastAdd() via 'pif_api->AddrMulticastAdd()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The multicast protocol address MUST be in network-order.
*********************************************************************************************************
*/

void  NetIF_802x_AddrMulticastAdd (NET_IF             *p_if,
                                   CPU_INT08U         *p_addr_protocol,
                                   CPU_INT08U          addr_protocol_len,
                                   NET_PROTOCOL_TYPE   addr_protocol_type,
                                   NET_ERR            *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_API  *p_dev_api;
    CPU_INT08U    addr_hw[NET_IF_802x_ADDR_SIZE];
    CPU_INT08U    addr_hw_len;


    switch (addr_protocol_type) {
        case NET_PROTOCOL_TYPE_IP_V4:
             addr_hw_len = sizeof(addr_hw);
             NetIF_802x_AddrMulticastProtocolToHW((NET_IF          *) p_if,
                                                  (CPU_INT08U      *) p_addr_protocol,
                                                  (CPU_INT08U       ) addr_protocol_len,
                                                  (NET_PROTOCOL_TYPE) addr_protocol_type,
                                                  (CPU_INT08U      *)&addr_hw[0],
                                                  (CPU_INT08U      *)&addr_hw_len,
                                                  (NET_ERR         *) p_err);
             if (*p_err != NET_IF_ERR_NONE) {
                  return;
             }
             break;

        case NET_PROTOCOL_TYPE_IP_V6:
             addr_hw_len = sizeof(addr_hw);
             NetIF_802x_AddrMulticastProtocolToHW((NET_IF          *) p_if,
                                                  (CPU_INT08U      *) p_addr_protocol,
                                                  (CPU_INT08U       ) addr_protocol_len,
                                                  (NET_PROTOCOL_TYPE) addr_protocol_type,
                                                  (CPU_INT08U      *)&addr_hw[0],
                                                  (CPU_INT08U      *)&addr_hw_len,
                                                  (NET_ERR         *) p_err);
             if (*p_err != NET_IF_ERR_NONE) {
                  return;
             }
             break;
        default:
            *p_err = NET_IF_ERR_INVALID_PROTOCOL;
             return;
    }

    p_dev_api = (NET_DEV_API *)p_if->Dev_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (void *)0) {
       *p_err = NET_ERR_FAULT_NULL_OBJ;
        return;
    }
    if (p_dev_api->AddrMulticastAdd == (void (*)(NET_IF     *,
                                                 CPU_INT08U *,
                                                 CPU_INT08U  ,
                                                 NET_ERR    *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* Add multicast addr to dev.                           */
    p_dev_api->AddrMulticastAdd((NET_IF     *) p_if,
                                (CPU_INT08U *)&addr_hw[0],
                                (CPU_INT08U  ) addr_hw_len,
                                (NET_ERR    *) p_err);


#else
   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_protocol;
   (void)&addr_protocol_len;
   (void)&addr_protocol_type;


   *p_err = NET_ERR_FAULT_FEATURE_DIS;
#endif
}


/*
*********************************************************************************************************
*                                     NetIF_802x_AddrMulticastRemove()
*
* Description : Remove a multicast address from an 802x interface.
*
* Argument(s) : p_if                    Pointer to an 802x network interface to remove address.
*               ----                    Argument validated in NetIF_AddrMulticastRemove().
*
*               p_addr_protocol         Pointer to a multicast protocol address to remove (see Note #1).
*               ---------------         Argument checked   in NetIF_AddrMulticastRemove().
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network hardware address successfully removed.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled     API function.
*
*                                                               - RETURNED BY NetIF_802x_AddrMulticastProtocolToHW() : -
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware/protocol
*                                                                   address length.
*                               NET_IF_ERR_INVALID_PROTOCOL     Invalid network protocol.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrMulticastRemove() via 'pif_api->AddrMulticastRemove()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The multicast protocol address MUST be in network-order.
*********************************************************************************************************
*/

void  NetIF_802x_AddrMulticastRemove (NET_IF             *p_if,
                                      CPU_INT08U         *p_addr_protocol,
                                      CPU_INT08U          addr_protocol_len,
                                      NET_PROTOCOL_TYPE   addr_protocol_type,
                                      NET_ERR            *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV_API  *p_dev_api;
    CPU_INT08U    addr_hw[NET_IF_802x_ADDR_SIZE];
    CPU_INT08U    addr_hw_len;


    switch (addr_protocol_type) {
        case NET_PROTOCOL_TYPE_IP_V4:
        case NET_PROTOCOL_TYPE_IP_V6:
             addr_hw_len = sizeof(addr_hw);
             NetIF_802x_AddrMulticastProtocolToHW((NET_IF          *) p_if,
                                                  (CPU_INT08U      *) p_addr_protocol,
                                                  (CPU_INT08U       ) addr_protocol_len,
                                                  (NET_PROTOCOL_TYPE) addr_protocol_type,
                                                  (CPU_INT08U      *)&addr_hw[0],
                                                  (CPU_INT08U      *)&addr_hw_len,
                                                  (NET_ERR         *) p_err);
             if (*p_err != NET_IF_ERR_NONE) {
                  return;
             }
             break;


        default:
            *p_err = NET_IF_ERR_INVALID_PROTOCOL;
             return;
    }

    p_dev_api = (NET_DEV_API *)p_if->Dev_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (void *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_dev_api->AddrMulticastRemove == (void (*)(NET_IF     *,
                                                    CPU_INT08U *,
                                                    CPU_INT08U  ,
                                                    NET_ERR    *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* Remove multicast addr from dev.                       */
    p_dev_api->AddrMulticastRemove((NET_IF     *) p_if,
                                   (CPU_INT08U *)&addr_hw[0],
                                   (CPU_INT08U  ) addr_hw_len,
                                   (NET_ERR    *) p_err);


#else
   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_protocol;
   (void)&addr_protocol_len;
   (void)&addr_protocol_type;


   *p_err = NET_ERR_FAULT_FEATURE_DIS;
#endif
}


/*
*********************************************************************************************************
*                               NetIF_802x_AddrMulticastProtocolToHW()
*
* Description : Convert a multicast protocol address into an 802x address.
*
* Argument(s) : p_if                    Pointer to an 802x network interface to transmit the packet.
*               ----                    Argument validated in NetIF_AddrMulticastProtocolToHW().
*
*               p_addr_protocol         Pointer to a multicast protocol address to convert
*               ---------------             (see Note #1a).
*
*                                       Argument checked   in NetIF_AddrMulticastProtocolToHW().
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_addr_hw               Pointer to a variable that will receive the hardware address
*               ---------                   (see Note #1b).
*
*                                       Argument checked   in NetIF_AddrMulticastProtocolToHW().
*
*               p_addr_hw_len           Pointer to a variable to ... :
*               -------------
*                                           (a) Pass the length of the              hardware address
*                                                   argument, in octets.
*                                           (b) (1) Return the actual length of the hardware address,
*                                                       in octets, if NO error(s);
*                                               (2) Return 0, otherwise.
*
*                                       Argument checked   in NetIF_AddrMulticastProtocolToHW().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network protocol address successfully
*                                                                   converted.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware/protocol address length.
*                               NET_IF_ERR_INVALID_PROTOCOL     Invalid network protocol.
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_AddrMulticastAdd(),
*               NetIF_802x_AddrMulticastRemove(),
*               NetIF_AddrMulticastProtocolToHW() via 'pif_api->AddrMulticastProtocolToHW'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) The multicast protocol address MUST be in network-order.
*
*                   (b) The 802x hardware address is returned in network-order; i.e. the pointer to
*                       the hardware address points to the highest-order octet.
*
*               (2) Since 'paddr_hw_len' argument is both an input & output argument (see 'Argument(s) :
*                   paddr_hw_len'), ... :
*
*                   (a) Its input value SHOULD be validated prior to use; ...
*                   (b) While its output value MUST be initially configured to return a default value
*                       PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/

void  NetIF_802x_AddrMulticastProtocolToHW (NET_IF             *p_if,
                                            CPU_INT08U         *p_addr_protocol,
                                            CPU_INT08U          addr_protocol_len,
                                            NET_PROTOCOL_TYPE   addr_protocol_type,
                                            CPU_INT08U         *p_addr_hw,
                                            CPU_INT08U         *p_addr_hw_len,
                                            NET_ERR            *p_err)
{
#ifdef  NET_MCAST_TX_MODULE_EN
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
#ifdef NET_IPv4_MODULE_EN
    CPU_INT08U      addr_hw_len;
#endif
#endif
#ifdef NET_IPv4_MODULE_EN
    CPU_INT08U      addr_protocol_v4[NET_IPv4_ADDR_LEN];
#endif
#ifdef NET_IPv6_MODULE_EN
    NET_IPv6_ADDR   addr_protocol_v6;
#endif


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
#ifdef NET_IPv4_MODULE_EN
      addr_hw_len = *p_addr_hw_len;
#endif
#endif
#endif
   *p_addr_hw_len =  0u;                                        /* Cfg dflt addr len for err (see Note #2b).            */

#ifdef  NET_MCAST_TX_MODULE_EN
    switch (addr_protocol_type) {
        case NET_PROTOCOL_TYPE_IP_V4:
#ifdef NET_IPv4_MODULE_EN
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------ VALIDATE ADDRS ------------------ */
             if (addr_protocol_len != sizeof(NET_IPv4_ADDR)) {
                *p_err = NET_IF_ERR_INVALID_ADDR_LEN;
                 return;
             }
             if (addr_hw_len != NET_IF_802x_ADDR_SIZE) {
                *p_err = NET_IF_ERR_INVALID_ADDR_LEN;
                 return;
             }
#endif

             Mem_Clr(addr_protocol_v4, sizeof(addr_protocol_v4));

             NET_UTIL_VAL_COPY(&addr_protocol_v4[0],
                                p_addr_protocol,
                                addr_protocol_len);

             NET_UTIL_VAL_COPY(&p_addr_hw[0],
                               &NetIF_802x_AddrMulticastBaseIPv4[0],
                                NET_IF_802x_ADDR_SIZE);
             p_addr_hw[3] = addr_protocol_v4[1] & 0x7Fu;
             p_addr_hw[4] = addr_protocol_v4[2];
             p_addr_hw[5] = addr_protocol_v4[3];
#else
            *p_err = NET_ERR_FAULT_FEATURE_DIS;
#endif
             break;


        case NET_PROTOCOL_TYPE_IP_V6:
#ifdef NET_IPv6_MODULE_EN
             Mem_Copy(&addr_protocol_v6.Addr[0],
                       p_addr_protocol,
                       addr_protocol_len);

             Mem_Copy(&p_addr_hw[0],
                      &NetIF_802x_AddrMulticastBaseIPv6[0],
                       NET_IF_802x_ADDR_SIZE);
             p_addr_hw[2] = addr_protocol_v6.Addr[12];
             p_addr_hw[3] = addr_protocol_v6.Addr[13];
             p_addr_hw[4] = addr_protocol_v6.Addr[14];
             p_addr_hw[5] = addr_protocol_v6.Addr[15];
#else
            *p_err = NET_ERR_FAULT_FEATURE_DIS;
#endif
             break;


        default:
            *p_err = NET_IF_ERR_INVALID_PROTOCOL;
             return;
    }

   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

   *p_addr_hw_len = NET_IF_802x_ADDR_SIZE;
   *p_err         = NET_IF_ERR_NONE;


#else
   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_protocol;
   (void)&addr_protocol_len;
   (void)&addr_protocol_type;
   (void)&p_addr_hw;

   *p_addr_hw_len = 0u;                                         /* Cfg dflt addr len for err (see Note #2b).            */
   *p_err         = NET_ERR_FAULT_FEATURE_DIS;
#endif
}


/*
*********************************************************************************************************
*                                   NetIF_802x_BufPoolCfgValidate()
*
* Description : (1) Validate 802x network buffer pool configuration :
*
*                   (a) Validate configured size of network buffers
*
*
* Argument(s) : p_if        Pointer to an 802x network interface.
*               ----        Argument validated in NetIF_BufPoolInit().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 802x buffer pool configuration valid.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size of 802x buffer data areas
*                                                                   configured.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_BufPoolInit() via 'pif_api->BufPoolCfgValidate()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) All 802x buffer data area sizes MUST be configured greater than or equal to
*                   NET_IF_802x_BUF_SIZE_MIN.
*********************************************************************************************************
*/

void  NetIF_802x_BufPoolCfgValidate (NET_IF   *p_if,
                                     NET_ERR  *p_err)
{
    NET_DEV_CFG  *p_dev_cfg;
    CPU_INT16U    net_if_802x_buf_size_min;


    p_dev_cfg = (NET_DEV_CFG *)p_if->Dev_Cfg;

    net_if_802x_buf_size_min = 0u;
    NetIF_802x_TxIxDataGet(&net_if_802x_buf_size_min,
                            p_err);


                                                                    /* ----------- VALIDATE BUF DATA SIZES ------------ */
    if (p_dev_cfg->RxBufLargeSize < net_if_802x_buf_size_min) {     /*     Validate large rx buf size (see Note #2).    */
       *p_err = NET_BUF_ERR_INVALID_SIZE;
        return;
    }

    if (p_dev_cfg->TxBufLargeNbr > 0) {                              /* If any large tx bufs cfg'd, ...                  */
        if (p_dev_cfg->TxBufLargeSize < net_if_802x_buf_size_min) { /* ... validate large tx buf size (see Note #2).    */
           *p_err = NET_BUF_ERR_INVALID_SIZE;
            return;
        }
    }

    if (p_dev_cfg->TxBufSmallNbr > 0) {                              /* If any small tx bufs cfg'd, ...                  */
        if (p_dev_cfg->TxBufSmallSize < net_if_802x_buf_size_min) {  /* ... validate small tx buf size (see Note #2).    */
           *p_err = NET_BUF_ERR_INVALID_SIZE;
            return;
        }
    }


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetIF_802x_MTU_Set()
*
* Description : Set 802x interface's MTU.
*
* Argument(s) : p_if        Pointer to an 802x network interface.
*               ----        Argument validated in NetIF_MTU_SetHandler().
*
*               mtu         Desired maximum transmission unit (MTU) size to configure (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 802x interface's MTU successfully set.
*                               NET_IF_ERR_INVALID_MTU          Invalid MTU.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_MTU_SetHandler() via 'pif_api->MTU_Set()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_802x_MTU_Set (NET_IF   *p_if,
                          NET_MTU   mtu,
                          NET_ERR  *p_err)
{
    NET_DEV_CFG   *p_dev_cfg;
    NET_BUF_SIZE   buf_size_max;
    NET_MTU        mtu_max;


    p_dev_cfg     = (NET_DEV_CFG *)p_if->Dev_Cfg;
    buf_size_max  =  DEF_MAX((p_dev_cfg->TxBufLargeSize - p_dev_cfg->TxBufIxOffset),
                             (p_dev_cfg->TxBufSmallSize - p_dev_cfg->TxBufIxOffset));
    mtu_max       =  DEF_MIN(mtu, buf_size_max);

    if (mtu <= mtu_max) {
        p_if->MTU = mtu;
       *p_err     = NET_IF_ERR_NONE;
    } else {
       *p_err     = NET_IF_ERR_INVALID_MTU;
    }
}


/*
*********************************************************************************************************
*                                     NetIF_802x_GetPktSizeHdr()
*
* Description : Get 802x packet header size.
*
* Argument(s) : p_if    Pointer to an 802x network interface.
*               ----    Argument validated in NetIF_MTU_GetProtocol().
*
* Return(s)   : The 802x packet header size.
*
* Caller(s)   : NetIF_MTU_GetProtocol() via 'pif_api->GetPktSizeHdr()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_802x_GetPktSizeHdr (NET_IF  *p_if)
{
    CPU_INT16U  pkt_size;


   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    pkt_size = (CPU_INT16U)NET_IF_HDR_SIZE_ETHER;

    return (pkt_size);
}


/*
*********************************************************************************************************
*                                     NetIF_802x_GetPktSizeMin()
*
* Description : Get minimum allowable 802x packet size.
*
* Argument(s) : p_if    Pointer to an 802x network interface.
*               ----    Argument validated in NetIF_GetPktSizeMin().
*
* Return(s)   : The minimum allowable 802x packet size.
*
* Caller(s)   : NetIF_GetPktSizeMin() via 'pif_api->GetPktSizeMin()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_802x_GetPktSizeMin (NET_IF  *p_if)
{
    CPU_INT16U  pkt_size;


   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    pkt_size = (CPU_INT16U)NET_IF_802x_FRAME_MIN_SIZE;

    return (pkt_size);
}



/*
*********************************************************************************************************
*                                      NetIF_802x_GetPktSizeMax()
*
* Description : Get maximum allowable 802x packet size.
*
* Argument(s) : p_if    Pointer to an 802x network interface.
*               ----    Argument validated in NetIF_GetPktSizeMin().
*
* Return(s)   : The maximum allowable 802x packet size.
*
* Caller(s)   : NetIF_GetPktSizeMax() via 'pif_api->GetPktSizeMax()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_802x_GetPktSizeMax (NET_IF  *p_if)
{
    CPU_INT16U  pkt_size;


   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    pkt_size = (CPU_INT16U)NET_IF_ETHER_FRAME_MAX_SIZE;

    return (pkt_size);
}


/*
*********************************************************************************************************
*                                     NetIF_802x_PktSizeIsValid()
*
* Description : Validate an 802x packet size.
*
* Argument(s) : size    Size of 802x packet frame (in octets).
*
* Return(s)   : DEF_YES, if 802x packet size valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIF_Ether_Rx(),
*               NetIF_WiFi_Rx().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_802x_PktSizeIsValid (CPU_INT16U  size)
{
    CPU_BOOLEAN  valid;


    valid = DEF_YES;

    if (size  < NET_IF_802x_FRAME_MIN_SIZE) {
        valid = DEF_NO;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                      NetIF_802x_ISR_Handler()
*
* Description : Handle Wireless device's interrupt service routine (ISR) function(s).
*
* Argument(s) : p_if        Pointer to an 802x network interface.
*               ----        Argument validated in NetIF_ISR_Handler().
*
*               type        Device interrupt type(s) to handle :
*
*                               NET_DEV_ISR_TYPE_UNKNOWN                Handle unknown device           ISR(s).
*                               NET_DEV_ISR_TYPE_RX                     Handle device receive           ISR(s).
*                               NET_DEV_ISR_TYPE_RX_RUNT                Handle device runt              ISR(s).
*                               NET_DEV_ISR_TYPE_RX_OVERRUN             Handle device receive overrun   ISR(s).
*                               NET_DEV_ISR_TYPE_TX_RDY                 Handle device transmit ready    ISR(s).
*                               NET_DEV_ISR_TYPE_TX_COMPLETE            Handle device transmit complete ISR(s).
*                               NET_DEV_ISR_TYPE_TX_COLLISION_LATE      Handle device late   collision  ISR(s).
*                               NET_DEV_ISR_TYPE_TX_COLLISION_EXCESS    Handle device excess collision  ISR(s).
*                               NET_DEV_ISR_TYPE_JABBER                 Handle device jabber            ISR(s).
*                               NET_DEV_ISR_TYPE_BABBLE                 Handle device late babble       ISR(s).
*                               NET_DEV_ISR_TYPE_PHY                    Handle device physical layer    ISR(s).
*
*                                                                       See specific network device(s) for
*                                                                           additional device ISR(s).
*
*                           See also 'net_if.h  NETWORK DEVICE INTERRUPT SERVICE ROUTINE (ISR) TYPE DEFINES'
*                               for other available & supported network device ISR types.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wireless interface's device ISR(s)
*                                                                   successfully handled.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_ISR_Handler() via 'pif_api->ISR_Handler()'.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) NetIF_WiFi_ISR_Handler() is called within the context of an ISR & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST NOT block by pending on & acquiring the global network lock.
*
*                       (1) Although blocking on the global network lock is typically required since any
*                           external API function access is asynchronous to other network protocol tasks;
*                           interrupt service routines (ISRs) are (typically) prohibited from pending on
*                           OS objects & therefore can NOT acquire the global network lock.
*
*                       (2) Therefore, ALL network interface & network device driver functions called by
*                           NetIF_WiFi_ISR_Handler() MUST be able to be asynchronously accessed without
*                           the global network lock & without corrupting any network data or task.
*
*               (2) Network device interrupt service routines (ISR) handler(s) SHOULD be able to correctly
*                   function regardless of whether their corresponding network interface(s) are enabled.
*
*                   See also Note #1b2.
*********************************************************************************************************
*/

void  NetIF_802x_ISR_Handler (NET_IF            *p_if,
                              NET_DEV_ISR_TYPE   type,
                              NET_ERR           *p_err)
{
    NET_DEV_API  *p_dev_api;


                                                                /* ---------------- VALIDATE ISR TYPE ---------------- */
    switch (type) {
        case NET_DEV_ISR_TYPE_UNKNOWN:
        case NET_DEV_ISR_TYPE_RX:
        case NET_DEV_ISR_TYPE_RX_RUNT:
        case NET_DEV_ISR_TYPE_RX_OVERRUN:
        case NET_DEV_ISR_TYPE_TX_RDY:
        case NET_DEV_ISR_TYPE_TX_COMPLETE:
        case NET_DEV_ISR_TYPE_TX_COLLISION_LATE:
        case NET_DEV_ISR_TYPE_TX_COLLISION_EXCESS:
             p_dev_api = (NET_DEV_API *)p_if->Dev_API;
             if (p_dev_api->ISR_Handler == (void (*)(NET_IF         *,
                                                     NET_DEV_ISR_TYPE))0) {
                *p_err = NET_ERR_FAULT_NULL_FNCT;
                 return;
             }
             p_dev_api->ISR_Handler(p_if, type);
             break;


        case NET_DEV_ISR_TYPE_PHY:
        case NET_DEV_ISR_TYPE_JABBER:
        case NET_DEV_ISR_TYPE_BABBLE:
        default:
            *p_err = NET_IF_ERR_INVALID_ISR_TYPE;
             return;
    }


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                    NetIF_802x_RxPktValidateBuf()
*
* Description : Validate received buffer header as 802x protocol.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received a packet.
*               ---------   Argument validated in NetIF_Ether_Rx(),
*                                                 NetIF_WiFi_Rx().
*
*               p_ctrs_err  Pointer to an 802x network interface error counters.
*               ----------  Argument checked   in NetIF_802x_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Received buffer's IF header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT IF.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Rx(),
*               NetIF_WiFi_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIF_802x_RxPktValidateBuf (NET_BUF_HDR           *p_buf_hdr,
                                           NET_CTR_IF_802x_ERRS  *p_ctrs_err,
                                           NET_ERR               *p_err)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  valid;
    NET_ERR      err;


   (void)&p_ctrs_err;                                           /* Prevent possible 'variable unused' warning.          */


                                                                /* -------------- VALIDATE NET BUF TYPE --------------- */
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
             break;


        case NET_BUF_TYPE_NONE:
        case NET_BUF_TYPE_BUF:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

                                                                /* ------------- VALIDATE 802x IF BUF HDR ------------- */
    if (p_buf_hdr->ProtocolHdrType != NET_PROTOCOL_TYPE_IF) {
        if_nbr = p_buf_hdr->IF_Nbr;
        valid  = NetIF_IsValidHandler(if_nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvProtocolCtr);
            NET_CTR_ERR_INC(p_ctrs_err->RxInvProtocolCtr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->IF_HdrIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvBufIxCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_IF_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                    NetIF_802x_RxPktFrameDemux()
*
* Description : (1) Validate received packet frame & demultiplex to appropriate protocol layer :
*
*                   (a) Validate destination address :
*                       (1) Check for broadcast address                 See RFC #1122, Section 2.4
*                       (2) Check for multicast address                 See RFC #1112, Section 6.4
*                       (3) Check for this host's hardware address
*                   (b) Validate source address                         See 'NetIF_802x_IsValidAddrSrc()  Note #1'
*                   (c) Demultiplex & validate frame :
*                       (1) Ethernet   frame type
*                       (2) IEEE 802.3 frame length
*                   (d) Demultiplex packet to appropriate protocol layer :
*                       (1) IP  receive
*                       (2) ARP receive
*
*
* Argument(s) : p_if            Pointer to an 802x network interface that received a packet.
*               ----            Argument validated in NetIF_RxHandler().
*
*               p_buf           Pointer to a       network buffer    that received a packet.
*               -----           Argument checked   in NetIF_Ether_Rx(),
*                                                     NetIF_WiFi_Rx().
*
*               p_buf_hdr       Pointer to received packet frame's network buffer header.
*               ---------       Argument validated in NetIF_Ether_Rx(),
*                                                     NetIF_WiFi_Rx().
*
*               p_if_hdr        Pointer to received packet frame's header.
*               --------        Argument validated in NetIF_Ether_Rx(),
*                                                     NetIF_WiFi_Rx().
*
*               p_ctrs_stat     Pointer to an 802x network interface statistic counters.
*               -----------     Argument checked   in NetIF_802x_Rx().
*
*               p_ctrs_err      Pointer to an 802x network interface error     counters.
*               ----------      Argument checked   in NetIF_802x_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_INVALID_ADDR_DEST        Invalid destination address.
*                               NET_IF_ERR_INVALID_ADDR_SRC         Invalid source      address.
*                               NET_ERR_INVALID_PROTOCOL            Invalid network protocol.
*
*                                                                   -- RETURNED BY NetIF_802x_RxPktFrameDemuxEther() : --
*                               NET_IF_ERR_INVALID_ETHER_TYPE       Invalid Ethernet Frame Type value.
*
*                                                                   - RETURNED BY NetIF_802x_RxPktFrameDemuxIEEE802() : -
*                               NET_IF_ERR_INVALID_LEN_FRAME        Invalid IEEE 802.3 frame length.
*                               NET_IF_ERR_INVALID_LLC_DSAP         Invalid IEEE 802.2 LLC  DSAP    value.
*                               NET_IF_ERR_INVALID_LLC_SSAP         Invalid IEEE 802.2 LLC  SSAP    value.
*                               NET_IF_ERR_INVALID_LLC_CTRL         Invalid IEEE 802.2 LLC  Control value.
*                               NET_IF_ERR_INVALID_SNAP_CODE        Invalid IEEE 802.2 SNAP OUI     value.
*                               NET_IF_ERR_INVALID_SNAP_TYPE        Invalid IEEE 802.2 SNAP Type    value.
*
*                                                                   ------------- RETURNED BY NetARP_Rx() : -------------
*                               NET_ARP_ERR_NONE                    ARP message successfully demultiplexed.
*
*                                                                   ------------- RETURNED BY NetIPv4_Rx() : -------------
*                               NET_IPv4_ERR_NONE                   IPv4 datagram successfully demultiplexed.
*
*                                                                   ------------- RETURNED BY NetIPv6_Rx() : -------------
*                               NET_IPv6_ERR_NONE                   IPv6 datagram successfully demultiplexed.
*
*                               NET_ERR_RX                          Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Rx(),
*               NetIF_WiFi_Rx().
*
* Note(s)     : (2) When network buffer is demultiplexed to higher-layer protocol receive, the buffer's
*                   reference counter is NOT incremented since the network interface layer does NOT
*                   maintain a reference to the buffer.
*********************************************************************************************************
*/

static  void  NetIF_802x_RxPktFrameDemux (NET_IF                 *p_if,
                                          NET_BUF                *p_buf,
                                          NET_BUF_HDR            *p_buf_hdr,
                                          NET_IF_HDR_802x        *p_if_hdr,
                                          NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                                          NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                          NET_ERR                *p_err)
{
    NET_IF_DATA_802x  *p_if_data;
    CPU_BOOLEAN        valid;
    CPU_BOOLEAN        dest_this_host;
#ifdef  NET_MCAST_RX_MODULE_EN
    CPU_BOOLEAN        dest_multicast;
#endif
    CPU_BOOLEAN        dest_broadcast;
    CPU_INT16U         frame_type_len;


   (void)&p_ctrs_stat;                                          /* Prevent possible 'variable unused' warnings.         */


                                                                /* ---------------- VALIDATE DEST ADDR ---------------- */
    dest_broadcast = Mem_Cmp((void     *)&p_if_hdr->AddrDest[0],
                             (void     *)&NetIF_802x_AddrBroadcast[0],
                             (CPU_SIZE_T) NET_IF_802x_ADDR_SIZE);

#ifdef  NET_MCAST_RX_MODULE_EN
    dest_multicast = ((p_if_hdr->AddrDest[0] & NetIF_802x_AddrMulticastMask[0]) ==
                                               NetIF_802x_AddrMulticastMask[0]) ? DEF_YES : DEF_NO;
#endif

    if (dest_broadcast == DEF_YES) {
        NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.RxPktBcastCtr);
        NET_CTR_STAT_INC(p_ctrs_stat->RxPktBcastCtr);
        DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_BROADCAST);   /* Flag rx'd multicast pkt (see Note #1a1).         */

#ifdef  NET_MCAST_RX_MODULE_EN
    } else if (dest_multicast == DEF_YES) {
        NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.RxPktMcastCtr);
        NET_CTR_STAT_INC(p_ctrs_stat->RxPktMcastCtr);
        DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_MULTICAST);   /* Flag rx'd broadcast pkt (see Note #1a2).         */
#endif

    } else {
        p_if_data      = (NET_IF_DATA_802x  *) p_if->IF_Data;
        dest_this_host =  Mem_Cmp((void     *)&p_if_hdr->AddrDest[0],
                                  (void     *)&p_if_data->HW_Addr[0],
                                  (CPU_SIZE_T) NET_IF_802x_ADDR_SIZE);
        if (dest_this_host != DEF_YES) {                        /* Discard invalid dest addr (see Note #1a3).           */
            NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvAddrDestCtr);
            NET_CTR_ERR_INC(p_ctrs_err->RxInvAddrDestCtr);
           *p_err = NET_IF_ERR_INVALID_ADDR_DEST;
            return;
        }
    }


                                                                /* ---------------- VALIDATE SRC  ADDR ---------------- */
    valid = NetIF_802x_AddrHW_IsValid(p_if, &p_if_hdr->AddrSrc[0]);

    if (valid != DEF_YES) {                                     /* Discard invalid src addr (see Note #1b).             */
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvAddrSrcCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvAddrSrcCtr);
       *p_err = NET_IF_ERR_INVALID_ADDR_SRC;
        return;
    }

#ifdef  NET_DAD_MODULE_EN
                                                                /* --------------- DEMUX RX IF HW ADDR ---------------- */
    p_buf_hdr->IF_HW_AddrLen     =  NET_IF_802x_ADDR_SIZE;
    p_buf_hdr->IF_HW_AddrSrcPtr  = &p_if_hdr->AddrSrc[0];
    p_buf_hdr->IF_HW_AddrDestPtr = &p_if_hdr->AddrDest[0];
#endif


                                                                /* --------------- DEMUX/VALIDATE FRAME --------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&frame_type_len, &p_if_hdr->FrameType_Len);
    if (frame_type_len <= NET_IF_IEEE_802_FRAME_LEN_MAX) {
        NetIF_802x_RxPktFrameDemuxIEEE802(p_if,
                                          p_buf_hdr,
                                          p_if_hdr,
                                          p_ctrs_err,
                                          p_err);
    } else {
        NetIF_802x_RxPktFrameDemuxEther(p_if,
                                        p_buf_hdr,
                                        p_if_hdr,
                                        p_ctrs_err,
                                        p_err);
    }


                                                                /* -------------------- DEMUX PKT --------------------- */
    switch (*p_err) {                                           /* See Note #2.                                         */
        case NET_IF_ERR_NONE:
             switch (p_buf_hdr->ProtocolHdrType) {              /* Demux buf to appropriate protocol.                   */


#ifdef  NET_ARP_MODULE_EN
                 case NET_PROTOCOL_TYPE_ARP:
                      NetARP_Rx(p_buf, p_err);
                      break;
#endif

#ifdef  NET_IPv4_MODULE_EN
                 case NET_PROTOCOL_TYPE_IP_V4:
                      NetIPv4_Rx(p_buf, p_err);
                      break;
#endif


#ifdef  NET_IPv6_MODULE_EN
                case  NET_PROTOCOL_TYPE_IP_V6:
                      NetIPv6_Rx(p_buf, p_err);
                      break;
#endif

                 case NET_PROTOCOL_TYPE_NONE:
                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvProtocolCtr);
                      NET_CTR_ERR_INC(p_ctrs_err->RxInvProtocolCtr);
                      NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[p_if->Nbr].RxInvProtocolCtr);
                     *p_err = NET_ERR_INVALID_PROTOCOL;
                      return;
             }
             break;


        case NET_IF_ERR_INVALID_ETHER_TYPE:
        case NET_IF_ERR_INVALID_LEN_FRAME:
        case NET_IF_ERR_INVALID_LLC_DSAP:
        case NET_IF_ERR_INVALID_LLC_SSAP:
        case NET_IF_ERR_INVALID_LLC_CTRL:
        case NET_IF_ERR_INVALID_SNAP_CODE:
        case NET_IF_ERR_INVALID_SNAP_TYPE:
        default:
             return;
    }
}


/*
*********************************************************************************************************
*                                  NetIF_802x_RxPktFrameDemuxEther()
*
* Description : (1) Validate & demultiplex Ethernet packet frame :
*
*                   (a) Validate & demultiplex Ethernet packet frame
*                   (b) Update buffer controls
*
*
* Argument(s) : p_buf_hdr   Pointer to received packet frame's network buffer header.
*               ---------   Argument validated in NetIF_Ether_Rx(),
*                                                 NetIF_WiFi_Rx().
*
*               p_if_hdr    Pointer to received packet frame's header.
*               --------    Argument validated in NetIF_Ether_Rx(),
*                                                 NetIF_WiFi_Rx().
*
*               p_ctrs_err  Pointer to an 802x network interface error counters.
*               ----------  Argument checked   in NetIF_802x_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Valid   Ethernet frame packet .
*                               NET_IF_ERR_INVALID_ETHER_TYPE       Invalid Ethernet Frame Type value.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_RxPktFrameDemux().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_802x_RxPktFrameDemuxEther (NET_IF                *p_if,
                                               NET_BUF_HDR           *p_buf_hdr,
                                               NET_IF_HDR_802x       *p_if_hdr,
                                               NET_CTR_IF_802x_ERRS  *p_ctrs_err,
                                               NET_ERR               *p_err)
{
    NET_IF_HDR_ETHER  *p_if_hdr_ether;
    CPU_INT16U         frame_type;
    CPU_INT16U         ix;


   (void)&p_ctrs_err;                                           /* Prevent possible 'variable unused' warnings.         */
   (void)&p_if;

    p_if_hdr_ether = (NET_IF_HDR_ETHER *)p_if_hdr;

                                                                /* -------------- VALIDATE / DEMUX FRAME -------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&frame_type, &p_if_hdr_ether->FrameType);
    ix = p_buf_hdr->IF_HdrIx + NET_IF_HDR_SIZE_ETHER;
    switch (frame_type) {                                       /* Validate & demux Ether frame type.                   */


#ifdef  NET_IPv4_MODULE_EN
        case NET_IF_802x_FRAME_TYPE_IPv4:
             p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_IP_V4;
             p_buf_hdr->ProtocolHdrTypeNet    =  NET_PROTOCOL_TYPE_IP_V4;
             p_buf_hdr->IP_HdrIx              = (NET_BUF_SIZE)ix;
             break;
#endif

#ifdef  NET_IPv6_MODULE_EN
        case NET_IF_802x_FRAME_TYPE_IPv6:
             p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_IP_V6;
             p_buf_hdr->ProtocolHdrTypeNet    =  NET_PROTOCOL_TYPE_IP_V6;
             p_buf_hdr->IP_HdrIx            = (NET_BUF_SIZE)ix;
             break;
#endif

#ifdef  NET_ARP_MODULE_EN
        case NET_IF_802x_FRAME_TYPE_ARP:
             p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_ARP;
             p_buf_hdr->ProtocolHdrTypeIF_Sub =  NET_PROTOCOL_TYPE_ARP;
             p_buf_hdr->ARP_MsgIx             = (NET_BUF_SIZE)ix;
             break;
#endif

        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
             NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
            *p_err = NET_IF_ERR_INVALID_ETHER_TYPE;
             return;
    }

                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr->IF_HdrLen         =  NET_IF_HDR_SIZE_ETHER;
    p_buf_hdr->DataLen          -= (NET_BUF_SIZE)p_buf_hdr->IF_HdrLen;

    p_buf_hdr->ProtocolHdrTypeIF =  NET_PROTOCOL_TYPE_IF_ETHER;


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                 NetIF_802x_RxPktFrameDemuxIEEE802()
*
* Description : (1) Validate & demultiplex IEEE 802 packet frame :
*
*                   (a) Validate & demultiplex IEEE 802 packet frame
*                       (1) IEEE 802.2 LLC
*                       (2) IEEE 802.2 SNAP Organization Code
*                       (3) IEEE 802.2 SNAP Frame Type
*                   (b) Update buffer controls
*
*
* Argument(s) : p_buf_hdr   Pointer to received packet frame's network buffer header.
*               ---------   Argument validated in NetIF_Ether_Rx(),
*                                                 NetIF_WiFi_Rx().
*
*               p_if_hdr    Pointer to received packet frame's header.
*               --------    Argument validated in NetIF_Ether_Rx(),
*                                                 NetIF_WiFi_Rx().
*
*               p_ctrs_err  Pointer to an 802x network interface error counters.
*               ----------  Argument checked   in NetIF_802x_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Valid IEEE 802 packet frame.
*                               NET_IF_ERR_INVALID_LEN_FRAME        Invalid IEEE 802.3 frame length.
*                               NET_IF_ERR_INVALID_LLC_DSAP         Invalid IEEE 802.2 LLC  DSAP    value.
*                               NET_IF_ERR_INVALID_LLC_SSAP         Invalid IEEE 802.2 LLC  SSAP    value.
*                               NET_IF_ERR_INVALID_LLC_CTRL         Invalid IEEE 802.2 LLC  Control value.
*                               NET_IF_ERR_INVALID_SNAP_CODE        Invalid IEEE 802.2 SNAP OUI     value.
*                               NET_IF_ERR_INVALID_SNAP_TYPE        Invalid IEEE 802.2 SNAP Type    value.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_RxPktFrameDemux().
*
* Note(s)     : (2) The IEEE 802.3 Frame Length field specifies the number of frame data octets & does NOT
*                   include the trailing frame CRC field octets.  However, since some Ethernet devices MAY
*                   append the CRC field as part of a received packet frame, any validation of the minimum
*                   frame size MUST assume that the CRC field may be present.  Therefore, the minimum frame
*                   packet size for comparison MUST include the number of CRC field octets.
*********************************************************************************************************
*/

static  void  NetIF_802x_RxPktFrameDemuxIEEE802 (NET_IF                *p_if,
                                                 NET_BUF_HDR           *p_buf_hdr,
                                                 NET_IF_HDR_802x       *p_if_hdr,
                                                 NET_CTR_IF_802x_ERRS  *p_ctrs_err,
                                                 NET_ERR               *p_err)
{
    NET_IF_HDR_IEEE_802  *p_if_hdr_ieee_802;
    CPU_INT16U            frame_len;
    CPU_INT16U            frame_len_actual;
    CPU_INT16U            frame_type;
    CPU_INT16U            ix;


   (void)&p_ctrs_err;                                               /* Prevent possible 'variable unused' warnings.     */
   (void)&p_if;


    p_if_hdr_ieee_802 = (NET_IF_HDR_IEEE_802 *)p_if_hdr;

                                                                    /* ------------- VALIDATE FRAME SIZE -------------- */
    if (p_buf_hdr->TotLen >= NET_IF_802x_FRAME_MIN_CRC_SIZE) {      /* If pkt size >= min frame pkt size (see Note #2)  */
        NET_UTIL_VAL_COPY_GET_NET_16(&frame_len, &p_if_hdr_ieee_802->FrameLen);
        frame_len_actual = (CPU_INT16U)(p_buf_hdr->TotLen - NET_IF_HDR_SIZE_ETHER - NET_IF_802x_FRAME_CRC_SIZE);
        if (frame_len   != frame_len_actual) {                      /* ... & frame len != rem pkt len, rtn err.         */
            NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
            NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
           *p_err = NET_IF_ERR_INVALID_LEN_FRAME;
            return;
        }
    }

                                                                    /* ------------ VALIDATE IEEE 802.2 LLC ----------- */
    if (p_if_hdr_ieee_802->LLC_DSAP != NET_IF_IEEE_802_LLC_DSAP) {  /* Validate IEEE 802.2 LLC DSAP.                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
       *p_err = NET_IF_ERR_INVALID_LLC_DSAP;
        return;
    }

    if (p_if_hdr_ieee_802->LLC_SSAP != NET_IF_IEEE_802_LLC_SSAP) {  /* Validate IEEE 802.2 LLC SSAP.                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
       *p_err = NET_IF_ERR_INVALID_LLC_SSAP;
        return;
    }

    if (p_if_hdr_ieee_802->LLC_Ctrl != NET_IF_IEEE_802_LLC_CTRL) {  /* Validate IEEE 802.2 LLC Ctrl.                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
       *p_err = NET_IF_ERR_INVALID_LLC_CTRL;
        return;
    }
                                                                    /* ----------- VALIDATE IEEE 802.2 SNAP ----------- */
                                                                    /* Validate IEEE 802.2 SNAP OUI.                    */
    if (p_if_hdr_ieee_802->SNAP_OrgCode[0] != NET_IF_IEEE_802_SNAP_CODE_00) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
       *p_err = NET_IF_ERR_INVALID_SNAP_CODE;
        return;
    }
    if (p_if_hdr_ieee_802->SNAP_OrgCode[1] != NET_IF_IEEE_802_SNAP_CODE_01) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
       *p_err = NET_IF_ERR_INVALID_SNAP_CODE;
        return;
    }
    if (p_if_hdr_ieee_802->SNAP_OrgCode[2] != NET_IF_IEEE_802_SNAP_CODE_02) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
        NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
       *p_err = NET_IF_ERR_INVALID_SNAP_CODE;
        return;
    }


    NET_UTIL_VAL_COPY_GET_NET_16(&frame_type, &p_if_hdr_ieee_802->SNAP_FrameType);
    ix = p_buf_hdr->IF_HdrIx + NET_IF_HDR_SIZE_IEEE_802;
    switch (frame_type) {                                           /* Validate & demux IEEE 802.2 SNAP Frame Type.     */
#ifdef  NET_IPv4_MODULE_EN
        case NET_IF_IEEE_802_SNAP_TYPE_IPv4:
             p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_IP_V4;
             p_buf_hdr->ProtocolHdrTypeNet    =  NET_PROTOCOL_TYPE_IP_V4;
             p_buf_hdr->IP_HdrIx              = (NET_BUF_SIZE)ix;
             break;
#endif

#ifdef  NET_IPv6_MODULE_EN
        case NET_IF_IEEE_802_SNAP_TYPE_IPv6:
             p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_IP_V6;
             p_buf_hdr->ProtocolHdrTypeNet    =  NET_PROTOCOL_TYPE_IP_V6;
             p_buf_hdr->IP_HdrIx            = (NET_BUF_SIZE)ix;
             break;
#endif

#ifdef  NET_ARP_MODULE_EN
        case NET_IF_IEEE_802_SNAP_TYPE_ARP:
             p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_ARP;
             p_buf_hdr->ProtocolHdrTypeIF_Sub =  NET_PROTOCOL_TYPE_ARP;
             p_buf_hdr->ARP_MsgIx             = (NET_BUF_SIZE)ix;
             break;
#endif

        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.RxInvFrameCtr);
             NET_CTR_ERR_INC(p_ctrs_err->RxInvFrameCtr);
            *p_err = NET_IF_ERR_INVALID_SNAP_TYPE;
             return;
    }

                                                                    /* --------------- UPDATE BUF CTRLS --------------- */
    p_buf_hdr->IF_HdrLen         =  NET_IF_HDR_SIZE_IEEE_802;
    p_buf_hdr->DataLen          -= (NET_BUF_SIZE)p_buf_hdr->IF_HdrLen;

    p_buf_hdr->ProtocolHdrTypeIF =  NET_PROTOCOL_TYPE_IF_IEEE_802;


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetIF_802x_RxPktDiscard()
*
* Description : On any 802x receive error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_ctrs_err  Pointer to an 802x network interface error counters.
*               ----------  Argument checked in NetIF_802x_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_Rx().
*
* Note(s)     : (1) Network buffer freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIF_802x_RxPktDiscard (NET_BUF               *p_buf,
                                       NET_CTR_IF_802x_ERRS  *p_ctrs_err,
                                       NET_ERR               *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IFs.IFs_802x.RxPktDisCtr;
    NET_CTR_ERR_INC(p_ctrs_err->RxPktDisCtr);
#else
    p_ctr = (NET_CTR *) 0;
   (void)&p_ctrs_err;                                           /* Prevent possible 'variable unused' warnings.         */
#endif

   (void)NetBuf_FreeBuf(p_buf, p_ctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                     NetIF_802x_TxPktValidate()
*
* Description : (1) Validate 802x transmit packet parameters :
*
*                   (a) Validate the following transmit packet parameters :
*
*                       (1) Buffer type
*                       (2) Supported protocols :
*                           (A) ARP
*                           (B) IP
*
*                       (3) Buffer protocol index
*                       (4) Total Length
*
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIF_Ether_Tx(),
*                                                 NetIF_WiFi_Tx().
*
*               p_ctrs_err  Pointer to an 802x network interface error counters.
*               ----------  Argument checked   in NetIF_802x_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Transmit packet validated.
*                               NET_IF_ERR_INVALID_LEN_DATA     Invalid protocol/data length.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer   type.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Tx(),
*               NetIF_WiFi_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIF_802x_TxPktValidate (NET_BUF_HDR           *p_buf_hdr,
                                        NET_CTR_IF_802x_ERRS  *p_ctrs_err,
                                        NET_ERR               *p_err)
{
    CPU_INT16U  ix;
    CPU_INT16U  len;


   (void)&p_ctrs_err;                                           /* Prevent possible 'variable unused' warnings.         */


                                                                /* -------------- VALIDATE NET BUF TYPE --------------- */
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        case NET_BUF_TYPE_BUF:
        case NET_BUF_TYPE_RX_LARGE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }


                                                                /* ----------------- VALIDATE PROTOCOL ---------------- */
    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_IF_ETHER:
            *p_err = NET_IF_ERR_NONE;
             return;

#ifdef  NET_ARP_MODULE_EN
        case NET_PROTOCOL_TYPE_ARP:
             ix  = p_buf_hdr->ARP_MsgIx;
             len = p_buf_hdr->ARP_MsgLen;
             break;
#endif
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             ix  = p_buf_hdr->IP_HdrIx;
             len = p_buf_hdr->IP_TotLen;
             break;
#endif

        case NET_PROTOCOL_TYPE_IP_V6:
             ix  = p_buf_hdr->IP_HdrIx;
             len = p_buf_hdr->IP_TotLen;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.TxInvProtocolCtr);
             NET_CTR_ERR_INC(p_ctrs_err->TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.TxInvBufIxCtr);
        NET_CTR_ERR_INC(p_ctrs_err->TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }
    if (ix <  NET_IF_HDR_SIZE_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.TxInvBufIxCtr);
        NET_CTR_ERR_INC(p_ctrs_err->TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }


                                                                /* -------------- VALIDATE TOT DATA LEN --------------- */
    if (len != p_buf_hdr->TotLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.TxHdrDataLenCtr);
        NET_CTR_ERR_INC(p_ctrs_err->TxHdrDataLenCtr);
       *p_err = NET_IF_ERR_INVALID_LEN_DATA;
        return;
    }


   *p_err = NET_IF_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     NetIF_802x_TxPktPrepareFrame()
*
* Description : (1) Prepare data packet with 802x frame format :
*
*                   (a) Demultiplex Ethernet frame type
*                   (b) Update buffer controls
*                   (c) Write Ethernet values into packet frame
*                       (1) Ethernet destination broadcast address, if necessary
*                       (2) Ethernet source      MAC       address
*                       (3) Ethernet frame type
*                   (d) Clear Ethernet frame pad octets, if any
*
*
* Argument(s) : p_if            Pointer to an 802x network interface.
*               ----            Argument validated in NetIF_TxHandler().
*
*               p_buf           Pointer to network buffer with data packet to encapsulate.
*               -----           Argument checked   in NetIF_Ether_Tx(),
*                                                     NetIF_WiFi_Tx().
*
*               p_buf_hdr       Pointer to network buffer header.
*               ---------       Argument validated in NetIF_Ether_Tx(),
*                                                     NetIF_WiFi_Tx().
*
*               p_ctrs_stat     Pointer to an 802x network interface error counters.
*               ----------      Argument checked   in NetIF_802x_Tx().
*
*               p_ctrs_err      Pointer to an 802x network interface error counters.
*               ----------      Argument checked   in NetIF_802x_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_TX_RDY               Ethernet frame already encapsulated. ARP
*                                                                   cache resolved, frame ready for
*                                                                   transmission to the device.
*                               NET_IF_ERR_TX_BROADCAST         Ethernet frame successfully prepared for
*                                                                   Ethernet broadcast on local network.
*                               NET_IF_ERR_TX_MULTICAST         Ethernet frame successfully prepared for
*                                                                   Ethernet multicast on local network.
*                               NET_IF_ERR_TX_ADDR_REQ          Ethernet frame successfully prepared &
*                                                                   requires hardware address binding.
*
*                               NET_ERR_INVALID_PROTOCOL        Invalid network protocol.
*                               NET_BUF_ERR_INVALID_LEN         Insufficient buffer length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Tx(),
*               NetIF_WiFI_Tx().
*
* Note(s)     : (2) Supports ONLY Ethernet frame format for network transmit (see 'net_if_802x.c  Note #2a').
*
*               (3) Supports ONLY ARP & IP protocols (see 'net.h  Note #2a').
*
*               (4) To prepare the packet buffer for ARP resolution, the buffer's ARP protocol address
*                   pointer needs to be configured to the appropriate outbound address :
*
*                   (a) For ARP packets, the ARP layer will configure the ARP protocol address pointer
*                       (see 'net_arp.c  NetARP_TxPktPrepareHdr()  Note #1d').
*
*                   (b) For IP  packets, configure the ARP protocol address pointer to the IP's next-
*                       route address.
*
*               (5) RFC #894, Section 'Frame Format' states that :
*
*                   (a) "The minimum length of the data field of a packet sent over an Ethernet is 46
*                        octets."
*
*                   (b) (1) "If necessary, the data field should be padded (with octets of zero) to
*                            meet the Ethernet minimum frame size."
*                       (2) "This padding is not part of the IP packet and is not included in the
*                            total length field of the IP header."
*********************************************************************************************************
*/

static  void  NetIF_802x_TxPktPrepareFrame (NET_IF                 *p_if,
                                            NET_BUF                *p_buf,
                                            NET_BUF_HDR            *p_buf_hdr,
                                            NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                                            NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                            NET_ERR                *p_err)
{
#ifdef  NET_MCAST_TX_MODULE_EN
    CPU_BOOLEAN        tx_multicast;
#endif
    NET_IF_DATA_802x  *p_if_data;
    NET_IF_HDR_ETHER  *p_if_hdr_ether;
    CPU_INT16U         protocol_ix;
    CPU_INT16U         frame_type;
    CPU_INT16U         clr_ix;
    CPU_INT16U         clr_len;
    CPU_INT16U         clr_size;
    CPU_BOOLEAN        clr_buf_mem;
    CPU_BOOLEAN        tx_broadcast;


   (void)&p_ctrs_stat;                                          /* Prevent possible 'variable unused' warnings.         */
   (void)&p_ctrs_err;


                                                                /* ----------------- DEMUX FRAME TYPE ----------------- */
    switch (p_buf_hdr->ProtocolHdrType) {                       /* Demux protocol for frame type (see Note #3).         */
        case NET_PROTOCOL_TYPE_IF_ETHER:
            *p_err       = NET_IF_ERR_TX_RDY;
             return;

#ifdef  NET_ARP_MODULE_EN
        case NET_PROTOCOL_TYPE_ARP:
             protocol_ix = p_buf_hdr->ARP_MsgIx;
             frame_type  = NET_IF_802x_FRAME_TYPE_ARP;
             break;
#endif

#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             protocol_ix = p_buf_hdr->IP_HdrIx;
             frame_type  = NET_IF_802x_FRAME_TYPE_IPv4;
#ifdef  NET_ARP_MODULE_EN
                                                                /* Cfg ARP addr ptr (see Note #4b).                     */
             p_buf_hdr->ARP_AddrProtocolPtr = (CPU_INT08U *)&p_buf_hdr->IP_AddrNextRouteNetOrder;
#endif
             break;
#endif


#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V6:
             protocol_ix = p_buf_hdr->IP_HdrIx;
             frame_type  = NET_IF_802x_FRAME_TYPE_IPv6;
                                                                /* Cfg NDP addr ptr (see Note #4b).                     */
#ifdef  NET_ARP_MODULE_EN
             p_buf_hdr->ARP_AddrProtocolPtr = (CPU_INT08U *)&p_buf_hdr->IPv6_AddrNextRoute;
#endif
#ifdef  NET_NDP_MODULE_EN
             p_buf_hdr->NDP_AddrProtocolPtr = (CPU_INT08U *)&p_buf_hdr->IPv6_AddrNextRoute;
#endif
             break;
#endif

        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IFs_802x.TxInvProtocolCtr);
             NET_CTR_ERR_INC(p_ctrs_err->TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }


                                                                            /* ----------- UPDATE BUF CTRLS ----------- */
    p_buf_hdr->IF_HdrLen         =  NET_IF_HDR_SIZE_ETHER;
    p_buf_hdr->IF_HdrIx          =  protocol_ix - p_buf_hdr->IF_HdrLen;
    p_buf_hdr->TotLen           += (NET_BUF_SIZE) p_buf_hdr->IF_HdrLen;

    p_buf_hdr->ProtocolHdrType   =  NET_PROTOCOL_TYPE_IF_ETHER;
    p_buf_hdr->ProtocolHdrTypeIF =  NET_PROTOCOL_TYPE_IF_ETHER;


                                                                            /* ---------- PREPARE 802x FRAME ---------- */
    p_if_hdr_ether = (NET_IF_HDR_ETHER *)&p_buf->DataPtr[p_buf_hdr->IF_HdrIx];

                                                                            /* --------- PREPARE FRAME ADDRS ---------- */
    tx_broadcast   =  DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_BROADCAST);
#ifdef  NET_MCAST_TX_MODULE_EN
    tx_multicast   =  DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_MULTICAST);
#endif

    if (tx_broadcast == DEF_YES) {                                          /* If dest addr broadcast,      ...         */
        NET_UTIL_VAL_COPY(&p_if_hdr_ether->AddrDest[0],                     /* ... wr broadcast addr into frame.        */
                          &NetIF_802x_AddrBroadcast[0],
                           NET_IF_802x_ADDR_SIZE);
        NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.TxPktBcastCtr);
        NET_CTR_STAT_INC(p_ctrs_stat->TxPktBcastCtr);
       *p_err = NET_IF_ERR_TX_BROADCAST;

#ifdef  NET_MCAST_TX_MODULE_EN
    } else if (tx_multicast == DEF_YES) {
                                                                /* If dest addr multicast, ...                          */
        if (frame_type != NET_IF_802x_FRAME_TYPE_IPv6) {
            NET_CTR_STAT_INC(Net_StatCtrs.IFs.IFs_802xCtrs.TxPktMcastCtr);
            NET_CTR_STAT_INC(p_ctrs_stat->TxPktMcastCtr);

#ifdef  NET_ARP_MODULE_EN
            p_buf_hdr->ARP_AddrHW_Ptr = &p_if_hdr_ether->AddrDest[0];           /* ...  req hw addr binding.                */
#endif

        } else {


#ifdef  NET_IPv6_MODULE_EN
            NetIPv6_AddrHW_McastSet(&p_if_hdr_ether->AddrDest[0], &p_buf_hdr->IPv6_AddrDest, p_err);
            if (*p_err != NET_IPv6_ERR_NONE) {
                 return;
            }
#ifdef  NET_NDP_MODULE_EN
            p_buf_hdr->NDP_AddrHW_Ptr = &p_if_hdr_ether->AddrDest[0];
#endif

#endif
        }

       *p_err = NET_IF_ERR_TX_MULTICAST;
#endif

    } else {                                                                /* Else req hw addr binding.                */
#ifdef  NET_ARP_MODULE_EN
        p_buf_hdr->ARP_AddrHW_Ptr = &p_if_hdr_ether->AddrDest[0];
#endif
#ifdef  NET_NDP_MODULE_EN
        p_buf_hdr->NDP_AddrHW_Ptr = &p_if_hdr_ether->AddrDest[0];
#endif
       *p_err =  NET_IF_ERR_TX_ADDR_REQ;
    }

    p_if_data = (NET_IF_DATA_802x *)p_if->IF_Data;
    NET_UTIL_VAL_COPY(&p_if_hdr_ether->AddrSrc[0],                          /* Wr src addr into frame.                  */
                      &p_if_data->HW_Addr[0],
                       NET_IF_802x_ADDR_SIZE);

                                                                            /* ---------- PREPARE FRAME TYPE ---------- */
    NET_UTIL_VAL_COPY_SET_NET_16(&p_if_hdr_ether->FrameType, &frame_type);

                                                                            /* --------- CLR/PAD FRAME OCTETS --------- */
    if (p_buf_hdr->TotLen < NET_IF_802x_FRAME_MIN_SIZE) {                   /* If tot len < min frame len (see Note #5a)*/
        clr_buf_mem = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_CLR_MEM);
        if (clr_buf_mem != DEF_YES) {                                       /* ... & buf mem NOT clr,               ... */
            clr_ix   = p_buf_hdr->IF_HdrIx        + (CPU_INT16U)p_buf_hdr->TotLen;
            clr_len  = NET_IF_802x_FRAME_MIN_SIZE - (CPU_INT16U)p_buf_hdr->TotLen;
            clr_size = clr_ix + clr_len;
            if (clr_size > p_buf_hdr->Size) {
               *p_err = NET_BUF_ERR_INVALID_LEN;
                return;
            }
            Mem_Clr((void     *)&p_buf->DataPtr[clr_ix],                    /* ... clr rem'ing octets (see Note #5b1).  */
                    (CPU_SIZE_T) clr_len);
        }
        p_buf_hdr->TotLen = NET_IF_802x_FRAME_MIN_SIZE;                     /* Update tot frame len.                    */
    }
}


/*
*********************************************************************************************************
*                                      NetIF_802x_TxPktDiscard()
*
* Description : On any 802x transmit error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_ctrs_err  Pointer to an 802x network interface error counters.
*               ----------  Argument checked in NetIF_802x_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_802x_TxPktDiscard (NET_BUF               *p_buf,
                                       NET_CTR_IF_802x_ERRS  *p_ctrs_err,
                                       NET_ERR               *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IFs.IFs_802x.TxPktDisCtr;
    NET_CTR_ERR_INC(p_ctrs_err->TxPktDisCtr);
#else
    p_ctr = (NET_CTR *) 0;
   (void)&p_ctrs_err;                                           /* Prevent possible 'variable unused' warnings.         */
#endif

   (void)NetBuf_FreeBuf(p_buf, p_ctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                       NetIF_802x_TxIxDataGet()
*
* Description : Get the offset of a buffer at which the IPv4 data can be written.
*
* Argument(s) : p_ix    Pointer to the current protocol index.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IF_ERR_NONE
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_BufPoolCfgValidate().
*
* Note(s)     : none.
*********************************************************************************************************
*/
static  void  NetIF_802x_TxIxDataGet (CPU_INT16U  *p_ix,
                                      NET_ERR     *p_err)
{
    *p_ix  += NET_IF_HDR_SIZE_ETHER_MIN;
    *p_err  = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_IF_802x_MODULE_EN */

