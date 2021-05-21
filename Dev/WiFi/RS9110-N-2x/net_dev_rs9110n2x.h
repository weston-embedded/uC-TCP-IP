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
*                                             RS9110-N-2x
*
* Filename : net_dev_rs9110n2x.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.00.00 (or more recent version) is included in the project build.
*
*            (2) Device driver compatible with these hardware:
*
*                (a) RS9110-N-22
*                (b) RS9110-N-23
*
*            (3) Redpine firmware V4.7.1 command:
*
*              (a) Band.
*                  (1) RS9110-N-22 support only band 2.4 Ghz.
*                  (2) RS9110-N-23 support band 2.4 Ghz and 5Ghz
*
*                  The band is set when the device is started (NetDev_Start()). Currently it's NOT possible to
*                  change the band after the start is completed.
*
*              (b) The initialization command is sent when the device is started (NetDev_Start()).
*
*              (c) Scan is FULLY supported.
*
*              (d) Join
*
*              (c) Query MAC address and Netowrk Type of Scanned Networks is NOT currently supported.
*
*              (d) Scan
*
*              (e) Power Save
*
*              (f) Query Connection status
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_RS9110N2X_MODULE_PRESENT
#define  NET_DEV_RS9110N2X_MODULE_PRESENT
#ifdef   NET_IF_WIFI_MODULE_EN

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../Manager/Generic/net_wifi_mgr.h"


/*
*********************************************************************************************************
*                                      DEVICE DRIVER ERROR CODES
*
* Note(s) : (1) ALL device-independent error codes #define'd in      'net_err.h';
*               ALL device-specific    error codes #define'd in this 'net_dev_&&&.h'.
*
*           (2) Network error code '11,000' series reserved for network device drivers.
*               See 'net_err.h  NETWORK DEVICE ERROR CODES' to ensure that device-specific
*               error codes do NOT conflict with device-independent error codes.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_WIFI  NetDev_API_RS9110N2x;


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_WIFI_MODULE_EN            */
#endif /* NET_DEV_RS9110N2X_MODULE_PRESENT */
