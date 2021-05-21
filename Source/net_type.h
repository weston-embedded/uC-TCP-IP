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
*                                         NETWORK DATA TYPES
*
* Filename : net_type.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_TYPE_MODULE_PRESENT
#define  NET_TYPE_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <cpu.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*
* Note(s) : (1) Ideally, each network module &/or protocol layer would define all its own data types.
*               However, some network module &/or protocol layer data types MUST be defined PRIOR to
*               all other network modules/layers that require their definitions.
*
*               See also 'net.h  NETWORK INCLUDE FILES'.
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                NETWORK TASK CONFIGURATION DATA TYPE
*
* Note(s): (1) When the Stack pointer is defined as null pointer (DEF_NULL), the task's stack should be
*              automatically allowed on the heap of uC/LIB.
*********************************************************************************************************
*/

typedef  struct  net_task_cfg {
    CPU_INT32U   Prio;                                          /* Task priority.                                       */
    CPU_INT32U   StkSizeBytes;                                  /* Size of the stack.                                   */
    void        *StkPtr;                                        /* Pointer to base of the stack (see Note #1).          */
} NET_TASK_CFG;


/*
*********************************************************************************************************
*                                       NETWORK FLAGS DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_FLAGS;


/*
*********************************************************************************************************
*                                    NETWORK TRANSACTION DATA TYPE
*********************************************************************************************************
*/

typedef  enum  net_transaction {
   NET_TRANSACTION_NONE =   0,
   NET_TRANSACTION_RX   = 100,
   NET_TRANSACTION_TX   = 200
} NET_TRANSACTION;

/*
*********************************************************************************************************
*                          NETWORK MAXIMUM TRANSMISSION UNIT (MTU) DATA TYPE
*
* Note(s) : (1) NET_MTU_MIN_VAL & NET_MTU_MAX_VAL  SHOULD be #define'd based on 'NET_MTU' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_MTU;
                                                                /* See Note #1.                                         */
#define  NET_MTU_MIN_VAL                DEF_INT_16U_MIN_VAL
#define  NET_MTU_MAX_VAL                DEF_INT_16U_MAX_VAL


/*
*********************************************************************************************************
*                                     NETWORK CHECK-SUM DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_CHK_SUM;


/*
*********************************************************************************************************
*                                    NETWORK TIMESTAMP DATA TYPES
*
* Note(s) : (1) RFC #791, Section 3.1 'Options : Internet Timestamp' states that "the Timestamp is a
*               right-justified, 32-bit timestamp in milliseconds since midnight UT [Universal Time]".
*********************************************************************************************************
*/

typedef  CPU_INT32U  NET_TS;
typedef  NET_TS      NET_TS_MS;


/*
*********************************************************************************************************
*                                      NETWORK BUFFER DATA TYPES
*
* Note(s) : (1) NET_BUF_NBR_MAX  SHOULD be #define'd based on 'NET_BUF_QTY' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_BUF_QTY;                               /* Defines max qty of net bufs to support.              */

#define  NET_BUF_NBR_MIN                                   1
#define  NET_BUF_NBR_MAX                 DEF_INT_16U_MAX_VAL    /* See Note #1.                                         */


typedef  struct  net_buf  NET_BUF;


/*
*********************************************************************************************************
*                                  NETWORK PACKET COUNTER DATA TYPE
*********************************************************************************************************
*/

typedef  NET_BUF_QTY  NET_PKT_CTR;                              /* Defines max nbr of pkts to cnt.                      */

#define  NET_PKT_CTR_MIN                NET_BUF_NBR_MIN
#define  NET_PKT_CTR_MAX                NET_BUF_NBR_MAX


/*
*********************************************************************************************************
*                                    NETWORK CONNECTION DATA TYPES
*********************************************************************************************************
*/

typedef  CPU_INT16S  NET_CONN_ID;


/*
*********************************************************************************************************
*                                     NETWORK PROTOCOL DATA TYPE
*
* Note(s) : (1) See 'net.h  Note #2'.
*********************************************************************************************************
*/

typedef  enum  net_protocol_type {

                                                                /* ---------------- NET PROTOCOL TYPES ---------------- */
    NET_PROTOCOL_TYPE_NONE                 = 0,
    NET_PROTOCOL_TYPE_ALL                  = 1,

                                                                /* --------------- LINK LAYER PROTOCOLS --------------- */
    NET_PROTOCOL_TYPE_LINK                 = 10,

                                                                /* -------------- NET IF LAYER PROTOCOLS -------------- */
    NET_PROTOCOL_TYPE_IF                   = 20,
    NET_PROTOCOL_TYPE_IF_FRAME             = 21,

    NET_PROTOCOL_TYPE_IF_ETHER             = 25,
    NET_PROTOCOL_TYPE_IF_IEEE_802          = 26,

    NET_PROTOCOL_TYPE_ARP                  = 30,
    NET_PROTOCOL_TYPE_NDP                  = 35,

                                                                /* ---------------- NET LAYER PROTOCOLS --------------- */
    NET_PROTOCOL_TYPE_IP_V4                = 40,
    NET_PROTOCOL_TYPE_IP_V4_OPT            = 41,

    NET_PROTOCOL_TYPE_IP_V6                = 42,
    NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP = 43,
    NET_PROTOCOL_TYPE_IP_V6_EXT_ROUTING    = 44,
    NET_PROTOCOL_TYPE_IP_V6_EXT_FRAG       = 45,
    NET_PROTOCOL_TYPE_IP_V6_EXT_ESP        = 46,
    NET_PROTOCOL_TYPE_IP_V6_EXT_AUTH       = 47,
    NET_PROTOCOL_TYPE_IP_V6_EXT_NONE       = 48,
    NET_PROTOCOL_TYPE_IP_V6_EXT_DEST       = 49,
    NET_PROTOCOL_TYPE_IP_V6_EXT_MOBILITY   = 50,


    NET_PROTOCOL_TYPE_ICMP_V4              = 60,
    NET_PROTOCOL_TYPE_ICMP_V6              = 61,

    NET_PROTOCOL_TYPE_IGMP                 = 62,


                                                                /* ------------- TRANSPORT LAYER PROTOCOLS ------------ */
    NET_PROTOCOL_TYPE_UDP_V4               = 70,
    NET_PROTOCOL_TYPE_TCP_V4               = 71,

    NET_PROTOCOL_TYPE_UDP_V6               = 72,
    NET_PROTOCOL_TYPE_TCP_V6               = 73,

                                                                /* ---------------- APP LAYER PROTOCOLS --------------- */
    NET_PROTOCOL_TYPE_APP                  = 80,
    NET_PROTOCOL_TYPE_SOCK                 = 81

} NET_PROTOCOL_TYPE;


typedef  enum  net_addr_hw_type {
    NET_ADDR_HW_TYPE_NONE,
    NET_ADDR_HW_TYPE_802x
} NET_ADDR_HW_TYPE;



/*
*********************************************************************************************************
*                           NETWORK TRANSPORT LAYER PORT NUMBER DATA TYPES
*
* Note(s) : (1) NET_PORT_NBR_MAX  SHOULD be #define'd based on 'NET_PORT_NBR' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_PORT_NBR;
typedef  CPU_INT16U  NET_PORT_NBR_QTY;                          /* Defines max qty of port nbrs to support.             */

#define  NET_PORT_NBR_MAX               DEF_INT_16U_MAX_VAL     /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                NETWORK INTERFACE & DEVICE DATA TYPES
*
* Note(s) : (1) NET_IF_NBR_MIN_VAL & NET_IF_NBR_MAX_VAL  SHOULD be #define'd based on 'NET_IF_NBR'
*               data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_IF_NBR;
                                                                /* See Note #1.                                         */
#define  NET_IF_NBR_MIN_VAL             DEF_INT_08U_MIN_VAL
#define  NET_IF_NBR_MAX_VAL             DEF_INT_08U_MAX_VAL

typedef  CPU_INT16U  NET_IF_FLAG;

#define  NET_IF_FLAG_NONE               DEF_INT_16U_MIN_VAL
#define  NET_IF_FLAG_FRAG               DEF_BIT_00

typedef  NET_BUF_QTY  NET_IF_Q_SIZE;                            /* Defines max size of net IF q's to support.           */

#define  NET_IF_Q_SIZE_MIN              NET_BUF_NBR_MIN
#define  NET_IF_Q_SIZE_MAX              NET_BUF_NBR_MAX



typedef  struct  net_if       NET_IF;
typedef  struct  net_if_api   NET_IF_API;


typedef  struct  net_dev_cfg  NET_DEV_CFG;


typedef  enum  net_if_type {
    NET_IF_TYPE_NONE,
    NET_IF_TYPE_LOOPBACK,
    NET_IF_TYPE_SERIAL,
    NET_IF_TYPE_PPP,
    NET_IF_TYPE_ETHER,
    NET_IF_TYPE_WIFI
} NET_IF_TYPE;


/*
*********************************************************************************************************
*                                     NETWORK IPv4 LAYER DATA TYPES
*********************************************************************************************************
*/

typedef  CPU_INT32U  NET_IPv4_ADDR;                             /* Defines IPv4 IP addr size.                           */

#define  NET_IPv4_ADDR_LEN                       (sizeof(NET_IPv4_ADDR))
#define  NET_IPv4_ADDR_SIZE                      (sizeof(NET_IPv4_ADDR))


typedef  CPU_INT08U  NET_IPv4_TOS;
typedef  CPU_INT08U  NET_IPv4_TTL;

typedef  NET_FLAGS   NET_IPv4_FLAGS;
typedef  CPU_INT16U  NET_IPv4_HDR_FLAGS;

typedef enum {
    NET_IP_ADDR_FAMILY_UNKNOWN,
    NET_IP_ADDR_FAMILY_NONE,
    NET_IP_ADDR_FAMILY_IPv4,
    NET_IP_ADDR_FAMILY_IPv6
} NET_IP_ADDR_FAMILY;

typedef  struct  net_ipv4_mreq {                                /* IPv4 Multicast Membership Request (socket option)    */
    NET_IPv4_ADDR  mcast_addr;                                  /* IP Address of the multicast group                    */
    NET_IPv4_ADDR  if_ip_addr;                                  /* IP Address of the IF on which to join the group      */
} NET_IPv4_MREQ;

/*
*********************************************************************************************************
*                                     NETWORK IPv6 LAYER DATA TYPES
*********************************************************************************************************
*/

#define  NET_IPv6_ADDR_LEN             (4 * sizeof(CPU_INT32U))



typedef  struct  net_ipv6_addr {                                /* Defines IPv6 IP addr size.                           */
    CPU_INT08U  Addr[NET_IPv6_ADDR_LEN];
} NET_IPv6_ADDR;

#define  NET_IPv6_ADDR_SIZE            (sizeof(NET_IPv6_ADDR))

#define  NET_IPv6_ADDR_LEN_NBR_BITS     NET_IPv6_ADDR_LEN * DEF_OCTET_NBR_BITS

typedef  CPU_INT16U  NET_IPv6_TRAFFIC_CLASS;

typedef  CPU_INT32U  NET_IPv6_FLOW_LABEL;

typedef  CPU_INT08U  NET_IPv6_HOP_LIM;

typedef  NET_FLAGS   NET_IPv6_FLAGS;

typedef  CPU_INT16U  NET_IPv6_FRAG_FLAGS;


/*
*********************************************************************************************************
*                                    NETWORK TCP LAYER DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                    TCP SEQUENCE NUMBER DATA TYPE
*
* Note(s) : (1) 'NET_TCP_SEQ_NBR'  pre-defined in 'net_type.h' PRIOR to all other network modules that
*                require TCP sequence number data type(s).
*********************************************************************************************************
*/

                                                                /* See Note #1.                                         */
typedef  CPU_INT32U  NET_TCP_SEQ_NBR;



/*
*********************************************************************************************************
*                                     TCP SEGMENT SIZE DATA TYPE
*
* Note(s) : (1) 'NET_TCP_SEG_SIZE' pre-defined in 'net_type.h' PRIOR to all other network modules that
*                require TCP segment size data type(s).
*********************************************************************************************************
*/

                                                                /* See Note #1.                                         */
typedef  CPU_INT16U  NET_TCP_SEG_SIZE;

typedef  NET_FLAGS   NET_TCP_FLAGS;
typedef  CPU_INT16U  NET_TCP_HDR_FLAGS;


/*
*********************************************************************************************************
*                                      TCP WINDOW SIZE DATA TYPE
*
* Note(s) : (1) 'NET_TCP_WIN_SIZE' pre-defined in 'net_type.h' PRIOR to all other network modules that
*                require TCP window size data type(s).
*********************************************************************************************************
*/

                                                                /* See Note #1.                                         */
typedef  CPU_INT16U  NET_TCP_WIN_SIZE;

/*
*********************************************************************************************************
*                                   TCP RTT MEASUREMENT DATA TYPES
*
* Note(s) : (1) RTT measurement data types MUST be defined to ensure sufficient range for both scaled
*               & un-scaled, signed & unsigned time measurement values.
*
*           (2) 'NET_TCP_TX_RTT_TS_MS' pre-defined in 'net_type.h' PRIOR to all other network modules
*                that require TCP Transmit Round-Trip Time data type(s).
*********************************************************************************************************
*/
                                                                /* See Note #2.                                         */
typedef  NET_TS_MS   NET_TCP_TX_RTT_TS_MS;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_TYPE_MODULE_PRESENT */
