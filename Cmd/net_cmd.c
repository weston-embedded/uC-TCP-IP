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
* Filename : net_cmd.c
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
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE
#define  NET_CMD_MODULE


#include  "net_cmd.h"
#include  "../Source/net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#include  "../IP/IPv6/net_ndp.h"
#endif


#include  "net_cmd_args_parser.h"

#include  "../Source/net_sock.h"
#include  "../Source/net_app.h"
#include  "../Source/net_ascii.h"
#include  "../Source/net_icmp.h"
#include  "../Source/net_err.h"
#include  "../Source/net.h"


#include  "../IF/net_if.h"
#include  "../IF/net_if_ether.h"
#include  "../IF/net_if_wifi.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  NET_CMD_ARG_BEGIN                  ASCII_CHAR_HYPHEN_MINUS


#define  NET_CMD_ARG_IPv4                   ASCII_CHAR_DIGIT_FOUR
#define  NET_CMD_ARG_IPv6                   ASCII_CHAR_DIGIT_SIX

#define  NET_CMD_ARG_IF                     ASCII_CHAR_LATIN_LOWER_I
#define  NET_CMD_ARG_SOCK_ID                ASCII_CHAR_LATIN_LOWER_I
#define  NET_CMD_ARG_LEN                    ASCII_CHAR_LATIN_LOWER_L
#define  NET_CMD_ARG_CNT                    ASCII_CHAR_LATIN_LOWER_C
#define  NET_CMD_ARG_HW_ADDR                ASCII_CHAR_LATIN_LOWER_H
#define  NET_CMD_ARG_ADDR                   ASCII_CHAR_LATIN_LOWER_A
#define  NET_CMD_ARG_MASK                   ASCII_CHAR_LATIN_LOWER_M
#define  NET_CMD_ARG_FMT                    ASCII_CHAR_LATIN_LOWER_F
#define  NET_CMD_ARG_DATA                   ASCII_CHAR_LATIN_LOWER_D
#define  NET_CMD_ARG_VAL                    ASCII_CHAR_LATIN_LOWER_V

#define  NET_CMD_ARG_WIFI_CHANNEL           ASCII_CHAR_LATIN_LOWER_C
#define  NET_CMD_ARG_WIFI_SSID              ASCII_CHAR_LATIN_LOWER_S
#define  NET_CMD_ARG_WIFI_PSK               ASCII_CHAR_LATIN_LOWER_P
#define  NET_CMD_ARG_WIFI_NET_TYPE          ASCII_CHAR_LATIN_LOWER_T
#define  NET_CMD_ARG_WIFI_SECURITY_TYPE     ASCII_CHAR_LATIN_LOWER_S


#define  NET_CMD_ARG_MTU                    ASCII_CHAR_LATIN_UPPER_M
#define  NET_CMD_ARG_SOCK_FAMILY            ASCII_CHAR_LATIN_LOWER_F
#define  NET_CMD_ARG_SOCK_TYPE              ASCII_CHAR_LATIN_LOWER_T
#define  NET_CMD_ARG_SOCK_PORT              ASCII_CHAR_LATIN_LOWER_P
#define  NET_CMD_ARG_SOCK_Q_SIZE            ASCII_CHAR_LATIN_LOWER_Q

#define  NET_CMD_ARG_SOCK_SEL_RD            ASCII_CHAR_LATIN_LOWER_R
#define  NET_CMD_ARG_SOCK_SEL_WR            ASCII_CHAR_LATIN_LOWER_W
#define  NET_CMD_ARG_SOCK_SEL_ERR           ASCII_CHAR_LATIN_LOWER_E
#define  NET_CMD_ARG_SOCK_SEL_TIMEOUT       ASCII_CHAR_LATIN_LOWER_T




/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

typedef struct net_cmd_reset_cmd_arg {
    CPU_CHAR     *IF_NbrPtr;
    CPU_BOOLEAN   IPv4_En;
    CPU_BOOLEAN   IPv6_En;
} NET_CMD_RESET_CMD_ARG;

typedef  struct net_cmd_reset_arg {
    NET_IF_NBR         IF_Nbr;
    CPU_BOOLEAN        IPv4_En;
    CPU_BOOLEAN        IPv6_En;
} NET_CMD_RESET_ARG;


typedef  struct net_cmd_route_cmd_arg {
    CPU_CHAR                 *IF_NbrPtr;
    NET_CMD_IPv4_ASCII_CFG    IPv4;
    NET_CMD_IPv6_ASCII_CFG    IPv6;
} NET_CMD_ROUTE_CMD_ARG;

typedef  struct net_cmd_route_arg {
    NET_IF_NBR         IF_Nbr;
    CPU_BOOLEAN        IPv4_En;
    NET_CMD_IPv4_CFG   IPv4Cfg;
    CPU_BOOLEAN        IPv6_En;
    NET_CMD_IPv6_CFG   IPv6Cfg;
} NET_CMD_ROUTE_ARG;



typedef struct net_cmd_mtu_cmd_arg {
    CPU_CHAR  *IF_NbrPtr;
    CPU_CHAR  *MTU_Ptr;
} NET_CMD_MTU_CMD_ARG;

typedef struct net_cmd_mtu_arg {
    NET_IF_NBR  IF_Nbr;
    CPU_INT16U  MTU;
} NET_CMD_MTU_ARG;



typedef  struct  net_cmd_sock_open_cmd_arg {
    CPU_CHAR                 *FamilyPtr;
    CPU_CHAR                 *TypePtr;
} NET_CMD_SOCK_OPEN_CMD_ARG;

typedef  struct  net_cmd_sock_open_arg {
    NET_SOCK_PROTOCOL_FAMILY  Family;
    NET_SOCK_TYPE             Type;
} NET_CMD_SOCK_OPEN_ARG;



typedef  struct  net_cmd_sock_id_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
} NET_CMD_SOCK_ID_CMD_ARG;

typedef  struct  net_cmd_sock_id_arg {
    NET_SOCK_ID              SockID;
} NET_CMD_SOCK_ID_ARG;



typedef  struct  net_cmd_sock_bind_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
    CPU_CHAR                 *PortPtr;
    CPU_CHAR                 *FamilyPtr;
} NET_CMD_SOCK_BIND_CMD_ARG;


typedef  struct  net_cmd_sock_bind_arg {
    NET_SOCK_ID               SockID;
    NET_PORT_NBR              Port;
    NET_SOCK_PROTOCOL_FAMILY  Family;
} NET_CMD_SOCK_BIND_ARG;



typedef  struct  net_cmd_sock_listen_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
    CPU_CHAR                 *QueueSizePtr;
} NET_CMD_SOCK_LISTEN_CMD_ARG;

typedef  struct  net_cmd_sock_listen_arg {
    NET_SOCK_ID               SockID;
    CPU_INT16U                QueueSize;
} NET_CMD_SOCK_LISTEN_ARG;



typedef  struct  net_cmd_sock_conn_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
    CPU_CHAR                 *AddrPtr;
    CPU_CHAR                 *PortPtr;
} NET_CMD_SOCK_CONN_CMD_ARG;

typedef  struct  net_cmd_sock_conn_arg {
    NET_SOCK_ID               SockID;
    NET_PORT_NBR              Port;
    CPU_INT08U                Addr[NET_IP_MAX_ADDR_SIZE];
    CPU_INT08U                AddrLen;
} NET_CMD_SOCK_CONN_ARG;



typedef  struct  net_cmd_sock_rx_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
    CPU_CHAR                 *DataLenPtr;
    CPU_CHAR                 *OutputFmtPtr;
} NET_CMD_SOCK_RX_CMD_ARG;

typedef  struct  net_cmd_sock_rx_arg {
    NET_SOCK_ID               SockID;
    CPU_INT16U                DataLen;
    NET_CMD_OUTPUT_FMT        OutputFmt;
} NET_CMD_SOCK_RX_ARG;



typedef  struct  net_cmd_sock_tx_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
    CPU_CHAR                 *DataLenPtr;
    CPU_CHAR                 *DataPtr;
} NET_CMD_SOCK_TX_CMD_ARG;

typedef  struct  net_cmd_sock_tx_arg {
    NET_SOCK_ID               SockID;
    CPU_INT16U                DataLen;
    CPU_INT08U               *DataPtr;
} NET_CMD_SOCK_TX_ARG;



typedef  struct  net_cmd_sock_sel_cmd_arg {
    CPU_CHAR                 *RdListPtr;
    CPU_CHAR                 *WrListPtr;
    CPU_CHAR                 *ErrListPtr;
    CPU_CHAR                 *Timeout_sec_Ptr;
} NET_CMD_SOCK_SEL_CMD_ARG;

typedef  struct  net_cmd_sock_sel_arg {
    CPU_INT16S                SockNbrMax;
    NET_SOCK_DESC             DescRd;
    NET_SOCK_DESC             DescWr;
    NET_SOCK_DESC             DescErr;
    NET_SOCK_TIMEOUT          Timeout;
} NET_CMD_SOCK_SEL_ARG;


typedef  struct  net_cmd_sock_opt_cmd_arg {
    CPU_CHAR                 *SockIDPtr;
    CPU_CHAR                 *ValPtr;
} NET_CMD_SOCK_OPT_CMD_ARG;

typedef  struct  net_cmd_sock_opt_arg {
    NET_SOCK_ID               SockID;
    CPU_INT32U                Value;
} NET_CMD_SOCK_OPT_ARG;


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

static  SHELL_CMD NetCmdTbl[] =
{
    {"net_help",         NetCmd_Help},
    {"net_ifconfig",     NetCmd_IF_Config},
    {"net_if_reset",     NetCmd_IF_Reset},
    {"net_if_set_mtu",   NetCmd_IF_SetMTU},
    {"net_route_add",    NetCmd_IF_RouteAdd},
    {"net_route_remove", NetCmd_IF_RouteRemove},
    {"net_ping",         NetCmd_Ping},
    {"net_if_start",     NetCmd_IF_Start},
    {"net_if_stop",      NetCmd_IF_Stop},

#ifdef  NET_IPv4_MODULE_EN
    {"net_ip_setup",     NetCmd_IP_Config},
#endif

#ifdef  NET_IF_WIFI_MODULE_EN
    {"net_wifi_scan",    NetCmd_WiFi_Scan},
    {"net_wifi_join",    NetCmd_WiFi_Join},
    {"net_wifi_create",  NetCmd_WiFi_Create},
    {"net_wifi_leave",   NetCmd_WiFi_Leave},
    {"net_wifi_peer",    NetCmd_WiFi_GetPeerInfo},
#endif

    {"net_sock_open",          NetCmd_Sock_Open},
    {"net_sock_close",         NetCmd_Sock_Close},
    {"net_sock_bind",          NetCmd_Sock_Bind},
    {"net_sock_listen",        NetCmd_Sock_Listen},
    {"net_sock_accept",        NetCmd_Sock_Accept},
    {"net_sock_connect",       NetCmd_Sock_Conn},
    {"net_sock_rx",            NetCmd_Sock_Rx},
    {"net_sock_tx",            NetCmd_Sock_Tx},
    /*{"net_sock_sel",     NetCmd_Sock_Sel},*/
    {"net_sock_opt_set_child", NetCmd_SockOptSetChild},
    {0, 0 }
};

/*
*********************************************************************************************************
*                                          NET CMD DICTIONARY
*********************************************************************************************************
*/

typedef  CPU_INT32U  NET_CMD_DICTIONARY_KEY;

#define  NET_CMD_DICTIONARY_KEY_INVALID            DEF_INT_32U_MAX_VAL

typedef  struct  net_cmd_dictionary {
    const  NET_CMD_DICTIONARY_KEY   Key;
    const  CPU_CHAR                *StrPtr;
    const  CPU_INT32U               StrLen;
} NET_CMD_DICTIONARY;

#ifdef  NET_IF_WIFI_MODULE_EN

#define NET_CMD_NET_TYPE_STR_INFRA                 "infra"
#define NET_CMD_NET_TYPE_STR_ADHOC                 "adhoc"
#define NET_CMD_NET_TYPE_STR_LEN                   5u

static  const  NET_CMD_DICTIONARY  NetCmd_DictionaryNetType[] = {
    { NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE,    NET_CMD_NET_TYPE_STR_INFRA,     NET_CMD_NET_TYPE_STR_LEN   },
    { NET_IF_WIFI_NET_TYPE_ADHOC,             NET_CMD_NET_TYPE_STR_ADHOC,     NET_CMD_NET_TYPE_STR_LEN   },
};

#define NET_CMD_SECURITY_TYPE_STR_OPEN             "open"
#define NET_CMD_SECURITY_TYPE_STR_WEP              "wep"
#define NET_CMD_SECURITY_TYPE_STR_WPA              "wpa"
#define NET_CMD_SECURITY_TYPE_STR_WPA2             "wpa2"
#define NET_CMD_SECURITY_TYPE_STR_WPS              "wps"

#define NET_CMD_SECURITY_TYPE_STR_LEN              4u

static  const  NET_CMD_DICTIONARY  NetCmd_DictionarySecurityType[] = {
    { NET_IF_WIFI_SECURITY_OPEN, NET_CMD_SECURITY_TYPE_STR_OPEN , (sizeof(NET_CMD_SECURITY_TYPE_STR_OPEN) - 1)},
    { NET_IF_WIFI_SECURITY_WEP,  NET_CMD_SECURITY_TYPE_STR_WEP ,  (sizeof(NET_CMD_SECURITY_TYPE_STR_WEP)  - 1)},
    { NET_IF_WIFI_SECURITY_WPA2, NET_CMD_SECURITY_TYPE_STR_WPA2,  (sizeof(NET_CMD_SECURITY_TYPE_STR_WPA2) - 1)},
    { NET_IF_WIFI_SECURITY_WPA,  NET_CMD_SECURITY_TYPE_STR_WPA,   (sizeof(NET_CMD_SECURITY_TYPE_STR_WPA)  - 1)},
};
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  NET_CMD_RESET_CMD_ARG        NetCmd_ResetCmdArgParse       (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_RESET_ARG            NetCmd_ResetTranslate         (       NET_CMD_RESET_CMD_ARG         cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_MTU_CMD_ARG          NetCmd_MTU_CmdArgParse        (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_MTU_ARG              NetCmd_MTU_Translate          (       NET_CMD_MTU_CMD_ARG           cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_ROUTE_CMD_ARG        NetCmd_RouteCmdArgParse       (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_ROUTE_ARG            NetCmd_RouteTranslate         (       NET_CMD_ROUTE_CMD_ARG         cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  CPU_INT16S                   NetCmd_Ping4                  (       NET_IPv4_ADDR                *p_addr_remote,
                                                                           void                         *p_data,
                                                                           CPU_INT16U                    data_len,
                                                                           CPU_INT32U                    cnt,
                                                                           SHELL_OUT_FNCT                out_fnct,
                                                                           SHELL_CMD_PARAM              *p_cmd_param);

static  CPU_INT16S                   NetCmd_Ping6                  (       NET_IPv6_ADDR                *p_addr_remote,
                                                                           void                         *p_data,
                                                                           CPU_INT16U                    data_len,
                                                                           SHELL_OUT_FNCT                out_fnct,
                                                                           SHELL_CMD_PARAM              *p_cmd_param);

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  void                         NetCmd_AutoCfgResult          (       NET_IF_NBR                    if_nbr,
                                                                    const  NET_IPv6_ADDR                *p_addr_local,
                                                                    const  NET_IPv6_ADDR                *p_addr_global,
                                                                           NET_IPv6_AUTO_CFG_STATUS      autocfg_result);
#endif

#ifdef  NET_IF_WIFI_MODULE_EN
static  NET_CMD_WIFI_SCAN_ARG        NetCmd_WiFiScanCmdArgParse    (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_WIFI_JOIN_ARG        NetCmd_WiFiJoinCmdArgParse    (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);
static  NET_CMD_WIFI_CREATE_ARG      NetCmd_WiFiCreateCmdArgParse  (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);
#endif


static  NET_CMD_SOCK_OPEN_CMD_ARG    NetCmd_Sock_OpenCmdParse      (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_OPEN_ARG        NetCmd_Sock_OpenCmdTranslate  (       NET_CMD_SOCK_OPEN_CMD_ARG     cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_ID_CMD_ARG      NetCmd_Sock_ID_CmdParse       (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_ID_ARG          NetCmd_Sock_ID_CmdTranslate   (       NET_CMD_SOCK_ID_CMD_ARG       cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_BIND_CMD_ARG    NetCmd_Sock_BindCmdParse      (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_BIND_ARG        NetCmd_Sock_BindCmdTranslate  (       NET_CMD_SOCK_BIND_CMD_ARG     cmd_args,
                                                                           NET_CMD_ERR                  *p_err);


static  NET_CMD_SOCK_LISTEN_CMD_ARG  NetCmd_Sock_ListenCmdParse    (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_LISTEN_ARG      NetCmd_Sock_ListenCmdTranslate(       NET_CMD_SOCK_LISTEN_CMD_ARG   cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_CONN_CMD_ARG    NetCmd_Sock_ConnCmdParse      (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_CONN_ARG        NetCmd_Sock_ConnCmdTranslate  (       NET_CMD_SOCK_CONN_CMD_ARG     cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_RX_CMD_ARG      NetCmd_Sock_RxCmdParse        (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_RX_ARG          NetCmd_Sock_RxCmdTranslate    (       NET_CMD_SOCK_RX_CMD_ARG       cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_TX_CMD_ARG      NetCmd_Sock_TxCmdParse        (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_TX_ARG          NetCmd_Sock_TxCmdTranslate    (       NET_CMD_SOCK_TX_CMD_ARG       cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_OPT_CMD_ARG     NetCmd_Sock_OptCmdParse       (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_OPT_ARG         NetCmd_Sock_OptCmdTranslate   (       NET_CMD_SOCK_OPT_CMD_ARG      cmd_args,
                                                                           NET_CMD_ERR                  *p_err);

#if 0
static  NET_CMD_SOCK_SEL_CMD_ARG     NetCmd_Sock_SelCmdParse       (       CPU_INT16U                    argc,
                                                                           CPU_CHAR                     *p_argv[],
                                                                           NET_CMD_ERR                  *p_err);

static  NET_CMD_SOCK_SEL_ARG         NetCmd_Sock_SelCmdTranslate   (       NET_CMD_SOCK_SEL_CMD_ARG      cmd_args,
                                                                           NET_CMD_ERR                  *p_err);


static  NET_SOCK_ID                  NetCmd_Sock_SelGetSockID      (       CPU_CHAR                     *p_str,
                                                                           CPU_CHAR                     *p_str_next);
#endif

#ifdef  NET_IF_WIFI_MODULE_EN

static  CPU_INT32U                   NetCmd_DictionaryGet          (const  NET_CMD_DICTIONARY           *p_dictionary_tbl,
                                                                           CPU_INT32U                    dictionary_size,
                                                                    const  CPU_CHAR                     *p_str_cmp,
                                                                           CPU_INT32U                    str_len);
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             NetCmd_Init()
*
* Description : Add Network commands to uC-Shell.
*
* Argument(s) : p_err    is a pointer to an error code which will be returned to your application:
*
*                             NET_CMD_ERR_NONE            No error.
*
*                             NET_CMD_ERR_SHELL_INIT    Command table not added to uC-Shell
*
* Return(s)   : none.
*
* Caller(s)   : AppTaskStart().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetCmd_Init (NET_CMD_ERR  *p_err)
{
    SHELL_ERR  shell_err;


    Shell_CmdTblAdd("net", NetCmdTbl, &shell_err);

    if (shell_err == SHELL_ERR_NONE) {
        *p_err = NET_CMD_ERR_NONE;
    } else {
        *p_err = NET_CMD_ERR_SHELL_INIT;
         return;
    }
}


/*
*********************************************************************************************************
*                                            NetCmd_Start()
*
* Description : (1) Initialize Network stack and Network command:
*
*                   (a) Initialize uC/TCP-IP.
*                   (b) Add the specified interface.
*                   (c) Configure the interface following command argument (IP addresses, MAC address, MTU, etc.).
*                   (d) Start the interface.
*
* Argument(s) : p_if_api    Pointer to specific network interface API.
*
*               p_dev_api   Pointer to specific network device driver API.
*
*               p_dev_bsp   Pointer to specific network device board-specific API.
*
*               p_dev_cfg   Pointer to specific network device hardware configuration.
*
*               p_ext_api   Pointer to specific network extension layer API
*
*               p_ext_cfg   Pointer to specific network extension layer configuration
*
*               p_test_arg  Pointer to configuration arguments.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetCmd_Start (NET_TASK_CFG   *p_rx_task_cfg,
                    NET_TASK_CFG   *p_tx_task_cfg,
                    NET_TASK_CFG   *p_tmr_task_cfg,
                    void           *p_if_api,
                    void           *p_dev_api,
                    void           *p_dev_bsp,
                    void           *p_dev_cfg,
                    void           *p_ext_api,
                    void           *p_ext_cfg,
                    NET_CMD_ARGS   *p_cmd_args,
                    NET_CMD_ERR    *p_err)
{
    NET_IF_NBR          if_nbr;
    CPU_BOOLEAN         link_status;
    NET_ERR             err;
#ifdef  NET_IF_ETHER_MODULE_EN
    NET_DEV_CFG_ETHER  *p_ether_dev_cfg;
#endif


    err = Net_Init(p_rx_task_cfg,                               /* Init uC/TCP-IP.                                      */
                   p_tx_task_cfg,
                   p_tmr_task_cfg);

    if (err != NET_ERR_NONE) {
       *p_err = NET_CMD_ERR_TCPIP_INIT;
        return;
    }

#ifdef  NET_IF_ETHER_MODULE_EN
    if (p_cmd_args->WindowsIF_Nbr != 0) {

        p_ether_dev_cfg           = (NET_DEV_CFG_ETHER *)p_dev_cfg;
        p_ether_dev_cfg->BaseAddr = p_cmd_args->WindowsIF_Nbr;
    }
#endif

    if_nbr  = NetIF_Add(p_if_api,                               /* Ethernet  interface API.                         */
                        p_dev_api,                              /* Device API.                                      */
                        p_dev_bsp,                              /* Device BSP.                                      */
                        p_dev_cfg,                              /* Device configuration.                            */
                        p_ext_api,                              /* No Phy API.                                      */
                        p_ext_cfg,                              /* No PHY configuration.                            */
                       &err);
    if (err != NET_IF_ERR_NONE) {
       *p_err = NET_CMD_ERR_TCPIP_IF_ADD;
        return;
    }

    if (p_cmd_args->MAC_CfgEn == DEF_YES) {
        NetIF_AddrHW_Set(if_nbr, &p_cmd_args->MAC_Addr.MAC_Addr[0], sizeof(p_cmd_args->MAC_Addr), &err);
        if (err != NET_IF_ERR_NONE) {
            *p_err = NET_CMD_ERR_TCPIP_MAC_CFG;
             return;
        }
    }


    NetIF_Start(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
       *p_err = NET_CMD_ERR_TCPIP_IF_START;
        return;
    }

    link_status = NetIF_LinkStateWaitUntilUp(if_nbr, 3, 1000, &err);
    if (link_status == NET_IF_LINK_DOWN) {
       *p_err = NET_CMD_ERR_LINK_DOWN;
        return;
    }

#ifdef  NET_IPv4_MODULE_EN
    if (p_cmd_args->IPv4_CfgEn == DEF_YES) {
        NetCmd_IF_IPv4AddrCfgStatic(if_nbr, &p_cmd_args->IPv4, p_err);
    }
#endif

#ifdef  NET_IPv6_MODULE_EN
    if (p_cmd_args->IPv6_CfgEn == DEF_YES) {
        NetCmd_IF_IPv6AddrCfgStatic(if_nbr, &p_cmd_args->IPv6, p_err);
    }

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    NetIPv6_AddrAutoCfgHookSet(if_nbr, &NetCmd_AutoCfgResult, &err); /* Ignore error.                                   */
    /* NetIPv6_AddrAutoCfgEn(if_nbr, DEF_YES, &err); */
#endif

#endif

    *p_err = NET_CMD_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             NetCmd_Help()
*
* Description : Command function to print out Net Commands help.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_Help (CPU_INT16U        argc,
                        CPU_CHAR         *p_argv[],
                        SHELL_OUT_FNCT    out_fnct,
                        SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S  ret_val;


    ret_val = NetCmd_OutputCmdTbl(NetCmdTbl, out_fnct, p_cmd_param);

    (void)&argc;
    (void)&p_argv;

    return (ret_val);
}



/*
*********************************************************************************************************
*                                          NetCmd_IF_Config()
*
* Description : Command function to print out interfaces information.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_IF_Config (CPU_INT16U        argc,
                             CPU_CHAR         *p_argv[],
                             SHELL_OUT_FNCT    out_fnct,
                             SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT08U         if_nbr;
    CPU_INT08U         addr_ix;
#ifdef  NET_IPv4_MODULE_EN
    NET_IPv4_IF_CFG   *p_ipv4_if_cfg;
    NET_IPv4_ADDRS    *p_addrs_ipv4;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_IPv6_IF_CFG   *p_ipv6_if_cfg;
    NET_IPv6_ADDRS    *p_addrs_ipv6;
#endif
    CPU_CHAR           addr_ip_str[NET_ASCII_LEN_MAX_ADDR_IP];
    CPU_CHAR           str_output[DEF_INT_32U_NBR_DIG_MAX + 1];
    CPU_INT16S         ret_val;
    NET_ERR            err_net;


    if (argc > 1) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    for (if_nbr = 1; if_nbr <= NET_IF_CFG_MAX_NBR_IF; if_nbr++) {

        ret_val = NetCmd_OutputMsg("Interface ID : ", DEF_YES, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

        (void)Str_FmtNbr_Int32U(if_nbr,
                                DEF_INT_32U_NBR_DIG_MAX,
                                DEF_NBR_BASE_DEC,
                                DEF_NULL,
                                DEF_NO,
                                DEF_YES,
                                str_output);
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

#ifdef  NET_IPv4_MODULE_EN
        p_ipv4_if_cfg  = &NetIPv4_IF_CfgTbl[if_nbr];
        p_addrs_ipv4   =  p_ipv4_if_cfg->AddrsTbl;
        addr_ix        =  0u;
        while (addr_ix < p_ipv4_if_cfg->AddrsNbrCfgd) {
            ret_val = NetCmd_OutputMsg("Host Address : ", DEF_YES, DEF_NO,  DEF_YES, out_fnct, p_cmd_param);
            NetASCII_IPv4_to_Str(p_addrs_ipv4->AddrHost, addr_ip_str, DEF_NO, &err_net);
            ret_val = NetCmd_OutputMsg(addr_ip_str, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

            ret_val = NetCmd_OutputMsg("Mask         : ", DEF_YES, DEF_NO,  DEF_YES, out_fnct, p_cmd_param);
            NetASCII_IPv4_to_Str(p_addrs_ipv4->AddrHostSubnetMask, addr_ip_str, DEF_NO, &err_net);
            ret_val = NetCmd_OutputMsg(addr_ip_str, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

            ret_val = NetCmd_OutputMsg("Gateway      : ", DEF_YES, DEF_NO,  DEF_YES, out_fnct, p_cmd_param);
            NetASCII_IPv4_to_Str(p_addrs_ipv4->AddrDfltGateway, addr_ip_str, DEF_NO, &err_net);
            ret_val = NetCmd_OutputMsg(addr_ip_str, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

            p_addrs_ipv4++;
            addr_ix++;
        }
#endif

#ifdef  NET_IPv6_MODULE_EN
        p_ipv6_if_cfg  = NetIPv6_GetIF_CfgObj(if_nbr, &err_net);
        p_addrs_ipv6   = p_ipv6_if_cfg->AddrsTbl;
        addr_ix        = 0u;

        while (addr_ix < p_ipv6_if_cfg->AddrsNbrCfgd) {
            ret_val = NetCmd_OutputMsg("Host Address : ", DEF_YES, DEF_NO,  DEF_YES, out_fnct, p_cmd_param);
            NetASCII_IPv6_to_Str(&p_addrs_ipv6->AddrHost, addr_ip_str, DEF_NO, DEF_NO, &err_net);
            ret_val = NetCmd_OutputMsg(addr_ip_str, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

            ret_val = NetCmd_OutputMsg("%", DEF_NO, DEF_NO,  DEF_NO, out_fnct, p_cmd_param);
            (void)Str_FmtNbr_Int32U(p_addrs_ipv6->AddrHostPrefixLen,
                                    DEF_INT_32U_NBR_DIG_MAX,
                                    DEF_NBR_BASE_DEC,
                                    DEF_NULL,
                                    DEF_NO,
                                    DEF_YES,
                                    str_output);
            ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

            p_addrs_ipv6++;
            addr_ix++;
        }
#endif
    }

    ret_val = NetCmd_OutputMsg(" ", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

    (void)&p_argv;

    return (ret_val);
}


/*
*********************************************************************************************************
*                                           NetCmd_IF_Reset()
*
* Description : Command function to reset interface(s) : remove all configured addresses, IPv4 or IPv6 or both,
*               on specified interface. If no interface is specified, all existing interfaces will be reset.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments. (see Note #1)
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : (1) This function takes as arguments :
*                       -i if_nbr     Specified on which interface the reset will occur.
*                       -4            Specified to clear only the IPv4 addresses of the interface.
*                       -6            Specified to clear only the IPv6 addresses of the interface.
*
*                   If no arguments are passed, the function will reset all interfaces and both IPv4 and IPv6
*                   addresses.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_IF_Reset (CPU_INT16U                argc,
                             CPU_CHAR                 *p_argv[],
                             SHELL_OUT_FNCT            out_fnct,
                             SHELL_CMD_PARAM          *p_cmd_param)
{
    CPU_INT08U             if_nbr;
    CPU_INT16S             ret_val;
    CPU_BOOLEAN            result;
    NET_CMD_RESET_CMD_ARG  cmd_args;
    NET_CMD_RESET_ARG      args;
    NET_CMD_ERR            err;
    NET_ERR                err_net;


    cmd_args = NetCmd_ResetCmdArgParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    args = NetCmd_ResetTranslate(cmd_args, &err);

#ifdef  NET_IPv4_MODULE_EN
    if (args.IPv4_En == DEF_YES) {
        if (args.IF_Nbr != NET_IF_NBR_NONE) {
            result = NetIPv4_CfgAddrRemoveAll(args.IF_Nbr, &err_net);
        } else {
            for (if_nbr = 1; if_nbr <= NET_IF_CFG_MAX_NBR_IF; if_nbr++) {
                result = NetIPv4_CfgAddrRemoveAll(if_nbr, &err_net);
                if (result != DEF_OK) {
                    break;
                }
            }
        }
        if (result != DEF_OK) {
            ret_val = NetCmd_OutputError("Failed to reset Interface for IPv4", out_fnct, p_cmd_param);
            return (ret_val);
        } else {
            ret_val = NetCmd_OutputMsg("Reset Interface for IPv4", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        }

    }
#endif

#ifdef  NET_IPv6_MODULE_EN
    if (args.IPv6_En == DEF_YES) {
        if (args.IF_Nbr != NET_IF_NBR_NONE) {
            result = NetIPv6_CfgAddrRemoveAll(args.IF_Nbr, &err_net);
        } else {
            for (if_nbr = 1; if_nbr <= NET_IF_CFG_MAX_NBR_IF; if_nbr++) {
                result = NetIPv6_CfgAddrRemoveAll(if_nbr, &err_net);
                if (result != DEF_OK) {
                    break;
                }
            }
        }
        if (result != DEF_OK) {
            ret_val = NetCmd_OutputError("Failed to reset Interface for IPv6", out_fnct, p_cmd_param);
            return (ret_val);
        } else {
            ret_val = NetCmd_OutputMsg("Reset Interface for IPv6", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        }
    }
#endif

    ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);

    return (ret_val);

}


/*
*********************************************************************************************************
*                                          NetCmd_IF_SetMTU()
*
* Description : Command function to configure MTU of given Interface.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments. (see Note #1)
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : (1) This function takes as arguments :
*                       -i if_nbr     Specified on which interface the reset will occur.
*                       -M mtu        Specified the new MTU to configure.
*********************************************************************************************************
*/

CPU_INT16S   NetCmd_IF_SetMTU (CPU_INT16U        argc,
                               CPU_CHAR         *p_argv[],
                               SHELL_OUT_FNCT    out_fnct,
                               SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_MTU_CMD_ARG  cmd_args;
    NET_CMD_MTU_ARG      args;
    CPU_INT16S           ret_val;
    NET_CMD_ERR          err;
    NET_ERR              net_err;


    ret_val    = 0u;

    cmd_args = NetCmd_MTU_CmdArgParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    args = NetCmd_MTU_Translate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    NetIF_MTU_Set (args.IF_Nbr,
                   args.MTU,
                  &net_err);
    if (net_err != NET_IF_ERR_NONE) {
        ret_val = NetCmd_OutputError("Failed to configure Interface MTU", out_fnct, p_cmd_param);
        return (ret_val);
    }

    ret_val = NetCmd_OutputMsg("Configured Interface MTU", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

    ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                         NetCmd_IF_RouteAdd()
*
* Description : Command function to add IP address route.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_IF_RouteAdd (CPU_INT16U        argc,
                               CPU_CHAR         *p_argv[],
                               SHELL_OUT_FNCT    out_fnct,
                               SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_ROUTE_CMD_ARG  cmd_args;
    NET_CMD_ROUTE_ARG      args;
    CPU_INT16S             ret_val;
    CPU_BOOLEAN            addr_added;
    NET_CMD_ERR            err;


    ret_val    = 0u;
    addr_added = DEF_NO;


    if (argc < 6) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }


    cmd_args = NetCmd_RouteCmdArgParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    args = NetCmd_RouteTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    if (args.IF_Nbr == NET_IF_NBR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

#ifdef  NET_IPv4_MODULE_EN
    if (args.IPv4_En == DEF_YES) {
        NetCmd_IF_IPv4AddrCfgStatic(args.IF_Nbr, &args.IPv4Cfg, &err);
        if (err != NET_CMD_ERR_NONE) {
            ret_val  = NetCmd_OutputError("Failed to configure IPv4 static address", out_fnct, p_cmd_param);
            ret_val += NetCmd_OutputErrorNetNbr((NET_ERR)err, out_fnct,p_cmd_param);
            return (ret_val);
        }
        ret_val    = NetCmd_OutputMsg("Added IPv4 static address", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        addr_added = DEF_YES;
    }
#endif

#ifdef  NET_IPv6_MODULE_EN
    if (args.IPv6_En == DEF_YES) {
        NetCmd_IF_IPv6AddrCfgStatic(args.IF_Nbr, &args.IPv6Cfg, &err);
        if (err != NET_CMD_ERR_NONE) {
            ret_val  = NetCmd_OutputError("Failed to configure IPv6 static address", out_fnct, p_cmd_param);
            ret_val += NetCmd_OutputErrorNetNbr((NET_ERR)err, out_fnct, p_cmd_param);
            return (ret_val);
        }
        ret_val    = NetCmd_OutputMsg("Added IPv6 static address", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        addr_added = DEF_YES;
    }
#endif

    if (addr_added == DEF_NO) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }


    ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                        NetCmd_IF_RouteRemove()
*
* Description : Command function to remove a route (previously added using NetCmd_IF_RouteAdd()).
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_IF_RouteRemove (CPU_INT16U        argc,
                                  CPU_CHAR         *p_argv[],
                                  SHELL_OUT_FNCT    out_fnct,
                                  SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_ROUTE_CMD_ARG  cmd_args;
    NET_CMD_ROUTE_ARG      args;
    CPU_INT16S             ret_val;
    CPU_BOOLEAN            addr_removed;
    NET_CMD_ERR            err;


    ret_val      = 0u;
    addr_removed = DEF_NO;


    cmd_args = NetCmd_RouteCmdArgParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    args = NetCmd_RouteTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

    if (args.IF_Nbr == NET_IF_NBR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }

#ifdef  NET_IPv4_MODULE_EN
    if (args.IPv4_En == DEF_YES) {
        NetCmd_IF_IPv4AddrRemove(args.IF_Nbr, &args.IPv4Cfg, &err);
        if (err != NET_CMD_ERR_NONE) {
            ret_val = NetCmd_OutputError("Failed to remove IPv4 static address", out_fnct, p_cmd_param);
            return (ret_val);
        }
        ret_val    = NetCmd_OutputMsg("Removed IPv4 static address", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        addr_removed = DEF_YES;
    }
#endif

#ifdef  NET_IPv6_MODULE_EN
    if (args.IPv6_En == DEF_YES) {
        NetCmd_IF_IPv6AddrRemove(args.IF_Nbr, &args.IPv6Cfg, &err);
        if (err != NET_CMD_ERR_NONE) {
            ret_val = NetCmd_OutputError("Failed to remove IPv6 static address", out_fnct, p_cmd_param);
            return (ret_val);
        }
        ret_val      = NetCmd_OutputMsg("Removed IPv6 static address", DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        addr_removed = DEF_YES;
    }
#endif

    if (addr_removed == DEF_NO) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        return (ret_val);
    }


    ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);

    return (ret_val);

}


/*
*********************************************************************************************************
*                                     NetCmd_IF_IPv4AddrCfgStatic()
*
* Description : Add a static IPv4 address on an interface.
*
* Argument(s) : if_id       Network interface number.
*
*               p_ip_cfg    Pointer to IPv4 address configuration structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*               NetCmd_IF_RouteAdd(),
*               NetCmd_IP_Config(),
*               NetCmd_Start().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
void  NetCmd_IF_IPv4AddrCfgStatic (NET_IF_NBR         if_id,
                                   NET_CMD_IPv4_CFG  *p_ip_cfg,
                                   NET_CMD_ERR       *p_err)
{
    NET_ERR  err;


    (void)NetIPv4_CfgAddrAdd(if_id,
                             p_ip_cfg->Host,
                             p_ip_cfg->Mask,
                             p_ip_cfg->Gateway,
                            &err);
    switch (err) {
        case NET_IPv4_ERR_NONE:
             break;


        case NET_IF_ERR_INVALID_IF:
            *p_err = NET_CMD_ERR_IF_INVALID;
             return;


        case NET_IPv4_ERR_ADDR_TBL_FULL:
            *p_err = NET_CMD_ERR_IPv4_ADDR_TBL_FULL;
             return;


        case NET_IPv4_ERR_ADDR_CFG_STATE:
            *p_err = NET_CMD_ERR_IPv4_ADDR_CFGD;
             return;


        case NET_IPv4_ERR_ADDR_CFG_IN_USE:
            *p_err = NET_CMD_ERR_IPv4_ADDR_IN_USE;
             return;


        case NET_IPv4_ERR_INVALID_ADDR_HOST:
        case NET_IPv4_ERR_INVALID_ADDR_GATEWAY:
            *p_err = NET_CMD_ERR_IPv4_ADDR_INVALID;
             return;


        case NET_INIT_ERR_NOT_COMPLETED:
        case NET_ERR_FAULT_LOCK_ACQUIRE:
        default:
            *p_err = NET_CMD_ERR_FAULT;
             return;
    }


    *p_err = NET_CMD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     NetCmd_IF_IPv6AddrCfgStatic()
*
* Description : Add a static IPv6 address on an interface.
*
* Argument(s) : if_id       Network interface number.
*
*               p_ip_cfg    Pointer to IPv6 address configuration structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*               NetCmd_IF_RouteAdd(),
*               NetCmd_Start().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
void  NetCmd_IF_IPv6AddrCfgStatic (NET_IF_NBR          if_id,
                                   NET_CMD_IPv6_CFG  *p_ip_cfg,
                                   NET_CMD_ERR       *p_err)
{
    NET_IPv6_ADDR  prefix;
    NET_FLAGS      ipv6_flags;
    NET_ERR        err;


    NetIPv6_AddrMaskByPrefixLen(&p_ip_cfg->Host, p_ip_cfg->PrefixLen, &prefix, &err);

#ifdef  NET_NDP_MODULE_EN
    NetNDP_PrefixAddCfg(if_id, &prefix, p_ip_cfg->PrefixLen, DEF_NO, DEF_NULL, 0, &err);
#endif
    ipv6_flags = 0;
    DEF_BIT_CLR(ipv6_flags, NET_IPv6_FLAG_BLOCK_EN);
    DEF_BIT_SET(ipv6_flags, NET_IPv6_FLAG_DAD_EN);

    (void)NetIPv6_CfgAddrAdd(if_id,
                            &p_ip_cfg->Host,
                             p_ip_cfg->PrefixLen,
                             ipv6_flags,
                            &err);
    switch (err) {
        case NET_IPv6_ERR_NONE:
        case NET_ERR_FAULT_FEATURE_DIS:
        case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
             break;


        case NET_IF_ERR_INVALID_IF:
            *p_err = NET_CMD_ERR_IF_INVALID;
             return;


        case NET_IPv6_ERR_ADDR_TBL_FULL:
            *p_err = NET_CMD_ERR_IPv6_ADDR_TBL_FULL;
             return;


        case NET_IPv6_ERR_ADDR_CFG_STATE:
        case NET_IPv6_ERR_ADDR_CFG_IN_USE:
            *p_err = NET_CMD_ERR_IPv6_ADDR_CFGD;
             return;


        case NET_IPv6_ERR_INVALID_ADDR_HOST:
            *p_err = NET_CMD_ERR_IPv6_ADDR_INVALID;
             return;


        case NET_INIT_ERR_NOT_COMPLETED:
        case NET_ERR_FAULT_LOCK_ACQUIRE:
        default:
            *p_err = NET_CMD_ERR_FAULT;
             return;
    }


   *p_err = NET_CMD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetCmd_IF_IPv4AddrRemove()
*
* Description : Remove an IPv4 address.
*
* Argument(s) : if_id       Network interface number.
*
*               p_ip_cfg    Pointer to IPv4 address configuration structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*               NetCmd_IF_RouteRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
void  NetCmd_IF_IPv4AddrRemove (NET_IF_NBR          if_id,
                                NET_CMD_IPv4_CFG  *p_ip_cfg,
                                NET_CMD_ERR       *p_err)
{
    NET_ERR  err;


    (void)NetIPv4_CfgAddrRemove(if_id,
                                p_ip_cfg->Host,
                               &err);

    switch (err) {
    case NET_IPv4_ERR_NONE:
         break;

    case NET_IF_ERR_INVALID_IF:
        *p_err = NET_CMD_ERR_IF_INVALID;
         return;

    case NET_IPv4_ERR_INVALID_ADDR_HOST:
        *p_err = NET_CMD_ERR_IPv4_ADDR_INVALID;
         return;


    case NET_IPv4_ERR_ADDR_CFG_STATE:
        *p_err = NET_CMD_ERR_IPv4_ADDR_CFGD;
         return;

    case NET_IPv4_ERR_ADDR_TBL_EMPTY:
        *p_err = NET_CMD_ERR_IPv4_ADDR_TBL_EMPTY;
         return;


    case NET_IPv4_ERR_ADDR_NOT_FOUND:
        *p_err = NET_CMD_ERR_IPv4_ADDR_NOT_FOUND;
         return;


    case NET_INIT_ERR_NOT_COMPLETED:
    case NET_ERR_FAULT_LOCK_ACQUIRE:
    default:
        *p_err = NET_CMD_ERR_FAULT;
         return;
    }

   *p_err = NET_CMD_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetCmd_IF_IPv6AddrRemove()
*
* Description : Remove an IPv6 address.
*
* Argument(s) : if_id       Network interface number.
*
*               p_ip_cfg    Pointer to IPv6 address configuration structure.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*               NetCmd_IF_RouteRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
void  NetCmd_IF_IPv6AddrRemove (NET_IF_NBR          if_id,
                                NET_CMD_IPv6_CFG  *p_ip_cfg,
                                NET_CMD_ERR       *p_err)
{
    NET_ERR  err;


    (void)NetIPv6_CfgAddrRemove(if_id,
                               &p_ip_cfg->Host,
                               &err);

    switch (err) {
    case NET_IPv6_ERR_NONE:
         break;

    case NET_IF_ERR_INVALID_IF:
        *p_err = NET_CMD_ERR_IF_INVALID;
         return;

    case NET_IPv6_ERR_INVALID_ADDR_HOST:
        *p_err = NET_CMD_ERR_IPv6_ADDR_INVALID;
         return;


    case NET_IPv6_ERR_ADDR_CFG_STATE:
        *p_err = NET_CMD_ERR_IPv6_ADDR_CFGD;
         return;

    case NET_IPv6_ERR_ADDR_TBL_EMPTY:
        *p_err = NET_CMD_ERR_IPv6_ADDR_TBL_EMPTY;
         return;


    case NET_IPv6_ERR_ADDR_NOT_FOUND:
        *p_err = NET_CMD_ERR_IPv6_ADDR_NOT_FOUND;
         return;


    case NET_INIT_ERR_NOT_COMPLETED:
    case NET_ERR_FAULT_LOCK_ACQUIRE:
    default:
        *p_err = NET_CMD_ERR_FAULT;
         return;
    }

   *p_err = NET_CMD_ERR_NONE;
}
#endif

/*
*********************************************************************************************************
*                                             NetCmd_Ping()
*
* Description : Function command to ping another host.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_Ping (CPU_INT16U        argc,
                        CPU_CHAR         *p_argv[],
                        SHELL_OUT_FNCT    out_fnct,
                        SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_CHAR              *p_data;
    CPU_INT16S             ret_val;
    NET_CMD_PING_CMD_ARG   cmd_arg;
    NET_CMD_PING_ARG       args;
    CPU_INT16U             data_len;
    CPU_INT32U             cnt;
    CPU_SIZE_T             octets_reqd;
    LIB_ERR                err_lib;
    NET_CMD_ERR            err;


    ret_val = 0u;
    cmd_arg = NetCmd_PingCmdArgParse(argc, p_argv, &err);

    switch (err) {
        case NET_CMD_ERR_NONE:
             break;

        case NET_CMD_ERR_CMD_ARG_INVALID:
        default:
             ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
             return (ret_val);
    }


    args = NetCmd_PingCmdArgTranslate(&cmd_arg, &err);
    switch (err) {
        case NET_CMD_ERR_NONE:
             break;

        case NET_CMD_ERR_CMD_ARG_INVALID:
        default:
             ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
             return (ret_val);
    }


    if (args.DataLen == 0) {
        data_len = Str_Len(cmd_arg.AddrPtr);
        p_data   = cmd_arg.AddrPtr;
    } else {
        data_len = args.DataLen;
        p_data   = (CPU_CHAR *) Mem_HeapAlloc (               data_len,
                                               (CPU_SIZE_T  ) sizeof(void *),
                                               (CPU_SIZE_T *)&octets_reqd,
                                                             &err_lib);
    }

    if (args.Cnt == 0) {
        cnt = 1u;
    } else {
        cnt = args.Cnt;
    }


    switch (args.family) {
        case NET_IP_ADDR_FAMILY_IPv4:
             ret_val = NetCmd_Ping4((NET_IPv4_ADDR *)&args.Addr,
                                                       p_data,
                                                       data_len,
                                                       cnt,
                                                       out_fnct,
                                                       p_cmd_param);
             break;


        case NET_IP_ADDR_FAMILY_IPv6:
             ret_val = NetCmd_Ping6((NET_IPv6_ADDR *)&args.Addr,
                                                       p_data,
                                                       data_len,
                                                       out_fnct,
                                                       p_cmd_param);
             break;


        default:
            ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
            return (ret_val);
    }

    return (ret_val);
}


/*
*********************************************************************************************************
*                                       NetCmd_PingCmdArgParse()
*
* Description : Parse ping command line.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Ping arguments parsed.
*
* Caller(s)   : NetIxANVL_BkgndPing(),
*               NetCmd_Ping().
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_CMD_PING_CMD_ARG  NetCmd_PingCmdArgParse (CPU_INT16U    argc,
                                              CPU_CHAR     *p_argv[],
                                              NET_CMD_ERR  *p_err)
{
    NET_CMD_PING_CMD_ARG  cmd_args;
    CPU_BOOLEAN            dig_hex;
    CPU_INT16U             i;


    cmd_args.IF_NbrPtr  = DEF_NULL;
    cmd_args.AddrPtr    = DEF_NULL;
    cmd_args.DataLenPtr = DEF_NULL;
    cmd_args.CntPtr     = DEF_NULL;

    if (argc > 10) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.IF_NbrPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                case NET_CMD_ARG_LEN:
                     i += NetCmd_ArgsParserParseDataLen(&p_argv[i], &cmd_args.DataLenPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                case NET_CMD_ARG_CNT:
                     i += NetCmd_ArgsParserParseDataLen(&p_argv[i], &cmd_args.CntPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }

        } else {
            dig_hex = ASCII_IS_DIG_HEX(*p_argv[i]);
            if (dig_hex == DEF_NO) {
               *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }

            cmd_args.AddrPtr = p_argv[i];
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                     NetCmd_PingCmdArgTranslate()
*
* Description : Translate ping arguments.
*
* Argument(s) : cmd_arg     Pointer to the ping argument to translate.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Ping argument converted.
*
* Caller(s)   : NetIxANVL_BkgndPing(),
*               NetCmd_Ping().
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_CMD_PING_ARG  NetCmd_PingCmdArgTranslate (NET_CMD_PING_CMD_ARG  *p_cmd_args,
                                              NET_CMD_ERR           *p_err)
{
    NET_CMD_PING_ARG  args;
    NET_ERR            err;


    if (p_cmd_args->IF_NbrPtr != DEF_NULL) {
        args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(p_cmd_args->IF_NbrPtr, p_err);
            if (*p_err != NET_CMD_ERR_NONE) {
                 return (args);
        }
    } else {
        args.IF_Nbr = NET_IF_NBR_NONE;
    }

    if (p_cmd_args->DataLenPtr != DEF_NULL) {
        args.DataLen = NetCmd_ArgsParserTranslateDataLen(p_cmd_args->DataLenPtr, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
             return (args);
        }
    } else {
        args.DataLen = 0;
    }

    if (p_cmd_args->CntPtr != DEF_NULL) {
        args.Cnt = NetCmd_ArgsParserTranslateDataLen(p_cmd_args->CntPtr, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
             return (args);
        }
    } else {
        args.Cnt = 0;
    }


    args.family = NetASCII_Str_to_IP((void *)p_cmd_args->AddrPtr,
                                            &args.Addr,
                                             sizeof(args.Addr),
                                            &err);
    switch (err) {
        case NET_ASCII_ERR_NONE:
             break;


        default:
            *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
             return (args);
    }

#if 0
    args.Background = p_cmd_args->Background;
#endif

   *p_err = NET_CMD_ERR_NONE;

    return (args);
}



/*
*********************************************************************************************************
*                                          NetCmd_IP_Config()
*
* Description : Command function to configure an IPv4 address.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv4_MODULE_EN
CPU_INT16S NetCmd_IP_Config (CPU_INT16U        argc,
                             CPU_CHAR         *p_argv[],
                             SHELL_OUT_FNCT    out_fnct,
                             SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_IF_NBR              if_nbr;
    NET_CMD_IPv4_ASCII_CFG  ip_str_cfg;
    NET_CMD_IPv4_CFG        ip_cfg;
    CPU_INT16S              ret_val;
    CPU_INT32U              arg_ix;
    NET_CMD_ERR             cmd_err;
    NET_ERR                 net_err;


                                                                /* Initializing address configuration parameters.       */
    if_nbr                = NET_IF_NBR_NONE;
    ret_val               = 0u;

    ip_str_cfg.HostPtr    = DEF_NULL;
    ip_str_cfg.MaskPtr    = DEF_NULL;
    ip_str_cfg.GatewayPtr = DEF_NULL;

    for (arg_ix = 1; arg_ix < argc; arg_ix++) {
        if (*p_argv[arg_ix] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[arg_ix] + 1)) {
                case NET_CMD_ARG_IF:
                     arg_ix++;
                     if_nbr = (NET_IF_NBR)Str_ParseNbr_Int32U(p_argv[arg_ix],
                                                              DEF_NULL,
                                                              10);
                     break;


                case NET_CMD_ARG_ADDR:
                     arg_ix++;
                     ip_str_cfg.HostPtr = p_argv[arg_ix];
                     break;


                case NET_CMD_ARG_MASK:
                     arg_ix++;
                     ip_str_cfg.MaskPtr = p_argv[arg_ix];
                     break;


                default:
                     cmd_err = NET_CMD_ERR_PARSER_ARG_INVALID;
                     return (0);
            }
        }
    }

    ip_cfg.Host = NetASCII_Str_to_IPv4(ip_str_cfg.HostPtr, &net_err);
    if (net_err != NET_ASCII_ERR_NONE) {
        ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        return (ret_val);
    }

    ip_cfg.Mask = NetASCII_Str_to_IPv4(ip_str_cfg.MaskPtr, &net_err);

    if (net_err != NET_ASCII_ERR_NONE) {
        ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        return (ret_val);
    }

    ip_cfg.Gateway = NET_IPv4_ADDR_NONE;

    NetCmd_IF_IPv4AddrCfgStatic(if_nbr, &ip_cfg, &cmd_err);

    if ((cmd_err == NET_CMD_ERR_NONE) ||
        (cmd_err == NET_CMD_ERR_IPv4_ADDR_IN_USE)) {
        ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);
    } else {
        ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
    }

    return (ret_val);
}
#endif


/*
*********************************************************************************************************
*                                          NetCmd_IF_Start()
*
* Description : Command function to start an Network interface.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_IF_Start (CPU_INT16U        argc,
                           CPU_CHAR         *p_argv[],
                           SHELL_OUT_FNCT    out_fnct,
                           SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S              ret_val;
    NET_ERR                 net_err;
    NET_CMD_ERR             cmd_err;
    NET_IF_NBR              if_nbr;
    CPU_INT08U              i;
    CPU_CHAR               *p_if_str;


    ret_val = 0u;

     if (argc == 3u) {

        for (i = 1u; i < argc; i++) {

            if (*p_argv[i] == NET_CMD_ARG_BEGIN) {

                if (*(p_argv[i] + 1) == NET_CMD_ARG_IF) {

                    i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, &cmd_err);

                    if (cmd_err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                    }
                    if_nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, &cmd_err);
                    if (cmd_err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                    }
                }
            }
        }


        NetIF_Start(if_nbr, &net_err);

        if (net_err == NET_IF_ERR_NONE) {
            NetCmd_OutputSuccess(out_fnct,
                                 p_cmd_param);

        } else if (net_err == NET_IF_ERR_INVALID_STATE) {
            ret_val = NetCmd_OutputError("The interface is already started.",
                                            out_fnct,
                                           p_cmd_param);

        } else {
            ret_val = NetCmd_OutputError("Interface cannot be started.",
                                           out_fnct,
                                           p_cmd_param);
        }

    } else {
        ret_val = NetCmd_OutputMsg("Usage: net_if_start [-i interface_nbr]",
                                  DEF_YES,
                                  DEF_YES,
                                  DEF_NO,
                                  out_fnct,
                                  p_cmd_param);
    }
    return ret_val;
}


/*
*********************************************************************************************************
*                                          NetCmd_IF_Stop()
*
* Description : Command function to stop an Network interface.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_IF_Stop (CPU_INT16U        argc,
                           CPU_CHAR         *p_argv[],
                           SHELL_OUT_FNCT    out_fnct,
                           SHELL_CMD_PARAM  *p_cmd_param)

{
    CPU_INT16S              ret_val;
    NET_ERR                 net_err;
    NET_CMD_ERR             cmd_err;
    NET_IF_NBR              if_nbr;
    CPU_INT08U              i;
    CPU_CHAR               *p_if_str;

    ret_val = 0u;

    if (argc == 3u) {

        for (i = 1u; i < argc; i++) {

            if (*p_argv[i] == NET_CMD_ARG_BEGIN) {

                if (*(p_argv[i] + 1) == NET_CMD_ARG_IF) {

                    i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, &cmd_err);

                    if (cmd_err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                    }
                    if_nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, &cmd_err);
                    if (cmd_err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                    }
                }
            }
        }

        NetIF_Stop(if_nbr, &net_err);

        if (net_err == NET_IF_ERR_NONE) {
             NetCmd_OutputSuccess(out_fnct,
                               p_cmd_param);
        } else if (net_err == NET_IF_ERR_INVALID_STATE) {
            ret_val = NetCmd_OutputError("The interface is already stopped.",
                                           out_fnct,
                                           p_cmd_param);
        } else {
            ret_val = NetCmd_OutputError("Interface cannot be stopped.",
                                         out_fnct,
                                         p_cmd_param);
       }

    } else {
        ret_val = NetCmd_OutputMsg("Usage: net_if_stop [-i interface_nbr]",
                                   DEF_YES,
                                   DEF_YES,
                                   DEF_NO,
                                   out_fnct,
                                   p_cmd_param);
    }
    return ret_val;
}


/*
*********************************************************************************************************
*                                        NetCmd_WiFi_Scan()
*
* Description : Command function to Scan for available WiFi SSID.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
CPU_INT16S NetCmd_WiFi_Scan (CPU_INT16U        argc,
                             CPU_CHAR         *p_argv[],
                             SHELL_OUT_FNCT    out_fnct,
                             SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S              ret_val;
    NET_CMD_WIFI_SCAN_ARG   args;
    NET_CMD_ERR             err;
    NET_IF_WIFI_AP          ap[30];
    CPU_INT16U              ctn;
    CPU_INT16U              i;
    NET_ERR                 net_err;
    CPU_CHAR                str_output[NET_IF_WIFI_STR_LEN_MAX_SSID + 1];
    CPU_INT08U              ssid_len;


    ret_val = 0u;
    if (argc >= 3u) {

        args = NetCmd_WiFiScanCmdArgParse(argc, p_argv, &err);
        switch (err) {
            case NET_CMD_ERR_NONE:
                 break;

            case NET_CMD_ERR_CMD_ARG_INVALID:
            default:
                 ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                 return (ret_val);
        }
    } else {
        ret_val = NetCmd_OutputMsg("Usage: net_wifi_scan wanted_SSID [-i interface_nbr] [-c channel] ",
                                   DEF_YES,
                                   DEF_YES,
                                   DEF_NO,
                                   out_fnct,
                                   p_cmd_param);
       return (ret_val);
    }
                                                                /* ------------ SCAN FOR WIRELESS NETWORKS ------------ */
    ctn = NetIF_WiFi_Scan( args.IF_Nbr,
                           ap,                                   /* Access point table location.                         */
                           30,                                   /* Access point table size.                             */
                          &args.SSID,
                           args.Ch,
                          &net_err);

    if (net_err != NET_IF_WIFI_ERR_NONE) {
          ret_val = NetCmd_OutputError("The Scan has failed.",
                                        out_fnct,
                                        p_cmd_param);
          return (ret_val);
    }

    ret_val = NetCmd_OutputMsg("Number of Access point found: ", DEF_YES, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
    (void)Str_FmtNbr_Int32U(ctn,
                            DEF_INT_32U_NBR_DIG_MAX,
                            DEF_NBR_BASE_DEC,
                            DEF_NULL,
                            DEF_NO,
                            DEF_YES,
                            str_output);

     ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

        if (ctn == 0) {
            return (ret_val);
        }
     ret_val = NetCmd_OutputMsg("    SSID                             BSSID              Ch  Si   Type   Security",
                                DEF_YES,
                                DEF_YES,
                                DEF_NO,
                                out_fnct,
                                p_cmd_param);
     ret_val = NetCmd_OutputMsg(" -------------------------------------------------------------------------------",
                                DEF_NO,
                                DEF_YES,
                                DEF_NO,
                                out_fnct,
                                p_cmd_param);

                                                                /* --------- ANALYSE WIRELESS NETWORKS FOUND ---------- */

    for (i = 0u; i < ctn ; i++) {

        (void)Str_FmtNbr_Int32U(i + 1,
                                2u,
                                DEF_NBR_BASE_DEC,
                                ' ',
                                DEF_NO,
                                DEF_YES,
                                str_output);
        ret_val  = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        ret_val  = NetCmd_OutputMsg("- ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);



        ssid_len = Str_Len(ap[i].SSID.SSID);
        Mem_Copy(str_output, ap[i].SSID.SSID ,ssid_len);
        Mem_Set(&str_output[ssid_len],' ',NET_IF_WIFI_STR_LEN_MAX_SSID - ssid_len);
        str_output[NET_IF_WIFI_STR_LEN_MAX_SSID] = 0u;
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        ret_val = NetCmd_OutputMsg(" ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

        NetASCII_MAC_to_Str(ap[i].BSSID.BSSID,
                            str_output,
                            DEF_YES,
                            DEF_YES,
                           &net_err);
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

        ret_val = NetCmd_OutputMsg(", ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        (void)Str_FmtNbr_Int32U(ap[i].Ch,
                                2u,
                                DEF_NBR_BASE_DEC,
                                ' ',
                                DEF_NO,
                                DEF_YES,
                                str_output);
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        ret_val = NetCmd_OutputMsg(", -", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

        (void)Str_FmtNbr_Int32U(ap[i].SignalStrength,
                                2u,
                                DEF_NBR_BASE_DEC,
                                ' ',
                                DEF_NO,
                                DEF_YES,
                                str_output);
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        ret_val = NetCmd_OutputMsg(", ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);


        if (ap[i].NetType == NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE) {
            Str_Copy(str_output,"INFRA");
        } else if (ap[i].NetType == NET_IF_WIFI_NET_TYPE_ADHOC){
            Str_Copy(str_output,"ADHOC");
        } else {
            Str_Copy(str_output,"UNKWN");
        }
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        ret_val = NetCmd_OutputMsg(", ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

        switch(ap[i].SecurityType){
           case NET_IF_WIFI_SECURITY_OPEN:
                Str_Copy(str_output,"OPEN");
                break;

           case NET_IF_WIFI_SECURITY_WEP:
                Str_Copy(str_output,"WEP");
                 break;

           case NET_IF_WIFI_SECURITY_WPA:
                Str_Copy(str_output,"WPA");
                break;

           case NET_IF_WIFI_SECURITY_WPA2:
                Str_Copy(str_output,"WPA2");
                break;

           default:
                Str_Copy(str_output,"UNKNOWN");
                break;
        }

        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
    }

    return ret_val;
}
#endif


/*
*********************************************************************************************************
*                                        NetCmd_WiFi_Join()
*
* Description : Command function to Join an WiFi Access Point.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
CPU_INT16S NetCmd_WiFi_Join (CPU_INT16U        argc,
                             CPU_CHAR         *p_argv[],
                             SHELL_OUT_FNCT    out_fnct,
                             SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S              ret_val;
    NET_CMD_WIFI_JOIN_ARG   cmd_args;
    NET_CMD_ERR             err;
    NET_ERR                 net_err;


    cmd_args = NetCmd_WiFiJoinCmdArgParse (argc, p_argv, &err);
    switch (err) {
        case NET_CMD_ERR_NONE:
             break;

        case NET_CMD_ERR_CMD_ARG_INVALID:
        default:
            ret_val = NetCmd_OutputMsg("Usage: net_if_join SSID [-p password] [-i interface_nbr] [-t net_type] [-s security_type]",
                                        DEF_YES,
                                        DEF_YES,
                                        DEF_NO,
                                        out_fnct,
                                        p_cmd_param);
             return (ret_val);
    }


    NetIF_WiFi_Join( cmd_args.IF_Nbr,
                     cmd_args.NetType,
                     NET_IF_WIFI_DATA_RATE_AUTO,
                     cmd_args.SecurityType,
                     NET_IF_WIFI_PWR_LEVEL_HI,
                     cmd_args.SSID,
                     cmd_args.PSK,
                    &net_err);
    if (net_err == NET_IF_WIFI_ERR_NONE) {
         NetCmd_OutputSuccess(out_fnct,
                              p_cmd_param);

    } else {
        ret_val = NetCmd_OutputError("Impossible to Join the specified Access Point.",
                                     out_fnct,
                                     p_cmd_param);
    }

    return ret_val;
}
#endif




/*
*********************************************************************************************************
*                                        NetCmd_WiFi_Create()
*
* Description : Command function to Create an WiFi Access point.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
CPU_INT16S NetCmd_WiFi_Create (CPU_INT16U        argc,
                               CPU_CHAR         *p_argv[],
                               SHELL_OUT_FNCT    out_fnct,
                               SHELL_CMD_PARAM  *p_cmd_param)
{

    CPU_INT16S               ret_val;
    NET_CMD_WIFI_CREATE_ARG  cmd_args;
    NET_CMD_ERR              err;
    NET_ERR                  net_err;


    cmd_args = NetCmd_WiFiCreateCmdArgParse (argc, p_argv, &err);
    switch (err) {
        case NET_CMD_ERR_NONE:
             break;

        case NET_CMD_ERR_CMD_ARG_INVALID:
        default:
            ret_val = NetCmd_OutputMsg("Usage: net_if_create SSID [-p password] [-c channel] [-i interface_nbr] [-t net_type] [-s security_type]",
                                        DEF_YES,
                                        DEF_YES,
                                        DEF_NO,
                                        out_fnct,
                                        p_cmd_param);
             return (ret_val);
    }

    NetIF_WiFi_CreateAP( cmd_args.IF_Nbr,
                       cmd_args.NetType,
                       NET_IF_WIFI_DATA_RATE_AUTO,
                       cmd_args.SecurityType,
                       NET_IF_WIFI_PWR_LEVEL_HI,
                       cmd_args.Ch,
                       cmd_args.SSID,
                       cmd_args.PSK,
                      &net_err);

    if (net_err == NET_IF_WIFI_ERR_NONE) {
         NetCmd_OutputSuccess(out_fnct,
                              p_cmd_param);

    } else {
        ret_val  = NetCmd_OutputError("Impossible to Create the specified Access Point.",
                                         out_fnct,
                                       p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
    }

    return ret_val;

}
#endif



/*
*********************************************************************************************************
*                                        NetCmd_WiFi_Leave()
*
* Description : Command function to Leave an WiFi Access point.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
CPU_INT16S  NetCmd_WiFi_Leave (CPU_INT16U        argc,
                               CPU_CHAR         *p_argv[],
                               SHELL_OUT_FNCT    out_fnct,
                               SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S                  ret_val;
    CPU_INT16U                  i;
    CPU_CHAR                   *p_if_str;
    NET_ERR                     net_err;
    NET_CMD_ERR                 err;
    NET_IF_NBR                  if_nbr;


    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:

                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, &err);
                     if (err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                     }

                     if_nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, &err);
                     if (err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                     }
                     break;
            }
        }
    }

    NetIF_WiFi_Leave(if_nbr,&net_err);
    if (net_err == NET_IF_WIFI_ERR_NONE) {
         NetCmd_OutputSuccess(out_fnct,
                              p_cmd_param);

    } else {
        ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
    }

    return ret_val;
}
#endif

/*
*********************************************************************************************************
*                                       NetCmd_WiFi_GetPeerInfo()
*
* Description : Command function to output the peer information when acting as an WiFi Access point.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
CPU_INT16S  NetCmd_WiFi_GetPeerInfo (CPU_INT16U        argc,
                                     CPU_CHAR         *p_argv[],
                                     SHELL_OUT_FNCT    out_fnct,
                                     SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S         ret_val;
    CPU_INT16U         i;
    CPU_CHAR          *p_if_str;
    NET_ERR            net_err;
    NET_CMD_ERR        err;
    NET_IF_NBR         if_nbr;
    NET_IF_WIFI_PEER   buf_peer_info[5];
    CPU_INT16U         ctn;
    CPU_CHAR           str_output[32];


    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:

                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, &err);
                     if (err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                     }

                     if_nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, &err);
                     if (err != NET_CMD_ERR_NONE) {
                        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
                        return (ret_val);
                     }
                     break;
            }
        }
    }

    ctn = NetIF_WiFi_GetPeerInfo( if_nbr,
                                  buf_peer_info,
                                  10,
                                 &net_err);
    if (net_err == NET_IF_WIFI_ERR_NONE) {
        (void)Str_FmtNbr_Int32U(ctn,
                                2u,
                                DEF_NBR_BASE_DEC,
                                ' ',
                                DEF_NO,
                                DEF_YES,
                                str_output);
        ret_val = NetCmd_OutputMsg("Nb of Peer : ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
        ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        for(i = 0 ; i < ctn; i++) {

           (void)Str_FmtNbr_Int32U(i+1,
                                   2u,
                                   DEF_NBR_BASE_DEC,
                                   ' ',
                                   DEF_NO,
                                   DEF_YES,
                                   str_output);
            ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
            ret_val = NetCmd_OutputMsg(" - ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
            NetASCII_MAC_to_Str((CPU_INT08U *) buf_peer_info[i].HW_Addr,
                                               str_output,
                                               DEF_NO,
                                               DEF_YES,
                                             &net_err);
            ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
        }

    } else {
        ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
    }

    return (ret_val);
}
#endif


/*
*********************************************************************************************************
*                                          NetCmd_Sock_Open()
*
* Description : Open a socket
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Open (CPU_INT16U        argc,
                              CPU_CHAR         *p_argv[],
                              SHELL_OUT_FNCT    out_fnct,
                              SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_OPEN_CMD_ARG   cmd_args;
    NET_CMD_SOCK_OPEN_ARG       args;
    NET_SOCK_ID                 sock_id;
    CPU_INT16S                  ret_val = 0;
    NET_CMD_ERR                 err;
    NET_ERR                     net_err;


    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_OpenCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val += NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_OpenCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val += NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    sock_id = NetSock_Open(args.Family, args.Type, NET_SOCK_PROTOCOL_DFLT, &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        ret_val += NetCmd_OutputMsg("Unable to open a socket ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        goto exit;

    } else {

        ret_val += NetCmd_OutputMsg("Socket opened: ", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputSockID(sock_id, out_fnct, p_cmd_param);
    }

    ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                          NetCmd_Sock_Close()
*
* Description : Close a socket
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Close (CPU_INT16U        argc,
                               CPU_CHAR         *p_argv[],
                               SHELL_OUT_FNCT    out_fnct,
                               SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_ID_CMD_ARG  cmd_args;
    NET_CMD_SOCK_ID_ARG      args;
    CPU_INT16S               ret_val = 0;
    NET_CMD_ERR              err;
    NET_ERR                  net_err;



    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_ID_CmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_ID_CmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    NetSock_Close(args.SockID, &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to close the socket ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);

    } else {
        ret_val  = NetCmd_OutputMsg("Socket Closed", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);
    }


exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                          NetCmd_Sock_Bind()
*
* Description : Bind a socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Bind (CPU_INT16U        argc,
                              CPU_CHAR         *p_argv[],
                              SHELL_OUT_FNCT    out_fnct,
                              SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_SOCK_ADDR               sock_addr;
    NET_SOCK_ADDR_FAMILY        addr_family;
    NET_SOCK_ADDR_LEN           sock_addr_len = sizeof(sock_addr);
    CPU_INT08U                 *p_addr;
#ifdef  NET_IPv4_MODULE_EN
    NET_IPv4_ADDR               addr;
#endif
    NET_IP_ADDR_LEN             addr_len;
    NET_CMD_SOCK_BIND_CMD_ARG   cmd_args;
    NET_CMD_SOCK_BIND_ARG       args;
    CPU_INT16S                  ret_val = 0;
    NET_CMD_ERR                 err;
    NET_ERR                     net_err;



    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_BindCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_BindCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    switch (args.Family) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_SOCK_PROTOCOL_FAMILY_IP_V4:
             addr_family =  NET_SOCK_ADDR_FAMILY_IP_V4;
             addr        =  NET_IPv4_ADDR_ANY;
             p_addr      = (CPU_INT08U *)&addr;
             addr_len    =  NET_IPv4_ADDR_SIZE;
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_SOCK_PROTOCOL_FAMILY_IP_V6:
             addr_family =  NET_SOCK_ADDR_FAMILY_IP_V6;
             p_addr      = (CPU_INT08U *)&NET_IPv6_ADDR_ANY;
             addr_len    =  NET_IPv6_ADDR_SIZE;
             break;
#endif

        default:
             ret_val += NetCmd_OutputError("Fault", out_fnct, p_cmd_param);
             goto exit;
    }

    NetApp_SetSockAddr(&sock_addr,
                        addr_family,
                        args.Port,
                        p_addr,
                        addr_len,
                       &net_err);
    if (net_err != NET_APP_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to set the address ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        goto exit;
    }



    NetSock_Bind(args.SockID, &sock_addr, sock_addr_len, &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to Bind the socket ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        goto exit;

    } else {
        ret_val  = NetCmd_OutputMsg("Socket Binded", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);
    }

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);

    return ret_val;
}


/*
*********************************************************************************************************
*                                         NetCmd_Sock_Listen()
*
* Description : Listen on a socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Listen (CPU_INT16U        argc,
                                CPU_CHAR         *p_argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_LISTEN_CMD_ARG  cmd_args;
    NET_CMD_SOCK_LISTEN_ARG      args;
    CPU_INT16S                   ret_val = 0;
    NET_CMD_ERR                  err;
    NET_ERR                      net_err;


    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_ListenCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_ListenCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }


    NetSock_Listen(args.SockID, args.QueueSize, &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to listen on the socket ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);

    } else {
        ret_val  = NetCmd_OutputMsg("Listening", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);
    }


exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);
    return ret_val;
}


/*
*********************************************************************************************************
*                                         NetCmd_Sock_Accept()
*
* Description : Accept connection from a socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Accept (CPU_INT16U        argc,
                                CPU_CHAR         *p_argv[],
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_ID_CMD_ARG  cmd_args;
    NET_CMD_SOCK_ID_ARG      args;
    NET_SOCK_ADDR            sock_addr;
    NET_SOCK_ADDR_LEN        addr_len;
    NET_SOCK_ID              sock_id;
    CPU_INT16S               ret_val = 0;
    NET_CMD_ERR              err;
    NET_ERR                  net_err;



    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_ID_CmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_ID_CmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }


    addr_len = sizeof(sock_addr);
    sock_id = NetSock_Accept(args.SockID, &sock_addr, &addr_len, &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to accept connection ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        goto exit;

    } else {
        ret_val  = NetCmd_OutputMsg("Connection accepted: ", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputSockID(sock_id, out_fnct, p_cmd_param);
    }

    ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);
    return ret_val;
}


/*
*********************************************************************************************************
*                                          NetCmd_Sock_Conn()
*
* Description : Connect a socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Conn (CPU_INT16U        argc,
                              CPU_CHAR         *p_argv[],
                              SHELL_OUT_FNCT    out_fnct,
                              SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_CONN_CMD_ARG  cmd_args;
    NET_CMD_SOCK_CONN_ARG      args;
    NET_SOCK_ADDR              sock_addr;
    NET_SOCK_ADDR_FAMILY       addr_family;
    CPU_INT16S                 ret_val = 0;
    NET_CMD_ERR                err;
    NET_ERR                    net_err;



    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_ConnCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_ConnCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    switch (args.AddrLen) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_IPv4_ADDR_SIZE:
             addr_family =  NET_SOCK_ADDR_FAMILY_IP_V4;
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_IPv6_ADDR_SIZE:
             addr_family =  NET_SOCK_ADDR_FAMILY_IP_V6;
             break;
#endif

        default:
             ret_val += NetCmd_OutputError("Fault", out_fnct, p_cmd_param);
             goto exit;
    }

    NetApp_SetSockAddr(&sock_addr,
                        addr_family,
                        args.Port,
          (CPU_INT08U *)args.Addr,
                        args.AddrLen,
                       &net_err);
    if (net_err != NET_APP_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to set the address ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        goto exit;
    }


    NetSock_Conn(args.SockID, &sock_addr, sizeof(sock_addr), &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        ret_val  = NetCmd_OutputMsg("Unable to accept connection ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
        ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
        goto exit;

    } else {
        ret_val  = NetCmd_OutputMsg("Connected", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
    }

    ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);
    return ret_val;
}



/*
*********************************************************************************************************
*                                           NetCmd_Sock_Rx()
*
* Description : Receive from a socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Rx (CPU_INT16U        argc,
                            CPU_CHAR         *p_argv[],
                            SHELL_OUT_FNCT    out_fnct,
                            SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_RX_CMD_ARG  cmd_args;
    NET_CMD_SOCK_RX_ARG      args;
    CPU_INT08U               rx_buf[1472];
    CPU_INT16U               rx_len     = 1472u;
    CPU_INT16U               rx_len_tot = 0u;
    CPU_INT16S               ret_val    = 0;
    NET_CMD_ERR              err;
    NET_ERR                  net_err;


    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_RxCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_RxCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    while (rx_len_tot < args.DataLen) {
        CPU_INT16U  len_rem = args.DataLen - rx_len_tot;
        CPU_INT32S  len     = 0;


        if (len_rem > 1472u) {
            rx_len = 1472u;
        } else {
            rx_len = len_rem;
        }

        len = NetSock_RxData(args.SockID, rx_buf, rx_len, NET_SOCK_FLAG_NONE, &net_err);
        switch (net_err) {
            case NET_SOCK_ERR_NONE:
                 ret_val    += NetCmd_OutputData(rx_buf, len, args.OutputFmt, out_fnct, p_cmd_param);
                 rx_len_tot += len;
                 break;

            case NET_SOCK_ERR_RX_Q_EMPTY:
                 KAL_Dly(1);
                 break;

            default:
                ret_val  = NetCmd_OutputMsg("Receive error ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
                ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
                goto exit;
        }
    }


    NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);
    return (ret_val);
}



/*
*********************************************************************************************************
*                                           NetCmd_Sock_Tx()
*
* Description : Transmit on a socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_Sock_Tx (CPU_INT16U        argc,
                            CPU_CHAR         *p_argv[],
                            SHELL_OUT_FNCT    out_fnct,
                            SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_TX_CMD_ARG  cmd_args;
    NET_CMD_SOCK_TX_ARG      args;
    CPU_INT16U               tx_len_tot = 0u;
    CPU_INT16S               ret_val    = 0;
    NET_CMD_ERR              err;
    NET_ERR                  net_err;


    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_TxCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_TxCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    while (tx_len_tot < args.DataLen) {
        CPU_INT16U   len_rem = args.DataLen - tx_len_tot;
        CPU_INT08U  *p_data  = args.DataPtr + tx_len_tot;



        tx_len_tot += NetSock_TxData(args.SockID, p_data, len_rem, NET_SOCK_FLAG_NONE, &net_err);
        switch (net_err) {
            case NET_SOCK_ERR_NONE:
                 break;

            case NET_SOCK_ERR_RX_Q_EMPTY:
                 KAL_Dly(1);
                 break;

            default:
                ret_val  = NetCmd_OutputMsg("Receive error ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
                ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
                goto exit;
        }
    }

    ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);
    return (ret_val);
}


/*
*********************************************************************************************************
*                                           NetCmd_Sock_Sel()
*
* Description : Do a select on many socket.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : Shell.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if 0
CPU_INT16S  NetCmd_Sock_Sel (CPU_INT16U        argc,
                             CPU_CHAR         *p_argv[],
                             SHELL_OUT_FNCT    out_fnct,
                             SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_SOCK_SEL_CMD_ARG  cmd_args;
    NET_CMD_SOCK_SEL_ARG      args;
    CPU_INT16S                ret_val = 0;
    CPU_INT16S                sock_ready_ctr;
    CPU_INT16U                i;
    NET_CMD_ERR               err;
    NET_ERR                   net_err;


    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_SelCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_SelCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }



    sock_ready_ctr = NetSock_Sel(args.SockNbrMax, &args.DescRd, &args.DescWr, &args.DescErr, &args.Timeout, &net_err);
    switch (net_err) {
        case NET_SOCK_ERR_NONE:
             ret_val += NetCmd_OutputMsg("No Error,  Number of ready socket = ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
             ret_val += NetCmd_OutputInt32U(sock_ready_ctr, out_fnct, p_cmd_param);

             ret_val += NetCmd_OutputMsg("Read Sockets: ", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
             for (i = 0; i <= args.SockNbrMax; i++) {
                 if (NET_SOCK_DESC_IS_SET(i, (NET_SOCK_DESC *)&args.DescRd)) {
                     ret_val += NetCmd_OutputInt32U(sock_ready_ctr, out_fnct, p_cmd_param);
                     ret_val += NetCmd_OutputMsg(", ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
                 }
             }

             ret_val += NetCmd_OutputMsg("Write Sockets: ", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
             for (i = 0; i <= args.SockNbrMax; i++) {
                 if (NET_SOCK_DESC_IS_SET(i, &args.DescWr)) {
                     ret_val += NetCmd_OutputInt32U(sock_ready_ctr, out_fnct, p_cmd_param);
                     ret_val += NetCmd_OutputMsg(", ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
                 }
             }

             ret_val += NetCmd_OutputMsg("Error Sockets: ", DEF_YES, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
             for (i = 0; i <= args.SockNbrMax; i++) {
                 if (NET_SOCK_DESC_IS_SET(i, &args.DescErr)) {
                     ret_val += NetCmd_OutputInt32U(sock_ready_ctr, out_fnct, p_cmd_param);
                     ret_val += NetCmd_OutputMsg(", ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
                 }
             }
             break;

        default:
            ret_val  = NetCmd_OutputMsg("Select error ", DEF_YES, DEF_NO, DEF_YES, out_fnct, p_cmd_param);
            ret_val += NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
            goto exit;
    }


    ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    NetCmd_OutputEnd(out_fnct, p_cmd_param);
    return (ret_val);
}
#endif


CPU_INT16S  NetCmd_SockOptSetChild (CPU_INT16U        argc,
                                    CPU_CHAR         *p_argv[],
                                    SHELL_OUT_FNCT    out_fnct,
                                    SHELL_CMD_PARAM  *p_cmd_param)
{
    NET_CMD_ERR               err;
    NET_CMD_SOCK_OPT_CMD_ARG  cmd_args;
    NET_CMD_SOCK_OPT_ARG      args;
    CPU_INT16S                ret_val = 0;
    NET_ERR                   err_net;

    NetCmd_OutputBeginning(out_fnct, p_cmd_param);

    cmd_args = NetCmd_Sock_OptCmdParse(argc, p_argv, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }

    args = NetCmd_Sock_OptCmdTranslate(cmd_args, &err);
    if (err != NET_CMD_ERR_NONE) {
        ret_val = NetCmd_OutputCmdArgInvalid(out_fnct, p_cmd_param);
        goto exit;
    }


    NetSock_CfgConnChildQ_SizeSet(args.SockID, args.Value, &err_net);
    if (err_net != NET_SOCK_ERR_NONE) {
        ret_val += NetCmd_OutputErrorNetNbr(err_net, out_fnct, p_cmd_param);
        goto exit;
    }

    ret_val += NetCmd_OutputSuccess(out_fnct, p_cmd_param);

exit:
    return (ret_val);
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
*                                       NetCmd_ResetCmdArgParse()
*
* Description : Parse Reset command arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Reset argument parsed.
*
* Caller(s)   : NetCmd_IF_Reset().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_RESET_CMD_ARG  NetCmd_ResetCmdArgParse (CPU_INT16U    argc,
                                                        CPU_CHAR     *p_argv[],
                                                        NET_CMD_ERR  *p_err)
{
    NET_CMD_RESET_CMD_ARG  cmd_args;
    CPU_INT16U              i;


    cmd_args.IF_NbrPtr = DEF_NULL;
    cmd_args.IPv4_En   = DEF_NO;
    cmd_args.IPv6_En   = DEF_NO;

    if (argc > 5) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.IF_NbrPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                case NET_CMD_ARG_IPv4:
                     cmd_args.IPv4_En = DEF_YES;
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_args);
                     }
                     break;


                case NET_CMD_ARG_IPv6:
                     cmd_args.IPv6_En = DEF_YES;
                     break;


                default:
                    *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

    if ((cmd_args.IPv4_En == DEF_NO) &&
        (cmd_args.IPv6_En == DEF_NO)) {
         cmd_args.IPv4_En = DEF_YES;
         cmd_args.IPv6_En = DEF_YES;
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                        NetCmd_ResetTranslate()
*
* Description : Translate reset argument.
*
* Argument(s) : cmd_arg     Pointer to the reset argument to translate.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Reset argument converted.
*
* Caller(s)   : NetCmd_IF_Reset().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_RESET_ARG  NetCmd_ResetTranslate (NET_CMD_RESET_CMD_ARG   cmd_args,
                                                  NET_CMD_ERR            *p_err)
{
    NET_CMD_RESET_ARG  args;


    args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.IF_NbrPtr, p_err);

    if (cmd_args.IPv4_En == DEF_YES) {
        args.IPv4_En = DEF_YES;
    } else {
        args.IPv4_En = DEF_NO;
    }


    if (cmd_args.IPv6_En == DEF_YES) {
        args.IPv6_En = DEF_YES;
    } else {
        args.IPv6_En = DEF_NO;
    }


    return (args);
}


/*
*********************************************************************************************************
*                                      NetCmd_MTU_CmdArgParse()
*
* Description : Parse set MTU command arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Set MTU argument parsed.
*
* Caller(s)   : NetCmd_IF_SetMTU().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_MTU_CMD_ARG  NetCmd_MTU_CmdArgParse (CPU_INT16U    argc,
                                                     CPU_CHAR     *p_argv[],
                                                     NET_CMD_ERR  *p_err)
{
    NET_CMD_MTU_CMD_ARG  cmd_args;
    CPU_INT16U              i;


       cmd_args.IF_NbrPtr = DEF_NULL;
       cmd_args.MTU_Ptr   = DEF_NULL;


       for (i = 1; i < argc; i++) {
           if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
               switch (*(p_argv[i] + 1)) {
                   case NET_CMD_ARG_IF:
                        i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.IF_NbrPtr, p_err);
                        if (*p_err != NET_CMD_ERR_NONE) {
                             return (cmd_args);
                        }
                        break;


                   case NET_CMD_ARG_MTU:
                        i += NetCmd_ArgsParserParseMTU(&p_argv[i], &cmd_args.MTU_Ptr, p_err);
                        if (*p_err != NET_CMD_ERR_NONE) {
                            return (cmd_args);
                        }
                        break;


                   default:
                       *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
                        return (cmd_args);
               }
           }
       }


      *p_err = NET_CMD_ERR_NONE;

       return (cmd_args);
}


/*
*********************************************************************************************************
*                                      NetCmd_MTU_Translate()
*
* Description : Translate set MTU argument.
*
* Argument(s) : cmd_arg     Pointer to the set MTU argument to translate.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Set MTU argument converted.
*
* Caller(s)   : NetCmd_IF_SetMTU().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_MTU_ARG  NetCmd_MTU_Translate (NET_CMD_MTU_CMD_ARG   cmd_args,
                                               NET_CMD_ERR          *p_err)
{
    NET_CMD_MTU_ARG  args;


        args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.IF_NbrPtr, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
             return (args);
        }

        args.MTU = NetCmd_ArgsParserTranslateMTU(cmd_args.MTU_Ptr, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
             return (args);
        }

        return (args);
}


/*
*********************************************************************************************************
*                                       NetCmd_RouteCmdArgParse()
*
* Description : Parse Route command arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Route argument parsed.
*
* Caller(s)   : NetCmd_IF_RouteAdd(),
*               NetCmd_IF_RouteRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_ROUTE_CMD_ARG  NetCmd_RouteCmdArgParse (CPU_INT16U    argc,
                                                        CPU_CHAR     *p_argv[],
                                                        NET_CMD_ERR  *p_err)
{
    NET_CMD_ROUTE_CMD_ARG  cmd_arg;
    CPU_INT16U              i;


    cmd_arg.IF_NbrPtr         = DEF_NULL;
    cmd_arg.IPv4.HostPtr      = DEF_NULL;
    cmd_arg.IPv4.MaskPtr      = DEF_NULL;
    cmd_arg.IPv4.GatewayPtr   = DEF_NULL;
    cmd_arg.IPv6.HostPtr      = DEF_NULL;
    cmd_arg.IPv6.PrefixLenPtr = DEF_NULL;

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_arg.IF_NbrPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_IPv4:
                     i += NetCmd_ArgsParserParseIPv4(&p_argv[i], &cmd_arg.IPv4, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_IPv6:
                     i += NetCmd_ArgsParserParseIPv6(&p_argv[i], &cmd_arg.IPv6, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_arg);
                     }
                     break;


                default:
                    *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
                     return (cmd_arg);
            }
        }
    }


   *p_err = NET_CMD_ERR_NONE;

    return (cmd_arg);
}

/*
*********************************************************************************************************
*                                        NetCmd_RouteTranslate()
*
* Description : Translate route argument.
*
* Argument(s) : cmd_arg     Pointer to the set MTU argument to translate.
*
*               p_err       Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : ROUTE argument converted.
*
* Caller(s)   : NetCmd_IF_RouteAdd(),
*               NetCmd_IF_RouteRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_ROUTE_ARG  NetCmd_RouteTranslate (NET_CMD_ROUTE_CMD_ARG   cmd_args,
                                                  NET_CMD_ERR            *p_err)
{
    NET_CMD_ROUTE_ARG  args;


    args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.IF_NbrPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         return (args);
    }

#ifdef  NET_IPv4_MODULE_EN
    if (cmd_args.IPv4.HostPtr != DEF_NULL) {
        args.IPv4Cfg = NetCmd_ArgsParserTranslateIPv4(&cmd_args.IPv4, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
            return (args);
        }

        args.IPv4_En = DEF_YES;

    } else {
        args.IPv4_En = DEF_NO;
    }
#endif

#ifdef  NET_IPv6_MODULE_EN
    if (cmd_args.IPv6.HostPtr != DEF_NULL) {
        args.IPv6Cfg = NetCmd_ArgsParserTranslateIPv6(&cmd_args.IPv6, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
            return (args);
        }

        args.IPv6_En = DEF_YES;

    } else {
        args.IPv6_En = DEF_NO;
    }
#endif

    return (args);
}


/*
*********************************************************************************************************
*                                            NetCmd_Ping4()
*
* Description : Ping using an IPv4 address.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : NetCmd_Ping().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16S  NetCmd_Ping4 (NET_IPv4_ADDR    *p_addr_remote,
                                  void             *p_data,
                                  CPU_INT16U        data_len,
                                  CPU_INT32U        cnt,
                                  SHELL_OUT_FNCT    out_fnct,
                                  SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S          ret_val;
#ifdef  NET_IPv4_MODULE_EN
    CPU_INT32U          ix;
    NET_ERR             net_err;


    for (ix = 0 ; ix < cnt ; ix++) {
        (void)NetICMP_TxEchoReq((void *) p_addr_remote,
                                         sizeof(NET_IPv4_ADDR),
                                         1000,
                                (void *) p_data,
                                         data_len,
                                        &net_err);
    }

    switch(net_err) {
        case NET_ICMP_ERR_NONE:
             ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);
             break;

        case NET_ICMP_ERR_SIGNAL_TIMEOUT:
             ret_val = NetCmd_OutputError("Timeout", out_fnct, p_cmd_param);
             return (ret_val);

        case NET_ERR_IF_LINK_DOWN:
             ret_val = NetCmd_OutputError("Link down", out_fnct, p_cmd_param);
             return (ret_val);

        default:
             ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
             return (ret_val);
    }


    return (ret_val);
#else
    ret_val = out_fnct("IXANVL ping: IPv4 not present\n\rFAILED\n\r\n\r",
                        43,
                        p_cmd_param->pout_opt);
    return (ret_val);
#endif
}


/*
*********************************************************************************************************
*                                            NetCmd_Ping6()
*
* Description : Ping using an IPv6 address.
*
* Argument(s) : argc            is a count of the arguments supplied.
*
*               p_argv          an array of pointers to the strings which are those arguments.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED,                 if implemented connection closed
*
*               SHELL_OUT_ERR,                                  otherwise
*
* Caller(s)   : NetCmd_Ping().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16S  NetCmd_Ping6 (NET_IPv6_ADDR    *p_addr_remote,
                                  void             *p_data,
                                  CPU_INT16U        data_len,
                                  SHELL_OUT_FNCT    out_fnct,
                                  SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S          ret_val;
#ifdef  NET_IPv6_MODULE_EN
    NET_ERR             net_err;


    (void)NetICMP_TxEchoReq((CPU_INT08U *)p_addr_remote,
                                          sizeof(NET_IPv6_ADDR),
                                          1000u,
                                          p_data,
                                          data_len,
                                         &net_err);
    switch(net_err) {
        case NET_ICMP_ERR_NONE:
             ret_val = NetCmd_OutputSuccess(out_fnct, p_cmd_param);
             break;

        case NET_ICMP_ERR_SIGNAL_TIMEOUT:
             ret_val = NetCmd_OutputError("Timeout", out_fnct, p_cmd_param);
             return (ret_val);

        case NET_ERR_IF_LINK_DOWN:
             ret_val = NetCmd_OutputError("Link down", out_fnct, p_cmd_param);
             return (ret_val);

        default:
             ret_val = NetCmd_OutputErrorNetNbr(net_err, out_fnct, p_cmd_param);
             return (ret_val);
    }


#else
    ret_val = NetCmd_OutputError("IPv6 not present", out_fnct, p_cmd_param);
#endif

    (void)&p_addr_remote;
    (void)&p_data;
    (void)&data_len;

    return (ret_val);
}

/*
*********************************************************************************************************
*                                        NetCmd_AutoCfgResult()
*
* Description : Hook function called when auto-config is completed.
*
* Argument(s) : if_nbr          Interface number.
*
*               autocfg_result  Auto configuration result.
*
* Return(s)   : None.
*
* Caller(s)   : Referenced by NetCmd_Start().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  void  NetCmd_AutoCfgResult (       NET_IF_NBR                 if_nbr,
                                    const  NET_IPv6_ADDR             *p_addr_local,
                                    const  NET_IPv6_ADDR             *p_addr_global,
                                           NET_IPv6_AUTO_CFG_STATUS   autocfg_result)
{
    CPU_BOOLEAN   check;


    if (p_addr_local != DEF_NULL) {
        check = NetIPv6_IsAddrCfgdValidHandler(p_addr_local);
        if (check != DEF_OK) {
            NET_CMD_TRACE_INFO(("ERROR!"));
        }
    }

    if (p_addr_global != DEF_NULL) {
        check = NetIPv6_IsAddrCfgdValidHandler(p_addr_global);
        if (check != DEF_OK) {
            NET_CMD_TRACE_INFO(("ERROR!"));
        }
    }

    switch (autocfg_result) {
        case NET_IPv6_AUTO_CFG_STATUS_FAILED:
             NET_CMD_TRACE_INFO(("Auto-Configuration failed.\n"));
             break;

        case NET_IPv6_AUTO_CFG_STATUS_SUCCEEDED:
             NET_CMD_TRACE_INFO(("Auto-Configuration succeeded.\n"));
             break;

        case NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL:
             NET_CMD_TRACE_INFO(("Auto-Configuration with Link-Local address only.\n"));
             break;

        default:
             NET_CMD_TRACE_INFO(("Unknown Auto-Configuration result.\n"));
             break;
    }

   (void)&if_nbr;
}
#endif


/*
*********************************************************************************************************
*                                       NetCmd_WiFiScanCmdArgParse ()
*
* Description : Parse WiFi Scan command arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : WiFi Scan argument parsed.
*
* Caller(s)   : NetCmd_WiFiScanCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
static  NET_CMD_WIFI_SCAN_ARG  NetCmd_WiFiScanCmdArgParse (CPU_INT16U    argc,
                                                           CPU_CHAR     *p_argv[],
                                                           NET_CMD_ERR  *p_err)
{
    NET_CMD_WIFI_SCAN_ARG  cmd_args;
    CPU_BOOLEAN            is_graph;
    CPU_INT16U             i;
    CPU_CHAR              *p_if_str;
    CPU_INT32U             ch;
    CPU_INT08U             j;

    cmd_args.Ch     = NET_IF_WIFI_CH_ALL;
    cmd_args.IF_Nbr = 0u;
    Mem_Set(cmd_args.SSID.SSID, 0x00, NET_IF_WIFI_STR_LEN_MAX_SSID);

    if (argc > 10) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:

                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }

                     cmd_args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_WIFI_CHANNEL:
                     ch = Str_ParseNbr_Int32U(p_argv[i + 1] , DEF_NULL, DEF_NBR_BASE_DEC);
                     if ((ch > 0u) && (ch < 14u)){
                        cmd_args.Ch = ch;
                     } else {
                        cmd_args.Ch = 0u;
                       *p_err       = NET_CMD_ERR_CMD_ARG_INVALID;
                        return (cmd_args);

                     }
                     i++;
                     break;
             }

        } else {

            if (cmd_args.SSID.SSID[0] != DEF_NULL) {
               *p_err       = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }


            j = 0;
            while (j < NET_IF_WIFI_STR_LEN_MAX_SSID +1) {
                is_graph = ASCII_IS_GRAPH(*(p_argv[i] + j));
                if (is_graph == DEF_FALSE){
                    break;
                }
                j++;
            }


            if (j < NET_IF_WIFI_STR_LEN_MAX_SSID ) {
                Mem_Copy(cmd_args.SSID.SSID, p_argv[i], j );
            } else {
               *p_err       = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}
#endif



/*
*********************************************************************************************************
*                                     NetCmd_WiFiJoinCmdArgParse()
*
* Description : Parse WiFi Join command arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : WiFi Join argument parsed.
*
* Caller(s)   : NetCmd_WiFi_Join ().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
static  NET_CMD_WIFI_JOIN_ARG  NetCmd_WiFiJoinCmdArgParse (CPU_INT16U    argc,
                                                           CPU_CHAR     *p_argv[],
                                                           NET_CMD_ERR  *p_err)
{
    NET_CMD_WIFI_JOIN_ARG       cmd_args;
    CPU_BOOLEAN                 is_graph;
    CPU_INT16U                  i;
    CPU_CHAR                   *p_if_str;
    CPU_INT08U                  j;
    CPU_INT32U                  net_type_result;
    CPU_INT32U                  security_type_result;

    cmd_args.IF_Nbr = 0u;
    Mem_Set(cmd_args.SSID.SSID, 0x00, NET_IF_WIFI_STR_LEN_MAX_SSID);

    if ((argc > 10) || (argc <= 3)) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }

                     cmd_args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                case NET_CMD_ARG_WIFI_PSK:
                     while (j < NET_IF_WIFI_STR_LEN_MAX_PSK +1) {
                        is_graph = ASCII_IS_GRAPH(*(p_argv[i+1] + j));
                        if (is_graph == DEF_FALSE){
                            break;
                        }
                        j++;
                     }
                     if (j < NET_IF_WIFI_STR_LEN_MAX_PSK ) {
                        Mem_Copy(cmd_args.PSK.PSK, p_argv[i+1], j );
                     } else {
                       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                        return (cmd_args);
                     }
                     i++;
                     break;


                case NET_CMD_ARG_WIFI_NET_TYPE:
                     net_type_result = NetCmd_DictionaryGet(                   NetCmd_DictionaryNetType,
                                                                               sizeof(NetCmd_DictionaryNetType),
                                                            (const CPU_CHAR *) p_argv[i+1],
                                                                               NET_CMD_NET_TYPE_STR_LEN  );

                    if (net_type_result != NET_CMD_DICTIONARY_KEY_INVALID) {
                        cmd_args.NetType = net_type_result;
                    } else {
                       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                        return (cmd_args);
                    }
                    i++;
                    break;


                case NET_CMD_ARG_WIFI_SECURITY_TYPE:
                     security_type_result = NetCmd_DictionaryGet(                   NetCmd_DictionarySecurityType,
                                                                                    sizeof(NetCmd_DictionarySecurityType),
                                                                 (const CPU_CHAR *) p_argv[i+1],
                                                                                    NET_CMD_SECURITY_TYPE_STR_LEN  );

                    if (security_type_result != NET_CMD_DICTIONARY_KEY_INVALID) {
                        cmd_args.SecurityType = security_type_result;
                    } else {
                        *p_err       = NET_CMD_ERR_CMD_ARG_INVALID;
                        return (cmd_args);
                    }
                    i++;
                     break;
             }

        } else {
            if (cmd_args.SSID.SSID[0] != DEF_NULL) {
               *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }
            j = 0;
            while (j < NET_IF_WIFI_STR_LEN_MAX_SSID +1) {
                is_graph = ASCII_IS_GRAPH(*(p_argv[i] + j));
                if (is_graph == DEF_FALSE) {
                    break;
                }
                j++;
            }
            if (j < NET_IF_WIFI_STR_LEN_MAX_SSID ) {
                Mem_Copy(cmd_args.SSID.SSID, p_argv[i], j );
            } else {
               *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}
#endif



/*
*********************************************************************************************************
*                                    NetCmd_WiFiCreateCmdArgParse()
*
* Description : Parse WiFi Create command arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : WiFi Create argument parsed.
*
* Caller(s)   : NetCmd_WiFi_Create().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
static  NET_CMD_WIFI_CREATE_ARG  NetCmd_WiFiCreateCmdArgParse (CPU_INT16U    argc,
                                                               CPU_CHAR     *p_argv[],
                                                               NET_CMD_ERR  *p_err)
{
    NET_CMD_WIFI_CREATE_ARG     cmd_args;
    CPU_BOOLEAN                 is_graph;
    CPU_INT16U                  i;
    CPU_CHAR                   *p_if_str;
    CPU_INT08U                  j;
    CPU_INT32U                  net_type_result;
    CPU_INT32U                  security_type_result;
    CPU_INT32U                  ch;

    cmd_args.IF_Nbr = 0u;
    Mem_Set(cmd_args.SSID.SSID, 0x00, NET_IF_WIFI_STR_LEN_MAX_SSID);

    if ((argc > 15) || (argc <= 3)) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_IF:

                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &p_if_str, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }

                     cmd_args.IF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(p_if_str, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_WIFI_PSK:
                     while (j < NET_IF_WIFI_STR_LEN_MAX_PSK +1) {
                        is_graph = ASCII_IS_GRAPH(*(p_argv[i+1] + j));
                        if (is_graph == DEF_FALSE){
                            break;
                        }
                        j++;
                     }
                     if (j < NET_IF_WIFI_STR_LEN_MAX_PSK ) {
                         Mem_Copy(cmd_args.PSK.PSK, p_argv[i+1], j );
                         cmd_args.PSK.PSK[j] = 0;
                     } else {
                        *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                         return (cmd_args);
                     }
                     i++;
                     break;

                case NET_CMD_ARG_WIFI_NET_TYPE:
                     net_type_result = NetCmd_DictionaryGet(                   NetCmd_DictionaryNetType,
                                                                               sizeof(NetCmd_DictionaryNetType),
                                                            (const CPU_CHAR *) p_argv[i+1],
                                                                               NET_CMD_NET_TYPE_STR_LEN  );

                    if (net_type_result != NET_CMD_DICTIONARY_KEY_INVALID) {
                        cmd_args.NetType = net_type_result;
                    } else {
                       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                        return (cmd_args);
                    }
                    i++;
                    break;

                case NET_CMD_ARG_WIFI_SECURITY_TYPE:
                     security_type_result = NetCmd_DictionaryGet(                   NetCmd_DictionarySecurityType,
                                                                                    sizeof(NetCmd_DictionarySecurityType),
                                                                 (const CPU_CHAR *) p_argv[i+1],
                                                                                    NET_CMD_SECURITY_TYPE_STR_LEN  );

                    if (security_type_result != NET_CMD_DICTIONARY_KEY_INVALID) {
                        cmd_args.SecurityType = security_type_result;
                    } else {
                        *p_err       = NET_CMD_ERR_CMD_ARG_INVALID;
                        return (cmd_args);
                    }
                    i++;
                     break;

                case NET_CMD_ARG_WIFI_CHANNEL:
                     ch = Str_ParseNbr_Int32U(p_argv[i + 1] , DEF_NULL, DEF_NBR_BASE_DEC);
                     if ((ch > 0u) && (ch < 14u)){
                         cmd_args.Ch = ch;
                     } else {
                         cmd_args.Ch = 0u;
                        *p_err       = NET_CMD_ERR_CMD_ARG_INVALID;
                         return (cmd_args);

                     }
                     i++;
                     break;
             }

        } else {

            if (cmd_args.SSID.SSID[0] != DEF_NULL) {
               *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }


            j = 0;
            while (j < NET_IF_WIFI_STR_LEN_MAX_SSID +1) {
                is_graph = ASCII_IS_GRAPH(*(p_argv[i] + j));
                if (is_graph == DEF_FALSE) {
                    break;
                }
                j++;
            }


            if (j < NET_IF_WIFI_STR_LEN_MAX_SSID ) {
                Mem_Copy(cmd_args.SSID.SSID, p_argv[i], j );
            } else {
               *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}
#endif


/*
*********************************************************************************************************
*                                      NetCmd_Sock_OpenCmdParse()
*
* Description : Parse sock open command arguments
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Socket open argument parsed
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_SOCK_OPEN_CMD_ARG  NetCmd_Sock_OpenCmdParse (CPU_INT16U        argc,
                                                             CPU_CHAR         *p_argv[],
                                                             NET_CMD_ERR      *p_err)
{
    NET_CMD_SOCK_OPEN_CMD_ARG  cmd_args;
    CPU_INT16U             i;



    Mem_Clr(&cmd_args, sizeof(cmd_args));

    if (argc > 10) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {


        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            CPU_CHAR  *p_letter = p_argv[i];

            p_letter++;
            i++;
            switch (*p_letter) {
                case NET_CMD_ARG_SOCK_FAMILY:
                     NetCmd_ArgsParserParseSockFamily(&p_argv[i], &cmd_args.FamilyPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_SOCK_TYPE:
                     NetCmd_ArgsParserParseSockType(&p_argv[i], &cmd_args.TypePtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                default:
                     break;
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;


    return (cmd_args);
}


/*
*********************************************************************************************************
*                                    NetCmd_Sock_OpenCmdTranslate()
*
* Description : Translate socket open command argument
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_OPEN_ARG  NetCmd_Sock_OpenCmdTranslate (NET_CMD_SOCK_OPEN_CMD_ARG   cmd_args,
                                                             NET_CMD_ERR                *p_err)
{
    NET_CMD_SOCK_OPEN_ARG  arg;


    arg.Family = NetCmd_ArgsParserTranslateSockFamily(cmd_args.FamilyPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
        goto exit;
    }


    arg.Type = NetCmd_ArgsParserTranslateSockType(cmd_args.TypePtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
        goto exit;
    }

exit:
    return (arg);
}



/*
*********************************************************************************************************
*                                       NetCmd_Sock_ID_CmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_ID_CMD_ARG  NetCmd_Sock_ID_CmdParse (CPU_INT16U        argc,
                                                          CPU_CHAR         *p_argv[],
                                                          NET_CMD_ERR      *p_err)
{
    NET_CMD_SOCK_ID_CMD_ARG  cmd_args;
    CPU_INT16U               i;



    Mem_Clr(&cmd_args, sizeof(cmd_args));

    if (argc > 3) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                     NetCmd_Sock_ID_CmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_ID_ARG  NetCmd_Sock_ID_CmdTranslate (NET_CMD_SOCK_ID_CMD_ARG   cmd_args,
                                                          NET_CMD_ERR              *p_err)
{
    NET_CMD_SOCK_ID_ARG  arg;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);


    return (arg);
}


/*
*********************************************************************************************************
*                                      NetCmd_Sock_BindCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_BIND_CMD_ARG  NetCmd_Sock_BindCmdParse (CPU_INT16U        argc,
                                                             CPU_CHAR         *p_argv[],
                                                             NET_CMD_ERR      *p_err)
{
    NET_CMD_SOCK_BIND_CMD_ARG  cmd_args;
    CPU_INT16U                  i;



    Mem_Clr(&cmd_args, sizeof(cmd_args));
    if (argc > 7) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     i++;
                     break;

                case NET_CMD_ARG_SOCK_FAMILY:
                     i++;
                     NetCmd_ArgsParserParseSockFamily(&p_argv[i], &cmd_args.FamilyPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_SOCK_PORT:
                     NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.PortPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     i++;
                     break;


                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                    NetCmd_Sock_BindCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_BIND_ARG  NetCmd_Sock_BindCmdTranslate (NET_CMD_SOCK_BIND_CMD_ARG   cmd_args,
                                                             NET_CMD_ERR                *p_err)
{
    NET_CMD_SOCK_BIND_ARG  arg;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }


    arg.Family = NetCmd_ArgsParserTranslateSockFamily(cmd_args.FamilyPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
        goto exit;
    }


    arg.Port = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.PortPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }


exit:
    return (arg);
}


/*
*********************************************************************************************************
*                                     NetCmd_Sock_ListenCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_LISTEN_CMD_ARG  NetCmd_Sock_ListenCmdParse (CPU_INT16U    argc,
                                                                 CPU_CHAR     *p_argv[],
                                                                 NET_CMD_ERR  *p_err)
{
    NET_CMD_SOCK_LISTEN_CMD_ARG  cmd_args;
    CPU_INT16U                   i;



    Mem_Clr(&cmd_args, sizeof(cmd_args));

    if (argc > 6) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_SOCK_Q_SIZE:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.QueueSizePtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                   NetCmd_Sock_ListenCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_SOCK_LISTEN_ARG  NetCmd_Sock_ListenCmdTranslate (NET_CMD_SOCK_LISTEN_CMD_ARG   cmd_args,
                                                                 NET_CMD_ERR                  *p_err)
{
    NET_CMD_SOCK_LISTEN_ARG  arg;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }


    arg.QueueSize = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.QueueSizePtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }


exit:
    return (arg);
}


/*
*********************************************************************************************************
*                                      NetCmd_Sock_ConnCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_CONN_CMD_ARG  NetCmd_Sock_ConnCmdParse (CPU_INT16U    argc,
                                                             CPU_CHAR     *p_argv[],
                                                             NET_CMD_ERR  *p_err)
{
    NET_CMD_SOCK_CONN_CMD_ARG  cmd_arg;
    CPU_INT16U                 i;



    Mem_Clr(&cmd_arg, sizeof(cmd_arg));

    if (argc > 7) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_arg);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_arg.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_SOCK_PORT:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_arg.PortPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_ADDR:
                     i++;
                     cmd_arg.AddrPtr = p_argv[i];
                     break;


                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_arg);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_arg);
}


/*
*********************************************************************************************************
*                                    NetCmd_Sock_ConnCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_SOCK_CONN_ARG  NetCmd_Sock_ConnCmdTranslate (NET_CMD_SOCK_CONN_CMD_ARG   cmd_args,
                                                             NET_CMD_ERR                *p_err)
{
    NET_CMD_SOCK_CONN_ARG  arg;
    NET_IP_ADDR_FAMILY     family;
    NET_ERR                err;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

    arg.Port = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.PortPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

    family = NetASCII_Str_to_IP(cmd_args.AddrPtr, &arg.Addr, sizeof(arg.Addr), &err);
    switch (family) {
        case NET_IP_ADDR_FAMILY_IPv4:
             arg.AddrLen = NET_IPv4_ADDR_SIZE;
             break;

        case NET_IP_ADDR_FAMILY_IPv6:
             arg.AddrLen = NET_IPv4_ADDR_SIZE;
             break;

        default:
            *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
             goto exit;
    }

exit:
    return (arg);
}


/*
*********************************************************************************************************
*                                       NetCmd_Sock_RxCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_RX_CMD_ARG  NetCmd_Sock_RxCmdParse (CPU_INT16U        argc,
                                                         CPU_CHAR         *p_argv[],
                                                         NET_CMD_ERR      *p_err)
{
    NET_CMD_SOCK_RX_CMD_ARG  cmd_args;
    CPU_INT16U               i;


    Mem_Clr(&cmd_args, sizeof(cmd_args));


    if (argc > 6) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_LEN:
                     i += NetCmd_ArgsParserParseDataLen(&p_argv[i], &cmd_args.DataLenPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_FMT:
                     i += NetCmd_ArgsParserParseFmt(&p_argv[i], &cmd_args.OutputFmtPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                     NetCmd_Sock_RxCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_SOCK_RX_ARG  NetCmd_Sock_RxCmdTranslate (NET_CMD_SOCK_RX_CMD_ARG   cmd_args,
                                                         NET_CMD_ERR              *p_err)
{
    NET_CMD_SOCK_RX_ARG  arg;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

    arg.DataLen = NetCmd_ArgsParserTranslateDataLen(cmd_args.DataLenPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

    arg.OutputFmt = NetCmd_ArgsParserTranslateFmt(cmd_args.OutputFmtPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }


exit:
    return (arg);
}


/*
*********************************************************************************************************
*                                       NetCmd_Sock_TxCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_TX_CMD_ARG  NetCmd_Sock_TxCmdParse (CPU_INT16U    argc,
                                                         CPU_CHAR     *p_argv[],
                                                         NET_CMD_ERR  *p_err)
{
    NET_CMD_SOCK_TX_CMD_ARG  cmd_args;
    CPU_INT16U               i;



    Mem_Clr(&cmd_args, sizeof(cmd_args));

    if (argc > 6) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_LEN:
                     i += NetCmd_ArgsParserParseDataLen(&p_argv[i], &cmd_args.DataLenPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_DATA:
                     cmd_args.DataPtr = p_argv[i];
                     break;


                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}


/*
*********************************************************************************************************
*                                     NetCmd_Sock_TxCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_SOCK_TX_ARG  NetCmd_Sock_TxCmdTranslate (NET_CMD_SOCK_TX_CMD_ARG   cmd_args,
                                                         NET_CMD_ERR              *p_err)
{
    NET_CMD_SOCK_TX_ARG  arg;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

    arg.DataLen = NetCmd_ArgsParserTranslateDataLen(cmd_args.DataLenPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

exit:
    return (arg);
}


/*
*********************************************************************************************************
*                                       NetCmd_Sock_TxCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  NET_CMD_SOCK_OPT_CMD_ARG  NetCmd_Sock_OptCmdParse (CPU_INT16U    argc,
                                                           CPU_CHAR     *p_argv[],
                                                           NET_CMD_ERR  *p_err)
{
    NET_CMD_SOCK_OPT_CMD_ARG  cmd_args;
    CPU_INT16U                i;



    Mem_Clr(&cmd_args, sizeof(cmd_args));

    if (argc > 6) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_SOCK_ID:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_args.SockIDPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;

                case NET_CMD_ARG_VAL:
                     i += NetCmd_ArgsParserParseDataLen(&p_argv[i], &cmd_args.ValPtr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_args);
                     }
                     break;


                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }
        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}

/*
*********************************************************************************************************
*                                     NetCmd_Sock_TxCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_SOCK_OPT_ARG  NetCmd_Sock_OptCmdTranslate (NET_CMD_SOCK_OPT_CMD_ARG   cmd_args,
                                                           NET_CMD_ERR               *p_err)
{
    NET_CMD_SOCK_OPT_ARG  arg;


    arg.SockID = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.SockIDPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

    arg.Value = NetCmd_ArgsParserTranslateVal32U(cmd_args.ValPtr, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
         goto exit;
    }

exit:
    return (arg);
}


/*
*********************************************************************************************************
*                                       NetCmd_Sock_SelCmdParse()
*
* Description : $$$$ Add function description.
*
* Argument(s) : argc    $$$$ Add description for 'argc'
*
*               p_argv  $$$$ Add description for 'p_argv'
*
*               p_err   $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/
#if 0
static  NET_CMD_SOCK_SEL_CMD_ARG  NetCmd_Sock_SelCmdParse (CPU_INT16U    argc,
                                                           CPU_CHAR     *p_argv[],
                                                           NET_CMD_ERR  *p_err)
{
    NET_CMD_SOCK_SEL_CMD_ARG  cmd_args;
    CPU_INT16U                i;


    Mem_Clr(&cmd_args, sizeof(cmd_args));

    if (argc > 6) {
       *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
        return (cmd_args);
    }

    cmd_args.RdListPtr      = DEF_NULL;
    cmd_args.WrListPtr      = DEF_NULL;
    cmd_args.ErrListPtr     = DEF_NULL;
    cmd_args.Timeout_sec_Ptr = DEF_NULL;

    for (i = 1; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_BEGIN) {

            i++;
            switch (*(p_argv[i])) {
                case NET_CMD_ARG_SOCK_SEL_RD:
                     cmd_args.RdListPtr = p_argv[i] + 1;
                     break;

                case NET_CMD_ARG_SOCK_SEL_WR:
                     cmd_args.WrListPtr = p_argv[i] + 1;
                     break;

                case NET_CMD_ARG_SOCK_SEL_ERR:
                     cmd_args.ErrListPtr = p_argv[i] + 1;
                     break;

                case NET_CMD_ARG_SOCK_SEL_TIMEOUT:
                     cmd_args.Timeout_sec_Ptr = p_argv[i];
                     break;

                default:
                    *p_err = NET_CMD_ERR_CMD_ARG_INVALID;
                     return (cmd_args);
            }

        }
    }

   *p_err = NET_CMD_ERR_NONE;

    return (cmd_args);
}
#endif


/*
*********************************************************************************************************
*                                     NetCmd_Sock_SelCmdTranslate()
*
* Description : $$$$ Add function description.
*
* Argument(s) : cmd_args    $$$$ Add description for 'cmd_args'
*
*               p_err       $$$$ Add description for 'p_err'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_Open().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if 0
static  NET_CMD_SOCK_SEL_ARG  NetCmd_Sock_SelCmdTranslate (NET_CMD_SOCK_SEL_CMD_ARG   cmd_args,
                                                           NET_CMD_ERR               *p_err)
{
    NET_CMD_SOCK_SEL_ARG   arg;
    CPU_CHAR              *p_str;
    NET_SOCK_ID            sock_id;



    NET_SOCK_DESC_INIT(&arg.DescRd);
    NET_SOCK_DESC_INIT(&arg.DescWr);
    NET_SOCK_DESC_INIT(&arg.DescErr);


    p_str = cmd_args.RdListPtr;
    while (p_str != DEF_NULL) {
         sock_id = NetCmd_Sock_SelGetSockID(p_str, p_str);
         if (sock_id != NET_SOCK_ID_NONE) {
             NET_SOCK_DESC_SET(sock_id, &arg.DescRd);
         }
    }


    p_str = cmd_args.WrListPtr;
    while (p_str != DEF_NULL) {
         sock_id = NetCmd_Sock_SelGetSockID(p_str, p_str);
         if (sock_id != NET_SOCK_ID_NONE) {
             NET_SOCK_DESC_SET(sock_id, &arg.DescWr);
         }
    }


    p_str = cmd_args.ErrListPtr;
    while (p_str != DEF_NULL) {
         sock_id = NetCmd_Sock_SelGetSockID(p_str, p_str);
         if (sock_id != NET_SOCK_ID_NONE) {
             NET_SOCK_DESC_SET(sock_id, &arg.DescErr);
         }
    }


    if (cmd_args.Timeout_sec_Ptr != DEF_NULL) {
        CPU_INT32U   timeout;
        NET_CMD_ERR  err;


        timeout = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.Timeout_sec_Ptr , &err);
        arg.Timeout.timeout_us  = 0u;
        arg.Timeout.timeout_sec = timeout;

    } else {
        arg.Timeout.timeout_us  = 0u;
        arg.Timeout.timeout_sec = 0u;
    }

   *p_err = NET_CMD_ERR_NONE;

    return (arg);
}
#endif


/*
*********************************************************************************************************
*                                      NetCmd_Sock_SelGetSockID()
*
* Description : $$$$ Add function description.
*
* Argument(s) : p_str       $$$$ Add description for 'p_str'
*
*               p_str_next  $$$$ Add description for 'p_str_next'
*
* Return(s)   : $$$$ Add return value description.
*
* Caller(s)   : NetCmd_Sock_SelCmdTranslate().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/
#if 0
static  NET_SOCK_ID  NetCmd_Sock_SelGetSockID (CPU_CHAR  *p_str,
                                               CPU_CHAR  *p_str_next)
{
    NET_SOCK_ID    val = NET_SOCK_ID_NONE;
    CPU_CHAR      *p_str_copy = p_str;
    NET_CMD_ERR    err;

    if (p_str == DEF_NULL) {
        goto exit;
    }

   (void)&p_str_next;

    p_str_next = DEF_NULL;


    while (p_str_copy != DEF_NULL) {
        switch (*p_str_copy) {
            case ASCII_CHAR_SPACE:
                *p_str_copy = ASCII_CHAR_NULL;
                 val        = NetCmd_ArgsParserTranslateID_Nbr(p_str, &err);
                *p_str_copy = ASCII_CHAR_SPACE;
                 p_str_next = p_str_copy + 1;
                 goto exit;


            case NET_CMD_ARG_BEGIN:
                 goto exit;

            default:
                p_str_copy++;
                break;
        }
    }

exit:
    return (val);
}
#endif


/*
*********************************************************************************************************
*                                        NetCmd_DictionaryGet()
*
* Description : Find dictionary key by comparing string with dictionary string entries.
*
* Argument(s) : p_dictionary_tbl    Pointer on the dictionary table.
*
*               dictionary_size     Size of the dictionary in octet.
*
*               p_str_cmp           Pointer to string to find key.
*               ---------           Argument validated by callers.
*
*               str_len             Length of the string.
*
* Return(s)   :
*
* Caller(s)   : NetDev_MgmtProcessRespScan().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
static CPU_INT32U  NetCmd_DictionaryGet  (const  NET_CMD_DICTIONARY *p_dictionary_tbl,
                                                 CPU_INT32U          dictionary_size,
                                          const  CPU_CHAR           *p_str_cmp,
                                                 CPU_INT32U          str_len)
{
    CPU_INT32U           nbr_entry;
    CPU_INT32U           ix;
    CPU_INT32U           len;
    CPU_INT16S           cmp;
    NET_CMD_DICTIONARY  *p_srch;


    nbr_entry =  dictionary_size / sizeof(NET_CMD_DICTIONARY);
    p_srch    = (NET_CMD_DICTIONARY *)p_dictionary_tbl;
    for (ix = 0; ix < nbr_entry; ix++) {
        len = DEF_MIN(str_len, p_srch->StrLen);
        cmp = Str_Cmp_N(p_str_cmp, p_srch->StrPtr, len);
        if (cmp == 0) {
            return (p_srch->Key);
        }
        p_srch++;
    }

    return (NET_CMD_DICTIONARY_KEY_INVALID);
}
#endif

