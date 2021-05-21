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
*                                    NETWORK INTERFACE MANAGEMENT
*
* Filename : net_if.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Network Interface modules located in the following network directory :
*
*                (a) \<Network Protocol Suite>\IF\net_if.*
*                                                 net_if_*.*
*
*                        where
*                                <Network Protocol Suite>      directory path for network protocol suite
*                                 net_if.*                     Generic  Network Interface Management
*                                                                  module files
*                                 net_if_*.*                   Specific Network Interface(s) module files
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_IF_MODULE


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_if.h"
#include  "../Source/net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_arp.h"
#include  "../IP/IPv4/net_ipv4.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#include  "../IP/IPv6/net_mldp.h"
#endif


#ifdef  NET_IF_802x_MODULE_EN
#include  "net_if_802x.h"
#endif

#ifdef  NET_IF_ETHER_MODULE_EN
#include  "net_if_ether.h"
#endif

#ifdef  NET_IF_LOOPBACK_MODULE_EN
#include  "net_if_loopback.h"
#endif

#ifdef  NET_IF_WIFI_MODULE_EN
#include  "net_if_wifi.h"
#endif

#ifdef  NET_IGMP_MODULE_EN
#include  "../IP/IPv4/net_igmp.h"
#endif


#include  "../Source/net.h"
#include  "../Source/net_udp.h"
#include  "../Source/net_tcp.h"
#include  "../Source/net_mgr.h"
#include  "../Source/net_util.h"

#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* -------------------- TASK NAMES -------------------- */
#define  NET_IF_RX_TASK_NAME                "Net IF Rx Task"
#define  NET_IF_TX_DEALLOC_TASK_NAME        "Net IF Tx Dealloc Task"


                                                                /* -------------------- OBJ NAMES --------------------- */
#define  NET_IF_RX_Q_NAME                   "Net IF Rx Q"
#define  NET_IF_TX_DEALLOC_Q_NAME           "Net IF Tx Dealloc Q"
#define  NET_IF_TX_SUSPEND_NAME             "Net IF Tx Suspend"

#define  NET_IF_LINK_SUBSCRIBER             "Net IF Link subscriber pool"

#define  NET_IF_CFG_NAME                    "Cfg lock"

#define  NET_IF_DEV_TX_RDY_NAME             "Net IF Dev Tx Rdy"



#define  NET_IF_TX_SUSPEND_TIMEOUT_MIN_MS                  0
#define  NET_IF_TX_SUSPEND_TIMEOUT_MAX_MS                100


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

static  KAL_TASK_HANDLE  NetIF_RxTaskHandle;
static  KAL_TASK_HANDLE  NetIF_TxDeallocTaskHandle;

static  KAL_Q_HANDLE     NetIF_RxQ_Handle;
static  KAL_Q_HANDLE     NetIF_TxQ_Handle;



static  NET_IF          NetIF_Tbl[NET_IF_NBR_IF_TOT];       /* Net IF tbl.                                          */
static  NET_IF_NBR      NetIF_NbrBase;                      /* Net IF tbl base nbr.                                 */
static  NET_IF_NBR      NetIF_NbrNext;                      /* Net IF tbl next nbr to cfg.                          */


static  NET_STAT_CTR    NetIF_RxTaskPktCtr;                 /*        Net IF rx task q'd pkts ctr.                  */

static  NET_IF_Q_SIZE   NetIF_RxQ_SizeCfgd;                 /*        Net IF rx q cfg'd size.                       */
static  NET_IF_Q_SIZE   NetIF_RxQ_SizeCfgdRem;              /*        Net IF rx q cfg'd size rem'ing.               */


static  NET_BUF        *NetIF_TxListHead;                   /* Ptr to net IF tx list head.                          */
static  NET_BUF        *NetIF_TxListTail;                   /* Ptr to net IF tx list tail.                          */

static  NET_IF_Q_SIZE   NetIF_TxDeallocQ_SizeCfgd;          /*        Net IF tx dealloc cfg'd size.                 */
static  NET_IF_Q_SIZE   NetIF_TxDeallocQ_SizeCfgdRem;       /*        Net IF tx dealloc cfg'd size rem'ing.         */


static  NET_TMR        *NetIF_PhyLinkStateTmr;              /* Phy link state tmr.                                  */
static  CPU_INT16U      NetIF_PhyLinkStateTime_ms;          /* Phy link state time (in ms   ).                      */
static  NET_TMR_TICK    NetIF_PhyLinkStateTime_tick;        /* Phy link state time (in ticks).                      */

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
static  NET_TMR        *NetIF_PerfMonTmr;                   /* Perf mon       tmr.                                  */
static  CPU_INT16U      NetIF_PerfMonTime_ms;               /* Perf mon       time (in ms   ).                      */
static  NET_TMR_TICK    NetIF_PerfMonTime_tick;             /* Perf mon       time (in ticks).                      */

static  CPU_BOOLEAN     NetIF_CtrsResetEn = DEF_NO;         /* Variable added for uC-Probe to reset counters.       */
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/


static  void           NetIF_InitTaskObj                (const  NET_TASK_CFG       *p_rx_task_cfg,
                                                         const  NET_TASK_CFG       *p_tx_task_cfg,
                                                                NET_ERR            *p_err);

static  void           NetIF_ObjInit                    (       NET_IF             *p_if,
                                                                NET_ERR            *p_err);

static  void           NetIF_ObjDel                     (       NET_IF             *p_if);

static  void           NetIF_DevTxRdyWait               (       NET_IF             *p_if,
                                                                NET_ERR            *p_err);

                                                                /* --------------------- RX FNCTS --------------------- */
static  void           NetIF_RxTask                     (       void               *p_data);

static  void           NetIF_RxTaskHandler              (       void);

static  NET_IF_NBR     NetIF_RxTaskWait                 (       NET_ERR            *p_err);

static  void           NetIF_RxHandler                  (       NET_IF_NBR          if_nbr);

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void           NetIF_RxHandlerLoadBal           (       NET_IF             *p_if);
#endif


static  NET_BUF_SIZE   NetIF_RxPkt                      (       NET_IF             *p_if,
                                                                NET_ERR            *p_err);

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void           NetIF_RxPktDec                   (       NET_IF             *p_if);
#endif

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void           NetIF_RxPktDiscard               (       NET_BUF            *p_buf,
                                                                NET_ERR            *p_err);
#endif

static  void           NetIF_RxQ_SizeCfg                (       NET_IF_Q_SIZE       size);


                                                                /* --------------------- TX FNCTS --------------------- */
static  void           NetIF_TxDeallocTask              (       void               *p_data);

static  void           NetIF_TxDeallocTaskHandler       (       void);

                                                                /* Wait for dev tx comp signal.                         */
static  CPU_INT08U    *NetIF_TxDeallocTaskWait          (       NET_ERR            *p_err);

static  void           NetIF_TxDeallocQ_SizeCfg         (       NET_IF_Q_SIZE       size);

static  void           NetIF_TxHandler                  (       NET_BUF            *p_buf,
                                                                NET_ERR            *p_err);

static  NET_BUF_SIZE   NetIF_TxPkt                      (       NET_IF             *p_if,
                                                                NET_BUF            *p_buf,
                                                                NET_ERR            *p_err);

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void           NetIF_TxPktValidate              (       NET_IF             *p_if,
                                                                NET_BUF_HDR        *p_buf_hdr,
                                                                NET_ERR            *p_err);
#endif

static  void           NetIF_TxPktListDealloc           (       CPU_INT08U         *p_buf_data);

static  NET_BUF       *NetIF_TxPktListSrch              (       CPU_INT08U         *p_buf_data);

static  void           NetIF_TxPktListInsert            (       NET_BUF            *p_buf);

static  void           NetIF_TxPktListRemove            (       NET_BUF            *p_buf);


static  void           NetIF_TxPktFree                  (       NET_BUF            *p_buf);

static  void           NetIF_TxPktDiscard               (       NET_BUF            *p_buf,
                                                                CPU_BOOLEAN         inc_ctrs,
                                                                NET_ERR            *p_err);


#ifdef  NET_LOAD_BAL_MODULE_EN
static  void           NetIF_TxSuspendTimeoutInit       (       NET_IF             *p_if,
                                                                NET_ERR            *p_err);

static  void           NetIF_TxSuspendSignal            (       NET_IF             *p_if);

static  void           NetIF_TxSuspendWait              (       NET_IF             *p_if);
#endif



                                                                /* ------------------ HANDLER FNCTS ------------------- */
static  void           NetIF_BufPoolCfgValidate         (       NET_IF_NBR          if_nbr,
                                                                NET_DEV_CFG        *p_dev_cfg,
                                                                NET_ERR            *p_err);


static  void          *NetIF_GetDataAlignPtr            (       NET_IF_NBR          if_nbr,
                                                                NET_TRANSACTION     transaction,
                                                                void               *p_data,
                                                                NET_ERR            *p_err);

static  CPU_INT16U     NetIF_GetProtocolHdrSize         (       NET_IF             *p_if,
                                                                NET_PROTOCOL_TYPE   protocol,
                                                                NET_ERR            *p_err);


static  void           NetIF_IO_CtrlHandler             (       NET_IF_NBR          if_nbr,
                                                                CPU_INT08U          opt,
                                                                void               *p_data,
                                                                NET_ERR            *p_err);


#ifndef  NET_CFG_LINK_STATE_POLL_DISABLED
static  void           NetIF_PhyLinkStateHandler        (       void               *p_obj);
#endif


#if  (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
static  void           NetIF_PerfMonHandler             (       void               *p_obj);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN

#ifndef  NET_IF_CFG_TX_SUSPEND_TIMEOUT_MS
#error  "NET_IF_CFG_TX_SUSPEND_TIMEOUT_MS        not #define'd in 'net_cfg.h'            "
#error  "                                  [MUST be  >= NET_IF_TX_SUSPEND_TIMEOUT_MIN_MS]"
#error  "                                  [     &&  <= NET_IF_TX_SUSPEND_TIMEOUT_MAX_MS]"

#elif   (DEF_CHK_VAL(NET_IF_CFG_TX_SUSPEND_TIMEOUT_MS,          \
                     NET_IF_TX_SUSPEND_TIMEOUT_MIN_MS,          \
                     NET_IF_TX_SUSPEND_TIMEOUT_MAX_MS) != DEF_OK)
#error  "NET_IF_CFG_TX_SUSPEND_TIMEOUT_MS  illegally #define'd in 'net_cfg.h'            "
#error  "                                  [MUST be  >= NET_IF_TX_SUSPEND_TIMEOUT_MIN_MS]"
#error  "                                  [     &&  <= NET_IF_TX_SUSPEND_TIMEOUT_MAX_MS]"
#endif

#endif


/*
*********************************************************************************************************
*                                            NetIF_Init()
*
* Description : (1) Initialize the Network  Interface Management Module :
*
*                   (a) Perform    Network Interface/OS initialization
*                   (b) Initialize Network Interface table
*                   (c) Initialize Network Interface counter(s)
*                   (d) Initialize Network Interface transmit list pointers
*                   (e) Initialize Network Interface timers
*                   (f) Initialize Network Interface(s)/Layer(s) :
*                       (1) (A) Initialize          Loopback Interface
*                           (B) Initialize specific network  interface(s)
*                       (2) Initialize ARP Layer
*
*
* Argument(s) : p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                         Network interface module successfully
*                                                                           initialized.
*
*                               -------------------- RETURNED BY NetIF_InitTaskObj() : ----------------------
*                               See NetIF_InitTaskObj() for additional return error codes.
*
*                               -------------------- RETURNED BY NetIF_Loopback_Init() : --------------------
*                               See NetIF_Loopback_Init() for additional return error codes.
*
*                               ---------------------- RETURNED BY NetIF_&&&_Init() : -----------------------
*                                See specific network interface(s) 'Init()' for additional return error codes.
*
*                               ------------------------ RETURNED BY NetTmr_Get() : -------------------------
*                               See NetTmr_Get() for additional return error codes.
*
*                               ---------------------- RETURNED BY NetStat_CtrInit() : ----------------------
*                               See NetStat_CtrInit() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) The following network interface initialization functions MUST be sequenced as follows :
*
*                   (a) Net_IF_KAL_Init()  MUST precede ALL other network interface initialization functions
*                   (b) NetIF_&&&_Init()'s MUST follow  ALL other network interface initialization functions
*
*               (3) Regardless of whether the Loopback interface is enabled or disabled, network interface
*                   numbers MUST be initialized to reserve the lowest possible network interface number
*                   ('NET_IF_NBR_LOOPBACK') for the Loopback interface.
*
*                           NetIF_NbrLoopback
*                           NetIF_NbrBaseCfgd
*********************************************************************************************************
*/

void  NetIF_Init (const  NET_TASK_CFG  *p_rx_task_cfg,
                  const  NET_TASK_CFG  *p_tx_task_cfg,
                         NET_ERR       *p_err)
{
    NET_IF        *p_if;
    NET_IF_NBR     if_nbr;
    NET_TMR_TICK   timeout_tick;
    CPU_SR_ALLOC();


                                                                        /* ---------- PERFORM NET IF/OS INIT ---------- */
    NetIF_InitTaskObj(p_rx_task_cfg, p_tx_task_cfg, p_err);             /* Create net IF obj(s)/task(s) [see Note #2a]. */
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }




                                                                        /* --------------- INIT IF TBL ---------------- */
    p_if = &NetIF_Tbl[0];
    for (if_nbr = 0u; if_nbr < NET_IF_NBR_IF_TOT; if_nbr++) {
        p_if->Nbr      =  NET_IF_NBR_NONE;
        p_if->Type     =  NET_IF_TYPE_NONE;
        p_if->Init     =  DEF_NO;
        p_if->En       =  DEF_DISABLED;
        p_if->Link     =  NET_IF_LINK_DOWN;
        p_if->MTU      =  0u;
        p_if->IF_API   = (void *)0;
        p_if->IF_Data  = (void *)0;
        p_if->Dev_API  = (void *)0;
        p_if->Dev_Cfg  = (void *)0;
        p_if->Dev_Data = (void *)0;
        p_if->Ext_API  = (void *)0;
        p_if->Ext_Cfg  = (void *)0;

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
        p_if->PerfMonState      = NET_IF_PERF_MON_STATE_STOP;
        p_if->PerfMonTS_Prev_ms = NET_TS_NONE;
#else
       (void)&NetIF_NbrBase;
#endif

#ifdef  NET_LOAD_BAL_MODULE_EN
        NetStat_CtrInit(&p_if->RxPktCtr,     p_err);
        if (*p_err != NET_STAT_ERR_NONE) {
             return;
        }

        NetStat_CtrInit(&p_if->TxSuspendCtr, p_err);
        if (*p_err != NET_STAT_ERR_NONE) {
             return;
        }
#endif

        p_if++;
    }
                                                                        /* Init base/next IF nbrs (see Note #3).        */
#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    NetIF_NbrBase = NET_IF_NBR_BASE;
    NetIF_NbrNext = NET_IF_NBR_LOOPBACK;
#else
    NetIF_NbrBase = NET_IF_NBR_BASE_CFGD;
    NetIF_NbrNext = NET_IF_NBR_BASE_CFGD;
#endif


                                                                        /* ------------ INIT NET IF CTR's ------------- */
    NetStat_CtrInit(&NetIF_RxTaskPktCtr, p_err);
    if (*p_err != NET_STAT_ERR_NONE) {
         return;
    }


                                                                        /* ----------- INIT NET IF TX LIST ------------ */
    NetIF_TxListHead = DEF_NULL;
    NetIF_TxListTail = DEF_NULL;


                                                                        /* ------------ INIT NET IF TMR's ------------- */
    CPU_CRITICAL_ENTER();
    timeout_tick          = NetIF_PhyLinkStateTime_tick;
    CPU_CRITICAL_EXIT();

#ifndef  NET_CFG_LINK_STATE_POLL_DISABLED
    NetIF_PhyLinkStateTmr = NetTmr_Get((CPU_FNCT_PTR)&NetIF_PhyLinkStateHandler,
                                       (void       *) 0,
                                       (NET_TMR_TICK) timeout_tick,
                                       (NET_ERR    *) p_err);
    if (*p_err != NET_TMR_ERR_NONE) {
         return;
    }
#endif

   (void)&NetIF_PhyLinkStateTmr;

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    CPU_CRITICAL_ENTER();
    timeout_tick     = NetIF_PerfMonTime_tick;
    CPU_CRITICAL_EXIT();
    NetIF_PerfMonTmr = NetTmr_Get((CPU_FNCT_PTR)&NetIF_PerfMonHandler,
                                  (void       *) 0,
                                  (NET_TMR_TICK) timeout_tick,
                                  (NET_ERR    *) p_err);
    if (*p_err != NET_TMR_ERR_NONE) {
         return;
    }

   (void)&NetIF_PerfMonTmr;
#endif


                                                                        /* -------------- INIT NET IF(s) -------------- */
                                                                        /* See Note #2b.                                */
#ifdef  NET_IF_LOOPBACK_MODULE_EN
    NetIF_Loopback_Init(p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

#ifdef  NET_IF_ETHER_MODULE_EN
    NetIF_Ether_Init(p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

#ifdef  NET_IF_WIFI_MODULE_EN
    NetIF_WiFi_Init(p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

#ifdef  NET_ARP_MODULE_EN
    NetARP_Init(p_err);
    if (*p_err != NET_ARP_ERR_NONE) {
         return;
    }
#endif

   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetIF_BufPoolInit()
*
* Description : (1) Create network interface buffer memory pools :
*
*                   (a) Validate network interface buffer configuration
*                   (b) Create   network buffer memory pools :
*                       (1) Create receive  large buffer pool
*                       (2) Create transmit large buffer pool
*                       (3) Create transmit small buffer pool
*                       (4) Create network buffer header pool
*
*
* Argument(s) : p_if        Pointer to network interface.
*               ----        Argument validated in NetIF_Add().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Buffer pools successfully created.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               -- RETURNED BY NetIF_BufPoolCfgValidate() : --
*                               NET_IF_ERR_INVALID_POOL_TYPE    Invalid network interface buffer pool type.
*                               NET_IF_ERR_INVALID_POOL_ADDR    Invalid network interface buffer pool address.
*                               NET_IF_ERR_INVALID_POOL_SIZE    Invalid network interface buffer pool size.
*                               NET_IF_ERR_INVALID_POOL_QTY     Invalid network interface buffer pool number
*                                                                   of buffers configured.
*
*                                                               --- RETURNED BY NetBuf_PoolCfgValidate() : ---
*                               NET_BUF_ERR_INVALID_QTY         Invalid number of network buffers configured.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size   of network buffer data areas
*                                                                   configured.
*                               NET_BUF_ERR_INVALID_IX          Invalid offset from base index into network
*                                                                   buffer data area configured.
*
*                                                               ------ RETURNED BY NetBuf_PoolInit() : -------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_BUF_ERR_INVALID_POOL_TYPE   Invalid network buffer pool type.
*                               NET_BUF_ERR_POOL_MEM_ALLOC           Network buffer pool initialization failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add(),
*               NetIF_WiFi_IF_Add(),
*               NetIF_Loopback_IF_Add().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (2) (a) Each added network interfaces MUST decrement its total number of configured receive
*                       buffers from the remaining network interface receive queue configured size.
*
*                   (b) Each added network interfaces MUST decrement its total number of configured transmit
*                       buffers from the remaining network interface transmit deallocation queue configured
*                       size.
*
*                       (1) However, since the network loopback interface does NOT deallocate transmit
*                           packets via the network interface transmit deallocation task (see
*                           'net_if_loopback.c  NetIF_Loopback_Tx()  Note #4'); then the network interface
*                           transmit deallocation queue size does NOT need to be adjusted by the network
*                           loopback interface's number of configured transmit buffers.
*
*                   See also 'NetBuf_PoolCfgValidate()  Note #2'.
*
*               (3) Each network buffer data area allocates additional octets for its configured offset
*                   (see 'net_dev_cfg.c  EXAMPLE NETWORK INTERFACE / DEVICE CONFIGURATION  Note #5').  This
*                   ensures that each data area's effective, useable size still equals its configured size
*                   (see 'net_dev_cfg.c  EXAMPLE NETWORK INTERFACE / DEVICE CONFIGURATION  Note #2').
*********************************************************************************************************
*/

void  NetIF_BufPoolInit (NET_IF   *p_if,
                         NET_ERR  *p_err)
{
    NET_IF_NBR    if_nbr;
    NET_IF_API   *p_if_api;
    NET_DEV_CFG  *p_dev_cfg;
    void         *p_addr;
    CPU_ADDR      size;
    CPU_SIZE_T    size_buf;
    CPU_SIZE_T    reqd_octets;
    NET_BUF_QTY   nbr_bufs_tot;


                                                                /* ------------- VALIDATE NET IF BUF CFG -------------- */
    if_nbr    = (NET_IF_NBR   )p_if->Nbr;
    p_if_api  = (NET_IF_API  *)p_if->IF_API;
    p_dev_cfg = (NET_DEV_CFG *)p_if->Dev_Cfg;

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->BufPoolCfgValidate == (void (*)(NET_IF  *,
                                                  NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

    NetBuf_PoolCfgValidate(p_if->Type, p_dev_cfg, p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         return;
    }

    NetIF_BufPoolCfgValidate(if_nbr, p_dev_cfg, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

    p_if_api->BufPoolCfgValidate(p_if, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }


                                                                /* --------- INIT NET IF RX BUF POOL ---------- */
    if (p_dev_cfg->RxBufPoolType == NET_IF_MEM_TYPE_MAIN) {
        p_addr = (void   *)0;
        size  = (CPU_ADDR)0u;
    } else {
        p_addr = (void   *)p_dev_cfg->MemAddr;
        size  = (CPU_ADDR)p_dev_cfg->MemSize;
    }

    if (p_dev_cfg->RxBufLargeNbr > 0) {
        size_buf = p_dev_cfg->RxBufLargeSize +                  /* Inc cfg'd rx buf size by ...                         */
                   p_dev_cfg->RxBufIxOffset;                    /* ... cfg'd rx buf offset (see Note #3).               */
        NetBuf_PoolInit(if_nbr,
                        NET_BUF_TYPE_RX_LARGE,                  /* Create  large rx buf data area pool.                 */
                        p_addr,                                 /* Create  pool  in dedicated mem, if avail.            */
                        size,                                   /* Size  of dedicated mem, if avail.                    */
                        p_dev_cfg->RxBufLargeNbr,               /* Nbr  of large rx bufs to create.                     */
                        size_buf,                               /* Size of large rx bufs to create.                     */
                        p_dev_cfg->RxBufAlignOctets,            /* Align   large rx bufs to octet boundary.             */
                       &reqd_octets,
                        p_err);

        if (*p_err != NET_BUF_ERR_NONE) {
             return;
        }

        NetIF_RxQ_SizeCfgdRem -= p_dev_cfg->RxBufLargeNbr;      /* Dec rem'ing rx Q size by nbr large rx bufs ..        */
                                                                /* .. (see Note #2a).                                   */
    }


                                                                /* ------------- INIT NET IF TX BUF POOLS ------------- */
    if (p_dev_cfg->TxBufPoolType == NET_IF_MEM_TYPE_MAIN) {
        p_addr = (void   *)0;
        size  = (CPU_ADDR)0u;
    } else {
        p_addr = (void   *)p_dev_cfg->MemAddr;
        size  = (CPU_ADDR)p_dev_cfg->MemSize;
    }

    if (p_dev_cfg->TxBufLargeNbr > 0) {
        size_buf = p_dev_cfg->TxBufLargeSize +                  /* Inc cfg'd tx buf size by ...                         */
                   p_dev_cfg->TxBufIxOffset;                    /* ... cfg'd tx buf offset (see Note #3).               */
        NetBuf_PoolInit(if_nbr,
                        NET_BUF_TYPE_TX_LARGE,                  /* Create  large tx buf data area pool.                 */
                        p_addr,                                 /* Create  pool  in dedicated mem, if avail.            */
                        size,                                   /* Size  of dedicated mem, if avail.                    */
                        p_dev_cfg->TxBufLargeNbr,               /* Nbr  of large tx bufs to create.                     */
                        size_buf,                               /* Size of large tx bufs to create.                     */
                        p_dev_cfg->TxBufAlignOctets,            /* Align   large tx bufs to octet boundary.             */
                       &reqd_octets,
                        p_err);

        if (*p_err != NET_BUF_ERR_NONE) {
             return;
        }

        if (if_nbr != NET_IF_NBR_LOOPBACK) {                    /* For all non-loopback IF's (see Note #2b1),..         */
                                                                /* .. dec rem'ing tx dealloc Q size by nbr   ..         */
                                                                /* .. of large tx bufs (see Note #2b).                  */
            NetIF_TxDeallocQ_SizeCfgdRem -= p_dev_cfg->TxBufLargeNbr;
        }
    }

    if (p_dev_cfg->TxBufSmallNbr > 0) {
        size_buf = p_dev_cfg->TxBufSmallSize +                  /* Inc cfg'd tx buf size by ...                         */
                   p_dev_cfg->TxBufIxOffset;                    /* ... cfg'd tx buf offset (see Note #3).               */
        NetBuf_PoolInit(if_nbr,
                        NET_BUF_TYPE_TX_SMALL,                  /* Create  small tx buf data area pool.                 */
                        p_addr,                                 /* Create  pool  in dedicated mem, if avail.            */
                        size,                                   /* Size  of dedicated mem, if avail.                    */
                        p_dev_cfg->TxBufSmallNbr,               /* Nbr  of small tx bufs to create.                     */
                        size_buf,                               /* Size of small tx bufs to create.                     */
                        p_dev_cfg->TxBufAlignOctets,            /* Align   small tx bufs to octet boundary.             */
                       &reqd_octets,
                        p_err);

        if (*p_err != NET_BUF_ERR_NONE) {
             return;
        }

        if (if_nbr != NET_IF_NBR_LOOPBACK) {                    /* For all non-loopback IF's (see Note #2b1),..         */
                                                                /* .. dec rem'ing tx dealloc Q size by nbr   ..         */
                                                                /* .. of small tx bufs (see Note #2b).                  */
            NetIF_TxDeallocQ_SizeCfgdRem -= p_dev_cfg->TxBufSmallNbr;
        }
    }

                                                                /* --------------- INIT NET IF BUF POOL --------------- */
                                                                /* Calc IF's tot nbr net bufs.                          */
    nbr_bufs_tot = p_dev_cfg->RxBufLargeNbr +
                   p_dev_cfg->TxBufLargeNbr +
                   p_dev_cfg->TxBufSmallNbr;

    if (nbr_bufs_tot > 0) {
        NetBuf_PoolInit(if_nbr,
                        NET_BUF_TYPE_BUF,                       /* Create  net buf pool.                                */
                        0,                                      /* Create  net bufs from main mem heap.                 */
                        0u,
                        nbr_bufs_tot,                           /* Nbr  of net bufs to create.                          */
                        sizeof(NET_BUF),                        /* Size of net bufs.                                    */
                        sizeof(CPU_DATA),                       /* Align   net bufs to CPU data word size.              */
                       &reqd_octets,
                        p_err);

        if (*p_err != NET_BUF_ERR_NONE) {
             return;
        }
    }


   *p_err = NET_IF_ERR_NONE;
}



/*
*********************************************************************************************************
*                                      NetIF_CfgPhyLinkPeriod()
*
* Description : (1) Configure Network Interface Physical Link State Handler time (i.e. scheduling period) :
*
*                   (a) Validate  desired physical link state handler time
*                   (b) Configure desired physical link state handler time
*
*
* Argument(s) : time_ms     Desired value for Network Interface Physical Link State Handler time
*                               (in milliseconds).
*
* Return(s)   : DEF_OK,   Network Interface Physical Link State Handler timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API)
*               function & MAY be called by application function(s).
*
* Note(s)     : (2) Configured time does NOT reschedule the next physical link state handling but
*                   configures the scheduling of all subsequent  physical link state handling.
*
*               (3) Configured time converted to 'NET_TMR_TICK' ticks to avoid run-time conversion.
*
*               (4) 'NetIF_PhyLinkStateTime' variables MUST ALWAYS be accessed exclusively in critical
*                   sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_CfgPhyLinkPeriod (CPU_INT16U  time_ms)
{
    NET_TMR_TICK  time_tick;
    CPU_SR_ALLOC();


    if (time_ms < NET_IF_PHY_LINK_TIME_MIN_MS) {
        return (DEF_FAIL);
    }
    if (time_ms > NET_IF_PHY_LINK_TIME_MAX_MS) {
        return (DEF_FAIL);
    }

    time_tick                   = ((NET_TMR_TICK)time_ms * NET_TMR_TIME_TICK_PER_SEC) / DEF_TIME_NBR_mS_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetIF_PhyLinkStateTime_ms   = time_ms;
    NetIF_PhyLinkStateTime_tick = time_tick;
    CPU_CRITICAL_EXIT();

   (void)&NetIF_PhyLinkStateTime_ms;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                      NetIF_CfgPerfMonPeriod()
*
* Description : (1) Configure Network Interface Performance Monitor Handler time (i.e. scheduling period) :
*
*                   (a) Validate  desired performance monitor handler time
*                   (b) Configure desired performance monitor handler time
*
*
* Argument(s) : time_ms     Desired value for Network Interface Performance Monitor Handler time
*                               (in milliseconds).
*
* Return(s)   : DEF_OK,   Network Interface Performance Monitor Handler time configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API)
*               function & MAY be called by application function(s).
*
* Note(s)     : (2) Configured time does NOT reschedule the next performance monitor handling but
*                   configures the scheduling of all  subsequent performance monitor handling.
*
*               (3) Configured time converted to 'NET_TMR_TICK' ticks to avoid run-time conversion.
*
*               (4) 'NetIF_PerfMonTime' variables MUST ALWAYS be accessed exclusively in critical
*                   sections.
*********************************************************************************************************
*/

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
CPU_BOOLEAN  NetIF_CfgPerfMonPeriod (CPU_INT16U  time_ms)
{
    NET_TMR_TICK  time_tick;
    CPU_SR_ALLOC();


    if (time_ms < NET_IF_PERF_MON_TIME_MIN_MS) {
        return (DEF_FAIL);
    }
    if (time_ms > NET_IF_PERF_MON_TIME_MAX_MS) {
        return (DEF_FAIL);
    }

    time_tick              = ((NET_TMR_TICK)time_ms * NET_TMR_TIME_TICK_PER_SEC) / DEF_TIME_NBR_mS_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetIF_PerfMonTime_ms   = time_ms;
    NetIF_PerfMonTime_tick = time_tick;
    CPU_CRITICAL_EXIT();

   (void)&NetIF_PerfMonTime_ms;

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                              NetIF_Add()
*
* Description : (1) Add & initialize a specific instance of a network interface :
*
*                   (a) Acquire network lock
*                   (b) Validate   network interface :
*                       (1) Validate next network interface available
*                       (2) Validate      network interface parameters
*                   (c) Initialize network interface :
*                       (1) Configure  network interface parameters
*                       (2) Initialize network buffers & pools
*                       (3) Initialize specific network interface & device
*                   (d) Release network lock
*                   (e) Return  network interface number
*                         OR
*                       Null    network interface number & error code, on failure
*
*
* Argument(s) : if_api      Pointer to specific network interface API.
*
*               dev_api     Pointer to specific network device driver API.
*
*               dev_bsp     Pointer to specific network device board-specific API.
*
*               dev_cfg     Pointer to specific network device hardware configuration.
*
*               ext_api     Pointer to specific network extension layer API           (see Note #4).
*
*               ext_cfg     Pointer to specific network extension layer configuration (see Note #4).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Network interface successfully added.
*                               NET_ERR_FAULT_NULL_PTR                 Argument 'if_api'/'dev_cfg'/'dev_api'/'dev_bsp'
*                                                                       passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid/NULL network interface function pointer.
*                               NET_IF_ERR_INVALID_CFG              Invalid      network interface API configuration.
*                               NET_IF_ERR_INVALID_IF               NO available network interfaces
*                                                                       (see 'net_cfg_dev.h  NET_IF_NBR_IF_TOT').
*
*                               NET_DEV_ERR_NULL_PTR                Argument(s) 'dev_api'/'dev_bsp'/'dev_cfg'
*                                                                       passed a NULL pointer.
*                               NET_DEV_ERR_INVALID_CFG             Argument(s) 'phy_api'/'phy_cfg'/'phy_data'
*                                                                       incorrectly configured (see Note #4b).
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   ------- RETURNED BY NetIF_BufPoolInit() : --------
*                               NET_IF_ERR_INVALID_POOL_TYPE        Invalid network interface buffer pool type.
*                               NET_IF_ERR_INVALID_POOL_ADDR        Invalid network interface buffer pool address.
*                               NET_IF_ERR_INVALID_POOL_SIZE        Invalid network interface buffer pool size.
*                               NET_IF_ERR_INVALID_POOL_QTY         Invalid network interface buffer pool number
*                                                                       of buffers configured.
*
*                               NET_BUF_ERR_POOL_MEM_ALLOC               Network buffer pool initialization failed.
*                               NET_BUF_ERR_INVALID_POOL_TYPE       Invalid network buffer pool type.
*                               NET_BUF_ERR_INVALID_QTY             Invalid number of network buffers configured.
*                               NET_BUF_ERR_INVALID_SIZE            Invalid size   of network buffer data areas
*                                                                       configured.
*                               NET_BUF_ERR_INVALID_IX              Invalid offset from base index into network
*                                                                       buffer data area configured.
*
*                                                                   --------- RETURNED BY 'p_if_api->Add()' : ---------
*                               NET_ERR_FAULT_MEM_ALLOC             Insufficient resources available to add interface.
*
*                               NET_DEV_ERR_INVALID_CFG             Invalid/NULL network device API configuration.
*                               NET_ERR_FAULT_NULL_FNCT               Invalid NULL network device function pointer.
*
*                                                                   See specific network interface(s) 'Add()' for
*                                                                       additional return error codes.
*
*                                                                   ---- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : Interface number of the added interface, if NO error(s).
*
*               NET_IF_NBR_NONE,                         otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIF_Add() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) NetIF_Add() blocked until network initialization completes.
*
*                   See also 'net.c  Net_Init()  Note #3d'.
*
*               (4) (a) Network physical layer arguments MAY be NULL for any of the following :
*
*                       (1) The network device does not require   Physical layer support.
*                       (2) The network device uses an integrated Physical layer supported from within
*                           the network device &/or the network device driver.
*
*                   (b) However, if network physical layer API is available, then ALL network physical
*                       layer API arguments MUST be provided.
*
*               (5) (a) The following parameters MUST be configured PRIOR to initializing the specific
*                       network interface/device so that the initialized network interface is valid :
*
*                       (1) The network interface's initialization flag MUST be set; ...
*                       (2) The next available interface number MUST be incremented.
*
*                   (b) On ANY error(s), network interface parameters MUST be appropriately reset :
*
*                       (1) The network interface's initialization flag SHOULD be cleared; ...
*                       (2) The next available interface number MUST be decremented.
*********************************************************************************************************
*/

NET_IF_NBR  NetIF_Add (void     *if_api,
                       void     *dev_api,
                       void     *dev_bsp,
                       void     *dev_cfg,
                       void     *ext_api,
                       void     *ext_cfg,
                       NET_ERR  *p_err)
{
    NET_IF       *p_if     = DEF_NULL;
    NET_IF_API   *p_if_api;
    NET_IF_NBR    if_nbr   = NET_IF_NBR_NONE;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(DEF_NULL);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_Add, p_err);           /* See Note #2b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (NET_IF_NBR_NONE);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail_init;
    }
#endif


                                                                /* -------------- VALIDATE NET IF AVAIL --------------- */
    CPU_CRITICAL_ENTER();
    if_nbr = NetIF_NbrNext;                                     /* Get cur net IF nbr.                                  */
    CPU_CRITICAL_EXIT();
    if (if_nbr >= NET_IF_NBR_IF_TOT) {
       *p_err = NET_IF_ERR_INVALID_IF;
        goto exit_fail_init;
    }

    CPU_CRITICAL_ENTER();
    NetIF_NbrNext++;                                            /* Inc to next avail net IF nbr (see Note #5a2).        */
    CPU_CRITICAL_EXIT();



#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* --------------- VALIDATE IF API PTRS --------------- */
    if (if_api == DEF_NULL) {                                   /* If NULL IF API,                               ...    */
       *p_err = NET_ERR_FAULT_NULL_PTR;                         /* ... & rtn err.                                       */
        goto exit_fail;
    }

    if (dev_cfg == DEF_NULL) {                                  /* If NULL dev cfg,                              ...    */
       *p_err = NET_DEV_ERR_NULL_PTR;
        goto exit_fail;
    }

    if ((if_nbr  != NET_IF_NBR_LOOPBACK) &&
        (dev_api == DEF_NULL)){                                 /* For non-loopback IF's,                        ...    */
                                                                /* ... or NULL dev BSP;                          ...    */
        *p_err = NET_DEV_ERR_NULL_PTR;
         goto exit_fail;
    }
#endif


                                                                /* ------------------- INIT NET IF -------------------- */
    p_if           = &NetIF_Tbl[if_nbr];
    p_if->Nbr      =  if_nbr;
    p_if->IF_API   =  if_api;
    p_if->Dev_API  =  dev_api;
    p_if->Dev_BSP  =  dev_bsp;
    p_if->Dev_Cfg  =  dev_cfg;
    p_if->Ext_API  =  ext_api;
    p_if->Ext_Cfg  =  ext_cfg;
    p_if->En       =  DEF_DISABLED;
    p_if->Link     =  NET_IF_LINK_DOWN;
    p_if->LinkPrev =  NET_IF_LINK_DOWN;
    CPU_CRITICAL_ENTER();
    p_if->Init     =  DEF_YES;                                  /* See Note #5a1.                                       */
    CPU_CRITICAL_EXIT();


                                                                /* --------------- INIT SPECIFIC NET IF --------------- */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api->Add == (void (*)(NET_IF  *,
                                  NET_ERR *))0) {               /* If net IF add fnct NOT avail,                   ...  */
       *p_err =  NET_ERR_FAULT_NULL_FNCT;
        goto exit_fail;
    }
#endif

    NetIF_ObjInit(p_if, p_err);
    if (*p_err != NET_IF_ERR_NONE) {                            /* On any err(s);                                  ...  */
         goto exit_fail;
    }


    p_if_api->Add(p_if, p_err);                                 /* Init/add IF & dev.                                   */
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail_deinit;
    }

                                                                /* -------- INIT IP LAYER FOR ADDED INTERFACE --------- */
    NetIP_IF_Init(if_nbr, p_err);
    if (*p_err != NET_ERR_NONE) {
        goto exit_fail_deinit;
    }

   *p_err = NET_IF_ERR_NONE;

    goto exit_release;

 exit_fail_deinit:
     NetIF_ObjDel(p_if);

exit_fail:
    if (p_if != DEF_NULL) {
        CPU_CRITICAL_ENTER();                                   /* On any err(s);                                  ...   */
        p_if->Init = DEF_NO;                                    /* ... Clr net IF init           (see Note #5b1) & ...   */
        NetIF_NbrNext--;                                        /* ... dec next avail net IF nbr (see Note #5b2).        */
        CPU_CRITICAL_EXIT();
    }
exit_fail_init:
    if_nbr = NET_IF_NBR_NONE;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                            NetIF_Start()
*
* Description : (1) Start a network interface :
*
*                   (a) Acquire network lock
*                   (b) Start   network interface
*                   (c) Release network lock
*
*
* Argument(s) : if_nbr      Network interface number to start.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully started.
*                               NET_IF_ERR_INVALID_STATE        Network interface already started.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -------- RETURNED BY NetIF_Get() : --------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY 'p_if_api->Start()' : -----
*                                                               See specific network interface(s) 'Start()'
*                                                                   for additional return error codes.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIF_Start() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) NetIF_Start() blocked until network initialization completes.
*
*               (4) Each specific network interface 'Start()' is responsible for setting the initial link
*                   state after starting a network interface.
*
*                   (a) Specific network interface that do not require link state MUST set the network
*                       interface's link state to 'UP'.
*********************************************************************************************************
*/

void  NetIF_Start (NET_IF_NBR   if_nbr,
                   NET_ERR     *p_err)
{
    NET_IF          *p_if;
    NET_IF_API      *p_if_api;
#ifdef  NET_MLDP_MODULE_EN
    NET_IPv6_ADDR  addr_allnode_mcast;
#endif


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_Start, p_err);         /* See Note #2b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif


                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }

    if (p_if->En != DEF_DISABLED) {                              /* If net IF already started, ...                       */
       *p_err = NET_IF_ERR_INVALID_STATE;                        /* ... rtn err.                                         */
        goto exit_release;
    }


                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        goto exit_release;
    }
    if (p_if_api->Start == (void (*)(NET_IF  *,
                                    NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        goto exit_release;
    }
#endif


                                                                /* ------------------- START NET IF ------------------- */
    p_if_api->Start(p_if, p_err);                               /* Init/start IF & dev hw.                              */
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }

    p_if->En = DEF_ENABLED;                                     /* En IF AFTER IF/dev start.                            */

#ifdef   NET_MLDP_MODULE_EN
    if (p_if->Type != NET_IF_TYPE_LOOPBACK) {
        NetIPv6_AddrMcastAllNodesSet(&addr_allnode_mcast,       /* Create all-node mcast addr.                          */
                                      p_err);
        if (*p_err != NET_IPv6_ERR_NONE) {
            goto exit_stop;
        }

        NetMLDP_HostGrpJoinHandler(if_nbr,
                                  &addr_allnode_mcast,
                                  p_err);
        if (*p_err != NET_MLDP_ERR_NONE) {
             goto exit_stop;
        }
    }
#endif


#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_if->PerfMonState = NET_IF_PERF_MON_STATE_START;           /* Start perf mon.                                      */
#endif


   *p_err = NET_IF_ERR_NONE;
    goto exit_release;

#ifdef   NET_MLDP_MODULE_EN
exit_stop:
    p_if->En = DEF_DISABLED;                                    /* Dis IF AFTER IF/dev start.                           */
    p_if_api->Stop(p_if, p_err);
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                            NetIF_Stop()
*
* Description : (1) Stop a network interface :
*
*                   (a) Acquire network lock
*                   (b) Stop    network interface
*                   (c) Release network lock
*
*
* Argument(s) : if_nbr      Network interface number to stop.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully stopped.
*                               NET_IF_ERR_INVALID_STATE        Network interface already stopped.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               ------- RETURNED BY NetIF_Get() : --------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY 'p_if_api->Stop()' : -----
*                                                               See specific network interface(s) 'Stop()'
*                                                                   for additional return error codes.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : --
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIF_Stop() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) NetIF_Stop() blocked until network initialization completes.
*
*               (4) Each specific network interface 'Stop()' SHOULD be responsible for clearing the link
*                   state after stopping a network interface.  However, clearing the link state to 'DOWN'
*                   is included for completeness & as an extra precaution in case the specific network
*                   interface 'Stop()' fails to clear the link state.
*********************************************************************************************************
*/

void  NetIF_Stop (NET_IF_NBR   if_nbr,
                  NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_Stop, p_err);          /* See Note #2b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif


                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }

    if (p_if->En != DEF_ENABLED) {                              /* If net IF NOT started, ...                           */
       *p_err = NET_IF_ERR_INVALID_STATE;                       /* ... rtn err.                                         */
        goto exit_release;
    }


                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        goto exit_release;
    }
    if (p_if_api->Stop == (void (*)(NET_IF  *,
                                   NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        goto exit_release;
    }
#endif


                                                                /* ------------------- STOP NET IF -------------------- */
    p_if_api->Stop(p_if, p_err);                                /* Stop IF & dev hw.                                    */
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }

    p_if->En               = DEF_DISABLED;
    p_if->Link             = NET_IF_LINK_DOWN;                  /* See Note #4.                                         */
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_if->PerfMonState     = NET_IF_PERF_MON_STATE_STOP;
#endif


exit_release:
    Net_GlobalLockRelease();                                    /* ----------------- RELEASE NET LOCK ----------------- */
}


/*
*********************************************************************************************************
*                                            NetIF_Get()
*
* Description : Get a pointer to a network interface.
*
* Argument(s) : if_nbr      Network interface number to get.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully returned.
*
*                                                               - RETURNED BY NetIF_IsValidHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Pointer to corresponding network interface, if NO error(s).
*
*               Pointer to NULL,                            otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) (a) NetIF_Get() is called by network protocol suite function(s) & SHOULD be called
*                       with the global network lock already acquired.
*
*                   (b) (1) However, although acquiring the global network lock is typically required;
*                           interrupt service routines (ISRs) are (typically) prohibited from pending
*                           on OS objects & therefore can NOT acquire the global network lock.
*
*                       (2) Therefore, ALL network interface & network device driver functions that may
*                           be called by interrupt service routines MUST be able to be asynchronously
*                           accessed without acquiring the global network lock AND without corrupting
*                           any network data or task.
*
*                       See also 'NetIF_ISR_Handler()  Note #1b'.
*********************************************************************************************************
*/

NET_IF  *NetIF_Get (NET_IF_NBR   if_nbr,
                    NET_ERR     *p_err)
{
    NET_IF  *p_if;

                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
    if (if_nbr == NET_IF_NBR_NONE) {
       *p_err = NET_IF_ERR_INVALID_IF;
        return ((NET_IF *)0);
    }

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err !=  NET_IF_ERR_NONE) {
         return ((NET_IF *)0);
    }
#endif

    p_if  = &NetIF_Tbl[if_nbr];
   *p_err =  NET_IF_ERR_NONE;

    return (p_if);
}


/*
*********************************************************************************************************
*                                            NetIF_GetDflt()
*
* Description : Get the interface number of the default network interface; i.e. the first enabled network
*                   interface with at least one configured host address.
*
* Argument(s) : none.
*
* Return(s)   : Interface number of the first enabled network interface, if any.
*
*               NET_IF_NBR_NONE,                                         otherwise.
*
* Caller(s)   : NetSock_BindHandler(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT  be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_GetDflt() is called by network protocol suite function(s) & MUST be called with
*                   the global network lock already acquired.
*********************************************************************************************************
*/

NET_IF_NBR  NetIF_GetDflt (void)
{
    NET_IF       *p_if;
    NET_IF_NBR    if_nbr;
    NET_IF_NBR    if_nbr_ix;
    NET_IF_NBR    if_nbr_next;
    CPU_BOOLEAN   init;
    CPU_SR_ALLOC();


    if_nbr      = NET_IF_NBR_NONE;
    if_nbr_ix   = NET_IF_NBR_BASE_CFGD;
    CPU_CRITICAL_ENTER();
    if_nbr_next = NetIF_NbrNext;
    CPU_CRITICAL_EXIT();

    p_if = &NetIF_Tbl[if_nbr_ix];

    while ((if_nbr_ix <  if_nbr_next) &&                        /* Srch ALL cfg'd IF's            ...                   */
           (if_nbr    == NET_IF_NBR_NONE)) {

        CPU_CRITICAL_ENTER();
        init = p_if->Init;
        CPU_CRITICAL_EXIT();
        if ((init    == DEF_YES) &&                             /* ... for first init'd & en'd IF ...                   */
            (p_if->En == DEF_ENABLED)) {
            if_nbr = p_if->Nbr;
        }

        if (if_nbr == NET_IF_NBR_NONE) {                        /* If NO cfg'd host addr(s) found, ...                  */
            p_if++;                                             /* ... adv to next IF.                                  */
            if_nbr_ix++;
        }
    }

#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    if (if_nbr == NET_IF_NBR_NONE) {                            /* If NO init'd, en'd, & cfg'd IF found; ...            */
        if_nbr  = NET_IF_NBR_LOOPBACK;                          /* ... rtn loopback IF, if en'd/avail.                  */
    }
#endif

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                      NetIF_GetRxDataAlignPtr()
*
* Description : (1) Get aligned pointer into a receive application data buffer :
*
*                   (a) Get    pointer to aligned receive application data buffer address       See Note #3
*                   (b) Return pointer to aligned receive application data buffer address
*                         OR
*                       Null pointer & error code, on failure
*
*
* Argument(s) : if_nbr      Network interface number to get a receive application buffer's aligned data
*                               pointer.
*
*               p_data      Pointer to receive application data buffer to get an aligned pointer into
*                               (see also Note #3b).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                                                               -- RETURNED BY NetIF_GetDataAlignPtr() : --
*                               NET_IF_ERR_NONE                 Aligned pointer into receive application
*                                                                   data buffer successfully returned.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_data' passed a NULL pointer.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_ALIGN_NOT_AVAIL      Alignment between application data buffer &
*                                                                   network interface's network buffer data
*                                                                   area(s) NOT possible (see Note #3a1B).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Pointer to aligned receive application data buffer address, if NO error(s).
*
*               Pointer to NULL,                                            otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIF_GetRxDataAlignPtr() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'NetIF_GetDataAlignPtr()  Note #1a').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) (a) (1) (A) Optimal alignment between application data buffer(s) & network interface's
*                               network buffer data area(s) is NOT guaranteed & is possible if & only if
*                               all of the following conditions are true :
*
*                               (1) Network interface's network buffer data area(s) MUST be aligned to a
*                                   multiple of the CPU's data word size (see 'net_buf.h  NETWORK BUFFER
*                                   INDEX & SIZE DEFINES  Note #2b2').
*
*                           (B) Otherwise, a single, fixed alignment between application data buffer(s) &
*                               network interface's buffer data area(s) is NOT possible.
*
*                       (2) (A) Even when application data buffers & network buffer data areas are aligned
*                               in the best case; optimal alignment is NOT guaranteed for every read/write
*                               of data to/from application data buffers & network buffer data areas.
*
*                               For any single read/write of data to/from application data buffers & network
*                               buffer data areas, optimal alignment occurs if & only if all of the following
*                               conditions are true :
*
*                               (1) Data read/written to/from application data buffer(s) to network buffer
*                                   data area(s) MUST start on addresses with the same relative offset from
*                                   CPU word-aligned addresses.
*
*                                   In other words, the modulus of the specific read/write address in the
*                                   application data buffer with the CPU's data word size MUST be equal to
*                                   the modulus of the specific read/write address in the network buffer
*                                   data area with the CPU's data word size.
*
*                                   This condition MIGHT NOT be satisfied whenever :
*
*                                   (a) Data is read/written to/from fragmented packets
*                                   (b) Data is NOT maximally read/written to/from stream-type packets
*                                           (e.g. TCP data segments)
*                                   (c) Packets include variable number of header options (e.g. IP options)
*
*                           (B) However, even though optimal alignment between application data buffers &
*                               network buffer data areas is NOT guaranteed for every read/write; optimal
*                               alignment SHOULD occur more frequently leading to improved network data
*                               throughput.
*
*                   (b) Since the first aligned address in the application data buffer may be 0 to
*                       (CPU_CFG_DATA_SIZE - 1) octets after the application data buffer's starting
*                       address, the application data buffer SHOULD allocate & reserve an additional
*                       (CPU_CFG_DATA_SIZE - 1) number of octets.
*
*                       However, the application data buffer's effective, useable size is still limited
*                       to its original declared size (before reserving additional octets) & SHOULD NOT
*                       be increased by the additional, reserved octets.
*
*                   See also 'NetIF_GetDataAlignPtr()  Note #3'.
*********************************************************************************************************
*/

void  *NetIF_GetRxDataAlignPtr (NET_IF_NBR   if_nbr,
                                void        *p_data,
                                NET_ERR     *p_err)
{
    void  *p_data_align;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((void *)0);
    }
#endif


    p_data_align = NetIF_GetDataAlignPtr(if_nbr, NET_TRANSACTION_RX, p_data, p_err);


    return (p_data_align);
}


/*
*********************************************************************************************************
*                                       NetIF_GetTxDataAlignPtr()
*
* Description : (1) Get aligned pointer into a transmit application data buffer :
*
*                   (a) Get    pointer to aligned transmit application data buffer address      See Note #3
*                   (b) Return pointer to aligned transmit application data buffer address
*                         OR
*                       Null pointer & error code, on failure
*
*
* Argument(s) : if_nbr      Network interface number to get a transmit application buffer's aligned data
*                               pointer.
*
*               p_data      Pointer to transmit application data buffer to get an aligned pointer into
*                               (see also Note #3b).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                               -- RETURNED BY NetIF_GetDataAlignPtr() : --
*                               NET_IF_ERR_NONE                 Aligned pointer into transmit application
*                                                                   data buffer successfully returned.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_data' passed a NULL pointer.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_ALIGN_NOT_AVAIL      Alignment between application data buffer &
*                                                                   network interface's network buffer data
*                                                                   area(s) NOT possible (see Note #3a1B).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Pointer to aligned transmit application data buffer address, if NO error(s).
*
*               Pointer to NULL,                                             otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIF_GetTxDataAlignPtr() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'NetIF_GetDataAlignPtr()  Note #1a').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) (a) (1) (A) Optimal alignment between application data buffer(s) & network interface's
*                               network buffer data area(s) is NOT guaranteed & is possible if & only if
*                               all of the following conditions are true :
*
*                               (1) Network interface's network buffer data area(s) MUST be aligned to a
*                                   multiple of the CPU's data word size (see 'net_buf.h  NETWORK BUFFER
*                                   INDEX & SIZE DEFINES  Note #2b2').
*
*                           (B) Otherwise, a single, fixed alignment between application data buffer(s) &
*                               network interface's buffer data area(s) is NOT possible.
*
*                       (2) (A) Even when application data buffers & network buffer data areas are aligned
*                               in the best case; optimal alignment is NOT guaranteed for every read/write
*                               of data to/from application data buffers & network buffer data areas.
*
*                               For any single read/write of data to/from application data buffers & network
*                               buffer data areas, optimal alignment occurs if & only if all of the following
*                               conditions are true :
*
*                               (1) Data read/written to/from application data buffer(s) to network buffer
*                                   data area(s) MUST start on addresses with the same relative offset from
*                                   CPU word-aligned addresses.
*
*                                   In other words, the modulus of the specific read/write address in the
*                                   application data buffer with the CPU's data word size MUST be equal to
*                                   the modulus of the specific read/write address in the network buffer
*                                   data area with the CPU's data word size.
*
*                                   This condition MIGHT NOT be satisfied whenever :
*
*                                   (a) Data is read/written to/from fragmented packets
*                                   (b) Data is NOT maximally read/written to/from stream-type packets
*                                           (e.g. TCP data segments)
*                                   (c) Packets include variable number of header options (e.g. IP options)
*
*                           (B) However, even though optimal alignment between application data buffers &
*                               network buffer data areas is NOT guaranteed for every read/write; optimal
*                               alignment SHOULD occur more frequently leading to improved network data
*                               throughput.
*
*                   (b) Since the first aligned address in the application data buffer may be 0 to
*                       (CPU_CFG_DATA_SIZE - 1) octets after the application data buffer's starting
*                       address, the application data buffer SHOULD allocate & reserve an additional
*                       (CPU_CFG_DATA_SIZE - 1) number of octets.
*
*                       However, the application data buffer's effective, useable size is still limited
*                       to its original declared size (before reserving additional octets) & SHOULD NOT
*                       be increased by the additional, reserved octets.
*
*                   See also 'NetIF_GetDataAlignPtr()  Note #3'.
*********************************************************************************************************
*/

void  *NetIF_GetTxDataAlignPtr (NET_IF_NBR   if_nbr,
                                void        *p_data,
                                NET_ERR     *p_err)
{
    void  *p_data_align;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((void *)0);
    }
#endif
    *p_err = NET_IF_ERR_NONE;

    p_data_align = NetIF_GetDataAlignPtr(if_nbr, NET_TRANSACTION_TX, p_data, p_err);

    return (p_data_align);
}


/*
*********************************************************************************************************
*                                        NetIF_GetExtAvailCtr()
*
* Description : Return number of external interface configured.
*
* Argument(s) : p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Number of external interface available
*
* Caller(s)   : NetIF_GetExtAvailCtr().
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetIF_GetExtAvailCtr (NET_ERR  *p_err)
{
    CPU_INT08U  nbr  = 0u;
    CPU_INT08U  init = NET_IF_NBR_BASE;



#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    init = NET_IF_NBR_LOOPBACK;
#endif

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_GetExtAvailCtr, p_err);       /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
        return (DEF_NO);
    }

    nbr = NetIF_NbrNext - init;

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (nbr);
}


/*
*********************************************************************************************************
*                                          NetIF_GetBaseNbr()
*
* Description : Get the interface base number (first interface ID).
*
* Argument(s) : none.
*
* Return(s)   : Interface base number.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_IF_NBR  NetIF_GetNbrBaseCfgd (void)
{
    return (NET_IF_NBR_BASE_CFGD);
}


/*
*********************************************************************************************************
*                                            NetIF_IsValid()
*
* Description : Validate network interface number.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIF_IsValidHandler() : --
*                               NET_IF_ERR_NONE                 Network interface successfully validated.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               - RETURNED BY Net_GlobalLockAcquire() : -
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, network interface number   valid.
*
*               DEF_NO,  network interface number invalid / NOT yet configured.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsValid() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_IsValidHandler()  Note #1'.
*
*               (2) NetIF_IsValid() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsValid (NET_IF_NBR   if_nbr,
                            NET_ERR     *p_err)
{
    CPU_BOOLEAN  valid;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_IsValid, p_err);       /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (DEF_NO);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidHandler(if_nbr, p_err);
    goto exit_release;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    valid = DEF_NO;
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (valid);
}


/*
*********************************************************************************************************
*                                        NetIF_IsValidHandler()
*
* Description : Validate network interface number.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully validated.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : DEF_YES, network interface number   valid.
*
*               DEF_NO,  network interface number invalid / NOT yet configured.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1a].
*
* Note(s)     : (1) (a) NetIF_IsValidHandler() is called by network protocol suite function(s) & SHOULD
*                       be called with the global network lock already acquired.
*
*                       See also 'NetIF_IsValid()  Note #1'.
*
*                   (b) (1) However, although acquiring the global network lock is typically required;
*                           interrupt service routines (ISRs) are (typically) prohibited from pending
*                           on OS objects & therefore can NOT acquire the global network lock.
*
*                       (2) Therefore, ALL network interface & network device driver functions that may
*                           be called by interrupt service routines MUST be able to be asynchronously
*                           accessed without acquiring the global network lock AND without corrupting
*                           any network data or task.
*
*                       (3) Thus the following variables MUST ALWAYS be accessed exclusively in critical
*                           sections :
*
*                           (A) 'NetIF_NbrNext'
*                           (B)  Network interfaces 'Init'
*
*                       See also 'NetIF_ISR_Handler()  Note #1b'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsValidHandler (NET_IF_NBR   if_nbr,
                                   NET_ERR     *p_err)
{
    NET_IF       *p_if;
    NET_IF_NBR    if_nbr_next;
    CPU_BOOLEAN   init;
    CPU_SR_ALLOC();

                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
    CPU_CRITICAL_ENTER();
    if_nbr_next = NetIF_NbrNext;
    CPU_CRITICAL_EXIT();
    if (if_nbr >= if_nbr_next) {
       *p_err =  NET_IF_ERR_INVALID_IF;
        return (DEF_NO);
    }

                                                                /* ----------------- VALIDATE NET IF ------------------ */
    p_if  = &NetIF_Tbl[if_nbr];
    CPU_CRITICAL_ENTER();
    init =  p_if->Init;
    CPU_CRITICAL_EXIT();
    if (init != DEF_YES) {
       *p_err  = NET_IF_ERR_INVALID_IF;
        return (DEF_NO);
    }


   *p_err =  NET_IF_ERR_NONE;

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                         NetIF_IsValidCfgd()
*
* Description : Validate configured network interface number.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_NONE                 Network interface successfully validated.
*                               NET_IF_ERR_INVALID_IF           Invalid configured network interface number.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, network interface number   valid.
*
*               DEF_NO,  network interface number invalid / NOT yet configured or reserved.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsValidCfgd() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_IsValidCfgdHandler()  Note #1'.
*
*               (2) NetIF_IsValidCfgd() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsValidCfgd (NET_IF_NBR   if_nbr,
                                NET_ERR     *p_err)
{
    CPU_BOOLEAN  valid;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_IsValidCfgd, p_err);   /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (DEF_NO);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, p_err);
    goto exit_release;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    valid = DEF_NO;
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (valid);
}


/*
*********************************************************************************************************
*                                      NetIF_IsValidCfgdHandler()
*
* Description : Validate configured network interface number.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully validated.
*                               NET_IF_ERR_INVALID_IF           Invalid configured network interface number.
*
* Return(s)   : DEF_YES, network interface number   valid.
*
*               DEF_NO,  network interface number invalid / NOT yet configured or reserved.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsValidCfgdHandler() is called by network protocol suite function(s) & MUST be
*                   called with the global network lock already acquired.
*
*                   See also 'NetIF_IsValidCfgd()  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsValidCfgdHandler (NET_IF_NBR   if_nbr,
                                       NET_ERR     *p_err)
{
    NET_IF       *p_if;
    NET_IF_NBR    if_nbr_next;
    CPU_BOOLEAN   init;
    CPU_SR_ALLOC();

                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
    CPU_CRITICAL_ENTER();
    if_nbr_next = NetIF_NbrNext;
    CPU_CRITICAL_EXIT();
    if ((if_nbr <  NET_IF_NBR_BASE_CFGD) ||
        (if_nbr >= if_nbr_next)) {
       *p_err =  NET_IF_ERR_INVALID_IF;
        return (DEF_NO);
    }

                                                                /* ----------------- VALIDATE NET IF ------------------ */
    p_if  = &NetIF_Tbl[if_nbr];
    CPU_CRITICAL_ENTER();
    init =  p_if->Init;
    CPU_CRITICAL_EXIT();
    if (init != DEF_YES) {
       *p_err =  NET_IF_ERR_INVALID_IF;
        return (DEF_NO);
    }


   *p_err =  NET_IF_ERR_NONE;

    return (DEF_YES);
}



/*
*********************************************************************************************************
*                                            NetIF_IsEn()
*
* Description : Validate network interface as enabled.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               ---- RETURNED BY NetIF_IsEnHandler() : -----
*                               NET_IF_ERR_NONE                 Network interface successfully validated
*                                                                   as enabled.
*                               NET_IF_ERR_INVALID_IF           Invalid OR disabled network interface.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, network interface   valid  &  enabled.
*
*               DEF_NO,  network interface invalid or disabled.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsEn() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_IsEnHandler()  Note #1'.
*
*               (2) NetIF_IsEn() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsEn (NET_IF_NBR   if_nbr,
                         NET_ERR     *p_err)
{
    CPU_BOOLEAN  en;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_IsEn, p_err);          /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (DEF_NO);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* --------------- VALIDATE NET IF EN'D --------------- */
    en = NetIF_IsEnHandler(if_nbr, p_err);
    goto exit_release;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
   en = DEF_NO;
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (en);
}


/*
*********************************************************************************************************
*                                         NetIF_IsEnHandler()
*
* Description : Validate network interface as enabled.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully validated
*                                                                   as enabled.
*                               NET_IF_ERR_INVALID_IF           Invalid OR disabled network interface.
*
* Return(s)   : DEF_YES, network interface   valid  &  enabled.
*
*               DEF_NO,  network interface invalid or disabled.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsEnHandler() is called by network protocol suite function(s) & MUST be called
*                   with the global network lock already acquired.
*
*                   See also 'NetIF_IsEn()  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsEnHandler (NET_IF_NBR   if_nbr,
                                NET_ERR     *p_err)
{
    NET_IF       *p_if;
    CPU_BOOLEAN   en;

                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (DEF_NO);
    }

                                                                /* --------------- VALIDATE NET IF EN'D --------------- */
    p_if = &NetIF_Tbl[if_nbr];
    if (p_if->En == DEF_ENABLED) {
        en   = DEF_YES;
       *p_err = NET_IF_ERR_NONE;
    } else {
        en   = DEF_NO;
       *p_err = NET_IF_ERR_INVALID_IF;
    }

    return (en);
}


/*
*********************************************************************************************************
*                                           NetIF_IsEnCfgd()
*
* Description : Validate configured network interface as enabled.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY NetIF_IsEnCfgdHandler() : ---
*                               NET_IF_ERR_NONE                 Network interface successfully validated
*                                                                   as enabled.
*                               NET_IF_ERR_INVALID_IF           Invalid OR disabled network interface.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, network interface   valid  &  enabled.
*
*               DEF_NO,  network interface invalid or disabled.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsEnCfgd() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_IsEnCfgdHandler()  Note #1'.
*
*               (2) NetIF_IsEnCfgd() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsEnCfgd (NET_IF_NBR   if_nbr,
                             NET_ERR     *p_err)
{
    CPU_BOOLEAN  en;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_IsEnCfgd, p_err);      /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (DEF_NO);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* --------------- VALIDATE NET IF EN'D --------------- */
    en = NetIF_IsEnCfgdHandler(if_nbr, p_err);
    goto exit_release;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
   en = DEF_NO;
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (en);
}


/*
*********************************************************************************************************
*                                        NetIF_IsEnCfgdHandler()
*
* Description : Validate configured network interface as enabled.
*
* Argument(s) : if_nbr      Network interface number to validate.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface successfully validated
*                                                                   as enabled.
*                               NET_IF_ERR_INVALID_IF           Invalid OR disabled network interface.
*
* Return(s)   : DEF_YES, network interface   valid  &  enabled.
*
*               DEF_NO,  network interface invalid or disabled.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_IsEnCfgdHandler() is called by network protocol suite function(s) & MUST be
*                   called with the global network lock already acquired.
*
*                   See also 'NetIF_IsEnCfgd()  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_IsEnCfgdHandler (NET_IF_NBR   if_nbr,
                                    NET_ERR     *p_err)
{
    NET_IF       *p_if;
    CPU_BOOLEAN   en;

                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (DEF_NO);
    }

                                                                /* --------------- VALIDATE NET IF EN'D --------------- */
    p_if = &NetIF_Tbl[if_nbr];
    if (p_if->En == DEF_ENABLED) {
        en    = DEF_YES;
       *p_err = NET_IF_ERR_NONE;
    } else {
        en    = DEF_NO;
       *p_err = NET_IF_ERR_INVALID_IF;
    }

    return (en);
}


/*
*********************************************************************************************************
*                                         NetIF_AddrHW_Get()
*
* Description : Get network interface's hardware address.
*
* Argument(s) : if_nbr      Network interface number to get hardware address.
*
*               p_addr_hw   Pointer to variable that will receive the hardware address (see Note #3).
*
*               p_addr_len  Pointer to a variable to ... :
*
*                               (a) Pass the length of the address buffer pointed to by 'p_addr'.
*                               (b) (1) Return the actual size of the protocol address, if NO error(s);
*                                   (2) Return 0,                                       otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               --- RETURNED BY NetIF_AddrHW_GetHandler() : ----
*                               NET_IF_ERR_NONE                 Network interface's hardware address
*                                                                   successfully returned.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_hw'/'p_addr_len' passed a
*                                                                   NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               See specific network interface(s) 'AddrHW_Get()'
*                                                                   for additional return error codes.
*
*                                                               ---- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_AddrHW_Get() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_AddrHW_GetHandler()  Note #1'.
*
*               (2) NetIF_AddrHW_Get() blocked until network initialization completes.
*
*               (3) The hardware address is returned in network-order; i.e. the pointer to the hardware
*                   address points to the highest-order octet.
*********************************************************************************************************
*/

void  NetIF_AddrHW_Get (NET_IF_NBR   if_nbr,
                        CPU_INT08U  *p_addr_hw,
                        CPU_INT08U  *p_addr_len,
                        NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_AddrHW_Get, p_err);    /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif

                                                                /* ---------------- GET NET IF HW ADDR ---------------- */
    NetIF_AddrHW_GetHandler(if_nbr, p_addr_hw, p_addr_len, p_err);

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                       NetIF_AddrHW_GetHandler()
*
* Description : Get network interface's hardware address.
*
* Argument(s) : if_nbr      Network interface number to get hardware address.
*
*               p_addr_hw   Pointer to variable that will receive the hardware address (see Note #2).
*
*               p_addr_len  Pointer to a variable to ... :
*
*                               (a) Pass the length of the address buffer pointed to by 'p_addr_hw'.
*                               (b) (1) Return the actual size of the protocol address, if NO error(s);
*                                   (2) Return 0,                                       otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's hardware address
*                                                                   successfully returned.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_hw'/'p_addr_len' passed a
*                                                                   NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*
*                                                               ---------- RETURNED BY NetIF_Get() : -----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY 'p_if_api->AddrHW_Get()' : -----
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               See specific network interface(s) 'AddrHW_Get()'
*                                                                   for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrHW_Get().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_AddrHW_GetHandler() is called by network protocol suite function(s) & MUST be
*                   called with the global network lock already acquired.
*
*                   See also 'NetIF_AddrHW_Get()  Note #1'.
*
*               (2) The hardware address is returned in network-order; i.e. the pointer to the hardware
*                   address points to the highest-order octet.
*
*               (3) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

void  NetIF_AddrHW_GetHandler (NET_IF_NBR   if_nbr,
                               CPU_INT08U  *p_addr_hw,
                               CPU_INT08U  *p_addr_len,
                               NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;
    CPU_INT08U   addr_len;

                                                                /* ---------------- VALIDATE ADDR PTRS ---------------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_addr_len == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    addr_len   = *p_addr_len;
   *p_addr_len =  0u;                                           /* Init len for err (see Note #3).                      */

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_addr_hw == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->AddrHW_Get == (void (*)(NET_IF     *,
                                         CPU_INT08U *,
                                         CPU_INT08U *,
                                         NET_ERR    *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* ---------------- GET NET IF HW ADDR ---------------- */
    p_if_api->AddrHW_Get(p_if, p_addr_hw, &addr_len, p_err);
   *p_addr_len = addr_len;
}


/*
*********************************************************************************************************
*                                          NetIF_AddrHW_Set()
*
* Description : Set network interface's hardware address.
*
* Argument(s) : if_nbr      Network interface number to set hardware address.
*
*               p_addr_hw   Pointer to hardware address (see Note #3).
*
*               addr_len    Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               --- RETURNED BY NetIF_AddrHW_SetHandler() : ----
*                               NET_IF_ERR_NONE                 Network interface's hardware address
*                                                                   successfully set.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_hw' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state (see Note #4).
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               See specific network interface(s) 'AddrHW_Set()'
*                                                                   for additional return error codes.
*
*                                                               ---- RETURNED BY Net_GlobalLockAcquire() : -----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_AddrHW_Set() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_AddrHW_SetHandler()  Note #1'.
*
*               (2) NetIF_AddrHW_Set() blocked until network initialization completes.
*
*               (3) The hardware address MUST be in network-order; i.e. the pointer to the hardware
*                   address MUST point to the highest-order octet.
*
*               (4) The interface MUST be stopped BEFORE setting a new hardware address, which does
*                   NOT take effect until the interface is re-started.
*********************************************************************************************************
*/

void  NetIF_AddrHW_Set (NET_IF_NBR   if_nbr,
                        CPU_INT08U  *p_addr_hw,
                        CPU_INT08U   addr_len,
                        NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_AddrHW_Set, p_err);    /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif

                                                                /* ---------------- SET NET IF HW ADDR ---------------- */
    NetIF_AddrHW_SetHandler(if_nbr, p_addr_hw, addr_len, p_err);

    goto exit_release;
exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                       NetIF_AddrHW_SetHandler()
*
* Description : Set network interface's hardware address.
*
* Argument(s) : if_nbr      Network interface number to set hardware address.
*
*               p_addr_hw   Pointer to hardware address (see Note #2).
*
*               addr_len    Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's hardware address
*                                                                   successfully set.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_hw' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state (see Note #3).
*
*                                                               ---------- RETURNED BY NetIF_Get() : -----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY 'p_if_api->AddrHW_Set()' : -----
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               See specific network interface(s) 'AddrHW_Set()'
*                                                                   for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrHW_Set().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_AddrHW_SetHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetIF_AddrHW_Set()  Note #1'.
*
*               (2) The hardware address MUST be in network-order; i.e. the pointer to the hardware
*                   address MUST point to the highest-order octet.
*
*               (3) The interface MUST be stopped BEFORE setting a new hardware address, which does
*                   NOT take effect until the interface is re-started.
*********************************************************************************************************
*/

void  NetIF_AddrHW_SetHandler (NET_IF_NBR   if_nbr,
                               CPU_INT08U  *p_addr_hw,
                               CPU_INT08U   addr_len,
                               NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_addr_hw == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

    if (p_if->En != DEF_DISABLED) {                              /* If net IF NOT dis'd (see Note #3), ...               */
       *p_err = NET_IF_ERR_INVALID_STATE;                        /* ... rtn err.                                         */
        return;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->AddrHW_Set == (void (*)(NET_IF     *,
                                         CPU_INT08U *,
                                         CPU_INT08U  ,
                                         NET_ERR    *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* ---------------- SET NET IF HW ADDR ---------------- */
    p_if_api->AddrHW_Set(p_if, p_addr_hw, addr_len, p_err);
}


/*
*********************************************************************************************************
*                                        NetIF_AddrHW_IsValid()
*
* Description : Validate network interface's hardware address.
*
* Argument(s) : if_nbr      Interface number to validate the hardware address.
*
*               p_addr_hw   Pointer to an interface hardware address (see Note #3).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIF_AddrHW_IsValidHandler() : -
*                               NET_IF_ERR_NONE                 Network interface's hardware address
*                                                                   successfully validated.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_hw' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, if hardware address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_AddrHW_IsValid() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_AddrHW_IsValidHandler()  Note #1'.
*
*               (2) NetIF_AddrHW_IsValid() blocked until network initialization completes.
*
*               (3) The hardware address MUST be in network-order; i.e. the pointer to the hardware
*                   address MUST point to the highest-order octet.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_AddrHW_IsValid (NET_IF_NBR   if_nbr,
                                   CPU_INT08U  *p_addr_hw,
                                   NET_ERR     *p_err)
{
    CPU_BOOLEAN  valid;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetIF_AddrHW_IsValid, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (DEF_NO);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* ------------- VALIDATE NET IF HW ADDR -------------- */
    valid = NetIF_AddrHW_IsValidHandler(if_nbr, p_addr_hw, p_err);
    goto exit_release;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    valid = DEF_NO;
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (valid);
}


/*
*********************************************************************************************************
*                                     NetIF_AddrHW_IsValidHandler()
*
* Description : Validate network interface's hardware address.
*
* Argument(s) : if_nbr      Interface number to validate the hardware address.
*
*               p_addr_hw   Pointer to an interface hardware address (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's hardware address
*                                                                   successfully validated.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_hw' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*
*                                                               ------- RETURNED BY NetIF_Get() : --------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : DEF_YES, if hardware address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIF_AddrHW_IsValid().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_AddrHW_IsValidHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIF_AddrHW_IsValid()  Note #1'.
*
*               (2) The hardware address MUST be in network-order; i.e. the pointer to the hardware
*                   address MUST point to the highest-order octet.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_AddrHW_IsValidHandler (NET_IF_NBR   if_nbr,
                                          CPU_INT08U  *p_addr_hw,
                                          NET_ERR     *p_err)
{
    NET_IF       *p_if;
    NET_IF_API   *p_if_api;
    CPU_BOOLEAN   valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ADDR PTR ----------------- */
    if (p_addr_hw == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (DEF_NO);
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (DEF_NO);
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err =  NET_IF_ERR_INVALID_CFG;
        return (DEF_NO);
    }
    if (p_if_api->AddrHW_IsValid == (CPU_BOOLEAN (*)(NET_IF     *,
                                                    CPU_INT08U *))0) {
       *p_err =  NET_ERR_FAULT_NULL_FNCT;
        return (DEF_NO);
    }
#endif

                                                                /* ------------- VALIDATE NET IF HW ADDR -------------- */
    valid = p_if_api->AddrHW_IsValid(p_if, p_addr_hw);
   *p_err = NET_IF_ERR_NONE;

    return (valid);
}


/*
*********************************************************************************************************
*                                       NetIF_AddrMulticastAdd()
*
* Description : Add a multicast address to a network interface.
*
* Argument(s) : if_nbr                  Interface number to add a multicast address.
*
*               p_addr_protocol         Pointer to a multicast protocol address to add (see Note #1).
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Multicast address successfully added.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_protocol' passed a NULL pointer.
*
*                                                               ---------- RETURNED BY NetIF_Get() : -----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               - RETURNED BY 'p_if_api->AddrMulticastAdd()' : --
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware/protocol address length.
*                               NET_IF_ERR_INVALID_PROTOCOL     Invalid network  protocol.
*
*                                                               See specific network interface(s)
*                                                                   'AddrMulticastAdd()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpAdd().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The multicast protocol address MUST be in network-order.
*********************************************************************************************************
*/
#ifdef  NET_MCAST_MODULE_EN
void  NetIF_AddrMulticastAdd (NET_IF_NBR          if_nbr,
                              CPU_INT08U         *p_addr_protocol,
                              CPU_INT08U          addr_protocol_len,
                              NET_PROTOCOL_TYPE   addr_protocol_type,
                              NET_ERR            *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTRS ------------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        goto exit;
    }
    if (p_if_api->AddrMulticastAdd == (void (*)(NET_IF          *,
                                               CPU_INT08U      *,
                                               CPU_INT08U       ,
                                               NET_PROTOCOL_TYPE,
                                               NET_ERR         *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        goto exit;
    }
#endif

                                                                /* ----------- ADD MULTICAST ADDR TO NET IF ----------- */
    p_if_api->AddrMulticastAdd(p_if,
                              p_addr_protocol,
                              addr_protocol_len,
                              addr_protocol_type,
                              p_err);
    switch (*p_err) {
        case NET_DEV_ERR_NONE:
        case NET_IF_ERR_NONE:
             break;

        case NET_ERR_FAULT_NULL_FNCT:
        case NET_ERR_FAULT_FEATURE_DIS:
        case NET_IF_ERR_INVALID_CFG:
        case NET_IF_ERR_INVALID_ADDR_LEN:
        case NET_IF_ERR_INVALID_PROTOCOL:
             goto exit;

        case NET_DEV_ERR_INIT:
        case NET_DEV_ERR_FAULT:
        case NET_DEV_ERR_NULL_PTR:
        case NET_DEV_ERR_MEM_ALLOC:
        case NET_DEV_ERR_DEV_OFF:
            *p_err = NET_IF_ERR_DEV_FAULT;
             goto exit;

        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             goto exit;
    }

    *p_err = NET_IF_ERR_NONE;

exit:
    return;
}
#endif


/*
*********************************************************************************************************
*                                      NetIF_AddrMulticastRemove()
*
* Description : Remove a multicast address from a network interface.
*
* Argument(s) : if_nbr                  Interface number to remove a multicast address.
*
*               p_addr_protocol         Pointer to a multicast protocol address to remove (see Note #1).
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Multicast address successfully removed.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_addr_protocol' passed a NULL pointer.
*
*                                                               ----------- RETURNED BY NetIF_Get() : ------------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               - RETURNED BY 'p_if_api->AddrMulticastRemove()' : -
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware/protocol address length.
*                               NET_IF_ERR_INVALID_PROTOCOL     Invalid network  protocol.
*
*                                                               See specific network interface(s)
*                                                                   'AddrMulticastRemove()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpRemove().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The multicast protocol address MUST be in network-order.
*********************************************************************************************************
*/

#ifdef  NET_MCAST_MODULE_EN
void  NetIF_AddrMulticastRemove (NET_IF_NBR          if_nbr,
                                 CPU_INT08U         *p_addr_protocol,
                                 CPU_INT08U          addr_protocol_len,
                                 NET_PROTOCOL_TYPE   addr_protocol_type,
                                 NET_ERR            *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTRS ------------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->AddrMulticastRemove == (void (*)(NET_IF          *,
                                                  CPU_INT08U      *,
                                                  CPU_INT08U       ,
                                                  NET_PROTOCOL_TYPE,
                                                  NET_ERR         *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* -------- REMOVE MULTICAST ADDR FROM NET IF --------- */
    p_if_api->AddrMulticastRemove(p_if,
                                 p_addr_protocol,
                                 addr_protocol_len,
                                 addr_protocol_type,
                                 p_err);
}
#endif


/*
*********************************************************************************************************
*                                   NetIF_AddrMulticastProtocolToHW()
*
* Description : Convert a multicast protocol address into a hardware address.
*
* Argument(s) : if_nbr                  Interface number to convert address.
*
*               p_addr_protocol         Pointer to a multicast protocol address to convert (see Note #1a).
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_addr_hw               Pointer to a variable that will receive the hardware address
*                                           (see Note #1b).
*
*               p_addr_hw_len            Pointer to a variable to ... :
*
*                                           (a) Pass the length of the hardware address, in octets.
*                                           (b) (1) Return the actual length of the hardware address,
*                                                       in octets, if NO error(s);
*                                               (2) Return 0, otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Protocol address successfully converted.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               -------------- RETURNED BY NetIF_Get() : ---------------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               - RETURNED BY 'p_if_api->AddrMulticastProtocolToHW()' : -
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware/protocol address length.
*                               NET_IF_ERR_INVALID_PROTOCOL     Invalid network  protocol.
*
*                                                               See specific network interface(s)
*                                                                   'AddrMulticastProtocolToHW()' for
*                                                                   additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_CacheHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) The multicast protocol address MUST be in network-order.
*
*                   (b) The hardware address is returned in network-order; i.e. the pointer to the
*                       hardware address points to the highest-order octet.
*
*               (2) Since 'p_addr_hw_len' argument is both an input & output argument (see 'Argument(s) :
*                   p_addr_hw_len'), ... :
*
*                   (a) Its input value SHOULD be validated prior to use; ...
*                       (1) In the case that the 'p_addr_len' argument is passed a null pointer,
*                           NO input value is validated or used.
*
*                   (b) While its output value MUST be initially configured to return a default value
*                       PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/
#ifdef  NET_MCAST_TX_MODULE_EN
void  NetIF_AddrMulticastProtocolToHW (NET_IF_NBR          if_nbr,
                                       CPU_INT08U         *p_addr_protocol,
                                       CPU_INT08U          addr_protocol_len,
                                       NET_PROTOCOL_TYPE   addr_protocol_type,
                                       CPU_INT08U         *p_addr_hw,
                                       CPU_INT08U         *p_addr_hw_len,
                                       NET_ERR            *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;
    CPU_INT08U   addr_hw_len;

                                                                /* ------------------ VALIDATE ADDRS ------------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_hw_len == (CPU_INT08U *)0) {                      /* See Note #2a1.                                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

     addr_hw_len = *p_addr_hw_len;
#endif
   *p_addr_hw_len =  0u;                                         /* Cfg dflt addr len for err (see Note #2b).            */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_addr_hw == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->AddrMulticastProtocolToHW == (void (*)(NET_IF          *,
                                                        CPU_INT08U      *,
                                                        CPU_INT08U       ,
                                                        NET_PROTOCOL_TYPE,
                                                        CPU_INT08U      *,
                                                        CPU_INT08U      *,
                                                        NET_ERR         *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* -------------- CONVERT MULTICAST ADDR -------------- */
    p_if_api->AddrMulticastProtocolToHW(p_if,
                                       p_addr_protocol,
                                       addr_protocol_len,
                                       addr_protocol_type,
                                       p_addr_hw,
                                      &addr_hw_len,
                                       p_err);

   *p_addr_hw_len = addr_hw_len;
}
#endif


/*
*********************************************************************************************************
*                                            NetIF_MTU_Get()
*
* Description : Get network interface's MTU.
*
* Argument(s) : if_nbr      Network interface number to get MTU.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully returned.
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               --------- RETURNED BY NetIF_Get() : ----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Network interface's MTU, if NO error(s).
*
*               0,                       otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_MTU_Get() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetIF_MTU_Get() blocked until network initialization completes.
*********************************************************************************************************
*/

NET_MTU  NetIF_MTU_Get (NET_IF_NBR   if_nbr,
                        NET_ERR     *p_err)
{
    NET_IF   *p_if;
    NET_MTU   mtu;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_MTU)0);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_MTU_Get, p_err);       /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (0u);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }

                                                                /* ------------------ GET NET IF MTU ------------------ */
    mtu = p_if->MTU;
    goto exit_release;


exit_fail:
    mtu = 0;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (mtu);
}


/*
*********************************************************************************************************
*                                        NetIF_MTU_GetProtocol()
*
* Description : Get network interface's MTU for desired protocol layer.
*
* Argument(s) : if_nbr      Network interface number to get MTU.
*
*               protocol    Desired protocol layer of network interface MTU.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully returned.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_INVALID_PROTOCOL        Invalid network protocol.
*
*                                                               --------- RETURNED BY NetIF_Get() : ----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Network interface's MTU at desired protocol, if NO error(s).
*
*               0,                                           otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_MTU  NetIF_MTU_GetProtocol (NET_IF_NBR          if_nbr,
                                NET_PROTOCOL_TYPE   protocol,
                                NET_IF_FLAG         opt,
                                NET_ERR            *p_err)
{
    NET_IF      *p_if         = DEF_NULL;
    NET_IF_API  *p_if_api     = DEF_NULL;
    NET_MTU      mtu          = 0u;
    CPU_INT16U   pkt_size_hdr = 0u;

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }

                                                                /* ------------- CALC PROTOCOL LAYER MTU -------------- */

    pkt_size_hdr = NetIF_GetProtocolHdrSize(DEF_NULL, protocol, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }


    if (protocol == NET_PROTOCOL_TYPE_LINK) {
        p_if_api = p_if->IF_API;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
             if (p_if_api == DEF_NULL) {
                *p_err =  NET_IF_ERR_INVALID_CFG;
                 goto exit;
             }
             if (p_if_api->GetPktSizeHdr == DEF_NULL) {
                *p_err =  NET_ERR_FAULT_NULL_FNCT;
                 goto exit;
             }
#endif
             pkt_size_hdr +=  p_if_api->GetPktSizeHdr(p_if);
    }


    mtu = p_if->MTU - pkt_size_hdr;

   (void)&opt;

exit:
    return (mtu);
}


/*
*********************************************************************************************************
*                                        NetIF_GetPayloadRxMax()
*
* Description : Get maximum payload that can be received.
*
* Argument(s) : if_nbr      Network interface number to get payload.
*
*               protocol    Desired protocol layer of network interface.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully returned.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_INVALID_PROTOCOL        Invalid network protocol.
*
*                                                               --------- RETURNED BY NetIF_Get() : ----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Rx payload max
*
* Caller(s)   : NetTCP_RxConnWinSizeHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_GetPayloadRxMax (NET_IF_NBR          if_nbr,
                                   NET_PROTOCOL_TYPE   protocol,
                                   NET_ERR            *p_err)
{
    NET_IF        *p_if;
    NET_BUF_SIZE   buf_size = 0u;
    CPU_INT16U     hdr_size = 0u;
    CPU_INT16U     payload  = 0u;


    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }

                                                                /* -------------------- GET NET IF -------------------- */
    buf_size = NetIF_GetPktSizeMax(if_nbr, p_err);
    hdr_size = NetIF_GetProtocolHdrSize(p_if, protocol, p_err);

    payload  = buf_size - hdr_size;


exit:
    return (payload);
}


/*
*********************************************************************************************************
*                                        NetIF_GetPayloadTxMax()
*
* Description : Get maximum payload that can be transmitted.
*
* Argument(s) : if_nbr      Network interface number to get payload.
*
*               protocol    Desired protocol layer of network interface MTU.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully returned.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_INVALID_PROTOCOL        Invalid network protocol.
*
*                                                               --------- RETURNED BY NetIF_Get() : ----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Maximum Transmit payload.
*
* Caller(s)   : NetTCP_TxConnSync().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_GetPayloadTxMax (NET_IF_NBR          if_nbr,
                                   NET_PROTOCOL_TYPE   protocol,
                                   NET_ERR            *p_err)
{
    NET_IF        *p_if;
    CPU_INT16U     hdr_size = 0u;
    CPU_INT16U     payload  = 0u;


    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }

                                                                /* -------------------- GET NET IF -------------------- */
    hdr_size = NetIF_GetProtocolHdrSize(DEF_NULL, protocol, p_err);
    payload  = p_if->MTU - hdr_size;

exit:
    return (payload);
}


/*
*********************************************************************************************************
*                                            NetIF_MTU_Set()
*
* Description : Set network interface's MTU.
*
* Argument(s) : if_nbr      Network interface number to set MTU.
*
*               mtu         Desired maximum transmission unit size to configure.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               --- RETURNED BY NetIF_MTU_SetHandler() : ----
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully set.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_MTU          Invalid network interface MTU.
*
*                                                               See specific network interface(s) 'MTU_Set()'
*                                                                   for additional return error codes.
*
*                                                               --- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_MTU_Set() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetIF_MTU_Set() blocked until network initialization completes.
*********************************************************************************************************
*/

void  NetIF_MTU_Set (NET_IF_NBR   if_nbr,
                     NET_MTU      mtu,
                     NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_MTU_Set, p_err);       /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif

                                                                /* ------------------ SET NET IF MTU ------------------ */
    NetIF_MTU_SetHandler(if_nbr, mtu, p_err);

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                         NetIF_GetPktSizeMin()
*
* Description : Get network interface's minimum packet size.
*
* Argument(s) : if_nbr      Network interface number to get minimum packet size.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's minimum packet size
*                                                                   successfully returned.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               ------ RETURNED BY NetIF_Get() : ------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Network interface's minimum packet size, if NO error(s).
*
*               0,                                       otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_GetPktSizeMin (NET_IF_NBR   if_nbr,
                                 NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;
    CPU_INT16U   pkt_size_min;

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (0u);
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err =  NET_IF_ERR_INVALID_CFG;
        return (0u);
    }
    if (p_if_api->GetPktSizeMin == (CPU_INT16U (*)(NET_IF *))0) {
       *p_err =  NET_ERR_FAULT_NULL_FNCT;
        return (0u);
    }
#endif

                                                                /* ------------- GET NET IF MIN PKT SIZE -------------- */
    pkt_size_min = p_if_api->GetPktSizeMin(p_if);

   *p_err =  NET_IF_ERR_NONE;

    return (pkt_size_min);
}


/*
*********************************************************************************************************
*                                         NetIF_GetPktSizeMax()
*
* Description : Get network interface's maximum packet size.
*
* Argument(s) : if_nbr      Network interface number to get minimum packet size.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's minimum packet size
*                                                                   successfully returned.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               ------ RETURNED BY NetIF_Get() : ------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Network interface's minimum packet size, if NO error(s).
*
*               0,                                       otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_GetPktSizeMax (NET_IF_NBR   if_nbr,
                                 NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;
    CPU_INT16U   pkt_size_max;

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (0u);
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err =  NET_IF_ERR_INVALID_CFG;
        return (0u);
    }
    if (p_if_api->GetPktSizeMin == (CPU_INT16U (*)(NET_IF *))0) {
       *p_err =  NET_ERR_FAULT_NULL_FNCT;
        return (0u);
    }
#endif

                                                                /* ------------- GET NET IF MIN PKT SIZE -------------- */
    pkt_size_max = p_if_api->GetPktSizeMax(p_if);

   *p_err =  NET_IF_ERR_NONE;

    return (pkt_size_max);
}


/*
*********************************************************************************************************
*                                          NetIF_ISR_Handler()
*
* Description : Handle network interface's device interrupt service routine (ISR) function(s).
*
* Argument(s) : if_nbr      Network interface number  to handle ISR(s).
*
*               type        Device  interrupt type(s) to handle :
*
*                               NET_DEV_ISR_TYPE_UNKNOWN        Handle unknown device           ISR(s).
*                               NET_DEV_ISR_TYPE_RX             Handle device receive           ISR(s).
*                               NET_DEV_ISR_TYPE_RX_OVERRUN     Handle device receive  overrun  ISR(s).
*                               NET_DEV_ISR_TYPE_TX_RDY         Handle device transmit ready    ISR(s).
*                               NET_DEV_ISR_TYPE_TX_COMPLETE    Handle device transmit complete ISR(s).
*
*                           See also 'net_if.h  NETWORK DEVICE INTERRUPT SERVICE ROUTINE (ISR) TYPE DEFINES'
*                               for other available & supported network device ISR types.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface ISR(s) successfully handled.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state (see Note #3).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               ----------- RETURNED BY NetIF_Get() : -----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY 'p_if_api->ISR_Handler()' : -----
*                                                               See specific network interface(s) 'ISR_Handler()'
*                                                                   for additional return error codes.
*
*
* Return(s)   : none.
*
* Caller(s)   : Device driver(s)' Board Support Package (BSP) Interrupt Service Routine (ISR) handler(s).
*
*               This function is a network interface (IF) to network device function & SHOULD be called
*               only by appropriate network device driver ISR handler function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_ISR_Handler() is called by device driver function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST NOT block by pending on & acquiring the global network lock.
*
*                       (1) Although blocking on the global network lock is typically required since any
*                           external API function access is asynchronous to other network protocol tasks;
*                           interrupt service routines (ISRs) are (typically) prohibited from pending on
*                           OS objects & therefore can NOT acquire the global network lock.
*
*                       (2) Therefore, ALL network interface & network device driver functions that may
*                           be called by interrupt service routines MUST be able to be asynchronously
*                           accessed without acquiring the global network lock AND without corrupting
*                           any network data or task.
*
*               (2) NetIF_ISR_Handler() blocked until network initialization completes.
*
*                   (a) Although blocking on the global network lock is typically required to verify
*                       that network protocol suite initialization is complete; interrupt service routines
*                       (ISRs) are (typically) prohibited from pending on OS objects & therefore can NOT
*                       acquire the global network lock.
*
*                   (b) Therefore, since network protocol suite initialization complete MUST be able to
*                       be verified from interrupt service routines without acquiring the global network
*                       lock; 'Net_InitDone' MUST be accessed exclusively in critical sections during
*                       initialization & from asynchronous interrupt service routines.
*
*               (3) Network device interrupt service routines (ISR) handler(s) SHOULD be able to correctly
*                   function regardless of whether their corresponding network interface(s) are enabled.
*
*                   See also Note #1b2.
*********************************************************************************************************
*/

void  NetIF_ISR_Handler (NET_IF_NBR         if_nbr,
                         NET_DEV_ISR_TYPE   type,
                         NET_ERR           *p_err)
{
    NET_IF       *p_if;
    NET_IF_API   *p_if_api;
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_BOOLEAN   done;
    CPU_SR_ALLOC();
#endif


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }

    CPU_CRITICAL_ENTER();
    done = Net_InitDone;
    CPU_CRITICAL_EXIT();
    if (done != DEF_YES) {                                      /* If init NOT complete, exit (see Note #2).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        return;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);                              /* See Note #1b2.                                       */
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

#if 0                                                           /* See Note #3.                                         */
    if (p_if->En != DEF_ENABLED) {
       *p_err = NET_IF_ERR_INVALID_STATE;
        return;
    }
#endif

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->ISR_Handler == (void (*)(NET_IF         *,
                                          NET_DEV_ISR_TYPE,
                                          NET_ERR        *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* --------------- HANDLE NET IF ISR(s) --------------- */
    p_if_api->ISR_Handler(p_if, type, p_err);                      /* See Note #1b2.                                       */
}


/*
*********************************************************************************************************
*                                         NetIF_LinkStateGet()
*
* Description : Get network interface's last known physical link state (see also Note #3).
*
* Argument(s) : if_nbr      Network interface number to get last known physical link state.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's last known physical
*                                                                   link state successfully returned.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -------- RETURNED BY NetIF_Get() : --------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : --
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : NET_IF_LINK_UP,   if NO error(s) & network interface's last known physical link state was 'UP'.
*
*               NET_IF_LINK_DOWN, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_LinkStateGet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetIF_LinkStateGet() blocked until network initialization completes.
*
*               (3) NetIF_LinkStateGet() only returns a network interface's last known physical link state
*                   since enabled network interfaces' physical link states are only periodically updated
*                   (see 'NetIF_PhyLinkStateHandler()  Note #1a').
*
*                   See also 'NetIF_IO_Ctrl()  Note #5'.
*********************************************************************************************************
*/

NET_IF_LINK_STATE  NetIF_LinkStateGet (NET_IF_NBR   if_nbr,
                                       NET_ERR     *p_err)
{
    NET_IF_LINK_STATE   link_state;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(NET_IF_LINK_DOWN);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_LinkStateGet, p_err);  /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return (NET_IF_LINK_DOWN);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

    link_state = NetIF_LinkStateGetHandler(if_nbr, p_err);


    goto exit_release;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    link_state = NET_IF_LINK_DOWN;
#endif

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

   *p_err =  NET_IF_ERR_NONE;

    return (link_state);
}


/*
*********************************************************************************************************
*                                      NetIF_LinkStateGetHandler()
*
* Description : Get network interface's last known physical link state (see also Note #3).
*
* Argument(s) : if_nbr      Network interface number to get last known physical link state.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's last known physical
*                                                                   link state successfully returned.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -------- RETURNED BY NetIF_Get() : --------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : --
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : NET_IF_LINK_UP,   if NO error(s) & network interface's last known physical link state was 'UP'.
*
*               NET_IF_LINK_DOWN, otherwise.
*
* Caller(s)   : NetIF_LinkStateGet(),
*               NetMLDP_TxAdvertiseMembership().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_IF_LINK_STATE  NetIF_LinkStateGetHandler (NET_IF_NBR   if_nbr,
                                              NET_ERR     *p_err)
{
    NET_IF  *p_if;


                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (NET_IF_LINK_DOWN);
    }

                                                                /* -------------- GET NET IF LINK STATE --------------- */
    return (p_if->Link);
}


/*
*********************************************************************************************************
*                                     NetIF_LinkStateWaitUntilUp()
*
* Description : Wait for a network interface's link state to be 'UP'.
*
* Argument(s) : if_nbr          Network interface number to check link state.
*
*               retry_max       Maximum number of consecutive wait retries (see Note #2).
*
*               time_dly_ms     Transitory delay value, in milliseconds    (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's link state 'UP'.
*                               NET_ERR_IF_LINK_DOWN            Network interface's link state 'DOWN'.
*
*                                                               - RETURNED BY NetIF_LinkStateGet() : -
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : NET_IF_LINK_UP,   if NO error(s) & network interface's link state is 'UP'.
*
*               NET_IF_LINK_DOWN, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_LinkStateWaitUntilUp() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'NetIF_LinkStateGet()  Note #1b').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) If a non-zero number of retries is requested then a non-zero time delay SHOULD also be
*                   requested; otherwise, all retries will most likely fail immediately since no time will
*                   elapse to wait for & allow the network interface's link state to successfully be 'UP'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_LinkStateWaitUntilUp (NET_IF_NBR   if_nbr,
                                         CPU_INT16U   retry_max,
                                         CPU_INT32U   time_dly_ms,
                                         NET_ERR     *p_err)
{
    NET_IF_LINK_STATE  link_state = NET_IF_LINK_DOWN;
    CPU_BOOLEAN        done       = DEF_NO;
    CPU_BOOLEAN        dly        = DEF_NO;
    CPU_INT16U         retry_cnt  = 0u;
    NET_ERR            err        = NET_IF_ERR_NONE;
    NET_ERR            err_rtn    = NET_IF_ERR_NONE;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(NET_IF_LINK_DOWN);
    }
#endif
                                                                /* --------- WAIT FOR NET IF LINK STATE 'UP' ---------- */
    while ((retry_cnt <= retry_max) &&                          /* While retry <= max retry ...                         */
           (done      == DEF_NO)) {                             /* ... & link NOT UP,       ...                         */

        if (dly == DEF_YES) {
            KAL_Dly(time_dly_ms);
        }

        link_state = NetIF_LinkStateGet(if_nbr, &err);          /* ... chk link state.                                  */
        switch (err) {
            case NET_IF_ERR_NONE:
                 if (link_state == NET_IF_LINK_UP) {
                     done    = DEF_YES;
                     err_rtn = NET_IF_ERR_NONE;
                 } else {
                     retry_cnt++;
                     dly     = DEF_YES;
                     err_rtn = NET_ERR_IF_LINK_DOWN;
                 }
                 break;


            case NET_INIT_ERR_NOT_COMPLETED:                       /* If transitory err(s), ...                            */
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly retry.                                       */
                 err_rtn = err;
                 break;


            case NET_IF_ERR_INVALID_IF:
            default:
                 done    = DEF_YES;
                 err_rtn = err;
                 break;
        }
    }

   *p_err =  err_rtn;

    return (link_state);
}


/*
*********************************************************************************************************
*                                      NetIF_LinkStateSubscribe()
*
* Description : Subscribe to get notified when an interface link state changes.
*
* Argument(s) : if_nbr  Network interface number to check link state.
*
*               fcnt    Function to call when the link changes.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IF_ERR_NONE                     Link State Subscription successful.
*                           NET_ERR_FAULT_LOCK_ACQUIRE          Error while getting Network Lock.
*                           NET_INIT_ERR_NOT_COMPLETED          Network Initialization is not completed.
*
*                           --------- RETURNED BY NetIF_LinkStateSubscribeHandler() ---------
*                           See NetIF_LinkStateSubscribeHandler() for additional error codes.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_LinkStateSubscribe (NET_IF_NBR                    if_nbr,
                                NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                NET_ERR                      *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(NET_IF_LINK_DOWN);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_LinkStateGet, p_err);  /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         goto exit;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif


    NetIF_LinkStateSubscribeHandler(if_nbr, fcnt, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }


   *p_err = NET_IF_ERR_NONE;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

exit:
    return;
}


/*
*********************************************************************************************************
*                                   NetIF_LinkStateSubscribeHandler()
*
* Description : Subscribe to get notified when an interface link state changes.
*
* Argument(s) : if_nbr  Network interface number to check link state.
*
*               fcnt    Function to call when the link changes.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IF_ERR_NONE                         Link State Subscription successful.
*                           NET_ERR_FAULT_NULL_PTR                  Argument fcnt passed a null pointer.
*                           NET_IF_ERR_LINK_SUBSCRIBER_MEM_ALLOC    Error while allocating the memory block
*                                                                       for the subscriber.
*
*                           --------- RETURNED BY NetIF_Get() ---------
*                           See NetIF_Get() for additional error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_LinkStateSubscribe(),
*               NetMLDP_HostGrpAdd().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_LinkStateSubscribeHandler (NET_IF_NBR                    if_nbr,
                                       NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                       NET_ERR                      *p_err)
{
    NET_IF                      *p_if;
    NET_IF_LINK_SUBSCRIBER_OBJ  *p_obj;
    LIB_ERR                      err_lib;


#if ((NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED))
    if (fcnt == DEF_NULL) {
       *p_err  = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        goto exit;
    }


                                                                /* -------- VALIDATE SUBSCRIBER DOESN'T EXIST --------- */
    p_obj = p_if->LinkSubscriberListHeadPtr;
    while (p_obj != DEF_NULL) {
        if (p_obj->Fnct == fcnt) {
            p_obj->RefCtn++;
           *p_err = NET_IF_ERR_NONE;
            goto exit;
        }

        p_obj = p_obj->NextPtr;
    }


                                                                /* -------- GET MEMORY TO STORE NEW SUBSCRIBER -------- */
    p_obj = (NET_IF_LINK_SUBSCRIBER_OBJ *)Mem_DynPoolBlkGet(&p_if->LinkSubscriberPool, &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = NET_IF_ERR_LINK_SUBSCRIBER_MEM_ALLOC;
        goto exit;
    }

    p_obj->RefCtn  = 0u;
    p_obj->Fnct    = fcnt;
    p_obj->NextPtr = DEF_NULL;


                                                                /* -------------- UPDATE SUBSCRIBER LIST -------------- */
    if (p_if->LinkSubscriberListHeadPtr == DEF_NULL) {
        p_if->LinkSubscriberListHeadPtr  = p_obj;
        p_if->LinkSubscriberListEndPtr   = p_obj;
    } else {
        p_if->LinkSubscriberListEndPtr->NextPtr = p_obj;
        p_if->LinkSubscriberListEndPtr          = p_obj;
    }


   *p_err = NET_IF_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                     NetIF_LinkStateUnsubscribe()
*
* Description : Unsubscribe to get notified when interface link state changes.
*
* Argument(s) : if_nbr  Network interface number to check link state.
*
*               fcnt    Function to call when the link changes.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IF_ERR_NONE                     Link State Unsubscription successful.
*                           NET_ERR_FAULT_LOCK_ACQUIRE          Error while getting Network Lock.
*                           NET_INIT_ERR_NOT_COMPLETED          Network Initialization is not completed.
*
*                           --------- RETURNED BY NetIF_LinkStateUnSubscribeHandler() ---------
*                           See NetIF_LinkStateUnSubscribeHandler() for additional error codes.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_LinkStateUnsubscribe (NET_IF_NBR                    if_nbr,
                                  NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                  NET_ERR                      *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(NET_IF_LINK_DOWN);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_LinkStateGet, p_err);  /* See Note #1b.                                        */
    if (*p_err != NET_ERR_NONE) {
         goto exit;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif


    NetIF_LinkStateUnSubscribeHandler(if_nbr, fcnt, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_release;
    }


   *p_err = NET_IF_ERR_NONE;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

exit:
    return;
}


/*
*********************************************************************************************************
*                                  NetIF_LinkStateUnsubscribeHandler()
*
* Description : Unsubscribe to get notified when interface link state changes.
*
* Argument(s) : if_nbr  Network interface number to check link state.
*
*               fcnt    Function to call when the link changes.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IF_ERR_NONE                         Link State Unsubscription successful.
*                           NET_IF_ERR_LINK_SUBSCRIBER_NOT_FOUND    No Subscriber found.
*
*                           --------- RETURNED BY NetIF_Get() ---------
*                           See NetIF_Get() for additional error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_LinkStateUnsubscribe().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_LinkStateUnSubscribeHandler(NET_IF_NBR                    if_nbr,
                                        NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                        NET_ERR                      *p_err)
{
    NET_IF                      *p_if;
    NET_IF_LINK_SUBSCRIBER_OBJ  *p_obj;
    NET_IF_LINK_SUBSCRIBER_OBJ  *p_obj_prev;
    LIB_ERR                      err_lib;



                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit;
    }



    p_obj_prev = DEF_NULL;
    p_obj      = p_if->LinkSubscriberListHeadPtr;
    while (p_obj != DEF_NULL) {
                                                                /* ----------- FIND FNCT IN SUBSCRIBER LIST ----------- */
        if (p_obj->Fnct == fcnt) {
            if (p_obj->RefCtn == 0u) {
                if (p_obj == p_if->LinkSubscriberListHeadPtr) {

                    p_if->LinkSubscriberListHeadPtr = p_if->LinkSubscriberListHeadPtr->NextPtr;

                    if (p_if->LinkSubscriberListEndPtr == p_obj) {
                        p_if->LinkSubscriberListHeadPtr = DEF_NULL;
                        p_if->LinkSubscriberListEndPtr  = DEF_NULL;
                    }

                } else if (p_obj == p_if->LinkSubscriberListEndPtr) {
                    p_obj_prev->NextPtr = DEF_NULL;
                    p_if->LinkSubscriberListEndPtr = p_obj_prev;
                } else {
                    p_obj_prev->NextPtr = p_obj->NextPtr;
                }

                                                                    /* Release memory blk.                                  */
                Mem_DynPoolBlkFree(&p_if->LinkSubscriberPool, p_obj, &err_lib);
               (void)&err_lib;                                      /* Ignore error.                                        */
            } else {
                p_obj->RefCtn--;
            }

           *p_err = NET_IF_ERR_NONE;
            goto exit;
        }

        p_obj_prev = p_obj;
        p_obj      = p_obj->NextPtr;
    }


   *p_err = NET_IF_ERR_LINK_SUBSCRIBER_NOT_FOUND;

exit:
    return;
}


/*
*********************************************************************************************************
*                                            NetIF_IO_Ctrl()
*
* Description : (1) Handle network interface &/or device specific (I/O) control(s) :
*
*                   (a) Device link :
*                       (1) Get    device link info
*                       (2) Get    device link state
*                       (3) Update device link state
*
*
* Argument(s) : if_nbr      Network interface number to handle (I/O) controls.
*
*               opt         Desired I/O control option code to perform; additional control options may be
*                           defined by the device driver :
*
*                               NET_IF_IO_CTRL_LINK_STATE_GET       Get    device's current  physical link state,
*                                                                       'UP' or 'DOWN' (see Note #5).
*                               NET_IF_IO_CTRL_LINK_STATE_GET_INFO  Get    device's detailed physical link state
*                                                                       information.
*                               NET_IF_IO_CTRL_LINK_STATE_UPDATE    Update device's current  physical link state.
*
*               p_data      Pointer to variable that will receive possible I/O control data (see Note #4).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   --- RETURNED BY NetIF_IO_CtrlHandler() : ----
*                               NET_IF_ERR_NONE                     I/O control option successfully handled.
*                               NET_ERR_FAULT_NULL_PTR                 Argument 'p_data' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid I/O control option.
*
*                                                                   See specific network interface(s) 'IO_Ctrl()'
*                                                                       for additional return error codes.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIF_IO_Ctrl() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIF_IO_CtrlHandler()  Note #2'.
*
*               (3) NetIF_IO_Ctrl() blocked until network initialization completes.
*
*               (4) 'p_data' MUST point to a variable or memory buffer that is sufficiently sized AND
*                    aligned to receive any return data. If the option is :
*
*                   (a) NET_IF_IO_CTRL_LINK_STATE_GET:
*                       (1) For Ethernet or Wireless interface: p_data MUST point to a CPU_BOOLEAN variable.
*
*                   (b) NET_IF_IO_CTRL_LINK_STATE_GET_INFO
                        NET_IF_IO_CTRL_LINK_STATE_UPDATE
*
*                       (1) For an ethernet interface: p_data MUST point to a variable of data type
*                               NET_DEV_LINK_ETHER.
*
*                       (2) For a  Wireless interface: p_data MUST point to a variable of data type
*                               NET_DEV_LINK_WIFI.
*
*               (5) NetIF_IO_Ctrl() can return a network device's current physical link state (using the
*                   'NET_IF_IO_CTRL_LINK_STATE_GET' option).
*
*                   See also 'NetIF_LinkStateGet()  Note #3'.
*********************************************************************************************************
*/

void  NetIF_IO_Ctrl (NET_IF_NBR   if_nbr,
                     CPU_INT08U   opt,
                     void        *p_data,
                     NET_ERR     *p_err)
{
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_IO_Ctrl, p_err);       /* See Note #2b.                                        */
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }
#endif

                                                                /* ---------------- HANDLE NET IF I/O ----------------- */
    NetIF_IO_CtrlHandler(if_nbr, opt, p_data, p_err);

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                         NetIF_RxTaskSignal()
*
* Description : Signal network interface receive task of received packet.
*
* Argument(s) : if_nbr      Network interface to signal  receive.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface receive queue successfully
*                                                                   signaled.
*                               NET_IF_ERR_RX_Q_FULL            Network interface receive queue full.
*                               NET_IF_ERR_RX_Q_SIGNAL_FAULT    Network interface receive queue signal fault.
*
*                                                               --- RETURNED BY NetIF_IsValidHandler() : ----
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : Device driver receive ISR handler(s),
*               NetIF_Loopback_Tx().
*
*               This function is a network protocol suite to network device function & SHOULD be called
*               only by appropriate network device driver function(s).
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets SHOULD be
*                   handled in an APPROXIMATELY balanced ratio.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                   See also 'net_if.c  NetIF_RxPktInc()  Note #1'.
*
*               (2) Encoding/decoding the network interface number does NOT require any message size.
*
*                   See also 'NetIF_RxTaskWait()  Note #2'.
*********************************************************************************************************
*/

void  NetIF_RxTaskSignal (NET_IF_NBR   if_nbr,
                          NET_ERR     *p_err)
{
    CPU_ADDR  if_nbr_msg;
    KAL_ERR   err_kal;


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
   (void)NetIF_IsValidHandler(if_nbr, p_err);                   /* Validate interface number.                           */
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

    if_nbr_msg = (CPU_ADDR)if_nbr;                              /* Encode interface number of signaled receive.         */
    KAL_QPost(        NetIF_RxQ_Handle,
              (void *)if_nbr_msg,
                      KAL_OPT_PEND_NONE,
                     &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
                                                                /* Increment number of receive packets queued ...       */
             NetIF_RxPktInc(if_nbr);                            /* ... to a network interface (see Note #1b1A).         */
            *p_err = NET_IF_ERR_NONE;
             break;


        case KAL_ERR_OVF:
            *p_err = NET_IF_ERR_RX_Q_FULL;
             break;


        case KAL_ERR_RSRC:
        case KAL_ERR_OS:
        default:
            *p_err = NET_IF_ERR_RX_Q_SIGNAL_FAULT;
             break;
    }
}


/*
*********************************************************************************************************
*                                           NetIF_RxPktInc()
*
* Description : Increment number receive packet(s) queued & available on a network interface.
*
* Argument(s) : if_nbr      Interface number that received packet(s).
*               ------      Argument validated in NetIF_RxTaskSignal().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxTaskSignal().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets on each
*                   network interface SHOULD be handled in an APPROXIMATELY balanced ratio.
*
*                   (a) Network task priorities & lock mechanisms partially maintain a balanced ratio
*                       between network receive versus transmit packet handling.
*
*                       However, the handling of network receive & transmit packets :
*
*                       (1) SHOULD be interleaved so that for every few packet(s) received & handled,
*                           several packet(s) should be transmitted; & vice versa.
*
*                       (2) SHOULD NOT exclusively handle receive nor transmit packets, even for a
*                           short period of time, but especially for a prolonged period of time.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                           (B) Decrement the number of available network receive packets queued to a
*                               network interface for each received packet processed.
*
*                       (2) Certain network connections MUST periodically suspend network transmit(s)
*                           to handle network interface(s)' receive packet(s) :
*
*                           (A) Suspend network connection transmit(s) if any receive packets are
*                               available on a network interface.
*
*                           (B) Signal or timeout network connection transmit suspend(s) to restart
*                               transmit(s).
*
*                   See also 'NetIF_RxPktDec()         Note #1',
*                            'NetIF_RxPktIsAvail()     Note #1',
*                            'NetIF_TxSuspend()        Note #1',
*                          & 'NetIF_TxSuspendSignal()  Note #1'.
*
*               (2) Network interfaces' 'RxPktCtr' variables MUST ALWAYS be accessed exclusively in
*                   critical sections.
*********************************************************************************************************
*/

void  NetIF_RxPktInc (NET_IF_NBR  if_nbr)
{
    NET_ERR   err;
#ifdef  NET_LOAD_BAL_MODULE_EN
    NET_IF   *p_if;


    p_if = &NetIF_Tbl[if_nbr];
    NetStat_CtrInc(&p_if->RxPktCtr, &err);                       /* Inc net IF's  nbr q'd rx pkts avail (see Note #1b1A).*/

#else
   (void)&if_nbr;                                               /* Prevent 'variable unused' compiler warning.          */
#endif

    NetStat_CtrInc(&NetIF_RxTaskPktCtr, &err);                  /* Inc rx task's nbr q'd rx pkts avail.                 */
}


/*
*********************************************************************************************************
*                                         NetIF_RxPktIsAvail()
*
* Description : Determine if any network interface receive packet(s) are available.
*
* Argument(s) : rx_chk_nbr      Number of consecutive times that network interface's receive packet
*                                   availability has been checked (see Note #2b1).
*
* Return(s)   : DEF_YES, network interface receive packet(s)     available (see Note #2a1).
*
*               DEF_NO,  network interface receive packet(s) NOT available (see Note #2a2).
*
* Caller(s)   : NetTCP_TxConnTxQ().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets on each
*                   network interface SHOULD be handled in an APPROXIMATELY balanced ratio.
*
*                   (a) Network task priorities & lock mechanisms partially maintain a balanced ratio
*                       between network receive versus transmit packet handling.
*
*                       However, the handling of network receive & transmit packets :
*
*                       (1) SHOULD be interleaved so that for every few packet(s) received & handled,
*                           several packet(s) should be transmitted; & vice versa.
*
*                       (2) SHOULD NOT exclusively handle receive nor transmit packets, even for a
*                           short period of time, but especially for a prolonged period of time.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                           (B) Decrement the number of available network receive packets queued to a
*                               network interface for each received packet processed.
*
*                   See also 'NetIF_RxPktInc()  Note #1'
*                          & 'NetIF_RxPktDec()  Note #1'.
*
*               (2) (a) To approximate a balanced ratio of network receive versus transmit packets
*                       handled; the availability of network receive packets returned is conditionally
*                       based on the consecutive number of times the availability is checked :
*
*                       (1) If the number of available network receive packets queued ('NetIF_RxPktCtr')
*                           is greater than the consecutive number of times the availability is checked
*                           ('rx_chk_nbr'), then the actual availability of network receive packet is
*                           returned.
*
*                       (2) Otherwise, no available network receive packets is returned -- even if
*                           network receive packets ARE available.
*
*                   (b) (1) The number of consecutive times that the network receive availability
*                           is checked ('rx_chk_nbr') SHOULD correspond to the consecutive number
*                           of times that a network connection transmit suspends itself to check
*                           for & handle any network receive packet(s).
*
*                       (2) (A) To check actual network receive packet availability,
*                               call NetIF_RxPktIsAvail() with 'rx_chk_nbr' always    set to 0.
*
*                           (B) To check        network receive packet availability consecutively,
*                               call NetIF_RxPktIsAvail() with 'rx_chk_nbr' initially set to 0 &
*                               incremented by 1 for each consecutive call thereafter.
*
*               (3) Network interfaces' 'RxPktCtr' variables MUST ALWAYS be accessed exclusively in
*                   critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIF_RxPktIsAvail (NET_IF_NBR  if_nbr,
                                 NET_CTR     rx_chk_nbr)
{
    CPU_BOOLEAN    rx_pkt_avail;
#ifdef  NET_LOAD_BAL_MODULE_EN
    NET_IF        *p_if;
    NET_STAT_CTR  *pstat_ctr;
    CPU_SR_ALLOC();


    p_if      = &NetIF_Tbl[if_nbr];
    pstat_ctr = &p_if->RxPktCtr;

    CPU_CRITICAL_ENTER();
    rx_pkt_avail = (pstat_ctr->CurCtr > rx_chk_nbr) ? DEF_YES : DEF_NO;     /* See Note #2a1.                           */
    CPU_CRITICAL_EXIT();

#else
   (void)&if_nbr;                                               /* Prevent 'variable unused' compiler warnings.         */
   (void)&rx_chk_nbr;
    rx_pkt_avail = DEF_NO;
#endif

    return (rx_pkt_avail);
}


/*
*********************************************************************************************************
*                                       NetIF_TxDeallocTaskPost()
*
* Description : Post network buffer transmit data areas to deallocate from device(s) to network interface
*                   transmit deallocation queue.
*
* Argument(s) : p_buf_data   Pointer to transmit buffer data area to deallocate.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                         Transmit buffer data area successfully
*                                                                           posted to deallocation queue.
*                               NET_IF_ERR_TX_DEALLC_Q_FULL             Network interface transmit deallocation
*                                                                           queue full.
*                               NET_IF_ERR_TX_DEALLC_Q_SIGNAL_FAULT     Network interface transmit deallocation
*                                                                           queue signal fault.
*
* Return(s)   : none.
*
* Caller(s)   : Device driver transmit complete ISR handler(s).
*
*               This function is a network protocol suite to network device function & SHOULD be called
*               only by appropriate network interface/device controller function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_TxDeallocTaskPost (CPU_INT08U  *p_buf_data,
                               NET_ERR     *p_err)
{
    KAL_ERR  err_kal;


    KAL_QPost(        NetIF_TxQ_Handle,
              (void *)p_buf_data,
                      KAL_OPT_PEND_NONE,
                     &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_IF_ERR_NONE;
             break;


        case KAL_ERR_OVF:
            *p_err = NET_IF_ERR_TX_DEALLC_Q_FULL;
             break;


        case KAL_ERR_RSRC:
        case KAL_ERR_OS:
        default:
            *p_err = NET_IF_ERR_TX_DEALLC_Q_SIGNAL_FAULT;
             break;
    }
}



/*
*********************************************************************************************************
*                                              NetIF_Tx()
*
* Description : Transmit data packets to network interface(s)/device(s).
*
* Argument(s) : p_buf_list   Pointer to network buffer data packet(s) to transmit via network interface(s)/
*                               device(s) [see Note #1a].
*
*               p_err        Pointer to variable that will receive the return error code from this function
*                               (see Note #1b) :
*
*                                                               ----- RETURNED BY NetIF_TxHandler() : -----
*                               NET_IF_ERR_NONE                 Packet(s) successfully transmitted (or
*                                                                   queued for later transmission).
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
*                                                               --- RETURNED BY NetIF_TxPktDiscard() : ----
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_Tx(),
*               NetARP_CacheTxPktHandler(),
*               NetIP_TxPktDatagram().
*
*               This function is a network protocol suite to network interface (IF) function & SHOULD be
*               called only by appropriate network interface function(s).
*
* Note(s)     : (1) (a) On any error(s), the current transmit packet may be discarded by handler functions;
*                       but any remaining transmit packet(s) are still transmitted.
*
*                       However, while IP transmit fragmentation is NOT currently supported (see
*                      'net_ip.h  Note #1d'), transmit data packet lists are limited to a single transmit
*                       data packet.
*
*                       See also 'NetIF_TxPktDiscard()  Note #2'.
*
*                   (b) Error code returned by 'p_err' refers to the last transmit packet's error ONLY.
*********************************************************************************************************
*/

void  NetIF_Tx (NET_BUF  *p_buf_list,
                NET_ERR  *p_err)
{
    NET_BUF      *p_buf;
    NET_BUF      *p_buf_next;
    NET_BUF_HDR  *p_buf_hdr;
    NET_ERR       err_rtn;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf_list == (NET_BUF *)0) {
        NetIF_TxPktDiscard(p_buf_list, DEF_YES, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
        return;
    }
#endif

                                                                /* ----------------- TX NET IF PKT(S) ----------------- */
    p_buf    = p_buf_list;
    err_rtn = NET_ERR_TX;
    while (p_buf != (NET_BUF *)0) {                              /* Tx ALL pkt bufs in list.                             */
        p_buf_hdr  = &p_buf->Hdr;
        p_buf_next =  p_buf_hdr->NextBufPtr;


        NetIF_TxHandler(p_buf,
                       &err_rtn);                               /* See Note #1a.                                        */

        if (err_rtn != NET_IF_ERR_TX_ADDR_PEND) {
                                                                /* --------------- UNLINK CHAINED BUFS ---------------- */
            if (p_buf_next != (NET_BUF *)0) {
                p_buf_hdr->NextBufPtr = (NET_BUF *)0;
                p_buf_hdr = &p_buf_next->Hdr;
                p_buf_hdr->PrevBufPtr = (NET_BUF *)0;
            }
        }

        p_buf = p_buf_next;                                       /* Adv to next tx pkt buf.                              */
    }


   *p_err = err_rtn;                                             /* See Note #1b.                                        */
}


/*
*********************************************************************************************************
*                                          NetIF_TxIxDataGet()
*
* Description : Get the offset of a buffer at which the IPv6 packet can be written.
*
* Argument(s) : if_nbr      Network interface number to transmit data.
*
*               data_len    IPv6 payload size.
*
*               mtu         MTU for the upper-layer protocol.
*
*               p_ix        Pointer to the current protocol index.
*               ----        Argument validated in NetARP_TxIxDataGet(),
*                                                 NetIF_GetDataAlignPtr(),
*                                                 NetIPv6_GetTxDataIx().
*                                                 NetIPv4_TxIxDataGet(),
*                                                 NetIPv6_GetTxDataIx(),
*                                                 NetIPv6_TxPktPrepareExtHdr(),
*                                                 NetIPv6_TxPktPrepareHdr().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No errors.
*
*                                                               -------- Returned by NetIF_GetTxDataIx() ---------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_PROTOCOL     Network interface type is unsupported.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_TxIxDataGet(),
*               NetIF_GetDataAlignPtr(),
*               NetIPv4_TxIxDataGet(),
*               NetIPv6_GetTxDataIx(),
*               NetIPv6_TxPktPrepareExtHdr(),
*               NetIPv6_TxPktPrepareHdr().
*
* Note(s)     : none.
*********************************************************************************************************
*/
void  NetIF_TxIxDataGet (NET_IF_NBR   if_nbr,
                         CPU_INT32U   data_len,
                         CPU_INT16U  *p_ix,
                         NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;


    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        return;
    }


    p_if_api = (NET_IF_API *)p_if->IF_API;
    if (p_if_api == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_OBJ;
        goto exit;
    }


   *p_ix += p_if_api->GetPktSizeHdr(p_if);


   (void)&data_len;

   *p_err = NET_IF_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                          NetIF_TxSuspend()
*
* Description : Suspend transmit on network interface connection(s).
*
* Argument(s) : if_nbr      Interface number to suspend transmit.
*               ------      Argument checked in NetTCP_TxConnTxQ().
*
* Return(s)   : none.
*
* Caller(s)   : NetTCP_TxConnTxQ().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets on each
*                   network interface SHOULD be handled in an APPROXIMATELY balanced ratio.
*
*                   (a) Network task priorities & lock mechanisms partially maintain a balanced ratio
*                       between network receive versus transmit packet handling.
*
*                       However, the handling of network receive & transmit packets :
*
*                       (1) SHOULD be interleaved so that for every few packet(s) received & handled,
*                           several packet(s) should be transmitted; & vice versa.
*
*                       (2) SHOULD NOT exclusively handle receive nor transmit packets, even for a
*                           short period of time, but especially for a prolonged period of time.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                           (B) Decrement the number of available network receive packets queued to a
*                               network interface for each received packet processed.
*
*                       (2) Certain network connections MUST periodically suspend network transmit(s)
*                           to handle network interface(s)' receive packet(s) :
*
*                           (A) Suspend network connection transmit(s) if any receive packets are
*                               available on a network interface.
*
*                           (B) Signal or timeout network connection transmit suspend(s) to restart
*                               transmit(s).
*
*                   See also 'NetIF_RxPktInc()         Note #1',
*                            'NetIF_RxPktDec()         Note #1',
*                          & 'NetIF_TxSuspendSignal()  Note #1'.
*
*               (2) (a) To approximate a balanced ratio of network receive versus transmit packets handled;
*                       the number of consecutive times that a network connection transmit suspends itself
*                       to check for & handle any network receive packet(s) SHOULD APPROXIMATELY correspond
*                       to the number of queued receive packet(s) available on a network interface.
*
*                       See also 'NetIF_RxPktIsAvail()  Note #2'.
*
*                   (b) To protect connections from transmit corruption while suspended, ALL transmit
*                       operations for suspended connections MUST be blocked until the connection is no
*                       longer suspended.
*
*               (3) Network interfaces' 'TxSuspendCtr' variables may be accessed with only the global
*                   network lock acquired & are NOT required to be accessed exclusively in critical
*                   sections.
*********************************************************************************************************
*/

void  NetIF_TxSuspend (NET_IF_NBR  if_nbr)
{
#ifdef  NET_LOAD_BAL_MODULE_EN
    NET_IF   *p_if;
    NET_ERR   err;


    p_if = &NetIF_Tbl[if_nbr];
    NetStat_CtrInc(&p_if->TxSuspendCtr, &err);                   /* Inc net IF's tx suspend ctr.                         */
    NetIF_TxSuspendWait(p_if);                                   /* Wait on      tx suspend signal (see Note #1b2A).     */
    NetStat_CtrDec(&p_if->TxSuspendCtr, &err);                   /* Dec net IF's tx suspend ctr.                         */
#else
   (void)&if_nbr;                                               /* Prevent 'variable unused' compiler warning.          */
#endif
}

/*
*********************************************************************************************************
*                                       NetIF_DevCfgTxRdySignal()
*
* Description : (1) Configure the value of a network device transmit ready signal :
*
*                   (a) The value of the transmit ready signal should be configured with either the
*                       number of available transmit descriptors for a DMA device or the number of
*                       packet that can be buffered within a non-DMA device.
*
*
* Argument(s) : if_nbr      Interface number of the network device transmit ready signal.
*
*               cnt         Desired count    of the network device transmit ready signal.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                        Network device transmit ready signal
*                                                                       value successfully configured.
*                               NET_IF_ERR_INIT_DEV_TX_RDY_VAL      Invalid device transmit ready signal.
*
*                                                                   - RETURNED BY NetIF_IsValidHandler() : -
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : Device driver start functions.
*
*               This function is a network interface (IF) to network device function & SHOULD (optionally)
*               be called only by appropriate network device driver function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_DevCfgTxRdySignal (NET_IF      *p_if,
                               CPU_INT16U   cnt,
                               NET_ERR     *p_err)
{
    KAL_ERR  err_kal;


    if (cnt < 1) {
       *p_err = NET_IF_ERR_DEV_TX_RDY_VAL;
        return;
    }

    KAL_SemSet(p_if->DevTxRdySignalObj, cnt, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_NULL_PTR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_OS:
        default:
            *p_err = NET_IF_ERR_DEV_TX_RDY_VAL;
             return;
    }

   *p_err = NET_IF_ERR_NONE;
}



/*
*********************************************************************************************************
*                                        NetIF_DevTxRdySignal()
*
* Description : Signal that device transmit is ready.
*
* Argument(s) : if_nbr  Interface number to signal transmit ready.
*
* Return(s)   : none.
*
* Caller(s)   : Device driver transmit ISR handler(s).
*
*               This function is a network interface (IF) to network device function & SHOULD be called
*               only by appropriate network device driver function(s).
*
* Note(s)     : (1) Device transmit ready MUST be signaled--i.e. MUST signal without failure.
*
*                   (a) Failure to signal device transmit ready will prevent device from transmitting
*                       packets.  Thus, device transmit ready is assumed to be successfully signaled
*                       since NO uC/OS-III error handling could be performed to counteract failure.
*********************************************************************************************************
*/

void  NetIF_DevTxRdySignal (NET_IF  *p_if)
{
    KAL_ERR  err_kal;



    KAL_SemPost(p_if->DevTxRdySignalObj,                        /* Signal device that transmit ready.                   */
                KAL_OPT_PEND_NONE,
               &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;

        case KAL_ERR_OVF:
        case KAL_ERR_OS:
        default:
             return;

    }

   (void)&err_kal;                                              /* See Note #1a.                                        */
}



/*
*********************************************************************************************************
*                                      NetIF_TxSuspendTimeoutSet()
*
* Description : Set network interface transmit suspend timeout value.
*
* Argument(s) : if_nbr      Interface number to set timeout value.
*
*               timeout_ms  Timeout value (in milliseconds).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                    Network interface transmit suspend timeout
*                                                                   successfully set.
*
*                                                               --- RETURNED BY NetIF_IsValidHandler() : ----
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API)
*               function & MAY be called by application function(s).
*
* Note(s)     : (1) 'NetIF_TxSuspendTimeout_tick' variables MUST ALWAYS be accessed exclusively
*                    in critical sections.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
void  NetIF_TxSuspendTimeoutSet (NET_IF_NBR   if_nbr,
                                 CPU_INT32U   timeout_ms,
                                 NET_ERR     *p_err)
{
    NET_IF    *p_if;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
   (void)NetIF_IsValidHandler(if_nbr, p_err);                    /* Validate interface number.                           */
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }


    CPU_CRITICAL_ENTER();
    p_if->TxSuspendTimeout_ms = timeout_ms;                     /* Set transmit suspend timeout value (in OS ticks).    */
    CPU_CRITICAL_EXIT();


   *p_err = NET_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                   NetIF_TxSuspendTimeoutGet_ms()
*
* Description : Get network interface transmit suspend timeout value.
*
* Argument(s) : if_nbr      Interface number to get timeout value.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_NONE                    Network interface transmit suspend timeout
*                                                                   successfully returned.
*
*                                                               -- RETURNED BY NetIF_IsValidHandler() : --
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Network transmit suspend timeout value (in milliseconds), if NO error(s).
*
*               0,                                                        otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API)
*               function & MAY be called by application function(s).
*
* Note(s)     : (1) 'NetIF_TxSuspendTimeout_tick' variables MUST ALWAYS be accessed exclusively
*                    in critical sections.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
CPU_INT32U  NetIF_TxSuspendTimeoutGet_ms (NET_IF_NBR   if_nbr,
                                          NET_ERR     *p_err)
{
    NET_IF      *p_if;
    CPU_INT32U   timeout_ms;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
   (void)NetIF_IsValidHandler(if_nbr, p_err);                    /* Validate interface number.                           */
    if (*p_err != NET_IF_ERR_NONE) {
         return (0u);
    }
#endif

    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (0u);
    }

    CPU_CRITICAL_ENTER();
    timeout_ms = p_if->TxSuspendTimeout_ms;                     /* Get transmit suspend timeout value (in OS ticks).    */
    CPU_CRITICAL_EXIT();

   *p_err            = NET_ERR_NONE;

    return (timeout_ms);
}
#endif


/*
*********************************************************************************************************
*                                        NetIF_MTU_SetHandler()
*
* Description : Set network interface's MTU.
*
* Argument(s) : if_nbr      Network interface number to set MTU.
*
*               mtu         Desired maximum transmission unit size to set.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               --------- RETURNED BY NetIF_Get() : ---------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY 'p_if_api->MTU_Set()' : -----
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully set.
*                               NET_IF_ERR_INVALID_MTU          Invalid network interface MTU.
*
*                                                               See specific network interface(s) 'MTU_Set()'
*                                                                   for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_MTU_Set(),
*               NetNDP_RxRouterAdvertisement().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_MTU_SetHandler (NET_IF_NBR   if_nbr,
                            NET_MTU      mtu,
                            NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->MTU_Set == (void (*)(NET_IF  *,
                                      NET_MTU  ,
                                      NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* ------------------ SET NET IF MTU ------------------ */
    p_if_api->MTU_Set(p_if, mtu, p_err);
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
*                                          NetIF_InitTaskObj()
*
* Description : (1) Perform network interface/OS initialization :
*
*                   (a) (1) Create Network Interface Receive Task
*
*                       (2) Implement network interface receive queue by creating a message queue.
*
*                           (A) Initialize network interface receive queue with no received packets by NOT
*                               posting any messages to the queue.
*
*                   (b) (1) Create Network Interface Transmit Deallocation Task
*
*                       (2) Implement network interface transmit deallocation queue by creating a message
*                           queue.
*
*                           (A) Initialize network interface transmit deallocation queue with no posted
*                               transmit packets by NOT posting any messages to the queue.
*
*                   (c) Implement  network interface transmit suspend signal by creating a counting semaphore.
*
*                       (1) Initialize network interface transmit suspend signal with no signal by setting
*                           the semaphore count to 0 to block the semaphore.
*
*
* Argument(s) : p_err   Pointer to variable that will receive the return error code from this function :
*                       successful.
*
*                           NET_IF_ERR_NONE
*                           NET_IF_ERR_INIT_RX_TASK_MEM_ALLOC
*                           NET_IF_ERR_INIT_RX_TASK_CREATE
*                           NET_IF_ERR_INIT_RX_Q_INVALID_ARG
*                           NET_IF_ERR_INIT_RX_Q_MEM_ALLOC
*                           NET_IF_ERR_INIT_RX_Q_CREATE
*                           NET_IF_ERR_INIT_TX_DEALLOC_TASK_INVALID_ARG
*                           NET_IF_ERR_INIT_TX_DEALLOC_TASK_MEM_ALLOC
*                           NET_IF_ERR_INIT_TX_DEALLOC_TASK_CREATE
*                           NET_IF_ERR_INIT_TX_DEALLOC_Q_MEM_ALLOC
*                           NET_IF_ERR_INIT_TX_DEALLOC_Q_INVALID_ARG
*                           NET_IF_ERR_INIT_TX_DEALLOC_Q_CREATE
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_InitTaskObj (const  NET_TASK_CFG  *p_rx_task_cfg,
                                 const  NET_TASK_CFG  *p_tx_task_cfg,
                                        NET_ERR       *p_err)
{
    KAL_ERR  err_kal;



                                                                /* ----------- INITIALIZE NETWORK INTERFACE RECEIVE ----------- */
                                                                /* Create    network interface receive task & queue ...         */
                                                                /* ... (see Note #1a).                                          */
    NetIF_RxTaskHandle = KAL_TaskAlloc((const  CPU_CHAR *)NET_IF_RX_TASK_NAME,
                                                          p_rx_task_cfg->StkPtr,
                                                          p_rx_task_cfg->StkSizeBytes,
                                                          DEF_NULL,
                                                         &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_INVALID_ARG:
            *p_err = NET_IF_ERR_INIT_RX_TASK_INVALID_ARG;
             return;


        case KAL_ERR_MEM_ALLOC:
        default:
            *p_err = NET_IF_ERR_INIT_RX_TASK_MEM_ALLOC;
             return;
    }



    NetIF_RxQ_Handle = KAL_QCreate((const CPU_CHAR *)NET_IF_RX_Q_NAME,
                                                     NET_CFG_IF_RX_Q_SIZE,
                                                     DEF_NULL,
                                                    &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_IF_ERR_INIT_RX_Q_MEM_ALLOC;
             return;


        case KAL_ERR_INVALID_ARG:
            *p_err = NET_IF_ERR_INIT_RX_Q_INVALID_ARG;
             return;


        case KAL_ERR_ISR:
        case KAL_ERR_CREATE:
        default:
            *p_err = NET_IF_ERR_INIT_RX_Q_CREATE;
             return;
    }


    NetIF_RxQ_SizeCfg(NET_CFG_IF_RX_Q_SIZE);                    /* Configure network interface receive queue size.      */


    KAL_TaskCreate(NetIF_RxTaskHandle,
                   NetIF_RxTask,
                   DEF_NULL,
                   p_rx_task_cfg->Prio,
                   DEF_NULL,
                  &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ISR:
        case KAL_ERR_OS:
        default:
            *p_err = NET_IF_ERR_INIT_RX_TASK_CREATE;
             return;
    }




                                                        /* ---- INITIALIZE NETWORK INTERFACE TRANSMIT DEALLOCATION ---- */
                                                        /* Create    network interface transmit deallocation task ...   */
                                                        /* ... & queue (see Note #1b).                                  */
    NetIF_TxDeallocTaskHandle = KAL_TaskAlloc((const  CPU_CHAR *)NET_IF_TX_DEALLOC_TASK_NAME,
                                                                 p_tx_task_cfg->StkPtr,
                                                                 p_tx_task_cfg->StkSizeBytes,
                                                                 DEF_NULL,
                                                                &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_INVALID_ARG:
            *p_err = NET_IF_ERR_INIT_TX_DEALLOC_TASK_INVALID_ARG;
             return;


        case KAL_ERR_MEM_ALLOC:
        default:
            *p_err = NET_IF_ERR_INIT_TX_DEALLOC_TASK_MEM_ALLOC;
             return;
    }



    NetIF_TxQ_Handle = KAL_QCreate((const CPU_CHAR *)NET_IF_TX_DEALLOC_Q_NAME,
                                                     NET_CFG_IF_TX_DEALLOC_Q_SIZE,
                                                     DEF_NULL,
                                                    &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_IF_ERR_INIT_TX_DEALLOC_Q_MEM_ALLOC;
             return;


        case KAL_ERR_INVALID_ARG:
            *p_err = NET_IF_ERR_INIT_TX_DEALLOC_Q_INVALID_ARG;
             return;


        case KAL_ERR_ISR:
        case KAL_ERR_CREATE:
        default:
            *p_err = NET_IF_ERR_INIT_TX_DEALLOC_Q_CREATE;
             return;
    }

    NetIF_TxDeallocQ_SizeCfg(NET_CFG_IF_TX_DEALLOC_Q_SIZE);


    KAL_TaskCreate(NetIF_TxDeallocTaskHandle,
                   NetIF_TxDeallocTask,
                   DEF_NULL,
                   p_tx_task_cfg->Prio,
                   DEF_NULL,
                  &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ISR:
        case KAL_ERR_OS:
        default:
            *p_err = NET_IF_ERR_INIT_TX_DEALLOC_TASK_CREATE;
             return;
    }


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetIF_ObjInit()
*
* Description : Create and initialize interface's OS objects.
*
* Argument(s) : p_if    Pointer to network interface.
*               ----    Argument validated in NetIF_Add().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                       NET_IF_ERR_NONE
*                       NET_IF_ERR_INIT_TX_SUSPEND_MEM_ALLOC
*                       NET_IF_ERR_INIT_TX_SUSPEND_SEM_INVALID_ARG
*                       NET_IF_ERR_INIT_TX_SUSPEND_SEM_CREATE
*                       NET_IF_ERR_INIT_TX_SUSPEND_TIMEOUT
*                       NET_IF_ERR_INIT_TX_SUSPEND_MEM_ALLOC
*                       NET_IF_ERR_INIT_TX_SUSPEND_SEM_INVALID_ARG
*                       NET_IF_ERR_INIT_TX_SUSPEND_SEM_CREATE
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Add().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_ObjInit (NET_IF   *p_if,
                             NET_ERR  *p_err)
{
    KAL_ERR  err_kal;
    LIB_ERR  err_lib;


    p_if->DevTxRdySignalObj = KAL_SemCreate((const CPU_CHAR  *)NET_IF_DEV_TX_RDY_NAME,
                                                               DEF_NULL,
                                                              &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_IF_ERR_INIT_TX_SUSPEND_MEM_ALLOC;
             goto exit;


        case KAL_ERR_INVALID_ARG:
            *p_err = NET_IF_ERR_INIT_TX_SUSPEND_SEM_INVALID_ARG;
             goto exit;


        case KAL_ERR_ISR:
        case KAL_ERR_CREATE:
        default:
             *p_err = NET_IF_ERR_INIT_TX_SUSPEND_SEM_CREATE;
              goto exit;
    }


#ifdef  NET_LOAD_BAL_MODULE_EN                /* ------ INITIALIZE NETWORK INTERFACE TRANSMIT SUSPEND ------- */

                                                        /* Initialize transmit suspend signals' timeout values.         */
    NetIF_TxSuspendTimeoutInit(p_if, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
       *p_err = NET_IF_ERR_INIT_TX_SUSPEND_TIMEOUT;
        goto exit_fail_tx_signal;
    }


   p_if->TxSuspendSignalObj = KAL_SemCreate((const CPU_CHAR  *)NET_IF_TX_SUSPEND_NAME,
                                                               DEF_NULL,
                                                              &err_kal);
   switch (err_kal) {
       case KAL_ERR_NONE:
            break;


       case KAL_ERR_MEM_ALLOC:
           *p_err = NET_IF_ERR_INIT_TX_SUSPEND_MEM_ALLOC;
            goto exit_fail_tx_signal;


       case KAL_ERR_INVALID_ARG:
           *p_err = NET_IF_ERR_INIT_TX_SUSPEND_SEM_INVALID_ARG;
            goto exit_fail_tx_signal;


       case KAL_ERR_ISR:
       case KAL_ERR_CREATE:
       default:
            *p_err = NET_IF_ERR_INIT_TX_SUSPEND_SEM_CREATE;
             goto exit_fail_tx_signal;

   }
#endif


   Mem_DynPoolCreate(NET_IF_LINK_SUBSCRIBER,
                    &p_if->LinkSubscriberPool,
                     DEF_NULL,
                     sizeof(NET_IF_LINK_SUBSCRIBER_OBJ),
                     sizeof(CPU_DATA),
                     0u,
                     LIB_MEM_BLK_QTY_UNLIMITED,
                    &err_lib);
   if(err_lib != LIB_MEM_ERR_NONE) {
      *p_err = NET_IF_ERR_LINK_SUBSCRIBER_MEM_ALLOC;
       goto exit_fail_tx_suspend;
   }

   p_if->LinkSubscriberListHeadPtr = DEF_NULL;
   p_if->LinkSubscriberListEndPtr  = DEF_NULL;


  *p_err = NET_IF_ERR_NONE;
   goto exit;


exit_fail_tx_suspend:
#ifdef  NET_LOAD_BAL_MODULE_EN
    KAL_SemDel(p_if->TxSuspendSignalObj, &err_kal);

exit_fail_tx_signal:

#endif
    KAL_SemDel(p_if->DevTxRdySignalObj, &err_kal);

exit:
     return;
}


/*
*********************************************************************************************************
*                                            NetIF_ObjDel()
*
* Description : Delete interface's OS objects.
*
* Argument(s) : p_if    Pointer to network interface.
*               ----    Argument validated in NetIF_Add().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Add().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_ObjDel (NET_IF  *p_if)
{
    KAL_ERR  err_kal;


#ifdef  NET_LOAD_BAL_MODULE_EN
    KAL_SemDel(p_if->TxSuspendSignalObj, &err_kal);
#endif

    KAL_SemDel(p_if->DevTxRdySignalObj,  &err_kal);

   (void)&err_kal;
}


/*
*********************************************************************************************************
*                                         NetIF_DevTxRdyWait()
*
* Description : Wait on device transmit ready signal.
*
* Argument(s) : if_nbr      Interface number to wait on transmit ready signal.
*               ------      Argument validated in NetIF_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                    Device transmit ready signal     received.
*                               NET_DEV_ERR_TX_RDY_SIGNAL_TIMEOUT   Device transmit ready signal NOT received
*                                                                       within timeout.
*                               NET_DEV_ERR_TX_RDY_SIGNAL_FAULT     Device transmit ready signal fault.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Tx().
*
*               This function is a network interface (IF) to network device function & SHOULD be called
*               only by appropriate network interface function(s).
*
* Note(s)     : (1) (a) If timeouts NOT desired, wait for device transmit ready signal.
*
*                   (b) If timeout      desired, return NET_DEV_ERR_TX_RDY_SIGNAL_TIMEOUT error on transmit
*                       ready timeout.  Implement timeout with OS-dependent functionality.
*********************************************************************************************************
*/

static  void  NetIF_DevTxRdyWait (NET_IF   *p_if,
                                  NET_ERR  *p_err)
{
    KAL_ERR  err_kal;


    KAL_SemPend(p_if->DevTxRdySignalObj, KAL_OPT_PEND_NONE, 15,  &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_IF_ERR_NONE;
             break;


        case KAL_ERR_TIMEOUT:
            *p_err = NET_IF_ERR_TX_RDY_SIGNAL_TIMEOUT;          /* See Note #1b.                                        */
             break;


        case KAL_ERR_ABORT:
        case KAL_ERR_ISR:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
            *p_err = NET_IF_ERR_TX_RDY_SIGNAL_FAULT;
             break;
    }
}


/*
*********************************************************************************************************
*                                            NetIF_RxTask()
*
* Description : OS-dependent shell task to schedule & run Network Interface Receive Task handler.
*
*               (1) Shell task's primary purpose is to schedule & run NetIF_RxTaskHandler() forever;
*                   (i.e. shell task should NEVER exit).
*
*
* Argument(s) : p_data      Pointer to task initialization data (required by uC/OS-III).
*
* Return(s)   : none.
*
* Created by  : NetIF_InitTaskObj().
*
* Note(s)     : (2) To prevent deadlocking any lower priority task(s), network tasks SHOULD delay for a
*                   (brief) time after any network task handlers exit.
*********************************************************************************************************
*/

static  void  NetIF_RxTask (void  *p_data)
{
   (void)&p_data;                                               /* Prevent 'variable unused' compiler warning.          */

    while (DEF_ON) {
        NetIF_RxTaskHandler();
        KAL_DlyTick(1u, KAL_OPT_DLY_NONE);                      /* Dly for lower prio task(s) [see Note #2].            */
    }
}



/*
*********************************************************************************************************
*                                         NetIF_RxTaskHandler()
*
* Description : (1) Handle received data packets from all enabled network interface(s)/device(s) :
*
*                   (a) Wait for packet receive signal from network interface(s)/device(s)
*                   (b) Acquire  network  lock                                              See Note #3
*                   (c) Handle   received packet
*                   (d) Release  network  lock
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxTask().
*
*               This function is a network protocol suite to operating system (OS) function & SHOULD be
*               called only by appropriate network-operating system port function(s).
*
* Note(s)     : (2) NetIF_RxTaskHandler() blocked until network initialization completes.
*
*               (3) NetIF_RxTaskHandler() blocks ALL other network protocol tasks by pending on & acquiring
*                   the global network lock (see 'net.h  Note #3').
*********************************************************************************************************
*/

static  void  NetIF_RxTaskHandler (void)
{
    NET_IF_NBR  if_nbr;
    NET_ERR     err;


    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        Net_InitCompWait(&err);                                 /* ... wait on net init (see Note #2).                  */
        if (err != NET_ERR_NONE) {
            return;
        }
    }


    while (DEF_ON) {
                                                                /* ------------------ WAIT FOR RX PKT ----------------- */
        do {
            if_nbr = NetIF_RxTaskWait(&err);
        } while (err != NET_IF_ERR_NONE);

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #3.                                         */
        Net_GlobalLockAcquire((void *)&NetIF_RxTaskHandler, &err);
        if (err != NET_ERR_NONE) {
            continue;
        }

                                                                /* ------------------ HANDLE RX PKT ------------------- */
        NetIF_RxHandler(if_nbr);

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
        Net_GlobalLockRelease();
    }
}


/*
*********************************************************************************************************
*                                          NetIF_RxTaskWait()
*
* Description : Wait on network interface receive queue for receive signal.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface receive queue signal
*                                                                   successfully  received.
*                               NET_IF_ERR_RX_Q_EMPTY           Network interface receive queue empty.
*                               NET_IF_ERR_RX_Q_SIGNAL_FAULT    Network interface receive queue signal
*                                                                   fault.
*
* Return(s)   : Interface number of signaled receive, if NO error(s).
*
*               NET_IF_NBR_NONE,                      otherwise.
*
* Caller(s)   : NetIF_RxTaskHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) If timeouts NOT desired, wait on network interface receive queue until signaled
*                       (i.e. do NOT exit).
*
*                   (b) If timeout      desired, return NET_IF_ERR_RX_Q_EMPTY error on receive queue
*                       timeout.  Implement timeout with OS-dependent functionality.
*
*               (2) Encoding/decoding the network interface number does NOT require any message size.
*
*                   See also 'NetIF_RxTaskSignal()  Note #2'.
*********************************************************************************************************
*/

static  NET_IF_NBR  NetIF_RxTaskWait (NET_ERR  *p_err)
{
    void         *p_rx_q;
    CPU_ADDR      if_nbr_msg;
    NET_IF_NBR    if_nbr;
    KAL_ERR       err_kal;

                                                                /* Wait on network interface receive task queue ...     */
                                                                /* ... preferably without timeout (see Note #1a).       */
    p_rx_q = KAL_QPend(NetIF_RxQ_Handle, KAL_OPT_PEND_NONE, 0, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             if_nbr_msg = (CPU_ADDR  )p_rx_q;
             if_nbr     = (NET_IF_NBR)if_nbr_msg;               /* Decode interface number of signaled receive.         */
            *p_err      =  NET_IF_ERR_NONE;
             break;


        case KAL_ERR_TIMEOUT:
             if_nbr = NET_IF_NBR_NONE;
            *p_err   = NET_IF_ERR_RX_Q_EMPTY;                    /* See Note #1b.                                        */
             break;


        case KAL_ERR_ISR:
        case KAL_ERR_ABORT:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
             if_nbr = NET_IF_NBR_NONE;
            *p_err  = NET_IF_ERR_RX_Q_SIGNAL_FAULT;
             break;
    }

    return (if_nbr);
}


/*
*******************************************************************************************************
*                                           NetIF_RxHandler()
*
* Description : (1) Receive data packets from network interface(s)/device(s) :
*
*                   (a) Receive packet from interface/device :
*                       (1) Get    receive packet's   network interface
*                       (2) Update receive packet counters
*                       (3)        Receive packet via network interface
*
*                   (b) Handle network load balancing  #### NET-821
*                   (c) Update receive statistics
*
*
* Argument(s) : if_nbr      Network interface number that received a packet.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxTaskHandler().
*
* Note(s)     : (2) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIF_RxHandler (NET_IF_NBR  if_nbr)
{
    NET_IF        *p_if;
    NET_BUF_SIZE   size;
    NET_ERR        err;


                                                                /* --------------- GET RX PKT's NET IF ---------------- */
    p_if = NetIF_Get(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        return;
    }


                                                                /* ------------------ RX NET IF PKT ------------------- */
    NetStat_CtrDec(&NetIF_RxTaskPktCtr, &err);                  /* Dec rx task's nbr q'd rx pkts avail.                 */

    NET_CTR_STAT_INC(Net_StatCtrs.IFs.RxPktCtr);
    NET_CTR_STAT_INC(Net_StatCtrs.IFs.IF[if_nbr].RxNbrPktCtr);


    switch (if_nbr) {
        case NET_IF_NBR_LOOPBACK:
#ifdef NET_IF_LOOPBACK_MODULE_EN
             size = NetIF_Loopback_Rx(p_if,
                                     &err);
#else
             err = NET_IF_ERR_INVALID_IF;
#endif
             break;


        default:
             size = NetIF_RxPkt(p_if,
                               &err);
             break;
    }



#ifdef  NET_LOAD_BAL_MODULE_EN                                  /* --------------- HANDLE NET LOAD BAL ---------------- */
    NetIF_RxHandlerLoadBal(p_if);
#endif


                                                                /* ----------------- UPDATE RX STATS ------------------ */
    switch (err) {                                              /* Chk err from NetIF_Loopback_Rx() / NetIF_RxPkt().    */
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.IF[if_nbr].RxNbrPktCtrProcessed);
             NET_CTR_STAT_ADD(Net_StatCtrs.IFs.IF[if_nbr].RxNbrOctets, size);

            (void)&size;                                        /* Prevent possible 'variable unused' warning.          */
             break;


        case NET_ERR_INVALID_TRANSACTION:
        case NET_ERR_INVALID_PROTOCOL:
        case NET_ERR_RX:
        case NET_DEV_ERR_RX:
        case NET_IF_ERR_INVALID_IF:
        case NET_IF_ERR_INVALID_CFG:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_ERR_FAULT_MEM_ALLOC:
        case NET_ERR_IF_LOOPBACK_DIS:
        case NET_BUF_ERR_NONE_AVAIL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_SIZE:
        case NET_BUF_ERR_INVALID_IX:
        case NET_BUF_ERR_INVALID_LEN:
        default:
                                                                /* See Note #2.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.RxPktDisCtr);
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].RxPktDisCtr);
             return;
    }
}


/*
*********************************************************************************************************
*                                       NetIF_RxHandlerLoadBal()
*
* Description : Handle network receive versus transmit load balancing.
*
* Argument(s) : p_if        Pointer to network interface to handle load balancing.
*               ----        Argument validated in NetIF_RxHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxHandler().
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets on each
*                   network interface SHOULD be handled in an APPROXIMATELY balanced ratio.
*
*                   (a) Network task priorities & lock mechanisms partially maintain a balanced ratio
*                       between network receive versus transmit packet handling.
*
*                       However, the handling of network receive & transmit packets :
*
*                       (1) SHOULD be interleaved so that for every few packet(s) received & handled,
*                           several packet(s) should be transmitted; & vice versa.
*
*                       (2) SHOULD NOT exclusively handle receive nor transmit packets, even for a
*                           short period of time, but especially for a prolonged period of time.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                           (B) Decrement the number of available network receive packets queued to a
*                               network interface for each received packet processed.
*
*                       (2) Certain network connections MUST periodically suspend network transmit(s)
*                           to handle network interface(s)' receive packet(s) :
*
*                           (A) Suspend network connection transmit(s) if any receive packets are
*                               available on a network interface.
*
*                           (B) Signal or timeout network connection transmit suspend(s) to restart
*                               transmit(s).
*
*                   See also 'net_if.c  NetIF_RxPkt()  Note #1'.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void  NetIF_RxHandlerLoadBal (NET_IF  *p_if)
{
    NetIF_RxPktDec(p_if);                                        /* Dec net IF's nbr q'd rx pkts avail (see Note #1b1B). */
    NetIF_TxSuspendSignal(p_if);                                 /* Signal net tx suspend              (see Note #1b2B). */
}
#endif


/*
*********************************************************************************************************
*                                             NetIF_RxPkt()
*
* Description : (1) Receive data packets from devices & demultiplex to network interface layer :
*
*                   (a) Update network interface's link status                          See Note #2
*                   (b) Receive packet from device :
*                       (1) Get receive packet from device
*                       (2) Get receive packet network buffer
*                   (c) Demultiplex receive packet to specific network interface
*
*
* Argument(s) : p_if        Pointer to network interface that received a packet.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface packet successfully
*                                                                   received & processed.
*
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                               NET_DEV_ERR_RX                  Network device receive error.
*
*                                                               --- RETURNED BY NetIF_RxPktDiscard() : ---
*                               NET_ERR_RX                      Receive error; packet discarded.
*
*                                                               ------- RETURNED BY NetBuf_Get() : -------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum
*                                                                   buffer size available.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*                               NET_BUF_ERR_INVALID_LEN         Invalid buffer length.
*
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               ----- RETURNED BY 'p_if_api->Rx()' : ------
*                                                               See specific network interface(s) 'Rx()'
*                                                                   for additional return error codes.
*
* Return(s)   : Size of received packet, if NO error(s).
*
*               0,                       otherwise.
*
* Caller(s)   : NetIF_RxHandler().
*
* Note(s)     : (2) If a network interface receives a packet, its physical link must be 'UP' & the
*                   interface's physical link state is set accordingly.
*
*                   (a) An attempt to check for link state is made after an interface has been started.
*                       However, many physical layer devices, such as Ethernet physical layers require
*                       several seconds for Auto-Negotiation to complete before the link becomes
*                       established.  Thus the interface link flag is not updated until the link state
*                       timer expires & one or more attempts to check for link state have been completed.
*
*               (3) When network buffer is demultiplexed to network IF receive, the buffer's reference
*                   counter is NOT incremented since the packet interface layer does NOT maintain a
*                   reference to the buffer.
*
*               (4) Network buffer already freed by higher layer.
*********************************************************************************************************
*/

static  NET_BUF_SIZE  NetIF_RxPkt (NET_IF   *p_if,
                                   NET_ERR  *p_err)
{
    NET_IF_API    *p_if_api;
    NET_DEV_API   *pdev_api;
    CPU_INT08U    *p_buf_data;
    NET_BUF       *p_buf;
    NET_BUF_HDR   *p_buf_hdr;
    NET_BUF_SIZE   size;
    NET_BUF_SIZE   ix_rx;
    NET_BUF_SIZE   ix_offset;
    NET_ERR        err;

                                                                    /* --------------- RX PKT FROM DEV ---------------- */
    pdev_api = (NET_DEV_API *)p_if->Dev_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (pdev_api == (NET_DEV_API *)0) {
       *p_err =  NET_IF_ERR_INVALID_CFG;
        return (0u);
    }
    if (pdev_api->Rx == (void (*)(NET_IF      *,
                                  CPU_INT08U **,
                                  CPU_INT16U  *,
                                  NET_ERR     *))0) {
       *p_err =  NET_ERR_FAULT_NULL_FNCT;
        return (0u);
    }
#endif

    pdev_api->Rx(p_if, &p_buf_data, &size, &err);                     /* Get rx'd buf data area from dev.                 */
    if ( err != NET_DEV_ERR_NONE) {
       *p_err  = NET_DEV_ERR_RX;
        return (0u);
    }

                                                                    /* Get net buf for rx'd buf data area.              */
    ix_rx = NET_IF_IX_RX;
    p_buf = NetBuf_Get(p_if->Nbr,
                       NET_TRANSACTION_RX,
                       size,
                       ix_rx,
                       &ix_offset,
                       NET_BUF_FLAG_NONE,
                       p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         NetBuf_FreeBufDataAreaRx(p_if->Nbr, p_buf_data);
         return (0u);
    }

    p_buf->DataPtr = p_buf_data;
    ix_rx         += ix_offset;


                                                                    /* ----------------- DEMUX RX PKT ----------------- */
    p_buf_hdr                    = &p_buf->Hdr;
    p_buf_hdr->TotLen            = (NET_BUF_SIZE)size;               /* Set pkt size as buf tot len & data len.          */
    p_buf_hdr->DataLen           = (NET_BUF_SIZE)p_buf_hdr->TotLen;
    p_buf_hdr->IF_HdrIx          = (CPU_INT16U  )ix_rx;
    p_buf_hdr->ProtocolHdrType   =  NET_PROTOCOL_TYPE_IF;
    p_buf_hdr->ProtocolHdrTypeIF =  NET_PROTOCOL_TYPE_IF;
    DEF_BIT_SET(p_buf_hdr->Flags,   NET_BUF_FLAG_RX_REMOTE);

                                                                    /* See Note #3.                                     */

    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
        NetIF_RxPktDiscard(p_buf, p_err);
        return (0u);
    }
    if (p_if_api->Rx == (void (*)(NET_IF  *,
                                 NET_BUF *,
                                 NET_ERR *))0) {
        NetIF_RxPktDiscard(p_buf, p_err);
        return (0u);
    }
#endif


    p_if_api->Rx(p_if, p_buf, p_err);                                   /* Demux rx pkt to appropriate net IF rx handler.   */
    if (*p_err != NET_IF_ERR_NONE) {
                                                                    /* See Note #4.                                     */
         return (0u);
    }


                                                                    /* -------------- RTN RX'D DATA SIZE -------------- */
   *p_err =  NET_IF_ERR_NONE;

    return (size);
}


/*
*********************************************************************************************************
*                                           NetIF_RxPktDec()
*
* Description : Decrement number receive packet(s) queued & available for a network interface.
*
* Argument(s) : p_if    Pointer to network interface that received a packet.
*               ----    Argument validated in NetIF_RxHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxHandlerLoadBal().
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets on each
*                   network interface SHOULD be handled in an APPROXIMATELY balanced ratio.
*
*                   (a) Network task priorities & lock mechanisms partially maintain a balanced ratio
*                       between network receive versus transmit packet handling.
*
*                       However, the handling of network receive & transmit packets :
*
*                       (1) SHOULD be interleaved so that for every few packet(s) received & handled,
*                           several packet(s) should be transmitted; & vice versa.
*
*                       (2) SHOULD NOT exclusively handle receive nor transmit packets, even for a
*                           short period of time, but especially for a prolonged period of time.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                           (B) Decrement the number of available network receive packets queued to a
*                               network interface for each received packet processed.
*
*                       (2) Certain network connections MUST periodically suspend network transmit(s)
*                           to handle network interface(s)' receive packet(s) :
*
*                           (A) Suspend network connection transmit(s) if any receive packets are
*                               available on a network interface.
*
*                           (B) Signal or timeout network connection transmit suspend(s) to restart
*                               transmit(s).
*
*                   See also 'NetIF_RxPktInc()         Note #1',
*                            'NetIF_RxPktIsAvail()     Note #1',
*                            'NetIF_TxSuspend()        Note #1',
*                          & 'NetIF_TxSuspendSignal()  Note #1'.
*
*               (2) Network interfaces' 'RxPktCtr' variables MUST ALWAYS be accessed exclusively in
*                   critical sections.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void  NetIF_RxPktDec (NET_IF  *p_if)
{
    NET_ERR  err;


    NetStat_CtrDec(&p_if->RxPktCtr, &err);                       /* Dec net IF's nbr q'd rx pkts avail (see Note #1b1B). */
}
#endif


/*
*********************************************************************************************************
*                                         NetIF_RxPktDiscard()
*
* Description : On any IF receive error(s), discard packet & buffer.
*
* Argument(s) : p_buf        Pointer to network buffer.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void  NetIF_RxPktDiscard (NET_BUF  *p_buf,
                                  NET_ERR  *p_err)
{
#if (NET_CTR_CFG_ERR_EN         == DEF_ENABLED)
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_ERR       err;
#endif
    NET_BUF_HDR  *p_buf_hdr;
    CPU_BOOLEAN   valid;
    NET_IF_NBR    if_nbr;
    NET_BUF_QTY   i;
#endif
    NET_BUF_QTY   nbr_freed;


#if (NET_CTR_CFG_ERR_EN         == DEF_ENABLED)
    if (p_buf != (NET_BUF *)0) {
        p_buf_hdr = &p_buf->Hdr;
        if_nbr   =  p_buf_hdr->IF_Nbr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        valid    =  NetIF_IsValidHandler(if_nbr, &err);
#else
        valid    =  DEF_YES;
#endif
    } else {
        valid    =  DEF_NO;
    }
#endif


    nbr_freed = NetBuf_FreeBufList((NET_BUF *)p_buf,
                                   (NET_CTR *)0);


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    for (i = 0u; i < nbr_freed; i++) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.RxPktDisCtr);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].RxPktDisCtr);
        }
    }
#else
   (void)&nbr_freed;                                            /* Prevent 'variable unused' compiler warning.          */
#endif


   *p_err = NET_ERR_RX;
}
#endif


/*
*********************************************************************************************************
*                                          NetIF_RxQ_SizeCfg()
*
* Description : Configure the maximum number of receive signals that may be concurrently queued to the
*                   network interface receive queue.
*
* Argument(s) : size    Configured size of network interface receive queue :
*
*                           NET_IF_Q_SIZE_MAX                   Maximum configurable queue size
*                                                                  (i.e. NO limit on queue size).
*                           In number of receive signals,       otherwise.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_InitTaskObj().
*
*               This function is a board-support package function & SHOULD be called only by
*               appropriate product function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_RxQ_SizeCfg (NET_IF_Q_SIZE  size)
{
    NetIF_RxQ_SizeCfgd    = size;
    NetIF_RxQ_SizeCfgdRem = size;

   (void)&NetIF_RxQ_SizeCfgd;
}


/*
*********************************************************************************************************
*                                         NetIF_TxDeallocTask()
*
* Description : OS-dependent shell task to schedule & run Network Interface Transmit Deallocation Task
*                   handler.
*
*               (1) Shell task's primary purpose is to schedule & run NetIF_TxDeallocTaskHandler()
*                   forever; (i.e. shell task should NEVER exit).
*
*
* Argument(s) : p_data      Pointer to task initialization data (required by uC/OS-III).
*
* Return(s)   : none.
*
* Created by  : NetIF_InitTaskObj().
*
* Note(s)     : (2) To prevent deadlocking any lower priority task(s), network tasks SHOULD delay for a
*                   (brief) time after any network task handlers exit.
*********************************************************************************************************
*/

static  void  NetIF_TxDeallocTask (void  *p_data)
{
   (void)&p_data;                                               /* Prevent 'variable unused' compiler warning.          */

    while (DEF_ON) {
        NetIF_TxDeallocTaskHandler();
        KAL_Dly(1u);
    }
}


/*
*********************************************************************************************************
*                                     NetIF_TxDeallocTaskHandler()
*
* Description : (1) Deallocate network buffers & data areas :
*
*                   (a) Wait for   transmitted network buffer data areas deallocated from network device(s)
*                   (b) Acquire network lock
*                   (c) Deallocate transmitted network buffer(s)
*                   (d) Release network lock
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxDeallocTask().
*
*               This function is a network protocol suite to operating system (OS) function & SHOULD be
*               called only by appropriate network-operating system port function(s).
*
* Note(s)     : (2) NetIF_TxDeallocTaskHandler() blocked until network initialization completes.
*
*               (3) NetIF_TxDeallocTaskHandler() blocks ALL other network protocol tasks by pending on &
*                   acquiring the global network lock (see 'net.h  Note #3').
*********************************************************************************************************
*/

static  void  NetIF_TxDeallocTaskHandler (void)
{
    CPU_INT08U  *p_buf_data;
    NET_ERR      err;


    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        Net_InitCompWait(&err);                                 /* ... wait on net init (see Note #2).                  */
        if (err != NET_ERR_NONE) {
            return;
        }
    }


    while (DEF_ON) {
                                                                /* ---------- WAIT FOR TX'D NET BUF DATA AREA --------- */
        do {
            p_buf_data = NetIF_TxDeallocTaskWait(&err);
        } while (err!= NET_IF_ERR_NONE);

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #3.                                         */
        Net_GlobalLockAcquire((void *)&NetIF_TxDeallocTaskHandler, &err);
        if (err != NET_ERR_NONE) {
            continue;
        }

                                                                /* ---------------- DEALLOC TX NET BUF ---------------- */
        NetIF_TxPktListDealloc(p_buf_data);

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
        Net_GlobalLockRelease();
    }
}


/*
*********************************************************************************************************
*                                       NetIF_TxDeallocTaskWait()
*
* Description : Wait on network interface transmit deallocation queue for network buffer transmit data
*                   areas deallocated from device(s).
*
* Argument(s) : p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                         Transmit buffer data area  deallocated
*                                                                            from device.
*                               NET_IF_ERR_TX_DEALLC_Q_EMPTY            Network interface transmit deallocation
*                                                                           queue empty.
*                               NET_IF_ERR_TX_DEALLC_Q_SIGNAL_FAULT     Network interface transmit deallocation
*                                                                           queue signal fault.
*
* Return(s)   : Pointer to deallocated transmit buffer data area, if NO error(s).
*
*               Pointer to NULL,                                  otherwise.
*
* Caller(s)   : NetIF_TxDeallocTaskHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) If timeouts NOT desired, wait on network interface transmit deallocation queue
*                       until signaled (i.e. do NOT exit).
*
*                   (b) If timeout      desired, return NET_IF_ERR_TX_DEALLC_Q_EMPTY error on transmit
*                       deallocation queue timeout.  Implement timeout with OS-dependent functionality.
*********************************************************************************************************
*/

static  CPU_INT08U  *NetIF_TxDeallocTaskWait (NET_ERR  *p_err)
{
    void         *p_tx_q;
    CPU_INT08U   *p_buf_data;
    KAL_ERR       err_kal;

                                                                /* Wait for deallocated transmit buffer data area ...   */
                                                                /* ... preferably without timeout (see Note #1a).       */
    p_tx_q = KAL_QPend(NetIF_TxQ_Handle, KAL_OPT_PEND_NONE, 0, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             p_buf_data = (CPU_INT08U *)p_tx_q;                  /* Decode pointer to transmit buffer data area.         */
            *p_err      =  NET_IF_ERR_NONE;
             break;


        case KAL_ERR_TIMEOUT:
             p_buf_data = (CPU_INT08U *)0;
            *p_err      =  NET_IF_ERR_TX_DEALLC_Q_EMPTY;         /* See Note #1b.                                        */
             break;


        case KAL_ERR_ISR:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
             p_buf_data = (CPU_INT08U *)0;
            *p_err      =  NET_IF_ERR_TX_DEALLC_Q_SIGNAL_FAULT;
             break;
    }

    return (p_buf_data);
}


/*
*********************************************************************************************************
*                                      NetIF_TxDeallocQ_SizeCfg()
*
* Description : Configure the maximum number of transmit data areas that may be concurrently queued to
*                   the network interface transmit deallocation queue.
*
* Argument(s) : size    Configured size of network interface transmit deallocation queue :
*
*                           NET_IF_Q_SIZE_MAX                   Maximum configurable queue size
*                                                                  (i.e. NO limit on queue size).
*                           In number of transmit data areas,   otherwise.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_InitTaskObj().
*
*               This function is a board-support package function & SHOULD be called only by
*               appropriate product function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_TxDeallocQ_SizeCfg (NET_IF_Q_SIZE  size)
{
    NetIF_TxDeallocQ_SizeCfgd    = size;
    NetIF_TxDeallocQ_SizeCfgdRem = size;

   (void)&NetIF_TxDeallocQ_SizeCfgd;
}


/*
*********************************************************************************************************
*                                           NetIF_TxHandler()
*
* Description : (1) Transmit data packets to  network interface(s)/device(s) :
*
*                   (a) Get transmit packet's network interface
*                   (b) Check network interface's/device's link state
*                   (c) Transmit packet via   network interface
*                   (d) Update transmit statistics
*
*
* Argument(s) : p_buf        Pointer to network buffer data packet to transmit.
*               ----        Argument checked in NetIF_Tx().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Packet successfully transmitted (or queued
*                                                                   for later transmission).
*                               NET_ERR_IF_LINK_DOWN            Network interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*                               NET_ERR_TX                      Network interface/device transmit error.
*
*                                                               ---- RETURNED BY NetIF_Loopback_Tx() : ----
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Tx().
*
* Note(s)     : (2) Network buffer already freed by lower layer; only increment error counters.
*********************************************************************************************************
*/

static  void  NetIF_TxHandler (NET_BUF  *p_buf,
                               NET_ERR  *p_err)
{
    NET_BUF_HDR   *p_buf_hdr;
    NET_IF        *p_if_tx;
    NET_IF_NBR     if_nbr;
    NET_IF_NBR     if_nbr_tx;
    NET_BUF_SIZE   size;
    NET_ERR        err;


                                                                /* ------------ VALIDATE TX PKT's NET IF's ------------ */
    p_buf_hdr  = &p_buf->Hdr;
    if_nbr    =  p_buf_hdr->IF_Nbr;
    if_nbr_tx =  p_buf_hdr->IF_NbrTx;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (if_nbr != if_nbr_tx) {                                  /* If net IF to tx to NOT same as tx'ing net IF & ..    */
        if (if_nbr_tx != NET_IF_NBR_LOOPBACK) {                 /* .. net IF to tx to NOT loopback IF,            ..    */
            NetIF_TxPktDiscard(p_buf, DEF_YES, &err);           /* .. discard tx pkt & rtn err.                         */
           *p_err = NET_IF_ERR_INVALID_IF;
            return;
        }
    }
#endif


                                                                /* --------------- GET TX PKT's NET IF ---------------- */
    p_if_tx = NetIF_Get(if_nbr_tx, p_err);                       /* Get net IF to tx to.                                 */
    if (*p_err != NET_IF_ERR_NONE) {
         NetIF_TxPktDiscard(p_buf, DEF_YES, &err);
         return;
    }


                                                                /* -------------- CHK NET IF LINK STATE --------------- */
    switch (p_if_tx->Link) {                                    /* Chk link state of net IF to tx to.                   */
        case NET_IF_LINK_UP:
             break;


        case NET_IF_LINK_DOWN:
        default:
             NetIF_TxPktDiscard(p_buf, DEF_YES, &err);
            *p_err = NET_ERR_IF_LINK_DOWN;
             return;
    }


                                                                /* ------------------ TX NET IF PKT ------------------- */
    switch (if_nbr_tx) {
        case NET_IF_NBR_LOOPBACK:
#ifdef NET_IF_LOOPBACK_MODULE_EN
             size = NetIF_Loopback_Tx(p_if_tx,
                                      p_buf,
                                      p_err);
#else
            *p_err = NET_ERR_IF_LOOPBACK_DIS;
#endif
             break;


        default:
             size = NetIF_TxPkt(p_if_tx,
                                p_buf,
                                p_err);
             break;
    }


                                                                /* ----------------- UPDATE TX STATS ------------------ */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.TxPktCtr);
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.IF[if_nbr].TxNbrPktCtr);
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.IF[if_nbr].TxNbrPktCtrProcessed);
             NET_CTR_STAT_ADD(Net_StatCtrs.IFs.IF[if_nbr].TxNbrOctets, size);

            (void)&if_nbr;                                      /* Prevent possible 'variable unused' warnings.         */
            (void)&size;
             break;


        case NET_IF_ERR_TX_ADDR_PEND:                           /* Tx pending on hw addr; will tx when addr resolved.   */
            *p_err = NET_IF_ERR_NONE;
             return;


        case NET_ERR_IF_LOOPBACK_DIS:
             NetIF_TxPktDiscard(p_buf, DEF_YES, &err);
                                                                /* Rtn err from NetIF_Loopback_Tx().                    */
             return;


        case NET_ERR_INVALID_TRANSACTION:
        case NET_ERR_INVALID_PROTOCOL:
        case NET_ERR_TX:
        case NET_IF_ERR_INVALID_IF:
        case NET_IF_ERR_INVALID_CFG:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_IF_ERR_RX_Q_FULL:
        case NET_IF_ERR_RX_Q_SIGNAL_FAULT:
        case NET_DEV_ERR_TX_RDY_SIGNAL_TIMEOUT:
        case NET_DEV_ERR_TX_RDY_SIGNAL_FAULT:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_BUF_ERR_NONE_AVAIL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_SIZE:
        case NET_BUF_ERR_INVALID_LEN:
        case NET_BUF_ERR_INVALID_IX:
        default:
                                                                /* See Note #2.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.TxPktDisCtr);
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].TxPktDisCtr);
            *p_err = NET_ERR_TX;
             return;
    }
}



/*
*********************************************************************************************************
*                                             NetIF_TxPkt()
*
* Description : (1) Transmit data packets from network interface layer to network device(s) :
*
*                   (a) Validate   transmit data packet
*                   (b) Prepare    transmit data packet(s) via network interface layer
*                   (c) Wait for   transmit ready signal from  network device
*                   (d) Prepare    transmit data packet(s) via network device driver :
*                       (1) Set    transmit data packet(s)' transmit lock
*                       (2) Insert transmit data packet(s) into network interface transmit list
*                   (e)            Transmit data packet(s) via network device driver
*
*
* Argument(s) : p_if        Pointer to network interface to transmit a packet.
*               ----        Argument validated in NetIF_TxHandler().
*
*               p_buf       Pointer to network buffer data packet to transmit.
*               -----       Argument checked   in NetIF_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function
*                               (see Note #4) :
*
*                               NET_IF_ERR_NONE                     Packet successfully transmitted.
*                               NET_IF_ERR_TX_ADDR_PEND             Packet successfully prepared & queued
*                                                                       for later transmission.
*
*                               NET_ERR_TX                          Network interface/device transmit error
*                                                                       (see Note #4b).
*
*                                                                   --- RETURNED BY NetIF_TxPktValidate() : ---
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL API configuration.
*                               NET_ERR_INVALID_PROTOCOL            Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE            Invalid network buffer   type.
*                               NET_BUF_ERR_INVALID_IX              Invalid/insufficient buffer index.
*
*                                                                   --- RETURNED BY NetIF_Dev_TxRdyWait() : ---
*                               NET_DEV_ERR_TX_RDY_SIGNAL_TIMEOUT   Device transmit ready signal NOT received
*                                                                       within timeout.
*                               NET_DEV_ERR_TX_RDY_SIGNAL_FAULT     Device transmit ready signal fault.
*
*                                                                   ----- RETURNED BY 'pdev_api->Tx()' : ------
*                                                                   See specific network device(s) 'Tx()'
*                                                                       for additional return error codes.
*
* Return(s)   : Size of transmitted packet, if NO error(s).
*
*               0,                          otherwise.
*
* Caller(s)   : NetIF_TxHandler().
*
* Note(s)     : (2) On ANY error(s), network resources MUST be appropriately freed :
*
*                   (a) After  transmit packet buffer queued to Network Interface Transmit List,
*                       remove transmit packet buffer from      Network Interface Transmit List.
*
*               (3) Network buffer already freed by lower layer.
*
*               (4) Error codes from network interface/device driver handler functions returned as is.
*********************************************************************************************************
*/

static  NET_BUF_SIZE  NetIF_TxPkt (NET_IF   *p_if,
                                   NET_BUF  *p_buf,
                                   NET_ERR  *p_err)
{
    NET_IF_API    *p_if_api;
    NET_DEV_API   *pdev_api;
    NET_BUF_HDR   *p_buf_hdr;
    CPU_INT08U    *p_data;
    NET_BUF_SIZE   size;
    NET_ERR        err;



                                                                /* ---------------- VALIDATE IF TX PKT ---------------- */
    p_buf_hdr = &p_buf->Hdr;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    NetIF_TxPktValidate(p_if,
                        p_buf_hdr,
                        p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_IF_ERR_INVALID_CFG:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        case NET_ERR_INVALID_PROTOCOL:
        default:
             NetIF_TxPktDiscard(p_buf, DEF_NO, &err);
                                                                /* Rtn err from NetIF_TxPktValidate() [see Note #4].    */
             return (0u);
    }
#endif


                                                                /* ------------ PREPARE TX PKT VIA NET IF ------------- */
    p_if_api = (NET_IF_API *)p_if->IF_API;
    p_if_api->Tx(p_if, p_buf, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_IF_ERR_TX_ADDR_PEND:                           /* Tx pending on hw addr; will tx when addr resolved.   */
             return (0u);


        case NET_ERR_TX:
        default:
                                                                /* See Note #3.                                         */
            *p_err =  NET_ERR_TX;
             return (0u);
    }


                                                                /* ------------- WAIT FOR DEV TX RDY SIGNAL ----------- */
    NetIF_DevTxRdyWait(p_if, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_IF_ERR_TX_RDY_SIGNAL_TIMEOUT:
        case NET_IF_ERR_TX_RDY_SIGNAL_FAULT:
        default:
             NetIF_TxPktDiscard(p_buf, DEF_NO, &err);
                                                                /* Rtn err from NetIF_Dev_TxRdyWait() [see Note #4].    */
             return (0u);
    }


                                                                /* ------------ PREPARE TX PKT VIA NET DEV ------------ */
    p_data = &p_buf->DataPtr[p_buf_hdr->IF_HdrIx];
    size   =  p_buf_hdr->TotLen;
    DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_LOCK);         /* Protect tx pkt buf from concurrent access by dev hw. */
    NetIF_TxPktListInsert(p_buf);                                /* Insert  tx pkt buf into tx list.                     */


                                                                /* ---------------- TX PKT VIA NET DEV ---------------- */
    pdev_api = (NET_DEV_API *)p_if->Dev_API;
    pdev_api->Tx(p_if, p_data, size, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         NetIF_TxPktListRemove(p_buf);                           /* See Note #2a.                                        */
         NetIF_TxPktDiscard(p_buf, DEF_NO, &err);
                                                                /* Rtn err from 'pdev_api->Tx()' [see Note #4].         */
         return (0u);
    }


                                                                /* ---------------- RTN TX'D DATA SIZE ---------------- */
   *p_err =  NET_IF_ERR_NONE;

    return (size);
}


/*
*********************************************************************************************************
*                                         NetIF_TxPktValidate()
*
* Description : (1) Validate network interface transmit packet :
*
*                   (a) Validate network interface :
*                       (1) Interface/device transmit API
*
*                   (b) Validate the following transmit packet parameters :
*
*                       (1) Network interface
*                       (2) Buffer type
*                       (3) Supported protocols :
*                           (A) Ethernet
*                           (B) ARP
*                           (C) IPv4
*
*                       (4) Buffer protocol index
*
*
* Argument(s) : p_if        Pointer to network interface.
*               ----        Argument validated in NetIF_TxPkt().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIF_TxPkt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Transmit packet validated.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer   type.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*
*                                                               - RETURNED BY NetIF_IsValidHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt().
*
* Note(s)     : (2) Network buffer's network interface number was previously validated via NetIF_Get().
*                   The network interface number does NOT need to be re-validated but is shown for
*                   completeness.
*********************************************************************************************************
*/

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void  NetIF_TxPktValidate (NET_IF       *p_if,
                                   NET_BUF_HDR  *p_buf_hdr,
                                   NET_ERR      *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_IF_NBR    if_nbr;
    CPU_INT16U    ix;
#endif
    NET_IF_API   *p_if_api;
    NET_DEV_API  *pdev_api;


                                                                /* ----------------- VALIDATE NET IF ------------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if_nbr = p_buf_hdr->IF_Nbr;
#if 0                                                           /* See Note #2.                                         */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif
   (void)&if_nbr;                                               /* Prevent possible 'variable unused' warning.          */
#endif


                                                                /* ------------- VALIDATE NET IF/DEV API -------------- */
    p_if_api = (NET_IF_API *)p_if->IF_API;
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->Tx == (void (*)(NET_IF  *,
                                 NET_BUF *,
                                 NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }

    pdev_api = (NET_DEV_API *)p_if->Dev_API;
    if (pdev_api == (NET_DEV_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (pdev_api->Tx == (void (*)(NET_IF     *,
                                  CPU_INT08U *,
                                  CPU_INT16U  ,
                                  NET_ERR    *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* -------------- VALIDATE NET BUF TYPE --------------- */
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        case NET_BUF_TYPE_BUF:
        case NET_BUF_TYPE_RX_LARGE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }


                                                                /* ---------------- VALIDATE PROTOCOL ----------------- */
    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_IF_FRAME:
        case NET_PROTOCOL_TYPE_IF_ETHER:
             ix = p_buf_hdr->IF_HdrIx;
             break;

#ifdef  NET_ARP_MODULE_EN
        case NET_PROTOCOL_TYPE_ARP:
             ix = p_buf_hdr->ARP_MsgIx;
             break;
#endif

#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             ix = p_buf_hdr->IP_HdrIx;
             break;
#endif

        case NET_PROTOCOL_TYPE_IP_V6:
             ix = p_buf_hdr->IP_HdrIx;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

#else
   (void)&p_buf_hdr;                                             /* Prevent 'variable unused' compiler warning.          */
#endif


   *p_err = NET_IF_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       NetIF_TxPktListDealloc()
*
* Description : (1) Deallocate transmitted packet's network buffer & data area :
*
*                   (a) Search Network Interface Transmit List for transmitted network buffer
*                   (b) Deallocate transmit packet buffer :
*                       (1) Remove transmit packet buffer from network interface transmit list
*                       (2) Free network buffer
*
*
* Argument(s) : p_buf_data   Pointer to a network packet buffer's transmitted data area (see Note #2b1).
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxDeallocTaskHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_TxPktListDealloc (CPU_INT08U  *p_buf_data)
{
    NET_BUF  *p_buf;


    p_buf = NetIF_TxPktListSrch(p_buf_data);                      /* Srch tx list for tx'd pkt buf.                       */
    if (p_buf != (NET_BUF *)0) {                                 /* If   tx'd  pkt buf found,       ...                  */
        NetIF_TxPktListRemove(p_buf);                            /* ... remove pkt buf from tx list ...                  */
        NetIF_TxPktFree(p_buf);                                  /* ... & free net buf.                                  */
        NET_CTR_STAT_INC(Net_StatCtrs.IFs.TxPktDeallocCtr);

    } else {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.TxPktDeallocCtr);
    }
}


/*
*********************************************************************************************************
*                                         NetIF_TxPktListSrch()
*
* Description : Search Network Interface Transmit List for transmitted network buffer.
*
*               (1) Network buffers whose data areas have been transmitted via network interface(s)/device(s)
*                   are queued to await acknowledgement of transmission complete & subsequent deallocation.
*
*                   (a) Transmitted network buffers are linked to form a Network Interface Transmit List.
*
*                       In the diagram below, ... :
*
*                       (1) The horizontal row represents the list of transmitted network buffers.
*
*                       (2) (A) 'NetIF_TxListHead' points to the head of the Network Interface Transmit List;
*                           (B) 'NetIF_TxListTail' points to the tail of the Network Interface Transmit List.
*
*                       (3) Network buffers' 'PrevTxListPtr' & 'NextTxListPtr' doubly-link each network buffer
*                           to form the Network Interface Transmit List.
*
*                   (b) (1) (A) For each network buffer data area that has been transmitted, all network buffers
*                               are searched in order to find (& deallocate) the corresponding network buffer.
*
*                           (B) The network buffer corresponding to the transmitted data area has a network
*                               interface index into its data area that points to the address of the transmitted
*                               data area.
*
*                       (2) To expedite faster network buffer searches :
*
*                           (A) (1) Network buffers are added             at the tail of the Network Interface
*                                       Transmit List;
*                               (2) Network buffers are searched starting at the head of the Network Interface
*                                       Transmit List.
*
*                           (B) As network buffers are added into the list, older network buffers migrate to the
*                               head of the Network Interface Transmit List.  Once a network buffer's data area
*                               has been transmitted, the network buffer is removed from the Network Interface
*                               Transmit List & deallocated.
*
*
*                                      |                                               |
*                                      |<------ Network Interface Transmit List ------>|
*                                      |               (see Note #1a1)                 |
*
*                             Transmitted network buffers                      New transmit
*                               awaiting acknowledgement                     network buffers
*                                   of transmission                          inserted at tail
*                                  (see Note #1b2A2)                         (see Note #1b2A1)
*
*                                         |              NextTxListPtr              |
*                                         |             (see Note #1a3)             |
*                                         v                    |                    v
*                                                              |
*                   Head of            -------       -------   v   -------       -------       (see Note #1a2B)
*              Network Interface  ---->|     |------>|     |------>|     |------>|     |
*                Transmit List         |     |       |     |       |     |       |     |            Tail of
*                                      |     |<------|     |<------|     |<------|     |<----  Network Interface
*              (see Note #1a2A)        |     |       |     |   ^   |     |       |     |         Transmit List
*                                      |     |       |     |   |   |     |       |     |
*                                      -------       -------   |   -------       -------
*                                                              |
*                                                        PrevTxListPtr
*                                                       (see Note #1a3)
*
*
* Argument(s) : p_buf_data   Pointer to a network packet buffer's transmitted data area (see Note #2).
*
* Return(s)   : Pointer to transmitted data area's network buffer, if found.
*
*               Pointer to NULL,                                   otherwise.
*
* Caller(s)   : NetIF_TxPktListDealloc().
*
* Note(s)     : (2) The network buffer corresponding to the transmitted data area has a network interface
*                   index into its data area that points to the address of the transmitted data area.
*
*                   See also Note #1b1.
*********************************************************************************************************
*/

static  NET_BUF  *NetIF_TxPktListSrch (CPU_INT08U  *p_buf_data)
{
    NET_BUF      *p_buf;
    NET_BUF_HDR  *p_buf_hdr;
    CPU_INT08U   *p_buf_data_if;
    CPU_BOOLEAN   found;


    p_buf  = NetIF_TxListHead;                                       /* Start @ Net IF Tx List head (see Note #1b2A2).   */
    found = DEF_NO;

    while ((p_buf  != (NET_BUF *)0) &&                               /* Srch    Net IF Tx List ...                       */
           (found ==  DEF_NO)) {                                    /* ... until tx'd pkt buf found.                    */
        p_buf_hdr     = &p_buf->Hdr;
        p_buf_data_if = &p_buf->DataPtr[p_buf_hdr->IF_HdrIx];
        found        = (p_buf_data_if == p_buf_data) ? DEF_YES : DEF_NO;  /* Cmp tx data area ptrs (see Note #1b1B).      */

        if (found != DEF_YES) {                                     /* If NOT found, ...                                */
            p_buf = p_buf_hdr->NextTxListPtr;                         /* ... adv to next tx pkt buf.                      */
        }
    }

    return (p_buf);
}


/*
*********************************************************************************************************
*                                       NetIF_TxPktListInsert()
*
* Description : Insert a network packet buffer into the Network Interface Transmit List.
*
* Argument(s) : p_buf       Pointer to a network buffer.
*               -----       Argument checked in NetIF_Tx().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt().
*
* Note(s)     : (1) Some buffer controls were previously initialized in NetBuf_Get() when the buffer was
*                   allocated.  These buffer controls do NOT need to be re-initialized but are shown for
*                   completeness.
*
*               (2) See 'NetIF_TxPktListSrch()  Note #1b2A1'.
*********************************************************************************************************
*/

static  void  NetIF_TxPktListInsert (NET_BUF  *p_buf)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_HDR  *p_buf_hdr_tail;

                                                                /* ----------------- CFG NET BUF PTRS ----------------- */
    p_buf_hdr                = (NET_BUF_HDR *)&p_buf->Hdr;
    p_buf_hdr->PrevTxListPtr = (NET_BUF     *) NetIF_TxListTail;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #1].                */
    p_buf_hdr->NextTxListPtr = (NET_BUF     *) 0;
#endif

                                                                /* -------- INSERT PKT BUF INTO NET IF TX LIST -------- */
    if (NetIF_TxListTail != (NET_BUF *)0) {                     /* If list NOT empty, insert after tail.                */
        p_buf_hdr_tail                = &NetIF_TxListTail->Hdr;
        p_buf_hdr_tail->NextTxListPtr =  p_buf;
    } else {                                                    /* Else add first pkt buf to list.                      */
        NetIF_TxListHead             =  p_buf;
    }
    NetIF_TxListTail = p_buf;                                    /* Insert pkt buf @ list tail (see Note #2).            */
}


/*
*********************************************************************************************************
*                                        NetIF_TxPktListRemove()
*
* Description : Remove a network packet buffer from the Network Interface Transmit List.
*
* Argument(s) : p_buf       Pointer to a network buffer.
*               -----       Argument checked in NetIF_TxPktListDealloc().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt(),
*               NetIF_TxPktListDealloc().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_TxPktListRemove (NET_BUF  *p_buf)
{
    NET_BUF      *p_buf_list_prev;
    NET_BUF      *p_buf_list_next;
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_HDR  *p_buf_list_prev_hdr;
    NET_BUF_HDR  *p_buf_list_next_hdr;

                                                                /* -------- REMOVE PKT BUF FROM NET IF TX LIST -------- */
    p_buf_hdr       = &p_buf->Hdr;
    p_buf_list_prev =  p_buf_hdr->PrevTxListPtr;
    p_buf_list_next =  p_buf_hdr->NextTxListPtr;

                                                                /* Point prev pkt buf to next pkt buf.                  */
    if (p_buf_list_prev != (NET_BUF *)0) {
        p_buf_list_prev_hdr                = &p_buf_list_prev->Hdr;
        p_buf_list_prev_hdr->NextTxListPtr =  p_buf_list_next;
    } else {
        NetIF_TxListHead                  =  p_buf_list_next;
    }
                                                                /* Point next pkt buf to prev pkt buf.                  */
    if (p_buf_list_next != (NET_BUF *)0) {
        p_buf_list_next_hdr                = &p_buf_list_next->Hdr;
        p_buf_list_next_hdr->PrevTxListPtr =  p_buf_list_prev;
    } else {
        NetIF_TxListTail                  =  p_buf_list_prev;
    }

                                                                /* ----------------- CLR NET BUF PTRS ----------------- */
    p_buf_hdr->PrevTxListPtr = (NET_BUF *)0;
    p_buf_hdr->NextTxListPtr = (NET_BUF *)0;
}


/*
*********************************************************************************************************
*                                           NetIF_TxPktFree()
*
* Description : (1) Free network buffer :
*
*                   (a) Unlock network buffer's transmit lock
*                   (b) Free   network buffer
*
*
* Argument(s) : p_buf       Pointer to network buffer.
*               -----       Argument checked in NetIF_TxPktListDealloc().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPktListDealloc().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_TxPktFree (NET_BUF  *p_buf)
{
    NET_BUF_HDR  *p_buf_hdr;


    p_buf_hdr = &p_buf->Hdr;
    DEF_BIT_CLR(p_buf_hdr->Flags, NET_BUF_FLAG_TX_LOCK);         /* Clr  net buf's tx lock.                              */

   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,                        /* Free net buf.                                        */
                        (NET_CTR *)0);
}


/*
*********************************************************************************************************
*                                         NetIF_TxPktDiscard()
*
* Description : (1) On any IF transmit error(s), discard packet & buffer :
*
*                   (a) Unlock network buffer's transmit lock
*                   (b) Free   network buffer
*
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               inc_ctrs    Indicate whether to increment error counter(s) :
*
*                               DEF_YES                                Increment error counter(s).
*                               DEF_NO                          Do NOT increment error counter(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Tx().
*
* Note(s)     : (2) ONLY the current transmit packet buffer is discarded.
*********************************************************************************************************
*/

static  void  NetIF_TxPktDiscard (NET_BUF      *p_buf,
                                  CPU_BOOLEAN   inc_ctrs,
                                  NET_ERR      *p_err)
{
#if (NET_CTR_CFG_ERR_EN         == DEF_ENABLED)
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_ERR       err;
#endif
    CPU_BOOLEAN   valid;
    NET_IF_NBR    if_nbr;
    NET_BUF_QTY   i;
#endif
    NET_BUF_QTY   nbr_freed;
    NET_BUF_HDR  *p_buf_hdr;


    if (p_buf != (NET_BUF *)0) {
        p_buf_hdr = &p_buf->Hdr;
        DEF_BIT_CLR(p_buf_hdr->Flags, NET_BUF_FLAG_TX_LOCK);     /* Clr net buf's tx lock.                               */

#if (NET_CTR_CFG_ERR_EN         == DEF_ENABLED)
        if_nbr = p_buf_hdr->IF_Nbr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        valid  = NetIF_IsValidHandler(if_nbr, &err);
#else
        valid  = DEF_YES;
#endif
    } else {
        valid  = DEF_NO;
#endif
    }


    nbr_freed = NetBuf_FreeBuf((NET_BUF *)p_buf,                 /* Free net buf (see Note #2).                          */
                               (NET_CTR *)0);


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    if (inc_ctrs == DEF_YES) {
        for (i = 0u; i < nbr_freed; i++) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IFs.TxPktDisCtr);
            if (valid == DEF_YES) {
                NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].TxPktDisCtr);
            }
        }
    }
#else
   (void)&inc_ctrs;                                             /* Prevent 'variable unused' compiler warnings.         */
   (void)&nbr_freed;
#endif


   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                     NetIF_TxSuspendTimeoutInit()
*
* Description : Initialize network interface transmit suspend timeout value.
*
* Argument(s) : p_if    Pointer to network interface.
*               ----    Argument validated in NetIF_ObjInit().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IF_ERR_NONE
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_ObjInit().
*
* Note(s)     : (1) 'NetIF_TxSuspendTimeout_tick' variables MUST ALWAYS be accessed exclusively
*                    in critical sections.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void  NetIF_TxSuspendTimeoutInit (NET_IF      *p_if,
                                          NET_ERR     *p_err)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_if->TxSuspendTimeout_ms = NET_IF_CFG_TX_SUSPEND_TIMEOUT_MS;              /* Set transmit suspend timeout value (in OS ticks).    */
    CPU_CRITICAL_EXIT();

   *p_err = NET_IF_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        NetIF_TxSuspendSignal()
*
* Description : Signal suspended network interface connection(s)' transmit(s).
*
* Argument(s) : p_if        Pointer to network interface to signal suspended transmit(s).
*               ----        Argument validated in NetIF_RxHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxHandlerLoadBal().
*
* Note(s)     : (1) To balance network receive versus transmit packet loads for certain network connection
*                   types (e.g. stream-type connections), network receive & transmit packets SHOULD be
*                   handled in an APPROXIMATELY balanced ratio.
*
*                   (a) Network task priorities & lock mechanisms partially maintain a balanced ratio
*                       between network receive versus transmit packet handling.
*
*                       However, the handling of network receive & transmit packets :
*
*                       (1) SHOULD be interleaved so that for every few packet(s) received & handled,
*                           several packet(s) should be transmitted; & vice versa.
*
*                       (2) SHOULD NOT exclusively handle receive nor transmit packets, even for a
*                           short period of time, but especially for a prolonged period of time.
*
*                   (b) To implement network receive versus transmit load balancing :
*
*                       (1) The availability of network receive packets MUST be managed for each network
*                           interface :
*
*                           (A) Increment the number of available network receive packets queued to a
*                               network interface for each packet received.
*
*                           (B) Decrement the number of available network receive packets queued to a
*                               network interface for each received packet processed.
*
*                       (2) Certain network connections MUST periodically suspend network transmit(s)
*                           to handle network interface(s)' receive packet(s) :
*
*                           (A) Suspend network connection transmit(s) if any receive packets are
*                               available on a network interface.
*
*                           (B) Signal or timeout network connection transmit suspend(s) to restart
*                               transmit(s).
*
*                   See also 'NetIF_RxPktInc()      Note #1',
*                            'NetIF_RxPktDec()      Note #1',
*                            'NetIF_RxPktIsAvail()  Note #1',
*                          & 'NetIF_TxSuspend()     Note #1'.
*
*               (3) Network interfaces' 'TxSuspendCtr' variables may be accessed with only the global
*                   network lock acquired & are NOT required to be accessed exclusively in critical
*                   sections.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void  NetIF_TxSuspendSignal (NET_IF  *p_if)
{
    NET_STAT_CTR  *pstat_ctr;
    NET_CTR        nbr_tx_suspend;
    KAL_ERR        err_kal;
    NET_CTR        i;
    CPU_SR_ALLOC();


    pstat_ctr      = &p_if->TxSuspendCtr;
    CPU_CRITICAL_ENTER();
    nbr_tx_suspend =  pstat_ctr->CurCtr;
    CPU_CRITICAL_EXIT();

    for (i = 0u; i < nbr_tx_suspend; i++) {
                                                                /* Signal ALL suspended net conn tx's (see Note #1b2B). */
        KAL_SemPost(p_if->TxSuspendSignalObj, KAL_OPT_PEND_NONE, &err_kal);
        (void)&err_kal;
    }
}
#endif


/*
*********************************************************************************************************
*                                         NetIF_TxSuspendWait()
*
* Description : Wait on network interface transmit suspend signal.
*
* Argument(s) : p_if    Pointer to network interface.
*               ----    Argument validated in NetIF_TxSuspend().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxSuspend().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network interface transmit suspend waits until :
*
*                   (a) Signaled
*                   (b) Timed out
*                   (c) Any OS fault occurs
*
*               (2) Implement timeout with OS-dependent functionality.
*********************************************************************************************************
*/

#ifdef  NET_LOAD_BAL_MODULE_EN
static  void  NetIF_TxSuspendWait (NET_IF  *p_if)
{
    CPU_INT32U  timeout_ms;
    KAL_ERR     err_kal;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    timeout_ms =  p_if->TxSuspendTimeout_ms;
    CPU_CRITICAL_EXIT();

                                                                /* Wait on network interface transmit suspend signal.   */
    KAL_SemPend(p_if->TxSuspendSignalObj, KAL_OPT_PEND_NONE, timeout_ms, &err_kal);


   (void)&err_kal;                                              /* See Note #1c.                                        */
}
#endif


/*
*********************************************************************************************************
*                                     NetIF_BufPoolCfgValidate()
*
* Description : (1) Validate network interface buffer pool configuration :
*
*                   (a) Validate network interface buffer memory pool types
*                   (b) Validate configured number of network interface buffers
*
*
* Argument(s) : if_nbr      Interface number to initialize network buffer pools.
*               ------      Argument validated in NetIF_BufPoolInit().
*
*               p_dev_cfg   Pointer to network interface's device configuration.
*               ---------   Argument validated in NetIF_BufPoolInit().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface buffer pool configuration
*                                                                   valid.
*                               NET_IF_ERR_INVALID_POOL_TYPE    Invalid network interface buffer pool type.
*                               NET_IF_ERR_INVALID_POOL_ADDR    Invalid network interface buffer pool address.
*                               NET_IF_ERR_INVALID_POOL_SIZE    Invalid network interface buffer pool size.
*                               NET_IF_ERR_INVALID_POOL_QTY     Invalid network interface buffer pool number
*                                                                   of buffers configured.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_BufPoolInit().
*
* Note(s)     : (2) (a) All added network interfaces MUST NOT configure a total number of receive  buffers
*                       greater than the configured network interface receive queue size.
*
*                   (b) All added network interfaces MUST NOT configure a total number of transmit buffers
*                       greater than the configured network interface transmit deallocation queue size.
*
*                       (1) However, since the network loopback interface does NOT deallocate transmit
*                           packets via the network interface transmit deallocation task (see
*                           'net_if_loopback.c  NetIF_Loopback_Tx()  Note #4'); then the network interface
*                           transmit deallocation queue size does NOT need to be adjusted by the network
*                           loopback interface's number of configured transmit buffers.
*
*                       See also 'NetIF_BufPoolInit()  Note #2'.
*********************************************************************************************************
*/

static  void  NetIF_BufPoolCfgValidate (NET_IF_NBR    if_nbr,
                                        NET_DEV_CFG  *p_dev_cfg,
                                        NET_ERR      *p_err)
{
    NET_BUF_QTY  nbr_bufs_tx;


                                                                /* ------------- VALIDATE MEM POOL TYPES -------------- */
    switch (p_dev_cfg->RxBufPoolType) {                         /* Validate rx buf mem pool type.                       */
        case NET_IF_MEM_TYPE_MAIN:
             break;


        case NET_IF_MEM_TYPE_DEDICATED:
             if (p_dev_cfg->MemAddr == (CPU_ADDR)0u) {
                *p_err = NET_IF_ERR_INVALID_POOL_ADDR;
                 return;
             }
             if (p_dev_cfg->MemSize < 1) {
                *p_err = NET_IF_ERR_INVALID_POOL_SIZE;
                 return;
             }
             break;


        case NET_IF_MEM_TYPE_NONE:
        default:
            *p_err = NET_IF_ERR_INVALID_POOL_TYPE;
             return;
    }

    switch (p_dev_cfg->TxBufPoolType) {                          /* Validate tx buf mem pool type.                       */
        case NET_IF_MEM_TYPE_MAIN:
             break;


        case NET_IF_MEM_TYPE_DEDICATED:
             if (p_dev_cfg->MemAddr == (CPU_ADDR)0u) {
                *p_err = NET_IF_ERR_INVALID_POOL_ADDR;
                 return;
             }
             if (p_dev_cfg->MemSize < 1) {
                *p_err = NET_IF_ERR_INVALID_POOL_SIZE;
                 return;
             }
             break;


        case NET_IF_MEM_TYPE_NONE:
        default:
            *p_err = NET_IF_ERR_INVALID_POOL_TYPE;
             return;
    }


                                                                /* ---------------- VALIDATE NBR BUFS ----------------- */
    if (p_dev_cfg->RxBufLargeNbr > NetIF_RxQ_SizeCfgdRem) {     /* Validate nbr rx bufs (see Note #2a).                 */
       *p_err = NET_IF_ERR_INVALID_POOL_QTY;
        return;
    }

    if (if_nbr != NET_IF_NBR_LOOPBACK) {                        /* For all non-loopback IF's (see Note #2b1), ...       */
        nbr_bufs_tx = p_dev_cfg->TxBufLargeNbr +
                      p_dev_cfg->TxBufSmallNbr;
        if (nbr_bufs_tx > NetIF_TxDeallocQ_SizeCfgdRem) {       /* ... validate nbr tx bufs  (see Note #2b).            */
           *p_err = NET_IF_ERR_INVALID_POOL_QTY;
            return;
        }
    }


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetIF_GetDataAlignPtr()
*
* Description : (1) Get aligned pointer into application data buffer :
*
*                   (a) Acquire network lock
*                   (b) Calculate pointer to aligned application data buffer address        See Note #3
*                   (c) Release network lock
*                   (d) Return    pointer to aligned application data buffer address
*                         OR
*                       Null pointer & error code, on failure
*
*
* Argument(s) : if_nbr          Network interface number to get an application buffer's aligned data pointer.
*
*               transaction     Transaction type :
*
*                                   NET_TRANSACTION_RX          Receive  transaction.
*                                   NET_TRANSACTION_TX          Transmit transaction.
*
*               p_data          Pointer to application data buffer to get an aligned pointer into (see also
*                                   Note #3c).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Aligned pointer into application data buffer
*                                                                   successfully returned.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_data' passed a NULL pointer.
*                               NET_IF_ERR_ALIGN_NOT_AVAIL      Alignment between application data buffer &
*                                                                   network interface's network buffer data
*                                                                   area(s) NOT possible (see Note #3b1B).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               -------- RETURNED BY NetIF_Get() : ---------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Pointer to aligned application data buffer address, if available.
*
*               Pointer to         application data buffer address, if aligned address NOT available.
*
*               Pointer to NULL,                                    otherwise.
*
* Caller(s)   : NetIF_GetRxDataAlignPtr(),
*               NetIF_GetTxDataAlignPtr().
*
* Note(s)     : (2) NetIF_GetDataAlignPtr() blocked until network initialization completes.
*
*               (3) (a) The first aligned address in the application data buffer is calculated based on
*                       the following equations :
*
*
*                       (1) Addr         =  Addr  %  Word
*                               Offset                   Size
*
*
*                       (2) Align        =  [ (Word     -  Addr      )  +  ([Ix     + Ix      ]  %  Word    ) ]  %  Word
*                                Offset            Size        Offset          Data     Offset          Size            Size
*
*
*                                           { (A) Addr  +  Align       , if optimal alignment between application data
*                                           {                   Offset      buffer & network interface's network buffer
*                                           {                               data area(s) is  possible (see Note #3b1A)
*                       (3) Addr         =  {
*                               Align       { (B) Addr                 , if optimal alignment between application data
*                                           {                               buffer & network interface's network buffer
*                                           {                               data area(s) NOT possible (see Note #3b1B)
*
*                           where
*
*                               (A) Addr            Application data buffer's address ('p_data')
*
*                               (B) Addr            Non-negative offset from application data buffer's
*                                       Offset          address to previous CPU word-aligned address
*
*                               (C) Align           Non-negative offset from application data buffer's
*                                        Offset         address to first address that is aligned with
*                                                       network interface's network buffer data area(s)
*
*                               (D) Addr            First address in application data buffer that is aligned
*                                       Align           with network interface's network buffer data area(s)
*
*                               (E) Word            CPU's data word size (see 'cpu.h  CPU WORD CONFIGURATION
*                                       Size            Note #1')
*
*                               (F) Ix              Network buffer's base data index (see 'net_buf.h
*                                     Data              NETWORK BUFFER INDEX & SIZE DEFINES  Note #2b')
*
*                               (G) Ix              Network interface's configured network buffer receive/
*                                     Offset            transmit data offset (see 'net_dev_cfg.c  EXAMPLE
*                                                       NETWORK DEVICE CONFIGURATION  Note #5')
*
*
*                   (b) (1) (A) Optimal alignment between application data buffer(s) & network interface's
*                               network buffer data area(s) is NOT guaranteed & is possible if & only if
*                               all of the following conditions are true :
*
*                               (1) Network interface's network buffer data area(s) MUST be aligned to a
*                                   multiple of the CPU's data word size (see 'net_buf.h  NETWORK BUFFER
*                                   INDEX & SIZE DEFINES  Note #2b2').
*
*                           (B) Otherwise, a single, fixed alignment between application data buffer(s) &
*                               network interface's buffer data area(s) is NOT possible.
*
*                       (2) (A) Even when application data buffers & network buffer data areas are aligned
*                               in the best case; optimal alignment is NOT guaranteed for every read/write
*                               of data to/from application data buffers & network buffer data areas.
*
*                               For any single read/write of data to/from application data buffers & network
*                               buffer data areas, optimal alignment occurs if & only if all of the following
*                               conditions are true :
*
*                               (1) Data read/written to/from application data buffer(s) to network buffer
*                                   data area(s) MUST start on addresses with the same relative offset from
*                                   CPU word-aligned addresses.
*
*                                   In other words, the modulus of the specific read/write address in the
*                                   application data buffer with the CPU's data word size MUST be equal to
*                                   the modulus of the specific read/write address in the network buffer
*                                   data area with the CPU's data word size.
*
*                                   This condition MIGHT NOT be satisfied whenever :
*
*                                   (a) Data is read/written to/from fragmented packets
*                                   (b) Data is NOT maximally read/written to/from stream-type packets
*                                           (e.g. TCP data segments)
*                                   (c) Packets include variable number of header options (e.g. IP options)
*
*                           (B) However, even though optimal alignment between application data buffers &
*                               network buffer data areas is NOT guaranteed for every read/write; optimal
*                               alignment SHOULD occur more frequently leading to improved network data
*                               throughput.
*
*                   (c) Since the first aligned address in the application data buffer may be 0 to
*                       (CPU_CFG_DATA_SIZE - 1) octets after the application data buffer's starting
*                       address, the application data buffer SHOULD allocate & reserve an additional
*                       (CPU_CFG_DATA_SIZE - 1) number of octets.
*
*                       However, the application data buffer's effective, useable size is still limited
*                       to its original declared size (before reserving additional octets) & SHOULD NOT
*                       be increased by the additional, reserved octets.
*********************************************************************************************************
*/

static  void  *NetIF_GetDataAlignPtr (NET_IF_NBR        if_nbr,
                                      NET_TRANSACTION   transaction,
                                      void             *p_data,
                                      NET_ERR          *p_err)
{
    NET_IF        *p_if;
    NET_DEV_CFG   *p_dev_cfg;
    void          *p_data_align;
    CPU_ADDR       addr;
    NET_BUF_SIZE   addr_offset;
    NET_BUF_SIZE   align;
    NET_BUF_SIZE   ix_align;
    NET_BUF_SIZE   ix_offset;
    NET_BUF_SIZE   data_ix;
    NET_BUF_SIZE   data_size;



#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ---------------- VALIDATE DATA PTR ----------------- */
    if (p_data == (void *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return ((void *)0);
    }
#endif


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIF_GetDataAlignPtr, p_err);
    if (*p_err !=  NET_ERR_NONE) {
         return ((void *)0);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail_null;
    }
#endif


                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err !=  NET_IF_ERR_NONE) {
         goto exit_fail_null;
    }


                                                                /* --------------- GET NET IF ALIGN/IX ---------------- */


    p_dev_cfg = (NET_DEV_CFG *)p_if->Dev_Cfg;
    switch (transaction) {
        case NET_TRANSACTION_RX:
             align     = p_dev_cfg->RxBufAlignOctets;
             ix_offset = p_dev_cfg->RxBufIxOffset;
             data_ix   = NET_BUF_DATA_IX_RX;
             break;


        case NET_TRANSACTION_TX:
             align     = p_dev_cfg->TxBufAlignOctets;
             ix_offset = p_dev_cfg->TxBufIxOffset;
             NetIF_TxIxDataGet(if_nbr,
                               0,
                              &data_ix,
                               p_err);
             break;


        case NET_TRANSACTION_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.InvTransactionTypeCtr);
            *p_err = NET_ERR_INVALID_TRANSACTION;
             goto exit_fail_null;
    }

    data_size = CPU_CFG_DATA_SIZE;

    if (((align %  data_size) != 0u) ||                         /* If net  buf align NOT multiple of CPU data word size,*/
         (align == 0u)) {
       *p_err =  NET_IF_ERR_ALIGN_NOT_AVAIL;                    /* .. data buf align NOT possible (see Note #3b1A1).    */
        goto exit_fail_data;
    }


                                                                /* ------------ CALC ALIGN'D DATA BUF PTR ------------- */
    addr        = (CPU_ADDR    ) p_data;
    addr_offset = (NET_BUF_SIZE)(addr % data_size);             /* Calc data addr  offset (see Note #3a1).              */
                                                                /* Calc data align offset (see Note #3a2).              */
    ix_align    =  data_ix    +  ix_offset;
    ix_align   %=  data_size;
    ix_align   +=  data_size  -  addr_offset;
    ix_align   %=  data_size;

    p_data_align = (void *)((CPU_INT08U *)p_data + ix_align);   /* Calc data align'd ptr  (see Note #3a3A).             */



    *p_err =  NET_IF_ERR_NONE;
     goto exit_release;

exit_fail_data:
     p_data_align = p_data;
     goto exit_release;

exit_fail_null:
    p_data_align = DEF_NULL;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();



    return (p_data_align);
}


/*
*********************************************************************************************************
*                                      NetIF_GetProtocolHdrSize()
*
* Description : Get the header length required by protocols.
*
* Argument(s) : p_if        Pointer to network interface.
*               ----        Argument validated in NetIF_ObjInit().
*
*               protocol    Desired protocol layer of network interface MTU.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network interface's MTU successfully returned.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_INVALID_PROTOCOL        Invalid network protocol.
*
*                                                               --------- RETURNED BY NetIF_Get() : ----------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Headers size.
*
* Caller(s)   : NetIF_GetPayloadRxMax(),
*               NetIF_GetPayloadTxMax(),
*               NetIF_MTU_GetProtocol().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  NetIF_GetProtocolHdrSize (NET_IF             *p_if,
                                              NET_PROTOCOL_TYPE   protocol,
                                              NET_ERR            *p_err)
{
    NET_IF_API  *p_if_api;
    CPU_INT16U   hdr_size = 0u;


    if (p_if != DEF_NULL) {
        switch (p_if->Type) {
            case NET_IF_TYPE_NONE:
                 break;

            case NET_IF_TYPE_ETHER:
            case NET_IF_TYPE_WIFI:
                 hdr_size += NET_IF_HDR_SIZE_ETHER;
                 break;

            case NET_IF_TYPE_LOOPBACK:
                 hdr_size += NET_IF_HDR_SIZE_LOOPBACK;
                 break;

            case NET_IF_TYPE_PPP:
            case NET_IF_TYPE_SERIAL:
            default:
                *p_err = NET_IF_ERR_INVALID_PROTOCOL;
                 goto exit;
        }
    }

                                                                /* ------------- CALC PROTOCOL LAYER MTU -------------- */
    switch (protocol) {
        case NET_PROTOCOL_TYPE_LINK:
             if (p_if == DEF_NULL) {
                *p_err = NET_ERR_FAULT_NULL_OBJ;
                 goto exit;
             }

             p_if_api = p_if->IF_API;

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
             if (p_if_api == DEF_NULL) {
                *p_err =  NET_IF_ERR_INVALID_CFG;
                 goto exit;
             }
             if (p_if_api->GetPktSizeHdr == DEF_NULL) {
                *p_err =  NET_ERR_FAULT_NULL_FNCT;
                 goto exit;
             }
#endif
             hdr_size = p_if_api->GetPktSizeHdr(p_if);
             break;


        case NET_PROTOCOL_TYPE_IF:
        case NET_PROTOCOL_TYPE_IF_FRAME:
        case NET_PROTOCOL_TYPE_IF_ETHER:
        case NET_PROTOCOL_TYPE_IF_IEEE_802:
             break;

        case NET_PROTOCOL_TYPE_ARP:
             hdr_size += NET_ARP_HDR_SIZE;
             break;

        case NET_PROTOCOL_TYPE_IP_V4:
        case NET_PROTOCOL_TYPE_ICMP_V4:
        case NET_PROTOCOL_TYPE_UDP_V4:
        case NET_PROTOCOL_TYPE_IGMP:
        case NET_PROTOCOL_TYPE_TCP_V4:
             hdr_size += NET_IPv4_HDR_SIZE_MIN;
             switch (protocol) {
                 case NET_PROTOCOL_TYPE_IP_V4:
                 case NET_PROTOCOL_TYPE_ICMP_V4:
                      break;

                 case NET_PROTOCOL_TYPE_UDP_V4:
                      hdr_size += NET_UDP_HDR_SIZE;
                      break;

                 case NET_PROTOCOL_TYPE_IGMP:
                      hdr_size += NET_IGMP_HDR_SIZE;
                      break;

                 case NET_PROTOCOL_TYPE_TCP_V4:
                      hdr_size += NET_TCP_HDR_SIZE_MIN;
                      break;

                 default:
                      break;
             }
             break;


        case NET_PROTOCOL_TYPE_IP_V6:
        case NET_PROTOCOL_TYPE_ICMP_V6:
        case NET_PROTOCOL_TYPE_UDP_V6:
        case NET_PROTOCOL_TYPE_TCP_V6:
             hdr_size += NET_IPv6_HDR_SIZE;
             switch (protocol) {
                 case NET_PROTOCOL_TYPE_IP_V6:
                 case NET_PROTOCOL_TYPE_ICMP_V6:
                      break;


                 case NET_PROTOCOL_TYPE_UDP_V6:
                      hdr_size += NET_UDP_HDR_SIZE;
                      break;


                 case NET_PROTOCOL_TYPE_TCP_V6:
                      hdr_size += NET_TCP_HDR_SIZE_MIN;
                      break;

                 default:
                      break;
             }
             break;

        case NET_PROTOCOL_TYPE_NONE:
        case NET_PROTOCOL_TYPE_APP:
        case NET_PROTOCOL_TYPE_SOCK:
        default:
            *p_err =  NET_ERR_INVALID_PROTOCOL;
             goto exit;
    }


   *p_err =  NET_IF_ERR_NONE;


exit:
    return (hdr_size);
}


/*
*********************************************************************************************************
*                                        NetIF_IO_CtrlHandler()
*
* Description : (1) Handle network interface &/or device specific (I/O) control(s) :
*
*                   (a) Device link :
*                       (1) Get    device link info
*                       (2) Get    device link state
*                       (3) Update device link state
*
*
* Argument(s) : if_nbr      Network interface number to handle (I/O) controls.
*
*               opt         Desired I/O control option code to perform; additional control options may be
*                           defined by the device driver :
*
*                               NET_IF_IO_CTRL_LINK_STATE_GET       Get    device's current  physical link state,
*                                                                       'UP' or 'DOWN' (see Note #3).
*                               NET_IF_IO_CTRL_LINK_STATE_GET_INFO  Get    device's detailed physical link state
*                                                                       information.
*                               NET_IF_IO_CTRL_LINK_STATE_UPDATE    Update device's current  physical link state.
*
*               p_data      Pointer to variable that will receive possible I/O control data (see Note #3).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL function pointer.
*
*                                                                   ---- RETURNED BY 'p_if_api->IO_Ctrl()' : -----
*                               NET_IF_ERR_NONE                     I/O control option successfully handled.
*                               NET_ERR_FAULT_NULL_PTR                 Argument 'p_data' passed a NULL pointer.
*                               NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid I/O control option.
*
*                                                                   See specific network interface(s) 'IO_Ctrl()'
*                                                                       for additional return error codes.
*
*                                                                   --------- RETURNED BY NetIF_Get() : ---------
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_IO_Ctrl(),
*               NetIF_PhyLinkStateHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (2) NetIF_IO_CtrlHandler() is called by network protocol suite function(s) & MUST be
*                   called with the global network lock already acquired.
*
*                   See also 'NetIF_IO_Ctrl()  Note #2'.
*
*               (3) 'p_data' MUST point to a variable or memory buffer that is sufficiently sized AND
*                    aligned to receive any return data.
*********************************************************************************************************
*/

static  void  NetIF_IO_CtrlHandler (NET_IF_NBR   if_nbr,
                                    CPU_INT08U   opt,
                                    void        *p_data,
                                    NET_ERR     *p_err)
{
    NET_IF      *p_if;
    NET_IF_API  *p_if_api;

                                                                /* -------------------- GET NET IF -------------------- */
    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* ------------------ GET NET IF API ------------------ */
    p_if_api = (NET_IF_API *)p_if->IF_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_if_api == (NET_IF_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_if_api->IO_Ctrl == (void (*)(NET_IF   *,
                                      CPU_INT08U,
                                      void     *,
                                      NET_ERR  *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* ----------------- HANDLE NET IF I/O ---------------- */
    p_if_api->IO_Ctrl(p_if, opt, p_data, p_err);
}


/*
*********************************************************************************************************
*                                     NetIF_PhyLinkStateHandler()
*
* Description : (1) Monitor network interfaces' physical layer link state :
*
*                   (a) Poll devices for current link state
*                   (b) Get Physical Link State Handler timer
*
*
* Argument(s) : p_obj       Pointer to this Network Interface Physical Link State Handler function
*                               (see Note #2).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetIF_Init(),
*                             NetIF_PhyLinkStateHandler().
*
* Note(s)     : (2) Network timer module requires a pointer to an object when allocating a timer.
*                   However, since the Physical Link State Handler does NOT use or require any
*                   object in order to execute, a NULL object pointer is passed instead.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (3) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Reset by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*
*               (4) (a) If a network interface's physical link state cannot be determined, it should NOT
*                       be updated until the interface's physical link state can be correctly determined.
*                       This allows the interface to continue to transmit packets despite any transitory
*                       error(s) in determining network interface's physical link state.
*
*                   (b) (1) Network interfaces' 'Link' variables MUST ALWAYS be accessed exclusively with
*                           the global network lock already acquired.
*
*                       (2) Therefore, physical layer link states CANNOT be asynchronously updated by any
*                           network interface, device driver, or physical layer functions; including  any
*                           interrupt service routines (ISRs).
*********************************************************************************************************
*/
#ifndef  NET_CFG_LINK_STATE_POLL_DISABLED
static  void  NetIF_PhyLinkStateHandler (void  *p_obj)
{
    NET_IF                      *p_if;
    NET_IF_NBR                   if_nbr;
    NET_IF_NBR                   if_nbr_next;
    NET_TMR_TICK                 time_tick;
    NET_IF_LINK_STATE            link_state;
    NET_IF_LINK_SUBSCRIBER_OBJ  *p_subsciber_obj;
    NET_ERR                      err;
    CPU_SR_ALLOC();


   (void)&p_obj;                                                    /* Prevent 'variable unused' warning (see Note #2). */

                                                                    /* ------- GET ALL NET IF's PHY LINK STATE -------- */
    if_nbr      = NET_IF_NBR_BASE_CFGD;
    CPU_CRITICAL_ENTER();
    if_nbr_next = NetIF_NbrNext;
    CPU_CRITICAL_EXIT();

    p_if = &NetIF_Tbl[if_nbr];

    for ( ; if_nbr < if_nbr_next; if_nbr++) {
        if (p_if->En == DEF_ENABLED) {
            NetIF_IO_CtrlHandler(              if_nbr,
                                               NET_IF_IO_CTRL_LINK_STATE_GET,
                                 (void      *)&link_state,
                                              &err);

            if (err == NET_IF_ERR_NONE) {                           /* If NO err(s)               [see Note #4a], ...   */
                p_if->Link = link_state;                            /* ... update IF's link state (see Note #4b).       */

                if (link_state != p_if->LinkPrev) {                 /* If Link state changed since last read ...        */
                                                                    /* ...  notify subscriber.                          */
                    p_subsciber_obj = p_if->LinkSubscriberListHeadPtr;

                                                                    /* --------------- RELEASE NET LOCK --------------- */
                    Net_GlobalLockRelease();


                    while (p_subsciber_obj != DEF_NULL) {

                        if (p_subsciber_obj->Fnct != DEF_NULL) {
                            p_subsciber_obj->Fnct(p_if->Nbr, link_state);
                        }

                        p_subsciber_obj = p_subsciber_obj->NextPtr;
                    }

                                                                    /* --------------- ACQUIRE NET LOCK --------------- */
                    Net_GlobalLockAcquire((void *)&NetIF_PhyLinkStateHandler, &err);
                }

                p_if->LinkPrev = link_state;
            }
        }

        p_if++;
    }

                                                                    /* ------------ GET PHY LINK STATE TMR ------------ */
    CPU_CRITICAL_ENTER();
    time_tick             = NetIF_PhyLinkStateTime_tick;
    CPU_CRITICAL_EXIT();
    NetIF_PhyLinkStateTmr = NetTmr_Get(&NetIF_PhyLinkStateHandler,
                                        0,                          /* See Note #2.                                     */
                                        time_tick,
                                       &err);
}
#endif


/*
*********************************************************************************************************
*                                       NetIF_PerfMonHandler()
*
* Description : (1) Monitor network interfaces' performance :
*
*                   (a) Calculate & update network interfaces' performance statistics
*                   (b) Get Performance Monitor Handler timer
*
*
* Argument(s) : p_obj       Pointer to this Network Interface Performance Monitor Handler function
*                               (see Note #2).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetIF_Init(),
*                             NetIF_PerfMonHandler().
*
* Note(s)     : (2) Network timer module requires a pointer to an object when allocating a timer.
*                   However, since the Performance Monitor Handler does NOT use or require any
*                   object in order to execute, a NULL object pointer is passed instead.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (3) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Reset by NetTmr_Get().
*
*                   (b) but do NOT re-free the timer.
*********************************************************************************************************
*/

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
static  void  NetIF_PerfMonHandler (void  *p_obj)
{
    NET_IF            *p_if;
    NET_CTR_IF_STATS  *p_if_stats;
    NET_IF_NBR         if_nbr;
    NET_IF_NBR         if_nbr_next;
    NET_CTR            rx_octets_cur;
    NET_CTR            rx_octets_prev;
    NET_CTR            rx_octets_per_sec;
    NET_CTR            rx_pkt_cnt_cur;
    NET_CTR            rx_pkt_cnt_prev;
    NET_CTR            rx_pkt_per_sec;
    NET_CTR            tx_octets_cur;
    NET_CTR            tx_octets_prev;
    NET_CTR            tx_octets_per_sec;
    NET_CTR            tx_pkt_cnt_prev;
    NET_CTR            tx_pkt_cnt_cur;
    NET_CTR            tx_pkt_per_sec;
    NET_TS_MS          ts_ms_cur;
    NET_TS_MS          ts_ms_prev;
    NET_TS_MS          ts_ms_delta;
    NET_TMR_TICK       time_tick;
    CPU_BOOLEAN        update_dlyd;
    CPU_BOOLEAN        reset;
    NET_ERR            err;
    CPU_SR_ALLOC();


   (void)&p_obj;                                                            /* Prevent 'variable unused' (see Note #2). */


                                                                            /* ------- UPDATE NET IF PERF STATS ------- */
    if_nbr      = NetIF_NbrBase;
    CPU_CRITICAL_ENTER();
    if_nbr_next = NetIF_NbrNext;
    reset       = NetIF_CtrsResetEn;
    CPU_CRITICAL_EXIT();

    if (reset == DEF_YES) {
        NetCtr_Init();
        CPU_CRITICAL_ENTER();
        NetIF_CtrsResetEn = DEF_NO;
        CPU_CRITICAL_EXIT();
    }

    p_if       = &NetIF_Tbl[if_nbr];
    p_if_stats = &Net_StatCtrs.IFs.IF[if_nbr];

    ts_ms_cur =  NetUtil_TS_Get_ms();

    for ( ; if_nbr < if_nbr_next; if_nbr++) {
        if (p_if->En == DEF_ENABLED) {

            rx_octets_cur  = p_if_stats->RxNbrOctets;
            tx_octets_cur  = p_if_stats->TxNbrOctets;
            rx_pkt_cnt_cur = p_if_stats->RxNbrPktCtrProcessed;
            tx_pkt_cnt_cur = p_if_stats->TxNbrPktCtrProcessed;

            update_dlyd    = DEF_NO;

            if (p_if->PerfMonState == NET_IF_PERF_MON_STATE_RUN) {          /* If perf mon already running &       ...  */

                ts_ms_prev = p_if->PerfMonTS_Prev_ms;
                if (ts_ms_cur > ts_ms_prev) {                               /* ... cur ts > prev ts,               ...  */
                                                                            /* ... update   perf mon stats :       ...  */
                                                                            /* ... get prev perf mon vals,         ...  */
                    rx_octets_prev    = p_if_stats->RxNbrOctetsPrev;
                    tx_octets_prev    = p_if_stats->TxNbrOctetsPrev;
                    rx_pkt_cnt_prev   = p_if_stats->RxNbrPktCtrProcessedPrev;
                    tx_pkt_cnt_prev   = p_if_stats->TxNbrPktCtrProcessedPrev;

                    ts_ms_delta       = ts_ms_cur - ts_ms_prev;             /* ... calc delta ts (in ms), &        ...  */

                                                                            /* ... calc/update cur perf mon stats; ...  */
                    rx_octets_per_sec = ((rx_octets_cur  - rx_octets_prev ) * DEF_TIME_NBR_mS_PER_SEC) / ts_ms_delta;
                    tx_octets_per_sec = ((tx_octets_cur  - tx_octets_prev ) * DEF_TIME_NBR_mS_PER_SEC) / ts_ms_delta;
                    rx_pkt_per_sec    = ((rx_pkt_cnt_cur - rx_pkt_cnt_prev) * DEF_TIME_NBR_mS_PER_SEC) / ts_ms_delta;
                    tx_pkt_per_sec    = ((tx_pkt_cnt_cur - tx_pkt_cnt_prev) * DEF_TIME_NBR_mS_PER_SEC) / ts_ms_delta;


                    p_if_stats->RxNbrOctetsPerSec = rx_octets_per_sec;
                    p_if_stats->TxNbrOctetsPerSec = tx_octets_per_sec;
                    p_if_stats->RxNbrPktCtrPerSec = rx_pkt_per_sec;
                    p_if_stats->TxNbrPktCtrPerSec = tx_pkt_per_sec;

                    if (p_if_stats->RxNbrOctetsPerSecMax < rx_octets_per_sec) {
                        p_if_stats->RxNbrOctetsPerSecMax = rx_octets_per_sec;
                    }
                    if (p_if_stats->TxNbrOctetsPerSecMax < tx_octets_per_sec) {
                        p_if_stats->TxNbrOctetsPerSecMax = tx_octets_per_sec;
                    }
                    if (p_if_stats->RxNbrPktCtrPerSecMax < rx_pkt_per_sec) {
                        p_if_stats->RxNbrPktCtrPerSecMax = rx_pkt_per_sec;
                    }
                    if (p_if_stats->TxNbrPktCtrPerSecMax < tx_pkt_per_sec) {
                        p_if_stats->TxNbrPktCtrPerSecMax = tx_pkt_per_sec;
                    }

                } else {                                                    /* ... else dly perf mon stats update.      */
                    update_dlyd = DEF_YES;
                }
            }

            if (update_dlyd != DEF_YES) {                                   /* If update NOT dly'd, ...                 */
                                                                            /* ... save cur stats for next update.      */
                p_if_stats->RxNbrOctetsPrev          = rx_octets_cur;
                p_if_stats->TxNbrOctetsPrev          = tx_octets_cur;
                p_if_stats->RxNbrPktCtrProcessedPrev = rx_pkt_cnt_cur;
                p_if_stats->TxNbrPktCtrProcessedPrev = tx_pkt_cnt_cur;

                p_if->PerfMonTS_Prev_ms                        = ts_ms_cur;
            }

            p_if->PerfMonState = NET_IF_PERF_MON_STATE_RUN;

        } else {
            p_if->PerfMonState = NET_IF_PERF_MON_STATE_STOP;
        }

        p_if++;
        p_if_stats++;
    }


                                                                            /* ----------- GET PERF MON TMR ----------- */
    CPU_CRITICAL_ENTER();
    time_tick        = NetIF_PerfMonTime_tick;
    CPU_CRITICAL_EXIT();
    NetIF_PerfMonTmr = NetTmr_Get((CPU_FNCT_PTR )&NetIF_PerfMonHandler,
                                  (void        *) 0,                        /* See Note #2.                             */
                                  (NET_TMR_TICK ) time_tick,
                                  (NET_ERR     *)&err);
}
#endif
