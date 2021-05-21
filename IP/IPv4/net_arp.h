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
* Filename : net_arp.h
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
*                                               MODULE
*
* Note(s) : (1) ARP Layer module is required for some network interfaces (see 'net_arp.h  Note #1').
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_ARP_MODULE_PRESENT                                 /* See Note #2b.                                        */
#define  NET_ARP_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../../Source/net_cfg_net.h"
#include  "../../Source/net_cache.h"
#include  "../../Source/net_tmr.h"
#include  "../../Source/net_buf.h"
#include  "../../Source/net_err.h"
#include  "../../Source/net_stat.h"

#ifdef   NET_ARP_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef NET_ARP_MODULE
#define  NET_ARP_EXT
#else
#define  NET_ARP_EXT  extern
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


#define  NET_ARP_CACHE_TIMEOUT_DFLT_SEC          (10  *  DEF_TIME_NBR_SEC_PER_MIN)  /* Cache timeout dflt = 10 min      */


#define  NET_ARP_REQ_RETRY_DFLT                            3
#define  NET_ARP_CACHE_TX_Q_TH_DFLT                        2                        /* See Note #1b.                    */
#define  NET_ARP_CACHE_ACCESSED_TH_MIN                    10
#define  NET_ARP_CACHE_ACCESSED_TH_MAX                 65000
#define  NET_ARP_CACHE_ACCESSED_TH_DFLT                  100
#define  NET_ARP_REQ_RETRY_TIMEOUT_DFLT_SEC                5    /* ARP req retry timeout dflt =  5 sec                  */


#define  NET_ARP_PROTOCOL_TYPE_NONE                   0x0000u




/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          ARP CACHE STATES
*
*                          (1a)                            (1b)
*                            ARP REQUEST                     ARP REPLY
*              ----------    FETCHES NEW     -----------     RESOLVES       ------------
*              |        |     ARP CACHE      |         |     ARP CACHE      |          |
*              |  FREE  | -----------------> | PENDING | -----------------> | RESOLVED |
*              |        |                    |         |                    |          |
*              ----------  (1c)              -----------                    ------------
*                 ^  ^       ARP REQUEST          |                               |
*                 |  |        TIMES OUT           |                               |
*                 |  |      AFTER RETRIES         |        (1d)                   |
*                 |  ------------------------------          ARP CACHE            |
*                 |                                          TIMES OUT            |
*                 -----------------------------------------------------------------
*
*
* Note(s) : (1) (a) ARP cache lookup fails to find the ARP cache with corresponding protocol address.
*                   A new ARP cache is allocated from the ARP cache pool & inserted into the ARP Cache
*                   List in the 'PENDING' state.  An ARP Request is generated & transmitted to resolve
*                   the pending ARP cache.
*
*               (b) An ARP Reply resolves the pending ARP cache's hardware address.
*
*               (c) Alternatively, no corresponding ARP Reply is received after the maximum number of
*                   ARP Request retries & the ARP cache is freed.
*
*               (d) ARP cache times out & is freed.
*********************************************************************************************************
*/

typedef  enum  net_app_cache_state {
    NET_ARP_CACHE_STATE_NONE,
    NET_ARP_CACHE_STATE_FREE,
    NET_ARP_CACHE_STATE_PEND,
    NET_ARP_CACHE_STATE_RENEW,
    NET_ARP_CACHE_STATE_RESOLVED
} NET_ARP_CACHE_STATE;


/*
*********************************************************************************************************
*                                    ARP CACHE QUANTITY DATA TYPE
*
* Note(s) : (1) NET_ARP_CACHE_NBR_MAX  SHOULD be #define'd based on 'NET_ARP_CACHE_QTY' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_ARP_CACHE_QTY;                         /* Defines max qty of ARP caches to support.            */

#define  NET_ARP_CACHE_NBR_MIN                             1
#define  NET_ARP_CACHE_NBR_MAX           DEF_INT_16U_MAX_VAL    /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                    ARP ADDRESS LENGTH DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_ARP_ADDR_LEN;


/*
*********************************************************************************************************
*                                         ARP FLAGS DATA TYPE
*********************************************************************************************************
*/

typedef  NET_FLAGS   NET_ARP_FLAGS;


/*
*********************************************************************************************************
*                                            ARP HEADER
*
* Note(s) : (1) See RFC #826, Section 'Packet Format' for ARP packet header format.
*
*           (2) See 'ARP HEADER / MESSAGE DEFINES'.
*********************************************************************************************************
*/

                                                                        /* --------------- NET ARP HDR ---------------- */
typedef  struct  net_arp_hdr {
    CPU_INT16U      AddrHW_Type;                                        /* ARP pkt hw       type.                       */
    CPU_INT16U      AddrProtocolType;                                   /* ARP pkt protocol type.                       */

    CPU_INT08U      AddrHW_Len;                                         /* ARP pkt hw       addr len (in octets).       */
    CPU_INT08U      AddrProtocolLen;                                    /* ARP pkt protocol addr len (in octets).       */

    CPU_INT16U      OpCode;                                             /* ARP op code (see Note #2).                   */

    CPU_INT08U      AddrHW_Sender[NET_IF_HW_ADDR_LEN_MAX];              /* Sender hw       addr.                        */

                                                                        /* Sender protocol addr.                        */
    CPU_INT08U      AddrProtocolSender[NET_IPv4_ADDR_SIZE];

    CPU_INT08U      AddrHW_Target[NET_IF_HW_ADDR_LEN_MAX];              /* Target hw       addr.                        */

                                                                        /* Target protocol addr.                        */
    CPU_INT08U      AddrProtocolTarget[NET_IPv4_ADDR_SIZE];
} NET_ARP_HDR;


/*
*********************************************************************************************************
*                                      ARP CACHE ENTRY DATA TYPE
*
*                                  NET_ARP_CACHE
*                                 |-------------|
*                                 | Cache Type  |
*                      Previous   |-------------|
*                       Cache <----------O      |
*                                 |-------------|     Next
*                                 |      O----------> Cache      Buffer Queue
*                                 |-------------|                    Head       -------
*                                 |      O------------------------------------> |     |
*                                 |-------------|                               |     |
*                                 |      O----------------------                -------
*                                 |-------------|              |                  | ^
*                                 |      O----------> Cache    |                  v |
*                                 |-------------|     Timer    |                -------
*                                 |  Hardware:  |              |                |     |
*                                 |    Type     |              |                |     |
*                                 |   Length    |              |                -------
*                                 |   Address   |              |                  | ^
*                                 |-------------|              | Buffer Queue     v |
*                                 |  Protocol:  |              |     Tail       -------
*                                 |    Type     |              ---------------> |     |
*                                 |   Length    |                               |     |
*                                 |   Address   |                               -------
*                                 |-------------|
*                                 |    Flags    |
*                                 |-------------|
*                                 |   Accessed  |
*                                 |   Counter   |
*                                 |-------------|
*                                 |   Request   |
*                                 |   Counter   |
*                                 |-------------|
*                                 |    State    |
*                                 |-------------|
*
*
* Note(s) : (1) Configured by the developer via 'net_cfg.h' & 'net_cfg_net.h' at compile time.
*
*               See 'net_arp.h  Note #2' & 'ARP HARDWARE & PROTOCOL DEFINES  Note #1' for supported
*               hardware & protocol types.
*********************************************************************************************************
*/

                                                                        /* --------------- NET ARP CACHE -------------- */
typedef  struct  net_arp_cache  NET_ARP_CACHE;

#ifdef NET_IPv4_MODULE_EN
struct  net_arp_cache {
    NET_CACHE_TYPE        Type;
    NET_CACHE_ADDR_ARP   *CacheAddrPtr;                                 /* Ptr to ARP addr cache.                       */
    NET_TMR              *TmrPtr;                                       /* Ptr to cache TMR.                            */
    CPU_INT08U            ReqAttemptsCtr;                               /* ARP req attempts ctr.                        */
    NET_ARP_CACHE_STATE   State;                                        /* ARP cache state.                             */
    CPU_INT16U            Flags;                                        /* ARP cache flags.                             */
};
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

NET_ARP_EXT  CPU_INT16U      NetARP_CacheAccessedTh_nbr;                /* Nbr successful srch's to promote ARP cache.  */


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
*                                             PUBLIC API
*********************************************************************************************************
*/

void               NetARP_CfgAddrFilterEn          (CPU_BOOLEAN          en);

void               NetARP_TxReqGratuitous          (NET_PROTOCOL_TYPE    protocol_type,
                                                    CPU_INT08U          *p_addr_protocol,
                                                    NET_CACHE_ADDR_LEN   addr_protocol_len,
                                                    NET_ERR             *p_err);

                                                                                    /* ---------- CFG FNCTS ----------- */
CPU_BOOLEAN         NetARP_CfgCacheTimeout         (CPU_INT16U           timeout_sec);


CPU_BOOLEAN         NetARP_CfgCacheTxQ_MaxTh       (NET_BUF_QTY          nbr_buf_max);

CPU_BOOLEAN         NetARP_CfgCacheAccessedTh      (CPU_INT16U           nbr_access);

CPU_BOOLEAN         NetARP_CfgReqTimeout           (CPU_INT08U           timeout_sec);

CPU_BOOLEAN         NetARP_CfgReqMaxRetries        (CPU_INT08U           max_nbr_retries);


                                                                                    /* --------- STATUS FNCTS --------- */

CPU_BOOLEAN         NetARP_IsAddrProtocolConflict  (NET_IF_NBR           if_nbr,
                                                    NET_ERR             *p_err);

NET_CACHE_ADDR_LEN  NetARP_CacheGetAddrHW          (NET_IF_NBR           if_nbr,
                                                    CPU_INT08U          *p_addr_hw,
                                                    NET_CACHE_ADDR_LEN   addr_hw_len_buf,
                                                    CPU_INT08U          *p_addr_protocol,
                                                    NET_CACHE_ADDR_LEN   addr_protocol_len,
                                                    NET_ERR             *p_err);

void               NetARP_CacheProbeAddrOnNet      (NET_PROTOCOL_TYPE    protocol_type,
                                                    CPU_INT08U          *p_addr_protocol_sender,
                                                    CPU_INT08U          *p_addr_protocol_target,
                                                    NET_CACHE_ADDR_LEN   addr_protocol_len,
                                                    NET_ERR             *p_err);

                                                                                    /* ---- ARP CACHE STATUS FNCTS ---- */
CPU_INT08U         NetARP_CacheCalcStat            (void);


NET_STAT_POOL      NetARP_CachePoolStatGet         (void);

void               NetARP_CachePoolStatResetMaxUsed(void);


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

#ifdef NET_IPv4_MODULE_EN
void  NetARP_Init        (NET_ERR             *p_err);

void  NetARP_CacheHandler(NET_BUF             *p_buf,
                          NET_ERR             *p_err);

void  NetARP_CacheAddPend(NET_BUF             *p_buf,
                          NET_BUF_HDR         *p_buf_hdr,
                          CPU_INT08U          *p_addr_protocol,
                          NET_ERR             *p_err);

void  NetARP_CacheTimeout(void                *p_cache_timeout);

void  NetARP_Rx          (NET_BUF             *p_buf,
                          NET_ERR             *p_err);

void  NetARP_TxReq       (NET_CACHE_ADDR_ARP  *p_cache);
#endif


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


#ifndef  NET_ARP_CFG_CACHE_NBR
#error  "NET_ARP_CFG_CACHE_NBR                not #define'd in 'net_cfg.h' "
#error  "                               [MUST be  >= NET_ARP_CACHE_NBR_MIN]"
#error  "                               [     &&  <= NET_ARP_CACHE_NBR_MAX]"

#elif  ((NET_ARP_CFG_CACHE_NBR < NET_ARP_CACHE_NBR_MIN) || \
        (NET_ARP_CFG_CACHE_NBR > NET_ARP_CACHE_NBR_MAX))
#error  "NET_ARP_CFG_NBR_CACHE          illegally #define'd in 'net_cfg.h' "
#error  "                               [MUST be  >= NET_ARP_CACHE_NBR_MIN]"
#error  "                               [     &&  <= NET_ARP_CACHE_NBR_MAX]"
#endif



/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/
#endif  /* NET_ARP_MODULE_EN      */
#endif  /* NET_ARP_MODULE_PRESENT */

