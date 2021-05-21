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
* Filename : net_icmpv4.h
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
*                                               MODULE
*
* Note(s) : (1) Network IPv4 Layer module is required for applications that requires IPv4 services.
*
*               See also 'net_cfg.h  IP LAYER CONFIGURATION'.
*********************************************************************************************************
*/

#ifndef  NET_ICMPv4_MODULE_PRESENT
#define  NET_ICMPv4_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../../Source/net_cfg_net.h"
#include  "../../Source/net_type.h"
#include  "../../Source/net_stat.h"
#include  "../../Source/net.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_ICMPv4_HDR_SIZE_DFLT                          8

#define  NET_ICMPv4_HDR_SIZE_DEST_UNREACH                NET_ICMPv4_HDR_SIZE_DFLT
#define  NET_ICMPv4_HDR_SIZE_SRC_QUENCH                  NET_ICMPv4_HDR_SIZE_DFLT
#define  NET_ICMPv4_HDR_SIZE_TIME_EXCEED                 NET_ICMPv4_HDR_SIZE_DFLT
#define  NET_ICMPv4_HDR_SIZE_PARAM_PROB                  NET_ICMPv4_HDR_SIZE_DFLT
#define  NET_ICMPv4_HDR_SIZE_ECHO                        NET_ICMPv4_HDR_SIZE_DFLT


/*
*********************************************************************************************************
*                                    ICMPv4 MESSAGE TYPES & CODES
*
* Note(s) : (1) 'DEST_UNREACH' abbreviated to 'DEST' for ICMP 'Destination Unreachable' message
*                error codes to enforce ANSI-compliance of 31-character symbol length uniqueness.
*
*           (2) ICMPv4 'Redirect' & 'Router' messages are NOT supported (see 'net_icmp.h  Note #1').
*
*           (3) ICMPv4 'Address Mask Request' messages received by this host are NOT supported (see
*               'net_icmp.h  Note #3').
*********************************************************************************************************
*/

                                                                /* ----------------- ICMPv4 MSG TYPES ----------------- */
#define  NET_ICMPv4_MSG_TYPE_NONE                        DEF_INT_08U_MAX_VAL

#define  NET_ICMPv4_MSG_TYPE_ECHO_REPLY                    0u
#define  NET_ICMPv4_MSG_TYPE_DEST_UNREACH                  3u
#define  NET_ICMPv4_MSG_TYPE_SRC_QUENCH                    4u
#define  NET_ICMPv4_MSG_TYPE_REDIRECT                      5u   /* See Note #2.                                         */
#define  NET_ICMPv4_MSG_TYPE_ECHO_REQ                      8u
#define  NET_ICMPv4_MSG_TYPE_ROUTE_AD                      9u   /* See Note #2.                                         */
#define  NET_ICMPv4_MSG_TYPE_ROUTE_REQ                    10u   /* See Note #2.                                         */
#define  NET_ICMPv4_MSG_TYPE_TIME_EXCEED                  11u
#define  NET_ICMPv4_MSG_TYPE_PARAM_PROB                   12u
#define  NET_ICMPv4_MSG_TYPE_TS_REQ                       13u
#define  NET_ICMPv4_MSG_TYPE_TS_REPLY                     14u
#define  NET_ICMPv4_MSG_TYPE_ADDR_MASK_REQ                17u
#define  NET_ICMPv4_MSG_TYPE_ADDR_MASK_REPLY              18u


                                                                /* ----------------- ICMPv4 MSG CODES ----------------- */
#define  NET_ICMPv4_MSG_CODE_NONE                        DEF_INT_08U_MAX_VAL

#define  NET_ICMPv4_MSG_CODE_ECHO                          0u
#define  NET_ICMPv4_MSG_CODE_ECHO_REQ                      0u
#define  NET_ICMPv4_MSG_CODE_ECHO_REPLY                    0u

#define  NET_ICMPv4_MSG_CODE_DEST_NET                      0u   /* See Note #1.                                         */
#define  NET_ICMPv4_MSG_CODE_DEST_HOST                     1u
#define  NET_ICMPv4_MSG_CODE_DEST_PROTOCOL                 2u
#define  NET_ICMPv4_MSG_CODE_DEST_PORT                     3u
#define  NET_ICMPv4_MSG_CODE_DEST_FRAG_NEEDED              4u
#define  NET_ICMPv4_MSG_CODE_DEST_ROUTE_FAIL               5u
#define  NET_ICMPv4_MSG_CODE_DEST_NET_UNKNOWN              6u
#define  NET_ICMPv4_MSG_CODE_DEST_HOST_UNKNOWN             7u
#define  NET_ICMPv4_MSG_CODE_DEST_HOST_ISOLATED            8u
#define  NET_ICMPv4_MSG_CODE_DEST_NET_TOS                 11u
#define  NET_ICMPv4_MSG_CODE_DEST_HOST_TOS                12u

#define  NET_ICMPv4_MSG_CODE_SRC_QUENCH                    0u

#define  NET_ICMPv4_MSG_CODE_TIME_EXCEED_TTL               0u
#define  NET_ICMPv4_MSG_CODE_TIME_EXCEED_FRAG_REASM        1u

#define  NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR             0u
#define  NET_ICMPv4_MSG_CODE_PARAM_PROB_OPT_MISSING        1u

#define  NET_ICMPv4_MSG_CODE_TS                            0u
#define  NET_ICMPv4_MSG_CODE_TS_REQ                        0u
#define  NET_ICMPv4_MSG_CODE_TS_REPLY                      0u

#define  NET_ICMPv4_MSG_CODE_ADDR_MASK                     0u
#define  NET_ICMPv4_MSG_CODE_ADDR_MASK_REQ                 0u
#define  NET_ICMPv4_MSG_CODE_ADDR_MASK_REPLY               0u

                                                                                /* --------- IPv4 HDR PTR IXs --------- */
#define  NET_ICMPv4_PTR_IX_BASE                            0
#define  NET_ICMPv4_PTR_IX_IP_BASE                       NET_ICMPv4_PTR_IX_BASE

#define  NET_ICMPv4_PTR_IX_IP_VER                       (NET_ICMPv4_PTR_IX_IP_BASE +  0)
#define  NET_ICMPv4_PTR_IX_IP_HDR_LEN                   (NET_ICMPv4_PTR_IX_IP_BASE +  0)
#define  NET_ICMPv4_PTR_IX_IP_TOS                       (NET_ICMPv4_PTR_IX_IP_BASE +  1)
#define  NET_ICMPv4_PTR_IX_IP_TOT_LEN                   (NET_ICMPv4_PTR_IX_IP_BASE +  2)
#define  NET_ICMPv4_PTR_IX_IP_ID                        (NET_ICMPv4_PTR_IX_IP_BASE +  4)
#define  NET_ICMPv4_PTR_IX_IP_FLAGS                     (NET_ICMPv4_PTR_IX_IP_BASE +  6)
#define  NET_ICMPv4_PTR_IX_IP_FRAG_OFFSET               (NET_ICMPv4_PTR_IX_IP_BASE +  6)
#define  NET_ICMPv4_PTR_IX_IP_TTL                       (NET_ICMPv4_PTR_IX_IP_BASE +  8)
#define  NET_ICMPv4_PTR_IX_IP_PROTOCOL                  (NET_ICMPv4_PTR_IX_IP_BASE +  9)
#define  NET_ICMPv4_PTR_IX_IP_CHK_SUM                   (NET_ICMPv4_PTR_IX_IP_BASE + 10)
#define  NET_ICMPv4_PTR_IX_IP_ADDR_SRC                  (NET_ICMPv4_PTR_IX_IP_BASE + 12)
#define  NET_ICMPv4_PTR_IX_IP_ADDR_DEST                 (NET_ICMPv4_PTR_IX_IP_BASE + 16)
#define  NET_ICMPv4_PTR_IX_IP_OPTS                      (NET_ICMPv4_PTR_IX_IP_BASE + 20)


#define  NET_ICMPv4_MSG_PTR_NONE                         DEF_INT_08U_MAX_VAL


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                   ICMP REQUEST MESSAGE IDENTIFICATION & SEQUENCE NUMBER DATA TYPE
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_ICMPv4_REQ_ID_NONE                               0u
#define  NET_ICMPv4_REQ_SEQ_NONE                              0u

                                                                        /* ------- NET ICMP REQ MSG ID/SEQ NBR -------- */
typedef  struct  net_icmpv4_req_id_seq {
    CPU_INT16U      ID;                                                 /* ICMP Req Msg ID.                             */
    CPU_INT16U      SeqNbr;                                             /* ICMP Req Msg Seq Nbr.                        */
} NET_ICMPv4_REQ_ID_SEQ;


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
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     EXTERNAL C LANGUAGE LINKAGE
*
* Note(s) : (1) C++ compilers MUST 'extern'ally declare ALL C function prototypes & variable/object
*               declarations for correct C language linkage.
*********************************************************************************************************
*/

#ifdef  __cplusplus
extern "C" {
#endif

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


void                   NetICMPv4_Init     (NET_ERR        *p_err);

void                   NetICMPv4_Rx       (NET_BUF        *p_buf,
                                           NET_ERR        *p_err);

void                   NetICMPv4_TxMsgErr (NET_BUF        *p_buf,
                                           CPU_INT08U      type,
                                           CPU_INT08U      code,
                                           CPU_INT08U      ptr,
                                           NET_ERR        *p_err);

CPU_INT16U             NetICMPv4_TxEchoReq(NET_IPv4_ADDR  *p_addr_dest,
                                           CPU_INT16U      id,
                                           void           *p_data,
                                           CPU_INT16U      data_len,
                                           NET_ERR        *p_err);


/*
*********************************************************************************************************
*                                   EXTERNAL C LANGUAGE LINKAGE END
*********************************************************************************************************
*/

#ifdef  __cplusplus
}
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/
#endif /* NET_ICMPv4_MODULE_PRESENT */

