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
* Filename : net_util.h
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
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_UTIL_MODULE_PRESENT
#define  NET_UTIL_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  <lib_mem.h>
#include  <lib_math.h>
#include  "net_tmr.h"
#include  "net_type.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                              MACRO'S
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                           NETWORK WORD ORDER - TO - CPU WORD ORDER MACRO'S
*
* Description : Convert data values to & from network word order to host CPU word order.
*
* Argument(s) : val       Data value to convert (see Note #1).
*
* Return(s)   : Converted data value (see Note #1).
*
* Caller(s)   : various.
*
*               These macro's are network protocol suite application programming interface (API) macro's
*               & MAY be called by application function(s).
*
* Note(s)     : (1) 'val' data value to convert & any variable to receive the returned conversion MUST
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
*                   See also 'lib_mem.h  MEMORY DATA VALUE MACRO'S  Note #1a'
*                          & 'lib_mem.h  ENDIAN WORD ORDER MACRO'S  Note #2'.
*********************************************************************************************************
*/

#define  NET_UTIL_NET_TO_HOST_32(val)                   MEM_VAL_BIG_TO_HOST_32(val)
#define  NET_UTIL_NET_TO_HOST_16(val)                   MEM_VAL_BIG_TO_HOST_16(val)

#define  NET_UTIL_HOST_TO_NET_32(val)                   MEM_VAL_HOST_TO_BIG_32(val)
#define  NET_UTIL_HOST_TO_NET_16(val)                   MEM_VAL_HOST_TO_BIG_16(val)

#define  NET_UTIL_VAL_SWAP_ORDER_16(val)                MEM_VAL_BIG_TO_LITTLE_16(val)
#define  NET_UTIL_VAL_SWAP_ORDER_32(val)                MEM_VAL_BIG_TO_LITTLE_32(val)


/*
*********************************************************************************************************
*                                     NETWORK DATA VALUE MACRO'S
*
* Description : Encode/decode data values to & from any CPU memory addresses.
*
* Argument(s) : various.
*
* Return(s)   : various.
*
* Caller(s)   : various.
*
*               These macro's are INTERNAL network protocol suite macro's & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Network data value macro's appropriately convert data words :
*
*                   (a) (1) From network  word order to host CPU word order
*                       (2) From host CPU word order to network  word order
*
*                   (b) NO network-to-host word-order conversion performed
*
*               (2) (a) Some network data values MUST start on appropriate CPU word-aligned addresses :
*
*                       (1) Data values
*                       (2) Variables to receive data values
*
*                   (b) Some network data values may start on any CPU address, word-aligned or not :
*
*                       (1) Addresses to         data values
*                       (2) Addresses to receive data values
*
*                   See also 'lib_mem.h  MEMORY DATA VALUE MACRO'S  Note #1'.
*********************************************************************************************************
*/
                                                                                        /* See Notes #1a1, #2a2, & #2b1.*/
#define  NET_UTIL_VAL_GET_NET_16(addr)                          MEM_VAL_GET_INT16U_BIG(addr)
#define  NET_UTIL_VAL_GET_NET_32(addr)                          MEM_VAL_GET_INT32U_BIG(addr)
                                                                                        /* See Notes #1a2, #2a1, & #2b2.*/
#define  NET_UTIL_VAL_SET_NET_16(addr, val)                     MEM_VAL_SET_INT16U_BIG((addr), (val))
#define  NET_UTIL_VAL_SET_NET_32(addr, val)                     MEM_VAL_SET_INT32U_BIG((addr), (val))

                                                                                        /* See Notes #1a1 & #2b.        */
#define  NET_UTIL_VAL_COPY_GET_NET_16(addr_dest, addr_src)      MEM_VAL_COPY_GET_INT16U_BIG((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_GET_NET_32(addr_dest, addr_src)      MEM_VAL_COPY_GET_INT32U_BIG((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_GET_NET(addr_dest, addr_src, size)   MEM_VAL_COPY_GET_INTU_BIG((addr_dest), (addr_src), (size))
                                                                                        /* See Notes #1a2 & #2b.        */
#define  NET_UTIL_VAL_COPY_SET_NET_16(addr_dest, addr_src)      MEM_VAL_COPY_SET_INT16U_BIG((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_SET_NET_32(addr_dest, addr_src)      MEM_VAL_COPY_SET_INT32U_BIG((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_SET_NET(addr_dest, addr_src, size)   MEM_VAL_COPY_SET_INTU_BIG((addr_dest), (addr_src), (size))


                                                                                        /* See Notes #1b, #2a2, & #2b1. */
#define  NET_UTIL_VAL_GET_HOST_16(addr)                         MEM_VAL_GET_INT16U(addr)
#define  NET_UTIL_VAL_GET_HOST_32(addr)                         MEM_VAL_GET_INT32U(addr)
                                                                                        /* See Notes #1b, #2a1, & #2b2. */
#define  NET_UTIL_VAL_SET_HOST_16(addr, val)                    MEM_VAL_SET_INT16U((addr), (val))
#define  NET_UTIL_VAL_SET_HOST_32(addr, val)                    MEM_VAL_SET_INT32U((addr), (val))

                                                                                        /* See Notes #1b & #2b.         */
#define  NET_UTIL_VAL_COPY_GET_HOST_16(addr_dest, addr_src)     MEM_VAL_COPY_GET_INT16U((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_GET_HOST_32(addr_dest, addr_src)     MEM_VAL_COPY_GET_INT32U((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_GET_HOST(addr_dest, addr_src, size)  MEM_VAL_COPY_GET_INTU((addr_dest), (addr_src), (size))
                                                                                        /* See Notes #1b & #2b.         */
#define  NET_UTIL_VAL_COPY_SET_HOST_16(addr_dest, addr_src)     MEM_VAL_COPY_SET_INT16U((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_SET_HOST_32(addr_dest, addr_src)     MEM_VAL_COPY_SET_INT32U((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_SET_HOST(addr_dest, addr_src, size)  MEM_VAL_COPY_SET_INTU((addr_dest), (addr_src), (size))


                                                                                        /* See Notes #1b & #2b.         */
#define  NET_UTIL_VAL_COPY_16(addr_dest, addr_src)              MEM_VAL_COPY_16((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY_32(addr_dest, addr_src)              MEM_VAL_COPY_32((addr_dest), (addr_src))
#define  NET_UTIL_VAL_COPY(addr_dest, addr_src, size)           MEM_VAL_COPY((addr_dest), (addr_src), (size))


#define  NET_UTIL_VAL_SWAP_ORDER(addr)                          MEM_VAL_BIG_TO_LITTLE_32(addr)


/*
*********************************************************************************************************
*                                    NETWORK IPv6 ADDRESS MACRO'S
*
* Description : Set and validate different type of IPv6 addresses.
*
* Argument(s) : various.
*
* Return(s)   : various.
*
* Caller(s)   : various.
*
*               These macro's are INTERNAL network protocol suite macro's & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

#define  NET_UTIL_IPv6_ADDR_COPY(addr_src, addr_dest)           (addr_dest).Addr[0]  = (addr_src).Addr[0];  \
                                                                (addr_dest).Addr[1]  = (addr_src).Addr[1];  \
                                                                (addr_dest).Addr[2]  = (addr_src).Addr[2];  \
                                                                (addr_dest).Addr[3]  = (addr_src).Addr[3];  \
                                                                (addr_dest).Addr[4]  = (addr_src).Addr[4];  \
                                                                (addr_dest).Addr[5]  = (addr_src).Addr[5];  \
                                                                (addr_dest).Addr[6]  = (addr_src).Addr[6];  \
                                                                (addr_dest).Addr[7]  = (addr_src).Addr[7];  \
                                                                (addr_dest).Addr[8]  = (addr_src).Addr[8];  \
                                                                (addr_dest).Addr[9]  = (addr_src).Addr[9];  \
                                                                (addr_dest).Addr[10] = (addr_src).Addr[10]; \
                                                                (addr_dest).Addr[11] = (addr_src).Addr[11]; \
                                                                (addr_dest).Addr[12] = (addr_src).Addr[12]; \
                                                                (addr_dest).Addr[13] = (addr_src).Addr[13]; \
                                                                (addr_dest).Addr[14] = (addr_src).Addr[14]; \
                                                                (addr_dest).Addr[15] = (addr_src).Addr[15];

#define  NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr)               (addr).Addr[0]   = DEF_BIT_NONE; \
                                                                (addr).Addr[1]   = DEF_BIT_NONE; \
                                                                (addr).Addr[2]   = DEF_BIT_NONE; \
                                                                (addr).Addr[3]   = DEF_BIT_NONE; \
                                                                (addr).Addr[4]   = DEF_BIT_NONE; \
                                                                (addr).Addr[5]   = DEF_BIT_NONE; \
                                                                (addr).Addr[6]   = DEF_BIT_NONE; \
                                                                (addr).Addr[7]   = DEF_BIT_NONE; \
                                                                (addr).Addr[8]   = DEF_BIT_NONE; \
                                                                (addr).Addr[9]   = DEF_BIT_NONE; \
                                                                (addr).Addr[10]  = DEF_BIT_NONE; \
                                                                (addr).Addr[11]  = DEF_BIT_NONE; \
                                                                (addr).Addr[12]  = DEF_BIT_NONE; \
                                                                (addr).Addr[13]  = DEF_BIT_NONE; \
                                                                (addr).Addr[14]  = DEF_BIT_NONE; \
                                                                (addr).Addr[15]  = DEF_BIT_NONE;

#define  NET_UTIL_IPv6_ADDR_IS_UNSPECIFIED(addr)              (((addr).Addr[0]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[1]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[2]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[3]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[4]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[5]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[6]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[7]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[8]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[9]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[10] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[11] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[12] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[13] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[14] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[15] == DEF_BIT_NONE))

#define  NET_UTIL_IPv6_ADDR_SET_LOOPBACK(addr)                  (addr).Addr[0]   = DEF_BIT_NONE; \
                                                                (addr).Addr[1]   = DEF_BIT_NONE; \
                                                                (addr).Addr[2]   = DEF_BIT_NONE; \
                                                                (addr).Addr[3]   = DEF_BIT_NONE; \
                                                                (addr).Addr[4]   = DEF_BIT_NONE; \
                                                                (addr).Addr[5]   = DEF_BIT_NONE; \
                                                                (addr).Addr[6]   = DEF_BIT_NONE; \
                                                                (addr).Addr[7]   = DEF_BIT_NONE; \
                                                                (addr).Addr[8]   = DEF_BIT_NONE; \
                                                                (addr).Addr[9]   = DEF_BIT_NONE; \
                                                                (addr).Addr[10]  = DEF_BIT_NONE; \
                                                                (addr).Addr[11]  = DEF_BIT_NONE; \
                                                                (addr).Addr[12]  = DEF_BIT_NONE; \
                                                                (addr).Addr[13]  = DEF_BIT_NONE; \
                                                                (addr).Addr[14]  = DEF_BIT_NONE; \
                                                                (addr).Addr[15]  = DEF_BIT_00;

#define  NET_UTIL_IPv6_ADDR_IS_LOOPBACK(addr)                 (((addr).Addr[0]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[1]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[2]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[3]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[4]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[5]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[6]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[7]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[8]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[9]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[10] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[11] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[12] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[13] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[14] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[15] == DEF_BIT_00))

#define  NET_UTIL_IPv6_ADDR_SET_MCAST_ALL_NODES(addr)           (addr).Addr[0]   = 0xFF;         \
                                                                (addr).Addr[1]   = 0x02;         \
                                                                (addr).Addr[2]   = DEF_BIT_NONE; \
                                                                (addr).Addr[3]   = DEF_BIT_NONE; \
                                                                (addr).Addr[4]   = DEF_BIT_NONE; \
                                                                (addr).Addr[5]   = DEF_BIT_NONE; \
                                                                (addr).Addr[6]   = DEF_BIT_NONE; \
                                                                (addr).Addr[7]   = DEF_BIT_NONE; \
                                                                (addr).Addr[8]   = DEF_BIT_NONE; \
                                                                (addr).Addr[9]   = DEF_BIT_NONE; \
                                                                (addr).Addr[10]  = DEF_BIT_NONE; \
                                                                (addr).Addr[11]  = DEF_BIT_NONE; \
                                                                (addr).Addr[12]  = DEF_BIT_NONE; \
                                                                (addr).Addr[13]  = DEF_BIT_NONE; \
                                                                (addr).Addr[14]  = DEF_BIT_NONE; \
                                                                (addr).Addr[15]  = 0x01;

#define  NET_UTIL_IPv6_ADDR_IS_MCAST_ALL_NODES(addr)          (((addr).Addr[0]  == 0xFF)         && \
                                                               ((addr).Addr[1]  == 0x02)         && \
                                                               ((addr).Addr[2]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[3]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[4]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[5]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[6]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[7]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[8]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[9]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[10] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[11] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[12] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[13] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[14] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[15] == 0x01))

#define  NET_UTIL_IPv6_ADDR_SET_MCAST_ALL_ROUTERS(addr)         (addr).Addr[0]   = 0xFF;         \
                                                                (addr).Addr[1]   = 0x02;         \
                                                                (addr).Addr[2]   = DEF_BIT_NONE; \
                                                                (addr).Addr[3]   = DEF_BIT_NONE; \
                                                                (addr).Addr[4]   = DEF_BIT_NONE; \
                                                                (addr).Addr[5]   = DEF_BIT_NONE; \
                                                                (addr).Addr[6]   = DEF_BIT_NONE; \
                                                                (addr).Addr[7]   = DEF_BIT_NONE; \
                                                                (addr).Addr[8]   = DEF_BIT_NONE; \
                                                                (addr).Addr[9]   = DEF_BIT_NONE; \
                                                                (addr).Addr[10]  = DEF_BIT_NONE; \
                                                                (addr).Addr[11]  = DEF_BIT_NONE; \
                                                                (addr).Addr[12]  = DEF_BIT_NONE; \
                                                                (addr).Addr[13]  = DEF_BIT_NONE; \
                                                                (addr).Addr[14]  = DEF_BIT_NONE; \
                                                                (addr).Addr[15]  = 0x02;

#define  NET_UTIL_IPv6_ADDR_IS_MCAST_ALL_ROUTERS(addr)        (((addr).Addr[0]  == 0xFF)         && \
                                                               ((addr).Addr[1]  == 0x02)         && \
                                                               ((addr).Addr[2]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[3]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[4]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[5]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[6]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[7]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[8]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[9]  == DEF_BIT_NONE) && \
                                                               ((addr).Addr[10] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[11] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[12] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[13] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[14] == DEF_BIT_NONE) && \
                                                               ((addr).Addr[15] == 0x02))

#define  NET_UTIL_IPv6_ADDR_IS_LINK_LOCAL(addr)               (((addr).Addr[0]  == 0xFE) && \
                                                               ((addr).Addr[1]  == 0x80))

#define  NET_UTIL_IPv6_ADDR_IS_SITE_LOCAL(addr)               (((addr).Addr[0]  == 0xFE) && \
                                                               ((addr).Addr[1]  == 0xC0))

#define  NET_UTIL_IPv6_ADDR_IS_MULTICAST(addr)                 ((addr).Addr[0]  == 0xFF)




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
*********************************************************************************************************
*                                           GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_TCP_CFG_RANDOM_ISN_GEN
extern  RAND_NBR  Math_RandSeedCur;
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
*                                         EXTERNAL FUNCTIONS
*********************************************************************************************************
*/

#ifdef  NET_TCP_CFG_RANDOM_ISN_GEN
extern  CPU_INT32U  NetBSP_GetEntropyVal (void);
#endif


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/
                                                                /* ------------------ CHK SUM FNCTS ------------------- */
NET_CHK_SUM  NetUtil_16BitOnesCplChkSumHdrCalc   (void        *phdr,
                                                  CPU_INT16U   hdr_size,
                                                  NET_ERR     *p_err);

CPU_BOOLEAN  NetUtil_16BitOnesCplChkSumHdrVerify (void        *phdr,
                                                  CPU_INT16U   hdr_size,
                                                  NET_ERR     *p_err);

NET_CHK_SUM  NetUtil_16BitOnesCplChkSumDataCalc  (void        *pdata_buf,
                                                  void        *ppseudo_hdr,
                                                  CPU_INT16U   pseudo_hdr_size,
                                                  NET_ERR     *p_err);

CPU_BOOLEAN  NetUtil_16BitOnesCplChkSumDataVerify(void        *pdata_buf,
                                                  void        *ppseudo_hdr,
                                                  CPU_INT16U   pseudo_hdr_size,
                                                  NET_ERR     *p_err);


                                                                /* -------------------- CRC FNCTS --------------------- */
CPU_INT32U   NetUtil_32BitCRC_Calc               (CPU_INT08U  *p_data,
                                                  CPU_INT32U   data_len,
                                                  NET_ERR     *p_err);

CPU_INT32U   NetUtil_32BitCRC_CalcCpl            (CPU_INT08U  *p_data,
                                                  CPU_INT32U   data_len,
                                                  NET_ERR     *p_err);

CPU_INT32U   NetUtil_32BitReflect                (CPU_INT32U   val);

                                                                /* -------------------- TIME FNCTS -------------------- */
CPU_INT32U   NetUtil_TimeSec_uS_To_ms            (CPU_INT32U   time_sec,
                                                  CPU_INT32U   time_us);

                                                                /* --------------------- TS FNCTS --------------------- */
NET_TS      NetUtil_TS_Get                       (void);

NET_TS_MS   NetUtil_TS_Get_ms                    (void);



CPU_INT32U  NetUtil_InitSeqNbrGet                (void);

CPU_INT32U  NetUtil_RandomRangeGet               (CPU_INT32U   min,
                                                  CPU_INT32U   max);


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*                                 DEFINED IN PRODUCT'S  net_util_a.*
*
* Note(s) : (1) The network protocol suite's network-specific library port optimization file(s) are located
*               in the following directories :
*
*               (a) \<Network Protocol Suite>\Ports\<cpu>\<compiler>\net_util_a.*
*
*                       where
*                               <Network Protocol Suite>        directory path for network protocol suite
*                               <cpu>                           directory name for specific processor (CPU)
*                               <compiler>                      directory name for specific compiler
*********************************************************************************************************
*/

#if (NET_CFG_OPTIMIZE_ASM_EN == DEF_ENABLED)
                                                                        /* Optimize 16-bit sum for 32-bit.              */
CPU_INT32U  NetUtil_16BitSumDataCalcAlign_32(void        *pdata_32,
                                             CPU_INT32U   size);
#endif


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*                                   DEFINED IN PRODUCT'S  net_bsp.c
*********************************************************************************************************
*/


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
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of net util module include.                      */
