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
*                                         NETWORK SOURCE FILE
*
* Filename : net.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_MODULE
#include  "net.h"
#include  "net_cfg_net.h"
#include  <net_cfg.h>

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_arp.h"
#include  "../IP/IPv4/net_igmp.h"
#endif

#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ndp.h"
#include  "../IP/IPv6/net_mldp.h"
#endif

#ifdef NET_SECURE_MODULE_EN
#include  "../Secure/net_secure.h"
#endif

#include  "net_bsd.h"
#include  "net_conn.h"
#include  "net_mgr.h"
#include  "net_icmp.h"
#include  "net_tcp.h"
#include  "net_udp.h"
#include  "net_util.h"
#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_SIGNAL_INIT_NAME               "Net Init Signal"
#define  NET_LOCK_GLOBAL_NAME               "Net Global Lock"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  KAL_SEM_HANDLE     Net_InitSignal;
static  KAL_LOCK_HANDLE    Net_GlobalLock;

static  void              *Net_GlobaLockFcntPtr;

        CPU_INT32U         Net_Version = NET_VERSION;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static void  Net_KAL_Init (NET_ERR  *p_err);


/*
*********************************************************************************************************
*                                             Net_Init()
*
* Description : (1) Initialize & startup network protocol suite :
*
*                   (a) Initialize network protocol suite default values
*                   (b) Perform    network protocol suite/operating system initialization
*                   (c) Initialize network protocol suite modules
*                   (d) Signal ALL network protocol suite modules & tasks
*                           that network initialization is complete
*
*
* Argument(s) : p_rx_task_cfg   Pointer to the Rx Task Configuration Object.
*
*               p_tx_task_cfg   Pointer to the Tx Dealloc Task Configuration Object.
*
*               p_tmr_task_cfg  Pointer to the Timer Task Configuration Object.
*
* Return(s)   : NET_ERR_NONE,                                     if NO error(s).
*
*               Specific initialization error code (see Note #4), otherwise.
*
* Caller(s)   : Your Product's Application.
*
*               This function is a network protocol suite initialization function & MAY be called by
*               application/initialization function(s).
*
* Note(s)     : (2) NetInit() MUST be called ... :
*
*                   (a) ONLY ONCE from a product's application; ...
*                   (b) (1) AFTER :
*                           (A) Product's OS   has been initialized
*                           (B) Memory library has been initialized
*                       (2) BEFORE product's application calls any network protocol suite function(s)
*
*               (3) The following initialization functions MUST be sequenced as follows :
*
*                   (a) Net_InitDflt()  MUST precede ALL other network initialization functions
*                   (b) Net_KAL_Init()  MUST precede remaining network initialization functions
*                   (c) NetIF_Init()    MUST follow  signaling network initialization complete
*
*               (4) (a) If any network initialization error occurs, any remaining network initialization
*                       is immediately aborted & the specific initialization error code is returned.
*
*                   (b) Network error codes are listed in 'net_err.h', organized by network modules &/or
*                       layers.  A search of the specific error code number(s) provides the corresponding
*                       error code label(s).  A search of the error code label(s) provides the source code
*                       location of the network initialization error(s).
*
*               (5) In order for network protocol suite initialization complete to be able to be verified
*                   from interrupt service routines, 'Net_InitDone' MUST be accessed exclusively in critical
*                   sections during initialization.
*
*                   See also 'net_if.c  NetIF_ISR_Handler()  Note #2'.
*********************************************************************************************************
*/

NET_ERR  Net_Init (const  NET_TASK_CFG  *rx_task_cfg,
                   const  NET_TASK_CFG  *tx_task_cfg,
                   const  NET_TASK_CFG  *tmr_task_cfg)
{
    CPU_BOOLEAN  is_en;
    NET_ERR      err;
    CPU_INT08U   i;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();                                       /* See Note #5.                                         */
    Net_InitDone = DEF_NO;                                      /* Block net fncts/tasks until init complete.           */
    CPU_CRITICAL_EXIT();

                                                                /* --------------- VALIDATE OS SERVICE ---------------- */
    is_en  = KAL_FeatureQuery(KAL_FEATURE_DLY,          KAL_OPT_DLY_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_LOCK_CREATE,  KAL_OPT_CREATE_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_LOCK_ACQUIRE, KAL_OPT_PEND_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_LOCK_RELEASE, KAL_OPT_POST_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_Q_CREATE,     KAL_OPT_CREATE_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_Q_PEND,       KAL_OPT_PEND_BLOCKING);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_Q_POST,       KAL_OPT_POST_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_SEM_CREATE,   KAL_OPT_CREATE_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_SEM_PEND,     KAL_OPT_PEND_BLOCKING);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_SEM_POST,     KAL_OPT_PEND_BLOCKING);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_SEM_ABORT,    KAL_OPT_ABORT_NONE);
    is_en &= KAL_FeatureQuery(KAL_FEATURE_TASK_CREATE,  KAL_OPT_CREATE_NONE);

    if (is_en != DEF_YES) {
        return (NET_INIT_ERR_OS_FEATURE_NOT_EN);
    }


                                                                /* ---------------- INIT NET DFLT VALS ---------------- */
    Net_InitDflt();                                             /* Init cfg vals to dflt vals (see Note #3a).           */



                                                                /* --------------- PERFORM NET/OS INIT ---------------- */
    Net_KAL_Init(&err);                                         /* Create net obj(s) [see Note #3b].                    */
    if (err != NET_ERR_NONE) {
        goto exit_fail_delete_obj;
    }

                                                                /* ----------------- INIT NET MODULES ----------------- */
                                                                /* Init net rsrc mgmt module(s).                        */
    NetCtr_Init();
    NetStat_Init();

    NetTmr_Init(tmr_task_cfg, &err);
    if (err != NET_TMR_ERR_NONE) {
        goto exit_fail_delete_obj;
    }

    NetBuf_Init();
    NetConn_Init();


                                                                /* Init net layers.                                     */
    NetMgr_Init(&err);
    if (err != NET_MGR_ERR_NONE) {
        goto exit_fail_delete_obj;
    }

    NetIP_Init(&err);
    if (err != NET_ERR_NONE) {
        goto exit_fail_delete_obj;
    }

    NetICMP_Init(&err);
    if (err != NET_ICMP_ERR_NONE) {
        goto exit_fail_delete_obj;
    }

#ifdef  NET_IGMP_MODULE_EN
    NetIGMP_Init();
#endif

                                                                /* Init net transport layers.                           */
    NetUDP_Init();

#ifdef  NET_TCP_MODULE_EN
    NetTCP_Init(&err);
    if (err != NET_TCP_ERR_NONE) {
        goto exit_fail_delete_obj;
    }
#endif

                                                                /* Init net app layers.                                 */
    NetSock_Init(&err);
    if (err != NET_SOCK_ERR_NONE) {
        goto exit_fail_delete_obj;
    }


#ifdef  NET_SOCK_BSD_EN
    NetBSD_Init(&err);
    if (err != NET_ERR_NONE) {
        goto exit_fail_delete_obj;
    }
#endif

                                                                /* Init net secure module.                              */
#ifdef  NET_SECURE_MODULE_EN
    NetSecure_Init(&err);
    if (err != NET_SECURE_ERR_NONE) {
        goto exit_fail_delete_obj;
    }
#endif


    CPU_CRITICAL_ENTER();                                       /* See Note #5.                                         */
    Net_InitDone = DEF_YES;                                     /* Signal net fncts/tasks that init complete.           */
    CPU_CRITICAL_EXIT();

                                                                /* Init   net IF module(s) [see Note #3d].              */
    NetIF_Init(rx_task_cfg, tx_task_cfg, &err);
    if (err != NET_IF_ERR_NONE) {
        CPU_CRITICAL_ENTER();
        Net_InitDone = DEF_NO;
        CPU_CRITICAL_EXIT();
        goto exit_fail_delete_obj;
    }

#ifdef  NET_NDP_MODULE_EN
    NetNDP_Init(&err);
    if (err != NET_NDP_ERR_NONE) {
        goto exit_fail_delete_obj;

    }
    NetMLDP_Init();
#endif



                                                                /* ------------- SIGNAL NET INIT COMPLETE ------------- */
    for (i = 0u; i < NET_TASK_NBR; i++) {                       /* Signal ALL net   tasks that init complete.           */
        Net_InitCompSignal(&err);
        if (err != NET_ERR_NONE) {
            CPU_CRITICAL_ENTER();
            Net_InitDone = DEF_NO;
            CPU_CRITICAL_EXIT();
            goto exit_fail_delete_obj;
        }
    }

    err = NET_ERR_NONE;
    goto exit;

exit_fail_delete_obj:
                                                                /* Not yet possible to free Pools, delete task, etc.    */
exit:
    return (err);
}


/*
*********************************************************************************************************
*                                            Net_KAL_Init()
*
* Description : (1) Perform network/OS initialization :
*
*                   (a) KAL initialization for the OS used.
*
*                   (b) Implement network initialization signal by creating a counting semaphore.
*
*                       (1) Initialize network initialization signal with no signal by setting the
*                           semaphore count to 0 to block the semaphore.
*
*                   (c) Implement global network lock by creating a binary semaphore.
*
*                       (1) Initialize network   lock as released by setting the semaphore count to 1.
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                        Network/OS initialization successful.
*
*                               NET_INIT_ERR_SIGNAL_CREATE          Network    initialization signal
*                                                                       NOT successfully initialized.
*
*                               NET_INIT_ERR_GLOBAL_LOCK_CREATE     Network    lock           signal
*                                                                       NOT successfully initialized.
*
*                               NET_ERR_FAULT_MEM_ALLOC             Error in memory allocation.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static void  Net_KAL_Init (NET_ERR  *p_err)
{
    KAL_ERR  err_kal;


    KAL_Init(DEF_NULL, &err_kal);
    if (err_kal != KAL_ERR_NONE) {
       *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
        return;
    }

                                                                /* ------------ INITIALIZE NETWORK SIGNAL ------------- */
                                                                /* Create network initialization signal ...             */
                                                                /* ... with NO network tasks signaled (see Note #1b1).  */
    Net_InitSignal = KAL_SemCreate((const CPU_CHAR *)NET_SIGNAL_INIT_NAME,
                                                     DEF_NULL,
                                                    &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
             *p_err = NET_ERR_FAULT_MEM_ALLOC;
              return;


        case KAL_ERR_ISR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_CREATE:
        default:
            *p_err = NET_INIT_ERR_SIGNAL_CREATE;
             return;
    }


                                                                /* ------------- INITIALIZE NETWORK LOCK -------------- */
                                                                /* Create network lock signal (see Note #1c).           */
    Net_GlobaLockFcntPtr = DEF_NULL;
   (void)&Net_GlobaLockFcntPtr;
    Net_GlobalLock       = KAL_LockCreate((const CPU_CHAR *)NET_LOCK_GLOBAL_NAME,
                                                            DEF_NULL,
                                                           &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_ERR_FAULT_MEM_ALLOC;
             goto fail_del_init_signal;


        case KAL_ERR_CREATE:
        default:
            *p_err = NET_INIT_ERR_GLOBAL_LOCK_CREATE;
             goto fail_del_init_signal;
    }


   *p_err = NET_ERR_NONE;
    return;

fail_del_init_signal:
    KAL_SemDel(Net_InitSignal,  &err_kal);
}


/*
*********************************************************************************************************
*                                          Net_InitCompWait()
*
* Description : Wait on signal indicating network initialization is complete.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                    Initialization signal     received.
*                               NET_INIT_ERR_COMP_SIGNAL_FAULT  Initialization signal NOT received.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxTaskHandler(),
*               NetTmr_TaskHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network initialization signal MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                   (a) Failure to acquire signal will prevent network task(s) from running.
*********************************************************************************************************
*/

void  Net_InitCompWait (NET_ERR  *p_err)
{
    KAL_ERR  err_kal;


    KAL_SemPend(Net_InitSignal, KAL_OPT_PEND_NONE, KAL_TIMEOUT_INFINITE, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_ERR_NONE;
             break;


        case KAL_ERR_ABORT:
        case KAL_ERR_ISR:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_TIMEOUT:
        case KAL_ERR_OS:
        default:
            *p_err = NET_INIT_ERR_COMP_SIGNAL_FAULT;            /* See Note #1a.                                        */
             break;
    }
}


/*
*********************************************************************************************************
*                                        Net_InitCompSignal()
*
* Description : Signal that network initialization is complete.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                 Network initialization     successfully signaled.
*                               NET_INIT_ERR_SIGNAL_COMPL    Network initialization NOT successfully signaled.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network initialization MUST be signaled--i.e. MUST signal without failure.
*
*                   (a) Failure to signal will prevent network task(s) from running.
*********************************************************************************************************
*/

void  Net_InitCompSignal (NET_ERR  *p_err)
{
    KAL_ERR  err_kal;


    KAL_SemPost(Net_InitSignal, KAL_OPT_PEND_NONE, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_ERR_NONE;
             break;


        case KAL_ERR_OVF:
        case KAL_ERR_OS:
        default:
            *p_err = NET_INIT_ERR_SIGNAL_COMPL;                 /* See Note #1a.                                        */
             break;
    }
}


/*
*********************************************************************************************************
*                                        Net_GlobalLockAcquire()
*
* Description : Acquire mutually exclusive access to network protocol suite.
*
* Argument(s) : p_fcnt  Pointer to the caller.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_ERR_NONE                  Network access     acquired.
*                           NET_ERR_FAULT_LOCK_ACQUIRE    Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) Network access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                       (1) Failure to acquire network access will prevent network task(s)/operation(s)
*                           from functioning.
*
*                   (b) Network access MUST be acquired exclusively by only a single task at any one time.
*********************************************************************************************************
*/

void  Net_GlobalLockAcquire (void     *p_fcnt,
                             NET_ERR  *p_err)
{
    KAL_ERR  err_kal;

                                                                /* Acquire exclusive network access (see Note #1b) ...  */
                                                                /* ... without timeout              (see Note #1a) ...  */
    KAL_LockAcquire(Net_GlobalLock, KAL_OPT_PEND_NONE, KAL_TIMEOUT_INFINITE, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             Net_GlobaLockFcntPtr = p_fcnt;
            *p_err = NET_ERR_NONE;
             break;


        case KAL_ERR_LOCK_OWNER:
            *p_err = NET_ERR_FAULT_LOCK_ACQUIRE;
             return;


        case KAL_ERR_NULL_PTR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ABORT:
        case KAL_ERR_TIMEOUT:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
            *p_err = NET_ERR_FAULT_LOCK_ACQUIRE;                               /* See Note #1a1.                                       */
             break;
    }
}


/*
*********************************************************************************************************
*                                        Net_GlobalLockRelease()
*
* Description : Release mutually exclusive access to network protocol suite.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network access MUST be released--i.e. MUST unlock access without failure.
*
*                   (a) Failure to release network access will prevent network task(s)/operation(s) from
*                       functioning.  Thus, network access is assumed to be successfully released since
*                       NO uC/OS-III error handling could be performed to counteract failure.
*********************************************************************************************************
*/

void  Net_GlobalLockRelease (void)
{
    KAL_ERR  err_kal;


    Net_GlobaLockFcntPtr = DEF_NULL;
    KAL_LockRelease(Net_GlobalLock,  &err_kal);                 /* Release exclusive network access.                    */


   (void)&err_kal;                                               /* See Note #1a.                                        */
}


/*
*********************************************************************************************************
*                                           Net_InitDflt()
*
* Description : Initialize default values for network protocol suite configurable parameters.
*
*               (1) Network protocol suite configurable parameters MUST be initialized PRIOR to all other
*                   network initialization.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function but MAY be called by
*               application/initialization functions (see Note #2b1B).
*
* Note(s)     : (3) Ignores configuration functions' return value indicating configuration success/failure.
*********************************************************************************************************
*/

void  Net_InitDflt (void)
{
                                                                /* ----------- CFG NET CONN INIT DFLT VALS ------------ */
   (void)NetConn_CfgAccessedTh(NET_CONN_ACCESSED_TH_DFLT);





                                                                /* ------------ CFG NET IF INIT DFLT VALS ------------- */
   (void)NetIF_CfgPhyLinkPeriod(NET_IF_PHY_LINK_TIME_DFLT_MS);
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
   (void)NetIF_CfgPerfMonPeriod(NET_IF_PERF_MON_TIME_DFLT_MS);
#endif




#ifdef  NET_ARP_MODULE_EN                                  /* -------------- CFG ARP INIT DFLT VALS -------------- */
   (void)NetARP_CfgCacheTimeout(NET_ARP_CACHE_TIMEOUT_DFLT_SEC);
   (void)NetARP_CfgCacheTxQ_MaxTh(NET_ARP_CACHE_TX_Q_TH_DFLT);
   (void)NetARP_CfgCacheAccessedTh(NET_ARP_CACHE_ACCESSED_TH_DFLT);
   (void)NetARP_CfgReqTimeout(NET_ARP_REQ_RETRY_TIMEOUT_DFLT_SEC);
   (void)NetARP_CfgReqMaxRetries(NET_ARP_REQ_RETRY_DFLT);
#endif




#ifdef  NET_NDP_MODULE_EN
                                                                /* -------------- CFG NDP INIT DFLT VALS -------------- */
   (void)NetNDP_CfgNeighborCacheTimeout(NET_NDP_CACHE_TIMEOUT_DFLT_SEC);
   (void)NetNDP_CfgReachabilityTimeout(NET_NDP_TIMEOUT_REACHABLE, NET_NDP_REACHABLE_TIMEOUT_SEC);
   (void)NetNDP_CfgReachabilityTimeout(NET_NDP_TIMEOUT_SOLICIT,   NET_NDP_RETRANS_TIMEOUT_SEC);
   (void)NetNDP_CfgReachabilityTimeout(NET_NDP_TIMEOUT_DELAY,     NET_NDP_DELAY_FIRST_PROBE_TIMEOUT_SEC);
   (void)NetNDP_CfgSolicitMaxNbr(NET_NDP_SOLICIT_MULTICAST,       NET_NDP_SOLICIT_MAX_MULTICAST);
   (void)NetNDP_CfgSolicitMaxNbr(NET_NDP_SOLICIT_UNICAST,         NET_NDP_SOLICIT_MAX_UNICAST);
#ifdef  NET_DAD_MODULE_EN
   (void)NetNDP_CfgSolicitMaxNbr(NET_NDP_SOLICIT_DAD,             NET_NDP_CFG_DAD_MAX_NBR_ATTEMPTS);
#endif
   (void)NetCache_CfgAccessedTh(NET_CACHE_TYPE_NDP, NET_NDP_CACHE_ACCESSED_TH_DFLT);
   (void)NetNDP_CfgCacheTxQ_MaxTh(NET_NDP_CACHE_TX_Q_TH_DFLT);
#endif




                                                                /* -------------- CFG IP INIT DFLT VALS --------------- */
#ifdef  NET_IPv4_MODULE_EN
   (void)NetIPv4_CfgFragReasmTimeout(NET_IPv4_FRAG_REASM_TIMEOUT_DFLT_SEC);
#endif


                                                                /* ------------- CFG ICMP INIT DFLT VALS -------------- */


#ifdef  NET_TCP_MODULE_EN                                       /* -------------- CFG TCP INIT DFLT VALS -------------- */
                                                                /* NOT yet implemented (remove if unnecessary).         */
#endif
}


/*
*********************************************************************************************************
*                                          Net_VersionGet()
*
* Description : Get network protocol suite software version.
*
* Argument(s) : none.
*
* Return(s)   : Network protocol suite software version (see Note #1b).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) The network protocol suite software version is denoted as follows :
*
*                       Vx.yy.zz
*
*                           where
*                                   V               denotes 'Version' label
*                                   x               denotes     major software version revision number
*                                   yy              denotes     minor software version revision number
*                                   zz              denotes sub-minor software version revision number
*
*                   (b) The software version is returned as follows :
*
*                           ver = x.yyzz * 100 * 100
*
*                               where
*                                       ver         denotes software version number scaled as an integer value
*                                       x.yyzz      denotes software version number, where the unscaled integer
*                                                       portion denotes the major version number & the unscaled
*                                                       fractional portion denotes the (concatenated) minor
*                                                       version numbers
*
*                   See also 'net.h  NETWORK VERSION NUMBER  Note #1'.
*********************************************************************************************************
*/

CPU_INT16U  Net_VersionGet (void)
{
    CPU_INT16U  ver;


    ver = Net_Version;

    return (ver);
}


/*
*********************************************************************************************************
*                                           Net_TimeDly()
*
* Description : Delay for specified time, in seconds & microseconds.
*
* Argument(s) : time_dly_sec    Time delay value, in      seconds (see Note #1).
*
*               time_dly_us     Time delay value, in microseconds (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                    Time delay successful.
*                               NET_ERR_TIME_DLY_MAX            Time delay successful but limited to
*                                                                   maximum OS time delay.
*
* Return(s)   : none.
*
* Caller(s)   : NetSock_Sel().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) Time delay of 0 seconds/microseconds allowed.
*
*                   (b) Time delay limited to the maximum possible OS time delay
*                             if greater than the maximum possible OS time delay.
*
*               (2) KAL does NOT support microsecond time values :
*
*                   (a) Microsecond timeout    values provided for network microsecond timeout values, but
*                       are rounded to millisecond timeout values.
*
*                   (b) Microsecond time delay values NOT supported.
*
*               (3) To avoid macro integer overflow, an OS timeout tick threshold value MUST be configured
*                   to avoid values that overflow the target CPU &/or compiler environment.
*********************************************************************************************************
*/

void  Net_TimeDly (CPU_INT32U   time_dly_sec,
                   CPU_INT32U   time_dly_us,
                   NET_ERR     *p_err)
{
    CPU_INT32U  time_dly_ms;


    if ((time_dly_sec < 1) &&                                   /* If zero time delay requested, ..                     */
        (time_dly_us  < 1)) {
       *p_err = NET_ERR_NONE;                                   /* .. exit time delay (see Note #1a).                   */
        return;
    }


                                                                /* Calculate us time delay's millisecond value, ..      */
                                                                /* .. rounded up to next millisecond.                   */
    time_dly_ms = NetUtil_TimeSec_uS_To_ms(time_dly_sec, time_dly_us);
    if (time_dly_ms == NET_TMR_TIME_INFINITE) {
       *p_err = NET_ERR_TIME_DLY_MAX;
    } else {
       *p_err = NET_ERR_NONE;
    }

                                                                /* Delay for calculated time delay.                     */
    KAL_Dly(time_dly_ms);
}

