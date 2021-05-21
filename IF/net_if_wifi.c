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
*                                              WIRELESS
*
* Filename : net_if_wifi.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports following network interface layers:
*
*                (a) Ethernet See 'net_if_802x.h Note #1a'
*
*                (b) IEEE 802 See 'net_if_802x.h Note #1b'
*
*            (2) Wireless implementation conforms to RFC #1122, Section 2.3.3, bullets (a) & (b), but
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
*            (3) Wireless implemtation doesn't supports wireless supplicant and/or IEEE 802.11. The
*                wireless module-hardware must provide the wireless suppliant and all relevant IEEE 802.11
*                supports.
*
*            (4) REQUIREs the following network protocol files in network directories :
*
*                (a) (1) Network Interface Layer
*                    (2) 802x    Interface layer
*
*                      Located in the following network directory
*
*                          \<Network Protocol Suite>\IF\
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_IF_MODULE_WIFI


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_if_wifi.h"
#include  "net_if.h"
#include  "../Source/net.h"
#include  "../Source/net_type.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IF_WIFI_MODULE_EN


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

                                                    /* ----------------------- NET WIFI IF DATA ----------------------- */
typedef  struct  net_if_data_wifi {
    CPU_INT08U  HW_Addr[NET_IF_802x_ADDR_SIZE];     /* WiFi IF's dev hw addr.                                           */
} NET_IF_DATA_WIFI;


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

                                                                /* --------------------- RX FNCTS --------------------- */
static  void  NetIF_WiFi_Rx                (NET_IF      *p_if,
                                            NET_BUF     *p_buf,
                                            NET_ERR     *p_err);

static  void  NetIF_WiFi_RxPktHandler      (NET_IF      *p_if,
                                            NET_BUF     *p_buf,
                                            NET_ERR     *p_err);

static  void  NetIF_WiFi_RxMgmtFrameHandler(NET_IF      *p_if,
                                            NET_BUF     *p_buf,
                                            NET_ERR     *p_err);

static  void  NetIF_WiFi_RxDiscard         (NET_BUF     *p_buf,
                                            NET_ERR     *p_err);


                                                                /* --------------------- TX FNCTS --------------------- */
static  void  NetIF_WiFi_Tx                (NET_IF      *p_if,
                                            NET_BUF     *p_buf,
                                            NET_ERR     *p_err);

static  void  NetIF_WiFi_TxPktDiscard      (NET_BUF     *p_buf,
                                            NET_ERR     *p_err);


                                                                /* -------------------- API FNCTS --------------------- */
static  void  NetIF_WiFi_IF_Add            (NET_IF      *p_if,
                                            NET_ERR     *p_err);

static  void  NetIF_WiFi_IF_Start          (NET_IF      *p_if,
                                            NET_ERR     *p_err);

static  void  NetIF_WiFi_IF_Stop           (NET_IF      *p_if,
                                            NET_ERR     *p_err);


                                                                /* -------------------- MGMT FNCTS -------------------- */
static  void  NetIF_WiFi_IO_CtrlHandler    (NET_IF      *p_if,
                                            CPU_INT08U   opt,
                                            void        *p_data,
                                            NET_ERR     *p_err);


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

const  NET_IF_API  NetIF_API_WiFi = {                                           /* WiFi IF API fnct ptrs :              */
                                      &NetIF_WiFi_IF_Add,                       /*   Init/add                           */
                                      &NetIF_WiFi_IF_Start,                     /*   Start                              */
                                      &NetIF_WiFi_IF_Stop,                      /*   Stop                               */
                                      &NetIF_WiFi_Rx,                           /*   Rx                                 */
                                      &NetIF_WiFi_Tx,                           /*   Tx                                 */
                                      &NetIF_802x_AddrHW_Get,                   /*   Hw        addr get                 */
                                      &NetIF_802x_AddrHW_Set,                   /*   Hw        addr set                 */
                                      &NetIF_802x_AddrHW_IsValid,               /*   Hw        addr valid               */
                                      &NetIF_802x_AddrMulticastAdd,             /*   Multicast addr add                 */
                                      &NetIF_802x_AddrMulticastRemove,          /*   Multicast addr remove              */
                                      &NetIF_802x_AddrMulticastProtocolToHW,    /*   Multicast addr protocol-to-hw      */
                                      &NetIF_802x_BufPoolCfgValidate,           /*   Buf cfg validation                 */
                                      &NetIF_802x_MTU_Set,                      /*   MTU set                            */
                                      &NetIF_802x_GetPktSizeHdr,                /*   Get pkt hdr size                   */
                                      &NetIF_802x_GetPktSizeMin,                /*   Get pkt min size                   */
                                      &NetIF_802x_GetPktSizeMax,
                                      &NetIF_802x_ISR_Handler,                  /*   ISR handler                        */
                                      &NetIF_WiFi_IO_CtrlHandler                /*   I/O ctrl                           */
                                    };


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          NetIF_WiFi_Init()
*
* Description : (1) Initialize Wireless Network Interface Module :
*
*                   Module initialization NOT yet required/implemented
*
*
* Argument(s) : p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Wireless network interface module
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

void  NetIF_WiFi_Init (NET_ERR  *p_err)
{
   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetIF_WiFi_Scan()
*
* Description : Scan available wireless access point.
*
* Argument(s) : if_nbr              Wireless network interface number.
*
*               p_buf_scan          Pointer to a buffer that will receive available access point.
*
*               buf_scan_len_max    Maximum number of access point that can be stored
*
*               p_ssid              Pointer to ... :
*
*                                       (a) null, if scan for all access point is resquested.
*                                       (b) a string that contains the SSID to scan.
*
*               ch                  Channel number:
*
*                                       NET_IF_WIFI_CH_ALL
*                                       NET_IF_WIFI_CH_1
*                                       NET_IF_WIFI_CH_2
*                                       NET_IF_WIFI_CH_3
*                                       NET_IF_WIFI_CH_4
*                                       NET_IF_WIFI_CH_5
*                                       NET_IF_WIFI_CH_6
*                                       NET_IF_WIFI_CH_7
*                                       NET_IF_WIFI_CH_8
*                                       NET_IF_WIFI_CH_9
*                                       NET_IF_WIFI_CH_10
*                                       NET_IF_WIFI_CH_11
*                                       NET_IF_WIFI_CH_12
*                                       NET_IF_WIFI_CH_13
*                                       NET_IF_WIFI_CH_14
*
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_WIFI_ERR_NONE            Wireless network interface module
*                                                                   successfully scanned.
*                               NET_IF_WIFI_ERR_CH_INVALID      Channel argument passed is invalid.
*                               NET_IF_WIFI_ERR_SCAN            Wireless access point scan failed.
*                               NET_ERR_FAULT_NULL_PTR             Argument 'p_buf_scan' passed a NULL pointer.
*
* Return(s)   : Number of wireless access point found.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_WiFi_Scan() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_WiFi_Scan (       NET_IF_NBR         if_nbr,
                                    NET_IF_WIFI_AP    *p_buf_scan,
                                    CPU_INT16U         buf_scan_len_max,
                             const  NET_IF_WIFI_SSID  *p_ssid,
                                    NET_IF_WIFI_CH     ch,
                                    NET_ERR           *p_err)
{

    NET_IF            *p_if;
    CPU_INT16U         ctn;
    NET_WIFI_MGR_API  *p_mgr_api;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ------------------- VALIDATE ARG ------------------- */
    if (p_buf_scan == (NET_IF_WIFI_AP *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }

    if (ch > NET_IF_WIFI_CH_MAX) {
       *p_err = NET_IF_WIFI_ERR_INVALID_CH;
        return (0u);
    }
#endif

    Net_GlobalLockAcquire((void *)&NetIF_WiFi_Scan, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (0u);
    }

    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (0u);
    }

    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;

    ctn = p_mgr_api->AP_Scan(p_if, p_buf_scan, buf_scan_len_max, p_ssid, ch, p_err);
    switch (*p_err) {
        case NET_WIFI_MGR_ERR_NONE:
            *p_err = NET_IF_WIFI_ERR_NONE;
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_SCAN;
             ctn   = 0u;
             break;
    }

    Net_GlobalLockRelease();

    return (ctn);
}


/*
*********************************************************************************************************
*                                          NetIF_WiFi_Join()
*
* Description : Join a wireless access point.
*
* Argument(s) : if_nbr              Wireless network interface number.
*
*               net_type            Wireless network type:
*
*                                       NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE
*                                       NET_IF_WIFI_NET_TYPE_ADHOC
*
*
*               data_rate           Wireless date rate to configure:
*
*                                       NET_IF_WIFI_DATA_RATE_AUTO
*                                       NET_IF_WIFI_DATA_RATE_1_MBPS
*                                       NET_IF_WIFI_DATA_RATE_2_MBPS
*                                       NET_IF_WIFI_DATA_RATE_5_5_MBPS
*                                       NET_IF_WIFI_DATA_RATE_6_MBPS
*                                       NET_IF_WIFI_DATA_RATE_9_MBPS
*                                       NET_IF_WIFI_DATA_RATE_11_MBPS
*                                       NET_IF_WIFI_DATA_RATE_12_MBPS
*                                       NET_IF_WIFI_DATA_RATE_18_MBPS
*                                       NET_IF_WIFI_DATA_RATE_24_MBPS
*                                       NET_IF_WIFI_DATA_RATE_36_MBPS
*                                       NET_IF_WIFI_DATA_RATE_48_MBPS
*                                       NET_IF_WIFI_DATA_RATE_54_MBPS
*                                       NET_IF_WIFI_DATA_RATE_MCS0
*                                       NET_IF_WIFI_DATA_RATE_MCS1
*                                       NET_IF_WIFI_DATA_RATE_MCS2
*                                       NET_IF_WIFI_DATA_RATE_MCS3
*                                       NET_IF_WIFI_DATA_RATE_MCS4
*                                       NET_IF_WIFI_DATA_RATE_MCS5
*                                       NET_IF_WIFI_DATA_RATE_MCS6
*                                       NET_IF_WIFI_DATA_RATE_MCS7
*                                       NET_IF_WIFI_DATA_RATE_MCS8
*                                       NET_IF_WIFI_DATA_RATE_MCS9
*                                       NET_IF_WIFI_DATA_RATE_MCS10
*                                       NET_IF_WIFI_DATA_RATE_MCS11
*                                       NET_IF_WIFI_DATA_RATE_MCS12
*                                       NET_IF_WIFI_DATA_RATE_MCS13
*                                       NET_IF_WIFI_DATA_RATE_MCS14
*                                       NET_IF_WIFI_DATA_RATE_MCS15
*
*
*               security_type       Wireless security type:
*
*                                       NET_IF_WIFI_SECURITY_OPEN
*                                       NET_IF_WIFI_SECURITY_WEP
*                                       NET_IF_WIFI_SECURITY_WPA
*                                       NET_IF_WIFI_SECURITY_WPA2
*
*
*               pwr_level           Wireless radio power to configure:
*
*                                       NET_IF_WIFI_PWR_LEVEL_LO
*                                       NET_IF_WIFI_PWR_LEVEL_MED
*                                       NET_IF_WIFI_PWR_LEVEL_HI
*
*
*               ssid                SSID of the access point to join.
*
*               psk                 Pre shared key of the access point.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Wireless network interface module
*                                                                       successfully joined.
*
*                               NET_IF_WIFI_ERR_JOIN                Wireless network interface module
*                                                                       access point join failed.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_WiFi_Join() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) Before join an access point, the access point should have been found during a
*                   previous scan.
*********************************************************************************************************
*/

void  NetIF_WiFi_Join (NET_IF_NBR                  if_nbr,
                       NET_IF_WIFI_NET_TYPE        net_type,
                       NET_IF_WIFI_DATA_RATE       data_rate,
                       NET_IF_WIFI_SECURITY_TYPE   security_type,
                       NET_IF_WIFI_PWR_LEVEL       pwr_level,
                       NET_IF_WIFI_SSID            ssid,
                       NET_IF_WIFI_PSK             psk,
                       NET_ERR                    *p_err)
{

    NET_IF               *p_if;
    NET_WIFI_MGR_API     *p_mgr_api;
    NET_IF_WIFI_AP_CFG    ap_cfg;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ------------------- VALIDATE ARG ------------------- */
    switch (net_type) {
        case NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE:
        case NET_IF_WIFI_NET_TYPE_ADHOC:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_NET_TYPE;
             return;
    }

    switch (data_rate) {
        case NET_IF_WIFI_DATA_RATE_AUTO:
        case NET_IF_WIFI_DATA_RATE_1_MBPS:
        case NET_IF_WIFI_DATA_RATE_2_MBPS:
        case NET_IF_WIFI_DATA_RATE_5_5_MBPS:
        case NET_IF_WIFI_DATA_RATE_6_MBPS:
        case NET_IF_WIFI_DATA_RATE_9_MBPS:
        case NET_IF_WIFI_DATA_RATE_11_MBPS:
        case NET_IF_WIFI_DATA_RATE_12_MBPS:
        case NET_IF_WIFI_DATA_RATE_18_MBPS:
        case NET_IF_WIFI_DATA_RATE_24_MBPS:
        case NET_IF_WIFI_DATA_RATE_36_MBPS:
        case NET_IF_WIFI_DATA_RATE_48_MBPS:
        case NET_IF_WIFI_DATA_RATE_54_MBPS:
        case NET_IF_WIFI_DATA_RATE_MCS0:
        case NET_IF_WIFI_DATA_RATE_MCS1:
        case NET_IF_WIFI_DATA_RATE_MCS2:
        case NET_IF_WIFI_DATA_RATE_MCS3:
        case NET_IF_WIFI_DATA_RATE_MCS4:
        case NET_IF_WIFI_DATA_RATE_MCS5:
        case NET_IF_WIFI_DATA_RATE_MCS6:
        case NET_IF_WIFI_DATA_RATE_MCS7:
        case NET_IF_WIFI_DATA_RATE_MCS8:
        case NET_IF_WIFI_DATA_RATE_MCS9:
        case NET_IF_WIFI_DATA_RATE_MCS10:
        case NET_IF_WIFI_DATA_RATE_MCS11:
        case NET_IF_WIFI_DATA_RATE_MCS12:
        case NET_IF_WIFI_DATA_RATE_MCS13:
        case NET_IF_WIFI_DATA_RATE_MCS14:
        case NET_IF_WIFI_DATA_RATE_MCS15:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_DATA_RATE;
             return;
    }

    switch (security_type) {
        case NET_IF_WIFI_SECURITY_OPEN:
        case NET_IF_WIFI_SECURITY_WEP:
        case NET_IF_WIFI_SECURITY_WPA:
        case NET_IF_WIFI_SECURITY_WPA2:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_SECURITY;
             return;
    }

    switch (pwr_level) {
        case NET_IF_WIFI_PWR_LEVEL_LO:
        case NET_IF_WIFI_PWR_LEVEL_MED:
        case NET_IF_WIFI_PWR_LEVEL_HI:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_PWR_LEVEL;
             return;
    }
#endif

    Net_GlobalLockAcquire((void *)&NetIF_WiFi_Join, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }


    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }


    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;

    Mem_Clr(&ap_cfg, sizeof(ap_cfg));
    ap_cfg.NetType      = net_type;
    ap_cfg.DataRate     = data_rate;
    ap_cfg.SecurityType = security_type;
    ap_cfg.PwrLevel     = pwr_level;
    ap_cfg.SSID         = ssid;
    ap_cfg.PSK          = psk;
    ap_cfg.Ch           = NET_IF_WIFI_CH_ALL;

    p_mgr_api->AP_Join(p_if, &ap_cfg, p_err);
    switch (*p_err) {
        case NET_WIFI_MGR_ERR_NONE:
             p_if->Link = NET_IF_LINK_UP;
            *p_err      = NET_IF_WIFI_ERR_NONE;
             break;

        default:
             break;
    }

    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                      NetIF_WiFi_CreateAP()
*
* Description : Create a wireless access point.
*
* Argument(s) : if_nbr              Wireless network interface number.
*
*               net_type            Wireless network type:
*
*                                       NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE
*                                       NET_IF_WIFI_NET_TYPE_ADHOC
*
*               data_rate           Wireless date rate to configure:
*
*                                       NET_IF_WIFI_DATA_RATE_AUTO
*                                       NET_IF_WIFI_DATA_RATE_1_MBPS
*                                       NET_IF_WIFI_DATA_RATE_2_MBPS
*                                       NET_IF_WIFI_DATA_RATE_5_5_MBPS
*                                       NET_IF_WIFI_DATA_RATE_6_MBPS
*                                       NET_IF_WIFI_DATA_RATE_9_MBPS
*                                       NET_IF_WIFI_DATA_RATE_11_MBPS
*                                       NET_IF_WIFI_DATA_RATE_12_MBPS
*                                       NET_IF_WIFI_DATA_RATE_18_MBPS
*                                       NET_IF_WIFI_DATA_RATE_24_MBPS
*                                       NET_IF_WIFI_DATA_RATE_36_MBPS
*                                       NET_IF_WIFI_DATA_RATE_48_MBPS
*                                       NET_IF_WIFI_DATA_RATE_54_MBPS
*                                       NET_IF_WIFI_DATA_RATE_MCS0
*                                       NET_IF_WIFI_DATA_RATE_MCS1
*                                       NET_IF_WIFI_DATA_RATE_MCS2
*                                       NET_IF_WIFI_DATA_RATE_MCS3
*                                       NET_IF_WIFI_DATA_RATE_MCS4
*                                       NET_IF_WIFI_DATA_RATE_MCS5
*                                       NET_IF_WIFI_DATA_RATE_MCS6
*                                       NET_IF_WIFI_DATA_RATE_MCS7
*                                       NET_IF_WIFI_DATA_RATE_MCS8
*                                       NET_IF_WIFI_DATA_RATE_MCS9
*                                       NET_IF_WIFI_DATA_RATE_MCS10
*                                       NET_IF_WIFI_DATA_RATE_MCS11
*                                       NET_IF_WIFI_DATA_RATE_MCS12
*                                       NET_IF_WIFI_DATA_RATE_MCS13
*                                       NET_IF_WIFI_DATA_RATE_MCS14
*                                       NET_IF_WIFI_DATA_RATE_MCS15
*
*
*               security_type       Wireless security type:
*
*                                       NET_IF_WIFI_SECURITY_OPEN
*                                       NET_IF_WIFI_SECURITY_WEP
*                                       NET_IF_WIFI_SECURITY_WPA
*                                       NET_IF_WIFI_SECURITY_WPA2
*
*
*               pwr_level           Wireless radio power to configure:
*
*                                       NET_IF_WIFI_PWR_LEVEL_LO
*                                       NET_IF_WIFI_PWR_LEVEL_MED
*                                       NET_IF_WIFI_PWR_LEVEL_HI
*
*
*               ch                  Channel of the wireless network to create:
*
*                                       NET_IF_WIFI_CH_1
*                                       NET_IF_WIFI_CH_2
*                                       NET_IF_WIFI_CH_3
*                                       NET_IF_WIFI_CH_4
*                                       NET_IF_WIFI_CH_5
*                                       NET_IF_WIFI_CH_6
*                                       NET_IF_WIFI_CH_7
*                                       NET_IF_WIFI_CH_8
*                                       NET_IF_WIFI_CH_9
*                                       NET_IF_WIFI_CH_10
*                                       NET_IF_WIFI_CH_11
*                                       NET_IF_WIFI_CH_12
*                                       NET_IF_WIFI_CH_13
*                                       NET_IF_WIFI_CH_14
*
*
*               ssid                SSID of the access point to create.
*
*               psk                 Pre shared key of the access point.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Wireless network interface module
*                                                                       successfully created.
*
*                               NET_IF_WIFI_ERR_CREATE              Wireless network interface module
*                                                                       access point create failed.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_WiFi_CreateAP() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*********************************************************************************************************
*/

void  NetIF_WiFi_CreateAP    (NET_IF_NBR                  if_nbr,
                              NET_IF_WIFI_NET_TYPE        net_type,
                              NET_IF_WIFI_DATA_RATE       data_rate,
                              NET_IF_WIFI_SECURITY_TYPE   security_type,
                              NET_IF_WIFI_PWR_LEVEL       pwr_level,
                              NET_IF_WIFI_CH              ch,
                              NET_IF_WIFI_SSID            ssid,
                              NET_IF_WIFI_PSK             psk,
                              NET_ERR                    *p_err)
{
    NET_IF               *p_if;
    NET_WIFI_MGR_API     *p_mgr_api;
    NET_IF_WIFI_AP_CFG    ap_cfg;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ------------------- VALIDATE ARG ------------------- */
    switch (data_rate) {
        case NET_IF_WIFI_DATA_RATE_AUTO:
        case NET_IF_WIFI_DATA_RATE_1_MBPS:
        case NET_IF_WIFI_DATA_RATE_2_MBPS:
        case NET_IF_WIFI_DATA_RATE_5_5_MBPS:
        case NET_IF_WIFI_DATA_RATE_6_MBPS:
        case NET_IF_WIFI_DATA_RATE_9_MBPS:
        case NET_IF_WIFI_DATA_RATE_11_MBPS:
        case NET_IF_WIFI_DATA_RATE_12_MBPS:
        case NET_IF_WIFI_DATA_RATE_18_MBPS:
        case NET_IF_WIFI_DATA_RATE_24_MBPS:
        case NET_IF_WIFI_DATA_RATE_36_MBPS:
        case NET_IF_WIFI_DATA_RATE_48_MBPS:
        case NET_IF_WIFI_DATA_RATE_54_MBPS:
        case NET_IF_WIFI_DATA_RATE_MCS0:
        case NET_IF_WIFI_DATA_RATE_MCS1:
        case NET_IF_WIFI_DATA_RATE_MCS2:
        case NET_IF_WIFI_DATA_RATE_MCS3:
        case NET_IF_WIFI_DATA_RATE_MCS4:
        case NET_IF_WIFI_DATA_RATE_MCS5:
        case NET_IF_WIFI_DATA_RATE_MCS6:
        case NET_IF_WIFI_DATA_RATE_MCS7:
        case NET_IF_WIFI_DATA_RATE_MCS8:
        case NET_IF_WIFI_DATA_RATE_MCS9:
        case NET_IF_WIFI_DATA_RATE_MCS10:
        case NET_IF_WIFI_DATA_RATE_MCS11:
        case NET_IF_WIFI_DATA_RATE_MCS12:
        case NET_IF_WIFI_DATA_RATE_MCS13:
        case NET_IF_WIFI_DATA_RATE_MCS14:
        case NET_IF_WIFI_DATA_RATE_MCS15:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_DATA_RATE;
             return;
    }


    switch (security_type) {
        case NET_IF_WIFI_SECURITY_OPEN:
        case NET_IF_WIFI_SECURITY_WEP:
        case NET_IF_WIFI_SECURITY_WPA:
        case NET_IF_WIFI_SECURITY_WPA2:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_SECURITY;
             return;
    }


    switch (pwr_level) {
        case NET_IF_WIFI_PWR_LEVEL_LO:
        case NET_IF_WIFI_PWR_LEVEL_MED:
        case NET_IF_WIFI_PWR_LEVEL_HI:
             break;


        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_PWR_LEVEL;
             return;
    }


    switch (ch) {
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
             break;


        case NET_IF_WIFI_CH_ALL:
        default:
            *p_err = NET_IF_WIFI_ERR_INVALID_CH;
             return;
    }

#endif

    Net_GlobalLockAcquire((void *)&NetIF_WiFi_CreateAP, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }


    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }


    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;

    Mem_Clr(&ap_cfg, sizeof(ap_cfg));
    ap_cfg.NetType      = net_type;
    ap_cfg.DataRate     = data_rate;
    ap_cfg.SecurityType = security_type;
    ap_cfg.PwrLevel     = pwr_level;
    ap_cfg.SSID         = ssid;
    ap_cfg.PSK          = psk;
    ap_cfg.Ch           = ch;

    p_mgr_api->AP_Create(p_if, &ap_cfg, p_err);
    switch (*p_err) {
        case NET_WIFI_MGR_ERR_NONE:
             p_if->Link = NET_IF_LINK_UP;
            *p_err      = NET_IF_WIFI_ERR_NONE;
             break;


        default:
             p_if->Link = NET_IF_LINK_DOWN;
            *p_err      = NET_IF_WIFI_ERR_CREATE;
             break;
    }

    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                          NetIF_WiFi_Leave()
*
* Description : Leave the access point previously joined.
*
*
* Argument(s) : if_nbr  Wireless network interface number.
*
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Wireless network interface module
*                                                                       successfully left.
*
*                               NET_IF_WIFI_ERR_LEAVE               Wireless network interface module
*                                                                       access point leave failed.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_WiFi_Leave() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

void  NetIF_WiFi_Leave (NET_IF_NBR   if_nbr,
                        NET_ERR     *p_err)
{

    NET_IF            *p_if;
    NET_WIFI_MGR_API  *p_mgr_api;


    Net_GlobalLockAcquire((void *)&NetIF_WiFi_Leave, p_err);
    if (*p_err != NET_ERR_NONE) {
         return;
    }


    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }


    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;

    p_mgr_api->AP_Leave(p_if, p_err);
    switch (*p_err) {
        case NET_WIFI_MGR_ERR_NONE:
             p_if->Link = NET_IF_LINK_DOWN;
            *p_err      = NET_IF_WIFI_ERR_NONE;
             break;

        default:
             break;
    }

    Net_GlobalLockRelease();
}

/*
*********************************************************************************************************
*                                          NetIF_WiFi_GetPeerInfo()
*
* Description : Get the info of peers connected to the access point (When acting as an access point).
*
*
* Argument(s) : if_nbr              Wireless network interface number.
*
*               p_buf_peer          Pointer to the buffer to save the peer information.
*
*               buf_peer_len_max    Length in bytes of p_buf_peer.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       NET_IF_ERR_NONE                     Wireless network interface module
*                                                                           successfully get the peer info.
*
*                                       NET_IF_WIFI_ERR_GET_PEER_INFO       Wireless network interface module
*                                                                           access point get peer info failed.
*
* Return(s)   : Number of peers on the network and that are set in the buffer.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetIF_WiFi_GetPeerInfo() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

CPU_INT16U  NetIF_WiFi_GetPeerInfo (NET_IF_NBR          if_nbr,
                                    NET_IF_WIFI_PEER   *p_buf_peer,
                                    CPU_INT16U          buf_peer_len_max,
                                    NET_ERR            *p_err)
{

    NET_IF            *p_if;
    CPU_INT16U         ctn;
    NET_WIFI_MGR_API  *p_mgr_api;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ------------------- VALIDATE ARG ------------------- */
    if (p_buf_peer == DEF_NULL) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return (0u);
    }
#endif

    Net_GlobalLockAcquire((void *)&NetIF_WiFi_GetPeerInfo, p_err);
    if (*p_err != NET_ERR_NONE) {
         return (0u);
    }

    p_if = NetIF_Get(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return (0u);
    }

    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;

    ctn = p_mgr_api->AP_GetPeerInfo(p_if,
                                    p_buf_peer,
                                    buf_peer_len_max,
                                    p_err);
    switch (*p_err) {

        case NET_WIFI_MGR_ERR_NONE:
            *p_err = NET_IF_WIFI_ERR_NONE;
             break;

        default:
            *p_err = NET_IF_WIFI_ERR_GET_PEER_INFO;
             ctn   = 0u;
             break;
    }

    Net_GlobalLockRelease();

    return (ctn);
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
*                                          NetIF_WiFi_Rx()
*
* Description : Process received data packets or wireless management frame.
*
*
* Argument(s) : p_if        Pointer to an network interface that received a packet.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_buf       Pointer to a network buffer that received a packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                                                               ----- RETURNED BY NetIF_WiFi_RxPktHandler() : -----
*                               NET_IF_ERR_NONE                 802x packet successfully received & processed.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*
*                                                               - RETURNED BY NetIF_WiFi_RxMgmtFrameHandler() : --
*                               NET_IF_ERR_NONE                 Wireless management frame successfully received.
*
*                                                               ---- RETURNED BY NetIF_WiFI_RxPktDiscard() : -----
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pif_api->Rx()'.
*
* Note(s)     : (1) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_Rx (NET_IF   *p_if,
                             NET_BUF  *p_buf,
                             NET_ERR  *p_err)
{
    NET_IF_WIFI_FRAME_TYPE  *p_frame_type;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.IF_802xCtrs.NullPtrCtr);
       *p_err = NET_ERR_RX;
        return;
    }
#endif


                                                                /* --------------- DEMUX WIFI PKT/FRAME --------------- */
    p_frame_type = (NET_IF_WIFI_FRAME_TYPE *)p_buf->DataPtr;
    switch (*p_frame_type) {
        case NET_IF_WIFI_DATA_PKT:
             NetIF_WiFi_RxPktHandler(p_if, p_buf, p_err);
             break;


        case NET_IF_WIFI_MGMT_FRAME:
             NetIF_WiFi_RxMgmtFrameHandler(p_if, p_buf, p_err);
             break;


        default:
             NetIF_WiFi_RxDiscard(p_buf, p_err);
             break;
    }
}


/*
*********************************************************************************************************
*                                      NetIF_WiFi_RxPktHandler()
*
* Description : (1) Process received data packets & forward to network protocol layers :
*
*                   (a) Validate & demultiplex packet to higher-layer protocols
*                   (b) Update receive statistics
*
*
* Argument(s) : p_if        Pointer to an network interface that received a packet.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_buf       Pointer to a network buffer that received a packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wireless packet successfully received &
*                                                                   processed.
*
*                                                               - RETURNED BY NetIF_WiFI_RxPktDiscard() : -
*                               NET_ERR_RX                      Receive error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pif_api->Rx()'.
*
* Note(s)     : (2) Network buffer already freed by higher layer.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_RxPktHandler (NET_IF   *p_if,
                                       NET_BUF  *p_buf,
                                       NET_ERR  *p_err)
{
    NET_CTR_IF_802x_STATS  *p_ctrs_stat;
    NET_CTR_IF_802x_ERRS   *p_ctrs_err;


#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_ctrs_stat = &Net_StatCtrs.IFs.WiFi.IF_802xCtrs;
#else
    p_ctrs_stat = (NET_CTR_IF_802x_STATS *)0;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    p_ctrs_err  = &Net_ErrCtrs.IFs.WiFi.IF_802xCtrs;
#else
    p_ctrs_err  = (NET_CTR_IF_802x_ERRS *)0;
#endif


                                                                /* ------------------- RX WIFI PKT -------------------- */
    NetIF_802x_Rx(p_if,
                 p_buf,
                 p_ctrs_stat,
                 p_ctrs_err,
                 p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.WiFi.RxPktCtr);
             break;


        case NET_ERR_RX:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.RxPktDisCtr);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        default:
             NetIF_WiFi_RxDiscard(p_buf, p_err);
             break;
    }
}


/*
*********************************************************************************************************
*                                   NetIF_WiFi_RxMgmtFrameHandler()
*
* Description : Demultiplex management wireless frame.
*
*
* Argument(s) : p_if        Pointer to an network interface that received a packet.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_buf       Pointer to a network buffer that received a packet.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wireless packet successfully demultiplexed.
*
*                                                               - RETURNED BY NetIF_WiFI_RxPktDiscard() : -
*                               NET_ERR_RX                      Receive error; frame discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pif_api->Rx()'.
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
*               (3) Network buffer already freed by higher layer; only increment error counter.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_RxMgmtFrameHandler (NET_IF   *p_if,
                                             NET_BUF  *p_buf,
                                             NET_ERR  *p_err)
{
    NET_DEV_API_IF_WIFI  *p_dev_api;


    if (p_if->Init != DEF_YES) {
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.RxMgmtDisCtr);
        NetIF_WiFi_RxDiscard(p_buf, p_err);
        return;
    }


                                                                /* -------------- DEMUX WIFI MGMT FRAME --------------- */
    NET_CTR_STAT_INC(Net_StatCtrs.IFs.WiFi.RxMgmtCtr);
    p_dev_api = (NET_DEV_API_IF_WIFI *)p_if->Dev_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_IF_WIFI *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_dev_api->Rx == (void (*)(NET_IF      *,
                                   CPU_INT08U **,
                                   CPU_INT16U  *,
                                   NET_ERR     *))0) {
        NetIF_WiFi_RxDiscard(p_buf, p_err);
        return;
    }
#endif


    p_dev_api->MgmtDemux(p_if, p_buf, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.RxMgmtDisCtr);
         NetIF_WiFi_RxDiscard(p_buf, p_err);
         return;
    }

    NET_CTR_STAT_INC(Net_StatCtrs.IFs.WiFi.RxMgmtCompCtr);
   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetIF_WiFi_RxPktDiscard()
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
* Caller(s)   : NetIF_WiFi_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_RxDiscard (NET_BUF  *p_buf,
                                    NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IFs.WiFi.RxDisCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif
   (void)NetBuf_FreeBuf(p_buf, p_ctr);

   *p_err = NET_ERR_RX;
}


/*
*********************************************************************************************************
*                                          NetIF_WiFi_Tx()
*
* Description : Prepare data packets from network protocol layers for Wireless transmit.
*
* Argument(s) : p_if        Pointer to a network interface to transmit data packet(s).
*               ----        Argument validated in NetIF_TxHandler().
*
*               p_buf       Pointer to network buffer with data packet to transmit.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wireless packet successfully prepared
*                                                                   for transmission.
*                               NET_IF_ERR_TX_ADDR_PEND         Wireless packet successfully prepared
*                                                                   & queued for later transmission.
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pif_api->Tx()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_Tx (NET_IF   *p_if,
                             NET_BUF  *p_buf,
                             NET_ERR  *p_err)
{
    NET_CTR_IF_802x_STATS  *p_ctrs_stat;
    NET_CTR_IF_802x_ERRS   *p_ctrs_err;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        NetIF_WiFi_TxPktDiscard(p_buf, p_err);
        NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.TxNullPtrCtr);
       *p_err = NET_ERR_TX;
        return;
    }
#endif

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_ctrs_stat = &Net_StatCtrs.IFs.WiFi.IF_802xCtrs;
#else
    p_ctrs_stat = (NET_CTR_IF_802x_STATS *)0;
#endif

#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctrs_err  = &Net_ErrCtrs.IFs.WiFi.IF_802xCtrs;
#else
    p_ctrs_err  = (NET_CTR_IF_802x_ERRS *)0;
#endif

                                                                /* --------------- PREPARE WIFI TX PKT ---------------- */
    NetIF_802x_Tx(p_if,
                  p_buf,
                  p_ctrs_stat,
                  p_ctrs_err,
                  p_err);
    switch (*p_err) {
        case NET_IF_ERR_NONE:
             NET_CTR_STAT_INC(Net_StatCtrs.IFs.WiFi.TxPktCtr);
             break;


        case NET_IF_ERR_TX_ADDR_PEND:
             break;


        case NET_ERR_TX:
             NET_CTR_ERR_INC(Net_ErrCtrs.IFs.WiFi.TxPktDisCtr);
             break;


        case NET_ERR_FAULT_NULL_PTR:
        default:
             NetIF_WiFi_TxPktDiscard(p_buf, p_err);
             break;
    }
}


/*
*********************************************************************************************************
*                                      NetIF_WiFi_TxPktDiscard()
*
* Description : On any Wireless transmit error(s), discard packet & buffer.
*
* Argument(s) : p_buf       Pointer to network buffer.
*               -----       Argument checked in NetIF_WiFi_Tx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ERR_TX                      Transmit error; packet discarded.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_Tx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_TxPktDiscard (NET_BUF  *p_buf,
                                       NET_ERR  *p_err)
{
    NET_CTR  *p_ctr;


#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    p_ctr = (NET_CTR *)&Net_ErrCtrs.IFs.WiFi.TxPktDisCtr;
#else
    p_ctr = (NET_CTR *) 0;
#endif

   (void)NetBuf_FreeBuf(p_buf, p_ctr);

   *p_err = NET_ERR_TX;
}


/*
*********************************************************************************************************
*                                         NetIF_WiFi_IF_Add()
*
* Description : (1) Add & initialize an Wireless network interface :
*
*                   (a) Validate   Wireless device configuration
*                   (b) Initialize Wireless device data area
*                   (c) Perform    Wireless/OS initialization
*                   (d) Initialize Wireless device hardware MAC address
*                   (e) Initialize Wireless device hardware
*                   (f) Initialize Wireless device MTU
*                   (g) Configure  Wireless interface
*
*
* Argument(s) : p_if        Pointer to Wireless network interface to add.
*               ----        Argument validated in NetIF_Add().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                     Wireless interface successfully added.
*                               NET_ERR_FAULT_MEM_ALLOC             Insufficient resources available to add
*                                                                       Wireless interface.
*                               NET_IF_ERR_INVALID_CFG              Invalid/NULL network interface API configuration.
*                               NET_ERR_FAULT_NULL_FNCT                Invalid NULL network interface function pointer.
*
*                               NET_DEV_ERR_INVALID_CFG             Invalid/NULL network device    API configuration.
*                               NET_ERR_FAULT_NULL_FNCT               Invalid NULL network device    function pointer.
*
*                                                                   ------- RETURNED BY NetDev_Init() : ----------
*                                                                   See NetDev_Init() for addtional return error codes.
*
*                                                                   ----- RETURNED BY 'p_dev_api->Init()' : ------
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
*               (4) Upon adding an Wireless interface, the highest possible Wireless MTU is configured.
*                   If this value needs to be changed, either prior to starting the interface, or during
*                   run-time, it may be reconfigured by calling NetIF_MTU_Set() from the application.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_IF_Add (NET_IF   *p_if,
                                 NET_ERR  *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    CPU_BOOLEAN           flags_invalid;
#endif
    NET_DEV_CFG_WIFI     *p_dev_cfg;
    NET_DEV_API_IF_WIFI  *p_dev_api;
    NET_IF_DATA_WIFI     *p_if_data;
    NET_WIFI_MGR_API     *p_mgr_api;
    void                 *p_addr_hw;
    CPU_SIZE_T            reqd_octets;
    NET_BUF_SIZE          buf_size_max;
    NET_MTU               mtu_max;
    LIB_ERR               err_lib;


    p_dev_cfg = (NET_DEV_CFG_WIFI *)p_if->Dev_Cfg;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* ----------------- VALIDATE DEV CFG ----------------- */
    flags_invalid =  DEF_BIT_IS_SET_ANY(p_dev_cfg->Flags,
                  ~((NET_DEV_CFG_FLAGS)NET_DEV_CFG_FLAG_MASK));
    if (flags_invalid == DEF_YES) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

    if (p_dev_cfg->RxBufIxOffset < NET_IF_WIFI_CFG_RX_BUF_IX_OFFSET_MIN) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }

                                                                /* ---------------- VALIDATE WIFI MGR ----------------- */
    if (p_if->Ext_API == (NET_WIFI_MGR_API *)0) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        goto exit;
    }
#endif

                                                                /* ------------------- CFG WIFI IF -------------------- */
    p_if->Type  = NET_IF_TYPE_WIFI;                             /* Set IF type to WiFi.                                 */


    NetIF_BufPoolInit(p_if, p_err);                             /* Init IF's buf pools.                                 */
    if (*p_err != NET_IF_ERR_NONE) {                            /* On any err(s);                                  ...  */
         goto exit;
    }

                                                                /* ------------- INIT WIFI DEV DATA AREA -------------- */
    p_if->IF_Data = Mem_HeapAlloc(sizeof(NET_IF_DATA_WIFI),
                                  sizeof(void *),
                                 &reqd_octets,
                                 &err_lib);
    if (p_if->IF_Data == (void *)0) {
       *p_err = NET_ERR_FAULT_MEM_ALLOC;
        goto exit;
    }

    p_if_data = (NET_IF_DATA_WIFI *)p_if->IF_Data;



                                                                /* --------------- INIT IF HW/MAC ADDR ---------------- */
    p_addr_hw = &p_if_data->HW_Addr[0];
    Mem_Clr((void     *)p_addr_hw,                              /* Clr hw addr (see Note #2).                           */
            (CPU_SIZE_T)NET_IF_802x_ADDR_SIZE);


                                                                /* ------------------ INIT WIFI MGR ------------------- */
    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_mgr_api == (NET_WIFI_MGR_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        goto exit;
    }
    if (p_mgr_api->Init == (void (*)(NET_IF  *,
                                     NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        goto exit;
    }
#endif

    p_mgr_api->Init(p_if, p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         goto exit;
    }
                                                                /* ------------------- INIT DEV HW -------------------- */
    p_dev_api = (NET_DEV_API_IF_WIFI *)p_if->Dev_API;
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_IF_WIFI *)0) {
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
    p_if->MTU    = mtu_max;                                     /* Set WiFi MTU (see Note #4).                          */




   *p_err = NET_IF_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                       NetIF_WiFi_IF_Start()
*
* Description : (1) Start an Wireless Network Interface :
*                   (a) Start WiFi manager
*                   (b) Start WiFi device
*
*
* Argument(s) : p_if        Pointer to Wireless network interface to start.
*               ----        Argument validated in NetIF_Start().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wireless interface successfully started.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               --- RETURNED BY 'pphy_api->Init()' : ---
*                               NET_ERR_FAULT_NULL_PTR            Argument(s) passed a NULL pointer.
*                               NET_PHY_ERR_TIMEOUT_RESET       Phy reset          time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_RD      Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR      Phy register write time-out.
*
*                                                               -- RETURNED BY 'p_dev_api->Start()' : --
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
*               (3) The MII register block remains enabled while the Phy PWRDOWN bit is set.  Thus, all
*                   parameters may be configured PRIOR to enabling the analog portions of the Phy logic.
*
*               (4) If the Phy enable or link state get functions return an error, they may be ignored
*                   since the Phy may be enabled by default after reset, & the link may become established
*                   at a later time.
*********************************************************************************************************
*/

static  void  NetIF_WiFi_IF_Start (NET_IF   *p_if,
                                   NET_ERR  *p_err)
{
    NET_DEV_API_IF_WIFI  *p_dev_api;
    NET_WIFI_MGR_API     *p_mgr_api;


    p_dev_api = (NET_DEV_API_IF_WIFI *)p_if->Dev_API;
    p_mgr_api = (NET_WIFI_MGR_API    *)p_if->Ext_API;

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_IF_WIFI *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_dev_api->Start == (void (*)(NET_IF  *,
                                      NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }


    if (p_mgr_api == (NET_WIFI_MGR_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_mgr_api->Start == (void (*)(NET_IF  *,
                                      NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

    p_mgr_api->Start(p_if, p_err);                              /* Start wifi mgr.                                      */
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return;
    }

    p_dev_api->Start(p_if, p_err);                              /* Start dev.                                           */
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    p_if->Link = NET_IF_LINK_DOWN;


   *p_err = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetIF_WiFi_IF_Stop()
*
* Description : (1) Stop Specific Network Interface :
*
*                   (a) Stop Wireless device
*                   (b) Stop Wireless physical layer, if available
*
*
* Argument(s) : p_if        Pointer to Wireless network interface to stop.
*               ----        Argument validated in NetIF_Stop().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wirless interface successfully stopped.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*
*                                                               -- RETURNED BY 'p_dev_api->Stop()' : ---
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

static  void  NetIF_WiFi_IF_Stop (NET_IF   *p_if,
                                  NET_ERR  *p_err)
{
    NET_DEV_API_IF_WIFI  *p_dev_api;
    NET_WIFI_MGR_API     *p_mgr_api;


    p_dev_api = (NET_DEV_API_IF_WIFI *)p_if->Dev_API;
    p_mgr_api = (NET_WIFI_MGR_API    *)p_if->Ext_API;

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_dev_api == (NET_DEV_API_IF_WIFI *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_dev_api->Stop == (void (*)(NET_IF  *,
                                     NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }

    if (p_mgr_api == (NET_WIFI_MGR_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_mgr_api->Stop == (void (*)(NET_IF  *,
                                     NET_ERR *))0) {
       *p_err = NET_ERR_FAULT_NULL_FNCT;
        return;
    }
#endif

    p_dev_api->Stop(p_if, p_err);                               /* Stop dev.                                            */
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }

    p_mgr_api->Stop(p_if, p_err);                               /* Stop wifi mgr.                                       */
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return;
    }

    p_if->Link = NET_IF_LINK_DOWN;

   *p_err      = NET_IF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetIF_WiFi_IO_CtrlHandler()
*
* Description : Handle an Wireless interface's (I/O) control(s).
*
* Argument(s) : p_if        Pointer to an Wireless network interface.
*               ----        Argument validated in NetIF_IO_CtrlHandler().
*
*               opt         Desired I/O control option code to perform; additional control options may be
*                           defined by the device driver :
*
*                               NET_IF_IO_CTRL_LINK_STATE_GET           Get    Wireless interface's link state,
*                                                                           'UP' or 'DOWN'.
*                               NET_IF_IO_CTRL_LINK_STATE_GET_INFO      Get    Wireless interface's detailed
*                                                                           link state info.
*                               NET_IF_IO_CTRL_LINK_STATE_UPDATE        Update Wireless interface's link state.
*
*               p_data      Pointer to variable that will receive possible I/O control data (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_IF_ERR_NONE                 Wireless I/O control option successfully
*                                                                   handled.
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*
*                                                               -- RETURNED BY 'p_dev_api->IO_Ctrl()' : --
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

static  void  NetIF_WiFi_IO_CtrlHandler (NET_IF      *p_if,
                                         CPU_INT08U   opt,
                                         void        *p_data,
                                         NET_ERR     *p_err)
{
    CPU_BOOLEAN        *p_link_state;
    NET_DEV_LINK_WIFI   link_info;
    NET_WIFI_MGR_API   *p_mgr_api;


    p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;

                                                                /* ------------ VALIDATE EXT API I/O PTRS ------------- */
#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
    if (p_data == (void *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_mgr_api == (NET_WIFI_MGR_API *)0) {
       *p_err = NET_IF_ERR_INVALID_CFG;
        return;
    }
    if (p_mgr_api->IO_Ctrl == (void (*)(NET_IF   *,
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
             p_mgr_api->IO_Ctrl((NET_IF   *) p_if,
                                (CPU_INT08U) NET_IF_IO_CTRL_LINK_STATE_GET,
                                (void     *)&link_info,         /* Get link state info.                                 */
                                (NET_ERR  *) p_err);

             p_link_state = (CPU_BOOLEAN *)p_data;              /* See Note #1.                                         */

             if (*p_err       != NET_WIFI_MGR_ERR_NONE) {
                 *p_link_state = NET_IF_LINK_DOWN;
                  return;
             }

            *p_link_state = link_info.LinkState;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
                                                                /* Rtn err for unavail ctrl opt?                        */
             break;


        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
        default:                                                /* Handle other dev I/O opt(s).                         */
             p_mgr_api->IO_Ctrl((NET_IF   *)p_if,
                                (CPU_INT08U)opt,
                                (void     *)p_data,             /* See Note #1.                                         */
                                (NET_ERR  *)p_err);
             if (*p_err != NET_WIFI_MGR_ERR_NONE) {
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

#endif  /* NET_IF_WIFI_MODULE_EN */

