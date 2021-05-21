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
*                                        NETWORK SHELL COMMAND
*
* Filename : net_cmd.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) uC/TCP-IP V3.01.00
*                (b) uC/OS-II  V2.90.00 or
*                    uC/OS-III V3.03.01
*                (c) uC/Shell  V1.04.00
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_CMD_MODULE_PRESENT
#define  NET_CMD_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*
* Note(s) : (1) The following common software files are located in the following directories :
*
*               (a) \<Custom Library Directory>\lib*.*
*
*               (b) (1) \<CPU-Compiler Directory>\cpu_def.h
*
*                   (2) \<CPU-Compiler Directory>\<cpu>\<compiler>\cpu*.*
*
*                   where
*                   <Custom Library Directory>      directory path for custom   library      software
*                   <CPU-Compiler Directory>        directory path for common   CPU-compiler software
*                   <cpu>                           directory name for specific processor (CPU)
*                   <compiler>                      directory name for specific compiler
*
*           (2)
*
*           (3) NO compiler-supplied standard library functions SHOULD be used.
*********************************************************************************************************
*/

#include  "Source/net_cfg_net.h"
#include  "../Source/net_type.h"
#include  "net_cmd_output.h"
#include  "../IF/net_if_802x.h"
#include  "../IF/net_if_wifi.h"

#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#include  "../IP/IPv6/net_ndp.h"
#endif

#include  <cpu.h>
#include  <Source/shell.h>

#ifndef  NET_CMD_TRACE
#include <stdio.h>
#endif


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  NET_CMD_STR_USER_MAX_LEN          256
#define  NET_CMD_STR_PASSWORD_MAX_LEN      256


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

typedef  enum net_cmd_type {
    NET_CMD_TYPE_HELP,
    NET_CMD_TYPE_IF_CONFIG,
    NET_CMD_TYPE_IF_RESET,
    NET_CMD_TYPE_ROUTE_ADD,
    NET_CMD_TYPE_ROUTE_REMOVE,
    NET_CMD_TYPE_PING,
    NET_CMD_TYPE_IP_SETUP,
    NET_CMD_TYPE_IPV6_AUTOCFG,
    NET_CMD_TYPE_NDP_CLR_CACHE,
    NET_CMD_TYPE_NDP_CACHE_STATE,
    NET_CMD_TYPE_NDP_CACHE_IS_ROUTER,
} NET_CMD_TYPE;

typedef enum net_cmd_err {
    NET_CMD_ERR_NONE                       =   0,              /* No errors.                                            */
    NET_CMD_ERR_FAULT                      =   1,
    NET_CMD_ERR_LINK_DOWN                  =   2,


    NET_CMD_ERR_TCPIP_INIT                 =  10,
    NET_CMD_ERR_TCPIP_IF_ADD               =  11,
    NET_CMD_ERR_TCPIP_IF_START             =  12,
    NET_CMD_ERR_TCPIP_MAC_CFG              =  13,
    NET_CMD_ERR_TCPIP_MISSING_MODULE       =  14,


    NET_CMD_ERR_SHELL_INIT                 =  20,              /* Command table not added to uC-Shell.                    */

    NET_CMD_ERR_CMD_ARG_INVALID            =  30,


    NET_CMD_ERR_IF_INVALID                 =  40,

    NET_CMD_ERR_IPv4_ADDR_CFGD             =  50,
    NET_CMD_ERR_IPv4_ADDR_TBL_FULL         =  51,
    NET_CMD_ERR_IPv4_ADDR_TBL_EMPTY        =  52,
    NET_CMD_ERR_IPv4_ADDR_INVALID          =  53,
    NET_CMD_ERR_IPv4_ADDR_NOT_FOUND        =  54,
    NET_CMD_ERR_IPv4_ADDR_IN_USE           =  55,

    NET_CMD_ERR_IPv6_ADDR_CFGD             =  60,
    NET_CMD_ERR_IPv6_ADDR_TBL_FULL         =  61,
    NET_CMD_ERR_IPv6_ADDR_TBL_EMPTY        =  62,
    NET_CMD_ERR_IPv6_ADDR_INVALID          =  63,
    NET_CMD_ERR_IPv6_ADDR_NOT_FOUND        =  64,


    NET_CMD_ERR_PARSER_ARG_NOT_EN          =  70,
    NET_CMD_ERR_PARSER_ARG_INVALID         =  71,
    NET_CMD_ERR_PARSER_ARG_VALUE_INVALID   =  72,

    NET_CMD_ERR_RX_Q_EMPTY                 =  80,
    NET_CMD_ERR_RX_Q_SIGNAL_FAULT          =  81,

    NET_CMD_ERR_MEM_POOL_CREATE            =  90,

} NET_CMD_ERR;


typedef  struct  net_cmd_ipv4_ascii_cfg {
    CPU_CHAR           *HostPtr;
    CPU_CHAR           *MaskPtr;
    CPU_CHAR           *GatewayPtr;
} NET_CMD_IPv4_ASCII_CFG;

typedef  struct  net_cmd_ipv4_cfg {
    NET_IPv4_ADDR       Host;
    NET_IPv4_ADDR       Mask;
    NET_IPv4_ADDR       Gateway;
} NET_CMD_IPv4_CFG;

typedef  struct  net_cmd_ipv6_ascii_cfg {
    CPU_CHAR           *HostPtr;
    CPU_CHAR           *PrefixLenPtr;
} NET_CMD_IPv6_ASCII_CFG;

typedef  struct  net_cmd_ipv6_cfg {
    NET_IPv6_ADDR       Host;
    CPU_INT08U          PrefixLen;
} NET_CMD_IPv6_CFG;

typedef  struct  net_cmd_mac_arg {
    CPU_INT08U          MAC_Addr[NET_IF_802x_HW_ADDR_LEN];
} NET_CMD_MAC_CFG;


typedef  struct  net_cmd_credential_ascii_cfg {
    CPU_CHAR           *User_Ptr;
    CPU_CHAR           *Password_Ptr;
} NET_CMD_CREDENTIAL_ASCII_CFG;

typedef  struct  net_cmd_credential {
    CPU_CHAR            User[NET_CMD_STR_USER_MAX_LEN];
    CPU_CHAR            Password[NET_CMD_STR_PASSWORD_MAX_LEN];
} NET_CMD_CREDENTIAL_CFG;

typedef  struct net_cmd_ping_arg {
    NET_IF_NBR          IF_Nbr;
    NET_IP_ADDR_FAMILY  family;
    CPU_INT08U          Addr[16];
    CPU_INT16U          DataLen;
    CPU_INT32U          Cnt;
#if 0
    CPU_BOOLEAN         Background;
#endif
} NET_CMD_PING_ARG;


typedef  struct net_cmd_ping_cmd_arg {
    CPU_CHAR           *IF_NbrPtr;
    CPU_CHAR           *AddrPtr;
    CPU_CHAR           *DataLenPtr;
    CPU_CHAR           *CntPtr;
} NET_CMD_PING_CMD_ARG;
#ifdef  NET_IF_WIFI_MODULE_EN
typedef  struct net_cmd_wifi_scan_arg {
    NET_IF_NBR          IF_Nbr;
    NET_IF_WIFI_SSID    SSID;
    NET_IF_WIFI_CH      Ch;
} NET_CMD_WIFI_SCAN_ARG;

typedef  struct net_cmd_wifi_join_arg {
    NET_IF_NBR                   IF_Nbr;
    NET_IF_WIFI_SSID             SSID;
    NET_IF_WIFI_PSK              PSK;
    NET_IF_WIFI_NET_TYPE         NetType;                               /* Wifi AP net type.                            */
    NET_IF_WIFI_SECURITY_TYPE    SecurityType;                          /* WiFi AP security type.                       */
} NET_CMD_WIFI_JOIN_ARG;

typedef  struct net_cmd_wifi_create_arg {
    NET_IF_NBR                   IF_Nbr;
    NET_IF_WIFI_SSID             SSID;
    NET_IF_WIFI_PSK              PSK;
    NET_IF_WIFI_CH               Ch;
    NET_IF_WIFI_NET_TYPE         NetType;                               /* Wifi AP net type.                            */
    NET_IF_WIFI_SECURITY_TYPE    SecurityType;                          /* WiFi AP security type.                       */
} NET_CMD_WIFI_CREATE_ARG;

#endif

typedef  struct  net_cmd_args {
    CPU_INT08U              WindowsIF_Nbr;

    CPU_BOOLEAN             IPv4_CfgEn;
    NET_CMD_IPv4_CFG        IPv4;

    CPU_BOOLEAN             IPv6_CfgEn;
    NET_CMD_IPv6_CFG        IPv6;

    CPU_BOOLEAN             MAC_CfgEn;
    NET_CMD_MAC_CFG         MAC_Addr;

    CPU_BOOLEAN             Credential_CfgEn;
    NET_CMD_CREDENTIAL_CFG  Credential;

    CPU_BOOLEAN             Telnet_Reqd;
} NET_CMD_ARGS;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void                  NetCmd_Init                 (NET_CMD_ERR           *p_err);

void                  NetCmd_Start                (NET_TASK_CFG          *p_rx_task_cfg,
                                                   NET_TASK_CFG          *p_tx_task_cfg,
                                                   NET_TASK_CFG          *p_tmr_task_cfg,
                                                   void                  *p_if_api,
                                                   void                  *p_dev_api,
                                                   void                  *p_dev_bsp,
                                                   void                  *p_dev_cfg,
                                                   void                  *p_ext_api,
                                                   void                  *p_ext_cfg,
                                                   NET_CMD_ARGS          *p_cmd_args,
                                                   NET_CMD_ERR           *p_err);


CPU_INT16S            NetCmd_Help                 (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_IF_Config            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_IF_Reset             (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_IF_SetMTU            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_IF_RouteAdd          (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_IF_RouteRemove       (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

void                  NetCmd_IF_IPv4AddrCfgStatic (NET_IF_NBR             if_id,
                                                   NET_CMD_IPv4_CFG      *p_ip_cfg,
                                                   NET_CMD_ERR           *p_err);

void                  NetCmd_IF_IPv6AddrCfgStatic (NET_IF_NBR             if_id,
                                                   NET_CMD_IPv6_CFG      *p_ip_cfg,
                                                   NET_CMD_ERR           *p_err);

void                  NetCmd_IF_IPv4AddrRemove    (NET_IF_NBR              if_id,
                                                   NET_CMD_IPv4_CFG      *p_ip_cfg,
                                                   NET_CMD_ERR           *p_err);

void                  NetCmd_IF_IPv6AddrRemove    (NET_IF_NBR              if_id,
                                                   NET_CMD_IPv6_CFG      *p_ip_cfg,
                                                   NET_CMD_ERR           *p_err);

CPU_INT16S            NetCmd_Ping                 (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

#ifdef  NET_IPv4_MODULE_EN
CPU_INT16S            NetCmd_IP_Config            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);
#endif

CPU_INT16S            NetCmd_IF_Start             (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_IF_Stop              (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);
#ifdef  NET_IF_WIFI_MODULE_EN
CPU_INT16S            NetCmd_WiFi_Scan            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_WiFi_Join            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_WiFi_Create          (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_WiFi_Leave           (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_WiFi_GetPeerInfo     (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_WiFi_Status          (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);
#endif


CPU_INT16S            NetCmd_Sock_Open            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Close           (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Bind            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Listen          (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Accept          (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Conn            (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Rx              (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

CPU_INT16S            NetCmd_Sock_Tx              (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

#if 0
CPU_INT16S            NetCmd_Sock_Sel             (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);
#endif

CPU_INT16S            NetCmd_SockOptSetChild      (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   SHELL_OUT_FNCT         out_fnct,
                                                   SHELL_CMD_PARAM       *p_cmd_param);

NET_CMD_PING_CMD_ARG  NetCmd_PingCmdArgParse      (CPU_INT16U             argc,
                                                   CPU_CHAR              *p_argv[],
                                                   NET_CMD_ERR           *p_err);

NET_CMD_PING_ARG      NetCmd_PingCmdArgTranslate  (NET_CMD_PING_CMD_ARG  *p_cmd_args,
                                                   NET_CMD_ERR           *p_err);


/*
*********************************************************************************************************
*                                     TRACE / DEBUG CONFIGURATION
*********************************************************************************************************
*/

#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                       0
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO                      1
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                       2
#endif

#ifndef  NET_CMD_TRACE_LEVEL
#define  NET_CMD_TRACE_LEVEL             TRACE_LEVEL_OFF
#endif


#ifndef  NET_CMD_TRACE
#define  NET_CMD_TRACE                   printf
#endif

#define  NET_CMD_TRACE_INFO(x)           ((NET_CMD_TRACE_LEVEL >= TRACE_LEVEL_INFO)  ? (void)(NET_CMD_TRACE x) : (void)0)
#define  NET_CMD_TRACE_DBG(x)            ((NET_CMD_TRACE_LEVEL >= TRACE_LEVEL_DBG)   ? (void)(NET_CMD_TRACE x) : (void)0)

/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
