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
*                                        WIRELESS SPI TEMPLATE
*
* Filename : net_dev_wifi_template_spi.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_TEMPLATE_WIFI_SPI_MODULE



#include  <Source/net.h>
#include  <IF/net_if_wifi.h>
#include  <Dev/WiFi/Manager/Generic/net_wifi_mgr.h>
#include  "net_dev_wifi_template_spi.h"

#ifdef  NET_IF_WIFI_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Receive buffers MUST contain a prefix of at least one octet for the packet type
*               and any other data that help the driver to demux the management frame.
*
*           (2) The driver MUST handle and implement generic commands defined in net_if_wifi.h. But the driver
*               can also define and implement its own management command which need an response by calling
*               the wireless manager api (p_mgr_api->Mgmt()) to send the management command and to receive
*               the response.
*
*               (a) Driver management command code '100' series reserved for driver.
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_DEV_RX_BUF_SPI_OFFSET_OCTETS                  4u   /* See note 1.                                          */
#define  NET_DEV_SPI_CLK_FREQ_MIN                    1000000L
#define  NET_DEV_SPI_CLK_FREQ_MAX                   60000000L


#define  NET_DEV_MGMT_NONE                               100u   /* See note 2.                                          */
#define  NET_DEV_MGMT_BOOT_FIRMWARE                      101u
#define  NET_DEV_MGMT_BOOT_UPGRADE                       101u
#define  NET_DEV_MGMT_INIT_MAC                           102u
#define  NET_DEV_MGMT_SET_PWR_SAVE                       103u
#define  NET_DEV_MGMT_SET_HW_ADDR                        104u
#define  NET_DEV_MGMT_GET_HW_ADDR                        105u
#define  NET_DEV_MGMT_SET_SPD_MODE                       106u
#define  NET_DEV_MGMT_LOAD_FIRMWARE                      107u
#define  NET_DEV_MGMT_LOAD_FIRMWARE_FF_DATA              108u
#define  NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM1             109u
#define  NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM2             110u
#define  NET_DEV_MGMT_GET_FIRMWARE_VERSION               120u

#define  NET_DEV_RESP_CARD_RDY                             1u
#define  NET_DEV_RESP_CTRL                                 2u
#define  NET_DEV_RESP_INIT                                 3u
#define  NET_DEV_RESP_JOIN                                 4u
#define  NET_DEV_RESP_SCAN                                 5u
#define  NET_DEV_RESP_TADM                                 6u
#define  NET_DEV_RESP_TAIM1                                7u
#define  NET_DEV_RESP_TAIM2                                8u
#define  NET_DEV_RESP_WAKEUP                               9u

#define  NET_DEV_FIRMWARE_VERSION                         10u


#define  NET_DEV_INT_RX_STATUS_ERR_MSK                     1u
#define  NET_DEV_INT_TYPE_MASK_DATA                        2u


#define  NET_DEV_AP_PER_SCAN_MAX                          32u


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) Instance specific data area structures should be defined within this section.
*               The data area structure typically includes error counters and variables used
*               to track the state of the device.  Variables required for correct operation
*               of the device MUST NOT be defined globally and should instead be included within
*               the instance specific data area structure and referenced as pif->Dev_Data structure
*               members.
*
*           (2) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {

    CPU_INT08U      SubState;
    CPU_INT08U      WaitResponseType;

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR         StatRxPktCtr;
    NET_CTR         StatTxPktCtr;
#endif
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR         ErrRxPktDiscardedCtr;
    NET_CTR         ErrTxPktDiscardedCtr;
#endif

    CPU_INT08U     *TxBufCompPtr;                               /* See Note #2.                                         */
    CPU_INT08U     *GlobalBufPtr;
} NET_DEV_DATA;


typedef  struct  net_dev_desc {
    CPU_INT08U      Octet[16];
} NET_DEV_DESC;


typedef  struct  net_dev_desc_ctrl {
    NET_DEV_DESC    Desc;
    CPU_INT08U     *WrBufPtr;
    CPU_INT08U     *RdBufPtr;
    CPU_INT32U      len;
} NET_DEV_DESC_CTRL;


typedef  struct  net_dev_mgmt_scan_info {
    CPU_INT08U        Ch;
    CPU_INT08U        SecMode;
    CPU_INT08U        RSSID;
    NET_IF_WIFI_SSID  SSID;
} NET_DEV_MGMT_SCAN_AP;

typedef  struct  net_dev_mgmt_scan_response {
    CPU_INT32U            ScanCnt;
    CPU_INT32U            Error;
    NET_DEV_MGMT_SCAN_AP  APs[NET_DEV_AP_PER_SCAN_MAX];
} NET_DEV_MGMT_SCAN_RESP;

typedef  struct  net_dev_mgmt_frame_desc {
    CPU_INT16U   Word0;
    CPU_INT16U   Word1;
    CPU_INT16U   Word2;
    CPU_INT16U   Word3;
    CPU_INT16U   Word4;
    CPU_INT16U   Word5;
    CPU_INT16U   Word6;
    CPU_INT16U   Word7;
} NET_DEV_MGMT_DESC;

typedef  struct  net_dev_mgmt_frame_scan {
    CPU_INT32U        Channel;
    NET_IF_WIFI_SSID  SSID;
} NET_DEV_MGMT_FRAME_SCAN;

typedef  struct  net_dev_mgmt_frame_join {
    CPU_INT08U        NetworkType;
    CPU_INT08U        SecType;
    CPU_INT08U        DataRate;
    CPU_INT08U        PwrLevel;
    NET_IF_WIFI_PSK   PSK;
    NET_IF_WIFI_SSID  SSID;
    CPU_INT08U        Mode;
    CPU_INT08U        Ch;
    CPU_INT08U        Action;
} NET_DEV_MGMT_FRAME_JOIN;


typedef  struct  net_dev_mgmt_frame_power_save {
    CPU_INT08U  Mode;
    CPU_INT08U  Interval;
} NET_DEV_MGMT_FRAME_PWR_SAVE;

typedef union net_dev_mgmt_frame {
    NET_DEV_MGMT_FRAME_SCAN         Scan;
    NET_DEV_MGMT_FRAME_JOIN         Join;
    NET_DEV_MGMT_FRAME_PWR_SAVE     Pwr_Save;
} NET_DEV_MGMT_FRAME;

typedef  struct  net_dev_mgmt_desc_frame {
    NET_DEV_MGMT_DESC   Desc;
    NET_DEV_MGMT_FRAME  Frame;
} NET_DEV_MGMT_DESC_FRAME;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*
* Note(s) : (1) Global variables are highly discouraged and should only be used for storing NON-instance
*               specific data and the array of instance specific data.  Global variables, those that are
*               not declared within the NET_DEV_DATA area, are not multiple-instance safe and could lead
*               to incorrect driver operation if used to store device state information.
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Device driver functions may be arbitrarily named.  However, it is recommended that device
*               driver functions be named using the names provided below.  All driver function prototypes
*               should be located within the driver C source file ('net_dev_&&&.c') & be declared as
*               static functions to prevent name clashes with other network protocol suite device drivers.
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                              /* -------- FNCT'S COMMON TO ALL DEV'S -------- */
static  void        NetDev_Init                (NET_IF               *p_if,
                                                NET_ERR              *p_err);


static  void        NetDev_Start               (NET_IF               *p_if,
                                                NET_ERR              *p_err);

static  void        NetDev_Stop                (NET_IF               *p_if,
                                                NET_ERR              *p_err);


static  void        NetDev_Rx                  (NET_IF               *p_if,
                                                CPU_INT08U          **p_data,
                                                CPU_INT16U           *p_size,
                                                NET_ERR              *p_err);

static  void        NetDev_Tx                  (NET_IF               *p_if,
                                                CPU_INT08U           *p_data,
                                                CPU_INT16U            size,
                                                NET_ERR              *p_err);


static  void        NetDev_AddrMulticastAdd    (NET_IF               *p_if,
                                                CPU_INT08U           *p_addr_hw,
                                                CPU_INT08U            addr_hw_len,
                                                NET_ERR              *p_err);

static  void        NetDev_AddrMulticastRemove (NET_IF               *p_if,
                                                CPU_INT08U           *p_addr_hw,
                                                CPU_INT08U            addr_hw_len,
                                                NET_ERR              *p_err);


static  void        NetDev_ISR_Handler         (NET_IF               *p_if,
                                                NET_DEV_ISR_TYPE      type);


                                                                             /* ------- FNCT'S COMMON TO WIFI DEV'S -------- */
static  void        NetDev_MgmtDemux           (NET_IF               *p_if,
                                                NET_BUF              *p_buf,
                                                NET_ERR              *p_err);


static  CPU_INT32U  NetDev_MgmtExecutCmd       (NET_IF               *p_if,
                                                NET_IF_WIFI_CMD       cmd,
                                                NET_WIFI_MGR_CTX     *p_ctx,
                                                void                 *p_cmd_data,
                                                CPU_INT16U            cmd_data_len,
                                                CPU_INT08U           *p_buf_rtn,
                                                CPU_INT08U            buf_rtn_len_max,
                                                NET_ERR              *p_err);

static  CPU_INT32U  NetDev_MgmtProcessResp     (NET_IF               *p_if,
                                                NET_IF_WIFI_CMD       cmd,
                                                NET_WIFI_MGR_CTX     *p_ctx,
                                                CPU_INT08U           *p_buf_rxd,
                                                CPU_INT16U            buf_rxd_len,
                                                CPU_INT08U           *p_buf_rtn,
                                                CPU_INT16U            buf_rtn_len_max,
                                                NET_ERR              *p_err);

static  CPU_INT32U  NetDev_MgmtProcessScanResp (NET_IF               *p_if,
                                                CPU_INT08U           *p_frame,
                                                CPU_INT16U            frame_len,
                                                CPU_INT08U           *p_ap_buf,
                                                CPU_INT16U            buf_len);

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
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

const  NET_DEV_API_WIFI  NetDev_API_TemplateWiFiSpi = {                             /* Ether PIO dev API fnct ptrs : */
                                                       &NetDev_Init,                /*   Init/add                    */
                                                       &NetDev_Start,               /*   Start                       */
                                                       &NetDev_Stop,                /*   Stop                        */
                                                       &NetDev_Rx,                  /*   Rx                          */
                                                       &NetDev_Tx,                  /*   Tx                          */
                                                       &NetDev_AddrMulticastAdd,    /*   Multicast addr add          */
                                                       &NetDev_AddrMulticastRemove, /*   Multicast addr remove       */
                                                       &NetDev_ISR_Handler,         /*   ISR handler                 */
                                                       &NetDev_MgmtDemux,           /*   Demux mgmt frame rx'd       */
                                                       &NetDev_MgmtExecutCmd,       /*   Excute mgmt cmd             */
                                                       &NetDev_MgmtProcessResp      /*   Process mgmt resp rx'd      */
                                                      };


/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Initialize required clock sources
*                   (b) Initialize external interrupt controller
*                   (c) Initialize external GPIO controller
*                   (d) Initialize driver state variables
*                   (e) Initialize driver statistics & error counters
*                   (f) Allocate memory for device DMA descriptors
*                   (g) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
*                       (2) Disable device interrupts
*                       (3) Disable device receiver and transmitter
*                       (4) Other necessary device initialization
*
* Argument(s) : p_if    Pointer to an wireless network interface.
*               ----    Argument validated in NetIF_Add().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization        error.
*                           NET_DEV_ERR_   INVALID_CFG     General invalid configuration error.
*                           NET_DEV_ERR_MEM_ALLOC       Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Add() via 'p_dev_api->Init()'.
*
* Note(s)     : (2) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.  Micrium recommends defining and calling this NetBSP
*                   function even if no additional GPIO initialization is required.
*
*               (3) The application developper SHOULD define NetDev_SPI_Init within net_bsp.c
*                   in order to properly configure SPI registers for the device to operate
*                   properly.
*
*               (4) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
*                   in order to properly enable interrupts on an external or CPU integrated
*                   interrupt controller.  Interrupt sources that are specific to the DEVICE
*                   hardware MUST NOT be initialized from within NetDev_CfgIntCtrl() and
*                   SHOULD only be modified from within the device driver.
*
*                   (a) External interrupt sources are cleared within the NetBSP first level
*                       ISR handler either before or after the call to the device driver ISR
*                       handler function.  The device driver ISR handler function SHOULD only
*                       clear the device specific interrupts and NOT external or CPU interrupt
*                       controller interrupt sources.
*
*               (5) The application developer SHOULD define NetDev_IntCtrl() within net_bsp.c
*                   in order to properly enable  or disable interrupts on an external or CPU integrated
*                   interrupt controller.
*
*               (6) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (7) All device drivers that store instance specific data MUST declare all
*                   instance specific variables within the device data area defined above.
*
*               (8) Drivers SHOULD validate device configuration values and set *perr to
*                   NET_DEV_ERR_INVALID_CFG if unacceptible values have been specified. Fields
*                   of interest generally include, but are not limited to :
*
*                   (a) pdev_cfg->RxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (b) pdev_cfg->TxBufPoolType :
*
*                       (1) NET_IF_MEM_TYPE_MAIN
*                       (2) NET_IF_MEM_TYPE_DEDICATED
*
*                   (c) pdev_cfg->RxBufAlignOctets
*                   (d) pdev_cfg->TxBufAlignOctets
*                   (e) pdev_cfg->DataBusSizeNbrBits
*                   (f) pdev_cfg->SPI_ClkFreq
*                   (g) pdev_cfg->SPI_ClkPol
*
*                       (1) NET_DEV_SPI_CLK_POL_INACTIVE_LOW
*                       (2) NET_DEV_SPI_CLK_POL_INACTIVE_HIGH
*
*                   (h) pdev_cfg->SPI_ClkPhase
*
*                       (1) NET_DEV_SPI_CLK_PHASE_FALLING_EDGE
*                       (2) NET_DEV_SPI_CLK_PHASE_RASING_EDGE
*
*                   (i) pdev_cfg->SPI_XferUnitLen
*
*                       (1) NET_DEV_SPI_XFER_UNIT_LEN_8_BITS
*                       (2) NET_DEV_SPI_XFER_UNIT_LEN_16_BITS
*                       (3) NET_DEV_SPI_XFER_UNIT_LEN_32_BITS
*                       (4) NET_DEV_SPI_XFER_UNIT_LEN_64_BITS
*
*                   (j) pdev_cfg->SPI_XferShiftDir
*
*                       (1) NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB
*                       (2) NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_LSB
*
*               (9) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    NET_DEV_DATA          *p_dev_data;
    NET_BUF_SIZE           buf_rx_size_max;
    CPU_SIZE_T             reqd_octets;
    CPU_INT16U             buf_size_max;
    CPU_BOOLEAN            valid;
    LIB_ERR                lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_cfg = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;
    p_dev_bsp = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;


                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf ix offset.                           */
    if (p_dev_cfg->RxBufIxOffset != NET_DEV_RX_BUF_SPI_OFFSET_OCTETS) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    buf_rx_size_max = NetBuf_GetMaxSize(                 p_if->Nbr,
                                                         NET_TRANSACTION_RX,
                                        (NET_BUF       *)0,
                                                         NET_IF_IX_RX);
    if (buf_rx_size_max < NET_IF_802x_FRAME_MAX_SIZE) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate SPI freq.                                   */
    valid = DEF_CHK_VAL(p_dev_cfg->SPI_ClkFreq,
                        NET_DEV_SPI_CLK_FREQ_MIN,
                        NET_DEV_SPI_CLK_FREQ_MAX);
    if (valid != DEF_OK) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate SPI pol.                                    */
    if (p_dev_cfg->SPI_ClkPol != NET_DEV_SPI_CLK_POL_INACTIVE_HIGH) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate SPI phase.                                  */
    if (p_dev_cfg->SPI_ClkPol != NET_DEV_SPI_CLK_POL_INACTIVE_LOW) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate xfer unit len.                              */
    if (p_dev_cfg->SPI_XferUnitLen != NET_DEV_SPI_XFER_UNIT_LEN_8_BITS) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate xfer shift dir first.                       */
    if (p_dev_cfg->SPI_XferShiftDir != NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB) {
       *p_err = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    p_if->Dev_Data = Mem_HeapAlloc(sizeof(NET_DEV_DATA),
                                   p_dev_cfg->RxBufAlignOctets,
                                  &reqd_octets,
                                  &lib_err);
    if (p_if->Dev_Data == (void *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    p_dev_data               = (NET_DEV_DATA *)p_if->Dev_Data;

    buf_size_max             =  DEF_MAX(p_dev_cfg->RxBufLargeSize, p_dev_cfg->TxBufLargeSize);
    p_dev_data->GlobalBufPtr = (CPU_INT08U *)Mem_HeapAlloc(buf_size_max,
                                                           p_dev_cfg->RxBufAlignOctets,
                                                          &reqd_octets,
                                                          &lib_err);
    if (p_dev_data->GlobalBufPtr == (CPU_INT08U *)0) {
       *p_err = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

                                                                /* ------- INITIALIZE EXTERNAL GPIO CONTROLLER -------- */
    p_dev_bsp->CfgGPIO(p_if, p_err);                            /* Configure Wireless Controller GPIO (see Note #2).    */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }


                                                                /* ------------ INITIALIZE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_Init(p_if, p_err);                           /* Configure SPI Controller (see Note #3).              */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }


                                                                /* ----- INITIALIZE EXTERNAL INTERRUPT CONTROLLER ----- */
    p_dev_bsp->CfgIntCtrl(p_if, p_err);                         /* Configure ext int ctrl'r (see Note #4).              */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }


                                                                /* ------------ DISABLE EXTERNAL INTERRUPT ------------ */
    p_dev_bsp->IntCtrl(p_if, DEF_NO, p_err);                    /* Disable ext int ctrl'r (See Note #5)                 */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_INIT;
         return;
    }


                                                                /* ---- INITIALIZE ALL DEVICE DATA AREA VARIABLES ----- */
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    p_dev_data->StatRxPktCtr         = 0;
    p_dev_data->StatTxPktCtr         = 0;
#endif

#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    p_dev_data->ErrRxPktDiscardedCtr = 0;
    p_dev_data->ErrTxPktDiscardedCtr = 0;
#endif


   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Start()
*
* Description : (1) Start network interface hardware :
*
*                   (a) Initialize transmit semaphore count
*                   (b) Start      Wireless device
*                   (c) Initialize Wireless device
*                   (d) Validate   Wireless Firmware version
*
*                       (1) Boot Wireless device to upgrade firmware
*                       (2) Load Firmware
*                       (3) Reset Wireless device
*
*                   (e) Boot       Wireless firmware
*                   (f) Initialize Wireless firmware MAC layer
*                   (g) Configure hardware address
*
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_Start().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE                Wireless device successfully started.
*
*                                                           - RETURNED BY NetIF_AddrHW_SetHandler() : -
*                           NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                           NET_IF_ERR_NULL_FNCT            Invalid NULL function pointer.
*                           NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                           NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                           NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                           NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (2) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, some devices can queue multiple frames for
*                   transmission before blocking is required.  The default semaphore value is one.
*
*               (3) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_802x_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (4) The device hardware address is set from one of the data sources below. Each source
*                   is listed in the order of precedence.
*
*                   (a) Device Configuration Structure      Configure a valid HW address during
*                                                                compile time.
*
*                                                           Configure either "00:00:00:00:00:00" or
*                                                                an empty string, "", in order to
*                                                                configure the HW address using using
*                                                                method (b).
*
*                   (b) NetIF_802x_HW_AddrSet()             Call NetIF_802x_HW_AddrSet() if the HW
*                                                                address needs to be configured via
*                                                                run-time from a different data
*                                                                source. E.g. Non auto-loading
*                                                                memory such as I2C or SPI EEPROM
*                                                               (see Note #3).
*
*                   (c) Auto-Loading via EEPROM              If neither options a) or b) are used,
*                                                                the IF layer will use the HW address
*                                                                obtained from the network hardware
*                                                                address registers.
*
*               (5) More than one SPI device could share the same SPI controller, thus we MUST protect
*                   the access to the SPI controller and these step MUST be followed:
*
*                   (a) Acquire SPI register lock.
*
*                       (1) If no other device share the same SPI controller, it is NOT required to
*                           implement any type of ressource lock.
*
*                   (b) Enable the Device Chip Select.
*
*                       (1) The Chip Select of the device SHOULD be disabled between each device access
*
*                   (c) Set the SPI configuration of the device.
*
*                       (1) If no other device share the same SPI controller, SPI configuration can be
*                           done during the SPI initialization.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *p_if,
                            NET_ERR  *p_err)
{
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_WIFI_MGR_API      *p_mgr_api;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    CPU_INT08U             hw_addr[NET_IF_802x_ADDR_SIZE];
    CPU_INT08U             firmware_version;
    CPU_INT08U             hw_addr_len;
    CPU_INT16U             len;
    CPU_BOOLEAN            firmware_upgrade;
    CPU_BOOLEAN            hw_addr_cfg;
    NET_ERR                err;


                                                                /* ----------- OBTAIN REFERENCE TO MGR/BSP ------------ */
    p_mgr_api  = (NET_WIFI_MGR_API     *)p_if->Ext_API;         /* Obtain ptr to the mgr api struct.                    */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;


                                                                /* ------- INITIALIZE TRANSMIT SEMAPHORE COUNT -------- */
    NetIF_DevCfgTxRdySignal(p_if,                               /* See Note #2.                                         */
                            1,
                            p_err);
    if (*p_err != NET_IF_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

                                                                /* -------------- START WIRELESS DEVICE --------------- */
    p_dev_bsp->Start(p_if, p_err);                              /* Power up.                                            */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

    p_dev_bsp->IntCtrl(p_if, DEF_YES, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }


                                                                /* ------------ INITIALIZE WIRELESS DEVICE ------------ */
    /* TODO Insert code to intialize the SPI.                                                                           */
    /*                                                                                                                  */
    /* Ex:                                                                                                              */
                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
                                                                /* See Note #5a.                                        */
    /*p_dev_bsp->SPI_Lock(p_if, p_err);                                                                                 */
    /*if (*p_err != NET_DEV_ERR_NONE) {                                                                                 */
    /*    *p_err  = NET_DEV_ERR_RX_BUSY;                                                                                */
    /*     return;                                                                                                      */
    /*}                                                                                                                 */
    /*                                                                                                                  */
    /*                                                                                                                  */
                                                                /* -------------- ENABLE SPI CHIP SELECT -------------- */
                                                                /* See Note #5b.                                        */
    /*p_dev_bsp->SPI_ChipSelEn(p_if, p_err);                                                                            */
    /*if (*p_err != NET_DEV_ERR_NONE) {                                                                                 */
    /*     p_dev_bsp->SPI_Unlock(p_if);                                                                                 */
    /*    *p_err  = NET_DEV_ERR_RX;                                                                                     */
    /*     return;                                                                                                      */
    /*}                                                                                                                 */
    /*                                                                                                                  */
    /*                                                                                                                  */
                                                                /* ------------- CONFIGURE SPI CONTROLLER ------------- */
                                                                /* See Note #5c.                                        */
    /*p_dev_bsp->SPI_SetCfg(p_if,                                                                                       */
    /*                      p_dev_cfg->SPI_ClkFreq,                                                                     */
    /*                      p_dev_cfg->SPI_ClkPol,                                                                      */
    /*                      p_dev_cfg->SPI_ClkPhase,                                                                    */
    /*                      p_dev_cfg->SPI_XferUnitLen,                                                                 */
    /*                      p_dev_cfg->SPI_XferShiftDir,                                                                */
    /*                      p_err);                                                                                     */
    /*if (*p_err != NET_DEV_ERR_NONE) {                                                                                 */
    /*     p_dev_bsp->SPI_ChipSelDis(p_if);                                                                             */
    /*     p_dev_bsp->SPI_Unlock(p_if);                                                                                 */
    /*    *p_err  = NET_DEV_ERR_FAULT;                                                                                  */
    /*     return;                                                                                                      */
    /*}                                                                                                                 */
    /* wr_buf[0] = 0x00;                                                                                                */
    /* ...                                                                                                              */
    /*p_dev_bsp->SPI_WrRd(p_if,                                                                                         */
    /*                   &wr_buf,                                                                                       */
    /*                   &rd_buf,                                                                                       */
    /*                    len,                                                                                          */
    /*                    p_err);                                                                                       */
    /*if (*p_err != NET_DEV_ERR_NONE) {                                                                                 */
    /*    *p_err  = NET_DEV_ERR_FAULT;                                                                                  */
    /*     return;                                                                                                      */
    /*}                                                                                                                 */
    /*                                                                                                                  */
    /* if (rd_buf[2] != 0x52) {                                                                                         */
    /*    *p_err  = NET_DEV_ERR_FAULT;                                                                                  */
    /*     return;                                                                                                      */
    /*}                                                                                                                 */


                                                                /* -------- VALIDATE WIRELESS FIRMWARE VERSION -------- */
    p_mgr_api->Mgmt( p_if,
                     NET_DEV_MGMT_GET_FIRMWARE_VERSION,
                    &firmware_version,
                     sizeof (firmware_version),
                     0,
                     0,
                     p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

    if (firmware_version != NET_DEV_FIRMWARE_VERSION) {
        firmware_upgrade = DEF_YES;
    } else {
        firmware_upgrade = DEF_NO;
    }




    if (firmware_upgrade == DEF_YES) {
                                                                /* -------------- BOOT UPGRADE FIRMWARE --------------- */
        p_mgr_api->Mgmt(p_if,
                        NET_DEV_MGMT_BOOT_UPGRADE,
                        0,
                        0,
                        0,
                        0,
                        p_err);
        if (*p_err != NET_WIFI_MGR_ERR_NONE) {
            *p_err  = NET_DEV_ERR_FAULT;
             return;
        }
                                                                /* ---------------- LOAD NEW FIRMWARE ----------------- */
        p_mgr_api->Mgmt(              p_if,
                                      NET_DEV_MGMT_LOAD_FIRMWARE,
                        (CPU_INT08U *)0,
                                      0,
                        (CPU_INT08U *)0,
                                      0,
                                      p_err);
        if (*p_err != NET_WIFI_MGR_ERR_NONE) {
            *p_err  = NET_DEV_ERR_FAULT;
             return;
        }


                                                                /* ------------------- RESET DEVICE ------------------- */
        p_dev_bsp->Stop(p_if, p_err);                           /* Power down.                                          */
        if (*p_err != NET_DEV_ERR_NONE) {
            *p_err  = NET_DEV_ERR_FAULT;
             return;
        }

        p_dev_bsp->Start(p_if, p_err);                          /* Power up.                                            */
        if (*p_err != NET_DEV_ERR_NONE) {
            *p_err  = NET_DEV_ERR_FAULT;
             return;
        }
    }

                                                                /* ------------------ BOOT FIRMWARE ------------------- */
    p_mgr_api->Mgmt(              p_if,                         /* Boot firmware.                                       */
                                  NET_DEV_MGMT_BOOT_FIRMWARE,
                    (CPU_INT08U *)0,
                                  0,
                    (CPU_INT08U *)0,
                                  0,
                                  p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }


                                                                /* ------------- INIT FIRMWARE MAC LAYER -------------- */
    p_mgr_api->Mgmt(              p_if,
                                  NET_DEV_MGMT_INIT_MAC,
                    (CPU_INT08U *)0,
                                  0,
                    (CPU_INT08U *)0,
                                  0,
                                  p_err);
    if (*p_err != NET_WIFI_MGR_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }


                                                                /* ------------ CONFIGURE HARDWARE ADDRESS ------------ */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #3 & #4.                                   */

    NetASCII_Str_to_MAC(p_dev_cfg->HW_AddrStr,                  /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #4a).                                  */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) p_if->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors,  configure device HW MAC address.      */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Get copy of configured IF layer HW MAC address, ...  */
                                                                /* ... if any (see Note #4b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(p_if->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {                           /* Check if the IF HW address has been user configured. */
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(p_if->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* If NOT valid, attempt to use automatically ...       */
                                                                /* ... loaded HW MAC address (see Note #4c).            */
            len = sizeof(hw_addr);
            p_mgr_api->Mgmt(               p_if,
                                           NET_DEV_MGMT_GET_HW_ADDR,
                            (CPU_INT08U *) hw_addr,
                                           len,
                            (CPU_INT08U *) 0,
                                           0,
                                           p_err);
            if (*p_err != NET_WIFI_MGR_ERR_NONE) {
                *p_err  = NET_DEV_ERR_FAULT;
                 return;
            }

            NetIF_AddrHW_SetHandler(p_if->Nbr,                  /* Configure IF layer to use automatically-loaded ...   */
                                   &hw_addr[0],                 /* ... HW MAC address.                                  */
                                    sizeof(hw_addr),
                                    p_err);
            if (*p_err != NET_IF_ERR_NONE) {                    /* No valid HW MAC address configured, return error.    */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        p_mgr_api->Mgmt(               p_if,
                                       NET_DEV_MGMT_SET_HW_ADDR,
                        (CPU_INT08U *) 0,
                                       0,
                        (CPU_INT08U *) hw_addr,
                                       len,
                                       p_err);
        if (*p_err != NET_WIFI_MGR_ERR_NONE) {
            *p_err  = NET_DEV_ERR_FAULT;
             return;
        }
    }


   *p_err = NET_DEV_ERR_NONE;
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
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_Stop().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE    Device     successfully stopped.
*                           NET_DEV_ERR_FAULT   Device NOT successfully stopped.
* Return(s)   : none.
*
* Caller(s)   : NetIF_WiFi_IF_Stop() via 'p_dev_api->Stop()'.
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
    NET_DEV_DATA          *p_dev_data;
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_ERR                err;

                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;        /* Obtain ptr to dev data area.                         */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */


                                                                /* --------------- STOP WIRELESS DEVICE --------------- */
    p_dev_bsp->Stop(p_if, p_err);                               /* Power down.                                          */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

                                                                /* ------------------- FREE TX BUFS ------------------- */
    if (p_dev_data->TxBufCompPtr != (CPU_INT08U *)0) {         /* If NOT yet  tx'd, ...                                */
                                                                /* ... dealloc tx buf (see Note #2a1).                  */
        NetIF_TxDeallocTaskPost((CPU_INT08U *)p_dev_data->TxBufCompPtr, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        p_dev_data->TxBufCompPtr = (CPU_INT08U *)0;
    }


   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*
*                   (a) Acquire   SPI Lock
*                   (b) Enable    SPI Chip Select
*                   (c) Configure SPI Controller
*                   (d) Determine what caused the interrupt
*                   (e) Obtain pointer to data area to replace existing data area
*                   (f) Read data from the SPI
*                   (g) Set packet type
*                   (h) Set return values, pointer to received data area and size
*                   (i) Increment counters.
*                   (j) Disable SPI Chip Select
*                   (k) Release SPI lock
*
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_RxHandler().
*
*               p_data  Pointer to pointer to received data area. The received data
*                       area address should be returned to the stack by dereferencing
*                       p_data as *p_data = (address of receive data area).
*
*               p_size  Pointer to size. The number of bytes received should be returned
*                       to the stack by dereferencing size as *size = (number of bytes).
*               ------  Argument validated in NetIF_RxPkt().
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE                No Error
*                           NET_DEV_ERR_RX                  Generic Rx error.
*                           NET_DEV_ERR_RX_BUSY             Device is busy.
*                           NET_DEV_ERR_INVALID_SIZE        Invalid Rx frame size.
*                           NET_DEV_ERR_FAULT               Device  fault/failure.
*
*                                                           ------ RETURNED BY NetBuf_GetDataPtr() : ------
*                           NET_BUF_ERR_NONE                Network buffer data area successfully allocated
*                                                               & initialized.
*                           NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                           NET_BUF_ERR_INVALID_IX          Invalid index.
*                           NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                               maximum network buffer data area size
*                                                               available.
*                           NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                               buffer's data area.
*                           NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt() via 'pdev_api->Rx()'.
*
* Note(s)     : (2) More than one SPI device could share the same SPI controller, thus we MUST protect
*                   the access to the SPI controller and these step MUST be followed:
*
*                   (a) Acquire SPI register lock.
*
*                       (1) If no other device are shared the same SPI controller, it's not required to
*                           implement any type of ressource lock.
*
*                   (b) Enable the Device Chip Select.
*
*                       (1) The Chip Select of the device SHOULD be disbaled between each device access
*
*                   (c) Set the SPI configuration of the device.
*
*                       (1) If no other device are shared the same SPI controller, SPI configuration can be
*                           done during the SPI initialization.
*
*               (3) If a receive error occurs, the function SHOULD return 0 for the size, a
*                   NULL pointer to the data area AND an error code equal to NET_DEV_ERR_RX.
*                   Some devices may require that driver instruct the hardware to drop the
*                   frame if it has been commited to internal device memory.
*
*                   (a) If a new data area is unavailable, the driver MUST instruct hardware
*                       to discard the frame.
*
*               (4) Reading data from the device hardware may occur in various sized reads :
*
*                   (a) Device drivers that require read sizes equivalent to the size of the
*                       device data bus MAY examine pdev_cfg->DataBusSizeNbrBits in order to
*                       determine the number of required data reads.
*
*                   (b) Devices drivers that require read sizes equivalent to the size of the
*                       Rx FIFO width SHOULD use the known FIFO width to determine the number
*                       of required data reads.
*
*                   (c) It may be necessary to round the number of data reads up, OR perform the
*                       last data read outside of the loop.
*
*               (5) A pointer set equal to pbuf_new and sized according to the required data
*                   read size determined in (4) should be used to read data from the device
*                   into the receive buffer.
*
*               (6) Some devices may interrupt only ONCE for a recieved frame.  The driver MAY need
*                   check if additional frames have been received while processing the current
*                   received frame.  If additional frames have been received, the driver MAY need
*                   to signal the receive task before exiting NetDev_Rx().
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *p_if,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *p_size,
                         NET_ERR      *p_err)
{
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    NET_DEV_DATA          *p_dev_data;
    CPU_INT08U             rd_buf[4];
    CPU_INT08U            *p_buf;
    CPU_INT08U             int_type;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;         /* Obtain ptr to the bsp api struct.                    */
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;         /* Obtain ptr to dev cfg area.                          */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;        /* Obtain ptr to dev data area.                         */


                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Lock(p_if, p_err);                           /* See Note #2a.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_RX_BUSY;
         return;
    }


                                                                /* -------------- ENABLE SPI CHIP SELECT -------------- */
    p_dev_bsp->SPI_ChipSelEn(p_if, p_err);                      /* See Note #2b.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err  = NET_DEV_ERR_RX;
         return;
    }


                                                                /* ------------- CONFIGURE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_SetCfg(p_if,                                 /* See Note #2c.                                        */
                          p_dev_cfg->SPI_ClkFreq,
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_ChipSelDis(p_if);
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err  = NET_DEV_ERR_FAULT;
         return;
    }

   (void)&p_buf;
                                                                /* ------------- READ INTERRUPT REGISTER -------------- */
    /* TODO Insert code to read the interrupt register.                                                                 */
    /* Ex:                                                                                                              */
    /* wr_buf[0] = 0x00;                                                                                                */
    /* ...                                                                                                              */
    /*p_dev_bsp->SPI_WrRd(p_if,                                                                                         */
    /*                   &wr_buf,                                                                                       */
    /*                   &rd_buf,                                                                                       */
    /*                    len,                                                                                          */
    /*                    p_err);                                                                                       */


    int_type = rd_buf[3];
                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
    if ((int_type & NET_DEV_INT_RX_STATUS_ERR_MSK) > 0) {
        *p_err = NET_DEV_ERR_RX;
    }


    if ((int_type & NET_DEV_INT_TYPE_MASK_DATA) > 0) {
                                                                /* ----------- OBTAIN PTR TO NEW DATA AREA ------------ */
        p_buf     = NetBuf_GetDataPtr(p_if,
                                      NET_TRANSACTION_RX,
                                      NET_IF_ETHER_FRAME_MAX_SIZE,
                                      NET_IF_IX_RX,
                                      0,
                                      0,
                                      0,
                                      p_err);
        if (*p_err != NET_BUF_ERR_NONE) {                       /* See Note #3.                                         */
            *p_size = 0;
            *p_data = 0;
             p_dev_bsp->SPI_ChipSelDis(p_if);
             p_dev_bsp->SPI_Unlock(p_if);
             return;
        }

        /* TODO Insert code to get/read data packet or management response.                                             */
        /* Ex:                                                                                                          */
        /* wr_buf[0] = 0x00;                                                                                            */
        /* ...                                                                                                          */
        /*p_dev_bsp->SPI_WrRd(p_if,                                                                                     */
        /*                   &wr_buf,                                                                                   */
        /*                    p_buf,                                                                                    */
        /*                    len,                                                                                      */
        /*                    p_err);                                                                                   */
        /* int_type = rd_buf[3];                                                                                        */


                                                                /* ------------ ANALYSE & SET PACKET TYPE ------------- */
        /* TODO Insert code to analyse and set the packet type.                                                         */
        /* Ex:                                                                                                          */
        /* type = rd_buf[8];                                                                                            */
        /* if (type == NET_DEV_MGMT_RESP) {                                                                             */
        /*    *p_buf = NET_IF_WIFI_MGMT_FRAME;                                                                          */
        /* } else {                                                                                                     */
        /*    *p_buf = NET_IF_WIFI_DATA_PKT;                                                                            */
        /* }                                                                                                            */

    } else {
        NET_CTR_ERR_INC(p_dev_data->ErrRxPktDiscardedCtr);
       *p_err = NET_DEV_ERR_RX;
    }

   (void)&p_dev_data;

    p_dev_bsp->SPI_ChipSelDis(p_if);
    p_dev_bsp->SPI_Unlock(p_if);
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Acquire   SPI Lock
*                   (b) Enable    SPI Chip Select
*                   (c) Configure SPI Controller
*                   (d) Write data to the device to transmit
*                   (e) Increment counters.
*                   (f) Disable SPI Chip Select
*                   (g) Release SPI lock
*
* Argument(s) : p_if    Pointer to a network interface.
*               ----    Argument validated in NetIF_TxHandler().
*
*               p_data  Pointer to data to transmit.
*               ------  Argument checked   in NetIF_Tx().
*
*               size    Size    of data to transmit.
*
*               p_err   Pointer to variable that will receive the return error code from this function :
*
*                           NET_DEV_ERR_NONE        No         error
*                           NET_DEV_ERR_TX          Generic Tx error.
*                           NET_DEV_ERR_TX_BUSY     No Tx descriptors available
*                           NET_DEV_ERR_FAULT       Device fault/failure.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_TxPkt() via 'pdev_api->Tx()'.
*
* Note(s)     : (2) More than one SPI device could share the same SPI controller, thus we MUST protect
*                   the access to the SPI controller and these step MUST be followed:
*
*                   (a) Acquire SPI register lock.
*
*                       (1) If no other device are shared the same SPI controller, it's not required to
*                           implement any type of ressource lock.
*
*                   (b) Enable the Device Chip Select.
*
*                       (1) The Chip Select of the device SHOULD be disbaled between each device access
*
*                   (c) Set the SPI configuration of the device.
*
*                       (1) If no other device are shared the same SPI controller, SPI configuration can be
*                           done during the SPI initialization.
*
*               (3) Software MUST track all transmit buffer addresses that are that are queued for
*                   transmission but have not received a transmit complete notification (interrupt).
*                   Once the frame has been transmitted, software must post the buffer address of
*                   the frame that has completed transmission to the transmit deallocation task.
*
*                   (a) If the device doesn't support transmit complete notification software must
*                       post the buffer address of the frame once the data is wrote on the device FIFO.
*
*               (4) Writing data to the device hardware may occur in various sized writes :
*
*                   (a) Devices drivers that require write sizes equivalent to the size of the
*                       Tx FIFO width SHOULD use the known FIFO width to determine the number
*                       of required data reads.
*
*                   (b) It may be necessary to round the number of data writes up, OR perform the
*                       last data write outside of the loop.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *p_if,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *p_err)
{
    NET_DEV_DATA             *p_dev_data;
    NET_DEV_BSP_WIFI_SPI     *p_dev_bsp;
    NET_DEV_CFG_WIFI         *p_dev_cfg;
    NET_ERR                   err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;


                                                                /* ----------------- ACQUIRE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Lock(p_if, p_err);                           /* See Note #2a.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err  = NET_DEV_ERR_TX_BUSY;
         return;
    }


                                                                /* -------------- ENABLE SPI CHIP SELECT -------------- */
    p_dev_bsp->SPI_ChipSelEn(p_if, p_err);                      /* See Note #2b.                                        */
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err  = NET_DEV_ERR_TX;
         return;
    }


                                                                /* ------------- CONFIGURE SPI CONTROLLER ------------- */
    p_dev_bsp->SPI_SetCfg(p_if,                                 /* See Note #2c.                                        */
                          p_dev_cfg->SPI_ClkFreq,
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_ChipSelDis(p_if);
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err = NET_DEV_ERR_TX;
         return;
    }


                                                                /* -------------------- SEND DATA --------------------- */
    /* TODO Insert code to write & tx data packet.                                                                      */
    /* Ex:                                                                                                              */
    /* wr_buf[0] = 0x01;                                                                                                */
    /* ...                                                                                                              */
    /*p_dev_bsp->SPI_WrRd(p_if,                                                                                         */
    /*                   &wr_buf,                                                                                       */
    /*                    p_dev_data-GlobalBuf,                                                                         */
    /*                    len,                                                                                          */
    /*                    p_err);                                                                                       */
    /*p_dev_bsp->SPI_WrRd(p_if,                                                                                         */
    /*                    p_data,                                                                                       */
    /*                    p_dev_data-GlobalBuf,                                                                         */
    /*                    size,                                                                                         */
    /*                    p_err);                                                                                       */


    p_dev_data->TxBufCompPtr = p_data;                          /* See Note #3.                                         */
    NetIF_TxDeallocTaskPost(p_data, &err);                   /* See Note #3a.                                        */
    NetIF_DevTxRdySignal(p_if);

   (void)&err;                                                  /* Prevent possible 'variable unused' warnings.         */
    p_dev_data->TxBufCompPtr = (CPU_INT08U *)0;


                                                                /* ------------- DISBALE SPI CHIP SELECT -------------- */
    p_dev_bsp->SPI_ChipSelDis(p_if);


                                                                /* ----------------- RELEASE SPI LOCK ----------------- */
    p_dev_bsp->SPI_Unlock(p_if);


   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_AddrMulticastAdd()
*
* Description : Configure hardware address filtering to accept specified hardware address.
*
* Argument(s) : p_if            Pointer to a network interface.
*               ----            Argument validated in NetIF_AddrMulticastAdd().
*
*               paddr_hw        Pointer to hardware address.
*               --------        Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_hw_len     Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully configured.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *p_if,
                                       CPU_INT08U  *p_addr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *p_err)
{
#ifdef NET_MULTICAST_PRESENT
#endif

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetDev_AddrMulticastRemove()
*
* Description : Configure hardware address filtering to reject specified hardware address.
*
* Argument(s) : p_if            Pointer to a network interface.
*               ----            Argument validated in NetIF_AddrHW_SetHandler().
*
*               p_addr_hw       Pointer to hardware address.
*               ---------       Argument checked   in NetIF_AddrHW_SetHandler().
*
*               addr_hw_len     Length  of hardware address.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully removed.
*
*                                                               - RETURNED BY NetUtil_32BitCRC_Calc() : -
*                               NET_UTIL_ERR_NULL_PTR           Argument 'pdata_buf' passed NULL pointer.
*                               NET_UTIL_ERR_NULL_SIZE          Argument 'len' passed equal to 0.
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_AddrMulticastAdd() via 'pdev_api->AddrMulticastRemove()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *p_if,
                                          CPU_INT08U  *p_addr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *p_err)
{
#ifdef NET_MULTICAST_PRESENT
#endif

   *p_err = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_ISR_Handler()
*
* Description : This function serves as the device Interrupt Service Routine Handler. This ISR
*               handler MUST service and clear all necessary and enabled interrupt events for
*               the device.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_ISR_Handler().
*
*               type    Network Interface defined argument representing the type of ISR in progress. Codes
*                       for Rx, Tx, Overrun, Jabber, etc... are defined within net_if.h and are passed
*                       into this function by the corresponding Net BSP ISR handler function. The Net
*                       BSP ISR handler function may be called by a specific ISR vector and therefore
*                       know which ISR type code to pass.  Otherwise, the Net BSP may pass
*                       NET_DEV_ISR_TYPE_UNKNOWN and the device driver MAY ignore the parameter when
*                       the ISR type can be deduced by reading an available interrupt status register.
*
*                       Type codes that are defined within net_if.c include but are not limited to :
*
*                           NET_DEV_ISR_TYPE_RX
*                           NET_DEV_ISR_TYPE_TX_COMPLETE
*                           NET_DEV_ISR_TYPE_UNKNOWN
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_802x_ISR_Handler() via 'p_dev_api->ISR_Handler()'.
*
* Note(s)     : (1) This function is called via function pointer from the context of an ISR.
*
*               (2) In the case of an interrupt occurring prior to Network Protocol Stack initialization,
*                   the device driver should ensure that the interrupt source is cleared in order
*                   to prevent the potential for an infinite interrupt loop during system initialization.
*
*               (3) Many devices generate only one interrupt event for several ready frames.
*
*                   (a) It is NOT recommended to read from the SPI controller in the ISR handler as the SPI
*                       can be shared with another peripheral. If the SPI lock has been acquired by another
*                       application/chip the entire application could be locked forever.
*
*                   (b) If the device support the transmit completed notification and it is NOT possible to know
*                       the interrupt type without reading on the device, we suggest to notify the receive task
*                       where the interrupt register is read. And in this case, NetDev_rx() should return a
*                       management frame, which will be send automaticcly to NetDev_Demux() and where the stack
*                       is called to dealloc the packet transmitted.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *p_if,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_DATA  *p_dev_data;
    NET_ERR        err;


    p_dev_data = (NET_DEV_DATA *)p_if->Dev_Data;

    switch (type) {
        case NET_DEV_ISR_TYPE_RX:
        case NET_DEV_ISR_TYPE_UNKNOWN:                          /* See Note #3.                                         */
             NetIF_RxTaskSignal(p_if->Nbr, &err);
             break;


        case NET_DEV_ISR_TYPE_TX_COMPLETE:
             NetIF_TxDeallocTaskPost(p_dev_data->TxBufCompPtr, &err);
             NetIF_DevTxRdySignal(p_if);
             p_dev_data->TxBufCompPtr = (CPU_INT08U *)0;
             break;


        default:
             break;
    }

    (void)&err;                                                  /* Prevent possible 'variable unused' warnings.         */
}


/*
*********************************************************************************************************
*                                         NetDev_MgmtDemux()
*
* Description : (1) This function should analyse the management frame received:
*
*                   (a) Apply some operations on the device
*                   (b) Call the stack to change the link state or
*                   (c) Signal the wireless manager when response is received (See Note #3)
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_RxHandler().
*
*               p_buf       Pointer to a network buffer that received a management frame.
*               -----       Argument checked   in NetIF_WiFi_Rx().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Management     successfully processed
*                               NET_DEV_ERR_RX      Management NOT successfully processed
*
* Caller(s)   : NetIF_WiFi_RxMgmtFrameHandler() via 'p_dev_api->DemuxMgmt()'.
*
* Return(s)   : none.
*
* Note(s)     : (2) (a) The network buffer MUST be freed by this functions when the buffer is only used by this function.
*
*                   (b) The network buffer MUST NOT be freed by this function when an error occured and is returned,
*                       the upper layer free the buffer and increment discarded management frame global counter.
*
*                   (c) The netowrk buffer MUST NOT be freed by this function when the wireless manager is signaled as
*                       the network buffer will be used and freed by the wireless manager.
*
*               (3) The wireless manager MUST be signaled only when the response received is for a management
*                   command previously sent.
*********************************************************************************************************
*/

static  void  NetDev_MgmtDemux (NET_IF   *p_if,
                                NET_BUF  *p_buf,
                                NET_ERR  *p_err)
{
    NET_DEV_DATA      *p_dev_data;
    NET_WIFI_MGR_API  *p_mgr_api;
    CPU_INT08U         type;
    CPU_BOOLEAN        signal;


    p_dev_data = (NET_DEV_DATA *) p_if->Dev_Data;
    type       =  p_buf->DataPtr[2];
    signal     =  DEF_NO;

    switch (p_dev_data->WaitResponseType) {
        case NET_DEV_MGMT_BOOT_FIRMWARE:
             if (type == NET_DEV_RESP_CARD_RDY) {
                 signal = DEF_YES;
             }
             break;


        case NET_DEV_MGMT_GET_HW_ADDR:
             if (type == NET_DEV_RESP_CTRL) {
                 signal = DEF_YES;
             }
             break;


        case NET_DEV_MGMT_INIT_MAC:
             if (type == NET_DEV_RESP_INIT) {
                 signal = DEF_YES;
             }
             break;


        case NET_DEV_MGMT_SET_PWR_SAVE:
             if (type == NET_DEV_RESP_WAKEUP) {
                 signal = DEF_YES;
             }
             break;


        case NET_IF_WIFI_CMD_SCAN:
             if (type == NET_DEV_RESP_SCAN) {
                 signal = DEF_YES;
             }
             break;


        case NET_IF_WIFI_CMD_JOIN:
             if (type == NET_DEV_RESP_JOIN) {
                 signal = DEF_YES;
             }
             break;


        case NET_DEV_MGMT_LOAD_FIRMWARE:
             if ((type == NET_DEV_RESP_TAIM1) |
                 (type == NET_DEV_RESP_TAIM2) |
                 (type == NET_DEV_RESP_TADM )) {
                  signal = DEF_YES;
             }
             break;


        case NET_DEV_MGMT_NONE:
        default:
             signal = DEF_NO;
             break;
    }

    if (signal == DEF_YES) {
        p_mgr_api = (NET_WIFI_MGR_API *)p_if->Ext_API;
        p_mgr_api->Signal(p_if, p_buf, p_err);
       *p_err = NET_DEV_ERR_NONE;
    } else {
       *p_err = NET_DEV_ERR_RX;
    }
}


/*
*********************************************************************************************************
*                                       NetDev_MgmtExecutCmd()
*
* Description : This function MUST initialize or continue a management command.
*
* Argument(s) : p_if                Pointer to a network interface.
*               ----                Argument validated in NetIF_IF_Add(),
*                                                         NetIF_IF_Start(),
*                                                         NetIF_RxHandler(),
*                                                         NetIF_WiFi_Scan(),
*                                                         NetIF_WiFi_Join(),
*                                                         NetIF_WiFi_Leave().
*
*               cmd                 Management command to be executed:
*
*                                       NET_IF_WIFI_CMD_SCAN
*                                       NET_IF_WIFI_CMD_JOIN
*                                       NET_IF_WIFI_CMD_LEAVE
*                                       NET_IF_IO_CTRL_LINK_STATE_GET
*                                       NET_IF_IO_CTRL_LINK_STATE_GET_INFO
*                                       NET_IF_IO_CTRL_LINK_STATE_UPDATE
*                                       Others management commands defined by this driver.
*
*               p_ctx               State machine context See Note #1.
*
*               p_cmd_data          Pointer to a buffer that contains data to be used by the driver to execute
*                                   the command.
*
*               cmd_data_len        Command data length.
*
*               p_buf_rtn           Pointer to buffer that will receive return data.
*
*               buf_rtn_len_max     Return maximum data length.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Management     successfully executed
*                               NET_DEV_ERR_FAULT   Management NOT successfully executed
*
* Caller(s)   : NetWiFiMgr_MgmtHandler() via 'p_dev_api->MgmtExecuteCmd()'.
*
* Return(s)   : Length of data wrote in the return buffer in octet.
*
* Note(s)     : (1) The state machine context is used by the wireless manager to know what it MUST do after
*                   this call:
*
*                   (a) WaitResp            is used by the wireless manager to know if an asynchronous response is
*                                           required.
*
*                   (b) WaitRespTimeout_ms  is used by the wireless manager to know what is the timeout to receive
*                                           the response.
*
*                   (c) MgmtCompleted       is used by the wireless manager to know if the management process is
*                                           completed.
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_MgmtExecutCmd  (NET_IF            *p_if,
                                           NET_IF_WIFI_CMD    cmd,
                                           NET_WIFI_MGR_CTX  *p_ctx,
                                           void              *p_cmd_data,
                                           CPU_INT16U         cmd_data_len,
                                           CPU_INT08U        *p_buf_rtn,
                                           CPU_INT08U         buf_rtn_len_max,
                                           NET_ERR           *p_err)
{
    NET_DEV_DATA          *p_dev_data;
    NET_DEV_BSP_WIFI_SPI  *p_dev_bsp;
    NET_DEV_CFG_WIFI      *p_dev_cfg;
    CPU_INT08U             rd_buf[4];
    CPU_INT32U             rtn_len;



    p_dev_data = (NET_DEV_DATA         *)p_if->Dev_Data;
    p_dev_bsp  = (NET_DEV_BSP_WIFI_SPI *)p_if->Dev_BSP;
    p_dev_cfg  = (NET_DEV_CFG_WIFI     *)p_if->Dev_Cfg;


   *p_err                     = NET_DEV_ERR_NONE;
    p_ctx->WaitRespTimeout_ms = 0;


    p_dev_bsp->SPI_Lock(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        *p_err = NET_DEV_ERR_FAULT;
         return (0);
    }

    p_dev_bsp->SPI_ChipSelEn(p_if, p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err = NET_DEV_ERR_FAULT;
         return (0);
    }

    p_dev_bsp->SPI_SetCfg(p_if,
                          p_dev_cfg->SPI_ClkFreq,
                          p_dev_cfg->SPI_ClkPol,
                          p_dev_cfg->SPI_ClkPhase,
                          p_dev_cfg->SPI_XferUnitLen,
                          p_dev_cfg->SPI_XferShiftDir,
                          p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
         p_dev_bsp->SPI_Unlock(p_if);
        *p_err  = NET_DEV_ERR_RX;
         return (0);
    }

    rtn_len = rd_buf[3];


    switch (cmd) {
        case NET_DEV_MGMT_BOOT_FIRMWARE:

            /* TODO Insert code to boot the device.                                                                     */
            /* Ex:                                                                                                      */
            /* wr_buf[0] = 0x01;                                                                                        */
            /* ...                                                                                                      */
            /*p_dev_bsp->SPI_WrRd(p_if,                                                                                 */
            /*                   &wr_buf,                                                                               */
            /*                    p_dev_data-GlobalBuf,                                                                 */
            /*                    len,                                                                                  */
            /*                    p_err);                                                                               */

             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp              = DEF_NO;
                  p_ctx->MgmtCompleted         = DEF_YES;
                  p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                 *p_err                        = NET_DEV_ERR_FAULT;
                  break;
             }
             p_ctx->WaitResp              = DEF_YES;
             p_ctx->MgmtCompleted         = DEF_NO;
             p_dev_data->WaitResponseType = NET_DEV_MGMT_BOOT_FIRMWARE;
             p_dev_data->SubState         = NET_DEV_MGMT_NONE;
            *p_err                        = NET_DEV_ERR_NONE;
             break;


        case NET_DEV_MGMT_LOAD_FIRMWARE:
             switch (p_dev_data->SubState) {
                 case NET_DEV_MGMT_NONE:
                      /* TODO Insert code to load firmware part 1.                                                      */
                      /* Ex:                                                                                            */
                      /* wr_buf[0] = 0x01;                                                                              */
                      /* ...                                                                                            */
                      /*p_dev_bsp->SPI_WrRd(p_if,                                                                       */
                      /*                   &wr_buf,                                                                     */
                      /*                    p_dev_data-GlobalBuf,                                                       */
                      /*                    len,                                                                        */
                      /*                    p_err);                                                                     */
                      /*p_dev_bsp->SPI_WrRd(p_if,                                                                       */
                      /*                    p_firmware_part1,                                                           */
                      /*                    p_dev_data-GlobalBuf,                                                       */
                      /*                    len,                                                                        */
                      /*                    p_err);                                                                     */
                      if (*p_err != NET_DEV_ERR_NONE) {
                           p_ctx->WaitResp              = DEF_NO;
                           p_ctx->MgmtCompleted         = DEF_YES;
                           p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                           p_dev_data->SubState         = NET_DEV_MGMT_NONE;
                          *p_err                        = NET_DEV_ERR_FAULT;
                           break;
                      }
                      p_ctx->WaitResp              = DEF_YES;
                      p_ctx->MgmtCompleted         = DEF_NO;
                      p_dev_data->WaitResponseType = NET_DEV_MGMT_LOAD_FIRMWARE;
                      p_dev_data->SubState         = NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM1;
                     *p_err                        = NET_DEV_ERR_NONE;
                      break;


                 case NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM2:
                      /* TODO Insert code to load firmware part 2.                                                      */
                      /* Ex:                                                                                            */
                      /* wr_buf[0] = 0x01;                                                                              */
                      /* ...                                                                                            */
                      /*p_dev_bsp->SPI_WrRd(p_if,                                                                       */
                      /*                   &wr_buf,                                                                     */
                      /*                    p_dev_data-GlobalBuf,                                                       */
                      /*                    len,                                                                        */
                      /*                    p_err);                                                                     */
                      /*p_dev_bsp->SPI_WrRd(p_if,                                                                       */
                      /*                    p_firmware_part1,                                                           */
                      /*                    p_dev_data-GlobalBuf,                                                       */
                      /*                    len,                                                                        */
                      /*                    p_err);                                                                     */
                      if (*p_err != NET_DEV_ERR_NONE) {
                           p_ctx->WaitResp              = DEF_NO;
                           p_ctx->MgmtCompleted         = DEF_YES;
                           p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                          *p_err                        = NET_DEV_ERR_FAULT;
                           break;
                      }
                      p_ctx->WaitResp              = DEF_YES;
                      p_ctx->MgmtCompleted         = DEF_NO;
                      p_dev_data->WaitResponseType = NET_DEV_MGMT_LOAD_FIRMWARE;
                      p_dev_data->SubState         = NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM2;
                     *p_err                        = NET_DEV_ERR_NONE;
                      break;


                 case NET_DEV_MGMT_LOAD_FIRMWARE_FF_DATA:
                      /* TODO Insert code to load firmware part 3.                                                      */
                      /* Ex:                                                                                            */
                      /* wr_buf[0] = 0x01;                                                                              */
                      /* ...                                                                                            */
                      /*p_dev_bsp->SPI_WrRd(p_if,                                                                       */
                      /*                   &wr_buf,                                                                     */
                      /*                    p_dev_data-GlobalBuf,                                                       */
                      /*                    len,                                                                        */
                      /*                    p_err);                                                                     */
                      /*p_dev_bsp->SPI_WrRd(p_if,                                                                       */
                      /*                    p_firmware_part1,                                                           */
                      /*                    p_dev_data-GlobalBuf,                                                       */
                      /*                    len,                                                                        */
                      /*                    p_err);                                                                     */
                      if (*p_err != NET_DEV_ERR_NONE) {
                           p_ctx->WaitResp              = DEF_NO;
                           p_ctx->MgmtCompleted         = DEF_YES;
                           p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                          *p_err                        = NET_DEV_ERR_FAULT;
                           break;
                      }
                      p_ctx->WaitResp              = DEF_YES;
                      p_ctx->MgmtCompleted         = DEF_NO;
                      p_dev_data->WaitResponseType = NET_DEV_MGMT_LOAD_FIRMWARE;
                      p_dev_data->SubState         = NET_DEV_MGMT_LOAD_FIRMWARE_FF_DATA;
                     *p_err                        = NET_DEV_ERR_NONE;
                      break;


                 default:
                     *p_err = NET_DEV_ERR_FAULT;
                      break;
             }
             break;


        case NET_DEV_MGMT_INIT_MAC:
             /* TODO Insert code to intialize mac layer.                                                                */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp              = DEF_NO;
                  p_ctx->MgmtCompleted         = DEF_YES;
                  p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                 *p_err                        = NET_DEV_ERR_FAULT;
                  break;
             }
             p_ctx->WaitResp              = DEF_YES;
             p_ctx->MgmtCompleted         = DEF_NO;
             p_dev_data->WaitResponseType = NET_DEV_MGMT_INIT_MAC;
            *p_err                        = NET_DEV_ERR_NONE;
             break;


        case NET_DEV_MGMT_GET_HW_ADDR:
             /* TODO Insert code to receive the mac address (asynchronous response).                                    */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp              = DEF_NO;
                  p_ctx->MgmtCompleted         = DEF_YES;
                  p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                 *p_err                        = NET_DEV_ERR_FAULT;
                  break;
             }

             p_ctx->WaitResp              = DEF_YES;
             p_ctx->MgmtCompleted         = DEF_NO;
             p_dev_data->WaitResponseType = NET_DEV_MGMT_GET_HW_ADDR;
            *p_err                        = NET_DEV_ERR_NONE;
             break;


        case NET_DEV_MGMT_GET_FIRMWARE_VERSION:
             /* TODO Insert code to receive the firmware version (asynchronous response).                               */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp              = DEF_NO;
                  p_ctx->MgmtCompleted         = DEF_YES;
                  p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                 *p_err                        = NET_DEV_ERR_FAULT;
                  break;
             }

             p_ctx->WaitResp              = DEF_YES;
             p_ctx->MgmtCompleted         = DEF_NO;
             p_dev_data->WaitResponseType = NET_DEV_MGMT_GET_HW_ADDR;
            *p_err                        = NET_DEV_ERR_NONE;
             break;


        case NET_IF_WIFI_CMD_SCAN:
             /* TODO Insert code to scan for available wireless network (asynchronous response).                        */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp              = DEF_NO;
                  p_ctx->MgmtCompleted         = DEF_YES;
                  p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                 *p_err                        = NET_DEV_ERR_FAULT;
                  break;
             }

             p_ctx->WaitResp              = DEF_YES;
             p_ctx->MgmtCompleted         = DEF_NO;
             p_dev_data->WaitResponseType = NET_IF_WIFI_CMD_SCAN;
            *p_err                        = NET_DEV_ERR_NONE;
             break;


        case NET_IF_WIFI_CMD_JOIN:
             /* TODO Insert code to join a wireless network (asynchronous response).                                    */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             if (*p_err != NET_DEV_ERR_NONE) {
                  p_ctx->WaitResp              = DEF_NO;
                  p_ctx->MgmtCompleted         = DEF_YES;
                  p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                 *p_err                        = NET_DEV_ERR_FAULT;
                  break;
             }

             p_ctx->WaitResp              = DEF_YES;
             p_ctx->MgmtCompleted         = DEF_NO;
             p_dev_data->WaitResponseType = NET_IF_WIFI_CMD_JOIN;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_GET:
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             /* TODO Insert code to get the link state.                                                                 */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             /* p_link_state = (CPU_BOOLEAN *)p_dev_data-GlobalBuf[2];                                                  */
             p_ctx->WaitResp      = DEF_NO;
             p_ctx->MgmtCompleted = DEF_YES;
            *p_err                = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
             /* TODO Insert code to get the link state.                                                                 */
             /* Ex:                                                                                                     */
             /* wr_buf[0] = 0x01;                                                                                       */
             /* ...                                                                                                     */
             /*p_dev_bsp->SPI_WrRd(p_if,                                                                                */
             /*                   &wr_buf,                                                                              */
             /*                    p_dev_data-GlobalBuf,                                                                */
             /*                    len,                                                                                 */
             /*                    p_err);                                                                              */
             /* p_link_state = (CPU_BOOLEAN *)p_cmd_data;                                                               */
             /* p_link_state = (CPU_BOOLEAN *)p_dev_data-GlobalBuf[2];                                                  */
             p_ctx->WaitResp      = DEF_NO;
             p_ctx->MgmtCompleted = DEF_YES;
            *p_err                = NET_DEV_ERR_NONE;
             break;


        default:
             p_ctx->WaitResp              = DEF_NO;
             p_ctx->MgmtCompleted         = DEF_YES;
             p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
            *p_err                        = NET_DEV_ERR_FAULT;
             break;
    }

    p_dev_bsp->SPI_ChipSelDis(p_if);
    p_dev_bsp->SPI_Unlock(p_if);

    return (rtn_len);
}


/*
*********************************************************************************************************
*                                      NetDev_MgmtProcessResp()
*
* Description : After that the wireless manager has received the response, this function is called to analyse,
*               set the state machine context and fill the return buffer.analyse
*
* Argument(s) : p_if                Pointer to a network interface.
*               ----                Argument validated in NetIF_IF_Add(),
*                                                         NetIF_IF_Start(),
*                                                         NetIF_RxHandler(),
*                                                         NetIF_WiFi_Scan(),
*                                                         NetIF_WiFi_Join(),
*                                                         NetIF_WiFi_Leave().
*
*               cmd                 Management command to be executed:
*
*                                       NET_IF_WIFI_CMD_SCAN
*                                       NET_IF_WIFI_CMD_JOIN
*                                       NET_IF_WIFI_CMD_LEAVE
*                                       NET_IF_IO_CTRL_LINK_STATE_GET
*                                       NET_IF_IO_CTRL_LINK_STATE_GET_INFO
*                                       NET_IF_IO_CTRL_LINK_STATE_UPDATE
*                                       Others management commands defined by this driver.
*
*               p_ctx               State machine context See Note #1.
*
*               p_buf_rxd           Pointer to a network buffer that contains the command data response.
*
*               buf_rxd_len         Length of the data response.
*
*               p_buf_rtn           Pointer to buffer that will receive return data.
*
*               buf_rtn_len_max     Return maximum data length.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE    Management response     successfully processed
*                               NET_DEV_ERR_FAULT   Management response NOT successfully processed
*
* Caller(s)   : NetWiFiMgr_MgmtHandler() via 'p_dev_api->MgmtProcessResp()'.
*
* Return(s)   : Length of data wrote in the return buffer in octet.
*
* Note(s)     : (1) The network buffer is always freed by the wireless manager, no matter the error returned.
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_MgmtProcessResp  (NET_IF            *p_if,
                                             NET_IF_WIFI_CMD    cmd,
                                             NET_WIFI_MGR_CTX  *p_ctx,
                                             CPU_INT08U        *p_buf_rxd,
                                             CPU_INT16U         buf_rxd_len,
                                             CPU_INT08U        *p_buf_rtn,
                                             CPU_INT16U         buf_rtn_len_max,
                                             NET_ERR           *p_err)
{
    CPU_INT08U         type;
    CPU_INT32U         rtn;
    CPU_INT08U        *p_frame;
    CPU_INT08U        *p_dst;
    void const        *p_src;
    CPU_INT16U         len;
    NET_DEV_DATA      *p_dev_data;
    NET_DEV_CFG_WIFI  *p_dev_cfg;


    p_dev_cfg  = (NET_DEV_CFG_WIFI *)p_if->Dev_Cfg;
    p_dev_data = (NET_DEV_DATA     *)p_if->Dev_Data;
    type        = p_buf_rxd[2];
    p_frame     = p_buf_rxd + p_dev_cfg->RxBufIxOffset;
   *p_err       = NET_DEV_ERR_NONE;
    rtn         = 0;

    switch (cmd) {
        case NET_DEV_MGMT_BOOT_FIRMWARE:
             if ((p_dev_data->WaitResponseType == NET_DEV_MGMT_BOOT_FIRMWARE) &&
                 (type                         == NET_DEV_RESP_CARD_RDY)) {
                  p_ctx->WaitResp      = DEF_NO;
                  p_ctx->MgmtCompleted = DEF_YES;
             } else {
                  p_ctx->WaitResp      = DEF_YES;
                  p_ctx->MgmtCompleted = DEF_NO;
             }
             break;


        case NET_DEV_MGMT_GET_HW_ADDR:
             if ((p_dev_data->WaitResponseType == NET_DEV_MGMT_GET_HW_ADDR) &&
                 (type                         == NET_DEV_RESP_CTRL)    ) {
                  p_dst = (CPU_INT08U *)p_buf_rtn;
                  p_src =  p_frame + 2;
                  Mem_Copy(p_dst, p_src, buf_rtn_len_max);
                  rtn = buf_rtn_len_max;
                  p_ctx->WaitResp      = DEF_NO;
                  p_ctx->MgmtCompleted = DEF_YES;
             } else {
                  p_ctx->WaitResp      = DEF_YES;
                  p_ctx->MgmtCompleted = DEF_NO;
             }
             break;


        case NET_DEV_MGMT_INIT_MAC:
             if ((p_dev_data->WaitResponseType == NET_DEV_MGMT_INIT_MAC) &&
                 (type                         == NET_DEV_RESP_INIT)    ) {
                  p_ctx->WaitResp      = DEF_NO;
                  p_ctx->MgmtCompleted = DEF_YES;
             } else {
                  p_ctx->WaitResp      = DEF_YES;
                  p_ctx->MgmtCompleted = DEF_NO;
             }
             break;


        case NET_IF_WIFI_CMD_SCAN:
             if ((p_dev_data->WaitResponseType == NET_IF_WIFI_CMD_SCAN) &&
                 (type                         == NET_DEV_RESP_SCAN)   ) {
                  len = buf_rxd_len - p_dev_cfg->RxBufIxOffset;
                  rtn = NetDev_MgmtProcessScanResp(p_if, p_frame, len, p_buf_rtn, buf_rtn_len_max);
                  p_ctx->WaitResp      = DEF_NO;
                  p_ctx->MgmtCompleted = DEF_YES;
             }
             break;

             break;


        case NET_IF_WIFI_CMD_JOIN:
             if ((p_dev_data->WaitResponseType == NET_IF_WIFI_CMD_JOIN) &&
                 (type                         == NET_DEV_RESP_JOIN)   ) {
                  p_ctx->WaitResp      = DEF_NO;
                  p_ctx->MgmtCompleted = DEF_YES;

                  if (p_frame[2] != DEF_CLR) {
                     *p_err = NET_DEV_ERR_FAULT;
                  }
             }
             break;


        case NET_DEV_MGMT_LOAD_FIRMWARE:
             switch (p_dev_data->SubState) {
                 case NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM1:
                      if (type == NET_DEV_RESP_TAIM1) {
                          p_dev_data->WaitResponseType = NET_DEV_MGMT_LOAD_FIRMWARE;
                          p_dev_data->SubState         = NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM2;
                          p_ctx->WaitResp              = DEF_NO;
                          p_ctx->MgmtCompleted         = DEF_NO;

                      } else {
                          p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                          p_dev_data->SubState         = NET_DEV_MGMT_NONE;
                          p_ctx->WaitResp              = DEF_NO;
                          p_ctx->MgmtCompleted         = DEF_YES;
                         *p_err                        = NET_DEV_ERR_FAULT;
                      }
                      break;


                 case NET_DEV_MGMT_LOAD_FIRMWARE_FF_TAIM2:
                      if (type == NET_DEV_RESP_TAIM2) {
                          p_dev_data->WaitResponseType = NET_DEV_MGMT_LOAD_FIRMWARE;
                          p_dev_data->SubState         = NET_DEV_MGMT_LOAD_FIRMWARE_FF_DATA;
                          p_ctx->WaitResp              = DEF_NO;
                          p_ctx->MgmtCompleted         = DEF_NO;

                      } else {
                          p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                          p_dev_data->SubState         = NET_DEV_MGMT_NONE;
                          p_ctx->WaitResp              = DEF_NO;
                          p_ctx->MgmtCompleted         = DEF_YES;
                         *p_err                        = NET_DEV_ERR_FAULT;
                      }
                      break;


                 case NET_DEV_MGMT_LOAD_FIRMWARE_FF_DATA:
                      p_dev_data->WaitResponseType = NET_DEV_MGMT_NONE;
                      p_dev_data->SubState         = NET_DEV_MGMT_NONE;
                      p_ctx->WaitResp              = DEF_NO;
                      p_ctx->MgmtCompleted         = DEF_YES;
                      if (type != NET_DEV_RESP_TADM) {
                         *p_err = NET_DEV_ERR_FAULT;
                      }
                      break;


                 default:
                     *p_err = NET_DEV_ERR_FAULT;
                      break;
             }
             break;


        default:
            *p_err = NET_DEV_ERR_FAULT;
             break;
    }

    return (rtn);
}


/*
*********************************************************************************************************
*                                    NetDev_MgmtProcessScanResp()
*
* Description : This function fill the application buffer by translating the scan response.
*
* Argument(s) : p_if        Pointer to a network interface.
*               ----        Argument validated in NetIF_IF_Add(),
*                                                 NetIF_IF_Start(),
*                                                 NetIF_RxHandler(),
*                                                 NetIF_WiFi_Scan(),
*                                                 NetIF_WiFi_Join(),
*                                                 NetIF_WiFi_Leave().
*
*               p_frame     Pointer to a wireless management frame that contains the scan response.
*
*               frame_len   Length of the scan response.
*
*               p_ap_buf    Pointer to buffer that will receive access point found.
*
*               buf_len     Length of the access point buffer.
*
* Caller(s)   : NetDev_MgmtProcessResp()
*
* Return(s)   : Length of data wrote in the return buffer in octet.
*
* Note(s)     : none.
*********************************************************************************************************
*/

CPU_INT32U  NetDev_MgmtProcessScanResp (NET_IF      *p_if,
                                        CPU_INT08U  *p_frame,
                                        CPU_INT16U   frame_len,
                                        CPU_INT08U  *p_ap_buf,
                                        CPU_INT16U   buf_len)
{
    CPU_INT08U               i;
    CPU_INT08U               ap_ctn;
    CPU_INT08U               ap_rtn_max;
    NET_IF_WIFI_AP          *p_ap;
    NET_DEV_MGMT_SCAN_AP    *p_scan_ap;
    NET_DEV_MGMT_SCAN_RESP  *p_scan_response;


    p_ap            = (NET_IF_WIFI_AP         *)p_ap_buf;
    p_scan_response = (NET_DEV_MGMT_SCAN_RESP *)p_frame;
    ap_rtn_max      =  buf_len / sizeof(NET_IF_WIFI_AP);
    ap_ctn          =  p_scan_response->ScanCnt;

    for (i = 0; i < ap_ctn - 1; i++) {
        p_scan_ap = (NET_DEV_MGMT_SCAN_AP *)&p_scan_response->APs[i];
        Str_Copy_N((CPU_CHAR *)&p_ap->SSID,
                   (CPU_CHAR *)&p_scan_ap->SSID,
                                NET_IF_WIFI_STR_LEN_MAX_SSID);
        p_ap->SignalStrength = p_scan_ap->RSSID;
        p_ap->Ch             = p_scan_ap->Ch;
        switch (p_scan_ap->SecMode) {
            case 1:
                 p_ap->SecurityType = NET_IF_WIFI_SECURITY_WPA;
                 break;


            case 2:
                 p_ap->SecurityType = NET_IF_WIFI_SECURITY_WPA2;
                 break;


            case 3:
                 p_ap->SecurityType = NET_IF_WIFI_SECURITY_WEP;
                 break;


            case 0:
            default:
                p_ap->SecurityType = NET_IF_WIFI_SECURITY_OPEN;
                break;
        }
        p_ap++;

        if (i == ap_rtn_max - 1) {
            break;
        }
    }

    if (i < ap_rtn_max - 1) {
        return (ap_ctn * sizeof(NET_IF_WIFI_AP));
    } else {
        return (ap_rtn_max);
    }
}

#endif /* NET_IF_WIFI_MODULE_EN */

