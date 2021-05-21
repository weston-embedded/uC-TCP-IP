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
*                                        NETWORK CONFIGURATION
*
* Filename : net_cfg_net.h
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

#ifndef  NET_CFG_NET_MODULE_PRESENT
#define  NET_CFG_NET_MODULE_PRESENT

#include  <net_cfg.h>
#include  <lib_def.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                    NETWORK CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_ERR_CFG_ARG_CHK_EXT_EN
#error  "NET_ERR_CFG_ARG_CHK_EXT_EN        not #define'd in 'net_cfg.h'"
#error  "                            [MUST be  DEF_DISABLED]           "
#error  "                            [     ||  DEF_ENABLED ]           "

#elif  ((NET_ERR_CFG_ARG_CHK_EXT_EN != DEF_DISABLED) && \
        (NET_ERR_CFG_ARG_CHK_EXT_EN != DEF_ENABLED ))
#error  "NET_ERR_CFG_ARG_CHK_EXT_EN  illegally #define'd in 'net_cfg.h'"
#error  "                            [MUST be  DEF_DISABLED]           "
#error  "                            [     ||  DEF_ENABLED ]           "
#endif



#ifndef  NET_ERR_CFG_ARG_CHK_DBG_EN
#error  "NET_ERR_CFG_ARG_CHK_DBG_EN        not #define'd in 'net_cfg.h'"
#error  "                            [MUST be  DEF_DISABLED]           "
#error  "                            [     ||  DEF_ENABLED ]           "

#elif  ((NET_ERR_CFG_ARG_CHK_DBG_EN != DEF_DISABLED) && \
        (NET_ERR_CFG_ARG_CHK_DBG_EN != DEF_ENABLED ))
#error  "NET_ERR_CFG_ARG_CHK_DBG_EN  illegally #define'd in 'net_cfg.h'"
#error  "                            [MUST be  DEF_DISABLED]           "
#error  "                            [     ||  DEF_ENABLED ]           "
#endif



#if ((NET_IPv4_CFG_EN == DEF_DISABLED) && \
     (NET_IPv6_CFG_EN == DEF_DISABLED))
#error  "NET_IPv4_CFG_EN and NET_IPv6_CFG_EN    illegally #define'd in 'net_cfg.h'"
#error  "NET_IPv4_CFG_EN and/or NET_IPv6_CFG_EN [MUST be  DEF_ENABLED]            "
#endif


/*
*********************************************************************************************************
*                                NETWORK INTERFACE LAYER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure network interface parameters.
*               DO NOT MODIFY.
*
*           (2) (a) (1) Loopback interface  required only if internal loopback communication is enabled.
*
*                   (2) Specific interfaces required only if devices are configured for the interface(s).
*
*               (b) Some network interface share common code to receive and transmit packets, implemented in
*                   an 802x Protocol Layer (see 'net_if_802x.h' Note #1).
*
*               (c) Some network interfaces require network-address-to-hardware-address bindings, implemented
*                   in an Address Resolution Protocol Layer (see 'net_arp.h  Note #1').
*
*                   Ideally, the ARP Layer would configure the network protocol suite for the inclusion of
*                   the ARP Layer via the NET_ARP_MODULE_EN #define (see 'net_arp.h  MODULE  Note #2'
*                   &  'ARP LAYER CONFIGURATION  Note #2b').
*
*                   However, since the ARP Layer is required only for SOME network interfaces, the presence
*                   of the ARP Layer MUST be configured ...
*
*                   (a) By each network interface that requires the     ARP Layer
*                         AND
*                   (b) PRIOR to all other network modules that require ARP Layer configuration
*
*           (3) Ideally, the Network Interface layer would define ALL network interface numbers.  However,
*               certain network interface numbers MUST be defined PRIOR to all other network modules that
*               require network interface numbers.
*
*               See also 'net_if.h  NETWORK INTERFACE NUMBER DATA TYPE  Note #2b'.
*********************************************************************************************************
*/

                                                                /* ---------------- CFG NET IF PARAMS ----------------- */
#if  (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    #define  NET_IF_LOOPBACK_MODULE_EN                          /* See Note #2a1.                                       */
#endif


                                                                /* See Note #2a2.                                       */
#if  (NET_IF_CFG_ETHER_EN == DEF_ENABLED)
    #define  NET_IF_ETHER_MODULE_EN
#endif


#if  (NET_IF_CFG_WIFI_EN == DEF_ENABLED)
    #define  NET_IF_WIFI_MODULE_EN
#endif


#if  (NET_IF_CFG_ETHER_EN    == DEF_ENABLED || \
      NET_IF_CFG_WIFI_EN     == DEF_ENABLED || \
      NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)

    #define  NET_IF_802x_MODULE_EN                              /* See Note #2b.                                        */
#endif


/*
*********************************************************************************************************
*                                       IP LAYER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure IP parameters.  DO NOT MODIFY.
*********************************************************************************************************
*/



#if (NET_IPv4_CFG_EN == DEF_ENABLED)

    #define  NET_IPv4_MODULE_EN

    #if     (defined(NET_IF_ETHER_MODULE_EN)) || \
            (defined(NET_IF_WIFI_MODULE_EN))
        #define  NET_ARP_MODULE_EN
        #define  NET_ICMPv4_MODULE_EN
    #endif


    #if (NET_MCAST_CFG_IPv4_RX_EN == DEF_ENABLED)
        #define  NET_IGMP_MODULE_EN                                /* See Note #2.                                         */

    #elif (NET_MCAST_CFG_IPv4_TX_EN == DEF_ENABLED)
        #define  NET_IGMP_MCAST_TX_MODULE_EN
    #endif

#ifndef  NET_IPV4_CFG_CHK_SUM_OFFLOAD_RX_EN
    #define  NET_IPV4_CFG_CHK_SUM_OFFLOAD_RX_EN                 DEF_DISABLED
#endif

#ifndef  NET_IPV4_CFG_CHK_SUM_OFFLOAD_TX_EN
    #define  NET_IPV4_CFG_CHK_SUM_OFFLOAD_TX_EN                 DEF_DISABLED
#endif

#if (NET_IPV4_CFG_CHK_SUM_OFFLOAD_RX_EN == DEF_ENABLED)
    #define  NET_IPV4_CHK_SUM_OFFLOAD_RX
#endif

#if (NET_IPV4_CFG_CHK_SUM_OFFLOAD_TX_EN == DEF_ENABLED)
    #define  NET_IPV4_CHK_SUM_OFFLOAD_TX
#endif

#endif




#if     (NET_IPv6_CFG_EN == DEF_ENABLED)
    #define  NET_IPv6_MODULE_EN

    #if     (defined(NET_IF_ETHER_MODULE_EN)  || \
             defined(NET_IF_WIFI_MODULE_EN))
        #define  NET_ICMPv6_MODULE_EN
        #define  NET_MLDP_MODULE_EN
        #define  NET_NDP_MODULE_EN

        #if (NET_IPv6_CFG_DAD_EN == DEF_ENABLED)
            #define  NET_DAD_MODULE_EN
        #endif

        #if (NET_IPv6_CFG_ADDR_AUTO_CFG_EN == DEF_ENABLED)
            #define  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
        #endif

    #endif

#endif




#if (defined(NET_IPv4_MODULE_EN) || \
     defined(NET_IPv6_MODULE_EN))
    #define  NET_IP_MODULE_EN

#endif


#if (defined(NET_IGMP_MODULE_EN) || \
     defined(NET_MLDP_MODULE_EN))

    #define  NET_MCAST_MODULE_EN
    #define  NET_MCAST_RX_MODULE_EN
    #define  NET_MCAST_TX_MODULE_EN


#elif (defined(NET_IGMP_MCAST_TX_MODULE_EN))
    #define  NET_MCAST_MODULE_EN
    #define  NET_MCAST_TX_MODULE_EN
#endif


/*
*********************************************************************************************************
*                        NETWORK ADDRESS CACHE MANAGEMENT LAYER CONFIGURATION
*
* Note(s) : (1) Network Address Cache Management layer required by some network layers (see 'net_cache.h  Note #1').
*********************************************************************************************************
*/

#if ((defined(NET_ARP_MODULE_EN)) || \
     (defined(NET_NDP_MODULE_EN)))

    #define  NET_CACHE_MODULE_EN                               /* See Note #1.                                         */

#endif


/*
*********************************************************************************************************
*                                      IGMP LAYER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure IGMP parameters.  DO NOT MODIFY.
*
*           (2) Ideally, the IGMP Layer would configure the network protocol suite for the inclusion of
*               the IGMP Layer via the NET_IGMP_MODULE_EN #define (see 'net_igmp.h  MODULE  Note #2').
*               However, the presence of the IGMP Layer MUST be configured PRIOR to all other network
*               modules that require IGMP Layer configuration.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      MLDP LAYER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure MLDP parameters.  DO NOT MODIFY.
*
*           (2) Ideally, the MLDP Layer would configure the network protocol suite for the inclusion of
*               the MLDP Layer via the NET_MLDP_MODULE_EN #define (see 'net_mldp.h  MODULE  Note #2').
*               However, the presence of the MLDP Layer MUST be configured PRIOR to all other network
*               modules that require MLDP Layer configuration.
*********************************************************************************************************
*/




/*
*********************************************************************************************************
*                                       TCP LAYER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure TCP parameters.  DO NOT MODIFY.
*
*           (2) (a) TCP Layer required only for some application interfaces (see 'net_tcp.h  MODULE
*                   Note #1').
*
*               (b) Ideally, the TCP Layer would configure the network protocol suite for the inclusion of
*                   the TCP Layer via the NET_TCP_MODULE_EN #define (see 'net_tcp.h  MODULE  Note #2').
*                   However, the presence of the TCP Layer MUST be configured PRIOR to all other network
*                   modules that require TCP Layer configuration.
*********************************************************************************************************
*/

#if     (NET_TCP_CFG_EN == DEF_ENABLED)
    #define  NET_TCP_MODULE_EN                                 /* See Note #2.                                         */

    #ifndef  NET_TCP_CFG_CHK_SUM_OFFLOAD_RX_EN
        #define  NET_TCP_CFG_CHK_SUM_OFFLOAD_RX_EN              DEF_DISABLED
    #endif

    #ifndef  NET_TCP_CFG_CHK_SUM_OFFLOAD_TX_EN
        #define  NET_TCP_CFG_CHK_SUM_OFFLOAD_TX_EN              DEF_DISABLED
    #endif

    #if (NET_TCP_CFG_CHK_SUM_OFFLOAD_RX_EN == DEF_ENABLED)
        #define  NET_TCP_CHK_SUM_OFFLOAD_RX
    #endif

    #if (NET_TCP_CFG_CHK_SUM_OFFLOAD_TX_EN == DEF_ENABLED)
        #define  NET_TCP_CHK_SUM_OFFLOAD_TX
    #endif

#endif


/*
*********************************************************************************************************
*                                       UDP LAYER CONFIGURATION
*********************************************************************************************************
*/

#ifndef  NET_UDP_CFG_CHK_SUM_OFFLOAD_RX_EN
    #define  NET_UDP_CFG_CHK_SUM_OFFLOAD_RX_EN                 DEF_DISABLED
#endif

#ifndef  NET_UDP_CFG_CHK_SUM_OFFLOAD_TX_EN
    #define  NET_UDP_CFG_CHK_SUM_OFFLOAD_TX_EN                 DEF_DISABLED
#endif

#if (NET_UDP_CFG_CHK_SUM_OFFLOAD_RX_EN == DEF_ENABLED)
    #define  NET_UDP_CHK_SUM_OFFLOAD_RX
#endif

#if (NET_UDP_CFG_CHK_SUM_OFFLOAD_TX_EN == DEF_ENABLED)
    #define  NET_UDP_CHK_SUM_OFFLOAD_TX
#endif


/*
*********************************************************************************************************
*                                       UDP LAYER CONFIGURATION
*********************************************************************************************************
*/

#ifndef  NET_ICMP_CFG_CHK_SUM_OFFLOAD_RX_EN
    #define  NET_ICMP_CFG_CHK_SUM_OFFLOAD_RX_EN                 DEF_DISABLED
#endif

#ifndef  NET_ICMP_CFG_CHK_SUM_OFFLOAD_TX_EN
    #define  NET_ICMP_CFG_CHK_SUM_OFFLOAD_TX_EN                 DEF_DISABLED
#endif

#if (NET_ICMP_CFG_CHK_SUM_OFFLOAD_RX_EN == DEF_ENABLED)
    #define  NET_ICMP_CHK_SUM_OFFLOAD_RX
#endif

#if (NET_ICMP_CFG_CHK_SUM_OFFLOAD_TX_EN == DEF_ENABLED)
    #define  NET_ICMP_CHK_SUM_OFFLOAD_TX
#endif


/*
*********************************************************************************************************
*                                 NETWORK SOCKET LAYER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure network socket parameters.
*               DO NOT MODIFY.
*
*           (2) The pre-processor NET_SOCK_DFLT_BSD_EN check is controlled in net_cfg.h and can be
*               enabled by uncommenting it there.
*********************************************************************************************************
*/

                                                                /* ------------ CFG SOCK MODULE INCLUSION ------------- */
#if  (defined(NET_TCP_MODULE_EN))
    #define  NET_SOCK_TYPE_STREAM_MODULE_EN

                                                                /* ----------- CFG SECURE MODULE INCLUSION ------------ */
    #if (NET_SECURE_CFG_EN  == DEF_ENABLED)
        #define  NET_SECURE_MODULE_EN                           /* See Note #1.                                         */
    #endif
#endif

                                                                /* ---------- CFG SOCK BSD MODULE INCLUSION ----------- */
#ifndef  NET_SOCK_DFLT_BSD_EN
    #define NET_SOCK_BSD_EN                                     /* See Note #2.                                         */
#else
    #if (NET_SOCK_DFLT_BSD_EN == DEF_ENABLED)
        #define NET_SOCK_BSD_EN
    #endif
#endif


/*
*********************************************************************************************************
*                               NETWORK SECURITY MANAGER CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure network security manager
*               parameters.  DO NOT MODIFY.
*
*           (2) (a) Network Security Layer required only for some configurations (see 'net_secure_mgr.h
*                   Note #1').
*
*               (b) Ideally, the Network Security Layer would configure the network protocol suite for
*                   the inclusion of the Security Layer via the NET_SECURE_MODULE_EN #define (see
*                   'net_secure_mgr.h  MODULE  Note #2').  However, the presence of the Security Layer
*                   MUST be configured PRIOR to all other network modules that require  Security Layer
*                   configuration.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                               NETWORK COUNTER MANAGEMENT CONFIGURATION
*********************************************************************************************************
*/

#if    ((NET_CTR_CFG_STAT_EN == DEF_ENABLED) || \
        (NET_CTR_CFG_ERR_EN  == DEF_ENABLED))

    #define  NET_CTR_MODULE_EN
#endif


/*
*********************************************************************************************************
*                               NETWORK BUFFER MANAGEMENT CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure network buffer parameters.
*               DO NOT MODIFY.
*
*               (a) NET_BUF_DATA_PROTOCOL_HDR_SIZE_MAX's ideal #define'tion :
*
*                       (A) IF Hdr  +  max(Protocol Headers)
*
*               (b) NET_BUF_DATA_PROTOCOL_HDR_SIZE_MAX  #define'd with hard-coded knowledge that IF, IP,
*                   & TCP headers have the largest combined maximum size of all the protocol headers :
*
*                     ARP Hdr   68       IP Hdr   60      IP Hdr   60      IP Hdr   60     IP Hdr   60
*                                      ICMP Hdr    0    IGMP Hdr    0     UDP Hdr    8    TCP Hdr   60
*                     ------------     -------------    -------------     ------------    ------------
*                     Total     68     Total      60    Total      60     Total     68    Total    120
*
*               See also 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #1'.
*
*           (3) Network interface minimum/maximum header sizes MUST be #define'd based on network interface
*               type(s) configured in 'net_cfg.h'.  Assumes header sizes are fixed based on configured network
*               interface type(s) [see any 'net_if_&&&.h  NETWORK INTERFACE HEADER DEFINES  Note #1'].
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                  NETWORK CONNECTION CONFIGURATION
*
* Note(s) : (1) The following pre-processor directives correctly configure network communication
*               parameters.  DO NOT MODIFY.
*
*           (2) To balance network receive versus transmit packet loads for certain network connection
*               types (e.g. stream-type connections), network receive & transmit packets SHOULD be
*               handled in an APPROXIMATELY balanced ratio.
*
*               See also 'net_if.c  NetIF_RxPktInc()         Note #1',
*                        'net_if.c  NetIF_RxPktDec()         Note #1',
*                        'net_if.c  NetIF_TxSuspend()        Note #1',
*                      & 'net_if.c  NetIF_TxSuspendSignal()  Note #1'.
*********************************************************************************************************
*/

#if (defined(NET_TCP_MODULE_EN))
    #define  NET_LOAD_BAL_MODULE_EN
#endif


#define  NET_BUF_MODULE_EN
#define  NET_TMR_MODULE_EN

#ifndef NET_EXT_MODULE_CFG_DNS_EN
    #define  NET_EXT_MODULE_CFG_DNS_EN      DEF_DISABLED
#endif

#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
    #define  NET_EXT_MODULE_DNS_EN
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of net cfg net module include.                   */
