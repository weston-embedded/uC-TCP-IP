/*
*********************************************************************************************************
*                                              uC/TCP-IP
*                                      The Embedded TCP/IP Suite
*
*                    Copyright 2004-2020 Silicon Laboratories Inc. www.silabs.com
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
*                                              TEMPLATE
*
* Filename : bsp_net_eth.c
* Version  : V3.06.00
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <IF/net_if_ether.h>


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
*                                  ETHERNET DEVICE INTERFACE NUMBERS
*
* Note(s) : (1) Each network device maps to a unique network interface number.
*********************************************************************************************************
*/

static  NET_IF_NBR  Board_IF_Nbr = NET_IF_NBR_NONE;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#ifdef  NET_IF_ETHER_MODULE_EN

static  void        BSP_NET_Template_CfgClk    (NET_IF   *p_if,
                                                NET_ERR  *p_err);

static  void        BSP_NET_Template_CfgIntCtrl(NET_IF   *p_if,
                                                NET_ERR  *p_err);

static  void        BSP_NET_Template_CfgGPIO   (NET_IF   *p_if,
                                                NET_ERR  *p_err);

static  CPU_INT32U  BSP_NET_Template_ClkFreqGet(NET_IF   *p_if,
                                                NET_ERR  *p_err);

        void        BSP_NET_Template_IntHandler(void);


/*
*********************************************************************************************************
*                                    ETHERNET DEVICE BSP INTERFACE
*********************************************************************************************************
*/

const  NET_DEV_BSP_ETHER  NET_DrvBSP_Template = {
                                                 &BSP_NET_Template_CfgClk,
                                                 &BSP_NET_Template_CfgIntCtrl,
                                                 &BSP_NET_Template_CfgGPIO,
                                                 &BSP_NET_Template_ClkFreqGet
                                                };


/*
*********************************************************************************************************
*                                     LOCAL CONFIGURATION ERRORS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      BSP_NET_Template_CfgClk()
*
* Description : Configure clocks for the specified interface/device.
*
* Argument(s) : p_if     Pointer to network interface to configure.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device clock(s) successfully configured.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_NET_Template_CfgClk (NET_IF   *p_if,
                                       NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to configure each network interface's/device's clocks. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    BSP_NET_Template_CfgIntCtrl()
*
* Description : Configure interrupts and/or interrupt controller for the specified interface/device.
*
* Argument(s) : p_if     Pointer to network interface to configure.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device interrupt(s) successfully configured.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_NET_Template_CfgIntCtrl (NET_IF   *p_if,
                                           NET_ERR  *p_err)
{
    Board_IF_Nbr = p_if->Nbr;                                   /* Configure BSP instance with specific interface nbr.  */

    /* $$$$ Insert code to configure each network interface's/device's interrupt(s)/controller. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     BSP_NET_Template_CfgGPIO()
*
* Description : Configure general-purpose I/O (GPIO) for the specified interface/device.
*
* Argument(s) : p_if     Pointer to network interface to configure.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device GPIO successfully configured.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_NET_Template_CfgGPIO (NET_IF   *p_if,
                                        NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to configure each network interface's/device's GPIO. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    BSP_NET_Template_ClkFreqGet()
*
* Description : Get device clock frequency.
*
* Argument(s) : p_if     Pointer to network interface to get clock frequency.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device clock frequency successfully returned.
*
* Return(s)   : Device clock frequency (in Hz).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  CPU_INT32U  BSP_NET_Template_ClkFreqGet (NET_IF   *p_if,
                                                 NET_ERR  *p_err)
{
    CPU_INT32U  clk_freq;


    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    (void)clk_freq;

    /* $$$$ Insert code to return each network interface's/device's clock frequency. */

    *p_err = NET_DEV_ERR_NONE;

    return (clk_freq);
}


/*
*********************************************************************************************************
*                                    BSP_NET_Template_IntHandler()
*
* Description : BSP-level ISR handler(s) for device interrupts.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  BSP_NET_Template_IntHandler (void)
{
    NET_ERR  err;


    /* $$$$ Insert code to handle each network interface's/device's interrupt(s) [see Note #1a] :   */

    NetIF_ISR_Handler(Board_IF_Nbr, NET_DEV_ISR_TYPE_UNKNOWN, &err);

    (void)err;

    /* $$$$ Insert code to clear  each network interface's/device's interrupt(s), if necessary.     */
}


/*
*********************************************************************************************************
*                                              MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_ETHER_MODULE_EN */
