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
*                                      NETWORK ICMP GENERIC LAYER
*                                 (INTERNET CONTROL MESSAGE PROTOCOL)
*
* Filename : net_icmp.c
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

#define    NET_ICMP_MODULE
#include  "net_icmp.h"
#include  "net_cfg_net.h"

#ifdef  NET_ICMPv4_MODULE_EN
#include  "../IP/IPv4/net_icmpv4.h"
#endif
#ifdef  NET_ICMPv6_MODULE_EN
#include  "../IP/IPv6/net_icmpv6.h"
#endif

#include  <KAL/kal.h>
#include  "net_type.h"
#include  "net_ip.h"
#include  "net_err.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_ICMP_LOCK_NAME                 "Net ICMP Lock"
#define  NET_ICMP_INTERNAL_DATA_SEG_NAME    "Net ICMP internal data"
#define  NET_ICMP_ECHO_REQ_SEM_NAME         "Net ICMP Echo Req Sem"
#define  NET_ICMP_ECHO_REQ_POOL_NAME        "Net ICMP echo req pool"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

typedef  struct  net_icmp_echo_req  NET_ICMP_ECHO_REQ;

struct  net_icmp_echo_req {
    KAL_SEM_HANDLE      Sem;
    CPU_INT16U          ID;
    CPU_INT16U          Seq;
    void               *SrcDataPtr;
    CPU_INT16U          SrcDataLen;
    CPU_BOOLEAN         DataCmp;
    NET_ICMP_ECHO_REQ  *PrevPtr;
    NET_ICMP_ECHO_REQ  *NextPtr;
};

typedef  struct  net_icmpv4_data {
    MEM_SEG            *MemSegPtr;                             /* Mem Seg to alloc from.                               */

    MEM_DYN_POOL        EchoReqPool;
    NET_ICMP_ECHO_REQ  *EchoReqHandleListStartPtr;
    NET_ICMP_ECHO_REQ  *EchoReqHandleListEndPtr;
} NET_ICMP_DATA;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  KAL_LOCK_HANDLE    NetICMP_Lock;
static  NET_ICMP_DATA     *NetICMP_DataPtr;
static  CPU_INT16U         NetICMP_ID;


/*
*********************************************************************************************************
*                                           NetICMP_Init()
*
* Description : (1) Initialize Internet Control Message Protocol Layer:
*
*                   (a) Initialize ICMP   OS objects
*                   (b) Initialize ICMPv4 variables if IPv4 is present.
*                   (c) Initialize ICMPv6 variables if IPv6 is present.
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

void  NetICMP_Init (NET_ERR  *p_err)
{
    MEM_SEG  *p_seg;
    LIB_ERR   err_lib;
    KAL_ERR   err_kal;


    NetICMP_ID      = 1u;
    NetICMP_DataPtr = DEF_NULL;

                                                                /* --------------- INITIALIZE ICMP LOCK --------------- */
                                                                /* Create ICMP lock signal ...                          */
                                                                /* ... with ICMP access available (see Note #1d1).      */
    NetICMP_Lock = KAL_LockCreate((const CPU_CHAR *)NET_ICMP_LOCK_NAME,
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
            *p_err = NET_ICMP_ERR_LOCK_CREATE;
             return;
    }


    p_seg = DEF_NULL;
    NetICMP_DataPtr = (NET_ICMP_DATA *)Mem_SegAlloc(NET_ICMP_INTERNAL_DATA_SEG_NAME,
                                                    p_seg,
                                                    sizeof(NET_ICMP_DATA),
                                                   &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }

    NetICMP_DataPtr->MemSegPtr                 = p_seg;
    NetICMP_DataPtr->EchoReqHandleListStartPtr = DEF_NULL;
    NetICMP_DataPtr->EchoReqHandleListEndPtr   = DEF_NULL;


    Mem_DynPoolCreate(NET_ICMP_ECHO_REQ_POOL_NAME,
                     &NetICMP_DataPtr->EchoReqPool,
                      NetICMP_DataPtr->MemSegPtr,
                      sizeof(NET_ICMP_ECHO_REQ),
                      sizeof(CPU_ALIGN),
                      0u,
                      LIB_MEM_BLK_QTY_UNLIMITED,
                     &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }


#ifdef  NET_ICMPv4_MODULE_EN
    NetICMPv4_Init(p_err);
    if (*p_err != NET_ICMPv4_ERR_NONE) {
         return;
    }
#endif

#ifdef  NET_ICMPv6_MODULE_EN
    NetICMPv6_Init();
#endif

   *p_err = NET_ICMP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetICMP_LockAcquire()
*
* Description : Acquire mutually exclusive access to ICMP layer.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMP_ERR_NONE               Network access     acquired.
*                               NET_ICMP_ERR_LOCK_ACQUIRE       Network access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) ICMP access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                       (1) Failure to acquire ICMP access will prevent network task(s)/operation(s)
*                           from functioning.
*
*                   (b) ICMP access MUST be acquired exclusively by only a single task at any one time.
*
*                   See also 'NetICMP_LockRelease()  Note #1'.
*********************************************************************************************************
*/

void  NetICMP_LockAcquire (NET_ERR  *p_err)
{
    KAL_ERR  err_kal;
                                                                /* Acquire exclusive ICMP access    (see Note #1b) ...  */
                                                                /* ... without timeout              (see Note #1a) ...  */
    KAL_LockAcquire(NetICMP_Lock, KAL_OPT_PEND_NONE, KAL_TIMEOUT_INFINITE, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_ICMP_ERR_NONE;
             break;


        case KAL_ERR_LOCK_OWNER:
            *p_err = NET_ICMP_ERR_LOCK_ACQUIRE;
             return;


        case KAL_ERR_NULL_PTR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ABORT:
        case KAL_ERR_TIMEOUT:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
            *p_err = NET_ICMP_ERR_LOCK_ACQUIRE;                 /* See Note #1a1.                                       */
             break;
    }
}

/*
*********************************************************************************************************
*                                              NetICMP_LockRelease()
*
* Description : Release mutually exclusive access to ICMP layer.
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
* Note(s)     : (1) ICMP access MUST be released--i.e. MUST unlock access without failure.
*
*                   (a) Failure to release ICMP access will prevent network task(s)/operation(s) from
*                       functioning.  Thus, ICMP access is assumed to be successfully released since
*                       NO OS error handling could be performed to counteract failure.
*
*                   See also 'NetICMP_LockAcquire()  Note #1'.
*********************************************************************************************************
*/

void  NetICMP_LockRelease (void)
{
    KAL_ERR  err_kal;


    KAL_LockRelease(NetICMP_Lock,  &err_kal);                   /* Release exclusive network access.                    */

   (void)&err_kal;                                               /* See Note #1a.                                        */
}


/*
*********************************************************************************************************
*                                          NetICMP_TxEchoReq()
*
* Description : Transmit an ICMPv4 or ICMPv6 echo request message.
*
* Argument(s) : p_addr_dest     Pointer to IP destination address to send the ICMP echo request.
*
*               addr_len        IP address length :
*                                   NET_IPv4_ADDR_SIZE
*                                   NET_IPv6_ADDR_SIZE
*
*               timeout_ms      Timeout value to wait for ICMP echo response.
*
*               p_data          Pointer to the data buffer to include in the ICMP echo request.
*
*               data_len        Number of data buffer octets to include in the ICMP echo request.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMP_ERR_NONE                       ICMP echo request send and response received with success.
*                               NET_ERR_INVALID_ADDR                    Invalid IP address.
*                               NET_ERR_FAULT_MEM_ALLOC                 Error with memory allocation.
*                               NET_ICMP_ERR_ECHO_REQ_TIMEOUT           Echo request timed out.
*                               NET_ICMP_ERR_ECHO_REQ_SIGNAL_FAULT      Error with the Echo response received signal.
*                               NET_ICMP_ERR_ECHO_REPLY_DATA_CMP_FAIL   Data received in echo response doesn't match data send.
*
*                               -- RETURNED BY NetICMP_LockAcquire() --
*                               See NetICMP_LockAcquire() for additional returned error codes.
*
*                               -- RETURNED BY NetICMPv4_TxEchoReq() --
*                               See NetICMPv4_TxEchoReq() for additional returned error codes.
*
*                               -- RETURNED BY NetICMPv6_TxEchoReq() --
*                               See NetICMPv6_TxEchoReq() for additional returned error codes.
*
* Return(s)   : DEF_OK,   if ICMP echo request message successfully sent to remote host.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application,
*               NetIxANVL_BgWkr(),
*               NetIxANVL_Ping(),
*               NetIxANVL_Ping6(),
*               NetCmd_Ping4(),
*               NetCmd_Ping6().
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetICMP_TxEchoReq (CPU_INT08U       *p_addr_dest,
                                NET_IP_ADDR_LEN   addr_len,
                                CPU_INT32U        timeout_ms,
                                void             *p_data,
                                CPU_INT16U        data_len,
                                NET_ERR          *p_err)
{
    NET_ICMP_ECHO_REQ  *p_echo_req_handle_new;
    NET_ICMP_ECHO_REQ  *p_echo_req_handle_end;
    CPU_INT16U          seq;
    CPU_BOOLEAN         result = DEF_FAIL;
    KAL_SEM_HANDLE      sem_handle;
    LIB_ERR             lib_err;
    KAL_ERR             kal_err;
    NET_ERR             net_lock_err;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if ((addr_len != NET_IPv4_ADDR_SIZE) &&
        (addr_len != NET_IPv6_ADDR_SIZE)) {
        *p_err = NET_ERR_INVALID_ADDR;
    }

#ifndef  NET_IPv4_MODULE_EN
    if (addr_len == NET_IPv4_ADDR_SIZE) {
       *p_err = NET_ERR_INVALID_ADDR;
        goto exit_return;
    }
#endif

#ifndef  NET_IPv6_MODULE_EN
    if (addr_len == NET_IPv6_ADDR_SIZE) {
       *p_err = NET_ERR_INVALID_ADDR;
        goto exit_return;
    }
#endif

#endif

    NetICMP_LockAcquire(&net_lock_err);
    if (net_lock_err != NET_ICMP_ERR_NONE) {
        result       = DEF_FAIL;
       *p_err        = net_lock_err;
        goto exit_return;
    }


    p_echo_req_handle_new = (NET_ICMP_ECHO_REQ *)Mem_DynPoolBlkGet(&NetICMP_DataPtr->EchoReqPool,
                                                                   &lib_err);
    if(lib_err != LIB_MEM_ERR_NONE) {
         result = DEF_FAIL;
        *p_err  = NET_ERR_FAULT_MEM_ALLOC;
         goto exit_release_lock;
    }

    sem_handle = KAL_SemCreate(NET_ICMP_ECHO_REQ_SEM_NAME, DEF_NULL, &kal_err);
    if (kal_err != KAL_ERR_NONE) {
        result = DEF_FAIL;
       *p_err  = NET_ERR_FAULT_MEM_ALLOC;
        goto exit_kal_fault;
    }

    p_echo_req_handle_new->Sem = sem_handle;

    if (NetICMP_DataPtr->EchoReqHandleListStartPtr == DEF_NULL) {
        p_echo_req_handle_new->NextPtr              = DEF_NULL;
        p_echo_req_handle_new->PrevPtr              = DEF_NULL;
        NetICMP_DataPtr->EchoReqHandleListStartPtr  = p_echo_req_handle_new;
        NetICMP_DataPtr->EchoReqHandleListEndPtr    = p_echo_req_handle_new;

    } else {
        p_echo_req_handle_end                    = NetICMP_DataPtr->EchoReqHandleListEndPtr;
        p_echo_req_handle_end->NextPtr           = p_echo_req_handle_new;

        p_echo_req_handle_new->PrevPtr           = p_echo_req_handle_end;
        p_echo_req_handle_new->NextPtr           = DEF_NULL;

        NetICMP_DataPtr->EchoReqHandleListEndPtr = p_echo_req_handle_new;
    }

    p_echo_req_handle_new->ID = NetICMP_ID;
    NetICMP_ID++;

    p_echo_req_handle_new->SrcDataPtr = p_data;
    p_echo_req_handle_new->SrcDataLen = data_len;
    p_echo_req_handle_new->DataCmp    = DEF_FAIL;


    if (addr_len == NET_IPv4_ADDR_SIZE) {
#ifdef  NET_ICMPv4_MODULE_EN
        seq = NetICMPv4_TxEchoReq((NET_IPv4_ADDR *)p_addr_dest,
                                                   p_echo_req_handle_new->ID,
                                                   p_data,
                                                   data_len,
                                                   p_err);
        if (*p_err != NET_ICMPv4_ERR_NONE) {
             NetICMP_LockRelease();
             result = DEF_FAIL;
             goto release;
        }

#else
        NetICMP_LockRelease();
        result = DEF_FAIL;
       *p_err  = NET_ERR_INVALID_ADDR;
        goto release;
#endif

    } else if (addr_len == NET_IPv6_ADDR_SIZE) {

#ifdef  NET_ICMPv6_MODULE_EN
        seq = NetICMPv6_TxEchoReq((NET_IPv6_ADDR *)p_addr_dest,
                                                   p_echo_req_handle_new->ID,
                                                   p_data,
                                                   data_len,
                                                   p_err);
        if (*p_err != NET_ICMPv6_ERR_NONE) {
             NetICMP_LockRelease();
             result = DEF_FAIL;
             goto release;
        }

#else
        NetICMP_LockRelease();
        result = DEF_FAIL;
       *p_err  = NET_ERR_INVALID_ADDR;
        goto release;
#endif

    } else {
         NetICMP_LockRelease();
         result = DEF_FAIL;
        *p_err = NET_ERR_INVALID_ADDR;
         goto release;
    }

    p_echo_req_handle_new->Seq = seq;

    NetICMP_LockRelease();

    KAL_SemPend(sem_handle, KAL_OPT_PEND_NONE, timeout_ms, &kal_err);
    switch (kal_err) {
        case KAL_ERR_NONE:
             result = DEF_OK;
            *p_err  = NET_ICMP_ERR_NONE;
             goto release;

        case KAL_ERR_TIMEOUT:
             result = DEF_FAIL;
            *p_err  = NET_ICMP_ERR_ECHO_REQ_TIMEOUT;
             goto release;

        case KAL_ERR_ABORT:
        case KAL_ERR_ISR:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
             result = DEF_FAIL;
            *p_err  = NET_ICMP_ERR_ECHO_REQ_SIGNAL_FAULT;
             goto release;
    }


release:
    NetICMP_LockAcquire(&net_lock_err);

    KAL_SemDel(sem_handle, &kal_err);

    if (*p_err == NET_ICMP_ERR_NONE) {
        if (data_len > 0) {
            if (p_echo_req_handle_new->DataCmp != DEF_OK) {
                *p_err = NET_ICMP_ERR_ECHO_REPLY_DATA_CMP_FAIL;
            }
        }
    }

    if ((NetICMP_DataPtr->EchoReqHandleListStartPtr == p_echo_req_handle_new) &&
        (NetICMP_DataPtr->EchoReqHandleListEndPtr   == p_echo_req_handle_new)) {

        NetICMP_DataPtr->EchoReqHandleListStartPtr = DEF_NULL;
        NetICMP_DataPtr->EchoReqHandleListEndPtr   = DEF_NULL;

    } else if (NetICMP_DataPtr->EchoReqHandleListStartPtr == p_echo_req_handle_new) {
        NetICMP_DataPtr->EchoReqHandleListStartPtr = p_echo_req_handle_new->NextPtr;

    } else if (NetICMP_DataPtr->EchoReqHandleListEndPtr == p_echo_req_handle_new) {
        NetICMP_DataPtr->EchoReqHandleListEndPtr = p_echo_req_handle_new->PrevPtr;

    } else {
        p_echo_req_handle_end          = p_echo_req_handle_new->PrevPtr;
        p_echo_req_handle_end->NextPtr = p_echo_req_handle_new->NextPtr;
    }

    p_echo_req_handle_new->NextPtr = DEF_NULL;
    p_echo_req_handle_new->PrevPtr = DEF_NULL;



exit_kal_fault:
    Mem_DynPoolBlkFree(&NetICMP_DataPtr->EchoReqPool,
                        p_echo_req_handle_new,
                       &lib_err);


exit_release_lock:
    NetICMP_LockRelease();


exit_return:
    return (result);
}


/*
*********************************************************************************************************
*                                         NetICMP_RxEchoReply()
*
* Description : (1) Received a ICMPv4 or ICMPv6 Echo Reply message.
*               (2) Compare date received with data sent.
*
*
* Argument(s) : id          ICMP message ID.
*
*               seq         ICMP sequence number.
*
*               p_data      Pointer to data received in the ICMP reply message.
*
*               data_len    ICMP reply message data length.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMP_ERR_NONE               ICMP echo reply received successfully.
*                               NET_ICMP_ERR_SIGNAL_FAULT       Error in posting ICMP request semaphore.
*                               NET_ICMP_ERR_ECHO_REPLY_RX      ICMP echo reply received does not match a echo
*                                                                   request send.
*
*                               ------------- RETURNED BY NetICMP_LockAcquire() -------------
*                               NET_ICMP_ERR_LOCK_ACQUIRE   Error in acquiring the ICMP lock.
*
* Return(s)   : None.
*
* Caller(s)   : NetICMPv4_RxReplyDemux(),
*               NetICMPv6_RxReplyDemux().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetICMP_RxEchoReply (CPU_INT16U   id,
                           CPU_INT16U   seq,
                           CPU_INT08U  *p_data,
                           CPU_INT16U   data_len,
                           NET_ERR     *p_err)
{
    NET_ICMP_ECHO_REQ  *p_icmp_echo_req;
    KAL_ERR             kal_err;


    NetICMP_LockAcquire(p_err);
    if (*p_err != NET_ICMP_ERR_NONE) {
         goto exit_fail;
    }

    p_icmp_echo_req = NetICMP_DataPtr->EchoReqHandleListStartPtr;
    while (p_icmp_echo_req != DEF_NULL) {
        if ((p_icmp_echo_req->ID  == id ) &&
            (p_icmp_echo_req->Seq == seq)) {

            if (p_icmp_echo_req->SrcDataPtr != DEF_NULL) {

                if (p_icmp_echo_req->SrcDataLen != data_len) {
                    p_icmp_echo_req->DataCmp = DEF_FAIL;
                } else {
                    p_icmp_echo_req->DataCmp = Mem_Cmp((void *)p_data,
                                                               p_icmp_echo_req->SrcDataPtr,
                                                               data_len);
                }

            } else if (data_len == 0u) {
                p_icmp_echo_req->DataCmp = DEF_OK;

            } else {
                p_icmp_echo_req->DataCmp = DEF_FAIL;
            }

            KAL_SemPost(p_icmp_echo_req->Sem, KAL_OPT_PEND_NONE, &kal_err);
            if (kal_err != KAL_ERR_NONE) {
               *p_err = NET_ICMP_ERR_SIGNAL_FAULT;
                goto exit_release_lock;
            }

           *p_err = NET_ICMP_ERR_NONE;
            goto exit_release_lock;
        }

        p_icmp_echo_req = p_icmp_echo_req->NextPtr;
    }

   *p_err = NET_ICMP_ERR_ECHO_REPLY_RX;

exit_release_lock:
    NetICMP_LockRelease();

exit_fail:
    return;
}

