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
*                                    NETWORK PHYSICAL LAYER DRIVER
*
*                              10/100/1000 Gigabit Ethernet Transceiver
*                                         Micrel KSZ9021RN/RL
*                                          Micrel KSZ9031RNX
*
* Filename : net_phy_ksz9021r.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.06 (or more recent version) is included in the project build.
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

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_PHY_KSZ9021R_MODULE_PRESENT
#define  NET_PHY_KSZ9021R_MODULE_PRESENT


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

extern  const  NET_PHY_API_ETHER  NetPhy_API_ksz9021r;
extern  const  NET_PHY_API_ETHER  NetPhy_API_ksz9031rnx;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                             MODULE END
*
* Note(s) : (1) See 'net_phy.h  MODULE'.
*********************************************************************************************************
*/

#endif                                                          /* End of generic phy module include (see Note #1).     */

