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
* Filename : net_dev_usbd_cdceem.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This driver requires the uC/USB-Device stack with the CDC-EEM class.
*
*            (2) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) uC/TCP-IP V3.00.00
*
*                (b) uC/USB-Device V4.03.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_USBD_CDCEEM_MODULE
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>

#include  <Class/CDC-EEM/usbd_cdc_eem.h>
#include  <Source/usbd_core.h>
#include  "net_dev_usbd_cdceem.h"
#include  "lib_math.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                               MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Device driver functions may be arbitrarily named.  However, it is recommended that device
*               driver functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_&&&.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*********************************************************************************************************
*/

                                                                /* ------------ FNCT'S COMMON TO ALL DRV'S ------------ */
static  void  NetDev_Init               (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_Start              (NET_IF             *pif,
                                         NET_ERR            *perr);


static  void  NetDev_Stop               (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_Rx                 (NET_IF             *pif,
                                         CPU_INT08U        **p_data,
                                         CPU_INT16U         *size,
                                         NET_ERR            *perr);

static  void  NetDev_Tx                 (NET_IF             *pif,
                                         CPU_INT08U         *p_data,
                                         CPU_INT16U          size,
                                         NET_ERR            *perr);

static  void  NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *perr);

static  void  NetDev_AddrMulticastRemove(NET_IF             *pif,
                                         CPU_INT08U         *paddr_hw,
                                         CPU_INT08U          addr_hw_len,
                                         NET_ERR            *perr);

static  void  NetDev_IO_Ctrl            (NET_IF             *pif,
                                         CPU_INT08U          opt,
                                         void               *p_data,
                                         NET_ERR            *perr);


/*
*********************************************************************************************************
*                                      USBD CDC-EEM DRIVER FNCTS
*********************************************************************************************************
*/

static  CPU_INT08U  *NetDev_USBD_CDCEEM_RxBufGet (CPU_INT08U   class_nbr,
                                                  void        *p_arg,
                                                  CPU_INT16U  *p_buf_len);

static  void         NetDev_USBD_CDCEEM_RxBufRdy (CPU_INT08U   class_nbr,
                                                  void        *p_arg);

static  void         NetDev_USBD_CDCEEM_TxBufFree(CPU_INT08U   class_nbr,
                                                  void        *p_arg,
                                                  CPU_INT08U  *p_buf,
                                                  CPU_INT16U   buf_len);


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                      NETWORK DEVICE DRIVER API
*
* Note(s) : (1) Device driver API structures are used by applications during calls to NetIF_Add().  This
*               API structure allows higher layers to call specific device driver functions via function
*               pointer instead of by name.  This enables the network protocol suite to compile & operate
*               with multiple device drivers.
*
*           (2) In most cases, the API structure provided below SHOULD suffice for most device drivers
*               exactly as is with the exception that the API structure's name which MUST be unique &
*               SHOULD clearly identify the device being implemented.  For example, the Cirrus Logic
*               CS8900A Ethernet controller's API structure should be named NetDev_API_CS8900A[].
*
*               The API structure MUST also be externally declared in the device driver header file
*               ('net_dev_&&&.h') with the exact same name & type.
*********************************************************************************************************
*/

const  NET_DEV_API_ETHER  NetDev_API_USBD_CDCEEM = {
    NetDev_Init,                                                /* Init/add                                             */
    NetDev_Start,                                               /* Start                                                */
    NetDev_Stop,                                                /* Stop                                                 */
    NetDev_Rx,                                                  /* Rx                                                   */
    NetDev_Tx,                                                  /* Tx                                                   */
    NetDev_AddrMulticastAdd,                                    /* Multicast addr add                                   */
    NetDev_AddrMulticastRemove,                                 /* Multicast addr remove                                */
    DEF_NULL,                                                   /* ISR handler                                          */
    NetDev_IO_Ctrl,                                             /* I/O ctrl                                             */
    DEF_NULL,                                                   /* Phy reg rd                                           */
    DEF_NULL                                                    /* Phy reg wr                                           */
};


/*
*********************************************************************************************************
*                                         USBD CDC EEM DRIVER
*********************************************************************************************************
*/

USBD_CDC_EEM_DRV  NetDev_DrvUSBD_CDC_EEM = {
    NetDev_USBD_CDCEEM_RxBufGet,
    NetDev_USBD_CDCEEM_RxBufRdy,
    NetDev_USBD_CDCEEM_TxBufFree
};


/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : Initializes driver.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
*                   NET_DEV_ERR_NONE            Operation successful.
*                   NET_DEV_ERR_INVALID_CFG     Invalid device configuration.
*                   NET_DEV_ERR_INIT            CDC-EEM instance initialisation failed.
*
* Return(s)   : None.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    LIB_ERR                    err_lib;
    NET_DEV_CFG_USBD_CDC_EEM  *p_cfg;
    USBD_CDC_EEM_CFG          *p_cdc_eem_cfg;
    USBD_ERR                   err_usbd;


    p_cfg = (NET_DEV_CFG_USBD_CDC_EEM *)pif->Dev_Cfg;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
                                                                /* Ensure we have valid dev cfg for CDC-EEM.            */
    if (( p_cfg->RxBufLargeSize                                <  NET_IF_802x_FRAME_MAX_CRC_SIZE) ||
        ( p_cfg->TxBufLargeSize                                <  NET_IF_802x_FRAME_MAX_CRC_SIZE) ||
        ((p_cfg->TxBufAlignOctets % USBD_CFG_BUF_ALIGN_OCTETS) != 0u                            ) ||
        ( p_cfg->TxBufIxOffset                                 != USBD_CDC_EEM_HDR_LEN          )) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
#endif

    p_cdc_eem_cfg = (USBD_CDC_EEM_CFG *)Mem_SegAlloc("NET - USBD CDCEEM cfg",
                                                      DEF_NULL,
                                                      sizeof(USBD_CDC_EEM_CFG),
                                                     &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    p_cdc_eem_cfg->RxBufQSize = p_cfg->RxBufLargeNbr;
    p_cdc_eem_cfg->TxBufQSize = p_cfg->TxBufLargeNbr + p_cfg->TxBufSmallNbr;

    USBD_CDC_EEM_InstanceInit(        p_cfg->ClassNbr,          /* Configure class instance according to net drv needs. */
                                      p_cdc_eem_cfg,
                                     &NetDev_DrvUSBD_CDC_EEM,
                              (void *)pif,
                                     &err_usbd);
   *perr = (err_usbd == USBD_ERR_NONE) ? NET_DEV_ERR_NONE : NET_DEV_ERR_INIT;
}


/*
*********************************************************************************************************
*                                            NetDev_Start()
*
* Description : (1) Start network interface hardware :
*
*                   (a) Initialize transmit semaphore count
*                   (b) Initialize hardware address registers
*                   (c) Initialize receive and transmit descriptors
*                   (d) Clear all pending interrupt sources
*                   (e) Enable supported interrupts
*                   (f) Enable the transmitter and receiver
*                   (g) Start / Enable DMA if required
*
*
* Argument(s) : pif         Pointer to a network interface.
*               ---         Argument validated in NetIF_Start().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE        Operation successful.
*                               NET_DEV_ERR_FAULT       CDC-EEM class instance failed to start.
*
*                                                       - RETURNED BY NetIF_DevCfgTxRdySignal -
*                                                       See NetIF_DevCfgTxRdySignal() for additional
*                                                       return error codes.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    CPU_INT08U                 hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT16U                 tx_buf_qty;
    USBD_ERR                   err_usbd;
    NET_DEV_CFG_USBD_CDC_EEM  *p_cfg = (NET_DEV_CFG_USBD_CDC_EEM *)pif->Dev_Cfg;


    USBD_CDC_EEM_Start(p_cfg->ClassNbr,
                      &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
       *perr = NET_DEV_ERR_FAULT;
        return;
    }

    NetASCII_Str_to_MAC(p_cfg->HW_AddrStr,
                        hw_addr,
                        perr);
    if (*perr != NET_ASCII_ERR_NONE) {
        return;
    }

    NetIF_AddrHW_SetHandler(pif->Nbr,                           /* Set MAC address.                                     */
                            hw_addr,
                            NET_IF_ETHER_ADDR_SIZE,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
        return;
    }

                                                                /* CDC EEM class can Q all buffer.                      */
    tx_buf_qty = p_cfg->TxBufLargeNbr + p_cfg->TxBufSmallNbr;

    NetIF_DevCfgTxRdySignal(pif,
                            tx_buf_qty,
                            perr);
   *perr = (*perr == NET_IF_ERR_NONE) ? NET_DEV_ERR_NONE : *perr;
}


/*
*********************************************************************************************************
*                                            NetDev_Stop()
*
* Description : (1) Shutdown network interface hardware :
*
*                   (a) Disable the receiver and transmitter
*                   (b) Disable receive and transmit interrupts
*                   (c) Clear pending interrupt requests
*                   (d) Free ALL receive descriptors (Return ownership to hardware)
*                   (e) Deallocate ALL transmit buffers
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
*                           NET_DEV_ERR_NONE        Operation successful.
*                           NET_DEV_ERR_FAULT       CDC-EEM class instance failed to stop.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Stop() via 'pdev_api->Stop()'.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    CPU_INT08U  class_nbr = ((NET_DEV_CFG_USBD_CDC_EEM *)pif->Dev_Cfg)->ClassNbr;
    USBD_ERR    err_usbd;


    USBD_CDC_EEM_Stop(class_nbr, &err_usbd);
   *perr = (err_usbd == USBD_ERR_NONE) ? NET_DEV_ERR_NONE : NET_DEV_ERR_FAULT;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : Gets rx data packet.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to pointer to received data area. The received data
*                       area address should be returned to the stack by dereferencing
*                       p_data as *p_data = (address of receive data area).
*
*               size    Pointer to size. The number of bytes received should be returned
*                       to the stack by dereferencing size as *size = (number of bytes).
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            Operation successful.
*                           NET_DEV_ERR_RX              Failed to retrieve a Rx buffer from CDC-EEM class.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pdev_api->Rx()'.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    CPU_INT08U  class_nbr = ((NET_DEV_CFG_USBD_CDC_EEM *)pif->Dev_Cfg)->ClassNbr;
    CPU_INT16U  buf_len;
    USBD_ERR    err_usbd;


   *p_data = USBD_CDC_EEM_RxDataPktGet(class_nbr,               /* Retrieve data packet from CDC EEM class.             */
                                      &buf_len,
                                       DEF_NULL,
                                      &err_usbd);
    if (err_usbd != USBD_ERR_NONE) {
       *perr = NET_DEV_ERR_RX;
        return;
    }

                                                                /* --------------------- CLR CRC ---------------------- */
    buf_len -= NET_IF_802x_FRAME_CRC_SIZE;                      /* Remove CRC from buf as net stack doesn't expect it.  */

                                                                /* ------------------- ADD PADDING -------------------- */
   *size = DEF_MAX(buf_len, NET_IF_802x_FRAME_MIN_SIZE);        /* Add padding if necessary.                            */

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    if (*size > buf_len) {
        Mem_Clr(&((*p_data)[buf_len]),                          /* Clr buffer content if padding added.                 */
                   *size - buf_len);
    }
#endif

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : Transmits specified data.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to buffer that contains data to send.
*
*               size    Size of buffer in bytes.
*
*               perr    Pointer to return error code.
*
*                           NET_DEV_ERR_NONE    Operation successful.
*                           NET_DEV_ERR_TX      Failed to submit the buffer to CDC-EEM class.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pdev_api->Tx()'.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    CPU_INT08U  *p_buf     =  p_data - USBD_CDC_EEM_HDR_LEN;
    CPU_INT08U   class_nbr = ((NET_DEV_CFG_USBD_CDC_EEM *)pif->Dev_Cfg)->ClassNbr;
    USBD_ERR     err_usbd;


    USBD_CDC_EEM_TxDataPktSubmit(class_nbr,
                                 p_buf,
                                 size,
                                 DEF_NO,
                                &err_usbd);
   *perr = (err_usbd == USBD_ERR_NONE) ? NET_DEV_ERR_NONE : NET_DEV_ERR_TX;
}


/*
*********************************************************************************************************
*                                       NetDev_AddrMulticastAdd()
*
* Description : Configure hardware address filtering to accept specified hardware address.
*
* Argument(s) : pif         Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               paddr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Operation successful.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : (1) This functionality is not supported by CDC-EEM.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
   (void)&pif;
   (void)&paddr_hw;
   (void)&addr_hw_len;

                                                                /* CDC-EEM unable to perform address filtering.         */
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetDev_AddrMulticastRemove()
*
* Description : Configure hardware address filtering to reject specified hardware address.
*
* Argument(s) : pif         Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               paddr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_len    Length  of hardware address.
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Operation successful.
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastRemove()'.
*
* Note(s)     : (1) This functionality is not supported by CDC-EEM.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
   (void)&pif;
   (void)&paddr_hw;
   (void)&addr_hw_len;

                                                                /* CDC-EEM unable to perform address filtering.         */
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_IO_Ctrl()
*
* Description : This function provides a mechanism for the Phy driver to update the MAC link
*               and duplex settings, as well as a method for the application and link state
*               timer to obtain the current link status.  Additional user specified driver
*               functionality MAY be added if necessary.
*
* Argument(s) : pif     Pointer to interface requiring service.
*
*               opt     Option code representing desired function to perform. The Network Protocol Suite
*                       specifies the option codes below. Additional option codes may be defined by the
*                       driver developer in the driver's header file.
*                           NET_IF_IO_CTRL_LINK_STATE_GET
*                           NET_IF_IO_CTRL_LINK_STATE_UPDATE
*
*                       Driver defined operation codes MUST be defined starting from 20 or higher
*                       to prevent clashing with the pre-defined operation code types. See the
*                       device driver header file for more details.
*
*               p_data  Pointer to optional data for either sending or receiving additional function
*                       arguments or return data.
*
*               perr    Pointer to return error code.
*
*                           NET_DEV_ERR_NONE                    Operation successful.
*                           NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid option number specified.
*                           NET_DEV_ERR_INVALID_CFG             Invalid configuration.
*

*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IO_CtrlHandler() via 'pdev_api->IO_Ctrl()',
*               NetPhy_LinkStateGet()        via 'pdev_api->IO_Ctrl()'.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *pif,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *perr)
{
    CPU_BOOLEAN          is_conn;
    CPU_INT08U           class_nbr = ((NET_DEV_CFG_USBD_CDC_EEM *)pif->Dev_Cfg)->ClassNbr;
    CPU_INT08U           dev_nbr;
    USBD_DEV_SPD         dev_spd;
    USBD_ERR             err_usbd;
    NET_DEV_LINK_ETHER  *plink_state;


    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:                /* Retrieve spd/conn state from device conn status.     */
             plink_state = (NET_DEV_LINK_ETHER *)p_data;

             plink_state->Spd    = NET_PHY_SPD_0;
             plink_state->Duplex = NET_PHY_DUPLEX_UNKNOWN;

             is_conn = USBD_CDC_EEM_IsConn(class_nbr);
             if (is_conn != DEF_YES) {
                *perr = NET_DEV_ERR_NONE;
                 return;
             }

             dev_nbr = USBD_CDC_EEM_DevNbrGet(class_nbr, &err_usbd);
             if (err_usbd != USBD_ERR_NONE) {
                *perr = NET_DEV_ERR_INVALID_CFG;
                 return;
             }

             dev_spd = USBD_DevSpdGet(dev_nbr, &err_usbd);
             if (err_usbd != USBD_ERR_NONE) {
                *perr = NET_DEV_ERR_INVALID_CFG;
                 return;
             }

             plink_state->Duplex = NET_PHY_DUPLEX_AUTO;

             switch (dev_spd) {
                 case USBD_DEV_SPD_HIGH:
                      plink_state->Spd = NET_PHY_SPD_1000;
                      break;


                 case USBD_DEV_SPD_FULL:
                 case USBD_DEV_SPD_LOW:
                      plink_state->Spd = NET_PHY_SPD_10;
                      break;


                 case USBD_DEV_SPD_INVALID:
                 default:
                      break;
             }

            *perr = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;

             switch (plink_state->Duplex) {                     /* Update duplex setting on device.                     */
                case NET_PHY_DUPLEX_FULL:
                case NET_PHY_DUPLEX_HALF:
                     break;


                default:
                    *perr =  NET_PHY_ERR_INVALID_CFG;
                     return;
             }

             switch (plink_state->Spd) {                        /* Update speed setting on device.                      */
                case NET_PHY_SPD_10:
                case NET_PHY_SPD_100:
                     break;


                default:
                    *perr =  NET_PHY_ERR_INVALID_CFG;
                     return;
             }

            *perr = NET_DEV_ERR_NONE;
             break;


        default:
            *perr = NET_IF_ERR_INVALID_IO_CTRL_OPT;
             break;
    }
}


/*
*********************************************************************************************************
*                                    USBD CDC EEM DRIVER FUNCTIONS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                    NetDev_USBD_CDCEEM_RxBufGet()
*
* Description : Gets a free network rx buffer.
*
* Argument(s) : class_nbr   CDC EEM class number.
*
*               p_arg       Pointer to application specific argument.
*
*               p_buf_len   Pointer to variable that will receive the buffer length in bytes.
*
* Return(s)   : Pointer to retrieved rx buffer, if successful.
*               Null pointer,                   otherwise.
*
* Caller(s)   : USBD CDC-EEM class.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  CPU_INT08U  *NetDev_USBD_CDCEEM_RxBufGet (CPU_INT08U   class_nbr,
                                                  void        *p_arg,
                                                  CPU_INT16U  *p_buf_len)
{
    CPU_INT08U  *p_buf;
    NET_IF      *p_if = (NET_IF *)p_arg;
    NET_ERR      err_net;


    (void)&class_nbr;

    p_buf = NetBuf_GetDataPtr(p_if,
                              NET_TRANSACTION_RX,
                              NET_IF_ETHER_FRAME_MAX_SIZE,
                              NET_IF_IX_RX,
                              DEF_NULL,
                              p_buf_len,
                              DEF_NULL,
                             &err_net);
    (void)&err_net;

    return (p_buf);
}


/*
*********************************************************************************************************
*                                    NetDev_USBD_CDCEEM_RxBufRdy()
*
* Description : Signals that a rx buffer is ready.
*
* Argument(s) : class_nbr   CDC EEM class number.
*
*               p_arg       Pointer to application specific argument.
*
* Return(s)   : None.
*
* Caller(s)   : USBD CDC-EEM class.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_USBD_CDCEEM_RxBufRdy (CPU_INT08U   class_nbr,
                                           void        *p_arg)
{
    NET_IF   *p_if = (NET_IF *)p_arg;
    NET_ERR   err_net;


    (void)&class_nbr;

    NetIF_RxTaskSignal(p_if->Nbr, &err_net);

    (void)&err_net;
}


/*
*********************************************************************************************************
*                                   NetDev_USBD_CDCEEM_TxBufFree()
*
* Description : Signals that a tx buffer can be freed.
*
* Argument(s) : class_nbr   CDC EEM class number.
*
*               p_arg       Pointer to application specific argument.
*
*               p_buf       Pointer to buffer to be freed.
*
*               buf_len     Buffer length in bytes.
*
* Return(s)   : None.
*
* Caller(s)   : USBD CDC-EEM class.
*
* Note(s)     : None.
*********************************************************************************************************
*/

static  void  NetDev_USBD_CDCEEM_TxBufFree (CPU_INT08U   class_nbr,
                                            void        *p_arg,
                                            CPU_INT08U  *p_buf,
                                            CPU_INT16U   buf_len)
{
    CPU_INT08U  *p_data =  p_buf + USBD_CDC_EEM_HDR_LEN;
    NET_IF      *p_if   = (NET_IF *)p_arg;
    NET_ERR      err_net;


    (void)&class_nbr;
    (void)&buf_len;

    NetIF_TxDeallocTaskPost(p_data, &err_net);
    NetIF_DevTxRdySignal(p_if);

    (void)&err_net;
}

#endif  /* NET_IF_ETHER_MODULE_EN */
