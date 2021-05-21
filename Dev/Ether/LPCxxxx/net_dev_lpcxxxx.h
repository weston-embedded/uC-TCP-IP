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
*                                               LPCxxxx
*
* Filename : net_dev_lpcxxxx.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) The following parts may use this 'net_dev_lpcxxxx' device driver :
*
*                  * LPC23xx Series
*                  * LPC24xx Series
*                  * LPC32x0 Series
*                  * LPC17xx Series
*                  * LPC408x Series
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_LPCxxxx_MODULE_PRESENT
#define  NET_DEV_LPCxxxx_MODULE_PRESENT

#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_LPCxxxx;           /* Generic LPCxxxx device API.                          */
extern  const  NET_DEV_API_ETHER  NetDev_API_LPC1758;           /*         LPC1758 device API.                          */


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_ETHER_MODULE_EN         */
#endif /* NET_DEV_LPCxxxx_MODULE_PRESENT */

