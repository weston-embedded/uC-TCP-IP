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
*                                          NETWORK MLDP LAYER
*                               (MULTICAST LISTENER DISCOVERY PROTOCOL)
*
* Filename : net_mldp.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Neighbor Discovery Protocol as described in RFC #2710.
*
*            (2) Only the MLDP v1 is supported. The MLDP v2 as described in RFC #3810 is not yet
*                supported.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    NET_MLDP_MODULE
#include  "net_mldp.h"
#include  "net_ipv6.h"
#include  "net_icmpv6.h"
#include  "../../Source/net_tmr.h"
#include  "../../IF/net_if.h"
#include  "../../Source/net.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'net_mldp.h  MODULE'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_MLDP_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         MLDP REPORT DEFINES
*
* Note(s) : (1) RFC #2710,  Section 4 'Protocol Description' states that :
*
*               (a) "When a node starts listening to a multicast address on an interface, it should
*                    immediately transmit an unsolicited Report for that address on that interface,
*                    in case it is the first listener on the link. To cover the possibility of the initial
*                    Report being lost or damaged, it is recommended that it be repeated once or twice
*                    after short delays."
*
*                    The delay between the report transmissions is set to 2 seconds in this implementation.
*
*               (b) "When a node receives a Multicast-Address-Specific Query, if it is listening to the
*                    queried Multicast Address on the interface from which the Query was received, it
*                    sets a delay timer for that address to a random value selected from the range
*                    [0, Maximum Response Delay]."
*
*           (2) When a transmit error occurs when attempting to transmit an MLDP report, a new timer
*               is set with a delay of NET_MLDP_HOST_GRP_REPORT_DLY_RETRY_SEC seconds to retransmit
*               the report.
*********************************************************************************************************
*/

#define  NET_MLDP_HOST_GRP_REPORT_DLY_JOIN_SEC             2    /* See Note #1a.                                        */
                                                                /* See Note #1b.                                        */
#define  NET_MLDP_HOST_GRP_REPORT_DLY_MIN_SEC              0
#define  NET_MLDP_HOST_GRP_REPORT_DLY_MAX_SEC             10

#define  NET_MLDP_HOST_GRP_REPORT_DLY_RETRY_SEC            2    /* See Note #2.                                         */


/*
*********************************************************************************************************
*                                          MLDP FLAG DEFINES
*********************************************************************************************************
*/
                                                                /* ------------------ NET MLDP FLAGS ------------------ */
#define  NET_MLDP_FLAG_NONE                              DEF_BIT_NONE
#define  NET_MLDP_FLAG_USED                              DEF_BIT_00    /* MLDP host grp cur used; i.e. NOT in free pool.*/


/*
*********************************************************************************************************
*                                 MLDP HOP BY HOP EXT HDR SIZE DEFINES
*********************************************************************************************************
*/

#define  NET_MLDP_OPT_HDR_SIZE                             8


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

static  NET_MLDP_HOST_GRP   NetMLDP_HostGrpTbl[NET_MCAST_CFG_HOST_GRP_NBR_MAX]; /* Table of MDLP Host Group Objects.    */
static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpPoolPtr;                             /* Ptr to pool of free host grp.        */
static  NET_STAT_POOL       NetMLDP_HostGrpPoolStat;                            /* Stastici Pool for MLDP.              */

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpListHead;                            /* Ptr to head of MLDP Host Grp List.   */

static  RAND_NBR            NetMLDP_RandSeed;                                   /* Variable for MLDP random delay.      */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static  void                NetMLDP_RxPktValidate           (      NET_BUF             *p_buf,
                                                                   NET_BUF_HDR         *p_buf_hdr,
                                                                   NET_MLDP_V1_HDR     *p_mldp_hdr,
                                                                   NET_ERR             *p_err);

static  void                NetMLDP_RxQuery                 (      NET_IF_NBR          if_nbr,
                                                             const NET_IPv6_ADDR       *p_addr_dest,
                                                                   NET_MLDP_V1_HDR     *p_mldp_hdr,
                                                                   NET_ERR             *p_err);

static  void                NetMLDP_RxReport                (      NET_IF_NBR          if_nbr,
                                                             const NET_IPv6_ADDR      *p_addr_dest,
                                                                   NET_MLDP_V1_HDR    *p_mldp_hdr,
                                                                   NET_ERR            *p_err);

static  void                NetMLDP_TxAdvertiseMembership   (      NET_MLDP_HOST_GRP  *p_grp,
                                                                   NET_ERR            *p_err);

static  void                NetMLDP_TxReport                (      NET_IF_NBR          if_nbr,
                                                                   NET_IPv6_ADDR      *p_addr_mcast_dest,
                                                                   NET_ERR            *p_err);

static  void                NetMLDP_TxMsgDone               (      NET_IF_NBR          if_nbr,
                                                                   NET_IPv6_ADDR      *p_addr_mcast_dest,
                                                                   NET_ERR            *p_err);

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpSrch             (      NET_IF_NBR          if_nbr,
                                                             const NET_IPv6_ADDR      *p_addr);

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpSrchIF           (      NET_IF_NBR          if_nbr);

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpAdd              (      NET_IF_NBR          if_nbr,
                                                             const NET_IPv6_ADDR      *p_addr,
                                                                   NET_ERR            *p_err);

static  void                NetMLDP_HostGrpRemove           (      NET_MLDP_HOST_GRP  *p_host_grp);

static  void                NetMLDP_HostGrpInsert           (      NET_MLDP_HOST_GRP  *p_host_grp);

static  void                NetMLDP_HostGrpUnlink           (      NET_MLDP_HOST_GRP  *p_host_grp);

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpGet              (      NET_ERR            *p_err);


static  void                NetMLDP_HostGrpFree             (      NET_MLDP_HOST_GRP  *p_host_grp);

static  void                NetMLDP_HostGrpClr              (      NET_MLDP_HOST_GRP  *p_host_grp);

#if 0
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void                NetMLDP_HostGrpDiscard          (      NET_MLDP_HOST_GRP  *p_host_grp);
#endif
#endif

static  void                NetMLDP_HostGrpReportDlyTimeout (      void               *p_host_grp_timeout);

static  void                NetMLDP_LinkStateNotification   (      NET_IF_NBR           if_nbr,
                                                                   NET_IF_LINK_STATE    link_state);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            NetMLDP_Init()
*
* Description : (1) Initialize Multicast Listener Discovery Layer :
*
*                   (a) Initialize MLDP host group pool
*                   (b) Initialize MLDP host group table
*                   (c) Initialize MLDP Host Group List pointer
*                   (d) Initialize MLDP random seed
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) MLDP host group pool MUST be initialized PRIOR to initializing the pool with pointers
*                   to MLDP host group.
*********************************************************************************************************
*/

void  NetMLDP_Init (void)
{

    NET_MLDP_HOST_GRP      *p_host_grp;
    NET_MLDP_HOST_GRP_QTY   i;
    NET_ERR                 err;


                                                                /* ---------- INIT MLDP HOST GRP POOL/STATS ----------- */
    NetMLDP_HostGrpPoolPtr = (NET_MLDP_HOST_GRP *)0;            /* Init-clr MLDP host grp pool (see Note #2).           */

    NetStat_PoolInit(&NetMLDP_HostGrpPoolStat,
                      NET_MCAST_CFG_HOST_GRP_NBR_MAX,
                     &err);

    (void)&err;                                                 /* Prevent 'variable unused' warning.                   */

                                                                /* -------------- INIT MLDP HOST GRP TBL -------------- */
    p_host_grp = &NetMLDP_HostGrpTbl[0];
    for (i = 0u; i < NET_MCAST_CFG_HOST_GRP_NBR_MAX; i++) {
        p_host_grp->State = NET_MLDP_HOST_GRP_STATE_FREE;        /* Init each MLDP host grp as free/NOT used.            */

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        NetMLDP_HostGrpClr(p_host_grp);
#endif
                                                                /* Free each MLDP host grp to pool.                     */
        p_host_grp->NextListPtr = NetMLDP_HostGrpPoolPtr;
        NetMLDP_HostGrpPoolPtr  = p_host_grp;

        p_host_grp++;
    }

    NetMLDP_HostGrpListHead = (NET_MLDP_HOST_GRP *)0;           /* ----------- INIT MLDP HOST GRP LIST PTR ------------ */

                                                                /* ------------------ INIT RAND SEED ------------------ */
    NetMLDP_RandSeed = 1u;                                      /* See 'lib_math.c  Math_Init()  Note #2'.              */
}


/*
*********************************************************************************************************
*                                            NetMLDP_HostGrpJoin()
*
* Description :  Join a IPv6 MLDP group associated with a multicast address.
*
* Argument(s) : if_nbr      Interface number associated with the MDLP host group.
*
*               p_addr      Pointer to IPv6 address of host group to join.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetMLDP_HostGrpJoinHandler() : -
*                               NET_MDLP_ERR_NONE                   Host group successfully joined.
*                               NET_MDLP_ERR_INVALID_ADDR_GRP       Invalid group address.
*                               NET_MDLP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*                               NET_MDLP_ERR_HOST_GRP_INVALID_TYPE  Host group is NOT a valid host group type.
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled configured interface.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*                               NET_MLDP_ERR_TX                     Err in Tx of MDLP Report Message.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : Pointer to MLDP Host Group object added to the MLDP list.
*
*               DEF_NULL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetMLDP_HostGrpJoin() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetMLDP_HostGrpJoinHandler()  Note #2'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMLDP_HostGrpJoin (NET_IF_NBR      if_nbr,
                                  NET_IPv6_ADDR  *p_addr,
                                  NET_ERR        *p_err)
{
    CPU_BOOLEAN         result = DEF_FAIL;
    NET_MLDP_HOST_GRP  *p_grp  = DEF_NULL;


    Net_GlobalLockAcquire((void *)&NetMLDP_HostGrpJoin, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit_release;
    }
#endif

                                                                /* Join host grp.                                       */
    p_grp = NetMLDP_HostGrpJoinHandler(if_nbr,
                                      p_addr,
                                      p_err);
    goto exit_release;


exit_release:
    Net_GlobalLockRelease();

    if (p_grp != DEF_NULL) {
        result = DEF_OK;
    } else {
        result = DEF_FAIL;
    }

    goto exit;

exit_lock_fault:
exit:
    return (result);
}


/*
*********************************************************************************************************
*                                    NetMLDP_HostGrpJoinHandler()
*
* Description : (1) Join a IPv6 MLDP group associated with a multicast address:
*
* *                 (a) Validate interface number
*                   (b) Validate multicast group address
*                   (c) Search MLDP Host Group List for host group with corresponding address
*                           & interface number
*                   (d) If host group NOT found, allocate new host group.
*                   (e) Advertise membership to multicast router(s)
*
*
* Argument(s) : if_nbr      Interface number associated with the MDLP host group.
*
*               p_addr      Pointer to IPv6 address of host group to join.
*               ------      Argument validated by caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE                   Host group successfully joined.
*                               NET_MLDP_ERR_INVALID_ADDR_GRP       Invalid host group address.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   --- RETURNED BY NetMLDP_HostGrpAdd() : ---
*                               NET_MLDP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*
*                                                                   ---- RETURNED BY NetMLDP_TxReport() : ----
*                               NET_MLDP_ERR_TX                     Err in Tx of MDLP Report Message.
*
*                                                                   - RETURNED BY NetIF_IsEnCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled configured interface.
*
* Return(s)   : Pointer to MLDP Host Group object added to the MLDP list.
*
*               DEF_NULL, otherwise.
*
* Caller(s)   : NetMLDP_HostGrpJoin(),
*               NetIF_Start(),
*               NetIPv6_CfgAddrAddHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
*
* Note(s)     : (2) NetMLDP_HostGrpJoinHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetMLDP_HostGrpJoin()  Note #1.
*
*               (3) NetMLDP_HostGrpJoinHandler() blocked until network initialization completes.
*********************************************************************************************************
*/

NET_MLDP_HOST_GRP  *NetMLDP_HostGrpJoinHandler (      NET_IF_NBR      if_nbr,
                                                const NET_IPv6_ADDR  *p_addr,
                                                      NET_ERR        *p_err)
{
    NET_MLDP_HOST_GRP  *p_grp    = DEF_NULL;
    CPU_BOOLEAN         is_mcast = DEF_NO;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit;
    }
#endif


                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }


                                                                /* -------------- VALIDATE HOST GRP ADDR -------------- */
    is_mcast = NetIPv6_IsAddrMcast(p_addr);
    if (is_mcast != DEF_YES) {
       *p_err = NET_MLDP_ERR_INVALID_ADDR_GRP;
        goto exit;
    }

                                                                /* ---------------- SRCH HOST GRP LIST ---------------- */
    p_grp = NetMLDP_HostGrpSrch(if_nbr, p_addr);
    if (p_grp != DEF_NULL) {                                    /* If host grp found, ...                               */
        p_grp->RefCtr++;
       *p_err = NET_MLDP_ERR_NONE;
        goto exit;
    }

    p_grp = NetMLDP_HostGrpAdd(if_nbr, p_addr, p_err);          /* Add new host grp into Host Grp List.                 */
    if (*p_err != NET_MLDP_ERR_NONE) {
         goto exit;
    }

   *p_err = NET_MLDP_ERR_NONE;

exit:
    return (p_grp);
}


/*
*********************************************************************************************************
*                                            NetMLDP_HostGrpLeave()
*
* Description : Leave MDLP group associated with the received IPv6 multicast address.
*
* Argument(s) : if_nbr      Interface number associated with host group.
*
*               p_addr      Pointer to IPv6 address of host group to leave.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetMLDP_HostGrpLeaveHandler() : -
*                               NET_MLDP_ERR_NONE                   Host group successfully left.
*                               NET_MLDP_ERR_INVALID_ADDR_GRP       Invalid host group address.
*                               NET_MLDP_ERR_HOST_GRP_NOT_FOUND     Host group NOT found.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled configured interface.
*                               NET_MLDP_ERR_TX                     Err in Tx of MDLP Done Message.
*
*                                                                   ---- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if host group successfully left.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetMLDP_HostGrpLeave() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetMLDP_HostGrpLeaveHandler()  Note #2'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMLDP_HostGrpLeave (NET_IF_NBR      if_nbr,
                                   NET_IPv6_ADDR  *p_addr,
                                   NET_ERR        *p_err)
{
    CPU_BOOLEAN  host_grp_leave;


                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetMLDP_HostGrpLeave, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit_release;
    }
#endif

                                                                /* Leave host grp.                                      */
    host_grp_leave = NetMLDP_HostGrpLeaveHandler(if_nbr, p_addr, p_err);

    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */


    return (host_grp_leave);
}


/*
*********************************************************************************************************
*                                            NetMLDP_HostGrpLeaveHandler()
*
* Description : (1) Leave MDLP group associated with the received IPv6 multicast address :
*
* *                 (a) Search MLDP Host Group List for host group with corresponding address
*                           & interface number
*                   (b) If host group found, remove host group from MLDP Host Group List.
*                   (c) Advertise end of Membership if host is last member of the group.
*
* Argument(s) : if_nbr      Interface number associated with host group.
*
*               p_addr      Pointer to IPv6 address of host group to leave.
*               ------      Argument validated by caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE                   Host group successfully left.
*                               NET_MLDP_ERR_INVALID_ADDR_GRP       Invalid host group address.
*                               NET_MLDP_ERR_HOST_GRP_NOT_FOUND     Host group NOT found.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   - RETURNED BY NetIF_IsEnCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled configured interface.
*
*                                                                   ----- RETURNED BY NetMLDP_TxDone() : -----
*                               NET_MLDP_ERR_TX                     Err in Tx of MDLP Done Message.
*
* Return(s)   : DEF_OK,   if host group successfully left.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetMLDP_HostGrpLeave()
*               NetIPv6_CfgAddrRemove()
*               NetIPv6_CfgAddrRemoveAllHandler()
*
* Note(s)     : (2) NetMLDP_HostGrpLeaveHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetMLDP_HostGrpLeave()  Note #1'.
*
*               (3) NetMLDP_HostGrpLeaveHandler() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMLDP_HostGrpLeaveHandler (      NET_IF_NBR     if_nbr,
                                          const NET_IPv6_ADDR *p_addr,
                                                NET_ERR       *p_err)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    CPU_BOOLEAN         is_mcast_allnodes;
    CPU_BOOLEAN         is_mcast;
    CPU_INT08U          scope;
    CPU_BOOLEAN         result;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err  = NET_INIT_ERR_NOT_COMPLETED;
        result = DEF_FAIL;
        goto exit;
    }
#endif


                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {                            /* If cfg'd IF NOT en'd (see Note #4), ...              */
        result = DEF_FAIL;                                      /* ... rtn err.                                         */
        goto exit;
    }


                                                                /* -------------- VALIDATE HOST GRP ADDR -------------- */
    is_mcast = NetIPv6_IsAddrMcast(p_addr);
    if (is_mcast != DEF_YES) {
       *p_err = NET_MLDP_ERR_INVALID_ADDR_GRP;
        result = DEF_FAIL;
        goto exit;
    }
                                                                /* ---------------- SRCH HOST GRP LIST ---------------- */
    p_host_grp = NetMLDP_HostGrpSrch(if_nbr, p_addr);
    if (p_host_grp == (NET_MLDP_HOST_GRP *)0) {                 /* If host grp NOT found, ...                           */
       *p_err  =  NET_MLDP_ERR_HOST_GRP_NOT_FOUND;              /* ... rtn err.                                         */
        result = DEF_FAIL;
        goto exit;
    }

    p_host_grp->RefCtr--;                                       /* Dec ref ctr.                                         */

                                                                /* ----------- ADVERTISE END OF MEMBERSHIP ------------ */
    is_mcast_allnodes = NetIPv6_IsAddrMcastAllNodes(&p_host_grp->AddrGrp);

    scope             = NetIPv6_GetAddrScope(&p_host_grp->AddrGrp);

    if (( is_mcast_allnodes == DEF_NO)            &&
        ((scope != NET_IPv6_ADDR_SCOPE_RESERVED)  &&
         (scope != NET_IPv6_ADDR_SCOPE_IF_LOCAL))) {

        if (p_host_grp->RefCtr < 1) {
            NetMLDP_TxMsgDone(if_nbr,
                             &p_host_grp->AddrGrp,
                              p_err);
            switch (*p_err) {
                case NET_MLDP_ERR_NONE:
                case NET_ERR_IF_LINK_DOWN:
                     result = DEF_OK;
                     break;

                case NET_MLDP_ERR_INVALID_HOP_HDR:
                case NET_ERR_TX:
                case NET_ERR_TX_BUF_NONE_AVAIL:
                case NET_ERR_FAULT_UNKNOWN_ERR:
                     result = DEF_FAIL;
                     goto exit;

                default:
                    result = DEF_FAIL;
                   *p_err  = NET_ERR_FAULT_UNKNOWN_ERR;
                    goto exit;
            }
        }
    }

                                                                /* -------- REMOVE HOST GRP FROM HOST GRP LIST -------- */
    if (p_host_grp->RefCtr < 1) {
        NetMLDP_HostGrpRemove(p_host_grp);
    }

   *p_err = NET_MLDP_ERR_NONE;

exit:
    return (result);
}


/*
*********************************************************************************************************
*                                      NetMLDP_IsGrpJoinedOnIF()
*
* Description : Check for joined host group on specified interface.
*
* Argument(s) : if_nbr      Interface number to search.
*
*               p_addr_grp  Pointer to IPv6 address of host group.
*
* Return(s)   : DEF_YES, if host group address joined on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv6_RxPktValidate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMLDP_IsGrpJoinedOnIF (      NET_IF_NBR      if_nbr,
                                      const NET_IPv6_ADDR  *p_addr_grp)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    CPU_BOOLEAN         grp_joined;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr_grp == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
        return (DEF_FAIL);
    }
#endif

    p_host_grp  =  NetMLDP_HostGrpSrch(if_nbr, p_addr_grp);
    grp_joined = (p_host_grp != (NET_MLDP_HOST_GRP *)0) ? DEF_YES : DEF_NO;

    return (grp_joined);
}


/*
*********************************************************************************************************
*                                       NetMLDP_HopByHopHdr()
*
* Description : Callback function called by IP layer when Extension headers are added to Tx buffer.
*
* Argument(s) : p_ext_hdr_arg   Pointer to list of arguments.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPktPrepareExtHdr()
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetMLDP_PrepareHopByHopHdr(void  *p_ext_hdr_arg)
{
    NET_IPv6_EXT_HDR_ARG_GENERIC  *p_hop_hdr_arg;
    NET_IPv6_OPT_HDR              *p_hop_hdr;
    NET_IPv6_EXT_HDR_TLV          *p_tlv;
    NET_BUF                       *p_buf;
    CPU_INT16U                     hop_hdr_ix;


    p_hop_hdr_arg      = (NET_IPv6_EXT_HDR_ARG_GENERIC *)p_ext_hdr_arg;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_ext_hdr_arg == (NET_IPv6_EXT_HDR_ARG_GENERIC *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
        return;
    }
#endif

    p_buf              =  p_hop_hdr_arg->BufPtr;
    hop_hdr_ix         =  p_hop_hdr_arg->BufIx;

    p_hop_hdr          = (NET_IPv6_OPT_HDR *)&p_buf->DataPtr[hop_hdr_ix];
    p_hop_hdr->NextHdr =  p_hop_hdr_arg->NextHdr;
    p_hop_hdr->HdrLen  =  0;
    p_tlv              = (NET_IPv6_EXT_HDR_TLV  *)&p_hop_hdr->Opt[0];
    p_tlv->Type        =  NET_IPv6_EH_TYPE_ROUTER_ALERT;
    p_tlv->Len         =  2;
    p_tlv->Val[0]      =  0;
    p_tlv->Val[1]      =  0;
    p_tlv              = (NET_IPv6_EXT_HDR_TLV  *) (&p_hop_hdr->Opt[0] + 4);
    p_tlv->Type        =  NET_IPv6_EH_TYPE_PADN;
    p_tlv->Len         =  0;
}


/*
*********************************************************************************************************
*                                           NetMLDP_Rx()
*
* Description : (1) Process received MLDP packets & update host group status :
*
*                   (a) Validate MLDP packet
*                   (b) Update   MLDP host group status
*                   (c) Free     MLDP packet
*                   (d) Update receive statistics
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMP packet.
*               -----       Argument checked   in NetICMPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetICMPv6_Rx().
*
*               p_mldp_hdr  Pointer to received packet's MLDP header.
*               ----------  Argument validated in NetICMPv6_Rx()/NetICMPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE               MLDP packet successfully received & processed.
*
*                                                               --- RETURNED BY NetMLDP_RxPktValidate() : ----
*                               NET_MLDP_ERR_HOP_LIMIT          Invalid Hop Limit
*                               NET_MLDP_ERR_INVALID_HOP_HDR    Invalid Hop-by_hop header received.
*                               NET_MLDP_ERR_INVALID_ADDR_SRC   Invalid source address.
*                               NET_MLDP_ERR_INVALID_ADDR_DEST  Invalid destination address.
*                               NET_MLDP_ERR_INVALID_LEN        Invalid MLDP message length.
*                               NET_MDLP_ERR_INVALID_TYPE       Invalid MDLP message type.
*
*                                                               ------ RETURNED BY NetMLDP_RxQuery() : -------
*                                                               ------ RETURNED BY NetMLDP_RxReport() : ------
*                               NET_MLDP_ERR_INVALID_ADDR_GRP
*                               NET_MLDP_ERR_HOST_GRP_NOT_FOUND
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_Rx()
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetMLDP_Rx (NET_BUF          *p_buf,
                  NET_BUF_HDR      *p_buf_hdr,
                  NET_MLDP_V1_HDR  *p_mldp_hdr,
                  NET_ERR          *p_err)
{
    NET_IF_NBR   if_nbr;
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR     *p_ctr;
#endif


    if_nbr = p_buf_hdr->IF_Nbr;

                                                                /*-------------- VALIDATE RX'D MLDP PKT -------------- */
    NetMLDP_RxPktValidate(p_buf,
                          p_buf_hdr,
                          p_mldp_hdr,
                          p_err);

                                                                /* ------------------ DEMUX MLDP MSG ------------------ */
    switch (*p_err) {
        case NET_MLDP_ERR_MSG_TYPE_QUERY:
             NetMLDP_RxQuery(if_nbr,
                            &p_buf_hdr->IPv6_AddrDest,
                             p_mldp_hdr,
                             p_err);
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
             p_ctr = (NET_CTR *)Net_StatCtrs.MLDP.RxMsgQueryCtr;
#endif
             break;


        case NET_MLDP_ERR_MSG_TYPE_REPORT:
             NetMLDP_RxReport(if_nbr,
                             &p_buf_hdr->IPv6_AddrDest,
                              p_mldp_hdr,
                              p_err);
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
             p_ctr = (NET_CTR *)Net_StatCtrs.MLDP.RxMsgReportCtr;
#endif
             break;


        case NET_MLDP_ERR_HOP_LIMIT:
        case NET_MLDP_ERR_INVALID_HOP_HDR:
        case NET_MLDP_ERR_INVALID_ADDR_SRC:
        case NET_MLDP_ERR_INVALID_LEN:
        case NET_MDLP_ERR_INVALID_TYPE:
        default:
             return;
    }

                                                                /* ------------------ UPDATE RX STATS ----------------- */
    NET_CTR_STAT_INC(Net_StatCtrs.MLDP.RxMsgCompCtr);
    NET_CTR_STAT_INC(*p_ctr);
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
*                                          NetMLDP_RxPktValidate()
*
* Description : (1) Validate received MLDP packet :
*
*                   (a) Validate Hop Limit of packet.
*                   (b) Validate Router Alert Option in Hop-By-Hop IPv6 Extension Header.
*                   (c) Validate Rx Source address as a link-local address
*                   (d) Validate Rx Destination address as multicast
*                   (e) Validate Rx Source address is not configured on host.
*                   (f) Validate MDLP message length.
*                   (g) Validate MDLP message type.
*
* Argument(s) : p_buf       Pointer to network buffer that received ICMP packet.
*               -----       Argument checked   in NetICMPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetICMPv6_Rx().
*
*               p_mldp_hdr  Pointer to received packet's NDP header.
*               ----------  Argument validated in NetICMPv6_Rx()/NetICMPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_HOP_LIMIT          Invalid Hop Limit
*                               NET_MLDP_ERR_INVALID_HOP_HDR    Invalid Hop-by_hop header received.
*                               NET_MLDP_ERR_INVALID_ADDR_SRC   Invalid source address.
*                               NET_MLDP_ERR_INVALID_ADDR_DEST  Invalid destination address.
*                               NET_MLDP_ERR_INVALID_LEN        Invalid MLDP message length.
*                               NET_MDLP_ERR_INVALID_TYPE       Invalid MDLP message type.
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_RxPktValidate (NET_BUF          *p_buf,
                                     NET_BUF_HDR      *p_buf_hdr,
                                     NET_MLDP_V1_HDR  *p_mldp_hdr,
                                     NET_ERR          *p_err)
{
    NET_IPv6_HDR          *p_ip_hdr;
    NET_IPv6_OPT_HDR      *p_opt_hdr;
    CPU_INT08U            *p_data;
    NET_IPv6_EXT_HDR_TLV  *p_tlv;
    NET_IPv6_OPT_TYPE      opt_type;
    NET_IF_NBR             if_nbr;
    NET_IF_NBR             if_nbr_found;
    CPU_INT16U             mldp_msg_len;
    CPU_INT16U             ext_hdr_ix;
    CPU_INT16U             eh_len;
    CPU_INT16U             next_tlv_offset;
    CPU_INT08U             mldp_type;
    CPU_INT08U             hop_limit_ip;
    CPU_BOOLEAN            is_addr_mcast;
    CPU_BOOLEAN            is_addr_linklocal;
    CPU_BOOLEAN            rtr_alert;


    (void)&p_buf;                                               /* Prevent 'variable unused' warning (see Note #3b).    */

    if_nbr = p_buf_hdr->IF_Nbr;

                                                                /* -------------- VALIDATE HOP LIMIT VALUE ------------ */
    p_ip_hdr     = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    hop_limit_ip = p_ip_hdr->HopLim;
    if (hop_limit_ip != NET_IPv6_HDR_HOP_LIM_MIN) {
       *p_err = NET_MLDP_ERR_HOP_LIMIT;
        return;
    }
                                                                /* ---------- VALIDATE HOP BY HOP IPv6 HEADER --------- */
    ext_hdr_ix      =  p_buf_hdr->IPv6_HopByHopHdrIx;
    p_data          =  p_buf->DataPtr;
                                                                /* Get opt hdr from ext hdr data space.                 */
    p_opt_hdr       = (NET_IPv6_OPT_HDR *)&p_data[ext_hdr_ix];

    eh_len          = (p_opt_hdr->HdrLen + 1) * NET_IPv6_EH_ALIGN_SIZE;
    next_tlv_offset =  0u;
    rtr_alert       =  DEF_NO;

    eh_len -= 2u;
    while (next_tlv_offset < eh_len) {

        p_tlv     = (NET_IPv6_EXT_HDR_TLV  *)&p_opt_hdr->Opt[next_tlv_offset];
        opt_type  =  p_tlv->Type & NET_IPv6_EH_TLV_TYPE_OPT_MASK;

        switch (opt_type) {
            case NET_IPv6_EH_TYPE_ROUTER_ALERT:
                 rtr_alert = DEF_YES;
                 break;


            case NET_IPv6_EH_TYPE_PAD1:
            case NET_IPv6_EH_TYPE_PADN:
            default:
                break;
        }

        if (opt_type == NET_IPv6_EH_TYPE_PAD1) {                /* The format of the Pad1 opt is a special case, it ... */
            next_tlv_offset++;                                  /* ... doesn't have len and value fields.               */
        } else {
            next_tlv_offset += p_tlv->Len + 2u;
        }

    }

    if (rtr_alert == DEF_NO) {
       *p_err = NET_MLDP_ERR_INVALID_HOP_HDR;
        return;
    }

                                                                /* ------------- VALIDATE MLDP RX SRC ADDR ------------ */
    is_addr_linklocal = NetIPv6_IsAddrLinkLocal(&p_buf_hdr->IPv6_AddrSrc);
    if (is_addr_linklocal == DEF_NO) {
       *p_err = NET_MLDP_ERR_INVALID_ADDR_SRC;
        return;
    }

                                                                /* ------------- VALIDATE MLDP RX DEST ADDR ------------ */
    is_addr_mcast = NetIPv6_IsAddrMcast(&p_buf_hdr->IPv6_AddrDest);
    if (is_addr_mcast == DEF_NO) {
       *p_err = NET_MLDP_ERR_INVALID_ADDR_DEST;
        return;
    }

                                                                /* ------- VALIDATE SRC ADDR IS NOT LOCAL ADDR -------- */
    if_nbr_found = NetIPv6_GetAddrHostIF_Nbr(&p_buf_hdr->IPv6_AddrSrc);
    if (if_nbr == if_nbr_found) {
       *p_err = NET_MLDP_ERR_INVALID_ADDR_SRC;
        return;
    }

                                                                /* ------------- VALIDATE MLDP RX MSG LEN ------------- */
    mldp_msg_len           = p_buf_hdr->IP_DatagramLen;
    p_buf_hdr->MLDP_MsgLen = mldp_msg_len;
    if (mldp_msg_len < NET_MLDP_MSG_SIZE_MIN) {                 /* If msg len < min msg len, rtn err (see Note #4a).    */
       *p_err = NET_MLDP_ERR_INVALID_LEN;
        return;
    }

                                                                /*  ------------ VALIDATE MLDP MESSAGE TYPE ----------- */
    mldp_type = p_mldp_hdr->Type;
    switch (mldp_type) {
        case NET_ICMPv6_MSG_TYPE_MLDP_QUERY:
            *p_err = NET_MLDP_ERR_MSG_TYPE_QUERY;
             break;


        case NET_ICMPv6_MSG_TYPE_MLDP_REPORT_V1:
            *p_err = NET_MLDP_ERR_MSG_TYPE_REPORT;
             break;


        default:
            *p_err = NET_MDLP_ERR_INVALID_TYPE;
             return;
    }
}


/*
*********************************************************************************************************
*                                         NetMLDP_RxQuery()
*
* Description : (1) Receive Multicast Listener Query message :
*
*                   (a) Find if the query is a general query or a specific query.
*                   (b) Send Report Message if case apply.
*
* Argument(s) : if_nbr        Interface number on which message was received.
*
*               p_addr_dest   Pointer to Destination address of Rx buffer.
*
*               p_mldp_hdr    Pointer to received packet's MLDP header.
*               ----------    Argument validated in caller(s).
*
*               p_err         Pointer to variable that will receive the return error code from this function :
*
*                                 NET_MLDP_ERR_NONE
*                                 NET_MLDP_ERR_INVALID_ADDR_GRP
*                                 NET_MLDP_ERR_HOST_GRP_NOT_FOUND
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_Rx().
*
* Note(s)     : (2) RFC #2710 Section 5 page 9 :
*                   (a) The link-scope all-nodes address (FF02::1) is handled as a special
*                       case. The node starts in Idle Listener state for that address on
*                       every interface, never transitions to another state, and never sends
*                       a Report or Done for that address
*
*                   (b) MLD messages are never sent for multicast addresses whose scope is 0
*                      (reserved) or 1 (node-local).
*********************************************************************************************************
*/

static  void  NetMLDP_RxQuery (      NET_IF_NBR        if_nbr,
                               const NET_IPv6_ADDR    *p_addr_dest,
                                     NET_MLDP_V1_HDR  *p_mldp_hdr,
                                     NET_ERR          *p_err)
{
    NET_MLDP_HOST_GRP        *p_mldp_grp;
    NET_IPv6_ADDR            *p_mldp_grp_addr;
    NET_MLDP_HOST_GRP_STATE   mldp_grp_state;
    CPU_INT32U                mldp_grp_delay;
    NET_TMR_TICK              timeout_tick;
    CPU_INT16U                timeout_sec;
    CPU_INT32U                resp_delay_ms;
    CPU_INT08U                scope;
    CPU_BOOLEAN               is_addr_unspecified;
    CPU_BOOLEAN               is_addr_mcast;
    CPU_BOOLEAN               is_mcast_allnodes;
    CPU_BOOLEAN               if_grp_list;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr_dest == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    p_mldp_grp_addr     = &p_mldp_hdr->McastAddr;

    is_addr_unspecified =  NetIPv6_IsAddrUnspecified(p_mldp_grp_addr);
    is_addr_mcast       =  NetIPv6_IsAddrMcast(p_mldp_grp_addr);

                                                                /* -------------- VALIDATE ADDRESS GROUP -------------- */
    if ((is_addr_unspecified == DEF_NO) &&
        (is_addr_mcast       == DEF_NO)) {
        *p_err = NET_MLDP_ERR_INVALID_ADDR_GRP;
         return;
    }

                                                                /* ----------------- SET DELAY VALUE ------------------ */
    resp_delay_ms = p_mldp_hdr->MaxResponseDly;
    if (resp_delay_ms != 0) {
        timeout_tick = resp_delay_ms * NET_TMR_TIME_TICK_PER_SEC / 1000;
    } else {
        timeout_tick = 0;
    }

                                                                /* ---------------- GET TYPE OF QUERY ----------------- */
    if (is_addr_unspecified == DEF_YES) {                       /* Received a General Query ...                         */

        if_grp_list = DEF_YES;

        p_mldp_grp  = NetMLDP_HostGrpSrchIF(if_nbr);            /* ... Find all MLDP groups related to IF.              */

    } else {                                                    /* Received a Mulitcast-Address-Specific Query ...      */

        if_grp_list = DEF_NO;
                                                                /* ... search MLD grp List for multicast addr.          */
        p_mldp_grp  = NetMLDP_HostGrpSrch(if_nbr, p_addr_dest);
        if (p_mldp_grp == (NET_MLDP_HOST_GRP *)0) {
           *p_err = NET_MLDP_ERR_HOST_GRP_NOT_FOUND;
            return;
        }
    }

                                                                /* ------------- SEND MLDP REPORT MESSAGE ------------- */
    while (p_mldp_grp != (NET_MLDP_HOST_GRP *)0) {

        mldp_grp_state = p_mldp_grp->State;
        mldp_grp_delay = p_mldp_grp->Delay_ms;

        is_mcast_allnodes = NetIPv6_IsAddrMcastAllNodes(&p_mldp_grp->AddrGrp);
        scope             = NetIPv6_GetAddrScope(&p_mldp_grp->AddrGrp);

        if (( is_mcast_allnodes == DEF_NO)            &&        /* See Note #2.                                         */
            ((scope != NET_IPv6_ADDR_SCOPE_RESERVED)  &&
             (scope != NET_IPv6_ADDR_SCOPE_IF_LOCAL))) {

            if (timeout_tick == 0) {                            /* If delay received is null, ...                       */
                NetMLDP_TxReport(if_nbr,                        /* ... send report right away.                          */
                                &p_mldp_grp->AddrGrp,
                                 p_err);
                if (*p_err != NET_MLDP_ERR_NONE) {
                     NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.TxPktDisCtr);
                }

            } else if ((mldp_grp_state == NET_MLDP_HOST_GRP_STATE_IDLE) ||
                       (mldp_grp_delay  > resp_delay_ms)) {

                                                                /* Set Delay timer.                                     */
                NetMLDP_RandSeed   =  Math_RandSeed(NetMLDP_RandSeed);
                timeout_sec        =  NetMLDP_RandSeed % (CPU_INT16U)(NET_MLDP_HOST_GRP_REPORT_DLY_MAX_SEC + 1);
                timeout_tick       = (NET_TMR_TICK)(timeout_sec * NET_TMR_TIME_TICK_PER_SEC);

                p_mldp_grp->TmrPtr =  NetTmr_Get((CPU_FNCT_PTR )NetMLDP_HostGrpReportDlyTimeout,
                                                 (void        *)p_mldp_grp,
                                                                timeout_tick,
                                                                p_err);
                if (*p_err != NET_TMR_ERR_NONE) {               /* If err setting tmr,     ...                          */
                    NetMLDP_TxReport(if_nbr,
                                    &p_mldp_grp->AddrGrp,
                                     p_err);                    /* ... tx report immediately; ...                       */
                    if (*p_err != NET_MLDP_ERR_NONE) {
                         NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.TxPktDisCtr);
                    }

                } else {                                        /* ... else set host grp state to DELAYING.             */
                    p_mldp_grp->State = NET_MLDP_HOST_GRP_STATE_DELAYING;
                }
            } else {
                                                                /* Empty Else Statement                                 */
            }
        } else {
            p_mldp_grp->TmrPtr   = (NET_TMR *)0;
            p_mldp_grp->State    = NET_MLDP_HOST_GRP_STATE_IDLE;
            p_mldp_grp->Delay_ms = 0;
        }

        if (if_grp_list == DEF_NO) {                            /* For a specific query, send only the report for ...   */
            break;                                              /* ... the specific grp.                                */
        }

        p_mldp_grp = p_mldp_grp->NextIF_ListPtr;
    }

   *p_err = NET_MLDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetMLDP_RxReport()
*
* Description : (1) Receive Multicast Listener Report message :
*
*                   (a) Find if IF is listening to the multicast address.
*                   (b) If the group address is one the IF listen to :
*                           i)   remove delay timer.
*                           ii)  Increase the listener count.
*                           iii) Change the group state to IDLE.
*
* Argument(s) : if_nbr        Interface number on which message was received.
*
*               p_addr_dest   Pointer to Destination address of Rx buf.
*
*               p_mldp_hdr    Pointer to received packet's MLDP header.
*               ----------    Argument validated in caller(s).
*
*               p_err         Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE                 Report message successfully Rx & Handled.
*                               NET_MLDP_ERR_INVALID_ADDR_GRP     Invalid Multicast address group.
*                               NET_MLDP_ERR_HOST_GRP_NOT_FOUND   Multicast Host group not found.
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_RxReport (      NET_IF_NBR        if_nbr,
                                const NET_IPv6_ADDR    *p_addr_dest,
                                      NET_MLDP_V1_HDR  *p_mldp_hdr,
                                      NET_ERR          *p_err)
{
    NET_MLDP_HOST_GRP  *p_mldp_grp;
    NET_IPv6_ADDR      *p_mldp_grp_addr;
    CPU_BOOLEAN         is_addr_mcast;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr_dest == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    p_mldp_grp_addr = &p_mldp_hdr->McastAddr;

    is_addr_mcast   =  NetIPv6_IsAddrMcast(p_mldp_grp_addr);

                                                                /* ------- VALIDATE RX MULTICAST GROUP ADDRESS -------- */
    if (is_addr_mcast == DEF_NO) {
       *p_err = NET_MLDP_ERR_INVALID_ADDR_GRP;
        return;
    }

                                                                /* ------- SEARCH FOR GRP ADDR IN MLDP GRP LIST ------- */
    p_mldp_grp = NetMLDP_HostGrpSrch(if_nbr, p_addr_dest);
    if (p_mldp_grp == (NET_MLDP_HOST_GRP *)0) {
       *p_err = NET_MLDP_ERR_HOST_GRP_NOT_FOUND;
        return;
    }

    p_mldp_grp->RefCtr++;                                       /* Increase Listener counter.                           */

    p_mldp_grp->State = NET_MLDP_HOST_GRP_STATE_IDLE;           /* Set state to IDLE.                                   */

                                                                /* Free timer.                                          */
    if (p_mldp_grp->TmrPtr != (NET_TMR *)0) {
        NetTmr_Free(p_mldp_grp->TmrPtr);
        p_mldp_grp->TmrPtr = (NET_TMR *)0;
    }

   *p_err = NET_MLDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetMLDP_TxAdvertiseMembership()
*
* Description : Transmit a MLDP multicast membership advertisement.
*
* Argument(s) : p_grp   Pointer to the MLDP host group.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_MLDP_ERR_NONE                   MLDP membership adv. successfully Tx.
*                           NET_ERR_TX                          Error while transmitting the MLDP message.
*                           NET_ERR_FAULT_UNKNOWN_ERR           Unknown error while tramsmitting the MLDP message.
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpAdd(),
*               NetMLDP_TxAdvertiseMembership().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_TxAdvertiseMembership (NET_MLDP_HOST_GRP  *p_grp,
                                             NET_ERR            *p_err)
{
    NET_TMR_TICK        timeout_tick;
    CPU_BOOLEAN         is_mcast_allnodes;
    CPU_INT08U          scope;


    is_mcast_allnodes = NetIPv6_IsAddrMcastAllNodes(&p_grp->AddrGrp);
    scope             = NetIPv6_GetAddrScope(&p_grp->AddrGrp);

    if (( is_mcast_allnodes == DEF_NO)            &&
        ((scope != NET_IPv6_ADDR_SCOPE_RESERVED)  &&
         (scope != NET_IPv6_ADDR_SCOPE_IF_LOCAL))) {

        NET_IF_LINK_STATE  link_state;


        link_state = NetIF_LinkStateGetHandler(p_grp->IF_Nbr, p_err);
        if (link_state != NET_IF_LINK_UP) {
           *p_err = NET_ERR_IF_LINK_DOWN;
            goto exit;
        }

        timeout_tick  = (NET_TMR_TICK)(NET_MLDP_HOST_GRP_REPORT_DLY_JOIN_SEC * NET_TMR_TIME_TICK_PER_SEC);
        p_grp->TmrPtr =  NetTmr_Get(NetMLDP_HostGrpReportDlyTimeout,
                                    p_grp,
                                    timeout_tick,
                                    p_err);
        if(*p_err == NET_TMR_ERR_NONE) {
            p_grp->State    = NET_MLDP_HOST_GRP_STATE_DELAYING;
            p_grp->Delay_ms = NET_MLDP_HOST_GRP_REPORT_DLY_RETRY_SEC * 1000;

        } else {                                                /* If no timer available tx once.                       */
            p_grp->State    = NET_MLDP_HOST_GRP_STATE_IDLE;
            p_grp->Delay_ms = 0u;
        }

        NetMLDP_TxReport(p_grp->IF_Nbr,
                        &p_grp->AddrGrp,
                         p_err);
        switch (*p_err) {
            case NET_MLDP_ERR_NONE:
            case NET_ERR_TX_BUF_NONE_AVAIL:
                 break;

            case NET_ERR_IF_LINK_DOWN:
                 NetTmr_Free(p_grp->TmrPtr);
                 break;

            case NET_ERR_TX:
            case NET_MLDP_ERR_INVALID_HOP_HDR:
                *p_err = NET_ERR_TX;
                 goto exit;

            default:
                *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
                 goto exit;
        }

    } else {
        p_grp->TmrPtr   = (NET_TMR *)0;
        p_grp->State    =  NET_MLDP_HOST_GRP_STATE_IDLE;
        p_grp->Delay_ms =  0;
    }


   *p_err =  NET_MLDP_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                         NetMLDP_TxReport()
*
* Description : Transmit Multicast Listener Report message.
*
* Argument(s) : if_nbr              Network interface number to transmit Multicast Listener Report message.
*
*               p_addr_mcast_dest   Pointer to IPv6 multicast group address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE               MLDP Report successfully transmitted.
*                               NET_MLDP_ERR_INVALID_HOP_HDR    Error while setting Hop-by-Hop ext header.
*                               NET_MLDP_ERR_TX                 Error while transmitting MDLP report.
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpJoinHandler()
*               NetMLDP_RxQuery()
*               NetMLDP_HostGrpReportDlyTimeout()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_TxReport (NET_IF_NBR      if_nbr,
                                NET_IPv6_ADDR  *p_addr_mcast_dest,
                                NET_ERR        *p_err)
{
    NET_IPv6_ADDR      addr_unspecified;
    NET_IPv6_ADDR     *p_addr_dest;
    NET_IPv6_ADDR     *p_addr_src;
    NET_IPv6_EXT_HDR   hop_hdr;
    NET_IPv6_EXT_HDR  *p_ext_hdr_head;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr_mcast_dest == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* Take the multicast address being reported as dest.   */
    p_addr_dest = p_addr_mcast_dest;

                                                                /* Take a Link-Local address cfgd on IF as src addr.    */
    p_addr_src = NetIPv6_GetAddrLinkLocalCfgd(if_nbr);
    if (p_addr_src == (NET_IPv6_ADDR *)0) {
        NetIPv6_AddrUnspecifiedSet(&addr_unspecified, p_err);
        p_addr_src = &addr_unspecified;
    }

                                                                /* ---------------- ADD HOP-HOP HEADER ---------------- */
    p_ext_hdr_head = (NET_IPv6_EXT_HDR *)0;

    p_ext_hdr_head =  NetIPv6_ExtHdrAddToList(p_ext_hdr_head,
                                             &hop_hdr,
                                              NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP,
                                              NET_MLDP_OPT_HDR_SIZE,
                                              NetMLDP_PrepareHopByHopHdr,
                                              NET_IPv6_EXT_HDR_KEY_HOP_BY_HOP,
                                              p_err);
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             break;

        case NET_IPv6_ERR_INVALID_EH:
             *p_err = NET_MLDP_ERR_INVALID_HOP_HDR;
              goto exit;

        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             goto exit;
    }

                                                                /* ---------------- TX MLDP REPORT MSG ---------------- */
    (void)NetICMPv6_TxMsgReqHandler(        if_nbr,
                                            NET_ICMPv6_MSG_TYPE_MLDP_REPORT_V1,
                                            NET_ICMPv6_MSG_CODE_MLDP_REPORT,
                                            0u,
                                            p_addr_src,
                                            p_addr_dest,
                                            NET_IPv6_HDR_HOP_LIM_MIN,
                                            DEF_NO,
                                            p_ext_hdr_head,
                                    (void *)p_addr_mcast_dest,
                                            NET_IPv6_ADDR_SIZE,
                                            p_err);
    switch (*p_err) {
        case NET_ICMPv6_ERR_NONE:
             break;

        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
             goto exit;

        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             goto exit;
    }


   *p_err = NET_MLDP_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                          NetMLDP_TxDone()
*
* Description : (1) Transmit Multicast Listener Done message.
*
* Argument(s) : if_nbr              Network interface number to transmit Multicast Listener Done message.
*
*               p_addr_mcast_dest   Pointer to multicast IPv6 address.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE               MLDP Done message successfully transmitted.
*                               NET_MLDP_ERR_INVALID_HOP_HDR    Error while setting Hop-by-Hop ext header.
*                               NET_MLDP_ERR_TX                 Error while transmitting MDLP Done message.
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpLeaveHandler().
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetMLDP_TxMsgDone (NET_IF_NBR      if_nbr,
                                 NET_IPv6_ADDR  *p_addr_mcast_dest,
                                 NET_ERR        *p_err)
{
    NET_IPv6_ADDR      addr_unspecified;
    NET_IPv6_ADDR      addr_mcast_all_routers;
    NET_IPv6_ADDR     *p_addr_src;
    NET_IPv6_EXT_HDR   hop_hdr;
    NET_IPv6_EXT_HDR  *p_ext_hdr_head;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE POINTER ------------------ */
    if (p_addr_mcast_dest == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* Take the multicast all routers addr as dest addr.    */
    NetIPv6_AddrMcastAllRoutersSet(&addr_mcast_all_routers, DEF_NO, p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
        *p_err = NET_MLDP_ERR_INVALID_ADDR_DEST;
         return;
    }

                                                                /* Take a Link-Local address cfgd on IF as src addr.    */
    p_addr_src = NetIPv6_GetAddrLinkLocalCfgd(if_nbr);
    if (p_addr_src == (NET_IPv6_ADDR *)0) {
        NetIPv6_AddrUnspecifiedSet(&addr_unspecified, p_err);
        p_addr_src = &addr_unspecified;
    }

                                                                /* ---------------- ADD HOP-HOP HEADER ---------------- */
    p_ext_hdr_head = (NET_IPv6_EXT_HDR *)0;

    p_ext_hdr_head = NetIPv6_ExtHdrAddToList(p_ext_hdr_head,
                                            &hop_hdr,
                                             NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP,
                                             NET_MLDP_OPT_HDR_SIZE,
                                             NetMLDP_PrepareHopByHopHdr,
                                             NET_IPv6_EXT_HDR_KEY_HOP_BY_HOP,
                                             p_err);
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             break;

        case NET_IPv6_ERR_INVALID_EH:
             *p_err = NET_MLDP_ERR_INVALID_HOP_HDR;
              goto exit;

        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             goto exit;
    }

                                                                /* ----------------- TX MLDP DONE MSG ----------------- */
   (void) NetICMPv6_TxMsgReqHandler(         if_nbr,
                                             NET_ICMPv6_MSG_TYPE_MLDP_DONE,
                                             NET_ICMPv6_MSG_CODE_MLDP_DONE,
                                             0u,
                                             p_addr_src,
                                            &addr_mcast_all_routers,
                                             NET_IPv6_HDR_HOP_LIM_MIN,
                                             DEF_NO,
                                             p_ext_hdr_head,
                                    (void *) p_addr_mcast_dest,
                                             NET_IPv6_ADDR_SIZE,
                                             p_err);
   switch (*p_err) {
       case NET_ICMPv6_ERR_NONE:
            break;


       case NET_ERR_IF_LINK_DOWN:
       case NET_ERR_TX:
       case NET_ERR_TX_BUF_NONE_AVAIL:
            goto exit;

       default:
           *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
            goto exit;
   }


   *p_err = NET_MLDP_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                        NetMLDP_HostGrpSrch()
*
* Description : Search MLDP Host Group List for host group with specific address & interface number.
*
*               (1) MLDP host groups are linked to form an MLDP Host Group List.
*
*                   (a) In the diagram below, ... :
*
*                       (1) The horizontal row represents the list of MLDP host groups.
*
*                       (2) 'NetMLDP_HostGrpListHead' points to the head of the MLDP Host Group List.
*
*                       (3) MLDP host groups' 'PrevListPtr' & 'NextListPtr' doubly-link each host group to
*                           form the MLDP Host Group List.
*
*                   (b) (1) For any MLDP Host Group List lookup, all MLDP host groups are searched in order
*                           to find the host group with the appropriate host group address on the specified
*                           interface.
*
*                       (2) To expedite faster MLDP Host Group List lookup :
*
*                           (A) (1) (a) MLDP host groups are added at;            ...
*                                   (b) MLDP host groups are searched starting at ...
*                               (2) ... the head of the MLDP Host Group List.
*
*                           (B) As MLDP host groups are added into the list, older MLDP host groups migrate
*                               to the tail of the MLDP Host Group List.   Once an MLDP host group is left,
*                               it is removed from the MLDP Host Group List.
*
*
*                                        |                                               |
*                                        |<---------- List of MLDP Host Groups --------->|
*                                        |                (see Note #1a1)                |
*
*                                 New MLDP host groups                      Oldest MLDP host group
*                                   inserted at head                        in MLDP Host Group List
*                                  (see Note #1b2A2)                           (see Note #1b2B)
*
*                                           |                 NextPtr                 |
*                                           |             (see Note #1a3)             |
*                                           v                    |                    v
*                                                                |
*                   Head of MLDP         -------       -------   v   -------       -------
*                  Host Group List  ---->|     |------>|     |------>|     |------>|     |
*                                        |     |       |     |       |     |       |     |        Tail of MLDP
*                  (see Note #1a2)       |     |<------|     |<------|     |<------|     |<----  Host Group List
*                                        -------       -------   ^   -------       -------
*                                                                |
*                                                                |
*                                                             PrevPtr
*                                                         (see Note #1a3)
*
*
* Argument(s) : if_nbr      Interface number to search for host group.
*
*               addr_grp    IP address of host group to search (see Note #2).
*
* Return(s)   : Pointer to MLDP host group with specific IP group address & interface, if found.
*
*               Pointer to NULL,                                                       otherwise.
*
* Caller(s)   : NetMLDP_HostGrpJoinHandler(),
*               NetMLDP_HostGrpLeaveHandler(),
*               NetMLDP_IsGrpJoinedOnIF(),
*               NetMLDP_RxQuery(),
*               NetMLDP_RxReport().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpSrch (      NET_IF_NBR      if_nbr,
                                                 const NET_IPv6_ADDR  *p_addr)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_MLDP_HOST_GRP  *p_host_grp_next;
    CPU_INT08U          match_bit;
    CPU_BOOLEAN         found;


    p_host_grp = NetMLDP_HostGrpListHead;                       /* Start @ MLDP Host Grp List head.                     */
    found      = DEF_NO;

    while ((p_host_grp != (NET_MLDP_HOST_GRP *)0) &&            /* Srch MLDP Host Grp List ...                          */
           (found      ==  DEF_NO)               ) {            /* ... until host grp found.                            */

        p_host_grp_next = (NET_MLDP_HOST_GRP *) p_host_grp->NextListPtr;

        match_bit       =  NetIPv6_GetAddrMatchingLen(&p_host_grp->AddrGrp, p_addr);

        found = ((p_host_grp->IF_Nbr  == if_nbr) &&             /* Cmp IF nbr & grp addr.                               */
                 (match_bit == 128)) ? DEF_YES : DEF_NO;

        if (found != DEF_YES) {                                 /* If NOT found, adv to next MLDP host grp.             */
            p_host_grp = p_host_grp_next;
        }
    }

    return (p_host_grp);
}


/*
*********************************************************************************************************
*                                        NetMLDP_HostGrpSrchIF()
*
* Description : Search MLDP Host Group List for all host groups attached to the same interface number.
*
* Argument(s) : if_nbr      Interface number to search for host group.
*
* Return(s)   : Pointer to the MDLP host group at the head of the list containing all the host groups
*                 for the given Interface number.
*
* Caller(s)   : NetMDLP_RxQuery().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpSrchIF (NET_IF_NBR  if_nbr)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_MLDP_HOST_GRP  *p_if_grp_head;



    p_host_grp    =  NetMLDP_HostGrpListHead;                   /* Start @ MLDP Host Grp List head.                     */
    p_if_grp_head = (NET_MLDP_HOST_GRP *)0;

    while (p_host_grp != (NET_MLDP_HOST_GRP *)0) {              /* Srch MLDP Host Grp List ...                          */
                                                                /* ... for all grp attached to IF.                      */

        if (p_host_grp->IF_Nbr  == if_nbr) {
            if (p_if_grp_head == (NET_MLDP_HOST_GRP *)0) {
                p_host_grp->PrevIF_ListPtr = (NET_MLDP_HOST_GRP *)0;
                p_host_grp->NextIF_ListPtr = (NET_MLDP_HOST_GRP *)0;
            } else {
                p_if_grp_head->PrevIF_ListPtr = p_host_grp;
                p_host_grp->NextIF_ListPtr    = p_if_grp_head;
            }
            p_if_grp_head = p_host_grp;
        }

        p_host_grp = p_host_grp->NextListPtr;

    }

    return (p_if_grp_head);
}


/*
*********************************************************************************************************
*                                        NetMLDP_HostGrpAdd()
*
* Description : (1) Add a host group to the MLDP Host Group List :
*
*                   (a) Get a     host group from      host group pool
*                   (b) Configure host group
*                   (c) Insert    host group into MLDP Host Group List
*                   (d) Configure interface for multicast address
*
*
* Argument(s) : if_nbr      Interface number to add host group.
*               ------      Argument checked in NetMLDP_HostGrpJoinHandler().
*
*               p_addr      Pointer to IPv6 address of host group to add.
*               --------    Argument checked in NetMLDP_HostGrpJoinHandler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE                   Host group succesfully added.
*
*                                                                   --- RETURNED BY NetMLDP_HostGrpGet() : ---
*                               NET_MLDP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*
* Return(s)   : Pointer to host group, if NO error(s).
*
*               Pointer to NULL,       otherwise.
*
* Caller(s)   : NetMLDP_HostGrpJoinHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpAdd (      NET_IF_NBR      if_nbr,
                                                const NET_IPv6_ADDR  *p_addr,
                                                      NET_ERR        *p_err)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_PROTOCOL_TYPE   addr_protocol_type;
    CPU_INT08U         *p_addr_protocol;
    CPU_INT08U          addr_protocol_len;
    NET_ERR             err;



    p_host_grp = NetMLDP_HostGrpGet(p_err);                     /* ------------------- GET HOST GRP ------------------- */
    if (p_host_grp == (NET_MLDP_HOST_GRP *)0) {
        return (p_host_grp);                                    /* Rtn err from NetMLDP_HostGrpGet().                   */
    }

                                                                /* ------------------- CFG HOST GRP ------------------- */
    Mem_Copy(&p_host_grp->AddrGrp, p_addr, sizeof(p_host_grp->AddrGrp));

    p_host_grp->IF_Nbr  =   if_nbr;
    p_host_grp->RefCtr  =   1u;

                                                                /* Set host grp state.                                  */
    p_host_grp->State = NET_MLDP_HOST_GRP_STATE_IDLE;

                                                                /* -------- INSERT HOST GRP INTO HOST GRP LIST -------- */
    NetMLDP_HostGrpInsert(p_host_grp);

                                                                /* ------------ CFG IF FOR MULTICAST ADDR ------------- */
    addr_protocol_type =  NET_PROTOCOL_TYPE_IP_V6;
    p_addr_protocol    = (CPU_INT08U *)p_addr;
    addr_protocol_len  =  NET_IPv6_ADDR_SIZE;

    NetIF_AddrMulticastAdd(if_nbr,
                           p_addr_protocol,
                           addr_protocol_len,
                           addr_protocol_type,
                          &err);
    switch (err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_IF_ERR_INVALID_CFG:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_ERR_FAULT_NULL_PTR:
        default:
            *p_err = NET_ERR_FAULT_NULL_OBJ;
             goto exit_remove;
    }

                                                                /* ------------ ADVERTISE MCAST MEMBERSHIP ------------ */
    NetMLDP_TxAdvertiseMembership(p_host_grp, p_err);
    switch (*p_err) {
        case NET_MLDP_ERR_NONE:
        case NET_ERR_TX_BUF_NONE_AVAIL:
        case NET_ERR_IF_LINK_DOWN:
             break;


        case NET_ERR_TX:
        default:
             goto exit;
    }

    NetIF_LinkStateSubscribeHandler(if_nbr, &NetMLDP_LinkStateNotification, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_ERR_FAULT_NULL_PTR:
            *p_err = NET_ERR_FAULT_NULL_OBJ;
             goto exit_remove;


        case NET_IF_ERR_LINK_SUBSCRIBER_MEM_ALLOC:
            *p_err = NET_ERR_FAULT_MEM_ALLOC;
             goto exit_remove;


        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             goto exit_remove;
    }

   *p_err = NET_MLDP_ERR_NONE;
    goto exit;

exit_remove:
    NetMLDP_HostGrpRemove(p_host_grp);
    NetMLDP_HostGrpFree(p_host_grp);
    p_host_grp = DEF_NULL;

exit:
    return (p_host_grp);
}


/*
*********************************************************************************************************
*                                       NetMLDP_HostGrpRemove()
*
* Description : Remove a host group from the MLDP Host Group List :
*
*                   (a) Remove host group from MLDP Host Group List
*                   (b) Free   host group back to   host group pool
*                   (c) Remove multicast address from interface
*
*
* Argument(s) : p_host_grp   Pointer to a host group.
*               ---------    Argument checked in NetMLDP_HostGrpLeaveHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpLeaveHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_HostGrpRemove (NET_MLDP_HOST_GRP  *p_host_grp)
{
    NET_IF_NBR          if_nbr;
    NET_PROTOCOL_TYPE   addr_protocol_type;
    NET_IPv6_ADDR      *p_addr_grp;
    CPU_INT08U         *p_addr_protocol;
    CPU_INT08U          addr_protocol_len;
    NET_ERR             err;


    if_nbr     =  p_host_grp->IF_Nbr;
    p_addr_grp = &p_host_grp->AddrGrp;


    NetMLDP_HostGrpUnlink(p_host_grp);                          /* -------- REMOVE HOST GRP FROM HOST GRP LIST -------- */

    NetMLDP_HostGrpFree(p_host_grp);                            /* ------------------ FREE HOST GRP ------------------- */

                                                                /* ---------- REMOVE MULTICAST ADDR FROM IF ----------- */
    addr_protocol_type =  NET_PROTOCOL_TYPE_IP_V6;
    p_addr_protocol    = (CPU_INT08U *)p_addr_grp;
    addr_protocol_len  =  NET_IPv6_ADDR_SIZE;

    NetIF_AddrMulticastRemove(if_nbr,
                              p_addr_protocol,
                              addr_protocol_len,
                              addr_protocol_type,
                             &err);

    NetIF_LinkStateUnSubscribeHandler(if_nbr, &NetMLDP_LinkStateNotification, &err);

   (void)&err;
}


/*
*********************************************************************************************************
*                                       NetMLDP_HostGrpInsert()
*
* Description : Insert a host group into the MLDP Host Group List.
*
* Argument(s) : p_host_grp   Pointer to a host group.
*               ----------   Argument checked in NetMLDP_HostGrpAdd().
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpAdd().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_HostGrpInsert (NET_MLDP_HOST_GRP  *p_host_grp)
{
                                                                        /* ---------- CFG MLDP HOST GRP PTRS ---------- */
    p_host_grp->PrevListPtr = (NET_MLDP_HOST_GRP *)0;
    p_host_grp->NextListPtr = (NET_MLDP_HOST_GRP *)NetMLDP_HostGrpListHead;

                                                                        /* ------ INSERT MLDP HOST GRP INTO LIST ------ */
    if (NetMLDP_HostGrpListHead != (NET_MLDP_HOST_GRP *)0) {            /* If list NOT empty, insert before head.       */
        NetMLDP_HostGrpListHead->PrevListPtr = p_host_grp;
    }

    NetMLDP_HostGrpListHead = p_host_grp;                               /* Insert host grp @ list head.                 */
}


/*
*********************************************************************************************************
*                                       NetMLDP_HostGrpUnlink()
*
* Description : Unlink a host group from the MLDP Host Group List.
*
* Argument(s) : p_host_grp   Pointer to a host group.
*               ----------   Argument checked in NetMLDP_HostGrpRemove()
*                                             by NetMLDP_HostGrpLeaveHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpRemove().
*
* Note(s)     : (1) Since NetMLDP_HostGrpUnlink() called ONLY to remove & then re-link or free host
*                   groups, it is NOT necessary to clear the entry's previous & next pointers.  However,
*                   pointers cleared to NULL shown for correctness & completeness.
*********************************************************************************************************
*/

static  void  NetMLDP_HostGrpUnlink (NET_MLDP_HOST_GRP  *p_host_grp)
{
    NET_MLDP_HOST_GRP  *p_host_grp_prev;
    NET_MLDP_HOST_GRP  *p_host_grp_next;

                                                                        /* ------ UNLINK MLDP HOST GRP FROM LIST ------ */
    p_host_grp_prev = p_host_grp->PrevListPtr;
    p_host_grp_next = p_host_grp->NextListPtr;
                                                                        /* Point prev host grp to next host grp.        */
    if (p_host_grp_prev != (NET_MLDP_HOST_GRP *)0) {
        p_host_grp_prev->NextListPtr = p_host_grp_next;
    } else {
        NetMLDP_HostGrpListHead = p_host_grp_next;
    }
                                                                        /* Point next host grp to prev host grp.        */
    if (p_host_grp_next != (NET_MLDP_HOST_GRP *)0) {
        p_host_grp_next->PrevListPtr = p_host_grp_prev;
    }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                             /* Clr host grp ptrs (see Note #1).             */
    p_host_grp->PrevListPtr = (NET_MLDP_HOST_GRP *)0;
    p_host_grp->NextListPtr = (NET_MLDP_HOST_GRP *)0;
#endif
}


/*
*********************************************************************************************************
*                                        NetMLDP_HostGrpGet()
*
* Description : (1) Allocate & initialize a host group :
*
*                   (a) Get a host group
*                   (b) Validate   host group
*                   (c) Initialize host group
*                   (d) Update host group pool statistics
*                   (e) Return pointer to host group
*                         OR
*                       Null pointer & error code, on failure
*
*               (2) The host group pool is implemented as a stack :
*
*                   (a) 'NetMLDP_HostGrpPoolPtr' points to the head of the host group pool.
*
*                   (b) Host groups' 'NextPtr's link each host group to form the host group pool stack.
*
*                   (c) Host groups are inserted & removed at the head of    the host group pool stack.
*
*
*                                       Host groups are
*                                     inserted & removed
*                                        at the head
*                                       (see Note #2c)
*
*                                             |                 NextPtr
*                                             |             (see Note #2b)
*                                             v                    |
*                                                                  |
*                                          -------       -------   v   -------       -------
*                          Host group ---->|     |------>|     |------>|     |------>|     |
*                            Pointer       |     |       |     |       |     |       |     |
*                                          |     |       |     |       |     |       |     |
*                         (see Note #2a)   -------       -------       -------       -------
*
*                                          |                                               |
*                                          |<--------- Pool of Free host groups ---------->|
*                                          |                (see Note #2)                  |
*
*
* Argument(s) : p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_MLDP_ERR_NONE                   Host group successfully allocated &
*                                                                       initialized.
*                               NET_MLDP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*
* Return(s)   : Pointer to host group, if NO error(s).
*
*               Pointer to NULL,       otherwise.
*
* Caller(s)   : NetMLDP_HostGrpAdd().
*
* Note(s)     : (3) (a) Host group pool is accessed by 'NetMLDP_HostGrpPoolPtr' during execution of
*
*                       (1) NetMLDP_Init()
*                       (2) NetMLDP_HostGrpGet()
*                       (3) NetMLDP_HostGrpFree()
*
*                   (b) Since the primary tasks of the network protocol suite are prevented from running
*                       concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                       resources of the host group pool since no asynchronous access from other network
*                       tasks is possible.
*********************************************************************************************************
*/

static  NET_MLDP_HOST_GRP  *NetMLDP_HostGrpGet (NET_ERR  *p_err)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_ERR             err;


                                                                /* ------------------- GET HOST GRP ------------------- */
    if (NetMLDP_HostGrpPoolPtr != (NET_MLDP_HOST_GRP *)0) {     /* If host grp pool NOT empty, get host grp from pool.  */
        p_host_grp              = (NET_MLDP_HOST_GRP *)NetMLDP_HostGrpPoolPtr;
        NetMLDP_HostGrpPoolPtr  = (NET_MLDP_HOST_GRP *)p_host_grp->NextListPtr;

    } else {                                                    /* Else none avail, rtn err.                            */
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NoneAvailCtr);
       *p_err = NET_MLDP_ERR_HOST_GRP_NONE_AVAIL;
        return ((NET_MLDP_HOST_GRP *)0);
    }

                                                                /* ------------------ INIT HOST GRP ------------------- */
    NetMLDP_HostGrpClr(p_host_grp);
    DEF_BIT_SET(p_host_grp->Flags, NET_MLDP_FLAG_USED);         /* Set host grp as used.                                */

                                                                /* ------------ UPDATE HOST GRP POOL STATS ------------ */
    NetStat_PoolEntryUsedInc(&NetMLDP_HostGrpPoolStat, &err);
   (void)&err;


   *p_err = NET_MLDP_ERR_NONE;

    return (p_host_grp);
}


/*
*********************************************************************************************************
*                                        NetMLDP_HostGrpFree()
*
* Description : (1) Free a host group :
*
*                   (a) Free   host group timer
*                   (b) Clear  host group controls
*                   (c) Free   host group back to host group pool
*                   (d) Update host group pool statistics
*
*
* Argument(s) : p_host_grp   Pointer to a host group.
*               ---------    Argument checked in NetMLDP_HostGrpRemove()
*                                             by NetMLDP_HostGrpLeaveHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_HostGrpRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_HostGrpFree (NET_MLDP_HOST_GRP  *p_host_grp)
{
    NET_ERR  err;


                                                                    /* -------------- FREE HOST GRP TMR --------------- */
    if (p_host_grp->TmrPtr != (NET_TMR *)0) {
        NetTmr_Free(p_host_grp->TmrPtr);
        p_host_grp->TmrPtr = (NET_TMR *)0;
    }

                                                                    /* ----------------- CLR HOST GRP ----------------- */
    p_host_grp->State = NET_MLDP_HOST_GRP_STATE_FREE;                /* Set host grp as freed/NOT used.                  */
    DEF_BIT_CLR(p_host_grp->Flags, NET_MLDP_FLAG_USED);
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
     NetMLDP_HostGrpClr(p_host_grp);
#endif

                                                                    /* ---------------- FREE HOST GRP ----------------- */
    p_host_grp->NextListPtr = NetMLDP_HostGrpPoolPtr;
    NetMLDP_HostGrpPoolPtr  = p_host_grp;

                                                                    /* ---------- UPDATE HOST GRP POOL STATS ---------- */
    NetStat_PoolEntryUsedDec(&NetMLDP_HostGrpPoolStat, &err);
   (void)&err;
}


/*
*********************************************************************************************************
*                                        NetMLDP_HostGrpClr()
*
* Description : Clear MLDP host group controls.
*
* Argument(s) : p_host_grp   Pointer to a MLDP host group.
*               ---------    Argument checked in caller(s).
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_Init(),
*               NetMLDP_HostGrpGet(),
*               NetMLDP_HostGrpFree().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_HostGrpClr (NET_MLDP_HOST_GRP  *p_host_grp)
{
    p_host_grp->PrevListPtr     = (NET_MLDP_HOST_GRP *)0;
    p_host_grp->NextListPtr     = (NET_MLDP_HOST_GRP *)0;

    p_host_grp->PrevIF_ListPtr  = (NET_MLDP_HOST_GRP *)0;
    p_host_grp->NextIF_ListPtr  = (NET_MLDP_HOST_GRP *)0;

    p_host_grp->TmrPtr          = (NET_TMR           *)0;
    p_host_grp->Delay_ms        =  0;

    p_host_grp->IF_Nbr          =  NET_IF_NBR_NONE;
    p_host_grp->AddrGrp         =  NET_IPv6_ADDR_NONE;

    p_host_grp->State           =  NET_MLDP_HOST_GRP_STATE_FREE;
    p_host_grp->RefCtr          =  0u;
    p_host_grp->Flags           =  NET_MLDP_FLAG_NONE;
}


/*
*********************************************************************************************************
*                                  NetMLDP_HostGrpReportDlyTimeout()
*
* Description : Transmit an MLDP report on MLDP Delay timeout.
*
* Argument(s) : p_host_grp_timeout   Pointer to a host group (see Note #1b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetMLDP_HostGrpJoinHandler(),
*                             NetMLDP_RxQuery(),
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
*                           (A) in this case, a 'NET_MLDP_HOST_GRP' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer,
*                   (b) but do NOT re-free the timer.
*
*               (3) In case of a transmit error :
*
*                   (a) Configure a timer to attempt retransmission of the MLDP report, if the error
*                       is transitory.
*
*                   (b) Revert to 'IDLE' state, if the error is permanent.
*********************************************************************************************************
*/

static  void  NetMLDP_HostGrpReportDlyTimeout (void  *p_host_grp_timeout)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_TMR_TICK        timeout_tick;
    NET_ERR             err;


    p_host_grp = (NET_MLDP_HOST_GRP *)p_host_grp_timeout;       /* See Note #1b2A.                                      */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* -------------- VALIDATE MLDP HOST GRP -------------- */
    if (p_host_grp == (NET_MLDP_HOST_GRP *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.MLDP.NullPtrCtr);
        return;
    }
#endif

    p_host_grp->TmrPtr = (NET_TMR *)0;                          /* Clear tmr (see Note #2).                             */


                                                                /* ------------------ TX MLDP REPORT ------------------ */
    NetMLDP_TxReport((NET_IF_NBR)       p_host_grp->IF_Nbr,
                     (NET_IPv6_ADDR *) &p_host_grp->AddrGrp,
                     (NET_ERR       *) &err);
    switch (err) {
        case NET_MLDP_ERR_NONE:                                 /* If NO err, ...                                       */
        case NET_ERR_IF_LINK_DOWN:
             p_host_grp->State = NET_MLDP_HOST_GRP_STATE_IDLE;  /* ... set state to 'IDLE'.                             */
             break;


        case NET_ERR_TX:                                        /* If tx err, ...                                       */
                                                                /* ... cfg new tmr (see Note #3a).                      */
             timeout_tick       = (NET_TMR_TICK)(NET_MLDP_HOST_GRP_REPORT_DLY_RETRY_SEC * NET_TMR_TIME_TICK_PER_SEC);
             p_host_grp->TmrPtr =  NetTmr_Get(              NetMLDP_HostGrpReportDlyTimeout,
                                              (void       *)p_host_grp,
                                                            timeout_tick,
                                                           &err);
             if(err == NET_TMR_ERR_NONE) {
                p_host_grp->State    = NET_MLDP_HOST_GRP_STATE_DELAYING;
                p_host_grp->Delay_ms = NET_MLDP_HOST_GRP_REPORT_DLY_RETRY_SEC * 1000;
             } else {
                p_host_grp->State    = NET_MLDP_HOST_GRP_STATE_IDLE;
                p_host_grp->Delay_ms = 0;
             }
             break;


        default:                                                /* On all other errs, ...                               */
             p_host_grp->State = NET_MLDP_HOST_GRP_STATE_IDLE;  /* ... set state to 'IDLE'.                             */
             break;
    }
}



/*
*********************************************************************************************************
*                                    NetMLDP_LinkStateNotification()
*
* Description : Callback function called when the Interface link state has changed and the MDLP module
*               needs to update its memberships on the Interface.
*
* Argument(s) : if_nbr      Interface number associated with the link status notification.
*
*               link_state  Current IF link state :
*                               NET_IF_LINK_UP
*                               NET_IF_LINK_DOWN
*
* Return(s)   : none.
*
* Caller(s)   : Referenced NetMLDP_HostGrpAdd(),
*                          NetMLDP_HostGrpRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMLDP_LinkStateNotification (NET_IF_NBR         if_nbr,
                                             NET_IF_LINK_STATE  link_state)
{
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_ERR             err;


    switch (link_state) {
        case NET_IF_LINK_UP:

             p_host_grp = NetMLDP_HostGrpListHead;                      /* Start @ MLDP Host Grp List head.                     */

             while (p_host_grp != DEF_NULL)  {                          /* Srch MLDP Host Grp List ...                          */
                 if (p_host_grp->IF_Nbr == if_nbr) {
                     NetMLDP_TxAdvertiseMembership(p_host_grp, &err);
                    (void)&err;
                 }
                 p_host_grp = p_host_grp->NextListPtr;
             }
             break;


        case NET_IF_LINK_DOWN:
        default:
             break;
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_MLDP_MODULE_EN */

