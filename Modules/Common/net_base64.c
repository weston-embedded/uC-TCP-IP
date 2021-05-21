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
*                                       NETWORK BASE64 LIBRARY
*
* Filename : net_base64.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           INCLUDES FILES
*********************************************************************************************************
*/

#include "net_base64.h"
#include  <lib_ascii.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL TABLES
*
* Note(s): (1) This table represents the alphabet for the base-64 encoder.
*********************************************************************************************************
*********************************************************************************************************
*/
                                                                /* See Note #1.                                         */
static  const  CPU_CHAR  NetBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL FUNCTION PROTOTYPTES
*********************************************************************************************************
*********************************************************************************************************
*/

static  CPU_INT08U  NetBase64_ConvertTo6Bits(CPU_CHAR  b64_val);


/*
*********************************************************************************************************
*                                          NetUtil_Base64Encode()
*
* Description : (1) Encode a buffer to the Base64 standard.
*
*                   (a) Calculate number of groups & output len
*                   (b) Encode groups of 3 octets                                       See Note #2
*                   (c) Encode remaining   octets, if any
*                   (d) NULL terminate out buffer
*
*
* Argument(s) : p_buf_in        Pointer to buffer holding data to encode.
*
*               buf_in_len      Length of data in buffer input.
*
*               p_buf_out       Pointer to a buffer that will receive the encoded data.
*
*               buf_out_len     Length of buffer output.
*
*               p_err           Pointer to variable that will hold the return error code from this function :
*
*                                   NET_ERR_NONE                        No error.
*                                   NET_ERR_FAULT_NULL_PTR              Argument passed a NULL pointer.
*                                   NET_UTIL_ERR_BUF_TOO_SMALL          Buffer too small.
*                                   NET_UTIL_ERR_NULL_SIZE              Invalid buffer size
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a application programming interface (API) function & MAY be called
                by application function(s).
*
* Note(s)     : (2) From RFC #4648, Section 4 'Base 64 Encoding' "The encoding process represents 24-bit
*                   groups of input bits as output strings of 4 encoded characters.  Each of [these
*                   encoded characters] is translated into a single character in the base 64 alphabet".
*********************************************************************************************************
*/

void  NetBase64_Encode (CPU_CHAR     *p_buf_in,
                        CPU_INT16U    buf_in_len,
                        CPU_CHAR     *p_buf_out,
                        CPU_INT16U    buf_out_len,
                        NET_ERR      *p_err)
{
    CPU_INT16U  nbr_in_grp;
    CPU_INT16U  nbr_octets_remaining;
    CPU_INT16U  grp_ix;
    CPU_INT16U  rd_ix;
    CPU_INT16U  wr_ix;
    CPU_INT16U  out_expect_len;


                                                                /* ---------------- VALIDATE PTR & LEN ---------------- */
    if ((p_buf_in  == DEF_NULL) ||
        (p_buf_out == DEF_NULL)) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (buf_in_len == 0u) {
       *p_err = NET_UTIL_ERR_NULL_SIZE ;
        return;
    }
                                                                /* ------------- CALC NBR OF GRP'S & LEN -------------- */
                                                                /* Nbr of grp's (see Note #2).                          */
    nbr_in_grp           = buf_in_len / NET_BASE64_ENCODER_OCTETS_IN_GRP;
    nbr_octets_remaining = buf_in_len % NET_BASE64_ENCODER_OCTETS_IN_GRP;

                                                                /* Calc out buf size and validate.                      */
    out_expect_len = (nbr_in_grp * NET_BASE64_ENCODER_OCTETS_OUT_GRP);
    if (nbr_octets_remaining != 0u) {
        out_expect_len = out_expect_len + NET_BASE64_ENCODER_OCTETS_OUT_GRP;
    }
    out_expect_len++;                                           /* Provision for termination NUL char.                  */

    if (buf_out_len < out_expect_len) {                         /* If out buf len < than expected out ...               */
       *p_err = NET_UTIL_ERR_BUF_TOO_SMALL;                     /* ... rtn err.                                         */
        return;
    }

    grp_ix = 0u;
    rd_ix  = 0u;
    wr_ix  = 0u;

                                                                /* -------------- ENC GRP'S OF 3 OCTETS --------------- */
    while (grp_ix < nbr_in_grp) {
        p_buf_out[wr_ix    ] = NetBase64Alphabet[ (p_buf_in[rd_ix    ] &  0xFC) >> 2];
        p_buf_out[wr_ix + 1] = NetBase64Alphabet[((p_buf_in[rd_ix    ] &  0x03) << 4) | ((p_buf_in[rd_ix + 1] & 0xF0) >> 4)];
        p_buf_out[wr_ix + 2] = NetBase64Alphabet[((p_buf_in[rd_ix + 1] &  0x0F) << 2) | ((p_buf_in[rd_ix + 2] & 0xC0) >> 6)];
        p_buf_out[wr_ix + 3] = NetBase64Alphabet[p_buf_in[rd_ix + 2] &  0x3F];

        wr_ix += NET_BASE64_ENCODER_OCTETS_OUT_GRP;
        rd_ix += NET_BASE64_ENCODER_OCTETS_IN_GRP;
        grp_ix++;
    }

                                                                /* --------------- ENC REMAINING OCTETS --------------- */
    if (nbr_octets_remaining == 1) {
        p_buf_out[wr_ix    ] = NetBase64Alphabet[(p_buf_in[rd_ix] &  0xFC) >> 2];
        p_buf_out[wr_ix + 1] = NetBase64Alphabet[(p_buf_in[rd_ix] &  0x03) << 4];
        p_buf_out[wr_ix + 2] = (CPU_CHAR)NET_BASE64_ENCODER_PAD_CHAR;
        p_buf_out[wr_ix + 3] = (CPU_CHAR)NET_BASE64_ENCODER_PAD_CHAR;

        wr_ix += NET_BASE64_ENCODER_OCTETS_OUT_GRP;

    } else if (nbr_octets_remaining == 2) {
        p_buf_out[wr_ix    ] = NetBase64Alphabet[(p_buf_in[rd_ix] &  0xFC) >> 2];
        p_buf_out[wr_ix + 1] = NetBase64Alphabet[((p_buf_in[rd_ix    ] &  0x03) << 4) | ((p_buf_in[rd_ix + 1] & 0xF0) >> 4)];
        p_buf_out[wr_ix + 2] = NetBase64Alphabet[ (p_buf_in[rd_ix + 1] &  0x0F) << 2];
        p_buf_out[wr_ix + 3] = (CPU_CHAR)NET_BASE64_ENCODER_PAD_CHAR;

        wr_ix += NET_BASE64_ENCODER_OCTETS_OUT_GRP;
    }


    p_buf_out[wr_ix] = ASCII_CHAR_NULL;                              /* NULL terminate out buf.                              */


   *p_err = NET_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetUtil_Base64Decode()
*
* Description : (1) Decode a Base64 value to a hex value .
*
*                   (a) Calculate number of groups & output len
*                   (b) Decode groups of 4 octets
*                   (c) DEcode the last part of the string.
*
*
* Argument(s) : p_buf_in        Pointer to buffer holding encoded data.
*
*               buf_in_len      Length of data in buffer input.
*
*               p_buf_out       Pointer to a buffer that will receive the decoded data.
*
*               buf_out_len     Length of buffer output.
*
*               p_err           Pointer to variable that will hold the return error code from this function :
*
*                                   NET_ERR_NONE                        No error.
*                                   NET_ERR_FAULT_NULL_PTR              Argument passed a NULL pointer.
*                                   NET_UTIL_ERR_BUF_TOO_SMALL          Buffer too small.
*                                   NET_UTIL_ERR_NULL_SIZE              Invalid buffer size
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a application programming interface (API) function & MAY be called
                by application function(s).
*
* Note(s)     : (2) From RFC #4648, Section 4 'Base 64 Encoding' "The encoding process represents 24-bit
*                   groups of input bits as output strings of 4 encoded characters.  Each of [these
*                   encoded characters] is translated into a single character in the base 64 alphabet".
*********************************************************************************************************
*/

void  NetBase64_Decode (CPU_CHAR     *p_buf_in,
                        CPU_INT16U    buf_in_len,
                        CPU_CHAR     *p_buf_out,
                        CPU_INT16U    buf_out_len,
                        NET_ERR      *p_err)
{
    CPU_INT16U  nbr_in_grp;
    CPU_INT16U  grp_ix;
    CPU_INT16U  rd_ix;
    CPU_INT16U  wr_ix;
    CPU_INT16U  out_expect_len;
    CPU_INT08U  tmp_val[NET_BASE64_DECODER_OCTETS_IN_GRP];
    CPU_INT08U  i;

                                                                /* ---------------- VALIDATE PTR & LEN ---------------- */
    if ((p_buf_in  == DEF_NULL) ||
        (p_buf_out == DEF_NULL)) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (buf_in_len == 0u) {
       *p_err = NET_UTIL_ERR_NULL_SIZE ;
        return;
    }
                                                                /* ------------- CALC NBR OF GRP'S & LEN -------------- */
                                                                /* Nbr of grp's (see Note #2).                          */
    nbr_in_grp = buf_in_len / NET_BASE64_DECODER_OCTETS_IN_GRP;
                                                                /* Calc out buf size and validate.                      */
    out_expect_len = (nbr_in_grp * NET_BASE64_DECODER_OCTETS_OUT_GRP);
    if (p_buf_in[buf_in_len - 2] == '=') {
        if (p_buf_in[buf_in_len - 3] == '=') {
            out_expect_len -= 2;
        } else {
            out_expect_len -= 1;
        }
    }

    if (buf_out_len < out_expect_len) {                         /* If out buf len < than expected out ...               */
       *p_err = NET_UTIL_ERR_BUF_TOO_SMALL;                     /* ... rtn err.                                         */
        return;
    }


    grp_ix = 0u;
    rd_ix  = 0u;
    wr_ix  = 0u;

                                                                /* -------------- DEC GRP'S OF 3 OCTETS --------------- */
    while (grp_ix < nbr_in_grp - 1) {
        for (i = 0; i < NET_BASE64_DECODER_OCTETS_IN_GRP ; i++) {
            tmp_val[i] = NetBase64_ConvertTo6Bits(p_buf_in[rd_ix + i]);
        }
        p_buf_out[wr_ix    ] = ((tmp_val[0] << 2) & 0xFC) + ((tmp_val[1] >> 4) & 0x03);
        p_buf_out[wr_ix + 1] = ((tmp_val[1] << 4) & 0xF0) + ((tmp_val[2] >> 2) & 0x0F);
        p_buf_out[wr_ix + 2] = ((tmp_val[2] << 6) & 0xC0) +  (tmp_val[3] & 0x3F);

        wr_ix += NET_BASE64_DECODER_OCTETS_OUT_GRP;
        rd_ix += NET_BASE64_DECODER_OCTETS_IN_GRP;
        grp_ix++;
    }

                                                                /* --------------- DEC REMAINING OCTETS --------------- */
    if (out_expect_len % NET_BASE64_DECODER_OCTETS_OUT_GRP == 2) {
        for (i = 0; i < 3 ; i++) {
            tmp_val[i] = NetBase64_ConvertTo6Bits(p_buf_in[rd_ix + i]);
        }

        p_buf_out[wr_ix    ] = ((tmp_val[0] << 2) & 0xFC) + ((tmp_val[1] >> 4) & 0x03);
        p_buf_out[wr_ix + 1] = ((tmp_val[1] << 4) & 0xF0) + ((tmp_val[2] >> 2) & 0x0F);

    } else if (out_expect_len % NET_BASE64_DECODER_OCTETS_OUT_GRP == 1) {

        for (i = 0; i < 2 ; i++) {
            tmp_val[i] = NetBase64_ConvertTo6Bits(p_buf_in[rd_ix + i]);
        }
        p_buf_out[wr_ix    ] = ((tmp_val[0] << 2) & 0xFC) + ((tmp_val[1] >> 4) & 0x03);
    }


   *p_err = NET_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTION
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        NetUtil_Base64To6Bits()
*
* Description : Convert a Base64 symbol to a 6-bits hex value..
*
* Argument(s) : b64_val     Base64 symbol to convert.
*
* Return(s)   : Converted base64 into a 6-bits hex value.
*
* Caller(s)   : NetUtil_Base64Decode().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static CPU_INT08U NetBase64_ConvertTo6Bits (CPU_CHAR  b64_val)
{

    CPU_INT08U  result = 0u;

    if ((b64_val >= 'A') && (b64_val <= 'Z')) {
        result = b64_val - 0x41;
    } else if ((b64_val >= 'a') && (b64_val <= 'z')) {
        result = b64_val - 0x61 + 26;
    } else if ((b64_val >= '0') && (b64_val <= '9')) {
        result = b64_val - 0x30 + 52;
    } else if (b64_val == '+') {
        result = 62;
    } else if (b64_val == '/') {
        result = 63;
    }

    return result;
}
