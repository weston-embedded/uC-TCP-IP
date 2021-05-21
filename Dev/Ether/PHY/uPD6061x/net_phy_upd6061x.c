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
*                                        GENERIC ETHERNET PHY
*
* Filename : net_phy_upd6061x.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V3.00 (or more recent version) is included in the project build.
*
*            (2) The (R)MII interface port is assumed to be part of the host EMAC.  Therefore, (R)MII
*                reads/writes MUST be performed through the network device API interface via calls to
*                function pointers 'Phy_RegRd()' & 'Phy_RegWr()'.
*
*            (3) Interrupt support is Phy specific, therefore the generic Phy driver does NOT support
*                interrupts.  However, interrupt support is easily added to the generic Phy driver &
*                thus the ISR handler has been prototyped and & populated within the function pointer
*                structure for example purposes.
*
*            (4) Does NOT support 1000Mbps Phy.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <Source/net.h>
#include  <IF/net_if_ether.h>
#include  "net_phy_upd6061x.h"


/*
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*/

#define  NET_PHY_ADDR_MAX                     31u               /* 5 bit Phy address max value                          */
#define  NET_PHY_INIT_AUTO_NEG_RETRIES         3u               /* Attempt Auto-Negotiation x times                     */
#define  NET_PHY_INIT_RESET_RETRIES            3u               /* Check for successful reset x times                   */


/*
*********************************************************************************************************
*                                       REGISTER DEFINES
*********************************************************************************************************
*/

                                                         /* ----------------- BASIC REGISTERS ------------------ */
#define  PHY_BMCR                           0x00u               /* Basic mode control reg                               */
#define  PHY_BMSR                           0x01u               /* Basic mode status reg                                */
#define  PHY_PHYSID1                        0x02u               /* PHYS ID 1 reg (0xB824)                               */
#define  PHY_PHYSID2                        0x03u               /* PHYS ID 2 reg (0x2824)                               */
#define  PHY_ANAR                           0x04u               /* Advertisement control reg                            */
#define  PHY_ANLPABPR                       0x05u               /* Link partner ability (base page) reg                 */
#define  PHY_ANLPANPR                       0x05u               /* Link partner ability (next page) reg                 */
#define  PHY_ANER                           0x06u               /* Expansion reg                                        */
#define  PHY_ANNPTR                         0x07u               /* Next page transmit reg                               */

                                                                /* ----------------- OTHER REGISTERS ------------------ */
#define  PHY_REVIDR                         0x10u               /* Silicon Rev ID reg (0x0040)                          */
#define  PHY_MCSR                           0x11u               /* Mode Control/Status reg                              */
#define  PHY_SPMR                           0x12u               /* Special Modes reg                                    */
#define  PHY_EBSR                           0x13u               /* Elastic Buffer Status reg                            */
#define  PHY_BERCR                          0x17u               /* Bit Error Counter reg                                */
#define  PHY_FEQMR                          0x18u               /* FEQ Moniter reg                                      */
#define  PHY_DCSR                           0x19u               /* Diagnosis Control/Status reg                         */
#define  PHY_DCNTR                          0x1Au               /* Diagnosis Counter reg                                */
#define  PHY_SPCSIR                         0x1Bu               /* Special Control/Status Indications reg               */
#define  PHY_ISFR                           0x1Du               /* Interrupt Source Flags reg                           */
#define  PHY_IER                            0x1Eu               /* Interrupt Enable reg                                 */
#define  PHY_SPCSR                          0x1Fu               /* Special Control/Status reg                           */


/*
*********************************************************************************************************
*                                         REGISTER BITS
*********************************************************************************************************
*/
                                                                /* -------------- PHY_BMCR REGISTER BITS -------------- */
#define BMCR_RESET                    DEF_BIT_15                /* Reset                                                */
#define BMCR_LOOPBACK                 DEF_BIT_14                /* TXD loopback bits                                    */
#define BMCR_SPEED100                 DEF_BIT_13                /* Select 100Mbps                                       */
#define BMCR_ANENABLE                 DEF_BIT_12                /* Enable auto negotiation                              */
#define BMCR_PDOWN                    DEF_BIT_11                /* Power down                                           */
#define BMCR_ISOLATE                  DEF_BIT_10                /* Disconnect Phy from MII                              */
#define BMCR_ANRESTART                DEF_BIT_09                /* Auto negotiation restart                             */
#define BMCR_FULLDPLX                 DEF_BIT_08                /* Full duplex                                          */
#define BMCR_CTST                     DEF_BIT_07                /* Collision test                                       */
#define BMCR_RESV                         0x007Fu               /* Unused                                               */

                                                                /* -------------- PHY_BMSR REGISTER BITS -------------- */
#define BMSR_100BASE4                 DEF_BIT_15                /* Can do 100mbps, 4k packets (not supported)           */
#define BMSR_100FULL                  DEF_BIT_14                /* Can do 100mbps, full-duplex                          */
#define BMSR_100HALF                  DEF_BIT_13                /* Can do 100mbps, half-duplex                          */
#define BMSR_10FULL                   DEF_BIT_12                /* Can do 10mbps, full-duplex                           */
#define BMSR_10HALF                   DEF_BIT_11                /* Can do 10mbps, half-duplex                           */
#define BMSR_RESV                         0x07C0u               /* Unused                                               */
#define BMSR_ANEGCOMPLETE             DEF_BIT_05                /* Auto-negotiation complete                            */
#define BMSR_RFAULT                   DEF_BIT_04                /* Remote fault detected                                */
#define BMSR_ANEGCAPABLE              DEF_BIT_03                /* Able to do auto-negotiation                          */
#define BMSR_LSTATUS                  DEF_BIT_02                /* Link status                                          */
#define BMSR_JCD                      DEF_BIT_01                /* Jabber detected                                      */
#define BMSR_ERCAP                    DEF_BIT_00                /* Ext-reg capability                                   */

                                                                /* -------------- PHY_ANAR REGISTER BITS -------------- */
#define ANAR_NPAGE                    DEF_BIT_15                /* Next page bit                                        */
#define ANAR_RESV                         0x5000u               /* Unused (bits 14 & 12)                                */
#define ANAR_RFAULT                   DEF_BIT_13                /* Say we can detect faults                             */
#define ANAR_PAUSE                        0x0C00u               /* Pause operation bits                                 */
#define ANAR_100BASE4                 DEF_BIT_09                /* Try for 100mbps 4k packets (not supported)           */
#define ANAR_100FULL                  DEF_BIT_08                /* Try for 100mbps full-duplex                          */
#define ANAR_100HALF                  DEF_BIT_07                /* Try for 100mbps half-duplex                          */
#define ANAR_10FULL                   DEF_BIT_06                /* Try for 10mbps full-duplex                           */
#define ANAR_10HALF                   DEF_BIT_05                /* Try for 10mbps half-duplex                           */
#define ANAR_SLCT                         0x001Fu               /* Selector bits                                        */

#define ANAR_FULL               (ANAR_100FULL  | ANAR_10FULL  | ANAR_CSMA)
#define ANAR_ALL                (ANAR_100BASE4 | ANAR_100FULL | ANAR_10FULL | ANAR_100HALF | ANAR_10HALF)

                                                                /* ------------ PHY_ANLPABPR REGISTER BITS ------------ */
#define ANLPABPR_NPAGE                DEF_BIT_15                /* Next page bit                                        */
#define ANLPABPR_LPACK                DEF_BIT_14                /* Link partner ack'ed us                               */
#define ANLPABPR_RFAULT               DEF_BIT_13                /* Link partner faulted                                 */
#define ANLPABPR_RESV                     0x1800u               /* Unused                                               */
#define ANLPABPR_PAUSE                DEF_BIT_10                /* Pause supported                                      */
#define ANLPABPR_100BASE4             DEF_BIT_09                /* Can do 100mbps 4k packets                            */
#define ANLPABPR_100FULL              DEF_BIT_08                /* Can do 100mbps full-duplex                           */
#define ANLPABPR_100HALF              DEF_BIT_07                /* Can do 100mbps half-duplex                           */
#define ANLPABPR_10FULL               DEF_BIT_06                /* Can do 10mbps full-duplex                            */
#define ANLPABPR_10HALF               DEF_BIT_05                /* Can do 10mbps half-duplex                            */
#define ANLPABPR_SLCT                     0x001Fu               /* Same as advertise selector                           */

#define ANLPABPR_DUPLEX         (ANLPABPR_10FULL  | ANLPABPR_100FULL)
#define ANLPABPR_100            (ANLPABPR_100FULL | ANLPABPR_100HALF | ANLPABPR_100BASE4)

                                                                /* ------------ PHY_ANLPANPR REGISTER BITS ------------ */
#define ANLPANPR_NPAGE                DEF_BIT_15                /* Next page will follow                                */
#define ANLPANPR_LPACK                DEF_BIT_14                /* Link partner ack'ed us                               */
#define ANLPANPR_MPAGE                DEF_BIT_13                /* Message page                                         */
#define ANLPANPR_LPACK2               DEF_BIT_12                /* Will comply with message                             */
#define ANLPANPR_TOGGLE               DEF_BIT_11                /* Previous value was 0                                 */
#define ANLPANPR_CODE                     0x07FFu               /* 11-bit code word received from link partner          */

                                                                /* -------------- PHY_ANER REGISTER BITS -------------- */
#define ANER_RESV                         0xFFE0u               /* Unused                                               */
#define ANER_FAULT                    DEF_BIT_04                /* Fault detected                                       */
#define ANER_LPNPABLE                 DEF_BIT_03                /* Link partner supports npage                          */
#define ANER_NPABLE                   DEF_BIT_02                /* Local device supports npage                          */
#define ANER_RXPAGE                   DEF_BIT_01                /* New page received                                    */
#define ANER_LPANABLE                 DEF_BIT_00                /* Link partner supports auto-negotiation               */

                                                                /* ------------- PHY_ANNPTR REGISTER BITS ------------- */
#define ANNPTR_NPAGE                  DEF_BIT_15                /* Next page exists                                     */
#define ANNPTR_RESV                   DEF_BIT_14                /* Unused                                               */
#define ANNPTR_MPAGE                  DEF_BIT_13                /* Message page                                         */
#define ANNPTR_LPACK2                 DEF_BIT_12                /* Will comply with message                             */
#define ANNPTR_TOGGLE                 DEF_BIT_11                /* Previous value was 0                                 */
#define ANNPTR_CODE                       0x07FFu               /* 11-bit code word to send to link partner             */


                                                                /* -------------- PHY_MCSR REGISTER BITS -------------- */
#define MCSR_RESV                         0x9429u               /* Unused (bits 15, 12, 10, 5, 3, 0)                    */
#define MCSR_FASTRIP                  DEF_BIT_14                /* PHYT_10 test mode                                    */
#define MCSR_EDPWDNEN                 DEF_BIT_13                /* Energy detect power-down enable                      */
#define MCSR_LOWSQEN                  DEF_BIT_11                /* Low squelch signal                                   */
#define MCSR_FARLPBKEN                DEF_BIT_09                /* Remote loopback enable                               */
#define MCSR_FASTEST                  DEF_BIT_08                /* Auto-negotiation test mode                           */
#define MCSR_AMDIXEN                  DEF_BIT_07                /* Auto-detect MDI/MDIX mode                            */
#define MCSR_MDIXMD                   DEF_BIT_06                /* MDIX mode                                            */
#define MCSR_DCDPATEN                 DEF_BIT_04                /* Enables DCD measuring pattern generation             */
#define MCSR_FORCELINK                DEF_BIT_02                /* Force 100BASE-X link active (only use when testing)  */
#define MCSR_ENERGYON                 DEF_BIT_01                /* Indicates if energy is detected on the line          */

                                                                /* -------------- PHY_SPMR REGISTER BITS -------------- */
#define SPMR_RESV                         0x8D00u               /* Unused (bits 15, 12, 11, 9)                          */
#define SPMR_MIIMD                    DEF_BIT_14                /* RMII mode                                            */
#define SPMR_CLKFRQSEL                DEF_BIT_13                /* Clock frequency determined by SPMR_MIIMD             */
#define SPMR_FXMD                     DEF_BIT_10                /* Enable FX mode                                       */
#define SPMR_PHYMD                        0x01E0u               /* Phy mode of operation                                */
#define SPMR_ADDDEV                       0x0018u               /* Phy address upper two bits                           */
#define SPMR_ADDMOD                       0x0007u               /* Phy address lower three bits                         */

                                                                /* -------------- PHY_EBSR REGISTER BITS -------------- */
#define EBSR_RESV                         0xFF0Fu               /* Unused (bits 15:8, 3:0)                              */
#define EBSR_TXOVF                    DEF_BIT_07                /* Transmitter elastic overflow                         */
#define EBSR_TXUDF                    DEF_BIT_06                /* Transmitter elastic underflow                        */
#define EBSR_RXOVF                    DEF_BIT_05                /* Receiver elastic overflow                            */
#define EBSR_RXUDF                    DEF_BIT_04                /* Receiver elastic underflow                           */

                                                                /* ------------- PHY_BERCR REGISTER BITS -------------- */
#define BERCR_LNKOK                   DEF_BIT_15                /* FSM in 'Good Link' state                             */
#define BERCR_LNKEN                   DEF_BIT_14                /* A trigger on the BER/FEQ moniter causes a 'link down'*/
#define BERCR_TRIG                        0x3800u               /* Trigger level for BER Counter                        */
#define BERCR_WNDW                        0x0780u               /* Length of time for BER Counter                       */
#define BERCR_COUNT                       0x007Fu               /* Shows amount of errors during last time window       */

                                                                /* ------------- PHY_FEQMR REGISTER BITS -------------- */
#define FEQMR_DELTA                       0xFFFFu               /* Write only, 0xFFFE: read reference value, Else: disable*/
#define FEQMR_VAL                         0xFFFFu               /* Read only, used with DELTA, returns Bit 17:2 of      */
                                                                /* 0xFFFE: ref value, Else: current FEQ2 coefficient    */

                                                                /* -------------- PHY_DCSR REGISTER BITS -------------- */
#define DCSR_RESV                     DEF_BIT_15                /* Unused                                               */
#define DCSR_INIT                     DEF_BIT_14                /* Init TDR test                                        */
#define DCSR_ADCMAX                       0x3F00u               /* Read only, shows the signed max(+) or min(-) of      */
                                                                /* reflected wave                                       */
#define DCSR_ADCTRIG                      0x3F00u               /* Write only, 00111:cable length detection, 01111:     */
                                                                /* no cable detection                                   */
#define DCSR_DONE                     DEF_BIT_07                /* Indicates counter is stopped by overrun or ADC       */
                                                                /* trigger                                              */
#define DCSR_POL                      DEF_BIT_06                /* Counter stopped by 0:pos,1:neg trigger level         */
#define DCSR_LINESEL                  DEF_BIT_05                /* Perform diagnosis on 0:RX,1:TX line                  */
#define DCSR_PW                           0x001Fu               /* Pulse width for diagnosis                            */

                                                                /* ------------- PHY_DCNTR REGISTER BITS -------------- */
#define DCNTR_WNDW                        0xFF00u               /* Min time until counter stops                         */
#define DCNTR_COUNT                       0x00FFu               /* Indicates location of the received signal which      */
                                                                /* exceeded threshold                                   */

                                                                /* ------------- PHY_SPCSIR REGISTER BITS ------------- */
#define SPCSIR_RESV                       0xEFCFu               /* Unused (bits 15:13, 10:6, 3:0)                       */
#define SPCSIR_SWRSTFAST              DEF_BIT_12                /* Accelerates SW reset counter from 256us to 10 us     */
#define SPCSIR_SQEOFF                 DEF_BIT_11                /* Disable SQE test                                     */
#define SPCSIR_FEFIEN                 DEF_BIT_05                /* Far end fault indication enable                      */
#define SPCSIR_XPOL                   DEF_BIT_04                /* Polarity of 10BASET-T 0:normal,1:reversed            */

                                                                /* -------------- PHY_ISFR REGISTER BITS -------------- */
#define ISFR_RESV                         0xF901u               /* Unused (bits 15:11, 8, 0)                            */
#define ISFR_INT10                    DEF_BIT_10                /* BER counter trigger                                  */
#define ISFR_INT9                     DEF_BIT_09                /* FEQ trigger                                          */
#define ISFR_INT7                     DEF_BIT_07                /* ENERGYON generated                                   */
#define ISFR_INT6                     DEF_BIT_06                /* Auto-negotiation complete                            */
#define ISFR_INT5                     DEF_BIT_05                /* Remote fault detected                                */
#define ISFR_INT4                     DEF_BIT_04                /* Link down                                            */
#define ISFR_INT3                     DEF_BIT_03                /* Auto-negotiation last page ack                       */
#define ISFR_INT2                     DEF_BIT_02                /* Parallel detection fault                             */
#define ISFR_INT1                     DEF_BIT_01                /* Auto-negotiation page received                       */

                                                                /* -------------- PHY_IER REGISTER BITS --------------- */
#define ISFR_RESV                         0xF901u               /* Unused (bits 15:11, 8, 0)                            */
#define ISFR_MASK                   (~ISFR_RESV)                /* Interrupt mask enable bits                           */

                                                                /* ------------- PHY_SPCSR REGISTER BITS -------------- */
#define SPCSR_RESV                        0xEFA0u               /* Unused (bits 15:13, 11:7, 5)                         */
#define SPCSR_AUTODONE                DEF_BIT_12                /* Auto-negotiation done indication                     */
#define SPCSR_4B5BEN                  DEF_BIT_06                /* Enable 4B/5B encoding/decoding                       */
#define SPCSR_SPEED                       0x001Cu               /* HCDSPEED indication                                  */
#define SPCSR_RXDV                    DEF_BIT_01                /* Controls how rx_dv rises and falls                   */
#define SPCSR_SCRMDIS                 DEF_BIT_00                /* Disable data scrambling                              */


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

const  NET_PHY_API_ETHER  NetPhy_API_UPD6061X = {                                    /* Generic phy API fnct ptrs :      */
                                                 &NetPhy_Init,                      /*   Init                           */
                                                 &NetPhy_EnDis,                     /*   En/dis                         */
                                                 &NetPhy_LinkStateGet,              /*   Link get                       */
                                                 &NetPhy_LinkStateSet,              /*   Link set                       */
                                                  0                                 /*   ISR handler                    */
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
    NET_DEV_API_ETHER * pdev_api;
    NET_PHY_CFG_ETHER * pPhy_Cfg;
    NET_IF_DATA_ETHER * pif_data;
    CPU_INT16U          reg_val;
    CPU_INT16U          retries;
    CPU_INT08U          phy_addr;

    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    pPhy_Cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;

    phy_addr = pPhy_Cfg->BusAddr;

    if (phy_addr == NET_PHY_ADDR_AUTO) {
                                                                /* Attempt to automatically determine Phy addr          */
        NetPhy_AddrProbe(pif, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

        phy_addr = pif_data->Phy_Addr;

    } else {
        pif_data->Phy_Addr = phy_addr;                          /* Set Phy addr to configured value                     */
    }


                                                                /* -------------------- RESET PHY --------------------- */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, BMCR_RESET, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* Read ctrl reg, get reset bit                         */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* Mask out reset status bit                            */
    reg_val &= BMCR_RESET;

                                                                /* Wait for reset to complete                           */
    retries = NET_PHY_INIT_RESET_RETRIES;
    while ((reg_val == BMCR_RESET) && (retries > 0u)) {
        KAL_Dly(50u);

        pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

        reg_val &= BMCR_RESET;
        retries--;
    }

    if (retries == 0u) {
       *perr = NET_PHY_ERR_TIMEOUT_RESET;
        return;
    }

                                                                /* ----------------- CONFIG RMII MODE ----------------- */
    if (pPhy_Cfg->BusMode == NET_PHY_BUS_MODE_RMII) {
                                                                /* Make sure the strap option is set to RMII mode       */
        pdev_api->Phy_RegRd(pif, phy_addr, PHY_SPMR, &reg_val, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

        reg_val &= SPMR_MIIMD;

        if (reg_val != 0) {
            *perr = NET_PHY_ERR_INVALID_BUS_MODE;
            return;
        }
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

                                                                /* Get current control reg value                        */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* Enable/disable Phy                                   */
    switch (en) {
        case DEF_DISABLED:
             reg_val |=  BMCR_PDOWN;
             break;
        case DEF_ENABLED:
        default:
             reg_val &= ~BMCR_PDOWN;
             break;
    }

                                                                /* Power up/down the Phy                                */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, reg_val, perr);
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
    CPU_INT16U          link_partner;
    CPU_INT08U          phy_addr;
    NET_ERR             err;

    pdev_api = (NET_DEV_API_ETHER *)pif->Dev_API;
    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    phy_addr =  pif_data->Phy_Addr;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (plink_state == DEF_NULL) {
       *perr = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

    plink_state->Spd    = NET_PHY_SPD_0;
    plink_state->Duplex = NET_PHY_DUPLEX_UNKNOWN;

                                                                /* -------------- OBTAIN CUR LINK STATUS -------------- */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMSR, &link_self, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* Read BMSR twice (see Note #1)                        */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMSR, &link_self, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* Check if link is down                                */
    if ((link_self & BMSR_LSTATUS) == 0u) {
        *perr = NET_PHY_ERR_NONE;
         return;
    }

                                                                /* ------------- DETERMINE SPD AND DUPLEX ------------- */
                                                                /* Obtain AN settings                                   */
    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* If AN disabled                                       */
    if ((reg_val & BMCR_ANENABLE) == 0u) {
                                                                /* Determine speed                                      */
        if ((reg_val & BMCR_SPEED100) == 0u) {
            plink_state->Spd = NET_PHY_SPD_10;
        } else {
            plink_state->Spd = NET_PHY_SPD_100;
        }

                                                                /* Determine duplex                                     */
        if ((reg_val & BMCR_FULLDPLX) == 0u) {
            plink_state->Duplex = NET_PHY_DUPLEX_HALF;
        } else {
            plink_state->Duplex = NET_PHY_DUPLEX_FULL;
        }
    } else {
                                                                /* AN is enabled, get self link capabilities            */
        pdev_api->Phy_RegRd(pif, phy_addr, PHY_ANAR, &link_self, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

                                                                /* Get link partner link capabilities                   */
        pdev_api->Phy_RegRd(pif, phy_addr, PHY_ANLPABPR, &link_partner, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             return;
        }

                                                                /* Preserve link status bits                            */
        link_partner &= (ANLPABPR_100BASE4 |
                         ANLPABPR_100FULL  |
                         ANLPABPR_100HALF  |
                         ANLPABPR_10FULL   |
                         ANLPABPR_10HALF);

                                                                /* Match self capabilites to partner capabilites        */
        link_self    &= link_partner;

                                                                /* Determine most likely link state                     */
        if (link_self           >= ANLPABPR_100FULL) {
            plink_state->Spd     = NET_PHY_SPD_100;
            plink_state->Duplex  = NET_PHY_DUPLEX_FULL;
        } else if (link_self    >= ANLPABPR_100HALF) {
            plink_state->Spd     = NET_PHY_SPD_100;
            plink_state->Duplex  = NET_PHY_DUPLEX_HALF;
        } else if (link_self    >= ANLPABPR_10FULL) {
            plink_state->Spd     = NET_PHY_SPD_10;
            plink_state->Duplex  = NET_PHY_DUPLEX_FULL;
        } else {
            plink_state->Spd     = NET_PHY_SPD_10;
            plink_state->Duplex  = NET_PHY_DUPLEX_HALF;
        }
    }

                                                                /* Link established, update MAC settings (we swallow    */
                                                                /* device update errors)                                */
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

                                                                /* Enable AN if cfg invalid or any member set to AUTO   */
    if (((spd    != NET_PHY_SPD_10 )     &&
         (spd    != NET_PHY_SPD_100))    ||
        ((duplex != NET_PHY_DUPLEX_HALF) &&
         (duplex != NET_PHY_DUPLEX_FULL))) {
        NetPhy_AutoNegStart(pif, perr);
        return;
    }

    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

                                                                /* Clear AN enable bit                                  */
    reg_val &= ~BMCR_ANENABLE;

                                                                /* Set speed                                            */
    switch (spd) {
        case NET_PHY_SPD_10:
             reg_val &= ~BMCR_SPEED100;
             break;
        case NET_PHY_SPD_100:
             reg_val |=  BMCR_SPEED100;
             break;
        default:
             break;
    }

                                                                /* Set duplex                                           */
    switch (duplex) {
        case NET_PHY_DUPLEX_HALF:
             reg_val &= ~BMCR_FULLDPLX;
             break;
        case NET_PHY_DUPLEX_FULL:
             reg_val |=  BMCR_FULLDPLX;
             break;
        default:
             break;
    }

                                                                /* Configure Phy                                        */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetPhy_AutoNegStart()
*
* Description : Start the Auto-Negotiation process.
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

    pdev_api->Phy_RegRd(pif, phy_addr, PHY_BMCR, &reg_val, perr);
    if (*perr != NET_PHY_ERR_NONE) {
         return;
    }

    reg_val |=  BMCR_ANENABLE |
                BMCR_ANRESTART;

                                                                /* Restart Auto-Negotiation                             */
    pdev_api->Phy_RegWr(pif, phy_addr, PHY_BMCR, reg_val, perr);
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

    for (i = 0u; i <= NET_PHY_ADDR_MAX; i++) {
                                                                /* Get Phy ID #1 reg val.                               */
        pdev_api->Phy_RegRd(pif, i, PHY_PHYSID1, &reg_id1, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             continue;
        }

                                                                /* Get Phy ID #2 reg val.                               */
        pdev_api->Phy_RegRd(pif, i, PHY_PHYSID2, &reg_id2, perr);
        if (*perr != NET_PHY_ERR_NONE) {
             continue;
        }

        if ((reg_id1 == 0xB824) && (reg_id2 == 0x2814)) {
            break;
        }
    }

    if (i > NET_PHY_ADDR_MAX) {
       *perr = NET_PHY_ERR_ADDR_PROBE;
        return;
    }

    pif_data = (NET_IF_DATA_ETHER *)pif->IF_Data;
    pif_data->Phy_Addr =  i;

   *perr = NET_PHY_ERR_NONE;
}

