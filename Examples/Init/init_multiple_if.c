/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                               EXAMPLE
*
*                                  MULTIPLE INTERFACE INITIALIZATION
*
* Filename : init_multiple_if.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This example shows how to initialize uC/TCP-IP:
*
*                (a) Initialize Stack tasks and objects
*
*                (b) Initialize an Ethernet Interface
*                (c) Start that Ethernet Interface
*                (d) Configure IP addresses of the Ethernet Interface
*
*                (e) Initialize an Wireless Interface
*                (f) Start that Wireless Interface
*                (g) Scan for Wireless networks available
*                (h) Analyze scan result
*                (i) Join a Wireless network
*                (j) Configure IP addresses of that Wireless Interface
*
*            (2) This example is based on template files so some modifications will be required. Insert the
*                appropriate project/board specific code to perform the stated actions wherever 'TODO'
*                comments are found.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <cpu_core.h>
#include  <lib_mem.h>

#include  <Source/net.h>
#include  <Source/net_ascii.h>
#include  <IF/net_if.h>
#include  <IF/net_if_wifi.h>

#ifdef NET_IPv4_MODULE_EN
#include  <IP/IPv4/net_ipv4.h>
#endif

#ifdef NET_IPv6_MODULE_EN
#include  <IP/IPv6/net_ipv6.h>
#endif

#include  <Cfg/Template/net_dev_cfg.h>                      /* TODO Device configuration header. See Note #1.           */

#include  <Dev/Ether/Template/net_dev_ether_template_dma.h> /* TODO Device driver header.        See Note #2.           */
#include  <Dev/WiFi/Template/net_dev_wifi_template_spi.h>   /* TODO Device driver header.        See Note #2.           */

#include  <Dev/Ether/PHY/Generic/net_phy.h>                 /* TODO PHY driver header.           See Note #3.           */
#include  <Dev/WiFi/Manager/Generic/net_wifi_mgr.h>

#include  <BSP/Template/net_bsp_ether.h>                    /* TODO BSP header.                  See Note #4.           */
#include  <BSP/Template/net_bsp_wifi.h>                     /* TODO BSP header.                  See Note #4.           */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef  NET_IPv6_MODULE_EN
static  void  App_AutoCfgResult(      NET_IF_NBR                 if_nbr,
                                const NET_IPv6_ADDR             *p_addr_local,
                                const NET_IPv6_ADDR             *p_addr_global,
                                      NET_IPv6_AUTO_CFG_STATUS   auto_cfg_result);
#endif


/*
*********************************************************************************************************
*                                     AppInit_TCPIP_MultipleIF()
*
* Description : Initialize uC/TCP-IP:
*
*                   (a) Initialize tasks and objects
*
*                   (b) Initialize an Ethernet Interface
*                   (c) Start the Ethernet Interface
*                   (d) Configure IP addresses of the Ethernet Interface
*
*                   (e) Initialize a Wireless Interface
*                   (f) Start the Wireless Interface
*                   (g) Configure IP addresses of the Wireless Interface
*                   (h) Scan for Wireless network available
*                   (i) Analyze scan results
*                   (j) Join a Wireless network
*
* Argument(s) : none.
*
* Return(s)   : DEF_OK,   initialization completed successfully.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
* Note(s)     : (1) Some prerequisite module initializations are required. The following modules must be initialized
*                   prior to starting the Network Protocol stacks initialization:
*
*                   (a) uC/CPU
*                   (b) uC/LIB Memory module
*
*               (2) Net_Init() is the Network Protocol stack's initialization function. It must only be called
*                   once and before any other Network functions.
*
*                   (a) This function takes the three TCP-IP internal task configuration structures as
*                       arguments (such as priority, stack size, etc.). By default these configuration
*                       structures are defined in net_cfg.c :
*
*                       NetRxTaskCfg            RX    task configuration
*                       NetTxDeallocTaskCfg     TX    task configuration
*                       NetTmrTaskCfg           Timer task configuration
*
*                   (b) We recommend you configure the Network Protocol Stack task priorities & Network application
*                       (such as a Web server) task priorities as follows:
*
*                           NetTxDeallocTaskCfg                         (highest priority)
*
*                           Network applications (HTTPs, FTP, DNS, etc.)
*
*                           NetTmrTaskCfg
*                           NetRxTaskCfg                                (lowest  priority)
*
*                       We recommend that the uC/TCP-IP Timer task and network interface Receive task be lower
*                       priority than almost all other application tasks; but we recommend that the network
*                       interface Transmit De-allocation task be higher priority than all application tasks that use
*                       uC/TCP-IP network services.
*
*                       However, better performance can be observed when the Network applications are set with the
*                       lowest priority. Some experimentation could be required to identify the best task priority
*                       configuration.
*
*               (3) NetIF_Add() is a network interface function responsible for initializing a Network Device driver.
*
*                   (a) NetIF_Add() returns the interface index number. The interface index number should start at '1',
*                       since the interface '0' is reserved for the loopback interface. The interface index number must
*                       be used when you want to access the interface using any Network interface API.
*
*                   (b) The first parameter is the address of the Network interface API. These API are provided by
*                       Micrium and are defined in file 'net_if_<type>.h'. It should be either:
*
*                           NetIF_API_Ether     Ethernet interface
*                           NetIF_API_WiFi      Wireless interface
*
*                   (c) The second parameter is the address of the device API function. The API should be defined in the
*                       Device driver header:
*
*                            $/uC-TCPIP/Dev/<if_type>/<controller>/net_dev_<controller>.h
*
*                   (d) The third parameter is the address of the device BSP data structure. Refer to the section
*                       'INCLUDE FILES - Note #4' of this file for more details.
*
*                   (e) The fourth parameter is the address of the device configuration data structure. Refer to the
*                       section 'INCLUDE FILES - Note #1' of this file for more details.
*
*                   (f) The fifth parameter is the address of the PHY API function. Refer to the section
*                       'INCLUDE FILES - Note #3' of this file for more details.
*
*                   (g) The sixth and last parameter is the address of the PHY configuration data structure. The PHY
*                       configuration should be located in net_dev_cfg.c/h.
*
*               (4) NetIF_Start() makes the network interface scan, join or create adhoc wireless network. This function must interact
*                   with the wireless module thus some interrupt should be generated from the wirless's interrupt pin when calling
*                   this function.
*
*               (5) NetASCII_Str_to_IP() converts the human readable address into a format required by the protocol stack.
*
*                   In this example the IP address used, 10.10.10.64, addresses out of the 10.10.10.1 network with a subnet mask
*                   of 255.255.255.0. To match different networks, the IP address, the subnet mask and the default gateway's IP
*                   address have to be customized.
*
*               (6) NetIPv4_CfgAddrAdd() configures the network IPv4 static parameters (IPv4 address, subnet mask and
*                   default gateway) required for the interface. More than one set of network parameters can be
*                   configured per interface. NetIPv4_CfgAddrAdd() can be repeated for as many network parameter
*                   sets as need configuring for an interface.
*
*                   IPv4 parameters can be added whenever as long as the interface was added (initialized) even if the interface
*                   is started or not.
*
*                   For Dynamic IPv4 configuration, uC/DHCPc is required
*
*               (7) NetIPv6_CfgAddrAdd() configures the network IPv6 static parameters (IPv6 address and prefix length)
*                   required for the interface. More than one set of network parameters can be configured per interface.
*                   NetIPv6_CfgAddrAdd() can be repeated for as many network parameter sets as need configuring for
*                   an interface.
*
*                   IPv6 parameters can be added whenever as long as the interface is added (initialized) even if the interface
*                   is started or not.
*
*                   For the moment dynamic IPv6 is not yet supported either by IPv6 Autoconfig or DHCPv6c.
*
*               (8) NetIF_WiFi_Scan() is used to scan for available Wireless Network available in the range.
*
*                   (a) The second parameter is a table of access point that will be receive access points found in the range.
*                       Obviously, the maximum number of access point that the table can store must be past to the function.
*
*                   (b) It's possible to scan for a specific hidden network by passing a string that contains the SSID of the
*                       hidden network. If the scan request is for all access point, you only have to pass a null pointer.
*
*                   (c) The fourth parameter is the wireless channel to scan on, it can be:
*
*                                       NET_IF_WIFI_CH_ALL
*                                       NET_IF_WIFI_CH_1
*                                       NET_IF_WIFI_CH_2
*                                       NET_IF_WIFI_CH_3
*                                       NET_IF_WIFI_CH_4
*                                       NET_IF_WIFI_CH_5
*                                       NET_IF_WIFI_CH_6
*                                       NET_IF_WIFI_CH_7
*                                       NET_IF_WIFI_CH_8
*                                       NET_IF_WIFI_CH_9
*                                       NET_IF_WIFI_CH_10
*                                       NET_IF_WIFI_CH_11
*                                       NET_IF_WIFI_CH_12
*                                       NET_IF_WIFI_CH_13
*                                       NET_IF_WIFI_CH_14
*
*               (9) NetIF_WiFi_Join() is used to join a wireless network. Note that the network must has been found during a scan
*                   previously. Once the wireless access point is join, it is possible to receive and transmit packet on the network.
*
*                   (a) The second parameter is the Network type it can be either:
*
*                           NET_IF_WIFI_NET_TYPE_INFRASTRUCTURE
*                           NET_IF_WIFI_NET_TYPE_ADHOC
*
*                       The scan function should return the network type as well.
*
*
*                   (b) The third parameter is the wireless date rate to configure:
*
*                           NET_IF_WIFI_DATA_RATE_AUTO
*                           NET_IF_WIFI_DATA_RATE_1_MBPS
*                           NET_IF_WIFI_DATA_RATE_2_MBPS
*                           NET_IF_WIFI_DATA_RATE_5_5_MBPS
*                           NET_IF_WIFI_DATA_RATE_6_MBPS
*                           NET_IF_WIFI_DATA_RATE_9_MBPS
*                           NET_IF_WIFI_DATA_RATE_11_MBPS
*                           NET_IF_WIFI_DATA_RATE_12_MBPS
*                           NET_IF_WIFI_DATA_RATE_18_MBPS
*                           NET_IF_WIFI_DATA_RATE_24_MBPS
*                           NET_IF_WIFI_DATA_RATE_36_MBPS
*                           NET_IF_WIFI_DATA_RATE_48_MBPS
*                           NET_IF_WIFI_DATA_RATE_54_MBPS
*                           NET_IF_WIFI_DATA_RATE_MCS0
*                           NET_IF_WIFI_DATA_RATE_MCS1
*                           NET_IF_WIFI_DATA_RATE_MCS2
*                           NET_IF_WIFI_DATA_RATE_MCS3
*                           NET_IF_WIFI_DATA_RATE_MCS4
*                           NET_IF_WIFI_DATA_RATE_MCS5
*                           NET_IF_WIFI_DATA_RATE_MCS6
*                           NET_IF_WIFI_DATA_RATE_MCS7
*                           NET_IF_WIFI_DATA_RATE_MCS8
*                           NET_IF_WIFI_DATA_RATE_MCS9
*                           NET_IF_WIFI_DATA_RATE_MCS10
*                           NET_IF_WIFI_DATA_RATE_MCS11
*                           NET_IF_WIFI_DATA_RATE_MCS12
*                           NET_IF_WIFI_DATA_RATE_MCS13
*                           NET_IF_WIFI_DATA_RATE_MCS14
*                           NET_IF_WIFI_DATA_RATE_MCS15
*
*                   (c) The fourth parameter is the wireless network's security type. It can be:
*
*                           NET_IF_WIFI_SECURITY_OPEN
*                           NET_IF_WIFI_SECURITY_WEP
*                           NET_IF_WIFI_SECURITY_WPA
*                           NET_IF_WIFI_SECURITY_WPA2
*
*                   (d) The fifth parameter is the wireless radio's power level. It can be:
*
*                           NET_IF_WIFI_PWR_LEVEL_LO
*                           NET_IF_WIFI_PWR_LEVEL_MED
*                           NET_IF_WIFI_PWR_LEVEL_HI
*
*                   (e) The sixth parameter is the access point's SSID to join.
*
*                   (f) The seventh parameter is the Pre shared key (PSK) of the access point's. If the security
*                       type of the access point is open, the PSK can set to 0.
*********************************************************************************************************
*/

CPU_BOOLEAN  AppInit_TCPIP_MultipleIF (void)
{
#ifdef NET_IPv4_MODULE_EN
    NET_IPv4_ADDR       addr_ipv4;
    NET_IPv4_ADDR       msk_ipv4;
    NET_IPv4_ADDR       gateway_ipv4;
#endif

#ifdef NET_IPv6_MODULE_EN
    CPU_BOOLEAN         cfg_result;
    NET_FLAGS           ipv6_flags;
    NET_IPv6_ADDR       addr_ipv6;
#endif

    NET_IF_NBR          if_nbr_ether;
    NET_IF_NBR          if_nbr_wifi;
    NET_IF_WIFI_AP      ap[10];
    NET_IF_WIFI_SSID   *p_ssid;
    NET_IF_WIFI_SSID    ssid;
    NET_IF_WIFI_PSK     psk;
    CPU_INT16U          ctn;
    CPU_INT16U          i;
    CPU_INT16S          result;
    CPU_BOOLEAN         found;
    NET_ERR             err_net;


                                                                /* -------- INITIALIZE NETWORK TASKS & OBJECTS -------- */
    err_net = Net_Init(&NetRxTaskCfg,
                       &NetTxDeallocTaskCfg,
                       &NetTmrTaskCfg);
    if (err_net != NET_ERR_NONE) {
        return (DEF_FAIL);
    }


                                                                /* -------------- ADD ETHERNET INTERFACE -------------- */
                                                                /* See Note #3.                                         */
    if_nbr_ether = NetIF_Add((void    *)&NetIF_API_Ether,             /* See Note #3b.                                  */
                             (void    *)&NetDev_API_TemplateEtherDMA, /* TODO Device driver API,    See Note #3c.       */
                             (void    *)&NetDev_BSP_BoardDev_Nbr,     /* TODO BSP API,              See Note #3d.       */
                             (void    *)&NetDev_Cfg_Ether_1,          /* TODO Device configuration, See Note #3e.       */
                             (void    *)&NetPhy_API_Generic,          /* TODO PHY driver API,       See Note #3f.       */
                             (void    *)&NetPhy_Cfg_Ether_1,          /* TODO PHY configuration,    See Note #3g.       */
                                        &err_net);
    if (err_net != NET_IF_ERR_NONE) {
        return (DEF_FAIL);
    }

                                                                /* ------------- START ETHERNET INTERFACE ------------- */
                                                                /* See Note #4.                                         */
    NetIF_Start(if_nbr_ether, &err_net);                        /* Makes the interface ready to receive and transmit.   */
    if (err_net != NET_IF_ERR_NONE) {
        return (DEF_FAIL);
    }


#ifdef NET_IPv4_MODULE_EN
                                                                /* --------- CONFIGURE IPV4 STATIC ADDRESSES ---------- */
                                                                /* For Dynamic IPv4 configuration, uC/DHCPc is required */

                                                                /* TODO Update IPv4 Addresses following your network ...*/
                                                                /* ... requirements.                                    */

                                                                /* See Note #5.                                         */
    NetASCII_Str_to_IP("10.10.10.64",                           /* Convert Host IPv4 string address to 32 bit address.  */
                       &addr_ipv4,
                        NET_IPv4_ADDR_SIZE,
                       &err_net);

    NetASCII_Str_to_IP("255.255.255.0",                         /* Convert IPv4 mask string to 32 bit address.          */
                       &msk_ipv4,
                        NET_IPv4_ADDR_SIZE,
                       &err_net);

    NetASCII_Str_to_IP("10.10.10.1",                            /* Convert Gateway string address to 32 bit address.    */
                       &gateway_ipv4,
                        NET_IPv4_ADDR_SIZE,
                       &err_net);

    NetIPv4_CfgAddrAdd(if_nbr_ether,                            /* Add a statically-configured IPv4 host address,   ... */
                       addr_ipv4,                               /* ... subnet mask, & default gateway to the        ... */
                       msk_ipv4,                                /* ... interface. See Note #6.                          */
                       gateway_ipv4,
                      &err_net);
    if (err_net != NET_IPv4_ERR_NONE) {
        return (DEF_FAIL);
    }
#endif


#ifdef  NET_IPv6_MODULE_EN

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
                                                                /* ----- START IPV6 STATELESS AUTO-CONFIGURATION ------ */
    NetIPv6_AddrAutoCfgHookSet(if_nbr_ether,                /* Set Callback function to received Auto-Cfg result.   */
                              &App_AutoCfgResult,
                              &err_net);

    cfg_result = NetIPv6_AddrAutoCfgEn(if_nbr_ether,            /* Enable and Start Auto-Configuration process.         */
                                       DEF_YES,
                                      &err_net);
    if (cfg_result == DEF_FAIL) {
        return (DEF_FAIL);
    }

#endif
                                                                /* ----- CONFIGURE IPV6 STATIC LINK LOCAL ADDRESS ----- */
                                                                /* DHCPv6c is not yet available.                        */

    ipv6_flags = 0;
    DEF_BIT_SET(ipv6_flags, NET_IPv6_FLAG_BLOCK_EN);            /* Set Address Configuration as blocking.               */
    DEF_BIT_SET(ipv6_flags, NET_IPv6_FLAG_DAD_EN);              /* Enable DAD with Address Configuration.               */

                                                                /* TODO Update IPv6 Address following your network ...  */
                                                                /* ... requirements.                                    */

                                                                /* See Note #7.                                         */
    NetASCII_Str_to_IP("fe80::1111:1111",                       /* Convert IPv6 string address to 128 bit address.      */
                       &addr_ipv6,
                        NET_IPv6_ADDR_SIZE,
                       &err_net);

    cfg_result = NetIPv6_CfgAddrAdd(if_nbr_ether,               /* Add a statically-configured IPv6 host address to ... */
                                   &addr_ipv6,                  /* ... the interface. See Note 6.                       */
                                    64,
                                    ipv6_flags,
                                   &err_net);
    if (cfg_result == DEF_FAIL) {
        return (DEF_FAIL);
    }

#endif  /* NET_IPv6_MODULE_EN */


                                                                /* -------------- ADD WIRELESS INTERFACE -------------- */
    if_nbr_wifi = NetIF_Add((void    *)&NetIF_API_WiFi,
                            (void    *)&NetDev_API_TemplateWiFiSpi,  /* TODO Change following your Device driver API.   */
                            (void    *)&NetDev_BSP_WiFi,             /* TODO Change for your BSP API.                   */
                            (void    *)&NetDev_Cfg_WiFi_1,           /* TODO Change for Device configuration.           */
                            (void    *)&NetWiFiMgr_API_Generic,      /* TODO Might be change for another PHY.           */
                                        DEF_NULL,
                                       &err_net);
    if (err_net != NET_IF_ERR_NONE) {
        return (DEF_FAIL);
    }

                                                                /* ------------- START WIRELESS INTERFACE ------------- */
                                                                /* See Note #4.                                         */
    NetIF_Start(if_nbr_wifi, &err_net);                         /* Makes the interface ready to receive and transmit.   */
    if (err_net != NET_IF_ERR_NONE) {
        return (DEF_FAIL);
    }


                                                                /* ------------ SCAN FOR WIRELESS NETWORKS ------------ */
    ctn = NetIF_WiFi_Scan(if_nbr_wifi,                          /* See Note #8.                                         */
                          ap,                                   /* Access point table See Note #8a.                     */
                          10,                                   /* Access point table size.                             */
                          DEF_NULL,                             /* Hidden SSID See Note #8b.                            */
                          NET_IF_WIFI_CH_ALL,                   /* Channel to scan See Note #8c.                        */
                         &err_net);
    if (err_net != NET_IF_WIFI_ERR_NONE) {
        return (DEF_FAIL);
    }


                                                                /* --------- ANALYSE WIRELESS NETWORKS FOUND ---------- */
    found = DEF_NO;
    for (i = 0; i < ctn - 1; i++) {                             /* Browse table of access point found.                  */
        p_ssid = &ap[i].SSID;
        result = Str_Cmp_N((CPU_CHAR *)p_ssid,                  /* Search for a specific Wireless Network SSID.         */
                                      "Wifi_AP_SSID",           /* TODO Change for your WiFi Network SSID.              */
                                       NET_IF_WIFI_STR_LEN_MAX_SSID);
        if (result == 0) {
            found = DEF_YES;
            break;
        }
    }

    if (found == DEF_NO) {
        return (DEF_FAIL);
    }


                                                                /* ------------- JOIN A WIRELESS NETWORK -------------- */
    Mem_Clr(&ssid, sizeof(ssid));
    Mem_Clr(&psk,  sizeof(psk));
    Str_Copy_N((CPU_CHAR *)&ssid,
                           "Wifi_AP_SSID",                      /* TODO Change for your WiFi Network SSID.              */
                            12);                                /* SSID string length.                                  */
    Str_Copy_N((CPU_CHAR *)&psk,
                           "Password",                          /* TODO Change for your WiFi Network Password.          */
                            8);                                 /* PSK string length.                                   */

    NetIF_WiFi_Join(if_nbr_wifi,                                /* See Note #9.                                         */
                    ap[i].NetType,                              /* See Note #9a.                                        */
                    NET_IF_WIFI_DATA_RATE_AUTO,                 /* See Note #9b.                                        */
                    ap[i].SecurityType,                         /* See Note #9c.                                        */
                    NET_IF_WIFI_PWR_LEVEL_HI,                   /* See Note #9d.                                        */
                    ssid,                                       /* See Note #9e.                                        */
                    psk,                                        /* See Note #9f.                                        */
                   &err_net);
    if (err_net != NET_IF_WIFI_ERR_NONE) {
        return (DEF_FAIL);
    }


#ifdef NET_IPv4_MODULE_EN
                                                                /* --------- CONFIGURE IPV4 STATIC ADDRESSES ---------- */
                                                                /* For Dynamic IPv4 configuration, uC/DHCPc is required */

                                                                /* TODO Update IPv4 Addresses following your network ...*/
                                                                /* ... requirements.                                    */

                                                                /* See Note #5.                                         */
    NetASCII_Str_to_IP("192.168.1.10",                          /* Convert Host IPv4 string address to 32 bit address.  */
                       &addr_ipv4,
                        NET_IPv4_ADDR_SIZE,
                       &err_net);

    NetASCII_Str_to_IP("255.255.255.0",                         /* Convert IPv4 mask string to 32 bit address.          */
                       &msk_ipv4,
                        NET_IPv4_ADDR_SIZE,
                       &err_net);

    NetASCII_Str_to_IP("192.168.1.1",                           /* Convert Gateway string address to 32 bit address.    */
                       &gateway_ipv4,
                        NET_IPv4_ADDR_SIZE,
                       &err_net);

    NetIPv4_CfgAddrAdd(if_nbr_wifi,                             /* Add a statically-configured IPv4 host address,   ... */
                       addr_ipv4,                               /* ... subnet mask, & default gateway to the        ... */
                       msk_ipv4,                                /* ... interface. See Note #6.                          */
                       gateway_ipv4,
                      &err_net);
    if (err_net != NET_IPv4_ERR_NONE) {
        return (DEF_FAIL);
    }
#endif


#ifdef NET_IPv6_MODULE_EN

#ifdef  NET_IPv6_ADDR_AUTO_CFG_MODULE_EN
                                                                /* ----- START IPV6 STATELESS AUTO-CONFIGURATION ------ */
    NetIPv6_AddrAutoCfgHookSet(if_nbr_wifi,                     /* Set hook function to received Auto-Cfg result.       */
                              &App_AutoCfgResult,
                              &err_net);

    cfg_result = NetIPv6_AddrAutoCfgEn(if_nbr_wifi,             /* Enable and Start Auto-Configuration process.         */
                                       DEF_YES,
                                      &err_net);
    if (cfg_result == DEF_FAIL) {
        return (DEF_FAIL);
    }

#endif
                                                                /* ----- CONFIGURE IPV6 STATIC LINK LOCAL ADDRESS ----- */
                                                                /* DHCPv6c is not yet available.                        */

                                                                /* TODO Update IPv6 Address following your network ...  */
                                                                /* ... requirements.                                    */

                                                                /* See Note #5.                                         */
    NetASCII_Str_to_IP("fe80::4444:1111",                       /* Convert IPv6 string address to 128 bit address.      */
                       &addr_ipv6,
                        NET_IPv6_ADDR_SIZE,
                       &err_net);

    ipv6_flags = 0;
    DEF_BIT_SET(ipv6_flags, NET_IPv6_FLAG_BLOCK_EN);            /* Set Address Configuration as blocking.               */
    DEF_BIT_SET(ipv6_flags, NET_IPv6_FLAG_DAD_EN);              /* Enable DAD with Address Configuration.               */

    cfg_result = NetIPv6_CfgAddrAdd(if_nbr_wifi,                /* Add a statically-configured IPv6 host address to ... */
                                   &addr_ipv6,                  /* ... the interface. See Note 7.                       */
                                    64,
                                    ipv6_flags,
                                   &err_net);
    if (cfg_result == DEF_FAIL) {
        return (DEF_FAIL);
    }

#endif  /* NET_IPv6_MODULE_EN */


    return (DEF_OK);
}

/*
*********************************************************************************************************
*                                          App_AutoCfgResult()
*
* Description : Hook function to received the IPv6 Auto-Configuration process result.
*
* Argument(s) : if_nbr          Network Interface number on which Auto-Configuration occurred.
*
*               p_addr_local    Pointer to IPv6 link-local address configured, if any.
*                               DEF_NULL, otherwise.
*
*               p_addr_global   Pointer to IPv6 global address configured, if any.
*                               DEF_NULL, otherwise.
*
*               auto_cfg_result Result status of the IPv6 Auto-Configuration process.
*
* Return(s)   : None.
*
* Caller(s)   : Referenced in AppInit_TCPIP_MultipleIF().
*
* Note(s)     : None.
*********************************************************************************************************
*/
#ifdef  NET_IPv6_MODULE_EN
static  void  App_AutoCfgResult (      NET_IF_NBR                 if_nbr,
                                 const NET_IPv6_ADDR             *p_addr_local,
                                 const NET_IPv6_ADDR             *p_addr_global,
                                       NET_IPv6_AUTO_CFG_STATUS   auto_cfg_result)
{
    CPU_CHAR  ip_string[NET_ASCII_LEN_MAX_ADDR_IPv6];
    NET_ERR   err_net;


    if (p_addr_local != DEF_NULL) {
        NetASCII_IPv6_to_Str((NET_IPv6_ADDR *)p_addr_local, ip_string, DEF_NO, DEF_YES, &err_net);
        printf("IPv6 Address Link Local: %s\n", ip_string);
    }

    if (p_addr_global != DEF_NULL) {
        NetASCII_IPv6_to_Str((NET_IPv6_ADDR *)p_addr_global, ip_string, DEF_NO, DEF_YES, &err_net);
        printf("IPv6 Address Global: %s\n", ip_string);
    }

    switch (auto_cfg_result) {
        case NET_IPv6_AUTO_CFG_STATUS_FAILED:
             printf("Auto-Configuration failed.\n");
             break;


        case NET_IPv6_AUTO_CFG_STATUS_SUCCEEDED:
             printf("Auto-Configuration succeeded.\n");
             break;


        case NET_IPv6_AUTO_CFG_STATUS_LINK_LOCAL:
             printf("Auto-Configuration with Link-Local address only.\n");
             break;


        default:
             printf("Invalid Auto-Configuration result.\n");
             break;
    }
}
#endif
