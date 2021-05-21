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
*                                    NETWORK ERROR CODE MANAGEMENT
*
* Filename : net_err.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_ERR_MODULE_PRESENT
#define  NET_ERR_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"


/*
*********************************************************************************************************
*                                         NETWORK ERROR CODES
*
* Note(s) : (1) All generic network error codes are #define'd in 'net_err.h';
*               Any port-specific   error codes are #define'd in port-specific header files.
*********************************************************************************************************
*/


typedef enum net_err {

/*
---------------------------------------------------------------------------------------------------------
-                                      NETWORK-GENERIC ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_ERR_NONE                                =    1u,        /* Network operation completed successfully.            */

    NET_ERR_FAULT_LOCK_ACQUIRE                  =   20u,        /* Error occurred while acquiring the Network Lock.     */
    NET_ERR_FAULT_NULL_OBJ                      =   21u,        /* Program execution hit a NULL object in TCP/IP.       */
    NET_ERR_FAULT_NULL_FNCT                     =   22u,        /* Program execution hit a NULL function in TCP/IP.     */
    NET_ERR_FAULT_NULL_PTR                      =   23u,        /* Program execution hit a NULL pointer in TCP/IP.      */
    NET_ERR_FAULT_MEM_ALLOC                     =   24u,        /* Memory allocation error occurred.                    */
    NET_ERR_FAULT_NOT_SUPPORTED                 =   25u,        /* Requested feature is not supported.                  */
    NET_ERR_FAULT_FEATURE_DIS                   =   26u,        /* Requested feature is disabled.                       */
    NET_ERR_FAULT_UNKNOWN_ERR                   =   27u,        /* An unknown error occurred.                           */

    NET_ERR_IF_LINK_DOWN                        =   30u,        /* Net IF link state down.                              */
    NET_ERR_IF_LOOPBACK_DIS                     =   31u,        /* Loopback IF dis'd.                                   */

    NET_ERR_INVALID_PROTOCOL                    =   40u,        /* Invalid/unknown/unsupported net protocol.            */
    NET_ERR_INVALID_TRANSACTION                 =   41u,        /* Invalid transaction type.                            */
    NET_ERR_INVALID_ADDR                        =   42u,        /* Invalid address encountered.                         */
    NET_ERR_INVALID_TIME                        =   43u,        /* Invalid time/tick value.                             */
    NET_ERR_INVALID_TYPE                        =   44u,        /* Invalid type encountered.                            */
    NET_ERR_INVALID_STATE                       =   45u,        /* Invalid state encountered.                           */
    NET_ERR_INVALID_LEN                         =   46u,        /* Invalid length encountered.                          */

    NET_ERR_RX                                  =   50u,        /* Rx err.                                              */
    NET_ERR_RX_DEST                             =   51u,        /* Invalid rx dest.                                     */

    NET_ERR_TX                                  =   60u,        /* Tx err.                                              */
    NET_ERR_TX_BUF_NONE_AVAIL                   =   61u,        /* No Tx buffer available.                              */
    NET_ERR_TX_BUF_LOCK                         =   62u,        /* Invalid tx buf access; tx is locked.                 */

    NET_ERR_TIME_DLY_FAULT                      =   70u,        /* Time dly fault.                                      */
    NET_ERR_TIME_DLY_MAX                        =   71u,        /* Time dly max'd.                                      */


/*
---------------------------------------------------------------------------------------------------------
-                                      NETWORK-INIT ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_INIT_ERR_SIGNAL_CREATE                  =   100u,       /* Network Initialization complete successfully.        */

    NET_INIT_ERR_SIGNAL_COMPL                   =   101u,       /* Net Init completion NOT successfully signl'd.        */
    NET_INIT_ERR_NOT_COMPLETED                  =   102u,       /* Net Init NOT completed.                              */
    NET_INIT_ERR_COMP_SIGNAL_FAULT              =   103u,       /* Net Init complete signal creation failed.            */
    NET_INIT_ERR_OS_FEATURE_NOT_EN              =   104u,       /* A required OS feature is not enabled.                */
    NET_INIT_ERR_GLOBAL_LOCK_CREATE             =   105u,       /* Net lock signal name NOT successfully cfg'd.         */


/*
---------------------------------------------------------------------------------------------------------
-                                 NETWORK UTILITY LIBRARY ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_UTIL_ERR_NONE                           =    200u,      /* Network Utility operation complete successfully.     */

    NET_UTIL_ERR_NULL_SIZE                      =    210u,      /* Argument(s) passed a zero size.                      */
    NET_UTIL_ERR_INVALID_PROTOCOL               =    211u,      /* Invalid/unknown/unsupported net protocol.            */
    NET_UTIL_ERR_BUF_TOO_SMALL                  =    212u,

/*
---------------------------------------------------------------------------------------------------------
-                                      ASCII LIBRARY ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_ASCII_ERR_NONE                          =    300u,      /* Network ASCII operation complete successfully.       */

    NET_ASCII_ERR_INVALID_STR_LEN               =    310u,      /* Invalid ASCII str  len.                              */
    NET_ASCII_ERR_INVALID_CHAR                  =    311u,      /* Invalid ASCII char.                                  */
    NET_ASCII_ERR_INVALID_CHAR_LEN              =    312u,      /* Invalid ASCII char len.                              */
    NET_ASCII_ERR_INVALID_CHAR_VAL              =    313u,      /* Invalid ASCII char val.                              */
    NET_ASCII_ERR_INVALID_CHAR_SEQ              =    314u,      /* Invalid ASCII char seq.                              */
    NET_ASCII_ERR_INVALID_PART_LEN              =    315u,      /* Invalid ASCII part len.                              */
    NET_ASCII_ERR_INVALID_DOT_NBR               =    316u,      /* Invalid ASCII IP addr dot count.                     */

    NET_ASCII_ERR_IP_FAMILY_NOT_PRESENT         =    320u,      /* IP family not present or disabled.                   */


/*
---------------------------------------------------------------------------------------------------------
-                               NETWORK STATISTIC MANAGEMENT ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_STAT_ERR_NONE                           =    400u,      /* Network Statistic operation complete successfully.   */

    NET_STAT_ERR_INVALID_TYPE                   =    410u,      /* Type specified invalid or unknown.                   */

    NET_STAT_ERR_POOL_NONE_AVAIL                =    420u,      /* NO stat pool entries avail.                          */
    NET_STAT_ERR_POOL_NONE_USED                 =    421u,      /* NO stat pool entries used.                           */
    NET_STAT_ERR_POOL_NONE_REM                  =    422u,      /* NO stat pool entries rem'ing.                        */


/*
---------------------------------------------------------------------------------------------------------
-                                NETWORK TIMER MANAGEMENT ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_TMR_ERR_NONE                            =    500u,      /* Network Timer operation completed successfully.      */

    NET_TMR_ERR_INIT_TASK_CREATE                =    510u,      /* Network timer task creation failed.                  */
    NET_TMR_ERR_INIT_TASK_INVALID_ARG           =    511u,      /* Invalid argument in task allocation.                 */
    NET_TMR_ERR_INIT_TASK_INVALID_FREQ          =    512u,      /* INvalid frequency for the timer task.                */

    NET_TMR_ERR_NONE_AVAIL                      =    520u,      /* NO Network timers available.                         */
    NET_TMR_ERR_INVALID_TYPE                    =    521u,      /* Type specified invalid or unknown.                   */

    NET_TMR_ERR_IN_USE                          =    531u,      /* Network timer in use instead of available            */


/*
---------------------------------------------------------------------------------------------------------
-                                NETWORK BUFFER MANAGEMENT ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_BUF_ERR_NONE                            =    600u,      /* Network Buffer operation completed successfully.     */

    NET_BUF_ERR_NONE_AVAIL                      =    610u,      /* NO net bufs of req'd size avail.                     */
    NET_BUF_ERR_NOT_USED                        =    611u,      /* Net buf NOT used.                                    */

    NET_BUF_ERR_INVALID_TYPE                    =    620u,      /* Invalid buf type or unknown.                         */
    NET_BUF_ERR_INVALID_SIZE                    =    621u,      /* Invalid buf size.                                    */
    NET_BUF_ERR_INVALID_IX                      =    622u,      /* Invalid buf ix  outside DATA area.                   */
    NET_BUF_ERR_INVALID_LEN                     =    623u,      /* Invalid buf len outside DATA area.                   */

    NET_BUF_ERR_POOL_MEM_ALLOC                  =    630u,      /* Buf pool init      failed.                           */

    NET_BUF_ERR_INVALID_POOL_TYPE               =    640u,      /* Invalid buf pool type.                               */
    NET_BUF_ERR_INVALID_CFG_RX_NBR              =    643u,      /* Invalid Rx buf number configured.                    */
    NET_BUF_ERR_INVALID_CFG_TX_NBR              =    644u,      /* Invalid Tx buf number configured.                    */


/*
---------------------------------------------------------------------------------------------------------
-                              NETWORK CONNECTION MANAGEMENT ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_CONN_ERR_NONE                           =    700u,      /* Network Connection operation completed successfully. */

    NET_CONN_ERR_NONE_AVAIL                     =    710u,      /* NO net conns avail.                                  */
    NET_CONN_ERR_NOT_USED                       =    711u,      /* Net conn NOT used.                                   */

    NET_CONN_ERR_INVALID_TYPE                   =    720u,      /* Invalid conn type.                                   */
    NET_CONN_ERR_INVALID_CONN                   =    721u,      /* Invalid conn id.                                     */
    NET_CONN_ERR_INVALID_FAMILY                 =    722u,      /* Invalid conn list family.                            */
    NET_CONN_ERR_INVALID_PROTOCOL_IX            =    723u,      /* Invalid conn list protocol ix.                       */
    NET_CONN_ERR_INVALID_ADDR                   =    724u,      /* Invalid conn addr.                                   */
    NET_CONN_ERR_INVALID_ADDR_LEN               =    725u,      /* Invalid conn addr len.                               */
    NET_CONN_ERR_INVALID_ARG                    =    726u,      /* Invalid conn arg.                                    */
    NET_CONN_ERR_INVALID_PROTOCOL               =    727u,

    NET_CONN_ERR_ADDR_NOT_USED                  =    730u,      /* Conn addr NOT    used.                               */
    NET_CONN_ERR_ADDR_IN_USE                    =    731u,      /* Conn addr cur in use.                                */

    NET_CONN_ERR_CONN_NONE                      =    740u,      /* NO   conn.                                           */
    NET_CONN_ERR_CONN_HALF                      =    741u,      /* Half conn (local                   addr valid).      */
    NET_CONN_ERR_CONN_HALF_WILDCARD             =    742u,      /* Half conn (local wildcard          addr valid).      */
    NET_CONN_ERR_CONN_FULL                      =    743u,      /* Full conn (local          & remote addr valid).      */
    NET_CONN_ERR_CONN_FULL_WILDCARD             =    744u,      /* Full conn (local wildcard & remote addr valid).      */


/*
---------------------------------------------------------------------------------------------------------
-                           NETWORK BOARD SUPPORT PACKAGE (BSP) ERROR CODES
-
- Note(s) : (1) Specific BSP error codes #define'd (between 1000 and 1999) in their respective network-/board-specific
-               header files ('net_bsp.h').
---------------------------------------------------------------------------------------------------------
*/

    NET_BSP_ERR_NONE                            =   1000u,      /* Network BSP operation completed successfully.        */


/*
---------------------------------------------------------------------------------------------------------
-                                     NETWORK DEVICE ERROR CODES
-
- Note(s) : (1) Specific device error codes #define'd (between 2000 and 2999) in their respective network device driver
-               header files ('net_dev_&&&.h').
---------------------------------------------------------------------------------------------------------
*/

    NET_DEV_ERR_NONE                            =   2000u,      /* Network Device operation completed successfully.     */

    NET_DEV_ERR_INIT                            =   2010u,      /* Dev init  failed.                                    */
    NET_DEV_ERR_FAULT                           =   2011u,      /* Dev fault/failure.                                   */
    NET_DEV_ERR_NULL_PTR                        =   2012u,      /* Ptr arg(s) passed NULL ptr(s).                       */
    NET_DEV_ERR_MEM_ALLOC                       =   2013u,      /* Mem alloc failed.                                    */
    NET_DEV_ERR_DEV_OFF                         =   2014u,      /* Dev status is 'OFF' or 'DOWN'.                       */
    NET_DEV_ERR_NOT_SUPPORTED                   =   2015u,      /* Dev don't suppport the requested feature.            */

    NET_DEV_ERR_INVALID_IF                      =   2020u,      /* Invalid dev/IF nbr.                                  */
    NET_DEV_ERR_INVALID_CFG                     =   2021u,      /* Invalid dev cfg.                                     */
    NET_DEV_ERR_INVALID_SIZE                    =   2022u,      /* Invalid size.                                        */
    NET_DEV_ERR_INVALID_DATA_PTR                =   2023u,      /* Invalid dev drv data ptr.                            */

    NET_DEV_ERR_ADDR_MCAST_ADD                  =   2030u,      /* Multicast addr add    failed.                        */
    NET_DEV_ERR_ADDR_MCAST_REMOVE               =   2031u,      /* Multicast addr remove failed.                        */

    NET_DEV_ERR_RX                              =   2040u,      /* Dev rx failed or fault.                              */
    NET_DEV_ERR_RX_BUSY                         =   2041u,      /* Rx'r busy -- cannot rx data.                         */

    NET_DEV_ERR_TX                              =   2050u,      /* Dev tx failed or fault.                              */
    NET_DEV_ERR_TX_BUSY                         =   2051u,      /* Tx'r busy -- cannot tx data.                         */
    NET_DEV_ERR_TX_RDY_SIGNAL                   =   2052u,      /* Tx rdy signal failed.                                */
    NET_DEV_ERR_TX_RDY_SIGNAL_TIMEOUT           =   2053u,      /* Tx rdy signal timeout; NO signal rx'd from dev.      */
    NET_DEV_ERR_TX_RDY_SIGNAL_FAULT             =   2054u,      /* Tx rdy signal fault.                                 */


/*
---------------------------------------------------------------------------------------------------------
-                                 NETWORK EXTENSION LAYER ERROR CODES
-
- Note(s) : (1) Specific extension layer error codes #define'd (between 3000 and 3999) in their respective extension
-               layer driver header files ('net_phy_&&&.h').
---------------------------------------------------------------------------------------------------------
*/

    NET_PHY_ERR_NONE                            =   3000u,      /* Network PHY device operation completed successfully. */

    NET_PHY_ERR_INVALID_CFG                     =   3010u,      /* Invalid phy cfg.                                     */
    NET_PHY_ERR_INVALID_ADDR                    =   3011u,      /* Invalid phy addr.                                    */
    NET_PHY_ERR_INVALID_BUS_MODE                =   3012u,      /* Invalid phy bus mode.                                */

    NET_PHY_ERR_TIMEOUT_REG_RD                  =   3020u,      /* Phy reg rd           timeout.                        */
    NET_PHY_ERR_TIMEOUT_REG_WR                  =   3021u,      /* Phy reg wr           timeout.                        */
    NET_PHY_ERR_TIMEOUT_AUTO_NEG                =   3022u,      /* Phy auto-negotiation timeout.                        */
    NET_PHY_ERR_TIMEOUT_RESET                   =   3023u,      /* Phy reset            timeout.                        */

    NET_PHY_ERR_ADDR_PROBE                      =   3030u,      /* Phy addr detection   failed.                         */

/*
---------------------------------------------------------------------------------------------------------
-                                 WIRELESS MANAGER LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_WIFI_MGR_ERR_NONE                       =   4000u,      /* WiFi Manager operation completed successfully.       */

    NET_WIFI_MGR_ERR_MEM_ALLOC                  =   4010u,      /* Mem alloc failed.                                    */

    NET_WIFI_MGR_ERR_NOT_STARTED                =   4020u,      /* Mgr not started.                                     */

    NET_WIFI_MGR_ERR_LOCK_CREATE                =   4030u,      /* Mgr Lock creation failed.                            */
    NET_WIFI_MGR_ERR_LOCK_ACQUIRE               =   4031u,      /* Acquire Mgr Lock failed.                             */

    NET_WIFI_MGR_ERR_RESP_SIGNAL_CREATE         =   4040u,      /* Resp signal creation failed.                         */
    NET_WIFI_MGR_ERR_RESP_SIGNAL_TIMEOUT        =   4041u,      /* Resp signal timeout.                                 */

    NET_WIFI_MGR_ERR_CMD_FAULT                  =   4050u,      /* Mgmt cmd  fault.                                     */
    NET_WIFI_MGR_ERR_RESP_FAULT                 =   4051u,      /* Mgmt Resp fault.                                     */

    NET_WIFI_MGR_ERR_STATE                      =   4060u,      /* Mgmt err state.                                      */


/*
---------------------------------------------------------------------------------------------------------
-                                     WIRELESS LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_IF_WIFI_ERR_NONE                        =   5000u,      /* WiFi operation completed successfully.               */

    NET_IF_WIFI_ERR_SCAN                        =   5010u,      /* Scan  wifi net available failed.                     */
    NET_IF_WIFI_ERR_JOIN                        =   5011u,      /* Join  wifi net           failed.                     */
    NET_IF_WIFI_ERR_CREATE                      =   5012u,      /* Create wifi net          failed.                     */
    NET_IF_WIFI_ERR_LEAVE                       =   5013u,      /* Leave wifi net           failed.                     */
    NET_IF_WIFI_ERR_GET_PEER_INFO               =   5014u,      /* Get Peer Info wifi net   failed.                     */

    NET_IF_WIFI_ERR_INVALID_CH                  =   5020u,      /* Argument passed invalid ch.                          */
    NET_IF_WIFI_ERR_INVALID_NET_TYPE            =   5021u,      /* Argument passed invalid net  type.                   */
    NET_IF_WIFI_ERR_INVALID_DATA_RATE           =   5022u,      /* Argument passed invalid data rate.                   */
    NET_IF_WIFI_ERR_INVALID_SECURITY            =   5023u,      /* Argument passed invalid security.                    */
    NET_IF_WIFI_ERR_INVALID_PWR_LEVEL           =   5024u,      /* Argument passed invalid pwr  level.                  */
    NET_IF_WIFI_ERR_INVALID_REG_DOMAIN          =   5025u,      /* Argument passed invalid pwr  level.                  */

/*
---------------------------------------------------------------------------------------------------------
-                                 NETWORK INTERFACE LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_IF_ERR_NONE                             =   6000u,      /* Network Interface operation completed successfully.  */

    NET_IF_ERR_TX_RDY_SIGNAL_TIMEOUT            =   6010u,      /* Tx Ready Signal not received before timeout.         */
    NET_IF_ERR_TX_RDY_SIGNAL_FAULT              =   6011u,      /* Tx Ready Signal faulted.                             */

    NET_IF_ERR_INIT_RX_Q_CREATE                 =   6020u,      /* IF Rx Queue creation failed.                         */
    NET_IF_ERR_INIT_RX_Q_INVALID_ARG            =   6021u,      /* Invalid Argument in Rx Queue Init.                   */
    NET_IF_ERR_INIT_RX_Q_MEM_ALLOC              =   6022u,      /* Memory allocation error in Rx Queue Init.            */
    NET_IF_ERR_INIT_RX_TASK_CREATE              =   6023u,      /* IF Rx Task creation failed.                          */
    NET_IF_ERR_INIT_RX_TASK_INVALID_ARG         =   6024u,      /* Invalid Argument in Rx Task Init.                    */
    NET_IF_ERR_INIT_RX_TASK_MEM_ALLOC           =   6025u,      /* Memory allocation error in Rx Task Init.             */

    NET_IF_ERR_INIT_TX_DEALLOC_Q_CREATE         =   6030u,      /* IF Tx Queue creation failed.                         */
    NET_IF_ERR_INIT_TX_DEALLOC_Q_INVALID_ARG    =   6031u,      /* Invalid Argument in Tx Queue Init.                   */
    NET_IF_ERR_INIT_TX_DEALLOC_Q_MEM_ALLOC      =   6032u,      /* Memory allocation error in Tx Queue Init.            */
    NET_IF_ERR_INIT_TX_DEALLOC_TASK_CREATE      =   6033u,      /* IF Tx Task creation failed.                          */
    NET_IF_ERR_INIT_TX_DEALLOC_TASK_INVALID_ARG =   6034u,      /* Invalid Argument in Tx Task Init.                    */
    NET_IF_ERR_INIT_TX_DEALLOC_TASK_MEM_ALLOC   =   6035u,      /* Memory allocation error in Tx Task Init.             */

    NET_IF_ERR_INIT_TX_SUSPEND_SEM_CREATE       =   6040u,      /* IF Tx Suspend semaphore creation failed.             */
    NET_IF_ERR_INIT_TX_SUSPEND_MEM_ALLOC        =   6041u,      /* Memory allocation error in Tx Suspend sem creation.  */
    NET_IF_ERR_INIT_TX_SUSPEND_SEM_INVALID_ARG  =   6042u,      /* Invalid argument in Tx Suspend sem creation.         */
    NET_IF_ERR_INIT_TX_SUSPEND_TIMEOUT          =   6043u,      /* Failed to set Tx Suspend timeout.                    */

    NET_IF_ERR_ALIGN_NOT_AVAIL                  =   6050u,      /* App data buf alignment NOT possible.                 */

    NET_IF_ERR_LOOPBACK_RX_Q_EMPTY              =   6060u,      /* Loopback rx Q empty; i.e.       NO rx'd pkt(s) in Q. */
    NET_IF_ERR_LOOPBACK_RX_Q_FULL               =   6061u,      /* Loopback rx Q full;  i.e. too many rx'd pkt(s) in Q. */
    NET_IF_ERR_LOOPBACK_INVALID_MTU             =   6062u,      /* Loopback IF invalid MTU.                             */
    NET_IF_ERR_LOOPBACK_INVALID_ADDR            =   6063u,      /* Loopback IF invalid address.                         */
    NET_IF_ERR_LOOPBACK_DEMUX_PROTOCOL          =   6064u,      /* Loopback IF demuxed invalid protocol.                */

    NET_IF_ERR_LINK_SUBSCRIBER_MEM_ALLOC        =   6070u,      /* Memory allocation error in Subscriber pool init.     */
    NET_IF_ERR_LINK_SUBSCRIBER_NOT_FOUND        =   6071u,      /* IF Link Subscriber Not Found.                        */

    NET_IF_ERR_DEV_FAULT                        =   6080u,      /* IF device faulted.                                   */
    NET_IF_ERR_DEV_TX_RDY_VAL                   =   6081u,      /* Dev tx rdy  signal          NOT successfully init'd. */

    NET_IF_ERR_INVALID_IF                       =   6100u,      /* Invalid IF nbr.                                      */
    NET_IF_ERR_INVALID_CFG                      =   6101u,      /* Invalid IF API/Cfg/Data.                             */
    NET_IF_ERR_INVALID_STATE                    =   6102u,      /* Invalid IF state.                                    */
    NET_IF_ERR_INVALID_IO_CTRL_OPT              =   6103u,      /* Invalid/unsupported IO ctrl opt.                     */
    NET_IF_ERR_INVALID_ISR_TYPE                 =   6104u,      /* Invalid/unsupported ISR type.                        */
    NET_IF_ERR_INVALID_PROTOCOL                 =   6105u,      /* Invalid protocol.                                    */
    NET_IF_ERR_INVALID_ADDR                     =   6106u,      /* Invalid addr.                                        */
    NET_IF_ERR_INVALID_ADDR_LEN                 =   6107u,      /* Invalid addr len.                                    */
    NET_IF_ERR_INVALID_ADDR_DEST                =   6108u,      /* Invalid addr dest.                                   */
    NET_IF_ERR_INVALID_ADDR_SRC                 =   6109u,      /* Invalid addr sec.                                    */
    NET_IF_ERR_INVALID_MTU                      =   6110u,      /* Invalid MTU.                                         */
    NET_IF_ERR_INVALID_LEN_DATA                 =   6111u,      /* Invalid data  len.                                   */
    NET_IF_ERR_INVALID_LEN_FRAME                =   6112u,      /* Invalid frame len.                                   */
    NET_IF_ERR_INVALID_POOL_TYPE                =   6113u,      /* Invalid IF buf pool type.                            */
    NET_IF_ERR_INVALID_POOL_ADDR                =   6114u,      /* Invalid IF buf pool addr.                            */
    NET_IF_ERR_INVALID_POOL_SIZE                =   6115u,      /* Invalid IF buf pool size.                            */
    NET_IF_ERR_INVALID_POOL_QTY                 =   6116u,      /* Invalid IF buf pool qty cfg'd.                       */
    NET_IF_ERR_INVALID_ETHER_TYPE               =   6117u,      /* Invalid Ethernet type.                               */
    NET_IF_ERR_INVALID_LLC_DSAP                 =   6118u,      /* Invalid IEEE 802.2 LLC  DSAP val.                    */
    NET_IF_ERR_INVALID_LLC_SSAP                 =   6119u,      /* Invalid IEEE 802.2 LLC  SSAP val.                    */
    NET_IF_ERR_INVALID_LLC_CTRL                 =   6120u,      /* Invalid IEEE 802.2 LLC  Ctrl val.                    */
    NET_IF_ERR_INVALID_SNAP_CODE                =   6121u,      /* Invalid IEEE 802.2 SNAP OUI  val.                    */
    NET_IF_ERR_INVALID_SNAP_TYPE                =   6122u,      /* Invalid IEEE 802.2 SNAP Type val.                    */

    NET_IF_ERR_RX                               =   6200u,      /* Rx failed or fault.                                  */
    NET_IF_ERR_RX_Q_EMPTY                       =   6201u,      /* Rx Q empty; i.e.       NO rx'd pkt(s) in Q.          */
    NET_IF_ERR_RX_Q_FULL                        =   6202u,      /* Rx Q full;  i.e. too many rx'd pkt(s) in Q.          */
    NET_IF_ERR_RX_Q_SIGNAL                      =   6203u,      /* Rx Q signal failed.                                  */
    NET_IF_ERR_RX_Q_SIGNAL_TIMEOUT              =   6204u,      /* Rx Q signal timeout; NO pkt(s) rx'd from dev.        */
    NET_IF_ERR_RX_Q_SIGNAL_FAULT                =   6205u,      /* Rx Q signal fault.                                   */

    NET_IF_ERR_TX                               =   6300u,      /* Tx failed or fault.                                  */
    NET_IF_ERR_TX_RDY                           =   6301u,      /* Tx to dev rdy.                                       */
    NET_IF_ERR_TX_BROADCAST                     =   6302u,      /* Tx broadcast on local net.                           */
    NET_IF_ERR_TX_MULTICAST                     =   6303u,      /* Tx multicast on local net.                           */
    NET_IF_ERR_TX_ADDR_REQ                      =   6304u,      /* Tx req  for hw addr.                                 */
    NET_IF_ERR_TX_ADDR_PEND                     =   6305u,      /* Tx pend on  hw addr.                                 */
    NET_IF_ERR_TX_DEALLC_Q_EMPTY                =   6306u,      /* Tx dealloc Q empty; i.e.       NO tx completed pkts. */
    NET_IF_ERR_TX_DEALLC_Q_FULL                 =   6307u,      /* Tx dealloc Q full;  i.e. too many tx completed pkts. */
    NET_IF_ERR_TX_DEALLC_Q_SIGNAL               =   6308u,      /* Tx dealloc Q signal failed.                          */
    NET_IF_ERR_TX_DEALLC_Q_SIGNAL_TIMEOUT       =   6309u,      /* Tx dealloc Q signal timeout.                         */
    NET_IF_ERR_TX_DEALLC_Q_SIGNAL_FAULT         =   6310u,      /* Tx dealloc Q signal fault.                           */


/*
---------------------------------------------------------------------------------------------------------
-                                       CACHE LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_CACHE_ERR_NONE                          =   7000u,      /* Network Cache operation successful.                  */

    NET_CACHE_ERR_NONE_AVAIL                    =   7010u,      /* Cache NOT avail.                                     */
    NET_CACHE_ERR_INVALID_TYPE                  =   7011u,      /* Cache type invalid or unknown.                       */
    NET_CACHE_ERR_NOT_FOUND                     =   7012u,      /* Cache NOT found.                                     */
    NET_CACHE_ERR_PEND                          =   7013u,      /* Cache hw addr pending.                               */
    NET_CACHE_ERR_RESOLVED                      =   7014u,      /* Cache hw addr resolved.                              */
    NET_CACHE_ERR_UNRESOLVED                    =   7015u,      /* Cache hw addr un-resolved.                           */

    NET_CACHE_ERR_INVALID_ADDR_PROTO_LEN        =   7020u,      /* Invalid protocol addr len (see Note #1a).            */
    NET_CACHE_ERR_INVALID_ADDR_HW_LEN           =   7021u,      /* Invalid hw       addr len.                           */


/*
---------------------------------------------------------------------------------------------------------
-                                        ARP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_ARP_ERR_NONE                            =   8000u,      /* Network ARP operation successful.                    */

    NET_ARP_ERR_INVALID_HW_TYPE                 =   8010u,      /* Invalid ARP hw       type.                           */
    NET_ARP_ERR_INVALID_HW_ADDR                 =   8011u,      /* Invalid ARP hw       addr.                           */
    NET_ARP_ERR_INVALID_HW_ADDR_LEN             =   8012u,      /* Invalid ARP hw       addr len.                       */
    NET_ARP_ERR_INVALID_PROTOCOL_TYPE           =   8013u,      /* Invalid ARP protocol type.                           */
    NET_ARP_ERR_INVALID_PROTOCOL_ADDR           =   8014u,      /* Invalid ARP protocol addr.                           */
    NET_ARP_ERR_INVALID_PROTOCOL_LEN            =   8015u,      /* Invalid ARP protocol addr len (see Note #1a).        */
    NET_ARP_ERR_INVALID_OP_CODE                 =   8016u,      /* Invalid ARP op code.                                 */
    NET_ARP_ERR_INVALID_OP_ADDR                 =   8017u,      /* Invalid ARP op code  addr.                           */
    NET_ARP_ERR_INVALID_LEN_MSG                 =   8018u,      /* Invalid ARP msg len.                                 */

    NET_ARP_ERR_CACHE_NONE_AVAIL                =   8020u,      /* NO ARP caches avail.                                 */
    NET_ARP_ERR_CACHE_INVALID_TYPE              =   8021u,      /* ARP cache type invalid or unknown.                   */
    NET_ARP_ERR_CACHE_NOT_FOUND                 =   8022u,      /* ARP cache NOT found.                                 */
    NET_ARP_ERR_CACHE_PEND                      =   8023u,      /* ARP cache hw addr pending.                           */
    NET_ARP_ERR_CACHE_RESOLVED                  =   8024u,      /* ARP cache hw addr resolved.                          */

    NET_ARP_ERR_RX_TARGET_THIS_HOST             =   8030u,      /* Rx ARP msg     for this host.                        */
    NET_ARP_ERR_RX_TARGET_NOT_THIS_HOST         =   8031u,      /* Rx ARP msg NOT for this host.                        */
    NET_ARP_ERR_RX_TARGET_REPLY                 =   8032u,      /* Rx ARP Reply   for this host NOT in ARP cache.       */

    NET_ARP_ERR_RX_REQ_TX_REPLY                 =   8040u,      /* Rx'd ARP Req;   tx ARP Reply.                        */
    NET_ARP_ERR_RX_REPLY_TX_PKTS                =   8041u,      /* Rx'd ARP Reply; tx ARP cache pkts.                   */


/*
---------------------------------------------------------------------------------------------------------
-                                NETWORK LAYER MANAGEMENT ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_MGR_ERR_NONE                            =   9000u,      /* Network Manager operation successful.                */

    NET_MGR_ERR_INVALID_PROTOCOL                =   9010u,      /* Invalid/unsupported protocol.                        */
    NET_MGR_ERR_INVALID_PROTOCOL_ADDR           =   9011u,      /* Invalid protocol addr.                               */
    NET_MGR_ERR_INVALID_PROTOCOL_LEN            =   9012u,      /* Invalid protocol addr len (see Note #1a).            */

    NET_MGR_ERR_ADDR_CFG_IN_PROGRESS            =   9020u,      /* IF addr cfg in progress.                             */
    NET_MGR_ERR_ADDR_TBL_SIZE                   =   9021u,      /* Invalid addr tbl size.                               */


/*
---------------------------------------------------------------------------------------------------------
-                                        IPv4 LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_IPv4_ERR_NONE                           =   10000u,     /* Network IPv4 operation successful.                   */

    NET_IPv4_ERR_INVALID_VER                    =   10010u,     /* Invalid IP ver.                                      */
    NET_IPv4_ERR_INVALID_LEN_HDR                =   10011u,     /* Invalid IP hdr  len.                                 */
    NET_IPv4_ERR_INVALID_TOS                    =   10012u,     /* Invalid IP TOS.                                      */
    NET_IPv4_ERR_INVALID_LEN_TOT                =   10013u,     /* Invalid IP tot  len.                                 */
    NET_IPv4_ERR_INVALID_LEN_DATA               =   10014u,     /* Invalid IP data len.                                 */
    NET_IPv4_ERR_INVALID_FLAG                   =   10015u,     /* Invalid IP flags.                                    */
    NET_IPv4_ERR_INVALID_FRAG                   =   10016u,     /* Invalid IP fragmentation.                            */
    NET_IPv4_ERR_INVALID_TTL                    =   10017u,     /* Invalid IP TTL.                                      */
    NET_IPv4_ERR_INVALID_PROTOCOL               =   10018u,     /* Invalid/unknown protocol type.                       */
    NET_IPv4_ERR_INVALID_CHK_SUM                =   10019u,     /* Invalid IP chk sum.                                  */
    NET_IPv4_ERR_INVALID_ADDR_LEN               =   10020u,     /* Invalid IP           addr len.                       */
    NET_IPv4_ERR_INVALID_ADDR_SRC               =   10021u,     /* Invalid IP src       addr.                           */
    NET_IPv4_ERR_INVALID_ADDR_DEST              =   10022u,     /* Invalid IP dest      addr.                           */
    NET_IPv4_ERR_INVALID_ADDR_BROADCAST         =   10023u,     /* Invalid IP broadcast addr.                           */
    NET_IPv4_ERR_INVALID_ADDR_HOST              =   10025u,     /* Invalid IP host      addr.                           */
    NET_IPv4_ERR_INVALID_ADDR_NET               =   10026u,     /* Invalid IP net       addr.                           */
    NET_IPv4_ERR_INVALID_ADDR_GATEWAY           =   10027u,     /* Invalid IP gateway   addr.                           */
    NET_IPv4_ERR_INVALID_OPT                    =   10028u,     /* Invalid IP opt.                                      */
    NET_IPv4_ERR_INVALID_OPT_PTR                =   10029u,     /* Invalid IP opt ptr.                                  */
    NET_IPv4_ERR_INVALID_OPT_LEN                =   10040u,     /* Invalid IP opt len.                                  */
    NET_IPv4_ERR_INVALID_OPT_TYPE               =   10041u,     /* Invalid IP opt type.                                 */
    NET_IPv4_ERR_INVALID_OPT_NBR                =   10042u,     /* Invalid IP opt nbr same opt.                         */
    NET_IPv4_ERR_INVALID_OPT_CFG                =   10043u,     /* Invalid IP opt cfg.                                  */
    NET_IPv4_ERR_INVALID_OPT_FLAG               =   10044u,     /* Invalid IP opt flag.                                 */
    NET_IPv4_ERR_INVALID_OPT_ROUTE              =   10045u,     /* Invalid IP opt route.                                */
    NET_IPv4_ERR_INVALID_OPT_END                =   10046u,     /* Invalid IP opt list ending.                          */

    NET_IPv4_ERR_ADDR_CFG                       =   10100u,     /* Invalid IP addr cfg.                                 */
    NET_IPv4_ERR_ADDR_CFG_STATE                 =   10101u,     /* Invalid IP addr cfg state.                           */
    NET_IPv4_ERR_ADDR_CFG_IN_PROGRESS           =   10102u,     /*         IF addr cfg in progress.                     */
    NET_IPv4_ERR_ADDR_CFG_IN_USE                =   10103u,     /*         IP addr cur in use.                          */
    NET_IPv4_ERR_ADDR_NONE_AVAIL                =   10104u,     /* NO      IP addr(s)  cfg'd.                           */
    NET_IPv4_ERR_ADDR_NOT_FOUND                 =   10105u,     /*         IP addr NOT found.                           */
    NET_IPv4_ERR_ADDR_TBL_SIZE                  =   10106u,     /* Invalid IP addr tbl size.                            */
    NET_IPv4_ERR_ADDR_TBL_EMPTY                 =   10107u,     /*         IP addr tbl empty.                           */
    NET_IPv4_ERR_ADDR_TBL_FULL                  =   10108u,     /*         IP addr tbl full.                            */

    NET_IPv4_ERR_RX_FRAG_NONE                   =   10200u,     /* Rx'd datagram NOT frag'd.                            */
    NET_IPv4_ERR_RX_FRAG_OFFSET                 =   10201u,     /* Invalid frag offset.                                 */
    NET_IPv4_ERR_RX_FRAG_SIZE                   =   10202u,     /* Invalid frag     size.                               */
    NET_IPv4_ERR_RX_FRAG_SIZE_TOT               =   10203u,     /* Invalid frag tot size.                               */
    NET_IPv4_ERR_RX_FRAG_LEN_TOT                =   10204u,     /* Invalid frag tot len.                                */
    NET_IPv4_ERR_RX_FRAG_DISCARD                =   10205u,     /* Invalid frag(s) discarded.                           */
    NET_IPv4_ERR_RX_FRAG_REASM                  =   10206u,     /* Frag'd datagram reasm in progress.                   */
    NET_IPv4_ERR_RX_FRAG_COMPLETE               =   10207u,     /* Frag'd datagram reasm'd.                             */

    NET_IPv4_ERR_RX_OPT_BUF_NONE_AVAIL          =   10210u,     /* No bufs avail for IP opts.                           */
    NET_IPv4_ERR_RX_OPT_BUF_LEN                 =   10211u,     /* IP opt buf len err.                                  */
    NET_IPv4_ERR_RX_OPT_BUF_WR                  =   10212u,     /* IP opt buf wr  err.                                  */

    NET_IPv4_ERR_TX_PKT                         =   10300u,     /* Tx pkt err.                                          */
    NET_IPv4_ERR_TX_DEST_NONE                   =   10301u,     /* NO      tx dest.                                     */
    NET_IPv4_ERR_TX_DEST_INVALID                =   10302u,     /* Invalid tx dest.                                     */
    NET_IPv4_ERR_TX_DEST_LOCAL_HOST             =   10303u,     /* Tx to local host   addr.                             */
    NET_IPv4_ERR_TX_DEST_BROADCAST              =   10304u,     /* Tx to local net broadcast.                           */
    NET_IPv4_ERR_TX_DEST_MULTICAST              =   10305u,     /* Tx to local net multicast.                           */
    NET_IPv4_ERR_TX_DEST_HOST_THIS_NET          =   10306u,     /* Tx to local net host.                                */
    NET_IPv4_ERR_TX_DEST_DFLT_GATEWAY           =   10307u,     /* Tx to local net dflt gateway.                        */


/*
---------------------------------------------------------------------------------------------------------
-                                       IPv6 LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_IPv6_ERR_NONE                           =  11000u,      /* Network IPv6 operation successful.                   */
    NET_IPv6_ERR_FAULT                          =  11001u,      /* IPv6 layer faulted.                                  */

    NET_IPv6_ERR_INVALID_VER                    =  11010u,      /* Invalid IPv6 ver.                                    */
    NET_IPv6_ERR_INVALID_TRAFFIC_CLASS          =  11011u,      /* Invalid IPv6 traffic class.                          */
    NET_IPv6_ERR_INVALID_FLOW_LABEL             =  11012u,      /* Invalid IPv6 flow label.                             */
    NET_IPv6_ERR_INVALID_LEN_HDR                =  11011u,      /* Invalid IP hdr  len.                                 */
    NET_IPv6_ERR_INVALID_TOS                    =  11012u,      /* Invalid IP TOS.                                      */
    NET_IPv6_ERR_INVALID_LEN_TOT                =  11013u,      /* Invalid IP tot  len.                                 */
    NET_IPv6_ERR_INVALID_LEN_DATA               =  11014u,      /* Invalid IP data len.                                 */
    NET_IPv6_ERR_INVALID_FLAG                   =  11015u,      /* Invalid IP flags.                                    */
    NET_IPv6_ERR_INVALID_FRAG                   =  11016u,      /* Invalid IP fragmentation.                            */
    NET_IPv6_ERR_INVALID_HOP_LIMIT              =  11017u,      /* Invalid IP TTL.                                      */
    NET_IPv6_ERR_INVALID_PROTOCOL               =  11018u,      /* Invalid/unknown protocol type.                       */
    NET_IPv6_ERR_INVALID_CHK_SUM                =  11019u,      /* Invalid IP chk sum.                                  */
    NET_IPv6_ERR_INVALID_ADDR_LEN               =  11020u,      /* Invalid IP           addr len.                       */
    NET_IPv6_ERR_INVALID_ADDR_SRC               =  11021u,      /* Invalid IP src       addr.                           */
    NET_IPv6_ERR_INVALID_ADDR_DEST              =  11022u,      /* Invalid IP dest      addr.                           */
    NET_IPv6_ERR_INVALID_ADDR_BROADCAST         =  11023u,      /* Invalid IP broadcast addr.                           */
    NET_IPv6_ERR_INVALID_ADDR_HOST              =  11024u,      /* Invalid IP host      addr.                           */
    NET_IPv6_ERR_INVALID_ADDR_NET               =  11025u,      /* Invalid IP net       addr.                           */
    NET_IPv6_ERR_INVALID_EH                     =  11026u,      /* Invalid IP ext hdr.                                  */
    NET_IPv6_ERR_INVALID_EH_LEN                 =  11027u,      /* Invalid IP ext hdr len.                              */
    NET_IPv6_ERR_INVALID_EH_OPT                 =  11028u,      /* Invalid IP ext hdr opt.                              */
    NET_IPv6_ERR_INVALID_EH_OPT_SEQ             =  11029u,      /* Invalid IP ext hdr sequence.                         */
    NET_IPv6_ERR_INVALID_ADDR_ID_TYPE           =  11030u,      /* Invalid addr ID type.                                */
    NET_IPv6_ERR_INVALID_ADDR_PREFIX_TYPE       =  11031u,      /* Invalid addr prefix type.                            */
    NET_IPv6_ERR_INVALID_ADDR_PREFIX_LEN        =  11032u,      /* Invalid addr prefix len.                             */
    NET_IPv6_ERR_INVALID_ADDR_CFG_MODE          =  11033u,      /* Invalid addr cfg mode.                               */

    NET_IPv6_ERR_ADDR_CFG                       =  11100u,      /* Invalid IP addr cfg.                                 */
    NET_IPv6_ERR_ADDR_CFG_STATE                 =  11101u,      /* Invalid IP addr cfg state.                           */
    NET_IPv6_ERR_ADDR_CFG_IN_PROGRESS           =  11102u,      /*         IF addr cfg in progress.                     */
    NET_IPv6_ERR_ADDR_CFG_IN_USE                =  11103u,      /*         IP addr cur in use.                          */
    NET_IPv6_ERR_ADDR_CFG_DUPLICATED            =  11104u,      /*         IP addr is duplicated on network.            */
    NET_IPv6_ERR_ADDR_CFG_LINK_LOCAL            =  11105u,      /* Link-local connectivity only.                        */
    NET_IPv6_ERR_ADDR_NONE_AVAIL                =  11106u,      /* NO      IP addr(s)  cfg'd.                           */
    NET_IPv6_ERR_ADDR_NOT_FOUND                 =  11107u,      /*         IP addr NOT found.                           */
    NET_IPv6_ERR_ADDR_TBL_SIZE                  =  11108u,      /* Invalid IP addr tbl size.                            */
    NET_IPv6_ERR_ADDR_TBL_EMPTY                 =  11109u,      /*         IP addr tbl empty.                           */
    NET_IPv6_ERR_ADDR_TBL_FULL                  =  11110u,      /*         IP addr tbl full.                            */

    NET_IPv6_ERR_RX_FRAG_NONE                   =  11200u,      /* Rx'd datagram NOT frag'd.                            */
    NET_IPv6_ERR_RX_FRAG_OFFSET                 =  11201u,      /* Invalid frag offset.                                 */
    NET_IPv6_ERR_RX_FRAG_SIZE                   =  11202u,      /* Invalid frag     size.                               */
    NET_IPv6_ERR_RX_FRAG_SIZE_TOT               =  11203u,      /* Invalid frag tot size.                               */
    NET_IPv6_ERR_RX_FRAG_LEN_TOT                =  11204u,      /* Invalid frag tot len.                                */
    NET_IPv6_ERR_RX_FRAG_DISCARD                =  11205u,      /* Invalid frag(s) discarded.                           */
    NET_IPv6_ERR_RX_FRAG_REASM                  =  11206u,      /* Frag'd datagram reasm in progress.                   */
    NET_IPv6_ERR_RX_FRAG_COMPLETE               =  11207u,      /* Frag'd datagram reasm'd.                             */

    NET_IPv6_ERR_RX_OPT_BUF_NONE_AVAIL          =  11210u,      /* No bufs avail for IP opts.                           */
    NET_IPv6_ERR_RX_OPT_BUF_LEN                 =  11211u,      /* IP opt buf len err.                                  */
    NET_IPv6_ERR_RX_OPT_BUF_WR                  =  11212u,      /* IP opt buf wr  err.                                  */

    NET_IPv6_ERR_TX_PKT                         =  11300u,      /* Tx pkt err.                                          */

    NET_IPv6_ERR_TX_SRC_SEL_FAIL                =  11310u,      /* NO      tx src.                                      */
    NET_IPv6_ERR_TX_SRC_INVALID                 =  11311u,      /* Invalid tx src.                                      */

    NET_IPv6_ERR_TX_DEST_NONE                   =  11320u,      /* NO      tx dest.                                     */
    NET_IPv6_ERR_TX_DEST_INVALID                =  11321u,      /* Invalid tx dest.                                     */
    NET_IPv6_ERR_TX_DEST_LOCAL_HOST             =  11322u,      /* Tx to local host   addr.                             */
    NET_IPv6_ERR_TX_DEST_MULTICAST              =  11323u,      /* Tx to local net multicast.                           */
    NET_IPv6_ERR_TX_DEST_HOST_THIS_NET          =  11324u,      /* Tx to local net host.                                */

    NET_IPv6_ERR_NEXT_HOP                       =  11340u,      /* No Next Hop.                                         */

    NET_IPv6_ERR_AUTO_CFG_DISABLED              =  11400u,
    NET_IPv6_ERR_AUTO_CFG_STARTED               =  11401u,
    NET_IPv6_ERR_ROUTER_ADV_SIGNAL_TIMEOUT      =  11402u,


/*
---------------------------------------------------------------------------------------------------------
-                                   GENERIC DAD LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_DAD_ERR_NONE                            =  11700u,
    NET_DAD_ERR_IN_PROGRESS                     =  11701u,
    NET_DAD_ERR_FAILED                          =  11702u,
    NET_DAD_ERR_ADDR_NOT_FOUND                  =  11703u,
    NET_DAD_ERR_SIGNAL_CREATE                   =  11704u,
    NET_DAD_ERR_SIGNAL_FAULT                    =  11705u,
    NET_DAD_ERR_SIGNAL_INVALID                  =  11706u,
    NET_DAD_ERR_OBJ_POOL_CREATE                 =  11707u,
    NET_DAD_ERR_OBJ_MEM_ALLOC                   =  11708u,
    NET_DAD_ERR_OBJ_NOT_FOUND                   =  11709u,
    NET_DAD_ERR_FAULT                           =  11710u,


/*
---------------------------------------------------------------------------------------------------------
-                                   GENERIC ICMP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_ICMP_ERR_NONE                           =  12000u,      /* Network ICMP operation successful.                   */

    NET_ICMP_ERR_LOCK_CREATE                    =  12010u,      /* ICMP lock creation failed.                           */
    NET_ICMP_ERR_LOCK_ACQUIRE                   =  12011u,      /* Acquiring ICMP lock failed.                          */

    NET_ICMP_ERR_SIGNAL_TIMEOUT                 =  12020u,      /* ICMP signal timed out.                               */
    NET_ICMP_ERR_SIGNAL_FAULT                   =  12021u,      /* ICMP signal faulted.                                 */

    NET_ICMP_ERR_ECHO_REQ_TIMEOUT               =  12030u,      /* ICMP Echo Req signal timed out.                      */
    NET_ICMP_ERR_ECHO_REQ_SIGNAL_FAULT          =  12031u,      /* ICMP Echo Req signal faulted.                        */
    NET_ICMP_ERR_ECHO_REPLY_RX                  =  12032u,      /* ICMP Echo Reply rx doesn't matched any Echo Req.     */
    NET_ICMP_ERR_ECHO_REPLY_DATA_CMP_FAIL       =  12033u,      /* ICMP Echo Reply rx data doesn't matched data send.   */


/*
---------------------------------------------------------------------------------------------------------
-                                      ICMPv4 LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_ICMPv4_ERR_NONE                         =  13000u,      /* Network ICMPv4 operation successful.                 */

    NET_ICMPv4_ERR_INVALID_TYPE                 =  13010u,      /* Invalid ICMPv4 msg  type / ICMP data type.           */
    NET_ICMPv4_ERR_INVALID_CODE                 =  13011u,      /* Invalid ICMPv4 msg  code.                            */
    NET_ICMPv4_ERR_INVALID_PTR                  =  13012u,      /* Invalid ICMPv4 msg  ptr.                             */
    NET_ICMPv4_ERR_INVALID_LEN                  =  13013u,      /* Invalid ICMPv4 msg  len.                             */
    NET_ICMPv4_ERR_INVALID_LEN_DATA             =  13014u,      /* Invalid ICMPv4 data len.                             */
    NET_ICMPv4_ERR_INVALID_CHK_SUM              =  13015u,      /* Invalid ICMPv4 chk  sum.                             */
    NET_ICMPv4_ERR_INVALID_ARG                  =  13016u,      /* Invalid ICMPv4 argument.                             */


    NET_ICMPv4_ERR_MSG_TYPE_ERR                 =  13020u,      /* ICMPv4 err   msg type.                               */
    NET_ICMPv4_ERR_MSG_TYPE_REQ                 =  13021u,      /* ICMPv4 req   msg type.                               */
    NET_ICMPv4_ERR_MSG_TYPE_REPLY               =  13022u,      /* ICMPv4 reply msg type.                               */

    NET_ICMPv4_ERR_RX_BROADCAST                 =  13030u,      /* ICMPv4 rx invalid broadcast.                         */
    NET_ICMPv4_ERR_RX_MCAST                     =  13031u,      /* ICMPv4 rx invalid multicast.                         */
    NET_ICMPv4_ERR_RX_REPLY                     =  13032u,      /* ICMPv4 rx invalid reply.                             */

    NET_ICMPv4_ERR_TX_INVALID_BROADCAST         =  13040u,      /* ICMPv4 tx invalid broadcast.                         */
    NET_ICMPv4_ERR_TX_INVALID_MCAST             =  13041u,      /* ICMPv4 tx invalid multicast.                         */
    NET_ICMPv4_ERR_TX_INVALID_FRAG              =  13042u,      /* ICMPv4 tx invalid frag.                              */
    NET_ICMPv4_ERR_TX_INVALID_ADDR_SRC          =  13043u,      /* ICMPv4 tx invalid addr src.                          */
    NET_ICMPv4_ERR_TX_INVALID_ERR_MSG           =  13044u,      /* ICMPv4 tx invalid err msg.                           */


/*
---------------------------------------------------------------------------------------------------------
-                                      ICMPv6 LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_ICMPv6_ERR_NONE                         =  14000u,      /* Network ICMPv6 operation successful.                 */
    NET_ICMPv6_ERR_FAULT                        =  14001u,      /* ICMPv6 layer faulted.                                */

    NET_ICMPv6_ERR_INVALID_TYPE                 =  14010u,      /* Invalid ICMPv6 msg  type / ICMP data type.           */
    NET_ICMPv6_ERR_INVALID_CODE                 =  14011u,      /* Invalid ICMPv6 msg  code.                            */
    NET_ICMPv6_ERR_INVALID_PTR                  =  14012u,      /* Invalid ICMPv6 msg  ptr.                             */
    NET_ICMPv6_ERR_INVALID_LEN                  =  14013u,      /* Invalid ICMPv6 msg  len.                             */
    NET_ICMPv6_ERR_INVALID_LEN_DATA             =  14014u,      /* Invalid ICMPv6 data len.                             */
    NET_ICMPv6_ERR_INVALID_CHK_SUM              =  14015u,      /* Invalid ICMPv6 chk  sum.                             */

    NET_ICMPv6_ERR_MSG_TYPE_ERR                 =  14020u,      /* ICMPv6 err   msg type.                               */
    NET_ICMPv6_ERR_MSG_TYPE_REQ                 =  14021u,      /* ICMPv6 req   msg type.                               */
    NET_ICMPv6_ERR_MSG_TYPE_REPLY               =  14022u,      /* ICMPv6 reply msg type.                               */
    NET_ICMPv6_ERR_MSG_TYPE_NEIGHBOR_SOL        =  14023u,      /* ICMPv6 Neighbor Solicitation  msg type.              */
    NET_ICMPv6_ERR_MSG_TYPE_NEIGHBOR_ADV        =  14024u,      /* ICMPv6 Neighbor Advertisement msg type.              */
    NET_ICMPv6_ERR_MSG_TYPE_ROUTER_SOL          =  14025u,      /* ICMPv6 Router   Solicitation  msg type.              */
    NET_ICMPv6_ERR_MSG_TYPE_ROUTER_ADV          =  14026u,      /* ICMPv6 Router   Advertisement msg type.              */
    NET_ICMPv6_ERR_MSG_TYPE_REDIRECT            =  14027u,      /* ICMPv6 Router   Advertisement msg type.              */
    NET_ICMPv6_ERR_MSG_TYPE_QUERY               =  14028u,      /* ICMPv6 MLDP     query         msg type.              */
    NET_ICMPv6_ERR_MSG_TYPE_REPORT              =  14029u,      /* ICMPv6 MLDP     report        msg type.              */

    NET_ICMPv6_ERR_RX_REPLY                     =  14100u,      /* IMCPv6 rx invalid reply message.                     */
    NET_ICMPv6_ERR_RX_BCAST                     =  14101u,      /* ICMPv6 rx invalid broadcast.                         */
    NET_ICMPv6_ERR_RX_MCAST                     =  14102u,      /* ICMPv6 rx invalid multicast.                         */

    NET_ICMPv6_ERR_TX_INVALID_BCAST             =  14200u,      /* ICMPv6 tx invalid broadcast.                         */
    NET_ICMPv6_ERR_TX_INVALID_MCAST             =  14201u,      /* ICMPv6 tx invalid multicast.                         */
    NET_ICMPv6_ERR_TX_INVALID_FRAG              =  14202u,      /* ICMPv6 tx invalid frag.                              */
    NET_ICMPv6_ERR_TX_INVALID_ADDR_SRC          =  14203u,      /* ICMPv6 tx invalid addr src.                          */
    NET_ICMPv6_ERR_TX_INVALID_ERR_MSG           =  14204u,      /* ICMPv6 tx invalid err msg.                           */


/*
---------------------------------------------------------------------------------------------------------
-                                    NETWORK NDP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_NDP_ERR_NONE                            =  15000u,      /* Network NDP operation successful.                    */

    NET_NDP_ERR_LOCK                            =  15010u,      /* NDP lock error.                                      */
    NET_NDP_ERR_FAULT                           =  15011u,      /* A NDP fault occurs.                                  */

    NET_NDP_ERR_INVALID_TYPE                    =  15020u,      /* Invalid NDP type in NDP message.                     */
    NET_NDP_ERR_OPT_TYPE                        =  15021u,      /* Invalid option type in NDP message.                  */
    NET_NDP_ERR_OPT_LEN                         =  15022u,      /* Invalid option length in NDP message.                */
    NET_NDP_ERR_HW_ADDR_LEN                     =  15023u,      /* Invalid Hardware Address Length in NDP message.      */
    NET_NDP_ERR_HW_ADDR_THIS_HOST               =  15024u,      /* h/w addr specified same as host h/w addr.            */
    NET_NDP_ERR_ADDR_SRC                        =  15025u,      /* Invalid address source.                              */
    NET_NDP_ERR_ADDR_DEST                       =  15026u,      /* Invalid address destination.                         */
    NET_NDP_ERR_ADDR_TARGET                     =  15027u,      /* Invalid address target.                              */
    NET_NDP_ERR_HOP_LIMIT                       =  15028u,      /* Invalid Hop Limit in NDP message.                    */
    NET_NDP_ERR_ADDR_CFG_FAILED                 =  15029u,      /* Configure addr from received prefix failed.          */
    NET_NDP_ERR_ADDR_CFG_IN_PROGRESS            =  15030u,      /* Configure addr from received prefix not completed.   */

    NET_NDP_ERR_INVALID_ARG                     =  15040u,      /* NDP function passed invalid argument.                */
    NET_NDP_ERR_INVALID_PROTOCOL_LEN            =  15041u,      /* Invalid NDP protocol addr len.                       */
    NET_NDP_ERR_INVALID_HW_ADDR_LEN             =  15042u,      /* Invalid NDP hw       addr len.                       */
    NET_NDP_ERR_INVALID_PREFIX                  =  15043u,      /* Invalid NDP prefix.                                  */

    NET_NDP_ERR_NEIGHBOR_CACHE_NONE_AVAIL       =  15050u,      /* NO NDP Neighbor cache available in pool.             */
    NET_NDP_ERR_NEIGHBOR_CACHE_ADD_FAIL         =  15053u,      /* Failed to add NDP Neighbor cache to cache list.      */
    NET_NDP_ERR_NEIGHBOR_CACHE_NOT_FOUND        =  15054u,      /* NDP Neighbor cache not found.                        */
    NET_NDP_ERR_NEIGHBOR_CACHE_PEND             =  15055u,      /* NDP Neighbor cache addr resolution is pending.       */
    NET_NDP_ERR_NEIGHBOR_CACHE_RESOLVED         =  15056u,      /* NDP Neighbor cache addr resolved.                    */
    NET_NDP_ERR_NEIGHBOR_CACHE_STALE            =  15057u,      /* NDP Neighbor cache is in stale state.                */

    NET_NDP_ERR_ROUTER_NONE_AVAIL               =  15060u,      /* NO NDP Router entry available.                       */
    NET_NDP_ERR_PREFIX_NONE_AVAIL               =  15061u,      /* NO NDP Prefix entry available.                       */
    NET_NDP_ERR_DEST_CACHE_NONE_AVAIL           =  15062u,      /* NO NDP Destination cache available.                  */

    NET_NDP_ERR_ROUTER_DFLT_FIND                =  15070u,      /* NDP Default Router found.                            */
    NET_NDP_ERR_ROUTER_DFLT_NONE                =  15071u,      /* NDP Router found (but not default).                  */
    NET_NDP_ERR_ROUTER_NOT_FOUND                =  15072u,      /* NO corresponding NDP router found in list.           */

    NET_NDP_ERR_PREFIX_NOT_FOUND                =  15080u,      /* NO corresponding NDP prefix found in list.           */

    NET_NDP_ERR_DESTINATION_NOT_FOUND           =  15090u,      /* NO corresponding NDP dest cache found in list.       */

    NET_NDP_ERR_TX_DEST_MULTICAST               =  15100u,      /* Destination is multicast.                            */
    NET_NDP_ERR_TX_DEST_HOST_THIS_NET           =  15101u,      /* Destination is on same local network.                */
    NET_NDP_ERR_TX_DFLT_GATEWAY                 =  15102u,      /* Packet is sent trough default gateway.               */
    NET_NDP_ERR_TX_NO_DFLT_GATEWAY              =  15103u,      /* Packet is sent through a none default gateway.       */
    NET_NDP_ERR_TX_NO_NEXT_HOP                  =  15104u,      /* NO valid Packet Nest Hop found.                      */
    NET_NDP_ERR_TX                              =  15105u,      /* NDP Tx packet error.                                 */

    NET_NDP_ERR_ADDR_DUPLICATE                  =  15201u,
    NET_NDP_ERR_ADDR_TENTATIVE                  =  15202u,

    NET_NDP_ERR_ROUTER_ADV_SIGNAL_CREATE        =  15300u,
    NET_NDP_ERR_ROUTER_ADV_SIGNAL_FAULT         =  15301u,
    NET_NDP_ERR_ROUTER_ADV_SIGNAL_TIMEOUT       =  15302u,


/*
---------------------------------------------------------------------------------------------------------
-                                       IGMP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_IGMP_ERR_NONE                           =  16000u,      /* Network IGMP operation successful.                   */

    NET_IGMP_ERR_INVALID_VER                    =  16010u,      /* Invalid IGMP ver.                                    */
    NET_IGMP_ERR_INVALID_TYPE                   =  16011u,      /* Invalid IGMP msg  type.                              */
    NET_IGMP_ERR_INVALID_LEN                    =  16012u,      /* Invalid IGMP msg  len.                               */
    NET_IGMP_ERR_INVALID_CHK_SUM                =  16013u,      /* Invalid IGMP chk  sum.                               */
    NET_IGMP_ERR_INVALID_ADDR_SRC               =  16014u,      /* Invalid IGMP src  addr.                              */
    NET_IGMP_ERR_INVALID_ADDR_DEST              =  16015u,      /* Invalid IGMP dest addr                               */
    NET_IGMP_ERR_INVALID_ADDR_GRP               =  16016u,      /* Invalid IGMP grp  addr.                              */

    NET_IGMP_ERR_MSG_TYPE_QUERY                 =  16020u,      /* IGMP query  msg type.                                */
    NET_IGMP_ERR_MSG_TYPE_REPORT                =  16021u,      /* IGMP report msg type.                                */

    NET_IGMP_ERR_HOST_GRP_NONE_AVAIL            =  16030u,      /* NO IGMP host grp avail.                              */
    NET_IGMP_ERR_HOST_GRP_NOT_FOUND             =  16031u,      /*    IGMP host grp NOT found.                          */
    NET_IGMP_ERR_HOST_GRP_INVALID_TYPE          =  16032u,      /*    IGMP host grp type invalid or unknown.            */


/*
---------------------------------------------------------------------------------------------------------
-                                       MLDP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_MLDP_ERR_NONE                           =  17000u,       /* Network MLDP operation successful.                  */

    NET_MLDP_ERR_HOP_LIMIT                      =  17010u,       /* Invalid Hop Limit in MLDP message.                  */

    NET_MLDP_ERR_INVALID_ADDR_SRC               =  17020u,       /* Invalid address source in MLDP message.             */
    NET_MLDP_ERR_INVALID_ADDR_DEST              =  17021u,       /* Invalid address destination in MLDP message.        */
    NET_MLDP_ERR_INVALID_ADDR_GRP               =  17022u,       /* Invalid multicast MLDP address group.               */
    NET_MLDP_ERR_INVALID_LEN                    =  17023u,       /* Invalid MDLP message length.                        */
    NET_MDLP_ERR_INVALID_TYPE                   =  17024u,       /* Invalid MLDP message type.                          */
    NET_MLDP_ERR_INVALID_CHK_SUM                =  17025u,       /* Invalid check sum in MLDP message.                  */
    NET_MLDP_ERR_INVALID_HOP_HDR                =  17026u,       /* Invalid IPv6 Hop extension header in MLDP message.    */

    NET_MLDP_ERR_HOST_GRP_NONE_AVAIL            =  17030u,       /* No more MLDP multicast group available.             */
    NET_MLDP_ERR_HOST_GRP_NOT_FOUND             =  17031u,       /* MLDP multicast group not found.                     */

    NET_MLDP_ERR_MSG_TYPE_QUERY                 =  17040u,       /* Rx a MLDP Query message.                            */
    NET_MLDP_ERR_MSG_TYPE_REPORT                =  17041u,       /* Rx a MLDP Report message.                           */


/*
---------------------------------------------------------------------------------------------------------
-                                        UDP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_UDP_ERR_NONE                            =  18000u,      /* Network UDP operation successful.                    */

    NET_UDP_ERR_INVALID_DATA_SIZE               =  18010u,      /* Invalid UDP data size.                               */
    NET_UDP_ERR_INVALID_ARG                     =  18011u,      /* Invalid UDP arg.                                     */
    NET_UDP_ERR_INVALID_LEN                     =  18012u,      /* Invalid UDP datagram len.                            */
    NET_UDP_ERR_INVALID_LEN_DATA                =  18013u,      /* Invalid UDP data     len.                            */
    NET_UDP_ERR_INVALID_ADDR_SRC                =  18014u,      /* Invalid UDP src  addr.                               */
    NET_UDP_ERR_INVALID_PORT_NBR                =  18015u,      /* Invalid UDP port nbr.                                */
    NET_UDP_ERR_INVALID_CHK_SUM                 =  18016u,      /* Invalid UDP chk  sum.                                */
    NET_UDP_ERR_INVALID_FLAG                    =  18017u,      /* Invalid UDP flags.                                   */


/*
---------------------------------------------------------------------------------------------------------
-                                        TCP LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_TCP_ERR_NONE                            =  19000u,      /* Network TCP operation successful.                    */

    NET_TCP_ERR_INIT_RX_Q_FAULT                 =  19010u,      /* TCP Rx Q Init failed.                                */
    NET_TCP_ERR_INIT_TX_Q_FAULT                 =  19010u,      /* TCP Tx Q Init failed.                                */

    NET_TCP_ERR_NONE_AVAIL                      =  19020u,      /* NO TCP conns avail.                                  */

    NET_TCP_ERR_INVALID_DATA_SIZE               =  19030u,      /* Invalid TCP data size.                               */
    NET_TCP_ERR_INVALID_ARG                     =  19031u,      /* Invalid TCP arg.                                     */
    NET_TCP_ERR_INVALID_LEN_HDR                 =  19032u,      /* Invalid TCP hdr  len.                                */
    NET_TCP_ERR_INVALID_LEN_SEG                 =  19033u,      /* Invalid TCP seg  len.                                */
    NET_TCP_ERR_INVALID_LEN_DATA                =  19034u,      /* Invalid TCP data len.                                */
    NET_TCP_ERR_INVALID_PORT_NBR                =  19035u,      /* Invalid TCP port nbr.                                */
    NET_TCP_ERR_INVALID_FLAG                    =  19036u,      /* Invalid TCP flags.                                   */
    NET_TCP_ERR_INVALID_CHK_SUM                 =  19037u,      /* Invalid TCP chk sum.                                 */

    NET_TCP_ERR_INVALID_OPT                     =  19040u,      /* Invalid TCP opt.                                     */
    NET_TCP_ERR_INVALID_OPT_TYPE                =  19041u,      /* Invalid TCP opt type.                                */
    NET_TCP_ERR_INVALID_OPT_NBR                 =  19042u,      /* Invalid TCP opt nbr same opt.                        */
    NET_TCP_ERR_INVALID_OPT_LEN                 =  19043u,      /* Invalid TCP opt len.                                 */
    NET_TCP_ERR_INVALID_OPT_CFG                 =  19044u,      /* Invalid TCP opt cfg.                                 */
    NET_TCP_ERR_INVALID_OPT_END                 =  19045u,      /* Invalid TCP opt list ending.                         */

    NET_TCP_ERR_INVALID_CONN_TYPE               =  19050u,      /* Invalid   TCP conn type.                             */
    NET_TCP_ERR_INVALID_CONN                    =  19051u,      /* Invalid   TCP conn/id.                               */
    NET_TCP_ERR_INVALID_CONN_ID                 =  19052u,      /* Invalid   TCP conn's conn id.                        */
    NET_TCP_ERR_INVALID_CONN_OP                 =  19053u,      /* Invalid   TCP conn op.                               */
    NET_TCP_ERR_INVALID_CONN_STATE              =  19054u,      /* Invalid   TCP conn state.                            */

    NET_TCP_ERR_CONN_NONE                       =  19100u,      /* NO        TCP conn.                                  */
    NET_TCP_ERR_CONN_NOT_USED                   =  19101u,      /*           TCP conn NOT used.                         */
    NET_TCP_ERR_CONN_CLOSED                     =  19102u,      /*           TCP conn successfully closed.              */
    NET_TCP_ERR_CONN_CLOSE                      =  19103u,      /*           TCP conn abort        closed.              */
    NET_TCP_ERR_CONN_FAULT                      =  19104u,      /*           TCP conn fault        closed.              */
    NET_TCP_ERR_CONN_FAIL                       =  19105u,      /*           TCP conn op failed.                        */
    NET_TCP_ERR_CONN_TIMEOUT                    =  19106u,      /*           TCP conn timeout.                          */
    NET_TCP_ERR_CONN_LISTEN_Q_MAX               =  19107u,      /*           TCP conn listen Q max lim.                 */
    NET_TCP_ERR_CONN_PROTO_FAMILY               =  19108u,      /* Invalid   TCP conn protocol family.                  */

    NET_TCP_ERR_CONN_SEQ_NONE                   =  19110u,      /* NO        TCP conn seq.                              */
    NET_TCP_ERR_CONN_SEQ_SYNC                   =  19111u,      /*   Valid   TCP conn sync.                             */
    NET_TCP_ERR_CONN_SEQ_SYNC_INVALID           =  19112u,      /* Invalid   TCP conn sync.                             */
    NET_TCP_ERR_CONN_SEQ_VALID                  =  19113u,      /*   Valid   TCP conn seq.                              */
    NET_TCP_ERR_CONN_SEQ_INVALID                =  19114u,      /* Invalid   TCP conn seq.                              */
    NET_TCP_ERR_CONN_SEQ_FIN_VALID              =  19115u,      /*   Valid   TCP conn fin.                              */
    NET_TCP_ERR_CONN_SEQ_FIN_INVALID            =  19116u,      /* Invalid   TCP conn fin.                              */

    NET_TCP_ERR_CONN_ACK_NONE                   =  19120u,      /* NO        TCP conn ack.                              */
    NET_TCP_ERR_CONN_ACK_VALID                  =  19121u,      /*   Valid   TCP conn ack.                              */
    NET_TCP_ERR_CONN_ACK_INVALID                =  19122u,      /* Invalid   TCP conn ack.                              */
    NET_TCP_ERR_CONN_ACK_DUP                    =  19123u,      /* Duplicate TCP conn ack.                              */
    NET_TCP_ERR_CONN_ACK_DLYD                   =  19125u,      /* Dly'd     TCP conn ack.                              */
    NET_TCP_ERR_CONN_ACK_PREVLY_TXD             =  19126u,      /*           TCP conn ack prev'ly tx'd.                 */

    NET_TCP_ERR_CONN_RESET_NONE                 =  19130u,      /* NO        TCP conn reset.                            */
    NET_TCP_ERR_CONN_RESET_VALID                =  19131u,      /*   Valid   TCP conn reset.                            */
    NET_TCP_ERR_CONN_RESET_INVALID              =  19132u,      /* Invalid   TCP conn reset.                            */

    NET_TCP_ERR_CONN_PROBE_INVALID              =  19140u,      /* Invalid   TCP conn probe.                            */

    NET_TCP_ERR_CONN_DATA_NONE                  =  19150u,      /* NO        TCP conn data.                             */
    NET_TCP_ERR_CONN_DATA_VALID                 =  19151u,      /*   Valid   TCP conn data.                             */
    NET_TCP_ERR_CONN_DATA_INVALID               =  19152u,      /* Invalid   TCP conn data.                             */
    NET_TCP_ERR_CONN_DATA_DUP                   =  19153u,      /* Duplicate TCP conn data.                             */

    NET_TCP_ERR_RX                              =  19200u,      /*    Rx err.                                           */
    NET_TCP_ERR_RX_Q_CLOSED                     =  19201u,      /*    Rx Q closed; i.e.   do NOT rx   pkt(s) to Q.      */
    NET_TCP_ERR_RX_Q_EMPTY                      =  19202u,      /*    Rx Q empty;  i.e.      NO  rx'd pkt(s) in Q.      */
    NET_TCP_ERR_RX_Q_FULL                       =  19203u,      /*    Rx Q full;   i.e. too many rx'd pkt(s) in Q.      */
    NET_TCP_ERR_RX_Q_ABORT                      =  19204u,      /*    Rx Q abort      failed.                           */
    NET_TCP_ERR_RX_Q_SIGNAL                     =  19205u,      /*    Rx Q signal     failed.                           */
    NET_TCP_ERR_RX_Q_SIGNAL_CLR                 =  19206u,      /*    Rx Q signal clr failed.                           */
    NET_TCP_ERR_RX_Q_SIGNAL_TIMEOUT             =  19207u,      /*    Rx Q signal timeout.                              */
    NET_TCP_ERR_RX_Q_SIGNAL_ABORT               =  19208u,      /*    Rx Q signal aborted.                              */
    NET_TCP_ERR_RX_Q_SIGNAL_FAULT               =  19209u,      /*    Rx Q signal fault.                                */

    NET_TCP_ERR_TX_PKT                          =  19300u,      /*    Tx pkt err.                                       */
    NET_TCP_ERR_TX_Q_CLOSED                     =  19301u,      /*    Tx Q closed; i.e.   do NOT Q tx   pkt(s) to Q.    */
    NET_TCP_ERR_TX_Q_EMPTY                      =  19302u,      /*    Tx Q empty;  i.e.      NO    tx   pkt(s) in Q.    */
    NET_TCP_ERR_TX_Q_FULL                       =  19303u,      /*    Tx Q full;   i.e. too many   tx'd pkt(s) in Q.    */
    NET_TCP_ERR_TX_Q_ABORT                      =  19304u,      /*    Tx Q abort      failed.                           */
    NET_TCP_ERR_TX_Q_SUSPEND                    =  19305u,      /*    Tx Q suspended.                                   */
    NET_TCP_ERR_TX_Q_SIGNAL                     =  19306u,      /*    Tx Q signal     failed.                           */
    NET_TCP_ERR_TX_Q_SIGNAL_CLR                 =  19307u,      /*    Tx Q signal clr failed.                           */
    NET_TCP_ERR_TX_Q_SIGNAL_TIMEOUT             =  19308u,      /*    Tx Q signal timeout.                              */
    NET_TCP_ERR_TX_Q_SIGNAL_ABORT               =  19309u,      /*    Tx Q signal aborted.                              */
    NET_TCP_ERR_TX_Q_SIGNAL_FAULT               =  19310u,      /*    Tx Q signal fault.                                */

    NET_TCP_ERR_TX_KEEP_ALIVE_TH                =  19400u,      /* Keep-alive seg(s)    tx'd > th.                      */

    NET_TCP_ERR_RE_TX_SEG_TH                    =  19500u,      /* Re-tx Q    seg(s) re-tx'd > th.                      */


/*
---------------------------------------------------------------------------------------------------------
-                                  NETWORK SOCKET LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_SOCK_ERR_NONE                           =  20000u,      /* Network Socket operation successful.                 */

    NET_SOCK_ERR_INIT_RX_Q_FAULT                =  20010u,      /* Sock Init OS object Rx Q failed.                     */
    NET_SOCK_ERR_INIT_RX_Q_TIMEOUT_CFG          =  20011u,      /* Sock Init Rx Q timeout failed.                       */
    NET_SOCK_ERR_INIT_CONN_REQ_FAULT            =  20012u,      /* Sock Init OS object Conn Req failed.                 */
    NET_SOCK_ERR_INIT_CONN_REQ_INVALID_TIMEOUT  =  20013u,      /* Sock Init Conn Req timeout failed.                   */
    NET_SOCK_ERR_INIT_CONN_ACCEPT_FAULT         =  20014u,      /* Sock Init OS object Conn Accept failed.              */
    NET_SOCK_ERR_INIT_CONN_ACCEPT_TIMEOUT_CFG   =  20015u,      /* Sock Init Conn Accept timeout failed.                */
    NET_SOCK_ERR_INIT_CONN_CLOSE_FAULT          =  20016u,      /* Sock Init OS object Conn Close failed.               */
    NET_SOCK_ERR_INIT_CONN_CLOSE_TIMEOUT_CFG    =  20017u,      /* Sock Init Conn Close timeout failed.                 */
    NET_SOCK_ERR_INIT_MEM_ALLOC                 =  20018u,

    NET_SOCK_ERR_NONE_AVAIL                     =  20020u,      /* NO socks avail.                                      */
    NET_SOCK_ERR_NOT_USED                       =  20021u,      /* Sock NOT used.                                       */
    NET_SOCK_ERR_CLOSED                         =  20022u,      /* Sock       closed.                                   */
    NET_SOCK_ERR_FAULT                          =  20023u,      /* Sock fault closed.                                   */
    NET_SOCK_ERR_TIMEOUT                        =  20024u,      /* Sock op(s) timeout.                                  */

    NET_SOCK_ERR_INVALID_DATA_SIZE              =  20100u,      /* Invalid sock data size.                              */
    NET_SOCK_ERR_INVALID_ARG                    =  20101u,      /* Invalid sock arg.                                    */

    NET_SOCK_ERR_INVALID_FAMILY                 =  20120u,      /* Invalid sock protocol family.                        */
    NET_SOCK_ERR_INVALID_PROTOCOL               =  20121u,      /* Invalid sock protocol.                               */
    NET_SOCK_ERR_INVALID_TYPE                   =  20122u,      /* Invalid sock type.                                   */
    NET_SOCK_ERR_INVALID_SOCK                   =  20123u,      /* Invalid sock      id.                                */
    NET_SOCK_ERR_INVALID_DESC                   =  20124u,      /* Invalid sock desc id(s).                             */
    NET_SOCK_ERR_INVALID_CONN                   =  20125u,      /* Invalid sock conn/id.                                */
    NET_SOCK_ERR_INVALID_STATE                  =  20126u,      /* Invalid sock state.                                  */
    NET_SOCK_ERR_INVALID_OP                     =  20127u,      /* Invalid sock op.                                     */

    NET_SOCK_ERR_INVALID_OPT                    =  20130u,      /* Invalid sock opt.                                    */
    NET_SOCK_ERR_INVALID_FLAG                   =  20131u,      /* Invalid sock flag.                                   */
    NET_SOCK_ERR_INVALID_TIMEOUT                =  20132u,      /* Invalid sock timeout val.                            */
    NET_SOCK_ERR_INVALID_OPT_LEN                =  20133u,      /* Invalid sock opt len.                                */
    NET_SOCK_ERR_INVALID_OPT_GET                =  20134u,      /* Error in sock opt get sub-fnct.                      */
    NET_SOCK_ERR_INVALID_OPT_LEVEL              =  20135u,      /* Incompatible sock opt & opt level.                   */

    NET_SOCK_ERR_INVALID_ADDR                   =  20140u,      /* Invalid sock addr.                                   */
    NET_SOCK_ERR_INVALID_ADDR_LEN               =  20141u,      /* Invalid sock addr len.                               */
    NET_SOCK_ERR_ADDR_IN_USE                    =  20142u,      /* Sock addr cur in use.                                */

    NET_SOCK_ERR_INVALID_PORT_NBR               =  20150u,      /* Invalid port nbr.                                    */
    NET_SOCK_ERR_INVALID_PORT_Q_NBR_USED        =  20151u,      /* Invalid nbr Q entries used.                          */
    NET_SOCK_ERR_PORT_NBR_NONE_AVAIL            =  20153u,      /* Port nbr(s) NOT avail.                               */
    NET_SOCK_ERR_PORT_NBR_IN_Q                  =  20154u,      /* Port nbr cur in Q.                                   */

    NET_SOCK_ERR_CONN_IN_USE                    =  20200u,      /* Sock conn cur in use.                                */
    NET_SOCK_ERR_CONN_IN_PROGRESS               =  20201u,      /* Sock conn        NOT complete.                       */
    NET_SOCK_ERR_CONN_CLOSED                    =  20205u,      /* Sock conn closed.                                    */
    NET_SOCK_ERR_CONN_CLOSE_IN_PROGRESS         =  20206u,      /* Sock conn close  NOT complete.                       */

    NET_SOCK_ERR_CONN_FAIL                      =  20210u,      /* Sock conn op         failed.                         */
    NET_SOCK_ERR_CONN_ABORT                     =  20212u,      /* Sock conn abort      failed.                         */

    NET_SOCK_ERR_CONN_SIGNAL                    =  20220u,      /* Sock conn signal     failed.                         */
    NET_SOCK_ERR_CONN_SIGNAL_CLR                =  20221u,      /* Sock conn signal clr failed.                         */
    NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT            =  20222u,      /* Sock conn signal timeout.                            */
    NET_SOCK_ERR_CONN_SIGNAL_ABORT              =  20223u,      /* Sock conn signal aborted.                            */
    NET_SOCK_ERR_CONN_SIGNAL_FAULT              =  20224u,      /* Sock conn signal fault.                              */

    NET_SOCK_ERR_CONN_ACCEPT_Q_NONE_AVAIL       =  20230u,      /* Sock conn accept Q conn id's NOT avail.              */
    NET_SOCK_ERR_CONN_ACCEPT_Q_MAX              =  20232u,      /* Sock conn accept Q max  lim.                         */
    NET_SOCK_ERR_CONN_ACCEPT_Q_DUP              =  20233u,      /* Sock conn accept Q conn id dup.                      */

    NET_SOCK_ERR_SEL_SIGNAL_FAULT               =  20300u,      /* Create Socket Select signal failed.                  */

    NET_SOCK_ERR_RX_Q_CLOSED                    =  20400u,      /* Rx Q closed; i.e.   do NOT rx   pkt(s) to Q.         */
    NET_SOCK_ERR_RX_Q_EMPTY                     =  20401u,      /* Rx Q empty;  i.e.      NO  rx'd pkt(s) in Q.         */
    NET_SOCK_ERR_RX_Q_FULL                      =  20402u,      /* Rx Q full;   i.e. too many rx'd pkt(s) in Q.         */
    NET_SOCK_ERR_RX_Q_ABORT                     =  20405u,      /* Rx Q abort      failed.                              */

    NET_SOCK_ERR_RX_Q_SIGNAL                    =  20510u,      /* Rx Q signal     failed.                              */
    NET_SOCK_ERR_RX_Q_SIGNAL_CLR                =  20511u,      /* Rx Q signal clr failed.                              */
    NET_SOCK_ERR_RX_Q_SIGNAL_TIMEOUT            =  20512u,      /* Rx Q signal timeout.                                 */
    NET_SOCK_ERR_RX_Q_SIGNAL_ABORT              =  20513u,      /* Rx Q signal aborted.                                 */
    NET_SOCK_ERR_RX_Q_SIGNAL_FAULT              =  20514u,      /* Rx Q signal fault.                                   */

    NET_SOCK_ERR_TX_Q_CLOSED                    =  20600u,      /* Tx Q closed; i.e. do NOT Q pkt(s) to tx.             */


/*
---------------------------------------------------------------------------------------------------------
-                                    APPLICATION LAYER ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_APP_ERR_NONE                            =  21000u,      /* Network Application operation successful.            */

    NET_APP_ERR_NONE_AVAIL                      =  21010u,      /* NO application resource(s) available.                */

    NET_APP_ERR_INVALID_ARG                     =  21020u,      /* Invalid application argument(s).                     */
    NET_APP_ERR_INVALID_OP                      =  21021u,      /* Invalid application operation(s).                    */
    NET_APP_ERR_INVALID_ADDR_LEN                =  21022u,      /* Invalid/unknown/unsupported address length.          */
    NET_APP_ERR_INVALID_ADDR_FAMILY             =  21023u,      /* Invalid/unknown/unsupported address family.          */

    NET_APP_ERR_FAULT                           =  21030u,      /* Application fatal      fault.                        */
    NET_APP_ERR_FAULT_TRANSITORY                =  21031u,      /* Application transitory fault.                        */

    NET_APP_ERR_CONN_CLOSED                     =  21040u,      /* Application connection closed.                       */
    NET_APP_ERR_CONN_IN_PROGRESS                =  21041u,      /* Application connection in progress.                  */

    NET_APP_ERR_DATA_BUF_OVF                    =  21050u,      /* Application data buffer overflow ...                 */
                                                                /* ... some data MAY have been discarded.               */

    NET_APP_ERR_CONN_FAIL                      =   21060u,      /* Unable to connect with remote host.                  */


/*
---------------------------------------------------------------------------------------------------------
-                                    NETWORK SECURITY ERROR CODES
---------------------------------------------------------------------------------------------------------
*/

    NET_SECURE_ERR_NONE                         =  22000u,      /* Network security operation successful.               */

    NET_SECURE_ERR_INIT_POOL                    =  22001u,      /* Failed to init mem pool.                             */
    NET_SECURE_ERR_INIT_OS                      =  22002u,      /* Failed to init OS  rsrc(s).                          */
    NET_SECURE_ERR_NOT_AVAIL                    =  22010u,      /* Failed to get secure obj(s) from mem pool.           */

    NET_SECURE_ERR_LOCK_CREATE                  =  22060u,      /* Failed to create  lock.                              */
    NET_SECURE_ERR_LOCK_DEL                     =  22061u,      /* Failed to del     lock.                              */
    NET_SECURE_ERR_LOCK                         =  22062u,      /* Failed to acquire lock.                              */
    NET_SECURE_ERR_UNLOCK                       =  22063u,      /* Failed to release lock.                              */

    NET_SECURE_ERR_HANDSHAKE                    =  22100u,      /* Failed to perform secure handshake.                  */
    NET_SECURE_ERR_BLK_GET                      =  22101u,      /* Failed to get  blk from mem pool.                    */
    NET_SECURE_ERR_BLK_FREE                     =  22102u,      /* Failed to free blk to   mem pool.                    */

    NET_SECURE_ERR_INSTALL                      =  22110u,      /* Keying material failed to install.                   */
    NET_SECURE_ERR_INSTALL_NOT_TRUSTED          =  22111u,      /* Keying material is NOT trusted.                      */
    NET_SECURE_ERR_INSTALL_DATE_EXPIRATION      =  22112u,      /* Keying material is expired.                          */
    NET_SECURE_ERR_INSTALL_DATE_CREATION        =  22113u,      /* Keying material creation date invalid.               */
    NET_SECURE_ERR_INSTALL_CA_SLOT              =  22114u       /* No more CA slot available.                           */

} NET_ERR;


/*
*********************************************************************************************************
*                                   BACKWARD COMPATIBILITY DEFINES
*********************************************************************************************************
*/

#define  NET_ERR_INIT_INCOMPLETE            NET_INIT_ERR_NOT_COMPLETED
#define  NET_SOCK_ERR_NULL_PTR              NET_ERR_FAULT_NULL_PTR
#define  NET_SOCK_ERR_API_DIS               NET_ERR_FAULT_FEATURE_DIS
#define  NET_SOCK_ERR_NULL_SIZE             NET_ERR_FAULT_NULL_PTR
#define  NET_SOCK_ERR_LINK_DOWN             NET_ERR_IF_LINK_DOWN


#define  NET_APP_ERR_NULL_PTR               NET_ERR_FAULT_NULL_PTR
#define  NET_SOCK_ERR_NULL_PTR              NET_ERR_FAULT_NULL_PTR
#define  NET_CONN_ERR_NULL_PTR              NET_ERR_FAULT_NULL_PTR
#define  NET_SOCK_ERR_NULL_SIZE             NET_ERR_FAULT_NULL_PTR
#define  NET_ASCII_ERR_NULL_PTR             NET_ERR_FAULT_NULL_PTR
#define  NET_SECURE_ERR_NULL_PTR            NET_ERR_FAULT_NULL_PTR


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_ERR_MODULE_PRESENT */
