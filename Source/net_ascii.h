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
*                                         NETWORK ASCII LIBRARY
*
* Filename : net_ascii.h
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

#ifndef  NET_ASCII_MODULE_PRESENT
#define  NET_ASCII_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  <cpu.h>
#include  "net_type.h"
#include  "net_err.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  NET_ASCII_CHAR_LEN_DOT                            1u
#define  NET_ASCII_CHAR_LEN_HYPHEN                         1u
#define  NET_ASCII_CHAR_LEN_COLON                          1u
#define  NET_ASCII_CHAR_LEN_NUL                            1u


#define  NET_ASCII_CHAR_MIN_OCTET                          1u

#define  NET_ASCII_CHAR_MAX_OCTET_08                     DEF_INT_08U_NBR_DIG_MAX
#define  NET_ASCII_CHAR_MAX_OCTET_16                     DEF_INT_16U_NBR_DIG_MAX
#define  NET_ASCII_CHAR_MAX_OCTET_32                     DEF_INT_32U_NBR_DIG_MAX

#define  NET_ASCII_STR_LOCAL_HOST                        "localhost"


/*
*********************************************************************************************************
*                                    NETWORK ASCII ADDRESS DEFINES
*
* Note(s) : (1) ONLY supports 48-bit Ethernet MAC addresses.
*********************************************************************************************************
*/

#define  NET_ASCII_NBR_OCTET_ADDR_MAC                      6u   /* See Note #1.                                         */
#define  NET_ASCII_CHAR_MAX_OCTET_ADDR_MAC                 2u

#define  NET_ASCII_LEN_MAX_ADDR_MAC                    ((NET_ASCII_NBR_OCTET_ADDR_MAC       * NET_ASCII_CHAR_MAX_OCTET_ADDR_MAC) + \
                                                       ((NET_ASCII_NBR_OCTET_ADDR_MAC - 1u) * NET_ASCII_CHAR_LEN_HYPHEN        ) + \
                                                         NET_ASCII_CHAR_LEN_NUL                                                )


                                                                /* IPv4 addresses.                                      */
#define  NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv4                3u
#define  NET_ASCII_VAL_MAX_OCTET_ADDR_IPv4               255u

#define  NET_ASCII_NBR_OCTET_ADDR_IPv4           (sizeof(NET_IPv4_ADDR))


#define  NET_ASCII_LEN_MAX_ADDR_IPv4                   ((NET_ASCII_NBR_OCTET_ADDR_IPv4      * NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv4) + \
                                                       ((NET_ASCII_NBR_OCTET_ADDR_IPv4 - 1) * NET_ASCII_CHAR_LEN_DOT            ) + \
                                                         NET_ASCII_CHAR_LEN_NUL                                                 )

#define  NET_ASCII_NBR_MAX_DEC_PARTS                       4u
#define  NET_ASCII_CHAR_MAX_PART_ADDR_IP                 DEF_INT_32U_NBR_DIG_MAX
#define  NET_ASCII_VAL_MAX_PART_ADDR_IP                  DEF_INT_32U_MAX_VAL
#define  NET_ASCII_NBR_MAX_DOT_ADDR_IP                     3u

#define  NET_ASCII_MASK_32_01_BIT                      0xFFFFFFFFu
#define  NET_ASCII_MASK_24_01_BIT                      0x00FFFFFFu
#define  NET_ASCII_MASK_16_01_BIT                      0x0000FFFFu
#define  NET_ASCII_MASK_08_01_BIT                      0x000000FFu
#define  NET_ASCII_MASK_16_09_BIT                      0x0000FF00u
#define  NET_ASCII_MASK_24_17_BIT                      0x00FF0000u


                                                                /* IPv6 addresses.                                      */
#define  NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv6                2u
#define  NET_ASCII_CHAR_MAX_DIG_ADDR_IPv6               (NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv6 * 2)
#define  NET_ASCII_CHAR_MIN_COLON_IPv6                     2u
#define  NET_ASCII_CHAR_MAX_COLON_IPv6                     7u
#define  NET_ASCII_VAL_MAX_OCTET_ADDR_IPv6               255u

#define  NET_ASCII_NBR_OCTET_ADDR_IPv6           (sizeof(NET_IPv6_ADDR))


#define  NET_ASCII_LEN_MAX_ADDR_IPv6                   (((NET_ASCII_CHAR_MAX_DIG_ADDR_IPv6 * 4)      * NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv6) + \
                                                       (((NET_ASCII_CHAR_MAX_DIG_ADDR_IPv6 * 2) - 1) * NET_ASCII_CHAR_LEN_COLON          ) + \
                                                          NET_ASCII_CHAR_LEN_NUL                                                         )

#ifdef   NET_IP_MODULE_EN

#ifdef   NET_IPv4_MODULE_EN
#undef   NET_ASCII_LEN_MAX_ADDR_IP
#define  NET_ASCII_LEN_MAX_ADDR_IP                        NET_ASCII_LEN_MAX_ADDR_IPv4
#endif

#ifdef   NET_IPv6_MODULE_EN
#undef   NET_ASCII_LEN_MAX_ADDR_IP
#define  NET_ASCII_LEN_MAX_ADDR_IP                        NET_ASCII_LEN_MAX_ADDR_IPv6
#endif

#define  NET_ASCII_LEN_MAX_PORT                           DEF_INT_16U_NBR_DIG_MAX


#endif


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

void                NetASCII_Str_to_MAC         (CPU_CHAR       *p_addr_mac_ascii,
                                                 CPU_INT08U     *p_addr_mac,
                                                 NET_ERR        *p_err);

void                NetASCII_MAC_to_Str         (CPU_INT08U     *p_addr_mac,
                                                 CPU_CHAR       *p_addr_mac_ascii,
                                                 CPU_BOOLEAN     hex_lower_case,
                                                 CPU_BOOLEAN     hex_colon_sep,
                                                 NET_ERR        *p_err);

NET_IP_ADDR_FAMILY  NetASCII_Str_to_IP          (CPU_CHAR       *p_addr_ip_ascii,
                                                 void           *p_addr,
                                                 CPU_INT08U      addr_max_len,
                                                 NET_ERR        *p_err);

NET_IPv4_ADDR       NetASCII_Str_to_IPv4        (CPU_CHAR       *p_addr_ip_ascii,
                                                 NET_ERR        *p_err);

NET_IPv6_ADDR       NetASCII_Str_to_IPv6        (CPU_CHAR       *p_addr_ip_ascii,
                                                 NET_ERR        *p_err);

void                NetASCII_IPv4_to_Str        (NET_IPv4_ADDR   addr_ip,
                                                 CPU_CHAR       *p_addr_ip_ascii,
                                                 CPU_BOOLEAN     lead_zeros,
                                                 NET_ERR        *p_err);

NET_IPv4_ADDR       NetASCII_Str_to_IPv4_Handler(CPU_CHAR       *p_addr_ip_ascii,
                                                 CPU_INT08U     *p_dot_nbr,
                                                 NET_ERR        *p_err);

void                NetASCII_IPv6_to_Str        (NET_IPv6_ADDR  *p_addr_ip,
                                                 CPU_CHAR       *p_addr_ip_ascii,
                                                 CPU_BOOLEAN     hex_lower_case,
                                                 CPU_BOOLEAN     lead_zeros,
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
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_ASCII_MODULE_PRESENT */
