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
*                                  NETWORK LOOPBACK INTERFACE LAYER
*
* Filename : net_if_loopback.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports internal loopback communication.
*
*                (a) Internal loopback interface is NOT linked to, associated with, or handled by
*                    any physical network device(s) & therefore has NO physical protocol overhead.
*
*            (2) REQUIREs the following network protocol files in network directories :
*
*                (a) Network Interface Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\IF\
*
*                         where
*                                 <Network Protocol Suite>    directory path for network protocol suite
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) Loopback Interface Layer module is included regardless of whether the loopback interface
*               is enabled (see 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #2a1').
*
*           (2) The following loopback-module-present configuration value MUST be pre-#define'd in
*               'net_cfg_net.h' PRIOR to all other network modules that require loopback configuration
*               (see 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #2a1') :
*
*                   NET_IF_LOOPBACK_MODULE_EN
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IF_LOOPBACK_MODULE_PRESENT
#define  NET_IF_LOOPBACK_MODULE_PRESENT



/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "../Source/net_cfg_net.h"
#include  "net_if.h"
#include  "net_if_802x.h"
#include  "../Source/net_type.h"
#include  "../Source/net_stat.h"
#include  "../Source/net_buf.h"

#ifdef   NET_IF_LOOPBACK_MODULE_EN                          /* See Note #2.                                         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_IF_LOOPBACK_BUF_RX_LEN_MIN     NET_IF_ETHER_FRAME_MIN_SIZE
#define  NET_IF_LOOPBACK_BUF_TX_LEN_MIN     NET_IF_802x_BUF_TX_LEN_MIN


/*
*********************************************************************************************************
*                                  NETWORK INTERFACE HEADER DEFINES
*
* Note(s) : (1) The following network interface value MUST be pre-#define'd in 'net_def.h' PRIOR to
*               'net_cfg.h' so that the developer can configure the network interface for the correct
*               network interface layer values (see 'net_def.h  NETWORK INTERFACE LAYER DEFINES' &
*               'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #4') :
*
*               (a) NET_IF_HDR_SIZE_LOOPBACK               0
*********************************************************************************************************
*/

#if 0
#define  NET_IF_HDR_SIZE_LOOPBACK                          0    /* See Note #1a.                                        */
#endif

#define  NET_IF_HDR_SIZE_LOOPBACK_MIN                    NET_IF_HDR_SIZE_LOOPBACK
#define  NET_IF_HDR_SIZE_LOOPBACK_MAX                    NET_IF_HDR_SIZE_LOOPBACK


/*
*********************************************************************************************************
*              NETWORK LOOPBACK INTERFACE SIZE & MAXIMUM TRANSMISSION UNIT (MTU) DEFINES
*
* Note(s) : (1) The loopback interface is NOT linked to, associated with, or handled by any physical
*               network device(s) & therefore has NO physical protocol overhead.
*
*               See also 'net_if_loopback.h  Note #1a'.
*********************************************************************************************************
*/
                                                                /* See Note #1.                                         */
#define  NET_IF_MTU_LOOPBACK                             NET_MTU_MAX_VAL

#define  NET_IF_LOOPBACK_BUF_SIZE_MIN                   (NET_IF_LOOPBACK_SIZE_MIN + NET_BUF_DATA_SIZE_MIN - NET_BUF_DATA_PROTOCOL_HDR_SIZE_MIN)


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                          NETWORK LOOPBACK INTERFACE CONFIGURATION DATA TYPE
*
* Note(s) : (1) The network loopback interface configuration data type is a specific instantiation of a
*               network device configuration data type.  ALL specific network device configuration data
*               types MUST be defined with ALL of the generic network device configuration data type's
*               configuration parameters, synchronized in both the sequential order & data type of each
*               parameter.
*
*               Thus ANY modification to the sequential order or data types of generic configuration
*               parameters MUST be appropriately synchronized between the generic network device
*               configuration data type & the network loopback interface configuration data type.
*
*               See also 'net_if.h  GENERIC NETWORK DEVICE CONFIGURATION DATA TYPE  Note #1'.
*********************************************************************************************************
*/

                                                    /* --------------------- NET LOOPBACK IF CFG ---------------------- */
typedef  struct  net_if_cfg_loopback {
                                                    /* ---------------- GENERIC  LOOPBACK CFG MEMBERS ----------------- */
    NET_IF_MEM_TYPE     RxBufPoolType;              /* Rx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE        RxBufLargeSize;             /* Size  of loopback IF large buf data areas     (in octets).       */
    NET_BUF_QTY         RxBufLargeNbr;              /* Nbr   of loopback IF large buf data areas.                       */
    NET_BUF_SIZE        RxBufAlignOctets;           /* Align of loopback IF       buf data areas     (in octets).       */
    NET_BUF_SIZE        RxBufIxOffset;              /* Offset from base ix to rx data into data area (in octets).       */


    NET_IF_MEM_TYPE     TxBufPoolType;              /* Tx buf mem pool type :                                           */
                                                    /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                    /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE        TxBufLargeSize;             /* Size  of loopback IF large buf data areas     (in octets).       */
    NET_BUF_QTY         TxBufLargeNbr;              /* Nbr   of loopback IF large buf data areas.                       */
    NET_BUF_SIZE        TxBufSmallSize;             /* Size  of loopback IF small buf data areas     (in octets).       */
    NET_BUF_QTY         TxBufSmallNbr;              /* Nbr   of loopback IF small buf data areas.                       */
    NET_BUF_SIZE        TxBufAlignOctets;           /* Align of loopback IF       buf data areas     (in octets).       */
    NET_BUF_SIZE        TxBufIxOffset;              /* Offset from base ix to tx data from data area (in octets).       */


    CPU_ADDR            MemAddr;                    /* Base addr of (loopback IF's) dedicated mem, if avail.            */
    CPU_ADDR            MemSize;                    /* Size      of (loopback IF's) dedicated mem, if avail.            */


    NET_DEV_CFG_FLAGS   Flags;                      /* Opt'l bit flags.                                                 */

                                                    /* ---------------- SPECIFIC LOOPBACK CFG MEMBERS ----------------- */

} NET_IF_CFG_LOOPBACK;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

extern  const  NET_IF_CFG_LOOPBACK   NetIF_Cfg_Loopback;        /*    Net loopback IF cfg.                              */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     EXTERNAL C LANGUAGE LINKAGE
*
* Note(s) : (1) C++ compilers MUST 'extern'ally declare ALL C function prototypes & variable/object
*               declarations for correct C language linkage.
*********************************************************************************************************
*/

#ifdef  __cplusplus
extern "C" {
#endif

/*
*********************************************************************************************************
*                                             PUBLIC API
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void          NetIF_Loopback_Init(NET_ERR  *p_err);


                                                                /* --------------------- RX FNCTS --------------------- */
NET_BUF_SIZE  NetIF_Loopback_Rx  (NET_IF   *p_if,
                                  NET_ERR  *p_err);

                                                                /* --------------------- TX FNCTS --------------------- */
NET_BUF_SIZE  NetIF_Loopback_Tx  (NET_IF   *p_if,
                                  NET_BUF  *p_buf_tx,
                                  NET_ERR  *p_err);


/*
*********************************************************************************************************
*                                   EXTERNAL C LANGUAGE LINKAGE END
*********************************************************************************************************
*/

#ifdef  __cplusplus
}
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IF_CFG_LOOPBACK_EN
#error  "NET_IF_CFG_LOOPBACK_EN        not #define'd in 'net_cfg.h'"
#error  "                        [MUST be  DEF_DISABLED]           "
#error  "                        [     ||  DEF_ENABLED ]           "

#elif  ((NET_IF_CFG_LOOPBACK_EN != DEF_DISABLED) && \
        (NET_IF_CFG_LOOPBACK_EN != DEF_ENABLED ))
#error  "NET_IF_CFG_LOOPBACK_EN  illegally #define'd in 'net_cfg.h'"
#error  "                        [MUST be  DEF_DISABLED]           "
#error  "                        [     ||  DEF_ENABLED ]           "
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_IF_LOOPBACK_MODULE_EN        */
#endif  /* NET_IF_LOOPBACK_MODULE_PRESENT   */

