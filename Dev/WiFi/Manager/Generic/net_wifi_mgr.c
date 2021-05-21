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
*                                       NETWORK WIRELESS MANAGER
*
*                                              WIFI MANAGER
*
* Filename : net_wifi_mgr.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.12.00 (or more recent version) is included in the project build.
*
*            (2) The wireless hardware used with this wireless manager is assumed to embed the wireless
*                supplicant within the wireless hardware and provide common command.
*
*            (3) Interrupt support is hardware specific, therefore the generic wireless manager does NOT
*                support interrupts.  However, interrupt support is easily added to the generic Wireless
*                manager & thus the ISR handler has been prototyped & populated within the function
*                pointer structure for example purposes.

*            (4) REQUIREs the following network protocol files in network directories :
*
*                (a) (1) Wireless Network Interface Layer
*
*                      Located in the following network directory
*
*                          \<Network Protocol Suite>\IF\
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_WIFI_MGR_MODULE
#include  "../../../../IF/net_if.h"
#include  "../../../../IF/net_if_wifi.h"
#include  "net_wifi_mgr.h"

#ifdef  NET_IF_WIFI_MODULE_EN

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  NET_WIFI_MGR_LOCK_OBJ_NAME          "WiFi Mgr Lock"
#define  NET_WIFI_MGR_RESP_OBJ_NAME          "WiFi Mgr MGMT Response"


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void         NetWiFiMgr_Init           (       NET_IF               *p_if,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_Start          (       NET_IF               *p_if,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_Stop           (       NET_IF               *p_if,
                                                       NET_ERR              *p_err);

static  CPU_INT16U   NetWiFiMgr_AP_Scan        (       NET_IF               *p_if,
                                                       NET_IF_WIFI_AP       *p_buf_scan,
                                                       CPU_INT16U            scan_len_max,
                                                const  NET_IF_WIFI_SSID     *p_ssid,
                                                       NET_IF_WIFI_CH        ch,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_AP_Join        (       NET_IF               *p_if,
                                                const  NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_AP_Leave       (       NET_IF               *p_if,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_IO_Ctrl        (       NET_IF               *p_if,
                                                       CPU_INT08U            opt,
                                                       void                 *p_data,
                                                       NET_ERR              *p_err);

static  CPU_INT32U   NetWiFiMgr_Mgmt           (       NET_IF               *p_if,
                                                       NET_IF_WIFI_CMD       cmd,
                                                       CPU_INT08U           *p_buf_cmd,
                                                       CPU_INT16U            buf_cmd_len,
                                                       CPU_INT08U           *p_buf_rtn,
                                                       CPU_INT16U            buf_rtn_len_max,
                                                       NET_ERR              *p_err);


static  CPU_INT32U   NetWiFiMgr_MgmtHandler    (       NET_IF               *p_if,
                                                       NET_IF_WIFI_CMD       cmd,
                                                       CPU_INT08U           *p_buf_cmd,
                                                       CPU_INT16U            buf_cmd_len,
                                                       CPU_INT08U           *p_buf_rtn,
                                                       CPU_INT16U            buf_rtn_len_max,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_Signal         (       NET_IF               *p_if,
                                                       NET_BUF              *p_buf,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_LockAcquire    (       KAL_LOCK_HANDLE       lock,
                                                       NET_ERR              *p_err);

static  void         NetWiFiMgr_LockRelease    (       KAL_LOCK_HANDLE       lock);

static  void         NetWiFiMgr_AP_Create      (       NET_IF                *p_if,
                                                const  NET_IF_WIFI_AP_CFG    *p_ap_cfg,
                                                       NET_ERR               *p_err);

static  CPU_INT16U   NetWiFiMgr_AP_GetPeerInfo (       NET_IF                *p_if,
                                                const  NET_IF_WIFI_PEER      *p_buf_peer,
                                                       CPU_INT16U             peer_info_len_max,
                                                       NET_ERR               *p_err);

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/
                                                                                /* WiFi Mgr API fnct ptrs :             */
const  NET_WIFI_MGR_API  NetWiFiMgr_API_Generic = {
                                                   &NetWiFiMgr_Init,            /*   Init/add                           */
                                                   &NetWiFiMgr_Start,           /*   Start                              */
                                                   &NetWiFiMgr_Stop,            /*   Stop                               */
                                                   &NetWiFiMgr_AP_Scan,         /*   Scan                               */
                                                   &NetWiFiMgr_AP_Join,         /*   Join                               */
                                                   &NetWiFiMgr_AP_Leave,        /*   Leave                              */
                                                   &NetWiFiMgr_IO_Ctrl,         /*   IO Ctrl                            */
                                                   &NetWiFiMgr_Mgmt,            /*   Mgmt                               */
                                                   &NetWiFiMgr_Signal,          /*   Signal                             */
                                                   &NetWiFiMgr_AP_Create,       /*   Create                             */
                                                   &NetWiFiMgr_AP_GetPeerInfo,  /*   GetClientInfo                      */
                                                  };


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          NetWiFiMgr_Init()
*
* Description : (1) Initialize wireless manager layer.
*
*                   (a) Allocate memory for manager object
*                   (b) Initalize wireless  manager lock
*                   (c) Initalize wireless  manager Response Signal
*
*
* Argument(s) : p_if    Pointer to interface to initialize Wifi manager.
*               ----    Argument validated in NetIF_IF_Add().
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE           Wireless manager        successfully      initialized.
*                                   NET_WIFI_MGR_ERR_MEM_ALLOC      Wireless manager memory allocatation      failed.
*
*                                                                   ----- RETURNED BY 'NetOS_WiFiMgr_LockInit()' : -----
*                                   NET_OS_ERR_LOCK_INIT            Wireless manager lock      NOT            initialized.
*                                   NET_OS_ERR_INIT_SIGNAL_NAME     Wireless manager lock name NOT            initialized.
*
*                                                                   ----- RETURNED BY 'NetOS_WiFiMgr_RespInit()' : -----
*                                   NET_OS_ERR_INIT_SIGNAL          Response signal object NOT successfully   initialized.
*                                   NET_OS_ERR_INIT_SIGNAL_NAME     Response signal name   NOT successfully   initialized.
*                                   NET_WIFI_MGR_ERR_INIT_OS        Wireless manager os object    initializati   on failed.
*
* Return(s)   : none
*
* Caller(s)   : NetIF_WiFi_IF_Add() via 'p_mgr_api->Init()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Wireless manager initialization occurs only when the interface is added.
*                       See 'net_if_wifi.c  NetIF_WiFi_IF_Add()'.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_Init(NET_IF   *p_if,
                              NET_ERR  *p_err)
{
    NET_DEV_CFG_WIFI   *p_cfg;
    NET_WIFI_MGR_DATA  *p_mgr_data;
    CPU_SIZE_T          size;
    CPU_SIZE_T          reqd_octets;
    KAL_ERR             kal_err;
    LIB_ERR             err_lib;



   *p_err = NET_WIFI_MGR_ERR_NONE;


                                                                /* ----------- ALLOCATE WIFI MGR DATA AREA ------------ */
    size           =  sizeof(NET_WIFI_MGR_DATA);
    p_if->Ext_Data =  Mem_HeapAlloc(size,
                                   (CPU_SIZE_T  ) sizeof(void *),
                                   &reqd_octets,
                                   &err_lib);
    if (p_if->Ext_Data == (void *)0) {
       *p_err = NET_WIFI_MGR_ERR_MEM_ALLOC;
        goto exit;
    }

    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;


                                                                /* ---------------- INIT WIFI MGR LOCK ---------------- */
    p_mgr_data->MgrLock = KAL_LockCreate(NET_WIFI_MGR_LOCK_OBJ_NAME, DEF_NULL, &kal_err);
    switch (kal_err) {
        case KAL_ERR_NONE:
             break;

        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_WIFI_MGR_ERR_MEM_ALLOC;
             goto exit;

        case KAL_ERR_CREATE:
        default:
            *p_err = NET_WIFI_MGR_ERR_LOCK_CREATE;
             goto exit;
    }


    p_cfg = (NET_DEV_CFG_WIFI *)p_if->Dev_Cfg;

                                                                /* --------------- INIT WIFI MGR SIGNAL --------------- */
    p_mgr_data->MgmtSignalResp = KAL_QCreate(NET_WIFI_MGR_RESP_OBJ_NAME,
                                             p_cfg->RxBufLargeNbr,
                                             DEF_NULL,
                                            &kal_err);
    switch (kal_err) {
        case KAL_ERR_NONE:
             break;

        case KAL_ERR_MEM_ALLOC:
            *p_err = NET_WIFI_MGR_ERR_MEM_ALLOC;
             goto exit_delete_lock;

        case KAL_ERR_CREATE:
        default:
            *p_err = NET_WIFI_MGR_ERR_RESP_SIGNAL_CREATE;
             goto exit_delete_lock;
    }


    p_mgr_data->DevStarted = DEF_NO;
    p_mgr_data->AP_Joined  = DEF_NO;
    p_mgr_data->AP_Created = DEF_NO;

    goto exit;


exit_delete_lock:
    KAL_LockDel(p_mgr_data->MgrLock, &kal_err);

exit:
    return;
}


/*
*********************************************************************************************************
*                                          NetWiFiMgr_Start()
*
* Description : Start wireless manager of the interface.
*
* Argument(s) : p_if    Pointer to interface to Start wireless manager.
*               ----    Argument validated in NetIF_IF_Start().
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                               NET_WIFI_MGR_ERR_NONE           Wireless manager for the interface successfully
*                                                                   started.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Start() via 'p_mgr_api->Start()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Wireless manager start occurs each time the interface is started.
*                       See 'net_if_wifi.c  NetIF_WiFi_IF_Start()'.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_Start (NET_IF   *p_if,
                                NET_ERR  *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;


    p_mgr_data             = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    p_mgr_data->DevStarted =  DEF_YES;


   *p_err = NET_WIFI_MGR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetWiFiMgr_Stop()
*
* Description : Shutdown wireless manager of the interface.
*
* Argument(s) : p_if    Pointer to interface to Start Wifi manager.
*               ----    Argument validated in NetIF_Stop().
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                               NET_WIFI_MGR_ERR_NONE           Wireless manager for the interface successfully
*                                                                   stopped.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Stop() via 'p_mgr_api->Stop()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Wireless manager stop occurs each time the interface is stopped.
*                       See 'net_if_wifi.c  NetIF_WiFi_IF_Stop()'.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_Stop (NET_IF   *p_if,
                               NET_ERR  *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;


    p_mgr_data             = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    p_mgr_data->DevStarted =  DEF_NO;


   *p_err = NET_WIFI_MGR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetWiFiMgr_AP_Scan()
*
* Description : (1) Scan for available wireless network by the interface:
*
*                   (a) Release network          lock
*                   (b) Acquire wireless manager lock
*                   (c) Acquire network          lock
*                   (d) Send scan command and get result
*                   (e) Release network          lock
*                   (f) Release wireless manager lock
*
*
* Argument(s) : p_if            Pointer to interface to Scan with.
*               ----            Argument validated in NetIF_WiFi_Scan().
*
*               p_buf_scan      Pointer to table that will receive the return network found.
*
*               scan_len_max    Length of the scan buffer (i.e. Number of network that can be found).
*
*               p_ssid          Pointer to variable that contains the SSID to scan for.
*
*               ch              The wireless channel to scan.
*               --              Argument checked in NetSock_CfgTimeoutTxQ_Get_ms().
*
*                                   NET_IF_WIFI_CH_ALL         Scan Wireless network for all channel.
*                                   NET_IF_WIFI_CH_1           Scan Wireless network on channel 1.
*                                   NET_IF_WIFI_CH_2           Scan Wireless network on channel 2.
*                                   NET_IF_WIFI_CH_3           Scan Wireless network on channel 3.
*                                   NET_IF_WIFI_CH_4           Scan Wireless network on channel 4.
*                                   NET_IF_WIFI_CH_5           Scan Wireless network on channel 5.
*                                   NET_IF_WIFI_CH_6           Scan Wireless network on channel 6.
*                                   NET_IF_WIFI_CH_7           Scan Wireless network on channel 7.
*                                   NET_IF_WIFI_CH_8           Scan Wireless network on channel 8.
*                                   NET_IF_WIFI_CH_9           Scan Wireless network on channel 9.
*                                   NET_IF_WIFI_CH_10          Scan Wireless network on channel 10.
*                                   NET_IF_WIFI_CH_11          Scan Wireless network on channel 11.
*                                   NET_IF_WIFI_CH_12          Scan Wireless network on channel 12.
*                                   NET_IF_WIFI_CH_13          Scan Wireless network on channel 13.
*                                   NET_IF_WIFI_CH_14          Scan Wireless network on channel 14.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*                                   NET_WIFI_MGR_ERR_NOT_STARTED            Wireless manager NOT started.
*
*                                                                           --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                                   NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                           - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.

*
* Return(s)   : Number of wireless network found, if any.
*
*               0,                                otherwise.
*
* Caller(s)   : NetIF_WiFi_IF_Scan() via 'p_mgr_api->Scan()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT16U  NetWiFiMgr_AP_Scan (       NET_IF            *p_if,
                                               NET_IF_WIFI_AP    *p_buf_scan,
                                               CPU_INT16U         scan_len_max,
                                        const  NET_IF_WIFI_SSID  *p_ssid,
                                               NET_IF_WIFI_CH     ch,
                                               NET_ERR           *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    NET_IF_WIFI_SCAN    scan;
    CPU_INT08U         *p_buf_cmd;
    CPU_INT08U         *p_buf_rtn;
    CPU_INT16U          len;
    CPU_INT32U          rtn_len;
    NET_ERR             err;

    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    if (p_mgr_data->DevStarted != DEF_YES) {
       *p_err = NET_WIFI_MGR_ERR_NOT_STARTED;
        return (0);
    }
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return(0);
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_AP_Scan, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return(0);
    }

                                                                /* ----------- SEND SCAN CMD AND GET RESULT ----------- */
    Mem_Clr(&scan.SSID, sizeof(scan.SSID));
    if (p_ssid != (NET_IF_WIFI_SSID *)0) {
        Str_Copy_N((      CPU_CHAR *)&scan.SSID,
                   (const CPU_CHAR *) p_ssid,
                                      sizeof(scan.SSID));
    }

    scan.Ch   =  ch;
    len       =  scan_len_max * sizeof(NET_IF_WIFI_AP);
    p_buf_cmd = (CPU_INT08U *)&scan;
    p_buf_rtn = (CPU_INT08U *) p_buf_scan;
    rtn_len   =  NetWiFiMgr_MgmtHandler(p_if,
                                        NET_IF_WIFI_CMD_SCAN,
                                        p_buf_cmd,
                                        sizeof(scan),
                                        p_buf_rtn,
                                        len,
                                        p_err);

                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);


    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return (0);
    }

    len = rtn_len / sizeof(NET_IF_WIFI_AP);


   *p_err = NET_WIFI_MGR_ERR_NONE;

    return (len);
}


/*
*********************************************************************************************************
*                                         NetWiFiMgr_AP_Join()
*
* Description : Join wireless network:
*
*                   (a) Release network          lock
*                   (b) Acquire wireless manager lock
*                   (c) Acquire network          lock
*                   (d) Send 'Join' command and get result
*                   (e) Release network          lock
*                   (f) Release wireless manager lock
*
*
* Argument(s) : p_if    Pointer to interface to join with.
*               ----    Argument validated in NetIF_WiFi_Join().
*
*               p_join  Pointer to variable that contains the wireless network to join.
*               ------  Argument validated in NetIF_WiFi_Join().
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*                                   NET_WIFI_MGR_ERR_NOT_STARTED            Wireless manager NOT started.
*
*                                                                           --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                                   NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                           - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_Join() via 'p_mgr_api->Join()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_AP_Join (       NET_IF               *p_if,
                                  const  NET_IF_WIFI_AP_CFG   *p_ap_cfg,
                                         NET_ERR              *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    CPU_INT08U         *p_buf_cmd;
    CPU_INT16U          buf_data_len;
    NET_ERR             err;


    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    if (p_mgr_data->DevStarted != DEF_YES) {
       *p_err = NET_WIFI_MGR_ERR_NOT_STARTED;
        return;
    }

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return;
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_AP_Join, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return;
    }
                                                                /* ----------- SEND JOIN CMD AND GET RESULT ----------- */
    buf_data_len = sizeof(NET_IF_WIFI_AP_CFG);
    p_buf_cmd    = (CPU_INT08U *)p_ap_cfg;
    (void)NetWiFiMgr_MgmtHandler(p_if,
                                 NET_IF_WIFI_CMD_JOIN,
                                 p_buf_cmd,
                                 buf_data_len,
                                 0,
                                 0,
                                 p_err);


                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);


    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return;
    }

    p_mgr_data->AP_Joined = DEF_YES;


   *p_err = NET_WIFI_MGR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetWiFiMgr_AP_Leave()
*
* Description : Leave wireless network.
*
*                   (a) Release network          lock
*                   (b) Acquire wireless manager lock
*                   (c) Acquire network          lock
*                   (d) Send 'Leave' command and get result
*                   (e) Release network          lock
*                   (f) Release wireless manager lock
*
*
* Argument(s) : p_if            Pointer to interface to leave wireless network.
*               ----            Argument validated in NetIF_WiFi_Leave().
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*                                   NET_WIFI_MGR_ERR_NOT_STARTED            Wireless manager NOT started.
*
*                                                                           --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                                   NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                           - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_Leave() via 'p_mgr_api->Leave()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_AP_Leave (NET_IF   *p_if,
                                   NET_ERR  *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    NET_ERR             err;

    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    if (p_mgr_data->DevStarted != DEF_YES) {
       *p_err = NET_WIFI_MGR_ERR_NOT_STARTED;
        return;
    }


                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return;
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_AP_Leave, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return;
    }




                                                                /* ---------- SEND LEAVE CMD AND GET RESULT ----------- */
    (void)NetWiFiMgr_MgmtHandler(p_if,
                                 NET_IF_WIFI_CMD_LEAVE,
                                 0,
                                 0,
                                 0,
                                 0,
                                 p_err);


                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);

    if (*p_err == NET_WIFI_MGR_ERR_NONE) {
         p_if->Link             = NET_IF_LINK_DOWN;
         p_mgr_data->AP_Joined  = DEF_NO;
         p_mgr_data->AP_Created = DEF_NO;
    } else {
         return;
    }


   *p_err = NET_WIFI_MGR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetWiFiMgr_IO_Ctrl()
*
* Description : Handle a wireless interface's (I/O) control(s).
*
* Argument(s) : p_if    Pointer to a Wireless network interface.
*               ----    Argument validated in NetIF_IO_CtrlHandler().
*
*               opt     Desired I/O control option code to perform; additional control options may be
*                       defined by the device driver :
*               ---     Argument checked in NetIF_IO_CtrlHandler().
*
*                           NET_IF_IO_CTRL_LINK_STATE_GET           Get    Wireless interface's link state,
*                                                                       'UP' or 'DOWN'.
*                           NET_IF_IO_CTRL_LINK_STATE_GET_INFO      Get    Wireless interface's detailed
*                                                                       link state info.
*                           NET_IF_IO_CTRL_LINK_STATE_UPDATE        Update Wireless interface's link state.
*
*               p_data  Pointer to variable that will receive possible I/O control data (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*                               NET_WIFI_MGR_ERR_NOT_STARTED            Wireless manager NOT started.
*                               NET_IF_ERR_INVALID_IO_CTRL_OPT          Invalid I/O control option.
*                               NET_ERR_FAULT_NULL_PTR                     Argument 'p_data' passed a NULL pointer.
*
*                                                                       --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                               NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                       - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                               NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                               NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                               NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.
* Return(s)   : none.
*
* Caller(s)   : NetIF_IO_CtrlHandler() via 'pif_api->IO_Ctrl()'.
*
* Note(s)     : (1) 'p_data' MUST point to a variable (or memory) that is sufficiently sized AND aligned
*                    to receive any return data.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_IO_Ctrl (NET_IF      *p_if,
                                  CPU_INT08U   opt,
                                  void        *p_data,
                                  NET_ERR     *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    CPU_BOOLEAN        *p_link_state;
    NET_ERR             err;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_data == (void *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif


    p_link_state = (CPU_BOOLEAN *)p_data;
    p_mgr_data   = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;

    if (p_mgr_data->DevStarted != DEF_YES){
       *p_link_state = NET_IF_LINK_DOWN;
       *p_err        = NET_WIFI_MGR_ERR_NOT_STARTED;
        return;
    }

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return;
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_AP_Leave, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return;
    }


                                                                /* ------------- SEND CMD AND GET RESULT -------------- */
    (void)NetWiFiMgr_MgmtHandler(p_if,
                                 opt,
                                 p_data,
                                 0,
                                 p_data,
                                 0,
                                 p_err);


                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);

    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return;
    }


   *p_err = NET_WIFI_MGR_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetWiFiMgr_Mgmt()
*
* Description : (1) Send driver management command:
*
*                   (a) Acquire wireless manager lock
*                   (b) Send management command and get result
*                   (c) Release wireless manager lock
*                   (d) Execute & process management command
*
*
* Argument(s) : p_if                Pointer to interface to manage wireless network.
*               ----                Argument validated in NetIF_IF_Add(),
*                                                         NetIF_IF_Start(),
*                                                         NetIF_RxHandler().
*
*               cmd                 Management command to send.
*
*                                       See Note #2a.
*
*               p_buf_cmd           Pointer to variable that contains the data to send.
*
*               buf_cmd_len         Length of the command buffer.
*
*               p_buf_rtn           Pointer to variable that will receive the data.
*
*               buf_rtn_len_max     Length of the return buffer.
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*
*                                                                           --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                                   NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                           - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.
*
*
* Return(s)   : none.
*
* Caller(s)   : Device driver.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) The driver can define and implement its own management commands which need a response by
*                   calling the wireless manager api (p_mgr_api->Mgmt()) to send the management command and to
*                   receive the response.
*
*                   (a) Driver management command code '100' series reserved for driver.
*
*               (3) Prior calling this function, the network lock must be acquired.
*********************************************************************************************************
*/

static  CPU_INT32U  NetWiFiMgr_Mgmt (NET_IF           *p_if,
                                     NET_IF_WIFI_CMD   cmd,
                                     CPU_INT08U       *p_buf_cmd,
                                     CPU_INT16U        buf_cmd_len,
                                     CPU_INT08U       *p_buf_rtn,
                                     CPU_INT16U        buf_rtn_len_max,
                                     NET_ERR          *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    CPU_INT32U          rtn_len;
    NET_ERR             err;

    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    if (p_mgr_data->DevStarted != DEF_YES) {
       *p_err = NET_WIFI_MGR_ERR_NOT_STARTED;
        return (0u);
    }


                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return(0);
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_Mgmt, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return(0);
    }


                                                                /* ----------- SEND MGMT CMD AND GET RESULT ----------- */
    rtn_len = NetWiFiMgr_MgmtHandler(p_if,
                                     cmd,
                                     p_buf_cmd,
                                     buf_cmd_len,
                                     p_buf_rtn,
                                     buf_rtn_len_max,
                                     p_err);


                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);

    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return (rtn_len);
    }


   *p_err = NET_WIFI_MGR_ERR_NONE;

    return (rtn_len);
}


/*
*********************************************************************************************************
*                                       NetWiFiMgr_MgmtHandler()
*
* Description : (1) Send mgmt command and get result:
*
*                   (a) Ask driver to transmit management command
*                   (b) Release network lock
*                   (b) Wait response
*                   (c) Ask driver to decode the received management response
*                   (d) Free received buffer
*                   (e) Acquire network lock
*
*
* Argument(s) : p_if                Pointer to interface to leave wireless network.
*               ----                Argument validated in NetIF_IF_Add(),
*                                                         NetIF_IF_Start(),
*                                                         NetIF_RxHandler(),
*                                                         NetIF_WiFi_Scan(),
*                                                         NetIF_WiFi_Join(),
*                                                         NetIF_WiFi_Leave().
*
*               cmd                 Management command to send.
*               ----                Argument validated in NetWiFiMgr_AP_Scan(),
*                                                         NetWiFiMgr_AP_Join(),
*                                                         NetWiFiMgr_AP_Leave().
*
*                                       NET_IF_WIFI_CMD_SCAN                    Scan  for available Wireless network.
*                                       NET_IF_WIFI_CMD_JOIN                    Join  a             Wireless network.
*                                       NET_IF_WIFI_CMD_LEAVE                   Leave the           Wireless network.
*                                       NET_IF_IO_CTRL_LINK_STATE_GET           Get    Wireless interface's link state,
*                                                                                   'UP' or 'DOWN'.*
*                                       NET_IF_IO_CTRL_LINK_STATE_GET_INFO      Get    Wireless interface's detailed*               p_buf_data_rx       Pointer to variable that will receive the data.
*                                                                                   link state info.*
*                                       NET_IF_IO_CTRL_LINK_STATE_UPDATE        Update Wireless interface's link state.
*
*
*                                       See 'NetWiFiMgr_Mgmt()' Note 2a.
*
*
*               p_buf_len_rx        Length of the receive buffer.
*
*               p_buf_data_tx       Pointer to variable that contains the data to send.
*
*               buf_data_tx_len     Length of the data to send.
*
*               p_err           Pointer to variable  that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager command successfully
*                                                                               completed.
*
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*
*                                                                           - RETURNED BY 'NetOS_WiFiMgr_RespWait()' : -
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.
*
*
* Return(s)   : none.
*
* Caller(s)   : Net_WiFiMgr_AP_Scan(),
*               Net_WiFiMgr_AP_Join(),
*               Net_WiFiMgr_AP_Leave(),
*               Net_WiFiMgr_Mgmt().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  NetWiFiMgr_MgmtHandler (NET_IF           *p_if,
                                            NET_IF_WIFI_CMD   cmd,
                                            CPU_INT08U       *p_buf_cmd,
                                            CPU_INT16U        buf_cmd_len,
                                            CPU_INT08U       *p_buf_rtn,
                                            CPU_INT16U        buf_rtn_len_max,
                                            NET_ERR          *p_err)
{
    NET_DEV_API_WIFI   *p_dev_api;
    NET_WIFI_MGR_DATA  *p_mgr_data;
    NET_WIFI_MGR_CTX    ctx;
    NET_BUF            *p_buf;
    NET_BUF_HDR        *p_hdr;
    CPU_INT32U          rtn_len;
    CPU_BOOLEAN         done;
    KAL_ERR             kal_err;
    NET_ERR             err;


    p_dev_api  =  (NET_DEV_API_WIFI  *)p_if->Dev_API;
    p_mgr_data =  (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    done       =   DEF_NO;
    rtn_len    =   0u;
    while (done != DEF_YES) {
                                                                /* ------------- PREPARE & SEND MGMT CMD -------------- */
        rtn_len = p_dev_api->MgmtExecuteCmd(p_if,
                                            cmd,
                                           &ctx,
                                            p_buf_cmd,
                                            buf_cmd_len,
                                            p_buf_rtn,
                                            buf_rtn_len_max,
                                            p_err);
        if (*p_err != NET_DEV_ERR_NONE) {
            *p_err  = NET_WIFI_MGR_ERR_CMD_FAULT;
             goto exit;
        }

        if ((*p_err        == NET_DEV_ERR_NONE) &&
            ( ctx.WaitResp == DEF_YES         )) {              /* If the cmd requires a resp.                          */


                                                                /* ----------------- RELEASE NET LOCK ----------------- */
            Net_GlobalLockRelease();                            /* Require to rx pkt & mgmt frame.                      */



                                                                /* -------------------- WAIT RESP --------------------- */
            p_buf = (NET_BUF *)KAL_QPend(p_mgr_data->MgmtSignalResp,
                                         KAL_OPT_PEND_NONE,
                                         ctx.WaitRespTimeout_ms,
                                        &kal_err);
            switch (kal_err) {
                case KAL_ERR_NONE:
                     break;

                case KAL_ERR_TIMEOUT:
                    *p_err = NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT;
                     goto exit_fail_acquire_lock;

                case KAL_ERR_ABORT:
                case KAL_ERR_ISR:
                case KAL_ERR_WOULD_BLOCK:
                case KAL_ERR_OS:
                default:
                    *p_err = NET_WIFI_MGR_ERR_RESP_FAULT;
                     goto exit_fail_acquire_lock;
            }


            p_hdr   = &p_buf->Hdr;

                                                                /* ----------------- RX & DECODE RESP ----------------- */
            rtn_len = p_dev_api->MgmtProcessResp(p_if,
                                                 cmd,
                                                &ctx,
                                                 p_buf->DataPtr,
                                                 p_hdr->DataLen,
                                                 p_buf_rtn,
                                                 buf_rtn_len_max,
                                                 p_err);

                                                                /* ------------------ FREE BUF RX'D ------------------- */
            NetBuf_Free(p_buf);

            if (*p_err != NET_DEV_ERR_NONE) {
                *p_err  = NET_WIFI_MGR_ERR_RESP_FAULT;
                 goto exit_acquire_lock;
            }





                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
            Net_GlobalLockAcquire((void *)&NetWiFiMgr_MgmtHandler, &err);
            if (err != NET_ERR_NONE) {
                rtn_len = 0u;
               *p_err   = err;
                goto exit;
            }
        }

        if ((*p_err             == NET_DEV_ERR_NONE) &&
            ( ctx.MgmtCompleted == DEF_YES         )) {
           *p_err = NET_WIFI_MGR_ERR_NONE;
            goto exit;
        }
    }


exit_fail_acquire_lock:
    rtn_len = 0u;

exit_acquire_lock:
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_MgmtHandler, &err);

exit:
    return (rtn_len);
}


/*
*********************************************************************************************************
*                                         NetWiFiMgr_Signal()
*
* Description : Signal reception of a management response.
*
* Argument(s) : p_if    Pointer to interface to leave wireless network.
*               ----    Argument validated in NetIF_RxHandler().
*
*               p_buf   Pointer to net buffer that contains the management buffer received.
*               -----   Argument checked   in NetIF_RxHandler().
*
*               p_err           Pointer to variable   that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE           Wireless manager for the interface successfully
*                                                                   left.
*
*                                                                   - RETURNED BY 'NetOS_WiFiMgr_RespSignal()' : --
*                                   NET_OS_ERR_RESP                 Post managemment response failed
*
* Return(s)   : none.
*
* Caller(s)   : Device driver via 'p_mgr_api->Signal()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_Signal (NET_IF   *p_if,
                                 NET_BUF  *p_buf,
                                 NET_ERR  *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    KAL_ERR             kal_err;


    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;


    KAL_QPost(        p_mgr_data->MgmtSignalResp,
              (void *)p_buf,
                      KAL_OPT_POST_NONE,
                     &kal_err);
    if (kal_err != KAL_ERR_NONE) {
         NetBuf_Free(p_buf);
    }


   *p_err = NET_WIFI_MGR_ERR_NONE;
}




/*
*********************************************************************************************************
*                                        NetOS_WiFiMgr_Lock()
*
* Description : Lock wireless manager.
*
* Argument(s) : plock_obj   Pointer to variable that contains the wireless manager lock object.
*               ---------   Argument checked in NetOS_WiFiMgr_LockInit().
*
*               ptask_obj   Pointer to task object that will receive the current task pointer.
*               ---------   Argument checked in NetOS_WiFiMgr_LockInit().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_OS_ERR_NONE             Wireless manager access     acquired.
*                               NET_WIFI_MGR_ERR_LOCK       Wireless manager access NOT acquired.
*
* Return(s)   : none.
*
* Caller(s)   : NetWiFiMgr_AP_Scan(),
*               NetWiFiMgr_AP_Join(),
*               NetWiFiMgr_AP_Leave(),
*               NetWiFiMgr_Mgmt().
*
*               This function is a wireless manager function & SHOULD be called only by appropriate network
*               device driver function(s).
*
* Note(s)     : (1) (a) Wireless manager access MUST be acquired--i.e. MUST wait for access; do NOT timeout.
*
*                       (1) Failure to acquire manager access will prevent network task(s)/operation(s)
*                           from functioning.
*
*                   (b) Wireless manager access MUST be acquired exclusively by only a single task at any one
*                       time.
*
*                   See also 'NetOS_WiFiMgr_Unlock()  Note #1'.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
static  void  NetWiFiMgr_LockAcquire (KAL_LOCK_HANDLE   lock,
                                      NET_ERR          *p_err)
{
    KAL_ERR  kal_err;


    KAL_LockAcquire(lock, KAL_OPT_PEND_NONE, KAL_TIMEOUT_INFINITE, &kal_err);
    switch (kal_err) {
        case KAL_ERR_NONE:
            *p_err = NET_WIFI_MGR_ERR_NONE;
             break;


        case KAL_ERR_LOCK_OWNER:
            *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
             return;


        case KAL_ERR_NULL_PTR:
        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ABORT:
        case KAL_ERR_TIMEOUT:
        case KAL_ERR_WOULD_BLOCK:
        case KAL_ERR_OS:
        default:
            *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
             return;
    }
}
#endif


/*
*********************************************************************************************************
*                                       NetOS_WiFiMgr_Unlock()
*
* Description : Unlock wireless manager.
*
* Argument(s) : plock_obj   Pointer to variable that contains the wireless manager lock object.
*               ---------   Argument checked in NetOS_WiFiMgr_LockInit().
*
*               ptask_obj   Pointer to task object that will receive the current task pointer.
*               ---------   Argument checked in NetOS_WiFiMgr_LockInit().
*
* Return(s)   : none.
*
* Caller(s)   : NetWiFiMgr_AP_Scan(),
*               NetWiFiMgr_AP_Join(),
*               NetWiFiMgr_AP_Leave(),
*               NetWiFiMgr_Mgmt().
*
*               This function is a wireless manager function & SHOULD be called only by appropriate network
*               device driver function(s).
*
* Note(s)     : (1) Wireless manager MUST be released--i.e. MUST unlock access without failure.
*
*                   (a) Failure to release Wireless manager access will prevent task(s)/operation(s) from
*                       functioning.  Thus Wireless manager access is assumed to be successfully released
*                       since NO uC/OS-III error handling could be performed to counteract failure.
*
*                   See also 'NetOS_WiFiMgr_Lock()  Note #1'.
*********************************************************************************************************
*/
#ifdef  NET_IF_WIFI_MODULE_EN
static  void  NetWiFiMgr_LockRelease (KAL_LOCK_HANDLE  lock)
{
    KAL_ERR  err_kal;


    KAL_LockRelease(lock,  &err_kal);                           /* Release exclusive network access.                    */

   (void)&err_kal;                                               /* See Note #1a.                                        */
}
#endif

/*
*********************************************************************************************************
*                                         NetWiFiMgr_AP_Create()
*
* Description : Create wireless network:
*
*                   (a) Release network          lock
*                   (b) Acquire wireless manager lock
*                   (c) Acquire network          lock
*                   (d) Send 'Create' command and get result
*                   (e) Release network          lock
*                   (f) Release wireless manager lock
*
*
* Argument(s) : p_if    Pointer to interface to create with.
*               ----    Argument validated in NetIF_WiFi_CreateAP().
*
*               p_join  Pointer to variable that contains the wireless network to join.
*               ------  Argument validated in NetIF_WiFi_CreateAP().
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*                                   NET_WIFI_MGR_ERR_NOT_STARTED            Wireless manager NOT started.
*
*                                                                           --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                                   NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                           - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_CreateAP() via 'p_mgr_api->Create()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetWiFiMgr_AP_Create (       NET_IF                 *p_if,
                                    const  NET_IF_WIFI_AP_CFG     *p_ap_cfg,
                                           NET_ERR                *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    CPU_INT08U         *p_buf_cmd;
    CPU_INT16U          buf_data_len;
    NET_ERR             err;

    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    if (p_mgr_data->DevStarted != DEF_YES) {
       *p_err = NET_WIFI_MGR_ERR_NOT_STARTED;
        return;
    }


                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return;
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_AP_Create, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return;
    }

                                                                /* ---------- SEND CREATE CMD AND GET RESULT ---------- */
    buf_data_len = sizeof(NET_IF_WIFI_AP_CFG);
    p_buf_cmd    = (CPU_INT08U *)p_ap_cfg;
    (void)NetWiFiMgr_MgmtHandler(p_if,
                                 NET_IF_WIFI_CMD_CREATE,
                                 p_buf_cmd,
                                 buf_data_len,
                                 0,
                                 0,
                                 p_err);


                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);


    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return;
    }

    p_mgr_data->AP_Created = DEF_YES;

   *p_err = NET_WIFI_MGR_ERR_NONE;
}
/*
*********************************************************************************************************
*                                         NetWiFiMgr_AP_GetPeerInfo()
*
* Description : (1) Get the info of the peer connected to the access point created by the interface:
*
*                   (a) Release network          lock
*                   (b) Acquire wireless manager lock
*                   (c) Acquire network          lock
*                   (d) Send get peer info command and get result
*                   (e) Release network          lock
*                   (f) Release wireless manager lock
*
*
* Argument(s) : p_if            Pointer to interface to Scan with.
*               ----            Argument validated in NetIF_WiFi_Scan().
*
*               p_buf_peer      Pointer to table that will receive the peer info found.
*
*               peer_len_max    Length of the scan buffer (i.e. Number of network that can be found).s
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_WIFI_MGR_ERR_NONE                   Wireless manager cmd successufully executed.
*                                   NET_WIFI_MGR_ERR_NOT_STARTED            Wireless manager NOT started.
*
*                                                                           --- RETURNED BY 'NetOS_WiFiMgr_Lock()' : ---
*                                   NET_WIFI_MGR_ERR_LOCK                   Wireless manager access NOT acquired.
*
*                                                                           - RETURNED BY 'NetWiFiMgr_MgmtHandler()' : -
*                                   NET_WIFI_MGR_ERR_CMD_FAULT              Management command  fault.
*                                   NET_WIFI_MGR_ERR_RESP_FAULT             Management response fault.
*                                   NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT    Wireless manager response signal timeout.

*
* Return(s)   : Number of peer found, if any.
*
*               0, otherwise.
*
* Caller(s)   : NetIF_WiFi_IF_GetPeerInfo() via 'p_mgr_api->GetPeerInfo()'.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static CPU_INT16U NetWiFiMgr_AP_GetPeerInfo (       NET_IF                *p_if,
                                             const  NET_IF_WIFI_PEER      *p_buf_peer,
                                                    CPU_INT16U             peer_len_max,
                                                    NET_ERR               *p_err)
{
    NET_WIFI_MGR_DATA  *p_mgr_data;
    CPU_INT08U         *p_buf_rtn;
    CPU_INT16U          len;
    CPU_INT32U          rtn_len;
    NET_ERR             err;

    p_mgr_data = (NET_WIFI_MGR_DATA *)p_if->Ext_Data;
    if (p_mgr_data->DevStarted != DEF_YES) {
       *p_err = NET_WIFI_MGR_ERR_NOT_STARTED;
        return (0);
    }
    if (p_mgr_data->AP_Created == DEF_NO) {
       *p_err = NET_WIFI_MGR_ERR_STATE;
        return (0);
    }
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
                                                                /* -------------- ACQUIRE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockAcquire(p_mgr_data->MgrLock,
                           p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
          return(0);
    }
                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetWiFiMgr_AP_Create, &err);
    if (err != NET_ERR_NONE) {
       *p_err = NET_WIFI_MGR_ERR_LOCK_ACQUIRE;
        return(0);
    }
                                                                /* ------ SEND GET PEER INFO CMD AND GET RESULT ------- */

    len       =  peer_len_max * sizeof(NET_IF_WIFI_PEER);
    p_buf_rtn = (CPU_INT08U *) p_buf_peer;
    rtn_len   =  NetWiFiMgr_MgmtHandler(p_if,
                                        NET_IF_WIFI_CMD_GET_PEER_INFO,
                                        DEF_NULL,
                                        0,
                                        p_buf_rtn,
                                        len,
                                        p_err);

                                                                /* -------------- RELEASE WIFI MGR LOCK --------------- */
    NetWiFiMgr_LockRelease(p_mgr_data->MgrLock);


    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
         return (0);
    }

    len = rtn_len / sizeof(NET_IF_WIFI_PEER);

   *p_err = NET_WIFI_MGR_ERR_NONE;

    return (len);
}
/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_WIFI_MODULE_EN       */

