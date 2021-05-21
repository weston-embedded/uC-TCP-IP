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
*                                       NETWORK INTERFACE LAYER
*
*                                              ETHERNET
*
* Filename : net_if_ether.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports Ethernet as described in RFC #894; supports IEEE 802 as described in RFC #1042.
*
*            (2) Ethernet implementation conforms to RFC #1122, Section 2.3.3, bullets (a) & (b), but
*                does NOT implement bullet (c) :
*
*                RFC #1122                  LINK LAYER                  October 1989
*
*                2.3.3  ETHERNET (RFC-894) and IEEE 802 (RFC-1042) ENCAPSULATION
*
*                       Every Internet host connected to a 10Mbps Ethernet cable :
*
*                       (a) MUST be able to send and receive packets using RFC-894 encapsulation;
*
*                       (b) SHOULD be able to receive RFC-1042 packets, intermixed with RFC-894 packets; and
*
*                       (c) MAY be able to send packets using RFC-1042 encapsulation.
*
*            (3) REQUIREs the following network protocol files in network directories :
*
*                (a) Network Interface Layer           located in the following network directory :
*
*                        \<Network Protocol Suite>\IF\
*
*                (b) Address Resolution Protocol Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\
*
*                    See also 'net_arp.h  Note #1'.
*
*                         where
*                                 <Network Protocol Suite>    directory path for network protocol suite
*
*                (c) IEEE 802 Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\IF\
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_IF_MODULE_ETHER


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_if_ether.h"
#include  "net_if.h"
#include  "../Source/net_type.h"
#include  "../Source/net_cfg_net.h"
#include  "../Source/net_err.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IF_ETHER_MODULE_EN


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
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                            /* ----------- RX FNCT ----------- */
static  void  NetIF_Ether_Rx            (NET_IF      *p_if,
                                         NET_BUF     *p_buf,
                                         NET_ERR     *p_err);

static  void  NetIF_Ether_RxPktDiscard  (NET_BUF     *p_buf,
                                         NET_ERR     *p_err);


                                                            /* ----------- TX FNCT ----------- */
static  void  NetIF_Ether_Tx            (NET_IF      *p_if,
                                         NET_BUF     *p_buf,
                                         NET_ERR     *p_err);

static  void  NetIF_Ether_TxPktDiscard  (NET_BUF     *p_buf,
                                         NET_ERR     *p_err);


                                                               /* -------- API FNCTS --------- */
static  void  NetIF_Ether_IF_Add        (NET_IF      *p_if,
                                         NET_ERR     *p_err);

static  void  NetIF_Ether_IF_Start      (NET_IF      *p_if,
                                         NET_ERR     *p_err);

static  void  NetIF_Ether_IF_Stop       (NET_IF      *p_if,
                                         NET_ERR     *p_err);


                                                               /* -------- MGMT FNCTS -------- */
static  void  NetIF_Ether_IO_CtrlHandler(NET_IF      *p_if,
                                         CPU_INT08U   opt,
                                         void        *p_data,
                                         NET_ERR     *p_err);


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

const  NET_IF_API  NetIF_API_Ether = {                                          /* Ether IF API fnct ptrs :             */
                                       &NetIF_Ether_IF_Add,                     /*   Init/add                           */
                                       &NetIF_Ether_IF_Start,                   /*   Start                              */
                                       &NetIF_Ether_IF_Stop,                    /*   Stop                               */
                                       &NetIF_Ether_Rx,                         /*   Rx                                 */
                                       &NetIF_Ether_Tx,                         /*   Tx                                 */
                                       &NetIF_802x_AddrHW_Get,                  /*   Hw        addr get                 */
                                       &NetIF_802x_AddrHW_Set,                  /*   Hw        addr set                 */
                                       &NetIF_802x_AddrHW_IsValid,              /*   Hw        addr valid               */
                                       &NetIF_802x_AddrMulticastAdd,            /*   Multicast addr add                 */
                                       &NetIF_802x_AddrMulticastRemove,         /*   Multicast addr remove              */
                                       &NetIF_802x_AddrMulticastProtocolToHW,   /*   Multicast addr protocol-to-hw      */
                                       &NetIF_802x_BufPoolCfgValidate,          /*   Buf cfg validation                 */
                                       &NetIF_802x_MTU_Set,                     /*   MTU set                            */
                                       &NetIF_802x_GetPktSizeHdr,               /*   Get pkt hdr size                   */
                                       &NetIF_802x_GetPktSizeMin,               /*   Get pkt min size                   */
                                       &NetIF_802x_GetPktSizeMax,
                                       &NetIF_802x_ISR_Handler,                 /*   ISR handler                        */
                                       &NetIF_Ether_IO_CtrlHandler              /*   I/O ctrl                           */
                                     };

/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          NetIF_Ether_Init()
*
* Description : (1) Initialize Ethernet Network Interface Module :
*
*                   Module initialization NOT yet required/implemented
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Ethernet network interface module
*                                                                       successfully initialized.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIF_Ether_Init (NET_ERR  *p_err)
{
   *p_err = NET_IF_ERR_NONE;
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
*                                           NetIF_Ether_Rx()
*
* Description : Process received data packets & forward to network protocol layers.
*
*
* Argument(s) : p_if        Pointer to an Ethernet network interface to transmit data packet(s).
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_buf       Pointer to a network buffer that received a packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Ethernet packet successfully received &
*                                                                   processed.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*
*                                                               - RETURNED BY NetIF_Ether_RxPktDiscard() : -
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pif_api->Rx()'.
*
* Note(s)     : (1) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIF_Ether_Rx (NET_IF   *p_if,
                              NET_BUF  *p_buf,
                              NET_ERR  *p_err)
{
    NET_CTR_IF_802x_STATS  *p_ctrs_stat;
    NET_CTR_IF_802x_ERRS   *p_ctrs_err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Ether.NullPtrCtr);
        NetIF_Ether_RxPktDiscard(p_buf, p_err);
       *p_err = NET_ERR_RX;
        return;
    }
#endif

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_ctrs_stat = &Net_StatCtrs.IFs.Ether.IF_802xCtrs;
#else
    p_ctrs_stat = (NET_CTR_IF_802x_STATS *)0;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    p_ctrs_err  = &Net_ErrCtrs.IFs.Ether.IF_802xCtrs;
#else
    p_ctrs_err  = (NET_CTR_IF_802x_ERRS *)0;
#endif


                                                                /* ------------------- RX ETHER PKT ------------------- */
    NetIF_802x_Rx(p_if,
                  p_buf,
                  p_ctrs_stat,
                  p_ctrs_err,
                  p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.Ether.RxPktCtr);
             break;


        case NET_ERR_RX:
                                                                /* See Note #1.                                             */
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Ether.RxPktDisCtr);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        default:
             NetIF_Ether_RxPktDiscard(p_buf, p_err);
             break;
    }
}


/*
*********************************************************************************************************
*                                      NetIF_Ether_RxPktDiscard()
*
* Description : On any receive error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*               -----       Argument checked in NetIF_WiFi_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Ether_RxPktDiscard (NET_BUF  *p_buf,
                                        NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IFs.Ether.RxPktDisCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf(p_buf, p_ctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                           NetIF_Ether_Tx()
*
* Description : Prepare data packets from network protocol layers for Ethernet transmit.
*
* Argument(s) : p_if        Pointer to a network interface to transmit data packet(s).
*               ----        Argument validated in NetIF_TxHandler().
*
*               p_buf       Pointer to network buffer with data packet to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Ethernet packet successfully prepared
*                                                                   for transmission.
*                               NET_IF_ERR_TX_ADDR_PEND         Ethernet packet successfully prepared
*                                                                   & queued for later transmission.
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pif_api->Tx()'.
*
* Note(s)     : (1) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIF_Ether_Tx (NET_IF   *p_if,
                              NET_BUF  *p_buf,
                              NET_ERR  *p_err)
{
    NET_CTR_IF_802x_STATS  *p_ctrs_stat;
    NET_CTR_IF_802x_ERRS   *p_ctrs_err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIF_Ether_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Ether.NullPtrCtr);
       *p_err = NET_ERR_TX;
        return;
    }
#endif

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_ctrs_stat = &Net_StatCtrs.IFs.Ether.IF_802xCtrs;
#else
    p_ctrs_stat = (NET_CTR_IF_802x_STATS *)0;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    p_ctrs_err  = &Net_ErrCtrs.IFs.Ether.IF_802xCtrs;
#else
    p_ctrs_err  = (NET_CTR_IF_802x_ERRS *)0;
#endif


                                                                /* --------------- PREPARE ETHER TX PKT --------------- */
    NetIF_802x_Tx(p_if,
                  p_buf,
                  p_ctrs_stat,
                  p_ctrs_err,
                  p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.Ether.TxPktCtr);
             break;


        case NET_IF_ERR_TX_ADDR_PEND:
             break;


        case NET_ERR_TX:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.Ether.TxPktDisCtr);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        default:
             NetIF_Ether_TxPktDiscard(p_buf, p_err);
             break;
    }
}


/*
*********************************************************************************************************
*                                      NetIF_Ether_TxPktDiscard()
*
* Description : On any Ethernet transmit error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*               -----       Argument checked in NetIF_Ether_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_Ether_TxPktDiscard (NET_BUF  *p_buf,
                                        NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IFs.Ether.TxPktDisCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif

   (void)NetBuf_FreeBuf(p_buf, p_ctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                         NetIF_Ether_IF_Add()
*
* Description : (1) Add & initialize an Ethernet network interface :
*
*                   (a) Validate   Ethernet device configuration
*                   (b) Initialize Ethernet device data area
*                   (c) Perform    Ethernet/OS initialization
*                   (d) Initialize Ethernet device hardware MAC address
*                   (e) Initialize Ethernet device hardware
*                   (f) Initialize Ethernet device MTU
*                   (g) Configure  Ethernet interface
*
*
* Argument(s) : p_if        Pointer to Ethernet network interface to add.
*               ----        Argument validated in NetIF_Add().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Ethernet interface successfully added.
*                               NET_ERR_FAULT_MEM_ALLOC             Insufficient resources available to add
*                                                                       Ethernet interface.
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL network interface API configuration.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL network interface function pointer.
*
*                               NET_DEV_ERR_INVALID_CFG             Invalid/NULL network device    API configuration.
*                               NET_ERR_FAULT_NULL_FNCT               Invalid NULL network device    function pointer.
*
*                                                                   -------- RETURNED BY NetDev_Init() : ---------
*                                                                   See NetDev_Init() for addtional return error codes.
*
*                                                                   ------- RETURNED BY 'pdev_api->Init()' : --------
*                                                                   See specific network device(s) 'Init()' for
*                                                                       additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Add() via 'pif_api->Add()'.
*
* Note(s)     : (2) This function sets the interface MAC address to all 0's.  This ensures that the
*                   device driver can compare the MAC for all 0 in order to check if the MAC has
*                   been configured before.
*
*               (3) The return error is not checked because there isn't anything that can be done from
*                   software in order to recover from a device hardware initializtion error.  The cause
*                   is most likely associated with either a driver or hardware failure.  The best
*                   course of action it to increment the interface number & allow software to attempt
*                   to bring up the next interface.
*
*               (4) Upon adding an Ethernet interface, the highest possible Ethernet MTU is configured.
*                   If this value needs to be changed, either prior to starting the interface, or during
*                   run-time, it may be reconfigured by calling NetIF_MTU_Set() from the application.
*********************************************************************************************************
*/

static  void  NetIF_Ether_IF_Add (NET_IF   *p_if,
                                  NET_ERR  *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_BOOLEAN         flags_invalid;
    NET_PHY_API_ETHER  *p_phy_api;
    NET_PHY_CFG_ETHER  *p_phy_cfg;
    CPU_BOOLEAN         phy_api_none;
    CPU_BOOLEAN         phy_api_avail;
#endif
    NET_DEV_CFG_ETHER  *p_dev_cfg;
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    void               *p_addr_hw;
    CPU_SIZE_T          reqd_octets;
    NET_BUF_SIZE        buf_size_max;
    NET_MTU             mtu_max;
    LIB_ERR             err_lib;


    p_dev_cfg = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    p_phy_api = (NET_PHY_API_ETHER *)p_if->Ext_API;
    p_phy_cfg = (NET_PHY_CFG_ETHER *)p_if->Ext_Cfg;

    phy_api_none  = ((p_phy_api == (void *)0) &&
                     (p_phy_cfg == (void *)0)) ? DEF_YES : DEF_NO;
    phy_api_avail = ((p_phy_api != (void *)0) &&
                     (p_phy_cfg != (void *)0)) ? DEF_YES : DEF_NO;
    if ((phy_api_none  != DEF_YES) &&                           /* If phy API NOT NULL                           ...    */
        (phy_api_avail != DEF_YES)) {                           /* ... or avail (see Note #4b);                  ...    */
        *p_err =  NET_PHY_ERR_INVALID_CFG;                      /* ... & rtn err.                                       */
         goto exit;
    }

                                                                /* ----------------- VALIDATE DEV CFG ----------------- */
    flags_invalid = DEF_BIT_IS_SET_ANY(p_dev_cfg->Flags,
                   (NET_DEV_CFG_FLAGS)~NET_DEV_CFG_FLAG_MASK);
    if (flags_invalid == DEF_YES) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

                                                                /* ----------------- VALIDATE PHY CFG ----------------- */
    if (p_if->Ext_API != (NET_PHY_API_ETHER *)0) {
        switch (p_phy_cfg->Spd) {                               /* Validate phy bus spd.                                */
            case NET_PHY_SPD_10:
            case NET_PHY_SPD_100:
            case NET_PHY_SPD_1000:
            case NET_PHY_SPD_AUTO:
                 break;

            default:
                *p_err = NET_DEV_ERR_INVALID_CFG;
                 goto exit;
        }


        switch (p_phy_cfg->Duplex) {                            /* Validate phy bus duplex.                             */
            case NET_PHY_DUPLEX_HALF:
            case NET_PHY_DUPLEX_FULL:
            case NET_PHY_DUPLEX_AUTO:
                 break;

            default:
                *p_err = NET_DEV_ERR_INVALID_CFG;
                 goto exit;
        }
    }
#endif

                                                                /* ------------------- CFG ETHER IF ------------------- */
    p_if->Type = NET_IF_TYPE_ETHER;                             /* Set IF type to Ether.                                */


    NetIF_BufPoolInit(p_if, p_err);                             /* Init IF's buf pools.                                 */
    if (*p_err != NET_IF_ERR_NONE) {                            /* On any err(s);                                  ...  */
         goto exit;
    }

                                                                /* ------------- INIT ETHER DEV DATA AREA ------------- */
    p_if->IF_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_IF_DATA_ETHER),
                                  (CPU_SIZE_T  ) sizeof(void *),
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&err_lib);
    if (p_if->IF_Data == (void *)0) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        goto exit;
    }

    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;



                                                                /* --------------- INIT IF HW/MAC ADDR ---------------- */
    p_addr_hw = &p_if_data->HW_Addr[0];
    Mem_Clr((void     *)p_addr_hw,                              /* Clr hw addr (see Note #2).                           */
            (CPU_SIZE_T)NET_IF_802x_ADDR_SIZE);

                                                                /* ------------------- INIT DEV HW -------------------- */
    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_ETHER *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        goto exit;
    }
    if (p_dev_api->Init == (void (*)(NET_IF  *,
                                     NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        goto exit;
    }
#endif

    p_dev_api->Init(p_if, p_err);                               /* Init but don't start dev HW.                         */
    if (*p_err != NET_DEV_ERR_NONE) {                           /* See Note #3.                                         */
         goto exit;
    }

                                                                /* --------------------- INIT MTU --------------------- */
    buf_size_max = DEF_MAX(p_dev_cfg->TxBufLargeSize,
                           p_dev_cfg->TxBufSmallSize);
    mtu_max      = DEF_MIN(NET_IF_MTU_ETHER, buf_size_max);
    p_if->MTU    = mtu_max;                                     /* Set Ether MTU (see Note #4).                         */


   *p_err = NET_IF_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                        NetIF_Ether_IF_Start()
*
* Description : (1) Start an Ethernet Network Interface :
*
*                   (a) Start Ethernet device
*                   (b) Start Ethernet physical layer, if available :
*                       (1) Initialize physical layer
*                       (2) Enable     physical layer
*                       (3) Check      physical layer link status
*
*
* Argument(s) : p_if        Pointer to Ethernet network interface to start.
*               ----        Argument validated in NetIF_Start().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Ethernet interface successfully started.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               ----- RETURNED BY 'pphy_api->Init()' : -----
*                                                               See specific PHY device(s)  'Init()'
*                                                                   for additional return error codes.
*
*                                                               - RETURNED BY 'pphy_api->LinkStateSet()' : -
*                                                               See specific PHY device(s) 'LinkStateSet()'
*                                                                   for additional return error codes.
*
*                                                               ---- RETURNED BY 'pdev_api->Start()' : -----
*                                                               See specific network device(s) 'Start()'
*                                                                   for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Start() via 'pif_api->Start()'.
*
* Note(s)     : (2) If present, an attempt will be made to initialize the Ethernet Phy (Physical Layer).
*                   This function assumes that the device driver has initialized the Phy (R)MII bus prior
*                   to the Phy initialization & link state get calls.
*
*               (3) The MII register block remains enabled while the Phy PWRDOWN bit is set.  Thus all
*                   parameters may be configured PRIOR to enabling the analog portions of the Phy logic.
*
*               (4) If the Phy enable or link state get functions return an error, they may be ignored
*                   since the Phy may be enabled by default after reset, & the link may become established
*                   at a later time.
*********************************************************************************************************
*/

static  void  NetIF_Ether_IF_Start (NET_IF   *p_if,
                                    NET_ERR  *p_err)
{
    NET_DEV_LINK_ETHER   link_state;
    NET_DEV_API_ETHER   *p_dev_api;
    NET_PHY_API_ETHER   *p_phy_api;
    NET_PHY_CFG_ETHER   *p_phy_cfg;
    NET_ERR              phy_err;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    p_phy_api = (NET_PHY_API_ETHER *)p_if->Ext_API;
    p_phy_cfg = (NET_PHY_CFG_ETHER *)p_if->Ext_Cfg;

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_ETHER *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_dev_api->Start == (void (*)(NET_IF  *,
                                      NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

    p_dev_api->Start(p_if, p_err);                              /* Start dev.                                           */
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    if (p_phy_api != (NET_PHY_API_ETHER *)0) {                  /* If avail, ...                                        */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
        if (p_phy_api->Init  == (void (*)(NET_IF  *,
                                          NET_ERR *))0) {
            p_dev_api->Stop(p_if, p_err);                       /* Stop device and deallocate resources.                */
           *p_err = NET_ERR_FAULT_NULL_FNCT;
            return;
        }
        if (p_phy_api->EnDis == (void (*)(NET_IF    *,
                                          CPU_BOOLEAN,
                                          NET_ERR   *))0) {
            p_dev_api->Stop(p_if, p_err);                       /* Stop device and deallocate resources.                */
           *p_err = NET_ERR_FAULT_NULL_FNCT;
            return;
        }
        if (p_phy_api->LinkStateGet == (void (*)(NET_IF             *,
                                                 NET_DEV_LINK_ETHER *,
                                                 NET_ERR            *))0) {
            p_dev_api->Stop(p_if, p_err);                       /* Stop device and deallocate resources.                */
           *p_err = NET_ERR_FAULT_NULL_FNCT;
            return;
        }
        if (p_phy_api->LinkStateSet == (void (*)(NET_IF             *,
                                                 NET_DEV_LINK_ETHER *,
                                                 NET_ERR            *))0) {
            p_dev_api->Stop(p_if, p_err);                       /* Stop device and deallocate resources.                */
           *p_err = NET_ERR_FAULT_NULL_FNCT;
            return;
        }

        if (p_phy_cfg == (NET_PHY_CFG_ETHER *)0) {
            p_dev_api->Stop(p_if, p_err);                       /* Stop device and deallocate resources.                */
           *p_err = NET_IF_ERR_INVALID_CFG;
            return;
        }
#endif
        p_phy_api->Init(p_if, &phy_err);                        /* ... init Phy (see Note #2).                          */
        switch (phy_err) {
            case NET_PHY_ERR_NONE:
            case NET_PHY_ERR_TIMEOUT_AUTO_NEG:                  /* Initial auto-negotiation err is passive when ...     */
                 break;                                         /* ... Phy is dis'd from reset / init.                  */


            case NET_ERR_FAULT_NULL_PTR:
            case NET_PHY_ERR_TIMEOUT_RESET:
            case NET_PHY_ERR_TIMEOUT_REG_RD:
            case NET_PHY_ERR_TIMEOUT_REG_WR:
            default:
                 p_dev_api->Stop(p_if, p_err);                  /* Stop device and deallocate resources.                */
                 if (*p_err == NET_DEV_ERR_NONE) {              /* If driver dealloc'd resources w/o err don't return...*/
                    *p_err = phy_err;                           /* ...val in 'p_err'; return val in 'phy_err' instead.  */
                 }
                 return;
        }

                                                                /* Cfg link state (see Note #3).                        */
        link_state.Spd    = p_phy_cfg->Spd;
        link_state.Duplex = p_phy_cfg->Duplex;
        p_phy_api->LinkStateSet(p_if, &link_state, &phy_err);
        switch (phy_err) {
            case NET_PHY_ERR_NONE:
            case NET_PHY_ERR_TIMEOUT_AUTO_NEG:
                 break;

            default:
                 p_dev_api->Stop(p_if, p_err);                  /* Stop device and deallocate resources.                */
                 if (*p_err == NET_DEV_ERR_NONE) {              /* If driver dealloc'd resources w/o err don't return...*/
                    *p_err = phy_err;                           /* ...val in 'p_err'; return val in 'phy_err' instead.  */
                 }
                 return;
        }

        p_phy_api->EnDis(p_if, DEF_ENABLED, &phy_err);          /* En Phy.                                              */
       (void)&phy_err;                                          /* See Note #4.                                         */

        p_phy_api->LinkStateGet(p_if, &link_state, &phy_err);   /* See Note #4.                                         */
        if (phy_err != NET_PHY_ERR_NONE) {
             p_if->Link = NET_IF_LINK_DOWN;
        } else {
            if (link_state.Spd > NET_PHY_SPD_0) {
                p_if->Link = NET_IF_LINK_UP;
            } else {
                p_if->Link = NET_IF_LINK_DOWN;
            }
        }
    }


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetIF_Ether_IF_Stop()
*
* Description : (1) Stop Specific Network Interface :
*
*                   (a) Stop Ethernet device
*                   (b) Stop Ethernet physical layer, if available
*
*
* Argument(s) : p_if        Pointer to Ethernet network interface to stop.
*               ----        Argument validated in NetIF_Stop().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Ethernet interface successfully stopped.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               --- RETURNED BY 'pdev_api->Stop()' : ---
*                                                               See specific network device(s) 'Stop()'
*                                                                   for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Stop() via 'pif_api->Stop()'.
*
* Note(s)     : (2) If the Phy returns an error, it may be ignored since the device has been successfully
*                   stopped.  One side effect may be that the Phy remains powered on & possibly linked.
*********************************************************************************************************
*/

static  void  NetIF_Ether_IF_Stop (NET_IF   *p_if,
                                   NET_ERR  *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_PHY_API_ETHER  *p_phy_api;
    NET_ERR             phy_err;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    p_phy_api = (NET_PHY_API_ETHER *)p_if->Ext_API;

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_ETHER *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_dev_api->Stop == (void (*)(NET_IF  *,
                                     NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

    p_dev_api->Stop(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    if (p_phy_api != (void *)0) {
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
        if (p_phy_api->EnDis == (void (*)(NET_IF    *,
                                          CPU_BOOLEAN,
                                          NET_ERR   *))0) {
           *p_err = NET_ERR_FAULT_NULL_FNCT;
            return;
        }
#endif
        p_phy_api->EnDis(p_if, DEF_DISABLED, &phy_err);         /* Disable Phy.                                         */
       (void)&phy_err;                                          /* See Note #2.                                         */
    }

    p_if->Link = NET_IF_LINK_DOWN;

   *p_err      = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIF_Ether_IO_CtrlHandler()
*
* Description : Handle an Ethernet interface's (I/O) control(s).
*
* Argument(s) : p_if        Pointer to an Ethernet network interface.
*               ----        Argument validated in NetIF_IO_CtrlHandler().
*
*               opt         Desired I/O control option code to perform; additional control options may be
*                           defined by the device driver :
*
*                               NET_IF_IO_CTRL_LINK_STATE_GET           Get    Ethernet interface's link state,
*                                                                           'UP' or 'DOWN'.
*                               NET_IF_IO_CTRL_LINK_STATE_GET_INFO      Get    Ethernet interface's detailed
*                                                                           link state info.
*                               NET_IF_IO_CTRL_LINK_STATE_UPDATE        Update Ethernet interface's link state.
*
*               p_data      Pointer to variable that will receive possible I/O control data (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Ethernet I/O control option successfully
*                                                                   handled.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*
*                                                               -- RETURNED BY 'pdev_api->IO_Ctrl()' : ---
*                                                               See specific network device(s) 'IO_Ctrl()'
*                                                                   for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_IO_CtrlHandler() via 'pif_api->IO_Ctrl()'.
*
* Note(s)     : (1) 'p_data' MUST point to a variable (or memory) that is sufficiently sized AND aligned
*                    to receive any return data.
*********************************************************************************************************
*/

static  void  NetIF_Ether_IO_CtrlHandler (NET_IF      *p_if,
                                          CPU_INT08U   opt,
                                          void        *p_data,
                                          NET_ERR     *p_err)
{
    NET_DEV_API_ETHER   *p_dev_api;
    NET_DEV_LINK_ETHER   p_link_info;
    NET_IF_LINK_STATE   *p_link_state;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;

                                                                /* ------------ VALIDATE NET DEV I/O PTRS ------------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_data == (void *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_dev_api == (NET_DEV_API_ETHER *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_dev_api->IO_Ctrl == (void (*)(NET_IF   *,
                                        CPU_INT08U,
                                        void     *,
                                        NET_ERR  *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

                                                                /* ----------- HANDLE NET DEV I/O CTRL OPT ------------ */
    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET:
             p_dev_api->IO_Ctrl((NET_IF   *) p_if,
                                (CPU_INT08U) NET_IF_IO_CTRL_LINK_STATE_GET_INFO,
                                (void     *)&p_link_info,       /* Get link state info.                                 */
                                (NET_ERR  *) p_err);

             p_link_state = (NET_IF_LINK_STATE *)p_data;        /* See Note #1.                                         */

             if (*p_err       != NET_DEV_ERR_NONE) {
                 *p_link_state = NET_IF_LINK_DOWN;
                  return;
             }

             switch (p_link_info.Spd) {                         /* Demux link state from link spd.                      */
                 case NET_PHY_SPD_10:
                 case NET_PHY_SPD_100:
                 case NET_PHY_SPD_1000:
                     *p_link_state = NET_IF_LINK_UP;
                      break;


                 case NET_PHY_SPD_0:
                 default:
                     *p_link_state = NET_IF_LINK_DOWN;
                      break;
             }
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
                                                                /* Rtn err for unavail ctrl opt?                        */
             break;


        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
        default:                                                /* Handle other dev I/O opt(s).                         */
             p_dev_api->IO_Ctrl((NET_IF   *)p_if,
                                (CPU_INT08U)opt,
                                (void     *)p_data,             /* See Note #1.                                         */
                                (NET_ERR  *)p_err);
             if (*p_err != NET_DEV_ERR_NONE) {
                  return;
             }
             break;
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

#endif  /* NET_IF_ETHER_MODULE_EN   */

