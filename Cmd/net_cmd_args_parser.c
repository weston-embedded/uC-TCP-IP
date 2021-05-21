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
* Filename : net_cmd_args_parser.c
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
#define  NET_CMD_ARGS_PARSER_MODULE

#include  "net_cmd_args_parser.h"
#include  "../IF/net_if.h"





#define  NET_CMD_ARG_PARSER_CMD_BEGIN              ASCII_CHAR_HYPHEN_MINUS

#define  NET_CMD_ARG_PARSER_CMD_WINDOWS_IF_NBR     ASCII_CHAR_LATIN_LOWER_W
#define  NET_CMD_ARG_PARSER_CMD_IPv4_ADDR_CFG      ASCII_CHAR_DIGIT_FOUR
#define  NET_CMD_ARG_PARSER_CMD_IPv6_ADDR_CFG      ASCII_CHAR_DIGIT_SIX
#define  NET_CMD_ARG_PARSER_CMD_MAC_ADDR_CFG       ASCII_CHAR_LATIN_LOWER_M
#define  NET_CMD_ARG_PARSER_CMD_ID_CFG             ASCII_CHAR_LATIN_LOWER_I
#define  NET_CMD_ARG_PARSER_CMD_TELNET_CFG         ASCII_CHAR_LATIN_LOWER_T


static  NET_CMD_STR_ARGS  NetCmd_ArgsParserParse (CPU_INT16U    argc,
                                                  CPU_CHAR     *p_argv[],
                                                  NET_CMD_ERR  *p_err);

static  NET_CMD_ARGS  NetCmd_ArgsParserTranslate (NET_CMD_STR_ARGS   cmd_args,
                                                  NET_CMD_ERR       *p_err);

/*
*********************************************************************************************************
*                                       NetCmd_ArgsParserCmdParse()
*
* Description : Parse command line arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Return Network command arguments parsed.
*
* Caller(s)   : main().
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_CMD_ARGS  NetCmd_ArgsParserCmdParse (CPU_INT16U    argc,
                                         CPU_CHAR     *p_argv[],
                                         NET_CMD_ERR  *p_err)
{
    NET_CMD_ARGS      args;
    NET_CMD_STR_ARGS  cmd_args;


    args.IPv4_CfgEn = DEF_NO;
    args.IPv6_CfgEn = DEF_NO;

    cmd_args = NetCmd_ArgsParserParse(argc, p_argv, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
        return (args);
    }



    args = NetCmd_ArgsParserTranslate(cmd_args, p_err);
    if (*p_err != NET_CMD_ERR_NONE) {
        return (args);
    }


    return (args);
}


/*
*********************************************************************************************************
*                                    NetCmd_ArgsParserParseID_Nbr()
*
* Description : Validate that the argument value is an interface number.
*
* Argument(s) : p_argv  an array of pointers to the strings which are those arguments.
*
*               p_str_if_nbr    Pointer wich will receive the location of the Interface values to convert.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : DEF_OK, Argument is an interface number.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetCmd_ArgsParserParse(),
*               NetCmd_MTU_CmdArgParse(),
*               NetCmd_PingCmdArgParse(),
*               NetCmd_ResetCmdArgParse(),
*               NetCmd_RouteCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseID_Nbr (CPU_CHAR      *p_argv[],
                                          CPU_CHAR     **p_str_if_nbr,
                                          NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  dig;


   *p_str_if_nbr = p_argv[1];
    dig = ASCII_IS_DIG(**p_str_if_nbr);
    if (dig == DEF_NO) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (DEF_FAIL);
    }

   *p_err = NET_CMD_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                  NetCmd_ArgsParserTranslateID_Nbr()
*
* Description : Translate Interface number argument.
*
* Argument(s) : p_str_if_nbr    String that contains the interface number.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Interface number translated, if successfully converted,
*
*               NET_IF_NBR_NONE, otherwise.
*
* Caller(s)   : NetCmd_ArgsParserTranslate(),
*               NetCmd_MTU_Translate(),
*               NetCmd_PingCmdArgTranslate(),
*               NetCmd_ResetTranslate(),
*               NetCmd_RouteTranslate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetCmd_ArgsParserTranslateID_Nbr (CPU_CHAR     *p_str_if_nbr,
                                              NET_CMD_ERR  *p_err)
{
    CPU_INT16U  id;


    id = NET_IF_NBR_NONE;

    if (p_str_if_nbr != DEF_NULL) {
        id = Str_ParseNbr_Int32U(p_str_if_nbr, DEF_NULL, DEF_NBR_BASE_DEC);

    } else {
        id = NET_IF_NBR_NONE;
    }


   *p_err = NET_CMD_ERR_NONE;

    return (id);
}


/*
*********************************************************************************************************
*                                     NetCmd_ArgsParserParseIPv4()
*
* Description : Validate and local argument values of an IPv4 address configuration structure.
*
* Argument(s) : p_argv    an array of pointers to the strings which are those arguments
*
*               p_ip_cfg  Pointer to a variable that will receive the location of each IPv4 configuration field
*                         to be converted.
*
*               p_err     Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 3, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_ArgsParserParse(),
*               NetCmd_RouteCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseIPv4 (CPU_CHAR                *p_argv[],
                                        NET_CMD_IPv4_ASCII_CFG  *p_ip_cfg,
                                        NET_CMD_ERR             *p_err)
{
#ifdef NET_IPv4_MODULE_EN
    CPU_BOOLEAN  dig;


    dig = ASCII_IS_DIG_HEX(*p_argv[1]);
    if (dig == DEF_YES) {
        p_ip_cfg->HostPtr = p_argv[1];
    } else {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
        return (0);
    }

    dig = ASCII_IS_DIG_HEX(*p_argv[2]);
    if (dig == DEF_YES) {
        p_ip_cfg->MaskPtr = p_argv[2];
    } else {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
        return (0);
    }

    dig = ASCII_IS_DIG_HEX(*p_argv[3]);
    if (dig == DEF_YES) {
        p_ip_cfg->GatewayPtr = p_argv[3];
    } else {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (0);
    }

   *p_err = NET_CMD_ERR_NONE;
    return (3);

#else
   *p_err = NET_CMD_ERR_PARSER_ARG_NOT_EN;
    return (0);
#endif
}


/*
*********************************************************************************************************
*                                   NetCmd_ArgsParserTranslateIPv4()
*
* Description : Translate IPv4 argument value to an IPv4 address configuration structure.
*
* Argument(s) : p_ip_cfg    Structure that contains IPv4 addresses to convert.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Address configuration structure.
*
* Caller(s)   : NetCmd_ArgsParserTranslate(),
*               NetCmd_RouteTranslate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
NET_CMD_IPv4_CFG  NetCmd_ArgsParserTranslateIPv4 (NET_CMD_IPv4_ASCII_CFG  *p_ip_cfg,
                                                  NET_CMD_ERR             *p_err)
{
    NET_CMD_IPv4_CFG  ip_cfg;
    NET_ERR           err;


    ip_cfg.Host = NetASCII_Str_to_IPv4(p_ip_cfg->HostPtr, &err);
    if (err != NET_ASCII_ERR_NONE) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (ip_cfg);
    }


    ip_cfg.Mask = NetASCII_Str_to_IPv4(p_ip_cfg->MaskPtr, &err);
    if (err != NET_ASCII_ERR_NONE) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (ip_cfg);
    }


    ip_cfg.Gateway = NetASCII_Str_to_IPv4(p_ip_cfg->GatewayPtr, &err);
    if (err != NET_ASCII_ERR_NONE) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (ip_cfg);
    }

    *p_err = NET_CMD_ERR_NONE;


    return (ip_cfg);
}
#endif


/*
*********************************************************************************************************
*                                     NetCmd_ArgsParserParseIPv6()
*
* Description : Validate and local argument values of an IPv6 address configuration structure.
*
* Argument(s) : p_argv    an array of pointers to the strings which are those arguments
*
*               p_ip_cfg  Pointer to a variable that will receive the location of each IPv6 configuration field
*                         to be converted.
*
*               p_err     Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 2, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_ArgsParserParse(),
*               NetCmd_RouteCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseIPv6 (CPU_CHAR                *p_argv[],
                                        NET_CMD_IPv6_ASCII_CFG  *p_ip_cfg,
                                        NET_CMD_ERR             *p_err)
{
#ifdef NET_IPv6_MODULE_EN
    CPU_BOOLEAN  dig_hex;


    dig_hex = ASCII_IS_DIG_HEX(*p_argv[1]);
    if (dig_hex == DEF_YES) {
        p_ip_cfg->HostPtr = p_argv[1];
    } else {
       *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
        return (0);
    }

    dig_hex = ASCII_IS_DIG_HEX(*p_argv[2]);
    if (dig_hex == DEF_YES) {
        p_ip_cfg->PrefixLenPtr = p_argv[2];
    } else {
       *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
        return (0);
    }

   *p_err = NET_CMD_ERR_NONE;
    return (2);

#else
    (void)&p_argv;
    (void)&p_ip_cfg;

    *p_err = NET_CMD_ERR_PARSER_ARG_NOT_EN;
     return (0);
#endif
}


/*
*********************************************************************************************************
*                                   NetCmd_ArgsParserTranslateIPv6()
*
* Description : Translate IPv6 argument value to an IPv6 address configuration structure.
*
* Argument(s) : p_ip_cfg    Structure that contains IPv4 addresses to convert.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Address configuration structure.
*
* Caller(s)   : NetCmd_ArgsParserTranslate(),
*               NetCmd_RouteTranslate().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
NET_CMD_IPv6_CFG  NetCmd_ArgsParserTranslateIPv6 (NET_CMD_IPv6_ASCII_CFG  *p_ip_cfg,
                                                    NET_CMD_ERR             *p_err)
{
    NET_CMD_IPv6_CFG  ip_cfg;
    NET_ERR            err;


    ip_cfg.Host = NetASCII_Str_to_IPv6(p_ip_cfg->HostPtr, &err);
    if (err != NET_ASCII_ERR_NONE) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (ip_cfg);
    }


    ip_cfg.PrefixLen = Str_ParseNbr_Int32U(p_ip_cfg->PrefixLenPtr, DEF_NULL, DEF_NBR_BASE_DEC);


   *p_err = NET_CMD_ERR_NONE;

    return (ip_cfg);
}
#endif

/*
*********************************************************************************************************
*                                     NetCmd_ArgsParserParseMAC()
*
* Description : Validate and local argument values of an MAC address configuration structure.
*
* Argument(s) : p_argv    an array of pointers to the strings which are those arguments
*
*               p_ip_cfg  Pointer to a variable that will receive the location of each IPv6 configuration field
*                         to be converted.
*
*               p_err     Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 1, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_ArgsParserParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseMAC (CPU_CHAR      *p_argv[],
                                       CPU_CHAR     **p_str_mac,
                                       NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  dig_hex;


    dig_hex = ASCII_IS_DIG_HEX(*p_argv[1]);
    if (dig_hex == DEF_YES) {
       *p_str_mac = p_argv[1];
    } else {
       *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
        return (0);
    }

   *p_err = NET_CMD_ERR_NONE;

    return (1);
}


/*
*********************************************************************************************************
*                                   NetCmd_ArgsParserTranslateMAC()
*
* Description : Translate MAC address argument value to a MAC address configuration structure.
*
* Argument(s) : p_str_mac    String that contains MAC address to convert.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : MAC Address configuration structure.
*
* Caller(s)   : NetCmd_ArgsParserTranslate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_CMD_MAC_CFG  NetCmd_ArgsParserTranslateMAC (CPU_CHAR     *p_str_mac,
                                                NET_CMD_ERR  *p_err)
{
    NET_CMD_MAC_CFG  mac_cfg;
    NET_ERR           net_err;

    NetASCII_Str_to_MAC(                p_str_mac,
                        (CPU_INT08U  *)&mac_cfg.MAC_Addr,
                                       &net_err);
    if (net_err != NET_ASCII_ERR_NONE) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (mac_cfg);
    }

    *p_err = NET_CMD_ERR_NONE;

    return (mac_cfg);
}


/*
*********************************************************************************************************
*                                    NetCmd_ArgsParserParseDataLen()
*
* Description : Validate and local argument values of data length.
*
* Argument(s) : p_argv    an array of pointers to the strings which are those arguments
*
*               p_ip_cfg  Pointer to a string will receive the location of the argument value.
*
*               p_err     Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 1, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_PingCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseDataLen (CPU_CHAR      *p_argv[],
                                           CPU_CHAR     **p_str_len,
                                           NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  dig;


   *p_str_len = p_argv[1];
    dig = ASCII_IS_DIG(**p_str_len);
    if (dig == DEF_NO) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (0);
    }

   *p_err = NET_CMD_ERR_NONE;

    return (1);
}


/*
*********************************************************************************************************
*                                  NetCmd_ArgsParserTranslateDataLen()
*
* Description : Translate MAC address argument value to a MAC address configuration structure.
*
* Argument(s) : p_str_mac    String that contains MAC length to convert.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Length converted.
*
* Caller(s)   : NetCmd_PingCmdArgTranslate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetCmd_ArgsParserTranslateDataLen (CPU_CHAR     *p_str_len,
                                               NET_CMD_ERR  *p_err)
{
    CPU_INT16U  data_len;


    if (p_str_len != DEF_NULL) {
        data_len = Str_ParseNbr_Int32U(p_str_len, DEF_NULL, DEF_NBR_BASE_DEC);

    } else {
        data_len = 0;
    }

   *p_err = NET_CMD_ERR_NONE;

    return (data_len);
}


/*
*********************************************************************************************************
*                                    NetCmd_ArgsParserParseDataLen()
*
* Description : Validate and local argument values of data length.
*
* Argument(s) : p_argv    an array of pointers to the strings which are those arguments
*
*               p_ip_cfg  Pointer to a string will receive the location of the argument value.
*
*               p_err     Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 1, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_PingCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetCmd_ArgsParserTranslateVal32U (CPU_CHAR     *p_str_len,
                                              NET_CMD_ERR  *p_err)
{
    CPU_INT32U  val;


    if (p_str_len != DEF_NULL) {
        val = Str_ParseNbr_Int32U(p_str_len, DEF_NULL, DEF_NBR_BASE_DEC);

    } else {
        val = 0;
    }

   *p_err = NET_CMD_ERR_NONE;

    return (val);
}

/*
*********************************************************************************************************
*                                    NetCmd_ArgsParserParseDataLen()
*
* Description : Validate and local argument values of data length.
*
* Argument(s) : p_argv    an array of pointers to the strings which are those arguments
*
*               p_ip_cfg  Pointer to a string will receive the location of the argument value.
*
*               p_err     Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 1, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_PingCmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseFmt (CPU_CHAR      *p_argv[],
                                       CPU_CHAR     **p_str_len,
                                       NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  char_val;


   *p_str_len = p_argv[1];

   char_val = ASCII_IS_PRINT(**p_str_len);
    if (char_val == DEF_NO) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (0);
    }


   *p_err = NET_CMD_ERR_NONE;

    return (1);
}


/*
*********************************************************************************************************
*                                NetCmd_ArgsParserTranslateSockFamily()
*
* Description : Translate Interface number argument.
*
* Argument(s) : p_str_if_nbr    String that contains the interface number.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Interface number translated, if successfully converted,
*
*               NET_IF_NBR_NONE, otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_CMD_OUTPUT_FMT  NetCmd_ArgsParserTranslateFmt (CPU_CHAR     *p_char_type,
                                                   NET_CMD_ERR  *p_err)
{
    NET_CMD_OUTPUT_FMT  fmt;


    switch (*p_char_type) {
        case 'h':
        default:
             fmt = NET_CMD_OUTPUT_FMT_HEX;
             break;

        case 's':
             fmt = NET_CMD_OUTPUT_FMT_STRING;
             break;
    }


   *p_err = NET_CMD_ERR_NONE;

    return (fmt);
}


/*
*********************************************************************************************************
*                                  NetCmd_ArgsParserParseCredential()
*
* Description : Validate and local argument values of credential structure.
*
* Argument(s) : p_argv          an array of pointers to the strings which are those arguments.
*
*               p_credential    Pointer to a credential arguments values.
*
*               p_err           Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : 2, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_ArgsParserParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseCredential (CPU_CHAR                      *p_argv[],
                                              NET_CMD_CREDENTIAL_ASCII_CFG  *p_credential,
                                              NET_CMD_ERR                   *p_err)
{
    p_credential->User_Ptr     = p_argv[1];
    p_credential->Password_Ptr = p_argv[2];

   *p_err = NET_CMD_ERR_NONE;

    return (2);
}


/*
*********************************************************************************************************
*                                NetCmd_ArgsParserTranslateCredential()
*
* Description : Translate credential argument values to a Credential configuration structure.
*
* Argument(s) : p_credential    Structure that contains credential values.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Length converted.
*
* Caller(s)   : NetCmd_ArgsParserTranslate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_CMD_CREDENTIAL_CFG  NetCmd_ArgsParserTranslateCredential (NET_CMD_CREDENTIAL_ASCII_CFG  *p_credential,
                                                              NET_CMD_ERR                   *p_err)
{
    NET_CMD_CREDENTIAL_CFG  credential_cfg;

    if ((p_credential->User_Ptr     != DEF_NULL) &&
        (p_credential->Password_Ptr != DEF_NULL)) {
        Str_Copy_N(credential_cfg.User,     p_credential->User_Ptr,     NET_CMD_STR_USER_MAX_LEN);
        Str_Copy_N(credential_cfg.Password, p_credential->Password_Ptr, NET_CMD_STR_PASSWORD_MAX_LEN);

    } else {
        credential_cfg.User[0]     = ASCII_CHAR_NULL;
        credential_cfg.Password[0] = ASCII_CHAR_NULL;
       *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
        return (credential_cfg);
    }


   *p_err = NET_CMD_ERR_NONE;

    return (credential_cfg);
}


/*
*********************************************************************************************************
*                                   NetCmd_ArgsParserParseMTU()
*
* Description : Validate and local argument values of MTU.
*
* Argument(s) : p_argv     an array of pointers to the strings which are those arguments
*
*               p_str_len  Pointer to a string will receive the location of the argument value.
*
*               p_err      Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 1, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_MTU_CmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseMTU (CPU_CHAR      *p_argv[],
                                       CPU_CHAR     **p_str_len,
                                       NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  dig;


   *p_str_len = p_argv[1];
    dig = ASCII_IS_DIG(**p_str_len);
    if (dig == DEF_NO) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (0);
    }

   *p_err = NET_CMD_ERR_NONE;

    return (1);
}


/*
*********************************************************************************************************
*                                  NetCmd_ArgsParserTranslateMTU()
*
* Description : Translate MTU argument value.
*
* Argument(s) : p_str_mac    String that contains MTU value to convert.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : MTU converted.
*
* Caller(s)   : NetCmd_MTU_Translate().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16U  NetCmd_ArgsParserTranslateMTU (CPU_CHAR     *p_str_len,
                                           NET_CMD_ERR  *p_err)
{
    CPU_INT16U  mtu;


    if (p_str_len != DEF_NULL) {
        mtu = Str_ParseNbr_Int32U(p_str_len, DEF_NULL, DEF_NBR_BASE_DEC);

    } else {
        mtu = NET_IF_MTU_ETHER;
    }

   *p_err = NET_CMD_ERR_NONE;

    return (mtu);
}


/*
*********************************************************************************************************
*                                   NetCmd_ArgsParserParseMTU()
*
* Description : Validate and convert argument values of MTU.
*
* Argument(s) : p_argv     an array of pointers to the strings which are those arguments
*
*               p_str_len  Pointer to a string will receive the location of the argument value.
*
*               p_err      Pointer to variable that will receive the return error code from this function
*
* Return(s)   : 1, if no error.
*
*               0, otherwise.
*
* Caller(s)   : NetCmd_MTU_CmdArgParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseSockFamily (CPU_CHAR      *p_argv[],
                                              CPU_CHAR     **p_str_family,
                                              NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  dig;


   *p_str_family = p_argv[0];
    dig = ASCII_IS_DIG(**p_str_family);
    if (dig == DEF_NO) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (0);
    }


   *p_err = NET_CMD_ERR_NONE;

    return (1);
}


/*
*********************************************************************************************************
*                                NetCmd_ArgsParserTranslateSockFamily()
*
* Description : Translate Interface number argument.
*
* Argument(s) : p_str_if_nbr    String that contains the interface number.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Interface number translated, if successfully converted,
*
*               NET_IF_NBR_NONE, otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_SOCK_PROTOCOL_FAMILY  NetCmd_ArgsParserTranslateSockFamily (CPU_CHAR     *p_char_family,
                                                                NET_CMD_ERR  *p_err)
{
    NET_SOCK_PROTOCOL_FAMILY  family = NET_SOCK_PROTOCOL_FAMILY_NONE;


    switch (*p_char_family) {
        case '4':
             family = NET_SOCK_PROTOCOL_FAMILY_IP_V4;
             break;

        case '6':
            family = NET_SOCK_PROTOCOL_FAMILY_IP_V6;
            break;

        default:
           *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
            goto exit;
    }


   *p_err = NET_CMD_ERR_NONE;

exit:
    return (family);
}


/*
*********************************************************************************************************
*                                   NetCmd_ArgsParserParseSockType()
*
* Description : Validate and convert argument values of sokcet type.
*
* Argument(s) : p_argv     an array of pointers to the strings which are those arguments
*
*               p_str_len  Pointer to a string will receive the location of the argument value.
*
*               p_err      Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Socket type
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT08U  NetCmd_ArgsParserParseSockType (CPU_CHAR      *p_argv[],
                                            CPU_CHAR     **p_str_len,
                                            NET_CMD_ERR   *p_err)
{
    CPU_BOOLEAN  char_val;


   *p_str_len = p_argv[0];

   char_val = ASCII_IS_PRINT(**p_str_len);
    if (char_val == DEF_NO) {
        *p_err = NET_CMD_ERR_PARSER_ARG_VALUE_INVALID;
         return (0);
    }


   *p_err = NET_CMD_ERR_NONE;

    return (1);
}


/*
*********************************************************************************************************
*                                NetCmd_ArgsParserTranslateSockFamily()
*
* Description : Translate Interface number argument.
*
* Argument(s) : p_str_if_nbr    String that contains the interface number.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Interface number translated, if successfully converted,
*
*               NET_IF_NBR_NONE, otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_SOCK_TYPE  NetCmd_ArgsParserTranslateSockType (CPU_CHAR      *p_char_type,
                                                    NET_CMD_ERR  *p_err)
{
    NET_SOCK_TYPE  type = NET_SOCK_TYPE_NONE;


    switch (*p_char_type) {
        case 's':
             type = NET_SOCK_TYPE_STREAM;
             break;

        case 'd':
             type = NET_SOCK_TYPE_DATAGRAM;
             break;

        default:
            *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
             goto exit;
    }


   *p_err = NET_CMD_ERR_NONE;

exit:
    return (type);
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
*                                       NetCmd_ArgsParserParse()
*
* Description : Parse command line arguments.
*
* Argument(s) : argc    is a count of the arguments supplied.
*
*               p_argv  an array of pointers to the strings which are those arguments.
*
*               p_err   Pointer to variable that will receive the return error code from this function
*
* Return(s)   : Network configuration structure.
*
* Caller(s)   : NetCmd_ArgsParserCmdParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_STR_ARGS  NetCmd_ArgsParserParse (CPU_INT16U    argc,
                                                  CPU_CHAR     *p_argv[],
                                                  NET_CMD_ERR  *p_err)
{
    NET_CMD_STR_ARGS   cmd_arg;
    CPU_INT16U         i;
    CPU_INT16U         ix_start;

    NET_CMD_ARGS_PARSER_CMD_ARGS_INIT(cmd_arg);
    if (*p_argv[0] == NET_CMD_ARG_PARSER_CMD_BEGIN) {
         ix_start = 0;
    } else {
         ix_start = 1;
    }


    for (i = ix_start; i < argc; i++) {
        if (*p_argv[i] == NET_CMD_ARG_PARSER_CMD_BEGIN) {
            switch (*(p_argv[i] + 1)) {
                case NET_CMD_ARG_PARSER_CMD_WINDOWS_IF_NBR:
                     i += NetCmd_ArgsParserParseID_Nbr(&p_argv[i], &cmd_arg.WindowsIF_Nbr_Ptr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                          return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_PARSER_CMD_IPv4_ADDR_CFG:
                     i += NetCmd_ArgsParserParseIPv4(&p_argv[i], &cmd_arg.AddrIPv4, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_PARSER_CMD_IPv6_ADDR_CFG:
                     i += NetCmd_ArgsParserParseIPv6(&p_argv[i], &cmd_arg.AddrIPv6, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_PARSER_CMD_MAC_ADDR_CFG:
                     i += NetCmd_ArgsParserParseMAC(&p_argv[i], &cmd_arg.MAC_Addr_Ptr, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_PARSER_CMD_ID_CFG:
                     i += NetCmd_ArgsParserParseCredential(&p_argv[i], &cmd_arg.Credential, p_err);
                     if (*p_err != NET_CMD_ERR_NONE) {
                         return (cmd_arg);
                     }
                     break;


                case NET_CMD_ARG_PARSER_CMD_TELNET_CFG:
                     cmd_arg.Telnet = DEF_YES;
                     break;


                default:
                    *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
                     return (cmd_arg);
            }
        } else {
            *p_err = NET_CMD_ERR_PARSER_ARG_INVALID;
             return (cmd_arg);
        }
    }


   *p_err = NET_CMD_ERR_NONE;

    return (cmd_arg);
}


/*
*********************************************************************************************************
*                                     NetCmd_ArgsParserTranslate()
*
* Description : Translate argument values.
*
* Argument(s) : cmd_args    Argument string values structure.
*
*               p_err   Pointer to variable that will receive the return error code from this function.
*
* Return(s)   : Network configuration structure.
*
* Caller(s)   : NetCmd_ArgsParserCmdParse().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_CMD_ARGS  NetCmd_ArgsParserTranslate (NET_CMD_STR_ARGS   cmd_args,
                                                  NET_CMD_ERR       *p_err)
{
    NET_CMD_ARGS  args;
    NET_CMD_ERR   err;


    if (cmd_args.WindowsIF_Nbr_Ptr != DEF_NULL) {
        args.WindowsIF_Nbr = NetCmd_ArgsParserTranslateID_Nbr(cmd_args.WindowsIF_Nbr_Ptr, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
            return (args);
        }

    } else {
        args.WindowsIF_Nbr = 0;
    }

#ifdef  NET_IPv4_MODULE_EN
    if (cmd_args.AddrIPv4.HostPtr != DEF_NULL) {
        args.IPv4 = NetCmd_ArgsParserTranslateIPv4(&cmd_args.AddrIPv4, &err);
        if (err != NET_CMD_ERR_NONE) {
            return (args);
        }

        args.IPv4_CfgEn = DEF_YES;

    } else {
        args.IPv4_CfgEn = DEF_NO;
    }
#endif

#ifdef  NET_IPv6_MODULE_EN
    if (cmd_args.AddrIPv6.HostPtr != DEF_NULL) {
        args.IPv6 = NetCmd_ArgsParserTranslateIPv6(&cmd_args.AddrIPv6, &err);
        if (err != NET_CMD_ERR_NONE) {
            return (args);
        }
        args.IPv6_CfgEn = DEF_YES;

    } else {
        args.IPv6_CfgEn = DEF_NO;
    }
#endif


    if (cmd_args.MAC_Addr_Ptr != DEF_NULL) {
        args.MAC_CfgEn = DEF_YES;
        args.MAC_Addr = NetCmd_ArgsParserTranslateMAC(cmd_args.MAC_Addr_Ptr, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
             return (args);
        }
    } else {
        args.MAC_CfgEn = DEF_NO;
    }



    if (cmd_args.Credential.User_Ptr != DEF_NULL) {
        args.Credential_CfgEn = DEF_YES;
        args.Credential = NetCmd_ArgsParserTranslateCredential(&cmd_args.Credential, p_err);
        if (*p_err != NET_CMD_ERR_NONE) {
            return (args);
        }

    } else {
        args.Credential_CfgEn       = DEF_NO;
        args.Credential.User[0]     = DEF_NULL;
        args.Credential.Password[0] = DEF_NULL;
    }



    if (cmd_args.Telnet == DEF_YES) {
        args.Telnet_Reqd = DEF_YES;

    } else {
        args.Telnet_Reqd = DEF_NO;
    }


   *p_err = NET_CMD_ERR_NONE;

    return (args);
}

