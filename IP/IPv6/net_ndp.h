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
* Filename : net_ndp.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Neighbor Discovery Protocol as described in RFC #2461 with the
*                following restrictions/constraints :
*
*                (a) ONLY supports the following hardware types :
*                    (1) 48-bit Ethernet
*
*                (b) ONLY supports the following protocol types :
*                    (1) 128-bit IPv6 addresses
*
*                (c) IPv6 Stateless address Autoconfiguration not yet supported.
*
*                (d) IPv6 Duplication Address Detection not yet supported.
*
*                (e) Unsolicited Neighbor Advertisement Transmission.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) NDP Layer module is required for applications that requires IPv6 services.
*
*           (2) The following NDP-module-present configuration value MUST be pre-#define'd in
*               'net_cfg_net.h' PRIOR to all other network modules that require NDP Layer
*               configuration (see 'net_cfg_net.h  IP LAYER CONFIGURATION) :
*
*                   NET_NDP_MODULE_EN
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_NDP_MODULE_PRESENT
#define  NET_NDP_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../../Source/net_cfg_net.h"
#include  "../../Source/net_type.h"
#include  "../../Source/net_cache.h"
#include  "../../Source/net_buf.h"
#include  "net_icmpv6.h"

#ifdef   NET_NDP_MODULE_EN                                /* See Note #2.                                         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef NET_NDP_MODULE
#define  NET_NDP_EXT
#else
#define  NET_NDP_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_NDP_RX_ROUTER_ADV_SIGNAL_NAME              "Net NDP Rx Router Adv Signal"
#define  NET_NDP_RX_ROUTER_ADV_TIMEOUT_MS                500u
#define  NET_NDP_TX_ROUTER_SOL_RETRY_MAX                   3u

#define  NET_NDP_CACHE_TX_Q_TH_DFLT                        2


/*
*********************************************************************************************************
*                                            DAD DEFINES
*********************************************************************************************************
*/

#define  NET_NDP_CFG_DAD_MAX_NBR_ATTEMPTS       3u              /* Configured number of NDP DAD attempts.               */


/*
*********************************************************************************************************
*                                      NDP HEADER FLAGS DEFINES
*********************************************************************************************************
*/

#define  NET_NDP_HDR_FLAG_ROUTER                         DEF_BIT_31
#define  NET_NDP_HDR_FLAG_SOL                            DEF_BIT_30
#define  NET_NDP_HDR_FLAG_OVRD                           DEF_BIT_29

#define  NET_NDP_HDR_FLAG_ADDR_CFG_AUTO                  DEF_BIT_06
#define  NET_NDP_HDR_FLAG_ON_LINK                        DEF_BIT_07

#define  NET_NDP_HDR_FLAG_ADDR_CFG_OTHER                 DEF_BIT_06
#define  NET_NDP_HDR_FLAG_ADDR_CFG_MNGD                  DEF_BIT_07


/*
*********************************************************************************************************
*                                          NDP CACHE DEFINES
*********************************************************************************************************
*/

#define  NET_NDP_CACHE_ACCESSED_TH_MIN                    10
#define  NET_NDP_CACHE_ACCESSED_TH_MAX                 65000
#define  NET_NDP_CACHE_ACCESSED_TH_DFLT                  100

#define  NET_NDP_CACHE_TIMEOUT_MIN_SEC                   ( 1  *  DEF_TIME_NBR_SEC_PER_MIN)  /* Timeout min  =  1 min    */
#define  NET_NDP_CACHE_TIMEOUT_MAX_SEC                   (10  *  DEF_TIME_NBR_SEC_PER_MIN)  /* Timeout max  = 10 mins   */
#define  NET_NDP_CACHE_TIMEOUT_DFLT_SEC                  (10  *  DEF_TIME_NBR_SEC_PER_MIN)  /* Timeout dflt = 10 mins   */


/*
*********************************************************************************************************
*                                         NDP GENERAL DEFINES
*********************************************************************************************************
*/
                                                                /* ------------------ NODE CONSTANTS ------------------ */
#define  NET_NDP_SOLICIT_NBR_MIN                           0    /* Solicitations retries Min.                           */
#define  NET_NDP_SOLICIT_NBR_MAX                           5    /* Solicitations retries Max.                           */
#define  NET_NDP_SOLICIT_MAX_MULTICAST                     3    /* Multicast Solicitations retries Max.                 */
#define  NET_NDP_SOLICIT_MAX_UNICAST                       3    /* Unicast   Solicitations retries Max.                 */

                                                                /* Neighbor Unreachability Detection Timeouts.          */
#define  NET_NDP_REACHABLE_TIMEOUT_SEC                    30    /* Neighbor reachability timeout.                       */
#define  NET_NDP_REACHABLE_TIMEOUT_MIN_SEC                 1    /* NDP reachability timeout min  =  1 second            */
#define  NET_NDP_REACHABLE_TIMEOUT_MAX_SEC               120    /* NDP reachability timeout max  = 120 seconds          */
#define  NET_NDP_DELAY_FIRST_PROBE_TIMEOUT_SEC             3    /* Delay before first Probe.                            */
#define  NET_NDP_DELAY_FIRST_PROBE_TIMEOUT_MIN_SEC         1    /* NDP Delay First Probe timeout min  =  1 second       */
#define  NET_NDP_DELAY_FIRST_PROBE_TIMEOUT_MAX_SEC        10    /* NDP Delay First Probe timeout max  = 10 seconds      */
#define  NET_NDP_RETRANS_TIMEOUT_SEC                       1    /* Retransmit timeout.                                  */
#define  NET_NDP_RETRANS_TIMEOUT_MIN_SEC                   1    /* NDP Retransmit timeout min  =  1 second              */
#define  NET_NDP_RETRANS_TIMEOUT_MAX_SEC                  10    /* NDP Retransmit timeout max  = 10 seconds             */


/*
*********************************************************************************************************
*                                      NDP MESSAGE SIZE DEFINES
*********************************************************************************************************
*/

#define  NET_NDP_HDR_SIZE_NEIGHBOR_SOL                    24
#define  NET_NDP_MSG_LEN_MIN_NEIGHBOR_SOL                NET_ICMPv6_HDR_SIZE_DFLT
#define  NET_NDP_MSG_LEN_MAX_NEIGHBOR_SOL                NET_ICMPv6_MSG_LEN_MAX_NONE

#define  NET_NDP_HDR_SIZE_NEIGHBOR_ADV                    32
#define  NET_NDP_MSG_LEN_MIN_NEIGHBOR_ADV                NET_ICMPv6_HDR_SIZE_DFLT
#define  NET_NDP_MSG_LEN_MAX_NEIGHBOR_ADV                NET_ICMPv6_MSG_LEN_MAX_NONE

#define  NET_NDP_HDR_SIZE_ROUTER_SOL                      16
#define  NET_NDP_MSG_LEN_MIN_ROUTER_SOL                  NET_ICMPv6_HDR_SIZE_DFLT
#define  NET_NDP_MSG_LEN_MAX_ROUTER_SOL                  NET_ICMPv6_MSG_LEN_MAX_NONE

#define  NET_NDP_HDR_SIZE_ROUTER_ADV                      32
#define  NET_NDP_MSG_LEN_MIN_ROUTER_ADV                  NET_ICMPv6_HDR_SIZE_DFLT
#define  NET_NDP_MSG_LEN_MAX_ROUTER_ADV                  NET_ICMPv6_MSG_LEN_MAX_NONE

#define  NET_NDP_HDR_SIZE_REDIRECT                        48
#define  NET_NDP_MSG_LEN_MIN_REDIRECT                    NET_ICMPv6_HDR_SIZE_DFLT
#define  NET_NDP_MSG_LEN_MAX_REDIRECT                    NET_ICMPv6_MSG_LEN_MAX_NONE


/*
*********************************************************************************************************
*                                      NDP OPTION TYPES DEFINES
*********************************************************************************************************
*/

#define  NET_NDP_OPT_TYPE_NONE                             0u
#define  NET_NDP_OPT_TYPE_ADDR_SRC                         1u
#define  NET_NDP_OPT_TYPE_ADDR_TARGET                      2u
#define  NET_NDP_OPT_TYPE_PREFIX_INFO                      3u
#define  NET_NDP_OPT_TYPE_REDIRECT                         4u
#define  NET_NDP_OPT_TYPE_MTU                              5u


/*
*********************************************************************************************************
*                                     NDP CACHE STATES DEFINES
*********************************************************************************************************
*/

#define  NET_NDP_CACHE_STATE_NONE                          0u
#define  NET_NDP_CACHE_STATE_INCOMPLETE                    1u
#define  NET_NDP_CACHE_STATE_REACHABLE                     2u
#define  NET_NDP_CACHE_STATE_STALE                         3u
#define  NET_NDP_CACHE_STATE_DLY                           4u
#define  NET_NDP_CACHE_STATE_PROBE                         5u


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_NDP_OPT_TYPE;
typedef  CPU_INT08U  NET_NDP_OPT_LEN;
typedef  CPU_INT16U  NET_NDP_OPT_RESERVED;


/*
*********************************************************************************************************
*                                    NDP CACHE QUANTITY DATA TYPE
*
* Note(s) : (1) NET_NDP_CACHE_NBR_MAX  SHOULD be #define'd based on 'NET_NDP_CACHE_QTY' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_NDP_CACHE_QTY;                         /* Defines max qty of NDP caches to support.            */

#define  NET_NDP_CACHE_NBR_MIN                             1
#define  NET_NDP_CACHE_NBR_MAX                           DEF_INT_16U_MAX_VAL    /* See Note #1.                         */


/*
*********************************************************************************************************
*                                        NDP TIMEOUT DATA TYPE
*********************************************************************************************************
*/

typedef enum net_ndp_timeout {
    NET_NDP_TIMEOUT_REACHABLE,
    NET_NDP_TIMEOUT_DELAY,
    NET_NDP_TIMEOUT_SOLICIT,
} NET_NDP_TIMEOUT;


/*
*********************************************************************************************************
*                                  NDP SOLICITATION MESSAGE DATA TYPE
*********************************************************************************************************
*/

typedef enum net_ndp_solicit {
    NET_NDP_SOLICIT_MULTICAST,
    NET_NDP_SOLICIT_UNICAST,
    NET_NDP_SOLICIT_DAD,
} NET_NDP_SOLICIT;


/*
*********************************************************************************************************
*                                       NDP HEADERS DATA TYPE
*
* Note(s) : (1) See RFC #2461 for NDP message header formats.
*********************************************************************************************************
*/

                                                                /* -------- NET NDP GENERIC OPTION FIELD IN HDR ------- */
typedef  struct  net_ndp_opt_hdr {
    NET_NDP_OPT_TYPE  Type;                                     /* NDP opt type.                                        */
    NET_NDP_OPT_LEN   Len;                                      /* NDP opt len.                                         */
} NET_NDP_OPT_HDR;


                                                                /* ------------- NET NDP NEIGHBOR SOL HDR ------------- */
typedef  struct  net_ndp_neighbor_sol_hdr {
    CPU_INT08U      Type;                                       /* NDP msg type.                                        */
    CPU_INT08U      Code;                                       /* NDP msg code.                                        */
    CPU_INT16U      ChkSum;                                     /* NDP msg chk sum.                                     */
    CPU_INT32U      Reserved;                                   /* NDP reserved bits.                                   */
    NET_IPv6_ADDR   TargetAddr;                                 /* NDP target addr.                                     */
    CPU_INT08U      Opt;                                        /* NDP opt data.                                        */
} NET_NDP_NEIGHBOR_SOL_HDR;

                                                                /* ------------- NET NDP NEIGHBOR ADV HDR ------------- */
typedef  struct  net_ndp_neighbor_adv_hdr {
    CPU_INT08U      Type;                                       /* NDP msg type.                                        */
    CPU_INT08U      Code;                                       /* NDP msg code.                                        */
    CPU_INT16U      ChkSum;                                     /* NDP msg chk sum.                                     */
    CPU_INT32U      Flags;                                      /* NDP flags.                                           */
    NET_IPv6_ADDR   TargetAddr;                                 /* NDP target addr.                                     */
    CPU_INT08U      Opt;                                        /* NDP opt data.                                        */
} NET_NDP_NEIGHBOR_ADV_HDR;

                                                                /* -------------- NET NDP ROUTER SOL HDR -------------- */
typedef  struct  net_ndp_router_sol_hdr {
    CPU_INT08U        Type;                                     /* NDP msg type.                                        */
    CPU_INT08U        Code;                                     /* NDP msg code.                                        */
    CPU_INT16U        ChkSum;                                   /* NDP msg chk sum.                                     */
    CPU_INT32U        Reserved;                                 /* NDP reserved bits.                                   */
    CPU_INT08U        Opt;                                      /* NDP opt data.                                        */
} NET_NDP_ROUTER_SOL_HDR;

                                                                /* -------------- NET NDP ROUTER ADV HDR -------------- */
typedef  struct  net_ndp_router_adv_hdr {
    CPU_INT08U        Type;                                     /* NDP msg type.                                        */
    CPU_INT08U        Code;                                     /* NDP msg code.                                        */
    CPU_INT16U        ChkSum;                                   /* NDP msg chk sum.                                     */
    CPU_INT08U        HopLimit;                                 /* NDP current hop limit.                               */
    CPU_INT08U        Flags;                                    /* NDP flags.                                           */
    CPU_INT16U        RouterLifetime;                           /* NDP router life time.                                */
    CPU_INT32U        ReachableTime;                            /* NDP reachable time.                                  */
    CPU_INT32U        ReTxTmr;                                  /* NDP re-tx timer.                                     */
    CPU_INT08U        Opt;
} NET_NDP_ROUTER_ADV_HDR;

                                                                /* --------------- NET NDP REDIRECT HDR --------------- */
typedef  struct  net_ndp_redirect_hdr {
    CPU_INT08U        Type;                                     /* NDP msg type.                                        */
    CPU_INT08U        Code;                                     /* NDP msg code.                                        */
    CPU_INT16U        ChkSum;                                   /* NDP msg chk sum.                                     */
    CPU_INT32U        Reserved;                                 /* NDP reserved bits.                                   */
    NET_IPv6_ADDR     AddrTarget;                               /* NDP target      addr.                                */
    NET_IPv6_ADDR     AddrDest;                                 /* NDP destination addr.                                */
    CPU_INT08U        Opt;                                      /* NDP opt data.                                        */
} NET_NDP_REDIRECT_HDR;


#define  NET_NDP_OPT_DATA_OFFSET                        sizeof(NET_NDP_OPT_HDR)


/*
*********************************************************************************************************
*                                  NDP NEIGHBOR CACHE ENTRY DATA TYPE
*********************************************************************************************************
*/

                                                                /* -------------- NET NDP NEIGHBOR CACHE -------------- */
typedef struct  net_ndp_neighbor_cache {
    NET_CACHE_TYPE           Type;
    NET_CACHE_ADDR_NDP      *CacheAddrPtr;                      /* Ptr to          NDP addr   cache.                    */
    NET_TMR                 *TmrPtr;                            /* Ptr to neighbor cache TMR.                           */
    CPU_INT08U               ReqAttemptsCtr;                    /* NDP req attempts ctr.                                */
    CPU_INT08U               State;                             /* NDP neighbor cache state.                            */
    CPU_INT16U               Flags;                             /* NDP neighbor cache flags.                            */
} NET_NDP_NEIGHBOR_CACHE;


/*
*********************************************************************************************************
*                                      ROUTER ENTRY DATA TYPE
*********************************************************************************************************
*/
                                                                /* -------------------- NET ROUTER -------------------- */
typedef  struct  net_ndp_router  NET_NDP_ROUTER;

struct  net_ndp_router {
NET_NDP_ROUTER          *PrevPtr;                               /* Pointer to previous NDP router entry.                */
NET_NDP_ROUTER          *NextPtr;                               /* Pointer to next     NDP router entry.                */

NET_IF_NBR               IF_Nbr;                                /* Interface number associated with router.             */

NET_IPv6_ADDR            Addr;                                  /* IPv6 address of router.                              */

CPU_BOOLEAN              RoundRobin;                            /* Indicate if router is currently selected for the ... */
                                                                /* ... Round-Robin.                                     */

CPU_INT16U               LifetimeSec;                           /* Router's lifetime in seconds.                        */

NET_TMR                 *TmrPtr;                                /* Pointer to router Timer.                             */

NET_NDP_NEIGHBOR_CACHE  *NDP_CachePtr;                          /* Pointer to Neighbor cache entry link with router.    */
};


/*
*********************************************************************************************************
*                                      PREFIX ENTRY DATA TYPE
*********************************************************************************************************
*/
                                                                /* -------------------- NET ROUTER -------------------- */
typedef  struct  net_ndp_prefix  NET_NDP_PREFIX;

struct  net_ndp_prefix {
NET_NDP_PREFIX       *PrevPtr;                                  /* Pointer to previous NDP prefix entry.                */
NET_NDP_PREFIX       *NextPtr;                                  /* Pointer to next     NDP prefix entry.                */

NET_IF_NBR            IF_Nbr;                                   /* Interface number associated with prefix.             */

NET_IPv6_ADDR         Prefix;                                   /* Prefix IPv6 address.                                 */

CPU_INT08U            PrefixLen;                                /* Prefix length.                                       */

NET_TMR              *TmrPtr;                                   /* Pointer to prefix Timer.                             */
};


/*
*********************************************************************************************************
*                                    DESTINATION ENTRY DATA TYPE
*********************************************************************************************************
*/
                                                                /* -------------------- NET ROUTER -------------------- */
typedef  struct  net_ndp_dest_cache  NET_NDP_DEST_CACHE;

struct  net_ndp_dest_cache {
NET_NDP_DEST_CACHE   *PrevPtr;                                  /* Pointer to previous NDP destination cache entry.     */
NET_NDP_DEST_CACHE   *NextPtr;                                  /* Pointer to next     NDP destination cache entry.     */

NET_IF_NBR            IF_Nbr;                                   /* Interface number associated with destination cache.  */

NET_IPv6_ADDR         AddrDest;                                 /* IPv6 destination address.                            */

NET_IPv6_ADDR         AddrNextHop;                              /* IPv6 Next-Hop address for final destination.         */

CPU_BOOLEAN           OnLink;                                   /* On-Link status.                                      */

CPU_BOOLEAN           IsValid;                                  /* Valid destination address status.                    */
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/


NET_NDP_EXT  CPU_INT16U       NetNDP_NeighborCacheTimeout_sec;  /* NDP Neighbor cache timeout     (in secs ).           */
NET_NDP_EXT  NET_TMR_TICK     NetNDP_NeighborCacheTimeout_tick; /* NDP Neighbor cache timeout     (in ticks).           */

NET_NDP_EXT  CPU_INT16U       NetNDP_DelayTimeout_sec;          /* NDP Neighbor delay timeout     (in secs ).           */
NET_NDP_EXT  NET_TMR_TICK     NetNDP_DelayTimeout_tick;         /* NDP Neighbor delay timeout     (in ticks).           */

NET_NDP_EXT  CPU_INT16U       NetNDP_CacheAccessedTh_nbr;       /* Nbr successful srch's to promote NDP cache.          */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             PUBLIC API
*********************************************************************************************************
*/

                                                                /* --------- NDP NEIGHBOR CACHE CFG FUNCTIONS --------- */

CPU_BOOLEAN          NetNDP_CfgNeighborCacheTimeout  (CPU_INT16U                timeout_sec);

CPU_BOOLEAN          NetNDP_CfgReachabilityTimeout   (NET_NDP_TIMEOUT           timeout_type,
                                                      CPU_INT16U                timeout_sec);

CPU_BOOLEAN          NetNDP_CfgSolicitMaxNbr         (NET_NDP_SOLICIT           solicit_type,
                                                      CPU_INT08U                max_nbr);

CPU_BOOLEAN          NetNDP_CfgCacheTxQ_MaxTh        (NET_BUF_QTY               nbr_buf_max);


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void                        NetNDP_Init                     (       NET_ERR                  *p_err);

                                                                /* ---------- NDP NEIGHBOR CACHE FUNCTIONS ----------- */
void                        NetNDP_NeighborCacheHandler     (const  NET_BUF                  *p_buf,
                                                                    NET_ERR                  *p_err);


                                                                /* -------------------- RX FUNCTIONS ------------------- */
void                        NetNDP_Rx                       (const  NET_BUF                   *p_buf,
                                                                    NET_BUF_HDR               *p_buf_hdr,
                                                             const  NET_ICMPv6_HDR            *p_icmp_hdr,
                                                                    NET_ERR                   *p_err);


                                                                /* ------------------- TX FUNCTIONS ------------------- */
void                        NetNDP_TxRouterSolicitation     (       NET_IF_NBR                if_nbr,
                                                                    NET_IPv6_ADDR            *p_addr_src,
                                                                    NET_ERR                  *p_err);


                                                                /* ------------------ NDP FIND ROUTE ------------------ */
const  NET_IPv6_ADDR       *NetNDP_NextHopByIF              (       NET_IF_NBR                if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr_dest,
                                                                    NET_ERR                  *p_err);

const  NET_IPv6_ADDR       *NetNDP_NextHop                  (       NET_IF_NBR               *p_if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr_src,
                                                             const  NET_IPv6_ADDR            *p_addr_dest,
                                                                    NET_ERR                  *p_err);


                                                                /* ------- NDP NEIGHBOR CACHE TIMEOUT FUNCTIONS ------- */
void                        NetNDP_CacheTimeout             (       void                     *p_cache_timeout);

void                        NetNDP_DelayTimeout             (       void                     *p_cache_timeout);


                                                                /* ---------------- NDP DAD FUNCTIONS ----------------- */

#ifdef NET_DAD_MODULE_EN
void                        NetNDP_DAD_Start                (       NET_IF_NBR                if_nbr,
                                                                    NET_IPv6_ADDR            *p_addr,
                                                                    NET_ERR                  *p_err);

void                        NetNDP_DAD_Stop                 (       NET_IF_NBR                if_nbr,
                                                                    NET_IPv6_ADDR            *p_addr);


CPU_INT08U                  NetNDP_DAD_GetMaxAttemptsNbr    (       void);
#endif

                                                                /* ---------- RX ROUTER ADV SIGNAL FUNCTIONS ---------- */
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
KAL_SEM_HANDLE             *NetNDP_RouterAdvSignalCreate    (       NET_IF_NBR               if_nbr,
                                                                    NET_ERR                  *p_err);

void                        NetNDP_RouterAdvSignalPend      (       KAL_SEM_HANDLE           *p_signal,
                                                                    NET_ERR                  *p_err);

void                        NetNDP_RouterAdvSignalRemove    (       KAL_SEM_HANDLE           *p_signal);
#endif

                                                                /* -------------- IxANVL TEST FUNCTIONS --------------- */
NET_NDP_PREFIX             *NetNDP_PrefixAddCfg             (       NET_IF_NBR                if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr_prefix,
                                                                    CPU_INT08U                prefix_len,
                                                                    CPU_BOOLEAN               timer_en,
                                                                    CPU_FNCT_PTR              timeout_fnct,
                                                                    NET_TMR_TICK              timeout_tick,
                                                                    NET_ERR                  *p_err);

NET_NDP_DEST_CACHE         *NetNDP_DestCacheAddCfg          (       NET_IF_NBR                if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr_dest,
                                                             const  NET_IPv6_ADDR            *p_addr_next_hop,
                                                                    CPU_BOOLEAN               is_valid,
                                                                    CPU_BOOLEAN               on_link,
                                                                    NET_ERR                  *p_err);

void                        NetNDP_DestCacheRemoveCfg       (       NET_IF_NBR                if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr_dest,
                                                                    NET_ERR                  *p_err);

void                        NetNDP_CacheClrAll              (       void);

CPU_INT08U                  NetNDP_CacheGetState            (       NET_IF_NBR                if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr,
                                                                    NET_ERR                  *p_err);

CPU_BOOLEAN                 NetNDP_CacheGetIsRouterFlagState(       NET_IF_NBR                if_nbr,
                                                             const  NET_IPv6_ADDR            *p_addr,
                                                                    NET_ERR                  *p_err);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_NDP_CFG_CACHE_NBR
#error  "NET_NDP_CFG_CACHE_NBR                         not #define'd in 'net_cfg.h'  "
#error  "                                        [MUST be  >= NET_NDP_CACHE_NBR_MIN] "
#error  "                                        [     &&  <= NET_NDP_CACHE_NBR_MAX] "

#elif  ((NET_NDP_CFG_CACHE_NBR < NET_NDP_CACHE_NBR_MIN) || \
        (NET_NDP_CFG_CACHE_NBR > NET_NDP_CACHE_NBR_MAX))
#error  "NET_NDP_CFG_CACHE_NBR                   illegally #define'd in 'net_cfg.h'  "
#error  "                                        [MUST be  >= NET_NDP_CACHE_NBR_MIN] "
#error  "                                        [     &&  <= NET_NDP_CACHE_NBR_MAX] "
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_NDP_MODULE_EN       */
#endif  /* NET_NDP_MODULE_PRESENT  */
