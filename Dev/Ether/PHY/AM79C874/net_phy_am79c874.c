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
*                                            AMD AM79C874
*
* Filename : net_phy_am79c874.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.02 (or more recent version) is included in the project build.
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
#define    NET_PHY_AM79C874_MODULE
#include  <Source/net.h>
#include  <IF/net_if_ether.h>
#include  "net_phy_am79c874.h"

#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  NET_PHY_ADDR_MAX                     31                /* 5 bit Phy address max value.             */

#define  NET_PHY_INIT_AUTO_NEG_RETRIES         3                /* Attempt Auto-Negotiation x times.        */
#define  NET_PHY_INIT_RESET_RETRIES            3                /* Check for successful reset x times.      */


/*
*********************************************************************************************************
*                                       REGISTER DEFINES
*********************************************************************************************************
*/
                                                                /* ------- Generic MII registers ---------- */
#define  MII_BMCR                           0x00                /* Basic mode control reg.                  */
#define  MII_BMSR                           0x01                /* Basic mode status reg.                   */
#define  MII_PHYSID1                        0x02                /* PHYS ID 1 reg.                           */
#define  MII_PHYSID2                        0x03                /* PHYS ID 2 reg.                           */
#define  MII_ANAR                           0x04                /* Advertisement control reg.               */
#define  MII_ANLPAR                         0x05                /* Link partner ability reg.                */
#define  MII_ANER                           0x06                /* Expansion reg.                           */
#define  MII_ANNPTR                         0x07                /* Next page transmit reg.                  */

                                                                /*----- AM79C874 Specific registers ------- */
#define  MII_MFR                            0x10                /* Misc. Features reg.                      */
#define  MII_ICSR                           0x11                /* Interrupt Control & Status reg.          */
#define  MII_DR                             0x12                /* Diagnostic reg.                          */
#define  MII_PMLR                           0x13                /* Power Management & Loopback reg.         */
#define  MII_MCR                            0x15                /* Mode Control reg.                        */
#define  MII_DC                             0x17                /* Disconnect Counter reg.                  */
#define  MII_REC                            0x18                /* Receive Error Counter reg.               */


/*
*********************************************************************************************************
*                                         REGISTER BITS
*********************************************************************************************************
*/
                                                                /* -------- MII_BMCR Register Bits -------- */
#define  BMCR_RESV                         0x007F               /* Unused...                                */
#define  BMCR_CTST                       DEF_BIT_07             /* Collision test.                          */
#define  BMCR_FULLDPLX                   DEF_BIT_08             /* Full duplex.                             */
#define  BMCR_ANRESTART                  DEF_BIT_09             /* Auto negotiation restart.                */
#define  BMCR_ISOLATE                    DEF_BIT_10             /* Disconnect Phy from MII.                 */
#define  BMCR_PDOWN                      DEF_BIT_11             /* Power down.                              */
#define  BMCR_ANENABLE                   DEF_BIT_12             /* Enable auto negotiation.                 */
#define  BMCR_SPEED100                   DEF_BIT_13             /* Select 100Mbps.                          */
#define  BMCR_LOOPBACK                   DEF_BIT_14             /* TXD loopback bits.                       */
#define  BMCR_RESET                      DEF_BIT_15             /* Reset.                                   */

                                                                /* -------- MII_BMSR Register Bits -------- */
#define  BMSR_ERCAP                      DEF_BIT_00             /* Ext-reg capability.                      */
#define  BMSR_JCD                        DEF_BIT_01             /* Jabber detected.                         */
#define  BMSR_LSTATUS                    DEF_BIT_02             /* Link status.                             */
#define  BMSR_ANEGCAPABLE                DEF_BIT_03             /* Able to do auto-negotiation.             */
#define  BMSR_RFAULT                     DEF_BIT_04             /* Remote fault detected.                   */
#define  BMSR_ANEGCOMPLETE               DEF_BIT_05             /* Auto-negotiation complete.               */
#define  BMSR_RESV                         0x07C0               /* Unused...                                */
#define  BMSR_10HALF                     DEF_BIT_11             /* Can do 10mbps, half-duplex.              */
#define  BMSR_10FULL                     DEF_BIT_12             /* Can do 10mbps, full-duplex.              */
#define  BMSR_100HALF                    DEF_BIT_13             /* Can do 100mbps, half-duplex.             */
#define  BMSR_100FULL                    DEF_BIT_14             /* Can do 100mbps, full-duplex.             */
#define  BMSR_100BASE4                   DEF_BIT_15             /* Can do 100mbps, 4k packets.              */

                                                                /* -------- MII_ANAR Register Bits -------- */
#define  ANAR_SLCT                         0x001F               /* Selector bits.                           */
#define  ANAR_CSMA                       DEF_BIT_04             /* Only selector supported.                 */
#define  ANAR_10HALF                     DEF_BIT_05             /* Try for 10mbps half-duplex.              */
#define  ANAR_10FULL                     DEF_BIT_06             /* Try for 10mbps full-duplex.              */
#define  ANAR_100HALF                    DEF_BIT_07             /* Try for 100mbps half-duplex.             */
#define  ANAR_100FULL                    DEF_BIT_08             /* Try for 100mbps full-duplex.             */
#define  ANAR_100BASE4                   DEF_BIT_09             /* Try for 100mbps 4k packets.              */
#define  ANAR_RESV                         0x1C00               /* Unused...                                */
#define  ANAR_RFAULT                     DEF_BIT_13             /* Say we can detect faults.                */
#define  ANAR_LPACK                      DEF_BIT_14             /* Ack link partners response.              */
#define  ANAR_NPAGE                      DEF_BIT_15             /* Next page bit.                           */

#define  ANAR_FULL       (ANAR_100FULL  | ANAR_10FULL  | ANAR_CSMA)
#define  ANAR_ALL        (ANAR_100BASE4 | ANAR_100FULL | ANAR_10FULL | ANAR_100HALF | ANAR_10HALF)

                                                                /* ------- MII_ANLPAR Register Bits ------- */
#define  ANLPAR_SLCT                       0x001F               /* Same as advertise selector.              */
#define  ANLPAR_10HALF                   DEF_BIT_05             /* Can do 10mbps half-duplex.               */
#define  ANLPAR_10FULL                   DEF_BIT_06             /* Can do 10mbps full-duplex.               */
#define  ANLPAR_100HALF                  DEF_BIT_07             /* Can do 100mbps half-duplex.              */
#define  ANLPAR_100FULL                  DEF_BIT_08             /* Can do 100mbps full-duplex.              */
#define  ANLPAR_100BASE4                 DEF_BIT_09             /* Can do 100mbps 4k packets.               */
#define  ANLPAR_RESV                       0x1C00               /* Unused...                                */
#define  ANLPAR_RFAULT                   DEF_BIT_13             /* Link partner faulted.                    */
#define  ANLPAR_LPACK                    DEF_BIT_14             /* Link partner acked us.                   */
#define  ANLPAR_NPAGE                    DEF_BIT_15             /* Next page bit.                           */

#define  ANLPAR_DUPLEX   (ANLPAR_10FULL  | ANLPAR_100FULL)
#define  ANLPAR_100      (ANLPAR_100FULL | ANLPAR_100HALF | ANLPAR_100BASE4)

                                                                /* -------- MII_ANER Register Bits -------- */
#define  ANER_NWAY                       DEF_BIT_00             /* Can do N-way auto-negotiation.           */
#define  ANER_LCWP                       DEF_BIT_01             /* Got new RX page code word.               */
#define  ANER_ENABLENPAGE                DEF_BIT_02             /* This enables npage word.                 */
#define  ANER_NPCAPABLE                  DEF_BIT_03             /* Link partner supports npage.             */
#define  ANER_MFAULTS                    DEF_BIT_04             /* Multiple faults detected.                */
#define  ANER_RESV                         0xFFE0               /* Unused...                                */

                                                                /* -------- MII_MISC Register Bits -------- */
#define  MISC_RX_CLK_CTRL                DEF_BIT_00             /* Recv clk ctrl.                           */
#define  MISC_REV_POL                    DEF_BIT_04             /* Reverse Polarity.                        */
#define  MISC_AUTO_POL_DIS               DEF_BIT_05             /* Auto polarity disable.                   */
#define  MISC_GPIO0_DIR                  DEF_BIT_06             /* GPIO0 Dir.                               */
#define  MISC_GPIO0_DATA                 DEF_BIT_07             /* GPIO0 Data.                              */
#define  MISC_GPIO1_DIR                  DEF_BIT_08             /* GPIO1 Dir.                               */
#define  MISC_GPIO1_DATA                 DEF_BIT_09             /* GPIO1 Data.                              */
#define  MISC_10BT_LOOPBACK              DEF_BIT_10             /* 10 BASET loop back.                      */
#define  MISC_SQE_TEST_INHIBIT           DEF_BIT_11             /* SQE test inhibit.                        */
#define  MISC_INTR_LEVL                  DEF_BIT_14             /* Interrupt level. 1 = Active high.        */
#define  MISC_REPEATER                   DEF_BIT_15             /* Repeater mode.                           */

                                                                /* -------- MII_ICSR Register Bits -------- */
#define  ICSR_ANEG_COMP_INT              DEF_BIT_00             /* Auto-Negotiation Complete.               */
#define  ICSR_R_FAULT_INT                DEF_BIT_01             /* Remote Fault detected.                   */
#define  ICSR_LINK_NOT_OK_INT            DEF_BIT_02             /* Link not OK (Changed from UP to DOWN).   */
#define  ICSR_LP_ACK_INT                 DEF_BIT_03             /* FLP w/ ACK recv'd.                       */
#define  ICSR_PD_FAULT_INT               DEF_BIT_04             /* Parallel fault detected.                 */
#define  ICSR_PAGE_RX_INT                DEF_BIT_05             /* Link Partner Page recv'd.                */
#define  ICSR_RX_ER_INT                  DEF_BIT_06             /* RX_ER state transition to HIGH.          */
#define  ICSR_JABBER_INT                 DEF_BIT_07             /* Jabber detected.                         */
#define  ICSR_ANEG_COMP_IE               DEF_BIT_08             /* Auto-Negotiation Complete Int En.        */
#define  ICSR_R_FAULT_IE                 DEF_BIT_09             /* Remote Fault Int. En.                    */
#define  ICSR_LINK_NOT_OK_IE             DEF_BIT_10             /* Link Status NOT OK Int. En.              */
#define  ICSR_LP_ACK_IE                  DEF_BIT_11             /* Link Partner Acknowledge Int. En.        */
#define  ICSR_PD_FAULT_IE                DEF_BIT_12             /* Parallel detection fault Int. En.        */
#define  ICSR_PAGE_RX_IE                 DEF_BIT_13             /* Page recv'd Int. En.                     */
#define  ICSR_RX_ER_IE                   DEF_BIT_14             /* Rx Err Int. En.                          */
#define  ICSR_JABBER_IE                  DEF_BIT_15             /* Jabber Int. En.                          */

                                                                /* --------- MII_DR Register Bits --------- */
#define  DR_RX_LOCK                      DEF_BIT_08             /* Valid Rx signal and PLL has locked.      */
#define  DR_RX_PASS                      DEF_BIT_09             /* Valid Rx signal, or manchester detected. */
#define  DR_DATA_RATE                    DEF_BIT_10             /* Auto-Neg Data Rate result. 1 = 100Mbps.  */
#define  DR_DPLX                         DEF_BIT_11             /* Auto-Neg Duplex result. 1 = FULL.        */


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


static  void  NetPhy_ISR_Handler (NET_IF              *pif);


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
                                                                                    /* AM79C874 phy API fnct ptrs :     */
const  NET_PHY_API_ETHER  NetPhy_API_AM79C874 = { NetPhy_Init,                      /*   Init                           */
                                                  NetPhy_EnDis,                     /*   En/dis                         */
                                                  NetPhy_LinkStateGet,              /*   Link get                       */
                                                  NetPhy_LinkStateSet,              /*   Link set                       */
                                                  NetPhy_ISR_Handler                /*   ISR handler                    */
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
    NET_DEV_API_ETHER   *pdev_api;
    NET_PHY_CFG_ETHER   *pphy_cfg;
    NET_IF_DATA_ETHER   *pif_data;
    CPU_INT16U           reg_val;
    CPU_INT16U           retries;
    CPU_INT08U           phy_addr;


    pdev_api  = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data  = (NET_IF_DATA_ETHER *)pif->IF_Data;
    pphy_cfg  = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;

    phy_addr  =  pphy_cfg->BusAddr;

    if (phy_addr == NET_PHY_ADDR_AUTO) {                        /* Automatic detection of Phy address enabled.          */
        NetPhy_AddrProbe(pif, perr);                            /* Attempt to automatically determine Phy addr.         */
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

        phy_addr           = pif_data->Phy_Addr;
    } else {
        pif_data->Phy_Addr = phy_addr;                          /* Set Phy addr to cfg'd val.                           */
    }
                                                                /* -------------------- RESET PHY --------------------- */
                                                                /* Reset Phy.                                           */
    pdev_api->Phy_RegWr(pif, phy_addr, MII_BMCR, BMCR_RESET, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }
                                                                /* Rd ctrl reg, get reset bit.                          */
    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val &= BMCR_RESET;                                      /* Mask out reset status bit.                           */


    retries = NET_PHY_INIT_RESET_RETRIES;
    while ((reg_val == BMCR_RESET) && (retries > 0)) {          /* Wait for reset to complete.                          */
        KAL_Dly(2);

        pdev_api->Phy_RegRd(pif, phy_addr, MII_BMCR, &reg_val, perr);
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

                                                                /* ---------------- ENABLE INTERRUPTS ----------------- */
    pdev_api->Phy_RegRd(pif,                                    /* Determine current Int. pin active level.             */
                        phy_addr,
                        MII_ICSR,
                       &reg_val,
                        perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val |= MISC_INTR_LEVL;                                  /* Cfg Int. pin active level high.                      */
    pdev_api->Phy_RegWr(pif,
                        phy_addr,
                        MII_ICSR,
                        reg_val,
                        perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    pdev_api->Phy_RegRd(pif,                                    /* Determine current Int mask.                          */
                        phy_addr,
                        MII_ICSR,
                       &reg_val,
                        perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val |= ICSR_LINK_NOT_OK_IE;                             /* Unmask Link Not OK Int.                              */
    pdev_api->Phy_RegWr(pif,
                        phy_addr,
                        MII_ICSR,
                        reg_val,
                        perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }
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


    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;
                                                                /* Get cur ctrl reg val.                                */
    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    switch (en) {
        case DEF_DISABLED:
             reg_val |=  BMCR_PDOWN;                            /* Dis Phy.                                             */
             break;


        case DEF_ENABLED:
        default:
             reg_val &= ~BMCR_PDOWN;                            /* En  Phy.                                             */
             break;
    }
                                                                /* Pwr up / down the Phy.                               */
    pdev_api->Phy_RegWr(pif, phy_addr, MII_BMCR, reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

   *perr = NET_PHY_ERR_NONE;
}



/*
*********************************************************************************************************
*                                     NetPhy_LinkStateGet()
*
* Description : Obtains current Phy link state (speed and duplex).
*
* Argument(s) : pif             Pointer to the interface requiring service.
*
*               plink_state     Pointer to link state structure to store return information.
*
*               perr            Pointer to return error code.
*                                   NET_PHY_ERR_NONE            No  error.
*                                   NET_PHY_ERR_NULL_PTR        Pointer argument(s) passed NULL pointer(s).
*                                   NET_PHY_ERR_TIMEOUT_RESET   Phy reset          time-out.
*                                   NET_PHY_ERR_TIMEOUT_REG_RD  Phy register read  time-out.
*                                   NET_PHY_ERR_TIMEOUT_REG_WR  Phy register write time-out.
*
* Caller(s)   : Device driver IOCtrl().
*
* Return(s)   : none.
*
* Note(s)     : (1) Some Phy's have the link status field latched in the BMSR register.  The link status remains low
*                   after a temporary link failure until it is read. To retrieve the current link status, BMSR must
*                   be read twice.
*
*               (2) Current link state should be obtained by calling this function through the NetIF layer.
*                   See 'net_if.c  NetIF_IO_Ctrl()'.
*
*               (3) Not all MAC's require link state synchronization.  Therefore, the call to
*                   IO_Ctrl() is treated as a best effort attempt to inform the MAC of a potential link
*                   state change;  therefore, the return error code is ignored.
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
    CPU_INT16U          link_partner;
    CPU_INT08U          phy_addr;
    NET_ERR             err;


    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (plink_state == (NET_DEV_LINK_ETHER *)0) {
       *perr = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    plink_state->Spd    = NET_PHY_SPD_0;
    plink_state->Duplex = NET_PHY_DUPLEX_UNKNOWN;


                                                                /* ------------- OBTAIN CUR LINK STATUS  -------------- */
    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMSR, &link_self, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }
                                                                /* Rd BMSR twice (see Note #1).                         */
    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMSR, &link_self, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    if ((link_self & BMSR_LSTATUS) == 0) {                      /* Chk if link down.                                    */
        *perr = NET_PHY_ERR_NONE;
         return;
    }
                                                                /* ------------- DETERMINE SPD AND DUPLEX ------------- */
                                                                /* Obtain AN settings.                                  */
    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    if ((reg_val & BMCR_ANENABLE) == 0) {                       /* IF AN disabled.                                      */
        if ((reg_val & BMCR_SPEED100) == 0) {                   /* Determine spd.                                       */
            plink_state->Spd = NET_PHY_SPD_10;
        } else {
            plink_state->Spd = NET_PHY_SPD_100;
        }

        if ((reg_val & BMCR_FULLDPLX) == 0) {                   /* Determine duplex.                                    */
            plink_state->Duplex = NET_PHY_DUPLEX_HALF;
        } else {
            plink_state->Duplex = NET_PHY_DUPLEX_FULL;
        }
    } else {
                                                                /* AN enabled. Get self link capabilities.              */
        pdev_api->Phy_RegRd(pif, phy_addr, MII_ANAR, &link_self, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }
                                                                /* Get link partner link capabilities.                  */
        pdev_api->Phy_RegRd(pif, phy_addr, MII_ANLPAR, &link_partner, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

        link_partner &= (ANLPAR_100BASE4 |                      /* Preserve link status bits.                           */
                         ANLPAR_100FULL  |
                         ANLPAR_100HALF  |
                         ANLPAR_10FULL   |
                         ANLPAR_10HALF);

        link_self    &= link_partner;                           /* Match self capabilties to partner capabilities.      */

        if (link_self           >= ANLPAR_100FULL) {            /* Determine most likely link state.                    */
             plink_state->Spd    = NET_PHY_SPD_100;
             plink_state->Duplex = NET_PHY_DUPLEX_FULL;
        } else if (link_self    >= ANLPAR_100HALF) {
             plink_state->Spd    = NET_PHY_SPD_100;
             plink_state->Duplex = NET_PHY_DUPLEX_HALF;
        } else if (link_self    >= ANLPAR_10FULL) {
             plink_state->Spd    = NET_PHY_SPD_10;
             plink_state->Duplex = NET_PHY_DUPLEX_FULL;
        } else {
             plink_state->Spd    = NET_PHY_SPD_10;
             plink_state->Duplex = NET_PHY_DUPLEX_HALF;
        }
    }
                                                                /* Link established, update MAC settings.               */
    pdev_api->IO_Ctrl((NET_IF   *) pif,
                      (CPU_INT08U) NET_IF_IO_CTRL_LINK_STATE_UPDATE,
                      (void     *) plink_state,
                      (NET_ERR  *)&err);                        /* Ignore dev update err (see Note #3).                 */

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


    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
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

    if (((spd    != NET_PHY_SPD_10 )     &&                     /* Enable AN if cfg invalid or any member set to AUTO.  */
         (spd    != NET_PHY_SPD_100))    ||
        ((duplex != NET_PHY_DUPLEX_HALF) &&
         (duplex != NET_PHY_DUPLEX_FULL))) {
        NetPhy_AutoNegStart(pif, perr);
        return;
    }

    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val &= ~BMCR_ANENABLE;                                  /* Clr AN enable bit.                                   */

    switch (spd) {                                              /* Set spd.                                             */
        case NET_PHY_SPD_10:
             reg_val &= ~BMCR_SPEED100;
             break;

        case NET_PHY_SPD_100:
             reg_val |=  BMCR_SPEED100;
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
    pdev_api->Phy_RegWr(pif, phy_addr, MII_BMCR, reg_val, perr);
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


    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;

    pdev_api->Phy_RegRd(pif, phy_addr, MII_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val |= BMCR_ANENABLE |
               BMCR_ANRESTART;
                                                                /* Restart Auto-Negotiation.                            */
    pdev_api->Phy_RegWr(pif, phy_addr, MII_BMCR, reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
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


    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;

    for (i = 0; i <= NET_PHY_ADDR_MAX; i++) {
                                                                /* Get Phy ID #1 reg val.                               */
        pdev_api->Phy_RegRd(pif, i, MII_PHYSID1, &reg_id1, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             continue;
        }
                                                                /* Get Phy ID #2 reg val.                               */
        pdev_api->Phy_RegRd(pif, i, MII_PHYSID2, &reg_id2, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             continue;
        }

        if (((reg_id1 == 0x0000) && (reg_id2 == 0x0000)) ||
            ((reg_id1 == 0x3FFF) && (reg_id2 == 0x3FFF)) ||
            ((reg_id1 == 0x3FFF) && (reg_id2 == 0xFFFF)) ||
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

    pif_data           = (NET_IF_DATA_ETHER *)pif->IF_Data;
    pif_data->Phy_Addr =  i;

   *perr = NET_PHY_ERR_NONE;
}



/*
*********************************************************************************************************
*                                         NetPhy_ISR_Handler()
*
* Description : (1) Process Phy interrupt :
*                   (a) Demux interrupt source.
*                   (b) Process interrupt
*                   (c) Clear Phy interrupt source
*                   (d) Update EMAC link state
*
* Argument(s) : pif     Pointer to interface requiring service.
*               ---
* Return(s)   : none.
*
* Caller(s)   : NetIF_ISR_Handler().
*
* Note(s)     : (2) This function has been implemented for example purposes only.  The generic
*                   Phy driver does NOT support interrupts.
*
*               (3) This function assumes that interrupts are previously enabled within
*                   NetPhy_Init().  However, this is NOT the case for the generic Phy driver.
*
*               (4) If a link state change has occured, the driver SHOULD call the associated
*                   Net Dev driver IO_Ctl() function with the NET_IF_IO_CTRL_LINK_STATE_UPDATE
*                   option specified.  A NON GENERIC example has been provided below.
*
*               (5) The NET_DEV_LINK_ETHER structure members Spd and Duplex must be populated as follows:
*                   (a) Spd :
*                           NET_PHY_SPD_0
*                           NET_PHY_SPD_10
*                           NET_PHY_SPD_100
*                           NET_PHY_SPD_1000
*
*                   (b) Duplex :
*                           NET_PHY_DUPLEX_UNKNOWN
*                           NET_PHY_DUPLEX_HALF
*                           NET_PHY_DUPLEX_FULL
*
*               (6) Not all MAC's require link state synchronization.  Therefore, the call to
*                   IO_Ctrl() is treated as a best effort attempt to inform the MAC of a potential link
*                   state change;  therefore, the return error code is ignored.
*********************************************************************************************************
*/

static  void  NetPhy_ISR_Handler (NET_IF  *pif)
{
                                                                /* Start of non generic Phy Int. example code.          */
    NET_DEV_API_ETHER   *pdev_api;
    NET_IF_DATA_ETHER   *pif_data;
    NET_DEV_LINK_ETHER   plink_state;
    CPU_INT16U           reg_val;
    CPU_INT08U           phy_addr;
    NET_ERR              err;

                                                                /* ----------------- OBTAIN PHY ADDR ------------------ */
    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;

                                                                /* ----------------- DEMUX ISR TYPE ------------------- */
    pdev_api->Phy_RegRd(pif,                                    /* Read Phy ISR status reg.                             */
                        phy_addr,
                        MII_ICSR,
                       &reg_val,
                       &err);
    if (err != NET_PHY_ERR_NONE) {
        return;
    }

                                                                /* ----------- DETERMINE LINK SPD & DUPLEX ------------ */
    if ((reg_val & ICSR_LINK_NOT_OK_INT) > 0) {                 /* Check for link state change interrupt.               */
        pdev_api->Phy_RegRd(pif,                                /* Obtain current link state.                           */
                            phy_addr,
                            MII_DR,
                           &reg_val,
                           &err);
        if (err != NET_PHY_ERR_NONE) {
             return;
        }

        if ((reg_val & DR_RX_PASS) > 0) {                       /* Link up?                                             */
            if ((reg_val & DR_DATA_RATE) > 0) {                 /* 100 Mbps?                                            */
                plink_state.Spd = NET_PHY_SPD_100;
            } else {
                plink_state.Spd = NET_PHY_SPD_10;
            }
            if ((reg_val & DR_DPLX) > 0) {                      /* Full duplex?                                         */
                plink_state.Duplex = NET_PHY_DUPLEX_FULL;
            } else {
                plink_state.Duplex = NET_PHY_DUPLEX_HALF;
            }

        } else {                                                /* Link down.                                           */
            plink_state.Spd    = NET_PHY_SPD_0;
            plink_state.Duplex = NET_PHY_DUPLEX_UNKNOWN;
            return;
        }

                                                                /* ------------------- UPDATE EMAC -------------------- */
                                                                /* Link down, update MAC settings (see Note #6).        */
        pdev_api->IO_Ctrl((NET_IF   *) pif,
                          (CPU_INT08U) NET_IF_IO_CTRL_LINK_STATE_UPDATE,
                          (void     *)&plink_state,
                          (NET_ERR  *)&err);
    }
}

#endif  /* NET_IF_ETHER_MODULE_EN */

