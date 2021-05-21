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
*                                        NETWORK DEVICE DRIVER
*
*                                               GS2xxx
*
* Filename : net_dev_gs2000.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.02.00 (or more recent version) is included in the project build.
*
*            (2) Device driver compatible with these hardware:
*
*                (a) GS2011MIE
*                (b) GS2011MIZ
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_GS2000_MODULE

#include  "../../../IF/net_if_wifi.h"
#include  "net_dev_gs2000.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Receive buffers MUST contain a prefix of at least one octet for the packet type
*               and any other data that help the driver to demux the management frame.
*
*           (2) The driver MUST handle and implement generic commands defined in net_if_wifi.h. But the driver
*               can also define and implement its own management command which need an response by calling
*               the wireless manager api (p_mgr_api->Mgmt()) to send the management command and to receive
*               the response.
*
*               (a) Driver management command code '100' series reserved for driver.
*********************************************************************************************************
*/

                                                                /* --------------- NET DEV CFG DEFINES ---------------- */
#define  NET_DEV_MGMT_NONE                                  0u
#define  NET_DEV_MGMT_GENERAL_CMD                           100u
#define  NET_DEV_MGMT_SET_ECHO_MODE                         101u
#define  NET_DEV_MGMT_SET_VERBOSE_MODE                      102u
#define  NET_DEV_MGMT_GET_HW_ADDR                           103u
#define  NET_DEV_MGMT_SET_HW_ADDR                           105u

#define  NET_DEV_MAX_NB_OF_AP                               20u

#define  NET_DEV_SPI_CLK_FREQ_MIN                           1u
#define  NET_DEV_SPI_CLK_FREQ_MAX                           10000000u

#define  NET_DEV_RX_BUF_SPI_OFFSET_OCTETS                   4u

#define  NET_DEV_MGMT_RESP_COMMON_TIMEOUT_MS                5000u
#define  NET_DEV_MGMT_RESP_SCAN_TIMEOUT_MS                  10000u
#define  NET_DEV_SCAN_SECURITY_TYPE_STR_MAX_LEN             16u

                                                                /* ---------------- GS2XXX SPI DEFINES ---------------- */
#define  NET_DEV_HI_HDR_LEN                                 8u
#define  NET_DEV_IDLE_CHAR                                  0xF5
#define  NET_DEV_CHECKSUM_LENGTH                            6u

#define  NET_DEV_MAX_MSG_LENGTH                             2032u

#define  NET_DEV_START_OF_FRAME_DELIMITER                   0xA5

#define  NET_DEV_CLASS_WRITE_REQUEST                        0x01
#define  NET_DEV_CLASS_READ_REQUEST                         0x02
#define  NET_DEV_CLASS_DATA_FROM_MCU                        0x03
#define  NET_DEV_CLASS_READWRITE_REQUEST                    0x04

#define  NET_DEV_CLASS_WRITE_RESPONSE_OK                    0x11
#define  NET_DEV_CLASS_READ_RESPONSE_OK                     0x12
#define  NET_DEV_CLASS_WRITE_RESPONSE_NOK                   0x13
#define  NET_DEV_CLASS_READ_RESPONSE_NOK                    0x14
#define  NET_DEV_CLASS_DATA_TO_MCU                          0x15
#define  NET_DEV_CLASS_READWRITE_RESPONSE_OK                0x16
#define  NET_DEV_CLASS_READWRITE_RESPONSE_NOK               0x17

#define  NET_DEV_PACKET_HDR_MAX_LEN                         8u
#define  NET_DEV_PACKET_HDR_MIN_LEN                         6u

#define  NET_DEV_WAIT_HOST_WAKE_UP_SIGNAL_TIMEOUT           5000u

#define  NET_DEV_RESPONSE_NAME                              "Net Dev Response Signal"
#define  NET_DEV_LOCK_NAME                                  "Net Dev Lock"

                                                                /* ------------------ AT CMD DEFINES ------------------ */

#define  NET_DEV_AT_CMD                                     "AT"
#define  NET_DEV_AT_CMD_SET_ECHO_OFF                        "ATE0"
#define  NET_DEV_AT_CMD_SET_ECHO_ON                         "ATE1"
#define  NET_DEV_AT_CMD_SET_VERBOSE_OFF                     "ATV0"
#define  NET_DEV_AT_CMD_SET_VERBOSE_ON                      "ATV1"

#define  NET_DEV_AT_CMD_MAC_ADDR_CFG                        "AT+NMAC"
#define  NET_DEV_AT_CMD_REG_DOMAIN_CFG                      "AT+WREGDOMAIN"
#define  NET_DEV_AT_CMD_SCAN                                "AT+WS"
#define  NET_DEV_AT_CMD_SET_MODE                            "AT+WM"
#define  NET_DEV_AT_CMD_ASSOCIATE                           "AT+WA"
#define  NET_DEV_AT_CMD_DISASSOCIATE                        "AT+WD"
#define  NET_DEV_AT_CMD_WPS_ASSOCIATE                       "AT+WWPS"
#define  NET_DEV_AT_CMD_WIFI_STATUS                         "AT+WSTATUS"
#define  NET_DEV_AT_CMD_GET_RSSI                            "AT+WRSSI"
#define  NET_DEV_AT_CMD_GET_CLIENT_INFO                     "AT+APCLIENTINFO"
#define  NET_DEV_AT_CMD_SET_SECURITY                        "AT+WSEC"
#define  NET_DEV_AT_CMD_SET_PASSPHRASE_WEP                  "AT+WWEP1"
#define  NET_DEV_AT_CMD_SET_PASSPHRASE_WPA                  "AT+WWPA"

#define  NET_DEV_AT_CMD_ENABLE_RADIO                        "AT+WRXACTIVE"
#define  NET_DEV_AT_CMD_SET_TX_PWR                          "AT+WP"



#define  NET_DEV_ECHO_MODE_OFF                              ASCII_CHAR_DIGIT_ZERO
#define  NET_DEV_ECHO_MODE_ON                               ASCII_CHAR_DIGIT_ONE
#define  NET_DEV_VERBOSE_MODE_OFF                           ASCII_CHAR_DIGIT_ZERO
#define  NET_DEV_VERBOSE_MODE_ON                            ASCII_CHAR_DIGIT_ONE
#define  NET_DEV_RADIO_OFF                                  ASCII_CHAR_DIGIT_OFF
#define  NET_DEV_RADIO_ON                                   ASCII_CHAR_DIGIT_ONE

#define  NET_DEV_REG_DOMAIN_FCC                             ASCII_CHAR_DIGIT_ZERO
#define  NET_DEV_REG_DOMAIN_ETSI                            ASCII_CHAR_DIGIT_ONE
#define  NET_DEV_REG_DOMAIN_TELEC                           ASCII_CHAR_DIGIT_TWO

#define  NET_DEV_CRLF_STR                                   "\r\n"
#define  NET_DEV_CRLF_STR_LEN                               2u


#define  NET_DEV_PARAM_SET_MODE_INFRA                       ASCII_CHAR_DIGIT_ZERO
#define  NET_DEV_PARAM_SET_MODE_ADHOC                       ASCII_CHAR_DIGIT_ONE
#define  NET_DEV_PARAM_SET_MODE_AP                          ASCII_CHAR_DIGIT_TWO

#define  NET_DEV_NOT_ASSOCIATED_KEYWORD                     "NOT ASSOCIATED"
#define  NET_DEV_NOT_ASSOCIATED_KEYWORD_LEN                 14u

                                                                /* ---------------- STATE MACHINE ENUM ---------------- */
typedef enum{
    NET_DEV_SPI_STATE_IDLE                                = 1u,
    NET_DEV_SPI_STATE_WAIT_HI_HEADER_RESPONSE             = 2u,
}NET_DEV_SPI_STATE;

typedef enum{

    NET_DEV_WIFI_CMD_JOIN_STATE_ENABLE_RADIO              = 1u,
    NET_DEV_WIFI_CMD_JOIN_STATE_SET_PASSPHRASE            = 2u,
    NET_DEV_WIFI_CMD_JOIN_STATE_SET_MODE                  = 3u,
    NET_DEV_WIFI_CMD_JOIN_STATE_SET_PWR_LEVEL             = 4u,
    NET_DEV_WIFI_CMD_JOIN_STATE_ASSOCIATE                 = 5u,
}NET_DEV_CMD_JOIN_STATE;

typedef enum{
    NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO            = 1u,
    NET_DEV_WIFI_CMD_CREATE_STATE_SET_REG_DOMAIN          = 2u,
    NET_DEV_WIFI_CMD_CREATE_STATE_SET_PWR_LEVEL           = 3u,
    NET_DEV_WIFI_CMD_CREATE_STATE_SET_SECURITY            = 4u,
    NET_DEV_WIFI_CMD_CREATE_STATE_SET_MODE                = 5u,
    NET_DEV_WIFI_CMD_CREATE_STATE_SET_PASSPHRASE          = 6u,
    NET_DEV_WIFI_CMD_CREATE_STATE_ASSOCIATE               = 7u,
}NET_DEV_CMD_CREATE_STATE;

typedef enum{
    NET_DEV_WIFI_CMD_SCAN_STATE_READY                     = 1u,
    NET_DEV_WIFI_CMD_SCAN_STATE_PROCESSING                = 2u,
}NET_DEV_CMD_SCAN_STATE;

                                                                /* ---------------- DICTIONARY DEFINE ----------------- */

#define NET_DEV_DICTIONARY_KEY_INVALID                      DEF_INT_32U_MAX_VAL

#define NET_DEV_SCAN_NET_TYPE_STR_INFRA                     "INFRA"
#define NET_DEV_SCAN_NET_TYPE_STR_ADHOC                     "ADHOC"
#define NET_DEV_SCAN_NET_TYPE_STR_LEN                       5u


#define NET_DEV_SCAN_SECURITY_TYPE_STR_OPEN                 "NONE"
#define NET_DEV_SCAN_SECURITY_TYPE_STR_WEP                  "WEP"
#define NET_DEV_SCAN_SECURITY_TYPE_STR_WPA_PERSONAL         "WPA-PERSONAL"
#define NET_DEV_SCAN_SECURITY_TYPE_STR_WPA2_PERSONAL        "WPA2-PERSONAL"
#define NET_DEV_SCAN_SECURITY_TYPE_STR_WPA_ENTREPRISE       "WPA-ENTERPRISE"
#define NET_DEV_SCAN_SECURITY_TYPE_STR_WPA2_ENTREPRISE      "WPA2-ENTERPRISE"


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) Instance specific data area structures should be defined within this section.
*               The data area structure typically includes error counters and variables used
*               to track the state of the device.  Variables required for correct operation
*               of the device MUST NOT be defined globally and should instead be included within
*               the instance specific data area structure and referenced as pif->Dev_Data structure
*               members.
*********************************************************************************************************
*/

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR                   StatRxPktCtr;
    NET_CTR                   StatTxPktCtr;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR                   ErrRxPktDiscardedCtr;
    NET_CTR                   ErrTxPktDiscardedCtr;
#endif

    CPU_INT08U               *GlobalBufPtr;

    CPU_INT08U                WaitRespType;

    CPU_BOOLEAN               LinkStatus;
    CPU_INT08U                LinkStatusQueryCtr;

    KAL_LOCK_HANDLE           DevLock;
    KAL_SEM_HANDLE            ResponseSignal;

    NET_DEV_SPI_STATE         SPIState;
    CPU_BOOLEAN               PendingRxSignal;

    NET_DEV_CMD_JOIN_STATE    WifiCmdJoinState;
    NET_DEV_CMD_CREATE_STATE  WifiCmdCreateState;
    NET_DEV_CMD_SCAN_STATE    WifiCmdScanState;
    CPU_INT08U                WifiCmdScanAPCnt;
} NET_DEV_DATA;
                                                                /* ----------------- HI HEADER STRUCT ----------------- */
typedef  struct  net_dev_HI_hdr {
    CPU_INT08U   SoF;
    CPU_INT08U   Class;
    CPU_INT08U   Rsvd;
    CPU_INT08U   Info1;
    CPU_INT08U   Info2;
    CPU_INT08U   LenLSB;
    CPU_INT08U   LenMSB;
    CPU_INT08U   CheckSum;
} NET_DEV_HI_HDR;
                                                                /* -------------- AP SCAN RESULT STRUCT --------------- */
typedef  struct  net_dev_AP_info{

    CPU_INT08U   Dummy1[1];
    CPU_INT08U   BSSID[17];
    CPU_INT08U   Dummy2[2];
    CPU_INT08U   SSID[32];
    CPU_INT08U   Dummy3[2];
    CPU_INT08U   Channel[2];
    CPU_INT08U   Dummy4[3];
    CPU_INT08U   Type[5];
    CPU_INT08U   Dummy5[2];
    CPU_INT08U   RSSI[4];
    CPU_INT08U   Dummy6[3];
    CPU_INT08U   Security[16];

} NET_DEV_AP_INFO;
                                                                /* ------------- PEER SCAN RESULT STRUCT -------------- */
typedef  struct  net_dev_peer_info{

    CPU_INT08U   Dummy[7];
    CPU_INT08U   MAC[17];
    CPU_INT08U   Ip[13];

} NET_DEV_PEER_INFO;
                                                                /* ----------------- DEBUG INFO STRUCT ---------------- */
typedef  struct net_dev_dbg_info{

    CPU_INT32U  SendHdrCtr;
    CPU_INT32U  WaitDevRdyCtr;
    CPU_INT32U  WaitStateExit;
    CPU_INT32U  DevRdyCtr;
    CPU_INT32U  WriteOK;
    CPU_INT32U  ReadOK;
    CPU_INT32U  WriteNOK;
    CPU_INT32U  ReadNOK;
    CPU_INT32U  ReadWrite;
    CPU_INT32U  Error;
    CPU_INT32U  IntWait;
    CPU_INT32U  IntIdle;
    CPU_INT32U  PendingDataFlag;
    CPU_INT08U  LastCommand;
    CPU_INT08U  Command;
    CPU_INT32U  RxSignal;
    CPU_INT16U  LastDataLen;
    CPU_INT08U *LastDataPtr;
    CPU_INT08U  Data[1536];
    CPU_INT08U  IsMgmtWaiting;
}NET_DEV_DBG_INFO;
                                                                /* --------------- DICTIONARY TYPEDEFS ---------------- */
typedef  CPU_INT32U  NET_DEV_DICTIONARY_KEY;

typedef  struct  net_dev_dictionary {
    const  NET_DEV_DICTIONARY_KEY   Key;
    const  CPU_CHAR                *StrPtr;
    const  CPU_INT32U               StrLen;
} NET_DEV_DICTIONARY;

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) Global variables are highly discouraged and should only be used for storing NON-instance
*               specific data and the array of instance specific data.  Global variables, those that are
*               not declared within the NET_DEV_DATA area, are not multiple-instance safe and could lead
*               to incorrect driver operation if used to store device state information.
*********************************************************************************************************
*/

static  const  NET_DEV_DICTIONARY  NetDev_DictionaryNetType[] = {
    { NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE,    NET_DEV_SCAN_NET_TYPE_STR_INFRA,     NET_DEV_SCAN_NET_TYPE_STR_LEN   },
    { NET_IF_WIFI_NET_TYPE_ADHOC,             NET_DEV_SCAN_NET_TYPE_STR_ADHOC,     NET_DEV_SCAN_NET_TYPE_STR_LEN   },
};

static  const  NET_DEV_DICTIONARY  NetDev_DictionarySecurityType[] = {
    { NET_IF_WIFI_SECURITY_OPEN,    NET_DEV_SCAN_SECURITY_TYPE_STR_OPEN ,            (sizeof(NET_DEV_SCAN_SECURITY_TYPE_STR_OPEN)            - 1) },
    { NET_IF_WIFI_SECURITY_WEP,     NET_DEV_SCAN_SECURITY_TYPE_STR_WEP ,             (sizeof(NET_DEV_SCAN_SECURITY_TYPE_STR_WEP)             - 1) },
    { NET_IF_WIFI_SECURITY_WPA,     NET_DEV_SCAN_SECURITY_TYPE_STR_WPA_PERSONAL,     (sizeof(NET_DEV_SCAN_SECURITY_TYPE_STR_WPA_PERSONAL)    - 1) },
    { NET_IF_WIFI_SECURITY_WPA2,    NET_DEV_SCAN_SECURITY_TYPE_STR_WPA2_PERSONAL,    (sizeof(NET_DEV_SCAN_SECURITY_TYPE_STR_WPA2_PERSONAL)   - 1) },
    { NET_IF_WIFI_SECURITY_WPA,     NET_DEV_SCAN_SECURITY_TYPE_STR_WPA_ENTREPRISE,   (sizeof(NET_DEV_SCAN_SECURITY_TYPE_STR_WPA_ENTREPRISE)  - 1) },
    { NET_IF_WIFI_SECURITY_WPA2,    NET_DEV_SCAN_SECURITY_TYPE_STR_WPA2_ENTREPRISE,  (sizeof(NET_DEV_SCAN_SECURITY_TYPE_STR_WPA2_ENTREPRISE) - 1) },
};

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Device driver functions may be arbitrarily named.  However, it is recommended that device
*               driver functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_&&&.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*********************************************************************************************************
*/
                                                                /* ------------ FNCT'S COMMON TO ALL DEV'S ------------ */

static  void        NetDev_Init                            (NET_IF                   *p_if,
                                                            NET_ERR                  *p_err);


static  void        NetDev_Start                           (NET_IF                   *p_if,
                                                            NET_ERR                  *p_err);

static  void        NetDev_Stop                            (NET_IF                   *p_if,
                                                            NET_ERR                  *p_err);


static  void        NetDev_Rx                              (NET_IF                   *p_if,
                                                            CPU_INT08U              **p_data,
                                                            CPU_INT16U               *p_size,
                                                            NET_ERR                  *p_err);

static  void        NetDev_Tx                              (NET_IF                   *p_if,
                                                            CPU_INT08U               *p_data,
                                                            CPU_INT16U                size,
                                                            NET_ERR                  *p_err);


static  void        NetDev_AddrMulticastAdd                (NET_IF                   *p_if,
                                                            CPU_INT08U               *p_addr_hw,
                                                            CPU_INT08U                addr_hw_len,
                                                            NET_ERR                  *p_err);

static  void        NetDev_AddrMulticastRemove             (NET_IF                   *p_if,
                                                            CPU_INT08U               *p_addr_hw,
                                                            CPU_INT08U                addr_hw_len,
                                                            NET_ERR                  *p_err);


static  void        NetDev_ISR_Handler                     (NET_IF                   *p_if,
                                                            NET_DEV_ISR_TYPE          type);

static  void        NetDev_MgmtDemux                       (NET_IF                   *p_if,
                                                            NET_BUF                  *p_buf,
                                                            NET_ERR                  *p_err);


static  CPU_INT32U  NetDev_MgmtExecuteCmd                  (NET_IF                   *p_if,
                                                            NET_IF_WIFI_CMD           cmd,
                                                            NET_WIFI_MGR_CTX         *p_ctx,
                                                            void                     *p_cmd_data,
                                                            CPU_INT16U                cmd_data_len,
                                                            CPU_INT08U               *p_buf_rtn,
                                                            CPU_INT08U                buf_rtn_len_max,
                                                            NET_ERR                  *p_err);

static  CPU_INT32U  NetDev_MgmtProcessResp                 (NET_IF                   *p_if,
                                                            NET_IF_WIFI_CMD           cmd,
                                                            NET_WIFI_MGR_CTX         *p_ctx,
                                                            CPU_INT08U               *p_buf_rxd,
                                                            CPU_INT16U                buf_rxd_len,
                                                            CPU_INT08U               *p_buf_rtn,
                                                            CPU_INT16U                buf_rtn_len_max,
                                                            NET_ERR                  *p_err);

                                                                /* -------------- FNCT'S GS2XXX SPECIFIC -------------- */

static  void        NetDev_InitSPI                         (NET_IF                   *p_if,
                                                            NET_ERR                  *p_err);


static  void        NetDev_MgmtExecuteCmdScan              (NET_IF                   *p_if,
                                                            NET_IF_WIFI_SCAN         *p_scan,
                                                            NET_ERR                  *p_err);

static CPU_INT32U   NetDev_MgmtProcessRespScan             (NET_IF                   *p_if,
                                                            CPU_CHAR                 *p_frame,
                                                            CPU_INT16U                p_frame_len,
                                                            CPU_INT08U               *p_ap_buf,
                                                            CPU_INT16U                buf_len,
                                                            NET_ERR                  *p_err);

static CPU_INT08U   NetDev_HIHeaderCheckSum                (CPU_INT08U               *p_data,
                                                            CPU_INT08U                length);

static void         NetDev_FormatHIHeader                  (NET_DEV_HI_HDR           *p_hdr,
                                                            CPU_INT08U                cmd,
                                                            CPU_INT16U                len);

static void         NetDev_SpiSendCmd                      (NET_IF                   *p_if,
                                                            CPU_INT08U                cmd,
                                                            CPU_INT08U               *tx_data_buf,
                                                            CPU_INT08U               *rx_data_buf,
                                                            CPU_INT16U               *data_len,
                                                            NET_ERR                  *p_err);

static void         NetDev_WaitDevRdy                      (NET_IF                   *p_if,
                                                            NET_ERR                  *p_err);

static void         NetDev_ATCmdWr                         (NET_IF                   *p_if,
                                                            CPU_CHAR                 *cmd,
                                                            CPU_CHAR                 *param,
                                                            NET_ERR                  *p_err);

static void         NetDev_TxHeaderFormat                  (CPU_CHAR                 *tx_header,
                                                            CPU_INT16U                size);

static CPU_BOOLEAN  NetDev_CheckStatusCode                 (CPU_CHAR                 *line);

static void         NetDev_MgmtExecuteCmdSetSecurity       (NET_IF                   *p_if,
                                                            NET_IF_WIFI_AP_CFG       *p_ap_cfg,
                                                            NET_ERR                  *p_err);

static void         NetDev_MgmtExecuteCmdSetPassphrase     (NET_IF                   *p_if,
                                                            NET_IF_WIFI_AP_CFG       *p_ap_cfg,
                                                            NET_ERR                  *p_err);

static void         NetDev_MgmtExecuteCmdSetMode           (NET_IF                   *p_if,
                                                            NET_IF_WIFI_CMD           cmd,
                                                            NET_IF_WIFI_AP_CFG       *p_ap_cfg,
                                                            NET_ERR                  *p_err);

static void         NetDev_MgmtExecuteCmdSetTransmitPower  (NET_IF                   *p_if,
                                                            NET_IF_WIFI_AP_CFG       *p_ap_cfg,
                                                            NET_ERR                  *p_err);

static void         NetDev_MgmtExecuteCmdAssociate         (NET_IF                   *p_if,
                                                            NET_IF_WIFI_CMD           cmd,
                                                            NET_IF_WIFI_AP_CFG       *p_ap_cfg,
                                                            NET_ERR                  *p_err);

static CPU_INT08U   NetDev_RxBufLineDiscovery              (CPU_CHAR                 *p_frame,
                                                            CPU_INT16U                len,
                                                            CPU_CHAR                 *pp_line[],
                                                            NET_ERR                  *p_err);

static CPU_INT32U   NetDev_DictionaryGet             (const NET_DEV_DICTIONARY       *p_dictionary_tbl,
                                                            CPU_INT32U                dictionary_size,
                                                      const CPU_CHAR                 *p_str_cmp,
                                                            CPU_INT32U                str_len);

static void         NetDev_LockAcquire                     (NET_IF                   *p_if,
                                                            NET_ERR                  *p_err);

static void         NetDev_LockRelease                     (NET_IF                   *p_if);


/*
*********************************************************************************************************
*                                      NETWORK DEVICE DRIVER API
*
* Note(s) : (1) Device driver API structures are used by applications during calls to NetIF_Add().  This
*               API structure allows higher layers to call specific device driver functions via function
*               pointer instead of by name.  This enables the network protocol suite to compile & operate
*               with multiple device drivers.
*
*           (2) In most cases, the API structure provided below SHOULD suffice for most device drivers
*               exactly as is with the exception that the API structure's name which MUST be unique &
*               SHOULD clearly identify the device being implemented.  For example, the Cirrus Logic
*               CS8900A Ethernet controller's API structure should be named NetDev_API_CS8900A[].
*
*               The API structure MUST also be externally declared in the device driver header file
*               ('net_dev_&&&.h') with the exact same name & type.
*********************************************************************************************************
*/
const  NET_DEV_API_WIFI  NetDev_API_GS2000 = {                                   /* Ether PIO dev API fnct ptrs :       */
                                                 &NetDev_Init,                   /*   Init/add                          */
                                                 &NetDev_Start,                  /*   Start                             */
                                                 &NetDev_Stop,                   /*   Stop                              */
                                                 &NetDev_Rx,                     /*   Rx                                */
                                                 &NetDev_Tx,                     /*   Tx                                */
                                                 &NetDev_AddrMulticastAdd,       /*   Multicast addr add                */
                                                 &NetDev_AddrMulticastRemove,    /*   Multicast addr remove             */
                                                 &NetDev_ISR_Handler,            /*   ISR handler                       */
                                                 &NetDev_MgmtDemux,              /*   Demux   mgmt frame rx'd           */
                                                 &NetDev_MgmtExecuteCmd,         /*   Excute  mgmt cmd                  */
                                                 &NetDev_MgmtProcessResp         /*   Process mgmt resp  rx'd           */
                                                };

/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Initialize required clock sources
*                   (b) Initialize external interrupt controller
*                   (c) Initialize external GPIO controller
*                   (d) Initialize driver state variables
*                   (e) Initialize driver statistics & error counters
*                   (f) Allocate memory for device DMA descriptors
*                   (g) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
*                       (2) Disable device interrupts
*                       (3) Disable device receiver and transmitter
*                       (4) Other necessary device initialization
*
* Argument(s) : p_if    Pointer to an wireless network interface.
*               ----    Argument validated in NetIF_Add().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Add() via 'p_dev_api->Init()'.
*
* Note(s)     : (2) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.  Micrium recommends defining and calling this NetBSP
*                   function even if no additional GPIO initialization is required.
*
*               (3) The application developper SHOULD define NetDev_SPI_Init within net_bsp.c
*                   in order to properly configure SPI registers for the device to operate
*                   properly.
*
*               (4) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
*                   in order to properly enable interrupts on an external or CPU integrated
*                   interrupt controller.  Interrupt sources that are specific to the DEVICE
*                   hardware MUST NOT be initialized from within NetDev_CfgIntCtrl() and
*                   SHOULD only be modified from within the device driver.
*
*                   (a) External interrupt sources are cleared within the NetBSP first level
*                       ISR handler either before or after the call to the device driver ISR
*                       handler function.  The device driver ISR handler function SHOULD only
*                       clear the device specific interrupts and NOT external or CPU interrupt
*                       controller interrupt sources.
*
*               (5) The application developer SHOULD define NetDev_IntCtrl() within net_bsp.c
*                   in order to properly enable  or disable interrupts on an external or CPU integrated
*                   interrupt controller.
*
*               (6) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (7) All device drivers that store instance specific data MUST declare all
*                   instance specific variables within the device data area defined above.
*
*               (8) Drivers SHOULD validate device configuration values and set *perr to
*                   NET_DEV_ERR_INVALID_CFG if unacceptible values have been specified. Fields
*                   of interest generally include, but are not limited to :
*
*                   (a) pdev_cfg->RxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (b) pdev_cfg->TxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (c) pdev_cfg->RxBufAlignOctets
*                   (d) pdev_cfg->TxBufAlignOctets
*                   (e) pdev_cfg->DataBusSizeNbrBits
*                   (f) pdev_cfg->SPI_ClkFreq
*                   (g) pdev_cfg->SPI_ClkPol
*
*                       (1) NET_DEV_SPI_CLK_POL_INACTIVE_LOW
*                       (2) NET_DEV_SPI_CLK_POL_INACTIVE_HIGH
*
*                   (h) pdev_cfg->SPI_ClkPhase
*
*                       (1) NET_DEV_SPI_CLK_PHASE_FALLING_EDGE
*                       (2) NET_DEV_SPI_CLK_PHASE_RASING_EDGE
*
*                   (i) pdev_cfg->SPI_XferUnitLen
*
*                       (1) NET_DEV_SPI_XFER_UNIT_LEN_8_BITS
*                       (2) NET_DEV_SPI_XFER_UNIT_LEN_16_BITS
*                       (3) NET_DEV_SPI_XFER_UNIT_LEN_32_BITS
*                       (4) NET_DEV_SPI_XFER_UNIT_LEN_64_BITS
*
*                   (j) pdev_cfg->SPI_XferShiftDir
*
*                       (1) NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB
*                       (2) NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_LSB
*
*               (9) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    NET_DEV_DATA          *p_dev_data;
    NET_BUF_SIZE           buf_rx_size_max;
    CPU_SIZE_T             reqd_octets;
    CPU_INT16U             buf_size_max;
    LIB_ERR                lib_err;
    KAL_ERR                err_kal;

                                                                /* ---------- OBTAIN REFERENCE TO CFGs/BSP ------------ */
    p_dev_cfg = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;
    p_dev_bsp = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf alignment.                           */
    if (p_dev_cfg->RxBufAlignOctets == 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (p_dev_cfg->RxBufAlignOctets != p_dev_cfg->TxBufAlignOctets) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf ix offset.                           */
    if (p_dev_cfg->RxBufIxOffset != NET_DEV_RX_BUF_SPI_OFFSET_OCTETS) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    buf_rx_size_max = NetBuf_GetMaxSize(p_if->Nbr,
                                        NET_TRANSACTION_RX,
                                        0,
                                        NET_IF_IX_RX);
    if (buf_rx_size_max < NET_IF_802x_FRAME_MAX_SIZE) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (p_dev_cfg->Band != NET_DEV_BAND_2_4_GHZ) {              /* Validate Wireless band to use.                       */
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate SPI freq.                                   */
    if ((p_dev_cfg->SPI_ClkFreq < NET_DEV_SPI_CLK_FREQ_MIN ) ||
        (p_dev_cfg->SPI_ClkFreq > NET_DEV_SPI_CLK_FREQ_MAX)) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate SPI pol.                                    */
    if (p_dev_cfg->SPI_ClkPol != NET_DEV_SPI_CLK_POL_INACTIVE_HIGH) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate SPI phase.                                  */
    if (p_dev_cfg->SPI_ClkPhase != NET_DEV_SPI_CLK_PHASE_FALLING_EDGE) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate xfer unit len.                              */
    if (p_dev_cfg->SPI_XferUnitLen != NET_DEV_SPI_XFER_UNIT_LEN_8_BITS) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate xfer shift dir first.                       */
    if (p_dev_cfg->SPI_XferShiftDir != NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    p_if->Dev_Data = Mem_HeapAlloc( sizeof(NET_DEV_DATA),
                                    4,
                                   &reqd_octets,
                                   &lib_err);
    if (p_if->Dev_Data == (void *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    p_dev_data               = (NET_DEV_DATA *)p_if->Dev_Data;
    buf_size_max             =  DEF_MAX(p_dev_cfg->RxBufLargeSize, p_dev_cfg->TxBufLargeSize);
    p_dev_data->GlobalBufPtr = (CPU_INT08U   *)Mem_HeapAlloc( buf_size_max,
                                                              p_dev_cfg->RxBufAlignOctets,
                                                             &reqd_octets,
                                                             &lib_err);
    if (p_dev_data->GlobalBufPtr == (void *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* ------- INITIALIZE EXTERNAL GPIO CONTROLLER -------- */
    p_dev_bsp->CfgGPIO(p_if, p_err);                            /* Configure Wireless Controller GPIO (see Note #2).    */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }
                                                                /* -------------- GS2XXX RESPONSE SIGNAL -------------- */
    p_dev_data->ResponseSignal = KAL_SemCreate((CPU_CHAR *) NET_DEV_RESPONSE_NAME,
                                                            DEF_NULL,
                                                           &err_kal);
    if (err_kal !=  KAL_ERR_NONE) {
       *p_err = NET_DEV_ERR_INIT;
        return;
    }
                                                                /* ------------------ GS2XXX DEV LOCK ----------------- */
    p_dev_data->DevLock = KAL_LockCreate((const CPU_CHAR *) NET_DEV_LOCK_NAME,
                                                            DEF_NULL,
                                                           &err_kal);
    if (err_kal !=  KAL_ERR_NONE) {
       *p_err = NET_DEV_ERR_INIT;
        return;
    }
                                                                /* ------------ INITIALIZE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_Init(p_if, p_err);                           /* Configure SPI Controller (see Note #3).              */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
        return;
    }
                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
    p_dev_bsp->CfgIntCtrl(p_if, p_err);                         /* Configure ext int ctrl'r (see Note #4).              */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }
                                                                /* ------------ DISABLE EXTERNAL INTERRUPT ------------ */
    p_dev_bsp->IntCtrl(p_if, DEF_NO, p_err);                    /* Disable ext int ctrl'r (See Note #5)                 */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }

                                                                /* ---- INITIALIZE ALL DEVICE DATA AREA VARIABLES ----- */
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_dev_data->StatRxPktCtr         = 0;
    p_dev_data->StatTxPktCtr         = 0;
#endif

#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_dev_data->ErrRxPktDiscardedCtr = 0;
    p_dev_data->ErrTxPktDiscardedCtr = 0;
#endif
    p_dev_data->WifiCmdCreateState   = NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO;
    p_dev_data->WifiCmdJoinState     = NET_DEV_WIFI_CMD_JOIN_STATE_ENABLE_RADIO;
    p_dev_data->WifiCmdScanState     = NET_DEV_WIFI_CMD_SCAN_STATE_READY;
    p_dev_data->SPIState             = NET_DEV_SPI_STATE_IDLE;
    p_dev_data->PendingRxSignal      = DEF_NO;
}


/*
*********************************************************************************************************
*                                            NetDev_Start()
*
* Description : (1) Start network interface hardware :
*
*                   (a) Initialize transmit semaphore count
*                   (b) Start      Wireless device
*                   (c) Initialize Wireless device
*                   (d) Validate   Wireless Firmware version
*
*                       (1) Boot Wireless device to upgrade firmware
*                       (2) Load Firmware
*                       (3) Reset Wireless device
*
*                   (d) Boot       Wireless firmware
*                   (e) Initialize Wireless firmware MAC layer
*                   (f) Configure hardware address
*
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_Start().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE                Wireless device successfully started.
*
*                                                           - RETURNED BY NetIF_AddrHW_SetHandler() : -
*                           NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                           NET_IF_ERR_NULL_FNCT            Invalid NULL function pointer.
*                           NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                           NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                           NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                           NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (2) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, some devices can queue multiple frames for
*                   transmission before blocking is required.  The default semaphore value is one.
*
*               (3) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_802x_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (4) The device hardware address is set from one of the data sources below. Each source
*                   is listed in the order of precedence.
*
*                   (a) Device Configuration Structure      Configure a valid HW address during
*                                                                compile time.
*
*                                                           Configure either "00:00:00:00:00:00" or
*                                                                an empty string, "", in order to
*                                                                configure the HW address using using
*                                                                method (b).
*
*                   (b) NetIF_802x_HW_AddrSet()             Call NetIF_802x_HW_AddrSet() if the HW
*                                                                address needs to be configured via
*                                                                run-time from a different data
*                                                                source. E.g. Non auto-loading
*                                                                memory such as I2C or SPI EEPROM
*                                                               (see Note #3).
*
*                   (c) Auto-Loading via EEPROM              If neither options a) or b) are used,
*                                                                the IF layer will use the HW address
*                                                                obtained from the network hardware
*                                                                address registers.
*
*               (5) More than one SPI device could share the same SPI controller, thus we MUST protect
*                   the access to the SPI controller and these step MUST be followed:
*
*                   (a) Acquire SPI register lock.
*
*                       (1) If no other device share the same SPI controller, it is NOT required to
*                           implement any type of ressource lock.
*
*                   (b) Enable the Device Chip Select.
*
*                       (1) The Chip Select of the device SHOULD be disabled between each device access
*
*                   (c) Set the SPI configuration of the device.
*
*                       (1) If no other device share the same SPI controller, SPI configuration can be
*                           done during the SPI initialization.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *p_if,
                            NET_ERR  *p_err)
{
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_WIFI_MGR_API      *p_mgr_api;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    NET_DEV_DATA          *p_dev_data;
    CPU_INT08U             hw_addr[NET_IF_802x_ADDR_SIZE];
    CPU_INT08U             hw_addr_len;
    CPU_INT16U             len;
    CPU_REG32              reg;
    CPU_BOOLEAN            hw_addr_cfg;
    NET_ERR                err;
    CPU_INT08U             param;
    CPU_INT08U             rep[100];

                                                                /* ----------- OBTAIN REFERENCE TO MGR/BSP ------------ */
    p_mgr_api  = (NET_WIFI_MGR_API     *)p_if->Ext_API;         /* Obtain ptr to the mgr api struct.                    */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;        /* Obtain ptr to dev data area.                         */

                                                                /* -------------- START WIRELESS DEVICE --------------- */
    p_dev_bsp->Start(p_if, p_err);                              /* Power up.                                            */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }
                                                                /* -------------- EN EXTERNAL INTERRUPT --------------- */
    p_dev_bsp->IntCtrl(p_if, DEF_YES, p_err);                   /* En ext int needed to receive mgmt resp.              */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }
                                                                /* ------------ INITIALIZE WIRELESS DEVICE ------------ */
    NetDev_InitSPI(p_if, p_err);                                /* Init Dev SPI.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }
                                                                /* ---------------- DISABLE ECHO MODE -----------------    */
    param = DEF_NO;
    p_mgr_api->Mgmt(               p_if,
                                   NET_DEV_MGMT_SET_ECHO_MODE,
                    (CPU_INT08U *)&param,
                                   0,
                    (CPU_INT08U *) rep,
                                   1,
                                   p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }
                                                                /* ------------ CONFIGURE HARDWARE ADDRESS ------------ */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #3 & #4.                                   */

    NetASCII_Str_to_MAC( p_dev_cfg->HW_AddrStr,                 /* Get configured HW MAC address string, if any ...     */
                        &hw_addr[0],                            /* ... (see Note #4a).                                  */
                        &err);

    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler( p_if->Nbr,
                                &hw_addr[0],
                                 sizeof(hw_addr),
                                &err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors,  configure device HW MAC address.      */
        hw_addr_cfg = DEF_YES;
    } else {                                                    /* Get copy of configured IF layer HW MAC address, ...  */
                                                                /* ... if any (see Note #4b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(p_if->Nbr, &hw_addr[0], &hw_addr_len, &err);

        if (err == NET_IF_ERR_NONE) {                           /* Check if the IF HW address has been user configured. */
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(p_if->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* If NOT valid, attempt to use automatically ...       */
                                                                /* ... loaded HW MAC address (see Note #4c).            */
            len = sizeof(hw_addr);


            p_mgr_api->Mgmt(              p_if,                 /* Attempt to use automatically loaded HW address.      */
                                          NET_DEV_MGMT_GET_HW_ADDR,
                            (CPU_INT08U *)DEF_NULL,
                                          0,
                            (CPU_INT08U *)hw_addr,
                                          len,
                                          p_err);
            if (*p_err != NET_WIFI_MGR_ERR_NONE) {
                *p_err  = NET_DEV_ERR_FAULT;
                 return;
            }

            NetIF_AddrHW_SetHandler((NET_IF_NBR  )p_if->Nbr,    /* See Note #4a.                                        */
                                    (CPU_INT08U *)hw_addr,
                                    (CPU_INT08U  )sizeof(hw_addr),
                                    (NET_ERR    *)p_err);
            if (*p_err != NET_IF_ERR_NONE) {
                *p_err  = NET_DEV_ERR_FAULT;
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        p_mgr_api->Mgmt(              p_if,
                                      NET_DEV_MGMT_SET_HW_ADDR,
                        (CPU_INT08U *)p_dev_cfg->HW_AddrStr,
                                      0,
                        (CPU_INT08U *)hw_addr,
                                      len,
                                      p_err);
        if (*p_err != NET_WIFI_MGR_ERR_NONE) {
            *p_err  = NET_DEV_ERR_FAULT;
             return;
        }
    }
                                                                /* ------- INITIALIZE TRANSMIT SEMAPHORE COUNT -------- */
    NetIF_DevCfgTxRdySignal( p_if,                              /* See Note #2.                                         */
                             p_dev_cfg->TxBufLargeNbr,
                             p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }

    p_dev_data->LinkStatusQueryCtr = 0;
    p_dev_data->LinkStatus         = NET_IF_LINK_DOWN;

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Stop()
*
* Description : (1) Shutdown network interface hardware :
*
*                   (a) Disable the receiver and transmitter
*                   (b) Disable receive and transmit interrupts
*                   (c) Clear pending interrupt requests
*                   (d) Free ALL receive descriptors (Return ownership to hardware)
*                   (e) Deallocate ALL transmit buffers
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_Stop().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE    Device     successfully stopped.
*                           NET_DEV_ERR_FAULT   Device NOT successfully stopped.
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Stop() via 'p_dev_api->Stop()'.
*
* Note(s)     : (2) (a) (1) It is recommended that a device driver should only post all currently-used,
*                           i.e. not-fully-transmitted, transmit buffers to the network interface transmit
*                           deallocation queue.
*
*                       (2) However, a driver MAY attempt to post all queued &/or transmitted buffers.
*                           The network interface transmit deallocation task will silently ignore any
*                           unknown or duplicate transmit buffers.  This allows device drivers to
*                           indiscriminately & easily post all transmit buffers without determining
*                           which buffers have NOT yet been transmitted.
*
*                   (b) (1) Device drivers should assume that the network interface transmit deallocation
*                           queue is large enough to post all currently-used transmit buffers.
*
*                       (2) If the transmit deallocation queue is NOT large enough to post all transmit
*                           buffers, some transmit buffers may/will be leaked/lost.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *p_if,
                           NET_ERR  *p_err)
{

    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;

                                                                /* ----------- OBTAIN REFERENCE TO MGR/BSP ------------ */
    p_dev_bsp = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;          /* Obtain ptr to the bsp api struct.                    */

    p_dev_bsp->Stop(p_if, p_err);                               /* Power down.                                          */

    p_dev_bsp->IntCtrl(p_if, DEF_NO, p_err);                   /* Disable ext int.                                      */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*
*                   (a) Acquire   SPI Lock
*                   (b) Enable    SPI Chip Select
*                   (c) Configure SPI Controller
*                   (d) Determine what caused the interrupt
*                   (e) Obtain pointer to data area to replace existing data area
*                   (f) Read data from the SPI
*                   (g) Set packet type
*                   (h) Set return values, pointer to received data area and size
*                   (i) Increment counters.
*                   (j) Disable SPI Chip Select
*                   (k) Release SPI lock
*
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_RxHandler().
*
*               p_data  Pointer to pointer to received data area. The received data
*                       area address should be returned to the stack by dereferencing
*                       p_data as *p_data = (address of receive data area).
*
*               p_size  Pointer to size. The number of bytes received should be returned
*                       to the stack by dereferencing size as *size = (number of bytes).
*               ------  Argument validated in NetIF_RxPkt().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE                No Error
*                           NET_DEV_ERR_RX                  Generic Rx error.
*                           NET_DEV_ERR_INVALID_SIZE        Invalid Rx frame size.
*                           NET_DEV_ERR_FAULT               Device  fault/failure.
*
*                                                           ------ RETURNED BY NetBuf_GetDataPtr() : ------
*                           NET_BUF_ERR_NONE                Network buffer data area successfully allocated
*                                                               & initialized.
*                           NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                           NET_BUF_ERR_INVALID_IX          Invalid index.
*                           NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                               maximum network buffer data area size
*                                                               available.
*                           NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                               buffer's data area.
*                           NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pdev_api->Rx()'.
*
* Note(s)     : (2) More than one SPI device could share the same SPI controller, thus we MUST protect
*                   the access to the SPI controller and these step MUST be followed:
*
*                   (a) Acquire SPI register lock.
*
*                       (1) If no other device are shared the same SPI controller, it's not required to
*                           implement any type of ressource lock.
*
*                   (b) Enable the Device Chip Select.
*
*                       (1) The Chip Select of the device SHOULD be disbaled between each device access
*
*                   (c) Set the SPI configuration of the device.
*
*                       (1) If no other device are shared the same SPI controller, SPI configuration can be
*                           done during the SPI initialization.
*
*               (3) If a receive error occurs, the function SHOULD return 0 for the size, a
*                   NULL pointer to the data area AND an error code equal to NET_DEV_ERR_RX.
*                   Some devices may require that driver instruct the hardware to drop the
*                   frame if it has been commited to internal device memory.
*
*                   (a) If a new data area is unavailable, the driver MUST instruct hardware
*                       to discard the frame.
*
*               (4) Reading data from the device hardware may occur in various sized reads :
*
*                   (a) Device drivers that require read sizes equivalent to the size of the
*                       device data bus MAY examine pdev_cfg->DataBusSizeNbrBits in order to
*                       determine the number of required data reads.
*
*                   (b) Devices drivers that require read sizes equivalent to the size of the
*                       Rx FIFO width SHOULD use the known FIFO width to determine the number
*                       of required data reads.
*
*                   (c) It may be necessary to round the number of data reads up, OR perform the
*                       last data read outside of the loop.
*
*               (5) A pointer set equal to pbuf_new and sized according to the required data
*                   read size determined in (4) should be used to read data from the device
*                   into the receive buffer.
*
*               (6) Some devices may interrupt only ONCE for a recieved frame.  The driver MAY need
*                   check if additional frames have been received while processing the current
*                   received frame.  If additional frames have been received, the driver MAY need
*                   to signal the receive task before exiting NetDev_Rx().
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *p_if,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *p_size,
                         NET_ERR      *p_err)
{

    NET_DEV_BSP_WIFI_SPI    *p_dev_bsp;
    NET_DEV_CFG_WIFI        *p_dev_cfg;
    CPU_INT08U              *p_buf;
    CPU_INT16U               len;
    NET_ERR                  err;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to dev cfg area.                          */

                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    Net_GlobalLockRelease();                                    /* Release Net lock to prevent deadlocking...           */
    NetDev_LockAcquire(p_if,p_err);                             /* ... then acquire both Dev and Net lock.              */
    Net_GlobalLockAcquire((void *)&NetDev_WaitDevRdy, &err);
    if (*p_err != NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }
    if (err != NET_ERR_NONE) {
        NetDev_LockRelease(p_if);
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Lock(p_if, p_err);                           /* See Note #2a.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_RX_BUSY;
        p_dev_bsp->SPI_Unlock(p_if);
        NetDev_LockRelease(p_if);
        return;
    }

    p_dev_bsp->SPI_SetCfg(p_if,                                 /* ------------- CONFIGURE SPI CONTROLLER ------------- */
                          p_dev_cfg->SPI_ClkFreq,               /* See Note #2c.                                        */
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
         NetDev_LockRelease(p_if);
        *p_err = NET_DEV_ERR_FAULT;
         return;
    }
                                                                /* ----------- OBTAIN PTR TO NEW DATA AREA ------------ */
    p_buf = NetBuf_GetDataPtr(p_if,
                              NET_TRANSACTION_RX,
                              NET_IF_ETHER_FRAME_MAX_SIZE,
                              NET_IF_IX_RX,
                              0,
                              0,
                              0,
                              p_err);
    if (*p_err != NET_BUF_ERR_NONE) {                           /* If unable to get a buffer.                           */
       *p_size = 0;
       *p_data = 0;
        return;
    }
    len = p_dev_cfg->RxBufLargeSize;

    NetDev_SpiSendCmd( p_if,                                    /* ------------- READ THE AVAILABLE DATA ------------- */
                       NET_DEV_CLASS_READ_REQUEST,
                       DEF_NULL,
                       p_buf,
                       &len,
                       p_err);
    if (*p_err != NET_DEV_ERR_NONE) {                           /* If unable to read the data.                          */
       *p_size = 0;
       *p_data = 0;
        p_dev_bsp->SPI_Unlock(p_if);
        NetDev_LockRelease(p_if);
        return;
    }

   *p_data = p_buf;
   *p_size = len;
                                                                /* ----------------- RELEASE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Unlock(p_if);
                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    NetDev_LockRelease(p_if);
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Acquire   SPI Lock
*                   (b) Enable    SPI Chip Select
*                   (c) Configure SPI Controller
*                   (d) Write data to the device to transmit
*                   (e) Increment counters.
*                   (f) Disable SPI Chip Select
*                   (g) Release SPI lock
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_TxHandler().
*
*               p_data  Pointer to data to transmit.
*               ------  Argument checked   in NetIF_Tx().
*
*               size    Size of data to transmit.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE        No Error
*                           NET_DEV_ERR_TX_BUSY     No Tx descriptors available
*                           NET_DEV_ERR_FAULT       Device fault/failure.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pdev_api->Tx()'.
*
* Note(s)     : (2) More than one SPI device could share the same SPI controller, thus we MUST protect
*                   the access to the SPI controller and these step MUST be followed:
*
*                   (a) Acquire SPI register lock.
*
*                       (1) If no other device are shared the same SPI controller, it's not required to
*                           implement any type of ressource lock.
*
*                   (b) Enable the Device Chip Select.
*
*                       (1) The Chip Select of the device SHOULD be disbaled between each device access
*
*                   (c) Set the SPI configuration of the device.
*
*                       (1) If no other device are shared the same SPI controller, SPI configuration can be
*                           done during the SPI initialization.
*
*               (3) Software MUST track all transmit buffer addresses that are that are queued for
*                   transmission but have not received a transmit complete notification (interrupt).
*                   Once the frame has been transmitted, software must post the buffer address of
*                   the frame that has completed transmission to the transmit deallocation task.
*
*                   (a) If the device doesn't support transmit complete notification software must
*                       post the buffer address of the frame once the data is wrote on the device FIFO.
*
*               (4) Writing data to the device hardware may occur in various sized writes :
*
*                   (a) Devices drivers that require write sizes equivalent to the size of the
*                       Tx FIFO width SHOULD use the known FIFO width to determine the number
*                       of required data reads.
*
*                   (b) It may be necessary to round the number of data writes up, OR perform the
*                       last data write outside of the loop.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *p_if,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *p_err)
{
    NET_DEV_BSP_WIFI_SPI     *p_dev_bsp;
    NET_DEV_CFG_WIFI         *p_dev_cfg;
    CPU_CHAR                  tx_hdr[NET_DEV_PACKET_HDR_MAX_LEN];
    CPU_INT16U                hdr_len;
    CPU_INT16U                msg_len;
    CPU_INT08U               *p_msg;
    NET_ERR                   err;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to dev cfg area.                          */


                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    Net_GlobalLockRelease();                                    /* Release Net lock to prevent deadlocking...           */
    NetDev_LockAcquire(p_if,p_err);                             /* ... then acquire both Dev and Net lock.              */
    Net_GlobalLockAcquire((void *)&NetDev_WaitDevRdy, &err);
    if (*p_err != NET_DEV_ERR_NONE) {
       *p_err  = NET_DEV_ERR_FAULT;
        return;
    }
    if (err   != NET_ERR_NONE) {
        NetDev_LockRelease(p_if);
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Lock(p_if, p_err);                           /* See Note #2a.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
       *p_err  = NET_DEV_ERR_TX_BUSY;
        NetDev_LockRelease(p_if);
        return;
    }
                                                                /* ------------- CONFIGURE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_SetCfg(p_if,                                 /* See Note #2c.                                        */
                          p_dev_cfg->SPI_ClkFreq,
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        p_dev_bsp->SPI_Unlock(p_if);
        NetDev_LockRelease(p_if);
       *p_err = NET_DEV_ERR_TX;
        return;
    }
                                                                /* -------------- PREPARE THE MSG HEADER -------------- */

    NetDev_TxHeaderFormat(tx_hdr, size);                        /* Create the TX packet header(<ESC>R:Length:).        */
    hdr_len  = Str_Len(tx_hdr);                                 /* Copy the header at the start of the packet.         */
    p_msg    = p_data;
    p_msg   -= hdr_len;
    Mem_Copy(p_msg, tx_hdr, hdr_len);
    msg_len  = size + hdr_len;
                                                                /* -------------------- SEND DATA --------------------- */
    NetDev_SpiSendCmd( p_if,
                       NET_DEV_CLASS_WRITE_REQUEST,
                       p_msg,
                       DEF_NULL,
                       &msg_len,
                       p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        p_dev_bsp->SPI_Unlock(p_if);
        NetDev_LockRelease(p_if);
       *p_err = NET_DEV_ERR_TX;
        return;
    }

    NetIF_TxDeallocTaskPost(p_data, p_err);                     /* See Note #3a.                                        */
    NetIF_DevTxRdySignal(p_if);
    if (*p_err != NET_IF_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
         NetDev_LockRelease(p_if);
        *p_err = NET_DEV_ERR_FAULT;
         return;
    }

                                                                /* ----------------- RELEASE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Unlock(p_if);
                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    NetDev_LockRelease(p_if);

   *p_err = NET_DEV_ERR_NONE;

}


/*
*********************************************************************************************************
*                                       NetDev_AddrMulticastAdd()
*
* Description : Configure hardware address filtering to accept specified hardware address.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_AddrMulticastAdd().
*
*               paddr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully configured.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef NET_MULTICAST_PRESENT
#endif
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetDev_AddrMulticastRemove()
*
* Description : Configure hardware address filtering to reject specified hardware address.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_AddrHW_SetHandler().
*
*               p_addr_hw   Pointer to hardware address.
*               ---------   Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully removed.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_AddrMulticastAdd() via 'pdev_api->AddrMulticastRemove()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef NET_MULTICAST_PRESENT
#endif
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_ISR_Handler()
*
* Description : This function serves as the device Interrupt Service Routine Handler. This ISR
*               handler MUST service and clear all necessary and enabled interrupt events for
*               the device.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_ISR_Handler().
*
*               type    Network Interface defined argument representing the type of ISR in progress. Codes
*                       for Rx, Tx, Overrun, Jabber, etc... are defined within net_if.h and are passed
*                       into this function by the corresponding Net BSP ISR handler function. The Net
*                       BSP ISR handler function may be called by a specific ISR vector and therefore
*                       know which ISR type code to pass.  Otherwise, the Net BSP may pass
*                       NET_DEV_ISR_TYPE_UNKNOWN and the device driver MAY ignore the parameter when
*                       the ISR type can be deduced by reading an available interrupt status register.
*
*                       Type codes that are defined within net_if.c include but are not limited to :
*
*                           NET_DEV_ISR_TYPE_RX
*                           NET_DEV_ISR_TYPE_TX_COMPLETE
*                           NET_DEV_ISR_TYPE_UNKNOWN
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_ISR_Handler() via 'p_dev_api->ISR_Handler()'.
*
* Note(s)     : (1) This function is called via function pointer from the context of an ISR.
*
*               (2) In the case of an interrupt occurring prior to Network Protocol Stack initialization,
*                   the device driver should ensure that the interrupt source is cleared in order
*                   to prevent the potential for an infinite interrupt loop during system initialization.
*
*               (3) Many devices generate only one interrupt event for several ready frames.
*
*                   (a) It is NOT recommended to read from the SPI controller in the ISR handler as the SPI
*                       can be shared with another peripheral. If the SPI lock has been acquired by another
*                       application/chip the entire application could be locked forever.
*
*                   (b) If the device support the transmit completed notification and it is NOT possible to know
*                       the interrupt type without reading on the device, we suggest to notify the receive task
*                       where the interrupt register is read. And in this case, NetDev_rx() should return a
*                       management frame, which will be send automaticcly to NetDev_Demux() and where the stack
*                       is called to dealloc the packet transmitted.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *p_if,
                                  NET_DEV_ISR_TYPE   type)
{
    KAL_ERR         err_kal;
    NET_ERR         err;
    NET_DEV_DATA   *p_dev_data;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA*)p_if->Dev_Data;                 /* Obtain ptr to dev data area.                         */

    switch (p_dev_data->SPIState) {
                                                                /* ------- WAITING FOR HI HEADER RESPONSE STATE ------- */
        case NET_DEV_SPI_STATE_WAIT_HI_HEADER_RESPONSE:

             KAL_SemPost( p_dev_data->ResponseSignal,
                          KAL_OPT_PEND_NONE,
                         &err_kal);
             break;
                                                                /* ------------ WAITING FOR READ REQUEST -------------- */
        case NET_DEV_SPI_STATE_IDLE:

             if(p_dev_data->PendingRxSignal == DEF_NO){         /* Don't signal the RX task if there is already...     */                                                                                         /* ... one pending.                                    */
                p_dev_data->PendingRxSignal = DEF_YES;
                NetIF_RxTaskSignal(p_if->Nbr, &err);
             }
             break;

        default:
             break;
    }
   (void)&err;                                                  /* Prevent possible 'variable unused' warnings.         */
   (void)&err_kal;                                              /* Prevent possible 'variable unused' warnings.         */
}


/*
*********************************************************************************************************
*                                         NetDev_MgmtDemux()
*
* Description : (1) This function should analyse the management frame received and apply some operations on the device or
*                   call the stack to change the link state or signal the wireless manager when an response is
*                   received which has been requested by a previous management command sent.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_buf       Pointer to a network buffer that received a management frame.
*               -----       Argument checked   in NetIF_WiFi_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Management     successfully processed
*                               NET_DEV_ERR_RX      Management NOT successfully processed
*
* Caller(s)   : NetIF_WiFi_RxMgmtFrameHandler() via 'p_dev_api->DemuxMgmt()'.
*
* Return(s)   : none.
*
* Note(s)     : (2) (a) The network buffer MUST be freed by this functions when the buffer is only used by this function.
*
*                   (b) The network buffer MUST NOT be freed by this function when an error occured and is returned,
*                       the upper layer free the buffer and increment discarded management frame global counter.
*
*                   (c) The netowrk buffer MUST NOT be freed by this function when the wireless manager is signaled as
*                       the network buffer will be used and freed by the wireless manager.
*
*               (3) The wireless manager MUST be signaled only when the response received is for a management
*                   command previously sent.
*********************************************************************************************************
*/

static  void  NetDev_MgmtDemux (NET_IF   *p_if,
                                NET_BUF  *p_buf,
                                NET_ERR  *p_err)
{
    NET_DEV_DATA      *p_dev_data;
    NET_WIFI_MGR_API  *p_mgr_api;
    CPU_BOOLEAN        signal;


    p_dev_data = (NET_DEV_DATA *) p_if->Dev_Data;

    switch (p_dev_data->WaitRespType) {

        case NET_DEV_MGMT_GENERAL_CMD:
        case NET_DEV_MGMT_SET_ECHO_MODE:
        case NET_DEV_MGMT_GET_HW_ADDR:
        case NET_DEV_MGMT_SET_HW_ADDR:
        case NET_IF_WIFI_CMD_SCAN:
        case NET_IF_WIFI_CMD_JOIN:
        case NET_IF_WIFI_CMD_CREATE:
        case NET_IF_WIFI_CMD_LEAVE:
        case NET_IF_WIFI_CMD_GET_PEER_INFO:
        case NET_IF_IO_CTRL_LINK_STATE_GET:
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
                                                                /* The WiFi Manager wait and response.                       */
             signal = DEF_YES;
             break;

        default:
             signal = DEF_NO;
             break;
    }

    if (signal == DEF_YES) {
        p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;
        p_mgr_api->Signal(p_if, p_buf, p_err);
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_MgmtExecuteCmd()
*
* Description : This function MUST initializes or continues a management command.
*
* Argument(s) : p_if                Pointer to a network interface.
*               ----                Argument validated in NetIF_IF_Add(),
*                                                         NetIF_IF_Start(),
*                                                         NetIF_RxHandler(),
*                                                         NetIF_WiFi_Scan(),
*                                                         NetIF_WiFi_Join(),
*                                                         NetIF_WiFi_Leave().
*
*               cmd                 Management command to be executed:
*
*                                       NET_IF_WIFI_CMD_SCAN
*                                       NET_IF_WIFI_CMD_JOIN
*                                       NET_IF_WIFI_CMD_LEAVE
*                                       NET_IF_IO_CTRL_LINK_STATE_GET
*                                       NET_IF_IO_CTRL_LINK_STATE_GET_INFO
*                                       NET_IF_IO_CTRL_LINK_STATE_UPDATE
*                                       Others management commands defined by this driver.
*
*               p_ctx               State machine context See Note #1.
*
*               p_cmd_data          Pointer to a buffer that contains data to be used by the driver to execute
*                                   the command
*
*               cmd_data_len        Command data length.
*
*               p_buf_rtn           Pointer to buffer that will receive return data.
*
*               buf_rtn_len_max     Return data length max.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                      NET_DEV_ERR_NONE    Management     successfully executed
*                                      NET_DEV_ERR_FAULT   Management NOT successfully executed
*
* Caller(s)   : NetWiFiMgr_MgmtHandler() via 'p_dev_api->MgmtExecuteCmd()'.
*
* Return(s)   : length of data wrote in the return buffer in octet.
*
* Note(s)     : (1) The state machine context is used by the Wifi manager to know what it MUST do after this call:
*
*                   (a) WaitResp            is used by the wireless manager to know if an asynchronous response is
*                                           required.
*
*                   (b) WaitRespTimeout_ms  is used by the wireless manager to know what is the timeout to receive
*                                           the response.
*
*                   (c) MgmtCompleted       is used by the wireless manager to know if the management process is
*                                           completed.
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_MgmtExecuteCmd (NET_IF            *p_if,
                                           NET_IF_WIFI_CMD    cmd,
                                           NET_WIFI_MGR_CTX  *p_ctx,
                                           void              *p_cmd_data,
                                           CPU_INT16U         cmd_data_len,
                                           CPU_INT08U        *p_buf_rtn,
                                           CPU_INT08U         buf_rtn_len_max,
                                           NET_ERR           *p_err)
{
    NET_DEV_DATA          *p_dev_data;
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    CPU_BOOLEAN           *p_link_state;
    NET_ERR                err;
    volatile CPU_CHAR     *p_echo_mode;
    CPU_CHAR               param[2];

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to dev cfg area.                          */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;        /* Obtain ptr to dev data area.                         */

   *p_err                     = NET_DEV_ERR_NONE;
    p_ctx->WaitRespTimeout_ms = NET_DEV_MGMT_RESP_COMMON_TIMEOUT_MS;

                                                                /* ----------------- ACQUIRE DEV LOCK ----------------- */
    Net_GlobalLockRelease();
    NetDev_LockAcquire(p_if, p_err);
    Net_GlobalLockAcquire((void *)&NetDev_WaitDevRdy, &err);
    if (*p_err != NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return (0);
    }
    if (err != NET_ERR_NONE) {
        NetDev_LockRelease(p_if);
       *p_err = NET_DEV_ERR_FAULT;
        return (0);
    }
                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Lock(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
       *p_err  = NET_DEV_ERR_RX;
        NetDev_LockRelease(p_if);
        return (0);
    }
                                                                /* ------------- CONFIGURE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_SetCfg(p_if,
                          p_dev_cfg->SPI_ClkFreq,
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
         NetDev_LockRelease(p_if);
        *p_err  = NET_DEV_ERR_RX;
         return (0);
    }

    switch (cmd) {

        case NET_DEV_MGMT_GENERAL_CMD:                          /* ------------------- GENERAL CMD -------------------- */

             NetDev_ATCmdWr(p_if,
                            p_cmd_data,
                            DEF_NULL,
                            p_err);
             if (*p_err != NET_DEV_ERR_NONE) {
                 p_ctx->WaitResp          = DEF_NO;
                 p_ctx->MgmtCompleted     = DEF_YES;
                 p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                 break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_DEV_MGMT_GENERAL_CMD;
             break;

           case NET_DEV_MGMT_SET_ECHO_MODE:                        /* ---------------- SET ECHO MODE CMD ----------------- */

             p_echo_mode = (CPU_CHAR *)p_cmd_data;
             if(*p_echo_mode == DEF_YES){
                 NetDev_ATCmdWr(p_if,
                                NET_DEV_AT_CMD_SET_ECHO_ON,
                                DEF_NULL,
                                p_err);
             } else if(*p_echo_mode == DEF_NO){
                 NetDev_ATCmdWr(p_if,
                                NET_DEV_AT_CMD_SET_ECHO_OFF,
                                DEF_NULL,
                                p_err);
             }
             if (*p_err != NET_DEV_ERR_NONE) {
                 p_ctx->WaitResp          = DEF_NO;
                 p_ctx->MgmtCompleted     = DEF_YES;
                 p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                 break;
             }

             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_DEV_MGMT_SET_ECHO_MODE;

            break;

        case NET_DEV_MGMT_GET_HW_ADDR:                          /* ----------------- GET HW ADDR CMD ------------------ */

             param[0] = ASCII_CHAR_QUESTION_MARK;
             param[1] = ASCII_CHAR_NULL;
             NetDev_ATCmdWr(p_if,
                            NET_DEV_AT_CMD_MAC_ADDR_CFG,
                            param,
                            p_err);
             if (*p_err != NET_DEV_ERR_NONE) {
                 p_ctx->WaitResp          = DEF_NO;
                 p_ctx->MgmtCompleted     = DEF_YES;
                 p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                 break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_DEV_MGMT_GET_HW_ADDR;
             break;

        case NET_DEV_MGMT_SET_HW_ADDR:                          /* ----------------- SET HW ADDR CMD ------------------ */

             NetDev_ATCmdWr(p_if,
                            NET_DEV_AT_CMD_MAC_ADDR_CFG,
                            p_cmd_data,
                            p_err);
             if (*p_err != NET_DEV_ERR_NONE) {
                 p_ctx->WaitResp          = DEF_NO;
                 p_ctx->MgmtCompleted     = DEF_YES;
                 p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                 break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_DEV_MGMT_SET_HW_ADDR;
             break;

        case NET_IF_WIFI_CMD_SCAN:                              /* --------------------- SCAN CMD --------------------- */

             if (p_dev_data->WifiCmdScanState == NET_DEV_WIFI_CMD_SCAN_STATE_READY) {
                 NetDev_MgmtExecuteCmdScan(                    p_if,
                                           (NET_IF_WIFI_SCAN *)p_cmd_data,
                                                               p_err);
                 if (*p_err != NET_DEV_ERR_NONE) {
                     p_ctx->WaitResp          = DEF_NO;
                     p_ctx->MgmtCompleted     = DEF_YES;
                     p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                     break;
                 }
                 p_ctx->WaitResp              = DEF_YES;
                 p_ctx->MgmtCompleted         = DEF_NO;
                 p_ctx->WaitRespTimeout_ms    = NET_DEV_MGMT_RESP_SCAN_TIMEOUT_MS;
                 p_dev_data->WaitRespType     = NET_IF_WIFI_CMD_SCAN;
                 p_dev_data->WifiCmdScanState = NET_DEV_WIFI_CMD_SCAN_STATE_PROCESSING;
                 p_dev_data->WifiCmdScanAPCnt = 0u;
             }


             break;


        case NET_IF_WIFI_CMD_JOIN:                              /* --------------- JOIN SEQUENCE CMD SET -------------- */

             switch (p_dev_data->WifiCmdJoinState) {
                case NET_DEV_WIFI_CMD_JOIN_STATE_ENABLE_RADIO:
                     param[0] = NET_DEV_RADIO_ON;
                     param[1] = ASCII_CHAR_NULL;
                     NetDev_ATCmdWr(p_if,
                                    NET_DEV_AT_CMD_ENABLE_RADIO,
                                    param,
                                    p_err);
                    break;

                 case NET_DEV_WIFI_CMD_JOIN_STATE_SET_PASSPHRASE:
                      NetDev_MgmtExecuteCmdSetPassphrase(                       p_if,
                                                         (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                                p_err);
                      break;

                 case NET_DEV_WIFI_CMD_JOIN_STATE_SET_MODE:
                      NetDev_MgmtExecuteCmdSetMode(                       p_if,
                                                                          cmd,
                                                   (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                          p_err);
                      break;

                 case NET_DEV_WIFI_CMD_JOIN_STATE_SET_PWR_LEVEL:
                      NetDev_MgmtExecuteCmdSetTransmitPower(                       p_if,
                                                            (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                                   p_err);
                        break;

                 case NET_DEV_WIFI_CMD_JOIN_STATE_ASSOCIATE:
                      NetDev_MgmtExecuteCmdAssociate(                        p_if,
                                                                             cmd,
                                                     (NET_IF_WIFI_AP_CFG  *) p_cmd_data,
                                                                             p_err);
                      break;

                 default:
                     *p_err = NET_DEV_ERR_FAULT;
                      break;
             }

             if (*p_err != NET_DEV_ERR_NONE) {
                 p_dev_data->WifiCmdJoinState = NET_DEV_WIFI_CMD_JOIN_STATE_SET_PASSPHRASE;
                 p_ctx->WaitResp              = DEF_NO;
                 p_ctx->MgmtCompleted         = DEF_YES;
                 p_dev_data->WaitRespType     = NET_DEV_MGMT_NONE;
                 break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_IF_WIFI_CMD_JOIN;
             break;

        case NET_IF_WIFI_CMD_CREATE:

            switch (p_dev_data->WifiCmdCreateState) {
                case NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO:
                     param[0] = NET_DEV_RADIO_ON;
                     param[1] = ASCII_CHAR_NULL;
                     NetDev_ATCmdWr(p_if,
                                    NET_DEV_AT_CMD_ENABLE_RADIO,
                                    param,
                                    p_err);
                    break;

                case NET_DEV_WIFI_CMD_CREATE_STATE_SET_PWR_LEVEL:
                     NetDev_MgmtExecuteCmdSetTransmitPower(                       p_if,
                                                           (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                                  p_err);
                     break;

                case NET_DEV_WIFI_CMD_CREATE_STATE_SET_SECURITY:
                     NetDev_MgmtExecuteCmdSetSecurity(                       p_if,
                                                      (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                             p_err);
                     break;

                case NET_DEV_WIFI_CMD_CREATE_STATE_SET_MODE:
                     NetDev_MgmtExecuteCmdSetMode(                       p_if,
                                                                         cmd,
                                                  (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                         p_err);
                     break;

                case NET_DEV_WIFI_CMD_CREATE_STATE_SET_PASSPHRASE:
                     NetDev_MgmtExecuteCmdSetPassphrase(                       p_if,
                                                        (NET_IF_WIFI_AP_CFG *) p_cmd_data,
                                                                               p_err);
                    break;

                case NET_DEV_WIFI_CMD_CREATE_STATE_ASSOCIATE:
                     NetDev_MgmtExecuteCmdAssociate(                       p_if,
                                                                           cmd,
                                                    (NET_IF_WIFI_AP_CFG  *)p_cmd_data,
                                                                           p_err);
                    break;
             }
             if (*p_err != NET_DEV_ERR_NONE) {
                 p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO;
                 p_ctx->WaitResp                = DEF_NO;
                 p_ctx->MgmtCompleted           = DEF_YES;
                 p_dev_data->WaitRespType       = NET_DEV_MGMT_NONE;
                 break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_IF_WIFI_CMD_CREATE;
             p_dev_data->LinkStatus   = NET_IF_LINK_DOWN;
             break;

                                                                /* -------------------- LEAVE CMD --------------------- */
        case NET_IF_WIFI_CMD_LEAVE:

             NetDev_ATCmdWr(p_if,
                               NET_DEV_AT_CMD_DISASSOCIATE,
                            DEF_NULL,
                            p_err);

             if (*p_err != NET_DEV_ERR_NONE) {
                 p_ctx->WaitResp          = DEF_NO;
                 p_ctx->MgmtCompleted     = DEF_YES;
                 p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                 break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_IF_WIFI_CMD_LEAVE;
             p_dev_data->LinkStatus   = NET_IF_LINK_DOWN;
             break;

        case NET_IF_WIFI_CMD_GET_PEER_INFO:

             param[0] = ASCII_CHAR_QUESTION_MARK;
             param[1] = ASCII_CHAR_NULL;
             NetDev_ATCmdWr(p_if,
                               NET_DEV_AT_CMD_GET_CLIENT_INFO,
                            param,
                            p_err);
             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp          = DEF_NO;
                  p_ctx->MgmtCompleted     = DEF_YES;
                  p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                  break;
             }
             p_ctx->WaitResp          = DEF_YES;
             p_ctx->MgmtCompleted     = DEF_NO;
             p_dev_data->WaitRespType = NET_IF_WIFI_CMD_GET_PEER_INFO;
             break;
                                                                 /* ------------------- STATUS CMD -------------------- */
        case NET_IF_IO_CTRL_LINK_STATE_GET:
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             p_dev_data->LinkStatusQueryCtr++;
             if (p_dev_data->LinkStatusQueryCtr == 50) {
                NetDev_ATCmdWr(p_if,
                                "AT+WSTATUS",
                                DEF_NULL,
                                p_err);
                 if (*p_err != NET_DEV_ERR_NONE) {
                     p_ctx->WaitResp          = DEF_NO;
                     p_ctx->MgmtCompleted     = DEF_YES;
                     p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
                     break;
                 }
                 p_ctx->WaitResp                = DEF_YES;
                 p_ctx->MgmtCompleted           = DEF_NO;
                 p_dev_data->WaitRespType       = cmd;
                 p_dev_data->LinkStatusQueryCtr = 0;
             } else {
                 p_link_state         = (CPU_BOOLEAN *)p_buf_rtn;
                *p_link_state         =  p_dev_data->LinkStatus;
                 p_ctx->WaitResp      =  DEF_NO;
                 p_ctx->MgmtCompleted =  DEF_YES;
             }
             break;

                                                                /* ------------ UNKNOWN AND UNSUPORTED CMD ------------ */
        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
        default:
             p_ctx->WaitResp          = DEF_NO;
             p_ctx->MgmtCompleted     = DEF_YES;
             p_dev_data->WaitRespType = NET_DEV_MGMT_NONE;
            *p_err                    = NET_DEV_ERR_FAULT;
             break;
    }
                                                                /* ----------------- RELEASE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Unlock(p_if);
                                                                /* ----------------- RELEASE DEV LOCK ----------------- */
    NetDev_LockRelease(p_if);

    return (0);
}


/*
*********************************************************************************************************
*                                      NetDev_MgmtProcessResp()
*
* Description : After that the wireless manager has received the response, this function is called to
*               analyse, set the state machine context and fill the return buffer.
*
* Argument(s) : p_if                Pointer to a network interface.
*               ----                Argument validated in NetIF_IF_Add(),
*                                                         NetIF_IF_Start(),
*                                                         NetIF_RxHandler(),
*                                                         NetIF_WiFi_Scan(),
*                                                         NetIF_WiFi_Join(),
*                                                         NetIF_WiFi_Leave().
*
*               cmd                 Management command to be executed:
*
*                                       NET_IF_WIFI_CMD_SCAN
*                                       NET_IF_WIFI_CMD_JOIN
*                                       NET_IF_WIFI_CMD_LEAVE
*                                       NET_IF_IO_CTRL_LINK_STATE_GET
*                                       NET_IF_IO_CTRL_LINK_STATE_GET_INFO
*                                       NET_IF_IO_CTRL_LINK_STATE_UPDATE
*                                       Others management commands defined by this driver.
*
*               p_ctx               State machine context See Note #1.
*
*               p_buf_rxd           Pointer to a network buffer that contains the command data response.
*               ---------           Argument checked in NetIF_RxHandler().
*
*               buf_rxd_len         Length of the data response.
*
*               p_buf_rtn           Pointer to buffer that will receive return data.
*
*               buf_rtn_len_max     Return data length max.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE        Management response     successfully processed
*                               NET_DEV_ERR_FAULT       Management response NOT successfully processed
*                               NET_DEV_ERR_NULL_PTR    Pointer argument(s) passed NULL pointer(s).
*
* Caller(s)   : NetWiFiMgr_MgmtHandler() via 'p_dev_api->MgmtProcessResp()'.
*
* Return(s)   : length of data wrote in the return buffer in octet.
*
* Note(s)     : (1) The network buffer is always freed by the wireless manager, no matter the error returned.
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_MgmtProcessResp (NET_IF            *p_if,
                                            NET_IF_WIFI_CMD    cmd,
                                            NET_WIFI_MGR_CTX  *p_ctx,
                                            CPU_INT08U        *p_buf_rxd,
                                            CPU_INT16U         buf_rxd_len,
                                            CPU_INT08U        *p_buf_rtn,
                                            CPU_INT16U         buf_rtn_len_max,
                                            NET_ERR           *p_err)
{
    CPU_INT32U         rtn;
    CPU_CHAR          *p_frame;
    CPU_INT08U        *p_dst;
    NET_DEV_DATA      *p_dev_data;
    NET_DEV_CFG_WIFI  *p_dev_cfg;
    CPU_CHAR           mac_ascii[NET_ASCII_LEN_MAX_ADDR_MAC];
    CPU_INT08U         nb_line;
    CPU_INT08U         last_line_ix;
    CPU_CHAR          *pp_line[NET_DEV_MAX_NB_OF_AP + 4];
    CPU_BOOLEAN        status_ok;
    CPU_INT16U        line_cnt;
    NET_DEV_PEER_INFO *p_peer_info;
    NET_ERR            err;
    CPU_INT08U         hw_addr[NET_IF_802x_ADDR_SIZE];
    NET_IF_WIFI_PEER  *p_peer;
    CPU_INT08U         ctn;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_cfg  = (NET_DEV_CFG_WIFI *)p_if->Dev_Cfg;             /* Obtain ptr to dev cfg area.                          */
    p_dev_data = (NET_DEV_DATA     *)p_if->Dev_Data;            /* Obtain ptr to dev data area.                         */

    p_frame = (CPU_CHAR*)(p_buf_rxd + p_dev_cfg->RxBufIxOffset);
   *p_err   = NET_DEV_ERR_NONE;
    rtn     = 0;

    switch (cmd) {
                                                                /* ------------------- GENERAL CMD -------------------- */
        case NET_DEV_MGMT_GENERAL_CMD:
             if (buf_rtn_len_max < buf_rxd_len ) {
                 Mem_Copy(p_buf_rtn,
                          p_frame,
                          buf_rtn_len_max);
                 rtn = buf_rtn_len_max;
             } else {
                 Mem_Copy(p_buf_rtn,
                          p_frame,
                          buf_rxd_len);
                 rtn = buf_rxd_len;
             }
             if (p_dev_data->WaitRespType == NET_DEV_MGMT_GENERAL_CMD) {
                 p_ctx->WaitResp      = DEF_NO;
                 p_ctx->MgmtCompleted = DEF_YES;
             }
             break;
                                                                /* ---------------- SET ECHO MODE CMD ----------------- */
        case NET_DEV_MGMT_SET_ECHO_MODE:
             if (p_dev_data->WaitRespType == NET_DEV_MGMT_SET_ECHO_MODE) {
                                                                /* Get the ptr of the starting char of each line.       */
                 nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                     buf_rxd_len,
                                                     pp_line,
                                                     p_err);
                 if (*p_err == NET_DEV_ERR_NONE){
                     last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                     status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                     if (status_ok == DEF_NO) {
                        *p_err = NET_DEV_ERR_FAULT;
                     }
                 }
                 p_ctx->WaitResp      = DEF_NO;
                 p_ctx->MgmtCompleted = DEF_YES;
             }
             break;

        case NET_DEV_MGMT_SET_HW_ADDR:
             if (p_dev_data->WaitRespType == NET_DEV_MGMT_SET_HW_ADDR) {
                                                                /* Get the ptr of the starting char of each line.       */
                 nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                     buf_rxd_len,
                                                     pp_line,
                                                     p_err);
                 if (*p_err == NET_DEV_ERR_NONE){
                     last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                     status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                     if (status_ok == DEF_NO){
                        *p_err = NET_DEV_ERR_FAULT;
                     }
                 }
                 p_ctx->WaitResp      = DEF_NO;
                 p_ctx->MgmtCompleted = DEF_YES;
             }
             break;
                                                                /* ----------------- GET HW ADDR CMD ------------------ */
        case NET_DEV_MGMT_GET_HW_ADDR:
             if (p_dev_data->WaitRespType == NET_DEV_MGMT_GET_HW_ADDR) {
                                                                /* Get the ptr of the starting char of each line.       */
                 nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                     buf_rxd_len,
                                                     pp_line,
                                                     p_err);
                 if (*p_err == NET_DEV_ERR_NONE){
                     last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                     status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                     if (status_ok == DEF_YES){

                         Mem_Copy(mac_ascii,
                                  pp_line[0],
                                  NET_ASCII_LEN_MAX_ADDR_MAC);

                         mac_ascii[NET_ASCII_LEN_MAX_ADDR_MAC - 1] = ASCII_CHAR_NULL;

                         p_dst = (CPU_INT08U *)p_buf_rtn;
                         NetASCII_Str_to_MAC( mac_ascii,
                                              p_dst,
                                              p_err);
                         if (*p_err == NET_ASCII_ERR_NONE){
                            *p_err = NET_DEV_ERR_NONE;
                             rtn   = buf_rtn_len_max;
                         } else {
                            *p_err = NET_DEV_ERR_FAULT;
                             rtn   = DEF_NULL;
                         }
                     } else {
                        *p_err = NET_DEV_ERR_FAULT;
                     }
                 }
                 p_ctx->WaitResp      = DEF_NO;
                 p_ctx->MgmtCompleted = DEF_YES;
             }
             break;

                                                                /* --------------------- SCAN CMD --------------------- */
        case NET_IF_WIFI_CMD_SCAN:
             if (p_dev_data->WaitRespType == NET_IF_WIFI_CMD_SCAN) {
                  rtn = NetDev_MgmtProcessRespScan( p_if,
                                                    p_frame,
                                                    buf_rxd_len,
                                                    p_buf_rtn,
                                                    buf_rtn_len_max,
                                                    p_err);
                  if (p_dev_data->WifiCmdScanState == NET_DEV_WIFI_CMD_SCAN_STATE_READY) {
                      p_ctx->WaitResp      = DEF_NO;
                      p_ctx->MgmtCompleted = DEF_YES;
                  } else {
                      p_ctx->WaitResp      = DEF_YES;
                      p_ctx->MgmtCompleted = DEF_NO;
                  }
             }
             break;

        case NET_IF_WIFI_CMD_CREATE:                            /* --------------- CREATE SEQUENCE CMD SET -------------- */
            if (p_dev_data->WaitRespType == NET_IF_WIFI_CMD_CREATE){
                nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                    buf_rxd_len,
                                                    pp_line,
                                                    p_err);
                if (*p_err == NET_DEV_ERR_NONE){
                    last_line_ix = nb_line - 1;
                                                               /* Check the last line for status code and validate.    */
                    status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                    if (status_ok == DEF_NO){
                        p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO;
                       *p_err                          = NET_DEV_ERR_FAULT;
                        p_ctx->WaitResp                = DEF_NO;
                        p_ctx->MgmtCompleted           = DEF_YES;
                        return DEF_NULL;
                    }

                    switch (p_dev_data->WifiCmdCreateState){

                        case NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO:
                             p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_SET_PWR_LEVEL;
                             p_ctx->MgmtCompleted           = DEF_NO;
                             p_ctx->WaitResp                = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_CREATE_STATE_SET_PWR_LEVEL:
                             p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_SET_SECURITY;
                             p_ctx->MgmtCompleted           = DEF_NO;
                             p_ctx->WaitResp                = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_CREATE_STATE_SET_SECURITY:
                             p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_SET_MODE;
                             p_ctx->MgmtCompleted           = DEF_NO;
                             p_ctx->WaitResp                = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_CREATE_STATE_SET_MODE:
                             p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_SET_PASSPHRASE;
                             p_ctx->MgmtCompleted           = DEF_NO;
                             p_ctx->WaitResp                = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_CREATE_STATE_SET_PASSPHRASE:
                             p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_ASSOCIATE;
                             p_ctx->MgmtCompleted           = DEF_NO;
                             p_ctx->WaitResp                = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_CREATE_STATE_ASSOCIATE:
                             p_dev_data->WifiCmdCreateState = NET_DEV_WIFI_CMD_CREATE_STATE_ENABLE_RADIO;
                             p_ctx->MgmtCompleted           = DEF_YES;
                             p_ctx->WaitResp                = DEF_NO;
                             p_dev_data->LinkStatus         = NET_IF_LINK_UP;
                             break;
                       default:
                            *p_err = NET_DEV_ERR_FAULT;
                             break;
                    }
                }
            }
            break;

        case NET_IF_WIFI_CMD_JOIN:                              /* --------------- JOIN SEQUENCE CMD SET -------------- */

            if (p_dev_data->WaitRespType == NET_IF_WIFI_CMD_JOIN){
                                                                /* Get the ptr of the starting char of each line.       */
                 nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                     buf_rxd_len,
                                                     pp_line,
                                                     p_err);
                 if (*p_err == NET_DEV_ERR_NONE){
                     last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                     status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                     if (status_ok == DEF_NO){
                         p_dev_data->WifiCmdJoinState = NET_DEV_WIFI_CMD_JOIN_STATE_SET_PASSPHRASE;
                        *p_err                        = NET_DEV_ERR_FAULT;
                         p_ctx->WaitResp              = DEF_NO;
                         p_ctx->MgmtCompleted         = DEF_YES;
                         return DEF_NULL;
                     }

                    switch (p_dev_data->WifiCmdJoinState){
                        case NET_DEV_WIFI_CMD_JOIN_STATE_ENABLE_RADIO:
                             p_dev_data->WifiCmdJoinState   = NET_DEV_WIFI_CMD_JOIN_STATE_SET_PASSPHRASE;
                             p_ctx->MgmtCompleted           = DEF_NO;
                             p_ctx->WaitResp                = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_JOIN_STATE_SET_PASSPHRASE:
                             p_dev_data->WifiCmdJoinState = NET_DEV_WIFI_CMD_JOIN_STATE_SET_MODE;
                             p_ctx->MgmtCompleted         = DEF_NO;
                             p_ctx->WaitResp              = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_JOIN_STATE_SET_MODE:
                             p_dev_data->WifiCmdJoinState = NET_DEV_WIFI_CMD_JOIN_STATE_SET_PWR_LEVEL;
                             p_ctx->MgmtCompleted         = DEF_NO;
                             p_ctx->WaitResp              = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_JOIN_STATE_SET_PWR_LEVEL:
                             p_dev_data->WifiCmdJoinState = NET_DEV_WIFI_CMD_JOIN_STATE_ASSOCIATE;
                             p_ctx->MgmtCompleted         = DEF_NO;
                             p_ctx->WaitResp              = DEF_NO;
                             break;

                        case NET_DEV_WIFI_CMD_JOIN_STATE_ASSOCIATE:
                             p_dev_data->WifiCmdJoinState = NET_DEV_WIFI_CMD_JOIN_STATE_ENABLE_RADIO;
                             p_ctx->MgmtCompleted         = DEF_YES;
                             p_ctx->WaitResp              = DEF_NO;
                             p_dev_data->LinkStatus       = NET_IF_LINK_UP;
                             break;

                        default:
                            *p_err = NET_DEV_ERR_FAULT;
                             break;
                    }
                 }
             }
             break;
                                                                /* -------------------- LEAVE CMD --------------------- */
        case NET_IF_WIFI_CMD_LEAVE:
             if (p_dev_data->WaitRespType == NET_IF_WIFI_CMD_LEAVE) {
                                                                /* Get the ptr of the starting char of each line.       */
                 nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                     buf_rxd_len,
                                                     pp_line,
                                                     p_err);
                 if (*p_err == NET_DEV_ERR_NONE) {
                     last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                     status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                     if (status_ok == DEF_NO){

                        *p_err = NET_DEV_ERR_FAULT;
                     }
                 }
                 p_ctx->WaitResp      = DEF_NO;
                 p_ctx->MgmtCompleted = DEF_YES;
             }
             break;

        case NET_IF_WIFI_CMD_GET_PEER_INFO:
             if (p_dev_data->WaitRespType == NET_IF_WIFI_CMD_GET_PEER_INFO) {
                 nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                     buf_rxd_len,
                                                     pp_line,
                                                     p_err);
                 if (*p_err == NET_DEV_ERR_NONE) {
                     last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                     status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                     if (status_ok == DEF_NO) {
                        *p_err = NET_DEV_ERR_FAULT;
                     }
                 }
                 ctn = 0u;
                 p_peer = (NET_IF_WIFI_PEER *) p_buf_rtn;
                 for (line_cnt = 0; line_cnt < nb_line ; line_cnt++) {

                     p_peer_info =(NET_DEV_PEER_INFO*) pp_line[line_cnt];
                                                                /* Get and validate the MAC address of the AP.          */
                     Mem_Copy(mac_ascii,
                              p_peer_info->MAC,
                              NET_ASCII_LEN_MAX_ADDR_MAC);

                     mac_ascii[NET_ASCII_LEN_MAX_ADDR_MAC - 1] = ASCII_CHAR_NULL;

                     NetASCII_Str_to_MAC( mac_ascii,
                                          hw_addr,
                                         &err);
                     if (err != NET_ASCII_ERR_NONE) {               /* If failed to convert the MAC address...              */
                        continue;                                     /* ...consider not a AP line and go to the next one.    */
                     }

                     Mem_Copy(p_peer[ctn].HW_Addr, hw_addr, NET_IF_802x_ADDR_SIZE);
                     ctn++;

                 }
                 rtn = ctn * sizeof(NET_IF_WIFI_PEER);
                 p_ctx->WaitResp      = DEF_NO;
                 p_ctx->MgmtCompleted = DEF_YES;
             }
             break;
                                                                /* ------------------- STATUS CMD -------------------- */
        case NET_IF_IO_CTRL_LINK_STATE_GET:
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
                                                                /* Get the ptr of the starting char of each line.       */
             nb_line = NetDev_RxBufLineDiscovery(p_frame,
                                                 buf_rxd_len,
                                                 pp_line,
                                                 p_err);
             if (*p_err == NET_DEV_ERR_NONE){
                 last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
                 status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
                 if (status_ok == DEF_NO){
                    *p_err = NET_DEV_ERR_FAULT;
                     return DEF_NULL;
                 }
             }
             p_ctx->WaitResp      = DEF_NO;
             p_ctx->MgmtCompleted = DEF_YES;

             status_ok = Mem_Cmp(pp_line[0],
                                 NET_DEV_NOT_ASSOCIATED_KEYWORD,
                                 NET_DEV_NOT_ASSOCIATED_KEYWORD_LEN);

             if (status_ok == DEF_YES) {
                 *p_buf_rtn = NET_IF_LINK_DOWN;
             } else {
                 *p_buf_rtn = NET_IF_LINK_UP;
             }
             p_ctx->WaitResp      = DEF_NO;
             p_ctx->MgmtCompleted = DEF_YES;
             rtn = 1;
             break;

                                                                /* ------------ UNKNOWN AND UNSUPORTED CMD ------------ */
        default:
            *p_err = NET_DEV_ERR_FAULT;
             break;
    }
    return (rtn);
}


/*
*********************************************************************************************************
*                                          NetDev_InitSPI()
*
* Description : Initialize the SPI on the device.
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_IF_Start().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    SPI     successfully initialized
*                               NET_DEV_ERR_FAULT   SPI NOT successfully initialized
*
* Caller(s)   : NetDev_Start().
*
* Return(s)   : none.
*
* Note(s)     : (1) The SPI lock must not be acquired before calling this function.
*********************************************************************************************************
*/

static  void  NetDev_InitSPI (NET_IF   *p_if,
                              NET_ERR  *p_err)
{
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    CPU_INT16U             len;
    NET_DEV_DATA          *p_dev_data;

                                                                /* ----------- OBTAIN REFERENCE TO MGR/BSP ------------ */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;        /* Obtain ptr to dev data area.                         */

                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Lock(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

    p_dev_data->SPIState = NET_DEV_SPI_STATE_IDLE;
                                                                  /* ------------- CONFIGURE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_SetCfg(p_if,
                          p_dev_cfg->SPI_ClkFreq,
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        p_dev_bsp->SPI_ChipSelDis(p_if);
        p_dev_bsp->SPI_Unlock(p_if);
       *p_err  = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* ------------- READ DEVICE INIT BANNER ------------- */
    len = p_dev_cfg->RxBufLargeSize;
    NetDev_SpiSendCmd( p_if,
                       NET_DEV_CLASS_READ_REQUEST,
                       DEF_NULL,
                       p_dev_data->GlobalBufPtr,
                      &len,
                       p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_ChipSelDis(p_if);
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

                                                                /* ----------------- RELEASE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Unlock(p_if);

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetDev_MgmtExecuteCmdScan()
*
* Description : Send command to the device to start the scan for available wireless network.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_WiFi_Scan().
*
*               p_scan      Pointer to a variable that contains the information to scan for.
*               ------      Argument validated in NetWiFiMgr_AP_Scan().
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*
*                                                           - Returned by NetDev_WrMgmtCmd() -
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Caller(s)   : NetDev_MgmtExecuteCmd().
*
* Return(s)   : none.
*
* Note(s)     : (1) Prior calling this function, the SPI bus must be configured correctly:
*
*                   (a) The SPI  lock          MUST be acquired via 'p_dev_bsp->SPI_Lock()'.
*                   (b) The Chip select        MUST be enabled  via 'p_dev_bsp->SPI_ChipSelEn()'.
*                   (c) The SPI  configuration MUST be set      via 'p_dev_bsp->SPI_SetCfg()'.
*
*               (2) The scan is completed the device and then results are returned through a response.
*********************************************************************************************************
*/

static  void  NetDev_MgmtExecuteCmdScan (NET_IF            *p_if,
                                         NET_IF_WIFI_SCAN  *p_scan,
                                         NET_ERR           *p_err)
{
    NET_DEV_DATA  *p_dev_data;
    CPU_CHAR       channel_ASCII[3];
    CPU_CHAR      *p_at_cmd;
    CPU_INT16U     tx_len;
                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */

    p_at_cmd = (CPU_CHAR*) p_dev_data->GlobalBufPtr;
                                                                /* -------------- FORMAT THE AT CMD STR --------------- */
    Str_Copy(p_at_cmd, NET_DEV_AT_CMD_SCAN);
    Str_Cat(p_at_cmd, "=");
    Str_Cat(p_at_cmd, p_scan->SSID.SSID);
    Str_Cat(p_at_cmd, ",,");

    if((p_scan->Ch <= NET_IF_WIFI_CH_MAX) &&
       (p_scan->Ch != NET_IF_WIFI_CH_MIN) ){

        channel_ASCII[0] = (p_scan->Ch / 10) + 0x30;
        channel_ASCII[1] = (p_scan->Ch % 10) + 0x30;
        channel_ASCII[2] = ASCII_CHAR_NULL;
        Str_Cat(p_at_cmd, channel_ASCII);
    }

    Str_Cat(p_at_cmd,",");
    Str_Cat(p_at_cmd, STR_CR_LF);
    tx_len = Str_Len (p_at_cmd);
                                                                 /* ------------------ SEND AT CMD -------------------- */
    NetDev_SpiSendCmd(              p_if,
                                    NET_DEV_CLASS_WRITE_REQUEST,
                      (CPU_INT08U*) p_at_cmd,
                                    DEF_NULL,
                                   &tx_len,
                                    p_err);
}
/*
*********************************************************************************************************
*                                    NetDev_MgmtProcessRespScan()
*
* Description : Analyse a scan response and fill application buffer.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_WiFi_Scan().
*
*               p_frame     Pointer to a network buffer that contains the management frame response.
*               -------     Argument validated in NetIF_WiFi_Scan().
*
*               frame_len   Length of the data response.
*
*               p_ap_buf    Pointer to the access point buffer.
*
*               buf_len     Length of the access point buffer in octet.
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan response successfully processed
*                                   NET_DEV_ERR_NULL_PTR    Pointer argument(s) passed NULL pointer(s).
*                                   NET_DEV_ERR_FAULT       Invalid scan response.
*
*
* Caller(s)   : NetDev_MgmtProcessResp().
*
* Return(s)   : Number of octet wrote in the access point buffer.
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

CPU_INT32U  NetDev_MgmtProcessRespScan (NET_IF      *p_if,
                                        CPU_CHAR    *p_frame,
                                        CPU_INT16U   p_frame_len,
                                        CPU_INT08U  *p_ap_buf,
                                        CPU_INT16U   buf_len,
                                        NET_ERR     *p_err)
{
    NET_IF_WIFI_AP          *p_ap;
    NET_DEV_AP_INFO         *p_ap_info;
    CPU_BOOLEAN              status_ok;
    CPU_INT32U               net_type_result;
    CPU_INT32U               security_type_result;
    NET_ERR                  err;
    CPU_CHAR                 i;
    CPU_CHAR                 mac_ascii[NET_ASCII_LEN_MAX_ADDR_MAC];
    CPU_INT08U               nb_line;
    CPU_INT08U               line_cnt;
    CPU_INT08U               last_line_ix;
    CPU_CHAR                *pp_line[NET_DEV_MAX_NB_OF_AP + 4];
    CPU_INT08U               hw_addr[NET_IF_802x_ADDR_SIZE];
    CPU_CHAR                *p_srch;
    CPU_BOOLEAN              is_completed;
    CPU_BOOLEAN              is_eom_found;
    CPU_INT16U               len_rem;
    CPU_INT16U               len_parsed;
    NET_DEV_DATA            *p_dev_data;


#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABELD))
    if (p_ap_buf == (CPU_INT08U *)0) {
       *p_err = NET_DEV_ERR_NULL_PTR;
        return (0);
    }
#endif
                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */

                                                                /* ------------- VARIABLE INITIALIZATION -------------- */
    p_ap         = (NET_IF_WIFI_AP*)p_ap_buf;
    nb_line      = 0;
    p_srch       = (CPU_CHAR*) p_frame;
    is_completed = DEF_NO;
    len_rem      = p_frame_len;
                                                                /* ------------------- LINE PARSING ------------------- */
    while (is_completed == DEF_NO){

        p_srch = Str_Str_N(p_srch,                              /* Search for CRLF Char which is the start of line.     */
                           NET_DEV_CRLF_STR,
                           len_rem);
        if (p_srch == DEF_NULL) {
            is_eom_found = DEF_NO;                              /* Finish if CRLF is not found.                         */
            is_completed = DEF_YES;
            continue;

        } else {
            p_srch          += NET_DEV_CRLF_STR_LEN  ;          /* Set the pointer on the first useful char.            */
            pp_line[nb_line] =  p_srch;                         /* Save the pointer of the line.                        */
            nb_line++;
        }
                                                                /* Check if at the end of the buf.                      */
        len_parsed = p_srch - p_frame ;
        if (len_parsed >= p_frame_len) {
            is_eom_found = DEF_YES;
            is_completed = DEF_YES;
        }
                                                                 /* Update the remaining length of the buffer.           */
        len_rem = p_frame_len - len_parsed;
    }
                                                               /* ---------------- PARSING VALIDATION ---------------- */
    if (nb_line != 0) {
       nb_line--;                                                /* The last poitner found is not a line, but a EOM.     */
       *p_err = NET_DEV_ERR_NONE;
    } else {
       *p_err = NET_DEV_ERR_FAULT;
    }

    if (*p_err == NET_DEV_ERR_NONE) {
        last_line_ix = nb_line - 1;
                                                                /* Check the last line for status code and validate.    */
        if (is_eom_found == DEF_YES) {
            status_ok = NetDev_CheckStatusCode(pp_line[last_line_ix]);
            if (status_ok == DEF_NO){
               *p_err = NET_DEV_ERR_FAULT;
                return DEF_NULL;
            } else {
                p_dev_data->WifiCmdScanState = NET_DEV_WIFI_CMD_SCAN_STATE_READY;
            }
        }
    } else {
       *p_err = NET_DEV_ERR_FAULT;
        return DEF_NULL;
    }
                                                                /* ---------------- AP LINES PARSING ------------------ */
    for (line_cnt = 0; line_cnt < nb_line ; line_cnt++) {

        p_ap = &((NET_IF_WIFI_AP*)p_ap_buf)[p_dev_data->WifiCmdScanAPCnt];

        p_ap_info =(NET_DEV_AP_INFO*) pp_line[line_cnt];
                                                                /* Get and validate the MAC address of the AP.          */
        Mem_Copy(mac_ascii,
                 p_ap_info->BSSID,
                 NET_ASCII_LEN_MAX_ADDR_MAC);

        mac_ascii[NET_ASCII_LEN_MAX_ADDR_MAC - 1] = ASCII_CHAR_NULL;

        NetASCII_Str_to_MAC( mac_ascii,
                             hw_addr,
                            &err);
        if (err != NET_ASCII_ERR_NONE) {                        /* If failed to convert the MAC address...              */
            continue;                                           /* ...consider not a AP line and go to the next one.    */
        }

        Mem_Copy(p_ap->BSSID.BSSID, hw_addr, NET_IF_802x_ADDR_SIZE);
                                                                /* Replace the trailing space char of the SSID with ... */
                                                                /* ...null char.                                        */
        for (i = NET_IF_WIFI_STR_LEN_MAX_SSID - 1; i != 0 ; i--) {
            if(p_ap_info->SSID[i] == ASCII_CHAR_SPACE){
               p_ap_info->SSID[i] = ASCII_CHAR_NULL;
            } else {
               break;
            }
        }
                                                                /* Copy the SSID.                                       */
        Mem_Copy(p_ap->SSID.SSID, p_ap_info->SSID, NET_IF_WIFI_STR_LEN_MAX_SSID);

                                                                /* Get the Channel of the AP.                           */
        p_ap->Ch = (p_ap_info->Channel[0] - 0x30) * 10 +
                   (p_ap_info->Channel[1] - 0x30);

                                                                /* Get the network type.                                */
        net_type_result = NetDev_DictionaryGet(                   NetDev_DictionaryNetType,
                                                                  sizeof(NetDev_DictionaryNetType),
                                               (const CPU_CHAR *) p_ap_info->Type,
                                                                  NET_DEV_SCAN_NET_TYPE_STR_LEN);

        if (net_type_result != NET_DEV_DICTIONARY_KEY_INVALID) {
            p_ap->NetType = net_type_result;
        } else {
            p_ap->NetType = NET_IF_WIFI_NET_TYPE_UNKNOWN;
        }
                                                                /* Get the security type.                               */
        security_type_result = NetDev_DictionaryGet(                   NetDev_DictionarySecurityType,
                                                                       sizeof(NetDev_DictionarySecurityType),
                                                    (const CPU_CHAR *) p_ap_info->Security,
                                                                       NET_DEV_SCAN_SECURITY_TYPE_STR_MAX_LEN);
        if (security_type_result != NET_DEV_DICTIONARY_KEY_INVALID) {
            p_ap->SecurityType = security_type_result;
        } else {
            p_ap->SecurityType = NET_IF_WIFI_NET_TYPE_UNKNOWN;
        }
                                                                /* Get the Signal Strength.                             */
        p_ap->SignalStrength = (p_ap_info->RSSI[2] - 0x30) * 10 +
                               (p_ap_info->RSSI[3] - 0x30);

        p_dev_data->WifiCmdScanAPCnt++;
    }

    return (p_dev_data->WifiCmdScanAPCnt * sizeof(NET_IF_WIFI_AP));
}

/*
*********************************************************************************************************
*                                  NetDev_MgmtExecuteCmdSetSecurity()
*
* Description : Send the command to the device to set the WiFi security.
*
* Argument(s) : p_if        Pointer to a network interface.
*
*               p_ap_cfg    Pointer to variable that contains the wireless network config to create/join.
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_MgmtExecuteCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_MgmtExecuteCmdSetSecurity (NET_IF               *p_if,
                                                NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                                NET_ERR              *p_err)
{
    CPU_CHAR security[3];


    switch (p_ap_cfg->SecurityType) {

       case NET_IF_WIFI_SECURITY_OPEN:
           security[0] = '1';
           security[1] = ASCII_CHAR_NULL;
           break;

       case NET_IF_WIFI_SECURITY_WEP :
           security[0] = '2';
           security[1] = ASCII_CHAR_NULL;
           break;

       case NET_IF_WIFI_SECURITY_WPA :
           security[0] = '4';
           security[1] = ASCII_CHAR_NULL;
           break;

       case NET_IF_WIFI_SECURITY_WPA2:
           security[0] = '8';
           security[1] = ASCII_CHAR_NULL;
           break;

        default:
            *p_err = NET_DEV_ERR_FAULT;
             return;
    }

    NetDev_ATCmdWr(p_if,
                   NET_DEV_AT_CMD_SET_SECURITY,
                   security,
                   p_err);
}


/*
*********************************************************************************************************
*                                 NetDev_MgmtExecuteCmdSetPassphrase()
*
* Description : Send command to the device to set the passphrase(WEP or WPA/WPA2).
*
* Argument(s) : p_if        Pointer to a network interface.
*
*               p_ap_cfg    Pointer to variable that contains the wireless network config to create/join.
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_MgmtExecuteCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_MgmtExecuteCmdSetPassphrase  ( NET_IF               *p_if,
                                                    NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                                    NET_ERR              *p_err)
{
    switch (p_ap_cfg->SecurityType) {
                                                                /* ------------------ OPEN NETWORK -------------------- */
        case NET_IF_WIFI_SECURITY_OPEN:
             NetDev_ATCmdWr(p_if,                               /* To skip this step, only send a dummy cmd.            */
                            NET_DEV_AT_CMD,
                            DEF_NULL,
                            p_err);
             break;
                                                                /* ----------- SET WPA/WPA2 PASSPHRASE CMD ------------ */
        case NET_IF_WIFI_SECURITY_WPA:
        case NET_IF_WIFI_SECURITY_WPA2:
             NetDev_ATCmdWr(p_if,
                            NET_DEV_AT_CMD_SET_PASSPHRASE_WPA,
                            p_ap_cfg->PSK.PSK,
                            p_err);
            break;
                                                                /* ------------- SET WEP PASSPHRASE CMD --------------- */
        case NET_IF_WIFI_SECURITY_WEP:

             NetDev_ATCmdWr(p_if,
                            NET_DEV_AT_CMD_SET_PASSPHRASE_WEP,
                            p_ap_cfg->PSK.PSK,
                            p_err);
             break;

        default:
            *p_err = NET_DEV_ERR_FAULT;
             return;
    }

}


/*
*********************************************************************************************************
*                                    NetDev_MgmtExecuteCmdSetMode()
*
* Description : Send the command to set the network mode of the device (INFRA/ADHOC/AP).
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_WiFi_Join().
*
*               p_join      Pointer to variable that contains the wireless network to join.
*               ------      Argument validated in NetIF_WiFi_Join().
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_MgmtExecuteCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void   NetDev_MgmtExecuteCmdSetMode ( NET_IF               *p_if,
                                              NET_IF_WIFI_CMD       cmd,
                                               NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                               NET_ERR              *p_err)
{
    CPU_CHAR mode[2];

                                                                /* ------------ FORMAT SET MODE PARAMETER ------------- */
    switch (cmd) {
        case NET_IF_WIFI_CMD_JOIN:
            switch (p_ap_cfg->NetType) {

                case NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE:
                     mode[0] = NET_DEV_PARAM_SET_MODE_INFRA;
                     break;

                case NET_IF_WIFI_NET_TYPE_ADHOC:
                     mode[0] = NET_DEV_PARAM_SET_MODE_ADHOC;
                     break;

                case NET_IF_WIFI_NET_TYPE_UNKNOWN:
                default:
                    *p_err = NET_DEV_ERR_FAULT;
                     return;
            }
            break;

        case NET_IF_WIFI_CMD_CREATE:
             switch (p_ap_cfg->NetType) {
                case NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE:
                     mode[0] = NET_DEV_PARAM_SET_MODE_AP;
                     break;

                case NET_IF_WIFI_NET_TYPE_ADHOC:
                     mode[0] = NET_DEV_PARAM_SET_MODE_ADHOC;
                     break;

                case NET_IF_WIFI_NET_TYPE_UNKNOWN:
                default:
                    *p_err = NET_DEV_ERR_FAULT;
                     return;
            }
             break;

        default:
            *p_err = NET_DEV_ERR_FAULT;
             return;
    }
    mode[1] = ASCII_CHAR_NULL;
                                                                /* ---------------- SEND SET MODE CMD ----------------- */
    NetDev_ATCmdWr(p_if,
                   NET_DEV_AT_CMD_SET_MODE,
                   mode,
                   p_err);
}


/*
*********************************************************************************************************
*                                NetDev_MgmtExecuteCmdSetTransmitPower()
*
* Description : Send command to set the transmit power of the device.
*
* Argument(s) : p_if        Pointer to a network interface.
*
*               p_ap_cfg    Pointer to variable that contains the wireless network config to create/join.
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_MgmtExecuteCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void   NetDev_MgmtExecuteCmdSetTransmitPower ( NET_IF               *p_if,
                                                          NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                                          NET_ERR              *p_err)
{
    CPU_CHAR pwr[2];

                                                                /* ----------- FORMAT SET TX PWR PARAMETER ------------ */
    switch (p_ap_cfg->PwrLevel) {

        case NET_IF_WIFI_PWR_LEVEL_LO:
             pwr[0] = '7';
             break;

        case NET_IF_WIFI_PWR_LEVEL_MED:
             pwr[0] = '3';
             break;

        case NET_IF_WIFI_PWR_LEVEL_HI:
             pwr[0] = '0';
             break;

        default:
            *p_err = NET_DEV_ERR_FAULT;
             return;
    }

    pwr[1] = ASCII_CHAR_NULL;
                                                                /* --------------- SEND SET TX PWR CMD ---------------- */
    NetDev_ATCmdWr(p_if,
                   NET_DEV_AT_CMD_SET_TX_PWR,
                   pwr,
                   p_err);
}


/*
*********************************************************************************************************
*                                   NetDev_MgmtExecuteCmdAssociate()
*
* Description : Send command to associate the device to a network / create an access point.
*
* Argument(s) : p_if        Pointer to a network interface.
*
*               cmd         Management command
*
*               p_join      Pointer to variable that contains the wireless network to join.
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_MgmtExecuteCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  void  NetDev_MgmtExecuteCmdAssociate (NET_IF               *p_if,
                                              NET_IF_WIFI_CMD       cmd,
                                               NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                               NET_ERR              *p_err)
{
    NET_DEV_DATA      *p_dev_data;
    CPU_CHAR           channel[3];
    CPU_CHAR          *p_at_cmd;
    CPU_INT16U         tx_len;
                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA*)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */

    p_at_cmd   = (CPU_CHAR*) p_dev_data->GlobalBufPtr;
                                                                /* -------------- FORMAT THE AT CMD STR --------------- */
    Str_Copy(p_at_cmd,
             NET_DEV_AT_CMD_ASSOCIATE);                         /* Copy the AT CMD.                                     */

    Str_Cat(p_at_cmd,                                           /* Copy the SSID.                                       */
            "=");

    Str_Cat(p_at_cmd,                                           /* Copy the SSID.                                       */
            p_ap_cfg->SSID.SSID);

    Str_Cat(p_at_cmd,",,");                                     /* BSSID (MAC Address)is skipped.                       */

    switch (cmd) {
        case NET_IF_WIFI_CMD_JOIN:                              /* Copy the Channel number if needed.                   */
            break;

        case NET_IF_WIFI_CMD_CREATE:
             switch (p_ap_cfg->Ch) {
                 case NET_IF_WIFI_CH_1:
                 case NET_IF_WIFI_CH_2:
                 case NET_IF_WIFI_CH_3:
                 case NET_IF_WIFI_CH_4:
                 case NET_IF_WIFI_CH_5:
                 case NET_IF_WIFI_CH_6:
                 case NET_IF_WIFI_CH_7:
                 case NET_IF_WIFI_CH_8:
                 case NET_IF_WIFI_CH_9:
                 case NET_IF_WIFI_CH_10:
                 case NET_IF_WIFI_CH_11:
                 case NET_IF_WIFI_CH_12:
                 case NET_IF_WIFI_CH_13:
                 case NET_IF_WIFI_CH_14:
                      channel[0] = p_ap_cfg->Ch / 10 + 0x30;
                      channel[1] = p_ap_cfg->Ch % 10 + 0x30;
                      channel[2] = ASCII_CHAR_NULL;
                      Str_Cat(p_at_cmd, channel);
                      break;

                 case NET_IF_WIFI_CH_ALL:
                 default:
                     *p_err = NET_DEV_ERR_FAULT;
                      return;
             }
             break;

        default:
            *p_err = NET_DEV_ERR_FAULT;
             return;
    }

    Str_Cat(p_at_cmd, STR_CR_LF);                                /* Finish the At CMD fomating.                         */
    tx_len = Str_Len(p_at_cmd);

                                                                 /* ------------------ SEND AT CMD -------------------- */
    NetDev_SpiSendCmd(              p_if,
                                    NET_DEV_CLASS_WRITE_REQUEST,
                      (CPU_INT08U*) p_at_cmd,
                                    DEF_NULL,
                                   &tx_len,
                                    p_err);
}


/*
*********************************************************************************************************
*                                       NetDev_CheckStatusCode()
*
* Description : Validate the Status Code from a AT command response.
*
* Argument(s) : line   Pointer on the string to validate.
*
* Return(s)   : Result of the status code validation.
*                   DEF_YES     The AT command has been succesful
*                   DEF_NO      The AT command has failed.
*
* Caller(s)   : NetDev_MgmtProcessResp(),
*               NetDev_MgmtProcessRespScan().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static CPU_BOOLEAN NetDev_CheckStatusCode(CPU_CHAR *line)
{

    CPU_BOOLEAN is_ok ;

                                                                /* Check for ASCCI char 0 if the verbose mode...            */
                                                                /* ... is disabled.                                         */
    if (*line == '0') {
        return DEF_YES;
    }
                                                                /* Check for the string OK if the verbose mode...           */
                                                                /* ...is enabled.                                           */
    is_ok = Mem_Cmp(line,
                    "OK",
                    2);
    if (is_ok == DEF_YES) {
        return DEF_YES;
    }

    return DEF_NO;

}


/*
*********************************************************************************************************
*                                      NetDev_RxBufLineDiscovery()
*
* Description : Seek the CRLF string in a received frame to get the ptr on each line begining.
*
* Argument(s) : p_frame     Pointer on the frame to parse
*
*               len         Length of the frame to parse
*
*               pp_line     Array of pointer on the start of each line discovered
*
*               p_err       Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_DEV_ERR_NONE        Scan command successfully sent
*                                   NET_DEV_ERR_FAULT       Send the scan command failed.
*
* Return(s)   : Number of line in the received frame.
*
* Caller(s)   : NetDev_MgmtProcessResp(),
*               NetDev_MgmtProcessRespScan().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static CPU_INT08U NetDev_RxBufLineDiscovery(CPU_CHAR    *p_frame,
                                            CPU_INT16U   len,
                                            CPU_CHAR    *pp_line[],
                                            NET_ERR     *p_err)
{

    CPU_CHAR     *p_srch;
    CPU_INT08U    nb_line;
    CPU_BOOLEAN   is_completed ;
    CPU_INT16U    len_rem;
    CPU_INT16U    len_parsed;

                                                                /* ------------- VARIABLE INITIALIZATION -------------- */
    nb_line      = 0;
    p_srch       = (CPU_CHAR*) p_frame;
    is_completed = DEF_NO;
    len_rem      = len;
                                                                /* ------------------- LINE PARSING ------------------- */
    while (is_completed == DEF_NO) {

        p_srch = Str_Str_N(p_srch,                              /* Search for CRLF Char which is the start of line.     */
                           NET_DEV_CRLF_STR,
                           len_rem);
        if (p_srch == DEF_NULL) {
                                                                /* Finish if CRLF is not found.                         */
            is_completed = DEF_YES;
            continue;
        } else {
            p_srch          += NET_DEV_CRLF_STR_LEN  ;           /* Set the pointer on the first useful char.            */
            pp_line[nb_line] =  p_srch;                          /* Save the pointer of the line.                        */
            nb_line++;
        }
                                                                 /* Check if at the end of the buf.                      */
        len_parsed = p_srch - p_frame ;
        if (len_parsed >= len) {
            is_completed = DEF_YES;
        }
                                                                 /* Update the remaining length of the buffer.           */
        len_rem = len - len_parsed;
    }
                                                                 /* ---------------- PARSING VALIDATION ---------------- */
    if (nb_line != 0) {
        nb_line--;                                               /* The last poitner found is not a line, but a EOM.     */
       *p_err = NET_DEV_ERR_NONE;
    } else {
       *p_err = NET_DEV_ERR_FAULT;
    }

    return nb_line;
}


/*
*********************************************************************************************************
*                                   SPI READ/WRITE MANAGEMENT FUNCTIONS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         NetDev_SpiChecksum()
*
* Description : Compute the checksum for the HI Header.
*
* Argument(s) : p_hdr   Pointer on the data to parse for the checksum
*
*               length  Data length to be parsed for the checksum
*
* Return(s)   : Computed checksum.
*
* Caller(s)   : NetDev_SpiMakeHeader().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static  CPU_INT08U  NetDev_HIHeaderCheckSum (CPU_INT08U *p_data,
                                             CPU_INT08U  length)

{
    CPU_INT32U  cpt;
    CPU_INT08U *pt;
    CPU_INT32U  ckecksum;


    pt       = (CPU_INT08U*) p_data;
    ckecksum = 0x00000000;

    for (cpt = length; cpt != 0; cpt--) {
        ckecksum += *pt;
        pt++;
    }

    ckecksum ^= 0xFFFFFFFF;

    return ckecksum;
}


/*
*********************************************************************************************************
*                                        NetDev_TxHeaderFormat()
*
* Description : Format the Raw Data header for data tx.
*
* Argument(s) : p_tx_header Pointer for the formatted header.
*
*               size        Size of the packet to transmit.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void NetDev_TxHeaderFormat(CPU_CHAR   *p_tx_header,
                                  CPU_INT16U  size)
{

    p_tx_header[0] = 27;
    p_tx_header[1] = 'R';
    p_tx_header[2] = ':';

    Str_FmtNbr_Int32U(size,
                      4,
                      DEF_NBR_BASE_DEC,
                      '\0',
                      DEF_NO,
                      DEF_YES,
                     &p_tx_header[3]);

    Str_Cat(p_tx_header, ":");
}


/*
*********************************************************************************************************
*                                        NetDev_FormatHIHeader ()
*
* Description : Format the HI header.
*
* Argument(s) : p_hdr   Pointer on the struct to be formatted.
*
*               cmd     Class of the command to execute.
*
*               len     Total data length to be send in the DATA phase.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_SpiSendCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void NetDev_FormatHIHeader (NET_DEV_HI_HDR *p_hdr,
                                   CPU_INT08U      cmd,
                                   CPU_INT16U      len)
{
                                                                /* Format the HI Header.                                */
    p_hdr->SoF      = NET_DEV_START_OF_FRAME_DELIMITER ;
    p_hdr->Class    = cmd;
    p_hdr->Rsvd     = 0x00;
    p_hdr->Info1    = 0x00;
    p_hdr->Info2    = 0x00;
    p_hdr->LenLSB   = (CPU_INT08U)(0x00FF & len);
    p_hdr->LenMSB   = (CPU_INT08U)(0x00FF & (len >> 8));
                                                                /* The SOF Char is not calculated in the checksum.      */
    p_hdr->CheckSum = NetDev_HIHeaderCheckSum((CPU_INT08U *) p_hdr + 1,
                                                             NET_DEV_CHECKSUM_LENGTH);
}


/*
*********************************************************************************************************
*                                          NetDev_WaitDevRdy()
*
* Description : Wait for the Host Wake up signal.
*
* Argument(s) : p_if            Pointer to a network interface.
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE     successfully completed
*                               NET_DEV_ERR_FAULT   Read data failed
*
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ATCmdRd(),
*               NetDev_SpiSendCmd().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void NetDev_WaitDevRdy(NET_IF     *p_if,
                              NET_ERR    *p_err)
{
    KAL_ERR        err_kal;
    NET_DEV_DATA  *p_dev_data;
    NET_ERR        err;


    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;                /* Obtain ptr to dev data area.                         */

                                                                /* ----------------- RELEASE NET LOCK ----------------- */

    Net_GlobalLockRelease();

    KAL_SemPend( p_dev_data->ResponseSignal,
                 KAL_OPT_PEND_BLOCKING,
                 NET_DEV_WAIT_HOST_WAKE_UP_SIGNAL_TIMEOUT,
                &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_DEV_ERR_NONE;
             break;

        case KAL_ERR_TIMEOUT:
        case KAL_ERR_ABORT:
        case KAL_ERR_ISR:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
            *p_err = NET_DEV_ERR_FAULT;
             break;
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetDev_WaitDevRdy, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
    }
}


/*
*********************************************************************************************************
*                                          NetDev_SpiSendCmd()
*
* Description : Send a command to the device through SPI
*
* Argument(s) : p_if              Pointer to a network interface.
*
*               cmd               Class of the command to execute.
*
*               p_tx_data_buf     Pointer to the buffer to transmit to the device.
*
*               p_rx_data_buf     Pointer to buffer to receive from the device.
*
*               p_data_len        Pointer to the length of the buffer.
*
*               p_err             Pointer to variable  that will receive the return error code from this function :
*
*                                     NET_DEV_ERR_NONE    Successfully completed
*                                     NET_DEV_ERR_FAULT   Read data failed
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ATCmdWr(),
*               NetDev_InitSPI(),
*               NetDev_MgmtExecuteCmdAssociate(),
*               NetDev_MgmtExecuteCmdScan(),
*               NetDev_Rx(),
*               NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void NetDev_SpiSendCmd (NET_IF     *p_if,
                               CPU_INT08U  cmd,
                               CPU_INT08U *tx_data_buf,
                               CPU_INT08U *rx_data_buf,
                               CPU_INT16U *data_len,
                               NET_ERR    *p_err)
{
    NET_DEV_HI_HDR           hdr;
    NET_DEV_HI_HDR           hdr_resp;
    CPU_INT08U               hdr_data_len;
    CPU_INT08U               rem;
    CPU_INT08U              *p_rx_data;
    NET_DEV_BSP_WIFI_SPI    *p_dev_bsp;
    NET_DEV_CFG_WIFI        *p_dev_cfg;
    NET_DEV_DATA            *p_dev_data;


                                                                /* ----------- OBTAIN REFERENCE TO MGR/BSP ------------ */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to the dev cfg struct.                    */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;

                                                                /* -------------------- CMD PHASE --------------------- */
                                                                /* Specify to the state machine thaht the next host...  */
                                                                /* ...wake signal is for a HI Header Response.          */

    NetDev_FormatHIHeader(&hdr,                                 /* Format the 8 bytes HI Header.                        */
                           cmd,
                          *data_len);

    p_dev_bsp->IntCtrl(p_if,DEF_NO,p_err);                      /* Disable interrupt while transmitting the header.     */
    if (*p_err !=  NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }
    p_dev_data->SPIState = NET_DEV_SPI_STATE_WAIT_HI_HEADER_RESPONSE;

    p_dev_bsp->SPI_WrRd(               p_if,                    /* Send the cmd request, receive idle char (0xF5).      */
                        (CPU_INT08U*) &hdr,
                        (CPU_INT08U*) &hdr_resp,
                                       NET_DEV_HI_HDR_LEN,
                                       p_err);

    if (*p_err !=  NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

    p_dev_bsp->IntCtrl(p_if,DEF_YES,p_err);                     /* Enable Interrupt for the HI header response singal.  */
    if (*p_err !=  NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }


    NetDev_WaitDevRdy(p_if, p_err);                             /* Wait for the host wake up signal from the module.    */
    if (*p_err !=  NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* ------------------ RESPONSE PHASE ------------------ */
    p_dev_bsp->SPI_WrRd(               p_if,                    /* Send idle char (0xF5), receive cmd response.         */
                                       DEF_NULL,
                        (CPU_INT08U*) &hdr_resp,
                                       NET_DEV_HI_HDR_LEN,
                                       p_err);

    if (*p_err !=  NET_DEV_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* Specify the state machine that the SPI sequence...   */
                                                                /* ...will be complete after this transfer and the...   */
                                                                /* host wake signal is for a new cmd sequence.          */
    p_dev_data->SPIState = NET_DEV_SPI_STATE_IDLE;

                                                                /* -------------------- DATA PHASE -------------------  */
    switch (hdr_resp.Class) {

        case NET_DEV_CLASS_WRITE_RESPONSE_OK:                   /* WRITE Data section.                                  */

             p_dev_bsp->SPI_WrRd(               p_if,           /* Send the cmd request, receive idle char (0xF5).      */
                                 (CPU_INT08U*) &hdr,
                                                DEF_NULL,
                                                NET_DEV_HI_HDR_LEN,
                                                p_err);
             if (*p_err !=  NET_DEV_ERR_NONE) {
                *p_err = NET_DEV_ERR_FAULT;
                 return;
             }

             p_dev_bsp->SPI_WrRd(p_if,                           /* Send the DATA to write (AT commands or packets).    */
                                 tx_data_buf,
                                 DEF_NULL,
                                *data_len,
                                 p_err);

             if (*p_err !=  NET_DEV_ERR_NONE) {
                *p_err = NET_DEV_ERR_FAULT;
                 return;
             }

            break;

        case NET_DEV_CLASS_READ_RESPONSE_OK :                   /* READ Data section.                                   */
                                                                /* Get the length to receive by the device.             */
            *data_len = ((hdr_resp.LenMSB << 8) & 0xFF00) +
                          hdr_resp.LenLSB;

             p_dev_bsp->SPI_WrRd(               p_if,           /* Receive the Hi Header.                               */
                                                DEF_NULL,
                                 (CPU_INT08U*) &hdr_resp,
                                                NET_DEV_HI_HDR_LEN,
                                                p_err);
             if (*p_err !=  NET_DEV_ERR_NONE) {
                *p_err = NET_DEV_ERR_FAULT;
                 return;
             }

             p_dev_data->PendingRxSignal = DEF_NO;

             if (*data_len > NET_DEV_PACKET_HDR_MIN_LEN) {
                                                                /* Set a pointer with the index offset.                 */
                 p_rx_data = &rx_data_buf[p_dev_cfg->RxBufIxOffset];
                                                                /* Read the first 8 bytes.                              */
                 p_dev_bsp->SPI_WrRd( p_if,
                                      DEF_NULL,
                                      p_rx_data,
                                      NET_DEV_PACKET_HDR_MAX_LEN,
                                      p_err);
                if (*p_err !=  NET_DEV_ERR_NONE) {
                   *p_err = NET_DEV_ERR_FAULT;
                    return;
                }
                                                                /* Check if the first received char is a...            */
                                                                /* ...ASCII ESC char.                                  */
                 if (*p_rx_data == ASCII_CHAR_ESCAPE) {

                     if(p_rx_data[1] != 'R'){
                         *p_err = NET_DEV_ERR_FAULT;
                          return;
                     }

                     *rx_data_buf = NET_IF_WIFI_DATA_PKT;       /* The message is a ethernet packet.                   */

                                                                /* Find where the message start.                       */
                     for(hdr_data_len = 3; hdr_data_len < NET_DEV_PACKET_HDR_MAX_LEN; hdr_data_len ++) {
                         if(p_rx_data[hdr_data_len] == ':') {
                            break;
                         }
                     }

                                                                /* Copy the few chars of the packet ethernet that...   */
                                                                /* ...may be read in the first 8 bytes of the message. */
                     rem = NET_DEV_PACKET_HDR_MAX_LEN - hdr_data_len - 1;
                     if (rem > 0) {
                         Mem_Copy(p_rx_data, &p_rx_data[hdr_data_len + 1],rem);
                     }

                     p_dev_bsp->SPI_WrRd( p_if,                 /* Read the rest of the message.                       */
                                          DEF_NULL,
                                         &p_rx_data[rem],
                                         *data_len - NET_DEV_PACKET_HDR_MAX_LEN,
                                          p_err);
                     if (*p_err !=  NET_DEV_ERR_NONE) {
                        *p_err = NET_DEV_ERR_FAULT;
                         return;
                     }
                                                                /* Set data_len to the ethernet packet lenght.        */
                     *data_len -= hdr_data_len + 1;
                                                                /* Fill the packet with zeros if the len is under...  */
                                                                /* ...the min size.                                   */

                     while (*data_len  < NET_IF_802x_FRAME_MIN_SIZE ) {
                         p_rx_data[*data_len] = '\0';
                         (*data_len)++;
                     }
                 } else {
                    *rx_data_buf = NET_IF_WIFI_MGMT_FRAME;      /* The message is a mgmt response or asynchronus.     */
                     p_dev_bsp->SPI_WrRd( p_if,                 /* Receive the DATA (AT answers ).                   */
                                          DEF_NULL,
                                         &p_rx_data[NET_DEV_PACKET_HDR_MAX_LEN],
                                         *data_len - NET_DEV_PACKET_HDR_MAX_LEN,
                                          p_err);
                     if (*p_err !=  NET_DEV_ERR_NONE) {
                        *p_err = NET_DEV_ERR_FAULT;
                         return;
                     }
                 }
             } else {
                *rx_data_buf = NET_IF_WIFI_MGMT_FRAME;          /* The message is a mgmt response or asynchronus.       */
                 p_rx_data = &rx_data_buf[p_dev_cfg->RxBufIxOffset];
                 p_dev_bsp->SPI_WrRd( p_if,                     /* Receive the DATA (AT answers or packets)             */
                                      DEF_NULL,
                                      p_rx_data,
                                     *data_len,
                                      p_err);
                 if (*p_err !=  NET_DEV_ERR_NONE) {
                    *p_err = NET_DEV_ERR_FAULT;
                     return;
                 }

             }
             if (*p_err !=  NET_DEV_ERR_NONE) {
                *p_err = NET_DEV_ERR_FAULT;
                 return;
             }
             break;

        case NET_DEV_CLASS_READWRITE_RESPONSE_NOK :
        case NET_DEV_CLASS_READWRITE_RESPONSE_OK:               /* Not implemented yet.                                 */
            *p_err = NET_DEV_ERR_FAULT;
             return;

        case NET_DEV_CLASS_READ_RESPONSE_NOK :
             *p_err = NET_DEV_ERR_FAULT;
             break;

         case NET_DEV_CLASS_WRITE_RESPONSE_NOK :
             *p_err = NET_DEV_ERR_FAULT;
              break;

        default:
             *p_err = NET_DEV_ERR_FAULT;
              break;

    }
   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetDev_ATCmdWr()
*
* Description : Send a Write AT command.
*
* Argument(s) : p_if    Pointer to a network interface.
*
*               cmd     Pointer on the AT command string to send.
*
*               param   Pointer on the parameter that follow the AT command
*
*               p_err   Pointer to variable  that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE    Successfully completed
*                           NET_DEV_ERR_FAULT   Read data failed
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_MgmtExecuteCmd(),
*               NetDev_MgmtExecuteCmdAssociate(),
*               NetDev_MgmtExecuteCmdScan(),
*               NetDev_MgmtExecuteCmdSetMode(),
*               NetDev_MgmtExecuteCmdSetPassphrase(),
*               NetDev_MgmtExecuteCmdSetTransmitPower().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/
static void NetDev_ATCmdWr (NET_IF     *p_if,
                            CPU_CHAR   *cmd,
                            CPU_CHAR   *param,
                            NET_ERR    *p_err)
{
    CPU_INT16U            tx_len;
    NET_DEV_DATA         *p_dev_data;
    CPU_CHAR             *p_at_cmd;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;        /* Obtain ptr to dev data area.                         */

    p_at_cmd = (CPU_CHAR*)p_dev_data->GlobalBufPtr;
                                                                /* ---------------- FORMAT THE AT CMD ----------------- */
    Str_Copy(p_at_cmd, cmd);

    if (param != DEF_NULL) {
        Str_Cat(p_at_cmd, "=");
        Str_Cat(p_at_cmd, param);
    }
    Str_Cat(p_at_cmd, STR_CR_LF);
    tx_len = Str_Len(p_at_cmd);
                                                                /* ----------------- SEND THE AT CMD ------------------ */
    NetDev_SpiSendCmd(              p_if,
                                    NET_DEV_CLASS_WRITE_REQUEST,
                      (CPU_INT08U*) p_at_cmd,
                                    DEF_NULL,
                                   &tx_len,
                                    p_err);
}


/*
*********************************************************************************************************
*                                      NetDev_DictionaryKeyGet()
*
* Description : Find dictionary key by comparing string with dictionary string entries.
*
* Argument(s) : p_dictionary_tbl    Pointer on the dictionary table.
*
*               dictionary_size     Size of the dictionary in octet.
*
*               p_str_cmp           Pointer to string to find key.
*               ---------           Argument validated by callers.
*
*               str_len             Length of the string.
*
* Return(s)   :
*
* Caller(s)   : NetDev_MgmtProcessRespScan().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static CPU_INT32U  NetDev_DictionaryGet  (const  NET_DEV_DICTIONARY *p_dictionary_tbl,
                                                 CPU_INT32U          dictionary_size,
                                          const  CPU_CHAR           *p_str_cmp,
                                                 CPU_INT32U          str_len)
{
    CPU_INT32U           nbr_entry;
    CPU_INT32U           ix;
    CPU_INT32U           len;
    CPU_INT16S           cmp;
    NET_DEV_DICTIONARY  *p_srch;


    nbr_entry =  dictionary_size / sizeof(NET_DEV_DICTIONARY);
    p_srch    = (NET_DEV_DICTIONARY *)p_dictionary_tbl;
    for (ix = 0; ix < nbr_entry; ix++) {
        len = DEF_MIN(str_len, p_srch->StrLen);
        cmp = Str_Cmp_N(p_str_cmp, p_srch->StrPtr, len);
        if (cmp == 0) {
            return (p_srch->Key);
        }
        p_srch++;
    }

    return (NET_DEV_DICTIONARY_KEY_INVALID);
}


/*
*********************************************************************************************************
*                                         NetDev_LockAcquire()
*
* Description : Attempt to acquire the device lock.
*
* Argument(s) : p_if     Pointer to a network interface.
*
*               p_err    Pointer to variable  that will receive the return error code from this function :
*
*                             NET_DEV_ERR_NONE    Successfully completed
*                             NET_DEV_ERR_FAULT   Read data failed
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_MgmtExecuteCmd(),
*               NetDev_Rx(),
*               NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void  NetDev_LockAcquire (NET_IF     *p_if,
                                 NET_ERR    *p_err)
{
    KAL_ERR         err_kal;
    NET_DEV_DATA   *p_dev_data;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA*)p_if->Dev_Data;                 /* Obtain ptr to dev data area.                         */

                                                                /* Acquire exclusive network access (see Note #1b) ...  */
                                                                /* ... without timeout              (see Note #1a) ...  */
    KAL_LockAcquire(p_dev_data->DevLock, KAL_OPT_PEND_NONE, KAL_TIMEOUT_INFINITE, &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
            *p_err = NET_DEV_ERR_NONE;
             break;

        case KAL_ERR_LOCK_OWNER:
            *p_err = NET_DEV_ERR_FAULT;
             return;

        case KAL_ERR_NULL_PTR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ABORT:
        case KAL_ERR_TIMEOUT:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
            *p_err = NET_DEV_ERR_FAULT;
             break;
    }
}


/*
*********************************************************************************************************
*                                         NetDev_LockRelease()
*
* Description : Release the device lock.
*
* Argument(s) : p_if  Pointer to a network interface.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_ATCmdWr(),
*               NetDev_MgmtExecuteCmd(),
*               NetDev_Rx(),
*               NetDev_Tx().
*
* Note(s)     : none.
*
*********************************************************************************************************
*/

static void  NetDev_LockRelease (NET_IF     *p_if)
{
    KAL_ERR       err_kal;
    NET_DEV_DATA *p_dev_data;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA*)p_if->Dev_Data;                 /* Obtain ptr to dev data area.                         */

    KAL_LockRelease(p_dev_data->DevLock,  &err_kal);            /* Release exclusive network access.                    */

   (void)&err_kal;
}

