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
*                                   NETWORK PHYSICAL LAYER DRIVER
*
*                                             TI DP83867IR
*
* Filename : net_phy_dp83867ir.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.00 (or more recent version) is included in the project build.
*
*            (2) The (R)MII interface port is assumed to be part of the host EMAC.  Therefore, (R)MII
*                reads/writes MUST be performed through the network device API interface via calls to
*                function pointers 'Phy_RegRd()' & 'Phy_RegWr()'.
*
*            (3) Interrupt support is Phy specific, therefore the generic Phy driver does NOT support
*                interrupts.  However, interrupt support is easily added to the generic Phy driver &
*                thus the ISR handler has been prototyped and & populated within the function pointer
*                structure for example purposes.
*********************************************************************************************************
*/

#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>


/*
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This network physical layer header file is protected from multiple pre-processor inclusion
*               through use of the network physical layer module present pre-processor macro definition.
*********************************************************************************************************
*/

#ifndef  NET_PHY_MODULE_PRESENT                                 /* See Note #1.                                         */
#define  NET_PHY_MODULE_PRESENT
#ifdef   NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                 NETWORK PHYSICAL LAYER ERROR CODES
*
* Note(s) : (1) ALL physical layer-independent error codes #define'd in      'net_err.h';
*               ALL physical layer-specific    error codes #define'd in this 'net_phy_&&&.h'.
*
*           (2) Network error code '12,000' series reserved for network physical layer drivers.
*               See 'net_err.h  NETWORK PHYSICAL LAYER ERROR CODES' to ensure that physical layer-
*               specific error codes do NOT conflict with physical layer-independent error codes.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_PHY_API_ETHER  NetPhy_API_DP83867IR;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void  NetPhy_DP83867IR_DelayCfg(NET_IF_NBR    if_nbr,
                                CPU_BOOLEAN   tx_en,
                                CPU_INT08U    tx_dly,
                                CPU_BOOLEAN   rx_en,
                                CPU_INT08U    rx_dly,
                                NET_ERR      *perr);


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_ETHER_MODULE_EN */
#endif  /* NET_PHY_MODULE_PRESENT */

