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
*                                          NETWORK ARP LAYER
*                                    (ADDRESS RESOLUTION PROTOCOL)
*
* Filename : net_arp.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Address Resolution Protocol ONLY required for network interfaces that require
*                network-address-to-hardware-address bindings (see RFC #826 'Abstract').
*
*            (2) Supports Address Resolution Protocol as described in RFC #826 with the following
*                restrictions/constraints :
*
*                (a) ONLY supports the following hardware types :
*                    (1) 48-bit Ethernet
*
*                (b) ONLY supports the following protocol types :
*                    (1) 32-bit IP
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    NET_ARP_MODULE

#include  "net_arp.h"
#include  "../../IF/net_if.h"
#include  "../../Source/net.h"
#include  "../../Source/net_util.h"
#include  "../../Source/net_mgr.h"
#include  "../../Source/net_ctr.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'net_arp.h  MODULE'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_ARP_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_ARP_HW_TYPE_NONE                         0x0000u
#define  NET_ARP_PROTOCOL_TYPE_IP_V4                  0x0800u
#define  NET_ARP_HW_TYPE_ETHER                        0x0001u


/*
*********************************************************************************************************
*                                          ARP CACHE DEFINES
*
* Note(s) : (1) (a) RFC #1122, Section 2.3.2.2 states that "the link layer SHOULD save (rather than
*                   discard) at least one ... packet of each set of packets destined to the same
*                   unresolved IP address, and transmit the saved packet when the address has been
*                   resolved."
*
*               (b) However, in order to avoid excessive discards, it seems reasonable that at least
*                   two transmit packet buffers should be queued to a pending ARP cache.
*********************************************************************************************************
*/
#define  NET_ARP_CACHE_TIMEOUT_MIN_SEC           ( 1  *  DEF_TIME_NBR_SEC_PER_MIN)  /* Cache timeout min  =  1 min      */
#define  NET_ARP_CACHE_TIMEOUT_MAX_SEC           (10  *  DEF_TIME_NBR_SEC_PER_MIN)  /* Cache timeout max  = 10 min      */

#define  NET_ARP_CACHE_TX_Q_TH_MIN                         0
#define  NET_ARP_CACHE_TX_Q_TH_MAX                       NET_BUF_NBR_MAX

#define  NET_ARP_RENEW_REQ_RETRY_DFLT                      15


/*
*********************************************************************************************************
*                                    ARP HEADER / MESSAGE DEFINES
*
* Note(s) : (1) See RFC #826, Section 'Packet Format' for ARP packet header format.
*
*               (a) ARP header includes two pairs of hardware & protocol type addresses--one each for the
*                   sender & the target.
*********************************************************************************************************
*/

#define  NET_ARP_HDR_OP_REQ                           0x0001u
#define  NET_ARP_HDR_OP_REPLY                         0x0002u


#define  NET_ARP_MSG_LEN                                 NET_ARP_HDR_SIZE
#define  NET_ARP_MSG_LEN_DATA                              0



/*
*********************************************************************************************************
*                                         ARP REQUEST DEFINES
*
* Note(s) : (1) RFC #1122, Section 2.3.2.1 states that "a mechanism to prevent ARP flooding (repeatedly
*               sending an ARP Request for the same IP address, at a high rate) MUST be included.  The
*               recommended maximum rate is 1 per second per destination".
*********************************************************************************************************
*/

#define  NET_ARP_REQ_RETRY_MIN                             0
#define  NET_ARP_REQ_RETRY_MAX                             5
                                                                /* ARP req retry timeouts (see Note #1).                */
#define  NET_ARP_REQ_RETRY_TIMEOUT_MIN_SEC                 1    /* ARP req retry timeout min  =  1 sec                  */
#define  NET_ARP_REQ_RETRY_TIMEOUT_MAX_SEC                10    /* ARP req retry timeout max  = 10 sec                  */


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

static  CPU_BOOLEAN     NetARP_AddrFltrEn;

NET_ARP_CACHE   NetARP_CacheTbl[NET_ARP_CFG_CACHE_NBR];

NET_ARP_CACHE  *NetARP_CacheListHead;                      /* Ptr to head of ARP Cache List.               */
NET_ARP_CACHE  *NetARP_CacheListTail;                      /* Ptr to tail of ARP Cache List.               */

#if 0
CPU_INT16U      NetARP_CacheTimeout_sec;                   /* ARP cache timeout              (in secs ).   */
NET_TMR_TICK    NetARP_CacheTimeout_tick;                  /* ARP cache timeout              (in ticks).   */
#endif

NET_BUF_QTY     NetARP_CacheTxQ_MaxTh_nbr;                 /* Max nbr tx bufs to enqueue    on ARP cache.  */


CPU_INT08U      NetARP_ReqMaxAttemptsPend_nbr;     /* ARP req max nbr of attempts in pend  state.          */
CPU_INT08U      NetARP_ReqTimeoutPend_sec;         /* ARP req wait-for-reply timeout (in secs ).           */
NET_TMR_TICK    NetARP_ReqTimeoutPend_tick;        /* ARP req wait-for-reply timeout (in ticks).           */

CPU_INT08U      NetARP_ReqMaxAttemptsRenew_nbr;    /* ARP req max nbr of attempts in renew state.          */
CPU_INT08U      NetARP_ReqTimeoutRenew_sec;        /* ARP req wait-for-reply timeout (in secs ).           */
NET_TMR_TICK    NetARP_ReqTimeoutRenew_tick;       /* ARP req wait-for-reply timeout (in ticks).           */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                            /* --------------- RX FNCTS --------------- */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void            NetARP_RxPktValidateBuf     (NET_BUF_HDR    *p_buf_hdr,
                                                     NET_ERR        *p_err);
#endif

static  void            NetARP_RxPktValidate        (NET_BUF_HDR    *p_buf_hdr,
                                                     NET_ARP_HDR    *p_arp_hdr,
                                                     NET_ERR        *p_err);


static  void            NetARP_RxPktCacheUpdate     (NET_IF_NBR      if_nbr,
                                                     NET_ARP_HDR    *p_arp_hdr,
                                                     NET_ERR        *p_err);


static  void            NetARP_RxPktReply           (NET_IF_NBR      if_nbr,
                                                     NET_ARP_HDR    *p_arp_hdr,
                                                     NET_ERR        *p_err);


static  void            NetARP_RxPktIsTargetThisHost(NET_IF_NBR      if_nbr,
                                                     NET_ARP_HDR    *p_arp_hdr,
                                                     NET_ERR        *p_err);


static  void            NetARP_RxPktFree            (NET_BUF        *p_buf);

static  void            NetARP_RxPktDiscard         (NET_BUF        *p_buf,
                                                     NET_ERR        *p_err);


                                                                            /* --------------- TX FNCTS --------------- */

static  void            NetARP_Tx                   (NET_IF_NBR      if_nbr,
                                                     CPU_INT08U     *p_addr_hw_sender,
                                                     CPU_INT08U     *p_addr_hw_target,
                                                     CPU_INT08U     *p_addr_protocol_sender,
                                                     CPU_INT08U     *p_addr_protocol_target,
                                                     CPU_INT16U      op_code,
                                                     NET_ERR        *p_err);


static  void            NetARP_TxReply              (NET_IF_NBR      if_nbr,
                                                     NET_ARP_HDR    *p_arp_hdr);


static  void            NetARP_TxPktPrepareHdr      (NET_BUF        *p_buf,
                                                     CPU_INT16U      msg_ix,
                                                     CPU_INT08U     *p_addr_hw_sender,
                                                     CPU_INT08U     *p_addr_hw_target,
                                                     CPU_INT08U     *p_addr_protocol_sender,
                                                     CPU_INT08U     *p_addr_protocol_target,
                                                     CPU_INT16U      op_code,
                                                     NET_ERR        *p_err);

static  void            NetARP_TxIxDataGet          (NET_IF_NBR      if_nbr,
                                                     CPU_INT32U      data_len,
                                                     CPU_INT16U     *p_ix,
                                                     NET_ERR        *p_err);

static  void            NetARP_TxPktFree            (NET_BUF        *p_buf);

static  void            NetARP_TxPktDiscard         (NET_BUF        *p_buf,
                                                     NET_ERR        *p_err);


                                                                            /* ----------- ARP CACHE FNCTS ------------ */

static  void            NetARP_CacheReqTimeout      (void           *p_cache_timeout);

static  void            NetARP_CacheRenewTimeout    (void           *p_cache_timeout);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                              FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             NetARP_Init()
*
* Description : (1) Initialize Address Resolution Protocol Layer :
*
*                   (a) Initialize ARP cache pool
*                   (b) Initialize ARP cache table
*                   (c) Initialize ARP cache list pointers
*
*
* Argument(s) : p_err    Pointer to variable that will receive the return error code from this function :
*
*                           NET_ARP_ERR_NONE    ARP module successfully initialized.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Init().
*
*               This function is a network protocol suite to network interface (IF) function & SHOULD be
*               called only by appropriate network interface initialization function(s).
*
* Note(s)     : (2) ARP cache pool MUST be initialized PRIOR to initializing the pool with pointers to
*                   ARP caches.
*********************************************************************************************************
*/

void  NetARP_Init (NET_ERR  *p_err)
{
    NET_ARP_CACHE      *p_cache;
    NET_ARP_CACHE_QTY   i;
    NET_ERR             net_err;


                                                                /* ------------ INIT ARP CACHE POOL/STATS ------------- */
    NetCache_AddrARP_PoolPtr = (NET_CACHE_ADDR_ARP *)0;         /* Init-clr ARP cache pool (see Note #2).               */

    NetStat_PoolInit((NET_STAT_POOL   *)&NetCache_AddrARP_PoolStat,
                     (NET_STAT_POOL_QTY) NET_ARP_CFG_CACHE_NBR,
                     (NET_ERR         *)&net_err);


                                                                /* ---------------- INIT ARP CACHE TBL ---------------- */
    p_cache = &NetARP_CacheTbl[0];
    for (i = 0u; i < NET_ARP_CFG_CACHE_NBR; i++) {
                                                                /* Init each ARP      cache type/addr len--NEVER modify.*/
        p_cache->Type           = NET_CACHE_TYPE_ARP;
        p_cache->CacheAddrPtr   = &NetCache_AddrARP_Tbl[i];     /* Init each ARP addr cache ptr.                        */

        p_cache->ReqAttemptsCtr = 0u;

        p_cache->State          =  NET_ARP_CACHE_STATE_FREE;    /* Init each ARP      cache as free/NOT used.           */
        p_cache->Flags          =  NET_CACHE_FLAG_NONE;

        NetCache_Init((NET_CACHE_ADDR *) p_cache,               /* Init each ARP addr cache.                            */
                      (NET_CACHE_ADDR *) p_cache->CacheAddrPtr,
                                        &net_err);

        if (net_err != NET_CACHE_ERR_NONE) {
            *p_err = net_err;
             return;
        }

        p_cache++;
    }

    NetARP_AddrFltrEn = DEF_ENABLED;
    NetARP_ReqMaxAttemptsRenew_nbr = NET_ARP_RENEW_REQ_RETRY_DFLT + 1u;

                                                                /* ------------- INIT ARP CACHE LIST PTRS ------------- */
    NetCache_AddrListHead[NET_CACHE_ADDR_LIST_IX_ARP] = (NET_CACHE_ADDR *)0;
    NetCache_AddrListTail[NET_CACHE_ADDR_LIST_IX_ARP] = (NET_CACHE_ADDR *)0;

   *p_err = NET_ARP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetARP_CfgAddrFilterEn()
*
* Description : Configure ARP address filter feature.
*
* Argument(s) : en  Set Filter enabled or disabled:
*
*                       DEF_ENABLED     Enable  filtering feature.
*                       DEF_DISABLED    Disable filtering feature.
*
* Return(s)   : None.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  NetARP_CfgAddrFilterEn (CPU_BOOLEAN  en)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    NetARP_AddrFltrEn = en;
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                       NetARP_CfgCacheTimeout()
*
* Description : Configure ARP cache timeout from ARP Cache List.
*
* Argument(s) : timeout_sec     Desired value for ARP cache timeout (in seconds).
*
* Return(s)   : DEF_OK,   ARP cache timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) RFC #1122, Section 2.3.2.1 states that "an implementation of the Address Resolution
*                   Protocol (ARP) ... MUST provide a mechanism to flush out-of-date cache entries.  If
*                   this mechanism involves a timeout, it SHOULD be possible to configure the timeout
*                   value".
*
*               (2) Configured timeout does NOT reschedule any current ARP cache timeout in progress but
*                   becomes effective the next time an ARP cache sets its timeout.
*
*               (3) Configured timeout converted to 'NET_TMR_TICK' ticks to avoid run-time conversion.
*
*               (4) 'NetARP_CacheTimeout' variables MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetARP_CfgCacheTimeout (CPU_INT16U  timeout_sec)
{
    NET_TMR_TICK  timeout_tick;
    CPU_SR_ALLOC();


#if (NET_ARP_CACHE_TIMEOUT_MIN_SEC > DEF_INT_16U_MIN_VAL)
    if (timeout_sec < NET_ARP_CACHE_TIMEOUT_MIN_SEC) {
        return (DEF_FAIL);
    }
#endif
#if (NET_ARP_CACHE_TIMEOUT_MAX_SEC < DEF_INT_16U_MAX_VAL)
    if (timeout_sec > NET_ARP_CACHE_TIMEOUT_MAX_SEC) {
        return (DEF_FAIL);
    }
#endif

    timeout_tick             = (NET_TMR_TICK)timeout_sec * NET_TMR_TIME_TICK_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetARP_ReqTimeoutRenew_sec  = timeout_sec;
    NetARP_ReqTimeoutRenew_tick = timeout_tick;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      NetARP_CfgCacheTxQ_MaxTh()
*
* Description : Configure ARP cache maximum number of transmit packet buffers to enqueue.
*
* Argument(s) : nbr_buf_max     Desired maximum number of transmit packet buffers to enqueue onto an
*
* Return(s)   : DEF_OK,   ARP cache transmit packet buffer threshold configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) 'NetARP_CacheTxQ_MaxTh_nbr' MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetARP_CfgCacheTxQ_MaxTh (NET_BUF_QTY  nbr_buf_max)
{
    CPU_SR_ALLOC();


#if (NET_ARP_CACHE_TX_Q_TH_MIN > DEF_INT_16U_MIN_VAL)
    if (nbr_buf_max < NET_ARP_CACHE_TX_Q_TH_MIN) {
        return (DEF_FAIL);
    }
#endif
#if (NET_ARP_CACHE_TX_Q_TH_MAX < DEF_INT_16U_MAX_VAL)
    if (nbr_buf_max > NET_ARP_CACHE_TX_Q_TH_MAX) {
        return (DEF_FAIL);
    }
#endif

    CPU_CRITICAL_ENTER();
    NetARP_CacheTxQ_MaxTh_nbr = nbr_buf_max;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      NetARP_CfgCacheAccessedTh()
*
* Description : Configure ARP cache access promotion threshold.
*
* Argument(s) : nbr_access  Desired number of ARP cache accesses before ARP cache is promoted.
*
* Return(s)   : DEF_OK,   ARP cache access promotion threshold configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) 'NetARP_CacheAccessedTh_nbr' MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetARP_CfgCacheAccessedTh (CPU_INT16U  nbr_access)
{
    CPU_SR_ALLOC();


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

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        NetARP_CfgReqTimeout()
*
* Description : Configure timeout between ARP Request retries.
*
* Argument(s) : timeout_sec     Desired value for ARP Request pending ARP Reply timeout (in seconds).
*
* Return(s)   : DEF_OK,   ARP Request timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Configured timeout does NOT reschedule any current ARP Request timeout in progress but
*                   becomes effective the next time an ARP Request is transmitted with timeout.
*
*               (2) Configured timeout converted to 'NET_TMR_TICK' ticks to avoid run-time conversion.
*
*               (3) 'NetARP_ReqTimeout' variables MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetARP_CfgReqTimeout (CPU_INT08U  timeout_sec)
{
    NET_TMR_TICK  timeout_tick;
    CPU_SR_ALLOC();


    if (timeout_sec < NET_ARP_REQ_RETRY_TIMEOUT_MIN_SEC) {
        return (DEF_FAIL);
    }
    if (timeout_sec > NET_ARP_REQ_RETRY_TIMEOUT_MAX_SEC) {
        return (DEF_FAIL);
    }

    timeout_tick           = (NET_TMR_TICK)timeout_sec * NET_TMR_TIME_TICK_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetARP_ReqTimeoutPend_sec  = timeout_sec;
    NetARP_ReqTimeoutPend_tick = timeout_tick;
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       NetARP_CfgReqMaxRetries()
*
* Description : Configure ARP Request maximum number of requests.
*
* Argument(s) : max_nbr_retries     Desired maximum number of ARP Request attempts.
*
* Return(s)   : DEF_OK,   ARP Request maximum number of request attempts configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) An ARP cache monitors the number of ARP Requests transmitted before receiving an ARP
*                   Reply.  In other words, an ARP cache monitors the number of ARP Request attempts.
*
*                   However, the maximum number of ARP Requests that each ARP cache is allowed to transmit
*                   is configured in terms of retries.  Thus the total number of attempts is equal to the
*                   configured number of retries plus one (1).
*
*               (2) 'NetARP_ReqMaxAttemptsPend_nbr' MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetARP_CfgReqMaxRetries (CPU_INT08U  max_nbr_retries)
{
    CPU_SR_ALLOC();


#if (NET_ARP_REQ_RETRY_MIN > DEF_INT_08U_MIN_VAL)
    if (max_nbr_retries < NET_ARP_REQ_RETRY_MIN) {
        return (DEF_FAIL);
    }
#endif
#if (NET_ARP_REQ_RETRY_MAX < DEF_INT_08U_MAX_VAL)
    if (max_nbr_retries > NET_ARP_REQ_RETRY_MAX) {
        return (DEF_FAIL);
    }
#endif

    CPU_CRITICAL_ENTER();
    NetARP_ReqMaxAttemptsPend_nbr = max_nbr_retries + 1u;       /* Set max attempts as max retries + 1 (see Note #1).       */
    CPU_CRITICAL_EXIT();

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                     NetARP_CacheProbeAddrOnNet()
*
* Description : (1) Transmit an ARP Request to probe the local network for a specific protocol address :
*
*                   (a) Acquire network lock
*                   (b) Remove ARP cache with desired protocol address from ARP Cache List, if available
*                   (c) Configure ARP cache :
*                       (1) Get default-configured ARP cache
*                       (2) ARP cache state
*                   (d) Transmit ARP Request to probe local network for desired protocol address
*                   (e) Release network lock
*
*
*               (2) NetARP_CacheProbeAddrOnNet() SHOULD be used in conjunction with NetARP_CacheGetAddrHW()
*                   to determine if a specific protocol address is available on the local network :
*
*                   (a) After successfully transmitting an ARP Request to probe the local network &  ...
*                   (b) After some time delay(s) [on the order of ARP Request timeouts & retries];   ...
*                   (c) Check ARP Cache for the hardware address of a host on the local network that
*                           corresponds to the desired protocol address.
*
*                   See also 'NetARP_CacheGetAddrHW()   Note #1'
*                          & 'NetARP_TxReqGratuitous()  Note #2'.
*
*
* Argument(s) : protocol_type           Address protocol type.
*
*               p_addr_protocol_sender  Pointer to protocol address to send probe from     (see Note #5a1).
*
*               p_addr_protocol_target  Pointer to protocol address to probe local network (see Note #5a2).
*
*               addr_protocol_len       Length  of protocol address (in octets)            [see Note #5b].
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                           pointer.
*                               NET_ARP_ERR_NONE                    ARP Request successfully transmitted to
*                                                                       probe local network for the desired
*                                                                       protocol address (see Note #2).
*                               NET_ERR_FAULT_NULL_PTR                Argument 'p_addr_protocol' passed a NULL
*                                                                      pointer.
*                               NET_ARP_ERR_INVALID_PROTOCOL_LEN    Invalid ARP protocol address length.*
*                               NET_INIT_ERR_NOT_COMPLETED          Network initialization NOT complete.
*
*                                                                   - RETURNED BY NetMgr_GetHostAddrProtocolIF_Nbr() : -
*                               NET_MGR_ERR_INVALID_PROTOCOL        Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_ADDR   Invalid protocol address NOT used by host.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN    Invalid protocol address length.
*
*                                                                   -------- RETURNED BY NetCache_CfgAddrs() : ---------
*                               NET_ARP_ERR_CACHE_NONE_AVAIL        NO available ARP caches to allocate.
*                               NET_ARP_ERR_CACHE_INVALID_TYPE      ARP cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT               Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL              NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE            Network timer is NOT a valid timer type.
*                               NET_ERR_FAULT_NULL_PTR              Variable is set to Null pointer.
*
*                                                                   ------ RETURNED BY Net_GlobalLockAcquire() : -------
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #3].
*
* Note(s)     : (3) NetARP_CacheProbeAddrOnNet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (4) NetARP_CacheProbeAddrOnNet() blocked until network initialization completes.
*
*               (5) (a) (1) 'p_addr_protocol_sender' MUST point to a valid protocol address in network-order,
*                            configured on a valid interface.
*
*                       (2) 'p_addr_protocol_target' MUST point to a valid protocol address in network-order.
*
*                           See also 'NetARP_CacheHandler()  Note #2e3'.
*
*                   (b) The length of the protocol address MUST be equal to NET_IPv4_ADDR_SIZE
*                       & is included for correctness & completeness.
*********************************************************************************************************
*/
void  NetARP_CacheProbeAddrOnNet (NET_PROTOCOL_TYPE     protocol_type,
                                  CPU_INT08U           *p_addr_protocol_sender,
                                  CPU_INT08U           *p_addr_protocol_target,
                                  NET_CACHE_ADDR_LEN    addr_protocol_len,
                                  NET_ERR              *p_err)
{
    NET_IF_NBR           if_nbr;
    NET_CACHE_ADDR_ARP  *p_cache_addr_arp;
    NET_ARP_CACHE       *p_cache_arp;
    NET_TMR_TICK         timeout_tick;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ------------- VALIDATE PROTOCOL ADDRS -------------- */
    if (p_addr_protocol_sender == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_addr_protocol_target == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                /* Chk protocol addr len (see Note #5b).                */
    if (addr_protocol_len != NET_IPv4_ADDR_SIZE) {
       *p_err = NET_ARP_ERR_INVALID_PROTOCOL_LEN;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #3b.                                        */
    Net_GlobalLockAcquire((void *)&NetARP_CacheProbeAddrOnNet, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #4).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif


                                                                /* -------------------- GET IF NBR -------------------- */
    if_nbr = NetMgr_GetHostAddrProtocolIF_Nbr(protocol_type,
                                              p_addr_protocol_sender,
                                              addr_protocol_len,
                                              p_err);
    if (*p_err != NET_MGR_ERR_NONE) {
         goto exit_release;
    }


                                                                /* --------- REMOVE PROTOCOL ADDR'S ARP CACHE --------- */
    p_cache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_AddrSrch(NET_CACHE_TYPE_ARP,
                                                               if_nbr,
                                                               p_addr_protocol_target,
                                                               NET_IPv4_ADDR_SIZE,
                                                               p_err);
    if (p_cache_addr_arp != (NET_CACHE_ADDR_ARP *)0) {          /* If protocol addr's ARP cache avail, ...              */
        NetCache_Remove((NET_CACHE_ADDR *)p_cache_addr_arp,     /* ... remove from ARP Cache List (see Note #1b).       */
                                          DEF_YES);
    }


                                                                /* ------------------ CFG ARP CACHE ------------------- */
    CPU_CRITICAL_ENTER();
    timeout_tick = NetARP_ReqTimeoutPend_tick;
    CPU_CRITICAL_EXIT();

    p_cache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_CfgAddrs(NET_CACHE_TYPE_ARP,
                                                               if_nbr,
                                                               0,
                                                               NET_IF_HW_ADDR_LEN_MAX,
                                                               p_addr_protocol_target,
                                                               p_addr_protocol_sender,
                                                               NET_IPv4_ADDR_SIZE,
                                                               DEF_YES,
                                                               NetARP_CacheReqTimeout,
                                                               timeout_tick,
                                                               p_err);
    if ((*p_err            != NET_CACHE_ERR_NONE) ||
        ( p_cache_addr_arp == (NET_CACHE_ADDR_ARP *)0)) {
         goto exit_release;
    }

    p_cache_arp = (NET_ARP_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_arp);
    if (p_cache_arp == ((NET_ARP_CACHE *)0)) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit_release;
    }

    p_cache_arp->State = NET_ARP_CACHE_STATE_PEND;

                                                                /* ------- INSERT ARP CACHE INTO ARP CACHE LIST ------- */
    NetCache_Insert((NET_CACHE_ADDR *)p_cache_addr_arp);

                                                                /* -------------------- TX ARP REQ -------------------- */

    NetARP_TxReq(p_cache_addr_arp);                             /* Tx ARP req to resolve ARP cache.                     */

   *p_err = NET_ARP_ERR_NONE;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                    NetARP_IsAddrProtocolConflict()
*
* Description : Get interface's protocol address conflict status between this interface's ARP host
*                   protocol address(s) & other host(s) on the local network.
*
* Argument(s) : if_nbr      Interface number to get protocol address conflict status.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               NET_ARP_ERR_NONE                Protocol address conflict status
*                                                                   successfully returned.
*
*                               NET_INIT_ERR_NOT_COMPLETED      Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIF_IsValidHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               - RETURNED BY Net_GlobalLockAcquire() : -
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, if address conflict detected.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetARP_IsAddrProtocolConflict().
*
*               This function is an application interface (API) function & MAY be called by application
*               function(s).
*
* Note(s)     : (1) NetARP_IsAddrProtocolConflict() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetARP_IsAddrProtocolConflict() blocked until network initialization completes.
*
*               (3) RFC #3927, Section 2.5 states that :
*
*                   (a) "If a host receives an ARP packet (request *or* reply) on an interface where" ...
*                       (1) "the 'sender hardware address' does not match the hardware address of
*                                 that interface, but"                                                ...
*                       (2) "the 'sender IP       address' is a IP address the host has configured for
*                                 that interface,"                                                    ...
*                   (b) "then this is a conflicting ARP packet, indicating an address conflict."
*********************************************************************************************************
*/

CPU_BOOLEAN  NetARP_IsAddrProtocolConflict (NET_IF_NBR   if_nbr,
                                            NET_ERR     *p_err)
{
    CPU_BOOLEAN  addr_conflict;

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetARP_IsAddrProtocolConflict, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (DEF_NO);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        addr_conflict = DEF_NO;
        goto exit;
    }
#endif

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         addr_conflict = DEF_NO;
         goto exit;
    }

                                                                /* ------------ CHK PROTOCOL ADDR CONFLICT ------------ */
    addr_conflict = NetMgr_IsAddrProtocolConflict(if_nbr);

   *p_err =  NET_ARP_ERR_NONE;


exit:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (addr_conflict);
}


/*
*********************************************************************************************************
*                                         NetARP_CacheHandler()
*
* Description : (1) Resolve destination hardware address using ARP for transmit data packet :
*
*                   (a) Search ARP Cache List for ARP cache with corresponding protocol address
*                   (b) If ARP cache     found, handle packet based on ARP cache state :
*
*                       (1) PENDING  -> Enqueue transmit packet buffer to ARP cache
*                       (2) RESOLVED -> Copy ARP cache's hardware address to data packet;
*                                       Return to Network Interface to transmit data packet
*
*                   (c) If ARP cache NOT found, allocate new ARP cache in 'PENDING' state (see Note #1b1)
*
*                   See 'net_cache.h  CACHE STATES' for cache state diagram.
*
*               (2) This ARP cache handler function assumes the following :
*
*                   (a) ALL ARP caches in the ARP Cache List are valid [validated by NetCache_AddrGet()]
*                   (b) ANY ARP cache in the 'PENDING' state MAY have already enqueued at LEAST one
*                           transmit packet buffer when ARP cache allocated [see NetCache_AddrGet()]
*                   (c) ALL ARP caches in the 'RESOLVED' state have valid hardware addresses
*                   (d) ALL transmit buffers enqueued on any ARP cache are valid
*                   (e) Buffer's ARP address pointers pre-configured by Network Interface to point to :
*
*                       (1) 'ARP_AddrProtocolPtr'               Pointer to the protocol address used to
*                                                                   resolve the hardware address
*                       (2) 'ARP_AddrHW_Ptr'                    Pointer to memory buffer to return the
*                                                                   resolved hardware address
*                       (3) ARP addresses                       Which MUST be in network-order
*
*
* Argument(s) : p_buf   Pointer to network buffer to transmit.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*                           NET_ARP_ERR_CACHE_RESOLVED          ARP cache resolved & hardware address
*                                                                   successfully copied.
*                           NET_ARP_ERR_CACHE_PEND              ARP cache in 'PENDING' state; transmit
*                                                                   buffer enqueued to ARP cache.
*                           NET_ERR_FAULT_NULL_PTR                Argument 'p_buf' passed a NULL pointer;
*                                                                   OR
*                                                               'p_buf's 'ARP_AddrProtocolPtr'/'ARP_AddrHWPtr'
*                                                                   are set as NULL pointers.
*                           NET_BUF_ERR_NONE_AVAIL              ARP buffer threshold is greater than number
*                                                                   of configured transmit buffers for current
*                                                                   interface.
*
*                           ----------------------- RETURNED BY NetIF_Get() -----------------------
*                           See NetIF_Get() for additional return error codes.
*
*                           ------------ RETURNED BY NetIF_AddrMulticastProtocolToHW() ------------
*                           See NetIF_AddrMulticastProtocolToHW() for additional return error codes.
*
*                           ------------------ RETURNED BY NetARP_CacheAddPend() ------------------
*                           See NetARP_CacheAddPend() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_Tx().
*
*               This function is a board-support package function & SHOULD be called only by
*               appropriate product function(s).
*
* Note(s)     : (3) (a) ARP Cache List is accessed by
*
*                       (1) NetCache_AddResolved()                  via NetCache_Insert()
*                       (2) NetCache_Remove()                       via NetCache_Unlink()
*                       (3) NetARP_CacheHandler()
*                       (4) NetARP_CacheAddPend()                   via NetCache_Insert()
*                       (5) NetARP_RxPktCacheUpdate()
*                       (6) ARP cache's 'TMR->Obj' pointer          via NetARP_CacheReqTimeout() &
*                                                                       NetARP_CacheTimeout()
*
*                   (b) Since the primary tasks of the network protocol suite are prevented from running
*                       concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                       resources of the ARP Cache List since no asynchronous access from other network
*                       tasks is possible.
*
*               (4) (a) RFC #1122, Section 2.3.2.2 states that "the link layer SHOULD" :
*
*                       (1) "Save (rather than discard) ... packets destined to the same unresolved
*                            IP address and" ...
*                       (2) "Transmit the saved packet[s] when the address has been resolved."
*
*                   (b) Since ARP Layer is the last layer to handle & queue the transmit network
*                       buffer, it is NOT necessary to increment the network buffer's reference
*                       counter to include the pending ARP cache buffer queue as a new reference
*                       to the network buffer.
*
*               (5) Some buffer controls were previously initialized in NetBuf_Get() when the packet
*                   was received at the network interface layer.  These buffer controls do NOT need
*                   to be re-initialized but are shown for completeness.
*
*               (6) A resolved multicast address still remains resolved even if any error(s) occur
*                   while adding it to the ARP cache.
*********************************************************************************************************
*/

void  NetARP_CacheHandler (NET_BUF  *p_buf,
                           NET_ERR  *p_err)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_PROTOCOL_TYPE    protocol_type_net;
    CPU_BOOLEAN          addr_protocol_multicast;
    CPU_INT08U           addr_hw_len;
    NET_ERR              err;
#endif
    NET_IF_NBR           if_nbr;
    NET_IF              *p_if;
    NET_DEV_CFG         *p_dev_cfg;
    CPU_INT08U          *p_addr_hw;
    CPU_INT08U          *p_addr_protocol;
    NET_BUF_HDR         *p_buf_hdr;
    NET_BUF_HDR         *ptail_buf_hdr;
    NET_BUF             *ptail_buf;
    NET_ARP_CACHE       *p_cache;
    NET_CACHE_ADDR_ARP  *p_cache_addr_arp;
    NET_BUF_QTY          buf_max_th;
    CPU_SR_ALLOC();


                                                                /* ------------------ VALIDATE PTRS ------------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    p_buf_hdr       = &p_buf->Hdr;
    p_addr_hw       =  p_buf_hdr->ARP_AddrHW_Ptr;
    p_addr_protocol =  p_buf_hdr->ARP_AddrProtocolPtr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_hw == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_addr_protocol == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    if_nbr = p_buf_hdr->IF_Nbr;                                 /* Obtain current interface number.                     */
    p_if   = NetIF_Get(if_nbr, p_err);                          /* Obtain pointer to current interface.                 */
    if (*p_err != NET_IF_ERR_NONE) {
        return;
    }

    p_dev_cfg = (NET_DEV_CFG *)p_if->Dev_Cfg;                   /* Obtain pointer to current interface device config.   */

                                                                /* ------------------ SRCH ARP CACHE ------------------ */
    p_cache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_AddrSrch(NET_CACHE_TYPE_ARP,
                                                              if_nbr,
                                                              p_addr_protocol,
                                                              NET_IPv4_ADDR_SIZE,
                                                              p_err);


    if (p_cache_addr_arp != DEF_NULL) {                         /* If ARP cache found, chk state.                       */
        p_cache = (NET_ARP_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_arp);
        if (p_cache == DEF_NULL) {
            return;
        }

        switch (p_cache->State) {
            case NET_ARP_CACHE_STATE_PEND:                      /* If ARP cache pend, append buf into Q (see Note #4a1).*/
                 CPU_CRITICAL_ENTER();
                 buf_max_th = NetARP_CacheTxQ_MaxTh_nbr;
                 CPU_CRITICAL_EXIT();
                                                                /* Make sure ARP buffer threshold doesn't go beyond ... */
                                                                /* ...the number of configured buffers for current IF.  */
                 if (buf_max_th > (p_dev_cfg->TxBufLargeNbr + p_dev_cfg->TxBufSmallNbr)) {
                    *p_err = NET_BUF_ERR_NONE_AVAIL;
                     return;
                 }

                 if (p_cache_addr_arp->TxQ_Nbr >= buf_max_th) {
                    *p_err = NET_CACHE_ERR_UNRESOLVED;
                     return;
                 }

                 ptail_buf = p_cache_addr_arp->TxQ_Tail;
                 if (ptail_buf != (NET_BUF *)0) {               /* If Q NOT empty,    append buf @ Q tail.              */
                     ptail_buf_hdr                 = &ptail_buf->Hdr;
                     ptail_buf_hdr->NextSecListPtr = (NET_BUF *)p_buf;
                     p_buf_hdr->PrevSecListPtr      = (NET_BUF *)ptail_buf;
                     p_cache_addr_arp->TxQ_Tail     = (NET_BUF *)p_buf;

                 } else {                                       /* Else add buf as first q'd buf.                       */
                     p_cache_addr_arp->TxQ_Head     = (NET_BUF *)p_buf;
                     p_cache_addr_arp->TxQ_Tail     = (NET_BUF *)p_buf;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #5].                */
                     p_buf_hdr->PrevSecListPtr      = (NET_BUF *)0;
                     p_buf_hdr->NextSecListPtr      = (NET_BUF *)0;
#endif
                 }

                 p_cache_addr_arp->TxQ_Nbr++;
                                                                /* Cfg buf's unlink fnct/obj to ARP cache.              */
                 p_buf_hdr->UnlinkFnctPtr = (NET_BUF_FNCT)&NetCache_UnlinkBuf;
                 p_buf_hdr->UnlinkObjPtr  = (void       *) p_cache_addr_arp;

                *p_err                    =  NET_ARP_ERR_CACHE_PEND;
                 break;


            case NET_ARP_CACHE_STATE_RENEW:
            case NET_ARP_CACHE_STATE_RESOLVED:                  /* If ARP cache resolved, copy hw addr.                 */
                 Mem_Copy(p_addr_hw,
                          p_cache_addr_arp->AddrHW,
                          NET_IF_HW_ADDR_LEN_MAX);
                 *p_err = NET_ARP_ERR_CACHE_RESOLVED;
                 break;


            case NET_ARP_CACHE_STATE_NONE:
            case NET_ARP_CACHE_STATE_FREE:
            default:
                 NetCache_Remove((NET_CACHE_ADDR *)p_cache_addr_arp, DEF_YES);
                 NetARP_CacheAddPend(p_buf, p_buf_hdr, p_addr_protocol, p_err);
                 break;
        }

    } else {
#ifdef  NET_MCAST_MODULE_EN
        protocol_type_net       = p_buf_hdr->ProtocolHdrTypeNet;
        addr_protocol_multicast = NetMgr_IsAddrProtocolMulticast((NET_PROTOCOL_TYPE)protocol_type_net,
                                                                 (CPU_INT08U      *)p_addr_protocol,
                                                                 (CPU_INT08U       )NET_IPv4_ADDR_SIZE);

        if (addr_protocol_multicast == DEF_YES) {               /* If multicast protocol addr,      ...                 */
            if_nbr      = p_buf_hdr->IF_Nbr;
            addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
                                                                /* ... convert to multicast hw addr ...                 */
            NetIF_AddrMulticastProtocolToHW(if_nbr,
                                            p_addr_protocol,
                                            NET_IPv4_ADDR_SIZE,
                                            protocol_type_net,
                                            p_addr_hw,
                                           &addr_hw_len,
                                            p_err);
            if (*p_err != NET_IF_ERR_NONE) {
                 return;
            }
                                                                /* ... & add as resolved ARP cache.                     */
            NetCache_AddResolved(if_nbr,
                                 p_addr_hw,
                                 p_addr_protocol,
                                 NET_CACHE_TYPE_ARP,
                                 NetARP_CacheRenewTimeout,
                                 NetARP_ReqTimeoutRenew_tick,
                                &err);

           *p_err = NET_ARP_ERR_CACHE_RESOLVED;                 /* Rtn resolved multicast hw addr (see Note #6).        */
            return;
        }
#endif
        NetARP_CacheAddPend(p_buf, p_buf_hdr, p_addr_protocol, p_err);
    }
}

/*
*********************************************************************************************************
*                                        NetARP_CacheGetAddrHW()
*
* Description : (1) Get hardware address that corresponds to a specific ARP cache's protocol address :
*
*                   (a) Acquire network lock
*                   (b) Search ARP Cache List for ARP cache with desired protocol address
*                   (c) If corresponding ARP cache found, get/return     hardware address
*                   (d) Release network lock
*
*
* Argument(s) : if_nbr              Network interface number.
*
*               p_addr_hw           Pointer to a memory buffer that will receive the hardware address :
*
*               addr_hw_len_buf     Length of hardware address memory buffer (in octets) [see Note #4a1].
*
*               p_addr_protocol     Pointer to the specific protocol address to search for corresponding
*
*               addr_protocol_len   Length of protocol address (in octets) [see Note #4b].
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                    Hardware address successfully returned.
*                               NET_ARP_ERR_CACHE_NOT_FOUND         ARP cache with corresponding  hardware/
*                                                                       protocol address NOT found.
*                               NET_ARP_ERR_CACHE_PEND              ARP cache in 'PENDING' state; hardware
*                                                                       address NOT yet resolved (see Note #6).
*
*                               NET_ERR_FAULT_NULL_PTR                Argument 'p_addr_hw'/'p_addr_protocol' passed
*                                                                       a NULL pointer.
*                               NET_ARP_ERR_INVALID_HW_ADDR_LEN     Invalid ARP hardware address length.
*                               NET_ARP_ERR_INVALID_PROTOCOL_LEN    Invalid ARP protocol address length.
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   ------ RETURNED BY Net_GlobalLockAcquire() : -------
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : Length of returned hardware address (see Note #4a2), if available.
*
*               0,                                                   otherwise.
*
* Caller(s)   : NetARP_CacheGetAddrHW().
*
*               This function is an application interface (API) function & MAY be called by application
*               function(s).
*
* Note(s)     : (2) NetARP_CacheGetAddrHW() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) NetARP_CacheGetAddrHW() blocked until network initialization completes.
*
*               (4) (a) (1) The size of the memory buffer that will receive the returned hardware address
*                           MUST be greater than or equal to NET_IF_HW_ADDR_LEN_MAX.
*
*                       (2) The length of any returned hardware address is equal to NET_IF_HW_ADDR_LEN_MAX.
*
*                       (3) Address memory buffer array cleared in case of any error(s).
*
*                           (A) Address memory buffer array SHOULD be initialized to return a NULL address
*                               PRIOR to all validation or function handling in case of any error(s).
*
*                   (b) The length of the protocol address MUST be equal to NET_IPv4_ADDR_SIZE
*                       & is included for correctness & completeness.
*
*               (5) ARP addresses handled in network-order :
*
*                   (a) 'p_addr_hw'            returns  a hardware address in network-order.
*                   (b) 'p_addr_protocol' MUST point to a protocol address in network-order.
*
*                   See also 'NetARP_CacheHandler()  Note #2e3'.
*
*               (6) While an ARP cache is in 'PENDING' state the hardware address is NOT yet resolved,
*                   but MAY be resolved in the near future by an awaited ARP Reply.
*********************************************************************************************************
*/
NET_CACHE_ADDR_LEN  NetARP_CacheGetAddrHW (NET_IF_NBR           if_nbr,
                                           CPU_INT08U          *p_addr_hw,
                                           NET_CACHE_ADDR_LEN   addr_hw_len_buf,
                                           CPU_INT08U          *p_addr_protocol,
                                           NET_CACHE_ADDR_LEN   addr_protocol_len,
                                           NET_ERR             *p_err)
{
    NET_CACHE_ADDR_LEN   addr_hw_len;
    NET_CACHE_ADDR_ARP  *p_cache_addr_arp;


                                                                /* --------------- VALIDATE HW ADDR BUF --------------- */
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_addr_hw == (CPU_INT08U *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
#endif
                                                                /* Clr hw addr         (see Note #4a3).                 */
    Mem_Clr((void     *)p_addr_hw,
            (CPU_SIZE_T) addr_hw_len_buf);

    addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;                       /* Cfg hw addr     len (see Note #4a2).                 */

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (addr_hw_len_buf < addr_hw_len) {                        /* Chk hw addr buf len (see Note #4a1).                 */
       *p_err =  NET_ARP_ERR_INVALID_HW_ADDR_LEN;
        return (0u);
    }
#endif


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ------------ VALIDATE PROTOCOL ADDR BUF ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
                                                                /* Chk protocol addr len (see Note #4b).                */
    if (addr_protocol_len != NET_IPv4_ADDR_SIZE) {
       *p_err =  NET_ARP_ERR_INVALID_PROTOCOL_LEN;
        return (0u);
    }
#else
   (void)&addr_protocol_len;                                    /* Prevent 'variable unused' compiler warning.          */
#endif


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #2b.                                        */
    Net_GlobalLockAcquire((void *)&NetARP_CacheGetAddrHW, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (0u);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif


                                                                /* -------------------- SRCH CACHE -------------------- */
    p_cache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_AddrSrch(NET_CACHE_TYPE_ARP,
                                                              if_nbr,
                                                              p_addr_protocol,
                                                              NET_IPv4_ADDR_SIZE,
                                                              p_err);
    if (p_cache_addr_arp == (NET_CACHE_ADDR_ARP *)0) {
       *p_err = NET_ARP_ERR_CACHE_NOT_FOUND;
        goto exit_fail;
    }

    switch (*p_err) {
        case NET_CACHE_ERR_PEND:
            *p_err =  NET_ARP_ERR_CACHE_PEND;                       /* ... (see Note #6).                                   */
             goto exit_fail;


        case NET_CACHE_ERR_RESOLVED:
             Mem_Copy(p_addr_hw,
                      p_cache_addr_arp->AddrHW,
                      NET_IF_HW_ADDR_LEN_MAX);
             break;


        default:
             NetCache_Remove((NET_CACHE_ADDR *)p_cache_addr_arp, DEF_YES);
            *p_err = NET_ARP_ERR_CACHE_NOT_FOUND;
             goto exit_fail;
    }


   *p_err = NET_ARP_ERR_NONE;
    goto exit_release;

exit_fail:
    addr_hw_len = 0;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (addr_hw_len);
}


/*
*********************************************************************************************************
*                                       NetARP_CachePoolStatGet()
*
* Description : Get ARP statistics pool.
*
* Argument(s) : none.
*
* Return(s)   : ARP  statistics pool, if NO error(s).
*
*               NULL statistics pool, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) NetARP_CachePoolStatGet() blocked until network initialization completes; return NULL
*                   statistics pool.
*
*               (2) 'NetARP_CachePoolStat' MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

NET_STAT_POOL  NetARP_CachePoolStatGet (void)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    NET_ERR        err;
#endif
    NET_STAT_POOL  stat_pool;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        NetStat_PoolClr(&stat_pool, &err);
        return (stat_pool);                                     /* ... rtn NULL stat pool (see Note #1).                */
    }
#endif


    CPU_CRITICAL_ENTER();
    stat_pool = NetCache_AddrARP_PoolStat;
    CPU_CRITICAL_EXIT();

    return (stat_pool);
}


/*
*********************************************************************************************************
*                                  NetARP_CachePoolStatResetMaxUsed()
*
* Description : Reset ARP statistics pool's maximum number of entries used.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_CachePoolStatGet(),
*               NetARP_CachePoolStatResetMaxUsed().
*
*               This function is an application interface (API) function & MAY be called by application
*               function(s).
*
* Note(s)     : (1) NetARP_CachePoolStatResetMaxUsed() blocked until network initialization completes.
*
*                   (a) However, since 'NetARP_CachePoolStat' is reset when network initialization
*                       completes; NO error is returned.
*********************************************************************************************************
*/

void  NetARP_CachePoolStatResetMaxUsed (void)
{
    NET_ERR  err;


                                                                /* Acquire net lock.                                    */
    Net_GlobalLockAcquire((void *)&NetARP_CachePoolStatResetMaxUsed, &err);
    if (err != NET_ERR_NONE) {
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        goto exit_release;                                      /* ... rtn w/o err (see Note #1a).                      */
    }
#endif


    NetStat_PoolResetUsedMax(&NetCache_AddrARP_PoolStat, &err); /* Reset ARP cache stat pool.                           */
    goto exit_release;

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */
}


/*
*********************************************************************************************************
*                                              NetARP_Rx()
*
* Description : (1) Process received ARP packets & update ARP Cache List :
*
*                   (a) Validate ARP packet
*                   (b) Update   ARP cache
*                   (c) Prepare & transmit an ARP Reply for a received ARP Request
*                   (d) Free ARP packet
*                   (e) Update receive statistics
*
*
* Argument(s) : p_buf   Pointer to network buffer that received ARP packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                ARP packet successfully received & processed.
*
*                                                               ---- RETURNED BY NetARP_RxPktDiscard() : ----
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_RxPktFrameDemux().
*
*               This function is a board-support package function & SHOULD be called only by
*               appropriate product function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetARP_Rx (NET_BUF  *p_buf,
                 NET_ERR  *p_err)
{
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR      *p_ctr;
#endif
    NET_BUF_HDR  *p_buf_hdr;
    NET_ARP_HDR  *p_arp_hdr;
    NET_IF_NBR    if_nbr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetARP_RxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        return;
    }
#endif


    NET_CTR_STAT_INC(Net_StatCtrs.ARP.RxPktCtr);


                                                                /* -------------- VALIDATE RX'D ARP PKT --------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetARP_RxPktValidateBuf(p_buf_hdr, p_err);                  /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_ARP_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetARP_RxPktDiscard(p_buf, p_err);
             return;
    }
#endif
    p_arp_hdr = (NET_ARP_HDR *)&p_buf->DataPtr[p_buf_hdr->ARP_MsgIx];
    NetARP_RxPktValidate(p_buf_hdr, p_arp_hdr, p_err);          /* Validate rx'd pkt.                                   */


                                                                /* ----------------- UPDATE ARP CACHE ----------------- */
    switch (*p_err) {                                           /* Chk err from NetARP_RxPktValidate().                 */
        case NET_ARP_ERR_NONE:
             if_nbr = p_buf_hdr->IF_Nbr;
             NetARP_RxPktCacheUpdate(if_nbr, p_arp_hdr, p_err);
             break;


        case NET_ARP_ERR_INVALID_HW_TYPE:
        case NET_ARP_ERR_INVALID_HW_ADDR:
        case NET_ARP_ERR_INVALID_HW_ADDR_LEN:
        case NET_ARP_ERR_INVALID_PROTOCOL_TYPE:
        case NET_ARP_ERR_INVALID_PROTOCOL_ADDR:
        case NET_ARP_ERR_INVALID_PROTOCOL_LEN:
        case NET_ARP_ERR_INVALID_OP_CODE:
        case NET_ARP_ERR_INVALID_OP_ADDR:
        case NET_ARP_ERR_INVALID_LEN_MSG:
        default:
             NetARP_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* ------------------- TX ARP REPLY ------------------- */
    switch (*p_err) {                                           /* Chk err from NetARP_RxPktCacheUpdate().              */
        case NET_CACHE_ERR_RESOLVED:
        case NET_ARP_ERR_CACHE_RESOLVED:
             NetARP_RxPktReply(if_nbr, p_arp_hdr, p_err);
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_CACHE_ERR_NONE_AVAIL:
        case NET_CACHE_ERR_INVALID_TYPE:
        case NET_ARP_ERR_INVALID_OP_CODE:
        case NET_ARP_ERR_INVALID_PROTOCOL_LEN:
        case NET_ARP_ERR_RX_TARGET_REPLY:
        case NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST:
        case NET_ERR_FAULT_UNKNOWN_ERR:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_MGR_ERR_ADDR_TBL_SIZE:
        case NET_MGR_ERR_INVALID_PROTOCOL:
        case NET_MGR_ERR_INVALID_PROTOCOL_LEN:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_TMR_ERR_NONE_AVAIL:
        case NET_TMR_ERR_INVALID_TYPE:
        default:
             NetARP_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* ---------- FREE ARP PKT / UPDATE RX STATS ---------- */
    switch (*p_err) {                                           /* Chk err from NetARP_RxPktReply().                    */
        case NET_ARP_ERR_RX_REQ_TX_REPLY:
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
             p_ctr = (NET_CTR *)&Net_StatCtrs.ARP.RxMsgReqCompCtr;
#endif
             break;


        case NET_ARP_ERR_RX_REPLY_TX_PKTS:
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
             p_ctr = (NET_CTR *)&Net_StatCtrs.ARP.RxMsgReplyCompCtr;
#endif
             break;


        case NET_ARP_ERR_INVALID_OP_CODE:
        case NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST:
        default:
             NetARP_RxPktDiscard(p_buf, p_err);
             return;
    }

    NetARP_RxPktFree(p_buf);

    NET_CTR_STAT_INC(Net_StatCtrs.ARP.RxMsgCompCtr);
    NET_CTR_STAT_INC(*p_ctr);


   *p_err = NET_ARP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetARP_TxReqGratuitous()
*
* Description : (1) Prepare & transmit a gratuitous ARP Request onto the local network :
*
*                   (a) Acquire  network lock
*                   (b) Get      network interface's hardware address
*                   (c) Prepare  ARP Request packet :
*                       (1) Configure sender's hardware address as this interface's hardware address
*                       (2) Configure target's hardware address as NULL
*                       (3) Configure sender's protocol address as this interface's protocol address
*                       (4) Configure target's protocol address as this interface's protocol address    See Note #6a
*                       (5) Configure ARP operation as ARP Request
*                   (d) Transmit ARP Request
*                   (e) Release  network lock
*
*
*               (2) NetARP_TxReqGratuitous() COULD be used in conjunction with NetARP_IsAddrProtocolConflict()
*                   to determine if the host's protocol address is already present on the local network :
*
*                   (a) After successfully transmitting a gratuitous ARP Request onto the local network & ...
*                   (b) After some time delay(s) [on the order of ARP Request timeouts & retries];        ...
*                   (c) Check this host's ARP protocol address conflict flag to see if any other host(s)
*                           are configured with this host's ARP protocol address.
*
*                   See also 'NetARP_IsAddrProtocolConflict()  Note #3'
*                          & 'NetARP_CacheProbeAddrOnNet()     Note #2'.
*
*
* Argument(s) : protocol_type       Address protocol type.
*
*               p_addr_protocol     Pointer to protocol address used to transmit gratuitous request (see Note #5).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*                               NET_ERR_FAULT_NULL_PTR                Argument(s) passed a NULL pointer.
*
*                                                                   ------------ RETURNED BY NetARP_Tx() : -------------
*                               NET_ARP_ERR_NONE                    Gratuitous ARP request packet successfully
*                                                                       transmitted.
*                               NET_ARP_ERR_INVALID_OP_CODE         Invalid ARP operation code.
*                               NET_BUF_ERR_NONE_AVAIL              NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE            Requested size is greater then the maximum
*                                                                       buffer size available.
*                               NET_BUF_ERR_INVALID_IX              Invalid buffer index.
*                               NET_BUF_ERR_INVALID_LEN             Invalid buffer length.
*                               NET_ERR_INVALID_TRANSACTION         Invalid transaction type.
*                               NET_ERR_TX                          Transmit error; packet discarded.
*                               NET_ERR_IF_LOOPBACK_DIS             Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN                Network  interface link state down (i.e.
*                                                                       NOT available for receive or transmit).
*
*                                                                   - RETURNED BY NetMgr_GetHostAddrProtocolIF_Nbr() : -
*                               NET_MGR_ERR_INVALID_PROTOCOL        Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_ADDR   Invalid protocol address NOT used by host.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN    Invalid protocol address length.
*
*                                                                   ----- RETURNED BY NetIF_AddrHW_GetHandler() : ------
*                               NET_ERR_FAULT_NULL_PTR                 Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_IF_ERR_INVALID_ADDR_LEN         Invalid hardware address length.
*
*                                                                   ------ RETURNED BY Net_GlobalLockAcquire() : -------
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Network Application.
*
*               This function is a network protocol suite function that SHOULD be called only by appropriate
*               network application function(s) [see also Note #3].
*
* Note(s)     : (3) NetARP_TxReqGratuitous() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (4) NetARP_TxReqGratuitous() blocked until network initialization completes.
*
*               (5) 'p_addr_protocol' MUST point to a valid protocol address in network-order.
*
*               (6) RFC #3927, Section 2.4 states that one purpose for transmitting a gratuitous ARP Request
*                   is for a host to "announce its [claim] ... [on] a unique address ... by broadcasting ...
*                   an ARP Request ... to make sure that other hosts on the link do not have stale ARP cache
*                   entries left over from some other host that may previously have been using the same
*                   address".
*
*                   (a) "The ... ARP Request ... announcement ... sender and target IP addresses are both
*                        set to the host's newly selected ... address."
*********************************************************************************************************
*/
void  NetARP_TxReqGratuitous (NET_PROTOCOL_TYPE    protocol_type,
                              CPU_INT08U          *p_addr_protocol,
                              NET_CACHE_ADDR_LEN   addr_protocol_len,
                              NET_ERR             *p_err)
{
    NET_IF_NBR   if_nbr;
    CPU_INT08U   addr_hw_len;
    CPU_INT08U   addr_hw_sender[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U  *p_addr_hw_sender;
    CPU_INT08U  *p_addr_hw_target;
    CPU_INT08U  *p_addr_protocol_sender;
    CPU_INT08U  *p_addr_protocol_target;
    CPU_INT16U   op_code;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* -------------- VALIDATE PROTOCOL ADDR -------------- */
    if (p_addr_protocol == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #3b.                                        */
    Net_GlobalLockAcquire((void *)&NetARP_TxReqGratuitous, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit tx (see Note #4).         */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif


                                                                /* ------------- GET PROTOCOL & HW ADDRS -------------- */
    if_nbr = NetMgr_GetHostAddrProtocolIF_Nbr(protocol_type,
                                              p_addr_protocol,
                                              addr_protocol_len,
                                              p_err);
    if (*p_err != NET_MGR_ERR_NONE) {
         goto exit_release;
    }

    addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
    NetIF_AddrHW_GetHandler((NET_IF_NBR  ) if_nbr,
                            (CPU_INT08U *)&addr_hw_sender[0],
                            (CPU_INT08U *)&addr_hw_len,
                            (NET_ERR    *) p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }

                                                                /* --------------- PREPARE ARP REQ PKT ---------------- */
                                                                /* See Note #1c.                                        */
    p_addr_hw_sender       = (CPU_INT08U *)&addr_hw_sender[0];
    p_addr_hw_target       = (CPU_INT08U *) 0;
    p_addr_protocol_sender = (CPU_INT08U *) p_addr_protocol;
    p_addr_protocol_target = (CPU_INT08U *) p_addr_protocol;

    op_code               =  NET_ARP_HDR_OP_REQ;

                                                                /* -------------------- TX ARP REQ -------------------- */
    NetARP_Tx(if_nbr,
              p_addr_hw_sender,
              p_addr_hw_target,
              p_addr_protocol_sender,
              p_addr_protocol_target,
              op_code,
              p_err);

    if (*p_err == NET_ARP_ERR_NONE) {
         NET_CTR_STAT_INC(Net_StatCtrs.ARP.TxMsgReqCtr);
    }


exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                            NetARP_TxReq()
*
* Description : (1) Prepare & transmit an ARP Request to resolve a pending ARP cache :
*
*                   (a) Get network interface's hardware & protocol addresses
*                   (b) Prepare  ARP Request packet :
*                       (1) Configure sender's hardware address as this interface's hardware address
*                       (2) Configure target's hardware address as NULL since unknown
*                       (3) Configure sender's protocol address as this interface's protocol address
*                       (4) Configure target's protocol address as the protocol address listed in the ARP cache
*                       (5) Configure ARP operation as ARP Request
*
*
* Argument(s) : p_cache     Pointer to an ARP cache.
*               ------      Argument checked in NetARP_CacheAddPend(),
*                                               NetARP_CacheReqTimeout(),
*                                               NetARP_CacheProbeAddrOnNet().
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_CacheAddPend(),
*               NetARP_CacheReqTimeout(),
*               NetARP_CacheProbeAddrOnNet().
*
* Note(s)     : (2) Do NOT need to verify success of ARP Request since failure will cause timeouts & retries.
*********************************************************************************************************
*/

void  NetARP_TxReq (NET_CACHE_ADDR_ARP  *p_cache_addr_arp)
{
    NET_ARP_CACHE  *p_cache_arp;
    CPU_INT08U      addr_hw_len;
    CPU_INT08U      addr_hw_sender[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U     *p_addr_hw_target;
    CPU_INT08U     *p_addr_protocol_target;
    CPU_INT08U     *p_addr_protocol_sender;
    NET_IF_NBR      if_nbr;
    CPU_INT16U      op_code;
    NET_ERR         err;


                                                                /* ------------ GET HW & PROTOCOL IF ADDRS ------------ */
    addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
    NetIF_AddrHW_GetHandler((NET_IF_NBR  ) p_cache_addr_arp->IF_Nbr,
                            (CPU_INT08U *)&addr_hw_sender[0],
                            (CPU_INT08U *)&addr_hw_len,
                            (NET_ERR    *)&err);
    if (err != NET_IF_ERR_NONE) {
        return;
    }


                                                                /* --------- CFG ARP REQ FROM ARP CACHE DATA ---------- */
                                                                /* See Note #1b.                                        */
    if_nbr                 =  p_cache_addr_arp->IF_Nbr;
    p_addr_protocol_target = (CPU_INT08U *)&p_cache_addr_arp->AddrProtocol[0];
    p_addr_protocol_sender = (CPU_INT08U *)&p_cache_addr_arp->AddrProtocolSender[0];

    p_cache_arp = NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_arp);
    if (p_cache_arp->State  != NET_ARP_CACHE_STATE_RENEW) {
        p_addr_hw_target = DEF_NULL;
    } else {
        p_addr_hw_target = p_cache_addr_arp->AddrHW;
    }

    op_code               =  NET_ARP_HDR_OP_REQ;

    NetARP_Tx(if_nbr,
             &addr_hw_sender[0],
              p_addr_hw_target,
              p_addr_protocol_sender,
              p_addr_protocol_target,
              op_code,
             &err);

    if (err == NET_ARP_ERR_NONE) {
        NET_CTR_STAT_INC(Net_StatCtrs.ARP.TxMsgReqCtr);
    }

    p_cache_arp = (NET_ARP_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_arp);
    if (p_cache_arp == ((NET_ARP_CACHE *)0)) {
        return;
    }

    p_cache_arp->ReqAttemptsCtr++;                              /* Inc req attempts ctr.                                */
}


/*
*********************************************************************************************************
*                                         NetARP_CacheAddPend()
*
* Description : (1) Add a 'PENDING' ARP cache into the ARP Cache List & transmit an ARP Request :
*
*                   (a) Configure ARP cache :
*                       (1) Get sender protocol sender
*                       (2) Get default-configured ARP cache
*                       (3) ARP cache state
*                       (4) Enqueue transmit buffer to ARP cache queue
*                   (b) Insert   ARP cache into ARP Cache List
*                   (c) Transmit ARP Request to resolve ARP cache
*
*
* Argument(s) : p_buf           Pointer to network buffer to transmit.
*               ----            Argument checked   in NetARP_CacheHandler().
*
*               p_buf_hdr       Pointer to network buffer header.
*               --------        Argument validated in NetARP_CacheHandler().
*
*               p_addr_protocol Pointer to protocol address (see Note #2).
*               --------------  Argument checked   in NetARP_CacheHandler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_CACHE_PEND              ARP cache added in 'PENDING' state.
*
*                                                                   -- RETURNED BY NetCache_CfgAddrs() : --
*                               NET_CACHE_ERR_NONE_AVAIL            NO available ARP caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE          ARP cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT               Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL              NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE            Network timer is NOT a valid timer type.
*                               NET_ERR_FAULT_NULL_PTR              Variable is set to Null pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_CacheHandler().
*
* Note(s)     : (2) 'p_addr_protocol' MUST point to a valid protocol address in network-order.
*
*                    See also 'NetARP_CacheHandler()  Note #2e3'.
*
*               (3) (a) RFC #1122, Section 2.3.2.2 states that "the link layer SHOULD" :
*
*                       (1) "Save (rather than discard) ... packets destined to the same unresolved
*                            IP address and" ...
*                       (2) "Transmit the saved packet[s] when the address has been resolved."
*
*                   (b) Since ARP Layer is the last layer to handle & queue the transmit network
*                       buffer, it is NOT necessary to increment the network buffer's reference
*                       counter to include the pending ARP cache buffer queue as a new reference
*                       to the network buffer.
*
*               (4) Some buffer controls were previously initialized in NetBuf_Get() when the packet
*                   was received at the network interface layer.  These buffer controls do NOT need
*                   to be re-initialized but are shown for completeness.
*********************************************************************************************************
*/
void  NetARP_CacheAddPend (NET_BUF      *p_buf,
                           NET_BUF_HDR  *p_buf_hdr,
                           CPU_INT08U   *p_addr_protocol,
                           NET_ERR      *p_err)
{
    NET_CACHE_ADDR_ARP  *p_cache_addr_arp;
    NET_ARP_CACHE       *p_cache_arp;
    CPU_INT08U           addr_protocol_sender[NET_IPv4_ADDR_SIZE];
    NET_IF_NBR           if_nbr;
    NET_TMR_TICK         timeout_tick;
    CPU_SR_ALLOC();

                                                                /* ------------------ CFG ARP CACHE ------------------- */
                                                                /* Copy sender protocol addr to net order.              */
                                                                /* Cfg protocol addr generically from IP addr.          */
    NET_UTIL_VAL_COPY_SET_NET_32(&addr_protocol_sender[0], &p_buf_hdr->IP_AddrSrc);

    if_nbr       = p_buf_hdr->IF_Nbr;
    CPU_CRITICAL_ENTER();
    timeout_tick = NetARP_ReqTimeoutPend_tick;
    CPU_CRITICAL_EXIT();

    p_cache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_CfgAddrs(NET_CACHE_TYPE_ARP,
                                                               if_nbr,
                                                               0,
                                                               NET_IF_HW_ADDR_LEN_MAX,
                                                               p_addr_protocol,
                                                              &addr_protocol_sender[0],
                                                               NET_IPv4_ADDR_SIZE,
                                                               DEF_YES,
                                                               NetARP_CacheReqTimeout,
                                                               timeout_tick,
                                                               p_err);
    if ((*p_err            != NET_CACHE_ERR_NONE) ||
        ( p_cache_addr_arp == (NET_CACHE_ADDR_ARP *)0)) {
        return;
    }

                                                                /* Cfg buf's unlink fnct/obj to ARP cache.              */
    p_buf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT)NetCache_UnlinkBuf;
    p_buf_hdr->UnlinkObjPtr   = (void       *)p_cache_addr_arp;

#if 0                                                           /* Init'd in NetBuf_Get() [see Note #4].                */
    p_buf_hdr->PrevSecListPtr = (NET_BUF *)0;
    p_buf_hdr->NextSecListPtr = (NET_BUF *)0;
#endif
                                                                /* Q buf to ARP cache (see Note #3a1).                  */
    p_cache_addr_arp->TxQ_Head         = (NET_BUF *)p_buf;
    p_cache_addr_arp->TxQ_Tail         = (NET_BUF *)p_buf;
    p_cache_addr_arp->TxQ_Nbr++;

    p_cache_arp = (NET_ARP_CACHE *)p_cache_addr_arp->ParentPtr;

    p_cache_arp->State = NET_ARP_CACHE_STATE_PEND;

                                                                /* ------- INSERT ARP CACHE INTO ARP CACHE LIST ------- */
    NetCache_Insert((NET_CACHE_ADDR *)p_cache_addr_arp);
                                                                /* -------------------- TX ARP REQ -------------------- */
    NetARP_TxReq(p_cache_addr_arp);                             /* Tx ARP req to resolve ARP cache.                     */


   *p_err = NET_ARP_ERR_CACHE_PEND;
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
*                                       NetARP_RxPktValidateBuf()
*
* Description : Validate received buffer header as ARP protocol.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received ARP packet.
*               --------    Argument validated in NetARP_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                Received buffer's ARP header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT ARP.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetARP_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
                                       NET_ERR      *p_err)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  valid;
    NET_ERR      err;

                                                                /* -------------- VALIDATE NET BUF TYPE --------------- */
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
             break;


        case NET_BUF_TYPE_NONE:
        case NET_BUF_TYPE_BUF:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

                                                                /* --------------- VALIDATE ARP BUF HDR --------------- */
    if (p_buf_hdr->ProtocolHdrType != NET_PROTOCOL_TYPE_ARP) {
        if_nbr = p_buf_hdr->IF_Nbr;
        valid  = NetIF_IsValidHandler(if_nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvProtocolCtr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->ARP_MsgIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_ARP_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        NetARP_RxPktValidate()
*
* Description : (1) Validate received ARP packet :
*
*                   (a) Validate the received packet's following ARP header fields :
*
*                       (1) Hardware  Type
*                       (2) Hardware  Address Length
*                       (3) Hardware  Address : Sender's
*                       (4) Protocol  Type
*                       (5) Protocol  Address Length
*                       (6) Protocol  Address : Sender's
*                       (7) Operation Code
*
*                   (b) Convert the following ARP header fields from network-order to host-order :
*
*                       (1) Hardware  Type
*                       (2) Protocol  Type
*                       (3) Operation Code                                          See Note #1bB
*
*                           (A) These fields are NOT converted directly in the received packet buffer's
*                               data area but are converted in local or network buffer variables ONLY.
*
*                           (B) To avoid storing the ARP operation code in a network buffer variable &
*                               passing an additional pointer to the network buffer header that received
*                               ARP packet, ARP operation code is converted in the following functions :
*
*                               (1) NetARP_RxPktValidate()
*                               (2) NetARP_RxPktCacheUpdate()
*                               (3) NetARP_RxPktReply()
*                               (4) NetARP_RxPktIsTargetThisHost()
*
*                           (C) Hardware & Protocol Addresses are NOT converted from network-order to
*                               host-order & MUST be accessed as multi-octet arrays.
*
*                   (c) Validate the received packet's following ARP packet controls :
*
*                       (1) ARP message length                                      See Note #4
*
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received ARP packet.
*               --------    Argument validated in NetARP_Rx().
*
*               p_arp_hdr   Pointer to received packet's ARP header.
*               --------    Argument validated in NetARP_Rx()/NetARP_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                    Received packet validated.
*                               NET_ARP_ERR_INVALID_HW_TYPE         Invalid ARP hardware type.
*                               NET_ARP_ERR_INVALID_HW_ADDR         Invalid ARP hardware address.
*                               NET_ARP_ERR_INVALID_HW_ADDR_LEN     Invalid ARP hardware address length.
*                               NET_ARP_ERR_INVALID_PROTOCOL_TYPE   Invalid ARP protocol type.
*                               NET_ARP_ERR_INVALID_PROTOCOL_ADDR   Invalid ARP protocol address.
*                               NET_ARP_ERR_INVALID_PROTOCOL_LEN    Invalid ARP protocol address length.
*                               NET_ARP_ERR_INVALID_OP_CODE         Invalid ARP operation code.
*                               NET_ARP_ERR_INVALID_OP_ADDR         Invalid address for ARP operation
*                                                                       (see Note #3).
*                               NET_ARP_ERR_INVALID_LEN_MSG         Invalid ARP message length
*                                                                       (see Note #1c1).
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Rx().
*
* Note(s)     : (2) See RFC #826, Section 'Packet Format' for ARP packet header format.
*
*               (3) (a) (1) RFC #826, Section 'Packet Generation' states that "the Address Resolution module
*                           ... does not set [the ARP Request packet's target hardware address] to anything
*                           in particular, because it is this value that it is trying to determine" :
*
*                           (A) "Could    set [the ARP Request packet's target hardware address] to the
*                                broadcast address for the hardware."
*
*                           (B) "Could    set [the ARP Request packet's target hardware address] to the ...
*                                hardware  address of target of this packet (if known)."
*
*                       (2) (A) Therefore, ARP Requests SHOULD typically be transmitted onto a network via
*                               the network's broadcast address.
*
*                           (B) However, an ARP Request COULD be transmitted directly to a specific host/
*                               hardware address.
*
*                           (C) Thus, any ARP Request NOT received as a broadcast OR directly-addressed
*                               packet MUST be discarded.
*
*                   (b) (1) RFC #826, Section 'Packet Reception' states to "send the [ARP Reply] packet to
*                           the ... address ... [from] which the request was received".
*
*                       (2) (A) Therefore, an ARP Reply SHOULD be transmitted directly to the ARP-Requesting
*                               host & SHOULD NOT be broadcast onto the network.
*
*                           (B) Thus, any ARP Reply received as a broadcast packet SHOULD be discarded.
*
*               (4) (a) The ARP message length SHOULD be compared to the remaining packet data length which
*                       should be identical.
*
*                   (b) (1) However, some network interfaces MAY append octets to their frames :
*
*                           (A) 'pad' octets, if the frame length does NOT meet the frame's required minimum size :
*
*                               (1) RFC #894, Section 'Frame Format' states that "the minimum length of  the data
*                                   field of a packet sent over an Ethernet is 46 octets.  If necessary, the data
*                                   field should be padded (with octets of zero) to meet the Ethernet minimum frame
*                                   size".
*
*                               (2) RFC #1042, Section 'Frame Format and MAC Level Issues : For all hardware types'
*                                   states that "IEEE 802 packets may have a minimum size restriction.  When
*                                   necessary, the data field should be padded (with octets of zero) to meet the
*                                   IEEE 802 minimum frame size requirements".
*
*                           (B) Trailer octets, to improve overall throughput :
*
*                               (1) RFC #893, Section 'Introduction' specifies "a link-level ... trailer
*                                   encapsulation, or 'trailer' ... to minimize the number and size of memory-
*                                   to-memory copy operations performed by a receiving host when processing a
*                                   data packet".
*
*                               (2) RFC #1122, Section 2.3.1 states that "trailer encapsulations[s] ... rearrange
*                                   the data contents of packets ... [to] improve the throughput of higher layer
*                                   protocols".
*
*                           (C) CRC or checksum values, optionally copied from a device.
*
*                       (2) Therefore, if ANY octets are appended to the total frame length, then the packet's
*                           remaining data length MAY be greater than the ARP message length :
*
*                           (A) Thus,    the ARP message length & the packet's remaining data length CANNOT be
*                               compared for equality.
*
*                               (1) Unfortunately, this eliminates the possibility to validate the ARP message
*                                   length to the packet's remaining data length.
*
*                           (B) And      the ARP message length MAY    be less    than the packet's remaining
*                               data length.
*
*                               (1) However, the packet's remaining data length MUST be reset to the ARP message
*                                   length to correctly calculate higher-layer application data length.
*
*                           (C) However, the ARP message length CANNOT be greater than the packet's remaining
*                               data length.
*********************************************************************************************************
*/

static  void  NetARP_RxPktValidate (NET_BUF_HDR  *p_buf_hdr,
                                    NET_ARP_HDR  *p_arp_hdr,
                                    NET_ERR      *p_err)
{
    CPU_INT08U          addr_hw_target[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U          addr_hw_len;
    CPU_INT08U         *p_addr_target_hw;
    CPU_BOOLEAN         target_hw_verifd;
    CPU_INT16U          hw_type;
    CPU_INT16U          protocol_type_arp;
    NET_PROTOCOL_TYPE   protocol_type;
    CPU_INT16U          op_code;
    CPU_BOOLEAN         rx_broadcast;
    CPU_BOOLEAN         valid;
    NET_ERR             err;


                                                                /* ------------ VALIDATE ARP HW TYPE/ADDR ------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&hw_type, &p_arp_hdr->AddrHW_Type);
    if (hw_type != NET_ADDR_HW_TYPE_802x) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvHW_TypeCtr);
       *p_err = NET_ARP_ERR_INVALID_HW_TYPE;
        return;
    }

    if (p_arp_hdr->AddrHW_Len != NET_IF_HW_ADDR_LEN_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvHW_AddrLenCtr);
       *p_err = NET_ARP_ERR_INVALID_HW_ADDR_LEN;
        return;
    }

    valid = NetIF_AddrHW_IsValidHandler((NET_IF_NBR  ) p_buf_hdr->IF_Nbr,
                                        (CPU_INT08U *)&p_arp_hdr->AddrHW_Sender[0],
                                        (NET_ERR    *)&err);
    if (valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvHW_AddrCtr);
       *p_err = NET_ARP_ERR_INVALID_HW_ADDR;
        return;
    }


                                                                /* --------- VALIDATE ARP PROTOCOL TYPE/ADDR ---------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&protocol_type_arp, &p_arp_hdr->AddrProtocolType);
    if (protocol_type_arp != NET_ARP_PROTOCOL_TYPE_IP_V4) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvProtocolTypeCtr);
       *p_err = NET_ARP_ERR_INVALID_PROTOCOL_TYPE;
        return;
    }

    if (p_arp_hdr->AddrProtocolLen != NET_IPv4_ADDR_SIZE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvLenCtr);
       *p_err = NET_ARP_ERR_INVALID_PROTOCOL_LEN;
        return;
    }


                                                                /* Get net protocol type.                               */
    switch (protocol_type_arp) {
        case NET_ARP_PROTOCOL_TYPE_IP_V4:
             protocol_type = NET_PROTOCOL_TYPE_IP_V4;
             break;


        default:
             protocol_type = NET_PROTOCOL_TYPE_NONE;
             break;
    }


    valid = NetMgr_IsValidAddrProtocol((NET_PROTOCOL_TYPE) protocol_type,
                                       (CPU_INT08U      *)&p_arp_hdr->AddrProtocolSender[0],
                                       (CPU_INT08U       ) NET_IPv4_ADDR_SIZE);
    if (valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvProtocolAddrCtr);
       *p_err = NET_ARP_ERR_INVALID_PROTOCOL_ADDR;
        return;
    }


                                                                /* --------------- VALIDATE ARP OP CODE --------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&op_code, &p_arp_hdr->OpCode);
    rx_broadcast = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_BROADCAST);
    switch (op_code) {
        case NET_ARP_HDR_OP_REQ:
             if (NetARP_AddrFltrEn == DEF_YES) {
                 if (rx_broadcast != DEF_YES) {                 /* If rx'd ARP Req NOT broadcast (see Note #3a2A)  ...  */
                     addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
                     NetIF_AddrHW_GetHandler((NET_IF_NBR  ) p_buf_hdr->IF_Nbr,
                                             (CPU_INT08U *)&addr_hw_target[0],
                                             (CPU_INT08U *)&addr_hw_len,
                                             (NET_ERR    *) p_err);
                     if (*p_err != NET_IF_ERR_NONE) {
                          NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvOpAddrCtr);
                         *p_err = NET_ARP_ERR_INVALID_OP_ADDR;
                          return;
                     }

                     p_addr_target_hw  = p_arp_hdr->AddrHW_Target;
                     target_hw_verifd = Mem_Cmp((void     *) p_addr_target_hw,
                                                (void     *)&addr_hw_target[0],
                                                (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
                     if (target_hw_verifd != DEF_YES) {         /* ... & NOT addr'd to this host (see Note #3a2B), ...  */
                         NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvOpAddrCtr);
                        *p_err = NET_ARP_ERR_INVALID_OP_ADDR;   /* ... rtn err / discard pkt     (see Note #3a2C).      */
                         return;
                     }
                 }
             }
             break;


        case NET_ARP_HDR_OP_REPLY:
             if (rx_broadcast != DEF_NO) {                      /* If rx'd ARP Reply broadcast, ...                     */
                 NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvOpAddrCtr);
                *p_err = NET_ARP_ERR_INVALID_OP_ADDR;           /* ... rtn err / discard pkt (see Note #3b2B).          */
                 return;
             }
             break;


        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvOpCodeCtr);
            *p_err = NET_ARP_ERR_INVALID_OP_CODE;
             return;
    }


                                                                /* --------------- VALIDATE ARP MSG LEN --------------- */
    p_buf_hdr->ARP_MsgLen = NET_ARP_HDR_SIZE;
    if (p_buf_hdr->ARP_MsgLen > p_buf_hdr->DataLen) {           /* If ARP msg len > rem pkt data len, ...               */
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvMsgLenCtr);
       *p_err = NET_ARP_ERR_INVALID_LEN_MSG;                    /* ... rtn err (see Note #4b2C).                        */
        return;
    }

    p_buf_hdr->DataLen = (NET_BUF_SIZE)p_buf_hdr->ARP_MsgLen;   /* Trunc pkt data len to ARP msg len (see Note #4b2B1). */



   *p_err = NET_ARP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetARP_RxPktCacheUpdate()
*
* Description : (1) Update an ARP cache based on received ARP packet's sender addresses :
*
*                   (a) Verify ARP message's intended target address is this host
*                   (b) Search ARP Cache List
*                   (c) Update ARP cache
*
*
* Argument(s) : if_nbr      Interface number the packet was received from.
*
*               p_arp_hdr   Pointer to received packet's ARP header.
*               --------    Argument validated in NetARP_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_CACHE_RESOLVED              ARP cache resolved & hardware address
*                                                                           successfully copied.
*                               NET_ARP_ERR_RX_TARGET_REPLY             ARP Reply received but NO corresponding ARP
*                                                                           cache currently pending for ARP Reply.
*
*                                                                       ----- RETURNED BY NetCacheAdd_Resolved() : -----
*                               NET_CACHE_ERR_NONE_AVAIL                NO available ARP caches to allocate.
*                               NET_CACHE_ERR_INVALID_TYPE              ARP cache is NOT a valid cache type.
*                               NET_ERR_FAULT_NULL_FNCT                   Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL                  NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE                Network timer is NOT a valid timer type.
*
*                                                                       - RETURNED BY NetARP_RxPktIsTargetThisHost() : -
*                               NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST     Received ARP message NOT intended for this host.
*                               NET_ARP_ERR_INVALID_OP_CODE             Invalid  ARP operation code.
*                               NET_ARP_ERR_INVALID_PROTOCOL_LEN        Invalid  ARP protocol address length.
*                               NET_IF_ERR_INVALID_IF                   Invalid network interface number.
*                               NET_ERR_FAULT_UNKNOWN_ERR                       Interface's protocol address(s) NOT
*                                                                           successfully returned.
*                               NET_ERR_FAULT_NULL_PTR                    Argument(s) passed a NULL pointer.
*                               NET_MGR_ERR_INVALID_PROTOCOL            Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN        Invalid protocol address length.
*                               NET_MGR_ERR_ADDR_TBL_SIZE               Invalid protocol address table size.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Rx().
*
* Note(s)     : (2) (a) RFC #826, Section 'Packet Reception' states that :
*
*                       (1) "If the ... sender protocol address ... is already in ... [the] translation table,
*                            update the sender hardware address field of the entry with the new information in
*                            the packet."
*
*                       (2) Otherwise, if the packet's target protocol address matches this host's ARP protocol
*                           address; then "add the ... sender protocol address, sender hardware address to the
*                           translation table".
*
*                   (b) (1) Thus, the ARP cache algorithm implies that ALL messages received at the ARP layer
*                           automatically update the ARP Cache List EVEN if this host is NOT the intended target
*                           host of a received ARP message -- but ONLY if an ARP cache for the sender's addresses
*                           is already cached.
*
*                       (2) However, if NO ARP cache exists for the sender's addresses, then even the ARP cache
*                           algorithm implies that a misdirected or incorrectly received ARP message is discarded.
*
*               (3) A configurable ARP address filtering feature is provided to selectively filter & discard
*                   ALL misdirected or incorrectly received ARP messages (see 'net_cfg.h  ADDRESS RESOLUTION
*                   PROTOCOL LAYER CONFIGURATION  Note #3') :
*
*                   (a) When ENABLED,  the ARP address filter discards :
*
*                       (1) ALL misdirected, broadcasted, or incorrectly received ARP messages.
*
*                       (2) Any ARP Reply received for this host for which NO corresponding ARP cache currently
*                           exists.  Note that such an ARP Reply may be a legitimate, yet late, ARP Reply to a
*                           pending ARP Request that has timed-out & been removed from the ARP Cache List.
*
*                   (b) When DISABLED, the ARP address filter discards :
*
*                       (1) Any misdirected or incorrectly received ARP messages if the sender's address(s)
*                           are NOT already cached.
*               (4) (a) RFC # 826, Section 'Related issue' states that "perhaps receiving of a packet from a
*                       host should reset a timeout in the address resolution entry ... [but] this may cause
*                       extra overhead to scan the table for each incoming packet".
*
*                   (b) RFC #1122, Section 2.3.2.1 restates "that this timeout should be restarted when the
*                       cache entry is 'refreshed' ... by ... an ARP broadcast from the system in question".
*
*                   (c) However, Stevens, TCP/IP Illustrated, Volume 1, 8th Printing, Section 4.5 'ARP Examples :
*                       ARP Cache Timeout' adds that "the Host Requirements RFC [#1122] says that this timeout
*                       should occur even if the entry is in use, but most Berkeley-derived implementations do
*                       not do this -- they restart the timeout each time the entry is referenced".
*********************************************************************************************************
*/

static  void  NetARP_RxPktCacheUpdate (NET_IF_NBR    if_nbr,
                                       NET_ARP_HDR  *p_arp_hdr,
                                       NET_ERR      *p_err)
{
    CPU_INT16U           op_code                = 0u;
    CPU_BOOLEAN          cache_add              = DEF_NO;
    CPU_INT08U          *p_addr_sender_hw       = DEF_NULL;
    CPU_INT08U          *p_addr_sender_protocol = DEF_NULL;
    NET_CACHE_ADDR_ARP  *p_cache_addr_arp       = DEF_NULL;
    NET_ARP_CACHE       *p_cache_arp            = DEF_NULL;
    NET_BUF             *p_buf_head             = DEF_NULL;
    NET_TMR_TICK         timeout_tick           = 0u;
    CPU_SR_ALLOC();


                                                                /* ----------------- CHK TARGET ADDR ------------------ */
    NetARP_RxPktIsTargetThisHost(if_nbr, p_arp_hdr, p_err);
    if (NetARP_AddrFltrEn == DEF_YES) {
        if (*p_err != NET_ARP_ERR_RX_TARGET_THIS_HOST) {        /* Fltr misdirected rx'd ARP msgs (see Note #3b1).      */
             NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxPktInvDest);
             return;
        }
    } else {
        cache_add = (*p_err == NET_ARP_ERR_RX_TARGET_THIS_HOST) ? DEF_YES : DEF_NO;
    }


                                                                /* ------------------ SRCH ARP CACHE ------------------ */
    p_addr_sender_hw       = p_arp_hdr->AddrHW_Sender;
    p_addr_sender_protocol = p_arp_hdr->AddrProtocolSender;

    p_cache_addr_arp = (NET_CACHE_ADDR_ARP *)NetCache_AddrSrch(NET_CACHE_TYPE_ARP,
                                                              if_nbr,
                                                              p_addr_sender_protocol,
                                                              NET_IPv4_ADDR_SIZE,
                                                              p_err);

                                                                /* ----------------- UPDATE ARP CACHE ----------------- */
    if (p_cache_addr_arp != DEF_NULL) {                         /* If ARP cache found, chk state.                       */
        p_cache_arp = (NET_ARP_CACHE *)NetCache_GetParent((NET_CACHE_ADDR *)p_cache_addr_arp);
        switch (p_cache_arp->State) {
            case NET_ARP_CACHE_STATE_PEND:                      /* If ARP cache pend, add sender's hw addr, ...         */
                 Mem_Copy((void     *)&p_cache_addr_arp->AddrHW[0],
                          (void     *) p_addr_sender_hw,
                          (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

                 p_cache_addr_arp->AddrHW_Valid = DEF_YES;
                                                                /* Reset ARP cache tmr (see Note #4).                   */
                 CPU_CRITICAL_ENTER();
                 timeout_tick = NetARP_ReqTimeoutRenew_tick;
                 CPU_CRITICAL_EXIT();
                 NetTmr_Set((NET_TMR    *)p_cache_arp->TmrPtr,
                            (CPU_FNCT_PTR)NetARP_CacheRenewTimeout,
                            (NET_TMR_TICK)timeout_tick,
                            (NET_ERR    *)p_err);

                 p_buf_head                  =  p_cache_addr_arp->TxQ_Head;
                 p_cache_addr_arp->TxQ_Head  = (NET_BUF *)0;
                 p_cache_addr_arp->TxQ_Tail  = (NET_BUF *)0;
                 p_cache_addr_arp->TxQ_Nbr   = 0u;
                 p_cache_arp->ReqAttemptsCtr = 0u;              /* Reset request attempts counter.                      */

                 if (p_buf_head != DEF_NULL) {
                     NetCache_TxPktHandler(NET_PROTOCOL_TYPE_ARP,
                                           p_buf_head,          /* ... & handle/tx cache's buf Q.                       */
                                           p_addr_sender_hw);
                 }
                 p_cache_arp->State = NET_ARP_CACHE_STATE_RESOLVED;
                *p_err              = NET_ARP_ERR_CACHE_RESOLVED;
                 break;


            case NET_ARP_CACHE_STATE_RENEW:
            case NET_ARP_CACHE_STATE_RESOLVED:                  /* If ARP cache resolved, update sender's hw addr.      */
                 Mem_Copy((void     *)&p_cache_addr_arp->AddrHW[0],
                          (void     *) p_addr_sender_hw,
                          (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
                                                                /* Reset ARP cache tmr (see Note #4).                   */
                 CPU_CRITICAL_ENTER();
                 timeout_tick = NetARP_ReqTimeoutRenew_tick;
                 CPU_CRITICAL_EXIT();
                 NetTmr_Set((NET_TMR    *)p_cache_arp->TmrPtr,
                            (CPU_FNCT_PTR)NetARP_CacheRenewTimeout,
                            (NET_TMR_TICK)timeout_tick,
                            (NET_ERR    *)p_err);
                                                                /* If entry was found in RENEW state, transition it  ...*/
                 if (p_cache_arp->State == NET_ARP_CACHE_STATE_RENEW) { /* ...to RESOLVED & reset the request attempt...*/
                     p_cache_arp->State = NET_ARP_CACHE_STATE_RESOLVED; /* ...counter.                                  */
                     p_cache_arp->ReqAttemptsCtr = 0u;
                 }
                *p_err = NET_ARP_ERR_CACHE_RESOLVED;
                 break;


            case NET_ARP_CACHE_STATE_NONE:
            case NET_ARP_CACHE_STATE_FREE:
            default:
                 NetCache_Remove((NET_CACHE_ADDR *)p_cache_addr_arp, DEF_YES);
                 NetCache_AddResolved(if_nbr,
                                      p_addr_sender_hw,
                                      p_addr_sender_protocol,
                                      NET_CACHE_TYPE_ARP,
                                      NetARP_CacheRenewTimeout,
                                      NetARP_ReqTimeoutRenew_tick,
                                      p_err);
                 break;
        }

    } else {                                                    /* Else add new ARP cache into ARP Cache List.          */
        if (NetARP_AddrFltrEn == DEF_YES) {                     /* If ARP addr fltr en'd          ..                    */
            NET_UTIL_VAL_COPY_GET_NET_16(&op_code, &p_arp_hdr->OpCode);
            if (op_code != NET_ARP_HDR_OP_REQ) {                /* .. but ARP pkt NOT an ARP Req, ..                    */
                                                                /* .. do  NOT add new ARP cache (see Note #3a2).        */
                NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxPktTargetReplyCtr);
               *p_err = NET_ARP_ERR_RX_TARGET_REPLY;
                return;
            }
        } else {
                                                                /* If ARP addr fltr dis'd            ..                 */
            if (cache_add != DEF_YES) {                         /* .. &   ARP pkt NOT for this host, ..                 */
                                                                /* .. do  NOT add new ARP cache (see Note #3b1).        */
                NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxPktInvDest);
                return;                                         /* Err rtn'd by NetARP_RxPktIsTargetThisHost().         */
            }
        }

        NetCache_AddResolved(if_nbr,
                             p_addr_sender_hw,
                             p_addr_sender_protocol,
                             NET_CACHE_TYPE_ARP,
                             NetARP_CacheRenewTimeout,
                             NetARP_ReqTimeoutRenew_tick,
                             p_err);
    }
}


/*
*********************************************************************************************************
*                                          NetARP_RxPktReply()
*
* Description : Reply to received ARP message, if necessary.
*
* Argument(s) : if_nbr      Interface number the packet was received from.
*
*               p_arp_hdr   Pointer to received packet's ARP header.
*               --------    Argument validated in NetARP_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_RX_REQ_TX_REPLY             ARP Reply transmitted.
*                               NET_ARP_ERR_RX_REPLY_TX_PKTS            ARP Reply received; ARP cache transmit
*                                                                           queue packets already transmitted
*                                                                           (see Note #1).
*                               NET_ARP_ERR_INVALID_OP_CODE             Invalid ARP operation code.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Rx().
*
* Note(s)     : (1) ARP Reply already transmitted the ARP cache's transmit buffer queue, if any, in
*                   NetARP_RxPktCacheUpdate(); no further action required.
*
*               (2) Default case already invalidated in NetARP_RxPktValidate().  However, the default
*                   case is included as an extra precaution in case 'OpCode' is incorrectly modified.
*********************************************************************************************************
*/

static  void  NetARP_RxPktReply (NET_IF_NBR    if_nbr,
                                 NET_ARP_HDR  *p_arp_hdr,
                                 NET_ERR      *p_err)
{
    CPU_INT16U  op_code;


    NET_UTIL_VAL_COPY_GET_NET_16(&op_code, &p_arp_hdr->OpCode);

    switch (op_code) {
        case NET_ARP_HDR_OP_REQ:                                /* Use rx'd ARP Req to tx ARP Reply.                    */
             NetARP_TxReply(if_nbr, p_arp_hdr);
            *p_err = NET_ARP_ERR_RX_REQ_TX_REPLY;
             break;


        case NET_ARP_HDR_OP_REPLY:                              /* See Note #1.                                         */
            *p_err = NET_ARP_ERR_RX_REPLY_TX_PKTS;
             break;


        default:                                                /* See Note #2.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvOpCodeCtr);
            *p_err = NET_ARP_ERR_INVALID_OP_CODE;
             return;
    }
}


/*
*********************************************************************************************************
*                                    NetARP_RxPktIsTargetThisHost()
*
* Description : (1) Determine if this host is the intended target of the received ARP message :
*
*                   (a) Validate interface
*                   (b) (1) Get    target hardware address
*                       (2) Verify target hardware address                              See Note #2
*                   (c) (1) Get    target protocol address
*                       (2) Verify target protocol address :
*                           (A) Check for protocol initialization address
*                           (B) Check for protocol address conflict                     See Note #4
*                   (d) Return target validation
*
*
* Argument(s) : if_nbr      Interface number the packet was received from.
*
*               p_arp_hdr   Pointer to received packet's ARP header.
*               --------    Argument validated in NetARP_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_RX_TARGET_THIS_HOST         Received ARP message     intended
*                                                                           for this host.
*                               NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST     Received ARP message NOT intended
*                                                                           for this host.
*                               NET_ARP_ERR_INVALID_OP_CODE             Invalid ARP operation code.
*                               NET_ARP_ERR_INVALID_PROTOCOL_LEN        Invalid ARP protocol address length.
*
*                                                                       ---- RETURNED BY NetIF_IsValidHandler() : ----
*                               NET_IF_ERR_INVALID_IF                   Invalid network interface number.
*
*                                                                       - RETURNED BY NetMgr_GetHostAddrProtocol() : -
*                               NET_ERR_FAULT_UNKNOWN_ERR                       Interface's protocol address(s) NOT
*                                                                           successfully returned.
*                               NET_ERR_FAULT_NULL_PTR                    Argument(s) passed a NULL pointer.
*                               NET_MGR_ERR_INVALID_PROTOCOL            Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN        Invalid protocol address length.
*                               NET_MGR_ERR_ADDR_TBL_SIZE               Invalid protocol address table size.
* Return(s)   : none.
*
* Caller(s)   : NetARP_RxPktCacheUpdate().
*
* Note(s)     : (2) (a) ARP Request target hardware address previously verified in NetARP_RxPktValidate()
*                       as     a network interface broadcast address or directly-addressed to this host's
*                       ARP hardware address.
*
*                       See 'NetARP_RxPktValidate()  Note #3a'.
*
*                   (b) ARP Reply   target hardware address previously verified in NetARP_RxPktValidate()
*                       as NOT a network interface broadcast address but NOT yet verified as directly-
*                       addressed to this host's ARP hardware address.
*
*                       See 'NetARP_RxPktValidate()  Note #3b'.
*
*               (3) Default case already invalidated in NetARP_RxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'OpCode' is incorrectly modified.
*
*               (4) RFC #3927, Section 2.5 states that :
*
*                   (a) "If a host receives an ARP packet (request *or* reply) on an interface where" ...
*                       (1) "the 'sender hardware address' does not match the hardware address of
*                                 that interface, but"                                                ...
*                       (2) "the 'sender IP       address' is a IP address the host has configured for
*                                 that interface,"                                                    ...
*                   (b) "then this is a conflicting ARP packet, indicating an address conflict."
*
*                   See also 'NetARP_IsAddrProtocolConflict()  Note #3'.
*********************************************************************************************************
*/
static  void  NetARP_RxPktIsTargetThisHost (NET_IF_NBR    if_nbr,
                                            NET_ARP_HDR  *p_arp_hdr,
                                            NET_ERR      *p_err)
{
    CPU_INT16U          op_code;
    CPU_INT08U         *p_addr_target_hw;
    CPU_INT08U         *p_addr_sender_hw;
    CPU_INT08U         *p_addr_target_protocol;
    CPU_INT08U         *p_addr_sender_protocol;
    CPU_INT08U         *p_addr_host_protocol;
    CPU_INT08U          addr_if_hw[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U          addr_if_protocol_tbl[NET_IPv4_ADDR_SIZE * NET_IPv4_CFG_IF_MAX_NBR_ADDR];
    CPU_INT08U          addr_hw_len;
    CPU_INT08U          addr_protocol_len;
    CPU_INT08U          addr_protocol_tbl_qty;
    CPU_INT08U          addr_ix;
    CPU_INT16U          protocol_type_arp;
    NET_PROTOCOL_TYPE   protocol_type;
    CPU_BOOLEAN         target_hw_verifd;
    CPU_BOOLEAN         sender_hw_verifd;
    CPU_BOOLEAN         target_protocol_verifd;
    NET_ERR             err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif


                                                                /* ------------------- GET HW ADDR -------------------- */
    addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
    NetIF_AddrHW_GetHandler((NET_IF_NBR  ) if_nbr,
                            (CPU_INT08U *)&addr_if_hw[0],
                            (CPU_INT08U *)&addr_hw_len,
                            (NET_ERR    *)&err);
    if ( err != NET_IF_ERR_NONE) {
       *p_err  = NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST;
        return;
    }


                                                                /* -------------- VERIFY TARGET HW ADDR --------------- */
    if (NetARP_AddrFltrEn == DEF_YES) {
        NET_UTIL_VAL_COPY_GET_NET_16(&op_code, &p_arp_hdr->OpCode);
        switch (op_code) {
            case NET_ARP_HDR_OP_REQ:                            /* See Note #2a.                                        */
                 target_hw_verifd = DEF_YES;
                 break;


            case NET_ARP_HDR_OP_REPLY:                          /* See Note #2b.                                        */
                 p_addr_target_hw  = p_arp_hdr->AddrHW_Target;
                 target_hw_verifd = Mem_Cmp((void     *) p_addr_target_hw,
                                            (void     *)&addr_if_hw[0],
                                            (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
                 break;


            default:                                            /* See Note #3.                                         */
                 NET_CTR_ERR_INC(Net_ErrCtrs.ARP.RxInvOpCodeCtr);
                *p_err = NET_ARP_ERR_INVALID_OP_CODE;
                 return;
        }
    } else {
        target_hw_verifd = DEF_YES;
    }


                                                                /* ---------------- GET PROTOCOL ADDR ----------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&protocol_type_arp, &p_arp_hdr->AddrProtocolType);
    switch (protocol_type_arp) {
        case NET_ARP_PROTOCOL_TYPE_IP_V4:
             protocol_type = NET_PROTOCOL_TYPE_IP_V4;
             break;


        default:
             protocol_type = NET_PROTOCOL_TYPE_NONE;
             break;
    }

    addr_protocol_tbl_qty  = NET_IPv4_CFG_IF_MAX_NBR_ADDR;
    addr_protocol_len      = NET_IPv4_ADDR_SIZE;
    NetMgr_GetHostAddrProtocol((NET_IF_NBR       ) if_nbr,
                               (NET_PROTOCOL_TYPE) protocol_type,
                               (CPU_INT08U      *)&addr_if_protocol_tbl[0],
                               (CPU_INT08U      *)&addr_protocol_tbl_qty,
                               (CPU_INT08U      *)&addr_protocol_len,
                               (NET_ERR         *) p_err);
    switch (*p_err) {
        case NET_MGR_ERR_NONE:
                                                                /* ----------- VERIFY TARGET PROTOCOL ADDR ------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
             if (addr_protocol_len != NET_IPv4_ADDR_SIZE) {
                *p_err = NET_ARP_ERR_INVALID_PROTOCOL_LEN;
                 return;
             }
#endif

             addr_ix                =  0u;
             p_addr_target_protocol  =  p_arp_hdr->AddrProtocolTarget;
             p_addr_host_protocol    = &addr_if_protocol_tbl[addr_ix];
             target_protocol_verifd =  DEF_NO;

             while ((addr_ix                 < addr_protocol_tbl_qty) &&
                    (target_protocol_verifd != DEF_YES)) {

                 target_protocol_verifd = Mem_Cmp((void     *)p_addr_target_protocol,
                                                  (void     *)p_addr_host_protocol,
                                                  (CPU_SIZE_T)addr_protocol_len);

                 p_addr_host_protocol += addr_protocol_len;
                 addr_ix++;
             }

                                                                /* ------------ CHK PROTOCOL ADDR CONFLICT ------------ */
             p_addr_sender_hw  = p_arp_hdr->AddrHW_Sender;      /* Cmp sender's hw addr           (see Note #4a1A).     */
             sender_hw_verifd = Mem_Cmp((void     *) p_addr_sender_hw,
                                        (void     *)&addr_if_hw[0],
                                        (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

             if (sender_hw_verifd != DEF_YES) {                 /* If sender hw addr NOT verifd, ...                    */
                                                                /* ... cmp sender's protocol addr (see Note #4a1B).     */
                 p_addr_sender_protocol = &p_arp_hdr->AddrProtocolSender[0];
                 addr_protocol_len     =  p_arp_hdr->AddrProtocolLen;

                 NetMgr_ChkAddrProtocolConflict(if_nbr,
                                                protocol_type,
                                                p_addr_sender_protocol,
                                                addr_protocol_len,
                                               &err);
             }
             break;


        case NET_MGR_ERR_ADDR_CFG_IN_PROGRESS:                  /* ----------- VERIFY TARGET PROTOCOL ADDR ------------ */
             addr_protocol_len      = NET_IPv4_ADDR_SIZE;
             target_protocol_verifd = NetMgr_IsAddrProtocolInit((NET_PROTOCOL_TYPE) protocol_type,
                                                                (CPU_INT08U      *)&p_arp_hdr->AddrProtocolTarget[0],
                                                                (CPU_INT08U       ) addr_protocol_len);
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_ERR_FAULT_UNKNOWN_ERR:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_MGR_ERR_ADDR_TBL_SIZE:
        case NET_MGR_ERR_INVALID_PROTOCOL:
        case NET_MGR_ERR_INVALID_PROTOCOL_LEN:
        default:
             return;
    }


                                                                /* -------------- RTN TARGET VALIDATION --------------- */
   *p_err = ((target_hw_verifd       == DEF_YES) &&
            (target_protocol_verifd == DEF_YES)) ? NET_ARP_ERR_RX_TARGET_THIS_HOST :
                                                   NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST;
}


/*
*********************************************************************************************************
*                                         NetARP_RxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetARP_RxPktFree (NET_BUF  *p_buf)
{
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)0);
}


/*
*********************************************************************************************************
*                                        NetARP_RxPktDiscard()
*
* Description : On any ARP receive error(s), discard ARP packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetARP_RxPktDiscard (NET_BUF  *p_buf,
                                   NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.ARP.RxPktDisCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)pctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                             NetARP_Tx()
*
* Description : (1) Prepare & transmit an ARP Request or ARP Reply :
*
*                   (a) Get network buffer for ARP transmit packet
*                   (b) Prepare & transmit packet
*                   (c) Free      transmit packet buffer(s)
*                   (d) Update    transmit statistics
*
*
* Argument(s) : if_nbr                  Interface number to transmit ARP Request.
*
*               p_addr_hw_sender        Pointer to sender's hardware address (see Note #2).
*
*               p_addr_hw_target        Pointer to target's hardware address (see Note #2).
*
*               p_addr_protocol_sender  Pointer to sender's protocol address (see Note #2).
*
*               p_addr_protocol_target  Pointer to target's protocol address (see Note #2).
*
*               op_code                 ARP operation : Request or Reply.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                ARP packet successfully transmitted.
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*                               NET_ARP_ERR_INVALID_OP_CODE     Invalid ARP operation code.
*
*                                                               ------- RETURNED BY NetBuf_Get() : --------
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum
*                                                                   buffer size available.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*                               NET_BUF_ERR_INVALID_LEN         Invalid buffer length.
*
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               --- RETURNED BY NetARP_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               -------- RETURNED BY NetIF_Tx() : ---------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_TxReq(),
*               NetARP_TxReply(),
*               NetARP_TxReqGratuitous().
*
* Note(s)     : (2) ARP addresses MUST be in network-order.
*
*               (3) Assumes network buffer's protocol header size is large enough to accommodate ARP header
*                   size (see 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #1').
*
*               (4) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/
static  void  NetARP_Tx (NET_IF_NBR   if_nbr,
                         CPU_INT08U  *p_addr_hw_sender,
                         CPU_INT08U  *p_addr_hw_target,
                         CPU_INT08U  *p_addr_protocol_sender,
                         CPU_INT08U  *p_addr_protocol_target,
                         CPU_INT16U   op_code,
                         NET_ERR     *p_err)
{
    CPU_INT16U   msg_ix;
    CPU_INT16U   msg_ix_offset;
    NET_BUF     *p_buf;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTRS ------------------- */
    if (p_addr_hw_sender == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxPktDisCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (p_addr_protocol_sender == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxPktDisCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (p_addr_protocol_target == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxPktDisCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

                                                                /* ------------------- VALIDATE OP -------------------- */
    switch (op_code) {
        case NET_ARP_HDR_OP_REQ:                                /* For ARP Req, NULL hw addr ptr expected.              */
             break;


        case NET_ARP_HDR_OP_REPLY:
             if (p_addr_hw_target == (CPU_INT08U *)0) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
                 NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxPktDisCtr);
                *p_err = NET_ERR_FAULT_NULL_PTR;
                 return;
             }
             break;


        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxHdrOpCodeCtr);
             NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxPktDisCtr);
            *p_err = NET_ARP_ERR_INVALID_OP_CODE;
             return;
    }
#endif


                                                                /* --------------------- GET BUF ---------------------- */
#if 0
#if (NET_BUF_DATA_IX_TX < NET_ARP_HDR_SIZE)                     /* See Note #3.                                         */
    NetARP_TxPktDiscard((NET_BUF *)0,
                        (NET_ERR *)p_err);
    NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxInvBufIxCtr);
    return;
#endif
#endif

#if 0
    msg_ix   = NET_BUF_DATA_IX_TX - msg_size_hdr;
#else
    msg_ix = 0u;
    NetARP_TxIxDataGet(if_nbr,
                       NET_ARP_MSG_LEN_DATA,
                      &msg_ix,
                       p_err);
#endif

    p_buf   = NetBuf_Get((NET_IF_NBR     ) if_nbr,
                        (NET_TRANSACTION) NET_TRANSACTION_TX,
                        (NET_BUF_SIZE   ) NET_ARP_MSG_LEN_DATA,
                        (NET_BUF_SIZE   ) msg_ix,
                        (NET_BUF_SIZE  *)&msg_ix_offset,
                        (CPU_INT16U     ) NET_BUF_FLAG_NONE,
                        (NET_ERR       *) p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         return;
    }

    msg_ix += msg_ix_offset;

                                                                /* ---------------- PREPARE/TX ARP PKT ---------------- */
    NetARP_TxPktPrepareHdr(p_buf,
                           msg_ix,
                           p_addr_hw_sender,
                           p_addr_hw_target,
                           p_addr_protocol_sender,
                           p_addr_protocol_target,
                           op_code,
                           p_err);
    switch (*p_err) {
        case NET_ARP_ERR_NONE:
             NetIF_Tx(p_buf, p_err);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        default:
             NetARP_TxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* ---------- FREE TX PKT / UPDATE TX STATS ----------- */
    switch (*p_err) {                                           /* Chk err from NetIF_Tx().                             */
        case NET_IF_ERR_NONE:
             NetARP_TxPktFree(p_buf);
             NET_CTR_STAT_INC(Net_StatCtrs.ARP.TxMsgCtr);
            *p_err = NET_ARP_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.ARP.TxPktDisCtr);
                                                                /* Rtn err from NetIF_Tx().                             */
             return;


        default:
             NetARP_TxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                         NetARP_TxReply()
*
* Description : (1) Prepare & transmit an ARP Reply in response to an ARP Request :
*
*                   (a) Configure sender's hardware address as this interface's           hardware address
*                   (b) Configure target's hardware address from the ARP Request's sender hardware address
*                   (c) Configure sender's protocol address from the ARP Request's target protocol address
*                   (d) Configure target's protocol address from the ARP Request's sender protocol address
*                   (e) Configure ARP operation as ARP Reply
*
*
* Argument(s) : if_nbr      Interface number to transmit ARP Reply.
*
*               p_arp_hdr   Pointer to a packet's ARP header.
*               --------    Argument checked in NetARP_RxPktValidate().
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_RxPktReply().
*
* Note(s)     : (2) Do NOT need to verify success of ARP Reply since failure will cause timeouts & retries.
*********************************************************************************************************
*/

static  void  NetARP_TxReply (NET_IF_NBR    if_nbr,
                              NET_ARP_HDR  *p_arp_hdr)
{
    CPU_INT08U   addr_hw_sender[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U  *p_addr_hw_target;
    CPU_INT08U  *p_addr_protocol_sender;
    CPU_INT08U  *p_addr_protocol_target;
    CPU_INT08U   addr_hw_len;
    CPU_INT16U   op_code;
    NET_ERR      err;

                                                                /* Cfg ARP Reply from ARP Req (see Note #1).            */
    addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;
    NetIF_AddrHW_GetHandler((NET_IF_NBR  ) if_nbr,
                            (CPU_INT08U *)&addr_hw_sender[0],
                            (CPU_INT08U *)&addr_hw_len,
                            (NET_ERR    *)&err);
    if (err != NET_IF_ERR_NONE) {
        return;
    }

    p_addr_hw_target       = (CPU_INT08U *)&p_arp_hdr->AddrHW_Sender[0];
    p_addr_protocol_sender = (CPU_INT08U *)&p_arp_hdr->AddrProtocolTarget[0];
    p_addr_protocol_target = (CPU_INT08U *)&p_arp_hdr->AddrProtocolSender[0];

    op_code               =  NET_ARP_HDR_OP_REPLY;

    NetARP_Tx((NET_IF_NBR  ) if_nbr,
              (CPU_INT08U *)&addr_hw_sender[0],
              (CPU_INT08U *) p_addr_hw_target,
              (CPU_INT08U *) p_addr_protocol_sender,
              (CPU_INT08U *) p_addr_protocol_target,
              (CPU_INT16U  ) op_code,
              (NET_ERR    *)&err);

    if (err == NET_ARP_ERR_NONE) {
        NET_CTR_STAT_INC(Net_StatCtrs.ARP.TxMsgReplyCtr);
    }
}


/*
*********************************************************************************************************
*                                      NetARP_TxPktPrepareHdr()
*
* Description : (1) Prepare ARP packet header :
*
*                   (a) Update network buffer's index & length controls
*
*                   (b) Prepare the transmit packet's following ARP header fields :
*
*                       (1) Hardware  Type
*                       (2) Protocol  Type
*                       (3) Hardware  Address Length
*                       (4) Protocol  Address Length
*                       (5) Operation Code
*                       (6) Sender's  Hardware Address
*                       (7) Sender's  Protocol Address
*                       (8) Target's  Hardware Address
*                       (9) Target's  Protocol Address
*
*                   (c) Convert the following ARP header fields from host-order to network-order :
*
*                       (1) Hardware  Type
*                       (2) Protocol  Type
*                       (3) Operation Code
*
*                   (d) Configure ARP protocol address pointer                      See Note #2
*
*
* Argument(s) : p_buf                   Pointer to network buffer to prepare ARP packet.
*
*               msg_ix                  Buffer index to prepare ARP packet.
*               ------                  Argument checked in NetARP_Tx().
*
*               p_addr_hw_sender        Pointer to sender's hardware address (see Note #2).
*               ---------------         Argument checked in NetARP_Tx().
*
*               p_addr_hw_target        Pointer to target's hardware address (see Note #2).
*               ---------------         Argument checked in NetARP_Tx().
*
*               p_addr_protocol_sender  Pointer to sender's protocol address (see Note #2).
*               ---------------------   Argument checked in NetARP_Tx().
*
*               p_addr_protocol_target  Pointer to target's protocol address (see Note #2).
*               ---------------------   Argument checked in NetARP_Tx().
*
*               op_code                 ARP operation : Request or Reply.
*               -------                 Argument checked in NetARP_Tx().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                ARP packet successfully prepared.
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_buf' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Tx().
*
* Note(s)     : (2) ARP addresses MUST be in network-order.
*
*               (3) Some buffer controls were previously initialized in NetBuf_Get() when the buffer was
*                   allocated.  These buffer controls do NOT need to be re-initialized but are shown for
*                   completeness.
*********************************************************************************************************
*/

static  void  NetARP_TxPktPrepareHdr (NET_BUF     *p_buf,
                                      CPU_INT16U   msg_ix,
                                      CPU_INT08U  *p_addr_hw_sender,
                                      CPU_INT08U  *p_addr_hw_target,
                                      CPU_INT08U  *p_addr_protocol_sender,
                                      CPU_INT08U  *p_addr_protocol_target,
                                      CPU_INT16U   op_code,
                                      NET_ERR     *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_ARP_HDR  *p_arp_hdr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr                        = &p_buf->Hdr;
    p_buf_hdr->ARP_MsgIx             = (CPU_INT16U  )msg_ix;
    p_buf_hdr->ARP_MsgLen            = (CPU_INT16U  )NET_ARP_HDR_SIZE;
    p_buf_hdr->TotLen                = (NET_BUF_SIZE)p_buf_hdr->ARP_MsgLen;
    p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_ARP;
    p_buf_hdr->ProtocolHdrTypeIF_Sub =  NET_PROTOCOL_TYPE_ARP;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #3].                */
    p_buf_hdr->DataIx                =  NET_BUF_IX_NONE;
    p_buf_hdr->DataLen               =  0u;
#endif


                                                                /* ----------------- PREPARE ARP HDR ------------------ */
    p_arp_hdr    =  (NET_ARP_HDR *)&p_buf->DataPtr[p_buf_hdr->ARP_MsgIx];


                                                                /* ---------- PREPARE ARP HW/PROTOCOL TYPES ----------- */
    NET_UTIL_VAL_SET_NET_16(&p_arp_hdr->AddrHW_Type,      NET_ARP_HW_TYPE_ETHER);
    NET_UTIL_VAL_SET_NET_16(&p_arp_hdr->AddrProtocolType, NET_ARP_PROTOCOL_TYPE_IP_V4);

                                                                /* -------- PREPARE ARP HW/PROTOCOL ADDR LENS --------- */
    p_arp_hdr->AddrHW_Len      =  NET_IF_HW_ADDR_LEN_MAX;
    p_arp_hdr->AddrProtocolLen =  NET_IPv4_ADDR_SIZE;

                                                                /* --------------- PREPARE ARP OP CODE ---------------- */
    NET_UTIL_VAL_COPY_SET_NET_16(&p_arp_hdr->OpCode, &op_code);

                                                                /* ------- PREPARE ARP HW/PROTOCOL SENDER ADDRS ------- */
    Mem_Copy((void     *)&p_arp_hdr->AddrHW_Sender[0],
             (void     *) p_addr_hw_sender,
             (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

    Mem_Copy((void     *)&p_arp_hdr->AddrProtocolSender[0],
             (void     *) p_addr_protocol_sender,
             (CPU_SIZE_T) NET_IPv4_ADDR_SIZE);

                                                                /* ------- PREPARE ARP HW/PROTOCOL TARGET ADDRS ------- */
    if (p_addr_hw_target == (CPU_INT08U *)0) {                  /* If ARP target hw addr NULL for ARP Req, ...          */
        Mem_Clr( (void     *)&p_arp_hdr->AddrHW_Target[0],      /* .. clr target hw addr octets.                        */
                 (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);

        DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_BROADCAST); /* ARP Req broadcast to ALL hosts on local net.       */

    } else {                                                    /* Else copy target hw addr for ARP Reply.              */
        Mem_Copy((void     *)&p_arp_hdr->AddrHW_Target[0],
                 (void     *) p_addr_hw_target,
                 (CPU_SIZE_T) NET_IF_HW_ADDR_LEN_MAX);
                                                                /* ARP Reply tx'd directly to target host.              */
    }

    Mem_Copy((void     *)&p_arp_hdr->AddrProtocolTarget[0],
             (void     *) p_addr_protocol_target,
             (CPU_SIZE_T) NET_IPv4_ADDR_SIZE);

                                                                /* ------------ CFG ARP PROTOCOL ADDR PTR ------------- */
    p_buf_hdr->ARP_AddrProtocolPtr = &p_arp_hdr->AddrProtocolTarget[0];



   *p_err = NET_ARP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetARP_TxIxDataGet()
*
* Description : (1) Solves the starting index of the ARP data from the data buffer begining.
*
*               (2) Starting index if found by adding up the header sizes of the lower-level
*                   protocol headers.
*
*
* Argument(s) : if_nbr      Network interface number to transmit data.
*
*               hdr_len     Length of the ARP header.
*
*               data_len    Length of the ARP payload.
*
*               p_ix        Pointer to the current protocol index.
*               ----        Argument validated in NetARP_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ARP_ERR_NONE                No error.
*
*                               NET_ARP_ERR_INVALID_LEN         The payload is greater than the largest fragmentable
*                                                               IPv4 datagram.
*
*                               -Returned by NetIF_GetTxDataIx()-
*                               See NetIF_GetTxDataIx() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Tx().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetARP_TxIxDataGet (NET_IF_NBR   if_nbr,
                                  CPU_INT32U   data_len,
                                  CPU_INT16U  *p_ix,
                                  NET_ERR     *p_err)
{
    NET_MTU  mtu;


    mtu = NetIF_MTU_GetProtocol(if_nbr, NET_PROTOCOL_TYPE_ARP, NET_IF_FLAG_NONE, p_err);
    if (data_len > mtu) {
       *p_err = NET_ARP_ERR_INVALID_LEN_MSG;                     /* See Note #2.                                         */
        return;
    }


                                                                /* Add the lower-level hdr offsets.                     */
    NetIF_TxIxDataGet(if_nbr, data_len, p_ix, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

   *p_err = NET_ARP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetARP_TxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Tx().
*
* Note(s)     : (1) (a) Although ARP Transmit initially requests the network buffer for transmit,
*                       the ARP layer does NOT maintain a reference to the buffer.
*
*                   (b) Also, since the network interface deallocation task frees ALL unreferenced buffers
*                       after successful transmission, the ARP layer must NOT free the transmit buffer.
*
*                       See also 'net_if.c  NetIF_TxDeallocTaskHandler()  Note #1c'.
*********************************************************************************************************
*/

static  void  NetARP_TxPktFree (NET_BUF  *p_buf)
{
   (void)&p_buf;                                                 /* Prevent 'variable unused' warning (see Note #1).     */
}


/*
*********************************************************************************************************
*                                        NetARP_TxPktDiscard()
*
* Description : On any ARP transmit handler error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetARP_TxPktDiscard (NET_BUF  *p_buf,
                                   NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.ARP.TxPktDisCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)pctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                      NetARP_CacheReqTimeout()
*
* Description : Retry ARP Request to resolve an ARP cache in the 'PENDING' state on ARP Request timeout.
*
* Argument(s) : p_cache_timeout     Pointer to an ARP cache (see Note #1b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetARP_CacheAddPend().
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
*                           (A) in this case, a 'NET_ARP_CACHE' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetCache_AddrFree() via NetCache_Remove(); or
*                       (2) Reset   by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/
static  void  NetARP_CacheReqTimeout (void  *p_cache_timeout)
{
    NET_ARP_CACHE       *p_cache;
    NET_CACHE_ADDR_ARP  *p_arp;
    NET_TMR_TICK         timeout_tick;
    CPU_INT08U           th_max;
    NET_ERR              err;
    CPU_SR_ALLOC();


    p_cache = (NET_ARP_CACHE      *)p_cache_timeout;
    p_arp   = (NET_CACHE_ADDR_ARP *)p_cache->CacheAddrPtr;

    p_cache->TmrPtr = DEF_NULL;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ARP CACHE ---------------- */
    if (p_cache == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        return;
    }

    if (p_arp == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        return;
    }

    if (p_arp->Type != NET_CACHE_TYPE_ARP) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.InvTypeCtr);
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    switch (p_cache->State) {
        case NET_ARP_CACHE_STATE_RENEW:
             th_max       = NetARP_ReqMaxAttemptsRenew_nbr;
             timeout_tick = NetARP_ReqTimeoutRenew_tick;
             break;


        case NET_ARP_CACHE_STATE_PEND:
        default:
             th_max       = NetARP_ReqMaxAttemptsPend_nbr;
             timeout_tick = NetARP_ReqTimeoutPend_tick;
             break;
    }
    CPU_CRITICAL_EXIT();

    if (p_cache->ReqAttemptsCtr >= th_max) {                    /* If nbr attempts >= max, free ARP cache.              */
        NetCache_Remove((NET_CACHE_ADDR *)p_arp, DEF_NO);
        return;
    }

                                                                /* ------------------ RETRY ARP REQ ------------------- */
    p_cache->TmrPtr = NetTmr_Get((CPU_FNCT_PTR) NetARP_CacheReqTimeout,
                                 (void       *) p_cache,
                                 (NET_TMR_TICK) timeout_tick,
                                 (NET_ERR    *)&err);
    if (err != NET_TMR_ERR_NONE) {                              /* If tmr unavail, free ARP cache.                      */
        NetCache_Remove((NET_CACHE_ADDR *)p_arp, DEF_NO);
        return;
    }

                                                                /* ------------------ RE-TX ARP REQ ------------------- */
    NetARP_TxReq(p_arp);
}


/*
*********************************************************************************************************
*                                        NetARP_CacheRenewTimeout()
*
* Description : Renew an ARP cache in the 'RESOLVED' state on timeout.
*
* Argument(s) : p_cache_timeout      Pointer to an ARP cache (see Note #2b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetARP_CacheAddResolved().
*
* Note(s)     : (1) RFC #1122, Section 2.3.2.1 states that "an implementation of the Address Resolution
*                   Protocol (ARP) ... MUST provide a mechanism to flush out-of-date cache entries".
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
*                           (A) in this case, a 'NET_ARP_CACHE' pointer.
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

static  void  NetARP_CacheRenewTimeout (void  *p_cache_timeout)
{
    CPU_FNCT_PTR         fcnt;
    NET_TMR_TICK         timeout_tick;
    NET_ARP_CACHE       *p_cache;
    NET_CACHE_ADDR_ARP  *p_arp;
    CPU_BOOLEAN          tx_req;
    NET_ERR              err;
    CPU_SR_ALLOC();


    p_cache = (NET_ARP_CACHE *)p_cache_timeout;                 /* See Note #2b2A.                                      */

    p_cache->TmrPtr = DEF_NULL;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ARP CACHE ---------------- */
    if (p_cache == (NET_ARP_CACHE *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.NullPtrCtr);
        return;
    }

    if (p_cache->Type != NET_CACHE_TYPE_ARP) {
        NET_CTR_ERR_INC(Net_ErrCtrs.ARP.InvTypeCtr);
        return;
    }
#endif

    p_arp = (NET_CACHE_ADDR_ARP *)p_cache->CacheAddrPtr;

    if ((CPU_INT32U)p_arp->AddrProtocolSender[0] != NET_ARP_PROTOCOL_TYPE_NONE) {
        tx_req = DEF_YES;
        fcnt   = NetARP_CacheRenewTimeout;

        CPU_CRITICAL_ENTER();
        timeout_tick = NetARP_ReqTimeoutPend_tick;
        CPU_CRITICAL_EXIT();

        if (p_cache->State == NET_ARP_CACHE_STATE_RENEW) {
            if (p_cache->ReqAttemptsCtr >= NetARP_ReqMaxAttemptsRenew_nbr) { /* If max nbr of renew attempts reached... */
                fcnt         = NetARP_CacheReqTimeout;          /* ...invalidate entry by forcing its removal from  ... */
                tx_req       = DEF_NO;                          /* ... ARP cache thru NetARP_CacheReqTimeout() callback.*/
                timeout_tick = 0u;
            }
        }
    } else {
        NetCache_Remove((NET_CACHE_ADDR *)p_arp, DEF_NO);
        return;
    }


                                                                /* ------------------ RETRY ARP REQ ------------------- */
    p_cache->TmrPtr = NetTmr_Get(fcnt, p_cache, timeout_tick, &err);
    if (err != NET_TMR_ERR_NONE) {                              /* If tmr unavail, free ARP cache.                      */
                                                                /* Clr but do NOT free tmr (see Note #2).               */
        NetCache_Remove((NET_CACHE_ADDR *)p_cache->CacheAddrPtr, DEF_NO);
        return;
    }

    p_cache->State = NET_ARP_CACHE_STATE_RENEW;

    if (tx_req == DEF_YES) {
        NetARP_TxReq(p_cache->CacheAddrPtr);
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_ARP_MODULE_EN */

