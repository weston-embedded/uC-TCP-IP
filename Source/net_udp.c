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
*                                          NETWORK UDP LAYER
*                                      (USER DATAGRAM PROTOCOL)
*
* Filename  : net_udp.c
* Version   : V3.05.04
*********************************************************************************************************
* Note(s)   : (1) Supports User Datagram Protocol as described in RFC #768.
*
*********************************************************************************************************
* Notice(s) : (1) The Institute of Electrical and Electronics Engineers and The Open Group, have given
*                 us permission to reprint portions of their documentation.  Portions of this text are
*                 reprinted and reproduced in electronic form from the IEEE Std 1003.1, 2004 Edition,
*                 Standard for Information Technology -- Portable Operating System Interface (POSIX),
*                 The Open Group Base Specifications Issue 6, Copyright (C) 2001-2004 by the Institute
*                 of Electrical and Electronics Engineers, Inc and The Open Group.  In the event of any
*                 discrepancy between these versions and the original IEEE and The Open Group Standard,
*                 the original IEEE and The Open Group Standard is the referee document.  The original
*                 Standard can be obtained online at http://www.opengroup.org/unix/online.html.
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
#define    NET_UDP_MODULE
#include  "net_udp.h"
#include  "net_cfg_net.h"

#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#include  "../IP/IPv4/net_icmpv4.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#include  "../IP/IPv6/net_icmpv6.h"
#endif

#include  "net.h"
#include  "net_stat.h"
#include  "net_buf.h"
#include  "net_ip.h"
#include  "net_util.h"
#include  "net_sock.h"
#include  "../IF/net_if.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                            /* --------------- RX FNCTS --------------- */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetUDP_RxPktValidateBuf  (NET_BUF_HDR       *p_buf_hdr,
                                        NET_ERR           *p_err);
#endif

static  void  NetUDP_RxPktValidate     (NET_BUF           *p_buf,
                                        NET_BUF_HDR       *p_buf_hdr,
                                        NET_UDP_HDR       *p_udp_hdr,
                                        NET_ERR           *p_err);


static  void  NetUDP_RxPktDemuxDatagram(NET_BUF           *p_buf,
                                        NET_ERR           *p_err);


static  void  NetUDP_RxPktFree         (NET_BUF           *p_buf);

static  void  NetUDP_RxPktDiscard      (NET_BUF           *p_buf,
                                        NET_ERR           *p_err);


                                                                            /* --------------- TX FNCTS --------------- */
#ifdef  NET_IPv4_MODULE_EN
static  void  NetUDP_TxIPv4            (NET_BUF            *p_buf,
                                        NET_IPv4_ADDR       src_addr,
                                        NET_UDP_PORT_NBR    src_port,
                                        NET_IPv4_ADDR       dest_addr,
                                        NET_UDP_PORT_NBR    dest_port,
                                        NET_IPv4_TOS        TOS,
                                        NET_IPv4_TTL        TTL,
                                        NET_UDP_FLAGS       flags_udp,
                                        NET_IPv4_FLAGS      flags_ip,
                                        void               *p_opts_ip,
                                        NET_ERR            *p_err);
#endif

#ifdef  NET_IPv6_MODULE_EN
static  void  NetUDP_TxIPv6            (NET_BUF                 *p_buf,
                                        NET_IPv6_ADDR           *p_src_addr,
                                        NET_UDP_PORT_NBR         src_port,
                                        NET_IPv6_ADDR           *p_dest_addr,
                                        NET_UDP_PORT_NBR         dest_port,
                                        NET_IPv6_TRAFFIC_CLASS   traffic_class,
                                        NET_IPv6_FLOW_LABEL      flow_label,
                                        NET_IPv6_HOP_LIM         hop_lim,
                                        NET_UDP_FLAGS            flags_udp,
                                        NET_ERR                 *p_err);
#endif

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetUDP_TxPktValidate     (NET_BUF_HDR             *p_buf_hdr,
                                        NET_UDP_PORT_NBR         src_port,
                                        NET_UDP_PORT_NBR         dest_port,
                                        NET_UDP_FLAGS            flags_udp,
                                        NET_ERR                 *p_err);
#endif

static  void  NetUDP_TxPktPrepareHdr   (NET_BUF                 *p_buf,
                                        NET_BUF_HDR             *p_buf_hdr,
                                        void                    *p_src_addr,
                                        NET_UDP_PORT_NBR         src_port,
                                        void                    *p_dest_addr,
                                        NET_UDP_PORT_NBR         dest_port,
                                        NET_UDP_FLAGS            flags_udp,
                                        NET_ERR                 *p_err);

static  void  NetUDP_TxPktFree         (NET_BUF                 *p_buf);

static  void  NetUDP_TxPktDiscard      (NET_BUF                 *p_buf,
                                        NET_ERR                 *p_err);

static  void  NetUDP_GetTxDataIx       (NET_IF_NBR               if_nbr,
                                        NET_PROTOCOL_TYPE        protocol,
                                        CPU_INT16U               data_len,
                                        NET_UDP_FLAGS            flags,
                                        CPU_INT16U              *p_ix,
                                        NET_ERR                 *p_err);


/*
*********************************************************************************************************
*                                            NetUDP_Init()
*
* Description : (1) Initialize User Datagram Protocol Layer :
*
*                   (a)
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
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetUDP_Init (void)
{
}


/*
*********************************************************************************************************
*                                             NetUDP_Rx()
*
* Description : (1) Process received datagrams & forward to socket or application layer :
*
*                   (a) Validate UDP packet
*                   (b) Demultiplex datagram to socket/application connection
*                   (c) Update receive statistics
*
*               (2) Although UDP data units are typically referred to as 'datagrams' (see RFC #768, Section
*                   'Introduction'), the term 'UDP packet' (see RFC #1983, 'packet') is used for UDP Receive
*                   until the packet is validated as a UDP datagram.
*
*
* Argument(s) : p_buf        Pointer to network buffer that received UDP packet.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                UDP datagram successfully received & processed.
*
*                                                               ----- RETURNED BY NetUDP_RxPktDiscard() : -----
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIP_RxPktDemuxDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (3) Network buffer already freed by higher layer; only increment error counter.
*
*               (4) RFC #792, Section 'Destination Unreachable Message : Description' states that
*                   "if, in the destination host, the IP module cannot deliver the datagram because
*                   the indicated ... process port is not active, the destination host may send a
*                   destination unreachable message to the source host".
*********************************************************************************************************
*/

void  NetUDP_Rx (NET_BUF  *p_buf,
                 NET_ERR  *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_UDP_HDR  *p_udp_hdr;
    NET_ERR       msg_err;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetUDP_RxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.NullPtrCtr);
        return;
    }
#endif


    NET_CTR_STAT_INC(Net_StatCtrs.UDP.RxPktCtr);


                                                                /* -------------- VALIDATE RX'D UDP PKT --------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetUDP_RxPktValidateBuf(p_buf_hdr, p_err);                    /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_UDP_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetUDP_RxPktDiscard(p_buf, p_err);
             return;
    }
#endif
    p_udp_hdr = (NET_UDP_HDR *)&p_buf->DataPtr[p_buf_hdr->TransportHdrIx];
    NetUDP_RxPktValidate(p_buf, p_buf_hdr, p_udp_hdr, p_err);       /* Validate rx'd pkt.                                   */


                                                                /* ------------------ DEMUX DATAGRAM ------------------ */
    switch (*p_err) {
        case NET_UDP_ERR_NONE:
             NetUDP_RxPktDemuxDatagram(p_buf, p_err);
             break;


        case NET_UDP_ERR_INVALID_PORT_NBR:
            *p_err = NET_ERR_RX_DEST;
             break;


        case NET_UDP_ERR_INVALID_LEN:
        case NET_UDP_ERR_INVALID_LEN_DATA:
        case NET_UDP_ERR_INVALID_CHK_SUM:
        default:
             NetUDP_RxPktDiscard(p_buf, p_err);
             return;
    }


                                                                /* ----------------- UPDATE RX STATS ------------------ */
    switch (*p_err) {                                            /* Chk err from NetUDP_RxPktDemuxDatagram().            */
        case NET_APP_ERR_NONE:
        case NET_SOCK_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.UDP.RxDgramCompCtr);
            *p_err = NET_UDP_ERR_NONE;
             break;


        case NET_ERR_RX:
                                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxPktDiscardedCtr);
                                                                /* Rtn err from NetUDP_RxPktDemuxDatagram().            */
             return;


        case NET_ERR_RX_DEST:
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxDestCtr);

             if (DEF_BIT_IS_CLR(p_buf_hdr->Flags, NET_BUF_FLAG_IPv6_FRAME)) {
#ifdef  NET_ICMPv4_MODULE_EN
                 NetICMPv4_TxMsgErr(p_buf,                             /* Tx ICMP port unreach (see Note #5).                  */
                                    NET_ICMPv4_MSG_TYPE_DEST_UNREACH,
                                    NET_ICMPv4_MSG_CODE_DEST_PORT,
                                    NET_ICMPv4_MSG_PTR_NONE,
                                   &msg_err);
#endif
             } else {
#ifdef  NET_ICMPv6_MODULE_EN
                NetICMPv6_TxMsgErr(p_buf,
                                   NET_ICMPv6_MSG_TYPE_DEST_UNREACH,
                                   NET_ICMPv6_MSG_CODE_DEST_PORT_UNREACHABLE,
                                   NET_ICMPv6_MSG_PTR_NONE,
                                  &msg_err);
#endif
             }
             (void)msg_err;

             NetUDP_RxPktDiscard(p_buf, p_err);
             return;


        case NET_INIT_ERR_NOT_COMPLETED:
        default:
             NetUDP_RxPktDiscard(p_buf, p_err);
             return;
    }
}


/*
*********************************************************************************************************
*                                         NetUDP_RxAppData()
*
* Description : (1) Deframe application data from received UDP packet buffer(s) :
*
*                   (a) Validate receive packet buffer(s)
*                   (b) Validate receive data buffer                                        See Note #4
*                   (c) Validate receive flags                                              See Note #5
*                   (d) Get any received IP options                                         See Note #6
*                   (e) Deframe application data from UDP packet buffer(s)
*                   (f) Free UDP packet buffer(s)
*
*
* Argument(s) : p_buf                Pointer to network buffer that received UDP datagram.
*
*               pdata_buf           Pointer to application buffer to receive application data.
*
*               data_buf_len        Size    of application receive buffer (in octets) [see Note #4].
*
*               flags               Flags to select receive options (see Note #5); bit-field flags logically OR'd :
*
*                                       NET_UDP_FLAG_NONE               No      UDP receive flags selected.
*                                       NET_UDP_FLAG_RX_DATA_PEEK       Receive UDP application data without consuming
*                                                                           the data; i.e. do NOT free any UDP receive
*                                                                           packet buffer(s).
*
*               pip_opts_buf        Pointer to buffer to receive possible IP options (see Note #6a), if NO error(s).
*
*               ip_opts_buf_len     Size of IP options receive buffer (in octets)    [see Note #6b].
*
*               pip_opts_len        Pointer to variable that will receive the return size of any received IP options,
*                                       if NO error(s).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                UDP application data successfully deframed; check
*                                                                   return value for number of data octets received.
*                               NET_UDP_ERR_INVALID_DATA_SIZE   UDP data receive buffer insufficient size; some,
*                                                                   but not all, UDP application data deframed
*                                                                   into receive buffer (see Note #4b).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_buf'/'pdata_buf' passed a NULL pointer.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flags.
*                               NET_UDP_ERR_INVALID_ARG         Invalid argument(s).
*
*                                                               ------- RETURNED BY NetUDP_RxPktDiscard() : --------
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : Total application data octets deframed into receive buffer, if NO error(s).
*
*               0,                                                          otherwise.
*
* Caller(s)   : NetUDP_RxAppDataHandler(),
*               NetSock_RxDataHandlerDatagram().
*
*               This function is a network protocol suite application programming interface (API) function & MAY
*               be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetUDP_RxAppData() MUST be called with the global network lock already acquired.
*
*                   See also 'NetUDP_RxPktDemuxAppData()  Note #1a1A1b'.
*
*               (3) NetUDP_RxAppData() blocked until network initialization completes.
*
*               (4) (a) Application data receive buffer should be large enough to receive either ...
*
*                       (1) The maximum UDP datagram size (i.e. 65,507 octets)
*                             OR
*                       (2) The application's expected maximum UDP datagram size
*
*                   (b) If the application receive buffer size is NOT large enough for the received UDP datagram,
*                       the remaining application data octets are discarded & NET_UDP_ERR_INVALID_DATA_SIZE error
*                       is returned.
*
*               (5) If UDP receive flag options that are NOT implemented are requested, NetUDP_RxAppData() aborts
*                   & returns appropriate error codes so that requested flag options are NOT silently ignored.
*
*               (6) (a) If ...
*
*                       (1) NO IP options were received with the UDP datagram
*                             OR
*                       (2) NO IP options receive buffer is provided by the application
*                             OR
*                       (3) IP options receive buffer NOT large enough for the received IP options
*
*                       ... then NO IP options are returned & any received IP options are silently discarded.
*
*                   (b) The IP options receive buffer size SHOULD be large enough to receive the maximum
*                       IP options size, NET_IP_HDR_OPT_SIZE_MAX.
*
*                   (c) IP options are received from the first packet buffer.  In other words, if multiple
*                       packet buffers are received for a fragmented datagram, IP options are received from
*                       the first fragment of the datagram.
*
*                   (d) (1) (A) RFC #1122, Section 3.2.1.8 states that "all IP options ... received in
*                               datagrams MUST be passed to the transport layer ... [which] MUST ... interpret
*                               those IP options that they understand and silently ignore the others".
*
*                           (B) RFC #1122, Section 4.1.3.2 adds that "UDP MUST pass any IP option that it
*                               receives from the IP layer transparently to the application layer".
*
*                       (2) Received IP options should be provided/decoded via appropriate IP layer API. #### NET-811
*
*               (7) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*
*               (8) Since pointer arithmetic is based on the specific pointer data type & inherent pointer
*                   data type size, pointer arithmetic operands :
*
*                   (a) MUST be in terms of the specific pointer data type & data type size; ...
*                   (b) SHOULD NOT & in some cases MUST NOT be cast to other data types or data type sizes.
*
*               (9) (a) On any internal receive     errors, UDP receive packets are     discarded.
*
*                   (b) On any external application errors, UDP receive packets are NOT discarded;
*                       the application MAY continue to attempt to receive the application data
*                       via NetUDP_RxAppData().
*
*              (10) IP options arguments may NOT be necessary.
*********************************************************************************************************
*/

CPU_INT16U  NetUDP_RxAppData (NET_BUF        *p_buf,
                              void           *pdata_buf,
                              CPU_INT16U      data_buf_len,
                              NET_UDP_FLAGS   flags,
                              void           *pip_opts_buf,
                              CPU_INT08U      ip_opts_buf_len,
                              CPU_INT08U     *pip_opts_len,
                              NET_ERR        *p_err)
{
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    CPU_INT08U     *pip_opts_len_init;
    NET_UDP_FLAGS   flag_mask;
    CPU_BOOLEAN     used;
#endif
#ifdef  NET_IPv4_MODULE_EN
    CPU_INT08U     *pip_opts;
    CPU_INT08U      ip_opts_len;
    CPU_INT08U      ip_opts_len_unused;
#endif
    CPU_BOOLEAN     peek;
    NET_BUF        *p_buf_head;
    NET_BUF        *p_buf_next;
    NET_BUF_HDR    *p_buf_head_hdr;
    NET_BUF_HDR    *p_buf_hdr;
    NET_BUF_SIZE    data_len_pkt;
    CPU_INT16U      data_len_buf_rem;
    CPU_INT16U      data_len_tot;
    CPU_INT08U     *p_data;

    NET_ERR         err;
    NET_ERR         err_rtn;


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((CPU_INT16U)0);
    }

    pip_opts_len_init =  (CPU_INT08U *) pip_opts_len;
#endif
#ifdef  NET_IPv4_MODULE_EN
    if (pip_opts_len ==  (CPU_INT08U *) 0) {                    /* If NOT avail, ...                                    */
        pip_opts_len  =  (CPU_INT08U *)&ip_opts_len_unused;     /* ... re-cfg NULL rtn ptr to unused local var.         */
       (void)&ip_opts_len_unused;                               /* Prevent possible 'variable unused' warning.          */
    }
   *pip_opts_len = 0u;                                          /* Init len for err (see Note #7).                      */
#endif

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit rx (see Notes #3 & #9b).  */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return (0u);
    }

                                                                /* --------------- VALIDATE RX PKT BUFS --------------- */
    if (p_buf == (NET_BUF *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;                           /* See Note #9b.                                        */
        return (0u);
    }

    used = NetBuf_IsUsed(p_buf);
    if (used != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxPktDiscardedCtr);
       *p_err =  NET_ERR_RX;                                     /* See Note #9b.                                        */
        return (0u);
    }

                                                                /* --------------- VALIDATE RX DATA BUF --------------- */
    if (pdata_buf == (CPU_INT08U *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;                           /* See Note #9b.                                        */
        return (0u);
    }
    if (data_buf_len < 1) {
       *p_err =  NET_UDP_ERR_INVALID_DATA_SIZE;                  /* See Note #9b.                                        */
        return (0u);
    }

                                                                /* ---------------- VALIDATE RX FLAGS ----------------- */
    flag_mask = NET_UDP_FLAG_NONE        |
                NET_UDP_FLAG_RX_DATA_PEEK;
                                                                /* If any invalid flags req'd, rtn err (see Note #5).   */
    if ((flags & (NET_UDP_FLAGS)~flag_mask) != NET_UDP_FLAG_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.InvalidFlagsCtr);
       *p_err =  NET_UDP_ERR_INVALID_FLAG;                       /* See Note #9b.                                        */
        return (0u);
    }

                                                                /* --------------- VALIDATE RX IP OPTS ---------------- */
    if (((pip_opts_buf      != (void       *)0)  &&             /* If (IP opts buf         avail BUT ..                 */
         (pip_opts_len_init == (CPU_INT08U *)0)) ||             /* ..  IP opts buf len NOT avail) OR ..                 */
        ((pip_opts_buf      == (void       *)0)  &&             /* .. (IP opts buf     NOT avail BUT ..                 */
         (pip_opts_len_init != (CPU_INT08U *)0))) {             /* ..  IP opts buf len     avail),   ..                 */
         *p_err =  NET_UDP_ERR_INVALID_ARG;                      /* ..  rtn err.                                         */
          return (0u);
    }
#endif


                                                                /* ----------------- GET RX'D IP OPTS ----------------- */
                                                                /* See Note #6.                                         */
    p_buf_hdr = &p_buf->Hdr;
#ifdef  NET_IPv4_MODULE_EN
    if (p_buf_hdr->IP_OptPtr != (NET_BUF *)0) {                  /* If IP opts rx'd,                 & ...               */
        if (pip_opts_buf    != (void    *)0) {                  /* .. IP opts rx buf avail,         & ...               */
            if (ip_opts_buf_len >= p_buf_hdr->IP_HdrLen) {       /* .. IP opts rx buf size sufficient, ...               */
                pip_opts     = &p_buf->DataPtr[p_buf_hdr->IP_HdrIx];
                ip_opts_len  = (CPU_INT08U)p_buf_hdr->IP_HdrLen;
                Mem_Copy((void     *)pip_opts_buf,              /* .. copy IP opts into rx buf.                         */
                         (void     *)pip_opts,
                         (CPU_SIZE_T) ip_opts_len);

               *pip_opts_len = ip_opts_len;
            }
        }
    }
#endif

                                                                /* ------------- DEFRAME UDP APP RX DATA -------------- */
    p_buf_head       =  p_buf;
    p_buf_head_hdr   = &p_buf_head->Hdr;
    p_data           = (CPU_INT08U *)pdata_buf;
    data_len_buf_rem =  data_buf_len;
    data_len_tot     =  0u;
    err_rtn          =  NET_UDP_ERR_NONE;

    while ((p_buf != (NET_BUF *)0) &&                            /* Copy app rx data from avail pkt buf(s).              */
           (data_len_buf_rem > 0)) {

        p_buf_hdr  = &p_buf->Hdr;
        p_buf_next =  p_buf_hdr->NextBufPtr;

        if (data_len_buf_rem >= p_buf_hdr->DataLen) {            /* If rem data buf len >= pkt buf data len, ...         */
            data_len_pkt = (NET_BUF_SIZE)p_buf_hdr->DataLen;     /* ...      copy all      pkt buf data len.             */
        } else {
            data_len_pkt = (NET_BUF_SIZE)data_len_buf_rem;      /* Else lim copy to rem data buf len ...                */
            err_rtn      =  NET_UDP_ERR_INVALID_DATA_SIZE;      /* ... & rtn data size err code (see Note #4b).         */
        }

        NetBuf_DataRd(p_buf,
                      p_buf_hdr->DataIx,
                      data_len_pkt,
                      p_data,
                     &err);
        if (err != NET_BUF_ERR_NONE) {                          /* See Note #9a.                                        */
            NetUDP_RxPktDiscard(p_buf_head, p_err);
            return (0u);
        }
                                                                /* Update data ptr & lens.                              */
        p_data           +=             data_len_pkt;           /* MUST NOT cast ptr operand (see Note #8b).            */
        data_len_tot     += (CPU_INT16U)data_len_pkt;
        data_len_buf_rem -= (CPU_INT16U)data_len_pkt;

        p_buf              =  p_buf_next;
    }


                                                                /* ----------------- FREE UDP RX PKTS ----------------- */
    peek = DEF_BIT_IS_SET(flags, NET_UDP_FLAG_RX_DATA_PEEK);
    if (peek != DEF_YES) {                                      /* If peek opt NOT req'd, pkt buf(s) consumed : ...     */
        p_buf_head_hdr->NextPrimListPtr = (NET_BUF *)0;          /* ... unlink from any other pkt bufs/chains    ...     */
        NetUDP_RxPktFree(p_buf_head);                            /* ... & free pkt buf(s).                               */
    }


   *p_err =  err_rtn;

    return (data_len_tot);
}


/*
*********************************************************************************************************
*                                         NetUDP_TxAppData()
*
* Description : (1) Transmit data from Application layer(s) via UDP layer :
*
*                   (a) Acquire  network lock
*                   (b) Transmit application data via UDP Transmit
*                   (c) Release  network lock
*
*
* Argument(s) : p_data      Pointer to application data.
*
*               data_len    Length  of application data (in octets) [see Note #5].
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*
*               TOS         Specific TOS to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IP_TTL_MIN                  Minimum TTL transmit value   (1)
*                               NET_IP_TTL_MAX                  Maximum TTL transmit value (255)
*                               NET_IP_TTL_DFLT                 Default TTL transmit value (128)
*                               NET_IP_TTL_NONE                 Replace with default TTL
*
*               flags_udp   Flags to select UDP transmit options (see Note #4); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #4a).
*
*               flags_ip    Flags to select IP  transmit options; bit-field flags logically OR'd :
*
*                               NET_IP_FLAG_NONE                No  IP transmit flags selected.
*                               NET_IP_FLAG_TX_DONT_FRAG        Set IP 'Don't Frag' flag.
*
*               p_opts_ip    Pointer to one or more IP options configuration data structures
*                               (see 'net_ip.h  IP HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IP_OPT_CFG_ROUTE_TS         Route &/or Internet Timestamp options configuration.
*                               NET_IP_OPT_CFG_SECURITY         Security options configuration
*                                                                   (see 'net_ip.c  Note #1e').
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                                                               ---- RETURNED BY NetUDP_TxAppDataHandler() : -----
*                               NET_UDP_ERR_NONE                Application data successfully prepared &
*                                                                   transmitted via UDP layer.
*
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_data'   passed a NULL pointer.
*                               NET_UDP_ERR_INVALID_DATA_SIZE   Argument 'data_len' passed an invalid size
*                                                                   (see Notes #5b & #5a2B).
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_ADDR_SRC    Argument 'src_addr' passed an invalid address.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum buffer
*                                                                   size available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index calculation overflows
*                                                                   buffer's DATA area.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*
*                                                               ----- RETURNED BY Net_GlobalLockAcquire() : -----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Number of data octets transmitted, if NO error(s).
*
*               0,                                 otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetUDP_TxAppData() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetUDP_TxAppDataHandler()  Note #2'.
*
*               (3) NetUDP_TxAppData() blocked until network initialization completes.
*
*                   See      'NetUDP_TxAppDataHandler()  Note #3'.
*
*               (4) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (5) (a) (1) Datagram transmission & reception MUST be atomic -- i.e. every single, complete
*                           datagram transmitted SHOULD be received as a single, complete datagram.  Thus
*                           each call to transmit data MUST be transmitted in a single, complete datagram.
*
*                       (2) (A) IEEE Std 1003.1, 2004 Edition, Section 'send() : DESCRIPTION' states that
*                               "if the message is too long to pass through the underlying protocol, send()
*                               shall fail and no data shall be transmitted".
*
*                           (B) Since IP transmit fragmentation is NOT currently supported (see 'net_ip.h
*                               Note #1d'), if the requested datagram transmit data length is greater than
*                               the UDP MTU, then NO data is transmitted & NET_UDP_ERR_INVALID_DATA_SIZE
*                               error is returned.
*
*                   (b) 'data_len' of 0 octets NOT allowed.
*********************************************************************************************************
*/
#ifdef  NET_IPv4_MODULE_EN
CPU_INT16U  NetUDP_TxAppDataIPv4 (void              *p_data,
                                  CPU_INT16U         data_len,
                                  NET_IPv4_ADDR      src_addr,
                                  NET_UDP_PORT_NBR   src_port,
                                  NET_IPv4_ADDR      dest_addr,
                                  NET_UDP_PORT_NBR   dest_port,
                                  NET_IPv4_TOS       TOS,
                                  NET_IPv4_TTL       TTL,
                                  NET_UDP_FLAGS      flags_udp,
                                  NET_IPv4_FLAGS     flags_ip,
                                  void              *p_opts_ip,
                                  NET_ERR           *p_err)
{
    CPU_INT16U  data_len_tot;


                                                                /* Acquire net lock (see Note #2b).                     */
    Net_GlobalLockAcquire((void *)&NetUDP_TxAppDataIPv4, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }
                                                                /* Tx UDP app data.                                     */
    data_len_tot = NetUDP_TxAppDataHandlerIPv4(p_data,
                                               data_len,
                                               src_addr,
                                               src_port,
                                               dest_addr,
                                               dest_port,
                                               TOS,
                                               TTL,
                                               flags_udp,
                                               flags_ip,
                                               p_opts_ip,
                                               p_err);

    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (data_len_tot);

exit_lock_fault:
   return (0u);
}
#endif


/*
*********************************************************************************************************
*                                        NetUDP_TxAppDataIPv6()
*
* Description : (1) Transmit data from Application layer(s) via UDP layer :
*
*                   (a) Acquire  network lock
*                   (b) Transmit application data via UDP Transmit
*                   (c) Release  network lock
*
*
* Argument(s) : p_data      Pointer to application data.
*
*               data_len    Length  of application data (in octets) [see Note #5].
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*
*               TOS         Specific TOS to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IP_TTL_MIN                  Minimum TTL transmit value   (1)
*                               NET_IP_TTL_MAX                  Maximum TTL transmit value (255)
*                               NET_IP_TTL_DFLT                 Default TTL transmit value (128)
*                               NET_IP_TTL_NONE                 Replace with default TTL
*
*               flags_udp   Flags to select UDP transmit options (see Note #4); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #4a).
*
*               flags_ip    Flags to select IP  transmit options; bit-field flags logically OR'd :
*
*                               NET_IP_FLAG_NONE                No  IP transmit flags selected.
*                               NET_IP_FLAG_TX_DONT_FRAG        Set IP 'Don't Frag' flag.
*
*               p_opts_ip    Pointer to one or more IP options configuration data structures
*                               (see 'net_ip.h  IP HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IP_OPT_CFG_ROUTE_TS         Route &/or Internet Timestamp options configuration.
*                               NET_IP_OPT_CFG_SECURITY         Security options configuration
*                                                                   (see 'net_ip.c  Note #1e').
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                                                               ---- RETURNED BY NetUDP_TxAppDataHandler() : -----
*                               NET_UDP_ERR_NONE                Application data successfully prepared &
*                                                                   transmitted via UDP layer.
*
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_data'   passed a NULL pointer.
*                               NET_UDP_ERR_INVALID_DATA_SIZE   Argument 'data_len' passed an invalid size
*                                                                   (see Notes #5b & #5a2B).
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_ADDR_SRC    Argument 'src_addr' passed an invalid address.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum buffer
*                                                                   size available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index calculation overflows
*                                                                   buffer's DATA area.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*
*                                                               ----- RETURNED BY Net_GlobalLockAcquire() : -----
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired.
*
* Return(s)   : Number of data octets transmitted, if NO error(s).
*
*               0,                                 otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (2) NetUDP_TxAppData() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access
*                   is asynchronous to other network protocol tasks.
*
*                   See also 'NetUDP_TxAppDataHandler()  Note #2'.
*
*               (3) NetUDP_TxAppData() blocked until network initialization completes.
*
*                   See      'NetUDP_TxAppDataHandler()  Note #3'.
*
*               (4) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (5) (a) (1) Datagram transmission & reception MUST be atomic -- i.e. every single, complete
*                           datagram transmitted SHOULD be received as a single, complete datagram.  Thus
*                           each call to transmit data MUST be transmitted in a single, complete datagram.
*
*                       (2) (A) IEEE Std 1003.1, 2004 Edition, Section 'send() : DESCRIPTION' states that
*                               "if the message is too long to pass through the underlying protocol, send()
*                               shall fail and no data shall be transmitted".
*
*                           (B) Since IP transmit fragmentation is NOT currently supported (see 'net_ip.h
*                               Note #1d'), if the requested datagram transmit data length is greater than
*                               the UDP MTU, then NO data is transmitted & NET_UDP_ERR_INVALID_DATA_SIZE
*                               error is returned.
*
*                   (b) 'data_len' of 0 octets NOT allowed.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
CPU_INT16U  NetUDP_TxAppDataIPv6 (void                    *p_data,
                                  CPU_INT16U               data_len,
                                  NET_IPv6_ADDR           *p_src_addr,
                                  NET_UDP_PORT_NBR         src_port,
                                  NET_IPv6_ADDR           *p_dest_addr,
                                  NET_UDP_PORT_NBR         dest_port,
                                  NET_IPv6_TRAFFIC_CLASS   traffic_class,
                                  NET_IPv6_FLOW_LABEL      flow_label,
                                  NET_IPv6_HOP_LIM         hop_lim,
                                  NET_UDP_FLAGS            flags_udp,
                                  NET_ERR                 *p_err)
{
    CPU_INT16U  data_len_tot;


                                                                /* Acquire net lock (see Note #2b).                     */
    Net_GlobalLockAcquire((void *)&NetUDP_TxAppDataIPv6, p_err);
    if (*p_err != NET_ERR_NONE) {
         goto exit_lock_fault;
    }
                                                                /* Tx UDP app data.                                     */
    data_len_tot = NetUDP_TxAppDataHandlerIPv6(p_data,
                                               data_len,
                                               p_src_addr,
                                               src_port,
                                               p_dest_addr,
                                               dest_port,
                                               traffic_class,
                                               flow_label,
                                               hop_lim,
                                               flags_udp,
                                               p_err);

    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (data_len_tot);

exit_lock_fault:
    return (0u);
}
#endif


/*
*********************************************************************************************************
*                                     NetUDP_TxAppDataHandlerIPv4()
*
* Description : (1) Prepare & transmit data from Application layer(s) via UDP layer :
*
*                   (a) Validate application data
*                   (b) Transmit application data via UDP Transmit :
*                       (1) Calculate/validate application data buffer size
*                       (2) Get buffer(s) for application data
*                       (3) Copy application data into UDP packet buffer(s)
*                       (4) Initialize UDP packet buffer controls
*                       (5) Free UDP packet buffer(s)
*
*
* Argument(s) : p_data      Pointer to application data.
*
*               data_len    Length  of application data (in octets) [see Note #5].
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*
*               TOS         Specific TOS to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IP_TTL_MIN                  Minimum TTL transmit value   (1)
*                               NET_IP_TTL_MAX                  Maximum TTL transmit value (255)
*                               NET_IP_TTL_DFLT                 Default TTL transmit value (128)
*                               NET_IP_TTL_NONE                 Replace with default TTL
*
*               flags_udp   Flags to select UDP transmit options (see Note #4); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #4a).
*
*               flags_ip    Flags to select IP  transmit options; bit-field flags logically OR'd :
*
*                               NET_IP_FLAG_NONE                No  IP transmit flags selected.
*                               NET_IP_FLAG_TX_DONT_FRAG        Set IP 'Don't Frag' flag.
*
*               p_opts_ip    Pointer to one or more IP options configuration data structures
*                               (see 'net_ip.h  IP HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IP_OPT_CFG_ROUTE_TS         Route &/or Internet Timestamp options configuration.
*                               NET_IP_OPT_CFG_SECURITY         Security options configuration
*                                                                   (see 'net_ip.c  Note #1e').
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                Application data successfully prepared &
*                                                                   transmitted via UDP layer.
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_data'   passed a NULL pointer.
*                               NET_UDP_ERR_INVALID_DATA_SIZE   Argument 'data_len' passed an invalid size
*                                                                   (see Notes #5b & #5a2B).
*                               NET_UDP_ERR_INVALID_ADDR_SRC    Argument 'src_addr' passed an invalid address.
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               ----- RETURNED BY NetIF_MTU_GetProtocol() : ------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               ----------- RETURNED BY NetBuf_Get() : -----------
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum buffer
*                                                                   size available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index calculation overflows
*                                                                   buffer's DATA area.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               --------- RETURNED BY NetBuf_DataWr() : ----------
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*
*                                                               ----------- RETURNED BY NetUDP_Tx() : ------------
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : Number of data octets transmitted, if NO error(s).
*
*               0,                                 otherwise.
*
* Caller(s)   : NetUDP_TxAppData(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetUDP_TxAppDataHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetUDP_TxAppData()  Note #2'.
*
*               (3) NetUDP_TxAppDataHandler() blocked until network initialization completes.
*
*               (4) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (5) (a) (1) Datagram transmission & reception MUST be atomic -- i.e. every single, complete
*                           datagram transmitted SHOULD be received as a single, complete datagram.  Thus,
*                           each call to transmit data MUST be transmitted in a single, complete datagram.
*
*                       (2) (A) IEEE Std 1003.1, 2004 Edition, Section 'send() : DESCRIPTION' states that
*                               "if the message is too long to pass through the underlying protocol, send()
*                               shall fail and no data shall be transmitted".
*
*                           (B) Since IP transmit fragmentation is NOT currently supported (see 'net_ip.h
*                               Note #1d'), if the requested datagram transmit data length is greater than
*                               the UDP MTU, then NO data is transmitted & NET_UDP_ERR_INVALID_DATA_SIZE
*                               error is returned.
*
*                   (b) 'data_len' of 0 octets NOT allowed.
*
*               (6) On ANY transmit error, any remaining application data transmit is immediately aborted.
*********************************************************************************************************
*/
#ifdef  NET_IPv4_MODULE_EN
CPU_INT16U  NetUDP_TxAppDataHandlerIPv4 (void               *p_data,
                                         CPU_INT16U          data_len,
                                         NET_IPv4_ADDR       src_addr,
                                         NET_UDP_PORT_NBR    src_port,
                                         NET_IPv4_ADDR       dest_addr,
                                         NET_UDP_PORT_NBR    dest_port,
                                         NET_IPv4_TOS        TOS,
                                         NET_IPv4_TTL        TTL,
                                         NET_UDP_FLAGS       flags_udp,
                                         NET_IPv4_FLAGS      flags_ip,
                                         void               *p_opts_ip,
                                         NET_ERR            *p_err)
{
    NET_BUF        *p_buf;
    NET_BUF_HDR    *p_buf_hdr;
    NET_IF_NBR      if_nbr;
    NET_MTU         udp_mtu;
    NET_BUF_SIZE    buf_size_max;
    NET_BUF_SIZE    buf_size_max_data;
    NET_BUF_SIZE    data_ix_pkt;
    NET_BUF_SIZE    data_ix_pkt_offset;
    NET_BUF_SIZE    data_len_pkt;
    CPU_INT16U      data_len_tot;
    CPU_INT08U     *p_data_pkt;
    NET_ERR         err;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((CPU_INT16U)0);
    }

    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit tx (see Note #3).         */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return (0u);
    }
#endif


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                /* ---------------- VALIDATE APP DATA ----------------- */
    if (p_data == (void *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
    if (data_len <= NET_UDP_DATA_LEN_MIN) {                     /* Validate data len (see Note #5b).                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxInvalidSizeCtr);
       *p_err =  NET_UDP_ERR_INVALID_DATA_SIZE;
        return (0u);
    }
#endif



    if_nbr = NetIPv4_GetAddrHostIF_Nbr(src_addr);               /* Get IF nbr of src addr.                              */
    if (if_nbr == NET_IF_NBR_NONE) {
        NetUDP_TxPktDiscard(DEF_NULL,
                           &err);
    *p_err =  NET_UDP_ERR_INVALID_ADDR_SRC;
        return (0u);
    }
                                                                /* Get IF's UDP MTU.                                    */
    udp_mtu = NetIF_MTU_GetProtocol(if_nbr,
                                    NET_PROTOCOL_TYPE_UDP_V4,
                                    NET_IF_FLAG_NONE,
                                    p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        NetUDP_TxPktDiscard((NET_BUF *) 0,
                            (NET_ERR *)&err);
        return (0u);
    }

                                                                /* ------------------- TX APP DATA -------------------- */
                                                                /* Calc buf max data size.                              */
#if 0
    data_ix_pkt  = NET_BUF_DATA_IX_TX;
#else
    data_ix_pkt = 0u;
    NetUDP_GetTxDataIx(if_nbr,
                       NET_PROTOCOL_TYPE_UDP_V4,
                       data_len,
                       flags_udp,
                      &data_ix_pkt,
                      &err);
#endif


    buf_size_max = NetBuf_GetMaxSize(if_nbr,
                                     NET_TRANSACTION_TX,
                                     DEF_NULL,
                                     data_ix_pkt);

    buf_size_max_data = (NET_BUF_SIZE)DEF_MIN(buf_size_max, udp_mtu);

    if (data_len > buf_size_max_data) {                         /* If data len > max data size, abort tx ...            */
       *p_err = NET_UDP_ERR_INVALID_DATA_SIZE;                  /* ... & rtn size err (see Note #5a2B).                 */
        return (0u);

    } else {                                                    /* Else lim pkt data len to data len.                   */
        data_len_pkt = (NET_BUF_SIZE)data_len;
    }

    data_len_tot =  0u;
    p_data_pkt   = (CPU_INT08U *)p_data;
                                                                /* Get app data tx buf.                                 */
    p_buf = NetBuf_Get(if_nbr,
                       NET_TRANSACTION_TX,
                       data_len_pkt,
                       data_ix_pkt,
                       &data_ix_pkt_offset,
                       NET_BUF_FLAG_NONE,
                       p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         NetUDP_TxPktDiscard(p_buf, &err);
         return (data_len_tot);
    }

    data_ix_pkt += data_ix_pkt_offset;
    NetBuf_DataWr(p_buf,                           /* Wr app data into app data tx buf.                    */
                  data_ix_pkt,
                  data_len_pkt,
                  p_data_pkt,
                  p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         NetUDP_TxPktDiscard(p_buf, &err);
         return (data_len_tot);
    }

                                                                /* Init app data tx buf ctrls.                          */
    p_buf_hdr                  = &p_buf->Hdr;
    p_buf_hdr->DataIx          =  data_ix_pkt;
    p_buf_hdr->DataLen         =  data_len_pkt;
    p_buf_hdr->TotLen          =  p_buf_hdr->DataLen;
    p_buf_hdr->ProtocolHdrType =  NET_PROTOCOL_TYPE_UDP_V4;


    NetUDP_TxIPv4(p_buf,                                        /* Tx app data buf via UDP tx.                          */
                  src_addr,
                  src_port,
                  dest_addr,
                  dest_port,
                  TOS,
                  TTL,
                  flags_udp,
                  flags_ip,
                  p_opts_ip,
                  p_err);
    if (*p_err != NET_UDP_ERR_NONE) {
         return (data_len_tot);
    }

    NetUDP_TxPktFree(p_buf);                                    /* Free app data tx buf.                                */


    data_len_tot += data_len_pkt;                               /* Calc tot app data len tx'd.                          */

   *p_err = NET_UDP_ERR_NONE;

    return (data_len_tot);
}
#endif


/*
*********************************************************************************************************
*                                     NetUDP_TxAppDataHandlerIPv6()
*
* Description : (1) Prepare & transmit data from Application layer(s) via UDP layer :
*
*                   (a) Validate application data
*                   (b) Transmit application data via UDP Transmit :
*                       (1) Calculate/validate application data buffer size
*                       (2) Get buffer(s) for application data
*                       (3) Copy application data into UDP packet buffer(s)
*                       (4) Initialize UDP packet buffer controls
*                       (5) Free UDP packet buffer(s)
*
*
* Argument(s) : p_data      Pointer to application data.
*
*               data_len    Length  of application data (in octets) [see Note #5].
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*
*               TOS         Specific TOS to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IP_TTL_MIN                  Minimum TTL transmit value   (1)
*                               NET_IP_TTL_MAX                  Maximum TTL transmit value (255)
*                               NET_IP_TTL_DFLT                 Default TTL transmit value (128)
*                               NET_IP_TTL_NONE                 Replace with default TTL
*
*               flags_udp   Flags to select UDP transmit options (see Note #4); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #4a).
*
*               flags_ip    Flags to select IP  transmit options; bit-field flags logically OR'd :
*
*                               NET_IP_FLAG_NONE                No  IP transmit flags selected.
*                               NET_IP_FLAG_TX_DONT_FRAG        Set IP 'Don't Frag' flag.
*
*               p_opts_ip    Pointer to one or more IP options configuration data structures
*                               (see 'net_ip.h  IP HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IP_OPT_CFG_ROUTE_TS         Route &/or Internet Timestamp options configuration.
*                               NET_IP_OPT_CFG_SECURITY         Security options configuration
*                                                                   (see 'net_ip.c  Note #1e').
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                Application data successfully prepared &
*                                                                   transmitted via UDP layer.
*                               NET_ERR_FAULT_NULL_PTR            Argument 'p_data'   passed a NULL pointer.
*                               NET_UDP_ERR_INVALID_DATA_SIZE   Argument 'data_len' passed an invalid size
*                                                                   (see Notes #5b & #5a2B).
*                               NET_UDP_ERR_INVALID_ADDR_SRC    Argument 'src_addr' passed an invalid address.
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
*                                                               ----- RETURNED BY NetIF_MTU_GetProtocol() : ------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               ----------- RETURNED BY NetBuf_Get() : -----------
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum buffer
*                                                                   size available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index calculation overflows
*                                                                   buffer's DATA area.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               --------- RETURNED BY NetBuf_DataWr() : ----------
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*
*                                                               ----------- RETURNED BY NetUDP_Tx() : ------------
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : Number of data octets transmitted, if NO error(s).
*
*               0,                                 otherwise.
*
* Caller(s)   : NetUDP_TxAppData(),
*               NetSock_TxDataHandlerDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetUDP_TxAppDataHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetUDP_TxAppData()  Note #2'.
*
*               (3) NetUDP_TxAppDataHandler() blocked until network initialization completes.
*
*               (4) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (5) (a) (1) Datagram transmission & reception MUST be atomic -- i.e. every single, complete
*                           datagram transmitted SHOULD be received as a single, complete datagram.  Thus,
*                           each call to transmit data MUST be transmitted in a single, complete datagram.
*
*                       (2) (A) IEEE Std 1003.1, 2004 Edition, Section 'send() : DESCRIPTION' states that
*                               "if the message is too long to pass through the underlying protocol, send()
*                               shall fail and no data shall be transmitted".
*
*                           (B) Since IP transmit fragmentation is NOT currently supported (see 'net_ip.h
*                               Note #1d'), if the requested datagram transmit data length is greater than
*                               the UDP MTU, then NO data is transmitted & NET_UDP_ERR_INVALID_DATA_SIZE
*                               error is returned.
*
*                   (b) 'data_len' of 0 octets NOT allowed.
*
*               (6) On ANY transmit error, any remaining application data transmit is immediately aborted.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
CPU_INT16U  NetUDP_TxAppDataHandlerIPv6(void                    *p_data,
                                        CPU_INT16U               data_len,
                                        NET_IPv6_ADDR           *p_src_addr,
                                        NET_UDP_PORT_NBR         src_port,
                                        NET_IPv6_ADDR           *p_dest_addr,
                                        NET_UDP_PORT_NBR         dest_port,
                                        NET_IPv6_TRAFFIC_CLASS   traffic_class,
                                        NET_IPv6_FLOW_LABEL      flow_label,
                                        NET_IPv6_HOP_LIM         hop_lim,
                                        NET_UDP_FLAGS            flags_udp,
                                        NET_ERR                 *p_err)
{
    NET_BUF        *p_buf;
    NET_BUF_HDR    *p_buf_hdr;
    NET_IF_NBR      if_nbr;
    NET_MTU         udp_mtu;
    NET_BUF_SIZE    buf_size_max;
    NET_BUF_SIZE    buf_size_max_data;
    NET_BUF_SIZE    data_ix_pkt;
    NET_BUF_SIZE    data_ix_pkt_offset;
    NET_BUF_SIZE    data_len_pkt;
    CPU_INT16U      data_len_tot;
    CPU_INT08U     *p_data_pkt;
    NET_ERR         err;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((CPU_INT16U)0);
    }

    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit tx (see Note #3).         */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return (0u);
    }
#endif


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
                                                                /* ---------------- VALIDATE APP DATA ----------------- */
    if (p_data == (void *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
    if (data_len < 1) {                                         /* Validate data len (see Note #5b).                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxInvalidSizeCtr);
       *p_err =  NET_UDP_ERR_INVALID_DATA_SIZE;
        return (0u);
    }
#endif



    if_nbr = NetIPv6_GetAddrHostIF_Nbr(p_src_addr);             /* Get IF nbr of src addr.                              */
    if (if_nbr == NET_IF_NBR_NONE) {
        NetUDP_TxPktDiscard((NET_BUF *) 0,
                            (NET_ERR *)&err);
       *p_err =  NET_UDP_ERR_INVALID_ADDR_SRC;
        return (0u);
    }
                                                                /* Get IF's UDP MTU.                                    */
    udp_mtu = NetIF_MTU_GetProtocol(if_nbr, NET_PROTOCOL_TYPE_UDP_V6, NET_IF_FLAG_NONE, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        NetUDP_TxPktDiscard((NET_BUF *) 0,
                            (NET_ERR *)&err);
        return (0u);
    }

                                                                /* ------------------- TX APP DATA -------------------- */
                                                                /* Calc buf max data size.                              */
#if 0
    data_ix_pkt  = NET_BUF_DATA_IX_TX;
#else
    data_ix_pkt = 0u;
    NetUDP_GetTxDataIx(if_nbr, NET_PROTOCOL_TYPE_UDP_V6, data_len, flags_udp, &data_ix_pkt, &err);
#endif
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )if_nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_TX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )data_ix_pkt);

    buf_size_max_data = (NET_BUF_SIZE)DEF_MIN(buf_size_max, udp_mtu);

    if (data_len > buf_size_max_data) {                         /* If data len > max data size, abort tx ...            */
       *p_err =  NET_UDP_ERR_INVALID_DATA_SIZE;                  /* ... & rtn size err (see Note #5a2B).                 */
        return (0u);

    } else {                                                    /* Else lim pkt data len to data len.                   */
        data_len_pkt = (NET_BUF_SIZE)data_len;
    }

    data_len_tot =  0u;
    p_data_pkt   = (CPU_INT08U *)p_data;
                                                                /* Get app data tx buf.                                 */
    p_buf = NetBuf_Get((NET_IF_NBR     ) if_nbr,
                      (NET_TRANSACTION) NET_TRANSACTION_TX,
                      (NET_BUF_SIZE   ) data_len_pkt,
                      (NET_BUF_SIZE   ) data_ix_pkt,
                      (NET_BUF_SIZE  *)&data_ix_pkt_offset,
                      (NET_BUF_FLAGS  ) NET_BUF_FLAG_NONE,
                      (NET_ERR       *) p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         NetUDP_TxPktDiscard(p_buf, &err);
         return (data_len_tot);
    }

    data_ix_pkt += data_ix_pkt_offset;
    NetBuf_DataWr((NET_BUF    *)p_buf,                           /* Wr app data into app data tx buf.                    */
                  (NET_BUF_SIZE)data_ix_pkt,
                  (NET_BUF_SIZE)data_len_pkt,
                  (CPU_INT08U *)p_data_pkt,
                  (NET_ERR    *)p_err);
    if (*p_err != NET_BUF_ERR_NONE) {
         NetUDP_TxPktDiscard(p_buf, &err);
         return (data_len_tot);
    }

                                                                /* Init app data tx buf ctrls.                          */
    p_buf_hdr                  = &p_buf->Hdr;
    p_buf_hdr->DataIx          = (CPU_INT16U  )data_ix_pkt;
    p_buf_hdr->DataLen         = (NET_BUF_SIZE)data_len_pkt;
    p_buf_hdr->TotLen          = (NET_BUF_SIZE)p_buf_hdr->DataLen;
    p_buf_hdr->ProtocolHdrType =  NET_PROTOCOL_TYPE_UDP_V6;


    DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_IPv6_FRAME);


    NetUDP_TxIPv6(p_buf,                                             /* Tx app data buf via UDP tx.                          */
                  p_src_addr,
                  src_port,
                  p_dest_addr,
                  dest_port,
                  traffic_class,
                  flow_label,
                  hop_lim,
                  flags_udp,
                  p_err);

    if (*p_err != NET_UDP_ERR_NONE) {
         return (data_len_tot);
    }

    NetUDP_TxPktFree(p_buf);                                     /* Free app data tx buf.                                */


    data_len_tot += data_len_pkt;                               /* Calc tot app data len tx'd.                          */



   *p_err =  NET_UDP_ERR_NONE;

    return (data_len_tot);
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
*                                      NetUDP_RxPktValidateBuf()
*
* Description : Validate received buffer header as UDP protocol.
*
* Argument(s) : p_buf_hdr    Pointer to network buffer header that received UDP packet.
*               --------    Argument validated in NetUDP_Rx().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                Received buffer's UDP header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT UDP.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetUDP_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
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

                                                                /* --------------- VALIDATE UDP BUF HDR --------------- */
    if (!((p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_UDP_V4)  ||
          (p_buf_hdr->ProtocolHdrType == NET_PROTOCOL_TYPE_UDP_V6))) {
        if_nbr = p_buf_hdr->IF_Nbr;
        valid  = NetIF_IsValidHandler(if_nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxInvalidProtocolCtr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->TransportHdrIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxInvalidBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_UDP_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       NetUDP_RxPktValidate()
*
* Description : (1) Validate received UDP packet :
*
*                   (a) Validate the received packet's following UDP header fields :
*
*                       (1) Source      Port
*                       (2) Destination Port
*                       (3) Datagram Length                                 See Note #3
*                       (4) Check-Sum                                       See Note #4
*
*                   (b) Convert the following UDP header fields from network-order to host-order :
*
*                       (1) Source      Port                                See Notes #1bB1
*                       (2) Destination Port                                See Notes #1bB2
*                       (3) Datagram Length                                 See Notes #1bB3
*                       (4) Check-Sum                                       See Note  #4d
*
*                           (A) These fields are NOT converted directly in the received packet buffer's
*                               data area but are converted in local or network buffer variables ONLY.
*
*                           (B) The following UDP header fields are converted & stored in network buffer
*                               variables :
*
*                               (1) Source      Port
*                               (2) Destination Port
*                               (3) Datagram Length
*
*                   (c) Update network buffer's protocol controls
*
*
* Argument(s) : p_buf        Pointer to network buffer that received UDP packet.
*               ----        Argument checked   in NetUDP_Rx().
*
*               p_buf_hdr    Pointer to network buffer header.
*               --------    Argument validated in NetUDP_Rx().
*
*               p_udp_hdr    Pointer to received packet's UDP header.
*               --------    Argument validated in NetUDP_Rx()/NetUDP_RxPktValidateBuf().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                Received packet validated.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_LEN         Invalid UDP datagram      length.
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid UDP datagram data length.
*                               NET_UDP_ERR_INVALID_CHK_SUM     Invalid UDP check-sum.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Rx().
*
* Note(s)     : (2) See 'net_udp.h  UDP HEADER' for UDP header format.
*
*               (3) In addition to validating that the UDP Datagram Length is greater than or equal to the
*                   minimum UDP header length,     the UDP Datagram Length is compared to the remaining IP
*                   Datagram Length which should be identical.
*
*               (4) (a) UDP header Check-Sum field MUST be validated BEFORE (or AFTER) any multi-octet words
*                       are converted from network-order to host-order since "the sum of 16-bit integers can
*                       be computed in either byte order" [RFC #1071, Section 2.(B)].
*
*                       In other words, the UDP Datagram Check-Sum CANNOT be validated AFTER SOME but NOT ALL
*                       multi-octet words have been converted from network-order to host-order.
*
*                   (b) However, ALL received packets' multi-octet words are converted in local or network
*                       buffer variables ONLY (see Note #1bA).  Therefore, UDP Datagram Check-Sum may be
*                       validated at any point.
*
*                   (c) The UDP Datagram Check-Sum MUST be validated AFTER the datagram length field has been
*                       validated so that the total UDP Datagram Length (in octets) will already be calculated
*                       for the UDP Check-Sum calculation.
*
*                       For efficiency, the UDP Datagram Check-Sum is validated AFTER all other UDP header
*                       fields have been validated.  Thus the iteration-intensive UDP Datagram Check-Sum is
*                       calculated only after all other UDP header fields have been quickly validated.
*
*                   (d) (1) Before the UDP Datagram Check-Sum is validated, it is necessary to convert the
*                           Check-Sum from network-order to host-order to verify whether the received UDP
*                           datagram's Check-Sum is valid -- i.e. whether the UDP datagram was transmitted
*                           with or without a computed Check-Sum (see RFC #768, Section 'Fields : Checksum').
*
*                       (2) Since the value that indicates no check-sum was computed for the received UDP
*                           datagram is one's-complement positive zero -- all check-sum bits equal to zero,
*                           a value that is endian-order independent -- it is NOT absolutely necessary to
*                           convert the UDP Datagram Check-Sum from network-order to host-order.
*
*                           However, network data value macro's inherently convert data words from network
*                           word order to CPU word order.
*
*                           See also 'net_util.h  NETWORK DATA VALUE MACRO'S  Note #1a1'.
*
*                       (3) (A) Any UDP datagram received with NO computed check-sum is flagged so that "an
*                               application MAY optionally ... discard ... UDP datagrams without checksums"
*                               (see RFC #1122, Section 4.1.3.4).
*
*                               Run-time API to handle/discard UDP datagrams without checksums NOT yet
*                               implemented. #### NET-819
*
*                           (B) UDP buffer flag value to clear was previously initialized in NetBuf_Get() when
*                               the buffer was allocated.  This buffer flag value does NOT need to be re-cleared
*                               but is shown for completeness.
*
*                   (e) (1) In addition to the UDP datagram header & data, the UDP Check-Sum calculation
*                           includes "a pseudo header of information from the IP header ... conceptually
*                           prefixed to the UDP header [which] contains the source address, the destination
*                           address, the protocol, and the UDP length" (see RFC #768, Section 'Fields :
*                           Checksum').
*
*                       (2) Since network check-sum functions REQUIRE that 16-bit one's-complement check-
*                           sum calculations be performed on headers & data arranged in network-order (see
*                           'net_util.c  NetUtil_16BitOnesCplChkSumDataVerify()  Note #4'), UDP pseudo-header
*                           values MUST be set or converted to network-order.
*
*                   (f) RFC #768, Section 'Fields : Checksum' specifies that "the data [is] padded with zero
*                       octets at the end (if necessary) to make a multiple of two octets".
*
*                       See also 'net_util.c  NetUtil_16BitSumDataCalc()  Note #8'.
*
*               (5) (a) Since the minimum network buffer size MUST be configured such that the entire UDP
*                       header MUST be received in a single packet (see 'net_buf.h  NETWORK BUFFER INDEX &
*                       SIZE DEFINES  Note #1c'), after the UDP header size is decremented from the first
*                       packet buffer's remaining number of data octets, any remaining octets MUST be user
*                       &/or application data octets.
*
*                       (1) Note that the 'Data' index is updated regardless of a null-size data length.
*
*                   (b) If additional packet buffers exist, the remaining IP datagram 'Data' MUST be user
*                       &/or application data.  Therefore, the 'Data' length does NOT need to be adjusted
*                       but the 'Data' index MUST be updated.
*
*                   (c) #### Total UDP Datagram Length & Data Length is duplicated in ALL fragmented packet
*                       buffers (may NOT be necessary; remove if unnecessary).
*
*               (6) RFC #1122, Sections 3.2.1 & 3.2.2 require that IP & ICMP packets with certain invalid
*                   header fields be "silently discarded".  However, NO RFC specifies how UDP should handle
*                   received datagrams with invalid header fields.
*
*                   In addition, UDP is a "transaction oriented" protocol that does NOT guarantee "delivery
*                   and duplicate protection" of UDP datagrams (see RFC #768, Section 'Introduction').
*
*                   Therefore, it is assumed that ALL UDP datagrams with ANY invalid header fields SHOULD
*                   be silently discarded.
*
*               (7) (a) RFC #1122, Section 3.2.1.8 states that "all IP options ... received in datagrams
*                       MUST be passed to the transport layer ... [which] MUST ... interpret those IP
*                       options that they understand and silently ignore the others".
*
*                   (b) RFC #1122, Section 4.1.3.2 adds that "UDP MUST pass any IP option that it receives
*                       from the IP layer transparently to the application layer".
*
*                   See also 'NetUDP_RxAppData()  Note #6d'.
*********************************************************************************************************
*/

static  void  NetUDP_RxPktValidate (NET_BUF      *p_buf,
                                    NET_BUF_HDR  *p_buf_hdr,
                                    NET_UDP_HDR  *p_udp_hdr,
                                    NET_ERR      *p_err)
{
    NET_UDP_PSEUDO_HDR   udp_pseudo_hdr;
#ifdef  NET_IPv6_MODULE_EN
    NET_IPv6_PSEUDO_HDR  ipv6_pseudo_hdr;
#endif
    CPU_INT16U           udp_tot_len;
    CPU_INT16U           udp_data_len;
    NET_CHK_SUM          udp_chk_sum;
    CPU_BOOLEAN          udp_chk_sum_valid;
    NET_BUF             *p_buf_next;
    NET_BUF_HDR         *p_buf_next_hdr;
    CPU_BOOLEAN          udp_chk_sum_ipv6;



                                                                /* ---------------- VALIDATE UDP PORTS ---------------- */
    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->TransportPortSrc,  &p_udp_hdr->PortSrc);
    if (p_buf_hdr->TransportPortSrc  == NET_UDP_PORT_NBR_RESERVED) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrPortSrcCtr);
       *p_err = NET_UDP_ERR_INVALID_PORT_NBR;
        return;
    }

    NET_UTIL_VAL_COPY_GET_NET_16(&p_buf_hdr->TransportPortDest, &p_udp_hdr->PortDest);
    if (p_buf_hdr->TransportPortDest == NET_UDP_PORT_NBR_RESERVED) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrPortDestCtr);
       *p_err = NET_UDP_ERR_INVALID_PORT_NBR;
        return;
    }


                                                                /* ------------ VALIDATE UDP DATAGRAM LEN ------------- */
                                                                /* See Note #3.                                         */
    NET_UTIL_VAL_COPY_GET_NET_16(&udp_tot_len, &p_udp_hdr->DatagramLen);
    p_buf_hdr->TransportTotLen = udp_tot_len;
    if (p_buf_hdr->TransportTotLen < NET_UDP_TOT_LEN_MIN) {      /* If datagram len <  min tot     len, rtn err.         */
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrDatagramLenCtr);
       *p_err = NET_UDP_ERR_INVALID_LEN;
        return;
    }
    if (p_buf_hdr->TransportTotLen > NET_UDP_TOT_LEN_MAX) {      /* If datagram len >  max tot     len, rtn err.         */
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrDatagramLenCtr);
       *p_err = NET_UDP_ERR_INVALID_LEN;
        return;
    }

    if (p_buf_hdr->TransportTotLen != p_buf_hdr->IP_DatagramLen) {/* If datagram len != IP datagram len, rtn err.         */
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrDatagramLenCtr);
       *p_err = NET_UDP_ERR_INVALID_LEN;
        return;
    }


                                                                /* --------------- VALIDATE UDP CHK SUM --------------- */
                                                                /* See Note #4.                                         */
    NET_UTIL_VAL_COPY_GET_NET_16(&udp_chk_sum, &p_udp_hdr->ChkSum);

    udp_chk_sum_ipv6 = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_IPv6_FRAME);

    if (udp_chk_sum != NET_UDP_HDR_CHK_SUM_NONE) {              /* If chk sum rx'd, verify chk sum (see Note #4d).      */
                                                                /* Prepare UDP chk sum pseudo-hdr  (see Note #4e).      */

        if (udp_chk_sum_ipv6 == DEF_NO) {
#ifdef NET_IPv4_MODULE_EN
#ifdef NET_UDP_CHK_SUM_OFFLOAD_RX
            udp_chk_sum_valid          =  DEF_YES;
           (void)&udp_chk_sum_valid;
#else
            udp_pseudo_hdr.AddrSrc     = (NET_IPv4_ADDR)NET_UTIL_HOST_TO_NET_32(p_buf_hdr->IP_AddrSrc);
            udp_pseudo_hdr.AddrDest    = (NET_IPv4_ADDR)NET_UTIL_HOST_TO_NET_32(p_buf_hdr->IP_AddrDest);
            udp_pseudo_hdr.Zero        = (CPU_INT08U )0x00u;
            udp_pseudo_hdr.Protocol    = (CPU_INT08U )NET_IP_HDR_PROTOCOL_UDP;
            udp_pseudo_hdr.DatagramLen = (CPU_INT16U )NET_UTIL_HOST_TO_NET_16(p_buf_hdr->TransportTotLen);
            udp_chk_sum_valid          =  NetUtil_16BitOnesCplChkSumDataVerify((void     *) p_buf,
                                                                               (void     *)&udp_pseudo_hdr,
                                                                               (CPU_INT16U) NET_UDP_PSEUDO_HDR_SIZE,
                                                                               (NET_ERR  *) p_err);
#endif
#else
            udp_chk_sum_valid          = DEF_FAIL;
#endif
        } else {
#ifdef NET_IPv6_MODULE_EN
#ifdef NET_UDP_CHK_SUM_OFFLOAD_RX
            udp_chk_sum_valid                = DEF_YES;
           (void)&udp_chk_sum_valid;
#else
            ipv6_pseudo_hdr.AddrSrc          = p_buf_hdr->IPv6_AddrSrc;
            ipv6_pseudo_hdr.AddrDest         = p_buf_hdr->IPv6_AddrDest;
            ipv6_pseudo_hdr.UpperLayerPktLen = NET_UTIL_HOST_TO_NET_32(p_buf_hdr->TransportTotLen);
            ipv6_pseudo_hdr.Zero             = 0x00u;
            ipv6_pseudo_hdr.NextHdr          = NET_UTIL_NET_TO_HOST_16(NET_IP_HDR_PROTOCOL_UDP);
            udp_chk_sum_valid                = NetUtil_16BitOnesCplChkSumDataVerify((void     *) p_buf,
                                                                                    (void     *)&ipv6_pseudo_hdr,
                                                                                                 NET_IPv6_PSEUDO_HDR_SIZE,
                                                                                                 p_err);
#endif
#else
            udp_chk_sum_valid = DEF_FAIL;
#endif
        }

#ifndef NET_UDP_CHK_SUM_OFFLOAD_RX
        if (udp_chk_sum_valid != DEF_OK) {
            NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrChkSumCtr);
           *p_err = NET_UDP_ERR_INVALID_CHK_SUM;
            return;
        }
#endif

        DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_RX_UDP_CHK_SUM_VALID);

    } else {                                                    /* Else discard or flag NO rx'd chk sum (see Note #4d3).*/
#if (NET_UDP_CFG_RX_CHK_SUM_DISCARD_EN != DEF_DISABLED)
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrChkSumCtr);
       *p_err = NET_UDP_ERR_INVALID_CHK_SUM;
        return;
#endif
#if 0                                                           /* Clr'd in NetBuf_Get() [see Note #4d3B].              */
        DEF_BIT_CLR(p_buf_hdr->Flags, NET_BUF_FLAG_RX_UDP_CHK_SUM_VALID);
#endif
    }


                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
                                                                /* Calc UDP data len/ix (see Note #5a).                 */
    p_buf_hdr->TransportHdrLen = NET_UDP_HDR_SIZE;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_buf_hdr->TransportHdrLen > udp_tot_len) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrDataLenCtr);
       *p_err = NET_UDP_ERR_INVALID_LEN_DATA;
        return;
    }
    if (p_buf_hdr->TransportHdrLen > p_buf_hdr->DataLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.RxHdrDataLenCtr);
       *p_err = NET_UDP_ERR_INVALID_LEN_DATA;
        return;
    }
#endif
    udp_data_len                 =  udp_tot_len - p_buf_hdr->TransportHdrLen;
    p_buf_hdr->TransportDataLen  =  udp_data_len;

    p_buf_hdr->DataLen          -= (NET_BUF_SIZE) p_buf_hdr->TransportHdrLen;
    p_buf_hdr->DataIx            = (CPU_INT16U  )(p_buf_hdr->TransportHdrIx + p_buf_hdr->TransportHdrLen);
    p_buf_hdr->ProtocolHdrType   =  NET_PROTOCOL_TYPE_APP;

    p_buf_next = p_buf_hdr->NextBufPtr;
    while (p_buf_next != (NET_BUF *)0) {                         /* Calc ALL pkt bufs' data len/ix    (see Note #5b).    */
        p_buf_next_hdr                   = &p_buf_next->Hdr;
        p_buf_next_hdr->DataIx           =  p_buf_next_hdr->TransportHdrIx;
        p_buf_next_hdr->TransportHdrLen  =  0u;                  /* NULL UDP hdr  len in each pkt buf.                   */
        p_buf_next_hdr->TransportTotLen  =  udp_tot_len;         /* Dup  UDP tot  len & ...                              */
        p_buf_next_hdr->TransportDataLen =  udp_data_len;        /* ...      data len in each pkt buf (see Note #5c).    */
        p_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_APP;
        p_buf_next                       =  p_buf_next_hdr->NextBufPtr;
    }


   (void)&udp_chk_sum_valid;
   (void)&udp_pseudo_hdr;

   *p_err = NET_UDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetUDP_RxPktDemuxDatagram()
*
* Description : Demultiplex UDP datagram to appropriate socket or application connection.
*
* Argument(s) : p_buf        Pointer to network buffer that received UDP datagram.
*               ----        Argument checked in NetUDP_Rx().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*                               NET_ERR_RX_DEST                 Invalid destination; no connection available
*                                                                   for received packet.
*
*                                                               -------- RETURNED BY NetSock_Rx() : --------
*                               NET_SOCK_ERR_NONE               UDP datagram successfully received to
*                                                                   socket      connection.
*
*                                                               - RETURNED BY NetUDP_RxPktDemuxAppData() : -
*                               NET_APP_ERR_NONE                UDP datagram successfully received to
*                                                                   application connection.
*                               NET_INIT_ERR_NOT_COMPLETED         Network initialization NOT complete.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Rx().
*
* Note(s)     : (1) (a) Attempt demultiplex of received UDP datagram to socket connections first, if enabled.
*
*                   (b) On any error(s), attempt demultiplex to application connections, if enabled.
*
*               (2) When network buffer is demultiplexed to socket or application receive, the buffer's reference
*                   counter is NOT incremented since the UDP layer does NOT maintain a reference to the buffer.
*********************************************************************************************************
*/

static  void  NetUDP_RxPktDemuxDatagram (NET_BUF  *p_buf,
                                         NET_ERR  *p_err)
{
    NetSock_Rx(p_buf, p_err);
}


/*
*********************************************************************************************************
*                                         NetUDP_RxPktFree()
*
* Description : Free network buffer(s).
*
* Argument(s) : p_buf        Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_RxAppData().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetUDP_RxPktFree (NET_BUF  *p_buf)
{
   (void)NetBuf_FreeBufList((NET_BUF *)p_buf,
                            (NET_CTR *)0);
}


/*
*********************************************************************************************************
*                                        NetUDP_RxPktDiscard()
*
* Description : On any UDP receive error(s), discard UDP packet(s) & buffer(s).
*
* Argument(s) : p_buf        Pointer to network buffer.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Rx(),
*               NetUDP_RxAppData().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetUDP_RxPktDiscard (NET_BUF  *p_buf,
                                   NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.UDP.RxPktDiscardedCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBufList((NET_BUF *)p_buf,
                            (NET_CTR *)pctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                             NetUDP_Tx()
*
* Description : (1) Prepare & transmit UDP datagram packet(s) :
*
*                   (a) Validate transmit packet
*                   (b) Prepare  UDP datagram header
*                   (c) Transmit UDP packet
*                   (d) Update   transmit statistics
*
*
* Argument(s) : p_buf        Pointer to network buffer to transmit UDP packet.
*               ----        Argument validated in NetUDP_TxAppDataHandler().
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*
*               TOS         Specific TOS to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IP_TTL_MIN                  Minimum TTL transmit value   (1)
*                               NET_IP_TTL_MAX                  Maximum TTL transmit value (255)
*                               NET_IP_TTL_DFLT                 Default TTL transmit value (128)
*                               NET_IP_TTL_NONE                 Replace with default TTL
*
*               flags_udp   Flags to select UDP transmit options (see Note #2); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #2a).
*
*               flags_ip    Flags to select IP  transmit options; bit-field flags logically OR'd :
*
*                               NET_IP_FLAG_NONE                No  IP transmit flags selected.
*                               NET_IP_FLAG_TX_DONT_FRAG        Set IP 'Don't Frag' flag.
*
*               p_opts_ip    Pointer to one or more IP options configuration data structures
*                               (see 'net_ip.h  IP HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IP_OPT_CFG_ROUTE_TS         Route &/or Internet Timestamp options configuration.
*                               NET_IP_OPT_CFG_SECURITY         Security options configuration
*                                                                   (see 'net_ip.c  Note #1e').
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                UDP datagram(s) successfully prepared &
*                                                                   transmitted to network layer.
*
*                                                               -- RETURNED BY NetUDP_TxPktValidate() : ---
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*
*                                                               -- RETURNED BY NetUDP_TxPktPrepareHdr() : -
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*
*                                                               --- RETURNED BY NetUDP_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*
*                                                               -------- RETURNED BY NetIP_Tx() : ---------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_TxAppDataHandler().
*
* Note(s)     : (2) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (3) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/
#ifdef  NET_IPv4_MODULE_EN
static  void  NetUDP_TxIPv4 (NET_BUF           *p_buf,
                             NET_IPv4_ADDR      src_addr,
                             NET_UDP_PORT_NBR   src_port,
                             NET_IPv4_ADDR      dest_addr,
                             NET_UDP_PORT_NBR   dest_port,
                             NET_IPv4_TOS       TOS,
                             NET_IPv4_TTL       TTL,
                             NET_UDP_FLAGS      flags_udp,
                             NET_IPv4_FLAGS     flags_ip,
                             void              *p_opts_ip,
                             NET_ERR           *p_err)
{
    NET_BUF_HDR    *p_buf_hdr;
    NET_ERR         err;


                                                                /* --------------- VALIDATE UDP TX PKT ---------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetUDP_TxPktValidate(p_buf_hdr,
                         src_port,
                         dest_port,
                         flags_udp,
                         p_err);
    switch (*p_err) {
        case NET_UDP_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        case NET_UDP_ERR_INVALID_LEN_DATA:
        case NET_UDP_ERR_INVALID_PORT_NBR:
        case NET_UDP_ERR_INVALID_FLAG:
        default:
             NetUDP_TxPktDiscard(p_buf, &err);
             return;
    }
#endif


                                                                /* ------------------ PREPARE UDP HDR ----------------- */
    NetUDP_TxPktPrepareHdr(p_buf,
                           p_buf_hdr,
                          &src_addr,
                           src_port,
                          &dest_addr,
                           dest_port,
                           flags_udp,
                           p_err);


                                                                /* -------------------- TX UDP PKT -------------------- */
    switch (*p_err) {
        case NET_UDP_ERR_NONE:
             NetIPv4_Tx(p_buf,
                        src_addr,
                        dest_addr,
                        TOS,
                        TTL,
                        flags_ip,
                        p_opts_ip,
                        p_err);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        case NET_UTIL_ERR_NULL_SIZE:
        case NET_UTIL_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetUDP_TxPktDiscard(p_buf, &err);
             return;
    }


                                                                /* ----------------- UPDATE TX STATS ------------------ */
    switch (*p_err) {                                            /* Chk err from NetIP_Tx().                             */
        case NET_IPv4_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.UDP.TxDgramCtr);
            *p_err = NET_UDP_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxPktDiscardedCtr);
                                                                /* Rtn err from NetIP_Tx().                             */
             return;


        case NET_IPv4_ERR_TX_PKT:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxPktDiscardedCtr);
            *p_err = NET_ERR_TX;
             return;


        default:
             NetUDP_TxPktDiscard(p_buf, p_err);
             return;
    }
}
#endif


/*
*********************************************************************************************************
*                                            NetUDP_TxIPv6()
*
* Description : (1) Prepare & transmit UDP datagram packet(s) :
*
*                   (a) Validate transmit packet
*                   (b) Prepare  UDP datagram header
*                   (c) Transmit UDP packet
*                   (d) Update   transmit statistics
*
*
* Argument(s) : p_buf        Pointer to network buffer to transmit UDP packet.
*               ----        Argument validated in NetUDP_TxAppDataHandler().
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*
*               TOS         Specific TOS to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TYPE OF SERVICE (TOS) DEFINES').
*
*               TTL         Specific TTL to transmit UDP/IP packet
*                               (see 'net_ip.h  IP HEADER TIME-TO-LIVE (TTL) DEFINES') :
*
*                               NET_IP_TTL_MIN                  Minimum TTL transmit value   (1)
*                               NET_IP_TTL_MAX                  Maximum TTL transmit value (255)
*                               NET_IP_TTL_DFLT                 Default TTL transmit value (128)
*                               NET_IP_TTL_NONE                 Replace with default TTL
*
*               flags_udp   Flags to select UDP transmit options (see Note #2); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #2a).
*
*               flags_ip    Flags to select IP  transmit options; bit-field flags logically OR'd :
*
*                               NET_IP_FLAG_NONE                No  IP transmit flags selected.
*                               NET_IP_FLAG_TX_DONT_FRAG        Set IP 'Don't Frag' flag.
*
*               p_opts_ip    Pointer to one or more IP options configuration data structures
*                               (see 'net_ip.h  IP HEADER OPTION CONFIGURATION DATA TYPES') :
*
*                               NULL                            NO IP transmit options configuration.
*                               NET_IP_OPT_CFG_ROUTE_TS         Route &/or Internet Timestamp options configuration.
*                               NET_IP_OPT_CFG_SECURITY         Security options configuration
*                                                                   (see 'net_ip.c  Note #1e').
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                UDP datagram(s) successfully prepared &
*                                                                   transmitted to network layer.
*
*                                                               -- RETURNED BY NetUDP_TxPktValidate() : ---
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*
*                                                               -- RETURNED BY NetUDP_TxPktPrepareHdr() : -
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*
*                                                               --- RETURNED BY NetUDP_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet(s) discarded.
*
*                                                               -------- RETURNED BY NetIP_Tx() : ---------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_TxAppDataHandler().
*
* Note(s)     : (2) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (3) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
static  void  NetUDP_TxIPv6(NET_BUF                 *p_buf,
                            NET_IPv6_ADDR           *p_src_addr,
                            NET_UDP_PORT_NBR         src_port,
                            NET_IPv6_ADDR           *p_dest_addr,
                            NET_UDP_PORT_NBR         dest_port,
                            NET_IPv6_TRAFFIC_CLASS   traffic_class,
                            NET_IPv6_FLOW_LABEL      flow_label,
                            NET_IPv6_HOP_LIM         hop_lim,
                            NET_UDP_FLAGS            flags_udp,
                            NET_ERR                 *p_err)
{
    NET_BUF_HDR    *p_buf_hdr;
    NET_ERR         err;


                                                                /* --------------- VALIDATE UDP TX PKT ---------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetUDP_TxPktValidate(p_buf_hdr,
                         src_port,
                         dest_port,
                         flags_udp,
                         p_err);
    switch (*p_err) {
        case NET_UDP_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        case NET_UDP_ERR_INVALID_LEN_DATA:
        case NET_UDP_ERR_INVALID_PORT_NBR:
        case NET_UDP_ERR_INVALID_FLAG:
        default:
             NetUDP_TxPktDiscard(p_buf, &err);
             return;
    }
#endif


                                                                /* ------------------ PREPARE UDP HDR ----------------- */
    NetUDP_TxPktPrepareHdr(p_buf,
                           p_buf_hdr,
                           p_src_addr,
                           src_port,
                           p_dest_addr,
                           dest_port,
                           flags_udp,
                           p_err);


                                                                /* -------------------- TX UDP PKT -------------------- */
    switch (*p_err) {
        case NET_UDP_ERR_NONE:
             NetIPv6_Tx(                    p_buf,
                                            p_src_addr,
                                            p_dest_addr,
                        (NET_IPv6_EXT_HDR *)0,
                                            traffic_class,
                                            flow_label,
                                            hop_lim,
                                            p_err);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        case NET_UTIL_ERR_NULL_SIZE:
        case NET_UTIL_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetUDP_TxPktDiscard(p_buf, &err);
             return;
    }


                                                                /* ----------------- UPDATE TX STATS ------------------ */
    switch (*p_err) {                                            /* Chk err from NetIP_Tx().                             */
        case NET_IPv6_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.UDP.TxDgramCtr);
            *p_err = NET_UDP_ERR_NONE;
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxPktDiscardedCtr);
                                                                /* Rtn err from NetIP_Tx().                             */
             return;


        case NET_IPv6_ERR_TX_PKT:
                                                                /* See Note #3.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxPktDiscardedCtr);
            *p_err = NET_ERR_TX;
             return;


        default:
             NetUDP_TxPktDiscard(p_buf, p_err);
             return;
    }
}
#endif


/*
*********************************************************************************************************
*                                       NetUDP_TxPktValidate()
*
* Description : (1) Validate UDP transmit packet parameters & options :
*
*                   (a) Validate the following transmit packet parameters :
*
*                       (1) Supported protocols :
*                           (A) Application
*                           (B) BSD Sockets
*
*                       (2) Buffer protocol index
*                       (3) Data Length
*                       (4) Source      Port
*                       (5) Destination Port
*                       (6) Flags
*
*
* Argument(s) : p_buf_hdr    Pointer to network buffer header.
*               --------    Argument validated in NetUDP_Tx().
*
*               src_port    Source      UDP port.
*
*               dest_port   Destination UDP port.
*
*               flags_udp   Flags to select UDP transmit options (see Note #2); bit-field flags logically OR'd :
*
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #2b1).
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                Transmit packet validated.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer   type.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*                               NET_UDP_ERR_INVALID_LEN_DATA    Invalid protocol/data length.
*                               NET_UDP_ERR_INVALID_PORT_NBR    Invalid UDP port number.
*                               NET_UDP_ERR_INVALID_FLAG        Invalid UDP flag(s).
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Tx().
*
* Note(s)     : (2) (a) Only some UDP transmit flag options are implemented.  If other flag options
*                       are requested, NetUDP_Tx() handler function(s) abort & return appropriate error
*                       codes so that requested flag options are NOT silently ignored.
*
*                   (b) Some UDP transmit flag options NOT yet implemented :
*
*                       (1) NET_UDP_FLAG_TX_BLOCK
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetUDP_TxPktValidate (NET_BUF_HDR       *p_buf_hdr,
                                    NET_UDP_PORT_NBR   src_port,
                                    NET_UDP_PORT_NBR   dest_port,
                                    NET_UDP_FLAGS      flags_udp,
                                    NET_ERR           *p_err)
{
    CPU_INT16U     ix;
    CPU_INT16U     len;
    NET_UDP_FLAGS  flag_mask;


                                                                /* -------------- VALIDATE NET BUF TYPE --------------- */
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


                                                                /* ----------------- VALIDATE PROTOCOL ---------------- */
    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_APP:
        case NET_PROTOCOL_TYPE_SOCK:
        case NET_PROTOCOL_TYPE_UDP_V4:
        case NET_PROTOCOL_TYPE_UDP_V6:
             ix  = (CPU_INT16U)p_buf_hdr->DataIx;
             len = (CPU_INT16U)p_buf_hdr->DataLen;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxInvalidProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxInvalidBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    if (ix <  NET_UDP_HDR_SIZE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxInvalidBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }



                                                                /* -------------- VALIDATE TOT DATA LEN --------------- */
    if (len != p_buf_hdr->TotLen) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxHdrDataLenCtr);
       *p_err = NET_UDP_ERR_INVALID_LEN_DATA;
        return;
    }



                                                                /* ---------------- VALIDATE UDP PORTS ---------------- */
    if (src_port  == NET_UDP_PORT_NBR_RESERVED) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxHdrPortSrcCtr);
       *p_err = NET_UDP_ERR_INVALID_PORT_NBR;
        return;
    }

    if (dest_port == NET_UDP_PORT_NBR_RESERVED) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxHdrPortDestCtr);
       *p_err = NET_UDP_ERR_INVALID_PORT_NBR;
        return;
    }



                                                                /* ---------------- VALIDATE UDP FLAGS ---------------- */
    flag_mask = NET_UDP_FLAG_NONE           |
                NET_UDP_FLAG_TX_CHK_SUM_DIS |
                NET_UDP_FLAG_TX_BLOCK;                          /* See Note #2b1.                                       */
                                                                /* If any invalid flags req'd, rtn err (see Note #2a).  */
    if ((flags_udp & (NET_UDP_FLAGS)~flag_mask) != NET_UDP_FLAG_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.UDP.TxHdrFlagsCtr);
       *p_err = NET_UDP_ERR_INVALID_FLAG;
        return;
    }


   *p_err = NET_UDP_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetUDP_TxPktPrepareHdr()
*
* Description : (1) Prepare UDP header :
*
*                   (a) Update network buffer's protocol index & length controls
*
*                   (b) Prepare the transmit packet's following UDP header fields :
*
*                       (1) Source      Port
*                       (2) Destination Port
*                       (3) Datagram Length
*                       (4) Check-Sum                                   See Note #3
*
*                   (c) Convert the following UDP header fields from host-order to network-order :
*
*                       (1) Source      Port
*                       (2) Destination Port
*                       (3) Datagram Length
*                       (4) Check-Sum                                   See Note #3g
*
*
* Argument(s) : p_buf        Pointer to network buffer to transmit UDP packet.
*               ----        Argument checked   in NetUDP_Tx().
*
*               p_buf_hdr    Pointer to network buffer header.
*               --------    Argument validated in NetUDP_Tx().
*
*               src_addr    Source      IP  address.
*
*               src_port    Source      UDP port.
*               --------    Argument validated in NetUDP_TxPktValidate().
*
*               dest_addr   Destination IP  address.
*
*               dest_port   Destination UDP port.
*               ---------   Argument validated in NetUDP_TxPktValidate().
*
*               flags_udp   Flags to select UDP transmit options (see Note #2); bit-field flags logically OR'd :
*               ---------
*                               NET_UDP_FLAG_NONE               No UDP  transmit flags selected.
*                               NET_UDP_FLAG_TX_CHK_SUM_DIS     DISABLE transmit check-sums.
*                               NET_UDP_FLAG_TX_BLOCK           Transmit UDP application data with blocking,
*                                                                   if flag set; without blocking, if clear
*                                                                   (see Note #2a).
*
*                           Argument checked    in NetUDP_TxPktValidate().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_UDP_ERR_NONE                UDP header successfully prepared.
*
*                                                               - RETURNED BY NetUtil_16BitOnesCplChkSumDataCalc() : -
*                               NET_UTIL_ERR_NULL_PTR           Check-sum passed a NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Check-sum passed a zero size.
*                               NET_UTIL_ERR_INVALID_PROTOCOL   Invalid data packet protocol.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Tx().
*
* Note(s)     : (2) Some UDP transmit flag options NOT yet implemented :
*
*                   (a) NET_UDP_FLAG_TX_BLOCK
*
*                   See also 'NetUDP_TxPktValidate()  Note #2b'.
*
*               (3) (a) UDP header Check-Sum MUST be calculated AFTER the entire UDP header has been prepared.
*                       In addition, ALL multi-octet words are converted from host-order to network-order
*                       since "the sum of 16-bit integers can be computed in either byte order" [RFC #1071,
*                       Section 2.(B)].
*
*                   (b) RFC #1122, Section 4.1.3.4 states that "an application MAY optionally be able to
*                       control whether a UDP checksum will be generated".
*
*                   (c) Although neither RFC #768 nor RFC #1122, Sections 4.1 expressly specifies, it is
*                       assumed that that the UDP header Check-Sum field MUST be cleared to '0' BEFORE the
*                       UDP header Check-Sum is calculated.
*
*                       See also 'net_ip.c    NetIP_TxPktPrepareHdr()   Note #6b',
*                                'net_icmp.c  NetICMP_TxMsgErr()        Note #6b',
*                                'net_icmp.c  NetICMP_TxMsgReq()        Note #7b',
*                                'net_icmp.c  NetICMP_TxMsgReply()      Note #5b',
*                                'net_tcp.c   NetTCP_TxPktPrepareHdr()  Note #3b'.
*
*                   (d) (1) In addition to the UDP datagram header & data, the UDP Check-Sum calculation
*                           includes "a pseudo header of information from the IP header ... conceptually
*                           prefixed to the UDP header [which] contains the source address, the destination
*                           address, the protocol, and the UDP length" (see RFC #768, Section 'Fields :
*                           Checksum').
*
*                       (2) Since network check-sum functions REQUIRE that 16-bit one's-complement check-
*                           sum calculations be performed on headers & data arranged in network-order (see
*                           'net_util.c  NetUtil_16BitOnesCplChkSumDataCalc()  Note #3'), UDP pseudo-header
*                           values MUST be set or converted to network-order.
*
*                   (e) RFC #768, Section 'Fields : Checksum' specifies that "the data [is] padded with zero
*                       octets at the end (if necessary) to make a multiple of two octets".
*
*                       See also 'net_util.c  NetUtil_16BitSumDataCalc()  Note #8'.
*
*                   (f) "If the computed checksum is zero" (i.e. one's-complement positive zero -- all
*                        bits equal to zero), then "it is transmitted as all ones (the equivalent in
*                        one's complement arithmetic" (i.e. one's-complement negative zero -- all bits
*                        equal to one) [RFC #768, Section 'Fields : Checksum'].
*
*                   (g) The UDP header Check-Sum field is returned in network-order & MUST NOT be re-
*                       converted back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumDataCalc()
*                       Note #4').
*********************************************************************************************************
*/

static  void  NetUDP_TxPktPrepareHdr (NET_BUF           *p_buf,
                                      NET_BUF_HDR       *p_buf_hdr,
                                      void              *p_src_addr,
                                      NET_UDP_PORT_NBR   src_port,
                                      void              *p_dest_addr,
                                      NET_UDP_PORT_NBR   dest_port,
                                      NET_UDP_FLAGS      flags_udp,
                                      NET_ERR           *p_err)
{
#ifndef  NET_UDP_CHK_SUM_OFFLOAD_TX
#ifdef  NET_IPv4_MODULE_EN
    NET_UDP_PSEUDO_HDR    udp_pseudo_hdr;
    NET_IPv4_ADDR        *p_src_addrv4;
    NET_IPv4_ADDR        *p_dest_addrv4;
#endif  /* NET_IPv4_MODULE_EN           */
#ifdef  NET_IPv6_MODULE_EN
    NET_IPv6_PSEUDO_HDR   ipv6_pseudo_hdr;
#endif  /* NET_IPv6_MODULE_EN           */
#endif  /* NET_UDP_CHK_SUM_OFFLOAD_TX   */
    NET_UDP_HDR          *p_udp_hdr;
    NET_CHK_SUM           udp_chk_sum;
    CPU_BOOLEAN           tx_chk_sum;


                                                                /* ----------------- UPDATE BUF CTRLS ----------------- */
    p_buf_hdr->TransportHdrLen   =  NET_UDP_HDR_SIZE;
    p_buf_hdr->TransportHdrIx    =  p_buf_hdr->DataIx - p_buf_hdr->TransportHdrLen;

    p_buf_hdr->TotLen          += (NET_BUF_SIZE)p_buf_hdr->TransportHdrLen;
    p_buf_hdr->TransportTotLen   = (CPU_INT16U  )p_buf_hdr->TotLen;
    p_buf_hdr->TransportDataLen  = (CPU_INT16U  )p_buf_hdr->DataLen;




                                                                /* ----------------- PREPARE UDP HDR ------------------ */
    p_udp_hdr = (NET_UDP_HDR *)&p_buf->DataPtr[p_buf_hdr->TransportHdrIx];



                                                                /* ---------------- PREPARE UDP PORTS ----------------- */
    NET_UTIL_VAL_COPY_SET_NET_16(&p_udp_hdr->PortSrc,  &src_port);
    NET_UTIL_VAL_COPY_SET_NET_16(&p_udp_hdr->PortDest, &dest_port);



                                                                /* ------------- PREPARE UDP DATAGRAM LEN ------------- */
    NET_UTIL_VAL_COPY_SET_NET_16(&p_udp_hdr->DatagramLen, &p_buf_hdr->TransportTotLen);



                                                                /* --------------- PREPARE UDP CHK SUM ---------------- */
#if (NET_UDP_CFG_TX_CHK_SUM_EN == DEF_ENABLED)
    tx_chk_sum = DEF_BIT_IS_CLR(flags_udp, NET_UDP_FLAG_TX_CHK_SUM_DIS);
#else
    tx_chk_sum = DEF_NO;
#endif

    if (tx_chk_sum == DEF_YES) {                                /* If en'd (see Note #3b), prepare UDP tx chk sum.      */
        NET_UTIL_VAL_SET_NET_16(&p_udp_hdr->ChkSum, 0x0000u);    /* Clr UDP chk sum            (see Note #3c).           */
                                                                /* Cfg UDP chk sum pseudo-hdr (see Note #3d).           */
        if (DEF_BIT_IS_CLR(p_buf_hdr->Flags, NET_BUF_FLAG_IPv6_FRAME)) {
#ifdef  NET_IPv4_MODULE_EN
                                                                    /* Calc UDP chk sum.                                    */
#ifdef NET_UDP_CHK_SUM_OFFLOAD_TX
            udp_chk_sum                =  0u;
#else
            p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_UDP_V4;
            p_buf_hdr->ProtocolHdrTypeTransport = NET_PROTOCOL_TYPE_UDP_V4;

            p_src_addrv4               = (NET_IPv4_ADDR *)p_src_addr;
            p_dest_addrv4              = (NET_IPv4_ADDR *)p_dest_addr;
            udp_pseudo_hdr.AddrSrc     = (NET_IPv4_ADDR)NET_UTIL_HOST_TO_NET_32(*p_src_addrv4);
            udp_pseudo_hdr.AddrDest    = (NET_IPv4_ADDR)NET_UTIL_HOST_TO_NET_32(*p_dest_addrv4);
            udp_pseudo_hdr.Zero        = (CPU_INT08U )0x00u;
            udp_pseudo_hdr.Protocol    = (CPU_INT08U )NET_IP_HDR_PROTOCOL_UDP;
            udp_pseudo_hdr.DatagramLen = (CPU_INT16U )NET_UTIL_HOST_TO_NET_16(p_buf_hdr->TransportTotLen);

            udp_chk_sum                =  NetUtil_16BitOnesCplChkSumDataCalc((void     *) p_buf,
                                                                             (void     *)&udp_pseudo_hdr,
                                                                             (CPU_INT16U) NET_UDP_PSEUDO_HDR_SIZE,
                                                                             (NET_ERR  *) p_err);
#endif
#endif
        } else {
#ifdef  NET_IPv6_MODULE_EN


#ifdef NET_UDP_CHK_SUM_OFFLOAD_TX
            udp_chk_sum = 0u;
#else
            Mem_Copy(&p_buf_hdr->IPv6_AddrSrc,  p_src_addr,  NET_IPv6_ADDR_SIZE);
            Mem_Copy(&p_buf_hdr->IPv6_AddrDest, p_dest_addr, NET_IPv6_ADDR_SIZE);

            p_buf_hdr->ProtocolHdrType          = NET_PROTOCOL_TYPE_UDP_V6;
            p_buf_hdr->ProtocolHdrTypeTransport = NET_PROTOCOL_TYPE_UDP_V6;

            ipv6_pseudo_hdr.AddrSrc          = p_buf_hdr->IPv6_AddrSrc;
            ipv6_pseudo_hdr.AddrDest         = p_buf_hdr->IPv6_AddrDest;
            ipv6_pseudo_hdr.UpperLayerPktLen = (CPU_INT32U)NET_UTIL_HOST_TO_NET_32(p_buf_hdr->TransportTotLen);
            ipv6_pseudo_hdr.Zero             = (CPU_INT16U)0x00u;
            ipv6_pseudo_hdr.NextHdr          = (CPU_INT32U)NET_UTIL_HOST_TO_NET_16(NET_IP_HDR_PROTOCOL_UDP);
            udp_chk_sum = NetUtil_16BitOnesCplChkSumDataCalc((void     *) p_buf,
                                                             (void     *)&ipv6_pseudo_hdr,
                                                             (CPU_INT16U) NET_IPv6_PSEUDO_HDR_SIZE,
                                                             (NET_ERR  *) p_err);
#endif
#endif
        }

#ifndef  NET_UDP_CHK_SUM_OFFLOAD_TX
        if (*p_err != NET_UTIL_ERR_NONE) {
             return;
        }
#endif

        if (udp_chk_sum == NET_UDP_HDR_CHK_SUM_POS_ZERO) {      /* If equal to one's-cpl pos zero, ...                  */
            udp_chk_sum  = NET_UDP_HDR_CHK_SUM_NEG_ZERO;        /* ...  set to one's-cpl neg zero (see Note #3f).       */
        }

    } else {                                                    /* Else tx NO chk sum.                                  */
        udp_chk_sum = NET_UTIL_HOST_TO_NET_16(NET_UDP_HDR_CHK_SUM_NONE);
    }

    NET_UTIL_VAL_COPY_16(&p_udp_hdr->ChkSum, &udp_chk_sum);      /* Copy UDP chk sum in net order  (see Note #3g).       */



   *p_err = NET_UDP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetUDP_TxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf        Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Tx(),
*               NetUDP_TxAppDataHandler().
*
* Note(s)     : (1) (a) Although UDP Transmit initially requests the network buffer for transmit, the UDP
*                       layer does NOT maintain a reference to the buffer.
*
*                   (b) Also, since the network interface transmit deallocation task frees ALL unreferenced
*                       buffers after successful transmission, the UDP layer must NOT free the buffer.
*
*                       See also 'net_if.c  NetIF_TxDeallocTaskHandler()  Note #1c'.
*********************************************************************************************************
*/

static  void  NetUDP_TxPktFree (NET_BUF  *p_buf)
{
   (void)&p_buf;                                                 /* Prevent 'variable unused' warning (see Note #1).     */
}


/*
*********************************************************************************************************
*                                        NetUDP_TxPktDiscard()
*
* Description : On any UDP transmit packet error(s), discard packet & buffer.
*
* Argument(s) : p_buf        Pointer to network buffer.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_Tx(),
*               NetUDP_TxAppDataHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetUDP_TxPktDiscard (NET_BUF  *p_buf,
                                   NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.UDP.TxPktDiscardedCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)pctr);

   *p_err = NET_ERR_TX;
}

/*
*********************************************************************************************************
*                                         NetUDP_GetTxDataIx()
*
* Description : Get the offset of a buffer at which the UDP data can be written.
*
* Argument(s) : if_nbr
*
*               protocol
*
*               data_len
*
*               flags
*
*               p_ix
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error.
*
* Return(s)   : none.
*
* Caller(s)   : NetUDP_TxAppDataHandlerIPv4(),
*               NetUDP_TxAppDataHandlerIPv6.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static  void  NetUDP_GetTxDataIx (NET_IF_NBR          if_nbr,
                                  NET_PROTOCOL_TYPE   protocol,
                                  CPU_INT16U          data_len,
                                  NET_UDP_FLAGS       flags,
                                  CPU_INT16U         *p_ix,
                                  NET_ERR            *p_err)
{
    NET_MTU  mtu;


    *p_ix += NET_UDP_HDR_SIZE_MAX;

    switch (protocol) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_PROTOCOL_TYPE_UDP_V4:
             mtu = NetIF_MTU_GetProtocol(if_nbr, NET_PROTOCOL_TYPE_UDP_V4, NET_IF_FLAG_NONE, p_err);

             if (*p_err != NET_IF_ERR_NONE) {
                return;
             }

             NetIPv4_TxIxDataGet(if_nbr,
                                 data_len,
                                 mtu,
                                 p_ix,
                                 p_err);
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_PROTOCOL_TYPE_UDP_V6:
             mtu = NetIF_MTU_GetProtocol(if_nbr, NET_PROTOCOL_TYPE_UDP_V6, NET_IF_FLAG_NONE, p_err);

             if (*p_err != NET_IF_ERR_NONE) {
                return;
             }

             NetIPv6_GetTxDataIx(if_nbr,
                                 DEF_NULL,
                                 data_len,
                                 mtu,
                                 p_ix,
                                 p_err);
             break;
#endif

        default:
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

   (void)&flags;
}
