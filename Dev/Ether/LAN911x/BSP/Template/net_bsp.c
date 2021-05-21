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
*                            NETWORK BOARD SUPPORT PACKAGE (BSP) FUNCTIONS
*
*                                               LAN911x
*
* Filename : net_bsp.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) To provide the required Board Support Package functionality, insert the appropriate
*                board-specific code to perform the stated actions wherever '$$$$' comments are found.
*
*                #### This note MAY be entirely removed for specific board support packages.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_BSP_MODULE
#include  <Source/net.h>
#include  <Source/net_cfg_net.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  NETWORK DEVICE INTERFACE NUMBERS
*
* Note(s) : (1) (a) Each network device maps to a unique network interface number.
*
*               (b) Instances of network devices' interface number SHOULD be named using the following
*                   convention :
*
*                       <Board><Device>[Number]_IF_Nbr
*
*                           where
*                                   <Board>         Development board name
*                                   <Device>        Network device name (or type)
*                                   [Number]        Network device number for each specific instance
*                                                       of device (optional if the development board
*                                                       does NOT support multiple instances of the
*                                                       specific device)
*
*                   For example, the network device interface number variable for the #2 MACB Ethernet
*                   controller on an Atmel AT91SAM92xx should be named 'AT91SAM92xx_MACB_2_IF_Nbr'.
*
*               (c) Network device interface number variables SHOULD be initialized to 'NET_IF_NBR_NONE'.
*********************************************************************************************************
*/
#if 0
static  NET_IF_NBR  LAN911x_Nbr_IF_Nbr = NET_IF_NBR_NONE;
#endif


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                   NETWORK DEVICE DRIVER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           NetDev_CfgClk()
*
* Description : Configure clocks for the specified interface/device.
*
* Argument(s) : if_nbr      Device's interface number to configure.
*               ------      Argument validated in NetDev_Init().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Clocks successfully configured.
*                               NET_DEV_ERR_INVALID_IF          Invalid interface number.
*
* Return(s)   : none
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) Interface numbers for network devices are developer-specified & MUST be added at
*                   run-time in the order specified by the devices' board-specific (BSP) functions.
*
*                   (a) Network interface #0 is reserved for the network loopback interface.
*
*                   (b) Networkinterface numbers for network devices added at run-time start with
*                       network interface #1.
*********************************************************************************************************
*/

void  NetDev_CfgClk (NET_IF_NBR   if_nbr,
                     NET_ERR     *perr)
{
    switch (if_nbr) {
        case 1:                                                 /* Configure IF #1 clks.                                */
             break;


        case 2:                                                 /* Configure IF #2 clks.                                */
             break;


        case 0:                                                 /* See Note #1a.                                        */
        default:
            *perr = NET_DEV_ERR_INVALID_IF;
             return;                                            /* Prevent 'break NOT reachable' compiler warning.      */
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetDev_CfgIntCtrl()
*
* Description : Configure interrupt controller for the specified interface/device.
*
* Argument(s) : if_nbr      Device's interface number to configure.
*               ------      Argument validated in NetDev_Init().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Interrupt(s) successfully configured.
*                               NET_DEV_ERR_INVALID_IF          Invalid interface number.
*
* Return(s)   : none
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) Interface numbers for network devices are developer-specified & MUST be added at
*                   run-time in the order specified by the devices' board-specific (BSP) functions.
*
*                   (a) Network interface #0 is reserved for the network loopback interface.
*
*                   (b) Networkinterface numbers for network devices added at run-time start with
*                       network interface #1.
*********************************************************************************************************
*/

void  NetDev_CfgIntCtrl (NET_IF_NBR   if_nbr,
                         NET_ERR     *perr)
{
    switch (if_nbr) {
        case 1:                                                 /* Configure IF #1 int ctrls.                           */
             break;


        case 2:                                                 /* Configure IF #2 int ctrls.                           */
             break;


        case 0:                                                 /* See Note #1a.                                        */
        default:
            *perr = NET_DEV_ERR_INVALID_IF;
             return;                                            /* Prevent 'break NOT reachable' compiler warning.      */
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_CfgGPIO()
*
* Description : Configure general-purpose I/O (GPIO) for the specified interface/device.
*
* Argument(s) : if_nbr      Device's interface number to configure.
*               ------      Argument validated in NetDev_Init().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                GPIO successfully configured.
*                               NET_DEV_ERR_INVALID_IF          Invalid interface number.
*
* Return(s)   : none
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) Interface numbers for network devices are developer-specified & MUST be added at
*                   run-time in the order specified by the devices' board-specific (BSP) functions.
*
*                   (a) Network interface #0 is reserved for the network loopback interface.
*
*                   (b) Networkinterface numbers for network devices added at run-time start with
*                       network interface #1.
*********************************************************************************************************
*/

void  NetDev_CfgGPIO (NET_IF_NBR   if_nbr,
                      NET_ERR     *perr)
{
    switch (if_nbr) {
        case 1:                                                 /* Configure IF #1 GPIO.                                */
             break;


        case 2:                                                 /* Configure IF #2 GPIO.                                */
             break;


        case 0:                                                 /* See Note #1a.                                        */
        default:
            *perr = NET_DEV_ERR_INVALID_IF;
             return;                                            /* Prevent 'break NOT reachable' compiler warning.      */
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_MDC_ClkFreqGet()
*
* Description : Get MDC clock frequency.
*
* Argument(s) : if_nbr      Device's interface number to get clock frequency.
*               ------      Argument validated in NetDev_Init().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                MDC clock frequency successfully returned.
*                               NET_DEV_ERR_INVALID_IF          Invalid interface number.
*
* Return(s)   : MDC clock frequency (in Hz).
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) Interface numbers for network devices are developer-specified & MUST be added at
*                   run-time in the order specified by the devices' board-specific (BSP) functions.
*
*                   (a) Network interface #0 is reserved for the network loopback interface.
*
*                   (b) Networkinterface numbers for network devices added at run-time start with
*                       network interface #1.
*********************************************************************************************************
*/

CPU_INT32U  NetDev_MDC_ClkFreqGet (NET_IF_NBR   if_nbr,
                                   NET_ERR     *perr)
{
    CPU_INT32U  clk_freq = 0u;


    switch (if_nbr) {
        case 1:                                                 /* Get IF #1 MDC clk freq.                              */
             break;


        case 2:                                                 /* Get IF #2 MDC clk freq.                              */
             break;


        case 0:                                                 /* See Note #1a.                                        */
        default:
            *perr =  NET_DEV_ERR_INVALID_IF;
             return (0);                                        /* Prevent 'break NOT reachable' compiler warning.      */
    }

   *perr =  NET_DEV_ERR_NONE;

    return (clk_freq);
}


/*
*********************************************************************************************************
*                                        NetDev_ISR_Handler()
*
* Description : BSP-level ISR handler(s) for device interrupts.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : CPU &/or device interrupts.
*
* Note(s)     : (1) (a) Each device interrupt, or set of device interrupts, MUST be handled by a
*                       unique BSP-level ISR handler which maps each specific device interrupt to
*                       its corresponding network interface ISR handler.
*
*                   (b) BSP-level device ISR handlers SHOULD be named using the following convention :
*
*                           NetDev_[Device]ISR_Handler[Type][Number]()
*
*                               where
*                                   (1) [Device]        Network device name or type (optional if the
*                                                           development board does NOT support multiple
*                                                           devices)
*                                   (2) [Type]          Network device interrupt type (optional if
*                                                           interrupt type is generic or unknown)
*                                   (3) [Number]        Network device number for each specific instance
*                                                           of device (optional if the development board
*                                                           does NOT support multiple instances of the
*                                                           specific device)
*
*                       See also 'NETWORK DEVICE BSP FUNCTION PROTOTYPES  Note #2a2'.
*********************************************************************************************************
*/
#if 0
static  void  NetDev_ISR_Handler (CPU_INT32U  source)
{
    NET_IF_NBR        if_nbr;
    NET_DEV_ISR_TYPE  type;
    NET_ERR           err;


    /* $$$$ Insert code to handle each network interface's/device's ISR(s) [see Note #2].   */

    if_nbr = LAN911x_Nbr_IF_Nbr;                                /* See Note #2b3.                                       */
    type   = NET_DEV_ISR_TYPE_UNKNOWN;                          /* See Note #2b2.                                       */

    NetIF_ISR_Handler(if_nbr, type, &err);
}
#endif


#endif /* NET_IF_ETHER_MODULE_EN */
