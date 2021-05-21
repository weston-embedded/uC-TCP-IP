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
*                                          NETWORK IP LAYER
*                                         (INTERNET PROTOCOL)
*
* Filename : net_ip.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This module is responsible to initialize different IP version enabled.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IP_MODULE_PRESENT
#define  NET_IP_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  "net_def.h"
#include  "net_type.h"
#include  "../IF/net_if.h"

#include  <lib_def.h>

#ifdef   NET_IP_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  IP HEADER PROTOCOL FIELD DEFINES
*
* Note(s) : (1) Supports ONLY a subset of allowed protocol numbers :
*
*               (a) ICMP
*               (b) IGMP
*               (c) UDP
*               (d) TCP
*               (e) ICMPv6
*
*               See also 'net.h  Note #2a';
*                  & see 'RFC #1340  Assigned Numbers' for a complete list of protocol numbers,
*                  & see 'RFC #2463,  Section 1' for ICMPv6 protocol number.
*********************************************************************************************************
*/


#define  NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP                0u
#define  NET_IP_HDR_PROTOCOL_ICMP                          1u
#define  NET_IP_HDR_PROTOCOL_IGMP                          2u
#define  NET_IP_HDR_PROTOCOL_TCP                           6u
#define  NET_IP_HDR_PROTOCOL_UDP                          17u
#define  NET_IP_HDR_PROTOCOL_EXT_ROUTING                  43u
#define  NET_IP_HDR_PROTOCOL_EXT_FRAG                     44u
#define  NET_IP_HDR_PROTOCOL_EXT_ESP                      50u
#define  NET_IP_HDR_PROTOCOL_EXT_AUTH                     51u
#define  NET_IP_HDR_PROTOCOL_ICMPv6                       58u
#define  NET_IP_HDR_PROTOCOL_EXT_NONE                     59u
#define  NET_IP_HDR_PROTOCOL_EXT_DEST                     60u
#define  NET_IP_HDR_PROTOCOL_EXT_MOBILITY                 135u



/*
*********************************************************************************************************
*                                           IP FLAG DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------- NET IP FLAGS ------------------- */
#define  NET_IPv4_FLAG_NONE                           DEF_BIT_NONE

#define  NET_IPv6_FLAG_NONE                           DEF_BIT_NONE
#define  NET_IPv6_FLAG                                DEF_BIT_00

                                                                /* IPv6 tx flags copied from IP hdr flags.              */
#define  NET_IPv6_FLAG_TX_DONT_FRAG                   NET_IPv6_HDR_FLAG_FRAG_DONT

#define  NET_IP_FRAG_SIZE_NONE                        DEF_INT_16U_MAX_VAL

/*
*********************************************************************************************************
*                                        IP ADDRESS DATA DEFINES
*********************************************************************************************************
*/

#if     (defined(NET_IPv4_MODULE_EN) && !defined(NET_IPv6_MODULE_EN))
#define  NET_IP_MAX_ADDR_SIZE                        NET_IPv4_ADDR_SIZE
#elif  (!defined(NET_IPv4_MODULE_EN) &&  defined(NET_IPv6_MODULE_EN))
#define  NET_IP_MAX_ADDR_SIZE                        NET_IPv6_ADDR_SIZE
#elif   (defined(NET_IPv4_MODULE_EN) &&  defined(NET_IPv6_MODULE_EN))
#define  NET_IP_MAX_ADDR_SIZE                        NET_IPv6_ADDR_SIZE
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_IP_ADDR_LEN;


/*
*********************************************************************************************************
*                                        IP ADDRESS DATA TYPES
*
* Note(s) : (1) 'NET_IP_ADDR' pre-defined in 'net_type.h' PRIOR to all other network modules that require
*                IP address data type.
*
*           (2) 'NET_IP_ADDRS_QTY_MAX'  SHOULD be #define'd based on 'NET_IP_ADDRS_QTY' data type declared.
*********************************************************************************************************
*/

#if 0                                                           /* See Note #1.                                         */
typedef  CPU_INT32U  NET_IP_ADDR;                               /* Defines IP IPv4 addr size.                           */
#endif


typedef  CPU_INT08U  NET_IP_ADDRS_QTY;                          /* Defines max qty of IP addrs per IF to support.       */

#define  NET_IP_ADDRS_QTY_MIN                              1
#define  NET_IP_ADDRS_QTY_MAX            DEF_INT_08U_MAX_VAL    /* See Note #2.                                         */


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
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void  NetIP_Init    (NET_ERR     *p_err);

void  NetIP_IF_Init (NET_IF_NBR   if_nbr,
                     NET_ERR     *p_err);


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
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IPv4_CFG_IF_MAX_NBR_ADDR
#error  "NET_IPv4_CFG_IF_MAX_NBR_ADDR        not #define'd in 'net_cfg.h'"
#error  "                              [MUST be  >= NET_IP_ADDRS_QTY_MIN]"
#error  "                              [     &&  <= NET_IP_ADDRS_QTY_MAX]"

#elif   (DEF_CHK_VAL(NET_IPv4_CFG_IF_MAX_NBR_ADDR,  \
                     NET_IP_ADDRS_QTY_MIN,          \
                     NET_IP_ADDRS_QTY_MAX) != DEF_OK)
#error  "NET_IPv4_CFG_IF_MAX_NBR_ADDR  illegally #define'd in 'net_cfg.h'"
#error  "                              [MUST be  >= NET_IP_ADDRS_QTY_MIN]"
#error  "                              [     &&  <= NET_IP_ADDRS_QTY_MAX]"
#endif



#ifndef  NET_IPv6_CFG_IF_MAX_NBR_ADDR
#error  "NET_IPv6_CFG_IF_MAX_NBR_ADDR        not #define'd in 'net_cfg.h'"
#error  "                              [MUST be  >= NET_IP_ADDRS_QTY_MIN]"
#error  "                              [     &&  <= NET_IP_ADDRS_QTY_MAX]"

#elif   (DEF_CHK_VAL(NET_IPv6_CFG_IF_MAX_NBR_ADDR,  \
                     NET_IP_ADDRS_QTY_MIN,          \
                     NET_IP_ADDRS_QTY_MAX) != DEF_OK)
#error  "NET_IPv6_CFG_IF_MAX_NBR_ADDR  illegally #define'd in 'net_cfg.h'"
#error  "                              [MUST be  >= NET_IP_ADDRS_QTY_MIN]"
#error  "                              [     &&  <= NET_IP_ADDRS_QTY_MAX]"
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
#endif                                                          /* End of net ip module include.                        */
