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
*                                      ACTEL SMART FUSION A2Fx00
*
* Filename : net_dev_a2fx00.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_A2FX00_MODULE_PRESENT
#define  NET_DEV_A2FX00_MODULE_PRESENT

#include  <cpu_core.h>
#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_A2FX00;


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*
* Note(s) : (1) RMII signals are generated in software generation are perform in software
                In order to generate 2.5Mhz signals CPU's time stamp feauture MUST be enabled.
*********************************************************************************************************
*/

#if     (CPU_CFG_TS_EN != DEF_ENABLED)
#error  "CPU_CFG_TS_EN  illegally #define'd in 'cpu_cfg.h'"
#error  "               [MUST be  DEF_ENABLED ]           "
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_ETHER_MODULE_EN        */
#endif  /* NET_DEV_A2FX00_MODULE_PRESENT */

