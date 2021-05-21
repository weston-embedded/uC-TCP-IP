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
*                                         NETWORK SOCKET LAYER
*
* Filename  : net_sock.h
* Version   : V3.05.04
*********************************************************************************************************
* Note(s)   : (1) Supports BSD 4.x Socket Layer with the following restrictions/constraints :
*
*                 (a) ONLY supports a single address family from the following families :
*                     (1) IPv4 (AF_INET)
*
*                 (b) ONLY supports the following socket types :
*                     (1) Datagram (SOCK_DGRAM)
*                     (2) Stream   (SOCK_STREAM)
*
*                 (c) ONLY supports a single protocol family from the following families :
*                     (1) IPv4 (PF_INET)
*                         (A) ONLY supports the following protocols :
*                             (1) UDP (IPPROTO_UDP)
*                             (2) TCP (IPPROTO_TCP)
*
*                 (d) ONLY supports the following socket options :
*
*                         Blocking
*                         Secure (TLS/SSL)
*                         Rx Queue size
*                         Tx Queue size
*                         Time of server (IPv4-TOS)
*                         Time to life   (IPv4-TTL)
*                         Time to life multicast
*                         UDP connection receive         timeout
*                         TCP connection accept          timeout
*                         TCP connection close           timeout
*                         TCP connection connect request timeout
*                         TCP connection receive         timeout
*                         TCP connection transmit        timeout
*                         TCP keep alive
*                         TCP MSL
*                         Force connection using a specific Interface
*
*                 (e) Multiple socket connections with the same local & remote address -- both
*                     addresses & port numbers -- OR multiple socket connections with only a
*                     local address but the same local address -- both address & port number --
*                     is NOT currently supported.
*
*                     See 'NetSock_BindHandler()  Note #8'.
*
*********************************************************************************************************
* Notice(s) : (1) The Institute of Electrical and Electronics Engineers and The Open Group, have given
*                 us permission to reprint portions of their documentation.  Portions of this text are
*                 reprinted and reproduced in electronic form from the IEEE Std 1003.1, 2004 Edition,
*                 Standard for Information Technology -- Portable Operating System Interface (POSIX),
*                 The Open Group Base Specifications Issue 6, Copyright (C) 2001-2004 by the Institute
*                 of Electrical and Electronics Engineers, Inc and The Open Group.  In the event of any
*                 discrepancy between these versions and the original IEEE and The Open Group Standard,
*                 the original IEEE and The Open Group Standard is the referee document.  The original
*                 Standard can be obtained online at http://www.opengroup.org/unix/online.html.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_SOCK_MODULE_PRESENT
#define  NET_SOCK_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#endif

#include  "../IF/net_if.h"

#include  "net_def.h"
#include  "net_type.h"
#include  "net_stat.h"
#include  "net_err.h"


#include  <lib_def.h>
#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef NET_SOCK_MODULE
#define  NET_SOCK_EXT
#else
#define  NET_SOCK_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_SOCK_NBR_SOCK                          NET_SOCK_CFG_SOCK_NBR_UDP + \
                                                   (NET_SOCK_CFG_SOCK_NBR_TCP)

#if (NET_SOCK_CFG_SOCK_NBR_TCP > 0)
#define  NET_SOCK_CONN_NBR                          NET_SOCK_CFG_SOCK_NBR_UDP + \
                                                   (NET_SOCK_CFG_SOCK_NBR_TCP * NET_SOCK_CFG_CONN_ACCEPT_Q_SIZE_MAX)
#else
#define  NET_SOCK_CONN_NBR                          NET_SOCK_CFG_SOCK_NBR_UDP
#endif



#if (defined(NET_IPv6_MODULE_EN))
#define  NET_SOCK_ADDR_LEN_MAX                      NET_SOCK_ADDR_LEN_IP_V6

#elif (defined(NET_IPv4_MODULE_EN))
#define  NET_SOCK_ADDR_LEN_MAX                      NET_SOCK_ADDR_LEN_IP_V4

#else
#define  NET_SOCK_ADDR_LEN_MAX                      0
#endif



/*
*********************************************************************************************************
*                                  NETWORK SOCKET PORT NUMBER DEFINES
*
* Note(s) : (1) Socket port numbers defined in host-order.
*
*               See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*********************************************************************************************************
*/

#define  NET_SOCK_PORT_NBR_RESERVED                      NET_PORT_NBR_RESERVED
#define  NET_SOCK_PORT_NBR_NONE                          NET_SOCK_PORT_NBR_RESERVED
#define  NET_SOCK_PORT_NBR_RANDOM                        NET_SOCK_PORT_NBR_RESERVED

#ifndef  NET_SOCK_DFLT_NO_BLOCK_EN
    #define  NET_SOCK_DFLT_NO_BLOCK_EN  DEF_DISABLED
#endif

#ifndef  NET_SOCK_DFLT_PORT_NBR_RANDOM_BASE
    #define  NET_SOCK_DFLT_PORT_NBR_RANDOM_BASE             49152u
#endif

#ifndef  NET_SOCK_DFLT_PORT_NBR_RANDOM_END
    #define  NET_SOCK_DFLT_PORT_NBR_RANDOM_END              65535u
#endif

#define  NET_SOCK_PORT_NBR_RANDOM_MIN                    NET_SOCK_DFLT_PORT_NBR_RANDOM_BASE
#define  NET_SOCK_PORT_NBR_RANDOM_MAX                    NET_SOCK_DFLT_PORT_NBR_RANDOM_END


#ifndef  NET_SOCK_DFLT_TIMEOUT_RX_Q_MS
                                                                /* Configure socket timeout values (see Note #5) :      */
                                                                /* Configure (datagram) socket receive queue timeout.   */
    #define  NET_SOCK_DFLT_TIMEOUT_RX_Q_MS           5000u
#endif

#ifndef  NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS
                                                                /* Configure socket connection request timeout.         */
    #define  NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS       5000u
#endif


#ifndef  NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS
                                                                /* Configure socket connection accept  timeout.         */
    #define  NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS        5000u
#endif

#ifndef  NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS
                                                                /* Configure socket connection close   timeout.         */
    #define  NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS            10000u
#endif


/*
*********************************************************************************************************
*                                        NETWORK SOCKET STATES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                             NETWORK SOCKET BLOCKING MODE SELECT DEFINES
*
* Note(s) : (1) The following socket values MUST be pre-#define'd in 'net_def.h' PRIOR to 'net_cfg.h'
*               so that the developer can configure sockets for the desired socket blocing mode (see
*               'net_def.h  BSD 4.x & NETWORK SOCKET LAYER DEFINES  Note #1b' & 'net_cfg_net.h  NETWORK
*               SOCKET LAYER CONFIGURATION') :
*
*               (a) NET_SOCK_BLOCK_SEL_DFLT
*               (b) NET_SOCK_BLOCK_SEL_BLOCK
*               (c) NET_SOCK_BLOCK_SEL_NO_BLOCK
*
*           (2) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition, 6th Printing,
*               Section 6.2 'Blocking I/O Model', Page 154 states that "by default, all sockets are
*               blocking".
*********************************************************************************************************
*/

#if 0                                                           /* See Note #1.                                         */
                                                                /* ------------------ SOCK BLOCK SEL ------------------ */
#define  NET_SOCK_BLOCK_SEL_NONE                           0u
#define  NET_SOCK_BLOCK_SEL_DFLT                           1u   /* Sock block mode determined by run-time sock opts ... */
#define  NET_SOCK_BLOCK_SEL_BLOCK                          2u   /* ... but dflts to blocking (see Note #2).             */
#define  NET_SOCK_BLOCK_SEL_NO_BLOCK                       3u

#endif


/*
*********************************************************************************************************
*                                NETWORK SOCKET (OBJECT) FLAG DEFINES
*********************************************************************************************************
*/

                                                                /* ---------------- NET SOCK OBJ FLAGS ---------------- */
#define  NET_SOCK_FLAG_SOCK_NONE                  DEF_BIT_NONE
#define  NET_SOCK_FLAG_SOCK_USED                  DEF_BIT_08    /* Sock cur used; i.e. NOT in free sock pool.           */
#define  NET_SOCK_FLAG_SOCK_NO_BLOCK              MSG_DONTWAIT  /* Sock blocking DISABLED.                              */
#define  NET_SOCK_FLAG_SOCK_SECURE                DEF_BIT_09    /* Sock security ENABLED.                               */
#define  NET_SOCK_FLAG_SOCK_SECURE_NEGO           DEF_BIT_10


/*
*********************************************************************************************************
*                                  NETWORK SOCKET EVENT TYPE DEFINES
*
* Note(s) : (1) 'EVENT_TYPE' abbreviated to 'EVENT' to enforce ANSI-compliance of 31-character symbol
*                length uniqueness.
*********************************************************************************************************
*/

#define  NET_SOCK_EVENT_NONE                               0u
#define  NET_SOCK_EVENT_ERR                                1u


#define  NET_SOCK_EVENT_SOCK_RX                           10u
#define  NET_SOCK_EVENT_SOCK_TX                           11u
#define  NET_SOCK_EVENT_SOCK_ACCEPT                       12u
#define  NET_SOCK_EVENT_SOCK_CONN                         13u
#define  NET_SOCK_EVENT_SOCK_CLOSE                        14u

#define  NET_SOCK_EVENT_SOCK_ERR_RX                       20u
#define  NET_SOCK_EVENT_SOCK_ERR_TX                       21u
#define  NET_SOCK_EVENT_SOCK_ERR_ACCEPT                   22u
#define  NET_SOCK_EVENT_SOCK_ERR_CONN                     23u
#define  NET_SOCK_EVENT_SOCK_ERR_CLOSE                    24u


#define  NET_SOCK_EVENT_TRANSPORT_RX                      30u
#define  NET_SOCK_EVENT_TRANSPORT_TX                      31u

#define  NET_SOCK_EVENT_TRANSPORT_ERR_RX                  40u
#define  NET_SOCK_EVENT_TRANSPORT_ERR_TX                  41u


/*
*********************************************************************************************************
*                              NETWORK SOCKET FAMILY & PROTOCOL DEFINES
*
* Note(s) : (1) The following socket values MUST be pre-#define'd in 'net_def.h' PRIOR to 'net_cfg.h'
*               so that the developer can configure sockets for the correct socket family values (see
*               'net_def.h  BSD 4.x & NETWORK SOCKET LAYER DEFINES  Note #1' & 'net_cfg_net.h  NETWORK
*               SOCKET LAYER CONFIGURATION') :
*
*               (a) (1) NET_SOCK_ADDR_FAMILY_IP_V4
*                   (2) NET_SOCK_PROTOCOL_FAMILY_IP_V4
*                   (3) NET_SOCK_FAMILY_IP_V4
*
*               (b) (1) NET_SOCK_ADDR_LEN_IP_V4
*                   (2) NET_SOCK_PROTO_MAX_IP_V4
*
*           (2) 'NET_SOCK_PROTOCOL_MAX' abbreviated to 'NET_SOCK_PROTO_MAX' to enforce ANSI-compliance of
*                31-character symbol length uniqueness.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                   NETWORK SOCKET ADDRESS DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------ SOCK ADDR CFG ------------------- */

#define  NET_SOCK_ADDR_IP_LEN_PORT                  (sizeof(NET_PORT_NBR))

#define  NET_SOCK_ADDR_IP_IX_BASE                             0
#define  NET_SOCK_ADDR_IP_IX_PORT                           NET_SOCK_ADDR_IP_IX_BASE

                                                                /* ---------------- IPv4 SOCK ADDR CFG ---------------- */
#ifdef   NET_IPv4_MODULE_EN

#define  NET_SOCK_ADDR_IP_V4_LEN_ADDR               (sizeof(NET_IPv4_ADDR))
#define  NET_SOCK_ADDR_IP_V4_LEN_PORT_ADDR                  NET_SOCK_ADDR_IP_V4_LEN_ADDR + NET_SOCK_ADDR_IP_LEN_PORT

#define  NET_SOCK_ADDR_IP_V4_IX_ADDR                       (NET_SOCK_ADDR_IP_IX_PORT + NET_SOCK_ADDR_IP_LEN_PORT)

#define  NET_SOCK_ADDR_IP_V4_WILDCARD                       NET_IPv4_ADDR_NONE
#define  NET_SOCK_ADDR_IP_V4_BROADCAST                      INADDR_BROADCAST

#endif

                                                                /* ---------------- IPv6 SOCK ADDR CFG ---------------- */
#ifdef   NET_IPv6_MODULE_EN

#define  NET_SOCK_ADDR_IP_V6_LEN_FLOW               (sizeof(CPU_INT32U))
#define  NET_SOCK_ADDR_IP_V6_LEN_ADDR               (sizeof(NET_IPv6_ADDR))
#define  NET_SOCK_ADDR_IP_V6_LEN_PORT_ADDR                  NET_SOCK_ADDR_IP_V6_LEN_ADDR + NET_SOCK_ADDR_IP_LEN_PORT

#define  NET_SOCK_ADDR_IP_V6_IX_FLOW                       (NET_SOCK_ADDR_IP_IX_PORT + NET_SOCK_ADDR_IP_LEN_PORT)
#define  NET_SOCK_ADDR_IP_V6_IX_ADDR                       (NET_SOCK_ADDR_IP_V6_IX_FLOW + NET_SOCK_ADDR_IP_V6_LEN_FLOW)

#define  NET_SOCK_ADDR_IP_V6_WILDCARD                       NET_IPv6_ADDR_ANY

#endif


/*
*********************************************************************************************************
*                                     NETWORK SOCKET API DEFINES
*********************************************************************************************************
*/

#define  NET_SOCK_BSD_ERR_NONE                           NET_BSD_ERR_NONE
#define  NET_SOCK_BSD_ERR_DFLT                           NET_BSD_ERR_DFLT

#define  NET_SOCK_BSD_ERR_OPEN                           NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_CLOSE                          NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_BIND                           NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_CONN                           NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_LISTEN                         NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_ACCEPT                         NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_RX                             NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_TX                             NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_SEL                            NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_OPT_SET                        NET_SOCK_BSD_ERR_DFLT
#define  NET_SOCK_BSD_ERR_OPT_GET                        NET_SOCK_BSD_ERR_DFLT


#define  NET_SOCK_BSD_RTN_CODE_OK                        NET_BSD_RTN_CODE_OK
#define  NET_SOCK_BSD_RTN_CODE_TIMEOUT                   NET_BSD_RTN_CODE_TIMEOUT
#define  NET_SOCK_BSD_RTN_CODE_CONN_CLOSED               NET_BSD_RTN_CODE_CONN_CLOSED


/*
*********************************************************************************************************
*                               NETWORK SOCKET (ARGUMENT) FLAG DEFINES
*********************************************************************************************************
*/

#define  NET_SOCK_FLAG_NONE                       NET_SOCK_FLAG_SOCK_NONE

#define  NET_SOCK_FLAG_RX_DATA_PEEK               MSG_PEEK

#define  NET_SOCK_FLAG_NO_BLOCK                   MSG_DONTWAIT
#define  NET_SOCK_FLAG_RX_NO_BLOCK                NET_SOCK_FLAG_NO_BLOCK
#define  NET_SOCK_FLAG_TX_NO_BLOCK                NET_SOCK_FLAG_NO_BLOCK


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                               NETWORK SOCKET ADDRESS FAMILY DATA TYPE
*********************************************************************************************************
*/




#define  NET_SOCK_ADDR_FAMILY_IP_V4         AF_INET             /* TCP/IPv4 sock addr     family type.                  */
#define  NET_SOCK_ADDR_FAMILY_IP_V6         AF_INET6            /* TCP/IPv6 sock addr     family type.                  */

typedef  CPU_INT16U  NET_SOCK_ADDR_FAMILY;



/*
*********************************************************************************************************
*                               NETWORK SOCKET ADDRESS LENGTH DATA TYPE
*********************************************************************************************************
*/

typedef CPU_INT08U NET_SOCK_ADDR_LEN;


/*
*********************************************************************************************************
*                              NETWORK SOCKET PROTOCOL FAMILY DATA TYPE
*********************************************************************************************************
*/

typedef  enum  net_sock_protocol_family {
    NET_SOCK_PROTOCOL_FAMILY_NONE  = 0,
    NET_SOCK_PROTOCOL_FAMILY_IP_V4 = PF_INET,                   /* TCP/IPv4 sock protocol family type.                  */
    NET_SOCK_PROTOCOL_FAMILY_IP_V6 = PF_INET6                   /* TCP/IPv6 sock protocol family type.                  */
} NET_SOCK_PROTOCOL_FAMILY;


/*
*********************************************************************************************************
*                              NETWORK SOCKET FAMILY DATA TYPE
*********************************************************************************************************
*/

typedef  enum  net_sock_family {
    NET_SOCK_FAMILY_IP_V4 = NET_SOCK_PROTOCOL_FAMILY_IP_V4,
    NET_SOCK_FAMILY_IP_V6 = NET_SOCK_PROTOCOL_FAMILY_IP_V6
} NET_SOCK_FAMILY;


/*
*********************************************************************************************************
*                                  NETWORK SOCKET PROTOCOL DATA TYPE
*********************************************************************************************************
*/


typedef  enum  net_sock_protocol {
    NET_SOCK_PROTOCOL_DFLT = 0,
    NET_SOCK_PROTOCOL_NONE = NET_SOCK_PROTOCOL_DFLT,

    NET_SOCK_PROTOCOL_TCP  = IPPROTO_TCP,
    NET_SOCK_PROTOCOL_UDP  = IPPROTO_UDP,
    NET_SOCK_PROTOCOL_IP   = IPPROTO_IP,
    NET_SOCK_PROTOCOL_SOCK = SOL_SOCKET
} NET_SOCK_PROTOCOL;


/*
*********************************************************************************************************
*                                    NETWORK SOCKET TYPE DATA TYPE
*********************************************************************************************************
*/

typedef  enum net_sock_type {
    NET_SOCK_TYPE_NONE     =  0,
    NET_SOCK_TYPE_FAULT    = -1,
    NET_SOCK_TYPE_DATAGRAM = SOCK_DGRAM,
    NET_SOCK_TYPE_STREAM   = SOCK_STREAM
} NET_SOCK_TYPE;

#if 0
/* -------------------- SOCK TYPES -------------------- */
#define  NET_SOCK_TYPE_NONE                                0
#define  NET_SOCK_TYPE_FAULT                              -1

#if 0
#define  NET_SOCK_TYPE_DATAGRAM                         SOCK_DGRAM
#define  NET_SOCK_TYPE_STREAM                           SOCK_STREAM
#endif

typedef  CPU_INT16S  NET_SOCK_TYPE;
#endif


/*
*********************************************************************************************************
*                     NETWORK SOCKET DATA LENGTH & (ERROR) RETURN CODE DATA TYPES
*
* Note(s) : (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/types.h : DESCRIPTION' states that :
*
*               (a) "ssize_t - Used for a count of bytes or an error indication."
*
*               (b) "ssize_t shall be [a] signed integer type ... capable of storing values at least in
*                    the range [-1, {SSIZE_MAX}]."
*
*                   (1) IEEE Std 1003.1, 2004 Edition, Section 'limits.h : DESCRIPTION' states that the
*                       "Minimum Acceptable Value ... [for] {SSIZE_MAX}" is "32767".
*
*                   (2) To avoid possible integer overflow, the network socket return code data type MUST
*                       be declared as a signed integer data type with a maximum positive value greater
*                       than or equal to all transport layers' maximum positive return value.
*
*                       See also 'net_udp.c  NetUDP_RxAppData()      Return(s)',
*                                'net_udp.c  NetUDP_TxAppData()      Return(s)',
*                                'net_tcp.c  NetTCP_RxAppData()      Return(s)',
*                              & 'net_tcp.c  NetTCP_TxConnAppData()  Return(s)'.
*
*           (2) NET_SOCK_DATA_SIZE_MAX  SHOULD be #define'd based on 'NET_SOCK_DATA_SIZE' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT32S          NET_SOCK_DATA_SIZE;
typedef  NET_SOCK_DATA_SIZE  NET_SOCK_RTN_CODE;

#define  NET_SOCK_DATA_SIZE_MIN                            0
#define  NET_SOCK_DATA_SIZE_MAX          DEF_INT_32S_MAX_VAL    /* See Note #2.                                         */


/*
*********************************************************************************************************
*                                   NETWORK SOCKET STATE DATA TYPE
*********************************************************************************************************
*/

/* typedef  CPU_INT08U  NET_SOCK_STATE; */
typedef enum net_sock_state {
    NET_SOCK_STATE_NONE               =  1u,

    NET_SOCK_STATE_FREE               =  2u,
    NET_SOCK_STATE_DISCARD            =  3u,
    NET_SOCK_STATE_CLOSED             = 10u,
    NET_SOCK_STATE_CLOSED_FAULT       = 11u,
    NET_SOCK_STATE_CLOSE_IN_PROGRESS  = 15u,
    NET_SOCK_STATE_CLOSING_DATA_AVAIL = 16u,
    NET_SOCK_STATE_BOUND              = 20u,
    NET_SOCK_STATE_LISTEN             = 30u,
    NET_SOCK_STATE_CONN               = 40u,
    NET_SOCK_STATE_CONN_IN_PROGRESS   = 41u,
    NET_SOCK_STATE_CONN_DONE          = 42u
} NET_SOCK_STATE;

/*
*********************************************************************************************************
*                               NETWORK SOCKET SHUTDOWN MODE DATA TYPE
*********************************************************************************************************
*/
typedef enum net_sock_sd_mode {
    NET_SOCK_SHUTDOWN_MODE_RD         = SHUT_RD,
    NET_SOCK_SHUTDOWN_MODE_WR         = SHUT_WR,
    NET_SOCK_SHUTDOWN_MODE_RDWR       = SHUT_RDWR,
    NET_SOCK_SHUTDOWN_MODE_NONE       = 3u
} NET_SOCK_SD_MODE;

/*
*********************************************************************************************************
*                                  NETWORK SOCKET QUANTITY DATA TYPE
*
* Note(s) : (1) See also 'NETWORK SOCKET IDENTIFICATION DATA TYPE  Note #1'.
*********************************************************************************************************
*/

typedef  CPU_INT16S  NET_SOCK_QTY;                              /* Defines max qty of socks to support.                 */


/*
*********************************************************************************************************
*                               NETWORK SOCKET IDENTIFICATION DATA TYPE
*
* Note(s) : (1) (a) NET_SOCK_NBR_MAX  SHOULD be #define'd based on 'NET_SOCK_QTY' data type declared.
*
*               (b) However, since socket handle identifiers are data-typed as 16-bit signed integers;
*                   the maximum unique number of valid socket handle identifiers, & therefore the
*                   maximum number of valid sockets, is the total number of non-negative values that
*                   16-bit signed integers support.
*********************************************************************************************************
*/

typedef  CPU_INT16S  NET_SOCK_ID;

#define  NET_SOCK_NBR_MIN                                  1
#define  NET_SOCK_NBR_MAX                DEF_INT_16S_MAX_VAL    /* See Note #1.                                         */

#define  NET_SOCK_ID_NONE                                 -1
#define  NET_SOCK_ID_MIN                                   0
#define  NET_SOCK_ID_MAX          (NET_SOCK_NBR_SOCK - 1)


/*
*********************************************************************************************************
*                                   NETWORK SOCKET FLAGS DATA TYPES
*
* Note(s) : (1) Ideally, network socket API argument flags data type SHOULD be defined as   an unsigned
*               integer data type since logical bitwise operations should be performed ONLY on unsigned
*               integer data types.
*********************************************************************************************************
*/

typedef  NET_FLAGS   NET_SOCK_FLAGS;
typedef  CPU_INT16S  NET_SOCK_API_FLAGS;                        /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                 NETWORK SOCKET OPTION NAME DATA TYPES
*********************************************************************************************************
*/

typedef  enum  net_sock_opt_name {
    NET_SOCK_OPT_SOCK_TX_BUF_SIZE            = SO_SNDBUF,
    NET_SOCK_OPT_SOCK_RX_BUF_SIZE            = SO_RCVBUF,
    NET_SOCK_OPT_SOCK_RX_TIMEOUT             = SO_RCVTIMEO,
    NET_SOCK_OPT_SOCK_TX_TIMEOUT             = SO_SNDTIMEO,
    NET_SOCK_OPT_SOCK_ERROR                  = SO_ERROR,
    NET_SOCK_OPT_SOCK_TYPE                   = SO_TYPE,
    NET_SOCK_OPT_SOCK_KEEP_ALIVE             = SO_KEEPALIVE,
    NET_SOCK_OPT_SOCK_ACCEPT_CONN            = SO_ACCEPTCONN,

    NET_SOCK_OPT_TCP_NO_DELAY                = TCP_NODELAY,
    NET_SOCK_OPT_TCP_KEEP_CNT                = TCP_KEEPCNT,
    NET_SOCK_OPT_TCP_KEEP_IDLE               = TCP_KEEPIDLE,
    NET_SOCK_OPT_TCP_KEEP_INTVL              = TCP_KEEPINTVL,

    NET_SOCK_OPT_IP_TOS                      = IP_TOS,
    NET_SOCK_OPT_IP_TTL                      = IP_TTL,
    NET_SOCK_OPT_IP_RX_IF                    = IP_RECVIF,
    NET_SOCK_OPT_IP_OPT                      = IP_OPTIONS,
    NET_SOCK_OPT_IP_HDR_INCL                 = IP_HDRINCL,
    NET_SOCK_OPT_IP_ADD_MEMBERSHIP           = IP_ADD_MEMBERSHIP,
    NET_SOCK_OPT_IP_DROP_MEMBERSHIP          = IP_DROP_MEMBERSHIP
} NET_SOCK_OPT_NAME;




/*
*********************************************************************************************************
*                               NETWORK SOCKET ADDRESS LENGTH DATA TYPE
*
* Note(s) : (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/socket.h : DESCRIPTION' states that
*               "socklen_t ... is an integer type of width of at least 32 bits".
*********************************************************************************************************
*/

typedef  CPU_INT32S  NET_SOCK_OPT_LEN;


/*
*********************************************************************************************************
*                                 NETWORK SOCKET QUEUE SIZE DATA TYPE
*
* Note(s) : (1) (a) NET_SOCK_Q_SIZE #define's SHOULD be #define'd based on 'NET_SOCK_Q_SIZE'
*                   data type declared.
*
*               (b) However, since socket/connection handle identifiers are data-typed as 16-bit
*                   signed integers; the maximum unique number of valid socket/connection handle
*                   identifiers, & therefore the maximum number of valid sockets/connections, is
*                   the total number of non-negative values that 16-bit signed integers support.
*
*                   See also             'NETWORK SOCKET     IDENTIFICATION DATA TYPE  Note #1b'
*                          & 'net_conn.h  NETWORK CONNECTION IDENTIFICATION DATA TYPE  Note #2b'.
*
*           (2) (a) NET_SOCK_Q_IX   #define's SHOULD be #define'd based on 'NET_SOCK_Q_SIZE'
*                   data type declared.
*
*               (b) Since socket queue size is data typed as a 16-bit unsigned integer but the
*                   maximum queue sizes are #define'd as 16-bit signed integer values ... :
*
*                   (1) Valid socket queue indices are #define'd within the range of     16-bit
*                         signed integer values, ...
*                   (2) but   socket queue indice exception values may be #define'd with 16-bit
*                       unsigned integer values.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_SOCK_Q_SIZE;

                                                                /* See Note #1.                                         */
#define  NET_SOCK_Q_SIZE_NONE                              0
#define  NET_SOCK_Q_SIZE_UNLIMITED                         0
#define  NET_SOCK_Q_SIZE_MIN                NET_SOCK_NBR_MIN
#define  NET_SOCK_Q_SIZE_MAX                NET_SOCK_NBR_MAX    /* See Note #1b.                                        */

                                                                /* See Note #2.                                         */
#define  NET_SOCK_Q_IX_NONE              DEF_INT_16U_MAX_VAL    /* See Note #2b.                                        */
#define  NET_SOCK_Q_IX_MIN                                 0
#define  NET_SOCK_Q_IX_MAX          (NET_SOCK_Q_SIZE_MAX - 1)


/*
*********************************************************************************************************
*                                  NETWORK SOCKET ADDRESS DATA TYPES
*
* Note(s) : (1) See 'net_sock.h  Note #1a' for supported socket address families.
*
*           (2) (a) Socket address structure 'AddrFamily' member MUST be configured in host-order & MUST
*                   NOT be converted to/from network-order.
*
*               (b) Socket address structure addresses MUST be configured/converted from host-order to
*                   network-order.
*
*               See also 'net_bsd.h  BSD 4.x SOCKET DATA TYPES  Note #2b'.
*********************************************************************************************************
*/

                                                                          /* ------------ NET SOCK ADDR IPv4 ------------ */
#define  NET_SOCK_ADDR_IPv4_NBR_OCTETS_UNUSED        8

typedef  struct  net_sock_addr_ipv4 {
    NET_SOCK_ADDR_FAMILY    AddrFamily;                                   /* Sock addr family type (see Note #2a).        */
    NET_PORT_NBR            Port;                                         /* UDP/TCP port nbr      (see Note #2b).        */
    NET_IPv4_ADDR           Addr;                                         /* IPv6 addr             (see Note #2b).        */
    CPU_INT08U              Unused[NET_SOCK_ADDR_IPv4_NBR_OCTETS_UNUSED]; /* Unused (MUST be zero).                       */
} NET_SOCK_ADDR_IPv4;

#define  NET_SOCK_ADDR_IPv4_SIZE            (sizeof(NET_SOCK_ADDR_IPv4))


                                                                            /* ------------ NET SOCK ADDR IPv6 ------------ */

typedef  struct  net_sock_addr_ipv6 {
    NET_SOCK_ADDR_FAMILY    AddrFamily;                                     /* Sock addr family type (see Note #2a).                */
    NET_PORT_NBR            Port;                                           /* UDP/TCP port nbr      (see Note #2b).                */
    CPU_INT32U              FlowInfo;
    NET_IPv6_ADDR           Addr;                                           /* IPv6 addr               (see Note #2b).              */
    CPU_INT32U              ScopeID;                                        /* Unused (MUST be zero).                               */
} NET_SOCK_ADDR_IPv6;

#define  NET_SOCK_ADDR_IPv6_SIZE            (sizeof(NET_SOCK_ADDR_IPv6))



                                                                        /* -------------- NET SOCK ADDR --------------- */
#if   (defined(NET_IPv6_MODULE_EN))
    #define  NET_SOCK_BSD_ADDR_LEN_MAX                  (NET_SOCK_ADDR_IPv6_SIZE  - sizeof(NET_SOCK_ADDR_FAMILY))

#elif (defined(NET_IPv4_MODULE_EN))

    #define  NET_SOCK_BSD_ADDR_LEN_MAX                  (NET_SOCK_ADDR_IPv4_SIZE  - sizeof(NET_SOCK_ADDR_FAMILY))

#else
    #define  NET_SOCK_BSD_ADDR_LEN_MAX                  0
#endif


typedef  struct  net_sock_addr {
    NET_SOCK_ADDR_FAMILY    AddrFamily;                                 /* Sock addr family type (see Note #2a).        */
    CPU_INT08U              Addr[NET_SOCK_BSD_ADDR_LEN_MAX];            /* Sock addr             (see Note #2b).        */
} NET_SOCK_ADDR;

#define  NET_SOCK_ADDR_SIZE                 (sizeof(NET_SOCK_ADDR))


/*
*********************************************************************************************************
*                                  NETWORK SOCKET ACCEPT Q DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_sock_accept_q_obj {
            NET_CONN_ID             ConnID;
            CPU_BOOLEAN             IsRdy;
    struct  net_sock_accept_q_obj  *Next;
} NET_SOCK_ACCEPT_Q_OBJ;


/*
*********************************************************************************************************
*                                   NETWORK SOCKET SEL EVENT DATA TYPES
*********************************************************************************************************
*/

typedef  enum  net_sock_msg_type {
    NET_SOCK_EVENT_TYPE_CONN_REQ_SIGNAL,
    NET_SOCK_EVENT_TYPE_CONN_REQ_ABORT,
    NET_SOCK_EVENT_TYPE_CONN_CLOSE_ABORT,
    NET_SOCK_EVENT_TYPE_CONN_CLOSE_SIGNAL,
    NET_SOCK_EVENT_TYPE_CONN_ACCEPT_SIGNAL,
    NET_SOCK_EVENT_TYPE_CONN_ACCEPT_ABORT,
    NET_SOCK_EVENT_TYPE_RX_ABORT,
    NET_SOCK_EVENT_TYPE_RX,
    NET_SOCK_EVENT_TYPE_TX,
    NET_SOCK_EVENT_TYPE_SEL_ABORT
} NET_SOCK_EVENT_TYPE;

typedef  CPU_INT08U  NET_SOCK_SEL_EVENT_FLAG;

#define  NET_SOCK_SEL_EVENT_FLAG_NONE           DEF_BIT_NONE
#define  NET_SOCK_SEL_EVENT_FLAG_RD             DEF_BIT_00
#define  NET_SOCK_SEL_EVENT_FLAG_WR             DEF_BIT_01
#define  NET_SOCK_SEL_EVENT_FLAG_ERR            DEF_BIT_02


/*
*********************************************************************************************************
*                                      NETWORK SOCKET DATA TYPE
*
*                             NET_SOCK
*                          |-------------|
*                          |  Sock Type  |
*                          |-------------|      Next
*                          |      O----------> Socket     Buffer Queue
*                          |-------------|                    Heads      -------
*                          |      O------------------------------------> |     |
*                          |-------------|                               |     |
*                          |      O----------------------                -------
*                          |-------------|              |                  | ^
*                          |  Conn IDs   |              |                  v |
*                          |-------------|              |                -------
*                          |    Sock     |              |                |     |
*                          |   Family/   |              |                |     |
*                          |  Protocol   |              |                -------
*                          |-------------|              |                  | ^
*                          | Conn Ctrls  |              | Buffer Queue     v |
*                          |-------------|              |     Tails      -------
*                          |    Flags    |              ---------------> |     |
*                          |-------------|                               |     |
*                          |    State    |                               -------
*                          |-------------|
*
*
* Note(s) : (1) (a) 'TxQ_Head'/'TxQ_Tail' may NOT be necessary but are included for consistency.
*               (b) 'TxQ_SizeCur'         may NOT be necessary but is  included for consistency.
*********************************************************************************************************
*/
typedef  struct  net_sock_sel_obj  NET_SOCK_SEL_OBJ;

struct  net_sock_sel_obj {
        KAL_SEM_HANDLE            SockSelTaskSignalObj;
        NET_SOCK_SEL_EVENT_FLAG   SockSelPendingFlags;
        NET_SOCK_SEL_OBJ         *ObjPrevPtr;
};

                                                                        /* ----------------- NET SOCK ----------------- */
typedef  struct  net_sock  NET_SOCK;

struct  net_sock {
    NET_SOCK                   *NextPtr;                                /* Ptr to NEXT sock.                            */

#if (NET_SOCK_CFG_SEL_EN == DEF_ENABLED)
    NET_SOCK_SEL_OBJ           *SelObjTailPtr;
#endif

    KAL_SEM_HANDLE              RxQ_SignalObj;
    CPU_INT32U                  RxQ_SignalTimeout_ms;

    NET_BUF                    *RxQ_Head;                               /* Ptr to head of sock's datagram rx buf Q.     */
    NET_BUF                    *RxQ_Tail;                               /* Ptr to tail of sock's datagram rx buf Q.     */
#if 0                                                                   /* See Note #2a.                                */
    NET_BUF                    *TxQ_Head;                               /* Ptr to head of sock's datagram tx buf Q.     */
    NET_BUF                    *TxQ_Tail;                               /* Ptr to tail of sock's datagram tx buf Q.     */
#endif

    NET_SOCK_DATA_SIZE          RxQ_SizeCfgd;                           /* Datagram rx buf Q size cfg'd (in octets).    */
    NET_SOCK_DATA_SIZE          RxQ_SizeCur;                            /* Datagram rx buf Q size cur   (in octets).    */

    NET_SOCK_DATA_SIZE          TxQ_SizeCfgd;                           /* Datagram tx buf Q size cfg'd (in octets).    */
#if 0                                                                   /* See Note #2b.                                */
    NET_SOCK_DATA_SIZE          TxQ_SizeCur;                            /* Datagram tx buf Q size cur   (in octets).    */
#endif


    NET_SOCK_ID                 ID;                                     /* Sock        id.                              */
#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
    NET_SOCK_ID                 ID_SockParent;                          /* Parent sock id.                              */
#endif
    NET_CONN_ID                 ID_Conn;                                /* Conn        id.                              */

    NET_IF_NBR                  IF_Nbr;                                 /* IF nbr.                                      */


    NET_SOCK_PROTOCOL_FAMILY    ProtocolFamily;                         /* Sock protocol family.                        */
    NET_SOCK_PROTOCOL           Protocol;                               /* Sock protocol.                               */
    NET_SOCK_TYPE               SockType;                               /* Sock type.                                   */


#ifdef  NET_SECURE_MODULE_EN
    void                       *SecureSession;                          /* Sock secure session.                         */
#endif

#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
    KAL_SEM_HANDLE              ConnReqSignalObj;
    CPU_INT32U                  ConnReqSignalTimeout_ms;

    KAL_SEM_HANDLE              ConnAcceptQSignalObj;
    CPU_INT32U                  ConnAcceptQSignalTimeout_ms;

    KAL_SEM_HANDLE              ConnCloseSignalObj;
    CPU_INT32U                  ConnCloseSignalTimeout_ms;

    NET_SOCK_ACCEPT_Q_OBJ      *ConnAcceptQ_Ptr;

    NET_SOCK_Q_SIZE             ConnAcceptQ_SizeMax;                    /* Max Q size to accept rx'd conn reqs.         */
    NET_SOCK_Q_SIZE             ConnAcceptQ_SizeCur;                    /* Cur Q size to accept rx'd conn reqs.         */

    NET_SOCK_Q_SIZE             ConnChildQ_SizeMax;                     /* Max Q size to child conn.                    */
    NET_SOCK_Q_SIZE             ConnChildQ_SizeCur;                     /* Cur Q size to child conn.                    */                                                                        /* Conn accept Q (conn id's q'd into array).    */
#endif


    NET_SOCK_STATE              State;                                  /* Sock state.                                  */
    NET_SOCK_SD_MODE            ShutdownMode;                           /* Indicates if sock is fully or part shut down.*/
    NET_SOCK_FLAGS              Flags;                                  /* Sock flags.                                  */
};


/*
*********************************************************************************************************
*                                  NETWORK SOCKET TIMEOUT DATA TYPE
*
* Note(s) : (1) (a) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states that "the
*                   timeval structure ... includes at least the following members" :
*
*                   (1) time_t         tv_sec     Seconds
*                   (2) suseconds_t    tv_usec    Microseconds
*
*               (b) Ideally, the Network Socket Layer's 'NET_SOCK_TIMEOUT' data type would be based on the
*                   BSD 4.x Layer's 'timeval' data type definition.  However, since BSD 4.x Layer application
*                   programming interface (API) is NOT guaranteed to be present in the project build (see
*                   'net_bsd.h  MODULE  Note #1bA'); the Network Socket Layer's 'NET_SOCK_TIMEOUT' data type
*                   MUST be independently defined.
*
*                   However, for correct interoperability between the BSD 4.x Layer 'timeval' data type &
*                   the Network Socket Layer's 'NET_SOCK_TIMEOUT' data type; ANY modification to either of
*                   these data types MUST be appropriately synchronized.
*
*               See also 'net_bsd.h  BSD 4.x SOCKET DATA TYPES  Note #4'.
*********************************************************************************************************
*/

typedef  struct  net_sock_timeout {                                     /* See Note #1a.                                */
    CPU_INT32S      timeout_sec;
    CPU_INT32S      timeout_us;
} NET_SOCK_TIMEOUT;


/*
*********************************************************************************************************
*                      NETWORK SOCKET (IDENTIFICATION) DESCRIPTOR SET DATA TYPE
*
* Note(s) : (1) (a) (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states
*                       that the "'fd_set' type ... shall [be] define[d] ... as a structure".
*
*                   (2) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                       6th Printing, Section 6.3, Pages 162-163 states that "descriptor sets [are]
*                       typically an array of integers, with each bit in each integer corresponding
*                       to a descriptor".
*
*               (b) Ideally, the Network Socket Layer's 'NET_SOCK_DESC' data type would be based on
*                   the BSD 4.x Layer's 'fd_set' data type definition.  However, since BSD 4.x Layer
*                   application programming interface (API) is NOT guaranteed to be present in the
*                   project build (see 'net_bsd.h  MODULE  Note #1bA'); the Network Socket Layer's
*                   'NET_SOCK_DESC' data type MUST be independently defined.
*
*                   However, for correct interoperability between the BSD 4.x Layer 'fd_set' data type
*                   & the Network Socket Layer's 'NET_SOCK_DESC' data type; ANY modification to either
*                   of these data types MUST be appropriately synchronized.
*
*               See also 'net_bsd.h  BSD 4.x SOCKET DATA TYPES  Note #5'.
*********************************************************************************************************
*/

#define  NET_SOCK_DESC_NBR_MIN_DESC                               0
#define  NET_SOCK_DESC_NBR_MAX_DESC           NET_SOCK_NBR_SOCK

#define  NET_SOCK_DESC_NBR_MIN                                             0
#define  NET_SOCK_DESC_NBR_MAX               (NET_SOCK_DESC_NBR_MAX_DESC - 1)

#define  NET_SOCK_DESC_ARRAY_SIZE          (((NET_SOCK_DESC_NBR_MAX_DESC - 1) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)) + 1)

typedef  struct  net_sock_desc {                                                /* See Note #1a.                        */
    CPU_DATA        SockID_DescNbrSet[NET_SOCK_DESC_ARRAY_SIZE];
} NET_SOCK_DESC;


/*
*********************************************************************************************************
*                       NETWORK SOCKET SECURITY CERTIFICATE & KEY INSTALLATION DEFINES
*********************************************************************************************************
*/

typedef  enum  NET_SOCK_SECURE_TYPE {
    NET_SOCK_SECURE_TYPE_NONE,
    NET_SOCK_SECURE_TYPE_SERVER,
    NET_SOCK_SECURE_TYPE_CLIENT
} NET_SOCK_SECURE_TYPE;


typedef enum net_sock_secure_cert_key_fmt {
    NET_SOCK_SECURE_CERT_KEY_FMT_NONE,
    NET_SOCK_SECURE_CERT_KEY_FMT_PEM,
    NET_SOCK_SECURE_CERT_KEY_FMT_DER
} NET_SOCK_SECURE_CERT_KEY_FMT;


/*
*********************************************************************************************************
*                          NETWORK SECURE SOCKET FUNCTION POINTER DATA TYPE
*********************************************************************************************************
*/

typedef enum net_sock_secure_untrusted_reason {
    NET_SOCK_SECURE_UNTRUSTED_BY_CA,
    NET_SOCK_SECURE_EXPIRE_DATE,
    NET_SOCK_SECURE_INVALID_DATE,
    NET_SOCK_SECURE_SELF_SIGNED,
    NET_SOCK_SECURE_UNKNOWN
} NET_SOCK_SECURE_UNTRUSTED_REASON;

typedef  CPU_BOOLEAN  (*NET_SOCK_SECURE_TRUST_FNCT)(void                              *,
                                                    NET_SOCK_SECURE_UNTRUSTED_REASON   );

/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

NET_SOCK_EXT  NET_SOCK          *NetSock_PoolPtr;                       /* Ptr to pool of free socks.                   */
NET_SOCK_EXT  NET_STAT_POOL      NetSock_PoolStat;



/*
*********************************************************************************************************
*********************************************************************************************************
*                                              MACRO'S
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  NETWORK SOCKET DESCRIPTOR MACRO'S
*
* Description : Initialize, modify, & check network socket descriptor sets for multiplexed I/O functions.
*
* Argument(s) : desc_nbr    Socket descriptor number to initialize, modify, or check; when applicable.
*
*               p_desc_set  Pointer to a descriptor set.
*
* Return(s)   : Return values macro-dependent :
*
*                   none, for    network socket descriptor initialization & modification macro's.
*
*                   1,    if any network socket descriptor condition(s) satisfied.
*
*                   0,    otherwise.
*
*               See also 'net_bsd.h  BSD 4.x FILE DESCRIPTOR MACRO'S  Note #2a2'.
*
* Caller(s)   : Application.
*
*               These macro's are network protocol suite application programming interface (API) macro's
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Ideally, network socket descriptor macro's ('NET_SOCK_DESC_&&&()') would be based
*                   on the BSD 4.x Layer's file descriptor macro ('FD_&&&()') definitions.  However,
*                   since  BSD 4.x Layer application programming interface (API) is NOT guaranteed to
*                   be present in the project build (see 'net_bsd.h  MODULE  Note #1bA'); the network
*                   socket descriptor macro's MUST be independently defined.
*
*                   However, for correct interoperability between network socket descriptor macro's
*                   & BSD 4.x Layer file descriptor macro's; ANY modification to any of these macro
*                   definitions MUST be appropriately synchronized.
*
*                   See also 'net_bsd.h  BSD 4.x FILE DESCRIPTOR MACRO'S  Note #3'.
*********************************************************************************************************
*/

#define  NET_SOCK_DESC_COPY(p_desc_set_dest, p_desc_set_src)                                                                       \
                                do {                                                                                               \
                                    if ((((NET_SOCK_DESC *)(p_desc_set_dest)) != (NET_SOCK_DESC *)0) &&                            \
                                        (((NET_SOCK_DESC *)(p_desc_set_src )) != (NET_SOCK_DESC *)0)) {                            \
                                        Mem_Copy((void     *)     (&(((NET_SOCK_DESC *)(p_desc_set_dest))->SockID_DescNbrSet[0])), \
                                                 (void     *)     (&(((NET_SOCK_DESC *)(p_desc_set_src ))->SockID_DescNbrSet[0])), \
                                                 (CPU_SIZE_T)(sizeof(((NET_SOCK_DESC *)(p_desc_set_dest))->SockID_DescNbrSet)));   \
                                    }                                                                                              \
                                } while (0)

#define  NET_SOCK_DESC_INIT(p_desc_set)                                                                                       \
                                do {                                                                                          \
                                    if  (((NET_SOCK_DESC *)(p_desc_set)) != (NET_SOCK_DESC *)0) {                             \
                                        Mem_Clr ((void     *)     (&(((NET_SOCK_DESC *)(p_desc_set))->SockID_DescNbrSet[0])), \
                                                 (CPU_SIZE_T)(sizeof(((NET_SOCK_DESC *)(p_desc_set))->SockID_DescNbrSet)));   \
                                    }                                                                                         \
                                } while (0)


#define  NET_SOCK_DESC_CLR(desc_nbr, p_desc_set)                                                                                                                                \
                                            do {                                                                                                                               \
                                                if (((desc_nbr) >= NET_SOCK_DESC_NBR_MIN) &&                                                                                   \
                                                    ((desc_nbr) <= NET_SOCK_DESC_NBR_MAX) &&                                                                                   \
                                                                    (((NET_SOCK_DESC *)(p_desc_set)) != (NET_SOCK_DESC *)0)) {                                                  \
                                                     DEF_BIT_CLR   ((((NET_SOCK_DESC *)(p_desc_set))->SockID_DescNbrSet[(desc_nbr) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)]), \
                                                     DEF_BIT                                                           ((desc_nbr) % (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS))); \
                                                }                                                                                                                              \
                                            } while (0)

#define  NET_SOCK_DESC_SET(desc_nbr, p_desc_set)                                                                                                                                \
                                            do {                                                                                                                               \
                                                if (((desc_nbr) >= NET_SOCK_DESC_NBR_MIN) &&                                                                                   \
                                                    ((desc_nbr) <= NET_SOCK_DESC_NBR_MAX) &&                                                                                   \
                                                                    (((NET_SOCK_DESC *)(p_desc_set)) != (NET_SOCK_DESC *)0)) {                                                  \
                                                     DEF_BIT_SET   ((((NET_SOCK_DESC *)(p_desc_set))->SockID_DescNbrSet[(desc_nbr) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)]), \
                                                     DEF_BIT                                                           ((desc_nbr) % (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS))); \
                                                }                                                                                                                              \
                                            } while (0)

#define  NET_SOCK_DESC_IS_SET(desc_nbr, p_desc_set)                                                                                                                             \
                                                  ((((desc_nbr) >= NET_SOCK_DESC_NBR_MIN) &&                                                                                   \
                                                    ((desc_nbr) <= NET_SOCK_DESC_NBR_MAX) &&                                                                                   \
                                                                    (((NET_SOCK_DESC *)(p_desc_set)) != (NET_SOCK_DESC *)0)) ?                                                  \
                                                  (((DEF_BIT_IS_SET((((NET_SOCK_DESC *)(p_desc_set))->SockID_DescNbrSet[(desc_nbr) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)]), \
                                                     DEF_BIT                                                           ((desc_nbr) % (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)))) \
                                                  == DEF_YES) ? 1 : 0)                                                                                                         \
                                                                  : 0)


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*
* Note(s) : (1) Ideally, socket data handler functions should be defined as local functions.  However,
*               since these handler functions are required as callback functions for network security
*               manager port files; these handler functions MUST be defined as global functions.
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

                                                                                        /* ------ SOCK API FNCTS ------ */
NET_SOCK_ID         NetSock_Open                         (       NET_SOCK_PROTOCOL_FAMILY       protocol_family,
                                                                 NET_SOCK_TYPE                  sock_type,
                                                                 NET_SOCK_PROTOCOL              protocol,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_Close                        (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_Bind                         (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_ADDR                 *paddr_local,
                                                                 NET_SOCK_ADDR_LEN              addr_len,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_Conn                         (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_ADDR                 *paddr_remote,
                                                                 NET_SOCK_ADDR_LEN              addr_len,
                                                                 NET_ERR                       *p_err);

#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
NET_SOCK_RTN_CODE   NetSock_Listen                       (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_Q_SIZE                sock_q_size,
                                                                 NET_ERR                       *p_err);


NET_SOCK_ID         NetSock_Accept                       (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_ADDR                 *paddr_remote,
                                                                 NET_SOCK_ADDR_LEN             *paddr_len,
                                                                 NET_ERR                       *p_err);
#endif  /* NET_SOCK_TYPE_STREAM_MODULE_EN */

NET_SOCK_RTN_CODE   NetSock_RxDataFrom                   (       NET_SOCK_ID                    sock_id,
                                                                 void                          *pdata_buf,
                                                                 CPU_INT16U                     data_buf_len,
                                                                 NET_SOCK_API_FLAGS             flags,
                                                                 NET_SOCK_ADDR                 *paddr_remote,
                                                                 NET_SOCK_ADDR_LEN             *paddr_len,
                                                                 void                          *pip_opts_buf,
                                                                 CPU_INT08U                     ip_opts_buf_len,
                                                                 CPU_INT08U                    *pip_opts_len,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_RxData                       (       NET_SOCK_ID                    sock_id,
                                                                 void                          *pdata_buf,
                                                                 CPU_INT16U                     data_buf_len,
                                                                 NET_SOCK_API_FLAGS             flags,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_TxDataTo                     (       NET_SOCK_ID                    sock_id,
                                                                 void                          *p_data,
                                                                 CPU_INT16U                     data_len,
                                                                 NET_SOCK_API_FLAGS             flags,
                                                                 NET_SOCK_ADDR                 *paddr_remote,
                                                                 NET_SOCK_ADDR_LEN              addr_len,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_TxData                       (       NET_SOCK_ID                    sock_id,
                                                                 void                          *p_data,
                                                                 CPU_INT16U                     data_len,
                                                                 NET_SOCK_API_FLAGS             flags,
                                                                 NET_ERR                       *p_err);

#if (NET_SOCK_CFG_SEL_EN == DEF_ENABLED)
NET_SOCK_RTN_CODE   NetSock_Sel                          (       NET_SOCK_QTY                   sock_nbr_max,
                                                                 NET_SOCK_DESC                 *psock_desc_rd,
                                                                 NET_SOCK_DESC                 *psock_desc_wr,
                                                                 NET_SOCK_DESC                 *psock_desc_err,
                                                                 NET_SOCK_TIMEOUT              *ptimeout,
                                                                 NET_ERR                       *p_err);


void                NetSock_SelAbort                     (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);
#endif /* NET_SOCK_CFG_SEL_EN */

CPU_BOOLEAN         NetSock_IsConn                       (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);


                                                                                        /* ------ SOCK CFG FNCTS ------ */
                                                                                        /* Cfg sock block  mode.        */
CPU_BOOLEAN         NetSock_CfgBlock                     (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT08U                     block,
                                                                 NET_ERR                       *p_err);


CPU_INT08U          NetSock_BlockGet                     (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg sock secure mode.        */
CPU_BOOLEAN         NetSock_CfgSecure                    (       NET_SOCK_ID                    sock_id,
                                                                 CPU_BOOLEAN                    secure,
                                                                 NET_ERR                       *p_err);


CPU_BOOLEAN         NetSock_CfgSecureServerCertKeyInstall(       NET_SOCK_ID                    sock_id,
                                                          const  void                          *p_cert,
                                                                 CPU_INT32U                     cert_len,
                                                          const  void                          *p_key,
                                                                 CPU_INT32U                     key_len,
                                                                 NET_SOCK_SECURE_CERT_KEY_FMT   fmt,
                                                                 CPU_BOOLEAN                    cert_chain,
                                                                 NET_ERR                       *p_err);


CPU_BOOLEAN         NetSock_CfgSecureClientCertKey       (       NET_SOCK_ID                    sock_id,
                                                          const  void                          *p_cert,
                                                                 CPU_INT32U                     cert_size,
                                                          const  void                          *p_key,
                                                                 CPU_INT32U                     key_size,
                                                                 NET_SOCK_SECURE_CERT_KEY_FMT   fmt,
                                                                 CPU_BOOLEAN                    cert_chain,
                                                                 NET_ERR                       *p_err);


CPU_BOOLEAN         NetSock_CfgSecureClientCommonName    (       NET_SOCK_ID                    sock_id,
                                                                 CPU_CHAR                      *pcommon_name,
                                                                 NET_ERR                       *p_err);


CPU_BOOLEAN         NetSock_CfgSecureClientTrustCallBack (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_SECURE_TRUST_FNCT     call_back_fnct,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg interface socket.        */
CPU_BOOLEAN         NetSock_CfgIF                        (       NET_SOCK_ID                    sock_id,
                                                                 NET_IF_NBR                     if_nbr,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg sock rx Q size.          */
CPU_BOOLEAN         NetSock_CfgRxQ_Size                  (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_DATA_SIZE             size,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg sock tx Q size.          */
CPU_BOOLEAN         NetSock_CfgTxQ_Size                  (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_DATA_SIZE             size,
                                                                 NET_ERR                       *p_err);

                                                                                    /* Cfg/set sock conn child Q size.  */
CPU_BOOLEAN         NetSock_CfgConnChildQ_SizeSet        (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_Q_SIZE                queue_size,
                                                                 NET_ERR                       *p_err);

                                                                                    /* Get     sock conn child Q size.  */
NET_SOCK_Q_SIZE     NetSock_CfgConnChildQ_SizeGet        (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);


CPU_BOOLEAN         NetSock_CfgTxNagle                   (       NET_SOCK_ID                    sock_id,
                                                                 CPU_BOOLEAN                    nagle_en,
                                                                 NET_ERR                       *p_err);
#ifdef  NET_IPv4_MODULE_EN
                                                                                        /* Cfg sock tx IP TOS.          */
CPU_BOOLEAN         NetSock_CfgTxIP_TOS                  (       NET_SOCK_ID                    sock_id,
                                                                 NET_IPv4_TOS                   ip_tos,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg sock tx IP TTL.          */
CPU_BOOLEAN         NetSock_CfgTxIP_TTL                  (       NET_SOCK_ID                    sock_id,
                                                                 NET_IPv4_TTL                   ip_ttl,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg sock tx IP TTL multicast.*/
CPU_BOOLEAN         NetSock_CfgTxIP_TTL_Multicast        (       NET_SOCK_ID                    sock_id,
                                                                 NET_IPv4_TTL                   ip_ttl,
                                                                 NET_ERR                       *p_err);
#endif  /* NET_IPv4_MODULE_EN */
                                                                                        /* Cfg dflt sock rx Q   timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutRxQ_Dflt           (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg/set  sock rx Q   timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutRxQ_Set            (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT32U                     timeout_ms,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Get      sock rx Q   timeout.*/
CPU_INT32U          NetSock_CfgTimeoutRxQ_Get_ms         (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg dflt sock tx Q   timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutTxQ_Dflt           (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg/set  sock tx Q   timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutTxQ_Set            (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT32U                     timeout_ms,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Get      sock tx Q   timeout.*/
CPU_INT32U          NetSock_CfgTimeoutTxQ_Get_ms         (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg dflt sock conn   timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutConnReqDflt        (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg/set  sock conn   timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutConnReqSet         (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT32U                     timeout_ms,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Get      sock conn   timeout.*/
CPU_INT32U          NetSock_CfgTimeoutConnReqGet_ms      (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);
#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
                                                                                        /* Cfg dflt sock accept timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutConnAcceptDflt     (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg/set  sock accept timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutConnAcceptSet      (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT32U                     timeout_ms,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Get      sock accept timeout.*/
CPU_INT32U          NetSock_CfgTimeoutConnAcceptGet_ms   (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);
#endif  /* NET_SOCK_TYPE_STREAM_MODULE_EN */
                                                                                        /* Cfg dflt sock close  timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutConnCloseDflt      (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Cfg/set  sock close  timeout.*/
CPU_BOOLEAN         NetSock_CfgTimeoutConnCloseSet       (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT32U                     timeout_ms,
                                                                 NET_ERR                       *p_err);

                                                                                        /* Get      sock close  timeout.*/
CPU_INT32U          NetSock_CfgTimeoutConnCloseGet_ms    (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_OptGet                       (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_PROTOCOL              level,
                                                                 NET_SOCK_OPT_NAME              opt_name,
                                                                 void                          *popt_val,
                                                                 NET_SOCK_OPT_LEN              *popt_len,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_OptSet                       (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK_PROTOCOL              level,
                                                                 NET_SOCK_OPT_NAME              opt_name,
                                                          const  void                          *popt_val,
                                                                 NET_SOCK_OPT_LEN               opt_len,
                                                                 NET_ERR                       *p_err);


NET_STAT_POOL       NetSock_PoolStatGet                  (void);


void                NetSock_PoolStatResetMaxUsed         (void);


void                NetSock_GetLocalIPAddr               (       NET_SOCK_ID                    sock_id,
                                                                 CPU_INT08U                    *p_buf_addr,
                                                                 NET_SOCK_FAMILY               *p_family,
                                                                 NET_ERR                       *p_err);


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void                NetSock_Init                         (       NET_ERR                       *p_err);


                                                                                        /* --------- RX FNCTS --------- */
void                NetSock_Rx                           (       NET_BUF                       *pbuf,
                                                                 NET_ERR                       *p_err);


void                NetSock_CloseFromConn                (       NET_SOCK_ID                    sock_id);


void                NetSock_FreeConnFromSock             (       NET_SOCK_ID                    sock_id,
                                                                 NET_CONN_ID                    conn_id);

#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
void                NetSock_ConnChildAdd                 (       NET_SOCK_ID                    sock_id,
                                                                 NET_CONN_ID                    conn_id,
                                                                 NET_ERR                       *p_err);


void                NetSock_ConnSignalReq                (       NET_SOCK_ID                    sock_id,
                                                                 NET_ERR                       *p_err);


void                NetSock_ConnSignalAccept             (       NET_SOCK_ID                    sock_id,
                                                                 NET_CONN_ID                    conn_id,
                                                                 NET_ERR                       *p_err);


void                NetSock_ConnSignalClose              (       NET_SOCK_ID                    sock_id,
                                                                 CPU_BOOLEAN                    data_avail,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_RxDataHandlerStream          (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK                      *psock,
                                                                 void                          *pdata_buf,
                                                                 CPU_INT16U                     data_buf_len,
                                                                 NET_SOCK_API_FLAGS             flags,
                                                                 NET_SOCK_ADDR                 *paddr_remote,
                                                                 NET_SOCK_ADDR_LEN             *paddr_len,
                                                                 NET_ERR                       *p_err);


NET_SOCK_RTN_CODE   NetSock_TxDataHandlerStream          (       NET_SOCK_ID                    sock_id,
                                                                 NET_SOCK                      *psock,
                                                                 void                          *p_data,
                                                                 CPU_INT16U                     data_len,
                                                                 NET_SOCK_API_FLAGS             flags,
                                                                 NET_ERR                       *p_err);
#endif /* NET_SOCK_TYPE_STREAM_MODULE_EN */



                                                                                        /* ---- SOCK STATUS FNCTS ----- */
CPU_BOOLEAN         NetSock_IsUsed                   (NET_SOCK_ID   sock_id,
                                                      NET_ERR      *p_err);

NET_SOCK           *NetSock_GetObj                   (NET_SOCK_ID   sock_id);

NET_CONN_ID         NetSock_GetConnTransportID       (NET_SOCK_ID   sock_id,
                                                      NET_ERR      *p_err);

                                                                                    /* Clr      sock rx Q signal.       */
void                NetSock_RxQ_Clr                  (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Wait for sock rx Q signal.       */
void                NetSock_RxQ_Wait                 (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Signal   sock rx Q.              */
void                NetSock_RxQ_Signal               (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Abort    sock rx Q.              */
void                NetSock_RxQ_Abort                (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);

                                                                                    /* Set dflt sock rx Q timeout.      */
void                NetSock_RxQ_TimeoutDflt          (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Set      sock rx Q timeout.      */
void                NetSock_RxQ_TimeoutSet           (NET_SOCK     *p_sock,
                                                      CPU_INT32U    timeout_ms,
                                                      NET_ERR      *p_err);
                                                                                    /* Get      sock rx Q timeout.      */
CPU_INT32U          NetSock_RxQ_TimeoutGet_ms        (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);

#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
                                                                                    /* Clr      sock conn signal.       */
void                NetSock_ConnReqClr               (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Wait for sock conn signal.       */
void                NetSock_ConnReqWait              (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Signal   sock conn.              */
void                NetSock_ConnReqSignal            (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Abort    sock conn.              */
void                NetSock_ConnReqAbort             (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);

                                                                                    /* Set dflt sock conn timeout.      */
void                NetSock_ConnReqTimeoutDflt       (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Set      sock conn timeout.      */
void                NetSock_ConnReqTimeoutSet        (NET_SOCK     *p_sock,
                                                      CPU_INT32U    timeout_ms,
                                                      NET_ERR      *p_err);
                                                                                    /* Get      sock conn timeout.      */
CPU_INT32U          NetSock_ConnReqTimeoutGet_ms     (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);


                                                                                    /* Clr      sock accept Q signal.   */
void                NetSock_ConnAcceptQ_SemClr        (NET_SOCK     *p_sock,
                                                       NET_ERR      *p_err);
                                                                                    /* Wait for sock accept Q signal.   */
void                NetSock_ConnAcceptQ_Wait         (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Signal   sock accept Q.          */
void                NetSock_ConnAcceptQ_Signal       (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Abort    sock accept Q wait.     */
void                NetSock_ConnAcceptQ_Abort        (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);

                                                                                    /* Set dflt sock accept Q timeout.  */
void                NetSock_ConnAcceptQ_TimeoutDflt  (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Set      sock accept Q timeout.  */
void                NetSock_ConnAcceptQ_TimeoutSet   (NET_SOCK     *p_sock,
                                                      CPU_INT32U    timeout_ms,
                                                      NET_ERR      *p_err);
                                                                                    /* Get      sock accept Q timeout.  */
CPU_INT32U          NetSock_ConnAcceptQ_TimeoutGet_ms(NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);

                                                                                    /* Clr      sock close signal.      */
void                NetSock_ConnCloseClr             (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Wait for sock close signal.      */
void                NetSock_ConnCloseWait            (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Signal   sock close.             */
void                NetSock_ConnCloseSignal          (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Abort    sock close.             */
void                NetSock_ConnCloseAbort           (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);

                                                                                    /* Set dflt sock close timeout.     */
void                NetSock_ConnCloseTimeoutDflt     (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
                                                                                    /* Set      sock close timeout.     */
void                NetSock_ConnCloseTimeoutSet      (NET_SOCK     *p_sock,
                                                      CPU_INT32U    timeout_ms,
                                                      NET_ERR      *p_err);
                                                                                    /* Get      sock close timeout.     */
CPU_INT32U          NetSock_ConnCloseTimeoutGet_ms   (NET_SOCK     *p_sock,
                                                      NET_ERR      *p_err);
#endif  /* NET_SOCK_TYPE_STREAM_MODULE_EN */

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


#ifndef  NET_SOCK_NBR_SOCK
#error  "NET_SOCK_NBR_SOCK                      not #define'd in 'net_cfg.h'"
#error  "                                     [MUST be  >= NET_SOCK_NBR_MIN    ]"
#error  "                                     [     &&  <= NET_SOCK_NBR_MAX    ]"

#elif   (DEF_CHK_VAL(NET_SOCK_NBR_SOCK,     \
                     NET_SOCK_NBR_MIN,          \
                     NET_SOCK_NBR_MAX) != DEF_OK)
#error  "NET_SOCK_NBR_SOCK                illegally #define'd in 'net_cfg.h'"
#error  "                                     [MUST be  >= NET_SOCK_NBR_MIN    ]"
#error  "                                     [     &&  <= NET_SOCK_NBR_MAX    ]"
#endif




#ifndef  NET_SOCK_CFG_SEL_EN
#error  "NET_SOCK_CFG_SEL_EN                        not #define'd in 'net_cfg.h'"
#error  "                                     [MUST be  DEF_DISABLED]           "
#error  "                                     [     ||  DEF_ENABLED ]           "

#elif  ((NET_SOCK_CFG_SEL_EN != DEF_DISABLED) && \
        (NET_SOCK_CFG_SEL_EN != DEF_ENABLED ))
#error  "NET_SOCK_CFG_SEL_EN                  illegally #define'd in 'net_cfg.h'"
#error  "                                     [MUST be  DEF_DISABLED]           "
#error  "                                     [     ||  DEF_ENABLED ]           "


#endif


#ifndef  NET_SOCK_CFG_RX_Q_SIZE_OCTET
#error  "NET_SOCK_CFG_RX_Q_SIZE_OCTET               not #define'd in 'net_cfg.h'  "
#error  "                                     [MUST be  >= NET_SOCK_DATA_SIZE_MIN]"
#error  "                                     [     &&  <= NET_SOCK_DATA_SIZE_MIN]"

#elif   (DEF_CHK_VAL(NET_SOCK_CFG_RX_Q_SIZE_OCTET,    \
                     NET_SOCK_DATA_SIZE_MIN,          \
                     NET_SOCK_DATA_SIZE_MAX) != DEF_OK)
#error  "NET_SOCK_CFG_RX_Q_SIZE_OCTET         illegally #define'd in 'net_cfg.h'  "
#error  "                                     [MUST be  >= NET_SOCK_DATA_SIZE_MIN]"
#error  "                                     [     &&  <= NET_SOCK_DATA_SIZE_MIN]"
#endif



#ifndef  NET_SOCK_CFG_TX_Q_SIZE_OCTET
#error  "NET_SOCK_CFG_TX_Q_SIZE_OCTET               not #define'd in 'net_cfg.h'  "
#error  "                                     [MUST be  >= NET_SOCK_DATA_SIZE_MIN]"
#error  "                                     [     &&  <= NET_SOCK_DATA_SIZE_MIN]"

#elif   (DEF_CHK_VAL(NET_SOCK_CFG_TX_Q_SIZE_OCTET,    \
                     NET_SOCK_DATA_SIZE_MIN,          \
                     NET_SOCK_DATA_SIZE_MAX) != DEF_OK)
#error  "NET_SOCK_CFG_TX_Q_SIZE_OCTET         illegally #define'd in 'net_cfg.h'  "
#error  "                                     [MUST be  >= NET_SOCK_DATA_SIZE_MIN]"
#error  "                                     [     &&  <= NET_SOCK_DATA_SIZE_MIN]"
#endif





#ifndef  NET_SOCK_DFLT_TIMEOUT_RX_Q_MS
#error  "NET_SOCK_DFLT_TIMEOUT_RX_Q_MS               not #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"

#elif  ((DEF_CHK_VAL(NET_SOCK_DFLT_TIMEOUT_RX_Q_MS,                                   \
                     NET_TIMEOUT_MIN_mS,                                             \
                     NET_TIMEOUT_MAX_mS) != DEF_OK)                  &&              \
     (!((DEF_CHK_VAL_MIN(NET_SOCK_DFLT_TIMEOUT_RX_Q_MS, 0) == DEF_OK) &&              \
                        (NET_SOCK_DFLT_TIMEOUT_RX_Q_MS     == NET_TMR_TIME_INFINITE))))
#error  "NET_SOCK_DFLT_TIMEOUT_RX_Q_MS         illegally #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"
#endif



#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN

#ifndef  NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS
#error  "NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS           not #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"

#elif  ((DEF_CHK_VAL(NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS,                                   \
                     NET_TIMEOUT_MIN_mS,                                              \
                     NET_TIMEOUT_MAX_mS) != DEF_OK)                   &&              \
     (!((DEF_CHK_VAL_MIN(NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS, 0) == DEF_OK) &&              \
                        (NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS     == NET_TMR_TIME_INFINITE))))
#error  "NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS     illegally #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"
#endif



#ifndef  NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS
#error  "NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS        not #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"

#elif  ((DEF_CHK_VAL(NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS,                                   \
                     NET_TIMEOUT_MIN_mS,                                                    \
                     NET_TIMEOUT_MAX_mS) != DEF_OK)                         &&              \
     (!((DEF_CHK_VAL_MIN(NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS, 0) == DEF_OK) &&              \
                        (NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS     == NET_TMR_TIME_INFINITE))))
#error  "NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS  illegally #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"
#endif


#if 0
#ifndef  NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS
#error  "NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS         not #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]   "
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]   "
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"

#elif  ((DEF_CHK_VAL(NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS,                                   \
                    NET_TIMEOUT_MIN_mS,                                                    \
                    NET_TIMEOUT_MAX_mS) != DEF_OK)                         &&              \
     (!((DEF_CHK_VAL_MIN(NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS, 0) == DEF_OK) &&              \
                        (NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS     == NET_TMR_TIME_INFINITE))))
#error  "NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS   illegally #define'd in 'net_cfg.h' "
#error  "                                     [MUST be  >= NET_TIMEOUT_MIN_mS]"
#error  "                                     [     &&  <= NET_TIMEOUT_MAX_mS]"
#error  "                                     [     ||  == NET_TMR_TIME_INFINITE]"
#endif
#endif
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_SOCK_MODULE_PRESENT */
