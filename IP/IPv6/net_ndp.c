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
*                                          NETWORK NDP LAYER
*                                    (NEIGHBOR DISCOVERY PROTOCOL)
*
* Filename : net_ndp.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Neighbor Discovery Protocol as described in RFC #4861 with the
*                following restrictions/constraints :
*
*                (a) ONLY supports the following hardware types :
*                    (1) 48-bit Ethernet
*
*                (b) ONLY supports the following protocol types :
*                    (1) 128-bit IPv6 addresses
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    NET_NDP_MODULE
#include  "net_ndp.h"
#include  "net_ipv6.h"
#include  "net_mldp.h"
#include  "net_dad.h"
#include  "../../IF/net_if.h"
#include  "../../IF/net_if_802x.h"
#include  "../../Source/net.h"
#include  "../../Source/net_mgr.h"
#include  "../../Source/net_util.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_NDP_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_NDP_CACHE_FLAG_ISROUTER                     DEF_BIT_00

#define  NET_NDP_PREFIX_LEN_MAX                          128u

#define  NET_NDP_TWO_HOURS_SEC_NBR                      7200u
#define  NET_NDP_LIFETIME_TIMEOUT_TWO_HOURS             NET_NDP_TWO_HOURS_SEC_NBR * NET_TMR_TIME_TICK_PER_SEC

#define  NET_NDP_MS_NBR_PER_SEC                         1000u

#define  NET_NDP_CACHE_TX_Q_TH_MIN                         0
#define  NET_NDP_CACHE_TX_Q_TH_MAX                       NET_BUF_NBR_MAX


/*
*********************************************************************************************************
*                                    INTERFACE SELECTION DEFINES
*
* Notes : (1) This classification is used to found the good interface to Tx for a given destination
*             address.
*********************************************************************************************************
*/

#define  NET_NDP_IF_DEST_ON_LINK_WITH_SRC_ADDR_CFGD          7
#define  NET_NDP_IF_DEST_ON_LINK                             6
#define  NET_NDP_IF_DFLT_ROUTER_ON_LINK_WITH_SRC_ADDR_CFGD   5
#define  NET_NDP_IF_DFLT_ROUTER_ON_LINK                      4
#define  NET_NDP_IF_ROUTER_ON_LINK_WITH_SRC_ADDR_CFGD        3
#define  NET_NDP_IF_ROUTER_ON_LINK                           2
#define  NET_NDP_IF_SRC_ADDR_CFGD                            1
#define  NET_NDP_IF_NO_MATCH                                 0


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
*                               NDP NEIGHBOR SOLICITATION TYPE DATA TYPE
*********************************************************************************************************
*/

typedef enum net_ndp_neighbor_sol_type {
    NET_NDP_NEIGHBOR_SOL_TYPE_DAD = 1u,
    NET_NDP_NEIGHBOR_SOL_TYPE_RES = 2u,
    NET_NDP_NEIGHBOR_SOL_TYPE_NUD = 3u,
} NET_NDP_NEIGHBOR_SOL_TYPE;


/*
*********************************************************************************************************
*                                     NDP HEADERS OPTIONS DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_ndp_opt_hw_addr_hdr {
    NET_NDP_OPT_HDR  Opt;                                               /* NDP opt type and length.                     */
    CPU_INT08U       Addr[NET_IF_HW_ADDR_LEN_MAX];                      /* NDP hw addr.                                 */
} NET_NDP_OPT_HW_ADDR_HDR;


typedef  struct  net_ndp_opt_prefix_info_hdr {
    NET_NDP_OPT_HDR  Opt;                                               /* NDP opt type and length.                     */
    CPU_INT08U       PrefixLen;                                         /* NDP opt prefix info len (in bits).           */
    CPU_INT08U       Flags;                                             /* NDP opt prefix info flags.                   */
    CPU_INT32U       ValidLifetime;                                     /* NDP opt prefix info valid    lifetime.       */
    CPU_INT32U       PreferredLifetime;                                 /* NDP opt prefix info prefered lifetime.       */
    CPU_INT32U       Reserved;
    NET_IPv6_ADDR    Prefix;                                            /* NDP opt prefix info prefix addr.             */
} NET_NDP_OPT_PREFIX_INFO_HDR;


typedef  struct  net_ndp_opt_redirect_hdr {
    NET_NDP_OPT_HDR  Opt;                                               /* NDP opt type and length.                     */
    CPU_INT16U       Reserved1;
    CPU_INT32U       Reserved2;
    CPU_INT08U       Data;                                              /* NDP IP header & data.                        */
} NET_NDP_OPT_REDIRECT_HDR;


typedef  struct  net_ndp_opt_mtu_hdr {
    NET_NDP_OPT_HDR  Opt;                                               /* NDP opt type and length.                     */
    CPU_INT16U       Reserved;
    CPU_INT32U       MTU;                                               /* NDP MTU.                                     */
} NET_NDP_OPT_MTU_HDR;


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

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  KAL_SEM_HANDLE            NetNDP_RxRouterAdvSignal[NET_IF_NBR_IF_TOT];
#endif

static  NET_NDP_NEIGHBOR_CACHE    NetNDP_NeighborCacheTbl[NET_NDP_CFG_CACHE_NBR];   /* Neighbor Cache Table.            */

static  NET_NDP_ROUTER            NetNDP_RouterTbl[NET_NDP_CFG_ROUTER_NBR];         /* Routers List Table.              */

static  NET_NDP_PREFIX            NetNDP_PrefixTbl[NET_NDP_CFG_PREFIX_NBR];         /* Prefix List Table.               */

static  NET_NDP_DEST_CACHE        NetNDP_DestTbl[NET_NDP_CFG_DEST_NBR];             /* Destination Cache Table.         */

static  NET_NDP_ROUTER           *NetNDP_DefaultRouterTbl[NET_IF_NBR_IF_TOT];       /* Default Router Table.            */

                                                                                    /* Router pool variables.           */
static  NET_NDP_ROUTER           *NetNDP_RouterPoolPtr;
static  NET_NDP_ROUTER           *NetNDP_RouterListHead;
static  NET_NDP_ROUTER           *NetNDP_RouterListTail;
static  NET_STAT_POOL             NetNDP_RouterPoolStat;

static  NET_NDP_PREFIX           *NetNDP_PrefixPoolPtr;
static  NET_NDP_PREFIX           *NetNDP_PrefixListHead;
static  NET_NDP_PREFIX           *NetNDP_PrefixListTail;
static  NET_STAT_POOL             NetNDP_PrefixPoolStat;

static  NET_NDP_DEST_CACHE       *NetNDP_DestPoolPtr;
static  NET_NDP_DEST_CACHE       *NetNDP_DestListHead;
static  NET_NDP_DEST_CACHE       *NetNDP_DestListTail;
static  NET_STAT_POOL             NetNDP_DestPoolStat;

static  CPU_INT16U                NetNDP_ReachableTimeout_sec;
static  NET_TMR_TICK              NetNDP_ReachableTimeout_tick;

static  CPU_INT08U                NetNDP_SolicitMaxAttempsMulticast_nbr;    /* NDP mcast   solicit. max attempts nbr.   */
static  CPU_INT08U                NetNDP_SolicitMaxAttempsUnicast_nbr;      /* NDP unicast solicit. max attempts nbr.   */
static  CPU_INT08U                NetNDP_SolicitTimeout_sec;                /* NDP solicitations timeout (in secs ).    */
static  NET_TMR_TICK              NetNDP_SolicitTimeout_tick;               /* NDP solicitations timeout (in ticks).    */

#ifdef NET_DAD_MODULE_EN
static  CPU_INT08U                NetNDP_DADMaxAttempts_nbr;
#endif

static  NET_BUF_QTY               NetNDP_CacheTxQ_MaxTh_nbr;            /* Max nbr tx bufs to enqueue    on NDP cache.  */

/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static  void                 NetNDP_RxRouterAdvertisement    (const  NET_BUF                    *p_buf,
                                                                     NET_BUF_HDR                *p_buf_hdr,
                                                              const  NET_NDP_ROUTER_ADV_HDR     *p_ndp_hdr,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_RxNeighborSolicitation   (const  NET_BUF                    *p_buf,
                                                                     NET_BUF_HDR                *p_buf_hdr,
                                                              const  NET_NDP_NEIGHBOR_SOL_HDR   *p_ndp_hdr,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_RxNeighborAdvertisement  (const  NET_BUF                    *p_buf,
                                                                     NET_BUF_HDR                *p_buf_hdr,
                                                              const  NET_NDP_NEIGHBOR_ADV_HDR   *p_ndp_hdr,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_RxRedirect               (const  NET_BUF                    *p_buf,
                                                                     NET_BUF_HDR                *p_buf_hdr,
                                                              const  NET_NDP_REDIRECT_HDR       *p_ndp_hdr,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_RxPrefixUpdate           (       NET_IF_NBR                  if_nbr,
                                                                     NET_IPv6_ADDR              *p_addr_prefix,
                                                                     CPU_INT08U                  prefix_len,
                                                                     CPU_BOOLEAN                 on_link,
                                                                     CPU_BOOLEAN                 addr_cfg_auto,
                                                                     CPU_INT32U                  lifetime_valid,
                                                                     CPU_INT32U                  lifetime_preferred,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_RxPrefixHandler          (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr_prefix,
                                                                     CPU_INT08U                  prefix_len,
                                                                     CPU_INT32U                  lifetime_valid,
                                                                     CPU_BOOLEAN                 on_link,
                                                                     CPU_BOOLEAN                 addr_cfg_auto,
                                                                     NET_ERR                    *p_err);

static  CPU_BOOLEAN          NetNDP_RxPrefixAddrsUpdate      (       NET_IF_NBR                  if_nbr,
                                                                     NET_IPv6_ADDR              *p_addr_prefix,
                                                                     CPU_INT08U                  prefix_len,
                                                                     CPU_INT32U                  lifetime_valid,
                                                                     CPU_INT32U                  lifetime_preferred);

static  void                 NetNDP_TxNeighborSolicitation   (       NET_IF_NBR                  if_nbr,
                                                                     NET_IPv6_ADDR              *p_addr_src,
                                                                     NET_IPv6_ADDR              *p_addr_dest,
                                                                     NET_NDP_NEIGHBOR_SOL_TYPE   ndp_sol_type,
                                                                     NET_ERR                    *p_err);

static  CPU_BOOLEAN          NetNDP_IsPrefixCfgdOnAddr       (       NET_IPv6_ADDR              *p_addr,
                                                                     NET_IPv6_ADDR              *p_addr_prefix,
                                                                     CPU_INT08U                  p_refix_len);

static  NET_NDP_ROUTER      *NetNDP_UpdateDefaultRouter      (       NET_IF_NBR                  if_nbr,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_UpdateDestCache          (       NET_IF_NBR                  if_nbr,
                                                              const  CPU_INT08U                 *p_addr,
                                                              const  CPU_INT08U                 *p_addr_new);

static  void                 NetNDP_RemoveAddrDestCache      (       NET_IF_NBR                  if_nbr,
                                                              const  CPU_INT08U                 *p_addr);

static  void                 NetNDP_RemovePrefixDestCache    (       NET_IF_NBR                  if_nbr,
                                                              const  CPU_INT08U                 *p_prefix,
                                                                     CPU_INT08U                  prefix_len);

static  void                 NetNDP_NeighborCacheAddPend     (const  NET_BUF                    *p_buf,
                                                                     NET_BUF_HDR                *p_buf_hdr,
                                                              const  CPU_INT08U                 *p_addr_protocol,
                                                                     NET_ERR                    *p_err);

static  NET_CACHE_ADDR_NDP  *NetNDP_NeighborCacheAddEntry    (       NET_IF_NBR                  if_nbr,
                                                              const  CPU_INT08U                 *p_addr_hw,
                                                              const  CPU_INT08U                 *p_addr_ipv6,
                                                              const  CPU_INT08U                 *p_addr_ipv6_sender,
                                                                     NET_TMR_TICK                timeout_tick,
                                                                     CPU_FNCT_PTR                timeout_fnct,
                                                                     CPU_INT08U                  cache_state,
                                                                     CPU_BOOLEAN                 is_router,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_NeighborCacheUpdateEntry (       NET_CACHE_ADDR_NDP         *p_cache_addr_ndp,
                                                                     CPU_INT08U                 *p_ndp_opt_hw_addr);

static  void                 NetNDP_NeighborCacheRemoveEntry (       NET_NDP_NEIGHBOR_CACHE     *p_cache,
                                                                     CPU_BOOLEAN                 tmr_free);

static  CPU_BOOLEAN          NetNDP_RouterDfltGet            (       NET_IF_NBR                  if_nbr,
                                                                     NET_NDP_ROUTER            **p_router,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_ROUTER      *NetNDP_RouterCfg                (       NET_IF_NBR                  if_nbr,
                                                                     NET_IPv6_ADDR              *p_addr,
                                                                     CPU_BOOLEAN                 timer_en,
                                                                     CPU_FNCT_PTR                timeout_fnct,
                                                                     NET_TMR_TICK                timeout_tick,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_ROUTER      *NetNDP_RouterGet                (       NET_ERR                    *p_err);

static  NET_NDP_ROUTER      *NetNDP_RouterSrch               (       NET_IF_NBR                  if_nbr,
                                                                     NET_IPv6_ADDR              *p_addr,
                                                                     NET_ERR                    *p_err);

static  void                 NetNDP_RouterRemove             (       NET_NDP_ROUTER             *p_router,
                                                                     CPU_BOOLEAN                 tmr_free);

static  void                 NetNDP_RouterClr                (       NET_NDP_ROUTER             *p_router);

static  NET_NDP_PREFIX      *NetNDP_PrefixSrchMatchAddr      (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_PREFIX      *NetNDP_PrefixCfg                (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr_prefix,
                                                                     CPU_INT08U                  prefix_len,
                                                                     CPU_BOOLEAN                 timer_en,
                                                                     CPU_FNCT_PTR                timeout_fnct,
                                                                     NET_TMR_TICK                timeout_tick,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_PREFIX      *NetNDP_PrefixSrch               (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_PREFIX      *NetNDP_PrefixGet                (       NET_ERR                    *p_err);

static  void                 NetNDP_PrefixRemove             (       NET_NDP_PREFIX             *p_prefix,
                                                                     CPU_BOOLEAN                 tmr_free);

static  void                 NetNDP_PrefixClr                (       NET_NDP_PREFIX             *p_prefix);

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheCfg             (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr_dest,
                                                              const  NET_IPv6_ADDR              *p_addr_next_hop,
                                                                     CPU_BOOLEAN                 is_valid,
                                                                     CPU_BOOLEAN                 on_link,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheGet             (       NET_ERR                    *p_err);

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheSrch            (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr,
                                                                     NET_ERR                    *p_err);

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheSrchInvalid     (       NET_ERR                    *p_err);

static  void                 NetNDP_DestCacheRemove          (       NET_NDP_DEST_CACHE         *p_dest);

static  void                 NetNDP_DestCacheClr             (       NET_NDP_DEST_CACHE         *p_dest);

static  CPU_BOOLEAN          NetNDP_IsAddrOnLink             (       NET_IF_NBR                  if_nbr,
                                                              const  NET_IPv6_ADDR              *p_addr);

static  void                 NetNDP_SolicitTimeout           (       void                       *p_cache_timeout);

static  void                 NetNDP_ReachableTimeout         (       void                       *p_cache_timeout);


static  void                 NetNDP_RouterTimeout            (       void                       *p_router_timeout);

static  void                 NetNDP_PrefixTimeout            (       void                       *p_prefix_timeout);


#ifdef  NET_DAD_MODULE_EN
static  void                 NetNDP_DAD_Timeout              (       void                       *p_cache_timeout);
#endif

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  void                 NetNDP_RouterAdvSignalPost      (       NET_IF_NBR                  if_nbr,
                                                                     NET_ERR                    *p_err);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             NetNDP_Init()
*
* Description : (1) Initialize Neighbor Discovery Protocol Layer :
*
*                   (a) Initialize NDP DAD signals.
*                   (b) Initialize NDP address cache pool.
*                   (c) Initialize NDP neighbor cache table.
*                   (d) Initialize NDP address cache.
*                   (e) Initialize NDP address cache list pointers.
*                   (f) Initialize NDP router table.
*                   (g) Initialize NDP router list pointers.
*                   (h) Initialize NDP prefix table.
*                   (i) Initialize NDP prefix list pointers.
*                   (j) Initialize NDP destination cache table.
*                   (k) Initialize NDP destination cache list pointers.
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetNDP_Init (NET_ERR  *p_err)
{
    NET_NDP_NEIGHBOR_CACHE      *p_cache;
    NET_NDP_ROUTER              *p_router;
    NET_NDP_PREFIX              *p_prefix;
    NET_NDP_DEST_CACHE          *p_dest;
    NET_NDP_CACHE_QTY            i;


                                                                /* ------------ INIT RX ROUTER ADV SIGNAL ------------- */
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    for (i = 0; i < NET_IF_NBR_IF_TOT; i++) {
        NetNDP_RxRouterAdvSignal[i].SemObjPtr = DEF_NULL;
    }
#endif

                                                                /* ------------ INIT NDP CACHE POOL/STATS ------------- */
    NetCache_AddrNDP_PoolPtr = DEF_NULL;                        /* Init-clr NDP addr. cache pool (see Note #2b).        */

    NetStat_PoolInit(&NetCache_AddrNDP_PoolStat,
                      NET_NDP_CFG_CACHE_NBR,
                      p_err);

                                                                /* ------------ INIT NDP NEIGHBOR CACHE TBL ----------- */
    p_cache = &NetNDP_NeighborCacheTbl[0];
    for (i = 0u; i < NET_NDP_CFG_CACHE_NBR; i++) {
        p_cache->Type           =  NET_CACHE_TYPE_NDP;
        p_cache->CacheAddrPtr   = &NetCache_AddrNDP_Tbl[i];     /* Init each NDP addr cache ptr.                        */
        p_cache->ReqAttemptsCtr =  0u;
        p_cache->State          =  NET_NDP_CACHE_STATE_NONE;    /* Init each NDP cache as free/NOT used.                */
        p_cache->Flags          =  NET_CACHE_FLAG_NONE;

        NetCache_Init((NET_CACHE_ADDR *)p_cache,                /* Init each NDP addr cache.                            */
                      (NET_CACHE_ADDR *)p_cache->CacheAddrPtr,
                                        p_err);
        p_cache++;
    }


                                                                /* ------------- INIT NDP CACHE LIST PTRS ------------- */
    NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP] = DEF_NULL;
    NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_NDP] = DEF_NULL;


                                                                /* ------------ INIT NDP ROUTER POOL/STATS ------------- */
    NetStat_PoolInit(&NetNDP_RouterPoolStat,
                      NET_NDP_CFG_ROUTER_NBR,
                      p_err);

                                                                /* ---------------- INIT ROUTER TABLE ----------------- */
    p_router = &NetNDP_RouterTbl[0];
    for (i = 0u; i < NET_NDP_CFG_ROUTER_NBR; i++) {
        p_router->IF_Nbr  = NET_IF_NBR_NONE;

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        Mem_Clr((void     *)&p_router->Addr,
                (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);
#endif

        p_router->TmrPtr       = DEF_NULL;
        p_router->NDP_CachePtr = DEF_NULL;
        p_router->NextPtr      = NetNDP_RouterPoolPtr;          /* Free Router to Router pool (see Note #2).            */
        NetNDP_RouterPoolPtr   = p_router;
        p_router++;
    }

                                                                /* -------------- INIT ROUTER LIST PTRS --------------- */
    NetNDP_RouterListHead = DEF_NULL;
    NetNDP_RouterListTail = DEF_NULL;


                                                                /* ------------ INIT NDP PREFIX POOL/STATS ------------ */
    NetStat_PoolInit(&NetNDP_PrefixPoolStat,
                      NET_NDP_CFG_PREFIX_NBR,
                      p_err);


                                                                /* ---------------- INIT PREFIX TABLE ----------------- */
    p_prefix = &NetNDP_PrefixTbl[0];
    for (i = 0u; i < NET_NDP_CFG_PREFIX_NBR; i++) {


#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        Mem_Clr((void     *)&p_prefix->Prefix,
                (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);
#endif
        p_prefix->IF_Nbr     = NET_IF_NBR_NONE;
        p_prefix->TmrPtr     = DEF_NULL;
        p_prefix->NextPtr    = NetNDP_PrefixPoolPtr;            /* Free Prefix to Prefix pool.                          */
        NetNDP_PrefixPoolPtr = p_prefix;
        p_prefix++;
    }


                                                                /* -------------- INIT PREFIX LIST PTRS --------------- */
    NetNDP_PrefixListHead = DEF_NULL;
    NetNDP_PrefixListTail = DEF_NULL;

                                                                /* --------- INIT NDP DESTINATION POOL/STATS ---------- */
    NetStat_PoolInit(&NetNDP_DestPoolStat,
                      NET_NDP_CFG_DEST_NBR,
                      p_err);

                                                                /* -------------- INIT DESTINATION TABLE -------------- */
    p_dest = &NetNDP_DestTbl[0];
    for (i = 0u; i < NET_NDP_CFG_DEST_NBR; i++) {
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        Mem_Clr((void     *)&p_dest->AddrDest,
                (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);

        Mem_Clr((void     *)&p_dest->AddrNextHop,
                (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);
#endif
        p_dest->IF_Nbr     = NET_IF_NBR_NONE;
        p_dest->OnLink     = DEF_NO;
        p_dest->IsValid    = DEF_NO;
        p_dest->NextPtr    = NetNDP_DestPoolPtr;                /* Free Destination Cache to Destination Cache pool.    */
        NetNDP_DestPoolPtr = p_dest;
        p_dest++;
    }

                                                                /* ------------ INIT DESTINATION LIST PTRS ------------ */
    NetNDP_DestListHead = DEF_NULL;
    NetNDP_DestListTail = DEF_NULL;


   *p_err = NET_NDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetNDP_NeighborCacheHandler()
*
* Description : (1) Resolve destination hardware address using NDP :
*
*                   (a) Search NDP Cache List for NDP cache with corresponding protocol address.
*                   (b) If NDP cache found, handle packet based on NDP cache state :
*
*                       (1) INCOMPLETE  -> Enqueue transmit packet buffer to NDP cache
*                       (2) REACHABLE   -> Copy NDP cache's hardware address to data packet;
*                                          Return to Network Interface to transmit data packet
*
*                   (c) If NDP cache NOT found, allocate new NDP cache in 'INCOMPLETE' state (see Note #1b1)
*
*                       See 'net_cache.h  CACHE STATES' for cache state diagram.
*
*               (2) This NDP cache handler function assumes the following :
*
*                   (a) ALL NDP caches in the NDP Cache List are valid. [validated by NetCache_AddrGet()]
*                   (b) ANY NDP cache in the 'INCOMPLETE' state MAY have already enqueued at LEAST one
*                           transmit packet buffer when NDP cache allocated. [see NetCache_AddrGet()]
*                   (c) ALL NDP caches in the 'REACHABLE' state have valid hardware addresses.
*                   (d) ALL transmit buffers enqueued on any NDP cache are valid.
*                   (e) Buffer's NDP address pointers pre-configured by Network Interface to point to :
*
*                       (1) 'NDP_AddrProtocolPtr'               Pointer to the protocol address used to
*                                                                   resolve the hardware address
*                       (2) 'NDP_AddrHW_Ptr'                    Pointer to memory buffer to return the
*                                                                   resolved hardware address
*                       (3) NDP addresses                       Which MUST be in network-order
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit.
*               -----       Argument checked by caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NEIGHBOR_CACHE_PEND         NDP neighbor cache in pending state.
*                               NET_NDP_ERR_NEIGHBOR_CACHE_RESOLVED     NDP neighbor cache resolved.
*                               NET_NDP_ERR_NEIGHBOR_CACHE_STALE        NDP neighbor cache in stale state.
*                               NET_NDP_ERR_FAULT                       NDP error fault.
*                               NET_ERR_FAULT_NULL_PTR                  Argument passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_Tx().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (3) Since the primary tasks of the network protocol suite are prevented from running
*                   concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                   resources of the NDP Cache List since no asynchronous access from other network
*                   tasks is possible.
*
*               (4) (a) RFC #4861, Section 7.2.2 states that :
*
*                       (1) "While waiting for address resolution to complete, the sender MUST,
*                            for each neighbor, retain a small queue of packets waiting for
*                            address resolution to complete.  The queue MUST hold at least one
*                            packet, and MAY contain more. ... Once address resolution completes,
*                            the node transmits any queued packets."
*
*                   (b) Since NDP Layer is the last layer to handle & queue the transmit network
*                       buffer, it is NOT necessary to increment the network buffer's reference
*                       counter to include the pending NDP cache buffer queue as a new reference
*                       to the network buffer.
*
*               (5) Some buffer controls were previously initialized in NetBuf_Get() when the packet
*                   was received at the network interface layer. These buffer controls do NOT need
*                   to be re-initialized but are shown for completeness.
*
*               (6) A resolved multicast address still remains resolved even if any error(s) occur
*                   while adding it to the NDP cache.
*********************************************************************************************************
*/

void   NetNDP_NeighborCacheHandler (const  NET_BUF  *p_buf,
                                           NET_ERR  *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_IF_NBR               if_nbr;
    NET_PROTOCOL_TYPE        protocol_type_net;
    CPU_BOOLEAN              addr_protocol_multicast;
    CPU_INT08U               addr_hw_len;
#endif
    NET_BUF_HDR             *p_buf_hdr;
    CPU_INT08U              *p_addr_hw;
    CPU_INT08U              *p_addr_protocol;
    NET_BUF_HDR             *p_tail_buf_hdr;
    NET_BUF                 *p_tail_buf;
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_ICMPv6_HDR          *p_icmp_hdr;
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_TMR_TICK             timeout_tick;
    CPU_INT08U               cache_state;
    NET_BUF_QTY              buf_max_th;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE PTRS ------------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

    p_buf_hdr       = (NET_BUF_HDR *)&p_buf->Hdr;
    p_addr_hw       =                 p_buf_hdr->NDP_AddrHW_Ptr;
    p_addr_protocol =                 p_buf_hdr->NDP_AddrProtocolPtr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_hw == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
    if (p_addr_protocol == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

    if_nbr = p_buf_hdr->IF_Nbr;

                                                                /* ------- VALIDATE IF PACKET IS OF ICMPv6 TYPE ------- */
    if (p_buf_hdr->ICMP_MsgIx != NET_BUF_IX_NONE) {
        p_icmp_hdr = (NET_ICMPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->ICMP_MsgIx];
    } else {
        p_icmp_hdr = (NET_ICMPv6_HDR *)0;
    }

                                                                /* ------------------ SRCH NDP CACHE ------------------ */
    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(NET_CACHE_TYPE_NDP,
                                                               if_nbr,
                                                               p_addr_protocol,
                                                               NET_IPv6_ADDR_SIZE,
                                                               p_err);
    switch (*p_err) {
        case NET_CACHE_ERR_NOT_FOUND:
        case NET_CACHE_ERR_RESOLVED:
        case NET_CACHE_ERR_PEND:
             break;


        case NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN:
        case NET_CACHE_ERR_INVALID_TYPE:
        default:
            *p_err = NET_NDP_ERR_FAULT;
             goto exit;
    }

    if (p_cache_addr_ndp != (NET_CACHE_ADDR_NDP *)0) {           /* If NDP cache found, chk state.                       */
        p_cache     = (NET_NDP_NEIGHBOR_CACHE *) p_cache_addr_ndp->ParentPtr;
        cache_state = p_cache->State;
        switch (cache_state) {
            case NET_NDP_CACHE_STATE_INCOMPLETE:                /* If NDP cache pend, append buf into Q (see Note #4a1).*/
                 CPU_CRITICAL_ENTER();
                 buf_max_th = NetNDP_CacheTxQ_MaxTh_nbr;
                 CPU_CRITICAL_EXIT();

                 if (p_cache_addr_ndp->TxQ_Nbr >= buf_max_th) {
                    *p_err = NET_CACHE_ERR_UNRESOLVED;
                     return;
                 }

                 p_tail_buf = p_cache_addr_ndp->TxQ_Tail;
                 if (p_tail_buf != (NET_BUF *)0) {              /* If Q NOT empty,    append buf @ Q tail.              */
                     p_tail_buf_hdr                 = &p_tail_buf->Hdr;
                     p_tail_buf_hdr->NextSecListPtr = (NET_BUF *)p_buf;
                     p_buf_hdr->PrevSecListPtr      = (NET_BUF *)p_tail_buf;
                     p_cache_addr_ndp->TxQ_Tail     = (NET_BUF *)p_buf;

                 } else {                                       /* Else add buf as first q'd buf.                       */
                     p_cache_addr_ndp->TxQ_Head     = (NET_BUF *)p_buf;
                     p_cache_addr_ndp->TxQ_Tail     = (NET_BUF *)p_buf;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #5].                */
                     p_buf_hdr->PrevSecListPtr      = (NET_BUF *)0;
                     p_buf_hdr->NextSecListPtr      = (NET_BUF *)0;
#endif
                 }
                                                                /* Cfg buf's unlink fnct/obj to NDP cache.              */
                 p_buf_hdr->UnlinkFnctPtr = (NET_BUF_FNCT)&NetCache_UnlinkBuf;
                 p_buf_hdr->UnlinkObjPtr  = (void       *) p_cache_addr_ndp;

                *p_err                    =  NET_NDP_ERR_NEIGHBOR_CACHE_PEND;
                 goto exit;


            case NET_NDP_CACHE_STATE_REACHABLE:                 /* If NDP cache REACHABLE, copy hw addr.                 */
                *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_RESOLVED;
                 goto exit_mem_copy;


            case NET_NDP_CACHE_STATE_STALE:
            case NET_NDP_CACHE_STATE_DLY:
                 if (p_icmp_hdr != (NET_ICMPv6_HDR *)0) {
                     if (p_icmp_hdr->Type != NET_ICMPv6_MSG_TYPE_NDP_NEIGHBOR_ADV) {
                         p_cache->State = NET_NDP_CACHE_STATE_DLY;
                         CPU_CRITICAL_ENTER();
                         timeout_tick = NetNDP_DelayTimeout_tick;
                         CPU_CRITICAL_EXIT();
                         NetTmr_Set(p_cache->TmrPtr,
                                    NetNDP_DelayTimeout,
                                    timeout_tick,
                                    p_err);
                         if (*p_err != NET_TMR_ERR_NONE) {
                              p_cache->State = NET_NDP_CACHE_STATE_STALE;
                         }
                     }
                 }
                *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_STALE;
                 goto exit_mem_copy;


            case NET_NDP_CACHE_STATE_PROBE:
                *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_STALE;
                 goto exit_mem_copy;


            default:
                 NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_YES);
                 NetNDP_NeighborCacheAddPend(p_buf, p_buf_hdr, p_addr_protocol, p_err);
                 if (*p_err != NET_NDP_ERR_NEIGHBOR_CACHE_PEND) {
                     *p_err = NET_NDP_ERR_FAULT;
                      goto exit;
                 }
                *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_PEND;
                 goto exit;
        }

    } else {
#ifdef  NET_MCAST_MODULE_EN
        protocol_type_net       = p_buf_hdr->ProtocolHdrTypeNet;
        addr_protocol_multicast = NetMgr_IsAddrProtocolMulticast(protocol_type_net,
                                                                 p_addr_protocol,
                                                                 NET_IPv6_ADDR_SIZE);

        if (addr_protocol_multicast == DEF_YES) {               /* If multicast protocol addr,      ...                 */
            addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
                                                                /* ... convert to multicast hw addr ...                 */
            NetIF_AddrMulticastProtocolToHW(if_nbr,
                                            p_addr_protocol,
                                            NET_IPv6_ADDR_SIZE,
                                            protocol_type_net,
                                            p_addr_hw,
                                           &addr_hw_len,
                                            p_err);
            if (*p_err != NET_IF_ERR_NONE) {
                *p_err  = NET_NDP_ERR_FAULT;
                 goto exit;
            }

           *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_RESOLVED;        /* Rtn resolved multicast hw addr (see Note #6).        */
            goto exit;
        }
#endif
        NetNDP_NeighborCacheAddPend(p_buf, p_buf_hdr, p_addr_protocol, p_err);
        if (*p_err != NET_NDP_ERR_NEIGHBOR_CACHE_PEND) {
            *p_err  = NET_NDP_ERR_FAULT;
             goto exit;
        }

       *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_PEND;
        goto exit;
    }


exit_mem_copy:
    Mem_Copy(p_addr_hw,
             p_cache_addr_ndp->AddrHW,
             NET_IF_HW_ADDR_LEN_MAX);

exit:
    return;
}


/*
*********************************************************************************************************
*                                              NetNDP_Rx()
*
* Description : (1) Process received NDP packets :
*
*                   (a) Demultiplex received ICMPv6 packet according to the ICMPv6/NDP Type.
*                   (b) Update receive statistics
*
* Argument(s) : p_buf       Pointer to network buffer that received the NDP packet.
*               -----       Argument checked by caller(s).
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument checked by caller(s).
*
*               p_icmp_hdr  Pointer to received packet's ICMP header.
*               ----------  Argument validated by caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_INVALID_TYPE        ICMPv6 Type is not a NDP valid type.
*
*                               ------------- RETURNED BY NetNDP_RxRouterAdvertisement() -------------
*                               See NetNDP_RxRouterAdvertisement() for additional return error codes.
*
*                               ------------ RETURNED BY NetNDP_RxNeighborSolicitation() -------------
*                               See NetNDP_RxNeighborSolicitation() for additional return error codes.
*
*                               ------------ RETURNED BY NetNDP_RxNeighborAdvertisement() ------------
*                               See NetNDP_RxNeighborAdvertisement() for additional return error codes.
*
*                               ------------------ RETURNED BY NetNDP_RxRedirect() -------------------
*                               See NetNDP_RxRedirect() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_Rx().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetNDP_Rx (const  NET_BUF         *p_buf,
                        NET_BUF_HDR     *p_buf_hdr,
                 const  NET_ICMPv6_HDR  *p_icmp_hdr,
                        NET_ERR         *p_err)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTRS ------------------ */
    if (p_buf_hdr == (NET_BUF_HDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (p_icmp_hdr == (NET_ICMPv6_HDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ICMPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    switch (p_icmp_hdr->Type) {
        case NET_ICMPv6_MSG_TYPE_NDP_ROUTER_ADV:
             NetNDP_RxRouterAdvertisement(                          p_buf,
                                                                    p_buf_hdr,
                                          (NET_NDP_ROUTER_ADV_HDR *)p_icmp_hdr,
                                                                    p_err);
             NET_CTR_STAT_INC(Net_StatCtrs.NDP.RxMsgAdvRouterCtr);
             break;


        case NET_ICMPv6_MSG_TYPE_NDP_NEIGHBOR_SOL:
             NetNDP_RxNeighborSolicitation(                            p_buf,
                                                                       p_buf_hdr,
                                           (NET_NDP_NEIGHBOR_SOL_HDR *)p_icmp_hdr,
                                                                       p_err);
             NET_CTR_STAT_INC(Net_StatCtrs.NDP.RxMsgSolNborCtr);
             break;


        case NET_ICMPv6_MSG_TYPE_NDP_NEIGHBOR_ADV:
             NetNDP_RxNeighborAdvertisement(                                 p_buf,
                                                                             p_buf_hdr,
                                           (const NET_NDP_NEIGHBOR_ADV_HDR *)p_icmp_hdr,
                                                                             p_err);
             NET_CTR_STAT_INC(Net_StatCtrs.NDP.RxMsgAdvNborCtr);
             break;


        case NET_ICMPv6_MSG_TYPE_NDP_REDIRECT:
             NetNDP_RxRedirect(                        p_buf,
                                                       p_buf_hdr,
                               (NET_NDP_REDIRECT_HDR *)p_icmp_hdr,
                                                       p_err);
             NET_CTR_STAT_INC(Net_StatCtrs.NDP.RxMsgRedirectCtr);
             break;


        default:
            *p_err = NET_NDP_ERR_INVALID_TYPE;
             return;
    }
}


/*
*********************************************************************************************************
*                                     NetNDP_TxRouterSolicitation()
*
* Description : (1) Transmit router solicitation message:
*
*                   (a) Set IPv6 source      address. See Note #2.
*                   (b) Set IPv6 destination address as the multicast all-routers address.
*                   (c) Transmit ICMP message.
*
*
* Argument(s) : if_nbr      Network interface number to transmit Router Solicitation message.
*
*               p_addr_src  Pointer to IPv6 source address (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                               --- RETURNED BY NetICMPv6_HandlerTxMsgReq() : ---
*                               NET_ICMPv6_ERR_NONE             ICMPv6 Request Message successfully transmitted.
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_TX                      Transmit error; packet discarded.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : AutoConfig functions.
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (2) If IPv6 source address pointer is NULL, the unspecified address will be used.
*********************************************************************************************************
*/

void  NetNDP_TxRouterSolicitation (NET_IF_NBR      if_nbr,
                                   NET_IPv6_ADDR  *p_addr_src,
                                   NET_ERR        *p_err)
{
    NET_IPv6_ADDR   addr_all_routers_mcast;
    NET_IPv6_ADDR   addr_unspecified;
    NET_IPv6_ADDR  *p_ndp_addr_src;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
                                                                /* ---------------- VALIDATE POINTERS ----------------- */
    if (p_addr_src == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* ---------------- SET SOURCE ADDRESS ---------------- */
    p_ndp_addr_src = p_addr_src;
    if (p_ndp_addr_src == (NET_IPv6_ADDR *)0) {
        NetIPv6_AddrUnspecifiedSet(&addr_unspecified, p_err);
        p_ndp_addr_src = &addr_unspecified;
    }

                                                                /* -------------- SET DESTINATION ADDRESS ------------- */
    NetIPv6_AddrMcastAllRoutersSet(&addr_all_routers_mcast, DEF_NO, p_err);

                                                                /* ------------ TX NDP ROUTER SOLICITATION ------------ */
                                                                /* Tx router solicitation.                              */
   (void)NetICMPv6_TxMsgReqHandler(if_nbr,
                                   NET_ICMPv6_MSG_TYPE_NDP_ROUTER_SOL,
                                   NET_ICMPv6_MSG_CODE_NDP_ROUTER_SOL,
                                   0u,
                                   p_ndp_addr_src,
                                  &addr_all_routers_mcast,
                                   NET_IPv6_HDR_HOP_LIM_MAX,
                                   DEF_NO,
                                   DEF_NULL,
                                   0,
                                   0,
                                   p_err);
   if (*p_err != NET_ICMPv6_ERR_NONE) {
       *p_err = NET_NDP_ERR_TX;
        return;
   }

  *p_err = NET_NDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetNDP_NextHopByIF()
*
* Description : Find Next Hop for the given destination address and interface.
*               Add new Destination cache if none present.
*
* Argument(s) : if_nbr          Interface number on which packet will be send.
*
*               p_addr_dest     Pointer to IPv6 destination address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_TX_DEST_HOST_THIS_NET    Destination is on same link.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY          Next-hop is the default router.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY_NONE     Next-hop is a router in the router list.
*                               NET_IPv6_ERR_TX_NEXT_HOP_NONE         No next-hop is available.
*
*                               --------- RETURNED BY NetIF_IsValidCfgdHandler() ---------
*                               See NetIF_IsValidCfgdHandler() for additional error codes.
*
* Return(s)   : Pointer to the next-hop ipv6 address
*
* Caller(s)   : NetIPv6_GetAddrSrcHandler(),
*               NetIPv6_TxPktDatagramRouteSel().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) The interface number given is assumed to be the good interface to used to reach
*                   the destination.
*
*               (2) Use function NetNDP_NextHop() when interface is unknown.
*********************************************************************************************************
*/

const  NET_IPv6_ADDR  *NetNDP_NextHopByIF (       NET_IF_NBR      if_nbr,
                                           const  NET_IPv6_ADDR  *p_addr_dest,
                                                  NET_ERR        *p_err)
{
    NET_NDP_DEST_CACHE  *p_dest_cache;
    NET_NDP_ROUTER      *p_router;
    NET_IPv6_ADDR       *p_addr_nexthop;
    CPU_BOOLEAN          on_link;
    CPU_BOOLEAN          addr_mcast;
    CPU_BOOLEAN          dflt_router;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        return ((NET_IPv6_ADDR *)0);
    }
#endif

                                                                /* --------- CHECK IF DEST ADDR IS MULTICAST ---------- */
    addr_mcast = NetIPv6_IsAddrMcast(p_addr_dest);
    if (addr_mcast == DEF_YES) {
       *p_err = NET_NDP_ERR_TX_DEST_MULTICAST;
        return (p_addr_dest);
    }

                                                                /* -------- CHECK FOR DESTINATION CACHE ENTRY --------- */
    p_dest_cache = NetNDP_DestCacheSrch(if_nbr, p_addr_dest, p_err);
    if (p_dest_cache != (NET_NDP_DEST_CACHE *)0) {              /* Destination cache exists for current destination.    */
        if (p_dest_cache->IsValid == DEF_YES) {
            p_addr_nexthop = &p_dest_cache->AddrNextHop;
            if (p_dest_cache->OnLink == DEF_YES) {
               *p_err = NET_NDP_ERR_TX_DEST_HOST_THIS_NET;
            } else {
               *p_err = NET_NDP_ERR_TX_DFLT_GATEWAY;
            }
            return (p_addr_nexthop);
        }
    } else {
        p_dest_cache = NetNDP_DestCacheCfg(if_nbr,
                                           p_addr_dest,
                                           DEF_NULL,
                                           DEF_NO,
                                           DEF_NO,
                                           p_err);
        if (*p_err != NET_NDP_ERR_NONE) {
            return ((NET_IPv6_ADDR *)0);
        }
    }
                                                                /* ---------- CHECK IF DESTINAION IS ON LINK ---------- */
    on_link = NetNDP_IsAddrOnLink(if_nbr,
                                  p_addr_dest);
    if (on_link == DEF_YES) {
        p_dest_cache->IsValid = DEF_YES;
        p_dest_cache->OnLink  = DEF_YES;
        Mem_Copy(&p_dest_cache->AddrNextHop, p_addr_dest, NET_IPv6_ADDR_SIZE);
       *p_err = NET_NDP_ERR_TX_DEST_HOST_THIS_NET;
        return (p_addr_dest);
    }

                                                                /* ------------- CHECK FOR ROUTER ON LINK ------------- */
    dflt_router = NetNDP_RouterDfltGet(if_nbr, &p_router, p_err);
    switch (*p_err) {
        case NET_NDP_ERR_NONE:
             if (dflt_router == DEF_YES) {
                 p_dest_cache->IsValid = DEF_YES;
                 p_dest_cache->OnLink  = DEF_NO;
                *p_err = NET_NDP_ERR_TX_DFLT_GATEWAY;
             } else {
                 p_dest_cache->IsValid = DEF_NO;
                 p_dest_cache->OnLink  = DEF_NO;
                *p_err = NET_NDP_ERR_TX_NO_DFLT_GATEWAY;
             }
             break;


        case NET_NDP_ERR_ROUTER_NOT_FOUND:
        case NET_NDP_ERR_FAULT:
        default:
            p_dest_cache->IsValid = DEF_NO;
            p_dest_cache->OnLink  = DEF_NO;
           *p_err = NET_NDP_ERR_TX_NO_NEXT_HOP;
            return ((NET_IPv6_ADDR *)0);
    }


    p_addr_nexthop = (NET_IPv6_ADDR *)&p_router->Addr;
    Mem_Copy(&p_dest_cache->AddrNextHop, p_addr_nexthop, NET_IPv6_ADDR_SIZE);


    return (p_addr_nexthop);
}


/*
*********************************************************************************************************
*                                           NetNDP_NextHop()
*
* Description : Find Next Hop and the best Interface for the given destination address.
*
* Argument(s) : p_if_nbr        Pointer to variable that will received the interface number.
*
*               p_addr_src      Pointer to IPv6 suggested source address if any.
*
*               p_addr_dest     Pointer to IPv6 destination address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_TX_DEST_HOST_THIS_NET    Destination is on same link.
*                               NET_IPv6_ERR_TX_DEST_DFLT_GATEWAY     Next-hop is the default router.
*                               NET_IPv6_ERR_TX_DEST_NO_DFLT_GATEWAY  Next-hop is a router in the router list.
*                               NET_IPv6_ERR_TX_DEST_NONE             No next-hop is available.
*
* Return(s)   : Pointer to the next-hop ipv6 address
*
* Caller(s)   : NetIPv6_GetAddrSrcHandler().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) Destination address that are Link-Local are ambiguous, no interface can be chosen
*                   as being adequate for the destination since link local prefix is the same for any
*                   network. Therefore the default inteface is selected.
*
*               (2) To found the Interface for a Multicast destination address, the function checks
*                   if any interface as joined the MLDP group of the address.
*
*               (3) This function finds the best matching interface for the given destination address
*                   according to the following classification :
*
*                   (a) Destination is on same link as the interface and the received source address is
*                       also configured on the interface.
*
*                   (b) Destination is on same link as the interface.
*
*                   (c) A Default router exit on the same link as the interface and the received source is
*                       also configured on the interface.
*
*                   (d) A default router exit on the same link as the interface.
*
*                   (e) A router is present on the same link as the interface and the received source
*                       address is also configured on the interface.
*
*                   (f) The received source address is configured on the interface.
*
*                   (g) No Interface is adequate for the destination address.
*********************************************************************************************************
*/

const  NET_IPv6_ADDR  *NetNDP_NextHop (       NET_IF_NBR     *p_if_nbr,
                                       const  NET_IPv6_ADDR  *p_addr_src,
                                       const  NET_IPv6_ADDR  *p_addr_dest,
                                              NET_ERR        *p_err)
{
           NET_IF_NBR           if_nbr;
           NET_NDP_DEST_CACHE  *p_dest;
           NET_NDP_DEST_CACHE  *p_dest_tmp;
           NET_NDP_ROUTER      *p_router;
    const  NET_IPv6_ADDR       *p_addr_nexthop;
           NET_IPv6_ADDR       *p_addr_nexthop_tmp;
           NET_IF_NBR           if_nbr_tmp;
           NET_IF_NBR           if_nbr_src_addr;
           CPU_INT08U           valid_nbr_current;
           CPU_INT08U           valid_nbr_best;
           CPU_BOOLEAN          link_local;
           CPU_BOOLEAN          addr_mcast;
           CPU_BOOLEAN          on_link;
           CPU_BOOLEAN          add_dest;
           CPU_BOOLEAN          add_dest_tmp;
           CPU_BOOLEAN          dflt_router;
           CPU_BOOLEAN          is_mcast_grp;
           CPU_BOOLEAN          if_found;


                                                                /* --------- CHECK IF DEST ADDR IS LINK LOCAL --------- */
    link_local = NetIPv6_IsAddrLinkLocal(p_addr_dest);
    if (link_local == DEF_YES) {
       *p_if_nbr = NetIF_GetDflt();
        p_addr_nexthop = NetNDP_NextHopByIF(*p_if_nbr, p_addr_dest, p_err);
        return (p_addr_nexthop);
    }

                                                                /* --------- CHECK IF DEST ADDR IS MULTICAST ---------- */
    addr_mcast = NetIPv6_IsAddrMcast(p_addr_dest);
    if (addr_mcast == DEF_YES) {
        if_nbr     = NET_IF_NBR_BASE_CFGD;
        if_nbr_tmp = NET_IF_NBR_BASE_CFGD;
        if_found   = DEF_NO;
        for ( ; if_nbr_tmp < NET_IF_NBR_IF_TOT; if_nbr_tmp++) {
            is_mcast_grp = NetMLDP_IsGrpJoinedOnIF(if_nbr_tmp, p_addr_dest);
            if (is_mcast_grp == DEF_YES) {
                if_found  = DEF_YES;
                if_nbr    = if_nbr_tmp;
            }
        }
        if (if_found == DEF_YES) {
           *p_if_nbr = if_nbr;
           *p_err    = NET_NDP_ERR_TX_DEST_MULTICAST;
        } else {
           *p_if_nbr = NET_IF_NBR_NONE;
           *p_err    = NET_NDP_ERR_TX_NO_NEXT_HOP;
        }
        return(p_addr_dest);
    }

                                                                /* ----------- FOUND BEST NEXT HOP ADDRESS ------------ */
    if (p_addr_src != (NET_IPv6_ADDR *)0) {
        if_nbr_src_addr = NetIPv6_GetAddrHostIF_Nbr(p_addr_src);
    }

    if_nbr            = NET_IF_NBR_BASE_CFGD;
    if_nbr_tmp        = NET_IF_NBR_BASE_CFGD;
    valid_nbr_best    = 0;
    valid_nbr_current = 0;
    for ( ; if_nbr_tmp < NET_IF_NBR_IF_TOT; if_nbr_tmp++) {
                                                                /* -------------- CHECK DESTINATION CACHE ------------- */
        p_dest_tmp = NetNDP_DestCacheSrch(if_nbr_tmp, p_addr_dest, p_err);
                                                                /* Destination cache exists for destination ...         */
                                                                /* ... and destination cache is valid.                  */
        if ((p_dest_tmp          != (NET_NDP_DEST_CACHE *)0) &&
            (p_dest_tmp->IsValid == DEF_YES)                ) {

            p_addr_nexthop_tmp = &p_dest_tmp->AddrNextHop;

            if(p_dest_tmp->OnLink == DEF_YES) {
                valid_nbr_current = NET_NDP_IF_DEST_ON_LINK;
            } else {
                valid_nbr_current = NET_NDP_IF_DFLT_ROUTER_ON_LINK;
            }

            add_dest_tmp = DEF_NO;

                                                                /* Destination cache doesn't exists for destination ... */
                                                                /* ... or is invalid.                                   */
        } else {

            if (p_dest_tmp == (NET_NDP_DEST_CACHE *)0) {
                add_dest_tmp = DEF_YES;
            } else {
                add_dest_tmp = DEF_NO;
            }

                                                                /* --------- CHECK IF DESTINATION IS ON LINK ---------- */
            on_link = NetNDP_IsAddrOnLink(if_nbr_tmp,
                                          p_addr_dest);
            if (on_link == DEF_YES) {
                valid_nbr_current  = NET_NDP_IF_DEST_ON_LINK;
                p_addr_nexthop_tmp = (NET_IPv6_ADDR *)p_addr_dest;

            } else {
                                                                /* ------------ CHECK FOR ROUTER ON LINK -------------- */
                if (NetNDP_DefaultRouterTbl[if_nbr_tmp] == (NET_NDP_ROUTER *)0) {

                    dflt_router = NetNDP_RouterDfltGet(if_nbr_tmp, &p_router, p_err);
                    if (p_router != (NET_NDP_ROUTER *)0) {
                        if (dflt_router == DEF_YES) {
                            valid_nbr_current = NET_NDP_IF_DFLT_ROUTER_ON_LINK;
                        } else {
                            valid_nbr_current = NET_NDP_IF_ROUTER_ON_LINK;
                        }
                        p_addr_nexthop_tmp = &p_router->Addr;
                    } else {
                        p_addr_nexthop_tmp = (NET_IPv6_ADDR *)0;
                       *p_err = NET_NDP_ERR_TX_NO_NEXT_HOP;
                    }
                } else {
                    valid_nbr_current = NET_NDP_IF_DFLT_ROUTER_ON_LINK;
                    p_addr_nexthop_tmp = &NetNDP_DefaultRouterTbl[if_nbr_tmp]->Addr;
                   *p_err = NET_NDP_ERR_TX_DFLT_GATEWAY;
                }
            }
        }

        if (if_nbr_src_addr == if_nbr_tmp) {
            valid_nbr_current++;
        }

        if (valid_nbr_current >= valid_nbr_best) {
            valid_nbr_best = valid_nbr_current;
            if_nbr         = if_nbr_tmp;
            p_addr_nexthop = p_addr_nexthop_tmp;
            add_dest       = add_dest_tmp;
            if (add_dest == DEF_NO) {
                 p_dest  = p_dest_tmp;
            }
        }
    }

                                                                /* Add a Destination Cache if none exist for dest.      */
    if (add_dest == DEF_YES) {
        p_dest = NetNDP_DestCacheCfg(if_nbr,
                                     p_addr_dest,
                                     p_addr_nexthop,
                                     DEF_NO,
                                     DEF_NO,
                                     p_err);
        if (*p_err != NET_NDP_ERR_NONE) {
             return ((NET_IPv6_ADDR *)0);
        }
    }

                                                                /* Set return status.                                   */
    switch (valid_nbr_best) {
        case NET_NDP_IF_DEST_ON_LINK_WITH_SRC_ADDR_CFGD:
        case NET_NDP_IF_DEST_ON_LINK:
             p_dest->IsValid = DEF_YES;
             p_dest->OnLink  = DEF_YES;
            *p_err = NET_NDP_ERR_TX_DEST_HOST_THIS_NET;
             break;

        case NET_NDP_IF_DFLT_ROUTER_ON_LINK_WITH_SRC_ADDR_CFGD:
        case NET_NDP_IF_DFLT_ROUTER_ON_LINK:
             p_dest->IsValid = DEF_YES;
             p_dest->OnLink  = DEF_NO;
            *p_err = NET_NDP_ERR_TX_DFLT_GATEWAY;
             break;


        case NET_NDP_IF_ROUTER_ON_LINK_WITH_SRC_ADDR_CFGD:
        case NET_NDP_IF_ROUTER_ON_LINK:
             p_dest->IsValid = DEF_NO;
             p_dest->OnLink  = DEF_NO;
            *p_err = NET_NDP_ERR_TX_NO_DFLT_GATEWAY;
             break;

        case NET_NDP_IF_SRC_ADDR_CFGD:
        case NET_NDP_IF_NO_MATCH:
        default:
             p_dest->IsValid = DEF_NO;
             p_dest->OnLink  = DEF_NO;
            *p_err = NET_NDP_ERR_TX_NO_NEXT_HOP;
             break;
    }

   *p_if_nbr = if_nbr;

    return (p_addr_nexthop);
}


/*
*********************************************************************************************************
*                                   NetNDP_CfgNeighborCacheTimeout()
*
* Description : Configure NDP Neighbor timeout from NDP Neighbor cache list.
*
* Argument(s) : timeout_sec     Desired value for NDP neighbor timeout (in seconds).
*
* Return(s)   : DEF_OK,   NDP neighbor cache timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application,
*               Net_InitDflt().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) Timeout in seconds converted to 'NET_TMR_TICK' ticks in order to pre-compute initial
*                   timeout value in 'NET_TMR_TICK' ticks.
*
*               (2) 'NetNDP_NeighborCacheTimeout' variables MUST ALWAYS be accessed exclusively in
*                    critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetNDP_CfgNeighborCacheTimeout (CPU_INT16U  timeout_sec)
{
    NET_TMR_TICK  timeout_tick;
    CPU_SR_ALLOC();


#if (NET_NDP_CACHE_TIMEOUT_MIN_SEC > DEF_INT_16U_MIN_VAL)
    if (timeout_sec < NET_NDP_CACHE_TIMEOUT_MIN_SEC) {
        return (DEF_FAIL);
    }
#endif
#if (NET_NDP_CACHE_TIMEOUT_MAX_SEC < DEF_INT_16U_MAX_VAL)
    if (timeout_sec > NET_NDP_CACHE_TIMEOUT_MAX_SEC) {
        return (DEF_FAIL);
    }
#endif

    timeout_tick = (NET_TMR_TICK)timeout_sec * NET_TMR_TIME_TICK_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetNDP_NeighborCacheTimeout_sec  =  timeout_sec;
    NetNDP_NeighborCacheTimeout_tick =  timeout_tick;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}

/*
*********************************************************************************************************
*                                      NetNDP_CfgReachabilityTimeout()
*
* Description : Configure possible NDP Neighbor reachability timeouts.
*
* Argument(s) : timeout_type    NDP timeout type.
*
*                                   NET_NDP_TIMEOUT_REACHABLE
*                                   NET_NDP_TIMEOUT_DELAY
*                                   NET_NDP_TIMEOUT_SOLICIT
*
*               timeout_sec     Desired value for NDP neighbor reachable timeout (in seconds).
*
* Return(s)   : DEF_OK,   NDP neighbor cache timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application,
*               Net_InitDflt().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetNDP_CfgReachabilityTimeout (NET_NDP_TIMEOUT  timeout_type,
                                            CPU_INT16U       timeout_sec)
{
    NET_TMR_TICK  timeout_tick;
    CPU_SR_ALLOC();


    timeout_tick = (NET_TMR_TICK)timeout_sec * NET_TMR_TIME_TICK_PER_SEC;

    switch (timeout_type) {
        case NET_NDP_TIMEOUT_REACHABLE:
             if (timeout_sec < NET_NDP_REACHABLE_TIMEOUT_MIN_SEC) {
                 return (DEF_FAIL);
             }
             if (timeout_sec > NET_NDP_REACHABLE_TIMEOUT_MAX_SEC) {
                 return (DEF_FAIL);
             }
             CPU_CRITICAL_ENTER();
             NetNDP_ReachableTimeout_sec  =  timeout_sec;
             NetNDP_ReachableTimeout_tick =  timeout_tick;
             CPU_CRITICAL_EXIT();
            (void)&NetNDP_ReachableTimeout_sec;
             break;


        case NET_NDP_TIMEOUT_DELAY:
             if (timeout_sec < NET_NDP_DELAY_FIRST_PROBE_TIMEOUT_MIN_SEC) {
                 return (DEF_FAIL);
             }
             if (timeout_sec > NET_NDP_DELAY_FIRST_PROBE_TIMEOUT_MAX_SEC) {
                 return (DEF_FAIL);
             }
             CPU_CRITICAL_ENTER();
             NetNDP_DelayTimeout_sec  =  timeout_sec;
             NetNDP_DelayTimeout_tick =  timeout_tick;
             CPU_CRITICAL_EXIT();
             break;


        case NET_NDP_TIMEOUT_SOLICIT:
             if (timeout_sec < NET_NDP_RETRANS_TIMEOUT_MIN_SEC) {
                 return (DEF_FAIL);
             }
             if (timeout_sec > NET_NDP_RETRANS_TIMEOUT_MAX_SEC) {
                 return (DEF_FAIL);
             }
             CPU_CRITICAL_ENTER();
             NetNDP_SolicitTimeout_sec  = timeout_sec;
             NetNDP_SolicitTimeout_tick = timeout_tick;
             CPU_CRITICAL_EXIT();
            (void)&NetNDP_SolicitTimeout_sec;
             break;


        default:
            return (DEF_FAIL);
    }

    return (DEF_OK);
}

/*
*********************************************************************************************************
*                                  NetNDP_CfgSolicitMaxNbr()
*
* Description : Configure NDP maximum number of NDP Solicitation sent for the given type of solicitation.
*
* Argument(s) : solicit_type    NDP Solicitation message type :
*
*                                   NET_NDP_SOLICIT_MULTICAST
*                                   NET_NDP_SOLICIT_UNICAST
*                                   NET_NDP_SOLICIT_DAD
*
*               max_nbr         Desired maximum number of NDP solicitation attempts.
*
* Return(s)   : DEF_OK,   NDP Request maximum number of solicitation attempts configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application,
*               Net_InitDflt().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetNDP_CfgSolicitMaxNbr (NET_NDP_SOLICIT  solicit_type,
                                      CPU_INT08U       max_nbr)
{
    CPU_SR_ALLOC();


    switch (solicit_type) {
        case NET_NDP_SOLICIT_MULTICAST:
#if (NET_NDP_SOLICIT_NBR_MIN > DEF_INT_08U_MIN_VAL)
             if (max_nbr < NET_NDP_SOLICIT_NBR_MIN) {
                 return (DEF_FAIL);
             }
#endif
#if (NET_NDP_SOLICIT_NBR_MAX < DEF_INT_08U_MAX_VAL)
             if (max_nbr > NET_NDP_SOLICIT_NBR_MAX) {
                 return (DEF_FAIL);
             }
#endif
             CPU_CRITICAL_ENTER();
             NetNDP_SolicitMaxAttempsMulticast_nbr = max_nbr;
             CPU_CRITICAL_EXIT();
             break;


        case NET_NDP_SOLICIT_UNICAST:
#if (NET_NDP_SOLICIT_NBR_MIN > DEF_INT_08U_MIN_VAL)
             if (max_nbr < NET_NDP_SOLICIT_NBR_MIN) {
                 return (DEF_FAIL);
             }
#endif
#if (NET_NDP_SOLICIT_NBR_MAX < DEF_INT_08U_MAX_VAL)
             if (max_nbr > NET_NDP_SOLICIT_NBR_MAX) {
                 return (DEF_FAIL);
             }
#endif
            CPU_CRITICAL_ENTER();
            NetNDP_SolicitMaxAttempsUnicast_nbr = max_nbr;
            CPU_CRITICAL_EXIT();
                     break;


        case NET_NDP_SOLICIT_DAD:
#ifdef NET_DAD_MODULE_EN
#if (NET_NDP_SOLICIT_NBR_MIN > DEF_INT_08U_MIN_VAL)
             if (max_nbr < NET_NDP_SOLICIT_NBR_MIN) {
                 return (DEF_FAIL);
             }
#endif
#if (NET_NDP_SOLICIT_NBR_MAX < DEF_INT_08U_MAX_VAL)
             if (max_nbr > NET_NDP_SOLICIT_NBR_MAX) {
                 return (DEF_FAIL);
             }
#endif
             CPU_CRITICAL_ENTER();
             NetNDP_DADMaxAttempts_nbr = max_nbr;
             CPU_CRITICAL_EXIT();
#endif
             break;


        default:
            return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      NetNDP_CfgCacheTxQ_MaxTh()
*
* Description : Configure NDP cache maximum number of transmit packet buffers to enqueue.
*
* Argument(s) : nbr_buf_max     Desired maximum number of transmit packet buffers to enqueue onto an
*
* Return(s)   : DEF_OK,   NDP cache transmit packet buffer threshold configured.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) 'NetNDP_CacheTxQ_MaxTh_nbr' MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetNDP_CfgCacheTxQ_MaxTh (NET_BUF_QTY  nbr_buf_max)
{
    CPU_SR_ALLOC();


#if (NET_NDP_CACHE_TX_Q_TH_MIN > DEF_INT_16U_MIN_VAL)
    if (nbr_buf_max < NET_NDP_CACHE_TX_Q_TH_MIN) {
        return (DEF_FAIL);
    }
#endif

#if (NET_NDP_CACHE_TX_Q_TH_MAX < DEF_INT_16U_MAX_VAL)
    if (nbr_buf_max > NET_NDP_CACHE_TX_Q_TH_MAX) {
        return (DEF_FAIL);
    }
#endif

    CPU_CRITICAL_ENTER();
    NetNDP_CacheTxQ_MaxTh_nbr = nbr_buf_max;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         NetNDP_CacheTimeout()
*
* Description : Discard an NDP cache in the 'STALE' state on timeout.
*
* Argument(s) : p_cache_timeout     Pointer to an NDP cache (see Note #2b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetCache_AddResolve(),
*                               NetNDP_RxRouterAdvertisement(),
*                               NetNDP_RxNeighborSolicitation(),
*                               NetNDP_RxNeighborAdvertisement(),
*                               NetNDP_RxRedirect(),
*                               NetNDP_NeighborCacheUpdateEntry(),
*                               NetNDP_ReachableTimeout().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
*
* Note(s)     : (1) RFC #4861 section 7.3.3 Node Behavior.
*
*               (2) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (3) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetCache_AddrFree() via NetCache_Remove().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/

void  NetNDP_CacheTimeout (void  *p_cache_timeout)
{
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_NDP_ROUTER          *p_router;
    CPU_BOOLEAN              is_router;
    NET_TMR_TICK             timeout_tick;
    NET_ERR                  err;
    CPU_SR_ALLOC();


    p_cache = (NET_NDP_NEIGHBOR_CACHE *)p_cache_timeout;        /* See Note #2b2A.                                      */

    p_cache_addr_ndp = p_cache->CacheAddrPtr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP CACHE ---------------- */
    if (p_cache == (NET_NDP_NEIGHBOR_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_cache->TmrPtr = (NET_TMR *)0;                             /* Un-reference tmr in the NDP Neighbor cache.          */

    is_router = DEF_BIT_IS_SET(p_cache->Flags,NET_NDP_CACHE_FLAG_ISROUTER);

    if (is_router == DEF_TRUE) {                                /* If the addr is still a valid router don't delete it. */

        p_router = NetNDP_RouterSrch(                  p_cache_addr_ndp->IF_Nbr,
                                     (NET_IPv6_ADDR *)&p_cache_addr_ndp->AddrProtocol[0],
                                                      &err);

        if (p_router != (NET_NDP_ROUTER *)0) {
            CPU_CRITICAL_ENTER();
            timeout_tick = (NET_TMR_TICK)p_router->LifetimeSec * NET_TMR_TIME_TICK_PER_SEC;
            CPU_CRITICAL_EXIT();

            p_cache->TmrPtr = NetTmr_Get(        NetNDP_CacheTimeout,
                                         (void *)p_cache,
                                                 timeout_tick,
                                                &err);
            if (err != NET_TMR_ERR_NONE) {                      /* If tmr unavailable, free NDP cache.                  */
                NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);
                return;
            }

        } else {
            NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);
        }

    } else {
        NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);
    }

}


/*
*********************************************************************************************************
*                                         NetNDP_DelayTimeout()
*
* Description : Change the NDP cache state to PROBE if the state of the NDP cache is still at DELAY when
*               the timer end.
*
* Argument(s) : p_cache_timeout     Pointer to an NDP cache.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetNDP_CacheHandler(),
*                               NetCache_TxPktHandler().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetNDP_CacheFree() via NetNDP_CacheRemove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/

void  NetNDP_DelayTimeout (void  *p_cache_timeout)
{
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_TMR_TICK             timeout_tick;
    NET_ERR                  err;
    CPU_SR_ALLOC();


    p_cache = (NET_NDP_NEIGHBOR_CACHE *)p_cache_timeout;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP CACHE ---------------- */
    if (p_cache == (NET_NDP_NEIGHBOR_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }

#endif


    p_cache->TmrPtr = (NET_TMR *)0;                             /* Deference already freed timer.                       */

    if(p_cache->State == NET_NDP_CACHE_STATE_DLY) {
        p_cache->State = NET_NDP_CACHE_STATE_PROBE;
        CPU_CRITICAL_ENTER();
        timeout_tick = NetNDP_SolicitTimeout_tick;
        CPU_CRITICAL_EXIT();

        p_cache->TmrPtr = NetTmr_Get(        NetNDP_SolicitTimeout,
                                     (void *)p_cache,
                                             timeout_tick,
                                            &err);
        if (err != NET_TMR_ERR_NONE) {                          /* If tmr unavail, free NDP cache.                      */
            NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);
            return;
        }
    }
}


/*
*********************************************************************************************************
*                                        NetNDP_DAD_Start()
*
* Description : (1) Start duplicate address detection procedure:
*
*                   (a) Use a new cache entry to save address info.
*
* Argument(s) : if_nbr      Network interface number to perform duplicate address detection.
*
*               p_addr      Pointer on the IPv6 addr to perform duplicate address detection.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                DAD process successfully started.
*
*                               ------------- RETURNED by NetNDP_NeighborCacheAddEntry() ------------
*                               See NetNDP_NeighborCacheAddEntry() for additional return error codes.
*
* Return(s)   : DEF_OK,   if duplicate address detection started successfully,
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_CfgAddrAddHandler(),
*               NetNDP_DAD_RetryTimeout().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) NetNDP_DAD_Start() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/
#ifdef NET_DAD_MODULE_EN
void  NetNDP_DAD_Start(NET_IF_NBR          if_nbr,
                       NET_IPv6_ADDR      *p_addr,
                       NET_ERR            *p_err)
{
    NET_TMR_TICK          timeout_tick;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* ---------- CREATE NEW NDP CACHE FOR ADDR ----------- */
    CPU_CRITICAL_ENTER();
    timeout_tick = NetNDP_SolicitTimeout_tick;
    CPU_CRITICAL_EXIT();

    (void)NetNDP_NeighborCacheAddEntry(               if_nbr,
                                                      DEF_NULL,
                                       (CPU_INT08U *) p_addr,
                                                      DEF_NULL,
                                                      timeout_tick,
                                                      NetNDP_DAD_Timeout,
                                                      NET_NDP_CACHE_STATE_INCOMPLETE,
                                                      DEF_NO,
                                                      p_err);
}
#endif


/*
*********************************************************************************************************
*                                           NetNDP_DAD_Stop()
*
* Description : Remove the NDP Neighbor cache entry associated with the DAD process.
*
* Argument(s) : if_nbr  Interface number of the address on which DAD is occurring.
*
*               p_addr  Pointer to the IPv6 address on which DAD is occurring.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_CfgAddrAddHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) NetNDP_DAD_Stop() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/
#ifdef NET_DAD_MODULE_EN
void  NetNDP_DAD_Stop(NET_IF_NBR      if_nbr,
                      NET_IPv6_ADDR  *p_addr)
{
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_ERR                  err_net;


    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(               NET_CACHE_TYPE_NDP,
                                                                              if_nbr,
                                                               (CPU_INT08U *) p_addr,
                                                                              NET_IPv6_ADDR_SIZE,
                                                                             &err_net);
    if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
        return;
    }

    p_cache = (NET_NDP_NEIGHBOR_CACHE *)p_cache_addr_ndp->ParentPtr;

    NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_YES);
}
#endif


/*
*********************************************************************************************************
*                                        NetNDP_DAD_GetMaxAttemptsNbr()
*
* Description : Get the number of DAD attempts configured.
*
* Argument(s) : None.
*
* Return(s)   : Number of DAD attempts configured.
*
* Caller(s)   : NetIPv6_DAD_Start().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) NetNDP_DAD_GetMaxAttemptsNbr() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/
#ifdef NET_DAD_MODULE_EN
CPU_INT08U  NetNDP_DAD_GetMaxAttemptsNbr (void)
{
    CPU_INT08U     dad_retx_nbr;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    dad_retx_nbr = NetNDP_DADMaxAttempts_nbr;
    CPU_CRITICAL_EXIT();

    return(dad_retx_nbr);
}
#endif


/*
*********************************************************************************************************
*                                    NetNDP_RouterAdvSignalCreate()
*
* Description : Create Signal for Rx Router Advertisement message.
*
* Argument(s) : if_nbr      Network Interface number.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                        Signal created successfully.
*                               NET_ERR_FAULT_MEM_ALLOC                 Memory allocation error for signal.
*                               NET_NDP_ERR_ROUTER_ADV_SIGNAL_CREATE    Creation signal error.
*
* Return(s)   : Pointer to Signal handle for the given interface.
*
* Caller(s)   : NetIPv6_CfgAddrGlobal().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
KAL_SEM_HANDLE  *NetNDP_RouterAdvSignalCreate(NET_IF_NBR   if_nbr,
                                              NET_ERR      *p_err)
{
    KAL_SEM_HANDLE  *p_signal;
    KAL_ERR          err_kal;


    p_signal = &NetNDP_RxRouterAdvSignal[if_nbr];
   *p_signal =  KAL_SemCreate((const CPU_CHAR *)NET_NDP_RX_ROUTER_ADV_SIGNAL_NAME,
                                                DEF_NULL,
                                               &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_ERR_FAULT_MEM_ALLOC;
             goto exit;


        case KAL_ERR_ISR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_CREATE:
        default:
            *p_err = NET_NDP_ERR_ROUTER_ADV_SIGNAL_CREATE;
             goto exit;
    }

   *p_err = NET_NDP_ERR_NONE;


exit:
    return (p_signal);
}
#endif


/*
*********************************************************************************************************
*                                     NetNDP_RouterAdvSignalPend()
*
* Description : Wait for Rx Router Advertisement Signal.
*
* Argument(s) : p_signal    Pointer to signal handle.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                        Signal received.
*                               NET_NDP_ERR_ROUTER_ADV_SIGNAL_TIMEOUT   Pending on signal timed out.
*                               NET_NDP_ERR_ROUTER_ADV_SIGNAL_FAULT     Pending on signal faulted.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_CfgAddrGlobal().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
void  NetNDP_RouterAdvSignalPend(KAL_SEM_HANDLE  *p_signal,
                                 NET_ERR         *p_err)
{
    KAL_ERR  err_kal;


    KAL_SemPend(*p_signal,
                 KAL_OPT_PEND_BLOCKING,
                 NET_NDP_RX_ROUTER_ADV_TIMEOUT_MS,
                &err_kal);
    switch (err_kal) {
       case KAL_ERR_NONE:
           *p_err = NET_NDP_ERR_NONE;
            break;


       case KAL_ERR_TIMEOUT:
           *p_err = NET_NDP_ERR_ROUTER_ADV_SIGNAL_TIMEOUT;
            break;


       case KAL_ERR_ABORT:
       case KAL_ERR_ISR:
       case KAL_ERR_WOULD_BLOCK:
       case KAL_ERR_OS:
       default:
           *p_err = NET_NDP_ERR_ROUTER_ADV_SIGNAL_FAULT;
            break;
    }
}
#endif


/*
*********************************************************************************************************
*                                    NetNDP_RouterAdvSignalRemove()
*
* Description : Delete given Router Adv Signal.
*
* Argument(s) : p_signal    Pointer to signal handle.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_CfgAddrGlobal().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
void  NetNDP_RouterAdvSignalRemove(KAL_SEM_HANDLE  *p_signal)
{
    KAL_ERR  err_kal;


    KAL_SemDel(*p_signal, &err_kal);

    p_signal->SemObjPtr = DEF_NULL;
}
#endif


/*
*********************************************************************************************************
*                                         NetNDP_PrefixAddCfg()
*
* Description : Add IPv6 prefix configuration in the prefix pool.
*
* Argument(s) : if_nbr          Interface number.
*
*               p_addr_prefix   Pointer to IPv6 prefix.
*
*               prefix_len      Prefix length
*
*               timer_en        Indicate whether are not to set a network timer for the prefix:
*
*                               DEF_YES                Set network timer for prefix.
*                               DEF_NO          Do NOT set network timer for prefix.
*
*               timeout_fnct    Pointer to timeout function.
*
*               timeout_tick    Timeout value (in 'NET_TMR_TICK' ticks).
*
*               p_err           Pointer to variable that will receive the return error code from this function:
*
*
* Return(s)   : Pointer to configured prefix entry.
*
* Caller(s)   : IxANVL testing tools.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Function for testing purpose.
*********************************************************************************************************
*/

NET_NDP_PREFIX  *NetNDP_PrefixAddCfg (       NET_IF_NBR      if_nbr,
                                      const  NET_IPv6_ADDR  *p_addr_prefix,
                                             CPU_INT08U      prefix_len,
                                             CPU_BOOLEAN     timer_en,
                                             CPU_FNCT_PTR    timeout_fnct,
                                             NET_TMR_TICK    timeout_tick,
                                             NET_ERR        *p_err)
{
    NET_NDP_PREFIX  *p_prefix;


    p_prefix = DEF_NULL;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_addr_prefix == DEF_NULL) {
        *p_err =  NET_ERR_FAULT_NULL_PTR;
         goto exit_fault;
    }

    if ((timer_en     == DEF_YES)  &&
        (timeout_fnct == DEF_NULL)) {
        *p_err =  NET_ERR_FAULT_NULL_PTR;
         goto exit_fault;
    }
#endif

    p_prefix = (NET_NDP_PREFIX *)0;

    Net_GlobalLockAcquire((void *)&NetNDP_PrefixAddCfg, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_fault;
    }


    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {                            /* If cfg'd IF NOT en'd (see Note #4), ...              */
         goto exit_release;                                     /* ... rtn err.                                         */
    }


    p_prefix = NetNDP_PrefixSrch(if_nbr,
                                 p_addr_prefix,
                                 p_err);
    if (*p_err == NET_NDP_ERR_NONE) {
        goto exit_release;
    }


    NetNDP_PrefixCfg(if_nbr,
                     p_addr_prefix,
                     prefix_len,
                     timer_en,
                     timeout_fnct,
                     timeout_tick,
                     p_err);
    if (*p_err != NET_NDP_ERR_NONE) {
        goto exit_release;
    }


    *p_err = NET_NDP_ERR_NONE;


exit_release:
    Net_GlobalLockRelease();

exit_fault:
    return (p_prefix);
}


/*
*********************************************************************************************************
*                                       NetNDP_DestCacheAddCfg()
*
* Description : Add IPv6 NDP Destination cache configuration in the Destination Cache pool.
*
* Argument(s) : if_nbr              Interface number for the destination to configure.
*
*               p_addr_dest         Pointer to IPv6 Destination address.
*
*               p_addr_next_hop     Pointer to Next-Hop IPv6 address.
*
*               is_valid            Indicate whether are not the Next-Hop address is valid.
*                                       DEF_YES, address is   valid
*                                       DEF_NO,  address is invalid
*
*               on_link             Indicate whether are not the Destination is on link.
*                                       DEF_YES, destination is     on link
*                                       DEF_NO,  destination is not on link
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Destination successfully configured.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_addr_dest'/'p_addr_next_hop'
*                                                                   passed a NULL pointer
*                               NET_NDP_ERR_DEST_NONE_AVAIL     NO available destination to allocate.
*
* Return(s)   : Pointer to destination entry configured.
*
* Caller(s)   : IxANVL testing tools.
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_NDP_DEST_CACHE  *NetNDP_DestCacheAddCfg(       NET_IF_NBR      if_nbr,
                                            const  NET_IPv6_ADDR  *p_addr_dest,
                                            const  NET_IPv6_ADDR  *p_addr_next_hop,
                                                   CPU_BOOLEAN     is_valid,
                                                   CPU_BOOLEAN     on_link,
                                                   NET_ERR        *p_err)
{
    NET_NDP_DEST_CACHE  *p_dest;


    p_dest = DEF_NULL;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_addr_dest == DEF_NULL) {
        *p_err =  NET_ERR_FAULT_NULL_PTR;
         goto exit_fault;
    }
#endif

    Net_GlobalLockAcquire((void *)&NetNDP_DestCacheAddCfg, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_fault;
    }


    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {                            /* If cfg'd IF NOT en'd (see Note #4), ...              */
         goto exit_release;                                     /* ... rtn err.                                         */
    }


    p_dest = NetNDP_DestCacheSrch(if_nbr,
                                  p_addr_dest,
                                  p_err);
    if (*p_err == NET_NDP_ERR_NONE) {
        goto exit_release;
    }

    NetNDP_DestCacheCfg(if_nbr,
                        p_addr_dest,
                        DEF_NULL,
                        DEF_YES,
                        DEF_NO,
                        p_err);
    if (*p_err != NET_NDP_ERR_NONE) {
        goto exit_release;
    }


   *p_err = NET_NDP_ERR_NONE;

   (void)&p_addr_next_hop;
   (void)&is_valid;
   (void)&on_link;


exit_release:
    Net_GlobalLockRelease();

exit_fault:
    return (p_dest);
}


/*
*********************************************************************************************************
*                                      NetNDP_DestCacheRemoveCfg()
*
* Description : Add IPv6 NDP Destination cache configuration in the Destination Cache pool.
*
* Argument(s) : if_nbr              Interface number for the destination to configure.
*
*               p_addr_dest         Pointer to IPv6 Destination address.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Destination successfully removed.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_addr_dest'/'p_addr_next_hop'
*                                                                   passed a NULL pointer
*
* Return(s)   : none.
*
* Caller(s)   : IxANVL testing tools.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetNDP_DestCacheRemoveCfg (       NET_IF_NBR      if_nbr,
                                 const  NET_IPv6_ADDR  *p_addr_dest,
                                        NET_ERR        *p_err)
{
    NET_NDP_DEST_CACHE  *p_dest;


    p_dest = DEF_NULL;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_addr_dest == DEF_NULL) {
        *p_err =  NET_ERR_FAULT_NULL_PTR;
         goto exit_fault;
    }
#endif

    Net_GlobalLockAcquire((void *)&NetNDP_DestCacheAddCfg, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_fault;
    }

    p_dest = NetNDP_DestCacheSrch(if_nbr,
                                  p_addr_dest,
                                  p_err);
    if (*p_err != NET_NDP_ERR_NONE) {
        goto exit_release;
    }


    NetNDP_DestCacheRemove(p_dest);


   *p_err = NET_NDP_ERR_NONE;

exit_release:
    Net_GlobalLockRelease();

exit_fault:
    return;
}


/*
*********************************************************************************************************
*                                       NetNDP_CacheClrAll()
*
* Description : Clear all entries of the NDP cache, Router list, Prefix list and Destination cache.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : IxANVL testing tools.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Function for testing purpose.
*********************************************************************************************************
*/

void  NetNDP_CacheClrAll (void)
{
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_NDP_ROUTER          *p_router;
    NET_NDP_PREFIX          *p_prefix;
    NET_NDP_DEST_CACHE      *p_dest;
    CPU_INT08U               i;
    NET_ERR                  err;


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetNDP_CacheClrAll, &err);
    if (err != NET_ERR_NONE) {
         goto exit_fault;
    }

    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
        goto exit_release;
    }


                                                                /* Clear NDP Addr Cache and Neighbor Cache.             */
    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP];
    while (p_cache_addr_ndp != DEF_NULL) {

        NetNDP_RemoveAddrDestCache(p_cache_addr_ndp->IF_Nbr,
                                  &p_cache_addr_ndp->AddrProtocol[0]);

        p_cache                 = (NET_NDP_NEIGHBOR_CACHE *)p_cache_addr_ndp->ParentPtr;
        p_cache->ReqAttemptsCtr =  0u;
        p_cache->State          =  NET_NDP_CACHE_STATE_NONE;
        p_cache->Flags          =  NET_CACHE_FLAG_NONE;

        NetCache_Remove((NET_CACHE_ADDR *)p_cache_addr_ndp,     /* Clr Addr Cache and free tmr if specified.            */
                                          DEF_YES);

        p_cache->TmrPtr  = DEF_NULL;
        p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP];
    }

                                                                /* Clear Default router list.                           */
    p_router = NetNDP_RouterListHead;
    while (p_router != DEF_NULL) {
        NetNDP_RouterRemove(p_router, DEF_YES);
        p_router = NetNDP_RouterListHead;
    }

    for (i = 0;i < NET_IF_NBR_IF_TOT; i++) {
        NetNDP_DefaultRouterTbl[i] = (NET_NDP_ROUTER *)0;
    }

                                                                /* Clear Prefix list.                                   */
    p_prefix = NetNDP_PrefixListHead;
    while (p_prefix != DEF_NULL) {
        NetNDP_PrefixRemove(p_prefix, DEF_YES);
        p_prefix = NetNDP_PrefixListHead;
    }

                                                                /* Clear Destination Cache.                             */
    p_dest = NetNDP_DestListHead;
    while (p_dest != DEF_NULL) {
        NetNDP_DestCacheRemove(p_dest);
        p_dest = NetNDP_DestListHead;
    }


exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

exit_fault:
    return;
}


/*
*********************************************************************************************************
*                                         NetNDP_CacheGetState()
*
* Description : Retrieve the cache state of the NDP neighbor cache entry related to the interface and
*               address received.
*
* Argument(s) : if_nbr      Interface number
*
*               p_addr      Pointer to IPv6 address of the neighbor
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                        Neighbor cache was found.
*                               NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND    Neighbor cache was not found.
*
* Return(s)   : Cache state of the Neighbor cache entry.
*
* Caller(s)   : IxANVL testing tools.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Function for testing purpose.
*********************************************************************************************************
*/

CPU_INT08U  NetNDP_CacheGetState (       NET_IF_NBR      if_nbr,
                                  const  NET_IPv6_ADDR  *p_addr,
                                         NET_ERR        *p_err)
{
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    CPU_INT08U               state;


    state = NET_NDP_CACHE_STATE_NONE;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_addr == DEF_NULL) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        goto exit_fault;
    }
#endif

    Net_GlobalLockAcquire((void *)&NetNDP_CacheGetState, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_fault;
    }

    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }




    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(              NET_CACHE_TYPE_NDP,
                                                                             if_nbr,
                                                               (CPU_INT08U *)p_addr,
                                                                             NET_IPv6_ADDR_SIZE,
                                                                             p_err);
    if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
       *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND;
        goto exit_release;
    }

    p_cache = (NET_NDP_NEIGHBOR_CACHE *) p_cache_addr_ndp->ParentPtr;
    state   = p_cache->State;

   *p_err   = NET_NDP_ERR_NONE;


exit_release:
                                                               /* ----------------- RELEASE NET LOCK ----------------- */
   Net_GlobalLockRelease();


exit_fault:
   return (state);
}


/*
*********************************************************************************************************
*                                  NetNDP_CacheGetIsRouterFlagState()
*
* Description : Retrieve the IsRouter flag state for the given neighbor cache related to the interface
*               and address received.
*
* Argument(s) : if_nbr      Interface number
*
*               p_addr      Pointer to IPv6 address of the neighbor
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                        Neighbor cache was found
*                               NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND    Neighbor cache was not found.
*
* Return(s)   : IsRouter flag state : DEF_YES, neighbor is also a router
*                                     DEF_NO , neighbor has not advertise itself as a router
*
* Caller(s)   : IxANVL testing tools.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Function for testing purpose.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetNDP_CacheGetIsRouterFlagState (       NET_IF_NBR      if_nbr,
                                               const  NET_IPv6_ADDR  *p_addr,
                                                      NET_ERR        *p_err)
{
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    CPU_BOOLEAN              is_router;


    is_router = DEF_NO;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_addr == DEF_NULL) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        goto exit_fault;
    }
#endif

    Net_GlobalLockAcquire((void *)&NetNDP_CacheGetState, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_fault;
    }

    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }



    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(              NET_CACHE_TYPE_NDP,
                                                                             if_nbr,
                                                               (CPU_INT08U *)p_addr,
                                                                             NET_IPv6_ADDR_SIZE,
                                                                             p_err);
    if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
       *p_err     = NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND;
        goto exit_release;
    }

    p_cache   = (NET_NDP_NEIGHBOR_CACHE *) p_cache_addr_ndp->ParentPtr;
    is_router = DEF_BIT_IS_SET(p_cache->Flags, NET_NDP_CACHE_FLAG_ISROUTER);


   *p_err     = NET_NDP_ERR_NONE;


exit_release:
                                                              /* ----------------- RELEASE NET LOCK ----------------- */
  Net_GlobalLockRelease();


exit_fault:
    return (is_router);
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
*                                   NetNDP_RxRouterAdvertisement()
*
* Description : Receive Router Advertisement message.
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMP packet.
*               ----        Argument checked   in NetICMPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetICMPv6_Rx().
*
*               p_ndp_hdr   Pointer to received packet's NDP header.
*               ---------   Argument validated in NetICMPv6_Rx()/NetICMPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                Router Adv. successfully processed.
*                               NET_NDP_ERR_OPT_LEN             Option with Length zero is present.
*                               NET_NDP_ERR_HW_ADDR_LEN         HW address length is invalid.
*                               NET_NDP_ERR_HW_ADDR_THIS_HOST   Same HW addr received as host.
*                               NET_NDP_ERR_OPT_TYPE            Invalid option type.
*
*                                                               --- RETURNED BY NetNDP_RouterCfg() : ---
*                               NET_NDP_ERR_ROUTER_NONE_AVAIL   NO available router to allocate.
*                               NET_ERR_FAULT_NULL_FNCT           Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL          NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE        Network timer is NOT a valid timer type.
*
*                                                               ------ RETURNED BY NetTmr_Set() : ------
*                               NET_ERR_FAULT_NULL_PTR            Argument 'ptmr' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT           Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_INVALID_TYPE        Invalid timer type.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_Rx().
*
* Note(s)     : (1) RFC #4861, Section 6.1.2 states that "A node MUST silently discard any received
*                   Router Advertisement messages that does not satisfy all of the following validity
*                   checks":
*
*                   (a) IP Source Address is a link-local address.
*
*                   (b) The IP Hop Limit field has a value of 255.
*
*                   (c) ICMP checksum     is valid.
*
*                   (d) ICMP code         is zero.
*
*                   (e) ICMP length (derived from the IP length) is 16 or more octets.
*
*                   (f) All included options have a length that is greater than zero.
*
*               (2) RFC #4861 section 6.3.4 Processing Received Router Advertisements :
*                   "On receipt of a valid Router Advertisement, a host extracts the source address of
*                    the packet and does the following:
*
*                   (a) If the address is not already present in the host's Default
*                       Router List, and the advertisement's Router Lifetime is non-
*                       zero, create a new entry in the list, and initialize its
*                       invalidation timer value from the advertisement's Router
*                       Lifetime field.
*
*                   (b) If the address is already present in the host's Default Router
*                       List as a result of a previously received advertisement, reset
*                       its invalidation timer to the Router Lifetime value in the newly
*                       received advertisement.
*
*                   (c) If the address is already present in the host's Default Router
*                       List and the received Router Lifetime value is zero, immediately
*                       time-out the entry."
*
*               (3) (a) "If the advertisement contains a Source Link-Layer Address
*                        option, the link-layer address SHOULD be recorded in the Neighbor
*                        Cache entry for the router (creating an entry if necessary) and the
*                        IsRouter flag in the Neighbor Cache entry MUST be set to TRUE."
*
*                   (b) "If no Source Link-Layer Address is included, but a corresponding Neighbor
*                        Cache entry exists, its IsRouter flag MUST be set to TRUE."
*
*                   (c) "If a Neighbor Cache entry is created for the router, its reachability state
*                        MUST be set to STALE as specified in Section 7.3.3."
*
*                   (d)"If a cache entry already exists and is updated with a different link-layer address,
*                       the reachability state MUST also be set to STALE."
*
*               (4) "If the MTU option is present, hosts SHOULD copy the option's value into LinkMTU so long
*                    as the value is greater than or equal to the minimum link MTU [IPv6] and does not
*                    exceed the maximum LinkMTU value specified in the link-type-specific document."
*
*               (5) See NetNDP_RxPrefixUpdate() function description for details on handling Rx Prefix
*                   Information.
*
*               (6) Additional processing of Rx NDP Router Advertisement may be needed to implement in the future
*                   #### NET-793
*********************************************************************************************************
*/

static  void  NetNDP_RxRouterAdvertisement (const  NET_BUF                 *p_buf,
                                                   NET_BUF_HDR             *p_buf_hdr,
                                            const  NET_NDP_ROUTER_ADV_HDR  *p_ndp_hdr,
                                                   NET_ERR                 *p_err)
{
    NET_IF_NBR                    if_nbr;
    NET_NDP_OPT_TYPE              opt_type;
    NET_NDP_OPT_LEN               opt_len;
    CPU_INT16U                    opt_len_tot;
    CPU_INT16U                    opt_len_cnt;
    CPU_INT08U                   *p_ndp_opt;
    NET_NDP_ROUTER               *p_router;
    NET_CACHE_ADDR_NDP           *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE       *p_cache_ndp;
    NET_NDP_OPT_HDR              *p_ndp_opt_hdr;
    NET_NDP_OPT_HW_ADDR_HDR      *p_ndp_opt_hw_addr_hdr;
    NET_NDP_OPT_MTU_HDR          *p_ndp_opt_mtu_hdr;
    NET_NDP_OPT_PREFIX_INFO_HDR  *p_ndp_opt_prefix_info_hdr;
    NET_IPv6_ADDR                *p_addr_prefix;
    NET_IPv6_HDR                 *p_ip_hdr;
    NET_TMR_TICK                  timeout_tick;
    CPU_INT32U                    router_mtu;
    CPU_INT16U                    router_lifetime_sec;
    CPU_INT32U                    retx_timeout;
    CPU_INT32U                    lifetime_valid;
    CPU_INT32U                    lifetime_preferred;
    CPU_INT08U                    prefix_len;
    CPU_INT08U                    hop_limit_ip;
    CPU_INT08U                    hw_addr[NET_IP_HW_ADDR_LEN];
    CPU_INT08U                    hw_addr_len;
    CPU_BOOLEAN                   addr_link_local;
    CPU_BOOLEAN                   addr_cfg_auto;
    CPU_BOOLEAN                   on_link;
    CPU_BOOLEAN                   hw_addr_this_host;
    CPU_BOOLEAN                   opt_type_valid;
    CPU_BOOLEAN                   opt_type_src_addr;
    CPU_BOOLEAN                   addr_unspecified;
    CPU_BOOLEAN                   prefix_Mcast;
    CPU_BOOLEAN                   prefix_link_local;
    CPU_BOOLEAN                   addr_cfg_other;
    CPU_BOOLEAN                   addr_cfg_managed;
    CPU_SR_ALLOC();


    addr_link_local = NetIPv6_IsAddrLinkLocal(&p_buf_hdr->IPv6_AddrSrc);
    if (addr_link_local == DEF_NO) {
       *p_err = NET_NDP_ERR_ADDR_SRC;                           /* See Note #1a.                                        */
        return;
    }

    p_ip_hdr      = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    hop_limit_ip = p_ip_hdr->HopLim;
    if (hop_limit_ip != NET_IPv6_HDR_HOP_LIM_MAX) {
       *p_err = NET_NDP_ERR_HOP_LIMIT;                          /* See Note #1b.                                        */
        return;
    }

    addr_unspecified = NetIPv6_IsAddrUnspecified(&p_buf_hdr->IPv6_AddrSrc);
    if (addr_unspecified == DEF_TRUE) {
       *p_err = NET_NDP_ERR_ADDR_SRC;
        return;
    }

                                                                /* DHCPv6 Flags                                         */
    addr_cfg_managed = DEF_BIT_IS_SET(p_ndp_hdr->Flags, NET_NDP_HDR_FLAG_ADDR_CFG_MNGD);
    addr_cfg_other   = DEF_BIT_IS_SET(p_ndp_hdr->Flags, NET_NDP_HDR_FLAG_ADDR_CFG_OTHER);

    (void)&addr_cfg_managed;
    (void)&addr_cfg_other;

    if_nbr              = p_buf_hdr->IF_Nbr;

    NET_UTIL_VAL_COPY_GET_NET_16(&router_lifetime_sec, &p_ndp_hdr->RouterLifetime);

    timeout_tick        = (NET_TMR_TICK)router_lifetime_sec * NET_TMR_TIME_TICK_PER_SEC;

                                                                /* ---- UPDATE ROUTER ENTRY IN DEFAULT ROUTER LIST ---- */
                                                                /* Search in Router List for Address.                   */
    p_router = NetNDP_RouterSrch(if_nbr,
                                &p_buf_hdr->IPv6_AddrSrc,
                                 p_err);

    if ((p_router            == (NET_NDP_ROUTER *)0)  &&        /* Router address is not in Default Router List.        */
        (router_lifetime_sec != 0)) {                           /* See Note #2a.                                        */

        p_router = NetNDP_RouterCfg(if_nbr,
                                   &p_buf_hdr->IPv6_AddrSrc,
                                    DEF_YES,
                                   &NetNDP_RouterTimeout,
                                    timeout_tick,
                                    p_err);
        if (*p_err != NET_NDP_ERR_NONE) {
             return;
        }

        p_router->LifetimeSec = router_lifetime_sec;

    } else if ((p_router            != (NET_NDP_ROUTER *)0)  && /* Router addr is already in list and...                */
               (router_lifetime_sec != 0)) {                    /* ... router lifetime is non-zero.                     */
                                                                /* See Note #2b.                                        */
        if (p_router->TmrPtr != (NET_TMR *)0) {
                                                                /* Update Router Lifetime tmr.                          */
            NetTmr_Set(p_router->TmrPtr,
                      &NetNDP_RouterTimeout,
                       timeout_tick,
                       p_err);

            p_router->LifetimeSec = router_lifetime_sec;
        }

    } else if ((p_router            != (NET_NDP_ROUTER *)0)  && /* Router addr is already in list and...                */
               (router_lifetime_sec == 0)) {                    /* ... router lifetime = 0.                             */
                                                                /* See Note #2c.                                        */

        NetNDP_RemoveAddrDestCache(p_router->IF_Nbr,
                                  &p_router->Addr.Addr[0]);
        NetNDP_RouterRemove(p_router, DEF_YES);                 /* Remove router from router list.                      */

    } else {                                                    /* Router addr is not in router list and ...            */
                                                                /* ... lifetime = 0.                                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.RxInvalidRouterAdvCtr);
       *p_err = NET_NDP_ERR_ROUTER_NOT_FOUND;
        return;
    }

                                                                /* ----------- UPDATE NDP RE-TRANSMIT TIMER ----------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&retx_timeout, &p_ndp_hdr->ReTxTmr);
    retx_timeout = retx_timeout/NET_NDP_MS_NBR_PER_SEC;
    (void)NetNDP_CfgReachabilityTimeout(             NET_NDP_TIMEOUT_SOLICIT,
                                        (CPU_INT16U) retx_timeout);

                                                                /* ------- SEARCH NEIGHBOR CACHE FOR ROUTER ADDR ------ */
    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(               NET_CACHE_TYPE_NDP,
                                                                              if_nbr,
                                                               (CPU_INT08U *)&p_buf_hdr->IPv6_AddrSrc,
                                                                              NET_IPv6_ADDR_SIZE,
                                                                              p_err);


                                                                /*------------- SCAN RA FOR VALID OPTIONS ------------ */
    opt_len_tot       =  p_buf_hdr->IP_TotLen - sizeof(NET_NDP_ROUTER_ADV_HDR) + sizeof(CPU_INT32U);
    p_ndp_opt         = (CPU_INT08U *)&p_ndp_hdr->Opt;

    opt_len_cnt       =  0u;
    opt_type_valid    =  DEF_NO;
    opt_type_src_addr =  DEF_NO;

    while(( p_ndp_opt    != (CPU_INT08U *)0)       &&
          (*p_ndp_opt    != NET_NDP_OPT_TYPE_NONE) &&
          ( opt_len_cnt  <  opt_len_tot))  {

        p_ndp_opt_hdr = (NET_NDP_OPT_HDR *)p_ndp_opt;
        opt_type     = p_ndp_opt_hdr->Type;
        opt_len      = p_ndp_opt_hdr->Len;

        if (opt_len == 0u) {
           *p_err = NET_NDP_ERR_OPT_LEN;                         /* See Note #1f.                                        */
            return;
        }

        switch (opt_type) {
           case NET_NDP_OPT_TYPE_ADDR_SRC:
                opt_type_valid        = DEF_YES;
                opt_type_src_addr     = DEF_YES;
                p_ndp_opt_hw_addr_hdr = (NET_NDP_OPT_HW_ADDR_HDR *) p_ndp_opt;
                hw_addr_len           = (opt_len * DEF_OCTET_NBR_BITS) - (NET_NDP_OPT_DATA_OFFSET);
                if (hw_addr_len != NET_IF_HW_ADDR_LEN_MAX) {
                   *p_err = NET_NDP_ERR_HW_ADDR_LEN;
                    return;
                }

                hw_addr_len = sizeof(hw_addr);
                NetIF_AddrHW_GetHandler(if_nbr, hw_addr, &hw_addr_len, p_err);
                if (*p_err != NET_IF_ERR_NONE) {
                     return;
                }

                hw_addr_this_host = Mem_Cmp((void     *)&hw_addr[0],
                                            (void     *)&p_ndp_opt_hw_addr_hdr->Addr[0],
                                            (CPU_SIZE_T) hw_addr_len);
                if (hw_addr_this_host == DEF_TRUE) {
                   *p_err = NET_NDP_ERR_HW_ADDR_THIS_HOST;
                    return;
                }

                                                                /* Neighbor cache entry exists for the router.          */
                                                                /* See Note #3d.                                        */
                if (p_cache_addr_ndp != (NET_CACHE_ADDR_NDP *)0) {

                    NetNDP_NeighborCacheUpdateEntry(p_cache_addr_ndp,
                                                   &p_ndp_opt_hw_addr_hdr->Addr[0]);

                    p_cache_ndp = (NET_NDP_NEIGHBOR_CACHE *) p_cache_addr_ndp->ParentPtr;
                    DEF_BIT_SET(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);

                    if (p_router != (NET_NDP_ROUTER *)0) {
                        p_router->NDP_CachePtr = p_cache_ndp;
                    }
                                                                /* No neighbor cache entry exits for the router.        */
                } else {                                        /* See Note #3a and #3c.                                */
                    CPU_CRITICAL_ENTER();
                    timeout_tick = NetNDP_NeighborCacheTimeout_tick;
                    CPU_CRITICAL_EXIT();

                    p_cache_addr_ndp = NetNDP_NeighborCacheAddEntry(               if_nbr,
                                                                    (CPU_INT08U *)&p_ndp_opt_hw_addr_hdr->Addr[0],
                                                                    (CPU_INT08U *)&p_buf_hdr->IPv6_AddrSrc,
                                                                    (CPU_INT08U *) 0,
                                                                                   timeout_tick,
                                                                                  &NetNDP_CacheTimeout,
                                                                                   NET_NDP_CACHE_STATE_STALE,
                                                                                   DEF_YES,
                                                                                   p_err);

                    p_cache_ndp = (NET_NDP_NEIGHBOR_CACHE *)p_cache_addr_ndp->ParentPtr;

                    if (p_router != (NET_NDP_ROUTER *)0) {
                        p_router->NDP_CachePtr = p_cache_ndp;
                    }

                }
                break;

                                                                /* See Note #4.                                        */
           case NET_NDP_OPT_TYPE_MTU:
                opt_type_valid = DEF_YES;
                p_ndp_opt_mtu_hdr = (NET_NDP_OPT_MTU_HDR *) p_ndp_opt;
                NET_UTIL_VAL_COPY_GET_NET_32(&router_mtu, &p_ndp_opt_mtu_hdr->MTU);

                if ((router_mtu >= NET_IPv6_MAX_DATAGRAM_SIZE_DFLT) &&
                    (router_mtu <= NET_IF_MTU_ETHER)) {
                     NetIF_MTU_SetHandler(if_nbr, router_mtu, p_err);
                }
                break;


           case NET_NDP_OPT_TYPE_PREFIX_INFO:
                opt_type_valid = DEF_YES;

                if (opt_len != CPU_WORD_SIZE_32) {              /* Prefix Information option must be 32 bytes.          */
                   *p_err = NET_NDP_ERR_OPT_TYPE;
                    break;
                }

                p_ndp_opt_prefix_info_hdr = (NET_NDP_OPT_PREFIX_INFO_HDR *)p_ndp_opt;

                p_addr_prefix             = &p_ndp_opt_prefix_info_hdr->Prefix;
                prefix_len                =  p_ndp_opt_prefix_info_hdr->PrefixLen;

                if (prefix_len > NET_NDP_PREFIX_LEN_MAX) {      /* Nbr of valid bits of the prefix cannot exceed 128.   */
                   *p_err = NET_NDP_ERR_OPT_TYPE;
                    break;
                }

                prefix_Mcast      = NetIPv6_IsAddrMcast(p_addr_prefix);
                prefix_link_local = NetIPv6_IsAddrLinkLocal(p_addr_prefix);

                if ((prefix_Mcast      == DEF_YES) ||           /* Prefix must not have a link-local scope and ...      */
                    (prefix_link_local == DEF_YES)) {           /* ... must not be a multicast addr prefix.             */
                   *p_err = NET_NDP_ERR_OPT_TYPE;
                    break;
                }

                on_link       = DEF_BIT_IS_SET(p_ndp_opt_prefix_info_hdr->Flags, NET_NDP_HDR_FLAG_ON_LINK);
                addr_cfg_auto = DEF_BIT_IS_SET(p_ndp_opt_prefix_info_hdr->Flags, NET_NDP_HDR_FLAG_ADDR_CFG_AUTO);

                NET_UTIL_VAL_COPY_GET_NET_32(&lifetime_valid,     &p_ndp_opt_prefix_info_hdr->ValidLifetime);
                NET_UTIL_VAL_COPY_GET_NET_32(&lifetime_preferred, &p_ndp_opt_prefix_info_hdr->PreferredLifetime);

                NetNDP_RxPrefixUpdate(if_nbr,
                                      p_addr_prefix,
                                      prefix_len,
                                      on_link,
                                      addr_cfg_auto,
                                      lifetime_valid,
                                      lifetime_preferred,
                                      p_err);
                switch (*p_err) {
                    case NET_NDP_ERR_NONE:
                    case NET_NDP_ERR_ADDR_CFG_IN_PROGRESS:
                    case NET_NDP_ERR_ADDR_CFG_FAILED:
                         break;


                    case NET_NDP_ERR_INVALID_ARG:
                    case NET_ERR_FAULT_NULL_PTR:
                    case NET_NDP_ERR_INVALID_PREFIX:
                    default:
                         return;
                }
                break;


           case NET_NDP_OPT_TYPE_ADDR_TARGET:
           case NET_NDP_OPT_TYPE_REDIRECT:
                break;


           default:
               *p_err = NET_NDP_ERR_OPT_TYPE;
                return;
       }

        p_ndp_opt   += (opt_len * DEF_OCTET_NBR_BITS);
        opt_len_cnt += (opt_len * DEF_OCTET_NBR_BITS);
    }

                                                                /* See Note #3b.                                        */
    if ((opt_type_src_addr == DEF_NO)                &&
        (p_cache_addr_ndp != (NET_CACHE_ADDR_NDP *)0)){
         p_cache_ndp = (NET_NDP_NEIGHBOR_CACHE *)p_cache_addr_ndp->ParentPtr;
         DEF_BIT_SET(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);
    }

    if ((opt_len_tot     > 0)      &&
        (opt_type_valid == DEF_NO)) {
        *p_err = NET_NDP_ERR_OPT_TYPE;
         return;
    }

    NetNDP_UpdateDefaultRouter(if_nbr, p_err);
    switch (*p_err) {
        case NET_NDP_ERR_ROUTER_DFLT_FIND:
        case NET_NDP_ERR_ROUTER_DFLT_NONE:
             break;


        case NET_NDP_ERR_INVALID_ARG:
        default:
             return;
    }

   *p_err = NET_NDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   NetNDP_RxNeighborSolicitation()
*
* Description : Receive Neighbor Solicitation message.
*
* Argument(s) : p_buf       Pointer to network buffer that received NDP packet.
*               ----        Argument checked   in NetICMPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetICMPv6_Rx().
*
*               p_ndp_hdr   Pointer to received packet's NDP header.
*               ---------   Argument validated in NetICMPv6_Rx(),
*                                                 NetICMPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                NS message successfully processed.
*                               NET_NDP_ERR_HOP_LIMIT           Invalid Hop limit number.
*                               NET_NDP_ERR_ADDR_TARGET         Invalid target address.
*                               NET_NDP_ERR_ADDR_DEST           Invalid Destination address.
*                               NET_NDP_ERR_ADDR_SRC            Invalid Source address.
*                               NET_NDP_ERR_OPT_LEN             Invalid NDP option length.
*                               NET_NDP_ERR_OPT_TYPE            Invalid NDP option type.
*                               NET_NDP_ERR_HW_ADDR_THIS_HOST   Same HW addr received as host.
*                               NET_NDP_ERR_FAULT               NDP operation faulted.
*                               NET_NDP_ERR_ADDR_DUPLICATE      IPv6 address is detected has duplicated.
*
*                                                               ----------- RETURNED BY NetIF_Get() : ------------
*                               NET_IF_ERR_INVALID_IF           Invalid interface.
*
*                                                               -- RETURNED BY NetNDP_NeighborCacheAddEntry() : --
*                               NET_ERR_FAULT_NULL_PTR            No Neighbor cache is associated with the NDP cache.
*                               NET_CACHE_ERR_NONE_AVAIL        NO available caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE      Cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT           Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL          NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE        Network timer is NOT a valid timer type.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_Rx().
*
* Note(s)     : (1) RFC #4861, Section 7.1.1 states that "A node MUST silently discard any received
*                   Neighbor Solicitation messages that does not satisfy all of the following validity
*                   checks":
*
*                   (a) The IP Hop Limit field has a value of 255.
*
*                   (b) ICMP checksum     is valid.
*
*                   (c) ICMP code         is zero.
*
*                   (d) ICMP length (derived from the IP length) is 24 or more octets.
*
*                   (e) Target Address is not a multicast address.
*
*                   (f) All included options have a length that is greater than zero.
*
*                   (g) If the IP source address is the unspecified address, the IP destination
*                       address is a solicited-node multicast address,
*
*                   (h) If the IP source address is the unspecified address, there is no source
*                       link-layer address option in the message.
*
*               (2) RFC #4861, Section 7.2.3 details the receipt of a Neighbor Solicitation Message :
*
*                   (a) "A valid Neighbor Solicitation that does not meet any of the following
*                        requirements MUST be silently discarded:"
*
*                       (1) "The Target Address is a "valid" unicast or anycast address assigned to
*                            the receiving interface,"
*
*                       (2) "The Target Address is a unicast or anycast address for which the node is
*                            offering proxy service, or"
*
*                       (3) "The Target Address is a "tentative" address on which Duplicate Address
*                            Detection is being performed."
*
*                   (b) "If the Target Address is tentative, the Neighbor Solicitation should be
*                        processed as described in RFC #4862 section 5.4 :"
*
*                       (1) "If the target address is tentative, and the source address is a unicast
*                            address, the solicitation's sender is performing address resolution on the
*                            target; the solicitation should be silently ignored."
*
*                       (2) "Otherwise, processing takes place as described below. In all cases, a node
*                            MUST NOT respond to a Neighbor Solicitation for a tentative address."
*
*                           (A) "If the source address of the Neighbor Solicitation is the unspecified
*                                address, the solicitation is from a node performing Duplicate Address
*                                Detection.
*
*                               (1) "If the solicitation is from another node, the tentative address is
*                                    a duplicate and should not be used (by either node)."
*
*                               (2) "If the solicitation is from the node itself (because the node loops
*                                    back multicast packets), the solicitation does not indicate the
*                                    presence of a duplicate address."
*
*                   (c) "If the Source Address is not the unspecified address and, on link layers that
*                        have addresses, the solicitation includes a Source Link-Layer Address option,
*                        then the recipient SHOULD create or update the Neighbor Cache entry for the IP
*                        Source Address of the solicitation."
*
*                   (d) "If an entry does not already exist, the node SHOULD create a new one and set
*                        its reachability state to STALE."
*
*                   (e) "If an entry already exists, and the cached link-layer address differs from the
*                        one in the received Source Link-Layer option, the cached address should be
*                        replaced by the received address, and the entry's reachability state MUST be
*                        set to STALE."
*
*                   (f) "If a Neighbor Cache entry is created, the IsRouter flag SHOULD be set to FALSE."
*
*                   (g) "If a Neighbor Cache entry already exists, its IsRouter flag MUST NOT be
*                        modified."
*
*                   (h) "If the Source Address is the unspecified address, the node MUST NOT create or
*                        update the Neighbor Cache entry."
*
*                   (i) "After any updates to the Neighbor Cache, the node sends a Neighbor Advertisement
*                        response as described in the next section."
*********************************************************************************************************
*/

static  void  NetNDP_RxNeighborSolicitation (const  NET_BUF                   *p_buf,
                                                    NET_BUF_HDR               *p_buf_hdr,
                                             const  NET_NDP_NEIGHBOR_SOL_HDR  *p_ndp_hdr,
                                                    NET_ERR                   *p_err)
{
    NET_IF_NBR                if_nbr;
    NET_IPv6_ADDRS           *p_ipv6_addrs;
    NET_IPv6_HDR             *p_ip_hdr;
    NET_CACHE_ADDR_NDP       *p_cache_addr_ndp;
    NET_NDP_OPT_HDR          *p_ndp_opt_hdr;
    NET_NDP_OPT_HW_ADDR_HDR  *p_ndp_opt_hw_addr_hdr;
    NET_NDP_OPT_TYPE          opt_type;
    NET_NDP_OPT_LEN           opt_len;
    CPU_INT08U               *p_ndp_opt;
    CPU_INT08U                hop_limit_ip;
    CPU_INT08U                hw_addr[NET_IP_HW_ADDR_LEN];
    CPU_INT08U                hw_addr_len;
    CPU_INT16U                opt_len_tot;
    CPU_INT16U                opt_len_cnt;
    CPU_BOOLEAN               addr_mcast;
    CPU_BOOLEAN               addr_mcast_sol_node;
    CPU_BOOLEAN               addr_unspecified;
    CPU_BOOLEAN               hw_addr_this_host;
    CPU_BOOLEAN               opt_type_src_addr;
    CPU_BOOLEAN               addr_identical;
    NET_TMR_TICK              timeout_tick;
#ifdef NET_DAD_MODULE_EN
    NET_DAD_OBJ              *p_dad_obj;
#endif
    CPU_SR_ALLOC();


    if_nbr        =  p_buf_hdr->IF_Nbr;

                                                                /* Get IF protocol HW addr and addr size.                */
    hw_addr_len = sizeof(hw_addr);
    NetIF_AddrHW_GetHandler(if_nbr, hw_addr, &hw_addr_len, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

    p_ip_hdr      = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    hop_limit_ip  = p_ip_hdr->HopLim;

    if (hop_limit_ip != NET_IPv6_HDR_HOP_LIM_MAX) {
       *p_err = NET_NDP_ERR_HOP_LIMIT;                          /* See Note #1a.                                        */
        return;
    }

    addr_mcast = NetIPv6_IsAddrMcast(&p_ndp_hdr->TargetAddr);
    if (addr_mcast == DEF_YES) {
       *p_err = NET_NDP_ERR_ADDR_TARGET;                        /* See Note #1e.                                        */
        return;
    }

    addr_unspecified = NetIPv6_IsAddrUnspecified(&p_buf_hdr->IPv6_AddrSrc);
    if (addr_unspecified == DEF_YES) {
        addr_mcast_sol_node = NetIPv6_IsAddrMcastSolNode(&p_buf_hdr->IPv6_AddrDest,
                                                         &p_ndp_hdr->TargetAddr );
        if (addr_mcast_sol_node == DEF_NO) {
            *p_err = NET_NDP_ERR_ADDR_DEST;                     /* See Note #1g.                                        */
             return;
        }
    }

                                                                /* Verify if target address is in IF address list.      */
    p_ipv6_addrs = NetIPv6_GetAddrsHostOnIF(p_buf_hdr->IF_Nbr,
                                           &p_ndp_hdr->TargetAddr);
    if (p_ipv6_addrs != DEF_NULL) {
        if (p_ipv6_addrs->AddrState == NET_IPv6_ADDR_STATE_TENTATIVE) {
            addr_identical = NetIPv6_IsAddrsIdentical(&p_buf_hdr->IPv6_AddrDest, &p_ndp_hdr->TargetAddr);
            if (addr_identical == DEF_YES) {                    /* Discard packet if src addr is same as target addr.   */
               *p_err = NET_NDP_ERR_ADDR_DEST;
                return;
            }

            if (addr_unspecified == DEF_NO) {
               *p_err = NET_NDP_ERR_ADDR_TARGET;                 /* See Note #2b1.                                       */
                return;
            } else {
#ifdef  NET_DAD_MODULE_EN
                hw_addr_this_host = Mem_Cmp((void     *)&hw_addr[0],
                                            (void     *) p_buf_hdr->IF_HW_AddrSrcPtr,
                                            (CPU_SIZE_T) p_buf_hdr->IF_HW_AddrLen);
                if (hw_addr_this_host != DEF_YES) {
                    p_dad_obj = NetDAD_ObjSrch(&p_ipv6_addrs->AddrHost, p_err);
                    if (*p_err != NET_DAD_ERR_NONE) {
                        *p_err = NET_NDP_ERR_FAULT;
                         return;
                    }

                    NetDAD_Signal(NET_DAD_SIGNAL_TYPE_ERR, p_dad_obj, p_err);
                    if (*p_err != NET_DAD_ERR_NONE) {
                        *p_err = NET_NDP_ERR_FAULT;
                         return;
                    }

                   *p_err = NET_NDP_ERR_ADDR_DUPLICATE;
                    return;
                } else {
                   *p_err = NET_NDP_ERR_HW_ADDR_THIS_HOST;
                    return;
                }
#else
               *p_err = NET_NDP_ERR_ADDR_TENTATIVE;
                return;
#endif
            }
        }
    }

    opt_len_tot           = p_buf_hdr->IP_TotLen - sizeof(NET_NDP_NEIGHBOR_SOL_HDR) + CPU_WORD_SIZE_32;

    p_ndp_opt             = (CPU_INT08U *)             &p_ndp_hdr->Opt;
    p_ndp_opt_hdr         = (NET_NDP_OPT_HDR *)         p_ndp_opt;
    p_ndp_opt_hw_addr_hdr = (NET_NDP_OPT_HW_ADDR_HDR *) p_ndp_opt;

    if ((addr_unspecified == DEF_YES) &&                        /* Case when the src addr is unspecified and there...   */
        (opt_len_tot      == 0)) {                              /* ..is no opt field.                                   */

        NET_UTIL_IPv6_ADDR_SET_MCAST_ALL_NODES(p_buf_hdr->IPv6_AddrDest);
        p_ndp_opt_hdr->Type = NET_NDP_OPT_TYPE_ADDR_TARGET;     /* Set NDP option type for NA to Tx.                    */

        NetICMPv6_TxMsgReply((NET_BUF        *)p_buf,
                                               p_buf_hdr,
                             (NET_ICMPv6_HDR *)p_ndp_hdr,
                                               p_err);
        switch (*p_err) {
            case NET_ICMPv6_ERR_NONE:
                 break;


            case NET_ERR_TX:
            case NET_ERR_IF_LOOPBACK_DIS:
            case NET_ERR_IF_LINK_DOWN:
            default:
                *p_err = NET_NDP_ERR_FAULT;
                 return;
        }

    } else if ((addr_unspecified == DEF_NO )  &&                /* Case when one or more option fields are present.     */
               (opt_len_tot      != 0)) {

        opt_type_src_addr = DEF_NO;
        opt_len_cnt       = 0u;

        while ( (opt_len_cnt  <  opt_len_tot)     &&
                (p_ndp_opt    != (CPU_INT08U *)0)  &&
               (*p_ndp_opt    != NET_NDP_OPT_TYPE_NONE)) {

            p_ndp_opt_hdr         = (NET_NDP_OPT_HDR *)         p_ndp_opt;
            p_ndp_opt_hw_addr_hdr = (NET_NDP_OPT_HW_ADDR_HDR *) p_ndp_opt;
            opt_type              = p_ndp_opt_hdr->Type;
            opt_len               = p_ndp_opt_hdr->Len;

            if (opt_len == 0u) {
               *p_err = NET_NDP_ERR_OPT_LEN;                    /* See Note #1f.                                        */
                return;
            }

            if (opt_type == NET_NDP_OPT_TYPE_ADDR_SRC) {        /* Only Source Link-Layer Addr Option type is valid.    */

                opt_type_src_addr = DEF_YES;

                hw_addr_len = (opt_len * DEF_OCTET_NBR_BITS) - (NET_NDP_OPT_DATA_OFFSET);
                if (hw_addr_len != NET_IF_HW_ADDR_LEN_MAX) {
                   *p_err = NET_NDP_ERR_HW_ADDR_LEN;
                    return;
                }

                hw_addr_this_host = Mem_Cmp((void     *)&hw_addr[0],
                                            (void     *)&p_ndp_opt_hw_addr_hdr->Addr,
                                            (CPU_SIZE_T) hw_addr_len);
                if (hw_addr_this_host == DEF_TRUE) {
                    *p_err = NET_NDP_ERR_HW_ADDR_THIS_HOST;
                     return;
                }
                                                                /* Search in cache for Address.                         */
                p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(               NET_CACHE_TYPE_NDP,
                                                                                          if_nbr,
                                                                           (CPU_INT08U *)&p_buf_hdr->IPv6_AddrSrc,
                                                                                          NET_IPv6_ADDR_SIZE,
                                                                                          p_err);
                if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {  /* See Note #2d. If NDP cache not found, ...        */

                    CPU_CRITICAL_ENTER();
                    timeout_tick = NetNDP_NeighborCacheTimeout_tick;
                    CPU_CRITICAL_EXIT();

                    NetNDP_NeighborCacheAddEntry(               if_nbr,
                                                 (CPU_INT08U *)&p_ndp_opt_hw_addr_hdr->Addr[0],
                                                 (CPU_INT08U *)&p_buf_hdr->IPv6_AddrSrc,
                                                 (CPU_INT08U *) 0,
                                                                timeout_tick,
                                                                NetNDP_CacheTimeout,
                                                                NET_NDP_CACHE_STATE_STALE,
                                                                DEF_NO,
                                                                p_err);
                    if (*p_err != NET_NDP_ERR_NONE) {
                         return;
                    }

                } else {                                        /* See Note #2e. If NDP cache found, ...                */

                    NetNDP_NeighborCacheUpdateEntry(p_cache_addr_ndp,
                                                   &p_ndp_opt_hw_addr_hdr->Addr[0]);
                }

                p_ndp_opt_hdr->Type = NET_NDP_OPT_TYPE_ADDR_TARGET;
                                                                /* See Note #2i.                                        */
                NetICMPv6_TxMsgReply((NET_BUF        *)p_buf,
                                                       p_buf_hdr,
                                     (NET_ICMPv6_HDR *)p_ndp_hdr,
                                                       p_err);
                switch (*p_err) {
                    case NET_ICMPv6_ERR_NONE:
                         break;


                    case NET_ERR_TX:
                    case NET_ERR_IF_LOOPBACK_DIS:
                    case NET_ERR_IF_LINK_DOWN:
                    default:
                        *p_err = NET_NDP_ERR_FAULT;
                         return;
                }
            }

            p_ndp_opt    += (opt_len * DEF_OCTET_NBR_BITS);
            opt_len_cnt  += (opt_len * DEF_OCTET_NBR_BITS);
        }

        if (opt_type_src_addr == DEF_NO) {                      /* Case when no Source Link-Layer Address option ...    */
           *p_err = NET_NDP_ERR_OPT_TYPE;                       /* ... is included in received NS.                      */
            return;
        }

    } else if ((addr_unspecified == DEF_NO) &&                  /* RFC#4861 s7.2.4 p.64                                 */
               (opt_len_tot      == 0)) {                       /* Case when no option is included in the NS.           */

        p_ndp_opt_hdr->Type = NET_NDP_OPT_TYPE_ADDR_TARGET;

        NetICMPv6_TxMsgReply((NET_BUF        *)p_buf,
                                               p_buf_hdr,
                             (NET_ICMPv6_HDR *)p_ndp_hdr,
                                               p_err);
        switch (*p_err) {
            case NET_ICMPv6_ERR_NONE:
                 break;


            case NET_ERR_TX:
            case NET_ERR_IF_LOOPBACK_DIS:
            case NET_ERR_IF_LINK_DOWN:
            default:
                *p_err = NET_NDP_ERR_FAULT;
                 return;
        }

    } else {
       *p_err = NET_NDP_ERR_ADDR_SRC;
        return;
    }

   *p_err = NET_NDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                  NetNDP_RxNeighborAdvertisement()
*
* Description : Receive Neighbor Advertisement message.
*
* Argument(s) : p_buf       Pointer to network buffer that received NDP packet.
*               ----        Argument checked   in NetICMPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetICMPv6_Rx().
*
*               p_ndp_hdr   Pointer to received packet's NDP header.
*               ---------   Argument validated in NetICMPv6_Rx()/NetICMPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                      NA successfully processed
*
*                               NET_NDP_ERR_HOP_LIMIT                 Invalid Hop-limit in Neighbor Adv.
*                               NET_NDP_ERR_ADDR_SRC                  Invalid source addr in Neighbor Adv.
*                               NET_NDP_ERR_ADDR_DEST                 Invalid destination addr in Neighbor Adv.
*                               NET_NDP_ERR_OPT_LEN                   Invalid option length in Neighbor Adv.
*                               NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND  No Neighbor cache find for Neighbor Adv addr.
*                               NET_NDP_ERR_HW_ADDR_LEN               Invalid HW addr len in Neighbor Adv.
*                               NET_NDP_ERR_OPT_TYPE                  Invalid option type in Rx Neighbor Adv.
*                               NET_NDP_ERR_ADDR_DUPLICATE            IPv6 address detected has duplicated.
*                               NET_NDP_ERR_FAULT                     NDP operation faulted.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_Rx().
*
* Note(s)     : (1) RFC #4861, Section 7.1.2 states that "A node MUST silently discard any received
*                   Neighbor Advertisement messages that does not satisfy all of the following validity
*                   checks":
*
*                   (a) The IP Hop Limit field has a value of 255.
*
*                   (b) ICMP checksum     is valid.
*
*                   (d) ICMP code         is zero.
*
*                   (e) ICMP length (derived from the IP length) is 24 or more octets.
*
*                   (f) Target address is NOT a multicast address.
*
*                   (g) If the IP destination address is a multicast address, the solicited flag is zero.
*
*                   (h) All included options have a length that is greater than zero.
*
*               (2) RFC #4862, Section 5.4.4 states that "On receipt of a valid Neighbor Advertisement
*                   message on an interface, node behavior depends on whether the target address is
*                   tentative or matches a unicast or anycast address assigned to the interface: "
*
*                   (a) "If the target address is tentative, the tentative address is not unique."
*
*                   (b) "If the target address matches a unicast address assigned to the receiving
*                        interface, it would possibly indicate that the address is a duplicate but it
*                        has not been detected by the Duplicate Address Detection procedure (recall that
*                        Duplicate Address Detection is not completely reliable).  How to handle such a
*                        case is beyond the scope of this document."
*
*                   (c) "Otherwise, the advertisement is processed as described in [RFC4861]."
*
*               (3) RFC #4861, Section 7.2.5 details the receipt of a Neighbor Advertisement Message :
*
*                   (a) "When a valid Neighbor Advertisement is received (either solicited or
*                        unsolicited), the Neighbor Cache is searched for the target's entry. If no
*                        entry exists, the advertisement SHOULD be silently discarded."
*
*                   (b) "If the target's Neighbor Cache entry is in the INCOMPLETE state when the
*                        advertisement is received, one of two things happens :
*
*                       (1) "If the link layer has addresses and no Target Link-Layer Address option is
*                            included, the receiving node SHOULD silently discard the received."
*
*                       (2) "Otherwise, the receiving node performs the following steps:"
*
*                           (A) "It records the link-layer address in the Neighbor Cache entry"
*
*                           (B) "If the advertisement's Solicited flag is set, the state of the entry is
*                                set to REACHABLE; otherwise, it is set to STALE"
*
*                           (C) "It sets the IsRouter flag in the cache entry based on the Router flag
*                                in the received advertisement"
*
*                           (D) "It sends any packets queued for the neighbor awaiting address
*                                resolution."
*
*                   (c) "If the target's Neighbor Cache entry is in any state other than INCOMPLETE when
*                        the advertisement is received, the following actions take place:"
*
*                       (1) "If the Override flag is clear and the supplied link-layer address differs
*                            from that in the cache, then one of two actions takes place:"
*                           (A) "If the state of the entry is REACHABLE, set it to STALE, but do not
*                                update the entry in any other way."
*
*                           (B) "Otherwise, the received advertisement should be ignored and MUST NOT
*                                update the cache"
*
*                       (2) "If the Override flag is set, or the supplied link-layer address is the same
*                            as that in the cache, or no Target Link-Layer Address option was supplied,
*                            the received advertisement MUST update the Neighbor Cache entry as follows:"
*
*                           (A) "The link-layer address in the Target Link-Layer Address option MUST be
*                                inserted in the cache (if one is supplied and differs from the already
*                                recorded address)"
*
*                           (B) "If the Solicited flag is set, the state of the entry MUST be set to
*                                REACHABLE.  If the Solicited flag is zero and the link-layer address
*                                was updated with a different address, the state MUST be set to STALE.
*                                Otherwise, the entry's state remains unchanged."
*
*                           (C) "The IsRouter flag in the cache entry MUST be set based on the Router
*                                flag in the received advertisement.  In those cases where the IsRouter
*                                flag changes from TRUE to FALSE as a result of this update, the node
*                                MUST remove that router from the Default Router List and update the
*                                Destination Cache entries for all destinations using that neighbor as a
*                                router as specified in Section 7.3.3.  This is needed to detect when a
*                                node that is used as a router stops forwarding packets due to being
*                                configured as a host."
*
*                   (d) "If none of the above apply, the advertisement prompts future Neighbor
*                        Unreachability Detection (if it is not already in progress) by changing the
*                        state in the cache entry."
*********************************************************************************************************
*/

static  void  NetNDP_RxNeighborAdvertisement (const  NET_BUF                   *p_buf,
                                                     NET_BUF_HDR               *p_buf_hdr,
                                              const  NET_NDP_NEIGHBOR_ADV_HDR  *p_ndp_hdr,
                                                     NET_ERR                   *p_err)
{
    NET_CACHE_ADDR_NDP       *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE   *p_cache_ndp;
    NET_NDP_ROUTER           *p_router;
    NET_NDP_OPT_HDR          *p_ndp_opt_hdr;
    NET_NDP_OPT_HW_ADDR_HDR  *p_ndp_opt_hw_addr_hdr = DEF_NULL;
    NET_IPv6_HDR             *p_ip_hdr;
    NET_IPv6_ADDRS           *p_ipv6_addrs;
    NET_BUF                  *p_buf_head;
    CPU_INT08U               *p_ndp_opt;
    NET_NDP_OPT_TYPE          opt_type;
    NET_NDP_OPT_LEN           opt_len;
    NET_TMR_TICK              timeout_tick;
    NET_IF_NBR                if_nbr;
    CPU_FNCT_PTR              tmr_fnct;
    CPU_INT08U                hw_addr_len;
    CPU_INT08U                cache_state;
    CPU_INT08U                hop_limit_ip;
    CPU_INT16U                opt_len_tot;
    CPU_INT16U                opt_len_cnt;
    CPU_INT32U                flags;
    CPU_BOOLEAN               is_solicited;
    CPU_BOOLEAN               is_override;
    CPU_BOOLEAN               is_router;
    CPU_BOOLEAN               addr_mcast;
    CPU_BOOLEAN               same_hw_addr;
    CPU_BOOLEAN               opt_type_target_addr;
    CPU_BOOLEAN               addr_identical;
#ifdef NET_DAD_MODULE_EN
    NET_DAD_OBJ              *p_dad_obj;
#endif
    CPU_SR_ALLOC();


    if_nbr = p_buf_hdr->IF_Nbr;

    NET_UTIL_VAL_COPY_GET_NET_32(&flags, &p_ndp_hdr->Flags);

    is_override  = DEF_BIT_IS_SET(flags, NET_NDP_HDR_FLAG_OVRD);
    is_solicited = DEF_BIT_IS_SET(flags, NET_NDP_HDR_FLAG_SOL);
    is_router    = DEF_BIT_IS_SET(flags, NET_NDP_HDR_FLAG_ROUTER);

                                                                /* ----------------- RX NA VALIDATION ----------------- */
    p_ip_hdr      = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    hop_limit_ip = p_ip_hdr->HopLim;
    if (hop_limit_ip != NET_IPv6_HDR_HOP_LIM_MAX) {             /* See Note #1a.                                        */
       *p_err = NET_NDP_ERR_HOP_LIMIT;
        return;
    }

    addr_mcast = NetIPv6_IsAddrMcast(&p_ndp_hdr->TargetAddr);
    if (addr_mcast == DEF_YES) {                                /* See Note #1f.                                        */
       *p_err = NET_NDP_ERR_ADDR_TARGET;
        return;
    }

    addr_mcast = NetIPv6_IsAddrMcast(&p_buf_hdr->IPv6_AddrDest);
    if (addr_mcast == DEF_YES) {                                /* See Note #1g.                                        */
        if (is_solicited == DEF_YES) {
          *p_err = NET_NDP_ERR_ADDR_DEST;
           return;
        }
    }

                                                                /* --------- SEARCH IN IF IPv6 CFGD ADDR LIST -------- */
    p_ipv6_addrs = NetIPv6_GetAddrsHostOnIF(p_buf_hdr->IF_Nbr,
                                           &p_ndp_hdr->TargetAddr);

    if (p_ipv6_addrs != DEF_NULL) {
        switch (p_ipv6_addrs->AddrState) {
            case NET_IPv6_ADDR_STATE_TENTATIVE:
                 addr_identical = NetIPv6_IsAddrsIdentical(&p_buf_hdr->IPv6_AddrDest, &p_ndp_hdr->TargetAddr);
                 if (addr_identical == DEF_YES) {               /* Discard packet if src addr is same as target addr.   */
                    *p_err = NET_NDP_ERR_ADDR_DEST;
                     return;
                 }
#ifdef  NET_DAD_MODULE_EN
                 p_dad_obj = NetDAD_ObjSrch(&p_ipv6_addrs->AddrHost, p_err);
                 if (*p_err != NET_DAD_ERR_NONE) {
                     *p_err = NET_NDP_ERR_FAULT;
                      return;
                 }

                 NetDAD_Signal(NET_DAD_SIGNAL_TYPE_ERR, p_dad_obj, p_err);           /* See Note #2a.                   */
                 if (*p_err != NET_DAD_ERR_NONE) {
                     *p_err = NET_NDP_ERR_FAULT;
                      return;
                 }
#endif
                *p_err = NET_NDP_ERR_ADDR_DUPLICATE;
                 return;


            case NET_IPv6_ADDR_STATE_PREFERRED:                 /* See Note #2b.                                        */
            case NET_IPv6_ADDR_STATE_DEPRECATED:
#ifdef  NET_DAD_MODULE_EN
                 NET_CTR_ERR_INC(Net_ErrCtrs.NDP.RxNeighborAdvAddrDuplicateCtr);
#endif
                *p_err = NET_NDP_ERR_ADDR_DUPLICATE;
                 return;


            case NET_IPv6_ADDR_STATE_NONE:
            case NET_IPv6_ADDR_STATE_DUPLICATED:
            default:
                 break;
        }
    }

                                                                /* --- SEARCH IN NEIGHBOR CACHE FOR IPv6 TARGET ADDR -- */
    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(               NET_CACHE_TYPE_NDP,
                                                                              if_nbr,
                                                               (CPU_INT08U *)&p_ndp_hdr->TargetAddr,
                                                                              NET_IPv6_ADDR_SIZE,
                                                                              p_err);

    if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {          /* See Note #3a.                                        */
       *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND;
        return;
    }

    p_cache_ndp = (NET_NDP_NEIGHBOR_CACHE *) p_cache_addr_ndp->ParentPtr;
    cache_state = p_cache_ndp->State;

                                                                /*------------- SCAN NA FOR VALID OPTIONS ------------ */
    opt_len_tot          =  p_buf_hdr->IP_TotLen - sizeof(NET_NDP_NEIGHBOR_ADV_HDR) + CPU_WORD_SIZE_32;
    p_ndp_opt            = (CPU_INT08U *)&p_ndp_hdr->Opt;
    opt_type_target_addr =  DEF_NO;
    opt_len_cnt          =  0u;

    while(( p_ndp_opt    != (CPU_INT08U *)0)       &&
          ( opt_len_cnt  <  opt_len_tot))  {

        p_ndp_opt_hdr = (NET_NDP_OPT_HDR *)p_ndp_opt;
        opt_type      = p_ndp_opt_hdr->Type;
        opt_len       = p_ndp_opt_hdr->Len;

        if (opt_len == 0u) {
           *p_err = NET_NDP_ERR_OPT_LEN;                        /* See Note #1f.                                        */
            return;
        }

        switch (opt_type) {
            case NET_NDP_OPT_TYPE_ADDR_TARGET :
                 opt_type_target_addr  = DEF_YES;
                 p_ndp_opt_hw_addr_hdr = (NET_NDP_OPT_HW_ADDR_HDR *) p_ndp_opt;

                 hw_addr_len = (opt_len * DEF_OCTET_NBR_BITS) - (NET_NDP_OPT_DATA_OFFSET);

                 if (hw_addr_len != NET_IF_HW_ADDR_LEN_MAX) {
                    *p_err = NET_NDP_ERR_HW_ADDR_LEN;
                     return;
                 }
                 break;


            case NET_NDP_OPT_TYPE_NONE:
            case NET_NDP_OPT_TYPE_ADDR_SRC:
            case NET_NDP_OPT_TYPE_PREFIX_INFO:
            case NET_NDP_OPT_TYPE_REDIRECT:
            case NET_NDP_OPT_TYPE_MTU:
            default:
                 break;

        }

        p_ndp_opt   += (opt_len * DEF_OCTET_NBR_BITS);
        opt_len_cnt += (opt_len * DEF_OCTET_NBR_BITS);
    }

                                                                /*------------------- RX NA HANDLING ------------------ */
    if (cache_state == NET_NDP_CACHE_STATE_INCOMPLETE) {
        if (opt_type_target_addr == DEF_YES) {
            Mem_Copy((void     *)&p_cache_addr_ndp->AddrHW[0],          /* See Note #3b2A.                              */
                     (void     *)&p_ndp_opt_hw_addr_hdr->Addr[0],
                     (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

            p_cache_addr_ndp->AddrHW_Valid = DEF_YES;

                                                                /* See Note #3b2B.                                      */
            if (is_solicited == DEF_YES) {
                CPU_CRITICAL_ENTER();
                timeout_tick = NetNDP_ReachableTimeout_tick;
                CPU_CRITICAL_EXIT();
                tmr_fnct = NetNDP_ReachableTimeout;
                p_cache_ndp->State = NET_NDP_CACHE_STATE_REACHABLE;

            } else {
                CPU_CRITICAL_ENTER();
                timeout_tick = NetNDP_NeighborCacheTimeout_tick;
                CPU_CRITICAL_EXIT();
                tmr_fnct = NetNDP_CacheTimeout;
                p_cache_ndp->State = NET_NDP_CACHE_STATE_STALE;
            }

                                                                /* Reset cache tmr.                                     */
            NetTmr_Set(p_cache_ndp->TmrPtr,
                       tmr_fnct,
                       timeout_tick,
                       p_err);
            if (*p_err != NET_TMR_ERR_NONE) {
                *p_err = NET_NDP_ERR_FAULT;
                 return;
            }
                                                                /* See Note #3b2C.                                      */
            if(is_router == DEF_YES) {
                DEF_BIT_SET(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);
            }

                                                                /* Re-initialize nbr of Solicitations sent.             */
            p_cache_ndp->ReqAttemptsCtr = 0;

                                                                /* See Note #3b2D.                                      */
            p_buf_head                  =  p_cache_addr_ndp->TxQ_Head;
            p_cache_addr_ndp->TxQ_Head  = (NET_BUF *)0;
            p_cache_addr_ndp->TxQ_Tail  = (NET_BUF *)0;
            p_cache_addr_ndp->TxQ_Nbr   = 0;

            NetCache_TxPktHandler(NET_PROTOCOL_TYPE_NDP,
                                  p_buf_head,
                                 &p_ndp_opt_hw_addr_hdr->Addr[0]);
        } else {                                                /* See Note #3b1.                                       */
            *p_err = NET_NDP_ERR_OPT_TYPE;
             return;
        }

    } else {                                                    /* Neighbor cache state other than INCOMPLETE.          */
        if (p_ndp_opt_hw_addr_hdr != DEF_NULL) {
            same_hw_addr =  Mem_Cmp((void     *)&p_cache_addr_ndp->AddrHW,
                                    (void     *)&p_ndp_opt_hw_addr_hdr->Addr,
                                    (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
        } else {
            same_hw_addr = DEF_NO;
        }

        if((is_override  == DEF_NO) &&
           (same_hw_addr == DEF_NO)    ) {                      /* See Note #3c1.                                       */

            if (p_cache_ndp->State == NET_NDP_CACHE_STATE_REACHABLE) {  /* See Note #3c1A & #3c1B.                      */
                p_cache_ndp->State = NET_NDP_CACHE_STATE_STALE;
                CPU_CRITICAL_ENTER();
                timeout_tick = NetNDP_NeighborCacheTimeout_tick;
                CPU_CRITICAL_EXIT();
                NetTmr_Set(p_cache_ndp->TmrPtr,
                           NetNDP_CacheTimeout,
                           timeout_tick,
                           p_err);
                if (*p_err != NET_TMR_ERR_NONE) {
                    *p_err = NET_NDP_ERR_FAULT;
                     return;
                }
            }

        } else if((is_override          == DEF_YES)  ||         /* See Note #3c2.                                       */
                  (same_hw_addr         == DEF_YES)  ||
                  (opt_type_target_addr == DEF_NO)) {

            if ((opt_type_target_addr == DEF_YES) &&            /* See Note #3c2A.                                      */
                (same_hw_addr         == DEF_NO)) {
                Mem_Copy((void     *)&p_cache_addr_ndp->AddrHW,
                         (void     *)&p_ndp_opt_hw_addr_hdr->Addr,
                         (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
            }

            if (is_solicited == DEF_YES) {                      /* See Note #3c2B.                                      */
                p_cache_ndp->State = NET_NDP_CACHE_STATE_REACHABLE;
                CPU_CRITICAL_ENTER();
                timeout_tick = NetNDP_ReachableTimeout_tick;
                CPU_CRITICAL_EXIT();
                NetTmr_Set(p_cache_ndp->TmrPtr,
                           NetNDP_ReachableTimeout,
                           timeout_tick,
                           p_err);
                if (*p_err != NET_TMR_ERR_NONE) {
                    *p_err = NET_NDP_ERR_FAULT;
                     return;
                }

            } else {
                if (same_hw_addr == DEF_NO) {
                    p_cache_ndp->State = NET_NDP_CACHE_STATE_STALE;
                    CPU_CRITICAL_ENTER();
                    timeout_tick = NetNDP_NeighborCacheTimeout_tick;
                    CPU_CRITICAL_EXIT();
                    NetTmr_Set(p_cache_ndp->TmrPtr,
                               NetNDP_CacheTimeout,
                               timeout_tick,
                               p_err);
                    if (*p_err != NET_TMR_ERR_NONE) {
                        *p_err = NET_NDP_ERR_FAULT;
                         return;
                    }
                }
            }
                                                                /* See Note #3c2C.                                      */
            if (is_router == DEF_YES) {
                DEF_BIT_SET(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);

            } else {
                DEF_BIT_CLR(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);

                p_router = NetNDP_RouterSrch(                  if_nbr,
                                             (NET_IPv6_ADDR *)&p_cache_addr_ndp->AddrProtocol[0],
                                                               p_err);
                if (p_router != (NET_NDP_ROUTER *)0) {
                    NetNDP_RemoveAddrDestCache(p_cache_addr_ndp->IF_Nbr, &p_cache_addr_ndp->AddrProtocol[0]);
                    NetNDP_RouterRemove(p_router, DEF_YES);
                    NetNDP_UpdateDefaultRouter(p_cache_addr_ndp->IF_Nbr, p_err);
                    switch (*p_err) {
                        case NET_NDP_ERR_ROUTER_DFLT_FIND:
                        case NET_NDP_ERR_ROUTER_DFLT_NONE:
                             break;


                        case NET_NDP_ERR_INVALID_ARG:
                        default:
                             return;
                    }
                }
            }
        } else {
                                                                /* Empty Else Statement                                 */
        }
    }

   *p_err = NET_NDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetNDP_RxRedirect()
*
* Description : Receive Redirect message.
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMP packet.
*               ----        Argument checked   in NetICMPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetICMPv6_Rx().
*
*               p_ndp_hdr   Pointer to received packet's NDP header.
*               ---------   Argument validated in NetICMPv6_Rx()/NetICMPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                Redirect message successfully processed.
*                               NET_NDP_ERR_HOP_LIMIT           Invalid Hop Limit
*                               NET_NDP_ERR_ADDR_SRC            Invalid source address
*                               NET_NDP_ERR_ADDR_DEST           Invalid destination address
*                               NET_NDP_ERR_ADDR_TARGET         Invalid target address
*                               NET_NDP_ERR_OPT_LEN             Invalid option length
*                               NET_NDP_ERR_HW_ADDR_LEN         Invalid HW address length
*                               NET_NDP_ERR_OPT_TYPE            Invalid option type
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_Rx().
*
* Note(s)     : (1) RFC #4861, Section 8.1 states that "A host MUST silently discard any received
*                   Redirect messages that does not satisfy all of the following validity
*                   checks":
*
*                   (a) IP Source Address is a link-local address.
*
*                   (b) The IP Hop Limit field has a value of 255.
*
*                   (c) ICMP checksum     is valid.
*
*                   (d) ICMP code         is zero.
*
*                   (e) ICMP length (derived from the IP length) is 40 or more octets.
*
*                   (f) The IP source address of the Redirect is the same as the current first-hop
*                       router for the specified ICMP Destination Address.
*
*                   (g) The ICMP Destination Address field in the redirect message does not contain
*                       a multicast address.
*
*                   (h) The ICMP Target Address is either a link-local address (when redirected to a
*                       router) or the same as the ICMP Destination Address (when redirected to the
*                       on-link-destination.
*
*                   (i) "All included options have a length that is greater than zero."
*
*               (2) If address target is the same as the destination address, the destination is on-link.
*                   If not, the target address contain the better first-hop router.
*
*               (3) The following information is not used by the current implementation of NDP:
*
*                   (a) Redirected header option. It should contain as much as possible of the IP
*                       packet that triggered the sending of the Redirect without making the redirect
*                       packet exceed the minimum MTU.
*********************************************************************************************************
*/

static  void  NetNDP_RxRedirect (const  NET_BUF               *p_buf,
                                        NET_BUF_HDR           *p_buf_hdr,
                                 const  NET_NDP_REDIRECT_HDR  *p_ndp_hdr,
                                        NET_ERR               *p_err)
{
    NET_CACHE_ADDR_NDP        *p_cache_addr_target;
    NET_NDP_NEIGHBOR_CACHE    *p_cache_target;
    NET_NDP_DEST_CACHE        *p_dest_cache;
    NET_NDP_OPT_TYPE           opt_type;
    NET_NDP_OPT_LEN            opt_len;
    CPU_INT16U                 opt_len_tot;
    CPU_INT16U                 opt_len_cnt;
    CPU_INT08U                *p_ndp_opt;
    NET_NDP_OPT_HDR           *p_ndp_opt_hdr;
    NET_NDP_OPT_HW_ADDR_HDR   *p_ndp_opt_hw_addr_hdr;
    NET_NDP_OPT_REDIRECT_HDR  *p_ndp_opt_redirect_hdr;
    CPU_INT08U                 hw_addr_len;
    CPU_INT08U                 hop_limit_ip;
    NET_IPv6_HDR              *p_ip_hdr;
    NET_IF_NBR                 if_nbr;
    NET_TMR_TICK               timeout_tick;
    CPU_BOOLEAN                addr_is_link_local;
    CPU_BOOLEAN                addr_is_mcast;
    CPU_BOOLEAN                mem_same;
    CPU_BOOLEAN                is_router;
    CPU_SR_ALLOC();


    if_nbr = p_buf_hdr->IF_Nbr;
                                                                /* -------- VALIDATION OF THE REDIRECT MESSAGE -------- */
    addr_is_link_local = NetIPv6_IsAddrLinkLocal(&p_buf_hdr->IPv6_AddrSrc);
    if (addr_is_link_local == DEF_NO) {
       *p_err = NET_NDP_ERR_ADDR_SRC;                           /* See Note #1a.                                        */
        return;
    }

    p_ip_hdr      = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    hop_limit_ip = p_ip_hdr->HopLim;
    if (hop_limit_ip != NET_IPv6_HDR_HOP_LIM_MAX) {
       *p_err = NET_NDP_ERR_HOP_LIMIT;                          /* See Note #1b.                                        */
        return;
    }

    addr_is_mcast = NetIPv6_IsAddrMcast(&p_ndp_hdr->AddrDest);
    if (addr_is_mcast == DEF_YES) {
       *p_err = NET_NDP_ERR_ADDR_DEST;                          /* See Note #1g.                                        */
        return;
    }

                                                                /* Search in cache for Destination Address.             */
    p_dest_cache = NetNDP_DestCacheSrch(if_nbr, &p_ndp_hdr->AddrDest, p_err);
    if (p_dest_cache == (NET_NDP_DEST_CACHE *)0) {
       *p_err = NET_NDP_ERR_ADDR_DEST;
        return;
    }

    mem_same = Mem_Cmp(&p_buf_hdr->IPv6_AddrSrc.Addr[0],
                       &p_dest_cache->AddrNextHop.Addr[0],
                        NET_IPv6_ADDR_SIZE);
    if (mem_same == DEF_NO) {
       *p_err = NET_NDP_ERR_ADDR_SRC;                           /* See Note #1f.                                        */
        return;
    }

    addr_is_link_local = NetIPv6_IsAddrLinkLocal(&p_ndp_hdr->AddrTarget);

    mem_same = Mem_Cmp(&p_ndp_hdr->AddrTarget,
                       &p_ndp_hdr->AddrDest,
                        NET_IPv6_ADDR_SIZE);

    is_router = !mem_same;


    if ((addr_is_link_local != DEF_YES) &&
        (mem_same           != DEF_YES)) {
        *p_err = NET_NDP_ERR_ADDR_TARGET;                       /* See Note #1h.                                        */
         return;
    }

                                                                /* ----------- VALIDATE ADN PROCESS OPTIONS ----------- */
    opt_len_tot = p_buf_hdr->IP_TotLen - sizeof(NET_NDP_REDIRECT_HDR) + sizeof(CPU_INT32U);
    p_ndp_opt   = (CPU_INT08U *)&p_ndp_hdr->Opt;

    opt_len_cnt = 0u;
    while(( p_ndp_opt    != (CPU_INT08U *)0)       &&
          (*p_ndp_opt    != NET_NDP_OPT_TYPE_NONE) &&
          ( opt_len_cnt  <  opt_len_tot))  {

        p_ndp_opt_hdr = (NET_NDP_OPT_HDR *)p_ndp_opt;
        opt_type      = p_ndp_opt_hdr->Type;
        opt_len       = p_ndp_opt_hdr->Len;

        if (opt_len == 0u) {
           *p_err = NET_NDP_ERR_OPT_LEN;                         /* See Note #1i.                                       */
            return;
        }

       switch (opt_type) {
           case NET_NDP_OPT_TYPE_ADDR_TARGET:
                p_ndp_opt_hw_addr_hdr = (NET_NDP_OPT_HW_ADDR_HDR *) p_ndp_opt;
                hw_addr_len = (opt_len * DEF_OCTET_NBR_BITS) - (NET_NDP_OPT_DATA_OFFSET);
                if (hw_addr_len != NET_IF_HW_ADDR_LEN_MAX) {
                   *p_err = NET_NDP_ERR_HW_ADDR_LEN;
                    return;
                }

                                                                /* Search in cache for Target Address.                  */
                p_cache_addr_target = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(              NET_CACHE_TYPE_NDP,
                                                                                            if_nbr,
                                                                             (CPU_INT08U *)&p_ndp_hdr->AddrTarget.Addr[0],
                                                                                            NET_IPv6_ADDR_SIZE,
                                                                                            p_err);
                if (p_cache_addr_target == (NET_CACHE_ADDR_NDP *)0) {
                    CPU_CRITICAL_ENTER();
                    timeout_tick = NetNDP_NeighborCacheTimeout_tick;
                    CPU_CRITICAL_EXIT();

                    NetNDP_NeighborCacheAddEntry(               if_nbr,
                                                 (CPU_INT08U *)&p_ndp_opt_hw_addr_hdr->Addr[0],
                                                 (CPU_INT08U *)&p_ndp_hdr->AddrTarget,
                                                 (CPU_INT08U *) 0,
                                                                timeout_tick,
                                                                NetNDP_CacheTimeout,
                                                                NET_NDP_CACHE_STATE_STALE,
                                                                is_router,
                                                                p_err);
                } else {
                    NetNDP_NeighborCacheUpdateEntry(p_cache_addr_target,
                                                   &p_ndp_opt_hw_addr_hdr->Addr[0]);

                    p_cache_target = (NET_NDP_NEIGHBOR_CACHE *)p_cache_addr_target->ParentPtr;
                    if (is_router == DEF_YES) {
                        DEF_BIT_SET(p_cache_target->Flags, NET_NDP_CACHE_FLAG_ISROUTER);
                    }
                }
                break;


           case NET_NDP_OPT_TYPE_REDIRECT:
#if 1                                                          /* Prevent compiler warning.                             */
               (void)&p_ndp_opt_redirect_hdr;                  /* See Note #3a                                          */
#else
                p_ndp_opt_redirect_hdr = (NET_NDP_OPT_REDIRECT_HDR *) p_ndp_opt;
#endif


                break;

           case NET_NDP_OPT_TYPE_ADDR_SRC:
           case NET_NDP_OPT_TYPE_PREFIX_INFO:
           case NET_NDP_OPT_TYPE_MTU:
           default:
               *p_err = NET_NDP_ERR_OPT_TYPE;
                break;
       }

        p_ndp_opt   += (opt_len * DEF_OCTET_NBR_BITS);
        opt_len_cnt += (opt_len * DEF_OCTET_NBR_BITS);
    }

                                                                /* ------------- UPDATE DESTINATION CACHE ------------- */
    p_dest_cache->AddrNextHop = p_ndp_hdr->AddrTarget;

    if (mem_same == DEF_YES) {
        p_dest_cache->OnLink = DEF_YES;
    }

    NetNDP_UpdateDestCache(if_nbr,
                          &p_ndp_hdr->AddrDest.Addr[0],
                          &p_ndp_hdr->AddrTarget.Addr[0]);

   *p_err = NET_NDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetNDP_RxPrefixUpdate()
*
* Description : (1) Add or Update an NDP prefix entry in the prefix list based on the received NDP prefix.
*
*                   (a) Search NDP prefix list
*                   (b) Add or Update NDP prefix
*                   (c) If Autonomous Flag is set, configure new addr with received prefix on Interface
*
*
* Argument(s) : if_nbr          Interface number the packet was received from.
*
*               p_addr_prefix   Pointer to received NDP prefix.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                       NDP Rx prefix successfully processed.
*                               NET_ERR_FAULT_NULL_PTR                   Argument 'p_addr_prefix' passed a NULL pointer.
*                               NET_NDP_ERR_INVALID_PREFIX             Invalid prefix received.
*                               NET_NDP_ERR_ADDR_CFG_FAILED            Auto-configured addr. with prefix failed.
*
*                                                                      -- RETURNED BY NetNDP_PrefixHandler() : --
*                               NET_NDP_ERR_INVALID_PREFIX             Invalid Prefix not added to the list.
*                               NET_NDP_ERR_PREFIX_NONE_AVAIL          NO available prefix to allocate.
*
*                                                                      - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF                  Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement().
*
* Note(s)     : (2) RFC #4861, Section 6.3.4 (Processing Received Router Advertisement) states that "For
*                   each Prefix Information option with the on-link flag set, a host does the following:"
*
*                   (a) "If the prefix is the link-local prefix, silently ignore the Prefix Information option."
*
*                   (b) "If the prefix is not already present in the Prefix List, and the Prefix Information
*                        option's Valid Lifetime field is non-zero, create a new entry for the prefix and
*                        initialize its invalidation timer to the Valid Lifetime value in the Prefix
*                        Information option."
*
*                   (c) "If the prefix is already present in the host's Prefix List as the result of a
*                        previously received advertisement, reset its invalidation timer to the Valid Lifetime
*                        value in the Prefix Information option.  If the new Lifetime value is zero, time-out
*                        the prefix immediately (see Section 6.3.5)."
*
*                   (d) "If the Prefix Information option's Valid Lifetime field is zero, and the prefix is
*                        not present in the host's Prefix List, silently ignore the option."
*
*               (3) RFC #4862, Section 5.5.3 states that "For each Prefix-Information option in the
*                   Router Advertisement:
*
*                   (a) If the Autonomous flag is not set,                            silently ignore the
*                       Prefix Information option.
*
*                   (b) If the prefix is the link-local prefix,                       silently ignore the
*                       Prefix Information option.
*
*                   (c) If the preferred lifetime is greater than the valid lifetime, silently ignore the
*                       Prefix Information option.
*
*                   (d) If the prefix advertised is not equal to the prefix of an address configured
*                       by stateless autoconfiguration already in the list of addresses associated
*                       with the interface (where "equal" means the two prefix lengths are the same
*                       and the first prefix-length bits of the prefixes are identical), and if the
*                       Valid Lifetime is not 0, form an address (and add it to the list) by combining
*                       the advertised prefix with an interface identifier of the link as follows:
*
*                       |            128 - N bits               |       N bits           |
*                       +---------------------------------------+------------------------+
*                       |            link prefix                |  interface identifier  |
*                       +----------------------------------------------------------------+
*********************************************************************************************************
*/

static  void  NetNDP_RxPrefixUpdate (NET_IF_NBR      if_nbr,
                                     NET_IPv6_ADDR  *p_addr_prefix,
                                     CPU_INT08U      prefix_len,
                                     CPU_BOOLEAN     on_link,
                                     CPU_BOOLEAN     addr_cfg_auto,
                                     CPU_INT32U      lifetime_valid,
                                     CPU_INT32U      lifetime_preferred,
                                     NET_ERR        *p_err)
{
    NET_IPv6_ADDRS           *p_addrs;
    NET_IPv6_ADDR             ipv6_id;
    CPU_BOOLEAN               updated;
    CPU_BOOLEAN               dad_en;
    CPU_INT08U                id_len;
    CPU_INT08U                total_addr_len;
    NET_IPv6_ADDR_CFG_TYPE    addr_cfg_type;
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    NET_IPv6_AUTO_CFG_OBJ    *p_auto_obj;
    NET_IPv6_AUTO_CFG_STATE   auto_state;
#endif
    CPU_SR_ALLOC();



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        *p_err = NET_NDP_ERR_INVALID_ARG;
         return;
    }
                                                                /* ---------- VALIDATE NO NULL POINTER AS ARG --------- */
    if (p_addr_prefix == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* --------- ADD/UPDATE PREFIX TO PREFIX LIST --------- */
    NetNDP_RxPrefixHandler(if_nbr,
                           p_addr_prefix,
                           prefix_len,
                           lifetime_valid,
                           on_link,
                           addr_cfg_auto,
                           p_err);

                                                                /* ---------- CREATE & ADD ADDR WITH PREFIX ----------- */
    if ((*p_err         == NET_NDP_ERR_NONE) &&
        ( addr_cfg_auto == DEF_YES)         ) {                 /* When Autonomous Flag is set.                         */

        if (lifetime_valid < lifetime_preferred) {
           *p_err = NET_NDP_ERR_INVALID_PREFIX;
            goto exit;
        }

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN

        p_auto_obj = NetIPv6_GetAddrAutoCfgObj(if_nbr);
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (p_auto_obj == DEF_NULL) {
           *p_err = NET_ERR_FAULT_NULL_PTR;
            goto exit;
        }
#endif
                                                                /* Post signal for Rx RA msg with prefix option.        */
        NetNDP_RouterAdvSignalPost(if_nbr, p_err);
        if (*p_err != NET_NDP_ERR_NONE) {
             goto exit;
        }

                                                                /* ------------------ SET DAD TYPE -------------------- */
        CPU_CRITICAL_ENTER();
        auto_state = p_auto_obj->State;
        CPU_CRITICAL_EXIT();
        if (auto_state == NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL) {
            addr_cfg_type = NET_IPv6_ADDR_CFG_TYPE_AUTO_CFG_NO_BLOCKING;
            dad_en        = p_auto_obj->DAD_En;
        } else {
            addr_cfg_type = NET_IPv6_ADDR_CFG_TYPE_RX_PREFIX_INFO;
            dad_en        = DEF_YES;
        }
#else
        addr_cfg_type = NET_IPv6_ADDR_CFG_TYPE_RX_PREFIX_INFO;
        dad_en        = DEF_YES;
#endif

                                                                /* Update addr(s) with same prefix already cfg on IF.   */
        updated =  NetNDP_RxPrefixAddrsUpdate (if_nbr,
                                               p_addr_prefix,
                                               prefix_len,
                                               lifetime_valid,
                                               lifetime_preferred);

        if ((updated   == DEF_NO) &&
            (lifetime_valid  > 0)) {
             id_len = NetIPv6_CreateIF_ID(if_nbr,               /* Create IF ID from HW mac addr.                       */
                                         &ipv6_id,
                                          NET_IPv6_ADDR_AUTO_CFG_ID_IEEE_48,
                                          p_err);
             switch (*p_err) {
                 case NET_IPv6_ERR_NONE:
                     *p_err = NET_NDP_ERR_NONE;
                      break;


                 case NET_ERR_FAULT_NULL_PTR:
                 case NET_IPv6_ERR_INVALID_ADDR_ID_TYPE:
                 default:
                     *p_err = NET_NDP_ERR_ADDR_CFG_FAILED;
                      goto exit_fail_stop_auto;
             }

             total_addr_len = id_len + prefix_len;
             if (total_addr_len > NET_IPv6_ADDR_PREFIX_LEN_MAX) {
                *p_err = NET_NDP_ERR_ADDR_CFG_FAILED;
                 goto exit_fail_stop_auto;
             }


            (void)NetIPv6_CreateAddrFromID(&ipv6_id,            /* Create IPv6 addr from IF ID.                         */
                                            p_addr_prefix,
                                            NET_IPv6_ADDR_PREFIX_CUSTOM,
                                            prefix_len,
                                            p_err);
            switch (*p_err) {
                case NET_IPv6_ERR_NONE:
                    *p_err = NET_NDP_ERR_NONE;
                     break;


                case NET_ERR_FAULT_NULL_PTR:
                case NET_IPv6_ERR_INVALID_ADDR_PREFIX_TYPE:
                case NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN:
                default:
                    *p_err = NET_NDP_ERR_ADDR_CFG_FAILED;
                     goto exit_fail_stop_auto;
            }

                                                                /* ---------------- ADD ADDRESS TO IF ----------------- */
            p_addrs = NetIPv6_CfgAddrAddHandler(if_nbr,
                                                p_addr_prefix,
                                                prefix_len,
                                                lifetime_valid,
                                                lifetime_preferred,
                                                NET_IPv6_ADDR_CFG_MODE_AUTO,
                                                dad_en,
                                                addr_cfg_type,
                                                p_err);
            switch (*p_err) {
                case NET_IPv6_ERR_NONE:
                case NET_ERR_FAULT_FEATURE_DIS:
                    *p_err = NET_NDP_ERR_NONE;
                     goto exit_succeed_stop_auto;
                     break;


                case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
                    *p_err = NET_NDP_ERR_ADDR_CFG_IN_PROGRESS;
                     goto exit;


                case NET_IPv6_ERR_ADDR_CFG_IN_USE:
                case NET_IPv6_ERR_ADDR_CFG_STATE:
                case NET_IPv6_ERR_ADDR_TBL_FULL:
                case NET_IPv6_ERR_INVALID_ADDR_CFG_MODE:
                default:
                    *p_err = NET_NDP_ERR_ADDR_CFG_FAILED;
                     goto exit_fail_stop_auto;
            }

        } else {
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
            NET_IPv6_IF_CFG   *p_ip_if_cfg;
            NET_IPv6_ADDRS    *p_ip_addrs;
            NET_IP_ADDRS_QTY   addr_ix      = 0;
            CPU_BOOLEAN        prefix_found = DEF_NO;


            if (auto_state == NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL) {
                p_ip_if_cfg  =  NetIPv6_GetIF_CfgObj(if_nbr, p_err);
                p_ip_addrs   = &p_ip_if_cfg->AddrsTbl[addr_ix];

                while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {   /* Srch all cfg'd addrs ...                             */

                    if ((p_ip_addrs->AddrHostPrefixLen == prefix_len)                        &&
                        (p_ip_addrs->AddrCfgState      == NET_IPv6_ADDR_CFG_STATE_AUTO_CFGD)) {

                        prefix_found = NetNDP_IsPrefixCfgdOnAddr(&p_ip_addrs->AddrHost,
                                                                  p_addr_prefix,
                                                                  prefix_len);

                        if (prefix_found == DEF_YES) {
                            p_addrs = p_ip_addrs;
                            break;
                        }
                    }

                    p_ip_addrs++;                               /* ... adv to IF's next addr.                           */
                    addr_ix++;
                }

                if (prefix_found == DEF_YES) {
                    goto exit_succeed_stop_auto;
                } else {
                    goto exit_fail_stop_auto;
                }
            }
#endif
        }
    }


   (void)&p_addrs;

    goto exit;


exit_succeed_stop_auto:
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    if (auto_state == NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL) {
        p_auto_obj->AddrGlobalPtr = &p_addrs->AddrHost;
        NetIPv6_AddrAutoCfgComp(if_nbr, NET_IPv6_AUTO_CFG_STATUS_SUCCEEDED);
    }
#endif
    goto exit;


exit_fail_stop_auto:
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    if (auto_state == NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL) {
        p_auto_obj->AddrGlobalPtr = DEF_NULL;
        NetIPv6_AddrAutoCfgComp(if_nbr, NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL);
    }
#endif


exit:
    return;
}


/*
*********************************************************************************************************
*                                       NetNDP_RxPrefixHandler()
*
* Description : (1) Add or Update an Prefix entry in the Prefix list based on received NDP prefix:
*
*                   (a) Search Prefix List
*                   (b) Update or add Prefix entry
*
*
* Argument(s) : if_nbr          Interface number the packet was received from.
*
*               p_addr_prefix   Pointer to received NDP prefix.
*
*               prefix_len      Length of the received prefix.
*
*               lifetime_valid  Lifetime of the received prefix.
*
*               on_link         Indicate if prefix is advertised as being on same link.
*
*               addr_cfg_auto   Indicate that prefix can be used for Autonomous Address Configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                NDP prefix successfully configured.
*                               NET_NDP_ERR_INVALID_PREFIX      Invalid Prefix not added to the list.
*
*                                                               ----- RETURNED BY NetNDP_PrefixCfg() : -----
*                               NET_NDP_ERR_PREFIX_NONE_AVAIL   NO available prefix to allocate.
*                               NET_ERR_FAULT_NULL_FNCT           Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL          NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE        Network timer is NOT a valid timer type.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RxPrefixUpdate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_RxPrefixHandler (       NET_IF_NBR      if_nbr,
                                      const  NET_IPv6_ADDR  *p_addr_prefix,
                                             CPU_INT08U      prefix_len,
                                             CPU_INT32U      lifetime_valid,
                                             CPU_BOOLEAN     on_link,
                                             CPU_BOOLEAN     addr_cfg_auto,
                                             NET_ERR        *p_err)
{
    NET_NDP_PREFIX      *p_prefix;
    NET_IPv6_ADDR        addr_masked;
    NET_TMR_TICK         timeout_tick;


                                                                /* Insure that the received prefix is consistent ...    */
                                                                /* ... with the prefix length.                          */
    NetIPv6_AddrMaskByPrefixLen(p_addr_prefix, prefix_len, &addr_masked, p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
        *p_err  = NET_NDP_ERR_INVALID_PREFIX;
         return;
    }

                                                                /* Calculate timeout tick.                              */
    timeout_tick = (NET_TMR_TICK)lifetime_valid * NET_TMR_TIME_TICK_PER_SEC;

                                                                /* ---------------- SEARCH PREFIX LIST ---------------- */
    p_prefix = NetNDP_PrefixSrch(if_nbr, &addr_masked, p_err);

    if (p_prefix != (NET_NDP_PREFIX *)0){                       /* Prefix already in prefix list...                     */
        if (on_link == DEF_YES) {
            if (lifetime_valid != 0) {
                                                                /* ... update prefix timeout.                           */
                if (p_prefix->TmrPtr != DEF_NULL) {
                    NetTmr_Set(              p_prefix->TmrPtr,
                               (CPU_FNCT_PTR)NetNDP_PrefixTimeout,
                                             timeout_tick,
                                             p_err);
                } else {
                    NetTmr_Get((CPU_FNCT_PTR)NetNDP_PrefixTimeout,
                               (void       *)p_prefix,
                                             timeout_tick,
                                             p_err);
                    if (*p_err != NET_TMR_ERR_NONE) {
                        *p_err = NET_NDP_ERR_NONE;
                         return;
                    }
                }
            } else {
                NetNDP_RemovePrefixDestCache(if_nbr, &p_prefix->Prefix.Addr[0], p_prefix->PrefixLen);
                NetNDP_PrefixRemove(p_prefix, DEF_YES);
            }
        }

       *p_err = NET_NDP_ERR_NONE;
        return;
    }

                                                            /* Prefix is not in NDP cache ...                       */
    if (lifetime_valid == 0u) {                             /* ... if prefix lifetime = 0, do not add it to cache.  */
       *p_err = NET_NDP_ERR_INVALID_PREFIX;
        return;
    }

    if ((on_link       == DEF_NO) &&                        /* If On-link flag and Autonomous flag are not set, ... */
        (addr_cfg_auto == DEF_NO)) {                        /* ... do not add prefix to the cache.                  */
        *p_err = NET_NDP_ERR_INVALID_PREFIX;
         return;
    }
                                                            /* Add new prefix to the prefix list.                   */
    p_prefix = NetNDP_PrefixCfg(if_nbr,
                               &addr_masked ,
                                prefix_len,
                                DEF_YES,
                                NetNDP_PrefixTimeout,
                                timeout_tick,
                                p_err);

}


/*
*********************************************************************************************************
*                                      NetNDP_PrefixAddrsUpdate()
*
* Description : Update all address on the given interface corresponding to the prefix.
*
* Argument(s) : if_nbr              Interface number to search for on.
*
*               p_addr_prefix       Pointer to prefix add.
*
*               prefix_len          Prefix Length.
*
*               lifetime_valid      Valid Lifetime received with Prefix Information Option.
*
*               lifetime_preferred  Preferred Lifetime received with Prefix Information Option.
*
* Return(s)   : DEF_YES     if one or more address where updated.
*
*               DEF_NO      otherwise.
*
* Caller(s)   : NetNDP_RxPrefixHandler().
*
* Note(s)     : (1) Address valid lifetime should be set to 2 hours according rules state in RFC #4862 Section 5.5.3
*                   Router Advertisement Processing. #### NET-779.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetNDP_RxPrefixAddrsUpdate (NET_IF_NBR      if_nbr,
                                                 NET_IPv6_ADDR  *p_addr_prefix,
                                                 CPU_INT08U      prefix_len,
                                                 CPU_INT32U      lifetime_valid,
                                                 CPU_INT32U      lifetime_preferred)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN        valid;
#endif
    NET_IPv6_IF_CFG   *p_ip_if_cfg;
    NET_IPv6_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    NET_TMR_TICK       timeout_tick;
    NET_TMR_TICK       remain_tick;
    CPU_BOOLEAN        prefix_found;
    CPU_BOOLEAN        addr_updated;
    NET_TMR           *p_tmr_pref;
    NET_TMR           *p_tmr_valid;
    NET_ERR            err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, &err);
    if (valid != DEF_YES) {
        return (DEF_NO);
    }
#endif
                                                                /* ------------- VALIDATE IPv6 PREFIX PTR ------------- */
    if (p_addr_prefix == (NET_IPv6_ADDR *)0) {
        return (DEF_NO);
    }

                                                                /* -------------- SRCH IF FOR IPv6 ADDR --------------- */
    addr_ix      = 0u;
    addr_updated = DEF_NO;
    prefix_found = DEF_NO;

    p_ip_if_cfg  =  NetIPv6_GetIF_CfgObj(if_nbr, &err);
    p_ip_addrs   = &p_ip_if_cfg->AddrsTbl[addr_ix];

    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {               /* Srch all cfg'd addrs ...                             */

        if ((p_ip_addrs->AddrHostPrefixLen == prefix_len)                        &&
            (p_ip_addrs->AddrCfgState      == NET_IPv6_ADDR_CFG_STATE_AUTO_CFGD)) {

            prefix_found = NetNDP_IsPrefixCfgdOnAddr(&p_ip_addrs->AddrHost,
                                                      p_addr_prefix,
                                                      prefix_len);

            if (prefix_found == DEF_YES) {                      /* If prefix found, ...                                 */
                                                                /* ... reset addr valid lifetime.                       */
                addr_updated = DEF_YES;
                                                                /* See Note #1.                                         */
                p_tmr_valid = p_ip_addrs->ValidLifetimeTmrPtr;
                p_tmr_pref  = p_ip_addrs->PrefLifetimeTmrPtr;

                if (p_tmr_valid != DEF_NULL) {

                    remain_tick  = p_tmr_valid->TmrVal;

                    timeout_tick = (NET_TMR_TICK)lifetime_valid * NET_TMR_TIME_TICK_PER_SEC;

                    if((timeout_tick > remain_tick   ) ||
                       (timeout_tick > NET_NDP_LIFETIME_TIMEOUT_TWO_HOURS)) {
                        NetTmr_Set(p_tmr_valid,
                                  &NetIPv6_AddrValidLifetimeTimeout,
                                   timeout_tick,
                                  &err);
                    } else if (remain_tick >= NET_NDP_LIFETIME_TIMEOUT_TWO_HOURS) {
                        NetTmr_Set(p_tmr_valid,
                                  &NetIPv6_AddrValidLifetimeTimeout,
                                   NET_NDP_LIFETIME_TIMEOUT_TWO_HOURS,
                                  &err);
                    } else {
                        p_ip_addrs++;                           /* ... adv to IF's next addr.                           */
                        addr_ix++;
                        continue;
                    }
                } else {

                    p_tmr_valid = NetTmr_Get(        &NetIPv6_AddrValidLifetimeTimeout,
                                             (void *) p_ip_addrs,
                                                      NET_NDP_LIFETIME_TIMEOUT_TWO_HOURS,
                                                     &err);
                }

                timeout_tick = (NET_TMR_TICK)lifetime_preferred * NET_TMR_TIME_TICK_PER_SEC;
                if (p_tmr_pref != DEF_NULL) {
                    NetTmr_Set(p_tmr_pref,
                              &NetIPv6_AddrPrefLifetimeTimeout,
                               timeout_tick,
                              &err);
                } else {
                    p_tmr_pref = NetTmr_Get(        &NetIPv6_AddrPrefLifetimeTimeout,
                                            (void *) p_ip_addrs,
                                                     timeout_tick,
                                                    &err);
                }
            }
        }

        p_ip_addrs++;                                            /* ... adv to IF's next addr.                           */
        addr_ix++;
    }

    return (addr_updated);
}


/*
*********************************************************************************************************
*                                   NetNDP_TxNeighborSolicitation()
*
* Description : Transmit Neighbor Solicitation message.
*
* Argument(s) : if_nbr          Network interface number to transmit Neighbor Solicitation message.
*
*               p_addr_src      Pointer to IPv6 source address (see Note #1).
*
*               p_addr_dest     Pointer to IPv6 destination address.
*
*               ndp_sol_type    Indicate what upper procedure is performing the Tx of Solicitations :
*
*                                   NET_NDP_NEIGHBOR_SOL_TYPE_DAD    Duplication Address Detection (DAD)
*
*                                   NET_NDP_NEIGHBOR_SOL_TYPE_RES    Address Resolution
*
*                                   NET_NDP_NEIGHBOR_SOL_TYPE_NUD    Neighbor Unreachability Detection
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                Tx Solicitations successful.
*
*                               ------------ RETURNED BY NetIF_IsValidCfgdHandler() : -----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                               ----------- RETURNED BY NetICMPv6_TxMsgReqHandler() : -----------
*                               See NetICMPv6_TxMsgReqHandler() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_NeighborCacheAddPend(),
*               NetNDP_SolicitTimeout(),
*               NetNDP_DupAddrDetection().
*
* Note(s)     : (1) If IPv6 source address pointer is NULL, the unspecified address is used.
*********************************************************************************************************
*/

static  void  NetNDP_TxNeighborSolicitation (NET_IF_NBR                  if_nbr,
                                             NET_IPv6_ADDR              *p_addr_src,
                                             NET_IPv6_ADDR              *p_addr_dest,
                                             NET_NDP_NEIGHBOR_SOL_TYPE   ndp_sol_type,
                                             NET_ERR                    *p_err)
{
    NET_IPv6_ADDR             addr_unspecified;
    NET_IPv6_ADDR            *p_ndp_addr_src;
    NET_NDP_OPT_HW_ADDR_HDR   ndp_opt_hw_addr_hdr;
    CPU_INT16U                data_len;
    CPU_INT08U                hw_addr[NET_IP_HW_ADDR_LEN];
    CPU_INT08U                hw_addr_len;
    CPU_BOOLEAN               dest_mcast;
    void                     *p_data;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
          return;
     }
#endif

    p_ndp_addr_src = p_addr_src;
    if (p_ndp_addr_src == (NET_IPv6_ADDR *)0) {                  /* If  src addr ptr is NULL                      ...    */
        NetIPv6_AddrUnspecifiedSet(&addr_unspecified, p_err);    /* ... tx neighbor solicitation with unspecified ...    */
        p_ndp_addr_src = &addr_unspecified;                      /* ... src addr.                                        */
        p_data        = (void *)0;
        data_len      = 0u;
    } else {
        ndp_opt_hw_addr_hdr.Opt.Type = NET_NDP_OPT_TYPE_ADDR_SRC;
        ndp_opt_hw_addr_hdr.Opt.Len  = 1u;

        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(if_nbr, hw_addr, &hw_addr_len, p_err);
        if (*p_err != NET_IF_ERR_NONE) {
             return;
        }

        Mem_Copy((void     *)&ndp_opt_hw_addr_hdr.Addr[0],
                 (void     *)&hw_addr[0],
                 (CPU_SIZE_T) NET_IP_HW_ADDR_LEN);

        p_data   = (void *)&ndp_opt_hw_addr_hdr;
        data_len = sizeof(NET_NDP_OPT_HW_ADDR_HDR);
    }

    if ((ndp_sol_type == NET_NDP_NEIGHBOR_SOL_TYPE_DAD) ||
        (ndp_sol_type == NET_NDP_NEIGHBOR_SOL_TYPE_RES)) {
        dest_mcast = DEF_YES;                                   /* Set destination addr to solicited-node-multicast.    */
    } else {
        dest_mcast = DEF_NO;
    }

                                                                /* -------------------- TX NDP REQ -------------------- */
                                                                /* Tx Neighbor Solicitation msg.                        */
   (void)NetICMPv6_TxMsgReqHandler(         if_nbr,
                                            NET_ICMPv6_MSG_TYPE_NDP_NEIGHBOR_SOL,
                                            NET_ICMPv6_MSG_CODE_NDP_NEIGHBOR_SOL,
                                            0u,
                                            p_ndp_addr_src,
                                            p_addr_dest,
                                            NET_IPv6_HDR_HOP_LIM_MAX,
                                            dest_mcast,
                                            DEF_NULL,
                                   (void  *)p_data,
                                            data_len,
                                            p_err);
   switch (*p_err) {
       case NET_ICMPv6_ERR_NONE:
           *p_err = NET_NDP_ERR_NONE;
            break;


       case NET_INIT_ERR_NOT_COMPLETED:
       case NET_ERR_TX:
       case NET_ERR_IF_LOOPBACK_DIS:
       case NET_ERR_IF_LINK_DOWN:
       case NET_ERR_FAULT_LOCK_ACQUIRE:
       default:
            break;

   }

}


/*
*********************************************************************************************************
*                                     NetNDP_IsPrefixCfgdOnAddr()
*
* Description : Validate if an IPv6 prefix is configured on a specific address.
*
* Argument(s) : p_addr          Pointer to IPv6 address to validate.
*
*               p_addr_prefix   Pointer to IPv6 prefix to search for.
*
*               prefix_len      Prefix length.
*
* Return(s)   : DEF_YES, if the prefix is configured on the specified address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetNDP_PrefixAddrsUpdate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetNDP_IsPrefixCfgdOnAddr (NET_IPv6_ADDR  *p_addr,
                                                NET_IPv6_ADDR  *p_addr_prefix,
                                                CPU_INT08U      prefix_len)
{
    CPU_INT08U   prefix_octets_nbr;
    CPU_INT08U   id_bits_nbr;
    CPU_INT08U   prefix_mask;
    CPU_INT08U   octet_ix;
    CPU_BOOLEAN  prefix_found;


    prefix_octets_nbr = prefix_len / DEF_OCTET_NBR_BITS;        /* Calc nbr of octets that contain 8 prefix bits.       */

                                                                /* Calc nbr of remaining ID bits to not consider.       */
    id_bits_nbr = DEF_OCTET_NBR_BITS - (prefix_len % DEF_OCTET_NBR_BITS);

    prefix_mask = DEF_OCTET_MASK << id_bits_nbr;                /* Set prefix mask.                                     */

    octet_ix = 0u;
    while (octet_ix <  prefix_octets_nbr) {
        if (p_addr_prefix->Addr[octet_ix] != p_addr->Addr[octet_ix]) {
            return (DEF_NO);
        }
        octet_ix++;
    }

    prefix_found = ((p_addr_prefix->Addr[octet_ix] & prefix_mask) ==  \
                    (p_addr->Addr[octet_ix]        & prefix_mask)) ? DEF_YES : DEF_NO;

    return (prefix_found);
}


/*
*********************************************************************************************************
*                                     NetNDP_UpdateDefaultRouter()
*
* Description : (1) Update the Default Router for the given interface:
*
*                   (a) Search NDP Default router list for a entry that is link with a neighbor cache
*                       entry who's state is Reachable are maybe reachable (stale, delay, probe).
*
*
* Argument(s) : if_nbr      Interface number of the router.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_ROUTER_DFLT_FIND    Default router as been find and set.
*                               NET_NDP_ERR_ROUTER_DFLT_NONE    No Default router as been set.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Pointer to the selected default router NDP cache entry.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement(),
*               NetNDP_RxNeighborAdvertisement(),
*               NetNDP_SolicitTimeout(),
*               NetNDP_RouterTimeout().
*
* Note(s)     : (2) RFC 4861 section 6.3.6 (Default Router Selection) specifies :
*                   "Routers that are reachable or probably reachable (i.e., in any state other than
*                    INCOMPLETE) SHOULD be preferred over routers whose reachability is unknown or suspect
*                    (i.e., in the INCOMPLETE state, or for which no Neighbor Cache entry exists)."
*********************************************************************************************************
*/

static  NET_NDP_ROUTER  *NetNDP_UpdateDefaultRouter(NET_IF_NBR   if_nbr,
                                                    NET_ERR     *p_err)
{
    NET_NDP_ROUTER          *p_router;
    NET_NDP_ROUTER          *p_router_temp;
    NET_NDP_NEIGHBOR_CACHE  *p_cache_neighbor;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
         *p_err =  NET_NDP_ERR_INVALID_ARG;
          return ((NET_NDP_ROUTER *)0);
     }
#endif

    p_router_temp = (NET_NDP_ROUTER *)0;

    p_router = NetNDP_RouterListHead;
    while (p_router != (NET_NDP_ROUTER *)0) {

        if (p_router->IF_Nbr == if_nbr) {
            p_cache_neighbor = p_router->NDP_CachePtr;
            if (p_cache_neighbor != (NET_NDP_NEIGHBOR_CACHE *)0) {
                if (p_cache_neighbor->State == NET_NDP_CACHE_STATE_REACHABLE) {
                    NetNDP_DefaultRouterTbl[if_nbr] = p_router;
                   *p_err = NET_NDP_ERR_ROUTER_DFLT_FIND;
                    return (p_router);
                } else if (p_cache_neighbor->State != NET_NDP_CACHE_STATE_INCOMPLETE) {
                    p_router_temp = p_router;
                } else {
                                                                /* Empty Else Statement                                 */
                }
            }
        }
        p_router = p_router->NextPtr;
    }

    if (p_router_temp != (NET_NDP_ROUTER *)0) {
        NetNDP_DefaultRouterTbl[if_nbr] = p_router_temp;
       *p_err = NET_NDP_ERR_ROUTER_DFLT_FIND;
        return (p_router_temp);
    }


    NetNDP_DefaultRouterTbl[if_nbr] = (NET_NDP_ROUTER *)0;

   *p_err = NET_NDP_ERR_ROUTER_DFLT_NONE;

    return ((NET_NDP_ROUTER *)0);

}


/*
*********************************************************************************************************
*                                    NetNDP_UpdateDestCache()
*
* Description : (1) Update entry in Destination Cache with same Next-Hop address as the received one:
*
*                   (a) Search NDP Destination Cache List
*                   (b) Update Next-Hop address in NDP destination cache whit new address.
*
*
* Argument(s) : if_nbr       Interface number of the interface for the given address.
*
*               p_addr       Pointer to the Next-Hop IPv6 address to update.
*
*               p_addr_new   Pointer to the new Next-Hop IPv6 address.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RxRedirect().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_UpdateDestCache(       NET_IF_NBR   if_nbr,
                                     const  CPU_INT08U  *p_addr,
                                     const  CPU_INT08U  *p_addr_new)
{
    NET_NDP_DEST_CACHE  *p_dest_cache;
    CPU_BOOLEAN          mem_same;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_ERR              err;

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, &err);
     if (err != NET_IF_ERR_NONE) {
         return;
     }
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }

    if (p_addr_new == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_dest_cache = NetNDP_DestListHead;
    while (p_dest_cache != (NET_NDP_DEST_CACHE *)0) {

        if (p_dest_cache->IF_Nbr == if_nbr) {
            mem_same = Mem_Cmp(&p_dest_cache->AddrNextHop, p_addr, NET_IPv6_ADDR_SIZE);
            if (mem_same == DEF_YES) {
                Mem_Copy(&p_dest_cache->AddrNextHop, p_addr_new, NET_IPv6_ADDR_SIZE);
            }
        }
        p_dest_cache = p_dest_cache->NextPtr;
    }
}


/*
*********************************************************************************************************
*                                    NetNDP_RemoveAddrDestCache()
*
* Description : (1) Invalidate Entry in Destination Cache with Next-Hop Address corresponding to
*                   given address:
*
*                   (a) Search NDP Destination Cache List
*                   (b) Invalidate Next-Hop address in NDP destination cache when same as given address.
*
*
* Argument(s) : if_nbr      Interface number of the interface for the given address.
*
*               p_prefix    Pointer to IPv6 address.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_CacheTimeout(),
*               NetNDP_CfgAddrPrefix().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_RemoveAddrDestCache(       NET_IF_NBR   if_nbr,
                                         const  CPU_INT08U  *p_addr)
{
    NET_NDP_DEST_CACHE  *p_dest_cache;
    CPU_BOOLEAN          mem_same;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_ERR              err;


                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, &err);
     if (err != NET_IF_ERR_NONE) {
         return;
     }
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_dest_cache = NetNDP_DestListHead;
    while (p_dest_cache != (NET_NDP_DEST_CACHE *)0) {

        if (p_dest_cache->IF_Nbr == if_nbr) {
            mem_same = Mem_Cmp(&p_dest_cache->AddrNextHop, p_addr, NET_IPv6_ADDR_SIZE);
            if (mem_same == DEF_YES) {
                Mem_Clr(&p_dest_cache->AddrNextHop, NET_IPv6_ADDR_SIZE);
                p_dest_cache->IsValid = DEF_NO;
                p_dest_cache->OnLink  = DEF_NO;
            }
        }
        p_dest_cache = p_dest_cache->NextPtr;
    }
}


/*
*********************************************************************************************************
*                                     NETNDP_RemovePrefixDestCache()
*
* Description : (1) Invalidate Entry in Destination Cache with Next-Hop Address with same given prefix:
*
*                   (a) Search NDP Destination Cache List
*                   (b) Invalidate Next-Hop address in NDP cache when prefix match.
*
*
* Argument(s) : if_nbr      Interface number of the interface for the given prefix.
*
*               p_prefix    Pointer to address prefix.
*
*               prefix_len  Length of the prefix.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_CacheTimeout(),
*               NetNDP_CfgAddrPrefix().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_RemovePrefixDestCache (       NET_IF_NBR   if_nbr,
                                            const  CPU_INT08U  *p_prefix,
                                                   CPU_INT08U   prefix_len)
{
    NET_NDP_DEST_CACHE  *p_dest_cache;
    CPU_BOOLEAN          valid;
    NET_ERR              err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, &err);
     if (err != NET_IF_ERR_NONE) {
         return;
     }
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_prefix == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_dest_cache = NetNDP_DestListHead;
    while (p_dest_cache != (NET_NDP_DEST_CACHE *)0) {

        if (p_dest_cache->IF_Nbr == if_nbr) {
            valid = NetIPv6_IsAddrAndPrefixLenValid(                  &p_dest_cache->AddrNextHop,
                                                     (NET_IPv6_ADDR *) p_prefix,
                                                                       prefix_len,
                                                                      &err);
           (void)&err;

            if (valid == DEF_YES) {
                Mem_Clr(&p_dest_cache->AddrNextHop, NET_IPv6_ADDR_SIZE);
                p_dest_cache->IsValid = DEF_NO;
                p_dest_cache->OnLink  = DEF_NO;
            }
        }
        p_dest_cache = p_dest_cache->NextPtr;
    }

}


/*
*********************************************************************************************************
*                                        NetNDP_CacheAddPend()
*
* Description : (1) Add a 'PENDING' NDP cache into the NDP Cache List & transmit an NDP Request :
*
*                   (a) Configure NDP cache :
*                       (1) Get sender protocol sender
*                       (2) Get default-configured NDP cache
*                       (3) NDP cache state
*                       (4) Enqueue transmit buffer to NDP cache queue
*                   (b) Insert   NDP cache into NDP Cache List
*                   (c) Transmit NDP Request to resolve NDP cache
*
*
* Argument(s) : p_buf            Pointer to network buffer to transmit.
*               ----             Argument checked   in NetCache_Handler().
*
*               p_buf_hdr        Pointer to network buffer header.
*               --------         Argument validated in NetCache_Handler().
*
*               p_addr_protocol  Pointer to protocol address (see Note #2).
*               --------------   Argument checked   in NetCache_Handler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_CACHE_ERR_PEND              NDP cache added in 'PENDING' state.
*
*                                                               - RETURNED BY NetNDP_NeighborCacheAddEntry() : -
*                               NET_CACHE_ERR_NONE_AVAIL        NO available NDP caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE      NDP cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT           Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL          NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE        Network timer is NOT a valid timer type.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_CacheHandler().
*
* Note(s)     : (2) 'p_addr_protocol' MUST point to a valid protocol address in network-order.
*
*                    See also 'NetNDP_CacheHandler()  Note #2e3'.
*
*               (3) (a) RFC #1122, Section 2.3.2.2 states that "the link layer SHOULD" :
*
*                       (1) "Save (rather than discard) ... packets destined to the same unresolved
*                            IP address and" ...
*                       (2) "Transmit the saved packet[s] when the address has been resolved."
*
*                   (b) Since NDP Layer is the last layer to handle & queue the transmit network
*                       buffer, it is NOT necessary to increment the network buffer's reference
*                       counter to include the pending NDP cache buffer queue as a new reference
*                       to the network buffer.
*
*               (4) Some buffer controls were previously initialized in NetBuf_Get() when the packet
*                   was received at the network interface layer.  These buffer controls do NOT need
*                   to be re-initialized but are shown for completeness.
*********************************************************************************************************
*/

static  void  NetNDP_NeighborCacheAddPend (const  NET_BUF      *p_buf,
                                                  NET_BUF_HDR  *p_buf_hdr,
                                           const  CPU_INT08U   *p_addr_protocol,
                                                  NET_ERR      *p_err)
{
    NET_CACHE_ADDR_NDP       *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE   *p_cache_ndp;
    CPU_INT08U                addr_protocol_sender[NET_IPv6_ADDR_SIZE];
    NET_IF_NBR                if_nbr;
    NET_TMR_TICK              timeout_tick;
    NET_NDP_OPT_HW_ADDR_HDR   ndp_opt_hw_addr_hdr;
    CPU_INT08U                hw_addr[NET_IP_HW_ADDR_LEN];
    CPU_INT08U                hw_addr_len;
    CPU_SR_ALLOC();


                                                                /* ------------------ CFG NDP CACHE ------------------- */
                                                                /* Copy sender protocol addr to net order.              */
                                                                /* Cfg protocol addr generically from IP addr.          */
    Mem_Copy(&addr_protocol_sender[0], &p_buf_hdr->IPv6_AddrSrc, NET_IPv6_ADDR_SIZE);

    if_nbr = p_buf_hdr->IF_Nbr;

    CPU_CRITICAL_ENTER();
    timeout_tick = NetNDP_SolicitTimeout_tick;
    CPU_CRITICAL_EXIT();

    p_cache_addr_ndp = NetNDP_NeighborCacheAddEntry(if_nbr,
                                                    0,
                                                    p_addr_protocol,
                                                    addr_protocol_sender,
                                                    timeout_tick,
                                                    NetNDP_SolicitTimeout,
                                                    NET_NDP_CACHE_STATE_INCOMPLETE,
                                                    DEF_NO,
                                                    p_err);
    if (*p_err != NET_NDP_ERR_NONE) {
        return;
    }

                                                                /* Cfg buf's unlink fnct/obj to NDP cache.              */
    p_buf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT)NetCache_UnlinkBuf;
    p_buf_hdr->UnlinkObjPtr   = (void       *)p_cache_addr_ndp;

#if 0                                                           /* Init'd in NetBuf_Get() [see Note #4].                */
    p_buf_hdr->PrevSecListPtr = (NET_BUF *)0;
    p_buf_hdr->NextSecListPtr = (NET_BUF *)0;
#endif
                                                                /* Q buf to NDP cache (see Note #3a1).                  */
    p_cache_addr_ndp->TxQ_Head    = (NET_BUF *)p_buf;
    p_cache_addr_ndp->TxQ_Tail    = (NET_BUF *)p_buf;
    p_cache_addr_ndp->TxQ_Nbr++;
                                                                /* -------------------- TX NDP REQ -------------------- */
                                                                /* Tx Neighbor Solicitation msg to resolve NDP cache.   */

    ndp_opt_hw_addr_hdr.Opt.Type = NET_NDP_OPT_TYPE_ADDR_SRC;
    ndp_opt_hw_addr_hdr.Opt.Len  = 1u;

    hw_addr_len = sizeof(hw_addr);
    NetIF_AddrHW_GetHandler(if_nbr, hw_addr, &hw_addr_len, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
          return;
    }

    Mem_Copy((void     *)&ndp_opt_hw_addr_hdr.Addr[0],
             (void     *)&hw_addr[0],
             (CPU_SIZE_T) hw_addr_len);

    NetNDP_TxNeighborSolicitation(                  if_nbr,
                                                   &p_buf_hdr->IPv6_AddrSrc,
                                  (NET_IPv6_ADDR *) p_addr_protocol,
                                                    NET_NDP_NEIGHBOR_SOL_TYPE_RES,
                                                    p_err);

    p_cache_ndp = (NET_NDP_NEIGHBOR_CACHE *)p_cache_addr_ndp->ParentPtr;
    p_cache_ndp->ReqAttemptsCtr++;                              /* Inc req attempts ctr.                                */

   *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_PEND;
}


/*
*********************************************************************************************************
*                                       NetNDP_NeighborCacheAddEntry()
*
* Description : Add new entry to the NDP cache.
*
* Argument(s) : if_nbr              Interface number for this cache entry.
*
*               p_addr_hw           Pointer to hardware address.
*
*               p_addr_ipv6         Pointer to IPv6 address of Neighbor.
*
*               p_addr_ipv6_sender  Pointer to IPv6 address of the sender.
*
*               timeout_tick        Timeout value (in 'NET_TMR_TICK' ticks).
*
*               timeout_fnct        Pointer to timeout function.
*
*               cache_state         Neighbor Cache initial state.
*
*               is_router           Indicate if Neighbor entry is also a router.
*                                       DEF_YES  Neighbor is also a router.
*                                       DEF_NO   Neighbor is not advertising itself as a router.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                     NDP cache entry successfully added to the cache.
*
*                               NET_ERR_FAULT_NULL_PTR                 Argument 'p_addr_hw' / 'p_addr_ipv6' passed
*                                                                        a NULL pointer.
*                                                                    NDP Address Cache find has not Neighbor cache
*                                                                        parent.
*
*                                                                    - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF                Invalid network interface number.
*
*                               NET_NDP_ERR_NEIGHBOR_CACHE_ADD_FAIL  Failed to add new neighbor in neighbor cache.
*
*                                                                    ----- RETURNED BY NetCache_CfgAddrs() : -----
*                               NET_CACHE_ERR_NONE_AVAIL             NO available caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE           Cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT                Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL               NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE             Network timer is NOT a valid timer type.
*                               NET_ERR_FAULT_NULL_PTR               Variable is set to Null pointer.
*
* Return(s)   : Pointer to the NDP cache entry created.
*
* Caller(s)   : NetNDP_CfgAddrPrefix(),
*               NetNDP_DAD_Start(),
*               NetNDP_CacheAddPend(),
*               NetNDP_RxNeighborSolicitation(),
*               NetNDP_RxRouterAdvertisement().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CACHE_ADDR_NDP  *NetNDP_NeighborCacheAddEntry (       NET_IF_NBR     if_nbr,
                                                           const  CPU_INT08U    *p_addr_hw,
                                                           const  CPU_INT08U    *p_addr_ipv6,
                                                           const  CPU_INT08U    *p_addr_ipv6_sender,
                                                                  NET_TMR_TICK   timeout_tick,
                                                                  CPU_FNCT_PTR   timeout_fnct,
                                                                  CPU_INT08U     cache_state,
                                                                  CPU_BOOLEAN    is_router,
                                                                  NET_ERR       *p_err)
{
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE  *p_cache_ndp;
    CPU_BOOLEAN              timer_en;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
          return ((NET_CACHE_ADDR_NDP *)0);
     }
                                                                /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr_ipv6 == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return ((NET_CACHE_ADDR_NDP *)0);
    }
#endif

    if (timeout_tick == 0) {
        timer_en = DEF_NO;
    } else {
        timer_en = DEF_YES;
    }

    p_cache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_CfgAddrs(              NET_CACHE_TYPE_NDP,
                                                                             if_nbr,
                                                               (CPU_INT08U *)p_addr_hw,
                                                                             NET_IF_HW_ADDR_LEN_MAX,
                                                               (CPU_INT08U *)p_addr_ipv6,
                                                               (CPU_INT08U *)p_addr_ipv6_sender,
                                                                             NET_IPv6_ADDR_SIZE,
                                                                             timer_en,
                                                                             timeout_fnct,
                                                                             timeout_tick,
                                                                             p_err);
    if (*p_err != NET_CACHE_ERR_NONE) {
        *p_err = NET_NDP_ERR_NEIGHBOR_CACHE_ADD_FAIL;
         return((NET_CACHE_ADDR_NDP *)0);
    }

                                                                /* Insert entry into NDP cache list.                    */
    NetCache_Insert((NET_CACHE_ADDR *) p_cache_addr_ndp);

                                                                /* Get parent cache.                                    */
    p_cache_ndp = (NET_NDP_NEIGHBOR_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_ndp);
    if (p_cache_ndp == ((NET_NDP_NEIGHBOR_CACHE *)0)) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return((NET_CACHE_ADDR_NDP *)0);
    }

    p_cache_ndp->State = cache_state;

    if (is_router == DEF_TRUE) {
                                                            /* Set isRouter Flag to high.                           */
        DEF_BIT_SET(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);
    } else {
        DEF_BIT_CLR(p_cache_ndp->Flags, NET_NDP_CACHE_FLAG_ISROUTER);
    }

   *p_err = NET_NDP_ERR_NONE;

    return(p_cache_addr_ndp);
}


/*
*********************************************************************************************************
*                                    NetNDP_NeighborCacheUpdateEntry()
*
* Description : Update existing entry in the Neighbor cache.
*
* Argument(s) : p_cacne_addr_ndp    Pointer to entry in Address Cache.
*
*               p_ndp_opt_hw_addr   Pointer to hw address received in NDP message.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement(),
*               NetNDP_RxNeighborSolicitation().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_NeighborCacheUpdateEntry(NET_CACHE_ADDR_NDP  *p_cache_addr_ndp,
                                              CPU_INT08U          *p_ndp_opt_hw_addr)
{
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    CPU_BOOLEAN              same_hw_addr;
    NET_TMR_TICK             timeout_tick;
    NET_ERR                  err;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_cache = (NET_NDP_NEIGHBOR_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_ndp);


    same_hw_addr =  Mem_Cmp((void     *)&p_cache_addr_ndp->AddrHW,
                            (void     *) p_ndp_opt_hw_addr,
                            (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

    if(same_hw_addr == DEF_NO) {                                /* Hw addr in cache isn't the same as the received one. */
        Mem_Copy((void     *)&p_cache_addr_ndp->AddrHW[0],
                 (void     *) p_ndp_opt_hw_addr,
                 (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

        CPU_CRITICAL_ENTER();
        timeout_tick = NetNDP_NeighborCacheTimeout_tick;
        CPU_CRITICAL_EXIT();
                                                                /* Set cache tmr to CacheTimeout.                       */
        NetTmr_Set(p_cache->TmrPtr,
                   NetNDP_CacheTimeout,
                   timeout_tick,
                  &err);
                                                                /* Add state of the entry cache to STALE.               */
        p_cache->State = NET_NDP_CACHE_STATE_STALE;
    } else {                                                    /* Hw addr in cache is the same as the received one.    */
        CPU_CRITICAL_ENTER();
        timeout_tick = NetNDP_ReachableTimeout_tick;
        CPU_CRITICAL_EXIT();
                                                                /* Set cache tmr to ReachableTimeout.                   */
        NetTmr_Set(p_cache->TmrPtr,
                   NetNDP_ReachableTimeout,
                   timeout_tick,
                  &err);
                                                                /* Add state of the entry cache to REACHABLE.           */
        p_cache->State = NET_NDP_CACHE_STATE_REACHABLE;
    }

}


/*
*********************************************************************************************************
*                                       NetNDP_CacheRemoveEntry()
*
* Description : Remove an entry in the NDP Neighbor cache.
*
* Argument(s) : p_cache   Pointer to the NDP Neighbor entry to remove.
*
*               tmr_free  Indicate if the neighbor cache timer must be freed.
*                           DEF_YES         Free timer.
*                           DEF_NO   Do not free timer.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_NeighborCacheRemoveEntry(NET_NDP_NEIGHBOR_CACHE  *p_cache,
                                              CPU_BOOLEAN              tmr_free)
{
    NET_CACHE_ADDR_NDP  *p_cache_addr_ndp;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_cache == (NET_NDP_NEIGHBOR_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_cache_addr_ndp = p_cache->CacheAddrPtr;

    NetNDP_RemoveAddrDestCache(p_cache_addr_ndp->IF_Nbr,
                              &p_cache_addr_ndp->AddrProtocol[0]);

    NetCache_Remove((NET_CACHE_ADDR *)p_cache_addr_ndp,         /* Clr Addr Cache and free tmr if specified.            */
                                      tmr_free);

    p_cache->TmrPtr         = (NET_TMR *)0;
    p_cache->ReqAttemptsCtr =  0u;
    p_cache->State          =  NET_NDP_CACHE_STATE_NONE;
    p_cache->Flags          =  NET_CACHE_FLAG_NONE;

}


/*
*********************************************************************************************************
*                                          NetNDP_RouterDfltGet()
*
* Description : Retrieve the default router for the given Interface or if no default router is defined,
*               get a router in the router list.
*
* Argument(s) : if_nbr      Interface number on which the packet must be send.
*
*               p_router    Pointer to variable that will receive the router object found.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                A  router has been found.
*                               NET_NDP_ERR_ROUTER_NOT_FOUND    No router has been found.
*                               NET_NDP_ERR_FAULT               Unexpected error.
*
* Return(s)   : DEF_YES, if the router found is the router listed as the default one.
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetNDP_NextHop().
*
* Note(s)     : (1) RFC 4861 section 6.3.6 (Default Router Selection) specifies :
*                   "When no routers on the list are known to be reachable or probably reachable, routers
*                    SHOULD be selected in a round-robin fashion, so that subsequent requests for a
*                    default router do not return the same router until all other routers have been
*                    selected."
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetNDP_RouterDfltGet(NET_IF_NBR        if_nbr,
                                          NET_NDP_ROUTER  **p_router,
                                          NET_ERR          *p_err)
{
    NET_NDP_ROUTER  *p_router_ix;
    NET_NDP_ROUTER  *p_router_next;
    NET_NDP_ROUTER  *p_router_cache;
    CPU_BOOLEAN      robin_found;
    CPU_BOOLEAN      router_found_next;


    if (NetNDP_DefaultRouterTbl[if_nbr] != (NET_NDP_ROUTER *)0) {
       *p_router = NetNDP_DefaultRouterTbl[if_nbr];
       *p_err = NET_NDP_ERR_NONE;
        return (DEF_YES);
    }

    robin_found       = DEF_NO;
    router_found_next = DEF_NO;
    p_router_ix       = DEF_NULL;
    p_router_next     = DEF_NULL;
    p_router_cache    = DEF_NULL;

    p_router_ix = NetNDP_RouterListHead;
    while ((p_router_ix != DEF_NULL) &&                         /* Search Default Router List for suitable router.      */
           (robin_found == DEF_NO)) {

        if (p_router_ix->IF_Nbr == if_nbr) {

            p_router_next = p_router_ix->NextPtr;
                                                                /* Search for suitable next router (on same IF).        */
            while ((p_router_next     != DEF_NULL) &&
                   (router_found_next == DEF_NO)  ) {
                if (p_router_next->IF_Nbr == if_nbr) {
                    router_found_next = DEF_YES;
                } else {
                    p_router_next = p_router_next->NextPtr;
                }
            }
                                                                /* Cache first good router in case round-robin has ..   */
                                                                /* ... not been assigned.                               */
            if (p_router_cache == DEF_NULL) {
                p_router_cache = p_router_ix;
            }

                                                                /* Find if router has been assigned with round robin.   */
            if (p_router_ix->RoundRobin == DEF_YES) {
                p_router_ix->RoundRobin = DEF_NO;
                robin_found = DEF_YES;
                if (p_router_next != DEF_NULL) {
                    p_router_next->RoundRobin = DEF_YES;
                    p_router_cache  = p_router_next;
                }
            }
        }

        router_found_next = DEF_NO;
        p_router_ix = p_router_next;
    }

    if (robin_found == DEF_YES) {                               /* A router assigned with round-robin was found.        */
       *p_router = p_router_cache;
       *p_err    = NET_NDP_ERR_NONE;
    } else if (p_router_cache != DEF_NULL) {                    /* No router was assign with the round robin...         */
        p_router_cache->RoundRobin = DEF_YES;                   /* ... assign first find in list.                       */
       *p_router = NetNDP_RouterListHead;
       *p_err    = NET_NDP_ERR_NONE;
    } else {                                                    /* No router was found.                                 */
       *p_router = DEF_NULL;
       *p_err    = NET_NDP_ERR_ROUTER_NOT_FOUND;
    }
    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                         NetNDP_RouterCfg()
*
* Description : (1) Get and Configure a Router from the router pool.
*
*                   (a) Get a router from the router pool and insert it in router list.
*                   (b) Configure router with received arguments.
*
*
* Argument(s) : if_nbr          Number of the Interface that is on same link than router.
*
*               p_addr          Pointer to router's IPv6 address.
*
*               timer_en        Indicate whether are not to set a network timer for the router:
*
*                                   DEF_YES                Set network timer for router.
*                                   DEF_NO          Do NOT set network timer for router.
*
*               timeout_fnct    Pointer to timeout function.
*
*               timeout_tick    Timeout value (in 'NET_TMR_TICK' ticks).
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                    Router successfully configured.
*                               NET_ERR_FAULT_NULL_PTR                Argument 'p_addr' passed a NULL pointer.
*
*                                                                   - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
*                                                                   ----- RETURNED BY NetNDP_RouterGet() : -----
*                               NET_NDP_ERR_ROUTER_NONE_AVAIL       NO available router to allocate.
*
*                                                                   -------- RETURNED BY NetTmr_Get() : --------
*                               NET_ERR_FAULT_NULL_FNCT               Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL              NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE            Network timer is NOT a valid timer type.
*
* Return(s)   : Pointer to configured router entry.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_ROUTER  *NetNDP_RouterCfg(NET_IF_NBR      if_nbr,
                                          NET_IPv6_ADDR  *p_addr,
                                          CPU_BOOLEAN     timer_en,
                                          CPU_FNCT_PTR    timeout_fnct,
                                          NET_TMR_TICK    timeout_tick,
                                          NET_ERR        *p_err)
{
    NET_NDP_ROUTER *p_router;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
          return ((NET_NDP_ROUTER *)0);
     }
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return ((NET_NDP_ROUTER *)0);
    }
#endif

    p_router = NetNDP_RouterGet(p_err);

    if (p_router == (NET_NDP_ROUTER *)0) {
       return ((NET_NDP_ROUTER *)0);                            /* Return err from NetNDP_RouterGet().                  */
    }

    if (timer_en == DEF_YES) {

        p_router->TmrPtr = NetTmr_Get((CPU_FNCT_PTR)timeout_fnct,
                                      (void       *)p_router,
                                      (NET_TMR_TICK)timeout_tick,
                                      (NET_ERR    *)p_err);
        if (*p_err != NET_TMR_ERR_NONE) {                       /* If timer unavailable, ...                            */
                                                                /* ... free Router.                                     */
            NetNDP_RouterRemove((NET_NDP_ROUTER *)p_router, DEF_NO);
            return ((NET_NDP_ROUTER *)0);
        }
    }

                                                                /* ---------------- CFG ROUTER ENTRY ------------------ */
    p_router->IF_Nbr = if_nbr;

    p_router->RoundRobin = DEF_NO;

    Mem_Copy(&p_router->Addr, p_addr, NET_IPv6_ADDR_SIZE);

    p_router->NDP_CachePtr = (NET_NDP_NEIGHBOR_CACHE*)0;

   *p_err = NET_NDP_ERR_NONE;

    return (p_router);
}


/*
*********************************************************************************************************
*                                        NetNDP_RouterSrch()
*
* Description : Search for a matching Router for the given address into the Default Router list.
*
* Argument(s) : if_nbr      Interface number of the router to look for.
*
*               p_addr      Pointer to the ipv6 address to look for in the Default Router list.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Router FOUND in router list.
*                               NET_NDP_ERR_ROUTER_NOT_FOUND    Router NOT found in router list.
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_addr' passed a NULL pointer.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Pointer to router entry found in router list.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement(),
*               NetNDP_RxNeighborAdvertisement(),
*               NetNDP_CacheReqTimeout().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_ROUTER  *NetNDP_RouterSrch(NET_IF_NBR      if_nbr,
                                           NET_IPv6_ADDR  *p_addr,
                                           NET_ERR        *p_err)
{
    NET_NDP_ROUTER    *p_router;
    CPU_INT08U        *p_router_addr;
    CPU_BOOLEAN        found;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
          return ((NET_NDP_ROUTER *)0);
     }
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return ((NET_NDP_ROUTER *)0);
    }
#endif

    p_router = (NET_NDP_ROUTER *)NetNDP_RouterListHead;

    while (p_router != (NET_NDP_ROUTER *)0) {                 /* Srch Default Router List ...                         */
                                                              /* ... until router found.                              */

        if (p_router->IF_Nbr == if_nbr) {

            p_router_addr = (CPU_INT08U   *)&p_router->Addr.Addr[0];

                                                                /* Cmp address with Router address.                     */
            found         =  Mem_Cmp((void     *)p_addr,
                                     (void     *)p_router_addr,
                                     (CPU_SIZE_T)NET_IPv6_ADDR_SIZE);

            if (found == DEF_YES) {                             /* If a router is found, ..                             */
               *p_err = NET_NDP_ERR_NONE;
                return (p_router);                              /* .. return found Router.                              */
            }
        }

        p_router = p_router->NextPtr;;
    }

   *p_err = NET_NDP_ERR_ROUTER_NOT_FOUND;

    return (p_router);
}


/*
*********************************************************************************************************
*                                         NetNDP_RouterGet()
*
* Description : (1) Get a Router entry.
*
*                   (a) Get a router entry from the router pool.
*                   (b) Insert router entry in the Default Router list.
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Router successfully acquired.
*                               NET_NDP_ERR_ROUTER_NONE_AVAIL   NO available router to allocate.
*
* Return(s)   : Pointer to router entry.
*
* Caller(s)   : NetNDP_RouterCfg().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_ROUTER  *NetNDP_RouterGet(NET_ERR  *p_err)
{
    NET_NDP_ROUTER  *p_router;
    NET_ERR          err;


    if (NetNDP_RouterPoolPtr != (NET_NDP_ROUTER *)0) {          /* If Router pool NOT empty, ...             */
                                                                /* ...  get router from pool.                    */
        p_router              = NetNDP_RouterPoolPtr;
        NetNDP_RouterPoolPtr  = (NET_NDP_ROUTER *)p_router->NextPtr;

                                                                /* If Router List NOT empty, ...              */
    } else if (NetNDP_RouterListTail != (NET_NDP_ROUTER *)0) {
                                                                /* ... get Router from list tail.             */
        p_router              = (NET_NDP_ROUTER *)NetNDP_RouterListTail;
        NetNDP_RouterRemove(p_router, DEF_YES);
        p_router              = NetNDP_RouterPoolPtr;
        NetNDP_RouterPoolPtr  = (NET_NDP_ROUTER *)p_router->NextPtr;

    } else {                                                   /* Else none avail, rtn err (see Note #4).      */
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.RouterNoneAvailCtr);
       *p_err =   NET_NDP_ERR_ROUTER_NONE_AVAIL;
        return ((NET_NDP_ROUTER *)0);

    }

                                                                /* -------------------- INIT CACHE -------------------- */
    NetNDP_RouterClr(p_router);

                                                                /* ------------ UPDATE ROUTER POOL STATS -------------- */
    NetStat_PoolEntryUsedInc(&NetNDP_RouterPoolStat, &err);

                                                                /* ---------- INSERT ROUTER INTO ROUTER LIST ---------- */
    p_router->PrevPtr = (NET_NDP_ROUTER *)0;
    p_router->NextPtr = NetNDP_RouterListHead;
                                                                /* If list NOT empty, insert before head.               */
    if (NetNDP_RouterListHead != (NET_NDP_ROUTER *)0) {
        NetNDP_RouterListHead->PrevPtr = p_router;
    } else {                                                    /* Else add first Router to list.                       */
        NetNDP_RouterListTail = p_router;
    }
                                                                /* Insert Router @ list head.                           */
    NetNDP_RouterListHead = p_router;

   *p_err =  NET_NDP_ERR_NONE;

    return (p_router);
}


/*
*********************************************************************************************************
*                                        NetNDP_RouterRemove()
*
* Description : (1) Remove a router from the router list.
*
*                   (a) Unlink router entry from router list.
*                   (b) Free router entry.
*
*
* Argument(s) : p_router    Pointer to the Router entry to insert into the router list.
*
*               tmr_free    Indicate whether to free network timer :
*
*                               DEF_YES                Free network timer for prefix.
*                               DEF_NO          Do NOT free network timer for prefix.
*                                                     [Freed by NetTmr_TaskHandler()].
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RxRouterAdvertisement(),
*               NetNDP_RxNeighborAdvertisement(),
*               NetNDP_RouterCfg(),
*               NetNDP_RouterGet(),
*               NetNDP_CacheReqTimeout(),
*               NetNDP_RouterTimeout().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_RouterRemove(NET_NDP_ROUTER  *p_router,
                                  CPU_BOOLEAN      tmr_free)
{
    NET_NDP_ROUTER  *p_router_next;
    NET_NDP_ROUTER  *p_router_prev;
    NET_ERR          err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_router == (NET_NDP_ROUTER *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_router_prev = p_router->PrevPtr;
    p_router_next = p_router->NextPtr;

                                                                /* ---------- UNLINK ROUTER FROM ROUTER LIST ---------- */
                                                                /* Point previous Router to next Router.                */
    if (p_router_prev != (NET_NDP_ROUTER *)0) {
        p_router_prev->NextPtr = p_router_next;
    } else {
        NetNDP_RouterListHead  = p_router_next;
    }
                                                                /* Point next Router to previous Router.                */
    if (p_router_next != (NET_NDP_ROUTER *)0) {
        p_router_next->PrevPtr = p_router_prev;
    } else {
        NetNDP_RouterListTail  = p_router_prev;
    }

    #if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                 /* Clear Router's pointers (see Note #1).               */
    p_router->PrevPtr = (NET_NDP_ROUTER *)0;
    p_router->NextPtr = (NET_NDP_ROUTER *)0;
    #endif

                                                                /* ----------------- FREE ROUTER TMR ------------------ */
    if (tmr_free == DEF_YES) {
        if (p_router->TmrPtr != (NET_TMR *)0) {
         NetTmr_Free(p_router->TmrPtr);
         p_router->TmrPtr = (NET_TMR *)0;
        }
    }

                                                                /* ---------------- CLEAR ROUTER ENTRY ---------------- */
    p_router->NDP_CachePtr = (NET_NDP_NEIGHBOR_CACHE *)0;

    #if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        NetNDP_RouterClr(p_router);
    #endif

                                                                /* ---------------- FREE ROUTER ENTRY ----------------- */
    p_router->NextPtr    = (NET_NDP_ROUTER *)NetNDP_RouterPoolPtr;
    NetNDP_RouterPoolPtr = (NET_NDP_ROUTER *)p_router;

                                                               /* ------------- UPDATE ROUTER POOL STATS -------------- */
    NetStat_PoolEntryUsedDec(&NetNDP_RouterPoolStat, &err);
}


/*
*********************************************************************************************************
*                                        NetNDP_RouterClr()
*
* Description : Clear a router entry.
*
* Argument(s) : p_router    Pointer to the Router entry to clear.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_RouterGet(),
*               NetNDP_RouterRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_RouterClr(NET_NDP_ROUTER  *p_router)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_router == (NET_NDP_ROUTER *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_router->PrevPtr = (NET_NDP_ROUTER *)0;
    p_router->NextPtr = (NET_NDP_ROUTER *)0;

    p_router->IF_Nbr = NET_IF_NBR_NONE;

    p_router->NDP_CachePtr = (NET_NDP_NEIGHBOR_CACHE *)0;

    Mem_Clr(&p_router->Addr, NET_IPv6_ADDR_SIZE);

    p_router->LifetimeSec = 0u;

    if (p_router->TmrPtr != (NET_TMR *)0) {
        NetTmr_Free(p_router->TmrPtr);
        p_router->TmrPtr = (NET_TMR *)0;
    }

}


/*
*********************************************************************************************************
*                                     NetNDP_PrefixSrchMatchAddr()
*
* Description : Search the prefix list for a match with the given address.
*
* Argument(s) : if_nbr      Interface number of the prefix to look for.
*               ------      Argument checked by caller(s).
*
*               p_addr      Pointer to the ipv6 address to match with a prefix in the Prefix list.
*               ------      Argument checked by caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Prefix FOUND in prefix list.
*                               NET_NDP_ERR_PREFIX_NOT_FOUND    Prefix NOT found in prefix list.
*
* Return(s)   : Pointer to prefix entry found in list.
*
* Caller(s)   : NetNDP_IsAddrOnLink().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_PREFIX  *NetNDP_PrefixSrchMatchAddr (       NET_IF_NBR      if_nbr,
                                                     const  NET_IPv6_ADDR  *p_addr,
                                                            NET_ERR        *p_err)
{
    NET_NDP_PREFIX    *p_prefix;
    NET_NDP_PREFIX    *p_prefix_next;
    CPU_INT08U        *p_prefix_addr;
    NET_IPv6_ADDR      addr_masked;
    CPU_BOOLEAN        found;


    found = DEF_NO;

    p_prefix = (NET_NDP_PREFIX *)NetNDP_PrefixListHead;
    while ((p_prefix != (NET_NDP_PREFIX *)0) &&                 /* Search Prefix List ...                               */
           (found    ==  DEF_NO)) {                             /* ... until Prefix found.                              */

        p_prefix_next = (NET_NDP_PREFIX *) p_prefix->NextPtr;

        if (p_prefix->IF_Nbr == if_nbr) {

            p_prefix_addr = (CPU_INT08U *)&p_prefix->Prefix.Addr[0];

            NetIPv6_AddrMaskByPrefixLen(p_addr, p_prefix->PrefixLen, &addr_masked, p_err);
                                                                /* Compare Prefix with prefix in list.                  */
            found =  Mem_Cmp((void     *)&addr_masked,
                             (void     *) p_prefix_addr,
                             (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);

            if (found == DEF_YES) {                             /* If Prefix found, ..                                  */
               *p_err = NET_NDP_ERR_NONE;
                return (p_prefix);
            }
        }

        p_prefix = (NET_NDP_PREFIX *)p_prefix_next;             /* Advertise to next Router.                            */
    }

   *p_err = NET_NDP_ERR_PREFIX_NOT_FOUND;

    return (p_prefix);
}


/*
*********************************************************************************************************
*                                          NetNDP_PrefixCfg()
*
* Description : (1) Get and Configured a prefix from the Prefix pool.
*
*                   (a) Get a prefix from the prefix pool and insert it in prefix list.
*                   (b) Configure prefix with received arguments.
*
*
* Argument(s) : if_nbr          Interface number of the prefix.
*
*               p_addr_prefix   Pointer to IPv6 prefix.
*
*               prefix_len      Length of the prefix to configure.
*
*               timer_en        Indicate whether are not to set a network timer for the prefix:
*
*                               DEF_YES                Set network timer for prefix.
*                               DEF_NO          Do NOT set network timer for prefix.
*
*               timeout_fnct    Pointer to timeout function.
*
*               timeout_tick    Timeout value (in 'NET_TMR_TICK' ticks).
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Prefix successfully configured.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_addr_prefix' passed
*                                                                   a NULL pointer.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ----- RETURNED BY NetNDP_PrefixGet() : -----
*                               NET_NDP_ERR_PREFIX_NONE_AVAIL   NO available prefix to allocate.
*
*                                                               -------- RETURNED BY NetTmr_Get() : --------
*                               NET_ERR_FAULT_NULL_FNCT         Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL          NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE        Network timer is NOT a valid timer type.
*
* Return(s)   : Pointer to configured prefix entry.
*
* Caller(s)   : NetNDP_RxPrefixHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_PREFIX  *NetNDP_PrefixCfg (       NET_IF_NBR      if_nbr,
                                           const  NET_IPv6_ADDR  *p_addr_prefix,
                                                  CPU_INT08U      prefix_len,
                                                  CPU_BOOLEAN     timer_en,
                                                  CPU_FNCT_PTR    timeout_fnct,
                                                  NET_TMR_TICK    timeout_tick,
                                                  NET_ERR        *p_err)
{
    NET_NDP_PREFIX  *p_prefix;
    NET_NDP_PREFIX  *p_prefix_rtn;


    p_prefix_rtn = DEF_NULL;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr_prefix == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
          goto exit;
     }


#endif

    p_prefix = NetNDP_PrefixGet(p_err);

    if (p_prefix == (NET_NDP_PREFIX *)0) {
        goto exit;                                              /* Return err from NetNDP_PrefixGet().                  */
    }

                                                                /* ----------------- CFG PREFIX TIMER ----------------- */
    if (timer_en == DEF_YES) {

        p_prefix->TmrPtr = NetTmr_Get((CPU_FNCT_PTR)timeout_fnct,
                                      (void       *)p_prefix,
                                      (NET_TMR_TICK)timeout_tick,
                                      (NET_ERR    *)p_err);
        if (*p_err != NET_TMR_ERR_NONE) {                       /* If timer unavailable, ...                            */
                                                                /* ... free Prefix.                                     */
            NetNDP_PrefixRemove(p_prefix, DEF_NO);
            goto exit;
        }
    }


                                                                /* ---------------- CFG PREFIX ENTRY ------------------ */
    p_prefix_rtn        = p_prefix;
    p_prefix->IF_Nbr    = if_nbr;
    p_prefix->PrefixLen = prefix_len;
    Mem_Copy(&p_prefix->Prefix, p_addr_prefix, NET_IPv6_ADDR_SIZE);


   *p_err = NET_NDP_ERR_NONE;

exit:
    return (p_prefix_rtn);
}


/*
*********************************************************************************************************
*                                          NetNDP_PrefixSrch()
*
* Description : Search the prefix list for a match with the given prefix.
*
* Argument(s) : if_nbr          Interface number of the prefix to look for.
*
*               p_addr_prefix   Pointer to the ipv6 prefix to look for in the Prefix list.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Prefix FOUND in prefix list.
*                               NET_NDP_ERR_PREFIX_NOT_FOUND    Prefix NOT found in prefix list.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_addr_prefix' passed
*                                                                   a NULL pointer.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Pointer to prefix entry found in list.
*
* Caller(s)   : NetNDP_PrefixAddCfg(),
*               NetNDP_RxPrefixHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_PREFIX  *NetNDP_PrefixSrch (       NET_IF_NBR      if_nbr,
                                            const  NET_IPv6_ADDR  *p_addr_prefix,
                                                   NET_ERR        *p_err)
{
    NET_NDP_PREFIX    *p_prefix;
    NET_NDP_PREFIX    *p_prefix_next;
    NET_NDP_PREFIX    *p_prefix_rtn;
    CPU_INT08U        *p_prefix_addr;
    CPU_BOOLEAN        found;


    p_prefix_rtn = DEF_NULL;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
     NetIF_IsValidCfgdHandler(if_nbr, p_err);
     if (*p_err != NET_IF_ERR_NONE) {
          goto exit;
     }
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_addr_prefix == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

    p_prefix = NetNDP_PrefixListHead;
    while (p_prefix != DEF_NULL) {                              /* Search Prefix List ...                               */
                                                                /* ... until Prefix found.                              */

        p_prefix_next = p_prefix->NextPtr;

        if (p_prefix->IF_Nbr == if_nbr) {

            p_prefix_addr = &p_prefix->Prefix.Addr[0];

                                                                /* Compare prefixes.                                    */
            found         =  Mem_Cmp((void     *)p_addr_prefix,
                                     (void     *)p_prefix_addr,
                                     (CPU_SIZE_T)NET_IPv6_ADDR_SIZE);
            if (found == DEF_YES) {                             /* If Prefix found, return prefix.                      */
                p_prefix_rtn = p_prefix;
               *p_err        = NET_NDP_ERR_NONE;
                goto exit;
            }
        }

        p_prefix = p_prefix_next;                               /* Advertise to next Prefix.                            */
    }


   *p_err = NET_NDP_ERR_PREFIX_NOT_FOUND;


exit:
    return (p_prefix_rtn);
}


/*
*********************************************************************************************************
*                                         NetNDP_PrefixGet()
*
* Description : (1) Get a Prefix entry.
*
*                   (a) Get a prefix entry from the prefix pool.
*                   (b) Insert prefix in the prefix list.
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Prefix successfully acquired.
*                               NET_NDP_ERR_PREFIX_NONE_AVAIL   NO available prefix to allocate.
*
* Return(s)   : Pointer to prefix entry.
*
* Caller(s)   : NetNDP_PrefixCfg().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_PREFIX  *NetNDP_PrefixGet(NET_ERR  *p_err)
{
    NET_NDP_PREFIX  *p_prefix;
    NET_ERR          err;


    if (NetNDP_PrefixPoolPtr != (NET_NDP_PREFIX *)0) {          /* If Prefix pool NOT empty, ...                        */
                                                                /* ...  get Prefix from pool.                           */
        p_prefix              =  NetNDP_PrefixPoolPtr;
        NetNDP_PrefixPoolPtr  = (NET_NDP_PREFIX *)p_prefix->NextPtr;

                                                                /* If Prefix List NOT empty, ...                        */
    } else if (NetNDP_PrefixListTail != (NET_NDP_PREFIX *)0) {
                                                                /* ... get Prefix from list tail.                       */
        p_prefix             = (NET_NDP_PREFIX *)NetNDP_PrefixListTail;
        NetNDP_PrefixRemove(p_prefix, DEF_YES);
        p_prefix             = NetNDP_PrefixPoolPtr;
        NetNDP_PrefixPoolPtr = (NET_NDP_PREFIX *)p_prefix->NextPtr;

    } else {                                                   /* Else none avail, rtn err.                             */
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.PrefixNoneAvailCtr);
       *p_err =   NET_NDP_ERR_PREFIX_NONE_AVAIL;
        return ((NET_NDP_PREFIX *)0);
    }

                                                                /* -------------------- INIT PREFIX ------------------- */
    NetNDP_PrefixClr(p_prefix);

                                                                /* ------------ UPDATE PREFIX POOL STATS -------------- */
    NetStat_PoolEntryUsedInc(&NetNDP_PrefixPoolStat, &err);


                                                                /* ---------- INSERT PREFIX INTO PREFIX LIST ---------- */
    p_prefix->PrevPtr = (NET_NDP_PREFIX *)0;
    p_prefix->NextPtr = NetNDP_PrefixListHead;
                                                                /* If list NOT empty, insert before head.               */
    if (NetNDP_PrefixListHead != (NET_NDP_PREFIX *)0) {
        NetNDP_PrefixListHead->PrevPtr = p_prefix;
    } else {                                                    /* Else add first Prefix to list.                       */
        NetNDP_PrefixListTail = p_prefix;
    }
                                                                /* Insert Prefix @ list head.                           */
    NetNDP_PrefixListHead = p_prefix;

   *p_err =  NET_NDP_ERR_NONE;

    return (p_prefix);
}


/*
*********************************************************************************************************
*                                        NetNDP_PrefixRemove()
*
* Description : (1) Remove a prefix from the prefix list.
*
*                   (a) Unlink prefix from prefix list.
*                   (b) Free prefix entry.
*
*
* Argument(s) : p_prefix    Pointer to the Prefix entry to remove from the Prefix list.
*
*               tmr_free    Indicate whether to free network timer :
*
*                               DEF_YES                Free network timer for prefix.
*                               DEF_NO          Do NOT free network timer for prefix.
*                                                     [Freed by NetTmr_TaskHandler()].
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_CfgAddrPrefix(),
*               NetNDP_PrefixCfg(),
*               NetNDP_PrefixGet().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_PrefixRemove(NET_NDP_PREFIX  *p_prefix,
                                  CPU_BOOLEAN      tmr_free)
{
    NET_NDP_PREFIX  *p_prefix_next;
    NET_NDP_PREFIX  *p_prefix_prev;
    NET_ERR          err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_prefix == (NET_NDP_PREFIX *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_prefix_prev = p_prefix->PrevPtr;
    p_prefix_next = p_prefix->NextPtr;

                                                                /* ---------- UNLINK PREFIX FROM PREFIX LIST ---------- */
                                                                /* Point previous Prefix to next Prefix.                */
    if (p_prefix_prev != (NET_NDP_PREFIX *)0) {
        p_prefix_prev->NextPtr = p_prefix_next;
    } else {
        NetNDP_PrefixListHead  = p_prefix_next;
    }
                                                                /* Point next Prefix to previous Prefix.                */
    if (p_prefix_next != (NET_NDP_PREFIX *)0) {
        p_prefix_next->PrevPtr = p_prefix_prev;
    } else {
        NetNDP_PrefixListTail  = p_prefix_prev;
    }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                     /* Clear Prefix's pointers.                             */
    p_prefix->PrevPtr = (NET_NDP_PREFIX *)0;
    p_prefix->NextPtr = (NET_NDP_PREFIX *)0;
#endif

                                                                /* ----------------- FREE PREFIX TMR ------------------ */
    if (tmr_free == DEF_YES) {
        if (p_prefix->TmrPtr != (NET_TMR *)0) {
         NetTmr_Free(p_prefix->TmrPtr);
         p_prefix->TmrPtr = (NET_TMR *)0;
        }
    }

                                                                /* ---------------- CLEAR PREFIX ENTRY ---------------- */
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        NetNDP_PrefixClr(p_prefix);
#endif

                                                                /* ---------------- FREE PREFIX ENTRY ----------------- */
    p_prefix->NextPtr    = (NET_NDP_PREFIX *)NetNDP_PrefixPoolPtr;
    NetNDP_PrefixPoolPtr = (NET_NDP_PREFIX *)p_prefix;

                                                               /* ------------- UPDATE PREFIX POOL STATS -------------- */
    NetStat_PoolEntryUsedDec(&NetNDP_PrefixPoolStat, &err);
}


/*
*********************************************************************************************************
*                                        NetNDP_PrefixClr()
*
* Description : Clear a Prefix entry.
*
* Argument(s) : p_prefix    Pointer to the Prefix entry to clear.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_PrefixGet(),
*               NetNDP_PrefixRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_PrefixClr(NET_NDP_PREFIX  *p_prefix)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP PREFIX --------------- */
    if (p_prefix == (NET_NDP_PREFIX *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_prefix->PrevPtr = (NET_NDP_PREFIX *)0;
    p_prefix->NextPtr = (NET_NDP_PREFIX *)0;

    p_prefix->IF_Nbr  = NET_IF_NBR_NONE;

    Mem_Clr(&p_prefix->Prefix, NET_IPv6_ADDR_SIZE);

    if (p_prefix->TmrPtr != (NET_TMR *)0) {
        NetTmr_Free(p_prefix->TmrPtr);
        p_prefix->TmrPtr = (NET_TMR *)0;
    }
}


/*
*********************************************************************************************************
*                                         NetNDP_DestCacheCfg()
*
* Description : (1) Get and Configure a Destination entry from the Destination pool.
*
*                   (a) Get a Destination from the destination pool and insert it in destination cache.
*                   (b) Configure destination with received arguments.
*
*
* Argument(s) : if_nbr          Interface number for the destination to configure.
*
*               p_addr_dest     Pointer to IPv6 Destination address.
*
*               p_addr_next_hop Pointer to Next-Hop IPv6 address.
*
*               is_valid        Indicate whether are not the Next-Hop address is valid.
*                                   DEF_YES, address is   valid
*                                   DEF_NO,  address is invalid
*
*               on_link         Indicate whether are not the Destination is on link.
*                                   DEF_YES, destination is     on link
*                                   DEF_NO,  destination is not on link
*
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                Destination successfully configured.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_addr_dest'/'p_addr_next_hop'
*                                                                   passed a NULL pointer
*
*                                                               ----- RETURNED BY NetNDP_DestGet() : -----
*                               NET_NDP_ERR_DEST_NONE_AVAIL     NO available destination to allocate.
*
* Return(s)   : Pointer to destination entry configured.
*
* Caller(s)   : NetNDP_NextHop(),
*               NetNDP_NextHopByIF().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static NET_NDP_DEST_CACHE  *NetNDP_DestCacheCfg (       NET_IF_NBR      if_nbr,
                                                 const  NET_IPv6_ADDR  *p_addr_dest,
                                                 const  NET_IPv6_ADDR  *p_addr_next_hop,
                                                        CPU_BOOLEAN     is_valid,
                                                        CPU_BOOLEAN     on_link,
                                                        NET_ERR        *p_err)
{
    NET_NDP_DEST_CACHE *p_dest;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_dest == (NET_IPv6_ADDR *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return ((NET_NDP_DEST_CACHE *)0);
    }
#endif

    p_dest = NetNDP_DestCacheGet(p_err);
    if (p_dest == (NET_NDP_DEST_CACHE *)0) {
       return ((NET_NDP_DEST_CACHE *)0);                        /* Return err from NetNDP_DestGet().                    */
    }


                                                                /* -------------- CFG DESTINATION ENTRY --------------- */
    p_dest->IF_Nbr  = if_nbr;
    p_dest->IsValid = is_valid;
    p_dest->OnLink  = on_link;

    Mem_Copy(&p_dest->AddrDest, p_addr_dest, NET_IPv6_ADDR_SIZE);

    if (p_addr_next_hop != (NET_IPv6_ADDR *)0) {
        Mem_Copy(&p_dest->AddrNextHop, p_addr_next_hop, NET_IPv6_ADDR_SIZE);
    }


   *p_err = NET_NDP_ERR_NONE;

    return (p_dest);
}


/*
*********************************************************************************************************
*                                         NetNDP_DestCacheGet()
*
* Description : (1) Get a Destination cache entry from the Destination pool.
*
*                   (a) Get a free destination entry from pool.
*                   (b) Insert destination into cache list.
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                    Destination successfully acquired.
*                               NET_NDP_ERR_DEST_CACHE_NONE_AVAIL   NO available destination to allocate.
*
* Return(s)   : Pointer to Destination Cache entry.
*
* Caller(s)   : NetNDP_DestCacheCfg().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheGet(NET_ERR  *p_err)
{
    NET_NDP_DEST_CACHE  *p_dest;
    NET_ERR              err;


    if (NetNDP_DestPoolPtr != (NET_NDP_DEST_CACHE *)0) {        /* If Destination entry pool NOT empty, ...             */
                                                                /* ...  get destination from pool.                      */
        p_dest              = NetNDP_DestPoolPtr;
        NetNDP_DestPoolPtr  = (NET_NDP_DEST_CACHE *)p_dest->NextPtr;

                                                                /* If Destination Cache List NOT empty, ...             */
    } else if (NetNDP_DestListTail != (NET_NDP_DEST_CACHE *)0) {

        p_dest = NetNDP_DestCacheSrchInvalid(p_err);            /* ... find an invalid Destination cache to remove ...  */

        if (p_dest == (NET_NDP_DEST_CACHE *) 0) {
            p_dest = (NET_NDP_DEST_CACHE *)NetNDP_DestListTail; /* ... if none avail, get Dest entry from list tail.    */
        }

        NetNDP_DestCacheRemove(p_dest);
        p_dest              = NetNDP_DestPoolPtr;
        NetNDP_DestPoolPtr  = (NET_NDP_DEST_CACHE *)p_dest->NextPtr;

    } else {                                                   /* Else none avail, return err.                          */
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.DestCacheNoneAvailCtr);
       *p_err =   NET_NDP_ERR_DEST_CACHE_NONE_AVAIL;
        return ((NET_NDP_DEST_CACHE *)0);
    }

                                                                /* -------------- INIT DESTINATION ENTRY -------------- */
    NetNDP_DestCacheClr(p_dest);

                                                                /* --------- UPDATE DESTINATION POOL STATS ------------ */
    NetStat_PoolEntryUsedInc(&NetNDP_DestPoolStat, &err);

                                                                /* ---- INSERT DESTINATION INTO DESTINATION CACHE ----- */
    p_dest->PrevPtr = (NET_NDP_DEST_CACHE *)0;
    p_dest->NextPtr = NetNDP_DestListHead;
                                                                /* If list NOT empty, insert before head.               */
    if (NetNDP_DestListHead != (NET_NDP_DEST_CACHE *)0) {
        NetNDP_DestListHead->PrevPtr = p_dest;
    } else {                                                    /* Else add first Destination to list.                  */
        NetNDP_DestListTail = p_dest;
    }
                                                                /* Insert Destination @ list head.                      */
    NetNDP_DestListHead = p_dest;

   *p_err =  NET_NDP_ERR_NONE;

    return (p_dest);
}


/*
*********************************************************************************************************
*                                        NetNDP_DestCacheSrch()
*
* Description : Search for a matching Destination entry for the given address into the Destination Cache.
*
* Argument(s) : if_nbr      Interface number of the destination to look for.
*
*               p_addr      Pointer to the ipv6 address to look for in the Destination Cache.
*
*               p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                    Destination FOUND in Destination cache.
*                               NET_NDP_ERR_DESTINATION_NOT_FOUND   Destination NOT found.
*
* Return(s)   : Pointer to destination entry found in destination cache.
*
* Caller(s)   : NetNDP_NextHop(),
*               NetNDP_NextHopByIF(),
*               NetNDP_RxRedirect().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheSrch (       NET_IF_NBR      if_nbr,
                                                   const  NET_IPv6_ADDR  *p_addr,
                                                          NET_ERR        *p_err)
{
    NET_NDP_DEST_CACHE  *p_dest;
    CPU_INT08U          *p_dest_addr;
    CPU_BOOLEAN          found;


    found  =  DEF_NO;
    p_dest = NetNDP_DestListHead;

    while ((p_dest != (NET_NDP_DEST_CACHE *)0) &&               /* Srch Destination Cache List ...                      */
           (found  ==  DEF_NO)                ) {               /* ... until cache found.                               */

        if (p_dest->IF_Nbr == if_nbr) {

            p_dest_addr = &p_dest->AddrDest.Addr[0];
                                                                /* Cmp address with Destination cache address.          */
            found       =  Mem_Cmp((void     *)p_addr,
                                   (void     *)p_dest_addr,
                                   (CPU_SIZE_T)NET_IPv6_ADDR_SIZE);

            if (found == DEF_YES) {                             /* If Destination Cache found, ...                      */
               *p_err = NET_NDP_ERR_NONE;                       /* ... rtn found Destination cache.                     */
                return (p_dest);
            }
        }

        p_dest = p_dest->NextPtr;                               /* Advertise to next Destination cache.                 */
    }

   *p_err  =  NET_NDP_ERR_DESTINATION_NOT_FOUND;

    return (DEF_NULL);
}


/*
*********************************************************************************************************
*                                     NetNDP_DestCacheSrchInvalid()
*
* Description : Search for an invalid destination cache entry in the destination cache.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function:
*
*                               NET_NDP_ERR_NONE                    Invalid Destination found.
*                               NET_NDP_ERR_DESTINATION_NOT_FOUND   No invalid Destination found.
*
* Return(s)   : Pointer to Destination Cache entry found.
*
* Caller(s)   : NetNDP_DestCacheGet().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_NDP_DEST_CACHE  *NetNDP_DestCacheSrchInvalid(NET_ERR  *p_err)
{
    NET_NDP_DEST_CACHE  *p_dest_cache;
    NET_NDP_DEST_CACHE  *p_dest_cache_next;


    p_dest_cache = (NET_NDP_DEST_CACHE *)NetNDP_DestListHead;
    while (p_dest_cache != (NET_NDP_DEST_CACHE *)0) {           /* Search Destination Cache List.                       */

        p_dest_cache_next = (NET_NDP_DEST_CACHE *) p_dest_cache->NextPtr;

        if (p_dest_cache->IsValid == DEF_NO) {
                                                                /* If Destination Cache found, ...                      */
           *p_err = NET_NDP_ERR_NONE;                           /* ... rtn found Destination cache.                     */
            return (p_dest_cache);

        }

        p_dest_cache = (NET_NDP_DEST_CACHE *)p_dest_cache_next; /* Advertise to next Destination cache.                 */
    }

   *p_err  =  NET_NDP_ERR_DESTINATION_NOT_FOUND;

    return (p_dest_cache);
}


/*
*********************************************************************************************************
*                                      NetNDP_DestCacheRemove()
*
* Description : (1) Remove a Destination cache entry from the Destination Cache.
*
*                   (a) Unlink destination entry from list
*                   (b) Free destination entry
*
*
* Argument(s) : p_router    Pointer to the Destination entry to remove from the Destination Cache list.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_DestGet().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_DestCacheRemove(NET_NDP_DEST_CACHE  *p_dest)
{
    NET_NDP_DEST_CACHE  *p_dest_next;
    NET_NDP_DEST_CACHE  *p_dest_prev;
    NET_ERR              err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* -------------- VALIDATE NDP DESTINATION ------------ */
    if (p_dest == (NET_NDP_DEST_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_dest_prev = p_dest->PrevPtr;
    p_dest_next = p_dest->NextPtr;

                                                                /* --------- UNLINK DEST ENTRY FROM DEST LIST --------- */
                                                                /* Point previous Destination to next Destination.      */
    if (p_dest_prev != (NET_NDP_DEST_CACHE *)0) {
        p_dest_prev->NextPtr = p_dest_next;
    } else {
        NetNDP_DestListHead  = p_dest_next;
    }
                                                                /* Point next Destination to previous Destination.      */
    if (p_dest_next != (NET_NDP_DEST_CACHE *)0) {
        p_dest_next->PrevPtr = p_dest_prev;
    } else {
        NetNDP_DestListTail  = p_dest_prev;
    }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                     /* Clear Destination entry pointers.                    */
    p_dest->PrevPtr = (NET_NDP_DEST_CACHE *)0;
    p_dest->NextPtr = (NET_NDP_DEST_CACHE *)0;
#endif

                                                                /* -------------- CLEAR DESTINATION ENTRY ------------- */
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
     NetNDP_DestCacheClr(p_dest);
#endif

                                                                /* -------------- FREE DESTINATION ENTRY -------------- */
    p_dest->NextPtr    = (NET_NDP_DEST_CACHE *)NetNDP_DestPoolPtr;
    NetNDP_DestPoolPtr = (NET_NDP_DEST_CACHE *)p_dest;

                                                               /* -------- UPDATE DESTINATION ENTRY POOL STATS -------- */
    NetStat_PoolEntryUsedDec(&NetNDP_DestPoolStat, &err);
}


/*
*********************************************************************************************************
*                                        NetNDP_DestCacheClr()
*
* Description : Clear a Destination cache entry.
*
* Argument(s) : p_dest    Pointer to the Destination entry to clear.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_DestGet(),
*               NetNDP_DestRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetNDP_DestCacheClr(NET_NDP_DEST_CACHE  *p_dest)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* -------------- VALIDATE NDP DESTINATION ------------ */
    if (p_dest == (NET_NDP_DEST_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_dest->PrevPtr = (NET_NDP_DEST_CACHE *)0;
    p_dest->NextPtr = (NET_NDP_DEST_CACHE *)0;

    p_dest->IF_Nbr = NET_IF_NBR_NONE;

    p_dest->IsValid = DEF_NO;
    p_dest->OnLink  = DEF_NO;

    Mem_Clr(&p_dest->AddrDest, NET_IPv6_ADDR_SIZE);
    Mem_Clr(&p_dest->AddrNextHop, NET_IPv6_ADDR_SIZE);

}


/*
*********************************************************************************************************
*                                       NetNDP_IsAddrOnLink()
*
* Description : Validate if an IPv6 address is on-link or not.
*
* Argument(s) : if_nbr      Interface number of the address to validate.
*
*               p_addr      Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is on-link.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetNDP_NextHopByIF(),
*               NetNDP_NextHop().
*
* Note(s)     : (1) A node considers an IPv6 address to be on-link if that addresses satisfied one of
*                   the following conditions:
*
*                   (a) The address is covered by one of the on-link prefixes assigned to the link.
*
*                   (b) The address is the target address of a Redirect message sent by a router.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetNDP_IsAddrOnLink (       NET_IF_NBR      if_nbr,
                                          const  NET_IPv6_ADDR  *p_addr)
{
    NET_NDP_PREFIX      *p_prefix;
    CPU_BOOLEAN          addr_linklocal;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN          valid;
#endif
    NET_ERR              net_err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, &net_err);
    if (valid != DEF_YES) {
        return (DEF_NO);
    }
#endif
                                                                /* -------------- VALIDATE IPv6 ADDR PTR -------------- */
    if (p_addr == (NET_IPv6_ADDR *)0) {
        return (DEF_NO);
    }
                                                                /* A link-local addr is always consider on link.        */
    addr_linklocal = NetIPv6_IsAddrLinkLocal(p_addr);
    if (addr_linklocal == DEF_TRUE) {
        return (DEF_YES);
    }

                                                                /* Srch prefix list for a matching prefix to the addr.  */
    p_prefix = NetNDP_PrefixSrchMatchAddr(if_nbr,
                                          p_addr,
                                         &net_err);
    if (p_prefix != (NET_NDP_PREFIX *)0) {
        return (DEF_YES);
    }

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                         NetNDP_ReachableTimeout()
*
* Description : Change the NDP neighbor cache state from 'REACHABLE' to 'STALE'.
*
* Argument(s) : p_cache_timeout      Pointer to an NDP neighbor cache.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetNDP_RxNeighborSolicitation(),
*                               NetNDP_RxNeighborAdvertisement().
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetNDP_CacheFree() via NetNDP_CacheRemove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/

static  void  NetNDP_ReachableTimeout (void  *p_cache_timeout)
{
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_TMR_TICK             timeout_tick;
    NET_ERR                  err;
    CPU_SR_ALLOC();


    p_cache = (NET_NDP_NEIGHBOR_CACHE *)p_cache_timeout;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP CACHE ---------------- */
    if (p_cache == (NET_NDP_NEIGHBOR_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

    p_cache->TmrPtr = (NET_TMR *)0;

    p_cache->State  = NET_NDP_CACHE_STATE_STALE;
    CPU_CRITICAL_ENTER();
    timeout_tick    = NetNDP_NeighborCacheTimeout_tick;
    CPU_CRITICAL_EXIT();

    p_cache->TmrPtr = NetTmr_Get(         NetNDP_CacheTimeout,
                                 (void *) p_cache,
                                          timeout_tick,
                                         &err);

    if (err != NET_TMR_ERR_NONE) {                              /* If tmr unavailable, free NDP cache.                  */
        NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);
        return;
    }
}


/*
*********************************************************************************************************
*                                      NetNDP_SolicitTimeout()
*
* Description : Retry NDP Solicitation to resolve an NDP neighbor cache in the 'INCOMPLETE' or
*               "PROBE" state on the NDP Solicitation timeout.
*
* Argument(s) : p_cache_timeout    Pointer to an NDP neighbor cache (see Note #1b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetNDP_NeighborCacheAddPend(),
*                               NetNDP_DelayTimeout().
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetNDP_CacheFree() via NetNDP_CacheRemove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*
*               (3) RFC 4861 section 7.7.2 :
*                   (a) "While awaiting a response, the sender SHOULD retransmit Neighbor
*                        Solicitation messages approximately every RetransTimer milliseconds,
*                        even in the absence of additional traffic to the neighbor.
*                        Retransmissions MUST be rate-limited to at most one solicitation per
*                        neighbor every RetransTimer milliseconds."
*
*                   (b) "If no Neighbor Advertisement is received after MAX_MULTICAST_SOLICIT
*                        solicitation, address resolution has failed.  The sender MUST return
*                        ICMP destination unreachable indications with code 3 (Address
*                        Unreachable) for each packet queued awaiting address resolution."
*
*                        NetICMPv6_TxMsgErr() function is not adequate because it assume that p_buf is a
*                        pointer to a received packet and not a queue of packet waiting to be send.
*                        Therefore the address destination and address source are inverted when sending
*                        the error message.
*                        #### NET-780
*                        #### NET-781
*********************************************************************************************************
*/

static  void  NetNDP_SolicitTimeout (void  *p_cache_timeout)
{
    NET_NDP_ROUTER             *p_router;
    NET_NDP_NEIGHBOR_CACHE     *p_cache;
    NET_CACHE_ADDR_NDP         *p_cache_addr_ndp;
#if 0
    NET_BUF                    *p_buf_list;
    NET_BUF                    *p_buf_list_next;
    NET_BUF                    *p_buf;
    NET_BUF                    *p_buf_next;
    NET_BUF_HDR                *p_buf_hdr;
#endif
    NET_TMR_TICK                timeout_tick;
    CPU_INT08U                  th_max;
    CPU_INT08U                  ndp_cache_state;
    NET_NDP_NEIGHBOR_SOL_TYPE   ndp_sol_type;
    CPU_BOOLEAN                 is_router;
    NET_ERR                     err;
    CPU_SR_ALLOC();


    p_cache          = (NET_NDP_NEIGHBOR_CACHE *)p_cache_timeout;
    p_cache_addr_ndp = p_cache->CacheAddrPtr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP CACHE ---------------- */
    if (p_cache == (NET_NDP_NEIGHBOR_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }

    if (p_cache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif


    p_cache->TmrPtr = (NET_TMR *)0;                            /* Un-reference tmr in the NDP cache.                   */

    ndp_cache_state = p_cache->State;
    switch (ndp_cache_state) {
        case NET_NDP_CACHE_STATE_INCOMPLETE :
             CPU_CRITICAL_ENTER();
             th_max = NetNDP_SolicitMaxAttempsMulticast_nbr;
             CPU_CRITICAL_EXIT();
             ndp_sol_type = NET_NDP_NEIGHBOR_SOL_TYPE_RES;
             break;


        case NET_NDP_CACHE_STATE_PROBE :
             CPU_CRITICAL_ENTER();
             th_max = NetNDP_SolicitMaxAttempsUnicast_nbr;
             CPU_CRITICAL_EXIT();
             ndp_sol_type = NET_NDP_NEIGHBOR_SOL_TYPE_NUD;
             break;


        default:
            return;
    }

    if (p_cache->ReqAttemptsCtr >= th_max) {                    /* If nbr attempts >= max, ...                          */
                                                                /* ... update dest cache to remove unreachable next-hop */
        NetNDP_RemoveAddrDestCache(p_cache_addr_ndp->IF_Nbr,
                                  &p_cache_addr_ndp->AddrProtocol[0]);

                                                                /* ... if Neighbor cache is also a router, ...          */
        is_router = DEF_BIT_IS_SET(p_cache->Flags, NET_NDP_CACHE_FLAG_ISROUTER);
        if (is_router == DEF_YES) {
            p_router = NetNDP_RouterSrch(                  p_cache_addr_ndp->IF_Nbr,
                                         (NET_IPv6_ADDR *)&p_cache_addr_ndp->AddrProtocol,
                                                          &err);
            if (p_router != (NET_NDP_ROUTER *)0) {
                NetNDP_RouterRemove(p_router, DEF_YES);         /* ... delete router in Default router list.            */
                NetNDP_UpdateDefaultRouter(p_cache_addr_ndp->IF_Nbr, &err);
            }
        }

                                                                /* See Note 3.b                                         */
#if 0
        if(p_cache->State == NET_NDP_CACHE_STATE_INCOMPLETE) {
            p_buf_list = p_cache_addr_ndp->TxQ_Head;
            while (p_buf_list  != (NET_BUF *)0) {
                p_buf_hdr       = &p_buf_list->Hdr;
                p_buf_list_next =  p_buf_hdr->NextSecListPtr;
                p_buf           = p_buf_list;
                while (p_buf != (NET_BUF *)0) {
                    p_buf_hdr  = &p_buf->Hdr;
                    p_buf_next = p_buf_hdr->NextBufPtr;
                    NetICMPv6_TxMsgErr(p_buf_list,
                                       NET_ICMPv6_MSG_TYPE_DEST_UNREACH,
                                       NET_ICMPv6_MSG_CODE_DEST_ADDR_UNREACHABLE,
                                       NET_ICMPv6_MSG_PTR_NONE,
                                      &err);
                    p_buf = p_buf_next;
                }

                p_buf_list = p_buf_list_next;
            }
        }
#endif

        NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);       /* ... free NDP cache.                                  */

        return;
    }

                                                                /* ------------------ RETRY NDP REQ ------------------- */
    CPU_CRITICAL_ENTER();
    timeout_tick   = NetNDP_SolicitTimeout_tick;
    CPU_CRITICAL_EXIT();

    p_cache->TmrPtr = NetTmr_Get(         NetNDP_SolicitTimeout,
                                 (void *) p_cache,
                                          timeout_tick,
                                         &err);
    if (err != NET_TMR_ERR_NONE) {                              /* If tmr unavail, free NDP cache.                      */
        NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_NO);
        return;
    }

                                                                /* ------------- RE_TX NEIGHBOR SOL MSG --------------- */

    NetNDP_TxNeighborSolicitation(                 p_cache_addr_ndp->IF_Nbr,
                                  (NET_IPv6_ADDR *)p_cache_addr_ndp->AddrProtocolSender,
                                  (NET_IPv6_ADDR *)p_cache_addr_ndp->AddrProtocol,
                                                   ndp_sol_type,
                                                  &err);

    p_cache->ReqAttemptsCtr++;                                  /* Inc req attempts ctr.                                */

}


/*
*********************************************************************************************************
*                                         NetNDP_RouterTimeout()
*
* Description : (1) Remove Router entry in NDP default router list.
*
*                   (a) Remove Next-Hop address in destination cache corresponding to router.
*                   (b) Remove NDP router entry in default router list.
*                   (c) Update the Default Router.
*
*
* Argument(s) : p_cache_timeout      Pointer to an NDP router.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetNDP_RxRouterAdvertisement.
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetNDP_CacheFree() via NetNDP_CacheRemove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/

static void  NetNDP_RouterTimeout(void  *p_router_timeout)
{
    NET_NDP_ROUTER      *p_router;
    NET_ERR              err_net;


    p_router = (NET_NDP_ROUTER *)p_router_timeout;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP ROUTER --------------- */
    if (p_router == (NET_NDP_ROUTER *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

                                                                /* Rem. NextHop in Dest. Cache corresponding to router. */
    NetNDP_RemoveAddrDestCache(p_router->IF_Nbr, &p_router->Addr.Addr[0]);

                                                                /* Remove Router from the default router list.          */
    p_router->TmrPtr = (NET_TMR *)0;
    NetNDP_RouterRemove(p_router, DEF_NO);

                                                                /* Update the Default Router.                           */
    NetNDP_UpdateDefaultRouter(p_router->IF_Nbr, &err_net);

}


/*
*********************************************************************************************************
*                                         NetNDP_PrefixTimeout()
*
* Description : (1) Remove Prefix entry in NDP prefix list.
*
*                   (a) Remove Next-Hop address in Destination Cache with corresponding prefix.
*                   (b) Remove prefix entry from NDP prefix list.
*
*
* Argument(s) : p_prefix_timeout    Pointer to NDP prefix entry.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetNDP_PrefixHandler().
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetNDP_CacheFree() via NetNDP_CacheRemove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/

static  void  NetNDP_PrefixTimeout (void  *p_prefix_timeout)
{
    NET_NDP_PREFIX  *p_prefix;


    p_prefix = (NET_NDP_PREFIX *)p_prefix_timeout;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE PREFIX ------------------ */
    if (p_prefix == (NET_NDP_PREFIX *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        return;
    }
#endif

                                                                /* Remove next-hop with prefix in destination cache ... */
                                                                /* ... See RFC 4861 section 5.3.                        */
    NetNDP_RemovePrefixDestCache(p_prefix->IF_Nbr,
                                &p_prefix->Prefix.Addr[0],
                                 p_prefix->PrefixLen);

                                                                /* Remove Prefix entry in Prefix list.                  */
    p_prefix->TmrPtr = (NET_TMR *)0;
    NetNDP_PrefixRemove(p_prefix, DEF_NO);
}


/*
*********************************************************************************************************
*                                         NetNDP_DAD_Timeout()
*
* Description : Retry NDP Request (sending NS) for the Duplication Address Detection (DAD).
*
* Argument(s) : p_cache_timeout     Pointer to an NDP cache.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetNDP_DupAddrDetection.
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_NDP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetNDP_CacheFree() via NetNDP_CacheRemove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/
#ifdef NET_DAD_MODULE_EN
static  void  NetNDP_DAD_Timeout(void  *p_cache_timeout)
{
    NET_IF_NBR               if_nbr;
    NET_IPv6_ADDRS          *p_ipv6_addrs;
    NET_IPv6_ADDR           *p_addr;
    NET_NDP_NEIGHBOR_CACHE  *p_cache;
    NET_CACHE_ADDR_NDP      *p_cache_addr_ndp;
    NET_DAD_OBJ             *p_dad_obj;
    NET_TMR_TICK             timeout_tick;
    CPU_INT08U               th_max;
    NET_IPv6_ADDR_CFG_STATUS status;
    NET_ERR                  err;
    KAL_ERR                  err_kal;
    CPU_SR_ALLOC();


    p_cache = (NET_NDP_NEIGHBOR_CACHE *) p_cache_timeout;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE NDP CACHE ---------------- */
    if (p_cache == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
        CPU_SW_EXCEPTION();
    }
#endif

    p_cache_addr_ndp = p_cache->CacheAddrPtr;

    if_nbr  = p_cache_addr_ndp->IF_Nbr;

    p_cache->TmrPtr = DEF_NULL;

    p_addr    = (NET_IPv6_ADDR *) &p_cache_addr_ndp->AddrProtocol[0];
    p_dad_obj = NetDAD_ObjSrch(p_addr, &err);
    if (err != NET_DAD_ERR_NONE) {
        CPU_SW_EXCEPTION();
    }

    CPU_CRITICAL_ENTER();
    th_max = NetNDP_DADMaxAttempts_nbr;
    CPU_CRITICAL_EXIT();

                                                                /* --------- VERIFY IF DAD SIGNAL ERR RECEIVED -------- */
    NetDAD_SignalWait(NET_DAD_SIGNAL_TYPE_ERR, p_dad_obj, &err);
    if (err == NET_DAD_ERR_NONE) {
        KAL_SemSet(p_dad_obj->SignalErr, 0, &err_kal);          /* DAD process failed.                                  */
        status = NET_IPv6_ADDR_CFG_STATUS_DUPLICATE;
        goto exit_update;
    }

                                                                /* -------- VERIFY IF ALL DAD ATTEMPTS ARE SENT ------- */
    if (p_cache->ReqAttemptsCtr >= th_max) {
        status = NET_IPv6_ADDR_CFG_STATUS_SUCCEED;              /* DAD process succeeded.                               */
        goto exit_update;
    }

                                                                /* ------------------ RETRY NDP REQ ------------------- */
    CPU_CRITICAL_ENTER();
    timeout_tick   = NetNDP_SolicitTimeout_tick;
    CPU_CRITICAL_EXIT();

                                                                /* Get new timer for NDP cache.                         */
    p_cache->TmrPtr = NetTmr_Get(         NetNDP_DAD_Timeout,
                                 (void *) p_cache,
                                          timeout_tick,
                                         &err);
    if (err != NET_TMR_ERR_NONE) {                              /* If timer unavailable, free NDP cache.                */
        status = NET_IPv6_ADDR_CFG_STATUS_FAIL;
        goto exit_update;
    }

                                                                /* Transmit NDP Solicitation message.                   */
    NetNDP_TxNeighborSolicitation(                  if_nbr,
                                                    DEF_NULL,
                                  (NET_IPv6_ADDR *)&p_cache_addr_ndp->AddrProtocol[0],
                                                    NET_NDP_NEIGHBOR_SOL_TYPE_DAD,
                                                   &err);
    if (err != NET_NDP_ERR_NONE) {
        status = NET_IPv6_ADDR_CFG_STATUS_FAIL;
        goto exit_update;
    }

    p_cache->ReqAttemptsCtr++;                                  /* Inc req attempts ctr.                                */

    goto exit;


exit_update:
                                                                /* ------------ RECOVER IPv6 ADDRS OBJECT ------------- */
    p_ipv6_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_addr);
    if (p_ipv6_addrs == DEF_NULL) {
        goto exit_clear;
    }

                                                                /* --------------- UPDATE ADDRESS STATE --------------- */
    if (status == NET_IPv6_ADDR_CFG_STATUS_SUCCEED) {
        p_ipv6_addrs->AddrState = NET_IPv6_ADDR_STATE_PREFERRED;
        p_ipv6_addrs->IsValid   = DEF_YES;
    } else {
        p_ipv6_addrs->AddrState = NET_IPv6_ADDR_STATE_DUPLICATED;
        p_ipv6_addrs->IsValid   = DEF_NO;
    }


exit_clear:

    NetNDP_NeighborCacheRemoveEntry(p_cache, DEF_YES);          /* Free NDP cache.                                      */

    NetDAD_Signal(NET_DAD_SIGNAL_TYPE_COMPL,                    /* Signal that DAD process is complete.                 */
                  p_dad_obj,
                 &err);
    if (p_dad_obj->Fnct != DEF_NULL) {
        p_dad_obj->Fnct(if_nbr, p_dad_obj, status);
    }


exit:
    return;
}
#endif


/*
*********************************************************************************************************
*                                     NetNDP_RouterAdvSignalPost()
*
* Description : Post Rx Router Advertisement Signal.
*
* Argument(s) : if_nbr      Network Interface Number on which Router Adv. message was received.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_NDP_ERR_NONE                        Signal post successfully.
*                               NET_NDP_ERR_ROUTER_ADV_SIGNAL_FAULT     Signal posting faulted.
*
* Return(s)   : None.
*
* Caller(s)   : NetNDP_RxPrefixUpdate().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  void  NetNDP_RouterAdvSignalPost(NET_IF_NBR   if_nbr,
                                         NET_ERR     *p_err)
{
    KAL_SEM_HANDLE  *p_signal;
    KAL_ERR          err_kal;


    p_signal = &NetNDP_RxRouterAdvSignal[if_nbr];
    if (p_signal->SemObjPtr != DEF_NULL) {
        KAL_SemPost(*p_signal,
                     KAL_OPT_PEND_NONE,
                    &err_kal);
        if (err_kal != KAL_ERR_NONE) {
           *p_err = NET_NDP_ERR_ROUTER_ADV_SIGNAL_FAULT;
            goto exit;
        }
    }

   *p_err = NET_NDP_ERR_NONE;


exit:
   return;
}
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_ndp.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of NDP module include (see Note #1).             */

