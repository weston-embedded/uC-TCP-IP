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
*                                            GMAC 10/100
*
* Filename : net_dev_gmac.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_GMAC_MODULE_PRESENT
#define  NET_DEV_GMAC_MODULE_PRESENT

#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
* Note(s) : (1) The following MCUs are support by NetDev_API_GMAC API:
*
*                          STMicroelectronics  STM32F107xx  series
*                          STMicroelectronics  STM32F2xxx   series
*                          STMicroelectronics  STM32F4xxx   series
*                          STMicroelectronics  STM32F74xxx  series
*                          STMicroelectronics  STM32F75xxx  series
*                          FUJITSU             MB9BFD10T    series
*                          FUJITSU             MB9BF610T    series
*                          FUJITSU             MB9BF210T    series
*
*           (2) The following MCUs are support by NetDev_API_LPCXX_ENET API:
*
*                          NXP                 LPC185x      series
*                          NXP                 LPC183x      series
*                          NXP                 LPC435x      series
*                          NXP                 LPC433x      series
*
*           (3) The following MCUs are support by NetDev_API_XMC4000_ENET API:
*
*                          INFINEON            XMC4500      series
*                          INFINEON            XMC4400      series
*
*           (4) The following MCUs are support by NetDev_API_TM4C12X_ENET API:
*
*                          TEXAS INSTRUMENTS   TM4C12x      series
*
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_GMAC;
extern  const  NET_DEV_API_ETHER  NetDev_API_LPCXX_ENET;
extern  const  NET_DEV_API_ETHER  NetDev_API_XMC4000_ENET;
extern  const  NET_DEV_API_ETHER  NetDev_API_TM4C12X_ENET;
extern  const  NET_DEV_API_ETHER  NetDev_API_HPS_EMAC;
extern  const  NET_DEV_API_ETHER  NetDev_API_STM32Fx_EMAC;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_ETHER_MODULE_EN      */
#endif /* NET_DEV_GMAC_MODULE_PRESENT */
