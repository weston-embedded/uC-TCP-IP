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
* Filename : net_cache.c
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
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    NET_CACHE_MODULE
#include  "net_cache.h"
#include  "net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_arp.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ndp.h"
#endif

#include  "net_type.h"
#include  "net_stat.h"
#include  "net_tmr.h"

#include  "../IF/net_if.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_CACHE_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static  NET_CACHE_ADDR  *NetCache_AddrGet (NET_CACHE_TYPE   cache_type,
                                           NET_ERR         *p_err);

static  void             NetCache_AddrFree(NET_CACHE_ADDR  *pcache,
                                           CPU_BOOLEAN      tmr_free);

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  CPU_BOOLEAN      NetCache_IsUsed   (NET_CACHE_ADDR   *pcache);

static  void             NetCache_Discard  (NET_CACHE_ADDR  *pcache);
#endif

static  void             NetCache_Unlink   (NET_CACHE_ADDR  *pcache);

static  void             NetCache_Clr      (NET_CACHE_ADDR  *pcache);


/*
*********************************************************************************************************
*                                           NetCache_Init()
*
* Description : (1) Initialize address cache:
*
*                   (a) Demultiplex  parent cache type
*                   (b) Set  address        cache type
*                   (c) Set hardware address type
*                   (d) Set hardware address length
*                   (e) Set protocol address length
*                   (f) Set protocol address length
*                   (g) Free address cache to cache pool
*
* Argument(s) : pcache_parent   Pointer on the parent   cache to be associated with the address cache.
*
*               pcache_child    Pointer on the address cache to be initialized.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_CACHE_ERR_NONE             Cache successfully initialized.
*
*                               NET_CACHE_ERR_INVALID_TYPE     Cache is NOT a valid cache type.
*
* Caller(s)   : NetARP_Init(),
*               NetNDP_Init().
*
* Note(s)     : (2) Cache pool MUST be           initialized PRIOR to initializing the pool with pointers
*                   to caches.
*
*               (3) Each NDP cache addr type are initialized to NET_CACHE_TYPE_NDP but will be modified to
*                   their respective NDP type when used.
*
*                   See also 'net_cache.h  NETWORK CACHE TYPE DEFINES'.
*********************************************************************************************************
*/

void  NetCache_Init (NET_CACHE_ADDR  *pcache_parent,
                     NET_CACHE_ADDR  *pcache_addr,
                     NET_ERR         *p_err)
{

    switch (pcache_parent->Type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
                                                                /* Init each ARP addr cache type--NEVER modify.         */
             pcache_addr->Type             = NET_CACHE_TYPE_ARP;

                                                                /* Init each ARP HW       type/addr len--NEVER modify.  */
             pcache_addr->AddrHW_Type      = NET_ADDR_HW_TYPE_802x;
             pcache_addr->AddrHW_Len       = NET_IF_HW_ADDR_LEN_MAX;

                                                                /* Init each ARP protocol type/addr len--NEVER modify.  */
             pcache_addr->AddrProtocolType = NET_PROTOCOL_TYPE_IP_V4;
             pcache_addr->AddrProtocolLen  = NET_IPv4_ADDR_SIZE;

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
             NetCache_Clr((NET_CACHE_ADDR *)pcache_addr);
#endif
                                                                /* Free ARP cache to cache pool (see Note #2).          */
             pcache_addr->NextPtr          = (NET_CACHE_ADDR     *)NetCache_AddrARP_PoolPtr;
             NetCache_AddrARP_PoolPtr      = (NET_CACHE_ADDR_ARP *)pcache_addr;
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
                                                                /* Init each NDP addr cache type (see Note #3.)         */
             pcache_addr->Type             = NET_CACHE_TYPE_NDP;

                                                                /* Init each NDP HW       type/addr len--NEVER modify.  */
             pcache_addr->AddrHW_Type      = NET_ADDR_HW_TYPE_802x;
             pcache_addr->AddrHW_Len       = NET_IF_HW_ADDR_LEN_MAX;

                                                                /* Init each NDP protocol type/addr len--NEVER modify.  */
             pcache_addr->AddrProtocolType = NET_PROTOCOL_TYPE_IP_V6;
             pcache_addr->AddrProtocolLen  = NET_IPv6_ADDR_SIZE;

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
             NetCache_Clr((NET_CACHE_ADDR *)pcache_addr);
#endif
                                                                /* Free NDP cache to cache pool (see Note #2).          */
             pcache_addr->NextPtr          = (NET_CACHE_ADDR     *)NetCache_AddrNDP_PoolPtr;
             NetCache_AddrNDP_PoolPtr      = (NET_CACHE_ADDR_NDP *)pcache_addr;
             break;
#endif

        default:
           *p_err = NET_CACHE_ERR_INVALID_TYPE;
            return;
    }

    pcache_addr->AddrHW_Valid            = DEF_NO;
    pcache_addr->AddrProtocolValid       = DEF_NO;
    pcache_addr->AddrProtocolSenderValid = DEF_NO;
                                                                /* Set ptr to parent cache.                             */
    pcache_addr->ParentPtr               = (void *)pcache_parent;


   *p_err = NET_CACHE_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetCache_CfgAccessedTh()
*
* Description : Configure cache access promotion threshold.
*
* Argument(s) : cache_type      Cache type:
*
*                                   NET_CACHE_TYPE_ARP           ARP          cache type
*                                   NET_CACHE_TYPE_NDP           NDP neighbor cache type
*
*               nbr_access      Desired number of cache accesses before cache is promoted.
*
* Return(s)   : DEF_OK,   cache access promotion threshold configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) 'NetARP_CacheAccessedTh_nbr' & 'NetNDP_CacheAccessedTh_nbr'  MUST ALWAYS be accessed
*                    exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetCache_CfgAccessedTh (NET_CACHE_TYPE  cache_type,
                                     CPU_INT16U      nbr_access)
{
    CPU_SR_ALLOC();


    switch (cache_type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
#if (NET_ARP_CACHE_ACCESSED_TH_MIN > DEF_INT_16U_MIN_VAL)
             if (nbr_access < NET_ARP_CACHE_ACCESSED_TH_MIN) {
                 return (DEF_FAIL);
             }
#endif
#if (NET_ARP_CACHE_ACCESSED_TH_MAX < DEF_INT_16U_MAX_VAL)
             if (nbr_access > NET_ARP_CACHE_ACCESSED_TH_MAX) {
                 return (DEF_FAIL);
             }
#endif

             CPU_CRITICAL_ENTER();
             NetARP_CacheAccessedTh_nbr = nbr_access;
             CPU_CRITICAL_EXIT();
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
#if (NET_NDP_CACHE_ACCESSED_TH_MIN > DEF_INT_16U_MIN_VAL)
             if (nbr_access < NET_NDP_CACHE_ACCESSED_TH_MIN) {
                 return (DEF_FAIL);
             }
#endif
#if (NET_NDP_CACHE_ACCESSED_TH_MAX < DEF_INT_16U_MAX_VAL)
             if (nbr_access > NET_NDP_CACHE_ACCESSED_TH_MAX) {
                 return (DEF_FAIL);
             }
#endif

             CPU_CRITICAL_ENTER();
             NetNDP_CacheAccessedTh_nbr = nbr_access;
             CPU_CRITICAL_EXIT();
             break;
#endif

        default:
             return (DEF_FAIL);

    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         NetCache_CfgAddrs()
*
* Description : (1) Configure a cache :
*
*                   (a) Get cache from cache pool
*                   (b) Get cache timer
*                   (c) Configure cache :
*                       (1) Configure interface number
*                       (2) Configure cache addresses :
*                           (A) Hardware address
*                           (B) Protocol address(s)
*                       (3) Configure cache controls
*
*
* Argument(s) :
*
*               if_nbr                  Interface number for this cache entry.
*               ------                  Argument validated in NetCache_AddResolved
*                                                             NetARP_CacheProbeAddrOnNet(),
*                                                             NetARP_CacheAddPend(),
*                                                             NetNDP_CacheAddPend().
*
*               cache_type              Cache type:
*
*                                           NET_CACHE_TYPE_ARP     ARP          cache type
*                                           NET_CACHE_TYPE_NDP     NDP neighbor cache type
*
*               paddr_hw                Pointer to hardware address        (see Note #2b).
*
*               addr_hw_len             Hardware address length.
*
*               paddr_protocol          Pointer to protocol address        (see Note #2c).
*               --------------          Argument checked in   NetCache_AddResolved
*                                                             NetARP_CacheProbeAddrOnNet(),
*                                                             NetARP_CacheAddPend(),
*                                                             NetNDP_CacheAddPend().
*
*               paddr_protocol_sender   Pointer to sender protocol address (see note #2a).
*
*               addr_protocol_len       Protocol address length.
*
*               timeout_fnct            Pointer to timeout function.
*
*               timeout_tick            Timeout value (in 'NET_TMR_TICK' ticks).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_CACHE_ERR_NONE                  Cache successfully configured.
*                               NET_ERR_FAULT_NULL_PTR              Variable is set to Null pointer.
*
*                                                                   ----- RETURNED BY NetCache_AddrGet() : -----
*                               NET_CACHE_ERR_NONE_AVAIL            NO available caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE          Cache is NOT a valid cache type.
*
*                                                                   -------- RETURNED BY NetTmr_Get() : --------
*                               NET_ERR_FAULT_NULL_FNCT               Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL              NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE            Network timer is NOT a valid timer type.
*
* Return(s)   : none.
*
* Caller(s)   : NetCache_AddResolved
*               NetARP_CacheProbeAddrOnNet(),
*               NetARP_CacheAddPend(),
*               NetNDP_CacheAddPend().
*
* Note(s)     : (2) (a) If 'paddr_protocol_sender' available, MUST point to a valid protocol address, in
*                           network-order, configured on interface number 'if_nbr'.
*
*                   (b) If 'paddr_hw'              available, MUST point to a valid hardware address, in
*                           network-order.
*
*                   (c) 'paddr_protocol' MUST point to a valid protocol address in network-order.
*
*                   See also 'net_arp.c NetARP_CacheHandler() Note #2e3' &
*                            'net_ndp.c NetNDP_CacheHandler() Note #2e3'.
*
*               (3) On ANY error(s), network resources MUST be appropriately freed.
*
*               (4) During ARP cache initialization, some cache controls were previously initialized
*                   in NetCache_AddrGet() when the cache was allocated from the cache pool.  These cache
*                   controls do NOT need to be re-initialized but are shown for completeness.
*********************************************************************************************************
*/
NET_CACHE_ADDR  *NetCache_CfgAddrs (NET_CACHE_TYPE       cache_type,
                                    NET_IF_NBR           if_nbr,
                                    CPU_INT08U          *paddr_hw,
                                    NET_CACHE_ADDR_LEN   addr_hw_len,
                                    CPU_INT08U          *paddr_protocol,
                                    CPU_INT08U          *paddr_protocol_sender,
                                    NET_CACHE_ADDR_LEN   addr_protocol_len,
                                    CPU_BOOLEAN          timer_en,
                                    CPU_FNCT_PTR         timeout_fnct,
                                    NET_TMR_TICK         timeout_tick,
                                    NET_ERR             *p_err)
{
#ifdef  NET_ARP_MODULE_EN
    NET_CACHE_ADDR_ARP      *pcache_addr_arp;
    NET_ARP_CACHE           *pcache_arp;
#endif
#ifdef  NET_NDP_MODULE_EN
    NET_CACHE_ADDR_NDP      *pcache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE  *pcache_ndp;
#endif


    (void)&addr_hw_len;                                         /* Prevent 'variable unused' compiler warning.          */
    (void)&addr_protocol_len;


    switch (cache_type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                    /* --------------- VALIDATE HW LEN ---------------- */
             if (addr_hw_len != NET_IF_HW_ADDR_LEN_MAX) {
                *p_err = NET_CACHE_ERR_INVALID_ADDR_HW_LEN;
                 return ((NET_CACHE_ADDR *)0);
             }
                                                                    /* ------------ VALIDATE PROTOCOL LEN ------------- */
             if (addr_protocol_len != NET_IPv4_ADDR_SIZE) {
                *p_err = NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN;
                 return ((NET_CACHE_ADDR *)0);
             }
#endif
                                                                    /* ---------------- GET ARP CACHE ----------------- */
            pcache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_AddrGet(cache_type, p_err);

             if (pcache_addr_arp == DEF_NULL) {
                 return (DEF_NULL);                                 /* Rtn err from NetCache_AddrGet().                 */
             }

             if (timer_en == DEF_ENABLED) {
                                                                    /* -------------- GET ARP CACHE TMR --------------- */
                 pcache_arp = (NET_ARP_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)pcache_addr_arp);
                 if (pcache_arp == DEF_NULL) {
                     *p_err = NET_ERR_FAULT_NULL_PTR;
                      return (DEF_NULL);                            /* Rtn err from NetCache_GetParent().               */
                 }


                 pcache_arp->TmrPtr = NetTmr_Get(timeout_fnct,
                                                 pcache_arp,
                                                 timeout_tick,
                                                 p_err);
                 if (*p_err != NET_TMR_ERR_NONE) {                  /* If tmr unavail, ...                              */
                                                                    /* ... free ARP cache (see Note #3).                */
                      NetCache_AddrFree((NET_CACHE_ADDR *)pcache_addr_arp, DEF_NO);
                      return (DEF_NULL);
                 }
             }

                                                                    /* ---------------- CFG ARP CACHE ----------------- */
                                                                    /* Cfg ARP cache addr(s).                           */
             if (paddr_hw != (CPU_INT08U *)0) {                     /* If hw addr avail (see Note #2b), ...             */
                 Mem_Copy((void     *)&pcache_addr_arp->AddrHW[0],  /* ... copy into ARP cache.                         */
                          (void     *) paddr_hw,
                          (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

                 pcache_addr_arp->AddrHW_Valid = DEF_YES;
             }

             if (paddr_protocol_sender != (CPU_INT08U *)0) {        /* If sender protocol addr avail (see Note #2a), ...*/
                 Mem_Copy((void     *)&pcache_addr_arp->AddrProtocolSender[0],
                          (void     *) paddr_protocol_sender,       /* ... copy into ARP cache.                         */
                          (CPU_SIZE_T) NET_IPv4_ADDR_SIZE);

                 pcache_addr_arp->AddrProtocolSenderValid = DEF_YES;
             }

             Mem_Copy((void     *)&pcache_addr_arp->AddrProtocol[0],/* Copy protocol addr into ARP cache (see Note #2c).*/
                      (void     *) paddr_protocol,
                      (CPU_SIZE_T) NET_IPv4_ADDR_SIZE);

             pcache_addr_arp->AddrProtocolValid = DEF_YES;

                                                                    /* Cfg ARP cache ctrl(s).                           */
             pcache_addr_arp->IF_Nbr         = if_nbr;

#if 0                                                               /* Init'd in NetCache_AddrGet() [see Note #4].      */
             pcache_addr_arp->AccessedCtr    =  0u;
             pcache_addr_arp->ReqAttemptsCtr =  0u;

             pcache_addr_arp->TxQ_Head       = (NET_BUF         *)0;
             pcache_addr_arp->TxQ_Tail       = (NET_BUF         *)0;
                                                                    /* Cfg'd  in NetCache_Insert().                     */
             pcache_addr_arp->PrevPtr        = (NET_CACHE_ADDR *)0;
             pcache_addr_arp->NextPtr        = (NET_CACHE_ADDR *)0;
#endif

            *p_err =  NET_CACHE_ERR_NONE;
             return((NET_CACHE_ADDR *)pcache_addr_arp);
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                   /* --------------- VALIDATE HW LEN ---------------- */
             if (addr_hw_len != NET_IF_HW_ADDR_LEN_MAX) {
                *p_err = NET_CACHE_ERR_INVALID_ADDR_HW_LEN;
                 return ((NET_CACHE_ADDR *)0);
             }
                                                                   /* ------------ VALIDATE PROTOCOL LEN -------------- */
             if (addr_protocol_len != NET_IPv6_ADDR_SIZE) {
                *p_err = NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN;
                 return ((NET_CACHE_ADDR *)0);
             }
#endif
                                                                    /* ---------------- GET NDP CACHE ----------------- */
             pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrGet(cache_type, p_err);

             if (pcache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
                  return ((NET_CACHE_ADDR *)0);                     /* Rtn err from NetCache_AddrGet().                 */
             }

             if (timer_en == DEF_ENABLED) {
                                                                    /* -------------- GET NDP CACHE TMR --------------- */
                 pcache_ndp = (NET_NDP_NEIGHBOR_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)pcache_addr_ndp);
                 if (pcache_ndp == (NET_NDP_NEIGHBOR_CACHE *)0) {
                    *p_err = NET_ERR_FAULT_NULL_PTR;
                     return ((NET_CACHE_ADDR *)0);                  /* Rtn err from NetCache_GetParent().               */
                 }

                 pcache_ndp->TmrPtr = NetTmr_Get((CPU_FNCT_PTR)timeout_fnct,
                                                 (void       *)pcache_ndp,
                                                 (NET_TMR_TICK)timeout_tick,
                                                 (NET_ERR    *)p_err);
                 if (*p_err != NET_TMR_ERR_NONE) {                  /* If tmr unavail, ...                              */
                                                                    /* ... free NDP cache (see Note #3).                */
                      NetCache_AddrFree((NET_CACHE_ADDR *)pcache_addr_ndp, DEF_NO);
                      return ((NET_CACHE_ADDR *)0);
                 }
             }
                                                                    /* ---------------- CFG NDP CACHE ----------------- */
                                                                    /* Cfg NDP cache addr(s).                           */
             if (paddr_hw != (CPU_INT08U *)0) {                     /* If hw addr avail (see Note #2b), ...             */
                 Mem_Copy((void     *)&pcache_addr_ndp->AddrHW[0],  /* ... copy into NDP cache.                         */
                          (void     *) paddr_hw,
                          (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

                 pcache_addr_ndp->AddrHW_Valid = DEF_YES;
             }

             if (paddr_protocol_sender != (CPU_INT08U *)0) {        /* If sender protocol addr avail (see Note #2a), ...*/
                 Mem_Copy((void     *)&pcache_addr_ndp->AddrProtocolSender[0],
                          (void     *) paddr_protocol_sender,       /* ... copy into NDP cache.                         */
                          (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);

                 pcache_addr_ndp->AddrProtocolSenderValid = DEF_YES;
             }

             Mem_Copy((void     *)&pcache_addr_ndp->AddrProtocol[0],/* Copy protocol addr into NDP cache (see Note #2c).*/
                      (void     *) paddr_protocol,
                      (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);

             pcache_addr_ndp->AddrProtocolValid     = DEF_YES;

                                                                    /* Cfg NDP cache ctrl(s).                           */
             pcache_addr_ndp->IF_Nbr         =  if_nbr;

#if 0                                                               /* Init'd in NetNDP_CacheGet() [see Note #4].       */
             pcache_addr_ndp->AccessedCtr    =  0u;
             pcache_addr_ndp->ReqAttemptsCtr =  0u;

             pcache_addr_ndp->TxQ_Head       = (NET_BUF       *)0;
             pcache_addr_ndp->TxQ_Tail       = (NET_BUF       *)0;
                                                                /* Cfg'd  in NetNDP_CacheInsert().                      */
             pcache_addr_ndp->PrevPtr        = (NET_NDP_CACHE *)0;
             pcache_addr_ndp->NextPtr        = (NET_NDP_CACHE *)0;
#endif

            *p_err =  NET_CACHE_ERR_NONE;
             return((NET_CACHE_ADDR *)pcache_addr_ndp);
#endif

        default:
            *p_err =  NET_CACHE_ERR_INVALID_TYPE;
             return ((NET_CACHE_ADDR *)0);
    }
}


/*
*********************************************************************************************************
*                                        NetCache_GetParent()
*
* Description : Get a ptr on a parent cache.
*
* Argument(s) : pcache      Pointer to child cache.
*
* Return(s)   : Pointer on the parent cache, if valid.
*
*               Pointer to NULL,             otherwise.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  *NetCache_GetParent (NET_CACHE_ADDR  *pcache)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------ VALIDATE PTRS ------------------- */
    if (pcache == (NET_CACHE_ADDR *)0) {
        return ((void *)0);
    }

    if (pcache->ParentPtr == (void *)0) {
        return ((void *)0);
    }
#endif

    return (pcache->ParentPtr);
}


/*
*********************************************************************************************************
*                                         NetCache_AddrSrch()
*
* Description : Search cache list for cache with specific protocol address.
*
*               (1) (a) Cache List resolves protocol-address-to-hardware-address bindings based on the
*                       following cache fields :
*
*                           (A) Some fields are configured at compile time
*                                   (see 'net_arp.h  ARP CACHE  Note #3').
*
*                       (1) Cache    Type                Should be configured at compile time (see Note #1aA)
*                       (2) Hardware Type                Should be configured at compile time (see Note #1aA)
*                       (3) Hardware Address Length      Should be configured at compile time (see Note #1aA)
*                       (4) Protocol Type                Should be configured at compile time (see Note #1aA)
*                       (5) Protocol Address Length      Should be configured at compile time (see Note #1aA)
*                       (6) Protocol Address             Should be generated  at run     time
*
*                   (b) Caches are linked to form Cache List.
*
*                       (1) In the diagram below, ... :
*
*                           (A) The top horizontal row represents the list of caches.
*
*                           (B) (1) 'NetCache_???_ListHead' points to the head of the Cache List;
*                               (2) 'NetCache_???_ListTail' points to the tail of the Cache List.
*
*                           (C) Caches' 'PrevPtr' & 'NextPtr' doubly-link each cache to form the Cache List.
*
*                       (2) Caches in the 'PENDING' state are pending hardware address resolution by an
*                           ARP Reply.  While in the 'PENDING' state, ALL transmit packet buffers are enqueued
*                           for later transmission when the corresponding ARP Reply is received.
*
*                           In the diagram below, ... :
*
*                           (A) (1) ARP caches' 'TxQ_Head' points to the head of the pending transmit packet queue;
*                               (2) ARP caches' 'TxQ_Tail' points to the tail of the pending transmit packet queue.
*
*                           (B) Buffer's 'PrevSecListPtr' & 'NextSecListPtr' link each buffer in a pending transmit
*                               packet queue.
*
*                       (3) (A) For any ARP cache lookup, all ARP caches are searched in order to find the
*                               ARP cache with the appropriate hardware address--i.e. the ARP cache with the
*                               corresponding protocol address (see Note #1a5).
*
*                           (B) To expedite faster ARP cache lookup for recently added (or recently promoted)
*                               ARP caches :
*
*                               (1) (a) (1) ARP caches are added at (or promoted to); ...
*                                       (2) ARP caches are searched starting at       ...
*                                   (b) ... the head of the ARP Cache List.
*
*                               (2) (a) As ARP caches are added into the list, older ARP caches migrate to the
*                                       tail of the ARP Cache List.  Once an ARP cache expires or is discarded,
*                                       it is removed from the ARP Cache List.
*
*                                   (b) Also if NO ARP cache is available & a new ARP cache is needed, then
*                                       the oldest ARP cache at the tail of the ARP Cache List is removed for
*                                       allocation.
*
*
*                                          |                                               |
*                                          |<-------------- List of Caches --------------->|
*                                          |               (see Note #1b1A)                |
*
*                                        New caches                              Oldest cache in
*                                     inserted at head                             Cache List
*                                    (see Note #1b3B1b)                         (see Note #1b3B2)
*
*                                             |             NextPtr                     |
*                                             |        (see Note #1b1C)                 |
*                                             v                   |                     v
*                                                                 |
*                           Head of        -------       -------  v    -------       -------   (see Note #1b1B2)
*                          Cache List ---->|     |------>|     |------>|     |------>|     |
*                                          |     |       |     |       |     |       |     |        Tail of
*                                          |     |<------|     |<------|     |<------|     |<----  Cache List
*                     (see Note #1b1B1)    -------       -------  ^    -------       -------
*                                            | |                  |      | |
*                                            | |                  |      | |
*                                            | ------       PrevPtr      | ------
*                            TxQ_Head  --->  |      |  (see Note #1b1C)  |      | <---  TxQ_Tail
*                        (see Note #1b2A1)   v      |                    v      |   (see Note #1b2A2)
*               ---                        -------  |                  -------  |
*                ^                         |     |  |                  |     |  |
*                |                         |     |  |                  |     |  |
*                |                         |     |  |                  |     |  |
*                |                         |     |  |                  |     |  |
*                |                         -------  |                  -------  |
*                |                           | ^    |                    | ^    |
*                |       NextSecListPtr ---> | |    |                    | | <----- PrevSecListPtr
*                |      (see Note #1b2B)     v |    |                    v |    |  (see Note #1b2B)
*                |                         -------  |                  -------  |
*                                          |     |  |                  |     |<--
*        Buffers pending on                |     |  |                  |     |
*          cache resolution                |     |  |                  -------
*         (see Note #1b2)                  |     |  |
*                                          -------  |
*                |                           | ^    |
*                |                           | |    |
*                |                           v |    |
*                |                         -------  |
*                |                         |     |<--
*                |                         |     |
*                |                         |     |
*                v                         |     |
*               ---                        -------
*
*
* Argument(s) : cache_type          Cache type:
*
*                                       NET_CACHE_TYPE_ARP     ARP          cache type
*                                       NET_CACHE_TYPE_NDP     NDP neighbor cache type
*
*               paddr_hw            Pointer to variable that will receive the hardware address (see Note #3).
*
*               addr_hw_len         Hardware address length.
*
*               paddr_protocol      Pointer to protocol address (see Note #2).
*               --------------      Argument checked in NetARP_CacheHandler(),
*                                                       NetNDP_CacheHandler(),
*                                                       NetARP_CacheProbeAddrOnNet().
*
*               addr_protocol_len   Protocol address length.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_CACHE_ERR_RESOLVED                  Protocol address successfully      resolved.
*
*                               NET_CACHE_ERR_PEND                      Protocol address     found but NOT resolved.
*
*                               NET_CACHE_ERR_NOT_FOUND                 Protocol address NOT found.
*
*                               NET_CACHE_ERR_INVALID_TYPE              Cache is NOT a valid cache type.
*                               NET_CACHE_ERR_INVALID_ADDR_HW_LEN       Invalid hardware address length.
*                               NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN    Invalid hardware address length.
*
* Return(s)   : Pointer to cache with specific protocol address, if found.
*
*               Pointer to NULL,                                 otherwise.
*
* Caller(s)   : NetARP_CacheHandler(),
*               NetNDP_CacheHandler(),
*               NetNDP_ProbeAddrOnNet().
*
* Note(s)     : (2) 'paddr_protocol' MUST point to a protocol address in network-order.
*
*                   See also 'net_arp.c NetARP_CacheHandler() Note #2e3' &
*                            'net_ndp.c NetNDP_CacheHandler() Note #2e3'.
*
*               (3) The hardware address is returned in network-order; i.e. the pointer to the hardware
*                   address points to the highest-order octet.
*********************************************************************************************************
*/

NET_CACHE_ADDR  *NetCache_AddrSrch (NET_CACHE_TYPE       cache_type,
                                    NET_IF_NBR           if_nbr,
                                    CPU_INT08U          *paddr_protocol,
                                    NET_CACHE_ADDR_LEN   addr_protocol_len,
                                    NET_ERR             *p_err)
{
#ifdef  NET_ARP_MODULE_EN
    NET_CACHE_ADDR_ARP  *pcache_addr_arp;
#endif
#ifdef  NET_NDP_MODULE_EN
    NET_CACHE_ADDR_NDP  *pcache_addr_ndp;
#endif
    NET_CACHE_ADDR      *pcache;
    NET_CACHE_ADDR      *pcache_next;
    CPU_INT08U          *pcache_addr;
    CPU_INT16U           th;
    CPU_BOOLEAN          found;
    CPU_SR_ALLOC();


    (void)&addr_protocol_len;                                   /* Prevent 'variable unused' compiler warning.          */
    (void)&if_nbr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    switch (cache_type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
             if (addr_protocol_len != NET_IPv4_ADDR_SIZE) {
                *p_err = NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN;
                 return ((NET_CACHE_ADDR *)0);
             }
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
             if (addr_protocol_len != NET_IPv6_ADDR_SIZE) {
                *p_err = NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN;
                 return ((NET_CACHE_ADDR *)0);
             }
             break;
#endif

        default:
             *p_err = NET_CACHE_ERR_INVALID_TYPE;
              return ((NET_CACHE_ADDR *)0);
    }
#endif

    found  =  DEF_NO;
   *p_err   =  NET_CACHE_ERR_NOT_FOUND;
    pcache = (NET_CACHE_ADDR *)0;

    switch (cache_type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
             pcache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP];
             while ((pcache_addr_arp != (NET_CACHE_ADDR_ARP *)0) &&         /* Srch    ARP Cache List ...               */
                    (found  ==  DEF_NO)) {                                  /* ... until cache found.                   */

                 pcache_next     = (NET_CACHE_ADDR *) pcache_addr_arp->NextPtr;
                 pcache_addr     = (CPU_INT08U     *)&pcache_addr_arp->AddrProtocol[0];

                                                                            /* Cmp ARP cache protocol addr.             */
                 found           =  Mem_Cmp((void     *)paddr_protocol,
                                            (void     *)pcache_addr,
                                            (CPU_SIZE_T)NET_IPv4_ADDR_SIZE);

                 if (found != DEF_YES) {                                    /* If NOT found, ..                         */
                     pcache_addr_arp = (NET_CACHE_ADDR_ARP *)pcache_next;   /* .. adv to next ARP cache.                */

                 } else {                                                   /* Else rtn found NDP cache.                */
                     pcache = (NET_CACHE_ADDR *)pcache_addr_arp;
                     if (pcache_addr_arp->AddrHW_Valid == DEF_YES) {
                        *p_err = NET_CACHE_ERR_RESOLVED;                    /* HW addr is     avail.                    */
                     } else {
                        *p_err = NET_CACHE_ERR_PEND;                        /* HW addr is NOT avail.                    */
                     }

                     pcache_addr_arp->AccessedCtr++;
                     CPU_CRITICAL_ENTER();
                     th = NetARP_CacheAccessedTh_nbr;
                     CPU_CRITICAL_EXIT();
                     if (pcache->AccessedCtr > th) {                        /* If ARP cache accessed > th, & ..         */
                         pcache->AccessedCtr = 0u;
                                                                            /* .. ARP cache NOT @ list head, ..         */
                         if (pcache != NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP]) {
                             NetCache_Unlink(pcache);
                             NetCache_Insert(pcache);                       /* .. promote ARP cache to list head.       */
                         }
                     }
                 }
             }
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
             pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP];
             while ((pcache_addr_ndp != (NET_CACHE_ADDR_NDP *)0) &&         /* Srch    NDP Cache List ...               */
                    (found           ==  DEF_NO)) {                         /* ... until cache found.                   */

                 pcache_next     = (NET_CACHE_ADDR *) pcache_addr_ndp->NextPtr;
                 pcache_addr     = (CPU_INT08U     *)&pcache_addr_ndp->AddrProtocol[0];


                 if (pcache_addr_ndp->Type != cache_type) {
                     pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)pcache_next;
                     continue;
                 }
                                                                            /* Cmp NDP cache protocol addr.             */
                 if (paddr_protocol != (CPU_INT08U *)0){
                     if (pcache_addr_ndp->IF_Nbr == if_nbr){
                         found =  Mem_Cmp((void     *)paddr_protocol,
                                          (void     *)pcache_addr,
                                          (CPU_SIZE_T)NET_IPv6_ADDR_SIZE);
                     }
                 } else {
                    *p_err   =  NET_CACHE_ERR_NOT_FOUND;
                     return (pcache);
                 }

                 if (found != DEF_YES) {                                    /* If NOT found, ..                         */
                     pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)pcache_next;   /* .. adv to next NDP cache.                */

                 } else {                                                   /* Else rtn found NDP cache.                */
                     pcache = (NET_CACHE_ADDR *)pcache_addr_ndp;
                     if (pcache->AddrHW_Valid == DEF_YES) {
                        *p_err = NET_CACHE_ERR_RESOLVED;                    /* HW addr is     avail.                    */
                     } else {
                        *p_err = NET_CACHE_ERR_PEND;                        /* HW addr is NOT avail.                    */
                     }

                     pcache_addr_ndp->AccessedCtr++;
                     CPU_CRITICAL_ENTER();
                     th = NetNDP_CacheAccessedTh_nbr;
                     CPU_CRITICAL_EXIT();
                     if (pcache->AccessedCtr > th) {                        /* If NDP cache accessed > th, & ..         */
                         pcache->AccessedCtr = 0u;
                                                                            /* .. NDP cache NOT @ list head, ..         */
                         if (pcache != NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP]) {
                             NetCache_Unlink(pcache);
                             NetCache_Insert(pcache);                       /* .. promote NDP cache to list head.       */
                         }
                     }
                 }
             }
             break;
#endif

        default:
           *p_err =  NET_CACHE_ERR_INVALID_TYPE;
            return ((NET_CACHE_ADDR *)0);
    }

    return (pcache);
}


/*
*********************************************************************************************************
*                                        NetCache_AddResolved()
*
* Description : (1) Add a 'RESOLVED' cache into the Cache List :
*
*                   (a) Configure cache :
*                       (1) Get default-configured cache
*                       (2) Cache state
*                   (b) Insert    cache into Cache List
*
*
* Argument(s) : if_nbr          Interface number for this cache entry.
*
*               paddr_hw        Pointer to hardware address (see Note #2a).
*               --------        Argument checked in caller(s)
*
*               paddr_protocol  Pointer to protocol address (see Note #2b).
*               --------------  Argument checked in caller(s)
*
*               cache_type      Cache type:
*
*                                   NET_CACHE_TYPE_ARP     ARP          cache type
*                                   NET_CACHE_TYPE_NDP     NDP neighbor cache type
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_CACHE_ERR_RESOLVED              Cache added in 'RESOLVED' state.
*                               NET_ERR_FAULT_NULL_PTR              Variable is set to Null pointer.
*
*                                                                   -- RETURNED BY NetCache_CfgAddrs() : --
*                               NET_CACHE_ERR_NONE_AVAIL            NO available caches to allocate.
*                               BET_CACHE_ERR_INVALID_TYPE          Cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT               Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL              NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE            Network timer is NOT a valid timer type.
*                               NET_ERR_FAULT_NULL_PTR              Variable is set to Null pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_CacheHandler(),
*               NetARP_RxPktCacheUpdate().
*
* Note(s)     : (2) Addresses MUST be in network-order :
*
*                   (a) 'paddr_hw'       MUST point to valid hardware address in network-order.
*                   (b) 'paddr_protocol' MUST point to valid protocol address in network-order.
*
*                   See also 'net_arp.c NetARP_CacheHandler() Note #2e3' &
*                            'net_ndp.c NetNDP_CacheHandler() Note #2e3'.
*********************************************************************************************************
*/

void  NetCache_AddResolved (NET_IF_NBR       if_nbr,
                            CPU_INT08U      *paddr_hw,
                            CPU_INT08U      *paddr_protocol,
                            NET_CACHE_TYPE   cache_type,
                            CPU_FNCT_PTR     fnct,
                            NET_TMR_TICK     timeout_tick,
                            NET_ERR         *p_err)
{
#ifdef  NET_ARP_MODULE_EN
    NET_CACHE_ADDR_ARP      *pcache_addr_arp;
    NET_ARP_CACHE           *pcache_arp;
#endif
#ifdef  NET_NDP_MODULE_EN
    NET_CACHE_ADDR_NDP      *pcache_addr_ndp;
    NET_NDP_NEIGHBOR_CACHE  *pcache_ndp;
    CPU_SR_ALLOC();
#endif



    switch (cache_type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:

             pcache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_CfgAddrs(NET_CACHE_TYPE_ARP,
                                                                       if_nbr,
                                                                       paddr_hw,
                                                                       NET_IF_HW_ADDR_LEN_MAX,
                                                                       paddr_protocol,
                                                                       0,
                                                                       NET_IPv4_ADDR_SIZE,
                                                                       DEF_YES,
                                                                       fnct,
                                                                       timeout_tick,
                                                                       p_err);

             if (pcache_addr_arp == (NET_CACHE_ADDR_ARP *)0) {
                *p_err = NET_ERR_FAULT_NULL_PTR;
                 return;
             }

             pcache_arp = (NET_ARP_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)pcache_addr_arp);
             if (pcache_arp == ((NET_ARP_CACHE *)0)) {
                *p_err = NET_ERR_FAULT_NULL_PTR;
                 return;
             }

             DEF_BIT_SET(pcache_arp->Flags, NET_CACHE_FLAG_USED);
             pcache_arp->State = NET_ARP_CACHE_STATE_RESOLVED;

                                                                /* ------- INSERT ARP CACHE INTO ARP CACHE LIST ------- */
             NetCache_Insert((NET_CACHE_ADDR *)pcache_addr_arp);

            *p_err = NET_CACHE_ERR_RESOLVED;
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
             CPU_CRITICAL_ENTER();
             timeout_tick = NetNDP_NeighborCacheTimeout_tick;
             CPU_CRITICAL_EXIT();

             pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_CfgAddrs(NET_CACHE_TYPE_NDP,
                                                                       if_nbr,
                                                                       paddr_hw,
                                                                       NET_IF_HW_ADDR_LEN_MAX,
                                                                       paddr_protocol,
                                                                       0,
                                                                       NET_IPv6_ADDR_SIZE,
                                                                       DEF_YES,
                                                                       fnct,
                                                                       timeout_tick,
                                                                       p_err);

             if (pcache_addr_ndp == (NET_CACHE_ADDR_NDP *)0) {
                *p_err = NET_ERR_FAULT_NULL_PTR;
                 return;
             }

             pcache_ndp = (NET_NDP_NEIGHBOR_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)pcache_addr_ndp);
             if (pcache_ndp == ((NET_NDP_NEIGHBOR_CACHE *)0)) {
                *p_err = NET_ERR_FAULT_NULL_PTR;
                 return;
             }

             pcache_ndp->State = NET_NDP_CACHE_STATE_REACHABLE;

                                                                         /* ------- INSERT NDP CACHE INTO NDP CACHE LIST ------- */
             NetCache_Insert((NET_CACHE_ADDR *) pcache_addr_ndp);

            *p_err = NET_CACHE_ERR_RESOLVED;

             break;
#endif

        default:
            *p_err = NET_CACHE_ERR_INVALID_TYPE;
             return;
    }

}


/*
*********************************************************************************************************
*                                          NetCache_Insert()
*
* Description : Insert a cache into the Cache List.
*
* Argument(s) : pcache      Pointer to a cache.
*               ------      Argument checked in NetCache_AddrSrch(),
*                                               NetCache_AddResolved(),
*                                               NetARP_CacheAddPend(),
*                                               NetNDP_CacheAddPend().
*
* Return(s)   : none.
*
* Caller(s)   : NetCache_AddrSrch(),
*               NetCache_AddResolved(),
*               NetARP_CacheAddPend(),
*               NetARP_CacheProbeAddrOnNet(),
*               NetNDP_CacheAddPend().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetCache_Insert (NET_CACHE_ADDR  *pcache)
{
    switch (pcache->Type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:                                /* ---------------- CFG ARP CACHE PTRS ---------------- */
             pcache->PrevPtr = (NET_CACHE_ADDR *)0;
             pcache->NextPtr = NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP];

                                                                /* ------- INSERT ARP CACHE INTO ARP CACHE LIST ------- */
                                                                /* If list NOT empty, insert before head.               */
             if (NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP] != (NET_CACHE_ADDR *)0) {
                 NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP]->PrevPtr = pcache;
             } else {                                           /* Else add first ARP cache to list.                    */
                 NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_ARP]          = pcache;
             }
                                                                /* Insert ARP cache @ list head.                        */
             NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP] = pcache;
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:                                /* ---------------- CFG NDP CACHE PTRS ---------------- */
             pcache->PrevPtr = (NET_CACHE_ADDR *)0;
             pcache->NextPtr = NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP];

                                                                /* ------- INSERT NDP CACHE INTO NDP CACHE LIST ------- */
                                                                /* If list NOT empty, insert before head.               */
             if (NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP] != (NET_CACHE_ADDR *)0) {
                 NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP]->PrevPtr = pcache;
             } else {                                           /* Else add first NDP cache to list.                    */
                 NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_NDP] = pcache;
             }
                                                                /* Insert NDP cache @ list head.                        */
             NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP]     = pcache;
             break;
#endif

        default:
             return;
    }
}


/*
*********************************************************************************************************
*                                          NetCache_Remove()
*
* Description : (1) Remove a cache from the Cache List :
*
*                   (a) Remove cache from    Cache List
*                   (b) Free   cache back to cache pool
*
*
* Argument(s) : pcache      Pointer to a cache.
*               ------      Argument checked in caller(s)
*
*               tmr_free    Indicate whether to free network timer :
*
*                               DEF_YES                Free network timer for cache.
*                               DEF_NO          Do NOT free network timer for cache
*                                                     [Freed by NetTmr_TaskHandler()].
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_CacheGetAddrHW(),
*               NetARP_CacheHandler(),
*               NetARP_CacheProbeAddrOnNet(),
*               NetARP_CacheReqTimeout(),
*               NetARP_CacheTimeout(),
*               NetARP_RxPktCacheUpdate(),
*               NetCache_AddrGet(),
*               NetCache_TxPktHandler(),
*               NetNDP_CacheClrAll(),
*               NetNDP_NeighborCacheRemoveEntry().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetCache_Remove (NET_CACHE_ADDR  *pcache,
                       CPU_BOOLEAN      tmr_free)
{
                                                                /* ----------- REMOVE CACHE FROM CACHE LIST ----------- */
    NetCache_Unlink(pcache);
                                                                /* -------------------- FREE CACHE -------------------- */
    NetCache_AddrFree(pcache, tmr_free);
}


/*
*********************************************************************************************************
*                                        NetCache_UnlinkBuf()
*
* Description : Unlink a network buffer from a cache's transmit queue.
*
* Argument(s) : pbuf        Pointer to network buffer enqueued in a cache transmit buffer queue.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetARP_CacheHandler(),
*                             NetARP_CacheAddPend(),
*                             NetNDP_CacheHandler(),
*                             NetNDP_CacheAddPend().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetCache_UnlinkBuf (NET_BUF  *pbuf)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN     used;
#endif
    NET_BUF             *pbuf_prev;
    NET_BUF             *pbuf_next;
    NET_BUF_HDR         *pbuf_hdr;
    NET_BUF_HDR         *pbuf_hdr_prev;
    NET_BUF_HDR         *pbuf_hdr_next;
    NET_CACHE_ADDR      *pcache;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------- VALIDATE PTR ------------------- */
    if (pbuf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Cache.NullPtrCtr);
        return;
    }
                                                                /* ------------------- VALIDATE BUF ------------------- */
    used = NetBuf_IsUsed(pbuf);
    if (used != DEF_YES) {
        return;
    }
#endif

    pbuf_hdr = (NET_BUF_HDR    *)&pbuf->Hdr;
    pcache   = (NET_CACHE_ADDR *) pbuf_hdr->UnlinkObjPtr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------ VALIDATE CACHE ------------------ */
    used = NetCache_IsUsed(pcache);
    if (used != DEF_YES) {
        return;
    }
#endif

                                                                /* ------------ UNLINK BUF FROM CACHE TX Q ------------ */
    pbuf_prev = pbuf_hdr->PrevSecListPtr;
    pbuf_next = pbuf_hdr->NextSecListPtr;
                                                                /* Point prev cache pending tx Q buf to next buf.       */
    if (pbuf_prev != (NET_BUF *)0) {
        pbuf_hdr_prev                 = &pbuf_prev->Hdr;
        pbuf_hdr_prev->NextSecListPtr =  pbuf_next;
    } else {
        pcache->TxQ_Head              =  pbuf_next;
    }
                                                                /* Point next cache pending tx Q buf to prev buf.       */
    if (pbuf_next != (NET_BUF *)0) {
        pbuf_hdr_next                 = &pbuf_next->Hdr;
        pbuf_hdr_next->PrevSecListPtr =  pbuf_prev;
    } else {
        pcache->TxQ_Tail              =  pbuf_prev;
    }


                                                                /* -------------- CLR BUF'S UNLINK CTRLS -------------- */
    pbuf_hdr->PrevSecListPtr = (NET_BUF    *)0;                 /* Clr pending tx Q ptrs.                               */
    pbuf_hdr->NextSecListPtr = (NET_BUF    *)0;

    pbuf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT)0;                 /* Clr unlink ptrs.                                     */
    pbuf_hdr->UnlinkObjPtr   = (void       *)0;
}


/*
*********************************************************************************************************
*                                        NetCache_TxPktHandler()
*
* Description : (1) Transmit packet buffers from cache transmit buffer queue :
*
*                   (a) Resolve  packet  buffer(s)' hardware address(s)
*                   (b) Update   packet  buffer(s)' unlink/reference values
*                   (c) Transmit packet  buffer(s)
*
*
* Argument(s) : pbuf_q      Pointer to network buffer(s) to transmit.
*
*               paddr_hw    Pointer to sender's hardware address (see Note #2).
*               --------    Argument checked in caller(s)
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_RxPktCacheUpdate(),
*               NetNDP_RxNeighborAdvertisement().
*
* Note(s)     : (2) Addresses MUST be in network-order.
*
*               (3) RFC #1122, Section 2.3.2.2 states that "the link layer SHOULD" :
*
*                   (a) "Save (rather than discard) ... packets destined to the same unresolved
*                        IP address and" ...
*                   (b) "Transmit the saved packet[s] when the address has been resolved."
*********************************************************************************************************
*/

void  NetCache_TxPktHandler (NET_PROTOCOL_TYPE   proto_type,
                             NET_BUF            *pbuf_q,
                             CPU_INT08U         *paddr_hw)
{
    NET_BUF                 *pbuf_list;
    NET_BUF                 *pbuf_list_next;
    NET_BUF                 *pbuf;
    NET_BUF                 *pbuf_next;
    NET_BUF_HDR             *pbuf_hdr;
#ifdef  NET_NDP_MODULE_EN
    NET_NDP_NEIGHBOR_CACHE  *pcache;
    NET_CACHE_ADDR_NDP      *pcache_addr_ndp;
    CPU_INT08U              *paddr_protocol;
    CPU_INT08U               cache_state;
    NET_TMR_TICK             timeout_tick;
#endif
    CPU_INT08U              *pbuf_addr_hw;
#ifdef  NET_NDP_MODULE_EN
    NET_IF_NBR               if_nbr;
    CPU_SR_ALLOC();
#endif
    NET_ERR                  err;


    pbuf_list = pbuf_q;
    while (pbuf_list  != (NET_BUF *)0) {                        /* Handle ALL buf lists in Q.                           */
        pbuf_hdr       = &pbuf_list->Hdr;
        pbuf_list_next = (NET_BUF *)pbuf_hdr->NextSecListPtr;

        pbuf           = (NET_BUF *)pbuf_list;
        while (pbuf   != (NET_BUF *)0) {                        /* Handle ALL bufs in buf list.                         */
            pbuf_hdr   = &pbuf->Hdr;
            pbuf_next  = (NET_BUF *)pbuf_hdr->NextBufPtr;

            switch (proto_type) {
#ifdef  NET_ARP_MODULE_EN
                case NET_PROTOCOL_TYPE_ARP:
                     pbuf_addr_hw = pbuf_hdr->ARP_AddrHW_Ptr;
                     Mem_Copy((void     *)pbuf_addr_hw,         /* Copy hw addr into pkt buf.                           */
                              (void     *)paddr_hw,
                              (CPU_SIZE_T)NET_IF_HW_ADDR_LEN_MAX);
                     break;
#endif
#ifdef  NET_NDP_MODULE_EN
                case NET_PROTOCOL_TYPE_NDP:
                     if_nbr       = pbuf_hdr->IF_Nbr;
                     pbuf_addr_hw = pbuf_hdr->NDP_AddrHW_Ptr;
                     Mem_Copy((void     *)pbuf_addr_hw,         /* Copy hw addr into pkt buf.                           */
                              (void     *)paddr_hw,
                              (CPU_SIZE_T)NET_IF_HW_ADDR_LEN_MAX);
                     paddr_protocol    =  pbuf_hdr->NDP_AddrProtocolPtr;
                     pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)NetCache_AddrSrch(NET_CACHE_TYPE_NDP,
                                                                               if_nbr,
                                                                               paddr_protocol,
                                                                               NET_IPv6_ADDR_SIZE,
                                                                              &err);
                     if (pcache_addr_ndp != (NET_CACHE_ADDR_NDP *)0) {           /* If NDP cache found, chk state.      */
                         pcache = (NET_NDP_NEIGHBOR_CACHE *) pcache_addr_ndp->ParentPtr;
                         cache_state = pcache->State;
                         if (cache_state == NET_NDP_CACHE_STATE_STALE) {
                             pcache->State = NET_NDP_CACHE_STATE_DLY;
                             CPU_CRITICAL_ENTER();
                             timeout_tick = NetNDP_DelayTimeout_tick;
                             CPU_CRITICAL_EXIT();
                             NetTmr_Set(pcache->TmrPtr,
                                        NetNDP_DelayTimeout,
                                        timeout_tick,
                                       &err);
                             if (err != NET_TMR_ERR_NONE) {                          /* If tmr unavail, free NDP cache. */
                                 NetCache_Remove((NET_CACHE_ADDR *)pcache_addr_ndp,  /* Clr but do NOT free tmr.        */
                                                                   DEF_YES);
                                 return;
                            }
                        }
                     }
                     break;
#endif

                default:
                     break;
            }
                                                                /* Clr buf sec list & unlink ptrs.                      */
            pbuf_hdr->PrevSecListPtr = (NET_BUF    *)0;
            pbuf_hdr->NextSecListPtr = (NET_BUF    *)0;
            pbuf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT)0;
            pbuf_hdr->UnlinkObjPtr   = (void       *)0;

            NetIF_Tx(pbuf, &err);                               /* Tx pkt to IF (see Note #3b).                         */

            pbuf  = pbuf_next;
        }

        pbuf_list = pbuf_list_next;
    }
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
*                                         NetCache_AddrGet()
*
* Description : (1) Allocate & initialize a cache :
*
*                   (a) Get a cache
*                   (b) Validate   cache
*                   (c) Initialize cache
*                   (d) Update cache pool statistics
*                   (e) Return pointer to cache
*                         OR
*                       Null pointer & error code, on failure
*
*               (2) The cache pool is implemented as a stack :
*
*                   (a) Caches' 'PoolPtr's points to the head of the   cache pool.
*
*                   (b) Caches' 'NextPtr's link each cache to form the cache pool stack.
*
*                   (c) Caches are inserted & removed at the head of   cache pool stack.
*
*
*                                         Caches are
*                                     inserted & removed
*                                        at the head
*                                       (see Note #2c)
*
*                                             |                 NextPtr
*                                             |             (see Note #2b)
*                                             v                    |
*                                                                  |
*                                          -------       -------   v   -------       -------
*                              Pool   ---->|     |------>|     |------>|     |------>|     |
*                            Pointer       |     |       |     |       |     |       |     |
*                                          |     |       |     |       |     |       |     |
*                         (see Note #2a)   -------       -------       -------       -------
*
*                                          |                                               |
*                                          |<------------ Pool of Free Caches ------------>|
*                                          |                (see Note #2)                  |
*
*
* Argument(s) : cache_type      Cache type:
*
*                                   NET_CACHE_TYPE_ARP     ARP cache type
*                                   NET_CACHE_TYPE_NDP     NDP neighbor cache type
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_CACHE_ERR_NONE                  Cache successfully allocated &
*                                                                       initialized.
*                               NET_CACHE_ERR_NONE_AVAIL            NO available caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE          Cache is NOT a valid cache type.
*
* Return(s)   : Pointer to cache, if NO error(s).
*
*               Pointer to NULL,  otherwise.
*
* Caller(s)   : NetCache_CfgAddrs().
*
* Note(s)     : (3) (a) Cache pool is accessed by 'NetARP_CachePoolPtr' during execution of
*
*                       (1) NetARP_Init()
*                       (2) NetNDP_Init()
*                       (3) NetCache_AddrGet()
*                       (4) NetCache_AddrFree()
*
*                   (b) Since the primary tasks of the network protocol suite are prevented from running
*                       concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                       resources of the cache pool since no asynchronous access from other network
*                       tasks is possible.
*
*               (4) 'No cache available' case NOT possible during correct operation of  the cache.
*                   However, the 'else' case is included as an extra precaution in case the cache
*                   list is incorrectly modified &/or corrupted.
*********************************************************************************************************
*/

static  NET_CACHE_ADDR  *NetCache_AddrGet(NET_CACHE_TYPE   cache_type,
                                          NET_ERR         *p_err)
{
#ifdef  NET_ARP_MODULE_EN
    NET_CACHE_ADDR_ARP  *pcache_addr_arp;
#endif
#ifdef  NET_NDP_MODULE_EN
    NET_CACHE_ADDR_NDP  *pcache_addr_ndp;
#endif
    NET_CACHE_ADDR      *pcache;
    NET_ERR              err;


    switch (cache_type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:                                        /* -------------- GET ARP CACHE --------------- */
             if (NetCache_AddrARP_PoolPtr != DEF_NULL) {                /* If ARP cache pool NOT empty, ...             */
                                                                        /* ...  get cache from pool.                    */
                 pcache_addr_arp           = NetCache_AddrARP_PoolPtr;
                 NetCache_AddrARP_PoolPtr  = (NET_CACHE_ADDR_ARP *)pcache_addr_arp->NextPtr;

             } else {                                                   /* If ARP Cache List NOT empty, ...              */
                                                                        /* ... get ARP cache from list tail.             */
                 NET_CACHE_ADDR  *p_entry = NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_ARP];


                 while (p_entry != DEF_NULL) {

                     if (p_entry->AddrHW_Valid == DEF_YES) {            /* Make sure to remove only a resolved entry.    */
                         break;
                     }

                     p_entry = p_entry->PrevPtr;
                 }

                 if (p_entry == DEF_NULL) {
                     NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NoneAvailCtr);
                    *p_err =   NET_CACHE_ERR_NONE_AVAIL;
                     return (DEF_NULL);
                 }

                 NetCache_Remove(p_entry, DEF_YES);
                 p_entry                   = (NET_CACHE_ADDR     *)NetCache_AddrARP_PoolPtr;
                 NetCache_AddrARP_PoolPtr  = (NET_CACHE_ADDR_ARP *)p_entry->NextPtr;

                 pcache_addr_arp = (NET_CACHE_ADDR_ARP  *)p_entry;
             }


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                         /* ------------- VALIDATE CACHE --------------- */
             if (pcache_addr_arp->Type != NET_CACHE_TYPE_ARP) {
                 NetCache_Discard((NET_CACHE_ADDR *)pcache_addr_arp);
                 NET_CTR_ERR_INC(Net_ErrCtrs.ARP.InvTypeCtr);
                *p_err =   NET_CACHE_ERR_INVALID_TYPE;
                 return ((NET_CACHE_ADDR *)0);
             }
#endif

                                                                        /* ---------------- INIT CACHE ---------------- */
             NetCache_Clr((NET_CACHE_ADDR *)pcache_addr_arp);
             DEF_BIT_SET(pcache_addr_arp->Flags, NET_CACHE_FLAG_USED);  /* Set cache as used.                           */

                                                                        /* --------- UPDATE CACHE POOL STATS ---------- */
             NetStat_PoolEntryUsedInc(&NetCache_AddrARP_PoolStat, &err);

             pcache = (NET_CACHE_ADDR *)pcache_addr_arp;

            *p_err =  NET_CACHE_ERR_NONE;
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:                                        /* -------------- GET NDP CACHE --------------- */

             if (NetCache_AddrNDP_PoolPtr != DEF_NULL) {                /* If ARP cache pool NOT empty, ...             */
                                                                        /* ...  get cache from pool.                    */
                 pcache_addr_ndp           = NetCache_AddrNDP_PoolPtr;
                 NetCache_AddrNDP_PoolPtr  = (NET_CACHE_ADDR_NDP *)pcache_addr_ndp->NextPtr;

             } else {                                                   /* If ARP Cache List NOT empty, ...              */
                                                                        /* ... get ARP cache from list tail.             */
                 NET_CACHE_ADDR  *p_entry = NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_NDP];


                 while (p_entry != DEF_NULL) {

                     if (p_entry->AddrHW_Valid == DEF_YES) {            /* Make sure to remove only a resolved entry.    */
                         break;
                     }

                     p_entry = p_entry->PrevPtr;
                 }

                 if (p_entry == DEF_NULL) {
                     NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NoneAvailCtr);
                    *p_err =   NET_CACHE_ERR_NONE_AVAIL;
                     return (DEF_NULL);
                 }


                 NetCache_Remove(p_entry, DEF_YES);
                 pcache_addr_ndp           = NetCache_AddrNDP_PoolPtr;
                 NetCache_AddrNDP_PoolPtr  = (NET_CACHE_ADDR_NDP *)p_entry->NextPtr;

                 pcache_addr_ndp = (NET_CACHE_ADDR_NDP  *)p_entry;
             }


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                         /* ------------- VALIDATE CACHE --------------- */
             if (pcache_addr_ndp->Type != NET_CACHE_TYPE_NDP) {
                 NetCache_Discard((NET_CACHE_ADDR *)pcache_addr_ndp);
                 NET_CTR_ERR_INC(Net_ErrCtrs.NDP.InvTypeCtr);
                *p_err =   NET_CACHE_ERR_INVALID_TYPE;
                 return ((NET_CACHE_ADDR *)0);
             }
#endif

                                                                        /* ---------------- INIT CACHE ---------------- */
             NetCache_Clr((NET_CACHE_ADDR *)pcache_addr_ndp);
             DEF_BIT_SET(pcache_addr_ndp->Flags, NET_CACHE_FLAG_USED);  /* Set cache as used.                           */

                                                                        /* --------- UPDATE CACHE POOL STATS ---------- */
             NetStat_PoolEntryUsedInc(&NetCache_AddrNDP_PoolStat, &err);

             pcache       = (NET_CACHE_ADDR *)pcache_addr_ndp;
             pcache->Type = cache_type;

            *p_err   =  NET_CACHE_ERR_NONE;
             break;
#endif

        default:
            *p_err = NET_CACHE_ERR_INVALID_TYPE;
             return ((NET_CACHE_ADDR *)0);
    }

    return (pcache);
}

/*
*********************************************************************************************************
*                                         NetCache_AddrFree()
*
* Description : (1) Free a cache :
*
*                   (a) Free   cache timer
*                   (b) Free   cache buffer queue
*                   (c) Clear  cache controls
*                   (d) Free   cache back to cache pool
*                   (e) Update cache pool statistics
*
*
* Argument(s) : pcache      Pointer to a cache.
*               ------      Argument checked in NetCache_CfgAddrs(),
*                                               NetCache_Remove()
*                                                 by NetCache_AddrGet(),
*                                                    NetARP_CacheHandler(),
*                                                    NetARP_CacheReqTimeout(),
*                                                    NetARP_CacheTimeout(),
*                                                    NetNDP_CacheHandler(),
*                                                    NetNDP_CacheReqTimeout(),
*                                                    NetNDP_CacheTimeout().
*
*               tmr_free    Indicate whether to free network timer :
*
*                               DEF_YES                Free network timer for cache.
*                               DEF_NO          Do NOT free network timer for cache
*                                                     [Freed by  NetTmr_TaskHandler()
*                                                            via NetCache_Remove()].
*
* Return(s)   : none.
*
* Caller(s)   : NetCache_CfgAddrs(),
*               NetCache_Remove().
*
* Note(s)     : (2) #### To prevent freeing an cache already freed via previous cache free,
*                   NetCache_AddrFree() checks the cache's 'USED' flag BEFORE freeing the cache.
*
*                   This prevention is only best-effort since any invalid duplicate cache frees
*                   MAY be asynchronous to potentially valid cache gets.  Thus the invalid cache
*                   free(s) MAY corrupt the cache's valid operation(s).
*
*                   However, since the primary tasks of the network protocol suite are prevented from
*                   running concurrently (see 'net.h  Note #3'), it is NOT necessary to protect cache
*                   resources from possible corruption since no asynchronous access from other network
*                   tasks is possible.
*
*               (3) When a cache in the 'PENDING' state is freed, it discards its transmit packet
*                   buffer queue.  The discard is performed by the network interface layer since it is
*                   the last layer to initiate transmission for these packet buffers.
*********************************************************************************************************
*/

static  void  NetCache_AddrFree (NET_CACHE_ADDR  *pcache,
                                 CPU_BOOLEAN      tmr_free)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN              used;
#endif
#ifdef  NET_ARP_MODULE_EN
    NET_ARP_CACHE           *pcache_arp;
#endif
#ifdef  NET_NDP_MODULE_EN
    NET_NDP_NEIGHBOR_CACHE  *pcache_ndp;
#endif
    NET_CTR                 *pctr;
    NET_ERR                  err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                    /* ------------ VALIDATE ARP CACHE USED ----------- */
    used = DEF_BIT_IS_SET(pcache->Flags, NET_CACHE_FLAG_USED);
    if (used != DEF_YES) {                                          /* If ARP cache NOT used, ...                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.Cache.NotUsedCtr);
        return;                                                     /* ... rtn but do NOT free (see Note #2).           */
    }
#endif

    pcache->AddrHW_Valid            = DEF_NO;
    pcache->AddrProtocolValid       = DEF_NO;
    pcache->AddrProtocolSenderValid = DEF_NO;

    switch (pcache->Type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
                                                                        /* ------------ FREE ARP CACHE TMR ------------ */
            pcache_arp = (NET_ARP_CACHE *)NetCache_GetParent(pcache);
            if (tmr_free == DEF_YES) {
                 if (pcache_arp->TmrPtr != (NET_TMR *)0) {
                     NetTmr_Free(pcache_arp->TmrPtr);
                 }
             }

                                                                        /* ----------- FREE ARP CACHE BUF Q ----------- */
#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
             pctr = &Net_ErrCtrs.Cache.TxPktDisCtr;                     /* See Note #3.                                 */
#else
             pctr = (NET_CTR *) 0;
#endif
            (void)NetBuf_FreeBufQ_SecList(pcache->TxQ_Head,
                                          pctr,
                                         &NetCache_UnlinkBuf);

             pcache->TxQ_Nbr = 0;

                                                                        /* --------------- CLR ARP CACHE -------------- */
             pcache_arp->State          = NET_ARP_CACHE_STATE_FREE;     /* Set ARP cache as freed/NOT used.             */
             pcache_arp->ReqAttemptsCtr = 0u;
             DEF_BIT_CLR(pcache->Flags, NET_CACHE_FLAG_USED);
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
             NetCache_Clr(pcache);
#endif

                                                                        /* -------------- FREE ARP CACHE -------------- */
             pcache->NextPtr          = (NET_CACHE_ADDR     *)NetCache_AddrARP_PoolPtr;
             NetCache_AddrARP_PoolPtr = (NET_CACHE_ADDR_ARP *)pcache;

                                                                        /* ------- UPDATE ARP CACHE POOL STATS -------- */
             NetStat_PoolEntryUsedDec(&NetCache_AddrARP_PoolStat, &err);
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
                                                                        /* ------------ FREE NDP CACHE TMR ------------ */
             pcache_ndp = (NET_NDP_NEIGHBOR_CACHE *)NetCache_GetParent(pcache);
             if (tmr_free == DEF_YES) {
                 if (pcache_ndp->TmrPtr != (NET_TMR *)0) {
                     NetTmr_Free(pcache_ndp->TmrPtr);
                 }
             }

                                                                        /* ----------- FREE NDP CACHE BUF Q ----------- */
#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
             pctr = &Net_ErrCtrs.Cache.TxPktDisCtr;                     /* See Note #3.                                 */
#else
             pctr = (NET_CTR *) 0;
#endif
            (void)NetBuf_FreeBufQ_SecList((NET_BUF    *)pcache->TxQ_Head,
                                          (NET_CTR    *)pctr,
                                          (NET_BUF_FNCT)NetCache_UnlinkBuf);

            pcache->TxQ_Nbr = 0;
                                                                        /* --------------- CLR NDP CACHE -------------- */
             pcache_ndp->State = NET_NDP_CACHE_STATE_NONE;              /* Set NDP cache as freed/NOT used.             */
             pcache_ndp->ReqAttemptsCtr = 0;
             DEF_BIT_CLR(pcache->Flags, NET_CACHE_FLAG_USED);

             pcache_ndp->CacheAddrPtr->AddrProtocolPrefixLen = 0u;      /* Clr prefix len.                              */
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
             NetCache_Clr(pcache);
#endif

                                                                        /* -------------- FREE NDP CACHE -------------- */
             pcache->NextPtr          = (NET_CACHE_ADDR     *)NetCache_AddrNDP_PoolPtr;
             NetCache_AddrNDP_PoolPtr = (NET_CACHE_ADDR_NDP *)pcache;

             pcache->Type             = NET_CACHE_TYPE_NDP;
                                                                        /* ------- UPDATE NDP CACHE POOL STATS -------- */
             NetStat_PoolEntryUsedDec(&NetCache_AddrNDP_PoolStat, &err);
             break;
#endif

        default:
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
            NetCache_Discard(pcache);
#endif
            return;
    }
}

/*
*********************************************************************************************************
*                                          NetCache_Unlink()
*
* Description : Unlink a cache from the Cache List.
*
* Argument(s) : pcache      Pointer to a cache.
*               ------      Argument checked in NetCache_AddrSrch();
*                                               NetCache_Remove()
*                                                 by NetCacheGet(),
*                                                    NetARP_CacheHandler(),
*                                                    NetARP_CacheReqTimeout(),
*                                                    NetARP_CacheTimeout(),
*                                                    NetNDP_CacheHandler(),
*                                                    NetNDP_CacheReqTimeout(),
*                                                    NetNDP_CacheTimeout().
*
* Return(s)   : none.
*
* Caller(s)   : NetCache_AddrSrch(),
*               NetCache_Remove().
*
* Note(s)     : (1) Since NetCache_Unlink() called ONLY to remove & then re-link or free caches,
*                   it is NOT necessary to clear the entry's previous & next pointers.  However, pointers
*                   cleared to NULL shown for correctness & completeness.
*********************************************************************************************************
*/

static  void  NetCache_Unlink (NET_CACHE_ADDR  *pcache)
{
    NET_CACHE_ADDR  *pcache_next;
    NET_CACHE_ADDR  *pcache_prev;


    pcache_prev = pcache->PrevPtr;
    pcache_next = pcache->NextPtr;

    switch (pcache->Type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:                                /* ------- UNLINK ARP CACHE FROM ARP CACHE LIST ------- */
                                                                /* Point prev ARP cache to next ARP cache.              */
             if (pcache_prev != DEF_NULL) {
                 pcache_prev->NextPtr                              = pcache_next;
             } else {
                 NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP] = pcache_next;
             }
                                                                /* Point next ARP cache to prev ARP cache.              */
             if (pcache_next != DEF_NULL) {
                 pcache_next->PrevPtr                              = pcache_prev;
             } else {
                 NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_ARP] = pcache_prev;
             }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                     /* Clr ARP cache's ptrs (see Note #1).                  */
             pcache->PrevPtr = (NET_CACHE_ADDR *)0;
             pcache->NextPtr = (NET_CACHE_ADDR *)0;
#endif
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:                                /* ------- UNLINK NDP CACHE FROM NDP CACHE LIST ------- */
                                                                /* Point prev NDP cache to next NDP cache.              */
             if (pcache_prev != (NET_CACHE_ADDR *)0) {
                 pcache_prev->NextPtr                              = pcache_next;
             } else {
                 NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_NDP] = pcache_next;
             }
                                                                /* Point next NDP cache to prev NDP cache.              */
             if (pcache_next != (NET_CACHE_ADDR *)0) {
                 pcache_next->PrevPtr                              = pcache_prev;
             } else {
                 NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_NDP] = pcache_prev;
             }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                     /* Clr NDP cache's ptrs (see Note #1).                  */
             pcache->PrevPtr = (NET_CACHE_ADDR *)0;
             pcache->NextPtr = (NET_CACHE_ADDR *)0;
#endif
             break;
#endif

        default:
             return;
    }
}


/*
*********************************************************************************************************
*                                          NetCache_IsUsed()
*
* Description : Validate cache in use.
*
* Argument(s) : pcache      Pointer to object to validate as a cache in use.
*
* Return(s)   : DEF_YES, cache   valid &      in use.
*
*               DEF_NO,  cache invalid or NOT in use.
*
* Caller(s)   : various.
*
* Note(s)     : (1) NetCache_IsUsed() MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  CPU_BOOLEAN  NetCache_IsUsed (NET_CACHE_ADDR  *pcache)
{
    CPU_BOOLEAN     used;

                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (pcache == (NET_CACHE_ADDR *)0) {
        return (DEF_NO);
    }

                                                                /* ------------------ VALIDATE TYPE ------------------- */
    if ((pcache->Type != NET_CACHE_TYPE_ARP)  &&
        (pcache->Type != NET_CACHE_TYPE_NDP)) {
        return (DEF_NO);
    }

                                                                /* ------------- VALIDATE ARP CACHE USED -------------- */
    used = DEF_BIT_IS_SET(pcache->Flags, NET_CACHE_FLAG_USED);

    return (used);
}
#endif


/*
*********************************************************************************************************
*                                         NetCache_Discard()
*
* Description : (1) Discard an invalid/corrupted cache :
*
*                   (a) Discard cache from available cache pool                     See Note #2
*                   (b) Update  cache pool statistics
*
*               (2) Assumes cache is invalid/corrupt & MUST be removed.  Cache removed
*                   simply by NOT returning the cache back to the cache pool.
*
*
* Argument(s) : pcache      Pointer to an invalid/corrupted cache.
*
* Return(s)   : none.
*
* Caller(s)   : NetCache_AddrGet(),
*               NetCache_AddrFree().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
void  NetCache_Discard (NET_CACHE_ADDR  *pcache)
{
    NET_ERR  err;


    switch (pcache->Type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
                                                                /* ---------------- DISCARD ARP CACHE ----------------- */
             if (pcache == (NET_CACHE_ADDR *)0) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
                 return;
             }
                                                                /* --------------- UPDATE DISCARD STATS --------------- */
             NetStat_PoolEntryLostInc(&NetCache_AddrARP_PoolStat, &err);
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
                                                                /* ---------------- DISCARD NDP CACHE ----------------- */
             if (pcache == (NET_CACHE_ADDR *)0) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.NDP.NullPtrCtr);
                 return;
             }
                                                                /* --------------- UPDATE DISCARD STATS --------------- */
             NetStat_PoolEntryLostInc(&NetCache_AddrNDP_PoolStat, &err);
             break;
#endif

        default:
             return;
    }

   (void)&pcache;
}
#endif


/*
*********************************************************************************************************
*                                           NetCache_Clr()
*
* Description : Clear cache controls.
*
* Argument(s) : pcache      Pointer to a cache.
*               ------      Argument validated in NetCache_AddrGet(),
*                                                 NetCache_AddrFree(),
*                                                 NetARP_Init(),
*                                                 NetNDP_Init().
*
* Return(s)   : none.
*
* Caller(s)   : NetCache_AddrGet(),
*               NetCache_AddrFree(),
*               NetARP_Init(),
*               NetNDP_Init().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetCache_Clr (NET_CACHE_ADDR  *pcache)
{

#ifdef  NET_ARP_MODULE_EN
    NET_CACHE_ADDR_ARP  *pcache_addr_arp;
#endif
#ifdef  NET_NDP_MODULE_EN
    NET_CACHE_ADDR_NDP  *pcache_addr_ndp;
#endif


    pcache->PrevPtr                 = (NET_CACHE_ADDR *)0;
    pcache->NextPtr                 = (NET_CACHE_ADDR *)0;
    pcache->TxQ_Head                = (NET_BUF *)0;
    pcache->TxQ_Tail                = (NET_BUF *)0;
    pcache->TxQ_Nbr                 =  0;
    pcache->IF_Nbr                  =  NET_IF_NBR_NONE;
    pcache->AccessedCtr             =  0u;
    pcache->Flags                   =  NET_CACHE_FLAG_NONE;

    pcache->AddrHW_Valid            = DEF_NO;
    pcache->AddrProtocolValid       = DEF_NO;
    pcache->AddrProtocolSenderValid = DEF_NO;


    switch (pcache->Type) {
#ifdef  NET_ARP_MODULE_EN
        case NET_CACHE_TYPE_ARP:
             pcache_addr_arp = (NET_CACHE_ADDR_ARP *)pcache;

             Mem_Clr((void     *)&pcache_addr_arp->AddrHW[0],
                     (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
             Mem_Clr((void     *)&pcache_addr_arp->AddrProtocol[0],
                     (CPU_SIZE_T) NET_IPv4_ADDR_SIZE);
             Mem_Clr((void     *)&pcache_addr_arp->AddrProtocolSender[0],
                     (CPU_SIZE_T) NET_IPv4_ADDR_SIZE);
             break;
#endif

#ifdef  NET_NDP_MODULE_EN
        case NET_CACHE_TYPE_NDP:
             pcache_addr_ndp = (NET_CACHE_ADDR_NDP *)pcache;

             Mem_Clr((void     *)&pcache_addr_ndp->AddrHW[0],
                     (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
             Mem_Clr((void     *)&pcache_addr_ndp->AddrProtocol[0],
                     (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);
             Mem_Clr((void     *)&pcache_addr_ndp->AddrProtocolSender[0],
                     (CPU_SIZE_T) NET_IPv6_ADDR_SIZE);
             break;
#endif

        default:
             return;
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_CACHE_MODULE_EN */

