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
*                                               LAN911x
*
* Filename : net_dev_lan911x.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) Supports LAN911{5,6,7,8} Ethernet controller as described in
*
*                    Standard Microsystems Corporation's (SMSC; http://www.smsc.com)
*                    (a) LAN9115 data sheet              (SMSC LAN9115; Revision 1.1 05/17/2005)
*
*            (3) It is assumed that the LAN911x is attached to a 32 bit little-endian host processor
*                and that the data lines are NOT byte swapped.
*
*                                   -----------                 -----------
*                                   | LAN911x |                 |   CPU   |
*                                   |         |                 |         |
*                                   |         |                 |         |
*                                   |    D07- |        8        | D07-    |
*                                   |    D00  |--------/--------| D00     |
*                                   |         |                 |         |
*                                   |    D15- |        8        | D15-    |
*                                   |    D08  |--------/--------| D08     |
*                                   |         |                 |         |
*                                   -----------                 -----------
*
*            (4) #### Power Management Events are not yet implemented
*                #### PHY interrupts are not enabled (no knowledge of link state changes)
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_LAN911x_MODULE_PRESENT
#define  NET_DEV_LAN911x_MODULE_PRESENT

#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_LAN911x;



/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_ETHER_MODULE_EN         */
#endif  /* NET_DEV_LAN911x_MODULE_PRESENT */

