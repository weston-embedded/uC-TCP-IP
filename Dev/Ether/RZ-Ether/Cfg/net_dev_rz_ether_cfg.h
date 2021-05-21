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
*                                            RZ ETHER 10/100
*
* Filename : net_dev_rz_ether_cfg.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This network device configuration header file is protected from multiple pre-processor
*               inclusion through use of the network module present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  NET_DEV_RZ_ETHER_CFG_MODULE_PRESENT                    /* See Note #1.                                         */
#define  NET_DEV_RZ_ETHER_CFG_MODULE_PRESENT

#include  <IF/net_if_ether.h>


/*
*********************************************************************************************************
*                                    NETWORK DEVICE CONFIGURATION
*
* Note(s) : (1) Configurations are done for board/application setup and it must be updated according to
*               application/projects requirements.
*********************************************************************************************************
*/

                                                                /* Declare each specific devices' cfg (see Note #1) :   */

extern  const  NET_DEV_CFG_ETHER  NetDev_Cfg_RTK772100FC;       /*   Example Ethernet     cfg for RTK772100FC board     */
extern  const  NET_PHY_CFG_ETHER  NetPhy_Cfg_RTK772100FC;       /*   Example Ethernet Phy cfg for RTK772100FC board     */


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_dev_cfg.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of net dev cfg module include.                   */

