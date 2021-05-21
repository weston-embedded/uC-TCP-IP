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
*                                            BSD 4.x LAYER
*
* Filename  : net_bsd.c
* Version   : V3.05.04
*********************************************************************************************************
* Note(s)   : (1) Supports BSD 4.x Layer API with the following restrictions/constraints :
*
*                 (a) ONLY supports the following BSD layer functionality :
*                     (1) BSD sockets                                           See 'net_sock.h  Note #1'
*
*                 (b) Return variable 'errno' NOT currently supported
*
*********************************************************************************************************
* Notice(s) : (1) The Institute of Electrical and Electronics Engineers and The Open Group, have given
*                 us permission to reprint portions of their documentation.  Portions of this text are
*                 reprinted and reproduced in electronic form from the IEEE Std 1003.1, 2004 Edition,
*                 Standard for Information Technology -- Portable Operating System Interface (POSIX),
*                 The Open Group Base Specifications Issue 6, Copyright (C) 2001-2004 by the Institute
*                 of Electrical and Electronics Engineers, Inc and The Open Group.  In the event of any
*                 discrepancy between these versions and the original IEEE and The Open Group Standard,
*                 the original IEEE and The Open Group Standard is the referee document.  The original
*                 Standard can be obtained online at http://www.opengroup.org/unix/online.html.
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
#define    NET_BSD_MODULE

#include  "net_cfg_net.h"
#include  <lib_str.h>

#ifdef  NET_SOCK_BSD_EN
#include  "net_bsd.h"
#include  "net_sock.h"
#include  "net_util.h"
#include  "net_err.h"
#include  "net_app.h"
#include  "net_dict.h"
#include  "net_def.h"
#include  "net_tcp.h"
#include  "net_conn.h"
#include  "net_buf.h"
#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
#include  <Source/dns-c.h>
#include  <Source/dns-c_cache.h>
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_BSD_SERVICE_FTP_DATA                      20
#define  NET_BSD_SERVICE_FTP                           21
#define  NET_BSD_SERVICE_TELNET                        23
#define  NET_BSD_SERVICE_SMTP                          25
#define  NET_BSD_SERVICE_DNS                           53
#define  NET_BSD_SERVICE_BOOTPS                        67
#define  NET_BSD_SERVICE_BOOTPC                        68
#define  NET_BSD_SERVICE_TFTP                          69
#define  NET_BSD_SERVICE_DNS                           53
#define  NET_BSD_SERVICE_HTTP                          80
#define  NET_BSD_SERVICE_NTP                          123
#define  NET_BSD_SERVICE_SNMP                         161
#define  NET_BSD_SERVICE_HTTPS                        443
#define  NET_BSD_SERVICE_SMTPS                        465

                                                                /* Service name sizes must NOT exceed NI_MAXSERV.       */
#define  NET_BSD_SERVICE_FTP_DATA_STR               "ftp_data"
#define  NET_BSD_SERVICE_FTP_STR                    "ftp"
#define  NET_BSD_SERVICE_TELNET_STR                 "telnet"
#define  NET_BSD_SERVICE_SMTP_STR                   "smtp"
#define  NET_BSD_SERVICE_DNS_STR                    "dns"
#define  NET_BSD_SERVICE_BOOTPS_STR                 "bootps"
#define  NET_BSD_SERVICE_BOOTPC_STR                 "bootpc"
#define  NET_BSD_SERVICE_TFTP_STR                   "tftp"
#define  NET_BSD_SERVICE_HTTP_STR                   "http"
#define  NET_BSD_SERVICE_SNMP_STR                   "snmp"
#define  NET_BSD_SERVICE_NTP_STR                    "ntp"
#define  NET_BSD_SERVICE_HTTPS_STR                  "https"
#define  NET_BSD_SERVICE_SMTPS_STR                  "smtps"


#define  NET_BSD_EAI_ERR_ADDRFAMILY_STR             "Address family for node_name not supported."
#define  NET_BSD_EAI_ERR_AGAIN_STR                  "Temporary failure in name resolution."
#define  NET_BSD_EAI_ERR_BADFLAGS_STR               "Invalid value for ai_flags."
#define  NET_BSD_EAI_ERR_FAIL_STR                   "Non-recoverable failure in name resolution."
#define  NET_BSD_EAI_ERR_FAMILY_STR                 "ai_family not supported."
#define  NET_BSD_EAI_ERR_MEMORY_STR                 "Memory allocation failure."
#define  NET_BSD_EAI_ERR_NONAME_STR                 "node_name or service_name not provided, or not known."
#define  NET_BSD_EAI_ERR_OVERFLOW_STR               "argument buffer overflow."
#define  NET_BSD_EAI_ERR_SERVICE_STR                "service_name is not supported for ai_socktype."
#define  NET_BSD_EAI_ERR_SOCKTYPE_STR               "ai_socktype is not supported."
#define  NET_BSD_EAI_ERR_SYSTEM_STR                 "System error."
#define  NET_BSD_EAI_ERR_UNKNOWN_STR                "Unknown EAI error."

#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
#define  NET_BSD_MAX_DNS_ADDR_PER_HOST              NET_BSD_CFG_ADDRINFO_HOST_NBR_MAX
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) BSD 4.x global variables are required only for applications that call BSD 4.x functions.
*
*               See also 'MODULE  Note #1b'
*                      & 'STANDARD BSD 4.x FUNCTION PROTOTYPES  Note #1'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
                                                                /* See Note #1.                                 */
static  CPU_CHAR        NetBSD_IP_to_Str_Array[NET_ASCII_LEN_MAX_ADDR_IPv4];
#endif

static  MEM_DYN_POOL    NetBSD_AddrInfoPool;
static  MEM_DYN_POOL    NetBSD_SockAddrPool;
#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
static  MEM_DYN_POOL    NetBSD_SockAddrCanonNamePool;
#endif


const  NET_DICT  NetBSD_ServicesStrTbl[] = {
    { NET_BSD_SERVICE_FTP_DATA, NET_BSD_SERVICE_FTP_DATA_STR,   (sizeof(NET_BSD_SERVICE_FTP_DATA_STR)   - 1) },
    { NET_BSD_SERVICE_FTP,      NET_BSD_SERVICE_FTP_STR,        (sizeof(NET_BSD_SERVICE_FTP_STR)        - 1) },
    { NET_BSD_SERVICE_TELNET,   NET_BSD_SERVICE_TELNET_STR,     (sizeof(NET_BSD_SERVICE_TELNET_STR)     - 1) },
    { NET_BSD_SERVICE_SMTP,     NET_BSD_SERVICE_SMTP_STR,       (sizeof(NET_BSD_SERVICE_SMTP_STR)       - 1) },
    { NET_BSD_SERVICE_DNS,      NET_BSD_SERVICE_DNS_STR,        (sizeof(NET_BSD_SERVICE_DNS_STR)        - 1) },
    { NET_BSD_SERVICE_BOOTPS,   NET_BSD_SERVICE_BOOTPS_STR,     (sizeof(NET_BSD_SERVICE_BOOTPS_STR)     - 1) },
    { NET_BSD_SERVICE_BOOTPC,   NET_BSD_SERVICE_BOOTPC_STR,     (sizeof(NET_BSD_SERVICE_BOOTPC_STR)     - 1) },
    { NET_BSD_SERVICE_TFTP,     NET_BSD_SERVICE_TFTP_STR,       (sizeof(NET_BSD_SERVICE_TFTP_STR)       - 1) },
    { NET_BSD_SERVICE_HTTP,     NET_BSD_SERVICE_HTTP_STR,       (sizeof(NET_BSD_SERVICE_HTTP_STR)       - 1) },
    { NET_BSD_SERVICE_SNMP,     NET_BSD_SERVICE_SNMP_STR,       (sizeof(NET_BSD_SERVICE_SNMP_STR)       - 1) },
    { NET_BSD_SERVICE_NTP,      NET_BSD_SERVICE_NTP_STR,        (sizeof(NET_BSD_SERVICE_NTP_STR)        - 1) },
    { NET_BSD_SERVICE_HTTPS,    NET_BSD_SERVICE_HTTPS_STR,      (sizeof(NET_BSD_SERVICE_HTTPS_STR)      - 1) },
    { NET_BSD_SERVICE_SMTPS,    NET_BSD_SERVICE_SMTPS_STR,      (sizeof(NET_BSD_SERVICE_SMTPS_STR)      - 1) },
};


const  NET_DICT  NetBSD_AddrInfoErrStrTbl[] = {
    { 0,                        DEF_NULL,                       0                                            },
    { EAI_ADDRFAMILY,           NET_BSD_EAI_ERR_ADDRFAMILY_STR, (sizeof(NET_BSD_EAI_ERR_ADDRFAMILY_STR) - 1) },
    { EAI_AGAIN,                NET_BSD_EAI_ERR_AGAIN_STR,      (sizeof(NET_BSD_EAI_ERR_AGAIN_STR)      - 1) },
    { EAI_BADFLAGS,             NET_BSD_EAI_ERR_BADFLAGS_STR,   (sizeof(NET_BSD_EAI_ERR_BADFLAGS_STR)   - 1) },
    { EAI_FAIL,                 NET_BSD_EAI_ERR_FAIL_STR,       (sizeof(NET_BSD_EAI_ERR_FAIL_STR)       - 1) },
    { EAI_FAMILY,               NET_BSD_EAI_ERR_FAMILY_STR,     (sizeof(NET_BSD_EAI_ERR_FAMILY_STR)     - 1) },
    { EAI_MEMORY,               NET_BSD_EAI_ERR_MEMORY_STR,     (sizeof(NET_BSD_EAI_ERR_MEMORY_STR)     - 1) },
    { EAI_NONAME,               NET_BSD_EAI_ERR_NONAME_STR,     (sizeof(NET_BSD_EAI_ERR_NONAME_STR)     - 1) },
    { EAI_OVERFLOW,             NET_BSD_EAI_ERR_OVERFLOW_STR,   (sizeof(NET_BSD_EAI_ERR_OVERFLOW_STR)   - 1) },
    { EAI_SERVICE,              NET_BSD_EAI_ERR_SERVICE_STR,    (sizeof(NET_BSD_EAI_ERR_SERVICE_STR)    - 1) },
    { EAI_SOCKTYPE,             NET_BSD_EAI_ERR_SOCKTYPE_STR,   (sizeof(NET_BSD_EAI_ERR_SOCKTYPE_STR)   - 1) },
    { EAI_SYSTEM,               NET_BSD_EAI_ERR_SYSTEM_STR,     (sizeof(NET_BSD_EAI_ERR_SYSTEM_STR)     - 1) },
};


typedef  enum   net_bsd_sock_protocol {
    NET_BSD_SOCK_PROTOCOL_UNKNOWN,
    NET_BSD_SOCK_PROTOCOL_UDP,
    NET_BSD_SOCK_PROTOCOL_TCP,
    NET_BSD_SOCK_PROTOCOL_TCP_UDP,
    NET_BSD_SOCK_PROTOCOL_UDP_TCP,
} NET_BSD_SOCK_PROTOCOL;


typedef  struct net_bsd_service_proto {
    NET_PORT_NBR           Port;
    NET_BSD_SOCK_PROTOCOL  Protocol;
} NET_BSD_SERVICE_PROTOCOL;


const  NET_BSD_SERVICE_PROTOCOL  NetBSD_ServicesProtocolTbl[] = {
    { NET_BSD_SERVICE_FTP_DATA, NET_BSD_SOCK_PROTOCOL_TCP     },
    { NET_BSD_SERVICE_FTP,      NET_BSD_SOCK_PROTOCOL_TCP     },
    { NET_BSD_SERVICE_TELNET,   NET_BSD_SOCK_PROTOCOL_TCP     },
    { NET_BSD_SERVICE_SMTP,     NET_BSD_SOCK_PROTOCOL_TCP     },
    { NET_BSD_SERVICE_DNS,      NET_BSD_SOCK_PROTOCOL_UDP_TCP },
    { NET_BSD_SERVICE_BOOTPS,   NET_BSD_SOCK_PROTOCOL_UDP_TCP },
    { NET_BSD_SERVICE_BOOTPC,   NET_BSD_SOCK_PROTOCOL_UDP_TCP },
    { NET_BSD_SERVICE_TFTP,     NET_BSD_SOCK_PROTOCOL_UDP     },
    { NET_BSD_SERVICE_HTTP,     NET_BSD_SOCK_PROTOCOL_TCP_UDP },
    { NET_BSD_SERVICE_SNMP,     NET_BSD_SOCK_PROTOCOL_TCP_UDP },
    { NET_BSD_SERVICE_NTP,      NET_BSD_SOCK_PROTOCOL_TCP_UDP },
    { NET_BSD_SERVICE_HTTPS,    NET_BSD_SOCK_PROTOCOL_TCP_UDP },
    { NET_BSD_SERVICE_SMTPS,    NET_BSD_SOCK_PROTOCOL_TCP     },
};

#ifdef  NET_IPv6_MODULE_EN
const  struct  in6_addr  in6addr_any                  = IN6ADDR_ANY_INIT;
const  struct  in6_addr  in6addr_loopback             = IN6ADDR_LOOPBACK_INIT;
const  struct  in6_addr  in6addr_linklocal_allnodes   = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
const  struct  in6_addr  in6addr_linklocal_allrouters = IN6ADDR_LINKLOCAL_ALLROUTERS_INIT;
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL FUNCTION PROTOTYPTES
*********************************************************************************************************
*********************************************************************************************************
*/

static  struct  addrinfo    *NetBSD_AddrInfoGet    (struct  addrinfo             **pp_head,
                                                    struct  addrinfo             **pp_tail);

static          void         NetBSD_AddrInfoFree   (struct  addrinfo              *p_addrinfo);

static          CPU_BOOLEAN  NetBSD_AddrInfoSet    (struct  addrinfo              *p_addrinfo,
                                                            NET_SOCK_ADDR_FAMILY   family,
                                                            NET_PORT_NBR           port_nbr,
                                                            CPU_INT08U            *p_addr,
                                                            NET_IP_ADDR_LEN        addr_len,
                                                            NET_SOCK_PROTOCOL      protocol,
                                                            CPU_CHAR              *p_canonname);

static          void         NetBSD_AddrCfgValidate(        CPU_BOOLEAN           *p_ipv4_cfgd,
                                                            CPU_BOOLEAN           *p_ipv6_cfgd);


/*
*********************************************************************************************************
*                                     STANDARD BSD 4.x FUNCTIONS
*
* Note(s) : (1) BSD 4.x function definitions are required only for applications that call BSD 4.x functions.
*
*               See 'net_bsd.h  MODULE  Note #1b3'
*                 & 'net_bsd.h  STANDARD BSD 4.x FUNCTION PROTOTYPES  Note #1'.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               NetBSD_Init()
*
* Description : Initialize BSD module.
*
* Argument(s) : p_err   Pointer to variable that will hold the return error code from this function.
*
* Return(s)   : None.
*
* Caller(s)   : Net_Init().
*********************************************************************************************************
*/

void  NetBSD_Init (NET_ERR  *p_err)
{
    LIB_ERR     lib_err;
#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
    CPU_INT08U  max_addr_per_host;
#endif


   *p_err = NET_ERR_NONE;
    Mem_DynPoolCreate("BSD AddrInfo Pool",
                      &NetBSD_AddrInfoPool,
                       DEF_NULL,
                       sizeof(struct addrinfo),
                       sizeof(CPU_SIZE_T),
                       0,
                       LIB_MEM_BLK_QTY_UNLIMITED,
                      &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }


    Mem_DynPoolCreate("BSD SockAddr Pool",
                      &NetBSD_SockAddrPool,
                       DEF_NULL,
                       sizeof(struct sockaddr),
                       sizeof(CPU_SIZE_T),
                       0,
                       LIB_MEM_BLK_QTY_UNLIMITED,
                      &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }

#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)                  /* Determine the max nmbr of hosts the table will hold. */
#ifdef  NET_IPv6_MODULE_EN
    max_addr_per_host = (DNSc_Cfg.AddrIPv4MaxPerHost +
                         DNSc_Cfg.AddrIPv6MaxPerHost);
#else
    max_addr_per_host = DNSc_Cfg.AddrIPv4MaxPerHost;
#endif
                                                                /* Allot memory region for Canonical names.             */
    Mem_DynPoolCreate("BSD Canonical Name Pool",
                      &NetBSD_SockAddrCanonNamePool,
                       DEF_NULL,
                       DNSc_DFLT_HOST_NAME_LEN,
                       sizeof(CPU_ALIGN),
                       0,
                       max_addr_per_host,
                      &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }
#endif

    return;
}


/*
*********************************************************************************************************
*                                              socket()
*
* Description : Create a socket.
*
* Argument(s) : protocol_family     Socket protocol family :
*
*                                       PF_INET                 Internet Protocol version 4 (IPv4).
*
*                                   See also 'net_sock.c  Note #1a'.
*
*               sock_type           Socket type :
*
*                                       SOCK_DGRAM              Datagram-type socket.
*                                       SOCK_STREAM             Stream  -type socket.
*
*                                   See also 'net_sock.c  Note #1b'.
*
*               protocol            Socket protocol :
*
*                                       0                       Default protocol for socket type.
*                                       IPPROTO_UDP             User Datagram        Protocol (UDP).
*                                       IPPROTO_TCP             Transmission Control Protocol (TCP).
*
*                                   See also 'net_sock.c  Note #1c'.
*
* Return(s)   : Socket descriptor/handle identifier, if NO error(s).
*
*               -1,                                  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

int  socket (int  protocol_family,
             int  sock_type,
             int  protocol)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_Open((NET_SOCK_PROTOCOL_FAMILY)protocol_family,
                                 (NET_SOCK_TYPE           )sock_type,
                                 (NET_SOCK_PROTOCOL       )protocol,
                                                          &err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               close()
*
* Description : Close a socket.
*
* Argument(s) : sock_id     Socket descriptor/handle identifier of socket to close.
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Once an application closes its socket, NO further operations on the socket are allowed
*                   & the application MUST NOT continue to access the socket.
*********************************************************************************************************
*/

int  close (int  sock_id)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_Close(sock_id,
                                 &err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               bind()
*
* Description : Bind a socket to a local address.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to bind to a local address.
*
*               p_addr_local    Pointer to socket address structure             (see Notes #1b1B, #1b2, & #2).
*
*               addr_len        Length  of socket address structure (in octets) [see Note  #1b1C].
*
* Return(s)   :  0, if NO error(s) [see Note #1c1].
*
*               -1, otherwise      (see Note #1c2A).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) IEEE Std 1003.1, 2004 Edition, Section 'bind() : DESCRIPTION' states that "the bind()
*                           function shall assign a local socket address ... to a socket".
*
*                       (2) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition, 6th Printing,
*                           Section 4.4, Page 102 states that "bind() lets us specify the ... address, the port,
*                           both, or neither".
*
*                   (b) (1) IEEE Std 1003.1, 2004 Edition, Section 'select() : DESCRIPTION' states that "the bind()
*                           function takes the following arguments" :
*
*                           (A) 'socket' - "Specifies the file descriptor of the socket to be bound."
*
*                           (B) 'address' - "Points to a 'sockaddr' structure containing the address to be bound
*                                to the socket.  The length and format of the address depend on the address family
*                                of the socket."
*
*                           (C) 'address_len' - "Specifies the length of the 'sockaddr' structure pointed to by
*                                the address argument."
*
*                       (2) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition, 6th Printing,
*                           Section 4.4, Page 102 states that "if ... bind() is called" with :
*
*                           (A) "A port number of 0, the kernel chooses an ephemeral port."
*
*                               (1) "bind() does not return the chosen value ... [of] an ephemeral port ... Call
*                                    getsockname() to return the protocol address ... to obtain the value of the
*                                    ephemeral port assigned by the kernel."
*
*                           (B) "A wildcard ... address, the kernel does not choose the local ... address until
*                                either the socket is connected (TCP) or a datagram is sent on the socket (UDP)."
*
*                               (1) "With IPv4, the wildcard address is specified by the constant INADDR_ANY,
*                                    whose value is normally 0."
*
*                   (c) IEEE Std 1003.1, 2004 Edition, Section 'bind() : RETURN VALUE' states that :
*
*                       (1) "Upon successful completion, bind() shall return 0;" ...
*
*                       (2) (A) "Otherwise, -1 shall be returned," ...
*                           (B) "and 'errno' shall be set to indicate the error."
*                               'errno' NOT currently supported (see 'net_bsd.h  Note #1b').
*
*                   (d) (1) IEEE Std 1003.1, 2004 Edition, Section 'bind() : ERRORS' states that "the bind()
*                           function shall fail if" :
*
*                           (A) "[EBADF] - The 'socket' argument is not a valid file descriptor."
*
*                           (B) "[EAFNOSUPPORT]  - The specified address is not a valid address for the address
*                                family of the specified socket."
*
*                           (C) "[EADDRNOTAVAIL] - The specified address is not available from the local machine."
*
*                           (D) "[EADDRINUSE]    - The specified address is already in use."
*
*                           (E) "[EINVAL]" -
*
*                               (1) (a) "The socket is already bound to an address,"                  ...
*                                   (b) "and the protocol does not support binding to a new address;" ...
*
*                               (2) "or the socket has been shut down."
*
*                       (2) IEEE Std 1003.1, 2004 Edition, Section 'bind() : ERRORS' states that "the bind()
*                           function may fail if" :
*
*                           (A) "[EINVAL]  - The 'address_len' argument is not a valid length for the address
*                                family."
*
*                           (B) "[EISCONN] - The socket is already connected."
*
*                           (C) "[ENOBUFS] - Insufficient resources were available to complete the call."
*
*                   See also 'net_sock.c  NetSock_BindHandler()  Note #2'.
*
*               (2) (a) Socket address structure 'sa_family' member MUST be configured in host-order &
*                       MUST NOT be converted to/from network-order.
*
*                   (b) Socket address structure addresses MUST be configured/converted from host-order
*                       to network-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'
*                          & 'net_sock.c  NetSock_BindHandler()  Note #3'.
*********************************************************************************************************
*/

int  bind (        int         sock_id,
           struct  sockaddr   *p_addr_local,
                   socklen_t   addr_len)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_Bind(                 sock_id,
                                 (NET_SOCK_ADDR *)p_addr_local,
                                                  addr_len,
                                                 &err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                              connect()
*
* Description : Connect a socket to a remote server.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to connect.
*
*               p_addr_remote   Pointer to socket address structure (see Note #1).
*
*               addr_len        Length  of socket address structure (in octets).
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) Socket address structure 'sa_family' member MUST be configured in host-order &
*                       MUST NOT be converted to/from network-order.
*
*                   (b) Socket address structure addresses MUST be configured/converted from host-order
*                       to network-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*********************************************************************************************************
*/

int  connect (        int         sock_id,
              struct  sockaddr   *p_addr_remote,
                      socklen_t   addr_len)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_Conn(                 sock_id,
                                 (NET_SOCK_ADDR *)p_addr_remote,
                                                  addr_len,
                                                 &err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                              listen()
*
* Description : Set socket to listen for connection requests.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to listen.
*
*               sock_q_size     Number of connection requests to queue on listen socket.
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
int  listen (int  sock_id,
             int  sock_q_size)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_Listen(sock_id,
                                   sock_q_size,
                                  &err);

    return (rtn_code);
}
#endif


/*
*********************************************************************************************************
*                                              accept()
*
* Description : Get a new socket accepted from a socket set to listen for connection requests.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of listen socket.
*
*               p_addr_remote   Pointer to an address buffer that will receive the socket address structure
*                                   of the accepted socket's remote address (see Note #1), if NO error(s).
*
*               p_addr_len      Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address buffer pointed to by 'p_addr_remote'.
*                                   (b) (1) Return the actual size of socket address structure with the
*                                               accepted socket's remote address, if NO error(s);
*                                       (2) Return 0,                             otherwise.
*
* Return(s)   : Socket descriptor/handle identifier of new accepted socket, if NO error(s).
*
*               -1,                                                         otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) Socket address structure 'sa_family' member returned in host-order & SHOULD NOT
*                       be converted to network-order.
*
*                   (b) Socket address structure addresses returned in network-order & SHOULD be converted
*                       from network-order to host-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*********************************************************************************************************
*/

#ifdef  NET_SOCK_TYPE_STREAM_MODULE_EN
int  accept (        int         sock_id,
             struct  sockaddr   *p_addr_remote,
                     socklen_t  *p_addr_len)
{
    int                rtn_code;
    NET_SOCK_ADDR_LEN  addr_len;
    NET_ERR            err;


    addr_len   = (NET_SOCK_ADDR_LEN)*p_addr_len;
    rtn_code   = (int)NetSock_Accept((NET_SOCK_ID        ) sock_id,
                                     (NET_SOCK_ADDR     *) p_addr_remote,
                                     (NET_SOCK_ADDR_LEN *)&addr_len,
                                     (NET_ERR           *)&err);

   *p_addr_len = (socklen_t)addr_len;

    return (rtn_code);
}
#endif


/*
*********************************************************************************************************
*                                             recvfrom()
*
* Description : Receive data from a socket.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to receive data.
*
*               p_data_buf      Pointer to an application data buffer that will receive the socket's received
*                                   data.
*
*               data_buf_len    Size of the   application data buffer (in octets) [see Note #1].
*
*               flags           Flags to select receive options (see Note #2); bit-field flags logically OR'd :
*
*                                   0                           No socket flags selected.
*                                   MSG_PEEK                    Receive socket data without consuming
*                                                                   the socket data.
*                                   MSG_DONTWAIT                Receive socket data without blocking.
*
*               p_addr_remote   Pointer to an address buffer that will receive the socket address structure
*                                   with the received data's remote address (see Note #3), if NO error(s).
*
*               p_addr_len      Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address buffer pointed to by 'p_addr_remote'.
*                                   (b) (1) Return the actual size of socket address structure with the
*                                               received data's remote address, if NO error(s);
*                                       (2) Return 0,                           otherwise.
*
* Return(s)   : Number of positive data octets received, if NO error(s)              [see Note #4a].
*
*                0,                                      if socket connection closed (see Note #4b).
*
*               -1,                                      otherwise                   (see Note #4c1).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) (A) Datagram-type sockets send & receive all data atomically -- i.e. every single,
*                               complete datagram transmitted MUST be received as a single, complete datagram.
*
*                           (B) IEEE Std 1003.1, 2004 Edition, Section 'recvfrom() : DESCRIPTION' summarizes
*                               that "for message-based sockets, such as ... SOCK_DGRAM ... the entire message
*                               shall be read in a single operation.  If a message is too long to fit in the
*                               supplied buffer, and MSG_PEEK is not set in the flags argument, the excess
*                               bytes shall be discarded".
*
*                       (2) Thus if the socket's type is datagram & the receive data buffer size is NOT
*                           large enough for the received data, the receive data buffer is maximally filled
*                           with receive data but the remaining data octets are silently discarded & NO
*                           error is returned.
*
*                           (A) IEEE Std 1003.1, 2004 Edition, Section 'send() : ERRORS' states to return an
*                               'EMSGSIZE' error when "the message is too large to be sent all at once".
*
*                               Similarly, a socket receive whose receive data buffer size is NOT large
*                               enough for the received data could return an 'EMSGSIZE' error.
*
*                   (b) (1) (A) (1) Stream-type sockets send & receive all data octets in one or more non-
*                                   distinct packets.  In other words, the application data is NOT bounded
*                                   by any specific packet(s); rather, it is contiguous & sequenced from
*                                   one packet to the next.
*
*                               (2) IEEE Std 1003.1, 2004 Edition, Section 'recv() : DESCRIPTION' summarizes
*                                   that "for stream-based sockets, such as SOCK_STREAM, message boundaries
*                                   shall be ignored.  In this case, data shall be returned to the user as
*                                   soon as it becomes available, and no data shall be discarded".
*
*                           (B) Thus if the socket's type is stream & the receive data buffer size is NOT
*                               large enough for the received data, the receive data buffer is maximally
*                               filled with receive data & the remaining data octets remain queued for
*                               later application-socket receives.
*
*                       (2) Thus it is typical -- but NOT absolutely required -- that a single application
*                           task ONLY receive or request to receive data from a stream-type socket.
*
*                   See also 'net_sock.c  NetSock_RxDataHandler()  Note #2'.
*
*               (2) Only some socket receive flag options are implemented.  If other flag options are requested,
*                   socket receive handler function(s) abort & return appropriate error codes so that requested
*                   flag options are NOT silently ignored.
*
*               (3) (a) Socket address structure 'sa_family' member returned in host-order & SHOULD NOT
*                       be converted to network-order.
*
*                   (b) Socket address structure addresses returned in network-order & SHOULD be converted
*                       from network-order to host-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*
*               (4) IEEE Std 1003.1, 2004 Edition, Section 'recvfrom() : RETURN VALUE' states that :
*
*                   (a) "Upon successful completion, recvfrom() shall return the length of the message in
*                        bytes."
*
*                   (b) "If no messages are available to be received and the peer has performed an orderly
*                        shutdown, recvfrom() shall return 0."
*
*                   (c) (1) "Otherwise, [-1 shall be returned]" ...
*                       (2) "and 'errno' set to indicate the error."
*                           'errno' NOT currently supported (see 'net_bsd.c  Note #1b').
*
*                   See also 'net_sock.c  NetSock_RxDataHandler()  Note #7'.
*********************************************************************************************************
*/

ssize_t  recvfrom (        int         sock_id,
                           void       *p_data_buf,
                          _size_t      data_buf_len,
                           int         flags,
                   struct  sockaddr   *p_addr_remote,
                           socklen_t  *p_addr_len)
{
    ssize_t            rtn_code;
    NET_SOCK_ADDR_LEN  addr_len;
    NET_ERR            err;


    if (data_buf_len > DEF_INT_16U_MAX_VAL) {
        return (NET_BSD_ERR_DFLT);
    }

    addr_len   = (NET_SOCK_ADDR_LEN)*p_addr_len;
    rtn_code   = (ssize_t)NetSock_RxDataFrom((NET_SOCK_ID        ) sock_id,
                                             (void              *) p_data_buf,
                                             (CPU_INT16U         ) data_buf_len,
                                             (NET_SOCK_API_FLAGS ) flags,
                                             (NET_SOCK_ADDR     *) p_addr_remote,
                                             (NET_SOCK_ADDR_LEN *)&addr_len,
                                             (void              *) 0,
                                             (CPU_INT08U         ) 0u,
                                             (CPU_INT08U        *) 0,
                                             (NET_ERR           *)&err);

   *p_addr_len = (socklen_t)addr_len;

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               recv()
*
* Description : Receive data from a socket.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to receive data.
*
*               p_data_buf      Pointer to an application data buffer that will receive the socket's received
*                                   data.
*
*               data_buf_len    Size of the   application data buffer (in octets) [see Note #1].
*
*               flags           Flags to select receive options (see Note #2); bit-field flags logically OR'd :
*
*                                   0                           No socket flags selected.
*                                   MSG_PEEK                    Receive socket data without consuming
*                                                                   the socket data.
*                                   MSG_DONTWAIT                Receive socket data without blocking.
*
* Return(s)   : Number of positive data octets received, if NO error(s)              [see Note #3a].
*
*                0,                                      if socket connection closed (see Note #3b).
*
*               -1,                                      otherwise                   (see Note #3c1).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) (A) Datagram-type sockets send & receive all data atomically -- i.e. every single,
*                               complete datagram transmitted MUST be received as a single, complete datagram.
*
*                           (B) IEEE Std 1003.1, 2004 Edition, Section 'recv() : DESCRIPTION' summarizes that
*                               "for message-based sockets, such as SOCK_DGRAM ... the entire message shall be
*                               read in a single operation.  If a message is too long to fit in the supplied
*                               buffer, and MSG_PEEK is not set in the flags argument, the excess bytes shall
*                               be discarded".
*
*                       (2) Thus if the socket's type is datagram & the receive data buffer size is NOT
*                           large enough for the received data, the receive data buffer is maximally filled
*                           with receive data but the remaining data octets are silently discarded & NO
*                           error is returned.
*
*                           (A) IEEE Std 1003.1, 2004 Edition, Section 'send() : ERRORS' states to return an
*                               'EMSGSIZE' error when "the message is too large to be sent all at once".
*
*                               Similarly, a socket receive whose receive data buffer size is NOT large
*                               enough for the received data could return an 'EMSGSIZE' error.
*
*                   (b) (1) (A) (1) Stream-type sockets send & receive all data octets in one or more non-
*                                   distinct packets.  In other words, the application data is NOT bounded
*                                   by any specific packet(s); rather, it is contiguous & sequenced from
*                                   one packet to the next.
*
*                               (2) IEEE Std 1003.1, 2004 Edition, Section 'recv() : DESCRIPTION' summarizes
*                                   that "for stream-based sockets, such as SOCK_STREAM, message boundaries
*                                   shall be ignored.  In this case, data shall be returned to the user as
*                                   soon as it becomes available, and no data shall be discarded".
*
*                           (B) Thus if the socket's type is stream & the receive data buffer size is NOT
*                               large enough for the received data, the receive data buffer is maximally
*                               filled with receive data & the remaining data octets remain queued for
*                               later application-socket receives.
*
*                       (2) Thus it is typical -- but NOT absolutely required -- that a single application
*                           task ONLY receive or request to receive data from a stream-type socket.
*
*                   See also 'net_sock.c  NetSock_RxDataHandler()  Note #2'.
*
*               (2) Only some socket receive flag options are implemented.  If other flag options are requested,
*                   socket receive handler function(s) abort & return appropriate error codes so that requested
*                   flag options are NOT silently ignored.
*
*               (3) IEEE Std 1003.1, 2004 Edition, Section 'recv() : RETURN VALUE' states that :
*
*                   (a) "Upon successful completion, recv() shall return the length of the message in bytes."
*
*                   (b) "If no messages are available to be received and the peer has performed an orderly
*                        shutdown, recv() shall return 0."
*
*                   (c) (1) "Otherwise, -1 shall be returned" ...
*                       (2) "and 'errno' set to indicate the error."
*                           'errno' NOT currently supported (see 'net_bsd.c  Note #1b').
*
*                   See also 'net_sock.c  NetSock_RxDataHandler()  Note #7'.
*********************************************************************************************************
*/

ssize_t  recv (int      sock_id,
               void    *p_data_buf,
              _size_t   data_buf_len,
               int      flags)
{
    ssize_t  rtn_code;
    NET_ERR  err;


    if (data_buf_len > DEF_INT_16U_MAX_VAL) {
        return (NET_BSD_ERR_DFLT);
    }

    rtn_code = (ssize_t)NetSock_RxData((NET_SOCK_ID       ) sock_id,
                                       (void             *) p_data_buf,
                                       (CPU_INT16U        ) data_buf_len,
                                       (NET_SOCK_API_FLAGS) flags,
                                       (NET_ERR          *)&err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                              sendto()
*
* Description : Send data through a socket.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to send data.
*
*               p_data          Pointer to application data to send.
*
*               data_len        Length of  application data to send (in octets) [see Note #1].
*
*               flags           Flags to select send options (see Note #2); bit-field flags logically OR'd :
*
*                                   0                           No socket flags selected.
*                                   MSG_DONTWAIT                Send socket data without blocking.
*
*               p_addr_remote   Pointer to destination address buffer (see Note #3);
*                                   required for datagram sockets, optional for stream sockets.
*
*               addr_len        Length of  destination address buffer (in octets).
*
* Return(s)   : Number of positive data octets sent, if NO error(s)              [see Note #4a1].
*
*                0,                                  if socket connection closed (see Note #4b).
*
*               -1,                                  otherwise                   (see Note #4a2A).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) (A) Datagram-type sockets send & receive all data atomically -- i.e. every single,
*                               complete datagram  sent      MUST be received    as a single, complete datagram.
*                               Thus each call to send data MUST be transmitted in a single, complete datagram.
*
*                           (B) (1) IEEE Std 1003.1, 2004 Edition, Section 'send() : DESCRIPTION' states that
*                                   "if the message is too long to pass through the underlying protocol, send()
*                                   shall fail and no data shall be transmitted".
*
*                               (2) Since IP transmit fragmentation is NOT currently supported (see 'net_ip.h
*                                   Note #1d'), if the socket's type is datagram & the requested send data
*                                   length is greater than the socket/transport layer MTU, then NO data is
*                                   sent & NET_SOCK_ERR_INVALID_DATA_SIZE error is returned.
*
*                       (2) (A) (1) Stream-type sockets send & receive all data octets in one or more non-
*                                   distinct packets.  In other words, the application data is NOT bounded
*                                   by any specific packet(s); rather, it is contiguous & sequenced from
*                                   one packet to the next.
*
*                               (2) Thus if the socket's type is stream & the socket's send data queue(s) are
*                                   NOT large enough for the send data, the send data queue(s) are maximally
*                                   filled with send data & the remaining data octets are discarded but may be
*                                   re-sent by later application-socket sends.
*
*                               (3) Therefore, NO stream-type socket send data length should be "too long to
*                                   pass through the underlying protocol" & cause the socket send to "fail ...
*                                   [with] no data ... transmitted" (see Note #1a1B1).
*
*                           (B) Thus it is typical -- but NOT absolutely required -- that a single application
*                               task ONLY send or request to send data to a stream-type socket.
*
*                   (b) 'data_len' of 0 octets NOT allowed.
*
*                   See also 'net_sock.c  NetSock_TxDataHandler()  Note #2'.
*
*               (2) Only some socket send flag options are implemented.  If other flag options are requested,
*                   socket send handler function(s) abort & return appropriate error codes so that requested
*                   flag options are NOT silently ignored.
*
*               (3) (a) Socket address structure 'sa_family' member MUST be configured in host-order &
*                       MUST NOT be converted to/from network-order.
*
*                   (b) Socket address structure addresses MUST be configured/converted from host-order
*                       to network-order.
*
*                   See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*
*               (4) (a) IEEE Std 1003.1, 2004 Edition, Section 'sendto() : RETURN VALUE' states that :
*
*                       (1) "Upon successful completion, sendto() shall return the number of bytes sent."
*
*                           (A) Section 'sendto() : DESCRIPTION' elaborates that "successful completion
*                               of a call to sendto() does not guarantee delivery of the message".
*
*                           (B) (1) Thus applications SHOULD verify the actual returned number of data
*                                   octets transmitted &/or prepared for transmission.
*
*                               (2) In addition, applications MAY desire verification of receipt &/or
*                                   acknowledgement of transmitted data to the remote host -- either
*                                   inherently by the transport layer or explicitly by the application.
*
*                       (2) (A) "Otherwise, -1 shall be returned" ...
*                               (1) Section 'sendto() : DESCRIPTION' elaborates that "a return value of
*                                   -1 indicates only locally-detected errors".
*
*                           (B) "and 'errno' set to indicate the error."
*                               'errno' NOT currently supported (see 'net_bsd.c  Note #1b').
*
*                   (b) Although NO socket send() specification states to return '0' when the socket's
*                       connection is closed, it seems reasonable to return '0' since it is possible for the
*                       socket connection to be close()'d or shutdown() by the remote host.
*
*                   See also 'net_sock.c  NetSock_TxDataHandler()  Note #5'.
*********************************************************************************************************
*/

ssize_t  sendto (        int         sock_id,
                         void       *p_data,
                        _size_t      data_len,
                         int         flags,
                 struct  sockaddr   *p_addr_remote,
                         socklen_t   addr_len)
{
    ssize_t  rtn_code;
    NET_ERR  err;


    rtn_code = (ssize_t)NetSock_TxDataTo((NET_SOCK_ID       ) sock_id,
                                         (void             *) p_data,
                                         (CPU_INT16U        ) data_len,
                                         (NET_SOCK_API_FLAGS) flags,
                                         (NET_SOCK_ADDR    *) p_addr_remote,
                                         (NET_SOCK_ADDR_LEN ) addr_len,
                                         (NET_ERR          *)&err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               send()
*
* Description : Send data through a socket.
*
* Argument(s) : sock_id         Socket descriptor/handle identifier of socket to send data.
*
*               p_data          Pointer to application data to send.
*
*               data_len        Length of  application data to send (in octets) [see Note #1].
*
*               flags           Flags to select send options (see Note #2); bit-field flags logically OR'd :
*
*                                   0                           No socket flags selected.
*                                   MSG_DONTWAIT                Send socket data without blocking.
*
* Return(s)   : Number of positive data octets sent, if NO error(s)              [see Note #3a1].
*
*                0,                                  if socket connection closed (see Note #3b).
*
*               -1,                                  otherwise                   (see Note #3a2A).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) (A) Datagram-type sockets send & receive all data atomically -- i.e. every single,
*                               complete datagram  sent      MUST be received    as a single, complete datagram.
*                               Thus each call to send data MUST be transmitted in a single, complete datagram.
*
*                           (B) (1) IEEE Std 1003.1, 2004 Edition, Section 'send() : DESCRIPTION' states that
*                                   "if the message is too long to pass through the underlying protocol, send()
*                                   shall fail and no data shall be transmitted".
*
*                               (2) Since IP transmit fragmentation is NOT currently supported (see 'net_ip.h
*                                   Note #1d'), if the socket's type is datagram & the requested send data
*                                   length is greater than the socket/transport layer MTU, then NO data is
*                                   sent & NET_SOCK_ERR_INVALID_DATA_SIZE error is returned.
*
*                       (2) (A) (1) Stream-type sockets send & receive all data octets in one or more non-
*                                   distinct packets.  In other words, the application data is NOT bounded
*                                   by any specific packet(s); rather, it is contiguous & sequenced from
*                                   one packet to the next.
*
*                               (2) Thus if the socket's type is stream & the socket's send data queue(s) are
*                                   NOT large enough for the send data, the send data queue(s) are maximally
*                                   filled with send data & the remaining data octets are discarded but may be
*                                   re-sent by later application-socket sends.
*
*                               (3) Therefore, NO stream-type socket send data length should be "too long to
*                                   pass through the underlying protocol" & cause the socket send to "fail ...
*                                   [with] no data ... transmitted" (see Note #1a1B1).
*
*                           (B) Thus it is typical -- but NOT absolutely required -- that a single application
*                               task ONLY send or request to send data to a stream-type socket.
*
*                   (b) 'data_len' of 0 octets NOT allowed.
*
*                   See also 'net_sock.c  NetSock_TxDataHandler()  Note #2'.
*
*               (2) Only some socket send flag options are implemented.  If other flag options are requested,
*                   socket send handler function(s) abort & return appropriate error codes so that requested
*                   flag options are NOT silently ignored.
*
*               (3) (a) IEEE Std 1003.1, 2004 Edition, Section 'send() : RETURN VALUE' states that :
*
*                       (1) "Upon successful completion, send() shall return the number of bytes sent."
*
*                           (A) Section 'send() : DESCRIPTION' elaborates that "successful completion
*                               of a call to sendto() does not guarantee delivery of the message".
*
*                           (B) (1) Thus applications SHOULD verify the actual returned number of data
*                                   octets transmitted &/or prepared for transmission.
*
*                               (2) In addition, applications MAY desire verification of receipt &/or
*                                   acknowledgement of transmitted data to the remote host -- either
*                                   inherently by the transport layer or explicitly by the application.
*
*                       (2) (A) "Otherwise, -1 shall be returned" ...
*                               (1) Section 'send() : DESCRIPTION' elaborates that "a return value of
*                                   -1 indicates only locally-detected errors".
*
*                           (B) "and 'errno' set to indicate the error."
*                               'errno' NOT currently supported (see 'net_bsd.c  Note #1b').
*
*                   (b) Although NO socket send() specification states to return '0' when the socket's
*                       connection is closed, it seems reasonable to return '0' since it is possible for the
*                       socket connection to be close()'d or shutdown() by the remote host.
*
*                   See also 'net_sock.c  NetSock_TxDataHandler()  Note #5'.
*********************************************************************************************************
*/

ssize_t  send (int      sock_id,
               void    *p_data,
              _size_t   data_len,
               int      flags)
{
    ssize_t  rtn_code;
    NET_ERR  err;


    rtn_code = (ssize_t)NetSock_TxData((NET_SOCK_ID       ) sock_id,
                                       (void             *) p_data,
                                       (CPU_INT16U        ) data_len,
                                       (NET_SOCK_API_FLAGS) flags,
                                       (NET_ERR          *)&err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                              select()
*
* Description : Check multiple file descriptors for available resources &/or operations.
*
* Argument(s) : desc_nbr_max    Maximum number of file descriptors in the file descriptor sets
*                                   (see Note #1b1).
*
*               p_desc_rd       Pointer to a set of file descriptors to :
*
*                                   (a) Check for available read  operation(s) [see Note #1b2A1].
*
*                                   (b) (1) Return the actual file descriptors ready for available
*                                               read  operation(s), if NO error(s) [see Note #1b2B1a1];
*                                       (2) Return the initial, non-modified set of file descriptors,
*                                               on any error(s) [see Note #1c2A];
*                                       (3) Return a null-valued (i.e. zero-cleared) descriptor set,
*                                               if any timeout expires (see Note #1c2B).
*
*               p_desc_wr       Pointer to a set of file descriptors to :
*
*                                   (a) Check for available write operation(s) [see Note #1b2A2].
*
*                                   (b) (1) Return the actual file descriptors ready for available
*                                               write operation(s), if NO error(s) [see Note #1b2B1a1];
*                                       (2) Return the initial, non-modified set of file descriptors,
*                                               on any error(s) [see Note #1c2A];
*                                       (3) Return a null-valued (i.e. zero-cleared) descriptor set,
*                                               if any timeout expires (see Note #1c2B).
*
*               p_desc_err      Pointer to a set of file descriptors to :
*
*                                   (a) Check for any error(s) &/or exception(s) [see Note #1b2A3].
*
*                                   (b) (1) Return the actual file descriptors flagged with any error(s)
*                                               &/or exception(s), if NO non-descriptor-related error(s)
*                                               [see Note #1b2B1a1];
*                                       (2) Return the initial, non-modified set of file descriptors,
*                                               on any error(s) [see Note #1c2A];
*                                       (3) Return a null-valued (i.e. zero-cleared) descriptor set,
*                                               if any timeout expires (see Note #1c2B).
*
*               p_timeout        Pointer to a timeout (see Note #1b3).
*
* Return(s)   : Number of file descriptors with available resources &/or operations, if any     (see Note #1c1A1).
*
*                0,                                                                  on timeout (see Note #1c1B).
*
*               -1,                                                                  otherwise  (see Note #1c1A2a).
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) IEEE Std 1003.1, 2004 Edition, Section 'select() : DESCRIPTION' states that :
*
*                       (1) (A) "select() ... shall support" the following file descriptor types :
*
*                               (1) "regular files,"                        ...
*                               (2) "terminal and pseudo-terminal devices," ...
*                               (3) "STREAMS-based files,"                  ...
*                               (4) "FIFOs,"                                ...
*                               (5) "pipes,"                                ...
*                               (6) "sockets."
*
*                           (B) "The behavior of ... select() on ... other types of ... file descriptors
*                                ... is unspecified."
*
*                       (2) Network Socket Layer supports BSD 4.x select() functionality with the following
*                           restrictions/constraints :
*
*                           (A) ONLY supports the following file descriptor types :
*                               (1) Sockets
*
*                   (b) IEEE Std 1003.1, 2004 Edition, Section 'select() : DESCRIPTION' states that :
*
*                       (1) (A) "The 'nfds' argument ('desc_nbr_max') specifies the range of descriptors to
*                                be tested.  The first 'nfds' descriptors shall be checked in each set; that
*                                is, the descriptors from zero through nfds-1 in the descriptor sets shall
*                                be examined."
*
*                           (B) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                               6th Printing, Section 6.3, Page 163 states that "the ['nfds'] argument"
*                               specifies :
*
*                               (1) "the number of descriptors," ...
*                               (2) "not the largest value."
*
*                       (2) "The select() function shall ... examine the file descriptor sets whose addresses
*                            are passed in the 'readfds' ('p_desc_rd'), 'writefds' ('p_desc_wr'), and 'errorfds'
*                            ('p_desc_err') parameters to see whether some of their descriptors are ready for
*                            reading, are ready for writing, or have an exceptional condition pending,
*                            respectively."
*
*                           (A) (1) (a) "If the 'readfds' argument ('p_desc_rd') is not a null pointer, it
*                                        points to an object of type 'fd_set' that on input specifies the
*                                        file descriptors to be checked for being ready to read, and on
*                                        output indicates which file descriptors are ready to read."
*
*                                   (b) "A descriptor shall be considered ready for reading when a call to
*                                        an input function ... would not block, whether or not the function
*                                        would transfer data successfully.  (The function might return data,
*                                        an end-of-file indication, or an error other than one indicating
*                                        that it is blocked, and in each of these cases the descriptor shall
*                                        be considered ready for reading.)" :
*
*                                       (1) "If the socket is currently listening, then it shall be marked
*                                            as readable if an incoming connection request has been received,
*                                            and a call to the accept() function shall complete without
*                                            blocking."
*
*                                   (c) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                                       6th Printing, Section 6.3, Pages 164-165 states that "a socket is ready
*                                       for reading if any of the following ... conditions is true" :
*
*                                       (1) "A read operation on the socket will not block and will return a
*                                            value greater than 0 (i.e., the data that is ready to be read)."
*
*                                       (2) "The read half of the connection is closed (i.e., a TCP connection
*                                            that has received a FIN).  A read operation ... will not block and
*                                            will return 0 (i.e., EOF)."
*
*                                       (3) "The socket is a listening socket and the number of completed
*                                            connections is nonzero.  An accept() on the listening socket
*                                            will ... not block."
*
*                                       (4) "A socket error is pending.  A read operation on the socket will
*                                            not block and will return an error (-1) with 'errno' set to the
*                                            specific error condition."
*
*                               (2) (a) "If the 'writefds' argument ('p_desc_wr') is not a null pointer, it
*                                        points to an object of type 'fd_set' that on input specifies the
*                                        file descriptors to be checked for being ready to write, and on
*                                        output indicates which file descriptors are ready to write."
*
*                                   (b) "A descriptor shall be considered ready for writing when a call to
*                                        an output function ... would not block, whether or not the function
*                                        would transfer data successfully" :
*
*                                       (1) "If a non-blocking call to the connect() function has been made
*                                            for a socket, and the connection attempt has either succeeded
*                                            or failed leaving a pending error, the socket shall be marked
*                                            as writable."
*
*                                   (c) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                                       6th Printing, Section 6.3, Page 165 states that "a socket is ready for
*                                       writing if any of the following ... conditions is true" :
*
*                                       (1) "A write operation will not block and will return a positive value
*                                            (e.g., the number of bytes accepted by the transport layer)."
*
*                                       (2) "The write half of the connection is closed."
*
*                                       (3) "A socket using a non-blocking connect() has completed the
*                                            connection, or the connect() has failed."
*
*                                       (4) "A socket error is pending.  A write operation on the socket will
*                                            not block and will return an error (-1) with 'errno' set to the
*                                            specific error condition."
*
*                               (3) (a) "If the 'errorfds' argument ('p_desc_err') is not a null pointer, it
*                                        points to an object of type 'fd_set' that on input specifies the file
*                                        descriptors to be checked for error conditions pending, and on output
*                                        indicates which file descriptors have error conditions pending."
*
*                                   (b) "A file descriptor ... shall be considered to have an exceptional
*                                        condition pending ... as noted below" :
*
*                                       (2) "If a socket has a pending error."
*
*                                       (3) "Other circumstances under which a socket may be considered to
*                                            have an exceptional condition pending are protocol-specific
*                                            and implementation-defined."
*
*                                   (d) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                                       6th Printing, Section 6.3, Page 165 states "that when an error occurs on
*                                       a socket, it is [also] marked as both readable and writeable by select()".
*
*                           (B) (1) (a) "Upon successful completion, ... select() ... shall" :
*
*                                       (1) "modify the objects pointed to by the 'readfds' ('p_desc_rd'),
*                                            'writefds' ('p_desc_wr'), and 'errorfds' ('p_desc_err') arguments
*                                            to indicate which file descriptors are ready for reading, ready
*                                            for writing, or have an error condition pending, respectively," ...
*
*                                       (2) "and shall return the total number of ready descriptors in all
*                                            the output sets."
*
*                                   (b) (1) "For each file descriptor less than nfds ('desc_nbr_max'), the
*                                            corresponding bit shall be set on successful completion" :
*
*                                           (A) "if it was set on input" ...
*                                           (B) "and the associated condition is true for that file descriptor."
*
*                               (2) select() can NOT absolutely guarantee that descriptors returned as ready
*                                   will still be ready during subsequent operations since any higher priority
*                                   tasks or processes may asynchronously consume the descriptors' operations
*                                   &/or resources.  This can occur since select() functionality & subsequent
*                                   operations are NOT atomic operations protected by network, file system,
*                                   or operating system mechanisms.
*
*                                   However, as long as no higher priority tasks or processes access any of
*                                   the same descriptors, then a single task or process can assume that all
*                                   descriptors returned as ready by select() will still be ready during
*                                   subsequent operations.
*
*                       (3) (A) "The 'timeout' parameter ('p_timeout') controls how long ... select() ... shall
*                                take before timing out."
*
*                               (1) (a) "If the 'timeout' parameter ('p_timeout') is not a null pointer, it
*                                        specifies a maximum interval to wait for the selection to complete."
*
*                                       (1) "If none of the selected descriptors are ready for the requested
*                                            operation, ... select() ... shall block until at least one of the
*                                            requested operations becomes ready ... or ... until the timeout
*                                            occurs."
*
*                                       (2) "If the specified time interval expires without any requested
*                                            operation becoming ready, the function shall return."
*
*                                       (3) "To effect a poll, the 'timeout' parameter ('p_timeout') should not be
*                                            a null pointer, and should point to a zero-valued timespec structure
*                                            ('timeval')."
*
*                                   (b) (1) (A) "If the 'readfds' ('p_desc_rd'), 'writefds' ('p_desc_wr'), and
*                                                'errorfds' ('p_desc_err') arguments are"                         ...
*                                               (1) "all null pointers"                                          ...
*                                               (2) [or all null-valued (i.e. no file descriptors set)]          ...
*                                           (B) "and the 'timeout' argument ('p_timeout') is not a null pointer," ...
*
*                                       (2) ... then "select() ... shall block for the time specified".
*
*                               (2) "If the 'timeout' parameter ('p_timeout') is a null pointer, then the call to
*                                    ... select() shall block indefinitely until at least one descriptor meets the
*                                    specified criteria."
*
*                           (B) (1) "For the select() function, the timeout period is given ... in an argument
*                                   ('p_timeout') of type struct 'timeval'" ... :
*
*                                   (a) "in seconds" ...
*                                   (b) "and microseconds."
*
*                               (2) (a) (1) "Implementations may place limitations on the maximum timeout interval
*                                            supported" :
*
*                                           (A) "All implementations shall support a maximum timeout interval of
*                                                at least 31 days."
*
*                                               (1) However, since maximum timeout interval values are dependent
*                                                   on the specific OS implementation; a maximum timeout interval
*                                                   can NOT be guaranteed.
*
*                                           (B) "If the 'timeout' argument ('p_timeout') specifies a timeout interval
*                                                greater than the implementation-defined maximum value, the maximum
*                                                value shall be used as the actual timeout value."
*
*                                       (2) "Implementations may also place limitations on the granularity of
*                                            timeout intervals" :
*
*                                           (A) "If the requested 'timeout' interval requires a finer granularity
*                                                than the implementation supports, the actual timeout interval
*                                                shall be rounded up to the next supported value."
*
*                   (c) (1) (A) IEEE Std 1003.1, 2004 Edition, Section 'select() : RETURN VALUE' states that :
*
*                               (1) "Upon successful completion, ... select() ... shall return the total
*                                    number of bits set in the bit masks."
*
*                               (2) (a) "Otherwise, -1 shall be returned," ...
*                                   (b) "and 'errno' shall be set to indicate the error."
*                                       'errno' NOT currently supported (see 'net_bsd.c  Note #1b').
*
*                           (B) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                               6th Printing, Section 6.3, Page 161 states that BSD select() function
*                               "returns ... 0 on timeout".
*
*                       (2) IEEE Std 1003.1, 2004 Edition, Section 'select() : DESCRIPTION' states that :
*
*                           (A) "On failure, the objects pointed to by the 'readfds' ('p_desc_rd'), 'writefds'
*                                ('p_desc_wr'), and 'errorfds' ('p_desc_err') arguments shall not be modified."
*
*                           (B) "If the 'timeout' interval expires without the specified condition being
*                                true for any of the specified file descriptors, the objects pointed to
*                                by the 'readfds' ('p_desc_rd'), 'writefds' ('p_desc_wr'), and 'errorfds'
*                                ('p_desc_err') arguments shall have all bits set to 0."
*
*                   (d) IEEE Std 1003.1, 2004 Edition, Section 'select() : ERRORS' states that "under the
*                       following conditions, ... select() shall fail and set 'errno' to" :
*
*                       (1) "[EBADF] - One or more of the file descriptor sets specified a file descriptor
*                            that is not a valid open file descriptor."
*
*                       (2) "[EINVAL]" -
*
*                           (A) "The 'nfds' argument ('desc_nbr_max') is" :
*                               (1) "less than 0 or" ...
*                               (2) "greater than FD_SETSIZE."
*
*                           (B) "An invalid timeout interval was specified."
*
*                           'errno' NOT currently supported (see 'net_bsd.c  Note #1b').
*
*                   See also 'net_sock.c  NetSock_Sel()  Note #3'.
*********************************************************************************************************
*/

#if    (NET_SOCK_CFG_SEL_EN == DEF_ENABLED)
int  select (        int       desc_nbr_max,
             struct  fd_set   *p_desc_rd,
             struct  fd_set   *p_desc_wr,
             struct  fd_set   *p_desc_err,
             struct  timeval  *p_timeout)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_Sel((NET_SOCK_QTY      ) desc_nbr_max,
                                (NET_SOCK_DESC    *) p_desc_rd,
                                (NET_SOCK_DESC    *) p_desc_wr,
                                (NET_SOCK_DESC    *) p_desc_err,
                                (NET_SOCK_TIMEOUT *) p_timeout,
                                (NET_ERR          *)&err);

    return (rtn_code);
}
#endif


/*
*********************************************************************************************************
*                                             inet_addr()
*
* Description : Convert an IPv4 address in ASCII dotted-decimal notation to a network protocol IPv4 address
*                   in network-order.
*
* Argument(s) : p_addr      Pointer to an ASCII string that contains a dotted-decimal IPv4 address (see Note #2).
*
* Return(s)   : Network-order IPv4 address represented by ASCII string, if NO error(s).
*
*               -1,                                                     otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) RFC #1983 states that "dotted decimal notation ... refers [to] IP addresses of the
*                   form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                   address".
*
*                   In other words, the dotted-decimal notation separates four decimal octet values by
*                   the dot, or period, character ('.').  Each decimal value represents one octet of
*                   the IP address starting with the most significant octet in network-order.
*
*                       IP Address Examples :
*
*                              DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                   127.0.0.1           =       0x7F000001
*                                   192.168.1.64        =       0xC0A80140
*                                   255.255.255.0       =       0xFFFFFF00
*                                   ---         -                 --    --
*                                    ^          ^                 ^      ^
*                                    |          |                 |      |
*                                   MSO        LSO               MSO    LSO
*
*                           where
*                                   MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                   LSO        Least Significant Octet in Dotted Decimal IP Address
*
*               (2) The dotted-decimal ASCII string MUST :
*
*                   (a) Include ONLY decimal values & the dot, or period, character ('.') ; ALL other
*                       characters trapped as invalid, including any leading or trailing characters.
*
*                   (b) (1) Include EXACTLY four decimal values ...
*                       (2) ... separated ...
*                       (3) ... by EXACTLY three dot characters.
*
*                   (c) Ensure that each decimal value does NOT exceed the maximum octet value (i.e. 255).
*
*                   (d) Ensure that each decimal value does NOT include leading zeros.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
in_addr_t  inet_addr (char  *p_addr)
{
    in_addr_t  addr;
    NET_ERR    err;


    addr = (in_addr_t)NetASCII_Str_to_IPv4((CPU_CHAR *) p_addr,
                                           (NET_ERR  *)&err);
    if (err !=  NET_ASCII_ERR_NONE) {
        addr = (in_addr_t)NET_BSD_ERR_DFLT;
    }
    addr =  NET_UTIL_HOST_TO_NET_32(addr);

    return (addr);
}
#endif


/*
*********************************************************************************************************
*                                             inet_ntoa()
*
* Description : Convert a network protocol IPv4 address into a dotted-decimal notation ASCII string.
*
* Argument(s) : addr        IPv4 address.
*
* Return(s)   : Pointer to ASCII string of converted IPv4 address (see Note #2), if NO error(s).
*
*               Pointer to NULL,                                                 otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) RFC #1983 states that "dotted decimal notation ... refers [to] IP addresses of the
*                   form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                   address".
*
*                   In other words, the dotted-decimal notation separates four decimal octet values by
*                   the dot, or period, character ('.').  Each decimal value represents one octet of
*                   the IP address starting with the most significant octet in network-order.
*
*                       IP Address Examples :
*
*                              DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                   127.0.0.1           =       0x7F000001
*                                   192.168.1.64        =       0xC0A80140
*                                   255.255.255.0       =       0xFFFFFF00
*                                   ---         -                 --    --
*                                    ^          ^                 ^      ^
*                                    |          |                 |      |
*                                   MSO        LSO               MSO    LSO
*
*                           where
*                                   MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                   LSO        Least Significant Octet in Dotted Decimal IP Address
*
*               (2) IEEE Std 1003.1, 2004 Edition, Section 'inet_ntoa() : DESCRIPTION' states that
*                   "inet_ntoa() ... need not be reentrant ... [and] is not required to be thread-safe".
*
*                   Since the character string is returned in a single, global character string array,
*                   this conversion function is NOT re-entrant.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
char  *inet_ntoa (struct  in_addr  addr)
{
    in_addr_t  addr_ip;
    NET_ERR    err;


    addr_ip = addr.s_addr;
    addr_ip = NET_UTIL_NET_TO_HOST_32(addr_ip);

    NetASCII_IPv4_to_Str((NET_IPv4_ADDR) addr_ip,
                         (CPU_CHAR    *)&NetBSD_IP_to_Str_Array[0],
                         (CPU_BOOLEAN  ) DEF_NO,
                         (NET_ERR     *)&err);
    if (err != NET_ASCII_ERR_NONE) {
        return ((char *)0);
    }

    return ((char *)&NetBSD_IP_to_Str_Array[0]);
}
#endif


/*
*********************************************************************************************************
*                                             inet_aton()
*
* Description : Convert an IPv4 address in ASCII dotted-decimal notation to a network protocol IPv4 address
*                   in network-order.
*
* Argument(s) : p_addr_in   Pointer to an ASCII string that contains a dotted-decimal IPv4 address (see Note #2).
*
*               p_addr      Pointer to an IPv4 address.
*
* Return(s)   : 1  if the supplied address is valid,
*
*               0, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) RFC #1983 states that "dotted decimal notation ... refers [to] IP addresses of the
*                   form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                   address".
*
*                   In other words, the dotted-decimal notation separates four decimal octet values by
*                   the dot, or period, character ('.').  Each decimal value represents one octet of
*                   the IP address starting with the most significant octet in network-order.
*
*                       IP Address Examples :
*
*                              DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                   127.0.0.1           =       0x7F000001
*                                   192.168.1.64        =       0xC0A80140
*                                   255.255.255.0       =       0xFFFFFF00
*                                   ---         -                 --    --
*                                    ^          ^                 ^      ^
*                                    |          |                 |      |
*                                   MSO        LSO               MSO    LSO
*
*                           where
*                                   MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                   LSO        Least Significant Octet in Dotted Decimal IP Address
*
*               (2) IEEE Std 1003.1, 2004 Edition - inet_addr, inet_ntoa - IPv4 address manipulation:
*
*                   (a) Values specified using IPv4 dotted decimal notation take one of the following forms:
*
*                       (1) a.b.c.d - When four parts are specified, each shall be interpreted ...
*                                     ... as a byte of data and assigned, from left to right,  ...
*                                     ... to the four bytes of an Internet address.
*
*                       (2) a.b.c   - When a three-part address is specified, the last part shall ...
*                                     ... be interpreted as a 16-bit quantity and placed in the   ...
*                                     ... rightmost two bytes of the network address. This makes  ...
*                                     ... the three-part address format convenient for specifying ...
*                                     ... Class B network addresses as "128.net.host".
*
*                       (3) a.b     - When a two-part address is supplied, the last part shall be  ...
*                                     ... interpreted as a 24-bit quantity and placed in the       ...
*                                     ... rightmost three bytes of the network address. This makes ...
*                                     ... the two-part address format convenient for specifying    ...
*                                     ... Class A network addresses as "net.host".
*
*                       (4) a       - When only one part is given, the value shall be stored ...
*                                     ... directly in the network address without any byte rearrangement.
*
*               (3) The dotted-decimal ASCII string MUST :
*
*                   (a) Include ONLY decimal values & the dot, or period, character ('.') ; ALL other
*                       characters trapped as invalid, including any leading or trailing characters.
*
*                   (b) (1) Include UP TO four decimal values ...
*                       (2) ... separated ...
*                       (3) ... by UP TO three dot characters.
*
*                   (c) Ensure that each decimal value does NOT exceed the maximum value for its form:
*
*                       (1) a.b.c.d - 255.255.255.255
*                       (2) a.b.c   - 255.255.65535
*                       (3) a.b     - 255.16777215
*                       (4) a       - 4294967295
*
*                   (d) Ensure that each decimal value does NOT include leading zeros.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
int  inet_aton (        char     *p_addr_in,
                struct  in_addr  *p_addr)
{
    in_addr_t   addr;
    CPU_INT08U  pdot_nbr;
    NET_ERR     err;


    addr = (in_addr_t)NetASCII_Str_to_IPv4_Handler( p_addr_in,
                                                   &pdot_nbr,
                                                   &err);

    if ((err      != NET_ASCII_ERR_NONE)            ||
        (pdot_nbr  > NET_ASCII_NBR_MAX_DOT_ADDR_IP)) {

        addr           = (in_addr_t)NET_BSD_ERR_NONE;
        p_addr->s_addr = addr;

        return (DEF_FAIL);
    }

    addr           = NET_UTIL_HOST_TO_NET_32(addr);
    p_addr->s_addr = addr;

    return (DEF_OK);
}
#endif


/*
*********************************************************************************************************
*                                               inet_ntop()
*
* Description : Converts an IPv4 or IPv6 Internet network address into a string in Internet standard format.
*
* Argument(s) : af      Address family:
*                       - AF_INET     Ipv4 Address Family
*                       - AF_INET6    Ipv6 Address Family
*
*               src     A pointer to the IP address in network byte to convert to a string.
*
*               dst     A pointer to a buffer in which to store the NULL-terminated string representation
*                       of the IP address.
*
*               size    Length, in characters, of the buffer pointed to by dst.
*
* Return(s)   : Pointer to a buffer containing the string representation of IP address in standard format,
*               if no error occurs.
*
*               DEF_NULL, otherwise.
*********************************************************************************************************
*/

const  char  *inet_ntop (int           af,
                         const  void  *src,
                         char         *dst,
                         socklen_t     size)
{
    char     *p_rtn = DEF_NULL;
    NET_ERR   err;


    switch (af) {
#ifdef  NET_IPv4_MODULE_EN
        case AF_INET:
             if (size < INET_ADDRSTRLEN) {
                 goto exit;
             }
             NetASCII_IPv4_to_Str(*((NET_IPv4_ADDR *)src), dst, DEF_NO, &err);
             if (err != NET_ASCII_ERR_NONE) {
                 goto exit;
             }
             break;
#endif

#ifdef  NET_IPv6_MODULE_EN
        case AF_INET6:
             if (size < INET6_ADDRSTRLEN) {
                 goto exit;
             }
             NetASCII_IPv6_to_Str((NET_IPv6_ADDR *)src, dst, DEF_NO, DEF_NO, &err);
             if (err != NET_ASCII_ERR_NONE) {
                 goto exit;
             }
             break;
#endif

             default:
                 goto exit;
    }

    p_rtn = dst;

exit:
    return ((const char *)p_rtn);
}


/*
*******************************************************************************************************
*                                               inet_pton()
*
* Description : Converts an IPv4 or IPv6 Internet network address in its standard text representation form
*               into its numeric binary form.
*
* Argument(s) : af      Address family:
*                       - AF_INET     Ipv4 Address Family
*                       - AF_INET6    Ipv6 Address Family
*
*               src     A pointer to the NULL-terminated string that contains the text representation of
*                       the IP address to convert to numeric binary form.
*
*               dst     A pointer to a buffer that will receive the numeric binary representation of the
*                       IP address.
*
* Return(s) :   1, if no error.
*               0, if src does not contain a character string representing a valid network address in
*                  the specified address family.
*              -1, if af does not contain a valid address family.
*******************************************************************************************************
*/

int  inet_pton (int           af,
                const  char  *src,
                void         *dst)
{
    int      rtn = NET_BSD_ERR_NONE;
    NET_ERR  err;
#ifdef  NET_IPv4_MODULE_EN
    NET_IPv4_ADDR  *p_addr_ipv4 = (NET_IPv4_ADDR *)dst;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_IPv6_ADDR  *p_addr_ipv6 = (NET_IPv6_ADDR *)dst;
#endif


    switch (af) {
#ifdef  NET_IPv4_MODULE_EN
        case AF_INET:
            *p_addr_ipv4 = NetASCII_Str_to_IPv4((CPU_CHAR *)src, &err);
             if (err != NET_ASCII_ERR_NONE) {
                 goto exit;
             }
             break;
#endif

#ifdef  NET_IPv6_MODULE_EN
        case AF_INET6:
            *p_addr_ipv6 = NetASCII_Str_to_IPv6((CPU_CHAR *)src, &err);
             if (err != NET_ASCII_ERR_NONE) {
                 goto exit;
             }
             break;
#endif

        default:
             rtn = NET_BSD_ERR_DFLT;
             goto exit;
    }

    rtn = 1;

exit:
    return (rtn);
}


/*
*********************************************************************************************************
*                                            setsockopt()
*
* Description : Set socket option.
*
* Argument(s) : sock_id     Socket descriptor/handle identifier of socket to set the option.
*
*               protocol    Protocol level at which the option resides.
*
*               opt_name    Name of the single socket option       to set.
*
*               p_opt_val   Pointer to the     socket option value to set.
*
*               opt_len     Option length.
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

int  setsockopt (int         sock_id,
                 int         protocol,
                 int         opt_name,
                 void       *p_opt_val,
                 socklen_t   opt_len)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_OptSet((NET_SOCK_ID      ) sock_id,
                                   (NET_SOCK_PROTOCOL) protocol,
                                   (NET_SOCK_OPT_NAME) opt_name,
                                   (void            *) p_opt_val,
                                   (NET_SOCK_OPT_LEN ) opt_len,
                                   (NET_ERR         *)&err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                            getsockopt()
*
* Description : Get socket option.
*
* Argument(s) : sock_id     Socket descriptor/handle identifier of socket to get the option.
*
*               protocol    Protocol level at which the option resides.
*
*               opt_name    Name of the single socket option       to get.
*
*               p_opt_val   Pointer to the     socket option value to get.
*
*               opt_len     Option length.
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

int  getsockopt (int         sock_id,
                 int         protocol,
                 int         opt_name,
                 void       *p_opt_val,
                 socklen_t  *p_opt_len)
{
    int      rtn_code;
    NET_ERR  err;


    rtn_code = (int)NetSock_OptGet((NET_SOCK_ID       ) sock_id,
                                   (NET_SOCK_PROTOCOL ) protocol,
                                   (NET_SOCK_OPT_NAME ) opt_name,
                                   (void             *) p_opt_val,
                                   (NET_SOCK_OPT_LEN *) p_opt_len,
                                   (NET_ERR          *)&err);

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                                 shutdown()
*
* Description : Shut down a socket's send and/or receive operations.
*
* Argument(s) : sock_id    Socket ID of the socket to shut down.
*
*               type       Indicates which operation(s) to shut down:
*                          SHUT_RD   The read-half of the socket is to be closed. No new data can be Rx'd
*                                    on the socket and any data currently present in the receive queue is
*                                    discarded. Any new data received is [ACK]'ed internally by TCP and
*                                    quietly discarded. However, data may still be transmitted through the
*                                    socket.
*
*                          SHUT_WR   The write-half of the socket is to be closed; which is equivalent to
*                                    performing a half-close on the TCP connection. No further data can be
*                                    sent through this socket. If the socket transmit queue is not empty,
*                                    its contents will be sent followed by a [FIN, ACK] frame.
*
*                          SHUT_RDWR Socket is closed on both directions. uC/TCP-IP does NOT keep a
*                                    reference counter for the socket descriptor, thus calling shutdown()
*                                    passing the SHUT_RDWR parameter is equivalent to calling close().
*
*
*
* Return(s)   : 0, if NO error(s).
*
*              -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) sock_id must be a stream socket and in a connected state.
*
*               (2) Stevens' UNIX Network Programming Volume 1, Second Edition, Section 6.6 states that if
*                   shutdown() is invoked with SHUT_RD as the second argument, then "no more data can be
*                   received on the socket and any data currently in the socket receive buffer is discarded."
*********************************************************************************************************
*/

int shutdown (int   sock_id,
              int   type)
{
    NET_SOCK          *p_sock;
    int                rtn_code;
    CPU_BOOLEAN        is_conn;
    CPU_BOOLEAN        is_valid;
    NET_SOCK_SD_MODE   mode;
    NET_CONN_ID        conn_id_tcp;
    NET_ERR            net_err;


    rtn_code =  NET_BSD_ERR_NONE;
    mode     = (NET_SOCK_SD_MODE)type;
    is_conn  =  NetSock_IsConn(sock_id, &net_err);

    is_valid = ( (is_conn == DEF_TRUE                   ) &&
                ((mode    == NET_SOCK_SHUTDOWN_MODE_RD  ) ||
                 (mode    == NET_SOCK_SHUTDOWN_MODE_WR  ) ||
                 (mode    == NET_SOCK_SHUTDOWN_MODE_RDWR)));

    Net_GlobalLockAcquire((void *)shutdown, &net_err);
    if (net_err != NET_ERR_NONE) {
        return (NET_BSD_ERR_DFLT);
    }

    if (!is_valid) {
        Net_GlobalLockRelease();                                /* Release global lock. Return errno val of EBADF or... */
        return (NET_BSD_ERR_DFLT);                              /* ...ENOTCONN depending on net_err; EINVAL if inv. mode*/
    }


    p_sock = NetSock_GetObj(sock_id);
    if (p_sock == (NET_SOCK *)0u) {
        Net_GlobalLockRelease();                                /* Release global lock. Return errno val of EBADF or... */
        return (NET_BSD_ERR_DFLT);
    }

    if (p_sock->SockType == NET_SOCK_TYPE_STREAM) {
        switch (p_sock->ShutdownMode) {
            case NET_SOCK_SHUTDOWN_MODE_NONE:
                 switch (mode) {
                     case NET_SOCK_SHUTDOWN_MODE_RDWR:          /* Close conn. uC/TCPIP keeps no socket reference ctrs. */
                          p_sock->ShutdownMode = NET_SOCK_SHUTDOWN_MODE_RDWR;
                          Net_GlobalLockRelease();

                          rtn_code = close(sock_id);

                          Net_GlobalLockAcquire((void *)shutdown, &net_err);
                          if (net_err != NET_ERR_NONE) {
                              return (NET_BSD_ERR_DFLT);
                          }
                          break;


                     case NET_SOCK_SHUTDOWN_MODE_RD:            /* Prevent any further receive calls.                   */
                          p_sock->ShutdownMode = NET_SOCK_SHUTDOWN_MODE_RD;
                          conn_id_tcp          = NetConn_ID_TransportGet(p_sock->ID_Conn, &net_err);

                          if (net_err == NET_CONN_ERR_NONE) {   /* Discard data in rx queue. Keep the connection's...   */
                                                                /* ...Tx resources intact.                              */
                                                                /* Abort wait on TCP conn rx Q.                         */
                              NetTCP_RxQ_Abort(conn_id_tcp, &net_err);

                              NetBuf_FreeBufQ_PrimList(NetTCP_ConnTbl[conn_id_tcp].RxQ_App_Head,
                                                       DEF_NULL);
                              NetTCP_ConnTbl[conn_id_tcp].RxQ_App_Tail = DEF_NULL;
                              NetTCP_ConnTbl[conn_id_tcp].RxQ_State    = NET_TCP_RX_Q_STATE_CLOSED;
                          } else {
                              rtn_code = (NET_BSD_ERR_DFLT);
                          }
                          break;


                     case NET_SOCK_SHUTDOWN_MODE_WR:            /* Perform a half-close of the TCP connection.          */
                          p_sock->ShutdownMode = NET_SOCK_SHUTDOWN_MODE_WR;
                          Net_GlobalLockRelease();

                          rtn_code = close(sock_id);

                          Net_GlobalLockAcquire((void *)shutdown, &net_err);
                          if (net_err != NET_ERR_NONE) {
                              return (NET_BSD_ERR_DFLT);
                          }
                          break;


                     default:
                          rtn_code = (NET_BSD_ERR_DFLT);        /* EINVAL error.                                        */
                          break;
                 }
                 break;


            case NET_SOCK_SHUTDOWN_MODE_RD:
                 switch (mode) {
                     case NET_SOCK_SHUTDOWN_MODE_WR:
                     case NET_SOCK_SHUTDOWN_MODE_RDWR:
                          p_sock->ShutdownMode = mode;
                          Net_GlobalLockRelease();

                          rtn_code = close(sock_id);

                          Net_GlobalLockAcquire((void *)shutdown, &net_err);
                          if (net_err != NET_ERR_NONE) {
                              return (NET_BSD_ERR_DFLT);
                          }
                          break;


                     case NET_SOCK_SHUTDOWN_MODE_RD:
                     default:
                          rtn_code = (NET_BSD_ERR_DFLT);        /* EINVAL error                                         */
                          break;
                 }
                 break;


            case NET_SOCK_SHUTDOWN_MODE_WR:
                 switch (mode) {
                     case NET_SOCK_SHUTDOWN_MODE_RD:
                     case NET_SOCK_SHUTDOWN_MODE_RDWR:
                          p_sock->ShutdownMode = mode;
                          Net_GlobalLockRelease();

                          rtn_code = close(sock_id);

                          Net_GlobalLockAcquire((void *)shutdown, &net_err);
                          if (net_err != NET_ERR_NONE) {
                              return (NET_BSD_ERR_DFLT);
                          }
                          break;


                     case NET_SOCK_SHUTDOWN_MODE_WR:
                     default:
                          rtn_code = (NET_BSD_ERR_DFLT);        /* EINVAL error                                         */
                          break;
                 }
                 break;


            default:
            case NET_SOCK_SHUTDOWN_MODE_RDWR:
                 rtn_code = (NET_BSD_ERR_DFLT);                /* Socket is already shut down in both directions.       */
                 break;
        }                                                      /* END switch (p_sock->ShutdownMode).                    */
    } else {
        rtn_code = (NET_BSD_ERR_DFLT);                         /* ENOTCONN error. UDP is connectionless.                */
    }

    Net_GlobalLockRelease();
    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               getpeername()
*
* Description : Returns the address of the peer connected on a socket.
*
* Argument(s) : sock_id  Socket whose peer name will get returned.
*
*               addr     Pointer to a struct sockaddr instance that will be populated with the peer's info.
*
*               len      Pointer to a socklen_t variable that will get the length of the peer's address.
*
* Return(s)   : 0, if no error,
*              -1, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) The addr->sa_data field is in network order.
*********************************************************************************************************
*/

int getpeername (        int         sock_id,
                 struct  sockaddr   *addr,
                         socklen_t  *addrlen)
{
    int                 rtn_code;
    CPU_INT08U          addr_remote[NET_BSD_ADDR_LEN_MAX];
    NET_CONN_ID         conn_id;
    NET_CONN_ADDR_LEN   len;
#ifdef  NET_IPv6_MODULE_EN
    int                 octets;
    int                 i;
#endif
    NET_SOCK           *p_sock;
    NET_ERR             net_err;


    net_err  = NET_CONN_ERR_INVALID_CONN;
    rtn_code = NET_BSD_ERR_DFLT;
    len      = NET_CONN_ADDR_LEN_MAX;

    Mem_Clr(addr_remote, NET_BSD_ADDR_LEN_MAX);
    p_sock = NetSock_GetObj(sock_id);
    if (p_sock != (NET_SOCK *)DEF_NULL) {
        conn_id = p_sock->ID_Conn;
        NetConn_AddrRemoteGet( conn_id,
                              &addr_remote[0],
                              &len,
                              &net_err);
    }

    if (net_err == NET_CONN_ERR_NONE) {
        Mem_Copy(addr->sa_data, &addr_remote[0], len);
        len = NET_SOCK_ADDR_LEN_IP_V4;

#ifdef  NET_IPv6_MODULE_EN                                      /* Determine actual address length.                     */
        octets = 0;

        for (i = 0; i < NET_BSD_ADDR_LEN_MAX; i++) {            /* Count how many non-zero octets we have.              */
             if (addr_remote[octets] != 0) {
                 octets++;
             }

             if (octets > NET_SOCK_ADDR_LEN_IP_V4) {            /* If nbr exceeds six octets, consider IPv6 addr length.*/
                 len = NET_SOCK_ADDR_LEN_IP_V6;
                 break;
             }
        }
#endif
        addr->sa_family = (len == NET_SOCK_ADDR_LEN_IP_V4) ? AF_INET : AF_INET6;
       *addrlen         = (socklen_t)len;
        rtn_code        = NET_BSD_ERR_NONE;
    }

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               getsockname()
*
* Description : Populates the locally bound name for the socket specified into the sockaddr provided.
*
* Argument(s) : sock_id  Socket whose local name will get returned.
*
*               addr     Pointer to a struct sockaddr instance that will be populated with the socket's info.
*
*               len      Pointer to a socklen_t variable that will get the length of the socket's address.
*
* Return(s)   : 0, if no error,
*              -1, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) The addr->sa_data field is in network order.
*********************************************************************************************************
*/

int getsockname (        int         sock_id,
                 struct  sockaddr   *addr,
                         socklen_t  *addrlen)
{
    int                 rtn_code;
    CPU_INT08U          addr_local[NET_BSD_ADDR_LEN_MAX];
    NET_CONN_ID         conn_id;
    NET_CONN_ADDR_LEN   len;
#ifdef  NET_IPv6_MODULE_EN
    int                 octets;
    int                 i;
#endif
    NET_SOCK           *p_sock;
    NET_ERR             net_err;


    net_err  = NET_CONN_ERR_INVALID_CONN;
    rtn_code = NET_BSD_ERR_DFLT;
    len      = NET_CONN_ADDR_LEN_MAX;

    Mem_Clr(addr_local, NET_BSD_ADDR_LEN_MAX);
    p_sock = NetSock_GetObj(sock_id);

    if (p_sock != (NET_SOCK *)DEF_NULL) {
        conn_id = p_sock->ID_Conn;
        NetConn_AddrLocalGet( conn_id,
                             &addr_local[0u],
                             &len,
                             &net_err);

        if (net_err == NET_CONN_ERR_NONE) {
            Mem_Copy(addr->sa_data, &addr_local[0], len);
            len = NET_SOCK_ADDR_LEN_IP_V4;

#ifdef  NET_IPv6_MODULE_EN                                      /* Determine actual address length.                     */
            octets = 0;

            for (i = 0; i < NET_BSD_ADDR_LEN_MAX; i++) {        /* Count how many non-zero octets we have.              */
                 if (addr_local[octets] != 0) {
                     octets++;
                 }

                 if (octets > NET_SOCK_ADDR_LEN_IP_V4) {        /* If nbr exceeds six octets, consider IPv6 addr length.*/
                     len = NET_SOCK_ADDR_LEN_IP_V6;
                     break;
                 }
            }
#endif
            addr->sa_family = (len == NET_SOCK_ADDR_LEN_IP_V4) ? AF_INET : AF_INET6;
           *addrlen         = (socklen_t)len;
            rtn_code        = NET_BSD_ERR_NONE;
        }
    }

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                               getaddrinfo()
*
* Description : Converts human-readable text strings representing hostnames or IP addresses into a
*               dynamically allocated linked list of struct addrinfo structures.
*
* Argument(s) : p_node_name     A pointer to a string that contains a host (node) name or a numeric host
*                               address string. For the Internet protocol, the numeric host address string
*                               is a dotted-decimal IPv4 address or an IPv6 hex address.
*
*               p_service_name  A pointer to a string that contains either a service name or port number
*                               represented as a string.
*
*               p_hints         A pointer to an addrinfo structure that provides hints about the type of
*                               socket the caller supports.
*
*               pp_res          A pointer to a linked list of one or more addrinfo structures that contains
*                               response information about the host.
*
* Return(s)   : 0, if no error,
*               Most nonzero error codes returned map to the set of errors outlined by Internet Engineering
*               Task Force (IETF) recommendations:
*                   - EAI_ADDRFAMILY
*                   - EAI_AGAIN
*                   - EAI_BADFLAGS
*                   - EAI_FAIL
*                   - EAI_FAMILY
*                   - EAI_MEMORY
*                   - EAI_NONAME
*                   - EAI_OVERFLOW
*                   - EAI_SERVICE
*                   - EAI_SOCKTYPE
*                   - EAI_SYSTEM
*
* Caller(s)   : Application.
*
* Note(s)     : none.
*********************************************************************************************************
*/

int  getaddrinfo (const          char       *p_node_name,
                  const          char       *p_service_name,
                  const  struct  addrinfo   *p_hints,
                         struct  addrinfo  **pp_res)
{
#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
            CPU_CHAR                canonname_str[DNSc_DFLT_HOST_NAME_LEN];
            DNSc_ERR                err_dns;
            CPU_BOOLEAN             canon_name        = DEF_NO;
#else
            NET_IP_ADDR_FAMILY      ip_family;
            NET_ERR                 err_net;
#endif
    struct  addrinfo               *p_addrinfo;
    struct  addrinfo               *p_addrinfo_head   = DEF_NULL;
    struct  addrinfo               *p_addrinfo_tail   = DEF_NULL;
            NET_IP_ADDR_FAMILY      hints_addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;
            NET_BSD_SOCK_PROTOCOL   hints_protocol    = NET_BSD_SOCK_PROTOCOL_UNKNOWN;
            NET_PORT_NBR            service_port      = NET_PORT_NBR_NONE;
            CPU_BOOLEAN             service_port_num  = DEF_NO;
            NET_BSD_SOCK_PROTOCOL   service_protocol  = NET_BSD_SOCK_PROTOCOL_UNKNOWN;
            CPU_BOOLEAN             valid_cfdg        = DEF_NO;
            CPU_BOOLEAN             host_num          = DEF_NO;
            CPU_BOOLEAN             wildcard          = DEF_NO;
            int                     rtn_val           = 0;
            CPU_BOOLEAN             result;
            NET_SOCK_PROTOCOL       protocol;
            CPU_BOOLEAN             set_ipv4;
            CPU_BOOLEAN             set_ipv6;


#ifdef  NET_IPv4_MODULE_EN
    set_ipv4 = DEF_YES;
#else
    set_ipv4 = DEF_NO;
#endif

#ifdef  NET_IPv6_MODULE_EN
    set_ipv6 = DEF_YES;
#else
    set_ipv6 = DEF_NO;
#endif

   *pp_res   = (struct addrinfo *)DEF_NULL;

    if ((p_node_name    == DEF_NULL) &&
        (p_service_name == DEF_NULL)) {
        rtn_val = EAI_NONAME;
        goto exit;
    }

    if (p_hints != DEF_NULL) {
        set_ipv4 = DEF_NO;
        set_ipv6 = DEF_NO;

        switch (p_hints->ai_socktype) {
            case 0:                                                 /* If ai_socktype is zero the caller will accept... */
                 hints_protocol = NET_BSD_SOCK_PROTOCOL_UNKNOWN;    /* ...any socket type.                              */
                 break;

#ifdef  NET_TCP_MODULE_EN
            case SOCK_STREAM:
                 hints_protocol = NET_BSD_SOCK_PROTOCOL_TCP;
                 break;
#endif

            case SOCK_DGRAM:
                 hints_protocol = NET_BSD_SOCK_PROTOCOL_UDP;
                 break;

            default:
                 rtn_val = EAI_SOCKTYPE;
                 goto exit;
        }


        switch (p_hints->ai_family) {
            case AF_UNSPEC:                                         /* When ai_family is set to AF_UNSPEC, it means ... */
                 hints_addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;    /* ...the caller will accept any address family ... */
                 break;                                             /* ...supported by the operating system.            */

#ifdef  NET_IPv4_MODULE_EN
            case AF_INET:
                 hints_addr_family = NET_IP_ADDR_FAMILY_IPv4;
                 break;
#endif

#ifdef  NET_IPv6_MODULE_EN
            case AF_INET6:
                 hints_addr_family = NET_IP_ADDR_FAMILY_IPv6;
                 break;
#endif

            default:
                 rtn_val = EAI_FAMILY;
                 goto exit;
        }


        switch (hints_addr_family) {
#ifdef  NET_IPv4_MODULE_EN
            case NET_IP_ADDR_FAMILY_IPv4:
                 set_ipv4 = DEF_YES;
                 set_ipv6 = DEF_NO;
                 break;
#endif

#ifdef  NET_IPv6_MODULE_EN
            case NET_IP_ADDR_FAMILY_IPv6:
                 set_ipv4 = DEF_NO;
                 set_ipv6 = DEF_YES;
                 break;
#endif

            case NET_IP_ADDR_FAMILY_UNKNOWN:
                 break;

            default:
                 rtn_val = EAI_SYSTEM;
                 goto exit;
        }

#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
        canon_name       = DEF_BIT_IS_SET(p_hints->ai_flags, AI_CANONNAME);
#endif

        wildcard         = DEF_BIT_IS_SET(p_hints->ai_flags, AI_PASSIVE);
                                                                /* If the AI_PASSIVE flag is specified in               */
                                                                /* hints.ai_flags, and node is NULL, then the returned  */
                                                                /* socket addresses will be suitable for bind(2)ing a   */
                                                                /* socket that will accept(2) connections.              */

        host_num         = DEF_BIT_IS_SET(p_hints->ai_flags, AI_NUMERICHOST);
                                                                /* If hints.ai_flags contains the AI_NUMERICHOST flag,  */
                                                                /* then node must be a numerical network address. The   */
                                                                /* AI_NUMERICHOST flag suppresses any potentially       */
                                                                /* lengthy network host address lookups.                */

        service_port_num = DEF_BIT_IS_SET(p_hints->ai_flags, AI_NUMERICSERV);
                                                                /* If AI_NUMERICSERV is specified in hints.ai_flags     */
                                                                /* and service is not NULL, then service must point to  */
                                                                /* a string containing a numeric port number.  This     */
                                                                /* flag is used to inhibit the invocation of a name     */
                                                                /* resolution service in cases where it is known not to */
                                                                /* be required.                                         */

        valid_cfdg       = DEF_BIT_IS_SET(p_hints->ai_flags, AI_ADDRCONFIG);
                                                                /* If the AI_ADDRCONFIG bit is set, IPv4 addresses      */
                                                                /* shall be returned only if an IPv4 address is         */
                                                                /* configured on the local system, and IPv6 addresses   */
                                                                /* shall be returned only if an IPv6 address is         */
                                                                /* configured on the local system.                      */
        if (valid_cfdg == DEF_YES) {
            NetBSD_AddrCfgValidate(&set_ipv4, &set_ipv6);
        }
    }   /* if (hints != DEF_NULL) */



                                                                /* If service is NULL, then the port number of the      */
                                                                /* returned socket addresses will be left uninitialized */

    if (p_service_name != DEF_NULL) {                           /* Service sets the port in each returned address       */
                                                                /* structure.  If this argument is a service name , it  */
                                                                /* is translated to the corresponding port number.      */
        CPU_SIZE_T  len;
        CPU_INT32U  i;
        CPU_INT32U  obj_ctn = sizeof(NetBSD_ServicesProtocolTbl) / sizeof(NET_BSD_SERVICE_PROTOCOL);


        if (service_port_num == DEF_NO) {
            len = Str_Len_N(p_service_name, NI_MAXSERV);
            service_port = NetDict_KeyGet(NetBSD_ServicesStrTbl, sizeof(NetBSD_ServicesStrTbl), p_service_name, DEF_NO, len);
            if (service_port >= NET_PORT_NBR_MAX) {
                service_port_num = DEF_YES;                     /* service argument can also be specified as a decimal  */
                                                                /* number, which is simply converted to binary.         */
            }
        }


        if (service_port_num == DEF_YES) {
            CPU_INT32U  val;

            val = Str_ParseNbr_Int32U(p_service_name, DEF_NULL, DEF_NBR_BASE_DEC);
            if ((val < NET_PORT_NBR_MIN) ||
                (val > NET_PORT_NBR_MAX)) {
                rtn_val = EAI_NONAME;
                goto exit;
            }

            service_port = (NET_PORT_NBR)val;
        }


        for (i = 0; i < obj_ctn; i++) {
            const NET_BSD_SERVICE_PROTOCOL  *p_obj = &NetBSD_ServicesProtocolTbl[i];

            if (service_port == p_obj->Port) {
                service_protocol = p_obj->Protocol;
                break;
            }
        }

        if (service_protocol == NET_BSD_SOCK_PROTOCOL_UNKNOWN) {
            rtn_val = EAI_SERVICE;
        }
    }


    switch (hints_protocol) {
        case NET_BSD_SOCK_PROTOCOL_UNKNOWN:
            switch (service_protocol) {
                case NET_BSD_SOCK_PROTOCOL_UNKNOWN:
                     protocol = NET_SOCK_PROTOCOL_NONE;
                     break;

                case NET_BSD_SOCK_PROTOCOL_UDP:
                case NET_BSD_SOCK_PROTOCOL_UDP_TCP:
                     protocol = NET_SOCK_PROTOCOL_UDP;
                     break;

                case NET_BSD_SOCK_PROTOCOL_TCP:
                case NET_BSD_SOCK_PROTOCOL_TCP_UDP:
                     protocol = NET_SOCK_PROTOCOL_TCP;
                     break;

                default:
                     rtn_val = EAI_SERVICE;
                     goto exit;
            }
            break;


        case NET_BSD_SOCK_PROTOCOL_UDP:
        case NET_BSD_SOCK_PROTOCOL_UDP_TCP:
             protocol = NET_SOCK_PROTOCOL_UDP;
             break;


        case NET_BSD_SOCK_PROTOCOL_TCP:
        case NET_BSD_SOCK_PROTOCOL_TCP_UDP:
             protocol = NET_SOCK_PROTOCOL_TCP;
             break;


        default:
             rtn_val = EAI_SERVICE;
             goto exit;
    }

    if (p_node_name == DEF_NULL) {
        if (set_ipv4 == DEF_YES) {
#ifdef  NET_IPv4_MODULE_EN
            if (wildcard == DEF_YES) {
                NET_IPv4_ADDR  addr = INADDR_ANY;
                                                                /* If the AI_PASSIVE flag is specified in               */
                                                                /* hints.ai_flags, and node is NULL, then the returned  */
                                                                /* socket addresses will be suitable for binding a      */
                                                                /* socket that will accept(2) connections.  The         */
                                                                /* returned socket address will contain the "wildcard   */
                                                                /* address" (INADDR_ANY for IPv4 addresses,             */
                                                                /* IN6ADDR_ANY_INIT for IPv6 address).                  */

                p_addrinfo = NetBSD_AddrInfoGet(&p_addrinfo_head, &p_addrinfo_tail);
                if (p_addrinfo == DEF_NULL) {
                    rtn_val = EAI_MEMORY;
                    goto exit_error;
                }


                result = NetBSD_AddrInfoSet(               p_addrinfo,
                                                           NET_SOCK_ADDR_FAMILY_IP_V4,
                                                           service_port,
                                            (CPU_INT08U *)&addr,
                                                           sizeof(addr),
                                                           protocol,
                                            (CPU_CHAR   *) DEF_NULL);
                if (result != DEF_OK) {
                    rtn_val = EAI_SYSTEM;
                    goto exit_error;
                }

            } else {
                NET_IPv4_ADDR  addr = INADDR_LOOPBACK;
                                                                /* If the AI_PASSIVE flag is not set in hints.ai_flags, */
                                                                /* then the returned socket addresses will be suitable  */
                                                                /* for use with connect(2), sendto(2), or sendmsg(2).   */
                                                                /* If node is NULL, then the network address will be    */
                                                                /* set to the loopback interface address (              */
                                                                /* INADDR_LOOPBACK for IPv4 addresses,                  */
                                                                /* IN6ADDR_LOOPBACK_INIT for IPv6 address); this is     */
                                                                /* used by applications that intend to communicate with */
                                                                /* peers running on the same host.                      */

                p_addrinfo = NetBSD_AddrInfoGet(&p_addrinfo_head, &p_addrinfo_tail);
                if (p_addrinfo == DEF_NULL) {
                    rtn_val = EAI_MEMORY;
                    goto exit_error;
                }

                result = NetBSD_AddrInfoSet(               p_addrinfo,
                                                           NET_SOCK_ADDR_FAMILY_IP_V4,
                                                           service_port,
                                            (CPU_INT08U *)&addr,
                                                           sizeof(INADDR_LOOPBACK),
                                                           protocol,
                                            (CPU_CHAR   *) DEF_NULL);
                if (result != DEF_OK) {
                    rtn_val = EAI_SYSTEM;
                    goto exit_error;
                }
            }
#endif
        } /* if (ipv4_set == DEF_YES) */



        if (set_ipv6 == DEF_YES) {
#ifdef  NET_IPv6_MODULE_EN
            if (wildcard == DEF_YES) {
                                                                /* If the AI_PASSIVE flag is specified in               */
                                                                /* hints.ai_flags, and node is NULL, then the returned  */
                                                                /* socket addresses will be suitable for binding a      */
                                                                /* socket that will accept(2) connections.  The         */
                                                                /* returned socket address will contain the "wildcard   */
                                                                /* address" (INADDR_ANY for IPv4 addresses,             */
                                                                /* IN6ADDR_ANY_INIT for IPv6 address).                  */
                p_addrinfo = NetBSD_AddrInfoGet(&p_addrinfo_head, &p_addrinfo_tail);
                if (p_addrinfo == DEF_NULL) {
                    rtn_val = EAI_MEMORY;
                    goto exit_error;
                }


                result = NetBSD_AddrInfoSet(               p_addrinfo,
                                                           NET_SOCK_ADDR_FAMILY_IP_V6,
                                                           service_port,
                                            (CPU_INT08U *)&in6addr_any,
                                                           sizeof(in6addr_any),
                                                           protocol,
                                            (CPU_CHAR   *) DEF_NULL);
                if (result != DEF_OK) {
                    rtn_val = EAI_SYSTEM;
                    goto exit_error;
                }

            } else {

                p_addrinfo = NetBSD_AddrInfoGet(&p_addrinfo_head, &p_addrinfo_tail);
                if (p_addrinfo == DEF_NULL) {
                    rtn_val = EAI_MEMORY;
                    goto exit_error;
                }

                result = NetBSD_AddrInfoSet(               p_addrinfo,
                                                           NET_SOCK_ADDR_FAMILY_IP_V6,
                                                           service_port,
                                            (CPU_INT08U *)&in6addr_loopback,
                                                           sizeof(in6addr_loopback),
                                                           protocol,
                                            (CPU_CHAR   *) DEF_NULL);
                if (result != DEF_OK) {
                    rtn_val = EAI_SYSTEM;
                    goto exit_error;
                }
            }
#endif
        } /* if (ipv6_set == DEF_YES) */


    } else { /* if (node == DEF_NULL) */
#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
        DNSc_FLAGS      dns_flags = DNSc_FLAG_NONE;
        DNSc_STATUS     status;
        CPU_INT08U      host_addr_nbr;
        DNSc_ADDR_OBJ  *p_dns_host_tbl;
        DNSc_ADDR_OBJ   dns_host_tbl[NET_BSD_MAX_DNS_ADDR_PER_HOST];
        CPU_INT08U      max_addr_per_host;


        Mem_Clr(canonname_str, sizeof(canonname_str));

        if ((set_ipv4 == DEF_YES) &&
            (set_ipv6 == DEF_NO)) {
            DEF_BIT_SET(dns_flags, DNSc_FLAG_IPv4_ONLY);
        } else if ((set_ipv4 == DEF_NO) &&
                   (set_ipv6 == DEF_YES )) {
            DEF_BIT_SET(dns_flags, DNSc_FLAG_IPv6_ONLY);
        }

        max_addr_per_host = set_ipv6                    ?
                            DNSc_Cfg.AddrIPv6MaxPerHost :
                            DNSc_Cfg.AddrIPv4MaxPerHost;

        p_dns_host_tbl    = dns_host_tbl;

        host_addr_nbr     = max_addr_per_host;

        if (canon_name == DEF_TRUE) {
            DEF_BIT_SET(dns_flags, DNSc_FLAG_CANON);            /* Instruct DNSc_GetHost() to process canonical names.  */
        }

        status = DNSc_GetHost( p_node_name,
                               canonname_str,
                               sizeof(canonname_str),
                               p_dns_host_tbl,
                              &host_addr_nbr,
                               dns_flags,
                               DEF_NULL,
                              &err_dns);

        switch (status) {
            case DNSc_STATUS_RESOLVED:
                 for (int i = 0; i < host_addr_nbr; i++) {
                     NET_SOCK_ADDR_FAMILY sock_family = NET_SOCK_ADDR_FAMILY_IP_V4;


                     p_addrinfo = NetBSD_AddrInfoGet(&p_addrinfo_head, &p_addrinfo_tail);
                     if (p_addrinfo == DEF_NULL) {
                         rtn_val = EAI_MEMORY;
                         goto exit_error;
                     }
                     switch (p_dns_host_tbl->Len) {
                         case NET_IPv4_ADDR_LEN:
                              sock_family = NET_SOCK_ADDR_FAMILY_IP_V4;
                              break;


                         case NET_IPv6_ADDR_LEN:
                              sock_family = NET_SOCK_ADDR_FAMILY_IP_V6;
                              break;


                         default:
                              rtn_val = EAI_SYSTEM;
                              goto exit_error;
                     }

                     result = NetBSD_AddrInfoSet(              p_addrinfo,
                                                               sock_family,
                                                               service_port,
                                                 (CPU_INT08U *)p_dns_host_tbl->Addr,
                                                               p_dns_host_tbl->Len,
                                                               protocol,
                                                               canonname_str);
                     if (result != DEF_OK) {
                         rtn_val = EAI_SYSTEM;
                         goto exit_error;
                     }
                    *p_dns_host_tbl++;
                 }

                *pp_res = p_addrinfo_head;
                 break;


            case DNSc_STATUS_FAILED:
                 rtn_val = EAI_FAIL;
                 break;


            case DNSc_STATUS_PENDING:
                 rtn_val = EAI_AGAIN;
                 goto exit;


            case DNSc_STATUS_UNKNOWN:
            case DNSc_STATUS_NONE:
                 rtn_val = EAI_AGAIN;
                 goto exit;


            default:
                 rtn_val = EAI_SYSTEM;
                 goto exit_error;
        }


       (void)host_num;                                          /* Prevent variable unused. Numeric conversion is done  */
                                                                /* by DNSc_GetHostAddrs as first step                   */

#else
       (void)pp_res;
        if (host_num == DEF_NO) {
            rtn_val = EAI_SYSTEM;
            goto exit_error;
        }

        p_addrinfo = NetBSD_AddrInfoGet(&p_addrinfo_head, &p_addrinfo_tail);
        if (p_addrinfo == DEF_NULL) {
            rtn_val = EAI_MEMORY;
            goto exit_error;
        }


        ip_family = NetASCII_Str_to_IP((CPU_CHAR *)p_node_name,
                                                   p_addrinfo->ai_addr,
                                                   sizeof(struct  sockaddr),
                                                  &err_net);
        if (err_net == NET_ASCII_ERR_NONE) {
            NET_SOCK_ADDR_FAMILY  sock_family = NET_SOCK_ADDR_FAMILY_IP_V4;
            NET_IP_ADDR_LEN       addr_len    = NET_IPv4_ADDR_LEN;


            switch (ip_family) {
                case NET_IP_ADDR_FAMILY_IPv4:
                     sock_family = NET_SOCK_ADDR_FAMILY_IP_V4;
                     addr_len    = NET_IPv4_ADDR_LEN;
                     break;

                case NET_IP_ADDR_FAMILY_IPv6:
                     sock_family = NET_SOCK_ADDR_FAMILY_IP_V6;
                     addr_len    = NET_IPv6_ADDR_LEN;
                     break;

                default:
                    break;
            }


            result = NetBSD_AddrInfoSet(              p_addrinfo,
                                                      sock_family,
                                                      service_port,
                                        (CPU_INT08U *)p_addrinfo->ai_addr,
                                                      addr_len,
                                                      protocol,
                                        (CPU_CHAR   *)DEF_NULL);
            if (result != DEF_OK) {
                rtn_val = EAI_SYSTEM;
                goto exit_error;
            }
        }
#endif
    }


    goto exit;


exit_error:
#if (NET_EXT_MODULE_CFG_DNS_EN == DEF_ENABLED)
    DNSc_CacheClrHost((CPU_CHAR *)p_node_name, &err_dns);
#endif
    NetBSD_AddrInfoFree(p_addrinfo_head);

exit:
    return (rtn_val);
}


/*
*********************************************************************************************************
*                                               getnameinfo()
*
* Description : Converts a socket address to a string describing the host and another string describing
*               the service.
*
* Argument(s) : p_sockaddr      Pointer to the sockaddr structure with the protocol address to be converted.
*
*               addrlen         Length of the sockaddr structure pointed to by p_sockaddr.
*
*               p_host_name     Pointer to the host name string allocated by the caller.
*
*               hostlen         Length of string pointed to by p_host_name.
*
*               p_service_name  Pointer to the service name string allocated by the caller.
*
*               servicelen      Length of string pointed to by p_service_name.
*
*               flags           Flags that change how this function converts the socket address.
*                               NI_NAMEREQD
*                               NI_DGRAM
*                               NI_NOFQDN
*                               NI_NUMERICHOST
*                               NI_NUMERICSERV
*
* Return(s)   : 0, if no error,
*               Most nonzero error codes returned map to the set of errors outlined by Internet Engineering
*               Task Force (IETF) recommendations:
*                   - EAI_AGAIN
*                   - EAI_BADFLAGS
*                   - EAI_FAIL
*                   - EAI_FAMILY
*                   - EAI_MEMORY
*                   - EAI_NONAME
*                   - EAI_OVERFLOW
*                   - EAI_SERVICE
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Only IPv4 reverse lookups are supported at the moment.
*
*               (2) It is impractical for an embedded TCP-IP stack to keep and maintain an internal lookup
*                   table containing the service names of every IANA-registered port (or even well-known
*                   ports); it is resource-intensive and the associated protocols may change. Therefore,
*                   we will only return service names for those services that the uC/TCP-IP network suite
*                   supports. For the services that we do NOT support, getnameinfo() will act as if the
*                   NI_NUMERICSERV flag were passed to it. That is, a string representation of the port
*                   number will be returned, say "22" in lieu of "ssh".
*
*               (3) At least one host name OR one service name must be requested.
*********************************************************************************************************
*/

int  getnameinfo (const  struct  sockaddr  *p_sockaddr,
                                 int        addrlen,
                                 char      *p_host_name,
                                 int        hostlen,
                                 char      *p_service_name,
                                 int        servicelen,
                                 int        flags)
{
    int                     rtn_val;
#ifdef  NET_EXT_MODULE_DNS_EN
    NET_IP_ADDR_FAMILY      family;
#ifdef  NET_IPv4_MODULE_EN
    NET_IPv4_ADDR           addr_ipv4;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_IPv6_ADDR           addr_ipv6;
    CPU_INT08U              ipv6_addr_offset;
#endif
    CPU_CHAR                p_remote_host_name[DNSc_DFLT_HOST_NAME_LEN];
    DNSc_ADDR_OBJ           addr_dns[NET_BSD_MAX_DNS_ADDR_PER_HOST];
    DNSc_STATUS             status;
    DNSc_ERR                dns_err;
    CPU_INT08U              addr_nbr;
    DNSc_FLAGS              dns_flags         = DNSc_FLAG_NONE;
    NET_BSD_SOCK_PROTOCOL   service_protocol  = NET_BSD_SOCK_PROTOCOL_UNKNOWN;
    CPU_CHAR               *p_service_nbr_str = "";
    CPU_CHAR               *p_no_fqdn_char    = "";
    CPU_INT16U              service_port;
    CPU_BOOLEAN             host_is_num;
    CPU_BOOLEAN             service_port_is_num;
    CPU_BOOLEAN             name_is_required;
    CPU_BOOLEAN             is_datagram;
    CPU_BOOLEAN             is_fully_qual_domain;


    family  = (NET_IP_ADDR_FAMILY)p_sockaddr->sa_family;
    rtn_val = NET_BSD_ERR_NONE;

    if ((p_host_name    == DEF_NULL)  &&                        /* See Note (3).                                        */
        (p_service_name == DEF_NULL)) {
        rtn_val = EAI_NONAME;
        goto exit;
    }

    if ((p_host_name == DEF_NULL)  &&
        (hostlen     != 0)) {
        rtn_val = EAI_SYSTEM;                                   /* errno should be set to EINVAL.                       */
        goto exit;
    }

    if ((p_service_name == DEF_NULL)  &&
        (servicelen     != 0)) {
        rtn_val = EAI_SYSTEM;                                   /* errno should be set to EINVAL.                       */
        goto exit;
    }
                                                                /* If NI_NUMERICSERV is set in flags, p_service_name... */
                                                                /* ...will be resolved to the string representation ... */
                                                                /* ...of numeric value of the service port.             */
    service_port_is_num  = DEF_BIT_IS_SET(flags, NI_NUMERICSERV);

                                                                /* If NI_NUMERICHOST is set in flags, p_host_name   ... */
                                                                /* ...will be resolved to the string representation ... */
                                                                /* ...of the IP address in dotted form.                 */
    host_is_num          = DEF_BIT_IS_SET(flags, NI_NUMERICHOST);

                                                                /* If NI_NAMEREQD is set in flags, an error will be ... */
                                                                /* ...returned if the host name cannot be resolved.     */
    name_is_required     = DEF_BIT_IS_SET(flags, NI_NAMEREQD);

                                                                /* If NI_DGRAM is set in flags, the resolved service... */
                                                                /* ...name is the name of the UDP service used by   ... */
                                                                /* ...that port. Some ports are shared by TCP and UDP.  */
    is_datagram          = DEF_BIT_IS_SET(flags, NI_DGRAM);

                                                                /* If NI_NOFQDN is set in flags, the resolved host   ...*/
                                                                /* ...name will be truncated until the first '.' char.  */
    is_fully_qual_domain = DEF_BIT_IS_CLR(flags, NI_NOFQDN);

    if (flags > NET_BSD_NI_MAX_FLAG) {
        rtn_val = EAI_BADFLAGS;
        goto exit;
    }

    Mem_Clr(p_remote_host_name, sizeof(p_remote_host_name));
    Mem_Clr(p_host_name,        hostlen);

    switch(family) {
#ifdef  NET_IPv4_MODULE_EN
        case AF_INET:
             if (addrlen < NET_IPv4_ADDR_LEN) {
                 rtn_val = EAI_FAMILY;
                 goto exit;
             }
             Mem_Copy(&addr_ipv4,
                      &p_sockaddr->sa_data[NET_SOCK_ADDR_IP_LEN_PORT],
                       NET_IPv4_ADDR_LEN);

             DEF_BIT_SET(dns_flags, DNSc_FLAG_IPv4_ONLY);       /* Get IPv4 address only.                               */

             addr_ipv4 = NET_UTIL_HOST_TO_NET_32(addr_ipv4);
             inet_ntop(AF_INET, &addr_ipv4, p_remote_host_name, INET_ADDRSTRLEN);
             Mem_Copy(&service_port, &p_sockaddr->sa_data, NET_SOCK_ADDR_IP_LEN_PORT);
             service_port = NET_UTIL_HOST_TO_NET_16(service_port);
             break;
#endif


#ifdef  NET_IPv6_MODULE_EN
        case AF_INET6:                                          /* See Note (1).                                        */
             if (addrlen < NET_IPv6_ADDR_LEN) {
                 rtn_val = EAI_FAMILY;
                 goto exit;
             }
                                                                /* Skip Port & FlowInfo fields to calculate addr offset.*/
             ipv6_addr_offset = sizeof(NET_PORT_NBR) + sizeof(CPU_INT32U);

             Mem_Copy(&addr_ipv6, &p_sockaddr->sa_data[ipv6_addr_offset], NET_IPv6_ADDR_LEN);

             DEF_BIT_SET(dns_flags, DNSc_FLAG_IPv6_ONLY);       /* Get IPv6 address only.                               */

             inet_ntop(AF_INET6, &addr_ipv6, p_remote_host_name, INET6_ADDRSTRLEN);
             Mem_Copy(&service_port, &p_sockaddr->sa_data, NET_SOCK_ADDR_IP_LEN_PORT);
             service_port = NET_UTIL_HOST_TO_NET_16(service_port);
             break;
#endif


        default:
             rtn_val = EAI_FAMILY;
             goto exit;
    }


    if (host_is_num) {                                          /* If hostname flagged as numeric, avoid DNS resolution.*/
        Str_Copy_N(p_host_name, p_remote_host_name, hostlen);
    } else {
        if (hostlen > 0u) {                                     /* ------------------ DNS RESOLUTION ------------------ */
            DEF_BIT_SET(dns_flags, DNSc_FLAG_REVERSE_LOOKUP);

            addr_nbr = (family == AF_INET)          ?
                        DNSc_Cfg.AddrIPv4MaxPerHost :
                        DNSc_Cfg.AddrIPv6MaxPerHost;

            status   = DNSc_GetHost( p_remote_host_name,
                                     p_host_name,
                                     hostlen,
                                     addr_dns,
                                    &addr_nbr,
                                     dns_flags,
                                     DEF_NULL,
                                    &dns_err);
            switch (status) {
                case DNSc_STATUS_UNKNOWN:                       /* Host was just resolved or already in cache; copy...  */
                case DNSc_STATUS_RESOLVED:                      /* ...updated 'p_remote_host_name' to 'p_host_name' arg.*/
                     break;


#ifdef   DNSc_TASK_MODULE_EN
                case DNSc_STATUS_NONE:
#endif
                case DNSc_STATUS_PENDING:
                     rtn_val = EAI_AGAIN;
                     goto exit;


                case DNSc_STATUS_FAILED:
                     if (name_is_required) {                    /* If there is a resolution failure but NI_NAMEREQD...  */
                         rtn_val = EAI_NONAME;                  /* ...flag is set, return with an error right away.     */
                         goto exit;
                     }
                     Str_Copy_N(p_host_name, p_remote_host_name, hostlen);
                     break;                                     /* Otherwise use dotted decimal notation as host name.  */


                default:
                     rtn_val = EAI_SYSTEM;
                     goto exit;
            }
        }
    }

    if (is_fully_qual_domain != DEF_TRUE) {                     /* If NI_NOFQDN flag set, truncate hostname at first... */
        if (host_is_num) {                                      /* ...instance of '.' char if hostname is NOT numeric.  */
            rtn_val = EAI_NONAME;
            goto exit;
        }
        p_no_fqdn_char = Str_Char_N(p_host_name, '.', hostlen);
       *p_no_fqdn_char = '\0';
    }
                                                                /* ------------- SERVICE NAME RESOLUTION  ------------- */
    if ((p_service_name != DEF_NULL) && (servicelen > 0u)) {
        CPU_INT32U   i;
        CPU_INT32U   obj_ctn = sizeof(NetBSD_ServicesProtocolTbl) / sizeof(NET_BSD_SERVICE_PROTOCOL);
        NET_DICT    *p_entry;


        if (service_port_is_num == DEF_NO) {
            p_entry = NetDict_EntryGet(NetBSD_ServicesStrTbl,
                                       sizeof(NetBSD_ServicesStrTbl),
                                       service_port);

            if (p_entry != DEF_NULL) {
                Str_Copy_N(p_service_name, p_entry->StrPtr, servicelen);
                for (i = 0u; i < obj_ctn; i++) {
                    const  NET_BSD_SERVICE_PROTOCOL  *p_obj = &NetBSD_ServicesProtocolTbl[i];

                    if (service_port == p_obj->Port) {
                        service_protocol = p_obj->Protocol;
                        break;
                    }
                }
                                                                /* If a datagram resolution was requested and the   ... */
                                                                /* ... service name was resolved to a known TCP-only... */
                                                                /* ... protocol, then return a resolution error back... */
                                                                /* ... to the caller.                                   */
                if (((is_datagram      == DEF_TRUE                     )  &&
                     (service_protocol == NET_BSD_SOCK_PROTOCOL_TCP    )) ||
                     (service_protocol == NET_BSD_SOCK_PROTOCOL_UNKNOWN)) {
                    rtn_val = EAI_NONAME;
                }
            } else {                                            /* If a service port is not defined, treat service  ... */
                service_port_is_num = DEF_YES;                  /* ...name resolution as if NI_NUMERICSERV flag was set.*/
            }                                                   /* See Note (2).                                        */
        }

        if (service_port_is_num == DEF_YES) {                   /* Convert service port number to string representation.*/
            if (service_port < NET_PORT_NBR_MIN) {
                rtn_val = EAI_NONAME;
                goto exit;
            }

            p_service_nbr_str = Str_FmtNbr_Int32U(service_port,
                                                  5u,
                                                  DEF_NBR_BASE_DEC,
                                                  DEF_NULL,
                                                  DEF_NO,
                                                  DEF_YES,
                                                  p_service_name);
            if (p_service_nbr_str == DEF_NULL) {
                rtn_val = EAI_NONAME;
            }
        }
    }


exit:

#else
    rtn_val = NET_BSD_ERR_DFLT;
#endif

    return (rtn_val);
}


/*
*********************************************************************************************************
*                                               gai_strerror()
*
* Description : Converts the numerical value of EAI-type error codes returned by getaddrinfo() into a
*               meaningful string.
*
* Argument(s) : errcode  The numerical (integer) value of the error code returned by getaddrinfo().
*                        - EAI_ADDRFAMILY
*                        - EAI_AGAIN
*                        - EAI_BADFLAGS
*                        - EAI_FAIL
*                        - EAI_FAMILY
*                        - EAI_MEMORY
*                        - EAI_NONAME
*                        - EAI_OVERFLOW
*                        - EAI_SERVICE
*                        - EAI_SOCKTYPE
*                        - EAI_SYSTEM
*
* Return(s)   : Pointer to a string that will contain the description of the EAI error so long as it is
*               one of the EAI codes listed in the arguments section above. Otherwise, the string
*               "Unknown EAI error." is returned.
*
* Caller(s)   : Application
*
* Note(s)     : None.
*********************************************************************************************************
*/

const  char  *gai_strerror (int  errcode)
{
           NET_DICT  *p_entry;
    const  char      *rtn_str = NET_BSD_EAI_ERR_UNKNOWN_STR;


    p_entry = NetDict_EntryGet(NetBSD_AddrInfoErrStrTbl,
                               sizeof(NetBSD_AddrInfoErrStrTbl),
                               errcode);
    if (p_entry != DEF_NULL) {
        rtn_str = p_entry->StrPtr;
    }

    return (rtn_str);
}


/*
*********************************************************************************************************
*                                               freeaddrinfo()
*
* Description : Frees addrinfo structures information that getaddrinfo has allocated.
*
* Argument(s) : res     A pointer to the addrinfo structure or linked list of addrinfo structures to be freed.
*
* Return(s)   : None.
*
* Caller(s)   : Application
*
* Note(s)     : None.
*********************************************************************************************************
*/

void freeaddrinfo(struct addrinfo *res)
{
    NetBSD_AddrInfoFree(res);
}


/*
*********************************************************************************************************
*                                           gethostname()
*
* Description : Returns the name of host in a null-terminated c-string array.
*
* Argument(s) : host_name  Character array that will receive the returned host name.
*
*               name_len   This is the length of the host name array in bytes including null-terminating char.
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) If the provided host name string is smaller than the actual host name then the string will
*                   be truncated. It is unspecified whether the returned buffer will include a terminating null
*                   byte or not. This function will append a terminating character even if truncation occurs.
*
*               (2) User MUST provide a correct name_len val for the size of the host_name string passed to
*                   this function. It will not account for this value being inaccurate. This value MUST
*                   include a space for the null terminating character as well.
*********************************************************************************************************
*/

int  gethostname (char   *host_name,
                 _size_t  name_len)
{
    int        rtn_code;
#if (CPU_CFG_NAME_EN == DEF_ENABLED)                            /* Ensure functionality is properly supported           */
    CPU_CHAR   temp_host_name[CPU_CFG_NAME_SIZE];               /* Used to hold string for truncation situation         */
   _size_t     temp_name_len;
    CPU_ERR    p_err;


    CPU_NameGet( temp_host_name,                                /* Used to get the desired host name                    */
                &p_err);
    if (p_err == CPU_ERR_NONE) {                                /* Check the success of host name search                */
        rtn_code = NET_BSD_ERR_NONE;
    } else {
        rtn_code = NET_BSD_ERR_DFLT;
    }
    if (rtn_code != NET_BSD_ERR_DFLT) {
        temp_name_len = Str_Len(temp_host_name);                /* Check if given host len exceeds actual host len      */
        if (name_len >= temp_name_len) {
            name_len = temp_name_len;
        } else if (name_len != 0u) {                            /* Truncated string needs space for null terminator     */
            name_len = name_len - 1;
        } else {
                                                                /* Empty Else Statement                                 */
        }
        Str_Copy_N(host_name,                                   /* Controlled copy of host name in case of truncation   */
                   temp_host_name,
                   name_len);
    }
#else                                                           /* Required functionality not enabled, return error     */
    rtn_code = NET_BSD_ERR_DFLT;
#endif

    return (rtn_code);
}


/*
*********************************************************************************************************
*                                           sethostname()
*
* Description : Sets the host name with the given value in the character array.
*
* Argument(s) : host_name  Character array with the desired host name.
*
*               name_len   Specifies the number of bytes in the given host name string.
*
* Return(s)   :  0, if NO error(s).
*
*               -1, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) Since you are specifiying the length of the character string you do not need a null
*                   terminating byte. Our CPU_NameSet() expects a null-terminating string so we concatenate
*                   it on to avoid impacting the users expectations of this BSD function. This is done with
*                   a call to Str_Copy() since we can't modify a const string that is passed into the function.
*                   This function is not called excessively so the overhead impact will be minimal.
*
*               (2) With name_len being unsigned there's no need to compare for a negative value. Although the
*                   specifications do say the length value should be non-negative.
*********************************************************************************************************
*/

int  sethostname(const  char    *host_name,
                       _size_t   name_len)
{
    int      rtn_code;
#if (CPU_CFG_NAME_EN == DEF_ENABLED)                            /* Ensure functionality is properly supported           */
    char     host_name_tmp[CPU_CFG_NAME_SIZE];
    CPU_ERR  p_err;


    if (name_len >= CPU_CFG_NAME_SIZE) {
        rtn_code = NET_BSD_ERR_DFLT;                            /* If len is negative or exceeds limit return EINVAL    */
    } else {
        Str_Copy(host_name_tmp,
                 host_name);
        CPU_NameSet( host_name,                                 /* Used to set the desired host name                    */
                    &p_err);
        if (p_err == CPU_ERR_NONE) {                            /* Check the- success of host name search               */
            rtn_code = NET_BSD_ERR_NONE;
        } else {
            rtn_code = NET_BSD_ERR_DFLT;
        }
    }
#else                                                           /* Required functionality not enabled, return error     */
    rtn_code = NET_BSD_ERR_DFLT;
#endif

    return (rtn_code);
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
*                                           NetBSD_AddrInfoGet()
*
* Description : Allocate and link to a list an addrinfo structure
*
* Argument(s) : pp_head     Pointer to the head pointer list
*
*               pp_tail     Pointer to the tail pointer list
*
* Return(s)   : Pointer to the allocated addrinfo structure.
*
* Caller(s)   : getaddrinfo().
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  struct  addrinfo  *NetBSD_AddrInfoGet (struct  addrinfo  **pp_head,
                                               struct  addrinfo  **pp_tail)
{
    struct  addrinfo  *p_addrinfo;
    struct  sockaddr  *p_sockaddr;
    struct  addrinfo  *p_addrinfo_rtn = DEF_NULL;
            LIB_ERR    lib_err;
#ifdef  NET_EXT_MODULE_DNS_EN
            CPU_CHAR  *p_canonical_name;
#endif


    p_addrinfo = (struct  addrinfo  *)Mem_DynPoolBlkGet(&NetBSD_AddrInfoPool, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
        goto exit;
    }


    p_sockaddr = (struct  sockaddr  *)Mem_DynPoolBlkGet(&NetBSD_SockAddrPool, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
        NetBSD_AddrInfoFree(p_addrinfo);
        goto exit;
    }

#ifdef  NET_EXT_MODULE_DNS_EN
    p_canonical_name = (CPU_CHAR *)Mem_DynPoolBlkGet(&NetBSD_SockAddrCanonNamePool, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
        NetBSD_AddrInfoFree(p_addrinfo);
        goto exit;
    }
    p_addrinfo->ai_canonname = p_canonical_name;
#else
    p_addrinfo->ai_canonname = (CPU_CHAR *)DEF_NULL;
#endif

    p_addrinfo->ai_addr      = p_sockaddr;
    p_addrinfo_rtn           = p_addrinfo;

    if (*pp_head == DEF_NULL) {
        *pp_head = p_addrinfo_rtn;
    }

    if (*pp_tail != DEF_NULL) {
      (*pp_tail)->ai_next = p_addrinfo_rtn;
    }

    *pp_tail = p_addrinfo_rtn;

exit:
    return (p_addrinfo_rtn);
}


/*
*********************************************************************************************************
*                                           NetBSD_AddrInfoFree()
*
* Description : Free an addrinfo structure
*
* Argument(s) : p_addrinfo  Pointer to the addrinfo structure to free.
*
* Return(s)   : None.
*
* Caller(s)   : freeaddrinfo().
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetBSD_AddrInfoFree (struct  addrinfo  *p_addrinfo)
{
    struct  addrinfo  *p_blk = p_addrinfo;
    struct  addrinfo  *next_blk;
            LIB_ERR    err;


    while (p_blk != DEF_NULL) {
        if (p_blk->ai_addr != DEF_NULL) {
            Mem_DynPoolBlkFree(&NetBSD_SockAddrPool, p_blk->ai_addr, &err);
            p_blk->ai_addr = DEF_NULL;
        }

#ifdef  NET_EXT_MODULE_DNS_EN
        Mem_DynPoolBlkFree(&NetBSD_SockAddrCanonNamePool, p_blk->ai_canonname, &err);
#endif

        Mem_DynPoolBlkFree(&NetBSD_AddrInfoPool, p_blk, &err);

        next_blk            = p_blk->ai_next;
        p_blk->ai_next      = DEF_NULL;
        p_blk->ai_canonname = DEF_NULL;
        p_blk               = next_blk;

       (void)&err;
    }
}


/*
*********************************************************************************************************
*                                           NetBSD_AddrInfoSet()
*
* Description : Set addrinfo's field.
*
* Argument(s) : p_addrinfo      Pointer to the addrinfo structure to be filled.
*
*               family          Socket family
*
*               port_nbr        Port number
*
*               p_addr          Pointer to the addrinfo structure to be filled.
*
*               addr_len        IP address length
*
*               protocol        Socket protocol
*
*               p_canonname     String indicating the host's canonical name.
*
* Return(s)   : DEF_OK,   if the address was set successfully.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : getaddrinfo().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetBSD_AddrInfoSet (struct  addrinfo              *p_addrinfo,
                                                 NET_SOCK_ADDR_FAMILY   family,
                                                 NET_PORT_NBR           port_nbr,
                                                 CPU_INT08U            *p_addr,
                                                 NET_IP_ADDR_LEN        addr_len,
                                                 NET_SOCK_PROTOCOL      protocol,
                                                 CPU_CHAR              *p_canonname)
{
    CPU_BOOLEAN  result = DEF_FAIL;
    NET_ERR      err;


    NetApp_SetSockAddr((NET_SOCK_ADDR *)p_addrinfo->ai_addr,
                                        family,
                                        port_nbr,
                                        p_addr,
                                        addr_len,
                                       &err);
    if (err != NET_APP_ERR_NONE) {
        goto exit;
    }

    p_addrinfo->ai_family   =      family;
    p_addrinfo->ai_addrlen  =      addr_len;
    p_addrinfo->ai_protocol = (int)protocol;

#ifdef  NET_EXT_MODULE_DNS_EN
    Str_Copy_N(p_addrinfo->ai_canonname,
               p_canonname,
               DNSc_DFLT_HOST_NAME_LEN);
#else
    p_addrinfo->ai_canonname = p_canonname;
#endif
    result = DEF_OK;

exit:
    return (result);
}



/*
*********************************************************************************************************
*                                           NetBSD_AddrCfgValidate()
*
* Description : Validate that IPs are configured on interface(s).
*
* Argument(s) : p_ipv4_cfgd     Pointer to a variable that will receive the IPv4 address configuration status
*
*               p_ipv6_cfgd     Pointer to a variable that will receive the IPv6 address configuration status
*
* Return(s)   : None.
*
* Caller(s)   : getaddrinfo().
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetBSD_AddrCfgValidate (CPU_BOOLEAN  *p_ipv4_cfgd,
                                      CPU_BOOLEAN  *p_ipv6_cfgd)
{
    NET_IF_NBR  if_nbr_ix;
    NET_IF_NBR  if_nbr_cfgd;
    NET_IF_NBR  if_nbr_base;
    NET_ERR     err;


    if_nbr_base  = NetIF_GetNbrBaseCfgd();
    if_nbr_cfgd  = NetIF_GetExtAvailCtr(&err);
    if_nbr_cfgd -= if_nbr_base;



#ifdef  NET_IPv4_MODULE_EN
     if (*p_ipv4_cfgd == DEF_YES) {
         for (if_nbr_ix = 0; if_nbr_ix <= if_nbr_cfgd; if_nbr_ix++) {
             *p_ipv4_cfgd = NetIPv4_IsAddrsCfgdOnIF(if_nbr_ix, &err);
             if (*p_ipv4_cfgd == DEF_YES) {
                 break;
             }
         }
     }
#else
    *p_ipv4_cfgd = DEF_NO;
#endif


#ifdef  NET_IPv6_MODULE_EN
    if (*p_ipv6_cfgd == DEF_YES) {
        for (if_nbr_ix = 0; if_nbr_ix <= if_nbr_cfgd; if_nbr_ix++) {
            *p_ipv6_cfgd = NetIPv6_IsAddrsCfgdOnIF(if_nbr_ix, &err);
             if (*p_ipv6_cfgd == DEF_YES) {
                 break;
             }
        }
    }
#else
   *p_ipv6_cfgd = DEF_NO;
#endif
}


#endif  /* NET_SOCK_BSD_EN */
