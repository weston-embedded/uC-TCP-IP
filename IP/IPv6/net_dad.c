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
*                                           NETWORK DAD LAYER
*                                     (DUPLICATION ADDRESS DETECTION)
*
* Filename : net_dad.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Duplicate address detection as described in RFC #4862.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    NET_DAD_MODULE
#include  "net_dad.h"
#include  "net_ndp.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_DAD_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_DAD_OBJ_POOL_NAME                  "Net DAD Object Pool"
#define  NET_DAD_SIGNAL_ERR_NAME                "Net DAD Signal Err"
#define  NET_DAD_SIGNAL_COMPL_NAME              "Net DAD Signal Complete"


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

static  MEM_DYN_POOL             NetDAD_Pool;
static  NET_DAD_OBJ             *NetDAD_ObjListHeadPtr;
static  NET_DAD_OBJ             *NetDAD_ObjListEndPtr;

/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             NetDAD_Init()
*
* Description : Initialize DAD module
*
* Argument(s) : Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Init()
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetDAD_Init (NET_ERR  *p_err)
{
    LIB_ERR  err_lib;


                                                                /* -------- CREATE DYNAMIC POOL FOR DAD OBJECT -------- */
    Mem_DynPoolCreate(NET_DAD_OBJ_POOL_NAME,
                     &NetDAD_Pool,
                      DEF_NULL,
                      sizeof(NET_DAD_OBJ),
                      sizeof(CPU_DATA),
                      1u,
                      LIB_MEM_BLK_QTY_UNLIMITED,
                     &err_lib);
    if(err_lib != LIB_MEM_ERR_NONE) {
      *p_err = NET_DAD_ERR_OBJ_POOL_CREATE;
       return;
    }

   *p_err = NET_DAD_ERR_NONE;
}



/*
*********************************************************************************************************
*                                          NetDAD_Start()
*
* Description : (1) Start Duplication Address Detection (DAD) procedure :
*
*                   (a) Validate that DAD is enabled.
*                   (b) Validate that IPv6 addrs object for addr to configured exists.
*                   (c) Get a new DAD object in pool.
*                   (d) Update DAD object parameters.
*                   (e) Start NDP DAD process.
*
* Argument(s) : if_nbr          Network interface number.
*
*               p_addr          Pointer to the IPv6 addr to perform duplicate address detection.
*
*               addr_cfg_type   Type of Address Configuration :
*
*                                   NET_IPV6_ADDR_CFG_TYPE_STATIC_BLOCKING
*                                   NET_IPV6_ADDR_CFG_TYPE_STATIC_NO_BLOCKING
*                                   NET_IPV6_ADDR_CFG_TYPE_AUTO_CFG_NO_BLOCKING
*                                   NET_IPV6_ADDR_CFG_TYPE_RX_PREFIX_INFO
*
*               dad_hook_fnct   Hook function.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_DAD_ERR_NONE              DAD process finished successfully.
*                                   NET_ERR_FAULT_NULL_PTR        Argument(s) passed a NULL pointer.
*                                   NET_ERR_FAULT_FEATURE_DIS     DAD is disabled.
*                                   NET_DAD_ERR_ADDR_NOT_FOUND    Failed to get host address.
*                                   NET_DAD_ERR_FAULT             Failed to create NDP cache.
*                                   NET_DAD_ERR_FAILED            IPv6 address state is neither preferred or deprecated.
*                                   NET_DAD_ERR_IN_PROGRESS       DAD procedure still in progress.
*
*                                   --------------- RETURNED BY NetDAD_ObjGet() : ----------------
*                                   NET_DAD_ERR_OBJ_MEM_ALLOC     Error while allocating the memory block for DAD object.
*                                   NET_ERR_FAULT_MEM_ALLOC       Memory allocation error with DAD signal creation.
*                                   NET_DAD_ERR_SIGNAL_CREATE     Error with DAD signal creation.
*
*                                   ------------- RETURNED BY NetDAD_SignalWait() : --------------
*                                   NET_DAD_ERR_SIGNAL_FAULT      DAD signal faulted.
*                                   NET_DAD_ERR_SIGNAL_INVALID    Invalid DAD signal type.
*
*                                   ----------- RETURNED BY Net_GlobalLockAcquire() : ------------
*                                   NET_ERR_FAULT_LOCK_ACQUIRE    Network access not acquired.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_CfgAddrAddHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetDAD_Start (NET_IF_NBR               if_nbr,
                    NET_IPv6_ADDR           *p_addr,
                    NET_IPv6_ADDR_CFG_TYPE   addr_cfg_type,
                    NET_DAD_FNCT             dad_hook_fnct,
                    NET_ERR                 *p_err)
{
    NET_DAD_OBJ          *p_dad_obj;
    NET_IPv6_ADDRS       *p_ipv6_addrs;
    CPU_INT08U            dad_retx_nbr;
    NET_IPv6_ADDR_STATE   state;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

                                                                /* ----------- VALIDATE THAT DAD IS ENABLED ----------- */
    dad_retx_nbr = NetNDP_DAD_GetMaxAttemptsNbr();
    if (dad_retx_nbr <= 0) {
       *p_err = NET_ERR_FAULT_FEATURE_DIS;
        goto exit;
    }

                                                                /* -------------- RECOVER IPV6 ADDRS OBJ -------------- */
    p_ipv6_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_addr);
    if (p_ipv6_addrs == DEF_NULL) {
       *p_err = NET_DAD_ERR_ADDR_NOT_FOUND;
        goto exit;
    }

                                                                /* --------------- GET A NEW DAD OBJECT --------------- */
    p_dad_obj = NetDAD_ObjGet(p_err);
    if (*p_err != NET_DAD_ERR_NONE) {
         goto exit;
    }

                                                                /* ----------- UPDATE DAD OBJECT PARAMETERS ----------- */
    Mem_Copy(&p_dad_obj->Addr, p_addr, NET_IPv6_ADDR_SIZE);

    p_dad_obj->Fnct  = dad_hook_fnct;

    switch (addr_cfg_type) {
        case NET_IPv6_ADDR_CFG_TYPE_STATIC_BLOCKING:
             p_dad_obj->NotifyComplEn = DEF_YES;
             break;


        case NET_IPv6_ADDR_CFG_TYPE_STATIC_NO_BLOCKING:
             p_dad_obj->NotifyComplEn = DEF_NO;
             break;


        case NET_IPv6_ADDR_CFG_TYPE_AUTO_CFG_NO_BLOCKING:
#ifdef NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
             p_dad_obj->NotifyComplEn = DEF_NO;
#else
            *p_err = NET_ERR_FAULT_FEATURE_DIS;
             goto exit_release;
#endif
             break;


        case NET_IPv6_ADDR_CFG_TYPE_RX_PREFIX_INFO:
             p_dad_obj->NotifyComplEn = DEF_NO;
             break;


        default:
             break;
    }

                                                                /* ---------- CREATE NEW NDP CACHE FOR ADDR ----------- */
    NetNDP_DAD_Start(if_nbr, p_addr, p_err);
    if (*p_err != NET_NDP_ERR_NONE) {
        *p_err  = NET_DAD_ERR_FAULT;
         goto exit_release;
    }

                                                                /* ---- WAIT FOR DAD SIGNAL COMPLETE IF BLOCKING EN --- */
    if (addr_cfg_type == NET_IPv6_ADDR_CFG_TYPE_STATIC_BLOCKING) {

        Net_GlobalLockRelease();

        NetDAD_SignalWait(NET_DAD_SIGNAL_TYPE_COMPL, p_dad_obj, p_err);
        if (*p_err != NET_DAD_ERR_NONE) {
             goto exit_relock;
        }

        Net_GlobalLockAcquire((void *)&NetDAD_Start, p_err);
        if (*p_err != NET_ERR_NONE) {
             goto exit_release;
        }

        NetDAD_Stop(if_nbr, p_dad_obj);

        state = p_ipv6_addrs->AddrState;
        switch (state) {
            case NET_IPv6_ADDR_STATE_PREFERRED:
            case NET_IPv6_ADDR_STATE_DEPRECATED:
                *p_err = NET_DAD_ERR_NONE;
                 goto exit;


            case NET_IPv6_ADDR_STATE_DUPLICATED:
            case NET_IPv6_ADDR_STATE_TENTATIVE:
            case NET_IPv6_ADDR_STATE_NONE:
            default:
                *p_err = NET_DAD_ERR_FAILED;
                 goto exit;
        }
    }

   *p_err = NET_DAD_ERR_IN_PROGRESS;

    goto exit;


exit_relock:
{
    NET_ERR  local_err;

    Net_GlobalLockAcquire((void *)&NetDAD_Start, &local_err);
   (void)&local_err;
}

exit_release:
    NetDAD_Stop(if_nbr, p_dad_obj);

exit:
    return;
}


/*
*********************************************************************************************************
*                                          NetDAD_Stop()
*
* Description : Stop the current running DAD process.
*
* Argument(s) : if_nbr      Interface number of the address on which DAD is occurring.
*
*               p_dad_obj   Pointer to the current DAD object.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_AddrAutoCfgDAD_Result(),
*               NetIPv6_CfgAddrAddDAD_Result(),
*               NetIPv6_DAD_Start().
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  NetDAD_Stop(NET_IF_NBR    if_nbr,
                  NET_DAD_OBJ  *p_dad_obj)
{
    NET_ERR                  err_net;
    KAL_ERR                  err_kal;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    p_dad_obj->Fnct          = DEF_NULL;
    p_dad_obj->NotifyComplEn = DEF_NO;
    CPU_CRITICAL_EXIT();

    KAL_SemSet(p_dad_obj->SignalCompl, 0, &err_kal);
    KAL_SemSet(p_dad_obj->SignalErr,   0, &err_kal);

    NetNDP_DAD_Stop(if_nbr, &p_dad_obj->Addr);

    NetDAD_ObjRelease(p_dad_obj, &err_net);
}


/*
*********************************************************************************************************
*                                            NetDAD_ObjGet()
*
* Description : Obtain a DAD object.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DAD_ERR_NONE                DAD object successfully recovered.
*                               NET_DAD_ERR_OBJ_MEM_ALLOC       Error while allocating the memory block for DAD
*                                                                   object.
*                               NET_ERR_FAULT_MEM_ALLOC         Memory allocation error with DAD signal creation.
*                               NET_DAD_ERR_SIGNAL_CREATE       Error with DAD signal creation.
*
* Return(s)   : Pointer to new DAD object created.
*
* Caller(s)   : NetNDP_DAD_Start().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetDAD_ObjGet() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

NET_DAD_OBJ  *NetDAD_ObjGet (NET_ERR  *p_err)
{
    NET_DAD_OBJ   *p_obj;
    LIB_ERR        err_lib;
    KAL_ERR        err_kal;

                                                                /* ------------ GET DAD OBJ FROM DYN POOL ------------- */
    p_obj = (NET_DAD_OBJ *)Mem_DynPoolBlkGet(&NetDAD_Pool, &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = NET_DAD_ERR_OBJ_MEM_ALLOC;
            goto exit;
        }

                                                                /* ------------ INIT DAD OBJ SIGNAL ERROR ------------- */
    p_obj->SignalErr = KAL_SemCreate((const CPU_CHAR *)NET_DAD_SIGNAL_ERR_NAME,
                                                       DEF_NULL,
                                                      &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_ERR_FAULT_MEM_ALLOC;
             goto exit_release;


        default:
            *p_err = NET_DAD_ERR_SIGNAL_CREATE;
             goto exit_release;
    }

                                                                /* ----------- INIT DAD OBJ SIGNAL COMPLETE ----------- */
    p_obj->SignalCompl = KAL_SemCreate((const CPU_CHAR *)NET_DAD_SIGNAL_COMPL_NAME,
                                                         DEF_NULL,
                                                        &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;


        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_ERR_FAULT_MEM_ALLOC;
             goto exit_release;


        default:
            *p_err = NET_DAD_ERR_SIGNAL_CREATE;
             goto exit_release;
    }

                                                                /* ---------- INIT OTHER DAD OBJ PARAMETERS ---------- */
    NetIPv6_AddrUnspecifiedSet(&p_obj->Addr, p_err);
    p_obj->NotifyComplEn = DEF_NO;
    p_obj->Fnct          = DEF_NULL;


                                                                /* --------------- UPDATE DAD OBJ LIST ---------------- */
   if (NetDAD_ObjListHeadPtr == DEF_NULL) {
       NetDAD_ObjListHeadPtr  = p_obj;
       NetDAD_ObjListEndPtr   = p_obj;
   } else {
       NetDAD_ObjListEndPtr->NextPtr = p_obj;
       NetDAD_ObjListEndPtr          = p_obj;
   }


  *p_err = NET_DAD_ERR_NONE;

   goto exit;

exit_release:
    Mem_DynPoolBlkFree(&NetDAD_Pool, p_obj, &err_lib);
    p_obj = DEF_NULL;

exit:
    return (p_obj);
}


/*
*********************************************************************************************************
*                                        NetDAD_ObjRelease()
*
* Description : Release DAD object.
*
* Argument(s) : p_obj       Pointer to DAD object to release.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DAD_ERR_NONE                DAD object successfully released.
*                               NET_DAD_ERR_OBJ_NOT_FOUND       DAD object not found in list.
*
* Return(s)   : None.
*
* Caller(s)   : NetNDP_DAD_Stop().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetDAD_ObjRelease() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

void  NetDAD_ObjRelease (NET_DAD_OBJ  *p_dad_obj,
                         NET_ERR      *p_err)
{
    NET_DAD_OBJ  *p_obj_prev;
    NET_DAD_OBJ  *p_obj;
    CPU_BOOLEAN   found;
    KAL_ERR       err_kal;
    LIB_ERR       err_lib;

                                                                /* --------------- UPDATE DAD OBJ LIST ---------------- */
    found      = DEF_NO;
    p_obj_prev = DEF_NULL;
    p_obj      = NetDAD_ObjListHeadPtr;
    while (p_obj != DEF_NULL) {
        if (p_obj == p_dad_obj) {
            if (p_obj == NetDAD_ObjListHeadPtr) {

                NetDAD_ObjListHeadPtr = p_obj->NextPtr;

                if (NetDAD_ObjListEndPtr == p_obj) {
                    NetDAD_ObjListHeadPtr = DEF_NULL;
                    NetDAD_ObjListEndPtr  = DEF_NULL;
                }

            } else if (p_obj == NetDAD_ObjListEndPtr) {
                NetDAD_ObjListEndPtr = p_obj_prev;

            } else {
                p_obj_prev->NextPtr = p_obj->NextPtr;
            }

            found = DEF_YES;
            break;
        }
        p_obj_prev = p_obj;
        p_obj      = p_obj->NextPtr;
    }

    if (found == DEF_YES) {
                                                                /* ------------- RELEASE KAL SEMAPHORES -------------- */
        KAL_SemDel(p_dad_obj->SignalCompl, &err_kal);
        p_dad_obj->SignalCompl.SemObjPtr = DEF_NULL;

        KAL_SemDel(p_dad_obj->SignalErr, &err_kal);
        p_dad_obj->SignalErr.SemObjPtr = DEF_NULL;

                                                                /* ------------------- FREE DAD OBJ ------------------- */
        Mem_DynPoolBlkFree(&NetDAD_Pool, p_dad_obj, &err_lib);

       *p_err = NET_DAD_ERR_NONE;

    } else {
       *p_err = NET_DAD_ERR_OBJ_NOT_FOUND;
    }
}


/*
*********************************************************************************************************
*                                           NetDAD_ObjSrch()
*
* Description : Search DAD object with specific IPv6 address in DAD object list.
*
* Argument(s) : p_addr      Pointer to IPv6 address
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DAD_ERR_NONE                DAD object successfully found.
*                               NET_DAD_ERR_OBJ_NOT_FOUND       DAD object not found in list.
*
* Return(s)   : Pointer to DAD object found.
*
* Caller(s)   : NetIPv6_CfgAddrAdd(),
*               NetIPv6_CfgAddrResult(),
*               NetIPv6_AddrAutoCfgDAD_Result(),
*               NetNDP_RxNeighborSolicitation(),
*               NetNDP_RxNeighborAdvertisement(),
*               NetNDP_DAD_Timeout(),
*               NetNDP_CfgAddrResult().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetDAD_ObjSrch() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

NET_DAD_OBJ  *NetDAD_ObjSrch (NET_IPv6_ADDR  *p_addr,
                              NET_ERR        *p_err)
{
    NET_DAD_OBJ  *p_obj;
    CPU_BOOLEAN   identical;


    p_obj = NetDAD_ObjListHeadPtr;

    while (p_obj != DEF_NULL) {

        identical = NetIPv6_IsAddrsIdentical(p_addr, &p_obj->Addr);
        if (identical == DEF_YES) {
           *p_err = NET_DAD_ERR_NONE;
            return (p_obj);
        }

        p_obj      = p_obj->NextPtr;
    }

   *p_err = NET_DAD_ERR_OBJ_NOT_FOUND;
    return (DEF_NULL);
}


/*
*********************************************************************************************************
*                                          NetDAD_SignalWait()
*
* Description : Wait for a NDP DAD signal.
*
* Argument(s) : signal_type     DAD signal type :
*                                   NET_DAD_SIGNAL_TYPE_ERR
*                                   NET_DAD_SIGNAL_TYPE_COMPL
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DAD_ERR_NONE                DAD signal received successfully.
*                               NET_DAD_ERR_SIGNAL_FAULT        DAD signal faulted.
*                               NET_DAD_ERR_SIGNAL_INVALID      Invalid DAD signal type.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_CfgAddrAdd(),
*               NetNDP_DAD_Timeout().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) NetIPv6_DAD_SignalWait() is called by network protocol suite function(s) BUT
*                   MUST be called with the global network lock NOT acquired.
*********************************************************************************************************
*/

void  NetDAD_SignalWait (NET_DAD_SIGNAL_TYPE   signal_type,
                         NET_DAD_OBJ          *p_dad_obj,
                         NET_ERR              *p_err)
{
    KAL_ERR  err_kal;


    switch (signal_type) {
        case NET_DAD_SIGNAL_TYPE_ERR:
             KAL_SemPend(p_dad_obj->SignalErr, KAL_OPT_PEND_NON_BLOCKING, KAL_TIMEOUT_INFINITE, &err_kal);
             switch (err_kal) {
                case KAL_ERR_NONE:
                    *p_err = NET_DAD_ERR_NONE;
                     break;


                case KAL_ERR_ABORT:
                case KAL_ERR_ISR:
                case KAL_ERR_WOULD_BLOCK:
                case KAL_ERR_TIMEOUT:
                case KAL_ERR_OS:
                default:
                    *p_err = NET_DAD_ERR_SIGNAL_FAULT;
                     break;
             }
             break;


        case NET_DAD_SIGNAL_TYPE_COMPL:
             KAL_SemPend(p_dad_obj->SignalCompl, KAL_OPT_PEND_BLOCKING, NET_IPv6_DAD_SIGNAL_TIMEOUT_MS, &err_kal);
             switch (err_kal) {
                case KAL_ERR_NONE:
                    *p_err = NET_DAD_ERR_NONE;
                     break;


                case KAL_ERR_ABORT:
                case KAL_ERR_ISR:
                case KAL_ERR_WOULD_BLOCK:
                case KAL_ERR_TIMEOUT:
                case KAL_ERR_OS:
                default:
                    *p_err = NET_DAD_ERR_SIGNAL_FAULT;
                     break;
             }
             break;


        default:
            *p_err = NET_DAD_ERR_SIGNAL_INVALID;
             break;
    }
}


/*
*********************************************************************************************************
*                                           NetDAD_Signal()
*
* Description : Post a IPv6 DAD signal.
*
* Argument(s) : signal_type     DAD signal type :
*                                   NET_DAD_SIGNAL_TYPE_ERR
*                                   NET_DAD_SIGNAL_TYPE_COMPL
*
*               p_dad_obj       Pointer to the current DAD object.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_DAD_ERR_NONE            DAD signal posted successfully.
*                                   NET_DAD_ERR_SIGNAL_FAULT    DAD signal posting faulted.
*                                   NET_DAD_ERR_SIGNAL_INVALID  Invalid DAD signal type.
*
* Return(s)   : None.
*
* Caller(s)   : NetNDP_RxNeighborSolicitation(),
*               NetNDP_RxNeighborAdvertisement(),
*               NetNDP_DAD_Timeout().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) NetDAD_Signal() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

void  NetDAD_Signal (NET_DAD_SIGNAL_TYPE   signal_type,
                     NET_DAD_OBJ          *p_dad_obj,
                     NET_ERR              *p_err)
{
    KAL_ERR  err_kal;


    switch (signal_type) {
                                                                /* --------------- POST DAD SIGNAL ERR ---------------- */
        case NET_DAD_SIGNAL_TYPE_ERR:
             KAL_SemPost(p_dad_obj->SignalErr, KAL_OPT_PEND_NONE, &err_kal);
             switch (err_kal) {
                case KAL_ERR_NONE:
                    *p_err = NET_DAD_ERR_NONE;
                     break;


                case KAL_ERR_OVF:
                case KAL_ERR_OS:
                default:
                    *p_err = NET_DAD_ERR_SIGNAL_FAULT;
                     break;
             }
             break;


        case NET_DAD_SIGNAL_TYPE_COMPL:
             if (p_dad_obj->NotifyComplEn == DEF_YES) {
                 KAL_SemPost(p_dad_obj->SignalCompl, KAL_OPT_PEND_NONE, &err_kal);
                 switch (err_kal) {
                   case KAL_ERR_NONE:
                       *p_err = NET_DAD_ERR_NONE;
                        break;


                   case KAL_ERR_OVF:
                   case KAL_ERR_OS:
                   default:
                       *p_err = NET_DAD_ERR_SIGNAL_FAULT;
                        break;
                 }
             }
             break;


        default:
            *p_err = NET_DAD_ERR_SIGNAL_INVALID;
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

#endif  /* NET_DAD_MODULE_EN */
