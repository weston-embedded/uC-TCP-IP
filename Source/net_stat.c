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
* Filename : net_stat.c
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
#define    NET_STAT_MODULE
#include  "net_stat.h"
#include  "net_ctr.h"
#include  "net_err.h"


/*
*********************************************************************************************************
*                                            NetStat_Init()
*
* Description : (1) Initialize Network Statistic Management Module :
*
*                   Module initialization NOT yet required/implemented
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
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetStat_Init (void)
{
}


/*
*********************************************************************************************************
*                                          NetStat_CtrInit()
*
* Description : Initialize a statistics counter.
*
* Argument(s) : p_stat_ctr  Pointer to a statistics counter (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics counter successfully initialized.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_ctr' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Assumes 'p_stat_ctr' points to valid statistics counter (if non-NULL).
*
*               (2) Statistic counters MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_CtrInit (NET_STAT_CTR  *p_stat_ctr,
                       NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_stat_ctr == (NET_STAT_CTR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_ctr->CurCtr = 0u;
    p_stat_ctr->MaxCtr = 0u;
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetStat_CtrClr()
*
* Description : Clear a statistics counter.
*
* Argument(s) : p_stat_ctr  Pointer to a statistics counter (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics counter successfully cleared.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_ctr' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Assumes 'p_stat_ctr' points to valid statistics counter (if non-NULL).
*
*               (2) Statistic counters MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_CtrClr (NET_STAT_CTR  *p_stat_ctr,
                      NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_stat_ctr == (NET_STAT_CTR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_ctr->CurCtr = 0u;
    p_stat_ctr->MaxCtr = 0u;
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetStat_CtrReset()
*
* Description : Reset a statistics counter.
*
* Argument(s) : p_stat_ctr  Pointer to a statistics counter.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics counter successfully reset.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_ctr' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics counter type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Statistic counters MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_CtrReset (NET_STAT_CTR  *p_stat_ctr,
                        NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_ctr == (NET_STAT_CTR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_ctr->CurCtr = 0u;
    p_stat_ctr->MaxCtr = 0u;
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetStat_CtrResetMax()
*
* Description : Reset a statistics counter's maximum number of counts.
*
*               (1) Resets maximum number of counts to the current number of counts.
*
*
* Argument(s) : p_stat_ctr  Pointer to a statistics counter.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics counter's maximum number of
*                                                                   counts successfully reset.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_ctr' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics counter type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Statistic counters MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_CtrResetMax (NET_STAT_CTR  *p_stat_ctr,
                           NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_ctr == (NET_STAT_CTR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_ctr->MaxCtr = p_stat_ctr->CurCtr;                    /* Reset max cnts (see Note #1).                        */
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetStat_CtrInc()
*
* Description : Increment a statistics counter.
*
* Argument(s) : p_stat_ctr  Pointer to a statistics counter.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics counter successfully incremented
*                                                                   (see Note #2).
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_ctr' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics counter type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Statistic counters MUST ALWAYS be accessed exclusively in critical sections.
*
*               (2) Statistic counter increment overflow prevented but ignored.
*
*                   See also 'NetStat_CtrDec()  Note #2'.
*********************************************************************************************************
*/

void  NetStat_CtrInc (NET_STAT_CTR  *p_stat_ctr,
                      NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_ctr == (NET_STAT_CTR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    if (p_stat_ctr->CurCtr < NET_CTR_MAX) {                     /* See Note #2.                                         */
        p_stat_ctr->CurCtr++;

        if (p_stat_ctr->MaxCtr < p_stat_ctr->CurCtr) {          /* If max cnt < cur cnt, set new max cnt.               */
            p_stat_ctr->MaxCtr = p_stat_ctr->CurCtr;
        }
    }
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetStat_CtrDec()
*
* Description : Decrement a statistics counter.
*
* Argument(s) : p_stat_ctr  Pointer to a statistics counter.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics counter successfully decremented
*                                                                   (see Note #2).
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_ctr' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics counter type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Statistic counters MUST ALWAYS be accessed exclusively in critical sections.
*
*               (2) Statistic counter decrement underflow prevented but ignored.
*
*                   See also 'NetStat_CtrInc()  Note #2'.
*********************************************************************************************************
*/

void  NetStat_CtrDec (NET_STAT_CTR  *p_stat_ctr,
                      NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_ctr == (NET_STAT_CTR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    if (p_stat_ctr->CurCtr > NET_CTR_MIN) {                     /* See Note #2.                                         */
        p_stat_ctr->CurCtr--;
    }
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetStat_PoolInit()
*
* Description : Initialize a statistics pool.
*
* Argument(s) : p_stat_pool Pointer to a statistics pool (see Note #1).
*
*               nbr_avail   Total number of available statistics pool entries.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool successfully initialized.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Assumes 'p_stat_pool' points to valid statistics pool (if non-NULL).
*
*               (2) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolInit (NET_STAT_POOL      *p_stat_pool,
                        NET_STAT_POOL_QTY   nbr_avail,
                        NET_ERR            *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_pool->EntriesInit       = nbr_avail;                 /* Init nbr of pool  entries is also ...                */
    p_stat_pool->EntriesTot        = nbr_avail;                 /* Tot  nbr of pool  entries is also ...                */
    p_stat_pool->EntriesAvail      = nbr_avail;                 /* Init nbr of avail entries.                           */
    p_stat_pool->EntriesUsed       = 0u;
    p_stat_pool->EntriesUsedMax    = 0u;
    p_stat_pool->EntriesLostCur    = 0u;
    p_stat_pool->EntriesLostTot    = 0u;
    p_stat_pool->EntriesAllocCtr   = 0u;
    p_stat_pool->EntriesDeallocCtr = 0u;
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetStat_PoolClr()
*
* Description : Clear a statistics pool.
*
* Argument(s) : p_stat_pool Pointer to a statistics pool (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool successfully cleared.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Assumes 'p_stat_pool' points to valid statistics pool (if non-NULL).
*
*               (2) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolClr (NET_STAT_POOL  *p_stat_pool,
                       NET_ERR        *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_pool->EntriesInit       = 0u;
    p_stat_pool->EntriesTot        = 0u;
    p_stat_pool->EntriesAvail      = 0u;
    p_stat_pool->EntriesUsed       = 0u;
    p_stat_pool->EntriesUsedMax    = 0u;
    p_stat_pool->EntriesLostCur    = 0u;
    p_stat_pool->EntriesLostTot    = 0u;
    p_stat_pool->EntriesAllocCtr   = 0u;
    p_stat_pool->EntriesDeallocCtr = 0u;
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetStat_PoolReset()
*
* Description : Reset a statistics pool.
*
*               (1) Assumes object pool is also reset; otherwise, statistics pool will NOT accurately
*                   reflect the state of the object pool.
*
*
* Argument(s) : p_stat_pool Pointer to a statistics pool.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool successfully reset.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics pool type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolReset (NET_STAT_POOL  *p_stat_pool,
                         NET_ERR        *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_pool->EntriesAvail      = p_stat_pool->EntriesTot;
    p_stat_pool->EntriesUsed       = 0u;
    p_stat_pool->EntriesUsedMax    = 0u;
    p_stat_pool->EntriesLostCur    = 0u;
    p_stat_pool->EntriesAllocCtr   = 0u;
    p_stat_pool->EntriesDeallocCtr = 0u;
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetStat_PoolResetUsedMax()
*
* Description : Reset a statistics pool's maximum number of entries used.
*
*               (1) Resets maximum number of entries used to the current number of entries used.
*
*
* Argument(s) : p_stat_pool Pointer to a statistics pool.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool's maximum number of entries
*                                                                   used successfully reset.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics pool type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolResetUsedMax (NET_STAT_POOL  *p_stat_pool,
                                NET_ERR        *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    p_stat_pool->EntriesUsedMax = p_stat_pool->EntriesUsed;     /* Reset nbr max used (see Note #1).                    */
    CPU_CRITICAL_EXIT();


   *p_err = NET_STAT_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetStat_PoolEntryUsedInc()
*
* Description : Increment a statistics pool's number of 'Used' entries.
*
* Argument(s) : p_stat_pool  Pointer to a statistics pool.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool's number used
*                                                                   successfully incremented.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics pool type.
*                               NET_STAT_ERR_POOL_NONE_AVAIL    NO available statistics pool entries; i.e.
*                                                                   number of available entries already zero.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolEntryUsedInc (NET_STAT_POOL  *p_stat_pool,
                                NET_ERR        *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    if (p_stat_pool->EntriesAvail > 0) {                            /* If any stat pool entry avail,             ...    */
        p_stat_pool->EntriesAvail--;                                /* ... adj nbr of avail/used entries in pool ...    */
        p_stat_pool->EntriesUsed++;
        p_stat_pool->EntriesAllocCtr++;                             /* ... & inc tot nbr of alloc'd entries.            */
        if (p_stat_pool->EntriesUsedMax < p_stat_pool->EntriesUsed) { /* If max used < nbr used, set new max used.      */
            p_stat_pool->EntriesUsedMax = p_stat_pool->EntriesUsed;
        }

       *p_err = NET_STAT_ERR_NONE;

    } else {
       *p_err = NET_STAT_ERR_POOL_NONE_AVAIL;
    }
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                     NetStat_PoolEntryUsedDec()
*
* Description : Decrement a statistics pool's number of 'Used' entries.
*
* Argument(s) : p_stat_pool  Pointer to a statistics pool.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool's number used
*                                                                   successfully decremented.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics pool type.
*                               NET_STAT_ERR_POOL_NONE_USED     NO used statistics pool entries; i.e.
*                                                                   number of used entries already zero.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolEntryUsedDec (NET_STAT_POOL  *p_stat_pool,
                                NET_ERR        *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    if (p_stat_pool->EntriesUsed > 0) {                         /* If any stat pool entry used,              ...        */
        p_stat_pool->EntriesAvail++;                            /* ... adj nbr of avail/used entries in pool ...        */
        p_stat_pool->EntriesUsed--;
        p_stat_pool->EntriesDeallocCtr++;                       /* ... & inc tot nbr of dealloc'd entries.              */

       *p_err = NET_STAT_ERR_NONE;

    } else {
       *p_err = NET_STAT_ERR_POOL_NONE_USED;
    }
    CPU_CRITICAL_EXIT();
}


/*
*********************************************************************************************************
*                                     NetStat_PoolEntryLostInc()
*
* Description : Increment a statistics pool's number of 'Lost' entries.
*
* Argument(s) : p_stat_pool Pointer to a statistics pool.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_STAT_ERR_NONE               Statistics pool's number lost
*                                                                   successfully incremented.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_stat_pool' passed a NULL pointer.
*                               NET_STAT_ERR_INVALID_TYPE       Invalid statistics pool type.
*                               NET_STAT_ERR_POOL_NONE_REM      NO statistics pool entries remaining; i.e.
*                                                                   total number of entries already zero.
*                               NET_STAT_ERR_POOL_NONE_USED     NO used statistics pool entries; i.e.
*                                                                   number of used entries is zero.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Pool statistic entries MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

void  NetStat_PoolEntryLostInc (NET_STAT_POOL  *p_stat_pool,
                                NET_ERR        *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_stat_pool == (NET_STAT_POOL *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Stat.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    CPU_CRITICAL_ENTER();
    if (p_stat_pool->EntriesTot > 0) {                          /* If   tot stat pool entries > 0  ...                  */
        if (p_stat_pool->EntriesUsed > 0) {                     /* ... & any stat pool entry used, ...                  */
            p_stat_pool->EntriesUsed--;                         /* ... adj nbr used/total/lost entries in pool.         */
            p_stat_pool->EntriesTot--;
            p_stat_pool->EntriesLostCur++;
            p_stat_pool->EntriesLostTot++;

           *p_err = NET_STAT_ERR_NONE;
        } else {
           *p_err = NET_STAT_ERR_POOL_NONE_USED;
        }

    } else {
       *p_err = NET_STAT_ERR_POOL_NONE_REM;
    }
    CPU_CRITICAL_EXIT();
}

