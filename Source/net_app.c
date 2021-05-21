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
*                        NETWORK APPLICATION PROGRAMMING INTERFACE (API) LAYER
*
* Filename : net_app.c
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
#define    NET_APP_MODULE
#include  "net_app.h"
#include  "net_util.h"
#include  "net_ascii.h"
#ifdef  NET_EXT_MODULE_DNS_EN
#include  <Source/dns-c.h>
#endif

/*
*********************************************************************************************************
*                                          NetApp_SockOpen()
*
* Description : Open an application socket, with error handling.
*
* Argument(s) : protocol_family     Socket protocol family :
*
*                                       NET_SOCK_PROTOCOL_FAMILY_IP_V4  Internet Protocol version 4 (IPv4).
*                                       NET_SOCK_PROTOCOL_FAMILY_IP_V6  Internet Protocol version 6 (IPv6).
*
*                                   See also 'net_sock.c  Note #1a'.
*
*               sock_type           Socket type :
*
*                                       NET_SOCK_TYPE_DATAGRAM          Datagram-type socket.
*                                       NET_SOCK_TYPE_STREAM            Stream  -type socket.
*
*                                   See also 'net_sock.c  Note #1b'.
*
*               protocol            Socket protocol :
*
*                                       NET_SOCK_PROTOCOL_DFLT          Default protocol for socket type.
*                                       NET_SOCK_PROTOCOL_UDP           User Datagram        Protocol (UDP).
*                                       NET_SOCK_PROTOCOL_TCP           Transmission Control Protocol (TCP).
*
*                                   See also 'net_sock.c  Note #1c'.
*
*               retry_max           Maximum number of consecutive socket open retries   (see Note #2).
*
*               time_dly_ms         Transitory socket open delay value, in milliseconds (see Note #2).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application  socket successfully opened.
*                               NET_APP_ERR_NONE_AVAIL          NO available sockets to allocate.
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s) [see Note #1].
*                               NET_APP_ERR_FAULT               Socket open fault(s); open aborted.
*
* Return(s)   : Socket descriptor/handle identifier, if NO error(s).
*
*               NET_SOCK_BSD_ERR_OPEN,               otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (2) If a non-zero number of retries is requested then a non-zero time delay SHOULD also be
*                   requested; otherwise, all retries will most likely fail immediately since no time will
*                   elapse to wait for & allow socket operation(s) to successfully complete.
*********************************************************************************************************
*/

NET_SOCK_ID  NetApp_SockOpen (NET_SOCK_PROTOCOL_FAMILY   protocol_family,
                              NET_SOCK_TYPE              sock_type,
                              NET_SOCK_PROTOCOL          protocol,
                              CPU_INT16U                 retry_max,
                              CPU_INT32U                 time_dly_ms,
                              NET_ERR                   *p_err)
{
    NET_SOCK_ID  sock_id;
    CPU_INT16U   retry_cnt;
    CPU_BOOLEAN  done;
    CPU_BOOLEAN  dly;
    NET_ERR      err;
    NET_ERR      err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_SOCK_ID)0);
    }
#endif
                                                                /* ------------------ OPEN APP SOCK ------------------- */
    retry_cnt = 0u;
    done      = DEF_NO;
    dly       = DEF_NO;

    while ((retry_cnt <= retry_max) &&                          /* While open retry <= max retry ...                    */
           (done      == DEF_NO)) {                             /* ... & open NOT done,          ...                    */

        if (dly == DEF_YES) {                                   /* Dly open, on retries.                                */
            NetApp_TimeDly_ms(time_dly_ms, &err);
        }
                                                                /* ... open sock.                                       */
        sock_id = NetSock_Open((NET_SOCK_PROTOCOL_FAMILY) protocol_family,
                               (NET_SOCK_TYPE           ) sock_type,
                               (NET_SOCK_PROTOCOL       ) protocol,
                               (NET_ERR                *)&err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_NONE;
                 break;


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_SOCK_ERR_NONE_AVAIL:                       /* If transitory open err(s), ...                       */
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly retry.                                       */
                 err_rtn = NET_APP_ERR_NONE_AVAIL;
                 break;


            case NET_SOCK_ERR_INVALID_FAMILY:
            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_SOCK_ERR_INVALID_TYPE:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_ARG;             /* Rtn invalid arg err(s) [see Note #1].                */
                 break;


            default:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_FAULT;                   /* Rtn fatal err(s).                                    */
                 break;
        }
    }

   *p_err =  err_rtn;

    return (sock_id);
}


/*
*********************************************************************************************************
*                                          NetApp_SockClose()
*
* Description : (1) Close an application socket, with error handling :
*
*                   (a) Configure close timeout, if any
*                   (b) Close application socket
*
*
* Argument(s) : sock_id     Socket descriptor/handle identifier of application socket to close.
*
*               timeout_ms  Socket close timeout value :
*
*                               0,                              if current configured timeout value desired.
*                               NET_TMR_TIME_INFINITE,          if infinite (i.e. NO timeout) value desired.
*                               In number of milliseconds,      otherwise.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application socket successfully closed.
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s) [see Note #2].
*                               NET_APP_ERR_FAULT_TRANSITORY    Transitory   fault(s); close aborted but
*                                                                   MIGHT     close  in a subsequent attempt.
*                               NET_APP_ERR_FAULT               Socket close fault(s); close aborted AND
*                                                                   CANNOT be closed in a subsequent attempt.
*
* Return(s)   : DEF_OK,   application socket successfully closed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (3) (a) Once an application closes its socket, NO further operations on the socket are
*                       allowed & the application MUST NOT continue to access the socket.
*
*                       See also 'net_sock.c  NetSock_Close()  Note #2'.
*
*                   (b) NO error is returned for any internal error while closing the socket.
*
*                       See also 'net_sock.c  NetSock_Close()  Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetApp_SockClose (NET_SOCK_ID   sock_id,
                               CPU_INT32U    timeout_ms,
                               NET_ERR      *p_err)
{
    NET_SOCK_RTN_CODE  rtn_code;
    CPU_BOOLEAN        rtn_status;
    NET_ERR            err;
    NET_ERR            err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                /* ---------------- CFG CLOSE TIMEOUT ----------------- */
    if (timeout_ms > 0) {                                       /* If timeout avail, ...                                */
                                                                /* ... cfg close timeout.                               */
        NetSock_CfgTimeoutConnCloseSet(sock_id, timeout_ms, &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_ERR_INVALID_TIME:
                *p_err =  NET_APP_ERR_INVALID_ARG;
                 return (DEF_FAIL);


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                *p_err =  NET_APP_ERR_FAULT_TRANSITORY;
                 return (DEF_FAIL);


            default:
                *p_err =  NET_APP_ERR_FAULT;
                 return (DEF_FAIL);
        }
    }


                                                                /* ------------------ CLOSE APP SOCK ------------------ */
    rtn_code = NetSock_Close((NET_SOCK_ID) sock_id,
                             (NET_ERR   *)&err);
    switch (err) {
        case NET_SOCK_ERR_NONE:
        case NET_SOCK_ERR_CLOSED:
        case NET_SOCK_ERR_CONN_CLOSE_IN_PROGRESS:
             err_rtn = NET_APP_ERR_NONE;
             break;


        case NET_SOCK_ERR_INVALID_FAMILY:
        case NET_SOCK_ERR_INVALID_TYPE:
        case NET_SOCK_ERR_INVALID_STATE:
        case NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT:
        case NET_SOCK_ERR_CONN_FAIL:
        case NET_SOCK_ERR_FAULT:
        case NET_CONN_ERR_INVALID_CONN:
        case NET_CONN_ERR_NOT_USED:
             err_rtn = NET_APP_ERR_NONE;                        /* See Note #3b.                                        */
             break;


        case NET_SOCK_ERR_NOT_USED:
        case NET_SOCK_ERR_INVALID_SOCK:
             err_rtn = NET_APP_ERR_INVALID_ARG;                 /* Rtn invalid arg err(s) [see Note #2].                */
             break;


        case NET_INIT_ERR_NOT_COMPLETED:
        case NET_ERR_FAULT_LOCK_ACQUIRE:
             err_rtn = NET_APP_ERR_FAULT_TRANSITORY;            /* Rtn transitory  err(s).                              */
             break;


        default:
             err_rtn = NET_APP_ERR_FAULT;                       /* Rtn fatal       err(s).                              */
             break;
    }

    rtn_status = (rtn_code == NET_SOCK_BSD_ERR_NONE) ? DEF_OK : DEF_FAIL;
   *p_err       =  err_rtn;

    return (rtn_status);
}


/*
*********************************************************************************************************
*                                          NetApp_SockBind()
*
* Description : Bind an application socket to a local address, with error handling.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of application socket to bind to a
*                                   local address.
*
*               p_addr_local    Pointer to socket address structure (see Note #2).
*
*               addr_len        Length  of socket address structure (in octets).
*
*               retry_max       Maximum number of consecutive socket bind retries   (see Note #3).
*
*               time_dly_ms     Transitory socket bind delay value, in milliseconds (see Note #3).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application socket successfully bound
*                                                                   to a local address.
*                               NET_APP_ERR_NONE_AVAIL          NO available resources to bind socket
*                                                                   to a local address.
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s)  [see Note #1].
*                               NET_APP_ERR_INVALID_OP          Invalid operation(s) [see Note #1].
*                               NET_APP_ERR_FAULT               Socket bind fault(s); bind aborted &
*                                                                   socket SHOULD be closed.
*
* Return(s)   : DEF_OK,   application socket successfully bound to a local address.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (2) (a) Socket address structure 'AddrFamily' member MUST be configured in host-order &
*                       MUST NOT be converted to/from network-order.
*
*                   (b) Socket address structure addresses MUST be configured/converted from host-order
*                       to network-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'
*                          & 'net_sock.c  NetSock_Bind()                     Note #3'.
*
*               (3) If a non-zero number of retries is requested then a non-zero time delay SHOULD also be
*                   requested; otherwise, all retries will most likely fail immediately since no time will
*                   elapse to wait for & allow socket operation(s) to successfully complete.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetApp_SockBind (NET_SOCK_ID         sock_id,
                              NET_SOCK_ADDR      *p_addr_local,
                              NET_SOCK_ADDR_LEN   addr_len,
                              CPU_INT16U          retry_max,
                              CPU_INT32U          time_dly_ms,
                              NET_ERR            *p_err)
{
    NET_SOCK_RTN_CODE  rtn_code;
    CPU_BOOLEAN        rtn_status;
    CPU_BOOLEAN        done;
    CPU_BOOLEAN        dly;
    CPU_INT16U         retry_cnt;
    NET_ERR            err;
    NET_ERR            err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
#endif


                                                                /* ------------------ BIND APP SOCK ------------------- */
    retry_cnt = 0u;
    done      = DEF_NO;
    dly       = DEF_NO;

    while ((retry_cnt <= retry_max) &&                          /* While bind retry <= max retry ...                    */
           (done      == DEF_NO)) {                             /* ... & bind NOT done,          ...                    */

        if (dly == DEF_YES) {                                   /* Dly bind, on retries.                                */
            NetApp_TimeDly_ms(time_dly_ms, &err);
        }
                                                                /* ... bind sock.                                       */
        rtn_code = NetSock_Bind((NET_SOCK_ID      ) sock_id,
                                (NET_SOCK_ADDR   *) p_addr_local,
                                (NET_SOCK_ADDR_LEN) addr_len,
                                (NET_ERR         *)&err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_NONE;
                 break;


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_SOCK_ERR_ADDR_IN_USE:                      /* If transitory bind err(s), ...                       */
            case NET_SOCK_ERR_PORT_NBR_NONE_AVAIL:
            case NET_CONN_ERR_NONE_AVAIL:
            case NET_CONN_ERR_ADDR_IN_USE:
            case NET_IPv4_ERR_ADDR_NONE_AVAIL:
            case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly retry.                                       */
                 err_rtn = NET_APP_ERR_NONE_AVAIL;
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_SOCK_ERR_INVALID_ADDR:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_ARG;             /* Rtn invalid arg err(s) [see Note #1].                */
                 break;


            case NET_SOCK_ERR_INVALID_OP:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_OP;              /* Rtn invalid op  err(s).                              */
                 break;


            case NET_IF_ERR_INVALID_IF:
            case NET_SOCK_ERR_CLOSED:
            case NET_SOCK_ERR_INVALID_FAMILY:
            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_SOCK_ERR_INVALID_TYPE:
            case NET_SOCK_ERR_INVALID_STATE:
            case NET_SOCK_ERR_CONN_FAIL:
            case NET_ERR_FAULT_NULL_FNCT:
            case NET_CONN_ERR_NOT_USED:
            case NET_CONN_ERR_INVALID_CONN:
            case NET_CONN_ERR_INVALID_FAMILY:
            case NET_CONN_ERR_INVALID_TYPE:
            case NET_CONN_ERR_INVALID_PROTOCOL_IX:
            case NET_CONN_ERR_INVALID_ADDR_LEN:
            case NET_CONN_ERR_ADDR_NOT_USED:
            default:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_FAULT;                   /* Rtn fatal err(s).                                    */
                 break;
        }
    }

    rtn_status = (rtn_code == NET_SOCK_BSD_ERR_NONE) ? DEF_OK : DEF_FAIL;
   *p_err       =  err_rtn;

    return (rtn_status);
}


/*
*********************************************************************************************************
*                                          NetApp_SockConn()
*
* Description : (1) Connect an application socket to a remote address, with error handling :
*
*                   (a) Configure connect timeout, if any
*                   (b) Connect application socket to remote address
*                   (c) Restore   connect timeout, if necessary
*
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of application socket to connect to
*                                   a remote address.
*
*               p_addr_remote   Pointer to socket address structure (see Note #3).
*
*               addr_len        Length  of socket address structure (in octets).
*
*               retry_max       Maximum number of consecutive socket connect retries   (see Note #4a1).
*
*               timeout_ms      Socket connect timeout value per attempt/retry         (see Note #4b1) :
*
*                                   0,                          if current configured timeout value desired.
*                                   NET_TMR_TIME_INFINITE,      if infinite (i.e. NO timeout) value desired.
*                                   In number of milliseconds,  otherwise.
*
*               time_dly_ms     Transitory socket connect delay value, in milliseconds (see Note #4b2).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application socket successfully connected
*                                                                   to a remote address.
*                               NET_APP_ERR_NONE_AVAIL          NO available resources to connect socket
*                                                                   to a remote address OR remote address
*                                                                   NOT available.
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s)  [see Note #2].
*                               NET_APP_ERR_INVALID_OP          Invalid operation(s) [see Note #2].
*                               NET_APP_ERR_FAULT_TRANSITORY    Transitory     fault(s); connect aborted but
*                                                                   MIGHT connect in a subsequent attempt.
*                               NET_APP_ERR_FAULT               Socket connect fault(s); connect aborted &
*                                                                   socket SHOULD be closed.
*
* Return(s)   : DEF_OK,   application socket successfully connected to a remote address.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (3) (a) Socket address structure 'AddrFamily' member MUST be configured in host-order &
*                       MUST NOT be converted to/from network-order.
*
*                   (b) Socket address structure addresses MUST be configured/converted from host-order
*                       to network-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'
*                          & 'net_sock.c  NetSock_Conn()                     Note #3'.
*
*               (4) (a) (1) If a non-zero number of retries is requested                AND ...
*                       (2) global socket blocking ('NET_SOCK_CFG_BLOCK_SEL') is configured ...
*                           for non-blocking operation ('NET_SOCK_BLOCK_SEL_NO_BLOCK');     ...
*
*                   (b) ... then one or more of the following SHOULD also be requested; otherwise, all
*                       retries will most likely fail immediately since no time will elapse to wait for
*                       & allow socket operation(s) to successfully complete :
*
*                       (1) A non-zero timeout
*                       (2) A non-zero time delay
*********************************************************************************************************
*/

CPU_BOOLEAN  NetApp_SockConn (NET_SOCK_ID         sock_id,
                              NET_SOCK_ADDR      *p_addr_remote,
                              NET_SOCK_ADDR_LEN   addr_len,
                              CPU_INT16U          retry_max,
                              CPU_INT32U          timeout_ms,
                              CPU_INT32U          time_dly_ms,
                              NET_ERR            *p_err)
{
    NET_SOCK_RTN_CODE  rtn_code;
    CPU_BOOLEAN        rtn_status;
    CPU_INT16U         retry_cnt;
    CPU_INT32U         timeout_ms_cfgd;
    CPU_BOOLEAN        timeout_cfgd;
    CPU_BOOLEAN        done;
    CPU_BOOLEAN        dly;
    NET_ERR            err;
    NET_ERR            err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                /* ----------------- CFG CONN TIMEOUT ----------------- */
    if (timeout_ms > 0) {                                       /* If timeout avail,         ...                        */
                                                                /* ... save cfg'd conn timeout ...                      */
        timeout_ms_cfgd = NetSock_CfgTimeoutConnReqGet_ms(sock_id, &err);
                                                                /* ... & cfg temp conn timeout.                         */
        NetSock_CfgTimeoutConnReqSet(sock_id, timeout_ms, &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 timeout_cfgd = DEF_YES;
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_ERR_INVALID_TIME:
                *p_err =  NET_APP_ERR_INVALID_ARG;
                 return (DEF_FAIL);


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                *p_err =  NET_APP_ERR_FAULT_TRANSITORY;
                 return (DEF_FAIL);


            default:
                *p_err =  NET_APP_ERR_FAULT;
                 return (DEF_FAIL);
        }

    } else {
        timeout_cfgd = DEF_NO;
    }


                                                                /* ------------------ CONN APP SOCK ------------------- */
    retry_cnt = 0u;
    done      = DEF_NO;
    dly       = DEF_NO;

    while ((retry_cnt <= retry_max) &&                          /* While conn retry <= max retry ...                    */
           (done      == DEF_NO)) {                             /* ... & conn NOT done,          ...                    */

        if (dly == DEF_YES) {                                   /* Dly conn, on retries.                                */
            NetApp_TimeDly_ms(time_dly_ms, &err);
        }
                                                                /* ... conn sock.                                       */
        rtn_code = NetSock_Conn((NET_SOCK_ID      ) sock_id,
                                (NET_SOCK_ADDR   *) p_addr_remote,
                                (NET_SOCK_ADDR_LEN)  addr_len,
                                (NET_ERR         *)&err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_NONE;
                 break;


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_SOCK_ERR_ADDR_IN_USE:                      /* If transitory conn err(s), ...                       */
            case NET_SOCK_ERR_CONN_IN_USE:
            case NET_SOCK_ERR_PORT_NBR_NONE_AVAIL:
            case NET_SOCK_ERR_CONN_IN_PROGRESS:
            case NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT:
            case NET_CONN_ERR_NONE_AVAIL:
            case NET_IPv4_ERR_ADDR_NONE_AVAIL:
            case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
            case NET_ERR_IF_LINK_DOWN:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly retry.                                       */
                 err_rtn = NET_APP_ERR_NONE_AVAIL;
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_TYPE:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_SOCK_ERR_INVALID_ADDR:
            case NET_SOCK_ERR_INVALID_ADDR_LEN:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_ARG;             /* Rtn invalid arg err(s) [see Note #2].                */
                 break;


            case NET_SOCK_ERR_INVALID_OP:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_OP;              /* Rtn invalid op  err(s).                              */
                 break;

            case NET_SOCK_ERR_CONN_FAIL:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_CONN_FAIL;
                 break;

            case NET_IF_ERR_INVALID_IF:
            case NET_SOCK_ERR_CLOSED:
            case NET_SOCK_ERR_INVALID_FAMILY:
            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_SOCK_ERR_INVALID_STATE:
            case NET_SOCK_ERR_FAULT:
            case NET_ERR_FAULT_NULL_FNCT:
            case NET_CONN_ERR_NOT_USED:
            case NET_CONN_ERR_INVALID_CONN:
            case NET_CONN_ERR_INVALID_FAMILY:
            case NET_CONN_ERR_INVALID_TYPE:
            case NET_CONN_ERR_INVALID_PROTOCOL_IX:
            case NET_CONN_ERR_INVALID_ADDR_LEN:
            case NET_CONN_ERR_ADDR_NOT_USED:
            case NET_CONN_ERR_ADDR_IN_USE:
            default:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_FAULT;                   /* Rtn fatal err(s).                                    */
                 break;
        }
    }


                                                                /* --------- RESTORE PREV CFG'D CONN TIMEOUT ---------- */
    if (timeout_cfgd == DEF_YES) {                              /* If timeout cfg'd, ...                                */
                                                                /* ... restore prev'ly cfg'd conn timeout.              */
        NetSock_CfgTimeoutConnReqSet(sock_id, timeout_ms_cfgd, &err);
    }


    rtn_status = (rtn_code == NET_SOCK_BSD_ERR_NONE) ? DEF_OK : DEF_FAIL;
   *p_err       =  err_rtn;

    return (rtn_status);
}


/*
*********************************************************************************************************
*                                         NetApp_SockListen()
*
* Description : Set an application socket to listen for connection requests, with error handling.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to listen (see Note #2).
*
*               sock_q_size     Maximum number of connection requests to accept & queue on listen socket.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application socket successfully set to
*                                                                   listen.
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s)  [see Note #1].
*                               NET_APP_ERR_INVALID_OP          Invalid operation(s) [see Note #1].
*                               NET_APP_ERR_FAULT_TRANSITORY    Transitory    fault(s); listen aborted but
*                                                                   MIGHT listen in a subsequent attempt.
*                               NET_APP_ERR_FAULT               Socket listen fault(s); listen aborted &
*                                                                   socket SHOULD be closed.
*
* Return(s)   : DEF_OK,   application socket successfully set to listen.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (2) Socket listen operation valid for stream-type sockets only.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetApp_SockListen (NET_SOCK_ID       sock_id,
                                NET_SOCK_Q_SIZE   sock_q_size,
                                NET_ERR          *p_err)
{
#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN                /* See Note #2.                                         */
    NET_SOCK_RTN_CODE  rtn_code;
    CPU_BOOLEAN        rtn_status;
    NET_ERR            err;
    NET_ERR            err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                /* -------------- SET APP SOCK TO LISTEN -------------- */
    rtn_code = NetSock_Listen((NET_SOCK_ID    ) sock_id,
                              (NET_SOCK_Q_SIZE) sock_q_size,
                              (NET_ERR       *)&err);
    switch (err) {
        case NET_SOCK_ERR_NONE:
             err_rtn = NET_APP_ERR_NONE;
             break;


        case NET_SOCK_ERR_NOT_USED:
        case NET_SOCK_ERR_INVALID_TYPE:
        case NET_SOCK_ERR_INVALID_SOCK:
             err_rtn = NET_APP_ERR_INVALID_ARG;                 /* Rtn invalid arg err(s) [see Note #1].                */
             break;


        case NET_SOCK_ERR_INVALID_OP:
             err_rtn = NET_APP_ERR_INVALID_OP;                  /* Rtn invalid op  err(s).                              */
             break;


        case NET_INIT_ERR_NOT_COMPLETED:
        case NET_ERR_FAULT_LOCK_ACQUIRE:
             err_rtn = NET_APP_ERR_FAULT_TRANSITORY;            /* Rtn transitory  err(s).                              */
             break;


        case NET_SOCK_ERR_CLOSED:
        case NET_SOCK_ERR_INVALID_FAMILY:
        case NET_SOCK_ERR_INVALID_PROTOCOL:
        case NET_SOCK_ERR_INVALID_STATE:
        case NET_SOCK_ERR_CONN_FAIL:
        case NET_CONN_ERR_NOT_USED:
        case NET_CONN_ERR_INVALID_CONN:
        default:
             err_rtn = NET_APP_ERR_FAULT;                       /* Rtn fatal       err(s).                              */
             break;
    }

    rtn_status = (rtn_code == NET_SOCK_BSD_ERR_NONE) ? DEF_OK : DEF_FAIL;
   *p_err       =  err_rtn;

    return (rtn_status);


#else
   (void)&sock_id;                                              /* Prevent 'variable unused' compiler warnings.         */
   (void)&sock_q_size;

   *p_err =  NET_APP_ERR_INVALID_ARG;
    return (DEF_FAIL);
#endif
}


/*
*********************************************************************************************************
*                                         NetApp_SockAccept()
*
* Description : (1) Return a new application socket accepted from a listen application socket, with
*                       error handling :
*
*                   (a) Configure accept timeout, if any
*                   (b) Wait for  accept socket
*                   (c) Restore   accept timeout, if necessary
*
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of listen socket (see Note #3).
*
*               p_addr_remote   Pointer to an address buffer that will receive the socket address structure
*                                   of the accepted socket's remote address (see Note #4).
*
*               p_addr_len      Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address buffer pointed to by 'p_addr_remote'.
*                                   (b) (1) Return the actual size of socket address structure with the
*                                               accepted socket's remote address, if NO error(s);
*                                       (2) Return 0,                             otherwise.
*
*               retry_max       Maximum number of consecutive socket accept retries   (see Note #5a1).
*
*               timeout_ms      Socket accept timeout value per attempt/retry         (see Note #5b1) :
*
*                                   0,                          if current configured timeout value desired.
*                                   NET_TMR_TIME_INFINITE,      if infinite (i.e. NO timeout) value desired.
*                                   In number of milliseconds,  otherwise.
*
*               time_dly_ms     Transitory socket accept delay value, in milliseconds (see Note #5b2).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                New application socket successfully accepted.
*                               NET_APP_ERR_NONE_AVAIL          NO available    sockets to accept.
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s)  [see Note #2].
*                               NET_APP_ERR_INVALID_OP          Invalid operation(s) [see Note #2].
*                               NET_APP_ERR_FAULT_TRANSITORY    Transitory    fault(s); accept aborted but
*                                                                   MIGHT accept in a subsequent attempt.
*                               NET_APP_ERR_FAULT               Socket accept fault(s); accept aborted &
*                                                                   socket SHOULD be closed.
*
* Return(s)   : Socket descriptor/handle identifier of new accepted socket, if NO error(s).
*
*               NET_SOCK_BSD_ERR_ACCEPT,                                    otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (3) Socket accept operation valid for stream-type sockets only.
*
*               (4) (a) Socket address structure 'AddrFamily' member returned in host-order & SHOULD NOT
*                       be converted to network-order.
*
*                   (b) Socket address structure addresses returned in network-order & SHOULD be converted
*                       from network-order to host-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'
*                          & 'net_sock.c  NetSock_Accept()                   Note #3'.
*
*               (5) (a) (1) If a non-zero number of retries is requested                AND ...
*                       (2) global socket blocking ('NET_SOCK_CFG_BLOCK_SEL') is configured ...
*                           for non-blocking operation ('NET_SOCK_BLOCK_SEL_NO_BLOCK');     ...
*
*                   (b) ... then one or more of the following SHOULD also be requested; otherwise, all
*                       retries will most likely fail immediately since no time will elapse to wait for
*                       & allow socket operation(s) to successfully complete :
*
*                       (1) A non-zero timeout
*                       (2) A non-zero time delay
*
*               (6) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

NET_SOCK_ID  NetApp_SockAccept (NET_SOCK_ID         sock_id,
                                NET_SOCK_ADDR      *p_addr_remote,
                                NET_SOCK_ADDR_LEN  *p_addr_len,
                                CPU_INT16U          retry_max,
                                CPU_INT32U          timeout_ms,
                                CPU_INT32U          time_dly_ms,
                                NET_ERR            *p_err)
{
#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN                /* See Note #3.                                         */
    NET_SOCK_ADDR_LEN  addr_len;
    NET_SOCK_ADDR_LEN  addr_len_unused;
    NET_SOCK_ID        sock_id_accept;
    CPU_INT16U         retry_cnt;
    CPU_INT32U         timeout_ms_cfgd;
    CPU_BOOLEAN        timeout_cfgd;
    CPU_BOOLEAN        done;
    CPU_BOOLEAN        dly;
    NET_ERR            err;
    NET_ERR            err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_SOCK_ID)0);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    if (p_addr_len != (NET_SOCK_ADDR_LEN *) 0) {                /* If avail,               ...                          */
         addr_len  = *p_addr_len;                               /* ... save init addr len; ...                          */
    } else {
        p_addr_len  = (NET_SOCK_ADDR_LEN *)&addr_len_unused;    /* ... else re-cfg NULL rtn ptr to unused local var.    */
         addr_len  =  0;
       (void)&addr_len_unused;                                  /* Prevent possible 'variable unused' warning.          */
    }
   *p_addr_len = 0;                                             /* Cfg dflt addr len for err (see Note #6).             */


                                                                /* ---------------- CFG ACCEPT TIMEOUT ---------------- */
    if (timeout_ms > 0) {                                       /* If timeout avail,         ...                        */
                                                                /* ... save cfg'd accept timeout ...                    */
        timeout_ms_cfgd = NetSock_CfgTimeoutConnAcceptGet_ms(sock_id, &err);
                                                                /* ... & cfg temp accept timeout.                       */
        NetSock_CfgTimeoutConnAcceptSet(sock_id, timeout_ms, &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 timeout_cfgd = DEF_YES;
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_ERR_INVALID_TIME:
                *p_err =  NET_APP_ERR_INVALID_ARG;
                 return (NET_SOCK_BSD_ERR_ACCEPT);


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                *p_err =  NET_APP_ERR_FAULT_TRANSITORY;
                 return (NET_SOCK_BSD_ERR_ACCEPT);


            default:
                *p_err =  NET_APP_ERR_FAULT;
                 return (NET_SOCK_BSD_ERR_ACCEPT);
        }

    } else {
        timeout_cfgd = DEF_NO;
    }


                                                                /* ------------- WAIT FOR APP ACCEPT SOCK ------------- */
    retry_cnt      = 0u;
    done           = DEF_NO;
    dly            = DEF_NO;

    while ((retry_cnt <= retry_max) &&                          /* While accept retry <= max retry ...                  */
           (done      == DEF_NO)) {                             /* ... & accept NOT done,          ...                  */

        if (dly == DEF_YES) {                                   /* Dly accept, on retries.                              */
            NetApp_TimeDly_ms(time_dly_ms, &err);
        }
                                                                /* ... wait for accept sock.                            */
       *p_addr_len     = addr_len;
        sock_id_accept = NetSock_Accept((NET_SOCK_ID        ) sock_id,
                                        (NET_SOCK_ADDR     *) p_addr_remote,
                                        (NET_SOCK_ADDR_LEN *) p_addr_len,
                                        (NET_ERR           *)&err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_NONE;
                 break;


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_SOCK_ERR_NONE_AVAIL:                       /* If transitory accept err(s), ...                     */
            case NET_SOCK_ERR_CONN_ACCEPT_Q_NONE_AVAIL:
            case NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly retry.                                       */
                 err_rtn = NET_APP_ERR_NONE_AVAIL;
                 break;


            case NET_ERR_FAULT_NULL_PTR:
            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_TYPE:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_SOCK_ERR_INVALID_ADDR_LEN:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_ARG;             /* Rtn invalid arg err(s) [see Note #2].                */
                 break;


            case NET_SOCK_ERR_INVALID_OP:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_OP;              /* Rtn invalid op  err(s).                              */
                 break;


            case NET_SOCK_ERR_CLOSED:
            case NET_SOCK_ERR_INVALID_FAMILY:
            case NET_SOCK_ERR_INVALID_STATE:
            case NET_SOCK_ERR_CONN_FAIL:
            case NET_SOCK_ERR_FAULT:
            default:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_FAULT;                   /* Rtn fatal err(s).                                    */
                 break;
        }
    }


                                                                /* -------- RESTORE PREV CFG'D ACCEPT TIMEOUT --------- */
    if (timeout_cfgd == DEF_YES) {                              /* If timeout cfg'd, ...                                */
                                                                /* ... restore prev'ly cfg'd accept timeout.            */
        NetSock_CfgTimeoutConnAcceptSet(sock_id, timeout_ms_cfgd, &err);
    }


   *p_err =  err_rtn;

    return (sock_id_accept);


#else
   (void)&sock_id;                                              /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_remote;
   (void)&p_addr_len;
   (void)&retry_max;
   (void)&timeout_ms;
   (void)&time_dly_ms;

   *p_err =  NET_APP_ERR_INVALID_ARG;
    return (NET_SOCK_BSD_ERR_ACCEPT);
#endif
}


/*
*********************************************************************************************************
*                                           NetApp_SockRx()
*
* Description : (1) Receive application data via socket, with error handling :
*
*                   (a) Validate  receive arguments
*                   (b) Configure receive timeout, if any
*                   (c) Receive   application data via socket
*                   (d) Restore   receive timeout, if necessary
*
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to receive application data.
*
*               p_data_buf      Pointer to an application data buffer that will  receive application data.
*
*               data_buf_len    Size of the   application data buffer (in octets).
*
*               data_rx_th      Application data receive threshold :
*
*                                   0,                                  NO minimum receive threshold; i.e.
*                                                                           receive ANY amount of data.
*                                                                           Recommended for datagram sockets
*                                                                           (see Note #4a1).
*                                   Minimum amount of application data
*                                       to receive (in octets) within
*                                       maximum number of retries,      otherwise.
*
*               flags           Flags to select receive options; bit-field flags logically OR'd :
*
*                                       NET_SOCK_FLAG_NONE              No socket flags selected.
*                                       NET_SOCK_FLAG_RX_DATA_PEEK      Receive socket data without consuming
*                                                                           the socket data; i.e. socket data
*                                                                           NOT removed from application receive
*                                                                           queue(s).
*                                       NET_SOCK_FLAG_RX_NO_BLOCK       Receive socket data without blocking.
*
*               p_addr_remote   Pointer to an address buffer that will receive the socket address structure
*                                   with the received data's remote address (see Note #3), if NO error(s);
*                                   recommended for datagram sockets, optional for stream sockets.
*
*               p_addr_len      Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address buffer pointed to by 'p_addr_remote'.
*                                   (b) (1) Return the actual size of socket address structure with the
*                                               received data's remote address, if NO error(s);
*                                       (2) Return 0,                           otherwise.
*
*               retry_max       Maximum number of consecutive socket receive retries   (see Note #5a1).
*
*               timeout_ms      Socket receive timeout value per attempt/retry         (see Note #5b1) :
*
*                                   0,                          if current configured timeout value desired.
*                                   NET_TMR_TIME_INFINITE,      if infinite (i.e. NO timeout) value desired.
*                                   In number of milliseconds,  otherwise.
*
*               time_dly_ms     Transitory socket receive delay value, in milliseconds (see Note #5b2).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application data successfully received; check
*                                                                   return value for number of data octets
*                                                                   received.
*
*                               NET_APP_ERR_DATA_BUF_OVF        Application data successfully received; check
*                                                                   return value for number of data octets
*                                                                   received.  However, some application data
*                                                                   MAY have been discarded (see Note #4a).
*
*                               NET_APP_ERR_CONN_CLOSED         Socket connection closed.  However, some
*                                                                   application data MAY have successfully
*                                                                   been received; check return value for
*                                                                   number of data octets received.
*
*                               NET_APP_ERR_FAULT_TRANSITORY    Transitory        fault(s); receive aborted
*                                                                   but MIGHT receive in a subsequent attempt.
*                               NET_APP_ERR_FAULT               Socket connection fault(s); connection(s)
*                                                                   aborted & socket SHOULD be closed.
*
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s)  [see Note #2].
*                               NET_APP_ERR_INVALID_OP          Invalid operation(s) [see Note #2].
*
*                               NET_ERR_RX                      Transitory receive error(s).  However, some
*                                                                   application data MAY have successfully
*                                                                   been received; check return value for
*                                                                   number of data octets received.
*
* Return(s)   : Number of positive data octets received, if NO error(s).
*
*               0,                                       otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (3) (a) Socket address structure 'AddrFamily' member returned in host-order & SHOULD NOT
*                       be converted to network-order.
*
*                   (b) Socket address structure addresses returned in network-order & SHOULD be converted
*                       from network-order to host-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*
*               (4) (a) (1) (A) Datagram-type sockets transmit & receive all data atomically -- i.e. every
*                               single, complete datagram transmitted MUST be received as a single, complete
*                               datagram.
*
*                           (B) IEEE Std 1003.1, 2004 Edition, Section 'recvfrom() : DESCRIPTION' summarizes
*                               that "for message-based sockets, such as ... SOCK_DGRAM ... the entire message
*                               shall be read in a single operation.  If a message is too long to fit in the
*                               supplied buffer, and MSG_PEEK is not set in the flags argument, the excess
*                               bytes shall be discarded".
*
*                       (2) Thus if the socket's type is datagram & the receive data buffer size is
*                           NOT large enough for the received data, the receive data buffer is maximally
*                           filled with receive data but the remaining data octets are discarded &
*                           NET_APP_ERR_DATA_BUF_OVF error is returned.
*
*                   (b) (1) (A) Stream-type sockets transmit & receive all data octets in one or more
*                               non-distinct packets.  In other words, the application data is NOT
*                               bounded by any specific packet(s); rather, it is contiguous & sequenced
*                               from one packet to the next.
*
*                           (B) IEEE Std 1003.1, 2004 Edition, Section 'recv() : DESCRIPTION' summarizes
*                               that "for stream-based sockets, such as SOCK_STREAM, message boundaries
*                               shall be ignored.  In this case, data shall be returned to the user as
*                               soon as it becomes available, and no data shall be discarded".
*
*                       (2) Thus if the socket's type is stream & the receive data buffer size is NOT
*                           large enough for the received data, the receive data buffer is maximally
*                           filled with receive data & the remaining data octets remain queued for
*                           later application receives.
*
*                   See also 'net_sock.c  NetSock_RxDataHandler()  Note #2'.
*
*               (5) (a) (1) If a non-zero number of retries is requested                    AND
*                       (2) (A) global socket blocking ('NET_SOCK_CFG_BLOCK_SEL') is configured
*                               for non-blocking operation ('NET_SOCK_BLOCK_SEL_NO_BLOCK'), OR
*                           (B) socket 'flags' argument set to 'NET_SOCK_FLAG_RX_BLOCK';
*
*                   (b) ... then one or more of the following SHOULD also be requested; otherwise, all
*                       retries will most likely fail immediately since no time will elapse to wait for
*                       & allow socket operation(s) to successfully complete :
*
*                       (1) A non-zero timeout
*                       (2) A non-zero time delay
*
*               (6) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

CPU_INT16U  NetApp_SockRx (NET_SOCK_ID          sock_id,
                           void                *p_data_buf,
                           CPU_INT16U           data_buf_len,
                           CPU_INT16U           data_rx_th,
                           NET_SOCK_API_FLAGS   flags,
                           NET_SOCK_ADDR       *p_addr_remote,
                           NET_SOCK_ADDR_LEN   *p_addr_len,
                           CPU_INT16U           retry_max,
                           CPU_INT32U           timeout_ms,
                           CPU_INT32U           time_dly_ms,
                           NET_ERR             *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    NET_SOCK_ADDR_LEN  *p_addr_len_init;
#endif
    NET_SOCK_ADDR_LEN   addr_len;
    NET_SOCK_ADDR_LEN   addr_len_unused;
    NET_SOCK_ADDR_LEN   addr_len_temp;
    NET_SOCK_ADDR       addr_temp;
    CPU_INT08U         *p_data_buf_rem;
    CPU_INT16U          data_buf_len_rem;
    CPU_INT16S          rx_len;
    CPU_INT16U          rx_len_tot;
    CPU_INT16U          rx_th_actual;
    CPU_INT16U          retry_cnt;
    CPU_INT32U          timeout_ms_cfgd;
    CPU_BOOLEAN         timeout_cfgd;
    CPU_BOOLEAN         done;
    CPU_BOOLEAN         dly;
    NET_ERR             err;
    NET_ERR             err_rtn;



#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(0u);
    }
                                                                /* ----------------- VALIDATE RX ADDR ----------------- */
    p_addr_len_init = (NET_SOCK_ADDR_LEN *) p_addr_len;
#endif
    if (p_addr_len != (NET_SOCK_ADDR_LEN *) 0) {                /* If avail,               ...                          */
         addr_len  = (NET_SOCK_ADDR_LEN  )*p_addr_len;          /* ... save init addr len; ...                          */
    } else {
        p_addr_len  = (NET_SOCK_ADDR_LEN *)&addr_len_unused;    /* ... else re-cfg NULL rtn ptr to unused local var.    */
         addr_len  = (NET_SOCK_ADDR_LEN  ) 0;
       (void)&addr_len_unused;                                  /* Prevent possible 'variable unused' warning.          */
    }
   *p_addr_len = 0;                                             /* Cfg dflt addr len for err (see Note #6).             */


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (((p_addr_remote   != (NET_SOCK_ADDR     *)0)  &&        /* If (remote addr         avail BUT ..                 */
         (p_addr_len_init == (NET_SOCK_ADDR_LEN *)0)) ||        /* ..  remote addr len NOT avail) OR ..                 */
        ((p_addr_remote   == (NET_SOCK_ADDR     *)0)  &&        /* .. (remote addr     NOT avail BUT ..                 */
         (p_addr_len_init != (NET_SOCK_ADDR_LEN *)0))) {        /* ..  remote addr len     avail),   ..                 */
         *p_err =  NET_APP_ERR_INVALID_ARG;                     /* ..  rtn err.                                         */
          return (0u);
    }
#endif
    if (p_addr_remote == (NET_SOCK_ADDR     *) 0) {             /* If remote addr/addr len NOT avail, ...               */
        p_addr_remote  = (NET_SOCK_ADDR     *)&addr_temp;       /* ...   use temp addr                ...               */
        p_addr_len     = (NET_SOCK_ADDR_LEN *)&addr_len_temp;   /* ...     & temp addr len.                             */
         addr_len     = (NET_SOCK_ADDR_LEN  ) sizeof(addr_temp);/* Save init temp addr len.                             */
    }

                                                                /* --------------- VALIDATE DATA RX TH ---------------- */
    if (data_rx_th   < 1) {
        rx_th_actual = 1u;                                      /* Lim rx th to at least 1 octet ...                    */
    } else if (data_rx_th > data_buf_len) {
        rx_th_actual = data_buf_len;                            /* ... & max of app buf data len.                       */
    } else {
        rx_th_actual = data_rx_th;
    }


                                                                /* ------------------ CFG RX TIMEOUT ------------------ */
    if (timeout_ms > 0) {                                       /* If timeout avail,         ...                        */
                                                                /* ... save cfg'd rx timeout ...                        */
        timeout_ms_cfgd = NetSock_CfgTimeoutRxQ_Get_ms(sock_id, &err);
                                                                /* ... & cfg temp rx timeout.                           */
        NetSock_CfgTimeoutRxQ_Set(sock_id, timeout_ms, &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 timeout_cfgd = DEF_YES;
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_TYPE:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_ERR_INVALID_TIME:
                *p_err =  NET_APP_ERR_INVALID_ARG;
                 return (0u);


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                *p_err =  NET_APP_ERR_FAULT_TRANSITORY;
                 return (0u);


            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_CONN_ERR_NOT_USED:
            case NET_CONN_ERR_INVALID_CONN:
            case NET_TCP_ERR_CONN_NOT_USED:
            case NET_TCP_ERR_INVALID_CONN:
            default:
                *p_err =  NET_APP_ERR_FAULT;
                 return (0u);
        }

    } else {
        timeout_cfgd = DEF_NO;
    }


                                                                /* ------------------- RX APP DATA -------------------- */
    rx_len_tot = 0u;
    retry_cnt  = 0u;
    done       = DEF_NO;
    dly        = DEF_NO;
    err_rtn    = NET_ERR_RX;

    while ((rx_len_tot <  rx_th_actual) &&                      /* While rx tot len <  rx th     ...                    */
           (retry_cnt  <= retry_max)    &&                      /* ... & rx retry   <= max retry ...                    */
           (done       == DEF_NO)) {                            /* ... & rx NOT done,            ...                    */

        if (dly == DEF_YES) {                                   /* Dly rx, on retries.                                  */
            NetApp_TimeDly_ms(time_dly_ms, &err);
        }
                                                                /* ... rx app data.                                     */
       *p_addr_len        = (NET_SOCK_ADDR_LEN) addr_len;
        p_data_buf_rem    = (CPU_INT08U      *)p_data_buf    + rx_len_tot;
        data_buf_len_rem  = (CPU_INT16U       )(data_buf_len - rx_len_tot);
        rx_len            = (CPU_INT16S       ) NetSock_RxDataFrom((NET_SOCK_ID        ) sock_id,
                                                                   (void              *) p_data_buf_rem,
                                                                   (CPU_INT16U         ) data_buf_len_rem,
                                                                   (NET_SOCK_API_FLAGS ) flags,
                                                                   (NET_SOCK_ADDR     *) p_addr_remote,
                                                                   (NET_SOCK_ADDR_LEN *) p_addr_len,
                                                                   (void              *) 0,
                                                                   (CPU_INT08U         ) 0u,
                                                                   (CPU_INT08U        *) 0,
                                                                   (NET_ERR           *)&err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
            case NET_SOCK_ERR_INVALID_DATA_SIZE:
                 if (rx_len > 0) {                              /* If          rx len > 0, ...                          */
                     rx_len_tot += (CPU_INT16U)rx_len;          /* ... inc tot rx len.                                  */
                 }

                 if (err == NET_SOCK_ERR_INVALID_DATA_SIZE) {   /* If app data buf NOT large enough for all rx'd data,  */
                     done      = DEF_YES;
                     err_rtn   = NET_APP_ERR_DATA_BUF_OVF;      /* .. rtn data buf ovf err (see Note #4a2).             */

                 } else {
                     retry_cnt = 0u;
                     dly       = DEF_NO;
                     err_rtn   = NET_APP_ERR_NONE;
                 }
                 break;


            case NET_ERR_RX:                                    /* If transitory rx err(s), ...                         */
            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_SOCK_ERR_RX_Q_EMPTY:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly next rx.                                     */
                 err_rtn = NET_ERR_RX;
                 break;


            case NET_SOCK_ERR_CLOSED:
            case NET_SOCK_ERR_RX_Q_CLOSED:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_CONN_CLOSED;             /* Rtn conn closed.                                     */
                 break;


            case NET_ERR_FAULT_NULL_PTR:
            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_TYPE:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_SOCK_ERR_INVALID_FLAG:
            case NET_SOCK_ERR_INVALID_ADDR_LEN:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_ARG;             /* Rtn invalid arg err(s) [see Note #1].                */
                 break;


            case NET_SOCK_ERR_INVALID_OP:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_OP;              /* Rtn invalid op  err(s).                              */
                 break;


            case NET_SOCK_ERR_INVALID_FAMILY:
            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_SOCK_ERR_INVALID_STATE:
            case NET_SOCK_ERR_CONN_FAIL:
            case NET_SOCK_ERR_FAULT:
            case NET_ERR_FAULT_NULL_FNCT:
            case NET_CONN_ERR_NOT_USED:
            case NET_CONN_ERR_INVALID_CONN:
            case NET_CONN_ERR_INVALID_ADDR_LEN:
            case NET_CONN_ERR_ADDR_NOT_USED:
            default:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_FAULT;                   /* Rtn fatal err(s).                                    */
                 break;
        }
    }


                                                                /* ---------- RESTORE PREV CFG'D RX TIMEOUT ----------- */
    if (timeout_cfgd == DEF_YES) {                              /* If timeout cfg'd, ...                                */
                                                                /* ... restore prev'ly cfg'd rx timeout.                */
        NetSock_CfgTimeoutRxQ_Set(sock_id, timeout_ms_cfgd, &err);
    }


   *p_err =  err_rtn;

    return (rx_len_tot);
}


/*
*********************************************************************************************************
*                                           NetApp_SockTx()
*
* Description : (1) Transmit application data via socket, with error handling :
*
*                   (a) Configure transmit timeout, if any
*                   (b) Transmit  application data via socket
*                   (c) Restore   transmit timeout, if necessary
*
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to transmit application data.
*
*               p_data          Pointer to application data to transmit.
*
*               data_len        Length  of application data to transmit (in octets).
*
*               flags           Flags to select transmit options; bit-field flags logically OR'd :
*
*                                   NET_SOCK_FLAG_NONE              No socket flags selected.
*                                   NET_SOCK_FLAG_TX_NO_BLOCK       Transmit socket data without blocking.
*
*               p_addr_remote   Pointer to destination address buffer (see Note #3);
*                                   required for datagram sockets, optional for stream sockets.
*
*               addr_len        Length of  destination address buffer (in octets).
*
*               retry_max       Maximum number of consecutive socket transmit retries   (see Note #4a1).
*
*               timeout_ms      Socket transmit timeout value per attempt/retry         (see Note #4b1) :
*
*                                   0,                          if current configured timeout value desired
*                                                                   [or NO timeout for datagram sockets
*                                                                   (see Note #5)].
*                                   NET_TMR_TIME_INFINITE,      if infinite (i.e. NO timeout) value desired.
*                                   In number of milliseconds,  otherwise.
*
*               time_dly_ms     Transitory socket transmit delay value, in milliseconds (see Note #4b2).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Application data successfully transmitted
*                                                                   &/or queued for transmission; check
*                                                                   return value for number of data octets
*                                                                   transmitted.
*
*                               NET_APP_ERR_CONN_CLOSED         Socket connection closed.  However, some
*                                                                   application data MAY have successfully
*                                                                   transmitted; check return value for
*                                                                   number of data octets transmitted.
*
*                               NET_APP_ERR_FAULT_TRANSITORY    Transitory        fault(s); transmit aborted
*                                                                   but MIGHT transmit in a subsequent attempt.
*                               NET_APP_ERR_FAULT               Socket connection fault(s); connection(s)
*                                                                   aborted & socket SHOULD be closed.
*
*                               NET_APP_ERR_INVALID_ARG         Invalid argument(s)  [see Note #2].
*                               NET_APP_ERR_INVALID_OP          Invalid operation(s) [see Note #2].
*
*                               NET_ERR_TX                      Transitory transmit error(s).  However, some
*                                                                   application data MAY have successfully
*                                                                   transmitted; check return value for
*                                                                   number of data octets transmitted.
*
* Return(s)   : Number of positive data octets transmitted, if NO error(s).
*
*               0,                                          otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) Socket arguments &/or operations validated in network socket handler functions.  Some
*                   arguments validated only if validation code is enabled (i.e. NET_ERR_CFG_ARG_CHK_EXT_EN
*                   is DEF_ENABLED in 'net_cfg.h').
*
*               (3) (a) Socket address structure 'AddrFamily' member MUST be configured in host-order &
*                       MUST NOT be converted to/from network-order.
*
*                   (b) Socket address structure addresses MUST be configured/converted from host-order
*                       to network-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*
*               (4) (a) (1) If a non-zero number of retries is requested                    AND ...
*                       (2) (A) global socket blocking ('NET_SOCK_CFG_BLOCK_SEL') is configured ...
*                               for non-blocking operation ('NET_SOCK_BLOCK_SEL_NO_BLOCK'), OR  ...
*                           (B) socket 'flags' argument set to 'NET_SOCK_FLAG_TX_BLOCK';        ...
*
*                   (b) ... then one or more of the following SHOULD also be requested; otherwise, all
*                       retries will most likely fail immediately since no time will elapse to wait for
*                       & allow socket operation(s) to successfully complete :
*
*                       (1) A non-zero timeout
*                       (2) A non-zero time delay
*
*               (5) Datagram sockets NOT currently blocked during transmit & therefore require NO
*                   transmit timeout.
*
*                   See also 'net_sock.c  NetSock_CfgTimeoutTxQ_Set()  Note #2b'.
*********************************************************************************************************
*/

CPU_INT16U  NetApp_SockTx (NET_SOCK_ID          sock_id,
                           void                *p_data,
                           CPU_INT16U           data_len,
                           NET_SOCK_API_FLAGS   flags,
                           NET_SOCK_ADDR       *p_addr_remote,
                           NET_SOCK_ADDR_LEN    addr_len,
                           CPU_INT16U           retry_max,
                           CPU_INT32U           timeout_ms,
                           CPU_INT32U           time_dly_ms,
                           NET_ERR             *p_err)
{
    CPU_INT08U   *p_data_buf;
    CPU_INT16U    data_buf_len;
    CPU_INT16S    tx_len;
    CPU_INT16U    tx_len_tot;
    CPU_INT16U    retry_cnt;
    CPU_INT32U    timeout_ms_cfgd;
    CPU_BOOLEAN   timeout_cfgd;
    CPU_BOOLEAN   done;
    CPU_BOOLEAN   dly;
    NET_ERR       err;
    NET_ERR       err_rtn;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(0u);
    }
#endif
                                                                /* ------------------ CFG TX TIMEOUT ------------------ */
    if (timeout_ms > 0) {                                       /* If timeout avail,         ...                        */
                                                                /* ... save cfg'd tx timeout ...                        */
        timeout_ms_cfgd = NetSock_CfgTimeoutTxQ_Get_ms(sock_id, &err);
                                                                /* ... & cfg temp tx timeout.                           */
        NetSock_CfgTimeoutTxQ_Set(sock_id, timeout_ms, &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 timeout_cfgd = DEF_YES;
                 break;


            case NET_SOCK_ERR_INVALID_TYPE:                     /* Datagram sock timeout NOT avail (see Note #5).       */
                 timeout_cfgd = DEF_NO;
                 break;


            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_ERR_INVALID_TIME:
                *p_err =  NET_APP_ERR_INVALID_ARG;
                 return (0u);


            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                *p_err =  NET_APP_ERR_FAULT_TRANSITORY;
                 return (0u);


            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_CONN_ERR_NOT_USED:
            case NET_CONN_ERR_INVALID_CONN:
            case NET_TCP_ERR_CONN_NOT_USED:
            case NET_TCP_ERR_INVALID_CONN:
            default:
                *p_err =  NET_APP_ERR_FAULT;
                 return (0u);
        }

    } else {
        timeout_cfgd = DEF_NO;
    }


                                                                /* ------------------- TX APP DATA -------------------- */
    tx_len_tot = 0u;
    retry_cnt  = 0u;
    done       = DEF_NO;
    dly        = DEF_NO;
    err_rtn    = NET_ERR_TX;

    while ((tx_len_tot <  data_len ) &&                         /* While tx tot len <  app data len ...                 */
           (retry_cnt  <= retry_max) &&                         /* ... & tx retry   <= max retry    ...                 */
           (done       == DEF_NO)) {                            /* ... & tx NOT done,               ...                 */

        if (dly == DEF_YES) {                                   /* Dly tx, on retries.                                  */
            NetApp_TimeDly_ms(time_dly_ms, &err);
        }
                                                                /* ... tx app data.                                     */
        p_data_buf    = (CPU_INT08U *)p_data     + tx_len_tot;
        data_buf_len  = (CPU_INT16U  )( data_len - tx_len_tot);
        tx_len        = (CPU_INT16S  )NetSock_TxDataTo((NET_SOCK_ID       ) sock_id,
                                                       (void             *) p_data_buf,
                                                       (CPU_INT16U        ) data_buf_len,
                                                       (NET_SOCK_API_FLAGS) flags,
                                                       (NET_SOCK_ADDR    *) p_addr_remote,
                                                       (NET_SOCK_ADDR_LEN ) addr_len,
                                                       (NET_ERR          *)&err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 if (tx_len > 0) {                              /* If          tx len > 0, ...                          */
                     tx_len_tot += (CPU_INT16U)tx_len;          /* ... inc tot tx len.                                  */
                 }
                 retry_cnt = 0u;
                 dly       = DEF_NO;
                 err_rtn   = NET_APP_ERR_NONE;
                 break;


            case NET_ERR_TX:                                    /* If transitory tx err(s), ...                         */
            case NET_INIT_ERR_NOT_COMPLETED:
            case NET_SOCK_ERR_PORT_NBR_NONE_AVAIL:
            case NET_CONN_ERR_NONE_AVAIL:
            case NET_IPv4_ERR_ADDR_NONE_AVAIL:
            case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
            case NET_ERR_FAULT_LOCK_ACQUIRE:
                 retry_cnt++;
                 dly     = DEF_YES;                             /* ... dly next tx.                                     */
                 err_rtn = NET_ERR_TX;
                 break;


            case NET_SOCK_ERR_CLOSED:
            case NET_SOCK_ERR_TX_Q_CLOSED:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_CONN_CLOSED;             /* Rtn conn closed.                                     */
                 break;


            case NET_ERR_FAULT_NULL_PTR:
            case NET_SOCK_ERR_NOT_USED:
            case NET_SOCK_ERR_INVALID_TYPE:
            case NET_SOCK_ERR_INVALID_SOCK:
            case NET_SOCK_ERR_INVALID_DATA_SIZE:
            case NET_SOCK_ERR_INVALID_FLAG:
            case NET_SOCK_ERR_INVALID_ADDR:
            case NET_SOCK_ERR_INVALID_ADDR_LEN:
            case NET_SOCK_ERR_INVALID_PORT_NBR:
            case NET_SOCK_ERR_INVALID_CONN:
            case NET_SOCK_ERR_ADDR_IN_USE:
            case NET_CONN_ERR_ADDR_IN_USE:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_ARG;             /* Rtn invalid arg err(s) [see Note #1].                */
                 break;


            case NET_SOCK_ERR_INVALID_OP:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_INVALID_OP;              /* Rtn invalid op  err(s).                              */
                 break;


            case NET_IF_ERR_INVALID_IF:
            case NET_SOCK_ERR_INVALID_FAMILY:
            case NET_SOCK_ERR_INVALID_PROTOCOL:
            case NET_SOCK_ERR_INVALID_STATE:
            case NET_SOCK_ERR_CONN_FAIL:
            case NET_SOCK_ERR_FAULT:
            case NET_ERR_FAULT_NULL_FNCT:
            case NET_CONN_ERR_NOT_USED:
            case NET_CONN_ERR_INVALID_TYPE:
            case NET_CONN_ERR_INVALID_CONN:
            case NET_CONN_ERR_INVALID_FAMILY:
            case NET_CONN_ERR_INVALID_PROTOCOL_IX:
            case NET_CONN_ERR_INVALID_ADDR_LEN:
            case NET_CONN_ERR_ADDR_NOT_USED:
            default:
                 done    = DEF_YES;
                 err_rtn = NET_APP_ERR_FAULT;                   /* Rtn fatal err(s).                                    */
                 break;
        }
    }


                                                                /* ---------- RESTORE PREV CFG'D TX TIMEOUT ----------- */
    if (timeout_cfgd == DEF_YES) {                              /* If timeout cfg'd, ...                                */
                                                                /* ... restore prev'ly cfg'd tx timeout.                */
        NetSock_CfgTimeoutTxQ_Set(sock_id, timeout_ms_cfgd, &err);
    }


   *p_err =  err_rtn;

    return (tx_len_tot);
}


/*
*********************************************************************************************************
*                                        NetApp_SetSockAddr()
*
* Description : Setup a socket address from an IPv4 or an IPv6 address.
*
* Argument(s) : p_sock_addr     Pointer to the socket address that will be configure by this function :
*
*               addr_family     IP address family to configure, possible values:
*
*               port_nbr        Port number.
*
*               p_addr          Pointer to IP address to use.
*
*               addr_len        Length of the IP address to use.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                        No error.
*                               NET_ERR_FAULT_NULL_PTR                  Null pointer.
*                               NET_APP_ERR_INVALID_ADDR_LEN            Invalid address length.
*                               NET_APP_ERR_INVALID_ADDR_FAMILY         Invalid address family.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetApp_SetSockAddr (NET_SOCK_ADDR        *p_sock_addr,
                          NET_SOCK_ADDR_FAMILY  addr_family,
                          NET_PORT_NBR          port_nbr,
                          CPU_INT08U           *p_addr,
                          NET_IP_ADDR_LEN       addr_len,
                          NET_ERR              *p_err)
{
#ifdef  NET_IPv4_MODULE_EN
    NET_SOCK_ADDR_IPv4  *p_addr_ipv4;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_SOCK_ADDR_IPv6  *p_addr_ipv6;
#endif


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }


    if (p_sock_addr == (NET_SOCK_ADDR *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (p_addr == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    switch(addr_family) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_SOCK_ADDR_FAMILY_IP_V4:
            if (addr_len != NET_IPv4_ADDR_SIZE) {
               *p_err = NET_APP_ERR_INVALID_ADDR_LEN;
                return;
            }

            Mem_Clr(p_sock_addr, NET_SOCK_ADDR_IPv4_SIZE);

            p_addr_ipv4             = (NET_SOCK_ADDR_IPv4 *)p_sock_addr;
            p_addr_ipv4->AddrFamily =  NET_SOCK_ADDR_FAMILY_IP_V4;
            p_addr_ipv4->Port       =  NET_UTIL_HOST_TO_NET_16(port_nbr);

            NET_UTIL_VAL_COPY_GET_NET_32(&p_addr_ipv4->Addr, p_addr);
            break;
#endif

#ifdef  NET_IPv6_MODULE_EN
        case NET_SOCK_ADDR_FAMILY_IP_V6:
            if (addr_len != NET_IPv6_ADDR_SIZE) {
               *p_err = NET_APP_ERR_INVALID_ADDR_LEN;
                return;
            }

            Mem_Clr(p_sock_addr, NET_SOCK_ADDR_IPv6_SIZE);

            p_addr_ipv6             = (NET_SOCK_ADDR_IPv6 *)p_sock_addr;
            p_addr_ipv6->AddrFamily =  NET_SOCK_ADDR_FAMILY_IP_V6;
            p_addr_ipv6->Port       =  NET_UTIL_HOST_TO_NET_16(port_nbr);

            Mem_Copy((void     *)&p_addr_ipv6->Addr,
                     (void     *) p_addr,
                     (CPU_SIZE_T) addr_len);
            break;
#endif

        default:
            *p_err = NET_APP_ERR_INVALID_ADDR_FAMILY;
             return;
    }

   *p_err = NET_APP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                  NetApp_ClientStreamOpenByHostname()
*
* Description : (1) Connect a client to a server using its host name with a stream (TCP) socket
*                   (select IP address automatically See Note #2):
*
*                   (a) Get IP address of the remote host from a string that contains either the IP address or
*                       the host name that will be resolved using DNS (See Note #2).
*
*                   (b) Open a stream socket.
*
*                   (c) Connect a stream socket.
*
*
* Argument(s) : p_sock_id           Pointer to a variable that will receive the socket ID opened from this function.
*
*               p_remote_host_name  Pointer to a string that contains the remote host name to resolve:*
*                                       Can be an IP (IPv4 or IPv6) or a host name (resolved using DNS).
*
*               remote_port_nbr     Port of the remote host.
*
*               p_sock_addr         Pointer to a variable that will receive the socket address of the remote host.
*
*                                       DEF_NULL, if not required.
*
*               p_secure_cfg        Pointer to the secure configuration (TLS/SSL):
*
*                                       DEF_NULL, if no security enabled.
*                                       Pointer to a structure that contains the parameters.
*
*                                       NOT used when socket type is NET_SOCK_TYPE_DATAGRAM
*
*               req_timeout_ms      Connection timeout in ms.
*                                       NOT used when socket type is NET_SOCK_TYPE_DATAGRAM
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE
*                               NET_ERR_FAULT_NULL_PTR
*                               NET_ERR_FAULT_FEATURE_DIS
*                               NET_APP_ERR_INVALID_ADDR_FAMILY
*                               NET_APP_ERR_FAULT
*
* Return(s)   : NET_IP_ADDR_FAMILY_IPv4, if the connected successfully using an IPv4 address.
*               NET_IP_ADDR_FAMILY_IPv6, if the connected successfully using an IPv6 address.
*               NET_IP_ADDR_FAMILY_UNKNOWN, otherwise.
*
* Caller(s)   : Application,
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) When a host name is passed into the remote host name parameter, this function will try to
*                   resolve the address of the remote host using DNS.
*
*                   (a) Obviously DNS must be present and enabled in the project to be possible.
*
*                   (b) If an IPv6 and an IPv4 address are found for the remote host. This function will first
*                       try to connect to the remote host using the IPv6 address. If the connection fails using
*                       IPv6 then a connection retry will occur using the IPv4 address.
*
*                   (c) This function always block, and the fail timeout depend of the DNS resolution timeout,
*                       the number of remote address found and the connection timeout parameter.
*
*               (3) This function is in blocking mode, meaning that at the end of the function, the
*                   socket will have succeed or failed to connect to the remote host.
*********************************************************************************************************
*/

NET_IP_ADDR_FAMILY  NetApp_ClientStreamOpenByHostname (NET_SOCK_ID              *p_sock_id,
                                                       CPU_CHAR                 *p_remote_host_name,
                                                       NET_PORT_NBR              remote_port_nbr,
                                                       NET_SOCK_ADDR            *p_sock_addr,
                                                       NET_APP_SOCK_SECURE_CFG  *p_secure_cfg,
                                                       CPU_INT32U                req_timeout_ms,
                                                       NET_ERR                  *p_err)
{
    NET_IP_ADDR_FAMILY   addr_rtn = NET_IP_ADDR_FAMILY_UNKNOWN;
    CPU_BOOLEAN          done     = DEF_NO;
    CPU_INT08U           addr[NET_SOCK_BSD_ADDR_LEN_MAX];
    NET_IP_ADDR_FAMILY   addr_family;
    NET_SOCK_ADDR        sock_addr_local;
    NET_SOCK_ADDR       *p_sock_addr_local;
#ifdef  NET_EXT_MODULE_DNS_EN
#if (defined(NET_IPv4_MODULE_EN) & \
     defined(NET_IPv6_MODULE_EN))
    CPU_BOOLEAN          switch_addr_type = DEF_NO;
#endif
    CPU_INT08U           conn_attempts    = 0u;
    DNSc_STATUS          status;
    DNSc_ERR             err;
#endif


                                                                /* ---------------- VALIDATE ARGUMENTS ---------------- */
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_sock_id == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }

    if (p_remote_host_name == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }

#ifndef  NET_SECURE_MODULE_EN
    if (p_secure_cfg != DEF_NULL) {
       *p_err = NET_ERR_FAULT_FEATURE_DIS;
        goto exit;
    }
#endif

#endif
                                                                /* ------------- SET SOCK ADDRESS POINTER ------------- */
    if (p_sock_addr == DEF_NULL) {
        p_sock_addr_local = &sock_addr_local;
    } else {
        p_sock_addr_local =  p_sock_addr;
    }

                                                                /* -------------- SET ADDRESS IP FAMILY --------------- */
#ifdef  NET_EXT_MODULE_DNS_EN
#ifdef  NET_IPv6_MODULE_EN
        addr_family = NET_IP_ADDR_FAMILY_IPv6;                  /* First try with IPv6 if enabled.                      */
#else
        addr_family = NET_IP_ADDR_FAMILY_IPv4;
#endif
#endif

                                                                /* ------- RESOLVE ADDR, OPEN & CONNECT SOCKET -------- */
    while (done == DEF_NO) {
        CPU_BOOLEAN    connect;
#ifdef  NET_EXT_MODULE_DNS_EN
        DNSc_ADDR_OBJ  addr_dns[NET_BSD_CFG_ADDRINFO_HOST_NBR_MAX];
        DNSc_FLAGS     flags    = DNSc_FLAG_NONE;
        CPU_INT08U     addr_nbr = NET_BSD_CFG_ADDRINFO_HOST_NBR_MAX;


                                                                /* ------------- RESOLVE REMOTE HOST ADDR ------------- */
        switch (addr_family) {                                  /* Set DNS Parameters                                   */
            case NET_IP_ADDR_FAMILY_IPv6:
                 flags          |= DNSc_FLAG_IPv6_ONLY;         /* Get IPv6 address only.                               */
                 break;

            case NET_IP_ADDR_FAMILY_IPv4:
                 flags          |= DNSc_FLAG_IPv4_ONLY;         /* Get IPv4 address only.                               */
                 break;

            default:
                *p_err = NET_APP_ERR_INVALID_ADDR_FAMILY;
                 goto exit;
        }

                                                                /* ------------------ DNS RESOLUTION ------------------ */
        status = DNSc_GetHost(p_remote_host_name, DEF_NULL, 0u, addr_dns, &addr_nbr, flags, DEF_NULL, &err);
        switch (status) {
            case DNSc_STATUS_RESOLVED:
                 if (addr_nbr != 0) {                           /* Copy all IPv4 or IPv6 addresses that resolve host.   */
                     Mem_Copy(addr, addr_dns[conn_attempts].Addr, addr_dns[conn_attempts].Len);
                     connect = DEF_YES;

                     switch (addr_dns[conn_attempts].Len) {
                         case NET_IPv4_ADDR_LEN:
                              addr_family = NET_IP_ADDR_FAMILY_IPv4;
                              break;

                         case NET_IPv6_ADDR_LEN:
                              addr_family = NET_IP_ADDR_FAMILY_IPv6;
                              break;

                         default:
                             *p_err = NET_APP_ERR_FAULT;
                              goto exit;

                     }
                 } else {
                     connect = DEF_NO;
                     if (addr_family == NET_IP_ADDR_FAMILY_IPv6) {
                         addr_family  = NET_IP_ADDR_FAMILY_IPv4;
                     } else {
                        *p_err = NET_APP_ERR_FAULT;
                         goto exit;
                     }
                 }
                 break;


            case DNSc_STATUS_FAILED:
            case DNSc_STATUS_PENDING:
            default:
                 connect = DEF_NO;
                 if (addr_family == NET_IP_ADDR_FAMILY_IPv6) {
                     addr_family  = NET_IP_ADDR_FAMILY_IPv4;
                 } else {
                    *p_err = NET_APP_ERR_FAULT;
                     goto exit;
                 }
                 break;
        }

#else
                                                                /* ------------ CONVERT STRING IP ADDRESS ------------- */
        addr_family = NetASCII_Str_to_IP(p_remote_host_name, addr, NET_SOCK_BSD_ADDR_LEN_MAX, p_err);
        if (*p_err != NET_ASCII_ERR_NONE) {
             goto exit;
        }

        switch (addr_family) {
             case NET_IP_ADDR_FAMILY_IPv6:
                  connect = DEF_YES;
                  break;

             case NET_IP_ADDR_FAMILY_IPv4:
                  connect = DEF_YES;
                  break;

             default:
                 *p_err = NET_APP_ERR_INVALID_ADDR_FAMILY;
                  goto exit;
        }
#endif

                                                                /* ------------ CONNECT TO THE REMOTE HOST ------------ */
        if (connect == DEF_YES) {

           *p_sock_id = NetApp_ClientStreamOpen(addr,
                                                addr_family,
                                                remote_port_nbr,
                                                p_sock_addr_local,
                                                p_secure_cfg,
                                                req_timeout_ms,
                                                p_err);
            switch (*p_err) {
                case NET_APP_ERR_NONE:
                case NET_APP_ERR_CONN_IN_PROGRESS:
                     addr_rtn = addr_family;
                     goto exit;

                case NET_SOCK_ERR_INVALID_ADDR:
                case NET_APP_ERR_CONN_FAIL:
#ifdef  NET_SECURE_MODULE_EN
                case NET_SECURE_ERR_HANDSHAKE:
#endif
#if (defined(NET_EXT_MODULE_DNS_EN) & \
     defined(NET_IPv4_MODULE_EN)    & \
     defined(NET_IPv6_MODULE_EN))
                     if (conn_attempts < (addr_nbr - 1u)) {     /* If conn failed but we haven't exhausted every addr...*/
                         conn_attempts++;                       /* ...increment index of returned DNS addresses table.  */
                         break;                                 /* Otherwise try connecting to host & port by using a...*/
                     }                                          /* ...different IP address type if possible.            */

                     switch_addr_type = DEF_YES;
                     break;
#endif

                default:
                    goto exit;
            }


#if (defined(NET_EXT_MODULE_DNS_EN) & \
     defined(NET_IPv4_MODULE_EN)    & \
     defined(NET_IPv6_MODULE_EN))
            if (switch_addr_type == DEF_YES) {
                if (addr_family == NET_IP_ADDR_FAMILY_IPv6) {
                    addr_family      = NET_IP_ADDR_FAMILY_IPv4;
                    conn_attempts    = 0u;
                    switch_addr_type = DEF_NO;
                } else {
                    goto exit;
                }
            }
#endif
        }
    }


exit:
    return (addr_rtn);
}


/*
*********************************************************************************************************
*                                 NetApp_ClientDatagramOpenByHostname()
*
* Description : (1) Open a datagram type (UDP) socket to the server using its host name.
*                   (select IP address automatically See Note #2):
*
*                   (a) Get IP address of the remote host from a string that contains either the IP address or
*                       the host name that will be resolved using DNS (See Note #2).
*
*                   (b) Open a datagram socket.
*
*
* Argument(s) : p_sock_id           Pointer to a variable that will receive the socket ID opened from this function.
*
*               p_remote_host_name  Pointer to a string that contains the remote host name to resolve:
*                                       Can be an IP (IPv4 or IPv6) or a host name (resolved using DNS).
*
*               remote_port_nbr     Port of the remote host.
*
*               ip_family           Select IP family of addresses returned by DNS resolution.
*
*                                       NET_IP_ADDR_FAMILY_IPv4
*                                       NET_IP_ADDR_FAMILY_IPv6
*
*               p_sock_addr         Pointer to a variable that will receive the socket address of the remote host.
*
*                                       DEF_NULL, if not required.
*
*               p_is_hostname       Pointer to variable that will received the boolean to indicate if the string
*                                   passed in p_remote_host_name was a hostname or a IP address.
*
*                                   DEF_YES, hostanme was received.
*                                   DEF_NO,  otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                   NET_APP_ERR_NONE                    Socket opening successful.
*                                   NET_ERR_FAULT_NULL_PTR              Null pointer(s) was passed as argument.
*                                   NET_APP_ERR_FAULT                   Operation faulted.
*                                   NET_APP_ERR_INVALID_ADDR_FAMILY     Invalid IP family.
*
*                                   ------------ RETURNED BY NetApp_ClientDatagramOpen() ------------
*                                   See NetApp_ClientDatagramOpen() for additional return error codes.
*
* Return(s)   : NET_IP_ADDR_FAMILY_IPv4,    if the opening was successful using an IPv4 address.
*               NET_IP_ADDR_FAMILY_IPv6,    if the opening was successful using an IPv6 address.
*               NET_IP_ADDR_FAMILY_UNKNOWN, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) When a host name is passed into the remote host name parameter, this function will try to
*                   resolve the address of the remote host using DNS.
*
*                   (a) Obviously DNS must be present and enabled in the project to be possible.
*
*                   (b) the ip_family argument let the application choose the IP family of the addresses
*                       returned by the DNS resolution.
*
*                   (c) This function always block, and the fail timeout depend of the DNS resolution timeout,
*                       the number of remote address found and the connection timeout parameter.
*********************************************************************************************************
*/

NET_IP_ADDR_FAMILY  NetApp_ClientDatagramOpenByHostname (NET_SOCK_ID         *p_sock_id,
                                                         CPU_CHAR            *p_remote_host_name,
                                                         NET_PORT_NBR         remote_port_nbr,
                                                         NET_IP_ADDR_FAMILY   ip_family,
                                                         NET_SOCK_ADDR       *p_sock_addr,
                                                         CPU_BOOLEAN         *p_is_hostname,
                                                         NET_ERR             *p_err)
{
    NET_SOCK_ADDR       *p_sock_addr_local;
    NET_SOCK_ADDR        sock_addr_local;
    NET_IP_ADDR_FAMILY   addr_family;
    CPU_INT08U           addr[NET_SOCK_BSD_ADDR_LEN_MAX];
    CPU_BOOLEAN          do_dns;
#ifdef  NET_EXT_MODULE_DNS_EN
    DNSc_ADDR_OBJ        addr_dns;
    DNSc_STATUS          status;
    DNSc_ERR             err_dns  = DNSc_ERR_NONE;
    DNSc_FLAGS           flags    = DNSc_FLAG_NONE;
    CPU_INT08U           addr_nbr = 1u;
#endif


                                                                /* ---------------- VALIDATE ARGUMENTS ---------------- */
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_sock_id == DEF_NULL) {
        addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;
       *p_err       = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }

    if (p_remote_host_name == DEF_NULL) {
        addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;
       *p_err       = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

                                                                /* ------------- SET SOCK ADDRESS POINTER ------------- */
    if (p_sock_addr == DEF_NULL) {
        p_sock_addr_local = &sock_addr_local;
    } else {
        p_sock_addr_local =  p_sock_addr;
    }

                                                                /* ------------- RESOLVE REMOTE HOST ADDR ------------- */
                                                                /* PARSE STRING FOR IP ADDR FORMAT                      */
    addr_family = NetASCII_Str_to_IP(p_remote_host_name,
                                     addr,
                                     NET_SOCK_BSD_ADDR_LEN_MAX,
                                     p_err);
    if (*p_err != NET_ASCII_ERR_NONE) {
#ifdef  NET_EXT_MODULE_DNS_EN
        do_dns = DEF_YES;
#else
        addr_family   = NET_IP_ADDR_FAMILY_UNKNOWN;
       *p_is_hostname = DEF_NO;
       *p_err         = NET_APP_ERR_FAULT;
        goto exit;
#endif
    } else {
        switch (addr_family) {
            case NET_IP_ADDR_FAMILY_IPv4:
            case NET_IP_ADDR_FAMILY_IPv6:
                *p_is_hostname = DEF_NO;
                 do_dns        = DEF_NO;
                 break;

            default:
#ifdef  NET_EXT_MODULE_DNS_EN
                 do_dns = DEF_YES;
                 break;
#else
                 addr_family   = NET_IP_ADDR_FAMILY_UNKNOWN;
                *p_is_hostname = DEF_NO;
                *p_err         = NET_APP_ERR_INVALID_ADDR_FAMILY;
                 goto exit;
#endif
        }
    }

#ifdef  NET_EXT_MODULE_DNS_EN                                   /* DNS RESOLUTION                                       */
    if (do_dns == DEF_YES) {
                                                                /* Set DNS Parameters                                   */
        DEF_BIT_SET(flags, DNSc_FLAG_FORCE_RESOLUTION);

        switch (ip_family) {
            case NET_IP_ADDR_FAMILY_IPv6:
                 flags          |= DNSc_FLAG_IPv6_ONLY;         /* Get IPv6 address only.                               */
                 break;

            case NET_IP_ADDR_FAMILY_IPv4:
                 flags          |= DNSc_FLAG_IPv4_ONLY;         /* Get IPv4 address only.                               */
                 break;

            default:
                 addr_family   = NET_IP_ADDR_FAMILY_UNKNOWN;
                *p_is_hostname = DEF_YES;
                *p_err         = NET_APP_ERR_INVALID_ADDR_FAMILY;
                 goto exit;
        }

        status = DNSc_GetHost(p_remote_host_name, DEF_NULL, 0u, &addr_dns, &addr_nbr, flags, DEF_NULL, &err_dns);
        switch (status) {
            case DNSc_STATUS_RESOLVED:
                 if (addr_nbr != 0) {
                     Mem_Copy(addr, addr_dns.Addr, addr_dns.Len);
                     switch (addr_dns.Len) {
                         case NET_IPv4_ADDR_LEN:
                         case NET_IPv6_ADDR_LEN:
                              addr_family   = ip_family;
                             *p_is_hostname = DEF_YES;
                              break;

                         default:
                              addr_family   = NET_IP_ADDR_FAMILY_UNKNOWN;
                             *p_is_hostname = DEF_YES;
                             *p_err         = NET_APP_ERR_FAULT;
                              goto exit;
                     }
                 } else {
                     addr_family   = NET_IP_ADDR_FAMILY_UNKNOWN;
                    *p_is_hostname = DEF_YES;
                    *p_err         = NET_APP_ERR_FAULT;
                     goto exit;
                 }
                 break;


            case DNSc_STATUS_FAILED:
            case DNSc_STATUS_PENDING:
            default:
                 addr_family   = NET_IP_ADDR_FAMILY_UNKNOWN;
                *p_is_hostname = DEF_YES;
                *p_err         = NET_APP_ERR_FAULT;
                 goto exit;
        }
    }
#endif

   (void)&do_dns;

                                                                /* ------------ OPEN TO THE REMOTE HOST ------------ */
   *p_sock_id = NetApp_ClientDatagramOpen(addr,
                                          ip_family,
                                          remote_port_nbr,
                                          p_sock_addr_local,
                                          p_err);
    if (*p_err != NET_APP_ERR_NONE) {
         addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;
         goto exit;
    }

   *p_err = NET_APP_ERR_NONE;


exit:
    return (addr_family);
}


/*
*********************************************************************************************************
*                                       NetApp_ClientStreamOpen()
*
* Description : (1) Connect a client to a server using an IP address (IPv4 or IPv6) with a stream socket :
*
*                   (a) Open a stream socket.
*
*                   (b) Set Security parameter (TLS/SSL) if required.
*
*                   (c) Set connection timeout.
*
*                   (d) Connect to the remote host.
*
* Argument(s) : p_addr              Pointer to IP address.
*
*               addr_family         IP family of the address.
*
*               remote_port_nbr     Port of the remote host.
*
*               p_sock_addr         Pointer to a variable that will receive the socket address of the remote host.
*
*                                       DEF_NULL, if not required.
*
*               p_secure_cfg        Pointer to the secure configuration (TLS/SSL):
*
*                                       DEF_NULL, if no security enabled.
*                                       Pointer to a structure that contains the parameters.
*
*               req_timeout_ms      Connection timeout in ms.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : Socket ID, if no error,
*
*               NET_SOCK_ID_NONE, otherwise.
*
* Caller(s)   : Application,
*               NetApp_ClientStreamOpenByHostname().
*
*               This function is an application interface (API) function & MAY be called by application
*               function(s).
*
* Note(s)     : (1) This function is in blocking mode, meaning that at the end of the function, the
*                   socket will have succeed or failed to connect to the remote host.
*********************************************************************************************************
*/

NET_SOCK_ID  NetApp_ClientStreamOpen (CPU_INT08U               *p_addr,
                                      NET_IP_ADDR_FAMILY        addr_family,
                                      NET_PORT_NBR              remote_port_nbr,
                                      NET_SOCK_ADDR            *p_sock_addr,
                                      NET_APP_SOCK_SECURE_CFG  *p_secure_cfg,
                                      CPU_INT32U                req_timeout_ms,
                                      NET_ERR                  *p_err)
{
    NET_SOCK_ID                sock_id = NET_SOCK_ID_NONE;
    CPU_INT08U                 addr_len;
    NET_SOCK_ADDR              sock_addr;
    NET_SOCK_ADDR             *p_sock_addr_local;
    NET_SOCK_PROTOCOL_FAMILY   protocol_family;
    NET_SOCK_ADDR_FAMILY       sock_addr_family;
    CPU_INT08U                 block_state;
    NET_ERR                    err;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_addr == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }

#ifndef  NET_SECURE_MODULE_EN
    if (p_secure_cfg != DEF_NULL) {
        *p_err = NET_ERR_FAULT_FEATURE_DIS;
    }
#endif
#endif

    if (p_sock_addr == DEF_NULL) {
        p_sock_addr_local = &sock_addr;
    } else {
        p_sock_addr_local = p_sock_addr;
    }


                                                                /* ---------- PREPARE PARAMETERS TO CONNECT ----------- */
    switch (addr_family) {
        case NET_IP_ADDR_FAMILY_IPv4:
             protocol_family  = NET_SOCK_PROTOCOL_FAMILY_IP_V4;
             sock_addr_family = NET_SOCK_ADDR_FAMILY_IP_V4;
             addr_len         = NET_IPv4_ADDR_LEN;
             break;

        case NET_IP_ADDR_FAMILY_IPv6:
             protocol_family  = NET_SOCK_PROTOCOL_FAMILY_IP_V6;
             sock_addr_family = NET_SOCK_ADDR_FAMILY_IP_V6;
             addr_len         = NET_IPv6_ADDR_LEN;
             break;

        default:
           *p_err = NET_APP_ERR_INVALID_ADDR_FAMILY;
            goto exit;
    }


                                                                /* ------------------ SET SOCK ADDR ------------------- */
    NetApp_SetSockAddr(p_sock_addr_local, sock_addr_family, remote_port_nbr, p_addr, addr_len, p_err);
    if (*p_err != NET_APP_ERR_NONE) {
        *p_err  = NET_ERR_FAULT_UNKNOWN_ERR;
         goto exit;
    }

                                                                /* -------------------- OPEN SOCK --------------------- */
    sock_id = NetSock_Open(protocol_family, NET_SOCK_TYPE_STREAM, NET_SOCK_PROTOCOL_DFLT, p_err);
    if (*p_err != NET_SOCK_ERR_NONE) {
         goto exit;
    }

                                                                /* ---------------- CFG SOCK BLOCK OPT ---------------- */
    block_state = NetSock_BlockGet(sock_id, p_err);             /* retrieve blocking mode of current socket.            */
    if (*p_err != NET_SOCK_ERR_NONE) {
         goto exit_close;
    }

                                                                /* Set mode to blocking for connect operation.          */
   (void)NetSock_CfgBlock(sock_id, NET_SOCK_BLOCK_SEL_BLOCK, p_err);
    if (*p_err != NET_SOCK_ERR_NONE) {
         goto exit_close;
    }

                                                                /* ----------------- SET CONN TIMEOUT ----------------- */
    NetSock_CfgTimeoutConnReqSet(sock_id,
                                 req_timeout_ms,
                                 p_err);
    if (*p_err != NET_SOCK_ERR_NONE) {
         goto exit_close;
    }

                                                                /* ----------- SET SECURITY CONN PARAMETERS ----------- */
#ifdef  NET_SECURE_MODULE_EN
    if (p_secure_cfg != DEF_NULL) {
        (void)NetSock_CfgSecure(sock_id,
                                DEF_YES,
                                p_err);
         if (*p_err != NET_SOCK_ERR_NONE) {
              goto exit_close;
         }

         NetSock_CfgSecureClientCommonName(            sock_id,
                                           (CPU_CHAR *)p_secure_cfg->CommonName,
                                                       p_err);
         if (*p_err != NET_SOCK_ERR_NONE) {
              goto exit_close;
         }

         NetSock_CfgSecureClientTrustCallBack(sock_id,
                                              p_secure_cfg->TrustCallback,
                                              p_err);
         if (*p_err != NET_SOCK_ERR_NONE) {
              goto exit_close;
         }

         if (p_secure_cfg->MutualAuthPtr != DEF_NULL) {
             NetSock_CfgSecureClientCertKey(sock_id,
                                            p_secure_cfg->MutualAuthPtr->CertPtr,
                                            p_secure_cfg->MutualAuthPtr->CertSize,
                                            p_secure_cfg->MutualAuthPtr->KeyPtr,
                                            p_secure_cfg->MutualAuthPtr->KeySize,
                                            p_secure_cfg->MutualAuthPtr->Fmt,
                                            p_secure_cfg->MutualAuthPtr->CertChained,
                                            p_err);
             if (*p_err != NET_SOCK_ERR_NONE) {
                  goto exit_close;
             }
         }

    }
#endif


                                                                /* ------------- CONN TO THE REMOTE HOST -------------- */
    NetSock_Conn(sock_id, p_sock_addr_local, addr_len, p_err);
    switch (*p_err) {
        case NET_SOCK_ERR_NONE:
            *p_err = NET_APP_ERR_NONE;
             break;

        case NET_SOCK_ERR_CONN_IN_PROGRESS:
            *p_err = NET_APP_ERR_CONN_IN_PROGRESS;
             break;

        case NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT:
            *p_err = NET_APP_ERR_CONN_FAIL;
             goto exit_close;

        default:
             goto exit_close;
    }

                                                                /* ---------------- CFG SOCK BLOCK OPT ---------------- */
   (void)NetSock_CfgBlock(sock_id, block_state, &err);          /* re-configure blocking mode to one before connect.    */
    if (err != NET_SOCK_ERR_NONE) {
       *p_err = err;
        goto exit_close;
    }

    (void)&p_secure_cfg;                                        /* Prevent 'variable unused' compiler warning.          */

    goto exit;


exit_close:
    NetSock_Close(sock_id, &err);
   (void)&err;                                                  /* Prevent 'variable unused' compiler warning.          */
    sock_id = NET_SOCK_ID_NONE;

exit:
    return (sock_id);
}


/*
*********************************************************************************************************
*                                      NetApp_ClientDatagramOpen()
*
* Description : (1) Connect a client to a server using an IP address (IPv4 or IPv6) with a datagram socket :
*
*                   (a) Open a datagram socket.
*                   (b) Set connection timeout.
*
*
* Argument(s) : p_addr              Pointer to IP address.
*
*               addr_family         IP family of the address.
*
*               remote_port_nbr     Port of the remote host.
*
*               p_sock_addr         Pointer to a variable that will receive the socket address of the remote host.
*
*                                       DEF_NULL, if not required.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       NET_APP_ERR_NONE
*                                       NET_ERR_FAULT_NULL_PTR
*                                       NET_APP_ERR_INVALID_ADDR_FAMILY
*
* Return(s)   : Socket ID, if no error,
*
*               NET_SOCK_ID_NONE, otherwise.
*
* Caller(s)   : NetApp_ClientOpenByHostname().
*
* Note(s)     : None.
*********************************************************************************************************
*/

NET_SOCK_ID  NetApp_ClientDatagramOpen (CPU_INT08U          *p_addr,
                                        NET_IP_ADDR_FAMILY   addr_family,
                                        NET_PORT_NBR         remote_port_nbr,
                                        NET_SOCK_ADDR       *p_sock_addr,
                                        NET_ERR             *p_err)
{
    NET_SOCK_ID                sock_id = NET_SOCK_ID_NONE;
    CPU_INT08U                 addr_len;
    NET_SOCK_ADDR              sock_addr;
    NET_SOCK_ADDR             *p_sock_addr_local;
    NET_SOCK_PROTOCOL_FAMILY   protocol_family;
    NET_SOCK_ADDR_FAMILY       sock_addr_family;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_addr == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit;
    }
#endif

    if (p_sock_addr == DEF_NULL) {
        p_sock_addr_local = &sock_addr;
    } else {
        p_sock_addr_local = p_sock_addr;
    }


                                                                /* ---------- PREPARE PARAMETERS TO CONNECT ----------- */
    switch (addr_family) {
        case NET_IP_ADDR_FAMILY_IPv4:
             protocol_family  = NET_SOCK_PROTOCOL_FAMILY_IP_V4;
             sock_addr_family = NET_SOCK_ADDR_FAMILY_IP_V4;
             addr_len         = NET_IPv4_ADDR_LEN;
             break;

        case NET_IP_ADDR_FAMILY_IPv6:
             protocol_family  = NET_SOCK_PROTOCOL_FAMILY_IP_V6;
             sock_addr_family = NET_SOCK_ADDR_FAMILY_IP_V6;
             addr_len         = NET_IPv6_ADDR_LEN;
             break;

        default:
           *p_err = NET_APP_ERR_INVALID_ADDR_FAMILY;
            goto exit;
    }


                                                                /* ------------------ SET SOCK ADDR ------------------- */
    NetApp_SetSockAddr(p_sock_addr_local, sock_addr_family, remote_port_nbr, p_addr, addr_len, p_err);
    if (*p_err != NET_APP_ERR_NONE) {
         goto exit;
    }

                                                                /* -------------------- OPEN SOCK --------------------- */
    sock_id = NetSock_Open(protocol_family, NET_SOCK_TYPE_DATAGRAM, NET_SOCK_PROTOCOL_DFLT, p_err);
    if (*p_err != NET_SOCK_ERR_NONE) {
        *p_err  = NET_APP_ERR_FAULT;
         goto exit;
    }

   *p_err = NET_APP_ERR_NONE;
    goto exit;


exit:
    return (sock_id);
}


/*
*********************************************************************************************************
*                                         NetApp_TimeDly_ms()
*
* Description : Delay for specified time, in milliseconds.
*
* Argument(s) : time_dly_ms     Time delay value, in milliseconds (see Note #1).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_APP_ERR_NONE                Time delay successful.
*                               NET_APP_ERR_INVALID_ARG         Time delay invalid.
*                               NET_APP_ERR_FAULT               Time delay fault.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*               NetApp_SockOpen(),
*               NetApp_SockBind(),
*               NetApp_SockConn(),
*               NetApp_SockAccept(),
*               NetApp_SockRx(),
*               NetApp_SockTx().
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) Time delay of 0 milliseconds allowed.
*
*                   (b) Time delay limited to the maximum possible OS time delay
*                             if greater than the maximum possible OS time delay.
*
*                   See also 'KAL/kal.c  KAL_Dly()  Note #1'.
*********************************************************************************************************
*/

void  NetApp_TimeDly_ms (CPU_INT32U   time_dly_ms,
                         NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif

    KAL_Dly(time_dly_ms);

   *p_err = NET_APP_ERR_NONE;
}

