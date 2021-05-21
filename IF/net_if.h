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
*                                    NETWORK INTERFACE MANAGEMENT
*
* Filename : net_if.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Network Interface modules located in the following network directory :
*
*                (a) \<Network Protocol Suite>\IF\net_if.*
*                                                 net_if_*.*
*
*                        where
*                                <Network Protocol Suite>      directory path for network protocol suite
*                                 net_if.*                     Generic  Network Interface Management
*                                                                  module files
*                                 net_if_*.*                   Specific Network Interface(s) module files
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IF_MODULE_PRESENT
#define  NET_IF_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../Source/net.h"
#include  "../Source/net_cfg_net.h"
#include  "../Source/net_buf.h"

#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_IF_PHY_LINK_TIME_MIN_MS                      50
#define  NET_IF_PHY_LINK_TIME_MAX_MS                   60000
#define  NET_IF_PHY_LINK_TIME_DFLT_MS                    250

#define  NET_IF_PERF_MON_TIME_MIN_MS                      50
#define  NET_IF_PERF_MON_TIME_MAX_MS                   60000
#define  NET_IF_PERF_MON_TIME_DFLT_MS                    250


/*
*********************************************************************************************************
*                                NETWORK INTERFACE I/O CONTROL DEFINES
*********************************************************************************************************
*/

#define  NET_IF_IO_CTRL_NONE                               0u
#define  NET_IF_IO_CTRL_LINK_STATE_GET                    10u   /* Get        link state.                               */
#define  NET_IF_IO_CTRL_LINK_STATE_GET_INFO               11u   /* Get        link state info.                          */
#define  NET_IF_IO_CTRL_LINK_STATE_UPDATE                 12u   /* Update dev link state regs.                          */



                                                                /* ----- CFG BUF DATA PROTOCOL MIN/MAX HDR SIZES ------ */

#if  (defined(NET_IF_ETHER_MODULE_EN) || \
      defined(NET_IF_WIFI_MODULE_EN))

    #define  NET_IF_HDR_SIZE_MIN                NET_IF_HDR_SIZE_ETHER
    #define  NET_IF_HDR_SIZE_MAX                NET_IF_HDR_SIZE_ETHER

#elif  (defined(NET_IF_LOOPBACK_MODULE_EN))

    #define  NET_IF_HDR_SIZE_MIN                NET_IF_HDR_SIZE_LOOPBACK
    #define  NET_IF_HDR_SIZE_MAX                NET_IF_HDR_SIZE_LOOPBACK

#else
    #define  NET_IF_HDR_SIZE_MIN                0
    #define  NET_IF_HDR_SIZE_MAX                0
#endif


/*
*********************************************************************************************************
*                                   NETWORK INTERFACE INDEX DEFINES
*
* Note(s) : (1) Since network data value macro's appropriately convert data values from any CPU addresses,
*               word-aligned or not; network receive & transmit packets are NOT required to ensure that
*               network packet headers (ARP/IP/UDP/TCP/etc.) & header members will locate on CPU word-
*               aligned addresses.  Therefore, network interface packets are NOT required to start on
*               any specific buffer indices.
*
*               See also 'net_util.h  NETWORK DATA VALUE MACRO'S           Note #2b'
*                      & 'net_buf.h   NETWORK BUFFER INDEX & SIZE DEFINES  Note #2'.
*********************************************************************************************************
*/
                                                                            /* See Note #1.                             */
#define  NET_IF_IX_RX                                    NET_BUF_DATA_IX_RX
#define  NET_IF_RX_IX                                    NET_IF_IX_RX       /* Req'd for backwards-compatibility.       */


/*
*********************************************************************************************************
*                                 NETWORK INTERFACE NUMBER DATA TYPE
*********************************************************************************************************
*/

#define  NET_IF_NBR_BASE                                   0

#if (NET_IF_CFG_LOOPBACK_EN == DEF_ENABLED)
#define  NET_IF_NBR_BASE_CFGD                            NET_IF_NBR_BASE
#else
#define  NET_IF_NBR_BASE_CFGD                           (NET_IF_NBR_BASE + NET_IF_NBR_IF_RESERVED)
#endif

#define  NET_IF_NBR_NONE                                 NET_IF_NBR_MAX_VAL
#define  NET_IF_NBR_MIN                                  NET_IF_NBR_IF_RESERVED
#define  NET_IF_NBR_MAX                                 (NET_IF_NBR_NONE - 1)

                                                                                /* Reserved net IF nbrs :               */
#define  NET_IF_NBR_LOOPBACK                            (NET_IF_NBR_BASE + 0)
#define  NET_IF_NBR_LOCAL_HOST                           NET_IF_NBR_LOOPBACK
#define  NET_IF_NBR_WILDCARD                             NET_IF_NBR_NONE


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                   NETWORK INTERFACE TYPE DEFINES
*
* Note(s) : (1) NET_IF_TYPE_&&& #define values specifically chosen as ASCII representations of the network
*               interface types.  Memory displays of network interfaces will display the network interface
*               TYPEs with their chosen ASCII names.
*********************************************************************************************************
*/


typedef  enum  net_if_mem_type {
    NET_IF_MEM_TYPE_NONE = 0,
    NET_IF_MEM_TYPE_MAIN,                       /* Create dev's net bufs in main      mem.              */
    NET_IF_MEM_TYPE_DEDICATED                   /* Create dev's net bufs in dedicated mem.              */
} NET_IF_MEM_TYPE;


/*
*********************************************************************************************************
*                     NETWORK DEVICE INTERRUPT SERVICE ROUTINE (ISR) TYPE DATA TYPE
*
* Note(s) : (1) The following network device interrupt service routine (ISR) types are currently supported.
*
*               However, this may NOT be a complete or exhaustive list of device ISR type(s).  Therefore,
*               ANY addition, modification, or removal of network device ISR types SHOULD be appropriately
*               synchronized &/or updated with (ALL) device driver ISR handlers.
*********************************************************************************************************
*/

typedef  enum  net_dev_isr_type {
    NET_DEV_ISR_TYPE_NONE,
    NET_DEV_ISR_TYPE_UNKNOWN,                                   /* Dev                     ISR unknown.                 */

    NET_DEV_ISR_TYPE_RX,                                        /* Dev rx                  ISR.                         */
    NET_DEV_ISR_TYPE_RX_RUNT,                                   /* Dev rx runt             ISR.                         */
    NET_DEV_ISR_TYPE_RX_OVERRUN,                                /* Dev rx overrun          ISR.                         */

    NET_DEV_ISR_TYPE_TX_RDY,                                    /* Dev tx rdy              ISR.                         */
    NET_DEV_ISR_TYPE_TX_COMPLETE,                               /* Dev tx complete         ISR.                         */
    NET_DEV_ISR_TYPE_TX_COLLISION_LATE,                         /* Dev tx late   collision ISR.                         */
    NET_DEV_ISR_TYPE_TX_COLLISION_EXCESS,                       /* Dev tx excess collision ISR.                         */

    NET_DEV_ISR_TYPE_JABBER,                                    /* Dev jabber              ISR.                         */
    NET_DEV_ISR_TYPE_BABBLE,                                    /* Dev babble              ISR.                         */

    NET_DEV_ISR_TYPE_PHY,                                       /* Dev phy                 ISR.                         */


    NET_DEV_ISR_TYPE_TX_DONE = NET_DEV_ISR_TYPE_TX_COMPLETE
} NET_DEV_ISR_TYPE;


/*
*********************************************************************************************************
*                              NETWORK DEVICE CONFIGURATION FLAG DATA TYPE
*
* Note(s) : (1) The following network device configuration flags are currently supported :
*
*********************************************************************************************************
*/

typedef  CPU_INT32U  NET_DEV_CFG_FLAGS;

                                                                /* See Note #1.                                         */
#define  NET_DEV_CFG_FLAG_NONE                           DEF_BIT_NONE
#define  NET_DEV_CFG_FLAG_SWAP_OCTETS                    DEF_BIT_00

#define  NET_DEV_CFG_FLAG_MASK                          (NET_DEV_CFG_FLAG_NONE       | \
                                                         NET_DEV_CFG_FLAG_SWAP_OCTETS)


/*
*********************************************************************************************************
*                        NETWORK INTERFACE PERFORMANCE MONITOR STATE DATA TYPE
*********************************************************************************************************
*/

typedef  enum net_if_perf_mon_state {
    NET_IF_PERF_MON_STATE_NONE,
    NET_IF_PERF_MON_STATE_STOP,
    NET_IF_PERF_MON_STATE_START,
    NET_IF_PERF_MON_STATE_RUN
} NET_IF_PERF_MON_STATE;

typedef  enum net_if_link_sate {
  NET_IF_LINK_UP,
  NET_IF_LINK_DOWN
} NET_IF_LINK_STATE;


/*
*********************************************************************************************************
*                         NETWORK INTERFACE PERFORMANCE MONITOR STATE DEFINES
*********************************************************************************************************
*/

typedef  void  (*NET_IF_LINK_SUBSCRIBER_FNCT)(NET_IF_NBR        if_nbr,
                                              NET_IF_LINK_STATE link_state);

typedef  struct  net_if_link_subscriber_obj  NET_IF_LINK_SUBSCRIBER_OBJ;

struct  net_if_link_subscriber_obj{
    NET_IF_LINK_SUBSCRIBER_FNCT   Fnct;
    CPU_INT32U                    RefCtn;
    NET_IF_LINK_SUBSCRIBER_OBJ   *NextPtr;
};


/*
*********************************************************************************************************
*                                     NETWORK INTERFACE DATA TYPE
*
* Note(s) : (1) A network interface's hardware MTU is computed as the minimum of the largest buffer size
*               configured for a specific interface & the configured MTU for the interface device.
*
*           (2) Network interface initialization flag set when an interface has been successfully added
*               & initialized to the interface table.  Once set, this flag is never cleared since the
*               removal of interfaces is currently not allowed.
*
*           (3) Network interface enable/disable independent of physical hardware link state of the
*               interface's associated device.
**********************************************************************************************************
*/

                                                        /* -------------------------- NET IF -------------------------- */
struct  net_if {
    NET_IF_TYPE                  Type;                  /* IF type (Loopback, Ethernet, PPP, Serial device, etc.).      */
    NET_IF_NBR                   Nbr;                   /* IF nbr.                                                      */

    CPU_BOOLEAN                  Init;                  /* IF init     status (see Note #2).                            */
    CPU_BOOLEAN                  En;                    /* IF en/dis   status (see Note #3).                            */

    NET_IF_LINK_STATE            LinkPrev;
    NET_IF_LINK_STATE            Link;                  /* IF current  Phy link status.                                 */
    MEM_DYN_POOL                 LinkSubscriberPool;
    NET_IF_LINK_SUBSCRIBER_OBJ  *LinkSubscriberListHeadPtr;
    NET_IF_LINK_SUBSCRIBER_OBJ  *LinkSubscriberListEndPtr;

    NET_MTU                      MTU;                   /* IF MTU             (see Note #1).                            */

    void                        *IF_API;                /* Ptr to IF's     API  fnct tbl.                               */
    void                        *IF_Data;               /* Ptr to IF's     data area.                                   */
    void                        *Dev_API;               /* Ptr to IF's dev API  fnct tbl.                               */
    void                        *Dev_BSP;               /* Ptr to IF's dev BSP  fnct tbl.                               */
    void                        *Dev_Cfg;               /* Ptr to IF's dev cfg       tbl.                               */
    void                        *Dev_Data;              /* Ptr to IF's dev data area.                                   */
    void                        *Ext_API;               /* Ptr to IF's phy API  fnct tbl.                               */
    void                        *Ext_Cfg;               /* Ptr to IF's phy cfg       tbl.                               */
    void                        *Ext_Data;              /* Ptr to IF's phy data area.                                   */

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_IF_PERF_MON_STATE        PerfMonState;          /* Perf mon state.                                              */
    NET_TS_MS                    PerfMonTS_Prev_ms;     /* Perf mon prev TS (in ms).                                    */
#endif

#ifdef  NET_LOAD_BAL_MODULE_EN
    NET_STAT_CTR                 RxPktCtr;              /* Indicates nbr of rx pkts q'd to IF but NOT yet handled.      */


    KAL_SEM_HANDLE               TxSuspendSignalObj;
    CPU_INT32U                   TxSuspendTimeout_ms;
    NET_STAT_CTR                 TxSuspendCtr;          /* Indicates nbr of tx conn's for  IF currently suspended.      */
#endif

    KAL_SEM_HANDLE               DevTxRdySignalObj;
};


/*
*********************************************************************************************************
*                           GENERIC NETWORK DEVICE CONFIGURATION DATA TYPE
*
* Note(s) : (1) The generic network device configuration data type is a template/subset for all specific
*               network device configuration data types.  Each specific network device configuration
*               data type MUST define ALL generic network device configuration parameters, synchronized
*               in both the sequential order & data type of each parameter.
*
*               Thus ANY modification to the sequential order or data types of generic configuration
*               parameters MUST be appropriately synchronized between the generic network device
*               configuration data type & ALL specific network device configuration data types.
*********************************************************************************************************
*/

                                                    /* ------------------------- NET DEV CFG -------------------------- */
struct  net_dev_cfg {
    NET_IF_MEM_TYPE     RxBufPoolType;              /* Rx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE        RxBufLargeSize;             /* Size  of dev rx large buf data areas (in octets).                */
    NET_BUF_QTY         RxBufLargeNbr;              /* Nbr   of dev rx large buf data areas.                            */
    NET_BUF_SIZE        RxBufAlignOctets;           /* Align of dev rx       buf data areas (in octets).                */
    NET_BUF_SIZE        RxBufIxOffset;              /* Offset from base ix to rx data into data area (in octets).       */


    NET_IF_MEM_TYPE     TxBufPoolType;              /* Tx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE        TxBufLargeSize;             /* Size  of dev tx large buf data areas (in octets).                */
    NET_BUF_QTY         TxBufLargeNbr;              /* Nbr   of dev tx large buf data areas.                            */
    NET_BUF_SIZE        TxBufSmallSize;             /* Size  of dev tx small buf data areas (in octets).                */
    NET_BUF_QTY         TxBufSmallNbr;              /* Nbr   of dev tx small buf data areas.                            */
    NET_BUF_SIZE        TxBufAlignOctets;           /* Align of dev tx       buf data areas (in octets).                */
    NET_BUF_SIZE        TxBufIxOffset;              /* Offset from base ix to tx data from data area (in octets).       */


    CPU_ADDR            MemAddr;                    /* Base addr of (dev's) dedicated mem, if avail.                    */
    CPU_ADDR            MemSize;                    /* Size      of (dev's) dedicated mem, if avail.                    */


    NET_DEV_CFG_FLAGS   Flags;                      /* Opt'l bit flags.                                                 */
};


/*
*********************************************************************************************************
*                               GENERIC NETWORK INTERFACE API DATA TYPE
*
* Note(s) : (1) The generic network interface application programming interface (API) data type is a
*               template/subset for all specific network interface API data types.
*
*               (a) Each specific network interface API data type definition MUST define ALL generic
*                   network interface API functions, synchronized in both the sequential order of the
*                   functions & argument lists for each function.
*
*                   Thus ANY modification to the sequential order or argument lists of the generic API
*                   functions MUST be appropriately synchronized between the generic network interface
*                   API data type & ALL specific network interface API data type definitions/instantiations.
*
*               (b) ALL API functions SHOULD be defined with NO NULL functions for all specific network
*                   interface API instantiations.  Any specific network interface API instantiation that
*                   does define any NULL API functions MUST ensure that NO NULL API functions are called
*                   for the specific network interface.
*
*                   Instead of  NULL functions, a specific network interface API instantiation COULD
*                   define empty API functions that return error code 'NET_ERR_FAULT_FEATURE_DIS'.
*********************************************************************************************************
*/

                                                                                    /* ---------- NET IF API ---------- */
                                                                                    /* Net IF API fnct ptrs :           */
struct  net_if_api {
                                                                                    /*   Init/add                       */
    void         (*Add)                      (NET_IF             *p_if,
                                              NET_ERR            *p_err);

                                                                                    /*   Start                          */
    void         (*Start)                    (NET_IF             *p_if,
                                              NET_ERR            *p_err);

                                                                                    /*   Stop                           */
    void         (*Stop)                     (NET_IF             *p_if,
                                              NET_ERR            *p_err);


                                                                                    /*   Rx                             */
    void         (*Rx)                       (NET_IF             *p_if,
                                              NET_BUF            *p_buf,
                                              NET_ERR            *p_err);

                                                                                    /*   Tx                             */
    void         (*Tx)                       (NET_IF             *p_if,
                                              NET_BUF            *p_buf,
                                              NET_ERR            *p_err);


                                                                                    /*   Hw addr get                    */
    void         (*AddrHW_Get)               (NET_IF             *p_if,
                                              CPU_INT08U         *p_addr,
                                              CPU_INT08U         *addr_len,
                                              NET_ERR            *p_err);

                                                                                    /*   Hw addr set                    */
    void         (*AddrHW_Set)               (NET_IF             *p_if,
                                              CPU_INT08U         *p_addr,
                                              CPU_INT08U          addr_len,
                                              NET_ERR            *p_err);

                                                                                    /*   Hw addr valid                  */
    CPU_BOOLEAN  (*AddrHW_IsValid)           (NET_IF             *p_if,
                                              CPU_INT08U         *p_addr_hw);


                                                                                    /*   Multicast addr add             */
    void         (*AddrMulticastAdd)         (NET_IF             *p_if,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          paddr_protocol_len,
                                              NET_PROTOCOL_TYPE   addr_protocol_type,
                                              NET_ERR            *p_err);

                                                                                    /*   Multicast addr remove          */
    void         (*AddrMulticastRemove)      (NET_IF             *p_if,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          paddr_protocol_len,
                                              NET_PROTOCOL_TYPE   addr_protocol_type,
                                              NET_ERR            *p_err);

                                                                                    /*   Multicast addr protocol-to-hw  */
    void         (*AddrMulticastProtocolToHW)(NET_IF             *p_if,
                                              CPU_INT08U         *p_addr_protocol,
                                              CPU_INT08U          addr_protocol_len,
                                              NET_PROTOCOL_TYPE   addr_protocol_type,
                                              CPU_INT08U         *p_addr_hw,
                                              CPU_INT08U         *p_addr_hw_len,
                                              NET_ERR            *p_err);


                                                                                    /*   Buf cfg validation             */
    void         (*BufPoolCfgValidate)       (NET_IF             *p_if,
                                              NET_ERR            *p_err);


                                                                                    /*   MTU set                        */
    void         (*MTU_Set)                  (NET_IF             *p_if,
                                              NET_MTU             mtu,
                                              NET_ERR            *p_err);


                                                                                    /*   Get pkt hdr size               */
    CPU_INT16U   (*GetPktSizeHdr)            (NET_IF             *p_if);

                                                                                    /*   Get pkt min size               */
    CPU_INT16U   (*GetPktSizeMin)            (NET_IF             *p_if);


    /*   Get pkt min size               */
    CPU_INT16U   (*GetPktSizeMax)            (NET_IF             *p_if);

                                                                                    /*   ISR handler                    */
    void         (*ISR_Handler)              (NET_IF             *p_if,
                                              NET_DEV_ISR_TYPE    type,
                                              NET_ERR            *p_err);


                                                                                    /*   I/O ctrl                       */
    void         (*IO_Ctrl)                  (NET_IF             *p_if,
                                              CPU_INT08U          opt,
                                              void               *p_data,
                                              NET_ERR            *p_err);
};


/*
*********************************************************************************************************
*                                GENERIC NETWORK DEVICE API DATA TYPE
*
* Note(s) : (1) The generic network device application programming interface (API) data type is a template/
*               subset for all specific network device API data types.
*
*               (a) Each specific network device API data type definition MUST define ALL generic network
*                   device API functions, synchronized in both the sequential order of the functions &
*                   argument lists for each function.
*
*                   Thus ANY modification to the sequential order or argument lists of the  generic API
*                   functions MUST be appropriately synchronized between the generic network device API
*                   data type & ALL specific network device API data type definitions/instantiations.
*
*                   However, specific network device API data type definitions/instantiations MAY include
*                   additional API functions after all generic network device API functions.
*
*               (b) ALL API functions MUST be defined with NO NULL functions for all specific network
*                   device API instantiations.  Any specific network device API instantiation that does
*                   NOT require a specific API's functionality MUST define an empty API function which
*                   may need to return an appropriate error code.
*********************************************************************************************************
*/

                                                                /* -------------------- NET DEV API ------------------- */
                                                                /* Net dev API fnct ptrs :                              */
typedef  struct  net_dev_api {
                                                                /* Init                                                 */
    void  (*Init)               (NET_IF              *p_if,
                                 NET_ERR             *p_err);
                                                                /* Start                                                */
    void  (*Start)              (NET_IF              *p_if,
                                 NET_ERR             *p_err);
                                                                /* Stop                                                 */
    void  (*Stop)               (NET_IF              *p_if,
                                 NET_ERR             *p_err);
                                                                /* Rx                                                   */
    void  (*Rx)                 (NET_IF              *p_if,
                                 CPU_INT08U         **p_data,
                                 CPU_INT16U          *size,
                                 NET_ERR             *p_err);
                                                                /* Tx                                                   */
    void  (*Tx)                 (NET_IF              *p_if,
                                 CPU_INT08U          *p_data,
                                 CPU_INT16U           size,
                                 NET_ERR             *p_err);
                                                                /* Multicast addr add                                   */
    void  (*AddrMulticastAdd)   (NET_IF              *p_if,
                                 CPU_INT08U          *p_addr_hw,
                                 CPU_INT08U           addr_hw_len,
                                 NET_ERR             *p_err);
                                                                /* Multicast addr remove                                */
    void  (*AddrMulticastRemove)(NET_IF              *p_if,
                                 CPU_INT08U          *p_addr_hw,
                                 CPU_INT08U           addr_hw_len,
                                 NET_ERR             *p_err);
                                                                /* ISR handler                                          */
    void  (*ISR_Handler)        (NET_IF              *p_if,
                                 NET_DEV_ISR_TYPE     type);
} NET_DEV_API;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

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
*                                             PUBLIC API
*********************************************************************************************************
*/

/* =========================================== CTRL FNCTS ============================================ */

NET_IF_NBR        NetIF_Add                   (void                         *if_api,
                                               void                         *dev_api,
                                               void                         *dev_bsp,
                                               void                         *dev_cfg,
                                               void                         *ext_api,
                                               void                         *ext_cfg,
                                               NET_ERR                      *p_err);

void              NetIF_Start                 (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

void              NetIF_Stop                  (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);


/* ============================================ CFG FNCTS ============================================ */

CPU_BOOLEAN       NetIF_CfgPhyLinkPeriod      (CPU_INT16U                    time_ms);

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
CPU_BOOLEAN       NetIF_CfgPerfMonPeriod      (CPU_INT16U                    time_ms);
#endif

void             *NetIF_GetRxDataAlignPtr     (NET_IF_NBR                    if_nbr,
                                               void                         *p_data,
                                               NET_ERR                      *p_err);


/* ========================================== STATUS FNCTS =========================================== */

CPU_BOOLEAN       NetIF_IsValid               (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

CPU_BOOLEAN       NetIF_IsValidCfgd           (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

CPU_BOOLEAN       NetIF_IsEn                  (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

CPU_BOOLEAN       NetIF_IsEnCfgd              (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

CPU_INT08U        NetIF_GetExtAvailCtr        (NET_ERR                      *p_err);


NET_IF_NBR        NetIF_GetNbrBaseCfgd            (void);


/* =========================================== MGMT FNCTS ============================================ */

void              NetIF_AddrHW_Get            (NET_IF_NBR                    if_nbr,
                                               CPU_INT08U                   *p_addr_hw,
                                               CPU_INT08U                   *p_addr_len,
                                               NET_ERR                      *p_err);

void              NetIF_AddrHW_Set            (NET_IF_NBR                    if_nbr,
                                               CPU_INT08U                   *p_addr_hw,
                                               CPU_INT08U                    addr_len,
                                               NET_ERR                      *p_err);

CPU_BOOLEAN       NetIF_AddrHW_IsValid        (NET_IF_NBR                    if_nbr,
                                               CPU_INT08U                   *p_addr_hw,
                                               NET_ERR                      *p_err);

NET_MTU           NetIF_MTU_Get               (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

void              NetIF_MTU_Set               (NET_IF_NBR                    if_nbr,
                                               NET_MTU                       mtu,
                                               NET_ERR                      *p_err);

NET_IF_LINK_STATE NetIF_LinkStateGet          (NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);

CPU_BOOLEAN       NetIF_LinkStateWaitUntilUp  (NET_IF_NBR                    if_nbr,
                                               CPU_INT16U                    retry_max,
                                               CPU_INT32U                    time_dly_ms,
                                               NET_ERR                      *p_err);

void              NetIF_LinkStateSubscribe    (NET_IF_NBR                    if_nbr,
                                               NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                               NET_ERR                      *p_err);

void              NetIF_LinkStateUnsubscribe  (NET_IF_NBR                    if_nbr,
                                               NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                               NET_ERR                      *p_err);

void              NetIF_IO_Ctrl               (NET_IF_NBR                    if_nbr,
                                               CPU_INT08U                    opt,
                                               void                         *p_data,
                                               NET_ERR                      *p_err);

#ifdef  NET_LOAD_BAL_MODULE_EN
void              NetIF_TxSuspendTimeoutSet   (NET_IF_NBR                    if_nbr,
                                               CPU_INT32U                    timeout_ms,
                                               NET_ERR                      *p_err);

CPU_INT32U        NetIF_TxSuspendTimeoutGet_ms(NET_IF_NBR                    if_nbr,
                                               NET_ERR                      *p_err);
#endif


/*
*********************************************************************************************************
*                                               BSP API
*********************************************************************************************************
*/

void  NetIF_ISR_Handler(NET_IF_NBR          if_nbr,
                        NET_DEV_ISR_TYPE    type,
                        NET_ERR            *p_err);


/*
*********************************************************************************************************
*                                             DRIVER API
*********************************************************************************************************
*/

void  NetIF_RxTaskSignal     (NET_IF_NBR   if_nbr,              /* Signal IF rx rdy    from dev rx ISR(s).              */
                              NET_ERR     *p_err);


void  NetIF_DevCfgTxRdySignal(NET_IF      *p_if,
                              CPU_INT16U   cnt,
                              NET_ERR     *p_err);

void  NetIF_DevTxRdySignal   (NET_IF      *p_if);

void  NetIF_TxDeallocTaskPost(CPU_INT08U  *p_buf_data,          /* Post to tx dealloc Q.                                */
                              NET_ERR     *p_err);


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/


void               NetIF_Init                       (const  NET_TASK_CFG                 *p_rx_task_cfg,
                                                     const  NET_TASK_CFG                 *p_tx_task_cfg,
                                                            NET_ERR                      *p_err);

void               NetIF_BufPoolInit                (       NET_IF                       *p_if,
                                                            NET_ERR                      *p_err);


NET_IF            *NetIF_Get                        (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);

NET_IF_NBR         NetIF_GetDflt                    (       void);


void              *NetIF_GetTxDataAlignPtr          (       NET_IF_NBR                    if_nbr,
                                                            void                         *p_data,
                                                            NET_ERR                      *p_err);


CPU_BOOLEAN        NetIF_IsValidHandler             (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);


CPU_BOOLEAN        NetIF_IsValidCfgdHandler         (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);



CPU_BOOLEAN        NetIF_IsEnHandler                (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);

CPU_BOOLEAN        NetIF_IsEnCfgdHandler            (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);

void               NetIF_AddrHW_GetHandler          (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT08U                   *p_addr_hw,
                                                            CPU_INT08U                   *p_addr_len,
                                                            NET_ERR                      *p_err);

void               NetIF_AddrHW_SetHandler          (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT08U                   *p_addr_hw,
                                                            CPU_INT08U                    addr_len,
                                                            NET_ERR                      *p_err);

CPU_BOOLEAN        NetIF_AddrHW_IsValidHandler      (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT08U                   *p_addr_hw,
                                                            NET_ERR                      *p_err);


#ifdef  NET_MCAST_MODULE_EN
void               NetIF_AddrMulticastAdd           (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT08U                   *p_addr_protocol,
                                                            CPU_INT08U                    addr_protocol_len,
                                                            NET_PROTOCOL_TYPE             addr_protocol_type,
                                                            NET_ERR                      *p_err);

void               NetIF_AddrMulticastRemove        (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT08U                   *p_addr_protocol,
                                                            CPU_INT08U                    addr_protocol_len,
                                                            NET_PROTOCOL_TYPE             addr_protocol_type,
                                                            NET_ERR                      *p_err);
#endif
#ifdef  NET_MCAST_TX_MODULE_EN
void               NetIF_AddrMulticastProtocolToHW  (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT08U                   *p_addr_protocol,
                                                            CPU_INT08U                    addr_protocol_len,
                                                            NET_PROTOCOL_TYPE             addr_protocol_type,
                                                            CPU_INT08U                   *p_addr_hw,
                                                            CPU_INT08U                   *p_addr_hw_len,
                                                            NET_ERR                      *p_err);
#endif


NET_MTU            NetIF_MTU_GetProtocol            (       NET_IF_NBR                    if_nbr,
                                                            NET_PROTOCOL_TYPE             protocol,
                                                            NET_IF_FLAG                   opt,
                                                            NET_ERR                      *p_err);

void               NetIF_MTU_SetHandler             (       NET_IF_NBR                    if_nbr,
                                                            NET_MTU                       mtu,
                                                            NET_ERR                      *p_err);

CPU_INT16U         NetIF_GetPayloadRxMax            (       NET_IF_NBR                    if_nbr,
                                                            NET_PROTOCOL_TYPE             protocol,
                                                            NET_ERR                      *p_err);

CPU_INT16U         NetIF_GetPayloadTxMax            (       NET_IF_NBR                    if_nbr,
                                                            NET_PROTOCOL_TYPE             protocol,
                                                            NET_ERR                      *p_err);

CPU_INT16U         NetIF_GetPktSizeMin              (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);

CPU_INT16U         NetIF_GetPktSizeMax              (       NET_IF_NBR                    if_nbr,
                                                            NET_ERR                      *p_err);

void               NetIF_RxPktInc                   (       NET_IF_NBR                    if_nbr);

CPU_BOOLEAN        NetIF_RxPktIsAvail               (       NET_IF_NBR                    if_nbr,
                                                            NET_CTR                       rx_chk_nbr);

void               NetIF_Tx                         (       NET_BUF                      *p_buf_list,
                                                            NET_ERR                      *p_err);

void               NetIF_TxSuspend                  (       NET_IF_NBR                    if_nbr);

void               NetIF_TxIxDataGet                (       NET_IF_NBR                    if_nbr,
                                                            CPU_INT32U                    data_size,
                                                            CPU_INT16U                   *p_ix,
                                                            NET_ERR                      *p_err);

NET_IF_LINK_STATE  NetIF_LinkStateGetHandler        (       NET_IF_NBR   if_nbr,
                                                            NET_ERR     *p_err);

void               NetIF_LinkStateSubscribeHandler  (       NET_IF_NBR                    if_nbr,
                                                            NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                                            NET_ERR                      *p_err);

void               NetIF_LinkStateUnSubscribeHandler(       NET_IF_NBR                    if_nbr,
                                                            NET_IF_LINK_SUBSCRIBER_FNCT   fcnt,
                                                            NET_ERR                      *p_err);


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
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IF_CFG_MAX_NBR_IF
#error  "NET_IF_CFG_MAX_NBR_IF                   not #define'd in 'net_cfg.h'"
#error  "                                  [MUST be  >= NET_IF_NBR_MIN_VAL]  "

#elif   (DEF_CHK_VAL_MIN(NET_IF_CFG_MAX_NBR_IF,       \
                         NET_IF_NBR_MIN_VAL) != DEF_OK)
#error  "NET_IF_CFG_MAX_NBR_IF             illegally #define'd in 'net_cfg.h'"
#error  "                                  [MUST be  >= NET_IF_NBR_MIN_VAL]  "
#endif


                                                            /* Correctly configured in 'net_cfg_net.h'; DO NOT MODIFY.  */
#ifndef  NET_IF_NBR_IF_TOT
#error  "NET_IF_NBR_IF_TOT                       not #define'd in 'net_cfg_net.h'"
#error  "                                  [MUST be  >= NET_IF_NBR_MIN]          "
#error  "                                  [     &&  <= NET_IF_NBR_MAX]          "

#elif   (DEF_CHK_VAL(NET_IF_NBR_IF_TOT,       \
                     NET_IF_NBR_MIN,          \
                     NET_IF_NBR_MAX) != DEF_OK)
#error  "NET_IF_NBR_IF_TOT                 illegally #define'd in 'net_cfg_net.h'"
#error  "                                  [MUST be  >= NET_IF_NBR_MIN]          "
#error  "                                  [     &&  <= NET_IF_NBR_MAX]          "
#endif




#if    ((NET_IF_CFG_LOOPBACK_EN == DEF_DISABLED) && \
        (NET_IF_CFG_ETHER_EN    == DEF_DISABLED) && \
        (NET_IF_CFG_WIFI_EN     == DEF_DISABLED))
#error  "NET_IF_CFG_LOOPBACK_EN &&                                           "
#error  "NET_IF_CFG_ETHER_EN               illegally #define'd in 'net_cfg.h'"
#error  "NET_IF_CFG_LOOPBACK_EN ||                                           "
#error  "NET_IF_CFG_ETHER_EN               [MUST be  DEF_ENABLED ]           "
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_IF_MODULE_PRESENT */

