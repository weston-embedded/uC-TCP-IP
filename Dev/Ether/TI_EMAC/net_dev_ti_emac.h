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
*                                          TEXAS INSTRUMENTS
*                                 ETHERNET MEDIA ACCESS CONTROLLER (EMAC)
*
* Filename : net_dev_ti_emac.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_TI_EMAC_MODULE_PRESENT
#define  NET_DEV_TI_EMAC_MODULE_PRESENT

#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
* Note(s) : (1) The following MCUs are support by NetDev_API_TI_EMAC API:
*
*                          TEXAS INSTRUMENTS   RMxx         series
*                          TEXAS INSTRUMENTS   TMS570LSxx   series
*
*           (2) The following MCUs are support by NetDev_API_AM35xx_EMAC API:
*
*                          TEXAS INSTRUMENTS   AM35xx       series
*
*           (3) The following MCUs are support by NetDev_API_OMAP_L1x_EMAC API:
*
*                          TEXAS INSTRUMENTS   OMAP-L1x     series
*
*           (4) The following MCUs are support by NetDev_API_TMS570LCxx_EMAC API:
*
*                          TEXAS INSTRUMENTS   TMS570LCxx   series
*
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_TI_EMAC;
extern  const  NET_DEV_API_ETHER  NetDev_API_AM35xx_EMAC;
extern  const  NET_DEV_API_ETHER  NetDev_API_OMAP_L1x_EMAC;
extern  const  NET_DEV_API_ETHER  NetDev_API_TMS570LCxx_EMAC;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* NET_IF_ETHER_MODULE_EN                               */
#endif                                                          /* NET_DEV_TI_EMAC_MODULE_PRESENT                       */
