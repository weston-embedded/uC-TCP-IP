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
*                                              WIRELESS
*
* Filename : net_if_wifi.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Supports following network interface layers:
*
*                (a) Ethernet See 'net_if_802x.h Note #1a'
*
*                (b) IEEE 802 See 'net_if_802x.h Note #1b'
*
*            (2) Wireless implementation conforms to RFC #1122, Section 2.3.3, bullets (a) & (b), but
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
*            (3) Wireless implemtation doesn't supports wireless supplicant and/or IEEE 802.11. The
*                wireless module-hardware must provide the wireless suppliant and all relevant IEEE 802.11
*                supports.
*
*            (4) REQUIREs the following network protocol files in network directories :
*
*                (a) (1) Network Interface Layer
*                    (2) 802x    Interface layer
*
*                      Located in the following network directory
*
*                          \<Network Protocol Suite>\IF\
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) Wireless Interface module is included only if support for Wireless devices is configured
*               (see 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #2a2').
*
*           (2) The following Wireless-module-present configuration value MUST be pre-#define'd in
*               'net_cfg_net.h' PRIOR to all other network modules that require Ethernet configuration
*               (see 'net_cfg_net.h  NETWORK INTERFACE LAYER CONFIGURATION  Note #2a2') :
*
*                   NET_IF_WIFI_MODULE_EN
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_IF_WIFI_MODULE_PRESENT
#define  NET_IF_WIFI_MODULE_PRESENT



/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_if_802x.h"
#include  "../Source/net_cfg_net.h"
#include  "../Source/net_buf.h"

#ifdef  NET_IF_WIFI_MODULE_EN                              /* See Note #2.                                         */


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

#define  NET_IF_WIFI_BUF_RX_LEN_MIN     NET_IF_802x_BUF_RX_LEN_MIN
#define  NET_IF_WIFI_BUF_TX_LEN_MIN     NET_IF_802x_BUF_TX_LEN_MIN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                   DEVICE SPI CFG VALUE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT32U   NET_DEV_CFG_SPI_CLK_FREQ;


typedef  CPU_BOOLEAN  NET_DEV_CFG_SPI_CLK_POL;

#define  NET_DEV_SPI_CLK_POL_INACTIVE_LOW                  0u   /* The clk is low  when inactive.                         */
#define  NET_DEV_SPI_CLK_POL_INACTIVE_HIGH                 1u   /* The clk is high when inactive.                         */



typedef  CPU_BOOLEAN  NET_DEV_CFG_SPI_CLK_PHASE;

#define  NET_DEV_SPI_CLK_PHASE_FALLING_EDGE                0u   /*Data is 'read'    on the leading   edge of the clk & .. */
                                                                /* ...    'changed' on the following edge of the clk.     */
#define  NET_DEV_SPI_CLK_PHASE_RAISING_EDGE                1u   /*Data is 'changed' on the following edge of the clk & .. */
                                                                /* ...    'read'    on the leading   edge of the clk.     */


typedef  CPU_INT08U   NET_DEV_CFG_SPI_XFER_UNIT_LEN;

#define  NET_DEV_SPI_XFER_UNIT_LEN_8_BITS                  8u   /* Xfer unit len is  8 bits.                              */
#define  NET_DEV_SPI_XFER_UNIT_LEN_16_BITS                16u   /* Xfer unit len is 16 bits.                              */
#define  NET_DEV_SPI_XFER_UNIT_LEN_32_BITS                32u   /* Xfer unit len is 32 bits.                              */
#define  NET_DEV_SPI_XFER_UNIT_LEN_64_BITS                64u   /* Xfer unit len is 64 bits.                              */



typedef  CPU_BOOLEAN  NET_DEV_CFG_SPI_XFER_SHIFT_DIR;

#define  NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB              0u   /* Xfer MSB first.                                        */
#define  NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_LSB              1u   /* Xfer LSB first.                                        */



typedef  CPU_INT08U   NET_DEV_BAND;

#define  NET_DEV_BAND_NONE                                 0u
#define  NET_DEV_BAND_2_4_GHZ                              1u
#define  NET_DEV_BAND_5_0_GHZ                              2u
#define  NET_DEV_BAND_DUAL                                 3u


/*
*********************************************************************************************************
*                                    WIRELESS FRAME TYPE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_FRAME_TYPE;

#define  NET_IF_WIFI_DATA_PKT                              1u
#define  NET_IF_WIFI_MGMT_FRAME                            2u

#define  NET_IF_WIFI_CFG_RX_BUF_IX_OFFSET_MIN              1u           /* The rx buf must be prefixed with pkt type.   */


/*
*********************************************************************************************************
*                                       WIRELESS CMD DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_CMD;                                /* Define WiFi cmd.                             */

#define  NET_IF_WIFI_CMD_SCAN                              1u
#define  NET_IF_WIFI_CMD_JOIN                              2u
#define  NET_IF_WIFI_CMD_LEAVE                             3u
#define  NET_IF_WIFI_CMD_LINK_STATE_GET                    4u
#define  NET_IF_WIFI_CMD_LINK_STATE_GET_INFO               5u
#define  NET_IF_WIFI_CMD_LINK_STATE_UPDATE                 6u
#define  NET_IF_WIFI_CMD_CREATE                               7u
#define  NET_IF_WIFI_CMD_GET_PEER_INFO                     8u

typedef  CPU_BOOLEAN    NET_IF_WIFI_CMD_TYPE;

#define  NET_IF_WIFI_CMD_TYPE_ANALYSE_RESP                 0u
#define  NET_IF_WIFI_CMD_TYPE_EXECUTE_CMD                  1u



/*
*********************************************************************************************************
*                                  WIRELESS SECURITY TYPE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_SECURITY_TYPE;                      /* Define WiFi security type.                   */

#define  NET_IF_WIFI_SECURITY_OPEN                         1u
#define  NET_IF_WIFI_SECURITY_WEP                          2u
#define  NET_IF_WIFI_SECURITY_WPA                          3u
#define  NET_IF_WIFI_SECURITY_WPA2                         4u


/*
*********************************************************************************************************
*                                    WIRELESS CHANNEL DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_CH;

                                                                        /* Defines WiFi ch.                             */
#define  NET_IF_WIFI_CH_ALL                                0u
#define  NET_IF_WIFI_CH_1                                  1u
#define  NET_IF_WIFI_CH_2                                  2u
#define  NET_IF_WIFI_CH_3                                  3u
#define  NET_IF_WIFI_CH_4                                  4u
#define  NET_IF_WIFI_CH_5                                  5u
#define  NET_IF_WIFI_CH_6                                  6u
#define  NET_IF_WIFI_CH_7                                  7u
#define  NET_IF_WIFI_CH_8                                  8u
#define  NET_IF_WIFI_CH_9                                  9u
#define  NET_IF_WIFI_CH_10                                10u
#define  NET_IF_WIFI_CH_11                                11u
#define  NET_IF_WIFI_CH_12                                12u
#define  NET_IF_WIFI_CH_13                                13u
#define  NET_IF_WIFI_CH_14                                14u


#define  NET_IF_WIFI_CH_MIN             NET_IF_WIFI_CH_ALL
#define  NET_IF_WIFI_CH_MAX             NET_IF_WIFI_CH_14


/*
*********************************************************************************************************
*                                   WIRELESS NETWORK TYPE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_NET_TYPE;                           /* Define WiFi net type.                        */

#define  NET_IF_WIFI_NET_TYPE_UNKNOWN                      0u
#define  NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE               1u
#define  NET_IF_WIFI_NET_TYPE_ADHOC                        2u


/*
*********************************************************************************************************
*                                    WIRELESS DATA RATE DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_DATA_RATE;                          /* Define WiFi data rate.                       */

#define  NET_IF_WIFI_DATA_RATE_AUTO                        0u
#define  NET_IF_WIFI_DATA_RATE_1_MBPS                      1u
#define  NET_IF_WIFI_DATA_RATE_2_MBPS                      2u
#define  NET_IF_WIFI_DATA_RATE_5_5_MBPS                    5u
#define  NET_IF_WIFI_DATA_RATE_6_MBPS                      6u
#define  NET_IF_WIFI_DATA_RATE_9_MBPS                      9u
#define  NET_IF_WIFI_DATA_RATE_11_MBPS                    11u
#define  NET_IF_WIFI_DATA_RATE_12_MBPS                    12u
#define  NET_IF_WIFI_DATA_RATE_18_MBPS                    18u
#define  NET_IF_WIFI_DATA_RATE_24_MBPS                    24u
#define  NET_IF_WIFI_DATA_RATE_36_MBPS                    36u
#define  NET_IF_WIFI_DATA_RATE_48_MBPS                    48u
#define  NET_IF_WIFI_DATA_RATE_54_MBPS                    54u
#define  NET_IF_WIFI_DATA_RATE_MCS0                      100u
#define  NET_IF_WIFI_DATA_RATE_MCS1                      101u
#define  NET_IF_WIFI_DATA_RATE_MCS2                      102u
#define  NET_IF_WIFI_DATA_RATE_MCS3                      103u
#define  NET_IF_WIFI_DATA_RATE_MCS4                      104u
#define  NET_IF_WIFI_DATA_RATE_MCS5                      105u
#define  NET_IF_WIFI_DATA_RATE_MCS6                      106u
#define  NET_IF_WIFI_DATA_RATE_MCS7                      107u
#define  NET_IF_WIFI_DATA_RATE_MCS8                      108u
#define  NET_IF_WIFI_DATA_RATE_MCS9                      109u
#define  NET_IF_WIFI_DATA_RATE_MCS10                     110u
#define  NET_IF_WIFI_DATA_RATE_MCS11                     111u
#define  NET_IF_WIFI_DATA_RATE_MCS12                     112u
#define  NET_IF_WIFI_DATA_RATE_MCS13                     113u
#define  NET_IF_WIFI_DATA_RATE_MCS14                     114u
#define  NET_IF_WIFI_DATA_RATE_MCS15                     115u


/*
*********************************************************************************************************
*                                 WIRELESS POWER & STRENGHT DATA TYPE
*********************************************************************************************************
*/

typedef  CPU_INT08U     NET_IF_WIFI_PWR_LEVEL;                          /* Define WiFi pwr level.                       */
typedef  CPU_INT08U     NET_IF_WIFI_SIGNAL_STRENGTH;                    /* WiFi IF's signal strength.                   */

#define  NET_IF_WIFI_PWR_LEVEL_LO                          0u
#define  NET_IF_WIFI_PWR_LEVEL_MED                         1u
#define  NET_IF_WIFI_PWR_LEVEL_HI                          2u


/*
*********************************************************************************************************
*                                       WIRELESS SSID DATA TYPE
*********************************************************************************************************
*/

#define  NET_IF_WIFI_STR_LEN_MAX_SSID                     32u

typedef  struct  net_if_wifi_ssid {                                     /* Define WiFi SSID.                            */
    CPU_CHAR                     SSID[NET_IF_WIFI_STR_LEN_MAX_SSID];    /* WiFi SSID.                                   */
} NET_IF_WIFI_SSID;


/*
*********************************************************************************************************
*                                       WIRELESS PSK DATA TYPE
*********************************************************************************************************
*/

#define  NET_IF_WIFI_STR_LEN_MAX_PSK                      32u

typedef  struct  net_if_wifi_psk {
    CPU_CHAR                     PSK[NET_IF_WIFI_STR_LEN_MAX_PSK];      /* WiFi PSK.                                    */
} NET_IF_WIFI_PSK;

/*
*********************************************************************************************************
*                                       WIRELESS BSS MAC ADDR
*********************************************************************************************************
*/


typedef  struct  net_if_wifi_bssid {
    CPU_INT08U                   BSSID[NET_IF_802x_ADDR_SIZE];          /* WiFi BSSID                                   */
} NET_IF_WIFI_BSSID;


/*
*********************************************************************************************************
*                                       WIRELESS SCAN DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_if_wifi_scan {
    NET_IF_WIFI_SSID             SSID;                                  /* WiFi scan SSID.                              */
    NET_IF_WIFI_CH               Ch;                                    /* WiFi Scan Ch.                                */
} NET_IF_WIFI_SCAN;


/*
*********************************************************************************************************
*                                 WIRELESS APs SCAN RESULT DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_if_wifi_ap {
    NET_IF_WIFI_BSSID            BSSID;                                 /* WiFi AP SSID.                                */
    NET_IF_WIFI_SSID             SSID;                                  /* WiFi AP SSID.                                */
    NET_IF_WIFI_CH               Ch;                                    /* WiFi AP Ch.                                  */
    NET_IF_WIFI_NET_TYPE         NetType;                               /* Wifi AP net type.                            */
    NET_IF_WIFI_SECURITY_TYPE    SecurityType;                          /* WiFi AP security type.                       */
    NET_IF_WIFI_SIGNAL_STRENGTH  SignalStrength;                        /* WiFi AP Signal Strength.                     */
} NET_IF_WIFI_AP;



/*
*********************************************************************************************************
*                                     WIRELESS AP CONFIG DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_if_wifi_ap_cfg {
    NET_IF_WIFI_NET_TYPE         NetType;                               /* WiFi AP Config net type.                     */
    NET_IF_WIFI_DATA_RATE        DataRate;                              /* WiFi AP Config data rate.                    */
    NET_IF_WIFI_SECURITY_TYPE    SecurityType;                          /* WiFi AP Config security type.                */
    NET_IF_WIFI_PWR_LEVEL        PwrLevel;                              /* WiFi AP Config pwr level.                    */
    NET_IF_WIFI_SSID             SSID;                                  /* WiFi AP Config ssid.                         */
    NET_IF_WIFI_PSK              PSK;                                   /* WiFi AP Config psk.                          */
    NET_IF_WIFI_CH               Ch;                                    /* WiFi AP Config ch.                           */
} NET_IF_WIFI_AP_CFG;

/*
*********************************************************************************************************
*                                     WIRELESS PEER INFO DATA TYPE
*********************************************************************************************************
*/

typedef  struct  net_if_wifi_peer {
    CPU_CHAR                     HW_Addr[NET_IF_802x_ADDR_SIZE];        /* WiFi peer MAC addr.                          */
} NET_IF_WIFI_PEER;



/*
*********************************************************************************************************
*                                     WIRELESS DEVICE DATA TYPES
*
* Note(s) : (1) The Wireless interface configuration data type is a specific definition of a network
*               device configuration data type.  Each specific network device configuration data type
*               MUST define ALL generic network device configuration parameters, synchronized in both
*               the sequential order & data type of each parameter.
*
*               Thus, ANY modification to the sequential order or data types of configuration parameters
*               MUST be appropriately synchronized between the generic network device configuration data
*               type & the Ethernet interface configuration data type.
*
*               See also 'net_if.h  GENERIC NETWORK DEVICE CONFIGURATION DATA TYPE  Note #1'.
*********************************************************************************************************
*/

                                                        /* ----------------------- NET WIFI DEV CFG ----------------------- */
typedef  struct  net_dev_cfg_wifi {
    NET_IF_MEM_TYPE                 RxBufPoolType;      /* Rx buf mem pool type :                                           */
                                                        /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                        /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE                    RxBufLargeSize;     /* Size  of dev rx large buf data areas (in octets).                */
    NET_BUF_QTY                     RxBufLargeNbr;      /* Nbr   of dev rx large buf data areas.                            */
    NET_BUF_SIZE                    RxBufAlignOctets;   /* Align of dev rx       buf data areas (in octets).                */
    NET_BUF_SIZE                    RxBufIxOffset;      /* Offset from base ix to rx data into data area (in octets).       */


    NET_IF_MEM_TYPE                 TxBufPoolType;      /* Tx buf mem pool type :                                           */
                                                        /*   NET_IF_MEM_TYPE_MAIN        bufs alloc'd from main      mem    */
                                                        /*   NET_IF_MEM_TYPE_DEDICATED   bufs alloc'd from dedicated mem    */
    NET_BUF_SIZE                    TxBufLargeSize;     /* Size  of dev tx large buf data areas (in octets).                */
    NET_BUF_QTY                     TxBufLargeNbr;      /* Nbr   of dev tx large buf data areas.                            */
    NET_BUF_SIZE                    TxBufSmallSize;     /* Size  of dev tx small buf data areas (in octets).                */
    NET_BUF_QTY                     TxBufSmallNbr;      /* Nbr   of dev tx small buf data areas.                            */
    NET_BUF_SIZE                    TxBufAlignOctets;   /* Align of dev tx       buf data areas (in octets).                */
    NET_BUF_SIZE                    TxBufIxOffset;      /* Offset from base ix to tx data from data area (in octets).       */


    CPU_ADDR                        MemAddr;            /* Base addr of (WiFi dev's) dedicated mem, if avail.               */
    CPU_ADDR                        MemSize;            /* Size      of (WiFi dev's) dedicated mem, if avail.               */


    NET_DEV_CFG_FLAGS               Flags;              /* Opt'l bit flags.                                                 */

    NET_DEV_BAND                    Band;               /* Wireless Band to use by the device.                              */
                                                        /*   NET_DEV_2_4_GHZ        Wireless band is 2.4Ghz.                */
                                                        /*   NET_DEV_5_0_GHZ        Wireless band is 5.0Ghz.                */
                                                        /*   NET_DEV_DUAL           Wireless band is dual (2.4 & 5.0 Ghz).  */

    NET_DEV_CFG_SPI_CLK_FREQ        SPI_ClkFreq;        /* SPI Clk freq (in Hz).                                            */

    NET_DEV_CFG_SPI_CLK_POL         SPI_ClkPol;         /* SPI Clk pol:                                                     */
                                                        /*   NET_DEV_SPI_CLK_POL_ACTIVE_LOW      clk is low  when inactive. */
                                                        /*   NET_DEV_SPI_CLK_POL_ACTIVE_HIGH     clk is high when inactive. */

    NET_DEV_CFG_SPI_CLK_PHASE       SPI_ClkPhase;       /* SPI Clk phase:                                                   */
                                                        /*   NET_DEV_SPI_CLK_PHASE_FALLING_EDGE  Data availables on  ...    */
                                                        /*                                          ... failling edge.      */
                                                        /*   NET_DEV_SPI_CLK_PHASE_RASING_EDGE   Data availables on  ...    */
                                                        /*                                          ... rasing edge.        */

    NET_DEV_CFG_SPI_XFER_UNIT_LEN   SPI_XferUnitLen;    /* SPI xfer unit length:                                            */
                                                        /*   NET_DEV_SPI_XFER_UNIT_LEN_8_BITS    Unit len of  8 bits.       */
                                                        /*   NET_DEV_SPI_XFER_UNIT_LEN_16_BITS   Unit len of 16 bits.       */
                                                        /*   NET_DEV_SPI_XFER_UNIT_LEN_32_BITS   Unit len of 32 bits.       */
                                                        /*   NET_DEV_SPI_XFER_UNIT_LEN_64_BITS   Unit len of 64 bits.       */

    NET_DEV_CFG_SPI_XFER_SHIFT_DIR  SPI_XferShiftDir;   /* SPI xfer shift dir:                                              */
                                                        /*   NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_MSB   Xfer MSB first.         */
                                                        /*   NET_DEV_SPI_XFER_SHIFT_DIR_FIRST_LSB   Xfer LSB first.         */

    CPU_CHAR                        HW_AddrStr[NET_IF_802x_ADDR_SIZE_STR];  /*  WiFi IF's dev hw addr str.                  */

} NET_DEV_CFG_WIFI;




                                                    /* -------------------- NET WIFI DEV LINK STATE ------------------- */
typedef  struct net_dev_link_state_wifi {
    CPU_BOOLEAN             LinkState;              /* Link   state.                                                    */
    NET_IF_WIFI_AP          AP;                     /* Access Point.                                                    */
    NET_IF_WIFI_DATA_RATE   DataRate;               /* Link   spd.                                                      */
    NET_IF_WIFI_PWR_LEVEL   PwrLevel;               /* Power  Level.                                                    */
} NET_DEV_LINK_WIFI;


/*
*********************************************************************************************************
*                                   WIRELESS DEVICE API DATA TYPES
*
* Note(s) : (1) (a) The Wireless device application programming interface (API) data type is a specific
*                   network device API data type definition which MUST define ALL generic network device
*                   API functions, synchronized in both the sequential order of the functions & argument
*                   lists for each function.
*
*                   Thus, ANY modification to the sequential order or argument lists of the API functions
*                   MUST be appropriately synchronized between the generic network device API data type &
*                   the Wireless device API data type definition/instantiations.
*
*                   However, specific Wireless device API data type definitions/instantiations MAY include
*                   additional API functions after all generic Ethernet device API functions.
*
*               (b) ALL API functions MUST be defined with NO NULL functions for all specific Wireless
*                   device API instantiations.  Any specific Ethernet device API instantiation that does
*                   NOT require a specific API's functionality MUST define an empty API function.
*
*               See also 'net_if.h  GENERIC NETWORK DEVICE API DATA TYPE  Note #1'.
*********************************************************************************************************
*/

                                                                        /* ------------- NET WIFI DEV API ------------- */
                                                                        /* Net WiFi dev API fnct ptrs :                 */
typedef  struct  net_dev_api_if_wifi {
                                                                        /*   Init/add                                   */
    void  (*Init)               (NET_IF               *p_if,
                                 NET_ERR              *p_err);
                                                                        /*   Start                                      */
    void  (*Start)              (NET_IF               *p_if,
                                 NET_ERR              *p_err);
                                                                        /*   Stop                                       */
    void  (*Stop)               (NET_IF               *p_if,
                                 NET_ERR              *p_err);
                                                                        /*   Rx                                         */
    void  (*Rx)                 (NET_IF               *p_if,
                                 CPU_INT08U          **p_data,
                                 CPU_INT16U           *size,
                                 NET_ERR              *p_err);
                                                                        /*   Tx                                         */
    void  (*Tx)                 (NET_IF               *p_if,
                                 CPU_INT08U           *p_data,
                                 CPU_INT16U            size,
                                 NET_ERR              *p_err);
                                                                        /*   Multicast addr add                         */
    void  (*AddrMulticastAdd)   (NET_IF               *p_if,
                                 CPU_INT08U           *p_addr_hw,
                                 CPU_INT08U            addr_hw_len,
                                 NET_ERR              *p_err);
                                                                        /*   Multicast addr remove                      */
    void  (*AddrMulticastRemove)(NET_IF               *p_if,
                                 CPU_INT08U           *p_addr_hw,
                                 CPU_INT08U            addr_hw_len,
                                 NET_ERR              *p_err);
                                                                        /*   ISR handler                                */
    void  (*ISR_Handler)        (NET_IF               *p_if,
                                 NET_DEV_ISR_TYPE      type);

                                                                        /*   Demux Pkt Mgmt                             */
    void  (*MgmtDemux)          (NET_IF               *p_if,
                                 NET_BUF              *p_buf,
                                 NET_ERR              *p_err);
} NET_DEV_API_IF_WIFI;


                                                                                        /* ----- NET WIFI MGR API ----- */
                                                                                        /* Net WiFi mgr API fnct ptrs : */
typedef  struct  net_wifi_mgr_api {
                                                                                        /*   Init                       */
    void           (*Init)                     (       NET_IF                *p_if,
                                                       NET_ERR               *p_err);
                                                                                        /*   Start                      */
    void           (*Start)                    (       NET_IF                *p_if,
                                                       NET_ERR               *p_err);
                                                                                        /*   Stop                       */
    void           (*Stop)                     (       NET_IF                *p_if,
                                                       NET_ERR               *p_err);
                                                                                        /*   Scan                       */
    CPU_INT16U     (*AP_Scan)                  (       NET_IF                *p_if,
                                                       NET_IF_WIFI_AP        *p_buf_scan,
                                                       CPU_INT16U             buf_scan_len_max,
                                                const  NET_IF_WIFI_SSID      *p_ssid,
                                                       NET_IF_WIFI_CH         ch,
                                                       NET_ERR               *p_err);
                                                                                        /*   Join                       */
    void           (*AP_Join)                  (       NET_IF                *p_if,
                                                const  NET_IF_WIFI_AP_CFG    *p_ap_cfg,
                                                       NET_ERR               *p_err);
                                                                                        /*   Leave                      */
    void           (*AP_Leave)                 (       NET_IF                *p_if,
                                                       NET_ERR               *p_err);

                                                                                        /*   I/O Control                */
    void           (*IO_Ctrl)                     (    NET_IF                *p_if,
                                                       CPU_INT08U             opt,
                                                       void                  *p_data,
                                                       NET_ERR               *p_err);
                                                                                        /*   Mgmt handler               */
    CPU_INT32U     (*Mgmt)                     (       NET_IF                *p_if,
                                                       NET_IF_WIFI_CMD        cmd,
                                                       CPU_INT08U            *p_buf_cmd,
                                                       CPU_INT16U             buf_cmd_len,
                                                       CPU_INT08U            *p_buf_rtn,
                                                       CPU_INT16U             buf_rtn_len_max,
                                                       NET_ERR               *p_err);
                                                                                        /*   Mgr signal                 */
    void           (*Signal)                   (       NET_IF                *p_if,
                                                       NET_BUF               *p_buf,
                                                       NET_ERR               *p_err);
                                                                                        /*   Create                     */
    void           (*AP_Create)                (       NET_IF                *p_if,
                                                const  NET_IF_WIFI_AP_CFG    *p_cfg,
                                                       NET_ERR               *p_err);
                                                                                        /*   Get Peer Info              */
    CPU_INT16U     (*AP_GetPeerInfo)           (       NET_IF                *p_if,
                                                const  NET_IF_WIFI_PEER      *p_buf_peer,
                                                       CPU_INT16U             peer_info_len_max,
                                                       NET_ERR               *p_err);
} NET_WIFI_MGR_API;


/*
*********************************************************************************************************
*                              WIRELESS DEVICE BSP INTERFACE DATA TYPE
*
* Note(s) : (1) (a) The Wireless device board-support package (BSP) interface data type is a specific
*                   network device BSP interface data type definition which SHOULD define ALL generic
*                   network device BSP functions, synchronized in both the sequential order of the
*                   functions & argument lists for each function.
*
*                   Thus, ANY modification to the sequential order or argument lists of the BSP functions
*                   SHOULD be appropriately synchronized between the generic network device BSP interface
*                   data type & the Wireless device BSP interface data type definition/instantiations.
*
*                   However, specific Wireless device BSP interface data type definitions/instantiations
*                   MAY include additional BSP functions after all generic Wireless device BSP functions.
*
*               (b) A specific Wireless device BSP interface instantiation MAY define NULL functions for
*                   any (or all) BSP functions provided that the specific Ethernet device driver does NOT
*                   require those specific BSP function(s).
*********************************************************************************************************
*/

                                                                            /* ----------- NET WIFI DEV BSP ----------- */
                                                                            /* Net WiFi dev BSP fnct ptrs :             */
typedef  struct  net_dev_bsp_wifi_spi {
                                                                            /*   Start                                  */
    void        (*Start)            (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Stop                                       */
    void        (*Stop )            (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Cfg GPIO                                   */
    void        (*CfgGPIO)          (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Cfg ISR                                    */
    void        (*CfgIntCtrl)       (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Enable/Disable ISR                         */
    void        (*IntCtrl)          (NET_IF                          *p_if,
                                     CPU_BOOLEAN                      en,
                                     NET_ERR                         *p_err);
                                                                            /*   Cfg SPI                                    */
    void        (*SPI_Init)         (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Lock                                       */
    void        (*SPI_Lock)         (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Unlock                                     */
    void        (*SPI_Unlock)       (NET_IF                          *p_if);
                                                                            /*   Wr & Rd                                    */
    void        (*SPI_WrRd)         (NET_IF                          *p_if,
                                     CPU_INT08U                      *p_buf_wr,
                                     CPU_INT08U                      *p_buf_rd,
                                     CPU_INT16U                       len,
                                     NET_ERR                         *p_err);
                                                                            /*   Chip Select                                */
    void        (*SPI_ChipSelEn)    (NET_IF                          *p_if,
                                     NET_ERR                         *p_err);
                                                                            /*   Chip Unselect                              */
    void        (*SPI_ChipSelDis)   (NET_IF                          *p_if);
                                                                            /*   Set SPI Cfg                                */
    void        (*SPI_SetCfg)       (NET_IF                          *p_if,
                                     NET_DEV_CFG_SPI_CLK_FREQ         freq,
                                     NET_DEV_CFG_SPI_CLK_POL          pol,
                                     NET_DEV_CFG_SPI_CLK_PHASE        pha,
                                     NET_DEV_CFG_SPI_XFER_UNIT_LEN    xfer_unit_len,
                                     NET_DEV_CFG_SPI_XFER_SHIFT_DIR   xfer_shift_dir,
                                     NET_ERR                         *p_err);
} NET_DEV_BSP_WIFI_SPI;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

extern  const  NET_IF_API  NetIF_API_WiFi;                      /* Wireless IF API fnct ptr(s).                         */


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

CPU_INT16U  NetIF_WiFi_Scan       (       NET_IF_NBR                  if_nbr,
                                          NET_IF_WIFI_AP             *p_buf_scan,
                                          CPU_INT16U                  buf_scan_len_max,
                                   const  NET_IF_WIFI_SSID           *p_ssid,
                                          NET_IF_WIFI_CH              ch,
                                          NET_ERR                    *p_err);


void        NetIF_WiFi_Join       (       NET_IF_NBR                  if_nbr,
                                          NET_IF_WIFI_NET_TYPE        net_type,
                                          NET_IF_WIFI_DATA_RATE       data_rate,
                                          NET_IF_WIFI_SECURITY_TYPE   security_type,
                                          NET_IF_WIFI_PWR_LEVEL       pwr_level,
                                          NET_IF_WIFI_SSID            ssid,
                                          NET_IF_WIFI_PSK             psk,
                                          NET_ERR                    *p_err);

void        NetIF_WiFi_CreateAP   (       NET_IF_NBR                  if_nbr,
                                          NET_IF_WIFI_NET_TYPE        net_type,
                                          NET_IF_WIFI_DATA_RATE       data_rate,
                                          NET_IF_WIFI_SECURITY_TYPE   security_type,
                                          NET_IF_WIFI_PWR_LEVEL       pwr_level,
                                          NET_IF_WIFI_CH              ch,
                                          NET_IF_WIFI_SSID            ssid,
                                          NET_IF_WIFI_PSK             psk,
                                          NET_ERR                    *p_err);

void        NetIF_WiFi_Leave      (       NET_IF_NBR                  if_nbr,
                                          NET_ERR                    *p_err);

CPU_INT16U  NetIF_WiFi_GetPeerInfo(       NET_IF_NBR                  if_nbr,
                                          NET_IF_WIFI_PEER           *p_buf_peer,
                                          CPU_INT16U                  buf_peer_info_len_max,
                                          NET_ERR                    *p_err);


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void       NetIF_WiFi_Init      (       NET_ERR                    *p_err);



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
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#ifndef  NET_IF_CFG_WIFI_EN
#error  "NET_IF_CFG_WIFI_EN         not #define'd in 'net_cfg.h'"
#error  "                     [MUST be  DEF_DISABLED]           "
#error  "                     [     ||  DEF_ENABLED ]           "

#elif  ((NET_IF_CFG_WIFI_EN != DEF_DISABLED) && \
        (NET_IF_CFG_WIFI_EN != DEF_ENABLED ))
#error  "NET_IF_CFG_WIFI_EN   illegally #define'd in 'net_cfg.h'"
#error  "                     [MUST be  DEF_DISABLED]           "
#error  "                     [     ||  DEF_ENABLED ]           "
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif  /* NET_IF_WIFI_MODULE_EN        */
#endif  /* NET_IF_WIFI_MODULE_PRESENT   */

