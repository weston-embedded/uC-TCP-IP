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
*                                      NETWORK COUNTER MANAGEMENT
*
* Filename : net_ctr.c
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
#define    NET_CTR_MODULE
#include  "net_cfg_net.h"
#include  "net_ctr.h"
#include  <cpu.h>
#include  <lib_mem.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           STATISTIC COUNTERS
*********************************************************************************************************
*/

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
NET_CTR_STATS  Net_StatCtrs;
#endif


/*
*********************************************************************************************************
*                                           ERROR COUNTERS
*********************************************************************************************************
*/

#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
NET_CTR_ERRS   Net_ErrCtrs;
#endif


/*
*********************************************************************************************************
*                                            NetCtr_Init()
*
* Description : (1) Initialize Network Counter Management Module :
*
*                   (a) Initialize network statistics counters
*                   (b) Initialize network error      counters
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

void  NetCtr_Init (void)
{
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)                        /* ---------------- INIT NET STAT CTRS ---------------- */
    Mem_Clr((void     *)      &Net_StatCtrs,
            (CPU_SIZE_T)sizeof(Net_StatCtrs));
#endif

#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)                        /* ---------------- INIT NET ERR  CTRS ---------------- */
    Mem_Clr((void     *)      &Net_ErrCtrs,
            (CPU_SIZE_T)sizeof(Net_ErrCtrs));
#endif
}


/*
*********************************************************************************************************
*                                            NetCtr_Inc()
*
* Description : Increment a network counter.
*
* Argument(s) : pctr        Pointer to a network counter.
*
* Return(s)   : none.
*
* Caller(s)   : NET_CTR_INC().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network counter variables MUST ALWAYS be accessed exclusively in critical sections.
*
*                   See also 'net_ctr.h  NETWORK COUNTER MACRO'S  Note #1a'.
*********************************************************************************************************
*/

#ifdef  NET_CTR_MODULE_EN
void  NetCtr_Inc (NET_CTR  *pctr)
{
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
  (*pctr)++;
    CPU_CRITICAL_EXIT();
}
#endif


/*
*********************************************************************************************************
*                                          NetCtr_IncLarge()
*
* Description : Increment a large network counter.
*
* Argument(s) : pctr_hi     Pointer to high half of a large network counter.
*
*               pctr_lo     Pointer to low  half of a large network counter.
*
* Return(s)   : none.
*
* Caller(s)   : NET_CTR_INC_LARGE().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network counter variables MUST ALWAYS be accessed exclusively in critical sections.
*
*                   See also 'net_ctr.h  NETWORK COUNTER MACRO'S  Note #1b'.
*********************************************************************************************************
*/

#ifdef  NET_CTR_MODULE_EN
void  NetCtr_IncLarge (NET_CTR  *pctr_hi,
                       NET_CTR  *pctr_lo)
{
  (*pctr_lo)++;                                                 /* Inc lo-half ctr.                                     */
    if (*pctr_lo == 0u) {                                       /* If  lo-half ctr ovfs, ...                            */
       (*pctr_hi)++;                                            /* inc hi-half ctr.                                     */
    }
}
#endif

