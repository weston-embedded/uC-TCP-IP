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
*                                         NETWORK ICMP V4 LAYER
*                                 (INTERNET CONTROL MESSAGE PROTOCOL)
*
* Filename : net_icmpv4.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Internet Control Message Protocol as described in RFC #792 with the following
*                restrictions/constraints :
*
*                (a) Does NOT support IP forwarding/routing               RFC #1122, Section 3.3.1
*
*                (b) Does NOT support ICMP Address Mask Agent/Server      RFC #1122, Section 3.2.2.9
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
#define    NET_ICMPv4_MODULE

#include  "../../Source/net_cfg_net.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'net_icmpv4.h  MODULE'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_ICMPv4_MODULE_EN


#include  "net_icmpv4.h"
#include  "net_ipv4.h"
#include  "../../IF/net_if.h"
#include  "../../Source/net_icmp.h"
#include  "../../Source/net_stat.h"
#include  "../../Source/net_util.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       ICMPv4 MESSAGE DEFINES
*********************************************************************************************************
*/


#define  NET_ICMPv4_HDR_SIZE_TS                           20
#define  NET_ICMPv4_HDR_SIZE_ADDR_MASK                    12


#define  NET_ICMPv4_HDR_NBR_OCTETS_UNUSED                  4
#define  NET_ICMPv4_HDR_NBR_OCTETS_UNUSED_PARAM_PROB       3


#define  NET_ICMPv4_MSG_ERR_HDR_SIZE_MIN               NET_IPv4_HDR_SIZE_MIN
#define  NET_ICMPv4_MSG_ERR_HDR_SIZE_MAX               NET_IPv4_HDR_SIZE_MAX

#define  NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_BITS            64                    /* See RFC #1122, Section 3.2.2.        */
#define  NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_OCTETS      (((NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_BITS - 1) / DEF_OCTET_NBR_BITS) + 1)

#define  NET_ICMPv4_MSG_ERR_LEN_MIN                     (NET_ICMPv4_MSG_ERR_HDR_SIZE_MIN  + NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_OCTETS)
#define  NET_ICMPv4_MSG_ERR_LEN_MAX                     (NET_ICMPv4_MSG_ERR_HDR_SIZE_MAX  + NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_OCTETS)


#define  NET_ICMPv4_MSG_LEN_MIN_DFLT                     NET_ICMPv4_HDR_SIZE_DFLT

#define  NET_ICMPv4_MSG_LEN_MIN_DEST_UNREACH            (NET_ICMPv4_HDR_SIZE_DEST_UNREACH + NET_ICMPv4_MSG_ERR_LEN_MIN)
#define  NET_ICMPv4_MSG_LEN_MIN_TIME_EXCEED             (NET_ICMPv4_HDR_SIZE_TIME_EXCEED  + NET_ICMPv4_MSG_ERR_LEN_MIN)
#define  NET_ICMPv4_MSG_LEN_MIN_PARAM_PROB              (NET_ICMPv4_HDR_SIZE_PARAM_PROB   + NET_ICMPv4_MSG_ERR_LEN_MIN)
#define  NET_ICMPv4_MSG_LEN_MIN_ECHO                     NET_ICMPv4_HDR_SIZE_ECHO
#define  NET_ICMPv4_MSG_LEN_MIN_TS                       NET_ICMPv4_HDR_SIZE_TS
#define  NET_ICMPv4_MSG_LEN_MIN_ADDR_MASK                NET_ICMPv4_HDR_SIZE_ADDR_MASK


#define  NET_ICMPv4_MSG_LEN_MAX_NONE                     DEF_INT_16U_MAX_VAL

#define  NET_ICMPv4_MSG_LEN_MAX_DEST_UNREACH             NET_ICMPv4_MSG_LEN_MAX_NONE
#define  NET_ICMPv4_MSG_LEN_MAX_TIME_EXCEED              NET_ICMPv4_MSG_LEN_MAX_NONE
#define  NET_ICMPv4_MSG_LEN_MAX_PARAM_PROB               NET_ICMPv4_MSG_LEN_MAX_NONE
#define  NET_ICMPv4_MSG_LEN_MAX_ECHO                     NET_ICMPv4_MSG_LEN_MAX_NONE
#define  NET_ICMPv4_MSG_LEN_MAX_TS                       NET_ICMPv4_HDR_SIZE_TS
#define  NET_ICMPv4_MSG_LEN_MAX_ADDR_MASK                NET_ICMPv4_HDR_SIZE_ADDR_MASK

#define  NET_ICMPv4_MSG_PTR_MIN_PARAM_PROB               NET_ICMPv4_MSG_LEN_MIN_DFLT


/*
*********************************************************************************************************
*                                       ICMPv4 POINTER DEFINES
*
* Note(s) : (1) RFC #791 & RFC #792 define a pointer (PTR) as an index (IX) into an option or message :
*
*               (a) RFC #791, Section 3.1 'Options : Loose/Strict Source & Record Route'
*               (b) RFC #791, Section 3.1 'Options : Internet Timestamp'
*               (c) RFC #792,             'Parameter Problem Message'
*
*           (2) ICMPv4 Parameter Problem Message pointer validation currently ONLY supports the following
*               protocols :
*
*               (a) IP
*
*           (3) The following pointer/indices abbreviated to enforce ANSI-compliance of 31-character
*               symbol length uniqueness :
*
*               (a) 'NET_ICMPv4_PTR_IX_IP_HDR'   abbreviated to 'NET_ICMPv4_PTR_IX_IP'
*               (b) 'NET_ICMPv4_PTR_IX_ICMP_MSG' abbreviated to 'NET_ICMPv4_PTR_IX_ICMP'
*********************************************************************************************************
*/
                                                                                /* -------- ICMPv4 MSG PTR IXs -------- */
#define  NET_ICMPv4_PTR_IX_ICMP_BASE                       0

#define  NET_ICMPv4_PTR_IX_ICMP_TYPE                       0
#define  NET_ICMPv4_PTR_IX_ICMP_CODE                       1
#define  NET_ICMPv4_PTR_IX_ICMP_CHK_SUM                    2

#define  NET_ICMPv4_PTR_IX_ICMP_PTR                        4
#define  NET_ICMPv4_PTR_IX_ICMP_UNUSED                     4
#define  NET_ICMPv4_PTR_IX_ICMP_UNUSED_PARAM_PROB          5


/*
*********************************************************************************************************
*                                         ICMPv4 FLAG DEFINES
*********************************************************************************************************
*/

                                                                /* ----------------- NET ICMPv4 FLAGS ----------------- */
#define  NET_ICMPv4_FLAG_NONE                     DEF_BIT_NONE
#define  NET_ICMPv4_FLAG_USED                     DEF_BIT_00    /* ICMPv4 entry cur used; i.e. NOT in free pool.        */





/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       ICMPv4 FLAGS DATA TYPE
*********************************************************************************************************
*/

typedef  NET_FLAGS   NET_ICMPv4_FLAGS;


/*
*********************************************************************************************************
*                                            ICMPv4 HEADER
*
* Note(s) : (1) See RFC #792 for ICMP message header formats.
*
*           (2) ICMP 'Redirect' & 'Router' messages are NOT supported (see 'net_icmp.h  Note #1').
*********************************************************************************************************
*/

                                                                        /* -------------- NET ICMPv4 HDR -------------- */
typedef  struct  net_ICMPv4_hdr {
    CPU_INT08U      Type;                                               /* ICMPv4 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv4 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv4 msg chk sum.                          */
} NET_ICMPv4_HDR;


/*
*********************************************************************************************************
*                                         ICMPv4 ERROR HEADER
*
* Note(s) : (1) See RFC #792, Sections 'Destination Unreachable Message', 'Source Quench Message', 'Time
*               Exceeded Message' for ICMP 'Error Message' header format.
*
*           (2) 'Unused' field MUST be cleared (i.e. ALL 'Unused' field octets MUST be set to 0x00).
*********************************************************************************************************
*/

                                                                        /* ------------ NET ICMPv4 ERR HDR ------------ */
typedef  struct  net_ICMPv4_hdr_err {
    CPU_INT08U      Type;                                               /* ICMPv4 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv4 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv4 msg chk sum.                          */

    CPU_INT08U      Unused[NET_ICMPv4_HDR_NBR_OCTETS_UNUSED];           /* See Note #2.                                 */

    CPU_INT08U      Data[NET_ICMPv4_MSG_ERR_LEN_MAX];
} NET_ICMPv4_HDR_ERR;


/*
*********************************************************************************************************
*                                   ICMPv4 PARAMETER PROBLEM HEADER
*
* Note(s) : (1) See RFC #792, Section 'Parameter Problem Message' for ICMP 'Parameter Problem Message'
*               header format.
*
*           (2) 'Unused' field MUST be cleared (i.e. ALL 'Unused' field octets MUST be set to 0x00).
*********************************************************************************************************
*/

                                                                        /* --------- NET ICMPv4 PARAM PROB HDR -------- */
typedef  struct  net_ICMPv4_hdr_param_prob {
    CPU_INT08U      Type;                                               /* ICMPv4 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv4 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv4 msg chk sum.                          */

    CPU_INT08U      Ptr;                                                /* Ptr into ICMPv4 err msg.                     */
    CPU_INT08U      Unused[NET_ICMPv4_HDR_NBR_OCTETS_UNUSED_PARAM_PROB];/* See Note #2.                                 */

    CPU_INT08U      Data[NET_ICMPv4_MSG_ERR_LEN_MAX];
} NET_ICMPv4_HDR_PARAM_PROB;


/*
*********************************************************************************************************
*                                 ICMPv4 ECHO REQUEST/REPLY HEADER
*
* Note(s) : (1) See RFC #792, Section 'Echo or Echo Reply Message' for ICMP 'Echo Request/Reply Message'
*               header format.
*
*           (2) 'Data' declared with 1 entry; prevents removal by compiler optimization.
*********************************************************************************************************
*/

                                                                        /* ------ NET ICMPv4 ECHO REQ/REPLY HDR ------- */
typedef  struct  net_ICMPv4_hdr_echo {
    CPU_INT08U      Type;                                               /* ICMPv4 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv4 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv4 msg chk sum.                          */

    CPU_INT16U      ID;                                                 /* ICMPv4 msg ID.                               */
    CPU_INT16U      SeqNbr;                                             /* ICMPv4 seq nbr.                              */

    CPU_INT08U      Data[1];                                            /* ICMPv4 msg data (see Note #2).               */
} NET_ICMPv4_HDR_ECHO;


/*
*********************************************************************************************************
*                                  ICMPv4 INTERNET TIMESTAMP HEADER
*
* Note(s) : (1) See RFC #792, Section 'Timestamp or Timestamp Reply Message' for ICMP 'Internet Timestamp
*               Message' header format.
*********************************************************************************************************
*/

                                                                        /* ------- NET ICMPv4 TS REQ/REPLY HDR -------- */
typedef  struct  net_ICMPv4_hdr_ts {
    CPU_INT08U      Type;                                               /* ICMPv4 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv4 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv4 msg chk sum.                          */

    CPU_INT16U      ID;                                                 /* ICMPv4 msg ID.                               */
    CPU_INT16U      SeqNbr;                                             /* ICMPv4 seq nbr.                              */

    NET_TS          TS_Originate;                                       /* TS @ req tx.                                 */
    NET_TS          TS_Rx;                                              /* TS @ target rx.                              */
    NET_TS          TS_Tx;                                              /* TS @ target tx.                              */
} NET_ICMPv4_HDR_TS;


/*
*********************************************************************************************************
*                              ICMPv4 ADDRESS MASK REQUEST/REPLY HEADER
*
* Note(s) : (1) See RFC #950, Appendix I 'Address Mask ICMP' for ICMP 'Address Mask Request/Reply Message'
*               header format.
*********************************************************************************************************
*/

                                                                        /* ---- NET ICMPv4 ADDR MASK REQ/REPLY HDR ---- */
typedef  struct  net_ICMPv4_hdr_addr_mask {
    CPU_INT08U      Type;                                               /* ICMPv4 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv4 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv4 msg chk sum.                          */

    CPU_INT16U      ID;                                                 /* ICMPv4 msg ID.                               */
    CPU_INT16U      SeqNbr;                                             /* ICMPv4 seq nbr.                              */

    NET_IPv4_ADDR   AddrMask;                                           /* Addr mask.                                   */
} NET_ICMPv4_HDR_ADDR_MASK;


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
*                                               MACRO'S
*********************************************************************************************************
*********************************************************************************************************
*/


static    CPU_INT16U               NetICMPv4_TxSeqNbrCtr;         /* Global tx seq nbr field ctr.                       */


/*
*********************************************************************************************************
*                                     NET_ICMPv4_TX_GET_SEQ_NBR()
*
* Description : Get next ICMPv4 transmit message sequence number.
*
* Argument(s) : seq_nbr     Variable that will receive the returned ICMPv4 transmit message sequence number.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgReq().
*
*               This macro is an INTERNAL network protocol suite macro & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Return ICMP sequence number is NOT converted from host-order to network-order.
*********************************************************************************************************
*/

#define  NET_ICMPv4_TX_GET_SEQ_NBR(seq_nbr)     do { NET_UTIL_VAL_COPY_16(&(seq_nbr), &NetICMPv4_TxSeqNbrCtr); \
                                                     NetICMPv4_TxSeqNbrCtr++;                                } while (0)



/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                    /* ------------------- RX FNCTS ------------------- */
static  void                       NetICMPv4_RxReplyDemux             (NET_BUF                 *p_buf,
                                                                       NET_BUF_HDR             *p_buf_hdr,
                                                                       NET_ICMPv4_HDR          *p_icmp_hdr,
                                                                       NET_ERR                 *p_err);

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void                       NetICMPv4_RxPktValidateBuf         (NET_BUF_HDR             *p_buf_hdr,
                                                                       NET_ERR                 *p_err);
#endif /* NET_ERR_CFG_ARG_CHK_DBG_EN */

static  void                       NetICMPv4_RxPktValidate            (NET_BUF                 *p_buf,
                                                                       NET_BUF_HDR             *p_buf_hdr,
                                                                       NET_ICMPv4_HDR          *p_icmp_hdr,
                                                                       NET_ERR                 *p_err);

static  void                       NetICMPv4_RxPktFree                (NET_BUF                 *p_buf);

static  void                       NetICMPv4_RxPktDiscard             (NET_BUF                 *p_buf,
                                                                       NET_ERR                 *p_err);


                                                                    /* ------------------- TX FNCTS ------------------- */

static  NET_ICMPv4_REQ_ID_SEQ      NetICMPv4_TxMsgReqHandler          (CPU_INT08U               type,
                                                                       CPU_INT08U               code,
                                                                       CPU_INT16U               id,
                                                                       NET_IPv4_ADDR            addr_src,
                                                                       NET_IPv4_ADDR            addr_dest,
                                                                       NET_IPv4_TOS             TOS,
                                                                       NET_IPv4_TTL             TTL,
                                                                       NET_IPv4_FLAGS           flags,
                                                                       void                    *p_opts,
                                                                       void                    *p_data,
                                                                       CPU_INT16U               data_len,
                                                                       NET_ERR                 *p_err);

static  void                       NetICMPv4_TxMsgErrValidate         (NET_BUF                 *p_buf,
                                                                       NET_BUF_HDR             *p_buf_hdr,
                                                                       NET_IPv4_HDR            *p_ip_hdr,
                                                                       CPU_INT08U               type,
                                                                       CPU_INT08U               code,
                                                                       CPU_INT08U               ptr,
                                                                       NET_ERR                 *p_err);


static  void                       NetICMPv4_TxMsgReqValidate         (CPU_INT08U               type,
                                                                       CPU_INT08U               code,
                                                                       NET_ERR                 *p_err);


static  void                       NetICMPv4_TxReqReply               (NET_BUF                 *p_buf,
                                                                       NET_BUF_HDR             *p_buf_hdr,
                                                                       NET_ICMPv4_HDR          *p_icmp_hdr,
                                                                       NET_ERR                 *p_err);

static  void                       NetICMPv4_TxIxDataGet              (NET_IF_NBR                 if_nbr,
                                                                       CPU_INT32U                 data_len,
                                                                       CPU_INT16U                *p_ix,
                                                                       NET_ERR                   *p_err);

static  void                       NetICMPv4_TxPktFree                (NET_BUF                 *p_buf);

static  void                       NetICMPv4_TxPktDiscard             (NET_BUF                 *p_buf,
                                                                       NET_ERR                 *p_err);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                              FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          NetICMPv4_Init()
*
* Description : (1) Initialize Internet Control Message Protocol Layer V6 :
*
*                   (a) Initialize ICMP transmit sequence number counter
*
*
* Argument(s) : p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_ICMPv4_ERR_NONE     ICMPv4 module successfully initialized.
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

void  NetICMPv4_Init (NET_ERR  *p_err)
{
                                                                    /* ----------- INIT ICMPv4 TX SEQ NBR CTR ----------- */
    NetICMPv4_TxSeqNbrCtr = 0u;

   *p_err = NET_ICMPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetICMPv4_Rx()
*
* Description : (1) Process received messages :
*
*                   (a) Validate ICMPv4 packet
*                   (b) Demultiplex ICMPv4 message
*                   (c) Free ICMPv4 packet
*                   (d) Update receive statistics
*
*               (2) Although ICMPv4 data units are typically referred to as 'messages' (see RFC #792, Section
*                   'Introduction'), the term 'ICMP packet' (see RFC #1983, 'packet') is used for ICMP
*                   Receive until the packet is validated as an ICMPv4 message.
*
*
* Argument(s) : p_buf   Pointer to network buffer that received ICMPv4 packet.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_ICMPv4_ERR_NONE     Message successfully received and processed.
*
*                           -RETURNED BY NetICMPv4_RxPktValidate()-
*                           See NetICMPv4_RxPktValidate() for additional return error codes.
*
*                           -RETURNED BY NetICMPv4_TxReqReply()-
*                           See NetICMPv4_TxReqReply() for additional return error codes.
*
*                           -RETURNED BY NetICMPv4_RxPktDiscard()-
*                           See NetICMPv4_RxPktDiscard() for additional return error codes.
*
*                           -RETURNED BY NetICMPv4_RxPktValidateBuf()-
*                           See NetICMPv4_RxPktValidateBuf() for additional return error codes.
*
*                           -RETURNED BY NetICMPv4_RxReplyDemux()-
*                           See NetICMPv4_RxReplyDemux() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktDemuxDatagram().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (3) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

void  NetICMPv4_Rx (NET_BUF  *p_buf,
                    NET_ERR  *p_err)
{
    NET_BUF_HDR     *p_buf_hdr;
    NET_ICMPv4_HDR  *p_icmp_hdr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetICMPv4_RxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.NullPtrCtr);
        return;
    }
#endif


    NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.RxMsgCtr);


                                                                /* ------------- VALIDATE RX'D ICMPv4 PKT ------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetICMPv4_RxPktValidateBuf(p_buf_hdr, p_err);               /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_ICMPv4_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetICMPv4_RxPktDiscard(p_buf, p_err);
             return;
    }
#endif
    p_icmp_hdr = (NET_ICMPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->ICMP_MsgIx];
    NetICMPv4_RxPktValidate(p_buf,                              /* Validate rx'd pkt.                                   */
                            p_buf_hdr,
                            p_icmp_hdr,
                            p_err);


                                                                /* ------------------ DEMUX ICMP MSG ------------------ */
    switch (*p_err) {
        case NET_ICMPv4_ERR_MSG_TYPE_REQ:
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.RxMsgReqCtr);
             NetICMPv4_TxReqReply(p_buf, p_buf_hdr, p_icmp_hdr, p_err);
             break;


        case NET_ICMPv4_ERR_MSG_TYPE_REPLY:
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.RxMsgReplyCtr);
             NetICMPv4_RxReplyDemux(p_buf, p_buf_hdr, p_icmp_hdr, p_err);
             break;


        case NET_ICMPv4_ERR_MSG_TYPE_ERR:
                                                                /* See Note #3a.                                        */
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.RxMsgErrCtr);
             break;

        case NET_ICMPv4_ERR_RX_BROADCAST:
        case NET_ICMPv4_ERR_RX_MCAST:
        case NET_ICMPv4_ERR_INVALID_TYPE:
        case NET_ICMPv4_ERR_INVALID_CODE:
        case NET_ICMPv4_ERR_INVALID_PTR:
        case NET_ICMPv4_ERR_INVALID_LEN:
        case NET_ICMPv4_ERR_INVALID_LEN_DATA:
        case NET_ICMPv4_ERR_INVALID_CHK_SUM:
        default:
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.RxMsgUnknownCtr);
             NetICMPv4_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* -------- FREE ICMPv4 PKT / UPDATE RX STATS --------- */
    switch (*p_err) {                                           /* Chk err from NetICMPv4_TxMsg&&&().                   */
        case NET_ICMPv4_ERR_NONE:
             NetICMPv4_RxPktFree(p_buf);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.RxMsgCompCtr);
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
             NetICMPv4_RxPktFree(p_buf);
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxPktDiscardedCtr);
            *p_err = NET_ERR_RX;
             return;


        case NET_ICMPv4_ERR_RX_REPLY:
        default:
             NetICMPv4_RxPktDiscard(p_buf, p_err);
             return;
    }

   *p_err = NET_ICMPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetICMPv4_TxEchoReq()
*
* Description : Transmit ICMPv4 echo request message.
*
* Argument(s) : p_addr_dest     Pointer to IPv4 destination address to send the ICMP echo request.
*
*               id              ID that will be send as the ICMP echo request ID.
*
*               p_data          Pointer to the data buffer to include in the ICMP echo request.
*
*               data_len        Number of data buffer octets to include in the ICMP echo request.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_ICMPv4_ERR_NONE     ICMPv4 Echo request successfully sent.
*
*                                   ------------- RETURNED BY NetICMPv4_TxMsgReq() -------------
*                                   See NetICMPv4_TxMsgReq() for additional return error codes.
*
* Return(s)   : ICMPv4 echo request sequence number, if NO error(s).
*
*               NET_ICMPv4_REQ_SEQ_NONE, otherwise.
*
* Caller(s)   : NetICMP_TxEchoReq().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U   NetICMPv4_TxEchoReq (NET_IPv4_ADDR  *p_addr_dest,
                                  CPU_INT16U      id,
                                  void           *p_data,
                                  CPU_INT16U      data_len,
                                  NET_ERR        *p_err)
{
    NET_IPv4_ADDR           addr_dest;
    NET_IPv4_ADDR           addr_src;
    NET_ICMPv4_REQ_ID_SEQ   id_seq;


    id_seq.ID     = id;
    id_seq.SeqNbr = NET_ICMPv4_REQ_SEQ_NONE;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_dest == (NET_IPv4_ADDR *)0) {                    /* ---------------- VALIDATE ADDR PTR ----------------- */
       *p_err = NET_ICMPv4_ERR_INVALID_PTR;
        return (NET_ICMPv4_REQ_SEQ_NONE);
    }

    if ((data_len >  0u)       &&
        (p_data   == DEF_NULL)) {
        *p_err = NET_ICMPv4_ERR_INVALID_PTR;
         return (NET_ICMPv4_REQ_SEQ_NONE);
    }
#endif
                                                                /* ---------------- SET DEST IP ADDR ------------------ */
    Mem_Copy(&addr_dest, p_addr_dest, sizeof(NET_IPv4_ADDR));


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetICMPv4_TxEchoReq, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (0u);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit tx (see Note #2).         */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif

                                                                /* ----------------- SET SRC IP ADDR ------------------ */
    addr_src = NetIPv4_GetAddrSrcHandler(addr_dest);


                                                                /* ------------------ TX ICMPv4 REQ ------------------- */
    id_seq = NetICMPv4_TxMsgReqHandler(NET_ICMPv4_MSG_TYPE_ECHO_REQ,
                                       NET_ICMPv4_MSG_CODE_ECHO_REQ,
                                       id,
                                       addr_src,
                                       addr_dest,
                                       NET_IPv4_HDR_TOS_NONE,
                                       NET_IPv4_HDR_TTL_MAX,
                                       NET_IPv4_FLAG_NONE,
                                       DEF_NULL,
                                       p_data,
                                       data_len,
                                       p_err);
    if (*p_err != NET_ICMPv4_ERR_NONE) {
         goto exit_release;
    }

   *p_err = NET_ICMPv4_ERR_NONE;

exit_release:
    Net_GlobalLockRelease();
    return (id_seq.SeqNbr);
}


/*
*********************************************************************************************************
*                                         NetICMPv4_TxMsgErr()
*
* Description : (1) Transmit ICMPv4 Error Message in response to received packet with one or more errors :
*
*                   (a) Validate ICMPv4 Error Message
*
*                   (b) Get buffer for ICMPv4 Error Message :
*
*                       (1) Calculate  ICMPv4 Error Message buffer size
*                       (2) Copy received packet's IP header & data into ICMPv4 Error Message
*                       (3) Initialize ICMPv4 Error Message buffer controls
*
*                   (c) Prepare ICMPv4 Error Message :
*
*                       (1) Type                           See 'net_icmpv4.h  ICMP MESSAGE TYPES & CODES'
*                       (2) Code                           See 'net_icmpv4.h  ICMP MESSAGE TYPES & CODES'
*                       (3) Pointer
*                       (4) Unused
*                       (5) Check-Sum
*
*                   (d) Transmit ICMPv4 Error Message
*
*                       (1) RFC #1122, Section 3.2.2 specifies that "an ICMP error message SHOULD be sent
*                           with normal (i.e., zero) TOS bits".  RFC #1349, Section 5.1 confirms that "an
*                           ICMP error message is always sent with the default TOS (0000)".
*
*                   (e) Free ICMPv4 Error Message buffer
*                   (f) Update transmit statistics
*
*
* Argument(s) : p_buf   Pointer to network buffer that received a packet with error(s).
*
*               type    ICMPv4 Error Message type (see Note #1c1) :
*                       NET_ICMPv4_MSG_TYPE_DEST_UNREACH
*                       NET_ICMPv4_MSG_TYPE_TIME_EXCEED
*                       NET_ICMPv4_MSG_TYPE_PARAM_PROB
*
*               code    ICMPv4 Error Message code (see Note #1c2).
*
*               ptr     Pointer to received packet's ICMPv4 error (optional).
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_ICMPv4_ERR_NONE             ICMPv4 Error Message successfully transmitted.
*
*                           ---------------------- RETURNED BY NetIPv4_Tx()-----------------------
*                           See NetIPv4_Tx() for additional return error codes.
*
*                           ---------------- RETURNED BY NetICMPv4_TxPktDiscard() ----------------
*                           See NetICMPv4_TxPktDiscard() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktFragTimeout(),
*               NetIPv4_RxPktValidate(),
*               NetIPv4_RxPktValidateOpt(),
*               NetTCP_RxPktDemuxSeg(),
*               NetUDP_Rx().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (2) (a) The following IPv4 header fields MUST be decoded &/or converted from network-order to host-
*                       order BEFORE any ICMPv4 Error Messages are transmitted (see 'net_ip.c  NetIP_RxPktValidate()
*                       Note #3') :
*
*                       (1) Header Length
*                       (2) Total  Length
*                       (3) Source      Address
*                       (4) Destination Address
*
*                   (b) The following IPv4 header fields were NOT previously decoded &/or converted from network-
*                       order to host-order & are NOT available :
*
*                       (1) IP Data Length
*
*               (3) Default case already invalidated in NetICMPv4_TxMsgErrValidate().  However, the default
*                   case is included as an extra precaution in case 'type' is incorrectly modified.
*
*               (4) Assumes network buffer's protocol header size is large enough to accommodate ICMP header
*                   size (see 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #1').
*
*               (5) Some buffer controls were previously initialized in NetBuf_Get() when the buffer
*                   was allocated earlier in this function.  These buffer controls do NOT need to be
*                   re-initialized but are shown for completeness.
*
*               (6) (a) ICMPv4 message Check-Sum MUST be calculated AFTER the entire ICMPv4 message has been
*                       prepared.  In addition, ALL multi-octet words are converted from host-order to
*                       network-order since "the sum of 16-bit integers can be computed in either byte
*                       order" [RFC #1071, Section 2.(B)].
*
*                   (b) ICMPv4 message Check-Sum field MUST be cleared to '0' BEFORE the ICMP message Check-Sum
*                       is calculated (see RFC #792, Sections 'Destination Unreachable Message : Checksum',
*                       'Time Exceeded Message : Checksum', 'Source Quench Message : Checksum', & 'Parameter
*                       Problem Message : Checksum').
*
*                   (c) The ICMPv4 message Check-Sum field is returned in network-order & MUST NOT be re-converted
*                       back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumHdrCalc()  Note #3b').
*
*               (7) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

void  NetICMPv4_TxMsgErr (NET_BUF     *p_buf,
                          CPU_INT08U   type,
                          CPU_INT08U   code,
                          CPU_INT08U   ptr,
                          NET_ERR     *p_err)
{
    NET_BUF_HDR                *p_buf_hdr;
    NET_IPv4_HDR               *p_ip_hdr;
    NET_BUF                    *p_msg_err;
    NET_BUF_HDR                *p_msg_err_hdr;
    NET_ICMPv4_HDR_ERR         *p_icmp_hdr_err;
    NET_ICMPv4_HDR_PARAM_PROB  *p_icmp_hdr_param_prob;
    CPU_INT16U                  msg_size_hdr;
    CPU_INT16U                  msg_size_data_ip;
    CPU_INT16U                  msg_size_data;
    CPU_INT16U                  msg_size_tot;
    CPU_INT16U                  msg_ix;
    CPU_INT16U                  msg_ix_offset;
    CPU_INT16U                  msg_ix_data;
    CPU_INT16U                  msg_chk_sum;
    NET_ERR                     err;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.NullPtrCtr);
        return;
    }
#endif



                                                                /* ------------ VALIDATE ICMPv4 TX ERR MSG ------------ */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf_hdr->IP_HdrIx == NET_BUF_IX_NONE) {
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvalidBufIxCtr);
        return;
    }
#endif
    p_ip_hdr = (NET_IPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];


    NetICMPv4_TxMsgErrValidate(p_buf, p_buf_hdr, p_ip_hdr, type, code, ptr, &err);
    if (err != NET_ICMPv4_ERR_NONE) {
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        return;
    }


                                                                /* -------------- GET ICMPv4 ERR MSG BUF -------------- */
                                                                /* Calc err msg buf size.                               */
    switch (type) {
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
             msg_size_hdr = NET_ICMPv4_HDR_SIZE_DEST_UNREACH;
             break;


        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
             msg_size_hdr = NET_ICMPv4_HDR_SIZE_TIME_EXCEED;
             break;


        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
             msg_size_hdr = NET_ICMPv4_HDR_SIZE_PARAM_PROB;
             break;


        default:                                                /* See Note #3.                                         */
             NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                    (NET_ERR *)p_err);
             return;
    }


    if (p_buf_hdr->IP_TotLen >= p_buf_hdr->IP_HdrLen) {                     /* If IP tot len > IP hdr len &      ...    */
        msg_size_data_ip = p_buf_hdr->IP_TotLen - p_buf_hdr->IP_HdrLen;     /* Calc IP data len (see Note #2b1).        */
        if (msg_size_data_ip >= NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_OCTETS) {  /* ... ip data >= min ICMPv4 data len, ...  */
                                                                            /* ... get max ICMPv4 err msg len.          */
            msg_size_data = p_buf_hdr->IP_HdrLen + NET_ICMPv4_MSG_ERR_DATA_SIZE_MIN_OCTETS;
        } else {                                                            /* Else get max IP tot len.                 */
            msg_size_data = p_buf_hdr->IP_TotLen;
        }
    } else {                                                                /* Else get max IP tot len.                 */
        msg_size_data = p_buf_hdr->IP_TotLen;
    }

    msg_size_tot = msg_size_hdr + msg_size_data;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
#if 0
    if (NET_BUF_DATA_IX_TX < msg_size_hdr) {                    /* See Note #4.                                         */
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxInvalidBufIxCtr);
        return;
    }
#endif
#endif
                                                                /* Get err msg buf.                                     */
#if 0
    msg_ix   = NET_BUF_DATA_IX_TX - msg_size_hdr;
#else
    msg_ix = 0u;
    NetICMPv4_TxIxDataGet(p_buf_hdr->IF_Nbr,
                          msg_size_tot,
                         &msg_ix,
                         &err);
#endif

    p_msg_err = NetBuf_Get((NET_IF_NBR     ) p_buf_hdr->IF_Nbr,
                          (NET_TRANSACTION) NET_TRANSACTION_TX,
                          (NET_BUF_SIZE   ) msg_size_tot,
                          (NET_BUF_SIZE   ) msg_ix,
                          (NET_BUF_SIZE  *)&msg_ix_offset,
                          (NET_BUF_FLAGS  ) NET_BUF_FLAG_NONE,
                          (NET_ERR       *)&err);
    if (err != NET_BUF_ERR_NONE) {
        NetICMPv4_TxPktDiscard(p_msg_err, p_err);
        return;
    }


    msg_ix      += msg_ix_offset;
    msg_ix_data  = msg_ix + msg_size_hdr;
    NetBuf_DataWr((NET_BUF    *) p_msg_err,                     /* Copy rx'd IPv4 hdr & data into ICMPv4 err tx buf.    */
                  (NET_BUF_SIZE) msg_ix_data,
                  (NET_BUF_SIZE) msg_size_data,
                  (CPU_INT08U *) p_ip_hdr,
                  (NET_ERR    *)&err);
    if (err != NET_BUF_ERR_NONE) {
        NetICMPv4_TxPktDiscard(p_msg_err, p_err);
        return;
    }

                                                                /* Init err msg buf ctrls.                              */
    p_msg_err_hdr                        = &p_msg_err->Hdr;
    p_msg_err_hdr->ICMP_MsgIx            = (CPU_INT16U  )msg_ix;
    p_msg_err_hdr->ICMP_MsgLen           = (CPU_INT16U  )msg_size_tot;
    p_msg_err_hdr->ICMP_HdrLen           = (CPU_INT16U  )msg_size_hdr;
    p_msg_err_hdr->TotLen                = (NET_BUF_SIZE)p_msg_err_hdr->ICMP_MsgLen;
    p_msg_err_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_ICMP_V4;
    p_msg_err_hdr->ProtocolHdrTypeNetSub =  NET_PROTOCOL_TYPE_ICMP_V4;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #5].                */
    p_msg_err_hdr->DataIx                =  NET_BUF_IX_NONE;
    p_msg_err_hdr->DataLen               =  0u;
#endif




                                                                /* -------------- PREPARE ICMPv4 ERR MSG -------------- */
    switch (type) {
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
             p_icmp_hdr_err       = (NET_ICMPv4_HDR_ERR *)&p_msg_err->DataPtr[p_msg_err_hdr->ICMP_MsgIx];
             p_icmp_hdr_err->Type =  type;
             p_icmp_hdr_err->Code =  code;
                                                                            /* Clr unused octets.                       */
             Mem_Clr((void     *)p_icmp_hdr_err->Unused,
                     (CPU_SIZE_T)NET_ICMPv4_HDR_NBR_OCTETS_UNUSED);
                                                                            /* Calc ICMPv4 msg chk sum (see Note #6).   */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_err->ChkSum, 0x0000u);     /* Clr             chk sum (see Note #6b).  */
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *) p_icmp_hdr_err,
                                                             (CPU_INT16U) p_msg_err_hdr->ICMP_MsgLen,
                                                             (NET_ERR  *)&err);
#endif
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_err->ChkSum, &msg_chk_sum);   /* Copy chk sum in net order (see Note #6c).*/
             break;


        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
             p_icmp_hdr_param_prob       = (NET_ICMPv4_HDR_PARAM_PROB *)&p_msg_err->DataPtr[p_msg_err_hdr->ICMP_MsgIx];
             p_icmp_hdr_param_prob->Type =  type;
             p_icmp_hdr_param_prob->Code =  code;
             p_icmp_hdr_param_prob->Ptr  =  ptr;
                                                                                /* Clr unused octets.                   */
             Mem_Clr((void     *)p_icmp_hdr_param_prob->Unused,
                     (CPU_SIZE_T)NET_ICMPv4_HDR_NBR_OCTETS_UNUSED_PARAM_PROB);
                                                                                /* Calc msg chk sum (see Note #6).      */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_param_prob->ChkSum, 0x0000u);  /* Clr      chk sum (see Note #6b).     */
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *) p_icmp_hdr_param_prob,
                                                             (CPU_INT16U) p_msg_err_hdr->ICMP_MsgLen,
                                                             (NET_ERR  *)&err);
#endif
                                                                                /* Copy chk sum in net order (see Note #6c).*/
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_param_prob->ChkSum, &msg_chk_sum);
             break;


        default:                                                /* See Note #3.                                         */
             NetICMPv4_TxPktDiscard(p_msg_err, p_err);
             return;
    }

    if (err != NET_UTIL_ERR_NONE) {                             /* Chk err from NetUtil_16BitOnesCplChkSumHdrCalc().    */
        NetICMPv4_TxPktDiscard(p_msg_err, p_err);
        return;
    }



                                                                /* ---------------- TX ICMPv4 ERR MSG ----------------- */
    NetIPv4_Tx((NET_BUF      *)p_msg_err,
               (NET_IPv4_ADDR )p_buf_hdr->IP_AddrDest,
               (NET_IPv4_ADDR )p_buf_hdr->IP_AddrSrc,
               (NET_IPv4_TOS  )NET_IPv4_TOS_DFLT,               /* See Note #1d1.                                       */
               (NET_IPv4_TTL  )NET_IPv4_TTL_DFLT,
               (NET_IPv4_FLAGS)NET_IPv4_FLAG_NONE,
               (void         *)0,
               (NET_ERR      *)p_err);



                                                                /* ------ FREE ICMPv4 ERR MSG / UPDATE TX STATS ------- */
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             NetICMPv4_TxPktFree(p_msg_err);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.TxMsgCtr);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.TxMsgErrCtr);
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #7.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr);
                                                                /* Rtn err from NetIPv4_Tx().                           */
             return;


        case NET_IPv4_ERR_TX_PKT:
                                                                /* See Note #7.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr);
            *p_err = NET_ERR_TX;
             return;


        default:
             NetICMPv4_TxPktDiscard(p_msg_err, p_err);
             return;
    }


   *p_err = NET_ICMPv4_ERR_NONE;
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
*                                       NetICMPv4_RxReplyDemux()
*
* Description : Demultiplex ICMPv4 reply.
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMPv4 message.
*               -----       Argument checked   in NetICMPv4_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetICMPv4_Rx().
*
*               p_icmp_hdr  Pointer to network buffer ICMPv4 header.
*               ----------  Argument validated in NetICMPv4_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               NET_ICMPv4_ERR_NONE      Message     successfully demultiplexed.
*                               NET_ICMPv4_ERR_RX_REPLY  Message not successfully demultiplexed.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_Rx().
*
* Note(s)     : (1) ICMPv4 Receive Error/Reply Messages NOT yet fully implemented #### NET-806
*
*                   (a) Only support Echo Reply.
*
*                   (a) Define "User Process" to report ICMPv4 Error Messages to Transport &/or Application Layers.
*
*                   (b) Define procedure to demultiplex & enqueue ICMPv4 Reply Messages to Application.
*
*                       (1) MUST implement mechanism to de-queue ICMPv4 message data from single, complete
*                           datagram packet buffers or multiple, fragmented packet buffers.
*********************************************************************************************************
*/

static  void  NetICMPv4_RxReplyDemux (NET_BUF         *p_buf,
                                      NET_BUF_HDR     *p_buf_hdr,
                                      NET_ICMPv4_HDR  *p_icmp_hdr,
                                      NET_ERR         *p_err)
{
    NET_ICMPv4_HDR_ECHO  *p_icmp_echo_hdr;
    CPU_INT16U            id;
    CPU_INT16U            seq;
    CPU_INT16U            data_len;



    switch (p_icmp_hdr->Type) {
        case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
             p_icmp_echo_hdr = (NET_ICMPv4_HDR_ECHO *)p_icmp_hdr;
             data_len        =  p_buf_hdr->ICMP_MsgLen - p_buf_hdr->ICMP_HdrLen;
             id              =  NET_UTIL_NET_TO_HOST_16(p_icmp_echo_hdr->ID);
             seq             =  NET_UTIL_NET_TO_HOST_16(p_icmp_echo_hdr->SeqNbr);

             NetICMP_RxEchoReply(id,
                                 seq,
                                 p_icmp_echo_hdr->Data,
                                 data_len,
                                 p_err);
             if (*p_err != NET_ICMP_ERR_NONE) {
                 *p_err  = NET_ICMPv4_ERR_RX_REPLY;
                  return;
             }
             break;


        default:
            *p_err = NET_ICMPv4_ERR_RX_REPLY;
             return;
    }

   (void)&p_buf;


   *p_err = NET_ICMPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetICMPv4_RxPktValidateBuf()
*
* Description : Validate received buffer header as ICMPv4 protocol.
*
* Argument(s) : p_buf_hdr    Pointer to network buffer header that received ICMPv4 packet.
*               ---------    Argument validated in NetICMPv4_Rx().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE             Received buffer's ICMPv4 header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT ICMPv4.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetICMPv4_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
                                          NET_ERR      *p_err)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  valid;
    NET_ERR      err;

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

                                                                /* -------------- VALIDATE ICMPv4 BUF HDR ------------- */
    if (p_buf_hdr->ProtocolHdrType != NET_PROTOCOL_TYPE_ICMP_V4) {
        if_nbr = p_buf_hdr->IF_Nbr;
        valid  = NetIF_IsValidHandler(if_nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvalidProtocolCtr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->ICMP_MsgIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvalidBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_ICMPv4_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       NetICMPv4_RxPktValidate()
*
* Description : (1) Validate received ICMPv4 packet :
*
*                   (a) Validate the received packet's destination                  See Note #3
*
*                   (b) (1) Validate the received packet's following ICMPv4 header fields :
*
*                           (A) Type
*                           (B) Code
*                           (C) Message Length
*                           (D) Pointer                         See RFC  #792, Section 'Parameter Problem Message'
*                           (E) Check-Sum                       See Note #7a
*
*                       (2) Validation ignores the following ICMPv4 header fields :
*
*                           (A) Unused                          See RFC # 792, Section 'Message Formats'
*                           (B) Data                            See RFC #1122, Sections 3.2.2 & 3.2.2.6
*                           (C) Identification (ID)             See RFC # 792, Sections 'Echo or Echo Reply Message'
*                                                                   & 'Timestamp or Timestamp Reply Message'
*                           (D) Sequence Number                 See RFC # 792, Sections 'Echo or Echo Reply Message'
*                                                                   & 'Timestamp or Timestamp Reply Message'
*
*                   (c) Convert the following ICMPv4 message fields from network-order to host-order :
*
*                       (1) Check-Sum                                               See Note #7c
*
*                           (A) These fields are NOT converted directly in the received packet buffer's
*                               data area but are converted in local or network buffer variables ONLY.
*
*                   (d) Update network buffer's length controls
*
*                   (e) Demultiplex ICMPv4 message type
*
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMP packet.
*               -----       Argument checked   in NetICMPv4_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetICMPv4_Rx().
*
*               p_icmp_hdr  Pointer to received packet's ICMP header.
*               ----------  Argument validated in NetICMPv4_Rx()/NetICMPv4_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_MSG_TYPE_ERR       Received packet validated as ICMPv4 Error   Message.
*                               NET_ICMPv4_ERR_MSG_TYPE_REQ       Received packet validated as ICMPv4 Request Message.
*                               NET_ICMPv4_ERR_MSG_TYPE_REPLY     Received packet validated as ICMPv4 Reply   Message.
*
*                               NET_ICMPv4_ERR_RX_BROADCAST       Packet received via broadcast.
*                               NET_ICMPv4_ERR_RX_MCAST       Packet received via multicast.
*
*                               NET_ICMPv4_ERR_INVALID_TYPE       Invalid/unknown ICMPv4 message type.
*                               NET_ICMPv4_ERR_INVALID_CODE       Invalid/unknown ICMPv4 message code.
*                               NET_ICMPv4_ERR_INVALID_PTR        Invalid ICMPv4 message pointer outside ICMPv4 message.
*                               NET_ICMPv4_ERR_INVALID_LEN        Invalid ICMPv4 message      length.
*                               NET_ICMPv4_ERR_INVALID_LEN_DATA   Invalid ICMPv4 message data length.
*                               NET_ICMPv4_ERR_INVALID_CHK_SUM    Invalid ICMPv4 check-sum.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_Rx().
*
* Note(s)     : (2) See 'net_icmpv4.h  ICMPv4 HEADER' for ICMPv4 header format.
*
*               (3) (a) RFC #1122, Sections 3.2.2.6 & 3.2.2.8 state that "an ICMP ... Request destined to an
*                       IP broadcast or IP multicast address MAY be silently discarded".
*
*                       However, NO RFC specifies how OTHER ICMPv4 messages should handle messages received by
*                       broadcast or multicast.  Therefore, it is assumed that ALL ICMPv4 messages destined
*                       to an IP broadcast or IP multicast address SHOULD be silently discarded.
*
*                   (b) Since a packet destined to a valid IPv4 broadcast address MUST also have been received as
*                       a link-layer broadcast (see RFC #1122, Section 3.3.6 & 'net_ip.c  NetIPv4_RxPktValidate()
*                       Note #9d3B1b'), the determination of an IPv4 broadcast destination address need only verify
*                       that the received packet was received as a link-layer broadcast packet.
*
*               (4) (a) RFC #1122, Section 3.2.2 requires that ICMPv4 messages with the following invalid ICMPv4
*                       header fields be "silently discarded" :
*
*                       (1) Type                                    RFC #1122, Section 3.2.2
*
*                   (b) Assumes that ICMPv4 messages with the following invalid ICMPv4 header fields should also
*                       be "silently discarded" :
*
*                       (1) Code
*                       (2) Message Length
*                       (3) Pointer
*                       (4) Check-Sum
*
*               (5) See 'net_icmpv4.h  ICMPv4 MESSAGE TYPES & CODES' for supported ICMPv4 message types/codes.
*
*               (6) Since ICMPv4 message headers do NOT contain a message length field, the ICMPv4 Message Length
*                   is assumed to be the remaining IPv4 Datagram Length.
*
*               (7) (a) ICMPv4 message Check-Sum field MUST be validated AFTER (or BEFORE) any multi-octet
*                       words are converted from network-order to host-order since "the sum of 16-bit
*                       integers can be computed in either byte order" [RFC #1071, Section 2.(B)].
*
*                       In other words, the ICMPv4 message Check-Sum CANNOT be validated AFTER SOME but NOT
*                       ALL multi-octet words have been converted from network-order to host-order.
*
*                       However, ALL received packets' multi-octet words are converted in local or network
*                       buffer variables ONLY (see Note #1cA).  Therefore, ICMPv4 message Check-Sum may be
*                       validated at any point.
*
*                   (b) RFC #792, Section 'Echo or Echo Reply Message : Checksum' specifies that "if the
*                       total length is odd, the received data is padded with one octet of zeros for
*                       computing the checksum".
*
*                       However, NO RFC specifies how OTHER ICMP message types should handle odd-length
*                       check-sums.  Therefore, it is assumed that ICMPv4 Echo Request & Echo Reply Messages
*                       should handle odd-length check-sums according to RFC #792's 'Echo or Echo Reply
*                       Message : Checksum' specification, while ALL other ICMPv4 message types should handle
*                       odd-length check-sums according to RFC #1071, Section 4.1.
*
*                       See also 'net_util.c  NetUtil_16BitSumDataCalc()  Note #8'.
*
*                   (c) After the ICMPv4 message Check-Sum is validated, it is NOT necessary to convert the
*                       Check-Sum from network-order to host-order since it is NOT required for further
*                       processing.
*
*               (8) Default case already invalidated earlier in this function.  However, the default case
*                   is included as an extra precaution in case 'Type' is incorrectly modified.
*
*               (9) (a) (1) Except for ICMPv4 Echo & Echo Reply Messages (see Note #9b), most ICMP messages do
*                           NOT permit user &/or application data (see RFC #792 & RFC #1122, Sections 3.2.2).
*
*                       (2) Most ICMPv4 messages that do NOT contain user &/or application data will NOT be
*                           received in separate packet buffers since most ICMP messages are NOT large enough
*                           to be fragmented since the minimum network buffer size MUST be configured such
*                           that most ICMPv4 messages fit within a single packet buffer (see 'net_buf.h
*                           NETWORK BUFFER INDEX & SIZE DEFINES  Note #1d').
*
*                           However, RFC #1122, Section 3.2.2 states that "every ICMPv4 error message includes
*                           the Internet header and at least the first 8 data octets of the datagram that
*                           triggered the error; more than 8 octets MAY be sent".  Thus, it is possible that
*                           some received ICMPv4 error messages MAY contain more than 8 octets of the Internet
*                           header & may therefore be received in one or more fragmented packet buffers.
*
*                           Furthermore, these additional error message octets SHOULD NOT contain user &/or
*                           application data.
*
*                       (3) ICMPv4 data index value to clear was previously initialized in NetBuf_Get() when
*                           the buffer was allocated.  This index value does NOT need to be re-cleared but
*                           is shown for completeness.
*
*                   (b) (1) ICMPv4 Echo & Echo Reply Messages permit the transmission & receipt of user &/or
*                           application data (see RFC #792, Section 'Echo or Echo Reply Message' & RFC #1122,
*                           Section 3.2.2.6).
*
*                           Since the minimum network buffer size MUST be configured such that the entire
*                           ICMPv4 Echo Message header MUST be received in a single packet (see 'net_buf.h
*                           NETWORK BUFFER INDEX & SIZE DEFINES  Note #1d'), after the ICMPv4 Echo Message
*                           header size is decremented from the first packet buffer's remaining number of
*                           data octets, any remaining octets MUST be user &/or application data octets.
*
*                           (A) Note that the 'Data' index is updated regardless of a null-size data length.
*
*                       (2) If additional packet buffers exist, the remaining IP datagram 'Data' MUST be
*                           user &/or application data.  Therefore, the 'Data' length does NOT need to
*                           be adjusted but the 'Data' index MUST be updated.
*
*                   (c) #### Total ICMPv4 Message Length is duplicated in ALL fragmented packet buffers
*                       (may NOT be necessary; remove if unnecessary).
*********************************************************************************************************
*/
static  void  NetICMPv4_RxPktValidate (NET_BUF         *p_buf,
                                       NET_BUF_HDR     *p_buf_hdr,
                                       NET_ICMPv4_HDR  *p_icmp_hdr,
                                       NET_ERR         *p_err)
{
    CPU_BOOLEAN                 rx_broadcast       = DEF_NO;
    CPU_BOOLEAN                 addr_multicast     = DEF_NO;
    CPU_BOOLEAN                 icmp_chk_sum_valid = DEF_OK;
    CPU_INT16U                  icmp_msg_len_hdr   = 0u;
    CPU_INT16U                  icmp_msg_len_min   = 0u;
    CPU_INT16U                  icmp_msg_len_max   = 0u;
    CPU_INT16U                  icmp_msg_len       = 0u;
    NET_ICMPv4_HDR_PARAM_PROB  *p_icmp_param_prob  = DEF_NULL;
    NET_BUF                    *p_buf_next         = DEF_NULL;
    NET_BUF_HDR                *p_buf_next_hdr     = DEF_NULL;


                                                                /* ------------- VALIDATE ICMPv4 RX DEST -------------- */
    rx_broadcast = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_BROADCAST);
    if (rx_broadcast != DEF_NO) {                               /* If ICMPv4 rx'd as broadcast, rtn err (see Note #3).  */
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxBcastCtr);
       *p_err = NET_ICMPv4_ERR_RX_BROADCAST;
        return;
    }

    addr_multicast = NetIPv4_IsAddrMulticast(p_buf_hdr->IP_AddrDest);
    if (addr_multicast != DEF_NO) {                             /* If ICMPv4 rx'd as multicast, rtn err (see Note #3).  */
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxMcastCtr);
       *p_err = NET_ICMPv4_ERR_RX_MCAST;
        return;
    }



                                                                /* ------------ VALIDATE ICMPv4 TYPE/CODE ------------- */
    switch (p_icmp_hdr->Type) {                                 /* See Notes #4a1 & #4b1.                               */
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
             switch (p_icmp_hdr->Code) {
                 case NET_ICMPv4_MSG_CODE_DEST_NET:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST:
                 case NET_ICMPv4_MSG_CODE_DEST_PROTOCOL:
                 case NET_ICMPv4_MSG_CODE_DEST_PORT:
                 case NET_ICMPv4_MSG_CODE_DEST_FRAG_NEEDED:
                 case NET_ICMPv4_MSG_CODE_DEST_ROUTE_FAIL:
                 case NET_ICMPv4_MSG_CODE_DEST_NET_UNKNOWN:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST_UNKNOWN:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST_ISOLATED:
                 case NET_ICMPv4_MSG_CODE_DEST_NET_TOS:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST_TOS:
                      icmp_msg_len_hdr = NET_ICMPv4_HDR_SIZE_DEST_UNREACH;
                      icmp_msg_len_min = NET_ICMPv4_MSG_LEN_MIN_DEST_UNREACH;
                      icmp_msg_len_max = NET_ICMPv4_MSG_LEN_MAX_DEST_UNREACH;
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
             switch (p_icmp_hdr->Code) {
                 case NET_ICMPv4_MSG_CODE_TIME_EXCEED_TTL:
                 case NET_ICMPv4_MSG_CODE_TIME_EXCEED_FRAG_REASM:
                      icmp_msg_len_hdr = NET_ICMPv4_HDR_SIZE_TIME_EXCEED;
                      icmp_msg_len_min = NET_ICMPv4_MSG_LEN_MIN_TIME_EXCEED;
                      icmp_msg_len_max = NET_ICMPv4_MSG_LEN_MAX_TIME_EXCEED;
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
             switch (p_icmp_hdr->Code) {
                 case NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR:
                 case NET_ICMPv4_MSG_CODE_PARAM_PROB_OPT_MISSING:
                      icmp_msg_len_hdr =  NET_ICMPv4_HDR_SIZE_PARAM_PROB;
                      icmp_msg_len_min =  NET_ICMPv4_MSG_LEN_MIN_PARAM_PROB;
                      icmp_msg_len_max =  NET_ICMPv4_MSG_LEN_MAX_PARAM_PROB;

                      p_icmp_param_prob = (NET_ICMPv4_HDR_PARAM_PROB *)p_icmp_hdr;
                      if (p_icmp_param_prob->Ptr < NET_ICMPv4_MSG_PTR_MIN_PARAM_PROB) {  /* If ptr val < min ptr val, .. */
                          NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvPtrCtr);
                         *p_err = NET_ICMPv4_ERR_INVALID_PTR;                            /* ... rtn err (see Note #4b3). */
                          return;
                      }
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
        case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
             switch (p_icmp_hdr->Code) {
                 case NET_ICMPv4_MSG_CODE_ECHO:
                      icmp_msg_len_hdr = NET_ICMPv4_HDR_SIZE_ECHO;
                      icmp_msg_len_min = NET_ICMPv4_MSG_LEN_MIN_ECHO;
                      icmp_msg_len_max = NET_ICMPv4_MSG_LEN_MAX_ECHO;
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_TS_REQ:
        case NET_ICMPv4_MSG_TYPE_TS_REPLY:
             switch (p_icmp_hdr->Code) {
                 case NET_ICMPv4_MSG_CODE_TS:
                      icmp_msg_len_hdr = NET_ICMPv4_HDR_SIZE_TS;
                      icmp_msg_len_min = NET_ICMPv4_MSG_LEN_MIN_TS;
                      icmp_msg_len_max = NET_ICMPv4_MSG_LEN_MAX_TS;
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY:
             switch (p_icmp_hdr->Code) {
                 case NET_ICMPv4_MSG_CODE_ADDR_MASK:
                      icmp_msg_len_hdr = NET_ICMPv4_HDR_SIZE_ADDR_MASK;
                      icmp_msg_len_min = NET_ICMPv4_MSG_LEN_MIN_ADDR_MASK;
                      icmp_msg_len_max = NET_ICMPv4_MSG_LEN_MAX_ADDR_MASK;
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;

#if 0                                                           /* ---------------- UNSUPPORTED TYPES ----------------- */
                                                                /* See Note #5.                                         */
        case NET_ICMPv4_MSG_TYPE_REDIRECT:
        case NET_ICMPv4_MSG_TYPE_ROUTE_REQ:
        case NET_ICMPv4_MSG_TYPE_ROUTE_AD:
        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ:
#endif
        default:                                                /* ------------------ INVALID TYPES ------------------- */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvTypeCtr);
            *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
             return;
    }



                                                                /* ------------- VALIDATE ICMPv4 MSG LEN -------------- */
    icmp_msg_len          = p_buf_hdr->IP_DatagramLen;          /* See Note #6.                                         */
    p_buf_hdr->ICMP_MsgLen = icmp_msg_len;
    if (p_buf_hdr->ICMP_MsgLen < icmp_msg_len_min) {            /* If msg len < min msg len, rtn err.                   */
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvMsgLenCtr);
       *p_err = NET_ICMPv4_ERR_INVALID_LEN;
        return;
    }
    if (icmp_msg_len_max != NET_ICMPv4_MSG_LEN_MAX_NONE) {
        if (p_buf_hdr->ICMP_MsgLen > icmp_msg_len_max) {         /* If msg len > max msg len, rtn err.                   */
            NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvMsgLenCtr);
           *p_err = NET_ICMPv4_ERR_INVALID_LEN;
            return;
        }
    }

    if (p_icmp_hdr->Type == NET_ICMPv4_MSG_TYPE_PARAM_PROB) {    /* For ICMP Param Prob msg,   ...                       */
        if (p_icmp_param_prob->Ptr >= p_buf_hdr->ICMP_MsgLen) {  /* ... if ptr val >= msg len, ...                       */
            NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvPtrCtr);
           *p_err = NET_ICMPv4_ERR_INVALID_PTR;                  /* ... rtn err (see Note #4b3).                         */
            return;
        }
    }


                                                                /* ------------- VALIDATE ICMPv4 CHK SUM -------------- */
                                                                /* See Note #7.                                         */
    switch (p_icmp_hdr->Type) {                                 /* See Note #7b.                                        */
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
        case NET_ICMPv4_MSG_TYPE_SRC_QUENCH:
        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
        case NET_ICMPv4_MSG_TYPE_TS_REQ:
        case NET_ICMPv4_MSG_TYPE_TS_REPLY:
        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY:
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_RX
             icmp_chk_sum_valid = DEF_YES;
#else
             icmp_chk_sum_valid = NetUtil_16BitOnesCplChkSumHdrVerify(p_icmp_hdr, p_buf_hdr->ICMP_MsgLen, p_err);
#endif
             break;


        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
        case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
#ifdef  NET_ICMP_CHK_SUM_OFFLOAD_RX
             icmp_chk_sum_valid = DEF_YES;
#else
             icmp_chk_sum_valid = NetUtil_16BitOnesCplChkSumDataVerify(p_buf, 0, 0u, p_err);
#endif
             break;


        default:                                                /* See Note #8.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvTypeCtr);
            *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
             return;
    }

    if (icmp_chk_sum_valid != DEF_OK) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvChkSumCtr);
       *p_err = NET_ICMPv4_ERR_INVALID_CHK_SUM;
        return;
    }
#if 0                                                           /* Conv to host-order NOT necessary (see Note #7c).     */
   (void)NET_UTIL_VAL_GET_NET_16(&p_icmp_hdr->ChkSum);
#endif



                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr->ICMP_HdrLen = icmp_msg_len_hdr;

    switch (p_icmp_hdr->Type) {
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
        case NET_ICMPv4_MSG_TYPE_SRC_QUENCH:
        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
        case NET_ICMPv4_MSG_TYPE_TS_REQ:
        case NET_ICMPv4_MSG_TYPE_TS_REPLY:
        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY:
             p_buf_hdr->DataLen = 0u;                           /* Clr data len/ix       (see Note #9a1).               */
#if 0                                                           /* Clr'd in NetBuf_Get() [see Note #9a3].               */
             p_buf_hdr->DataIx  = NET_BUF_IX_NONE;
#endif

             p_buf_next = p_buf_hdr->NextBufPtr;
             while (p_buf_next != (NET_BUF *)0) {               /* Clr ALL pkt bufs' data len/ix (see Note #9a2).       */
                 p_buf_next_hdr              = &p_buf_next->Hdr;
                 p_buf_next_hdr->DataLen     =  0u;
#if 0                                                           /* Clr'd in NetBuf_Get() [see Note #9a3].               */
                 p_buf_next_hdr->DataIx      =  NET_BUF_IX_NONE;
#endif
                 p_buf_next_hdr->ICMP_HdrLen =  0u;             /* NULL ICMPv4 hdr len in each pkt buf.                 */
                 p_buf_next_hdr->ICMP_MsgLen =  icmp_msg_len;   /* Dup  ICMPv4 msg len in each pkt buf (see Note #9c).  */
                 p_buf_next                  =  p_buf_next_hdr->NextBufPtr;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
        case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
                                                                /* Calc ICMP Echo Msg data len/ix (see Note #9b).       */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
             if (p_buf_hdr->ICMP_HdrLen > p_buf_hdr->DataLen) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxHdrDataLenCtr);
                *p_err = NET_ICMPv4_ERR_INVALID_LEN_DATA;
                 return;
             }
#endif
             p_buf_hdr->DataLen -= (NET_BUF_SIZE) p_buf_hdr->ICMP_HdrLen;
             p_buf_hdr->DataIx   = (CPU_INT16U  )(p_buf_hdr->ICMP_MsgIx + NET_ICMPv4_MSG_LEN_MIN_ECHO);

             p_buf_next = p_buf_hdr->NextBufPtr;
             while (p_buf_next != (NET_BUF *)0) {               /* Calc ALL pkt bufs' data len/ix   (see Note #9b2).    */
                 p_buf_next_hdr              = &p_buf_next->Hdr;
                 p_buf_next_hdr->DataIx      =  p_buf_next_hdr->ICMP_MsgIx;
                 p_buf_next_hdr->ICMP_HdrLen =  0u;             /* NULL ICMP hdr len in each pkt buf.                   */
                 p_buf_next_hdr->ICMP_MsgLen =  icmp_msg_len;   /* Dup  ICMP msg len in each pkt buf (see Note #9c).    */
                 p_buf_next                  =  p_buf_next_hdr->NextBufPtr;
             }
             break;


        default:                                                /* See Note #8.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvTypeCtr);
            *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
             return;
    }



                                                                /* -------------- DEMUX ICMPv4 MSG TYPE --------------- */
    switch (p_icmp_hdr->Type) {
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
        case NET_ICMPv4_MSG_TYPE_SRC_QUENCH:
        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
            *p_err = NET_ICMPv4_ERR_MSG_TYPE_ERR;
             break;


        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
        case NET_ICMPv4_MSG_TYPE_TS_REQ:
            *p_err = NET_ICMPv4_ERR_MSG_TYPE_REQ;
             break;


        case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
        case NET_ICMPv4_MSG_TYPE_TS_REPLY:
        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY:
            *p_err = NET_ICMPv4_ERR_MSG_TYPE_REPLY;
             break;


        default:                                                /* See Note #8.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvTypeCtr);
            *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
             return;
    }
}


/*
*********************************************************************************************************
*                                         NetICMPv4_RxPktFree()
*
* Description : Free network buffer(s).
*
* Argument(s) : p_buf  Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetICMPv4_RxPktFree (NET_BUF  *p_buf)
{
   (void)NetBuf_FreeBufList((NET_BUF *)p_buf,
                            (NET_CTR *)0);
}


/*
*********************************************************************************************************
*                                      NetICMPv4_RxPktDiscard()
*
* Description : On any ICMPv4 receive error(s), discard ICMPv4 packet(s) & buffer(s).
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetICMPv4_RxPktDiscard (NET_BUF  *p_buf,
                                      NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.ICMPv4.RxPktDiscardedCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBufList((NET_BUF *)p_buf,
                            (NET_CTR *)p_ctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                      NetICMPv4_TxMsgReqHandler()
*
* Description : (1) Transmit ICMPv4 Request Message :
*
*                   (a) Acquire  network lock
*
*                   (b) Validate ICMPv4 Request Message :
*
*                       (1) Validate the following arguments :
*
*                           (A) Type
*                           (B) Code
*                           (C) Source Address
*
*                       (2) Validation of the following arguments deferred to NetIPv4_Tx() :
*
*                           (A) Type of Service (TOS)
*
*                               (1) RFC #1349, Section 5.1 states that "an ICMP request message may
*                                   be sent with any value in the TOS field.  A mechanism to allow
*                                   the user to specify the TOS value to be used would be a useful
*                                   feature in many applications that generate ICMP request messages".
*
*                           (B) Time-to-Live    (TTL)
*                           (C) Destination Address
*                           (D) IP flags
*                           (E) IP options
*
*                       (3) Validation ignores the following arguments :
*
*                           (A) Data
*                           (B) Data length
*
*                   (c) Get buffer for ICMPv4 Request Message :
*
*                       (1) Calculate      ICMPv4 Request Message buffer size
*                       (2) Copy data into ICMPv4 Request Message
*                       (3) Initialize     ICMPv4 Request Message buffer controls
*
*                   (d) Prepare ICMPv4 Request Message :
*
*                       (1) Type                               See 'net_icmpv4.h  ICMPv4 MESSAGE TYPES & CODES'
*                       (2) Code                               See 'net_icmpv4.h  ICMPv4 MESSAGE TYPES & CODES'
*                       (3) Identification (ID)
*                       (4) Sequence Number
*                       (5) Data
*                       (6) Timestamps
*                           (A) Timestamp Request Message
*                               (1) "The Originate Timestamp is the time the sender last touched the message
*                                    before sending it" (RFC #792, Section 'Timestamp or Timestamp Reply Message :
*                                    Description').
*
*                   (e) Transmit ICMPv4 Request Message
*                   (f) Free     ICMPv4 Request Message buffer
*                   (g) Update   ICMPv4 transmit statistics
*                   (h) Release  network lock
*                   (i) Return   ICMPv4 Request Message Identification & Sequence Number
*                         OR
*                       NULL id & sequence number structure, on failure
*
*
*
* Argument(s) : type        ICMPv4 Request Message type (see Note #1d1) :
*                           NET_ICMPv4_MSG_TYPE_ECHO_REQ
*                           NET_ICMPv4_MSG_TYPE_TS_REQ
*                           NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ
*
*               code        ICMPv4 Request Message code (see Note #1d2).
*
*               id          ICMPv4 Request Message id.
*
*               addr_src    Source      IPv4 address.
*
*               addr_dest   Destination IPv4 address.
*
*               TOS         Specific TOS to transmit ICMP/IP packet (see Notes #1b2A
*
*               TTL         Specific TTL to transmit ICMP/IP packet
*
*               flags       Flags to select transmit options; bit-field flags logically OR'd :
*
*               p_opts      Pointer to one or more IPv4 options configuration data structures
*
*               p_data      Pointer to application data to transmit.
*               ------      Argument validated in NetICMPv4_TxEchoReq().
*
*               data_len    Length of application data to transmit (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE             ICMPv4 Request Message successfully transmitted.
*
*
*                               --------------------------- RETURNED BY NetIPv4_Tx() ---------------------------
*                               See NetIPv4_Tx() for additional return error codes.
*
*
*                               --------------------- RETURNED BY NetICMPv4_TxPktDiscard() ---------------------
*                               See NetICMPv4_TxPktDiscard() for additional return error codes.
*
*
* Return(s)   : ICMPv4 Request Message's Identification (ID) & Sequence Numbers, if NO error(s).
*
*               NULL                     Identification (ID) & Sequence Numbers, otherwise.
*
* Caller(s)   : NetICMPv4_TxEchoReq().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (2) NetICMPv4_TxMsgReq() blocked until network initialization completes.
*
*               (3) Default case already invalidated in NetICMPv4_TxMsgReqValidate().  However, the default
*                   case is included as an extra precaution in case 'type' is incorrectly modified.
*
*               (4) Assumes network buffer's protocol header size is large enough to accommodate ICMPv4 header
*                   size (see 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #1').
*
*               (5) Some buffer controls were previously initialized in NetBuf_Get() when the buffer
*                   was allocated earlier in this function.  These buffer controls do NOT need to be
*                   re-initialized but are shown for completeness.
*
*               (6) (a) ICMPv4 message Check-Sum MUST be calculated AFTER the entire ICMPv4 message has been
*                       prepared.  In addition, ALL multi-octet words are converted from host-order to
*                       network-order since "the sum of 16-bit integers can be computed in either byte
*                       order" [RFC #1071, Section 2.(B)].
*
*                   (b) ICMPv4 message Check-Sum field MUST be cleared to '0' BEFORE the ICMPv4 message
*                       Check-Sum is calculated (see RFC #792, Sections 'Echo or Echo Reply Message :
*                       Checksum', 'Timestamp or Timestamp Reply Message : Checksum'; & RFC #950,
*                       Appendix I 'Address Mask ICMP', Section 'ICMP Fields : Checksum').
*
*                   (c) The ICMPv4 message Check-Sum field is returned in network-order & MUST NOT be re-
*                       converted back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumHdrCalc()
*                       Note #3b' & 'net_util.c  NetUtil_16BitOnesCplChkSumDataCalc()  Note #4b').
*
*               (7) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

static  NET_ICMPv4_REQ_ID_SEQ  NetICMPv4_TxMsgReqHandler (CPU_INT08U       type,
                                                          CPU_INT08U       code,
                                                          CPU_INT16U       id,
                                                          NET_IPv4_ADDR    addr_src,
                                                          NET_IPv4_ADDR    addr_dest,
                                                          NET_IPv4_TOS     TOS,
                                                          NET_IPv4_TTL     TTL,
                                                          NET_IPv4_FLAGS   flags,
                                                          void            *p_opts,
                                                          void            *p_data,
                                                          CPU_INT16U       data_len,
                                                          NET_ERR         *p_err)
{
    NET_ICMPv4_REQ_ID_SEQ      id_seq;
    NET_IF_NBR                 if_nbr;
    CPU_INT16U                 msg_size_hdr;
    CPU_INT16U                 msg_size_data;
    CPU_INT16U                 msg_size_tot;
    CPU_INT16U                 msg_seq_nbr;
    CPU_INT16U                 msg_ix;
    CPU_INT16U                 msg_ix_offset;
    CPU_INT16U                 msg_ix_data;
    CPU_INT16U                 msg_chk_sum;
    NET_TS                     ts;
    NET_BUF                   *p_msg_req;
    NET_BUF_HDR               *p_msg_req_hdr;
    NET_ICMPv4_HDR_ECHO       *p_icmp_hdr_echo;
    NET_ICMPv4_HDR_TS         *p_icmp_hdr_ts;
    NET_ICMPv4_HDR_ADDR_MASK  *p_icmp_hdr_addr;
    NET_ERR                    err;


    id_seq.ID     = NET_ICMPv4_REQ_ID_NONE;
    id_seq.SeqNbr = NET_ICMPv4_REQ_SEQ_NONE;

                                                                /* ------------ VALIDATE ICMPv4 TX REQ MSG ------------ */
    NetICMPv4_TxMsgReqValidate(type, code, &err);

    if (err != NET_ICMPv4_ERR_NONE) {
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        return (id_seq);
    }


                                                                /* ------- VALIDATE ICMPv4 TX REQ MSG SRC ADDR -------- */
    if_nbr = NetIPv4_GetAddrHostIF_Nbr(addr_src);
    if (if_nbr == NET_IF_NBR_NONE) {
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        return (id_seq);
    }


                                                                /* -------------- GET ICMPv4 REQ MSG BUF -------------- */
                                                                /* Calc req msg buf size.                               */
    switch (type) {
        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
             msg_size_hdr = NET_ICMPv4_HDR_SIZE_ECHO;

             if (p_data != (void *)0) {
                 msg_size_data = data_len;
             } else {
                 msg_size_data = 0u;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_TS_REQ:
             if (p_data != (void *)0) {
                 NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                        (NET_ERR *)p_err);
                *p_err = NET_ICMPv4_ERR_INVALID_ARG;
                 return (id_seq);
             }

             if (data_len > 0) {
                 NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                        (NET_ERR *)p_err);
                *p_err = NET_ICMPv4_ERR_INVALID_ARG;
                 return (id_seq);
             }

             msg_size_hdr  = NET_ICMPv4_HDR_SIZE_TS;
             msg_size_data = 0u;
             break;


        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ:
             if (p_data != (void *)0) {
                 NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                        (NET_ERR *)p_err);
                *p_err = NET_ICMPv4_ERR_INVALID_ARG;
                 return (id_seq);
             }

             if (data_len > 0) {
                 NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                        (NET_ERR *)p_err);
                *p_err = NET_ICMPv4_ERR_INVALID_ARG;
                 return (id_seq);
             }

             msg_size_hdr  = NET_ICMPv4_HDR_SIZE_ADDR_MASK;
             msg_size_data = 0u;
             break;


        default:                                                /* See Note #3.                                         */
             NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                  (NET_ERR *)p_err);
             return (id_seq);
    }

    msg_size_tot = msg_size_hdr + msg_size_data;


#if 0
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (NET_BUF_DATA_IX_TX < msg_size_hdr) {                    /* See Note #4.                                         */
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxInvalidBufIxCtr);
        return (id_seq);
    }
#endif
#endif


                                                                /* Get req msg buf.                                     */
#if 0
    msg_ix   = NET_BUF_DATA_IX_TX - msg_size_hdr;
#else
    msg_ix = 0u;
    NetICMPv4_TxIxDataGet(if_nbr,
                          msg_size_tot,
                         &msg_ix,
                         &err);
#endif

    p_msg_req = NetBuf_Get((NET_IF_NBR     ) if_nbr,
                          (NET_TRANSACTION) NET_TRANSACTION_TX,
                          (NET_BUF_SIZE   ) msg_size_tot,
                          (NET_BUF_SIZE   ) msg_ix,
                          (NET_BUF_SIZE  *)&msg_ix_offset,
                          (NET_BUF_FLAGS  ) NET_BUF_FLAG_NONE,
                          (NET_ERR       *)&err);
    if (err != NET_BUF_ERR_NONE) {
        NetICMPv4_TxPktDiscard(p_msg_req, p_err);
        return (id_seq);
    }

    msg_ix      += msg_ix_offset;
    msg_ix_data  = msg_ix + msg_size_hdr;

                                                                /* Init req msg buf ctrls.                              */
    p_msg_req_hdr                        = &p_msg_req->Hdr;
    p_msg_req_hdr->ICMP_MsgIx            = (CPU_INT16U  )msg_ix;
    p_msg_req_hdr->ICMP_MsgLen           = (CPU_INT16U  )msg_size_tot;
    p_msg_req_hdr->ICMP_HdrLen           = (CPU_INT16U  )msg_size_hdr;
    p_msg_req_hdr->TotLen                = (NET_BUF_SIZE)p_msg_req_hdr->ICMP_MsgLen;
    p_msg_req_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_ICMP_V4;
    p_msg_req_hdr->ProtocolHdrTypeNetSub =  NET_PROTOCOL_TYPE_ICMP_V4;


    if (msg_size_data > 0) {                                    /* Copy data into ICMPv4 req tx buf.                    */
        NetBuf_DataWr((NET_BUF    *) p_msg_req,
                      (NET_BUF_SIZE) msg_ix_data,
                      (NET_BUF_SIZE) msg_size_data,
                      (CPU_INT08U *) p_data,
                      (NET_ERR    *)&err);
        if (err != NET_BUF_ERR_NONE) {
            NetICMPv4_TxPktDiscard(p_msg_req, p_err);
            return (id_seq);
        }

        p_msg_req_hdr->DataIx  = (CPU_INT16U  )msg_ix_data;
        p_msg_req_hdr->DataLen = (NET_BUF_SIZE)msg_size_data;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #5].                */
    } else {
        p_msg_req_hdr->DataIx  =  NET_BUF_IX_NONE;
        p_msg_req_hdr->DataLen =  0u;
#endif
    }


                                                                /* --------------- PREPARE ICMP REQ MSG --------------- */
    NET_ICMPv4_TX_GET_SEQ_NBR(msg_seq_nbr);

    switch (type) {
        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
             p_icmp_hdr_echo       = (NET_ICMPv4_HDR_ECHO *)&p_msg_req->DataPtr[p_msg_req_hdr->ICMP_MsgIx];
             p_icmp_hdr_echo->Type =  NET_ICMPv4_MSG_TYPE_ECHO_REQ;
             p_icmp_hdr_echo->Code =  NET_ICMPv4_MSG_CODE_ECHO_REQ;
             NET_UTIL_VAL_COPY_SET_NET_16(&p_icmp_hdr_echo->ID,     &id);
             NET_UTIL_VAL_COPY_SET_NET_16(&p_icmp_hdr_echo->SeqNbr, &msg_seq_nbr);
                                                                            /* Calc ICMPv4 msg chk sum (see Note #6).   */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_echo->ChkSum, 0x0000u);    /* Clr             chk sum (see Note #6b).  */
#ifdef NET_ICMP_CFG_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumDataCalc((void     *) p_msg_req,
                                                              (void     *) 0,
                                                              (CPU_INT16U) 0u,
                                                              (NET_ERR  *)&err);
#endif
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_echo->ChkSum, &msg_chk_sum);  /* Copy chk sum in net order (see Note #6c).*/
             break;


        case NET_ICMPv4_MSG_TYPE_TS_REQ:
             p_icmp_hdr_ts       = (NET_ICMPv4_HDR_TS   *)&p_msg_req->DataPtr[p_msg_req_hdr->ICMP_MsgIx];
             p_icmp_hdr_ts->Type =  NET_ICMPv4_MSG_TYPE_TS_REQ;
             p_icmp_hdr_ts->Code =  NET_ICMPv4_MSG_CODE_TS_REQ;
             ts                 =  NetUtil_TS_Get();                        /* See Note #1d6A1.                         */
             NET_UTIL_VAL_COPY_SET_NET_16(&p_icmp_hdr_ts->ID,           &id);
             NET_UTIL_VAL_COPY_SET_NET_16(&p_icmp_hdr_ts->SeqNbr,       &msg_seq_nbr);
             NET_UTIL_VAL_COPY_SET_NET_32(&p_icmp_hdr_ts->TS_Originate, &ts);
             NET_UTIL_VAL_SET_NET_32(&p_icmp_hdr_ts->TS_Rx,  NET_TS_NONE);
             NET_UTIL_VAL_SET_NET_32(&p_icmp_hdr_ts->TS_Tx,  NET_TS_NONE);
                                                                            /* Calc ICMPv4 msg chk sum (see Note #6).   */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_ts->ChkSum, 0x0000u);      /* Clr             chk sum (see Note #6b).  */
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *) p_icmp_hdr_ts,
                                                             (CPU_INT16U) p_msg_req_hdr->ICMP_MsgLen,
                                                             (NET_ERR  *)&err);
#endif
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_ts->ChkSum, &msg_chk_sum);    /* Copy chk sum in net order (see Note #6c).*/
             break;


        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ:
             p_icmp_hdr_addr       = (NET_ICMPv4_HDR_ADDR_MASK *)&p_msg_req->DataPtr[p_msg_req_hdr->ICMP_MsgIx];
             p_icmp_hdr_addr->Type =  NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ;
             p_icmp_hdr_addr->Code =  NET_ICMPv4_MSG_CODE_ADDR_MASK_REQ;
             NET_UTIL_VAL_COPY_SET_NET_16(&p_icmp_hdr_addr->ID,     &id);
             NET_UTIL_VAL_COPY_SET_NET_16(&p_icmp_hdr_addr->SeqNbr, &msg_seq_nbr);
             NET_UTIL_VAL_SET_NET_32(&p_icmp_hdr_addr->AddrMask, NET_IPv4_ADDR_NONE);
                                                                            /* Calc ICMPv4 msg chk sum (see Note #6).   */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_addr->ChkSum,   0x0000u);  /* Clr             chk sum (see Note #6b).  */
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *) p_icmp_hdr_addr,
                                                             (CPU_INT16U) p_msg_req_hdr->ICMP_MsgLen,
                                                             (NET_ERR  *)&err);
#endif
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_addr->ChkSum, &msg_chk_sum);  /* Copy chk sum in net order (see Note #6c).*/
             break;


        default:                                                /* See Note #3.                                         */
             NetICMPv4_TxPktDiscard(p_msg_req, p_err);
             return (id_seq);
    }

    if (err != NET_UTIL_ERR_NONE) {                             /* Chk err from NetUtil_16BitOnesCplChkSum()'s.         */
        NetICMPv4_TxPktDiscard(p_msg_req, p_err);
        return (id_seq);
    }



                                                                /* ---------------- TX ICMPv4 REQ MSG ----------------- */
    NetIPv4_Tx((NET_BUF      *)p_msg_req,
               (NET_IPv4_ADDR )addr_src,
               (NET_IPv4_ADDR )addr_dest,
               (NET_IPv4_TOS  )TOS,
               (NET_IPv4_TTL  )TTL,
               (NET_IPv4_FLAGS)flags,
               (void         *)p_opts,
               (NET_ERR      *)p_err);



                                                                /* ------ FREE ICMPv4 REQ MSG / UPDATE TX STATS ------- */
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             NetICMPv4_TxPktFree(p_msg_req);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.TxMsgCtr);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.TxMsgReqCtr);
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #7.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr);
                                                                /* Rtn err from NetIPv4_Tx().                           */
             return (id_seq);


        case NET_IPv4_ERR_TX_PKT:
                                                                /* See Note #7.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr);
            *p_err =  NET_ERR_TX;
             return (id_seq);


        default:
             NetICMPv4_TxPktDiscard(p_msg_req, p_err);
             return (id_seq);
    }


                                                                /* ---------- RTN ICMPv4 REQ MSG ID/SEQ NBR ----------- */
    id_seq.ID     = id;
    id_seq.SeqNbr = msg_seq_nbr;

   *p_err          = NET_ICMPv4_ERR_NONE;

    return (id_seq);
}


/*
*********************************************************************************************************
*                                     NetICMPv4_TxMsgErrValidate()
*
* Description : (1) Validate received packet & transmit error parameters for ICMPv4 Error Message transmit :
*
*                   (a) RFC #1122, Section 3.2.2 specifies that "an ICMP error message MUST NOT be sent as
*                       the result of receiving" :
*
*                       (1) "An ICMP Error Message",                                          ...
*
*                       (2) "A datagram destined to an IP broadcast or IP multicast address", ...
*
*                           (A) Any packet received as an IP broadcast destination address MUST also have
*                               been received as a link-layer broadcast (see RFC #1122, Section 3.3.6 &
*                               'net_ip.c  NetIPv4_RxPktValidate()  Note #9d3B1a').
*
*                           (B) Therefore, it is NOT necessary to re-validate the IP destination address
*                               as a non-broadcast address since it has ALREADY been validated as a non-
*                               broadcast at the link-layer (see Note #1a3).
*
*                       (3) "A datagram sent as a link-layer broadcast",                      ...
*
*                       (4) "A non-initial fragment",                                         ...
*
*                       (5) "A datagram whose source address does not define a single host -- e.g., a zero
*                            address, a loopback address, a broadcast address, a multicast address, or a
*                            Class E address"
*
*                           (A) ALL IPv4 source addresses already validated (see 'net_ip.c  NetIPv4_RxPktValidate()
*                               Note #9c') except 'This Host' & 'Specified Host' addresses.
*
*                   (b) Validate ICMPv4 Error Message transmit parameters :
*
*                       (1) Type
*                       (2) Code
*                       (3) Pointer                                         See Note #6
*
*
* Argument(s) : p_buf       Pointer to network buffer that received a packet with error(s).
*               -----       Argument checked   in NetICMPv4_TxMsgErr().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetICMPv4_TxMsgErr().
*
*               p_ip_hdr    Pointer to received packet's IP header.
*               --------    Argument validated in NetICMPv4_TxMsgErr().
*
*               type        ICMPv4 Error Message type (see Note #5) :
*                               NET_ICMPv4_MSG_TYPE_DEST_UNREACH
*                               NET_ICMPv4_MSG_TYPE_TIME_EXCEED
*                               NET_ICMPv4_MSG_TYPE_PARAM_PROB
*
*               code        ICMPv4 Error Message code (see Note #5).
*
*               ptr         Pointer to received packet's ICMPv4 error (optional).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE                   Received packet successfully validated
*                                                                         for ICMPv4 transmit error message.
*
*                               NET_ICMPv4_ERR_TX_INVALID_MCAST   Packet received as a multicast packet.
*                               NET_ICMPv4_ERR_TX_INVALID_BROADCAST   Packet received as a broadcast packet.
*                               NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC    Packet received with a non-single host
*                                                                         IPv4 source address.
*                               NET_ICMPv4_ERR_TX_INVALID_FRAG        Packet received as a non-initial fragment.
*                               NET_ICMPv4_ERR_TX_INVALID_ERR_MSG     Packet received as an ICMP Error Message.
*                               NET_ICMPv4_ERR_INVALID_TYPE           Invalid/unknown ICMPv4 message type.
*                               NET_ICMPv4_ERR_INVALID_CODE           Invalid/unknown ICMPv4 message code.
*                               NET_ICMPv4_ERR_INVALID_PTR            Invalid message pointer outside error message.
*                               NET_BUF_ERR_INVALID_IX                Invalid/insufficient buffer index.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgErr().
*
* Note(s)     : (2) The following IPv4 header fields MUST be converted from network-order to host-order BEFORE
*                   any transmit ICMPv4 Error Messages are validated :
*
*                   (a) Total  Length                       See 'net_ipv4.c  NetIPv4_RxPktValidate()  Note #3b'
*                   (b) Source Address                      See 'net_ipv4.c  NetIPv4_RxPktValidate()  Note #3c'
*
*               (3) See 'net_ip.h  IP ADDRESS DEFINES  Notes #2 & #3' for supported IP addresses.
*
*               (4) Default case already invalidated in NetIPv4_RxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'IP_AddrSrc' is incorrectly modified.
*
*               (5) See 'net_icmpv4.h  ICMPv4 MESSAGE TYPES & CODES' for supported ICMPv4 message types & codes.
*
*               (6) (a) ICMPv4 Parameter Problem Messages' pointer fields validated by comparing the pointer
*                       field value to minimum & maximum pointer field values.
*
*                   (b) Since an ICMPv4 Parameter Problem Message's minimum pointer field value is NOT less
*                       than zero (see 'net_icmpv4.h  ICMPv4 POINTER DEFINES'), a minimum pointer field value
*                       check is NOT required unless native data type 'CPU_INT16U' is incorrectly configured
*                       as a signed integer in 'cpu.h'.
*
*                   (c) Since an ICMPv4 Parameter Problem Message may be received for an IPv4 or higher-layer
*                       protocol, the maximum pointer field value is specific to each received ICMPv4 packets'
*                       IPv4 header length & demultiplexed protocol header length :
*
*                           Pointer Field Value < Maximum Pointer Field Value = IPv4 Header Length
*                                                                           [ + Other Protocol Header Length ]
*
*                   (d) See 'net_icmpv4.h  ICMP POINTER DEFINES  Note #2' for supported ICMPv4 Parameter Problem
*                       Message protocols.
*********************************************************************************************************
*/

static  void  NetICMPv4_TxMsgErrValidate (NET_BUF       *p_buf,
                                          NET_BUF_HDR   *p_buf_hdr,
                                          NET_IPv4_HDR  *p_ip_hdr,
                                          CPU_INT08U     type,
                                          CPU_INT08U     code,
                                          CPU_INT08U     ptr,
                                          NET_ERR       *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT16U       ptr_max;
    CPU_INT16U       ptr_max_protocol;
#endif
#if 0                                                           /* See Note #1a2B.                                      */
    CPU_BOOLEAN      addr_broadcast;
#endif
    CPU_BOOLEAN      addr_multicast;
    CPU_BOOLEAN      rx_broadcast;
    CPU_BOOLEAN      ip_flag_frags_more;
    CPU_INT16U       ip_flags;
    CPU_INT16U       ip_frag_offset;
    NET_ICMPv4_HDR  *p_icmp_hdr;



                                                                /* ------------- CHK LINK-LAYER BROADCAST ------------- */
    rx_broadcast = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_BROADCAST);
    if (rx_broadcast != DEF_NO) {                               /* If pkt rx'd via broadcast, ...                       */
       *p_err = NET_ICMPv4_ERR_TX_INVALID_BROADCAST;            /* ... rtn err (see Note #1a3).                         */
        return;
    }


                                                                /* ---------------- CHK IPv4 DEST ADDR ---------------- */
#if 0                                                           /* See Note #1a2B.                                      */
    addr_broadcast = NetIPv4_IsAddrBroadcast(p_buf_hdr->IP_AddrDest);
    if (addr_broadcast != DEF_NO) {                             /* If pkt rx'd via broadcast, ...                       */
       *p_err = NET_ICMPv4_ERR_TX_INVALID_BROADCAST;            /* ... rtn err (see Note #1a2).                         */
        return;
    }
#endif
    addr_multicast = NetIPv4_IsAddrMulticast(p_buf_hdr->IP_AddrDest);
    if (addr_multicast != DEF_NO) {                             /* If pkt rx'd via multicast, ...                       */
       *p_err = NET_ICMPv4_ERR_TX_INVALID_MCAST;            /* ... rtn err (see Note #1a2).                         */
        return;
    }


                                                                /* ---------------- CHK IPv4 SRC  ADDR ---------------- */
                                                                /* See Note #1a5.                                       */
    if (p_buf_hdr->IP_AddrSrc == NET_IPv4_ADDR_THIS_HOST) {     /* Chk invalid         'This Host' (see Note #1a5A).    */
       *p_err = NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC;
        return;
    }

    if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
                                                                /* Chk invalid Class-A 'This Host' (see Note #1a5A).    */
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
            *p_err = NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC;
             return;
        }

    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
                                                                /* Chk invalid Class-B 'This Host' (see Note #1a5A).    */
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
            *p_err = NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC;
             return;
        }

    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
                                                                /* Chk invalid Class-C 'This Host' (see Note #1a5A).    */
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
            *p_err = NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC;
             return;
        }

    } else {                                                    /* Discard invalid addr class (see Notes #3 & #4).      */
       *p_err = NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC;
        return;
    }


                                                                /* ------------------ CHK IPv4 FRAG ------------------- */

    ip_flags           = p_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FLAG_MASK;
    ip_flag_frags_more = DEF_BIT_IS_SET(ip_flags, NET_IPv4_HDR_FLAG_FRAG_MORE);
    if (ip_flag_frags_more != DEF_NO) {                         /* If 'More Frags' flag set ...                         */
        ip_frag_offset = p_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FRAG_OFFSET_MASK;
        if (ip_frag_offset != NET_IPv4_HDR_FRAG_OFFSET_NONE) {  /* ... & frag offset != 0,  ...                         */
           *p_err = NET_ICMPv4_ERR_TX_INVALID_FRAG;             /* ... rtn err for non-initial frag (see Note #1a4).    */
            return;
        }
    }



                                                                /* ---------------- CHK ICMPv4 ERR MSG ---------------- */
    if (p_ip_hdr->Protocol == NET_IP_HDR_PROTOCOL_ICMP) {       /* If rx'd IP datagram is ICMP, ...                     */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (p_buf_hdr->ICMP_MsgIx == NET_BUF_IX_NONE) {
            NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.RxInvalidBufIxCtr);
           *p_err = NET_BUF_ERR_INVALID_IX;
            return;
        }
#endif
        p_icmp_hdr = (NET_ICMPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->ICMP_MsgIx];

        switch (p_icmp_hdr->Type) {                             /* ... chk ICMPv4 msg type & ...                        */
            case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
            case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
            case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
                *p_err = NET_ICMPv4_ERR_TX_INVALID_ERR_MSG;     /* ... rtn err for ICMPv4 err msgs (see Note #1a1).     */
                 return;


            case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
            case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
            case NET_ICMPv4_MSG_TYPE_TS_REQ:
            case NET_ICMPv4_MSG_TYPE_TS_REPLY:
            case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY:
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                 ptr_max_protocol = p_buf_hdr->ICMP_MsgLen;     /* See Note #6c.                                        */
#endif
                 break;

#if 0                                                           /* ---------------- UNSUPPORTED TYPES ----------------- */
                                                                /* See Note #5.                                         */
            case NET_ICMPv4_MSG_TYPE_REDIRECT:
            case NET_ICMPv4_MSG_TYPE_ROUTE_REQ:
            case NET_ICMPv4_MSG_TYPE_ROUTE_AD:
            case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ:
#endif
            default:                                            /* ------------------ INVALID TYPES ------------------- */
                *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
                 return;
        }


    } else {                                                    /* See Note #6d.                                        */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        ptr_max_protocol = 0u;
#endif
    }



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------- VALIDATE ICMP ERR MSG TYPE/CODE/PTR -------- */
    switch (type) {
        case NET_ICMPv4_MSG_TYPE_DEST_UNREACH:
             switch (code) {
                 case NET_ICMPv4_MSG_CODE_DEST_PROTOCOL:
                 case NET_ICMPv4_MSG_CODE_DEST_PORT:
                      break;


                 case NET_ICMPv4_MSG_CODE_DEST_NET:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST:
                 case NET_ICMPv4_MSG_CODE_DEST_FRAG_NEEDED:
                 case NET_ICMPv4_MSG_CODE_DEST_ROUTE_FAIL:
                 case NET_ICMPv4_MSG_CODE_DEST_NET_UNKNOWN:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST_UNKNOWN:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST_ISOLATED:
                 case NET_ICMPv4_MSG_CODE_DEST_NET_TOS:
                 case NET_ICMPv4_MSG_CODE_DEST_HOST_TOS:
                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_TIME_EXCEED:
             switch (code) {
                 case NET_ICMPv4_MSG_CODE_TIME_EXCEED_TTL:
                 case NET_ICMPv4_MSG_CODE_TIME_EXCEED_FRAG_REASM:
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_PARAM_PROB:
             switch (code) {
                 case NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR:
                 case NET_ICMPv4_MSG_CODE_PARAM_PROB_OPT_MISSING:
                                                                /* Validate ICMPv4 Param Prob Msg ptr (see Note #6).    */
#if 0                                                           /* See Note #6b.                                        */
                      if (ptr < NET_ICMPv4_PTR_IX_BASE) {
                          NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrPtrCtr);
                         *p_err = NET_ICMPv4_ERR_INVALID_PTR;
                      }
#endif
                      ptr_max = p_buf_hdr->IP_HdrLen + ptr_max_protocol;
                      if (ptr >= ptr_max) {
                          NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrPtrCtr);
                         *p_err = NET_ICMPv4_ERR_INVALID_PTR;
                      }
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
        case NET_ICMPv4_MSG_TYPE_ECHO_REPLY:
        case NET_ICMPv4_MSG_TYPE_TS_REQ:
        case NET_ICMPv4_MSG_TYPE_TS_REPLY:
        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY:
            *p_err = NET_ICMPv4_ERR_TX_INVALID_ERR_MSG;
             return;

#if 0                                                           /* ---------------- UNSUPPORTED TYPES ----------------- */
                                                                /* See Note #5.                                         */
        case NET_ICMPv4_MSG_TYPE_REDIRECT:
        case NET_ICMPv4_MSG_TYPE_ROUTE_REQ:
        case NET_ICMPv4_MSG_TYPE_ROUTE_AD:
        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ:
#endif
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrTypeCtr);
            *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
             return;
    }

#else                                                           /* Prevent 'variable unused' compiler warnings.         */
   (void)&type;
   (void)&code;
   (void)&ptr;
#endif


   *p_err = NET_ICMPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetICMPv4_TxMsgReqValidate()
*
* Description : (1) Validate parameters for ICMPv4 Request Message transmit :
*
*                   (a) Type                                                        See Note #2
*                   (b) Code                                                        See Note #2
*
*
* Argument(s) : type        ICMPv4 Request Message type (see Note #1a) :
*
*                               NET_ICMPv4_MSG_TYPE_ECHO_REQ
*                               NET_ICMPv4_MSG_TYPE_TS_REQ
*                               NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ
*
*               code        ICMPv4 Request Message code (see Note #1b).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE                 Transmit parameters successfully validated
*                                                                       for ICMPv4 transmit request message.
*                               NET_ICMPv4_ERR_INVALID_TYPE         Invalid/unknown ICMPv4 message type.
*                               NET_ICMv6P_ERR_INVALID_CODE         Invalid/unknown ICMPv4 message code.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgReqHandler().
*
* Note(s)     : (2) See 'net_icmpv4.h  ICMPv4 MESSAGE TYPES & CODES' for supported ICMPv4 message types & codes.
*********************************************************************************************************
*/

static  void  NetICMPv4_TxMsgReqValidate (CPU_INT08U   type,
                                          CPU_INT08U   code,
                                          NET_ERR     *p_err)
{

                                                                /* -------- VALIDATE ICMPv4 REQ MSG TYPE/CODE --------- */
    switch (type) {
        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
             switch (code) {
                 case NET_ICMPv4_MSG_CODE_ECHO_REQ:
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_TS_REQ:
             switch (code) {
                 case NET_ICMPv4_MSG_CODE_TS_REQ:
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        case NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ:
             switch (code) {
                 case NET_ICMPv4_MSG_CODE_ADDR_MASK_REQ:
                      break;


                 default:
                      NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrCodeCtr);
                     *p_err = NET_ICMPv4_ERR_INVALID_CODE;
                      return;
             }
             break;


        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxHdrTypeCtr);
            *p_err = NET_ICMPv4_ERR_INVALID_TYPE;
             return;
    }


   *p_err = NET_ICMPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetICMPv4_TxMsgReply()
*
* Description : (1) Transmit ICMPv4 Reply Message in response to received ICMPv4 Request Message :
*
*                   (a) Process ICMPv4 Reply Message :
*
*                       (1) The following ICMPv4 Reply Messages require receive processing :
*
*                           (A) Timestamp Reply Message
*
*                               (1) "The Receive Timestamp is the time the echoer first touched it on receipt"
*                                   (RFC #792, Section 'Timestamp or Timestamp Reply Message : Description').
*
*                   (b) Get buffer for ICMPv4 Reply Message :
*
*                       (1) Copy ICMPv4 Request Message into ICMPv4 Reply Message
*                       (2) Initialize ICMPv4 Reply Message buffer controls
*
*                   (c) Prepare ICMPv4 Reply Message's IPv4 header :
*
*                       (1) RFC #1349, Section 5.1 specifies that "an ICMP reply message is sent with the same
*                           value in the TOS field as was used in the corresponding ICMP request message".
*
*                       (2) RFC #1122, Sections 3.2.2.6 & 3.2.2.8 specify that "if a Record Route and/or Time
*                           Stamp option is received in [an ICMP Request, these options] SHOULD be updated ...
*                           and included in the IP header of the ... Reply message".  Also "if a Source Route
*                           option is received ... the return route MUST be reversed and used as a Source Route
*                           option for the ... Reply message".
*
*                           #### These IPv4 header option requirements for ICMPv4 Reply Messages are NOT yet implemented.
*
*                   (d) Prepare ICMPv4 Reply Message :
*
*                       (1) Echo Reply Message
*
*                           (A) "To form an echo reply message, the source and destination addresses are simply
*                                reversed, the type code changed to [reply], and the checksum recomputed"
*                               (RFC #792, Section 'Echo or Echo Reply Message : Addresses').
*
*                       (2) Timestamp Reply Message
*
*                           (A) "The Transmit Timestamp is the time the echoer last touched the message on sending
*                                it" (RFC #792, Section 'Timestamp or Timestamp Reply Message : Description').
*
*                           (B) "To form a timestamp reply message, the source and destination addresses are
*                                simply reversed, the type code changed to [reply], and the checksum recomputed"
*                                (RFC #792, Section 'Timestamp or Timestamp Reply Message : Addresses').
*
*                       (3) Some ICMPv4 Reply Message fields are copied directly from the ICMPv4 Request Message.
*
*                           (A) ICMPv4 Reply Message Identification & Sequence Number fields were NOT validated
*                               or converted from network-order to host-order (see 'NetICMPv4_RxPktValidate()
*                               Notes #1b2C & #1b2D') & therefore do NOT need to be converted from host-order
*                               to network-order.
*
*                   (e) Transmit ICMPv4 Reply Message
*                   (f) Free     ICMPv4 Reply Message buffer
*                   (g) Update transmit statistics
*
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMP packet.
*               -----       Argument checked   in NetICMPv4_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetICMPv4_Rx().
*
*               p_icmp_hdr  Pointer to received packet's ICMP header.
*               ----------  Argument validated in NetICMPv4_Rx()/NetICMPv4_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE             ICMPv4 Reply Message successfully transmitted.
*
*                                                               -- RETURNED BY NetICMPv4_TxPktDiscard() : --
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               -------- RETURNED BY NetIPv4_Tx() : --------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_Rx().
*
* Note(s)     : (2) Default case already invalidated in NetICMPv4_RxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'Type' is incorrectly modified.
*
*               (3) (a) RFC #1122, Section 3.2.2.6 states that "data received in an ICMP Echo Request MUST
*                       be entirely included in the resulting Echo Reply.  However, if sending the Echo Reply
*                       requires intentional fragmentation that is not implemented, the datagram MUST be
*                       truncated to maximum transmission size ... and sent".
*
*                       See also 'net_ipv4.h  Note #1c'.
*
*                   (b) In case the maximum network buffer size is smaller than the ICMPv4 message data length,
*                       the ICMPv4 Echo Request Message data should be similarly truncated in order to transmit
*                       the ICMPv4 Echo Reply Message.
*
*               (4) Some buffer controls were previously initialized in NetBuf_Get() when the buffer
*                   was allocated earlier in this function.  These buffer controls do NOT need to be
*                   re-initialized but are shown for completeness.
*
*               (5) (a) ICMPv4 message Check-Sum MUST be calculated AFTER the entire ICMP message has been
*                       prepared.  In addition, ALL multi-octet words are converted from host-order to
*                       network-order since "the sum of 16-bit integers can be computed in either byte
*                       order" [RFC #1071, Section 2.(B)].
*
*                   (b) ICMPv4 message Check-Sum field MUST be cleared to '0' BEFORE the ICMPv4 message
*                       Check-Sum is calculated (see RFC #792, Sections 'Echo or Echo Reply Message :
*                       Checksum' & 'Timestamp or Timestamp Reply Message : Checksum').
*
*                   (c) The ICMPv4 message Check-Sum field is returned in network-order & MUST NOT be re-
*                       converted back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumHdrCalc()
*                       Note #3b' & 'net_util.c  NetUtil_16BitOnesCplChkSumDataCalc()  Note #4b').
*
*               (6) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetICMPv4_TxReqReply (NET_BUF         *p_buf,
                                    NET_BUF_HDR     *p_buf_hdr,
                                    NET_ICMPv4_HDR  *p_icmp_hdr,
                                    NET_ERR         *p_err)
{
#if 0                                                           /* DISABLED while NOT yet implemented.                  */
    NET_IPv4_OPT_CFG_ROUTE_TS   msg_opt_route_ts;
#endif
    NET_ICMPv4_HDR_ECHO          *p_icmp_hdr_echo;
    NET_ICMPv4_HDR_TS            *p_icmp_hdr_ts;
    NET_BUF                      *p_msg_req;
    NET_BUF                      *p_msg_reply;
    NET_BUF_HDR                  *p_msg_req_hdr;
    NET_BUF_HDR                  *p_msg_reply_hdr;
    NET_IPv4_OPT_CFG_ROUTE_TS    *p_msg_opt_cfg_route_ts;
    NET_IPv4_HDR                 *p_ip_hdr;
    NET_BUF_SIZE                  buf_size_max;
    NET_BUF_SIZE                  buf_size_max_data;
    NET_MTU                       icmp_mtu;
    CPU_INT16U                    msg_size_hdr;
    CPU_INT16U                    msg_len;
    CPU_INT16U                    msg_len_min;
    CPU_INT16U                    msg_len_rem;
    CPU_INT16U                    msg_ix;
    CPU_INT16U                    msg_ix_offset;
    CPU_INT16U                    msg_reply_ix;
    CPU_INT16U                    msg_reply_len;
    CPU_INT16U                    msg_chk_sum;
    CPU_INT08U                   *p_msg_req_data;
    NET_TS                        ts;
    NET_IPv4_TOS                  TOS;
    NET_IPv4_FLAGS                flags;
    NET_ERR                       err;



                                                                /* ------------ PROCESS ICMPv4 REPLY MSGs ------------- */
    switch (p_icmp_hdr->Type) {
        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:
             msg_size_hdr =  NET_ICMPv4_HDR_SIZE_ECHO;
             msg_len_min  =  NET_ICMPv4_MSG_LEN_MIN_ECHO;
             break;


        case NET_ICMPv4_MSG_TYPE_TS_REQ:
             p_icmp_hdr_ts = (NET_ICMPv4_HDR_TS *)p_icmp_hdr;
             ts           =  NetUtil_TS_Get();                  /* See Note #1a1A1.                                     */
             NET_UTIL_VAL_COPY_SET_NET_32(&p_icmp_hdr_ts->TS_Rx, &ts);
             msg_size_hdr =  NET_ICMPv4_HDR_SIZE_TS;
             msg_len_min  =  NET_ICMPv4_MSG_LEN_MIN_TS;
             break;


        default:                                                /* See Note #2.                                         */
             NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                    (NET_ERR *)p_err);
             return;
    }



                                                                /* ------------- GET ICMPv4 REPLY MSG BUF ------------- */
    msg_len  = p_buf_hdr->ICMP_MsgLen;                          /* Adj req msg len for reply msg len.                   */
                                                                /* Get IF's ICMPv4 MTU.                                 */
    icmp_mtu = NetIF_MTU_GetProtocol(p_buf_hdr->IF_Nbr, NET_PROTOCOL_TYPE_ICMP_V4, NET_IF_FLAG_NONE, &err);
    if (err != NET_IF_ERR_NONE) {
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        return;
    }

#if 0
    msg_ix   = NET_BUF_DATA_IX_TX - msg_size_hdr;
#else
    msg_ix = 0u;
    NetICMPv4_TxIxDataGet(p_buf_hdr->IF_Nbr,
                          msg_len,
                         &msg_ix,
                         &err);
#endif

    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )p_buf_hdr->IF_Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_TX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )msg_ix);

    buf_size_max_data = (NET_BUF_SIZE)DEF_MIN(buf_size_max, icmp_mtu);

    if (msg_len > buf_size_max_data) {                          /* If msg len > max data size, ...                      */
        if (p_icmp_hdr->Type != NET_ICMPv4_MSG_TYPE_ECHO_REQ) { /* ... for Echo Req Msg ONLY,  ...                      */
            NetICMPv4_TxPktDiscard((NET_BUF *)0,
                                   (NET_ERR *)p_err);
            return;
        }
        msg_len = (CPU_INT16U)buf_size_max_data;                /* ... truncate msg len (see Note #3).                  */
    }

    if (msg_len < msg_len_min) {                                /* If msg len < min msg len, rtn err.                   */
        NetICMPv4_TxPktDiscard((NET_BUF *)0,
                               (NET_ERR *)p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxInvalidLenCtr);
        return;
    }

                                                                /* Get reply msg buf.                                   */
    p_msg_reply = NetBuf_Get(p_buf_hdr->IF_Nbr,
                             NET_TRANSACTION_TX,
                             msg_len,
                             msg_ix,
                            &msg_ix_offset,
                             NET_BUF_FLAG_NONE,
                            &err);
    if (err != NET_BUF_ERR_NONE) {
        NetICMPv4_TxPktDiscard(p_msg_reply, p_err);
        return;
    }

    msg_ix       += msg_ix_offset;
    msg_reply_ix  = msg_ix;
    msg_len_rem   = msg_len;

    p_msg_req = p_buf;

    while ((p_msg_req != (NET_BUF *)0) && (msg_len_rem > 0)) {  /* For ALL ICMPv4 req msg pkt bufs, ...                 */
                                                                /* ... copy rx'd ICMPv4 req msg into ICMP reply tx buf. */
        p_msg_req_hdr  = &p_msg_req->Hdr;
        p_msg_req_data = &p_msg_req->DataPtr[p_msg_req_hdr->ICMP_MsgIx];
        msg_reply_len  =  p_msg_req_hdr->IP_DataLen;             /* Each pkt buf's IP data is ICMPv4 req msg data.       */
        if (msg_reply_len > msg_len_rem) {                      /* If req msg pkt buf data len > rem msg len, ...       */
            msg_reply_len = msg_len_rem;                        /* ... truncate req msg pkt buf data len.               */
        }

        NetBuf_DataWr((NET_BUF    *) p_msg_reply,
                      (NET_BUF_SIZE) msg_reply_ix,
                      (NET_BUF_SIZE) msg_reply_len,
                      (CPU_INT08U *) p_msg_req_data,
                      (NET_ERR    *)&err);
        if (err != NET_BUF_ERR_NONE) {
            NetICMPv4_TxPktDiscard(p_msg_reply, p_err);
            return;
        }

        msg_reply_ix += msg_reply_len;
        msg_len_rem  -= msg_reply_len;

        p_msg_req      = p_msg_req_hdr->NextBufPtr;
    }

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (msg_len_rem > 0) {                                      /* If entire ICMPv4 req msg NOT copied, rtn err.        */
        NetICMPv4_TxPktDiscard(p_msg_reply, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxInvalidLenCtr);
        return;
    }
#endif

                                                                /* Init reply msg buf ctrls.                            */
    p_msg_reply_hdr                        = &p_msg_reply->Hdr;
    p_msg_reply_hdr->ICMP_MsgIx            = (CPU_INT16U  )msg_ix;
    p_msg_reply_hdr->ICMP_MsgLen           = (CPU_INT16U  )msg_len;
    p_msg_reply_hdr->ICMP_HdrLen           = (CPU_INT16U  )msg_size_hdr;
    p_msg_reply_hdr->TotLen                = (NET_BUF_SIZE)p_msg_reply_hdr->ICMP_MsgLen;
    p_msg_reply_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_ICMP_V4;
    p_msg_reply_hdr->ProtocolHdrTypeNetSub =  NET_PROTOCOL_TYPE_ICMP_V4;



                                                                /* ----------- PREPARE ICMPv4 REPLY IP HDR ------------ */
    p_ip_hdr               = (NET_IPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    TOS                   =  p_ip_hdr->TOS;                     /* See Note #1c1.                                       */
    flags                 =  NET_IPv4_FLAG_NONE;                /* See Note #1c2.                                       */
    p_msg_opt_cfg_route_ts = (NET_IPv4_OPT_CFG_ROUTE_TS *)0;



                                                                /* ------------- PREPARE ICMPv4 REPLY MSG ------------- */
    switch (p_icmp_hdr->Type) {
        case NET_ICMPv4_MSG_TYPE_ECHO_REQ:                      /* See Note #1d1.                                       */
             p_icmp_hdr_echo         = (NET_ICMPv4_HDR_ECHO *)&p_msg_reply->DataPtr[p_msg_reply_hdr->ICMP_MsgIx];
             p_icmp_hdr_echo->Type   =  NET_ICMPv4_MSG_TYPE_ECHO_REPLY;
#if 0                                                           /* Copied from ICMPv4 req msg (see Note #1d3).          */
             p_icmp_hdr_echo->Code   =  NET_ICMP_MSG_CODE_ECHO;
                                                                /* See Note #1d3A.                                      */
            (void)&p_icmp_hdr_echo->ID;
            (void)&p_icmp_hdr_echo->SeqNbr;
#endif
                                                                /* Calc ICMP msg data len.                              */
             if (p_buf_hdr->ICMP_MsgLen > msg_len_min) {
                 p_msg_reply_hdr->DataIx  = (CPU_INT16U  )(p_msg_reply_hdr->ICMP_MsgIx  + msg_len_min);
                 p_msg_reply_hdr->DataLen = (NET_BUF_SIZE)(p_msg_reply_hdr->ICMP_MsgLen - msg_len_min);
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #4].                */
             } else {
                 p_msg_reply_hdr->DataIx  = NET_BUF_IX_NONE;
                 p_msg_reply_hdr->DataLen = 0u;
#endif
             }
                                                                            /* Calc ICMPv4 msg chk sum (see Note #5).   */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_echo->ChkSum, 0x0000u);    /* Clr             chk sum (see Note #5b).  */
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumDataCalc((void     *) p_msg_reply,
                                                              (void     *) 0,
                                                              (CPU_INT16U) 0u,
                                                              (NET_ERR  *)&err);
#endif
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_echo->ChkSum, &msg_chk_sum);   /* Copy chk sum in net order (see Note #5c).*/
             break;


        case NET_ICMPv4_MSG_TYPE_TS_REQ:                        /* See Note #1d2.                                       */
             p_icmp_hdr_ts            = (NET_ICMPv4_HDR_TS *)&p_msg_reply->DataPtr[p_msg_reply_hdr->ICMP_MsgIx];
             ts                      =  NetUtil_TS_Get();       /* See Note #1d2A.                                      */
             NET_UTIL_VAL_COPY_SET_NET_32(&p_icmp_hdr_ts->TS_Tx, &ts);

             p_icmp_hdr_ts->Type      =  NET_ICMPv4_MSG_TYPE_TS_REPLY;
#if 0                                                           /* Copied from ICMPv4 req msg (see Note #1d3).          */
             p_icmp_hdr_ts->Code      =  NET_ICMPv4_MSG_CODE_TS;
                                                                /* See Note #1d3A.                                      */
            (void)&p_icmp_hdr_ts->ID;
            (void)&p_icmp_hdr_ts->SeqNbr;
#endif
                                                                /* NULL ICMPv4 msg data len.                            */
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #4].                */
             p_msg_reply_hdr->DataIx  =  NET_BUF_IX_NONE;
             p_msg_reply_hdr->DataLen =  0;
#endif
                                                                            /* Calc ICMPv4 msg chk sum (see Note #5).   */
             NET_UTIL_VAL_SET_NET_16(&p_icmp_hdr_ts->ChkSum, 0x0000u);      /* Clr             chk sum (see Note #5b).  */
#ifdef NET_ICMP_CHK_SUM_OFFLOAD_TX
             msg_chk_sum = 0u;
#else
             msg_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *) p_icmp_hdr_ts,
                                                             (CPU_INT16U) p_msg_reply_hdr->ICMP_MsgLen,
                                                             (NET_ERR  *)&err);
#endif
             NET_UTIL_VAL_COPY_16(&p_icmp_hdr_ts->ChkSum, &msg_chk_sum);     /* Copy chk sum in net order (see Note #5c).*/
             break;


        default:                                                /* See Note #2.                                         */
             NetICMPv4_TxPktDiscard(p_msg_reply, p_err);
             return;
    }

#ifndef NET_ICMP_CHK_SUM_OFFLOAD_TX
    if (err != NET_UTIL_ERR_NONE) {                             /* Chk err from NetUtil_16BitOnesCplChkSum()'s.         */
        NetICMPv4_TxPktDiscard(p_msg_reply, p_err);
        return;
    }
#endif




                                                                /* --------------- TX ICMPv4 REPLY MSG ---------------- */
    NetIPv4_Tx((NET_BUF      *)p_msg_reply,
               (NET_IPv4_ADDR )p_buf_hdr->IP_AddrDest,
               (NET_IPv4_ADDR )p_buf_hdr->IP_AddrSrc,
               (NET_IPv4_TOS  )TOS,
               (NET_IPv4_TTL  )NET_IPv4_TTL_DFLT,
               (NET_IPv4_FLAGS)flags,
               (void         *)p_msg_opt_cfg_route_ts,
               (NET_ERR      *)p_err);



                                                                /* ----- FREE ICMPv4 REPLY MSG / UPDATE TX STATS ------ */
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             NetICMPv4_TxPktFree(p_msg_reply);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.TxMsgCtr);
             NET_CTR_STAT_INC(Net_StatCtrs.ICMPv4.TxMsgReplyCtr);
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #6.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr);
                                                                /* Rtn err from NetIPv4_Tx().                           */
             return;


        case NET_IPv4_ERR_TX_PKT:
                                                                /* See Note #6.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr);
            *p_err = NET_ERR_TX;
             return;


        default:
             NetICMPv4_TxPktDiscard(p_msg_reply, p_err);
             return;
    }


   *p_err = NET_ICMPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetICMPv4_TxIxDataGet()
*
* Description : (1) Retrieve stating index of ICMPv4 data from the data buffer beginning:
*
*                   (a) Starting index if found by adding up the header sizes of the lower-level
*                       protocol headers.
*
*
* Argument(s) : if_nbr      Network interface number to transmit data.
*
*               data_len    Length of the ICMPv4 payload.
*
*               p_ix        Pointer to the current protocol index.
*               ----        Argument validated in NetICMPv4_TxMsgErr(),
*                                                 NetICMPv4_TxMsgReqHandler(),
*                                                 NetICMPv4_TxReqReply().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE     No errors.
*
*                               -Returned by NetIF_MTU_GetProtocol()-
*                               See NetIF_MTU_GetProtocol() for additional return error codes.
*
*                               -Returned by NetIPv4_GetTxDataIx()-
*                               See NetIPv4_GetTxDataIx() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgErr(),
*               NetICMPv4_TxMsgReqHandler(),
*               NetICMPv4_TxReqReply().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (2) If the payload is greater than the largest fragmentable IPv4 datagram (65535 bytes) the
*                   packet must be discarded (see RFC #2460, Section 4.5).
*********************************************************************************************************
*/
static  void  NetICMPv4_TxIxDataGet (NET_IF_NBR   if_nbr,
                                     CPU_INT32U   data_len,
                                     CPU_INT16U  *p_ix,
                                     NET_ERR     *p_err)
{
    NET_MTU  mtu;


    if (data_len > NET_IPv4_FRAG_SIZE_MAX) {
       *p_err = NET_ICMPv4_ERR_INVALID_LEN;                     /* See Note #2.                                         */
        return;
    }

    mtu = NetIF_MTU_GetProtocol(if_nbr, NET_PROTOCOL_TYPE_ICMP_V4, NET_IF_FLAG_NONE, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        return;
    }

                                                                /* Add the lower-level hdr offsets.                     */
    NetIPv4_TxIxDataGet(if_nbr, data_len, mtu, p_ix, p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         return;
    }

   *p_err = NET_ICMPv4_ERR_NONE;
}

/*
*********************************************************************************************************
*                                        NetICMPv4_TxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf        Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgErr(),
*               NetICMPv4_TxMsgReq(),
*               NetICMPv4_TxMsgReply().
*
* Note(s)     : (1) (a) Although ICMPv4 Transmit initially requests the network buffer for transmit,
*                       the ICMPv4 layer does NOT maintain a reference to the buffer.
*
*                   (b) Also, since the network interface deallocation task frees ALL unreferenced buffers
*                       after successful transmission, the ICMP layer must NOT free the transmit buffer.
*
*                       See also 'net_if.c  NetIF_TxDeallocTaskHandler()  Note #1c'.
*********************************************************************************************************
*/

static  void  NetICMPv4_TxPktFree (NET_BUF  *p_buf)
{
   (void)&p_buf;                                                 /* Prevent 'variable unused' warning (see Note #1).     */
}


/*
*********************************************************************************************************
*                                      NetICMPv4_TxPktDiscard()
*
* Description : On any ICMPv4 transmit packet error(s), discard packet & buffer.
*
* Argument(s) : p_buf        Pointer to network buffer.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgErr(),
*               NetICMPv4_TxMsgReq(),
*               NetICMPv4_TxMsgReply().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetICMPv4_TxPktDiscard (NET_BUF  *p_buf,
                                      NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.ICMPv4.TxPktDiscardedCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)p_ctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_ICMPv4_MODULE_EN */

