/*
*********************************************************************************************************
*                                              uC/TCP-IP
*                                      The Embedded TCP/IP Suite
*
*                    Copyright 2004-2020 Silicon Laboratories Inc. www.silabs.com
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
*                                     NETWORK CONFIGURATION FILE
*
*                                              TEMPLATE
*
* Filename : net_cfg.h
* Version  : V3.06.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_CFG_MODULE_PRESENT
#define  NET_CFG_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <Source/net_def.h>
#include  <Source/net_type.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                             NETWORK EXTERNAL APPLICATION CONFIGURATION
*
* Note(s) : (1) When uC/DNS-Client is present in the project some high level functions can resolve hostname.
*               So uC/TCPIP should know that uC/DNS-Client is present to call the proper API.
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Configure DNS Client feature  (see Note #1) :        */
#define  NET_EXT_MODULE_CFG_DNS_EN                                 DEF_DISABLED
                                                                /*   DEF_DISABLED       DNS Client is DISABLED          */
                                                                /*   DEF_ENABLED        DNS Client is  ENABLED          */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        TASKS CONFIGURATION
*
* Note(s) : (1) (a) Each network task maps to a unique, developer-configured task configuration that
*                   MUST be defined in application files, typically 'net_cfg.c', & SHOULD be forward-
*                   declared with the exact same name & type in order to be used by the application during
*                   calls to Net_Init().
*
*               (b) Since these task configuration structures are referenced ONLY by application files,
*                   there is NO required naming convention for these configuration structures.
*********************************************************************************************************
*********************************************************************************************************
*/

extern  const  NET_TASK_CFG  NetRxTaskCfg;
extern  const  NET_TASK_CFG  NetTxDeallocTaskCfg;
extern  const  NET_TASK_CFG  NetTmrTaskCfg;



/*
*********************************************************************************************************
*********************************************************************************************************
*                                        TASKS Q CONFIGURATION
*
* Note(s) : (1) Rx queue size should be configured such that it reflects the total number of DMA receive descriptors on all
*               devices. If DMA is not available, or a combination of DMA and I/O based interfaces are configured then this
*               number reflects the maximum number of packets that can be acknowledged and signaled during a single receive
*               interrupt event for all interfaces.
*
*           (2) Tx queue size should be defined to be the total number of small and large transmit buffers declared for
*               all interfaces.
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_CFG_IF_RX_Q_SIZE                   50u             /*Configure RX queue size (See Note #1).                */
#define  NET_CFG_IF_TX_DEALLOC_Q_SIZE           50u             /*Configure TX queue size (See Note #2).                */



/*
*********************************************************************************************************
*********************************************************************************************************
*                                        NETWORK CONFIGURATION
*
* Note(s) : (1) uC/TCP-IP code may call optimized assembly functions. Optimized assembly files/functions must be included
*               in the project to be enabled. Optimized functions are located in files under folders:
*
*                   $uC-TCPIP/Ports/<processor>/<compiler>
*********************************************************************************************************
*********************************************************************************************************
*/
                                                                /* Configure network protocol suite's assembly ...      */
                                                                /* ... optimization (see Note #1) :                     */
#define  NET_CFG_OPTIMIZE_ASM_EN                DEF_DISABLED
                                                                /*   DEF_DISABLED       Assembly optimization DISABLED  */
                                                                /*   DEF_ENABLED        Assembly optimization ENABLED   */



/*
*********************************************************************************************************
*********************************************************************************************************
*                                     NETWORK DEBUG CONFIGURATION
*
* Note(s) : (1) Configure NET_DBG_CFG_MEM_CLR_EN to enable/disable the network protocol suite from clearing
*               internal data structure memory buffers; a convenient feature while debugging.
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Configure memory clear feature  (see Note #1) :      */
#define  NET_DBG_CFG_MEM_CLR_EN                 DEF_DISABLED
                                                                /*   DEF_DISABLED  Data structure clears DISABLED       */
                                                                /*   DEF_ENABLED   Data structure clears ENABLED        */



/*
*********************************************************************************************************
*********************************************************************************************************
*                                NETWORK ARGUMENT CHECK CONFIGURATION
*
* Note(s) : (1) Configure NET_ERR_CFG_ARG_CHK_EXT_EN to enable/disable the network protocol suite external
*               argument check feature :
*
*               (a) When ENABLED,  ALL arguments received from any port interface provided by the developer
*                   or application are checked/validated.
*
*               (b) When DISABLED, NO  arguments received from any port interface provided by the developer
*                   or application are checked/validated.
*
*           (2) Configure NET_ERR_CFG_ARG_CHK_DBG_EN to enable/disable the network protocol suite internal,
*               debug argument check feature :
*
*               (a) When ENABLED,     internal arguments are checked/validated to debug the network protocol
*                   suite.
*
*               (b) When DISABLED, NO internal arguments are checked/validated to debug the network protocol
*                   suite.
*********************************************************************************************************
*********************************************************************************************************
*/
                                                                /* Configure external argument check feature ...        */
                                                                /* ... (see Note #1) :                                  */
#define  NET_ERR_CFG_ARG_CHK_EXT_EN             DEF_ENABLED
                                                                /*   DEF_DISABLED     Argument check DISABLED           */
                                                                /*   DEF_ENABLED      Argument check ENABLED            */

                                                                /* Configure internal argument check feature ...        */
                                                                /* ... (see Note #2) :                                  */
#define  NET_ERR_CFG_ARG_CHK_DBG_EN             DEF_DISABLED
                                                                /*   DEF_DISABLED     Argument check DISABLED           */
                                                                /*   DEF_ENABLED      Argument check ENABLED            */



/*
*********************************************************************************************************
*********************************************************************************************************
*                               NETWORK COUNTER MANAGEMENT CONFIGURATION
*
* Note(s) : (1) Configure NET_CTR_CFG_STAT_EN to enable/disable network protocol suite statistics counters.
*
*           (2) Configure NET_CTR_CFG_ERR_EN  to enable/disable network protocol suite error      counters.
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Configure statistics counter feature (see Note #1) : */
#define  NET_CTR_CFG_STAT_EN                    DEF_ENABLED
                                                                /*   DEF_DISABLED     Stat  counters DISABLED           */
                                                                /*   DEF_ENABLED      Stat  counters ENABLED            */

                                                                /* Configure error      counter feature (see Note #2) : */
#define  NET_CTR_CFG_ERR_EN                     DEF_ENABLED
                                                                /*   DEF_DISABLED     Error counters DISABLED           */
                                                                /*   DEF_ENABLED      Error counters ENABLED            */



/*
*********************************************************************************************************
*********************************************************************************************************
*                               NETWORK TIMER MANAGEMENT CONFIGURATION
*
* Note(s) : (1) Configure NET_TMR_CFG_NBR_TMR with the desired number of network TIMER objects.
*
*               Timers are required for :
*
*               (a) ARP & NDP cache entries
*               (b) IP        fragment reassembly
*               (c) TCP       state machine connections
*               (d) IF        Link status check-up
*
*           (2) Configure NET_TMR_CFG_TASK_FREQ to schedule the execution frequency of the network timer
*               task -- how often NetTmr_TaskHandler() is scheduled to run per second as implemented in
*               NetTmr_Task().
*
*               (a) NET_TMR_CFG_TASK_FREQ  MUST NOT be configured as a floating-point frequency.
*
*               See also 'net_tmr.h  NETWORK TIMER TASK TIME DEFINES  Notes #1 & #2'
*                      & 'net_tmr.c  NetTmr_Task()  Notes #1 & #2'.
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_TMR_CFG_NBR_TMR                    100u            /* Configure total number of TIMERs (see Note #1).      */
#define  NET_TMR_CFG_TASK_FREQ                  10u             /* Configure Timer Task frequency   (see Note #2).      */




/*
*********************************************************************************************************
*********************************************************************************************************
*                                NETWORK INTERFACE LAYER CONFIGURATION
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_IF_CFG_MAX_NBR_IF                  1u              /* Configure maximum number of network interfaces.      */

                                                                /* Configure specific interface(s) :                    */
#define  NET_IF_CFG_LOOPBACK_EN                 DEF_DISABLED

#define  NET_IF_CFG_ETHER_EN                    DEF_ENABLED

#define  NET_IF_CFG_WIFI_EN                     DEF_DISABLED
                                                                /*   DEF_DISABLED      Interface type DISABLED          */
                                                                /*   DEF_ENABLED       interface type ENABLED           */

#define  NET_IF_CFG_TX_SUSPEND_TIMEOUT_MS       1u              /* Configure interface transmit suspend timeout in ms.  */



/*
*********************************************************************************************************
*********************************************************************************************************
*                           ADDRESS RESOLUTION PROTOCOL LAYER CONFIGURATION
*
* Note(s) : (1) Address resolution protocol ONLY required for IPv4.
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_ARP_CFG_CACHE_NBR                  3u              /* Configure ARP cache size.                            */


/*
*********************************************************************************************************
*********************************************************************************************************
*                           NEIGHBOR DISCOVERY PROTOCOL LAYER CONFIGURATION
*
* Note(s) : (1) Neighbor Discovery Protocol ONLY required for IPv6.
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_NDP_CFG_CACHE_NBR                  5u              /* Configures number of NDP Neighbor cache entries.     */
#define  NET_NDP_CFG_DEST_NBR                   5u              /* Configures number of NDP Destination cache entries.  */
#define  NET_NDP_CFG_PREFIX_NBR                 5u              /* Configures number of NDP Prefix entries.             */
#define  NET_NDP_CFG_ROUTER_NBR                 1u              /* Configures number of NDP Router entries.             */



/*
*********************************************************************************************************
*********************************************************************************************************
*                            INTERNET PROTOCOL LAYER VERSION CONFIGURATION
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                                IPv4
*********************************************************************************************************
*/
                                                                /* Configure IPv4.                                      */
#define  NET_IPv4_CFG_EN                        DEF_ENABLED
                                                                /*   DEF_DISABLED    IPv4 disabled.                     */
                                                                /*   DEF_ENABLED     IPv4 enabled.                      */


#define  NET_IPv4_CFG_IF_MAX_NBR_ADDR           1u              /* Configure maximum number of addresses per interface. */

/*
*********************************************************************************************************
*                                                IPv6
*********************************************************************************************************
*/

                                                                /* Configure IPv6.                                      */
#define  NET_IPv6_CFG_EN                        DEF_DISABLED
                                                                /*   DEF_DISABLED    IPv6 disabled.                     */
                                                                /*   DEF_ENABLED     IPv6 enabled.                      */

                                                                /* Configure IPv6 Stateless Address Auto-Configuration. */
#define  NET_IPv6_CFG_ADDR_AUTO_CFG_EN          DEF_ENABLED
                                                                /*   DEF_DISABLED    IPv6 Auto-Cfg disabled.            */
                                                                /*   DEF_ENABLED     IPv6 Auto-Cfg enabled.             */

                                                                /* Configure IPv6 Duplication Address Detection (DAD).  */
#define  NET_IPv6_CFG_DAD_EN                    DEF_ENABLED
                                                                /*   DEF_DISABLED    IPv6 DAD disabled.                 */
                                                                /*   DEF_ENABLED     IPv6 DAD enabled.                  */

#define  NET_IPv6_CFG_IF_MAX_NBR_ADDR           2u              /* Configure maximum number of addresses per interface. */



/*
*********************************************************************************************************
*********************************************************************************************************
*                  INTERNET GROUP MANAGEMENT PROTOCOL(MULTICAST) LAYER CONFIGURATION
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Configure IPv4 multicast support :                   */
#define  NET_MCAST_CFG_IPv4_RX_EN               DEF_ENABLED
#define  NET_MCAST_CFG_IPv4_TX_EN               DEF_ENABLED
                                                                /*   DEF_DISABLED    Multicast rx or tx disabled.       */
                                                                /*   DEF_ENABLED     Multicast rx or tx enabled.        */

#define  NET_MCAST_CFG_HOST_GRP_NBR_MAX         2u              /* Configure maximum number of Multicast groups.        */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                 NETWORK SOCKET LAYER CONFIGURATION
*
* Note(s) : (1) The maximum accept queue size represents the number of connection that can be queued by
*               the stack before being accepted. For a TCP server when a connection is queued, it means
*               that the SYN, ACK packet has been sent back, so the remote host can start transmitting
*               data once the connection is queued and the stack will queue up all data received until
*               the connection is accepted and the data is read.
*
*           (2) Receive and transmit queue size MUST be properly configured to optimize performance.
*
*               (a) It represents the number of bytes that can be queued by one socket. It's important
*                   that all socket are not able to queue more data than what the device can hold in its
*                   buffers.
*
*               (b) The size should be also a multiple of the maximum segment size (MSS) to optimize
*                   performance. UDP MSS is 1470 and TCP MSS is 1460.
*
*               (c) RX and TX queue size can be reduce at runtime using socket option API.
*
*               (d) Window calculation example:
*
*                       Number of TCP connection  : 2
*                       Number of UDP connection  : 0
*                       Number of RX large buffer : 10
*                       Number of TX Large buffer : 6
*                       Number of TX small buffer : 2
*                       Size of RX large buffer   : 1518
*                       Size of TX large buffer   : 1518
*                       Size of TX small buffer   : 60
*
*                       TCP MSS RX                = 1460
*                       TCP MSS TX large buffer   = 1460
*                       TCP MSS TX small buffer   = 0
*
*                       Maximum receive  window   = (10 * 1460)           = 14600 bytes
*                       Maximum transmit window   = (6  * 1460) + (2 * 0) = 8760  bytes
*
*                       RX window size per socket = (14600 / 2)           =  7300 bytes
*                       TX window size per socket = (8760  / 2)           =  4380 bytes
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_SOCK_CFG_SOCK_NBR_TCP              5u              /* Configure number of TCP connections.                 */
#define  NET_SOCK_CFG_SOCK_NBR_UDP              2u              /* Configure number of UDP connections.                 */

                                                                /* Configure socket select functionality :              */
#define  NET_SOCK_CFG_SEL_EN                    DEF_ENABLED
                                                                /*   DEF_DISABLED  Socket select  DISABLED              */
                                                                /*   DEF_ENABLED   Socket select  ENABLED               */

                                                                /* Configure stream-type sockets' accept queue          */
#define  NET_SOCK_CFG_CONN_ACCEPT_Q_SIZE_MAX    2u              /* maximum size. (See Note # 1)                         */


                                                                /* Configure sockets' buffer sizes in number of octets  */
                                                                /* (see Note #2):                                       */
#define  NET_SOCK_CFG_RX_Q_SIZE_OCTET           4096u           /* Configure socket receive  queue buffer size.         */
#define  NET_SOCK_CFG_TX_Q_SIZE_OCTET           4096u           /* Configure socket transmit queue buffer size.         */


/* ==================================  ADVANCED SOCKET CONFIGURATION: DEFAULT VALUES ================================== */
/* By default sockets are set to block. Add the following define to set all sockets as non-blocking. Note that it's     */
/* possible to change socket's blocking mode at runtime using socket option API.                                        */
/*                                                                                                                      */
/*     #define  NET_SOCK_DFLT_NO_BLOCK_EN                     DEF_ENABLED                                               */
/*                                                                                                                      */
/* By default random port start at 65000, redefine the following define to modify where random port start:              */
/*                                                                                                                      */
/*     #define  NET_SOCK_DFLT_PORT_NBR_RANDOM_BASE            65000u                                                    */
/*                                                                                                                      */
/* When a socket is set as blocking the following default timeout values are used. Redefine the following defines to    */
/* change default timeouts. Timeout values may also be configured with network time constant, NET_TMR_TIME_INFINITE,    */
/* to never time out. Note that it's possible to change at runtime any timeout values using Socket option API.          */
/*                                                                                                                      */
/*     #define  NET_SOCK_DFLT_TIMEOUT_RX_Q_MS                 10000u                                                    */
/*     #define  NET_SOCK_DFLT_TIMEOUT_CONN_REQ_MS             10000u                                                    */
/*     #define  NET_SOCK_DFLT_TIMEOUT_CONN_ACCEPT_MS          10000u                                                    */
/*     #define  NET_SOCK_DFLT_TIMEOUT_CONN_CLOSE_MS           10000u                                                    */
/* ==================================================================================================================== */


/* ==================================  BSD SOCKET CONFIGURATION: ENABLE/DISABLE ======================================= */
/* By default BSD socket functions will be included in the stack. If the user wishes to remove our BSD functions they   */
/* should uncomment the line with the following #define. This will remove our BSD implementations and allow for the     */
/* user to use or define their own at their own discretion.                                                             */
/*                                                                                                                      */
/* Uncomment or re-define to remove uC/TCP-IP BSD Function Implementation:                                              */
/*                                                                                                                      */
/*    #define  NET_SOCK_DFLT_BSD_EN                        DEF_ENABLED                                                  */
/* ==================================================================================================================== */
                                                                /* Max nbr of addrinfo structs for BSD. Val should be...*/
                                                                /* ...<= (AddrIPv4MaxPerHost + AddrIPv6MaxPerHost) in...*/
#define  NET_BSD_CFG_ADDRINFO_HOST_NBR_MAX      5u              /* ... dflt uC/DNSc cfg. (See DNSc_Cfg in dns-c_cfg.c). */


/*
*********************************************************************************************************
*********************************************************************************************************
*                          TRANSMISSION CONTROL PROTOCOL LAYER CONFIGURATION
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Configure TCP support :                              */
#define  NET_TCP_CFG_EN                         DEF_ENABLED
                                                                /*   DEF_DISABLED  TCP layer  DISABLED                  */
                                                                /*   DEF_ENABLED   TCP layer  ENABLED                   */

/* ========================================= ADVANCED TCP LAYER CONFIGURATION ========================================= */
/* By default TCP RX and TX windows are set to equal the socket RX and TX queue sizes. Default values can be changed by */
/* redefining the following defines. TCP windows must be properly configured to optimize performance (see note about    */
/* Socket TX and RX windows). Note that it's possible to decrease window size at run time using Socket option API.      */
/*                                                                                                                      */
/*     #define  NET_TCP_DFLT_RX_WIN_SIZE_OCTET      NET_SOCK_CFG_RX_Q_SIZE_OCTET                                        */
/*     #define  NET_TCP_DFLT_TX_WIN_SIZE_OCTET      NET_SOCK_CFG_TX_Q_SIZE_OCTET                                        */
/*                                                                                                                      */
/* As shown in the TCP state diagram (see RFC #793), before moving from 'TIME-WAIT' state to 'CLOSED' state a timeout   */
/* (2MSL) must expire. This means that the TCP connection cannot be made available for subsequent TCP connections until */
/* this timeout. It can be a problem for embedded systems with low resources especially when many TCP connections are   */
/* made in a small period of time since it is possible to run out of free TCP connections quickly. Therefore this       */
/* timeout is set to 0 by default to avoid this kind of problem and the connection is made available as soon as the     */
/* 'TIME-WAIT' state is reached. However, it's possible to set the default MSL timeout to something else by redefining  */
/* the following define. Note that it is possible to change the MSL timeout for a specific TCP connection using Socket  */
/* option API.                                                                                                          */
/*                                                                                                                      */
/*     #define  NET_TCP_DFLT_TIMEOUT_CONN_MAX_SEG_SEC       0u                                                          */
/*                                                                                                                      */
/* To avoid leaving a connection in the FIN_WAIT_2 state forever when a connection moves from the 'FIN_WAIT_1' state to */
/* the FIN_WAIT_2, the TCP connection's timer is set to 15 second, and when it expires the connection is dropped. Thus, */
/* if the other host doesn't response to the close request, the connection will still be closed after the timeout.      */
/* This default timeout can be change by redefining the following define.                                               */
/*                                                                                                                      */
/*     #define  NET_TCP_DFLT_TIMEOUT_CONN_FIN_WAIT_2_SEC    15u                                                         */
/*                                                                                                                      */
/* The number of TCP connections is configured following the number of TCP sockets and the accept queue size when the   */
/* MSL is set to 0 ms. However, since the default MSL can be modified, it might be needed to increase the number of TCP */
/* connections to establish more connections when waiting for the MSL expiration. It is possible to add more TCP        */
/* connections by defining the following define.                                                                        */
/*                                                                                                                      */
/*     #define  NET_TCP_CFG_NBR_CONN                        0u                                                          */
/*                                                                                                                      */
/* By default an 'ACK' is generated within 500 ms of the arrival of the first unacknowledged packet, as specified in    */
/* RFC #2581, Section 4.2. However it's possible to modify this value by defining the following define.                 */
/*                                                                                                                      */
/*     #define  NET_TCP_DFLT_TIMEOUT_CONN_ACK_DLY_MS        500u                                                        */
/*                                                                                                                      */
/* When a socket is set as blocking the following default timeout values are used. Redefine the following defines to    */
/* change default timeout. Timeout values may also be configured with network time constant, NET_TMR_TIME_INFINITE,     */
/* to never time out. Note that it's possible to change at runtime any timeout values using Socket option API.          */
/*     #define  NET_TCP_DFLT_TIMEOUT_CONN_RX_Q_MS           1000u                                                       */
/*     #define  NET_TCP_DFLT_TIMEOUT_CONN_TX_Q_MS           1000u                                                       */
/* ==================================================================================================================== */




/*
*********************************************************************************************************
*********************************************************************************************************
*                             USER DATAGRAM PROTOCOL LAYER CONFIGURATION
*
* Note(s) : (1) Configure NET_UDP_CFG_APP_API_SEL with the desired configuration for demultiplexing
*               UDP datagrams to application connections :
*
*                   NET_UDP_APP_API_SEL_SOCK        Demultiplex UDP datagrams to BSD sockets ONLY.
*                   NET_UDP_APP_API_SEL_APP         Demultiplex UDP datagrams to application-specific
*                                                       connections ONLY.
*                   NET_UDP_APP_API_SEL_SOCK_APP    Demultiplex UDP datagrams to BSD sockets first;
*                                                       if NO socket connection found to demultiplex
*                                                       a UDP datagram, demultiplex to application-
*                                                       specific connection.
*
*               See also 'net_udp.c  NetUDP_RxPktDemuxDatagram()  Note #1'
*                      & 'net_udp.c  NetUDP_RxPktDemuxAppData()   Note #1'.
*
*           (2) (a) RFC #1122, Section 4.1.3.4 states that "an application MAY optionally ... discard
*                   ... [or allow] ... received ... UDP datagrams without checksums".
*
*               (b) Configure NET_UDP_CFG_RX_CHK_SUM_DISCARD_EN to enable/disable discarding of UDP
*                   datagrams received with NO computed check-sum :
*
*                   (1) When ENABLED,  ALL UDP datagrams received without a check-sum are discarded.
*
*                   (2) When DISABLED, ALL UDP datagrams received without a check-sum are flagged so
*                       that application(s) may handle &/or discard.
*
*               See also 'net_udp.c  NetUDP_RxPktValidate()  Note #4d3A'.
*
*           (3) (a) RFC #1122, Section 4.1.3.4 states that "an application MAY optionally be able to
*                   control whether a UDP checksum will be generated".
*
*               (b) Configure NET_UDP_CFG_TX_CHK_SUM_EN to enable/disable transmitting UDP datagrams
*                   with check-sums :
*
*                   (1) When ENABLED,  ALL UDP datagrams are transmitted with    a computed check-sum.
*
*                   (2) When DISABLED, ALL UDP datagrams are transmitted without a computed check-sum.
*
*               See also 'net_udp.c  NetUDP_TxPktPrepareHdr()  Note #3b'.
*********************************************************************************************************
*********************************************************************************************************
*/
                                                                /* Configure UDP Receive Check-Sum Discard feature ...  */
                                                                /* ... (see Note #2b) :                                 */
#define  NET_UDP_CFG_RX_CHK_SUM_DISCARD_EN      DEF_DISABLED
                                                                /*   DEF_DISABLED  UDP Check-Sums  Received without ... */
                                                                /*                     Check-Sums Validated             */
                                                                /*   DEF_ENABLED   UDP Datagrams  Received without ...  */
                                                                /*                     Check-Sums Discarded             */

                                                                /* Configure UDP Transmit Check-Sum feature ...         */
                                                                /* ... (see Note #3b) :                                 */
#define  NET_UDP_CFG_TX_CHK_SUM_EN              DEF_ENABLED
                                                                /*   DEF_DISABLED  Transmit Check-Sums  DISABLED        */
                                                                /*   DEF_ENABLED   Transmit Check-Sums  ENABLED         */




/*
*********************************************************************************************************
*********************************************************************************************************
*                               NETWORK SECURITY MANAGER CONFIGURATION
*
* Note(s): (1) The network security layer can be enabled ONLY if the application project contains a secure module
*              supported by uC/TCPIP such as:
*
*              (a) emSSL provided by Segger.
*
*          (2) The network security port must be also added to the project. Security port can be found under the folder:
*
*                 $uC-TCPIP/Secure/<module>
*********************************************************************************************************
*********************************************************************************************************
*/
                                                                /* Configure network security layer (See Note #1 & #2): */
#define  NET_SECURE_CFG_EN                      DEF_DISABLED
                                                                /*   DEF_DISABLED  Security layer  DISABLED             */
                                                                /*   DEF_ENABLED   Security layer  ENABLED              */

                                                                /* Configure the network security port to use any HW ...*/
                                                                /* ... cryptoengine supported by both the MCU and the...*/
                                                                /* ... secure module (e.g. Segger emSSL).               */
#define  NET_SECURE_CFG_HW_CRYPTO_EN            DEF_DISABLED
                                                                /*  DEF_DISABLED  Security layer Cryptoengine  DISABLED */
                                                                /*  DEF_ENABLED   Security layer Cryptoengine  ENABLED  */

#define  NET_SECURE_CFG_MAX_NBR_SOCK_SERVER     2u              /* Configure total number of server secure sockets.     */
#define  NET_SECURE_CFG_MAX_NBR_SOCK_CLIENT     2u              /* Configure total number of client secure sockets.     */

#define  NET_SECURE_CFG_MAX_CERT_LEN            1500u           /* Configure servers certificate maximum length (bytes) */
#define  NET_SECURE_CFG_MAX_KEY_LEN             1500u           /* Configure servers key maximum length (bytes)         */

                                                                /* Configure maximum number of certificate authorities  */
#define  NET_SECURE_CFG_MAX_NBR_CA              1u              /* that can be installed.                               */

#define  NET_SECURE_CFG_MAX_CA_CERT_LEN         1500u           /* Configure CA certificate maximum length (bytes)      */



/*
*********************************************************************************************************
*********************************************************************************************************
*                               INTERFACE CHECKSUM OFFLOAD CONFIGURATION
*
* Note(s): (1) These configuration can be enabled only if all your interfaces support the specific
*              checksum offload option.
*
*          (2) By default a driver should enable all available checksum offload capabilities.
*********************************************************************************************************
*********************************************************************************************************
*/

/* ========================================== ADVANCED OFFLOAD CONFIGURATION ========================================== */
/* By default all checksums are validated by the stack. However, if you wish to offload checksum calculations to the    */
/* controller, you can prevent the stack from calculating checksums by enabling one or more of the following defines.   */
/* If your hardware does not support any form of checksum offloading, you should leave these options disabled.          */
/* ==================================================================================================================== */

/* -------------------------------------------------- IPv4 CHECKSUM --------------------------------------------------- */
#define  NET_IPV4_CFG_CHK_SUM_OFFLOAD_RX_EN     DEF_DISABLED
#define  NET_IPV4_CFG_CHK_SUM_OFFLOAD_TX_EN     DEF_DISABLED

/* -------------------------------------------------- ICMP CHECKSUM --------------------------------------------------- */
#define  NET_ICMP_CFG_CHK_SUM_OFFLOAD_RX_EN     DEF_DISABLED
#define  NET_ICMP_CFG_CHK_SUM_OFFLOAD_TX_EN     DEF_DISABLED

/* --------------------------------------------------- UDP CHECKSUM --------------------------------------------------- */
#define  NET_UDP_CFG_CHK_SUM_OFFLOAD_RX_EN      DEF_DISABLED
#define  NET_UDP_CFG_CHK_SUM_OFFLOAD_TX_EN      DEF_DISABLED

/* --------------------------------------------------- TCP CHECKSUM --------------------------------------------------- */
#define  NET_TCP_CFG_CHK_SUM_OFFLOAD_RX_EN      DEF_DISABLED
#define  NET_TCP_CFG_CHK_SUM_OFFLOAD_TX_EN      DEF_DISABLED

/* ======================================================= END ======================================================== */
#endif  /* NET_CFG_MODULE_PRESENT */

