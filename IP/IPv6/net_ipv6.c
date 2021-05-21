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
*                                     NETWORK IP LAYER VERSION 6
*                                       (INTERNET PROTOCOL V6)
*
* Filename : net_ipv6.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Internet Protocol as described in RFC #2460, also known as IPv6, with the
*                following restrictions/constraints :
*
*                (a) IPv6 forwarding/routing NOT currently supported       RFC #2460
*
*                (b) Transmit fragmentation  NOT currently supported       RFC #2460, Section 4.5
*                                                                         'Fragment Header'
*
*                (c) IPv6 Security options   NOT           supported       RFC #4301
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    NET_IPv6_MODULE
#include  "net_ipv6.h"
#include  "net_ndp.h"
#include  "net_icmpv6.h"
#include  "net_mldp.h"
#include  "net_dad.h"
#include  "../../Source/net_buf.h"
#include  "../../Source/net_conn.h"
#include  "../../Source/net_util.h"
#include  "../../Source/net_tcp.h"
#include  "../../Source/net_udp.h"
#include  "../../Source/net.h"
#include  "../../IF/net_if.h"
#include  <KAL/kal.h>
#include  <lib_math.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) See 'net_ipv6.h  MODULE'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IPv6_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_IPv6_ADDR_LOCAL_HOST_ADDR                   NET_IPv6_ADDR_ANY_INIT


/*
*********************************************************************************************************
                                        IPv6 AUTO CFG DEFINES
*********************************************************************************************************
*/

#define  NET_IPv6_AUTO_CFG_RAND_RETRY_MAX                3


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     IPv6 POLICY TABLE DATA TYPE
*
* Note(s) : (1) The policy table is a longest-matching-prefix lookup table used for source and
*               destination address selection algorithm.
*
*           (2) See the RFC #6724 'Default Address Selection for Internet Protocol Version 6' for more
*               details.
*********************************************************************************************************
*/

static const NET_IPv6_ADDR  NET_IPv6_POLICY_MASK_128 = { { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF } };
static const NET_IPv6_ADDR  NET_IPv6_POLICY_MASK_096 = { { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0,   0,   0,   0    } };
static const NET_IPv6_ADDR  NET_IPv6_POLICY_MASK_016 = { { 0xFF,0xFF,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    } };
static const NET_IPv6_ADDR  NET_IPv6_POLICY_MASK_032 = { { 0xFF,0xFF,0xFF,0xFF,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    } };
static const NET_IPv6_ADDR  NET_IPv6_POLICY_MASK_007 = { { 0XFE,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    } };
static const NET_IPv6_ADDR  NET_IPv6_POLICY_MASK_010 = { { 0XFF,0XC0,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    } };


typedef  struct  net_ipv6_policy {
    const  NET_IPv6_ADDR  *PrefixAddrPtr;
    const  NET_IPv6_ADDR  *PrefixMaskPtr;
    const  CPU_INT08U      Precedence;
    const  CPU_INT08U      Label;
} NET_IPv6_POLICY;


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 01
---------------------------------------------------------------------------------------------------------
 */

#define  NET_IPv6_POLICY_01_PREFIX_ADDR_PTR       &NET_IPv6_ADDR_LOOPBACK_INIT
#define  NET_IPv6_POLICY_01_MASK_PTR              &NET_IPv6_POLICY_MASK_128
#define  NET_IPv6_POLICY_01_PRECEDENCE             50
#define  NET_IPv6_POLICY_01_LABEL                   0
static  const  NET_IPv6_POLICY NetIPv6_Policy_01       = {
                                                            NET_IPv6_POLICY_01_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_01_MASK_PTR,
                                                            NET_IPv6_POLICY_01_PRECEDENCE,
                                                            NET_IPv6_POLICY_01_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 02
---------------------------------------------------------------------------------------------------------
 */

#define  NET_IPv6_POLICY_02_PREFIX_ADDR_PTR       &NET_IPv6_ADDR_ANY_INIT
#define  NET_IPv6_POLICY_02_MASK_PTR              &NET_IPv6_ADDR_ANY_INIT
#define  NET_IPv6_POLICY_02_PRECEDENCE             40
#define  NET_IPv6_POLICY_02_LABEL                   1
static  const  NET_IPv6_POLICY NetIPv6_Policy_02       = {
                                                            NET_IPv6_POLICY_02_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_02_MASK_PTR,
                                                            NET_IPv6_POLICY_02_PRECEDENCE,
                                                            NET_IPv6_POLICY_02_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 03
---------------------------------------------------------------------------------------------------------
 */

static  const  NET_IPv6_ADDR   NET_IPv6_POLICY_03_ADDR = { { 0,   0,   0,0,0,0,0,0,0,0,0,0,0xFF,0xFF,0,0 } };

#define  NET_IPv6_POLICY_03_PREFIX_ADDR_PTR       &NET_IPv6_POLICY_03_ADDR
#define  NET_IPv6_POLICY_03_MASK_PTR              &NET_IPv6_POLICY_MASK_096
#define  NET_IPv6_POLICY_03_PRECEDENCE             35
#define  NET_IPv6_POLICY_03_LABEL                   4
static  const  NET_IPv6_POLICY NetIPv6_Policy_03       = {
                                                            NET_IPv6_POLICY_03_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_03_MASK_PTR,
                                                            NET_IPv6_POLICY_03_PRECEDENCE,
                                                            NET_IPv6_POLICY_03_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 04
---------------------------------------------------------------------------------------------------------
 */


static  const  NET_IPv6_ADDR   NET_IPv6_POLICY_04_ADDR = { { 0x20,0x02,0,0,0,0,0,0,0,0,0,0,0,   0,   0,0 } };
#define  NET_IPv6_POLICY_04_PREFIX_ADDR_PTR       &NET_IPv6_POLICY_04_ADDR
#define  NET_IPv6_POLICY_04_MASK_PTR              &NET_IPv6_POLICY_MASK_016
#define  NET_IPv6_POLICY_04_PRECEDENCE             30
#define  NET_IPv6_POLICY_04_LABEL                   2

static  const  NET_IPv6_POLICY NetIPv6_Policy_04       = {
                                                            NET_IPv6_POLICY_04_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_04_MASK_PTR,
                                                            NET_IPv6_POLICY_04_PRECEDENCE,
                                                            NET_IPv6_POLICY_04_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 05
---------------------------------------------------------------------------------------------------------
 */

static  const  NET_IPv6_ADDR   NET_IPv6_POLICY_05_ADDR = { { 0x20,0x01,0,0,0,0,0,0,0,0,0,0,0,   0,   0,0 } };
#define  NET_IPv6_POLICY_05_PREFIX_ADDR_PTR       &NET_IPv6_POLICY_05_ADDR
#define  NET_IPv6_POLICY_05_MASK_PTR              &NET_IPv6_POLICY_MASK_032
#define  NET_IPv6_POLICY_05_PRECEDENCE              5
#define  NET_IPv6_POLICY_05_LABEL                   5

static  const  NET_IPv6_POLICY NetIPv6_Policy_05       = {
                                                            NET_IPv6_POLICY_05_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_05_MASK_PTR,
                                                            NET_IPv6_POLICY_05_PRECEDENCE,
                                                            NET_IPv6_POLICY_05_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 06
---------------------------------------------------------------------------------------------------------
 */

static  const  NET_IPv6_ADDR   NET_IPv6_POLICY_06_ADDR = { { 0xFC,0,   0,0,0,0,0,0,0,0,0,0,0,   0,   0,0 } };
#define  NET_IPv6_POLICY_06_PREFIX_ADDR_PTR       &NET_IPv6_POLICY_06_ADDR
#define  NET_IPv6_POLICY_06_MASK_PTR              &NET_IPv6_POLICY_MASK_007
#define  NET_IPv6_POLICY_06_PRECEDENCE              3
#define  NET_IPv6_POLICY_06_LABEL                  13

static  const  NET_IPv6_POLICY NetIPv6_Policy_06       = {
                                                            NET_IPv6_POLICY_06_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_06_MASK_PTR,
                                                            NET_IPv6_POLICY_06_PRECEDENCE,
                                                            NET_IPv6_POLICY_06_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 07
---------------------------------------------------------------------------------------------------------
 */

#define  NET_IPv6_POLICY_07_PREFIX_ADDR_PTR       &NET_IPv6_ADDR_ANY_INIT
#define  NET_IPv6_POLICY_07_MASK_PTR              &NET_IPv6_POLICY_MASK_096
#define  NET_IPv6_POLICY_07_PRECEDENCE              1
#define  NET_IPv6_POLICY_07_LABEL                   3

static  const  NET_IPv6_POLICY NetIPv6_Policy_07       = {
                                                            NET_IPv6_POLICY_07_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_07_MASK_PTR,
                                                            NET_IPv6_POLICY_07_PRECEDENCE,
                                                            NET_IPv6_POLICY_07_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 08
---------------------------------------------------------------------------------------------------------
 */

static  const  NET_IPv6_ADDR   NET_IPv6_POLICY_08_ADDR = { { 0xFE,0xC0,0,0,0,0,0,0,0,0,0,0,0,   0,   0,0 } };
#define  NET_IPv6_POLICY_08_PREFIX_ADDR_PTR       &NET_IPv6_POLICY_08_ADDR
#define  NET_IPv6_POLICY_08_MASK_PTR              &NET_IPv6_POLICY_MASK_010
#define  NET_IPv6_POLICY_08_PRECEDENCE              1
#define  NET_IPv6_POLICY_08_LABEL                  11

static  const  NET_IPv6_POLICY NetIPv6_Policy_08       = {
                                                            NET_IPv6_POLICY_08_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_08_MASK_PTR,
                                                            NET_IPv6_POLICY_08_PRECEDENCE,
                                                            NET_IPv6_POLICY_08_LABEL
                                                         };


/*
---------------------------------------------------------------------------------------------------------
-                                              POLICY 09
---------------------------------------------------------------------------------------------------------
 */


static  const  NET_IPv6_ADDR   NET_IPv6_POLICY_09_ADDR = { { 0x3F,0xFE,0,0,0,0,0,0,0,0,0,0,0,   0,   0,0 } };
#define  NET_IPv6_POLICY_09_PREFIX_ADDR_PTR       &NET_IPv6_POLICY_09_ADDR
#define  NET_IPv6_POLICY_09_MASK_PTR              &NET_IPv6_POLICY_MASK_016
#define  NET_IPv6_POLICY_09_PRECEDENCE              1
#define  NET_IPv6_POLICY_09_LABEL                  12

static  const  NET_IPv6_POLICY NetIPv6_Policy_09       = {
                                                            NET_IPv6_POLICY_09_PREFIX_ADDR_PTR,
                                                            NET_IPv6_POLICY_09_MASK_PTR,
                                                            NET_IPv6_POLICY_09_PRECEDENCE,
                                                            NET_IPv6_POLICY_09_LABEL
                                                         };



/*
---------------------------------------------------------------------------------------------------------
-                                            POLICY TABLE
---------------------------------------------------------------------------------------------------------
 */

                                                        /* IPv6 Policy table                                    */
static  const  NET_IPv6_POLICY  *NetIPv6_PolicyTbl[]   = {
                                                           &NetIPv6_Policy_01,
                                                           &NetIPv6_Policy_02,
                                                           &NetIPv6_Policy_03,
                                                           &NetIPv6_Policy_04,
                                                           &NetIPv6_Policy_05,
                                                           &NetIPv6_Policy_06,
                                                           &NetIPv6_Policy_07,
                                                           &NetIPv6_Policy_08,
                                                           &NetIPv6_Policy_09,
                                                         };

#define NET_IPv6_POLICY_TBL_SIZE            (sizeof(NetIPv6_PolicyTbl))


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  NET_IPv6_IF_CFG               NetIPv6_IF_CfgTbl[NET_IF_NBR_IF_TOT];


static  NET_BUF                      *NetIPv6_FragReasmListsHead;     /* Ptr to head of frag reasm lists.               */
static  NET_BUF                      *NetIPv6_FragReasmListsTail;     /* Ptr to tail of frag reasm lists.               */

static  CPU_INT08U                    NetIPv6_FragReasmTimeout_sec;   /* IPv6 frag reasm timeout (in secs ).            */
static  NET_TMR_TICK                  NetIPv6_FragReasmTimeout_tick;  /* IPv6 frag reasm timeout (in ticks).            */

static  CPU_INT32U                    NetIPv6_TxID_Ctr;               /* Global tx ID field ctr.                        */

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  NET_IPv6_AUTO_CFG_OBJ        *NetIPv6_AutoCfgObjTbl[NET_IF_NBR_IF_TOT];

static  NET_IPv6_AUTO_CFG_HOOK_FNCT   NetIPv6_AutoCfgHookFnct;
#endif

static  NET_IPv6_ADDR_HOOK_FNCT       NetIPv6_AddrCfgHookFnct;



/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                            /* --------- IPv6 AUTO CFG FNCTS ---------- */
#ifdef NET_IPv6_ADDR_AUTO_CFG_MODULE_EN

static         void              NetIPv6_AddrAutoCfgHandler       (       NET_IF_NBR                 if_nbr,
                                                                          NET_ERR                   *p_err);

static         NET_IPv6_ADDR    *NetIPv6_CfgAddrRand              (       NET_IF_NBR                 if_nbr,
                                                                          NET_IPv6_ADDR             *p_ipv6_id,
                                                                          CPU_BOOLEAN                dad_en,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_CfgAddrGlobal            (       NET_IF_NBR                 if_nbr,
                                                                          NET_IPv6_ADDR             *p_ipv6_addr,
                                                                          CPU_BOOLEAN                dad_en,
                                                                          NET_ERR                   *p_err);

#ifdef NET_DAD_MODULE_EN
static         void              NetIPv6_AddrAutoCfgDAD_Result    (       NET_IF_NBR                 if_nbr,
                                                                          NET_DAD_OBJ               *p_dad_obj,
                                                                          NET_IPv6_ADDR_CFG_STATUS   status);
#endif
#endif

#ifdef NET_DAD_MODULE_EN
static         void              NetIPv6_CfgAddrAddDAD_Result     (       NET_IF_NBR                 if_nbr,
                                                                          NET_DAD_OBJ               *p_dad_obj,
                                                                          NET_IPv6_ADDR_CFG_STATUS   status);
#endif

                                                                            /* ------- SRC ADDR SELECTION FNCTS ------- */
static  const  NET_IPv6_ADDRS   *NetIPv6_AddrSrcSel               (       NET_IF_NBR                 if_nbr,
                                                                   const  NET_IPv6_ADDR             *p_addr_dest,
                                                                   const  NET_IPv6_ADDRS            *p_addr_tbl,
                                                                          NET_IP_ADDRS_QTY           addr_tbl_qty,
                                                                          NET_ERR                   *p_err);

static  const  NET_IPv6_POLICY  *NetIPv6_AddrSelPolicyGet         (const  NET_IPv6_ADDR             *p_addr);



                                                                            /* -------------- CFG FNCTS --------------- */
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
static         void              NetIPv6_CfgAddrValidate          (       NET_IPv6_ADDR             *p_addr,
                                                                          CPU_INT08U                 prefix_len,
                                                                          NET_ERR                   *p_err);
#endif  /* NET_ERR_CFG_ARG_CHK_EXT_EN */

                                                                            /* -------- VALIDATE RX DATAGRAMS --------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static         void              NetIPv6_RxPktValidateBuf         (       NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_ERR                   *p_err);
#endif  /* NET_ERR_CFG_ARG_CHK_DBG_EN */

static         void              NetIPv6_RxPktValidate            (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_IPv6_HDR              *p_ip_hdr,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_RxPktValidateNextHdr     (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_IPv6_NEXT_HDR          next_hdr,
                                                                          CPU_INT16U                 protocol_ix,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_RxPktProcessExtHdr       (       NET_BUF                   *p_buf,
                                                                          NET_ERR                   *p_err);

static         CPU_INT16U        NetIPv6_RxOptHdr                 (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_PROTOCOL_TYPE          proto_type,
                                                                          NET_ERR                   *p_err);

static         CPU_INT16U        NetIPv6_RxRoutingHdr             (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_ERR                   *p_err);

static         CPU_INT16U        NetIPv6_RxFragHdr                (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_ERR                   *p_err);

static         CPU_INT16U        NetIPv6_RxESP_Hdr                (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_ERR                   *p_err);

static         CPU_INT16U        NetIPv6_RxAuthHdr                (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_ERR                   *p_err);

#if 0
static         CPU_INT16U        NetIPv6_RxNoneHdr                (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_ERR                   *p_err);
#endif

static         CPU_INT16U        NetIPv6_RxMobilityHdr            (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_NEXT_HDR         *p_next_hdr,
                                                                          NET_ERR                   *p_err);


                                                                            /* ------------ REASM RX FRAGS ------------ */
static         NET_BUF          *NetIPv6_RxPktFragReasm           (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_IPv6_HDR              *p_ip_hdr,
                                                                          NET_ERR                   *p_err);

static         NET_BUF          *NetIPv6_RxPktFragListAdd         (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          CPU_INT16U                 frag_ip_flags,
                                                                          CPU_INT32U                 frag_offset,
                                                                          CPU_INT32U                 frag_size,
                                                                          NET_ERR                   *p_err);

static         NET_BUF          *NetIPv6_RxPktFragListInsert      (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          CPU_INT16U                 frag_ip_flags,
                                                                          CPU_INT32U                 frag_offset,
                                                                          CPU_INT32U                 frag_size,
                                                                          NET_BUF                   *p_frag_list,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_RxPktFragListRemove      (       NET_BUF                   *p_frag_list,
                                                                          CPU_BOOLEAN                tmr_free);

static         void              NetIPv6_RxPktFragListDiscard     (       NET_BUF                   *p_frag_list,
                                                                          CPU_BOOLEAN                tmr_free,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_RxPktFragListUpdate      (       NET_BUF                   *p_frag_list,
                                                                          NET_BUF_HDR               *p_frag_list_buf_hdr,
                                                                          CPU_INT16U                 frag_ip_flags,
                                                                          CPU_INT32U                 frag_offset,
                                                                          CPU_INT32U                 frag_size,
                                                                          NET_ERR                   *p_err);

static         NET_BUF          *NetIPv6_RxPktFragListChkComplete (       NET_BUF                   *p_frag_list,
                                                                          NET_BUF_HDR               *p_frag_list_buf_hdr,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_RxPktFragTimeout         (       void                      *p_frag_list_timeout);


                                                                            /* ---------- DEMUX RX DATAGRAMS ---------- */
static         void              NetIPv6_RxPktDemuxDatagram       (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_ERR                   *p_err);


static         void              NetIPv6_RxPktDiscard             (       NET_BUF                   *p_buf,
                                                                          NET_ERR                   *p_err);

                                                                            /* ----------- VALIDATE TX PKTS ----------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static         void              NetIPv6_TxPktValidate            (const  NET_BUF_HDR               *p_buf_hdr,
                                                                   const  NET_IPv6_ADDR             *p_addr_src,
                                                                   const  NET_IPv6_ADDR             *p_addr_dest,
                                                                          NET_IPv6_TRAFFIC_CLASS     traffic_class,
                                                                          NET_IPv6_FLOW_LABEL        flow_label,
                                                                          NET_IPv6_HOP_LIM           hop_lim,
                                                                          NET_ERR                   *p_err);
#endif  /* NET_ERR_CFG_ARG_CHK_???_EN */

                                                                            /* ------------- TX IPv6 PKTS ------------- */
static         void              NetIPv6_TxPkt                    (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_IPv6_ADDR             *p_addr_src,
                                                                          NET_IPv6_ADDR             *p_addr_dest,
                                                                          NET_IPv6_EXT_HDR          *p_ext_hdr_list,
                                                                          NET_IPv6_TRAFFIC_CLASS     traffic_class,
                                                                          NET_IPv6_FLOW_LABEL        flow_label,
                                                                          NET_IPv6_HOP_LIM           hop_lim,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_TxPktPrepareHdr          (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          CPU_INT16U                 protocol_ix,
                                                                          NET_IPv6_ADDR             *p_addr_src,
                                                                          NET_IPv6_ADDR             *p_addr_dest,
                                                                          NET_IPv6_EXT_HDR          *p_ext_hdr_list,
                                                                          NET_IPv6_TRAFFIC_CLASS     traffic_class,
                                                                          NET_IPv6_FLOW_LABEL        flow_label,
                                                                          NET_IPv6_HOP_LIM           hop_lim,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_TxPktPrepareExtHdr       (       NET_BUF                   *p_buf,
                                                                          NET_IPv6_EXT_HDR          *p_ext_hdr_list,
                                                                          NET_ERR                   *p_err);


#if 0
static         void              NetIPv6_TxPktPrepareFragHdr      (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          CPU_INT16U                *p_protocol_ix,
                                                                          NET_ERR                   *p_err);
#endif


                                                                            /* ----------- TX IPv6 DATAGRAMS ---------- */
static         void              NetIPv6_TxPktDatagram            (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_TxPktDatagramRouteSel    (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_ERR                   *p_err);


static         void              NetIPv6_TxPktDiscard             (       NET_BUF                   *p_buf,
                                                                          NET_ERR                   *p_err);


                                                                            /* ----------- RE-TX IPv6 PKTS ------------ */
static         void              NetIPv6_ReTxPkt                  (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_ERR                   *p_err);

static         void              NetIPv6_ReTxPktPrepareHdr        (       NET_BUF                   *p_buf,
                                                                          NET_BUF_HDR               *p_buf_hdr,
                                                                          NET_ERR                   *p_err);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            NetIPv6_Init()
*
* Description : (1) Initialize Internet Protocol Layer :
*
*                   (a) Initialize ALL interfaces' configurable IPv6 addresses.
*                   (b) Initialize IPv6 fragmentation list pointers and fragmentation timeout.
*                   (c) Initialize IPv6 identification (ID) counter.
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                 IPv6 Layer Initialization successful.
*                               NET_IPv6_ERR_DAD_OBJ_POOL_CREATE  Error in DAD obj pool creation.
*                               NET_ERR_FAULT_MEM_ALLOC           Error in Memory alllocation for Auto-Cfg obj.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) (a) Default IPv6 address initialization is invalid & forces the developer or higher-layer
*                       protocol application to configure valid IPv6 address(s).
*
*                   (b) Address configuration state initialized to 'static' by default.
*
*                       See also 'net_ipv6.h  NETWORK INTERFACES' IPv6 ADDRESS CONFIGURATION DATA TYPE'.
*********************************************************************************************************
*/

void  NetIPv6_Init (NET_ERR  *p_err)
{
    NET_IPv6_IF_CFG        *p_ip_if_cfg;
    NET_IPv6_ADDRS         *p_ip_addrs;
#ifdef NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    NET_IPv6_AUTO_CFG_OBJ  *p_obj;
    CPU_INT08U              i;
    LIB_ERR                 err_lib;
#endif
    NET_IP_ADDRS_QTY        addr_ix;
    NET_IF_NBR              if_nbr;
    NET_ERR                 err;


                                                                /* ----------------- INIT IPv6 ADDRS ------------------ */
    if_nbr      =  NET_IF_NBR_BASE;
    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];
    for ( ; if_nbr < NET_IF_NBR_IF_TOT; if_nbr++) {             /* Init ALL IF's IPv6 addrs to NONE (see Note #2a).     */
        addr_ix    =  0u;
        p_ip_addrs = &p_ip_if_cfg->AddrsTbl[addr_ix];
        for ( ; addr_ix < NET_IPv6_CFG_IF_MAX_NBR_ADDR; addr_ix++) {
            NetIPv6_AddrUnspecifiedSet(&p_ip_addrs->AddrHost, &err);
            p_ip_addrs->AddrMcastSolicitedPtr = (NET_IPv6_ADDR *)0;
            p_ip_addrs->AddrCfgState          = NET_IPv6_ADDR_CFG_STATE_NONE;
            p_ip_addrs->AddrHostPrefixLen     = 0u;
            p_ip_addrs->AddrState             = NET_IPv6_ADDR_STATE_NONE;
            p_ip_addrs->IsValid               = DEF_NO;
            p_ip_addrs++;
        }

        p_ip_if_cfg->AddrsNbrCfgd         = 0u;

        p_ip_if_cfg++;

    }

#ifdef NET_DAD_MODULE_EN
    NetDAD_Init(p_err);
    if (*p_err != NET_DAD_ERR_NONE) {
        *p_err = NET_IPv6_ERR_FAULT;
         goto exit;
    }
#endif

                                                                /* -------------- INIT IPV6 AUTOCFG OBJ --------------- */
#ifdef NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    for (i = 0; i < NET_IF_NBR_IF_TOT; i++) {

        p_obj = (NET_IPv6_AUTO_CFG_OBJ *)Mem_SegAlloc(DEF_NULL,
                                                      DEF_NULL,
                                                      sizeof(NET_IPv6_AUTO_CFG_OBJ),
                                                     &err_lib);
        if (err_lib != LIB_MEM_ERR_NONE) {
           *p_err = NET_ERR_FAULT_MEM_ALLOC;
            goto exit;
        }

        NetIPv6_AutoCfgObjTbl[i] = p_obj;

        p_obj->En                        = DEF_NO;
        p_obj->DAD_En                    = DEF_NO;
        p_obj->State                     = NET_IPv6_AUTO_CFG_STATE_NONE;
        p_obj->AddrLocalPtr              = DEF_NULL;
        p_obj->AddrGlobalPtr             = DEF_NULL;
    }
#endif

                                                                /* --------------- INIT IPv6 FRAG LISTS --------------- */
    NetIPv6_FragReasmListsHead = DEF_NULL;
    NetIPv6_FragReasmListsTail = DEF_NULL;

                                                                /* -------------- INIT IPv6 FRAG TIMEOUT -------------- */
    NetIPv6_CfgFragReasmTimeout(NET_IPv6_FRAG_REASM_TIMEOUT_DFLT_SEC);

                                                                /* ----------------- INIT IPv6 ID CTR ----------------- */
    NetIPv6_TxID_Ctr =  NET_IPv6_ID_INIT;

   *p_err = NET_IPv6_ERR_NONE;

    goto exit;

exit:
   return;
}


/*
*********************************************************************************************************
*                                     NetIPv6_LinkStateSubscriber()
*
* Description : (1) IPv6 subscriber function to link change. When link becomes UP:
*                   (a) Disabled all configured IPv6 addresses.
*                   (b) Do IPv6 Address Static Configuration for addresses that where statically configured.
*                   (c) Do IPv6 Auto-configuration if conditions apply.
*
*
* Argument(s) : if_nbr      Network interface number on which link state change occurred.
*
*               link_state  Current link state of given interface.
*
* Return(s)   : None.
*
* Caller(s)   : Referenced in NetIP_IF_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

void  NetIPv6_LinkStateSubscriber (NET_IF_NBR         if_nbr,
                                   NET_IF_LINK_STATE  link_state)
{
    NET_IPv6_IF_CFG           *p_ipv6_if_cfg  = DEF_NULL;
    NET_IP_ADDRS_QTY           addr_cfgd_nbr  = 0;
    NET_IPv6_ADDRS            *p_ip_addrs     = DEF_NULL;
    NET_IP_ADDRS_QTY           addr_ix        = 0;
    CPU_BOOLEAN                start_auto     = DEF_YES;
    NET_IPv6_ADDR_CFG_STATE    addr_cfg_state = NET_IPv6_ADDR_CFG_STATE_NONE;
#ifdef NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    NET_IPv6_AUTO_CFG_OBJ     *p_auto_obj     = DEF_NULL;
    CPU_BOOLEAN                auto_en        = DEF_NO;
#endif
    NET_ERR                    err_net;
    CPU_SR_ALLOC();


                                                                /* ----------- ACQUIRE NETWORK GLOBAL LOCK ------------ */
    Net_GlobalLockAcquire((void *)&NetIPv6_LinkStateSubscriber, &err_net);
    if (err_net != NET_ERR_NONE) {
         goto exit;
    }

                                                                /* --------- CHECK IF ADDRESSES ARE CFG ON IF --------- */
    p_ipv6_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];

    addr_cfgd_nbr = p_ipv6_if_cfg->AddrsNbrCfgd;

    if (addr_cfgd_nbr > 0) {

        addr_ix     =  0u;
        p_ip_addrs  = &p_ipv6_if_cfg->AddrsTbl[addr_ix];

        for (addr_ix = 0u; addr_ix < NET_IPv6_CFG_IF_MAX_NBR_ADDR; addr_ix++) {
            p_ip_addrs->AddrState = NET_IPv6_ADDR_STATE_NONE;
            p_ip_addrs->IsValid   = DEF_NO;

            if (link_state == NET_IF_LINK_UP) {
                addr_cfg_state = p_ip_addrs->AddrCfgState;

                if (addr_cfg_state == NET_IPv6_ADDR_CFG_STATE_STATIC) {
                    NetIPv6_CfgAddrAddHandler(if_nbr,
                                             &p_ip_addrs->AddrHost,
                                              p_ip_addrs->AddrHostPrefixLen,
                                              0,
                                              0,
                                              NET_IPv6_ADDR_CFG_MODE_MANUAL,
                                              DEF_YES,
                                              NET_IPv6_ADDR_CFG_TYPE_STATIC_NO_BLOCKING,
                                             &err_net);
                    switch (err_net) {
                        case NET_IPv6_ERR_NONE:
                        case NET_ERR_FAULT_FEATURE_DIS:
                        case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
                        case NET_IPv6_ERR_ADDR_CFG_DUPLICATED:
                             start_auto = DEF_NO;                   /* Disable auto-cfg since static addr are configured.   */
                             break;

                        default:
                             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.AddrStaticCfgFaultCtr);
                             break;
                    }
                }
            }
            p_ip_addrs++;
        }
    }

                                                                    /* --------- RESTART IPV6 AUTO-CONFIGURATION ---------- */
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    if (link_state == NET_IF_LINK_UP) {
        p_auto_obj = NetIPv6_AutoCfgObjTbl[if_nbr];
        CPU_CRITICAL_ENTER();
        auto_en    = p_auto_obj->En;
        CPU_CRITICAL_EXIT();

        if ((start_auto == DEF_YES) &&
            (auto_en    == DEF_YES)) {

            NetIPv6_AddrAutoCfgHandler(if_nbr,
                                      &err_net);
            if (err_net != NET_IPv6_ERR_NONE) {
                NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.AddrAutoCfgFaultCtrl);
            }
        }
    }
#endif


    (void)&link_state;
    (void)&start_auto;


                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

exit:
    return;
}


/*
*********************************************************************************************************
*                                      NetIPv6_AddrAutoCfgEn()
*
* Description : Enable IPv6 Auto-configuration process. If the link state is UP, the IPv6 Auto-configuration
*               will start, else the Auto-configuration will start when the link becomes UP.
*
* Argument(s) : if_nbr      Network interface number to enable the address auto-configuration on.
*
*               dad_en      DEF_YES, Do the Duplication Address Detection (DAD)
*                           DEF_NO , otherwise
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Enabled IPv6 Auto-Cfg successful.
*                               NET_INIT_ERR_NOT_COMPLETED      Network stack initialization is not completed.
*
*                               ----------- RETURNED BY Net_GlobalLockAcquire() ------------
*                               See Net_GlobalLockAcquire() for additional error codes.
*
*                               ---------- RETURNED BY NetIF_IsValidCfgdHandler() ----------
*                               See NetIF_IsValidCfgdHandler() for additional error codes.
*
*                               --------- RETURNED BY NetIF_LinkStateGetHandler() ----------
*                               See NetIF_LinkStateGetHandler() for additional error codes.
*
*                               --------- RETURNED BY NetIPv6_AddrAutoCfgHandler() ---------
*                               See NetIPv6_AddrAutoCfgHandler() for additional error codes.
*
* Return(s)   : DEF_OK,   if IPv6 Auto-Configuration is enabled successfully
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) NetIPv6_AddrAutoCfgEn() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) RFC #4862 , Section 4 states that "Autoconfiguration is performed only on
*                   multicast-capable links and begins when a multicast-capable interface is enabled,
*                   e.g., during system startup".
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_AddrAutoCfgEn (NET_IF_NBR    if_nbr,
                                    CPU_BOOLEAN   dad_en,
                                    NET_ERR      *p_err)
{
    CPU_BOOLEAN             result;
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    NET_IPv6_AUTO_CFG_OBJ  *p_auto_obj;
    NET_IF_LINK_STATE       link_state;



                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIPv6_AddrAutoCfgEn, p_err);
    if (*p_err != NET_ERR_NONE) {
         result   = DEF_FAIL;
         goto exit;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------- VALIDATE NET INIT IS COMPLETED ---------- */
    if (Net_InitDone != DEF_YES) {
        result   = DEF_FAIL;
       *p_err    =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         result   = DEF_FAIL;
         goto exit_release;
    }
#endif

                                                                /* ----------- SAVED PREFERENCE FOR AUTOCFG ----------- */
    p_auto_obj         =  NetIPv6_AutoCfgObjTbl[if_nbr];
    p_auto_obj->En     =  DEF_YES;
    p_auto_obj->DAD_En = (dad_en == DEF_YES) ? DEF_YES : DEF_NO;

                                                                /* ----------------- CHECK LINK STATE ----------------- */
    link_state = NetIF_LinkStateGetHandler(if_nbr, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             if (link_state == NET_IF_LINK_UP) {
                 NetIPv6_AddrAutoCfgHandler(if_nbr, p_err);
                 if (*p_err != NET_IPv6_ERR_NONE) {
                     result = DEF_FAIL;
                     goto exit_release;
                 }
             }
             break;


        default:
             result = DEF_FAIL;
             goto exit_release;
    }

    result = DEF_OK;
   *p_err  = NET_IPv6_ERR_NONE;


exit_release:
                                                                   /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
#else
   *p_err = NET_ERR_FAULT_FEATURE_DIS;
    result = DEF_FAIL;
    goto exit;
#endif

   (void)&if_nbr;
   (void)&dad_en;

exit:
    return(result);
}



/*
*********************************************************************************************************
*                                       NetIPv6_AddrAutoCfgDis()
*
* Description : Disabled the IPv6 Auto-Configuration.
*
* Argument(s) : if_nbr      Network interface number on which to disabled address auto-configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Disabled IPv6 Auto-Cfg successful.
*                               NET_INIT_ERR_NOT_COMPLETED      Network stack initialization is not completed.
*
*                               --------- RETURNED BY Net_GlobalLockAcquire() ---------
*                               See Net_GlobalLockAcquire() for additional error codes.
*
*                               --------- RETURNED BY NetIF_IsValidCfgdHandler() ---------
*                               See NetIF_IsValidCfgdHandler() for additional error codes.
*
* Return(s)   : DEF_OK,   if IPv6 Auto-Configuration was disabled successfully.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) NetIPv6_AddrAutoCfgDis() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) In a case of a link status change, the auto-configuration will not be called.
*                   The Hook function after the Auto-configuration completion will also not occurred.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_AddrAutoCfgDis (NET_IF_NBR   if_nbr,
                                     NET_ERR     *p_err)
{
    CPU_BOOLEAN             result;
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
    NET_IPv6_AUTO_CFG_OBJ  *p_auto_obj;



                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIPv6_AddrAutoCfgEn, p_err);
    if (*p_err != NET_ERR_NONE) {
        result = DEF_FAIL;
        goto exit;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------- VALIDATE NET INIT IS COMPLETED ---------- */
    if (Net_InitDone != DEF_YES) {
        result = DEF_FAIL;
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        Net_GlobalLockRelease();                                /* Release network lock                                 */
        goto exit;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        result = DEF_FAIL;
        Net_GlobalLockRelease();                                /* Release network lock                                 */
        goto exit;
    }
#endif

    p_auto_obj     = NetIPv6_AutoCfgObjTbl[if_nbr];
    p_auto_obj->En = DEF_NO;

    result         = DEF_OK;

#else
    result = DEF_FAIL;
   *p_err= NET_ERR_FAULT_FEATURE_DIS;
    goto exit;
#endif


exit:
   (void)if_nbr;

    return (result);
}



/*
*********************************************************************************************************
*                                       NetIPv6_CfgAddrHookSet()
*
* Description : Configure the IPv6 Static Address Configuration hook function. This function will be called
*               each time a IPv6 static address has finished being configured.
*
* Argument(s) : if_nbr      Network interface number where the address configuration took place.
*
*               fnct        Pointer to hook function to call when the IPv6 static address configuration ends.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Hook function set successful.
*
*                               ----------- RETURNED BY Net_GlobalLockAcquire() : ------------
*                               See Net_GlobalLockAcquire() for additional return error codes.
*
* Return(s)   : None.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) NetIPv6_CfgAddrHookSet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

void  NetIPv6_CfgAddrHookSet (NET_IF_NBR                if_nbr,
                              NET_IPv6_ADDR_HOOK_FNCT   fnct,
                              NET_ERR                  *p_err)
{


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION();
    }
#endif

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIPv6_CfgAddrHookSet, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------- VALIDATE NET INIT IS COMPLETED ---------- */
    if (Net_InitDone != DEF_YES) {
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

                                                                /* ---------------- SET HOOK FUNCTION ----------------- */
    NetIPv6_AddrCfgHookFnct = fnct;

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetIPv6_AddrAutoCfgHookSet()
*
* Description : Configure IPv6 Auto-Configuration application hook function. This function will be called
*               each time an Auto-Configuration process is completed.
*
* Argument(s) : if_nbr      Network interface number where the Auto-configuration took place.
*
*               fnct        Pointer to hook function to call when the Auto-configuration ends.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Hook function set successful.
*
*                               ----------- RETURNED BY Net_GlobalLockAcquire() : ------------
*                               See Net_GlobalLockAcquire() for additional return error codes.
*
* Return(s)   : None.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) NetIPv6_AddrAutoCfgHookSet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

void  NetIPv6_AddrAutoCfgHookSet (NET_IF_NBR                    if_nbr,
                                  NET_IPv6_AUTO_CFG_HOOK_FNCT   fnct,
                                  NET_ERR                      *p_err)
{
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION();
    }
#endif

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIPv6_AddrAutoCfgHookSet, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------- VALIDATE NET INIT IS COMPLETED ---------- */
    if (Net_InitDone != DEF_YES) {
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

                                                                /* ---------------- SET HOOK FUNCTION ----------------- */
    NetIPv6_AutoCfgHookFnct = fnct;

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

   *p_err = NET_IPv6_ERR_NONE;
#else
   *p_err = NET_ERR_FAULT_FEATURE_DIS;

    (void)&if_nbr;
    (void)&fnct;
#endif
}



/*
*********************************************************************************************************
*                                        NetIPv6_CfgAddrAdd()
*
* Description : (1) Add a statically-configured IPv6 host address to an interface :
*
*                   (a) Acquire network lock
*                   (b) Validate arguments & configure address
*                   (d) Release network lock
*
*
* Argument(s) : if_nbr      Interface number to configure.
*
*               p_addr      Pointer to desired IPv6 address to add to this interface (see Note #5).
*
*               prefix_len  Prefix length of the desired IPv6 address to add to this interface.
*
*               flags       Set of flags to select options for the address configuration:
*
*                               NET_IPv6_FLAG_BLOCK_EN      Enables blocking mode if set.
*                               NET_IPv6_FLAG_DAD_EN        Enables DAD if set.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       IPv6 address successfully configured.
*                               NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS       IPv6 address configuration in progress.
*                               NET_IPv6_ERR_DAD_DISABLED               DAD flags was set but DAD is disabled.
*                               NET_IPv6_ERR_ADDR_CFG_DUPLICATED        IPv6 addr cfg failed because addr is duplicated.
*                               NET_INIT_ERR_NOT_COMPLETED              Network initialization NOT complete.
*
*                               ----------- RETURNED BY Net_GlobalLockAcquire() : ------------
*                               See Net_GlobalLockAcquire() for additional return error codes.
*
*                               ----------- RETURNED BY NetIF_IsValidCfgdHandler() : ------------
*                               See NetIF_IsValidCfgdHandler() for additional return error codes.
*
*                               ----------- RETURNED BY NetIPv6_CfgAddrValidate() : ------------
*                               See NetIPv6_CfgAddrValidate() for additional return error codes.
*
*                               ----------- RETURNED BY NetIPv6_CfgAddrAddHandler() : ------------
*                               See NetIPv6_CfgAddrAddHandler() for additional return error codes.
*
*
* Return(s)   : DEF_OK,   if valid IPv6 address configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #3].
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_CfgAddrAdd (NET_IF_NBR      if_nbr,
                                 NET_IPv6_ADDR  *p_addr,
                                 CPU_INT08U      prefix_len,
                                 NET_FLAGS       flags,
                                 NET_ERR        *p_err)
{
    NET_IPv6_ADDR_CFG_TYPE  addr_cfg_type;
    CPU_BOOLEAN             block_en;
    CPU_BOOLEAN             dad_en;
    CPU_BOOLEAN             result;


                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetIPv6_CfgAddrAdd, p_err);  /* See Note #3b.                                        */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT completed, exit (see Note #4).           */
        result = DEF_FAIL;
       *p_err = NET_INIT_ERR_NOT_COMPLETED;
        goto exit_release;
    }

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         result = DEF_FAIL;
         goto exit_release;
    }

                                                                /* --------------- VALIDATE IPv6 ADDR ----------------- */
    NetIPv6_CfgAddrValidate(p_addr, prefix_len, p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         result = DEF_FAIL;
         goto exit_release;
    }

                                                                /* ----------------- VALIDATE ERR PTR ----------------- */
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }
#endif

    block_en = DEF_BIT_IS_SET(flags, NET_IPv6_FLAG_BLOCK_EN);
    dad_en   = DEF_BIT_IS_SET(flags, NET_IPv6_FLAG_DAD_EN);

                                                                /* --------------- SET DAD PARAMETERS ----------------- */
    if (block_en == DEF_YES) {
        addr_cfg_type = NET_IPv6_ADDR_CFG_TYPE_STATIC_BLOCKING;
    } else {
        addr_cfg_type = NET_IPv6_ADDR_CFG_TYPE_STATIC_NO_BLOCKING;
    }

                                                                /* --------------------- CFG ADDR --------------------- */
    (void)NetIPv6_CfgAddrAddHandler(if_nbr,
                                    p_addr,
                                    prefix_len,
                                    0,
                                    0,
                                    NET_IPv6_ADDR_CFG_MODE_MANUAL,
                                    dad_en,
                                    addr_cfg_type,
                                    p_err);
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
        case NET_ERR_FAULT_FEATURE_DIS:
             result = DEF_OK;
             break;


        case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
        case NET_IPv6_ERR_ADDR_CFG_DUPLICATED:
        default:
             result = DEF_FAIL;
             break;
    }

    goto exit_release;


exit_lock_fault:
    result = DEF_FAIL;
    goto exit;

exit_release:
    Net_GlobalLockRelease();

exit:
    return (result);
}


/*
*********************************************************************************************************
*                                     NetIPv6_CfgAddrAddHandler()
*
* Description : (1) Add a statically-configured IPv6 host address to an interface :
*
*                   (a) Validate  address configuration :
*                       (1) Check if address is already configured.
*                       (2) Validate number of configured IP addresses.
*
*                   (b) Configure static IPv6 address
*
*                   (c) Join multicast group associated with address.
*
*                   (d) Do Duplication address detection (DAD) if enabled.
*
*               (2) Configured IPv6 addresses are organized in an address table implemented as an array :
*
*                   (a) (1) (A) NET_IPv6_CFG_IF_MAX_NBR_ADDR configures each interface's maximum number
*                               of configured IPv6 addresses.
*
*                           (B) This value is used to declare the size of each interface's address table.
*
*                       (2) Each configurable interface's 'AddrsNbrCfgd' indicates the current number of
*                           configured IPv6 addresses.
*
*                   (b) Each address table is zero-based indexed :
*
*                       (1) Configured addresses are organized contiguously from indices '0' to 'N - 1'.
*
*                       (2) NO addresses         are configured             from indices 'N' to 'M - 1',
*                           for 'N' NOT equal to 'M'.
*
*                       (3) The next available table index to add a configured address is at index 'N',
*                           if  'N' NOT equal to 'M'.
*
*                       (4) Each address table is initialized, & also de-configured, with NULL address
*                           value NET_IPv6_ADDR_NONE, at ALL table indices following configured addresses.
*
*                               where
*                                       M       maximum number of configured addresses (see Note #2a1A)
*                                       N       current number of configured addresses (see Note #2a1B)
*
*
*                                                    Configured IPv6
*                                                    Addresses Table
*                                                     (see Note #2)
*
*                                -------------------      -----            -----
*                                |  Cfg'd Addr #0  |        ^                ^
*                                |-----------------|        |                |
*                                |  Cfg'd Addr #1  |                         |
*                                |-----------------|  Current number         |
*                                |  Cfg'd Addr #2  |  of configured          |
*                                |-----------------|  IPv6 addresses         |
*                                |        .        | on an interface         |
*                                |        .        | (see Note #2a2)
*                                |        .        |                   Maximum number
*                                |-----------------|        |          of configured
*                                |  Cfg'd Addr #N  |        v          IPv6 addresses
*      Next available            |-----------------|      -----       for an interface
*   address to configure  -----> |    ADDR NONE    |        ^         (see Note #2a1)
*      (see Note #2b3)           |-----------------|        |
*                                |        .        |                         |
*                                |        .        |  Non-configured         |
*                                |        .        | address entries         |
*                                |        .        | (see Note #2b4)         |
*                                |        .        |                         |
*                                |-----------------|        |                |
*                                |    ADDR NONE    |        v                v
*                                -------------------      -----            -----
*
*
*                   See also 'net_ipv6.h  NETWORK INTERFACES' IPv6 ADDRESS CONFIGURATION DATA TYPE  Note #1a'.
*
*
* Argument(s) : if_nbr      Interface number to configure.
*
*               p_addr      Pointer to desired IPv6 address to add to this interface (see Note #5).
*               ------      Argument validated by caller(s).
*
*               prefix_len  Prefix length of the desired IPv6 address to add to this interface.
*
*               cfg_mode    Desired value for configuration mode :
*
*                               NET_IPv6_ADDR_CFG_MODE_MANUAL   Address is configured manually.
*
*                               NET_IPv6_ADDR_CFG_MODE_AUTO     Address is configured using stateless address
*                                                                   auto-configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       IPv6 address successfully configured.
*                               NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS       IPv6 address configuration in progress.
*                               NET_IPv6_ERR_ADDR_CFG_DUPLICATED        IPv6 address is already used on network.
*                               NET_IPv6_ERR_ADDR_CFG                   IPv6 address configuration faulted.
*                               NET_IPv6_ERR_INVALID_ADDR_CFG_MODE      Invalid IPv6 address configuration mode.
*                               NET_IPv6_ERR_ADDR_CFG_STATE             Invalid IPv6 address configuration state
*                               NET_IPv6_ERR_ADDR_TBL_FULL              Interface's configured IPv6 address table full.
*                               NET_IPv6_ERR_ADDR_CFG_IN_USE            IPv6 address already configured on an interface.
*
*
* Return(s)   : DEF_OK,   if valid IPv6 address configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_CfgAddrAdd(),
*               NetIPv6_AddrAutoCfgHandler(),
*               NetNDP_RxPrefixUpdate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

NET_IPv6_ADDRS  *NetIPv6_CfgAddrAddHandler (NET_IF_NBR               if_nbr,
                                            NET_IPv6_ADDR           *p_addr,
                                            CPU_INT08U               prefix_len,
                                            CPU_INT32U               lifetime_valid,
                                            CPU_INT32U               lifetime_preferred,
                                            CPU_INT08U               cfg_mode,
                                            CPU_BOOLEAN              dad_en,
                                            NET_IPv6_ADDR_CFG_TYPE   addr_cfg_type,
                                            NET_ERR                 *p_err)
{
    NET_IF_LINK_STATE   link_state;
    CPU_BOOLEAN         is_addr_cfgd;
    CPU_BOOLEAN         is_addr_valid;
    NET_IPv6_IF_CFG    *p_ip_if_cfg;
    NET_IPv6_ADDRS     *p_ip_addrs;
    NET_IPv6_ADDRS     *p_ip_addrs_return;
    NET_TMR_TICK        timeout_tick;
#ifdef  NET_MLDP_MODULE_EN
    NET_MLDP_HOST_GRP  *p_host_grp;
    NET_IPv6_ADDR       addr_mcast_sol;
#endif
#ifdef NET_DAD_MODULE_EN
    NET_DAD_FNCT        dad_hook_fnct;
#endif


    link_state = NetIF_LinkStateGetHandler(if_nbr, p_err);

    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];

                                                                /* ---------- VALIDATE NBR CFG'D IP ADDRS ------------- */
    is_addr_cfgd = NetIPv6_IsAddrHostCfgdHandler(p_addr);
    if (is_addr_cfgd != DEF_YES) {

        if (p_ip_if_cfg->AddrsNbrCfgd >= NET_IPv6_CFG_IF_MAX_NBR_ADDR) {/* If nbr cfg'd addrs >= max, ...               */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgAddrTblFullCtr);
           *p_err  = NET_IPv6_ERR_ADDR_TBL_FULL;                        /* ... rtn tbl full.                            */
            p_ip_addrs_return = DEF_NULL;
            goto exit;
        }
    } else {

        is_addr_valid = NetIPv6_IsAddrCfgdValidHandler(p_addr);  /* Check if address is already valid.                  */
        if (is_addr_valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgInvAddrInUseCtr);
           *p_err =  NET_IPv6_ERR_ADDR_CFG_IN_USE;
            p_ip_addrs_return = DEF_NULL;
            goto exit;                                          /* NET_TODO: or update valid addr ?                     */
        }
    }

    if (is_addr_cfgd == DEF_YES) {
        p_ip_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_addr);
        if (p_ip_addrs == DEF_NULL) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgAddrNotFoundCtr);
           *p_err =  NET_IPv6_ERR_ADDR_NOT_FOUND;
            p_ip_addrs_return = DEF_NULL;
            goto exit;
        }
    } else {
        p_ip_addrs = &p_ip_if_cfg->AddrsTbl[p_ip_if_cfg->AddrsNbrCfgd];
    }

                                                                /* ------------------ CFG IPv6 ADDR ------------------- */

                                                                /* Configure addr cfg state.                            */
    switch (cfg_mode) {
        case NET_IPv6_ADDR_CFG_MODE_MANUAL:
             p_ip_addrs->AddrCfgState = NET_IPv6_ADDR_CFG_STATE_STATIC;
             break;

        case NET_IPv6_ADDR_CFG_MODE_AUTO:
             p_ip_addrs->AddrCfgState = NET_IPv6_ADDR_CFG_STATE_AUTO_CFGD;
             break;

        default:
            *p_err  = NET_IPv6_ERR_INVALID_ADDR_CFG_MODE;
             p_ip_addrs_return = DEF_NULL;
             goto exit;
    }

    if (is_addr_cfgd != DEF_YES) {
        Mem_Copy(&p_ip_addrs->AddrHost,                         /* Configure host address.                              */
                  p_addr,
                  NET_IPv6_ADDR_SIZE);

        p_ip_addrs->AddrHostPrefixLen = prefix_len;             /* Configure address prefix length.                     */
        p_ip_addrs->IfNbr             = if_nbr;                 /* Configure IF number of address.                      */

        p_ip_if_cfg->AddrsNbrCfgd++;                            /* Increment number of address configured on IF.        */

    }


#ifdef  NET_MLDP_MODULE_EN
    if (link_state == NET_IF_LINK_UP) {
        if (p_ip_addrs->AddrMcastSolicitedPtr == DEF_NULL) {
            NetIPv6_AddrMcastSolicitedSet(&addr_mcast_sol,      /* Create solicited mcast addr.                         */
                                           p_addr,
                                           if_nbr,
                                           p_err);
            if (*p_err != NET_IPv6_ERR_NONE) {
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
            }


            p_host_grp = NetMLDP_HostGrpJoinHandler(if_nbr,     /* Join  mcast group of the solicited mcast.            */
                                                   &addr_mcast_sol,
                                                    p_err);
            if (*p_err != NET_MLDP_ERR_NONE) {
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
            }

            p_ip_addrs->AddrMcastSolicitedPtr = &p_host_grp->AddrGrp;

        } else {
            p_host_grp = NetMLDP_HostGrpJoinHandler(if_nbr,     /* Join  mcast group of the solicited mcast.            */
                                                    p_ip_addrs->AddrMcastSolicitedPtr,
                                                    p_err);
            if (*p_err != NET_MLDP_ERR_NONE) {
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
            }
        }
    }
#endif

    if (lifetime_valid > 0) {                                   /* Set addr Valid Lifetime Tmr                          */

        timeout_tick = (NET_TMR_TICK)lifetime_valid * NET_TMR_TIME_TICK_PER_SEC;

        if (p_ip_addrs->ValidLifetimeTmrPtr == DEF_NULL) {
            p_ip_addrs->ValidLifetimeTmrPtr = NetTmr_Get(        NetIPv6_AddrValidLifetimeTimeout,
                                                         (void *)p_ip_addrs,
                                                                 timeout_tick,
                                                                 p_err);
            if(*p_err != NET_TMR_ERR_NONE) {
                NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                p_ip_addrs_return = DEF_NULL;
               *p_err = NET_IPv6_ERR_ADDR_CFG;
                goto exit;
            }
        } else {
            NetTmr_Set(p_ip_addrs->ValidLifetimeTmrPtr,
                       NetIPv6_AddrValidLifetimeTimeout,
                       timeout_tick,
                       p_err);
            if (*p_err != NET_TMR_ERR_NONE) {
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
            }
        }
    }

    if (lifetime_preferred > 0) {                               /* Set addr Preferred Lifetime Tmr.                     */

        timeout_tick = (NET_TMR_TICK)lifetime_preferred * NET_TMR_TIME_TICK_PER_SEC;

        if (p_ip_addrs->PrefLifetimeTmrPtr == DEF_NULL) {
            p_ip_addrs->PrefLifetimeTmrPtr = NetTmr_Get(        NetIPv6_AddrPrefLifetimeTimeout,
                                                        (void *)p_ip_addrs,
                                                                timeout_tick,
                                                                p_err);
            if (*p_err != NET_TMR_ERR_NONE) {
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
            }
        } else {
            NetTmr_Set(p_ip_addrs->PrefLifetimeTmrPtr,
                       NetIPv6_AddrPrefLifetimeTimeout,
                       timeout_tick,
                       p_err);
            if (*p_err != NET_TMR_ERR_NONE) {
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
            }
        }
    }

#ifdef NET_DAD_MODULE_EN
    if ((dad_en     == DEF_YES)        &&
        (link_state == NET_IF_LINK_UP)) {

        p_ip_addrs->AddrState = NET_IPv6_ADDR_STATE_TENTATIVE;
        p_ip_addrs->IsValid   = DEF_NO;

        switch (addr_cfg_type) {
            case NET_IPv6_ADDR_CFG_TYPE_STATIC_BLOCKING:
                 dad_hook_fnct = DEF_NULL;
                 break;


            case NET_IPv6_ADDR_CFG_TYPE_STATIC_NO_BLOCKING:
            case NET_IPv6_ADDR_CFG_TYPE_RX_PREFIX_INFO:
                 dad_hook_fnct = &NetIPv6_CfgAddrAddDAD_Result;
                 break;

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
            case NET_IPv6_ADDR_CFG_TYPE_AUTO_CFG_NO_BLOCKING:
                 dad_hook_fnct = &NetIPv6_AddrAutoCfgDAD_Result;
                 break;
#endif

            default:
                 dad_hook_fnct = DEF_NULL;
                 break;
        }
                                                                /* Do DAD on address configured and ...                 */
                                                                /* ... configure addr state.                            */
        NetDAD_Start(if_nbr, &p_ip_addrs->AddrHost, addr_cfg_type, dad_hook_fnct, p_err);
        switch (*p_err) {
            case NET_DAD_ERR_NONE:
                *p_err = NET_IPv6_ERR_NONE;
                 break;


            case NET_DAD_ERR_FAILED:
                *p_err = NET_IPv6_ERR_ADDR_CFG_DUPLICATED;
                 break;


            case NET_DAD_ERR_IN_PROGRESS:
                *p_err = NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS;
                 break;


            case NET_ERR_FAULT_FEATURE_DIS:
                 p_ip_addrs->AddrState = NET_IPv6_ADDR_STATE_PREFERRED;
                 p_ip_addrs->IsValid   = DEF_YES;
                 break;


            default:
                 NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr, p_err);
                 p_ip_addrs_return = DEF_NULL;
                *p_err = NET_IPv6_ERR_ADDR_CFG;
                 goto exit;
        }
    } else if (dad_en == DEF_YES) {
        p_ip_addrs->AddrState = NET_IPv6_ADDR_STATE_TENTATIVE;
        p_ip_addrs->IsValid   = DEF_NO;
       *p_err                 = NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS;
    } else {
        p_ip_addrs->AddrState = NET_IPv6_ADDR_STATE_PREFERRED;
        p_ip_addrs->IsValid   = DEF_YES;
       *p_err                 = NET_IPv6_ERR_NONE;
    }
#else
    p_ip_addrs->AddrState = NET_IPv6_ADDR_STATE_PREFERRED;
    p_ip_addrs->IsValid   = DEF_YES;
   *p_err                 = NET_IPv6_ERR_NONE;
#endif

    p_ip_addrs_return = p_ip_addrs;

    (void)&dad_en;
    (void)&addr_cfg_type;
exit:
    return (p_ip_addrs_return);
}


/*
*********************************************************************************************************
*                                       NetIPv6_CfgAddrRemove()
*
* Description : (1) Remove a configured IPv6 host address & multicast solicited mode address from an
*                   interface :
*
*                   (a) Acquire  network lock.
*                   (b) Validate address to remove.
*                   (c) Remove configured IPv6 address from interface's IPv6 address table.
*                   (d) Release  network lock.
*
*
* Argument(s) : if_nbr          Interface number to remove address configuration.
*
*               p_addr_host     Pointer to IPv6 address to remove.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Configured IPv6 address successfully removed.
*                               NET_IPv6_ERR_INVALID_ADDR_HOST  Invalid    IPv6 address.
*                               NET_IPv6_ERR_ADDR_CFG_STATE     Invalid    IPv6 address configuration state
*                                                                 (see Note #5b).
*                               NET_IPv6_ERR_ADDR_TBL_EMPTY     Interface's configured IPv6 address table empty.
*                               NET_IPv6_ERR_ADDR_NOT_FOUND     IPv6 address NOT found in interface's configured
*                                                               IPv6 address table for specified interface.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY NetIF_IsValidCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if IPv6 address removed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv6_CfgAddrRemove() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) NetIPv6_CfgAddrRemove() blocked until network initialization completes.
*
*               (4) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv6 addresses (default configuration)
*
*                   (b) If an interface's IPv6 host address(s) are NOT currently statically- or dynamically-
*                       configured, then NO address(s) may NOT be removed.
*
*                   (c) If NO address(s) are configured on an interface after an address is removed, then
*                       the interface's address configuration is defaulted back to statically-configured.
*
*                       See also 'NetIPv6_Init()              Note #2b'
*                              & 'NetIPv6_CfgAddrRemoveAll()  Note #4c'.
*
*                   See also 'net_ipv6.h  NETWORK INTERFACES' IPv6 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv6_CfgAddrAdd()         Note #7',
*                            'NetIPv6_CfgAddrAddDynamic()  Note #8',
*                          & 'NetIPv6_CfgAddrRemoveAll()   Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_CfgAddrRemove (NET_IF_NBR      if_nbr,
                                    NET_IPv6_ADDR  *p_addr_host,
                                    NET_ERR        *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_BOOLEAN        addr_valid;
#endif
    CPU_BOOLEAN        result;


                                                                        /* ------------- ACQUIRE NET LOCK ------------- */
    Net_GlobalLockAcquire((void *)&NetIPv6_CfgAddrRemove, p_err);       /* See Note #2b.                                */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #3).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

                                                                        /* ------------- VALIDATE IF NBR -------------- */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }

                                                                        /* ------------ VALIDATE IPv6 ADDR ------------ */
    addr_valid = NetIPv6_IsValidAddrHost(p_addr_host);
    if (addr_valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgInvAddrHostCtr);
       *p_err =  NET_IPv6_ERR_INVALID_ADDR_HOST;
        goto exit_fail;
    }
#endif  /* NET_ERR_CFG_ARG_CHK_EXT_EN */

                                                                        /* Remove specific IF's cfg'd host addr.        */
    result = NetIPv6_CfgAddrRemoveHandler(if_nbr, p_addr_host, p_err);
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    result = DEF_FAIL;
#endif
exit_release:
                                                                       /* ------------- RELEASE NET LOCK ------------- */
    Net_GlobalLockRelease();

    return (result);
}


/*
*********************************************************************************************************
*                                       NetIPv6_CfgAddrRemoveHandler()
*
* Description : (1) Remove a configured IPv6 host address & multicast solicited mode address from an
*                   interface :
*
*                   (a) Validate address to remove :
*                       (1) Validate interface
*                       (2) Validate IPv6 address
*
*                   (b) Remove configured IPv6 address from interface's IPv6 address table :
*                       (1) Search table for address to remove
*                       (2) Close all connections for address
*
*
* Argument(s) : if_nbr          Interface number        to remove address configuration.
*
*               p_addr_host     Pointer to IPv6 address to remove.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Configured IPv6 address successfully removed.
*                               NET_IPv6_ERR_INVALID_ADDR_HOST  Invalid    IPv6 address.
*                               NET_IPv6_ERR_ADDR_CFG_STATE     Invalid    IPv6 address configuration state
*                                                                 (see Note #5b).
*                               NET_IPv6_ERR_ADDR_TBL_EMPTY     Interface's configured IPv6 address table empty.
*                               NET_IPv6_ERR_ADDR_NOT_FOUND     IPv6 address NOT found in interface's configured
*                                                               IPv6 address table for specified interface.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY NetIF_IsValidCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ---- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if IPv6 address configuration removed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_CfgAddrAddHandler(),
*               NetIPv6_CfgAddrRemove().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s). [see also Note #1].
*
* Note(s)     : (1) NetIPv6_CfgAddrRemoveHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_CfgAddrRemoveHandler (NET_IF_NBR      if_nbr,
                                           NET_IPv6_ADDR  *p_addr_host,
                                           NET_ERR        *p_err)
{
    NET_IPv6_ADDR      addr_cfgd;
    NET_IPv6_IF_CFG   *p_ip_if_cfg;
    NET_IPv6_ADDRS    *p_ip_addrs;
    NET_IPv6_ADDRS    *p_ip_addrs_next;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_BOOLEAN        addr_found;
    CPU_BOOLEAN        result;
#ifdef  NET_MLDP_MODULE_EN
    NET_IPv6_ADDR     *p_solicit_node_addr;
#endif


    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];

                                                                        /* ------ VALIDATE NBR CFG'D IPv6 ADDRS ------- */
    if (p_ip_if_cfg->AddrsNbrCfgd < 1) {                                /* If nbr cfg'd addrs < 1, ...                  */
        result = DEF_FAIL;
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgAddrTblEmptyCtr);
       *p_err =  NET_IPv6_ERR_ADDR_TBL_EMPTY;                           /* ... rtn tbl empty.                           */
        goto exit;
    }


                                                                        /* -------- SRCH IPv6 ADDR IN ADDR TBL -------- */
    addr_ix     =  0u;
    addr_found  =  DEF_NO;
    p_ip_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];

    while ((addr_ix    <  p_ip_if_cfg->AddrsNbrCfgd) &&                 /* Srch ALL addrs ...                           */
           (addr_found == DEF_NO)) {                                    /* ... until addr found.                        */

        addr_found = Mem_Cmp(&p_ip_addrs->AddrHost, p_addr_host, NET_IPv6_ADDR_SIZE);
        if (addr_found != DEF_YES) {                                    /* If addr NOT found, ...                       */
            p_ip_addrs++;                                               /* ... adv to next entry.                       */
            addr_ix++;
        }
    }

    if (addr_found != DEF_YES) {                                        /* If addr NOT found, ...                       */
        result = DEF_FAIL;
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgAddrNotFoundCtr);
       *p_err =  NET_IPv6_ERR_ADDR_NOT_FOUND;                           /* ... rtn err.                                 */
        goto exit;
    }



                                                                        /* -------- CLOSE ALL IPv6 ADDR CONNS --------- */
                                                                        /* Close all cfg'd addr's conns.                */
    Mem_Copy(&addr_cfgd, &p_ip_addrs->AddrHost, NET_IPv6_ADDR_SIZE);
    NetConn_CloseAllConnsByAddrHandler((CPU_INT08U      *)      &addr_cfgd,
                                       (NET_CONN_ADDR_LEN)sizeof(addr_cfgd));

                                                                        /* ----------- FREE ADDRESS TIMERS ------------ */
    NetTmr_Free(p_ip_addrs->PrefLifetimeTmrPtr);
    NetTmr_Free(p_ip_addrs->ValidLifetimeTmrPtr);

    p_ip_addrs->PrefLifetimeTmrPtr  = DEF_NULL;
    p_ip_addrs->ValidLifetimeTmrPtr = DEF_NULL;

#ifdef  NET_MLDP_MODULE_EN
                                                                        /* LEAVE MLDP GROUP OF THE MCAST SOLICITED ADDR */
    p_solicit_node_addr = p_ip_addrs->AddrMcastSolicitedPtr;
    if (p_solicit_node_addr != DEF_NULL) {
        NetMLDP_HostGrpLeaveHandler(if_nbr,
                                    p_solicit_node_addr,
                                    p_err);
    }
#endif

                                                                        /* ------ REMOVE IPv6 ADDR FROM ADDR TBL ------ */
    p_ip_addrs_next = p_ip_addrs;
    p_ip_addrs_next++;
    addr_ix++;

    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {                       /* Shift ALL remaining tbl addr(s) ...          */
                                                                        /* ... following removed addr to prev tbl entry.*/
        Mem_Copy(&p_ip_addrs->AddrHost, &p_ip_addrs_next->AddrHost, NET_IPv6_ADDR_SIZE);
        p_ip_addrs++;
        p_ip_addrs_next++;
        addr_ix++;
    }

                                                                        /* Clr last addr tbl entry.                     */
    NetIPv6_AddrUnspecifiedSet(&p_ip_addrs->AddrHost, p_err);

    p_ip_addrs->AddrMcastSolicitedPtr = DEF_NULL;
    p_ip_addrs->AddrCfgState          = NET_IPv6_ADDR_CFG_STATE_NONE;
    p_ip_addrs->AddrHostPrefixLen     = 0u;

    p_ip_if_cfg->AddrsNbrCfgd--;

   *p_err  = NET_IPv6_ERR_NONE;
    result = DEF_OK;

exit:
    return (result);

}


/*
*********************************************************************************************************
*                                     NetIPv6_CfgAddrRemoveAll()
*
* Description : (1) Remove all configured IPv6 host address(s) from an interface :
*
*                   (a) Acquire  network lock
*                   (b) Validate interface
*                   (c) Remove   ALL configured IPv6 host address(s) from interface
*                   (d) Release  network lock
*
*
* Argument(s) : if_nbr      Interface number to remove address configuration.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY NetIPv6_CfgAddrRemoveAllHandler() : ---
*                               NET_IPv6_ERR_NONE               ALL configured IPv6 address(s) successfully removed.
*                               NET_IPv6_ERR_ADDR_CFG_STATE     Invalid IPv6 address configuration state (see Note #4b).
*
*                                                               ------ RETURNED BY NetIF_IsValidCfgdHandler() : ------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ------- RETURNED BY Net_GlobalLockAcquire() : --------
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if ALL interface's configured IP host address(s) successfully removed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv6_CfgAddrRemoveAll() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv6_CfgAddrRemoveAllHandler()  Note #2'.
*
*               (3) NetIPv6_CfgAddrRemoveAll() blocked until network initialization completes.
*
*               (4) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv6 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv6 address
*
*                   (b) If an interface's IPv6 host address(s) are NOT currently statically- or dynamically-
*                       configured, then NO address(s) may NOT be removed.
*
*                   (c) When NO address(s) are configured on an interface after ALL address(s) are removed,
*                       the interface's address configuration is defaulted back to statically-configured.
*
*                       See also 'NetIPv6_Init()           Note #2b'
*                              & 'NetIPv6_CfgAddrRemove()  Note #5c'.
*
*                   See also 'net_ipv6.h  NETWORK INTERFACES' IPv6 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv6._CfgAddrAdd()         Note #7',
*                            'NetIPv6._CfgAddrAddDynamic()  Note #8',
*                          & 'NetIPv6._CfgAddrRemove()      Note #5'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_CfgAddrRemoveAll (NET_IF_NBR   if_nbr,
                                       NET_ERR     *p_err)
{
    CPU_BOOLEAN  addr_remove;



    Net_GlobalLockAcquire((void *)&NetIPv6_CfgAddrRemoveAll,p_err);     /* Acquire net lock (see Note #1b).             */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #3).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);                        /* Validate IF nbr.                             */
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }
#endif
                                                                        /* Remove all IF's cfg'd host addr(s).          */
    addr_remove = NetIPv6_CfgAddrRemoveAllHandler(if_nbr, p_err);
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_remove = DEF_FAIL;
#endif

exit_release:
    Net_GlobalLockRelease();                                            /* Release net lock.                            */

    return (addr_remove);
}


/*
*********************************************************************************************************
*                                  NetIPv6_CfgAddrRemoveAllHandler()
*
* Description : (1) Remove all configured IPv6 host address(s) from an interface :
*
*                   (a) Validate IPv6 address configuration state                                   See Note #3b
*                   (b) Remove ALL configured IPv6 address(s) from interface's IPv6 address table :
*                       (1) Close all connections for each address
*                   (c) Reset    IPv6 address configuration state to static                         See Note #3c
*
*
*
* Argument(s) : if_nbr      Interface number to remove address configuration.
*               ------      Argument validated in NetIPv6_CfgAddrRemoveAll(),
*                                                 NetIPv6_CfgAddrAddDynamicStart(),
*                                                 NetIPv6_CfgAddrAddDynamic().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               ALL configured IPv6 address(s) successfully
*                                                                   removed.
*                               NET_IPv6_ERR_ADDR_CFG_STATE     Invalid IPv6 address configuration state
*                                                                   (see Note #3b).
*
* Return(s)   : DEF_OK,   if ALL interface's configured IPv6 host address(s) successfully removed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_CfgAddrRemoveAll(),
*               NetIPv6_CfgAddrAddDynamicStart(),
*               NetIPv6_CfgAddrAddDynamic().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv6_CfgAddrRemoveAllHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv6_CfgAddrRemoveAll()  Note #2'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_CfgAddrRemoveAllHandler (NET_IF_NBR   if_nbr,
                                              NET_ERR     *p_err)
{
    NET_IPv6_IF_CFG   *p_ip_if_cfg;
    NET_IPv6_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;


   p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];

                                                                        /* ------ REMOVE ALL CFG'D IPv6 ADDR(S) ------- */
    addr_ix    =  0u;
    p_ip_addrs = &p_ip_if_cfg->AddrsTbl[addr_ix];
    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {                       /* Remove ALL cfg'd addrs.                      */

                                                                        /* Close  all cfg'd addr's conns.               */
        NetConn_CloseAllConnsByAddrHandler((CPU_INT08U      *)&p_ip_addrs->AddrHost,
                                           (NET_CONN_ADDR_LEN) NET_IPv6_ADDR_SIZE);


#ifdef  NET_MLDP_MODULE_EN
                                                                /* Leave the MLDP grp of the mcast solicited addr.      */
        NetMLDP_HostGrpLeaveHandler(if_nbr,
                                    p_ip_addrs->AddrMcastSolicitedPtr,
                                    p_err);
#endif
                                                                        /* Remove addr from tbl.                        */
        NetIPv6_AddrUnspecifiedSet(&p_ip_addrs->AddrHost, p_err);

        p_ip_addrs->AddrMcastSolicitedPtr = (NET_IPv6_ADDR *)0;
        p_ip_addrs->AddrCfgState          = NET_IPv6_ADDR_CFG_STATE_NONE;
        p_ip_addrs->AddrState             = NET_IPv6_ADDR_STATE_NONE;
        p_ip_addrs->AddrHostPrefixLen     = 0u;
        p_ip_addrs->IfNbr                 = NET_IF_NBR_NONE;
        p_ip_addrs->IsValid               = DEF_NO;

        NetTmr_Free(p_ip_addrs->PrefLifetimeTmrPtr);
        NetTmr_Free(p_ip_addrs->ValidLifetimeTmrPtr);

        p_ip_addrs->PrefLifetimeTmrPtr  = DEF_NULL;
        p_ip_addrs->ValidLifetimeTmrPtr = DEF_NULL;

        p_ip_addrs++;
        addr_ix++;
    }

    p_ip_if_cfg->AddrsNbrCfgd = 0u;                                     /* NO  addr(s) cfg'd.                           */


   *p_err =  NET_IPv6_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                    NetIPv6_CfgFragReasmTimeout()
*
* Description : Configure IPv6 fragment reassembly timeout.
*
*               (1) IPv6 fragment reassembly timeout is the maximum time allowed between received IPv6
*                   fragments from the same IPv6 datagram.
*
* Argument(s) : timeout_sec     Desired value for IPv6 fragment reassembly timeout (in seconds).
*
* Return(s)   : DEF_OK,   IPv6 fragment reassembly timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) Timeout in seconds converted to 'NET_TMR_TICK' ticks in order to pre-compute initial
*                   timeout value in 'NET_TMR_TICK' ticks.
*
*               (3) 'NetIPv6_FragReasmTimeout' variables MUST ALWAYS be accessed exclusively in critical
*                    sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_CfgFragReasmTimeout (CPU_INT08U  timeout_sec)
{
    NET_TMR_TICK  timeout_tick;
    CPU_SR_ALLOC();


    if (timeout_sec < NET_IPv6_FRAG_REASM_TIMEOUT_MIN_SEC) {
        return (DEF_FAIL);
    }
    if (timeout_sec > NET_IPv6_FRAG_REASM_TIMEOUT_MAX_SEC) {
        return (DEF_FAIL);
    }

    timeout_tick                  = (NET_TMR_TICK)timeout_sec * NET_TMR_TIME_TICK_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetIPv6_FragReasmTimeout_sec  =  timeout_sec;
    NetIPv6_FragReasmTimeout_tick =  timeout_tick;
    CPU_CRITICAL_EXIT();

   (void)&NetIPv6_FragReasmTimeout_sec;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        NetIPv6_GetAddrHost()
*
* Description : Get an interface's IPv6 host address(s) [see Note #3].
*
* Argument(s) : if_nbr          Interface number to get IPv6 host address(s).
*
*               p_addr_tbl       Pointer to IPv6 address table that will receive the IPv6 host address(s)
*                                   in host-order for this interface.
*
*               p_addr_tbl_qty   Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address table, in number of IPv6 addresses,
*                                           pointed to by 'p_addr_tbl'.
*                                   (b) (1) Return the actual number of IPv6 addresses, if NO error(s);
*                                       (2) Return 0,                                 otherwise.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   - RETURNED BY NetIPv6_GetAddrHostHandler() : -
*                               NET_IPv6_ERR_NONE                   IP host address(s) successfully returned.
*                               NET_ERR_FAULT_NULL_PTR               Argument 'p_addr_tbl'/'p_addr_tbl_qty' passed
*                                                                       a NULL pointer.
*                               NET_IPv6_ERR_ADDR_NONE_AVAIL        NO IPv6 host address(s) configured on specified
*                                                                       interface.
*                               NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS   Interface in dynamic address initialization.
*                               NET_IPv6_ERR_ADDR_TBL_SIZE          Invalid IPv6 address table size.
*                               NET_IFV6_ERR_INVALID_IF             Invalid network interface number.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if interface's IPv6 host address(s) successfully returned.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrHost() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv6_GetAddrHostHandler()  Note #1'.
*
*               (2) NetIPv6_GetAddrHost() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_GetAddrHost (NET_IF_NBR         if_nbr,
                                  NET_IPv6_ADDR     *p_addr_tbl,
                                  NET_IP_ADDRS_QTY  *p_addr_tbl_qty,
                                  NET_ERR           *p_err)
{
    CPU_BOOLEAN  addr_avail;



    Net_GlobalLockAcquire((void *)&NetIPv6_GetAddrHost, p_err); /* Acquire net lock (see Note #1b).                     */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* Get all IF's host addr(s).                           */
    addr_avail = NetIPv6_GetAddrHostHandler(if_nbr, p_addr_tbl, p_addr_tbl_qty, p_err);
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_avail = DEF_FAIL;
#endif

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */


    return (addr_avail);
}


/*
*********************************************************************************************************
*                                  NetIPv6_GetAddrHostHandler()
*
* Description : Get an interface's IPv6 host address(s) [see Note #2].
*
* Argument(s) : if_nbr          Interface number to get IPv6 host address(s).
*
*               p_addr_tbl       Pointer to IPv6 address table that will receive the IPv6 host address(s)
*                                   in host-order for this interface.
*
*               p_addr_tbl_qty   Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address table, in number of IPv6 addresses,
*                                           pointed to by 'p_addr_tbl'.
*                                   (b) (1) Return the actual number of IPv6 addresses, if NO error(s);
*                                       (2) Return 0,                                   otherwise.
*
*                               See also Note #3.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                   IPv6 host address(es) successfully returned.
*                               NET_ERR_FAULT_NULL_PTR              Argument 'p_addr_tbl'/'p_addr_tbl_qty' passed
*                                                                       a NULL pointer.
*                               NET_IPv6_ERR_ADDR_NONE_AVAIL        NO IPv6 host address(s) configured on specified
*                                                                       interface.
*                               NET_IPv6_ERR_ADDR_TBL_SIZE          Invalid IPv6 address table size.
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : DEF_OK,   if interface's IPv6 host address(s) successfully returned.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_GetAddrHost(),
*               NetIPv6_GetHostAddrProtocol(),
*               NetSock_BindHandler(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrHostHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv6_GetAddrHost()  Note #1'.
*
*               (2) IPv6 address(s) returned in host-order.
*
*               (3) Since 'p_addr_tbl_qty' argument is both an input & output argument
*                   (see 'Argument(s) : p_addr_tbl_qty'), ... :
*
*                   (a) Its input value SHOULD be validated prior to use; ...
*
*                       (1) In the case that the 'p_addr_tbl_qty' argument is passed a null pointer,
*                           NO input value is validated or used.
*
*                       (2) The number of IP addresses of the table that will receive the configured
*                           IP address(s) MUST be greater than or equal to NET_IPv6_CFG_IF_MAX_NBR_ADDR.
*
*                   (b) While its output value MUST be initially configured to return a default value
*                       PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_GetAddrHostHandler (NET_IF_NBR         if_nbr,
                                         NET_IPv6_ADDR     *p_addr_tbl,
                                         NET_IP_ADDRS_QTY  *p_addr_tbl_qty,
                                         NET_ERR           *p_err)
{
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    NET_IP_ADDRS_QTY   addr_tbl_qty;
#endif
    NET_IPv6_IF_CFG   *p_ip_if_cfg;
    NET_IPv6_ADDR     *p_ip_addr;
    NET_IPv6_ADDR     *p_addr;
    NET_IP_ADDRS_QTY   addr_ix;


                                                                /* ---------------- VALIDATE ADDR TBL ----------------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_addr_tbl_qty == (NET_IP_ADDRS_QTY *)0) {              /* See Note #3a1.                                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (DEF_FAIL);
    }

     addr_tbl_qty = *p_addr_tbl_qty;
#endif
   *p_addr_tbl_qty =  0u;                                       /* Cfg rtn addr tbl qty for err  (see Note #3b).        */

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (addr_tbl_qty < NET_IPv6_CFG_IF_MAX_NBR_ADDR) {          /* Validate initial addr tbl qty (see Note #3a).        */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgAddrTblSizeCtr);
       *p_err =  NET_IPv6_ERR_ADDR_TBL_SIZE;
        return (DEF_FAIL);
    }

    if (p_addr_tbl == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (DEF_FAIL);
    }
#endif


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (DEF_FAIL);
    }
#endif


                                                                /* ------------------ GET IPv6 ADDRS ------------------ */
    p_addr = p_addr_tbl;

    if (if_nbr == NET_IF_NBR_LOOPBACK) {                        /* For loopback IF,                  ...                */
#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
                                                                /* ... get dflt IPv6 localhost addr; ...                */
        p_addr            =  (NET_IPv6_ADDR *)&NET_IPv6_ADDR_LOCAL_HOST_ADDR;
       *p_addr_tbl_qty    =   1u;
       *p_err             =   NET_IPv6_ERR_NONE;
        return (DEF_OK);
#else
       *p_err             = NET_IF_ERR_INVALID_IF;
        return (DEF_FAIL);
#endif
    }

    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];

    if (p_ip_if_cfg->AddrsNbrCfgd < 1) {
       *p_err =  NET_IPv6_ERR_ADDR_NONE_AVAIL;
        return (DEF_FAIL);
    }

    addr_ix  =  0u;
    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {               /* ... else get all cfg'd host addr(s).                 */
        p_ip_addr = &p_ip_if_cfg->AddrsTbl[addr_ix].AddrHost;
        Mem_Copy(p_addr, p_ip_addr, sizeof(NET_IPv6_ADDR));
        p_addr++;
        addr_ix++;
    }

   *p_addr_tbl_qty = p_ip_if_cfg->AddrsNbrCfgd;                 /* Rtn nbr of cfg'd addrs.                              */
   *p_err          = NET_IPv6_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       NetIPv6_GetAddrSrc()
*
* Description : Find the best matched source address in the IPv6 configured host addresses for the
*               specified destination address.
*
* Argument(s) : p_if_nbr        Pointer to given interface number if any, variable that will received the
*                                  interface number if none is given.
*
*               p_addr_src      Pointer to IPv6 suggested source address if any.
*
*               p_addr_dest     Pointer to the destination address.
*
*               p_addr_nexthop  Pointer to Next Hop IPv6 address that the function will found.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                               -- RETURNED BY NetIPv6_GetAddrSrcHandler() : -
*                               NET_IPv6_ERR_NONE               IPv6 Source address found.
*                               NET_ERR_FAULT_NULL_PTR          Argument(s) passed NULL pointer.
*                               NET_IPv6_ERR_TX_NEXT_HOP_NONE   No Next Hop was found for destination.
*                               NET_IPv6_ERR_TX_SRC_SEL_FAIL    No adequate Source address was found.
*
*                                                               --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Pointer to the IPv6 addresses structure associated with the best source address for the given
*               destination.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #3].
*
* Note(s)     : (1) NetIPv6_GetAddrSrc() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv6_GetAddrSrcHandler()  Note #1'.
*********************************************************************************************************
*/
const  NET_IPv6_ADDRS  *NetIPv6_GetAddrSrc(       NET_IF_NBR     *p_if_nbr,
                                           const  NET_IPv6_ADDR  *p_addr_src,
                                           const  NET_IPv6_ADDR  *p_addr_dest,
                                                  NET_IPv6_ADDR  *p_addr_nexthop,
                                                  NET_ERR        *p_err)
{
    const  NET_IPv6_ADDRS  *p_addrs;


    Net_GlobalLockAcquire((void *)&NetIPv6_GetAddrSrc, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

                                                                /* Get source address.                                  */
    p_addrs = NetIPv6_GetAddrSrcHandler(p_if_nbr,
                                        p_addr_src,
                                        p_addr_dest,
                                        p_addr_nexthop,
                                        p_err);

    goto exit_release;


exit_lock_fault:
    return ((NET_IPv6_ADDRS *)0);

exit_release:
    Net_GlobalLockRelease();

    return (p_addrs);
}


/*
*********************************************************************************************************
*                                   NetIPv6_GetAddrSrcHandler()
*
* Description : Find the best matched source address for the given destination address.
*
* Argument(s) : p_if_nbr        Pointer to given interface number if any, variable that will received the
*                                  interface number if none is given.
*
*               p_addr_src      Pointer to IPv6 suggested source address if any.
*
*               p_addr_dest     Pointer to the destination address.
*
*               p_addr_nexthop  Pointer to Next Hop IPv6 address that the function will found.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IPv6 Source address found.
*                               NET_ERR_FAULT_NULL_PTR          Argument(s) passed NULL pointer.
*
*                                                               ------- RETURNED BY NetNDP_NextHop() : -------
*                               NET_IPv6_ERR_TX_NEXT_HOP_NONE   No Next Hop was found for destination.
*
*                                                               --- RETURNED BY NetIPv6_AddrSrcSelect() : ----
*                               NET_IPv6_ERR_TX_SRC_SEL_FAIL    No adequate Source address was found.
*
*
* Return(s)   : Pointer to the IPv6 addresses structure associated with the best source address for the given
*               destination.
*
* Caller(s)   : NetIPv6_GetAddrSrc(),
*               NetICMPv6_TxMsgReply(),
*               NetICMPv6_TxReqHandler(),
*               NetSock_ConnHandlerAddrLocalBind(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
*
* Note(s)     : (1) NetIPv6_GetAddrSrcHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*               (2) If a valid Interface number is given, the function will pull all the addresses
*                   configured on the Interface and found the best source address for the given destination
*                   address according to the Source Address Selection Rules define in RFC#6724.
*
*               (3) Else, if no Interface number is passed, the function will first call
*                   NetNDP_NextHop() to determine the Next-Hop and therefore the good
*                   Interface to Transmit given the destination address. Afterwards, the function will
*                   continue with the source selection as mentioned in note #1.
*
*               (4) If a suggested source address is passed, the function will check if it's a address
*                   configured on the outgoing Interface. If it is the case, the suggest source address
*                   will be return and the Source Selection Algorithm will be bypass.
*********************************************************************************************************
*/

const  NET_IPv6_ADDRS  *NetIPv6_GetAddrSrcHandler (       NET_IF_NBR     *p_if_nbr,
                                                   const  NET_IPv6_ADDR  *p_addr_src,
                                                   const  NET_IPv6_ADDR  *p_addr_dest,
                                                          NET_IPv6_ADDR  *p_addr_nexthop,
                                                          NET_ERR        *p_err)
{
           NET_IF_NBR         if_nbr;
           NET_IF_NBR         if_nbr_src_addr;
           NET_IPv6_IF_CFG   *p_ip_if_cfg;
#ifdef  NET_NDP_MODULE_EN
    const  NET_IPv6_ADDR     *p_next_hop;
#endif
    const  NET_IPv6_ADDRS    *p_addrs_src;
           NET_IPv6_ADDRS    *p_addr_ip_tbl;
           NET_IP_ADDRS_QTY   addr_ip_tbl_qty;
           CPU_BOOLEAN        valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                 /* ------------ VALIDATE NO NULL POINTERS ------------ */
    if (p_if_nbr == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit_fail;
    }

    if (p_addr_dest == DEF_NULL) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        goto exit_fail;
    }
#endif

    if_nbr = *p_if_nbr;

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, p_err);
#ifdef  NET_NDP_MODULE_EN
    if (valid == DEF_YES) {                                     /* A valid IF nbr is given, ...                         */
                                                                /* ... find address of Next-Hop.                        */
        p_next_hop = NetNDP_NextHopByIF(if_nbr, p_addr_dest, p_err);
        if (*p_err == NET_NDP_ERR_TX_NO_NEXT_HOP) {
            *p_err  = NET_IPv6_ERR_NEXT_HOP;
             goto exit_fail;
        }

    } else {                                                    /* No valid IF nbr is passed to function, ...           */
                                                                /* ... find the outgoing IF and Next-Hop address.       */
        p_next_hop = NetNDP_NextHop(&if_nbr, p_addr_src, p_addr_dest, p_err);
        switch (*p_err) {
            case NET_NDP_ERR_TX_DEST_HOST_THIS_NET:
            case NET_NDP_ERR_TX_DFLT_GATEWAY:
            case NET_NDP_ERR_TX_NO_DFLT_GATEWAY:
            case NET_NDP_ERR_TX_DEST_MULTICAST:
                 break;


            case NET_NDP_ERR_TX_NO_NEXT_HOP:
            default:
                *p_err = NET_IPv6_ERR_NEXT_HOP;
                 goto exit_fail;
        }
    }


                                                                /* Set the Next-Hop address to return.                  */
    if (p_addr_nexthop != DEF_NULL) {
        p_addr_nexthop  = (NET_IPv6_ADDR *)p_next_hop;
    }
#endif

   (void)&valid;
                                                                /* Check if given source addr is configured on IF.      */
    if (p_addr_src != DEF_NULL) {
        if_nbr_src_addr = NetIPv6_GetAddrHostIF_Nbr(p_addr_src);

        if (if_nbr_src_addr == if_nbr) {                        /* Return the given src addr if it's cfgd on the IF.    */
            p_addrs_src = NetIPv6_GetAddrsHostOnIF(if_nbr, p_addr_src);
           *p_err       = NET_IPv6_ERR_NONE;
            goto exit;
        }
    }

                                                                /* Retrieve pointer to IPv6 addrs configured on IF.     */
    p_ip_if_cfg     = &NetIPv6_IF_CfgTbl[if_nbr];
    addr_ip_tbl_qty =  p_ip_if_cfg->AddrsNbrCfgd;
    p_addr_ip_tbl   = &p_ip_if_cfg->AddrsTbl[0];

                                                                /* Find best matching source address.                   */
    p_addrs_src = NetIPv6_AddrSrcSel(if_nbr,
                                     p_addr_dest,
                                     p_addr_ip_tbl,
                                     addr_ip_tbl_qty,
                                     p_err);

    goto exit;
#if (defined(NET_NDP_MODULE_EN) ||              \
     NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
exit_fail:
    p_addrs_src = DEF_NULL;
#endif

exit:
    return (p_addrs_src);
}


/*
*********************************************************************************************************
*                                   NetIPv6_GetAddrHostIF_Nbr()
*
* Description : Get the interface number for a configured IPv6 host address.
*
* Argument(s) : p_addr       Pointer to configured IPv6 host address to get the interface number (see Note #2).
*
* Return(s)   : Interface number of a configured  IPv6 host address, if available.
*
*               NET_IF_NBR_NONE,                                     otherwise.
*
* Caller(s)   : NetIPv6_GetAddrHostIF_Nbr(),
*               NetIPv6_IsAddrHostCfgdHandler(),
*               NetIPv6_GetAddrProtocolIF_Nbr().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrHostIF_Nbr() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*               (2) This function assumes that the uC/TCPIP stack does not permit two interfaces to setup
*                   the same address.
*********************************************************************************************************
*/

NET_IF_NBR  NetIPv6_GetAddrHostIF_Nbr (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN      addr_unspecified;
    NET_IPv6_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;


    addr_unspecified = NetIPv6_IsAddrUnspecified(p_addr);
    if (addr_unspecified == DEF_YES) {
        return (NET_IF_NBR_NONE);
    }

    p_ip_addrs = NetIPv6_GetAddrsHost(p_addr, &if_nbr);
    if (p_ip_addrs == (NET_IPv6_ADDRS *)0) {
        return (NET_IF_NBR_NONE);
    }

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                 NetIPv6_GetAddrHostMatchPrefix()
*
* Description : Validate a prefix as being used by an address on the given interface.
*
* Argument(s) : if_nbr         Interface number of the interface to search on.
*
*               p_prefix       Pointer to IPv6 prefix.
*
*               prefix_len     Length of the prefix in bits.
*
* Return(s)   : Pointer to IPv6 Addresses object matching the prefix.
*
*
* Caller(s)   : ????
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrHostMatchPrefix() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/
NET_IPv6_ADDRS  *NetIPv6_GetAddrHostMatchPrefix (NET_IF_NBR      if_nbr,
                                                 NET_IPv6_ADDR  *p_prefix,
                                                 CPU_INT08U      prefix_len)
{
    NET_IPv6_IF_CFG  *p_ip_if_cfg;
    NET_IPv6_ADDRS   *p_addrs;
    NET_IPv6_ADDR     mask;
    CPU_BOOLEAN       valid;
    CPU_INT08U        addr_ix;
    NET_ERR           err;


                                                                /* Set mask for given prefix.                           */
    NetIPv6_MaskGet(&mask, prefix_len, &err);
    if (err != NET_IPv6_ERR_NONE) {
        p_addrs = DEF_NULL;
        goto exit;
    }

    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];

    for (addr_ix = 0; addr_ix < NET_IPv6_CFG_IF_MAX_NBR_ADDR; addr_ix++) {
        p_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];
        valid    =  NetIPv6_IsAddrAndMaskValid((const NET_IPv6_ADDR *)&p_addrs->AddrHost,
                                               (const NET_IPv6_ADDR *)&mask);
        if (valid == DEF_YES) {
            goto exit;
        }
    }


    p_addrs = DEF_NULL;

   (void)&p_prefix;

exit:
    return (p_addrs);
}


/*
*********************************************************************************************************
*                                     NetIPv6_IsAddrAndMaskValid()
*
* Description : Validate that an IPv6 address and a mask are valid (match).
*
* Argument(s) : p_addr  Pointer to IPv6 address to validate.
*               -----   Argument checked by caller(s).
*
*               p_mask  Pointer to IPv6 mask to be use.
*               ------  Argument validated by caller(s).
*
* Return(s)   : DEF_OK,   if the address and the mask are matching.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_GetAddrHostMatchPrefix(),
*               NetIPv6_LookUpPolicyTbl().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrAndMaskValid (const  NET_IPv6_ADDR  *p_addr,
                                         const  NET_IPv6_ADDR  *p_mask)
{
    NET_IPv6_ADDR  addr_masked;
    CPU_BOOLEAN    cmp;


    NetIPv6_AddrMask(p_addr, p_mask, &addr_masked);
    cmp = NetIPv6_IsAddrsIdentical(p_mask, &addr_masked);

    return (cmp);
}


/*
*********************************************************************************************************
*                                   NetIPv6_IsAddAndPrefixLenValid()
*
* Description : Validate that an IPv6 address and the prefix length are valid (match).
*
* Argument(s) : p_addr      Pointer to IPv6 address to validate.
*               -----       Argument checked by caller(s).
*
*               prefix_len  Prefix length,
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                      Result is valid.
*
*                               ------------- Returned by NetIPv6_MaskGet() --------------
*                               NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN   Invalid mask length.
*
*
* Return(s)   : DEF_OK,   if the address and the prefix mask are matching.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetNDP_RemovePrefixDestCache().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrAndPrefixLenValid (const  NET_IPv6_ADDR  *p_addr,
                                              const  NET_IPv6_ADDR  *p_prefix,
                                                     CPU_INT08U      prefix_len,
                                                     NET_ERR        *p_err)
{
    NET_IPv6_ADDR  mask;
    NET_IPv6_ADDR  addr_masked;
    CPU_BOOLEAN    cmp;


    NetIPv6_MaskGet(&mask, prefix_len, p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         cmp = DEF_FAIL;
         goto exit;
    }

    NetIPv6_AddrMask(                        p_addr,
                     (const  NET_IPv6_ADDR *)&mask,
                                             &addr_masked);

    cmp = NetIPv6_IsAddrsIdentical(p_prefix, &addr_masked);

exit:
    return (cmp);
}



/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrMaskedValid()
*
* Description : Validate that a masked address is matching the mask.
*
* Argument(s) : p_prefix_mask   Pointer to IPv6 address mask.
*               -------------   Argument validated by caller(s).
*
*               p_addr_masked   Pointer to the masked IPv6 address.
*               -------------   Argument validated by caller(s).
*
* Return(s)   : DEF_OK,   if the address and the prefix mask are matching.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv6_AddrSrcSel(),
*               NetIPv6_IsAddrAndMaskValid(),
*               NetIPv6_IsAddrAndPrefixLenValid().
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrsIdentical (const  NET_IPv6_ADDR  *p_prefix_mask,
                                       const  NET_IPv6_ADDR  *p_addr_masked)
{
    CPU_INT08U     i;
    CPU_BOOLEAN    cmp;


    for (i = 0u; i < NET_IPv6_ADDR_SIZE; i++) {
        if (p_prefix_mask->Addr[i] != p_addr_masked->Addr[i]) {
            cmp = DEF_NO;
            goto exit;
        }
    }


    cmp = DEF_YES;

exit:
    return (cmp);
}

/*
*********************************************************************************************************
*                                     NetIPv6_GetAddrLinkLocalCfgd()
*
* Description : Get a link-local IPv6 address configured on a specific interface.
*
* Argument(s) : if_nbr      Network interface number to search for the link-local address.
*
* Return(s)   : Pointer on the link-local address, if found.
*
*               Pointer to NULL,                   otherwise.
*
* Caller(s)   : NetMLDP_TxReport().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrLinkLocalCfgd() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

NET_IPv6_ADDR  *NetIPv6_GetAddrLinkLocalCfgd (NET_IF_NBR  if_nbr)
{
 #if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN        valid;
    NET_ERR            err;
#endif
    NET_IPv6_IF_CFG   *p_ip_if_cfg;
    NET_IPv6_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_BOOLEAN        addr_found;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, &err);
    if (valid != DEF_YES) {
        return ((NET_IPv6_ADDR *)0);
    }
#endif

                                                                /* -------------- SRCH IF FOR IPv6 ADDR --------------- */
    addr_ix          = 0u;
    addr_found       = DEF_NO;
    p_ip_if_cfg      = &NetIPv6_IF_CfgTbl[if_nbr];
    p_ip_addrs       = &p_ip_if_cfg->AddrsTbl[addr_ix];

    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {               /* Srch all cfg'd addrs ...                             */
        addr_found = NetIPv6_IsAddrLinkLocal(&p_ip_addrs->AddrHost);
        if (addr_found == DEF_YES) {
            return (&p_ip_addrs->AddrHost);
        }

        p_ip_addrs++;                                           /* ... adv to IF's next addr.                           */
        addr_ix++;
    }

    return ((NET_IPv6_ADDR *)0);
}


/*
*********************************************************************************************************
*                                    NetIPv6_GetAddrMatchingLen()
*
* Description : Compute the number of identical most significant bits of two IPv6 addresses.
*
* Argument(s) : p_addr_1   First  IPv6 address for comparison.
*
*               p_addr_2   Second IPv6 address for comparison.
*
* Return(s)   : Number of matching bits, if any.
*
*               0,                       otherwise.
*
* Caller(s)   : Application,
*               NetIPv6_AddrSrcSelect(),
*               NetMLDP_HostGrpSrch().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) The returned number is based on the number of matching MSB of both IPv6 addresses:
*
*                   (a) Calling the function with the following addresses will return  32 matching bits:
*
*                       p_addr_1 -> FE80:ABCD:0000:0000:0000:0000:0000:0000
*                       p_addr_2 -> FE80:ABCD:F000:0000:0000:0000:0000:0000
*
*                   (b) Calling the function with the following addresses will return 127 matching bits:
*
*                       p_addr_1 -> FE80:ABCD:0000:0000:0000:0000:0000:0000
*                       p_addr_2 -> FE80:ABCD:0000:0000:0000:0000:0000:0001
*
*                   (c) Calling the function with the following addresses will return 0   matching bits:
*
*                       p_addr_1 -> FE80:ABCD:0000:0000:0000:0000:0000:0000
*                       p_addr_2 -> 7E80:ABCD:0000:0000:0000:0000:0000:0000
*
*                   (d) Calling the function with identical     addresses will return 128 matching bits.
*********************************************************************************************************
*/

CPU_INT08U  NetIPv6_GetAddrMatchingLen (const  NET_IPv6_ADDR  *p_addr_1,
                                        const  NET_IPv6_ADDR  *p_addr_2)
{
    CPU_INT08U   matching_bit_qty;
    CPU_INT08U   bit_ix;
    CPU_INT08U   octet_ix;
    CPU_INT08U   octet_diff;
    CPU_BOOLEAN  octet_match;
    CPU_BOOLEAN  bit_match;
    CPU_BOOLEAN  bit_clr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                             /* ------------ VALIDATE ADDRS ------------ */
    if (p_addr_1 == (NET_IPv6_ADDR *)0) {
        return (0);
    }
    if (p_addr_2 == (NET_IPv6_ADDR *)0) {
        return (0);
    }
#endif


    matching_bit_qty = 0u;
    octet_ix         = 0u;
    octet_diff       = 0u;
    bit_ix           = 0u;
    octet_match      = DEF_YES;
    bit_match        = DEF_YES;
    bit_clr          = DEF_YES;

    while((octet_ix    <  NET_IPv6_ADDR_SIZE) &&
          (octet_match == DEF_YES)) {

        if (p_addr_1->Addr[octet_ix] == p_addr_2->Addr[octet_ix]) {         /* If octets are     identical ...          */
            matching_bit_qty = matching_bit_qty + DEF_OCTET_NBR_BITS;       /* ... add 8 bits to matching bit ctr.      */
        } else {                                                            /* If octets are NOT identical ...          */
            octet_diff = p_addr_1->Addr[octet_ix] ^ p_addr_2->Addr[octet_ix];/* ... determine which bits are identical. */
            while((bit_ix    <  DEF_OCTET_NBR_BITS) &&                      /* Calculate number of identical bits ...   */
                  (bit_match == DEF_YES)) {                                 /* ... starting from the MSB of the octet.  */
                bit_clr = DEF_BIT_IS_CLR(octet_diff, DEF_BIT_07);
                if (bit_clr == DEF_YES) {                                   /* If bits are identical .                  */
                    matching_bit_qty++;                                     /* ... inc maching bits ctr.                */
                    octet_diff <<= 1u;
                } else {
                    bit_match    = DEF_NO;                                  /* Return as soon as a bit is different.    */
                }
            }
            octet_match = DEF_NO;
        }
        octet_ix++;
    }

    return (matching_bit_qty);
}


/*
*********************************************************************************************************
*                                      NetIPv6_GetAddrScope()
*
* Description : Get the scope of the given IPv6 address.
*
* Argument(s) : p_addr      Pointer to IPv6 address.
*
* Return(s)   : Scope of the given IPv6 address.
*
* Caller(s)   : Application,
*               NetIPv6_AddrSrcSelect(),
*               NetMLDP_HostGrpJoinHandler(),
*               NetMLDP_HostGrpLeaveHandler(),
*               NetMDLP_RxQuery().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) For an unicast address, the scope is given by the 16 first bits of the address. Three
*                   scopes are possible:
*                       (a) 0xFE80  Link-local
*                       (b) 0xFEC0  Site-local  -> Deprecated
*                       (c) others  Global
*
*               (2) For a mulicast address, the scope is given by a four bits field inside the address.
*                   Current possible scopes are :
*                       (a) 0x0  reserved
*                       (b) 0x1  interface-local
*                       (c) 0x2  link-local
*                       (d) 0x3  reserved
*                       (e) 0x4  admin-local
*                       (f) 0x5  site-local
*                       (g) 0x6  unassigned
*                       (h) 0x7  unassigned
*                       (i) 0x8  organization-local
*                       (j) 0x9  unassigned
*                       (k) 0xA  unassigned
*                       (l) 0xB  unassigned
*                       (m) 0xC  unassigned
*                       (n) 0xD  unassigned
*                       (o) 0xE  global
*                       (p) 0xF  reserved
*********************************************************************************************************
*/

NET_IPv6_SCOPE  NetIPv6_GetAddrScope (const NET_IPv6_ADDR  *p_addr)
{
    CPU_INT08U   scope;
    CPU_BOOLEAN  is_loopback;
    CPU_BOOLEAN  is_unspecified;

                                                                /* ------------------ UNICAST ADDRESS ----------------- */
    if (p_addr->Addr[0] == 0xFE) {
        scope = p_addr->Addr[1] & 0xC0;

        switch (scope) {
            case 0x80:
                 return (NET_IPv6_ADDR_SCOPE_LINK_LOCAL);


            case 0xC0:
                 return (NET_IPv6_ADDR_SCOPE_SITE_LOCAL);


            default:
                return (NET_IPv6_ADDR_SCOPE_GLOBAL);
        }
    }

                                                                /* ----------------- MULITCAST ADDRESS ---------------- */
    if (p_addr->Addr[0] == 0xFF) {
        scope = p_addr->Addr[1] & 0x0F;
        switch (scope) {
            case NET_IPv6_ADDR_SCOPE_RESERVED:
                 return (NET_IPv6_ADDR_SCOPE_RESERVED);


            case NET_IPv6_ADDR_SCOPE_IF_LOCAL:
                 return (NET_IPv6_ADDR_SCOPE_IF_LOCAL);


            case NET_IPv6_ADDR_SCOPE_LINK_LOCAL:
                 return (NET_IPv6_ADDR_SCOPE_LINK_LOCAL);


            case NET_IPv6_ADDR_SCOPE_ADMIN_LOCAL:
                 return (NET_IPv6_ADDR_SCOPE_ADMIN_LOCAL);


            case NET_IPv6_ADDR_SCOPE_SITE_LOCAL:
                 return (NET_IPv6_ADDR_SCOPE_SITE_LOCAL);


            case NET_IPv6_ADDR_SCOPE_ORG_LOCAL:
                 return (NET_IPv6_ADDR_SCOPE_ORG_LOCAL);


            case NET_IPv6_ADDR_SCOPE_GLOBAL:
                 return (NET_IPv6_ADDR_SCOPE_GLOBAL);


            default:
                 return(NET_IPv6_ADDR_SCOPE_GLOBAL);
                 break;
        }
    }

                                                                /* ----------------- LOOPBACK ADDRESS ----------------- */
    is_loopback = NetIPv6_IsAddrLoopback(p_addr);
    if (is_loopback == DEF_YES) {
        return (NET_IPv6_ADDR_SCOPE_LINK_LOCAL);
    }

                                                                /* ---------------- UNSPECIFIED ADDRESS --------------- */
    is_unspecified = NetIPv6_IsAddrUnspecified(p_addr);
    if (is_unspecified == DEF_YES) {
        return (NET_IPv6_ADDR_SCOPE_GLOBAL);
    }

    return (NET_IPv6_ADDR_SCOPE_GLOBAL);

}


/*
*********************************************************************************************************
*                                     NetIPv6_GetAddrsHost()
*
* Description : Get interface number & IPv6 addresses object for configured IPv6 address.
*
* Argument(s) : p_addr       Pointer to configured IPv6 host address to get the interface number & IPv6
*                               addresses structure (see Note #1).
*
*               p_if_nbr     Pointer to variable that will receive ... :
*
*                               (a) The interface number for this configured IPv6 address, if available;
*                               (b) NET_IF_NBR_NONE,                                       otherwise.
*
* Return(s)   : Pointer to corresponding IPv6 address structure, if IPv6 address configured on any interface.
*
*               Pointer to NULL,                                 otherwise.
*
* Caller(s)   : NetIPv6_GetAddrHostIF_Nbr().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrsHost() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*               (2) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

NET_IPv6_ADDRS  *NetIPv6_GetAddrsHost (const  NET_IPv6_ADDR  *p_addr,
                                              NET_IF_NBR     *p_if_nbr)
{
    CPU_BOOLEAN      addr_unspecified;
    NET_IPv6_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;


    if (p_if_nbr != (NET_IF_NBR *)0) {                           /* Init IF nbr for err (see Note #2).                   */
       *p_if_nbr  =  NET_IF_NBR_NONE;
    }

                                                                /* ---------------- VALIDATE IPv6 ADDR ---------------- */
    addr_unspecified = NetIPv6_IsAddrUnspecified(p_addr);
    if (addr_unspecified == DEF_YES) {
        return ((NET_IPv6_ADDRS *)0);
    }


                                                                /* -------- SRCH ALL CFG'D IF's FOR IPv6 ADDR --------- */
    if_nbr     =  NET_IF_NBR_BASE_CFGD;
    p_ip_addrs = (NET_IPv6_ADDRS *)0;

    while ((if_nbr     <   NET_IF_NBR_IF_TOT) &&                /* Srch all cfg'd IF's ...                              */
           (p_ip_addrs == (NET_IPv6_ADDRS *)0)) {               /* ... until addr found.                                */

        p_ip_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_addr);
        if (p_ip_addrs == (NET_IPv6_ADDRS *)0) {                /* If addr NOT found, ...                               */
            if_nbr++;                                           /* ... adv to next IF nbr.                              */
        }
    }

    if (p_ip_addrs != (NET_IPv6_ADDRS *)0) {                    /* If addr avail, ...                                   */
        if (p_if_nbr != (NET_IF_NBR *)0) {
           *p_if_nbr  =  if_nbr;                                 /* ... rtn IF nbr.                                      */
        }
    }

    return (p_ip_addrs);
}


/*
*********************************************************************************************************
*                                      NetIPv6_GetAddrsHostOnIF()
*
* Description : Get IPv6 addresses structure for an interface's configured IPv6 address.
*
* Argument(s) : p_addr      Pointer to configured IPv6 host address to get the interface number & IPv6
*                               addresses structure (see Note #1).
*
*               if_nbr      Interface number to search for configured IPv6 address.
*
* Return(s)   : Pointer to corresponding IP address structure, if IPv6 address configured on this interface.
*
*               Pointer to NULL,                               otherwise.
*
* Caller(s)   : NetIPv6_GetAddrSrcHandler(),
*               NetIPv6_GetAddrsHost(),
*               NetIPv6_RxPktValidate(),
*               NetIPv6_TxPktValidate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrsHostOnIF() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

NET_IPv6_ADDRS  *NetIPv6_GetAddrsHostOnIF (       NET_IF_NBR      if_nbr,
                                           const  NET_IPv6_ADDR  *p_addr)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN        valid;
    NET_ERR            err;
#endif
           NET_IPv6_IF_CFG   *p_ip_if_cfg;
           NET_IPv6_ADDRS    *p_ip_addrs;
    const  NET_IPv6_ADDR     *p_mcast_solicited_addr;
           NET_IP_ADDRS_QTY   addr_ix;
           CPU_BOOLEAN        addr_found;
           CPU_BOOLEAN        addr_found_host;
           CPU_BOOLEAN        addr_found_mcast_solicited;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, &err);
    if (valid != DEF_YES) {
        return ((NET_IPv6_ADDRS *)0);
    }
#endif
                                                                /* -------------- VALIDATE IPv6 ADDR PTR -------------- */
    if (p_addr == (NET_IPv6_ADDR *)0) {
        return ((NET_IPv6_ADDRS *)0);
    }

                                                                /* -------------- SRCH IF FOR IPv6 ADDR --------------- */
    addr_ix                    =  0u;
    addr_found                 =  DEF_NO;
    addr_found_host            =  DEF_NO;
    addr_found_mcast_solicited =  DEF_NO;
    p_ip_if_cfg                = &NetIPv6_IF_CfgTbl[if_nbr];
    p_ip_addrs                 = &p_ip_if_cfg->AddrsTbl[addr_ix];

    while ((addr_ix    <  p_ip_if_cfg->AddrsNbrCfgd) &&         /* Srch all cfg'd addrs ...                             */
           (addr_found == DEF_NO)) {                            /* ... until addr found.                                */

        p_mcast_solicited_addr = p_ip_addrs->AddrMcastSolicitedPtr;

        addr_found_host = Mem_Cmp(p_addr, &p_ip_addrs->AddrHost, NET_IPv6_ADDR_SIZE);
        if (addr_found_host == DEF_NO) {
            addr_found_mcast_solicited = Mem_Cmp(p_addr, p_mcast_solicited_addr, NET_IPv6_ADDR_SIZE);
        }

        addr_found = addr_found_host | addr_found_mcast_solicited;

        if (addr_found == DEF_NO) {                             /* If addr NOT found, ...                               */
            p_ip_addrs++;                                       /* ... adv to IF's next addr.                           */
            addr_ix++;
        }
    }

    if (addr_found != DEF_YES) {
        return ((NET_IPv6_ADDRS *)0);
    }

    return (p_ip_addrs);
}


/*
*********************************************************************************************************
*                                      NetIPv6_GetIF_CfgObj()
*
* Description : Get the pointer to the IPv6 Interface configuration object for a given interface number.
*
* Argument(s) : if_nbr  Interface number of which to get the IPv6 configuration pointer.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_IPv6_ERR_NONE                   IPv6 IF config successfully returned.
*                           NET_IFV6_ERR_INVALID_IF             Invalid network interface number.
*
* Return(s)   : Pointer to IPv6 Interface configuration for specified interface number.
*
* Caller(s)   : NetNDP_RxPrefixAddrsUpdate(),
*               NetCmd_IF_Config().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetIF_CfgObj() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

NET_IPv6_IF_CFG  *NetIPv6_GetIF_CfgObj(NET_IF_NBR   if_nbr,
                                       NET_ERR     *p_err)
{
    NET_IPv6_IF_CFG  *p_ip_if_cfg;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN       valid;


                                                                /* ------------ VALIDATE INTERFACE NUMBER ------------- */

    valid = NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (valid != DEF_YES) {
        return ((NET_IPv6_IF_CFG *)0);
    }
#endif

                                                                /* ------------ GET POINTER TO IPv6 IF CFG ------------ */
    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];


   *p_err = NET_IPv6_ERR_NONE;

    return (p_ip_if_cfg);

}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrHostCfgd()
*
* Description : Validate an IPv6 address as a configured IPv6 host address on an enabled interface.
*
* Argument(s) : p_addr      Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is one of the host's configured IPv6 addresses.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_IsAddrHostCfgd() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv6_IsAddrHostCfgdHandler()  Note #1'.
*
*               (2) NetIPv6_IsAddrHostCfgd() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrHostCfgd (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_host;
    NET_ERR      err;

                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetIPv6_IsAddrHostCfgd, &err);
    if (err != NET_ERR_NONE) {
        goto exit_lock_fault;
    }


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
        goto exit_fail;
    }
#endif


    addr_host = NetIPv6_IsAddrHostCfgdHandler(p_addr);           /* Chk if any cfg'd host addr.                          */
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_host = DEF_FAIL;
#endif

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (addr_host);
}


/*
*********************************************************************************************************
*                                   NetIPv6_IsAddrHostCfgdHandler()
*
* Description : Validate an IPv6 address as a configured IPv6 host address on an enabled interface.
*
* Argument(s) : p_addr       Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is one of the host's configured IPv6 addresses.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv6_CfgAddrAddHandler(),
*               NetIPv6_IsAddrHostCfgd().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_IsAddrHostCfgdHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv6_IsAddrHostCfgd()  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrHostCfgdHandler (const  NET_IPv6_ADDR  *p_addr)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  addr_host;


    if_nbr    =  NetIPv6_GetAddrHostIF_Nbr(p_addr);
    addr_host = (if_nbr != NET_IF_NBR_NONE) ? DEF_YES : DEF_NO;

    return (addr_host);
}


/*
*********************************************************************************************************
*                                   NetIPv6_IsAddrCfgdValidHandler()
*
* Description : Check if IPv6 address configured is valid (can be used).
*
* Argument(s) : p_addr  Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is one of the host's configured IPv6 addresses.*
*               DEF_NO,  otherwise.
*
* Caller(s)   : none.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_IsAddrCfgdValidHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrCfgdValidHandler (const  NET_IPv6_ADDR  *p_addr)
{
    NET_IPv6_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;


    p_ip_addrs = NetIPv6_GetAddrsHost(p_addr, &if_nbr);
    if (p_ip_addrs == DEF_NULL) {
        return (DEF_NO);
    }

    return (p_ip_addrs->IsValid);
}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrsCfgdOnIF()
*
* Description : Check if any IPv6 host address(s) configured on an interface.
*
* Argument(s) : if_nbr      Interface number to check for configured IPv6 host address(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIPv6_IsAddrsCfgdOnIF_Handler() : -
*                               NET_IPv6_ERR_NONE               Configured IPv6 host address(s) availability
*                                                                   successfully returned.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ----- RETURNED BY Net_GlobalLockAcquire() : ------
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, if any IP host address(s) configured on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_IsAddrsCfgdOnIF() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv6_IsAddrsCfgdOnIF_Handler()  Note #1'.
*
*               (2) NetIPv6_IsAddrsCfgdOnIF() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrsCfgdOnIF (NET_IF_NBR   if_nbr,
                                      NET_ERR     *p_err)
{
    CPU_BOOLEAN  addr_avail;

                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetIPv6_IsAddrsCfgdOnIF, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif


    addr_avail = NetIPv6_IsAddrsCfgdOnIF_Handler(if_nbr, p_err);/* Chk IF for any cfg'd host addr(s).                   */
    goto exit_release;


exit_lock_fault:

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_avail = DEF_FAIL;
#endif

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (addr_avail);
}


/*
*********************************************************************************************************
*                                  NetIPv6_IsAddrsCfgdOnIF_Handler()
*
* Description : Check if any IPv6 address(s) configured on an interface.
*
* Argument(s) : if_nbr      Interface number to check for configured IPv6 address(s).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Configured IPv6 host address(s) availability
*                                                                   successfully returned.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : DEF_YES, if any IPv6 host address(s) configured on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv6_IsAddrsCfgdOnIF(),
*               NetMgr_IsAddrsCfgdOnIF().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_IsAddrsCfgdOnIF_Handler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv6_IsAddrsCfgdOnIF()  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrsCfgdOnIF_Handler (NET_IF_NBR   if_nbr,
                                              NET_ERR     *p_err)
{
    NET_IPv6_IF_CFG  *p_ip_if_cfg;
    CPU_BOOLEAN       addr_avail;


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (DEF_NO);
    }
#endif

                                                                /* ------------ CHK CFG'D IPv6 ADDRS AVAIL ------------ */
    p_ip_if_cfg = &NetIPv6_IF_CfgTbl[if_nbr];
    addr_avail = (p_ip_if_cfg->AddrsNbrCfgd > 0) ? DEF_YES : DEF_NO;
   *p_err       =  NET_IPv6_ERR_NONE;


    return (addr_avail);
}


/*
*********************************************************************************************************
*                                      NetIPv6_IsValidAddrHost()
*
* Description : (1) Validate an IPv6 host address :
*
*                   (a) MUST NOT be one of the following :
*
*                       (1) The unspecified address
*                       (2) The loopback    address
*                       (3) A Multicast     address
*
* Argument(s) : p_addr_host      Pointer to IPv6 host address to validate.
*
* Return(s)   : DEF_YES, if IPv6 host address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : various.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (3) See 'net_ipv6.h  IPv6 ADDRESS DEFINES  Notes #2 & #3' for supported IPv6 addresses.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsValidAddrHost (const  NET_IPv6_ADDR  *p_addr_host)
{
    CPU_BOOLEAN  is_unspecified;
    CPU_BOOLEAN  is_loopback;
    CPU_BOOLEAN  is_mcast;


    is_unspecified = NetIPv6_IsAddrUnspecified(p_addr_host);
    if (is_unspecified == DEF_YES) {
        return (DEF_NO);
    }

    is_loopback = NetIPv6_IsAddrLoopback(p_addr_host);
    if (is_loopback == DEF_YES) {
        return (DEF_NO);
    }

    is_mcast = NetIPv6_IsAddrMcast(p_addr_host);
    if (is_mcast == DEF_YES) {
        return (DEF_NO);
    }

    return (DEF_YES);

}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrLinkLocal()
*
* Description : (1) Validate an IPv6 address as a link-local IPv6 address.
*
*                   (a) RFC #4291, Section 2.5.6 specifies that the "Link-Local addresses have the
*                       following format" :
*
*                           +----------+-------------------------+----------------------------+
*                           | 10 bits  |         54 bits         |          64 bits           |
*                           +----------+-------------------------+----------------------------+
*                           |1111111010|           0             |       interface ID         |
*                           +----------+-------------------------+----------------------------+
*
*                   (b) In text representation, it corresponds to the following format:
*
*                       FE80:0000:0000:0000:aaaa.bbbb.cccc.dddd  OR:
*                       FE80::aaaa.bbbb.cccc.dddd                WHERE:
*
*                             aaaa.bbbb.cccc.dddd is the interface ID.
*
* Argument(s) : p_addr       Pointer to IPv6 address to validate
*
* Return(s)   : DEF_YES, if IPv6 address is a link-local IPv6 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrLinkLocal (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_link_local;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_link_local = DEF_NO;
    if ( (p_addr->Addr[0]         == 0xFE)  &&
        ((p_addr->Addr[1] & 0xC0) == 0x80)  &&
         (p_addr->Addr[2] == DEF_BIT_NONE)  &&
         (p_addr->Addr[3] == DEF_BIT_NONE)  &&
         (p_addr->Addr[4] == DEF_BIT_NONE)  &&
         (p_addr->Addr[5] == DEF_BIT_NONE)  &&
         (p_addr->Addr[6] == DEF_BIT_NONE)  &&
         (p_addr->Addr[7] == DEF_BIT_NONE)) {

         addr_link_local = DEF_YES;
    }

    return (addr_link_local);
}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrSiteLocal()
*
* Description : (1) Validate an IPv6 address as a site-local address :
*
*                   (a) RFC #4291, Section 2.5.7 specifies that the "Site-Local addresses have the
*                       following format" :
*
*                           +----------+-------------------------+----------------------------+
*                           | 10 bits  |         54 bits         |          64 bits           |
*                           +----------+-------------------------+----------------------------+
*                           |1111111011|        subnet ID        |       interface ID         |
*                           +----------+-------------------------+----------------------------+
*
*                   (b) In text representation, it corresponds to the following format:
*
*                       FEC0:AAAA:BBBB:CCCC:DDDD:aaaa.bbbb.cccc.dddd  WHERE:
*
*                             AAAA.BBBB.CCCC.DDDD is the subnet    ID AND ...
*                             aaaa.bbbb.cccc.dddd is the interface ID.
*
* Argument(s) : p_addr       Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is a site-local IPv6 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrSiteLocal (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_site_local;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_site_local = DEF_NO;
    if ((p_addr->Addr[0] == 0xFE)  &&
        (p_addr->Addr[1] == 0xC0)) {
        addr_site_local = DEF_YES;
    }

    return (addr_site_local);
}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrMcast()
*
* Description : (1) Validate an IPv6 address as a multicast address :
*
*                   (a) RFC #4291, Section 2.7 specifies that "Multicast addresses have the following
*                       format:
*
*                           +--------+----+----+---------------------------------------------+
*                           |   8    |  4 |  4 |                  112 bits                   |
*                           +------ -+----+----+---------------------------------------------+
*                           |11111111|flgs|scop|                  group ID                   |
*                           +--------+----+----+---------------------------------------------+
*
* Argument(s) : p_addr       Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is a multicast IPv6 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application,
*               NetICMPv6_RxPktValidate(),
*               NetICMPv6_TxMsgErrValidate(),
*               NetIPv6_AddrTypeValidate(),
*               NetIPv6_CfgAddrValidate(),
*               NetIPv6_IsAddrProtocolMulticast().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrMcast (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_mcast;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_mcast = DEF_NO;
    if (p_addr->Addr[0] == 0xFF) {
        addr_mcast = DEF_YES;
    }

    return (addr_mcast);
}


/*
*********************************************************************************************************
*                                    NetIPv6_IsAddrMcastSolNode()
*
* Description : (1) Validate that an IPv6 address is solicited node multicast address.
*
* Argument(s) : p_addr           Pointer to       IPv6 address to validate (see Note #2).
*
*               p_addr_input     Pointer to input IPv6 address             (see Note #3).
*
* Return(s)   : DEF_YES, if IPv6 address is a solicited node multicast IPv6 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) IPv6 address MUST be in host-order.
*
*               (3) RFC #4291 Section 2.7.1 specifies that "Solicited-Node multicast address are computed
*                   as a function of a node's unicast and anycast addresses.  A Solicited-Node multicast
*                   address is formed by taking the low-order 24 bits of an address (unicast or anycast)
*                   and appending those bits to the prefix FF02:0:0:0:0:1:FF00::/104 resulting in a
*                   multicast address in the range" from :
*
*                   (a) FF02:0000:0000:0000:0000:0001:FF00:0000
*
*                        to ...
*
*                   (b) FF02:0000:0000:0000:0000:0001:FFFF:FFFF"
*
*               (4) As the 24 low-order 24 bits of the input address MUST be compared, the length argument
*                   of the Mem_Cmp() function call is 3 bytes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrMcastSolNode (const  NET_IPv6_ADDR  *p_addr,
                                         const  NET_IPv6_ADDR  *p_addr_input)
{
    CPU_BOOLEAN   addr_mcast_sol_node;

                                                                /* --------------- VALIDATE ADDR PTRS ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }

    if (p_addr_input == (NET_IPv6_ADDR *)0) {
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_mcast_sol_node = DEF_NO;
    if ((p_addr->Addr[0]  != 0xFF)         ||                    /* See Note #3.                                         */
        (p_addr->Addr[1]  != 0x02)         ||
        (p_addr->Addr[2]  != DEF_BIT_NONE) ||
        (p_addr->Addr[3]  != DEF_BIT_NONE) ||
        (p_addr->Addr[4]  != DEF_BIT_NONE) ||
        (p_addr->Addr[5]  != DEF_BIT_NONE) ||
        (p_addr->Addr[6]  != DEF_BIT_NONE) ||
        (p_addr->Addr[7]  != DEF_BIT_NONE) ||
        (p_addr->Addr[8]  != DEF_BIT_NONE) ||
        (p_addr->Addr[9]  != DEF_BIT_NONE) ||
        (p_addr->Addr[10] != DEF_BIT_NONE) ||
        (p_addr->Addr[11] != 0x01)         ||
        (p_addr->Addr[12] != 0xFF)){

        return (DEF_NO);
    }
    addr_mcast_sol_node = Mem_Cmp(            &p_addr->Addr[13],
                                              &p_addr_input->Addr[13],
                                  (CPU_SIZE_T) 3u);             /* See Note #4.                                         */

    return (addr_mcast_sol_node);
}


/*
*********************************************************************************************************
*                                    NetIPv6_IsAddrMcastAllNodes()
*
* Description : (1) Validate that an IPv6 address is a multicast to all routers IPv6 nodes :
*
*                   (a) RFC #4291 Section 2.7.1 specifies that the following IPv6 addresses "identify
*                       the group of all IPv6 nodes" within their respective scope :
*
*                       FF01:0000:0000:0000:0000:0000:0000:1    Scope 1 -> Interface-local
*                       FF02:0000:0000:0000:0000:0000:0000:1    Scope 2 -> Link-local
*
*
* Argument(s) : p_addr      Pointer to IPv6 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv6 address is a mutlicast to all nodes IPv6 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application,
*               NetIPv6_AddrTypeValidate().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrMcastAllNodes (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_mcast_all_nodes;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_mcast_all_nodes = DEF_NO;
    if ((p_addr->Addr[0]  != 0xFF)         ||
        (p_addr->Addr[2]  != DEF_BIT_NONE) ||
        (p_addr->Addr[3]  != DEF_BIT_NONE) ||
        (p_addr->Addr[4]  != DEF_BIT_NONE) ||
        (p_addr->Addr[5]  != DEF_BIT_NONE) ||
        (p_addr->Addr[6]  != DEF_BIT_NONE) ||
        (p_addr->Addr[7]  != DEF_BIT_NONE) ||
        (p_addr->Addr[8]  != DEF_BIT_NONE) ||
        (p_addr->Addr[9]  != DEF_BIT_NONE) ||
        (p_addr->Addr[10] != DEF_BIT_NONE) ||
        (p_addr->Addr[11] != DEF_BIT_NONE) ||
        (p_addr->Addr[12] != DEF_BIT_NONE) ||
        (p_addr->Addr[13] != DEF_BIT_NONE) ||
        (p_addr->Addr[14] != DEF_BIT_NONE) ||
        (p_addr->Addr[15] != 0x01)) {

        return (DEF_NO);
    }

    if ((p_addr->Addr[1] == 0x01) ||
        (p_addr->Addr[1] == 0x02)) {
        addr_mcast_all_nodes = DEF_YES;
    }

    return (addr_mcast_all_nodes);
}


/*
*********************************************************************************************************
*                                   NetIPv6_IsAddrMcastAllRouters()
*
* Description : (1) Validate that an IPv6 address is a multicast to all routers IPv6 address :
*
*                   (a) RFC #4291 Section 2.7.1 specifies that the following IPv6 addresses "identify
*                       the group of all IPv6 routers" within their respective scope :
*
*                       (a) FF01:0000:0000:0000:0000:0000:0000:2    Scope 1 -> Interface-local
*                       (b) FF02:0000:0000:0000:0000:0000:0000:2    Scope 2 -> Link-local
*                       (c) FF05:0000:0000:0000:0000:0000:0000:2    Scope 5 -> Site-local
*
*
* Argument(s) : p_addr  Pointer to IPv6 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv6 address is a mutlicast to all routers IPv6 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application,
*               NetIPv6_AddrTypeValidate().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrMcastAllRouters (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_mcast_all_routers;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_mcast_all_routers = DEF_NO;
    if ((p_addr->Addr[0]  != 0xFF)         ||
        (p_addr->Addr[2]  != DEF_BIT_NONE) ||
        (p_addr->Addr[3]  != DEF_BIT_NONE) ||
        (p_addr->Addr[4]  != DEF_BIT_NONE) ||
        (p_addr->Addr[5]  != DEF_BIT_NONE) ||
        (p_addr->Addr[6]  != DEF_BIT_NONE) ||
        (p_addr->Addr[7]  != DEF_BIT_NONE) ||
        (p_addr->Addr[8]  != DEF_BIT_NONE) ||
        (p_addr->Addr[9]  != DEF_BIT_NONE) ||
        (p_addr->Addr[10] != DEF_BIT_NONE) ||
        (p_addr->Addr[11] != DEF_BIT_NONE) ||
        (p_addr->Addr[12] != DEF_BIT_NONE) ||
        (p_addr->Addr[13] != DEF_BIT_NONE) ||
        (p_addr->Addr[14] != DEF_BIT_NONE) ||
        (p_addr->Addr[15] != 0x02)) {

        return (DEF_NO);
    }

    if ((p_addr->Addr[1] == 0x01) ||
        (p_addr->Addr[1] == 0x02) ||
        (p_addr->Addr[1] == 0x05)) {
        addr_mcast_all_routers = DEF_YES;
    }

    return (addr_mcast_all_routers);
}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrMcastRsvd()
*
* Description : (1) Validate that an IPv6 address is a reserved multicast IPv6 address :
*
*                   (a) RFC #4291 Section 2.7.1 specifies that the following addresses "are reserved and
*                       shall never be assigned to any multicast group" :
*
*                           FF00:0:0:0:0:0:0:0
*                           FF01:0:0:0:0:0:0:0
*                           FF02:0:0:0:0:0:0:0
*                           FF03:0:0:0:0:0:0:0
*                           FF04:0:0:0:0:0:0:0
*                           FF05:0:0:0:0:0:0:0
*                           FF06:0:0:0:0:0:0:0
*                           FF07:0:0:0:0:0:0:0
*                           FF08:0:0:0:0:0:0:0
*                           FF09:0:0:0:0:0:0:0
*                           FF0A:0:0:0:0:0:0:0
*                           FF0B:0:0:0:0:0:0:0
*                           FF0C:0:0:0:0:0:0:0
*                           FF0D:0:0:0:0:0:0:0
*                           FF0E:0:0:0:0:0:0:0
*                           FF0F:0:0:0:0:0:0:0
*
*
* Argument(s) : p_addr      Pointer to IPv6 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv6 address is a reserved multicast IPv6 addresss.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrMcastRsvd (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_mcast_rsvd;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_mcast_rsvd = DEF_NO;
    if ((p_addr->Addr[0]  != 0xFF)         ||
        (p_addr->Addr[1]  != DEF_BIT_NONE) ||
        (p_addr->Addr[2]  != DEF_BIT_NONE) ||
        (p_addr->Addr[3]  != DEF_BIT_NONE) ||
        (p_addr->Addr[4]  != DEF_BIT_NONE) ||
        (p_addr->Addr[5]  != DEF_BIT_NONE) ||
        (p_addr->Addr[6]  != DEF_BIT_NONE) ||
        (p_addr->Addr[7]  != DEF_BIT_NONE) ||
        (p_addr->Addr[8]  != DEF_BIT_NONE) ||
        (p_addr->Addr[9]  != DEF_BIT_NONE) ||
        (p_addr->Addr[10] != DEF_BIT_NONE) ||
        (p_addr->Addr[11] != DEF_BIT_NONE) ||
        (p_addr->Addr[12] != DEF_BIT_NONE) ||
        (p_addr->Addr[13] != DEF_BIT_NONE) ||
        (p_addr->Addr[14] != DEF_BIT_NONE) ||
        (p_addr->Addr[15] != DEF_BIT_NONE)) {

        return (DEF_NO);
    }

    if (p_addr->Addr[1]  < NET_IPv6_MCAST_RSVD_ADDR_MAX_VAL) {
        addr_mcast_rsvd = DEF_YES;
    }

    return (addr_mcast_rsvd);
}


/*
*********************************************************************************************************
*                                     NetIPv6_IsAddrUnspecified()
*
* Description : (1) Validate that an IPv6 address is the unspecified IPv6 address :
*
*                   (a) RFC #4291 Section 2.5.2 specifies that the following unicast address "is called
*                       the unspecified address" :
*
*                           0000:0000:0000:0000:0000:0000:0000:0000
*
* Argument(s) : p_addr   Pointer to IPv6 address to validate.
*
* Return(s)   : DEF_YES, if IPv6 address is the unspecified address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Various.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) This address indicates the absence of an address and MUST NOT be assigned to any
*                   physical interface. However, it can be used in some cases in the Source Address field
*                   of IPv6 packets sent by a host before it has learned its own address.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrUnspecified (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_unspecified;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_unspecified = DEF_NO;
    if ((p_addr->Addr[0]  == DEF_BIT_NONE) &&
        (p_addr->Addr[1]  == DEF_BIT_NONE) &&
        (p_addr->Addr[2]  == DEF_BIT_NONE) &&
        (p_addr->Addr[3]  == DEF_BIT_NONE) &&
        (p_addr->Addr[4]  == DEF_BIT_NONE) &&
        (p_addr->Addr[5]  == DEF_BIT_NONE) &&
        (p_addr->Addr[6]  == DEF_BIT_NONE) &&
        (p_addr->Addr[7]  == DEF_BIT_NONE) &&
        (p_addr->Addr[8]  == DEF_BIT_NONE) &&
        (p_addr->Addr[9]  == DEF_BIT_NONE) &&
        (p_addr->Addr[10] == DEF_BIT_NONE) &&
        (p_addr->Addr[11] == DEF_BIT_NONE) &&
        (p_addr->Addr[12] == DEF_BIT_NONE) &&
        (p_addr->Addr[13] == DEF_BIT_NONE) &&
        (p_addr->Addr[14] == DEF_BIT_NONE) &&
        (p_addr->Addr[15] == DEF_BIT_NONE)) {

        addr_unspecified = DEF_YES;
    }

    return (addr_unspecified);
}


/*
*********************************************************************************************************
*                                      NetIPv6_IsAddrLoopback()
*
* Description : (1) Validate that an IPv6 address is the IPv6 loopback address :
*
*                   (a) RFC #4291 Section 2.5.3 specifies that the following unicast address "is called
*                       the loopback address" :
*
*                           0000:0000:0000:0000:0000:0000:0000:0001
*
* Argument(s) : p_addr           Pointer to IPv6 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv6 address is the IPv6 loopback address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application,
*               NetIPv6_AddrTypeValidate().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) This address may be used by a node to send an IPv6 packet to itself and MUST NOT
*                   be assigned to any physical interface.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrLoopback (const  NET_IPv6_ADDR  *p_addr)
{
    CPU_BOOLEAN  addr_loopback;

                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(DEF_NO);
    }
#endif
                                                                /* ------------------ VALIDATE ADDR ------------------- */
    addr_loopback = DEF_NO;
    if ((p_addr->Addr[0]  == DEF_BIT_NONE) &&
        (p_addr->Addr[1]  == DEF_BIT_NONE) &&
        (p_addr->Addr[2]  == DEF_BIT_NONE) &&
        (p_addr->Addr[3]  == DEF_BIT_NONE) &&
        (p_addr->Addr[4]  == DEF_BIT_NONE) &&
        (p_addr->Addr[5]  == DEF_BIT_NONE) &&
        (p_addr->Addr[6]  == DEF_BIT_NONE) &&
        (p_addr->Addr[7]  == DEF_BIT_NONE) &&
        (p_addr->Addr[8]  == DEF_BIT_NONE) &&
        (p_addr->Addr[9]  == DEF_BIT_NONE) &&
        (p_addr->Addr[10] == DEF_BIT_NONE) &&
        (p_addr->Addr[11] == DEF_BIT_NONE) &&
        (p_addr->Addr[12] == DEF_BIT_NONE) &&
        (p_addr->Addr[13] == DEF_BIT_NONE) &&
        (p_addr->Addr[14] == DEF_BIT_NONE) &&
        (p_addr->Addr[15] == 0x01)) {
             addr_loopback = DEF_YES;
    }

    return (addr_loopback);
}


/*
*********************************************************************************************************
*                                       NetIPv6_AddrIsWildcard()
*
* Description : Verify if an NET_IPv6_ADDR address is the IPv6 wildcard address or not.
*
* Argument(s) : p_addr      Pointer to a NET_IPv6_ADDR address.
*
* Return(s)   : DEF_YES, if the address is IPv6 wildcard.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetSock_IsValidAddrLocal().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN NetIPv6_IsAddrWildcard(NET_IPv6_ADDR  *p_addr)
{
    CPU_INT08U    i;
    CPU_BOOLEAN   is_wildcard;
    CPU_INT08U   *p_addr_in;
    CPU_INT08U   *p_addr_wildcard;


    is_wildcard     = DEF_YES;
    p_addr_in       = (CPU_INT08U *) p_addr;
    p_addr_wildcard = (CPU_INT08U *)&NET_IPv6_ADDR_WILDCARD;

    for (i = 0 ; i < NET_IPv6_ADDR_SIZE ; i++) {
        if (*p_addr_in != *p_addr_wildcard) {
            is_wildcard = DEF_NO;
            break;
        }
        p_addr_in++;
        p_addr_wildcard++;
    }

    return (is_wildcard);
}


/*
*********************************************************************************************************
*                                        NetIPv6_IsValidHopLim()
*
* Description : Verify if an NET_IPv6_HOP_LIM value is valid or not.
*
* Argument(s) : hop_lim     The IPv6 hop limit to check the validity.
*
* Return(s)   : DEF_YES, if the IPv6 hop limit is valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetSock_IsValidHopLim().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Hop limit has to be greater than or equal to 1 to be forwarded by a gateway.
*********************************************************************************************************
*/
CPU_BOOLEAN  NetIPv6_IsValidHopLim (NET_IPv6_HOP_LIM  hop_lim)
{
    if (hop_lim < 1) {
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                     NetIPv6_AddrTypeValidate()
*
* Description : Validate the type of an IPv6 address.
*
* Argument(s) : p_addr      Pointer to IPv6 address to validate.
*
*               if_nbr      Interface number associated to the address.
*
* Return(s)   : NET_IPv6_ADDR_TYPE_MCAST,         if IPv6 address is an unknown type muticast     address.
*               NET_IPv6_ADDR_TYPE_MCAST_SOL,     if IPv6 address is the multicast solicited node address.
*               NET_IPv6_ADDR_TYPE_MCAST_ROUTERS, if IPv6 address is the multicast all routers    address.
*               NET_IPv6_ADDR_TYPE_MCAST_NODES,   if IPv6 address is the multicast all nodes      address.
*               NET_IPv6_ADDR_TYPE_LINK_LOCAL,    if IPv6 address is a link-local                 address.
*               NET_IPv6_ADDR_TYPE_SITE_LOCAL,    if IPv6 address is a site-local                 address.
*               NET_IPv6_ADDR_TYPE_UNSPECIFIED,   if IPv6 address is the unspecified              address.
*               NET_IPv6_ADDR_TYPE_LOOPBACK,      if IPv6 address is the loopback                 address.
*               NET_IPv6_ADDR_TYPE_UNICAST,       otherwise.
*
* Caller(s)   : NetIPv6_RxPktValidate().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_IPv6_ADDR_TYPE  NetIPv6_AddrTypeValidate (const  NET_IPv6_ADDR  *p_addr,
                                                     NET_IF_NBR      if_nbr)

{
    CPU_BOOLEAN  valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return(NET_IPv6_ADDR_TYPE_NONE);
    }
#endif

    valid = NetIPv6_IsAddrMcast(p_addr);
    if (valid == DEF_YES) {
        valid = NetIPv6_IsAddrMcastAllRouters(p_addr);
        if (valid == DEF_YES) {
            return (NET_IPv6_ADDR_TYPE_MCAST_ROUTERS);
        }

        valid = NetIPv6_IsAddrMcastAllNodes(p_addr);
        if (valid == DEF_YES) {
            return (NET_IPv6_ADDR_TYPE_MCAST_NODES);
        }

        valid = NetIPv6_IsAddrMcastRsvd(p_addr);
        if (valid == DEF_YES) {
            return (NET_IPv6_ADDR_TYPE_MCAST_RSVD);
        }

        return (NET_IPv6_ADDR_TYPE_MCAST);
    }

    valid = NetIPv6_IsAddrLoopback(p_addr);
    if (valid == DEF_YES) {
        return (NET_IPv6_ADDR_TYPE_LOOPBACK);
    }

    valid = NetIPv6_IsAddrUnspecified(p_addr);
    if (valid == DEF_YES) {
        return (NET_IPv6_ADDR_TYPE_UNSPECIFIED);
    }

    valid = NetIPv6_IsAddrLinkLocal(p_addr);
    if (valid == DEF_YES) {
        return (NET_IPv6_ADDR_TYPE_LINK_LOCAL);
    }

    valid = NetIPv6_IsAddrSiteLocal(p_addr);
    if (valid == DEF_YES) {
        return (NET_IPv6_ADDR_TYPE_SITE_LOCAL);
    }


    (void)&if_nbr;

    return (NET_IPv6_ADDR_TYPE_UNICAST);
}


/*
*********************************************************************************************************
*                                        NetIPv6_CreateIF_ID()
*
* Description : Create an IPv6 interface identifier.
*
* Argument(s) : if_nbr      Network interface number to obtain link-layer hardware address.
*
*               p_addr_id   Pointer to the IPv6 address that will receive the IPv6 interface identifier.
*
*               id_type     IPv6 interface identifier type (see Note #1):
*
*                               NET_IPv6_ADDR_AUTO_CFG_ID_IEEE_48    Universal token from IEEE 802 48-bit MAC
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       Address successfully created.
*                               NET_IPv6_ERR_INVALID_ADDR_ID_TYPE       Invalid address ID type.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*               NetIPv6_AddrAutoCfgHandler().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) Universal interface identifier generated from IEEE 802 48-bit MAC address is the
*                   only interface identifier actually supported.
*
*               (2) RFC #4291, Section 2.5.1 states that:
*
*                   (a) "For all unicast addresses, except those that start with the binary value 000,
*                        Interface IDs are required to be 64 bits long and to be constructed in Modified
*                        EUI-64 format. Modified EUI-64 format-based interface identifiers may have
*                        universal scope when derived from a universal token (e.g., IEEE 802 48-bit MAC
*                         or IEEE EUI-64 identifiers [EUI64]) or may have local scope where a global
*                        token is not available (e.g., serial links, tunnel end-points) or where global
*                        tokens are undesirable (e.g., temporary tokens for privacy [PRIV])."
*
*                   (b) "In the resulting Modified EUI-64 format, the "u" bit is set to one (1) to
*                        indicate universal scope, and it is set to zero (0) to indicate local scope."
*
*               (3) RFC #4291, Appendix A, states that "[EUI-64] actually defines 0xFF and 0xFF as the
*                   bits to be inserted to create an IEEE EUI-64 identifier from an IEEE MAC-48 identifier.
*                   The 0xFF and 0xFE values are used when starting  with an IEEE EUI-48 identifier."
*********************************************************************************************************
*/

CPU_INT08U  NetIPv6_CreateIF_ID (NET_IF_NBR              if_nbr,
                                 NET_IPv6_ADDR          *p_addr_id,
                                 NET_IPv6_ADDR_ID_TYPE   id_type,
                                 NET_ERR                *p_err)
{
    CPU_INT08U  hw_addr[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U  hw_addr_len;


    hw_addr_len = sizeof(hw_addr);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE ADDR PTR ----------------- */
    if (p_addr_id == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
    }
                                                                /* -------------- VALIDATE ADDR ID TYPE --------------- */
    if (id_type != NET_IPv6_ADDR_AUTO_CFG_ID_IEEE_48) {         /* See Note #1.                                         */
        *p_err = NET_IPv6_ERR_INVALID_ADDR_ID_TYPE;
         return (NET_IPv6_ADDR_AUTO_CFG_ID_LEN_NONE);
    }
#endif

    NetIPv6_AddrUnspecifiedSet(p_addr_id, p_err);               /* Clear IF ID.                                         */

    NetIF_AddrHW_GetHandler(if_nbr, hw_addr, &hw_addr_len, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (NET_IPv6_ADDR_AUTO_CFG_ID_LEN_NONE);
    }

    if (hw_addr_len >= 3) {
                                                                /* ------------------- CREATE IF ID ------------------- */
        Mem_Copy((void     *)&p_addr_id->Addr[8],               /* Copy first three octets of IF HW addr after ...      */
                 (void     *)&hw_addr[0],                       /* ... 64 bits of IF ID.                                */
                 (CPU_SIZE_T) 3u);

        p_addr_id->Addr[8]  ^= 0x02;                            /* Set the ID as universal.               See Note #2b. */

        p_addr_id->Addr[11]  = 0xFF;                            /* Insert the two octets between HW addr. See Note #3.  */
        p_addr_id->Addr[12]  = 0xFE;
    }

    if (hw_addr_len >= 6) {
        Mem_Copy((void     *)&p_addr_id->Addr[13],              /* Copy last three octets of IF HW addr after ...       */
                 (void     *)&hw_addr[3],                       /* ... the two inserted octets.                         */
                 (CPU_SIZE_T) 3u);
    }

   *p_err = NET_IPv6_ERR_NONE;

    return (NET_IPv6_ADDR_AUTO_CFG_ID_LEN_IEEE_48);
}


/*
*********************************************************************************************************
*                                     NetIPv6_CreateAddrFromID()
*
* Description : (1) Create an IPv6 addr from a prefix and an identifier.
*
*                   (a) Validate prefix length.
*                   (b) Append address ID to IPv6 address prefix.
*
*
* Argument(s) : p_addr_id       Pointer to IPv6 address ID.
*
*               p_addr_prefix   Pointer to variable that will receive the created IPv6 addr (see Note #2).
*
*               prefix_type     Prefix type:
*
*                                   NET_IPv6_ADDR_PREFIX_CUSTOM         Custom     prefix type
*                                   NET_IPv6_ADDR_PREFIX_LINK_LOCAL     Link-local prefix type
*
*               prefix_len      Prefix len (in bits).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       Address successfully created.
*
*                               NET_ERR_FAULT_NULL_PTR                   Argument(s) passed a NULL pointer.
*                               NET_IPv6_ERR_INVALID_ADDR_PREFIX_TYPE   Invalid prefix type.
*                               NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN    Invalid prefix len.
*
* Return(s)   : none.
*
* Caller(s)   : Application,
*               NetIPv6_AddrAutoCfgHandler().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) If the prefix type is custom, p_addr_prefix SHOULD point on a variable that contain
*                   the prefix address. The address ID will be append to this prefix address. If the
*                   prefix type is link-local, the content of the variable pointed by p_addr_prefix does
*                   not matter and will be overwrited.
*
*               (3) If the prefix type is link-local, prefix addr SHOULD be initialized to the unspecified
*                   IPv6 addr to make sure resulting addr does not have any uninitialized values (i.e. if
*                   any of the lower 8 octets of the ID address are NOT       initialized).
*********************************************************************************************************
*/

void  NetIPv6_CreateAddrFromID (NET_IPv6_ADDR              *p_addr_id,
                                NET_IPv6_ADDR              *p_addr_prefix,
                                NET_IPv6_ADDR_PREFIX_TYPE   prefix_type,
                                CPU_SIZE_T                  prefix_len,
                                NET_ERR                    *p_err)
{
    CPU_SIZE_T  id_len_octets;
    CPU_INT08U  prefix_octets_nbr;
    CPU_INT08U  prefix_octets_ix;
    CPU_INT08U  id_bits_nbr;
    CPU_INT08U  prefix_mask;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE ADDR ID PTR --------------- */
    if (p_addr_id == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                /* ------------- VALIDATE ADDR PREFIX PTR ------------- */
    if (p_addr_prefix == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                /* ------------ VALIDATE ADDR PREFIX TYPE ------------- */
    if ((prefix_type != NET_IPv6_ADDR_PREFIX_CUSTOM)      &&
        (prefix_type != NET_IPv6_ADDR_PREFIX_LINK_LOCAL)) {
       *p_err = NET_IPv6_ERR_INVALID_ADDR_PREFIX_TYPE;
        return;
    }
#endif

    if (prefix_len > NET_IPv6_ADDR_PREFIX_LEN_MAX) {
       *p_err = NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN;
        return;
    }

    if (prefix_type == NET_IPv6_ADDR_PREFIX_LINK_LOCAL) {       /* If addr prefix type is link-local ...                */
        if (prefix_len != NET_IPv6_ADDR_PREFIX_LINK_LOCAL_LEN) {
           *p_err = NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN;
            return;
        }
        NetIPv6_AddrUnspecifiedSet(p_addr_prefix, p_err);       /* ... init addr to unspecified addr. See Note #3.      */
    }

    prefix_octets_nbr = prefix_len / DEF_OCTET_NBR_BITS;        /* Calc nbr of octets that will contain 8 prefix bits.  */
    prefix_octets_ix  = prefix_octets_nbr + 1u;                 /* Set ix of the first octet that might NOT contain ... */
                                                                /* ... 8 prefix bits.                                   */
    id_len_octets     = NET_IPv6_ADDR_SIZE - prefix_octets_ix;  /* Calc nbr of octets that will contain 8 ID bits.      */

    Mem_Copy((void     *)&p_addr_prefix->Addr[prefix_octets_ix],/* Copy ID octets                into prefix addr.      */
             (void     *)&p_addr_id->Addr[prefix_octets_ix],
             (CPU_SIZE_T) id_len_octets);

                                                                /* Calc nbr of remaining ID bits to copy.               */
    id_bits_nbr = DEF_OCTET_NBR_BITS - (prefix_len % DEF_OCTET_NBR_BITS);

    prefix_mask = DEF_OCTET_MASK << id_bits_nbr;                /* Set prefix mask.                                     */

                                                                /* Copy        remaining ID bits into prefix addr.      */
    p_addr_prefix->Addr[prefix_octets_nbr] = ((p_addr_prefix->Addr[prefix_octets_nbr] &  prefix_mask) | \
                                              (p_addr_id->Addr[prefix_octets_nbr]     & ~prefix_mask));

    if (prefix_type == NET_IPv6_ADDR_PREFIX_LINK_LOCAL) {       /* If addr prefix type is link-local ...                */
        p_addr_prefix->Addr[0] = 0xFE;                          /* ... set link-local prefix before the ID.             */
        p_addr_prefix->Addr[1] = 0x80;
        p_addr_prefix->Addr[2] = DEF_BIT_NONE;
        p_addr_prefix->Addr[3] = DEF_BIT_NONE;
        p_addr_prefix->Addr[4] = DEF_BIT_NONE;
        p_addr_prefix->Addr[5] = DEF_BIT_NONE;
        p_addr_prefix->Addr[6] = DEF_BIT_NONE;
        p_addr_prefix->Addr[7] = DEF_BIT_NONE;
    }

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetIPv6_AddrHW_McastSet()
*
* Description : Set the multicast Hardware address.
*
* Argument(s) : p_addr_mac_ascii    Pointer to the Multicast MAC address to configured.
*
*               p_addr              Pointer to IPv6 address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Multicast MAC address configured successfully.
*                               NET_ERR_FAULT_NULL_PTR          Argument(s) passed a NULL pointer.
*                               NET_IPv6_ERR_INVALID_ADDR_DEST  Invalid IPv6 address passed as argument.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_TxPktPrepareFrame().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_AddrHW_McastSet (CPU_INT08U     *p_addr_mac_ascii,
                               NET_IPv6_ADDR  *p_addr,
                               NET_ERR        *p_err)
{
    CPU_BOOLEAN  is_unspecified;


    if (p_addr_mac_ascii == (CPU_INT08U *)0) {
        *p_err = NET_ERR_FAULT_NULL_PTR;
         return;
    }

    is_unspecified = NetIPv6_IsAddrUnspecified(p_addr);
    if (is_unspecified == DEF_YES) {
       *p_err = NET_IPv6_ERR_INVALID_ADDR_DEST;
        return;
    }

    p_addr_mac_ascii[0] = 0x33;
    p_addr_mac_ascii[1] = 0x33;

    Mem_Copy((void     *)&p_addr_mac_ascii[2],
             (void     *)&p_addr->Addr[12],
             (CPU_SIZE_T) 4u);

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIPv6_SetAddrSolicitedMcast()
*
* Description : Set the solicited node multicast address for a given address.
*
* Argument(s) : p_addr_result   Pointer to the solicited node address to create
*
*               p_addr_input    Pointer to the IPv6 address associated with the solicited node address.
*
*               if_nbr          Interface number to get the HW address if necessary.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE       Solicited Node Multicast address successfully configured.
*                               NET_ERR_FAULT_NULL_PTR  Argument(s) passed a NULL pointer.
*
*                                                       -- RETURNED by NetIF_IsValidCfgdHandler() : --
*                               NET_IF_ERR_NONE         Network interface successfully validated.
*                               NET_IF_ERR_INVALID_IF   Invalid configured network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_TxMsgReqHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The solicited node multicast address for a specific IPv6 address is formed with the
*                   prefix FF02:0:0:0:0:1:FF00::/104 and the last 3 bytes of the given IPv6 address.
*
*               (2) If not address is passed as argument, the hardware address of the given Interface is
*                   used to formed the Solicited Node Multicast address.
*********************************************************************************************************
*/

void  NetIPv6_AddrMcastSolicitedSet (NET_IPv6_ADDR  *p_addr_result,
                                     NET_IPv6_ADDR  *p_addr_input,
                                     NET_IF_NBR      if_nbr,
                                     NET_ERR        *p_err)
{
    CPU_INT08U   addr_hw[NET_IF_HW_ADDR_LEN_MAX];
    CPU_INT08U   addr_hw_len;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* --------------- VALIDATE ADDR PTRS ----------------- */
    if (p_addr_result == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err =  NET_ERR_FAULT_NULL_PTR;
    }

    if (p_addr_input  == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err =  NET_ERR_FAULT_NULL_PTR;
    }
#endif

                                                                /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

    p_addr_result->Addr[0]  = 0xFF;
    p_addr_result->Addr[1]  = 0x02;
    p_addr_result->Addr[2]  = DEF_BIT_NONE;
    p_addr_result->Addr[3]  = DEF_BIT_NONE;
    p_addr_result->Addr[4]  = DEF_BIT_NONE;
    p_addr_result->Addr[5]  = DEF_BIT_NONE;
    p_addr_result->Addr[6]  = DEF_BIT_NONE;
    p_addr_result->Addr[7]  = DEF_BIT_NONE;
    p_addr_result->Addr[8]  = DEF_BIT_NONE;
    p_addr_result->Addr[9]  = DEF_BIT_NONE;
    p_addr_result->Addr[10] = DEF_BIT_NONE;
    p_addr_result->Addr[11] = 0x01;
    p_addr_result->Addr[12] = 0xFF;

    if (p_addr_input == (NET_IPv6_ADDR *)0) {                   /* See Note #2.                                         */

        addr_hw_len = NET_IF_HW_ADDR_LEN_MAX;

        NetIF_AddrHW_GetHandler(if_nbr, &addr_hw[0], &addr_hw_len, p_err);
        if (*p_err != NET_IF_ERR_NONE) {
             return;
        }

        Mem_Copy((void     *)&p_addr_result->Addr[13],
                 (void     *)&addr_hw[3],
                 (CPU_SIZE_T) 3u);

       *p_err = NET_IPv6_ERR_NONE;
        return;
    }

    Mem_Copy((void     *)&p_addr_result->Addr[13],              /* See Note #1.                                         */
             (void     *)&p_addr_input->Addr[13],
             (CPU_SIZE_T) 3u);


   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                  NetIPv6_AddrMcastAllRoutersSet()
*
* Description : Set the multicast all-routers IPv6 address.
*
* Argument(s) : p_addr      Pointer to the IPv6 Multicast All-Routers address to configured.
*
*               mldp_v2     DEF_YES, Multicast Listiner Discovery Protocol version 2 is used
*                           DEF_NO,  Multicast Listiner Discovery Protocol version 1 is used
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE       Multicast All-Routers address sucessfully configured.
*                               NET_ERR_FAULT_NULL_PTR  Argument(s) passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetMLDP_TxReport(),
*               NetMLDP_TxDone(),
*               NetNDP_TxRouterSolicitation().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The IPv6 Mulitcast All-Routers address is defined as FF02:0:0:0:0:0:0:2 for the link-local
*                   scope.
*
*               (2) For the MLDv2 protocol, the address FF02:0:0:0:0:0:0:16 is used as the destination
*                   address for sending MDL report messages.
*********************************************************************************************************
*/

void  NetIPv6_AddrMcastAllRoutersSet (NET_IPv6_ADDR  *p_addr,
                                      CPU_BOOLEAN     mldp_v2,
                                      NET_ERR        *p_err)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ADDR PTR ----------------- */
    if (p_addr  == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
    }
#endif
                                                                /* --------------------- SET ADDR --------------------- */
    p_addr->Addr[0]  = 0xFF;
    p_addr->Addr[1]  = 0x02;
    p_addr->Addr[2]  = DEF_BIT_NONE;
    p_addr->Addr[3]  = DEF_BIT_NONE;
    p_addr->Addr[4]  = DEF_BIT_NONE;
    p_addr->Addr[5]  = DEF_BIT_NONE;
    p_addr->Addr[6]  = DEF_BIT_NONE;
    p_addr->Addr[7]  = DEF_BIT_NONE;
    p_addr->Addr[8]  = DEF_BIT_NONE;
    p_addr->Addr[9]  = DEF_BIT_NONE;
    p_addr->Addr[10] = DEF_BIT_NONE;
    p_addr->Addr[11] = DEF_BIT_NONE;
    p_addr->Addr[12] = DEF_BIT_NONE;
    p_addr->Addr[13] = DEF_BIT_NONE;
    p_addr->Addr[14] = DEF_BIT_NONE;

    if (mldp_v2 == DEF_NO) {
        p_addr->Addr[15] = 0x02;
    } else {
        p_addr->Addr[15] = 0x16;
    }

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   NetIPv6_AddrMcastAllNodesSet()
*
* Description : Set the multicast all-nodes IPv6 address.
*
* Argument(s) : p_addr      Pointer to the IPv6 Multicast All-Nodes address to configured.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE       Multicast All-Nodes address sucessfully configured.
*                               NET_ERR_FAULT_NULL_PTR  Argument(s) passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Start().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The IPv6 Multicast All Nodes address is defined as FF02:0:0:0:0:0:0:0:1
*********************************************************************************************************
*/

void  NetIPv6_AddrMcastAllNodesSet (NET_IPv6_ADDR  *p_addr,
                                    NET_ERR        *p_err)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ADDR PTR ----------------- */
    if (p_addr  == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
    }
#endif

    p_addr->Addr[0]  = 0xFF;
    p_addr->Addr[1]  = 0x02;
    p_addr->Addr[2]  = DEF_BIT_NONE;
    p_addr->Addr[3]  = DEF_BIT_NONE;
    p_addr->Addr[4]  = DEF_BIT_NONE;
    p_addr->Addr[5]  = DEF_BIT_NONE;
    p_addr->Addr[6]  = DEF_BIT_NONE;
    p_addr->Addr[7]  = DEF_BIT_NONE;
    p_addr->Addr[8]  = DEF_BIT_NONE;
    p_addr->Addr[9]  = DEF_BIT_NONE;
    p_addr->Addr[10] = DEF_BIT_NONE;
    p_addr->Addr[11] = DEF_BIT_NONE;
    p_addr->Addr[12] = DEF_BIT_NONE;
    p_addr->Addr[13] = DEF_BIT_NONE;
    p_addr->Addr[14] = DEF_BIT_NONE;
    p_addr->Addr[15] = 0x01;

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetIPv6_AddrLoopbackSet()
*
* Description : Set the loopback IPv6 address.
*
* Argument(s) : p_addr      Pointer to the IPv6 Loopback address to configured.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE       IPv6 loopback address sucessfully configured.
*                               NET_ERR_FAULT_NULL_PTR  Argument(s) passed a NULL pointer
*
* Return(s)   : none.
*
* Caller(s)   : ????
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) The IPv6 loopback address is defined as 0:0:0:0:0:0:0:1.
*********************************************************************************************************
*/

void  NetIPv6_AddrLoopbackSet (NET_IPv6_ADDR  *p_addr,
                               NET_ERR        *p_err)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ADDR PTR ----------------- */
    if (p_addr  == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
    }
#endif

    p_addr->Addr[0]  = DEF_BIT_NONE;
    p_addr->Addr[1]  = DEF_BIT_NONE;
    p_addr->Addr[2]  = DEF_BIT_NONE;
    p_addr->Addr[3]  = DEF_BIT_NONE;
    p_addr->Addr[4]  = DEF_BIT_NONE;
    p_addr->Addr[5]  = DEF_BIT_NONE;
    p_addr->Addr[6]  = DEF_BIT_NONE;
    p_addr->Addr[7]  = DEF_BIT_NONE;
    p_addr->Addr[8]  = DEF_BIT_NONE;
    p_addr->Addr[9]  = DEF_BIT_NONE;
    p_addr->Addr[10] = DEF_BIT_NONE;
    p_addr->Addr[11] = DEF_BIT_NONE;
    p_addr->Addr[12] = DEF_BIT_NONE;
    p_addr->Addr[13] = DEF_BIT_NONE;
    p_addr->Addr[14] = DEF_BIT_NONE;
    p_addr->Addr[15] = 0x01;

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIPv6_AddrUnspecifiedSet()
*
* Description : Set the unspecified IPv6 address.
*
* Argument(s) : p_addr      Pointer to the IPv6 Unspecified address to configured.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE       IPv6 unspecified address sucessfully configured.
*                               NET_ERR_FAULT_NULL_PTR  Argument(s) passed a NULL pointer
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_TxMsgReq(),
*               NetIPv6_CfgAddrRemove(),
*               NetIPv6_CfgAddrRemoveAllHandler(),
*               NetIPv6_CreateAddrFromID(),
*               NetIPv6_CreateIF_ID(),
*               NetIPv6_Init(),
*               NetMLDP_TxMsgDone(),
*               NetMLDP_TxReport(),
*               NetNDP_TxNeighborSolicitation().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) The IPv6 Unspecified address is defined as 0:0:0:0:0:0:0:0.
*********************************************************************************************************
*/

void  NetIPv6_AddrUnspecifiedSet (NET_IPv6_ADDR  *p_addr,
                                  NET_ERR        *p_err)
{

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE ADDR PTR ----------------- */
    if (p_addr  == (NET_IPv6_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
    }
#endif

    p_addr->Addr[0]  = DEF_BIT_NONE;
    p_addr->Addr[1]  = DEF_BIT_NONE;
    p_addr->Addr[2]  = DEF_BIT_NONE;
    p_addr->Addr[3]  = DEF_BIT_NONE;
    p_addr->Addr[4]  = DEF_BIT_NONE;
    p_addr->Addr[5]  = DEF_BIT_NONE;
    p_addr->Addr[6]  = DEF_BIT_NONE;
    p_addr->Addr[7]  = DEF_BIT_NONE;
    p_addr->Addr[8]  = DEF_BIT_NONE;
    p_addr->Addr[9]  = DEF_BIT_NONE;
    p_addr->Addr[10] = DEF_BIT_NONE;
    p_addr->Addr[11] = DEF_BIT_NONE;
    p_addr->Addr[12] = DEF_BIT_NONE;
    p_addr->Addr[13] = DEF_BIT_NONE;
    p_addr->Addr[14] = DEF_BIT_NONE;
    p_addr->Addr[15] = DEF_BIT_NONE;

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetIPv6_MaskGet()
*
* Description : Get an IPv6 mask based on prefix length.
*
* Argument(s) : p_mask_rtn  Pointer to IPv6 address mask to set.
*               ----------  Argument validated by caller(s)
*
*               prefix_len  Length of the prefix mask in bits.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                      Mask successfully set.
*                               NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN   Invalid mask length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_AddrMaskByPrefixLen(),
*               NetIPv6_GetAddrHostMatchPrefix(),
*               NetIPv6_IsAddrAndPrefixLenValid().
*
*               This function is an application interface (API) function & MAY be called by application
*               function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_MaskGet (NET_IPv6_ADDR  *p_mask_rtn,
                       CPU_INT08U      prefix_len,
                       NET_ERR        *p_err)
{
    CPU_INT08U  mask;
    CPU_INT08U  index;
    CPU_INT08U  byte_index;
    CPU_INT08U  modulo;


                                                                /* Validate mask length.                                */
    if ((prefix_len == 0u)                         ||
        (prefix_len  > NET_IPv6_ADDR_LEN_NBR_BITS)) {
       *p_err = NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN;
        goto exit;
    }

    index      = 0u;                                            /* Index of bit in IPv6 address.                        */
    byte_index = 0u;                                            /* Number of the current byte in the IPv6 addr.         */
    modulo     = 0u;                                            /* Modulo 8.                                            */

                                                                /* Set mask for given prefix.                           */
    while (index < NET_IPv6_ADDR_LEN_NBR_BITS) {
        modulo = index % DEF_OCTET_NBR_BITS;
        if ((modulo == 0) &&                                    /* Increment the byte index when the modulo is null.    */
            (index  != 0)) {
             byte_index++;
        }

        mask = 1u << modulo;                                    /* Set the bit mask for the current byte.               */

        if (index < prefix_len) {                               /* Apply mask for current address byte.                 */
            DEF_BIT_SET(p_mask_rtn->Addr[byte_index], mask);
        } else {
            DEF_BIT_CLR(p_mask_rtn->Addr[byte_index], mask);
        }

        index++;
    }


   *p_err = NET_IPv6_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                     NetIPv6_AddrMaskByPrefixLen()
*
* Description : Get IPv6 masked with the prefix length.
*
*
* Argument(s) : p_addr      Pointer to IPv6 address.
*               ------      Argument checked by caller(s).
*
*               prefix_len  IPv6 address prefix length.
*
*               p_addr_rtn  Pointer to IPv6 address that will receive the masked address.
*               ----------  Argument validated by caller(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       Address successfully masked.
*
*                               ----------------- RETURNED BY NetIPv6_MaskGet() : ------------------
*                               NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN    Invalid mask length.
*
* Return(s)   : none.
*
* Caller(s)   : NetNDP_PrefixSrchMatchAddr(),
*               NetNDP_RxPrefixHandler().
*
*               This function is an application interface (API) function & MAY be called by application
*               function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_AddrMaskByPrefixLen (const  NET_IPv6_ADDR  *p_addr,
                                          CPU_INT08U      prefix_len,
                                          NET_IPv6_ADDR  *p_addr_rtn,
                                          NET_ERR        *p_err)
{
    NET_IPv6_ADDR  mask;


    NetIPv6_MaskGet(&mask, prefix_len, p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         goto exit;
    }


    NetIPv6_AddrMask(                        p_addr,
                     (const NET_IPv6_ADDR *)&mask,
                                             p_addr_rtn);

   *p_err = NET_IPv6_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                          NetIPv6_AddrMask()
*
* Description : Apply IPv6 mask on an address.
*
* Argument(s) : p_addr      Pointer to IPv6 address to be masked.
*               ------      Argument checked by caller(s).
*
*               p_mask      Pointer to IPv6 address mask.
*               ------      Argument validated by caller(s).
*
*               p_addr_rtn  Pointer to IPv6 address that will received the masked address.
*               ----------  Argument validated by caller(s).
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_AddrMaskByPrefixLen(),
*               NetIPv6_IsAddrAndMaskValid(),
*               NetIPv6_IsAddrAndPrefixLenValid().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_AddrMask (const  NET_IPv6_ADDR   *p_addr,
                        const  NET_IPv6_ADDR   *p_mask,
                               NET_IPv6_ADDR   *p_addr_rtn)
{
    CPU_INT08U  i;


    for (i = 0u; i < NET_IPv6_ADDR_LEN; i++) {
        p_addr_rtn->Addr[i] = p_addr->Addr[i] & p_mask->Addr[i];
    }
}


/*
*********************************************************************************************************
*                                            NetIPv6_Rx()
*
* Description : (1) Process received datagrams & forward to network protocol layers :
*
*                   (a) Validate IPv6 packet & options
*                   (b) Reassemble fragmented datagrams
*                   (c) Demultiplex datagram to higher-layer protocols
*                   (d) Update receive statistics
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IP datagram successfully received & processed.
*
*                                                               ---- RETURNED BY NetIPv6_RxPktDiscard() : ----
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_RxPktDemux(),
*               Network interface receive functions.
*
*               This function is a network protocol suite to network interface (IF) function & SHOULD be
*               called only by ap_propriate network interface function(s).
*
* Note(s)     : (2) Since NetIPv6_RxPktFragReasm() may return a pointer to a different packet buffer (see
*                   'NetIPv6_RxPktFragReasm()  Return(s)', 'p_buf_hdr' MUST be reloaded.
*
*               (3) (a) For single packet buffer IPv6 datagrams, the datagram length is equal to the IPv6
*                       Total Length minus the IPv6 Header Length.
*
*                   (b) For multiple packet buffer, fragmented IPv6 datagrams, the datagram length is
*                       equal to the previously calculated total fragment size.
*
*                       (1) IP datagram length is stored ONLY in the first packet buffer of any
*                           fragmented packet buffers.
*
*               (4) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

void  NetIPv6_Rx (NET_BUF  *p_buf,
                  NET_ERR  *p_err)
{
    NET_BUF_HDR    *p_buf_hdr;
    NET_IPv6_HDR   *p_ip_hdr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIPv6_RxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return;
    }
#endif


    NET_CTR_STAT_INC(Net_StatCtrs.IPv6.RxPktCtr);


                                                                /* -------------- VALIDATE RX'D IPv6 PKT -------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIPv6_RxPktValidateBuf(p_buf_hdr, p_err);                 /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIPv6_RxPktDiscard(p_buf, p_err);
             return;
    }
#endif
    p_ip_hdr = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
    NetIPv6_RxPktValidate(p_buf, p_buf_hdr, p_ip_hdr, p_err);   /* Validate rx'd pkt.                                   */

                                                                /* ----------------- PROCESS EXT HDR ------------------ */
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             NetIPv6_RxPktProcessExtHdr(p_buf, p_err);
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_IPv6_ERR_INVALID_VER:
        case NET_IPv6_ERR_INVALID_LEN_HDR:
        case NET_IPv6_ERR_INVALID_LEN_TOT:
        case NET_IPv6_ERR_INVALID_LEN_DATA:
        case NET_IPv6_ERR_INVALID_FLAG:
        case NET_IPv6_ERR_INVALID_FRAG:
        case NET_IPv6_ERR_INVALID_PROTOCOL:
        case NET_IPv6_ERR_INVALID_CHK_SUM:
        case NET_IPv6_ERR_INVALID_ADDR_SRC:
        case NET_IPv6_ERR_INVALID_ADDR_DEST:
        case NET_IPv6_ERR_RX_OPT_BUF_NONE_AVAIL:
        case NET_IPv6_ERR_RX_OPT_BUF_LEN:
        case NET_IPv6_ERR_RX_OPT_BUF_WR:
        default:
             NetIPv6_RxPktDiscard(p_buf, p_err);
             return;
    }

                                                                /* ------------------- REASM FRAGS -------------------- */
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             p_buf = NetIPv6_RxPktFragReasm(p_buf, p_buf_hdr, p_ip_hdr, p_err);
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_IPv6_ERR_INVALID_VER:
        case NET_IPv6_ERR_INVALID_LEN_HDR:
        case NET_IPv6_ERR_INVALID_LEN_TOT:
        case NET_IPv6_ERR_INVALID_LEN_DATA:
        case NET_IPv6_ERR_INVALID_FLAG:
        case NET_IPv6_ERR_INVALID_FRAG:
        case NET_IPv6_ERR_INVALID_PROTOCOL:
        case NET_IPv6_ERR_INVALID_CHK_SUM:
        case NET_IPv6_ERR_INVALID_ADDR_SRC:
        case NET_IPv6_ERR_INVALID_ADDR_DEST:
        case NET_IPv6_ERR_RX_OPT_BUF_NONE_AVAIL:
        case NET_IPv6_ERR_RX_OPT_BUF_LEN:
        case NET_IPv6_ERR_RX_OPT_BUF_WR:
        default:
             NetIPv6_RxPktDiscard(p_buf, p_err);
             return;
    }



                                                                          /* ------------- DEMUX DATAGRAM ------------- */
    switch (*p_err) {                                                     /* Chk err from NetIPv6_RxPktFragReasm().     */
        case NET_IPv6_ERR_RX_FRAG_NONE:
        case NET_IPv6_ERR_RX_FRAG_COMPLETE:
             p_buf_hdr = &p_buf->Hdr;                                     /* Reload buf hdr ptr (see Note #3).          */
             if (*p_err == NET_IPv6_ERR_RX_FRAG_NONE) {                   /* If pkt NOT frag'd, ...                     */
                  p_buf_hdr->IP_DatagramLen = (p_buf_hdr->IP_TotLen - p_buf_hdr->IPv6_ExtHdrLen);
                                                                          /* ... calc buf datagram len (see Note #4a).  */
             } else {                                                     /* Else set tot frag size ...                 */
                  p_buf_hdr->IP_DatagramLen = p_buf_hdr->IP_FragSizeTot;  /* ...       as datagram len (see Note #4b).  */
             }
             NetIPv6_RxPktDemuxDatagram(p_buf, p_buf_hdr, p_err);
             break;


        case NET_IPv6_ERR_RX_FRAG_REASM:                        /* Frag'd datagram in reasm.                            */
            *p_err = NET_IPv6_ERR_NONE;
             return;


        case NET_IPv6_ERR_RX_FRAG_DISCARD:
        case NET_IPv6_ERR_RX_FRAG_OFFSET:
        case NET_IPv6_ERR_RX_FRAG_SIZE:
        case NET_IPv6_ERR_RX_FRAG_SIZE_TOT:
        case NET_IPv6_ERR_RX_FRAG_LEN_TOT:
        default:
             NetIPv6_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* ----------------- UPDATE RX STATS ------------------ */
    switch (*p_err) {                                           /* Chk err from NetIPv6_RxPktDemuxDatagram().           */
        case NET_ICMPv6_ERR_NONE:
        case NET_IGMP_ERR_NONE:
        case NET_UDP_ERR_NONE:
        case NET_TCP_ERR_NONE:
        case NET_NDP_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IPv6.RxDgramCompCtr);
            *p_err = NET_IPv6_ERR_NONE;
             break;


        case NET_ERR_RX:
                                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxPktDisCtr);
                                                                /* Rtn err from NetIPv6_RxPktDemuxDatagram().           */
             return;


        case NET_ERR_INVALID_PROTOCOL:
        default:
             NetIPv6_RxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                            NetIPv6_Tx()
*
* Description : (1) Prepare & transmit IPv6 datagram packet(s) :
*
*                   (a) Validate  transmit packet
*                   (b) Prepare & transmit packet datagram
*                   (c) Update    transmit statistics
*
*
* Argument(s) : p_buf           Pointer to network buffer to transmit IPv6 packet.
*
*               p_addr_src      Pointer to source        IPv6 address.
*
*               p_addr_dest     Pointer to destination   IPv6 address.
*
*               traffic_class   Specific traffic class to transmit IPv6 packet (see Note #2a).
*
*               flow_label      Specific flow label    to transmit IPv6 packet (see Note #x).
*
*               next_hdr        Next header            to transmit IPv6 packet (see Note #x).
*
*               hop_limit       Specific hop limit to transmit IPv6 packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IPv6 datagram(s)     successfully prepared &
*                                                                   transmitted to network interface layer.
*                               NET_IPv6_ERR_TX_PKT             IPv6 datagram(s) NOT successfully prepared or
*                                                                   transmitted to network interface layer.
*
*                                                               -- RETURNED BY NetIPv6_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               ------ RETURNED BY NetIPv6_TxPkt() : ------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_TxMsgErr(),
*               NetICMPv6_TxMsgReq(),
*               NetICMPv6_TxMsgReply(),
*               NetUDP_Tx(),
*               NetTCP_TxPkt().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

void  NetIPv6_Tx (NET_BUF                 *p_buf,
                  NET_IPv6_ADDR           *p_addr_src,
                  NET_IPv6_ADDR           *p_addr_dest,
                  NET_IPv6_EXT_HDR        *p_ext_hdr_list,
                  NET_IPv6_TRAFFIC_CLASS   traffic_class,
                  NET_IPv6_FLOW_LABEL      flow_label,
                  NET_IPv6_HOP_LIM         hop_lim,
                  NET_ERR                 *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_ERR       err;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIPv6_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return;
    }
#endif


                                                                /* --------------- VALIDATE IPv6 TX PKT --------------- */
    p_buf_hdr = &p_buf->Hdr;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    NetIPv6_TxPktValidate(p_buf_hdr,
                          p_addr_src,
                          p_addr_dest,
                          traffic_class,
                          flow_label,
                          hop_lim,
                          p_err);
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_IF_ERR_INVALID_IF:
        case NET_IPv6_ERR_INVALID_LEN_DATA:
        case NET_IPv6_ERR_INVALID_TOS:
        case NET_IPv6_ERR_INVALID_FLAG:
        case NET_IPv6_ERR_INVALID_HOP_LIMIT:
        case NET_IPv6_ERR_INVALID_ADDR_SRC:
        case NET_IPv6_ERR_INVALID_ADDR_DEST:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIPv6_TxPktDiscard(p_buf, &err);
            *p_err = NET_IPv6_ERR_TX_PKT;
             return;
    }
#endif



                                                                /* ------------------- TX IPv6 PKT -------------------- */
    NetIPv6_TxPkt(p_buf,
                  p_buf_hdr,
                  p_addr_src,
                  p_addr_dest,
                  p_ext_hdr_list,
                  traffic_class,
                  flow_label,
                  hop_lim,
                  p_err);


                                                                /* ----------------- UPDATE TX STATS ------------------ */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IPv6.TxDgramCtr);
            *p_err = NET_IPv6_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxPktDisCtr);
                                                                /* Rtn err from NetIPv6_TxPkt().                        */
             return;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_IF_ERR_INVALID_IF:
        case NET_IF_ERR_INVALID_CFG:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_IPv6_ERR_INVALID_LEN_HDR:
        case NET_IPv6_ERR_INVALID_FRAG:
        case NET_IPv6_ERR_INVALID_ADDR_HOST:
        case NET_IPv6_ERR_TX_DEST_INVALID:
        case NET_BUF_ERR_INVALID_IX:
        case NET_BUF_ERR_INVALID_LEN:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_UTIL_ERR_NULL_SIZE:
             NetIPv6_TxPktDiscard(p_buf, &err);
            *p_err = NET_IPv6_ERR_TX_PKT;
             return;


        default:
             NetIPv6_TxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                           NetIPv6_ReTx()
*
* Description : (1) Prepare & re-transmit packets from transport protocol layers to network interface layer :
*
*                   (a) Validate  re-transmit packet
*                   (b) Prepare & re-transmit packet datagram
*                   (c) Update    re-transmit statistics
*
*
* Argument(s) : p_buf       Pointer to network buffer to re-transmit IPv6 packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IPv6 datagram(s) successfully re-transmitted
*                                                                   to network interface layer.
*
*                                                               -- RETURNED BY NetIPv6_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               ----- RETURNED BY NetIPv6_ReTxPkt() : -----
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetTCP_TxConnReTxQ().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

void  NetIPv6_ReTx (NET_BUF  *p_buf,
                    NET_ERR  *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIPv6_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return;
    }
#endif


                                                                /* ------------- VALIDATE IPv6 RE-TX PKT -------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf_hdr->IP_HdrIx == NET_BUF_IX_NONE) {
        NetIPv6_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvBufIxCtr);
        return;
    }
#endif


                                                                /* ------------------ RE-TX IPv6 PKT ------------------ */
    NetIPv6_ReTxPkt(p_buf,
                    p_buf_hdr,
                    p_err);


                                                                /* ---------------- UPDATE RE-TX STATS ---------------- */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IPv6.TxDgramCtr);
            *p_err = NET_IPv6_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #2.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxPktDisCtr);
                                                                /* Rtn err from NetIPv6_ReTxPkt().                      */
             return;


        case NET_IPv6_ERR_INVALID_ADDR_HOST:
        case NET_IPv6_ERR_TX_DEST_INVALID:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_UTIL_ERR_NULL_SIZE:
        default:
             NetIPv6_TxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                         NetIPv6_GetTxDataIx()
*
* Description : Get the offset of a buffer at which the IPv6 data can be written.
*
* Argument(s) : if_nbr      Network interface number to transmit data.
*
*               data_len    IPv6 payload size.
*
*               mtu         MTU for the upper-layer protocol.
*
*               p_ix        Pointer to the current protocol index.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No errors.
*                               NET_IPv6_ERR_FAULT              Covers errors return by NetIF_GetTxDataIx().
*                                                               See NetIF_GetTxDataIx() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv6_TxMsgReqHander(),
*               NetICMPv6_GetTxDataIx(),
*               NetTCP_GetTxDataIx(),
*               NetUDP_GetTxDataIx().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_GetTxDataIx (NET_IF_NBR         if_nbr,
                           NET_IPv6_EXT_HDR  *p_ext_hdr_list,
                           CPU_INT32U         data_len,
                           CPU_INT16U         mtu,
                           CPU_INT16U        *p_ix,
                           NET_ERR           *p_err)
{
    NET_IPv6_EXT_HDR  *p_ext_hdr;

                                                                /* Add IPv6 min hdr len to current offset.              */
   *p_ix += NET_IPv6_HDR_SIZE;

#if 0
    if (data_len > mtu) {
       *p_ix += NET_IPv6_FRAG_HDR_SIZE;                         /* Add IPv6 frag ext   hdr len if frag is req'd.        */
    }
#endif
                                                                /* Add Size of existing extension headers.              */
    p_ext_hdr = p_ext_hdr_list;
    while (p_ext_hdr != (NET_IPv6_EXT_HDR *)0) {
       *p_ix      += p_ext_hdr->Len;
        p_ext_hdr  = p_ext_hdr->NextHdrPtr;
    }

                                                                /* Add the lower-level hdr        offsets.              */
    NetIF_TxIxDataGet(if_nbr, data_len, p_ix, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
            *p_err = NET_IPv6_ERR_NONE;
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_IF_ERR_INVALID_PROTOCOL:
        default:
            *p_err = NET_IPv6_ERR_FAULT;
             break;
    }

    (void)&mtu;
}



/*
*********************************************************************************************************
*                                     NetIPv6_AddExtHdrToList()
*
* Description : Add an Extension Header object into its right place in the extension headers List.
*
* Argument(s) : p_ext_hdr_head  Pointer on the current top of the extension header list.
*
*               p_ext_hdr       Pointer to the extension header object to add to the list.
*
*               type            Type of the extension header.
*
*               len             Length of the extension header.
*
*               fnct            Pointer to callback function that will be call to fill the ext hdr.
*
*               sort_key        Key to sort the extension header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No error.
*                               NET_IPv6_ERR_INVALID_EH         Invalid extension header to add.
*
* Return(s)   : Pointer to the new extension header list head.
*
* Caller(s)   : NetICMPv6_TxMsgReply(),
*               NetICMPv6_TxMsgReqHandler(),
*               NetMLDP_TxReport()
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_IPv6_EXT_HDR  *NetIPv6_ExtHdrAddToList(NET_IPv6_EXT_HDR   *p_ext_hdr_head,
                                           NET_IPv6_EXT_HDR   *p_ext_hdr,
                                           CPU_INT08U          type,
                                           CPU_INT16U          len,
                                           CPU_FNCT_PTR        fnct,
                                           CPU_INT08U          sort_key,
                                           NET_ERR            *p_err)
{
    NET_IPv6_EXT_HDR  *p_list_head;
    NET_IPv6_EXT_HDR  *p_ext_hdr_prev;
    NET_IPv6_EXT_HDR  *p_ext_hdr_next;
    NET_IPv6_EXT_HDR  *p_ext_hdr_item;
    NET_IPv6_EXT_HDR  *p_ext_hdr_cache;


                                                                /* -------------- VALIDATE EXT HDR TYPE --------------- */
    switch (sort_key) {
        case NET_IPv6_EXT_HDR_KEY_HOP_BY_HOP:
        case NET_IPv6_EXT_HDR_KEY_DEST_01:
        case NET_IPv6_EXT_HDR_KEY_ROUTING:
        case NET_IPv6_EXT_HDR_KEY_FRAG:
             break;

        case NET_IPv6_EXT_HDR_KEY_AUTH:
        case NET_IPv6_EXT_HDR_KEY_ESP:
        case NET_IPv6_EXT_HDR_KEY_DEST_02:
            *p_err = NET_IPv6_ERR_INVALID_EH;
             return (p_ext_hdr_head);
    }


                                                                /* --------------- SET EXT HEADER VALUES -------------- */
    p_ext_hdr->Type    =         type;
    p_ext_hdr->Len     =         len;
    p_ext_hdr->SortKey =         sort_key;
    p_ext_hdr->Fnct    = (void *)fnct;
    p_ext_hdr->Arg     =         0;

                                                                /* ----------- ADD EXT HDR TO LIST IN ORDER ----------- */
    if (p_ext_hdr_head == (NET_IPv6_EXT_HDR *)0) {              /* List is empty, ...                                   */
        p_ext_hdr->NextHdrPtr = (NET_IPv6_EXT_HDR *)0;          /* ... add ext hdr at top of list.                      */
        p_ext_hdr->PrevHdrPtr = (NET_IPv6_EXT_HDR *)0;
        p_list_head           = p_ext_hdr;
    } else {

        p_ext_hdr_item = p_ext_hdr_head;
        while (p_ext_hdr_item != (NET_IPv6_EXT_HDR *)0) {       /* Goto the list to find the right place for the hdr:   */

            if (p_ext_hdr_item->SortKey > sort_key) {
                break;
            } else if (p_ext_hdr_item->SortKey == sort_key) {   /* Ext hdr type is already in list...                   */
               *p_err = NET_IPv6_ERR_INVALID_EH;                /* ... return with error.                               */
                return (p_ext_hdr_head);
            } else {
                                                                /* Empty Else Statement                                 */
            }

            p_ext_hdr_cache = p_ext_hdr_item;
            p_ext_hdr_item  = p_ext_hdr_item->NextHdrPtr;
        }

        if (p_ext_hdr_item == (NET_IPv6_EXT_HDR *)0) {          /* Add ext hdr blk to end of list.                      */

            p_ext_hdr_item = p_ext_hdr_cache;
            p_ext_hdr_item->NextHdrPtr = p_ext_hdr;
            p_ext_hdr->PrevHdrPtr = p_ext_hdr_item;
            p_ext_hdr->NextHdrPtr = (NET_IPv6_EXT_HDR *)0;
            p_list_head = p_ext_hdr_head;

        } else {                                                /* Add ext hdr blk inside the list.                     */

            p_ext_hdr_prev = p_ext_hdr_item->PrevHdrPtr;
            p_ext_hdr_next = p_ext_hdr_item;

            if (p_ext_hdr_prev != (NET_IPv6_EXT_HDR *)0) {
                p_ext_hdr_prev->NextHdrPtr = p_ext_hdr;
                p_list_head                = p_ext_hdr_head;
            } else {
                p_list_head = p_ext_hdr;                        /* Set new list head if hdr blk is added at the top.    */
            }
            p_ext_hdr->PrevHdrPtr      = p_ext_hdr_prev;
            p_ext_hdr->NextHdrPtr      = p_ext_hdr_next;
            p_ext_hdr_next->PrevHdrPtr = p_ext_hdr;

        }

    }

   *p_err = NET_IPv6_ERR_NONE;

    return (p_list_head);
}


/*
*********************************************************************************************************
*                                     NetIPv6_PrepareFragHdr()
*
* Description : Callback function to fill up the fragment header inside the packet to be send.
*
* Argument(s) : p_ext_hdr_arg   Pointer to the function arguments.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPktPrepareExtHdr().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_PrepareFragHdr(void  *p_ext_hdr_arg)
{
    NET_IPv6_FRAG_HDR          *p_frag_hdr;
    NET_BUF                    *p_buf;
    NET_IPv6_EXT_HDR_ARG_FRAG  *p_frag_hdr_arg;
    CPU_INT16U                  frag_hdr_ix;


    p_frag_hdr_arg = (NET_IPv6_EXT_HDR_ARG_FRAG *) p_ext_hdr_arg;

    p_buf       = p_frag_hdr_arg->BufPtr;
    frag_hdr_ix = p_frag_hdr_arg->BufIx;


    p_frag_hdr = (NET_IPv6_FRAG_HDR *)&p_buf->DataPtr[frag_hdr_ix];

    Mem_Clr(p_frag_hdr, NET_IPv6_FRAG_HDR_SIZE);

    p_frag_hdr->NextHdr        = p_frag_hdr_arg->NextHdr;
    p_frag_hdr->FragOffsetFlag = p_frag_hdr_arg->FragOffset;
    p_frag_hdr->ID             = p_frag_hdr_arg->FragID;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                 NETWORK MANAGER INTERFACE FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                    NetIPv6_GetHostAddrProtocol()
*
* Description : Get an interface's IPv6 protocol address(s) [see Note #1].
*
* Argument(s) : if_nbr                      Interface number to get IPv6 protocol address(s).
*
*               p_addr_protocol_tbl         Pointer to a protocol address table that will receive the protocol
*                                               address(s) in network-order for this interface.
*
*               p_addr_protocol_tbl_qty     Pointer to a variable to ... :
*
*                                               (a) Pass the size of the protocol address table, in number of
*                                                       protocol address(s), pointed to by 'p_addr_protocol_tbl'.
*                                               (b) (1) Return the actual number of IPv6 protocol address(s),
*                                                           if NO error(s);
*                                                   (2) Return 0, otherwise.
*
*                                           See also Note #1a.
*
*               p_addr_protocol_len         Pointer to a variable to ... :
*
*                                               (a) Pass the length of the protocol address table address(s),
*                                                       in octets.
*                                               (b) (1) Return the actual length of IPv6 protocol address(s),
*                                                           in octets, if NO error(s);
*                                                   (2) Return 0, otherwise.
*
*                                           See also Note #1b.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                   Interface's IPv6 protocol address(s)
*                                                                       successfully returned.
*                               NET_ERR_FAULT_NULL_PTR               Argument(s) passed a NULL pointer.
*                               NET_IPv6_ERR_INVALID_ADDR_LEN       Invalid protocol address length.
*                               NET_IPv6_ERR_ADDR_TBL_SIZE          Invalid protocol address table size.
*
*                                                                   - RETURNED BY NetIPv6_GetAddrHostHandler() : -
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_IP_ERR_ADDR_CFG_IN_PROGRESS     Interface in dynamic address configuration
*                                                                       initialization state.
*
* Return(s)   : none.
*
* Caller(s)   : NetMgr_GetHostAddrProtocol().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) Since 'p_addr_protocol_tbl_qty' argument is both an input & output argument
*                       (see 'Argument(s) : p_addr_protocol_tbl_qty'), ... :
*
*                       (1) Its input value SHOULD be validated prior to use; ...
*
*                           (A) In the case that the 'p_addr_protocol_tbl_qty' argument is passed a null
*                               pointer, NO input value is validated or used.
*
*                           (B) The protocol address table's size MUST be greater than or equal to each
*                               interface's maximum number of IPv6 protocol addresses times the size of
*                               an IPv6 protocol address :
*
*                               (1) (p_addr_protocol_tbl_qty *  >=  [ NET_IPv6_CFG_IF_MAX_NBR_ADDR *
*                                    p_addr_protocol_len    )         NET_IPv6_ADDR_SIZE      ]
*
*                       (2) While its output value MUST be initially configured to return a default value
*                           PRIOR to all other validation or function handling in case of any error(s).
*
*                   (b) Since 'p_addr_protocol_len' argument is both an input & output argument
*                       (see 'Argument(s) : p_addr_protocol_len'), ... :
*
*                       (1) Its input value SHOULD be validated prior to use; ...
*
*                           (A) In the case that the 'p_addr_protocol_len' argument is passed a null pointer,
*                               NO input value is validated or used.
*
*                           (B) The table's protocol address(s) length SHOULD be greater than or equal to the
*                               size of an IPv6 protocol address :
*
*                               (1) p_addr_protocol_len  >=  NET_IPv6_ADDR_SIZE
*
*                       (2) While its output value MUST be initially configured to return a default value
*                           PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/

void  NetIPv6_GetHostAddrProtocol (NET_IF_NBR   if_nbr,
                                   CPU_INT08U  *p_addr_protocol_tbl,
                                   CPU_INT08U  *p_addr_protocol_tbl_qty,
                                   CPU_INT08U  *p_addr_protocol_len,
                                   NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT16U         addr_protocol_tbl_size;
    CPU_INT16U         addr_ip_tbl_size;
    CPU_INT08U         addr_protocol_tbl_qty;
    CPU_INT08U         addr_protocol_len;
#endif
    CPU_INT08U        *p_addr_protocol;
    NET_IPv6_ADDR     *p_addr_ip;
    NET_IPv6_ADDR      addr_ip_tbl[NET_IPv6_CFG_IF_MAX_NBR_ADDR];
    NET_IP_ADDRS_QTY   addr_ip_tbl_qty;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_INT08U         addr_ip_len;


                                                                    /* ---------- VALIDATE PROTOCOL ADDR TBL ---------- */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol_tbl_qty == (CPU_INT08U *)0) {               /* See Note #1a1A.                                  */
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    addr_protocol_tbl_qty = *p_addr_protocol_tbl_qty;
#endif
   *p_addr_protocol_tbl_qty =  0u;                                   /* Cfg dflt tbl qty for err  (see Note #2a2).       */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol_len == (CPU_INT08U *)0) {                    /* See Note #1b1A.                                  */
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

     addr_protocol_len = *p_addr_protocol_len;
#endif
   *p_addr_protocol_len =  0u;                                       /* Cfg dflt addr len for err (see Note #2b2).       */
    addr_ip_len        =  NET_IPv6_ADDR_SIZE;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol_tbl == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (addr_protocol_len < addr_ip_len) {                          /* Validate protocol addr len      (see Note #2b1B).*/
       *p_err = NET_IPv6_ERR_INVALID_ADDR_LEN;
        return;
    }

    addr_protocol_tbl_size = addr_protocol_tbl_qty        * addr_protocol_len;
    addr_ip_tbl_size       = NET_IPv6_CFG_IF_MAX_NBR_ADDR * addr_ip_len;
    if (addr_protocol_tbl_size < addr_ip_tbl_size) {                /* Validate protocol addr tbl size (see Note #2a1B).*/
       *p_err = NET_IPv6_ERR_ADDR_TBL_SIZE;
        return;
    }
#endif


                                                                    /* ----------- GET IPv6 PROTOCOL ADDRS ------------ */
    addr_ip_tbl_qty = sizeof(addr_ip_tbl) / NET_IPv6_ADDR_SIZE;
   (void)NetIPv6_GetAddrHostHandler(if_nbr,
                                   &addr_ip_tbl[0],
                                   &addr_ip_tbl_qty,
                                    p_err);
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
        case NET_IPv6_ERR_ADDR_NONE_AVAIL:
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
        case NET_IPv6_ERR_ADDR_TBL_SIZE:
        default:
             return;
    }

    addr_ix         =  0u;
    p_addr_ip       = &addr_ip_tbl[addr_ix];
    p_addr_protocol = &p_addr_protocol_tbl[addr_ix];
    while (addr_ix <  addr_ip_tbl_qty) {                            /* Rtn all IPv6 protocol addr(s) ...                */
        NET_UTIL_VAL_COPY_SET_NET_32(p_addr_protocol, p_addr_ip);   /* ... in net-order (see Note #1).                  */
        p_addr_protocol += addr_ip_len;
        p_addr_ip++;
        addr_ix++;
    }

                                                                    /* Rtn nbr & len of IPv6 protocol addr(s).          */
   *p_addr_protocol_tbl_qty = (CPU_INT08U)addr_ip_tbl_qty;
   *p_addr_protocol_len     = (CPU_INT08U)addr_ip_len;


   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   NetIPv6_GetAddrProtocolIF_Nbr()
*
* Description : (1) Get the interface number for a host's IPv6 protocol address :
*
*                   (a) A configured IPv6 host address (on an enabled interface)
*
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #2).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IPv6 protocol address's interface number
*                                                                   successfully returned.
*                               NET_ERR_FAULT_NULL_PTR           Argument 'p_addr_protocol' passed a NULL
*                                                                   pointer.
*                               NET_IPv6_ERR_INVALID_ADDR_LEN   Invalid IPv6 protocol address length.
*                               NET_IPv6_ERR_INVALID_ADDR_HOST  IPv6 protocol address NOT used by host.
*
* Return(s)   : Interface number for IPv6 protocol address, if configured on this host.
*
*               Interface number of  IPv6 protocol address
*                   in address initialization,              if available.
*
*               NET_IF_NBR_LOCAL_HOST,                      for a localhost address.
*
*               NET_IF_NBR_NONE,                            otherwise.
*
* Caller(s)   : NetMgr_GetHostAddrProtocolIF_Nbr().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_IF_NBR  NetIPv6_GetAddrProtocolIF_Nbr (CPU_INT08U  *p_addr_protocol,
                                           CPU_INT08U   addr_protocol_len,
                                           NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv6_ADDR  addr_ip;
    NET_IF_NBR     if_nbr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (NET_IF_NBR_NONE);
    }

    addr_ip_len = NET_IPv6_ADDR_SIZE;
    if (addr_protocol_len != addr_ip_len) {
       *p_err =  NET_IPv6_ERR_INVALID_ADDR_LEN;
        return (NET_IF_NBR_NONE);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* ------- GET IPv6 PROTOCOL ADDR's IF NBR -------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&addr_ip, p_addr_protocol);
    if_nbr =  NetIPv6_GetAddrHostIF_Nbr(&addr_ip);
   *p_err   = (if_nbr != NET_IF_NBR_NONE) ? NET_IPv6_ERR_NONE
                                         : NET_IPv6_ERR_INVALID_ADDR_HOST;

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                    NetIPv6_IsValidAddrProtocol()
*
* Description : Validate an IPv6 protocol address.
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if IPv6 protocol address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsValidAddrProtocol().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) IPv6 protocol address MUST be in network-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsValidAddrProtocol (CPU_INT08U  *p_addr_protocol,
                                          CPU_INT08U   addr_protocol_len)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv6_ADDR  addr_ip;
    CPU_BOOLEAN    valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        return (DEF_NO);
    }

    addr_ip_len = NET_IPv6_ADDR_SIZE;
    if (addr_protocol_len != addr_ip_len) {
        return (DEF_NO);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* --------- VALIDATE IPv6 PROTOCOL ADDR ---------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&addr_ip, p_addr_protocol);
    valid = NetIPv6_IsValidAddrHost(&addr_ip);

    return (valid);
}


/*
*********************************************************************************************************
*                                        NetIPv6_IsAddrInit()
*
* Description : Validate an IPv6 protocol address as the initialization address.
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if IPv6 protocol address is the initialization address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsAddrProtocolInit().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv6_IsAddrInit (CPU_INT08U  *p_addr_protocol,
                                 CPU_INT08U   addr_protocol_len)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    CPU_BOOLEAN    addr_init;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        return (DEF_NO);
    }

    addr_ip_len = NET_IPv6_ADDR_SIZE;
    if (addr_protocol_len != addr_ip_len) {
        return (DEF_NO);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* ------- VALIDATE IPv6 PROTOCOL INIT ADDR ------- */
    addr_init = NetIPv6_IsAddrUnspecified((NET_IPv6_ADDR *)p_addr_protocol);

    return (addr_init);
}


/*
*********************************************************************************************************
*                                  NetIPv6_IsAddrProtocolMulticast()
*
* Description : Validate an IPv6 protocol address as a multicast address.
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if IPv6 protocol address is a multicast address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsAddrProtocolMulticast().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef  NET_MCAST_MODULE_EN
CPU_BOOLEAN  NetIPv6_IsAddrProtocolMulticast (CPU_INT08U  *p_addr_protocol,
                                              CPU_INT08U   addr_protocol_len)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv6_ADDR  addr_ip;
    CPU_BOOLEAN    addr_multicast;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        return (DEF_NO);
    }

    addr_ip_len = NET_IPv6_ADDR_SIZE;
    if (addr_protocol_len != addr_ip_len) {
        return (DEF_NO);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* ------- VALIDATE IPv6 PROTOCOL INIT ADDR ------- */
    Mem_Copy(&addr_ip,
              p_addr_protocol,
              NET_IPv6_ADDR_SIZE);
    addr_multicast = NetIPv6_IsAddrMcast(&addr_ip);

    return (addr_multicast);
}
#endif


/*
*********************************************************************************************************
*                                     NetIPv6_AddrValidLifetimeTimeout()
*
* Description : Remove IPv6 address from address list when valid lifetime comes to an end.
*
* Argument(s) : p_ipv6_addr_timeout  Pointer to ipv6 addrs object that has timed out.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetIPv6_CfgAddrAddHandler.
*               Referenced in : NetNDP_RxPrefixAddrsUpdate.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_AddrValidLifetimeTimeout (void  *p_ipv6_addr_timeout)
{
    NET_IPv6_ADDRS   *p_ipv6_addrs;
    NET_IPv6_IF_CFG  *p_ipv6_if_cfg;

    p_ipv6_addrs  = (NET_IPv6_ADDRS *) p_ipv6_addr_timeout;
    p_ipv6_if_cfg = &NetIPv6_IF_CfgTbl[p_ipv6_addrs->IfNbr];

    p_ipv6_addrs->AddrState = NET_IPv6_ADDR_STATE_NONE;
    p_ipv6_addrs->IsValid   = DEF_NO;

    p_ipv6_addrs->ValidLifetimeTmrPtr = DEF_NULL;

    Mem_Clr(&p_ipv6_addrs->AddrHost, NET_IPv6_ADDR_SIZE);

    p_ipv6_if_cfg->AddrsNbrCfgd--;
}


/*
*********************************************************************************************************
*                                     NetIPv6_AddrPrefLifetimeTimeout()
*
* Description : Set the IPv6 address has being deprecated when preferred lifetime comes to an end.
*
* Argument(s) : p_ipv6_addr_timeout  Pointer to ipv6 addrs object that has timed out.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in : NetIPv6_CfgAddrAddHandler.
*               Referenced in : NetNDP_RxPrefixAddrsUpdate.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIPv6_AddrPrefLifetimeTimeout (void  *p_ipv6_addr_timeout)
{
    NET_IPv6_ADDRS   *p_ipv6_addrs;


    p_ipv6_addrs  = (NET_IPv6_ADDRS *) p_ipv6_addr_timeout;

    p_ipv6_addrs->AddrState = NET_IPv6_ADDR_STATE_DEPRECATED;
    p_ipv6_addrs->IsValid   = DEF_YES;

    p_ipv6_addrs->PrefLifetimeTmrPtr = DEF_NULL;
}


/*
*********************************************************************************************************
*                                        NetIPv6_AddrAutoCfgComp()
*
* Description : (1) Complete the IPv6 Auto-Configuration process.
*
*                   (a) Update IPv6 Auto-Configuration object for given interface.
*                   (b) Call Application Hook function.
*
*
* Argument(s) : if_nbr           Network interface number on which address auto-configuration occurred.
*
*               auto_cfg_status  Status of the IPv6 Auto-Configuration process.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_AddrAutoCfgHandler(),
*               NetIPv6_AddrAutoCfgDAD_Result(),
*               NetNDP_RxPrefixUpdate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv6_AddrAutoCfgComp() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
void  NetIPv6_AddrAutoCfgComp (NET_IF_NBR                 if_nbr,
                               NET_IPv6_AUTO_CFG_STATUS   auto_cfg_status)
{
    NET_IPv6_AUTO_CFG_OBJ         *p_auto_obj;
    NET_IPv6_AUTO_CFG_HOOK_FNCT    fnct;
    NET_IPv6_AUTO_CFG_STATE        state;
    CPU_BOOLEAN                    enabled;


    p_auto_obj = NetIPv6_AutoCfgObjTbl[if_nbr];

    state      = p_auto_obj->State;
    enabled    = p_auto_obj->En;

    fnct       = NetIPv6_AutoCfgHookFnct;

    if ((state == NET_IPv6_AUTO_CFG_STATE_STARTED_LOCAL)  ||
        (state == NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL)) {

        p_auto_obj->State = NET_IPv6_AUTO_CFG_STATE_NONE;

        if ((enabled == DEF_YES) &&
            (fnct   != DEF_NULL)) {
             fnct(if_nbr, p_auto_obj->AddrLocalPtr, p_auto_obj->AddrGlobalPtr, auto_cfg_status);
        }
    }

}
#endif


/*
*********************************************************************************************************
*                                      NetIPv6_GetAddrAutoCfgObj()
*
* Description : Recover the IPv6 Auto-Configuration object for the given interface number.
*
* Argument(s) : if_nbr      Network interface number where the Auto-configuration is taken place.
*
* Return(s)   : Pointer to the IPv6 Auto-Configuration object for the given Interface.
*
* Caller(s)   : NetNDP_RxPrefixUpdate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_GetAddrAutoCfgObj() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
NET_IPv6_AUTO_CFG_OBJ  *NetIPv6_GetAddrAutoCfgObj(NET_IF_NBR  if_nbr)
{
    NET_IPv6_AUTO_CFG_OBJ  *p_obj;


    p_obj = NetIPv6_AutoCfgObjTbl[if_nbr];

    return (p_obj);
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     NetIPv6_AddrAutoCfgHandler()
*
* Description : (1) Perform IPv6 Stateless Address Auto-Configuration:
*
*                   (a) Create link-local address.
*                   (b) Add link-local address to the interface.
*                   (c) If link-local configuration failed, restart with random address.
*                   (d) Start global address configuration.
*
* Argument(s) : if_nbr      Network interface number to perform address auto-configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       Address auto-configuration successful.
*                               NET_IPv6_ERR_AUTO_CFG_STARTED           Address auto-configuration already started on IF.
*
*                               ----------- RETURNED BY NetIPv6_CreateIF_ID() : ------------
*                               See NetIPv6_CreateIF_ID() for additional return error codes.
*
*                               ----------- RETURNED BY NetIPv6_CreateAddrFromID() : ------------
*                               See NetIPv6_CreateAddrFromID() for additional return error codes.
*
*                               ----------- RETURNED BY NetIPv6_CfgAddrAddHandler() : -------------
*                               See NetIPv6_CfgAddrAddHandler() for additional return error codes.
*
*                               ----------- RETURNED BY NetIPv6_CfgAddrRand() : ------------
*                               See NetIPv6_CfgAddrRand() for additional return error codes.
*
*                               ----------- RETURNED BY NetIPv6_CfgAddrGlobal() : ------------
*                               See NetIPv6_CfgAddrGlobal() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_AddrAutoCfgEn(),
*               NetIPv6_LinkStateSubscriber().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv6_AddrAutoCfgHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*               (2) RFC #4862 , Section 4 states that "Auto-configuration is performed only on
*                   multicast-capable links and begins when a multicast-capable interface is enabled,
*                   e.g., during system startup".
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  void  NetIPv6_AddrAutoCfgHandler (NET_IF_NBR   if_nbr,
                                          NET_ERR     *p_err)
{
    NET_IPv6_AUTO_CFG_OBJ    *p_obj;
    NET_IPv6_ADDRS           *p_addrs;
    NET_IPv6_ADDR            *p_addr_cfgd;
    NET_IPv6_ADDR             ipv6_id;
    NET_IPv6_ADDR             ipv6_addr;
    NET_IPv6_AUTO_CFG_STATE   state;
    NET_IPv6_AUTO_CFG_STATUS  status;
    CPU_BOOLEAN               dad_en;


                                                                /* --------- ADVERTISE AUTOCONFIG AS STARTED ---------- */
    p_obj = NetIPv6_AutoCfgObjTbl[if_nbr];

    state = p_obj->State;
    if ((state == NET_IPv6_AUTO_CFG_STATE_STARTED_LOCAL)  ||
        (state == NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL)) {
        *p_err  = NET_IPv6_ERR_AUTO_CFG_STARTED;
         goto exit;
    }

    p_obj->State = NET_IPv6_AUTO_CFG_STATE_STARTED_LOCAL;

                                                                /* Notify with hook that aut-cfg has started.           */
    NetIPv6_AddrAutoCfgComp(if_nbr, NET_IPv6_AUTO_CFG_STATUS_STARTED);
    p_obj->State = NET_IPv6_AUTO_CFG_STATE_STARTED_LOCAL;

                                                                /* ------------- SET DAD ENABLE VARIABLE -------------- */
    dad_en = p_obj->DAD_En;

                                                                /* -------------- CREATE LINK-LOCAL ADDR -------------- */
    (void)NetIPv6_CreateIF_ID(if_nbr,                           /* Create IF ID from HW mac addr.                       */
                             &ipv6_id,
                              NET_IPv6_ADDR_AUTO_CFG_ID_IEEE_48,
                              p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         p_obj->AddrLocalPtr  = DEF_NULL;
         p_obj->AddrGlobalPtr = DEF_NULL;
         status               = NET_IPv6_AUTO_CFG_STATUS_FAILED;
         goto exit_comp;
    }

    NetIPv6_CreateAddrFromID(&ipv6_id,                          /* Create link-local IPv6 addr from IF ID.              */
                             &ipv6_addr,
                              NET_IPv6_ADDR_PREFIX_LINK_LOCAL,
                              NET_IPv6_ADDR_PREFIX_LINK_LOCAL_LEN,
                              p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         p_obj->AddrLocalPtr  = DEF_NULL;
         p_obj->AddrGlobalPtr = DEF_NULL;
         status               = NET_IPv6_AUTO_CFG_STATUS_FAILED;
         goto exit_comp;
    }

                                                                /* ------------- ADD ADDRESS TO INTERFACE ------------- */
    p_addrs = NetIPv6_CfgAddrAddHandler(if_nbr,
                                       &ipv6_addr,
                                        NET_IPv6_ADDR_PREFIX_LINK_LOCAL_LEN,
                                        0,
                                        0,
                                        NET_IPv6_ADDR_CFG_MODE_AUTO,
                                        dad_en,
                                        NET_IPv6_ADDR_CFG_TYPE_AUTO_CFG_NO_BLOCKING,
                                        p_err);
    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
        case NET_ERR_FAULT_FEATURE_DIS:
             p_obj->AddrLocalPtr = &p_addrs->AddrHost;
             dad_en              =  DEF_NO;
                                                                /* Continue autoconfig with global addr cfg.            */
             NetIPv6_CfgAddrGlobal(if_nbr,
                                  &ipv6_addr,
                                   dad_en,
                                   p_err);
             switch (*p_err) {
                 case NET_IPv6_ERR_NONE:
                      goto exit;

                 case NET_IPv6_ERR_ROUTER_ADV_SIGNAL_TIMEOUT:
                      p_obj->AddrGlobalPtr = DEF_NULL;
                      status               = NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL;
                     *p_err                = NET_IPv6_ERR_NONE;
                      goto exit_comp;

                 default:
                     p_obj->AddrGlobalPtr = DEF_NULL;
                     status               = NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL;
                     goto exit;
             }
             break;


        case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
            *p_err = NET_IPv6_ERR_NONE;
             goto exit;                                         /* Callback function will continue auto-config.         */


        case NET_IPv6_ERR_ADDR_CFG_DUPLICATED:
                                                                /* Restart autoconfig with random address.              */
             p_addr_cfgd = NetIPv6_CfgAddrRand(if_nbr,
                                              &ipv6_id,
                                               dad_en,
                                               p_err);
             switch (*p_err) {
                 case NET_IPv6_ERR_NONE:
                 case NET_ERR_FAULT_FEATURE_DIS:
                      p_obj->AddrLocalPtr = p_addr_cfgd;
                      dad_en              = DEF_NO;
                                                                /* Continue autoconfig with global addr cfg.            */
                      NetIPv6_CfgAddrGlobal(if_nbr,
                                           &ipv6_addr,
                                            dad_en,
                                            p_err);
                      switch (*p_err) {
                          case NET_IPv6_ERR_NONE:
                               goto exit;

                          case NET_IPv6_ERR_ROUTER_ADV_SIGNAL_TIMEOUT:
                               p_obj->AddrGlobalPtr = DEF_NULL;
                               status               = NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL;
                              *p_err                = NET_IPv6_ERR_NONE;
                               goto exit_comp;

                          default:
                              p_obj->AddrGlobalPtr = DEF_NULL;
                              status               = NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL;
                              goto exit_comp;

                      }
                      break;


                 case NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS:
                     *p_err = NET_IPv6_ERR_NONE;
                      goto exit;                                /* Callback function will continue autoconfig.          */


                 case NET_IPv6_ERR_ADDR_CFG_DUPLICATED:
                 default:
                      p_obj->AddrLocalPtr  = DEF_NULL;
                      p_obj->AddrGlobalPtr = DEF_NULL;
                      status               = NET_IPv6_AUTO_CFG_STATUS_FAILED;
                      goto exit_comp;
             }
             break;


         default:
             p_obj->AddrLocalPtr  = DEF_NULL;
             p_obj->AddrGlobalPtr = DEF_NULL;
             status               = NET_IPv6_AUTO_CFG_STATUS_FAILED;
             goto exit_comp;
    }

exit_comp:
    NetIPv6_AddrAutoCfgComp(if_nbr, status);

exit:
    return;
}
#endif


/*
*********************************************************************************************************
*                                         NetIPv6_CfgAddrRand()
*
* Description : Create Random Link-Local address and start address configuration.
*
* Argument(s) : if_nbr      Network interface number to perform address auto-configuration.
*
*               p_ipv6_id   Pointer to IPv6 address Interface ID.
*
*               dad_en      DEF_YES, Do the Duplication Address Detection (DAD)
*                           DEF_NO , otherwise
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               ----------- RETURNED BY NetIPv6_CreateAddrFromID() : ------------
*                               See NetIPv6_CreateAddrFromID() for additional return error codes.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_AddrAutoCfgHandler(),
*               NetIPv6_AddrAutoCfgDAD_Result().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static  NET_IPv6_ADDR  *NetIPv6_CfgAddrRand (NET_IF_NBR      if_nbr,
                                             NET_IPv6_ADDR  *p_ipv6_id,
                                             CPU_BOOLEAN     dad_en,
                                             NET_ERR        *p_err)
{
    NET_IPv6_ADDRS  *p_ipv6_addrs;
    NET_IPv6_ADDR   *p_addr_return;
    NET_IPv6_ADDR    ipv6_addr;
    RAND_NBR         rand_nbr;
    CPU_INT08U      *p_addr_08;
    CPU_INT08U       retry_cnt;
    CPU_INT32U       ix;


    rand_nbr = ((p_ipv6_id->Addr[ 3] << 24) | (p_ipv6_id->Addr[ 2] << 16) | (p_ipv6_id->Addr[ 1] <<  8) | (p_ipv6_id->Addr[ 0])) ^
               ((p_ipv6_id->Addr[ 7] << 24) | (p_ipv6_id->Addr[ 6] << 16) | (p_ipv6_id->Addr[ 5] <<  8) | (p_ipv6_id->Addr[ 4])) ^
               ((p_ipv6_id->Addr[11] << 24) | (p_ipv6_id->Addr[10] << 16) | (p_ipv6_id->Addr[ 9] <<  8) | (p_ipv6_id->Addr[ 8])) ^
               ((p_ipv6_id->Addr[15] << 24) | (p_ipv6_id->Addr[14] << 16) | (p_ipv6_id->Addr[13] <<  8) | (p_ipv6_id->Addr[12]));

    Math_RandSetSeed(rand_nbr);

    retry_cnt = 0;

    while ((*p_err    != NET_IPv6_ERR_NONE)                  &&
           (*p_err    != NET_ERR_FAULT_FEATURE_DIS)          &&
           (*p_err    != NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS)  &&
           ( retry_cnt < NET_IPv6_AUTO_CFG_RAND_RETRY_MAX)) {
                                                                /* Generate a new ID from a random src.                 */
        rand_nbr  = Math_Rand();
        ix        = NET_IPv6_ADDR_PREFIX_LINK_LOCAL_LEN / 8u;
        p_addr_08 = (CPU_INT08U *)&p_ipv6_id->Addr[ix];

        while (ix < NET_IPv6_ADDR_SIZE) {
           *p_addr_08 ^= (CPU_INT08U)rand_nbr;                  /* Bitwise XOR each byte of the ID with the random nbr. */
            p_addr_08++;
            ix++;
        }

        NetIPv6_CreateAddrFromID(p_ipv6_id,                     /* Create link-local IPv6 addr from IF ID.              */
                                &ipv6_addr,
                                 NET_IPv6_ADDR_PREFIX_LINK_LOCAL,
                                 NET_IPv6_ADDR_PREFIX_LINK_LOCAL_LEN,
                                 p_err);
        if (*p_err != NET_IPv6_ERR_NONE) {
            p_addr_return = DEF_NULL;
            goto exit;
        }

                                                                /* ------------- ADD ADDRESS TO INTERFACE ------------- */
        (void)NetIPv6_CfgAddrAddHandler(if_nbr,
                                       &ipv6_addr,
                                        NET_IPv6_ADDR_PREFIX_LINK_LOCAL_LEN,
                                        0,
                                        0,
                                        NET_IPv6_ADDR_CFG_STATE_AUTO_CFGD,
                                        dad_en,
                                        NET_IPv6_ADDR_CFG_TYPE_AUTO_CFG_NO_BLOCKING,
                                        p_err);

        retry_cnt++;
    }

    if (retry_cnt == NET_IPv6_AUTO_CFG_RAND_RETRY_MAX) {
        p_addr_return = DEF_NULL;
        goto exit;
    }

    p_ipv6_addrs  =  NetIPv6_GetAddrsHostOnIF(if_nbr, &ipv6_addr);
    if (p_ipv6_addrs == DEF_NULL) {
        p_addr_return = DEF_NULL;
       *p_err = NET_IPv6_ERR_ADDR_NOT_FOUND;
        goto exit;
    }

    p_addr_return = &p_ipv6_addrs->AddrHost;

exit:
    return (p_addr_return);
}
#endif


/*
*********************************************************************************************************
*                                        NetIPv6_CfgAddrGlobal()
*
* Description : (1) Start IPv6 global address configuration:
*                   (a) Create Signal to be advertise of Router Advertisement reception.
*                   (b) Send Router Solicitation.
*                   (c) Wait for a received Router Advertisement.
*
*
* Argument(s) : if_nbr          Network interface number to perform address auto-configuration.
*
*               p_ipv6_addr     Pointer to IPv6 link-local address that was configured.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Started global addr cfg successfully.
*                               NET_ERR_FAULT_MEM_ALLOC         Memory allocation error.
*                               NET_IPv6_ERR_SIGNAL_CREATE      Rx RA Signal creation error.
*                               NET_IPv6_ERR_ADDR_CFG           Global addr cfg faulted.
*                               NET_IPv6_ERR_RX_RA_TIMEOUT      Pending on Rx RA signal timed out.
*                               NET_IPv6_ERR_SIGNAL_FAULT       Pending on Rx RA signal faulted.
*
*                               ------------ RETURNED BY Net_GlobalLockAcquire() -------------
*                               See Net_GlobalLockAcquire() for additional return error codes.
*
* Return(s)   : None.
*
* Caller(s)   : NetIPv6_AddrAutoCfgHandler(),
*               NetIPv6_AddrAutoCfgDAD_Result().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#ifdef NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
static void  NetIPv6_CfgAddrGlobal (NET_IF_NBR      if_nbr,
                                    NET_IPv6_ADDR  *p_ipv6_addr,
                                    CPU_BOOLEAN     dad_en,
                                    NET_ERR        *p_err)
{
    NET_IPv6_AUTO_CFG_OBJ  *p_obj;
    KAL_SEM_HANDLE         *p_signal;
    CPU_INT08U              retry_cnt;
    CPU_BOOLEAN             done;
    NET_ERR                 err_net;
    CPU_SR_ALLOC();


    p_obj = NetIPv6_AutoCfgObjTbl[if_nbr];
    CPU_CRITICAL_ENTER();
    p_obj->State = NET_IPv6_AUTO_CFG_STATE_STARTED_GLOBAL;
    CPU_CRITICAL_EXIT();

                                                                /* ----------------- INIT RX RA SIGNAL ---------------- */
    p_signal = NetNDP_RouterAdvSignalCreate(if_nbr, p_err);
    if (*p_err != NET_NDP_ERR_NONE) {
        *p_err = NET_IPv6_ERR_ADDR_CFG;
         goto exit;
    }

    retry_cnt = 0;
    done      = DEF_NO;
    while ((done      != DEF_YES)                 &&
           (retry_cnt  < NET_NDP_TX_ROUTER_SOL_RETRY_MAX)) {
                                                                /* -------------- TX ROUTER SOLICIT MSG --------------- */
        NetNDP_TxRouterSolicitation(if_nbr,
                                    p_ipv6_addr,
                                    p_err);
        if (*p_err != NET_NDP_ERR_NONE) {
            *p_err = NET_IPv6_ERR_ADDR_CFG;
             goto exit_del_signal;
        }

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
        Net_GlobalLockRelease();

                                                                /* ---------- WAIT FOR RX ROUTER ADV SIGNAL ----------- */
        NetNDP_RouterAdvSignalPend(p_signal, p_err);
        switch (*p_err) {
           case NET_NDP_ERR_NONE:
                done  = DEF_YES;
               *p_err = NET_IPv6_ERR_NONE;
                break;


           case NET_NDP_ERR_ROUTER_ADV_SIGNAL_TIMEOUT:
               *p_err = NET_IPv6_ERR_ROUTER_ADV_SIGNAL_TIMEOUT;
                break;


           default:
               *p_err = NET_IPv6_ERR_ADDR_CFG;
                goto exit_lock_acquire;
        }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
        Net_GlobalLockAcquire((void *)&NetIPv6_CfgAddrGlobal, &err_net);
        if (err_net != NET_ERR_NONE) {
           *p_err = err_net;
            goto exit_del_signal;
        }

        retry_cnt++;
    }


   (void)&dad_en;

    goto exit_del_signal;


exit_lock_acquire:
    Net_GlobalLockAcquire((void *)&NetIPv6_CfgAddrGlobal, &err_net);

exit_del_signal:
    NetNDP_RouterAdvSignalRemove(p_signal);

exit:
    return;
}
#endif


/*
*********************************************************************************************************
*                                     NetIPv6_AddrAutoCfgDAD_Result()
*
* Description : IPv6 Auto-configuration callback function when DAD is enabled to continue auto-configuration
*               process.
*
* Argument(s) : if_nbr      Network Interface number on which DAD is occurring.
*
*               p_dad_obj   Pointer to current DAD object.
*
*               status      Status of the DAD process.
*
* Return(s)   : None.
*
* Caller(s)   : Referenced in NetNDP_DAD_Start().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#if (defined(NET_IPv6_ADDR_AUTO_CFG_MODULE_EN) && \
    (defined(NET_DAD_MODULE_EN))                      )
static  void  NetIPv6_AddrAutoCfgDAD_Result (NET_IF_NBR                 if_nbr,
                                             NET_DAD_OBJ               *p_dad_obj,
                                             NET_IPv6_ADDR_CFG_STATUS   status)
{
    NET_IPv6_AUTO_CFG_OBJ     *p_auto_cfg_obj;
    NET_IPv6_ADDRS            *p_ipv6_addrs;
    NET_IPv6_ADDR             *p_ipv6_addr;
    NET_IPv6_AUTO_CFG_STATUS   auto_cfg_result;
    CPU_BOOLEAN                local_addr;
    NET_ERR                    err_net;


    p_auto_cfg_obj = NetIPv6_AutoCfgObjTbl[if_nbr];

    p_ipv6_addr  = &p_dad_obj->Addr;

    p_ipv6_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_ipv6_addr);/* Recover IPv6 addrs object.                          */
    if (p_ipv6_addrs == DEF_NULL) {
        p_auto_cfg_obj->AddrLocalPtr  = DEF_NULL;
        p_auto_cfg_obj->AddrGlobalPtr = DEF_NULL;
        auto_cfg_result               = NET_IPv6_AUTO_CFG_STATUS_FAILED;
        NetDAD_Stop(if_nbr, p_dad_obj);
        goto exit_comp;
    }

    NetDAD_Stop(if_nbr, p_dad_obj);                             /* Stop current DAD process.                            */

    p_ipv6_addr = &p_ipv6_addrs->AddrHost;

    local_addr  = NetIPv6_IsAddrLinkLocal(p_ipv6_addr);

    if (local_addr == DEF_YES) {                                /* Cfg of link-local addr has finished.                 */

        if (status != NET_IPv6_ADDR_CFG_STATUS_SUCCEED) {

            NetIPv6_CfgAddrRand(if_nbr,
                                p_ipv6_addr,
                                DEF_YES,
                               &err_net);
            if ((err_net != NET_IPv6_ERR_NONE)                &&
                (err_net != NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS)) {
                 p_auto_cfg_obj->AddrLocalPtr  = DEF_NULL;
                 p_auto_cfg_obj->AddrGlobalPtr = DEF_NULL;
                 auto_cfg_result               = NET_IPv6_AUTO_CFG_STATUS_FAILED;
                 goto exit_comp;
            }

            goto exit;

        } else {

            p_auto_cfg_obj->AddrLocalPtr = p_ipv6_addr;

            NetIPv6_CfgAddrGlobal(if_nbr,
                                  p_ipv6_addr,
                                  DEF_YES,
                                 &err_net);
            if (err_net != NET_IPv6_ERR_NONE) {
                p_auto_cfg_obj->AddrGlobalPtr = DEF_NULL;
                auto_cfg_result               = NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL;
                goto exit_comp;
            }

            goto exit;

        }
    } else {                                                    /* Cfg of global addr has finished.                     */

        if (status != NET_IPv6_ADDR_CFG_STATUS_SUCCEED) {
            p_auto_cfg_obj->AddrGlobalPtr = DEF_NULL;
            auto_cfg_result               = NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL;
            goto exit_comp;
        } else {
            p_auto_cfg_obj->AddrGlobalPtr = p_ipv6_addr;
            auto_cfg_result               = NET_IPv6_AUTO_CFG_STATUS_SUCCEEDED;
            goto exit_comp;
        }
    }


exit_comp:
    NetIPv6_AddrAutoCfgComp(if_nbr, auto_cfg_result);

exit:
    return;
}
#endif


/*
*********************************************************************************************************
*                                    NetIPv6_CfgAddrAddDAD_Result()
*
* Description : Hook function call after IPv6 static address configuration with DAD enabled to release DAD
*               object.
*
* Argument(s) : if_nbr      Interface number on which DAD is occurring.
*
*               p_dad_obj   Pointer to current DAD object.
*
*               status      Status of the DAD process.
*
* Return(s)   : None.
*
* Caller(s)   : Referenced in NetNDP_DAD_Start().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#ifdef NET_DAD_MODULE_EN
static  void  NetIPv6_CfgAddrAddDAD_Result (NET_IF_NBR                 if_nbr,
                                            NET_DAD_OBJ               *p_dad_obj,
                                            NET_IPv6_ADDR_CFG_STATUS   status)
{
    NET_IPv6_ADDRS               *p_ipv6_addrs;
    NET_IPv6_ADDR                *p_ipv6_addr;
    NET_IPv6_ADDR                *p_addr_return;
    NET_IPv6_ADDR_HOOK_FNCT       fnct;
    NET_IPv6_ADDR_CFG_STATUS      status_return;


    p_ipv6_addr = &p_dad_obj->Addr;

    fnct = NetIPv6_AddrCfgHookFnct;

    p_ipv6_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_ipv6_addr);
    if (p_ipv6_addrs == DEF_NULL) {
        p_addr_return = DEF_NULL;
        status_return = NET_IPv6_ADDR_CFG_STATUS_FAIL;
    } else {
        p_addr_return = &p_ipv6_addrs->AddrHost;
        status_return =  status;
    }

    if (fnct != DEF_NULL) {
        fnct(if_nbr, p_addr_return, status_return);
    }

    NetDAD_Stop(if_nbr, p_dad_obj);
}
#endif


/*
*********************************************************************************************************
*                                         NetIPv6_AddrSrcSel()
*
* Description : Select the best Source address for the given destination address following the rules
*               given by the Default Source Selection mentioned in RFC #6724.
*
* Argument(s) : if_nbr          Interface number on which to send packet.
*
*               p_addr_dest     Pointer to IPv6 destination address.
*
*               p_addr_tbl      Pointer to head of addresses table of Interface.
*
*               p_addr_tbl_qty  Pointer to number of address configured on Interface.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_IPv6_ERR_NONE               Address selection succeeded.
*                                   NET_IPv6_ERR_TX_SRC_SEL_FAIL    Address selection failed.
*
* Return(s)   : Pointer to IPv6 addresses object to use as source address.
*
* Caller(s)   : NetIPv6_GetAddrSrcHandler().
*
* Note(s)     : (1) If source address selection failed, the first address in the Interface addresses
*                   table is return.
*
*               (2) RFC #6724 Section 5 - "Source Address Selection" describe selection rules:
*
*                   (a) Rule 1: Prefer same address.
*
*                   (b) Rule 3: Avoid deprecated addresses.
*
*                   (c) Rule 4: Prefer home addresses.
*
*                       (i) Not applicable
*
*                   (d) Rule 5: Prefer outgoing interface.
*
*                       (i) Not applicable
*
*                   (e) Rule 5.5: Prefer addresses in a prefix advertised by the next-hop.
*
*                       (i) Not applicable
*
*                   (f) Rule 6: Prefer matching label.
*
*                   (g) Rule 7: Prefer temporary addresses.
*
*                       (i) Not applicable
*
*                   (h) Rule 8: Use longest matching prefix.
*********************************************************************************************************
*/

static  const  NET_IPv6_ADDRS  *NetIPv6_AddrSrcSel (       NET_IF_NBR         if_nbr,
                                                    const  NET_IPv6_ADDR     *p_addr_dest,
                                                    const  NET_IPv6_ADDRS    *p_addr_tbl,
                                                           NET_IP_ADDRS_QTY   addr_tbl_qty,
                                                           NET_ERR           *p_err)
{
    const  NET_IPv6_ADDRS    *p_addr_cur;
    const  NET_IPv6_ADDRS    *p_addr_sel;
    const  NET_IPv6_POLICY   *p_policy_dest;
    const  NET_IPv6_POLICY   *p_policy_addr;
           NET_IP_ADDRS_QTY   addr_ix;
           CPU_BOOLEAN        valid;
           CPU_INT08U         rule_sel;
           CPU_INT08U         dest_scope;
           CPU_INT08U         addr_scope;
           CPU_INT08U         len_cur;
           CPU_INT08U         len_sel;


    dest_scope    = NetIPv6_GetAddrScope(p_addr_dest);
    p_policy_dest = NetIPv6_AddrSelPolicyGet(p_addr_dest);
    rule_sel      = NET_IPv6_SRC_SEL_RULE_NONE;
    len_sel       = 0u;


    for (addr_ix = 0u; addr_ix < addr_tbl_qty; addr_ix++) {

        p_addr_cur = &p_addr_tbl[addr_ix];

                                                                /* RULE #1 : Prefer same address.                       */
        valid = NetIPv6_IsAddrsIdentical(&p_addr_cur->AddrHost, p_addr_dest);
        if (valid == DEF_YES) {
            p_addr_sel = p_addr_cur;
            goto rule_found;
        }

                                                                /* RULE #2 : Prefer appropriate scope.                  */
        addr_scope = NetIPv6_GetAddrScope(&p_addr_cur->AddrHost);
        if (addr_scope == dest_scope) {
            p_addr_sel  = p_addr_cur;
            rule_sel    = NET_IPv6_SRC_SEL_RULE_02;
            continue;
        }

        if (rule_sel > NET_IPv6_SRC_SEL_RULE_03) {              /* RULE #3 : Avoid deprecated addresses.                */

            if (p_addr_cur->AddrState == NET_IPv6_ADDR_STATE_PREFERRED) {
                p_addr_sel = p_addr_cur;
                rule_sel   = NET_IPv6_SRC_SEL_RULE_03;
                continue;
            }

        } else {
            continue;
        }

                                                                /* RULE #4 :   See Note 2d.                             */
                                                                /* RULE #5 :   See Note 2e.                             */
                                                                /* RULE #5.5 : See Note 2f.                             */

                                                                /* RULE #6 : Prefer matching label.                     */
        if (rule_sel > NET_IPv6_SRC_SEL_RULE_06) {

            p_policy_addr = NetIPv6_AddrSelPolicyGet(&p_addr_cur->AddrHost);
            if (p_policy_addr->Label == p_policy_dest->Label) {
                p_addr_sel = p_addr_cur;
                rule_sel   = NET_IPv6_SRC_SEL_RULE_06;
                continue;
            }

        } else {
            continue;
        }

                                                                /* RULE #7 :   See Note 2d.                             */

                                                                /* RULE #8 : Use longest matching prefix.               */
        len_cur = NetIPv6_GetAddrMatchingLen(p_addr_dest, &p_addr_cur->AddrHost);
        if (len_cur >= len_sel) {
            len_sel    = len_cur;
            p_addr_sel = p_addr_cur;
            rule_sel   = NET_IPv6_SRC_SEL_RULE_08;
            continue;
        }
    }



    if (rule_sel == NET_IPv6_SRC_SEL_RULE_NONE) {
        p_addr_sel = &p_addr_tbl[0];
       *p_err      =  NET_IPv6_ERR_TX_SRC_SEL_FAIL;
        goto exit;
    }


    (void)&if_nbr;


rule_found:
   *p_err = NET_IPv6_ERR_NONE;

exit:
    return (p_addr_sel);
}


/*
*********************************************************************************************************
*                                       NetIPv6_LookUpPolicyTbl()
*
* Description : Get address selection policy for a given IPv6 address.
*
* Argument(s) : p_addr      Pointer to IPv6 address
*               ------      Argument checked by caller(s).
*
* Return(s)   : Pointer to address selection policy matching the given address.
*
* Caller(s)   : NetIPv6_AddrSrcSelect().
*
* Note(s)     : (1) The Policy table is part of the procedure to select the source and destination
*                   address to Transmit packets.
*********************************************************************************************************
*/

const  NET_IPv6_POLICY  *NetIPv6_AddrSelPolicyGet (const  NET_IPv6_ADDR *p_addr)
{
    const  NET_IPv6_POLICY  *p_policy_entry;
    const  NET_IPv6_POLICY  *p_policy_found;
           CPU_INT08U        i;
           CPU_BOOLEAN       valid;


    for (i = 0; i < NET_IPv6_POLICY_TBL_SIZE; i++) {
        p_policy_entry = NetIPv6_PolicyTbl[i];
        valid = NetIPv6_IsAddrAndMaskValid(p_addr, p_policy_entry->PrefixMaskPtr);
        if (valid == DEF_YES) {
            p_policy_found = p_policy_entry;
            goto exit;
        }
    }


exit:
    return (p_policy_found);
}


/*
*********************************************************************************************************
*                                      NetIPv6_CfgAddrValidate()
*
* Description : Validate an IPv6 host address and prefix length for configuration on an interface.
*
* Argument(s) : p_addr       Pointer to desired IPv6 address to configure.
*
*               prefix_len   Prefix length of the desired IPv6 address.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       IPv6 address successfully configured.
*                               NET_IPv6_ERR_INVALID_ADDR_HOST          Invalid IPv6 address.
*                               NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN    Invalid IPv6 address prefix length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_CfgAddrAdd().
*
* Note(s)     : (1) See function NetIPv6_IsValidAddrHost() for supported IPv6 address host.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
static  void  NetIPv6_CfgAddrValidate (NET_IPv6_ADDR  *p_addr,
                                       CPU_INT08U      prefix_len,
                                       NET_ERR        *p_err)
{
    CPU_BOOLEAN  addr_valid;


                                                                /* ----------- VALIDATE IPv6 ADDR PREFIX LEN ---------- */
    if (prefix_len > NET_IPv6_ADDR_PREFIX_LEN_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgInvAddrHostCtr);
       *p_err = NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN;
        return;
    }

                                                                /* -------------- VALIDATE HOST ADDRESS --------------- */
    addr_valid = NetIPv6_IsValidAddrHost(p_addr);
    if (addr_valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.CfgInvAddrHostCtr);
       *p_err =  NET_IPv6_ERR_INVALID_ADDR_HOST;
        return;
    }


   *p_err = NET_IPv6_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                     NetIPv6_RxPktValidateBuf()
*
* Description : Validate received buffer header as IPv6 protocol.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received IPv6 packet.
*               --------    Argument validated in NetIPv6_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Received buffer's IPv6 header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT IPv6.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIPv6_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
                                        NET_ERR      *p_err)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  valid;
    NET_ERR      err;

                                                                /* -------------- VALIDATE NET BUF TYPE --------------- */
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
             break;


        case NET_BUF_TYPE_NONE:
        case NET_BUF_TYPE_BUF:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

                                                                /* --------------- VALIDATE IPv6 BUF HDR -------------- */
    if (p_buf_hdr->ProtocolHdrType != NET_PROTOCOL_TYPE_IP_V6) {
        if_nbr = p_buf_hdr->IF_Nbr;
        valid  = NetIF_IsValidHandler(if_nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvIF_Ctr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->IP_HdrIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_IPv6_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       NetIPv6_RxPktValidate()
*
* Description : (1) Validate received IPv6 packet :
*
*                   (a) (1) Validate the received packet's following IPv6 header fields :
*
*                           (A) Version
*                           (B) Traffic Class
*                           (C) Flow Label
*                           (D) Payload Length
*                           (E) Next Header
*                           (F) Source      Address
*                           (G) Destination Address
*
*                       (2) Validation ignores the following IPv6 header fields :
*
*                           (A) Hop Limit
*
*                   (b) Convert the following IPv6 header fields from network-order to host-order :
*
*                       (1) Version
*                       (2) Traffic Class
*                       (3) Flow Label
*
*                           (A) These fields are NOT converted directly in the received packet buffer's
*                               data area but are converted in local or network buffer variables ONLY.
*
*                           (B) The following IPv6 header fields are converted & stored in network buffer
*                               variables :
*
*                               (1) Payload Length
*                               (4) Source Address
*                               (5) Destination Address
*                               (6) Next Header
*
*                   (c) Update network buffer's protocol controls
*
*                   (d) Process IPv6 packet in ICMPv6 Receive Handler
*
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 packet.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetIPv6_Rx().
*
*               p_ip_hdr    Pointer to received packet's IPv6 header.
*               -------     Argument validated in NetIPv6_Rx()/NetIPv6_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                       Received packet validated.
*                               NET_IPv6_ERR_INVALID_VER                Invalid IPv6 version.
*                               NET_IPv6_ERR_INVALID_TRAFFIC_CLASS      Invalid IPv6 Traffic Class.
*                               NET_IPv6_ERR_INVALID_FLOW_LABEL         Invalid IPv6 Flow Label.
*                               NET_IPv6_ERR_INVALID_LEN_TOT            Invalid IPv6 total  length.
*                               NET_IPv6_ERR_INVALID_PROTOCOL           Invalid/unknown protocol type.
*                               NET_IPv6_ERR_INVALID_LEN_DATA           Invalid IPv6 data   length.
*                               NET_IPv6_ERR_INVALID_ADDR_SRC           Invalid IPv6 source      address.
*                               NET_IPv6_ERR_INVALID_ADDR_DEST          Invalid IPv6 destination address.
*                               NET_IPv6_ERR_INVALID_EH                 Invalid IPv6 Extension header.
*
*                                                                       --- RETURNED BY NetIF_IsEnHandler() : ----
*                               NET_IF_ERR_INVALID_IF                   Invalid OR disabled network interface.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Rx().
*
* Note(s)     : (2) See 'net_ipv6.h  IP HEADER' for IPv6 header format.
*
*               (3) The following IPv6 header fields MUST be decoded &/or converted from network-order to host-order
*                   BEFORE any ICMP Error Messages are transmitted (see 'net_icmp.c  NetICMPv6_TxMsgErr()  Note #2') :
*
*                   (a) Header Length
*                   (b) Payload  Length
*                   (c) Source      Address
*                   (d) Destination Address
*
*               (4) (a) In addition to validating that the IPv6 header Total Length is greater than or equal to the
*                       IPv6 header Header Length, the IPv6 total length SHOULD be compared to the remaining packet
*                       data length which should be identical.
*
*                   (b) (1) However, some network interfaces MAY append octets to their frames :
*
*                           (A) 'pad' octets, if the frame length does NOT meet the frame's required minimum size :
*
*                               (1) RFC #894, Section 'Frame Format' states that "the minimum length of  the data
*                                   field of a packet sent over an Ethernet is 46 octets.  If necessary, the data
*                                   field should be padded (with octets of zero) to meet the Ethernet minimum frame
*                                   size.  This padding is not part of the IPv6 packet and is not included in the
*                                   total length field of the IPv6 header".
*
*                               (2) RFC #1042, Section 'Frame Format and MAC Level Issues : For all hardware types'
*                                   states that "IEEE 802 packets may have a minimum size restriction.  When
*                                   necessary, the data field should be padded (with octets of zero) to meet the
*                                   IEEE 802 minimum frame size requirements.  This padding is not part of the IPv6
*                                   datagram and is not included in the total length field of the IPv6 header".
*
*                           (B) Trailer octets, to improve overall throughput :
*
*                               (1) RFC #893, Section 'Introduction' specifies "a link-level ... trailer
*                                   encapsulation, or 'trailer' ... to minimize the number and size of memory-
*                                   to-memory copy operations performed by a receiving host when processing a
*                                   data packet".
*
*                               (2) RFC #1122, Section 2.3.1 states that "trailer encapsulations[s] ... rearrange
*                                   the data contents of packets ... [to] improve the throughput of higher layer
*                                   protocols".
*
*                           (C) CRC or checksum values, optionally copied from a device.
*
*                       (2) Therefore, if ANY octets are appended to the total frame length, then the packet's
*                           remaining data length MAY be greater than the IPv6 total length :
*
*                           (A) Thus, the IPv6 total length & the packet's remaining data length CANNOT be
*                               compared for equality.
*
*                               (1) Unfortunately, this eliminates the possibility to validate the IPv6 total
*                                   length to the packet's remaining data length.
*
*                           (B) And the IPv6 total length MAY be less than the packet's remaining
*                               data length.
*
*                               (1) However, the packet's remaining data length MUST be reset to the IPv6
*                                   total length to correctly calculate higher-layer application data
*                                   length.
*
*                           (C) However, the IPv6 total length CANNOT be greater than the packet's remaining
*                               data length.
*
*               (5) RFC #4443, Section 3.4 'Parameter Problem Message' states that "If an IPv6 node processing
*                   a packet finds a problem with a field in the IPv6 header or extension headers such that it
*                   cannot complete processing the packet, it MUST discard the packet and SHOULD originate an
*                   ICMPv6 Parameter Problem message to the packet's source, indicating the type and location
*                   of the problem."
*
*               (6) IPv6 packets with the following invalid IPv6 header fields be "silently discarded" :
*
*                   (a) Version
*
*                   (b) Source Address
*
*                       (1) (A) A host MUST silently discard an incoming datagram containing an IPv6 source
*                               address that is invalid by the rules of this section.
*
*                           (B) (1) MAY be one of the following :
*                                   (a) Configured host address
*                                   (b) Unspecified     address
*
*                               (2) MUST NOT be one of the following :
*                                   (a) IPv6 Multicast  address
*                                   (b) IPv6 Loopback   address
*
*                   (c) Destination Address
*
*                       (1) (A) A host MUST silently discard an incoming datagram that is not destined for :
*
*                               (1) (one of) the host's IPv6 address(es);
*                               (2) the address for a multicast group of which the host is a member
*                                    on the incoming physical interface.
*
*                           (B) (1) MUST     be one of the following :
*                                   (a) IPv6 Unicast address
*                                   (b) Multicast    address
*
*                               (2) MUST NOT be one of the following :
*                                   (a) Unspecified  IPv6 address
*
*              (7) See 'net_ipv6.h  IPv6 ADDRESS DEFINES  Notes #2 & #3' for supported IPv6 addresses.
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktValidate (NET_BUF        *p_buf,
                                     NET_BUF_HDR    *p_buf_hdr,
                                     NET_IPv6_HDR   *p_ip_hdr,
                                     NET_ERR        *p_err)
{
#if 0
    CPU_BOOLEAN                 addr_host_dest;
    NET_IPv6_ADDR              *p_ip_addr;
#endif
#if  (NET_IPv6_CFG_TRAFFIC_CLASS_EN == DEF_ENABLED)
    CPU_INT32U                  ip_traffic_class;
    CPU_INT08U                  ip_dscp;
    CPU_INT08U                  ip_ecn;
#endif
#if  (NET_IPv6_CFG_FLOW_LABEL_EN == DEF_ENABLED)
    CPU_INT32U                  ip_flow_label;
#endif
    const  NET_IPv6_ADDRS      *p_ip_addrs;
           NET_IPv6_ADDR_TYPE   addr_type;
           NET_IF_NBR           if_nbr;
           CPU_INT32U           ip_ver_traffic_flow;
           CPU_INT32U           ip_ver;
           CPU_INT16U           protocol_ix;
           CPU_BOOLEAN          rx_remote_host;
#ifdef NET_MLDP_MODULE_EN
           CPU_BOOLEAN          grp_joined;
#endif
#ifdef NET_ICMPv6_MODULE_EN
           NET_ERR              msg_err;
#endif

                                                                /* --------------- CONVERT IPv6 FIELDS ---------------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&ip_ver_traffic_flow,     &p_ip_hdr->VerTrafficFlow);
    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->IP_TotLen,    &p_ip_hdr->PayloadLen);

    Mem_Copy(&p_buf_hdr->IPv6_AddrSrc,  &p_ip_hdr->AddrSrc,  NET_IPv6_ADDR_SIZE);
    Mem_Copy(&p_buf_hdr->IPv6_AddrDest, &p_ip_hdr->AddrDest, NET_IPv6_ADDR_SIZE);

                                                                /* ---------------- VALIDATE IPv6 VER ----------------- */
    ip_ver      = ip_ver_traffic_flow & NET_IPv6_HDR_VER_MASK;
    ip_ver    >>= NET_IPv6_HDR_VER_SHIFT;
    if (ip_ver != NET_IPv6_HDR_VER) {                           /* Validate IP ver.                                     */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvVerCtr);
       *p_err = NET_IPv6_ERR_INVALID_VER;
        return;
    }

#if  (NET_IPv6_CFG_TRAFFIC_CLASS_EN == DEF_ENABLED)
                                                                /* ----------- VALIDATE IPv6 TRAFFIC CLASS ------------ */
                                                                /* See 'net_ipv6.h  IPv6 HEADER  Note #x'.              */

    ip_traffic_class      = ip_ver_traffic_flow & NET_IPv6_HDR_TRAFFIC_CLASS_MASK_32;
    ip_traffic_class    >>= NET_IPv6_HDR_TRAFFIC_CLASS_SHIFT;

                                                                /* - VALIDATE IPv6 DIFFERENTIATED SERVICES CODEPOINT -- */

    ip_dscp               = ip_traffic_class    & NET_IPv6_HDR_DSCP_MASK_08;
    ip_dscp             >>= NET_IPv6_HDR_DSCP_SHIFT;

                                                                /* -- VALIDATE IPv6 EXPLICIT CONGESTION NOTIFICATION -- */

    ip_ecn                = ip_traffic_class    & NET_IPv6_HDR_ECN_MASK_08;

    APP_TRACE_IPv6("           Traffic class = %u\r\n", ip_traffic_class);


    if (ip_traffic_class != NET_IPv6_HDR_TRAFFIC_CLASS) {       /* Validate IP traffic class.                           */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvTrafficClassCtr);
       *p_err = NET_IPv6_ERR_INVALID_TRAFFIC_CLASS;
        return;
    }

#endif

#if  (NET_IPv6_CFG_FLOW_LABEL_EN == DEF_ENABLED)
                                                                /* ------------ VALIDATE IPv6 FLOW LABEL -------------- */
                                                                /* See 'net_ipv6.h  IPv6 HEADER  Note #x'.              */
    ip_flow_label      = ip_ver_traffic_flow & NET_IPv6_HDR_FLOW_LABEL_MASK;
    ip_flow_label    >>= NET_IPv6_HDR_FLOW_LABEL_SHIFT;
    if (ip_flow_label != NET_IPv6_HDR_FLOW_LABEL) {             /* Validate IP flow label.                              */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvFlowLabelCtr);
       *p_err = NET_IPv6_ERR_INVALID_FLOW_LABEL;
        return;
    }
#endif

                                                                /* ------------ VALIDATE IPv6 PAYLOAD LEN ------------- */
    if (p_buf_hdr->IP_TotLen > p_buf_hdr->DataLen) {              /* If IPv6 tot len > rem pkt data len, ...              */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvTotLenCtr);
#ifdef NET_ICMPv6_MODULE_EN
        NetICMPv6_TxMsgErr(p_buf,
                           NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv6_PTR_IX_IP_PAYLOAD_LEN,
                          &msg_err);
#endif
       *p_err = NET_IPv6_ERR_INVALID_LEN_TOT;                    /* ... rtn err (see Note #4b2C).                        */
        return;
    }

    p_buf_hdr->IP_DataLen = (CPU_INT16U  )p_buf_hdr->IP_TotLen;
    p_buf_hdr->IP_HdrLen  = sizeof(NET_IPv6_HDR);

                                                                /* ------------ VALIDATE IPv6 NEXT HEADER ------------- */
    switch (p_ip_hdr->NextHdr) {                                /* See 'net_ipv6.h  IP HEADER PROTOCOL FIELD ...        */
        case NET_IP_HDR_PROTOCOL_ICMPv6:
        case NET_IP_HDR_PROTOCOL_UDP:
        case NET_IP_HDR_PROTOCOL_TCP:
             break;


        case NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP:
        case NET_IP_HDR_PROTOCOL_EXT_DEST:
        case NET_IP_HDR_PROTOCOL_EXT_ROUTING:
        case NET_IP_HDR_PROTOCOL_EXT_FRAG:
        case NET_IP_HDR_PROTOCOL_EXT_AUTH:
        case NET_IP_HDR_PROTOCOL_EXT_ESP:
        case NET_IP_HDR_PROTOCOL_EXT_NONE:
        case NET_IP_HDR_PROTOCOL_EXT_MOBILITY:
             break;


        default:                                                /* See Note #x.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvProtocolCtr);
#ifdef  NET_ICMPv6_MODULE_EN
             NetICMPv6_TxMsgErr(p_buf,
                                NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_NEXT_HDR,
                                NET_IPv6_HDR_NEXT_HDR_IX,
                               &msg_err);
#endif
            *p_err = NET_IPv6_ERR_INVALID_PROTOCOL;
             return;

    }
                                                                /* --------------- VALIDATE IPv6 ADDRS ---------------- */
    if_nbr = p_buf_hdr->IF_Nbr;                                 /* Get pkt's rx'd IF.                                   */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
   (void)NetIF_IsEnHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

                                                                /* Chk pkt rx'd to cfg'd host addr.                     */
#if 0
    if (if_nbr == NET_IF_NBR_LOCAL_HOST) {
        p_ip_addr      = (NET_IPv6_ADDR *)0;
        addr_host_dest =  NetIPv6_IsAddrHostCfgdHandler(&p_buf_hdr->IPv6_AddrDest);
    }
#endif
                                                                /* Chk pkt rx'd via local or remote host.               */
    rx_remote_host = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_REMOTE);
    if (((if_nbr  != NET_IF_NBR_LOCAL_HOST) && (rx_remote_host == DEF_NO)) ||
        ((if_nbr  == NET_IF_NBR_LOCAL_HOST) && (rx_remote_host != DEF_NO))) {
         NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvDestCtr);
        *p_err = NET_IPv6_ERR_INVALID_ADDR_DEST;
         return;
    }
                                                                /* -------------- VALIDATE IPv6 SRC ADDR -------------- */
    addr_type = NetIPv6_AddrTypeValidate(&p_buf_hdr->IPv6_AddrSrc, if_nbr);
    switch (addr_type) {
        case  NET_IPv6_ADDR_TYPE_UNICAST:
        case  NET_IPv6_ADDR_TYPE_LINK_LOCAL:
        case  NET_IPv6_ADDR_TYPE_SITE_LOCAL:
        case  NET_IPv6_ADDR_TYPE_UNSPECIFIED:
        case  NET_IPv6_ADDR_TYPE_NONE:
              break;


        case  NET_IPv6_ADDR_TYPE_MCAST:
        case  NET_IPv6_ADDR_TYPE_MCAST_SOL:
        case  NET_IPv6_ADDR_TYPE_MCAST_ROUTERS:
        case  NET_IPv6_ADDR_TYPE_MCAST_NODES:
        case  NET_IPv6_ADDR_TYPE_LOOPBACK:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvAddrSrcCtr);
            *p_err = NET_IPv6_ERR_INVALID_ADDR_SRC;
             return;
    }
                                                                /* -------------- VALIDATE IPv6 DEST ADDR ------------- */
    addr_type = NetIPv6_AddrTypeValidate(&p_buf_hdr->IPv6_AddrDest, if_nbr);
    switch (addr_type) {

        case  NET_IPv6_ADDR_TYPE_MCAST_ROUTERS:
        case  NET_IPv6_ADDR_TYPE_MCAST_NODES:
        case  NET_IPv6_ADDR_TYPE_LOOPBACK:
              (void)&p_ip_addrs;
              break;

        case  NET_IPv6_ADDR_TYPE_MCAST:
        case  NET_IPv6_ADDR_TYPE_MCAST_SOL:
#ifdef  NET_MLDP_MODULE_EN
              grp_joined = NetMLDP_IsGrpJoinedOnIF(if_nbr,
                                                  &p_buf_hdr->IPv6_AddrDest);
              if (grp_joined == DEF_NO) {
                  NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvDestCtr);
                 *p_err = NET_IPv6_ERR_INVALID_ADDR_DEST;
                  return;
              }
#else
              NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvDestCtr);
             *p_err = NET_IPv6_ERR_INVALID_ADDR_DEST;
#endif
              break;


        case  NET_IPv6_ADDR_TYPE_UNICAST:
        case  NET_IPv6_ADDR_TYPE_SITE_LOCAL:
        case  NET_IPv6_ADDR_TYPE_LINK_LOCAL:
        case  NET_IPv6_ADDR_TYPE_NONE:
        case  NET_IPv6_ADDR_TYPE_UNSPECIFIED:
        default:
            p_ip_addrs = NetIPv6_GetAddrsHostOnIF(                        if_nbr,
                                                  (const NET_IPv6_ADDR *)&p_buf_hdr->IPv6_AddrDest);
            if (p_ip_addrs == (NET_IPv6_ADDRS *)0) {
                NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvDestCtr);
                *p_err = NET_IPv6_ERR_INVALID_ADDR_DEST;
                return;
            }
    }

                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
                                                                /* See Note #3a.                                        */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf_hdr->IP_HdrLen > p_buf_hdr->DataLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvDataLenCtr);
       *p_err = NET_IPv6_ERR_INVALID_LEN_DATA;
        return;
    }
#endif
    p_buf_hdr->DataLen -= (NET_BUF_SIZE) p_buf_hdr->IP_HdrLen;
    protocol_ix        = (CPU_INT16U  )(p_buf_hdr->IP_HdrIx + p_buf_hdr->IP_HdrLen);

                                                                /* ----------- PROCESS NEXT HEADER PROTOCOL ----------- */
    NetIPv6_RxPktValidateNextHdr(p_buf,
                                 p_buf_hdr,
                                 p_ip_hdr->NextHdr,
                                 protocol_ix,
                                 p_err);

    switch (*p_err) {
        case NET_IPv6_ERR_NONE:
             break;


        case NET_IPv6_ERR_INVALID_EH_OPT_SEQ:
        default:
#ifdef   NET_ICMPv6_MODULE_EN
             NetICMPv6_TxMsgErr(p_buf,
                                NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_NEXT_HDR,
                               (p_buf_hdr->IP_HdrIx - p_buf_hdr->IP_HdrIx),
                                p_err);
#endif
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
            *p_err = NET_IPv6_ERR_INVALID_EH;
             return;
    }


    DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_IPv6_FRAME);

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetIPv6_RxPktValidateNextHdr()
*
* Description : (1) Validates the IPv6 Header or Extension Header Next Header.
*
*               (2) Updates the Protocol Header and Protocol Index field of p_buf_hdr.
*
*
* Argument(s) : p_buf           Pointer to network buffer that received IPv6 packet.
*               ----            Argument checked   in NetIPv6_Rx().
*
*               p_buf_hdr       Pointer to network buffer header that received IPv6 packet.
*               --------        Argument validated in NetIPv6_Rx().
*
*               next_hdr        Protocol type or Extension Header of the following header.
*
*               protocol_ix     Index of the Next Header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                   next_hdr is valid.
*                               NET_IPv6_ERR_INVALID_EH_OPT_SEQ     next_hdr is NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP
*                                                                   and not the first ext hdr of packet.
*                               NET_IPv6_ERR_INVALID_PROTOCOL       Otherwise.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : (3) If, a node encounters a Next Header value of zero (Hop-by-Hop) in any header other
*                   than an IPv6 header or an unrecognized Next Header, it must send an ICMP Parameter Problem
*                   message to the source of the packet, with an ICMP Code value of 1 ("unrecognized Next
*                   Header type encountered") and the ICMP Pointer field containing the offset of the
*                   unrecognized value within the original packet. (See RFC #2460, Section 4.0).
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktValidateNextHdr (NET_BUF            *p_buf,
                                            NET_BUF_HDR        *p_buf_hdr,
                                            NET_IPv6_NEXT_HDR   next_hdr,
                                            CPU_INT16U          protocol_ix,
                                            NET_ERR            *p_err)
{
    switch (next_hdr) {
        case NET_IP_HDR_PROTOCOL_ICMPv6:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_ICMP_V6;
             p_buf_hdr->ProtocolHdrTypeNetSub    = NET_PROTOCOL_TYPE_ICMP_V6;
             p_buf_hdr->ICMP_MsgIx               = protocol_ix;
             break;


        case NET_IP_HDR_PROTOCOL_UDP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_UDP_V6;
             p_buf_hdr->ProtocolHdrTypeTransport = NET_PROTOCOL_TYPE_UDP_V6;
             p_buf_hdr->TransportHdrIx           = protocol_ix;;
             break;


#ifdef  NET_TCP_MODULE_EN
        case NET_IP_HDR_PROTOCOL_TCP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_TCP_V6;
             p_buf_hdr->ProtocolHdrTypeTransport = NET_PROTOCOL_TYPE_TCP_V6;
             p_buf_hdr->TransportHdrIx           = protocol_ix;
             break;
#endif


        case NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP:
             p_buf_hdr->IPv6_HopByHopHdrIx       = protocol_ix;
                                                                /* See Note 3.                                          */
             if ((p_buf_hdr->IPv6_HopByHopHdrIx - p_buf_hdr->IP_HdrIx) != NET_IPv6_HDR_SIZE) {
                *p_err = NET_IPv6_ERR_INVALID_EH_OPT_SEQ;
                 return;
             }
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP;
             break;


        case NET_IP_HDR_PROTOCOL_EXT_ROUTING:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_ROUTING;
             p_buf_hdr->IPv6_RoutingHdrIx        = protocol_ix;
             break;


        case NET_IP_HDR_PROTOCOL_EXT_FRAG:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_FRAG;
             p_buf_hdr->IPv6_FragHdrIx           = protocol_ix;
             break;


        case NET_IP_HDR_PROTOCOL_EXT_ESP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_ESP;
             p_buf_hdr->IPv6_ESP_HdrIx           = protocol_ix;
             break;


        case NET_IP_HDR_PROTOCOL_EXT_AUTH:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_AUTH;
             p_buf_hdr->IPv6_AuthHdrIx           = protocol_ix;
             break;


        case NET_IP_HDR_PROTOCOL_EXT_NONE:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_NONE;
#if 0
             p_buf_hdr->IPv6_NoneHdrIx           = protocol_ix;
#endif
             break;


        case NET_IP_HDR_PROTOCOL_EXT_DEST:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_DEST;
             p_buf_hdr->IPv6_DestHdrIx           = protocol_ix;
             break;


        case NET_IP_HDR_PROTOCOL_EXT_MOBILITY:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IP_V6_EXT_MOBILITY;
             p_buf_hdr->IPv6_MobilityHdrIx       = protocol_ix;
             break;

        default:                                                /* See Note 3.                                          */
            *p_err = NET_IPv6_ERR_INVALID_PROTOCOL;
             return;
    }

   (void)&p_buf;

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetIPv6_RxPktProcessExtHdr()
*
* Description : Process the Extension Header(s) found in the received IPv6 packet.
*
* Argument(s) : p_buf       Pointer to a buffer to process the next extension header.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No Errors.
*                               NET_IPv6_ERR_INVALID_EH         Invalid ext hdr num.
*                               NET_IPv6_ERR_INVALID_EH_LEN     Invalid ext hdr len.
*                               NET_IPv6_ERR_INVALID_EH_OPT     Invalid ext hdr option.
*                               NET_IPv6_ERR_INVALID_PROTOCOL   Invalid/unknow protocol type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Rx().
*
* Note(s)     : (1) If, a node encounters a Next Header value of zero (Hop-by-Hop) in any header other
*                   than an IPv6 header or an unrecognized Next Header, it must send an ICMP Parameter Problem
*                   message to the source of the packet, with an ICMP Code value of 1 ("unrecognized Next
*                   Header type encountered") and the ICMP Pointer field containing the offset of the
*                   unrecognized value within the original packet. (See RFC #2460, Section 4.0).
*********************************************************************************************************
*/
static void NetIPv6_RxPktProcessExtHdr(NET_BUF  *p_buf,
                                       NET_ERR  *p_err)
{
    NET_BUF_HDR       *p_buf_hdr;
#ifdef NET_ICMPv6_MODULE_EN
    CPU_INT16U         prev_protocol_ix;
#endif
    CPU_INT16U         protocol_ix;
    NET_IPv6_NEXT_HDR  next_hdr;


    p_buf_hdr         = &p_buf->Hdr;
#ifdef NET_ICMPv6_MODULE_EN
    prev_protocol_ix =  p_buf_hdr->IP_HdrIx + NET_IPv6_HDR_SIZE + p_buf_hdr->IPv6_ExtHdrLen;
#endif
    while ((p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP) ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_ROUTING)    ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_FRAG)       ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_ESP)        ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_AUTH)       ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_NONE)       ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_DEST)       ||
           (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_IP_V6_EXT_MOBILITY)) {

#ifdef NET_ICMPv6_MODULE_EN
        prev_protocol_ix =  p_buf_hdr->IP_HdrIx + NET_IPv6_HDR_SIZE + p_buf_hdr->IPv6_ExtHdrLen;
#endif
                                                                /* Demux Ext Hdr fct.                                   */
        switch (p_buf_hdr->ProtocolHdrType) {
            case NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP:
                 protocol_ix = NetIPv6_RxOptHdr(p_buf, &next_hdr, NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_DEST:
                 protocol_ix = NetIPv6_RxOptHdr(p_buf, &next_hdr, NET_PROTOCOL_TYPE_IP_V6_EXT_DEST, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_ROUTING:
                 protocol_ix = NetIPv6_RxRoutingHdr(p_buf, &next_hdr, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_FRAG:
                 protocol_ix = NetIPv6_RxFragHdr(p_buf, &next_hdr, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_ESP:
                 protocol_ix = NetIPv6_RxESP_Hdr(p_buf, &next_hdr, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_AUTH:
                 protocol_ix = NetIPv6_RxAuthHdr(p_buf, &next_hdr, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_MOBILITY:
                 protocol_ix = NetIPv6_RxMobilityHdr(p_buf, &next_hdr, p_err);
                 break;


            case NET_PROTOCOL_TYPE_IP_V6_EXT_NONE:
                *p_err = NET_IPv6_ERR_NONE;
                 return;


            default:
#ifdef NET_ICMPv6_MODULE_EN
                                                                    /* Send ICMP Parameter Problem Msg (See Note 2).    */
                 NetICMPv6_TxMsgErr(p_buf,
                                    NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                    NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_NEXT_HDR,
                                   (prev_protocol_ix - p_buf_hdr->IP_HdrIx),
                                    p_err);
#endif
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
                *p_err = NET_IPv6_ERR_INVALID_EH;
                 return;
        }

        switch (*p_err) {
            case NET_IPv6_ERR_NONE:
                 break;


            case NET_IPv6_ERR_INVALID_EH:
            case NET_IPv6_ERR_INVALID_EH_LEN:
            case NET_IPv6_ERR_INVALID_EH_OPT:
            default:
                 return;
        }

        NetIPv6_RxPktValidateNextHdr(p_buf,
                                     p_buf_hdr,
                                     next_hdr,
                                     protocol_ix,
                                     p_err);

        switch (*p_err) {
            case NET_IPv6_ERR_NONE:
                 break;


            case NET_IPv6_ERR_INVALID_EH_OPT_SEQ:
            case NET_IPv6_ERR_INVALID_PROTOCOL:
            default:
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvProtocolCtr);
#ifdef  NET_ICMPv6_MODULE_EN
                 NetICMPv6_TxMsgErr(p_buf,
                                    NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                    NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_NEXT_HDR,
                                   (prev_protocol_ix - p_buf_hdr->IP_HdrIx),
                                    p_err);
#endif
                *p_err = NET_IPv6_ERR_INVALID_PROTOCOL;
                 return;
        }
    }
}


/*
*********************************************************************************************************
*                                          NetIPv6_RxOptHdr()
*
* Description : Validate and process a received Hop-By-Hop or Destination Header.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to variable that will receive the next header from this function.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No error.
*                               NET_ERR_FAULT_NULL_PTR           p_next_hdr is null.
*                               NET_IPv6_ERR_INVALID_EH_OPT     Header option is unhandled.
*
* Return(s)   : Index of the next extension header or upper layer protocol, if NO error(s).
*
*               0u                                                          otherwise.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : (1) Hop-by-Hop extension header and Destination header have an Option field based on the
*                   TLV model (Type-Length-Value). See 'net_ipv6.h' EXTENSION HEADER TLV OPTION DATA TYPE.
*********************************************************************************************************
*/
static  CPU_INT16U  NetIPv6_RxOptHdr(NET_BUF            *p_buf,
                                     NET_IPv6_NEXT_HDR  *p_next_hdr,
                                     NET_PROTOCOL_TYPE   proto_type,
                                     NET_ERR            *p_err)
{
#if 0
    CPU_BOOLEAN              change_en;
#endif
    CPU_INT08U              *p_data;
    NET_BUF_HDR             *p_buf_hdr;
    NET_IPv6_OPT_HDR        *p_opt_hdr;
    NET_IPv6_EXT_HDR_TLV    *p_tlv;
    CPU_INT16U               ext_hdr_ix;
    NET_IPv6_TLV_TYPE        action;
    NET_IPv6_OPT_TYPE        opt_type;
    CPU_INT16U               eh_len;
    CPU_INT16U               next_hdr_offset;
    CPU_INT16U               next_tlv_offset;
    CPU_BOOLEAN              dest_addr_multicast;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_next_hdr == (NET_IPv6_NEXT_HDR *)0u) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
#endif

    p_buf_hdr = &p_buf->Hdr;                                    /* Get ptr to buf hdr.                                  */

    switch (proto_type) {
        case NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP:
             ext_hdr_ix = p_buf_hdr->IPv6_HopByHopHdrIx;
             break;


        case NET_PROTOCOL_TYPE_IP_V6_EXT_DEST:
             ext_hdr_ix = p_buf_hdr->IPv6_DestHdrIx;
             break;


        default:
            *p_err = NET_IPv6_ERR_INVALID_PROTOCOL;
             return (0u);
    }

    p_data                    =  p_buf->DataPtr;
                                                                /* Get opt hdr from ext hdr data space.                 */
    p_opt_hdr                 = (NET_IPv6_OPT_HDR *)&p_data[ext_hdr_ix];

                                                                /* */
    eh_len                    = (p_opt_hdr->HdrLen + 1) * NET_IPv6_EH_ALIGN_SIZE;
    p_buf_hdr->DataLen        -= eh_len;
    p_buf_hdr->IPv6_ExtHdrLen += eh_len;

    next_hdr_offset           = ext_hdr_ix + eh_len;
    next_tlv_offset           = 0u;

    while (next_tlv_offset < (eh_len - 2u)) {

        p_tlv     = (NET_IPv6_EXT_HDR_TLV  *)&p_opt_hdr->Opt[next_tlv_offset];

        action    =  p_tlv->Type & NET_IPv6_EH_TLV_TYPE_ACT_MASK;
#if 0
        change_en = (p_tlv->Type & NET_IPv6_EH_TLV_TYPE_CHG_MASK) >> NET_IPv6_EH_TLV_TYPE_CHG_SHIFT;
#endif
        opt_type  =  p_tlv->Type & NET_IPv6_EH_TLV_TYPE_OPT_MASK;

        switch (opt_type) {
            case NET_IPv6_EH_TYPE_PAD1:
                 break;


            case NET_IPv6_EH_TYPE_PADN:
                 break;



            case NET_IPv6_EH_TYPE_ROUTER_ALERT:
                 break;


            default:
                 switch (action) {
                                                                /* Skip over opt & continue processing hdr.             */
                     case NET_IPv6_EH_TLV_TYPE_ACT_SKIP:
                          break;

                                                                /* Discard pkt.                                         */
                     case NET_IPv6_EH_TLV_TYPE_ACT_DISCARD:
                          NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
                         *p_err = NET_IPv6_ERR_INVALID_EH_OPT;
                          return (0u);

                                                                /* Discard pkt & send ICMP Param Problem code 2 msg.    */
                     case NET_IPv6_EH_TLV_TYPE_ACT_DISCARD_IPPM:
#ifdef  NET_ICMPv6_MODULE_EN
                          NetICMPv6_TxMsgErr(p_buf,
                                             NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                             NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_OPT,
                                            ((ext_hdr_ix + 2) - p_buf_hdr->IP_HdrIx),
                                             p_err);
#endif
                          NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
                         *p_err = NET_IPv6_ERR_INVALID_EH_OPT;
                          return (0u);
                                                                /* Discard pkt & send ICMP Param Problem code 2 msg ... */
                                                                /* ... if pkt dest is not multicast.                    */
                     case NET_IPv6_EH_TLV_TYPE_ACT_DISCARD_IPPM_MC:
                          p_buf_hdr = &p_buf->Hdr;
                          dest_addr_multicast = NetIPv6_IsAddrMcast(&p_buf_hdr->IPv6_AddrDest);
                          if (dest_addr_multicast == DEF_NO) {
#ifdef  NET_ICMPv6_MODULE_EN
                              NetICMPv6_TxMsgErr(p_buf,
                                                 NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                                 NET_ICMPv6_MSG_CODE_PARAM_PROB_BAD_OPT,
                                                ((ext_hdr_ix + 2) - p_buf_hdr->IP_HdrIx),
                                                 p_err);
#endif
                          }
                          NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
                         *p_err = NET_IPv6_ERR_INVALID_EH_OPT;
                          return (0u);
                 }
                 break;
        }

        if (opt_type == NET_IPv6_EH_TYPE_PAD1) {                /* The format of the Pad1 opt is a special case, it ... */
            next_tlv_offset++;                                  /* ... doesn't have len and value fields.               */
        } else {
            next_tlv_offset += p_tlv->Len + 2u;
        }
    }

   *p_next_hdr = p_opt_hdr->NextHdr;
   *p_err      = NET_IPv6_ERR_NONE;
    return (next_hdr_offset);
}


/*
*********************************************************************************************************
*                                        NetIPv6_RxRoutingHdr()
*
* Description : Process the received Routing extension header.
*
* Argument(s) : p_buf       Pointer to Received buffer.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to IPv6 next header object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                   Routing Extension header successfully processed.
*                               NET_IPv6_ERR_INVALID_EH_OPT_SEQ     Invalid Extension header.
*
* Return(s)   : Index in buffer of the IPv6 Next header.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : (1) Routing Extension Header is not yet supported. Therefore the function does not
*                   process the header and only set the pointer to the next header.
*********************************************************************************************************
*/
static  CPU_INT16U  NetIPv6_RxRoutingHdr (NET_BUF            *p_buf,
                                          NET_IPv6_NEXT_HDR  *p_next_hdr,
                                          NET_ERR            *p_err)
{
    CPU_INT08U            *p_data;
    NET_BUF_HDR           *p_buf_hdr;
    NET_IPv6_ROUTING_HDR  *prouting_hdr;
    CPU_INT16U             next_protocol_ix;
    CPU_INT16U             hdr_len;


    p_buf_hdr    = &p_buf->Hdr;                                 /* Get ptr to buf hdr.                                  */
    p_data       =  p_buf->DataPtr;

                                                                /* Get routing hdr from pkt data space.                 */
    prouting_hdr = (NET_IPv6_ROUTING_HDR *)&p_data[p_buf_hdr->IPv6_RoutingHdrIx];
    hdr_len      = (prouting_hdr->HdrLen + 1) * NET_IPv6_EH_ALIGN_SIZE;

                                                                /* Calculate next hdr ptr.                              */
    next_protocol_ix                = p_buf_hdr->IPv6_RoutingHdrIx + hdr_len;
    p_buf_hdr->DataLen              -=                               hdr_len;
    p_buf_hdr->IPv6_ExtHdrLen       +=                               hdr_len;

    switch (prouting_hdr->RoutingType) {
        case NET_IPv6_EH_ROUTING_TYPE_0:
        case NET_IPv6_EH_ROUTING_TYPE_1:
        case NET_IPv6_EH_ROUTING_TYPE_2:
            break;                                              /* No processing of Routing Header for now.             */

        default:
            if (prouting_hdr->SegLeft != 0) {
#ifdef  NET_ICMPv6_MODULE_EN
                NetICMPv6_TxMsgErr(p_buf,
                                   NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                                   NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR,
                                 ((p_buf_hdr->IPv6_RoutingHdrIx + 2) - p_buf_hdr->IP_HdrIx),
                                   p_err);
#endif
                NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvOptsCtr);
               *p_err = NET_IPv6_ERR_INVALID_EH_OPT_SEQ;
                return (0u);
            }
    }


   *p_next_hdr = prouting_hdr->NextHdr;
   *p_err      = NET_IPv6_ERR_NONE;
    return (next_protocol_ix);
}


/*
*********************************************************************************************************
*                                          NetIPv6_RxFragHdr()
*
* Description : Process the received Fragment extension header.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to IPv6 next header object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Fragment Extension Header successfully processed.
*
* Return(s)   : Index in buffer of the IPv6 Next header.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : none.
*********************************************************************************************************
*/
static  CPU_INT16U  NetIPv6_RxFragHdr (NET_BUF            *p_buf,
                                       NET_IPv6_NEXT_HDR  *p_next_hdr,
                                       NET_ERR            *p_err)
{
    CPU_INT08U         *p_data;
    NET_BUF_HDR        *p_buf_hdr;
    NET_IPv6_FRAG_HDR  *p_frag_hdr;
    CPU_INT16U          next_protocol_ix;


    p_buf_hdr  = &p_buf->Hdr;                                     /* Get ptr to buf hdr.                                  */
    p_data     =  p_buf->DataPtr;

                                                                /* Get frag hdr from pkt data space.                    */
    p_frag_hdr = (NET_IPv6_FRAG_HDR *)&p_data[p_buf_hdr->IPv6_FragHdrIx];

                                                                /* Get frag flag & ID.                                  */
    p_buf_hdr->IPv6_Flags_FragOffset = NET_UTIL_NET_TO_HOST_16(p_frag_hdr->FragOffsetFlag);
    p_buf_hdr->IPv6_ID               = NET_UTIL_NET_TO_HOST_32(p_frag_hdr->ID);

                                                                /* Calculate next hdr ptr.                              */
    next_protocol_ix                = p_buf_hdr->IPv6_FragHdrIx + NET_IPv6_FRAG_HDR_SIZE;
    p_buf_hdr->DataLen              -=                            NET_IPv6_FRAG_HDR_SIZE;
    p_buf_hdr->IPv6_ExtHdrLen       +=                            NET_IPv6_FRAG_HDR_SIZE;


   *p_next_hdr = p_frag_hdr->NextHdr;
   *p_err      = NET_IPv6_ERR_NONE;
    return (next_protocol_ix);
}


/*
*********************************************************************************************************
*                                          NetIPv6_RxESP_Hdr()
*
* Description : Process the received ESP (Encapsulation Security Payload) extension header.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to IPv6 next header object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : Index in buffer of the IPv6 Next header.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : (1) ESP Extension Header is not yet supported.
*********************************************************************************************************
*/
static  CPU_INT16U  NetIPv6_RxESP_Hdr (NET_BUF            *p_buf,
                                       NET_IPv6_NEXT_HDR  *p_next_hdr,
                                       NET_ERR            *p_err)
{
   (void)&p_buf;
   (void)&p_next_hdr;

   *p_err = NET_IPv6_ERR_INVALID_EH;

    return (0u);
}


/*
*********************************************************************************************************
*                                          NetIPv6_RxAuthHdr()
*
* Description : Process the received Authentication Extension Header.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to IPv6 next header object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : Index in buffer of the IPv6 Next header.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : (1) Authentication Extension Header is not yet supported. Therefore the function does not
*                   process the header and only set the pointer to the next header.
*********************************************************************************************************
*/
static  CPU_INT16U  NetIPv6_RxAuthHdr (NET_BUF            *p_buf,
                                       NET_IPv6_NEXT_HDR  *p_next_hdr,
                                       NET_ERR            *p_err)
{
    NET_BUF_HDR                    *p_buf_hdr;
    CPU_INT08U                     *p_data;
    NET_IPv6_AUTHENTICATION_HDR    *pauth_hdr;
    CPU_INT16U                      eh_len;
    CPU_INT16U                      next_hdr_offset;

                                                                /* Get ptr to buf hdr.                                  */
    p_buf_hdr  = &p_buf->Hdr;
    p_data     =  p_buf->DataPtr;

                                                                /* Get Auth hdr from pkt data space.                    */
    pauth_hdr = (NET_IPv6_AUTHENTICATION_HDR *)&p_data[p_buf_hdr->IP_HdrIx];

    eh_len    = ((pauth_hdr->HdrLen + 2)>>1) * NET_IPv6_EH_ALIGN_SIZE;
    p_buf_hdr->DataLen        -= eh_len;
    p_buf_hdr->IPv6_ExtHdrLen += eh_len;

    next_hdr_offset = p_buf_hdr->IPv6_AuthHdrIx + eh_len;

   *p_next_hdr = pauth_hdr->NextHdr;
   *p_err      = NET_IPv6_ERR_NONE;

    return (next_hdr_offset);
}


/*
*********************************************************************************************************
*                                          NetIPv6_RxNoneHdr()
*
* Description : Process the received None Extension Header.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to IPv6 next header object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : Index in buffer of the IPv6 Next header.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if 0
static  CPU_INT16U  NetIPv6_RxNoneHdr (NET_BUF            *p_buf,
    NET_CTR_ERR_INC(Net_ErrCtrs.NetIPv6_ErrRxHdrOptsCtr);
                                       NET_IPv6_NEXT_HDR  *p_next_hdr,
                                       NET_ERR            *p_err)
{
    (void)&p_buf;
    (void)&p_next_hdr;
    (void)&p_err;

    return (0u);
}
#endif

/*
*********************************************************************************************************
*                                        NetIPv6_RxMobilityHdr()
*
* Description : Process the received Mobility Extension Header.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_next_hdr  Pointer to IPv6 next header object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : Index in buffer of the IPv6 Next header.
*
* Caller(s)   : NetIPv6_RxPktProcessExtHdr().
*
* Note(s)     : (1) IPv6 Mobility Extension header is not yet supported.
*********************************************************************************************************
*/

static  CPU_INT16U  NetIPv6_RxMobilityHdr (NET_BUF            *p_buf,
                                           NET_IPv6_NEXT_HDR  *p_next_hdr,
                                           NET_ERR            *p_err)
{
   (void)&p_buf;
   (void)&p_next_hdr;

   *p_err = NET_IPv6_ERR_INVALID_EH;

    return (0u);
}


/*
*********************************************************************************************************
*                                      NetIPv6_RxPktFragReasm()
*
* Description : (1) Reassemble any IPv6 datagram fragments :
*
*                   (a) Determine if received IPv6 packet is a fragment
*                   (b) Reassemble IPv6 fragments, when possible
*
*               (2) (a) Received fragments are reassembled by sorting datagram fragments into fragment lists
*                       (also known as 'Fragmented Datagrams') grouped by the following IPv6 header fields :
*
*                       (1) Source      Address
*                       (2) Destination Address
*                       (3) Identification
*
*                   (b) Fragment lists are linked to form a list of Fragment Lists/Fragmented Datagrams.
*
*                       (1) In the diagram below, ... :
*
*                           (A) The top horizontal row  represents the list of fragment lists.
*
*                           (B) Each    vertical column represents the fragments in the same fragment list/
*                               Fragmented Datagram.
*
*                           (C) (1) 'NetIPv6_FragReasmListsHead' points to the head of the Fragment Lists;
*                               (2) 'NetIPv6_FragReasmListsTail' points to the tail of the Fragment Lists.
*
*                           (D) Fragment buffers' 'PrevPrimListPtr' & 'NextPrimListPtr' doubly-link each fragment
*                               list's head buffer to form the list of Fragment Lists.
*
*                           (E) Fragment buffer's 'PrevBufPtr'      & 'NextBufPtr'      doubly-link each fragment
*                               in a fragment list.
*
*                       (2) (A) For each received fragment, all fragment lists are searched in order to insert the
*                               fragment into the ap_propriate fragment list--i.e. the fragment list with identical
*                               fragment list IPv6 header field values (see Note #2a).
*
*                           (B) If a received fragment is the first fragment with its specific fragment list IPv6
*                               header field values, the received fragment starts a new fragment list which is
*                               added at the tail of the Fragment Lists.
*
*                               See also Note #3b2.
*
*                           (C) To expedite faster fragment list searches :
*
*                               (1) (a) Fragment lists are added             at the tail of the Fragment Lists;
*                                   (b) Fragment lists are searched starting at the head of the Fragment Lists.
*
*                               (2) As fragments are received & processed into fragment lists, older fragment
*                                   lists migrate to the head of the Fragment Lists.  Once a fragment list is
*                                   reassembled or discarded, it is removed from the Fragment Lists.
*
*                       (3) Fragment buffer size is irrelevant & ignored in the fragment reassembly procedure--
*                           i.e. the procedure functions correctly regardless of the buffer sizes used for any &
*                           all received fragments.
*
*
*
*                                        |                   List of                     |
*                                        |<----------- Fragmented Datagrams ------------>|
*                                        |               (see Note #2b1A)                |
*
*                               Oldest Fragmented Datagram                    New fragment lists
*                                   in Fragment Lists                          inserted at tail
*                                   (see Note #2b2C2)                         (see Note #2b2C1a)
*
*                                           |        NextPrimListPtr                  |
*                                           |       (see Note #2b1D)                  |
*                                           v                   |                     v
*                                                               |
*             ---         Head of        -------       -------  v    -------       -------  (see Note #2b1C2)
*              ^          Fragment  ---->|     |------>|     |------>|     |------>|     |
*              |           Lists         |     |       |     |       |     |       |     |       Tail of
*              |                         |     |<------|     |<------|     |<------|     |<----  Fragment
*              |     (see Note #2b1C1)   |     |       |     |  ^    |     |       |     |        Lists
*              |                         |     |       |     |  |    |     |       |     |
*              |                         -------       -------  |    -------       -------
*              |                           | ^                  |      | ^
*              |                           | |       PrevPrimListPtr   | |
*              |                           v |      (see Note #2b1D)   v |
*              |                         -------                     -------
*                                        |     |                     |     |
*     Fragments in the same              |     |                     |     |
*      Fragmented Datagram               |     |                     |     |
*       (see Note #2b1B)                 |     |                     |     |
*                                        |     |                     |     |        Fragments in a fragment
*              |                         -------                     -------        list may use different
*              |                           | ^                         | ^           size network buffers
*              |           NextBufPtr ---> | | <--- PrevBufPtr         | |             (see Note #2b3)
*              |        (see Note #2b1E)   v |   (see Note #2b1E)      v |
*              |                         -------                     -------        The last fragment in a
*              |                         |     |                     |     |  <--- fragment list may likely
*              |                         |     |                     |     |       use a smaller buffer size
*              |                         |     |                     -------
*              |                         |     |
*              v                         |     |
*             ---                        -------
*
*
*
* Argument(s) : p_buf        Pointer to network buffer that received IPv6 packet.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_buf_hdr    Pointer to network buffer header.
*               --------    Argument validated in NetIPv6_Rx().
*
*               p_ip_hdr     Pointer to received packet's IPv6 header.
*               -------     Argument checked   in NetIPv6_RxPktValidate().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_RX_FRAG_NONE       Datagram complete; NOT a fragment (see Note #3b1B).
*
*                                                               ------- RETURNED BY NetIPv6_RxPktFragListAdd() : --------
*                                                               ------ RETURNED BY NetIPv6_RxPktFragListInsert() : ------
*                               NET_IPv6_ERR_RX_FRAG_REASM      Fragments in reassembly progress.
*                               NET_IPv6_ERR_RX_FRAG_DISCARD    Fragment &/or datagram discarded.
*                               NET_IPv6_ERR_RX_FRAG_OFFSET     Invalid fragment offset.
*                               NET_IPv6_ERR_RX_FRAG_SIZE       Invalid fragment size.
*                               NET_IPv6_ERR_RX_FRAG_LEN_TOT    Invalid fragment   datagram total IPv6 length.
*
*                                                               ------ RETURNED BY NetIPv6_RxPktFragListInsert() : ------
*                               NET_IPv6_ERR_RX_FRAG_COMPLETE   Datagram complete; fragments reassembled (see Note #3b3).
*                               NET_IPv6_ERR_RX_FRAG_SIZE_TOT   Invalid fragmented datagram total size.
*
* Return(s)   : Pointer to reassembled datagram, if fragment reassembly complete.
*
*               Pointer to NULL,                 if fragment reassembly in progress.
*
*               Pointer to fragment buffer,      for any fragment discard error.
*
* Caller(s)   : NetIPv6_Rx().
*
* Note(s)     : (3) (a) RFC #2460, Section 4.5 'Fragment Header' states that the following IPv6 header
*                       fields are "used together ... to identify datagram fragments for reassembly" :
*
*                       (1) Internet identification field (ID)
*                       (2) source      address
*                       (3) destination address
*
*               (4) (a) Fragment lists are accessed by :
*
*                       (1) NetIPv6_RxPktFragReasm()
*                       (2) Fragment list's 'TMR->Obj' pointer      during execution of NetIPv6_RxPktFragTimeout()
*
*                   (b) Since the primary tasks of the network protocol suite are prevented from running
*                       concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                       resources of the fragment lists since no asynchronous access from other network
*                       tasks is possible.
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv6_RxPktFragReasm (NET_BUF        *p_buf,
                                          NET_BUF_HDR    *p_buf_hdr,
                                          NET_IPv6_HDR   *p_ip_hdr,
                                          NET_ERR        *p_err)
{
#if 0
    NET_IPv6_HDR   *p_frag_list_ip_hdr;
#endif
    CPU_BOOLEAN     frag;
    CPU_BOOLEAN     frag_done;
    CPU_BOOLEAN     ip_flag_frags_more;
    CPU_BOOLEAN     addr_cmp;
    CPU_INT16U      ip_flags;
    CPU_INT16U      frag_offset;
    CPU_INT16U      frag_size;
    NET_BUF        *p_frag;
    NET_BUF        *p_frag_list;
    NET_BUF_HDR    *p_frag_list_buf_hdr;


                                                                /* -------------- CHK FRAG REASM REQUIRED ------------- */
    frag = DEF_NO;

    ip_flags           = p_buf_hdr->IPv6_Flags_FragOffset & NET_IPv6_FRAG_FLAGS_MASK;
    ip_flag_frags_more = DEF_BIT_IS_SET(ip_flags, NET_IPv6_FRAG_FLAG_FRAG_MORE);
    if (ip_flag_frags_more != DEF_NO) {                         /* If 'More Frags' set (see Note #3b1A2), ...           */
        frag  = DEF_YES;                                        /* ... mark as frag.                                    */
    }

    frag_offset = p_buf_hdr->IPv6_Flags_FragOffset & NET_IPv6_FRAG_OFFSET_MASK;
    if (frag_offset != NET_IPv6_FRAG_OFFSET_NONE) {             /* If frag offset != 0 (see Note #3b1A1), ...           */
        frag  = DEF_YES;                                        /* ... mark as frag.                                    */
    }

    if (frag != DEF_YES) {                                      /* If pkt NOT a frag, ...                               */
       *p_err  = NET_IPv6_ERR_RX_FRAG_NONE;
        return (p_buf);                                          /* ... rtn non-frag'd datagram (see Note #3b1B).        */
    }

    NET_CTR_STAT_INC(Net_StatCtrs.IPv6.RxFragCtr);


                                                                /* ------------------- REASM FRAGS -------------------- */
    frag_size   = p_buf_hdr->IP_TotLen - p_buf_hdr->IPv6_ExtHdrLen;
    p_frag_list = NetIPv6_FragReasmListsHead;
    frag_done   = DEF_NO;

    while (frag_done == DEF_NO) {                               /* Insert frag into a frag list.                        */

        if (p_frag_list != (NET_BUF *)0) {                      /* Srch ALL existing frag lists first (see Note #2b2A). */
            p_frag_list_buf_hdr =                 &p_frag_list->Hdr;
#if 0
            p_frag_list_ip_hdr  = (NET_IPv6_HDR *)&p_frag_list->DataPtr[p_frag_list_buf_hdr->IP_HdrIx];
#endif
                                                                                /* If frag & this frag list's    ...    */

            addr_cmp = Mem_Cmp(&p_buf_hdr->IPv6_AddrSrc, &p_frag_list_buf_hdr->IPv6_AddrSrc, NET_IPv6_ADDR_SIZE);

            if (addr_cmp == DEF_YES) {                                          /* ... src  addr (see Note #2a1) ...    */
                addr_cmp = Mem_Cmp(&p_buf_hdr->IPv6_AddrDest, &p_frag_list_buf_hdr->IPv6_AddrDest, NET_IPv6_ADDR_SIZE);
                if (addr_cmp == DEF_YES) {                                      /* ... dest addr (see Note #2a2) ...    */
                    if (p_buf_hdr->IPv6_ID    == p_frag_list_buf_hdr->IPv6_ID) {/* ... ID        (see Note #2a3) ...    */
#if 0
                        if (p_ip_hdr->NextHdr == p_frag_list_ip_hdr->NextHdr) { /* ... next hdr  (see Note #2a4) ...    */
                                                                                /* ... fields identical,         ...    */
#endif
                        p_frag    = NetIPv6_RxPktFragListInsert(p_buf,          /* ... insert frag into frag list.      */
                                                                p_buf_hdr,
                                                                ip_flags,
                                                                frag_offset,
                                                                frag_size,
                                                                p_frag_list,
                                                                p_err);
                        frag_done = DEF_YES;
#if 0
                        }
#endif
                    }
                }
            }

            if (frag_done != DEF_YES) {                         /* If NOT done, adv to next frag list.                  */
                p_frag_list = p_frag_list_buf_hdr->NextPrimListPtr;
            }

        } else {                                                /* Else add new frag list (see Note #2b2B).             */
            p_frag    = NetIPv6_RxPktFragListAdd(p_buf,
                                                 p_buf_hdr,
                                                 ip_flags,
                                                 frag_offset,
                                                 frag_size,
                                                 p_err);
            frag_done = DEF_YES;
        }
    }

   (void)&p_ip_hdr;

    return (p_frag);
}


/*
*********************************************************************************************************
*                                     NetIPv6_RxPktFragListAdd()
*
* Description : (1) Add fragment as new fragment list at end of Fragment Lists :
*
*                   (a) Get    fragment reassembly timer
*                   (b) Insert fragment into Fragment Lists
*                   (c) Update fragment list reassembly calculations
*
*
* Argument(s) : p_buf           Pointer to network buffer that received fragment.
*               ----            Argument checked   in NetIPv6_Rx().
*
*               p_buf_hdr       Pointer to network buffer header.
*               --------        Argument validated in NetIPv6_Rx().
*
*               frag_ip_flags   Fragment IPv6 header flags.
*               -------------   Argument validated in NetIPv6_RxPktFragReasm().
*
*               frag_offset     Fragment offset.
*
*               frag_size       Fragment size (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_RX_FRAG_REASM      Fragment reassembly in progress.
*                               NET_IPv6_ERR_RX_FRAG_DISCARD    Fragment discarded.
*                               NET_IPv6_ERR_RX_FRAG_OFFSET     Invalid  fragment offset.
*                               NET_IPv6_ERR_RX_FRAG_SIZE       Invalid  fragment size.
*
*                                                               - RETURNED BY NetIPv6_RxPktFragListUpdate() : -
*                               NET_IPv6_ERR_RX_FRAG_LEN_TOT    Invalid fragment datagram total IPv6 length.
*
* Return(s)   : Pointer to NULL,            if fragment added as new fragment list.
*
*               Pointer to fragment buffer, for any fragment discard error.
*
* Caller(s)   : NetIPv6_RxPktFragReasm().
*
* Note(s)     : (2) (a) RFC #2460, Section 4.5 'Fragment Header' states that "if an internet
*                       datagram is fragmented" :
*
*                       (1) (A) "Fragments are counted in units of 8 octets."
*                           (B) "The minimum fragment is 8 octets."
*
*                       (2) (A) However, this CANNOT apply "if this is the last fragment" ...
*                               (1) "(that is the more fragments field is zero)";         ...
*                           (B) Which may be of ANY size.
*
*               (3) During fragment list insertion, some fragment buffer controls were previously initialized
*                   in NetBuf_Get() when the packet was received at the network interface layer.  These buffer
*                   controls do NOT need to be re-initialized but are shown for completeness.
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv6_RxPktFragListAdd (NET_BUF      *p_buf,
                                            NET_BUF_HDR  *p_buf_hdr,
                                            CPU_INT16U    frag_ip_flags,
                                            CPU_INT32U    frag_offset,
                                            CPU_INT32U    frag_size,
                                            NET_ERR      *p_err)
{
    CPU_BOOLEAN    ip_flag_frags_more;
    CPU_INT16U     frag_size_min;
    NET_TMR_TICK   timeout_tick;
    NET_ERR        tmr_err;
    NET_BUF       *p_frag;
    NET_BUF_HDR   *p_frag_list_tail_buf_hdr;
    CPU_SR_ALLOC();


                                                                /* ------------------- VALIDATE FRAG ------------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (frag_offset > NET_IPv6_FRAG_OFFSET_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvFragOffsetCtr);
       *p_err =  NET_IPv6_ERR_RX_FRAG_OFFSET;
        return (p_buf);
    }
#endif

    ip_flag_frags_more =  DEF_BIT_IS_SET(frag_ip_flags, NET_IPv6_FRAG_FLAG_FRAG_MORE);
    frag_size_min      = (ip_flag_frags_more == DEF_YES) ? NET_IPv6_FRAG_SIZE_MIN_FRAG_MORE
                                                         : NET_IPv6_FRAG_SIZE_MIN_FRAG_LAST;
    if (frag_size < frag_size_min) {                            /* See Note #2a.                                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragSizeCtr);
       *p_err =  NET_IPv6_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }

    if (frag_size > NET_IPv6_FRAG_SIZE_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragSizeCtr);
       *p_err =  NET_IPv6_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }

    if ((ip_flag_frags_more                    == DEF_YES) &&
        ((frag_size % NET_IPv6_FRAG_SIZE_UNIT) != 0u)) {
#ifdef  NET_ICMPv6_MODULE_EN
        NetICMPv6_TxMsgErr(p_buf,
                           NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv6_PTR_IX_IP_PAYLOAD_LEN,
                           p_err);
#endif
        *p_err =  NET_IPv6_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }


                                                                /* ------------------- GET FRAG TMR ------------------- */
    CPU_CRITICAL_ENTER();
    timeout_tick      = NetIPv6_FragReasmTimeout_tick;
    CPU_CRITICAL_EXIT();
    p_buf_hdr->TmrPtr = NetTmr_Get((CPU_FNCT_PTR) NetIPv6_RxPktFragTimeout,
                                   (void       *) p_buf,
                                   (NET_TMR_TICK) timeout_tick,
                                   (NET_ERR    *)&tmr_err);
    if (tmr_err != NET_TMR_ERR_NONE) {                          /* If tmr unavail, discard frag.                        */
       *p_err =  NET_IPv6_ERR_RX_FRAG_DISCARD;
        return (p_buf);
    }


                                                                /* ------------ INSERT FRAG INTO FRAG LISTS ----------- */
    if (NetIPv6_FragReasmListsTail != (NET_BUF *)0) {           /* If frag lists NOT empty, insert @ tail.              */
        p_buf_hdr->PrevPrimListPtr                = (NET_BUF     *) NetIPv6_FragReasmListsTail;
        p_frag_list_tail_buf_hdr                  = (NET_BUF_HDR *)&NetIPv6_FragReasmListsTail->Hdr;
        p_frag_list_tail_buf_hdr->NextPrimListPtr = (NET_BUF     *) p_buf;
        NetIPv6_FragReasmListsTail                = (NET_BUF     *) p_buf;

    } else {                                                    /* Else add frag as first frag list.                    */
        NetIPv6_FragReasmListsHead                = (NET_BUF     *) p_buf;
        NetIPv6_FragReasmListsTail                = (NET_BUF     *) p_buf;
        p_buf_hdr->PrevPrimListPtr                = (NET_BUF     *) 0;
    }

#if 0                                                           /* Init'd in NetBuf_Get() [see Note #3].                */
    p_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
    p_buf_hdr->PrevBufPtr      = (NET_BUF *)0;
    p_buf_hdr->NextBufPtr      = (NET_BUF *)0;
    p_buf_hdr->IP_FragSizeTot  =  NET_IPv6_FRAG_SIZE_NONE;
    p_buf_hdr->IP_FragSizeCur  =  0u;
#endif


                                                                /* ----------------- UPDATE FRAG CALCS ---------------- */
    NetIPv6_RxPktFragListUpdate(p_buf,
                                p_buf_hdr,
                                frag_ip_flags,
                                frag_offset,
                                frag_size,
                                p_err);
    if (*p_err !=  NET_IPv6_ERR_NONE) {
         return ((NET_BUF *)0);
    }


   *p_err  =  NET_IPv6_ERR_RX_FRAG_REASM;
    p_frag = (NET_BUF *)0;

    return (p_frag);
}


/*
*********************************************************************************************************
*                                    NetIPv6_RxPktFragListInsert()
*
* Description : (1) Insert fragment into corresponding fragment list.
*
*                   (a) Fragments are sorted into fragment lists by fragment offset.
*
*
* Argument(s) : p_buf           Pointer to network buffer that received fragment.
*               ----            Argument checked   in NetIPv6_Rx().
*
*               p_buf_hdr       Pointer to network buffer header.
*               --------        Argument validated in NetIPv6_Rx().
*
*               frag_ip_flags   Fragment IPv6 header flags.
*               -------------   Argument validated in NetIPv6_RxPktFragReasm().
*
*               frag_offset     Fragment offset.
*
*               frag_size       Fragment size (in octets).
*
*               p_frag_list     Pointer to fragment list head buffer.
*               ----------      Argument validated in NetIPv6_RxPktFragReasm().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_RX_FRAG_REASM      Fragment reassembly in progress.
*                               NET_IPv6_ERR_RX_FRAG_DISCARD    Fragment discarded.
*                               NET_IPv6_ERR_RX_FRAG_OFFSET     Invalid  fragment offset.
*                               NET_IPv6_ERR_RX_FRAG_SIZE       Invalid  fragment size.
*
*                                                               -- RETURNED BY NetIPv6_RxPktFragListChkComplete() : -
*                               NET_IPv6_ERR_RX_FRAG_COMPLETE   Datagram complete; fragments reassembled.
*                               NET_IPv6_ERR_RX_FRAG_SIZE_TOT   Invalid fragmented datagram total size.
*
*                                                               --- RETURNED BY NetIPv6_RxPktFragListUpdate() : ---
*                                                               - RETURNED BY NetIPv6_RxPktFragListChkComplete() : -
*                               NET_IPv6_ERR_RX_FRAG_LEN_TOT    Invalid fragment   datagram total IPv6 length.
*
* Return(s)   : Pointer to reassembled datagram, if fragment reassembly complete.
*
*               Pointer to NULL,                 if fragment reassembly in progress.
*
*               Pointer to fragment buffer,      for any fragment discard error.
*
* Caller(s)   : NetIPv6_RxPktFragReasm().
*
* Note(s)     : (2) Assumes ALL fragments in fragment lists have previously been validated for buffer &
*                   IPv6 header fields.
*
*               (3) During fragment list insertion, some fragment buffer controls were previously
*                   initialized in NetBuf_Get() when the packet was received at the network interface
*                   layer.  These buffer controls do NOT need to be re-initialized but are shown for
*                   completeness.
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv6_RxPktFragListInsert (NET_BUF      *p_buf,
                                               NET_BUF_HDR  *p_buf_hdr,
                                               CPU_INT16U    frag_ip_flags,
                                               CPU_INT32U    frag_offset,
                                               CPU_INT32U    frag_size,
                                               NET_BUF      *p_frag_list,
                                               NET_ERR      *p_err)
{
#if 0
    CPU_INT16U    frag_size_cur;
#endif
    CPU_BOOLEAN   ip_flag_frags_more;
    CPU_BOOLEAN   frag_insert_done;
    CPU_BOOLEAN   frag_list_discard;
    CPU_INT16U    frag_offset_actual;
    CPU_INT16U    frag_list_cur_frag_offset;
    CPU_INT16U    frag_list_cur_frag_offset_actual;
    CPU_INT16U    frag_list_prev_frag_offset;
    CPU_INT16U    frag_list_prev_frag_offset_actual;
    CPU_INT16U    frag_size_min;
    NET_BUF      *p_frag;
    NET_BUF      *p_frag_list_prev_buf;
    NET_BUF      *p_frag_list_cur_buf;
    NET_BUF      *p_frag_list_prev_list;
    NET_BUF      *p_frag_list_next_list;
    NET_BUF_HDR  *p_frag_list_buf_hdr;
    NET_BUF_HDR  *p_frag_list_prev_buf_hdr;
    NET_BUF_HDR  *p_frag_list_cur_buf_hdr;
    NET_BUF_HDR  *p_frag_list_prev_list_buf_hdr;
    NET_BUF_HDR  *p_frag_list_next_list_buf_hdr;
    NET_TMR      *p_tmr;
    NET_ERR       err;


                                                                /* ------------------- VALIDATE FRAG ------------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (frag_offset > NET_IPv6_FRAG_OFFSET_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvFragOffsetCtr);
       *p_err =  NET_IPv6_ERR_RX_FRAG_OFFSET;
        return (p_buf);
    }
#endif

    ip_flag_frags_more =  DEF_BIT_IS_SET(frag_ip_flags, NET_IPv6_FRAG_FLAG_FRAG_MORE);
    frag_size_min      = (ip_flag_frags_more == DEF_YES) ? NET_IPv6_FRAG_SIZE_MIN_FRAG_MORE
                                                         : NET_IPv6_FRAG_SIZE_MIN_FRAG_LAST;
    if (frag_size < frag_size_min) {                            /* See Note #2a1.                                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragSizeCtr);
       *p_err =  NET_IPv6_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }

    if (frag_size > NET_IPv6_FRAG_SIZE_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragSizeCtr);
       *p_err =  NET_IPv6_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }

    if ((ip_flag_frags_more                    == DEF_YES) &&
        ((frag_size % NET_IPv6_FRAG_SIZE_UNIT) != 0u)) {
#ifdef  NET_ICMPv6_MODULE_EN
        NetICMPv6_TxMsgErr(p_buf,
                           NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv6_PTR_IX_IP_PAYLOAD_LEN,
                           p_err);
#endif
        *p_err =  NET_IPv6_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }


                                                                        /* ------- INSERT FRAG INTO FRAG LISTS -------- */
    frag_insert_done       =  DEF_NO;

    p_frag_list_cur_buf     =  p_frag_list;
    p_frag_list_cur_buf_hdr = &p_frag_list_cur_buf->Hdr;

    while (frag_insert_done == DEF_NO) {

        frag_list_cur_frag_offset = p_frag_list_cur_buf_hdr->IPv6_Flags_FragOffset & NET_IPv6_FRAG_OFFSET_MASK;
        if (frag_offset > frag_list_cur_frag_offset) {                  /* While frag offset > cur frag offset, ...     */

            if (p_frag_list_cur_buf_hdr->NextBufPtr != (NET_BUF *)0) {   /* ... adv to next frag in list.                */
                p_frag_list_cur_buf     =  p_frag_list_cur_buf_hdr->NextBufPtr;
                p_frag_list_cur_buf_hdr = &p_frag_list_cur_buf->Hdr;

            } else {                                                    /* If @ last frag in list, append frag @ end.   */
                frag_offset_actual               =  frag_offset;
                frag_list_cur_frag_offset_actual =  frag_list_cur_frag_offset
                                                 + (p_frag_list_cur_buf_hdr->IP_TotLen - p_frag_list_cur_buf_hdr->IPv6_ExtHdrLen);

                if (frag_offset_actual >= frag_list_cur_frag_offset_actual) {   /* If frag does NOT overlap, ...        */
                                                                                /* ... append @ end of frag list.       */
                    p_buf_hdr->PrevBufPtr               = (NET_BUF *)p_frag_list_cur_buf;
#if 0                                                                   /* Init'd in NetBuf_Get() [see Note #4].        */
                    p_buf_hdr->NextBufPtr               = (NET_BUF *)0;
                    p_buf_hdr->PrevPrimListPtr          = (NET_BUF *)0;
                    p_buf_hdr->NextPrimListPtr          = (NET_BUF *)0;
                    p_buf_hdr->TmrPtr                   = (NET_TMR *)0;
                    p_buf_hdr->IP_FragSizeTot           =  NET_IPv6_FRAG_SIZE_NONE;
                    p_buf_hdr->IP_FragSizeCur           =  0u;
#endif

                    p_frag_list_cur_buf_hdr->NextBufPtr = (NET_BUF *)p_buf;


                    p_frag_list_buf_hdr = &p_frag_list->Hdr;
                    NetIPv6_RxPktFragListUpdate(p_frag_list,            /* Update frag list reasm calcs.                */
                                                p_frag_list_buf_hdr,
                                                frag_ip_flags,
                                                frag_offset,
                                                frag_size,
                                                p_err);
                    if (*p_err !=  NET_IPv6_ERR_NONE) {
                         return ((NET_BUF *)0);
                    }
                                                                        /* Chk    frag list reasm complete.             */
                    p_frag = NetIPv6_RxPktFragListChkComplete(p_frag_list,
                                                              p_frag_list_buf_hdr,
                                                              p_err);

                } else {                                                /* Else discard overlap frag & datagram.        */
                    NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);
                    NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragDisCtr);
                    p_frag = p_buf;
                   *p_err  = NET_IPv6_ERR_RX_FRAG_DISCARD;
                }

                frag_insert_done = DEF_YES;
            }


        } else if (frag_offset < frag_list_cur_frag_offset) {           /* If frag offset < cur frag offset, ...        */
                                                                        /* ... insert frag into frag list.              */
            frag_list_discard                = DEF_NO;

            frag_offset_actual               = frag_offset + frag_size;
            frag_list_cur_frag_offset_actual = frag_list_cur_frag_offset;

            if (frag_offset_actual > frag_list_cur_frag_offset_actual) {/* If frag overlaps with next frag, ...         */
                frag_list_discard  = DEF_YES;                           /* ... discard frag & datagram (see Note #1b2). */

            } else if (p_frag_list_cur_buf_hdr->PrevBufPtr != (NET_BUF *)0) {
                p_frag_list_prev_buf              =  p_frag_list_cur_buf_hdr->PrevBufPtr;
                p_frag_list_prev_buf_hdr          = &p_frag_list_prev_buf->Hdr;

                frag_offset_actual                =  frag_offset;

                frag_list_prev_frag_offset        =  p_frag_list_prev_buf_hdr->IPv6_Flags_FragOffset & NET_IPv6_FRAG_OFFSET_MASK;
                frag_list_prev_frag_offset_actual =  frag_list_prev_frag_offset
                                                  + (p_frag_list_prev_buf_hdr->IP_TotLen - p_frag_list_prev_buf_hdr->IP_HdrLen);
                                                                        /* If frag overlaps with prev frag, ...         */
                if (frag_offset_actual < frag_list_prev_frag_offset_actual) {
                    frag_list_discard  = DEF_YES;                       /* ... discard frag & datagram (see Note #1b2). */
                }
            } else {
                                                                /* Empty Else Statement                                 */
            }

            if (frag_list_discard == DEF_NO) {                          /* If frag does NOT overlap, ...                */
                                                                        /* ... insert into frag list.                   */
                p_buf_hdr->PrevBufPtr = p_frag_list_cur_buf_hdr->PrevBufPtr;
                p_buf_hdr->NextBufPtr = p_frag_list_cur_buf;

                if (p_buf_hdr->PrevBufPtr != (NET_BUF *)0) {             /* Insert p_buf between prev & cur bufs.         */
                    p_frag_list_prev_buf                     =  p_buf_hdr->PrevBufPtr;
                    p_frag_list_prev_buf_hdr                 = &p_frag_list_prev_buf->Hdr;

                    p_frag_list_prev_buf_hdr->NextBufPtr     =  p_buf;
                    p_frag_list_cur_buf_hdr->PrevBufPtr      =  p_buf;

#if 0                                                                   /* Init'd in NetBuf_Get() [see Note #4].        */
                    p_buf_hdr->PrevPrimListPtr               = (NET_BUF *)0;
                    p_buf_hdr->NextPrimListPtr               = (NET_BUF *)0;
                    p_buf_hdr->TmrPtr                        = (NET_TMR *)0;
                    p_buf_hdr->IP_FragSizeTot                =  NET_IPv6_FRAG_SIZE_NONE;
                    p_buf_hdr->IP_FragSizeCur                =  0u;
#endif

                } else {                                                /* Else p_buf is new frag list head.             */
                    p_frag_list                              =  p_buf;
                                                                        /* Move frag list head info to cur buf ...      */
                                                                        /* ... (see Note #2b1).                         */
                    p_buf_hdr->PrevPrimListPtr               =  p_frag_list_cur_buf_hdr->PrevPrimListPtr;
                    p_buf_hdr->NextPrimListPtr               =  p_frag_list_cur_buf_hdr->NextPrimListPtr;
                    p_buf_hdr->TmrPtr                        =  p_frag_list_cur_buf_hdr->TmrPtr;
                    p_buf_hdr->IP_FragSizeTot                =  p_frag_list_cur_buf_hdr->IP_FragSizeTot;
                    p_buf_hdr->IP_FragSizeCur                =  p_frag_list_cur_buf_hdr->IP_FragSizeCur;

                    p_frag_list_cur_buf_hdr->PrevBufPtr      = (NET_BUF *)p_buf;
                    p_frag_list_cur_buf_hdr->PrevPrimListPtr = (NET_BUF *)0;
                    p_frag_list_cur_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
                    p_frag_list_cur_buf_hdr->TmrPtr          = (NET_TMR *)0;
                    p_frag_list_cur_buf_hdr->IP_FragSizeTot  =  NET_IPv6_FRAG_SIZE_NONE;
                    p_frag_list_cur_buf_hdr->IP_FragSizeCur  =  0u;

                                                                        /* Point tmr            to new frag list head.  */
                    p_tmr                                    = (NET_TMR *)p_buf_hdr->TmrPtr;
                    p_tmr->Obj                               = (void    *)p_buf;

                                                                        /* Point prev frag list to new frag list head.  */
                    p_frag_list_prev_list = p_buf_hdr->PrevPrimListPtr;
                    if (p_frag_list_prev_list != (NET_BUF *)0) {
                        p_frag_list_prev_list_buf_hdr                  = &p_frag_list_prev_list->Hdr;
                        p_frag_list_prev_list_buf_hdr->NextPrimListPtr =  p_buf;
                    } else {
                        NetIPv6_FragReasmListsHead                     =  p_buf;
                    }

                                                                        /* Point next frag list to new frag list head.  */
                    p_frag_list_next_list = p_buf_hdr->NextPrimListPtr;
                    if (p_frag_list_next_list != (NET_BUF *)0) {
                        p_frag_list_next_list_buf_hdr                  = &p_frag_list_next_list->Hdr;
                        p_frag_list_next_list_buf_hdr->PrevPrimListPtr =  p_buf;
                    } else {
                        NetIPv6_FragReasmListsTail                     =  p_buf;
                    }
                }

                p_frag_list_buf_hdr = &p_frag_list->Hdr;
                NetIPv6_RxPktFragListUpdate(p_frag_list,                /* Update frag list reasm calcs.                */
                                            p_frag_list_buf_hdr,
                                            frag_ip_flags,
                                            frag_offset,
                                            frag_size,
                                            p_err);
                if (*p_err !=  NET_IPv6_ERR_NONE) {
                     return ((NET_BUF *)0);
                }
                                                                        /* Chk    frag list reasm complete.             */
                p_frag = NetIPv6_RxPktFragListChkComplete(p_frag_list,
                                                          p_frag_list_buf_hdr,
                                                          p_err);

            } else {                                                    /* Else discard overlap frag & datagram ...     */
                NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... (see Note #1b2).                         */
                NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragDisCtr);
                p_frag = p_buf;
               *p_err  = NET_IPv6_ERR_RX_FRAG_DISCARD;
            }

            frag_insert_done = DEF_YES;


        } else {                                                        /* Else if frag offset = cur frag offset, ...   */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragDisCtr);
            p_frag = p_buf;
           *p_err  = NET_IPv6_ERR_RX_FRAG_DISCARD;                       /* ... discard duplicate frag (see Note #1b1).  */

#if 0
            frag_size_cur  = p_frag_list_cur_buf_hdr->IP_TotLen - p_frag_list_cur_buf_hdr->IP_HdrLen;
            if (frag_size != frag_size_cur) {                           /* If frag size != cur frag size,    ...        */
                NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... discard overlap frag datagram ...        */
                                                                        /* ... (see Note #1b2).                         */
            }
#endif
            frag_insert_done = DEF_YES;
        }
    }

    return (p_frag);
}


/*
*********************************************************************************************************
*                                    NetIPv6_RxPktFragListRemove()
*
* Description : (1) Remove fragment list from Fragment Lists :
*
*                   (a) Free   fragment reassembly timer
*                   (b) Remove fragment list from Fragment Lists
*                   (c) Clear  buffer's fragment pointers
*
*
* Argument(s) : p_frag_list     Pointer to fragment list head buffer.
*               ----------      Argument validated in NetIPv6_RxPktFragListDiscard(),
*                                                     NetIPv6_RxPktFragListChkComplete().
*
*               tmr_free        Indicate whether to free network timer :
*
*                                   DEF_YES            Free network timer for fragment list discard.
*                                   DEF_NO      Do NOT free network timer for fragment list discard
*                                                   [Freed by  NetTmr_TaskHandler()
*                                                          via NetIPv6_RxPktFragListDiscard()].
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_RxPktFragListDiscard(),
*               NetIPv6_RxPktFragListChkComplete().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktFragListRemove (NET_BUF      *p_frag_list,
                                           CPU_BOOLEAN   tmr_free)
{
    NET_BUF      *p_frag_list_prev_list;
    NET_BUF      *p_frag_list_next_list;
    NET_BUF_HDR  *p_frag_list_prev_list_buf_hdr;
    NET_BUF_HDR  *p_frag_list_next_list_buf_hdr;
    NET_BUF_HDR  *p_frag_list_buf_hdr;
    NET_TMR      *p_tmr;


    p_frag_list_buf_hdr = &p_frag_list->Hdr;

                                                                /* ------------------ FREE FRAG TMR ------------------- */
    if (tmr_free == DEF_YES) {
        p_tmr = p_frag_list_buf_hdr->TmrPtr;
        if (p_tmr != (NET_TMR *)0) {
            NetTmr_Free(p_tmr);
        }
    }

                                                                /* --------- REMOVE FRAG LIST FROM FRAG LISTS --------- */
    p_frag_list_prev_list = p_frag_list_buf_hdr->PrevPrimListPtr;
    p_frag_list_next_list = p_frag_list_buf_hdr->NextPrimListPtr;

                                                                /* Point prev frag list to next frag list.              */
    if (p_frag_list_prev_list != (NET_BUF *)0) {
        p_frag_list_prev_list_buf_hdr                  = &p_frag_list_prev_list->Hdr;
        p_frag_list_prev_list_buf_hdr->NextPrimListPtr =  p_frag_list_next_list;
    } else {
        NetIPv6_FragReasmListsHead                    =  p_frag_list_next_list;
    }

                                                                /* Point next frag list to prev frag list.              */
    if (p_frag_list_next_list != (NET_BUF *)0) {
        p_frag_list_next_list_buf_hdr                  = &p_frag_list_next_list->Hdr;
        p_frag_list_next_list_buf_hdr->PrevPrimListPtr =  p_frag_list_prev_list;
    } else {
        NetIPv6_FragReasmListsTail                    =  p_frag_list_prev_list;
    }

                                                                /* ---------------- CLR BUF FRAG PTRS ----------------- */
    p_frag_list_buf_hdr->PrevPrimListPtr = (NET_BUF *)0;
    p_frag_list_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
    p_frag_list_buf_hdr->TmrPtr          = (NET_TMR *)0;
}


/*
*********************************************************************************************************
*                                   NetIPv6_RxPktFragListDiscard()
*
* Description : Discard fragment list from Fragment Lists.
*
* Argument(s) : p_frag_list     Pointer to fragment list head buffer.
*               ----------      Argument validated in NetIPv6_RxPktFragListInsert(),
*                                                     NetIPv6_RxPktFragListChkComplete(),
*                                                     NetIPv6_RxPktFragListDiscard().
*
*               tmr_free        Indicate whether to free network timer :
*
*                                   DEF_YES            Free network timer for fragment list discard.
*                                   DEF_NO      Do NOT free network timer for fragment list discard
*                                                   [Freed by NetTmr_TaskHandler()].
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_RX_FRAG_DISCARD      Fragment list discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_RxPktFragListInsert(),
*               NetIPv6_RxPktFragListChkComplete(),
*               NetIPv6_RxPktFragListTimeout().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktFragListDiscard (NET_BUF      *p_frag_list,
                                            CPU_BOOLEAN   tmr_free,
                                            NET_ERR      *p_err)
{
    NET_ERR  err;


    NetIPv6_RxPktFragListRemove(p_frag_list, tmr_free);                  /* Remove frag list from Frag Lists.            */
    NetIPv6_RxPktDiscard(p_frag_list, &err);                             /* Discard every frag buf in frag list.         */

    NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragDgramDisCtr); /* Inc discarded frag'd datagram ctr.           */

   *p_err = NET_IPv6_ERR_RX_FRAG_DISCARD;
}


/*
*********************************************************************************************************
*                                    NetIPv6_RxPktFragListUpdate()
*
* Description : Update fragment list reassembly calculations.
*
* Argument(s) : p_frag_list_buf_hdr     Pointer to fragment list head buffer's header.
*               ------------------      Argument validated in NetIPv6_RxPktFragListAdd(),
*                                                             NetIPv6_RxPktFragListInsert().
*
*               frag_ip_flags           Fragment IPv6 header flags.
*               -------------           Argument validated in NetIPv6_RxPktFragListAdd(),
*                                                             NetIPv6_RxPktFragListInsert().
*
*               frag_offset             Fragment offset.
*               -----------             Argument validated in NetIPv6_RxPktFragListAdd(),
*                                                             NetIPv6_RxPktFragListInsert().
*
*               frag_size               Fragment size (in octets).
*               ---------               Argument validated in NetIPv6_RxPktFragListAdd(),
*                                                             NetIPv6_RxPktFragListInsert().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               Fragment list reassembly calculations
*                                                                   successfully updated.
*                               NET_IPv6_ERR_RX_FRAG_LEN_TOT    Invalid fragment datagram total IP length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_RxPktFragListAdd(),
*               NetIPv6_RxPktFragListInsert().
*
* Note(s)     : (1) RFC #2460, Section 4.5 'Fragment Header' states that :
*
*                   (a) "If insufficient fragments are received to complete reassembly of a packet within
*                        60 seconds of the reception of the first-arriving fragment of that packet,
*                        reassembly of that packet must be abandoned and all the fragments that have been
*                        received for that packet must be discarded."
*
*                   (b) "If the first fragment (i.e., the one with a Fragment Offset of zero) has been
*                        received, an ICMP Time Exceeded -- Fragment Reassembly Time Exceeded message
*                        should be sent to the source of that fragment."
*
*               (2) To avoid possible integer arithmetic overflow, the fragmentation arithmetic result MUST
*                   be declared as an integer data type with a greater resolution -- i.e. greater number of
*                   bits -- than the fragmentation arithmetic operands' data type(s).
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktFragListUpdate (NET_BUF      *p_frag_list,
                                           NET_BUF_HDR  *p_frag_list_buf_hdr,
                                           CPU_INT16U    frag_ip_flags,
                                           CPU_INT32U    frag_offset,
                                           CPU_INT32U    frag_size,
                                           NET_ERR      *p_err)
{
    NET_BUF      *p_buf_last;
    NET_BUF_HDR  *p_buf_hdr_last;
    CPU_INT32U    frag_size_tot;                                     /* See Note #2.                                     */
    CPU_BOOLEAN   ip_flag_frags_more;
    NET_ERR       err;


    p_frag_list_buf_hdr->IP_FragSizeCur += frag_size;
    ip_flag_frags_more                  = DEF_BIT_IS_SET(frag_ip_flags, NET_IPv6_FRAG_FLAG_FRAG_MORE);
    if (ip_flag_frags_more != DEF_YES) {                            /* If 'More Frags' NOT set (see Note #1b1A), ...    */
                                                                    /* ... calc frag tot size  (see Note #1b2).         */
        frag_size_tot = (CPU_INT32U)frag_offset + (CPU_INT32U)frag_size;
        if (frag_size_tot > NET_IPv6_TOT_LEN_MAX) {                 /* If frag tot size > IP tot len max, ...           */

                                                                    /* Send ICMPv6 Problem message.                     */
            p_buf_last     = p_frag_list;
            p_buf_hdr_last = p_frag_list_buf_hdr;
            while (p_buf_hdr_last->NextBufPtr != (NET_BUF *)0) {
                p_buf_last     =  p_buf_hdr_last->NextBufPtr;
                p_buf_hdr_last = &p_buf_last->Hdr;
            }
#ifdef  NET_ICMPv6_MODULE_EN
            NetICMPv6_TxMsgErr(p_buf_last,
                               NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                               NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR,
                              (p_frag_list_buf_hdr->IPv6_FragHdrIx - p_frag_list_buf_hdr->IP_HdrIx) + NET_ICMPv6_PTR_IX_IP_FRAG_OFFSET,
                              &err);
#endif
            NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... discard ovf'd frag datagram (see Note #1a3). */
           *p_err = NET_IPv6_ERR_RX_FRAG_LEN_TOT;
            return;
        }

        p_frag_list_buf_hdr->IP_FragSizeTot = (CPU_INT16U)frag_size_tot;
    }

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                 NetIPv6_RxPktFragListChkComplete()
*
* Description : Check if fragment list complete; i.e. fragmented datagram reassembled.
*
* Argument(s) : p_frag_list             Pointer to fragment list head buffer.
*               ----------              Argument validated in NetIPv6_RxPktFragListInsert().
*
*               p_frag_list_buf_hdr     Pointer to fragment list head buffer's header.
*               ------------------      Argument validated in NetIPv6_RxPktFragListInsert().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_RX_FRAG_COMPLETE   Datagram complete; fragments reassembled.
*                               NET_IPv6_ERR_RX_FRAG_REASM      Fragments in reassembly progress.
*                               NET_IPv6_ERR_RX_FRAG_SIZE_TOT   Invalid fragmented datagram total size.
*                               NET_IPv6_ERR_RX_FRAG_LEN_TOT    Invalid fragment   datagram total IPv6 length.
*
*                                                               - RETURNED BY NetIPv6_RxPktFragListDiscard() : -
*                               NET_IPv6_ERR_RX_FRAG_DISCARD    Fragment list discarded.
*
* Return(s)   : Pointer to reassembled datagram, if fragment reassembly complete.
*
*               Pointer to NULL,                 if fragment reassembly in progress.
*                                                   OR
*                                                for any fragment discard error.
*
* Caller(s)   : NetIPv6_RxPktFragListInsert().
*
* Note(s)     : (1) RFC #2460, Section 4.5 'Fragment Header' states that :
*
*                   (a) "If insufficient fragments are received to complete reassembly of a packet within
*                        60 seconds of the reception of the first-arriving fragment of that packet,
*                        reassembly of that packet must be abandoned and all the fragments that have been
*                        received for that packet must be discarded."
*
*                   (b) "If the first fragment (i.e., the one with a Fragment Offset of zero) has been
*                        received, an ICMP Time Exceeded -- Fragment Reassembly Time Exceeded message
*                        should be sent to the source of that fragment."
*
*               (2) To avoid possible integer arithmetic overflow, the fragmentation arithmetic result
*                   MUST be declared as an integer data type with a greater resolution -- i.e. greater
*                   number of bits -- than the fragmentation arithmetic operands' data type(s).
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv6_RxPktFragListChkComplete (NET_BUF      *p_frag_list,
                                                    NET_BUF_HDR  *p_frag_list_buf_hdr,
                                                    NET_ERR      *p_err)
{
    NET_BUF       *p_frag;
    CPU_INT32U     frag_tot_len;                                    /* See Note #2.                                     */
#if 0
    NET_TMR_TICK   timeout_tick;
#endif
    NET_ERR        err;

                                                                    /* If tot frag size complete, ...                   */
    if (p_frag_list_buf_hdr->IP_FragSizeCur == p_frag_list_buf_hdr->IP_FragSizeTot) {
                                                                    /* Calc frag IPv6 tot len (see Note #1a2).          */
#if 0
        frag_tot_len = (CPU_INT32U)p_frag_list_buf_hdr->IPv6_ExtHdrLen + (CPU_INT32U)p_frag_list_buf_hdr->IP_FragSizeTot;
#else
        frag_tot_len = (CPU_INT32U)p_frag_list_buf_hdr->IP_FragSizeTot;
#endif
        if (frag_tot_len > NET_IPv6_TOT_LEN_MAX) {                  /* If tot frag len > IPv6 tot len max, ...          */
            NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ...discard ovf'd frag datagram (see Note #1a1C).*/

#ifdef  NET_ICMPv6_MODULE_EN
                                                                    /* Send ICMPv6 Problem message.                     */
            NetICMPv6_TxMsgErr(p_frag_list,
                               NET_ICMPv6_MSG_TYPE_PARAM_PROB,
                               NET_ICMPv6_MSG_CODE_PARAM_PROB_IP_HDR,
                              (p_frag_list_buf_hdr->IPv6_FragHdrIx - p_frag_list_buf_hdr->IP_HdrIx) + NET_ICMPv6_PTR_IX_IP_FRAG_OFFSET,
                              &err);
#endif
           *p_err =   NET_IPv6_ERR_RX_FRAG_LEN_TOT;
            return ((NET_BUF *)0);
        }

        NetIPv6_RxPktFragListRemove(p_frag_list, DEF_YES);
        NET_CTR_STAT_INC(Net_StatCtrs.IPv6.RxFragDgramReasmCtr);
        p_frag = p_frag_list;                                       /* ... rtn reasm'd datagram (see Note #1b1).        */
       *p_err  = NET_IPv6_ERR_RX_FRAG_COMPLETE;

#if 0
                                                                    /* If cur frag size > tot frag size, ...            */
    } else if (p_frag_list_buf_hdr->IP_FragSizeCur > p_frag_list_buf_hdr->IP_FragSizeTot) {
        NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, p_err);  /* ... discard ovf'd frag datagram.                 */
       *p_err =   NET_IPv6_ERR_RX_FRAG_SIZE_TOT;
        return ((NET_BUF *)0);
#endif

    } else {                                                        /* Else reset frag tmr (see Note #1b2A).            */
#if 0
        CPU_CRITICAL_ENTER();
        timeout_tick = NetIPv6_FragReasmTimeout_tick;
        CPU_CRITICAL_EXIT();
        NetTmr_Set((NET_TMR    *) p_frag_list_buf_hdr->TmrPtr,
                   (CPU_FNCT_PTR) NetIPv6_RxPktFragTimeout,
                   (NET_TMR_TICK) timeout_tick,
                   (NET_ERR    *)&err);
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (err != NET_TMR_ERR_NONE) {
            NetIPv6_RxPktFragListDiscard(p_frag_list, DEF_YES, p_err);
            return ((NET_BUF *)0);
        }
#endif
#endif
       *p_err  =  NET_IPv6_ERR_RX_FRAG_REASM;
        p_frag = (NET_BUF *)0;

    }

    return (p_frag);
}


/*
*********************************************************************************************************
*                                     NetIPv6_RxPktFragTimeout()
*
* Description : Discard fragment list on fragment reassembly timeout.
*
* Argument(s) : p_frag_list_timeout     Pointer to network buffer fragment reassembly list (see Note #1b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetIPv6_RxPktFragListAdd().
*
* Note(s)     : (1) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
*                   type functions -- even though network timer API functions cast callback functions
*                   to generic 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]').
*
*                   (a) (1) Unfortunately, ISO/IEC 9899:TC2, Section 6.3.2.3.(7) states that "a pointer
*                           to an object ... may be converted to a pointer to a different object ...
*                           [but] if the resulting pointer is not correctly aligned ... the behavior
*                           is undefined".
*
*                           And since compilers may NOT correctly convert 'void' pointers to non-'void'
*                           pointer arguments, network timer callback functions MUST avoid incorrect
*                           pointer conversion behavior between 'void' pointer parameters & non-'void'
*                           pointer arguments & therefore CANNOT be defined as '[(void) (OBJECT *)]'.
*
*                       (2) However, Section 6.3.2.3.(1) states that "a pointer to void may be converted
*                           to or from a pointer to any ... object ... A pointer to any ... object ...
*                           may be converted to a pointer to void and back again; the result shall
*                           compare equal to the original pointer".
*
*                   (b) Therefore, to correctly convert 'void' pointer objects back to ap_propriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_BUF' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetIPv6_RxPktFragListRemove() via NetIPv6_RxPktFragListDiscard().
*
*                   (b) but do NOT re-free the timer.
*
*               (3) RFC #2460, Section 4.5 'Fragment Header' states that :
*
*                   (a) "If insufficient fragments are received to complete reassembly of a packet within
*                        60 seconds of the reception of the first-arriving fragment of that packet,
*                        reassembly of that packet must be abandoned and all the fragments that have been
*                        received for that packet must be discarded."
*
*                   (b) "If the first fragment (i.e., the one with a Fragment Offset of zero) has been
*                        received, an ICMP Time Exceeded -- Fragment Reassembly Time Exceeded message
*                        should be sent to the source of that fragment."
*
*               (4) MUST send ICMP 'Time Exceeded' error message BEFORE NetIPv6_RxPktFragListDiscard()
*                   frees fragment buffers.
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktFragTimeout (void  *p_frag_list_timeout)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN   used;
#endif
    NET_BUF      *p_frag_list;
    NET_ERR       err;


    p_frag_list = (NET_BUF *)p_frag_list_timeout;               /* See Note #2b2A.                                      */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* -------------- VALIDATE IPv6 RX FRAG --------------- */
    if (p_frag_list == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return;
    }

    used = NetBuf_IsUsed(p_frag_list);
    if (used != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.NullPtrCtr);
        return;
    }
#endif

#ifdef  NET_ICMPv6_MODULE_EN
    NetICMPv6_TxMsgErr((NET_BUF  *) p_frag_list,                 /* Send ICMPV6 'Time Exceeded' err msg (see Note #4).     */
                       (CPU_INT08U) NET_ICMPv6_MSG_TYPE_TIME_EXCEED,
                       (CPU_INT08U) NET_ICMPv6_MSG_CODE_TIME_EXCEED_FRAG_REASM,
                       (CPU_INT08U) NET_ICMPv6_MSG_PTR_NONE,
                       (NET_ERR  *)&err);
#endif
                                                                /* Discard frag list (see Note #1b).                    */
    NetIPv6_RxPktFragListDiscard((NET_BUF   *) p_frag_list,
                                 (CPU_BOOLEAN) DEF_NO,          /* Clr but do NOT free tmr (see Note #3).               */
                                 (NET_ERR   *)&err);


    NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxFragDgramTimeoutCtr);
}


/*
*********************************************************************************************************
*                                    NetIPv6_RxPktDemuxDatagram()
*
* Description : Demultiplex IPv6 datagram to ap_propriate ICMPv6, UDP, or TCP layer.
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv6 datagram.
*               ----        Argument checked   in NetIPv6_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetIPv6_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_ERR_RX                      Receive error; packet discarded.
*
*                                                               ----- RETURNED BY NetICMPv6_Rx() : -----
*                               NET_ICMPV6_ERR_NONE             ICMPv6 message successfully demultiplexed.
*
*                                                               ------ RETURNED BY NetUDP_Rx() : -------
*                               NET_UDP_ERR_NONE                UDP datagram successfully demultiplexed.
*
*                                                               ------ RETURNED BY NetTCP_Rx() : -------
*                               NET_TCP_ERR_NONE                TCP segment  successfully demultiplexed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Rx().
*
* Note(s)     : (1) When network buffer is demultiplexed to higher-layer protocol receive, buffer's reference
*                   counter is NOT incremented since the IPv6 layer does NOT maintain a reference to the
*                   buffer.
*
*               (2) Default case already invalidated in NetIPv6_RxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktDemuxDatagram (NET_BUF      *p_buf,
                                          NET_BUF_HDR  *p_buf_hdr,
                                          NET_ERR      *p_err)
{
    NET_BUF_HDR  *p_hdr;


    p_hdr = &p_buf->Hdr;
    DEF_BIT_SET(p_hdr->Flags, NET_BUF_FLAG_IPv6_FRAME);         /* Set IPv6 Flag.                                       */

    switch (p_buf_hdr->ProtocolHdrType) {                       /* Demux buf to appropriate protocol (see Note #1).     */
#ifdef  NET_ICMPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_ICMP_V6:
             NetICMPv6_Rx(p_buf, p_err);
             break;
#endif


        case NET_PROTOCOL_TYPE_UDP_V6:
             NetUDP_Rx(p_buf, p_err);
             break;


#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V6:
             NetTCP_Rx(p_buf, p_err);
             break;
#endif

        case NET_PROTOCOL_TYPE_NONE:
        default:                                                /* See Note #2.                                         */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.RxInvProtocolCtr);
           *p_err = NET_ERR_INVALID_PROTOCOL;
            return;
    }
}


/*
*********************************************************************************************************
*                                       NetIPv6_RxPktDiscard()
*
* Description : On any IPv6 receive error(s), discard IPv6 packet(s) & buffer(s).
*
* Argument(s) : p_buf        Pointer to network buffer.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Rx(),
*               NetIPv6_RxPktFragListDiscard().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_RxPktDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IPv6.RxPktDisCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBufList((NET_BUF *)p_buf,
                            (NET_CTR *)p_ctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                       NetIPv6_TxPktValidate()
*
* Description : (1) Validate IPv6 transmit packet parameters :
*
*                   (a) Validate the following transmit packet parameters :
*
*                       (1) Supported protocols :
*                           (A) ICMPv6
*                           (B) UDP
*                           (C) TCP
*
*                       (2) Buffer protocol index
*                       (3) Total Length
*                       (4) Hop Limit                                               See Note #2d
*                       (5) Destination Address                                     See Note #2f
*                       (6) Source      Address                                     See Note #2e
*
*
* Argument(s) : p_buf_hdr       Pointer to network buffer header.
*               --------        Argument validated in NetIPv6_Tx().
*
*               p_addr_src      Pointer to source        IPv6 address.
*
*               p_addr_dest     Pointer to destination   IPv6 address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE                   Transmit packet validated.
*                               NET_BUF_ERR_INVALID_TYPE            Invalid network buffer type.
*                               NET_ERR_INVALID_PROTOCOL            Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_IX              Invalid/insufficient buffer index.
*                               NET_IPv6_ERR_INVALID_LEN_DATA       Invalid protocol/data length.
*                               NET_IPv6_ERR_INVALID_HOP_LIMIT      Invalid IPv6 Hop Limit.
*                               NET_IPv6_ERR_INVALID_ADDR_SRC       Invalid IPv6 source          address.
*
*                                                                   -- RETURNED BY NetIF_IsEnHandler() : --
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled network interface.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Tx().
*
* Note(s)     : (2) See 'net_ipv6.h  IPv6 ADDRESS DEFINES  Notes #2 & #3' for supported IPv6 addresses.
*********************************************************************************************************
*/

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void  NetIPv6_TxPktValidate (const  NET_BUF_HDR             *p_buf_hdr,
                                     const  NET_IPv6_ADDR           *p_addr_src,
                                     const  NET_IPv6_ADDR           *p_addr_dest,
                                            NET_IPv6_TRAFFIC_CLASS   traffic_class,
                                            NET_IPv6_FLOW_LABEL      flow_label,
                                            NET_IPv6_HOP_LIM         hop_lim,
                                            NET_ERR                 *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT16U       ix;
    CPU_INT16U       len;
#endif
    NET_IPv6_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;
    CPU_BOOLEAN      addr_unspecified;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                    /* ------------ VALIDATE NET BUF TYPE ------------- */
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        case NET_BUF_TYPE_BUF:
        case NET_BUF_TYPE_RX_LARGE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }


                                                                    /* --------------- VALIDATE PROTOCOL -------------- */
    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_ICMP_V6:
             ix  = p_buf_hdr->ICMP_MsgIx;
             len = p_buf_hdr->ICMP_MsgLen;
             break;


        case NET_PROTOCOL_TYPE_UDP_V6:
#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V6:
#endif
             ix  = p_buf_hdr->TransportHdrIx;
             len = p_buf_hdr->TransportHdrLen + (CPU_INT16U)p_buf_hdr->DataLen;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    if (ix <  NET_IPv6_HDR_SIZE_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

                                                                    /* ------------ VALIDATE TOT DATA LEN ------------- */
    if (len != p_buf_hdr->TotLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvDataLenCtr);
       *p_err = NET_IPv6_ERR_INVALID_LEN_DATA;
        return;
    }

                                                                    /* ----------- VALIDATE IPv6 HOP LIMIT ------------ */
    if (hop_lim < 1) {                                              /* If Hop Limit < 1, rtn err.                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvTTL_Ctr);
       *p_err = NET_IPv6_ERR_INVALID_HOP_LIMIT;
        return;
    }
#endif

                                                                    /* ------------- VALIDATE IPv6 ADDRS -------------- */
    if_nbr = p_buf_hdr->IF_Nbr;                                     /* Get pkt's tx IF.                                 */
   (void)NetIF_IsEnHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                    /* Chk pkt's tx cfg'd host addr for src addr.       */
    addr_unspecified = NetIPv6_IsAddrUnspecified(p_addr_src);
    if (addr_unspecified == DEF_NO) {
        if (if_nbr != NET_IF_NBR_LOCAL_HOST) {
            p_ip_addrs = NetIPv6_GetAddrsHostOnIF(if_nbr, p_addr_src);
        } else {
            p_ip_addrs = NetIPv6_GetAddrsHost(p_addr_src,
                                              DEF_NULL);
        }

        if (p_ip_addrs == (NET_IPv6_ADDRS *)0) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvAddrSrcCtr);
           *p_err = NET_IPv6_ERR_INVALID_ADDR_SRC;
            return;
        }
    }


   (void)&p_addr_dest;
   (void)&traffic_class;
   (void)&flow_label;

   *p_err = NET_IPv6_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                           NetIPv6_TxPkt()
*
* Description : (1) Prepare IPv6 header & transmit IPv6 packet :

*                   (a) Calculate IPv6 header buffer controls
*                   (b) Check for transmit fragmentation        See Note #2
*                   (c) Prepare   IPv6 Extension header(s)
*                   (d) Prepare   IPv6 header
*                   (e) Transmit  IPv6 packet datagram
*
*
* Argument(s) : p_buf           Pointer to network buffer to transmit IPv6 packet.
*               ----            Argument checked   in NetIPv6_Tx().
*
*               p_buf_hdr       Pointer to network buffer header.
*               --------        Argument validated in NetIPv6_Tx().
*
*               p_addr_src      Pointer to source      IPv6 address.
*               ---------       Argument checked   in NetIPv6_TxPktValidate().
*
*               p_addr_dest     Pointer to destination IPv6 address.
*               ----------      Argument checked   in NetIPv6_TxPktValidate().
*
*               p_ext_hdr_list  Pointer to extension header list to add to IPv6 packet.
*
*               traffic_class   Traffic class to add in the IPv6 header to send.
*
*               flow_label      Flow label to add in the IPv6 header to send.
*
*               hop_lim         Hop limit to add in the IPv6 header of the packet to send.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_INVALID_PROTOCOL            Invalid/unknown protocol type.
*                               NET_IPv6_ERR_INVALID_LEN_HDR        Invalid IPv6 header length.
*                               NET_IPv6_ERR_INVALID_FRAG           Invalid IPv6 fragmentation.
*
*                                                                   --- RETURNED BY NetIF_MTU_GetProtocol() : ---
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL function pointer.
*
*                                                                   - RETURNED BY NetIPv6_TxPktPrepareExtHdr() :-
*                               NET_IPv6_ERR_INVALID_PROTOCOL       Invalid/unknown protocol type.
*                               NET_IPv6_ERR_INVALID_EH             Invalid extension header.
*
*                                                                   -- RETURNED BY NetIPv6_TxPktPrepareHdr() : --
*                               NET_IPv6_ERR_INVALID_PROTOCOL       Invalid/unknown protocol type.
*
*                                                                   --- RETURNED BY NetIPv6_TxPktDatagram() : ---
*                               NET_IPv6_ERR_TX_DEST_LOCAL_HOST     Destination is a local host address.
*                               NET_IPv6_ERR_TX_DEST_MULTICAST      Multicast destination.
*                               NET_IPv6_ERR_TX_DEST_HOST_THIS_NET  Destination is on same link.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY        Next-hop is the default router.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY_NONE   Next-hop is a router in the router list.
*                               NET_IPv6_ERR_TX_NEXT_HOP_NONE       No next-hop is available.
*                               NET_IF_ERR_NONE                     Packet successfully transmitted.
*                               NET_ERR_IF_LOOPBACK_DIS             Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN                Network  interface link state down (i.e.
*                                                                       NOT available for receive or transmit).
*                               NET_ERR_TX                          Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Tx().
*
* Note(s)     : (2) IPv6 transmit fragmentation NOT currently supported (see 'net_IPv6.c  Note #1c').
*                   fragmentation is supported for ICMPv6 packet but not for TCP and UDP packets.
*
*               (3) Default case already invalidated in NetIPv6_TxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv6_TxPkt (NET_BUF                 *p_buf,
                             NET_BUF_HDR             *p_buf_hdr,
                             NET_IPv6_ADDR           *p_addr_src,
                             NET_IPv6_ADDR           *p_addr_dest,
                             NET_IPv6_EXT_HDR        *p_ext_hdr_list,
                             NET_IPv6_TRAFFIC_CLASS   traffic_class,
                             NET_IPv6_FLOW_LABEL      flow_label,
                             NET_IPv6_HOP_LIM         hop_lim,
                             NET_ERR                 *p_err)
{
    CPU_INT16U         ip_hdr_len_size;
    CPU_INT16U         protocol_ix;
    CPU_BOOLEAN        ip_tx_frag;


                                                                /* ---------------- PREPARE IPv6 OPTS ----------------- */

    ip_tx_frag = DEF_NO;
                                                                /* --------------- CALC IPv6 HDR CTRLS ---------------- */
                                                                /* Calc tot IPv6 hdr len (in octets).                   */
    ip_hdr_len_size = (CPU_INT16U)(NET_IPv6_HDR_SIZE);


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (ip_hdr_len_size > NET_IPv6_HDR_SIZE_MAX) {
       *p_err = NET_IPv6_ERR_INVALID_LEN_HDR;
        return;
    }
#endif

    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_ICMP_V6:
             protocol_ix = p_buf_hdr->ICMP_MsgIx;
             break;


        case NET_PROTOCOL_TYPE_UDP_V6:
#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V6:
#endif
             protocol_ix = p_buf_hdr->TransportHdrIx;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }


                                                                /* ----------------- CHK FRAG REQUIRED ---------------- */
                                                                /* Chained NET_BUF requires frag.                       */
    if (p_buf_hdr->NextBufPtr != (NET_BUF *)0) {
        ip_tx_frag = DEF_YES;
    }

    if (protocol_ix < ip_hdr_len_size) {                        /* If hdr len > allowed rem ix, tx frag req'd.          */
        *p_err = NET_IPv6_ERR_TX_PKT;
         return;
    }


#if 0
    if (ip_tx_frag == DEF_YES) {                                /* If tx frag NOT required, (see Note #2).              */
        NetIPv6_TxPktPrepareFragHdr(p_buf,
                                    p_buf_hdr,
                                   &protocol_ix,
                                    p_err);

        if (*p_err != NET_IPv6_ERR_NONE) {
             return;
        }
    }
#endif

                                                                /* ... prepare IPv6 Extension Headers ...               */
    NetIPv6_TxPktPrepareExtHdr (p_buf,
                                p_ext_hdr_list,
                                p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         return;
    }

    NetIPv6_TxPktPrepareHdr(p_buf,                              /* ... prepare IPv6 hdr     ...                         */
                            p_buf_hdr,
                            protocol_ix,
                            p_addr_src,
                            p_addr_dest,
                            p_ext_hdr_list,
                            traffic_class,
                            flow_label,
                            hop_lim,
                            p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         return;
    }

    NetIPv6_TxPktDatagram(p_buf, p_buf_hdr, p_err);             /* ... & tx IPv6 datagram.                              */

    (void)&ip_tx_frag;                                          /* Prevent 'variable unused' compiler warning.          */
}


/*
*********************************************************************************************************
*                                      NetIPv6_TxPktPrepareHdr()
*
* Description : (1) Prepare IPv6 header :
*
*                   (a) Update network buffer's protocol index & length controls
*
*                   (b) Prepare the transmit packet's following IPv6 header fields :
*
*                       (1) Version
*                       (2) Traffic Class
*                       (3) Flow Label
*                       (4) Payload Length
*                       (5) Next Header
*                       (6) Hop Limit
*                       (7) Source      Address
*                       (8) Destination Address
*
*                   (c) Convert the following IPv6 header fields from host-order to network-order :
*
*                       (1) Version / Traffic Class / Flow Label
*                       (2) Payload Length
*
*
* Argument(s) : p_buf               Pointer to network buffer to transmit IP packet.
*               ----                Argument checked   in NetIPv6_Tx().
*
*               p_buf_hdr           Pointer to network buffer header.
*               --------            Argument validated in NetIPv6_Tx().
*
*               protocol_ix         Index to higher-layer protocol header.
*
*               p_addr_src          Pointer to source        IPv6 address.
*               ---------           Argument checked   in NetIPv6_TxPktValidate().
*
*               p_addr_dest         Pointer to destination   IPv6 address.
*               ----------          Argument checked   in NetIPv6_TxPktValidate().
*
*               p_ext_hdr_list      Pointer to IPv6 Extensions headers list.
*
*               traffic_class       Traffic Class of packet.
*
*               flow_label          Flow Label of packet.
*
*               hop_lim             Hop Limit of packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IPv6 header successfully prepared.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPkt().
*
* Note(s)     : (2) See 'net_ipv6.h  IPv6 HEADER' for IPv6 header format.
*
*               (3) Supports ONLY the following protocols :
*
*                   (a) ICMPv6
*                   (b) UDP
*                   (c) TCP
*
*                   See also 'net.h  Note #2a'.
*
*               (4) Default case already invalidated in NetIPv6_TxPktValidate(). However, the default case is
*                   included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv6_TxPktPrepareHdr (NET_BUF                 *p_buf,
                                       NET_BUF_HDR             *p_buf_hdr,
                                       CPU_INT16U               protocol_ix,
                                       NET_IPv6_ADDR           *p_addr_src,
                                       NET_IPv6_ADDR           *p_addr_dest,
                                       NET_IPv6_EXT_HDR        *p_ext_hdr_list,
                                       NET_IPv6_TRAFFIC_CLASS   traffic_class,
                                       NET_IPv6_FLOW_LABEL      flow_label,
                                       NET_IPv6_HOP_LIM         hop_lim,
                                       NET_ERR                 *p_err)
{
    NET_IPv6_HDR      *p_ip_hdr;
    NET_IPv6_EXT_HDR  *p_ext_hdr;
    CPU_INT08U         ip_ver;
    CPU_INT32U         ip_ver_traffic_flow;
    NET_DEV_CFG       *p_dev_cfg;
    NET_IF            *p_if;

    p_if      = NetIF_Get(p_buf_hdr->IF_Nbr, p_err);
    p_dev_cfg = p_if->Dev_Cfg;

    while (p_buf != (NET_BUF *)0) {

                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
        p_buf_hdr->IP_HdrIx = p_dev_cfg->TxBufIxOffset;
        NetIF_TxIxDataGet(p_buf_hdr->IF_Nbr,
                          0,
                         &p_buf_hdr->IP_HdrIx,
                          p_err);
        p_buf_hdr->IP_DataLen     = (CPU_INT16U) p_buf_hdr->TotLen;
        p_buf_hdr->IP_DatagramLen = (CPU_INT16U) p_buf_hdr->TotLen;

                                                                /* ----------------- PREPARE IPv6 HDR ----------------- */
        p_ip_hdr = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];


                                                                /* ----- PREPARE IP VER/TRAFFIC CLASS/FLOW LABEL ------ */
        ip_ver = NET_IPv6_HDR_VER;
        ip_ver_traffic_flow  = (ip_ver        << NET_IPv6_HDR_VER_SHIFT);

        traffic_class        = (traffic_class  & NET_IPv6_HDR_TRAFFIC_CLASS_MASK_16);
        ip_ver_traffic_flow |= (traffic_class << NET_IPv6_HDR_TRAFFIC_CLASS_SHIFT);

        flow_label           = (flow_label     & NET_IPv6_HDR_FLOW_LABEL_MASK);
        ip_ver_traffic_flow |= (flow_label    << NET_IPv6_HDR_FLOW_LABEL_SHIFT);

        NET_UTIL_VAL_COPY_SET_NET_32(&p_ip_hdr->VerTrafficFlow, &ip_ver_traffic_flow);

                                                                /* ------------- PREPARE IPv6 PAYLOAD LEN ------------- */
        NET_UTIL_VAL_COPY_SET_NET_16(&p_ip_hdr->PayloadLen, &p_buf_hdr->TotLen);
        p_buf_hdr->TotLen    += sizeof(NET_IPv6_HDR);
        p_buf_hdr->IP_TotLen  = (CPU_INT16U) p_buf_hdr->TotLen;

                                                                /* -------------- PREPARE IPv6 NEXT HDR --------------- */
        p_ext_hdr = p_ext_hdr_list;
        if (p_ext_hdr == (NET_IPv6_EXT_HDR *)0) {
            switch (p_buf_hdr->ProtocolHdrType) {               /* Demux IPv6 protocol (see Note #3).                   */
                case NET_PROTOCOL_TYPE_ICMP_V6:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_ICMPv6;
                     break;


                case NET_PROTOCOL_TYPE_UDP_V6:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_UDP;
                     break;


    #ifdef  NET_TCP_MODULE_EN
                case NET_PROTOCOL_TYPE_TCP_V6:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_TCP;
                     break;
    #endif

                case NET_PROTOCOL_TYPE_IP_V6_EXT_HOP_BY_HOP:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_ROUTING:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_ROUTING;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_FRAG:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_FRAG;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_ESP:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_ESP;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_AUTH:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_AUTH;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_NONE:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_NONE;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_DEST:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_DEST;
                     break;


                case NET_PROTOCOL_TYPE_IP_V6_EXT_MOBILITY:
                     p_ip_hdr->NextHdr = NET_IP_HDR_PROTOCOL_EXT_MOBILITY;
                     break;


                default:                                            /* See Note #4.                                         */
                     NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvProtocolCtr);
                    *p_err = NET_ERR_INVALID_PROTOCOL;
                     return;
            }
        } else {
            p_ip_hdr->NextHdr = p_ext_hdr->Type;
        }

        p_buf_hdr->ProtocolHdrType    = NET_PROTOCOL_TYPE_IP_V6; /* Update buf protocol for IPv6.                       */
        p_buf_hdr->ProtocolHdrTypeNet = NET_PROTOCOL_TYPE_IP_V6;


                                                                /* -------------- PREPARE IPv6 HOP LIMIT -------------- */
        if (hop_lim != NET_IPv6_HOP_LIM_NONE) {
            p_ip_hdr->HopLim = hop_lim;
        } else {
            p_ip_hdr->HopLim = NET_IPv6_HOP_LIM_DFLT;
        }

                                                                /* ---------------- PREPARE IPv6 ADDRS ---------------- */
        Mem_Copy(&p_buf_hdr->IPv6_AddrSrc,  p_addr_src,  NET_IPv6_ADDR_SIZE);
        Mem_Copy(&p_buf_hdr->IPv6_AddrDest, p_addr_dest, NET_IPv6_ADDR_SIZE);

        Mem_Copy(&p_ip_hdr->AddrSrc,  p_addr_src,  NET_IPv6_ADDR_SIZE);
        Mem_Copy(&p_ip_hdr->AddrDest, p_addr_dest, NET_IPv6_ADDR_SIZE);

                                                                /* Move to next buffer.                                 */
        p_buf = p_buf_hdr->NextBufPtr;
        if (p_buf != (NET_BUF *)0) {
            p_buf_hdr = &p_buf->Hdr;
        }
    }

   (void)&protocol_ix;

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIPv6_TxPktPrepareExtHdr()
*
* Description : Prepare Extension headers in packets to send and, if necessary, chained fragments
*               for IPv6 fragmentation.
*
* Argument(s) : p_buf           Pointer to network buffer to transmit IPv6 packet.
*
*               p_ext_hdr_list  Pointer to list of extension headers to add.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No Error.
*                               NET_IPv6_ERR_INVALID_PROTOCOL   The protocol that carries the fragment is not
*                                                                   UDP, TCP or ICMP.
*                               NET_IPv6_ERR_INVALID_EH         An extension header is invalid.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPkt()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_TxPktPrepareExtHdr (NET_BUF           *p_buf,
                                          NET_IPv6_EXT_HDR  *p_ext_hdr_list,
                                          NET_ERR           *p_err)
{
    NET_BUF_HDR                   *p_buf_hdr;
    NET_IPv6_EXT_HDR              *p_ext_hdr;
    NET_IPv6_EXT_HDR              *p_ext_hdr_next;
    NET_IPv6_EXT_HDR              *p_ext_hdr_prev;
    CPU_FNCT_PTR                   fnct;
    NET_IPv6_EXT_HDR_ARG_GENERIC   ext_hdr_arg;
    CPU_INT16U                     data_ix;
    CPU_INT16U                     data_ix_prev;
    CPU_INT08U                     next_hdr_type;
    NET_IPv6_EXT_HDR_ARG_FRAG      frag_hdr_arg;
    NET_IPv6_FRAG_FLAGS            fragOffsetFlag;
    CPU_INT16U                     datagram_offset;
    CPU_INT32U                     ID;
    NET_DEV_CFG                   *p_dev_cfg;
    NET_IF                        *p_if;

    datagram_offset = 0u;
    fragOffsetFlag  = NET_IPv6_FRAG_OFFSET_NONE;
    NET_IPv6_TX_GET_ID(ID);

    while (p_buf != (NET_BUF *)0) {

        p_buf_hdr = &p_buf->Hdr;
        p_if      = NetIF_Get(p_buf_hdr->IF_Nbr, p_err);
        p_dev_cfg = p_if->Dev_Cfg;

        p_buf_hdr->IP_HdrIx = p_dev_cfg->TxBufIxOffset;
        NetIF_TxIxDataGet(p_buf_hdr->IF_Nbr,
                          0,
                         &p_buf_hdr->IP_HdrIx,
                          p_err);

                                                                /* ---------------- SET FRAGMENT INFO ----------------- */
                                                                /* Get the fragment offset.                             */
        fragOffsetFlag = datagram_offset & NET_IPv6_FRAG_OFFSET_MASK;

        datagram_offset   += p_buf_hdr->TotLen;

                                                                /* Determine the more frag flag.                        */
        if (p_buf_hdr->NextBufPtr != (NET_BUF *)0){
            DEF_BIT_SET(fragOffsetFlag, NET_IPv6_FRAG_FLAG_MORE);
        }

                                                                /* ------------ PREPARE EXTENSION HEADERS ------------- */
        data_ix      = 0;
        data_ix_prev = 0;
        p_ext_hdr    = p_ext_hdr_list;
        while (p_ext_hdr != (NET_IPv6_EXT_HDR *)0) {

            p_ext_hdr_next = p_ext_hdr->NextHdrPtr;
            p_ext_hdr_prev = p_ext_hdr->PrevHdrPtr;

                                                                /* Set Type of Next Header.                             */
            if(p_ext_hdr_next != (NET_IPv6_EXT_HDR *)0) {
                next_hdr_type = p_ext_hdr_next->Type;
            } else {
                switch (p_buf_hdr->ProtocolHdrType) {
                    case NET_PROTOCOL_TYPE_ICMP_V6:
                         next_hdr_type = NET_IP_HDR_PROTOCOL_ICMPv6;
                         break;


                    case NET_PROTOCOL_TYPE_UDP_V6:
                         next_hdr_type = NET_IP_HDR_PROTOCOL_UDP;
                         break;


#ifdef  NET_TCP_MODULE_EN
                    case NET_PROTOCOL_TYPE_TCP_V6:
                         next_hdr_type = NET_IP_HDR_PROTOCOL_TCP;
                         break;
#endif


                    default:
                         NET_CTR_ERR_INC(Net_ErrCtrs.IPv6.TxInvProtocolCtr);
                        *p_err = NET_ERR_INVALID_PROTOCOL;
                         return;
                }
            }

                                                                /* Set Ext Hdr Data Index.                              */
            if (p_ext_hdr_prev != (NET_IPv6_EXT_HDR *)0) {
                data_ix = data_ix_prev + p_ext_hdr_prev->Len;
            } else {
                data_ix = p_buf_hdr->IP_HdrIx + NET_IPv6_HDR_SIZE;
            }

                                                                /* Callback function to write content of ext hdr.      */
            switch (p_ext_hdr->Type) {
                case NET_IP_HDR_PROTOCOL_EXT_HOP_BY_HOP:
                case NET_IP_HDR_PROTOCOL_EXT_DEST:
                case NET_IP_HDR_PROTOCOL_EXT_ROUTING:
                case NET_IP_HDR_PROTOCOL_EXT_ESP:
                case NET_IP_HDR_PROTOCOL_EXT_AUTH:
                case NET_IP_HDR_PROTOCOL_EXT_MOBILITY:
                     ext_hdr_arg.BufIx   =  data_ix;
                     ext_hdr_arg.BufPtr  =  p_buf;
                     ext_hdr_arg.NextHdr =  next_hdr_type;
                     fnct                = (CPU_FNCT_PTR)p_ext_hdr->Fnct;
                     fnct((void *) &ext_hdr_arg);
                     break;


                case NET_IP_HDR_PROTOCOL_EXT_FRAG:
                     frag_hdr_arg.BufIx      =  data_ix;
                     frag_hdr_arg.BufPtr     =  p_buf;
                     frag_hdr_arg.NextHdr    =  next_hdr_type;
                     frag_hdr_arg.FragOffset =  NET_UTIL_HOST_TO_NET_16(fragOffsetFlag);
                     frag_hdr_arg.FragID     =  NET_UTIL_HOST_TO_NET_32(ID);
                     fnct                    = (CPU_FNCT_PTR)p_ext_hdr->Fnct;
                     fnct((void *) &frag_hdr_arg);
                     break;


                default:
                   *p_err = NET_IPv6_ERR_INVALID_EH;
                    return;
            }

            p_buf_hdr->IPv6_ExtHdrLen += p_ext_hdr->Len;
            p_buf_hdr->TotLen         += p_ext_hdr->Len;

            data_ix_prev = data_ix;
            p_ext_hdr    = p_ext_hdr->NextHdrPtr;
        }

        p_buf = p_buf_hdr->NextBufPtr;

    }

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIPv6_TxPktPrepareFragHdr()
*
* Description : Prepare packet and chained fragments for IPv6 fragmentation.
*
* Argument(s) : p_buf           Pointer to network buffer to transmit IPv6 packet.
*
*               p_buf_hdr       Pointer to network buffer header.
*
*               p_protocol_ix   Pointer to a protocol header index.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               No Error.
*
*                               NET_IPv6_ERR_INVALID_PROTOCOL   The protocol that carries the fragment is not
*                                                                   UDP, TCP or ICMP.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPkt().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if 0
static  void  NetIPv6_TxPktPrepareFragHdr(NET_BUF      *p_buf,
                                          NET_BUF_HDR  *p_buf_hdr,
                                          CPU_INT16U   *p_protocol_ix,
                                          NET_ERR      *p_err)
{
    NET_IPv6_FRAG_HDR    *p_frag_hdr;
    NET_IPv6_FRAG_FLAGS   fragOffsetFlag;
    CPU_INT16U            datagram_offset;
    CPU_INT32U            ID;


    datagram_offset = 0u;
    fragOffsetFlag  = NET_IPv6_FRAG_OFFSET_NONE;
    NET_IPv6_TX_GET_ID(ID);

   *p_protocol_ix -= NET_IPv6_FRAG_HDR_SIZE;

    while (p_buf != (NET_BUF *)0) {

        p_buf_hdr = &p_buf->Hdr;
        p_buf_hdr->IPv6_ExtHdrLen += NET_IPv6_FRAG_HDR_SIZE;
        p_frag_hdr = (NET_IPv6_FRAG_HDR *)&p_buf->DataPtr[*p_protocol_ix];
        Mem_Clr(p_frag_hdr, NET_IPv6_FRAG_HDR_SIZE);

                                                                /* Set the Next Hdr field of the Frag Hdr.              */
        switch (p_buf_hdr->ProtocolHdrType) {
            case NET_PROTOCOL_TYPE_ICMP_V6:
                 p_frag_hdr->NextHdr = NET_IP_HDR_PROTOCOL_ICMPv6;
                 break;

#ifdef  NET_TCP_MODULE_PRESENT
            case NET_PROTOCOL_TYPE_TCP_V6:
                 p_frag_hdr->NextHdr = NET_IP_HDR_PROTOCOL_TCP;
                 break;
#endif

            case NET_PROTOCOL_TYPE_UDP_V6:
                 p_frag_hdr->NextHdr = NET_IP_HDR_PROTOCOL_UDP;
                 break;


            default:
                *p_err = NET_IPv6_ERR_INVALID_PROTOCOL;
                 return;
        }

        p_buf_hdr->ProtocolHdrType = NET_PROTOCOL_TYPE_IP_V6_EXT_FRAG;

                                                                /* Get the fragment offset.                             */
        fragOffsetFlag = datagram_offset & NET_IPv6_FRAG_OFFSET_MASK;

#if 1
        datagram_offset  += p_buf_hdr->TotLen;
        p_buf_hdr->TotLen += NET_IPv6_FRAG_HDR_SIZE;
#else
        datagram_offset += 1448;
#endif
                                                                /* Determine the more frag flag.                        */
        if (p_buf_hdr->NextBufPtr != (NET_BUF *)0){
            DEF_BIT_SET(fragOffsetFlag, NET_IPv6_FRAG_FLAG_MORE);
        }

        p_frag_hdr->FragOffsetFlag = NET_UTIL_HOST_TO_NET_16(fragOffsetFlag);



        p_buf = p_buf_hdr->NextBufPtr;

    }

   *p_err = NET_IPv6_ERR_NONE;
}
#endif

/*
*********************************************************************************************************
*                                       NetIPv6_TxPktDatagram()
*
* Description : (1) Transmit IPv6 packet datagram :
*
*                   (a) Select next-route IPv6 address
*                   (b) Transmit IPv6 packet datagram via next IPv6 address route :
*
*                       (1) Destination is this host                Send to Loopback Interface
*                       (2) Destination is multicast                Send to Network  Interface Transmit
*                       (3) Destination is Local  Host              Send to Network  Interface Transmit
*                       (4) Destination is Remote Host              Send to Network  Interface Transmit
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit IPv6 packet.
*               ----        Argument checked   in NetIPv6_Tx(),
*                                                 NetIPv6_ReTx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetIPv6_Tx(),
*                                                 NetIPv6_ReTx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetIPv6_TxPktDatagramRouteSel() : -
*                               NET_IPv6_ERR_TX_DEST_LOCAL_HOST     Destination is a local host address.
*                               NET_IPv6_ERR_TX_DEST_MULTICAST      Multicast destination.
*                               NET_IPv6_ERR_TX_DEST_HOST_THIS_NET  Destination is on same link.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY        Next-hop is the default router.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY_NONE   Next-hop is a router in the router list.
*                               NET_IPv6_ERR_TX_NEXT_HOP_NONE       No next-hop is available.
*
*                                                                   ---------- RETURNED BY NetIF_Tx() : -----------
*                               NET_IF_ERR_NONE                     Packet successfully transmitted.
*                               NET_ERR_IF_LOOPBACK_DIS             Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN                Network  interface link state down (i.e.
*                                                                       NOT available for receive or transmit).
*                               NET_ERR_TX                          Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPkt(),
*               NetIPv6_ReTxPkt().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_TxPktDatagram (NET_BUF      *p_buf,
                                     NET_BUF_HDR  *p_buf_hdr,
                                     NET_ERR      *p_err)
{
                                                                /* --------------- SEL NEXT-ROUTE ADDR ---------------- */
    NetIPv6_TxPktDatagramRouteSel(p_buf,p_buf_hdr, p_err);


    switch (*p_err) {                                           /* --------------- TX IPv6 PKT DATAGRAM --------------- */
        case NET_IPv6_ERR_TX_DEST_LOCAL_HOST:
             p_buf_hdr->IF_NbrTx = NET_IF_NBR_LOCAL_HOST;
             NetIF_Tx(p_buf, p_err);
             break;


        case NET_IPv6_ERR_NONE:
        case NET_IPv6_ERR_TX_DEST_MULTICAST:
             p_buf_hdr->IF_NbrTx = p_buf_hdr->IF_Nbr;
             NetIF_Tx(p_buf, p_err);
             break;


        case NET_IPv6_ERR_TX_DEST_INVALID:
        case NET_IPv6_ERR_INVALID_ADDR_HOST:
        case NET_IPv6_ERR_NEXT_HOP:
        default:
             return;
    }
}


/*
*********************************************************************************************************
*                                  NetIPv6_TxPktDatagramRouteSel()
*
* Description : (1) Configure next-route IPv6 address for transmit IPv6 packet datagram :
*
*                   (a) Select next-route IPv6 address :
*                       (1) Destination is this host                    See Note #3
*                       (2) Link-local Host                             See Note #2a
*                       (3) Remote Host                                 See Note #2a
*                       (4) Multicast                                   See Note #2c
*
*                   (b) Configure next-route IPv6 address for all buffers in buffer list.
*
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header of IPv6 transmit packet.
*               --------    Argument validated in NetIPv6_Tx(),
*                                                 NetIPv6_ReTx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_TX_DEST_LOCAL_HOST      Destination is a local host address.
*                               NET_IPv6_ERR_TX_DEST_MULTICAST       Multicast destination.
*
*                                                                    --- RETURNED by NetNDP_NextHopByIF() ---
*                               NET_IPv6_ERR_TX_DEST_HOST_THIS_NET   Destination is on same link.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY         Next-hop is the default router.
*                               NET_IPv6_ERR_TX_DFLT_GATEWAY_NONE    Next-hop is a router in the router list.
*                               NET_IPv6_ERR_TX_NEXT_HOP_NONE        No next-hop is available.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_TxPktDatagram().
*
* Note(s)     :(2) RFC #4861, Section 5.2 Next-hop determination :
*                   (a) "The sender performs a longest prefix match against the Prefix List to determine
*                        whether the packet's destination is on- or off-link.  If the destination is on-link,
*                        the next-hop address is the same as the packet's destination address. Otherwise,
*                        the sender selects a router from the Default Router List."
*
*                   (b) " For efficiency reasons, next-hop determination is not performed on every packet
*                         that is sent. Instead, the results of next-hop determination computations are saved
*                         in the Destination Cache (which also contains updates learned from Redirect messages).
*                         When the sending node has a packet to send, it first examines the Destination Cache.
*                         If no entry exists for the destination, next-hop determination is invoked to create
*                         a Destination Cache entry."
*
*                   (c) "For multicast packets, the next-hop is always the (multicast) destination address
*                        and is considered to be on-link.
*********************************************************************************************************
*/

static  void  NetIPv6_TxPktDatagramRouteSel (NET_BUF      *p_buf,
                                             NET_BUF_HDR  *p_buf_hdr,
                                             NET_ERR      *p_err)
{
#if 0
    NET_IPv6_HDR        *p_ip_hdr;
#endif
           NET_BUF             *p_buf_list;
           NET_BUF_HDR         *p_buf_list_hdr;
           NET_IF_NBR           if_nbr;
           NET_IF_NBR           if_nbr_found;
    const  NET_IPv6_ADDR       *p_addr_next_hop;
    const  NET_IPv6_ADDR       *p_addr_dest;
           CPU_BOOLEAN          addr_mcast;


#if 0
    p_ip_hdr    = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
#endif

    if_nbr      =  p_buf_hdr->IF_Nbr;
    p_addr_dest = &p_buf_hdr->IPv6_AddrDest;

                                                                /* Multicast Address destination.                       */
    addr_mcast = NetIPv6_IsAddrMcast(p_addr_dest);
    if (addr_mcast == DEF_TRUE) {
        p_addr_next_hop = p_addr_dest;
       *p_err = NET_IPv6_ERR_TX_DEST_MULTICAST;
        goto exit;
    }

                                                                /* Verify that addr dest is not an addr on current IF.  */
    if_nbr_found = NetIPv6_GetAddrHostIF_Nbr(p_addr_dest);
    if (if_nbr == if_nbr_found) {
        p_addr_next_hop = p_addr_dest;
       *p_err = NET_IPv6_ERR_TX_DEST_LOCAL_HOST;
        goto exit;
    }

#ifdef  NET_NDP_MODULE_EN
                                                                /* ---------------- FIND NEXT HOP ADDRESS ------------- */
    p_addr_next_hop = NetNDP_NextHopByIF(if_nbr,
                                         p_addr_dest,
                                         p_err);
    switch (*p_err) {
        case NET_NDP_ERR_TX_DEST_MULTICAST:
        case NET_NDP_ERR_TX_DEST_HOST_THIS_NET:
        case NET_NDP_ERR_TX_DFLT_GATEWAY:
        case NET_NDP_ERR_TX_NO_DFLT_GATEWAY:
            *p_err = NET_IPv6_ERR_NONE;
             break;


        case NET_NDP_ERR_TX_NO_NEXT_HOP:
        default:
            *p_err = NET_IPv6_ERR_NEXT_HOP;
             break;
    }
#else
    *p_err = NET_IPv6_ERR_NEXT_HOP;
#endif


exit:
    if (*p_err != NET_IPv6_ERR_NEXT_HOP) {
         p_buf_list     = p_buf;
         p_buf_list_hdr = p_buf_hdr;
         while (p_buf_list != (NET_BUF *)0) {
            p_buf_list_hdr->IPv6_AddrNextRoute = *p_addr_next_hop;
            p_buf_list     = p_buf_list_hdr->NextBufPtr;
            p_buf_list_hdr = &p_buf_list->Hdr;
        }

    } else {
                                                                /* ICMPv6 Destination Unreachable Error message should ...  */
                                                                /* ... be sent. #### NET-781                                */
    }
}


/*
*********************************************************************************************************
*                                       NetIPv6_TxPktDiscard()
*
* Description : On any IPv6 transmit packet error(s), discard packet & buffer.
*
* Argument(s) : p_buf        Pointer to network buffer.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_TxPktDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IPv6.TxPktDisCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)p_ctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                          NetIPv6_ReTxPkt()
*
* Description : (1) Prepare & re-transmit IPv6 packet :
*
*                   (a) Prepare     IPv6 header
*                   (b) Re-transmit IPv6 packet datagram
*
*
* Argument(s) : p_buf       Pointer to network buffer to re-transmit IPv6 packet.
*               ----        Argument checked   in NetIPv6_ReTx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetIPv6_ReTx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   -- RETURNED BY NetIPv6_TxPktDatagram() : --
*                               NET_IF_ERR_NONE                     Packet successfully transmitted.
*                               NET_ERR_IF_LOOPBACK_DIS             Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN                Network  interface link state down (i.e.
*                                                                       NOT available for receive or transmit).
*                               NET_IPv6_ERR_INVALID_ADDR_HOST      Invalid IPv6 host    address.
*                               NET_IPv6_ERR_TX_DEST_INVALID        Invalid transmit destination.
*                               NET_ERR_TX                          Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_ReTx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_ReTxPkt (NET_BUF      *p_buf,
                               NET_BUF_HDR  *p_buf_hdr,
                               NET_ERR      *p_err)
{
                                                                /* ----------------- PREPARE IPv6 HDR ----------------- */
    NetIPv6_ReTxPktPrepareHdr(p_buf,
                            p_buf_hdr,
                            p_err);
    if (*p_err != NET_IPv6_ERR_NONE) {
         return;
    }

                                                                /* ------------- RE-TX IPv6 PKT DATAGRAM -------------- */
    NetIPv6_TxPktDatagram(p_buf, p_buf_hdr, p_err);
}


/*
*********************************************************************************************************
*                                     NetIPv6_ReTxPktPrepareHdr()
*
* Description : (1) Prepare IPv6 header for re-transmit IPv6 packet :
*
*                   (a) Update network buffer's protocol & length controls
*
*                   (b) (1) Version
*                       (2) Traffic Class
*                       (3) Flow Label
*                       (4) Payload Length
*                       (5) Next Header
*                       (6) Hop Limit
*                       (7) Source      Address
*                       (8) Destination Address
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit IPv6 packet.
*               ----        Argument checked   in NetIPv6_Tx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetIPv6_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv6_ERR_NONE               IPv6 header successfully prepared.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv6_ReTxPkt().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv6_ReTxPktPrepareHdr (NET_BUF      *p_buf,
                                         NET_BUF_HDR  *p_buf_hdr,
                                         NET_ERR      *p_err)
{
#if 0
    NET_IPv6_HDR  *p_ip_hdr;
#endif

                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr->ProtocolHdrType    =  NET_PROTOCOL_TYPE_IP_V6;   /* Update buf protocol for IPv6.                        */
    p_buf_hdr->ProtocolHdrTypeNet =  NET_PROTOCOL_TYPE_IP_V6;
                                                                /* Reset tot len for re-tx.                             */
    p_buf_hdr->TotLen             = (NET_BUF_SIZE)p_buf_hdr->IP_TotLen;


                                                                /* ----------------- PREPARE IPv6 HDR ----------------- */
#if 0
    p_ip_hdr = (NET_IPv6_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
#endif

   (void)&p_buf;

   *p_err = NET_IPv6_ERR_NONE;
}


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_ipv6.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of IPv6 module include (see Note #1).            */

