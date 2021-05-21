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
*                                          Renesas RX-EtherC
*
* Filename : net_dev_rx_etherc.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.00.00 (or more recent version) is included in the project build.
*
*            (2) The following parts may use this 'net_dev_rx_etherc' device driver :
*
*                (a) RX62N
*                (b) RX63N
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_RX_ETHERC_MODULE_PRESENT
#define  NET_DEV_RX_ETHERC_MODULE_PRESENT

#ifdef  NET_IF_ETHER_MODULE_EN
#include <IF/net_if_ether.h>
/*
*********************************************************************************************************
*                                      DEVICE DRIVER ERROR CODES
*
* Note(s) : (1) ALL device-independent error codes #define'd in      'net_err.h';
*               ALL device-specific    error codes #define'd in this 'net_dev_&&&.h'.
*
*           (2) Network error code '11,000' series reserved for network device drivers.
*               See 'net_err.h  NETWORK DEVICE ERROR CODES' to ensure that device-specific
*               error codes do NOT conflict with device-independent error codes.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/
extern  const  NET_DEV_API_ETHER  NetDev_API_RX_EtherC;
extern  const  NET_DEV_API_ETHER  NetDev_API_RX64_EtherC;


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                             ETHERNET DEVICE DATA TYPE SPECIFIC TO THE RX64
*
* Note(s) : (1) The Ethernet interface configuration data type is a specific definition of a network
*               device configuration data type.  Each specific network device configuration data type
*               MUST define ALL generic network device configuration parameters, synchronized in both
*               the sequential order & data type of each parameter.
*
*               Thus ANY modification to the sequential order or data types of generic configuration
*               parameters MUST be appropriately synchronized between the generic network device
*               configuration data type & the Ethernet interface configuration data type.
*
*               See also 'net_if.h  GENERIC NETWORK DEVICE CONFIGURATION DATA TYPE  Note #1'.
*********************************************************************************************************
*/

                                                    /* ---------------------- NET ETHER DEV CFG ----------------------- */
typedef  struct  net_dev_cfg_ether_RX64 {
                                                    /* ----------------- GENERIC  NET DEV CFG MEMBERS ----------------- */
    NET_IF_MEM_TYPE     RxBufPoolType;              /* Rx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE        RxBufLargeSize;             /* Size  of dev rx large buf data areas (in octets).                */
    NET_BUF_QTY         RxBufLargeNbr;              /* Nbr   of dev rx large buf data areas.                            */
    NET_BUF_SIZE        RxBufAlignOctets;           /* Align of dev rx       buf data areas (in octets).                */
    NET_BUF_SIZE        RxBufIxOffset;              /* Offset from base ix to rx data into data area (in octets).       */


    NET_IF_MEM_TYPE     TxBufPoolType;              /* Tx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE        TxBufLargeSize;             /* Size  of dev tx large buf data areas (in octets).                */
    NET_BUF_QTY         TxBufLargeNbr;              /* Nbr   of dev tx large buf data areas.                            */
    NET_BUF_SIZE        TxBufSmallSize;             /* Size  of dev tx small buf data areas (in octets).                */
    NET_BUF_QTY         TxBufSmallNbr;              /* Nbr   of dev tx small buf data areas.                            */
    NET_BUF_SIZE        TxBufAlignOctets;           /* Align of dev tx       buf data areas (in octets).                */
    NET_BUF_SIZE        TxBufIxOffset;              /* Offset from base ix to tx data from data area (in octets).       */


    CPU_ADDR            MemAddr;                    /* Base addr of (Ether dev's) dedicated mem, if avail.              */
    CPU_ADDR            MemSize;                    /* Size      of (Ether dev's) dedicated mem, if avail.              */


    NET_DEV_CFG_FLAGS   Flags;                      /* Opt'l bit flags.                                                 */

                                                    /* ----------------- SPECIFIC NET DEV CFG MEMBERS ----------------- */
    NET_BUF_QTY         RxDescNbr;                  /* Nbr rx dev desc's.                                               */
    NET_BUF_QTY         TxDescNbr;                  /* Nbr tx dev desc's.                                               */


    CPU_ADDR            BaseAddr;                   /* Base addr of Ether dev hw/regs.                                  */

    CPU_DATA            DataBusSizeNbrBits;         /* Size      of Ether dev's data bus (in bits), if avail.           */

    CPU_CHAR            HW_AddrStr[NET_IF_802x_ADDR_SIZE_STR]; /*  Ether IF's dev hw addr str.                          */

    CPU_ADDR            PTPRSTRegister;                /* EPTPC software reset register PTRSTR address                     */

    CPU_ADDR             SYMACRURegister;            /* EPTC MAC Frame filtering upper register SYMACRU address          */

    CPU_ADDR             PHYCtrl_BaseAddr;            /* Base addr to the Ether dev which control MDIO and MDC to the PHY */

} NET_DEV_CFG_ETHER_RX64;

/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_ETHER_MODULE_EN           */
#endif  /* NET_DEV_RX_ETHERC_MODULE_PRESENT */
