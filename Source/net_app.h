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
* Filename : net_app.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) Network Application Programming Interface (API) Layer module is required for :
*
*               (a) Applications that require network application programming interface (API) :
*                   (1) Network socket API with error handling
*                   (2) Network time delays
*
*               See also 'net_cfg.h  NETWORK APPLICATION PROGRAMMING INTERFACE (API) LAYER CONFIGURATION'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef   NET_APP_MODULE_PRESENT
#define   NET_APP_MODULE_PRESENT

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  "net_sock.h"
#include  "net_type.h"
#include  "net_ip.h"
#include  "net_err.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                NETWORK APPLICATION TIME DELAY DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_APP_TIME_DLY_MIN_SEC                        DEF_INT_32U_MIN_VAL
#define  NET_APP_TIME_DLY_MAX_SEC                        DEF_INT_32U_MAX_VAL

#define  NET_APP_TIME_DLY_MIN_mS                         DEF_INT_32U_MIN_VAL
#define  NET_APP_TIME_DLY_MAX_mS                        (DEF_TIME_NBR_mS_PER_SEC - 1u)


typedef  struct  net_app_sock_secure_mutual_cfg {
    NET_SOCK_SECURE_CERT_KEY_FMT  Fmt;
    CPU_CHAR                     *CertPtr;
    CPU_INT32U                    CertSize;
    CPU_BOOLEAN                   CertChained;
    CPU_CHAR                     *KeyPtr;
    CPU_INT32U                    KeySize;
} NET_APP_SOCK_SECURE_MUTUAL_CFG;


typedef  struct  net_app_sock_secure_cfg {
    CPU_CHAR                        *CommonName;
    NET_SOCK_SECURE_TRUST_FNCT       TrustCallback;
    NET_APP_SOCK_SECURE_MUTUAL_CFG  *MutualAuthPtr;
} NET_APP_SOCK_SECURE_CFG;


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

                                                                /* -------------------- SOCK FNCTS -------------------- */
NET_SOCK_ID         NetApp_SockOpen                     (NET_SOCK_PROTOCOL_FAMILY   protocol_family,
                                                         NET_SOCK_TYPE              sock_type,
                                                         NET_SOCK_PROTOCOL          protocol,
                                                         CPU_INT16U                 retry_max,
                                                         CPU_INT32U                 time_dly_ms,
                                                         NET_ERR                   *p_err);

CPU_BOOLEAN         NetApp_SockClose                    (NET_SOCK_ID                sock_id,
                                                         CPU_INT32U                 timeout_ms,
                                                         NET_ERR                   *p_err);


CPU_BOOLEAN         NetApp_SockBind                     (NET_SOCK_ID                sock_id,
                                                         NET_SOCK_ADDR             *p_addr_local,
                                                         NET_SOCK_ADDR_LEN          addr_len,
                                                         CPU_INT16U                 retry_max,
                                                         CPU_INT32U                 time_dly_ms,
                                                         NET_ERR                   *p_err);

CPU_BOOLEAN         NetApp_SockConn                     (NET_SOCK_ID                sock_id,
                                                         NET_SOCK_ADDR             *p_addr_remote,
                                                         NET_SOCK_ADDR_LEN          addr_len,
                                                         CPU_INT16U                 retry_max,
                                                         CPU_INT32U                 timeout_ms,
                                                         CPU_INT32U                 time_dly_ms,
                                                         NET_ERR                   *p_err);


CPU_BOOLEAN         NetApp_SockListen                   (NET_SOCK_ID                sock_id,
                                                         NET_SOCK_Q_SIZE            sock_q_size,
                                                         NET_ERR                   *p_err);

NET_SOCK_ID         NetApp_SockAccept                   (NET_SOCK_ID                sock_id,
                                                         NET_SOCK_ADDR             *p_addr_remote,
                                                         NET_SOCK_ADDR_LEN         *p_addr_len,
                                                         CPU_INT16U                 retry_max,
                                                         CPU_INT32U                 timeout_ms,
                                                         CPU_INT32U                 time_dly_ms,
                                                         NET_ERR                   *p_err);


CPU_INT16U          NetApp_SockRx                       (NET_SOCK_ID               sock_id,
                                                         void                     *p_data_buf,
                                                         CPU_INT16U                data_buf_len,
                                                         CPU_INT16U                data_rx_th,
                                                         NET_SOCK_API_FLAGS        flags,
                                                         NET_SOCK_ADDR            *p_addr_remote,
                                                         NET_SOCK_ADDR_LEN        *p_addr_len,
                                                         CPU_INT16U                retry_max,
                                                         CPU_INT32U                timeout_ms,
                                                         CPU_INT32U                time_dly_ms,
                                                         NET_ERR                  *p_err);

CPU_INT16U          NetApp_SockTx                       (NET_SOCK_ID               sock_id,
                                                         void                     *p_data,
                                                         CPU_INT16U                data_len,
                                                         NET_SOCK_API_FLAGS        flags,
                                                         NET_SOCK_ADDR            *p_addr_remote,
                                                         NET_SOCK_ADDR_LEN         addr_len,
                                                         CPU_INT16U                retry_max,
                                                         CPU_INT32U                timeout_ms,
                                                         CPU_INT32U                time_dly_ms,
                                                         NET_ERR                  *p_err);


void                NetApp_SetSockAddr                  (NET_SOCK_ADDR            *p_sock_addr,
                                                         NET_SOCK_ADDR_FAMILY      addr_family,
                                                         NET_PORT_NBR              port_nbr,
                                                         CPU_INT08U               *p_addr,
                                                         NET_IP_ADDR_LEN           addr_len,
                                                         NET_ERR                  *p_err);

                                                                /* ---------- ADVANCED STREAM OPEN FUNCTION ----------- */
NET_IP_ADDR_FAMILY  NetApp_ClientStreamOpenByHostname   (NET_SOCK_ID              *p_sock_id,
                                                         CPU_CHAR                 *p_host_server,
                                                         NET_PORT_NBR              port_nbr,
                                                         NET_SOCK_ADDR            *p_sock_addr,
                                                         NET_APP_SOCK_SECURE_CFG  *p_secure_cfg,
                                                         CPU_INT32U                req_timeout_ms,
                                                         NET_ERR                  *p_err);

NET_IP_ADDR_FAMILY  NetApp_ClientDatagramOpenByHostname (NET_SOCK_ID              *p_sock_id,
                                                         CPU_CHAR                 *p_remote_host_name,
                                                         NET_PORT_NBR              remote_port_nbr,
                                                         NET_IP_ADDR_FAMILY        ip_family,
                                                         NET_SOCK_ADDR            *p_sock_addr,
                                                         CPU_BOOLEAN              *p_is_hostname,
                                                         NET_ERR                  *p_err);

NET_SOCK_ID         NetApp_ClientStreamOpen             (CPU_INT08U               *p_addr,
                                                         NET_IP_ADDR_FAMILY        addr_family,
                                                         NET_PORT_NBR              remote_port_nbr,
                                                         NET_SOCK_ADDR            *p_sock_addr,
                                                         NET_APP_SOCK_SECURE_CFG  *p_secure_cfg,
                                                         CPU_INT32U                req_timeout_ms,
                                                         NET_ERR                  *p_err);


NET_SOCK_ID         NetApp_ClientDatagramOpen           (CPU_INT08U               *p_addr,
                                                         NET_IP_ADDR_FAMILY        addr_family,
                                                         NET_PORT_NBR              remote_port_nbr,
                                                         NET_SOCK_ADDR            *p_sock_addr,
                                                         NET_ERR                  *p_err);


                                                                /* ------------------ TIME DLY FNCTS ------------------ */
void                NetApp_TimeDly_ms                   (CPU_INT32U                 time_dly_ms,
                                                         NET_ERR                   *p_err);



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

#endif  /* NET_APP_MODULE_PRESENT */

