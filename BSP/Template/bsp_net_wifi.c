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
* Filename : bsp_net_wifi.c
* Version  : V3.06.00
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <IF/net_if_wifi.h>


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
*                                    WIFI DEVICE INTERFACE NUMBERS
*
* Note(s) : (1) Each network device maps to a unique network interface number.
*********************************************************************************************************
*/

static  NET_IF_NBR  WiFi_IF_Nbr = NET_IF_NBR_NONE;


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

#ifdef  NET_IF_WIFI_MODULE_EN

static  void  BSP_NET_WiFi_Template_Start         (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_Stop          (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_CfgGPIO       (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_CfgIntCtrl    (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_IntCtrl       (NET_IF                          *p_if,
                                                   CPU_BOOLEAN                      en,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_SPI_Init      (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_SPI_Lock      (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_SPI_Unlock    (NET_IF                          *p_if);

static  void  BSP_NET_WiFi_Template_SPI_WrRd      (NET_IF                          *p_if,
                                                   CPU_INT08U                      *p_buf_wr,
                                                   CPU_INT08U                      *p_buf_rd,
                                                   CPU_INT16U                       len,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_SPI_ChipSelEn (NET_IF                          *p_if,
                                                   NET_ERR                         *p_err);

static  void  BSP_NET_WiFi_Template_SPI_ChipSelDis(NET_IF                          *p_if);

static  void  BSP_NET_WiFi_Template_SPI_Cfg       (NET_IF                          *p_if,
                                                   NET_DEV_CFG_SPI_CLK_FREQ         freq,
                                                   NET_DEV_CFG_SPI_CLK_POL          pol,
                                                   NET_DEV_CFG_SPI_CLK_PHASE        phase,
                                                   NET_DEV_CFG_SPI_XFER_UNIT_LEN    xfer_unit_len,
                                                   NET_DEV_CFG_SPI_XFER_SHIFT_DIR   xfer_shift_dir,
                                                   NET_ERR                         *p_err);

        void  BSP_NET_WiFi_Template_IntHandler    (void);


/*
*********************************************************************************************************
*                                      WIFI DEVICE BSP INTERFACE
*********************************************************************************************************
*/

const  NET_DEV_BSP_WIFI_SPI  NET_WiFI_BSP_Template = {
                                                      &BSP_NET_WiFi_Template_Start,
                                                      &BSP_NET_WiFi_Template_Stop,
                                                      &BSP_NET_WiFi_Template_CfgGPIO,
                                                      &BSP_NET_WiFi_Template_CfgIntCtrl,
                                                      &BSP_NET_WiFi_Template_IntCtrl,
                                                      &BSP_NET_WiFi_Template_SPI_Init,
                                                      &BSP_NET_WiFi_Template_SPI_Lock,
                                                      &BSP_NET_WiFi_Template_SPI_Unlock,
                                                      &BSP_NET_WiFi_Template_SPI_WrRd,
                                                      &BSP_NET_WiFi_Template_SPI_ChipSelEn,
                                                      &BSP_NET_WiFi_Template_SPI_ChipSelDis,
                                                      &BSP_NET_WiFi_Template_SPI_Cfg,
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
*******************************************************************************************************
*                                    BSP_NET_WiFi_Template_Start()
*
* Description : Start (power up) interface's/device's.
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device successfully started.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_Start (NET_IF   *p_if,
                                           NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to start (power up) wireless spi device. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    BSP_NET_WiFi_Template_Stop()
*
* Description : Stop (power down) interface's/device's.
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device successfully stopped.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/
static  void  BSP_NET_WiFi_Template_Stop (NET_IF   *p_if,
                                          NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to stop (power down) wireless spi device. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   BSP_NET_WiFi_Template_CfgGPIO()
*
* Description : Configure general-purpose I/O (GPIO) for the specified interface/device. (SPI, External
*               Interrupt, Power, Reset, etc.)
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
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
static  void  BSP_NET_WiFi_Template_CfgGPIO (NET_IF   *p_if,
                                             NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to configure each network interface's/device's GPIO. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                 BSP_NET_WiFi_Template_CfgIntCtrl()
*
* Description : Configure interrupts &/or interrupt controller for the specified interface/device.
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
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

static  void  BSP_NET_WiFi_Template_CfgIntCtrl (NET_IF   *p_if,
                                                NET_ERR  *p_err)
{
    WiFi_SPI_IF_Nbr = p_if->Nbr;

    /* $$$$ Insert code to configure each network interface's/device's interrupt(s)/controller. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                   BSP_NET_WiFi_Template_IntCtrl()
*
* Description : Enable or disable interface's/device's interrupt.
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
*
*               en       Enable or disable the interrupt.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device interrupt(s) successfully enabled or disabled.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_IntCtrl (NET_IF       *p_if,
                                             CPU_BOOLEAN   en,
                                             NET_ERR      *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    if (en == DEF_YES) {
        /* $$$$ Insert code to enable the interface's/device's interrupt(s).  */
    } else if (en == DEF_NO) {
        /* $$$$ Insert code to disable the interface's/device's interrupt(s). */
    }

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                  BSP_NET_WiFi_Template_SPI_Init()
*
* Description : Initialize SPI controller.
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    SPI controller successfully initialized.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function is called only when the wireless network interface is added.
*
*               (2) CS (Chip Select) must be configured as a GPIO output; it cannot be controlled by the
*                   CPU's SPI peripheral. The functions 'BSP_NET_WiFi_Template_SPI_ChipSelEn()' and
*                  'BSP_NET_WiFi_Template_SPI_ChipSelDis()' should manually enable and disable the CS.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_SPI_Init (NET_IF   *p_if,
                                              NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to initialize SPI interface's/device's controller & lock. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                  BSP_NET_WiFi_Template_SPI_Lock()
*
* Description : Acquire SPI lock.
*
* Argument(s) : p_if     Pointer to interface to start the hardware.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    SPI lock successfully acquired.
*
* Return(s)   : none.
*
* Note(s)     : (1) This function will be called before the device driver begins to access the SPI. The
*                   application should not use the same bus to access another device until the matching
*                   call to 'BSP_NET_WiFi_Template_SPI_Unlock()' has been made.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_SPI_Lock (NET_IF   *p_if,
                                              NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to lock the SPI controller. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                 BSP_NET_WiFi_Template_SPI_Unlock()
*
* Description : Release SPI lock.
*
* Argument(s) : p_if    Pointer to interface to start the hardware.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'BSP_NET_WiFi_Template_SPI_Lock()' will be called before the device driver begins to
*                   access the SPI. The application should not use the same bus to access another device
*                   until the matching call to this function has been made.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_SPI_Unlock (NET_IF  *p_if)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to unlock the SPI controller. */
}


/*
*********************************************************************************************************
*                                  BSP_NET_WiFi_Template_SPI_WrRd()
*
* Description : Write and read to SPI bus.
*
* Argument(s) : p_if         Pointer to interface to write and read from.
*
*               p_buf_wr     Pointer to buffer to write.
*
*               p_buf_rd     Pointer to buffer for data read.
*
*               wr_rd_len    Number of octets to write and read.
*
*               p_err        Pointer to variable that will receive the return error code from this function:
*
*                                NET_DEV_ERR_NONE    Network buffers successfully written and/or read.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_SPI_WrRd (NET_IF      *p_if,
                                              CPU_INT08U  *p_buf_wr,
                                              CPU_INT08U  *p_buf_rd,
                                              CPU_INT16U   wr_rd_len,
                                              NET_ERR     *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    (void)p_buf_wr;
    (void)p_buf_rd;
    (void)wr_rd_len;

    /* $$$$ Insert code to write and read on the SPI controller. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                BSP_NET_WiFi_Template_SPI_ChipSelEn()
*
* Description : Enable device chip select.
*
* Argument(s) : p_if     Pointer to interface to enable the chip select.
*
*               p_err    Pointer to variable that will receive the return error code from this function:
*
*                            NET_DEV_ERR_NONE    Device chip select successfully enabled.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'BSP_NET_WiFi_Template_SPI_ChipSelEn()' will be called before the device driver
*                   begins to access the SPI.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_SPI_ChipSelEn (NET_IF   *p_if,
                                                   NET_ERR  *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to enable the device chip select. */
    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                               BSP_NET_WiFi_Template_SPI_ChipSelDis()
*
* Description : Disable device chip select.
*
* Argument(s) : p_if    Pointer to interface to disable the chip select.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'BSP_NET_WiFi_Template_SPI_ChipSelDis()' will be called when the device driver
*                   finished to access the SPI.
*********************************************************************************************************
*/

static  void  BSP_NET_WiFi_Template_SPI_ChipSelDis (NET_IF  *p_if)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */

    /* $$$$ Insert code to disable the device chip select. */
}


/*
*********************************************************************************************************
*                                   BSP_NET_WiFi_Template_SPI_Cfg()
*
* Description : Configure the SPI controller following the device configuration.
*
* Argument(s) : p_if              Pointer to interface to configure the SPI.
*
*               freq              Clock frequency, in Hz.
*
*               pol               Clock polarity:
*                                     NET_DEV_SPI_CLK_POL_INACTIVE_LOW        The clk is low  when inactive.
*                                     NET_DEV_SPI_CLK_POL_INACTIVE_HIGH       The clk is high when inactive.
*
*               phase             Clock Phase:
*                                     NET_DEV_SPI_CLK_PHASE_FALLING_EDGE      Data is 'read'    on the leading   edge &
*                                                                                     'changed' on the following edge
*
*                                     NET_DEV_SPI_CLK_PHASE_RAISING_EDGE      Data is 'changed' on the following edge &
*                                                                                     'read'    on the leading   edge.
*
*               xfer_unit_len     Transfer unit length:
*                                     NET_DEV_SPI_XFER_UNIT_LEN_8_BITS        Unit length is  8 bits.
*                                     NET_DEV_SPI_XFER_UNIT_LEN_16_BITS       Unit length is 16 bits.
*                                     NET_DEV_SPI_XFER_UNIT_LEN_32_BITS       Unit length is 32 bits.
*                                     NET_DEV_SPI_XFER_UNIT_LEN_64_BITS       Unit length is 64 bits.
*
*               xfer_shift_dir    Transfer Shift direction:
*                                     NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB    MSB first.
*                                     NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_LSB    LSB First
*
*               p_err             Pointer to variable that will receive the return error code from this function:
*
*                                     NET_DEV_ERR_NONE                        SPI controller successfully configured.
*
* Return(s)   : none.
*
* Note(s)     : (1) 'BSP_NET_WiFi_Template_SPI_Cfg()' will be called before the device driver begins to access
*                   the SPI and after 'BSP_NET_WiFi_Template_SPI_Lock()' has been called.
*
*               (2) The effective clock frequency must not be more than 'freq'. If the frequency cannot be
*                   configured equal to 'freq', it should be configured to less than 'freq'.
*********************************************************************************************************
*/

void  BSP_NET_WiFi_Template_SPI_Cfg (NET_IF                          *p_if,
                                     NET_DEV_CFG_SPI_CLK_FREQ         freq,
                                     NET_DEV_CFG_SPI_CLK_POL          pol,
                                     NET_DEV_CFG_SPI_CLK_PHASE        phase,
                                     NET_DEV_CFG_SPI_XFER_UNIT_LEN    xfer_unit_len,
                                     NET_DEV_CFG_SPI_XFER_SHIFT_DIR   xfer_shift_dir,
                                     NET_ERR                         *p_err)
{
    (void)p_if;                                                 /* Prevent 'variable unused' compiler warning.          */
    (void)freq;
    (void)pol;
    (void)phase;
    (void)xfer_unit_len;
    (void)xfer_shift_dir;

    /* $$$$ Insert code to configure correctly the SPI Bus. */

    *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                 BSP_NET_WiFi_Template_IntHandler()
*
* Description : BSP-level ISR handler(s) for WiFi device interrupt.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  BSP_NET_WiFi_Template_IntHandler (void)
{
    NET_ERR  err;


    NetIF_ISR_Handler(WiFi_SPI_IF_Nbr, NET_DEV_ISR_TYPE_UNKNOWN, &err);

    /* $$$$ Insert code to clear WiFi device interrupt(s), if necessary. */
}


/*
*********************************************************************************************************
*                                              MODULE END
*********************************************************************************************************
*/

#endif /* NET_IF_WIFI_MODULE_EN */
