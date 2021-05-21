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
*                                            uC/USB-Device
*                                  Communication Device Class (CDC)
*                               Ethernet Emulation Model (EEM) subclass
*
* Filename : net_dev_usbd_cdceem.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This driver requires the uC/USB-Device stack with the CDC-EEM class.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_USBD_CDCEEM_MODULE_PRESENT
#define  NET_DEV_USBD_CDCEEM_MODULE_PRESENT

#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

                                                    /* ------------------- NET USBD CDCEEM DEV CFG -------------------- */
typedef  struct  net_dev_cfg_usbd_cdc_eem {
                                                    /* ----------------- GENERIC  NET DEV CFG MEMBERS ----------------- */
    NET_IF_MEM_TYPE    RxBufPoolType;               /* Rx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE       RxBufLargeSize;              /* Size  of dev rx large buf data areas (in octets).                */
    NET_BUF_QTY        RxBufLargeNbr;               /* Nbr   of dev rx large buf data areas.                            */
    NET_BUF_SIZE       RxBufAlignOctets;            /* Align of dev rx       buf data areas (in octets).                */
    NET_BUF_SIZE       RxBufIxOffset;               /* Offset from base ix to rx data into data area (in octets).       */


    NET_IF_MEM_TYPE    TxBufPoolType;               /* Tx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE       TxBufLargeSize;              /* Size  of dev tx large buf data areas (in octets).                */
    NET_BUF_QTY        TxBufLargeNbr;               /* Nbr   of dev tx large buf data areas.                            */
    NET_BUF_SIZE       TxBufSmallSize;              /* Size  of dev tx small buf data areas (in octets).                */
    NET_BUF_QTY        TxBufSmallNbr;               /* Nbr   of dev tx small buf data areas.                            */
    NET_BUF_SIZE       TxBufAlignOctets;            /* Align of dev tx       buf data areas (in octets).                */
    NET_BUF_SIZE       TxBufIxOffset;               /* Offset from base ix to tx data from data area (in octets).       */


    CPU_ADDR           MemAddr;                     /* Base addr of (Ether dev's) dedicated mem, if avail.              */
    CPU_ADDR           MemSize;                     /* Size      of (Ether dev's) dedicated mem, if avail.              */


    NET_DEV_CFG_FLAGS  Flags;                       /* Opt'l bit flags.                                                 */

                                                    /* ----------------- SPECIFIC NET DEV CFG MEMBERS ----------------- */

    CPU_CHAR           HW_AddrStr[NET_IF_802x_ADDR_SIZE_STR]; /*  Ether IF's dev hw addr str.                           */

    CPU_INT08U         ClassNbr;                    /* Class number.                                                    */
} NET_DEV_CFG_USBD_CDC_EEM;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_USBD_CDCEEM;


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_ETHER_MODULE_EN      */
#endif /* NET_DEV_USBD_CDCEEM_MODULE_PRESENT */

