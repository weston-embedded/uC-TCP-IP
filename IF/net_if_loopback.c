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
*                                  NETWORK LOOPBACK INTERFACE LAYER
*
* Filename : net_if_loopback.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports internal loopback communication.
*
*                (a) Internal loopback interface is NOT linked to, associated with, or handled by
*                    any physical network device(s) & therefore has NO physical protocol overhead.
*
*            (2) REQUIREs the following network protocol files in network directories :
*
*                (a) Network Interface Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\IF\
*
*                         where
*                                 <Network Protocol Suite>    directory path for network protocol suite
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_IF_MODULE_LOOPBACK


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../Source/net_cfg_net.h"

#include  "net_if_loopback.h"
#include  "net_if.h"
#include  "../Source/net_type.h"

#ifdef NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#endif

#ifdef NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IF_LOOPBACK_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/


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

static  NET_BUF        *NetIF_Loopback_RxQ_Head;   /* Ptr to loopback IF rx Q head.                        */
static  NET_BUF        *NetIF_Loopback_RxQ_Tail;   /* Ptr to loopback IF rx Q tail.                        */

static  NET_STAT_CTR    NetIF_Loopback_RxQ_PktCtr; /*    Net loopback IF rx pkts ctr.                      */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                                    /* ----------- RX FNCTS ----------- */

static  void         NetIF_Loopback_RxQ_Add                  (NET_BUF            *p_buf);

static  NET_BUF     *NetIF_Loopback_RxQ_Get                  (void);

static  void         NetIF_Loopback_RxQ_Remove               (NET_BUF            *p_buf);

static  void         NetIF_Loopback_RxQ_Unlink               (NET_BUF            *p_buf);


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void         NetIF_Loopback_RxPktValidateBuf         (NET_BUF_HDR        *p_buf_hdr,
                                                              NET_ERR            *p_err);
#endif

static  void         NetIF_Loopback_RxPktDemux               (NET_BUF            *p_buf,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_RxPktDiscard             (NET_BUF            *p_buf,
                                                              NET_ERR            *p_err);


                                                                                    /* ----------- TX FNCTS ----------- */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void         NetIF_Loopback_TxPktValidate            (NET_BUF_HDR        *p_buf_hdr,
                                                              NET_ERR            *p_err);
#endif

static  void         NetIF_Loopback_TxPktFree                (NET_BUF            *p_buf);

static  void         NetIF_Loopback_TxPktDiscard             (NET_BUF            *p_buf,
                                                              NET_ERR            *p_err);


                                                                                    /* ---------- API FNCTS ----------- */

static  void         NetIF_Loopback_IF_Add                   (NET_IF             *p_if,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_IF_Start                 (NET_IF             *p_if,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_IF_Stop                  (NET_IF             *p_if,
                                                              NET_ERR            *p_err);


                                                                                    /* ---------- MGMT FNCTS ---------- */

static  void         NetIF_Loopback_AddrHW_Get               (NET_IF             *p_if,
                                                              CPU_INT08U         *p_addr_hw,
                                                              CPU_INT08U         *p_addr_len,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_AddrHW_Set               (NET_IF             *p_if,
                                                              CPU_INT08U         *p_addr_hw,
                                                              CPU_INT08U          addr_len,
                                                              NET_ERR            *p_err);

static  CPU_BOOLEAN  NetIF_Loopback_AddrHW_IsValid           (NET_IF             *p_if,
                                                              CPU_INT08U         *p_addr_hw);


static  void         NetIF_Loopback_AddrMulticastAdd         (NET_IF             *p_if,
                                                              CPU_INT08U         *p_addr_protocol,
                                                              CPU_INT08U          addr_protocol_len,
                                                              NET_PROTOCOL_TYPE   addr_protocol_type,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_AddrMulticastRemove      (NET_IF             *p_if,
                                                              CPU_INT08U         *p_addr_protocol,
                                                              CPU_INT08U          addr_protocol_len,
                                                              NET_PROTOCOL_TYPE   addr_protocol_type,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_AddrMulticastProtocolToHW(NET_IF             *p_if,
                                                              CPU_INT08U         *p_addr_protocol,
                                                              CPU_INT08U          addr_protocol_len,
                                                              NET_PROTOCOL_TYPE   addr_protocol_type,
                                                              CPU_INT08U         *p_addr_hw,
                                                              CPU_INT08U         *p_addr_hw_len,
                                                              NET_ERR            *p_err);



static  void         NetIF_Loopback_BufPoolCfgValidate       (NET_IF             *p_if,
                                                              NET_ERR            *p_err);


static  void         NetIF_Loopback_MTU_Set                  (NET_IF             *p_if,
                                                              NET_MTU             mtu,
                                                              NET_ERR            *p_err);


static  CPU_INT16U   NetIF_Loopback_GetPktSizeHdr            (NET_IF             *p_if);

static  CPU_INT16U   NetIF_Loopback_GetPktSizeMin            (NET_IF             *p_if);

static  CPU_INT16U   NetIF_Loopback_GetPktSizeMax            (NET_IF             *p_if);


static  void         NetIF_Loopback_ISR_Handler              (NET_IF             *p_if,
                                                              NET_DEV_ISR_TYPE    type,
                                                              NET_ERR            *p_err);

static  void         NetIF_Loopback_IO_CtrlHandler           (NET_IF             *p_if,
                                                              CPU_INT08U          opt,
                                                              void               *p_data,
                                                              NET_ERR            *p_err);


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
const  NET_IF_API  NetIF_API_Loopback = {                                           /* Loopback IF API fnct ptrs :      */
                                          &NetIF_Loopback_IF_Add,                   /*   Init/add                       */
                                          &NetIF_Loopback_IF_Start,                 /*   Start                          */
                                          &NetIF_Loopback_IF_Stop,                  /*   Stop                           */
                                           0,                                       /*   Rx                             */
                                           0,                                       /*   Tx                             */
                                          &NetIF_Loopback_AddrHW_Get,               /*   Hw        addr get             */
                                          &NetIF_Loopback_AddrHW_Set,               /*   Hw        addr set             */
                                          &NetIF_Loopback_AddrHW_IsValid,           /*   Hw        addr valid           */
                                          &NetIF_Loopback_AddrMulticastAdd,         /*   Multicast addr add             */
                                          &NetIF_Loopback_AddrMulticastRemove,      /*   Multicast addr remove          */
                                          &NetIF_Loopback_AddrMulticastProtocolToHW,/*   Multicast addr protocol-to-hw  */
                                          &NetIF_Loopback_BufPoolCfgValidate,       /*   Buf cfg validation             */
                                          &NetIF_Loopback_MTU_Set,                  /*   MTU set                        */
                                          &NetIF_Loopback_GetPktSizeHdr,            /*   Get pkt hdr size               */
                                          &NetIF_Loopback_GetPktSizeMin,            /*   Get pkt min size               */
                                          &NetIF_Loopback_GetPktSizeMax,            /*   Get pkt max size               */
                                          &NetIF_Loopback_ISR_Handler,              /*   ISR handler                    */
                                          &NetIF_Loopback_IO_CtrlHandler            /*   I/O ctrl                       */
                                        };
#endif


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                         NetIF_Loopback_Init()
*
* Description : (1) Initialize Network Loopback Interface Module :
*
*                   (a) Initialize network loopback interface counter(s)
*                   (b) Initialize network loopback interface receive queue pointers
*                   (c) Add        network loopback interface
*                   (d) Start      network loopback interface
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network Loopback Interface module
*                                                                   successfully initialized.
*
*                                                               ------ RETURNED BY NetStat_CtrInit() : -------
*                               NET_ERR_FAULT_NULL_PTR           Argument passed a NULL pointer.
*
*                                                               --------- RETURNED BY NetIF_Add() : ----------
*                                                               -------- RETURNED BY NetIF_Start() : ---------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_LOCK_ACQUIRE      Network access NOT acquired (see Note #2b2).
*
*                                                               --------- RETURNED BY NetIF_Add() : ----------
*                               NET_IF_ERR_INVALID_POOL_TYPE    Invalid network interface buffer pool type.
*                               NET_IF_ERR_INVALID_POOL_ADDR    Invalid network interface buffer pool address.
*                               NET_IF_ERR_INVALID_POOL_SIZE    Invalid network interface buffer pool size.
*                               NET_IF_ERR_INVALID_POOL_QTY     Invalid network interface buffer pool number
*                                                                   of buffers configured.
*
*                               NET_BUF_ERR_POOL_MEM_ALLOC           Network buffer pool initialization failed.
*                               NET_BUF_ERR_INVALID_POOL_TYPE   Invalid network buffer pool type.
*                               NET_BUF_ERR_INVALID_QTY         Invalid number of network buffers configured.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size   of network buffer data areas
*                                                                   configured.
*                               NET_BUF_ERR_INVALID_IX          Invalid offset from base index into network
*                                                                   buffer data area configured.
*
*                                                               -------- RETURNED BY NetIF_Start() : ---------
*                               NET_IF_ERR_INVALID_STATE        Network interface already started.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) The following network loopback interface initialization functions MUST be sequenced
*                   as follows :
*
*                   (a) NetIF_Loopback_Init() MUST precede ALL other network loopback interface
*                           initialization functions
*
*                   (b) NetIF_Add() & NetIF_Start() MUST :
*
*                       (1) Follow  NetIF_Init()'s initialization       of the network global lock
*                               (see also 'net_if.c  NetIF_Init()   Note #2b')
*                       (2) Precede any network or application task access to the network global lock
*                               (see also 'net_if.c  NetIF_Add()    Note #2'
*                                       & 'net_if.c  NetIF_Start()  Note #2')
*********************************************************************************************************
*/

void  NetIF_Loopback_Init (NET_ERR  *p_err)
{
#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    NET_IF_NBR  if_nbr;


                                                                /* ----------- INIT NET LOOPBACK IF CTR(s) ------------ */
    NetStat_CtrInit(&NetIF_Loopback_RxQ_PktCtr, p_err);
    if (*p_err!= NET_STAT_ERR_NONE) {
         return;
    }


                                                                /* -------------- INIT NET LOOPBACK RX Q -------------- */
    NetIF_Loopback_RxQ_Head = (NET_BUF *)0;
    NetIF_Loopback_RxQ_Tail = (NET_BUF *)0;

                                                                /* ----------------- INIT LOOPBACK IF ----------------- */
                                                                /* Start Loopback IF (see Note #2b).                    */
    if_nbr = NetIF_Add((void    *)&NetIF_API_Loopback,          /* Loopback IF's                      API.              */
                       (void    *) 0,                           /* Loopback IF   does NOT support dev API.              */
                       (void    *) 0,                           /* Loopback IF   does NOT support dev BSP.              */
                       (void    *)&NetIF_Cfg_Loopback,          /* Loopback IF's                  dev cfg.              */
                       (void    *) 0,                           /* Loopback IF   does NOT support Phy API.              */
                       (void    *) 0,                           /* Loopback IF   does NOT support Phy cfg.              */
                       (NET_ERR *) p_err);
    if (*p_err!= NET_IF_ERR_NONE) {
         return;
    }


    NetIF_Start(if_nbr, p_err);                                 /* Start Loopback IF.                                   */
    if (*p_err!= NET_IF_ERR_NONE) {
         return;
    }
#endif


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetIF_Loopback_Rx()
*
* Description : (1) Receive & handle packets from the network loopback interface :
*
*                   (a) Receive packet from network loopback interface :
*                       (1) Update  receive packet counters
*                       (2) Get     receive packet from loopback receive queue
*                   (b) Validate    receive packet
*                   (c) Demultiplex receive packet to network layer protocols
*                   (d) Update      receive statistics
*
*
* Argument(s) : p_if        Pointer to network loopback interface that received a packet.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Loopback packet successfully received & processed.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_FAULT_MEM_ALLOC         NO receive packets available on loopback interface.
*
*                                                               - RETURNED BY NetIF_Loopback_RxPktValidateBuf() : -
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
*                                                               ---- RETURNED BY NetIF_Loopback_RxPktDemux() : ----
*                               NET_ERR_INVALID_PROTOCOL        Invalid loopback/network protocol.
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : Size of received packet, if NO error(s).
*
*               0,                       otherwise.
*
* Caller(s)   : NetIF_RxHandler().
*
* Note(s)     : (2) If a network interface receives a packet, its physical link must be 'UP' & the
*                   interface's physical link state is set accordingly.
*
*                   (a) An attempt to check for link state is made after an interface has been started.
*                       However, many physical layer devices, such as Ethernet physical layers require
*                       several seconds for Auto-Negotiation to complete before the link becomes
*                       established.  Thus the interface link flag is not updated until the link state
*                       timer expires & one or more attempts to check for link state have been completed.
*
*               (3) When network buffer is demultiplexed to the network layer, the buffer's reference
*                   counter is NOT incremented since the network loopback interface receive does NOT
*                   maintain a reference to the buffer.
*
*               (4) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

NET_BUF_SIZE  NetIF_Loopback_Rx (NET_IF   *p_if,
                                 NET_ERR  *p_err)
{
#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    NET_BUF       *p_buf;
    NET_BUF_HDR   *p_buf_hdr;
    NET_BUF_SIZE   size;
    NET_ERR        err;


   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

                                                                /* ---------------- UPDATE LINK STATUS ---------------- */
    p_if->Link = NET_IF_LINK_UP;                                /* See Note #2.                                         */


                                                                /* -------------------- GET RX PKT -------------------- */
    p_buf= NetIF_Loopback_RxQ_Get();                            /* Get pkt from loopback rx Q.                          */
    if (p_buf== (NET_BUF *)0) {
        NetIF_Loopback_RxPktDiscard(p_buf, &err);
       *p_err =  NET_ERR_FAULT_MEM_ALLOC;
        return (0u);
    }

    NetStat_CtrDec(&NetIF_Loopback_RxQ_PktCtr, &err);           /* Dec loopback IF's nbr q'd rx pkts avail.             */

    NET_CTR_STAT_INC(Net_StatCtrs.IFs.Loopback.RxPktCtr);


                                                                /* ------------ VALIDATE RX'D LOOPBACK PKT ------------ */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIF_Loopback_RxPktValidateBuf(p_buf_hdr, p_err);          /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIF_Loopback_RxPktDiscard(p_buf, &err);
             return (0u);
    }
#endif


                                                                /* ------------------- DEMUX RX PKT ------------------- */
    size = p_buf_hdr->TotLen;                                   /* Rtn pkt tot len/size.                                */
                                                                /* See Note #3.                                         */
    NetIF_Loopback_RxPktDemux(p_buf, p_err);


                                                                /* ----------------- UPDATE RX STATS ------------------ */
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.Loopback.RxPktCompCtr);
             break;


        case NET_ERR_RX:
                                                                /* See Note #4.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Loopback.RxPktDisCtr);
                                                                /* Rtn err from NetIF_Loopback_RxPktDemux().            */
             return (0u);


        case NET_ERR_INVALID_PROTOCOL:
        default:
             NetIF_Loopback_RxPktDiscard(p_buf, &err);
             return (0u);
    }


                                                                /* ---------------- RTN RX'D DATA SIZE ---------------- */
    return (size);



#else
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warning.          */

   *p_err =  NET_ERR_IF_LOOPBACK_DIS;

    return (0u);
#endif
}


/*
*********************************************************************************************************
*                                          NetIF_Loopback_Tx()
*
* Description : (1) Transmit packets to the network loopback interface :
*
*                   (a) Validate loopback transmit packet
*                   (b) Get      loopback receive  buffer
*                   (c) Copy     loopback transmit packet to loopback receive buffer
*                   (d) Post     loopback receive  packet to loopback receive queue
*                   (e) Signal   network interface receive task
*                   (f) Free     loopback transmit packet                                   See Note #4
*                   (g) Update   loopback transmit statistics
*
*
* Argument(s) : p_if        Pointer to network loopback interface.
*               ----        Argument validated in NetIF_TxHandler().
*
*               p_buf_tx     Pointer to network buffer to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Loopback packet successfully transmitted.
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*
*                               NET_ERR_INVALID_PROTOCOL        Invalid loopback/network protocol.
*
*                                                               - RETURNED BY NetIF_Loopback_TxPktDiscard() : -
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               ----- RETURNED BY NetIF_RxTaskSignal() : ------
*                               NET_IF_ERR_RX_Q_FULL            Network interface receive queue full.
*                               NET_IF_ERR_RX_Q_SIGNAL_FAULT    Network interface receive queue signal fault.
*
*                                                               ---------- RETURNED BY NetBuf_Get() : ---------
*                                                               ------ RETURNED BY NetBuf_GetDataPtr() : ------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffers/data areas to allocate.
*                               NET_BUF_ERR_INVALID_SIZE        Requested size is greater then the maximum
*                                                                   buffer size available.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*                               NET_BUF_ERR_INVALID_LEN         Invalid buffer length.
*
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               ------- RETURNED BY NetBuf_DataCopy() : -------
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid buffer type.
*
* Return(s)   : Size of transmitted packet, if NO error(s).
*
*               0,                          otherwise.
*
* Caller(s)   : NetIF_TxHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) Loopback buffer flag value to clear was previously initialized in NetBuf_Get() when
*                   the buffer was allocated.  This buffer flag value does NOT need to be re-cleared but
*                   is shown for completeness.
*
*               (3) On ANY error(s), network resources MUST be appropriately freed :
*
*                   (a) For loopback receive buffers NOT yet posted to the network loopback interface
*                       receive queue, the buffer MUST be freed by NetBuf_Free().
*                   (b)     Loopback receive buffer data areas that have been linked to loopback receive
*                       buffers are inherently freed    by NetBuf_Free().
*                   (c)     Loopback receive buffers that have been queued to the loopback receive queue
*                               are inherently unlinked by NetBuf_Free().
*
*               (4) Since loopback transmit packets are NOT asynchronously transmitted from network
*                   devices, they do NOT need to be asynchronously deallocated by the network interface
*                   transmit deallocation task (see 'net_if.c  NetIF_TxDeallocTaskHandler()  Note #1a').
*********************************************************************************************************
*/

NET_BUF_SIZE  NetIF_Loopback_Tx (NET_IF   *p_if,
                                 NET_BUF  *p_buf_tx,
                                 NET_ERR  *p_err)
{
#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
    NET_BUF       *p_buf_rx;
    NET_BUF_HDR   *p_buf_hdr_rx;
    NET_BUF_HDR   *p_buf_hdr_tx;
    NET_BUF_SIZE   buf_data_ix_tx;
    NET_BUF_SIZE   buf_data_ix_rx;
    NET_BUF_SIZE   buf_data_ix_rx_offset;
    NET_BUF_SIZE   buf_data_size_rx;
    NET_BUF_SIZE   buf_data_len_tx;
    NET_ERR        err;



#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                         /* --------------- VALIDATE PTR --------------- */
    if (p_buf_tx == (NET_BUF *)0) {
        NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Loopback.NullPtrCtr);
        return (0u);
    }
#endif


                                                                        /* --------- VALIDATE LOOPBACK TX PKT --------- */
    p_buf_hdr_tx = &p_buf_tx->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIF_Loopback_TxPktValidate(p_buf_hdr_tx, p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_TYPE:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
             return (0u);
    }
#endif

                                                                        /* ----------- GET LOOPBACK RX BUF ------------ */
                                                                        /* Get rx buf.                                  */
    buf_data_len_tx = p_buf_hdr_tx->TotLen;
    buf_data_ix_rx  = NET_BUF_DATA_IX_RX;
    p_buf_rx         = NetBuf_Get((NET_IF_NBR     ) NET_IF_NBR_LOOPBACK,
                                 (NET_TRANSACTION) NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   ) buf_data_len_tx,
                                 (NET_BUF_SIZE   ) buf_data_ix_rx,
                                 (NET_BUF_SIZE  *)&buf_data_ix_rx_offset,
                                 (NET_BUF_FLAGS  ) NET_BUF_FLAG_NONE,
                                 (NET_ERR       *)&err);
    if (err != NET_BUF_ERR_NONE) {
        NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
        return (0u);
    }

                                                                        /* Get rx buf data area.                        */
    p_buf_rx->DataPtr = NetBuf_GetDataPtr((NET_IF        *) p_if,
                                         (NET_TRANSACTION) NET_TRANSACTION_RX,
                                         (NET_BUF_SIZE   ) buf_data_len_tx,
                                         (NET_BUF_SIZE   ) buf_data_ix_rx,
                                         (NET_BUF_SIZE  *)&buf_data_ix_rx_offset,
                                         (NET_BUF_SIZE  *)&buf_data_size_rx,
                                         (NET_BUF_TYPE  *) 0,
                                         (NET_ERR       *)&err);
    if (err != NET_BUF_ERR_NONE) {
        NetBuf_Free(p_buf_rx);                                          /* See Note #3a.                                */
        NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
        return (0u);
    }

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (buf_data_size_rx < buf_data_len_tx) {
        NetBuf_Free(p_buf_rx);                                          /* See Note #3b.                                */
        NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
        return (0u);
    }
#else
   (void)&buf_data_size_rx;                                             /* Prevent 'variable unused' compiler warning.  */
#endif

    buf_data_ix_rx += buf_data_ix_rx_offset;


                                                                        /* ------ COPY TX PKT TO LOOPBACK RX BUF ------ */
                                                                        /* Cfg rx loopback buf ctrls.                   */
    p_buf_hdr_rx                  = &p_buf_rx->Hdr;
    p_buf_hdr_rx->TotLen          = (NET_BUF_SIZE     )buf_data_len_tx;
    p_buf_hdr_rx->DataLen         = (NET_BUF_SIZE     )buf_data_len_tx;
    p_buf_hdr_rx->ProtocolHdrType = (NET_PROTOCOL_TYPE)p_buf_hdr_tx->ProtocolHdrType;
#if 0                                                                   /* Init'd in NetBuf_Get() [see Note #2].        */
    DEF_BIT_CLR(p_buf_hdr_rx->Flags, NET_BUF_FLAG_RX_REMOTE);
#endif

    switch (p_buf_hdr_tx->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_IP_V4:
             p_buf_hdr_rx->IP_HdrIx = (CPU_INT16U  )buf_data_ix_rx;
             buf_data_ix_tx        = (NET_BUF_SIZE)p_buf_hdr_tx->IP_HdrIx;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].TxInvProtocolCtr);
             NetBuf_Free(p_buf_rx);                                     /* See Note #3b.                                */
             NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
             return (0u);
    }

                                                                        /* Copy tx loopback pkt into rx loopback buf.   */
    NetBuf_DataCopy((NET_BUF    *) p_buf_rx,
                    (NET_BUF    *) p_buf_tx,
                    (NET_BUF_SIZE) buf_data_ix_rx,
                    (NET_BUF_SIZE) buf_data_ix_tx,
                    (NET_BUF_SIZE) buf_data_len_tx,
                    (NET_ERR    *)&err);
    if (err != NET_BUF_ERR_NONE) {
        NetBuf_Free(p_buf_rx);                                          /* See Note #3b.                                */
        NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
        return (0u);
    }


                                                                        /* ------- POST RX PKT TO LOOPBACK RX Q ------- */
    NetIF_Loopback_RxQ_Add(p_buf_rx);                                   /* Post pkt to loopback rx q.                   */
    NetStat_CtrInc(&NetIF_Loopback_RxQ_PktCtr, &err);                   /* Inc loopback IF's nbr q'd rx pkts avail.     */


                                                                        /* ------------ SIGNAL IF RX TASK ------------- */
    NetIF_RxTaskSignal(NET_IF_NBR_LOOPBACK, &err);
    if (err != NET_IF_ERR_NONE) {
        NetBuf_Free(p_buf_rx);                                          /* See Note #3c.                                */
        NetIF_Loopback_TxPktDiscard(p_buf_tx, p_err);
        return (0u);
    }


                                                                        /* ------ FREE TX PKT / UPDATE TX STATS ------- */
    NetIF_Loopback_TxPktFree(p_buf_tx);                                 /* See Note #4.                                 */

    NET_CTR_STAT_INC(Net_StatCtrs.IFs.Loopback.TxPktCtr);


                                                                        /* ------------ RTN TX'D DATA SIZE ------------ */
   *p_err =  NET_IF_ERR_NONE;

    return (buf_data_len_tx);



#else
   (void)&p_if;                                                         /* Prevent 'variable unused' compiler warnings. */
   (void)&p_buf_tx;

   *p_err =  NET_ERR_IF_LOOPBACK_DIS;

    return (0u);
#endif
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
*                                       NetIF_Loopback_RxQ_Add()
*
* Description : Add a network packet into the Network Loopback Interface Receive Queue.
*
*               (1) Network packets that have been received via the Network Loopback Interface are queued
*                   to await processing by the Network Interface Receive Task handler (see 'net_if.c
*                   NetIF_RxTaskHandler()  Note #1').
*
*                   (a) Received network packet buffers are linked to form a Network Loopback Interface
*                       Receive Queue.
*
*                       In the diagram below, ... :
*
*                       (1) The horizontal row represents the list of received network packet buffers.
*
*                       (2) (A) 'NetIF_Loopback_RxQ_Head' points to the head of the Network Loopback Interface
*                                   Receive Queue;
*                           (B) 'NetIF_Loopback_RxQ_Tail' points to the tail of the Network Loopback Interface
*                                   Receive Queue.
*
*                       (3) Network buffers' 'PrevSecListPtr' & 'NextSecListPtr' doubly-link each network
*                           packet  buffer to form the Network Loopback Interface Receive Queue.
*
*                   (b) The Network Loopback Interface Receive Queue is a FIFO Q :
*
*                       (1) Network packet buffers are added at     the tail of the Network Loopback Interface
*                               Receive Queue;
*                       (2) Network packet buffers are removed from the head of the Network Loopback Interface
*                               Receive Queue.
*
*
*                                      |                                               |
*                                      |<- Network Loopback Interface Receive Queue -->|
*                                      |                (see Note #1)                  |
*
*                                Packets removed from                        Packets added at
*                                 Receive Queue head                        Receive Queue tail
*                                  (see Note #1b2)                           (see Note #1b1)
*
*                                         |              NextSecListPtr             |
*                                         |             (see Note #1a3)             |
*                                         v                    |                    v
*                                                              |
*                       Head of        -------       -------   v   -------       -------  (see Note #1a2B)
*                       Receive   ---->|     |------>|     |------>|     |------>|     |
*                        Queue         |     |       |     |       |     |       |     |       Tail of
*                                      |     |<------|     |<------|     |<------|     |<----  Receive
*                  (see Note #1a2A)    |     |       |     |   ^   |     |       |     |        Queue
*                                      |     |       |     |   |   |     |       |     |
*                                      -------       -------   |   -------       -------
*                                                              |
*                                                        PrevSecListPtr
*                                                       (see Note #1a3)
*
*
* Argument(s) : p_buf       Pointer to a network buffer.
*               -----       Argument validated in NetIF_Loopback_Tx().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Tx().
*
* Note(s)     : (2) Some buffer controls were previously initialized in NetBuf_Get() when the buffer was
*                   allocated.  These buffer controls do NOT need to be re-initialized but are shown for
*                   completeness.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_RxQ_Add (NET_BUF  *p_buf)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_HDR  *p_buf_hdr_tail;

                                                                /* ----------------- CFG NET BUF PTRS ----------------- */
    p_buf_hdr                 = (NET_BUF_HDR *)&p_buf->Hdr;
    p_buf_hdr->PrevSecListPtr = (NET_BUF     *) NetIF_Loopback_RxQ_Tail;
    p_buf_hdr->NextSecListPtr = (NET_BUF     *) 0;
                                                                /* Cfg buf's unlink fnct/obj to loopback rx Q.          */
    p_buf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT )&NetIF_Loopback_RxQ_Unlink;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #2].                */
    p_buf_hdr->UnlinkObjPtr   = (void        *) 0;
#endif

                                                                /* ---------- ADD PKT BUF INTO LOOPBACK RX Q ---------- */
    if (NetIF_Loopback_RxQ_Tail != (NET_BUF *)0) {              /* If Q NOT empty, add after tail.                      */
        p_buf_hdr_tail                 = &NetIF_Loopback_RxQ_Tail->Hdr;
        p_buf_hdr_tail->NextSecListPtr =  p_buf;
    } else {                                                    /* Else add first pkt buf into Q.                       */
        NetIF_Loopback_RxQ_Head       =  p_buf;
    }
    NetIF_Loopback_RxQ_Tail = p_buf;                            /* Add pkt buf @ Q tail (see Note #1b1).                */
}


/*
*********************************************************************************************************
*                                       NetIF_Loopback_RxQ_Get()
*
* Description : Get a network packet buffer from the Network Loopback Interface Receive Queue.
*
* Argument(s) : none.
*
* Return(s)   : Pointer to received network packet buffer, if available.
*
*               Pointer to NULL,                           otherwise.
*
* Caller(s)   : NetIF_Loopback_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_BUF  *NetIF_Loopback_RxQ_Get (void)
{
    NET_BUF  *p_buf;

                                                                /* ---------- GET PKT BUF FROM LOOPBACK RX Q ---------- */
    p_buf= NetIF_Loopback_RxQ_Head;
    if (p_buf== (NET_BUF *)0) {
        return ((NET_BUF *)0);
    }
                                                                /* -------- REMOVE PKT BUF FROM LOOPBACK RX Q --------- */
    NetIF_Loopback_RxQ_Remove(p_buf);

    return (p_buf);
}


/*
*********************************************************************************************************
*                                      NetIF_Loopback_RxQ_Remove()
*
* Description : Remove a network packet buffer from the Network Loopback Interface Receive Queue.
*
* Argument(s) : p_buf       Pointer to a network buffer.
*               -----       Argument checked in NetIF_Loopback_RxQ_Get(),
*                                               NetIF_Loopback_RxQ_Unlink().
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_RxQ_Get(),
*               NetIF_Loopback_RxQ_Unlink().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_RxQ_Remove (NET_BUF  *p_buf)
{
    NET_BUF      *p_buf_list_prev;
    NET_BUF      *p_buf_list_next;
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_HDR  *p_buf_list_prev_hdr;
    NET_BUF_HDR  *p_buf_list_next_hdr;

                                                                /* -------- REMOVE PKT BUF FROM LOOPBACK RX Q --------- */
    p_buf_hdr       = &p_buf->Hdr;
    p_buf_list_prev =  p_buf_hdr->PrevSecListPtr;
    p_buf_list_next =  p_buf_hdr->NextSecListPtr;

                                                                /* Point prev pkt buf to next pkt buf.                  */
    if (p_buf_list_prev != (NET_BUF *)0) {
        p_buf_list_prev_hdr                 = &p_buf_list_prev->Hdr;
        p_buf_list_prev_hdr->NextSecListPtr =  p_buf_list_next;
    } else {
        NetIF_Loopback_RxQ_Head            =  p_buf_list_next;
    }
                                                                /* Point next pkt buf to prev pkt buf.                  */
    if (p_buf_list_next != (NET_BUF *)0) {
        p_buf_list_next_hdr                 = &p_buf_list_next->Hdr;
        p_buf_list_next_hdr->PrevSecListPtr =  p_buf_list_prev;
    } else {
        NetIF_Loopback_RxQ_Tail            =  p_buf_list_prev;
    }

                                                                /* ----------------- CLR NET BUF PTRS ----------------- */
    p_buf_hdr->PrevSecListPtr = (NET_BUF    *)0;                /* Clr buf sec list ptrs.                               */
    p_buf_hdr->NextSecListPtr = (NET_BUF    *)0;

    p_buf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT)0;                /* Clr unlink ptrs.                                     */
    p_buf_hdr->UnlinkObjPtr   = (void       *)0;
}


/*
*********************************************************************************************************
*                                      NetIF_Loopback_RxQ_Unlink()
*
* Description : Unlink a network packet buffer from the Network Loopback Interface Receive Queue.
*
* Argument(s) : p_buf       Pointer to network buffer enqueued on Network Loopback Interface Receive Queue.
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetIF_Loopback_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_RxQ_Unlink (NET_BUF  *p_buf)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN  used;
#endif


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf== (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Loopback.NullPtrCtr);
        return;
    }
                                                                /* ------------------- VALIDATE BUF ------------------- */
    used = NetBuf_IsUsed(p_buf);
    if (used != DEF_YES) {
        return;
    }
#endif

                                                                /* ---------- UNLINK BUF FROM LOOPBACK RX Q ----------- */
    NetIF_Loopback_RxQ_Remove(p_buf);
}


/*
*********************************************************************************************************
*                                   NetIF_Loopback_RxPktValidateBuf()
*
* Description : Validate received buffer header as a valid loopback packet.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received a packet.
*               ---------   Argument validated in NetIF_Loopback_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Received loopback buffer packet validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT a valid
*                                                                   loopback/network protocol.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer type.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer  index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIF_Loopback_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
                                               NET_ERR      *p_err)
{
    CPU_INT16U  ix;

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

                                                                /* ----------------- VALIDATE PROTOCOL ---------------- */
    switch (p_buf_hdr->ProtocolHdrType) {
        case NET_PROTOCOL_TYPE_IP_V4:
             ix = p_buf_hdr->IP_HdrIx;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].RxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].RxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_IF_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetIF_Loopback_RxPktDemux()
*
* Description : Demultiplex received loopback packet to appropriate network protocol.
*
* Argument(s) : p_buf      Pointer to network buffer that received loopback packet.
*               -----      Argument validated in NetIF_Loopback_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Loopback packet successfully demultiplexed
*                                                                   & processed.
*                               NET_ERR_INVALID_PROTOCOL        Invalid network protocol.
*
*                                                               -------- RETURNED BY NetIP_Rx() : --------
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_RxPktDemux (NET_BUF  *p_buf,
                                         NET_ERR  *p_err)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_ERR       err;


    p_buf_hdr = &p_buf->Hdr;
    switch (p_buf_hdr->ProtocolHdrType) {                       /* Demux buf to appropriate protocol.                   */
        case NET_PROTOCOL_TYPE_IP_V4:
#ifdef  NET_IPv4_MODULE_EN
             NetIPv4_Rx(p_buf, &err);
            *p_err = (err == NET_IPv4_ERR_NONE) ? NET_IF_ERR_NONE : err;
#else
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].RxInvProtocolCtr);
            *p_err = NET_IF_ERR_LOOPBACK_DEMUX_PROTOCOL;
#endif

             break;


        case NET_PROTOCOL_TYPE_IP_V6:
#ifdef  NET_IPv6_MODULE_EN
             NetIPv6_Rx(p_buf, &err);
            *p_err = (err == NET_IPv6_ERR_NONE) ? NET_IF_ERR_NONE : err;
#else
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].RxInvProtocolCtr);
            *p_err = NET_IF_ERR_LOOPBACK_DEMUX_PROTOCOL;
#endif
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].RxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             break;
    }
}

/*
*********************************************************************************************************
*                                     NetIF_Loopback_RxPktDiscard()
*
* Description : On any loopback receive error(s), discard loopback packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_RxPktDiscard (NET_BUF  *p_buf,
                                           NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.IFs.Loopback.RxPktDisCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)pctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                    NetIF_Loopback_TxPktValidate()
*
* Description : (1) Validate network loopback interface transmit packet parameters :
*
*                   (a) Validate the following transmit packet parameters :
*
*                       (1) Supported protocols :
*                           (A) IPv4
*
*                       (2) Buffer protocol index
*
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIF_Loopback_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Transmit packet validated.
*                               NET_ERR_INVALID_PROTOCOL        Invalid/unknown protocol type.
*                               NET_BUF_ERR_INVALID_TYPE        Invalid network buffer   type.
*                               NET_BUF_ERR_INVALID_IX          Invalid/insufficient buffer index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIF_Loopback_TxPktValidate (NET_BUF_HDR  *p_buf_hdr,
                                            NET_ERR      *p_err)
{
    CPU_INT16U  ix;

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
        case NET_PROTOCOL_TYPE_IP_V4:
             ix = p_buf_hdr->IP_HdrIx;
             break;


        case NET_PROTOCOL_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].TxInvProtocolCtr);
            *p_err = NET_ERR_INVALID_PROTOCOL;
             return;
    }

    if (ix == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[NET_IF_NBR_LOOPBACK].TxInvBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }


   *p_err = NET_IF_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                      NetIF_Loopback_TxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_TxPktFree (NET_BUF  *p_buf)
{
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)0);
}


/*
*********************************************************************************************************
*                                     NetIF_Loopback_TxPktDiscard()
*
* Description : On any loopback transmit packet error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Loopback_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_TxPktDiscard (NET_BUF  *p_buf,
                                           NET_ERR  *p_err)
{
    NET_CTR  *pctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    pctr = (NET_CTR *)&Net_ErrCtrs.IFs.Loopback.TxPktDisCtr;
#else
    pctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)pctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                        NetIF_Loopback_IF_Add()
*
* Description : (1) Add & initialize the Network Loopback Interface :
*
*                   (a) Initialize Loopback MTU
*                   (b) Configure  Loopback interface
*
*
* Argument(s) : p_if        Pointer to network loopback interface.
*               ----        Argument validated in NetIF_Add().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network loopback interface successfully added.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Add() via 'p_if_api->Add()'.
*
* Note(s)     : (2) Upon adding the loopback interface, the highest possible MTU is configured.  If this
*                   value needs to be changed, either prior to starting the interface or during run-time,
*                   it may be reconfigured by calling NetIF_MTU_Set() from the application.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_IF_Add (NET_IF   *p_if,
                                     NET_ERR  *p_err)
{
    NET_IF_CFG_LOOPBACK  *pcfg_loopback;
    NET_BUF_SIZE          buf_size_max;
    NET_MTU               mtu_loopback;
    NET_MTU               mtu_loopback_dflt;


    pcfg_loopback = (NET_IF_CFG_LOOPBACK *)p_if->Dev_Cfg;

                                                                /* ------------------ CFG LOOPBACK IF ----------------- */
    p_if->Type = NET_IF_TYPE_LOOPBACK;                          /* Set IF type to loopback.                             */


    NetIF_BufPoolInit(p_if, p_err);                             /* Init IF's buf pools.                                 */
    if (*p_err != NET_IF_ERR_NONE) {                            /* On any err(s);                                       */
         goto exit;
    }

                                                                /* --------------------- INIT MTU --------------------- */
    buf_size_max      = DEF_MAX(pcfg_loopback->TxBufLargeSize, pcfg_loopback->TxBufSmallSize);
    mtu_loopback_dflt = NET_IF_MTU_LOOPBACK;
    mtu_loopback      = DEF_MIN(mtu_loopback_dflt, buf_size_max);
    p_if->MTU         = mtu_loopback;                           /* Set loopback MTU (see Note #2).                      */




   *p_err = NET_IF_ERR_NONE;

exit:
   return;
}


/*
*********************************************************************************************************
*                                       NetIF_Loopback_IF_Start()
*
* Description : Start Network Loopback Interface.
*
* Argument(s) : p_if        Pointer to network loopback interface.
*               ----        Argument validated in NetIF_Start().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network loopback interface successfully
*                                                                   started.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Start() via 'p_if_api->Start()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_IF_Start (NET_IF   *p_if,
                                       NET_ERR  *p_err)
{
    p_if->Link = NET_IF_LINK_UP;
   *p_err      = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetIF_Loopback_IF_Stop()
*
* Description : Stop Network Loopback Interface.
*
* Argument(s) : p_if        Pointer to network loopback interface.
*               ----        Argument validated in NetIF_Stop().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Network loopback interface successfully
*                                                                   stopped.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Stop() via 'p_if_api->Stop()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_IF_Stop (NET_IF   *p_if,
                                      NET_ERR  *p_err)
{
    p_if->Link = NET_IF_LINK_DOWN;
   *p_err      = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetIF_Loopback_AddrHW_Get()
*
* Description : Get the loopback interface's hardware address.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_AddrHW_GetHandler().
*
*               p_addr_hw   Pointer to variable that will receive the hardware address.
*               ---------   Argument checked   in NetIF_AddrHW_GetHandler().
*
*               p_addr_len  Pointer to a variable to ... :
*               ----------
*                               (a) Pass the length of the address buffer pointed to by 'p_addr_hw'.
*                               (b) (1) Return the actual size of the protocol address, if NO error(s);
*                                   (2) Return 0,                                       otherwise.
*
*                           Argument checked in NetIF_AddrHW_GetHandler().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_INVALID_ADDR         Loopback interface's hardware address NULL
*                                                                   (see Note #1).
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrHW_GetHandler() via 'p_if_api->AddrHW_Get()'.
*
* Note(s)     : (1) The loopback interface is NOT linked to, associated with, or handled by any physical
*                   network device(s) & therefore has NO physical hardware address.
*
*                   See also 'net_if_loopback.c  Note #1a'.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_AddrHW_Get (NET_IF      *p_if,
                                         CPU_INT08U  *p_addr_hw,
                                         CPU_INT08U  *p_addr_len,
                                         NET_ERR     *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_hw;

   *p_addr_len = 0u;
   *p_err     = NET_IF_ERR_LOOPBACK_INVALID_ADDR;               /* See Note #1.                                         */
}


/*
*********************************************************************************************************
*                                      NetIF_Loopback_AddrHW_Set()
*
* Description : Set the loopback interface's hardware address.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_AddrHW_SetHandler().
*
*               p_addr_hw   Pointer to a memory that contains the hardware address (see Note #1).
*               ---------   Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Hardware address length.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address (see Note #1).
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrHW_SetHandler() via 'p_if_api->AddrHW_Set()'.
*
* Note(s)     : (1) The loopback interface is NOT linked to, associated with, or handled by any physical
*                   network device(s) & therefore has NO physical hardware address.
*
*                   See also 'net_if_loopback.c  Note #1a'.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_AddrHW_Set (NET_IF      *p_if,
                                         CPU_INT08U  *p_addr_hw,
                                         CPU_INT08U   addr_len,
                                         NET_ERR     *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_hw;
   (void)&addr_len;

   *p_err = NET_IF_ERR_LOOPBACK_INVALID_ADDR;                    /* See Note #1.                                         */
}


/*
*********************************************************************************************************
*                                    NetIF_Loopback_AddrHW_IsValid()
*
* Description : Validate a loopback interface hardware address.
*
* Argument(s) : p_if        Pointer to   loopback interface.
*               ----        Argument validated in NetIF_AddrHW_IsValidHandler().
*
*               p_addr_hw   Pointer to a loopback interface hardware address (see Note #1).
*               ---------   Argument checked   in NetIF_AddrHW_IsValidHandler().
*
* Return(s)   : DEF_NO, loopback hardware address NOT valid (see Note #1).
*
* Caller(s)   : NetIF_AddrHW_IsValidHandler() via 'p_if_api->AddrHW_IsValid()'.
*
* Note(s)     : (1) The loopback interface is NOT linked to, associated with, or handled by any physical
*                   network device(s) & therefore has NO physical hardware address.
*
*                   See also 'net_if_loopback.c  Note #1a'.
*********************************************************************************************************
*/

static  CPU_BOOLEAN  NetIF_Loopback_AddrHW_IsValid (NET_IF      *p_if,
                                                    CPU_INT08U  *p_addr_hw)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_hw;

    return (DEF_NO);                                            /* See Note #1.                                         */
}


/*
*********************************************************************************************************
*                                   NetIF_Loopback_AddrMulticastAdd()
*
* Description : Add a multicast address to the loopback interface.
*
* Argument(s) : p_if                    Pointer to network loopback interface to add address.
*               ----                    Argument validated in NetIF_AddrMulticastAdd().
*
*               p_addr_protocol         Pointer to a multicast protocol address to add (see Note #1).
*               ---------------         Argument checked   in NetIF_AddrMulticastAdd().
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function (see Note #1).
*
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrMulticastAdd() via 'p_if_api->AddrMulticastAdd()'.
*
* Note(s)     : (1) Multicast addresses are available ONLY for configured interface(s); NOT for any
*                   loopback interface(s).
*
*               (2) The multicast protocol address MUST be in network-order.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_AddrMulticastAdd (NET_IF             *p_if,
                                               CPU_INT08U         *p_addr_protocol,
                                               CPU_INT08U          addr_protocol_len,
                                               NET_PROTOCOL_TYPE   addr_protocol_type,
                                               NET_ERR            *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_protocol;
   (void)&addr_protocol_len;
   (void)&addr_protocol_type;

   *p_err = NET_ERR_FAULT_FEATURE_DIS;
}


/*
*********************************************************************************************************
*                                 NetIF_Loopback_AddrMulticastRemove()
*
* Description : Remove a multicast address from the loopback interface.
*
* Argument(s) : p_if                    Pointer to network loopback interface to remove address.
*               ----                    Argument validated in NetIF_AddrMulticastRemove().
*
*               p_addr_protocol         Pointer to a multicast protocol address to remove (see Note #1).
*               ---------------         Argument checked   in NetIF_AddrMulticastRemove().
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function (see Note #1).
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrMulticastRemove() via 'p_if_api->AddrMulticastRemove()'.
*
* Note(s)     : (1) Multicast addresses are available ONLY for configured interface(s); NOT for any
*                   loopback interface(s).
*
*               (2) The multicast protocol address MUST be in network-order.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_AddrMulticastRemove (NET_IF             *p_if,
                                                  CPU_INT08U         *p_addr_protocol,
                                                  CPU_INT08U          addr_protocol_len,
                                                  NET_PROTOCOL_TYPE   addr_protocol_type,
                                                  NET_ERR            *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_protocol;
   (void)&addr_protocol_len;
   (void)&addr_protocol_type;

   *p_err = NET_ERR_FAULT_FEATURE_DIS;
}


/*
*********************************************************************************************************
*                              NetIF_Loopback_AddrMulticastProtocolToHW()
*
* Description : Convert a multicast protocol address into a loopback interface address.
*
*
* Argument(s) : p_if                    Pointer to network loopback interface to transmit the packet.
*               ----                    Argument validated in NetIF_AddrMulticastProtocolToHW().
*
*               p_addr_protocol         Pointer to a multicast protocol address to convert,
*               ---------------             in network-order.
*
*                                       Argument checked   in NetIF_AddrMulticastProtocolToHW().
*
*               addr_protocol_len       Length of the protocol address, in octets.
*
*               addr_protocol_type      Protocol address type.
*
*               p_addr_hw               Pointer to a variable that will receive the hardware address
*               ---------                   in network order.
*
*                                       Argument checked   in NetIF_AddrMulticastProtocolToHW().
*
*               p_addr_hw_len           Pointer to a variable to ... :
*               -------------
*                                           (a) Pass the length of the              hardware address
*                                                   argument, in octets.
*                                           (b) (1) Return the actual length of the hardware address,
*                                                       in octets, if NO error(s);
*                                               (2) Return 0, otherwise.
*
*                                       Argument checked   in NetIF_AddrMulticastProtocolToHW().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function (see Note #1).
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_AddrMulticastProtocolToHW() via 'p_if_api->AddrMulticastProtocolToHW'.
*
* Note(s)     : (1) Multicast addresses are available ONLY for configured interface(s); NOT for any
*                   loopback interface(s).
*
*               (2) (a) The multicast protocol address MUST be in network-order.
*
*                   (b) The loopback hardware address is returned in network-order; i.e. the pointer to
*                       the hardware address points to the highest-order octet.
*
*               (3) Since 'p_addr_hw_len' argument is both an input & output argument (see 'Argument(s) :
*                   p_addr_hw_len'), ... :
*
*                   (a) Its input value SHOULD be validated prior to use; ...
*                   (b) While its output value MUST be initially configured to return a default value
*                       PRIOR to all other validation or function handling in case of any error(s).
*********************************************************************************************************
*/

static  void  NetIF_Loopback_AddrMulticastProtocolToHW (NET_IF             *p_if,
                                                        CPU_INT08U         *p_addr_protocol,
                                                        CPU_INT08U          addr_protocol_len,
                                                        NET_PROTOCOL_TYPE   addr_protocol_type,
                                                        CPU_INT08U         *p_addr_hw,
                                                        CPU_INT08U         *p_addr_hw_len,
                                                        NET_ERR            *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&p_addr_protocol;
   (void)&addr_protocol_len;
   (void)&addr_protocol_type;
   (void)&p_addr_hw;

   *p_addr_hw_len = 0u;                                          /* Cfg dflt addr len for err (see Note #3b).            */
   *p_err         = NET_ERR_FAULT_FEATURE_DIS;
}

/*
*********************************************************************************************************
*                                  NetIF_Loopback_BufPoolCfgValidate()
*
* Description : Validate loopback interface network buffer pool configuration.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_BufPoolInit().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Loopback interface's buffer pool
*                                                                   configuration valid.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_BufPoolInit() via 'p_if_api->BufPoolCfgValidate()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_BufPoolCfgValidate (NET_IF   *p_if,
                                                 NET_ERR  *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warning.          */

   *p_err = NET_IF_ERR_NONE;
}

/*
*********************************************************************************************************
*                                      NetIF_Loopback_MTU_Set()
*
* Description : Set the loopback interface's MTU.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_MTU_SetHandler().
*
*               mtu         Desired maximum transmission unit (MTU) size to configure (in octets).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Loopback interface's MTU successfully set.
*                               NET_IF_ERR_INVALID_MTU          Invalid MTU.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_MTU_SetHandler() via 'p_if_api->MTU_Set()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_MTU_Set (NET_IF   *p_if,
                                      NET_MTU   mtu,
                                      NET_ERR  *p_err)
{
    NET_IF_CFG_LOOPBACK  *pcfg_loopback;
    NET_BUF_SIZE          buf_size_max;
    NET_MTU               mtu_max;


    pcfg_loopback = (NET_IF_CFG_LOOPBACK *)p_if->Dev_Cfg;
    buf_size_max  =  DEF_MAX(pcfg_loopback->TxBufLargeSize, pcfg_loopback->TxBufSmallSize);
    mtu_max       =  DEF_MIN(mtu, buf_size_max);

    if (mtu <= mtu_max) {
        p_if->MTU  =  mtu;
       *p_err     =  NET_IF_ERR_NONE;
    } else {
       *p_err     =  NET_IF_ERR_LOOPBACK_INVALID_MTU;
    }
}


/*
*********************************************************************************************************
*                                   NetIF_Loopback_GetPktSizeHdr()
*
* Description : Get loopback interface header size.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_MTU_GetProtocol().
*
* Return(s)   : The loopback interface header size.
*
* Caller(s)   : NetIF_MTU_GetProtocol() via 'p_if_api->GetPktSizeHdr()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  NetIF_Loopback_GetPktSizeHdr (NET_IF  *p_if)
{
    CPU_INT16U  pkt_size;


   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warning.          */
    pkt_size = (CPU_INT16U)NET_IF_HDR_SIZE_LOOPBACK;

    return (pkt_size);
}


/*
*********************************************************************************************************
*                                   NetIF_Loopback_GetPktSizeMin()
*
* Description : Get minimum allowable loopback interface packet size.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_GetPktSizeMin().
*
* Return(s)   : The minimum allowable loopback interface packet size.
*
* Caller(s)   : NetIF_GetPktSizeMin() via 'p_if_api->GetPktSizeMin()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  NetIF_Loopback_GetPktSizeMin (NET_IF  *p_if)
{
    CPU_INT16U  pkt_size;


   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warning.          */
    pkt_size = (CPU_INT16U)NET_IF_LOOPBACK_SIZE_MIN;

    return (pkt_size);
}


/*
*********************************************************************************************************
*                                   NetIF_Loopback_GetPktSizeMin()
*
* Description : Get maximum allowable loopback interface packet size.
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_GetPktSizeMin().
*
* Return(s)   : The maximum allowable loopback interface packet size.
*
* Caller(s)   : NetIF_GetPktSizeMax() via 'p_if_api->GetPktSizeMin()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  NetIF_Loopback_GetPktSizeMax (NET_IF  *p_if)
{
    CPU_INT16U    pkt_size;
    NET_DEV_CFG  *p_cfg = (NET_DEV_CFG *)p_if->Dev_Cfg;


   (void)&p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    pkt_size = (CPU_INT16U)p_cfg->RxBufLargeSize;

    return (pkt_size);
}


/*
*********************************************************************************************************
*                                    NetIF_Loopback_ISR_Handler()
*
* Description : Handle loopback interface interrupt service routine (ISR) function(s).
*
*               (1) Interrupt service routines are available ONLY for configured interface(s)/device(s);
*                   NOT for any loopback interface(s).
*
*
* Argument(s) : p_if        Pointer to network loopback interface.
*               ----        Argument validated in NetIF_ISR_Handler().
*
*               type        Device interrupt type(s) to handle.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_FAULT_FEATURE_DIS              Disabled API function (see Note #1).
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_ISR_Handler() via 'p_if_api->ISR_Handler()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_ISR_Handler (NET_IF            *p_if,
                                          NET_DEV_ISR_TYPE   type,
                                          NET_ERR           *p_err)
{
   (void)&p_if;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&type;

   *p_err = NET_ERR_FAULT_FEATURE_DIS;
}


/*
*********************************************************************************************************
*                                    NetIF_Loopback_IO_CtrlHandler()
*
* Description : Handle loopback interface specific control(s).
*
* Argument(s) : p_if        Pointer to loopback interface.
*               ----        Argument validated in NetIF_IO_CtrlHandler().
*
*               opt         Desired I/O control option code to perform :
*
*                               NET_IF_IO_CTRL_LINK_STATE_GET           Get loopback interface's link state,
*                                                                           'UP' or 'DOWN'.
*
*               p_data      Pointer to variable that will receive possible I/O control data (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Loopback interface I/O control option
*                                                                       successfully handled.
*                               NET_ERR_FAULT_NULL_PTR                 Argument(s) passed a NULL pointer.
*                               NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid I/O control option.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_IO_CtrlHandler() via 'p_if_api->IO_Ctrl()'.
*
* Note(s)     : (1) 'p_data' MUST point to a variable (or memory) that is sufficiently sized AND aligned
*                    to receive any return data.
*********************************************************************************************************
*/

static  void  NetIF_Loopback_IO_CtrlHandler (NET_IF      *p_if,
                                             CPU_INT08U   opt,
                                             void        *p_data,
                                             NET_ERR     *p_err)
{
    CPU_BOOLEAN  *p_link_state;


    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET:
             if (p_data == (void *)0) {
                *p_err = NET_ERR_FAULT_NULL_PTR;
                 return;
             }

             p_link_state = (CPU_BOOLEAN *)p_data;              /* See Note #1.                                         */
            *p_link_state = (CPU_BOOLEAN  )p_if->Link;
             break;


        case NET_IF_IO_CTRL_NONE:
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
        default:
            *p_err = NET_IF_ERR_INVALID_IO_CTRL_OPT;
             return;
    }

   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_IF_LOOPBACK_MODULE_EN    */

