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
*                                       LUMINARY MICRO LM3S9Bxx
*
* Filename : net_dev_lm3s9bxx.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) The driver support DMA using the Micro Drect Memory Access (uDMA) module.
*
*            (3) Only the following devices are supported by this driver:
*
*                    LMS9B90.
*                    LMS9B92.
*                    LMS9B95.
*                    LMS9B96.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_LM3S9Bxx_MODULE_PRESENT
#define  NET_DEV_LM3S9Bxx_MODULE_PRESENT

#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                 MICRO DIRECT MEMORY ACCESS (uDMA) DEFINES
*********************************************************************************************************
*/

#define  NET_DEV_LM3S9Bxx_DMA_BASE_ADDR          0x400FF000
#define  NET_DEV_LM3S9Bxx_DMA_RX_CH                       6u
#define  NET_DEV_LM3S9Bxx_DMA_TX_CH                       7u
#define  NET_DEV_LM3S9Bxx_DMA_NBR_CH                     64u    /* 32 Primary channels + 32 Alternative channels        */



/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_LM3S9Bxx;


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*
* Note(s) : (1) The received packet is organized in the LM3S9Bxx ethernet controller's FIFO as:
*
*       +=================+======+==================+
*       | FIFO Word Rd/Wr | Byte |      RX FIFO     |
*       |=================|======+==================+                          +=========+          -----
*       |       1st       |  0   | Frame Length LSB |                          | xxxxxxx | + 0x00     ^
*       |                 |  1   | Frame Length MSB |                          | xxxxxxx | + 0x01     |
*       |                 |  2   |    DA Octet 1    |     -----                | Octet 1 | + 0x02     |
*       |                 |  3   |    DA Octet 2    |       ^                  | Octet 2 | + 0x03     |
*       +=================|======|==================+       |       -----      |=========|            |
*       |       2st       |  0   |    DA Octet 3    |       |         ^        | Octet 3 | + 0x04     |
*       |                 |  1   |    DA Octet 4    |       |         |        | Octet 4 | + 0x05     |
*       |                 |  2   |    DA Octet 5    |       |         |        | Octet 5 | + 0x06     |
*       |                 |  3   |    DA Octet 6    |       |         |        | Octet 6 | + 0x07     |
*       +=================|======|==================+       |         |        |=========|            |
*                            .                                                 |    .    |
*                            .                           Ethernet    DMA       |    .    |         Network
*                            .                            Frame    Transfer    |    .    |         Buffer
*                            .                                                 |    .    |
*       +=================|======|==================+       |         |        |    .    |            |
*       |       Last      |  0   |      FCS 1a      |       |         |        |    .    |            |
*       |                 |  1   |      FCS 2a      |       |         |        |    .    |            |
*       |                 |  2   |      FCS 3a      |       |         |        |    .    |            |
*       |                 |  3   |      FCS 4a      |       v         v        |    .    |            v
*       +=================|======|==================+     -----     -----      +=========+          -----
*
*               because the size of a received packet is not known until the header is examined,
*               the frame size must be determined first. Once the packet length is known, a
*               DMA transfer can be set up to transfer the remaining received packet payload from
*               the FIFO into a buffer.
*
*               Since DMA transfer MUST be 32-bit aligned then the network buffer must be 32-bit
*               aligned and the data section should start at index 2.
*
*               See also 'net_dev_lm3s9bxx.c  NetDev_Init()  Note #2'.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_ETHER_MODULE_EN          */
#endif /* NET_DEV_LM3S9Bxx_MODULE_PRESENT */

