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
*                                    NETWORK CONNECTION MANAGEMENT
*
* Filename : net_conn.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports network connections for local & remote addresses of the following :
*
*                (a) Families :
*                    (1) IPv4 Connections
*                        (A) BSD 4.x Sockets
*
*                (b) Connection types :
*                    (1) Datagram
*                    (2) Stream
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_CONN_MODULE_PRESENT
#define  NET_CONN_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  "net_def.h"
#include  "net_sock.h"
#include  "net_type.h"
#include  "net_err.h"
#include  "net_stat.h"
#include  <lib_def.h>
#include  <cpu.h>
#include  <cpu_core.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef NET_CONN_MODULE
#define  NET_CONN_EXT
#else
#define  NET_CONN_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#if (defined(NET_IPv6_MODULE_EN))
    #define  NET_CONN_ADDR_LEN_MAX                      NET_SOCK_ADDR_LEN_IP_V6

#elif (defined(NET_IPv4_MODULE_EN))
    #define  NET_CONN_ADDR_LEN_MAX                      NET_SOCK_ADDR_LEN_IP_V4

#else
    #define  NET_CONN_ADDR_LEN_MAX                      0
#endif


#if (defined(NET_IPv4_MODULE_EN) && defined(NET_IPv6_MODULE_EN))
    #define  NET_CONN_PROTOCOL_IP_NBR_MAX              2
#else
    #define  NET_CONN_PROTOCOL_IP_NBR_MAX              1
#endif


#ifdef NET_TCP_MODULE_EN
    #define  NET_CONN_PROTOCOL_TRANSPORT_NBR_MAX       2
#else
    #define  NET_CONN_PROTOCOL_TRANSPORT_NBR_MAX       1
#endif


#define  NET_CONN_PROTOCOL_NBR_MAX              NET_CONN_PROTOCOL_TRANSPORT_NBR_MAX + NET_CONN_PROTOCOL_IP_NBR_MAX



#define  NET_CONN_ACCESSED_TH_MIN                         10
#define  NET_CONN_ACCESSED_TH_MAX                      65000
#define  NET_CONN_ACCESSED_TH_DFLT                       100


/*
*********************************************************************************************************
*                              NETWORK CONNECTION PROTOCOL INDEX DEFINES
*********************************************************************************************************
*/

#define  NET_CONN_PROTOCOL_MAX_MIN                       NET_SOCK_PROTO_MAX_MIN


#define  NET_CONN_PROTOCOL_IX_IP_V4_UDP                    0
#define  NET_CONN_PROTOCOL_IX_IP_V4_TCP                    1

#ifdef   NET_IPv4_MODULE_EN
#define  NET_CONN_PROTOCOL_IX_OFFSET                       2
#else
#define  NET_CONN_PROTOCOL_IX_OFFSET                       0
#endif


#define  NET_CONN_PROTOCOL_IX_IP_V6_UDP                    0 + NET_CONN_PROTOCOL_IX_OFFSET
#define  NET_CONN_PROTOCOL_IX_IP_V6_TCP                    1 + NET_CONN_PROTOCOL_IX_OFFSET



/*
*********************************************************************************************************
*                                 NETWORK CONNECTION ADDRESS DEFINES
*********************************************************************************************************
*/

#define  NET_CONN_ADDR_IP_LEN_PORT                       NET_SOCK_ADDR_IP_LEN_PORT

#define  NET_CONN_ADDR_IP_IX_BASE                        NET_SOCK_ADDR_IP_IX_BASE
#define  NET_CONN_ADDR_IP_IX_PORT                        NET_SOCK_ADDR_IP_IX_PORT

#ifdef   NET_IPv4_MODULE_EN

#define  NET_CONN_ADDR_IP_V4_IX_ADDR                     NET_SOCK_ADDR_IP_V4_IX_ADDR

#define  NET_CONN_ADDR_IP_V4_LEN_ADDR                    NET_SOCK_ADDR_IP_V4_LEN_ADDR
#define  NET_CONN_ADDR_IP_V4_WILDCARD                    NET_SOCK_ADDR_IP_V4_WILDCARD
#define  NET_CONN_ADDR_IP_V4_BROADCAST                   NET_SOCK_ADDR_IP_V4_BROADCAST

#endif

#ifdef   NET_IPv6_MODULE_EN

#define  NET_CONN_ADDR_IP_V6_IX_ADDR                     NET_SOCK_ADDR_IP_V6_IX_ADDR

#define  NET_CONN_ADDR_IP_V6_LEN_ADDR                    NET_SOCK_ADDR_IP_V6_LEN_ADDR
#define  NET_CONN_ADDR_IP_V6_WILDCARD                    NET_SOCK_ADDR_IP_V6_WILDCARD

#endif


/*
*********************************************************************************************************
*                                   NETWORK CONNECTION FLAG DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------ NET CONN FLAGS ------------------ */
#define  NET_CONN_FLAG_NONE                       DEF_BIT_NONE
#define  NET_CONN_FLAG_USED                       DEF_BIT_00    /* Conn cur used; i.e. NOT in free conn pool.           */


/*
*********************************************************************************************************
*                                   NETWORK CONNECTION TYPE DEFINES
*********************************************************************************************************
*/

#define  NET_CONN_TYPE_CONN_NONE                           0u
#define  NET_CONN_TYPE_CONN_HALF                           1u
#define  NET_CONN_TYPE_CONN_FULL                           2u


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                 NETWORK CONNECTION FAMILY DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_CONN_FAMILY;


/*
*********************************************************************************************************
*                               NETWORK CONNECTION LIST INDEX DATA TYPE
*
* Note(s) : (1) NET_CONN_PROTOCOL_IX_NONE  SHOULD be #define'd based on 'NET_CONN_PROTOCOL_IX' data type
*               declared.
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_CONN_PROTOCOL_IX;

#define  NET_CONN_PROTOCOL_IX_NONE      DEF_INT_08U_MAX_VAL     /* Define as max unsigned val (see Note #1).            */


/*
*********************************************************************************************************
*                             NETWORK CONNECTION ADDRESS LENGTH DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_CONN_ADDR_LEN;


/*
*********************************************************************************************************
*                             NETWORK CONNECTION LIST QUANTITY DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_CONN_LIST_QTY;                         /* Defines max qty of conn lists to support.            */


/*
*********************************************************************************************************
*                                NETWORK CONNECTION QUANTITY DATA TYPE
*
* Note(s) : (1) See also 'NETWORK CONNECTION IDENTIFICATION DATA TYPE  Note #2b'.
*********************************************************************************************************
*/

typedef  CPU_INT16S  NET_CONN_QTY;                              /* Defines max qty of conns      to support.            */


/*
*********************************************************************************************************
*                             NETWORK CONNECTION IDENTIFICATION DATA TYPE
*
* Note(s) : (1) 'NET_CONN_ID' pre-defined in 'net_type.h' PRIOR to all other network modules that require
*                network connection data type.
*
*           (2) (a) The following network connection values are pre-#define'd in 'net_cfg_net.h' PRIOR
*                   to 'net_conn.h' based on other modules' configuration values :
*
*                   (1) NET_CONN_NBR_MIN
*
*               (b) (1) NET_CONN_NBR_MAX  SHOULD be #define'd based on 'NET_CONN_QTY' data type declared.
*
*                   (2) However, since network connection handle identifiers are data-typed as 16-bit
*                       signed integers; the maximum number of valid network connection identifiers, &
*                       therefore the maximum number of valid network connections, is the total number
*                       of non-negative values that 16-bit signed integers support.
*********************************************************************************************************
*/

                                                                /* CFG CONN NBR THs                                     */
                                                                /* See Note #4.                                         */
#ifdef   NET_TCP_MODULE_EN
    #if (NET_TCP_NBR_CONN > NET_SOCK_NBR_SOCK)
        #define NET_CONN_SOCK_TCP_NBR                 NET_SOCK_NBR_SOCK

    #else
        #define NET_CONN_SOCK_TCP_NBR                 NET_TCP_NBR_CONN
    #endif

#else
    #define NET_CONN_SOCK_TCP_NBR                 0
#endif


#ifdef   NET_TCP_MODULE_EN
    #define  NET_CONN_NBR_MIN                      (NET_SOCK_NBR_SOCK + \
                                                    NET_TCP_NBR_CONN -  \
                                                    NET_SOCK_CFG_SOCK_NBR_TCP)

#else
    #define  NET_CONN_NBR_MIN                       NET_SOCK_NBR_SOCK
#endif


#ifndef  NET_TCP_CFG_NBR_CONN
#define  NET_TCP_CFG_NBR_CONN                           0u
#endif

#ifndef  NET_CONN_CFG_NBR_CONN
#define  NET_CONN_NBR_CONN                          NET_SOCK_NBR_SOCK         + NET_TCP_CFG_NBR_CONN +             \
                                                   (NET_SOCK_CFG_SOCK_NBR_TCP * NET_SOCK_CFG_CONN_ACCEPT_Q_SIZE_MAX)
#else
#define  NET_CONN_NBR_CONN                          NET_CONN_CFG_NBR_CONN
#endif




#if 0                                                           /* See Note #1.                                         */
typedef  CPU_INT16S  NET_CONN_ID;
#endif

#if 0                                                           /* See Note #2a1.                                       */
#define  NET_CONN_NBR_MIN                                  1
#endif
#define  NET_CONN_NBR_MAX                DEF_INT_16S_MAX_VAL    /* See Note #2b.                                        */

#define  NET_CONN_ID_MIN                                   0
#define  NET_CONN_ID_MAX          (NET_CONN_NBR_CONN - 1)


/*
*********************************************************************************************************
*                               NETWORK CONNECTION LIST INDEX DATA TYPE
*********************************************************************************************************
*/

typedef  NET_CONN_QTY  NET_CONN_LIST_IX;

#define  NET_CONN_LIST_IX_NONE                            -1
#define  NET_CONN_LIST_IX_MIN                              0
#define  NET_CONN_LIST_IX_MAX     (NET_CONN_NBR_CONN - 1)


/*
*********************************************************************************************************
*                               NETWORK CONNECTION CLOSE CODE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_CONN_CLOSE_CODE;

#define  NET_CONN_CLOSE_NONE                               0u
#define  NET_CONN_CLOSE_ALL                                1u
#define  NET_CONN_CLOSE_BY_IF                              2u
#define  NET_CONN_CLOSE_BY_ADDR                            3u

#define  NET_CONN_CLOSE_HALF                              10u   /* Conn closed to tx's; open to rx's.                   */
#define  NET_CONN_CLOSE_FULL                              11u


/*
*********************************************************************************************************
*                                 NETWORK CONNECTION FLAGS DATA TYPE
*********************************************************************************************************
*/

typedef  NET_FLAGS  NET_CONN_FLAGS;


/*
*********************************************************************************************************
*                                    NETWORK CONNECTION DATA TYPE
*
*                                             NET_CONN
*                                          |-------------|
*                                          |  Conn Type  |
*                            Previous      |-------------|
*                           Connection <----------O      |
*                           Chain Lists    |-------------|        Next
*                                          |      O----------> Connection
*                            Previous      |-------------|     Chain Lists
*                           Connection <----------O      |
*                                          |-------------|        Next
*                                          |      O----------> Connection
*                                          |-------------|
*                                          |      O----------> Connection
*                                          |-------------|        List
*                                          |  Conn IDs   |
*                                          |-------------|
*                                          |   IF Nbr    |
*                                          |-------------|
*                                          |   Family    |
*                                          |  Protocol   |
*                                          |-------------|
*                                          |    Local    |
*                                          |   Address   |
*                                          |-------------|
*                                          |   Remote    |
*                                          |   Address   |
*                                          |-------------|
*                                          |  Accessed   |
*                                          |  Counters   |
*                                          |-------------|
*                                          |IP Tx Params |
*                                          |-------------|
*                                          |    Flags    |
*                                          |-------------|
*
*
* Note(s) : (1) (a) (1) RFC #1122, Section 4.1.4 states that "an application-layer program MUST be
*                       able to set the ... [IP] TOS ... for sending a ... datagram, [which] must
*                       be passed transparently to the IP layer".
*
*                   (2) RFC #1122, Section 4.2.4.2 reiterates that :
*
*                       (A) "The application layer MUST be able to specify the [IP] Type-of-Service
*                            (TOS) for [packets] that are sent on a connection."
*
*                       (B) "It not required [sic], but the application SHOULD be able to change the
*                            [IP] TOS during the connection lifetime."
*
*               (b) (1) (A) RFC #1122, Section 4.1.4 states that "an application-layer program MUST
*                           be able to set the [IP] TTL ... for sending a ... datagram, [which] must
*                           be passed transparently to the IP layer".
*
*                       (B) RFC #1122, Section 4.2.2.19 reiterates that "the [IP] TTL value used to
*                           send ... [packets] MUST be configurable".
*
*                   (2) RFC #1112, Section 6.1 states that "the service interface should provide a
*                       way for the upper-layer protocol to specify the IP time-to-live of an outgoing
*                       multicast datagram".
*
*               (c) (1) (A) RFC #1122, Section 4.1.4 states that "an application-layer program MUST
*                           be able to set the ... IP options for sending a ... datagram, [which] must
*                           be passed transparently to the IP layer".
*
*                       (B) RFC #1122, Section 4.1.3.2 reiterates that "an application MUST be able
*                           to specify IP options to be sent ... and ... MUST pass these options to
*                           the IP layer".
*
*                       (C) RFC #1122, Section 4.2.3.8 adds that :
*
*                           (1) "A TCP MAY support the [IP] Time Stamp and Record Route options."
*
*                           (2) "An application MUST be able to specify a [IP] source route when it
*                                ... opens a TCP connection, and this MUST take precedence over a
*                               source route received in a datagram."
*
*                   (2) IP transmit options currently NOT implemented      See 'net_tcp.h  Note #1d'
*********************************************************************************************************
*/

                                                                /* --------------------- NET CONN --------------------- */
typedef  struct  net_conn  NET_CONN;

struct  net_conn {
    NET_CONN                *PrevChainPtr;                      /* Ptr to PREV conn chain list.                         */
    NET_CONN                *NextChainPtr;                      /* Ptr to NEXT conn chain list.                         */

    NET_CONN                *PrevConnPtr;                       /* Ptr to PREV conn.                                    */
    NET_CONN                *NextConnPtr;                       /* Ptr to NEXT conn.                                    */

    NET_CONN               **ConnList;                          /* Conn list.                                           */


    NET_CONN_ID              ID;                                /* Net             conn id.                             */
    NET_CONN_ID              ID_App;                            /* App       layer conn id.                             */
    NET_CONN_ID              ID_AppClone;                       /* App       layer conn id clone.                       */
    NET_CONN_ID              ID_Transport;                      /* Transport layer conn id.                             */


    NET_IF_NBR               IF_Nbr;                            /* IF nbr.                                              */

    NET_CONN_FAMILY          Family;                            /* Conn family.                                         */
    NET_CONN_PROTOCOL_IX     ProtocolIx;                        /* Conn protocol ix.                                    */


    CPU_INT08U               AddrLocal[NET_CONN_ADDR_LEN_MAX];  /* Conn local  addr.                                    */
    CPU_BOOLEAN              AddrLocalValid;                    /* Conn local  addr valid flag.                         */

    CPU_INT08U               AddrRemote[NET_CONN_ADDR_LEN_MAX]; /* Conn remote addr.                                    */
    CPU_BOOLEAN              AddrRemoteValid;                   /* Conn remote addr valid flag.                         */


    CPU_INT16U               ConnChainAccessedCtr;              /* Nbr conn chain head accesses.                        */
    CPU_INT16U               ConnAccessedCtr;                   /* Nbr conn            accesses (non-chain head).       */


    NET_CONN_FLAGS           Flags;                             /* Conn flags.                                          */
#ifdef  NET_IPv4_MODULE_EN
    NET_IPv4_FLAGS           TxIPv4Flags;                       /* Conn tx IPv4 flags.                                  */
    NET_IPv4_TOS             TxIPv4TOS;                         /* Conn tx IPv4 TOS           (see Note #2a).           */
    NET_IPv4_TTL             TxIPv4TTL;                         /* Conn tx IPv4 TTL           (see Note #2b1).          */
#ifdef  NET_MCAST_TX_MODULE_EN
    NET_IPv4_TTL             TxIPv4TTL_Multicast;               /* Conn tx IPv4 TTL multicast (see Note #2b2).          */
#endif
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_IPv6_FLAGS           TxIPv6Flags;                       /* Conn tx IPv6 flags.                                  */
    NET_IPv6_TRAFFIC_CLASS   TxIPv6TrafficClass;                /* Conn tx IPv6 traffic class.                          */
    NET_IPv6_FLOW_LABEL      TxIPv6FlowLabel;                   /* Conn tx IPv6 flow label.                             */
    NET_IPv6_HOP_LIM         TxIPv6HopLim;                      /* Conn tx IPv6 hop lim.                                */
#ifdef  NET_MCAST_TX_MODULE_EN
    NET_IPv6_HOP_LIM         TxIPv6HopLimMulticast;             /* Conn tx IPv6 TTL multicast (see Note #2b2).          */
#endif
#endif
};


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

NET_CONN_EXT  NET_CONN          NetConn_Tbl[NET_CONN_NBR_CONN];
NET_CONN_EXT  NET_CONN         *NetConn_PoolPtr;                    /* Ptr to pool of free net conns.                   */
NET_CONN_EXT  NET_STAT_POOL     NetConn_PoolStat;

                                                                    /* Array of ptrs to net conn lists.                 */
NET_CONN_EXT  NET_CONN         *NetConn_ConnListHead[NET_CONN_PROTOCOL_NBR_MAX];

NET_CONN_EXT  NET_CONN         *NetConn_ConnListChainPtr;           /* Ptr to cur  conn list chain to update.           */
NET_CONN_EXT  NET_CONN         *NetConn_ConnListConnPtr;            /* Ptr to cur  conn list conn  to update.           */
NET_CONN_EXT  NET_CONN         *NetConn_ConnListNextChainPtr;       /* Ptr to next conn list chain to update.           */
NET_CONN_EXT  NET_CONN         *NetConn_ConnListNextConnPtr;        /* Ptr to next conn list conn  to update.           */


NET_CONN_EXT  CPU_INT16U        NetConn_AccessedTh_nbr;             /* Nbr successful srch's to promote net conns.      */

                                                                    /* Conn addr wildcard cfg.                          */
NET_CONN_EXT  CPU_BOOLEAN       NetConn_AddrWildCardAvailv4;
#ifdef  NET_IPv4_MODULE_EN
NET_CONN_EXT  CPU_INT08U        NetConn_AddrWildCardv4[NET_CONN_ADDR_LEN_MAX];
#endif


NET_CONN_EXT  CPU_BOOLEAN       NetConn_AddrWildCardAvailv6;
#ifdef  NET_IPv6_MODULE_EN
NET_CONN_EXT  CPU_INT08U        NetConn_AddrWildCardv6[NET_CONN_ADDR_LEN_MAX];
#endif


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

void              NetConn_Init                      (void);



                                                                                /* ------------ CFG FNCTS ------------- */
CPU_BOOLEAN       NetConn_CfgAccessedTh             (CPU_INT16U               nbr_access);



                                                                                /* ---------- CONN API FNCTS ---------- */
NET_CONN_ID       NetConn_Get                       (NET_CONN_FAMILY          family,
                                                     NET_CONN_PROTOCOL_IX     protocol_ix,
                                                     NET_ERR                 *p_err);

void              NetConn_Free                      (NET_CONN_ID              conn_id);


void              NetConn_Copy                      (NET_CONN_ID              conn_id_dest,
                                                     NET_CONN_ID              conn_id_src);


void              NetConn_CloseFromApp              (NET_CONN_ID              conn_id,
                                                     CPU_BOOLEAN              close_conn_transport);

void              NetConn_CloseFromTransport        (NET_CONN_ID              conn_id,
                                                     CPU_BOOLEAN              close_conn_app);


void              NetConn_CloseAllConns             (void);

void              NetConn_CloseAllConnsByIF         (NET_IF_NBR               if_nbr);

void              NetConn_CloseAllConnsByAddr       (CPU_INT08U              *p_addr,
                                                     NET_CONN_ADDR_LEN        addr_len);

void              NetConn_CloseAllConnsByAddrHandler(CPU_INT08U              *p_addr,
                                                     NET_CONN_ADDR_LEN        addr_len);



NET_IF_NBR        NetConn_IF_NbrGet                 (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);


NET_CONN_ID       NetConn_ID_AppGet                 (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_ID_AppSet                 (NET_CONN_ID              conn_id,
                                                     NET_CONN_ID              conn_id_app,
                                                     NET_ERR                 *p_err);


NET_CONN_ID       NetConn_ID_AppCloneGet            (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_ID_AppCloneSet            (NET_CONN_ID              conn_id,
                                                     NET_CONN_ID              conn_id_app,
                                                     NET_ERR                 *p_err);


NET_CONN_ID       NetConn_ID_TransportGet           (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_ID_TransportSet           (NET_CONN_ID              conn_id,
                                                     NET_CONN_ID              conn_id_transport,
                                                     NET_ERR                 *p_err);


void              NetConn_AddrLocalGet              (NET_CONN_ID              conn_id,
                                                     CPU_INT08U              *p_addr_local,
                                                     NET_CONN_ADDR_LEN       *p_addr_len,
                                                     NET_ERR                 *p_err);

void              NetConn_AddrLocalSet              (NET_CONN_ID              conn_id,
                                                     NET_IF_NBR               if_nbr,
                                                     CPU_INT08U              *p_addr_local,
                                                     NET_CONN_ADDR_LEN        addr_len,
                                                     CPU_BOOLEAN              addr_over_wr,
                                                     NET_ERR                 *p_err);


void              NetConn_AddrRemoteGet             (NET_CONN_ID              conn_id,
                                                     CPU_INT08U              *p_addr_remote,
                                                     NET_CONN_ADDR_LEN       *p_addr_len,
                                                     NET_ERR                 *p_err);

void              NetConn_AddrRemoteSet             (NET_CONN_ID              conn_id,
                                                     CPU_INT08U              *p_addr_remote,
                                                     NET_CONN_ADDR_LEN        addr_len,
                                                     CPU_BOOLEAN              addr_over_wr,
                                                     NET_ERR                 *p_err);

CPU_BOOLEAN       NetConn_AddrRemoteCmp             (NET_CONN_ID              conn_id,
                                                     CPU_INT08U              *p_addr_remote,
                                                     NET_CONN_ADDR_LEN        addr_len,
                                                     NET_ERR                 *p_err);


#ifdef  NET_IPv4_MODULE_EN
void              NetConn_IPv4TxParamsGet           (NET_CONN_ID              conn_id,
                                                     NET_IPv4_FLAGS          *p_ip_flags,
                                                     NET_IPv4_TOS            *p_ip_tos,
                                                     NET_IPv4_TTL            *p_ip_ttl,
                                                     NET_ERR                 *p_err);



NET_IPv4_FLAGS    NetConn_IPv4TxFlagsGet            (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_IPv4TxFlagsSet            (NET_CONN_ID              conn_id,
                                                     NET_IPv4_FLAGS           ip_flags,
                                                     NET_ERR                 *p_err);


NET_IPv4_TOS      NetConn_IPv4TxTOS_Get             (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_IPv4TxTOS_Set             (NET_CONN_ID              conn_id,
                                                     NET_IPv4_TOS             ip_tos,
                                                     NET_ERR                 *p_err);

NET_IPv4_TTL      NetConn_IPv4TxTTL_Get             (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_IPv4TxTTL_Set             (NET_CONN_ID              conn_id,
                                                     NET_IPv4_TTL             ip_ttl,
                                                     NET_ERR                 *p_err);

#ifdef  NET_MCAST_TX_MODULE_EN
NET_IPv4_TTL      NetConn_IPv4TxTTL_MulticastGet    (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_IPv4TxTTL_MulticastSet    (NET_CONN_ID              conn_id,
                                                     NET_IPv4_TTL             ip_ttl,
                                                     NET_ERR                 *p_err);
#endif

#endif

#ifdef  NET_IPv6_MODULE_EN

void              NetConn_IPv6TxParamsGet           (NET_CONN_ID              conn_id,
                                                     NET_IPv6_TRAFFIC_CLASS  *p_ip_traffic_class,
                                                     NET_IPv6_FLOW_LABEL     *p_ip_flow_label,
                                                     NET_IPv6_HOP_LIM        *p_ip_hop_lim,
                                                     NET_IPv6_FLAGS          *p_ip_flags,
                                                     NET_ERR                 *p_err);

#ifdef  NET_MCAST_TX_MODULE_EN
NET_IPv6_HOP_LIM  NetConn_IPv6TxHopLimMcastGet      (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_IPv6TxHopLimMcastSet      (NET_CONN_ID              conn_id,
                                                     NET_IPv6_HOP_LIM         ip_hop_lim,
                                                     NET_ERR                 *p_err);
#endif

#endif


                                                                                /* -------- CONN STATUS FNCTS --------- */
CPU_BOOLEAN       NetConn_IsUsed                    (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

CPU_BOOLEAN       NetConn_IsIPv6                    (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

CPU_BOOLEAN       NetConn_IsPortUsed                (NET_PORT_NBR             port_nbr,
                                                     NET_PROTOCOL_TYPE        protocol,
                                                     NET_ERR                 *p_err);

void              NetConn_IsConn                    (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);


NET_STAT_POOL     NetConn_PoolStatGet               (void);

void              NetConn_PoolStatResetMaxUsed      (void);



                                                                                /* --------- CONN LIST FNCTS ---------- */
NET_CONN_ID       NetConn_Srch                      (NET_CONN_FAMILY          family,
                                                     NET_CONN_PROTOCOL_IX     protocol_ix,
                                                     CPU_INT08U              *p_addr_local,
                                                     CPU_INT08U              *p_addr_remote,
                                                     NET_CONN_ADDR_LEN        addr_len,
                                                     NET_CONN_ID             *p_conn_id_transport,
                                                     NET_CONN_ID             *p_conn_id_app,
                                                     NET_ERR                 *p_err);


void              NetConn_ListAdd                   (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);

void              NetConn_ListUnlink                (NET_CONN_ID              conn_id,
                                                     NET_ERR                 *p_err);


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

#ifndef  NET_CONN_NBR_CONN
#error  "NET_CONN_NBR_CONN            not #define'd in 'net_cfg_net.h'"
#error  "                           [MUST be  >= NET_CONN_NBR_MIN]    "
#error  "                           [     &&  <= NET_CONN_NBR_MAX]    "

#elif   DEF_CHK_VAL(NET_CONN_NBR_CONN,     \
                    NET_CONN_NBR_MIN,          \
                    NET_CONN_NBR_MAX) != DEF_OK
#error  "NET_CONN_NBR_CONN      illegally #define'd in 'net_cfg.h'"
#error  "                           [MUST be  >= NET_CONN_NBR_MIN]    "
#error  "                           [     &&  <= NET_CONN_NBR_MAX]    "
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_CONN_MODULE_PRESENT */

