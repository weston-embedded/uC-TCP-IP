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
*                                 NETWORK DEVICE ADDRESS CONIFGURATION
*
*                                    Altera Triple Speed Ethernet
*
* Filename : net_dev_tse_addr_cfg.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_TSE_ADDR_CFG_MODULE_PRESENT
#define  NET_DEV_TSE_ADDR_CFG_MODULE_PRESENT

#include  <system.h>                                            /* You may have to modify the include accordingly ...   */
                                                                /* ... with you project and your ipcore generator.      */


/*
*********************************************************************************************************
*                                          GLOBAL DEFINES
*********************************************************************************************************
*/

                                                                /* You may have to modify these #define accordingly ... */
                                                                /* ... with the header file included.                   */

#define  BSP_SGDMA_RX_BASEADDR                  SGDMA_RX_BASE   /* Specify the Rx SGDMA base address.                   */
#define  BSP_SGDMA_TX_BASEADDR                  SGDMA_TX_BASE   /* Specify the Tx SGDMA base address.                   */
#define  BSP_SGDMA_RX_NAME                      SGDMA_RX_NAME   /* Specify the Rx SGDMA HAL device name.                */
#define  BSP_SGDMA_TX_NAME                      SGDMA_TX_NAME   /* Specify the Tx SGDMA HAL device name.                */

                                                                /* Specifiy the TSE Tx FIFO depth                       */
#define  BSP_TSE_MAX_TRANSMIT_FIFO_DEPTH        TSE_MAC_TRANSMIT_FIFO_DEPTH
                                                                /* Specifiy the TSE Rx FIFO depth                       */
#define  BSP_TSE_MAX_RECEIVE_FIFO_DEPTH         TSE_MAC_RECEIVE_FIFO_DEPTH


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of Ether DMA template module include.            */

