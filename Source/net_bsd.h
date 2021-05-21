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
* Filename  : net_bsd.h
* Version   : V3.05.04
*********************************************************************************************************
* Note(s)   : (1) Supports BSD 4.x Layer API with the following restrictions/constraints :
*
*                 (a) ONLY supports the following BSD layer functionality :
*                     (1) BSD sockets                                           See 'net_sock.h  Note #1'
*
*                 (b) Return variable 'errno' NOT currently supported #### NET-799
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
*                                               MODULE
*
* Note(s) : (1) BSD 4.x Layer module is required for :
*
*               (a) Network sockets
*               (b) Applications that require BSD 4.x application programming interface (API) :
*                   (1) Data Types
*                   (2) Macro's
*                   (3) Functions
*********************************************************************************************************
*********************************************************************************************************
*/


#ifndef  NET_BSD_MODULE_PRESENT
#define  NET_BSD_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#endif

#include  "net_sock.h"
#include  "net_util.h"
#include  "net_def.h"
#include  "net_ascii.h"
#include  <lib_def.h>

#ifdef   NET_SOCK_BSD_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MACROS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_BSD_ADDR_LEN_MAX                    NET_SOCK_BSD_ADDR_LEN_MAX
#define  NET_BSD_ADDR_IPv4_NBR_OCTETS_UNUSED     NET_SOCK_ADDR_IPv4_NBR_OCTETS_UNUSED

#define  NI_MAXHOST                              1025u
#define  NI_MAXSERV                                32u

                                                                /* ------------------ FILE DESC SETS ------------------ */
#define  FD_SETSIZE                NET_SOCK_NBR_SOCK            /* See Note #5a2A in "BSD 4.x SOCKET DATA TYPES" sect.  */
#define  FD_MIN                                 0               /* See Note #5a2B in "BSD 4.x SOCKET DATA TYPES" sect.  */
#define  FD_MAX                   (FD_SETSIZE - 1)
#define  FD_ARRAY_SIZE          (((FD_SETSIZE - 1) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)) + 1)


/*
*********************************************************************************************************
*                              BSD 4.x SOCKET FAMILY & PROTOCOL DEFINES
*
* Note(s) : (1) The following socket values MUST be pre-#define'd in 'net_def.h' PRIOR to 'net_cfg.h'
*               so that the developer can configure sockets for the correct socket family values (see
*               'net_def.h  BSD 4.x & NETWORK SOCKET LAYER DEFINES  Note #1' & 'net_cfg_net.h  NETWORK
*               SOCKET LAYER CONFIGURATION') :
*
*               (a) AF_INET
*               (b) PF_INET
*
*           (2) Ideally, AF_&&& constants SHOULD be defined as unsigned constants since AF_&&& constants
*               are used with the unsigned socket address family data type (see 'BSD 4.x SOCKET DATA TYPES
*               Note #2a1A').  However, since PF_&&& constants are typically defined to their equivalent
*               AF_&&& constants BUT PF_&&& constants are used with the signed socket protocol family data
*               types; AF_&&& constants are defined as signed constants.
*********************************************************************************************************
*/

#if 0                                                           /* See Note #1.                                         */
                                                                /* ------------ SOCK FAMILY/PROTOCOL TYPES ------------ */
                                                                /* See Note #2.                                         */
#ifdef   AF_INET
#undef   AF_INET
#endif
#define  AF_INET                                           2

#ifdef   PF_INET
#undef   PF_INET
#endif
#define  PF_INET                                     AF_INET

#endif


/*
*********************************************************************************************************
*                                   BSD 4.x SOCKET ADDRESS DEFINES
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
    #ifdef   INADDR_ANY
    #undef   INADDR_ANY
    #endif
    #define  INADDR_ANY                                      NET_IPv4_ADDR_NONE

    #ifdef   INADDR_LOOPBACK
    #undef   INADDR_LOOPBACK
    #endif
    #define  INADDR_LOOPBACK                                 NET_IPv4_ADDR_LOCAL_HOST_NET

    #ifdef   INADDR_BROADCAST
    #undef   INADDR_BROADCAST
    #endif
    #define  INADDR_BROADCAST                                NET_IPv4_ADDR_BROADCAST

    #define  INET_ADDRSTRLEN                                 16
#endif

#ifdef  NET_IPv6_MODULE_EN
    #ifdef   IN6ADDR_ANY_INIT
    #undef   IN6ADDR_ANY_INIT
    #endif
    #define  IN6ADDR_ANY_INIT                                { 0,   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

    #ifdef   IN6ADDR_LOOPBACK_INIT
    #undef   IN6ADDR_LOOPBACK_INIT
    #endif
    #define  IN6ADDR_LOOPBACK_INIT                           { 0,   0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }

    #ifdef   IN6ADDR_LINKLOCAL_ALLNODES_INIT
    #undef   IN6ADDR_LINKLOCAL_ALLNODES_INIT
    #endif
    #define  IN6ADDR_LINKLOCAL_ALLNODES_INIT                 { 0xFF,2,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }

    #ifdef   IN6ADDR_LINKLOCAL_ALLROUTERS_INIT
    #undef   IN6ADDR_LINKLOCAL_ALLROUTERS_INIT
    #endif
    #define  IN6ADDR_LINKLOCAL_ALLROUTERS_INIT               { 0xFF,2,0,0,0,0,0,0,0,0,0,0,0,0,0,2 }

    #define  INET6_ADDRSTRLEN                                46

    #ifdef   IN6ADDR_ANY
    #undef   IN6ADDR_ANY
    #endif
    #define  IN6ADDR_ANY                                     NET_IPv6_ADDR_ANY
#endif


/*
*********************************************************************************************************
*                                 BSD 4.x ADDRINFO & NAMEINFO DEFINES
*********************************************************************************************************
*/

#undef   EAI_ADDRFAMILY
#undef   EAI_AGAIN
#undef   EAI_BADFLAGS
#undef   EAI_FAIL
#undef   EAI_FAMILY
#undef   EAI_MEMORY
#undef   EAI_NODATA
#undef   EAI_NONAME
#undef   EAI_SERVICE
#undef   EAI_SOCKTYPE
#undef   EAI_SYSTEM
#undef   EAI_BADHINTS                                           /* Not Implemented.                                     */
#undef   EAI_PROTOCOL                                           /* Not Implemented.                                     */
#undef   EAI_MAX                                                /* Not Implemented.                                     */

                                                                /* ----- ERROR CODE DEFINITIONS FOR ADDRINFO. --------- */
#define  EAI_ADDRFAMILY           1                             /* Address family for node_name not supported.          */
#define  EAI_AGAIN                2                             /* Temporary failure in name resolution.                */
#define  EAI_BADFLAGS             3                             /* Invalid value for ai_flags.                          */
#define  EAI_FAIL                 4                             /* Non-recoverable failure in name resolution.          */
#define  EAI_FAMILY               5                             /* ai_family not supported.                             */
#define  EAI_MEMORY               6                             /* Memory allocation failure.                           */
#define  EAI_NONAME               7                             /* node_name or service_name not provided, or not known.*/
#define  EAI_OVERFLOW             8                             /* argument buffer overflow.                            */
#define  EAI_SERVICE              9                             /* service_name is not supported for ai_socktype.       */
#define  EAI_SOCKTYPE            10                             /* ai_socktype is not supported.                        */
#define  EAI_SYSTEM              11                             /* System error. See 'errno'.                           */

                                                                /* Flag values for getaddrinfo().                       */
#undef   AI_PASSIVE
#undef   AI_CANONNAME
#undef   AI_NUMERICHOST
#undef   AI_NUMERICSERV
#undef   AI_V4MAPPED
#undef   AI_ALL
#undef   AI_ADDRCONFIG
#undef   AI_DEFAULT

#define  AI_ADDRCONFIG            DEF_BIT_01
#define  AI_PASSIVE               DEF_BIT_02
#define  AI_CANONNAME             DEF_BIT_03
#define  AI_NUMERICHOST           DEF_BIT_04
#define  AI_NUMERICSERV           DEF_BIT_05
#define  AI_V4MAPPED              DEF_BIT_06
#define  AI_ALL                   DEF_BIT_07

#define  NI_NOFQDN                DEF_BIT_00
#define  NI_NUMERICHOST           DEF_BIT_01
#define  NI_NAMEREQD              DEF_BIT_02
#define  NI_NUMERICSERV           DEF_BIT_03
#define  NI_DGRAM                 DEF_BIT_04
#define  NET_BSD_NI_MAX_FLAG     (NI_NOFQDN | NI_NUMERICHOST | NI_NAMEREQD |NI_NUMERICSERV | NI_DGRAM)

/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      BSD 4.x SOCKET DATA TYPES
*
* Note(s) : (1) BSD 4.x data types are required only for applications that reference BSD 4.x data types.
*
*               See also 'MODULE  Note #1b2'.
*
*           (2) (a) (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/socket.h : DESCRIPTION' states that :
*
*                       (A) "sa_family_t ... shall define [as an] unsigned integer type."
*
*                       (B) "socklen_t ... is an integer type of width of at least 32 bits."
*
*                           (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/socket.h : APPLICATION USAGE'
*                               states that "it is recommended that applications not use values larger
*                               than (2^31 - 1) for the socklen_t type".
*
*                   (2) IEEE Std 1003.1, 2004 Edition, Section 'netinet/in.h : DESCRIPTION' states that :
*
*                       (A) "in_port_t - Equivalent to the type uint16_t."
*
*                       (B) "in_addr_t - Equivalent to the type uint32_t."
*
*               (b) (1) (A) IEEE Std 1003.1, 2004 Edition, Section 'sys/socket.h : DESCRIPTION' states
*                           that "the sockaddr structure ... includes at least the following members" :
*
*                           (1) sa_family_t    sa_family    Address family
*                           (2) char           sa_data[]    Socket address
*
*                       (B) (1) Socket address structure 'sa_family' member MUST be configured in host-
*                               order & MUST NOT be converted to/from network-order.
*
*                           (2) Socket address structure addresses MUST be configured/converted from host-
*                               order to network-order.
*
*                           See also 'net_sock.h  NETWORK SOCKET ADDRESS DATA TYPES  Note #2'.
*
*                   (2) IEEE Std 1003.1, 2004 Edition, Section 'netinet/in.h : DESCRIPTION' states that :
*
*                       (A) "The in_addr structure ... includes at least the following member" :
*
*                           (1) in_addr_t    s_addr
*
*                       (B) (1) "The sockaddr_in structure ... includes at least the following members" :
*
*                               (a) sa_family_t       sin_family    Address family (AF_INET)
*                               (b) in_port_t         sin_port      Port number
*                               (c) struct in_addr    sin_addr      IP   address
*
*                           (2) (a) "The sin_port and sin_addr members shall be in network byte order."
*
*                               (b) "The sin_zero member was removed from the sockaddr_in structure."
*
*                                   (1) However, this does NOT preclude the structure from including a
*                                       'sin_zero' member.
*
*           (3) IEEE Std 1003.1, 2004 Edition, Section 'sys/types.h : DESCRIPTION' states that :
*
*               (a) (1) (A) "size_t - Used for sizes of objects."
*
*                       (B) "size_t shall be an unsigned integer type."
*
*                       (C) To avoid possible namespace conflict with commonly-defined 'size_t' data type,
*                           '_size_t' data type is prefixed with a single underscore.
*
*                   (2) (A) "ssize_t - Used for a count of bytes or an error indication."
*
*                       (B) "ssize_t shall be [a] signed integer type ... capable of storing values at
*                            least in the range [-1, {SSIZE_MAX}]."
*
*                           (1) IEEE Std 1003.1, 2004 Edition, Section 'limits.h : DESCRIPTION' states
*                               that the "Minimum Acceptable Value ... [for] {SSIZE_MAX}" is "32767".
*
*               (b) (1) (A) "time_t - Used for time in seconds."
*
*                       (B) "time_t ... shall be integer or real-floating types."
*
*                       (C) To avoid possible namespace conflict with commonly-defined 'time_t' data type,
*                           '_time_t' data type is prefixed with a single underscore.
*
*                   (2) (A) "suseconds_t - Used for time in microseconds."
*
*                       (B) "suseconds_t shall be a signed integer type capable of storing values at least
*                            in the range [-1, 1000000]."
*
*               (c) "The implementation shall support ... size_t, ssize_t, suseconds_t ... widths ... no
*                    greater than the width of type long."
*
*           (4) (a) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states that "the
*                   'timeval' structure ... includes at least the following members" :
*
*                   (1) time_t         tv_sec     Seconds
*                   (2) suseconds_t    tv_usec    Microseconds
*
*               (b) Ideally, the BSD 4.x Layer's 'timeval' data type would be the basis for the Network
*                   Socket Layer's 'NET_SOCK_TIMEOUT' data type definition.  However, since the BSD 4.x
*                   Layer application programming interface (API) is NOT guaranteed to be present in the
*                   project build (see 'MODULE  Note #1bA'); the Network Socket Layer's 'NET_SOCK_TIMEOUT'
*                   data type MUST be independently defined.
*
*                   However, for correct interoperability between the BSD 4.x Layer 'timeval' data type
*                   & the Network Socket Layer's 'NET_SOCK_TIMEOUT' data type; ANY modification to either
*                   of these data types MUST be appropriately synchronized.
*
*                   See also 'net_sock.h  NETWORK SOCKET TIMEOUT DATA TYPE  Note #1b'.
*
*           (5) (a) (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states that
*                       the "'fd_set' type ... shall [be] define[d] ... as a structure".
*
*                       (A) "The requirement for the 'fd_set' structure to have a member 'fds_bits' has
*                            been removed."
*
*                           (1) However, this does NOT preclude the descriptor structure from including
*                               an 'fds_bits' member.
*
*                       (B) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                           6th Printing, Section 6.3, Pages 162-163 states that "descriptor sets [are]
*                           typically an array of integers, with each bit in each integer corresponding
*                           to a descriptor".
*
*                   (2) (A) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states
*                           that "FD_SETSIZE ... shall be defined as a macro ... [as the] maximum number
*                           of file descriptors in an 'fd_set' structure."
*
*                       (B) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition,
*                           6th Printing, Section 6.3, Page 163 states that "descriptors start at 0".
*
*               (b) Ideally, the BSD 4.x Layer's 'fd_set' data type would be the basis for the Network
*                   Socket Layer's 'NET_SOCK_DESC' data type definition.  However, since the BSD 4.x
*                   Layer application programming interface (API) is NOT guaranteed to be present in the
*                   project build (see 'MODULE  Note #1bA'); the Network Socket Layer's 'NET_SOCK_DESC'
*                   data type MUST be independently defined.
*
*                   However, for correct interoperability between the BSD 4.x Layer 'fd_set' data type
*                   & the Network Socket Layer's 'NET_SOCK_DESC' data type; ANY modification to either
*                   of these data types MUST be appropriately synchronized.
*
*               See also 'net_sock.h  NETWORK SOCKET (IDENTIFICATION) DESCRIPTOR SET DATA TYPE  Note #1'.
*********************************************************************************************************
*/


                                                                        /* ---------------- SOCK ADDR ----------------- */
typedef  CPU_INT16U       sa_family_t;                                  /* Sock addr family type (see Note #2a1A).      */

typedef  CPU_INT32S       socklen_t;                                    /* Sock addr len    type (see Note #2a1B).      */

typedef  CPU_INT16U       in_port_t;                                    /* Sock IP port nbr type (see Note #2a2A).      */
typedef  CPU_INT32U       in_addr_t;                                    /* Sock IPv4 addr   type (see Note #2a2B).      */
typedef  CPU_INT08U       in6_addr_t;                                   /* Sock IPv6 addr   type.                       */

struct  sockaddr {                                                      /* See Note #2b1.                               */
    sa_family_t           sa_family;                                    /* Sock family.                                 */
    CPU_CHAR              sa_data[NET_BSD_ADDR_LEN_MAX];                /* Sock addr.                                   */
};


struct  in_addr {                                                       /* See Note #2b2A.                              */
    in_addr_t             s_addr;
};


struct  sockaddr_in {                                                   /* See Note #2b2B.                              */
            sa_family_t   sin_family;                                   /* AF_INET family.                              */
            in_port_t     sin_port;                                     /* Port nbr              [see Note #2b2B2a].    */
    struct  in_addr       sin_addr;                                     /* IPv4   addr           [see Note #2b2B2a].    */
            CPU_CHAR      sin_zero[NET_BSD_ADDR_IPv4_NBR_OCTETS_UNUSED];/* Unused (MUST be zero) [see Note #2b2B2b].    */
};

#ifdef  NET_IPv6_MODULE_EN
struct  in6_addr {
    in6_addr_t             s_addr[16];
};


struct  sock_addr_in6 {
            sa_family_t   sin6_family;                                  /* AF_INET6   family.                           */
            in_port_t     sin6_port;                                    /* Port nbr.                                    */
            CPU_INT32U    sin6_flowinfo;                                /* Flow info.                                   */
    struct  in6_addr      sin6_addr;                                    /* IPv6   addr.                                 */
            CPU_INT32U    sin6_scope_id;                                /* Scope zone ix.                               */
};


extern  const  struct  in6_addr  in6addr_any;
extern  const  struct  in6_addr  in6addr_loopback;
extern  const  struct  in6_addr  in6addr_linklocal_allnodes;
extern  const  struct  in6_addr  in6addr_linklocal_allrouters;
#endif


                                                                        /* ----------- SOCK DATA/VAL SIZES ------------ */
typedef    unsigned  int  _size_t;                                      /* Sock app data buf size type (see Note #3a1). */
typedef      signed  int  ssize_t;                                      /* Sock rtn data/val size type (see Note #3a2). */

                                                                        /* -------------- SOCK TIME VALS -------------- */
typedef  CPU_INT32S       _time_t;                                      /* Signed time val in  sec (see Note #3b1).     */
typedef  CPU_INT32S       suseconds_t;                                  /* Signed time val in usec (see Note #3b2).     */



struct  timeval {                                                       /* See Note #4a.                                */
   _time_t                tv_sec;                                       /* Time val in  sec (see Note #4a1).            */
    suseconds_t           tv_usec;                                      /* Time val in usec (see Note #4a2).            */
};


struct  fd_set {                                                        /* See Note #5a1.                               */
    CPU_DATA              fds_bits[FD_ARRAY_SIZE];
};

struct  addrinfo {                                                      /* Structure and type definitions for addrinfo. */
            int           ai_flags;                                     /* Flags: AI_PASSIVE, AI_AI_NUMERICHOST, etc.   */
            int           ai_family;                                    /* Address family: AF_xxx.                      */
            int           ai_socktype;                                  /* Socket type: SOCK_xxx.                       */
            int           ai_protocol;                                  /* 0 or IPPROTO_xxx for IPv4 and IPv6.          */
            socklen_t     ai_addrlen;                                   /* Length of ai_addr.                           */
    struct  sockaddr     *ai_addr;                                      /* Pointer to socket address structure.         */
            char         *ai_canonname;                                 /* Poiter to canonical name for host.           */
    struct  addrinfo     *ai_next;                                      /* Pointer to next structure in linked list.    */
};

typedef  struct  addrinfo  addrinfo;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MACRO'S
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                       BSD 4.x NETWORK WORD ORDER - TO - CPU WORD ORDER MACRO'S
*
* Description : Convert data values to & from network word order to host CPU word order.
*
* Argument(s) : val       Data value to convert (see Note #2).
*
* Return(s)   : Converted data value (see Note #2).
*
* Caller(s)   : Application.
*
*               These macro's are network protocol suite application programming interface (API) macro's
*               & MAY be called by application function(s).
*
* Note(s)     : (1) BSD 4.x macro's are required only for applications that call BSD 4.x macro's.
*
*                   See also 'MODULE  Note #1b1'.
*
*               (2) 'val' data value to convert & any variable to receive the returned conversion MUST
*                   start on appropriate CPU word-aligned addresses.  This is required because most word-
*                   aligned processors are more efficient & may even REQUIRE that multi-octet words start
*                   on CPU word-aligned addresses.
*
*                   (a) For 16-bit word-aligned processors, this means that
*
*                           all 16- & 32-bit words MUST start on addresses that are multiples of 2 octets
*
*                   (b) For 32-bit word-aligned processors, this means that
*
*                           all 16-bit       words MUST start on addresses that are multiples of 2 octets
*                           all 32-bit       words MUST start on addresses that are multiples of 4 octets
*
*                   See also 'net_util.h  NETWORK WORD ORDER - TO - CPU WORD ORDER MACRO'S  Note #1'.
*********************************************************************************************************
*/

#define  ntohs(val)                      NET_UTIL_NET_TO_HOST_16(val)
#define  ntohl(val)                      NET_UTIL_NET_TO_HOST_32(val)

#define  htons(val)                      NET_UTIL_HOST_TO_NET_16(val)
#define  htonl(val)                      NET_UTIL_HOST_TO_NET_32(val)



/*
*********************************************************************************************************
*                                   BSD 4.x FILE DESCRIPTOR MACRO'S
*
* Description : Initialize, modify, & check file descriptor sets for multiplexed I/O functions.
*
* Argument(s) : fd          File descriptor number to initialize, modify, or check; when applicable.
*
*               fdsetp      Pointer to a file descriptor set.
*
* Return(s)   : Return values macro-dependent :
*
*                   none, for    file descriptor initialization & modification macro's (see Note #2a2A).
*
*                   1,    if any file descriptor condition(s) satisfied                (see Note #2a2B1).
*
*                   0,    otherwise                                                    (see Note #2a2B2).
*
* Caller(s)   : Application.
*
*               These macro's are network protocol suite application programming interface (API) macro's
*               & MAY be called by application function(s).
*
* Note(s)     : (1) BSD 4.x macro's are required only for applications that call BSD 4.x macro's.
*
*                   See also 'MODULE  Note #1b3'.
*
*               (2) (a) (1) (A) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states that
*                               "each of the following may be declared as a function, or defined as a macro, or
*                                both" :
*
*                               (1) void  FD_ZERO (fd_set *fdset);
*
*                                  "Initializes the file descriptor set 'fdset' to have zero bits for all file
*                                   descriptors."
*
*                               (2) void  FD_CLR  (int fd, fd_set *fdset);
*
*                                  "Clears the bit for the file descriptor 'fd' in the file descriptor set 'fdset'."
*
*                               (3) void  FD_SET  (int fd, fd_set *fdset);
*
*                                  "Sets   the bit for the file descriptor 'fd' in the file descriptor set 'fdset'."
*
*                               (4) int   FD_ISSET(int fd, fd_set *fdset);
*
*                                  "Returns a non-zero value if the bit for the file descriptor 'fd' is set in the
*                                   file descriptor set by 'fdset', and 0 otherwise."
*
*                           (B) IEEE Std 1003.1, 2004 Edition, Section 'select() : DESCRIPTION' reiterates that
*                               "file descriptor masks of type 'fd_set' can be initialized and tested with" :
*
*                               (1) "FD_ZERO(fdsetp) shall initialize the descriptor set pointed to by 'fdsetp'
*                                    to the null set.  No error is returned if the set is not empty at the time
*                                    FD_ZERO() is invoked."
*
*                               (2) "FD_CLR(fd, fdsetp) shall remove the file descriptor 'fd' from the set pointed
*                                    to by 'fdsetp'.  If 'fd' is not a member of this set, there shall be no effect
*                                    on the set, nor will an error be returned."
*
*                               (3) "FD_SET(fd, fdsetp) shall add the file descriptor 'fd' to the set pointed to by
*                                    'fdsetp'.  If the file descriptor 'fd' is already in this set, there shall be
*                                    no effect on the set, nor will an error be returned."
*
*                               (4) "FD_ISSET(fd, fdsetp) shall evaluate to non-zero if the file descriptor 'fd' is a
*                                    member of the set pointed to by 'fdsetp', and shall evaluate to zero otherwise."
*
*                       (2) IEEE Std 1003.1, 2004 Edition, Section 'select() : RETURN VALUE' states that :
*
*                           (A) The following macro's "do not return a value" :
*
*                               (1) "FD_CLR()," ...
*                               (2) "FD_SET()," ...
*                               (3) "FD_ZERO()."
*
*                           (B) "FD_ISSET() shall return" :
*
*                               (1) "a non-zero value if the bit for the file descriptor 'fd' is set in the file
*                                    descriptor set pointed to by 'fdset'," ...
*                               (2) "0 otherwise."
*
*                   (b) (1) IEEE Std 1003.1, 2004 Edition, Section 'sys/select.h : DESCRIPTION' states that :
*
*                           (A) "If implemented as macros, these may evaluate their arguments more than once, so
*                                applications should ensure that the arguments they supply are never expressions
*                                with side effects."
*
*                       (2) IEEE Std 1003.1, 2004 Edition, Section 'select() : DESCRIPTION' also states that :
*
*                           (A) "It is unspecified whether each of these is a macro or a function.  If a macro
*                                definition is suppressed in order to access an actual function, or a program
*                                defines an external identifier with any of these names, the behavior is undefined."
*
*                           (B) "The behavior of these macros is undefined" :
*
*                               (1) "if the 'fd' argument is" :
*                                   (a) "less than 0"                             ...
*                                   (b) "or greater than or equal to FD_SETSIZE," ...
*
*                               (2) "or if 'fd' is not a valid file descriptor,"  ...
*                               (3) "or if any of the arguments are expressions with side effects."
*
*                       (3) Stevens/Fenner/Rudoff, UNIX Network Programming, Volume 1, 3rd Edition, 6th Printing,
*                           Section 6.3, Page 163 adds that "it is important to initialize [a descriptor] set,
*                           since unpredictable results can occur if the set is allocated as an automatic variable
*                           and not initialized".
*
*                   See also 'BSD 4.x SOCKET DATA TYPES  Note #5b'.
*
*               (3) Ideally, the BSD 4.x Layer's file descriptor macro's ('FD_&&&()') would be the basis for
*                   the Network Socket Layer's socket descriptor macro ('NET_SOCK_DESC_&&&()') definitions.
*                   However, since the BSD 4.x Layer application programming interface (API) is NOT guaranteed
*                   to be present in the project build (see 'MODULE  Note #1bA'); the Network Socket Layer's
*                   socket descriptor macro's MUST be independently defined.
*
*                   However, for correct interoperability between the BSD 4.x Layer file descriptor macro's
*                   & the Network Socket Layer's socket descriptor macro's; ANY modification to any of these
*                   macro definitions MUST be appropriately synchronized.
*
*                   See also 'net_sock.h  NETWORK SOCKET DESCRIPTOR MACRO'S  Note #1'.
*********************************************************************************************************
*/


#define  FD_ZERO(fdsetp)    do {                                                                              \
                                if (((struct fd_set *)(fdsetp)) != (struct fd_set *)0) {                      \
                                    Mem_Clr ((void     *)      (&(((struct fd_set *)(fdsetp))->fds_bits[0])), \
                                             (CPU_SIZE_T) (sizeof(((struct fd_set *)(fdsetp))->fds_bits)));   \
                                }                                                                             \
                            } while (0)


#define  FD_CLR(fd, fdsetp)             do {                                                                                                              \
                                            if (((fd) >= FD_MIN) && ((fd) <= FD_MAX) &&                                                                   \
                                                                 (((struct fd_set *)(fdsetp)) != (struct fd_set *)0)) {                                   \
                                                  DEF_BIT_CLR   ((((struct fd_set *)(fdsetp))->fds_bits[(fd) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)]), \
                                                  DEF_BIT                                              ((fd) % (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS))); \
                                            }                                                                                                             \
                                        } while (0)

#define  FD_SET(fd, fdsetp)             do {                                                                                                              \
                                            if (((fd) >= FD_MIN) && ((fd) <= FD_MAX) &&                                                                   \
                                                                 (((struct fd_set *)(fdsetp)) != (struct fd_set *)0)) {                                   \
                                                  DEF_BIT_SET   ((((struct fd_set *)(fdsetp))->fds_bits[(fd) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)]), \
                                                  DEF_BIT                                              ((fd) % (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS))); \
                                            }                                                                                                             \
                                        } while (0)

#define  FD_ISSET(fd, fdsetp)                  ((((fd) >= FD_MIN) && ((fd) <= FD_MAX) &&                                                                  \
                                                                 (((struct fd_set *)(fdsetp)) != (struct fd_set *)0)) ?                                   \
                                               (((DEF_BIT_IS_SET((((struct fd_set *)(fdsetp))->fds_bits[(fd) / (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)]), \
                                                  DEF_BIT                                              ((fd) % (sizeof(CPU_DATA) * DEF_OCTET_NBR_BITS)))) \
                                               == DEF_YES) ? 1 : 0)                                                                                       \
                                                               : 0)


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
*                                 STANDARD BSD 4.x FUNCTION PROTOTYPES
*
* Note(s) : (1) BSD 4.x function prototypes are required only for applications that call BSD 4.x functions.
*
*               See also 'MODULE  Note #1b3'.
*********************************************************************************************************
*/


       void          NetBSD_Init (               NET_ERR    *p_err);


                                                                        /* ------------- SOCK ALLOC FNCTS ------------- */
       int           socket      (               int         protocol_family,
                                                 int         sock_type,
                                                 int         protocol);

       int           close       (               int         sock_id);

       int           shutdown    (               int         sock_id,
                                                 int         type);


                                                                        /* ------------- LOCAL CONN FCNTS ------------- */
       int           bind        (               int         sock_id,
                                        struct   sockaddr   *p_addr_local,
                                                 socklen_t   addr_len);


                                                                        /* ------------ CLIENT CONN FCNTS ------------- */
       int           connect     (               int         sock_id,
                                        struct   sockaddr   *p_addr_remote,
                                                 socklen_t   addr_len);


                                                                        /* ------------ SERVER CONN FCNTS ------------- */
       int           listen      (               int         sock_id,
                                                 int         sock_q_size);

       int           accept      (               int         sock_id,
                                        struct   sockaddr   *p_addr_remote,
                                                 socklen_t  *p_addr_len);


                                                                        /* ----------------- RX FNCTS ----------------- */
       ssize_t       recvfrom    (               int         sock_id,
                                                 void       *p_data_buf,
                                                _size_t      data_buf_len,
                                                 int         flags,
                                        struct   sockaddr   *p_addr_remote,
                                                 socklen_t  *p_addr_len);

       ssize_t       recv        (               int         sock_id,
                                                 void       *p_data_buf,
                                                _size_t      data_buf_len,
                                                 int         flags);


                                                                        /* ----------------- TX FNCTS ----------------- */
       ssize_t       sendto      (               int         sock_id,
                                                 void       *p_data,
                                                _size_t      data_len,
                                                 int         flags,
                                        struct   sockaddr   *p_addr_remote,
                                                 socklen_t   addr_len);

       ssize_t       send        (               int         sock_id,
                                                 void       *p_data,
                                                _size_t      data_len,
                                                 int         flags);


                                                                        /* ------------ MULTIPLEX I/O FNCTS ----------- */
       int           select      (               int         desc_nbr_max,
                                         struct  fd_set     *p_desc_rd,
                                         struct  fd_set     *p_desc_wr,
                                         struct  fd_set     *p_desc_err,
                                         struct  timeval    *p_timeout);


                                                                        /* ---------------- CONV FCNTS ---------------- */
       in_addr_t     inet_addr   (               char       *p_addr);

       int           getpeername (               int         sock_id,
                                         struct  sockaddr   *addr,
                                                 socklen_t  *addrlen);

       int           getsockname (               int         sock_id,
                                         struct  sockaddr   *addr,
                                                 socklen_t  *addrlen);

       int           getnameinfo (const  struct  sockaddr   *p_sockaddr,
                                                 int         addrlen,
                                                 char       *p_host_name,
                                                 int         hostlen,
                                                 char       *p_service_name,
                                                 int         servicelen,
                                                 int         flags);
  
const  char         *gai_strerror(               int         errcode);

       int           getaddrinfo (const          char       *p_node_name,
                                  const          char       *p_service_name,
                                  const  struct  addrinfo   *p_hints,
                                         struct  addrinfo  **pp_res);

       void          freeaddrinfo(       struct  addrinfo   *res);


#ifdef  NET_IPv4_MODULE_EN
       char         *inet_ntoa   (       struct  in_addr     addr);


       int           inet_aton   (               char       *p_addr_in,
                                         struct  in_addr    *p_addr);
#endif  /* NET_IPv4_MODULE_EN */

       int           inet_pton   (               int         af,
                                  const          char       *src,
                                                 void       *dst);

const  char         *inet_ntop   (               int         af,
                                  const          void       *src,
                                                 char       *dst,
                                                 socklen_t   size);

                                                                        /* ------------ GET/SET SOCK OPTS ------------- */

       int           setsockopt  (               int         sock_id,
                                                 int         protocol,
                                                 int         opt_name,
                                                 void       *opt_val,
                                                 socklen_t   opt_len);

       int           getsockopt  (               int         sock_id,
                                                 int         protocol,
                                                 int         opt_name,
                                                 void       *p_opt_val,
                                                 socklen_t  *p_opt_len);


                                                                        /* ---------- GET/SET HOSTNAME FCNTS ---------- */
       int           gethostname (               char       *host_name,
                                                _size_t      name_len);

       int           sethostname (const          char       *host_name,
                                                _size_t      name_len);


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
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_BSD_MODULE_PRESENT */

#endif /* NET_SOCK_BSD_EN */
