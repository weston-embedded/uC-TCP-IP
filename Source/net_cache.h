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
*                                  NETWORK ADDRESS CACHE MANAGEMENT
*
* Filename : net_cache.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Address cache management module ONLY required for network interfaces that require
*                network-address-to-hardware-address bindings (see RFC #826 'Abstract').
*
*            (2) Supports Address Resolution Protocol as described in RFC #826 with the following
*                restrictions/constraints :
*
*                (a) ONLY supports the following hardware types :
*                    (1) 48-bit Ethernet
*
*                (b) ONLY supports the following protocol types :
*                    (1)  32-bit IPv4
*                    (2) 128-bit IPv6
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) Address cache management module is required for some network interfaces (see 'net_cache.h  Note #1').
*
*           (2) (a) The following CACHE-module-present configuration value MUST be pre-#define'd
*                   in 'net_cfg_net.h' PRIOR to all other network modules that require cache Layer
*                   configuration (see 'net_cfg_net.h  NETWORK ADDRESS CACHE MANAGEMENT LAYER
*                   CONFIGURATION  Note #1') :
*
*                       NET_CACHE_MODULE_EN
*
*               (b) Since CACHE-module-present configuration is already pre-#define'd in 'net_cfg_net.h'
*                   (see Note #2a), the cache module is protected from multiple pre-processor inclusion
*                   through use of a module-already-available pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  NET_CACHE_MODULE_PRESENT                               /* See Note #2b.                                        */
#define  NET_CACHE_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  "net_def.h"
#include  "net_tmr.h"
#include  "net_type.h"
#include  "net_stat.h"
#include  "net_err.h"
#include  <cpu.h>
#include  <KAL/kal.h>

#ifdef   NET_CACHE_MODULE_EN                                    /* See Note #2a.                                        */

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef NET_CACHE_MODULE
#define  NET_CACHE_EXT
#else
#define  NET_CACHE_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  NETWORK CACHE LIST INDEX DEFINES
*********************************************************************************************************
*/

#define  NET_CACHE_ADDR_LIST_IX_ARP                        0u
#define  NET_CACHE_ADDR_LIST_IX_NDP                        1u

#define  NET_CACHE_ADDR_LIST_IX_MAX                        2u


/*
*********************************************************************************************************
*                                     NETWORK CACHE FLAG DEFINES
*********************************************************************************************************
*/

                                                                /* ----------------- NET CACHE FLAGS ------------------ */
#define  NET_CACHE_FLAG_NONE                      DEF_BIT_NONE
#define  NET_CACHE_FLAG_USED                      DEF_BIT_00    /* Cache cur used; i.e. NOT in free cache pool.         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         NETWORK CACHE TYPE
*********************************************************************************************************
*/

typedef  enum  net_cache_type {
    NET_CACHE_TYPE_NONE = 0,
    NET_CACHE_TYPE_ARP,
    NET_CACHE_TYPE_NDP
} NET_CACHE_TYPE;


/*
*********************************************************************************************************
*                               NETWORK CACHE ADDRESS LENGTH DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_CACHE_ADDR_LEN;


/*
*********************************************************************************************************
*                                NETWORK CACHE ADDRESS TYPE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_CACHE_ADDR_TYPE;


/*
*********************************************************************************************************
*                                  NETWORK CACHE ADDRESS DATA TYPES
*********************************************************************************************************
*/
                                                                        /* --------------- NET CACHE ADDR ------------- */
typedef  struct  net_cache_addr  NET_CACHE_ADDR;

struct  net_cache_addr {
    NET_CACHE_TYPE        Type;                                         /* Type cfg'd @ init.                           */

    NET_CACHE_ADDR       *PrevPtr;                                      /* Ptr to PREV   addr cache.                    */
    NET_CACHE_ADDR       *NextPtr;                                      /* Ptr to NEXT   addr cache.                    */
    void                 *ParentPtr;                                    /* Ptr to parent addr cache.                    */

    NET_BUF              *TxQ_Head;                                     /* Ptr to head of cache's buf Q.                */
    NET_BUF              *TxQ_Tail;                                     /* Ptr to tail of cache's buf Q.                */
    NET_BUF_QTY           TxQ_Nbr;

    NET_IF_NBR            IF_Nbr;                                       /* IF nbr of     addr cache.                    */

    CPU_INT16U            AccessedCtr;                                  /* Nbr successful srchs.                        */

    CPU_INT16U            Flags;                                        /* Cache flags.                                 */

    NET_CACHE_ADDR_TYPE   AddrHW_Type;                                  /* Remote hw       type     (see Note #2).      */
    CPU_INT08U            AddrHW_Len;                                   /* Remote hw       addr len (see Note #2).      */
    CPU_BOOLEAN           AddrHW_Valid;                                 /* Remote hw       addr        valid flag.      */

    NET_CACHE_ADDR_TYPE   AddrProtocolType;                             /* Remote protocol type     (see Note #2).      */
    CPU_INT08U            AddrProtocolLen;                              /* Remote protocol addr len (see Note #2).      */
    CPU_BOOLEAN           AddrProtocolValid;                            /* Remote protocol addr        valid flag.      */

    CPU_BOOLEAN           AddrProtocolSenderValid;                      /* Remote protocol addr sender valid flag.      */
};


#ifdef  NET_ARP_MODULE_EN
                                                                        /* ------------- NET CACHE ADDR ARP ----------- */
typedef  struct  net_cache_addr_arp  NET_CACHE_ADDR_ARP;

struct  net_cache_addr_arp {
    NET_CACHE_TYPE        Type;                                         /* Type cfg'd @ init : NET_ARP_TYPE_CACHE.      */

    NET_CACHE_ADDR_ARP   *PrevPtr;                                      /* Ptr to PREV       ARP addr  cache.           */
    NET_CACHE_ADDR_ARP   *NextPtr;                                      /* Ptr to NEXT       ARP addr  cache.           */
    void                 *ParentPtr;                                    /* Ptr to the parent ARP       cache.           */

    NET_BUF              *TxQ_Head;                                     /* Ptr to head of cache's buf Q.                */
    NET_BUF              *TxQ_Tail;                                     /* Ptr to tail of cache's buf Q.                */
    NET_BUF_QTY           TxQ_Nbr;

    NET_IF_NBR            IF_Nbr;                                       /* IF nbr of         ARP addr  cache.           */

    CPU_INT16U            AccessedCtr;                                  /* Nbr successful srchs.                        */

    CPU_INT16U            Flags;                                        /* Cache flags.                                 */

    NET_CACHE_ADDR_TYPE   AddrHW_Type;                                  /* Remote hw       type     (see Note #2).      */
    CPU_INT08U            AddrHW_Len;                                   /* Remote hw       addr len (see Note #2).      */
    CPU_BOOLEAN           AddrHW_Valid;                                 /* Remote hw       addr        valid flag.      */

    NET_CACHE_ADDR_TYPE   AddrProtocolType;                             /* Remote protocol type     (see Note #2).      */
    CPU_INT08U            AddrProtocolLen;                              /* Remote protocol addr len (see Note #2).      */
    CPU_BOOLEAN           AddrProtocolValid;                            /* Remote protocol addr        valid flag.      */

    CPU_BOOLEAN           AddrProtocolSenderValid;                      /* Remote protocol addr sender valid flag.      */

    CPU_INT08U            AddrHW[NET_IF_HW_ADDR_LEN_MAX];               /* Remote hw       addr.                        */

                                                                        /* Remote protocol addr.                        */
    CPU_INT08U            AddrProtocol[NET_IPv4_ADDR_SIZE];

                                                                        /* Sender protocol addr.                        */
    CPU_INT08U            AddrProtocolSender[NET_IPv4_ADDR_SIZE];
};
#endif


#ifdef  NET_NDP_MODULE_EN
                                                                        /* ------------- NET CACHE ADDR NDP ----------- */
typedef  struct  net_cache_addr_ndp  NET_CACHE_ADDR_NDP;

struct  net_cache_addr_ndp {
    NET_CACHE_TYPE       Type;                                          /* Type cfg'd @ init : NET_NDP_TYPE_CACHE.      */

    NET_CACHE_ADDR_NDP  *PrevPtr;                                       /* Ptr to PREV       NDP addr  cache.           */
    NET_CACHE_ADDR_NDP  *NextPtr;                                       /* Ptr to NEXT       NDP addr  cache.           */
    void                *ParentPtr;                                     /* Ptr to the parent NDP       cache.           */

    NET_BUF             *TxQ_Head;                                      /* Ptr to head of cache's buf Q.                */
    NET_BUF             *TxQ_Tail;                                      /* Ptr to tail of cache's buf Q.                */
    NET_BUF_QTY          TxQ_Nbr;

    NET_IF_NBR           IF_Nbr;                                        /* IF nbr of         NDP addr  cache.           */

    CPU_INT16U           AccessedCtr;                                   /* Nbr successful srchs.                        */

    CPU_INT16U           Flags;                                         /* Cache flags.                                 */

    NET_CACHE_ADDR_TYPE  AddrHW_Type;                                   /* Remote hw       type     (see Note #2).      */
    CPU_INT08U           AddrHW_Len;                                    /* Remote hw       addr len (see Note #2).      */
    CPU_BOOLEAN          AddrHW_Valid;                                  /* Remote hw       addr        valid flag.      */

    NET_CACHE_ADDR_TYPE  AddrProtocolType;                              /* Remote protocol type     (see Note #2).      */
    CPU_INT08U           AddrProtocolLen;                               /* Remote protocol addr len (see Note #2).      */
    CPU_BOOLEAN          AddrProtocolValid;                             /* Remote protocol addr        valid flag.      */

    CPU_BOOLEAN          AddrProtocolSenderValid;                       /* Remote protocol addr sender valid flag.      */

    CPU_INT08U           AddrHW[NET_IF_HW_ADDR_LEN_MAX];                /* Remote hw       addr.                        */

                                                                        /* Remote protocol addr.                        */
    CPU_INT08U           AddrProtocol[NET_IPv6_ADDR_SIZE];

                                                                        /* Sender protocol addr.                        */
    CPU_INT08U           AddrProtocolSender[NET_IPv6_ADDR_SIZE];

    CPU_INT08U           AddrProtocolPrefixLen;                         /* Remote protocol addr prefix len (in bits).   */

    KAL_SEM_HANDLE      *RxQ_SignalPtr;
};
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_ARP_MODULE_EN
NET_CACHE_EXT  NET_CACHE_ADDR_ARP   NetCache_AddrARP_Tbl[NET_ARP_CFG_CACHE_NBR];
NET_CACHE_EXT  NET_CACHE_ADDR_ARP  *NetCache_AddrARP_PoolPtr;           /* Ptr to pool of free ARP caches.              */
NET_CACHE_EXT  NET_STAT_POOL        NetCache_AddrARP_PoolStat;
#endif

#ifdef  NET_NDP_MODULE_EN
NET_CACHE_EXT  NET_CACHE_ADDR_NDP   NetCache_AddrNDP_Tbl[NET_NDP_CFG_CACHE_NBR];
NET_CACHE_EXT  NET_CACHE_ADDR_NDP  *NetCache_AddrNDP_PoolPtr;           /* Ptr to pool of free NDP caches.              */
NET_CACHE_EXT  NET_STAT_POOL        NetCache_AddrNDP_PoolStat;
#endif

NET_CACHE_EXT  NET_CACHE_ADDR      *NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_MAX];
NET_CACHE_EXT  NET_CACHE_ADDR      *NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_MAX];


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


void                NetCache_Init         (NET_CACHE_ADDR      *p_cache_parent,
                                           NET_CACHE_ADDR      *p_cache_addr,
                                           NET_ERR             *p_err);

                                                                            /* -------------- CFG FNCTS --------------- */
CPU_BOOLEAN         NetCache_CfgAccessedTh(NET_CACHE_TYPE       cache_type,
                                           CPU_INT16U           nbr_access);

NET_CACHE_ADDR     *NetCache_CfgAddrs     (NET_CACHE_TYPE       cache_type,
                                           NET_IF_NBR           if_nbr,
                                           CPU_INT08U          *p_addr_hw,
                                           NET_CACHE_ADDR_LEN   addr_hw_len,
                                           CPU_INT08U          *p_addr_protocol,
                                           CPU_INT08U          *p_addr_protocol_sender,
                                           NET_CACHE_ADDR_LEN   addr_protocol_len,
                                           CPU_BOOLEAN          timer_en,
                                           CPU_FNCT_PTR         timeout_fnct,
                                           NET_TMR_TICK         timeout_tick,
                                           NET_ERR             *p_err);

                                                                            /* -------------- MGMT FNCTS -------------- */
void               *NetCache_GetParent    (NET_CACHE_ADDR      *p_cache);

NET_CACHE_ADDR     *NetCache_AddrSrch     (NET_CACHE_TYPE       cache_type,
                                           NET_IF_NBR           if_nbr,
                                           CPU_INT08U          *p_addr_protocol,
                                           NET_CACHE_ADDR_LEN   addr_protocol_len,
                                           NET_ERR             *p_err);

void                NetCache_AddResolved  (NET_IF_NBR           if_nbr,
                                           CPU_INT08U          *p_addr_hw,
                                           CPU_INT08U          *p_addr_protocol,
                                           NET_CACHE_TYPE       cache_type,
                                           CPU_FNCT_PTR         fnct,
                                           NET_TMR_TICK         timeout_tick,
                                           NET_ERR             *p_err);

void                NetCache_Insert       (NET_CACHE_ADDR      *p_cache);

void                NetCache_Remove       (NET_CACHE_ADDR      *p_cache,
                                           CPU_BOOLEAN          tmr_free);

void                NetCache_UnlinkBuf    (NET_BUF             *p_buf);

                                                                            /* --------------- TX FNCTS --------------- */
void                NetCache_TxPktHandler (NET_PROTOCOL_TYPE    proto_type,
                                           NET_BUF             *p_buf_q,
                                           CPU_INT08U          *p_addr_hw);

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
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_CACHE_MODULE_EN      */
#endif  /* NET_CACHE_MODULE_PRESENT */

