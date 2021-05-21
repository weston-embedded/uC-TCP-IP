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
*                                      NETWORK LAYER MANAGEMENT
*
* Filename : net_mgr.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Network layer manager MAY eventually maintain each interface's network address(s)
*                & address configuration. #### NET-809
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_MGR_MODULE_PRESENT
#define  NET_MGR_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  "net_err.h"
#include  "../IF/net_if.h"


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

void         NetMgr_Init                     (NET_ERR            *p_err);


                                                                                    /* ------ NET MGR ADDR FNCTS ------ */
void         NetMgr_GetHostAddrProtocol      (NET_IF_NBR          if_nbr,
                                              NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol_tbl,
                                              CPU_INT08U         *p_addr_protocol_tbl_qty,
                                              CPU_INT08U         *p_addr_protocol_len,
                                              NET_ERR            *p_err);

NET_IF_NBR   NetMgr_GetHostAddrProtocolIF_Nbr(NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len,
                                              NET_ERR            *p_err);


CPU_BOOLEAN  NetMgr_IsValidAddrProtocol      (NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len);

CPU_BOOLEAN  NetMgr_IsAddrsCfgdOnIF          (NET_IF_NBR          if_nbr,
                                              NET_ERR            *p_err);

CPU_BOOLEAN  NetMgr_IsAddrProtocolInit       (NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len);

#ifdef  NET_MCAST_MODULE_EN
CPU_BOOLEAN  NetMgr_IsAddrProtocolMulticast  (NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len);
#endif

#ifdef  NET_IPv4_MODULE_EN
CPU_BOOLEAN  NetMgr_IsAddrProtocolConflict   (NET_IF_NBR          if_nbr);
#endif

void         NetMgr_ChkAddrProtocolConflict  (NET_IF_NBR          if_nbr,
                                              NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len,
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

#endif  /* NET_MGR_MODULE_PRESENT */
