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
* Filename : net_wifi_mgr.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.12 (or more recent version) is included in the project build.
*
*            (2) The wireless hardware used with this wireless manager is assumed to embed the wireless
*                supplicant within the wireless hardware and provide common command.
*
*            (3) Interrupt support is Hardware specific, therefore the generic Wireless manager does NOT
*                support interrupts.  However, interrupt support is easily added to the generic wireless
*                manager & thus the ISR handler has been prototyped and & populated within the function
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
*                                               MODULE
*
* Note(s) : (1) This network physical layer header file is protected from multiple pre-processor inclusion
*               through use of the network physical layer module present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  NET_WIFI_MGR_MODULE_PRESENT                                 /* See Note #1.                                         */
#define  NET_WIFI_MGR_MODULE_PRESENT

#ifdef    NET_IF_WIFI_MODULE_EN


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/net.h>
#include  <lib_str.h>


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                             NETWORK WIRELESS MANAGER LAYER ERROR CODES
*
* Note(s) : (1) ALL wireless layer-independent error codes #define'd in      'net_err.h';
*               ALL wireless layer-specific    error codes #define'd in this 'net_wifi_mgr_&&&.h'.
*
*           (2) Network error code '12,000' series reserved for network wireless manager layer.
*               See 'net_err.h  NETWORK EXTENSION LAYER ERROR CODES' to ensure that wireless manager
*               layer-specific error codes do NOT conflict with extension layer-independent error codes.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

typedef  struct  net_wifi_mgr_ctx {
    CPU_BOOLEAN  WaitResp;
    CPU_INT32U   WaitRespTimeout_ms;
    CPU_BOOLEAN  MgmtCompleted;
} NET_WIFI_MGR_CTX;



typedef  struct  net_if_wifi_mgr_data {
    KAL_LOCK_HANDLE  MgrLock;
    KAL_Q_HANDLE     MgmtSignalResp;
    CPU_BOOLEAN      DevStarted;
    CPU_BOOLEAN      AP_Joined;
    CPU_BOOLEAN      AP_Created;
} NET_WIFI_MGR_DATA;


/*
*********************************************************************************************************
*                                   WIRELESS DEVICE API DATA TYPES
*
* Note(s) : (1) (a) The Wireless device application programming interface (API) data type is a specific
*                   network device API data type definition which MUST define ALL generic network device
*                   API functions, synchronized in both the sequential order of the functions & argument
*                   lists for each function.
*
*                   Thus, ANY modification to the sequential order or argument lists of the API functions
*                   MUST be appropriately synchronized between the generic network device API data type &
*                   the Wireless device API data type definition/instantiations.
*
*                   However, specific Wireless device API data type definitions/instantiations MAY include
*                   additional API functions after all generic Ethernet device API functions.
*
*               (b) ALL API functions MUST be defined with NO NULL functions for all specific Ethernet
*                   device API instantiations.  Any specific Ethernet device API instantiation that does
*                   NOT require a specific API's functionality MUST define an empty API function.
*
*               See also 'net_if.h  GENERIC NETWORK DEVICE API DATA TYPE  Note #1'.
*********************************************************************************************************
*/

                                                                            /* ---------- NET WIFI DEV API ------------ */
                                                                            /* Net wifi dev API fnct ptrs :             */
typedef  struct  net_dev_api_wifi {

    void        (*Init)               (NET_IF             *p_if,            /*   Init/add.                              */
                                       NET_ERR            *p_err);

    void        (*Start)              (NET_IF             *p_if,            /*   Start.                                 */
                                       NET_ERR            *p_err);

    void        (*Stop)               (NET_IF             *p_if,            /*   Stop.                                  */
                                       NET_ERR            *p_err);

    void        (*Rx)                 (NET_IF             *p_if,            /*   Rx.                                    */
                                       CPU_INT08U        **p_data,
                                       CPU_INT16U         *size,
                                       NET_ERR            *p_err);

    void        (*Tx)                 (NET_IF             *p_if,            /*   Tx.                                    */
                                       CPU_INT08U         *p_data,
                                       CPU_INT16U          size,
                                       NET_ERR            *p_err);

    void        (*AddrMulticastAdd)   (NET_IF             *p_if,            /*   Multicast addr add.                    */
                                       CPU_INT08U         *p_addr_hw,
                                       CPU_INT08U          addr_hw_len,
                                       NET_ERR            *p_err);

    void        (*AddrMulticastRemove)(NET_IF             *p_if,            /*   Multicast addr remove.                 */
                                       CPU_INT08U         *p_addr_hw,
                                       CPU_INT08U          addr_hw_len,
                                       NET_ERR            *p_err);

    void        (*ISR_Handler)        (NET_IF             *p_if,            /*   ISR handler.                           */
                                       NET_DEV_ISR_TYPE    type);

    void        (*MgmtDemux)          (NET_IF             *p_if,            /*   Demux mgmt frame.                      */
                                       NET_BUF            *p_buf,
                                       NET_ERR            *p_err);

    CPU_INT32U  (*MgmtExecuteCmd)     (NET_IF             *p_if,            /*   Execute mgmt cmd.                      */
                                       NET_IF_WIFI_CMD     cmd,
                                       NET_WIFI_MGR_CTX   *p_ctx,
                                       void               *p_cmd_data,
                                       CPU_INT16U          cmd_data_len,
                                       CPU_INT08U         *p_buf_rtn,
                                       CPU_INT08U          buf_rtn_len_max,
                                       NET_ERR            *p_err);

    CPU_INT32U  (*MgmtProcessResp)    (NET_IF             *p_if,            /*   Process mgmt frame.                    */
                                       NET_IF_WIFI_CMD     cmd,
                                       NET_WIFI_MGR_CTX   *p_ctx,
                                       CPU_INT08U         *p_buf_rxd,
                                       CPU_INT16U          buf_rxd_len,
                                       CPU_INT08U         *p_buf_rtn,
                                       CPU_INT16U          buf_rtn_len_max,
                                       NET_ERR            *p_err);
} NET_DEV_API_WIFI;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_WIFI_MGR_API  NetWiFiMgr_API_Generic;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_wifi_mgr.h  MODULE'.
*********************************************************************************************************
*/

#endif  /* NET_IF_WIFI_MODULE_EN       */
#endif  /* NET_WIFI_MGR_MODULE_PRESENT */
