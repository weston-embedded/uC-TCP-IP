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
*                                         NETWORK ICMP V6 LAYER
*                                 (INTERNET CONTROL MESSAGE PROTOCOL)
*
* Filename : net_icmpv6.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Internet Control Message Protocol V6 as described in RFC #4443 with the
*                following restrictions/constraints :
*
*                (a) ICMPv6 Error Message received must be passed to the upper layer process that
*                    originated the packet that caused the error.
*
*                (b) Reception of ICMPv6 Packet too big messages not yet supported.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) Network ICMPv6 Layer module is required for applications that requires IPv6 services.
*
*               See also 'net_cfg.h  IP LAYER CONFIGURATION'.
*
*           (2) The following IP-module-present configuration value MUST be pre-#define'd in
*               'net_cfg_net.h' PRIOR to all other network modules that require IPv6 Layer
*               configuration (see 'net_cfg_net.h  IP LAYER CONFIGURATION  Note #2b') :
*
*                   NET_ICMPv6_MODULE_EN
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_ICMPv6_MODULE_PRESENT
#define  NET_ICMPv6_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../../Source/net_cfg_net.h"
#include  "../../Source/net_type.h"
#include  "../../Source/net_stat.h"
#include  "../../Source/net_buf.h"
#include  "net_ipv6.h"

#ifdef   NET_ICMPv6_MODULE_EN                                /* See Note #2.                                         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  ICMPv6 MESSAGE TYPES & CODES DEFINES
*********************************************************************************************************
*/

                                                                /* ----------------- ICMPv6 MSG TYPES ----------------- */
#define  NET_ICMPv6_MSG_TYPE_NONE                        DEF_INT_08U_MAX_VAL

#define  NET_ICMPv6_MSG_TYPE_ECHO_REQ                    128u
#define  NET_ICMPv6_MSG_TYPE_ECHO_REPLY                  129u

#define  NET_ICMPv6_MSG_TYPE_DEST_UNREACH                  1u
#define  NET_ICMPv6_MSG_TYPE_PKT_TOO_BIG                   2u
#define  NET_ICMPv6_MSG_TYPE_TIME_EXCEED                   3u
#define  NET_ICMPv6_MSG_TYPE_PARAM_PROB                    4u

#define  NET_ICMPv6_MSG_TYPE_MLDP_QUERY                  130u
#define  NET_ICMPv6_MSG_TYPE_MLDP_REPORT_V1              131u
#define  NET_ICMPv6_MSG_TYPE_MLDP_REPORT_V2              143u
#define  NET_ICMPv6_MSG_TYPE_MLDP_DONE                   132u

#define  NET_ICMPv6_MSG_TYPE_NDP_ROUTER_SOL              133u
#define  NET_ICMPv6_MSG_TYPE_NDP_ROUTER_ADV              134u
#define  NET_ICMPv6_MSG_TYPE_NDP_NEIGHBOR_SOL            135u
#define  NET_ICMPv6_MSG_TYPE_NDP_NEIGHBOR_ADV            136u
#define  NET_ICMPv6_MSG_TYPE_NDP_REDIRECT                137u

                                                                /* ----------------- ICMPv6 MSG CODES ----------------- */
#define  NET_ICMPv6_MSG_CODE_NONE                        DEF_INT_08U_MAX_VAL

#define  NET_ICMPv6_MSG_CODE_ECHO                          0u
#define  NET_ICMPv6_MSG_CODE_ECHO_REQ                      0u
#define  NET_ICMPv6_MSG_CODE_ECHO_REPLY                    0u

#define  NET_ICMPv6_MSG_CODE_DEST_NO_ROUTE                 0u
#define  NET_ICMPv6_MSG_CODE_DEST_COM_PROHIBITED           1u
#define  NET_ICMPv6_MSG_CODE_DEST_BEYONG_SCOPE             2u
#define  NET_ICMPv6_MSG_CODE_DEST_ADDR_UNREACHABLE         3u
#define  NET_ICMPv6_MSG_CODE_DEST_PORT_UNREACHABLE         4u
#define  NET_ICMPv6_MSG_CODE_DEST_SRC_ADDR_FAIL_INGRESS    5u
#define  NET_ICMPv6_MSG_CODE_DEST_ROUTE_REJECT             6u

#define  NET_ICMPv6_MSG_CODE_TIME_EXCEED_HOP_LIMIT         0u
#define  NET_ICMPv6_MSG_CODE_TIME_EXCEED_FRAG_REASM        1u

#define  NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR             0u
#define  NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_NEXT_HDR       1u
#define  NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_OPT            2u

#define  NET_ICMPv6_MSG_CODE_MLDP_QUERY                    0u
#define  NET_ICMPv6_MSG_CODE_MLDP_REPORT                   0u
#define  NET_ICMPv6_MSG_CODE_MLDP_DONE                     0u

#define  NET_ICMPv6_MSG_CODE_NDP_ROUTER_SOL                0u
#define  NET_ICMPv6_MSG_CODE_NDP_ROUTER_ADV                0u
#define  NET_ICMPv6_MSG_CODE_NDP_NEIGHBOR_SOL              0u
#define  NET_ICMPv6_MSG_CODE_NDP_NEIGHBOR_ADV              0u
#define  NET_ICMPv6_MSG_CODE_NDP_REDIRECT                  0u


/*
*********************************************************************************************************
*                                       ICMPv6 POINTER DEFINES
*
* Note(s) : (1) In RFC #4443, the Parameter Problem Message defines a pointer (PTR) as an index (IX) into
*              an option or message.
*
*           (2) ICMPv6 Parameter Problem Message pointer validation currently ONLY supports the following
*               protocols :
*
*               (a) IPv6
*********************************************************************************************************
*/

#define  NET_ICMPv6_PTR_IX_BASE                            0

                                                                                /* --------- IPv6 HDR PTR IXs --------- */
#define  NET_ICMPv6_PTR_IX_IP_BASE                       NET_ICMPv6_PTR_IX_BASE

#define  NET_ICMPv6_PTR_IX_IP_VER                       (NET_ICMPv6_PTR_IX_IP_BASE +  0)
#define  NET_ICMPv6_PTR_IX_IP_TRAFFIC_CLASS             (NET_ICMPv6_PTR_IX_IP_BASE +  0)
#define  NET_ICMPv6_PTR_IX_IP_FLOW_LABEL                (NET_ICMPv6_PTR_IX_IP_BASE +  1)
#define  NET_ICMPv6_PTR_IX_IP_PAYLOAD_LEN               (NET_ICMPv6_PTR_IX_IP_BASE +  4)
#define  NET_ICMPv6_PTR_IX_IP_NEXT_HDR                  (NET_ICMPv6_PTR_IX_IP_BASE +  6)
#define  NET_ICMPv6_PTR_IX_IP_HOP_LIM                   (NET_ICMPv6_PTR_IX_IP_BASE +  7)
#define  NET_ICMPv6_PTR_IX_IP_ADDR_SRC                  (NET_ICMPv6_PTR_IX_IP_BASE +  8)
#define  NET_ICMPv6_PTR_IX_IP_ADDR_DEST                 (NET_ICMPv6_PTR_IX_IP_BASE + 24)

                                                                                /* -------- ICMPv6 MSG PTR IXs -------- */
#define  NET_ICMPv6_PTR_IX_ICMP_BASE                       0

#define  NET_ICMPv6_PTR_IX_ICMP_TYPE                       0
#define  NET_ICMPv6_PTR_IX_ICMP_CODE                       1
#define  NET_ICMPv6_PTR_IX_ICMP_CHK_SUM                    2

#define  NET_ICMPv6_PTR_IX_ICMP_PTR                        4
#define  NET_ICMPv6_PTR_IX_ICMP_UNUSED                     4
#define  NET_ICMPv6_PTR_IX_ICMP_UNUSED_PARAM_PROB          5

#define  NET_ICMPv6_PTR_IX_IP_FRAG_OFFSET                  2


/*
*********************************************************************************************************
*                                       ICMPv6 MESSAGE DEFINES
*********************************************************************************************************
*/

#define  NET_ICMPv6_MSG_PTR_NONE                         DEF_INT_08U_MAX_VAL

#define  NET_ICMPv6_MSG_PTR_MIN_PARAM_PROB               NET_ICMPv6_MSG_LEN_MIN_DFLT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            ICMPv6 HEADER
*
* Note(s) : (1) See RFC #4443 for ICMPv6 message header formats.
*********************************************************************************************************
*/

                                                                        /* -------------- NET ICMPv6 HDR -------------- */
typedef  struct  net_ICMPv6_hdr {
    CPU_INT08U      Type;                                               /* ICMPv6 msg type.                             */
    CPU_INT08U      Code;                                               /* ICMPv6 msg code.                             */
    CPU_INT16U      ChkSum;                                             /* ICMPv6 msg chk sum.                          */
} NET_ICMPv6_HDR;


/*
*********************************************************************************************************
*                   ICMP REQUEST MESSAGE IDENTIFICATION & SEQUENCE NUMBER DATA TYPE
*********************************************************************************************************
*/

#define  NET_ICMPv6_REQ_ID_NONE                            0u
#define  NET_ICMPv6_REQ_SEQ_NONE                           0u

                                                                        /* ------- NET ICMP REQ MSG ID/SEQ NBR -------- */
typedef  struct  net_icmpv6_req_id_seq {
    CPU_INT16U      ID;                                                 /* ICMP Req Msg ID.                             */
    CPU_INT16U      SeqNbr;                                             /* ICMP Req Msg Seq Nbr.                        */
} NET_ICMPv6_REQ_ID_SEQ;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
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

/*
*********************************************************************************************************
*                                     NET_ICMPv6_TX_GET_SEQ_NBR()
*
* Description : Get next ICMPv6 transmit message sequence number.
*
* Argument(s) : seq_nbr     Variable that will receive the returned ICMPv6 transmit message sequence number.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_TxMsgReq().
*
*               This macro is an INTERNAL network protocol suite macro & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Return ICMP sequence number is NOT converted from host-order to network-order.
*********************************************************************************************************
*/

#define  NET_ICMPv6_TX_GET_SEQ_NBR(seq_nbr)     do { NET_UTIL_VAL_COPY_16(&(seq_nbr), &NetICMPv6_TxSeqNbrCtr); \
                                                     NetICMPv6_TxSeqNbrCtr++;                                } while (0)


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             PUBLIC API
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void                   NetICMPv6_Init             (void);

void                   NetICMPv6_Rx               (NET_BUF           *p_buf,
                                                   NET_ERR           *p_err);



void                   NetICMPv6_TxMsgErr         (NET_BUF           *p_buf,
                                                   CPU_INT08U         type,
                                                   CPU_INT08U         code,
                                                   CPU_INT32U         ptr,
                                                   NET_ERR           *p_err);

NET_ICMPv6_REQ_ID_SEQ  NetICMPv6_TxMsgReq         (NET_IF_NBR         if_nbr,       /* See Note #1.                     */
                                                   CPU_INT08U         type,
                                                   CPU_INT08U         code,
                                                   CPU_INT16U         id,
                                                   NET_IPv6_ADDR     *p_addr_src,
                                                   NET_IPv6_ADDR     *p_addr_dest,
                                                   NET_IPv6_HOP_LIM   hop_limit,
                                                   CPU_BOOLEAN        dest_mcast,
                                                   void              *p_data,
                                                   CPU_INT16U         data_len,
                                                   NET_ERR           *p_err);

NET_ICMPv6_REQ_ID_SEQ  NetICMPv6_TxMsgReqHandler  (NET_IF_NBR         if_nbr,     /* See Note #1.                       */
                                                   CPU_INT08U         type,
                                                   CPU_INT08U         code,
                                                   CPU_INT16U         id,
                                                   NET_IPv6_ADDR     *p_addr_src,
                                                   NET_IPv6_ADDR     *p_addr_dest,
                                                   NET_IPv6_HOP_LIM   TTL,
                                                   CPU_BOOLEAN        dest_mcast,
                                                   NET_IPv6_EXT_HDR  *p_ext_hdr_list,
                                                   void              *p_data,
                                                   CPU_INT16U         data_len,
                                                   NET_ERR           *p_err);

CPU_INT16U             NetICMPv6_TxEchoReq        (NET_IPv6_ADDR     *p_addr_dest,
                                                   CPU_INT16U         id,
                                                   void              *p_msg_data,
                                                   CPU_INT16U         p_data,
                                                   NET_ERR           *p_err);

void                   NetICMPv6_TxMsgReply       (NET_BUF           *p_buf,
                                                   NET_BUF_HDR       *p_buf_hdr,
                                                   NET_ICMPv6_HDR    *p_icmp_hdr,
                                                   NET_ERR           *p_err);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_ICMPv6_MODULE_EN       */
#endif  /* NET_ICMPv6_MODULE_PRESENT  */
