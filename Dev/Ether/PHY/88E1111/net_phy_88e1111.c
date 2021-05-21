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
*                                   NETWORK PHYSICAL LAYER DRIVER
*
*                              10/100/1000 Gigabit Ethernet Transceiver
*                                          Marvell 88E1111
*
* Filename : net_phy_88e1111.c
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
#define    NET_PHY_88E1111_MODULE
#include  <Source/net.h>
#include  <IF/net_if_ether.h>
#include  "net_phy_88e1111.h"


#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  NET_PHY_ADDR_MAX                     31                /* 5 bit Phy address max value.                                 */
#define  NET_PHY_INIT_RESET_RETRIES            3                /* Check for successful reset x times                           */
#define  NET_PHY_INIT_AUTO_NEG_RETRIES        64                /* Attempt Auto-Negotiation x times                             */

/*
*********************************************************************************************************
*                                       REGISTER DEFINES
*********************************************************************************************************
*/

#define  PHY_BMCR                           0x00                /* Basic mode Control register                              */
#define  PHY_BMSR                           0x01                /* Basic Status register                                    */
#define  PHY_PHYSID1                        0x02                /* PHY Identifier Register #1                               */
#define  PHY_PHYSID2                        0x03                /* PHY Identifier Register #2                               */
#define  PHY_ANAR                           0x04                /* Auto-Negotiation Advertisement control reg.              */
#define  PHY_ANLPAR                         0x05                /* Auto-Negotiation Link partner ability reg.               */
#define  PHY_ANER                           0x06                /* Auto-Negotiation Expansion reg.                          */
#define  PHY_ANNPTR                         0x07                /* Auto-Negotiation Next page transmit reg.                 */
#define  PHY_SSR                            0x11                /* Specific status register                                 */

#define  PHY_1000Base_T_CTL_REG             0x09                /* 1000Base-T Control Register (Register 9 )                */


/*
*********************************************************************************************************
*                                         REGISTER BITS
*********************************************************************************************************
*/

                                                                /* ------------------ PHY_BMCR Register Bits -------------- */
#define  BMCR_RESV                        0x003F                /* Unused...                                                */
#define  BMCR__MSB_SPEED1000          DEF_BIT_06                /* Select 1000Mbps. RO bit, bit 6,13 =10 for 1000 MBit/s    */
#define  BMCR_CTST                    DEF_BIT_07                /* Collision test.                                          */
#define  BMCR_FULLDPLX                DEF_BIT_08                /* Full duplex.                                             */
#define  BMCR_ANRESTART               DEF_BIT_09                /* Auto negotiation restart.                                */
#define  BMCR_ISOLATE                 DEF_BIT_10                /* Disconnect Phy from MII.                                 */
#define  BMCR_PDOWN                   DEF_BIT_11                /* Power down.                                              */
#define  BMCR_ANENABLE                DEF_BIT_12                /* Enable auto negotiation.                                 */
#define  BMCR__LSB_SPEED1000          DEF_BIT_13                /* Select 1000Mbps. RO bit, bit 6,13 =10 for 1000 MBit/s    */
#define  BMCR_LOOPBACK                DEF_BIT_14                /* TXD loopback bits.                                       */
#define  BMCR_RESET                   DEF_BIT_15                /* Reset.                                                   */

#define  BMCR_SPEED_MASK             (DEF_BIT_06 | DEF_BIT_13)
#define  BMCR_SPEED10_MASK                0x0000
#define  BMCR_SPEED100_MASK           DEF_BIT_13
#define  BMCR_SPEED1000_MASK          DEF_BIT_06

                                                                /* ---------------- PHY_BMSR Register Bits ---------------- */
#define  BMSR_ERCAP                   DEF_BIT_00                /* Ext-reg capability.                                      */
#define  BMSR_JCD                     DEF_BIT_01                /* Jabber detected.                                         */
#define  BMSR_LSTATUS                 DEF_BIT_02                /* Link status.                                             */
#define  BMSR_ANEGCAPABLE             DEF_BIT_03                /* Able to do auto-negotiation.                             */
#define  BMSR_RFAULT                  DEF_BIT_04                /* Remote fault detected.                                   */
#define  BMSR_ANEGCOMPLETE            DEF_BIT_05                /* Auto-negotiation complete.                               */
#define  BMSR_10HALF                  DEF_BIT_11                /* Can do 10mbps, half-duplex.                              */
#define  BMSR_10FULL                  DEF_BIT_12                /* Can do 10mbps, full-duplex.                              */
#define  BMSR_100HALF                 DEF_BIT_13                /* Can do 100mbps, half-duplex.                             */
#define  BMSR_100FULL                 DEF_BIT_14                /* Can do 100mbps, full-duplex.                             */
#define  BMSR_100BASE4                DEF_BIT_15                /* Can do 100mbps, 4k packets.                              */

                                                                /* --------------- PHY_ANAR Register Bits ----------------- */
#define  ANAR_SLCT                        0x001F                /* Selector bits.                                           */
#define  ANAR_10HALF                  DEF_BIT_05                /* Try for 10mbps half-duplex.                              */
#define  ANAR_10FULL                  DEF_BIT_06                /* Try for 10mbps full-duplex.                              */
#define  ANAR_100HALF                 DEF_BIT_07                /* Try for 100mbps half-duplex.                             */
#define  ANAR_100FULL                 DEF_BIT_08                /* Try for 100mbps full-duplex.                             */
#define  ANAR_100BASE4                DEF_BIT_09                /* Try for 100mbps 4k packets.                              */
#define  ANAR_PAUSE                   DEF_BIT_10                /* Pause.                                                   */
#define  ANAR_ASYM_PAUSE              DEF_BIT_11                /* Asymetric Pause.                                         */
#define  ANAR_RFAULT                  DEF_BIT_13                /* Remote fault.                                            */
#define  ANAR_LPACK                   DEF_BIT_14                /* Ack link partners response.                              */
#define  ANAR_NPAGE                   DEF_BIT_15                /* Next page bit.                                           */

                                                                /* -------------- PHY_ANLPAR Register Bits ---------------- */
#define  ANLPAR_SLCT                      0x001F                /* Same as advertise selector.                              */
#define  ANLPAR_10HALF                DEF_BIT_05                /* Can do 10mbps half-duplex.                               */
#define  ANLPAR_10FULL                DEF_BIT_06                /* Can do 10mbps full-duplex.                               */
#define  ANLPAR_100HALF               DEF_BIT_07                /* Can do 100mbps half-duplex.                              */
#define  ANLPAR_100FULL               DEF_BIT_08                /* Can do 100mbps full-duplex.                              */
#define  ANLPAR_100BASE4              DEF_BIT_09                /* Can do 100mbps 4k packets.                               */
#define  ANLPAR_RFAULT                DEF_BIT_13                /* Link partner faulted.                                    */
#define  ANLPAR_LPACK                 DEF_BIT_14                /* Link partner acked us.                                   */
#define  ANLPAR_NPAGE                 DEF_BIT_15                /* Next page bit.                                           */


                                                                /* ---------- Specific status register bit masks ---------- */
#define  SSR_LINK                     DEF_BIT_10                /* Link state mask                                          */
                                                                /* Speed mask                                               */
#define  SSR_SPD_MASK            (DEF_BIT_15 | DEF_BIT_14)
#define  SSR_SPD_1000                 DEF_BIT_15                /* Speed 1000 MBit/s mask                                   */
#define  SSR_SPD_100                  DEF_BIT_14                /* Speed 100 MBit/s mask                                    */
#define  SSR_SPD_10                       0x0000                /* Speed 1000 MBit/s mask                                   */
#define  SSR_FD                       DEF_BIT_13                /* Full duplex mode mask                                    */


#define  PHY_ADV_1000_BASE_T_FD       DEF_BIT_09                /* 1000-BaseT Full duplex                                   */



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

static  void  NetPhy_Init        (NET_IF              *pif,
                                  NET_ERR             *perr);

static  void  NetPhy_EnDis       (NET_IF              *pif,
                                  CPU_BOOLEAN          en,
                                  NET_ERR             *perr);


static  void  NetPhy_LinkStateGet(NET_IF              *pif,
                                  NET_DEV_LINK_ETHER  *plink_state,
                                  NET_ERR             *perr);

static  void  NetPhy_LinkStateSet(NET_IF              *pif,
                                  NET_DEV_LINK_ETHER  *plink_state,
                                  NET_ERR             *perr);


static  void  NetPhy_AutoNegStart(NET_IF              *pif,
                                  NET_ERR             *perr);

static  void  NetPhy_AddrProbe   (NET_IF              *pif,
                                  NET_ERR             *perr);

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
                                                                                    /* 88E1111 phy API fnct ptrs :      */
const  NET_PHY_API_ETHER  NetPhy_API_88E1111 = { NetPhy_Init,                       /*   Init                           */
                                                 NetPhy_EnDis,                      /*   En/dis                         */
                                                 NetPhy_LinkStateGet,               /*   Link get                       */
                                                 NetPhy_LinkStateSet,               /*   Link set                       */
                                                 0                                  /*   ISR handler                    */
                                               };



/*
*********************************************************************************************************
*                                            NetPhy_Init()
*
* Description : Initialize Ethernet physical layer.
*
* Argument(s) : pif         Pointer to interface to initialize Phy.
*               ---         Argument checked in NetIF_Ether_IF_Start().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE                Ethernet physical layer successfully
*                                                                   initialized.
*                               NET_PHY_ERR_TIMEOUT_RESET       Phy reset          time-out.
*                               NET_PHY_ERR_TIMEOUT_AUTO_NEG    Auto-Negotiation   time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_RD      Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR      Phy register write time-out.
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
*********************************************************************************************************
*/

static  void  NetPhy_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_API_ETHER  *pdev_api;
    NET_PHY_CFG_ETHER  *pphy_cfg;
    NET_IF_DATA_ETHER  *pif_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          retries;
    CPU_INT08U          phy_addr;


    pdev_api  =  pif->Dev_API;
    pphy_cfg  = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pif_data  = (NET_IF_DATA_ETHER *)pif->IF_Data;

    phy_addr  =  pphy_cfg->BusAddr;                             /* Obtain user cfg'd Phy addr.                              */

    if (phy_addr == NET_PHY_ADDR_AUTO) {                        /* Automatic detection of Phy address enabled.              */
        NetPhy_AddrProbe(pif, perr);                            /* Attempt to automatically determine Phy addr.             */
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }
        phy_addr           = pif_data->Phy_Addr;
    } else {
        pif_data->Phy_Addr = phy_addr;                          /* Set Phy addr to cfg'd val.                               */
    }

                                                                /* ---------------------- RESET PHY ----------------------- */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, BMCR_RESET, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }


    pdev_api->Phy_RegRd(pif,                                    /* Rd ctrl reg, get reset bit.                              */
                        phy_addr,
                        PHY_BMCR,
                       &reg_val,
                        perr);

    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val &= BMCR_RESET;                                      /* Mask out reset status bit.                               */


    retries = NET_PHY_INIT_RESET_RETRIES;
    while ((reg_val == BMCR_RESET) && (retries > 0)) {          /* Wait for reset to complete.                              */
        KAL_Dly(200);

        pdev_api->Phy_RegRd(pif,
                            phy_addr,
                            PHY_BMCR,
                           &reg_val,
                            perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

        reg_val &= BMCR_RESET;
        retries--;
    }

    if (retries == 0) {
       *perr = NET_PHY_ERR_TIMEOUT_RESET;
        return;
    }


    *perr = NET_PHY_ERR_NONE;
}



/*
*********************************************************************************************************
*                                           NetPhy_EnDis()
*
* Description : Enable/disable the Phy.
*
* Argument(s) : pif         Pointer to interface to enable/disable Phy.
*               ---
*
*               en          Enable option :
*
*                               DEF_ENABLED                     Enable  Phy
*                               DEF_DISABLED                    Disable Phy
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE                Physical layer successfully enabled/disabled.
*                               NET_PHY_ERR_TIMEOUT_RESET       Phy reset          time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_RD      Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR      Phy register write time-out.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pphy_api->EnDis()',
*               NetIF_Ether_IF_Stop()  via 'pphy_api->EnDis()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_EnDis (NET_IF       *pif,
                            CPU_BOOLEAN   en,
                            NET_ERR      *perr)
{
    NET_DEV_API_ETHER  *pdev_api;
    NET_IF_DATA_ETHER  *pif_data;
    CPU_INT16U          reg_val;
    CPU_INT08U          phy_addr;


    pdev_api = pif->Dev_API;
    pif_data = pif->IF_Data;
    phy_addr = pif_data->Phy_Addr;                              /* Obtain Phy addr.                                         */

    pdev_api->Phy_RegRd(pif,                                    /* Obtain current control register value.                   */
                        phy_addr,
                        PHY_BMCR,
                       &reg_val,
                        perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    switch (en) {
        case DEF_DISABLED:
             reg_val |=  BMCR_PDOWN;                            /* Disable.                                                 */
             break;

        case DEF_ENABLED:
        default:
             reg_val &= ~BMCR_PDOWN;                            /* Enable.                                                  */
             break;
    }

    pdev_api->Phy_RegWr(pif,                                    /* Power up / down the Phy.                                 */
                        phy_addr,
                        PHY_BMCR,
                        reg_val,
                        perr);

    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

   *perr = NET_PHY_ERR_NONE;
}



/*
*********************************************************************************************************
*                                     NetPhy_LinkStateGet()
*
* Description : Get current Phy link state (speed & duplex).
*
* Argument(s) : pif             Pointer to interface to get link state.
*               ---             Argument validated in NetIF_IO_CtrlHandler().
*
*               plink_state     Pointer to structure that will receive the link state.
*
*               perr            Pointer to variable  that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE            No  error.
*                               NET_PHY_ERR_NULL_PTR        Pointer argument(s) passed NULL pointer(s).
*                               NET_PHY_ERR_TIMEOUT_RESET   Phy reset          time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_RD  Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR  Phy register write time-out.
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

static  void  NetPhy_LinkStateGet (NET_IF              *pif,
                                   NET_DEV_LINK_ETHER  *plink_state,
                                   NET_ERR             *perr)
{
    NET_DEV_API_ETHER  *pdev_api;
    NET_IF_DATA_ETHER  *pif_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          link_self;
    CPU_INT08U          phy_addr;
    NET_ERR             err;
    CPU_INT16U          speed_res;


    pdev_api = pif->Dev_API;
    pif_data = pif->IF_Data;
    phy_addr = pif_data->Phy_Addr;                              /* Obtain Phy addr.                                         */


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (plink_state == (NET_DEV_LINK_ETHER *)0) {
       *perr = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    plink_state->Spd    = NET_PHY_SPD_0;
    plink_state->Duplex = NET_PHY_DUPLEX_UNKNOWN;

                                                                /* --------------- OBTAIN CUR LINK STATUS  ---------------- */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMSR, &link_self, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }
                                                                /* Rd BMSR twice (see Note #1).                             */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMSR, &link_self, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    if ((link_self & BMSR_LSTATUS) == 0) {                      /* Chk if link down.                                        */
        *perr = NET_PHY_ERR_NONE;
         return;
    }


                                                                /* --------------- DETERMINE SPD AND DUPLEX --------------- */
                                                                /* Obtain AN settings.                                      */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    if ((reg_val & BMCR_ANENABLE) == 0) {                       /* IF AN disabled.                                          */
                                                                /* Determine spd.                                           */
        speed_res = reg_val & BMCR_SPEED_MASK;
        switch (speed_res) {
            case BMCR_SPEED10_MASK:
                 plink_state->Spd = NET_PHY_SPD_10;
                 break;

            case BMCR_SPEED100_MASK:
                 plink_state->Spd = NET_PHY_SPD_100;
                 break;

            case BMCR_SPEED1000_MASK:
                 plink_state->Spd = NET_PHY_SPD_1000;
                 break;

            default:
                 break;
        }

        if ((reg_val & BMCR_FULLDPLX) == 0) {                   /* Determine duplex.                                        */
            plink_state->Duplex = NET_PHY_DUPLEX_HALF;
        } else {
            plink_state->Duplex = NET_PHY_DUPLEX_FULL;
        }
    } else {
                                                                /* Obtain current link state from the Special status reg.   */
        pdev_api->Phy_RegRd(pif,
                            phy_addr,
                            PHY_SSR,
                            &reg_val,
                            &err);
        if (err != NET_PHY_ERR_NONE) {
            return;
        }


        switch(reg_val & SSR_SPD_MASK) {
            case SSR_SPD_1000 :                                 /* 1000 MBit/s                                              */
                 plink_state->Spd = NET_PHY_SPD_1000;
                 break;

            case SSR_SPD_100 :                                  /* 100 MBit/s                                               */
                 plink_state->Spd = NET_PHY_SPD_100;
                 break;

            case SSR_SPD_10 :                                   /* 10 MBit/s                                                */
                 plink_state->Spd = NET_PHY_SPD_10;
                 break;

        }
        if ((reg_val & SSR_FD)) {                               /* Full duplex?                                             */
            plink_state->Duplex = NET_PHY_DUPLEX_FULL;

        } else {
            plink_state->Duplex = NET_PHY_DUPLEX_HALF;
        }
    }

                                                                 /* Link established, update MAC settings.                  */
    pdev_api->IO_Ctrl((NET_IF   *) pif,
                      (CPU_INT08U) NET_IF_IO_CTRL_LINK_STATE_UPDATE,
                      (void     *) plink_state,
                      (NET_ERR  *)&err);

    *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetPhy_LinkStateSet()
*
* Description : Set current Phy link state (speed & duplex).
*
* Argument(s) : pif             Pointer to interface to get link state.
*               ---             Argument validated in NetIF_Start().
*
*               plink_state     Pointer to structure that will contain the desired link state.
*
*               perr            Pointer to variable  that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE            No  error.
*                               NET_PHY_ERR_NULL_PTR        Pointer argument(s) passed NULL pointer(s).
*                               NET_PHY_ERR_TIMEOUT_RESET   Phy reset          time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_RD  Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR  Phy register write time-out.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pphy_api->LinkStateSet()'.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetPhy_LinkStateSet (NET_IF              *pif,
                                   NET_DEV_LINK_ETHER  *plink_state,
                                   NET_ERR             *perr)
{
    NET_DEV_API_ETHER  *pdev_api;
    NET_IF_DATA_ETHER  *pif_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          spd;
    CPU_INT08U          duplex;
    CPU_INT08U          phy_addr;


    pdev_api =  pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (plink_state == (NET_DEV_LINK_ETHER *)0) {
       *perr = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    spd    = plink_state->Spd;
    duplex = plink_state->Duplex;

    if (((spd    != NET_PHY_SPD_10)      &&                     /* Enable AN if cfg invalid or any member set to AUTO.      */
         (spd    != NET_PHY_SPD_100)     &&
         (spd    != NET_PHY_SPD_1000))   ||
        ((duplex != NET_PHY_DUPLEX_HALF) &&
         (duplex != NET_PHY_DUPLEX_FULL))) {

        NetPhy_AutoNegStart(pif, perr);
        return;
    }

    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val &= ~BMCR_ANENABLE;                                  /* Clr AN enable bit.                                       */

    switch (spd) {                                              /* Set spd.                                                 */
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

    switch (duplex) {                                           /* Set duplex.                                              */
        case NET_PHY_DUPLEX_HALF:
             reg_val &= ~BMCR_FULLDPLX;
             break;

        case NET_PHY_DUPLEX_FULL:
             reg_val |=  BMCR_FULLDPLX;
             break;

        default:
             break;
    }
                                                                /* Cfg Phy.                                                 */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, reg_val | BMCR_RESET , perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

   *perr = NET_PHY_ERR_NONE;
}



/*
*********************************************************************************************************
*                                        NetPhy_AutoNegStart()
*
* Description : Start the Auto-Negotiation processs.
*
* Argument(s) : pif         Pointer to interface to start auto-negotiation.
*               ---         Argument validated in NetPhy_Init().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE                Physical layer successfully started.
*                               NET_PHY_ERR_TIMEOUT_REG_RD      Phy register read  time-out.
*                               NET_PHY_ERR_TIMEOUT_REG_WR      Phy register write time-out.
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

static  void  NetPhy_AutoNegStart (NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_API_ETHER  *pdev_api;
    NET_IF_DATA_ETHER  *pif_data;
    CPU_INT16U          reg_val;
    CPU_INT08U          phy_addr;
    CPU_INT16U          retries;


    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;
    pdev_api =  pif->Dev_API;

    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val |= BMCR_ANENABLE |
               BMCR_ANRESTART;
                                                                /* Restart Auto-Negotiation.                                */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    retries = NET_PHY_INIT_AUTO_NEG_RETRIES;

    do {                                                       /* Wait until the autonegotiation will be completed         */
        retries--;
        pdev_api->Phy_RegRd (pif, phy_addr, PHY_BMSR, &reg_val, perr);
        if(*perr != NET_PHY_ERR_NONE) {
            return;
        }

        if (reg_val & BMSR_ANEGCOMPLETE) {
            break;
        }

        KAL_Dly(200);

    } while (retries > 0);

    if(retries == 0) {
        *perr = NET_PHY_ERR_TIMEOUT_AUTO_NEG;
        return;
    }

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetPhy_AddrProbe()
*
* Description : Automatically detect Phy bus address.
*
* Argument(s) : pif         Pointer to interface to probe.
*               ---         Argument validated in NetPhy_Init().
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_PHY_ERR_NONE                Physical layer's address successfully
*                                                                   detected.
*                               NET_PHY_ERR_ADDR_PROBE          Unable to determine Phy address.
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

static  void  NetPhy_AddrProbe (NET_IF   *pif,
                                NET_ERR  *perr)
{
    NET_DEV_API_ETHER  *pdev_api;
    NET_IF_DATA_ETHER  *pif_data;
    CPU_INT16U          reg_id1;
    CPU_INT16U          reg_id2;
    CPU_INT08U          i;


    pdev_api = pif->Dev_API;
    for (i = 0; i <= NET_PHY_ADDR_MAX; i++) {
        pdev_api->Phy_RegRd(pif,                                /* Obtain Phy ID 1 register value.                          */
                            i,
                            PHY_PHYSID1,
                           &reg_id1,
                            perr);
        if (*perr != NET_PHY_ERR_NONE) {
             continue;
        }

        pdev_api->Phy_RegRd(pif,                                /* Obtain Phy ID 2 register value.                          */
                            i,
                            PHY_PHYSID2,
                           &reg_id2,
                            perr);
        if (*perr != NET_PHY_ERR_NONE) {
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
       *perr = NET_PHY_ERR_ADDR_PROBE;
        return;
    }

    pif_data = pif->IF_Data;
    pif_data->Phy_Addr = i;                                     /* Store discovered Phy addr.                               */

   *perr = NET_PHY_ERR_NONE;
}

#endif


