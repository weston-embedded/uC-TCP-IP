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
*                               NETWORK SHELL COMMAND OUTPUT UTILITIES
*
* Filename : net_cmd_output.c
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
#define  NET_CMD_OUTPUT_MODULE

#include  "net_cmd_output.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  NET_CMD_OUTPUT_USAGE                         ("Usage: ")
#define  NET_CMD_OUTPUT_ERR                           ("Error: ")
#define  NET_CMD_OUTPUT_SUCCESS                       ("Completed successfully")
#define  NET_CMD_OUTPUT_TABULATION                    ("\t")

#define  NET_CMD_OUTPUT_ERR_CMD_ARG_INVALID           ("Invalid Arguments")
#define  NET_CMD_OUTPUT_ERR_CMD_NOT_IMPLEMENTED       ("This command is not yet implemented")
#define  NET_CMD_OUTPUT_ERR_CMD_BEGINNING             ("---")
#define  NET_CMD_OUTPUT_ERR_CMD_END                   ("---------------------------------------------")


/*
*********************************************************************************************************
*                                     NetCmd_OutputBeginning()
*
* Description : Print command not yet implemented message.
*
* Argument(s) : out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputBeginning (SHELL_OUT_FNCT    out_fnct,
                                    SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S ret_val;

    ret_val = NetCmd_OutputMsg(NET_CMD_OUTPUT_ERR_CMD_BEGINNING, DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                     NetCmd_OutputBeginning()
*
* Description : Print command not yet implemented message.
*
* Argument(s) : out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputEnd (SHELL_OUT_FNCT    out_fnct,
                              SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S ret_val;

    ret_val = NetCmd_OutputMsg(NET_CMD_OUTPUT_ERR_CMD_END, DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);

    return (ret_val);
}

/*
*********************************************************************************************************
*                                         NetCmd_OutputCmdTbl()
*
* Description : Print out command tables.
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
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : NetCmd_Help(),
*               TELNETsCmd_Help().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S NetCmd_OutputCmdTbl (SHELL_CMD        *p_cmd_tbl,
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    SHELL_CMD   *p_shell_cmd;
    CPU_INT16S   ret_val;


    ret_val = NetCmd_OutputMsg(NET_CMD_OUTPUT_USAGE, DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
    switch (ret_val) {
        case SHELL_OUT_RTN_CODE_CONN_CLOSED:
        case SHELL_OUT_ERR:
             return (SHELL_EXEC_ERR);

        default:
             break;
    }


    p_shell_cmd = p_cmd_tbl;

    while (p_shell_cmd->Fnct != 0) {
        ret_val = NetCmd_OutputMsg((CPU_CHAR *)p_shell_cmd->Name, DEF_NO, DEF_YES, DEF_YES, out_fnct, p_cmd_param);
        switch (ret_val) {
            case SHELL_OUT_RTN_CODE_CONN_CLOSED:
            case SHELL_OUT_ERR:
                 return (SHELL_EXEC_ERR);

            default:
                 break;
        }

        p_shell_cmd++;
    }

    return (ret_val);
}


/*
*********************************************************************************************************
*                                     NetCmd_OutputNotImplemented()
*
* Description : Print command not yet implemented message.
*
* Argument(s) : out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputNotImplemented (SHELL_OUT_FNCT    out_fnct,
                                         SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S ret_val;

    ret_val = NetCmd_OutputError(NET_CMD_OUTPUT_ERR_CMD_NOT_IMPLEMENTED, out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                     NetCmd_OutputCmdArgInvalid()
*
* Description : Print Invalid argument error.
*
* Argument(s) : out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : NetCmd_IF_Config(),
*               NetCmd_IF_Reset(),
*               NetCmd_IF_RouteAdd(),
*               NetCmd_IF_RouteRemove(),
*               NetCmd_IF_SetMTU(),
*               NetCmd_Ping(),
*               NetIxANVL_BkgndPing(),
*               NetIxANVL_IPv6_AutoCfg(),
*               NetIxANVL_MLD_JoinMcastGroup(),
*               NetIxANVL_MLD_LeaveMcastGroup(),
*               NetIxANVL_NDP_CacheAddDestEntry(),
*               NetIxANVL_NDP_CacheAddPrefixEntry(),
*               NetIxANVL_NDP_CacheClr(),
*               NetIxANVL_NDP_CacheDeleteDestEntry(),
*               NetIxANVL_NDP_CacheGetIsRouterFlag(),
*               NetIxANVL_NDP_CacheGetState(),
*               NetIxANVL_UDP_RouteAdd(),
*               NetIxANVL_UDP_RouteRem().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputCmdArgInvalid (SHELL_OUT_FNCT    out_fnct,
                                         SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S ret_val;

    ret_val = NetCmd_OutputError(NET_CMD_OUTPUT_ERR_CMD_ARG_INVALID, out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                         NetCmd_OutputError()
*
* Description : Print out error message.
*
* Argument(s) : p_msg           String that contains the error message.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : NetCmd_IF_Reset(),
*               NetCmd_IF_RouteAdd(),
*               NetCmd_IF_RouteRemove(),
*               NetCmd_IF_SetMTU(),
*               NetCmd_IP_Config(),
*               NetCmd_OutputCmdArgInvalid(),
*               NetCmd_OutputNotImplemented(),
*               NetCmd_Ping4(),
*               NetCmd_Ping6(),
*               NetIxANVL_BkgndPing(),
*               NetIxANVL_IPv6_AutoCfg(),
*               NetIxANVL_MLD_JoinMcastGroup(),
*               NetIxANVL_MLD_LeaveMcastGroup(),
*               NetIxANVL_NDP_CacheAddDestEntry(),
*               NetIxANVL_NDP_CacheAddPrefixEntry(),
*               NetIxANVL_UDP_RouteAdd(),
*               NetIxANVL_UDP_RouteRem().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputError (CPU_CHAR         *p_error,
                                SHELL_OUT_FNCT    out_fnct,
                                SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S ret_val;


    ret_val = NetCmd_OutputMsg(NET_CMD_OUTPUT_ERR, DEF_YES, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
    switch (ret_val) {
        case SHELL_OUT_RTN_CODE_CONN_CLOSED:
        case SHELL_OUT_ERR:
             return (SHELL_EXEC_ERR);

        default:
             break;
    }


    ret_val = NetCmd_OutputMsg(p_error, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
    switch (ret_val) {
        case SHELL_OUT_RTN_CODE_CONN_CLOSED:
        case SHELL_OUT_ERR:
             return (SHELL_EXEC_ERR);

        default:
             break;
    }

    return (ret_val);
}



/*
*********************************************************************************************************
*                                      NetCmd_OutputErrorNetNbr()
*
* Description : Print out error message using the net error code.
*
* Argument(s) : err             Net error code.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputErrorNetNbr (NET_ERR           err,
                                      SHELL_OUT_FNCT    out_fnct,
                                      SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S  ret_val;
    CPU_CHAR    str_output[DEF_INT_32U_NBR_DIG_MAX + 1];


     ret_val = NetCmd_OutputMsg(NET_CMD_OUTPUT_ERR, DEF_YES, DEF_NO, DEF_NO, out_fnct, p_cmd_param);
     switch (ret_val) {
         case SHELL_OUT_RTN_CODE_CONN_CLOSED:
         case SHELL_OUT_ERR:
              return (SHELL_EXEC_ERR);

         default:
              break;
     }

     ret_val += NetCmd_OutputMsg("Net error code = ", DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);


    (void)Str_FmtNbr_Int32U(err,
                            DEF_INT_32U_NBR_DIG_MAX,
                            DEF_NBR_BASE_DEC,
                            DEF_NULL,
                            DEF_NO,
                            DEF_YES,
                            str_output);

     ret_val += NetCmd_OutputMsg(str_output, DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);


     return (ret_val);
}

/*
*********************************************************************************************************
*                                        NetCmd_OutputSuccess()
*
* Description : Output a success message in the console.
*
* Argument(s) : p_msg           String that contains the message to output.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : NetCmd_IF_Reset(),
*               NetCmd_IF_RouteAdd(),
*               NetCmd_IF_RouteRemove(),
*               NetCmd_IF_SetMTU(),
*               NetCmd_IP_Config(),
*               NetCmd_Ping4(),
*               NetCmd_Ping6(),
*               NetIxANVL_BkgndPing(),
*               NetIxANVL_IPv6_AutoCfg(),
*               NetIxANVL_MLD_JoinMcastGroup(),
*               NetIxANVL_MLD_LeaveMcastGroup(),
*               NetIxANVL_NDP_CacheAddDestEntry(),
*               NetIxANVL_NDP_CacheAddPrefixEntry(),
*               NetIxANVL_NDP_CacheClr(),
*               NetIxANVL_NDP_CacheDeleteDestEntry(),
*               NetIxANVL_TCP_Reset(),
*               NetIxANVL_TCP_Setup(),
*               NetIxANVL_UDP_Reset(),
*               NetIxANVL_UDP_Setup(),
*               NetIxANVL_UDP_StubStart(),
*               NetIxANVL_UDP_StubStop().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputSuccess (SHELL_OUT_FNCT    out_fnct,
                                  SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S ret_val;


    ret_val = NetCmd_OutputMsg(NET_CMD_OUTPUT_SUCCESS, DEF_YES, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
    switch (ret_val) {
        case SHELL_OUT_RTN_CODE_CONN_CLOSED:
        case SHELL_OUT_ERR:
             return (SHELL_EXEC_ERR);

        default:
             break;
    }

    return (ret_val);
}


/*
*********************************************************************************************************
*                                         NetCmd_OutputInt32U()
*
* Description : Output a integer of 32 bits unsigned
*
* Argument(s) : nbr             Number
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputInt32U (CPU_INT32U        nbr,
                                 SHELL_OUT_FNCT    out_fnct,
                                 SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_CHAR    str_output[DEF_INT_32U_NBR_DIG_MAX + 1];
    CPU_INT16S  ret_val = 0;


    (void)Str_FmtNbr_Int32U(nbr,
                            DEF_INT_32U_NBR_DIG_MAX,
                            DEF_NBR_BASE_DEC,
                            DEF_NULL,
                            DEF_NO,
                            DEF_YES,
                            str_output);

    ret_val = NetCmd_OutputMsg(str_output, DEF_NO, DEF_NO, DEF_NO, out_fnct, p_cmd_param);

    return (ret_val);
}


/*
*********************************************************************************************************
*                                         NetCmd_OutputSockID()
*
* Description : Print out socket ID.
*
* Argument(s) : sock_id         Socket ID.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputSockID (CPU_INT16S        sock_id,
                                 SHELL_OUT_FNCT    out_fnct,
                                 SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16S  ret_val = 0;


    ret_val  = NetCmd_OutputMsg("Socket ID = ", DEF_NO, DEF_YES, DEF_NO, out_fnct, p_cmd_param);
    ret_val += NetCmd_OutputInt32U(sock_id, out_fnct, p_cmd_param);

    return (ret_val);
}

/*
*********************************************************************************************************
*                                          NetCmd_OutputMsg()
*
* Description : Output a message in the console.
*
* Argument(s) : p_msg           String that contains the message to output.
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputMsg (CPU_CHAR         *p_msg,
                              CPU_BOOLEAN       new_line_start,
                              CPU_BOOLEAN       new_line_end,
                              CPU_BOOLEAN       tab_start,
                              SHELL_OUT_FNCT    out_fnct,
                              SHELL_CMD_PARAM  *p_cmd_param)
{
    CPU_INT16U output_len;
    CPU_INT16S output;


    if (new_line_start == DEF_YES) {
        output = out_fnct(STR_NEW_LINE,
                          STR_NEW_LINE_LEN,
                          p_cmd_param->pout_opt);
        switch (output) {
            case SHELL_OUT_RTN_CODE_CONN_CLOSED:
            case SHELL_OUT_ERR:
                 return (SHELL_EXEC_ERR);

            default:
                 break;
        }
    }


    if (tab_start == DEF_YES) {
        output = out_fnct(NET_CMD_OUTPUT_TABULATION,
                          1,
                          p_cmd_param->pout_opt);
        switch (output) {
            case SHELL_OUT_RTN_CODE_CONN_CLOSED:
            case SHELL_OUT_ERR:
                 return (SHELL_EXEC_ERR);

           default:
                break;
        }
    }

    output_len = Str_Len(p_msg);
    output     = out_fnct(p_msg,
                          output_len,
                          p_cmd_param->pout_opt);
    switch (output) {
        case SHELL_OUT_RTN_CODE_CONN_CLOSED:
        case SHELL_OUT_ERR:
             return (SHELL_EXEC_ERR);

        default:
             break;
    }


    if (new_line_end == DEF_YES) {
        output = out_fnct(STR_NEW_LINE,
                          STR_NEW_LINE_LEN,
                          p_cmd_param->pout_opt);
        switch (output) {
            case SHELL_OUT_RTN_CODE_CONN_CLOSED:
            case SHELL_OUT_ERR:
                 return (SHELL_EXEC_ERR);

            default:
                 break;
        }
    }

    return (output);
}


/*
*********************************************************************************************************
*                                          NetCmd_OutputData()
*
* Description : Output data
*
* Argument(s) : p_buf           Pointer to a buffer that contains the data to output.
*
*               len             Data len contained in the buffer
*
*               out_fmt         Output format
*
*               out_fnct        is a callback to a respond to the requester.
*
*               p_cmd_param     is a pointer to additional information to pass to the command.
*
* Return(s)   : The number of positive data octets transmitted, if NO errors
*
*               SHELL_OUT_RTN_CODE_CONN_CLOSED, if implemented connection closed
*
*               SHELL_OUT_ERR,                  otherwise.
*
* Caller(s)   : NetCmd_Sock_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT16S  NetCmd_OutputData (CPU_INT08U          *p_buf,
                               CPU_INT16U           len,
                               NET_CMD_OUTPUT_FMT   out_fmt,
                               SHELL_OUT_FNCT       out_fnct,
                               SHELL_CMD_PARAM     *p_cmd_param)
{
    CPU_INT16S  output = 0;
    CPU_INT16U  i;


    output += out_fnct(STR_NEW_LINE,
                       STR_NEW_LINE_LEN,
                       p_cmd_param->pout_opt);


    switch (out_fmt) {
        case NET_CMD_OUTPUT_FMT_STRING:
             output += out_fnct((CPU_CHAR *)p_buf,
                                            len,
                                            p_cmd_param->pout_opt);
             break;

        case NET_CMD_OUTPUT_FMT_HEX:
             for (i = 0; i < len; i++) {
                 CPU_INT08U  *p_val = &p_buf[i];
                 CPU_INT08U   str_len;
                 CPU_CHAR     str_output[DEF_INT_08U_NBR_DIG_MAX + 1];


                 (void)Str_FmtNbr_Int32U(*p_val,
                                          DEF_INT_08U_NBR_DIG_MAX,
                                          DEF_NBR_BASE_HEX,
                                          DEF_NULL,
                                          DEF_NO,
                                          DEF_YES,
                                          str_output);

                 output += out_fnct("0x",
                                    2,
                                    p_cmd_param->pout_opt);

                 str_len  = Str_Len_N(str_output, DEF_INT_08U_NBR_DIG_MAX);
                 output  += out_fnct(str_output,
                                     str_len,
                                     p_cmd_param->pout_opt);

                 if (i < (len - 1)) {
                     output += out_fnct(", ",
                                         2,
                                         p_cmd_param->pout_opt);
                 }
             }
             break;

        default:
            break;
    }


    return (output);
}
