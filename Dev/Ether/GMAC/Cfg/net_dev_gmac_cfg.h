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
*                                            GMAC 10/100
*
* Filename : net_dev_gmac_cfg.h
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

#ifndef  NET_DEV_GMAC_CFG_MODULE_PRESENT                        /* See Note #1.                                         */
#define  NET_DEV_GMAC_CFG_MODULE_PRESENT


/*
*********************************************************************************************************
*                                    NETWORK DEVICE CONFIGURATION
*
* Note(s) : (1) Configurations are done for board/application setup and it must be updated according to
*               application/projects requirements.
*********************************************************************************************************
*/

                                                                /* Declare each specific devices' configuration (see Note #1) : */

extern  const  NET_DEV_CFG_ETHER  NetDev_Cfg_SK_FM3_176PMC_ETH; /*   Example Ethernet     cfg for SK-FM3-176PMC-ETH board       */
extern  const  NET_PHY_CFG_ETHER  NetPhy_Cfg_SK_FM3_176PMC_ETH; /*   Example Ethernet Phy cfg for SK-FM3-176PMC-ETH board       */

extern  const  NET_DEV_CFG_ETHER  NetDev_Cfg_MCB1800;           /*   Example Ethernet     cfg for MCB1800 board                 */
extern  const  NET_PHY_CFG_ETHER  NetPhy_Cfg_MCB1800;           /*   Example Ethernet Phy cfg for MCB1800 board                 */

extern  const  NET_DEV_CFG_ETHER  NetDev_Cfg_LPC_4350_DB1;      /*   Example Ethernet     cfg for LPC-4350-DB1 board            */
extern  const  NET_PHY_CFG_ETHER  NetPhy_Cfg_LPC_4350_DB1;      /*   Example Ethernet Phy cfg for LPC-4350-DB1 board            */

extern  const  NET_DEV_CFG_ETHER  NetDev_Cfg_uCEVALSTM32F107;   /*   Example Ethernet     cfg for uC-Eval-STM32F107 board       */
extern  const  NET_PHY_CFG_ETHER  NetPhy_Cfg_uCEVALSTM32F107;   /*   Example Ethernet Phy cfg for uC-Eval-STM32F107 board       */

/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_dev_cfg.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of net dev cfg module include.                   */

