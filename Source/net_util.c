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
*                                       NETWORK UTILITY LIBRARY
*
* Filename : net_util.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) NO compiler-supplied standard library functions are used by the network protocol suite.
*                'net_util.*' implements ALL network-specific library functions.
*
*                See also 'net.h  NETWORK INCLUDE FILES  Note #3'.
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
#define    NET_UTIL_MODULE
#include  <cpu_core.h>
#include  <lib_math.h>
#include  "net_util.h"
#include  "net_buf.h"
#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_UTIL_16_BIT_ONES_CPL_NEG_ZERO            0xFFFFu
#define  NET_UTIL_32_BIT_ONES_CPL_NEG_ZERO        0xFFFFFFFFu

#define  NET_UTIL_16_BIT_SUM_ERR_NONE             DEF_BIT_NONE
#define  NET_UTIL_16_BIT_SUM_ERR_NULL_SIZE        DEF_BIT_01
#define  NET_UTIL_16_BIT_SUM_ERR_LAST_OCTET       DEF_BIT_02


/*
*********************************************************************************************************
*                                           CRC-32 DEFINES
*
* Note(s) : (1) IEEE 802.3 CRC-32 uses the following binary polynomial :
*
*               (a) x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10
*                        + x^8  + x^7  + x^5  + x^4  + x^2  + x^1  + x^0
*********************************************************************************************************
*/

#define  NET_UTIL_32_BIT_CRC_POLY                 0x04C11DB7u   /*  = 0000 0100 1100 0001 0001 1101 1011 0111           */
#define  NET_UTIL_32_BIT_CRC_POLY_REFLECT         0xEDB88320u   /*  = 1110 1101 1011 1000 1000 0011 0010 0000           */

/*
*********************************************************************************************************
*                                            LOCAL TABLES
*
* Note(s): (1) This table represents the alphabet for the base-64 encoder.
*********************************************************************************************************
*/
                                                                /* See Note #1.                                         */

/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/



static  void        NetUtil_RandSetSeed            (void);


static  CPU_INT32U  NetUtil_16BitSumHdrCalc        (void         *phdr,
                                                    CPU_INT16U    hdr_size);

static  CPU_INT32U  NetUtil_16BitSumDataCalc       (void         *p_data,
                                                    CPU_INT16U    data_size,
                                                    CPU_INT08U   *poctet_prev,
                                                    CPU_INT08U   *poctet_last,
                                                    CPU_BOOLEAN   prev_octet_valid,
                                                    CPU_BOOLEAN   last_pkt_buf,
                                                    CPU_INT08U   *psum_err);

static  CPU_INT16U  NetUtil_16BitOnesCplSumDataCalc(void         *pdata_buf,
                                                    void         *ppseudo_hdr,
                                                    CPU_INT16U    pseudo_hdr_size,
                                                    NET_ERR      *p_err);

/*
*********************************************************************************************************
*                                 NetUtil_16BitOnesCplChkSumHdrCalc()
*
* Description : Calculate 16-bit one's-complement check-sum on packet header.
*
*               (1) See RFC #1071, Sections 1, 2.(1), & 4.1 for summary of 16-bit one's-complement
*                   check-sum & algorithm.
*
*               (2) To correctly calculate 16-bit one's-complement check-sums on memory buffers of any
*                   octet-length & word-alignment, the check-sums MUST be calculated in network-order
*                   on headers that are arranged in network-order (see also 'NetUtil_16BitSumHdrCalc()
*                   Note #5b').
*
*
* Argument(s) : phdr        Pointer to packet header.
*
*               hdr_size    Size    of packet header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE           Check-sum calculated; check return value.
*                               NET_UTIL_ERR_NULL_PTR       Argument 'phdr'     passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE      Argument 'hdr_size' passed a zero size.
*
* Return(s)   : 16-bit one's-complement check-sum, if NO error(s).
*
*               0,                                 otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (3) (a) Since the 16-bit sum calculation is returned as a 32-bit network-order value
*                       (see 'NetUtil_16BitSumHdrCalc()  Note #5c1'), ...
*
*                   (b) ... the final check-sum MUST be converted to host-order but MUST NOT be re-
*                       converted back to network-order (see 'NetUtil_16BitSumHdrCalc()  Note #5c3').
*********************************************************************************************************
*/

NET_CHK_SUM  NetUtil_16BitOnesCplChkSumHdrCalc (void        *phdr,
                                                CPU_INT16U   hdr_size,
                                                NET_ERR     *p_err)
{
    CPU_INT32U   sum;
    NET_CHK_SUM  chk_sum;
    NET_CHK_SUM  chk_sum_host;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (phdr == (void *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
                                                                /* ------------------ VALIDATE SIZE ------------------- */
    if (hdr_size < 1) {
       *p_err =  NET_UTIL_ERR_NULL_SIZE;
        return (0u);
    }
#endif

                                                                /* --------- CALC HDR'S 16-BIT ONE'S-CPL SUM ---------- */
    sum = NetUtil_16BitSumHdrCalc(phdr, hdr_size);              /* Calc  16-bit sum (see Note #3a).                     */

    while (sum >> 16u) {                                        /* While 16-bit sum ovf's, ...                          */
        sum = (sum & 0x0000FFFFu) + (sum >> 16u);               /* ... sum ovf bits back into 16-bit one's-cpl sum.     */
    }


    chk_sum      = (NET_CHK_SUM)(~((NET_CHK_SUM)sum));          /* Perform one's cpl on one's-cpl sum.                  */
    chk_sum_host =  NET_UTIL_NET_TO_HOST_16(chk_sum);           /* Conv back to host-order (see Note #3b).              */

   *p_err        =  NET_UTIL_ERR_NONE;

    return (chk_sum_host);                                      /* Rtn 16-bit chk sum (see Note #3).                    */
}


/*
*********************************************************************************************************
*                                 NetUtil_16BitOnesCplChkSumHdrVerify()
*
* Description : (1) Verify 16-bit one's-complement check-sum on packet header :
*
*                   (a) Calculate one's-complement sum on packet header
*                   (b) Verify check-sum by comparison of one's-complement sum to one's-complement
*                         '-0' value (negative zero)
*
*               (2) See RFC #1071, Sections 1, 2.(1), & 4.1 for summary of 16-bit one's-complement
*                   check-sum & algorithm.
*
*               (3) To correctly calculate 16-bit one's-complement check-sums on memory buffers of any
*                   octet-length & word-alignment, the check-sums MUST be calculated in network-order
*                   on headers that are arranged in network-order (see also 'NetUtil_16BitSumHdrCalc()
*                   Note #5b').
*
*
* Argument(s) : phdr        Pointer to packet header.
*
*               hdr_size    Size    of packet header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE           Check-sum verified; check return value.
*                               NET_UTIL_ERR_NULL_PTR       Argument 'phdr'     passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE      Argument 'hdr_size' passed a zero size.
*
* Return(s)   : DEF_OK,   if valid check-sum.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (4) (a) Since the 16-bit sum calculation is returned as a 32-bit network-order value
*                       (see 'NetUtil_16BitSumHdrCalc()  Note #5c1'), ...
*
*                   (b) ... the check-sum MUST be converted to host-order but MUST NOT be re-converted
*                       back to network-order for the final check-sum comparison
*                       (see 'NetUtil_16BitSumHdrCalc()  Note #5c3').
*********************************************************************************************************
*/

CPU_BOOLEAN  NetUtil_16BitOnesCplChkSumHdrVerify (void        *phdr,
                                                  CPU_INT16U   hdr_size,
                                                  NET_ERR     *p_err)
{
    CPU_INT32U   sum;
    NET_CHK_SUM  chk_sum;
    NET_CHK_SUM  chk_sum_host;
    CPU_BOOLEAN  valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (phdr == (void *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (DEF_FAIL);
    }
                                                                /* ------------------ VALIDATE SIZE ------------------- */
    if (hdr_size < 1) {
       *p_err =  NET_UTIL_ERR_NULL_SIZE;
        return (DEF_FAIL);
    }
#endif

                                                                /* -------- VERIFY HDR'S 16-BIT ONE'S-CPL SUM --------- */
    sum = NetUtil_16BitSumHdrCalc(phdr, hdr_size);              /* Calc 16-bit sum (see Note #4a).                      */

    while (sum >> 16u) {                                        /* While 16-bit sum ovf's, ...                          */
        sum = (sum & 0x0000FFFFu) + (sum >> 16u);               /* ... sum ovf bits back into 16-bit one's-cpl sum.     */
    }

    chk_sum      = (NET_CHK_SUM)sum;
    chk_sum_host =  NET_UTIL_NET_TO_HOST_16(chk_sum);           /* Conv back to host-order (see Note #4b).              */

                                                                /* Verify chk sum (see Note #1b).                       */
    valid        = (chk_sum_host == NET_UTIL_16_BIT_ONES_CPL_NEG_ZERO) ? DEF_OK : DEF_FAIL;

   *p_err        =  NET_UTIL_ERR_NONE;

    return (valid);
}


/*
*********************************************************************************************************
*                                NetUtil_16BitOnesCplChkSumDataCalc()
*
* Description : Calculate 16-bit one's-complement check-sum on packet data.
*
*               (1) See RFC #1071, Sections 1, 2.(1), & 4.1 for summary of 16-bit one's-complement
*                   check-sum & algorithm.
*
*               (2) Check-sum calculated on packet data encapsulated in :
*
*                   (a) One or more network buffers             Support non-fragmented & fragmented packets
*                   (b) Transport layer pseudo-header           See RFC #768, Section 'Fields : Checksum' &
*                                                                   RFC #793, Section 3.1 'Header Format :
*                                                                       Checksum'.
*
*               (3) To correctly calculate 16-bit one's-complement check-sums on memory buffers of any
*                   octet-length & word-alignment, the check-sums MUST be calculated in network-order on
*                   data & headers that are arranged in network-order (see also 'NetUtil_16BitSumDataCalc()
*                   Note #5b').
*
*
* Argument(s) : pdata_buf           Pointer to packet data network buffer(s) (see Note #2a).
*
*               ppseudo_hdr         Pointer to transport layer pseudo-header (see Note #2b).
*
*               pseudo_hdr_size     Size    of transport layer pseudo-header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE               Check-sum calculated; check return value.
*
*                                                               - RETURNED BY NetUtil_16BitOnesCplSumDataCalc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Packet data is a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*
* Return(s)   : 16-bit one's-complement check-sum, if NO error(s).
*
*               0,                                 otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (4) (a) Since the 16-bit one's-complement check-sum calculations are returned in host-
*                       order, ...
*
*                   (b) ... the returned check-sum MUST NOT be re-converted back to network-order.
*
*                   See also 'NetUtil_16BitSumDataCalc()         Note #5c3' &
*                            'NetUtil_16BitOnesCplSumDataCalc()  Note #5'.
*********************************************************************************************************
*/

NET_CHK_SUM  NetUtil_16BitOnesCplChkSumDataCalc (void        *pdata_buf,
                                                 void        *ppseudo_hdr,
                                                 CPU_INT16U   pseudo_hdr_size,
                                                 NET_ERR     *p_err)
{
    CPU_INT16U   sum;
    NET_CHK_SUM  chk_sum;

                                                                /* Calc 16-bit one's-cpl sum (see Note #4a).            */
    sum = NetUtil_16BitOnesCplSumDataCalc(pdata_buf, ppseudo_hdr, pseudo_hdr_size, p_err);
    if (*p_err != NET_UTIL_ERR_NONE) {
         return (0u);
    }

    chk_sum = (NET_CHK_SUM)(~((NET_CHK_SUM)sum));               /* Perform one's cpl on one's-cpl sum.                  */

   *p_err   =  NET_UTIL_ERR_NONE;

    return (chk_sum);                                           /* Rtn 16-bit chk sum (see Note #4).                    */
}


/*
*********************************************************************************************************
*                               NetUtil_16BitOnesCplChkSumDataVerify()
*
* Description : (1) Verify 16-bit one's-complement check-sum on packet data :
*
*                   (a) Calculate one's-complement sum on packet data & packet pseudo-header
*                   (b) Verify check-sum by comparison of one's-complement sum to one's-complement
*                         '-0' value (negative zero)
*
*               (2) See RFC #1071, Sections 1, 2.(1), & 4.1 for summary of 16-bit one's-complement
*                   check-sum & algorithm.
*
*               (3) Check-sum calculated on packet data encapsulated in :
*
*                   (a) One or more network buffers             Support non-fragmented & fragmented packets
*                   (b) Transport layer pseudo-header           See RFC #768, Section 'Fields : Checksum' &
*                                                                   RFC #793, Section 3.1 'Header Format :
*                                                                       Checksum'.
*
*               (4) To correctly calculate 16-bit one's-complement check-sums on memory buffers of any
*                   octet-length & word-alignment, the check-sums MUST be calculated in network-order on
*                   data & headers that are arranged in network-order (see also 'NetUtil_16BitSumDataCalc()
*                   Note #5b').
*
*
* Argument(s) : pdata_buf           Pointer to packet data network buffer(s) (see Note #3a).
*
*               ppseudo_hdr         Pointer to transport layer pseudo-header (see Note #3b).
*
*               pseudo_hdr_size     Size    of transport layer pseudo-header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE               Check-sum calculated; check return value.
*
*                                                               - RETURNED BY NetUtil_16BitOnesCplSumDataCalc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Packet data is a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*
* Return(s)   : DEF_OK,   if valid check-sum.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (5) (a) Since the 16-bit one's-complement check-sum calculations are returned in host-
*                       order, ...
*
*                   (b) ... the returned check-sum MUST NOT be re-converted back to network-order for
*                       the final check-sum comparison.
*
*                   See also 'NetUtil_16BitSumDataCalc()         Note #5c3' &
*                            'NetUtil_16BitOnesCplSumDataCalc()  Note #5'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetUtil_16BitOnesCplChkSumDataVerify (void        *pdata_buf,
                                                   void        *ppseudo_hdr,
                                                   CPU_INT16U   pseudo_hdr_size,
                                                   NET_ERR     *p_err)
{
    CPU_INT16U   sum;
    NET_CHK_SUM  chk_sum;
    CPU_BOOLEAN  valid;

                                                                /* Calc 16-bit one's-cpl sum (see Note #5a).            */
    sum = NetUtil_16BitOnesCplSumDataCalc(pdata_buf, ppseudo_hdr, pseudo_hdr_size, p_err);
    if (*p_err != NET_UTIL_ERR_NONE) {
         return (DEF_FAIL);
    }
                                                                /* Verify chk sum (see Notes #1b & #5b).                */
    chk_sum = (NET_CHK_SUM)sum;
    valid   = (chk_sum == NET_UTIL_16_BIT_ONES_CPL_NEG_ZERO) ? DEF_OK : DEF_FAIL;

   *p_err   =  NET_UTIL_ERR_NONE;

    return (valid);
}


/*
*********************************************************************************************************
*                                       NetUtil_32BitCRC_Calc()
*
* Description : Calculate 32-bit CRC.
*
* Argument(s) : p_data      Pointer to data to CRC.
*
*               data_len    Length  of data to CRC.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE               32-bit CRC successfully calculated;
*                                                                   check return value.
*                               NET_UTIL_ERR_NULL_PTR           Argument 'p_data'   passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'data_len' passed a NULL size.
*
* Return(s)   : 32-bit CRC, if NO error(s).
*
*               0,          otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function but MAY be called by
*               application function(s).
*
* Note(s)     : (1) IEEE 802.3 CRC-32 uses the following binary polynomial :
*
*                   (a) x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10
*                            + x^8  + x^7  + x^5  + x^4  + x^2  + x^1  + x^0
*********************************************************************************************************
*/

CPU_INT32U  NetUtil_32BitCRC_Calc (CPU_INT08U  *p_data,
                                   CPU_INT32U   data_len,
                                   NET_ERR     *p_err)
{
    CPU_INT32U   crc;
    CPU_INT32U   poly;
    CPU_INT32U   crc_data_val;
    CPU_INT32U   crc_data_val_bit_zero;
    CPU_INT32U   i;
    CPU_INT32U   j;
    CPU_INT08U  *pdata_val;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_data == (CPU_INT08U *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
                                                                /* ------------------ VALIDATE SIZE ------------------- */
    if (data_len < 1) {
       *p_err =  NET_UTIL_ERR_NULL_SIZE;
        return (0u);
    }
#endif

                                                                /* ----------------- CALC 32-BIT CRC ------------------ */
    crc  = NET_UTIL_32_BIT_ONES_CPL_NEG_ZERO;                   /* Init CRC to neg zero.                                */
    poly = NET_UTIL_32_BIT_CRC_POLY_REFLECT;                    /* Init reflected poly.                                 */

    pdata_val = p_data;
    for (i = 0u; i < data_len; i++) {
        crc_data_val = (CPU_INT32U)((crc ^ *pdata_val) & DEF_OCTET_MASK);

        for (j = 0u; j < DEF_OCTET_NBR_BITS; j++) {
            crc_data_val_bit_zero = crc_data_val & DEF_BIT_00;
            if (crc_data_val_bit_zero > 0) {
                crc_data_val = (crc_data_val >> 1u) ^ poly;
            } else {
                crc_data_val = (crc_data_val >> 1u);
            }
        }

        crc = (crc >> DEF_OCTET_NBR_BITS) ^ crc_data_val;
        pdata_val++;
    }


   *p_err =  NET_UTIL_ERR_NONE;

    return (crc);
}


/*
*********************************************************************************************************
*                                     NetUtil_32BitCRC_CalcCpl()
*
* Description : Calculate 32-bit CRC with complement.
*
* Argument(s) : p_data      Pointer to data to CRC.
*
*               data_len    Length  of data to CRC.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE               32-bit complemented CRC successfully
*                                                                   calculated; check return value.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : --
*                               NET_UTIL_ERR_NULL_PTR           Argument 'p_data'   passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'data_len' passed a NULL size.
*
* Return(s)   : 32-bit complemented CRC, if NO error(s).
*
*               0,                       otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function but MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetUtil_32BitCRC_CalcCpl (CPU_INT08U  *p_data,
                                      CPU_INT32U   data_len,
                                      NET_ERR     *p_err)
{
    CPU_INT32U  crc;


    crc = NetUtil_32BitCRC_Calc(p_data, data_len, p_err);       /* Calc CRC.                                            */
    if (*p_err != NET_UTIL_ERR_NONE) {
         return (0u);
    }

    crc ^= NET_UTIL_32_BIT_ONES_CPL_NEG_ZERO;                   /* Cpl  CRC.                                            */

   *p_err = NET_UTIL_ERR_NONE;

    return (crc);
}


/*
*********************************************************************************************************
*                                       NetUtil_32BitReflect()
*
* Description : Calculate 32-bit reflection.
*
* Argument(s) : val       32-bit value to reflect.
*
* Return(s)   : 32-bit reflection.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function but MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetUtil_32BitReflect (CPU_INT32U  val)
{
    CPU_INT32U  val_reflect;
    CPU_INT32U  bit;
    CPU_INT32U  bit_nbr;
    CPU_INT32U  bit_val;
    CPU_INT32U  bit_reflect;
    CPU_DATA    i;


    val_reflect = 0u;
    bit_nbr     = sizeof(val) * DEF_OCTET_NBR_BITS;
    bit         = DEF_BIT(0u);
    bit_reflect = DEF_BIT(bit_nbr - 1u);

    for (i = 0u; i < bit_nbr; i++) {
        bit_val = val & bit;
        if (bit_val > 0) {                                      /* If val's bit set, ...                                */
            val_reflect |= bit_reflect;                         /* ... set corresponding reflect bit.                   */
        }
        bit         <<= 1u;
        bit_reflect >>= 1u;
    }

    return (val_reflect);
}


/*
*********************************************************************************************************
*                                          NetUtil_TS_Get()
*
* Description : Get current Internet Timestamp.
*
*               (1) "The Timestamp is a right-justified, 32-bit timestamp in milliseconds since midnight
*                    UT [Universal Time]" (RFC #791, Section 3.1 'Options : Internet Timestamp').
*
*               (2) The developer is responsible for providing a real-time clock with correct time-zone
*                   configuration to implement the Internet Timestamp, if possible.
*
*
* Argument(s) : none.
*
* Return(s)   : Internet Timestamp.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function but MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_TS  NetUtil_TS_Get (void)
{
    NET_TS  ts;


    ts = (NET_TS) NetUtil_TS_Get_ms();

    return (ts);
}


/*
*********************************************************************************************************
*                                         NetUtil_TS_Get_ms()
*
* Description : Get current millisecond timestamp.
*
*               (1) (a) (1) Although RFC #2988, Section 4 states that "there is no requirement for the
*                           clock granularity G used for computing [TCP] RTT measurements ... experience
*                           has shown that finer clock granularities (<= 100 msec) perform somewhat
*                           better than more coarse granularities".
*
*                       (2) (A) RFC #2988, Section 2.4 states that "whenever RTO is computed, if it is
*                               less than 1 second then the RTO SHOULD be rounded up to 1 second".
*
*                           (B) RFC #1122, Section 4.2.3.1 states that "the recommended ... RTO ... upper
*                               bound should be 2*MSL" where RFC #793, Section 3.3 'Sequence Numbers :
*                               Knowing When to Keep Quiet' states that "the Maximum Segment Lifetime
*                               (MSL) is ... to be 2 minutes".
*
*                               Therefore, the required upper bound is :
*
*                                   2 * MSL = 2 * 2 minutes = 4 minutes = 240 seconds
*
*                   (b) Therefore, the developer is responsible for providing a timestamp clock with
*                       adequate resolution to satisfy the clock granularity (see Note #1a1) & adequate
*                       range to satisfy the minimum/maximum TCP RTO values  (see Note #1a2).
*
* Argument(s) : none.
*
* Return(s)   : Timestamp, in milliseconds.
*
* Caller(s)   : NetIF_PerfMonHandler(),
*               NetTCP_RxPktValidate(),
*               NetTCP_TxPktPrepareHdr().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) (a) To avoid  timestamp calculation overflow, timestamps are updated by continually
*                       summing OS time tick delta differences converted into milliseconds :
*
*                                  Total                       [                        1000 ms/sec  ]
*                           (A)  Timestamp  =    Summation     [ (time  - time   )  *  ------------- ]
*                                 (in ms)     i = 1 --> i = N  [      i       i-1       M ticks/sec  ]
*
*
*                                   where
*                                           time        Instantaneous time value (in OS ticks/second)
*                                           M           Number of OS time ticks per second
*
*
*                       (1) However, multiplicative overflow is NOT totally avoided if the product of
*                           the OS time tick delta difference & the constant time scalar (i.e. 1000
*                           milliseconds per second) overflows the integer data type :
*
*                           (A)                 (time_delta * time_scalar)  >=  2^N
*
*                                   where
*                                           time_delta      Calculated time delta difference
*                                           time_scalar     Constant   time scalar (e.g. 1000 ms/1 sec)
*                                           N               Number of data type bits (e.g. 32)
*
*
*                   (b) To ensure timestamp calculation accuracy, timestamp calculations sum timestamp
*                       integer remainders back into total accumulated timestamp :
*
*                                  Total                         [                        1000 ms/sec  ]
*                           (A)  Timestamp  =      Summation     [ (time  - time   )  *  ------------- ]
*                                 (in ms)       i = 1 --> i = N  [      i       i-1       M ticks/sec  ]
*
*                                                                [                                     ]
*                                                  Summation     [ (time  - time   )  *   1000 ms/sec  ]  modulo  (M ticks/sec)
*                                               i = 1 --> i = N  [      i       i-1                    ]
*                                           +  ---------------------------------------------------------------------------------
*
*                                                                                M ticks/sec
*
*
*                                   where
*                                           time        Instantaneous time value (in OS ticks/second)
*                                           M           Number of OS time ticks per second
*
*
*                       (1) However, these calculations are required only when the OS time ticks per
*                           second rate is not an integer multiple of the constant time scalar (i.e.
*                           1000 milliseconds per second).
*********************************************************************************************************
*/

NET_TS_MS  NetUtil_TS_Get_ms (void)
{
#if CPU_CFG_TS_32_EN == DEF_ENABLED
    static  CPU_BOOLEAN      ts_active           = DEF_NO;
            CPU_INT32U       ts_delta;
    static  CPU_INT32U       ts_prev             = 0u;
            CPU_TS32         ts_cur;
            CPU_TS_TMR_FREQ  ts_tmr_freq;
    static  NET_TS_MS        ts_ms_tot           = 0u;
    static  NET_TS_MS        ts_ms_delta_rem_tot = 0u;
            NET_TS_MS        ts_ms_delta_rem_ovf;
            NET_TS_MS        ts_ms_delta_rem;
            NET_TS_MS        ts_ms_delta_num;
            NET_TS_MS        ts_ms_delta;
            CPU_ERR          err;


    ts_cur      = CPU_TS_Get32();
    ts_tmr_freq = CPU_TS_TmrFreqGet(&err);
    if (err != CPU_ERR_NONE) {
        ts_tmr_freq = 0;
    }


    if (ts_tmr_freq > 0u) {
        if (ts_active == DEF_YES) {                                     /* If active, calc & update ts :                */
            ts_delta = ts_cur - ts_prev;                                /*     Calc time delta (in TS).                 */

            if ((DEF_TIME_NBR_mS_PER_SEC >= ts_tmr_freq) &&
               ((DEF_TIME_NBR_mS_PER_SEC %  ts_tmr_freq) == 0u)) {
                                                                        /*     Calc   ts delta (in ms).                 */
                ts_ms_delta          = (NET_TS_MS)(ts_delta * (DEF_TIME_NBR_mS_PER_SEC / ts_tmr_freq));
                ts_ms_tot           += (NET_TS_MS) ts_ms_delta;         /*     Update ts tot   (in ms) [see Note #2a].  */

            } else {
                                                                        /*     Calc   ts delta (in ms) [see Note #2a1]. */
                ts_ms_delta_num      = (NET_TS_MS)(ts_delta        * DEF_TIME_NBR_mS_PER_SEC);
                ts_ms_delta          = (NET_TS_MS)(ts_ms_delta_num / ts_tmr_freq);
                ts_ms_tot           += (NET_TS_MS) ts_ms_delta;         /*     Update ts tot   (in ms) [see Note #2a].  */
                                                                        /*     Calc   ts delta rem ovf (in ms) ...      */
                ts_ms_delta_rem      = (NET_TS_MS)(ts_ms_delta_num % ts_tmr_freq);
                ts_ms_delta_rem_tot +=  ts_ms_delta_rem;
                ts_ms_delta_rem_ovf  =  ts_ms_delta_rem_tot / ts_tmr_freq;
                ts_ms_delta_rem_tot -=  ts_ms_delta_rem_ovf * ts_tmr_freq;
                ts_ms_tot           +=  ts_ms_delta_rem_ovf;            /* ... & adj  ts tot by ovf    (see Note #2b).  */
            }

        } else {
            ts_active = DEF_YES;
        }

        ts_prev  = ts_cur;                                              /* Save cur time for next ts update.            */

    } else {
        ts_ms_tot += (NET_TS_MS)ts_cur;
    }


    return (ts_ms_tot);
#else
    static  NET_TS_MS    ts_ms_delta_rem_tot = 0u;
    static  CPU_BOOLEAN  ts_active           = DEF_NO;
            NET_TS_MS    ts_ms_delta;
    static  NET_TS_MS    ts_ms_tot           = 0u;
            KAL_TICK     tick_cur;
    static  KAL_TICK     tick_prev           = 0u;
            KAL_TICK     tick_delta;
            KAL_ERR      err;


    tick_cur = KAL_TickGet(&err);
   (void)&err;

    if (KAL_TickRate > 0) {
        if (ts_active == DEF_YES) {                             /* If active, calc & update ts :                        */

            tick_delta =  tick_cur - tick_prev;                 /* Calc time delta (in OS ticks).                       */

            if ( (DEF_TIME_NBR_mS_PER_SEC > KAL_TickRate)        &&
                ((DEF_TIME_NBR_mS_PER_SEC % KAL_TickRate) == 0u)) {
                                                                /* Calc   ts delta (in ms).                             */
                ts_ms_delta          = (NET_TS_MS)(tick_delta  * (DEF_TIME_NBR_mS_PER_SEC / KAL_TickRate));
                ts_ms_tot           += (NET_TS_MS) ts_ms_delta; /* Update ts tot   (in ms) [see Note #2a].              */

            } else {
                NET_TS_MS    ts_ms_delta_rem_ovf;
                NET_TS_MS    ts_ms_delta_rem;
                NET_TS_MS    ts_ms_delta_num;


                                                                /* Calc   ts delta (in ms) [see Note #2a1].             */
                ts_ms_delta_num      =  tick_delta   * DEF_TIME_NBR_mS_PER_SEC;
                ts_ms_delta          =  ts_ms_delta_num / KAL_TickRate;
                ts_ms_tot           +=  ts_ms_delta; /* Update ts tot   (in ms) [see Note #2a].              */
                                                                /* Calc   ts delta rem ovf (in ms) ...                  */
                ts_ms_delta_rem      =  ts_ms_delta_num % KAL_TickRate;
                ts_ms_delta_rem_tot +=  ts_ms_delta_rem;
                ts_ms_delta_rem_ovf  =  ts_ms_delta_rem_tot / KAL_TickRate;
                ts_ms_delta_rem_tot -=  ts_ms_delta_rem_ovf * KAL_TickRate;
                ts_ms_tot           +=  ts_ms_delta_rem_ovf;    /* ... & adj  ts tot by ovf    (see Note #2b).          */
            }

        } else {
            ts_active = DEF_YES;
        }

        tick_prev = tick_cur;                                   /* Save cur time for next ts update.                    */

    } else {
        ts_ms_tot += tick_cur;
    }

    return (ts_ms_tot);
#endif
}


/*
*********************************************************************************************************
*                                      NetUtil_TimeSec_uS_To_ms()
*
* Description : Convert seconds and microseconds values to milliseconds.
*
* Argument(s) : time_sec    seconds
*
*               time_us     microseconds
*
* Return(s)   : Number of milliseconds
*
* Caller(s)   : NetSock_Sel(),
*               Net_TimeDly().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetUtil_TimeSec_uS_To_ms (CPU_INT32U  time_sec,
                                      CPU_INT32U  time_us)
{
    CPU_INT32U  time_us_to_ms;
    CPU_INT32U  time_us_to_ms_max;
    CPU_INT32U  time_sec_to_ms;
    CPU_INT32U  time_sec_to_ms_max;
    CPU_INT32U  time_dly_ms;


    if ((time_sec == NET_TMR_TIME_INFINITE) &&
        (time_us  == NET_TMR_TIME_INFINITE)) {
        time_dly_ms = NET_TMR_TIME_INFINITE;
        goto exit;
    }
                                                                /* Calculate us time delay's millisecond value, ..      */
                                                                /* .. rounded up to next millisecond.                   */
    time_us_to_ms  = ((time_us % DEF_TIME_NBR_uS_PER_SEC) + ((DEF_TIME_NBR_uS_PER_SEC / DEF_TIME_NBR_mS_PER_SEC) - 1u))
                                                          /  (DEF_TIME_NBR_uS_PER_SEC / DEF_TIME_NBR_mS_PER_SEC);
    time_sec_to_ms =  time_sec * DEF_TIME_NBR_mS_PER_SEC;


    time_us_to_ms_max  = DEF_INT_32U_MAX_VAL - time_sec_to_ms;
    time_sec_to_ms_max = DEF_INT_32U_MAX_VAL - time_us_to_ms;

    if ((time_us_to_ms  < time_us_to_ms_max) &&                 /* If NO        time delay integer overflow.            */
        (time_sec_to_ms < time_sec_to_ms_max)) {

         time_dly_ms = time_sec_to_ms + time_us_to_ms;

    } else {                                                    /* Else limit to maximum time delay values.             */
         time_dly_ms = NET_TMR_TIME_INFINITE;
    }


exit:
    return (time_dly_ms);
}


/*
*********************************************************************************************************
*                                        NetUtil_InitSeqNbrGet()
*
* Description : Initialize the TCP Transmit Initial Sequence Counter, 'NetTCP_TxSeqNbrCtr'.
*
*               (1) Possible initialization methods include :
*
*                   (a) Time-based initialization is one preferred method since it more appropriately
*                       provides a pseudo-random initial sequence number.
*                   (b) Hardware-generated random number initialization is NOT a preferred method since it
*                       tends to produce a discrete set of pseudo-random initial sequence numbers--often
*                       the same initial sequence number.
*                   (c) Hard-coded initial sequence number is NOT a preferred method since it is NOT random.
*
*                   See also 'net_tcp.h  NET_TCP_TX_GET_SEQ_NBR()  Note #1'.
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : NetTCP_Init().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetUtil_InitSeqNbrGet (void)
{
#ifndef  NET_UTIL_INIT_SEQ_NBR_0
    CPU_INT32U  val;
    NetUtil_RandSetSeed();
    val = Math_Rand();

    return (val);
#else
    return (0u);
#endif
}


/*
*********************************************************************************************************
*                                       NetUtil_RandomRangeGet()
*
* Description : Get a random value in a specific range
*
* Argument(s) : min     Minimum value
*
*               max     Maximum value
*
* Return(s)   : Random value in the specified range.
*
* Caller(s)   : NetSock_Init(),
*               NetSock_RandomPortNbrGet().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetUtil_RandomRangeGet (CPU_INT32U  min,
                                    CPU_INT32U  max)
{
    CPU_INT32U  val;
    CPU_INT32U  diff;
    CPU_INT32U  rand;



    NetUtil_RandSetSeed();

    diff = (max - min) + 1;
    rand =  Math_Rand();

    val  = rand % diff + min;

    return (val);
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
*                                         NetUtil_RandSetSeed()
*
* Description : Set the current pseudo-random number generator seed.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : NetUtil_InitSeqNbrGet(),
*               NetUtil_RandRange().
*
* Note(s)     : (1) If NET_TCP_CFG_RANDOM_ISN_GEN is defined in net_cfg.h, the user must define a function
*                   (NetBSP_GetEntropyVal()) in the network BSP file that returns a 32-bit unsigned
*                   integer random value. This value can be obtained from a hardware random number
*                   generator (RNG) or some other pseudorandom hardware source. A simple option in the
*                   absence of a hardware RNG would be to left-shift and OR the lowest bit of an ADC
*                   reading of a floating pin on the hardware 32 times.
*********************************************************************************************************
*/

static  void  NetUtil_RandSetSeed (void)
{
    CPU_INT32U  val;
    KAL_ERR     err_kal;


#ifndef NET_TCP_CFG_RANDOM_ISN_GEN                              /* If the 4 uS timer and entropy source for a more ...  */
    val = Math_Rand();                                          /* ...random seed are NOT implemented in the BSP,  ...  */

#if CPU_CFG_TS_32_EN == DEF_ENABLED                             /* ... increment random number by a 32-bit timestamp.   */
    val += (CPU_INT32U)CPU_TS_Get32();
#else
    val += (CPU_INT32U)KAL_TickGet(&err_kal);
#endif

#else
                                                                /* Otherwise, generate a more random seed w/ the user...*/
                                                                /* ... defined BSP function. (See Note #1).             */
    Math_RandSeedCur = NetBSP_GetEntropyVal();
    val = Math_Rand();
#endif

   (void)&err_kal;

   Math_RandSetSeed(val);
}


/*
*********************************************************************************************************
*                                      NetUtil_16BitSumHdrCalc()
*
* Description : Calculate 16-bit sum on packet header memory buffer.
*
*               (1) Calculates the sum of consecutive 16-bit values.
*
*               (2) 16-bit sum is returned as a 32-bit value to preserve possible 16-bit summation overflow
*                   in the upper 16-bits.
*
*
* Argument(s) : phdr        Pointer to packet header.
*               ----        Argument checked in NetUtil_16BitOnesCplChkSumHdrCalc(),
*                                               NetUtil_16BitOnesCplChkSumHdrVerify(),
*                                               NetUtil_16BitOnesCplSumDataCalc().
*
*               hdr_size    Size    of packet header.
*
* Return(s)   : 16-bit sum (see Note #2), if NO error(s).
*
*               0,                        otherwise.
*
* Caller(s)   : NetUtil_16BitOnesCplChkSumHdrCalc(),
*               NetUtil_16BitOnesCplChkSumHdrVerify(),
*               NetUtil_16BitOnesCplSumDataCalc().
*
* Note(s)     : (3) Since many word-aligned processors REQUIRE that multi-octet words be located on word-
*                   aligned addresses, 16-bit sum calculation MUST ensure that 16-bit words are accessed
*                   on addresses that are multiples of 2 octets.
*
*                   If packet header memory buffer does NOT start on a 16-bit word address boundary, then
*                   16-bit sum calculation MUST be performed by concatenating two consecutive 8-bit values.
*
*               (4) Modulo arithmetic is used to determine whether a memory buffer starts on the desired
*                   word-aligned address boundary.
*
*                   Modulo arithmetic in ANSI-C REQUIREs operations performed on integer values.  Thus
*                   address values MUST be cast to an appropriately-sized integer value PRIOR to any
*                   modulo arithmetic operation.
*
*               (5) (a) Although "the sum of 16-bit integers can be computed in either byte order"
*                       [RFC #1071, Section 2.(B)], the handling of odd-length &/or off-word-boundary
*                       memory buffers is performed assuming network-order.
*
*                   (b) However, to correctly & consistently calculate 16-bit integer sums for even-/
*                       odd-length & word-/off-word-boundary memory buffers, the sums MUST be calculated
*                       in network-order.
*
*                   (c) (1) To preserve possible 16-bit summation overflow (see Note #2) during all check-
*                           sum calculations, the 16-bit sum is returned as a 32-bit network-order value.
*
*                       (2) To encapsulate the network-order transform to the 16-bit check-sum calculations
*                           ONLY, the final check-sum MUST be converted to host-order.
*
*                           See 'NetUtil_16BitOnesCplChkSumHdrCalc()    Note #3b' &
*                               'NetUtil_16BitOnesCplChkSumHdrVerify()  Note #4b').
*
*                       (3) However, since network-to-host-order conversion & host-order memory access are
*                           inverse operations, the final host-order check-sum value MUST NOT be converted
*                           back to network-order for calculation or comparison.
*
*               (6) RFC #1071, Section 4.1 explicitly casts & sums the last odd-length octet in a check-sum
*                   calculation as a 16-bit value.
*
*                   However, this contradicts the following sections which state that "if the total
*                   length is odd, ... the last octet is padded on the right with ... one octet of zeros
*                   ... to form a 16 bit word for ... purposes ... [of] computing the checksum" :
*
*                   (a) RFC #768, Section     'Fields                     : Checksum'
*                   (b) RFC #792, Section     'Echo or Echo Reply Message : Checksum'
*                   (c) RFC #793, Section 3.1 'Header Format              : Checksum'
*
*                   See also 'NetUtil_16BitSumDataCalc()  Note #8'.
*********************************************************************************************************
*/

static  CPU_INT32U  NetUtil_16BitSumHdrCalc (void        *phdr,
                                             CPU_INT16U   hdr_size)
{
    CPU_INT32U   sum_32;
    CPU_INT32U   sum_val_32;
    CPU_INT16U   hdr_val_16;
    CPU_INT16U   size_rem;
    CPU_INT16U  *phdr_16;
    CPU_INT08U  *phdr_08;
    CPU_DATA     mod_16;

                                                                    /* ---------------- VALIDATE SIZE ----------------- */
    if (hdr_size < 1) {
        return (0u);
    }


    size_rem =  hdr_size;
    sum_32   =  0u;

    mod_16   = (CPU_INT08U)((CPU_ADDR)phdr % sizeof(CPU_INT16U));   /* See Note #4.                                     */
    if (mod_16 == 0u) {                                             /* If pkt hdr on 16-bit word boundary (see Note #3),*/
        phdr_16 = (CPU_INT16U *)phdr;
        while (size_rem >=  sizeof(CPU_INT16U)) {
            hdr_val_16   = (CPU_INT16U)*phdr_16++;
            sum_val_32   = (CPU_INT32U) NET_UTIL_HOST_TO_NET_16(hdr_val_16);    /* Conv to net-order (see Note #5b).    */
            sum_32      += (CPU_INT32U) sum_val_32;                 /* ... calc sum with 16-bit data words.             */
            size_rem    -= (CPU_INT16U) sizeof(CPU_INT16U);
        }
        phdr_08 = (CPU_INT08U *)phdr_16;

    } else {                                                        /* Else if pkt hdr NOT on 16-bit word boundary, ... */
        phdr_08 = (CPU_INT08U *)phdr;
        while (size_rem >=  sizeof(CPU_INT16U)) {
            sum_val_32   = (CPU_INT32U)*phdr_08++;
            sum_val_32 <<=  DEF_OCTET_NBR_BITS;
            sum_val_32  += (CPU_INT32U)*phdr_08++;
            sum_32      += (CPU_INT32U) sum_val_32;                 /* ... calc sum with  8-bit data vals.              */
            size_rem    -= (CPU_INT16U) sizeof(CPU_INT16U);
        }
    }

    if (size_rem > 0) {                                             /* Sum last octet, if any (see Note #6).            */
        sum_32 += ((CPU_INT32U)*phdr_08 << 8);
    }


    return (sum_32);                                                /* Rtn 16-bit sum (see Note #5c1).                  */
}


/*
*********************************************************************************************************
*                                     NetUtil_16BitSumDataCalc()
*
* Description : Calculate 16-bit sum on packet data memory buffer.
*
*               (1) Calculates the sum of consecutive 16-bit values.
*
*               (2) 16-bit sum is returned as a 32-bit value to preserve possible 16-bit summation overflow
*                   in the upper 16-bits.
*
*
* Argument(s) : p_data              Pointer to packet data.
*               ------              Argument validated in NetUtil_16BitOnesCplSumDataCalc().
*
*               data_size           Size    of packet data (in this network buffer only
*                                                           [see 'NetUtil_16BitOnesCplSumDataCalc()  Note #1a']).
*
*               poctet_prev         Pointer to last octet from a fragmented packet's previous buffer.
*               -----------         Argument validated in NetUtil_16BitOnesCplSumDataCalc().
*
*               poctet_last         Pointer to variable that will receive the value of the last octet from a
*               -----------             fragmented packet's current buffer.
*
*                                   Argument validated in NetUtil_16BitOnesCplSumDataCalc().
*
*               prev_octet_valid    Indicate whether pointer to the last octet of the packet's previous
*               ----------------        buffer is valid.
*
*                                   Argument validated in NetUtil_16BitOnesCplSumDataCalc().
*
*               last_pkt_buf        Indicate whether the current packet buffer is the last packet buffer.
*               ------------        Argument validated in NetUtil_16BitOnesCplSumDataCalc().
*
*               psum_err            Pointer to variable that will receive the error return code(s) from this function :
*
*                                       NET_UTIL_16_BIT_SUM_ERR_NONE            No error return codes.
*
*                                   The following error return codes are bit-field codes logically OR'd & MUST
*                                       be individually tested by bit-wise tests :
*
*                                       NET_UTIL_16_BIT_SUM_ERR_NULL_SIZE       Packet buffer's data size is
*                                                                                   a zero size.
*                                       NET_UTIL_16_BIT_SUM_ERR_LAST_OCTET      Last odd-length octet in packet
*                                                                                   buffer is available; check
*                                                                                   'poctet_last' return value.
*
* Return(s)   : 16-bit sum (see Note #2), if NO error(s).
*
*               0,                        otherwise.
*
* Caller(s)   : NetUtil_16BitOnesCplSumDataCalc().
*
* Note(s)     : (3) Since many word-aligned processors REQUIRE that multi-octet words be located on word-
*                   aligned addresses, 16-bit sum calculation MUST ensure that 16-bit words are accessed
*                   on addresses that are multiples of 2 octets.
*
*                   If packet data memory buffer does NOT start on a 16-bit word address boundary, then
*                   16-bit sum calculation MUST be performed by concatenating two consecutive 8-bit values.
*
*               (4) Modulo arithmetic is used to determine whether a memory buffer starts on the desired
*                   word-aligned address boundary.
*
*                   Modulo arithmetic in ANSI-C REQUIREs operations performed on integer values.  Thus
*                   address values MUST be cast to an appropriately-sized integer value PRIOR to any
*                   modulo arithmetic operation.
*
*               (5) (a) Although "the sum of 16-bit integers can be computed in either byte order"
*                       [RFC #1071, Section 2.(B)], the handling of odd-length &/or off-word-boundary
*                       memory buffers is performed assuming network-order.
*
*                   (b) However, to correctly & consistently calculate 16-bit integer sums for even-/
*                       odd-length & word-/off-word-boundary memory buffers, the sums MUST be calculated
*                       in network-order so that the last octet of any packet buffer is correctly pre-
*                       pended to the first octet of the next packet buffer.
*
*                   (c) (1) To preserve possible 16-bit summation overflow (see Note #2) during all check-
*                           sum calculations, the 16-bit sum is returned as a 32-bit network-order value.
*
*                       (2) To encapsulate the network-order transform to the 16-bit check-sum calculations
*                           ONLY, the final check-sum MUST be converted to host-order
*                           (see 'NetUtil_16BitOnesCplSumDataCalc()  Note #5').
*
*                       (3) However, since network-to-host-order conversion & host-order memory access are
*                           inverse operations, the final host-order check-sum value MUST NOT be converted
*                           back to network-order for calculation or comparison.
*
*               (6) Optimized 32-bit sum calculations implemented in the network protocol suite's network-
*                   specific library port optimization file(s).
*
*                   See also 'net_util.h  FUNCTION PROTOTYPES  DEFINED IN PRODUCT'S  net_util_a.*  Note #1'.
*
*               (7) Since pointer arithmetic is based on the specific pointer data type & inherent pointer
*                   data type size, pointer arithmetic operands :
*
*                   (a) MUST be in terms of the specific pointer data type & data type size; ...
*                   (b) SHOULD NOT & in some cases MUST NOT be cast to other data types or data type sizes.
*
*               (8) The following sections state that "if the total length is odd, ... the last octet
*                   is padded on the right with ... one octet of zeros ... to form a 16 bit word for
*                   ... purposes ... [of] computing the checksum" :
*
*                   (a) RFC #768, Section     'Fields                     : Checksum'
*                   (b) RFC #792, Section     'Echo or Echo Reply Message : Checksum'
*                   (c) RFC #793, Section 3.1 'Header Format              : Checksum'
*
*                   See also 'NetUtil_16BitSumHdrCalc()  Note #6'.
*********************************************************************************************************
*/

static  CPU_INT32U  NetUtil_16BitSumDataCalc (void         *p_data,
                                              CPU_INT16U    data_size,
                                              CPU_INT08U   *poctet_prev,
                                              CPU_INT08U   *poctet_last,
                                              CPU_BOOLEAN   prev_octet_valid,
                                              CPU_BOOLEAN   last_pkt_buf,
                                              CPU_INT08U   *psum_err)
{
    CPU_INT08U    mod_32;
#if (NET_CFG_OPTIMIZE_ASM_EN == DEF_ENABLED)
    CPU_INT16U    size_rem_32_offset;
    CPU_INT16U    size_rem_32;
#else
    CPU_INT32U   *pdata_32;
    CPU_INT32U    data_val_32;
#endif
    CPU_INT32U    sum_32;
    CPU_INT32U    sum_val_32;
    CPU_INT16U    data_val_16;
    CPU_INT16U    size_rem;
    CPU_INT16U   *pdata_16;
    CPU_INT08U   *pdata_08;
    CPU_DATA      mod_16;
    CPU_BOOLEAN   pkt_aligned_16;


    sum_32 = 0u;


    if (data_size < 1) {                                        /* ------------ HANDLE NULL-SIZE DATA PKT ------------- */
       *psum_err = NET_UTIL_16_BIT_SUM_ERR_NULL_SIZE;

        if (prev_octet_valid != DEF_NO) {                       /* If null size & last octet from prev pkt buf avail .. */

            if (last_pkt_buf != DEF_NO) {                       /* ...   & on last pkt buf,              ...            */
                sum_val_32   = (CPU_INT32U)*poctet_prev;        /* ...   cast prev pkt buf's last octet, ...            */
                sum_val_32 <<=  DEF_OCTET_NBR_BITS;             /* ... pad odd-len pkt len (see Note #5) ...            */
                sum_32       =  sum_val_32;                     /* ...  & rtn prev pkt buf's last octet as last sum.    */

            } else {                                            /* ... & NOT on last pkt buf, ...                       */
               *poctet_last = *poctet_prev;                     /* ... rtn last octet from prev pkt buf as last octet.  */
                DEF_BIT_SET(*psum_err, NET_UTIL_16_BIT_SUM_ERR_LAST_OCTET);

            }

        } else {
            ;                                                   /* If null size & NO prev octet, NO action(s) req'd.    */
        }

        return (sum_32);                                        /* Rtn 16-bit sum (see Note #5c1).                      */
    }


                                                                    /* ----------- HANDLE NON-NULL DATA PKT ----------- */
    size_rem       =  data_size;
   *psum_err       =  NET_UTIL_16_BIT_SUM_ERR_NONE;

                                                                    /* See Notes #3 & #4.                               */
    mod_16         = (CPU_INT08U)((CPU_ADDR)p_data % sizeof(CPU_INT16U));
    pkt_aligned_16 = (((mod_16 == 0u) && (prev_octet_valid == DEF_NO )) ||
                      ((mod_16 != 0u) && (prev_octet_valid == DEF_YES))) ? DEF_YES : DEF_NO;


    pdata_08 = (CPU_INT08U *)p_data;
    if (prev_octet_valid == DEF_YES) {                              /* If last octet from prev pkt buf avail,   ...     */
        sum_val_32   = (CPU_INT32U)*poctet_prev;
        sum_val_32 <<=  DEF_OCTET_NBR_BITS;                         /* ... prepend last octet from prev pkt buf ...     */

        sum_val_32  += (CPU_INT32U)*pdata_08++;
        sum_32      += (CPU_INT32U) sum_val_32;                     /* ... to first octet in cur pkt buf.               */

        size_rem    -= (CPU_INT16U) sizeof(CPU_INT08U);
    }

    if (pkt_aligned_16 == DEF_YES) {                                /* If pkt data aligned on 16-bit boundary, ..       */
                                                                    /* .. calc sum with 16- & 32-bit data words.        */
        pdata_16 = (CPU_INT16U *)pdata_08;
        mod_32   = (CPU_INT08U  )((CPU_ADDR)pdata_16 % sizeof(CPU_INT32U)); /* See Note #4.                             */
        if ((mod_32       !=  0u) &&                                /* If leading 16-bit pkt data avail, ..             */
            (size_rem     >=  sizeof(CPU_INT16U))) {
             data_val_16   = (CPU_INT16U)*pdata_16++;
             sum_val_32    = (CPU_INT32U) NET_UTIL_HOST_TO_NET_16(data_val_16); /* Conv to net-order (see Note #5b).    */
             sum_32       += (CPU_INT32U) sum_val_32;               /* .. start calc sum with leading 16-bit data word. */
             size_rem     -= (CPU_INT16U) sizeof(CPU_INT16U);
        }

#if (NET_CFG_OPTIMIZE_ASM_EN == DEF_ENABLED)
                                                                    /* Calc optimized 32-bit size rem.                  */
        size_rem_32_offset = (CPU_INT16U)(size_rem % sizeof(CPU_INT32U));
        size_rem_32        = (CPU_INT16U)(size_rem - size_rem_32_offset);
                                                                    /* Calc optimized 32-bit sum (see Note #6).         */
        sum_val_32         = (CPU_INT32U)NetUtil_16BitSumDataCalcAlign_32((void     *)pdata_16,
                                                                          (CPU_INT32U)size_rem_32);
        sum_32            += (CPU_INT32U)sum_val_32;
        size_rem          -= (CPU_INT32U)size_rem_32;

        pdata_08  = (CPU_INT08U *)pdata_16;
        pdata_08 +=               size_rem_32;                      /* MUST NOT cast ptr operand (see Note #7b).        */
        pdata_16  = (CPU_INT16U *)pdata_08;

#else
        pdata_32 = (CPU_INT32U *)pdata_16;
        while (size_rem >=  sizeof(CPU_INT32U)) {                   /* While pkt data aligned on 32-bit boundary; ...   */
            data_val_32  = (CPU_INT32U) *pdata_32++;                /* ... get sum data with 32-bit data words,   ...   */

            data_val_16  = (CPU_INT16U)((data_val_32 >> 16u) & 0x0000FFFFu);
            sum_val_32   = (CPU_INT32U)  NET_UTIL_HOST_TO_NET_16(data_val_16);  /* Conv to net-order (see Note #5b).    */
            sum_32      += (CPU_INT32U)  sum_val_32;                /* ... & calc sum with upper 16-bit data word ...   */

            data_val_16  = (CPU_INT16U) (data_val_32         & 0x0000FFFFu);
            sum_val_32   = (CPU_INT32U)  NET_UTIL_HOST_TO_NET_16(data_val_16);  /* Conv to net-order (see Note #5b).    */
            sum_32      += (CPU_INT32U)  sum_val_32;                /* ...               & lower 16-bit data word.      */

            size_rem    -= (CPU_INT16U)  sizeof(CPU_INT32U);
        }
        pdata_16 = (CPU_INT16U *)pdata_32;
#endif

        while (size_rem >=  sizeof(CPU_INT16U)) {                   /* While pkt data aligned on 16-bit boundary; ..    */
            data_val_16  = (CPU_INT16U)*pdata_16++;
            sum_val_32   = (CPU_INT32U) NET_UTIL_HOST_TO_NET_16(data_val_16);   /* Conv to net-order (see Note #5b).    */
            sum_32      += (CPU_INT32U) sum_val_32;                 /* .. calc sum with 16-bit data words.              */
            size_rem    -= (CPU_INT16U) sizeof(CPU_INT16U);
        }
        if (size_rem > 0) {
            sum_val_32   = (CPU_INT32U)(*((CPU_INT08U *)pdata_16));
        }

    } else {                                                        /* Else pkt data NOT aligned on 16-bit boundary, .. */
        while (size_rem >=  sizeof(CPU_INT16U)) {
            sum_val_32   = (CPU_INT32U)*pdata_08++;
            sum_val_32 <<=  DEF_OCTET_NBR_BITS;
            sum_val_32  += (CPU_INT32U)*pdata_08++;
            sum_32      += (CPU_INT32U) sum_val_32;                 /* .. calc sum with  8-bit data vals.               */
            size_rem    -= (CPU_INT16U) sizeof(CPU_INT16U);
        }
        if (size_rem > 0) {
            sum_val_32   = (CPU_INT32U)*pdata_08;
        }
    }


    if (size_rem > 0) {
        if (last_pkt_buf !=  DEF_NO) {                              /* If last pkt buf, ...                             */
            sum_val_32  <<=  DEF_OCTET_NBR_BITS;                    /* ... pad odd-len pkt len (see Note #8).           */
            sum_32       += (CPU_INT32U)sum_val_32;
        } else {
           *poctet_last   = (CPU_INT08U)sum_val_32;                 /* Else rtn last octet.                             */
            DEF_BIT_SET(*psum_err, NET_UTIL_16_BIT_SUM_ERR_LAST_OCTET);
        }
    }


    return (sum_32);                                                /* Rtn 16-bit sum (see Note #5c1).                  */
}


/*
*********************************************************************************************************
*                                  NetUtil_16BitOnesCplSumDataCalc()
*
* Description : Calculate 16-bit one's-complement sum on packet data.
*
*               (1) Calculates the 16-bit one's-complement sum of packet data encapsulated in :
*
*                   (a) One or more network buffers             Support non-fragmented & fragmented packets
*                   (b) Transport layer pseudo-header           See RFC #768, Section 'Fields : Checksum' &
*                                                                   RFC #793, Section 3.1 'Header Format :
*                                                                       Checksum'.
*
* Argument(s) : pdata_buf           Pointer to packet data network buffer(s) (see Notes #1a & #2).
*
*               ppseudo_hdr         Pointer to transport layer pseudo-header (see Note  #1b).
*
*               pseudo_hdr_size     Size    of transport layer pseudo-header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_UTIL_ERR_NONE               16-bit one's-complement sum calculated;
*                                                                   check return value.
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Packet data is a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*
* Return(s)   : 16-bit one's-complement sum, if NO error(s).
*
*               0,                           otherwise.
*
* Caller(s)   : NetUtil_16BitOnesCplChkSumDataCalc(),
*               NetUtil_16BitOnesCplChkSumDataVerify().
*
* Note(s)     : (2) Pointer to network buffer packet NOT validated as a network buffer.  However, no memory
*                   corruption occurs since no write operations are performed.
*
*               (3) (a) The following network buffer packet header fields MUST be configured BEFORE any
*                       packet data checksum is calculated :
*
*                       (1) Packet's currently configured protocol index
*                       (2) Packet's currently configured protocol header length
*                       (3) Packet's current data length
*
*                   (b) The network buffer packet's currently configured protocol header length & current
*                       data length do NOT need to be individually correct but MUST be synchronized such
*                       that their sum equals the current protocol's total length--i.e. the total number
*                       of octets in the packet for the current protocol.
*
*                       For example, a protocol layer receive may NOT yet have configured a packet's
*                       protocol header length which would still be set to zero (0).  However, it will
*                       NOT have offset the current protocol header length from the packet's current
*                       data length.  Therefore, the sum of the packet's current protocol header length
*                       & current data length will still equal the current protocol's total length.
*
*               (4) Default case already invalidated in earlier functions.  However, the default case is
*                   included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*
*               (5) (a) Since the 16-bit sum calculations are returned as 32-bit network-order values
*                       (see 'NetUtil_16BitSumDataCalc()  Note #5c1'), ...
*
*                   (b) ... the one's-complement sum MUST be converted to host-order but MUST NOT be re-
*                       converted back to network-order (see 'NetUtil_16BitSumDataCalc()  Note #5c3').
*********************************************************************************************************
*/

static  CPU_INT16U  NetUtil_16BitOnesCplSumDataCalc (void        *pdata_buf,
                                                     void        *ppseudo_hdr,
                                                     CPU_INT16U   pseudo_hdr_size,
                                                     NET_ERR     *p_err)
{
    NET_BUF      *pbuf;
    NET_BUF      *pbuf_next;
    NET_BUF_HDR  *pbuf_hdr;
    void         *p_data;
    CPU_INT32U    sum;
    CPU_INT32U    sum_val;
    CPU_INT16U    sum_ones_cpl;
    CPU_INT16U    sum_ones_cpl_host;
    CPU_INT16U    data_ix;
    CPU_INT16U    data_len;
    CPU_INT08U    sum_err;
    CPU_INT08U    octet_prev;
    CPU_INT08U    octet_last;
    CPU_BOOLEAN   octet_prev_valid;
    CPU_BOOLEAN   octet_last_valid;
    CPU_BOOLEAN   mem_buf_last;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN   mem_buf_first;
    CPU_BOOLEAN   mem_buf_null_size;
#endif


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ----------------- VALIDATE PTR ----------------- */
    if (pdata_buf == (void *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
#endif


                                                                    /* ------ CALC PKT DATA 16-BIT ONE'S-CPL SUM ------ */
    pbuf             = (NET_BUF *)pdata_buf;
    sum              =  0u;
    octet_prev       =  0u;
    octet_last       =  0u;
    octet_prev_valid =  DEF_NO;
    mem_buf_last     =  DEF_NO;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    mem_buf_first    =  DEF_YES;
#endif

    if (ppseudo_hdr != (void *)0) {                                 /* Calc pkt's pseudo-hdr 16-bit sum (see Note #1b). */
        sum_val  = NetUtil_16BitSumDataCalc((void       *) ppseudo_hdr,
                                            (CPU_INT16U  ) pseudo_hdr_size,
                                            (CPU_INT08U *)&octet_prev,
                                            (CPU_INT08U *)&octet_last,
                                            (CPU_BOOLEAN ) octet_prev_valid,
                                            (CPU_BOOLEAN ) mem_buf_last,
                                            (CPU_INT08U *)&sum_err);
        sum     += sum_val;

        octet_last_valid = DEF_BIT_IS_SET(sum_err, NET_UTIL_16_BIT_SUM_ERR_LAST_OCTET);
        if (octet_last_valid == DEF_YES) {                          /* If last octet from pseudo-hdr avail, ...         */
            octet_prev        = octet_last;                         /* ... prepend last octet to first pkt buf.         */
            octet_prev_valid  = DEF_YES;
        } else {
            octet_prev        = 0u;
            octet_prev_valid  = DEF_NO;
        }
    }

    while (pbuf  != (NET_BUF *)0) {                                 /* Calc ALL data pkts' 16-bit sum  (see Note #1a).  */
        pbuf_hdr  = &pbuf->Hdr;
        switch (pbuf_hdr->ProtocolHdrType) {                        /* Demux pkt buf's protocol ix/len (see Note #3b).  */
            case NET_PROTOCOL_TYPE_ICMP_V4:
            case NET_PROTOCOL_TYPE_ICMP_V6:
                 data_ix  = pbuf_hdr->ICMP_MsgIx;
                 data_len = pbuf_hdr->ICMP_HdrLen    + (CPU_INT16U)pbuf_hdr->DataLen;
                 break;


            case NET_PROTOCOL_TYPE_UDP_V4:
            case NET_PROTOCOL_TYPE_UDP_V6:
#ifdef  NET_TCP_MODULE_EN
            case NET_PROTOCOL_TYPE_TCP_V4:
            case NET_PROTOCOL_TYPE_TCP_V6:
#endif
                 data_ix  = pbuf_hdr->TransportHdrIx;
                 data_len = pbuf_hdr->TransportHdrLen + (CPU_INT16U)pbuf_hdr->DataLen;
                 break;

            case NET_PROTOCOL_TYPE_IP_V6_EXT_NONE:
                data_ix  = pbuf_hdr->TotLen - pbuf_hdr->DataLen;
                data_len = pbuf_hdr->DataLen;
                break;


            default:                                                /* See Note #4.                                     */
                *p_err =  NET_UTIL_ERR_INVALID_PROTOCOL;
                 return (0u);
        }

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (data_ix == NET_BUF_IX_NONE) {
           *p_err =  NET_BUF_ERR_INVALID_IX;
            return (0u);
        }
#endif

        p_data       = (void    *)&pbuf->DataPtr[data_ix];
        pbuf_next    = (NET_BUF *) pbuf_hdr->NextBufPtr;
        mem_buf_last = (pbuf_next == (NET_BUF *)0) ? DEF_YES : DEF_NO;
                                                                    /* Calc pkt buf's 16-bit sum.                       */
        sum_val      =  NetUtil_16BitSumDataCalc((void       *) p_data,
                                                 (CPU_INT16U  ) data_len,
                                                 (CPU_INT08U *)&octet_prev,
                                                 (CPU_INT08U *)&octet_last,
                                                 (CPU_BOOLEAN ) octet_prev_valid,
                                                 (CPU_BOOLEAN ) mem_buf_last,
                                                 (CPU_INT08U *)&sum_err);

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (mem_buf_first == DEF_YES) {
            mem_buf_first  = DEF_NO;
            if (mem_buf_last == DEF_YES) {
                mem_buf_null_size = DEF_BIT_IS_SET(sum_err, NET_UTIL_16_BIT_SUM_ERR_NULL_SIZE);
                if (mem_buf_null_size != DEF_NO) {                  /* If ONLY mem buf & null size, rtn err.            */
                   *p_err =  NET_UTIL_ERR_NULL_SIZE;
                    return (0u);
                }
            }
        }
#endif

        if (mem_buf_last != DEF_YES) {                              /* If NOT on last pkt buf &                     ... */
            octet_last_valid = DEF_BIT_IS_SET(sum_err, NET_UTIL_16_BIT_SUM_ERR_LAST_OCTET);
            if (octet_last_valid == DEF_YES) {                      /* ...         last octet from cur  pkt buf avail,  */
                octet_prev        = octet_last;                     /* ... prepend last octet to   next pkt buf.        */
                octet_prev_valid  = DEF_YES;
            } else {
                octet_prev        = 0u;
                octet_prev_valid  = DEF_NO;
            }
        }

        sum  += sum_val;
        pbuf  = pbuf_next;
    }


    while (sum >> 16u) {                                            /* While 16-bit sum ovf's, ...                      */
        sum = (sum & 0x0000FFFFu) + (sum >> 16u);                   /* ... sum ovf bits back into 16-bit one's-cpl sum. */
    }

    sum_ones_cpl      = (CPU_INT16U)sum;
    sum_ones_cpl_host =  NET_UTIL_NET_TO_HOST_16(sum_ones_cpl);     /* Conv back to host-order  (see Note #5b).         */
   *p_err             =  NET_UTIL_ERR_NONE;


    return (sum_ones_cpl_host);                                     /* Rtn 16-bit one's-cpl sum (see Note #1).          */
}
