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
*                                        Davicom DM9000 (A/B/E)
*
* Filename : net_dev_dm9000.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_DM9000_MODULE_PRESENT
#define  NET_DEV_DM9000_MODULE_PRESENT

#include  <IF/net_if_ether.h>
#ifdef  NET_IF_ETHER_MODULE_EN


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

#define  NET_DEV_ERR_UNKNOWN_CHIP_REV                  11810    /* Unknown DM9000 derivative detected.                  */


#define  NET_DEV_ERR_INVALID_BUS_MODE           NET_PHY_ERR_INVALID_BUS_MODE
#define  NET_DEV_ERR_INVALID_PHY_ADDR           NET_PHY_ERR_INVALID_ADDR


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_DM9000;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*                                   DEFINED IN PRODUCT'S  net_bsp.c
*********************************************************************************************************
*/

CPU_ADDR  NetDev_CmdPinGet(NET_IF_NBR   if_nbr,
                           NET_ERR     *perr);


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

#endif  /* NET_IF_ETHER_MODULE_EN        */
#endif  /* NET_DEV_DM9000_MODULE_PRESENT */

