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
* Filename : net_mgr.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Network layer manager MAY eventually maintain each interface's network address(s)
*                & address configuration. #### NET-809
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
#define    NET_MGR_MODULE
#include  "net_mgr.h"
#include  "net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#endif


#include  "net_err.h"
#include  "../IF/net_if.h"


/*
*********************************************************************************************************
*                                            NetMgr_Init()
*
* Description : (1) Initialize Network Layer Management Module :
*
*                   Module initialization NOT yet required/implemented
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MGR_ERR_NONE            Network manager module successfully initialized.
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

void  NetMgr_Init (NET_ERR  *p_err)
{
   *p_err = NET_MGR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetMgr_GetHostAddrProtocol()
*
* Description : Get an interface's protocol address(s) [see Note #1].
*
* Argument(s) : if_nbr                      Interface number to get protocol address(s).
*
*               protocol_type               Protocol address type.
*
*               p_addr_protocol_tbl         Pointer to a protocol address table that will receive the protocol
*                                               address(s) in network-order for this interface.
*
*               p_addr_protocol_tbl_qty     Pointer to a variable to ... :
*
*                                               (a) Pass the size of the protocol address table, in number of
*                                                       protocol address(s), pointed to by 'p_addr_protocol_tbl'.
*                                               (b) (1) Return the actual number of protocol address(s),
*                                                           if NO error(s);
*                                                   (2) Return 0, otherwise.
*
*               p_addr_protocol_len         Pointer to a variable to ... :
*
*                                               (a) Pass the length of the protocol address table address(s),
*                                                       in octets.
*                                               (b) (1) Return the actual length of the protocol  address(s),
*                                                           in octets, if NO error(s);
*                                                   (2) Return 0, otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MGR_ERR_NONE                    Interface's protocol address(s)
*                                                                       successfully returned.
*                               NET_ERR_FAULT_UNKNOWN_ERR           Interface's protocol address(s) NOT
*                                                                       successfully returned.
*                               NET_ERR_FAULT_NULL_PTR              Argument(s) passed a NULL pointer.
*                               NET_MGR_ERR_INVALID_PROTOCOL        Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN    Invalid protocol address length.
*                               NET_MGR_ERR_ADDR_TBL_SIZE           Invalid protocol address table size.
*                               NET_MGR_ERR_ADDR_CFG_IN_PROGRESS    Interface in address configuration/
*                                                                       initialization state.
*
*                                                                   - RETURNED BY Net&&&_GetHostAddrProtocol() : -
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_RxPktIsTargetThisHost().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Protocol address(s) returned in network-order.
*********************************************************************************************************
*/

void  NetMgr_GetHostAddrProtocol (NET_IF_NBR          if_nbr,
                                  NET_PROTOCOL_TYPE   protocol_type,
                                  CPU_INT08U         *p_addr_protocol_tbl,
                                  CPU_INT08U         *p_addr_protocol_tbl_qty,
                                  CPU_INT08U         *p_addr_protocol_len,
                                  NET_ERR            *p_err)
{
    NET_ERR  err;


    switch (protocol_type) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             NetIPv4_GetHostAddrProtocol(if_nbr,
                                         p_addr_protocol_tbl,
                                         p_addr_protocol_tbl_qty,
                                         p_addr_protocol_len,
                                        &err);
             switch (err) {
                 case NET_IF_ERR_INVALID_IF:
                     *p_err = err;
                      break;


                 case NET_IPv4_ERR_NONE:
                     *p_err = NET_MGR_ERR_NONE;
                      break;


                 case NET_ERR_FAULT_NULL_PTR:
                     *p_err = NET_ERR_FAULT_NULL_PTR;
                      break;


                 case NET_IPv4_ERR_INVALID_ADDR_LEN:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_LEN;
                      break;


                 case NET_IPv4_ERR_ADDR_TBL_SIZE:
                     *p_err = NET_MGR_ERR_ADDR_TBL_SIZE;
                      break;


                 case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
                     *p_err = NET_MGR_ERR_ADDR_CFG_IN_PROGRESS;
                      break;


                 default:
                     *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
                      break;
             }
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V6:
             NetIPv6_GetHostAddrProtocol(if_nbr,
                                         p_addr_protocol_tbl,
                                         p_addr_protocol_tbl_qty,
                                         p_addr_protocol_len,
                                        &err);
             switch (err) {
                 case NET_IF_ERR_INVALID_IF:
                     *p_err = err;
                      break;


                 case NET_IPv6_ERR_NONE:
                     *p_err = NET_MGR_ERR_NONE;
                      break;


                 case NET_ERR_FAULT_NULL_PTR:
                     *p_err = NET_ERR_FAULT_NULL_PTR;
                      break;


                 case NET_IPv6_ERR_INVALID_ADDR_LEN:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_LEN;
                      break;


                 case NET_IPv6_ERR_ADDR_TBL_SIZE:
                     *p_err = NET_MGR_ERR_ADDR_TBL_SIZE;
                      break;


                 case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
                     *p_err = NET_MGR_ERR_ADDR_CFG_IN_PROGRESS;
                      break;


                 default:
                     *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
                      break;
             }
             break;
#endif

        default:
            *p_addr_protocol_tbl_qty = 0u;
            *p_addr_protocol_len     = 0u;
            *p_err                   = NET_MGR_ERR_INVALID_PROTOCOL;
             break;
    }
}


/*
*********************************************************************************************************
*                                 NetMgr_GetHostAddrProtocolIF_Nbr()
*
* Description : Get the interface number for a protocol address.
*
* Argument(s) : protocol_type       Address    protocol type.
*
*               p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MGR_ERR_NONE                    Protocol address's interface number
*                                                                           successfully returned.
*                               NET_ERR_FAULT_UNKNOWN_ERR                   Protocol address's interface number
*                                                                       NOT successfully returned.
*                               NET_ERR_FAULT_NULL_PTR                Argument(s) passed a NULL pointer.
*                               NET_MGR_ERR_INVALID_PROTOCOL        Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN    Invalid protocol address length.
*                               NET_MGR_ERR_INVALID_PROTOCOL_ADDR   Protocol address NOT used by host.
*
* Return(s)   : Interface number for the protocol address, if configured on this host.
*
*               Interface number of  a   protocol address
*                   in address initialization,             if any.
*
*               NET_IF_NBR_LOCAL_HOST,                     for a localhost address.
*
*               NET_IF_NBR_NONE,                           otherwise.
*
* Caller(s)   : NetARP_TxReqGratuitous(),
*               NetARP_CacheProbeAddrOnNet().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Protocol address MUST be in network-order.
*********************************************************************************************************
*/

NET_IF_NBR  NetMgr_GetHostAddrProtocolIF_Nbr (NET_PROTOCOL_TYPE   protocol_type,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len,
                                              NET_ERR            *p_err)
{
#ifdef  NET_IP_MODULE_EN
    NET_ERR     err;
#endif
    NET_IF_NBR  if_nbr;


    switch (protocol_type) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             if_nbr = NetIPv4_GetAddrProtocolIF_Nbr(p_addr_protocol,
                                                    addr_protocol_len,
                                                   &err);
             switch (err) {
                 case NET_IPv4_ERR_NONE:
                     *p_err = NET_MGR_ERR_NONE;
                      break;


                 case NET_ERR_FAULT_NULL_PTR:
                     *p_err = NET_ERR_FAULT_NULL_PTR;
                      break;


                 case NET_IPv4_ERR_INVALID_ADDR_HOST:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_ADDR;
                      break;


                 case NET_IPv4_ERR_INVALID_ADDR_LEN:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_LEN;
                      break;


                 default:
                     *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
                      break;
             }
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V6:
             if_nbr = NetIPv6_GetAddrProtocolIF_Nbr(p_addr_protocol,
                                                    addr_protocol_len,
                                                   &err);
             switch (err) {
                 case NET_IPv6_ERR_NONE:
                     *p_err = NET_MGR_ERR_NONE;
                      break;


                 case NET_ERR_FAULT_NULL_PTR:
                     *p_err = NET_ERR_FAULT_NULL_PTR;
                      break;


                 case NET_IPv6_ERR_INVALID_ADDR_HOST:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_ADDR;
                      break;


                 case NET_IPv6_ERR_INVALID_ADDR_LEN:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_LEN;
                      break;


                 default:
                     *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
                      break;
             }
             break;
#endif

        default:
             if_nbr = NET_IF_NBR_NONE;
            *p_err  = NET_MGR_ERR_INVALID_PROTOCOL;
             break;
    }

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                    NetMgr_IsValidAddrProtocol()
*
* Description : Validate a protocol address.
*
* Argument(s) : protocol_type       Address    protocol type.
*
*               p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if protocol address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetARP_RxPktValidate().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Protocol address MUST be in network-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMgr_IsValidAddrProtocol (NET_PROTOCOL_TYPE   protocol_type,
                                         CPU_INT08U         *p_addr_protocol,
                                         CPU_INT08U          addr_protocol_len)
{
    CPU_BOOLEAN  valid;


    switch (protocol_type) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             valid = NetIPv4_IsValidAddrProtocol(p_addr_protocol, addr_protocol_len);
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V6:
             valid = NetIPv6_IsValidAddrProtocol(p_addr_protocol, addr_protocol_len);
             break;
#endif

        case NET_PROTOCOL_TYPE_NONE:
        default:
             valid = DEF_NO;
             break;
    }

    return (valid);
}


/*
*********************************************************************************************************
*                                      NetMgr_IsAddrsCfgdOnIF()
*
* Description : Check if any address(s) configured on an interface.
*
* Argument(s) : if_nbr      Interface number to check for configured address(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MGR_ERR_NONE                Configured address(s) availability
*                                                                        successfully returned.
*                               NET_ERR_FAULT_UNKNOWN_ERR               Interface's address(s) availability
*                                                                    NOT successfully returned.
*
*                                                               - RETURNED BY Net&&&_IsAddrsCfgdOnIF() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : DEF_YES, if any protocol address(s) configured on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIF_GetDflt().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network layer manager SHOULD check configured address(s) availability based on
*                   address(s)' protocol type.
*
*                   See also 'net_mgr.c  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMgr_IsAddrsCfgdOnIF (NET_IF_NBR   if_nbr,
                                     NET_ERR     *p_err)
{
    CPU_BOOLEAN  addr_availv4;
    CPU_BOOLEAN  addr_availv6;
    NET_ERR      err;

#ifdef  NET_IPv4_MODULE_EN
    addr_availv4 = NetIPv4_IsAddrsCfgdOnIF_Handler(if_nbr, &err);
    switch (err) {
        case NET_IF_ERR_INVALID_IF:
            *p_err = err;
             break;


        case NET_IPv4_ERR_NONE:
            *p_err = NET_MGR_ERR_NONE;
             break;


        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             break;
    }
#else
    addr_availv4 = DEF_NO;
#endif

#ifdef  NET_IPv6_MODULE_EN
    addr_availv6 = NetIPv6_IsAddrsCfgdOnIF_Handler(if_nbr, &err);
    switch (err) {
        case NET_IF_ERR_INVALID_IF:
            *p_err = err;
             break;


        case NET_IPv6_ERR_NONE:
            *p_err = NET_MGR_ERR_NONE;
             break;


        default:
            *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
             break;
    }
#else
    addr_availv6 = DEF_NO;
#endif



    return ((addr_availv4 || addr_availv6));
}


/*
*********************************************************************************************************
*                                     NetMgr_IsAddrProtocolInit()
*
* Description : Validate a protocol address as the initialization address.
*
* Argument(s) : protocol_type       Address    protocol type.
*
*               p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if protocol address is the protocol's initialization address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetARP_RxPktIsTargetThisHost().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Protocol address MUST be in network-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetMgr_IsAddrProtocolInit (NET_PROTOCOL_TYPE   protocol_type,
                                        CPU_INT08U         *p_addr_protocol,
                                        CPU_INT08U          addr_protocol_len)
{
    CPU_BOOLEAN  addr_init;


    switch (protocol_type) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             addr_init = NetIPv4_IsAddrInit(p_addr_protocol, addr_protocol_len);
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V6:
             addr_init = NetIPv6_IsAddrInit(p_addr_protocol, addr_protocol_len);
             break;
#endif

        case NET_PROTOCOL_TYPE_NONE:
        default:
             addr_init = DEF_NO;
             break;
    }

    return (addr_init);
}


/*
*********************************************************************************************************
*                                   NetMgr_IsAddrProtocolMulticast()
*
* Description : Validate a protocol address as a multicast address.
*
* Argument(s) : protocol_type       Address    protocol type.
*
*               p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if protocol address is a multicast address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetARP_CacheHandler().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Protocol address MUST be in network-order.
*********************************************************************************************************
*/

#ifdef  NET_MCAST_MODULE_EN
CPU_BOOLEAN  NetMgr_IsAddrProtocolMulticast (NET_PROTOCOL_TYPE   protocol_type,
                                             CPU_INT08U         *p_addr_protocol,
                                             CPU_INT08U          addr_protocol_len)
{
    CPU_BOOLEAN  addr_multicast;


    switch (protocol_type) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             addr_multicast = NetIPv4_IsAddrProtocolMulticast(p_addr_protocol, addr_protocol_len);
             break;
#endif

#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V6:
             addr_multicast = NetIPv6_IsAddrProtocolMulticast(p_addr_protocol, addr_protocol_len);
             break;
#endif

        case NET_PROTOCOL_TYPE_NONE:
        default:
             addr_multicast = DEF_NO;
             break;
    }

    return (addr_multicast);
}
#endif


/*
*********************************************************************************************************
*                                   NetMgr_IsAddrProtocolConflict()
*
* Description : Get interface's protocol address conflict status.
*
* Argument(s) : if_nbr      Interface number to get protocol address conflict status.
*
* Return(s)   : DEF_YES, if protocol address conflict detected.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetARP_IsAddrProtocolConflict().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network layer manager SHOULD get address conflict status based on address's
*                   protocol type.
*
*                   See also 'net_mgr.c  Note #1'.
*********************************************************************************************************
*/
#ifdef  NET_IPv4_MODULE_EN
CPU_BOOLEAN  NetMgr_IsAddrProtocolConflict (NET_IF_NBR  if_nbr)
{
    CPU_BOOLEAN  addr_conflict;


    addr_conflict = NetIPv4_IsAddrProtocolConflict(if_nbr);

    return (addr_conflict);
}
#endif


/*
*********************************************************************************************************
*                                  NetMgr_ChkAddrProtocolConflict()
*
* Description : Check for any protocol address conflict between this interface's host address(s) & other
*                   host(s) on the local network.
*
* Argument(s) : if_nbr              Interface number to check protocol address conflict.
*
*               protocol_type       Protocol address type.
*
*               p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length of  protocol address (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_MGR_ERR_NONE                    Interface's protocol address conflict
*                                                                            successfully checked.
*                               NET_ERR_FAULT_UNKNOWN_ERR                   Interface's protocol address conflict
*                                                                        NOT successfully returned.
*                               NET_ERR_FAULT_NULL_PTR                Argument(s) passed a NULL pointer.
*                               NET_MGR_ERR_INVALID_PROTOCOL        Invalid/unsupported network protocol.
*                               NET_MGR_ERR_INVALID_PROTOCOL_LEN    Invalid protocol address length.
*                               NET_MGR_ERR_ADDR_TBL_SIZE           Invalid protocol address table size.
*                               NET_MGR_ERR_ADDR_CFG_IN_PROGRESS    Interface in address configuration/
*                                                                       initialization state.
*
*                                                                   - RETURNED BY Net&&&_ChkAddrProtocolConflict() : -
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetARP_RxPktIsTargetThisHost().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Protocol address MUST be in network-order.
*********************************************************************************************************
*/

void  NetMgr_ChkAddrProtocolConflict (NET_IF_NBR          if_nbr,
                                      NET_PROTOCOL_TYPE   protocol_type,
                                      CPU_INT08U         *p_addr_protocol,
                                      CPU_INT08U          addr_protocol_len,
                                      NET_ERR            *p_err)
{
#ifdef  NET_IP_MODULE_EN
    NET_ERR  err;
#endif

    switch (protocol_type) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_IP_V4:
             NetIPv4_ChkAddrProtocolConflict(if_nbr,
                                             p_addr_protocol,
                                             addr_protocol_len,
                                            &err);
             switch (err) {
                 case NET_IF_ERR_INVALID_IF:
                     *p_err = err;
                      break;


                 case NET_IPv4_ERR_NONE:
                     *p_err = NET_MGR_ERR_NONE;
                      break;


                 case NET_ERR_FAULT_NULL_PTR:
                     *p_err = NET_ERR_FAULT_NULL_PTR;
                      break;


                 case NET_IPv4_ERR_INVALID_ADDR_LEN:
                     *p_err = NET_MGR_ERR_INVALID_PROTOCOL_LEN;
                      break;


                 case NET_IPv4_ERR_ADDR_TBL_SIZE:
                     *p_err = NET_MGR_ERR_ADDR_TBL_SIZE;
                      break;


                 case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
                     *p_err = NET_MGR_ERR_ADDR_CFG_IN_PROGRESS;
                      break;


                 default:
                     *p_err = NET_ERR_FAULT_UNKNOWN_ERR;
                      break;
             }
             break;
#endif

        case NET_PROTOCOL_TYPE_IP_V6:
        default:
            *p_err = NET_MGR_ERR_INVALID_PROTOCOL;
             break;
    }
}

