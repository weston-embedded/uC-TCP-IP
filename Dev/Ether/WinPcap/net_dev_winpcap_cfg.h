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
*                                  NETWORK DEVICE CONFIGURATION FILE
*
*                                              TEMPLATE
*
* Filename : net_dev_winpcap_cfg.h
* Version  : V3.06.01
*********************************************************************************************************
*/

#include  <Source/net_cfg_net.h>


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This network device configuration header file is protected from multiple pre-processor
*               inclusion through use of the network module present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  NET_DEV_CFG_MODULE_PRESENT                             /* See Note #1.                                         */
#define  NET_DEV_CFG_MODULE_PRESENT


/*
*********************************************************************************************************
*                                    NETWORK DEVICE CONFIGURATION
*
* Note(s) : (1) (a) Each network device maps to a unique, developer-configured device configuration that
*                   MUST be defined in application files, typically 'net_dev_cfg.c', & SHOULD be forward-
*                   declared with the exact same name & type in order to be used by the application during
*                   calls to NetIF_Add().
*
*               (b) Since these device configuration structures are referenced ONLY by application files,
*                   there is NO required naming convention for these configuration structures.  However,
*                   the following naming convention is suggested for all developer-configured network
*                   device configuration structures :
*
*                       NetDev_Cfg_<Device>[_Number]
*
*                           where
*                                   <Device>        Name of device or device driver
*                                   [Number]        Network device number for each specific instance of
*                                                       device (optional if the development board does NOT
*                                                       support multiple instances of the specific device)
*
*                       Examples :
*
*                           NET_DEV_CFG_ETHER  NetDev_Cfg_MACB;         Ethernet configuration for MACB
*
*                           NET_DEV_CFG_ETHER  NetDev_Cfg_FEC_0;        Ethernet configuration for FEC #0
*                           NET_DEV_CFG_ETHER  NetDev_Cfg_FEC_1;        Ethernet configuration for FEC #1
*
*                           NET_DEV_CFG_WIFI   NetDev_Cfg_RS9110N21_0;  Wireless configuration for RS9110-N-21
*
*                           NET_DEV_CFG_PPP    NetDev_Cfg_UART_2;       PPP      configuration for UART #2
*********************************************************************************************************
*/

                                                        /* Declare each specific devices' configuration (see Note #1) : */
#ifdef  NET_IF_ETHER_MODULE_EN

extern  NET_DEV_CFG_ETHER  NetDev_Cfg_WinPcap_Dflt;     /*   Example Ethernet     configuration for device A, #1        */

#endif


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_dev_cfg.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of net dev cfg module include.                   */

