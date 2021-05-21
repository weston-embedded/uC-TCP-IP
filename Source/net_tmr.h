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
*                                       NETWORK TIMER MANAGEMENT
*
* Filename : net_tmr.h
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

#ifndef  NET_TMR_MODULE_PRESENT
#define  NET_TMR_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net.h"
#include  "net_cfg_net.h"
#include  "net_type.h"
#include  "net_stat.h"
#include  "net_err.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                   NETWORK TIMER TASK TIME DEFINES
*
* Note(s) : (1) Time constants based on NET_TMR_CFG_TASK_FREQ, NetTmr_TaskHandler()'s frequency [i.e. how
*               often NetTmr_TaskHandler() is scheduled to run per second as implemented in NetTmr_Task()].
*
*           (2) NET_TMR_CFG_TASK_FREQ  MUST NOT be configured as a floating-point frequency.
*********************************************************************************************************
*/

#define  NET_TMR_TIME_0S                                   0
#define  NET_TMR_TIME_1S                                  (1  *  NET_TMR_CFG_TASK_FREQ)

#define  NET_TMR_TIME_TICK                                 1
#define  NET_TMR_TIME_TICK_PER_SEC                       NET_TMR_TIME_1S



#define  NET_TMR_TASK_PERIOD_SEC                                 NET_TMR_CFG_TASK_FREQ
#define  NET_TMR_TASK_PERIOD_mS     (DEF_TIME_NBR_mS_PER_SEC  /  NET_TMR_CFG_TASK_FREQ)
#define  NET_TMR_TASK_PERIOD_uS     (DEF_TIME_NBR_uS_PER_SEC  /  NET_TMR_CFG_TASK_FREQ)
#define  NET_TMR_TASK_PERIOD_nS     (DEF_TIME_NBR_nS_PER_SEC  /  NET_TMR_CFG_TASK_FREQ)


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  NETWORK TIMER QUANTITY DATA TYPE
*
* Note(s) : (1) NET_TMR_NBR_MAX  SHOULD be #define'd based on 'NET_TMR_QTY' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_TMR_QTY;                               /* Defines max qty of net tmrs to support.              */

#define  NET_TMR_NBR_MIN                                  1
#define  NET_TMR_NBR_MAX                DEF_INT_16U_MAX_VAL     /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                    NETWORK TIMER TICK DATA TYPE
*
* Note(s) : (1) 'NET_TMR_TIME_INFINITE_TICK' & 'NET_TMR_TIME_INFINITE'  MUST be globally #define'd AFTER
*               'NET_TMR_TICK' data type declared.
*********************************************************************************************************
*/

typedef  CPU_INT32U    NET_TMR_TICK;

#define  NET_TMR_TIME_INFINITE          DEF_INT_32U_MAX_VAL     /* Define as max unsigned val (see Note #1).            */


/*
*********************************************************************************************************
*                                    NETWORK TIMER FLAGS DATA TYPE
*********************************************************************************************************
*/

typedef  NET_FLAGS  NET_TMR_FLAGS;


/*
*********************************************************************************************************
*                                       NETWORK TIMER DATA TYPE
*
*                                    NET_TMR

*                     Previous   |-------------|
*                      Timer <----------O      |
*                                |-------------|     Next
*                                |      O----------> Timer
*                                |-------------|                    -------------
*                                |      O-------------------------> |           |
*                                |-------------|       Object       |  Object   |
*                                |      O----------> Expiration     |   that    |
*                                |-------------|      Function      | requested |
*                                |   Current   |                    |   Timer   |
*                                | Timer value |                    |           |
*                                |-------------|                    -------------
*
*********************************************************************************************************
*/

                                                                /* --------------------- NET TMR ---------------------- */
typedef  struct  net_tmr  NET_TMR;

struct  net_tmr {
    NET_TMR        *PrevPtr;                                    /* Ptr to PREV tmr.                                     */
    NET_TMR        *NextPtr;                                    /* Ptr to NEXT tmr.                                     */

    void           *Obj;                                        /* Ptr to obj  using TMR.                               */
    CPU_FNCT_PTR    Fnct;                                       /* Ptr to fnct used on obj when TMR expires.            */

    NET_TMR_TICK    TmrVal;                                     /* Cur tmr val (in NET_TMR_TICK ticks).                 */
};


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

                                                                /* ----------------- TMR STATUS FNCTS ----------------- */
NET_STAT_POOL   NetTmr_PoolStatGet         (void);

void            NetTmr_PoolStatResetMaxUsed(void);


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void            NetTmr_Init                (const NET_TASK_CFG   *p_tmr_task_cfg,
                                                  NET_ERR        *p_err);

void            NetTmr_TaskHandler         (      void);


                                                                /* --------------- TMR ALLOCATION FNCTS --------------- */
NET_TMR        *NetTmr_Get                 (      CPU_FNCT_PTR    fnct,
                                                  void           *obj,
                                                  NET_TMR_TICK    time,
                                                  NET_ERR        *p_err);

void            NetTmr_Free                (      NET_TMR        *p_tmr);


                                                                /* ------------------ TMR API FNCTS ------------------- */
void            NetTmr_Set                 (      NET_TMR        *p_tmr,
                                                  CPU_FNCT_PTR    fnct,
                                                  NET_TMR_TICK    time,
                                                  NET_ERR        *p_err);


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
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  NET_TMR_CFG_NBR_TMR
#error  "NET_TMR_CFG_NBR_TMR          not #define'd in 'net_cfg.h'"
#error  "                       [MUST be  >= NET_TMR_NBR_MIN]     "
#error  "                       [     &&  <= NET_TMR_NBR_MAX]     "

#elif   (DEF_CHK_VAL(NET_TMR_CFG_NBR_TMR,      \
                     NET_TMR_NBR_MIN,          \
                     NET_TMR_NBR_MAX) != DEF_OK)
#error  "NET_TMR_CFG_NBR_TMR    illegally #define'd in 'net_cfg.h'"
#error  "                       [MUST be  >= NET_TMR_NBR_MIN]     "
#error  "                       [     &&  <= NET_TMR_NBR_MAX]     "
#endif




#ifndef  NET_TMR_CFG_TASK_FREQ
#error  "NET_TMR_CFG_TASK_FREQ        not #define'd in 'net_cfg.h'"
#error  "                       [MUST be  > 0 Hz]                 "

#elif   (DEF_CHK_VAL_MIN(NET_TMR_CFG_TASK_FREQ, 1) != DEF_OK)
#error  "NET_TMR_CFG_TASK_FREQ  illegally #define'd in 'net_cfg.h'"
#error  "                       [MUST be  > 0 Hz]                 "
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_TMR_MODULE_PRESENT */
