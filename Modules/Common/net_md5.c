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
*                                    NETWORK CRYPTO MD5 UTILITY
*
* Filename : net_md5.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_md5.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/
                                                                /* Shift Count Constants for the MD5 Transform Routine. */
#define  NET_MD5_S11                               7u
#define  NET_MD5_S12                              12u
#define  NET_MD5_S13                              17u
#define  NET_MD5_S14                              22u
#define  NET_MD5_S21                               5u
#define  NET_MD5_S22                               9u
#define  NET_MD5_S23                              14u
#define  NET_MD5_S24                              20u
#define  NET_MD5_S31                               4u
#define  NET_MD5_S32                              11u
#define  NET_MD5_S33                              16u
#define  NET_MD5_S34                              23u
#define  NET_MD5_S41                               6u
#define  NET_MD5_S42                              10u
#define  NET_MD5_S43                              15u
#define  NET_MD5_S44                              21u

                                                                /* Auxiliary MD5 functions.                             */
#define  NET_MD5_F1(x, y, z)                     (((x) &  (y)) | ((~x) &  (z)))
#define  NET_MD5_F2(x, y, z)                     (((x) &  (z)) |  ((y) & (~z)))
#define  NET_MD5_F3(x, y, z)                     ( (x) ^  (y)  ^   (z))
#define  NET_MD5_F4(x, y, z)                     ( (y) ^ ((x)  |  (~z)))

#define  NET_MD5_ROTATE_LEFT(x, n)               (((x) << (n)) | ((x) >> (32u - (n))))

                                                                /* Transformation operation performed by 'f' parameter. */
#define  NET_MD5_ROUND(f, a, b, c, d, x, s, ac)  do { (a) += f((b), (c), (d)) + (x) + (CPU_INT32U)(ac); \
                                                      (a)  = NET_MD5_ROTATE_LEFT ((a), (s));            \
                                                      (a) += (b);                                       \
                                                 } while (0u)


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  CPU_INT08U  NetMD5_PaddingBits [64u] = {
  0x80u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, \
  0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, \
  0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, \
  0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, \
  0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL FUNCTION PROTOTYPTES
*********************************************************************************************************
*********************************************************************************************************
*/

static  void        NetMD5_Encode(       CPU_INT08U  *p_output,
                                         CPU_INT32U  *p_input,
                                         CPU_INT32U   len);

static  CPU_INT32U  NetMD5_Decode(const  CPU_INT08U  *p_addr);


/*
*********************************************************************************************************
*                                            NetMD5_Decode()
*
* Description : Decodes buffer pointed to by 'p_addr' and converts it into a 32-bit unsigned integer.
*
* Argument(s) : p_addr    Pointer to a block of data.
*
* Return(s)   : A 32-bit integer constructed by the byte-swapped data block pointed by p_addr.
*
* Caller(s)   : NetTCP_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : Endianness check not required.
*********************************************************************************************************
*/

static  CPU_INT32U  NetMD5_Decode (const  CPU_INT08U  *p_addr)
{
    CPU_INT32U  output;


    output = (((((CPU_INT32U)p_addr[3u] << 8u) | p_addr[2u]) << 8u) | p_addr[1u]) << 8u | p_addr[0u];

    return (output);
}


/*
*********************************************************************************************************
*                                            NetMD5_Encode()
*
* Description : Encodes a 4-byte buffer into an unsigned char.
*
* Argument(s) : p_output    Pointer to array that will contain the byte swapped input data.
*
*               p_input     Pointer to the input data.
*
*               len         Length of output buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetMD5_Final().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetMD5_Encode (CPU_INT08U  *p_output,
                             CPU_INT32U  *p_input,
                             CPU_INT32U   len)
{
    CPU_INT32U  i;
    CPU_INT32U  j;


    for (i = 0u, j = 0u; j < len; i++, j += 4u) {
         p_output[j]      = (CPU_INT08U) (p_input[i]         & 0xFFu);
         p_output[j + 1u] = (CPU_INT08U)((p_input[i] >>  8u) & 0xFFu);
         p_output[j + 2u] = (CPU_INT08U)((p_input[i] >> 16u) & 0xFFu);
         p_output[j + 3u] = (CPU_INT08U)((p_input[i] >> 24u) & 0xFFu);
    }
}


/*
*********************************************************************************************************
*                                             NetMD5_Init()
*
* Description : Initializes the state and bit count variables within the MD5 context.
*
* Argument(s) : p_context    Pointer to the MD5 context.
*
* Return(s)   : none.
*
* Caller(s)   : NetTCP_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetMD5_Init (NET_MD5_CONTEXT  *p_context)
{
    p_context->state[0u] = 0x67452301u;                         /* Load up magic initialization constants.              */
    p_context->state[1u] = 0xEFCDAB89u;
    p_context->state[2u] = 0x98BADCFEu;
    p_context->state[3u] = 0x10325476u;
                                                                /* Set bit count to zero.                               */
    p_context->count[0u] = 0u;
    p_context->count[1u] = 0u;
}


/*
*********************************************************************************************************
*                                            NetMD5_Update()
*
* Description : Update the MD5 context to reflect the addition of another block of data.
*
* Argument(s) : p_context    Pointer to the MD5 context.
*
*               p_input      Pointer to the new block of data.
*
*               input_len    Length of the input buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetTCP_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetMD5_Update (       NET_MD5_CONTEXT  *p_context,
                     const  CPU_INT08U       *p_input,
                            CPU_INT32U        input_len)
{
    CPU_INT32U  i;
    CPU_INT32U  index;
    CPU_INT32U  patrial_len;

                                                                /* Compute number of bytes mod 64                       */
    index = (CPU_INT32U)((p_context->count[0u] >> 3u) & 0x3Fu);
                                                                /* Update number of bits                                */
    if ((p_context->count[0u] += ((CPU_INT32U)input_len << 3u)) < ((CPU_INT32U)input_len << 3u)) {
        p_context->count[1u]++;
    }
    p_context->count[1u] += ((CPU_INT32U)input_len >> 29u);

    patrial_len = (64u - index);

                                                                /* Transform as many times as possible.                 */
    if (input_len >= patrial_len) {
        Mem_Copy(&p_context->buffer[index], p_input, patrial_len);
        NetMD5_Transform(p_context->state, p_context->buffer);

        for (i = patrial_len; ((63u + i) < input_len); i += 64u) {
             NetMD5_Transform (p_context->state, &p_input[i]);
        }
        index = 0u;
    } else {
        i = 0u;
    }

                                                                /* Buffer remaining input                               */
    Mem_Copy(&p_context->buffer[index], &p_input[i], (input_len - i));
}


/*
*********************************************************************************************************
*                                             NetMD5_Final()
*
* Description : Final step of the MD5 algorihm. The bit pattern "NetMD5_PaddingBits" is padded to a 64-bit
*               boundary.
*
* Argument(s) : digest       A 16-byte array that will get the output digest.
*
*               p_context    Pointer to the MD5 context.
*
* Return(s)   : none.
*
* Caller(s)   : NetMD5_Update().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetMD5_Final (CPU_INT08U        digest[16u],
                    NET_MD5_CONTEXT  *p_context)
{
    CPU_INT08U  bits[8u];
    CPU_INT32U  index;
    CPU_INT32U  padding_len;

                                                                  /* Save number of bits                                  */
    NetMD5_Encode(bits, p_context->count, 8u);
                                                                  /* Pad out to 56 mod 64.                                */
    index  = (CPU_INT32U)((p_context->count[0u] >> 3u) & 0x3Fu);
    padding_len = (index < 56u) ? (56u - index) : (120u - index);
    NetMD5_Update(p_context, NetMD5_PaddingBits, padding_len);

                                                                  /* Append length (before padding)                       */
    NetMD5_Update(p_context, bits, 8u);
                                                                  /* Store state in digest                                */
    NetMD5_Encode(digest, p_context->state, 16u);

                                                                  /* Zeroize sensitive information.                       */
    Mem_Set(p_context, 0u, sizeof(*p_context));
}


/*
*****************************************************************************************************
*                                            NetMD5_Transform()
*
* Description : Main part of the MD5 algorithm. This function modifies an MD5 hash to reflect the addition
*               of a new block of data.
*
* Argument(s) : state    Pointer to the MD5 context's state variable (ABCD).
*
*               block    Pointer to a 64-byte data block to add to the MD5 digest.
*
* Return(s)   : none.
*
* Caller(s)   : NetMD5_Update().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetMD5_Transform (        CPU_INT32U  state[ 4u],
                        const   CPU_INT08U  block[64u])
{
    CPU_INT32U  A;
    CPU_INT32U  B;
    CPU_INT32U  C;
    CPU_INT32U  D;
    CPU_INT32U  x[16u];


    for (CPU_INT32U i = 0u; i < 16u; ++i) {
         x[i] = NetMD5_Decode(block + 4 * i);
    }

    A = state[0u];
    B = state[1u];
    C = state[2u];
    D = state[3u];
                                                                /* Round 1.                                             */

    NET_MD5_ROUND(NET_MD5_F1, A, B, C, D, x[ 0u], NET_MD5_S11, 0xD76AA478u);
    NET_MD5_ROUND(NET_MD5_F1, D, A, B, C, x[ 1u], NET_MD5_S12, 0xE8C7B756u);
    NET_MD5_ROUND(NET_MD5_F1, C, D, A, B, x[ 2u], NET_MD5_S13, 0x242070DBu);
    NET_MD5_ROUND(NET_MD5_F1, B, C, D, A, x[ 3u], NET_MD5_S14, 0xC1BDCEEEu);
    NET_MD5_ROUND(NET_MD5_F1, A, B, C, D, x[ 4u], NET_MD5_S11, 0xF57C0FAFu);
    NET_MD5_ROUND(NET_MD5_F1, D, A, B, C, x[ 5u], NET_MD5_S12, 0x4787C62Au);
    NET_MD5_ROUND(NET_MD5_F1, C, D, A, B, x[ 6u], NET_MD5_S13, 0xA8304613u);
    NET_MD5_ROUND(NET_MD5_F1, B, C, D, A, x[ 7u], NET_MD5_S14, 0xFD469501u);
    NET_MD5_ROUND(NET_MD5_F1, A, B, C, D, x[ 8u], NET_MD5_S11, 0x698098D8u);
    NET_MD5_ROUND(NET_MD5_F1, D, A, B, C, x[ 9u], NET_MD5_S12, 0x8B44F7AFu);
    NET_MD5_ROUND(NET_MD5_F1, C, D, A, B, x[10u], NET_MD5_S13, 0xFFFF5BB1u);
    NET_MD5_ROUND(NET_MD5_F1, B, C, D, A, x[11u], NET_MD5_S14, 0x895CD7BEu);
    NET_MD5_ROUND(NET_MD5_F1, A, B, C, D, x[12u], NET_MD5_S11, 0x6B901122u);
    NET_MD5_ROUND(NET_MD5_F1, D, A, B, C, x[13u], NET_MD5_S12, 0xFD987193u);
    NET_MD5_ROUND(NET_MD5_F1, C, D, A, B, x[14u], NET_MD5_S13, 0xA679438Eu);
    NET_MD5_ROUND(NET_MD5_F1, B, C, D, A, x[15u], NET_MD5_S14, 0x49B40821u);

                                                                /* Round 2.                                             */

    NET_MD5_ROUND(NET_MD5_F2, A, B, C, D, x[ 1u], NET_MD5_S21, 0xF61E2562u);
    NET_MD5_ROUND(NET_MD5_F2, D, A, B, C, x[ 6u], NET_MD5_S22, 0xC040B340u);
    NET_MD5_ROUND(NET_MD5_F2, C, D, A, B, x[11u], NET_MD5_S23, 0x265E5A51u);
    NET_MD5_ROUND(NET_MD5_F2, B, C, D, A, x[ 0u], NET_MD5_S24, 0xE9B6C7AAu);
    NET_MD5_ROUND(NET_MD5_F2, A, B, C, D, x[ 5u], NET_MD5_S21, 0xD62F105Du);
    NET_MD5_ROUND(NET_MD5_F2, D, A, B, C, x[10u], NET_MD5_S22, 0x02441453u);
    NET_MD5_ROUND(NET_MD5_F2, C, D, A, B, x[15u], NET_MD5_S23, 0xD8A1E681u);
    NET_MD5_ROUND(NET_MD5_F2, B, C, D, A, x[ 4u], NET_MD5_S24, 0xE7D3FBC8u);
    NET_MD5_ROUND(NET_MD5_F2, A, B, C, D, x[ 9u], NET_MD5_S21, 0x21E1CDE6u);
    NET_MD5_ROUND(NET_MD5_F2, D, A, B, C, x[14u], NET_MD5_S22, 0xC33707D6u);
    NET_MD5_ROUND(NET_MD5_F2, C, D, A, B, x[ 3u], NET_MD5_S23, 0xF4D50D87u);
    NET_MD5_ROUND(NET_MD5_F2, B, C, D, A, x[ 8u], NET_MD5_S24, 0x455A14EDu);
    NET_MD5_ROUND(NET_MD5_F2, A, B, C, D, x[13u], NET_MD5_S21, 0xA9E3E905u);
    NET_MD5_ROUND(NET_MD5_F2, D, A, B, C, x[ 2u], NET_MD5_S22, 0xFCEFA3F8u);
    NET_MD5_ROUND(NET_MD5_F2, C, D, A, B, x[ 7u], NET_MD5_S23, 0x676F02D9u);
    NET_MD5_ROUND(NET_MD5_F2, B, C, D, A, x[12u], NET_MD5_S24, 0x8D2A4C8Au);

                                                                /* Round 3.                                             */

    NET_MD5_ROUND(NET_MD5_F3, A, B, C, D, x[ 5u], NET_MD5_S31, 0xFFFA3942u);
    NET_MD5_ROUND(NET_MD5_F3, D, A, B, C, x[ 8u], NET_MD5_S32, 0x8771F681u);
    NET_MD5_ROUND(NET_MD5_F3, C, D, A, B, x[11u], NET_MD5_S33, 0x6D9D6122u);
    NET_MD5_ROUND(NET_MD5_F3, B, C, D, A, x[14u], NET_MD5_S34, 0xFDE5380Cu);
    NET_MD5_ROUND(NET_MD5_F3, A, B, C, D, x[ 1u], NET_MD5_S31, 0xA4BEEA44u);
    NET_MD5_ROUND(NET_MD5_F3, D, A, B, C, x[ 4u], NET_MD5_S32, 0x4BDECFA9u);
    NET_MD5_ROUND(NET_MD5_F3, C, D, A, B, x[ 7u], NET_MD5_S33, 0xF6BB4B60u);
    NET_MD5_ROUND(NET_MD5_F3, B, C, D, A, x[10u], NET_MD5_S34, 0xBEBFBC70u);
    NET_MD5_ROUND(NET_MD5_F3, A, B, C, D, x[13u], NET_MD5_S31, 0x289B7EC6u);
    NET_MD5_ROUND(NET_MD5_F3, D, A, B, C, x[ 0u], NET_MD5_S32, 0xEAA127FAu);
    NET_MD5_ROUND(NET_MD5_F3, C, D, A, B, x[ 3u], NET_MD5_S33, 0xD4EF3085u);
    NET_MD5_ROUND(NET_MD5_F3, B, C, D, A, x[ 6u], NET_MD5_S34, 0x04881D05u);
    NET_MD5_ROUND(NET_MD5_F3, A, B, C, D, x[ 9u], NET_MD5_S31, 0xD9D4D039u);
    NET_MD5_ROUND(NET_MD5_F3, D, A, B, C, x[12u], NET_MD5_S32, 0xE6DB99E5u);
    NET_MD5_ROUND(NET_MD5_F3, C, D, A, B, x[15u], NET_MD5_S33, 0x1FA27CF8u);
    NET_MD5_ROUND(NET_MD5_F3, B, C, D, A, x[ 2u], NET_MD5_S34, 0xC4AC5665u);

                                                                /* Round 4.                                             */

    NET_MD5_ROUND(NET_MD5_F4, A, B, C, D, x[ 0u], NET_MD5_S41, 0xF4292244u);
    NET_MD5_ROUND(NET_MD5_F4, D, A, B, C, x[ 7u], NET_MD5_S42, 0x432AFF97u);
    NET_MD5_ROUND(NET_MD5_F4, C, D, A, B, x[14u], NET_MD5_S43, 0xAB9423A7u);
    NET_MD5_ROUND(NET_MD5_F4, B, C, D, A, x[ 5u], NET_MD5_S44, 0xFC93A039u);
    NET_MD5_ROUND(NET_MD5_F4, A, B, C, D, x[12u], NET_MD5_S41, 0x655B59C3u);
    NET_MD5_ROUND(NET_MD5_F4, D, A, B, C, x[ 3u], NET_MD5_S42, 0x8F0CCC92u);
    NET_MD5_ROUND(NET_MD5_F4, C, D, A, B, x[10u], NET_MD5_S43, 0xFFEFF47Du);
    NET_MD5_ROUND(NET_MD5_F4, B, C, D, A, x[ 1u], NET_MD5_S44, 0x85845DD1u);
    NET_MD5_ROUND(NET_MD5_F4, A, B, C, D, x[ 8u], NET_MD5_S41, 0x6FA87E4Fu);
    NET_MD5_ROUND(NET_MD5_F4, D, A, B, C, x[15u], NET_MD5_S42, 0xFE2CE6E0u);
    NET_MD5_ROUND(NET_MD5_F4, C, D, A, B, x[ 6u], NET_MD5_S43, 0xA3014314u);
    NET_MD5_ROUND(NET_MD5_F4, A, B, C, D, x[ 4u], NET_MD5_S44, 0xF7537E82u);
    NET_MD5_ROUND(NET_MD5_F4, B, C, D, A, x[13u], NET_MD5_S41, 0x4E0811A1u);
    NET_MD5_ROUND(NET_MD5_F4, D, A, B, C, x[11u], NET_MD5_S42, 0xBD3AF235u);
    NET_MD5_ROUND(NET_MD5_F4, C, D, A, B, x[ 2u], NET_MD5_S43, 0x2AD7D2BBu);
    NET_MD5_ROUND(NET_MD5_F4, B, C, D, A, x[ 9u], NET_MD5_S44, 0xEB86D391u);

    state[0u] += A;
    state[1u] += B;
    state[2u] += C;
    state[3u] += D;
}
