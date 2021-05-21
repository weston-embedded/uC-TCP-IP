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
*                                       NETWORK INTERFACE LAYER
*
*                                                802x
*
* Filename : net_if_802x.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Implements code common to the following network interface layers :
*
*                (a) Ethernet as described in RFC # 894
*                (b) IEEE 802 as described in RFC #1042
*
*            (2) Ethernet implementation conforms to RFC #1122, Section 2.3.3, bullets (a) & (b), but
*                does NOT implement bullet (c) :
*
*                RFC #1122                  LINK LAYER                  October 1989
*
*                2.3.3  ETHERNET (RFC-894) and IEEE 802 (RFC-1042) ENCAPSULATION
*
*                       Every Internet host connected to a 10Mbps Ethernet cable :
*
*                       (a) MUST be able to send and receive packets using RFC-894 encapsulation;
*
*                       (b) SHOULD be able to receive RFC-1042 packets, intermixed with RFC-894 packets; and
*
*                       (c) MAY be able to send packets using RFC-1042 encapsulation.
*
*            (3) REQUIREs the following network protocol files in network directories :
*
*                (a) Network Interface Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\IF\
*
*                (b) Address Resolution Protocol    Layer located in the following network directory :
*
*                        \<Network Protocol Suite>\
*
*                    See also 'net_arp.h  Note #1'.
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
* Note(s) : (1) 802x Interface module is included only if support for any of the following devices
*               is configured :
*
*               (a) Ethernet
*               (b) Wireless
*
*               See 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #2a2'.
*
*           (2) The following 802x-module-present configuration value MUST be pre-#define'd in
*               'net_cfg_net.h' PRIOR to all other network modules that require 802x configuration :
*
*                   NET_IF_802x_MODULE_EN
*
*               See 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #2a2'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IF_802x_MODULE_PRESENT                             /* See Note #2c.                                        */
#define  NET_IF_802x_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_if.h"
#include  "../Source/net_cfg_net.h"
#include  "../Source/net_ascii.h"
#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_arp.h"                                /* See 'net_if_802x.h  Note #3b'.                       */
#endif
#ifdef   NET_IF_802x_MODULE_EN                                  /* See Note #2.                                         */

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

/*
*********************************************************************************************************
*                                  NETWORK INTERFACE / 802x DEFINES
*********************************************************************************************************
*/

#define  NET_IF_802x_ADDR_SIZE                        NET_IF_802x_HW_ADDR_LEN   /* Size of 48-bit Ether MAC addr (in octets).   */
#define  NET_IF_802x_ADDR_SIZE_STR                    NET_ASCII_LEN_MAX_ADDR_MAC
#define  NET_IF_ETHER_ADDR_SIZE                       NET_IF_802x_ADDR_SIZE     /* Req'd for backwards-compatibility.   */



/*
*********************************************************************************************************
*                         802x SIZE & MAXIMUM TRANSMISSION UNIT (MTU) DEFINES
*
* Note(s) : (1) (a) RFC #894, Section 'Frame Format' & RFC #1042, Section 'Frame Format and MAC Level Issues :
*                   For IEEE 802.3' specify the following range on Ethernet & IEEE 802 frame sizes :
*
*                   (1) Minimum frame size =   64 octets
*                   (2) Maximum frame size = 1518 octets
*
*               (b) Since the 4-octet CRC trailer included in the specified minimum & maximum frame sizes is
*                   NOT necessarily included or handled by the network protocol suite, the minimum & maximum
*                   frame sizes for receive & transmit packets is adjusted by the CRC size.
*********************************************************************************************************
*/

#define  NET_IF_802x_FRAME_CRC_SIZE                        4
#define  NET_IF_ETHER_FRAME_CRC_SIZE                     NET_IF_802x_FRAME_CRC_SIZE /* Req'd for backwards-compatibility.   */

#define  NET_IF_802x_FRAME_MIN_CRC_SIZE                   64    /* See Note #1a1.                                           */
#define  NET_IF_802x_FRAME_MIN_SIZE                     (NET_IF_802x_FRAME_MIN_CRC_SIZE - NET_IF_802x_FRAME_CRC_SIZE)
#define  NET_IF_ETHER_FRAME_MIN_SIZE                     NET_IF_802x_FRAME_MIN_SIZE

#define  NET_IF_802x_FRAME_MAX_CRC_SIZE                 1518    /* See Note #1a2.                                           */

                                                                /* Must keep for backyard compatibility.                    */
#define  NET_IF_ETHER_FRAME_MAX_CRC_SIZE                 NET_IF_802x_FRAME_MAX_CRC_SIZE
#define  NET_IF_ETHER_FRAME_MAX_SIZE                    (NET_IF_802x_FRAME_MAX_CRC_SIZE - NET_IF_802x_FRAME_CRC_SIZE)

#define  NET_IF_802x_FRAME_MAX_SIZE                     (NET_IF_802x_FRAME_MAX_CRC_SIZE - NET_IF_802x_FRAME_CRC_SIZE)


#define  NET_IF_MTU_ETHER                               (NET_IF_802x_FRAME_MAX_CRC_SIZE - NET_IF_802x_FRAME_CRC_SIZE - NET_IF_HDR_SIZE_ETHER)
#define  NET_IF_MTU_IEEE_802                            (NET_IF_802x_FRAME_MAX_CRC_SIZE - NET_IF_802x_FRAME_CRC_SIZE - NET_IF_HDR_SIZE_IEEE_802)

#define  NET_IF_IEEE_802_SNAP_CODE_SIZE                  3u     /*  3-octet SNAP org code         (see Note #1).        */

#define  NET_IF_802x_BUF_SIZE_MIN                       (NET_IF_ETHER_FRAME_MIN_SIZE    + NET_BUF_DATA_SIZE_MIN      - NET_BUF_DATA_PROTOCOL_HDR_SIZE_MIN)


#define  NET_IF_802x_BUF_RX_LEN_MIN                      NET_IF_ETHER_FRAME_MAX_SIZE
#define  NET_IF_802x_BUF_TX_LEN_MIN                      NET_IF_ETHER_FRAME_MIN_SIZE


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                             NETWORK INTERFACE HEADER / FRAME DATA TYPES
*********************************************************************************************************
*/

                                                                /* ----------------- NET IF 802x HDR ------------------ */
typedef  struct  net_if_hdr_802x {
    CPU_INT08U  AddrDest[NET_IF_802x_ADDR_SIZE];                /*       802x dest  addr.                               */
    CPU_INT08U  AddrSrc[NET_IF_802x_ADDR_SIZE];                 /*       802x src   addr.                               */
    CPU_INT16U  FrameType_Len;                                  /* Demux 802x frame type vs. IEEE 802.3 frame len.      */
} NET_IF_HDR_802x;



/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/


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

                                                                                        /* -------- INIT FNCTS -------- */
void         NetIF_802x_Init                     (NET_ERR                *p_err);

                                                                                        /* --------- RX FNCTS --------- */
void         NetIF_802x_Rx                       (NET_IF                 *p_if,
                                                  NET_BUF                *p_buf,
                                                  NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                                                  NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                  NET_ERR                *p_err);

                                                                                        /* --------- TX FNCTS --------- */
void         NetIF_802x_Tx                       (NET_IF                 *p_if,
                                                  NET_BUF                *p_buf,
                                                  NET_CTR_IF_802x_STATS  *p_ctrs_stat,
                                                  NET_CTR_IF_802x_ERRS   *p_ctrs_err,
                                                  NET_ERR                *p_err);

                                                                                        /* -------- MGMT FNCTS -------- */
void         NetIF_802x_AddrHW_Get               (NET_IF                 *p_if,
                                                  CPU_INT08U             *p_addr_hw,
                                                  CPU_INT08U             *p_addr_len,
                                                  NET_ERR                *p_err);

void         NetIF_802x_AddrHW_Set               (NET_IF                 *p_if,
                                                  CPU_INT08U             *p_addr_hw,
                                                  CPU_INT08U              addr_len,
                                                  NET_ERR                *p_err);

CPU_BOOLEAN  NetIF_802x_AddrHW_IsValid           (NET_IF                 *p_if,
                                                  CPU_INT08U             *p_addr_hw);

void         NetIF_802x_AddrMulticastAdd         (NET_IF                 *p_if,
                                                  CPU_INT08U             *p_addr_protocol,
                                                  CPU_INT08U              addr_protocol_len,
                                                  NET_PROTOCOL_TYPE       addr_protocol_type,
                                                  NET_ERR                *p_err);

void         NetIF_802x_AddrMulticastRemove      (NET_IF                 *p_if,
                                                  CPU_INT08U             *p_addr_protocol,
                                                  CPU_INT08U              addr_protocol_len,
                                                  NET_PROTOCOL_TYPE       addr_protocol_type,
                                                  NET_ERR                *p_err);

void         NetIF_802x_AddrMulticastProtocolToHW(NET_IF                 *p_if,
                                                  CPU_INT08U             *p_addr_protocol,
                                                  CPU_INT08U              addr_protocol_len,
                                                  NET_PROTOCOL_TYPE       addr_protocol_type,
                                                  CPU_INT08U             *p_addr_hw,
                                                  CPU_INT08U             *p_addr_hw_len,
                                                  NET_ERR                *p_err);

void         NetIF_802x_BufPoolCfgValidate       (NET_IF                 *p_if,
                                                  NET_ERR                *p_err);

void         NetIF_802x_MTU_Set                  (NET_IF                 *p_if,
                                                  NET_MTU                 mtu,
                                                  NET_ERR                *p_err);

CPU_INT16U   NetIF_802x_GetPktSizeHdr            (NET_IF                 *p_if);

CPU_INT16U   NetIF_802x_GetPktSizeMin            (NET_IF                 *p_if);

CPU_INT16U   NetIF_802x_GetPktSizeMax            (NET_IF                 *p_if);

CPU_BOOLEAN  NetIF_802x_PktSizeIsValid           (CPU_INT16U              size);


void         NetIF_802x_ISR_Handler              (NET_IF                 *p_if,
                                                  NET_DEV_ISR_TYPE        type,
                                                  NET_ERR                *p_err);


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


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_IF_802x_MODULE_EN        */
#endif  /* NET_IF_802x_MODULE_PRESENT   */

