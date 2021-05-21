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
*                                            uC/USB-Device
*                                  Communication Device Class (CDC)
*                               Ethernet Emulation Model (EEM) subclass
*
* Filename : net_dev_usbd_cdceem_cfg.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Dev/Ether/USBD_CDCEEM/net_dev_usbd_cdceem.h>


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This network device configuration header file is protected from multiple pre-processor
*               inclusion through use of the network module present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  NET_DEV_USBD_CDCEEM_CFG_MODULE_PRESENT                 /* See Note #1.                                                 */
#define  NET_DEV_USBD_CDCEEM_CFG_MODULE_PRESENT


/*
*********************************************************************************************************
*                                    NETWORK DEVICE CONFIGURATION
*
* Note(s) : (1) Configurations are done for board/application setup and it must be updated according to
*               application/projects requirements.
*********************************************************************************************************
*/

                                                                /* Declare each specific devices' configuration (see Note #1) : */
extern  NET_DEV_CFG_USBD_CDC_EEM  NetDev_Cfg_Ether_USBD_CDCEEM; /* Example Ethernet cfg for USB Device CDC EEM class.           */


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_dev_cfg.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of net dev cfg module include.                   */

