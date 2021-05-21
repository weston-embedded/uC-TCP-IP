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
*                            Xilinx Local Link Hard TEMAC Driver (Mode DMA)
*                                    Support Both Soft and Hard DMA
*
* Filename : net_dev_xps_ll_temac_dma.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) This driver version does not support the extended Multicast mode.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_DEV_XPS_LL_TEMAC_MODULE_PRESENT
#define  NET_DEV_XPS_LL_TEMAC_MODULE_PRESENT

#include  <IF/net_if_ether.h>
#ifdef  NET_IF_ETHER_MODULE_EN


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

#define  NET_DEV_MCAST_TAB_ERR_RD                      11600u   /* Multicast tbl rd err.                                */
#define  NET_DEV_ERR_RST                               11700u   /* EMAC reset failed.                                   */


/*
*********************************************************************************************************
*                       XPS_LL_TEMAC (ETHERNET) DEVICE BSP INTERFACE DATA TYPE
*
* Note(s) : (1) (a) The XPS_LL_TEMAC device board-support package (BSP) interface data type is a specific
*                   Ethernet device BSP interface data type definition which SHOULD define ALL Ethernet
*                   device BSP functions, synchronized in both the sequential order of the functions &
*                   argument lists for each function.
*
*                   Thus, ANY modification to the sequential order or argument lists of the BSP functions
*                   SHOULD be appropriately synchronized between the Ethernet device BSP interface data
*                   type & the XPS_LL_TEMAC device BSP interface data type/instantiations.
*
*                   However, the XPS_LL_TEMAC device BSP interface data types/instantiations MAY (& do)
*                   include additional BSP functions after all generic Ethernet device BSP functions.
*
*               (b) A specific XPS_LL_TEMAC device BSP interface instantiation MAY define NULL functions
*                   for any (or all) BSP functions provided that the XPS_LL_TEMAC device driver does NOT
*                   require those specific BSP function(s).
*
*               See also 'net_if_ether.h  ETHERNET DEVICE BSP INTERFACE DATA TYPE  Note #1'.
*********************************************************************************************************
*/

                                                                /* --------------- XPS_LL_TEMAC DEV BSP --------------- */
                                                                /* XPS_LL_TEMAC dev BSP fnct ptrs :                     */
typedef  struct  net_dev_bsp_xps_ll_temac_dma {
    void        (*CfgClk)     (NET_IF      *pif,                /* Cfg dev clk(s).                                      */
                               NET_ERR     *perr);

    void        (*CfgIntCtrl) (NET_IF      *pif,                /* Cfg dev int ctrl(s).                                 */
                               NET_ERR     *perr);

    void        (*CfgGPIO)    (NET_IF      *pif,                /* Cfg dev GPIO.                                        */
                               NET_ERR     *perr);


    CPU_INT32U  (*ClkFreqGet) (NET_IF      *pif,                /* Get dev clk freq.                                    */
                               NET_ERR     *perr);


    CPU_ADDR    (*DMA_AddrGet)(void);                           /* Get dev DMA base addr.                               */

    CPU_INT32U  (*DMA_Rd32)   (CPU_INT32U   addr);              /* Rd data from mem addr.                               */

    void        (*DMA_Wr32)   (CPU_INT32U   addr,               /* Wr data to   mem addr.                               */
                               CPU_INT32U   data);

} NET_DEV_BSP_XPS_LL_TEMAC_DMA;


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_DEV_API_ETHER  NetDev_API_XPS_LL_TEMAC_DMA;




/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_ETHER_MODULE_EN              */
#endif /* NET_DEV_XPS_LL_TEMAC_MODULE_PRESENT */

