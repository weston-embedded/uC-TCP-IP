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
*                                     NETWORK STATISTICS MANAGEMENT
*
* Filename : net_stat.h
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

#ifndef  NET_STAT_MODULE_PRESENT
#define  NET_STAT_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  <cpu_core.h>
#include  "net_ctr.h"
#include  "net_err.h"
#include  "net_type.h"


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                              NETWORK STATISTICS POOL QUANTITY DATA TYPE
*
* Note(s) : (1) Statistics pool quantity data type MUST be configured with an appropriate-sized network
*               data type large enough to perform calculations on the following data types :
*
*               (a) NET_BUF_QTY
*               (b) NET_TMR_QTY
*               (c) NET_CONN_QTY
*               (d) NET_CONN_LIST_QTY
*               (e) NET_ARP_CACHE_QTY
*               (f) NET_ICMP_SRC_QUENCH_QTY
*               (g) NET_TCP_CONN_QTY
*               (h) NET_SOCK_QTY
*
*           (2) NET_STAT_POOL_NBR_MAX  SHOULD be #define'd based on 'NET_STAT_POOL_QTY' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_STAT_POOL_QTY;                         /* Defines max qty of stat pool entries to support.     */

#define  NET_STAT_POOL_NBR_MIN                             1
#define  NET_STAT_POOL_NBR_MAX           DEF_INT_16U_MAX_VAL    /* See Note #2.                                         */

#define  NET_STAT_POOL_MIN_VAL                             0
#define  NET_STAT_POOL_MAX_VAL    (NET_STAT_POOL_NBR_MAX - 1)


/*
*********************************************************************************************************
*                                NETWORK STATISTICS COUNTER DATA TYPE
*
* Note(s) : (1) Counter statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*
*           (2) (a) 'CurCtr' tracks the current counter value; & ...
*               (b) 'MaxCtr' tracks the maximum counter value at any one time.
*********************************************************************************************************
*/

                                                                /* ------------------- NET STAT CTR ------------------- */
typedef  struct  net_stat_ctr {
    NET_CTR             CurCtr;                                 /* Cur ctr val.                                         */
    NET_CTR             MaxCtr;                                 /* Max ctr val.                                         */
} NET_STAT_CTR;


/*
*********************************************************************************************************
*                                  NETWORK STATISTICS POOL DATA TYPE
*
* Note(s) : (1) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*
*           (2) (a) 'EntriesInit'/'EntriesTot' indicate the initial/current total number of entries in
*                    the statistics pool.
*
*               (b) 'EntriesAvail'/'EntriesUsed' track the current number of entries available/used
*                    from the statistics pool, while 'EntriesUsedMax' tracks the maximum number of
*                    entries used at any one time.
*
*               (c) 'EntriesLostCur'/'EntriesLostTot' track the current/total number of unrecoverable
*                    invalid/corrupted entries lost from the statistics pool.  Lost entries MUST be
*                    determined by the owner of the statistics pool.
*
*               (d) 'EntriesAllocCtr'/'EntriesDeallocCtr' track the current number of allocated/
*                    deallocated entries in the statistics pool.
*
*           (3) Assuming statistics pool are always accessed in critical sections (see Note #2), the
*               following equations for statistics pools are true at all times :
*
*                       EntriesInit = EntriesTot   + EntriesLostTot
*
*                       EntriesTot  = EntriesAvail + EntriesUsed
*
*                       EntriesUsed = EntriesAllocCtr - EntriesDeallocCtr - EntriesLostCur
*********************************************************************************************************
*/

                                                                /* ------------------- NET STAT POOL ------------------ */
typedef  struct  net_stat_pool {
    NET_STAT_POOL_QTY   EntriesInit;                            /* Init  nbr entries      in   pool.                    */
    NET_STAT_POOL_QTY   EntriesTot;                             /* Tot   nbr entries      in   pool.                    */
    NET_STAT_POOL_QTY   EntriesAvail;                           /* Avail nbr entries      in   pool.                    */
    NET_STAT_POOL_QTY   EntriesUsed;                            /* Used  nbr entries      from pool.                    */
    NET_STAT_POOL_QTY   EntriesUsedMax;                         /* Max   nbr entries used from pool.                    */
    NET_STAT_POOL_QTY   EntriesLostCur;                         /* Cur   nbr entries lost from pool.                    */
    NET_STAT_POOL_QTY   EntriesLostTot;                         /* Tot   nbr entries lost from pool.                    */

    NET_CTR             EntriesAllocCtr;                        /* Tot   nbr entries successfully   alloc'd.            */
    NET_CTR             EntriesDeallocCtr;                      /* Tot   nbr entries successfully dealloc'd.            */
} NET_STAT_POOL;



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

void  NetStat_Init            (void);



                                                                            /* --------- NET STAT CTR  FNCTS ---------- */
void  NetStat_CtrInit         (NET_STAT_CTR       *p_stat_ctr,
                               NET_ERR            *p_err);

void  NetStat_CtrClr          (NET_STAT_CTR       *p_stat_ctr,
                               NET_ERR            *p_err);


void  NetStat_CtrReset        (NET_STAT_CTR       *p_stat_ctr,
                               NET_ERR            *p_err);

void  NetStat_CtrResetMax     (NET_STAT_CTR       *p_stat_ctr,
                               NET_ERR            *p_err);


void  NetStat_CtrInc          (NET_STAT_CTR       *p_stat_ctr,
                               NET_ERR            *p_err);

void  NetStat_CtrDec          (NET_STAT_CTR       *p_stat_ctr,
                               NET_ERR            *p_err);



                                                                            /* --------- NET STAT POOL FNCTS ---------- */
void  NetStat_PoolInit        (NET_STAT_POOL      *p_stat_pool,
                               NET_STAT_POOL_QTY   nbr_avail,
                               NET_ERR            *p_err);

void  NetStat_PoolClr         (NET_STAT_POOL      *p_stat_pool,
                               NET_ERR            *p_err);


void  NetStat_PoolReset       (NET_STAT_POOL      *p_stat_pool,
                               NET_ERR            *p_err);

void  NetStat_PoolResetUsedMax(NET_STAT_POOL      *p_stat_pool,
                               NET_ERR            *p_err);


void  NetStat_PoolEntryUsedInc(NET_STAT_POOL      *p_stat_pool,
                               NET_ERR            *p_err);

void  NetStat_PoolEntryUsedDec(NET_STAT_POOL      *p_stat_pool,
                               NET_ERR            *p_err);

void  NetStat_PoolEntryLostInc(NET_STAT_POOL      *p_stat_pool,
                               NET_ERR            *p_err);


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
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_STAT_MODULE_PRESENT */
