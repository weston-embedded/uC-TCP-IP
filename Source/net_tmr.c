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
* Filename : net_tmr.c
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
#define    NET_TMR_MODULE
#include  "net.h"
#include  "net_tmr.h"
#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* -------------------- TASK NAMES -------------------- */
#define  NET_TMR_TASK_NAME                  "Net Tmr Task"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  KAL_TASK_HANDLE  NetTmr_TaskHandle;

static  NET_TMR         NetTmr_Tbl[NET_TMR_CFG_NBR_TMR];
static  NET_TMR        *NetTmr_PoolPtr;                    /* Ptr to pool of free net tmrs.                        */
static  NET_STAT_POOL   NetTmr_PoolStat;

static  NET_TMR        *NetTmr_TaskListHead;               /* Ptr to head of Tmr Task List.                        */
static  NET_TMR        *NetTmr_TaskListPtr;                /* Ptr to cur     Tmr Task List tmr to update.          */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static  void  NetTmr_TaskInit      (const  NET_TASK_CFG  *p_tmr_task_cfg,
                                           NET_ERR       *p_err);

static  void  NetTmr_Task          (       void          *p_data);

static  void  NetTmr_Clr           (       NET_TMR       *p_tmr);


/*
*********************************************************************************************************
*                                            NetTmr_Init()
*
* Description : (1) Initialize Network Timer Management Module :
*
*                   (a) Perform Timer Module/OS initialization
*                   (b) Initialize timer pool
*                   (c) Initialize timer table
*                   (d) Initialize timer task list pointer
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_TMR_ERR_NONE                Network timer module successfully initialized.
*
*                               ------------ RETURNED BY NetTmr_TaskInit() : -----------
*                               See NetTmr_TaskInit() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) The following network timer initialization MUST be sequenced as follows :
*
*                   (a) NetTmr_Init()      MUST precede ALL other network timer initialization functions
*                   (b) Network timer pool MUST be initialized PRIOR to initializing the pool with pointers
*                            to timers
*********************************************************************************************************
*/

void  NetTmr_Init (const  NET_TASK_CFG  *p_tmr_task_cfg,
                          NET_ERR       *p_err)
{
    NET_TMR      *p_tmr;
    NET_TMR_QTY   i;
    NET_ERR       err;


                                                                /* --------------- PERFORM TMR/OS INIT ---------------- */
    NetTmr_TaskInit(p_tmr_task_cfg, p_err);                     /* Create Tmr Task       (see Note #2a).                */
    if (*p_err != NET_TMR_ERR_NONE) {
         return;
    }


                                                                /* ------------------ INIT TMR POOL ------------------- */
    NetTmr_PoolPtr = DEF_NULL;                                  /* Init-clr net tmr pool (see Note #2b).                */

    NetStat_PoolInit((NET_STAT_POOL   *)&NetTmr_PoolStat,
                     (NET_STAT_POOL_QTY) NET_TMR_CFG_NBR_TMR,
                     (NET_ERR         *)&err);


                                                                /* ------------------ INIT TMR TBL -------------------- */
    p_tmr = &NetTmr_Tbl[0];
    for (i = 0u; i < NET_TMR_CFG_NBR_TMR; i++) {
        NetTmr_Clr(p_tmr);

        p_tmr->NextPtr  = NetTmr_PoolPtr;                       /* Free each tmr to tmr pool (see Note #2).             */
        NetTmr_PoolPtr = p_tmr;

        p_tmr++;
    }


                                                                /* -------------- INIT TMR TASK LIST PTR -------------- */
    NetTmr_TaskListHead = DEF_NULL;
    NetTmr_TaskListPtr  = DEF_NULL;


   *p_err = NET_TMR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetTmr_TaskInit()
*
* Description : (1) Perform Timer/OS initialization :
*
*                   (a) Validate network timer/OS configuration :
*
*                       (1) OS ticker / Network Timer Task frequency
*
*                   (b) Create Network Timer Task
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_TMR_ERR_NONE                    Network timer/OS initialization successful.
*                               NET_TMR_ERR_INIT_TASK_INVALID_FREQ  Invalid OS ticker frequency configured; MUST
*                                                                       be greater than (or equal to) configured
*                                                                       network timer task frequency.
*                               NET_TMR_ERR_INIT_TASK_INVALID_ARG   Invalid argument to init timer task.
*                               NET_ERR_FAULT_MEM_ALLOC             Error in memory allocation.
*                               NET_ERR_FAULT_UNKNOWN_ERR           Unknown error code encounter.
*                               NET_TMR_ERR_INIT_TASK_CREATE        Network timer task NOT successfully
*                                                                       initialized.
*
* Return(s)   : none.
*
* Caller(s)   : NetTmr_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetTmr_TaskInit (const  NET_TASK_CFG  *p_tmr_task_cfg,
                                      NET_ERR       *p_err)
{
    KAL_ERR  err_kal;



                                                                /* ----- VALIDATE NETWORK TIMER/OS CONFIGURATION ------ */
    if (KAL_TickRate < NET_TMR_CFG_TASK_FREQ) {                 /* If OS ticker frequency < network timer task ...      */
       *p_err = NET_TMR_ERR_INIT_TASK_INVALID_FREQ;             /* ... frequency, return error (see Note #1a1).         */
        return;
    }


                                                                /* ------------ CREATE NETWORK TIMER TASK ------------- */
    NetTmr_TaskHandle = KAL_TaskAlloc((const  CPU_CHAR *)NET_TMR_TASK_NAME,
                                                         p_tmr_task_cfg->StkPtr,
                                                         p_tmr_task_cfg->StkSizeBytes,
                                                         DEF_NULL,
                                                        &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_INVALID_ARG:
            *p_err = NET_TMR_ERR_INIT_TASK_INVALID_ARG;
             return;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_ERR_FAULT_MEM_ALLOC;
             return;


        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             return;
    }



    KAL_TaskCreate(NetTmr_TaskHandle,
                   NetTmr_Task,
                   DEF_NULL,
                   p_tmr_task_cfg->Prio,
                   DEF_NULL,
                  &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ISR:
        case KAL_ERR_OS:
            *p_err = NET_TMR_ERR_INIT_TASK_CREATE;
             return;


        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             return;
    }


   *p_err = NET_TMR_ERR_NONE;
}

/*
*********************************************************************************************************
*                                            NetTmr_Task()
*
* Description : Shell task to schedule & run Timer Task handler.
*
*               (1) Shell task's primary purpose is to schedule & run NetTmr_TaskHandler(); shell task
*                   should run NetTmr_TaskHandler() at NET_TMR_CFG_TASK_FREQ rate forever (i.e. shell
*                   task should NEVER exit).
*
*
* Argument(s) : p_data      Pointer to task initialization data.
*
* Return(s)   : none.
*
* Created by  : NetTmr_Init().
*
* Note(s)     : (2) Assumes KAL_TickRate frequency is greater than NET_TMR_CFG_TASK_FREQ. Otherwise,
*                   timer task scheduling rate will NOT be correct.
*
*               (3) Timer task MUST delay without failure.
*
*                   (a) Failure to delay timer task will prevent some network task(s)/operation(s) from
*                       functioning correctly.  Thus, timer task is assumed to be successfully delayed
*                       since NO error handling could be performed to counteract failure.
*********************************************************************************************************
*/

static  void  NetTmr_Task (void  *p_data)
{
    KAL_TICK  dly;


   (void)&p_data;                                               /* Prevent 'variable unused' compiler warning.          */

    dly = KAL_TickRate / NET_TMR_CFG_TASK_FREQ;                 /* Delay task at NET_TMR_CFG_TASK_FREQ rate.            */

    while (DEF_ON) {
        KAL_DlyTick(dly, KAL_OPT_DLY_PERIODIC);
        NetTmr_TaskHandler();
    }
}

/*
*********************************************************************************************************
*                                         NetTmr_TaskHandler()
*
* Description : (1) Handle network timers in the Timer Task List :
*
*                   (a) Acquire network lock                                            See Note #4
*
*                   (b) Handle every  network timer in Timer Task List :
*                       (1) Decrement network timer(s)
*                       (2) For any timer that expires :                                See Note #8
*                           (A) Execute timer's callback function
*                           (B) Free from Timer Task List
*
*                   (c) Release network lock
*
*
*               (2) (a) Timers are managed in a doubly-linked Timer List.
*
*                       (1) 'NetTmr_TaskListHead' points to the head of the Timer List.
*
*                       (2) Timers' 'PrevPtr' & 'NextPtr' doubly-link each timer to form the Timer List.
*
*                   (b) New timers are added at the head of the Timer List.
*
*                   (c) As timers are added into the list, older timers migrate to the tail of the Timer
*                       List.  Once a timer expires or is discarded, it is removed from the Timer List.
*
*
*                                        |                                               |
*                                        |<-------------- List of Timers --------------->|
*                                        |                (see Note #2a)                 |
*
*                                      New Timers
*                                   inserted at head                         Oldest Timer in List
*                                    (see Note #2b)                            (see Note #2c)
*
*                                           |                 NextPtr                 |
*                                           |             (see Note #2a2)             |
*                                           v                    |                    v
*                                                                |
*                        Head of         -------       -------   v   -------       -------
*                       Timer List  ---->|     |------>|     |------>|     |------>|     |
*                                        |     |<------|     |<------|     |<------|     |
*                    (see Note #2a1)     -------       -------   ^   -------       -------
*                                                                |
*                                                                |
*                                                             PrevPtr
*                                                         (see Note #2a2)
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : NetTmr_Task().
*
*               This function is a network protocol suite to operating system (OS) function & SHOULD be
*               called only by appropriate network-operating system port function(s).
*
* Note(s)     : (3) NetTmr_TaskHandler() blocked until network initialization completes.
*
*               (4) NetTmr_TaskHandler() blocks ALL other network protocol tasks by pending on & acquiring
*                   the global network lock (see 'net.h  Note #3').
*
*               (5) (a) NetTmr_TaskHandler() handles all valid timers in Timer Task List, up to the first
*                       corrupted timer in the Timer Task List, if any.
*
*                   (b) If ANY timer(s) in Timer Task List are corrupted :
*
*                       (1) Discard/unlink current Timer Task timer.
*                           (A) Consequently, any remaining valid timers in Timer Task List are :
*                               (1) Unlinked from Timer Task List, ...
*                               (2) NOT handled.
*
*                       (2) Timer Task is aborted.
*
*               (6) Since NetTmr_TaskHandler() is asynchronous to NetTmr_Free() [via execution of certain
*                   timer callback functions], the Timer Task List timer ('NetTmr_TaskListPtr') MUST be
*                   coordinated with NetTmr_Free() to avoid Timer Task List corruption :
*
*                   (a) (1) Timer Task List timer is typically advanced by NetTmr_TaskHandler() to the next
*                           timer in the Timer Task List.
*
*                       (2) However, whenever the Timer Task List timer is freed by an asynchronous timer
*                           callback function, the Timer Task List timer MUST be advanced to the next
*                           valid & available timer in the Timer Task List.
*
*                           See also 'NetTmr_Free()  Note #3a'.
*
*                   (b) Timer Task List timer MUST be cleared after handling the Timer Task List.
*
*                       (1) However, Timer Task List timer is implicitly cleared after handling the
*                           Timer Task List.
*
*
*               (7) Since NetTmr_TaskHandler() is asynchronous to ANY timer Get/Set, one additional tick
*                   is added to each timer's count-down so that the requested timeout is ALWAYS satisfied.
*                   This additional tick is added by NOT checking for zero ticks after decrementing; any
*                   timer that expires is recognized at the next tick.
*
*               (8) When a network timer expires, the timer SHOULD be freed PRIOR to executing the timer
*                   callback function.  This ensures that at least one timer is available if the timer
*                   callback function requires a timer.
*********************************************************************************************************
*/

void  NetTmr_TaskHandler (void)
{
    NET_TMR       *p_tmr;
    void          *obj;
    CPU_FNCT_PTR   fnct;
    NET_ERR        err;
    CPU_SR_ALLOC();


    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        Net_InitCompWait(&err);                                 /* ... wait on net init (see Note #3).                  */
        if (err != NET_ERR_NONE) {
            return;
        }
    }

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetTmr_TaskHandler, &err);   /* See Note #4.                                         */
    if (err != NET_ERR_NONE) {
        return;                                                 /* Could not acquire the Global Network Lock.           */
    }

                                                                /* --------------- HANDLE TMR TASK LIST --------------- */
    CPU_CRITICAL_ENTER();
    NetTmr_TaskListPtr = NetTmr_TaskListHead;                   /* Start @ Tmr Task List head.                          */
    p_tmr              = NetTmr_TaskListPtr;
    CPU_CRITICAL_EXIT();

    while (p_tmr != DEF_NULL) {                                 /* Handle  Tmr Task List tmrs (see Note #5a).           */
        CPU_CRITICAL_ENTER();
        NetTmr_TaskListPtr = NetTmr_TaskListPtr->NextPtr;       /* Set next tmr to update (see Note #6a1).              */

        if (p_tmr->TmrVal > 0) {                                /* If tmr val > 0, dec tmr val (see Note #7).           */
            p_tmr->TmrVal--;

        } else {                                                /* Else tmr expired;           ...                      */

            obj  = p_tmr->Obj;                                  /* Get obj for ...                                      */
            fnct = p_tmr->Fnct;                                 /* ... tmr callback fnct.                               */

            NetTmr_Free(p_tmr);                                 /* ... free tmr (see Note #8); ...                      */

            CPU_CRITICAL_EXIT();
            if (fnct != DEF_NULL) {                             /* ... & if avail,             ...                      */
                fnct(obj);                                      /* ... exec tmr callback fnct.                          */

            }
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
            else {
                NET_CTR_ERR_INC(Net_ErrCtrs.Tmr.NotUsedCtr);    /* A timer without a callback is considered unused.     */
            }
#endif
            CPU_CRITICAL_ENTER();
        }
        p_tmr = NetTmr_TaskListPtr;
        CPU_CRITICAL_EXIT();
    }
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
   Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                            NetTmr_Get()
*
* Description : (1) Allocate & initialize a network timer :
*
*                   (a) Get        timer
*                   (b) Validate   timer
*                   (c) Initialize timer
*                   (d) Insert     timer at head of Timer Task List
*                   (e) Update timer pool statistics
*                   (f) Return pointer to timer
*                         OR
*                       Null pointer & error code, on failure
*
*               (2) The timer pool is implemented as a stack :
*
*                   (a) 'NetTmr_PoolPtr' points to the head of the timer pool.
*
*                   (b) Timers' 'NextPtr's link each timer to form   the timer pool stack.
*
*                   (c) Timers are inserted & removed at the head of the timer pool stack.
*
*
*                                        Timers are
*                                    inserted & removed
*                                        at the head
*                                      (see Note #2c)
*
*                                             |                NextPtr
*                                             |            (see Note #2b)
*                                             v                   |
*                                                                 |
*                                          -------       -------  v    -------       -------
*                         Timer Pool  ---->|     |------>|     |------>|     |------>|     |
*                          Pointer         |     |       |     |       |     |       |     |
*                                          |     |       |     |       |     |       |     |
*                       (see Note #2a)     -------       -------       -------       -------
*
*                                          |                                               |
*                                          |<------------ Pool of Free Timers ------------>|
*                                          |                 (see Note #2)                 |
*
*
* Argument(s) : fnct        Pointer to callback function to execute when timer expires (see Note #3).
*
*               obj         Pointer to object that requests a timer (MAY be NULL).
*
*               time        Initial timer value (in 'NET_TMR_TICK' ticks) [see Note #4].
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_TMR_ERR_NONE                Network timer successfully allocated & initialized.
*                               NET_ERR_FAULT_NULL_FNCT         Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_NONE_AVAIL          NO available timers to allocate.
*                               NET_TMR_ERR_INVALID_TYPE        Network timer is NOT a valid timer type.
*
* Return(s)   : Pointer to network timer, if NO error(s).
*
*               Pointer to NULL,          otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (3) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
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
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); & ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers.
*
*               (4) Timer value of 0 ticks/seconds allowed; next tick will expire timer.
*
*                   See also 'NetTmr_TaskHandler()  Note #7'.
*
*               (5) See 'NetTmr_TaskHandler()  Note #2b'.
*********************************************************************************************************
*/

NET_TMR  *NetTmr_Get (CPU_FNCT_PTR    fnct,
                      void           *obj,
                      NET_TMR_TICK    time,
                      NET_ERR        *p_err)
{
    NET_TMR  *p_tmr;
    NET_ERR   err;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE FNCT PTR ----------------- */
    if (fnct == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Tmr.NullFnctCtr);
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return (DEF_NULL);
    }
#endif

    CPU_CRITICAL_ENTER();
                                                                /* --------------------- GET TMR ---------------------- */
    if (NetTmr_PoolPtr != DEF_NULL) {                           /* If tmr pool NOT empty, get tmr from pool.            */
        p_tmr           = (NET_TMR *)NetTmr_PoolPtr;
        NetTmr_PoolPtr  = (NET_TMR *)p_tmr->NextPtr;

    } else {                                                    /* If none avail, rtn err.                              */
        NET_CTR_ERR_INC(Net_ErrCtrs.Tmr.NoneAvailCtr);
        CPU_CRITICAL_EXIT();
       *p_err = NET_TMR_ERR_NONE_AVAIL;
        return (DEF_NULL);
    }

    if (p_tmr->Fnct != DEF_NULL) {                              /* A timer fresh from the pool should never indicate... */
        CPU_CRITICAL_EXIT();
       *p_err = NET_TMR_ERR_IN_USE;                             /* ...that it is in use.                                */
        return (DEF_NULL);
    }

                                                                /* --------------------- INIT TMR --------------------- */
    p_tmr->PrevPtr = DEF_NULL;
    p_tmr->NextPtr = (NET_TMR *)NetTmr_TaskListHead;
    p_tmr->Obj     =  obj;
    p_tmr->Fnct    =  fnct;
    p_tmr->TmrVal  =  time;                                     /* Set tmr val (in ticks).                              */

                                                                /* ---------- INSERT TMR INTO TMR TASK LIST ----------- */
    if (NetTmr_TaskListHead != DEF_NULL) {                      /* If list NOT empty, insert before head.               */
        NetTmr_TaskListHead->PrevPtr = p_tmr;
    }
    NetTmr_TaskListHead = p_tmr;                                /* Insert tmr @ list head (see Note #5).                */

                                                                /* --------------- UPDATE TMR POOL STATS -------------- */
    CPU_CRITICAL_EXIT();

    NetStat_PoolEntryUsedInc(&NetTmr_PoolStat, &err);

   *p_err =  NET_TMR_ERR_NONE;

    return (p_tmr);                                             /* --------------------- RTN TMR ---------------------- */
}


/*
*********************************************************************************************************
*                                            NetTmr_Free()
*
* Description : (1) Free a network timer :
*
*                   (a) Remove timer from Timer Task List
*                   (b) Clear  timer controls
*                   (c) Free   timer back to timer pool
*                   (d) Update timer pool statistics
*
*
* Argument(s) : p_tmr       Pointer to a network timer.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) #### To prevent freeing a timer already freed via previous timer free, NetTmr_Free()
*                   checks the'.Fnct' field in the timer structure BEFORE freeing the timer.
*
*                   This prevention is only best-effort since any invalid duplicate timer frees MAY be
*                   asynchronous to potentially valid timer gets.  Thus the invalid timer free(s) MAY
*                   corrupt the timer's valid operation(s).
*
*                   However, since the primary tasks of the network protocol suite are prevented from
*                   running concurrently (see 'net.h  Note #3'), it is NOT necessary to protect network
*                   timer resources from possible corruption since no asynchronous access from other
*                   network tasks is possible.
*
*               (3) Since NetTmr_TaskHandler() is asynchronous to NetTmr_Free() [via execution of certain
*                   timer callback functions], the Timer Task List timer ('NetTmr_TaskListPtr') MUST be
*                   coordinated with NetTmr_Free() to avoid Timer Task List corruption :
*
*                   (a) Whenever the Timer Task List timer is freed, the Timer Task List timer MUST be
*                       advanced to the next valid & available timer in the Timer Task List.
*
*                       See also 'NetTmr_TaskHandler()  Note #6a2'.
*********************************************************************************************************
*/

void  NetTmr_Free (NET_TMR  *p_tmr)
{
    NET_TMR  *p_tmr_prev;
    NET_TMR  *p_tmr_next;
    NET_ERR   err;
    CPU_SR_ALLOC();

                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_tmr == DEF_NULL) {
        return;
    }

    CPU_CRITICAL_ENTER();

    if (p_tmr->Fnct == DEF_NULL) {                              /* Prevent situation where timer might get doubly freed.*/
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        NET_CTR_ERR_INC(Net_ErrCtrs.Tmr.NotUsedCtr);
#endif
        CPU_CRITICAL_EXIT();
        return;                                                 /* Timer has already been freed. (see Note #2).         */
    }

                                                                /* ----------- REMOVE TMR FROM TMR TASK LIST ---------- */
    if (p_tmr == NetTmr_TaskListPtr) {                          /* If tmr is next Tmr Task tmr to update, ...           */
        p_tmr_next          = NetTmr_TaskListPtr->NextPtr;
        NetTmr_TaskListPtr  = p_tmr_next;                       /* ... adv Tmr Task ptr to skip this tmr (see Note #3a).*/
    }

    p_tmr_prev = p_tmr->PrevPtr;
    p_tmr_next = p_tmr->NextPtr;
    if (p_tmr_prev != DEF_NULL) {                               /* If tmr is NOT    the head of Tmr Task List, ...      */
        p_tmr_prev->NextPtr  = p_tmr_next;                      /* ...  set prev tmr to skip tmr.                       */
    } else {                                                    /* Else set next tmr as head of Tmr Task List.          */
        NetTmr_TaskListHead = p_tmr_next;

        if (p_tmr_next != DEF_NULL) {                           /* Clear the new head's prev tmr.                       */
            p_tmr_next->PrevPtr = DEF_NULL;
        }

    }
    if (p_tmr_next != DEF_NULL) {                               /* If tmr is NOT @  the tail of Tmr Task List, ...      */
        p_tmr_next->PrevPtr  = p_tmr_prev;                      /* ...  set next tmr to skip tmr.                       */
    } else {
        if (p_tmr_prev != DEF_NULL) {
            p_tmr_prev->NextPtr = DEF_NULL;                     /* Clear the new tail's next tmr.                       */
        }
    }

                                                                /* --------------------- FREE TMR --------------------- */
    p_tmr->NextPtr = NetTmr_PoolPtr;
    NetTmr_PoolPtr = p_tmr;
    p_tmr->Fnct    = (CPU_FNCT_PTR)0u;

                                                                /* -------------- UPDATE TMR POOL STATS --------------- */
    CPU_CRITICAL_EXIT();

    NetStat_PoolEntryUsedDec(&NetTmr_PoolStat, &err);
}


/*
*********************************************************************************************************
*                                            NetTmr_Set()
*
* Description : Update a network timer with a new callback function & timer value.
*
* Argument(s) : p_tmr        Pointer to a network timer.
*
*               fnct        Pointer to callback function to execute when timer expires (see Note #2).
*
*               time        Update timer value (in seconds expressed in 'NET_TMR_TICK' ticks)
*                               [see also Note #3].
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_TMR_ERR_NONE                Network timer time successfully updated.
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_tmr' passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT           Argument 'fnct' passed a NULL pointer.
*                               NET_TMR_ERR_INVALID_TYPE        Invalid timer type.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Assumes network timer is ALREADY owned by a valid network object.
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
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); & ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers.
*
*                   See also 'NetTmr_Get()  Note #3'.
*
*               (3) Timer value of 0 ticks/seconds allowed; next tick will expire timer.
*
*                   See also 'NetTmr_TaskHandler()  Note #7'.
*********************************************************************************************************
*/

void  NetTmr_Set (NET_TMR       *p_tmr,
                  CPU_FNCT_PTR   fnct,
                  NET_TMR_TICK   time,
                  NET_ERR       *p_err)
{
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTRS ------------------- */
    if (p_tmr == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Tmr.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (fnct == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Tmr.NullFnctCtr);
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

    CPU_CRITICAL_ENTER();

    if (p_tmr->Fnct == DEF_NULL) {
        CPU_CRITICAL_EXIT();
       *p_err = NET_ERR_FAULT_NULL_PTR;                         /* Trying to update a timer that's been freed           */
        return;
    }

    p_tmr->Fnct   = fnct;
    p_tmr->TmrVal = time;

    CPU_CRITICAL_EXIT();

   *p_err        = NET_TMR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetTmr_PoolStatGet()
*
* Description : Get network timer statistics pool.
*
* Argument(s) : none.
*
* Return(s)   : Network timer statistics pool, if NO error(s).
*
*               NULL          statistics pool, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) NetTmr_PoolStatGet() blocked until network initialization completes; return NULL
*                   statistics pool.
*
*               (2) 'NetTmr_PoolStat' MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

NET_STAT_POOL  NetTmr_PoolStatGet (void)
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
    stat_pool = NetTmr_PoolStat;
    CPU_CRITICAL_EXIT();

    return (stat_pool);
}


/*
*********************************************************************************************************
*                                    NetTmr_PoolStatResetMaxUsed()
*
* Description : Reset network timer statistics pool's maximum number of entries used.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) NetTmr_PoolStatResetMaxUsed() blocked until network initialization completes.
*
*                   (a) However, since 'NetTmr_PoolStat' is reset when network initialization completes;
*                       NO error is returned.
*********************************************************************************************************
*/

void  NetTmr_PoolStatResetMaxUsed (void)
{
    NET_ERR  err;

                                                                /* Acquire net lock.                                    */
    Net_GlobalLockAcquire((void *)&NetTmr_PoolStatResetMaxUsed, &err);
    if (err != NET_ERR_NONE) {
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        Net_GlobalLockRelease();                                /* ... rtn w/o err (see Note #1a).                      */
        return;
    }
#endif


    NetStat_PoolResetUsedMax(&NetTmr_PoolStat, &err);           /* Reset net tmr stat pool.                             */

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
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
*                                            NetTmr_Clr()
*
* Description : Clear network timer controls.
*
* Argument(s) : p_tmr       Pointer to a network timer.
*               ----        Argument validated in NetTmr_Init(),
*                                                 NetTmr_Free().
*
* Return(s)   : none.
*
* Caller(s)   : NetTmr_Init(),
*               NetTmr_Free().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetTmr_Clr (NET_TMR  *p_tmr)
{
    p_tmr->PrevPtr = DEF_NULL;
    p_tmr->NextPtr = DEF_NULL;
    p_tmr->Obj     = DEF_NULL;
    p_tmr->Fnct    = DEF_NULL;
    p_tmr->TmrVal  = NET_TMR_TIME_0S;
}
