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
*                                          NETWORK IGMP LAYER
*                                 (INTERNET GROUP MANAGEMENT PROTOCOL)
*
* Filename : net_igmp.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Internet Group Management Protocol ONLY required for network interfaces that require
*                reception of IP class-D (multicast) packets (see RFC #1112, Section 3 'Levels of
*                Conformance : Level 2').
*
*                (a) IGMP is NOT required for the transmission of IP class-D (multicast) packets
*                    (see RFC #1112, Section 3 'Levels of Conformance : Level 1').
*
*            (2) Supports Internet Group Management Protocol version 1, as described in RFC #1112
*                with the following restrictions/constraints :
*
*                (a) Only one socket may receive datagrams for a specific host group address & port
*                    number at any given time.
*
*                    See also 'net_sock.c  Note #1e'.
*
*                (b) Since sockets do NOT automatically leave IGMP host groups when closed,
*                    it is the application's responsibility to leave each host group once it is
*                    no longer needed by the application.
*
*                (c) Transmission of IGMP Query Messages NOT currently supported. #### NET-820
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
#define    NET_IGMP_MODULE
#include  "net_igmp.h"
#include  "net_ipv4.h"
#include  "../../Source/net_buf.h"
#include  "../../Source/net.h"
#include  "../../IF/net_if.h"
#include  "../../Source/net_util.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IGMP_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         IGMP HEADER DEFINES
*********************************************************************************************************
*/

#define  NET_IGMP_HDR_VER_MASK                          0xF0u
#define  NET_IGMP_HDR_VER_SHIFT                            4u
#define  NET_IGMP_HDR_VER                                  1u   /* Supports IGMPv1 ONLY (see 'net_igmp.h  Note #2').    */

#define  NET_IGMP_HDR_TYPE_MASK                         0x0Fu


/*
*********************************************************************************************************
*                                      IGMP MESSAGE SIZE DEFINES
*
* Note(s) : (1) RFC #1112, Appendix I, Section 'State Transition Diagram' states that "to be valid, the
*               ... [received] message[s] must be at least 8 octets long".
*********************************************************************************************************
*/

#define  NET_IGMP_MSG_LEN                  NET_IGMP_HDR_SIZE
#define  NET_IGMP_MSG_LEN_DATA                             0


/*
*********************************************************************************************************
*                                      IGMP MESSAGE TYPE DEFINES
*
* Note(s) : (1) RFC #1112, Appendix I, Section 'Type' states that "there are two types of IGMP message[s]
*               ... to hosts" :
*
*               (a) 1 = Host Membership Query
*               (b) 2 = Host Membership Report
*********************************************************************************************************
*/

#define  NET_IGMP_MSG_TYPE_QUERY                           1u   /* See Note #1a.                                        */
#define  NET_IGMP_MSG_TYPE_REPORT                          2u   /* See Note #1b.                                        */


/*
*********************************************************************************************************
*                                       IGMP HOST GROUP STATES
*
*                                         -------------------
*                                         |                 |
*                                         |                 |
*                                         |                 |
*                                         |                 |
*                            ------------>|      FREE       |<------------
*                            |            |                 |            |
*                            |            |                 |            |
*                            |            |                 |            |
*                            |            |                 |            |
*                            |            -------------------            | (1e) LEAVE GROUP
*                            |                     |                     |
*                            | (1e) LEAVE GROUP    | (1a) JOIN GROUP     |
*                            |                     |                     |
*                   -------------------            |            -------------------
*                   |                 |<------------            |                 |
*                   |                 |                         |                 |
*                   |                 |<------------------------|                 |
*                   |                 |  (1c) QUERY  RECEIVED   |                 |
*                   |    DELAYING     |                         |      IDLE       |
*                   |                 |------------------------>|                 |
*                   |                 |  (1b) REPORT RECEIVED   |                 |
*                   |                 |                         |                 |
*                   |                 |------------------------>|                 |
*                   -------------------  (1d) TIMER  EXPIRED    -------------------
*
*
* Note(s) : (1) RFC #1112, Appendix I, Sections 'Informal Protocol Description' & 'State Transition Diagram'
*               outline the IGMP state diagram :
*
*               (a) An application performs a request to join a multicast group.  A new IGMP host group
*                   entry is allocated from the IGMP host group pool & inserted into the IGMP Host Group
*                   List in the 'DELAYING' state.  A timer is started to transmit a report to inform the
*                   IGMP enabled router.
*
*               (b) The host receives a valid IGMP Host Membership Report message, on the interface the
*                   host has joined the group on.  The timer is stopped & the host group transitions
*                   into the 'IDLE' state.
*
*               (c) A query is received for that IGMP group.  The host group transitions into the 'DELAYING'
*                   state & a timer is started to transmit a report to inform the IGMP router.
*
*               (d) The report delay timer expires for the group & a report for that group is transmitted.
*                   The host group then transitions into the 'IDLE' state.
*
*               (e) The application leaves the group on the interface; the IGMP host group is then freed.
*
*           (2) RFC #1112, Section 7.2 states that "to support IGMP, every level 2 host must join the
*               all-hosts group (address 224.0.0.1) ... and must remain a member for as long as the
*               host is active".
*
*               (a) Therefore, the group 224.0.0.1 is considered a special group, & is always in the
*                   'STATIC' state, meaning it neither can be deleted, nor be put in the 'IDLE' or
*                   'DELAYING' state.
*
*               (b) However, since network interfaces are not yet enabled at IGMP initialization time,
*                   the host delays joining the "all-hosts" group on an interface until the first group
*                   membership is requested on an interface.
*********************************************************************************************************
*/

#define  NET_IGMP_HOST_GRP_STATE_NONE                      0u
#define  NET_IGMP_HOST_GRP_STATE_FREE                      1u
#define  NET_IGMP_HOST_GRP_STATE_DELAYING                  2u
#define  NET_IGMP_HOST_GRP_STATE_IDLE                      3u

#define  NET_IGMP_HOST_GRP_STATE_STATIC                   10u   /* See Note #2.                                         */


/*
*********************************************************************************************************
*                                         IGMP REPORT DEFINES
*
* Note(s) : (1) RFC #1112, Appendix I, Section 'Informal Protocol Description' states that :
*
*               (a) "When a host joins a new group, it should immediately transmit a Report for that
*                    group [...].  To cover the possibility of the initial Report being lost or damaged,
*                    it is recommended that it be repeated once or twice after short delays."
*
*                    The delay between the report transmissions is set to 2 seconds in this implementation.
*
*               (b) "When a host receives a Query [...] it starts a report delay timer for each of its
*                    group memberships on the network interface of the incoming Query.  Each timer is
*                    set to a different, randomly-chosen value between zero and [10] seconds."
*
*           (2) When a transmit error occurs when attempting to transmit an IGMP report, a new timer
*               is set with a delay of NET_IGMP_HOST_GRP_REPORT_DLY_RETRY_SEC seconds to retransmit
*               the report.
*********************************************************************************************************
*/

#define  NET_IGMP_HOST_GRP_REPORT_DLY_JOIN_SEC             2    /* See Note #1a.                                        */
                                                                /* See Note #1b.                                        */
#define  NET_IGMP_HOST_GRP_REPORT_DLY_MIN_SEC              0
#define  NET_IGMP_HOST_GRP_REPORT_DLY_MAX_SEC             10

#define  NET_IGMP_HOST_GRP_REPORT_DLY_RETRY_SEC            2    /* See Note #2.                                         */


/*
*********************************************************************************************************
*                                          IGMP FLAG DEFINES
*********************************************************************************************************
*/

                                                                /* ------------------ NET IGMP FLAGS ------------------ */
#define  NET_IGMP_FLAG_NONE                       DEF_BIT_NONE
#define  NET_IGMP_FLAG_USED                       DEF_BIT_00    /* IGMP host grp cur used; i.e. NOT in free pool.       */



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
*                                 IGMP HOST GROUP QUANTITY DATA TYPE
*
* Note(s) : (1) NET_IGMP_HOST_GRP_NBR_MAX  SHOULD be #define'd based on 'NET_IGMP_HOST_GRP_QTY' data type
*               declared.
*********************************************************************************************************
*/

typedef  CPU_INT16U  NET_IGMP_HOST_GRP_QTY;                     /* Defines max qty of IGMP host groups to support.      */


/*
*********************************************************************************************************
*                                   IGMP HOST GROUP STATE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U  NET_IGMP_HOST_GRP_STATE;


/*
*********************************************************************************************************
*                                        IGMP FLAGS DATA TYPE
*********************************************************************************************************
*/

typedef  NET_FLAGS   NET_IGMP_FLAGS;


/*
*********************************************************************************************************
*                                             IGMP HEADER
*
* Note(s) : (1) See RFC #1112, Appendix I for IGMP packet header format.
*
*           (2) IGMP Version Number & Message Type are encoded in the first octet of an IGMP header as follows :
*
*                         7 6 5 4   3 2 1 0
*                       ---------------------
*                       |  V E R  | T Y P E |
*                       ---------------------
*
*                   where
*                           VER         IGMP version; currently 1 (see 'net_igmp.h  Note #2')
*                           TYPE        IGMP message type         (see 'net_igmp.h  IGMP MESSAGE TYPE DEFINES)
*********************************************************************************************************
*/

                                                                /* ------------------- NET IGMP HDR ------------------- */
typedef  struct  net_igmp_hdr {
    CPU_INT08U      Ver_Type;                                   /* IGMP pkt  ver/type (see Note #2).                    */
    CPU_INT08U      Unused;
    CPU_INT16U      ChkSum;                                     /* IGMP pkt  chk sum.                                   */
    NET_IPv4_ADDR   AddrGrp;                                    /* IPv4   host grp addr.                                */
} NET_IGMP_HDR;


/*
*********************************************************************************************************
*                                     IGMP HOST GROUP DATA TYPES
*
*                                          NET_IGMP_HOST_GRP
*                                         |-----------------|
*                                         | Host Group Type |
*                           Previous      |-----------------|
*                          Host Group <------------O        |
*                                         |-----------------|        Next
*                                         |        O------------> Host Group
*                                         |-----------------|
*                                         |        O------------> Host Group
*                                         |-----------------|        Timer
*                                         |    Interface    |
*                                         |     Number      |
*                                         |-----------------|
*                                         |   IP Address    |
*                                         |-----------------|
*                                         |      State      |
*                                         |-----------------|
*                                         |    Reference    |
*                                         |     Counter     |
*                                         |-----------------|
*********************************************************************************************************
*/

                                                                /* ---------------- NET IGMP HOST GRP ----------------- */
typedef  struct  net_igmp_host_grp  NET_IGMP_HOST_GRP;

struct  net_igmp_host_grp {
    NET_IGMP_HOST_GRP        *PrevPtr;                          /* Ptr to PREV IGMP host grp.                           */
    NET_IGMP_HOST_GRP        *NextPtr;                          /* Ptr to NEXT IGMP host grp.                           */

    NET_TMR                  *TmrPtr;                           /* Ptr to host grp TMR.                                 */

    NET_IF_NBR                IF_Nbr;                           /* IGMP   host grp IF nbr.                              */
    NET_IPv4_ADDR             AddrGrp;                          /* IGMP   host grp IPv4 addr.                           */

    NET_IGMP_HOST_GRP_STATE   State;                            /* IGMP   host grp state.                               */
    CPU_INT16U                RefCtr;                           /* IGMP   host grp ref ctr.                             */

    NET_IGMP_FLAGS            Flags;                            /* IGMP   host grp flags.                               */
};


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

static  NET_IGMP_HOST_GRP   NetIGMP_HostGrpTbl[NET_MCAST_CFG_HOST_GRP_NBR_MAX];
static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpPoolPtr;        /* Ptr to pool of free host grp.                       */
static  NET_STAT_POOL       NetIGMP_HostGrpPoolStat;

static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpListHead;       /* Ptr to head of IGMP Host Grp List.                  */

static  CPU_BOOLEAN         NetIGMP_AllHostsJoinedOnIF[NET_IF_NBR_IF_TOT];

static  RAND_NBR            NetIGMP_RandSeed;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                                            /* ------- RX FNCTS ------- */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void                NetIGMP_RxPktValidateBuf       (NET_BUF_HDR        *p_buf_hdr,
                                                            NET_ERR            *p_err);
#endif /* NET_ERR_CFG_ARG_CHK_DBG_EN */

static  void                NetIGMP_RxPktValidate          (NET_BUF            *p_buf,
                                                            NET_BUF_HDR        *p_buf_hdr,
                                                            NET_IGMP_HDR       *p_igmp_hdr,
                                                            NET_ERR            *p_err);


static  void                NetIGMP_RxMsgQuery             (NET_IF_NBR          if_nbr,
                                                            NET_IGMP_HDR       *p_igmp_hdr);

static  void                NetIGMP_RxMsgReport            (NET_IF_NBR          if_nbr,
                                                            NET_IGMP_HDR       *p_igmp_hdr);


static  void                NetIGMP_RxPktFree              (NET_BUF            *p_buf);

static  void                NetIGMP_RxPktDiscard           (NET_BUF            *p_buf,
                                                            NET_ERR            *p_err);


                                                                                            /* ------- TX FNCTS ------- */
static  void                NetIGMP_TxMsg                  (NET_IF_NBR          if_nbr,
                                                            NET_IPv4_ADDR       addr_src,
                                                            NET_IPv4_ADDR       addr_grp,
                                                            CPU_INT08U          type,
                                                            NET_ERR            *p_err);

static  void                NetIGMP_TxMsgReport            (NET_IGMP_HOST_GRP  *p_host_grp,
                                                            NET_ERR            *p_err);

static  void                NetIGMP_TxIxDataGet            (NET_IF_NBR          if_nbr,
                                                            CPU_INT16U          data_len,
                                                            CPU_INT16U         *p_ix,
                                                            NET_ERR            *p_err);

static  void                NetIGMP_TxPktFree              (NET_BUF            *p_buf);

static  void                NetIGMP_TxPktDiscard           (NET_BUF            *p_buf,
                                                            NET_ERR            *p_err);


                                                                                            /* -- IGMP HOST GRP FNCTS - */

static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpSrch            (NET_IF_NBR          if_nbr,
                                                            NET_IPv4_ADDR       addr_grp);


static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpAdd             (NET_IF_NBR          if_nbr,
                                                            NET_IPv4_ADDR       addr_grp,
                                                            NET_ERR            *p_err);

static  void                NetIGMP_HostGrpRemove          (NET_IGMP_HOST_GRP  *p_host_grp);

static  void                NetIGMP_HostGrpInsert          (NET_IGMP_HOST_GRP  *p_host_grp);

static  void                NetIGMP_HostGrpUnlink          (NET_IGMP_HOST_GRP  *p_host_grp);


static  void                NetIGMP_HostGrpReportDlyTimeout(void               *p_host_grp_timeout);


static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpGet             (NET_ERR            *p_err);

static  void                NetIGMP_HostGrpFree            (NET_IGMP_HOST_GRP  *p_host_grp);

static  void                NetIGMP_HostGrpClr             (NET_IGMP_HOST_GRP  *p_host_grp);


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
*                                            NetIGMP_Init()
*
* Description : (1) Initialize Internet Group Management Protocol Layer :
*
*                   (a) Initialize IGMP host group pool
*                   (b) Initialize IGMP host group table
*                   (c) Initialize IGMP Host Group List pointer
*                   (d) Initialize IGMP all-hosts groups
*                   (e) Initialize IGMP random seed
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
* Note(s)     : (2) IGMP host group pool MUST be initialized PRIOR to initializing the pool with pointers
*                   to IGMP host group.
*
*               (3) (a) RFC #1112, Section 7.2 states that "every level 2 host must join the 'all-hosts'
*                       group ... on each network interface at initialization time and must remain a
*                       member for as long as the host is active".
*
*                   (b) However, network interfaces are not enabled at IGMP initialization time &
*                       cannot be completely configured.  Therefore, joining the "all-hosts" group is
*                       postponed until the first group membership is requested on an interface. #### NET-802
*
*                   See also 'net_igmp.h  IGMP HOST GROUP STATES        Note #2b'
*                          & 'net_igmp.c  NetIGMP_HostGrpJoinHandler()  Note #7'.
*********************************************************************************************************
*/

void  NetIGMP_Init (void)
{
    NET_IGMP_HOST_GRP      *p_host_grp;
    NET_IGMP_HOST_GRP_QTY   i;
    NET_IF_NBR              if_nbr;
    NET_ERR                 err;


                                                                /* ---------- INIT IGMP HOST GRP POOL/STATS ----------- */
    NetIGMP_HostGrpPoolPtr = DEF_NULL;                          /* Init-clr IGMP host grp pool (see Note #2).           */

    NetStat_PoolInit(&NetIGMP_HostGrpPoolStat,
                      NET_MCAST_CFG_HOST_GRP_NBR_MAX,
                     &err);


                                                                /* -------------- INIT IGMP HOST GRP TBL -------------- */
    p_host_grp = &NetIGMP_HostGrpTbl[0];
    for (i = 0u; i < NET_MCAST_CFG_HOST_GRP_NBR_MAX; i++) {
        p_host_grp->State = NET_IGMP_HOST_GRP_STATE_FREE;       /* Init each IGMP host grp as free/NOT used.            */

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        NetIGMP_HostGrpClr(p_host_grp);
#endif
                                                                /* Free each IGMP host grp to pool (see Note #2).       */
        p_host_grp->NextPtr     = NetIGMP_HostGrpPoolPtr;
        NetIGMP_HostGrpPoolPtr = p_host_grp;

        p_host_grp++;
    }


    NetIGMP_HostGrpListHead = (NET_IGMP_HOST_GRP *)0;           /* ----------- INIT IGMP HOST GRP LIST PTR ------------ */


                                                                /* ---------------- INIT ALL-HOSTS GRP ---------------- */
    for (if_nbr = NET_IF_NBR_BASE_CFGD; if_nbr < NET_IF_NBR_IF_TOT; if_nbr++) {
        NetIGMP_AllHostsJoinedOnIF[if_nbr] = DEF_NO;            /* See Note #3.                                         */
    }


                                                                /* ------------------ INIT RAND SEED ------------------ */
    NetIGMP_RandSeed = 1u;                                      /* See 'lib_math.c  Math_Init()  Note #2'.              */
}


/*
*********************************************************************************************************
*                                         NetIGMP_HostGrpJoin()
*
* Description : Join a host group.
*
* Argument(s) : if_nbr      Interface number to join host group.
*
*               addr_grp    IP address of host group to join (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetIGMP_HostGrpJoinHandler() : -
*                               NET_IGMP_ERR_NONE                   Host group successfully joined.
*                               NET_IGMP_ERR_INVALID_ADDR_GRP       Invalid group address.
*                               NET_IGMP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*                               NET_IGMP_ERR_HOST_GRP_INVALID_TYPE  Host group is NOT a valid host group type.
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled configured interface.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   --- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if host group successfully joined.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIGMP_HostGrpJoin() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIGMP_HostGrpJoinHandler()  Note #2'.
*
*               (2) IP host group address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIGMP_HostGrpJoin (NET_IF_NBR      if_nbr,
                                  NET_IPv4_ADDR   addr_grp,
                                  NET_ERR        *p_err)
{
    CPU_BOOLEAN  host_grp_join;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif

    Net_GlobalLockAcquire((void *)&NetIGMP_HostGrpJoin, p_err); /* Acquire net lock (see Note #1b).                     */
    if (*p_err != NET_ERR_NONE) {
         return (DEF_FAIL);
    }
                                                                /* Join host grp.                                       */
    host_grp_join = NetIGMP_HostGrpJoinHandler(if_nbr, addr_grp, p_err);

    Net_GlobalLockRelease();                                    /* Release net lock.                                    */

    return (host_grp_join);
}


/*
*********************************************************************************************************
*                                     NetIGMP_HostGrpJoinHandler()
*
* Description : (1) Join a host group :
*
*                   (a) Validate interface number
*                   (b) Validate internet group address
*                   (c) Search IGMP Host Group List for host group with corresponding address
*                           & interface number
*                   (d) If host group NOT found, allocate new host group.
*                   (e) Advertise membership to multicast router(s)
*
*
* Argument(s) : if_nbr      Interface number to join host group (see Note  #4).
*
*               addr_grp    IP address of host group to join    (see Notes #5 & #6).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE                   Host group successfully joined.
*                               NET_IGMP_ERR_INVALID_ADDR_GRP       Invalid host group address.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   --- RETURNED BY NetIGMP_HostGrpAdd() : ---
*                               NET_IGMP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*                               NET_IGMP_ERR_HOST_GRP_INVALID_TYPE  Host group is NOT a valid host group type.
*
*                                                                   - RETURNED BY NetIF_IsEnCfgdHandler() : --
*                               NET_IF_ERR_INVALID_IF               Invalid OR disabled configured interface.
*
* Return(s)   : DEF_OK,   if host group successfully joined.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIGMP_HostGrpJoin().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : (2) NetIGMP_HostGrpJoinHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetIGMP_HostGrpJoin()  Note #1'.
*
*               (3) NetIGMP_HostGrpJoinHandler() blocked until network initialization completes.
*
*               (4) IGMP host groups can ONLY be joined on configured interface(s); NOT on any
*                   localhost/loopback interface(s).
*
*               (5) IP host group address MUST be in host-order.
*
*               (6) (a) RFC #1112, Section 4 specifies that "class D ... host group addresses range" :
*
*                       (1) "from 224.0.0.0" ...
*                       (2) "to   239.255.255.255".
*
*                   (b) However, RFC #1112, Section 4 adds that :
*
*                       (1) "address 224.0.0.0 is guaranteed not to be assigned to any group", ...
*                       (2) "and 224.0.0.1 is assigned to the permanent group of all IP hosts."
*
*               (7) (a) RFC #1112, Section 7.2 states that "every level 2 host must join the 'all-hosts'
*                       group ... on each network interface at initialization time and must remain a
*                       member for as long as the host is active".
*
*                   (b) However, network interfaces are not enabled at IGMP initialization time &
*                       cannot be completely configured.  Therefore, joining the "all-hosts" group is
*                       postponed until the first group membership is requested on an interface. #### NET-802
*
*                   See also 'net_igmp.h  IGMP HOST GROUP STATES  Note #2b'
*                          & 'net_igmp.c  NetIGMP_Init()          Note #3'.
*
*               (8) RFC #1112, Section 7.1 states that :
*
*                   (a) "It is permissible to join the same group on more than one interface"; ...
*                   (b) "It is also permissible for more than one upper-layer protocol to request
*                        membership in the same group."
*
*               (9) (a) RFC #1112, Appendix I, Section 'Informal Protocol Description' states that "when
*                       a host joins a new group, it should immediately transmit a Report for that group.
*                       ... To cover the possibility of the initial Report being lost of damaged, it is
*                       recommended that it be repeated once or twice after short delays".  Hence :
*
*                       (1) An IGMP Report is transmitted just after inserting the host group into IGMP
*                           Host Group List;
*
*                       (2) A Report timer is configured so a second Report is transmitted after a delay
*                           of NET_IGMP_HOST_GRP_REPORT_DLY_JOIN_SEC seconds.
*
*                   (b) However, failure to transmit the initial report or to configure the timer does
*                       NOT prevent the host group from being inserted into the IGMP Host Group List.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIGMP_HostGrpJoinHandler (NET_IF_NBR      if_nbr,
                                         NET_IPv4_ADDR   addr_grp,
                                         NET_ERR        *p_err)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    NET_TMR_TICK        timeout_tick;
    NET_ERR             err;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }

    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return (DEF_FAIL);
    }
#endif


                                                                /* ----------------- VALIDATE IF NBR ------------------ */
    NetIF_IsEnCfgdHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {                            /* If cfg'd IF NOT en'd (see Note #4), ...              */
         return (DEF_FAIL);                                     /* ... rtn err.                                         */
    }

                                                                /* Chk all-hosts grp (see Note #7b).                    */
    if (NetIGMP_AllHostsJoinedOnIF[if_nbr] != DEF_YES) {
        NetIGMP_HostGrpAdd(if_nbr,
                           NET_IPv4_ADDR_MULTICAST_ALL_HOSTS,
                           p_err);
        if (*p_err != NET_IGMP_ERR_NONE) {
             return (DEF_FAIL);
        }

        NetIGMP_AllHostsJoinedOnIF[if_nbr] = DEF_YES;
    }


                                                                /* -------------- VALIDATE HOST GRP ADDR -------------- */
    if (addr_grp == NET_IPv4_ADDR_MULTICAST_ALL_HOSTS) {        /* If host grp addr = all-hosts addr, ...               */
       *p_err =  NET_IGMP_ERR_NONE;                             /* ... rtn (see Note #6b2).                             */
        return (DEF_OK);
    }

    if ((addr_grp < NET_IPv4_ADDR_MULTICAST_HOST_MIN) ||        /* If host grp addr NOT valid multicast (see Note #6a), */
        (addr_grp > NET_IPv4_ADDR_MULTICAST_HOST_MAX)) {
        *p_err =  NET_IGMP_ERR_INVALID_ADDR_GRP;                /* ... rtn err.                                         */
         return (DEF_FAIL);
    }

                                                                /* ---------------- SRCH HOST GRP LIST ---------------- */
    p_host_grp = NetIGMP_HostGrpSrch(if_nbr, addr_grp);
    if (p_host_grp != (NET_IGMP_HOST_GRP *)0) {                 /* If host grp found, ...                               */
        p_host_grp->RefCtr++;                                   /* ... inc ref ctr.                                     */
       *p_err =  NET_IGMP_ERR_NONE;
        return (DEF_OK);
    }

    p_host_grp = NetIGMP_HostGrpAdd(if_nbr, addr_grp, p_err);    /* Add new host grp into Host Grp List (see Note #8a).  */
    if (*p_err != NET_IGMP_ERR_NONE) {
         return (DEF_FAIL);
    }


                                                                /* --------------- ADVERTISE MEMBERSHIP --------------- */
    NetIGMP_TxMsgReport(p_host_grp, &err);                      /* See Note #9a1.                                       */

                                                                /* Cfg report timeout.                                  */
    timeout_tick       = (NET_TMR_TICK)(NET_IGMP_HOST_GRP_REPORT_DLY_JOIN_SEC * NET_TMR_TIME_TICK_PER_SEC);
    p_host_grp->TmrPtr =  NetTmr_Get((CPU_FNCT_PTR ) NetIGMP_HostGrpReportDlyTimeout,
                                     (void        *) p_host_grp,/* See Note #7a.                                        */
                                     (NET_TMR_TICK ) timeout_tick,
                                     (NET_ERR     *)&err);

    p_host_grp->State  = (err == NET_TMR_ERR_NONE) ? NET_IGMP_HOST_GRP_STATE_DELAYING
                                                   : NET_IGMP_HOST_GRP_STATE_IDLE;

   *p_err =  NET_IGMP_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                        NetIGMP_HostGrpLeave()
*
* Description : Leave a host group.
*
* Argument(s) : if_nbr      Interface number to leave host group.
*
*               addr_grp    IP address of host group to leave (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                                   - RETURNED BY NetIGMP_HostGrpLeaveHandler() : -
*                               NET_IGMP_ERR_NONE                   Host group successfully left.
*                               NET_IGMP_ERR_HOST_GRP_NOT_FOUND     Host group NOT found.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
*                                                                   ---- RETURNED BY Net_GlobalLockAcquire() : ----
*                               NET_ERR_FAULT_LOCK_ACQUIRE          Network access NOT acquired.
*
* Return(s)   : DEF_OK,   if host group successfully left.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIGMP_HostGrpLeave() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*                   See also 'NetIGMP_HostGrpLeaveHandler()  Note #2'.
*
*               (2) IP host group address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIGMP_HostGrpLeave (NET_IF_NBR      if_nbr,
                                   NET_IPv4_ADDR   addr_grp,
                                   NET_ERR        *p_err)
{
    CPU_BOOLEAN  host_grp_leave;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* --------------- VALIDATE RTN ERR PTR --------------- */
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(DEF_FAIL);
    }
#endif

                                                                /* Acquire net lock (see Note #1b).                     */
    Net_GlobalLockAcquire((void *)&NetIGMP_HostGrpLeave, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (DEF_FAIL);
    }
                                                                /* Leave host grp.                                      */
    host_grp_leave = NetIGMP_HostGrpLeaveHandler(if_nbr, addr_grp, p_err);

    Net_GlobalLockRelease();                                    /* Release net lock.                                    */


    return (host_grp_leave);
}


/*
*********************************************************************************************************
*                                     NetIGMP_HostGrpLeaveHandler()
*
* Description : (1) Leave a host group :
*
*                   (a) Search IGMP Host Group List for host group with corresponding address
*                           & interface number
*                   (b) If host group found, remove host group from IGMP Host Group List
*
*
* Argument(s) : if_nbr      Interface number to leave host group.
*
*               addr_grp    IP address of host group to leave (see Note #4).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE                   Host group successfully left.
*                               NET_IGMP_ERR_HOST_GRP_NOT_FOUND     Host group NOT found.
*                               NET_INIT_ERR_NOT_COMPLETED             Network initialization NOT complete.
*
* Return(s)   : DEF_OK,   if host group successfully left.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetIGMP_HostGrpLeave().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) NetIGMP_HostGrpLeaveHandler() is called by network protocol suite function(s) & MUST
*                   be called with the global network lock already acquired.
*
*                   See also 'NetIGMP_HostGrpLeave()  Note #1'.
*
*               (3) NetIGMP_HostGrpLeaveHandler() blocked until network initialization completes.
*
*               (4) IP host group address MUST be in host-order.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIGMP_HostGrpLeaveHandler (NET_IF_NBR      if_nbr,
                                          NET_IPv4_ADDR   addr_grp,
                                          NET_ERR        *p_err)
{
    NET_IGMP_HOST_GRP  *p_host_grp;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, exit (see Note #3).            */
       *p_err =  NET_INIT_ERR_NOT_COMPLETED;
        return (DEF_FAIL);
    }
#endif

                                                                /* ---------------- SRCH HOST GRP LIST ---------------- */
    p_host_grp = NetIGMP_HostGrpSrch(if_nbr, addr_grp);
    if (p_host_grp == (NET_IGMP_HOST_GRP *)0) {                 /* If host grp NOT found, ...                           */
       *p_err =  NET_IGMP_ERR_HOST_GRP_NOT_FOUND;               /* ... rtn err.                                         */
        return (DEF_FAIL);
    }

    p_host_grp->RefCtr--;                                        /* Dec ref ctr.                                         */
    if (p_host_grp->RefCtr < 1) {                                /* If last ref to host grp, ...                         */
        NetIGMP_HostGrpRemove(p_host_grp);                       /* ... remove host grp from Host Grp List.              */
    }


   *p_err =  NET_IGMP_ERR_NONE;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                       NetIGMP_IsGrpJoinedOnIF()
*
* Description : Check for joined host group on specified interface.
*
* Argument(s) : if_nbr      Interface number to search.
*
*               addr_grp    IP address of host group.
*
* Return(s)   : DEF_YES, if host group address joined on interface.
*
*               DEF_NO,  otherwise.
*
* Caller(s)   : NetIP_RxPktValidate().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetIGMP_IsGrpJoinedOnIF (NET_IF_NBR     if_nbr,
                                      NET_IPv4_ADDR  addr_grp)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    CPU_BOOLEAN         grp_joined;


    p_host_grp  =  NetIGMP_HostGrpSrch(if_nbr, addr_grp);
    grp_joined = (p_host_grp != (NET_IGMP_HOST_GRP *)0) ? DEF_YES : DEF_NO;

    return (grp_joined);
}


/*
*********************************************************************************************************
*                                             NetIGMP_Rx()
*
* Description : (1) Process received IGMP packets & update host group status :
*
*                   (a) Validate IGMP packet
*                   (b) Update   IGMP host group status
*                   (c) Free     IGMP packet
*                   (d) Update receive statistics
*
*               (2) Although IGMP data units are typically referred to as 'messages' (see RFC #1112,
*                   Appendix I), the term 'IGMP packet' (see RFC #1983, 'packet') is used for IGMP
*                   Receive until the packet is validated as an IGMP message.
*
*
* Argument(s) : p_buf        Pointer to network buffer that received IGMP packet.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE               IGMP packet successfully received & processed.
*
*                                                               ---- RETURNED BY NetIGMP_RxPktDiscard() : ----
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIP_RxPktDemuxDatagram().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetIGMP_Rx (NET_BUF  *p_buf,
                  NET_ERR  *p_err)
{
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR       *p_ctr;
#endif
    NET_BUF_HDR   *p_buf_hdr;
    NET_IGMP_HDR  *p_igmp_hdr;
    NET_IF_NBR     if_nbr;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIGMP_RxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.NullPtrCtr);
        return;
    }
#endif


    NET_CTR_STAT_INC(Net_StatCtrs.IGMP.RxMsgCtr);


                                                                /* -------------- VALIDATE RX'D IGMP PKT -------------- */
    p_buf_hdr = &p_buf->Hdr;
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NetIGMP_RxPktValidateBuf(p_buf_hdr, p_err);                 /* Validate rx'd buf.                                   */
    switch (*p_err) {
        case NET_IGMP_ERR_NONE:
             break;


        case NET_ERR_INVALID_PROTOCOL:
        case NET_BUF_ERR_INVALID_IX:
        default:
             NetIGMP_RxPktDiscard(p_buf, p_err);
             return;
    }
#endif
    p_igmp_hdr = (NET_IGMP_HDR *)&p_buf->DataPtr[p_buf_hdr->IGMP_MsgIx];
    if_nbr    =  p_buf_hdr->IF_Nbr;
    NetIGMP_RxPktValidate(p_buf, p_buf_hdr, p_igmp_hdr, p_err); /* Validate rx'd pkt.                                   */


                                                                /* ------------------ DEMUX IGMP MSG ------------------ */
    switch (*p_err) {
        case NET_IGMP_ERR_MSG_TYPE_QUERY:
             NetIGMP_RxMsgQuery(if_nbr, p_igmp_hdr);

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
             p_ctr = &Net_StatCtrs.IGMP.RxMsgQueryCtr;
#endif
             break;


        case NET_IGMP_ERR_MSG_TYPE_REPORT:
             NetIGMP_RxMsgReport(if_nbr, p_igmp_hdr);

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
             p_ctr = &Net_StatCtrs.IGMP.RxMsgReportCtr;
#endif
             break;


        case NET_IGMP_ERR_INVALID_VER:
        case NET_IGMP_ERR_INVALID_TYPE:
        case NET_IGMP_ERR_INVALID_LEN:
        case NET_IGMP_ERR_INVALID_CHK_SUM:
        case NET_IGMP_ERR_INVALID_ADDR_DEST:
        case NET_IGMP_ERR_INVALID_ADDR_GRP:
        default:
             NetIGMP_RxPktDiscard(p_buf, p_err);
             return;
    }


    NetIGMP_RxPktFree(p_buf);                                   /* ------------------- FREE IGMP PKT ------------------ */

                                                                /* ------------------ UPDATE RX STATS ----------------- */
    NET_CTR_STAT_INC(Net_StatCtrs.IGMP.RxMsgCompCtr);
    NET_CTR_STAT_INC(*p_ctr);


   *p_err = NET_IGMP_ERR_NONE;
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
*                                      NetIGMP_RxPktValidateBuf()
*
* Description : Validate received buffer header as IGMP protocol.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header that received IGMP packet.
*               ---------   Argument validated in NetIGMP_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE               Received buffer's IGMP header validated.
*                               NET_ERR_INVALID_PROTOCOL        Buffer's protocol type is NOT IGMP.
*                               NET_BUF_ERR_INVALID_IX          Invalid buffer index.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
static  void  NetIGMP_RxPktValidateBuf (NET_BUF_HDR  *p_buf_hdr,
                                        NET_ERR      *p_err)
{
    NET_IF_NBR   if_nbr;
    CPU_BOOLEAN  valid;
    NET_ERR      err;

                                                                /* --------------- VALIDATE IGMP BUF HDR -------------- */
    if (p_buf_hdr->ProtocolHdrType != NET_PROTOCOL_TYPE_IGMP) {
        if_nbr = p_buf_hdr->IF_Nbr;
        valid  = NetIF_IsValidHandler(if_nbr, &err);
        if (valid == DEF_YES) {
            NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxInvalidProtocolCtr);
        }
       *p_err = NET_ERR_INVALID_PROTOCOL;
        return;
    }

    if (p_buf_hdr->IGMP_MsgIx == NET_BUF_IX_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxInvalidBufIxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

   *p_err = NET_IGMP_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                        NetIGMP_RxPktValidate()
*
* Description : (1) Validate received IGMP packet :
*
*                   (a) Validate the received message's length                          See Note #4
*
*                   (b) (1) Validate the received packet's following IGMP header fields :
*
*                           (A) Version
*                           (B) Type
*                           (C) Checksum
*
*                       (2) Validation ignores the following ICMP header fields :
*
*                           (A) Host Group Address
*
*
* Argument(s) : p_buf       Pointer to network buffer that received IGMP packet.
*               -----       Argument checked   in NetIGMP_Rx().
*
*               p_buf_hdr   Pointer to network buffer header.
*               ---------   Argument validated in NetIGMP_Rx().
*
*               p_igmp_hdr  Pointer to received packet's IGMP header.
*               ----------  Argument validated in NetIGMP_Rx()/NetIGMP_RxPktValidateBuf().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_MSG_TYPE_QUERY     Received packet validated as IGMP Query  Message.
*                               NET_IGMP_ERR_MSG_TYPE_REPORT    Received packet validated as IGMP Report Message.
*
*                               NET_IGMP_ERR_INVALID_VER        Invalid IGMP version.
*                               NET_IGMP_ERR_INVALID_TYPE       Invalid IGMP message type.
*                               NET_IGMP_ERR_INVALID_LEN        Invalid IGMP message length.
*                               NET_IGMP_ERR_INVALID_CHK_SUM    Invalid IGMP check-sum.
*                               NET_IGMP_ERR_INVALID_ADDR_DEST  Invalid IGMP IP group address for query.
*                               NET_IGMP_ERR_INVALID_ADDR_GRP   IP destination address & IGMP header group
*                                                                   address NOT identical.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Rx().
*
* Note(s)     : (2) See 'net_igmp.h  IGMP HEADER' for IGMP header format.
*
*               (3) (a) (1) RFC #1112, Section 7.2 states that "an ICMP error message ... is never generated
*                           in response to a datagram destined to an IP host group".
*
*                       (2) RFC #1122, Section 3.2.2 reiterates that "an ICMP error message MUST NOT be sent
*                           as the result of receiving ... a datagram destined to an ... IP multicast address".
*
*                   (b) 'p_buf' NOT used to transmit ICMP error messages but is included for consistency.
*
*               (4) (a) RFC #1112, Appendix I, Section 'State Transition Diagram' states that "to be valid,
*                       the ... [received] message[s] must be at least 8 octets long".
*
*                   (b) Since IGMP message headers do NOT contain a message length field, the IGMP Message
*                       Length is assumed to be the remaining IP Datagram Length.
*********************************************************************************************************
*/

static  void  NetIGMP_RxPktValidate (NET_BUF       *p_buf,
                                     NET_BUF_HDR   *p_buf_hdr,
                                     NET_IGMP_HDR  *p_igmp_hdr,
                                     NET_ERR       *p_err)
{
    CPU_INT16U     igmp_msg_len;
    CPU_BOOLEAN    igmp_chk_sum_valid;
    CPU_INT08U     igmp_ver;
    CPU_INT08U     igmp_type;
    NET_IPv4_ADDR  igmp_grp_addr;
    NET_ERR        err;
    NET_ERR        err_rtn;


   (void)&p_buf;                                                /* Prevent 'variable unused' warning (see Note #3b).    */


                                                                /* ------------- VALIDATE IGMP RX MSG LEN ------------- */
    igmp_msg_len          = p_buf_hdr->IP_DatagramLen;          /* See Note #3b.                                        */
    p_buf_hdr->IGMP_MsgLen = igmp_msg_len;
    if (igmp_msg_len < NET_IGMP_MSG_SIZE_MIN) {                 /* If msg len < min msg len, rtn err (see Note #4a).    */
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxHdrMsgLenCtr);
       *p_err = NET_IGMP_ERR_INVALID_LEN;
        return;
    }


                                                                /* ---------------- VALIDATE IGMP VER ----------------- */
    igmp_ver   = p_igmp_hdr->Ver_Type & NET_IGMP_HDR_VER_MASK;  /* See 'net_igmp.h  IGMP HEADER  Note #2'.              */
    igmp_ver >>= NET_IGMP_HDR_VER_SHIFT;
    if (igmp_ver != NET_IGMP_HDR_VER) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxHdrVerCtr);
       *p_err = NET_IGMP_ERR_INVALID_VER;
        return;
    }


                                                                /* -------------- VALIDATE IGMP MSG TYPE -------------- */
    NET_UTIL_VAL_COPY_GET_NET_32(&igmp_grp_addr, &p_igmp_hdr->AddrGrp);
    igmp_type = p_igmp_hdr->Ver_Type & NET_IGMP_HDR_TYPE_MASK;

    switch (igmp_type) {
        case NET_IGMP_MSG_TYPE_QUERY:
             if (p_buf_hdr->IP_AddrDest != NET_IPv4_ADDR_MULTICAST_ALL_HOSTS) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxPktInvalidAddrDestCtr);
                *p_err = NET_IGMP_ERR_INVALID_ADDR_DEST;
                 return;
             }
             err_rtn = NET_IGMP_ERR_MSG_TYPE_QUERY;
             break;


        case NET_IGMP_MSG_TYPE_REPORT:
             if (p_buf_hdr->IP_AddrDest != igmp_grp_addr) {
                 NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxPktInvalidAddrDestCtr);
                *p_err = NET_IGMP_ERR_INVALID_ADDR_GRP;
                 return;
             }
             err_rtn = NET_IGMP_ERR_MSG_TYPE_REPORT;
             break;


        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxHdrTypeCtr);
            *p_err = NET_IGMP_ERR_INVALID_TYPE;
             return;
    }


                                                                /* -------------- VALIDATE IGMP CHK SUM --------------- */
    igmp_chk_sum_valid = NetUtil_16BitOnesCplChkSumHdrVerify((void     *) p_igmp_hdr,
                                                             (CPU_INT16U) p_buf_hdr->IGMP_MsgLen,
                                                             (NET_ERR  *)&err);
    if (igmp_chk_sum_valid != DEF_OK) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.RxHdrChkSumCtr);
       *p_err = NET_IGMP_ERR_INVALID_CHK_SUM;
        return;
    }


   *p_err = err_rtn;
}


/*
*********************************************************************************************************
*                                         NetIGMP_RxMsgQuery()
*
* Description : (1) Process IGMP received query :
*
*                   (a) Search host group in IDLE     state
*                   (b) Configure timer for host group
*                   (c) Set    host group in DELAYING state
*
*
* Argument(s) : if_nbr      Interface number the packet was received from.
*
*               p_igmp_hdr  Pointer to received packet's IGMP header.
*               ----------  Argument validated in NetIGMP_Rx().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Rx().
*
* Note(s)     : (2) See 'net_igmp.h  IGMP HEADER' for IGMP header format.
*
*               (3) See 'net_igmp.h  IGMP HOST GROUP STATES  Note #1' for state transitions.
*********************************************************************************************************
*/

static  void  NetIGMP_RxMsgQuery (NET_IF_NBR     if_nbr,
                                  NET_IGMP_HDR  *p_igmp_hdr)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    CPU_INT16U          timeout_sec;
    NET_TMR_TICK        timeout_tick;
    NET_ERR             err;


   (void)&p_igmp_hdr;                                           /* Prevent 'variable unused' compiler warning.          */

    p_host_grp = NetIGMP_HostGrpListHead;
    while (p_host_grp != (NET_IGMP_HOST_GRP *)0) {              /* Handle ALL host grp in host grp list.                */
        if (p_host_grp->IF_Nbr == if_nbr) {                     /* If host grp IF nbr is query IF nbr & ..              */
                                                                /* .. host grp state  is IDLE,          ..              */
            if (p_host_grp->State == NET_IGMP_HOST_GRP_STATE_IDLE) {
                                                                /* .. calc new rand timeout             ..              */
                NetIGMP_RandSeed   =  Math_RandSeed(NetIGMP_RandSeed);
                timeout_sec        =  NetIGMP_RandSeed % (CPU_INT16U)(NET_IGMP_HOST_GRP_REPORT_DLY_MAX_SEC + 1);
                timeout_tick       = (NET_TMR_TICK)(timeout_sec * NET_TMR_TIME_TICK_PER_SEC);

                                                                /* .. & set tmr.                                        */
                p_host_grp->TmrPtr =  NetTmr_Get((CPU_FNCT_PTR ) NetIGMP_HostGrpReportDlyTimeout,
                                                 (void        *) p_host_grp,
                                                 (NET_TMR_TICK ) timeout_tick,
                                                 (NET_ERR     *)&err);
                if (err != NET_TMR_ERR_NONE) {                  /* If err setting tmr,     ...                          */
                    NetIGMP_TxMsgReport(p_host_grp, &err);      /* ... tx report immed'ly; ...                          */

                } else {                                        /* ... else set host grp state to DELAYING.             */
                    p_host_grp->State = NET_IGMP_HOST_GRP_STATE_DELAYING;
                }
            }
        }

        p_host_grp = (NET_IGMP_HOST_GRP *)p_host_grp->NextPtr;
    }
}


/*
*********************************************************************************************************
*                                         NetIGMP_RxMsgReport()
*
* Description : (1) Process received IGMP report :
*
*                   (a) Search host group in DELAYING state & targeted by report
*                   (b) Free   timer
*                   (c) Set    host group in IDLE     state
*
*
* Argument(s) : if_nbr      Interface number the packet was received from.
*
*               p_igmp_hdr  Pointer to received packet's IGMP header.
*               ----------  Argument validated in NetIGMP_Rx().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Rx().
*
* Note(s)     : (2) See 'net_igmp.h  IGMP HEADER' for IGMP header format.
*
*               (3) See 'net_igmp.h  IGMP HOST GROUP STATES  Note #1' for state transitions.
*********************************************************************************************************
*/

static  void  NetIGMP_RxMsgReport (NET_IF_NBR     if_nbr,
                                   NET_IGMP_HDR  *p_igmp_hdr)
{
    NET_IPv4_ADDR       addr_grp;
    NET_IGMP_HOST_GRP  *p_host_grp;


    NET_UTIL_VAL_COPY_GET_NET_32(&addr_grp, &p_igmp_hdr->AddrGrp);

    p_host_grp = NetIGMP_HostGrpSrch(if_nbr, addr_grp);

    if (p_host_grp != (NET_IGMP_HOST_GRP *)0) {                      /* If host grp            ...                       */
        if (p_host_grp->State == NET_IGMP_HOST_GRP_STATE_DELAYING) { /* ... in DELAYING state, ...                       */
            if (p_host_grp->TmrPtr != (NET_TMR *)0) {
                NetTmr_Free(p_host_grp->TmrPtr);                     /* ... free tmr if avail  ...                       */
                p_host_grp->TmrPtr  = (NET_TMR *)0;
            }

            p_host_grp->State = NET_IGMP_HOST_GRP_STATE_IDLE;        /* ... & set to IDLE state.                         */
        }
    }
}


/*
*********************************************************************************************************
*                                          NetIGMP_RxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_RxPktFree (NET_BUF  *p_buf)
{
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)0);
}


/*
*********************************************************************************************************
*                                        NetIGMP_RxPktDiscard()
*
* Description : On any IGMP receive error(s), discard IGMP packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_RxPktDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IGMP.RxPktDiscardedCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)p_ctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                            NetIGMP_TxMsg()
*
* Description : (1) Prepare & transmit an IGMP Report or IGMP Query :
*
*                   (a) Validate IGMP Message Type          See 'net_igmp.h  IGMP MESSAGE TYPE DEFINES'
*                                                             & 'net_icmp.c  Note #2c'
*                   (b) Get buffer for IGMP Message :
*                       (1) Initialize IGMP Message buffer controls
*
*                   (c) Prepare IGMP Message :
*
*                       (1) Type                            See  Note #1a
*                       (2) Version                         See 'net_igmp.h  IGMP HEADER DEFINES'
*                       (3) Group address
*                           (A) RFC #1112, Appendix I, Section 'Informal Protocl Description' states
*                               that "a report is sent with" :
*
*                               (1) "an IP destination address equal to the host group address being
*                                       reported" ...
*                               (2) "and with an IP time-to-live of 1, so that other members of the
*                                    same group can overhear the report."
*
*                       (4) Check sum
*
*                   (d) Transmit IGMP Message
*                   (e) Free     IGMP Message buffer
*                   (f) Update transmit statistics
*
*
* Argument(s) : if_nbr      Interface number to transmit IGMP message.
*
*               addr_src    Source     IP address (see Note #2).
*               --------    Argument validated in NetIGMP_TxMsgReport().
*
*               addr_grp    Host group IP address (see Note #2).
*
*               type        IGMP message type (see Note #1c1) :
*
*                               NET_IGMP_MSG_TYPE_QUERY         IGMP Message Query  type
*                               NET_IGMP_MSG_TYPE_REPORT        IGMP Message Report type
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE               IGMP message successfully transmitted.
*
*                                                               -- RETURNED BY NetIGMP_TxPktDiscard() : ---
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
*                                                               --------- RETURNED BY NetIP_Tx() : --------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_TxMsgReport().
*
* Note(s)     : (2) IP addresses MUST be in host-order.
*
*               (3) Assumes network buffer's protocol header size is large enough to accommodate IGMP header
*                   size (see 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #1a2').
*
*               (4) Some buffer controls were previously initialized in NetBuf_Get() when the buffer was
*                   allocated.  These buffer controls do NOT need to be re-initialized but are shown for
*                   completeness.
*
*               (5) (a) IGMP header Check-Sum MUST be calculated AFTER the entire IGMP header has been
*                       prepared.  In addition, ALL multi-octet words are converted from host-order to
*                       network-order since "the sum of 16-bit integers can be computed in either byte
*                       order" [RFC #1071, Section 2.(B)].
*
*                   (b) IGMP header Check-Sum field MUST be cleared to '0' BEFORE the IGMP header Check-Sum
*                       is calculated (see RFC #1112, Appendix I, Section 'Checksum').
*
*                   (c) The IGMP header Check-Sum field is returned in network-order & MUST NOT be re-converted
*                       back to host-order (see 'net_util.c  NetUtil_16BitOnesCplChkSumHdrCalc()  Note #3b').
*
*               (6) Network buffer already freed by lower layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIGMP_TxMsg (NET_IF_NBR      if_nbr,
                             NET_IPv4_ADDR   addr_src,
                             NET_IPv4_ADDR   addr_grp,
                             CPU_INT08U      type,
                             NET_ERR        *p_err)
{
    NET_BUF       *pmsg_buf;
    NET_BUF_HDR   *pmsg_buf_hdr;
    NET_IGMP_HDR  *p_igmp_hdr;
    CPU_INT16U     msg_ix;
    CPU_INT16U     msg_ix_offset;
    CPU_INT08U     igmp_ver;
    CPU_INT08U     igmp_type;
    CPU_INT16U     igmp_chk_sum;
    NET_ERR        err;


                                                                /* -------------- VALIDATE IGMP MSG TYPE -------------- */
    switch (type) {
        case NET_IGMP_MSG_TYPE_REPORT:
             break;


        case NET_IGMP_MSG_TYPE_QUERY:                           /* See 'net_icmp.c  Note #2c'.                          */
        default:
             NetIGMP_TxPktDiscard((NET_BUF *)0,
                                  (NET_ERR *)p_err);
             return;
    }


                                                                /* ------------------ GET IGMP TX BUF ----------------- */
    msg_ix = 0;
    NetIGMP_TxIxDataGet(if_nbr, NET_IGMP_HDR_SIZE, &msg_ix, &err);
    if (err != NET_IGMP_ERR_NONE) {
        return;
    }

    pmsg_buf = NetBuf_Get(if_nbr,
                          NET_TRANSACTION_TX,
                          NET_IGMP_MSG_LEN_DATA,
                          msg_ix,
                         &msg_ix_offset,
                          NET_BUF_FLAG_NONE,
                         &err);
    if (err != NET_BUF_ERR_NONE) {
        NetIGMP_TxPktDiscard(pmsg_buf, p_err);
        return;
    }

    msg_ix += msg_ix_offset;

                                                                /* Init msg buf ctrls.                                  */
    pmsg_buf_hdr                        = &pmsg_buf->Hdr;
    pmsg_buf_hdr->IGMP_MsgIx            = (CPU_INT16U  )msg_ix;
    pmsg_buf_hdr->IGMP_MsgLen           = (CPU_INT16U  )NET_IGMP_HDR_SIZE;
    pmsg_buf_hdr->TotLen                = (NET_BUF_SIZE)pmsg_buf_hdr->IGMP_MsgLen;
    pmsg_buf_hdr->ProtocolHdrType       =  NET_PROTOCOL_TYPE_IGMP;
    pmsg_buf_hdr->ProtocolHdrTypeNetSub =  NET_PROTOCOL_TYPE_IGMP;
#if 0                                                           /* Init'd in NetBuf_Get() [see Note #4].                */
    p_buf_hdr->DataIx                    =  NET_BUF_IX_NONE;
    p_buf_hdr->DataLen                   =  0u;
#endif


                                                                /* ----------------- PREPARE IGMP MSG ----------------- */
    p_igmp_hdr = (NET_IGMP_HDR *)&pmsg_buf->DataPtr[pmsg_buf_hdr->IGMP_MsgIx];

                                                                /* Prepare IGMP ver/type (see Note #1c1).               */
    igmp_ver              = NET_IGMP_HDR_VER;
    igmp_ver            <<= NET_IGMP_HDR_VER_SHIFT;

    igmp_type             = type;
    igmp_type            &= NET_IGMP_HDR_TYPE_MASK;

    p_igmp_hdr->Ver_Type   = igmp_ver | igmp_type;


    Mem_Clr(&p_igmp_hdr->Unused, 1u);                           /* Clr unused octets.                                   */

                                                                /* Prepare host grp addr (see Note #1c3).               */
    NET_UTIL_VAL_COPY_SET_NET_32(&p_igmp_hdr->AddrGrp, &addr_grp);

                                                                /* Prepare IGMP chk sum  (see Note #5).                 */
    NET_UTIL_VAL_SET_NET_16(&p_igmp_hdr->ChkSum, 0x0000u);      /* Clr  chk sum (see Note #5b).                         */
                                                                /* Calc chk sum.                                        */
    igmp_chk_sum = NetUtil_16BitOnesCplChkSumHdrCalc((void     *)p_igmp_hdr,
                                                     (CPU_INT16U)NET_IGMP_HDR_SIZE,
                                                     (NET_ERR  *)p_err);
    if (*p_err != NET_UTIL_ERR_NONE) {
         NetIGMP_TxPktDiscard(pmsg_buf, p_err);
         return;
    }

    NET_UTIL_VAL_COPY_16(&p_igmp_hdr->ChkSum, &igmp_chk_sum);   /* Copy chk sum in net order (see Note #5c).            */



                                                                /* ------------------- TX IGMP MSG -------------------- */
    NetIPv4_Tx(pmsg_buf,
               addr_src,
               addr_grp,
               NET_IPv4_TOS_DFLT,
               NET_IPv4_TTL_MULTICAST_IGMP,
               NET_IPv4_FLAG_NONE,
               DEF_NULL,
               p_err);


                                                                /* --------- FREE IGMP MSG / UPDATE TX STATS ---------- */
    switch (*p_err) {
        case NET_IPv4_ERR_NONE:
             NetIGMP_TxPktFree(pmsg_buf);
             NET_CTR_STAT_INC(Net_StatCtrs.IGMP.TxMsgCtr);
             break;


        case NET_ERR_TX:
        case NET_ERR_IF_LINK_DOWN:
        case NET_ERR_IF_LOOPBACK_DIS:
                                                                /* See Note #6.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.TxPktDiscardedCtr);
                                                                /* Rtn err from NetIP_Tx().                             */
             return;


        case NET_IPv4_ERR_TX_PKT:
                                                                /* See Note #6.                                         */
             NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.TxPktDiscardedCtr);
            *p_err = NET_ERR_TX;
             return;


        default:
             NetIGMP_TxPktDiscard(pmsg_buf, p_err);
             return;
    }


   *p_err = NET_IGMP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetIGMP_TxMsgReport()
*
* Description : (1) Prepare & transmit an IGMP Report :
*
*                   (a) Prepare IGMP Report Message :
*                       (1) Get host group's network interface number
*                       (2) Get host group's source IP address
*                       (3) Get host group's        IP address
*
*                   (b) Update transmit statistics
*
*
* Argument(s) : p_host_grp  Pointer to host group to transmit report.
*               ----------  Argument checked in NetIGMP_HostGrpJoinHandler(),
*                                               NetIGMP_HostGrpReportDlyTimeout(),
*                                               NetIGMP_RxMsgQuery().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE               IGMP Report Message successfully transmitted.
*                               NET_IGMP_ERR_INVALID_ADDR_SRC   Invalid/unavailable source address.
*
*                                                               ------- RETURNED BY NetIGMP_TxMsg() : -------
*                               NET_ERR_IF_LOOPBACK_DIS         Loopback interface disabled.
*                               NET_ERR_IF_LINK_DOWN            Network  interface link state down (i.e.
*                                                                   NOT available for receive or transmit).
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpJoinHandler(),
*               NetIGMP_HostGrpReportDlyTimeout(),
*               NetIGMP_RxMsgQuery().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_TxMsgReport (NET_IGMP_HOST_GRP  *p_host_grp,
                                   NET_ERR            *p_err)
{
    NET_IF_NBR        if_nbr;
    NET_IP_ADDRS_QTY  addr_ip_tbl_qty;
    NET_IPv4_ADDR     addr_ip_tbl[NET_IPv4_CFG_IF_MAX_NBR_ADDR];
    NET_IPv4_ADDR     addr_src;
    NET_IPv4_ADDR     addr_grp;
    NET_ERR           err;


                                                                /* ------------- PREPARE IGMP REPORT MSG -------------- */
    if_nbr          = p_host_grp->IF_Nbr;                       /* Get IGMP host grp IF nbr.                            */
    addr_ip_tbl_qty = sizeof(addr_ip_tbl) / sizeof(NET_IPv4_ADDR);
   (void)NetIPv4_GetAddrHostHandler((NET_IF_NBR        ) if_nbr,
                                    (NET_IPv4_ADDR    *)&addr_ip_tbl[0],
                                    (NET_IP_ADDRS_QTY *)&addr_ip_tbl_qty,
                                    (NET_ERR          *)&err);
    if (err != NET_IPv4_ERR_NONE) {
       *p_err = NET_IGMP_ERR_INVALID_ADDR_SRC;
        return;
    }

    addr_src = addr_ip_tbl[0];                                  /* Get IGMP host grp src addr.                          */
    addr_grp = p_host_grp->AddrGrp;                             /* Get IGMP host grp     addr.                          */


                                                                /* ---------------- TX IGMP REPORT MSG ---------------- */
    NetIGMP_TxMsg((NET_IF_NBR   )if_nbr,
                  (NET_IPv4_ADDR)addr_src,
                  (NET_IPv4_ADDR)addr_grp,
                  (CPU_INT08U   )NET_IGMP_MSG_TYPE_REPORT,
                  (NET_ERR     *)p_err);
    if (*p_err != NET_IGMP_ERR_NONE) {
         return;
    }


                                                                /* ------------------ UPDATE TX STATS ----------------- */
    NET_CTR_STAT_INC(Net_StatCtrs.IGMP.TxMsgReportCtr);


   *p_err = NET_IGMP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetIGMP_TxIxDataGet()
*
* Description : Get the offset of a buffer at which the UDP data can be written.
*
* Argument(s) : if_nbr      Network interface number to transmit data.
*
*               data_len    Length of the IGMP payload.
*
*               p_ix        Pointer to the current protocol index.
*               ----        Argument validated in NetIGMP_TxMsg().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ICMPv4_ERR_NONE     No errors.
*
*                               -Returned by NetIF_MTU_GetProtocol()-
*                               See NetIF_MTU_GetProtocol() for additional return error codes.
*
*                               -Returned by NetIPv4_GetTxDataIx()-
*                               See NetIPv4_GetTxDataIx() for additional return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_TxMsg().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_TxIxDataGet (NET_IF_NBR   if_nbr,
                                   CPU_INT16U   data_len,
                                   CPU_INT16U  *p_ix,
                                   NET_ERR     *p_err)
{
    NET_MTU  mtu;


    mtu   = NetIF_MTU_GetProtocol(if_nbr, NET_PROTOCOL_TYPE_IGMP, NET_IF_FLAG_NONE, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        return;
    }


    NetIPv4_TxIxDataGet(if_nbr,
                        data_len,
                        mtu,
                        p_ix,
                        p_err);
    if (*p_err != NET_IPv4_ERR_NONE) {
         return;
    }

   *p_err = NET_IGMP_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetIGMP_TxPktFree()
*
* Description : Free network buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_TxMsg().
*
* Note(s)     : (1) (a) Although IGMP Transmit initially requests the network buffer for transmit,
*                       the IGMP layer does NOT maintain a reference to the buffer.
*
*                   (b) Also, since the network interface layer frees ALL unreferenced buffers after
*                       successful transmission, the IGMP layer MUST not free the transmit buffer.
*********************************************************************************************************
*/

static  void  NetIGMP_TxPktFree (NET_BUF  *p_buf)
{
   (void)&p_buf;                                                 /* Prevent 'variable unused' warning (see Note #1).     */
}


/*
*********************************************************************************************************
*                                        NetIGMP_TxPktDiscard()
*
* Description : On any IGMP transmit packet error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_TxMsg().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_TxPktDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IGMP.TxPktDiscardedCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf((NET_BUF *)p_buf,
                        (NET_CTR *)p_ctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                         NetIGMP_HostGrpSrch()
*
* Description : Search IGMP Host Group List for host group with specific address & interface number.
*
*               (1) IGMP host groups are linked to form an IGMP Host Group List.
*
*                   (a) In the diagram below, ... :
*
*                       (1) The horizontal row represents the list of IGMP host groups.
*
*                       (2) 'NetIGMP_HostGrpListHead' points to the head of the IGMP Host Group List.
*
*                       (3) IGMP host groups' 'PrevPtr' & 'NextPtr' doubly-link each host group to form the
*                           IGMP Host Group List.
*
*                   (b) (1) For any IGMP Host Group List lookup, all IGMP host groups are searched in order
*                           to find the host group with the appropriate host group address on the specified
*                           interface.
*
*                       (2) To expedite faster IGMP Host Group List lookup :
*
*                           (A) (1) (a) IGMP host groups are added at;            ...
*                                   (b) IGMP host groups are searched starting at ...
*                               (2) ... the head of the IGMP Host Group List.
*
*                           (B) As IGMP host groups are added into the list, older IGMP host groups migrate
*                               to the tail of the IGMP Host Group List.   Once an IGMP host group is left,
*                               it is removed from the IGMP Host Group List.
*
*
*                                        |                                               |
*                                        |<---------- List of IGMP Host Groups --------->|
*                                        |                (see Note #1a1)                |
*
*                                 New IGMP host groups                      Oldest IGMP host group
*                                   inserted at head                        in IGMP Host Group List
*                                  (see Note #1b2A2)                           (see Note #1b2B)
*
*                                           |                 NextPtr                 |
*                                           |             (see Note #1a3)             |
*                                           v                    |                    v
*                                                                |
*                   Head of IGMP         -------       -------   v   -------       -------
*                  Host Group List  ---->|     |------>|     |------>|     |------>|     |
*                                        |     |       |     |       |     |       |     |        Tail of IGMP
*                  (see Note #1a2)       |     |<------|     |<------|     |<------|     |<----  Host Group List
*                                        -------       -------   ^   -------       -------
*                                                                |
*                                                                |
*                                                             PrevPtr
*                                                         (see Note #1a3)
*
*
* Argument(s) : if_nbr      Interface number to search for host group.
*
*               addr_grp    IP address of host group to search (see Note #2).
*
* Return(s)   : Pointer to IGMP host group with specific IP group address & interface, if found.
*
*               Pointer to NULL,                                                       otherwise.
*
* Caller(s)   : NetIGMP_HostGrpJoinHandler(),
*               NetIGMP_HostGrpLeaveHandler(),
*               NetIGMP_IsGrpJoinedOnIF(),
*               NetIGMP_RxMsgReport().
*
* Note(s)     : (2) IP host group address MUST be in host-order.
*********************************************************************************************************
*/

static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpSrch (NET_IF_NBR     if_nbr,
                                                 NET_IPv4_ADDR  addr_grp)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    NET_IGMP_HOST_GRP  *p_host_grp_next;
    CPU_BOOLEAN         found;


    p_host_grp = NetIGMP_HostGrpListHead;                       /* Start @ IGMP Host Grp List head (see Note #1b2A1b).  */
    found     = DEF_NO;

    while ((p_host_grp != (NET_IGMP_HOST_GRP *)0) &&            /* Srch IGMP Host Grp List ...                          */
           (found     ==  DEF_NO)) {                            /* ... until host grp found.                            */

        p_host_grp_next = (NET_IGMP_HOST_GRP *) p_host_grp->NextPtr;

        found = ((p_host_grp->IF_Nbr  == if_nbr) &&             /* Cmp IF nbr & grp addr.                               */
                 (p_host_grp->AddrGrp == addr_grp)) ? DEF_YES : DEF_NO;

        if (found != DEF_YES) {                                 /* If NOT found, adv to next IGMP host grp.             */
            p_host_grp = p_host_grp_next;
        }
    }

    return (p_host_grp);
}


/*
*********************************************************************************************************
*                                         NetIGMP_HostGrpAdd()
*
* Description : (1) Add a host group to the IGMP Host Group List :
*
*                   (a) Get a     host group from      host group pool
*                   (b) Configure host group
*                   (c) Insert    host group into IGMP Host Group List
*                   (d) Configure interface for multicast address
*
*
* Argument(s) : if_nbr      Interface number to add host group.
*               ------      Argument checked in NetIGMP_HostGrpJoinHandler().
*
*               addr_grp    IP address of host group to add.
*               --------    Argument checked in NetIGMP_HostGrpJoinHandler().
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE                   Host group succesfully added.
*
*                                                                   --- RETURNED BY NetIGMP_HostGrpGet() : ---
*                               NET_IGMP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*                               NET_IGMP_ERR_HOST_GRP_INVALID_TYPE  Host group is NOT a valid host group type.
*
* Return(s)   : Pointer to host group, if NO error(s).
*
*               Pointer to NULL,       otherwise.
*
* Caller(s)   : NetIGMP_HostGrpJoinHandler().
*
* Note(s)     : (2) IP host group address MUST be in host-order.
*********************************************************************************************************
*/

static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpAdd (NET_IF_NBR      if_nbr,
                                                NET_IPv4_ADDR   addr_grp,
                                                NET_ERR        *p_err)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    NET_PROTOCOL_TYPE   addr_protocol_type;
    NET_IPv4_ADDR       addr_grp_net;
    CPU_INT08U         *p_addr_protocol;
    CPU_INT08U          addr_protocol_len;
    NET_ERR             err;


    p_host_grp = NetIGMP_HostGrpGet(p_err);                     /* ------------------- GET HOST GRP ------------------- */
    if (p_host_grp == (NET_IGMP_HOST_GRP *)0) {
        return ((NET_IGMP_HOST_GRP *)0);                        /* Rtn err from NetIGMP_HostGrpGet().                   */
    }


                                                                /* ------------------- CFG HOST GRP ------------------- */
    p_host_grp->AddrGrp =  addr_grp;
    p_host_grp->IF_Nbr  =  if_nbr;
    p_host_grp->RefCtr  =  1u;
                                                                /* Set host grp state.                                  */
                                                                /* See 'net_igmp.h  IGMP HOST GROUP STATES  Note #2'.   */
    p_host_grp->State   = (addr_grp == NET_IPv4_ADDR_MULTICAST_ALL_HOSTS) ? NET_IGMP_HOST_GRP_STATE_STATIC
                                                                         : NET_IGMP_HOST_GRP_STATE_IDLE;


                                                                /* -------- INSERT HOST GRP INTO HOST GRP LIST -------- */
    NetIGMP_HostGrpInsert(p_host_grp);


                                                                /* ------------ CFG IF FOR MULTICAST ADDR ------------- */
    addr_grp_net       =  NET_UTIL_HOST_TO_NET_32(addr_grp);
    addr_protocol_type =  NET_PROTOCOL_TYPE_IP_V4;
    p_addr_protocol    = (CPU_INT08U *)&addr_grp_net;
    addr_protocol_len  =  sizeof(addr_grp_net);

    NetIF_AddrMulticastAdd(if_nbr,
                           p_addr_protocol,
                           addr_protocol_len,
                           addr_protocol_type,
                          &err);


   *p_err =  NET_IGMP_ERR_NONE;

    return (p_host_grp);
}


/*
*********************************************************************************************************
*                                        NetIGMP_HostGrpRemove()
*
* Description : Remove a host group from the IGMP Host Group List :
*
*                   (a) Remove host group from IGMP Host Group List
*                   (b) Free   host group back to   host group pool
*                   (c) Remove multicast address from interface
*
*
* Argument(s) : p_host_grp  Pointer to a host group.
*               ----------  Argument checked in NetIGMP_HostGrpLeaveHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpLeaveHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_HostGrpRemove (NET_IGMP_HOST_GRP  *p_host_grp)
{
    NET_IF_NBR          if_nbr;
    NET_PROTOCOL_TYPE   addr_protocol_type;
    NET_IPv4_ADDR       addr_grp;
    NET_IPv4_ADDR       addr_grp_net;
    CPU_INT08U         *p_addr_protocol;
    CPU_INT08U          addr_protocol_len;
    NET_ERR             err;


    if_nbr   = p_host_grp->IF_Nbr;
    addr_grp = p_host_grp->AddrGrp;

    NetIGMP_HostGrpUnlink(p_host_grp);                          /* -------- REMOVE HOST GRP FROM HOST GRP LIST -------- */

    NetIGMP_HostGrpFree(p_host_grp);                            /* ------------------ FREE HOST GRP ------------------- */

                                                                /* ---------- REMOVE MULTICAST ADDR FROM IF ----------- */
    addr_grp_net       =  NET_UTIL_HOST_TO_NET_32(addr_grp);
    addr_protocol_type =  NET_PROTOCOL_TYPE_IP_V4;
   p_addr_protocol      = (CPU_INT08U *)&addr_grp_net;
    addr_protocol_len  =  sizeof(addr_grp_net);

    NetIF_AddrMulticastRemove(if_nbr,
                              p_addr_protocol,
                              addr_protocol_len,
                              addr_protocol_type,
                             &err);
}


/*
*********************************************************************************************************
*                                        NetIGMP_HostGrpInsert()
*
* Description : Insert a host group into the IGMP Host Group List.
*
* Argument(s) : p_host_grp  Pointer to a host group.
*               ----------  Argument checked in NetIGMP_HostGrpAdd().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpAdd().
*
* Note(s)     : (1) See 'NetIGMP_HostGrpSrch()  Note #1b2A1a'.
*********************************************************************************************************
*/

static  void  NetIGMP_HostGrpInsert (NET_IGMP_HOST_GRP  *p_host_grp)
{
                                                                        /* ---------- CFG IGMP HOST GRP PTRS ---------- */
    p_host_grp->PrevPtr = (NET_IGMP_HOST_GRP *)0;
    p_host_grp->NextPtr = (NET_IGMP_HOST_GRP *)NetIGMP_HostGrpListHead;

                                                                        /* ------ INSERT IGMP HOST GRP INTO LIST ------ */
    if (NetIGMP_HostGrpListHead != (NET_IGMP_HOST_GRP *)0) {            /* If list NOT empty, insert before head.       */
        NetIGMP_HostGrpListHead->PrevPtr = p_host_grp;
    }

    NetIGMP_HostGrpListHead = p_host_grp;                                /* Insert host grp @ list head (see Note #1).   */
}


/*
*********************************************************************************************************
*                                        NetIGMP_HostGrpUnlink()
*
* Description : Unlink a host group from the IGMP Host Group List.
*
* Argument(s) : p_host_grp  Pointer to a host group.
*               ----------  Argument checked in NetIGMP_HostGrpRemove()
*                                                 by NetIGMP_HostGrpLeaveHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpRemove().
*
* Note(s)     : (1) Since NetIGMP_HostGrpUnlink() called ONLY to remove & then re-link or free host
*                   groups, it is NOT necessary to clear the entry's previous & next pointers.  However,
*                   pointers cleared to NULL shown for correctness & completeness.
*********************************************************************************************************
*/

static  void  NetIGMP_HostGrpUnlink (NET_IGMP_HOST_GRP  *p_host_grp)
{
    NET_IGMP_HOST_GRP  *p_host_grp_prev;
    NET_IGMP_HOST_GRP  *p_host_grp_next;

                                                                        /* ------ UNLINK IGMP HOST GRP FROM LIST ------ */
    p_host_grp_prev = p_host_grp->PrevPtr;
    p_host_grp_next = p_host_grp->NextPtr;
                                                                        /* Point prev host grp to next host grp.        */
    if (p_host_grp_prev != (NET_IGMP_HOST_GRP *)0) {
        p_host_grp_prev->NextPtr = p_host_grp_next;
    } else {
        NetIGMP_HostGrpListHead = p_host_grp_next;
    }
                                                                        /* Point next host grp to prev host grp.        */
    if (p_host_grp_next != (NET_IGMP_HOST_GRP *)0) {
        p_host_grp_next->PrevPtr = p_host_grp_prev;
    }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                             /* Clr host grp ptrs (see Note #1).             */
    p_host_grp->PrevPtr = (NET_IGMP_HOST_GRP *)0;
    p_host_grp->NextPtr = (NET_IGMP_HOST_GRP *)0;
#endif
}


/*
*********************************************************************************************************
*                                   NetIGMP_HostGrpReportDlyTimeout()
*
* Description : Transmit an IGMP report on IGMP Query timeout.
*
* Argument(s) : p_host_grp_timeout  Pointer to a host group (see Note #1b).
*
* Return(s)   : none.
*
* Caller(s)   : Referenced in NetIGMP_HostGrpJoinHandler(),
*                             NetIGMP_HostGrpReportDlyTimeout(),
*                             NetIGMP_RxMsgQuery().
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
*                   (b) Therefore, to correctly convert 'void' pointer objects back to appropriate
*                       network object pointer objects, network timer callback functions MUST :
*
*                       (1) Be defined as 'CPU_FNCT_PTR' type (i.e. '[(void) (void *)]'); &       ...
*                       (2) Explicitly cast 'void' pointer arguments to specific object pointers; ...
*                           (A) in this case, a 'NET_IGMP_HOST_GRP' pointer.
*
*                   See also 'net_tmr.c  NetTmr_Get()  Note #3'.
*
*               (2) This function is a network timer callback function :
*
*                   (a) Clear the timer pointer,
*                   (b) but do NOT re-free the timer.
*
*               (3) In case of a transmit error :
*
*                   (a) Configure a timer to attempt retransmission of the IGMP report, if the error
*                       is transitory.
*
*                       See also 'net_igmp.h  IGMP REPORT DEFINES  Note #2'.
*
*                   (b) Revert to 'IDLE' state, if the error is permanent.
*********************************************************************************************************
*/

static  void  NetIGMP_HostGrpReportDlyTimeout (void  *p_host_grp_timeout)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    NET_TMR_TICK        timeout_tick;
    NET_ERR             err;


    p_host_grp = (NET_IGMP_HOST_GRP *)p_host_grp_timeout;       /* See Note #1b2A.                                      */

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* -------------- VALIDATE IGMP HOST GRP -------------- */
    if (p_host_grp == (NET_IGMP_HOST_GRP *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.NullPtrCtr);
        return;
    }
#endif

    p_host_grp->TmrPtr = (NET_TMR *)0;                          /* Clear tmr (see Note #2).                             */


                                                                /* ------------------ TX IGMP REPORT ------------------ */
    NetIGMP_TxMsgReport(p_host_grp, &err);
    switch (err) {
        case NET_IGMP_ERR_NONE:                                 /* If NO err, ...                                       */
             p_host_grp->State = NET_IGMP_HOST_GRP_STATE_IDLE;  /* ... set state to 'IDLE'.                             */
             break;


        case NET_ERR_TX:                                        /* If tx err, ...                                       */
        case NET_ERR_IF_LINK_DOWN:
                                                                /* ... cfg new tmr (see Note #3a).                      */
             timeout_tick       = (NET_TMR_TICK)(NET_IGMP_HOST_GRP_REPORT_DLY_RETRY_SEC * NET_TMR_TIME_TICK_PER_SEC);
             p_host_grp->TmrPtr =  NetTmr_Get((CPU_FNCT_PTR )&NetIGMP_HostGrpReportDlyTimeout,
                                              (void        *) p_host_grp,
                                              (NET_TMR_TICK ) timeout_tick,
                                              (NET_ERR     *)&err);

             p_host_grp->State  = (err == NET_TMR_ERR_NONE) ? NET_IGMP_HOST_GRP_STATE_DELAYING
                                                            : NET_IGMP_HOST_GRP_STATE_IDLE;
             break;


        case NET_IGMP_ERR_INVALID_ADDR_SRC:
        default:                                                /* On all other errs, ...                               */
             p_host_grp->State = NET_IGMP_HOST_GRP_STATE_IDLE;  /* ... set state to 'IDLE'.                             */
             break;
    }
}


/*
*********************************************************************************************************
*                                         NetIGMP_HostGrpGet()
*
* Description : (1) Allocate & initialize a host group :
*
*                   (a) Get a host group
*                   (b) Validate   host group
*                   (c) Initialize host group
*                   (d) Update host group pool statistics
*                   (e) Return pointer to host group
*                         OR
*                       Null pointer & error code, on failure
*
*               (2) The host group pool is implemented as a stack :
*
*                   (a) 'NetIGMP_HostGrpPoolPtr' points to the head of the host group pool.
*
*                   (b) Host groups' 'NextPtr's link each host group to form the host group pool stack.
*
*                   (c) Host groups are inserted & removed at the head of    the host group pool stack.
*
*
*                                       Host groups are
*                                     inserted & removed
*                                        at the head
*                                       (see Note #2c)
*
*                                             |                 NextPtr
*                                             |             (see Note #2b)
*                                             v                    |
*                                                                  |
*                                          -------       -------   v   -------       -------
*                          Host group ---->|     |------>|     |------>|     |------>|     |
*                            Pointer       |     |       |     |       |     |       |     |
*                                          |     |       |     |       |     |       |     |
*                         (see Note #2a)   -------       -------       -------       -------
*
*                                          |                                               |
*                                          |<--------- Pool of Free host groups ---------->|
*                                          |                (see Note #2)                  |
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IGMP_ERR_NONE                   Host group successfully allocated &
*                                                                       initialized.
*                               NET_IGMP_ERR_HOST_GRP_NONE_AVAIL    NO available host group to allocate.
*                               NET_IGMP_ERR_HOST_GRP_INVALID_TYPE  Host group is NOT a valid host group type.
*
* Return(s)   : Pointer to host group, if NO error(s).
*
*               Pointer to NULL,       otherwise.
*
* Caller(s)   : NetIGMP_HostGrpAdd().
*
* Note(s)     : (3) (a) Host group pool is accessed by 'NetIGMP_HostGrpPoolPtr' during execution of
*
*                       (1) NetIGMP_Init()
*                       (2) NetIGMP_HostGrpGet()
*                       (3) NetIGMP_HostGrpFree()
*
*                   (b) Since the primary tasks of the network protocol suite are prevented from running
*                       concurrently (see 'net.h  Note #3'), it is NOT necessary to protect the shared
*                       resources of the host group pool since no asynchronous access from other network
*                       tasks is possible.
*********************************************************************************************************
*/

static  NET_IGMP_HOST_GRP  *NetIGMP_HostGrpGet (NET_ERR  *p_err)
{
    NET_IGMP_HOST_GRP  *p_host_grp;
    NET_ERR             err;


                                                                /* ------------------- GET HOST GRP ------------------- */
    if (NetIGMP_HostGrpPoolPtr != (NET_IGMP_HOST_GRP *)0) {     /* If host grp pool NOT empty, get host grp from pool.  */
        p_host_grp               = (NET_IGMP_HOST_GRP *)NetIGMP_HostGrpPoolPtr;
        NetIGMP_HostGrpPoolPtr  = (NET_IGMP_HOST_GRP *)p_host_grp->NextPtr;

    } else {                                                    /* Else none avail, rtn err.                            */
        NET_CTR_ERR_INC(Net_ErrCtrs.IGMP.NoneAvailCtr);
       *p_err =   NET_IGMP_ERR_HOST_GRP_NONE_AVAIL;
        return ((NET_IGMP_HOST_GRP *)0);
    }


                                                                /* ------------------ INIT HOST GRP ------------------- */
    NetIGMP_HostGrpClr(p_host_grp);
    DEF_BIT_SET(p_host_grp->Flags, NET_IGMP_FLAG_USED);         /* Set host grp as used.                                */

                                                                /* ------------ UPDATE HOST GRP POOL STATS ------------ */
    NetStat_PoolEntryUsedInc(&NetIGMP_HostGrpPoolStat, &err);

   *p_err =  NET_IGMP_ERR_NONE;

    return (p_host_grp);
}


/*
*********************************************************************************************************
*                                         NetIGMP_HostGrpFree()
*
* Description : (1) Free a host group :
*
*                   (a) Free   host group timer
*                   (b) Clear  host group controls
*                   (c) Free   host group back to host group pool
*                   (d) Update host group pool statistics
*
*
* Argument(s) : p_host_grp  Pointer to a host group.
*               ----------  Argument checked in NetIGMP_HostGrpRemove()
*                                                 by NetIGMP_HostGrpLeaveHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_HostGrpRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_HostGrpFree (NET_IGMP_HOST_GRP  *p_host_grp)
{
    NET_ERR  err;


                                                                    /* -------------- FREE HOST GRP TMR --------------- */
    if (p_host_grp->TmrPtr != (NET_TMR *)0) {
        NetTmr_Free(p_host_grp->TmrPtr);
    }

                                                                    /* ----------------- CLR HOST GRP ----------------- */
    p_host_grp->State = NET_IGMP_HOST_GRP_STATE_FREE;               /* Set host grp as freed/NOT used.                  */
    DEF_BIT_CLR(p_host_grp->Flags, NET_IGMP_FLAG_USED);
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    NetIGMP_HostGrpClr(p_host_grp);
#endif

                                                                    /* ---------------- FREE HOST GRP ----------------- */
    p_host_grp->NextPtr     = NetIGMP_HostGrpPoolPtr;
    NetIGMP_HostGrpPoolPtr = p_host_grp;

                                                                    /* ---------- UPDATE HOST GRP POOL STATS ---------- */
    NetStat_PoolEntryUsedDec(&NetIGMP_HostGrpPoolStat, &err);
}


/*
*********************************************************************************************************
*                                         NetIGMP_HostGrpClr()
*
* Description : Clear IGMP host group controls.
*
* Argument(s) : p_host_grp  Pointer to an IGMP host group.
*               ----------  Argument validated in NetIGMP_Init();
*                                    checked   in NetIGMP_HostGrpGet()
*                                                   by NetIGMP_HostGrpAdd();
*                                                 NetIGMP_HostGrpFree()
*                                                   by NetIGMP_HostGrpRemove().
*
* Return(s)   : none.
*
* Caller(s)   : NetIGMP_Init(),
*               NetIGMP_HostGrpGet(),
*               NetIGMP_HostGrpFree().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIGMP_HostGrpClr (NET_IGMP_HOST_GRP  *p_host_grp)
{
    p_host_grp->PrevPtr = (NET_IGMP_HOST_GRP *)0;
    p_host_grp->NextPtr = (NET_IGMP_HOST_GRP *)0;

    p_host_grp->TmrPtr  = (NET_TMR           *)0;

    p_host_grp->IF_Nbr  =  NET_IF_NBR_NONE;
    p_host_grp->AddrGrp =  NET_IPv4_ADDR_NONE;

    p_host_grp->State   =  NET_IGMP_HOST_GRP_STATE_FREE;
    p_host_grp->RefCtr  =  0u;
    p_host_grp->Flags   =  NET_IGMP_FLAG_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif /* NET_IGMP_MODULE_EN */

