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
*                             NETWORK COMMAND ARGUMENT PARSING UTILITIES
*
* Filename : net_cmd_args_parser.h
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
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../Source/net_cfg_net.h"
#include  "../Source/net_sock.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_CMD_ARGS_PARSER_MODULE_PRESENT
#define  NET_CMD_ARGS_PARSER_MODULE_PRESENT


/*
*********************************************************************************************************
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
*********************************************************************************************************
*/

#include <cpu.h>
#include "net_cmd.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/


typedef  struct  net_cmd_cmd_args {
    CPU_CHAR                       *WindowsIF_Nbr_Ptr;
    NET_CMD_IPv4_ASCII_CFG          AddrIPv4;
    NET_CMD_IPv6_ASCII_CFG          AddrIPv6;
    CPU_CHAR                       *MAC_Addr_Ptr;
    NET_CMD_CREDENTIAL_ASCII_CFG    Credential;
    CPU_BOOLEAN                     Telnet;
} NET_CMD_STR_ARGS;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                              MACROS
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_CMD_ARGS_PARSER_CMD_ARGS_INIT(cmd_args)   {                                                \
                                                            cmd_args.WindowsIF_Nbr_Ptr       = DEF_NULL; \
                                                            cmd_args.AddrIPv4.HostPtr        = DEF_NULL; \
                                                            cmd_args.AddrIPv4.MaskPtr        = DEF_NULL; \
                                                            cmd_args.AddrIPv4.GatewayPtr     = DEF_NULL; \
                                                            cmd_args.AddrIPv6.HostPtr        = DEF_NULL; \
                                                            cmd_args.AddrIPv6.PrefixLenPtr   = DEF_NULL; \
                                                            cmd_args.MAC_Addr_Ptr            = DEF_NULL; \
                                                            cmd_args.Credential.User_Ptr     = DEF_NULL; \
                                                            cmd_args.Credential.Password_Ptr = DEF_NULL; \
                                                            cmd_args.Telnet                  = DEF_NO;   \
                                                        }


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

NET_CMD_ARGS              NetCmd_ArgsParserCmdParse           (CPU_INT16U                      argc,
                                                               CPU_CHAR                       *p_argv[],
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseID_Nbr        (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_if_nbr,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT16U                NetCmd_ArgsParserTranslateID_Nbr    (CPU_CHAR                       *p_str_if_nbr,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseIPv4          (CPU_CHAR                       *p_argv[],
                                                               NET_CMD_IPv4_ASCII_CFG         *p_ip_cfg,
                                                               NET_CMD_ERR                    *p_err);

NET_CMD_IPv4_CFG          NetCmd_ArgsParserTranslateIPv4      (NET_CMD_IPv4_ASCII_CFG         *p_ip_cfg,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseIPv6          (CPU_CHAR                       *p_argv[],
                                                               NET_CMD_IPv6_ASCII_CFG         *p_ip_cfg,
                                                               NET_CMD_ERR                    *p_err);

NET_CMD_IPv6_CFG          NetCmd_ArgsParserTranslateIPv6      (NET_CMD_IPv6_ASCII_CFG         *p_ip_cfg,
                                                               NET_CMD_ERR                    *p_err);


CPU_INT08U                NetCmd_ArgsParserParseMAC           (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_mac,
                                                               NET_CMD_ERR                    *p_err);

NET_CMD_MAC_CFG           NetCmd_ArgsParserTranslateMAC       (CPU_CHAR                       *p_str_mac,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseDataLen       (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_len,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseFmt           (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_len,
                                                               NET_CMD_ERR                    *p_err);

NET_CMD_OUTPUT_FMT        NetCmd_ArgsParserTranslateFmt       (CPU_CHAR                       *p_char_type,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT16U                NetCmd_ArgsParserTranslateDataLen   (CPU_CHAR                       *p_str_len,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT32U                NetCmd_ArgsParserTranslateVal32U    (CPU_CHAR                       *p_str_len,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseCredential    (CPU_CHAR                       *p_argv[],
                                                               NET_CMD_CREDENTIAL_ASCII_CFG   *p_credential,
                                                               NET_CMD_ERR                    *p_err);

NET_CMD_CREDENTIAL_CFG    NetCmd_ArgsParserTranslateCredential(NET_CMD_CREDENTIAL_ASCII_CFG   *p_credential,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseMTU           (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_len,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT16U                NetCmd_ArgsParserTranslateMTU       (CPU_CHAR                       *p_str_len,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseSockFamily    (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_family,
                                                               NET_CMD_ERR                    *p_err);

NET_SOCK_PROTOCOL_FAMILY  NetCmd_ArgsParserTranslateSockFamily(CPU_CHAR                       *p_char_family,
                                                               NET_CMD_ERR                    *p_err);

CPU_INT08U                NetCmd_ArgsParserParseSockType      (CPU_CHAR                       *p_argv[],
                                                               CPU_CHAR                      **p_str_len,
                                                               NET_CMD_ERR                    *p_err);

NET_SOCK_TYPE             NetCmd_ArgsParserTranslateSockType  (CPU_CHAR                       *p_char_family,
                                                               NET_CMD_ERR                    *p_err);


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif
