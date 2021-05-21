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
* Filename : net_phy_ksz9021r.c
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
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_PHY_KSZ9021R_MODULE
#include  <Source/net.h>
#include  <IF/net_if_ether.h>
#include  "net_phy_ksz9021r.h"

#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  PHY_TYPE_KSZ9021R                     0
#define  PHY_TYPE_KSZ9031RNX                   1

#define  NET_PHY_ADDR_MAX                     31                /* 5 bit Phy address max value.                         */
#define  NET_PHY_INIT_RESET_RETRIES            3                /* Check for successful reset x times                   */
#define  NET_PHY_INIT_AUTO_NEG_RETRIES        64                /* Attempt Auto-Negotiation x times                     */

/*
*********************************************************************************************************
*                                          REGISTER DEFINES
*********************************************************************************************************
*/
                                                                /* -------------- IEEE DEFINED REGISTERS -------------- */
#define  PHY_BMCR                           0x00                /* Basic mode Control register                          */
#define  PHY_BMSR                           0x01                /* Basic Status register                                */
#define  PHY_PHYSID1                        0x02                /* PHY Identifier Register #1                           */
#define  PHY_PHYSID2                        0x03                /* PHY Identifier Register #2                           */
#define  PHY_ANAR                           0x04                /* Auto-Negotiation Advertisement control reg.          */
#define  PHY_ANLPAR                         0x05                /* Auto-Negotiation Link partner ability reg.           */
#define  PHY_ANER                           0x06                /* Auto-Negotiation Expansion reg.                      */
#define  PHY_ANNPTR                         0x07                /* Auto-Negotiation Next page transmit reg.             */
#define  PHY_1000BASE_T_CTRL                0x09                /* 1000Base-T Control Register (Register 9 )            */

                                                                /* -------- EXTENDED REGISTER ACCESS(KSZ9021R) -------- */
#define  PHY_EXT_REG_CTRL                   0x0B                /* Extended register control.                           */
#define  PHY_EXT_REG_DATA_WR                0x0C                /* Extended register data write.                        */
#define  PHY_EXT_REG_DATA_RD                0x0D                /* Extended register data read.                         */

                                                                /* --------- MMD ACCESS REGISTERS(KSZ9031RNX) --------- */
#define  PHY_MMD_CTRL                       0x0D                /* MMD Access - Control                                 */
#define  PHY_MMD_REGDATA                    0x0E                /* MMD Access - Register/Data                           */

#define  PHY_MMD_AN_FLP_BURST_TX_ADDR       0x00                /* MMD address  Auto-Negotiation FLP burst transmit     */
#define  PHY_MMD_AN_FLP_BURST_TX_LO_REG     0x03                /* MMD register Auto-Negotiation FLP burst transmit-LO  */
#define  PHY_MMD_AN_FLP_BURST_TX_HI_REG     0x04                /* MMD register Auto-Negotiation FLP burst transmit-HI  */

#define  PHY_MMD_COMMON_CTRL_ADDR           0x02                /* MMD Address  Common Control                          */
#define  PHY_MMD_COMMON_CTRL_REG            0x00                /* MMD register Common Control                          */

                                                                /* ------------ VENDOR SPECIFIC REGISTERS ------------- */
#define  PHY_CTRL                           0x1F                /* PHY Control register                                 */


/*
*********************************************************************************************************
*                                            REGISTER BITS
*********************************************************************************************************
*/

                                                                /* ---------------- PHY_BMCR Register Bits ------------ */
#define  BMCR_RESV                        0x003F                /* Unused...                                            */
#define  BMCR_MSB_SPEED1000           DEF_BIT_06                /* Select 1000Mbps. RO bit, bit 6 = 1, bit 13 = 0       */
#define  BMCR_CTST                    DEF_BIT_07                /* Collision test.                                      */
#define  BMCR_FULLDPLX                DEF_BIT_08                /* Full duplex.                                         */
#define  BMCR_ANRESTART               DEF_BIT_09                /* Auto negotiation restart.                            */
#define  BMCR_ISOLATE                 DEF_BIT_10                /* Disconnect Phy from MII.                             */
#define  BMCR_PDOWN                   DEF_BIT_11                /* Power down.                                          */
#define  BMCR_ANENABLE                DEF_BIT_12                /* Enable auto negotiation.                             */
#define  BMCR_LSB_SPEED1000           DEF_BIT_13                /* Select 1000Mbps. RO bit, bit 6 = 1, bit 13 = 0       */
#define  BMCR_LOOPBACK                DEF_BIT_14                /* TXD loopback bits.                                   */
#define  BMCR_RESET                   DEF_BIT_15                /* Reset.                                               */

#define  BMCR_SPEED_MASK             (DEF_BIT_06 | DEF_BIT_13)
#define  BMCR_SPEED10_MASK            DEF_BIT_NONE
#define  BMCR_SPEED100_MASK           DEF_BIT_13
#define  BMCR_SPEED1000_MASK          DEF_BIT_06

                                                                /* -------------- PHY_BMSR Register Bits -------------- */
#define  BMSR_ERCAP                   DEF_BIT_00                /* Ext-reg capability.                                  */
#define  BMSR_JCD                     DEF_BIT_01                /* Jabber detected.                                     */
#define  BMSR_LSTATUS                 DEF_BIT_02                /* Link status.                                         */
#define  BMSR_ANEGCAPABLE             DEF_BIT_03                /* Able to do auto-negotiation.                         */
#define  BMSR_RFAULT                  DEF_BIT_04                /* Remote fault detected.                               */
#define  BMSR_ANEGCOMPLETE            DEF_BIT_05                /* Auto-negotiation complete.                           */
#define  BMSR_10HALF                  DEF_BIT_11                /* Can do 10mbps, half-duplex.                          */
#define  BMSR_10FULL                  DEF_BIT_12                /* Can do 10mbps, full-duplex.                          */
#define  BMSR_100HALF                 DEF_BIT_13                /* Can do 100mbps, half-duplex.                         */
#define  BMSR_100FULL                 DEF_BIT_14                /* Can do 100mbps, full-duplex.                         */
#define  BMSR_100BASE4                DEF_BIT_15                /* Can do 100mbps, 4k packets.                          */

                                                                /* ------------- PHY_ANAR Register Bits --------------- */
#define  ANAR_SLCT                        0x001F                /* Selector bits.                                       */
#define  ANAR_10HALF                  DEF_BIT_05                /* Try for 10mbps half-duplex.                          */
#define  ANAR_10FULL                  DEF_BIT_06                /* Try for 10mbps full-duplex.                          */
#define  ANAR_100HALF                 DEF_BIT_07                /* Try for 100mbps half-duplex.                         */
#define  ANAR_100FULL                 DEF_BIT_08                /* Try for 100mbps full-duplex.                         */
#define  ANAR_100BASE4                DEF_BIT_09                /* Try for 100mbps 4k packets.                          */
#define  ANAR_PAUSE                   DEF_BIT_10                /* Pause.                                               */
#define  ANAR_ASYM_PAUSE              DEF_BIT_11                /* Asymetric Pause.                                     */
#define  ANAR_RFAULT                  DEF_BIT_13                /* Remote fault.                                        */
#define  ANAR_LPACK                   DEF_BIT_14                /* Ack link partners response.                          */
#define  ANAR_NPAGE                   DEF_BIT_15                /* Next page bit.                                       */

                                                                /* ------------ PHY_ANLPAR Register Bits -------------- */
#define  ANLPAR_SLCT                      0x001F                /* Same as advertise selector.                          */
#define  ANLPAR_10HALF                DEF_BIT_05                /* Can do 10mbps half-duplex.                           */
#define  ANLPAR_10FULL                DEF_BIT_06                /* Can do 10mbps full-duplex.                           */
#define  ANLPAR_100HALF               DEF_BIT_07                /* Can do 100mbps half-duplex.                          */
#define  ANLPAR_100FULL               DEF_BIT_08                /* Can do 100mbps full-duplex.                          */
#define  ANLPAR_100BASE4              DEF_BIT_09                /* Can do 100mbps 4k packets.                           */
#define  ANLPAR_RFAULT                DEF_BIT_13                /* Link partner faulted.                                */
#define  ANLPAR_NPAGE                 DEF_BIT_15                /* Next page bit.                                       */


                                                                /* ------------ PHY CONTROL Register Bitss ------------ */
                                                                /* Speed status mask                                    */
#define  PHY_CTRL_SPD_MASK           (DEF_BIT_04 | DEF_BIT_05 | DEF_BIT_06)
#define  PHY_CTRL_SPD_1000            DEF_BIT_06                /* Speed status at 1000 MBit/s                          */
#define  PHY_CTRL_SPD_100             DEF_BIT_05                /* Speed status at  100 MBit/s                          */
#define  PHY_CTRL_SPD_10              DEF_BIT_04                /* Speed status at  100 MBit/s                          */
#define  PHY_CTRL_DUPLEX              DEF_BIT_03                /* Duplex status mode bit                               */

                                                                /* ----------- MMD ACCESS BITS (KSZ9031RNX) ----------- */
#define  MMD_CTRL_OP_MODE_DATA        DEF_BIT_14                /* Data, no post increment                              */

#define  COMMON_CTRL_SINGLE_LED_MODE  DEF_BIT_04                /* Override strap-in for LED mode                       */

#define  AN_FLP_BURST_TX_LO_16MS_INTVL   0x1A80u                /* Select 16ms interval timing for AN FLP burst Tx      */
#define  AN_FLP_BURST_TX_HI_16MS_INTVL   0x0006u                /* Select 16ms interval timing for AN FLP burst Tx      */


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*
* Note(s) : (1) Physical layer driver functions may be arbitrarily named.  However, it is recommended that
*               physical layer driver functions be named using the names provided below.  All driver function
*               prototypes should be located within the driver C source file ('net_phy_&&&.c') & be declared
*               as static functions to prevent name clashes with other network protocol suite physical layer
*               drivers.
*********************************************************************************************************
*/

static  void  NetPhy_Init                   (NET_IF              *p_if,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_KSZ9031RNX_Init        (NET_IF              *p_if,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_InitHandler            (NET_IF              *p_if,
		                                     CPU_INT08U           phy_type,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_EnDis                  (NET_IF              *p_if,
                                             CPU_BOOLEAN          en,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_LinkStateGet           (NET_IF              *p_if,
                                             NET_DEV_LINK_ETHER  *p_link_state,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_LinkStateSet           (NET_IF              *p_if,
                                             NET_DEV_LINK_ETHER  *p_link_state,
                                             NET_ERR             *p_err);

static  void  NetPhy_KSZ9031RNX_LinkStateSet(NET_IF              *p_if,
                                             NET_DEV_LINK_ETHER  *p_link_state,
                                             NET_ERR             *p_err);

static  void  NetPhy_LinkStateSetHandler    (NET_IF              *p_if,
                                             NET_DEV_LINK_ETHER  *p_link_state,
			                                 CPU_INT08U           phy_type,
                                             NET_ERR             *p_err);

static  void  NetPhy_AutoNegStart           (NET_IF              *p_if,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_AddrProbe              (NET_IF              *p_if,
                                             NET_ERR             *p_err);

                                                                /* ---------- KSZ9031RNX MMD REGISTER ACCESS ---------- */
static  void  NetPhy_MMD_RegRd              (NET_IF              *p_if,
                                             CPU_INT08U           dev_addr,
                                             CPU_INT08U           reg_nbr,
                                             CPU_INT16U          *p_data,
                                             NET_ERR             *p_err);
                                            
static  void  NetPhy_MMD_RegWr              (NET_IF              *p_if,
                                             CPU_INT08U           dev_addr,
                                             CPU_INT08U           reg_nbr,
                                             CPU_INT16U           reg_data,
                                             NET_ERR             *p_err);

/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                  NETWORK PHYSICAL LAYER DRIVER API
*
* Note(s) : (1) Physical layer driver API structures are used by applications during calls to NetIF_Add().
*               This API structure allows higher layers to call specific physical layer driver functions
*               via function pointer instead of by name.  This enables the network protocol suite to
*               compile & operate with multiple physical layer drivers.
*
*           (2) In most cases, the API structure provided below SHOULD suffice for most physical layer
*               drivers exactly as is with the exception that the API structure's name which MUST be
*               unique & SHOULD clearly identify the physical layer being implemented.  For example,
*               the AMD 79C874's API structure should be named NetPhy_API_AM79C874[].
*
*               The API structure MUST also be externally declared in the physical layer driver header
*               file ('net_phy_&&&.h') with the exact same name & type.
*********************************************************************************************************
*/
                                                                /* KSZ9021R phy API fnct ptrs :                         */
const  NET_PHY_API_ETHER  NetPhy_API_ksz9021r = {
    &NetPhy_Init,                                               /*   Init                                               */
    &NetPhy_EnDis,                                              /*   En/dis                                             */
    &NetPhy_LinkStateGet,                                       /*   Link get                                           */
    &NetPhy_LinkStateSet,                                       /*   Link set                                           */
    0                                                           /*   ISR handler                                        */
};

                                                                /* KSZ9031RNX phy API fnct ptrs :                       */
const  NET_PHY_API_ETHER  NetPhy_API_ksz9031rnx = {
    &NetPhy_KSZ9031RNX_Init,                                    /*   Init                                               */
    &NetPhy_EnDis,                                              /*   En/dis                                             */
    &NetPhy_LinkStateGet,                                       /*   Link get                                           */
    &NetPhy_KSZ9031RNX_LinkStateSet,                            /*   Link set                                           */
    0                                                           /*   ISR handler                                        */
};


/*
*********************************************************************************************************
*                                            NetPhy_Init()
*                                      NetPhy_KSZ9031RNX_Init()
*
* Description : Initialize KSZ9021R/KSZ9031RNX Ethernet physical layer.
*
* Argument(s) : p_if      Pointer to interface to initialize Phy.
*               ---       Argument checked in NetIF_Ether_IF_Start().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_PHY_ERR_NONE                 Ethernet physical layer successfully
*                                                              initialized.
*                             NET_PHY_ERR_TIMEOUT_RESET        Phy reset          time-out.
*                             NET_PHY_ERR_TIMEOUT_AUTO_NEG     Auto-Negotiation   time-out.
*                             NET_PHY_ERR_TIMEOUT_REG_RD       Phy register read  time-out.
*                             NET_PHY_ERR_TIMEOUT_REG_WR       Phy register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Interface &/or device start handler(s).
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #3].
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_Init (NET_IF   *p_if,
                           NET_ERR  *p_err)
{
    NetPhy_InitHandler(p_if, PHY_TYPE_KSZ9021R, p_err);
}


static  void  NetPhy_KSZ9031RNX_Init (NET_IF   *p_if,
                                      NET_ERR  *p_err)
{
    NetPhy_InitHandler(p_if, PHY_TYPE_KSZ9031RNX, p_err);
}


/*
*********************************************************************************************************
*                                            NetPhy_Init()
*
* Description : Initialize Ethernet physical layer.
*
* Argument(s) : p_if         Pointer to interface to initialize Phy.
*               ---          Argument checked in NetIF_Ether_IF_Start().
*
*               phy_type     Micrel PHY type.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                                NET_PHY_ERR_NONE                 Ethernet physical layer successfully
*                                                                 initialized.
*                                NET_PHY_ERR_TIMEOUT_RESET        Phy reset          time-out.
*                                NET_PHY_ERR_TIMEOUT_AUTO_NEG     Auto-Negotiation   time-out.
*                                NET_PHY_ERR_TIMEOUT_REG_RD       Phy register read  time-out.
*                                NET_PHY_ERR_TIMEOUT_REG_WR       Phy register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Interface &/or device start handler(s).
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #3].
*
* Note(s)     : (1) Assumes the MDI port as already been enabled for the Phy.
*
*               (2) Phy initialization occurs each time the interface is started.
*                   See 'net_if.c  NetIF_Start()'.
*
*               (3) KSZ9031RNX
*                   ERRATA     : LED Toggle is not visible for Tri-color Dual-LED mode.
*                   Description: In Tri-color Dual-LED mode, the LED[2:1] pin outputs toggle high pulses
*                                for transmit/receive activity indication. The high pulse width incorrectly
*                                tracks the activity data rate. At low data rate (e.g., one frame per second),
*                                the LED pin drives high (OFF) with a narrow high pulse width of about 640ns.
*
*                   Work around: Use the Single-LED mode instead, so the LED toggle rate is visible to the
*                                human eye. A 640ns pulse is not visible.
*********************************************************************************************************
*/

static  void  NetPhy_InitHandler (NET_IF      *p_if,
                                  CPU_INT08U   phy_type,
                                  NET_ERR     *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_PHY_CFG_ETHER  *p_phy_cfg;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          retries;
    CPU_INT08U          phy_addr;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    p_phy_cfg = (NET_PHY_CFG_ETHER *)p_if->Ext_Cfg;
    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    phy_addr  =                      p_phy_cfg->BusAddr;        /* Obtain user cfg'd Phy addr.                          */
	
    if (phy_addr == NET_PHY_ADDR_AUTO) {                        /* Automatic detection of Phy address enabled.          */
        NetPhy_AddrProbe(p_if, p_err);                          /* Attempt to automatically determine Phy addr.         */
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }
        phy_addr            = p_if_data->Phy_Addr;
    } else {
        p_if_data->Phy_Addr = phy_addr;                         /* Set Phy addr to cfg'd val.                           */
    }

                                                                /* -------------------- RESET PHY --------------------- */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_BMCR, BMCR_RESET, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

                                                                /* Rd ctrl reg, get reset bit.                          */
    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMCR, &reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    reg_val &= BMCR_RESET;                                      /* Mask out reset status bit.                           */


    retries = NET_PHY_INIT_RESET_RETRIES;
    while ((reg_val == BMCR_RESET) && (retries > 0)) {          /* Wait for reset to complete.                          */
        KAL_Dly(200);

        p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMCR, &reg_val, p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }

        reg_val &= BMCR_RESET;
        retries--;
    }

    if (retries == 0u) {
       *p_err = NET_PHY_ERR_TIMEOUT_RESET;
        return;
    }

    if (phy_type == PHY_TYPE_KSZ9021R) {
        p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_EXT_REG_CTRL,(0x8000 | 0x104), p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }

        p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_EXT_REG_DATA_WR, 0xA0D0, p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }

    } else if (phy_type == PHY_TYPE_KSZ9031RNX) {
                                                                /* --------- SET SINGLE-LED MODE(See Note #3) --------- */
        NetPhy_MMD_RegRd(p_if,
                         PHY_MMD_COMMON_CTRL_ADDR,
                         PHY_MMD_COMMON_CTRL_REG,
                        &reg_val,
                         p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }

        reg_val |= COMMON_CTRL_SINGLE_LED_MODE;
        NetPhy_MMD_RegWr(p_if,
                         PHY_MMD_COMMON_CTRL_ADDR,
                         PHY_MMD_COMMON_CTRL_REG,
                         reg_val,
                         p_err);
    }
}


/*
*********************************************************************************************************
*                                           NetPhy_EnDis()
*
* Description : Enable/disable the Phy.
*
* Argument(s) : p_if      Pointer to interface to enable/disable Phy.
*               ---
*
*               en        Enable option :
*
*                             DEF_ENABLED                    Enable  Phy
*                             DEF_DISABLED                   Disable Phy
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_PHY_ERR_NONE               Physical layer successfully enabled/disabled.
*                             NET_PHY_ERR_TIMEOUT_RESET      Phy reset          time-out.
*                             NET_PHY_ERR_TIMEOUT_REG_RD     Phy register read  time-out.
*                             NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pphy_api->EnDis()',
*               NetIF_Ether_IF_Stop()  via 'pphy_api->EnDis()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_EnDis (NET_IF       *p_if,
                            CPU_BOOLEAN   en,
                            NET_ERR      *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT16U          reg_val;
    CPU_INT08U          phy_addr;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    phy_addr  =                      p_if_data->Phy_Addr;       /* Obtain Phy addr.                                     */
                                                                /* Obtain current control register value.               */
    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMCR, &reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    switch (en) {
        case DEF_DISABLED:
             reg_val |=  BMCR_PDOWN;                            /* Disable PHY.                                         */
             break;


        case DEF_ENABLED:
        default:
             reg_val &= ~BMCR_PDOWN;                            /* Enable PHY.                                          */
             break;
    }
                                                                /* Power up / down the Phy.                             */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_BMCR, reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetPhy_LinkStateGet()
*
* Description : Get current Phy link state (speed & duplex).
*
* Argument(s) : p_if             Pointer to interface to get link state.
*               ---              Argument validated in NetIF_IO_CtrlHandler().
*
*               p_link_state     Pointer to structure that will receive the link state.
*
*               p_err            Pointer to variable  that will receive the return error code from this function :
*
*                                    NET_PHY_ERR_NONE               No  error.
*                                    NET_PHY_ERR_NULL_PTR           Pointer argument(s) passed NULL pointer(s).
*                                    NET_PHY_ERR_TIMEOUT_RESET      Phy reset          time-out.
*                                    NET_PHY_ERR_TIMEOUT_REG_RD     Phy register read  time-out.
*                                    NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Caller(s)   : Device driver(s)' link state &/or I/O control handler(s).
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #2].
*
* Return(s)   : none.
*
* Note(s)     : (1) Some Phy's have the link status field latched in the BMSR register.  The link status
*                   remains low after a temporary link failure until it is read. To retrieve the current
*                   link status, BMSR must be read twice.
*
*               (2) Current link state should be obtained by calling this function through the NetIF layer.
*                   See 'net_if.c  NetIF_IO_Ctrl()'.
*********************************************************************************************************
*/

static  void  NetPhy_LinkStateGet (NET_IF              *p_if,
                                   NET_DEV_LINK_ETHER  *p_link_state,
                                   NET_ERR             *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          link_self;
    CPU_INT08U          phy_addr;
    CPU_INT16U          speed_res;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    phy_addr  =                      p_if_data->Phy_Addr;       /* Obtain Phy addr.                                     */


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_link_state == (NET_DEV_LINK_ETHER *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    p_link_state->Spd    = NET_PHY_SPD_0;
    p_link_state->Duplex = NET_PHY_DUPLEX_UNKNOWN;

                                                                /* -------------- OBTAIN CUR LINK STATUS -------------- */
    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMSR, &link_self, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Rd BMSR twice (see Note #1).                         */
    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMSR, &link_self, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    if ((link_self & BMSR_LSTATUS) == 0) {                      /* Chk if link down.                                    */
        *p_err = NET_PHY_ERR_NONE;
        return;
    }

                                                                /* ------------- DETERMINE SPD AND DUPLEX ------------- */
                                                                /* Obtain AN settings.                                  */
    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMCR, &reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    if ((reg_val & BMCR_ANENABLE) == 0) {                       /* IF AN disabled.                                      */
                                                                /* Determine spd.                                       */
        speed_res = reg_val & BMCR_SPEED_MASK;
        switch (speed_res) {
            case BMCR_SPEED10_MASK:
                 p_link_state->Spd = NET_PHY_SPD_10;
                 break;


            case BMCR_SPEED100_MASK:
                 p_link_state->Spd = NET_PHY_SPD_100;
                 break;


            case BMCR_SPEED1000_MASK:
                 p_link_state->Spd = NET_PHY_SPD_1000;
                 break;


            default:
                 break;
        }

        if ((reg_val & BMCR_FULLDPLX) == 0) {                   /* Determine duplex.                                    */
            p_link_state->Duplex = NET_PHY_DUPLEX_HALF;
        } else {
            p_link_state->Duplex = NET_PHY_DUPLEX_FULL;
        }
    } else {
                                                                /* Obtain current link state from the PHY control reg.  */
        p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_CTRL, &reg_val, p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }

        switch(reg_val & PHY_CTRL_SPD_MASK) {
            case PHY_CTRL_SPD_1000:                             /* 1000 MBit/s                                          */
                 p_link_state->Spd = NET_PHY_SPD_1000;
                 break;


            case PHY_CTRL_SPD_100:                              /* 100 MBit/s                                           */
                 p_link_state->Spd = NET_PHY_SPD_100;
                 break;


            case PHY_CTRL_SPD_10:                               /* 10 MBit/s                                            */
                 p_link_state->Spd = NET_PHY_SPD_10;
                 break;
				

            default:
                 break;
        }
        if ((reg_val & PHY_CTRL_DUPLEX)) {                      /* Full duplex?                                         */
            p_link_state->Duplex = NET_PHY_DUPLEX_FULL;

        } else {
            p_link_state->Duplex = NET_PHY_DUPLEX_HALF;
        }
    }

                                                                 /* Link established, update MAC settings.              */
    p_dev_api->IO_Ctrl(        p_if,
                               NET_IF_IO_CTRL_LINK_STATE_UPDATE,
                       (void *)p_link_state,
                               p_err);
    if (*p_err != NET_DEV_ERR_NONE) {
        return;
    }

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetPhy_LinkStateSet()
*                                  NetPhy_KSZ9031RNX_LinkStateSet()
*
* Description : Set current KSZ9021R/KSZ9031RNX Phy link state (speed & duplex).
*
* Argument(s) : p_if             Pointer to interface to get link state.
*               ---              Argument validated in NetIF_Start().
*
*               p_link_state     Pointer to structure that will contain the desired link state.
*
*               p_err            Pointer to variable  that will receive the return error code from this function :
*
*                                    NET_PHY_ERR_NONE               No  error.
*                                    NET_PHY_ERR_NULL_PTR           Pointer argument(s) passed NULL pointer(s).
*                                    NET_PHY_ERR_TIMEOUT_RESET      Phy reset          time-out.
*                                    NET_PHY_ERR_TIMEOUT_REG_RD     Phy register read  time-out.
*                                    NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pphy_api->LinkStateSet()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_LinkStateSet (NET_IF              *p_if,
                                   NET_DEV_LINK_ETHER  *p_link_state,
                                   NET_ERR             *p_err)
{
	NetPhy_LinkStateSetHandler(p_if, p_link_state, PHY_TYPE_KSZ9021R, p_err);
}


static  void  NetPhy_KSZ9031RNX_LinkStateSet (NET_IF              *p_if,
                                              NET_DEV_LINK_ETHER  *p_link_state,
                                              NET_ERR             *p_err)
{
	NetPhy_LinkStateSetHandler(p_if, p_link_state, PHY_TYPE_KSZ9031RNX, p_err);
}


/*
*********************************************************************************************************
*                                     NetPhy_LinkStateSetHandler()
*
* Description : Set current Phy link state (speed & duplex).
*
* Argument(s) : p_if             Pointer to interface to get link state.
*               ---              Argument validated in NetIF_Start().
*
*               p_link_state     Pointer to structure that will contain the desired link state.
*
*               phy_type         Micrel PHY type.
*
*               p_err            Pointer to variable  that will receive the return error code from this function :
*
*                                    NET_PHY_ERR_NONE               No  error.
*                                    NET_PHY_ERR_NULL_PTR           Pointer argument(s) passed NULL pointer(s).
*                                    NET_PHY_ERR_TIMEOUT_RESET      Phy reset          time-out.
*                                    NET_PHY_ERR_TIMEOUT_REG_RD     Phy register read  time-out.
*                                    NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pphy_api->LinkStateSet()'.
*
* Return(s)   : none.
*
* Note(s)     : (1) KSZ9031RNX
*                   ERRATA     : Auto-Negotiation link-up failure / long link-up time due to default FLP
*                                interval setting.
*                   Description: The device's Auto-Negotiation FLP (Fast Link Pulse) burst-to-burst timing
*                                defaults to 8ms. IEEE Standard specifies this timing to be 16ms +/-8ms.
*                                Some link partners, such as Intel G-PHY controllers, were observed in bench
*                                tests to have tighter timing requirements that need to detect the FLP
*                                interval timing centered at 16ms
*
*                   Work around: After device power-up/reset, change the FLP interval to 16ms
*********************************************************************************************************
*/

static  void  NetPhy_LinkStateSetHandler (NET_IF              *p_if,
                                          NET_DEV_LINK_ETHER  *p_link_state,
		                                  CPU_INT08U           phy_type,
                                          NET_ERR             *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          spd;
    CPU_INT08U          duplex;
    CPU_INT08U          phy_addr;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    phy_addr  =                      p_if_data->Phy_Addr;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_link_state == (NET_DEV_LINK_ETHER *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    spd    = p_link_state->Spd;
    duplex = p_link_state->Duplex;

    if (phy_type == PHY_TYPE_KSZ9031RNX) {
                                                                /* ----- SET AUTO-NEGOTIATION FLP(FAST LINK PULSE) ---- */
                                                                /* See Note #1.                                         */
        NetPhy_MMD_RegWr(p_if,
                         PHY_MMD_AN_FLP_BURST_TX_ADDR,
                         PHY_MMD_AN_FLP_BURST_TX_HI_REG,
                         AN_FLP_BURST_TX_HI_16MS_INTVL,
                         p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }

        NetPhy_MMD_RegWr(p_if,
                         PHY_MMD_AN_FLP_BURST_TX_ADDR,
                         PHY_MMD_AN_FLP_BURST_TX_LO_REG,
                         AN_FLP_BURST_TX_LO_16MS_INTVL,
                         p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            return;
        }
    }
                                                                /* Enable AN if cfg invalid or any member set to AUTO.  */
    if (((spd    != NET_PHY_SPD_10  )    &&
         (spd    != NET_PHY_SPD_100 )    &&
         (spd    != NET_PHY_SPD_1000))   ||
        ((duplex != NET_PHY_DUPLEX_HALF) &&
         (duplex != NET_PHY_DUPLEX_FULL))) {

        NetPhy_AutoNegStart(p_if, p_err);
        return;
    }

    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMCR, &reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    reg_val &= ~BMCR_ANENABLE;                                  /* Clr AN enable bit.                                   */

    switch (spd) {                                              /* Set spd.                                             */
        case NET_PHY_SPD_10:
             reg_val &= ~(DEF_BIT_06 | DEF_BIT_13);
             break;


        case NET_PHY_SPD_100:
             reg_val |=  DEF_BIT_13;
             reg_val &= ~DEF_BIT_06;
             break;


        case NET_PHY_SPD_1000:
             reg_val |=  DEF_BIT_06;
             reg_val &= ~DEF_BIT_13;
             break;


        default:
             break;
    }

    switch (duplex) {                                           /* Set duplex.                                          */
        case NET_PHY_DUPLEX_HALF:
             reg_val &= ~BMCR_FULLDPLX;
             break;


        case NET_PHY_DUPLEX_FULL:
             reg_val |=  BMCR_FULLDPLX;
             break;


        default:
             break;
    }
                                                                /* Cfg Phy.                                             */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_BMCR, reg_val | BMCR_RESET , p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetPhy_AutoNegStart()
*
* Description : Start the Auto-Negotiation processs.
*
* Argument(s) : p_if      Pointer to interface to start auto-negotiation.
*               ---       Argument validated in NetPhy_Init().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_PHY_ERR_NONE               Physical layer successfully started.
*                             NET_PHY_ERR_TIMEOUT_REG_RD     Phy register read  time-out.
*                             NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : NetPhy_LinkStateSet().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #2].
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_AutoNegStart (NET_IF   *p_if,
                                   NET_ERR  *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT16U          reg_val;
    CPU_INT08U          phy_addr;
    CPU_INT16U          retries;


    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    phy_addr  =                      p_if_data->Phy_Addr;

    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_BMCR, &reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    reg_val |= BMCR_ANENABLE |
               BMCR_ANRESTART;
                                                                /* Restart Auto-Negotiation.                            */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_BMCR, reg_val, p_err);
    if (*p_err != NET_PHY_ERR_NONE) {
        return;
    }

    retries = NET_PHY_INIT_AUTO_NEG_RETRIES;

    do {                                                        /* Wait until the auto-negotiation will be completed    */
        retries--;
        p_dev_api->Phy_RegRd (p_if, phy_addr, PHY_BMSR, &reg_val, p_err);
        if(*p_err != NET_PHY_ERR_NONE) {
            return;
        }

        if (reg_val & BMSR_ANEGCOMPLETE) {
            break;
        }

        KAL_Dly(200);

    } while (retries > 0);

    if(retries == 0) {
       *p_err = NET_PHY_ERR_TIMEOUT_AUTO_NEG;
        return;
    }

   *p_err = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetPhy_AddrProbe()
*
* Description : Automatically detect Phy bus address.
*
* Argument(s) : p_if      Pointer to interface to probe.
*               ---       Argument validated in NetPhy_Init().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_PHY_ERR_NONE           Physical layer's address successfully detected.
*                             NET_PHY_ERR_ADDR_PROBE     Unable to determine Phy address.
*
* Return(s)   : none.
*
* Caller(s)   : NetPhy_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) Assumes the MDI port has already been initialized for the Phy.
*********************************************************************************************************
*/

static  void  NetPhy_AddrProbe (NET_IF   *p_if,
                                NET_ERR  *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT16U          reg_id1;
    CPU_INT16U          reg_id2;
    CPU_INT08U          i;


    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    for (i = 0; i <= NET_PHY_ADDR_MAX; i++) {
		                                                        /* Obtain Phy ID 1 register value.                      */
        p_dev_api->Phy_RegRd(p_if, i, PHY_PHYSID1, &reg_id1, p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            continue;
        }
                                                                /* Obtain Phy ID 2 register value.                      */
        p_dev_api->Phy_RegRd(p_if, i, PHY_PHYSID2, &reg_id2, p_err);
        if (*p_err != NET_PHY_ERR_NONE) {
            continue;
        }

        if (((reg_id1 == 0)      && (reg_id2 == 0)) ||
            ((reg_id1 == 0xFFFF) && (reg_id2 == 0xFFFF))) {
            continue;
        } else {
            break;
        }
    }

    if (i > NET_PHY_ADDR_MAX) {
       *p_err = NET_PHY_ERR_ADDR_PROBE;
        return;
    }

    p_if_data           = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    p_if_data->Phy_Addr =                      i;               /* Store discovered Phy addr.                           */
   *p_err               =                      NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                   KSZ9031RNX MMD REGISTER ACCESS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         NetPhy_MMD_RegRd()
*
* Description : Provide indirect read access to MMD device addresses.
*
* Argument(s) : p_if         Pointer to interface to probe.
*               ---          Argument validated in NetPhy_Init().
*
*               dev_addr     MMD device address
*
*               reg_nbr      MMD register number of the MMD device address.
*
*               p_data       Pointer to variable to store returned register data.
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                                 NET_PHY_ERR_NONE               Physical layer's address successfully detected.
*                                 NET_PHY_ERR_TIMEOUT_REG_RD     Phy register read  time-out.
*                                 NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Return(s)   : none.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_MMD_RegRd (NET_IF      *p_if,
                                CPU_INT08U   dev_addr,
                                CPU_INT08U   reg_nbr,
                                CPU_INT16U  *p_data,
                                NET_ERR     *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT08U          phy_addr;


    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    phy_addr  =                      p_if_data->Phy_Addr;
                                                                /* Set up register address for MMD                      */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_CTRL, dev_addr, p_err);
    if(*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Select register number of MMD                        */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_REGDATA, reg_nbr, p_err);
    if(*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Select register data for MMD                         */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_CTRL, (dev_addr | MMD_CTRL_OP_MODE_DATA), p_err);
    if(*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Read data in MMD                                     */
    p_dev_api->Phy_RegRd(p_if, phy_addr, PHY_MMD_REGDATA, p_data, p_err);
}


/*
*********************************************************************************************************
*                                         NetPhy_MMD_RegWr()
*
* Description : Provide indirect write access to MMD device addresses.
*
* Argument(s) : p_if         Pointer to interface to probe.
*               ---          Argument validated in NetPhy_Init().
*
*               dev_addr     MMD device address
*
*               reg_nbr      MMD register number of the MMD device address.
*
*               reg_data     Data to write to the specified Phy register.   
*
*               p_err        Pointer to variable that will receive the return error code from this function :
*
*                                 NET_PHY_ERR_NONE               Physical layer's address successfully detected.
*                                 NET_PHY_ERR_TIMEOUT_REG_WR     Phy register write time-out.
*
* Return(s)   : none.
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_MMD_RegWr (NET_IF      *p_if,
                                CPU_INT08U   dev_addr,
                                CPU_INT08U   reg_nbr,
                                CPU_INT16U   reg_data,
                                NET_ERR     *p_err)
{
    NET_DEV_API_ETHER  *p_dev_api;
    NET_IF_DATA_ETHER  *p_if_data;
    CPU_INT08U          phy_addr;


    p_if_data = (NET_IF_DATA_ETHER *)p_if->IF_Data;
    p_dev_api = (NET_DEV_API_ETHER *)p_if->Dev_API;
    phy_addr  =                      p_if_data->Phy_Addr;
                                                                /* Set up register address for MMD                      */   
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_CTRL, dev_addr, p_err); 
    if(*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Select register number of MMD                        */ 
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_REGDATA, reg_nbr, p_err);
    if(*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Select register data for MMD                         */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_CTRL, (dev_addr | MMD_CTRL_OP_MODE_DATA) , p_err);
    if(*p_err != NET_PHY_ERR_NONE) {
        return;
    }
                                                                /* Write value to MMD                                   */
    p_dev_api->Phy_RegWr(p_if, phy_addr, PHY_MMD_REGDATA, reg_data, p_err);
}

#endif /* NET_IF_ETHER_MODULE_EN */

