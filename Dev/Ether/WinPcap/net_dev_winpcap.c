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
*                                               WinPcap
*
* Filename : net_dev_winpcap.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_WINPCAP_MODULE
#include  <Source/net_cfg_net.h>

#ifdef  NET_IF_ETHER_MODULE_EN
#include  <Source/net.h>
#include  <IF/net_if.h>
#include  "net_dev_winpcap.h"
#include  <IF/net_if_802x.h>


#ifdef  _MSC_VER
#ifndef  WIN32
#define  WIN32
#endif  /* _MSC_VER */
#endif  /* _MSC_VER */

#define  WPCAP
#define _WS2TCPIP_H_
#define _WINSOCKAPI_
#define _WINSOCK2API_

#ifdef  _WIN32
#define    WINDOWS_LEAN_AND_MEAN
#include  <windows.h>
#include "Include/pcap.h"

#else   /* _WIN32 */
#include  <pcap/pcap.h>

#endif  /* _WIN32 */

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#ifndef  NET_RX_TASK_PRIO
#define  NET_RX_TASK_PRIO       31
#endif

#define  NET_RX_TASK_STK_SIZE   1024

#define  INTERFACE_NUM_BUF_SIZE                            5u

#define  NET_DEV_FILTER_LEN                              117u

#define  NET_DEV_FILTER_QTY                               20u


#define PCAP_NETMASK_UNKNOWN    0xffffffff

#define NET_DEV_FILTER_PT1      "ether["
#define NET_DEV_FILTER_PT2      "] == 0x"
#define NET_DEV_FILTER_AND      " && "
#define NET_DEV_FILTER_OR       " || "

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
typedef  struct  net_dev_filter  NET_DEV_FILTER;

struct  net_dev_filter {
    NET_DEV_FILTER  *p_next;
    CPU_INT08U       filter[6];
    CPU_INT16U       len;
};

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data_winpcap {
    KAL_TASK_HANDLE       RxTaskHandle;
    KAL_SEM_HANDLE        RxSignalHandle;
    pcap_t               *PcapHandlePtr;
    struct  pcap_pkthdr  *PcapPktHdrPtr;
    CPU_INT08U           *RxDataPtr;
    MEM_POOL              FilterPool;
    NET_DEV_FILTER       *p_filter_head;
} NET_DEV_DATA;



/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
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

                                                                        /* -------- FNCT'S COMMON TO ALL DEV'S -------- */
static  void  NetDev_Init                  (NET_IF             *p_if,
                                            NET_ERR            *p_err);

static  void  NetDev_Start                 (NET_IF             *p_if,
                                            NET_ERR            *p_err);

static  void  NetDev_Stop                  (NET_IF             *p_if,
                                            NET_ERR            *p_err);


static  void  NetDev_Rx                    (NET_IF             *p_if,
                                            CPU_INT08U        **p_data,
                                            CPU_INT16U         *size,
                                            NET_ERR            *p_err);

static  void  NetDev_Tx                    (NET_IF             *p_if,
                                            CPU_INT08U         *p_data,
                                            CPU_INT16U          size,
                                            NET_ERR            *p_err);

static  void  NetDev_AddrMulticastAdd      (NET_IF             *pif,
                                            CPU_INT08U         *paddr_hw,
                                            CPU_INT08U          addr_hw_len,
                                            NET_ERR            *perr);

static  void  NetDev_AddrMulticastRemove   (NET_IF             *pif,
                                            CPU_INT08U         *paddr_hw,
                                            CPU_INT08U          addr_hw_len,
                                            NET_ERR            *perr);


static  void  NetDev_IO_Ctrl               (NET_IF             *pif,
                                            CPU_INT08U          opt,
                                            void               *p_data,
                                            NET_ERR            *perr);

static  void  NetDev_RxTaskStart (NET_IF   *p_if,
                                  NET_ERR  *p_err);

static  void  NetDev_RxTaskStop (NET_IF   *p_if,
                                 NET_ERR  *p_err);

static  void  NetDev_RxTask (void  *p_data);

static  void  NetDev_PcapInit (NET_DEV_DATA  *p_dev_data,
                               CPU_INT32U     pc_if_nbr,
                               NET_ERR       *p_err);


static void  NetDev_WinPcap_UpdateFilter(NET_IF          *p_if,
                                         NET_DEV_FILTER  *p_filter_head,
                                         NET_ERR         *p_err);

static  void NetDev_WinPcap_RxAddFilter    (NET_IF             *p_if,
                                            CPU_INT08U         *p_filter_str,
                                            CPU_INT16U          len,
                                            NET_ERR            *p_err);

static  void NetDev_WinPcap_RxRemoveFilter (NET_IF             *p_if,
                                            CPU_INT08U         *p_filter_str,
                                            CPU_INT16U          len,
                                            NET_ERR            *p_err);


static  void NetDev_WinPcap_ResetFilters   (NET_IF             *p_if,
                                            NET_ERR            *p_err);

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
                                                                                    /* WinPcap dev API fnct ptrs :      */
const  NET_DEV_API_ETHER  NetDev_API_WinPcap = { NetDev_Init,                       /*   Init/add                       */
                                                 NetDev_Start,                      /*   Start                          */
                                                 NetDev_Stop,                       /*   Stop                           */
                                                 NetDev_Rx,                         /*   Rx                             */
                                                 NetDev_Tx,                         /*   Tx                             */
                                                 NetDev_AddrMulticastAdd,           /*   Multicast addr add             */
                                                 NetDev_AddrMulticastRemove,        /*   Multicast addr remove          */
                                                 DEF_NULL,                          /*   ISR handler                    */
                                                 NetDev_IO_Ctrl,                    /*   I/O ctrl                       */
                                                 DEF_NULL,                          /* Phy reg rd                       */
                                                 DEF_NULL                           /* Phy reg wr                       */
                                               };


/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Perform Driver Layer OS initialization
*                   (b) Initialize driver status
*                   (c) Initialize driver statistics & error counters
*
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No Error.
*                           NET_DEV_ERR_INIT        General initialization error.
*                           NET_BUF_ERR_MEM_ALLOC   Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NET_DEV_DATA       *p_dev_data;
    NET_DEV_CFG_ETHER  *p_dev_cfg = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    LIB_ERR             lib_err;
    KAL_ERR             kal_err;
    CPU_SR_ALLOC();


                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf ix offset.                           */
    if (p_dev_cfg->RxBufIxOffset != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf size.                                */

    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize(p_if->Nbr,
                                     NET_TRANSACTION_RX,
                                     0,
                                     buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf ix offset.                           */
    if (p_dev_cfg->TxBufIxOffset != 0u) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */

    p_dev_data = Mem_HeapAlloc(sizeof(NET_DEV_DATA),
                                  sizeof(CPU_ALIGN),
                                 &reqd_octets,
                                 &lib_err);
    if (p_dev_data == DEF_NULL) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    p_if->Dev_Data = p_dev_data;


    p_dev_data->RxSignalHandle = KAL_SemCreate("Pcap Rx completed signal", DEF_NULL, &kal_err);
    if (kal_err != KAL_ERR_NONE) {
        *p_err = NET_DEV_ERR_FAULT;
         return;
    }

    p_dev_data->RxTaskHandle   = KAL_TaskAlloc("Pcap Rx task", DEF_NULL, NET_RX_TASK_STK_SIZE, DEF_NULL, &kal_err);
    if (kal_err != KAL_ERR_NONE) {
        *p_err = NET_DEV_ERR_FAULT;
         return;
    }

    CPU_CRITICAL_ENTER();
    NetDev_PcapInit(p_dev_data, p_dev_cfg->BaseAddr, p_err);
    CPU_CRITICAL_EXIT();
    if (*p_err != NET_DEV_ERR_NONE) {
         return;
    }


   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Start()
*
* Description : Start network interface hardware.
*
* Argument(s) : pif         Pointer to a network interface.
*               ---         Argument validated in NetIF_Start().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Ethernet device successfully started.
*
*                                                               - RETURNED BY NetIF_AddrHW_SetHandler() : --
*                               NET_ERR_FAULT_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT            Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               -- RETURNED BY NetIF_DevCfgTxRdySignal() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_OS_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (1) The physical HW address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_Ether_HW_AddrSet(), hard coded HW addresses from the device configuration
*                   structure, or auto-loading EEPROM's. Changes to the physical address only take
*                   effect when the device transitions from the DOWN to UP state.
*
*               (2) The device HW address is set from one of the data sources below. Each source is
*                   listed in the order of precedence.
*                       a) Device Configuration Structure       Configure a valid HW address in order
*                                                                   to hardcode the HW via compile time.
*
*                                                               Configure either "00:00:00:00:00:00" or
*                                                                   an empty string, "", in order to
*                                                                   configure the HW using using method
*                                                                   b).
*
*                       b) NetIF_Ether_HW_AddrSet()             Call NetIF_Ether_HW_AddrSet() if the HW
*                                                                   address needs to be configured via
*                                                                   run-time from a different data
*                                                                   source. E.g. Non auto-loading
*                                                                   memory such as I2C or SPI EEPROM.
*                                                                  (see Note #2).
*
*                       c) Auto-Loading via EEPROM.             If neither options a) or b) are used,
*                                                               the IF layers copy of the HW address
*                                                               will be obtained from the network
*                                                               hardware HW address registers.
*
*               (3) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, DMA based interfaces may have several
*                   frames configured for transmission before blocking is required. The number
*                   of queued transmit frames depends on the number of configured transmit
*                   descriptors.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *p_if,
                            NET_ERR  *p_err)
{
    NET_DEV_CFG_ETHER  *p_dev_cfg  = (NET_DEV_CFG_ETHER *)p_if->Dev_Cfg;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;



                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #1 & #2.                                   */

    NetASCII_Str_to_MAC(p_dev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #2a).                                  */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) p_if->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.    */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Else get  app-configured IF layer HW MAC address, ...*/
                                                                /* ... if any (see Note #2b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(p_if->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(p_if->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any (see Note #2c).           */
                                                                /* Use hard-coded addr.                                 */
            hw_addr[0] = 0x00;
            hw_addr[1] = 0x50;
            hw_addr[2] = 0xC2;
            hw_addr[3] = 0x25;
            hw_addr[4] = 0x60;
            hw_addr[5] = 0x02;

            NetIF_AddrHW_SetHandler((NET_IF_NBR  ) p_if->Nbr,    /* Configure IF layer to use automatically-loaded ...   */
                                    (CPU_INT08U *)&hw_addr[0],  /* ... HW MAC address.                                  */
                                    (CPU_INT08U  ) sizeof(hw_addr),
                                     (NET_ERR    *) p_err);
            if (*p_err != NET_IF_ERR_NONE) {                     /* No valid HW MAC address configured, return error.    */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */

        ;                                                       /* No device HW MAC address to configure.               */

    }

                                                                /* Add Unicast HW add to captude device.                */
    NetDev_WinPcap_RxAddFilter(p_if,
                               hw_addr,
                               sizeof(hw_addr),
                               p_err);

    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    Mem_Set(hw_addr, 0xFF, sizeof(hw_addr));                    /* Add Broadcast HW add to captude device.          */
    NetDev_WinPcap_RxAddFilter(p_if,
                               hw_addr,
                               sizeof(hw_addr),
                               p_err);

    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_RxTaskStart(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(p_if,                               /* See Note #3.                                         */
                            p_dev_cfg->TxDescNbr,
                            p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        return;
    }


   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Stop()
*
* Description : (1) Shutdown network interface hardware :
*
*                   (a) Disable receive and transmit interrupts
*                   (b) Disable the receiver and transmitter
*                   (c) Clear pending interrupt requests
*
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE    No Error
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Stop() via 'pdev_api->Stop()'.
*
* Note(s)     : (2) (a) (1) It is recommended that a device driver should only post all currently-used,
*                           i.e. not-fully-transmitted, transmit buffers to the network interface transmit
*                           deallocation queue.
*
*                       (2) However, a driver MAY attempt to post all queued &/or transmitted buffers.
*                           The network interface transmit deallocation task will silently ignore any
*                           unknown or duplicate transmit buffers.  This allows device drivers to
*                           indiscriminately & easily post all transmit buffers without determining
*                           which buffers have NOT yet been transmitted.
*
*                   (b) (1) Device drivers should assume that the network interface transmit deallocation
*                           queue is large enough to post all currently-used transmit buffers.
*
*                       (2) If the transmit deallocation queue is NOT large enough to post all transmit
*                           buffers, some transmit buffers may/will be leaked/lost.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NetDev_WinPcap_ResetFilters(p_if, p_err);

    NetDev_RxTaskStop(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : Returns a pointer to the received data to the caller.
*                   (1) Determine the descriptor that caused the interrupt.
*                   (2) Obtain a pointer to a Network Buffer Data Area for storing new data.
*                   (3) Reconfigure the descriptor with the pointer to the new data area.
*                   (4) Pass a pointer to the received data area back to the caller via p_data;
*                   (5) Clear interrupts
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to pointer to received DMA data area. The recevied data
*                       area address should be returned to the stack by dereferencing
*                       p_data as *p_data = (address of receive data area).
*
*               size    Pointer to size. The number of bytes received should be returned
*                       to the stack by dereferencing size as *size = (number of bytes).
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error
*                           NET_DEV_ERR_RX              Generic Rx error.
*                           NET_DEV_ERR_INVALID_SIZE    Invalid Rx frame size.
*                           NET_BUF error codes         Potential NET_BUF error codes
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pdev_api->Rx()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *p_if,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *p_err)
{
            NET_DEV_DATA  *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    struct  pcap_pkthdr   *p_hdr;
            CPU_INT16U     len;
            CPU_INT08U    *p_buf;
            KAL_ERR        kal_err;


    p_hdr  =  p_dev_data->PcapPktHdrPtr;
    len    =  p_hdr->len;
                                                                /* Verify frame len.                                    */
    if (len > NET_IF_ETHER_FRAME_MAX_SIZE) {
       *size   = 0;
       *p_data = DEF_NULL;
       *p_err   = NET_DEV_ERR_INVALID_SIZE;
        KAL_SemPost(p_dev_data->RxSignalHandle, KAL_OPT_POST_NONE, &kal_err);
        return;
    }

                                                                /* Request a buffer                                     */
    p_buf = NetBuf_GetDataPtr(p_if,
                              NET_TRANSACTION_RX,
                              NET_IF_ETHER_FRAME_MAX_SIZE,
                              NET_IF_IX_RX,
                              DEF_NULL,
                              DEF_NULL,
                              DEF_NULL,
                              p_err);
    if (*p_err != NET_BUF_ERR_NONE) {                           /* If unable to get a buffer, discard the frame         */
       *size   = 0;
       *p_data = DEF_NULL;
        KAL_SemPost(p_dev_data->RxSignalHandle, KAL_OPT_POST_NONE, &kal_err);
        return;
    }



    Mem_Copy(p_buf, p_dev_data->RxDataPtr, len);                  /* Mem_Copy received data into new buffer.              */


    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {                    /* See Note #1.                                         */
        len = NET_IF_ETHER_FRAME_MIN_SIZE;
    }

   *p_data = p_buf;
   *size   = len;

    KAL_SemPost(p_dev_data->RxSignalHandle, KAL_OPT_POST_NONE, &kal_err);

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : Transmit the specified data.
*                   (1) Check if the transmitter is ready.
*                   (2) Configure the next transmit descriptor for pointer to data and data size.
*                   (3) Issue the transmit command.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No Error
*                           NET_DEV_ERR_TX_BUSY     No Tx descriptors available
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pdev_api->Tx()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *p_if,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *p_err)
{
    NET_DEV_DATA  *pdev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    CPU_INT32U     tx;
    NET_ERR        err;
    CPU_SR_ALLOC();


    CPU_CRITICAL_ENTER();
    tx = pcap_sendpacket(pdev_data->PcapHandlePtr, p_data, size);
    if (tx != 0u) {
        *p_err = NET_ERR_TX;
    } else {
        *p_err = NET_DEV_ERR_NONE;
    }
    CPU_CRITICAL_EXIT();

    NetIF_TxDeallocTaskPost(p_data, &err);
    if (err == NET_IF_ERR_NONE) {
        NetIF_DevTxRdySignal(p_if);                     /* Signal Net IF that Tx resources are available        */
    }
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
*                               NET_DEV_ERR_NONE                Hardware address successfully configured.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : (1) The device is capable of the following multicast address filtering techniques :
*                       (c) Promiscous non filtering.
*
*               (2) This function implements the filtering mechanism described in 1a.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
    NetDev_WinPcap_RxAddFilter(pif,
                               paddr_hw,
                               addr_hw_len,
                               perr);

    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }

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
*                               NET_DEV_ERR_NONE                Hardware address successfully removed.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastRemove()'.
*
* Note(s)     : (1) The device is capable of the following multicast address filtering techniques :
*                       (a) Promiscous non filtering.
*
*               (2) This function implements the filtering mechanism described in 1a.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
    NetDev_WinPcap_RxRemoveFilter(pif,
                                  paddr_hw,
                                   addr_hw_len,
                                  perr);

    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_IO_Ctrl()
*
* Description : Implement various hardware functions.
*
* Argument(s) : pif     Pointer to interface requiring service.
*
*               opt     Option code representing desired function to perform. The Network Protocol Suite
*                       specifies the option codes below. Additional option codes may be defined by the
*                       driver developer in the driver's header file.
*                           NET_DEV_LINK_STATE_GET
*                           NET_DEF_LINK_STATE_UPDATE
*
*                       Driver defined operation codes MUST be defined starting from 20 or higher
*                       to prevent clashing with the pre-defined operation code types. See fec.h
*                       for more details.
*
*               data    Pointer to optional data for either sending or receiving additional function
*                       arguments or return data.
*
*               perr    Pointer to return error code.
*                           NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid option number specified.
*                           NET_ERR_FAULT_NULL_FNCT                Null interface function pointer encountered.
*
*                           NET_DEV_ERR_NONE                    IO Ctrl operation completed successfully.
*                           NET_DEV_ERR_NULL_PTR                Null argument pointer passed.
*
*                           NET_ERR_FAULT_NULL_PTR                Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_TIMEOUT_RESET           Phy reset          time-out.
*                           NET_PHY_ERR_TIMEOUT_AUTO_NEG        Auto-Negotiation   time-out.
*                           NET_PHY_ERR_TIMEOUT_REG_RD          Phy register read  time-out.
*                           NET_PHY_ERR_TIMEOUT_REG_WR          Phy register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IO_CtrlHandler() via 'pdev_api->IO_Ctrl()',
*               NetPhy_LinkStateGet()        via 'pdev_api->IO_Ctrl()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *pif,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *perr)
{
    NET_DEV_LINK_ETHER  *plink_state;


   (void)&pif;                                                  /* Prevent 'variable unused' compiler warning.          */

    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;
             if (plink_state == (NET_DEV_LINK_ETHER *)0) {
                *perr = NET_DEV_ERR_NULL_PTR;
                 return;
             }
             plink_state->Duplex = NET_PHY_DUPLEX_FULL;
             plink_state->Spd    = NET_PHY_SPD_100;
            *perr = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
            *perr = NET_DEV_ERR_NONE;
             break;


        default:
            *perr = NET_IF_ERR_INVALID_IO_CTRL_OPT;
             break;
    }
}


static  void  NetDev_RxTaskStart (NET_IF   *p_if,
                                  NET_ERR  *p_err)
{
    NET_DEV_DATA  *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    KAL_ERR        kal_err;


    KAL_TaskCreate(p_dev_data->RxTaskHandle, NetDev_RxTask, p_if, NET_RX_TASK_PRIO, DEF_NULL, &kal_err);
    if (kal_err != KAL_ERR_NONE) {
        *p_err = NET_DEV_ERR_FAULT;
    }
}


static  void  NetDev_RxTaskStop (NET_IF   *p_if,
                                 NET_ERR  *p_err)
{
    NET_DEV_DATA  *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    KAL_ERR        kal_err;



    KAL_TaskDel(p_dev_data->RxTaskHandle, &kal_err);
    KAL_SemSet(p_dev_data->RxSignalHandle, 0u, &kal_err);
   (void)&kal_err;
   (void)&p_err;
}


static  void  NetDev_RxTask (void  *p_data)
{
    NET_IF           *p_if;
    NET_DEV_DATA     *p_dev_data;
    CPU_INT16U        fail_cnt;
    CPU_INT08U        ret;
    NET_ERR           net_err;
    CPU_SR_ALLOC();


   (void)&p_data;                                               /* Prevent compiler warning.                            */

    p_if       = (NET_IF       *)p_data;
    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    fail_cnt =  0u;

    while (DEF_TRUE) {
                                                                /* To optimize CPU usage, give up CPU for 10ms once ... */
                                                                /* ... fail threshold has been reached.                 */
        if (fail_cnt >= 1000) {
            KAL_Dly(10u);
            fail_cnt = 0u;
        }

        Net_GlobalLockAcquire(&NetDev_RxTask, &net_err);


                                                                /* Retrieve packet.                                     */
        CPU_CRITICAL_ENTER();
        ret = pcap_next_ex(                 p_dev_data->PcapHandlePtr,
                                           &p_dev_data->PcapPktHdrPtr,
                          (const u_char **)&p_dev_data->RxDataPtr);
        CPU_CRITICAL_EXIT();
        Net_GlobalLockRelease();

        if (ret == 1u) {                                        /* Packet captured successfully.                        */
            NetIF_RxTaskSignal(p_if->Nbr, &net_err);             /* Signal Net IF RxQ Task of new frame                  */
            if (net_err == NET_IF_ERR_NONE) {
                KAL_ERR  kal_err;


                KAL_SemPend(p_dev_data->RxSignalHandle, KAL_OPT_PEND_NONE, 0, &kal_err);
                (void)&kal_err;
            }

        } else {
            fail_cnt++;
        }
    }
}




/*
*********************************************************************************************************
*                                           WinPcap_Init()
*
* Description : (1) Initialize & start WinPcap :
*
*                   (a) Retrieve the device list on the local machine.
*                   (b) Print the list.
*                   (c) Ask the user for a particular device to use.
*                   (d) Open the device.
*                   (e) Initialize filter pool for packet capture filtering.
*
* Argument(s) : if_ix       Index of the interface to be initialized.
*
*               p_handle    Winpcap handle.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE        Network interface card successfully initialized.
*                               NET_DEV_ERR_INIT        Network initialization error.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) This number guarentees that whole packet is captured on all link layers.
*********************************************************************************************************
*/

static  void  NetDev_PcapInit (NET_DEV_DATA  *p_dev_data,
                               CPU_INT32U     pc_if_nbr,
                               NET_ERR       *p_err)
{

    CPU_INT08U        interfaceNumStr[INTERFACE_NUM_BUF_SIZE];
    CPU_CHAR         *atoiRetVal;
    pcap_t           *p_adhandle;
    pcap_if_t        *alldevs;
    pcap_if_t        *d;
    CPU_INT08U        errbuf[PCAP_ERRBUF_SIZE];
    CPU_INT08U        inum;
    CPU_INT08U        pc_if_ix;
    CPU_SIZE_T        reqd_octets;
    LIB_ERR           lib_err;



    p_dev_data->PcapHandlePtr = NULL;

                                                                /* Retrieve device list on the local machine.           */
    if (pcap_findalldevs(&alldevs, (char *)errbuf) == -1) {
        printf("\nError in pcap_findalldevs: %s\n", errbuf);
       *p_err = NET_DEV_ERR_INIT;
        return;
    }

                                                                /* Print device list.                                   */
    pc_if_ix = 0u;
    for (d = alldevs; d != NULL; d = d->next) {
        if (pc_if_ix == 0u) {
            printf("\nPlease choose among the following adapter(s):\n\n");
        }
                                                                /* Open selected device.                                */
        p_adhandle = pcap_open_live(        d->name,            /* Name of the device.                                  */
                                            65536u,             /* Portion of packet to capture (see Note #1).          */
                                            1,                  /* Promoscuous mode.                                    */
                                            -1,                 /* No read timeout.                                     */
                                    (char *)errbuf);            /* Error buffer.                                        */

        if (p_adhandle != (pcap_t *)NULL) {
            if (pcap_datalink(p_adhandle) == DLT_EN10MB) {
                printf("%u. %s\n", ++pc_if_ix, d->name);
                if (d->description) {
                    printf("   (%s)\n\n", d->description);
                } else {
                    printf("   (No description available)\n\n");
                }
            }

            pcap_close(p_adhandle);
        }
    }

    if (pc_if_ix == 0u) {
        printf("\nNo interfaces found! Make sure WinPcap is installed.\n");
       *p_err = NET_DEV_ERR_INIT;
        return;
    }

    if ((pc_if_nbr < 1u) || (pc_if_nbr > pc_if_ix)) {
        inum = 0u;
        while ((inum < 1u) || (inum > pc_if_ix)) {
            printf("\nEnter the interface number (1-%u):", pc_if_ix);

            atoiRetVal = fgets((CPU_CHAR *)&interfaceNumStr[0], INTERFACE_NUM_BUF_SIZE, stdin);
            if (atoiRetVal != NULL) {
                inum = atoi((const char*)interfaceNumStr);
            }

            if ((inum < 1u) || (inum > pc_if_ix)) {
                printf("\nInterface number out of range.");
            }
        }
    } else {
        //printf("\nUsing interface #%u.", pc_if_nbr);
        inum = pc_if_nbr;
    }
    printf("\n\n");

                                                                /* Point to selected adapter.                           */
    for (d = alldevs, pc_if_ix = 0u; pc_if_ix < (inum - 1u); d = d->next, pc_if_ix++) {
        ;
    }

                                                                /* Open selected device.                                */
    p_adhandle = pcap_open_live(       d->name,                 /* Name of the device.                                  */
                                       65536u,                  /* Portion of packet to capture (see Note #1).          */
                                       1,                       /* Promiscuous mode.                                    */
                                       -1,                      /* No read timeout.                                     */
                               (char *)errbuf);                 /* Error buffer.                                        */

    if (p_adhandle == NULL) {
        printf("Unable to open the adapter. %s is not supported by WinPcap.\n\n", d->name);
        pcap_freealldevs(alldevs);                              /* Free device list.                                    */
       *p_err = NET_DEV_ERR_INIT;
        return;
    }

#ifdef _WIN32
                                                                /* Set large kernel buffer size (10 MB).                */
    if (pcap_setbuff(p_adhandle, (10u * 1024u * 1024u)) == -1 ) {
        printf("Unable to change kernel buffer size.\n\n");
        pcap_freealldevs(alldevs);                              /* Free device list.                                    */
        pcap_close(p_adhandle);
       *p_err = NET_DEV_ERR_INIT;
        return;
    }
#endif

    pcap_freealldevs(alldevs);                                  /* Free device list.                                    */


                                                                /* Init filter pool for packet capture (see Note #1e).  */
    Mem_PoolCreate(                  &p_dev_data->FilterPool,
                                      NULL,
                                      0u,
                   (MEM_POOL_BLK_QTY) NET_DEV_FILTER_QTY,
                   (CPU_SIZE_T      ) sizeof(NET_DEV_FILTER),
                   (CPU_SIZE_T      ) CPU_WORD_SIZE_32,
                                     &reqd_octets,
                                     &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
        *p_err = NET_DEV_ERR_INIT;
        return;
    }

    p_dev_data->p_filter_head = NULL;


    p_dev_data->PcapHandlePtr = p_adhandle;
   *p_err                     = NET_DEV_ERR_NONE;
}

/*
*********************************************************************************************************
*                                     NetDev_WinPcap_RxAddFilter()
*
* Description : Add an address to the packet capture filter.
*
* Argument(s) : if_nbr          Index of the interface on which to add the filter.
*
*               p_filter_addr   Pointer to an address to add to the packet capture filter.
*
*               len             Length of the filter address.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Address filter successfuly added or already present.
*
*                               NET_DEV_ERR_FAULT   An error occured.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start() and NetDev_AddrMulticastAdd().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_WinPcap_RxAddFilter (NET_IF       *p_if,
                                          CPU_INT08U   *p_filter_addr,
                                          CPU_INT16U    len,
                                          NET_ERR      *p_err)
{
    NET_DEV_DATA     *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    NET_DEV_FILTER   *p_filter;
    CPU_BOOLEAN       cmp;
    LIB_ERR           lib_err;
    CPU_SR_ALLOC();


                                                                /* Verify if addr is not already present in filters. */
    p_filter = p_dev_data->p_filter_head;
    cmp      = DEF_NO;

    while (p_filter != NULL) {
        cmp |= Mem_Cmp(p_filter->filter, p_filter_addr, len);

        p_filter = p_filter->p_next;
    }

    if (cmp == DEF_YES) {                                       /* If filter is already present, no need to add ... */
        *p_err = NET_DEV_ERR_NONE;                              /* ... it to the filter list.                       */
        return;
    }

    p_filter = (NET_DEV_FILTER *)Mem_PoolBlkGet(&p_dev_data->FilterPool,
                                                 0,
                                                &lib_err);

    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

    Mem_Copy(&p_filter->filter, p_filter_addr, len);

    p_filter->len    = len;
    p_filter->p_next = NULL;

    CPU_CRITICAL_ENTER();
                                                                /* Insert filter at filter list head.                   */
    p_filter->p_next = p_dev_data->p_filter_head;
    p_dev_data->p_filter_head = p_filter;
    CPU_CRITICAL_EXIT();

                                                                /* Update dev packet filter.                            */
    NetDev_WinPcap_UpdateFilter(p_if,
                                p_dev_data->p_filter_head,
                                p_err);

    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                    NetDev_WinPcap_RxRemoveFilter()
*
* Description : Remove an address to the packet capture filter.
*
* Argument(s) : if_nbr          Index of the interface on which to remove the filter.
*
*               p_filter_addr   Pointer to an address to remove from the packet capture filter.
*
*               len             Length of the filter address.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Address filter successfuly removed.
*
*                               NET_DEV_ERR_FAULT   An error occured.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_WinPcap_ResetFilters() and NetDev_AddrMulticastRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_WinPcap_RxRemoveFilter (NET_IF       *p_if,
                                             CPU_INT08U   *p_filter_addr,
                                             CPU_INT16U    len,
                                             NET_ERR      *p_err)
{
    NET_DEV_DATA    *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    NET_DEV_FILTER  *p_filter;
    NET_DEV_FILTER  *p_filter_prev;
    CPU_BOOLEAN      cmp;
    LIB_ERR          lib_err;
    CPU_SR_ALLOC();


                                                                /* Verify if addr is not already present in filters. */
    p_filter      = p_dev_data->p_filter_head;
    p_filter_prev = DEF_NULL;
    cmp           = DEF_NO;

    while ((p_filter != NULL) && (cmp == DEF_NO)) {
        cmp = Mem_Cmp(p_filter->filter, p_filter_addr, len);

        if (cmp == DEF_NO) {
            p_filter_prev = p_filter;
            p_filter      = p_filter->p_next;
        }
    }

    if (cmp == DEF_NO) {                                        /* If filter is not in the filter list then ...         */
        *p_err = NET_DEV_ERR_FAULT;                             /* ... return err.                                      */
        return;
    }

    CPU_CRITICAL_ENTER();                                       /* Remove filter from the filter list.                  */
    if (p_filter == p_dev_data->p_filter_head) {
        p_dev_data->p_filter_head = p_filter->p_next;
    } else {
        p_filter_prev->p_next = p_filter->p_next;
    }
    CPU_CRITICAL_EXIT();

    Mem_PoolBlkFree(&p_dev_data->FilterPool,                         /* Free the filter memory block.                        */
                     p_filter,
                    &lib_err);

    if (lib_err == LIB_ERR_NONE) {
        *p_err = NET_DEV_ERR_FAULT;
        return;
    }
                                                                /* Update dev packet filter.                            */
    NetDev_WinPcap_UpdateFilter(p_if,
                                p_dev_data->p_filter_head,
                                p_err);

    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
        return;
    }

   *p_err = NET_DEV_ERR_NONE;
}

/*
*********************************************************************************************************
*                                    NetDev_WinPcap_RxRemoveFilter()
*
* Description : Update the packet capture filter.
*
* Argument(s) : if_nbr          Index of the interface on which to update the filter.
*
*               p_filter_head   Pointer to the head of the address filter list.
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Address filter successfuly removed.
*
*                               NET_DEV_ERR_FAULT   An error occured.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_WinPcap_RxAddFilter() and NetDev_WinPcap_RxRemoveFilter().
*
* Note(s)     : (1) (a) An Address Filter (AF) is in the following format:
*                       "ether[0] == 0xFF && ether[1] == 0xFF && ether[2] == 0xFF &&
*                        ether[3] == 0xFF && ether[4] == 0xFF && ether[5] == 0xFF"
*
*                   (b) AFs are OR'd together to extend the filter capabilities:
*                       (AF) || (AF) || ... || (AF)
*
*                   (c) The resulting filter expression in then compiled and feed to Winpcap to perform the
*                       desired filtering function.
*********************************************************************************************************
*/

static  void  NetDev_WinPcap_UpdateFilter (NET_IF          *p_if,
                                           NET_DEV_FILTER  *p_filter_head,
                                           NET_ERR         *p_err)
{
            NET_DEV_DATA    *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
            CPU_INT08U       str[NET_DEV_FILTER_QTY * (NET_DEV_FILTER_LEN + 7)];
            CPU_CHAR         addr_str[6u*3u];
            NET_DEV_FILTER  *p_filter;
            CPU_INT16U       ix;
            CPU_INT08U      *p_str;
            CPU_INT16U       str_ix;
    struct  bpf_program      prog;
            CPU_INT32S       result;


    str_ix   = 0u;
    p_str    = str;
    p_filter = p_filter_head;

                                                                /* Appending filter to the filter expression.           */
    while (p_filter != NULL) {

        NetASCII_MAC_to_Str(p_filter->filter, addr_str, DEF_NO, DEF_NO, p_err);

                                                                /* Append "(filter) || " to filter str.                 */

        p_str[str_ix++] = '(';

        for (ix = 0 ; ix < p_filter->len; ix++) {               /* Writing "ether["               to buf.               */
            Mem_Copy(&p_str[str_ix], NET_DEV_FILTER_PT1, sizeof(NET_DEV_FILTER_PT1)-1);
            str_ix += sizeof(NET_DEV_FILTER_PT1) - 1;

            p_str[str_ix++] = ix + '0';                         /* Writing "ether[X"              to buf.               */

                                                                /* Writing "ether[X] == 0x"       to buf.               */
            Mem_Copy(&p_str[str_ix], NET_DEV_FILTER_PT2, sizeof(NET_DEV_FILTER_PT2)-1);
            str_ix += sizeof(NET_DEV_FILTER_PT2) - 1;

                                                                /* Writing "ether[X] == 0xXX"     to buf.               */
            Mem_Copy(&p_str[str_ix], &addr_str[3*ix], 2);
            str_ix += 2;

                                                                /* If the octet is not the last one of the addr, ...    */
                                                                /* ... it must me AND'd wiht the next octet.            */
            if (ix != (p_filter->len -1)) {                     /* Writing "ether[X] == 0xXX && " to buf.               */
                Mem_Copy(&p_str[str_ix], NET_DEV_FILTER_AND, sizeof(NET_DEV_FILTER_AND)-1);
                str_ix += sizeof(NET_DEV_FILTER_AND) - 1;
            }
        }
                                                                /* Closing condition of this addr filter.               */
        p_str[str_ix++] = ')';

                                                                /* If the filter is not the last of the list, ...       */
                                                                /* ... it must me OR'd wiht the next filter.            */
        if (p_filter->p_next != NULL) {
            Mem_Copy(&p_str[str_ix], NET_DEV_FILTER_OR, sizeof(NET_DEV_FILTER_OR)-1);
            str_ix += sizeof(NET_DEV_FILTER_OR) - 1;
        }

        p_filter = p_filter->p_next;
    }
                                                                /* Filter str must be null terminated.                  */
    p_str[str_ix++] = '\0';

                                                                /* Compile the string into a winpcap filter.            */
    result = pcap_compile(p_dev_data->PcapHandlePtr, &prog, (char *)p_str, 1, PCAP_NETMASK_UNKNOWN);

    if (result < 0) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

                                                                /* Set the filter to the capturing device.              */
    pcap_setfilter(p_dev_data->PcapHandlePtr, &prog);

    if (result < 0) {
       *p_err = NET_DEV_ERR_FAULT;
        return;
    }

    *p_err = NET_DEV_ERR_NONE;
}

/*
*********************************************************************************************************
*                                     NetDev_WinPcap_ResetFilters()
*
* Description : Removes all the filters from the packet capture filter.
*
* Argument(s) : if_nbr          Index of the interface on which to remove all the filters.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_WinPcap_ResetFilters (NET_IF   *p_if,
                                           NET_ERR  *p_err)
{
    NET_DEV_DATA    *p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;
    NET_DEV_FILTER  *p_filter;


    p_filter = p_dev_data->p_filter_head;


    while (p_filter != NULL) {
        NetDev_WinPcap_RxRemoveFilter(p_if,
                                      p_filter->filter,
                                      p_filter->len,
                                      p_err);

        p_filter = p_filter->p_next;
    }
}

#endif



