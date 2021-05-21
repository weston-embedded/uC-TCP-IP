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
*                                     NETWORK IP LAYER VERSION 4
*                                       (INTERNET PROTOCOL V4)
*
* Filename : net_ipv4.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Internet Protocol as described in RFC #791, also known as IPv4, with the
*                following restrictions/constraints :
*
*                (a) ONLY supports a single default gateway                RFC #1122, Section 3.3.1
*                        per interface
*
*                (b) IPv4 forwarding/routing NOT currently supported       RFC #1122, Sections 3.3.1,
*                                                                                      3.3.4 & 3.3.5
*
*                (c) Transmit fragmentation  NOT currently supported       RFC # 791, Section 2.3
*                                                                                      'Fragmentation &
*                                                                                         Reassembly'
*                (d) IPv4 Security options   NOT           supported       RFC #1108
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
#define    NET_IPv4_MODULE

#include  "net_ipv4.h"
#include  "net_icmpv4.h"
#include  "net_igmp.h"
#include  "../../Source/net_cfg_net.h"
#include  "../../Source/net_buf.h"
#include  "../../Source/net_util.h"
#include  "../../Source/net.h"
#include  "../../Source/net_udp.h"
#include  "../../Source/net_tcp.h"
#include  "../../Source/net_conn.h"
#include  "../../IF/net_if.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         IPv4 HEADER DEFINES
*********************************************************************************************************
*/

#define  NET_IPv4_HDR_VER_MASK                          0xF0u
#define  NET_IPv4_HDR_VER_SHIFT                            4u
#define  NET_IPv4_HDR_VER                                  4u   /* Supports IPv4 ONLY (see 'net_ipv4.h  Note #1').      */


#define  NET_IPv4_HDR_LEN_MASK                          0x0Fu

#define  NET_IPv4_ID_INIT                                NET_IPv4_ID_NONE


/*
*********************************************************************************************************
*                                     IPv4 HEADER OPTIONS DEFINES
*
* Note(s) : (1) See the following RFC's for IPv4 options summary :
*
*               (a) RFC # 791, Section 3.1     'Options'
*               (b) RFC #1122, Section 3.2.1.8
*               (c) RFC #1108
*
*           (2) IPv4 option types are encoded in the first octet for each IP option as follows :
*
*                         7   6 5  4 3 2 1 0
*                       ---------------------
*                       |CPY|CLASS|  N B R  |
*                       ---------------------
*
*                   where
*                           CPY         Indicates whether option is copied into all fragments :
*                                           '0' - IP option NOT copied into fragments
*                                           '1' - IP option     copied into fragments
*                           CLASS       Indicates options class :
*                                           '00' - Control
*                                           '01' - Reserved
*                                           '10' - Debug / Measurement
*                                           '11' - Reserved
*                           NBR         Option Number :
*                                           '00000' - End of Options List
*                                           '00001' - No Operation
*                                           '00010' - Security
*                                           '00011' - Loose  Source Routing
*                                           '00100' - Internet Timestamp
*                                           '00111' - Record Route
*                                           '01001' - Strict Source Routing
*
*           (3) IPv4 header allows for a maximum option list length of ten (10) 32-bit options :
*
*                   NET_IPv4_HDR_OPT_SIZE_MAX = (NET_IPv4_HDR_SIZE_MAX - NET_IPv4_HDR_SIZE_MIN) / NET_IPv4_HDR_OPT_SIZE_WORD
*
*                                           = (60 - 20) / (32-bits)
*
*                                           =  Ten (10) 32-bit options
*
*           (4) 'NET_IPv4_OPT_SIZE'  MUST be pre-defined PRIOR to all definitions that require IPv4 option
*                size data type.
*********************************************************************************************************
*/

#define  NET_IPv4_HDR_OPT_COPY_FLAG               DEF_BIT_07

#define  NET_IPv4_HDR_OPT_CLASS_MASK                    0x60u
#define  NET_IPv4_HDR_OPT_CLASS_CTRL                    0x00u
#define  NET_IPv4_HDR_OPT_CLASS_RESERVED_1              0x20u
#define  NET_IPv4_HDR_OPT_CLASS_DBG                     0x40u
#define  NET_IPv4_HDR_OPT_CLASS_RESERVED_2              0x60u

#define  NET_IPv4_HDR_OPT_NBR_MASK                      0x1Fu
#define  NET_IPv4_HDR_OPT_NBR_END_LIST                  0x00u
#define  NET_IPv4_HDR_OPT_NBR_NOP                       0x01u
#define  NET_IPv4_HDR_OPT_NBR_SECURITY                  0x02u   /* See 'net_ipv4.h  Note #1d'.                          */
#define  NET_IPv4_HDR_OPT_NBR_ROUTE_SRC_LOOSE           0x03u
#define  NET_IPv4_HDR_OPT_NBR_SECURITY_EXTENDED         0x05u   /* See 'net_ipv4.h  Note #1d'.                          */
#define  NET_IPv4_HDR_OPT_NBR_TS                        0x04u
#define  NET_IPv4_HDR_OPT_NBR_ROUTE_REC                 0x07u
#define  NET_IPv4_HDR_OPT_NBR_ROUTE_SRC_STRICT          0x09u

#define  NET_IPv4_HDR_OPT_END_LIST             (                             NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_END_LIST         )
#define  NET_IPv4_HDR_OPT_NOP                  (                             NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_NOP              )
#define  NET_IPv4_HDR_OPT_SECURITY             (NET_IPv4_HDR_OPT_COPY_FLAG | NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_SECURITY         )
#define  NET_IPv4_HDR_OPT_ROUTE_SRC_LOOSE      (NET_IPv4_HDR_OPT_COPY_FLAG | NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_ROUTE_SRC_LOOSE  )
#define  NET_IPv4_HDR_OPT_SECURITY_EXTENDED    (NET_IPv4_HDR_OPT_COPY_FLAG | NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_SECURITY_EXTENDED)
#define  NET_IPv4_HDR_OPT_TS                   (                             NET_IPv4_HDR_OPT_CLASS_DBG  | NET_IPv4_HDR_OPT_NBR_TS               )
#define  NET_IPv4_HDR_OPT_ROUTE_REC            (                             NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_ROUTE_REC        )
#define  NET_IPv4_HDR_OPT_ROUTE_SRC_STRICT     (NET_IPv4_HDR_OPT_COPY_FLAG | NET_IPv4_HDR_OPT_CLASS_CTRL | NET_IPv4_HDR_OPT_NBR_ROUTE_SRC_STRICT )

#define  NET_IPv4_HDR_OPT_PAD                           0x00u

                                                                /* ---------------- SRC/REC ROUTE OPTS ---------------- */
#define  NET_IPv4_OPT_ROUTE_PTR_OPT                        0    /* Ptr ix to       route opt itself.                    */
#define  NET_IPv4_OPT_ROUTE_PTR_ROUTE                      4    /* Ptr ix to first route (min legal ptr val).           */

                                                                /* --------------------- TS OPTS ---------------------- */
#define  NET_IPv4_OPT_TS_PTR_OPT                           0    /* Ptr ix to       TS    opt itself.                    */
#define  NET_IPv4_OPT_TS_PTR_TS                            4    /* Ptr ix to first TS    (min legal ptr val).           */

#define  NET_IPv4_OPT_TS_OVF_MASK                       0xF0u
#define  NET_IPv4_OPT_TS_OVF_SHIFT                         4u
#define  NET_IPv4_OPT_TS_OVF_MAX                          15u

#define  NET_IPv4_OPT_TS_FLAG_MASK                      0x0Fu
#define  NET_IPv4_OPT_TS_FLAG_TS_ONLY                      0u
#define  NET_IPv4_OPT_TS_FLAG_TS_ROUTE_REC                 1u
#define  NET_IPv4_OPT_TS_FLAG_TS_ROUTE_SPEC                3u




#define  NET_IPv4_HDR_OPT_SIZE_ROUTE                     NET_IPv4_HDR_OPT_SIZE_WORD
#define  NET_IPv4_HDR_OPT_SIZE_TS                        NET_IPv4_HDR_OPT_SIZE_WORD
#define  NET_IPv4_HDR_OPT_SIZE_SECURITY                    3

#define  NET_IPv4_OPT_PARAM_NBR_MIN                        1

#define  NET_IPv4_OPT_PARAM_NBR_MAX_ROUTE                  9
#define  NET_IPv4_OPT_PARAM_NBR_MAX_TS_ONLY                9
#define  NET_IPv4_OPT_PARAM_NBR_MAX_TS_ROUTE               4

#define  NET_IPv4_HDR_OPT_IX                             NET_IPv4_HDR_SIZE_MIN
#define  NET_IPv4_OPT_IX_RX                                0


/*
*********************************************************************************************************
*                              IPv4 ADDRESS CONFIGURATION STATE DEFINES
*********************************************************************************************************
*/

#define  NET_IPv4_ADDR_CFG_STATE_NONE                      0u
#define  NET_IPv4_ADDR_CFG_STATE_STATIC                   10u
#define  NET_IPv4_ADDR_CFG_STATE_DYNAMIC                  20u
#define  NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT             21u




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
*                                 IPv4 SOURCE ROUTE OPTION DATA TYPE
*
* Note(s) : (1) See the following RFC's for Source Route options summary :
*
*               (a) RFC # 791, Section 3.1         'Options : Loose/Strict Source & Record Route'
*               (b) RFC #1122, Section 3.2.1.8.(c)
*
*           (2) Used for both Source Route options & Record Route options :
*
*               (a) NET_IPv4_HDR_OPT_ROUTE_SRC_LOOSE
*               (b) NET_IPv4_HDR_OPT_ROUTE_SRC_STRICT
*               (c) NET_IPv4_HDR_OPT_ROUTE_REC
*
*           (3) 'Route' declared with 1 entry. We may loop beyond the bounds of the array while we convert
*               the IP adrresses from net order to host order. This is because we use '.Ptr' to determine
*               how many items (IPv4 addresses) there are.
*********************************************************************************************************
*/

typedef  struct  net_ipv4_opt_src_route {
    CPU_INT08U          Type;                                   /* Src route type (see Note #2).                        */
    CPU_INT08U          Len;                                    /* Len of   src route opt (in octets).                  */
    CPU_INT08U          Ptr;                                    /* Ptr into src route opt (octet-ix'd).                 */
    CPU_INT08U          Pad;                                    /* Forced word-alignment pad octet.                     */
    NET_IPv4_ADDR       Route[1];                               /* Src route IPv4 addrs (see Note #3).                  */
} NET_IPv4_OPT_SRC_ROUTE;


/*
*********************************************************************************************************
*                              IPv4 INTERNET TIMESTAMP OPTION DATA TYPE
*
* Note(s) : (1) See the following RFC's for Internet Timestamp option summary :
*
*               (a) RFC # 791, Section 3.1         'Options : Internet Timestamp'
*               (b) RFC #1122, Section 3.2.1.8.(e)
*
*           (2) 'TS'/'Route'/'Route_TS' declared with 1 entry. We may loop beyond the bounds of the array
*               while we convert the IP adrresses from net order to host order. This is because we use '.Ptr'
*               to determine how many items (timestamps, IPv4 addresses, or a combination of both) there are.
*********************************************************************************************************
*/

typedef  struct  net_ipv4_opt_ts {
    CPU_INT08U          Type;                                   /* TS type.                                             */
    CPU_INT08U          Len;                                    /* Len of   src route opt (in octets).                  */
    CPU_INT08U          Ptr;                                    /* Ptr into src route opt (octet-ix'd).                 */
    CPU_INT08U          Ovf_Flags;                              /* Ovf/Flags.                                           */
    NET_TS              TS[1];                                  /* Timestamps (see Note #2).                            */
} NET_IPv4_OPT_TS;



typedef  struct  net_ipv4_route_ts {
    NET_IPv4_ADDR       Route[1];                               /* Route IPv4 addrs (see Note #2).                      */
    NET_TS              TS[1];                                  /* Timestamps       (see Note #2).                      */
} NET_IPv4_ROUTE_TS;

#define  NET_IPv4_OPT_TS_ROUTE_SIZE              (sizeof(NET_IPv4_ROUTE_TS))


typedef  struct  net_ipv4_opt_ts_route {
    CPU_INT08U          Type;                                   /* TS type.                                             */
    CPU_INT08U          Len;                                    /* Len of   src route opt (in octets).                  */
    CPU_INT08U          Ptr;                                    /* Ptr into src route opt (octet-ix'd).                 */
    CPU_INT08U          Ovf_Flags;                              /* Ovf/Flags.                                           */
    NET_IPv4_ROUTE_TS   Route_TS[1];                            /* Route IPv4 addrs / TS (see Note #2).                 */
} NET_IPv4_OPT_TS_ROUTE;


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

static  NET_BUF          *NetIPv4_FragReasmListsHead;     /* Ptr to head of frag reasm lists.                     */
static  NET_BUF          *NetIPv4_FragReasmListsTail;     /* Ptr to tail of frag reasm lists.                     */

static  CPU_INT08U        NetIPv4_FragReasmTimeout_sec;   /* IPv4 frag reasm timeout (in secs ).                  */
static  NET_TMR_TICK      NetIPv4_FragReasmTimeout_tick;  /* IPv4 frag reasm timeout (in ticks).                  */


static  CPU_INT16U        NetIPv4_TxID_Ctr;               /* Global tx ID field ctr.                              */




/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                            /* -------------- CFG FNCTS --------------- */
static  CPU_BOOLEAN      NetIPv4_CfgAddrRemoveAllHandler (NET_IF_NBR         if_nbr,
                                                          NET_ERR           *p_err);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
static  void             NetIPv4_CfgAddrValidate         (NET_IPv4_ADDR   addr_host,
                                                          NET_IPv4_ADDR   addr_subnet_mask,
                                                          NET_IPv4_ADDR   addr_dflt_gateway,
                                                          NET_ERR        *p_err);
#endif

                                                                            /* -------------- GET FNCTS --------------- */

static  NET_IPv4_ADDRS  *NetIPv4_GetAddrsHostCfgd        (NET_IPv4_ADDR   addr,
                                                          NET_IF_NBR     *p_if_nbr);

static  NET_IPv4_ADDRS  *NetIPv4_GetAddrsHostCfgdOnIF    (NET_IPv4_ADDR   addr,
                                                          NET_IF_NBR      if_nbr);



                                                                            /* -------- VALIDATE RX DATAGRAMS --------- */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void             NetIPv4_RxPktValidateBuf        (NET_BUF_HDR    *p_buf_hdr,
                                                          NET_ERR        *p_err);
#endif

static  void             NetIPv4_RxPktValidate           (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_IPv4_HDR   *p_ip_hdr,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_RxPktValidateOpt        (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_IPv4_HDR   *p_ip_hdr,
                                                          CPU_INT08U      ip_hdr_len_size,
                                                          NET_ERR        *p_err);

static  CPU_BOOLEAN    NetIPv4_RxPktValidateOptRoute     (NET_BUF_HDR    *p_buf_hdr,
                                                          CPU_INT08U     *p_opts,
                                                          CPU_INT08U      opt_list_len_rem,
                                                          CPU_INT08U     *p_opt_len,
                                                          NET_ERR        *p_err);

static  CPU_BOOLEAN    NetIPv4_RxPktValidateOptTS        (NET_BUF_HDR    *p_buf_hdr,
                                                          CPU_INT08U     *p_opts,
                                                          CPU_INT08U      opt_list_len_rem,
                                                          CPU_INT08U     *p_opt_len,
                                                          NET_ERR        *p_err);

                                                                                    /* -------- REASM RX FRAGS -------- */

static  NET_BUF       *NetIPv4_RxPktFragReasm            (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_IPv4_HDR   *p_ip_hdr,
                                                          NET_ERR        *p_err);

static  NET_BUF       *NetIPv4_RxPktFragListAdd          (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          CPU_INT16U      frag_ip_flags,
                                                          CPU_INT16U      frag_offset,
                                                          CPU_INT16U      frag_size,
                                                          NET_ERR        *p_err);

static  NET_BUF       *NetIPv4_RxPktFragListInsert       (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          CPU_INT16U      frag_ip_flags,
                                                          CPU_INT16U      frag_offset,
                                                          CPU_INT16U      frag_size,
                                                          NET_BUF        *p_frag_list,
                                                          NET_ERR        *p_err);

static  void           NetIPv4_RxPktFragListRemove       (NET_BUF        *p_frag_list,
                                                          CPU_BOOLEAN     tmr_free);

static  void           NetIPv4_RxPktFragListDiscard      (NET_BUF        *p_frag_list,
                                                          CPU_BOOLEAN     tmr_free,
                                                          NET_ERR        *p_err);

static  void           NetIPv4_RxPktFragListUpdate       (NET_BUF        *p_frag_list,
                                                          NET_BUF_HDR    *p_frag_list_buf_hdr,
                                                          CPU_INT16U      frag_ip_flags,
                                                          CPU_INT16U      frag_offset,
                                                          CPU_INT16U      frag_size,
                                                          NET_ERR        *p_err);

static  NET_BUF         *NetIPv4_RxPktFragListChkComplete(NET_BUF        *p_frag_list,
                                                          NET_BUF_HDR    *p_frag_list_buf_hdr,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_RxPktFragTimeout        (void           *p_frag_list_timeout);


                                                                            /* ---------- DEMUX RX DATAGRAMS ---------- */

static  void             NetIPv4_RxPktDemuxDatagram      (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_ERR        *p_err);



static  void             NetIPv4_RxPktDiscard            (NET_BUF        *p_buf,
                                                          NET_ERR        *p_err);



                                                                                    /* ------- VALIDATE TX PKTS ------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void             NetIPv4_TxPktValidate           (NET_BUF_HDR      *p_buf_hdr,
                                                          NET_IPv4_ADDR     addr_src,
                                                          NET_IPv4_ADDR     addr_dest,
                                                          NET_IPv4_TOS      TOS,
                                                          NET_IPv4_TTL      TTL,
                                                          CPU_INT16U        flags,
                                                          void             *p_opts,
                                                          NET_ERR          *p_err);
#endif

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void             NetIPv4_TxPktValidateOpt        (void           *p_opts,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_TxPktValidateOptRouteTS (void           *p_opt_route_ts,
                                                          CPU_INT08U     *p_opt_len,
                                                          void          **p_opt_next,
                                                          NET_ERR        *p_err);
#endif

                                                                            /* ------------- TX IPv4 PKTS ------------- */

static  void             NetIPv4_TxPkt                   (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_IPv4_ADDR   addr_src,
                                                          NET_IPv4_ADDR   addr_dest,
                                                          NET_IPv4_TOS    TOS,
                                                          NET_IPv4_TTL    TTL,
                                                          CPU_INT16U      flags,
                                                          void           *p_opts,
                                                          NET_ERR        *p_err);

static  CPU_INT08U       NetIPv4_TxPktPrepareOpt         (void           *p_opts,
                                                          CPU_INT08U     *p_opt_hdr,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_TxPktPrepareOptRoute    (void           *p_opts,
                                                          CPU_INT08U     *p_opt_hdr,
                                                          CPU_INT08U     *p_opt_len,
                                                          void          **p_opt_next,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_TxPktPrepareOptTS       (void           *p_opts,
                                                          CPU_INT08U     *p_opt_hdr,
                                                          CPU_INT08U     *p_opt_len,
                                                          void          **p_opt_next,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_TxPktPrepareOptTSRoute  (void           *p_opts,
                                                          CPU_INT08U     *p_opt_hdr,
                                                          CPU_INT08U     *p_opt_len,
                                                          void          **p_opt_next,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_TxPktPrepareHdr         (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          CPU_INT16U      ip_hdr_len_tot,
                                                          CPU_INT08U      ip_opt_len_tot,
                                                          CPU_INT16U      protocol_ix,
                                                          NET_IPv4_ADDR   addr_src,
                                                          NET_IPv4_ADDR   addr_dest,
                                                          NET_IPv4_TOS    TOS,
                                                          NET_IPv4_TTL    TTL,
                                                          CPU_INT16U      flags,
                                                          CPU_INT32U     *p_ip_hdr_opts,
                                                          NET_ERR        *p_err);

                                                                                    /* -------- TX IP DATAGRAMS ------- */

static  void             NetIPv4_TxPktDatagram           (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_TxPktDatagramRouteSel   (NET_BUF_HDR    *p_buf_hdr,
                                                          NET_ERR        *p_err);


static  void             NetIPv4_TxPktDiscard            (NET_BUF        *p_buf,
                                                          NET_ERR        *p_err);


                                                                            /* ----------- RE-TX IPv4 PKTS ------------ */

static  void             NetIPv4_ReTxPkt                 (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_ERR        *p_err);

static  void             NetIPv4_ReTxPktPrepareHdr       (NET_BUF        *p_buf,
                                                          NET_BUF_HDR    *p_buf_hdr,
                                                          NET_ERR        *p_err);

static  CPU_BOOLEAN      NetIPv4_IsAddrHostHandler       (NET_IPv4_ADDR   addr);

static  CPU_BOOLEAN      NetIPv4_IsAddrHostCfgdHandler   (NET_IPv4_ADDR   addr);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                              FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            NetIPv4_Init()
*
* Description : (1) Initialize Internet Protocol Layer :
*
*                   (a) Initialize ALL interfaces' configurable IPv4 addresses
*                   (b) Initialize IPv4 fragmentation list pointers
*                   (c) Initialize IPv4 identification (ID) counter
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) (a) Default IPv4 address initialization is invalid & forces the developer or higher-layer
*                       protocol application to configure valid IPv4 address(s).
*
*                   (b) Address configuration state initialized to 'static' by default.
*
*                       See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                                'NetIPv4_CfgAddrRemove()     Note #5c',
*                              & 'NetIPv4_CfgAddrRemoveAll()  Note #4c'.
*
*                   See also 'NetIPv4_CfgAddrAdd()         Note #7',
*                            'NetIPv4_CfgAddrAddDynamic()  Note #8',
*                            'NetIPv4_CfgAddrRemove()      Note #5',
*                          & 'NetIPv4_CfgAddrRemoveAll()   Note #4'.
*********************************************************************************************************
*/

void  NetIPv4_Init (void)
{
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    NET_IF_NBR         if_nbr;


                                                                    /* --------------- INIT IPv4 ADDRS ---------------- */
                                                                    /* Init ALL IF's IPv4 addrs to NONE (see Note #2a). */
    for (if_nbr = NET_IF_NBR_BASE; if_nbr < NET_IF_NBR_IF_TOT; if_nbr++) {
        p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

        for (addr_ix = 0u; addr_ix < NET_IPv4_CFG_IF_MAX_NBR_ADDR; addr_ix++) {
            p_ip_addrs = &p_ip_if_cfg->AddrsTbl[addr_ix];
            p_ip_addrs->AddrHost               = NET_IPv4_ADDR_NONE;
            p_ip_addrs->AddrHostSubnetMask     = NET_IPv4_ADDR_NONE;
            p_ip_addrs->AddrHostSubnetMaskHost = NET_IPv4_ADDR_NONE;
            p_ip_addrs->AddrHostSubnetNet      = NET_IPv4_ADDR_NONE;
            p_ip_addrs->AddrDfltGateway        = NET_IPv4_ADDR_NONE;
        }

        p_ip_if_cfg->AddrsNbrCfgd         = 0u;
        p_ip_if_cfg->AddrCfgState         = NET_IPv4_ADDR_CFG_STATE_STATIC;  /* Init to static addr cfg (see Note #2b).  */
        p_ip_if_cfg->AddrProtocolConflict = DEF_NO;
    }


                                                                    /* ------------- INIT IPv4 FRAG LISTS ------------- */
    NetIPv4_FragReasmListsHead = (NET_BUF *)0;
    NetIPv4_FragReasmListsTail = (NET_BUF *)0;

                                                                    /* --------------- INIT IPv4 ID CTR --------------- */
    NetIPv4_TxID_Ctr           =  NET_IPv4_ID_INIT;
}


/*
*********************************************************************************************************
*                                         NetIPv4_CfgAddrAdd()
*
* Description : (1) Add a statically-configured IPv4 host address, subnet mask, & default gateway to an interface :
*
*                   (a) Acquire   network lock
*                   (b) Validate  address configuration :
*                       (1) Validate interface
*                       (2) Validate IPv4 address, subnet mask, & default gateway
*                       (3) Validate IPv4 address configured
*                       (4) Validate IPv4 address configuration state                       See Note #7b1
*                       (5) Validate number of    configured IP addresses
*
*                   (c) Configure static IPv4 address                                       See Note #7a1
*                   (d) Release   network lock
*
*
*               (2) Configured IPv4 addresses are organized in an address table implemented as an array :
*
*                   (a) (1) (A) NET_IPv4_CFG_IF_MAX_NBR_ADDR configures each interface's maximum number of
*                               configured IPv4 addresses.
*
*                           (B) This value is used to declare the size of each interface's address table.
*
*                       (2) Each configurable interface's 'AddrsNbrCfgd' indicates the current number of
*                           configured IPv4 addresses.
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
*                           value NET_IPv4_ADDR_NONE, at ALL table indices following configured addresses.
*
*                               where
*                                       M       maximum number of configured addresses (see Note #2a1A)
*                                       N       current number of configured addresses (see Note #2a1B)
*
*
*                                                    Configured IPv4
*                                                    Addresses Table
*                                                     (see Note #2)
*
*                                -------------------------------------------------------       -----            -----
*                                |  Cfg'd Addr #0  | Subnet Mask #0  | Dflt Gateway #0 |         ^                ^
*                                |-----------------|-----------------|-----------------|         |                |
*                                |  Cfg'd Addr #1  | Subnet Mask #1  | Dflt Gateway #1 |                          |
*                                |-----------------|-----------------|-----------------|   Current number         |
*                                |  Cfg'd Addr #2  | Subnet Mask #2  | Dflt Gateway #2 |   of configured          |
*                                |-----------------|-----------------|-----------------|   IPv4 addresses         |
*                                |        .        |        .        |        .        |  on an interface         |
*                                |        .        |        .        |        .        |  (see Note #2a2)
*                                |        .        |        .        |        .        |                    Maximum number
*                                |-----------------|-----------------|-----------------|         |          of configured
*                                |  Cfg'd Addr #N  | Subnet Mask #N  | Dflt Gateway #N |         v          IPv4 addresses
*      Next available            |-----------------|-----------------|-----------------|       -----       for an interface
*   address to configure  -----> |    ADDR NONE    |    ADDR NONE    |    ADDR NONE    |         ^         (see Note #2a1)
*      (see Note #2b3)           |-----------------|-----------------|-----------------|         |
*                                |        .        |        .        |        .        |                          |
*                                |        .        |        .        |        .        |   Non-configured         |
*                                |        .        |        .        |        .        |  address entries         |
*                                |        .        |        .        |        .        |  (see Note #2b4)         |
*                                |        .        |        .        |        .        |                          |
*                                |-----------------|-----------------|-----------------|         |                |
*                                |    ADDR NONE    |    ADDR NONE    |    ADDR NONE    |         v                v
*                                -------------------------------------------------------       -----            -----
*
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1a'.
*
* Argument(s) : if_nbr              Interface number to configure.
*
*               addr_host           Desired IPv4 address to add to this interface     (see Note #5).
*
*               addr_subnet_mask    Desired IPv4 address subnet mask     to configure (see Note #5).
*
*               addr_dflt_gateway   Desired IPv4 default gateway address to configure (see Note #5).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   IPv4 address successfully configured.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                               NET_IPv4_ERR_ADDR_CFG_STATE         Invalid IPv4 address configuration state --
*                                                                       NOT in static address configuration
*                                                                       (see Note #7b1).
*                               NET_IPv4_ERR_ADDR_TBL_FULL          Interface's configured IPv4 address table full.
*                               NET_IPv4_ERR_ADDR_CFG_IN_USE        IPv4 address already configured on an interface.
*
*                                                                   -- RETURNED BY NetIPv4_CfgAddrValidate() : ---
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid IPv4 address, subnet mask, or address/
*                                                                       subnet mask combination.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 default gateway address.
*
*                                                                   -- RETURNED BY NetIF_IsValidCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if valid IPv4 address, subnet mask, & default gateway configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #3].
*
* Note(s)     : (3) NetIPv4_CfgAddrAdd() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (4) NetIPv4_CfgAddrAdd() blocked until network initialization completes.
*
*               (5) IPv4 addresses MUST be in host-order.
*
*               (6) (a) RFC #1122, Section 3.3.1.6 states that "a manual method of entering ... the following
*                       ... configuration data MUST be provided" :
*
*                       (1) "IPv4 address(es)"
*                       (2) "Address mask(s)"
*                       (3) "A list of default gateways"
*
*                   (b) (1) RFC #1122, Section 3.3.1.1 states that "the host IPv4 layer MUST operate correctly
*                           in a minimal network environment, and in particular, when there are no gateways".
*
*                           In other words, a host on an isolated network should be able to correctly operate
*                           & communicate with all other hosts on its local network without need of a gateway
*                           or configuration of a gateway.
*
*                       (2) However, a configured gateway MUST be on the same network as the host's IPv4 address
*                           -- i.e. the network portion of the configured IPv4 address & the configured gateway
*                           addresses MUST be identical.
*
*                       See also 'NetIPv4_CfgAddrValidate()  Note #2'.
*
*               (7) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) (1) If an interface's address(s) are NOT already statically-configured, NO
*                           statically-configured address(s) may be added.
*
*                       (2) Any dynamically-configured address MUST be removed before adding any
*                           statically-configured address(s).
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4_CfgAddrAddDynamic()  Note #8',
*                            'NetIPv4_CfgAddrRemove()      Note #5',
*                          & 'NetIPv4_CfgAddrRemoveAll()   Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgAddrAdd (NET_IF_NBR      if_nbr,
                                 NET_IPv4_ADDR   addr_host,
                                 NET_IPv4_ADDR   addr_subnet_mask,
                                 NET_IPv4_ADDR   addr_dflt_gateway,
                                 NET_ERR        *p_err)
{
    CPU_BOOLEAN       addr_cfgd;
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    NET_IPv4_ADDRS   *p_ip_addrs;
    CPU_BOOLEAN       result;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                        /* ----------- VALIDATE RTN ERR PTR ----------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                        /* ------------- ACQUIRE NET LOCK ------------- */
    Net_GlobalLockAcquire((void *)&NetIPv4_CfgAddrAdd, p_err);          /* See Note #3b.                                */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #4).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

                                                                        /* ------------- VALIDATE IF NBR -------------- */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }

                                                                        /* ------------ VALIDATE IP ADDRS ------------- */
    NetIPv4_CfgAddrValidate(addr_host, addr_subnet_mask, addr_dflt_gateway, p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         goto exit_fail;
    }
#endif

                                                                        /* Validate host addr cfg'd.                    */
    addr_cfgd = NetIPv4_IsAddrHostCfgdHandler(addr_host);
    if (addr_cfgd != DEF_NO) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvAddrInUseCtr);
       *p_err =  NET_IPv4_ERR_ADDR_CFG_IN_USE;
        goto exit_fail;
    }



                                                                        /* ------- VALIDATE IPv4 ADDR CFG STATE ------- */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    switch (p_ip_if_cfg->AddrCfgState) {
        case NET_IPv4_ADDR_CFG_STATE_STATIC:                            /* See Note #7b1.                               */
             break;


        case NET_IPv4_ADDR_CFG_STATE_NONE:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
            *p_err =  NET_IPv4_ERR_ADDR_CFG_STATE;
             goto exit_fail;
    }


                                                                        /* ----- VALIDATE NBR CFG'D IPv4 ADDRS -------- */
    if (p_ip_if_cfg->AddrsNbrCfgd >= NET_IPv4_CFG_IF_MAX_NBR_ADDR) {    /* If nbr cfg'd addrs >= max, ...               */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrTblFullCtr);
       *p_err =  NET_IPv4_ERR_ADDR_TBL_FULL;                            /* ... rtn tbl full.                            */
        goto exit_fail;
    }


                                                                        /* ----------- CFG STATIC IPv4 ADDR ----------- */
    p_ip_addrs                         = &p_ip_if_cfg->AddrsTbl[p_ip_if_cfg->AddrsNbrCfgd];
    p_ip_addrs->AddrHost               =  addr_host;
    p_ip_addrs->AddrHostSubnetMask     =              addr_subnet_mask;
    p_ip_addrs->AddrHostSubnetMaskHost =             ~addr_subnet_mask;
    p_ip_addrs->AddrHostSubnetNet      =  addr_host & addr_subnet_mask;
    p_ip_addrs->AddrDfltGateway        =  addr_dflt_gateway;

    p_ip_if_cfg->AddrsNbrCfgd++;
#if 0                                                                   /* See Note #7b1.                               */
                                                                        /* Set to static  addr cfg (see Note #7a1).     */
    p_ip_if_cfg->AddrCfgState          =  NET_IPv4_ADDR_CFG_STATE_STATIC;
#endif
    CPU_CRITICAL_ENTER();
    p_ip_if_cfg->AddrProtocolConflict  =  DEF_NO;                       /* Clr    dynamic addr conflict.                */
    CPU_CRITICAL_EXIT();


    result = DEF_OK;
   *p_err  = NET_IPv4_ERR_NONE;
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

exit_fail:
    result = DEF_FAIL;

exit_release:
                                                                        /* ------------- RELEASE NET LOCK ------------- */
    Net_GlobalLockRelease();

    return (result);
}


/*
*********************************************************************************************************
*                                      NetIPv4_CfgAddrAddDynamic()
*
* Description : (1) Add a dynamically-configured IPv4 host address, subnet mask, & default gateway to
*                   an interface :
*
*                   (a) Acquire    network lock
*                   (b) Validate   address configuration :
*                       (1) Validate interface
*                       (2) Validate IPv4 address, subnet mask, & default gateway
*                       (3) Validate IPv4 address configured
*                       (4) Validate IPv4 address configuration state                   See Note #8b
*
*                   (c) Remove ALL configured IPv4 address(s)                           See Note #8c
*                   (d) Configure  dynamic    IPv4 address                              See Note #8a2
*                   (e) Release    network lock
*
*
*               (2) Configured IP addresses are organized in an address table implemented as an array :
*
*                   (a) (1) (A) NET_IPv4_CFG_IF_MAX_NBR_ADDR configures each interface's maximum number of
*                               configured IPv4 addresses.
*
*                           (B) This value is used to declare the size of each interface's address table.
*
*                       (2) Each configurable interface's 'AddrsNbrCfgd' indicates the current number of
*                           configured IPv4 addresses.
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
*                           value NET_IPv4_ADDR_NONE, at ALL table indices following configured addresses.
*
*                               where
*                                       M       maximum number of configured addresses (see Note #2a1A)
*                                       N       current number of configured addresses (see Note #2a1B)
*
*
*                                                    Configured IPv4
*                                                    Addresses Table
*                                                     (see Note #2)
*
*                                -------------------------------------------------------       -----            -----
*                                |  Cfg'd Addr #0  | Subnet Mask #0  | Dflt Gateway #0 |         ^                ^
*                                |-----------------|-----------------|-----------------|         |                |
*                                |  Cfg'd Addr #1  | Subnet Mask #1  | Dflt Gateway #1 |                          |
*                                |-----------------|-----------------|-----------------|   Current number         |
*                                |  Cfg'd Addr #2  | Subnet Mask #2  | Dflt Gateway #2 |   of configured          |
*                                |-----------------|-----------------|-----------------|   IPv4 addresses         |
*                                |        .        |        .        |        .        |  on an interface         |
*                                |        .        |        .        |        .        |  (see Note #2a2)
*                                |        .        |        .        |        .        |                    Maximum number
*                                |-----------------|-----------------|-----------------|         |          of configured
*                                |  Cfg'd Addr #N  | Subnet Mask #N  | Dflt Gateway #N |         v          IPv4 addresses
*      Next available            |-----------------|-----------------|-----------------|       -----       for an interface
*   address to configure  -----> |    ADDR NONE    |    ADDR NONE    |    ADDR NONE    |         ^         (see Note #2a1)
*      (see Note #2b3)           |-----------------|-----------------|-----------------|         |
*                                |        .        |        .        |        .        |                          |
*                                |        .        |        .        |        .        |   Non-configured         |
*                                |        .        |        .        |        .        |  address entries         |
*                                |        .        |        .        |        .        |  (see Note #2b4)         |
*                                |        .        |        .        |        .        |                          |
*                                |-----------------|-----------------|-----------------|         |                |
*                                |    ADDR NONE    |    ADDR NONE    |    ADDR NONE    |         v                v
*                                -------------------------------------------------------       -----            -----
*
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1a'.
*
*
* Argument(s) : if_nbr              Interface number to configure.
*
*               addr_host           Desired IPv4 address to add to this interface     (see Note #6).
*
*               addr_subnet_mask    Desired IPv4 address subnet mask     to configure (see Note #6).
*
*               addr_dflt_gateway   Desired IPv4 default gateway address to configure (see Note #6).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   IPv4 address successfully configured.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                               NET_IPv4_ERR_ADDR_CFG_STATE         Invalid IPv4 address configuration state --
*                                                                       NOT in dynamic address initialization
*                                                                       (see Note #8b1).
*                               NET_IPv4_ERR_ADDR_CFG_IN_USE        IPv4 address already configured on an interface.
*
*                                                                   -- RETURNED BY NetIPv4_CfgAddrValidate() : ---
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid IPv4 address, subnet mask, or address/
*                                                                       subnet mask combination.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 default gateway address.
*
*                                                                   --- RETURNED BY NetIF_IsEnCfgdHandler() : ----
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled network interface.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if valid IPv4 address, subnet mask, & default gateway configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Network Application.
*
*               This function is a network protocol suite initialization function & SHOULD be called only
*               by appropriate network application function(s) [see also Notes #3 & #4].
*
* Note(s)     : (3) NetIPv4_CfgAddrAddDynamic() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (4) Application calls to dynamic address configuration functions MUST be sequenced as follows :
*
*                   (a) NetIPv4_CfgAddrAddDynamicStart()  MUST precede NetIPv4_CfgAddrAddDynamic()
*
*                   See also 'NetIPv4_CfgAddrAddDynamicStart()  Note #3'
*                          & 'NetIPv4_CfgAddrAddDynamicStop()   Note #3'.
*
*               (5) NetIPv4_CfgAddrAddDynamic() blocked until network initialization completes.
*
*               (6) IPv4 addresses MUST be in host-order.
*
*               (7) (a) RFC #1122, Section 3.3.1.6 states that "a manual method of entering ... the following
*                       ... configuration data MUST be provided" :
*
*                       (1) "IPv4 address(es)"
*                       (2) "Address mask(s)"
*                       (3) "A list of default gateways"
*
*                   (b) (1) RFC #1122, Section 3.3.1.1 states that "the host IPv4 layer MUST operate correctly
*                           in a minimal network environment, and in particular, when there are no gateways".
*
*                           In other words, a host on an isolated network should be able to correctly operate
*                           & communicate with all other hosts on its local network without need of a gateway
*                           or configuration of a gateway.
*
*                       (2) However, a configured gateway MUST be on the same network as the host's IPv4 address
*                           -- i.e. the network portion of the configured IPv4 address & the configured gateway
*                           addresses MUST be identical.
*
*                       See also 'NetIPv4_CfgAddrValidate()  Note #2'.
*
*               (8) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) (1) If an interface's address configuration is NOT currently in dynamic address
*                           initialization, NO (dynamically-configured) address may be added.
*
*                       (2) Dynamic address initialization MUST be started before adding any dynamically-
*                           configured address (see Note #4a).
*
*                   (c) If any address(s) are configured on an interface when the application configures
*                       a dynamically-configured address, then ALL configured address(s) are removed.
*
*                       (1) ALL configured address(s) already  removed in NetIPv4_CfgAddrAddDynamicStart().
*                           These address(s) do NOT need to be re-removed but are shown for completeness.
*
*                           See also 'NetIPv4_CfgAddrAddDynamicStart()  Note #1d'.
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4_CfgAddrAdd()        Note #7',
*                            'NetIPv4_CfgAddrRemove()     Note #5',
*                          & 'NetIPv4_CfgAddrRemoveAll()  Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgAddrAddDynamic (NET_IF_NBR      if_nbr,
                                        NET_IPv4_ADDR   addr_host,
                                        NET_IPv4_ADDR   addr_subnet_mask,
                                        NET_IPv4_ADDR   addr_dflt_gateway,
                                        NET_ERR        *p_err)
{
    CPU_BOOLEAN       addr_cfgd;
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    NET_IPv4_ADDRS   *p_ip_addrs;
    CPU_BOOLEAN       result;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                        /* ----------- VALIDATE RTN ERR PTR ----------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                        /* ------------- ACQUIRE NET LOCK ------------- */
    Net_GlobalLockAcquire((void *)&NetIPv4_CfgAddrAddDynamic, p_err);   /* See Note #3b.                                */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #5).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

                                                                        /* ------------- VALIDATE IF NBR -------------- */
   (void)NetIF_IsEnCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }

                                                                        /* ----------- VALIDATE IPv4 ADDRS ------------ */
    NetIPv4_CfgAddrValidate(addr_host, addr_subnet_mask, addr_dflt_gateway, p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         goto exit_fail;
    }
#endif

                                                                        /* Validate host addr cfg'd.                    */
    addr_cfgd = NetIPv4_IsAddrHostCfgdHandler(addr_host);
    if (addr_cfgd != DEF_NO) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvAddrInUseCtr);
       *p_err =  NET_IPv4_ERR_ADDR_CFG_IN_USE;
        goto exit_fail;
    }



                                                                        /* ------- VALIDATE IPv4 ADDR CFG STATE ------- */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    switch (p_ip_if_cfg->AddrCfgState) {
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT:                      /* See Note #8b1.                               */
             break;


        case NET_IPv4_ADDR_CFG_STATE_NONE:
        case NET_IPv4_ADDR_CFG_STATE_STATIC:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
            *p_err =  NET_IPv4_ERR_ADDR_CFG_STATE;
             goto exit_fail;
    }



#if 0                                                                   /* Removed in dynamic start (see Note #8c1).    */
                                                                        /* ------ REMOVE ALL CFG'D IPv4 ADDR(S) ------- */
    NetIPv4_CfgAddrRemoveAllHandler(if_nbr, p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         goto exit_fail;
    }
#endif


                                                                        /* ---------- CFG DYNAMIC IPv4 ADDR ----------- */
    p_ip_addrs                         = &p_ip_if_cfg->AddrsTbl[0];
    p_ip_addrs->AddrHost               =  addr_host;
    p_ip_addrs->AddrHostSubnetMask     =              addr_subnet_mask;
    p_ip_addrs->AddrHostSubnetMaskHost =             ~addr_subnet_mask;
    p_ip_addrs->AddrHostSubnetNet      =  addr_host & addr_subnet_mask;
    p_ip_addrs->AddrDfltGateway        =  addr_dflt_gateway;

    p_ip_if_cfg->AddrsNbrCfgd          =  1u;                           /* Cfg single dynamic addr     (see Note #8a2). */
                                                                        /* Set to     dynamic addr cfg (see Note #8a2). */
    p_ip_if_cfg->AddrCfgState          =  NET_IPv4_ADDR_CFG_STATE_DYNAMIC;
    CPU_CRITICAL_ENTER();
    p_ip_if_cfg->AddrProtocolConflict  =  DEF_NO;                       /* Clr        dynamic addr conflict.            */
    CPU_CRITICAL_EXIT();



   *p_err =  NET_IPv4_ERR_NONE;
    result = DEF_OK;
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

exit_fail:
    result = DEF_FAIL;

exit_release:
                                                                       /* ------------- RELEASE NET LOCK ------------- */
    Net_GlobalLockRelease();

    return (result);
}


/*
*********************************************************************************************************
*                                   NetIPv4_CfgAddrAddDynamicStart()
*
* Description : (1) Start the dynamic address configuration for an interface :
*
*                   (a) Acquire  network lock
*                   (b) Validate interface
*                   (c) Validate IPv4 address configuration state(s)                         See Note #5b
*                   (d) Remove   ALL configured IPv4 host address(s) from interface
*                   (e) Set      IPv4 address configuration state to dynamic initialization
*                   (f) Release  network lock
*
*
* Argument(s) : if_nbr      Interface number to start dynamic address configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Dynamic address configuration successfully
*                                                                       started.
*                               NET_IPv4_ERR_ADDR_CFG_STATE         Invalid IPv4 address configuration state
*                                                                       (see Note #5b).
*                               NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS   Another interface already in dynamic address
*                                                                       initialization (see Note #5b2).
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   --- RETURNED BY NetIF_IsEnCfgdHandler() : ---
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled network interface.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if dynamic configuration successfully started.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Network Application.
*
*               This function is a network protocol suite initialization function & SHOULD be called only
*               by appropriate network application function(s) [see also Notes #2 & #3].
*
* Note(s)     : (2) NetIPv4_CfgAddrAddDynamicStart() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) Application calls to dynamic address configuration functions MUST be sequenced as follows :
*
*                   (a) NetIPv4_CfgAddrAddDynamicStart()  MUST precede NetIPv4_CfgAddrAddDynamic()
*
*                   See also 'NetIPv4_CfgAddrAddDynamic()      Note #4'
*                          & 'NetIPv4_CfgAddrAddDynamicStop()  Note #3'.
*
*               (4) NetIPv4_CfgAddrAddDynamicStart() blocked until network initialization completes.
*
*               (5) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) (1) If an interface's address configuration is already in dynamic address
*                           initialization, then dynamic address initialization does NOT need to be
*                           re-started.
*
*                       (2) Only a single interface is allowed to be in the dynamic address
*                           initialization state at any one time. #### NET-818
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4_CfgAddrAdd()         Note #7',
*                            'NetIPv4_CfgAddrAddDynamic()  Note #8',
*                            'NetIPv4_CfgAddrRemove()      Note #5',
*                          & 'NetIPv4_CfgAddrRemoveAll()   Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgAddrAddDynamicStart (NET_IF_NBR   if_nbr,
                                             NET_ERR     *p_err)
{
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    NET_IPv4_IF_CFG  *p_ip_if_cfg_srch;
    NET_IF_NBR        if_nbr_ix;
    CPU_BOOLEAN       addr_init;
    CPU_BOOLEAN       result;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                        /* ----------- VALIDATE RTN ERR PTR ----------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                        /* ------------- ACQUIRE NET LOCK ------------- */
                                                                        /* See Note #2b.                                */
    Net_GlobalLockAcquire((void *)&NetIPv4_CfgAddrAddDynamicStart, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #4).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

                                                                        /* ------------- VALIDATE IF NBR -------------- */
   (void)NetIF_IsEnCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }
#endif


                                                                        /* ----- VALIDATE IPv4 ADDR CFG STATE(S) ------ */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    switch (p_ip_if_cfg->AddrCfgState) {
        case NET_IPv4_ADDR_CFG_STATE_STATIC:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC:
             break;


        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT:                      /* See Note #5b1.                               */
            *p_err = NET_IPv4_ERR_NONE;
             goto exit_fail;


        case NET_IPv4_ADDR_CFG_STATE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
            *p_err =  NET_IPv4_ERR_ADDR_CFG_STATE;
             goto exit_fail;
    }


    if_nbr_ix       =  NET_IF_NBR_BASE_CFGD;
    p_ip_if_cfg_srch = &NetIPv4_IF_CfgTbl[if_nbr_ix];
    addr_init       =  DEF_NO;

    while ((if_nbr_ix <  NET_IF_NBR_IF_TOT) &&                          /* Srch ALL cfg'd IF's  ...                     */
           (addr_init == DEF_NO)) {

        if ((if_nbr_ix                     != if_nbr) &&                /* ... for any other IF ...                     */
            (p_ip_if_cfg_srch->AddrCfgState == NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT)) {
            addr_init  = DEF_YES;                                       /* ... in dynamic addr init.                    */

        } else {
            if_nbr_ix++;
            p_ip_if_cfg_srch++;
        }
    }

    if (addr_init != DEF_NO) {                                          /* If any cfg'd IF in dynamic addr init, ...    */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
       *p_err =  NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS;                     /* ... rtn err (see Note #5b2).                 */
        goto exit_fail;
    }



                                                                        /* ------ REMOVE ALL CFG'D IPv4 ADDR(S) ------- */
    NetIPv4_CfgAddrRemoveAllHandler(if_nbr, p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         goto exit_fail;
    }


                                                                        /* --------- START DYNAMIC ADDR INIT ---------- */
    p_ip_if_cfg->AddrCfgState = NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT;   /* Set to dynamic addr init.                    */





   *p_err =  NET_IPv4_ERR_NONE;
    result = DEF_OK;
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

exit_fail:
    result = DEF_FAIL;

exit_release:
                                                                        /* ------------- RELEASE NET LOCK ------------- */
    Net_GlobalLockRelease();

    return (result);
}


/*
*********************************************************************************************************
*                                    NetIPv4_CfgAddrAddDynamicStop()
*
* Description : (1) Stop the dynamic address configuration for an interface (see Note #3a) :
*
*                   (a) Acquire  network lock
*                   (b) Validate interface
*                   (c) Validate IPv4 address configuration state                       See Note #5b
*                   (d) Reset    IPv4 address configuration state to static             See Note #5c
*                   (e) Release  network lock
*
*
* Argument(s) : if_nbr      Interface number to stop dynamic address configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Dynamic address configuration successfully
*                                                                   stopped.
*                               NET_IPv4_ERR_ADDR_CFG_STATE     Invalid IPv4 address configuration state --
*                                                                   NOT in dynamic address initialization
*                                                                   (see Note #5b).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : ---
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if dynamic configuration successfully stopped.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Network Application.
*
*               This function is a network protocol suite initialization function & SHOULD be called only
*               by appropriate network application function(s) [see also Notes #2 & #3].
*
* Note(s)     : (2) NetIPv4_CfgAddrAddDynamicStop() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) Application calls to dynamic address configuration functions MUST be sequenced as follows :
*
*                   (a) NetIPv4_CfgAddrAddDynamicStop()  MUST follow NetIPv4_CfgAddrAddDynamicStart() --
*                                                      if & ONLY if dynamic address initialization fails
*
*                   See also 'NetIPv4_CfgAddrAddDynamicStart()  Note #3'
*                          & 'NetIPv4_CfgAddrAddDynamic()       Note #4'.
*
*               (4) NetIPv4_CfgAddrAddDynamicStop() blocked until network initialization completes.
*
*               (5) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) If an interface's address configuration is NOT currently in dynamic address
*                       initialization, then dynamic address initialization may NOT be stopped
*                       (see Note #3a).
*
*                   (c) When NO address(s) are configured on an interface after dynamic address
*                       initialization stops, then the interface's address configuration is
*                       defaulted back to statically-configured.
*
*                       See also 'NetIPv4_Init()              Note #2b',
*                                'NetIPv4_CfgAddrRemove()     Note #5c',
*                              & 'NetIPv4_CfgAddrRemoveAll()  Note #4c'.
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4_CfgAddrAdd()         Note #7',
*                            'NetIPv4_CfgAddrAddDynamic()  Note #8',
*                            'NetIPv4_CfgAddrRemove()      Note #5',
*                          & 'NetIPv4_CfgAddrRemoveAll()   Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgAddrAddDynamicStop (NET_IF_NBR   if_nbr,
                                            NET_ERR     *p_err)
{
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    CPU_BOOLEAN       result;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                        /* ----------- VALIDATE RTN ERR PTR ----------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                        /* ------------- ACQUIRE NET LOCK ------------- */
                                                                        /* See Note #2b.                                */
    Net_GlobalLockAcquire((void *)&NetIPv4_CfgAddrAddDynamicStop, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #4).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

                                                                        /* ------------- VALIDATE IF NBR -------------- */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }
#endif


                                                                        /* ------- VALIDATE IPv4 ADDR CFG STATE ------- */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    switch (p_ip_if_cfg->AddrCfgState) {
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT:                      /* See Note #5b.                                */
             break;


        case NET_IPv4_ADDR_CFG_STATE_NONE:
        case NET_IPv4_ADDR_CFG_STATE_STATIC:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
            *p_err =  NET_IPv4_ERR_ADDR_CFG_STATE;
             goto exit_fail;
    }


                                                                        /* ---------- STOP DYNAMIC ADDR INIT ---------- */
    p_ip_if_cfg->AddrCfgState = NET_IPv4_ADDR_CFG_STATE_STATIC;         /* Dflt to static addr cfg (see Note #5c).      */





   *p_err =  NET_IPv4_ERR_NONE;
    result = DEF_OK;
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

exit_fail:
    result = DEF_FAIL;

exit_release:
                                                                        /* ------------- RELEASE NET LOCK ------------- */
    Net_GlobalLockRelease();

    return (result);
}


/*
*********************************************************************************************************
*                                        NetIPv4_CfgAddrRemove()
*
* Description : (1) Remove a configured IPv4 host address, subnet mask, & default gateway from an interface :
*
*                   (a) Acquire  network lock
*                   (b) Validate address to remove :
*                       (1) Validate interface
*                       (2) Validate IPv4 address
*                       (3) Validate IPv4 address configuration state                         See Note #5b
*
*                   (c) Remove configured IPv4 address from interface's IPv4 address table :
*                       (1) Search table for address to remove
*                       (2) Close all connections for address
*                       (3) If NO configured IPv4 address(s) remain in table,               See Note #5c
*                              reset address configuration state to static
*
*                   (d) Release  network lock
*
*
* Argument(s) : if_nbr      Interface number to remove address configuration.
*
*               addr_host   IPv4 address     to remove (see Note #4).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Configured IPv4 address successfully removed.
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid    IPv4 address.
*                               NET_IPv4_ERR_ADDR_CFG_STATE         Invalid    IPv4 address configuration state
*                                                                   (see Note #5b).
*                               NET_IPv4_ERR_ADDR_TBL_EMPTY         Interface's configured IPv4 address table empty.
*                               NET_IPv4_ERR_ADDR_NOT_FOUND         IPv4 address NOT found in interface's configured
*                                                                   IPv4 address table for specified interface.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY NetIF_IsValidCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if IPv4 address configuration removed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_CfgAddrRemove() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (3) NetIPv4_CfgAddrRemove() blocked until network initialization completes.
*
*               (4) IPv4 address MUST be in host-order.
*
*               (5) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) If an interface's IPv4 host address(s) are NOT currently statically- or dynamically-
*                       configured, then NO address(s) may NOT be removed.
*
*                   (c) If NO address(s) are configured on an interface after an address is removed, then
*                       the interface's address configuration is defaulted back to statically-configured.
*
*                       See also 'NetIPv4_Init()              Note #2b'
*                              & 'NetIPv4_CfgAddrRemoveAll()  Note #4c'.
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4_CfgAddrAdd()         Note #7',
*                            'NetIPv4_CfgAddrAddDynamic()  Note #8',
*                          & 'NetIPv4_CfgAddrRemoveAll()   Note #4'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgAddrRemove (NET_IF_NBR      if_nbr,
                                    NET_IPv4_ADDR   addr_host,
                                    NET_ERR        *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_BOOLEAN        addr_valid;
#endif
    NET_IPv4_ADDR      addr_cfgd;
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IPv4_ADDRS    *p_ip_addrs_next;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_BOOLEAN        addr_found;
    CPU_BOOLEAN        result;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                        /* ----------- VALIDATE RTN ERR PTR ----------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif
                                                                        /* ------------- ACQUIRE NET LOCK ------------- */
    Net_GlobalLockAcquire((void *)&NetIPv4_CfgAddrRemove, p_err);       /* See Note #2b.                                */
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

                                                                        /* ------------ VALIDATE IPv4 ADDR ------------ */
    addr_valid = NetIPv4_IsValidAddrHost(addr_host);
    if (addr_valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvAddrHostCtr);
       *p_err =  NET_IPv4_ERR_INVALID_ADDR_HOST;
        goto exit_fail;
    }
#endif


                                                                        /* ------- VALIDATE IPv4 ADDR CFG STATE ------- */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    switch (p_ip_if_cfg->AddrCfgState) {
        case NET_IPv4_ADDR_CFG_STATE_STATIC:                            /* See Note #5b.                                */
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC:
             break;


        case NET_IPv4_ADDR_CFG_STATE_NONE:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
            *p_err =  NET_IPv4_ERR_ADDR_CFG_STATE;
             goto exit_fail;
    }


                                                                        /* ------ VALIDATE NBR CFG'D IPv4 ADDRS ------- */
    if (p_ip_if_cfg->AddrsNbrCfgd < 1) {                                /* If nbr cfg'd addrs < 1, ...                  */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrTblEmptyCtr);
       *p_err =  NET_IPv4_ERR_ADDR_TBL_EMPTY;                           /* ... rtn tbl empty.                           */
        goto exit_fail;
    }


                                                                        /* --------- SRCH IP ADDR IN ADDR TBL --------- */
    addr_ix    =  0u;
    addr_found =  DEF_NO;
    p_ip_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];

    while ((addr_ix    <  p_ip_if_cfg->AddrsNbrCfgd) &&                 /* Srch ALL addrs ...                           */
           (addr_found == DEF_NO)) {                                    /* ... until addr found.                        */

        if (p_ip_addrs->AddrHost != addr_host) {                        /* If addr NOT found, ...                       */
            p_ip_addrs++;                                               /* ... adv to next entry.                       */
            addr_ix++;

        } else {
            addr_found = DEF_YES;
        }
    }

    if (addr_found != DEF_YES) {                                        /* If addr NOT found, ...                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrNotFoundCtr);
       *p_err =  NET_IPv4_ERR_ADDR_NOT_FOUND;                           /* ... rtn err.                                 */
        goto exit_fail;
    }



                                                                        /* -------- CLOSE ALL IPv4 ADDR CONNS --------- */
                                                                        /* Close all cfg'd addr's conns.                */
    addr_cfgd = NET_UTIL_HOST_TO_NET_32(p_ip_addrs->AddrHost);
    NetConn_CloseAllConnsByAddrHandler((CPU_INT08U      *)      &addr_cfgd,
                                       (NET_CONN_ADDR_LEN)sizeof(addr_cfgd));


                                                                        /* ------ REMOVE IPv4 ADDR FROM ADDR TBL ------ */
    p_ip_addrs_next = p_ip_addrs;
    p_ip_addrs_next++;
    addr_ix++;

    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {                       /* Shift ALL rem'ing tbl addr(s) ...            */
                                                                        /* ... following removed addr to prev tbl entry.*/
        p_ip_addrs->AddrHost               = p_ip_addrs_next->AddrHost;
        p_ip_addrs->AddrHostSubnetMask     = p_ip_addrs_next->AddrHostSubnetMask;
        p_ip_addrs->AddrHostSubnetMaskHost = p_ip_addrs_next->AddrHostSubnetMaskHost;
        p_ip_addrs->AddrHostSubnetNet      = p_ip_addrs_next->AddrHostSubnetNet;
        p_ip_addrs->AddrDfltGateway        = p_ip_addrs_next->AddrDfltGateway;

        p_ip_addrs++;
        p_ip_addrs_next++;
        addr_ix++;
    }
                                                                        /* Clr last addr tbl entry.                     */
    p_ip_addrs->AddrHost               = NET_IPv4_ADDR_NONE;
    p_ip_addrs->AddrHostSubnetMask     = NET_IPv4_ADDR_NONE;
    p_ip_addrs->AddrHostSubnetMaskHost = NET_IPv4_ADDR_NONE;
    p_ip_addrs->AddrHostSubnetNet      = NET_IPv4_ADDR_NONE;
    p_ip_addrs->AddrDfltGateway        = NET_IPv4_ADDR_NONE;

    p_ip_if_cfg->AddrsNbrCfgd--;
    if (p_ip_if_cfg->AddrsNbrCfgd < 1) {                                /* If NO addr(s) cfg'd, ...                     */
                                                                        /* ... dflt to static addr cfg (see Note #5c).  */
        p_ip_if_cfg->AddrCfgState         = NET_IPv4_ADDR_CFG_STATE_STATIC;
        CPU_CRITICAL_ENTER();
        p_ip_if_cfg->AddrProtocolConflict = DEF_NO;                     /* Clr   addr conflict.                         */
        CPU_CRITICAL_EXIT();
    }




   *p_err =  NET_IPv4_ERR_NONE;
   result = DEF_OK;
   goto exit_release;


exit_lock_fault:
   return (DEF_FAIL);

exit_fail:
    result = DEF_FAIL;
                                                                        /* ------------- RELEASE NET LOCK ------------- */
exit_release:
    Net_GlobalLockRelease();


    return (result);
}


/*
*********************************************************************************************************
*                                      NetIPv4_CfgAddrRemoveAll()
*
* Description : (1) Remove all configured IPv4 host address(s) from an interface :
*
*                   (a) Acquire  network lock
*                   (b) Validate interface
*                   (c) Remove   ALL configured IPv4 host address(s) from interface
*                   (d) Release  network lock
*
*
* Argument(s) : if_nbr      Interface number to remove address configuration.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY NetIPv4_CfgAddrRemoveAllHandler() : ---
*                               NET_IPv4_ERR_NONE               ALL configured IPv4 address(s) successfully removed.
*                               NET_IPv4_ERR_ADDR_CFG_STATE     Invalid IPv4 address configuration state (see Note #4b).
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
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_CfgAddrRemoveAll() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv4_CfgAddrRemoveAllHandler()  Note #2'.
*
*               (3) NetIPv4_CfgAddrRemoveAll() blocked until network initialization completes.
*
*               (4) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) If an interface's IPv4 host address(s) are NOT currently statically- or dynamically-
*                       configured, then NO address(s) may NOT be removed.
*
*                   (c) When NO address(s) are configured on an interface after ALL address(s) are removed,
*                       the interface's address configuration is defaulted back to statically-configured.
*
*                       See also 'NetIPv4_Init()           Note #2b'
*                              & 'NetIPv4_CfgAddrRemove()  Note #5c'.
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4._CfgAddrAdd()         Note #7',
*                            'NetIPv4._CfgAddrAddDynamic()  Note #8',
*                          & 'NetIPv4._CfgAddrRemove()      Note #5'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgAddrRemoveAll (NET_IF_NBR   if_nbr,
                                       NET_ERR     *p_err)
{
    CPU_BOOLEAN  addr_remove;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                        /* ----------- VALIDATE RTN ERR PTR ----------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif

    Net_GlobalLockAcquire((void *)&NetIPv4_CfgAddrRemoveAll, p_err);    /* Acquire net lock (see Note #1b).             */
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                                      /* If init NOT complete, exit (see Note #3).    */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }

   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);                       /* Validate IF nbr.                             */
    if (*p_err != NET_IF_ERR_NONE) {
         goto exit_fail;
    }
#endif
                                                                        /* Remove all IF's cfg'd host addr(s).          */
    addr_remove = NetIPv4_CfgAddrRemoveAllHandler(if_nbr, p_err);
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
*                                     NetIPv4_CfgFragReasmTimeout()
*
* Description : (1) Configure IPv4 fragment reassembly timeout.
*
*                   (a) IPv4 fragment reassembly timeout is the maximum time allowed between received IPv4
*                       fragments from the same IPv4 datagram.
*
* Argument(s) : timeout_sec     Desired value for IPv4 fragment reassembly timeout (in seconds).
*
* Return(s)   : DEF_OK,   IPv4 fragment reassembly timeout configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Net_InitDflt(),
*               Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) Configured timeout does NOT reschedule any current IP fragment reassembly timeout in
*                   progress but becomes effective the next time IP fragments reassemble with timeout.
*
*               (3) Configured timeout converted to 'NET_TMR_TICK' ticks to avoid run-time conversion.
*
*               (3) 'NetIPv4_FragReasmTimeout' variables MUST ALWAYS be accessed exclusively in critical
*                    sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_CfgFragReasmTimeout (CPU_INT08U  timeout_sec)
{
    NET_TMR_TICK  timeout_tick;
    CPU_SR_ALLOC();


    if (timeout_sec < NET_IPv4_FRAG_REASM_TIMEOUT_MIN_SEC) {
        return (DEF_FAIL);
    }
    if (timeout_sec > NET_IPv4_FRAG_REASM_TIMEOUT_MAX_SEC) {
        return (DEF_FAIL);
    }

    timeout_tick                  = (NET_TMR_TICK)timeout_sec * NET_TMR_TIME_TICK_PER_SEC;
    CPU_CRITICAL_ENTER();
    NetIPv4_FragReasmTimeout_sec  =  timeout_sec;
    NetIPv4_FragReasmTimeout_tick =  timeout_tick;
    CPU_CRITICAL_EXIT();

   (void)&NetIPv4_FragReasmTimeout_sec;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         NetIPv4_GetAddrHost()
*
* Description : Get an interface's IPv4 host address(s) [see Note #3].
*
* Argument(s) : if_nbr          Interface number to get IPv4 host address(s).
*
*               p_addr_tbl       Pointer to IPv4 address table that will receive the IPv4 host address(s)
*                                   in host-order for this interface.
*
*               p_addr_tbl_qty   Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address table, in number of IPv4 addresses,
*                                           pointed to by 'p_addr_tbl'.
*                                   (b) (1) Return the actual number of IPv4 addresses, if NO error(s);
*                                       (2) Return 0,                                 otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   - RETURNED BY NetIPv4_GetAddrHostHandler() : -
*                               NET_IPv4_ERR_NONE                   IP host address(s) successfully returned.
*                               NET_ERR_FAULT_NULL_PTR               Argument 'p_addr_tbl'/'p_addr_tbl_qty' passed
*                                                                       a NULL pointer.
*                               NET_IPv4_ERR_ADDR_NONE_AVAIL        NO IP host address(s) configured on specified
*                                                                       interface.
*                               NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS   Interface in dynamic address initialization.
*                               NET_IPv4_ERR_ADDR_TBL_SIZE          Invalid IP address table size.
*                               NET_IFv4_ERR_INVALID_IF             Invalid network interface number.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if interface's IPv4 host address(s) successfully returned.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_GetAddrHost() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv4_GetAddrHostHandler()  Note #1'.
*
*               (2) NetIPv4_GetAddrHost() blocked until network initialization completes.
*
*               (3) IPv4 address(s) returned in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_GetAddrHost (NET_IF_NBR         if_nbr,
                                  NET_IPv4_ADDR     *p_addr_tbl,
                                  NET_IP_ADDRS_QTY  *p_addr_tbl_qty,
                                  NET_ERR           *p_err)
{
    CPU_BOOLEAN  addr_avail;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif

    Net_GlobalLockAcquire((void *)&NetIPv4_GetAddrHost, p_err); /* Acquire net lock (see Note #1b).                     */
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
    addr_avail = NetIPv4_GetAddrHostHandler(if_nbr, p_addr_tbl, p_addr_tbl_qty, p_err);
    goto exit_release;


exit_lock_fault:
   *p_addr_tbl_qty = 0u;
    return (DEF_FAIL);


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_avail    = DEF_FAIL;
   *p_addr_tbl_qty = 0u;
#endif


exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (addr_avail);
}


/*
*********************************************************************************************************
*                                     NetIPv4_GetAddrHostHandler()
*
* Description : Get an interface's IPv4 host address(s) [see Note #2].
*
* Argument(s) : if_nbr          Interface number to get IPv4 host address(s).
*
*               p_addr_tbl       Pointer to IPv4 address table that will receive the IPv4 host address(s)
*                                   in host-order for this interface.
*
*               p_addr_tbl_qty   Pointer to a variable to ... :
*
*                                   (a) Pass the size of the address table, in number of IPv4 addresses,
*                                           pointed to by 'p_addr_tbl'.
*                                   (b) (1) Return the actual number of IPv4 addresses, if NO error(s);
*                                       (2) Return 0,                                   otherwise.
*
*                               See also Note #3.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   IP host address(s) successfully returned.
*                               NET_ERR_FAULT_NULL_PTR               Argument 'p_addr_tbl'/'p_addr_tbl_qty' passed
*                                                                       a NULL pointer.
*                               NET_IPv4_ERR_ADDR_NONE_AVAIL        NO IP host address(s) configured on specified
*                                                                       interface.
*                               NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS   Interface in dynamic address initialization.
*                               NET_IPv4_ERR_ADDR_TBL_SIZE          Invalid IP address table size.
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : DEF_OK,   if interface's IPv4 host address(s) successfully returned.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv4_GetAddrHost(),
*               NetIPv4_GetHostAddrProtocol(),
*               NetIPv4_ChkAddrProtocolConflict(),
*               NetSock_BindHandler(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_GetAddrHostHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv4_GetAddrHost()  Note #1'.
*
*               (2) IPv4 address(s) returned in host-order.
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
*                           IP address(s) MUST be greater than or equal to NET_IPv4_CFG_IF_MAX_NBR_ADDR.
*
*                   (b) While its output value MUST be initially configured to return a default value
*                       PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_GetAddrHostHandler (NET_IF_NBR         if_nbr,
                                         NET_IPv4_ADDR     *p_addr_tbl,
                                         NET_IP_ADDRS_QTY  *p_addr_tbl_qty,
                                         NET_ERR           *p_err)
{
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    NET_IP_ADDRS_QTY   addr_tbl_qty;
#endif
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IPv4_ADDR     *p_addr;
    NET_IP_ADDRS_QTY   addr_ix;


                                                                /* ---------------- VALIDATE ADDR TBL ----------------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_addr_tbl_qty == (NET_IP_ADDRS_QTY *)0) {              /* See Note #3a1.                                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (DEF_FAIL);
    }

     addr_tbl_qty = *p_addr_tbl_qty;
#endif
   *p_addr_tbl_qty =  0u;                                       /* Cfg rtn addr tbl qty for err  (see Note #3b).        */

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (addr_tbl_qty < NET_IPv4_CFG_IF_MAX_NBR_ADDR) {          /* Validate initial addr tbl qty (see Note #3a).        */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrTblSizeCtr);
       *p_err =  NET_IPv4_ERR_ADDR_TBL_SIZE;
        return (DEF_FAIL);
    }

    if (p_addr_tbl == (NET_IPv4_ADDR *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
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


                                                                /* ------------------ GET IPv4 ADDRS ------------------ */
    p_addr = p_addr_tbl;

    if (if_nbr == NET_IF_NBR_LOOPBACK) {                        /* For loopback IF,                  ...                */
#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
       *p_addr         = NET_IPv4_ADDR_LOCAL_HOST_ADDR;         /* ... get dflt IPv4 localhost addr; ...                */
       *p_addr_tbl_qty = 1u;
       *p_err          = NET_IPv4_ERR_NONE;
        return (DEF_OK);
#else
       *p_err          = NET_IF_ERR_INVALID_IF;
        return (DEF_FAIL);
#endif
    }


    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
                                                                /* For IF in dynamic init state,   ...                  */
    if (p_ip_if_cfg->AddrCfgState == NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT) {
       *p_err = NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS;              /* ... rtn NO addr;                ...                  */
        return (DEF_FAIL);
    }

    if (p_ip_if_cfg->AddrsNbrCfgd < 1) {
       *p_err =  NET_IPv4_ERR_ADDR_NONE_AVAIL;
        return (DEF_FAIL);
    }

    addr_ix   =  0u;
    p_ip_addrs = &p_ip_if_cfg->AddrsTbl[addr_ix];
    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {               /* ... else get all cfg'd host addr(s).                 */
       *p_addr = p_ip_addrs->AddrHost;
        p_ip_addrs++;
        p_addr++;
        addr_ix++;
    }


   *p_addr_tbl_qty = p_ip_if_cfg->AddrsNbrCfgd;                 /* Rtn nbr of cfg'd addrs.                              */
   *p_err          = NET_IPv4_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         NetIPv4_GetAddrSrc()
*
* Description : Get corresponding configured IPv4 host address for a remote IPv4 address (to use as source
*               address).
*
* Argument(s) : addr_remote     Remote address to get configured IPv4 host address (see Note #3).
*
* Return(s)   : Configured IPv4 host address (see Note #3), if available.
*
*               NET_IPv4_ADDR_NONE,                         otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_GetAddrHostCfgd() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv4_GetAddrHostHandler()  Note #1'.
*
*               (2) NetIPv4_GetAddrHostCfgd() blocked until network initialization completes.
*
*               (3) IPv4 addresses MUST be in host-order.
*********************************************************************************************************
*/

NET_IPv4_ADDR  NetIPv4_GetAddrSrc (NET_IPv4_ADDR  addr_remote)
{
    NET_IPv4_ADDR  addr_host;
    NET_ERR        err;


                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetIPv4_GetAddrSrc, &err);
    if (err != NET_ERR_NONE) {
        goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
        goto exit_fail;
    }
#endif


    addr_host = NetIPv4_GetAddrSrcHandler(addr_remote);    /* Get cfg'd host addr.                                 */
    goto exit_release;


exit_lock_fault:
    return (NET_IPv4_ADDR_NONE);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_host = NET_IPv4_ADDR_NONE;
#endif

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (addr_host);
}


/*
*********************************************************************************************************
*                                      NetIPv4_GetAddrSrcHandler()
*
* Description : (1) Get corresponding configured IPv4 host address for a remote IPv4 address :
*
*                   (a) Search Remote IPv4 Address cache for corresponding  configured IPv4 host address
*                           that recently communicated with the remote IPv4 address
*
*                       (1) NOT yet implemented if IPv4 routing table to be implemented.
*
*                   (b) Search configured IP host addresses structure for configured IPv4 host address with
*                           same local network as the remote IPv4 address
*
*
* Argument(s) : addr_remote     Remote address to get configured IPv4 host address (see Note #3).
*
* Return(s)   : Configured IPv4 host address (see Note #3), if available.
*
*               NET_IPv4_ADDR_NONE,                         otherwise.
*
* Caller(s)   : NetSock_ConnHandlerAddr(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_GetAddrSrcHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*               (3) IPv4 addresses MUST be in host-order.
*********************************************************************************************************
*/

NET_IPv4_ADDR  NetIPv4_GetAddrSrcHandler (NET_IPv4_ADDR  addr_remote)
{
    NET_IPv4_ADDR      addr_host = NET_IPv4_ADDR_NONE;
    NET_IF_NBR         if_nbr    = NET_IF_NBR_BASE_CFGD;
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_BOOLEAN        is_valid;


                                                                /* ---------------- VALIDATE IPv4 ADDR ---------------- */
    if (addr_remote == NET_IPv4_ADDR_NONE) {
        return (NET_IPv4_ADDR_NONE);
    }
                                                                /* -------------- SRCH REMOTE ADDR CACHE -------------- */
                                                                /* See Note #1a1.                                       */

                                                                /* ... search address on all configured Interfaces.     */
    while ((if_nbr    <  NET_IF_NBR_IF_TOT) &&
           (addr_host == NET_IPv4_ADDR_NONE )) {
        p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
        addr_ix     = 0u;
        p_ip_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];

        while ((addr_ix   <  p_ip_if_cfg->AddrsNbrCfgd) &&      /* ...  all cfg'd addrs  ...                            */
               (addr_host == NET_IPv4_ADDR_NONE)) {
            if ((NET_UTIL_NET_TO_HOST_32(addr_remote) & p_ip_addrs->AddrHostSubnetMask) ==
                                                        p_ip_addrs->AddrHostSubnetNet ) {
                is_valid = NetIPv4_IsValidAddrHost (p_ip_addrs->AddrHost);
                if (is_valid == DEF_YES) {
                    addr_host = p_ip_addrs->AddrHost;
                    break;
                }
            }

            if (addr_host == NET_IPv4_ADDR_NONE) {              /* If addr NOT found,        ...                        */
                p_ip_addrs++;                                   /* ... adv to next IPv4 addr ...                        */
                addr_ix++;
            }
        }

        if (addr_host == NET_IPv4_ADDR_NONE) {
            if_nbr++;
        }
    }

                                                                /* If no addr found on subnet, return default interface */
    if (addr_host == NET_IPv4_ADDR_NONE) {
        addr_ix     =  0u;
        if_nbr      =  NetIF_GetDflt();
        if (if_nbr != NET_IF_NBR_NONE) {
            p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
            p_ip_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];

            while ((addr_ix   <  p_ip_if_cfg->AddrsNbrCfgd) &&      /* ...  all cfg'd addrs  ...                            */
                   (addr_host == NET_IPv4_ADDR_NONE)) {

                is_valid = NetIPv4_IsValidAddrHost (p_ip_addrs->AddrHost);
                if ((p_ip_addrs->AddrDfltGateway != NET_IPv4_ADDR_NONE) &&
                    (p_ip_addrs->AddrHost        != NET_IPv4_ADDR_NONE) &&
                    (is_valid                    == DEF_YES)          ) {
                     addr_host = p_ip_addrs->AddrHost;
                     break;
                }

                if (addr_host == NET_IPv4_ADDR_NONE) {              /* If addr NOT found,        ...                        */
                    p_ip_addrs++;                                   /* ... adv to next IPv4 addr ...                        */
                    addr_ix++;
                }
            }
        }
    }

    return (addr_host);
}


/*
*********************************************************************************************************
*                                      NetIPv4_GetAddrSubnetMask()
*
* Description : Get the IPv4 address subnet mask for a configured IPv4 host address.
*
* Argument(s) : addr        Configured IPv4 host address to get the subnet mask (see Note #3).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Configured IPv4 address's subnet mask
*                                                                       successfully returned.
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid or not configured IPv4 host
*                                                                       address.
*
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   -- RETURNED BY Net_GlobalLockAcquire() : --
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : Configured IPv4 host address's subnet mask in host-order, if NO error(s).
*
*               NET_IPv4_ADDR_NONE,                                       otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_GetAddrSubnetMask() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetIPv4_GetAddrSubnetMask() blocked until network initialization completes.
*
*               (3) IPv4 address returned in host-order.
*********************************************************************************************************
*/

NET_IPv4_ADDR  NetIPv4_GetAddrSubnetMask (NET_IPv4_ADDR   addr,
                                          NET_ERR        *p_err)
{
    NET_IPv4_ADDRS  *p_ip_addrs;
    NET_IPv4_ADDR    addr_subnet;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_IPv4_ADDR)0);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetIPv4_GetAddrSubnetMask, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* ----------- GET IPv4 ADDR'S SUBNET MASK ------------ */
    p_ip_addrs = NetIPv4_GetAddrsHostCfgd((NET_IPv4_ADDR )addr,
                                         (NET_IF_NBR   *)0);
    if (p_ip_addrs == (NET_IPv4_ADDRS *)0) {                    /* If addr NOT found, ...                               */
       *p_err =  NET_IPv4_ERR_INVALID_ADDR_HOST;                /* ... rtn err.                                         */
        goto exit_fail;
    }

    addr_subnet = p_ip_addrs->AddrHostSubnetMask;               /* Get IPv4 addr subnet mask.                           */


   *p_err =  NET_IPv4_ERR_NONE;
    goto exit_release;


exit_lock_fault:
    return (NET_IPv4_ADDR_NONE);

exit_fail:
    addr_subnet = NET_IPv4_ADDR_NONE;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (addr_subnet);
}


/*
*********************************************************************************************************
*                                     NetIPv4_GetAddrDfltGateway()
*
* Description : Get the default gateway IPv4 address for a configured IPv4 host address.
*
* Argument(s) : addr        Configured IPv4 host address to get the default gateway (see Note #3).
*
*               p_err      Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Configured IPv4 address's default gateway
*                                                                       successfully returned.
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid or not yet configured IPv4 host
*                                                                       address.
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               -- RETURNED BY Net_GlobalLockAcquire() : --
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Configured IPv4 host address's default gateway in host-order, if NO error(s).
*
*               NET_IPv4_ADDR_NONE,                                           otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_GetAddrDfltGateway() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetIPv4_GetAddrDfltGateway() blocked until network initialization completes.
*
*               (3) IPv4 address returned in host-order.
*********************************************************************************************************
*/

NET_IPv4_ADDR  NetIPv4_GetAddrDfltGateway (NET_IPv4_ADDR   addr,
                                           NET_ERR        *p_err)
{
    NET_IPv4_ADDRS  *p_ip_addrs;
    NET_IPv4_ADDR    addr_dflt_gateway;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_IPv4_ADDR)0);
    }
#endif
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetIPv4_GetAddrDfltGateway, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif

                                                                /* ----------- GET IPv4 ADDR'S DFLT GATEWAY ----------- */
    p_ip_addrs = NetIPv4_GetAddrsHostCfgd((NET_IPv4_ADDR )addr,
                                         (NET_IF_NBR *)0);
    if (p_ip_addrs == (NET_IPv4_ADDRS *)0) {                    /* If addr NOT found, ...                               */
       *p_err =  NET_IPv4_ERR_INVALID_ADDR_HOST;                /* ... rtn err.                                         */
        goto exit_fail;
    }

    addr_dflt_gateway = p_ip_addrs->AddrDfltGateway;            /* Get IPv4 addr dflt gateway.                          */


   *p_err =  NET_IPv4_ERR_NONE;
    goto exit_release;


exit_lock_fault:
    return (NET_IPv4_ADDR_NONE);

exit_fail:
    addr_dflt_gateway = NET_IPv4_ADDR_NONE;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();

    return (addr_dflt_gateway);
}


/*
*********************************************************************************************************
*                                      NetIPv4_GetAddrHostIF_Nbr()
*
* Description : (1) Get the interface number for an IPv4 host address :
*
*                   (a) A configured IPv4 host address (on an enabled interface)
*                   (b) A 'Localhost'                address
*                   (c) A 'This host' initialization address                        See Note #4
*
*
* Argument(s) : addr        Configured IPv4 host address to get the interface number (see Note #3).
*
* Return(s)   : Interface number of a configured IPv4 host address,    if available.
*
*               Interface number of              IPv4 host address
*                   in dynamic address initialization (see Note #4), if available.
*
*               NET_IF_NBR_LOCAL_HOST,                               for a localhost address.
*
*               NET_IF_NBR_NONE,                                     otherwise.
*
* Caller(s)   : NetICMPv4_TxMsgReqHandler(),
*               NetIPv4_GetAddrProtocolIF_Nbr(),
*               NetIPv4_IsAddrHostHandler(),
*               NetSock_IsValidAddrLocal(),
*               NetUDP_TxAppDataHandlerIPv4().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_GetAddrHostIF_Nbr() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*               (3) IPv4 address MUST be in host-order.
*
*               (4) For the 'This Host' initialization address, the interface in the dynamic address
*                   initialization state (if any) is returned (see 'NetIPv4_CfgAddrAddDynamicStart()
*                   Note #4b2').  This allows higher layers to select an interface in dynamic address
*                   initialization & transmit the 'This Host' initialization address in order to
*                   negotiate & configure a dynamic address for the interface.
*********************************************************************************************************
*/

NET_IF_NBR  NetIPv4_GetAddrHostIF_Nbr (NET_IPv4_ADDR  addr)
{
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    NET_IF_NBR        if_nbr;
    CPU_BOOLEAN       addr_this_host;
    CPU_BOOLEAN       addr_local_host;
    CPU_BOOLEAN       addr_init;


    addr_this_host = NetIPv4_IsAddrThisHost(addr);              /* Chk 'This Host' addr  (see Note #1c).                */
    if (addr_this_host == DEF_YES) {
        if_nbr     =  NET_IF_NBR_BASE_CFGD;
        p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
        addr_init  =  DEF_NO;

        while ((if_nbr    <  NET_IF_NBR_IF_TOT) &&              /* Srch ALL cfg'd IF's ...                              */
               (addr_init == DEF_NO)) {

            if (p_ip_if_cfg->AddrCfgState == NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT) {
                addr_init  = DEF_YES;                           /* ... for dynamic addr init (see Note #4).             */

            } else {
                if_nbr++;
                p_ip_if_cfg++;
            }
        }

        if (addr_init != DEF_YES) {                             /* If NO dynamic addr init found, ...                   */
            if_nbr = NET_IF_NBR_NONE;                           /* ... rtn NO IF.                                       */
        }

    } else {
                                                                /* Chk localhost  addrs (see Note #1b).                 */
        addr_local_host = NetIPv4_IsAddrLocalHost(addr);
        if (addr_local_host == DEF_YES) {
            if_nbr = NET_IF_NBR_LOCAL_HOST;
                                                                /* Chk cfg'd host addrs (see Note #1a).                 */
        } else {
            if_nbr = NetIPv4_GetAddrHostCfgdIF_Nbr(addr);
        }
    }

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                    NetIPv4_GetAddrHostCfgdIF_Nbr()
*
* Description : Get the interface number for a configured IPv4 host address.
*
* Argument(s) : addr        Configured IPv4 host address to get the interface number (see Note #2).
*
* Return(s)   : Interface number of a configured IPv4 host address, if available.
*
*               NET_IF_NBR_NONE,                                  otherwise.
*
* Caller(s)   : NetIPv4_GetAddrHostIF_Nbr(),
*               NetIPv4_IsAddrHostCfgdHandler(),
*               NetIPv4_GetAddrProtocolIF_Nbr().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_GetAddrHostCfgdIF_Nbr() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*               (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

NET_IF_NBR  NetIPv4_GetAddrHostCfgdIF_Nbr (NET_IPv4_ADDR  addr)
{
    NET_IPv4_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;


    p_ip_addrs = NetIPv4_GetAddrsHostCfgd(addr, &if_nbr);
    if (p_ip_addrs == (NET_IPv4_ADDRS *)0) {
        return (NET_IF_NBR_NONE);
    }

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                        NetIPv4_IsAddrClassA()
*
* Description : Validate an IPv4 address as a Class-A IPv4 address.
*
*               (1) RFC #791, Section 3.2 'Addressing : Address Format' specifies IPv4 Class-A addresses
*                   as :
*
*                       Class  High Order Bits
*                       -----  ---------------
*                        (A)         0
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a Class-A IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrClassA (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_class_a;


    addr_class_a = ((addr & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) ? DEF_YES : DEF_NO;

    return (addr_class_a);
}


/*
*********************************************************************************************************
*                                        NetIPv4_IsAddrClassB()
*
* Description : Validate an IPv4 address as a Class-B IPv4 address.
*
*               (1) RFC #791, Section 3.2 'Addressing : Address Format' specifies IPv4 Class-B addresses
*                   as :
*
*                       Class  High Order Bits
*                       -----  ---------------
*                        (B)         10
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a Class-B IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrClassB (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_class_b;


    addr_class_b = ((addr & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) ? DEF_YES : DEF_NO;

    return (addr_class_b);
}


/*
*********************************************************************************************************
*                                        NetIPv4_IsAddrClassC()
*
* Description : Validate an IPv4 address as a Class-C IPv4 address.
*
*               (1) RFC #791, Section 3.2 'Addressing : Address Format' specifies IPv4 Class-C addresses
*                   as :
*
*                       Class  High Order Bits
*                       -----  ---------------
*                        (C)         110
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a Class-C IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrClassC (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_class_c;


    addr_class_c = ((addr & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) ? DEF_YES : DEF_NO;

    return (addr_class_c);
}


/*
*********************************************************************************************************
*                                        NetIPv4_IsAddrClassD()
*
* Description : Validate an IPv4 address as a Class-D IPv4 address.
*
*               (1) RFC #1112, Section 4 specifies IPv4 Class-D addresses as :
*
*                       Class  High Order Bits
*                       -----  ---------------
*                        (D)        1110
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a Class-D IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrClassD (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_class_d;


    addr_class_d = ((addr & NET_IPv4_ADDR_CLASS_D_MASK) == NET_IPv4_ADDR_CLASS_D) ? DEF_YES : DEF_NO;

    return (addr_class_d);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsAddrThisHost()
*
* Description : Validate an IPv4 address as a 'This Host' initialization IPv4 address.
*
*               (1) RFC #1122, Section 3.2.1.3.(a) specifies the IPv4 'This Host' initialization address
*                   as :
*
*                       0.0.0.0
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a 'This Host' initialization IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrThisHost (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_this_host;


    addr_this_host = (addr == NET_IPv4_ADDR_THIS_HOST) ? DEF_YES : DEF_NO;

    return (addr_this_host);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsAddrLocalHost()
*
* Description : Validate an IPv4 address as a 'Localhost' IPv4 address.
*
*               (1) RFC #1122, Section 3.2.1.3.(g) specifies the IPv4 'Localhost' address as :
*
*                       127.<host>
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a 'Localhost' IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrLocalHost (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_local_host;


    addr_local_host = ((addr >= NET_IPv4_ADDR_LOCAL_HOST_MIN) &&
                       (addr <= NET_IPv4_ADDR_LOCAL_HOST_MAX)) ? DEF_YES : DEF_NO;

    return (addr_local_host);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsAddrLocalLink()
*
* Description : Validate an IPv4 address as a link-local IPv4 address.
*
*               (1) RFC #3927, Section 2.1 specifies the "IPv4 Link-Local address ... range ... [as]
*                   inclusive" ... :
*
*                   (a) "from 169.254.1.0" ...
*                   (b) "to   169.254.254.255".
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a link-local IPv4 address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrLocalLink (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_local_link;


    addr_local_link = ((addr >= NET_IPv4_ADDR_LOCAL_LINK_HOST_MIN) &&
                       (addr <= NET_IPv4_ADDR_LOCAL_LINK_HOST_MAX)) ? DEF_YES : DEF_NO;

    return (addr_local_link);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsAddrBroadcast()
*
* Description : Validate an IPv4 address as a limited broadcast IPv4 address.
*
*               (1) RFC #1122, Section 3.2.1.3.(c) specifies the IPv4 limited broadcast address as :
*
*                       255.255.255.255
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a limited broadcast IP address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrBroadcast (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_broadcast;


    addr_broadcast = (addr == NET_IPv4_ADDR_BROADCAST) ? DEF_YES : DEF_NO;

    return (addr_broadcast);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsAddrMulticast()
*
* Description : Validate an IPv4 address as a multicast IP address.
*
*               (1) RFC #1122, Section 3.2.1.3 specifies IPv4 multicast addresses as "(Class D) address[es]".
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is a multicast IP address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application,
*               NetIPv4_IsAddrProtocolMulticast(),
*               NetICMPv4_RxPktValidate(),
*               NetICMPv4_TxMsgErrValidate().
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrMulticast (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_multicast;


    addr_multicast = NetIPv4_IsAddrClassD(addr);

    return (addr_multicast);
}


/*
*********************************************************************************************************
*                                         NetIPv4_IsAddrHost()
*
* Description : (1) Validate an IPv4 address as an IPv4 host address :
*
*                   (a) A configured  IPv4 host address (on an enabled interface)
*                   (b) A 'Localhost' IPv4      address
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #4).
*
* Return(s)   : DEF_YES, if IPv4 address is one of the host's IPv4 addresses.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_IsAddrHost() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv4_IsAddrHostHandler()  Note #2'.
*
*               (3) NetIPv4_IsAddrHost() blocked until network initialization completes.
*
*               (4) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrHost (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_host;
    NET_ERR      err;


    Net_GlobalLockAcquire((void *)&NetIPv4_IsAddrHost, &err);   /* Acquire net lock (see Note #2b).                     */
    if (err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
        goto exit_fail;
    }
#endif


    addr_host = NetIPv4_IsAddrHostHandler(addr);                /* Chk if any host addr.                                */
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
*                                       NetIPv4_IsAddrHostCfgd()
*
* Description : Validate an IPv4 address as a configured IPv4 host address on an enabled interface.
*
* Argument(s) : addr        IPv4 address to validate (see Note #3).
*
* Return(s)   : DEF_YES, if IPv4 address is one of the host's configured IPv4 addresses.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_IsAddrHostCfgd() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv4_IsAddrHostCfgdHandler()  Note #1'.
*
*               (2) NetIPv4_IsAddrHostCfgd() blocked until network initialization completes.
*
*               (3) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrHostCfgd (NET_IPv4_ADDR  addr)
{
    CPU_BOOLEAN  addr_host;
    NET_ERR      err;


                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetIPv4_IsAddrHostCfgd, &err);
    if (err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
        goto exit_fail;
    }
#endif


    addr_host = NetIPv4_IsAddrHostCfgdHandler(addr);            /* Chk if any cfg'd host addr.                          */
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_host = DEF_NO;
#endif

exit_release:

    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (addr_host);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsAddrsCfgdOnIF()
*
* Description : Check if any IPv4 host address(s) configured on an interface.
*
* Argument(s) : if_nbr      Interface number to check for configured IPv4 host address(s).
*
*               p_err      Pointer to variable that will receive the return error code from this function :
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               - RETURNED BY NetIPv4_IsAddrsCfgdOnIF_Handler() : -
*                               NET_IPv4_ERR_NONE               Configured IPv4 host address(s) availability
*                                                                   successfully returned.
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ----- RETURNED BY Net_GlobalLockAcquire() : -------
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : DEF_YES, if any IP host address(s) configured on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_IsAddrsCfgdOnIF() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIPv4_IsAddrsCfgdOnIF_Handler()  Note #1'.
*
*               (2) NetIPv4_IsAddrsCfgdOnIF() blocked until network initialization completes.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrsCfgdOnIF (NET_IF_NBR   if_nbr,
                                      NET_ERR     *p_err)
{
    CPU_BOOLEAN  addr_avail;


                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }

                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetIPv4_IsAddrsCfgdOnIF, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #2).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        goto exit_fail;
    }
#endif


    addr_avail = NetIPv4_IsAddrsCfgdOnIF_Handler(if_nbr, p_err);    /* Chk IF for any cfg'd host addr(s).               */
    goto exit_release;


exit_lock_fault:
    return (DEF_FAIL);

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
exit_fail:
    addr_avail = DEF_NO;
#endif

exit_release:
    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (addr_avail);
}


/*
*********************************************************************************************************
*                                   NetIPv4_IsAddrsCfgdOnIF_Handler()
*
* Description : Check if any IPv4 address(s) configured on an interface.
*
* Argument(s) : if_nbr      Interface number to check for configured IPv4 address(s).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Configured IPv4 host address(s) availability
*                                                                   successfully returned.
*
*                                                               - RETURNED BY NetIF_IsValidCfgdHandler() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : DEF_YES, if any IPv4 host address(s) configured on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_IsAddrsCfgdOnIF(),
*               NetMgr_IsAddrsCfgdOnIF().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIPv4_IsAddrsCfgdOnIF_Handler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv4_IsAddrsCfgdOnIF()  Note #1'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrsCfgdOnIF_Handler (NET_IF_NBR   if_nbr,
                                              NET_ERR     *p_err)
{
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    CPU_BOOLEAN       addr_avail;


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_NO);
    }
                                                                /* ----------------- VALIDATE IF NBR ------------------ */
   (void)NetIF_IsValidCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (DEF_NO);
    }
#endif

                                                                /* ------------ CHK CFG'D IPv4 ADDRS AVAIL ------------ */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
    addr_avail = (p_ip_if_cfg->AddrsNbrCfgd > 0) ? DEF_YES : DEF_NO;
   *p_err       =  NET_IPv4_ERR_NONE;

    return (addr_avail);
}


/*
*********************************************************************************************************
*                                       NetIPv4_IsValidAddrHost()
*
* Description : (1) Validate an IPv4 host address :
*
*                   (a) MUST NOT be one of the following :
*
*                       (1) This      Host                              RFC #1122, Section 3.2.1.3.(a)
*                       (2) Specified Host                              RFC #1122, Section 3.2.1.3.(b)
*                       (3) Limited   Broadcast                         RFC #1122, Section 3.2.1.3.(c)
*                       (4) Directed  Broadcast                         RFC #1122, Section 3.2.1.3.(d)
*                       (5) Localhost                                   RFC #1122, Section 3.2.1.3.(g)
*                       (6) Multicast host address                      RFC #1112, Section 7.2
*
*                   (b) (1) RFC #3927, Section 2.1 specifies the "IPv4 Link-Local address" :
*
*                           (A) "Range ... inclusive"  ...
*                               (1) "from 169.254.1.0" ...
*                               (2) "to   169.254.254.255".
*
*               (2) ONLY validates typical IPv4 host addresses, since 'This Host' & 'Specified Host' IPv4
*                   host addresses are ONLY valid during a host's initialization (see Notes #1a1 & #1a4).
*                   This function CANNOT be used to validate any 'This Host' or 'Specified Host' host
*                   addresses.
*
* Argument(s) : addr_host   IPv4 host address to validate (see Note #4).
*
* Return(s)   : DEF_YES, if IPv4 host address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : various.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (3) See 'net_ipv4.h  IPv3 ADDRESS DEFINES  Notes #2 & #3' for supported IPv4 addresses.
*
*               (4) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidAddrHost (NET_IPv4_ADDR  addr_host)
{
    CPU_BOOLEAN  valid;


    valid = DEF_YES;
                                                                /* ---------------- VALIDATE HOST ADDR ---------------- */
                                                                /* Chk invalid 'This Host'         (see Note #1a1).     */
    if (addr_host == NET_IPv4_ADDR_THIS_HOST) {
        valid = DEF_NO;

                                                                /* Chk invalid lim'd broadcast     (see Note #1a3).     */
    } else if (addr_host == NET_IPv4_ADDR_BROADCAST) {
        valid = DEF_NO;

                                                                /* Chk invalid localhost           (see Note #1a5).     */
    } else if ((addr_host & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                            NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
        valid = DEF_NO;

                                                                /* Chk         link-local addrs    (see Note #1b1).     */
    } else if ((addr_host & NET_IPv4_ADDR_LOCAL_LINK_MASK_NET) ==
                            NET_IPv4_ADDR_LOCAL_LINK_NET     ) {
                                                                /* Chk invalid link-local addr     (see Note #1b1A).    */
        if ((addr_host < NET_IPv4_ADDR_LOCAL_LINK_HOST_MIN) ||
            (addr_host > NET_IPv4_ADDR_LOCAL_LINK_HOST_MAX)) {
             valid = DEF_NO;
        }


    } else if ((addr_host & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
                                                                /* Chk invalid Class-A 'This Host' (see Note #1a2).     */
        if ((addr_host               & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
             valid = DEF_NO;
        }
                                                                /* Chk invalid Class-A broadcast   (see Note #1a4).     */
        if ((addr_host               & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
             valid = DEF_NO;
        }


    } else if ((addr_host & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
                                                                /* Chk invalid Class-B 'This Host' (see Note #1a2).     */
        if ((addr_host               & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
             valid = DEF_NO;
        }
                                                                /* Chk invalid Class-B broadcast   (see Note #1a4).     */
        if ((addr_host               & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
             valid = DEF_NO;
        }


    } else if ((addr_host & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
                                                                /* Chk invalid Class-C 'This Host' (see Note #1a2).     */
        if ((addr_host               & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
             valid = DEF_NO;
        }
                                                                /* Chk invalid Class-C broadcast   (see Note #1a4).     */
        if ((addr_host               & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
             valid = DEF_NO;
        }


    } else if ((addr_host & NET_IPv4_ADDR_CLASS_D_MASK) == NET_IPv4_ADDR_CLASS_D) {
                                                                /* Chk invalid Class-D multicast   (see Note #1a6).     */
        valid = DEF_NO;


    } else {                                                    /* Invalid addr class (see Note #3).                    */
        valid = DEF_NO;
    }


    return (valid);
}


/*
*********************************************************************************************************
*                                     NetIPv4_IsValidAddrHostCfgd()
*
* Description : (1) Validate an IPv4 address for a configured IPv4 host address :
*
*                   (a) MUST NOT be one of the following :
*
*                       (1) This      Host                              RFC #1122, Section 3.2.1.3.(a)
*                       (2) Specified Host                              RFC #1122, Section 3.2.1.3.(b)
*                       (3) Limited   Broadcast                         RFC #1122, Section 3.2.1.3.(c)
*                       (4) Directed  Broadcast                         RFC #1122, Section 3.2.1.3.(d)
*                       (5) Subnet    Broadcast                         RFC #1122, Section 3.2.1.3.(e)
*                       (6) Localhost                                   RFC #1122, Section 3.2.1.3.(g)
*                       (7) Multicast host address                      RFC #1112, Section 7.2
*
*                   See also 'NetIPv4_IsValidAddrHost()  Note #1'.
*
*               (2) ONLY validates this host's IPv4 address, since 'This Host' & 'Specified Host' IPv4 host
*                   addresses are ONLY valid during a host's initialization (see Notes #1a1 & #1a4).  This
*                   function CANNOT be used to validate any 'This Host' or 'Specified Host' host addresses.
*
*
* Argument(s) : addr_host           IPv4 host address to validate (see Note #4).
*
*               addr_subnet_mask    IPv4      address subnet mask (see Note #4).
*
* Return(s)   : DEF_YES, if this host's IPv4 address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_CfgAddrAdd(),
*               NetIPv4_CfgAddrAddDynamic(),
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (3) See 'net_ipv4.h  IP ADDRESS DEFINES  Notes #2 & #3' for supported IPv4 addresses.
*
*               (4) IPv4 addresses MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidAddrHostCfgd (NET_IPv4_ADDR  addr_host,
                                          NET_IPv4_ADDR  addr_subnet_mask)
{
    CPU_BOOLEAN  valid_host;
    CPU_BOOLEAN  valid_mask;
    CPU_BOOLEAN  valid;


    if ((addr_host        == NET_IPv4_ADDR_NONE) ||                 /* Chk invalid NULL addr(s).                        */
        (addr_subnet_mask == NET_IPv4_ADDR_NONE)) {
         return (DEF_NO);
    }
                                                                    /* Chk invalid subnet 'This Host' (see Note #1a2).  */
    if ((addr_host               & ~addr_subnet_mask) ==
        (NET_IPv4_ADDR_THIS_HOST & ~addr_subnet_mask)) {
         return (DEF_NO);
    }
                                                                    /* Chk invalid subnet broadcast   (see Note #1a5).  */
    if ((addr_host               & ~addr_subnet_mask) ==
        (NET_IPv4_ADDR_BROADCAST & ~addr_subnet_mask)) {
         return (DEF_NO);
    }


    valid_host = NetIPv4_IsValidAddrHost(addr_host);
    valid_mask = NetIPv4_IsValidAddrSubnetMask(addr_subnet_mask);

    valid      = ((valid_host == DEF_YES) &&
                  (valid_mask == DEF_YES)) ? DEF_YES : DEF_NO;

    return (valid);
}


/*
*********************************************************************************************************
*                                    NetIPv4_IsValidAddrSubnetMask()
*
* Description : (1) Validate an IPv4 address subnet mask :
*
*                   (a) RFC #1122, Section 3.2.1.3 states that :
*
*                       (1) "IP addresses are not permitted to have the value 0 or -1 for any of the ...
*                            <Subnet-number> fields" ...
*                       (2) "This implies that each of these fields will be at least two bits long."
*
*                   (b) RFC #950, Section 2.1 'Special Addresses' reiterates that "the values of all
*                       zeros and all ones in the subnet field should not be assigned to actual
*                       (physical) subnets".
*
*                   (c) RFC #950, Section 2.1 also states that "the bits that identify the subnet ...
*                       need not be adjacent in the address.  However, we recommend that the subnet
*                       bits be contiguous and located as the most significant bits of the local
*                       address".
*
*                       #### Therefore, it is assumed that at least the most significant bit of the
*                       network portion of the subnet address SHOULD be set.
*
*                       See also 'net_ipv4.h  IPv4 ADDRESS DEFINES  Note #2b2'.
*
* Argument(s) : addr_subnet_mask    IPv4 address subnet mask to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address subnet mask valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_IsValidAddrHostCfgd().
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (2) IPv4 addresses MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidAddrSubnetMask (NET_IPv4_ADDR  addr_subnet_mask)
{
    CPU_BOOLEAN    valid;
    NET_IPv4_ADDR  mask;
    CPU_INT08U     mask_size;
    CPU_INT08U     mask_nbr_one_bits;
    CPU_INT08U     mask_nbr_one_bits_min;
    CPU_INT08U     mask_nbr_one_bits_max;
    CPU_INT08U     i;

                                                                    /* ------------- VALIDATE SUBNET MASK ------------- */
                                                                    /* Chk invalid subnet class (see Note #1c).         */
    if ((addr_subnet_mask & NET_IPv4_ADDR_CLASS_SUBNET_MASK_MIN) == NET_IPv4_ADDR_NONE) {
        valid = DEF_NO;

    } else {                                                        /* Chk invalid subnet mask (see Notes #1a & #1b).   */
        mask_size         = sizeof(addr_subnet_mask) * DEF_OCTET_NBR_BITS;
        mask              = DEF_BIT_00;
        mask_nbr_one_bits = 0u;
        for (i = 0u; i < mask_size; i++) {                          /* Calc nbr subnet bits.                            */
            if (addr_subnet_mask & mask) {
                mask_nbr_one_bits++;
            }
            mask <<= 1u;
        }

        mask_nbr_one_bits_min = 2u;                                 /* See Note #1a2.                                   */
        mask_nbr_one_bits_max = mask_size - mask_nbr_one_bits_min;
                                                                    /* Chk invalid nbr subnet bits (see Note #1a2).     */
        if (mask_nbr_one_bits < mask_nbr_one_bits_min) {
            valid = DEF_NO;

        } else if (mask_nbr_one_bits > mask_nbr_one_bits_max) {
            valid = DEF_NO;

        } else {
            valid = DEF_YES;
        }
    }


    return (valid);
}


/*
*********************************************************************************************************
*                                         NetIPv4_IsValidTOS()
*
* Description : Validate an IPv4 TOS.
*
* Argument(s) : TOS         IPv4 TOS to validate.
*
* Return(s)   : DEF_YES, if IPv4 TOS valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_TxPktValidate(),
*               NetConn_IPv4_TxTOS_Set().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) See 'net_ipv4.h  IPv4 HEADER TYPE OF SERVICE (TOS) DEFINES  Note #1'
*                     & 'net_ipv4.h  IPv4 HEADER  Note #3'.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidTOS (NET_IPv4_TOS  TOS)
{
    CPU_BOOLEAN  tos_mbz;


    tos_mbz = DEF_BIT_IS_SET(TOS, NET_IPv4_HDR_TOS_MBZ_MASK);     /* Chk for invalid TOS bit(s).                          */
    if (tos_mbz != DEF_NO) {
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                         NetIPv4_IsValidTTL()
*
* Description : Validate an IPv4 TTL.
*
* Argument(s) : TTL         IPv4 TTL to validate.
*
* Return(s)   : DEF_YES, if IPv4 TTL valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_TxPktValidate(),
*               NetConn_IPv4_TxTTL_Set().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) RFC #1122, Section 3.2.1.7 states that "a host MUST NOT send a datagram with a
*                   Time-to-Live (TTL) value of zero".
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidTTL (NET_IPv4_TTL  TTL)
{
    if (TTL < 1) {                                              /* Chk TTL < 1 (see Note #1).                           */
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                        NetIPv4_IsValidFlags()
*
* Description : Validate IPv4 flags.
*
* Argument(s) : flags    IPv4 flags to select options; bit-field flags logically OR'd :
*
*                               NET_IPv4_FLAG_NONE              No  IPv4 flags selected.
*                               NET_IPv4_FLAG_TX_DONT_FRAG      Set IPv4 'Don't Frag' flag.
*
* Return(s)   : DEF_YES, if IPv4 flags valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_TxPktValidate(),
*               NetConn_IPv4_TxFlagsSet().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidFlags (NET_IPv4_FLAGS  flags)
{
    NET_IPv4_FLAGS  flag_mask;


    flag_mask = NET_IPv4_FLAG_NONE        |
                NET_IPv4_FLAG_TX_DONT_FRAG;
                                                                /* Chk for any invalid flags req'd.                 */
    if ((flags & (NET_IPv4_FLAGS)~flag_mask) != NET_IPv4_FLAG_NONE) {
        return (DEF_NO);
    }

    return (DEF_YES);
}


/*
*********************************************************************************************************
*                                             NetIPv4_Rx()
*
* Description : (1) Process received datagrams & forward to network protocol layers :
*
*                   (a) Validate IPv4 packet & options
*                   (b) Reassemble fragmented datagrams
*                   (c) Demultiplex datagram to higher-layer protocols
*                   (d) Update receive statistics
*
*               (2) Although IPv4 data units are typically referred to as 'datagrams' (see RFC #791, Section 1.1),
*                   the term 'IP packet' (see RFC #1983, 'packet') is used for IPv4 Receive until the packet is
*                   validated, & possibly reassembled, as an IPv4 datagram.
*
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv4 packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IP datagram successfully received & processed.
*
*                                                               ---- RETURNED BY NetIPv4_RxPktDiscard() : ----
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_RxPktDemux(),
*               Network interface receive functions.
*
*               This function is a network protocol suite to network interface (IF) function & SHOULD be
*               called only by appropriate network interface function(s).
*
* Note(s)     : (3) Since NetIPv4_RxPktFragReasm() may return a pointer to a different packet buffer (see
*                   'NetIPv4_RxPktFragReasm()  Return(s)', 'p_buf_hdr' MUST be reloaded.
*
*               (4) (a) For single packet buffer IPv4 datagrams, the datagram length is equal to the IPv4
*                       Total Length minus the IPv4 Header Length.
*
*                   (b) For multiple packet buffer, fragmented IPv4 datagrams, the datagram length is
*                       equal to the previously calculated total fragment size.
*
*                       (1) IP datagram length is stored ONLY in the first packet buffer of any
*                           fragmented packet buffers.
*
*               (5) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

void  NetIPv4_Rx (NET_BUF  *p_buf,
                  NET_ERR  *p_err)
{
    NET_BUF_HDR    *p_buf_hdr;
    NET_IPv4_HDR   *p_ip_hdr;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIPv4_RxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
        return;
    }
#endif


    NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxPktCtr);

                                                                /* -------------- VALIDATE RX'D IPv4 PKT -------------- */
    p_buf_hdr = &p_buf->Hdr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIPv4_RxPktValidateBuf(p_buf_hdr, p_err);                 /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIPv4_RxPktDiscard(p_buf, p_err);
             return;
    }
#endif
    p_ip_hdr = (NET_IPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];

    NetIPv4_RxPktValidate(p_buf, p_buf_hdr, p_ip_hdr, p_err);   /* Validate rx'd pkt.                                   */



                                                                /* ------------------- REASM FRAGS -------------------- */
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             p_buf = NetIPv4_RxPktFragReasm(p_buf, p_buf_hdr, p_ip_hdr, p_err);
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_IPv4_ERR_INVALID_VER:
        case NET_IPv4_ERR_INVALID_LEN_HDR:
        case NET_IPv4_ERR_INVALID_LEN_TOT:
        case NET_IPv4_ERR_INVALID_LEN_DATA:
        case NET_IPv4_ERR_INVALID_FLAG:
        case NET_IPv4_ERR_INVALID_FRAG:
        case NET_IPv4_ERR_INVALID_PROTOCOL:
        case NET_IPv4_ERR_INVALID_CHK_SUM:
        case NET_IPv4_ERR_INVALID_ADDR_SRC:
        case NET_IPv4_ERR_INVALID_ADDR_DEST:
        case NET_IPv4_ERR_INVALID_OPT:
        case NET_IPv4_ERR_INVALID_OPT_LEN:
        case NET_IPv4_ERR_INVALID_OPT_NBR:
        case NET_IPv4_ERR_INVALID_OPT_END:
        case NET_IPv4_ERR_INVALID_OPT_FLAG:
        case NET_IPv4_ERR_RX_OPT_BUF_NONE_AVAIL:
        case NET_IPv4_ERR_RX_OPT_BUF_LEN:
        case NET_IPv4_ERR_RX_OPT_BUF_WR:
        default:
             NetIPv4_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                            /* ------------ DEMUX DATAGRAM ------------ */
    switch (*p_err) {                                                       /* Chk err from NetIPv4_RxPktFragReasm().   */
        case NET_IPv4_ERR_RX_FRAG_NONE:
        case NET_IPv4_ERR_RX_FRAG_COMPLETE:
             p_buf_hdr = &p_buf->Hdr;                                       /* Reload buf hdr ptr (see Note #3).        */
             if (*p_err == NET_IPv4_ERR_RX_FRAG_NONE) {                     /* If pkt NOT frag'd, ...                   */
                  p_buf_hdr->IP_DatagramLen = p_buf_hdr->IP_TotLen          /* ... calc buf datagram len (see Note #4a).*/
                                           - p_buf_hdr->IP_HdrLen;
             } else {                                                       /* Else set tot frag size ...               */
                  p_buf_hdr->IP_DatagramLen = p_buf_hdr->IP_FragSizeTot;    /* ...       as datagram len (see Note #4b).*/
             }
             NetIPv4_RxPktDemuxDatagram(p_buf, p_buf_hdr, p_err);
             break;


        case NET_IPv4_ERR_RX_FRAG_REASM:                        /* Frag'd datagram in reasm.                            */
            *p_err = NET_IPv4_ERR_NONE;
             return;


        case NET_IPv4_ERR_RX_FRAG_DISCARD:
        case NET_IPv4_ERR_RX_FRAG_OFFSET:
        case NET_IPv4_ERR_RX_FRAG_SIZE:
        case NET_IPv4_ERR_RX_FRAG_SIZE_TOT:
        case NET_IPv4_ERR_RX_FRAG_LEN_TOT:
        default:
             NetIPv4_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* ----------------- UPDATE RX STATS ------------------ */
    switch (*p_err) {                                           /* Chk err from NetIPv4_RxPktDemuxDatagram().           */
        case NET_ICMPv4_ERR_NONE:
        case NET_IGMP_ERR_NONE:
        case NET_UDP_ERR_NONE:
        case NET_TCP_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxDgramCompCtr);
            *p_err = NET_IPv4_ERR_NONE;
             break;


        case NET_ERR_RX:
                                                                /* See Note #5.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxPktDisCtr);
                                                                /* Rtn err from NetIPv4_RxPktDemuxDatagram().           */
             return;


        case NET_ERR_INVALID_PROTOCOL:
        default:
             NetIPv4_RxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                             NetIPv4_Tx()
*
* Description : (1) Prepare & transmit IPv4 datagram packet(s) :
*
*                   (a) Validate  transmit packet
*                   (b) Prepare & transmit packet datagram
*                   (c) Update    transmit statistics
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit IPv4 packet.
*
*               addr_src    Source      IPv4 address.
*
*               addr_dest   Destination IPv4 address.
*
*               TOS         Specific TOS to transmit IPv4 packet (see Note #2a).
*
*               TTL         Specific TTL to transmit IPv4 packet (see Note #2b) :
*
*                               NET_IPv4_TTL_MIN                minimum TTL transmit value   (1)
*                               NET_IPv4_TTL_MAX                maximum TTL transmit value (255)
*                               NET_IPv4_TTL_DFLT               default TTL transmit value (128)
*                               NET_IPv4_TTL_NONE               replace with default TTL
*
*               flags       Flags to select transmit options; bit-field flags logically OR'd :
*
*                               NET_IPv4_FLAG_NONE              No  IPv4 transmit flags selected.
*                               NET_IPv4_FLAG_TX_DONT_FRAG      Set IPv4 'Don't Frag' flag.
*
*               p_opts      Pointer to one or more IPv4 options configuration data structures (see Note #2c) :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IPv4_OPT_CFG_ROUTE_TS       Route &/or Internet Timestamp options configuration.
*                               NET_IPv4_OPT_CFG_SECURITY       Security options configuration
*                                                                   (see 'net_ipv4.c  Note #1d').
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IPv4 datagram(s)     successfully prepared &
*                                                                   transmitted to network interface layer.
*                               NET_IPv4_ERR_TX_PKT             IPv4 datagram(s) NOT successfully prepared or
*                                                                   transmitted to network interface layer.
*
*                                                               -- RETURNED BY NetIPv4_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               ------ RETURNED BY NetIPv4_TxPkt() : ------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxMsgErr(),
*               NetICMPv4_TxMsgReq(),
*               NetICMPv4_TxMsgReply(),
*               NetUDP_Tx(),
*               NetTCP_TxPkt().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) (a) RFC #1122, Section 3.2.1.6 states that :
*
*                       (1) "The IP layer MUST provide a means ... to set the TOS field of every datagram
*                            that is sent;" ...
*                       (2) "the default is all zero bits."
*
*                       See also 'net_ipv4.h  IPv4 HEADER TYPE OF SERVICE (TOS) DEFINES'.
*
*                   (b) RFC #1122, Section 3.2.1.7 states that :
*
*                       (1) "The IP layer MUST provide a means ... to set the TTL field of every datagram
*                            that is sent."
*
*                       (2) "A host MUST NOT send a datagram with a Time-to-Live (TTL) value of zero."
*
*                       (3) "When a fixed TTL value is used, it MUST be configurable."
*
*                       See also 'net_ipv4.h  IPv4 HEADER TIME-TO-LIVE (TTL) DEFINES'.
*
*                   (c) RFC #1122, Section 3.2.1.8 states that "there MUST be a means ... to specify IP
*                       options to be included in transmitted IP datagrams".
*
*                       See also 'net_ipv4.h  IPv4 HEADER OPTION CONFIGURATION DATA TYPES'.
*
*               (3) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

void  NetIPv4_Tx (NET_BUF        *p_buf,
                  NET_IPv4_ADDR   addr_src,
                  NET_IPv4_ADDR   addr_dest,
                  NET_IPv4_TOS    TOS,
                  NET_IPv4_TTL    TTL,
                  CPU_INT16U      flags,
                  void           *p_opts,
                  NET_ERR        *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_ERR       err;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIPv4_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
        return;
    }
#endif


                                                                /* --------------- VALIDATE IPv4 TX PKT --------------- */
    p_buf_hdr = &p_buf->Hdr;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    NetIPv4_TxPktValidate(p_buf_hdr,
                          addr_src,
                          addr_dest,
                          TOS,
                          TTL,
                          flags,
                          p_opts,
                          p_err);
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_IF_ERR_INVALID_IF:
        case NET_IPv4_ERR_INVALID_LEN_DATA:
        case NET_IPv4_ERR_INVALID_TOS:
        case NET_IPv4_ERR_INVALID_FLAG:
        case NET_IPv4_ERR_INVALID_TTL:
        case NET_IPv4_ERR_INVALID_ADDR_SRC:
        case NET_IPv4_ERR_INVALID_ADDR_DEST:
        case NET_IPv4_ERR_INVALID_ADDR_GATEWAY:
        case NET_IPv4_ERR_INVALID_OPT_TYPE:
        case NET_IPv4_ERR_INVALID_OPT_LEN:
        case NET_IPv4_ERR_INVALID_OPT_CFG:
        case NET_IPv4_ERR_INVALID_OPT_ROUTE:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIPv4_TxPktDiscard(p_buf, &err);
            *p_err = NET_IPv4_ERR_TX_PKT;
             return;
    }
#endif


                                                                /* ------------------- TX IPv4 PKT -------------------- */
    NetIPv4_TxPkt(p_buf,
                  p_buf_hdr,
                  addr_src,
                  addr_dest,
                  TOS,
                  TTL,
                  flags,
                  p_opts,
                  p_err);


                                                                /* ----------------- UPDATE TX STATS ------------------ */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDgramCtr);
            *p_err = NET_IPv4_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
        case NET_IF_ERR_INVALID_IF:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxPktDisCtr);
                                                                /* Rtn err from NetIPv4_TxPkt().                        */
             return;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_IF_ERR_INVALID_CFG:
        case NET_ERR_FAULT_NULL_FNCT:
        case NET_IPv4_ERR_INVALID_LEN_HDR:
        case NET_IPv4_ERR_INVALID_FRAG:
        case NET_IPv4_ERR_INVALID_OPT_TYPE:
        case NET_IPv4_ERR_INVALID_OPT_LEN:
        case NET_IPv4_ERR_INVALID_ADDR_HOST:
        case NET_IPv4_ERR_INVALID_ADDR_GATEWAY:
        case NET_IPv4_ERR_TX_DEST_INVALID:
        case NET_BUF_ERR_INVALID_IX:
        case NET_BUF_ERR_INVALID_LEN:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_UTIL_ERR_NULL_SIZE:
             NetIPv4_TxPktDiscard(p_buf, &err);
            *p_err = NET_IPv4_ERR_TX_PKT;
             return;


        default:
             NetIPv4_TxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                            NetIPv4_ReTx()
*
* Description : (1) Prepare & re-transmit packets from transport protocol layers to network interface layer :
*
*                   (a) Validate  re-transmit packet
*                   (b) Prepare & re-transmit packet datagram
*                   (c) Update    re-transmit statistics
*
*
* Argument(s) : p_buf       Pointer to network buffer to re-transmit IPv4 packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IPv4 datagram(s) successfully re-transmitted
*                                                                   to network interface layer.
*
*                                                               -- RETURNED BY NetIPv4_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               ----- RETURNED BY NetIPv4_ReTxPkt() : -----
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN         Network  interface link state down (i.e.
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

void  NetIPv4_ReTx (NET_BUF  *p_buf,
                    NET_ERR  *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIPv4_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
        return;
    }
#endif


                                                                /* ------------- VALIDATE IPv4 RE-TX PKT -------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf_hdr->IP_HdrIx == NET_BUF_IX_NONE) {
        NetIPv4_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvBufIxCtr);
        return;
    }
#endif


                                                                /* ------------------ RE-TX IPv4 PKT ------------------ */
    NetIPv4_ReTxPkt(p_buf,
                    p_buf_hdr,
                    p_err);


                                                                /* ---------------- UPDATE RE-TX STATS ---------------- */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDgramCtr);
            *p_err = NET_IPv4_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #2.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxPktDisCtr);
                                                                /* Rtn err from NetIPv4_ReTxPkt().                      */
             return;


        case NET_IPv4_ERR_INVALID_ADDR_HOST:
        case NET_IPv4_ERR_INVALID_ADDR_GATEWAY:
        case NET_IPv4_ERR_TX_DEST_INVALID:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_UTIL_ERR_NULL_SIZE:
        default:
             NetIPv4_TxPktDiscard(p_buf, p_err);
             return;
    }
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
*                                   NetIPv4_CfgAddrRemoveAllHandler()
*
* Description : (1) Remove all configured IPv4 host address(s) from an interface :
*
*                   (a) Validate IPv4 address configuration state                                   See Note #3b
*                   (b) Remove ALL configured IPv4 address(s) from interface's IPv4 address table :
*                       (1) Close all connections for each address
*                   (c) Reset    IPv4 address configuration state to static                         See Note #3c
*
*
*
* Argument(s) : if_nbr      Interface number to remove address configuration.
*               ------      Argument validated in NetIPv4_CfgAddrRemoveAll(),
*                                                 NetIPv4_CfgAddrAddDynamicStart(),
*                                                 NetIPv4_CfgAddrAddDynamic().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               ALL configured IPv4 address(s) successfully
*                                                                   removed.
*                               NET_IPv4_ERR_ADDR_CFG_STATE     Invalid IPv4 address configuration state
*                                                                   (see Note #3b).
*
* Return(s)   : DEF_OK,   if ALL interface's configured IPv4 host address(s) successfully removed.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIPv4_CfgAddrRemoveAll(),
*               NetIPv4_CfgAddrAddDynamicStart(),
*               NetIPv4_CfgAddrAddDynamic().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_CfgAddrRemoveAllHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv4_CfgAddrRemoveAll()  Note #2'.
*
*               (3) (a) An interface may be configured with either :
*
*                       (1) One or more statically- configured IPv4 addresses (default configuration)
*                               OR
*                       (2) Exactly one dynamically-configured IPv4 address
*
*                   (b) If an interface's IPv4 host address(s) are NOT currently statically- or dynamically-
*                       configured, then NO address(s) may NOT be removed.  However, an interface in the
*                       dynamic-init may call this function which effect will solely put that interface
*                       back in the default statically-configured mode.
*
*                   (c) When NO address(s) are configured on an interface after ALL address(s) are removed,
*                       the interface's address configuration is defaulted back to statically-configured.
*
*                       See also 'NetIPv4_Init()           Note #2b'
*                              & 'NetIPv4_CfgAddrRemove()  Note #5c'.
*
*                   See also 'net_ipv4.h  NETWORK INTERFACES' IPv4 ADDRESS CONFIGURATION DATA TYPE  Note #1b',
*                            'NetIPv4_CfgAddrAdd()         Note #7',
*                            'NetIPv4_CfgAddrAddDynamic()  Note #8',
*                          & 'NetIPv4_CfgAddrRemove()      Note #5'.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetIPv4_CfgAddrRemoveAllHandler (NET_IF_NBR   if_nbr,
                                                      NET_ERR     *p_err)
{
    NET_IPv4_ADDR      addr_cfgd;
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_SR_ALLOC();


                                                                        /* ------- VALIDATE IPv4 ADDR CFG STATE ------- */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    switch (p_ip_if_cfg->AddrCfgState) {
        case NET_IPv4_ADDR_CFG_STATE_STATIC:                            /* See Note #3b.                                */
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC:
        case NET_IPv4_ADDR_CFG_STATE_DYNAMIC_INIT:
             break;


        case NET_IPv4_ADDR_CFG_STATE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgAddrStateCtr);
            *p_err =  NET_IPv4_ERR_ADDR_CFG_STATE;
             return (DEF_FAIL);
    }


                                                                        /* ------ REMOVE ALL CFG'D IPv4 ADDR(S) ------- */
    addr_ix   =  0u;
    p_ip_addrs = &p_ip_if_cfg->AddrsTbl[addr_ix];
    while (addr_ix < p_ip_if_cfg->AddrsNbrCfgd) {                       /* Remove ALL cfg'd addrs.                      */
                                                                        /* Close  all cfg'd addr's conns.               */
        addr_cfgd = NET_UTIL_HOST_TO_NET_32(p_ip_addrs->AddrHost);
        NetConn_CloseAllConnsByAddrHandler((CPU_INT08U      *)      &addr_cfgd,
                                           (NET_CONN_ADDR_LEN)sizeof(addr_cfgd));

                                                                        /* Remove addr from tbl.                        */
        p_ip_addrs->AddrHost               = NET_IPv4_ADDR_NONE;
        p_ip_addrs->AddrHostSubnetMask     = NET_IPv4_ADDR_NONE;
        p_ip_addrs->AddrHostSubnetMaskHost = NET_IPv4_ADDR_NONE;
        p_ip_addrs->AddrHostSubnetNet      = NET_IPv4_ADDR_NONE;
        p_ip_addrs->AddrDfltGateway        = NET_IPv4_ADDR_NONE;

        p_ip_addrs++;
        addr_ix++;
    }

    p_ip_if_cfg->AddrsNbrCfgd         = 0u;                             /* NO  addr(s) cfg'd.                           */
    p_ip_if_cfg->AddrCfgState         = NET_IPv4_ADDR_CFG_STATE_STATIC; /* Set to static addr cfg (see Note #3c).       */
    CPU_CRITICAL_ENTER();
    p_ip_if_cfg->AddrProtocolConflict = DEF_NO;                         /* Clr addr conflict.                           */
    CPU_CRITICAL_EXIT();


   *p_err =  NET_IPv4_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       NetIPv4_CfgAddrValidate()
*
* Description : Validate an IPv4 host address, subnet mask, & default gateway for configuration on an
*               interface.
*
* Argument(s) : addr_host           Desired IPv4 address                 to configure (see Note #1).
*
*               addr_subnet_mask    Desired IPv4 address subnet mask     to configure (see Note #1).
*
*               addr_dflt_gateway   Desired IPv4 default gateway address to configure (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   IP address successfully configured.
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid IPv4 address, subnet mask, or
*                                                                       address/subnet mask combination.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 default gateway address.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_CfgAddrAdd(),
*               NetIPv4_CfgAddrAddDynamic().
*
* Note(s)     : (1) IPv4 addresses MUST be in host-order.
*
*               (2) (a) RFC #1122, Section 3.3.1.1 states that "the host IP layer MUST operate correctly
*                       in a minimal network environment, and in particular, when there are no gateways".
*
*                       In other words, a host on an isolated network should be able to correctly operate
*                       & communicate with all other hosts on its local network without need of a gateway
*                       or configuration of a gateway.
*
*                       See also 'NetIPv4_TxPktDatagramRouteSel()  Note #3b1c1'.
*
*                   (b) However, a configured gateway MUST be on the same network as the host's IPv4 address
*                       -- i.e. the network portion of the configured IPv4 address & the configured gateway
*                       addresses MUST be identical.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
static  void  NetIPv4_CfgAddrValidate (NET_IPv4_ADDR   addr_host,
                                       NET_IPv4_ADDR   addr_subnet_mask,
                                       NET_IPv4_ADDR   addr_dflt_gateway,
                                       NET_ERR        *p_err)
{
    CPU_BOOLEAN  addr_valid;

                                                                        /* Validate host addr & subnet mask.            */
    addr_valid = NetIPv4_IsValidAddrHostCfgd(addr_host, addr_subnet_mask);
    if (addr_valid != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvAddrHostCtr);
       *p_err =  NET_IPv4_ERR_INVALID_ADDR_HOST;
        return;
    }
                                                                        /* Validate dflt gateway (see Note #2).         */
    if (addr_dflt_gateway != NET_IPv4_ADDR_NONE) {
        addr_valid = NetIPv4_IsValidAddrHost(addr_dflt_gateway);
        if (addr_valid != DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvGatewayCtr);
           *p_err =  NET_IPv4_ERR_INVALID_ADDR_GATEWAY;
            return;
        }
                                                                        /* Validate dflt gateway subnet (see Note #2b). */
        if ((addr_dflt_gateway & addr_subnet_mask) !=
            (addr_host         & addr_subnet_mask)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvGatewayCtr);
            *p_err =  NET_IPv4_ERR_INVALID_ADDR_GATEWAY;
             return;
        }
    }

   *p_err = NET_IPv4_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetIPv4_GetAddrsHostCfgd()
*
* Description : Get interface number & IPv4 addresses structure for configured IPv4 address.
*
* Argument(s) : addr        Configured IPv4 host address to get the interface number & IPv4 addresses
*                               structure (see Note #1).
*
*               p_if_nbr    Pointer to variable that will receive ... :
*
*                               (a) The interface number for this configured IPv4 address, if available;
*                               (b) NET_IF_NBR_NONE,                                       otherwise.
*
* Return(s)   : Pointer to corresponding IPv4 address structure, if IPv4 address configured on any interface.
*
*               Pointer to NULL,                                 otherwise.
*
* Caller(s)   : NetIPv4_GetAddrSubnetMask(),
*               NetIPv4_GetAddrDfltGateway(),
*               NetIPv4_GetAddrHostCfgdIF_Nbr().
*
* Note(s)     : (1) IPv4 address MUST be in host-order.
*
*               (2) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

static  NET_IPv4_ADDRS  *NetIPv4_GetAddrsHostCfgd (NET_IPv4_ADDR   addr,
                                                   NET_IF_NBR     *p_if_nbr)
{
    NET_IPv4_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;


    if (p_if_nbr != (NET_IF_NBR *)0) {                          /* Init IF nbr for err (see Note #2).                   */
       *p_if_nbr  =  NET_IF_NBR_NONE;
    }

                                                                /* ---------------- VALIDATE IPv4 ADDR ---------------- */
    if (addr ==  NET_IPv4_ADDR_NONE) {
        return ((NET_IPv4_ADDRS *)0);
    }


                                                                /* -------- SRCH ALL CFG'D IF's FOR IPv4 ADDR --------- */
    if_nbr    =  NET_IF_NBR_BASE_CFGD;
    p_ip_addrs = (NET_IPv4_ADDRS *)0;

    while ((if_nbr    <   NET_IF_NBR_IF_TOT) &&                 /* Srch all cfg'd IF's ...                              */
           (p_ip_addrs == (NET_IPv4_ADDRS *)0)) {               /* ... until addr found.                                */

        p_ip_addrs = NetIPv4_GetAddrsHostCfgdOnIF(addr, if_nbr);
        if (p_ip_addrs == (NET_IPv4_ADDRS *)0) {                /* If addr NOT found, ...                               */
            if_nbr++;                                           /* ... adv to next IF nbr.                              */
        }
    }

    if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {                    /* If addr avail, ...                                   */
        if (p_if_nbr != (NET_IF_NBR *)0) {
           *p_if_nbr  =  if_nbr;                                /* ... rtn IF nbr.                                      */
        }
    }


    return (p_ip_addrs);
}


/*
*********************************************************************************************************
*                                    NetIPv4_GetAddrsHostCfgdOnIF()
*
* Description : Get IPv4 addresses structure for an interface's configured IPv4 address.
*
* Argument(s) : addr        Configured IPv4 host address to get the interface number & IPv4 addresses
*                               structure (see Note #1).
*
*               if_nbr      Interface number to search for configured IPv4 address.
*
* Return(s)   : Pointer to corresponding IP address structure, if IPv4 address configured on this interface.
*
*               Pointer to NULL,                                 otherwise.
*
* Caller(s)   : NetIPv4_GetAddrsHostCfgd(),
*               NetIPv4_RxPktValidate(),
*               NetIPv4_TxPktValidate(),
*               NetIPv4_TxPktDatagramRouteSel().
*
* Note(s)     : (1) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

static  NET_IPv4_ADDRS  *NetIPv4_GetAddrsHostCfgdOnIF (NET_IPv4_ADDR  addr,
                                                       NET_IF_NBR     if_nbr)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN        valid;
    NET_ERR            err;
#endif
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_BOOLEAN        addr_found;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE IF NBR ------------------ */
    valid = NetIF_IsValidCfgdHandler(if_nbr, &err);
    if (valid != DEF_YES) {
        return ((NET_IPv4_ADDRS *)0);
    }
#endif

                                                                /* ---------------- VALIDATE IPv4 ADDR ---------------- */
    if (addr == NET_IPv4_ADDR_NONE) {
        return ((NET_IPv4_ADDRS *)0);
    }


                                                                /* -------------- SRCH IF FOR IPv4 ADDR --------------- */
    addr_ix    =  0u;
    addr_found =  DEF_NO;
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
    p_ip_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];

    while ((addr_ix    <  p_ip_if_cfg->AddrsNbrCfgd) &&         /* Srch all cfg'd addrs ...                             */
           (addr_found == DEF_NO)) {                            /* ... until addr found.                                */

        if (p_ip_addrs->AddrHost != addr) {                     /* If addr NOT found, ...                               */
            p_ip_addrs++;                                       /* ... adv to IF's next addr.                           */
            addr_ix++;

        } else {
            addr_found = DEF_YES;
        }
    }

    if (addr_found != DEF_YES) {
        return ((NET_IPv4_ADDRS *)0);
    }


    return (p_ip_addrs);
}


/*
*********************************************************************************************************
*                                      NetIPv4_RxPktValidateBuf()
*
* Description : Validate received buffer header as IPv4 protocol.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received IPv4 packet.
*               ---------   Argument validated in NetIPv4_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Received buffer's IPv4 header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT IPv4.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIPv4_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
                                        NET_ERR      *p_err)
{
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
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvBufTypeCtr);
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

                                                                /* --------------- VALIDATE IPv4 BUF HDR -------------- */
    if (p_buf_hdr->ProtocolHdrType != NET_PROTOCOL_TYPE_IP_V4) {
        valid = NetIF_IsValidHandler(p_buf_hdr->IF_Nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvIF_Ctr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->IP_HdrIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_IPv4_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        NetIPv4_RxPktValidate()
*
* Description : (1) Validate received IPv4 packet :
*
*                   (a) (1) Validate the received packet's following IPv4 header fields :
*
*                           (A) Version
*                           (B) Header Length
*                           (C) Total  Length                               See Note #4
*                           (D) Flags
*                           (E) Fragment Offset
*                           (F) Protocol
*                           (G) Check-Sum                                   See Note #5
*                           (H) Source      Address                         See Note #9c
*                           (I) Destination Address                         See Note #9d
*                           (J) Options
*
*                       (2) Validation ignores the following IPv4 header fields :
*
*                           (A) Type of Service (TOS)
*                           (B) Identification  (ID)
*                           (C) Time-to-Live    (TTL)
*
*                   (b) Convert the following IPv4 header fields from network-order to host-order :
*
*                       (1) Total Length                                    See Notes #1bB1 & #3b
*                       (2) Identification (ID)                             See Note  #1bB2
*                       (3) Flags/Fragment Offset                           See Note  #1bB3
*                       (4) Check-Sum                                       See Note  #5d
*                       (5) Source      Address                             See Notes #1bB4 & #3c
*                       (6) Destination Address                             See Notes #1bB5 & #3d
*                       (7) All Options' multi-octet words                  See Notes #1bB6 & #1bC
*
*                           (A) These fields are NOT converted directly in the received packet buffer's
*                               data area but are converted in local or network buffer variables ONLY.
*
*                           (B) The following IPv4 header fields are converted & stored in network buffer
*                               variables :
*
*                               (1) Total Length
*                               (2) Identification (ID)
*                               (3) Flags/Fragment Offset
*                               (4) Source      Address
*                               (5) Destination Address
*                               (6) IPv4 Options' multi-octet words
*
*                           (C) Since any IPv4 packet may receive a number of various IPv4 options that may
*                               require conversion from network-order to host-order, IPv4 options are copied
*                               into a separate network buffer for validation, conversion, & demultiplexing.
*
*                   (c) Update network buffer's protocol controls
*
*                   (d) Process IPv4 packet in ICMP Receive Handler
*
*
* Argument(s) : p_buf       Pointer to network buffer that received IPv4 packet.
*               -----       Argument checked   in NetIPv4_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_Rx().
*
*               p_ip_hdr    Pointer to received packet's IPv4 header.
*               --------    Argument validated in NetIPv4_Rx()/NetIPv4_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                       Received packet validated.
*                               NET_IPv4_ERR_INVALID_VER                Invalid IPv4 version.
*                               NET_IPv4_ERR_INVALID_LEN_HDR            Invalid IPv4 header length.
*                               NET_IPv4_ERR_INVALID_LEN_TOT            Invalid IPv4 total  length.
*                               NET_IPv4_ERR_INVALID_LEN_DATA           Invalid IPv4 data   length.
*                               NET_IPv4_ERR_INVALID_FLAG               Invalid IPv4 flags.
*                               NET_IPv4_ERR_INVALID_FRAG               Invalid IPv4 fragmentation.
*                               NET_IPv4_ERR_INVALID_PROTOCOL           Invalid/unknown protocol type.
*                               NET_IPv4_ERR_INVALID_CHK_SUM            Invalid IPv4 check-sum.
*                               NET_IPv4_ERR_INVALID_ADDR_SRC           Invalid IPv4 source      address.
*                               NET_IPv4_ERR_INVALID_ADDR_DEST          Invalid IPv4 destination address.
*                               NET_IPv4_ERR_INVALID_ADDR_BROADCAST     Invalid IPv4 broadcast.
*
*                                                                       - RETURNED BY NetIPv4_RxPktValidateOpt() : -
*                               NET_IPv4_ERR_RX_OPT_BUF_NONE_AVAIL      No available buffers to process
*                                                                           IPv4 options.
*                               NET_IPv4_ERR_RX_OPT_BUF_LEN             Insufficient buffer length to write
*                                                                           IPv4 options to buffer.
*                               NET_IPv4_ERR_RX_OPT_BUF_WR              IPv4 options failed to write to buffer.
*                               NET_IPv4_ERR_INVALID_OPT                Invalid IPv4 option.
*                               NET_IPv4_ERR_INVALID_OPT_LEN            Invalid IPv4 option length.
*                               NET_IPv4_ERR_INVALID_OPT_NBR            Invalid IPv4 option number of same option.
*                               NET_IPv4_ERR_INVALID_OPT_END            Invalid IPv4 option list ending.
*                               NET_IPv4_ERR_INVALID_OPT_FLAG           Invalid IPv4 option flag.
*
*                                                                       --- RETURNED BY NetIF_IsEnHandler() : ----
*                               NET_IF_ERR_INVALID_IF                   Invalid OR disabled network interface.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_Rx().
*
* Note(s)     : (2) See 'net_ipv4.h  IP HEADER' for IPv4 header format.
*
*               (3) The following IPv4 header fields MUST be decoded &/or converted from network-order to host-order
*                   BEFORE any ICMP Error Messages are transmitted (see 'net_icmp.c  NetICMPv4_TxMsgErr()  Note #2') :
*
*                   (a) Header Length
*                   (b) Total  Length
*                   (c) Source      Address
*                   (d) Destination Address
*
*               (4) (a) In addition to validating that the IPv4 header Total Length is greater than or equal to the
*                       IPv4 header Header Length, the IPv4 total length SHOULD be compared to the remaining packet
*                       data length which should be identical.
*
*                   (b) (1) However, some network interfaces MAY append octets to their frames :
*
*                           (A) 'pad' octets, if the frame length does NOT meet the frame's required minimum size :
*
*                               (1) RFC #894, Section 'Frame Format' states that "the minimum length of  the data
*                                   field of a packet sent over an Ethernet is 46 octets.  If necessary, the data
*                                   field should be padded (with octets of zero) to meet the Ethernet minimum frame
*                                   size.  This padding is not part of the IPv4 packet and is not included in the
*                                   total length field of the IPv4 header".
*
*                               (2) RFC #1042, Section 'Frame Format and MAC Level Issues : For all hardware types'
*                                   states that "IEEE 802 packets may have a minimum size restriction.  When
*                                   necessary, the data field should be padded (with octets of zero) to meet the
*                                   IEEE 802 minimum frame size requirements.  This padding is not part of the IPv4
*                                   datagram and is not included in the total length field of the IPv4 header".
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
*                           remaining data length MAY be greater than the IPv4 total length :
*
*                           (A) Thus,    the IPv4 total length & the packet's remaining data length CANNOT be
*                               compared for equality.
*
*                               (1) Unfortunately, this eliminates the possibility to validate the IPv4 total
*                                   length to the packet's remaining data length.
*
*                           (B) And      the IPv4 total length MAY    be less    than the packet's remaining
*                               data length.
*
*                               (1) However, the packet's remaining data length MUST be reset to the IPv4
*                                   total length to correctly calculate higher-layer application data
*                                   length.
*
*                           (C) However, the IPv4 total length CANNOT be greater than the packet's remaining
*                               data length.
*
*               (5) (a) IPv4 header Check-Sum field MUST be validated BEFORE (or AFTER) any multi-octet words
*                       are converted from network-order to host-order since "the sum of 16-bit integers can
*                       be computed in either byte order" [RFC #1071, Section 2.(B)].
*
*                       In other words, the IPv4 header Check-Sum CANNOT be validated AFTER SOME but NOT ALL
*                       multi-octet words have been converted from network-order to host-order.
*
*                   (b) However, ALL received packets' multi-octet words are converted in local or network
*                       buffer variables ONLY (see Note #1bA).  Therefore, IPv4 header Check-Sum may be validated
*                       at any point.
*
*                   (c) For convenience, the IPv4 header Check-Sum is validated AFTER IPv4 Version, Header Length,
*                       & Total Length fields have been validated.  Thus, invalid IPv4 version or length packets
*                       are quickly discarded (see Notes #9a, #8a, & #8b) & the total IPv4 header length
*                       (in octets) will already be calculated for the IPv4 Check-Sum calculation.
*
*                   (d) After the IPv4 header Check-Sum is validated, it is NOT necessary to convert the Check-
*                       Sum from network-order to host-order since    it is NOT required for further processing.
*
*               (6) (a) RFC #791, Section 3.2 'Fragmentation and Reassembly' states that "if an internet datagram
*                       is fragmented" :
*
*                       (1) "Fragments are counted in units of 8 octets."
*                       (2) "The minimum fragment is 8 octets."
*
*                   (b) (1) However, this CANNOT apply "if this is the last fragment" ...
*                           (A) "(that is the more fragments field is zero)";         ...
*                       (2) Which may be of ANY size.
*
*                   See also 'net_ipv4.h  IPv4 FRAGMENTATION DEFINES  Note #1a'.
*
*               (7) (a) RFC #792, Section 'Destination Unreachable Message : Description' states that "if, in
*                       the destination host, the IPv4 module cannot deliver the datagram because the indicated
*                       protocol module ... is not active, the destination host may send a destination unreachable
*                       message to the source host".
*
*                   (b) Default case already invalidated earlier in this function.  However, the default case
*                       is included as an extra precaution in case 'Protocol' is incorrectly modified.
*
*               (8) ICMP Error Messages are sent if any of the following IP header fields are invalid :
*
*                   (a) Header Length                               ICMP 'Parameter   Problem'  Error Message
*                   (b) Total  Length                               ICMP 'Parameter   Problem'  Error Message
*                   (c) Flags                                       ICMP 'Parameter   Problem'  Error Message
*                   (d) Fragment Offset                             ICMP 'Parameter   Problem'  Error Message
*                   (e) Protocol                                    ICMP 'Unreachable Protocol' Error Message
*                   (f) Options                                     ICMP 'Parameter   Problem'  Error Messages
*                                                                        [see NetIPv4_RxPktValidateOpt()]
*
*               (9) RFC #1122, Section 3.2.1 requires that IPv4 packets with the following invalid IPv4 header
*                   fields be "silently discarded" :
*
*                   (a) Version                                             RFC #1122, Section 3.2.1.1
*                   (b) Check-Sum                                           RFC #1122, Section 3.2.1.2
*
*                   (c) Source Address
*
*                       (1) (A) RFC #1122, Section 3.2.1.3 states that "a host MUST silently discard
*                               an incoming datagram containing an IPv4 source address that is invalid
*                               by the rules of this section".
*
*                           (B) (1) MAY      be one of the following :
*                                   (a) Configured host address             RFC #1122, Section 3.2.1.3.(1)
*                                   (b) Localhost       address             RFC #1122, Section 3.2.1.3.(g)
*                                                                               See also Note #9c2A
*                                   (c) Link-local host address             RFC #3927, Section 2.1
*                                                                               See also Note #9c2B
*                                   (d) This       Host                     RFC #1122, Section 3.2.1.3.(a)
*                                   (e) Specified  Host                     RFC #1122, Section 3.2.1.3.(b)
*
*                               (2) MUST NOT be one of the following :
*                                   (a) Multicast  host address             RFC #1112, Section 7.2
*                                   (b) Limited    Broadcast                RFC #1122, Section 3.2.1.3.(c)
*                                   (c) Directed   Broadcast                RFC #1122, Section 3.2.1.3.(d)
*                                   (d) Subnet     Broadcast                RFC #1122, Section 3.2.1.3.(e)
*                                                                               See also Note #9c2C
*
*                       (2) (A) RFC #1122, Section 3.2.1.3.(g) states that the "internal host loopback
*                               address ... MUST NOT appear outside a host".
*
*                               (1) However, this does NOT prevent the host loopback address from being
*                                   used as an IPv4 packet's source address as long as BOTH the packet's
*                                   source AND destination addresses are internal host addresses, either
*                                   a configured host IP address or any host loopback address.
*
*                           (B) RFC #3927, Section 2.1 specifies the "IPv4 Link-Local address ... range
*                               ... [as] inclusive" ... :

*                               (1) "from 169.254.1.0" ...
*                               (2) "to   169.254.254.255".
*
*                           (C) Although received packets' IPv4 source addresses SHOULD be checked for
*                               invalid subnet broadcasts (see Note #9c1B2d), since multiple IPv4 host
*                               addresses MAY be configured on any single network interface & since
*                               each of these IPv4 host addresses may be configured on various networks
*                               with various subnet masks, it is NOT possible to absolutely determine
*                               if a received packet is a subnet broadcast for any specific network
*                               on the network interface.
*
*                   (d) Destination Address
*
*                       (1) (A) RFC #1122, Section 3.2.1.3 states that "a host MUST silently discard
*                               an incoming datagram that is not destined for" :
*
*                               (1) "(one of) the host's IPv4 address(es); or" ...
*                               (2) "an IPv4 broadcast address valid for the connected network; or"
*                               (3) "the address for a multicast group of which the host is a member
*                                    on the incoming physical interface."
*
*                           (B) (1) MUST     be one of the following :
*                                   (a) Configured host address             RFC #1122, Section 3.2.1.3.(1)
*                                   (b) Multicast  host address             RFC #1122, Section 3.2.1.3.(3)
*                                                                               See also Note #9d2A
*                                   (c) Localhost                           RFC #1122, Section 3.2.1.3.(g)
*                                                                               See also Note #9d2B
*                                   (d) Limited    Broadcast                RFC #1122, Section 3.2.1.3.(c)
*                                   (e) Directed   Broadcast                RFC #1122, Section 3.2.1.3.(d)
*                                   (f) Subnet     Broadcast                RFC #1122, Section 3.2.1.3.(e)
*
*                               (2) MUST NOT be one of the following :
*                                   (a) This       Host                     RFC #1122, Section 3.2.1.3.(a)
*                                   (b) Specified  Host                     RFC #1122, Section 3.2.1.3.(b)
*
*                       (2) (A) RFC #1122, Section 3.2.1.3 states that "for most purposes, a datagram
*                               addressed to a ... multicast destination is processed as if it had been
*                               addressed to one of the host's IP addresses".
*
*                           (B) RFC #1122, Section 3.2.1.3.(g) states that the "internal host loopback
*                               address ... MUST NOT appear outside a host".
*
*                           (C) RFC #3927, Section 2.8 states that "the 169.254/16 address prefix MUST
*                               NOT be subnetted".  Therefore, link-local broadcast packets may ONLY be
*                               received via directed broadcast (see Note #9d1B1e).
*
*                       (3) (A) RFC #1122, Section 3.3.6 states that :
*
*                               (1) "When a host sends a datagram to a link-layer broadcast address, the IP
*                                    destination address MUST be a legal IP broadcast or IP multicast address."
*
*                               (2) "A host SHOULD silently discard a datagram that is received via a link-
*                                    layer broadcast ... but does not specify an IP multicast or broadcast
*                                    destination address."
*
*                           (B) (1) Therefore, any packet received as ... :
*
*                                   (a) ... an IPv4 broadcast destination address MUST also have been received
*                                        as a link-layer broadcast.
*
*                                   (b) ... a link-layer broadcast MUST also be received as an IP broadcast or
*                                        as an IPv4 multicast.
*
*                               (2) Thus, the following packets MUST be silently discarded if received as ... :
*
*                                   (a) ... a link-layer broadcast but not as an IPv4 broadcast or multicast; ...
*                                   (b) ... a link-layer unicast   but     as an IPv4 broadcast.
*
*              (10) See 'net_ipv4.h  IPv4 ADDRESS DEFINES  Notes #2 & #3' for supported IPv4 addresses.
*
*              (11) (a) RFC #1122, Section 3.2.1.6 states that "the IP layer SHOULD pass received TOS values
*                       up to the transport layer".
*
*                       NOT currently implemented. #### NET-812
*
*                   (b) RFC #1122, Section 3.2.1.8 states that "all IP options ... received in datagrams MUST
*                       be passed to the transport layer (or to ICMP processing when the datagram is an ICMP
*                       message).  The IPv4 and transport layer MUST each interpret those IPv4 options that they
*                       understand and silently ignore the others".
*
*                       NOT currently implemented. #### NET-813
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktValidate (NET_BUF        *p_buf,
                                     NET_BUF_HDR    *p_buf_hdr,
                                     NET_IPv4_HDR   *p_ip_hdr,
                                     NET_ERR        *p_err)
{
#ifdef  NET_IGMP_MODULE_EN
    CPU_BOOLEAN        addr_host_grp_joined;
#endif
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDRS    *p_ip_addrs;
    NET_IP_ADDRS_QTY   addr_ix;
    NET_IF_NBR         if_nbr;
    CPU_INT08U         ip_ver;
    CPU_INT08U         ip_hdr_len;
    CPU_INT16U         ip_hdr_len_tot;
    CPU_INT16U         ip_flags;
    CPU_INT16U         ip_frag_offset;
    CPU_INT16U         protocol_ix;
    CPU_BOOLEAN        ip_flag_reserved;
    CPU_BOOLEAN        ip_flag_dont_frag;
    CPU_BOOLEAN        ip_flag_frags_more;
    CPU_BOOLEAN        ip_chk_sum_valid;
    CPU_BOOLEAN        addr_host_src;
    CPU_BOOLEAN        addr_host_dest;
    CPU_BOOLEAN        rx_remote_host;
    CPU_BOOLEAN        rx_broadcast;
    CPU_BOOLEAN        ip_broadcast;
    CPU_BOOLEAN        ip_multicast;
#ifdef  NET_ICMPv4_MODULE_EN
    NET_ERR            msg_err;
#endif


                                                                /* --------------- CONVERT IPv4 FIELDS ---------------- */
                                                                /* See Note #3.                                         */
    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->IP_TotLen,   &p_ip_hdr->TotLen);
    NET_UTIL_VAL_COPY_GET_NET_32(&p_buf_hdr->IP_AddrSrc,  &p_ip_hdr->AddrSrc);
    NET_UTIL_VAL_COPY_GET_NET_32(&p_buf_hdr->IP_AddrDest, &p_ip_hdr->AddrDest);


                                                                /* ---------------- VALIDATE IPv4 VER ----------------- */
    ip_ver   = p_ip_hdr->Ver_HdrLen & NET_IPv4_HDR_VER_MASK;    /* See 'net_ipv4.h  IPv4 HEADER  Note #2'.              */
    ip_ver >>= NET_IPv4_HDR_VER_SHIFT;
    if (ip_ver != NET_IPv4_HDR_VER) {                           /* Validate IP ver.                                     */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvVerCtr);
       *p_err = NET_IPv4_ERR_INVALID_VER;
        return;
    }


                                                                /* -------------- VALIDATE IPv4 HDR LEN --------------- */
                                                                /* See 'net_ipv4.h  IPv4 HEADER  Note #2'.              */
    ip_hdr_len          =  p_ip_hdr->Ver_HdrLen   & NET_IPv4_HDR_LEN_MASK;
    ip_hdr_len_tot      = (CPU_INT16U)ip_hdr_len * NET_IPv4_HDR_LEN_WORD_SIZE;
    p_buf_hdr->IP_HdrLen = (CPU_INT16U)ip_hdr_len_tot;          /* See Note #3a.                                        */

    if (ip_hdr_len < NET_IPv4_HDR_LEN_MIN) {                    /* If hdr len < min hdr len, rtn err.                   */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvLenCtr);
#ifdef  NET_ICMPv4_MODULE_EN
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_HDR_LEN,
                          &msg_err);
#endif
       *p_err = NET_IPv4_ERR_INVALID_LEN_HDR;
        return;
    }
#if ((NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED) & \
      defined(NET_ICMPv4_MODULE_EN))
    if (ip_hdr_len > NET_IPv4_HDR_LEN_MAX) {                    /* If hdr len > max hdr len, rtn err.                   */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvLenCtr);
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_HDR_LEN,
                          &msg_err);
       *p_err = NET_IPv4_ERR_INVALID_LEN_HDR;
        return;
    }
#endif



                                                                /* -------------- VALIDATE IPv4 TOT LEN --------------- */
#if 0                                                           /* See Note #3b.                                        */
    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->IP_TotLen, &p_ip_hdr->TotLen);
#endif
    if (p_buf_hdr->IP_TotLen < ip_hdr_len_tot) {                /* If IPv4 tot len < hdr len, rtn err.                  */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvTotLenCtr);
#ifdef  NET_ICMPv4_MODULE_EN
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_TOT_LEN,
                          &msg_err);
#endif
       *p_err = NET_IPv4_ERR_INVALID_LEN_TOT;
        return;
    }

    if (p_buf_hdr->IP_TotLen > p_buf_hdr->DataLen) {            /* If IPv4 tot len > rem pkt data len, ...              */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvTotLenCtr);
#ifdef  NET_ICMPv4_MODULE_EN
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_TOT_LEN,
                          &msg_err);
#endif
       *p_err = NET_IPv4_ERR_INVALID_LEN_TOT;                   /* ... rtn err (see Note #4b2C).                        */
        return;
    }

    p_buf_hdr->DataLen    = (NET_BUF_SIZE) p_buf_hdr->IP_TotLen;    /* Trunc data len to IP tot len (see Note #4b2B1).  */
    p_buf_hdr->IP_DataLen = (CPU_INT16U  )(p_buf_hdr->IP_TotLen - p_buf_hdr->IP_HdrLen);



                                                                /* --------------- VALIDATE IPv4 CHK SUM -------------- */
#ifdef  NET_IPV4_CHK_SUM_OFFLOAD_RX
    ip_chk_sum_valid = DEF_OK;
#else
                                                                /* See Note #5.                                         */
    ip_chk_sum_valid = NetUtil_16BitOnesCplChkSumHdrVerify((void     *)p_ip_hdr,
                                                           (CPU_INT16U)ip_hdr_len_tot,
                                                           (NET_ERR  *)p_err);
#endif
    if (ip_chk_sum_valid != DEF_OK) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvChkSumCtr);
       *p_err = NET_IPv4_ERR_INVALID_CHK_SUM;
        return;
    }
#if 0                                                           /* Conv to host-order NOT necessary (see Note #5d).     */
   (void)NET_UTIL_VAL_GET_NET_16(&p_ip_hdr->ChkSum);
#endif



                                                                /* ------------------ CONVERT IP ID ------------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->IP_ID, &p_ip_hdr->ID);


                                                                /* --------------- VALIDATE IPv4 FLAGS ---------------- */
                                                                /* See 'net_ipv4.h  IPv4 HEADER  Note #4'.              */
    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->IP_Flags_FragOffset, &p_ip_hdr->Flags_FragOffset);
    ip_flags         = p_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FLAG_MASK;
#if 1                                                           /* Allow invalid reserved flag for rx'd datagrams.      */
    ip_flag_reserved = DEF_BIT_IS_SET_ANY(ip_flags, NET_IPv4_HDR_FLAG_RESERVED);
    if (ip_flag_reserved != DEF_NO) {                           /* If reserved flag bit set, rtn err.                   */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvFlagsCtr);
#ifdef  NET_ICMPv4_MODULE_EN
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_FLAGS,
                          &msg_err);
#endif
       *p_err = NET_IPv4_ERR_INVALID_FLAG;
        return;
    }
#endif


                                                                /* ---------------- VALIDATE IPv4 FRAG ---------------- */
                                                                /* See 'net_ipv4.h  IPv4 HEADER  Note #4'.              */
    ip_flag_dont_frag  = DEF_BIT_IS_SET(ip_flags, NET_IPv4_HDR_FLAG_FRAG_DONT);
    ip_flag_frags_more = DEF_BIT_IS_SET(ip_flags, NET_IPv4_HDR_FLAG_FRAG_MORE);
    if (ip_flag_dont_frag != DEF_NO) {                          /* If  'Don't Frag' flag set & ...                      */
        if (ip_flag_frags_more != DEF_NO) {                     /* ... 'More Frags' flag set, rtn err.                  */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvFragCtr);
#ifdef  NET_ICMPv4_MODULE_EN
            NetICMPv4_TxMsgErr(p_buf,
                               NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                               NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                               NET_ICMPv4_PTR_IX_IP_FRAG_OFFSET,
                              &msg_err);
#endif
           *p_err = NET_IPv4_ERR_INVALID_FRAG;
            return;
        }

        ip_frag_offset = p_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FRAG_OFFSET_MASK;
        if (ip_frag_offset != NET_IPv4_HDR_FRAG_OFFSET_NONE) {  /* ... frag offset != 0,      rtn err.                  */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvFragCtr);
#ifdef  NET_ICMPv4_MODULE_EN
            NetICMPv4_TxMsgErr(p_buf,
                               NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                               NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                               NET_ICMPv4_PTR_IX_IP_FRAG_OFFSET,
                              &msg_err);
#endif
           *p_err = NET_IPv4_ERR_INVALID_FRAG;
            return;
        }
    }

    if (ip_flag_frags_more != DEF_NO) {                         /* If 'More Frags' set (see Note #6b1A)   ...           */
                                                                /* ... & IPv4 data len NOT multiple of    ...           */
        if ((p_buf_hdr->IP_DataLen % NET_IPv4_FRAG_SIZE_UNIT) != 0u) { /* ... frag size units (see Note #6a), rtn err.  */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvFragCtr);
#ifdef  NET_ICMPv4_MODULE_EN
            NetICMPv4_TxMsgErr(p_buf,
                               NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                               NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                               NET_ICMPv4_PTR_IX_IP_TOT_LEN,
                              &msg_err);
#endif
           *p_err = NET_IPv4_ERR_INVALID_FRAG;
            return;
        }
    }



                                                                /* -------------- VALIDATE IPv4 PROTOCOL -------------- */
    switch (p_ip_hdr->Protocol) {                               /* See 'net_ip.h  IP HEADER PROTOCOL FIELD DEFINES ...  */
        case NET_IP_HDR_PROTOCOL_ICMP:                          /* ... Note #1.                                         */
        case NET_IP_HDR_PROTOCOL_IGMP:
        case NET_IP_HDR_PROTOCOL_UDP:
        case NET_IP_HDR_PROTOCOL_TCP:
             break;


        default:                                                /* See Note #7a.                                        */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvProtocolCtr);
#ifdef  NET_ICMPv4_MODULE_EN
             NetICMPv4_TxMsgErr(p_buf,
                                NET_ICMPv4_MSG_TYPE_DEST_UNREACH,
                                NET_ICMPv4_MSG_CODE_DEST_PROTOCOL,
                                NET_ICMPv4_MSG_PTR_NONE,
                               &msg_err);
#endif
            *p_err = NET_IPv4_ERR_INVALID_PROTOCOL;
             return;
    }



                                                                    /* -------------- VALIDATE IP ADDRS --------------- */
#if 0                                                               /* See Notes #3c & #3d.                             */
    NET_UTIL_VAL_COPY_GET_NET_32(&p_buf_hdr->IP_AddrSrc,  &p_ip_hdr->AddrSrc);
    NET_UTIL_VAL_COPY_GET_NET_32(&p_buf_hdr->IP_AddrDest, &p_ip_hdr->AddrDest);
#endif

    if_nbr = p_buf_hdr->IF_Nbr;                                     /* Get pkt's rx'd IF.                               */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
   (void)NetIF_IsEnHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

                                                                    /* Chk pkt rx'd to cfg'd host addr.                 */
    if (if_nbr != NET_IF_NBR_LOCAL_HOST) {
        p_ip_addrs      =  NetIPv4_GetAddrsHostCfgdOnIF(p_buf_hdr->IP_AddrDest, if_nbr);
        addr_host_dest = (p_ip_addrs != (NET_IPv4_ADDRS *)0) ? DEF_YES : DEF_NO;
    } else {
        p_ip_addrs      = (NET_IPv4_ADDRS *)0;
        addr_host_dest =  NetIPv4_IsAddrHostCfgdHandler(p_buf_hdr->IP_AddrDest);
    }

                                                                    /* Chk pkt rx'd via local or remote host.           */
    rx_remote_host = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_REMOTE);
    if (((if_nbr  != NET_IF_NBR_LOCAL_HOST) && (rx_remote_host == DEF_NO)) ||
        ((if_nbr  == NET_IF_NBR_LOCAL_HOST) && (rx_remote_host != DEF_NO))) {
         NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
        *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
         return;
    }


                                                                /* -------------- VALIDATE IPv4 SRC ADDR -------------- */
                                                                /* See Note #9c.                                        */
    if (p_buf_hdr->IP_AddrSrc == NET_IPv4_ADDR_THIS_HOST) {     /* Chk 'This Host' addr        (see Note #9c1B1d).      */

                                                                /* Chk         localhost addrs (see Note #9c1B1b).      */
    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                                       NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
                                                                /* Chk invalid localhost addrs.                         */
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }

        if (rx_remote_host != DEF_NO) {                         /* If localhost addr rx'd via remote host, ...          */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;              /* ... rtn err / discard pkt   (see Note #9c2A1).       */
            return;
        }

                                                                /* Chk link-local addrs        (see Note #9c1B1c).      */
    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_LOCAL_LINK_MASK_NET) ==
                                       NET_IPv4_ADDR_LOCAL_LINK_NET     ) {
                                                                /* Chk invalid link-local addr (see Note #9c2B1).       */
        if ((p_buf_hdr->IP_AddrSrc < NET_IPv4_ADDR_LOCAL_LINK_HOST_MIN) ||
            (p_buf_hdr->IP_AddrSrc > NET_IPv4_ADDR_LOCAL_LINK_HOST_MAX)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }


                                                                /* Chk invalid lim'd broadcast (see Note #9c1B2b).      */
    } else if (p_buf_hdr->IP_AddrSrc == NET_IPv4_ADDR_BROADCAST) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
       *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
        return;


    } else if ((p_buf_hdr->IP_AddrSrc   & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
                                                                /* Chk Class-A broadcast       (see Note #9c1B2c).      */
        if ((p_buf_hdr->IP_AddrSrc      & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST   & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }

    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
                                                                /* Chk Class-B broadcast       (see Note #9c1B2c).      */
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }

    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
                                                                /* Chk Class-C broadcast       (see Note #9c1B2c).      */
        if ((p_buf_hdr->IP_AddrSrc    & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }

    } else if ((p_buf_hdr->IP_AddrSrc & NET_IPv4_ADDR_CLASS_D_MASK) == NET_IPv4_ADDR_CLASS_D) {
                                                                /* Chk Class-D multicast       (see Note #9c1B2a).      */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
       *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
        return;

    } else {                                                    /* Discard invalid addr class  (see Note #10).          */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
       *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
        return;
    }

                                                                /* Chk subnet  broadcast       (see Note #9c1B2d).      */
#if 0                                                           /* See Note #9c2C.                                      */
    if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {
        if ((p_buf_hdr->IP_AddrSrc & p_ip_addrs->AddrHostSubnetMask) ==
                                    p_ip_addrs->AddrHostSubnetNet ) {
            if ((p_buf_hdr->IP_AddrSrc    & p_ip_addrs->AddrHostSubnetMaskHost) ==
                (NET_IPv4_ADDR_BROADCAST & p_ip_addrs->AddrHostSubnetMaskHost)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvAddrSrcCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
                 return;
            }
        }
    }
#endif


                                                                    /* ------------- VALIDATE IP DEST ADDR ------------ */
                                                                    /* See Note #9d.                                    */
    ip_broadcast = DEF_NO;
    ip_multicast = DEF_NO;

                                                                    /* Chk this host's cfg'd addr    (see Note #9d1B1a).*/
    if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (p_ip_addrs->AddrHost == NET_IPv4_ADDR_NONE) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
            return;
        }
#endif
        if (p_buf_hdr->IP_AddrDest != p_ip_addrs->AddrHost) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
            return;
        }

                                                                    /* Chk this host's cfg'd addr(s) [see Note #9d1B1a].*/
    } else if (addr_host_dest == DEF_YES) {
        addr_host_src = NetIPv4_IsAddrHostHandler(p_buf_hdr->IP_AddrSrc);
        if (addr_host_src != DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
            return;
        }

                                                                    /* Chk         localhost         (see Note #9d1B1c).*/
    } else if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                                        NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
                                                                    /* Chk invalid localhost addrs.                     */
        if ((p_buf_hdr->IP_AddrDest    & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST  & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
             return;
        }

        if (rx_remote_host != DEF_NO) {                             /* If localhost addr rx'd via remote host, ...      */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;                 /* ... rtn err / discard pkt     (see Note #9d2B).  */
            return;
        }

        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxDestLocalHostCtr);

                                                                    /* Chk invalid 'This Host'       (see Note #9d1B2a).*/
    } else if (p_buf_hdr->IP_AddrDest == NET_IPv4_ADDR_THIS_HOST) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
       *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
        return;


#ifdef  NET_IGMP_MODULE_EN
                                                                    /* Chk joined multicast addr(s)  [see Note #9d1B1b].*/
    } else if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_CLASS_D_MASK) == NET_IPv4_ADDR_CLASS_D) {
        addr_host_grp_joined = NetIGMP_IsGrpJoinedOnIF(if_nbr, p_buf_hdr->IP_AddrDest);
        if (addr_host_grp_joined != DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
            return;
        }

        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxDestMcastCtr);
        ip_multicast = DEF_YES;
#endif


    } else {
                                                                    /* Chk lim'd broadcast           (see Note #9d1B1d).*/
        if (p_buf_hdr->IP_AddrDest == NET_IPv4_ADDR_BROADCAST) {
            ip_broadcast = DEF_YES;

        } else if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_LOCAL_LINK_MASK_NET) ==
                                            NET_IPv4_ADDR_LOCAL_LINK_NET     ) {
                                                                    /* Chk link-local broadcast      (see Note #9d2C).  */
            if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_LOCAL_LINK_MASK_HOST) !=
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_LOCAL_LINK_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }
            ip_broadcast = DEF_YES;


        } else if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
                                                                    /* Chk Class-A 'This Host'       (see Note #9d1B2b).*/
            if ((p_buf_hdr->IP_AddrDest   & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }
                                                                    /* Chk Class-A broadcast         (see Note #9d1B1e).*/
            if ((p_buf_hdr->IP_AddrDest   & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
                 ip_broadcast = DEF_YES;
            }

        } else if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
                                                                    /* Chk Class-B 'This Host'       (see Note #9d1B2b).*/
            if ((p_buf_hdr->IP_AddrDest    & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
                (NET_IPv4_ADDR_THIS_HOST  & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }
                                                                    /* Chk Class-B broadcast         (see Note #9d1B1e).*/
            if ((p_buf_hdr->IP_AddrDest   & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
                 ip_broadcast = DEF_YES;
            }

        } else if ((p_buf_hdr->IP_AddrDest & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
                                                                    /* Chk Class-C 'This Host'       (see Note #9d1B2b).*/
            if ((p_buf_hdr->IP_AddrDest    & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
                (NET_IPv4_ADDR_THIS_HOST  & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }
                                                                    /* Chk Class-C broadcast         (see Note #9d1B1e).*/
            if ((p_buf_hdr->IP_AddrDest   & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
                 ip_broadcast = DEF_YES;
            }

        } else {                                                    /* Discard invalid addr class    (see Note #10).    */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
            return;
        }

                                                                    /* Chk subnet broadcast          (see Note #9d1B1f).*/
        if (if_nbr != NET_IF_NBR_LOCAL_HOST) {                      /* If pkt rx'd via remote host, ...                 */
            p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];
            addr_ix    =  0u;
            p_ip_addrs  = &p_ip_if_cfg->AddrsTbl[addr_ix];
                                                                    /* ... chk broadcast on ALL cfg'd addrs on rx'd IF. */
            while ((addr_ix      <  p_ip_if_cfg->AddrsNbrCfgd) &&
                   (ip_broadcast == DEF_NO)) {
                if ((p_buf_hdr->IP_AddrDest & p_ip_addrs->AddrHostSubnetMask) ==
                                             p_ip_addrs->AddrHostSubnetNet ) {
                    if ((p_buf_hdr->IP_AddrDest   & p_ip_addrs->AddrHostSubnetMaskHost) ==
                        (NET_IPv4_ADDR_BROADCAST & p_ip_addrs->AddrHostSubnetMaskHost)) {
                         ip_broadcast = DEF_YES;
                    }
                }
                p_ip_addrs++;
                addr_ix++;
            }
        }

                                                                    /* If NOT any this host's addrs (see Note #9d1A1) & */
        if (ip_broadcast != DEF_YES) {                              /* .. NOT any broadcast   addrs (see Note #9d1A2);  */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;                 /* .. rtn err / discard pkt     (see Note #9d1A).   */
            return;
        }

        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxDestBcastCtr);
    }

                                                                    /* Chk invalid    broadcast     (see Note #9d3).    */
    rx_broadcast = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_BROADCAST);
    if (rx_broadcast == DEF_YES) {                                  /* If          IF   broadcast rx'd, ...             */
        if ((ip_broadcast != DEF_YES) &&                            /* ... BUT NOT IPv4 broadcast rx'd  ...             */
            (ip_multicast != DEF_YES)) {                            /* ... AND NOT IPv4 multicast rx'd; ...             */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestBcastCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_BROADCAST;           /* ... rtn err / discard pkt    (see Note #9d3B2a). */
             return;
        }
    } else {                                                        /* If      NOT IF   broadcast rx'd  ...             */
        if  (ip_broadcast == DEF_YES) {                             /* ... BUT     IPv4 broadcast rx'd, ...             */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxDestBcastCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_BROADCAST;           /* ... rtn err / discard pkt    (see Note #9d3B2b). */
             return;
        }
    }



                                                                /* ---------------- VALIDATE IPv4 OPTS ---------------- */
    if (ip_hdr_len_tot > NET_IPv4_HDR_SIZE_MIN) {               /* If hdr len > min, ...                                */
                                                                /* ... validate/process IPv4 opts (see Note #11b).      */
        NetIPv4_RxPktValidateOpt((NET_BUF      *)p_buf,
                                 (NET_BUF_HDR  *)p_buf_hdr,
                                 (NET_IPv4_HDR *)p_ip_hdr,
                                 (CPU_INT08U    )ip_hdr_len_tot,
                                 (NET_ERR      *)p_err);
        if (*p_err != NET_IPv4_ERR_NONE) {
             return;
        }
    }



                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
#if 0                                                           /* See Note #3a.                                        */
    p_buf_hdr->IP_HdrLen = ip_hdr_len_tot;
#endif
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf_hdr->IP_HdrLen > p_buf_hdr->DataLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvDataLenCtr);
       *p_err = NET_IPv4_ERR_INVALID_LEN_DATA;
        return;
    }
#endif
    p_buf_hdr->DataLen -= (NET_BUF_SIZE) p_buf_hdr->IP_HdrLen;
    protocol_ix        = (CPU_INT16U  )(p_buf_hdr->IP_HdrIx + p_buf_hdr->IP_HdrLen);
    switch (p_ip_hdr->Protocol) {
        case NET_IP_HDR_PROTOCOL_ICMP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_ICMP_V4;
             p_buf_hdr->ProtocolHdrTypeNetSub    = NET_PROTOCOL_TYPE_ICMP_V4;
             p_buf_hdr->ICMP_MsgIx               = protocol_ix;
             break;


#ifdef  NET_IGMP_MODULE_EN
        case NET_IP_HDR_PROTOCOL_IGMP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_IGMP;
             p_buf_hdr->ProtocolHdrTypeNetSub    = NET_PROTOCOL_TYPE_IGMP;
             p_buf_hdr->IGMP_MsgIx               = protocol_ix;
             break;
#endif


        case NET_IP_HDR_PROTOCOL_UDP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_UDP_V4;
             p_buf_hdr->ProtocolHdrTypeTransport = NET_PROTOCOL_TYPE_UDP_V4;
             p_buf_hdr->TransportHdrIx            = protocol_ix;
             break;


#ifdef  NET_TCP_MODULE_EN
        case NET_IP_HDR_PROTOCOL_TCP:
             p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_TCP_V4;
             p_buf_hdr->ProtocolHdrTypeTransport = NET_PROTOCOL_TYPE_TCP_V4;
             p_buf_hdr->TransportHdrIx            = protocol_ix;
             break;
#endif

        default:                                                /* See Note #7b.                                        */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvProtocolCtr);
            *p_err = NET_IPv4_ERR_INVALID_PROTOCOL;
             return;
    }



   *p_err = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetIPv4_RxPktValidateOpt()
*
* Description : (1) Validate & process received packet's IPv4 options :
*
*                   (a) Copy            IPv4 options into new buffer    See 'NetIPv4_RxPktValidate()  Note #1bC'
*                   (b) Decode/validate IPv4 options
*
*
* Argument(s) : p_buf               Pointer to network buffer that received IPv4 packet.
*               -----               Argument checked   in NetIPv4_Rx().
*
*               p_buf_hdr           Pointer to network buffer header.
*               ----------          Argument validated in NetIPv4_Rx().
*
*               p_ip_hdr            Pointer to received packet's IPv4 header.
*               -------             Argument validated in NetIPv4_Rx()/NetIPv4_RxPktValidateBuf().
*
*               ip_hdr_len_size     Length  of received packet's IPv4 header.
*               ----------------    Argument validated in NetIPv4_RxPktValidate().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                       IPv4 options validated & processed.
*                               NET_IPv4_ERR_RX_OPT_BUF_NONE_AVAIL      No available buffers       to process
*                                                                           IPv4 options.
*                               NET_IPv4_ERR_RX_OPT_BUF_LEN             Insufficient buffer length to write
*                                                                           IPv4 options to buffer.
*                               NET_IPv4_ERR_RX_OPT_BUF_WR              IP options failed to write to buffer.
*                               NET_IPv4_ERR_INVALID_OPT                Invalid IPv4 option.
*                               NET_IPv4_ERR_INVALID_OPT_LEN            Invalid IPv4 option length.
*                               NET_IPv4_ERR_INVALID_OPT_NBR            Invalid IPv4 option number of same option.
*                               NET_IPv4_ERR_INVALID_OPT_END            Invalid IPv4 option list ending.
*                               NET_IPv4_ERR_INVALID_OPT_FLAG           Invalid IPv4 option flag.
*
*                                                                     ----- RETURNED BY NetIF_Get() : ------
*                               NET_IF_ERR_INVALID_IF                 Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktValidate().
*
* Note(s)     : (2) (a) See 'net_ipv4.h  IPv4 HEADER OPTIONS DEFINES' for   supported IPv4 options' summary.
*
*                   (b) See 'net_ipv4.c  Note #1d'                    for unsupported IPv4 options.
*
*               (3) RFC #1122, Section 3.2.1.8 lists the processing of the following IPv4 options as optional :
*
*                   (a) Record Route                            RFC #1122, Section 3.2.1.8.(d)
*                   (b) Internet Timestamp                      RFC #1122, Section 3.2.1.8.(e)
*
*               (4) Each option's length MUST be multiples of NET_IP_HDR_OPT_SIZE_WORD octets so that "the
*                   beginning of a subsequent option [aligns] on a 32-bit boundary" (RFC #791, Section 3.1
*                   'Options : No Operation').
*
*               (5) RFC #1122, Section 3.2.1.8.(c) prohibits "an IP header" from transmitting with "more
*                   than one Source Route option".
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktValidateOpt (NET_BUF        *p_buf,
                                        NET_BUF_HDR    *p_buf_hdr,
                                        NET_IPv4_HDR   *p_ip_hdr,
                                        CPU_INT08U      ip_hdr_len_size,
                                        NET_ERR        *p_err)
{
    NET_IF        *p_if               = DEF_NULL;
    NET_BUF       *p_opt_buf          = DEF_NULL;
    NET_BUF_HDR   *p_opt_buf_hdr      = DEF_NULL;
    CPU_INT08U    *p_opts             = DEF_NULL;
    CPU_INT08U     opt_list_len_size  = 0u;
    CPU_INT08U     opt_list_len_rem   = 0u;
    CPU_INT08U     opt_len            = 0u;
    CPU_INT08U     opt_nbr_src_routes = 0u;
#ifdef  NET_ICMPv4_MODULE_EN
    CPU_INT08U     opt_ix_err         = 0u;
#endif
    CPU_INT16U     opt_ix             = 0u;
    NET_BUF_SIZE   opt_buf_size       = 0u;
    CPU_BOOLEAN    opt_err            = DEF_NO;
    CPU_BOOLEAN    opt_list_end       = DEF_NO;
    NET_IF_NBR     if_nbr             = NET_IF_NBR_NONE;
    NET_ERR        err                = NET_IPv4_ERR_NONE;
    NET_ERR        err_rtn            = NET_IPv4_ERR_NONE;



    opt_list_len_size = ip_hdr_len_size - NET_IPv4_HDR_SIZE_MIN;/* Calc opt list len size.                              */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------- VALIDATE IPv4 HDR OPT LIST SIZE --------- */
#ifdef  NET_ICMPv4_MODULE_EN
    if (opt_list_len_size > NET_IPv4_HDR_OPT_SIZE_MAX) {        /* If tot opt len > max opt size,           rtn err.    */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvOptsCtr);
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_OPTS,
                          &err);
       *p_err = NET_IPv4_ERR_INVALID_OPT_LEN;
        return;
    }
                                                                /* If tot opt len NOT multiple of opt size, rtn err.    */
    if ((opt_list_len_size % NET_IPv4_HDR_OPT_SIZE_WORD) != 0u) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvOptsCtr);
        NetICMPv4_TxMsgErr(p_buf,
                           NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                           NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                           NET_ICMPv4_PTR_IX_IP_OPTS,
                          &err);
       *p_err = NET_IPv4_ERR_INVALID_OPT_LEN;
        return;
    }
#endif
#endif


                                                                /* ------------- COPY IPv4 OPTS INTO BUF -------------- */
    if_nbr = p_buf_hdr->IF_Nbr;
    p_if   = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                /* Get IPv4 opt rx buf.                                 */
    opt_ix    = NET_IPv4_OPT_IX_RX;
    p_opt_buf = NetBuf_Get(if_nbr,
                           NET_TRANSACTION_RX,
                           opt_list_len_size,
                           opt_ix,
                           0,
                           NET_BUF_FLAG_NONE,
                          &err);
    if (err != NET_BUF_ERR_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxOptsBufNoneAvailCtr);
       *p_err = NET_IPv4_ERR_RX_OPT_BUF_NONE_AVAIL;
        return;
    }

                                                                /* Get IPv4 opt rx buf data area.                       */
    p_opt_buf->DataPtr = NetBuf_GetDataPtr(p_if,
                                           NET_TRANSACTION_RX,
                                           opt_list_len_size,
                                           opt_ix,
                                           0,
                                          &opt_buf_size,
                                           0,
                                          &err);
    if (err != NET_BUF_ERR_NONE) {
        NetBuf_Free(p_opt_buf);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxOptsBufNoneAvailCtr);
       *p_err = NET_IPv4_ERR_RX_OPT_BUF_NONE_AVAIL;
        return;
    }
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (opt_list_len_size > opt_buf_size) {
        NetBuf_Free(p_opt_buf);
       *p_err = NET_IPv4_ERR_RX_OPT_BUF_LEN;
        return;
    }
#else
   (void)&opt_buf_size;                                         /* Prevent 'variable unused' compiler warning.          */
#endif


    NetBuf_DataWr((NET_BUF    *) p_opt_buf,                     /* Copy IP opts from rx'd pkt to IP opt buf.            */
                  (NET_BUF_SIZE) opt_ix,
                  (NET_BUF_SIZE) opt_list_len_size,
                  (CPU_INT08U *)&p_ip_hdr->Opts[0],
                  (NET_ERR    *)&err);
    if ( err != NET_BUF_ERR_NONE) {
        NetBuf_Free(p_opt_buf);
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxOptsBufWrCtr);
       *p_err  = NET_IPv4_ERR_RX_OPT_BUF_WR;
        return;
    }
                                                                /* Init IPv4 opt buf ctrls.                             */
    p_buf_hdr->IP_OptPtr           = (NET_BUF     *) p_opt_buf;
    p_opt_buf_hdr                  = (NET_BUF_HDR *)&p_opt_buf->Hdr;
    p_opt_buf_hdr->IP_HdrIx        = (CPU_INT16U   ) opt_ix;
    p_opt_buf_hdr->IP_HdrLen       = (CPU_INT16U   ) opt_list_len_size;
    p_opt_buf_hdr->TotLen          = (NET_BUF_SIZE ) p_opt_buf_hdr->IP_HdrLen;
    p_opt_buf_hdr->ProtocolHdrType =  NET_PROTOCOL_TYPE_IP_V4_OPT;


                                                                /* ------------ DECODE/VALIDATE IPv4 OPTS ------------- */
    opt_err            =  DEF_NO;
    opt_list_end       =  DEF_NO;
    opt_nbr_src_routes =  0u;

    p_opts             = (CPU_INT08U *)&p_opt_buf->DataPtr[opt_ix];
    opt_list_len_rem   =  opt_list_len_size;

    while (opt_list_len_rem > 0) {                              /* Process each opt in list (see Notes #2 & #4).        */
        switch (*p_opts) {
            case NET_IPv4_HDR_OPT_END_LIST:                     /* ------------------- END OPT LIST ------------------- */
                 opt_list_end = DEF_YES;                        /* Mark end of opt list.                                */
                 opt_len      = NET_IPv4_HDR_OPT_SIZE_WORD;
                 break;


            case NET_IPv4_HDR_OPT_NOP:                          /* --------------------- NOP OPT ---------------------- */
                 opt_len      = NET_IPv4_HDR_OPT_SIZE_WORD;
                 break;


            case NET_IPv4_HDR_OPT_ROUTE_SRC_LOOSE:              /* ---------------- SRC/REC ROUTE OPTS ---------------- */
            case NET_IPv4_HDR_OPT_ROUTE_SRC_STRICT:
            case NET_IPv4_HDR_OPT_ROUTE_REC:
                 if (opt_list_end == DEF_NO) {
                     if (opt_nbr_src_routes < 1) {
                         opt_nbr_src_routes++;
                         opt_err = NetIPv4_RxPktValidateOptRoute(p_buf_hdr, p_opts, opt_list_len_rem, &opt_len, &err_rtn);

                     } else {                                   /* If > 1 src route opt, rtn err (see Note #5).         */
                         err_rtn = NET_IPv4_ERR_INVALID_OPT_NBR;
                         opt_err = DEF_YES;
                     }
                 } else {                                       /* If opt found AFTER end of opt list, rtn err.         */
                     err_rtn = NET_IPv4_ERR_INVALID_OPT_END;
                     opt_err = DEF_YES;
                 }
                 break;


            case NET_IPv4_HDR_OPT_TS:                           /* --------------------- TS OPTS ---------------------- */
                 if (opt_list_end == DEF_NO) {
                     opt_err = NetIPv4_RxPktValidateOptTS(p_buf_hdr, p_opts, opt_list_len_rem, &opt_len, &err_rtn);

                 } else {                                       /* If opt found AFTER end of opt list, rtn err.         */
                     err_rtn = NET_IPv4_ERR_INVALID_OPT_END;
                     opt_err = DEF_YES;
                 }
                 break;
                                                                /* --------------- UNSUPPORTED IP OPTS ---------------- */
                                                                /* See Note #2b.                                        */
            case NET_IPv4_HDR_OPT_SECURITY:
            case NET_IPv4_HDR_OPT_SECURITY_EXTENDED:
                 opt_len = *(p_opts + 1);
                 err_rtn = NET_IPv4_ERR_INVALID_OPT;
                 opt_err = DEF_YES;
                                                                /* Min len is 3 octets. See RFC 1108 Sections 2.2 & 3.2.*/
                 if (opt_len < NET_IPv4_HDR_OPT_SIZE_SECURITY) {
                     err_rtn = NET_IPv4_ERR_INVALID_OPT_LEN;
                 }
                 break;


            default:                                            /* ------------------- INVALID OPTS ------------------- */
                 opt_err = DEF_YES;
                 err_rtn = NET_IPv4_ERR_INVALID_OPT;            /* Ignore unknown opts.                                 */
                 break;
        }

        if (opt_err == DEF_NO) {
            if (opt_list_len_rem >= opt_len) {
                opt_list_len_rem -= opt_len;
                p_opts           += opt_len;
            } else {                                              /* If rem opt list len NOT multiple of opt size, ...  */
                err_rtn           = NET_IPv4_ERR_INVALID_OPT_LEN; /* ... rtn err.                                       */
                opt_err           = DEF_YES;
            }
        }

        if (opt_err != DEF_NO) {                                  /* If ANY opt errs, tx ICMP err msg.                  */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvOptsCtr);
#ifdef  NET_ICMPv4_MODULE_EN
            opt_ix_err = NET_ICMPv4_PTR_IX_IP_OPTS + (opt_list_len_size - opt_list_len_rem);
            NetICMPv4_TxMsgErr(p_buf,
                               NET_ICMPv4_MSG_TYPE_PARAM_PROB,
                               NET_ICMPv4_MSG_CODE_PARAM_PROB_IP_HDR,
                               opt_ix_err,
                              &err);
#endif
           *p_err = err_rtn;

            return;
        }
    }


   *p_err = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetIPv4_RxPktValidateOptRoute()
*
* Description : (1) Validate & process Source Route options :
*
*                   (a) Convert ALL Source Route IPv4 addresses from network-order to host-order
*                   (b) Add this host's IPv4 address to Source Route
*
*               (2) See 'net_ipv4.h  IPv4 SOURCE ROUTE OPTION DATA TYPE' for Source Route options summary.
*
*
* Argument(s) : p_buf_hdr           Pointer to network buffer header.
*               ---------           Argument validated in NetIPv4_Rx().
*
*               p_opts              Pointer to Source Route option.
*               -----               Argument validated in NetIPv4_RxPktValidateOpt().
*
*               opt_list_len_rem    Remaining option list length (in octets).
*
*               p_opt_len           Pointer to variable that will receive the Source Route option length
*               ---------               (in octets).
*
*                                   Argument validated in NetIPv4_RxPktValidateOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Source Route IPv4 option validated & processed.
*                               NET_IPv4_ERR_INVALID_OPT_LEN    Invalid IPv4 option length.
*
* Return(s)   : DEF_NO,  NO Source Route option error.
*
*               DEF_YES, otherwise.
*
* Caller(s)   : NetIPv4_RxPktValidateOpt().
*
* Note(s)     : (3) If Source Route option appends this host's IPv4 address to the source route, the IPv4
*                   header check-sum is NOT re-calculated since the check-sum was previously validated
*                   in NetIPv4_RxPktValidate() & is NOT required for further validation or processing.
*
*               (4) Default case already invalidated earlier in this function.  However, the default
*                   case is included as an extra precaution in case any of the IPv4 receive options is
*                   incorrectly modified.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetIPv4_RxPktValidateOptRoute (NET_BUF_HDR  *p_buf_hdr,
                                                    CPU_INT08U   *p_opts,
                                                    CPU_INT08U    opt_list_len_rem,
                                                    CPU_INT08U   *p_opt_len,
                                                    NET_ERR      *p_err)
{
    NET_IPv4_OPT_SRC_ROUTE  *p_opt_route;
    NET_IPv4_ADDR            opt_addr;
    CPU_INT08U               opt_ptr;
    CPU_INT08U               opt_ix;


    p_opt_route = (NET_IPv4_OPT_SRC_ROUTE *)p_opts;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE OPT TYPE ---------------- */
    switch (p_opt_route->Type) {
        case NET_IPv4_HDR_OPT_ROUTE_SRC_LOOSE:
        case NET_IPv4_HDR_OPT_ROUTE_SRC_STRICT:
        case NET_IPv4_HDR_OPT_ROUTE_REC:
             break;


        default:
            *p_err =  NET_IPv4_ERR_INVALID_OPT;
             return (DEF_YES);
    }
#endif

                                                                /* ----------------- VALIDATE OPT LEN ----------------- */
    if (p_opt_route->Ptr > p_opt_route->Len) {                  /* If ptr exceeds opt len,              rtn err.        */
       *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
        return (DEF_YES);
    }

    if (p_opt_route->Len > opt_list_len_rem) {                  /* If opt len exceeds rem opt len,      rtn err.        */
       *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
        return (DEF_YES);
    }

    if ((p_opt_route->Len % NET_IPv4_HDR_OPT_SIZE_WORD) != 0u) {    /* If opt len NOT multiple of opt size, rtn err.    */
       *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
        return (DEF_YES);
    }


    opt_ptr = NET_IPv4_OPT_ROUTE_PTR_ROUTE;
    opt_ix  = 0u;

                                                                /* --------------- CONVERT TO HOST-ORDER -------------- */
    while (opt_ptr < p_opt_route->Ptr) {                        /* Convert ALL src route addrs to host-order.           */
        NET_UTIL_VAL_COPY_GET_NET_32(&opt_addr, &p_opt_route->Route[opt_ix]);
        NET_UTIL_VAL_COPY_32(&p_opt_route->Route[opt_ix], &opt_addr);
        opt_ptr += NET_IPv4_HDR_OPT_SIZE_WORD;
        opt_ix++;
    }

                                                                /* ------------------- INSERT ROUTE ------------------- */
    if (p_opt_route->Ptr < p_opt_route->Len) {                  /* If ptr < len, append this host addr to src route.    */
        switch (*p_opts) {
            case NET_IPv4_HDR_OPT_ROUTE_SRC_LOOSE:
            case NET_IPv4_HDR_OPT_ROUTE_REC:
                 opt_addr         = p_buf_hdr->IP_AddrDest;
                 NET_UTIL_VAL_COPY_SET_HOST_32(&p_opt_route->Route[opt_ix], &opt_addr);
                 p_opt_route->Ptr += NET_IPv4_HDR_OPT_SIZE_WORD;
                 break;


            case NET_IPv4_HDR_OPT_ROUTE_SRC_STRICT:
                 break;


            default:                                            /* See Note #4.                                         */
                *p_err =  NET_IPv4_ERR_INVALID_OPT;
                 return (DEF_YES);
        }
    }


   *p_opt_len = p_opt_route->Len;                               /* Rtn src route opt len.                               */
   *p_err     = NET_IPv4_ERR_NONE;

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                     NetIPv4_RxPktValidateOptTS()
*
* Description : (1) Validate & process Internet Timestamp options :
*
*                   (a) Convert ALL Internet Timestamps & Source Route IPv4 addresses from network-order
*                           to host-order
*                   (b) Add current Internet Timestamp & this host's   IPv4 address to Internet Timestamp
*
*               (2) See 'net_ipv4.h  IPv4 INTERNET TIMESTAMP OPTION DATA TYPE' for Internet Timestamp options
*                       summary.
*
*
* Argument(s) : p_buf_hdr           Pointer to network buffer header.
*               ---------           Argument validated in NetIPv4_Rx().
*
*               p_opts              Pointer to Internet Timestamp option.
*               -----               Argument validated in NetIPv4_RxPktValidateOpt().
*
*               opt_list_len_rem    Remaining option list length (in octets).
*
*               p_opt_len           Pointer to variable that will return the Internet Timestamp option length
*               ---------               (in octets).
*
*                                   Argument validated in NetIPv4_RxPktValidateOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Internet Timestamp IPv4 option validated
*                                                                   & processed.
*                               NET_IPv4_ERR_INVALID_OPT_LEN    Invalid IPv4 option length.
*
* Return(s)   : DEF_NO,  NO Internet Timestamp option error.
*
*               DEF_YES, otherwise.
*
* Caller(s)   : NetIPv4_RxPktValidateOpt().
*
* Note(s)     : (3) If Internet Timestamp option appends the current Internet Timestamp &/or this host's
*                   IP address to the Internet Timestamp, the IPv4 header check-sum is NOT re-calculated
*                   since the check-sum was previously validated in NetIPv4_RxPktValidate() & is NOT
*                   required for further validation or processing.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetIPv4_RxPktValidateOptTS (NET_BUF_HDR  *p_buf_hdr,
                                                 CPU_INT08U   *p_opts,
                                                 CPU_INT08U    opt_list_len_rem,
                                                 CPU_INT08U   *p_opt_len,
                                                 NET_ERR      *p_err)
{
    NET_IPv4_OPT_TS        *p_opt_ts;
    NET_IPv4_OPT_TS_ROUTE  *p_opt_ts_route;
    NET_IPv4_ROUTE_TS      *p_route_ts;
    CPU_INT08U              opt_ptr;
    CPU_INT08U              opt_ix;
    CPU_INT08U              opt_ts_flags;
    CPU_INT08U              opt_ts_ovf;
    NET_TS                  opt_ts;
    NET_IPv4_ADDR           opt_addr;


    p_opt_ts = (NET_IPv4_OPT_TS *)p_opts;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ----------------- VALIDATE OPT TYPE ---------------- */
    switch (p_opt_ts->Type) {
        case NET_IPv4_HDR_OPT_TS:
             break;


        default:
            *p_err =  NET_IPv4_ERR_INVALID_OPT;
             return (DEF_YES);
    }
#endif

                                                                /* ----------------- VALIDATE OPT LEN ----------------- */
    if (p_opt_ts->Ptr > p_opt_ts->Len) {                        /* If ptr exceeds opt len,         rtn err.             */
       *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
        return (DEF_YES);
    }

    if (p_opt_ts->Len > opt_list_len_rem) {                     /* If opt len exceeds rem opt len, rtn err.             */
       *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
        return (DEF_YES);
    }


    opt_ptr      = NET_IPv4_OPT_TS_PTR_TS;
    opt_ix       = 0u;
    opt_ts_flags = p_opt_ts->Ovf_Flags & NET_IPv4_OPT_TS_FLAG_MASK;

    switch (opt_ts_flags) {
        case NET_IPv4_OPT_TS_FLAG_TS_ONLY:
             if ((p_opt_ts->Len % NET_IPv4_HDR_OPT_SIZE_WORD) != 0u) { /* If opt len NOT multiple of opt size, rtn err. */
                *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
                 return (DEF_YES);
             }

                                                                /* --------------- CONVERT TO HOST-ORDER -------------- */
             while (opt_ptr < p_opt_ts->Ptr) {                  /* Convert ALL TS's to host-order.                      */
                 NET_UTIL_VAL_COPY_GET_NET_32(&opt_ts, &p_opt_ts->TS[opt_ix]);
                 NET_UTIL_VAL_COPY_32(&p_opt_ts->TS[opt_ix], &opt_ts);
                 opt_ptr += NET_IPv4_HDR_OPT_SIZE_WORD;
                 opt_ix++;
             }

                                                                /* --------------------- INSERT TS -------------------- */
             if (p_opt_ts->Ptr < p_opt_ts->Len) {               /* If ptr < len, append ts to list.                     */
                 opt_ts        = NetUtil_TS_Get();
                 NET_UTIL_VAL_COPY_SET_HOST_32(&p_opt_ts->TS[opt_ix], &opt_ts);
                 p_opt_ts->Ptr += NET_IPv4_HDR_OPT_SIZE_WORD;

             } else {                                           /* Else inc & chk ovf ctr.                              */
                 opt_ts_ovf    = p_opt_ts->Ovf_Flags & NET_IPv4_OPT_TS_OVF_MASK;
                 opt_ts_ovf  >>= NET_IPv4_OPT_TS_OVF_SHIFT;
                 opt_ts_ovf++;

                 if (opt_ts_ovf > NET_IPv4_OPT_TS_OVF_MAX) {    /* If ovf ctr ovfs, rtn err.                            */
                    *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
                     return (DEF_YES);
                 }
             }
             break;


        case NET_IPv4_OPT_TS_FLAG_TS_ROUTE_REC:
        case NET_IPv4_OPT_TS_FLAG_TS_ROUTE_SPEC:
                                                                /* If opt len NOT multiple of opt size, rtn err.        */
             if ((p_opt_ts->Len % NET_IPv4_OPT_TS_ROUTE_SIZE) != NET_IPv4_HDR_OPT_SIZE_WORD) {
                *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
                 return (DEF_YES);
             }

             p_opt_ts_route = (NET_IPv4_OPT_TS_ROUTE *)p_opts;
             p_route_ts     = &p_opt_ts_route->Route_TS[0];

                                                                /* --------------- CONVERT TO HOST-ORDER -------------- */
             while (opt_ptr < p_opt_ts_route->Ptr) {            /* Convert ALL src route addrs & ts's to host-order.    */
                 NET_UTIL_VAL_COPY_GET_NET_32(&opt_addr, &p_route_ts->Route[opt_ix]);
                 NET_UTIL_VAL_COPY_GET_NET_32(&opt_ts,   &p_route_ts->TS[opt_ix]);
                 NET_UTIL_VAL_COPY_32(&p_route_ts->Route[opt_ix], &opt_addr);
                 NET_UTIL_VAL_COPY_32(&p_route_ts->TS[opt_ix],    &opt_ts);
                 opt_ptr += NET_IPv4_OPT_TS_ROUTE_SIZE;
                 opt_ix++;
             }

                                                                /* ---------------- INSERT SRC ROUTE/TS --------------- */
             if (p_opt_ts_route->Ptr < p_opt_ts_route->Len) {   /* If ptr < len, append src route addr & ts to list.    */
                 opt_addr            = p_buf_hdr->IP_AddrDest;
                 opt_ts              = NetUtil_TS_Get();
                 NET_UTIL_VAL_COPY_SET_HOST_32(&p_route_ts->Route[opt_ix], &opt_addr);
                 NET_UTIL_VAL_COPY_SET_HOST_32(&p_route_ts->TS[opt_ix],    &opt_ts);
                 p_opt_ts_route->Ptr += NET_IPv4_OPT_TS_ROUTE_SIZE;

             } else {                                           /* Else inc & chk ovf ctr.                              */
                 opt_ts_ovf   = p_opt_ts->Ovf_Flags & NET_IPv4_OPT_TS_OVF_MASK;
                 opt_ts_ovf >>= NET_IPv4_OPT_TS_OVF_SHIFT;
                 opt_ts_ovf++;

                 if (opt_ts_ovf > NET_IPv4_OPT_TS_OVF_MAX) {    /* If ovf ctr ovfs, rtn err.                            */
                    *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
                     return (DEF_YES);
                 }
             }
             break;


        default:                                                /* If invalid/unknown TS flag, rtn err.                 */
            *p_err =  NET_IPv4_ERR_INVALID_OPT_FLAG;
             return (DEF_YES);
    }


   *p_opt_len = p_opt_ts->Len;                                  /* Rtn TS opt len.                                      */
   *p_err     = NET_IPv4_ERR_NONE;

    return (DEF_NO);
}


/*
*********************************************************************************************************
*                                       NetIPv4_RxPktFragReasm()
*
* Description : (1) Reassemble any IPv4 datagram fragments :
*
*                   (a) Determine if received IPv4 packet is a fragment
*                   (b) Reassemble IPv4 fragments, when possible
*
*               (2) (a) Received fragments are reassembled by sorting datagram fragments into fragment lists
*                       (also known as 'Fragmented Datagrams') grouped by the following IPv4 header fields :
*
*                       (1) Source      Address
*                       (2) Destination Address
*                       (3) Identification
*                       (4) Protocol
*
*                       See also Note #3a.
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
*                           (C) (1) 'NetIPv4_FragReasmListsHead' points to the head of the Fragment Lists;
*                               (2) 'NetIPv4_FragReasmListsTail' points to the tail of the Fragment Lists.
*
*                           (D) Fragment buffers' 'PrevPrimListPtr' & 'NextPrimListPtr' doubly-link each fragment
*                               list's head buffer to form the list of Fragment Lists.
*
*                           (E) Fragment buffer's 'PrevBufPtr'      & 'NextBufPtr'      doubly-link each fragment
*                               in a fragment list.
*
*                       (2) (A) For each received fragment, all fragment lists are searched in order to insert the
*                               fragment into the appropriate fragment list--i.e. the fragment list with identical
*                               fragment list IPv4 header field values (see Note #2a).
*
*                           (B) If a received fragment is the first fragment with its specific fragment list IPv4
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
* Argument(s) : p_buf       Pointer to network buffer that received IPv4 packet.
*               -----       Argument checked   in NetIPv4_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_Rx().
*
*               p_ip_hdr    Pointer to received packet's IPv4 header.
*               --------    Argument checked   in NetIPv4_RxPktValidate().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_RX_FRAG_NONE       Datagram complete; NOT a fragment (see Note #3b1B).
*
*                                                               ------- RETURNED BY NetIPv4_RxPktFragListAdd() : --------
*                                                               ------ RETURNED BY NetIPv4_RxPktFragListInsert() : ------
*                               NET_IPv4_ERR_RX_FRAG_REASM      Fragments in reassembly progress.
*                               NET_IPv4_ERR_RX_FRAG_DISCARD    Fragment &/or datagram discarded.
*                               NET_IPv4_ERR_RX_FRAG_OFFSET     Invalid fragment offset.
*                               NET_IPv4_ERR_RX_FRAG_SIZE       Invalid fragment size.
*                               NET_IPv4_ERR_RX_FRAG_LEN_TOT    Invalid fragment   datagram total IPv4 length.
*
*                                                               ------ RETURNED BY NetIPv4_RxPktFragListInsert() : ------
*                               NET_IPv4_ERR_RX_FRAG_COMPLETE   Datagram complete; fragments reassembled (see Note #3b3).
*                               NET_IPv4_ERR_RX_FRAG_SIZE_TOT   Invalid fragmented datagram total size.
*
* Return(s)   : Pointer to reassembled datagram, if fragment reassembly complete.
*
*               Pointer to NULL,                 if fragment reassembly in progress.
*
*               Pointer to fragment buffer,      for any fragment discard error.
*
* Caller(s)   : NetIPv4_Rx().
*
* Note(s)     : (3) (a) RFC #791, Section 3.2 'Fragmentation & Reassembly' states that the following IPv4 header
*                       fields are "used together ... to identify datagram fragments for reassembly" :
*
*                       (1) "Internet identification field (ID)", ...
*                       (2) "source"     address,                 ...
*                       (3) "destination address", &              ...
*                       (4) "protocol field".
*
*                   (b) RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly Procedure'
*                       states that :
*
*                       (1) (A) "If ... both [of the following IPv4 header fields] ... are zero" ... :
*                               (1) "the fragment offset and" ...
*                               (2) "the more fragments";     ...
*
*                           (B) "Then ... this is a whole datagram ... and the datagram is forwarded to the
*                                next step in datagram processing."
*
*                       (2) "If no other fragment with [identical] buffer identifier[s] is [available] then
*                            [new] reassembly resources are allocated."
*
*                           See also 'NetIPv4_RxPktFragListAdd()  Note #2'.
*
*                       (3) When a "fragment [finally] completes the datagram ... then the datagram is sent
*                           to the next step in datagram processing".
*
*               (4) (a) Fragment lists are accessed by :
*
*                       (1) NetIPv4_RxPktFragReasm()
*                       (2) Fragment list's 'TMR->Obj' pointer      during execution of NetIPv4_RxPktFragTimeout()
*
*                   (b) Since the primary tasks of the network protocol suite are prevented from running
*                       concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                       resources of the fragment lists since no asynchronous access from other network
*                       tasks is possible.
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv4_RxPktFragReasm (NET_BUF        *p_buf,
                                          NET_BUF_HDR    *p_buf_hdr,
                                          NET_IPv4_HDR   *p_ip_hdr,
                                          NET_ERR        *p_err)
{
    CPU_BOOLEAN     frag;
    CPU_BOOLEAN     frag_done;
    CPU_BOOLEAN     ip_flag_frags_more;
    CPU_INT16U      ip_flags;
    CPU_INT16U      frag_offset;
    CPU_INT16U      frag_size;
    NET_BUF        *p_frag;
    NET_BUF        *p_frag_list;
    NET_BUF_HDR    *p_frag_list_buf_hdr;
    NET_IPv4_HDR   *p_frag_list_ip_hdr;


                                                                /* -------------- CHK FRAG REASM REQUIRED ------------- */
    frag = DEF_NO;

    ip_flags           = p_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FLAG_MASK;
    ip_flag_frags_more = DEF_BIT_IS_SET(ip_flags, NET_IPv4_HDR_FLAG_FRAG_MORE);
    if (ip_flag_frags_more != DEF_NO) {                         /* If 'More Frags' set (see Note #3b1A2), ...           */
        frag  = DEF_YES;                                        /* ... mark as frag.                                    */
    }

    frag_offset = (CPU_INT16U)(p_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FRAG_OFFSET_MASK);
    if (frag_offset != NET_IPv4_HDR_FRAG_OFFSET_NONE) {         /* If frag offset != 0 (see Note #3b1A1), ...           */
        frag  = DEF_YES;                                        /* ... mark as frag.                                    */
    }

    if (frag != DEF_YES) {                                      /* If pkt NOT a frag, ...                               */
       *p_err  = NET_IPv4_ERR_RX_FRAG_NONE;
        return (p_buf);                                         /* ... rtn non-frag'd datagram (see Note #3b1B).        */
    }


    NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxFragCtr);


                                                                /* ------------------- REASM FRAGS -------------------- */
    frag_size  = p_buf_hdr->IP_TotLen - p_buf_hdr->IP_HdrLen;
    p_frag_list = NetIPv4_FragReasmListsHead;
    frag_done  = DEF_NO;

    while (frag_done == DEF_NO) {                               /* Insert frag into a frag list.                        */

        if (p_frag_list != (NET_BUF *)0) {                      /* Srch ALL existing frag lists first (see Note #2b2A). */
            p_frag_list_buf_hdr =                 &p_frag_list->Hdr;
            p_frag_list_ip_hdr  = (NET_IPv4_HDR *)&p_frag_list->DataPtr[p_frag_list_buf_hdr->IP_HdrIx];

                                                                                  /* If frag & this frag list's    ...  */
            if (p_buf_hdr->IP_AddrSrc    == p_frag_list_buf_hdr->IP_AddrSrc) {    /* ... src  addr (see Note #2a1) ...  */
                if (p_buf_hdr->IP_AddrDest == p_frag_list_buf_hdr->IP_AddrDest) { /* ... dest addr (see Note #2a2) ...  */
                    if (p_buf_hdr->IP_ID     == p_frag_list_buf_hdr->IP_ID) {     /* ... ID        (see Note #2a3) ...  */
                        if (p_ip_hdr->Protocol == p_frag_list_ip_hdr->Protocol) { /* ... protocol  (see Note #2a4) ...  */
                                                                                  /* ... fields identical,         ...  */
                            p_frag     = NetIPv4_RxPktFragListInsert(p_buf,       /* ... insert frag into frag list.    */
                                                                    p_buf_hdr,
                                                                    ip_flags,
                                                                    frag_offset,
                                                                    frag_size,
                                                                    p_frag_list,
                                                                    p_err);
                            frag_done = DEF_YES;
                        }
                    }
                }
            }

            if (frag_done != DEF_YES) {                         /* If NOT done, adv to next frag list.                  */
                p_frag_list = p_frag_list_buf_hdr->NextPrimListPtr;
            }

        } else {                                                /* Else add new frag list (see Note #2b2B).             */
            p_frag     = NetIPv4_RxPktFragListAdd(p_buf,
                                                 p_buf_hdr,
                                                 ip_flags,
                                                 frag_offset,
                                                 frag_size,
                                                 p_err);
            frag_done = DEF_YES;
        }
    }

    return (p_frag);
}


/*
*********************************************************************************************************
*                                      NetIPv4_RxPktFragListAdd()
*
* Description : (1) Add fragment as new fragment list at end of Fragment Lists :
*
*                   (a) Get    fragment reassembly timer
*                   (b) Insert fragment into Fragment Lists
*                   (c) Update fragment list reassembly calculations
*
*
* Argument(s) : p_buf           Pointer to network buffer that received fragment.
*               -----           Argument checked   in NetIPv4_Rx().
*
*               p_buf_hdr       Pointer to network buffer header.
*               ---------       Argument validated in NetIPv4_Rx().
*
*               frag_ip_flags   Fragment IPv4 header flags.
*               -------------   Argument validated in NetIPv4_RxPktFragReasm().
*
*               frag_offset     Fragment offset.
*
*               frag_size       Fragment size (in octets).
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
* Return(s)   : Pointer to NULL,            if fragment added as new fragment list.
*
*               Pointer to fragment buffer, for any fragment discard error.
*
* Caller(s)   : NetIPv4_RxPktFragReasm().
*
* Note(s)     : (2) (a) RFC #791, Section 3.2 'Fragmentation and Reassembly' states that "if an internet
*                       datagram is fragmented" :
*
*                       (1) (A) "Fragments are counted in units of 8 octets."
*                           (B) "The minimum fragment is 8 octets."
*
*                       (2) (A) However, this CANNOT apply "if this is the last fragment" ...
*                               (1) "(that is the more fragments field is zero)";         ...
*                           (B) Which may be of ANY size.
*
*                       See also 'net_ipv4.h  IPv4 FRAGMENTATION DEFINES  Note #1a'.
*
*                   (b) RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly Procedure'
*                       states that "if no other fragment with [identical] buffer identifier[s] is [available]
*                       then [new] reassembly resources are allocated".
*
*               (3) During fragment list insertion, some fragment buffer controls were previously initialized
*                   in NetBuf_Get() when the packet was received at the network interface layer.  These buffer
*                   controls do NOT need to be re-initialized but are shown for completeness.
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv4_RxPktFragListAdd (NET_BUF           *p_buf,
                                          NET_BUF_HDR         *p_buf_hdr,
                                          NET_IPv4_HDR_FLAGS   frag_ip_flags,
                                          CPU_INT16U           frag_offset,
                                          CPU_INT16U           frag_size,
                                          NET_ERR             *p_err)
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
    if (frag_offset > NET_IPv4_HDR_FRAG_OFFSET_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvFragOffsetCtr);
       *p_err =  NET_IPv4_ERR_RX_FRAG_OFFSET;
        return (p_buf);
    }
#endif

    ip_flag_frags_more =  DEF_BIT_IS_SET(frag_ip_flags, NET_IPv4_HDR_FLAG_FRAG_MORE);
    frag_size_min      = (ip_flag_frags_more == DEF_YES) ? NET_IPv4_FRAG_SIZE_MIN_FRAG_MORE
                                                         : NET_IPv4_FRAG_SIZE_MIN_FRAG_LAST;
    if (frag_size < frag_size_min) {                            /* See Note #2a.                                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragSizeCtr);
       *p_err =  NET_IPv4_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }

    if (frag_size > NET_IPv4_FRAG_SIZE_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragSizeCtr);
       *p_err =  NET_IPv4_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }


                                                                /* ------------------- GET FRAG TMR ------------------- */
    CPU_CRITICAL_ENTER();
    timeout_tick      = NetIPv4_FragReasmTimeout_tick;
    CPU_CRITICAL_EXIT();
    p_buf_hdr->TmrPtr = NetTmr_Get((CPU_FNCT_PTR) NetIPv4_RxPktFragTimeout,
                                   (void       *) p_buf,
                                   (NET_TMR_TICK) timeout_tick,
                                   (NET_ERR    *)&tmr_err);
    if (tmr_err != NET_TMR_ERR_NONE) {                          /* If tmr unavail, discard frag.                        */
       *p_err =  NET_IPv4_ERR_RX_FRAG_DISCARD;
        return (p_buf);
    }


                                                                /* ------------ INSERT FRAG INTO FRAG LISTS ----------- */
    if (NetIPv4_FragReasmListsTail != (NET_BUF *)0) {           /* If frag lists NOT empty, insert @ tail.              */
        p_buf_hdr->PrevPrimListPtr                = (NET_BUF     *) NetIPv4_FragReasmListsTail;
        p_frag_list_tail_buf_hdr                  = (NET_BUF_HDR *)&NetIPv4_FragReasmListsTail->Hdr;
        p_frag_list_tail_buf_hdr->NextPrimListPtr = (NET_BUF     *) p_buf;
        NetIPv4_FragReasmListsTail                = (NET_BUF     *) p_buf;

    } else {                                                    /* Else add frag as first frag list.                    */
        NetIPv4_FragReasmListsHead                = (NET_BUF     *) p_buf;
        NetIPv4_FragReasmListsTail                = (NET_BUF     *) p_buf;
        p_buf_hdr->PrevPrimListPtr                = (NET_BUF     *) 0;
    }

#if 0                                                           /* Init'd in NetBuf_Get() [see Note #3].                */
    p_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
    p_buf_hdr->PrevBufPtr      = (NET_BUF *)0;
    p_buf_hdr->NextBufPtr      = (NET_BUF *)0;
    p_buf_hdr->IP_FragSizeTot  =  NET_IPv4_FRAG_SIZE_NONE;
    p_buf_hdr->IP_FragSizeCur  =  0u;
#endif


                                                                /* ----------------- UPDATE FRAG CALCS ---------------- */
    NetIPv4_RxPktFragListUpdate(p_buf,
                                p_buf_hdr,
                                frag_ip_flags,
                                frag_offset,
                                frag_size,
                                p_err);
    if (*p_err !=  NET_IPv4_ERR_NONE) {
         return ((NET_BUF *)0);
    }


   *p_err  =  NET_IPv4_ERR_RX_FRAG_REASM;
    p_frag = (NET_BUF *)0;

    return (p_frag);
}


/*
*********************************************************************************************************
*                                     NetIPv4_RxPktFragListInsert()
*
* Description : Insert fragment into corresponding fragment list.
*
*               (1) (a) Fragments are sorted into fragment lists by fragment offset.
*
*                       See Notes #2a3 & #2b3.
*
*                   (b) Although RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly
*                       Procedure' states that "in the case that two or more fragments contain the same data
*                       either identically or through a partial overlap ... [the IP fragmentation reassembly
*                       algorithm should] use the more recently arrived copy in the data buffer and datagram
*                       delivered"; in order to avoid the complexity of sequencing received fragments with
*                       duplicate data that overlap multiple previously-received fragments' data, duplicate
*                       & overlap fragments are discarded :
*
*                       (1) Duplicate fragments are discarded.  A fragment is a duplicate of a fragment already
*                           in the fragment list if both fragments have identical fragment offset & size values.
*
*                       (2) Overlap fragments are discarded & the entire Fragmented Datagram is also discarded.
*                           A fragment is an overlap fragment if any portion of its fragment data overlaps any
*                           other fragment's data :
*
*                           (A) [Fragment offset]  <  [(Any other fragment offset * FRAG_OFFSET_SIZE) +
*                                                       Any other fragment size                       ]
*
*                           (B) [(Fragment offset * FRAG_OFFSET_SIZE) + Fragment size]  >  [Any other fragment offset]
*
*
*                       See also 'net_tcp.c  NetTCP_RxPktConnHandlerRxQ_Conn()  Note #2a3'.
*
*
* Argument(s) : p_buf           Pointer to network buffer that received fragment.
*               ----            Argument checked   in NetIPv4_Rx().
*
*               p_buf_hdr       Pointer to network buffer header.
*               ---------       Argument validated in NetIPv4_Rx().
*
*               frag_ip_flags   Fragment IPv4 header flags.
*               --------------  Argument validated in NetIPv4_RxPktFragReasm().
*
*               frag_offset     Fragment offset.
*
*               frag_size       Fragment size (in octets).
*
*               p_frag_list     Pointer to fragment list head buffer.
*               -----------     Argument validated in NetIPv4_RxPktFragReasm().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_RX_FRAG_REASM      Fragment reassembly in progress.
*                               NET_IPv4_ERR_RX_FRAG_DISCARD    Fragment discarded.
*                               NET_IPv4_ERR_RX_FRAG_OFFSET     Invalid  fragment offset.
*                               NET_IPv4_ERR_RX_FRAG_SIZE       Invalid  fragment size.
*
*                                                               -- RETURNED BY NetIPv4_RxPktFragListChkComplete() : -
*                               NET_IPv4_ERR_RX_FRAG_COMPLETE   Datagram complete; fragments reassembled.
*                               NET_IPv4_ERR_RX_FRAG_SIZE_TOT   Invalid fragmented datagram total size.
*
*                                                               --- RETURNED BY NetIPv4_RxPktFragListUpdate() : ---
*                                                               - RETURNED BY NetIPv4_RxPktFragListChkComplete() : -
*                               NET_IPv4_ERR_RX_FRAG_LEN_TOT    Invalid fragment   datagram total IPv4 length.
*
* Return(s)   : Pointer to reassembled datagram, if fragment reassembly complete.
*
*               Pointer to NULL,                 if fragment reassembly in progress.
*
*               Pointer to fragment buffer,      for any fragment discard error.
*
* Caller(s)   : NetIPv4_RxPktFragReasm().
*
* Note(s)     : (2) (a) RFC #791, Section 3.2 'Fragmentation and Reassembly' states that "if an internet
*                       datagram is fragmented" :
*
*                       (1) (A) (1) "Fragments are counted in units of 8 octets."
*                               (2) "The minimum fragment is 8 octets."
*
*                           (B) (1) However, this CANNOT apply "if this is the last fragment" ...
*                                   (a) "(that is the more fragments field is zero)";         ...
*                               (2) Which may be of ANY size.
*
*                           See also 'net_ipv4.h  IPv4 FRAGMENTATION DEFINES  Note #1a'.
*
*                       (2) "Fragments ... data portion must be ... on 8 octet boundaries."
*
*                       (3) "The Fragment Offset field identifies the fragment location, relative to the
*                            beginning of the original unfragmented datagram."
*
*                   (b) RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly Procedure'
*                       states that :
*
*                       (1) (A) "If this is the first fragment"              ...
*                               (1) "(that is the fragment offset is zero)", ...
*                           (B) "this header is placed in the header buffer."
*
*                       (2) "Some [IP] options are copied [in every fragment], but others remain with
*                            the first fragment only."
*
*                       (3) "The data from the fragment is placed in the data buffer according to its
*                            fragment offset and length."
*
*                            See also Note #2a3.
*
*               (3) Assumes ALL fragments in fragment lists have previously been validated for buffer &
*                   IPv4 header fields.
*
*               (4) During fragment list insertion, some fragment buffer controls were previously
*                   initialized in NetBuf_Get() when the packet was received at the network interface
*                   layer.  These buffer controls do NOT need to be re-initialized but are shown for
*                   completeness.
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv4_RxPktFragListInsert (NET_BUF             *p_buf,
                                               NET_BUF_HDR         *p_buf_hdr,
                                               NET_IPv4_HDR_FLAGS   frag_ip_flags,
                                               CPU_INT16U           frag_offset,
                                               CPU_INT16U           frag_size,
                                               NET_BUF             *p_frag_list,
                                               NET_ERR             *p_err)
{
    CPU_BOOLEAN   ip_flag_frags_more;
    CPU_BOOLEAN   frag_insert_done;
    CPU_BOOLEAN   frag_list_discard;
    CPU_INT16U    frag_offset_actual;
    CPU_INT16U    frag_list_cur_frag_offset;
    CPU_INT16U    frag_list_cur_frag_offset_actual;
    CPU_INT16U    frag_list_prev_frag_offset;
    CPU_INT16U    frag_list_prev_frag_offset_actual;
    CPU_INT16U    frag_size_min;
    CPU_INT16U    frag_size_cur;
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
    if (frag_offset > NET_IPv4_HDR_FRAG_OFFSET_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvFragOffsetCtr);
       *p_err =  NET_IPv4_ERR_RX_FRAG_OFFSET;
        return (p_buf);
    }
#endif

    ip_flag_frags_more =  DEF_BIT_IS_SET(frag_ip_flags, NET_IPv4_HDR_FLAG_FRAG_MORE);
    frag_size_min      = (ip_flag_frags_more == DEF_YES) ? NET_IPv4_FRAG_SIZE_MIN_FRAG_MORE
                                                         : NET_IPv4_FRAG_SIZE_MIN_FRAG_LAST;
    if (frag_size < frag_size_min) {                            /* See Note #2a1.                                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragSizeCtr);
       *p_err =  NET_IPv4_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }

    if (frag_size > NET_IPv4_FRAG_SIZE_MAX) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragSizeCtr);
       *p_err =  NET_IPv4_ERR_RX_FRAG_SIZE;
        return (p_buf);
    }


                                                                        /* ------- INSERT FRAG INTO FRAG LISTS -------- */
    frag_insert_done       =  DEF_NO;

    p_frag_list_cur_buf     =  p_frag_list;
    p_frag_list_cur_buf_hdr = &p_frag_list_cur_buf->Hdr;

    while (frag_insert_done == DEF_NO) {

        frag_list_cur_frag_offset = (CPU_INT16U)(p_frag_list_cur_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FRAG_OFFSET_MASK);
        if (frag_offset > frag_list_cur_frag_offset) {                  /* While frag offset > cur frag offset, ...     */

            if (p_frag_list_cur_buf_hdr->NextBufPtr != (NET_BUF *)0) {   /* ... adv to next frag in list.                */
                p_frag_list_cur_buf     =  p_frag_list_cur_buf_hdr->NextBufPtr;
                p_frag_list_cur_buf_hdr = &p_frag_list_cur_buf->Hdr;

            } else {                                                    /* If @ last frag in list, append frag @ end.   */
                frag_offset_actual               =   frag_offset                      * NET_IPv4_FRAG_SIZE_UNIT;
                frag_list_cur_frag_offset_actual = ( frag_list_cur_frag_offset        * NET_IPv4_FRAG_SIZE_UNIT          )
                                                 + (p_frag_list_cur_buf_hdr->IP_TotLen - p_frag_list_cur_buf_hdr->IP_HdrLen);

                if (frag_offset_actual >= frag_list_cur_frag_offset_actual) {   /* If frag does NOT overlap, ...        */
                                                                                /* ... append @ end of frag list.       */
                    p_buf_hdr->PrevBufPtr               = (NET_BUF *)p_frag_list_cur_buf;
#if 0                                                                   /* Init'd in NetBuf_Get() [see Note #4].        */
                    p_buf_hdr->NextBufPtr               = (NET_BUF *)0;
                    p_buf_hdr->PrevPrimListPtr          = (NET_BUF *)0;
                    p_buf_hdr->NextPrimListPtr          = (NET_BUF *)0;
                    p_buf_hdr->TmrPtr                   = (NET_TMR *)0;
                    p_buf_hdr->IP_FragSizeTot           =  NET_IPv4_FRAG_SIZE_NONE;
                    p_buf_hdr->IP_FragSizeCur           =  0u;
#endif

                    p_frag_list_cur_buf_hdr->NextBufPtr = (NET_BUF *)p_buf;


                    p_frag_list_buf_hdr = &p_frag_list->Hdr;
                    NetIPv4_RxPktFragListUpdate(p_frag_list,             /* Update frag list reasm calcs.                */
                                                p_frag_list_buf_hdr,
                                                frag_ip_flags,
                                                frag_offset,
                                                frag_size,
                                                p_err);
                    if (*p_err !=  NET_IPv4_ERR_NONE) {
                         return ((NET_BUF *)0);
                    }
                                                                        /* Chk    frag list reasm complete.             */
                    p_frag = NetIPv4_RxPktFragListChkComplete(p_frag_list,
                                                             p_frag_list_buf_hdr,
                                                             p_err);

                } else {                                                /* Else discard overlap frag & datagram.        */
                    NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);
                    NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragDisCtr);
                    p_frag = p_buf;
                   *p_err  = NET_IPv4_ERR_RX_FRAG_DISCARD;
                }

                frag_insert_done = DEF_YES;
            }


        } else if (frag_offset < frag_list_cur_frag_offset) {           /* If frag offset < cur frag offset, ...        */
                                                                        /* ... insert frag into frag list.              */
            frag_list_discard                =  DEF_NO;

            frag_offset_actual               = (frag_offset               * NET_IPv4_FRAG_SIZE_UNIT) + frag_size;
            frag_list_cur_frag_offset_actual =  frag_list_cur_frag_offset * NET_IPv4_FRAG_SIZE_UNIT;

            if (frag_offset_actual > frag_list_cur_frag_offset_actual) {/* If frag overlaps with next frag, ...         */
                frag_list_discard  = DEF_YES;                           /* ... discard frag & datagram (see Note #1b2). */

            } else if (p_frag_list_cur_buf_hdr->PrevBufPtr != (NET_BUF *)0) {
                p_frag_list_prev_buf               =  p_frag_list_cur_buf_hdr->PrevBufPtr;
                p_frag_list_prev_buf_hdr           = &p_frag_list_prev_buf->Hdr;

                frag_offset_actual                =   frag_offset * NET_IPv4_FRAG_SIZE_UNIT;

                frag_list_prev_frag_offset        =  p_frag_list_prev_buf_hdr->IP_Flags_FragOffset & NET_IPv4_HDR_FRAG_OFFSET_MASK;
                frag_list_prev_frag_offset_actual = ( frag_list_prev_frag_offset        * NET_IPv4_FRAG_SIZE_UNIT             )
                                                  + (p_frag_list_prev_buf_hdr->IP_TotLen - p_frag_list_prev_buf_hdr->IP_HdrLen);
                                                                        /* If frag overlaps with prev frag, ...         */
                if (frag_offset_actual < frag_list_prev_frag_offset_actual) {
                    frag_list_discard  = DEF_YES;                       /* ... discard frag & datagram (see Note #1b2). */
                }
            } else {
                                                                        /* Empty Else Statement                         */
            }

            if (frag_list_discard == DEF_NO) {                          /* If frag does NOT overlap, ...                */
                                                                        /* ... insert into frag list.                   */
                p_buf_hdr->PrevBufPtr = p_frag_list_cur_buf_hdr->PrevBufPtr;
                p_buf_hdr->NextBufPtr = p_frag_list_cur_buf;

                if (p_buf_hdr->PrevBufPtr != (NET_BUF *)0) {            /* Insert p_buf between prev & cur bufs.         */
                    p_frag_list_prev_buf                     =  p_buf_hdr->PrevBufPtr;
                    p_frag_list_prev_buf_hdr                 = &p_frag_list_prev_buf->Hdr;

                    p_frag_list_prev_buf_hdr->NextBufPtr     =  p_buf;
                    p_frag_list_cur_buf_hdr->PrevBufPtr      =  p_buf;

#if 0                                                                   /* Init'd in NetBuf_Get() [see Note #4].        */
                    p_buf_hdr->PrevPrimListPtr               = (NET_BUF *)0;
                    p_buf_hdr->NextPrimListPtr               = (NET_BUF *)0;
                    p_buf_hdr->TmrPtr                        = (NET_TMR *)0;
                    p_buf_hdr->IP_FragSizeTot                =  NET_IPv4_FRAG_SIZE_NONE;
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
                    p_frag_list_cur_buf_hdr->IP_FragSizeTot  =  NET_IPv4_FRAG_SIZE_NONE;
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
                        NetIPv4_FragReasmListsHead                    =  p_buf;
                    }

                                                                        /* Point next frag list to new frag list head.  */
                    p_frag_list_next_list = p_buf_hdr->NextPrimListPtr;
                    if (p_frag_list_next_list != (NET_BUF *)0) {
                        p_frag_list_next_list_buf_hdr                  = &p_frag_list_next_list->Hdr;
                        p_frag_list_next_list_buf_hdr->PrevPrimListPtr =  p_buf;
                    } else {
                        NetIPv4_FragReasmListsTail                    =  p_buf;
                    }
                }

                p_frag_list_buf_hdr = &p_frag_list->Hdr;
                NetIPv4_RxPktFragListUpdate(p_frag_list,                 /* Update frag list reasm calcs.                */
                                            p_frag_list_buf_hdr,
                                            frag_ip_flags,
                                            frag_offset,
                                            frag_size,
                                            p_err);
                if (*p_err !=  NET_IPv4_ERR_NONE) {
                     return ((NET_BUF *)0);
                }
                                                                         /* Chk    frag list reasm complete.             */
                p_frag = NetIPv4_RxPktFragListChkComplete(p_frag_list,
                                                         p_frag_list_buf_hdr,
                                                         p_err);

            } else {                                                     /* Else discard overlap frag & datagram ...     */
                NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... (see Note #1b2).                         */
                NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragDisCtr);
                p_frag = p_buf;
               *p_err  = NET_IPv4_ERR_RX_FRAG_DISCARD;
            }

            frag_insert_done = DEF_YES;

        } else {                                                         /* Else if frag offset = cur frag offset, ...   */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragDisCtr);
            p_frag = p_buf;
           *p_err  = NET_IPv4_ERR_RX_FRAG_DISCARD;                       /* ... discard duplicate frag (see Note #1b1).  */

            frag_size_cur  = p_frag_list_cur_buf_hdr->IP_TotLen - p_frag_list_cur_buf_hdr->IP_HdrLen;
            if (frag_size != frag_size_cur) {                            /* If frag size != cur frag size,    ...        */
                NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... discard overlap frag datagram ...        */
                                                                         /* ... (see Note #1b2).                         */
            }

            frag_insert_done = DEF_YES;
        }
    }

    return (p_frag);
}


/*
*********************************************************************************************************
*                                     NetIPv4_RxPktFragListRemove()
*
* Description : (1) Remove fragment list from Fragment Lists :
*
*                   (a) Free   fragment reassembly timer
*                   (b) Remove fragment list from Fragment Lists
*                   (c) Clear  buffer's fragment pointers
*
*
* Argument(s) : p_frag_list     Pointer to fragment list head buffer.
*               -----------     Argument validated in NetIPv4_RxPktFragListDiscard(),
*                                                     NetIPv4_RxPktFragListChkComplete().
*
*               tmr_free        Indicate whether to free network timer :
*
*                                   DEF_YES            Free network timer for fragment list discard.
*                                   DEF_NO      Do NOT free network timer for fragment list discard
*                                                   [Freed by  NetTmr_TaskHandler()
*                                                          via NetIPv4_RxPktFragListDiscard()].
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktFragListChkComplete(),
*               NetIPv4_RxPktFragListDiscard().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktFragListRemove (NET_BUF      *p_frag_list,
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
        NetIPv4_FragReasmListsHead                    =  p_frag_list_next_list;
    }

                                                                /* Point next frag list to prev frag list.              */
    if (p_frag_list_next_list != (NET_BUF *)0) {
        p_frag_list_next_list_buf_hdr                  = &p_frag_list_next_list->Hdr;
        p_frag_list_next_list_buf_hdr->PrevPrimListPtr =  p_frag_list_prev_list;
    } else {
        NetIPv4_FragReasmListsTail                    =  p_frag_list_prev_list;
    }

                                                                /* ---------------- CLR BUF FRAG PTRS ----------------- */
    p_frag_list_buf_hdr->PrevPrimListPtr = (NET_BUF *)0;
    p_frag_list_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
    p_frag_list_buf_hdr->TmrPtr          = (NET_TMR *)0;
}


/*
*********************************************************************************************************
*                                    NetIPv4_RxPktFragListDiscard()
*
* Description : Discard fragment list from Fragment Lists.
*
* Argument(s) : p_frag_list     Pointer to fragment list head buffer.
*               -----------     Argument validated in NetIPv4_RxPktFragListInsert(),
*                                                     NetIPv4_RxPktFragListChkComplete(),
*                                                     NetIPv4_RxPktFragListDiscard().
*
*               tmr_free        Indicate whether to free network timer :
*
*                                   DEF_YES            Free network timer for fragment list discard.
*                                   DEF_NO      Do NOT free network timer for fragment list discard
*                                                   [Freed by NetTmr_TaskHandler()].
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_RX_FRAG_DISCARD      Fragment list discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktFragListChkComplete(),
*               NetIPv4_RxPktFragListInsert(),
*               NetIPv4_RxPktFragListUpdate(),
*               NetIPv4_RxPktFragTimeout().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktFragListDiscard (NET_BUF      *p_frag_list,
                                            CPU_BOOLEAN   tmr_free,
                                            NET_ERR      *p_err)
{
    NET_ERR  err;


    NetIPv4_RxPktFragListRemove(p_frag_list, tmr_free);                  /* Remove frag list from Frag Lists.            */
    NetIPv4_RxPktDiscard(p_frag_list, &err);                             /* Discard every frag buf in frag list.         */

    NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragDgramDisCtr); /* Inc discarded frag'd datagram ctr.           */

   *p_err = NET_IPv4_ERR_RX_FRAG_DISCARD;
}


/*
*********************************************************************************************************
*                                     NetIPv4_RxPktFragListUpdate()
*
* Description : Update fragment list reassembly calculations.
*
* Argument(s) : p_frag_list_buf_hdr     Pointer to fragment list head buffer's header.
*               -------------------     Argument validated in NetIPv4_RxPktFragListAdd(),
*                                                             NetIPv4_RxPktFragListInsert().
*
*               frag_ip_flags           Fragment IPv4 header flags.
*               --------------          Argument validated in NetIPv4_RxPktFragListAdd(),
*                                                             NetIPv4_RxPktFragListInsert().
*
*               frag_offset             Fragment offset.
*               ------------            Argument validated in NetIPv4_RxPktFragListAdd(),
*                                                             NetIPv4_RxPktFragListInsert().
*
*               frag_size               Fragment size (in octets).
*               ----------              Argument validated in NetIPv4_RxPktFragListAdd(),
*                                                             NetIPv4_RxPktFragListInsert().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Fragment list reassembly calculations
*                                                                   successfully updated.
*                               NET_IPv4_ERR_RX_FRAG_LEN_TOT    Invalid fragment datagram total IP length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktFragListAdd(),
*               NetIPv4_RxPktFragListInsert().
*
* Note(s)     : (1) (a) RFC #791, Section 3.2 'Fragmentation and Reassembly' states that "fragments ... data
*                       portion ... allows" up to :
*
*                       (1) "2**13 = 8192 fragments" ...
*                       (2) "of 8 octets each"       ...
*                       (3) "for a total of 65,536 [sic] octets."
*
*                   (b) RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly Procedure'
*                       states that :
*
*                       (1) "If this is the last fragment"                    ...
*                           (A) "(that is the more fragments field is zero)"; ...
*
*                       (2) "The total data length is computed."
*
*               (2) To avoid possible integer arithmetic overflow, the fragmentation arithmetic result MUST
*                   be declared as an integer data type with a greater resolution -- i.e. greater number of
*                   bits -- than the fragmentation arithmetic operands' data type(s).
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktFragListUpdate (NET_BUF             *p_frag_list,
                                           NET_BUF_HDR         *p_frag_list_buf_hdr,
                                           NET_IPv4_HDR_FLAGS   frag_ip_flags,
                                           CPU_INT16U           frag_offset,
                                           CPU_INT16U           frag_size,
                                           NET_ERR             *p_err)
{
    CPU_INT32U   frag_size_tot;                                     /* See Note #2.                                     */
    CPU_BOOLEAN  ip_flag_frags_more;
    NET_ERR      err;


    p_frag_list_buf_hdr->IP_FragSizeCur += frag_size;
    ip_flag_frags_more                  = DEF_BIT_IS_SET(frag_ip_flags, NET_IPv4_HDR_FLAG_FRAG_MORE);
    if (ip_flag_frags_more != DEF_YES) {                            /* If 'More Frags' NOT set (see Note #1b1A), ...    */
                                                                    /* ... calc frag tot size  (see Note #1b2).         */
        frag_size_tot = ((CPU_INT32U)frag_offset * NET_IPv4_FRAG_SIZE_UNIT) + (CPU_INT32U)frag_size;
        if (frag_size_tot > NET_IPv4_TOT_LEN_MAX) {                 /* If frag tot size > IP tot len max, ...           */
            NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... discard ovf'd frag datagram (see Note #1a3). */
           *p_err = NET_IPv4_ERR_RX_FRAG_LEN_TOT;
            return;
        }

        p_frag_list_buf_hdr->IP_FragSizeTot = (CPU_INT16U)frag_size_tot;
    }

   *p_err = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                  NetIPv4_RxPktFragListChkComplete()
*
* Description : Check if fragment list complete; i.e. fragmented datagram reassembled.
*
* Argument(s) : p_frag_list              Pointer to fragment list head buffer.
*               -----------              Argument validated in NetIPv4_RxPktFragListInsert().
*
*               p_frag_list_buf_hdr      Pointer to fragment list head buffer's header.
*               -------------------      Argument validated in NetIPv4_RxPktFragListInsert().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_RX_FRAG_COMPLETE   Datagram complete; fragments reassembled.
*                               NET_IPv4_ERR_RX_FRAG_REASM      Fragments in reassembly progress.
*                               NET_IPv4_ERR_RX_FRAG_SIZE_TOT   Invalid fragmented datagram total size.
*                               NET_IPv4_ERR_RX_FRAG_LEN_TOT    Invalid fragment   datagram total IPv4 length.
*
*                                                               - RETURNED BY NetIPv4_RxPktFragListDiscard() : -
*                               NET_IPv4_ERR_RX_FRAG_DISCARD    Fragment list discarded.
*
* Return(s)   : Pointer to reassembled datagram, if fragment reassembly complete.
*
*               Pointer to NULL,                 if fragment reassembly in progress.
*                                                   OR
*                                                for any fragment discard error.
*
* Caller(s)   : NetIPv4_RxPktFragListInsert().
*
* Note(s)     : (1) (a) RFC #791, Section 3.2 'Fragmentation and Reassembly' states that :
*
*                       (1) "Fragments ... data portion ... allows" up to :
*                           (A) "2**13 = 8192 fragments" ...
*                           (B) "of 8 octets each"       ...
*                           (C) "for a total of 65,536 [sic] octets."
*
*                       (2) "The header is counted in the total length and not in the fragments."
*
*                   (b) RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly
*                       Procedure' states that :
*
*                       (1) "If this fragment completes the datagram ... then the datagram is sent to
*                            the next step in datagram processing."
*
*                       (2) "Otherwise the timer is set to" :
*                           (A) "the maximum of the current timer value" ...
*                           (B) "and the value of the time to live field from this fragment."
*                               (1) However, since IP headers' Time-To-Live (TTL) field does NOT
*                                   directly correlate to a specific timeout value in seconds, this
*                                   requirement is NOT implemented. #### NET-803
*
*               (2) To avoid possible integer arithmetic overflow, the fragmentation arithmetic result
*                   MUST be declared as an integer data type with a greater resolution -- i.e. greater
*                   number of bits -- than the fragmentation arithmetic operands' data type(s).
*********************************************************************************************************
*/

static  NET_BUF  *NetIPv4_RxPktFragListChkComplete (NET_BUF      *p_frag_list,
                                                    NET_BUF_HDR  *p_frag_list_buf_hdr,
                                                    NET_ERR      *p_err)
{
    NET_BUF       *p_frag;
    CPU_INT32U     frag_tot_len;                                     /* See Note #2.                                     */
    NET_TMR_TICK   timeout_tick;
    NET_ERR        err;
    CPU_SR_ALLOC();

                                                                     /* If tot frag size complete, ...                   */
    if (p_frag_list_buf_hdr->IP_FragSizeCur == p_frag_list_buf_hdr->IP_FragSizeTot) {
                                                                     /* Calc frag IPv4 tot len (see Note #1a2).          */
        frag_tot_len = (CPU_INT32U)p_frag_list_buf_hdr->IP_HdrLen + (CPU_INT32U)p_frag_list_buf_hdr->IP_FragSizeTot;
        if (frag_tot_len > NET_IPv4_TOT_LEN_MAX) {                   /* If tot frag len > IPv4 tot len max, ...          */
            NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, &err);/* ... discard ovf'd frag datagram (see Note #1a1C).*/
           *p_err =   NET_IPv4_ERR_RX_FRAG_LEN_TOT;
            return ((NET_BUF *)0);
        }

        NetIPv4_RxPktFragListRemove(p_frag_list, DEF_YES);
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.RxFragDgramReasmCtr);
        p_frag = p_frag_list;                                        /* ... rtn reasm'd datagram (see Note #1b1).        */
       *p_err  = NET_IPv4_ERR_RX_FRAG_COMPLETE;

                                                                     /* If cur frag size > tot frag size, ...            */
    } else if (p_frag_list_buf_hdr->IP_FragSizeCur > p_frag_list_buf_hdr->IP_FragSizeTot) {
        NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, p_err);   /* ... discard ovf'd frag datagram.                 */
       *p_err =   NET_IPv4_ERR_RX_FRAG_SIZE_TOT;
        return ((NET_BUF *)0);


    } else {                                                         /* Else reset frag tmr (see Note #1b2A).            */
        CPU_CRITICAL_ENTER();
        timeout_tick = NetIPv4_FragReasmTimeout_tick;
        CPU_CRITICAL_EXIT();
        NetTmr_Set((NET_TMR    *) p_frag_list_buf_hdr->TmrPtr,
                   (CPU_FNCT_PTR) NetIPv4_RxPktFragTimeout,
                   (NET_TMR_TICK) timeout_tick,
                   (NET_ERR    *)&err);
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (err != NET_TMR_ERR_NONE) {
            NetIPv4_RxPktFragListDiscard(p_frag_list, DEF_YES, p_err);
            return ((NET_BUF *)0);
        }
#endif

       *p_err  =  NET_IPv4_ERR_RX_FRAG_REASM;
        p_frag = (NET_BUF *)0;
    }


    return (p_frag);
}


/*
*********************************************************************************************************
*                                      NetIPv4_RxPktFragTimeout()
*
* Description : Discard fragment list on fragment reassembly timeout.
*
* Argument(s) : p_frag_list_timeout     Pointer to network buffer fragment reassembly list (see Note #1b).
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_RxPktFragListAdd(),
*               NetIPv4_RxPktFragListChkComplete().
*
* Note(s)     : (1) RFC #791, Section 3.2 'Fragmentation and Reassembly : An Example Reassembly Procedure'
*                   states that :
*
*                   (a) "If the [IP fragments' reassembly] timer runs out," ...
*                   (b) "the [sic] all reassembly resources for this buffer identifier are released."
*
*               (2) Ideally, network timer callback functions could be defined as '[(void) (OBJECT *)]'
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
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_BUF' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (3) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer ... :
*                       (1) Cleared in NetIPv4_RxPktFragListRemove() via NetIPv4_RxPktFragListDiscard().
*
*                   (b) but do NOT re-free the timer.
*
*               (4) (a) RFC #792, Section 'Time Exceeded Message' states that :
*
*                       (1) "if a host reassembling a fragmented datagram cannot complete the reassembly
*                            due to missing fragments within its time limit" ...
*                       (2) (A) "it discards the datagram,"                  ...
*                           (B) "and it may send a time exceeded message."
*
*                   (b) MUST send ICMP 'Time Exceeded' error message BEFORE NetIPv4_RxPktFragListDiscard()
*                       frees fragment buffers.
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktFragTimeout (void  *p_frag_list_timeout)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN   used;
#endif
    NET_BUF      *p_frag_list;
    NET_ERR       err;


    p_frag_list = (NET_BUF *)p_frag_list_timeout;               /* See Note #2b2A.                                      */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* -------------- VALIDATE IPv4 RX FRAG --------------- */
    if (p_frag_list == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
        return;
    }

    used = NetBuf_IsUsed(p_frag_list);
    if (used != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.NullPtrCtr);
        return;
    }
#endif

#ifdef  NET_ICMPv4_MODULE_EN

    NetICMPv4_TxMsgErr((NET_BUF  *) p_frag_list,                /* Send ICMPv4 'Time Exceeded' err msg (see Note #4).   */
                       (CPU_INT08U) NET_ICMPv4_MSG_TYPE_TIME_EXCEED,
                       (CPU_INT08U) NET_ICMPv4_MSG_CODE_TIME_EXCEED_FRAG_REASM,
                       (CPU_INT08U) NET_ICMPv4_MSG_PTR_NONE,
                       (NET_ERR  *)&err);
#endif
                                                                /* Discard frag list (see Note #1b).                    */
    NetIPv4_RxPktFragListDiscard((NET_BUF   *) p_frag_list,
                                 (CPU_BOOLEAN) DEF_NO,          /* Clr but do NOT free tmr (see Note #3).               */
                                 (NET_ERR   *)&err);


    NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxFragDgramTimeoutCtr);
}


/*
*********************************************************************************************************
*                                     NetIPv4_RxPktDemuxDatagram()
*
* Description : Demultiplex IPv4 datagram to appropriate ICMP, IGMP, UDP, or TCP layer.
*
* Argument(s) : p_buf      Pointer to network buffer that received IPv4 datagram.
*               -----      Argument checked   in NetIPv4_Rx().
*
*               p_buf_hdr  Pointer to network buffer header.
*               ---------  Argument validated in NetIPv4_Rx().
*
*               p_err      Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_ERR_RX                      Receive error; packet discarded.
*
*                                                               ----- RETURNED BY NetICMPv4_Rx() : -----
*                               NET_ICMPv4_ERR_NONE             ICMP message successfully demultiplexed.
*
*                                                               ------ RETURNED BY NetIGMP_Rx() : ------
*                               NET_IGMP_ERR_NONE               IGMP message successfully demultiplexed.
*
*                                                               ------ RETURNED BY NetUDP_Rx() : -------
*                               NET_UDP_ERR_NONE                UDP datagram successfully demultiplexed.
*
*                                                               ------ RETURNED BY NetTCP_Rx() : -------
*                               NET_TCP_ERR_NONE                TCP segment  successfully demultiplexed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_Rx().
*
* Note(s)     : (1) When network buffer is demultiplexed to higher-layer protocol receive, buffer's reference
*                   counter is NOT incremented since the IPv4 layer does NOT maintain a reference to the
*                   buffer.
*
*               (2) Default case already invalidated in NetIPv4_RxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktDemuxDatagram (NET_BUF      *p_buf,
                                          NET_BUF_HDR  *p_buf_hdr,
                                          NET_ERR      *p_err)
{
    switch (p_buf_hdr->ProtocolHdrType) {                        /* Demux buf to appropriate protocol (see Note #1).     */
#ifdef NET_ICMPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_ICMP_V4:
             NetICMPv4_Rx(p_buf, p_err);
             break;
#endif


#ifdef  NET_IGMP_MODULE_EN
        case NET_PROTOCOL_TYPE_IGMP:
             NetIGMP_Rx(p_buf, p_err);
             break;
#endif


        case NET_PROTOCOL_TYPE_UDP_V4:
             NetUDP_Rx(p_buf, p_err);
             break;


#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V4:
             NetTCP_Rx(p_buf, p_err);
             break;
#endif

        case NET_PROTOCOL_TYPE_NONE:
        default:                                                /* See Note #2.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.RxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }
}


/*
*********************************************************************************************************
*                                        NetIPv4_RxPktDiscard()
*
* Description : On any IPv4 receive error(s), discard IPv4 packet(s) & buffer(s).
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_Rx(),
*               NetIPv4_RxPktFragListDiscard().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv4_RxPktDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.IPv4.RxPktDisCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBufList((NET_BUF *)p_buf,
                            (NET_CTR *)pctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                        NetIPv4_TxPktValidate()
*
* Description : (1) Validate IPv4 transmit packet parameters & options :
*
*                   (a) Validate the following transmit packet parameters :
*
*                       (1) Supported protocols :
*                           (A) ICMP
*                           (B) IGMP
*                           (C) UDP
*                           (D) TCP
*
*                       (2) Buffer protocol index
*                       (3) Total Length
*                       (4) Type of Service (TOS)                                   See Note #2c
*                       (5) Flags
*                       (6) Time-to-Live    (TTL)                                   See Note #2d
*                       (7) Destination Address                                     See Note #2f
*                       (8) Source      Address                                     See Note #2e
*                       (9) Options
*
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_Tx().
*
*               addr_src    Source      IPv4 address.
*
*               addr_dest   Destination IPv4 address.
*
*               TOS         Specific TOS to transmit IPv4 packet
*                               (see 'net_ipv4.h  IPv4 HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit IPv4 packet
*                               (see 'net_ipv4.h  IPv4 HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IPv4_TTL_MIN                minimum TTL transmit value   (1)
*                               NET_IPv4_TTL_MAX                maximum TTL transmit value (255)
*                               NET_IPv4_TTL_DFLT               default TTL transmit value (128)
*                               NET_IPv4_TTL_NONE               replace with default TTL
*
*               flags       Flags to select transmit options; bit-field flags logically OR'd :
*
*                               NET_IPv4_FLAG_NONE              No  IPv4 transmit flags selected.
*                               NET_IPv4_FLAG_TX_DONT_FRAG      Set IPv4 'Don't Frag' flag.
*
*               p_opts      Pointer to one or more IPv4 options configuration data structures
*                               (see 'net_ipv4.h  IPv4 HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IPv4 transmit options configuration.
*                               NET_IPv4_OPT_CFG_ROUTE_TS       Route &/or Internet Timestamp options configuration.
*                               NET_IPv4_OPT_CFG_SECURITY       Security options configuration
*                                                                   (see 'net_ipv4.h  Note #1d').
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Transmit packet validated.
*                               NET_IPv4_ERR_INVALID_LEN_DATA       Invalid protocol/data length.
*                               NET_IPv4_ERR_INVALID_TOS            Invalid IPv4 TOS.
*                               NET_IPv4_ERR_INVALID_FLAG           Invalid IPv4 flag(s).
*                               NET_IPv4_ERR_INVALID_TTL            Invalid IPv4 TTL.
*                               NET_IPv4_ERR_INVALID_ADDR_SRC       Invalid IPv4 source          address.
*                               NET_IPv4_ERR_INVALID_ADDR_DEST      Invalid IPv4 destination     address.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 default gateway address.
*                               NET_BUF_ERR_INVALID_TYPE            Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX              Invalid/insufficient buffer index.
*                               NET_ERR_INVALID_PROTOCOL            Invalid/unknown protocol type.
*
*                                                                   - RETURNED BY NetIPv4_TxPktValidateOpt() : -
*                               NET_IPv4_ERR_NONE                   IPv4 transmit option configurations validated.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE       Invalid IPv4 option  type.
*                               NET_IPv4_ERR_INVALID_OPT_LEN        Invalid IPv4 options length.
*                               NET_IPv4_ERR_INVALID_OPT_CFG        Invalid IPv4 options configuration.
*                               NET_IPv4_ERR_INVALID_OPT_ROUTE      Invalid IPv4 route address(s).
*
*                                                                   ---- RETURNED BY NetIF_IsEnHandler() : -----
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled network interface.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_Tx().
*
* Note(s)     : (2) RFC #1122, Section 3.2.1 requires that IPv4 packets be transmitted with the following
*                   valid IPv4 header fields :
*
*                   (a) Version                                             RFC #1122, Section 3.2.1.1
*                   (b) Check-Sum                                           RFC #1122, Section 3.2.1.2
*                   (c) Type of Service (TOS)                               RFC #1122, Section 3.2.1.6
*                   (d) Time-to-Live    (TTL)                               RFC #1122, Section 3.2.1.7
*
*                       (1) RFC #1122, Section 3.2.1.7 states that "a host MUST NOT send a datagram with
*                           a Time-to-Live (TTL) value of zero".
*
*                   (e) Source      Address
*
*                       (1) (A) RFC #1122, Section 3.2.1.3 states that "when a host sends any datagram,
*                               the IP source address MUST be one of its own IP addresses (but not a
*                               broadcast or multicast address)".
*
*                           (B) (1) MUST     be one of the following :
*                                   (a) Configured host address             RFC #1122, Section 3.2.1.3.(1)
*                                   (b) Localhost                           RFC #1122, Section 3.2.1.3.(g)
*                                                                               See also Note #2e2A
*                                   (c) This       Host                     RFC #1122, Section 3.2.1.3.(a)
*                                   (d) Specified  Host                     RFC #1122, Section 3.2.1.3.(b)
*
*                               (2) MUST NOT be one of the following :
*                                   (a) Multicast  host address             RFC #1112, Section 7.2
*                                   (b) Limited    Broadcast                RFC #1122, Section 3.2.1.3.(c)
*                                   (c) Directed   Broadcast                RFC #1122, Section 3.2.1.3.(d)
*                                   (d) Subnet     Broadcast                RFC #1122, Section 3.2.1.3.(e)
*
*                       (2) (A) RFC #1122, Section 3.2.1.3.(g) states that the "internal host loopback
*                               address ... MUST NOT appear outside a host".
*
*                               (1) However, this does NOT prevent the host loopback address from being
*                                   used as an IPv4 packet's source address as long as BOTH the packet's
*                                   source AND destination addresses are internal host addresses, either
*                                   a configured host IP address or any host loopback address.
*
*                   (f) Destination Address
*
*                       (1) (A) (1) MAY      be one of the following :
*                                   (a) Configured host address             RFC #1122, Section 3.2.1.3.(1)
*                                   (b) Multicast  host address             RFC #1122, Section 3.2.1.3.(3)
*                                                                               See also Note #2f2A
*                                   (c) Localhost                           RFC #1122, Section 3.2.1.3.(g)
*                                   (d) Link-local host address             RFC #3927, Section 2.1
*                                                                               See also Note #2f2B
*                                   (e) Limited    Broadcast                RFC #1122, Section 3.2.1.3.(c)
*                                   (f) Directed   Broadcast                RFC #1122, Section 3.2.1.3.(d)
*                                   (g) Subnet     Broadcast                RFC #1122, Section 3.2.1.3.(e)
*
*                               (2) MUST NOT be one of the following :
*                                   (a) This       Host                     RFC #1122, Section 3.2.1.3.(a)
*                                   (b) Specified  Host                     RFC #1122, Section 3.2.1.3.(b)
*
*                       (2) (A) (1) RFC #1112, Section 4 specifies that "class D ... host group addresses
*                                   range" :
*
*                                   (a) "from 224.0.0.0" ...
*                                   (b) "to   239.255.255.255".
*
*                               (2) However, RFC #1112, Section 4 adds that :
*
*                                   (a) "address 224.0.0.0 is guaranteed not to be assigned to any group", ...
*                                   (b) "and 224.0.0.1 is assigned to the permanent group of all IP hosts."
*
*                           (B) (1) RFC #3927, Section 2.1 specifies the "IPv4 Link-Local address ... range
*                                   ... [as] inclusive" ... :
*
*                                   (a) "from 169.254.1.0" ...
*                                   (b) "to   169.254.254.255".
*
*                               (2) RFC #3927, Section 2.6.2 states that "169.254.255.255 ... is the broadcast
*                                   address for the Link-Local prefix".
*
*               (3) See 'net_ipv4.h  IPv4 ADDRESS DEFINES  Notes #2 & #3' for supported IPv4 addresses.
*********************************************************************************************************
*/

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
static  void  NetIPv4_TxPktValidate (NET_BUF_HDR    *p_buf_hdr,
                                     NET_IPv4_ADDR   addr_src,
                                     NET_IPv4_ADDR   addr_dest,
                                     NET_IPv4_TOS    TOS,
                                     NET_IPv4_TTL    TTL,
                                     NET_IPv4_FLAGS  flags,
                                     void           *p_opts,
                                     NET_ERR        *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT16U       ix;
    CPU_INT16U       len;
    CPU_INT16U       flag_mask;
    CPU_BOOLEAN      tos_mbz;
#endif
    NET_IPv4_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;
    CPU_BOOLEAN      addr_host;
    CPU_BOOLEAN      tx_remote_host;


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
        case NET_PROTOCOL_TYPE_ICMP_V4:
             ix  = p_buf_hdr->ICMP_MsgIx;
             len = p_buf_hdr->ICMP_MsgLen;
             break;


#ifdef  NET_IGMP_MODULE_EN
        case NET_PROTOCOL_TYPE_IGMP:
             ix  = p_buf_hdr->IGMP_MsgIx;
             len = p_buf_hdr->IGMP_MsgLen;
             break;
#endif


        case NET_PROTOCOL_TYPE_UDP_V4:
#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V4:
#endif
             ix  = p_buf_hdr->TransportHdrIx;
             len = p_buf_hdr->TransportHdrLen + (CPU_INT16U)p_buf_hdr->DataLen;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    if (ix <  NET_IPv4_HDR_SIZE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }



                                                                    /* ------------ VALIDATE TOT DATA LEN ------------- */
    if (len != p_buf_hdr->TotLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvDataLenCtr);
       *p_err = NET_IPv4_ERR_INVALID_LEN_DATA;
        return;
    }



                                                                    /* -------------- VALIDATE IPv4 TOS --------------- */
    tos_mbz = DEF_BIT_IS_SET(TOS, NET_IPv4_HDR_TOS_MBZ_MASK);
    if (tos_mbz != DEF_NO) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvTOS_Ctr);
       *p_err = NET_IPv4_ERR_INVALID_TOS;
        return;
    }



                                                                    /* ------------- VALIDATE IPv4 FLAGS -------------- */
    flag_mask = NET_IPv4_FLAG_NONE        |
                NET_IPv4_FLAG_TX_DONT_FRAG;

    if ((flags & ~flag_mask) != NET_IPv4_FLAG_NONE) {               /* If any invalid flags req'd, rtn err.             */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvFlagsCtr);
       *p_err = NET_IPv4_ERR_INVALID_FLAG;
        return;
    }



                                                                    /* -------------- VALIDATE IPv4 TTL --------------- */
    if (TTL < 1) {                                                  /* If TTL < 1, rtn err (see Note #2d1).             */
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvTTL_Ctr);
       *p_err = NET_IPv4_ERR_INVALID_TTL;
        return;
    }


#else                                                               /* Prevent 'variable unused' compiler warnings.     */
   (void)&TOS;
   (void)&TTL;
   (void)&flags;
   (void)&p_opts;
#endif



                                                                    /* ------------- VALIDATE IPv4 ADDRS -------------- */
    if_nbr = p_buf_hdr->IF_Nbr;                                     /* Get pkt's tx IF.                                 */
   (void)NetIF_IsEnHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

                                                                    /* Chk pkt's tx cfg'd host addr.                    */
    if (if_nbr != NET_IF_NBR_LOCAL_HOST) {
        p_ip_addrs = NetIPv4_GetAddrsHostCfgdOnIF((NET_IPv4_ADDR)addr_src,
                                                 (NET_IF_NBR   )if_nbr);
    } else {
        p_ip_addrs = NetIPv4_GetAddrsHostCfgd((NET_IPv4_ADDR )addr_src,
                                             (NET_IF_NBR   *)0);
    }

    if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {
        if ((p_ip_addrs->AddrHost               == NET_IPv4_ADDR_NONE) ||
            (p_ip_addrs->AddrHostSubnetMask     == NET_IPv4_ADDR_NONE) ||
            (p_ip_addrs->AddrHostSubnetMaskHost == NET_IPv4_ADDR_NONE) ||
            (p_ip_addrs->AddrHostSubnetNet      == NET_IPv4_ADDR_NONE)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }
    }


                                                                    /* ----------- VALIDATE IPv4 DEST ADDR ------------ */
                                                                    /* See Note #2e.                                    */
    addr_host = NetIPv4_IsAddrHostCfgdHandler(addr_dest);           /* Chk this host's cfg'd addr(s) [see Note #2f1A1a].*/
    if (addr_host == DEF_YES) {

        tx_remote_host = DEF_NO;

                                                                    /* Chk invalid 'This Host'       (see Note #2f1A2a).*/
    } else if (addr_dest == NET_IPv4_ADDR_THIS_HOST) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
       *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
        return;

                                                                    /* Chk localhost addrs           (see Note #2f1A1c).*/
    } else if ((addr_dest & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                            NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
                                                                    /* Chk localhost 'This Host'     (see Note #2f1A2b).*/
        if ((addr_dest               & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
             return;
        }

        tx_remote_host = DEF_NO;

                                                                    /* Chk link-local addrs          (see Note #2f1A1d).*/
    } else if ((addr_dest & NET_IPv4_ADDR_LOCAL_LINK_MASK_NET) ==
                            NET_IPv4_ADDR_LOCAL_LINK_NET     ) {
                                                                    /* Chk link-local broadcast      (see Note #2f2B2). */
        if (addr_dest == NET_IPv4_ADDR_LOCAL_LINK_BROADCAST) {
            ;
                                                                    /* Chk invalid link-local addrs  (see Note #2f2B1a).*/
        } else if ((addr_dest < NET_IPv4_ADDR_LOCAL_LINK_HOST_MIN) ||
                   (addr_dest > NET_IPv4_ADDR_LOCAL_LINK_HOST_MAX)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
             return;
        } else {
                                                                    /* Empty Else Statement                             */
        }

        tx_remote_host = DEF_YES;

                                                                    /* Chk lim'd broadcast           (see Note #2f1A1e).*/
    } else if (addr_dest == NET_IPv4_ADDR_BROADCAST) {

        tx_remote_host = DEF_YES;


#ifdef  NET_MCAST_MODULE_EN
                                                                    /* Chk Class-D multicast         (see Note #2f1A1b).*/
    } else if ((addr_dest & NET_IPv4_ADDR_CLASS_D_MASK) == NET_IPv4_ADDR_CLASS_D) {
                                                                    /* Chk invalid multicast addrs   (see Note #2f2A).  */
        if ((addr_dest < NET_IPv4_ADDR_MULTICAST_MIN) ||
            (addr_dest > NET_IPv4_ADDR_MULTICAST_MAX)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
             return;
        }

        tx_remote_host = DEF_YES;
#endif

                                                                    /* Chk remote hosts :                               */
    } else if (p_ip_addrs == (NET_IPv4_ADDRS *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
       *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
        return;

                                                                    /* Chk local  subnet.                               */
    } else if ((addr_dest & p_ip_addrs->AddrHostSubnetMask) ==
                            p_ip_addrs->AddrHostSubnetNet ) {
                                                                    /* Chk local  subnet 'This Host' (see Note #2f1A2b).*/
        if ((addr_dest               & p_ip_addrs->AddrHostSubnetMaskHost) ==
            (NET_IPv4_ADDR_THIS_HOST & p_ip_addrs->AddrHostSubnetMaskHost)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
             return;
        }

        tx_remote_host = DEF_YES;


    } else {
                                                                    /* Chk remote subnet.                               */
        if ((addr_dest & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
                                                                    /* Chk Class-A 'This Host'       (see Note #2f1A2b).*/
            if ((addr_dest               & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }

        } else if ((addr_dest & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
                                                                    /* Chk Class-B 'This Host'       (see Note #2f1A2b).*/
            if ((addr_dest               & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }

        } else if ((addr_dest & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
                                                                    /* Chk Class-C 'This Host'       (see Note #2f1A2b).*/
            if ((addr_dest               & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
                 return;
            }

        } else {                                                    /* Discard invalid addr class    (see Note #3).     */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrDestCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_DEST;
            return;
        }

                                                                    /* Chk dflt gateway cfg'd.                          */
        if (p_ip_addrs->AddrDfltGateway == NET_IPv4_ADDR_NONE) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.CfgInvGatewayCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_GATEWAY;
            return;
        }

        tx_remote_host = DEF_YES;
    }


                                                                    /* ------------- VALIDATE IP SRC ADDR ------------- */
                                                                    /* See Note #2d.                                    */
                                                                    /* Chk this host's cfg'd addr  (see Note #2e1B1a).  */
    if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {
#if 0                                                               /* Chk'd in 'VALIDATE IPv4 ADDRS'.                  */
        if (p_ip_addrs->AddrHost == NET_IPv4_ADDR_NONE) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
            return;
        }
#endif
        if (addr_src != p_ip_addrs->AddrHost) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
            return;
        }

                                                                    /* Chk 'This Host'             (see Note #2e1B1c).  */
    } else if (addr_src == NET_IPv4_ADDR_THIS_HOST) {

                                                                    /* Chk         localhost addrs (see Note #2e1B1b).  */
    } else if ((addr_src & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                           NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
                                                                    /* Chk invalid localhost addrs.                     */
        if ((addr_src                & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST) ==
            (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }
        if ((addr_src                & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_LOCAL_HOST_MASK_HOST)) {
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
            *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
             return;
        }

        if (tx_remote_host != DEF_NO) {                             /* If localhost addr tx'd to remote host, ...       */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;                  /* ... rtn err / discard pkt   (see Note #2e2A).    */
            return;
        }


    } else {
        if ((addr_src & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
                                                                    /* Chk Class-A 'This Host'     (see Note #2e1B1d).  */
            if ((addr_src                & NET_IPv4_ADDR_CLASS_A_MASK_HOST) !=
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
                 return;
            }

        } else if ((addr_src & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
                                                                    /* Chk Class-B 'This Host'     (see Note #2e1B1d).  */
            if ((addr_src                & NET_IPv4_ADDR_CLASS_B_MASK_HOST) !=
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
                 return;
            }

        } else if ((addr_src & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
                                                                    /* Chk Class-C 'This Host'     (see Note #2e1B1d).  */
            if ((addr_src                & NET_IPv4_ADDR_CLASS_C_MASK_HOST) !=
                (NET_IPv4_ADDR_THIS_HOST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
                *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
                 return;
            }

        } else {                                                    /* Discard invalid addr class  (see Note #3).       */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvAddrSrcCtr);
           *p_err = NET_IPv4_ERR_INVALID_ADDR_SRC;
            return;
        }
    }



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* --------------- VALIDATE IP OPTS --------------- */
    if (p_opts != (void *)0) {
        NetIPv4_TxPktValidateOpt(p_opts, p_err);
        if (*p_err != NET_IPv4_ERR_NONE) {
            return;
        }
    }
#endif


   *p_err = NET_IPv4_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetIPv4_TxPktValidateOpt()
*
* Description : (1) Validate IPv4 transmit option configurations :
*
*                   (a) IPv4 transmit options MUST be configured by appropriate transmit options configuration
*                       data structure(s) passed via 'p_opts'; see 'net_ipv4.h  IP HEADER OPTION CONFIGURATION
*                       DATA TYPES' for IPv4 options configuration.
*
*                   (b) IPv4 header allows for a maximum option size of 40 octets (see 'net_ipv4.h  IPv4 HEADER
*                       OPTIONS DEFINES  Note #3').
*
*
* Argument(s) : p_opts      Pointer to one or more IPv4 options configuration data structures (see Note #1a) :
*
*                               NET_IPv4_OPT_CFG_ROUTE_TS           IPv4 Route &/or Internet Timestamp options
*                                                                       configuration.
*                               NET_IPv4_OPT_CFG_SECURITY           Security options configuration
*                                                                       (see 'net_ipv4.c  Note #1d').
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   IPv4 transmit option configurations validated.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE       Invalid IPv4 option type.
*                               NET_IPv4_ERR_INVALID_OPT_LEN        Total options length exceeds maximum IPv4
*                                                                       options length (see Note #1b).
*                               NET_IPv4_ERR_INVALID_OPT_CFG        Invalid number of exclusive IPv4 options
*                                                                       (see Note #3).
*
*                                                                   - RETURNED BY NetIPv4_TxPktValidateOptRouteTS() : -
*                               NET_IPv4_ERR_INVALID_OPT_TYPE       Invalid IPv4 option type.
*                               NET_IPv4_ERR_INVALID_OPT_LEN        Invalid number of Route &/or Internet
*                                                                     Timestamp entries configured.
*                               NET_IPv4_ERR_INVALID_OPT_ROUTE      Invalid route address(s).
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPktValidate().
*
* Note(s)     : (2) (a) See 'net_ipv4.h  IPv4 HEADER OPTIONS DEFINES' for   supported IPv4 options' summary.
*
*                   (b) See 'net_ipv4.c  Note #1d'                    for unsupported IPv4 options.
*
*               (3) The following IPv4 transmit options MUST be configured exclusively--i.e. only a single
*                   IPv4 Route or Internet Timestamp option may be configured for any one IPv4 datagram :
*
*                   (a) NET_IPv4_OPT_TYPE_ROUTE_STRICT
*                   (b) NET_IPv4_OPT_TYPE_ROUTE_LOOSE
*                   (c) NET_IPv4_OPT_TYPE_ROUTE_REC
*                   (d) NET_IPv4_OPT_TYPE_TS_ONLY
*                   (e) NET_IPv4_OPT_TYPE_TS_ROUTE_REC
*                   (f) NET_IPv4_OPT_TYPE_TS_ROUTE_SPEC
*
*                       (A) RFC #1122, Section 3.2.1.8.(c) prohibits "an IP header" from transmitting
*                           with "more than one Source Route option".
*
*               (4) RFC #791, Section 3.1 'Options : Internet Timestamp' states that "each timestamp"
*                   may be "preceded with [an] internet address".
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIPv4_TxPktValidateOpt (void     *p_opts,
                                        NET_ERR  *p_err)
{
    CPU_INT08U          opt_len_size;
    CPU_INT08U          opt_len;
    CPU_INT08U          opt_nbr_route_ts;
    NET_IPv4_OPT_TYPE  *p_opt_cfg_type;
    void               *p_opt_cfg;
    void               *p_opt_next;


    opt_len_size     = 0u;
    opt_nbr_route_ts = 0u;
    p_opt_cfg        = p_opts;

    while (p_opt_cfg  != (void *)0) {
        p_opt_cfg_type = (NET_IPv4_OPT_TYPE *)p_opt_cfg;
        switch (*p_opt_cfg_type) {
            case NET_IPv4_OPT_TYPE_ROUTE_STRICT:
            case NET_IPv4_OPT_TYPE_ROUTE_LOOSE:
            case NET_IPv4_OPT_TYPE_ROUTE_REC:
            case NET_IPv4_OPT_TYPE_TS_ONLY:
            case NET_IPv4_OPT_TYPE_TS_ROUTE_REC:
            case NET_IPv4_OPT_TYPE_TS_ROUTE_SPEC:
                 if (opt_nbr_route_ts > 0) {                    /* If > 1 exclusive IPv4 opt, rtn err (see Note #3A).   */
                     NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptCfgCtr);
                    *p_err = NET_IPv4_ERR_INVALID_OPT_CFG;
                     return;
                 }
                 opt_nbr_route_ts++;

                 NetIPv4_TxPktValidateOptRouteTS(p_opt_cfg, &opt_len, &p_opt_next, p_err);
                 break;

#if 0                                                           /* -------------- UNSUPPORTED IPv4 OPTS --------------- */
                                                                /* See Note #2b.                                        */
            case NET_IPv4_OPT_TYPE_SECURITY:
            case NET_IPv4_OPT_SECURITY_EXTENDED:
#endif
            case NET_IPv4_OPT_TYPE_NONE:                        /* ---------------- INVALID IPv4 OPTS ----------------- */
            default:
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptTypeCtr);
                *p_err = NET_IPv4_ERR_INVALID_OPT_TYPE;
                 return;
        }

        if (*p_err != NET_IPv4_ERR_NONE) {
            return;
        }

        opt_len_size += opt_len;
        if (opt_len_size > NET_IPv4_HDR_OPT_SIZE_MAX) {         /* If tot opt len exceeds max opt len, rtn err.         */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptLenCtr);
           *p_err = NET_IPv4_ERR_INVALID_OPT_LEN;
            return;
        }

        p_opt_cfg = p_opt_next;                                 /* Validate next cfg opt.                               */
    }

   *p_err = NET_IPv4_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                   NetIPv4_TxPktValidateOptRouteTS()
*
* Description : (1) Validate IPv4 Route &/or Internet Timestamp option configuration :
*
*                   (a) See 'net_ipv4.h  IPv4 ROUTE & INTERNET TIMESTAMP OPTIONS CONFIGURATION DATA TYPE' for
*                       valid IPv4 Route &/or Internet Timestamp option configuration.
*
*                   (b) Validate the following options' configuration parameters :
*
*                       (1) Type
*                       (2) Number
*                             * Less    than minimum
*                             * Greater than maximum
*                       (3) IPv4 Route addresses
*                             * MUST be IPv4 Class A, B, or C address   See 'net_ipv4.h  IPv4 ADDRESS DEFINES  Note #2a'
*                       (4) Internet Timestamps
*                             * Timestamp values are NOT validated
*
*                   (c) Return option values.
*
*
* Argument(s) : p_opt_route_ts  Pointer to IPv4 Route &/or Internet Timestamp option configuration data structure.
*               --------------  Argument checked   in NetIPv4_TxPktValidateOpt().
*
*               p_opt_len       Pointer to variable that will receive the Route/Internet Timestamp option length
*               ---------           (in octets).
*
*                               Argument validated in NetIPv4_TxPktValidateOpt().
*
*               p_opt_next       Pointer to variable that will receive the pointer to the next IPv4 transmit option.
*               ----------       Argument validated in NetIPv4_TxPktValidateOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Route &/or Internet Timestamp option
*                                                                       configuration validated.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE       Invalid IPv4 option type.
*                               NET_IPv4_ERR_INVALID_OPT_LEN        Invalid number of Route &/or Internet
*                                                                       Timestamp entries configured.
*                               NET_IPv4_ERR_INVALID_OPT_ROUTE      Invalid route address(s).
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPktValidateOpt().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIPv4_TxPktValidateOptRouteTS (void         *p_opt_route_ts,
                                               CPU_INT08U   *p_opt_len,
                                               void        **p_opt_next,
                                               NET_ERR      *p_err)
{
    NET_IPv4_OPT_CFG_ROUTE_TS  *p_opt_cfg_route_ts;
    CPU_INT08U                  opt_nbr_min;
    CPU_INT08U                  opt_nbr_max;
    CPU_INT08U                  opt_len;
    CPU_INT08U                  opt_len_opt;
    CPU_INT08U                  opt_len_param;
    CPU_INT08U                  opt_route_ix;
    CPU_BOOLEAN                 opt_route_spec;
    NET_IPv4_ADDR               opt_route_addr;


    p_opt_cfg_route_ts = (NET_IPv4_OPT_CFG_ROUTE_TS *)p_opt_route_ts;

                                                                /* ------------------ VALIDATE TYPE ------------------- */
    switch (p_opt_cfg_route_ts->Type) {
        case NET_IPv4_OPT_TYPE_ROUTE_STRICT:
             opt_nbr_min    = NET_IPv4_OPT_PARAM_NBR_MIN;
             opt_nbr_max    = NET_IPv4_OPT_PARAM_NBR_MAX_ROUTE;
             opt_len_opt    = NET_IPv4_HDR_OPT_SIZE_ROUTE;
             opt_len_param  = sizeof(NET_IPv4_ADDR);
             opt_route_spec = DEF_YES;
             break;


        case NET_IPv4_OPT_TYPE_ROUTE_LOOSE:
             opt_nbr_min    = NET_IPv4_OPT_PARAM_NBR_MIN;
             opt_nbr_max    = NET_IPv4_OPT_PARAM_NBR_MAX_ROUTE;
             opt_len_opt    = NET_IPv4_HDR_OPT_SIZE_ROUTE;
             opt_len_param  = sizeof(NET_IPv4_ADDR);
             opt_route_spec = DEF_YES;
             break;


        case NET_IPv4_OPT_TYPE_ROUTE_REC:
             opt_nbr_min    = NET_IPv4_OPT_PARAM_NBR_MIN;
             opt_nbr_max    = NET_IPv4_OPT_PARAM_NBR_MAX_ROUTE;
             opt_len_opt    = NET_IPv4_HDR_OPT_SIZE_ROUTE;
             opt_len_param  = sizeof(NET_IPv4_ADDR);
             opt_route_spec = DEF_NO;
             break;


        case NET_IPv4_OPT_TYPE_TS_ONLY:
             opt_nbr_min    = NET_IPv4_OPT_PARAM_NBR_MIN;
             opt_nbr_max    = NET_IPv4_OPT_PARAM_NBR_MAX_TS_ONLY;
             opt_len_opt    = NET_IPv4_HDR_OPT_SIZE_TS;
             opt_len_param  = sizeof(NET_TS);
             opt_route_spec = DEF_NO;
             break;


        case NET_IPv4_OPT_TYPE_TS_ROUTE_REC:
             opt_nbr_min    = NET_IPv4_OPT_PARAM_NBR_MIN;
             opt_nbr_max    = NET_IPv4_OPT_PARAM_NBR_MAX_TS_ROUTE;
             opt_len_opt    = NET_IPv4_HDR_OPT_SIZE_TS;
             opt_len_param  = sizeof(NET_IPv4_ADDR) + sizeof(NET_TS);
             opt_route_spec = DEF_NO;
             break;


        case NET_IPv4_OPT_TYPE_TS_ROUTE_SPEC:
             opt_nbr_min    = NET_IPv4_OPT_PARAM_NBR_MIN;
             opt_nbr_max    = NET_IPv4_OPT_PARAM_NBR_MAX_TS_ROUTE;
             opt_len_opt    = NET_IPv4_HDR_OPT_SIZE_TS;
             opt_len_param  = sizeof(NET_IPv4_ADDR) + sizeof(NET_TS);
             opt_route_spec = DEF_YES;
             break;


        case NET_IPv4_OPT_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptTypeCtr);
            *p_err = NET_IPv4_ERR_INVALID_OPT_TYPE;
             return;
    }

                                                                /* ------------------- VALIDATE NBR ------------------- */
    if (p_opt_cfg_route_ts->Nbr < opt_nbr_min) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptLenCtr);
       *p_err = NET_IPv4_ERR_INVALID_OPT_LEN;
        return;
    }
    if (p_opt_cfg_route_ts->Nbr > opt_nbr_max) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptLenCtr);
       *p_err = NET_IPv4_ERR_INVALID_OPT_LEN;
        return;
    }

                                                                /* ------------------ VALIDATE ROUTE ------------------ */
    if (opt_route_spec == DEF_YES) {                            /* For specified routes ...                             */
                                                                /* ... validate all route addrs (see Note #1b3).        */
        for (opt_route_ix = 0u; opt_route_ix < p_opt_cfg_route_ts->Nbr; opt_route_ix++) {

            opt_route_addr = p_opt_cfg_route_ts->Route[opt_route_ix];

            if ((opt_route_addr & NET_IPv4_ADDR_CLASS_A_MASK) != NET_IPv4_ADDR_CLASS_A) {
                if ((opt_route_addr & NET_IPv4_ADDR_CLASS_B_MASK) != NET_IPv4_ADDR_CLASS_B) {
                    if ((opt_route_addr & NET_IPv4_ADDR_CLASS_C_MASK) != NET_IPv4_ADDR_CLASS_C) {
                        NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptCfgCtr);
                       *p_err = NET_IPv4_ERR_INVALID_OPT_ROUTE;
                        return;
                    }
                }
            }
        }
    }

                                                                /* ------------------- VALIDATE TS -------------------- */
                                                                /* See Note #1b4.                                       */


                                                                /* ------------------- RTN OPT VALS ------------------- */
    opt_len   = opt_len_opt + (p_opt_cfg_route_ts->Nbr * opt_len_param);
   *p_opt_len  = opt_len;
   *p_opt_next = p_opt_cfg_route_ts->NextOptPtr;
   *p_err      = NET_IPv4_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                            NetIPv4_TxPkt()
*
* Description : (1) Prepare IPv4 header & transmit IPv4 packet :
*
*                   (a) Prepare   IPv4 options (if any)
*                   (b) Calculate IPv4 header buffer controls
*                   (c) Check for transmit fragmentation        See Note #2
*                   (d) Prepare   IPv4 header
*                   (e) Transmit  IPv4 packet datagram
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit IPv4 packet.
*               -----       Argument checked   in NetIPv4_Tx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_Tx().
*
*               addr_src    Source      IPv4 address.
*               ---------   Argument checked   in NetIPv4_TxPktValidate().
*
*               addr_dest   Destination IPv4 address.
*               ----------  Argument checked   in NetIPv4_TxPktValidate().
*
*               TOS         Specific TOS to transmit IPv4 packet
*               ---             (see 'net_ipv4.h  IPv4 HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*                           Argument checked   in NetIPv4_TxPktValidate().
*
*               TTL         Specific TTL to transmit IPv4 packet
*               ---             (see 'net_ipv4.h  IPv4 HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IPv4_TTL_MIN                minimum TTL transmit value   (1)
*                               NET_IPv4_TTL_MAX                maximum TTL transmit value (255)
*                               NET_IPv4_TTL_DFLT               default TTL transmit value (128)
*                               NET_IPv4_TTL_NONE               replace with default TTL
*
*                           Argument validated in NetIPv4_TxPktValidate().
*
*               flags       Flags to select transmit options; bit-field flags logically OR'd :
*               -----
*                               NET_IPv4_FLAG_NONE              No  IPv4 transmit flags selected.
*                               NET_IPv4_FLAG_TX_DONT_FRAG      Set IPv4 'Don't Frag' flag.
*
*                           Argument checked   in NetIPv4_TxPktValidate().
*
*               p_opts      Pointer to one or more IPv4 options configuration data structures
*               -----           (see 'net_ipv4.h  IPv4 HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IPv4 transmit options configuration.
*                               NET_IPv4_OPT_CFG_ROUTE_TS       Route &/or Internet Timestamp options configuration.
*                               NET_IPv4_OPT_CFG_SECURITY       Security options configuration
*                                                                   (see 'net_ipv4.h  Note #1d').
*
*                           Argument checked   in NetIPv4_TxPktValidate().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_INVALID_PROTOCOL            Invalid/unknown protocol type.
*                               NET_IPv4_ERR_INVALID_LEN_HDR        Invalid IPv4 header length.
*                               NET_IPv4_ERR_INVALID_FRAG           Invalid IPv4 fragmentation.
*
*                                                                   -- RETURNED BY NetIF_MTU_GetProtocol() : --
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL function pointer.
*
*                                                                   - RETURNED BY NetIPv4_TxPktPrepareOpt() : -
*                               NET_IPv4_ERR_INVALID_OPT_TYPE       Invalid IPv4 option type.
*                               NET_IPv4_ERR_INVALID_OPT_LEN        Invalid IPv4 option length.
*
*                                                                   - RETURNED BY NetIPv4_TxPktPrepareHdr() : -
*                               NET_BUF_ERR_INVALID_IX              Invalid/insufficient buffer index.
*                               NET_BUF_ERR_INVALID_LEN             Invalid buffer length.
*                               NET_UTIL_ERR_NULL_PTR               Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE              Check-sum passed a zero size.
*
*                                                                   -- RETURNED BY NetIPv4_TxPktDatagram() : --
*                               NET_IF_ERR_NONE                     Packet successfully transmitted.
*                               NET_ERR_IF_LOOPBACK_DIS             Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN                Network  interface link state down (i.e.
*                                                                       NOT available for receive or transmit).
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid IPv4 host    address.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 gateway address.
*                               NET_IPv4_ERR_TX_DEST_INVALID        Invalid transmit destination.
*                               NET_ERR_TX                          Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_Tx().
*
* Note(s)     : (2) IPv4 transmit fragmentation NOT currently supported (see 'net_ipv4.c  Note #1c'). #### NET-810
*
*               (3) Default case already invalidated in NetIPv4_TxPktValidate().  However, the default case
*                   is included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv4_TxPkt (NET_BUF        *p_buf,
                             NET_BUF_HDR    *p_buf_hdr,
                             NET_IPv4_ADDR   addr_src,
                             NET_IPv4_ADDR   addr_dest,
                             NET_IPv4_TOS    TOS,
                             NET_IPv4_TTL    TTL,
                             NET_IPv4_FLAGS  flags,
                             void           *p_opts,
                             NET_ERR        *p_err)
{
#if 0                                                           /* NOT currently implemented (see Note #2).             */
    CPU_BOOLEAN        flag_dont_frag;
#endif
    CPU_INT08U         ip_opt_len_size;
    CPU_INT16U         ip_hdr_len_size;
    CPU_INT16U         protocol_ix;
    NET_MTU            ip_mtu;
    NET_IPv4_OPT_SIZE  ip_hdr_opts[NET_IPv4_HDR_OPT_NBR_MAX];
    CPU_BOOLEAN        ip_tx_frag;


                                                                /* ---------------- PREPARE IPv4 OPTS ----------------- */
    if (p_opts != (void *)0) {
        ip_opt_len_size = NetIPv4_TxPktPrepareOpt((void       *) p_opts,
                                                  (CPU_INT08U *)&ip_hdr_opts[0],
                                                  (NET_ERR    *) p_err);
        if (*p_err != NET_IPv4_ERR_NONE) {
             return;
        }
    } else {
        ip_opt_len_size = 0u;
    }


                                                                /* --------------- CALC IPv4 HDR CTRLS ---------------- */
                                                                /* Calc tot IPv4 hdr len (in octets).                   */
    ip_hdr_len_size = (CPU_INT16U)(NET_IPv4_HDR_SIZE_MIN + ip_opt_len_size);
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (ip_hdr_len_size > NET_IPv4_HDR_SIZE_MAX) {
       *p_err = NET_IPv4_ERR_INVALID_LEN_HDR;
        return;
    }
#endif

    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_ICMP_V4:
             protocol_ix = p_buf_hdr->ICMP_MsgIx;
             break;


#ifdef  NET_IGMP_MODULE_EN
        case NET_PROTOCOL_TYPE_IGMP:
             protocol_ix = p_buf_hdr->IGMP_MsgIx;
             break;
#endif


        case NET_PROTOCOL_TYPE_UDP_V4:
#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V4:
#endif
             protocol_ix = p_buf_hdr->TransportHdrIx;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }


                                                                /* ----------------- CHK FRAG REQUIRED ---------------- */
    ip_tx_frag = DEF_NO;
    if (protocol_ix < ip_hdr_len_size) {                        /* If hdr len > allowed rem ix, tx frag req'd.          */
        ip_tx_frag = DEF_YES;
    }

    ip_mtu  = NetIF_MTU_GetProtocol(p_buf_hdr->IF_Nbr, NET_PROTOCOL_TYPE_IP_V4, NET_IF_FLAG_NONE, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
    ip_mtu -= ip_hdr_len_size - NET_IPv4_HDR_SIZE_MIN;
    if (p_buf_hdr->TotLen > ip_mtu) {                            /* If tot len > MTU,            tx frag req'd.          */
        ip_tx_frag = DEF_YES;
    }


    if (ip_tx_frag == DEF_NO) {                                 /* If tx frag NOT required, ...                         */

        NetIPv4_TxPktPrepareHdr(p_buf,                          /* ... prepare IPv4 hdr     ...                         */
                                p_buf_hdr,
                                ip_hdr_len_size,
                                ip_opt_len_size,
                                protocol_ix,
                                addr_src,
                                addr_dest,
                                TOS,
                                TTL,
                                flags,
                               &ip_hdr_opts[0],
                                p_err);

        if (*p_err != NET_IPv4_ERR_NONE) {
             return;
        }

        NetIPv4_TxPktDatagram(p_buf, p_buf_hdr, p_err);         /* ... & tx IPv4 datagram.                              */

    } else {
#if 0                                                           /* NOT currently implemented (see Note #2).             */
        flag_dont_frag = DEF_BIT_IS_SET(flags, NET_IPv4_FLAG_TX_DONT_FRAG);
        if (flag_dont_frag != DEF_NO) {
           *p_err = NET_IPv4_ERR_INVALID_FRAG;
            return;
        }
#else
       *p_err = NET_IPv4_ERR_INVALID_FRAG;
        return;
#endif
    }
}

/*
*********************************************************************************************************
*                                         NetIPv4_TxIxDataGet()
*
* Description : Get the offset of a buffer at which the IPv4 data can be written.
*
* Argument(s) : if_nbr      Network interface number to transmit data.
*
*               data_len    IPv4 payload size.
*
*               mtu         MTU for the upper-layer protocol.
*
*               p_ix        Pointer to the current protocol index.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE           No error.
*
*                               ----------- RETURNED BY NetIF_GetTxDataIx() : -----------
*                               See NetIF_GetTxDataIx() for additional return error codes.
*
*
* Return(s)   : none.
*
* Caller(s)   : NetICMPv4_TxIxDataGet(),
*               NetIGMP_TxIxDataGet(),
*               NetTCP_GetTxDataIx(),
*               NetUDP_GetTxDataIx().
*
* Note(s)     : none.
*********************************************************************************************************
*/
void  NetIPv4_TxIxDataGet (NET_IF_NBR   if_nbr,
                           CPU_INT32U   data_len,
                           CPU_INT16U   mtu,
                           CPU_INT16U  *p_ix,
                           NET_ERR     *p_err)
{
                                                                /* Add IPv4 min hdr len to current offset.              */
   *p_ix += NET_IPv4_HDR_SIZE;

                                                                /* Add the lower-level hdr        offsets.              */
    NetIF_TxIxDataGet(if_nbr, data_len, p_ix, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }


   (void)&mtu;

   *p_err  = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetIPv4_TxPktPrepareOpt()
*
* Description : (1) Prepare IPv4 header with IPv4 transmit options :
*
*                   (a) Prepare ALL IPv4 options from configuration
*                           data structure(s)
*                   (b) Pad remaining IPv4 header octets            See RFC #791, Section 3.1 'Padding'
*
*               (2) IP transmit options MUST be configured by appropriate options configuration data structure(s)
*                   passed via 'p_opts'; see 'net_ipv4.h  IPv4 HEADER OPTION CONFIGURATION DATA TYPES' for IPv4
*                   options configuration.
*
*               (3) Convert ALL IPv4 options' multi-octet words from host-order to network-order.
*
*
* Argument(s) : p_opts      Pointer to one or more IPv4 options configuration data structures (see Note #2) :
*               ------
*                               NULL                            NO IPv4 transmit options configuration.
*                               NET_IPv4_OPT_CFG_ROUTE_TS       Route &/or Internet Timestamp options configuration.
*                               NET_IPv4_OPT_CFG_SECURITY       Security options configuration
*                                                                   (see 'net_ipv4.h  Note #1d').
*
*                           Argument checked   in NetIPv4_TxPkt().
*
*               p_opt_hdr   Pointer to IPv4 transmit option buffer to prepare IPv4 options.
*               ---------   Argument validated in NetIPv4_TxPkt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IPv4 header options successfully prepared.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE   Invalid IPv4 option type.
*                               NET_IPv4_ERR_INVALID_OPT_LEN    Invalid IPv4 option length.
*
* Return(s)   : Total IPv4 option length (in octets), if NO error(s).
*
*               0,                                  otherwise.
*
* Caller(s)   : NetIPv4_TxPkt().
*
* Note(s)     : (4) (a) See 'net_ip.h  IP HEADER OPTIONS DEFINES' for   supported IP options' summary.
*
*                   (b) See 'net_ip.c  Note #1d'                  for unsupported IP options.
*
*               (5) Transmit arguments & options validated in NetIPv4_TxPktValidate()/NetIPv4_TxPktValidateOpt() :
*
*                   (a) Assumes ALL   transmit arguments & options are valid
*                   (b) Assumes total transmit options' lengths    are valid
*
*               (6) IP header allows for a maximum option size of 40 octets (see 'net_ipv4.h  IPv4 HEADER
*                   OPTIONS DEFINES  Note #3').
*
*               (7) Default case already invalidated in NetIPv4_TxPktValidateOpt().  However, the default
*                   case is included as an extra precaution in case any of the IPv4 transmit options types
*                   are incorrectly modified.
*********************************************************************************************************
*/

static  CPU_INT08U  NetIPv4_TxPktPrepareOpt (void        *p_opts,
                                             CPU_INT08U  *p_opt_hdr,
                                             NET_ERR     *p_err)
{
    CPU_INT08U          ip_opt_len_tot = 0u;
    CPU_INT08U          ip_opt_len     = 0u;
    CPU_INT08U         *p_opt_cfg_hdr  = DEF_NULL;
    NET_IPv4_OPT_TYPE  *p_opt_cfg_type = DEF_NULL;
    void               *p_opt_next     = DEF_NULL;
    void               *p_opt_cfg      = DEF_NULL;


    ip_opt_len_tot = 0u;
    p_opt_cfg       = p_opts;
    p_opt_cfg_hdr   = p_opt_hdr;
                                                                /* ---------------- PREPARE IPv4 OPTS ----------------- */
    while (p_opt_cfg  != (void *)0) {                           /* Prepare ALL cfg'd IPv4 opts (see Note #1a).          */
        p_opt_cfg_type = (NET_IPv4_OPT_TYPE *)p_opt_cfg;
        switch (*p_opt_cfg_type) {
            case NET_IPv4_OPT_TYPE_ROUTE_STRICT:
            case NET_IPv4_OPT_TYPE_ROUTE_LOOSE:
            case NET_IPv4_OPT_TYPE_ROUTE_REC:
                 NetIPv4_TxPktPrepareOptRoute(p_opt_cfg, p_opt_cfg_hdr, &ip_opt_len, &p_opt_next, p_err);
                 break;


            case NET_IPv4_OPT_TYPE_TS_ONLY:
                 NetIPv4_TxPktPrepareOptTS(p_opt_cfg, p_opt_cfg_hdr, &ip_opt_len, &p_opt_next, p_err);
                 break;


            case NET_IPv4_OPT_TYPE_TS_ROUTE_REC:
            case NET_IPv4_OPT_TYPE_TS_ROUTE_SPEC:
                 NetIPv4_TxPktPrepareOptTSRoute(p_opt_cfg, p_opt_cfg_hdr, &ip_opt_len, &p_opt_next, p_err);
                 break;

                                                                /* -------------- UNSUPPORTED IPv4 OPTS --------------- */
                                                                /* See Note #4b.                                        */
            case NET_IPv4_OPT_TYPE_SECURITY:
            case NET_IPv4_OPT_SECURITY_EXTENDED:
                 break;

                                                                /* ---------------- INVALID IPv4 OPTS ----------------- */
            case NET_IPv4_OPT_TYPE_NONE:
            default:                                            /* See Note #6.                                         */
                 NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptTypeCtr);
                *p_err =  NET_IPv4_ERR_INVALID_OPT_TYPE;
                 return (0u);
        }
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (*p_err != NET_IPv4_ERR_NONE) {                      /* See Note #4a.                                        */
             return (0u);
        }
        if (ip_opt_len_tot > NET_IPv4_HDR_OPT_SIZE_MAX) {       /* See Note #4b.                                        */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptLenCtr);
           *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
            return (0u);
        }
#endif

        ip_opt_len_tot += ip_opt_len;
        p_opt_cfg_hdr  += ip_opt_len;

        p_opt_cfg       = p_opt_next;                          /* Prepare next cfg opt.                                */
    }


                                                                /* ------------------- PAD IPv4 HDR ------------------- */
    if (ip_opt_len_tot > 0u) {
                                                                /* Pad rem'ing IPv4 hdr octets (see Note #1b).          */
        while ((ip_opt_len_tot %  NET_IPv4_HDR_OPT_SIZE_WORD) &&
               (ip_opt_len_tot <= NET_IPv4_HDR_OPT_SIZE_MAX )) {
           *p_opt_cfg_hdr = NET_IPv4_HDR_OPT_PAD;
            p_opt_cfg_hdr++;
            ip_opt_len_tot++;
        }
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (ip_opt_len_tot > NET_IPv4_HDR_OPT_SIZE_MAX) {       /* See Note #4b.                                        */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptLenCtr);
           *p_err =  NET_IPv4_ERR_INVALID_OPT_LEN;
            return (0u);
        }
#endif
    }


   *p_err =  NET_IPv4_ERR_NONE;

    return (ip_opt_len_tot);
}


/*
*********************************************************************************************************
*                                    NetIPv4_TxPktPrepareOptRoute()
*
* Description : (1) Prepare IPv4 header with IPv4 Route transmit options :
*
*                   (a) Prepare IPv4 Route option header
*                   (b) Prepare IPv4 Route
*
*               (2) See RFC #791, Section 3.1 'Options : Loose/Strict Source & Record Route'.
*
*
* Argument(s) : p_opts      Pointer to IPv4 Route option configuration data structure.
*               ------      Argument checked   in NetIPv4_TxPktPrepareOpt().
*
*               p_opt_hdr   Pointer to IPv4 transmit option buffer to prepare IPv4 Route option.
*               ---------   Argument validated in NetIPv4_TxPkt().
*
*               p_opt_len   Pointer to variable that will receive the Route option length (in octets).
*               ---------   Argument validated in NetIPv4_TxPktPrepareOpt().
*
*               p_opt_next  Pointer to variable that will receive the pointer to the next IPv4 transmit option.
*               ---------   Argument validated in NetIPv4_TxPktPrepareOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IPv4 Route option successfully prepared.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE   Invalid IPv4 option type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPktPrepareOpt().
*
* Note(s)     : (3) Transmit arguments & options validated in NetIPv4_TxPktValidate()/NetIPv4_TxPktValidateOpt() :
*
*                   (a) Assumes ALL   transmit arguments & options are valid
*                   (b) Assumes total transmit options' lengths    are valid
*
*               (4) Default case already invalidated in NetIPv4_TxPktValidateOpt().  However, the default
*                   case is included as an extra precaution in case any of the IPv4 transmit options types
*                   are incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktPrepareOptRoute (void         *p_opts,
                                            CPU_INT08U   *p_opt_hdr,
                                            CPU_INT08U   *p_opt_len,
                                            void        **p_opt_next,
                                            NET_ERR      *p_err)
{
    NET_IPv4_OPT_CFG_ROUTE_TS  *p_opt_cfg_route_ts;
    NET_IPv4_OPT_SRC_ROUTE     *p_opt_route;
    CPU_BOOLEAN                 opt_route_spec;
    CPU_INT08U                  opt_route_ix;
    NET_IPv4_ADDR               opt_route_addr;


                                                                /* -------------- PREPARE ROUTE OPT HDR --------------- */
    p_opt_cfg_route_ts = (NET_IPv4_OPT_CFG_ROUTE_TS *)p_opts;
    p_opt_route        = (NET_IPv4_OPT_SRC_ROUTE    *)p_opt_hdr;
    p_opt_route->Len   =  NET_IPv4_HDR_OPT_SIZE_ROUTE + (p_opt_cfg_route_ts->Nbr * sizeof(NET_IPv4_ADDR));
    p_opt_route->Ptr   =  NET_IPv4_OPT_ROUTE_PTR_ROUTE;
    p_opt_route->Pad   =  NET_IPv4_HDR_OPT_PAD;

    switch (p_opt_cfg_route_ts->Type) {
        case NET_IPv4_OPT_TYPE_ROUTE_STRICT:
             p_opt_route->Type = NET_IPv4_HDR_OPT_ROUTE_SRC_STRICT;
             opt_route_spec   = DEF_YES;
             break;


        case NET_IPv4_OPT_TYPE_ROUTE_LOOSE:
             p_opt_route->Type = NET_IPv4_HDR_OPT_ROUTE_SRC_LOOSE;
             opt_route_spec   = DEF_YES;
             break;


        case NET_IPv4_OPT_TYPE_ROUTE_REC:
             p_opt_route->Type = NET_IPv4_HDR_OPT_ROUTE_REC;
             opt_route_spec   = DEF_NO;
             break;


        case NET_IPv4_OPT_TYPE_NONE:
        default:                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptTypeCtr);
            *p_err = NET_IPv4_ERR_INVALID_OPT_TYPE;
             return;
    }


                                                                /* ------------------ PREPARE ROUTE ------------------- */
    for (opt_route_ix = 0u; opt_route_ix < p_opt_cfg_route_ts->Nbr; opt_route_ix++) {
                                                                /* Cfg specified or rec route addrs.                    */
        if (opt_route_spec == DEF_YES) {
            opt_route_addr =  p_opt_cfg_route_ts->Route[opt_route_ix];
        } else {
            opt_route_addr = (NET_IPv4_ADDR)NET_IPv4_ADDR_NONE;
        }
        NET_UTIL_VAL_COPY_SET_NET_32(&p_opt_route->Route[opt_route_ix], &opt_route_addr);
    }


   *p_opt_len  = p_opt_route->Len;
   *p_opt_next = p_opt_cfg_route_ts->NextOptPtr;
   *p_err      = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetIPv4_TxPktPrepareOptTS()
*
* Description : (1) Prepare IPv4 header with Internet Timestamp option :
*
*                   (a) Prepare Internet Timestamp option header
*                   (b) Prepare Internet Timestamps
*
*               (2) See RFC #791, Section 3.1 'Options : Internet Timestamp'.
*
*
* Argument(s) : p_opts      Pointer to Internet Timestamp option configuration data structure.
*               ------      Argument checked   in NetIPv4_TxPktPrepareOpt().
*
*               p_opt_hdr   Pointer to IP transmit option buffer to prepare Internet Timestamp option.
*               ---------   Argument validated in NetIPv4_TxPkt().
*
*               p_opt_len   Pointer to variable that will receive the Internet Timestamp option length
*               ---------        (in octets).
*
*                           Argument validated in NetIPv4_TxPktPrepareOpt().
*
*               p_opt_next  Pointer to variable that will receive the pointer to the next IPv4 transmit option.
*               ----------  Argument validated in NetIPv4_TxPktPrepareOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Internet Timestamp option successfully prepared.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE   Invalid IPv4 option type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPktPrepareOpt().
*
* Note(s)     : (3) Transmit arguments & options validated in NetIPv4_TxPktValidate()/NetIPv4_TxPktValidateOpt() :
*
*                   (a) Assumes ALL   transmit arguments & options are valid
*                   (b) Assumes total transmit options' lengths    are valid
*
*               (4) Default case already invalidated in NetIPv4_TxPktValidateOpt().  However, the default
*                   case is included as an extra precaution in case any of the IPv4 transmit options types
*                   are incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktPrepareOptTS (void         *p_opts,
                                         CPU_INT08U   *p_opt_hdr,
                                         CPU_INT08U   *p_opt_len,
                                         void        **p_opt_next,
                                         NET_ERR      *p_err)
{
    NET_IPv4_OPT_CFG_ROUTE_TS  *p_opt_cfg_route_ts;
    NET_IPv4_OPT_TS            *p_opt_ts;
    CPU_INT08U                  opt_ts_ovf;
    CPU_INT08U                  opt_ts_flags;
    CPU_INT08U                  opt_ts_ix;
    NET_TS                      opt_ts;


                                                                /* ---------------- PREPARE TS OPT HDR ---------------- */
    p_opt_cfg_route_ts = (NET_IPv4_OPT_CFG_ROUTE_TS *)p_opts;

    switch (p_opt_cfg_route_ts->Type) {
        case NET_IPv4_OPT_TYPE_TS_ONLY:
             p_opt_ts            = (NET_IPv4_OPT_TS *)p_opt_hdr;
             p_opt_ts->Type      =  NET_IPv4_HDR_OPT_TS;
             p_opt_ts->Len       =  NET_IPv4_HDR_OPT_SIZE_TS + (p_opt_cfg_route_ts->Nbr * sizeof(NET_TS));
             p_opt_ts->Ptr       =  NET_IPv4_OPT_TS_PTR_TS;

             opt_ts_ovf         =  0u;
             opt_ts_flags       =  NET_IPv4_OPT_TS_FLAG_TS_ONLY;
             p_opt_ts->Ovf_Flags =  opt_ts_ovf | opt_ts_flags;
             break;


        case NET_IPv4_OPT_TYPE_NONE:
        default:                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptTypeCtr);
            *p_err = NET_IPv4_ERR_INVALID_OPT_TYPE;
             return;
    }


                                                                /* -------------------- PREPARE TS -------------------- */
    for (opt_ts_ix = 0u; opt_ts_ix < p_opt_cfg_route_ts->Nbr; opt_ts_ix++) {
        opt_ts = p_opt_cfg_route_ts->TS[opt_ts_ix];
        NET_UTIL_VAL_COPY_SET_NET_32(&p_opt_ts->TS[opt_ts_ix], &opt_ts);
    }


   *p_opt_len  = p_opt_ts->Len;
   *p_opt_next = p_opt_cfg_route_ts->NextOptPtr;
   *p_err      = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   NetIPv4_TxPktPrepareOptTSRoute()
*
* Description : (1) Prepare IPv4 header with Internet Timestamp with IPv4 Route option :
*
*                   (a) Prepare Internet Timestamp option header
*                   (b) Prepare Internet Timestamps
*
*               (2) See RFC #791, Section 3.1 'Options : Internet Timestamp'.
*
*
* Argument(s) : p_opts      Pointer to Internet Timestamp option configuration data structure.
*               ------      Argument checked   in NetIPv4_TxPktPrepareOpt().
*
*               p_opt_hdr   Pointer to IP transmit option buffer to prepare Internet Timestamp option.
*               ---------   Argument validated in NetIPv4_TxPkt().
*
*               p_opt_len   Pointer to variable that will receive the Internet Timestamp option length
*               ---------   Argument validated in NetIPv4_TxPktPrepareOpt().
*                                   (in octets).
*
*               p_opt_next  Pointer to variable that will receive the pointer to the next IPv4 transmit option.
*               ----------  Argument validated in NetIPv4_TxPktPrepareOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               Internet Timestamp with IP Route option
*                                                                   successfully prepared.
*                               NET_IPv4_ERR_INVALID_OPT_TYPE   Invalid IPv4 option type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPktPrepareOpt().
*
* Note(s)     : (3) Transmit arguments & options validated in NetIPv4_TxPktValidate()/NetIPv4_TxPktValidateOpt() :
*
*                   (a) Assumes ALL   transmit arguments & options are valid
*                   (b) Assumes total transmit options' lengths    are valid
*
*               (4) Default case already invalidated in NetIPv4_TxPktValidateOpt().  However, the default
*                   case is included as an extra precaution in case any of the IPv4 transmit options types
*                   are incorrectly modified.
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktPrepareOptTSRoute (void         *p_opts,
                                              CPU_INT08U   *p_opt_hdr,
                                              CPU_INT08U   *p_opt_len,
                                              void        **p_opt_next,
                                              NET_ERR      *p_err)
{
    NET_IPv4_OPT_CFG_ROUTE_TS  *p_opt_cfg_route_ts;
    NET_IPv4_OPT_TS_ROUTE      *p_opt_ts_route;
    NET_IPv4_ROUTE_TS          *p_route_ts;
    CPU_INT08U                  opt_ts_ovf;
    CPU_INT08U                  opt_ts_flags;
    CPU_INT08U                  opt_ts_ix;
    NET_TS                      opt_ts;
    CPU_BOOLEAN                 opt_route_spec;
    NET_IPv4_ADDR               opt_route_addr;


                                                                /* ---------------- PREPARE TS OPT HDR ---------------- */
    p_opt_cfg_route_ts   = (NET_IPv4_OPT_CFG_ROUTE_TS *)p_opts;
    p_opt_ts_route       = (NET_IPv4_OPT_TS_ROUTE     *)p_opt_hdr;
    p_opt_ts_route->Type =  NET_IPv4_HDR_OPT_TS;
    p_opt_ts_route->Len  =  NET_IPv4_HDR_OPT_SIZE_TS + (p_opt_cfg_route_ts->Nbr * (sizeof(NET_IPv4_ADDR) + sizeof(NET_TS)));
    p_opt_ts_route->Ptr  =  NET_IPv4_OPT_TS_PTR_TS;
    opt_ts_ovf          =  0u;

    switch (p_opt_cfg_route_ts->Type) {
        case NET_IPv4_OPT_TYPE_TS_ROUTE_REC:
             opt_ts_flags             = NET_IPv4_OPT_TS_FLAG_TS_ROUTE_REC;
             p_opt_ts_route->Ovf_Flags = opt_ts_ovf | opt_ts_flags;
             opt_route_spec           = DEF_NO;
             break;


        case NET_IPv4_OPT_TYPE_TS_ROUTE_SPEC:
             opt_ts_flags             = NET_IPv4_OPT_TS_FLAG_TS_ROUTE_SPEC;
             p_opt_ts_route->Ovf_Flags = opt_ts_ovf | opt_ts_flags;
             opt_route_spec           = DEF_YES;
             break;


        case NET_IPv4_OPT_TYPE_NONE:
        default:                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvOptTypeCtr);
            *p_err = NET_IPv4_ERR_INVALID_OPT_TYPE;
             return;
    }


                                                                /* ----------------- PREPARE ROUTE/TS ----------------- */
    p_route_ts = &p_opt_ts_route->Route_TS[0];
    for (opt_ts_ix = 0u; opt_ts_ix < p_opt_cfg_route_ts->Nbr; opt_ts_ix++) {
                                                                /* Cfg specified or rec route addrs.                    */
        if (opt_route_spec == DEF_YES) {
            opt_route_addr =  p_opt_cfg_route_ts->Route[opt_ts_ix];
        } else {
            opt_route_addr = (NET_IPv4_ADDR)NET_IPv4_ADDR_NONE;
        }
        NET_UTIL_VAL_COPY_SET_NET_32(&p_route_ts->Route[opt_ts_ix], &opt_route_addr);

        opt_ts = p_opt_cfg_route_ts->TS[opt_ts_ix];
        NET_UTIL_VAL_COPY_SET_NET_32(&p_route_ts->TS[opt_ts_ix],    &opt_ts);
    }


   *p_opt_len  = p_opt_ts_route->Len;
   *p_opt_next = p_opt_cfg_route_ts->NextOptPtr;
   *p_err      = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetIPv4_TxPktPrepareHdr()
*
* Description : (1) Prepare IP header :
*
*                   (a) Update network buffer's protocol index & length controls
*
*                   (b) Prepare the transmit packet's following IP header fields :
*
*                       (1) Version
*                       (2) Header Length
*                       (3) Type of Service (TOS)
*                       (4) Total  Length
*                       (5) Identification  (ID)
*                       (6) Flags
*                       (7) Fragment Offset
*                       (8) Time-to-Live    (TTL)
*                       (9) Protocol
*                      (10) Check-Sum                                   See Note #6
*                      (11) Source      Address
*                      (12) Destination Address
*                      (13) Options
*
*                   (c) Convert the following IP header fields from host-order to network-order :
*
*                       (1) Total Length
*                       (2) Identification (ID)
*                       (3) Flags/Fragment Offset
*                       (4) Source      Address
*                       (5) Destination Address
*                       (6) Check-Sum                                   See Note #6c
*                       (7) Options                                     See Note #5
*
*
* Argument(s) : p_buf               Pointer to network buffer to transmit IP packet.
*               -----               Argument checked   in NetIPv4_Tx().
*
*               p_buf_hdr           Pointer to network buffer header.
*               ---------           Argument validated in NetIPv4_Tx().
*
*               ip_hdr_len_tot      Total IPv4 header length.
*               ---------------     Argument checked   in NetIPv4_TxPkt().
*
*               ip_opt_len_tot      Total IPv4 header options' length.
*               ---------------     Argument checked   in NetIPv4_TxPktPrepareOpt().
*
*               protocol_ix         Index to higher-layer protocol header.
*
*               addr_src            Source      IP address.
*               ---------           Argument checked   in NetIPv4_TxPktValidate().
*
*               addr_dest           Destination IPv4 address.
*               ----------          Argument checked   in NetIPv4_TxPktValidate().
*
*               TOS                 Specific TOS to transmit IPv4 packet
*               ---                     (see 'net_ipv4.h  IPv4 HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*                                   Argument checked   in NetIPv4_TxPktValidate().
*
*               TTL                 Specific TTL to transmit IPv4 packet
*               ---                     (see 'net_ipv4.h  IPv4 HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                                       NET_IPv4_TTL_MIN                minimum TTL transmit value   (1)
*                                       NET_IPv4_TTL_MAX                maximum TTL transmit value (255)
*                                       NET_IPv4_TTL_DFLT               default TTL transmit value (128)
*                                       NET_IPv4_TTL_NONE               replace with default TTL
*
*                                   Argument validated in NetIPv4_TxPktValidate().
*
*               flags               Flags to select transmit options; bit-field flags logically OR'd :
*               -----
*                                       NET_IPv4_FLAG_NONE              No  IPv4 transmit flags selected.
*                                       NET_IPv4_FLAG_TX_DONT_FRAG      Set IPv4 'Don't Frag' flag.
*
*                                   Argument checked   in NetIPv4_TxPktValidate().
*
*               p_ip_hdr_opts       Pointer to IPv4 options buffer.
*               -------------       Argument checked   in NetIPv4_TxPktPrepareOpt().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IPv4 header successfully prepared.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*
*                                                               ----------- RETURNED BY NetBuf_DataWr() : -----------
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index  for transmit options.
*                               NET_BUF_ERR_INVALID_LEN         Invalid buffer length for transmit options.
*
*                                                               - RETURNED BY NetUtil_16BitOnesCplChkSumHdrCalc() : -
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPkt().
*
* Note(s)     : (2) See 'net_ipv4.h  IPv4 HEADER' for IPv4 header format.
*
*               (3) Supports ONLY the following protocols :
*
*                   (a) ICMP
*                   (b) IGMP
*                   (c) UDP
*                   (d) TCP
*
*                   See also 'net.h  Note #2a'.
*
*               (4) Default case already invalidated in NetIPv4_TxPktValidate().  However, the default case is
*                   included as an extra precaution in case 'ProtocolHdrType' is incorrectly modified.
*
*               (5) Assumes ALL IPv4 options' multi-octet words previously converted from host-order to
*                   network-order.
*
*               (6) (a) IPv4 header Check-Sum MUST be calculated AFTER the entire IPv4 header has been prepared.
*                       In addition, ALL multi-octet words are converted from host-order to network-order
*                       since "the sum of 16-bit integers can be computed in either byte order" [RFC #1071,
*                       Section 2.(B)].
*
*                   (b) IPv4 header Check-Sum field MUST be cleared to '0' BEFORE the IP header Check-Sum is
*                       calculated (see RFC #791, Section 3.1 'Header Checksum').
*
*                   (c) The IPv4 header Check-Sum field is returned in network-order & MUST NOT be re-converted
*                       back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumHdrCalc()  Note #3b').
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktPrepareHdr (NET_BUF        *p_buf,
                                       NET_BUF_HDR    *p_buf_hdr,
                                       CPU_INT16U      ip_hdr_len_tot,
                                       CPU_INT08U      ip_opt_len_tot,
                                       CPU_INT16U      protocol_ix,
                                       NET_IPv4_ADDR   addr_src,
                                       NET_IPv4_ADDR   addr_dest,
                                       NET_IPv4_TOS    TOS,
                                       NET_IPv4_TTL    TTL,
                                       CPU_INT16U      flags,
                                       CPU_INT32U     *p_ip_hdr_opts,
                                       NET_ERR        *p_err)
{
    NET_IPv4_HDR  *p_ip_hdr;
    CPU_INT08U     ip_ver;
    CPU_INT08U     ip_hdr_len;
    CPU_INT16U     ip_id;
    CPU_INT16U     ip_flags;
    CPU_INT16U     ip_frag_offset;
    CPU_INT16U     ip_opt_ix;
    CPU_INT16U     ip_flags_frag_offset;
    CPU_INT16U     ip_chk_sum;
    CPU_BOOLEAN    addr_dest_multicast;


                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr->IP_HdrLen       =  ip_hdr_len_tot;
    p_buf_hdr->IP_HdrIx        =  protocol_ix - p_buf_hdr->IP_HdrLen;

    p_buf_hdr->IP_DataLen      = (CPU_INT16U  ) p_buf_hdr->TotLen;
    p_buf_hdr->IP_DatagramLen  = (CPU_INT16U  ) p_buf_hdr->TotLen;
    p_buf_hdr->TotLen         += (NET_BUF_SIZE) p_buf_hdr->IP_HdrLen;
    p_buf_hdr->IP_TotLen       = (CPU_INT16U  ) p_buf_hdr->TotLen;



                                                                /* ----------------- PREPARE IPv4 HDR ----------------- */
    p_ip_hdr = (NET_IPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];



                                                                /* -------------- PREPARE IP VER/HDR LEN -------------- */
    ip_ver               = NET_IPv4_HDR_VER;
    ip_ver             <<= NET_IPv4_HDR_VER_SHIFT;

    ip_hdr_len           = p_buf_hdr->IP_HdrLen / NET_IPv4_HDR_LEN_WORD_SIZE;
    ip_hdr_len          &= NET_IPv4_HDR_LEN_MASK;

    p_ip_hdr->Ver_HdrLen  = ip_ver | ip_hdr_len;



                                                                /* ------------------ PREPARE IP TOS ------------------ */
    p_ip_hdr->TOS = TOS;



                                                                /* --------------- PREPARE IPv4 TOT LEN --------------- */
    NET_UTIL_VAL_COPY_SET_NET_16(&p_ip_hdr->TotLen, &p_buf_hdr->TotLen);



                                                                /* ----------------- PREPARE IPv4 ID ------------------ */
    NET_IPv4_TX_GET_ID(ip_id);
    NET_UTIL_VAL_COPY_SET_NET_16(&p_ip_hdr->ID, &ip_id);



                                                                /* -------------- PREPARE IPv4 FLAGS/FRAG ------------- */
    ip_flags  = NET_IPv4_HDR_FLAG_NONE;
    ip_flags |= flags;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    ip_flags &= NET_IPv4_HDR_FLAG_MASK;
#endif

    ip_frag_offset       = NET_IPv4_HDR_FRAG_OFFSET_NONE;

    ip_flags_frag_offset = ip_flags | ip_frag_offset;
    NET_UTIL_VAL_COPY_SET_NET_16(&p_ip_hdr->Flags_FragOffset, &ip_flags_frag_offset);



                                                                /* ----------------- PREPARE IPv4 TTL ----------------- */
    if (TTL != NET_IPv4_TTL_NONE) {
        p_ip_hdr->TTL = TTL;
    } else {
#ifdef  NET_MCAST_TX_MODULE_EN
        addr_dest_multicast =  NetIPv4_IsAddrMulticast(addr_dest);
#else
        addr_dest_multicast =  DEF_NO;
#endif
                                                                /* ... set dflt multicast TTL for multicast dest addr.  */
        p_ip_hdr->TTL        = (addr_dest_multicast != DEF_YES) ? NET_IPv4_TTL_DFLT
                                                               : NET_IPv4_TTL_MULTICAST_DFLT;
    }


                                                                /* -------------- PREPARE IPv4 PROTOCOL --------------- */
    switch (p_buf_hdr->ProtocolHdrType) {                       /* Demux IPv4 protocol (see Note #3).                   */
        case NET_PROTOCOL_TYPE_ICMP_V4:
             p_ip_hdr->Protocol = NET_IP_HDR_PROTOCOL_ICMP;
             break;


#ifdef  NET_IGMP_MODULE_EN
        case NET_PROTOCOL_TYPE_IGMP:
             p_ip_hdr->Protocol = NET_IP_HDR_PROTOCOL_IGMP;
             break;
#endif


        case NET_PROTOCOL_TYPE_UDP_V4:
             p_ip_hdr->Protocol = NET_IP_HDR_PROTOCOL_UDP;
             break;


#ifdef  NET_TCP_MODULE_EN
        case NET_PROTOCOL_TYPE_TCP_V4:
             p_ip_hdr->Protocol = NET_IP_HDR_PROTOCOL_TCP;
             break;
#endif


        case NET_PROTOCOL_TYPE_NONE:
        default:                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    p_buf_hdr->ProtocolHdrType    = NET_PROTOCOL_TYPE_IP_V4;    /* Update buf protocol for IPv4.                        */
    p_buf_hdr->ProtocolHdrTypeNet = NET_PROTOCOL_TYPE_IP_V4;



                                                                /* ---------------- PREPARE IPv4 ADDRS ---------------- */
    p_buf_hdr->IP_AddrSrc  = addr_src;
    p_buf_hdr->IP_AddrDest = addr_dest;

    NET_UTIL_VAL_COPY_SET_NET_32(&p_ip_hdr->AddrSrc,  &addr_src);
    NET_UTIL_VAL_COPY_SET_NET_32(&p_ip_hdr->AddrDest, &addr_dest);



                                                                /* ---------------- PREPARE IPv4 OPTS ----------------- */
    if (ip_opt_len_tot > 0) {
        ip_opt_ix = p_buf_hdr->IP_HdrIx + NET_IPv4_HDR_OPT_IX;
        NetBuf_DataWr((NET_BUF    *)p_buf,                      /* See Note #5.                                         */
                      (NET_BUF_SIZE)ip_opt_ix,
                      (NET_BUF_SIZE)ip_opt_len_tot,
                      (CPU_INT08U *)p_ip_hdr_opts,
                      (NET_ERR    *)p_err);
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        if (*p_err != NET_BUF_ERR_NONE) {
             return;
        }
#endif
    }


                                                                /* --------------- PREPARE IPv4 CHK SUM --------------- */
                                                                /* See Note #6.                                         */
    NET_UTIL_VAL_SET_NET_16(&p_ip_hdr->ChkSum, 0x0000u);        /* Clr  chk sum (see Note #6b).                         */
                                                                /* Calc chk sum.                                        */
#ifdef  NET_IPV4_CHK_SUM_OFFLOAD_TX
    ip_chk_sum = 0u;
#else
    ip_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *)p_ip_hdr,
                                                   (CPU_INT16U)ip_hdr_len_tot,
                                                   (NET_ERR  *)p_err);
    if (*p_err != NET_UTIL_ERR_NONE) {
         return;
    }
#endif

    NET_UTIL_VAL_COPY_16(&p_ip_hdr->ChkSum, &ip_chk_sum);       /* Copy chk sum in net order (see Note #6c).            */



   *p_err = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetIPv4_TxPktDatagram()
*
* Description : (1) Transmit IPv4 packet datagram :
*
*                   (a) Select next-route IPv4 address
*                   (b) Transmit IPv4 packet datagram via next IPv4 address route :
*
*                       (1) Destination is this host                Send to Loopback Interface
*                           (A) Configured host address
*                           (B) Localhost       address
*
*                       (2) Limited Broadcast                       Send to Network  Interface Transmit
*                       (3) Local   Host                            Send to Network  Interface Transmit
*                       (4) Remote  Host                            Send to Network  Interface Transmit
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit IPv4 packet.
*               -----       Argument checked   in NetIPv4_Tx(),
*                                                 NetIPv4_ReTx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_Tx(),
*                                                 NetIPv4_ReTx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetIPv4_TxPktDatagramRouteSel() : -
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid IPv4 host    address.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 gateway address.
*                               NET_IPv4_ERR_TX_DEST_INVALID        Invalid transmit destination.
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
* Caller(s)   : NetIPv4_ReTxPkt(),
*               NetIPv4_TxPkt().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktDatagram (NET_BUF      *p_buf,
                                     NET_BUF_HDR  *p_buf_hdr,
                                     NET_ERR      *p_err)
{
                                                                /* --------------- SEL NEXT-ROUTE ADDR ---------------- */
    NetIPv4_TxPktDatagramRouteSel(p_buf_hdr, p_err);


    switch (*p_err) {                                           /* --------------- TX IPv4 PKT DATAGRAM --------------- */
        case NET_IPv4_ERR_TX_DEST_LOCAL_HOST:
             p_buf_hdr->IF_NbrTx = NET_IF_NBR_LOCAL_HOST;
             NetIF_Tx(p_buf, p_err);
             break;


        case NET_IPv4_ERR_TX_DEST_BROADCAST:
        case NET_IPv4_ERR_TX_DEST_MULTICAST:
        case NET_IPv4_ERR_TX_DEST_HOST_THIS_NET:
        case NET_IPv4_ERR_TX_DEST_DFLT_GATEWAY:
             p_buf_hdr->IF_NbrTx = p_buf_hdr->IF_Nbr;
             NetIF_Tx(p_buf, p_err);
             break;


        case NET_IPv4_ERR_TX_DEST_INVALID:
        case NET_IPv4_ERR_INVALID_ADDR_HOST:
        case NET_IPv4_ERR_INVALID_ADDR_GATEWAY:
        default:
             return;
    }
}


/*
*********************************************************************************************************
*                                    NetIPv4_TxPktDatagramRouteSel()
*
* Description : (1) Configure  next-route IPv4 address for transmit IPv4 packet datagram :
*
*                   (a) Select next-route IPv4 address :                See Note #3
*                       (1) Destination is this host :
*                           (A) Configured host address
*                           (B) Localhost       address                 See RFC #1122, Section 3.2.1.3.(g)
*                       (2) Link-local                                  See Note #3c
*                       (3) Limited Broadcast                           See Note #3b2a
*                       (4) Multicast                                   See Note #3b2a
*                       (5) Local  Net Host                             See Note #3b1b
*                       (6) Remote Net Host                             See Note #3b1c
*
*                   (b) Configure next-route IPv4 address in network-order.
*
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header of IPv4 transmit packet.
*               ---------   Argument validated in NetIPv4_Tx(),
*                                                 NetIPv4_ReTx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_TX_DEST_LOCAL_HOST         Destination is a local host address.
*                               NET_IPv4_ERR_TX_DEST_BROADCAST          Limited broadcast on local  network.
*                               NET_IPv4_ERR_TX_DEST_MULTICAST          Multicast         on local  network.
*                               NET_IPv4_ERR_TX_DEST_HOST_THIS_NET      Destination host  on local  network.
*                               NET_IPv4_ERR_TX_DEST_DFLT_GATEWAY       Destination host  on remote network.
*                               NET_IPv4_ERR_TX_DEST_INVALID            Invalid IPv4 destination address.
*
*                               NET_IPv4_ERR_INVALID_ADDR_HOST          Invalid IPv4 host        address.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY       Invalid IPv4 gateway     address.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_TxPktDatagram().
*
* Note(s)     : (2) See 'net_ipv4.h  IPv4 ADDRESS DEFINES  Notes #2 & #3' for supported IPv4 addresses.
*
*               (3) (a) (1) RFC #1122, Section 3.3.1 states that "the IP layer chooses the correct next hop
*                           for each datagram it sends" :
*
*                           (A) "If the destination is on a connected network, the datagram is sent directly
*                                to the destination host;" ...
*                           (B) "Otherwise, it has to be routed to a gateway on a connected network."
*
*                       (2) However, the IPv4 layer should route datagrams destined to any internal host
*                           to IPv4 receive processing.
*
*                           (A) RFC #950, Section 2.2 states that :
*
*                                   "IF ip_net_number(dg.ip_dest) = ip_net_number(my_ip_addr)
*                                       THEN send_dg_locally(dg, dg.ip_dest)"
*
*                           (B) RFC #1122, Section 3.2.1.3.(g) states that the "internal host loopback
*                               address ... MUST NOT appear outside a host".
*
*                               (1) However, this does NOT prevent the host loopback address from being
*                                   used as an IP packet's source address as long as BOTH the packet's
*                                   source AND destination addresses are internal host addresses, either
*                                   a configured host IPv4 address or any host loopback address.
*
*                   (b) RFC #1122, Section 3.3.1.1 states that "to decide if the destination is on a
*                       connected network, the following algorithm MUST be used" :
*
*                       (1) (b) "If the IPv4 destination address bits extracted by the address mask match the
*                                IP source address bits extracted by the same mask, then the destination is
*                                on the corresponding connected network, and the datagram is ... transmitted
*                                directly to the destination host."
*
*                           (c) "If not, then the destination is accessible only through a gateway."
*
*                               (1) However, "the host IPv4 layer MUST operate correctly in a minimal network
*                                   environment, and in particular, when there are no gateways."
*
*                       (2) (a) "For a limited broadcast or a multicast address, simply pass the datagram
*                                to the link layer for the appropriate interface."
*
*                           (b) "For a (network or subnet) directed broadcast, the datagram can use the
*                                standard routing algorithms."
*
*                               (1) RFC #950, Section 2.1 'Special Addresses' confirms that the broadcast
*                                   address for subnetted IPv4 addresses is still "the address of all ones".
*
*                   (c) (1) RFC #3927, Section 2.6.2 states that :
*
*                           (A) (1) "If ... the host ... send[s] [a] packet with an IPv4 Link-Local source
*                                    address ... [to] a unicast ... destination address ... outside the
*                                    169.254/16 prefix, ... then it MUST ARP for the destination address
*                                    and then send its packet ... directly to its destination on the same
*                                    physical link."
*
*                               (2) "The host MUST NOT send the packet to any router for forwarding."
*
*                           (B) (1) (a) "If the destination address is in the 169.254/16 prefix ... then
*                                        the sender MUST ARP for the destination address and then send its
*                                        packet directly to the destination on the same physical link."
*
*                                   (b) "The host MUST NOT send a packet with an IPv4 Link-Local destination
*                                        address to any router for forwarding."
*
*                               (2) "This MUST be done whether the interface is configured with a Link-Local
*                                    or a routable IPv4 address."
*
*                       (2) (A) RFC #3927, Section 2.8 reiterates that "the non-forwarding rule means that
*                               hosts may assume that all 169.254/16 destination addresses are 'on-link'
*                               and directly reachable".
*
*                           (B) RFC #3927, Section 2.6.2 concludes that "in the case of a device with a
*                               single interface and only an Link-Local IPv4 address, this requirement
*                               can be paraphrased as 'ARP for everything'".
*
*                       (3) (A) RFC #3927, Section 2.8   states that "the 169.254/16 address prefix MUST
*                               NOT be subnetted".
*
*                           (B) RFC #3927, Section 2.6.2 states that "169.254.255.255 ... is the broadcast
*                               address for the Link-Local prefix".
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktDatagramRouteSel (NET_BUF_HDR  *p_buf_hdr,
                                             NET_ERR      *p_err)
{
    NET_IPv4_ADDRS  *p_ip_addrs;
    NET_IF_NBR       if_nbr;
    NET_IPv4_ADDR    addr_src;
    NET_IPv4_ADDR    addr_dest;
    CPU_BOOLEAN      addr_cfgd;


                                                                        /* ----------- GET IPv4 TX PKT ADDRS ---------- */
    addr_src  = p_buf_hdr->IP_AddrSrc;
    addr_dest = p_buf_hdr->IP_AddrDest;

    if_nbr    = p_buf_hdr->IF_Nbr;
    p_ip_addrs = NetIPv4_GetAddrsHostCfgdOnIF(addr_src, if_nbr);
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {
        if ((p_ip_addrs->AddrHost               == NET_IPv4_ADDR_NONE) ||
            (p_ip_addrs->AddrHostSubnetMask     == NET_IPv4_ADDR_NONE) ||
            (p_ip_addrs->AddrHostSubnetMaskHost == NET_IPv4_ADDR_NONE) ||
            (p_ip_addrs->AddrHostSubnetNet      == NET_IPv4_ADDR_NONE)) {
            *p_err = NET_IPv4_ERR_INVALID_ADDR_HOST;
             return;
        }
    }
#endif

                                                                        /* Perform IPv4 routing/fwd'ing alg here?       */


                                                                        /* ---------- CHK CFG'D HOST ADDR(S) ---------- */
                                                                        /* Chk cfg'd host addr(s)   [see Note #3a2A].   */
    addr_cfgd = NetIPv4_IsAddrHostCfgdHandler(addr_dest);
    if (addr_cfgd == DEF_YES) {
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestThisHostCtr);
        p_buf_hdr->IP_AddrNextRoute = addr_dest;
       *p_err                       = NET_IPv4_ERR_TX_DEST_LOCAL_HOST;

                                                                        /* ----------- CHK LOCALHOST ADDRS ------------ */
                                                                        /* Chk localhost src   addr (see Note #3a2B1).  */
    } else if ((addr_src  & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                            NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalHostCtr);
        p_buf_hdr->IP_AddrNextRoute = addr_dest;
       *p_err                       = NET_IPv4_ERR_TX_DEST_LOCAL_HOST;
                                                                        /* Chk localhost dest  addr (see Note #3a2B).   */
    } else if ((addr_dest & NET_IPv4_ADDR_LOCAL_HOST_MASK_NET) ==
                            NET_IPv4_ADDR_LOCAL_HOST_NET     ) {
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalHostCtr);
        p_buf_hdr->IP_AddrNextRoute = addr_dest;
       *p_err                       = NET_IPv4_ERR_TX_DEST_LOCAL_HOST;

                                                                        /* ----------- CHK LINK-LOCAL ADDRS ----------- */
    } else if (((addr_src  & NET_IPv4_ADDR_LOCAL_LINK_MASK_NET) ==      /* Chk link-local src  addr (see Note #3c1A) OR */
                             NET_IPv4_ADDR_LOCAL_LINK_NET     ) ||
               ((addr_dest & NET_IPv4_ADDR_LOCAL_LINK_MASK_NET) ==      /* ... link-local dest addr (see Note #3c1B).   */
                             NET_IPv4_ADDR_LOCAL_LINK_NET     )) {
                                                                        /* Chk link-local broadcast (see Note #3c3B).   */
        if ((addr_dest               & NET_IPv4_ADDR_LOCAL_LINK_MASK_HOST) ==
            (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_LOCAL_LINK_MASK_HOST)) {
             NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestBcastCtr);
             DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_BROADCAST);
             p_buf_hdr->IP_AddrNextRoute = addr_dest;
            *p_err                       = NET_IPv4_ERR_TX_DEST_BROADCAST;

        } else {
            p_buf_hdr->IP_AddrNextRoute = addr_dest;
           *p_err                       = NET_IPv4_ERR_TX_DEST_HOST_THIS_NET;
        }

        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalLinkCtr);
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalNetCtr);


                                                                        /* ----------- CHK LIM'D BROADCAST ------------ */
    } else if (addr_dest == NET_IPv4_ADDR_BROADCAST) {                  /* Chk lim'd broadcast (see Note #3b2a).        */
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestBcastCtr);
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalNetCtr);
        DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_BROADCAST);
        p_buf_hdr->IP_AddrNextRoute = addr_dest;
       *p_err                       = NET_IPv4_ERR_TX_DEST_BROADCAST;


#ifdef  NET_MCAST_MODULE_EN                                             /* -------------- CHK MULTICAST --------------- */
                                                                        /* Chk multicast       (see Note #3b2a).        */
    } else if ((addr_dest & NET_IPv4_ADDR_CLASS_D_MASK) == NET_IPv4_ADDR_CLASS_D) {
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestMcastCtr);
        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalNetCtr);
        DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_MULTICAST);
        p_buf_hdr->IP_AddrNextRoute = addr_dest;
       *p_err                       = NET_IPv4_ERR_TX_DEST_MULTICAST;
#endif

                                                                        /* ------------- CHK REMOTE HOST -------------- */
    } else if (p_ip_addrs == (NET_IPv4_ADDRS *)0) {
       *p_err = NET_IPv4_ERR_INVALID_ADDR_HOST;
        return;

                                                                        /* ------------- CHK LOCAL  NET --------------- */
                                                                        /* Chk local subnet           (see Note #3b1b). */
    } else if ((addr_dest & p_ip_addrs->AddrHostSubnetMask) ==
                            p_ip_addrs->AddrHostSubnetNet ) {
                                                                        /* Chk local subnet broadcast (see Note #3b2b1).*/
        if ((addr_dest               & p_ip_addrs->AddrHostSubnetMaskHost) ==
            (NET_IPv4_ADDR_BROADCAST & p_ip_addrs->AddrHostSubnetMaskHost)) {
             NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestBcastCtr);
             DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_TX_BROADCAST);
             p_buf_hdr->IP_AddrNextRoute = addr_dest;
            *p_err                       = NET_IPv4_ERR_TX_DEST_BROADCAST;

        } else {
             p_buf_hdr->IP_AddrNextRoute = addr_dest;
            *p_err                       = NET_IPv4_ERR_TX_DEST_HOST_THIS_NET;
        }

        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestLocalNetCtr);


                                                                        /* ------------- CHK REMOTE  NET -------------- */
    } else {

                                                                        /* Tx to remote net (see Note #3b1c) ...        */
        if ((addr_dest & NET_IPv4_ADDR_CLASS_A_MASK) == NET_IPv4_ADDR_CLASS_A) {
            if ((addr_dest             & NET_IPv4_ADDR_CLASS_A_MASK_HOST) ==
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_A_MASK_HOST)) {
                 NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestBcastCtr);
            }

        } else if ((addr_dest & NET_IPv4_ADDR_CLASS_B_MASK) == NET_IPv4_ADDR_CLASS_B) {
            if ((addr_dest             & NET_IPv4_ADDR_CLASS_B_MASK_HOST) ==
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_B_MASK_HOST)) {
                 NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestBcastCtr);
            }

        } else if ((addr_dest & NET_IPv4_ADDR_CLASS_C_MASK) == NET_IPv4_ADDR_CLASS_C) {
            if ((addr_dest             & NET_IPv4_ADDR_CLASS_C_MASK_HOST) ==
                (NET_IPv4_ADDR_BROADCAST & NET_IPv4_ADDR_CLASS_C_MASK_HOST)) {
                 NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestBcastCtr);
            }

        } else {                                                        /* Discard invalid addr class (see Note #2).    */
            NET_CTR_ERR_INC(Net_ErrCtrs.IPv4.TxInvDestCtr);
           *p_err = NET_IPv4_ERR_TX_DEST_INVALID;
            return;
        }


        if (p_ip_addrs != (NET_IPv4_ADDRS *)0) {
            if (p_ip_addrs->AddrDfltGateway == NET_IPv4_ADDR_NONE) {     /* If dflt gateway NOT cfg'd, ...               */
               *p_err = NET_IPv4_ERR_INVALID_ADDR_GATEWAY;               /* ... rtn err (see Note #3b1c1).               */
                return;
            }
        }


        NET_CTR_STAT_INC(Net_StatCtrs.IPv4.TxDestRemoteNetCtr);
        p_buf_hdr->IP_AddrNextRoute = p_ip_addrs->AddrDfltGateway;      /* ... via dflt gateway (see Note #3a1B).       */
       *p_err                       = NET_IPv4_ERR_TX_DEST_DFLT_GATEWAY;
    }


                                                                        /* ---- CFG IPv4 NEXT ROUTE NET-ORDER ADDR ---- */
    p_buf_hdr->IP_AddrNextRouteNetOrder = NET_UTIL_HOST_TO_NET_32(p_buf_hdr->IP_AddrNextRoute);
}


/*
*********************************************************************************************************
*                                        NetIPv4_TxPktDiscard()
*
* Description : On any IPv4 transmit packet error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_ReTx(),
*               NetIPv4_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv4_TxPktDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.IPv4.TxPktDisCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)pctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                           NetIPv4_ReTxPkt()
*
* Description : (1) Prepare & re-transmit IPv4 packet :
*
*                   (a) Prepare     IPv4 header
*                   (b) Re-transmit IPv4 packet datagram
*
*
* Argument(s) : p_buf       Pointer to network buffer to re-transmit IPv4 packet.
*               -----       Argument checked   in NetIPv4_ReTx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_ReTx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetIPv4_ReTxPktPrepareHdr() : -
*                               NET_UTIL_ERR_NULL_PTR               Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE              Check-sum passed a zero size.
*
*                                                                   -- RETURNED BY NetIPv4_TxPktDatagram() : --
*                               NET_IF_ERR_NONE                     Packet successfully transmitted.
*                               NET_ERR_IF_LOOPBACK_DIS             Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN                Network  interface link state down (i.e.
*                                                                       NOT available for receive or transmit).
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      Invalid IPv4 host    address.
*                               NET_IPv4_ERR_INVALID_ADDR_GATEWAY   Invalid IPv4 gateway address.
*                               NET_IPv4_ERR_TX_DEST_INVALID        Invalid transmit destination.
*                               NET_ERR_TX                          Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_ReTx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIPv4_ReTxPkt (NET_BUF      *p_buf,
                               NET_BUF_HDR  *p_buf_hdr,
                               NET_ERR      *p_err)
{
                                                                /* ----------------- PREPARE IPv4 HDR ----------------- */
    NetIPv4_ReTxPktPrepareHdr(p_buf,
                            p_buf_hdr,
                            p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         return;
    }

                                                                /* ------------- RE-TX IPv4 PKT DATAGRAM -------------- */
    NetIPv4_TxPktDatagram(p_buf, p_buf_hdr, p_err);
}


/*
*********************************************************************************************************
*                                      NetIPv4_ReTxPktPrepareHdr()
*
* Description : (1) Prepare IPv4 header for re-transmit IPv4 packet :
*
*                   (a) Update network buffer's protocol & length controls
*
*                   (b) (1) Prepare the re-transmit packet's following IPv4 header fields :
*
*                           (A) Identification  (ID)                        See Note #2
*                           (B) Check-Sum                                   See Note #3
*
*                       (2) Assumes the following IP header fields are already validated/prepared &
*                           have NOT been modified :
*
*                           (A) Version
*                           (B) Header Length
*                           (C) Type of Service (TOS)
*                           (D) Total  Length
*                           (E) Flags
*                           (F) Fragment Offset
*                           (G) Time-to-Live    (TTL)
*                           (H) Protocol
*                           (I) Source      Address
*                           (J) Destination Address
*                           (K) Options
*
*                   (c) Convert the following IPv4 header fields from host-order to network-order :
*
*                       (1) Identification (ID)
*                       (2) Check-Sum                                       See Note #3c
*
*
* Argument(s) : p_buf       Pointer to network buffer to transmit IPv4 packet.
*               -----       Argument checked   in NetIPv4_Tx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIPv4_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE               IPv4 header successfully prepared.
*
*                                                               - RETURNED BY NetUtil_16BitOnesCplChkSumHdrCalc() : -
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*
* Return(s)   : none.
*
* Caller(s)   : NetIPv4_ReTxPkt().
*
* Note(s)     : (2) RFC #1122, Section 3.2.1.5 states that "some Internet protocol experts have maintained
*                   that when a host sends an identical copy of an earlier datagram, the new copy should
*                   contain the same Identification value as the original ... However, ... we believe that
*                   retransmitting the same Identification field is not useful".
*
*               (3) (a) IP header Check-Sum MUST be calculated AFTER the entire IPv4 header has been prepared.
*                       In addition, ALL multi-octet words are converted from host-order to network-order
*                       since "the sum of 16-bit integers can be computed in either byte order" [RFC #1071,
*                       Section 2.(B)].
*
*                   (b) IPv4 header Check-Sum field MUST be cleared to '0' BEFORE the IPv4 header Check-Sum is
*                       calculated (see RFC #791, Section 3.1 'Header Checksum').
*
*                   (c) The IPv4 header Check-Sum field is returned in network-order & MUST NOT be re-converted
*                       back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumHdrCalc()  Note #3b').
*********************************************************************************************************
*/

static  void  NetIPv4_ReTxPktPrepareHdr (NET_BUF      *p_buf,
                                         NET_BUF_HDR  *p_buf_hdr,
                                         NET_ERR      *p_err)
{
    NET_IPv4_HDR  *p_ip_hdr;
    CPU_INT16U     ip_id;
    CPU_INT16U     ip_hdr_len_tot;
    CPU_INT16U     ip_chk_sum;


                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr->ProtocolHdrType    =  NET_PROTOCOL_TYPE_IP_V4;   /* Update buf protocol for IPv4.                        */
    p_buf_hdr->ProtocolHdrTypeNet =  NET_PROTOCOL_TYPE_IP_V4;
                                                                /* Reset tot len for re-tx.                             */
    p_buf_hdr->TotLen             = (NET_BUF_SIZE)p_buf_hdr->IP_TotLen;


                                                                /* ----------------- PREPARE IPv4 HDR ----------------- */
    p_ip_hdr = (NET_IPv4_HDR *)&p_buf->DataPtr[p_buf_hdr->IP_HdrIx];


                                                                /* ----------------- PREPARE IPv4 ID ------------------ */
    NET_IPv4_TX_GET_ID(ip_id);                                  /* Get new IP ID (see Note #2).                         */
    NET_UTIL_VAL_COPY_SET_NET_16(&p_ip_hdr->ID, &ip_id);

                                                                /* --------------- PREPARE IPv4 CHK SUM --------------- */
                                                                /* See Note #3.                                         */
    NET_UTIL_VAL_SET_NET_16(&p_ip_hdr->ChkSum, 0x0000u);        /* Clr  chk sum (see Note #3b).                         */
    ip_hdr_len_tot = p_buf_hdr->IP_HdrLen;
#ifdef  NET_IPV4_CHK_SUM_OFFLOAD_TX                          /* Calc chk sum.                                        */
    ip_chk_sum     = 0u;
#else
    ip_chk_sum     = NetUtil_16BitOnesCplChkSumHdrCalc((void     *)p_ip_hdr,
                                                       (CPU_INT16U)ip_hdr_len_tot,
                                                       (NET_ERR  *)p_err);
    if (*p_err != NET_UTIL_ERR_NONE) {
         return;
    }
#endif

    NET_UTIL_VAL_COPY_16(&p_ip_hdr->ChkSum, &ip_chk_sum);       /* Copy chk sum in net order (see Note #3c).            */


   (void)&ip_hdr_len_tot;

   *p_err = NET_IPv4_ERR_NONE;
}

/*
*********************************************************************************************************
*                                      NetIPv4_IsAddrHostHandler()
*
* Description : (1) Validate an IPv4 address as an IPv4 host address :
*
*                   (a) A configured IPv4 host address (on an enabled interface)
*                   (b) A 'Localhost'     IPv4 address
*
*
* Argument(s) : addr        IPv4 address to validate (see Note #3).
*
* Return(s)   : DEF_YES, if IPv4 address is one of the host's IP addresses.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_IsAddrHost(),
*               NetIPv4_RxPktValidate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIPv4_IsAddrHostHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetIPv4_IsAddrHost()  Note #2'.
*
*               (3) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetIPv4_IsAddrHostHandler (NET_IPv4_ADDR  addr)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  addr_host;


    if_nbr    =  NetIPv4_GetAddrHostIF_Nbr(addr);
    addr_host = (if_nbr != NET_IF_NBR_NONE) ? DEF_YES : DEF_NO;

    return (addr_host);
}


/*
*********************************************************************************************************
*                                    NetIPv4_IsAddrHostCfgdHandler()
*
* Description : Validate an IPv4 address as a configured IPv4 host address on an enabled interface.
*
* Argument(s) : addr        IPv4 address to validate (see Note #2).
*
* Return(s)   : DEF_YES, if IPv4 address is one of the host's configured IPv4 addresses.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIPv4_IsAddrHostCfgd(),
*               NetIPv4_CfgAddrAdd(),
*               NetIPv4_CfgAddrAddDynamic(),
*               NetIPv4_RxPktValidate(),
*               NetIPv4_TxPktValidate(),
*               NetIPv4_TxPktDatagramRouteSel().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (1) NetIPv4_IsAddrHostCfgdHandler() is called by network protocol suite function(s) &
*                   MUST be called with the global network lock already acquired.
*
*                   See also 'NetIPv4_IsAddrHostCfgd()  Note #1'.
*
*               (2) IPv4 address MUST be in host-order.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetIPv4_IsAddrHostCfgdHandler (NET_IPv4_ADDR  addr)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  addr_host;


    if_nbr    =  NetIPv4_GetAddrHostCfgdIF_Nbr(addr);
    addr_host = (if_nbr != NET_IF_NBR_NONE) ? DEF_YES : DEF_NO;

    return (addr_host);
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
*                                     NetIPv4_GetHostAddrProtocol()
*
* Description : Get an interface's IPv4 protocol address(s) [see Note #1].
*
* Argument(s) : if_nbr                      Interface number to get IPv4 protocol address(s).
*
*               p_addr_protocol_tbl         Pointer to a protocol address table that will receive the protocol
*                                               address(s) in network-order for this interface.
*
*               p_addr_protocol_tbl_qty     Pointer to a variable to ... :
*
*                                               (a) Pass the size of the protocol address table, in number of
*                                                       protocol address(s), pointed to by 'p_addr_protocol_tbl'.
*                                               (b) (1) Return the actual number of IPv4 protocol address(s),
*                                                           if NO error(s);
*                                                   (2) Return 0, otherwise.
*
*                                           See also Note #2a.
*
*               p_addr_protocol_len         Pointer to a variable to ... :
*
*                                               (a) Pass the length of the protocol address table address(s),
*                                                       in octets.
*                                               (b) (1) Return the actual length of IPv4 protocol address(s),
*                                                           in octets, if NO error(s);
*                                                   (2) Return 0, otherwise.
*
*                                           See also Note #2b.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Interface's IPv4 protocol address(s)
*                                                                       successfully returned.
*                               NET_ERR_FAULT_NULL_PTR               Argument(s) passed a NULL pointer.
*                               NET_IPv4_ERR_INVALID_ADDR_LEN       Invalid protocol address length.
*                               NET_IPv4_ERR_ADDR_TBL_SIZE          Invalid protocol address table size.
*
*                                                                   - RETURNED BY NetIPv4_GetAddrHostHandler() : -
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*                               NET_IP_ERR_ADDR_CFG_IN_PROGRESS     Interface in dynamic address configuration
*                                                                       initialization state.
*
* Return(s)   : none.
*
* Caller(s)   : NetMgr_GetHostAddrProtocol().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) IPv4 protocol address(s) returned in network-order.
*
*               (2) (a) Since 'p_addr_protocol_tbl_qty' argument is both an input & output argument
*                       (see 'Argument(s) : p_addr_protocol_tbl_qty'), ... :
*
*                       (1) Its input value SHOULD be validated prior to use; ...
*
*                           (A) In the case that the 'p_addr_protocol_tbl_qty' argument is passed a null
*                               pointer, NO input value is validated or used.
*
*                           (B) The protocol address table's size MUST be greater than or equal to each
*                               interface's maximum number of IPv4 protocol addresses times the size of
*                               an IPv4 protocol address :
*
*                               (1) (p_addr_protocol_tbl_qty *  >=  [ NET_IPv4_CFG_IF_MAX_NBR_ADDR *
*                                    p_addr_protocol_len    )         sizeof(NET_IPv4_ADDR)      ]
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
*                               size of an IPv4 protocol address :
*
*                               (1) p_addr_protocol_len  >=  sizeof(NET_IPv4_ADDR)
*
*                       (2) While its output value MUST be initially configured to return a default value
*                           PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/

void  NetIPv4_GetHostAddrProtocol (NET_IF_NBR   if_nbr,
                                   CPU_INT08U  *p_addr_protocol_tbl,
                                   CPU_INT08U  *p_addr_protocol_tbl_qty,
                                   CPU_INT08U  *p_addr_protocol_len,
                                   NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT16U          addr_protocol_tbl_size;
    CPU_INT16U         addr_ip_tbl_size;
    CPU_INT08U         addr_protocol_tbl_qty;
    CPU_INT08U         addr_protocol_len;
#endif
    CPU_INT08U        *p_addr_protocol;
    NET_IPv4_ADDR     *p_addr_ip;
    NET_IPv4_ADDR      addr_ip_tbl[NET_IPv4_CFG_IF_MAX_NBR_ADDR];
    NET_IP_ADDRS_QTY   addr_ip_tbl_qty;
    NET_IP_ADDRS_QTY   addr_ix;
    CPU_INT08U         addr_ip_len;


                                                                /* ------------ VALIDATE PROTOCOL ADDR TBL ------------ */
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol_tbl_qty == (CPU_INT08U *)0) {           /* See Note #2a1A.                                      */
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

     addr_protocol_tbl_qty = *p_addr_protocol_tbl_qty;
#endif
   *p_addr_protocol_tbl_qty =  0u;                              /* Cfg dflt tbl qty for err  (see Note #2a2).           */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol_len == (CPU_INT08U *)0) {               /* See Note #2b1A.                                      */
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

     addr_protocol_len = *p_addr_protocol_len;
#endif
   *p_addr_protocol_len =  0u;                                  /* Cfg dflt addr len for err (see Note #2b2).           */
    addr_ip_len        =  sizeof(NET_IPv4_ADDR);

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_addr_protocol_tbl == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (addr_protocol_len < addr_ip_len) {                      /* Validate protocol addr len      (see Note #2b1B).    */
       *p_err = NET_IPv4_ERR_INVALID_ADDR_LEN;
        return;
    }

    addr_protocol_tbl_size = addr_protocol_tbl_qty        * addr_protocol_len;
    addr_ip_tbl_size       = NET_IPv4_CFG_IF_MAX_NBR_ADDR * addr_ip_len;
    if (addr_protocol_tbl_size < addr_ip_tbl_size) {            /* Validate protocol addr tbl size (see Note #2a1B).    */
       *p_err = NET_IPv4_ERR_ADDR_TBL_SIZE;
        return;
    }
#endif


                                                                /* ------------- GET IPv4 PROTOCOL ADDRS -------------- */
    addr_ip_tbl_qty = sizeof(addr_ip_tbl) / sizeof(NET_IPv4_ADDR);
   (void)NetIPv4_GetAddrHostHandler((NET_IF_NBR        ) if_nbr,
                                    (NET_IPv4_ADDR    *)&addr_ip_tbl[0],
                                    (NET_IP_ADDRS_QTY *)&addr_ip_tbl_qty,
                                    (NET_ERR          *) p_err);
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
        case NET_IPv4_ERR_ADDR_NONE_AVAIL:
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
        case NET_IPv4_ERR_ADDR_TBL_SIZE:
        default:
             return;
    }

    addr_ix        =  0u;
    p_addr_ip       = &addr_ip_tbl[addr_ix];
    p_addr_protocol = &p_addr_protocol_tbl[addr_ix];
    while (addr_ix <  addr_ip_tbl_qty) {                        /* Rtn all IPv4 protocol addr(s) ...                    */
        NET_UTIL_VAL_COPY_SET_NET_32(p_addr_protocol, p_addr_ip); /* ... in net-order (see Note #1).                    */
        p_addr_protocol += addr_ip_len;
        p_addr_ip++;
        addr_ix++;
    }

                                                                /* Rtn nbr & len of IPv4 protocol addr(s).              */
   *p_addr_protocol_tbl_qty = (CPU_INT08U)addr_ip_tbl_qty;
   *p_addr_protocol_len     = (CPU_INT08U)addr_ip_len;


   *p_err = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetIPv4_GetAddrProtocolIF_Nbr()
*
* Description : (1) Get the interface number for a host's IPv4 protocol address :
*
*                   (a) A configured IPv4 host address (on an enabled interface)
*                   (b) A 'Localhost'          address
*                   (c) An           IPv4 host initialization address
*
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #2).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   IPv4 protocol address's interface number
*                                                                       successfully returned.
*                               NET_ERR_FAULT_NULL_PTR               Argument 'p_addr_protocol' passed a NULL
*                                                                       pointer.
*                               NET_IPv4_ERR_INVALID_ADDR_LEN       Invalid IPv4 protocol address length.
*                               NET_IPv4_ERR_INVALID_ADDR_HOST      IPv4 protocol address NOT used by host.
*
* Return(s)   : Interface number for IPv4 protocol address, if configured on this host.
*
*               Interface number of  IPv4 protocol address
*                   in address initialization,              if available.
*
*               NET_IF_NBR_LOCAL_HOST,                      for a localhost address.
*
*               NET_IF_NBR_NONE,                            otherwise.
*
* Caller(s)   : NetMgr_GetHostAddrProtocolIF_Nbr().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (2) Protocol address MUST be in network-order.
*********************************************************************************************************
*/

NET_IF_NBR  NetIPv4_GetAddrProtocolIF_Nbr (CPU_INT08U  *p_addr_protocol,
                                           CPU_INT08U   addr_protocol_len,
                                           NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv4_ADDR  addr_ip;
    NET_IF_NBR     if_nbr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (NET_IF_NBR_NONE);
    }

    addr_ip_len = sizeof(NET_IPv4_ADDR);
    if (addr_protocol_len != addr_ip_len) {
       *p_err =  NET_IPv4_ERR_INVALID_ADDR_LEN;
        return (NET_IF_NBR_NONE);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* ------- GET IPv4 PROTOCOL ADDR's IF NBR -------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&addr_ip, p_addr_protocol);
    if_nbr =  NetIPv4_GetAddrHostIF_Nbr(addr_ip);
   *p_err   = (if_nbr != NET_IF_NBR_NONE) ? NET_IPv4_ERR_NONE
                                         : NET_IPv4_ERR_INVALID_ADDR_HOST;

    return (if_nbr);
}


/*
*********************************************************************************************************
*                                     NetIPv4_IsValidAddrProtocol()
*
* Description : Validate an IPv4 protocol address.
*
* Argument(s) : p_addr_protocol      Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if IPv4 protocol address valid.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsValidAddrProtocol().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) IPv4 protocol address MUST be in network-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsValidAddrProtocol (CPU_INT08U  *p_addr_protocol,
                                          CPU_INT08U   addr_protocol_len)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv4_ADDR  addr_ip;
    CPU_BOOLEAN    valid;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        return (DEF_NO);
    }

    addr_ip_len = sizeof(NET_IPv4_ADDR);
    if (addr_protocol_len != addr_ip_len) {
        return (DEF_NO);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* --------- VALIDATE IPv4 PROTOCOL ADDR ---------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&addr_ip, p_addr_protocol);
    valid = NetIPv4_IsValidAddrHost(addr_ip);

    return (valid);
}


/*
*********************************************************************************************************
*                                         NetIPv4_IsAddrInit()
*
* Description : Validate an IPv4 protocol address as the initialization address.
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if IPv4 protocol address is the initialization address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsAddrProtocolInit().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) IPv4 protocol address MUST be in network-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrInit (CPU_INT08U  *p_addr_protocol,
                                 CPU_INT08U   addr_protocol_len)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv4_ADDR  addr_ip;
    CPU_BOOLEAN    addr_init;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        return (DEF_NO);
    }

    addr_ip_len = sizeof(NET_IPv4_ADDR);
    if (addr_protocol_len != addr_ip_len) {
        return (DEF_NO);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* ------- VALIDATE IPv4 PROTOCOL INIT ADDR ------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&addr_ip, p_addr_protocol);
    addr_init = NetIPv4_IsAddrThisHost(addr_ip);

    return (addr_init);
}


/*
*********************************************************************************************************
*                                   NetIPv4_IsAddrProtocolMulticast()
*
* Description : Validate an IPv4 protocol address as a multicast address.
*
* Argument(s) : p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length  of protocol address (in octets).
*
* Return(s)   : DEF_YES, if IPv4 protocol address is a multicast address.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsAddrProtocolMulticast().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) IPv4 protocol address MUST be in network-order.
*********************************************************************************************************
*/

#ifdef  NET_MCAST_TX_MODULE_EN
CPU_BOOLEAN  NetIPv4_IsAddrProtocolMulticast (CPU_INT08U  *p_addr_protocol,
                                              CPU_INT08U   addr_protocol_len)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U     addr_ip_len;
#endif
    NET_IPv4_ADDR  addr_ip;
    CPU_BOOLEAN    addr_multicast;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
        return (DEF_NO);
    }

    addr_ip_len = sizeof(NET_IPv4_ADDR);
    if (addr_protocol_len != addr_ip_len) {
        return (DEF_NO);
    }
#else
   (void)&addr_protocol_len;                                        /* Prevent 'variable unused' compiler warning.      */
#endif

                                                                    /* ------- VALIDATE IPv4 PROTOCOL INIT ADDR ------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&addr_ip, p_addr_protocol);
    addr_multicast = NetIPv4_IsAddrMulticast(addr_ip);

    return (addr_multicast);
}
#endif


/*
*********************************************************************************************************
*                                   NetIPv4_IsAddrProtocolConflict()
*
* Description : Get interface's IPv4 protocol address conflict status.
*
* Argument(s) : if_nbr      Interface number to get IPv4 protocol address conflict status.
*
* Return(s)   : DEF_YES, if IPv4 protocol address conflict detected.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetMgr_IsAddrProtocolConflict().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) (a) RFC #3927, Section 2.5 states that :
*
*                       (1) "If a host receives ... [a] packet ... on an interface where"             ...
*                           (A) "the 'sender IP       address' is the IP address the host has configured
*                                 for that interface, but"                                            ...
*                           (B) "the 'sender hardware address' does not match the hardware address of
*                                     that interface,"                                                ...
*                       (2) "then this is a conflicting ... packet, indicating an address conflict."
*
*                   (b) Any address conflict between this host's IPv4 protocol address(s) & other host(s)
*                       on the local network is latched until checked & reset.
*
*                   See also 'NetIPv4_ChkAddrProtocolConflict()  Note #2'.
*
*               (2) Interfaces' IPv4 address configuration 'AddrProtocolConflict' variables MUST ALWAYS
*                   be accessed exclusively in critical sections.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIPv4_IsAddrProtocolConflict (NET_IF_NBR  if_nbr)
{
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
     NET_IF_NBR       valid;
     NET_ERR          err;
#endif
    NET_IPv4_IF_CFG  *p_ip_if_cfg;
    CPU_BOOLEAN       addr_conflict;
    CPU_SR_ALLOC();


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                    /* --------------- VALIDATE IF NBR ---------------- */
    valid = NetIF_IsValidHandler(if_nbr, &err);
    if (valid != DEF_YES) {
        return  (DEF_NO);
    }
#endif

                                                                    /* ------- CHK IPv4 PROTOCOL ADDR CONFLICT -------- */
    p_ip_if_cfg = &NetIPv4_IF_CfgTbl[if_nbr];

    CPU_CRITICAL_ENTER();
    addr_conflict                    = p_ip_if_cfg->AddrProtocolConflict;
    p_ip_if_cfg->AddrProtocolConflict = DEF_NO;                     /* Clr addr conflict (see Note #1b).                */
    CPU_CRITICAL_EXIT();

    return (addr_conflict);
}


/*
*********************************************************************************************************
*                                   NetIPv4_ChkAddrProtocolConflict()
*
* Description : Check for any IPv4 protocol address conflict between this interface's IPv4 host address(s)
*                   & other host(s) on the local network.
*
* Argument(s) : if_nbr              Interface number to get IPv4 protocol address conflict status.
*
*               p_addr_protocol     Pointer to protocol address (see Note #1).
*
*               addr_protocol_len   Length of  protocol address (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IPv4_ERR_NONE                   Protocol address conflict successfully checked.
*                               NET_ERR_FAULT_NULL_PTR               Argument 'p_addr_protocol' passed a NULL
*                                                                       pointer.
*                               NET_IPv4_ERR_INVALID_ADDR_LEN       Invalid IPv4 protocol address length.
*
*                                                                   - RETURNED BY NetIPv4_GetAddrHostHandler() : --
*                               NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS   Interface in dynamic address initialization.
*                               NET_IPv4_ERR_ADDR_TBL_SIZE          Invalid IP address table size.
*
*                                                                   ---- RETURNED BY NetIF_IsValidHandler() : -----
*                               NET_IF_ERR_INVALID_IF               Invalid network interface number.
*
* Return(s)   : none.
*
* Caller(s)   : NetMgr_ChkAddrProtocolConflict().
*
*               This function is an INTERNAL function & MUST NOT be called by application function(s).
*
* Note(s)     : (1) IPv4 protocol address MUST be in network-order.
*
*               (2) (a) RFC #3927, Section 2.5 states that :
*
*                       (1) "If a host receives ... [a] packet ... on an interface where"             ...
*                           (A) "the 'sender IP       address' is the IP address the host has configured
*                                 for that interface, but"                                            ...
*                           (B) "the 'sender hardware address' does not match the hardware address of
*                                     that interface,"                                                ...
*                       (2) "then this is a conflicting ... packet, indicating an address conflict."
*
*                   (b) Any address conflict between this host's IPv4 protocol address(s) & other host(s)
*                       on the local network is latched until checked & reset.
*
*                   See also 'NetIPv4_IsAddrProtocolConflict()  Note #1'.
*
*               (3) Network layer manager SHOULD eventually be responsible for maintaining each
*                   interface's network address(s)' configuration. #### NET-809
*
*                   See also 'net_mgr.c  Note #1'.
*********************************************************************************************************
*/

void  NetIPv4_ChkAddrProtocolConflict (NET_IF_NBR   if_nbr,
                                       CPU_INT08U  *p_addr_protocol,
                                       CPU_INT08U   addr_protocol_len,
                                       NET_ERR     *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_INT08U       addr_ip_len;
#endif
    NET_IPv4_IF_CFG   *p_ip_if_cfg;
    NET_IPv4_ADDR     *p_addr_ip;
    NET_IPv4_ADDR      addr_ip_tbl[NET_IPv4_CFG_IF_MAX_NBR_ADDR];
    NET_IP_ADDRS_QTY   addr_ip_tbl_qty;
    CPU_INT08U         addr_ix;
    CPU_BOOLEAN        addr_conflict;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                     /* ------------ VALIDATE PROTOCOL ADDR ------------ */
    if (p_addr_protocol == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    addr_ip_len = sizeof(NET_IPv4_ADDR);
    if (addr_protocol_len != addr_ip_len) {
       *p_err = NET_IPv4_ERR_INVALID_ADDR_LEN;
        return;
    }
#endif

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                    /* --------------- VALIDATE IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif


                                                                    /* ----------- GET IPv4 PROTOCOL ADDRS ------------ */
    addr_ip_tbl_qty = sizeof(addr_ip_tbl) / sizeof(NET_IPv4_ADDR);
   (void)NetIPv4_GetAddrHostHandler((NET_IF_NBR        ) if_nbr,
                                    (NET_IPv4_ADDR    *)&addr_ip_tbl[0],
                                    (NET_IP_ADDRS_QTY *)&addr_ip_tbl_qty,
                                    (NET_ERR          *) p_err);
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
        case NET_IPv4_ERR_ADDR_NONE_AVAIL:
             break;


        case NET_IF_ERR_INVALID_IF:
        case NET_ERR_FAULT_NULL_PTR:
        case NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS:
        case NET_IPv4_ERR_ADDR_TBL_SIZE:
        default:
             return;
    }


                                                                    /* ------ CHK IPv4 PROTOCOL ADDR(S) CONFLICT ------ */
    addr_ix       =  0u;
    p_addr_ip      = &addr_ip_tbl[addr_ix];
    addr_conflict =  DEF_NO;

    while ((addr_ix < addr_ip_tbl_qty) &&                           /* Srch ALL cfg'd addrs ...                         */
           (addr_conflict == DEF_NO)) {                             /* ... for protocol addr conflict (see Note #2a1A). */

       *p_addr_ip      = NET_UTIL_HOST_TO_NET_32(*p_addr_ip);
        addr_conflict = Mem_Cmp((void     *)p_addr_protocol,
                                (void     *)p_addr_ip,
                                (CPU_SIZE_T)addr_protocol_len);
        p_addr_ip++;
        addr_ix++;
    }

    if (addr_conflict == DEF_YES) {                                 /* If protocol addrs conflict, ...                  */
        p_ip_if_cfg                       = &NetIPv4_IF_CfgTbl[if_nbr];
        CPU_CRITICAL_ENTER();
        p_ip_if_cfg->AddrProtocolConflict =  DEF_YES;               /* ... set addr conflict (see Note #1a2).           */
        CPU_CRITICAL_EXIT();
    }


   *p_err = NET_IPv4_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_IPv4_MODULE_EN */

