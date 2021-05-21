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
*                                          GMAC 10/100/1000
*
* Filename : net_dev_gmac.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) With an appropriate BSP, this driver will support the following MCU series:
*
*                     STMicroelectronics  STM32F107xx  series
*                     STMicroelectronics  STM32F2xxx   series
*                     STMicroelectronics  STM32F4xxx   series
*                     STMicroelectronics  STM32F74xxx  series
*                     STMicroelectronics  STM32F75xxx  series
*                     FUJITSU             MB9BFD10T    series
*                     FUJITSU             MB9BF610T    series
*                     FUJITSU             MB9BF210T    series
*                     NXP                 LPC185x      series
*                     NXP                 LPC183x      series
*                     NXP                 LPC435x      series
*                     NXP                 LPC433x      series
*                     INFINEON            XMC4500      series
*                     INFINEON            XMC4400      series
*                     TEXAS INSTRUMENTS   TM4C12x      series
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_GMAC_MODULE
#include  <cpu_cache.h>
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_gmac.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*
* Note(s) : (1) Receive buffers usually MUST be aligned to some octet boundary.  However, adjusting
*               receive buffer alignment MUST be performed from within 'net_dev_cfg.h'.  Do not adjust
*               the value below as it is used for configuration checking only.
*********************************************************************************************************
*/

#define  NET_DEV_GMAC                                       0u
#define  NET_DEV_LPCxx_ENET                                 1u
#define  NET_DEV_XMC4000_ENET                               2u
#define  NET_DEV_TM4C12x_ENET                               3u
#define  NET_DEV_HPS_EMAC                                   4u
#define  NET_DEV_STM32Fx_EMAC                               5u

#define  NET_DEV_GMAC_DFLT_DMA_REG_OFFSET              0x1000u
#define  NET_DEV_TM4C12x_ENET_DMA_REG_OFFSET           0x0C00u
#define  NET_DEV_TM4C12x_ENET_CFG_REG_OFFSET           0x0FC0u

                                                                /* ------- DMA bus mode software reset Timeout -------- */
#define  GMAC_SWR_TO                               0x0004FFFFu

                                                                /* ----------- ETHERNET MAC addresses OFFSET ---------- */
#define  GMAC_HASH_TBL_HIGH                        0x00000000u
#define  GMAC_HASH_TBL_LOW                         0x00000000u
#define  GMAC_VLAN_TAG_COMPARAISON_16BIT           0x00000000u
#define  GMAC_VLAN_TAG_COMPARAISON_12BIT           0x00010000u
#define  GMAC_DMA_DESC_SKIP_LEN                    0x00000000u
#define  GMAC_VLAN_TAG_IDENTIFIER                  0x00000000u
#define  GMAC_GAR_CR_MASK                          0xFFFFFFC3u  /* ETHERNET MACMIIAR register Mask                      */
#define  GMAC_DMA_RX_DESC_FRAME_LEN_SHIFT                  16u  /* ETHERNET DMA Rx descriptors Frame Length Shift       */

                                                                /* ------------- PHY Read/write Timeouts -------------- */
#define  PHY_RD_TO                                 0x0004FFFFu
#define  PHY_WR_TO                                 0x0004FFFFu

#define  RX_BUF_SIZE_MULT                                  16u  /* Rx bufs must be a mul   of 16 bytes.                 */
#define  RX_DESC_BUF_ALIGN                                 16u  /* Rx bufs must be aligned to 16 byte boundary.         */
#define  TX_DESC_BUF_ALIGN                                 16u  /* Tx bufs must be aligned to 16 byte boundary.         */


/*
*********************************************************************************************************
*                                       REGISTER BIT DEFINITIONS
*********************************************************************************************************
*/
                                                                            /* --------- MAC CONFIGURATION REGISTER --------- */
#define  GMAC_MCR_CST                              DEF_BIT_25               /* CRC stripping for type frames bit              */
#define  GMAC_MCR_TC                               DEF_BIT_24               /* Transmit Configuration in MII/RMII bit         */
#define  GMAC_MCR_WD                               DEF_BIT_23               /* Watchdog Disable bit                           */
#define  GMAC_MCR_JD                               DEF_BIT_22               /* Jabber Disable bit                             */
#define  GMAC_MCR_BE                               DEF_BIT_21               /* Frame Burst Enable bit                         */
#define  GMAC_MCR_JE                               DEF_BIT_20               /* Jumbo Frame Enable bit                         */
#define  GMAC_MCR_IFG_96BITS                       DEF_BIT_NONE             /* Inter-Frame Gap bits: 96 bit times             */
#define  GMAC_MCR_IFG_88BITS                       DEF_BIT_MASK(1u, 17u)    /* Inter-Frame Gap bits: 88 bit times             */
#define  GMAC_MCR_IFG_80BITS                       DEF_BIT_MASK(2u, 17u)    /* Inter-Frame Gap bits: 80 bit times             */
#define  GMAC_MCR_IFG_72BITS                       DEF_BIT_MASK(3u, 17u)    /* Inter-Frame Gap bits: 72 bit times             */
#define  GMAC_MCR_IFG_64BITS                       DEF_BIT_MASK(4u, 17u)    /* Inter-Frame Gap bits: 64 bit times             */
#define  GMAC_MCR_IFG_56BITS                       DEF_BIT_MASK(5u, 17u)    /* Inter-Frame Gap bits: 56 bit times             */
#define  GMAC_MCR_IFG_48BITS                       DEF_BIT_MASK(6u, 17u)    /* Inter-Frame Gap bits: 48 bit times             */
#define  GMAC_MCR_IFG_40BITS                       DEF_BIT_MASK(7u, 17u)    /* Inter-Frame Gap bits: 40 bit times             */
#define  GMAC_MCR_DCRS                             DEF_BIT_16               /* Disable Carrier Sense During Transmission bit  */
#define  GMAC_MCR_PS                               DEF_BIT_15               /* Port Select bit                                */
#define  GMAC_MCR_FES                              DEF_BIT_14               /* Speed 0:10Mbps  1:100Mbps                      */
#define  GMAC_MCR_DO                               DEF_BIT_13               /* Disable Receive Own bit                        */
#define  GMAC_MCR_LM                               DEF_BIT_12               /* Loopback Mode bit                              */
#define  GMAC_MCR_DM                               DEF_BIT_11               /* Duplex Mode bit                                */
#define  GMAC_MCR_IPC                              DEF_BIT_10               /* Checksum Offload bit                           */
#define  GMAC_MCR_DR                               DEF_BIT_09               /* Disable Retry bit                              */
#define  GMAC_MCR_LUD                              DEF_BIT_08               /* Link Up/Down  bit                              */
#define  GMAC_MCR_ACS                              DEF_BIT_07               /* Automatic Pad/CRC Stripping bit                */
#define  GMAC_MCR_BL_10                            DEF_BIT_NONE             /* Back-Off Limit                                 */
#define  GMAC_MCR_BL_8                             DEF_BIT_MASK(1u, 5u)
#define  GMAC_MCR_BL_4                             DEF_BIT_MASK(2u, 5u)
#define  GMAC_MCR_BL_1                             DEF_BIT_MASK(3u, 5u)     /* Back-Off Limit bit                             */
#define  GMAC_MCR_DC                               DEF_BIT_04               /* Deferral Check bit                             */
#define  GMAC_MCR_TE                               DEF_BIT_03               /* Transmitter Enable bit                         */
#define  GMAC_MCR_RE                               DEF_BIT_02               /* Receiver Enable bit                            */

                                                                            /* --------- MAC FRAME FILTER REGISTER ---------- */
#define  GMAC_MFFR_RA                              DEF_BIT_31               /* Receive All bit                                */
#define  GMAC_MFFR_HPF                             DEF_BIT_10               /* Hash or Perfect Filter bit                     */
#define  GMAC_MFFR_SAF                             DEF_BIT_09               /* Source Address Filter Enable bit               */
#define  GMAC_MFFR_SAIF                            DEF_BIT_08               /* SA Inverse Filtering bit                       */
#define  GMAC_MFFR_PCF_00                          DEF_BIT_NONE             /* Pass Control Frames bit                        */
#define  GMAC_MFFR_PCF_01                          DEF_BIT_MASK(1u, 6u)
#define  GMAC_MFFR_PCF_10                          DEF_BIT_MASK(2u, 6u)
#define  GMAC_MFFR_PCF_11                          DEF_BIT_MASK(3u, 6u)
#define  GMAC_MFFR_DB                              DEF_BIT_05               /* Disable Broadcast Frames bit                   */
#define  GMAC_MFFR_PM                              DEF_BIT_04               /* Pass All Multicast bit                         */
#define  GMAC_MFFR_DAIF                            DEF_BIT_03               /* DA Inverse Filtering bit                       */
#define  GMAC_MFFR_HMC                             DEF_BIT_02               /* Hash Multicast bit                             */
#define  GMAC_MFFR_HUC                             DEF_BIT_01               /* Hash Unicast bit                               */
#define  GMAC_MFFR_PR                              DEF_BIT_00               /* Promiscuous Mode bit                           */

                                                                            /* --------- MAC FLOW CONTROL  REGISTER --------- */
#define  GMAC_FCR_ZQPD                             DEF_BIT_07               /* Disable Zero-Quanta Pause bit                  */
#define  GMAC_FCR_PLT_00                           DEF_BIT_NONE             /* Pause Low Threshold : 4 slot times             */
#define  GMAC_FCR_PLT_01                           DEF_BIT_MASK(1u, 4u)     /* Pause Low Threshold : 28 slot times            */
#define  GMAC_FCR_PLT_10                           DEF_BIT_MASK(2u, 4u)     /* Pause Low Threshold : 144 slot times           */
#define  GMAC_FCR_PLT_11                           DEF_BIT_MASK(3u, 4u)     /* Pause Low Threshold : 256 slot times           */
#define  GMAC_FCR_UP                               DEF_BIT_03               /* Unicast Pause Frame Detect bit                 */
#define  GMAC_FCR_RFE                              DEF_BIT_02               /* Receive Flow Control Enable bit                */
#define  GMAC_FCR_TFE                              DEF_BIT_01               /* Transmit Flow Control Enable bit               */
#define  GMAC_FCR_FCB_BPA                          DEF_BIT_00               /* Flow Control Busy/Backpressure Activate bit    */

                                                                            /* -------- DMA OPERATION MODE REGISTER --------- */
#define  GMAC_DMAOMR_DT                            DEF_BIT_26               /* Disable Dropp. of TCP/IP Checksum Error Frames */
#define  GMAC_DMAOMR_RSF                           DEF_BIT_25               /* Receive Store and Forward bit                  */
#define  GMAC_DMAOMR_DFF                           DEF_BIT_24               /* Disable Flushing of Received Frames bit        */
#define  GMAC_DMAOMR_TSF                           DEF_BIT_21               /* Transmit Store and Forward bit                 */
#define  GMAC_DMAOMR_FTF                           DEF_BIT_20               /* Flush Transmit FIFO bit                        */
#define  GMAC_DMAOMR_TTC_64                        DEF_BIT_NONE             /* Transmit FIFO Threshold Control: level 64      */
#define  GMAC_DMAOMR_TTC_128                       DEF_BIT_MASK(1u, 14u)    /* Transmit FIFO Threshold Control: level 128     */
#define  GMAC_DMAOMR_TTC_192                       DEF_BIT_MASK(2u, 14u)    /* Transmit FIFO Threshold Control: level 192     */
#define  GMAC_DMAOMR_TTC_256                       DEF_BIT_MASK(3u, 14u)    /* Transmit FIFO Threshold Control: level 256     */
#define  GMAC_DMAOMR_TTC_40                        DEF_BIT_MASK(4u, 14u)    /* Transmit FIFO Threshold Control: level 40      */
#define  GMAC_DMAOMR_TTC_32                        DEF_BIT_MASK(5u, 14u)    /* Transmit FIFO Threshold Control: level 32      */
#define  GMAC_DMAOMR_TTC_24                        DEF_BIT_MASK(6u, 14u)    /* Transmit FIFO Threshold Control: level 24      */
#define  GMAC_DMAOMR_TTC_16                        DEF_BIT_MASK(7u, 14u)    /* Transmit FIFO Threshold Control: level 16      */
#define  GMAC_DMAOMR_ST                            DEF_BIT_13               /* Start/Stop Transmission Command bit            */
#define  GMAC_DMAOMR_FEF                           DEF_BIT_07               /* Forward Error Frames bit                       */
#define  GMAC_DMAOMR_FUF                           DEF_BIT_06               /* Forward Undersized Good Frames bit             */
#define  GMAC_DMAOMR_RTC_64                        DEF_BIT_NONE             /* Receive FIFO Threshold Control : level 64      */
#define  GMAC_DMAOMR_RTC_32                        DEF_BIT_MASK(1u, 3u)     /* Receive FIFO Threshold Control : level 32      */
#define  GMAC_DMAOMR_RTC_96                        DEF_BIT_MASK(2u, 3u)     /* Receive FIFO Threshold Control : level 96      */
#define  GMAC_DMAOMR_RTC_128                       DEF_BIT_MASK(3u, 3u)     /* Receive FIFO Threshold Control : level 128     */
#define  GMAC_DMAOMR_OSF                           DEF_BIT_02               /* Operate on Second Frame bit                    */
#define  GMAC_DMAOMR_SR                            DEF_BIT_01               /* Start/Stop Receive bit                         */

                                                                            /* ----------- DMA BUS MODE REGISTER ------------ */
#define  GMAC_DMABMR_TXPR                          DEF_BIT_27               /* Transmit Priority bit                          */
#define  GMAC_DMABMR_MB                            DEF_BIT_26               /* Mixed Burst bit                                */
#define  GMAC_DMABMR_AAL                           DEF_BIT_25               /* Address-Aligned Beats bit                      */
#define  GMAC_DMABMR_PBL                           DEF_BIT_24               /* PBL Mode bit                                   */
#define  GMAC_DMABMR_USP                           DEF_BIT_23               /* Use Separate PBL bit                           */
#define  GMAC_DMABMR_RX_DMA_1_BEAT                 DEF_BIT_MASK( 1u, 17u)   /* RxDMA PBL : 1 Beat                             */
#define  GMAC_DMABMR_RX_DMA_2_BEAT                 DEF_BIT_MASK( 2u, 17u)   /* RxDMA PBL : 2 Beats                            */
#define  GMAC_DMABMR_RX_DMA_4_BEAT                 DEF_BIT_MASK( 4u, 17u)   /* RxDMA PBL : 4 Beats                            */
#define  GMAC_DMABMR_RX_DMA_8_BEAT                 DEF_BIT_MASK( 8u, 17u)   /* RxDMA PBL : 8 Beats                            */
#define  GMAC_DMABMR_RX_DMA_16_BEAT                DEF_BIT_MASK(16u, 17u)   /* RxDMA PBL : 16 Beats                           */
#define  GMAC_DMABMR_RX_DMA_32_BEAT                DEF_BIT_MASK(32u, 17u)   /* RxDMA PBL : 32 Beats                           */
#define  GMAC_DMABMR_FB                            DEF_BIT_16               /* Perform fixed burst transfers.                 */
#define  GMAC_DMABMR_TX_DMA_1_BEAT                 DEF_BIT_MASK( 1u,  8u)   /* TxDMA PBL : 1 Beat                             */
#define  GMAC_DMABMR_TX_DMA_2_BEAT                 DEF_BIT_MASK( 2u,  8u)   /* TxDMA PBL : 2 Beats                            */
#define  GMAC_DMABMR_TX_DMA_4_BEAT                 DEF_BIT_MASK( 4u,  8u)   /* TxDMA PBL : 4 Beats                            */
#define  GMAC_DMABMR_TX_DMA_8_BEAT                 DEF_BIT_MASK( 8u,  8u)   /* TxDMA PBL : 8 Beats                            */
#define  GMAC_DMABMR_TX_DMA_16_BEAT                DEF_BIT_MASK(16u,  8u)   /* TxDMA PBL : 16 Beats                           */
#define  GMAC_DMABMR_TX_DMA_32_BEAT                DEF_BIT_MASK(32u,  8u)   /* TxDMA PBL : 32 Beats                           */
#define  GMAC_DMABMR_RX_TX_DMA_PRIO_1_1            DEF_BIT_NONE             /* Rx:Tx priority ratio : 1:1                     */
#define  GMAC_DMABMR_RX_TX_DMA_PRIO_2_1            DEF_BIT_MASK( 1u, 14u)   /* Rx:Tx priority ratio : 2:1                     */
#define  GMAC_DMABMR_RX_TX_DMA_PRIO_3_1            DEF_BIT_MASK( 2u, 14u)   /* Rx:Tx priority ratio : 3:1                     */
#define  GMAC_DMABMR_RX_TX_DMA_PRIO_4_1            DEF_BIT_MASK( 3u, 14u)   /* Rx:Tx priority ratio : 4:1                     */
#define  GMAC_DMABMR_EDFE                          DEF_BIT_07               /* Enhanced descriptor format enable              */
#define  GMAC_DMABMR_SWR                           DEF_BIT_00               /* Software reset                                 */

                                                                            /* ------------ DMA STATUS REGISTER ------------- */
#define  GMAC_DMASR_NIS                            DEF_BIT_16               /* Normal interrupt summary                       */
#define  GMAC_DMASR_AIS                            DEF_BIT_15               /* Abnormal interrupt summary                     */
#define  GMAC_DMASR_ERI                            DEF_BIT_14               /* Early receive status                           */
#define  GMAC_DMASR_FBI                            DEF_BIT_13               /* Fatal bus error status                         */
#define  GMAC_DMASR_ETI                            DEF_BIT_10               /* Early transmit status                          */
#define  GMAC_DMASR_RWT                            DEF_BIT_09               /* Receive watchdog timeout status                */
#define  GMAC_DMASR_RPS                            DEF_BIT_08               /* Receive process stopped status                 */
#define  GMAC_DMASR_RU                             DEF_BIT_07               /* Receive buffer unavailable status              */
#define  GMAC_DMASR_RI                             DEF_BIT_06               /* Receive status                                 */
#define  GMAC_DMASR_UNF                            DEF_BIT_05               /* Transmit underflow status                      */
#define  GMAC_DMASR_OVF                            DEF_BIT_04               /* Receive overflow status                        */
#define  GMAC_DMASR_TJT                            DEF_BIT_03               /* Transmit jabber timeout status                 */
#define  GMAC_DMASR_TU                             DEF_BIT_02               /* Transmit buffer unavailable status             */
#define  GMAC_DMASR_TPS                            DEF_BIT_01               /* Transmit process stopped status                */
#define  GMAC_DMASR_TI                             DEF_BIT_00               /* Transmit status                                */

                                                                            /* ------- DMA INTERRUPT ENBALE  REGISTER ------- */
#define  GMAC_DMAIER_NIE                           DEF_BIT_16               /* Normal interrupt summary                       */
#define  GMAC_DMAIER_AIE                           DEF_BIT_15               /* Abnormal interrupt summary                     */
#define  GMAC_DMAIER_ERE                           DEF_BIT_14               /* Early receive interrupt                        */
#define  GMAC_DMAIER_FBE                           DEF_BIT_13               /* Fatal bus error interrupt                      */
#define  GMAC_DMAIER_ETE                           DEF_BIT_10               /* Early transmit interrupt                       */
#define  GMAC_DMAIER_RWE                           DEF_BIT_09               /* Receive watchdog timeout interrupt             */
#define  GMAC_DMAIER_RSE                           DEF_BIT_08               /* Receive process stopped interrupt              */
#define  GMAC_DMAIER_RUE                           DEF_BIT_07               /* Receive buffer unavailable interrupt           */
#define  GMAC_DMAIER_RIE                           DEF_BIT_06               /* Receive interrupt                              */
#define  GMAC_DMAIER_UNE                           DEF_BIT_05               /* Underflow interrupt                            */
#define  GMAC_DMAIER_OVE                           DEF_BIT_04               /* Overflow interrupt                             */
#define  GMAC_DMAIER_TJE                           DEF_BIT_03               /* Transmit jabber timeout interrupt              */
#define  GMAC_DMAIER_TUE                           DEF_BIT_02               /* Transmit buffer unavailable interrupt          */
#define  GMAC_DMAIER_TSE                           DEF_BIT_01               /* Transmit process stopped interrupt             */
#define  GMAC_DMAIER_TIE                           DEF_BIT_00               /* Transmit interrupt                             */

                                                                            /* ---------- MAC MII ADDRESS REGISTER ---------- */
#define  GMAC_GAR_PA                               DEF_BIT_FIELD(5u, 11u)   /* Physical layer address                         */
#define  GMAC_GAR_GR                               DEF_BIT_FIELD(5u,  6u)   /* MII register in the selected PHY               */
#define  GMAC_GAR_CR_DIV42                         DEF_BIT_NONE             /* ETH_CLK: 60-100 MHz; MDC clk= ETH_CLK/42       */
#define  GMAC_GAR_CR_DIV62                         DEF_BIT_MASK(1u, 2u)     /* ETH_CLK:100-150 MHz; MDC clk= ETH_CLK/62       */
#define  GMAC_GAR_CR_DIV16                         DEF_BIT_MASK(2u, 2u)     /* ETH_CLK:  20-35 MHz; MDC clk= ETH_CLK/16       */
#define  GMAC_GAR_CR_DIV26                         DEF_BIT_MASK(3u, 2u)     /* ETH_CLK:  35-60 MHz; MDC clk= ETH_CLK/26       */
#define  GMAC_GAR_CR_DIV102                        DEF_BIT_MASK(4u, 2u)     /* ETH_CLK:150-250 MHz; MDC clk= ETH_CLK/102      */
#define  GMAC_GAR_CR_DIV122_124                    DEF_BIT_MASK(5u, 2u)     /* ETH_CLK:250-300 MHz; MDC clk= ETH_CLK/122 ..   */
                                                                            /* .. ETH_CLK:250-300 MHz; MDC clk= ETH_CLK/124   */
#define  GMAC_GAR_GW                               DEF_BIT_01               /* MII write                                      */
#define  GMAC_GAR_GB                               DEF_BIT_00               /* MII busy                                       */

                                                                            /* -------- MAC PERIPHERAL CFG REGISTER --------- */
#define  GMAC_PC_PHYEXT                            DEF_BIT_31               /* PHY select; 0 = Internal PHY, 1 = External PHY */
#define  GMAC_PC_PINTFS                            DEF_BIT_FIELD(3u, 28u)
                                                                            /* MII(dflt): Used for internal PHY or ext PHY... */
#define  GMAC_PC_PINTFS_MII                        DEF_BIT_MASK(0u, 28u)    /* ...connected via MII.                          */
#define  GMAC_PC_PINTFS_RMII                       DEF_BIT_MASK(4u, 28u)    /* RMII: used for ext PHY connected via RMII.     */
#define  GMAC_PC_DIFRESTART                        DEF_BIT_25               /* PHY soft restart.                              */
#define  GMAC_PC_NIBDETDIS                         DEF_BIT_24               /* Odd nibble TXER destection disable.            */
#define  GMAC_PC_RXERIDLE                          DEF_BIT_23               /* RXER detection during idle.                    */
#define  GMAC_PC_ISOMIILL                          DEF_BIT_22               /* Isolate MII in link loss.                      */
#define  GMAC_PC_LRR                               DEF_BIT_21               /* Link loss recovery.                            */
#define  GMAC_PC_TDRRUN                            DEF_BIT_20               /* TDR auto run.                                  */
#define  GMAC_PC_FASTLDMODE                        DEF_BIT_FIELD(5u, 15u)   /* Fast link down mode.                           */
#define  GMAC_PC_POLSWAP                           DEF_BIT_14               /* Polarity swap.                                 */
#define  GMAC_PC_MDISWAP                           DEF_BIT_13               /* MDI swap.                                      */
#define  GMAC_PC_RBSTMDIX                          DEF_BIT_12               /* Robust auto MDI-X.                             */
#define  GMAC_PC_FASTMDIX                          DEF_BIT_11               /* Fast auto MDI-X.                               */
#define  GMAC_PC_NIBDETDIS                         DEF_BIT_24               /* Odd nibble TXER destection disable.            */
#define  GMAC_PC_MDIXEN                            DEF_BIT_10               /* Enable automatic cross-over.                   */
#define  GMAC_PC_FASTRXDV                          DEF_BIT_09               /* Fast RXDV detection.                           */
#define  GMAC_PC_FASTLUPD                          DEF_BIT_08               /* Fast Link-up in parallel dectect.              */
#define  GMAC_PC_EXTFD                             DEF_BIT_07               /* Extended full duplex ability.                  */
#define  GMAC_PC_FASTANEN                          DEF_BIT_06               /* Fast auto negotiation enable.                  */
#define  GMAC_PC_FASTANSEL                         DEF_BIT_FIELD(2u, 4u)
#define  GMAC_PC_ANEN                              DEF_BIT_03               /* Auto negotiation; 0 = Disable, 1 = Enable.     */
#define  GMAC_PC_ANMODE                            DEF_BIT_FIELD(2u, 1u)
                                                                            /* if ANEN = 0; then ANMODE = XXX.                */
#define  GMAC_PC_PHY_FORCE_10B_H_DUPLEX            0x00000000u              /* ANMODE = 0x0;         10B-T, Half Duplex.      */
#define  GMAC_PC_PHY_FORCE_10B_F_DUPLEX            0x00000002u              /* ANMODE = 0x1;         10B-T, Full Duplex.      */
#define  GMAC_PC_PHY_FORCE_100B_H_DUPLEX           0x00000004u              /* ANMODE = 0x2;        100B-T, Half Duplex.      */
#define  GMAC_PC_PHY_FORCE_100B_F_DUPLEX           0x00000006u              /* ANMODE = 0x3;        100B-T, Full Duplex.      */

                                                                            /* if ANEN = 1; then ANMODE = XXX.                */
#define  GMAC_PC_PHY_AN_10B_HF_DUPLEX              0x00000008u              /* ANMODE = 0x0;         10B-T, Half/Full Duplex. */
#define  GMAC_PC_PHY_AN_100B_HF_DUPLEX             0x0000000Au              /* ANMODE = 0x1;        100B-T, Half/Full Duplex. */
#define  GMAC_PC_PHY_AN_10B_100B_H_DUPLEX          0x0000000Cu              /* ANMODE = 0x2;  10B-T/100B-T, Half Duplex.      */
#define  GMAC_PC_PHY_AN_10B_100B_HF_DUPLEX         0x0000000Eu              /* ANMODE = 0x3;  10B-T/100B-T, Half/Full Duplex. */

#define  GMAC_PC_PHYHOLD                           DEF_BIT_00               /* Ethernet PHY Hold.                             */

                                                                            /* ----------- MAC CLOCK CFG REGISTER ----------- */
#define  GMAC_CC_PTPCEN                            DEF_BIT_18              /* PTP clock reference enable.                     */
#define  GMAC_CC_POL                               DEF_BIT_17              /* LED polarity control.                           */
#define  GMAC_CC_CLKEN                             DEF_BIT_16              /* EN0RREF_CLK signal enable.                      */


/*
*********************************************************************************************************
*                                    Ethernet DMA descriptors registers bits definition
*
* Note(s) : (1) Rx Descriptor :
*               -----------------------------------------------------------------------------------------------------------------
*               RDES0 | OWN(31) |                                             Status [30:0]                                      |
*               -----------------------------------------------------------------------------------------------------------------
*               RDES1 | CTRL(31) | Reserved[30:29] | Buffer2 ByteCnt[28:16] | CTRL[15:14] | Reserved(13) | Buffer1 ByteCnt[12:0] |
*               -----------------------------------------------------------------------------------------------------------------
*               RDES2 |                                       Buffer1 Address [31:0]                                             |
*               -----------------------------------------------------------------------------------------------------------------
*               RDES3 |                          Buffer2 Address [31:0] / Next Desciptor Address [31:0]                          |
*               -----------------------------------------------------------------------------------------------------------------
*
*           (2) Tx Descriptor :
*               -----------------------------------------------------------------------------------------------------------------
*               TDES0 | OWN(31) | CTRL[30:26] | Reserved[25:24] | CTRL[23:20] | Reserved[19:17] | Status[16:0]                   |
*               -----------------------------------------------------------------------------------------------------------------
*               TDES1 | Reserved[31:29] | Buffer2 ByteCount[28:16] | Reserved[15:13] | Buffer1 ByteCount[12:0]                   |
*               -----------------------------------------------------------------------------------------------------------------
*               TDES2 |                         Buffer1 Address [31:0]                                                           |
*               -----------------------------------------------------------------------------------------------------------------
*               TDES3 |                   Buffer2 Address [31:0] / Next Desciptor Address [31:0]                                 |
*               -----------------------------------------------------------------------------------------------------------------
*********************************************************************************************************
*/

                                                                            /* ---- TDES0 : DMA Tx descriptor status Reg ---- */
#define  GMAC_DMA_TX_DESC_OWN                      DEF_BIT_31               /* OWN bit: descriptor is owned by DMA engine     */
#define  GMAC_DMA_TX_DESC_IC                       DEF_BIT_30               /* Interrupt on Completion                        */
#define  GMAC_DMA_TX_DESC_LS                       DEF_BIT_29               /* Last Segment                                   */
#define  GMAC_DMA_TX_DESC_FS                       DEF_BIT_28               /* First Segment                                  */
#define  GMAC_DMA_TX_DESC_DC                       DEF_BIT_27               /* Disable CRC                                    */
#define  GMAC_DMA_TX_DESC_DP                       DEF_BIT_26               /* Disable Padding                                */
#define  GMAC_DMA_TX_DESC_TTSE                     DEF_BIT_25               /* Transmit Time Stamp Enable                     */
#define  GMAC_DMA_TX_DESC_CO1                      DEF_BIT_23               /* Checksum offload bit 1                         */
#define  GMAC_DMA_TX_DESC_CO0                      DEF_BIT_22               /* Checksum offload bit 0                         */
#define  GMAC_DMA_TX_DESC_CIC                      DEF_BIT_FIELD(2u, 22u)   /* Checksum Insertion Control: 4 cases            */
#define  GMAC_DMA_TX_DESC_CIC_BYPASS               DEF_BIT_NONE             /* Do Nothing: Checksum Engine is bypassed        */
#define  GMAC_DMA_TX_DESC_CIC_IPV4_HEADER          DEF_BIT_MASK(1u, 22u)    /* IPV4 header Checksum Insertion                 */
#define  GMAC_DMA_TX_DESC_CIC_TCPUDPICMP_SEGMENT   DEF_BIT_MASK(2u, 22u)    /* TCP/UDP/ICMP Checksum Insertion calculated     */
                                                                            /* over segment only                              */
#define  GMAC_DMA_TX_DESC_CIC_TCPUDPICMP_FULL      DEF_BIT_MASK(3u, 22u)    /* TCP/UDP/ICMP Checksum Insertion fully          */
                                                                            /* calculated                                     */
#define  GMAC_DMA_TX_DESC_TER                      DEF_BIT_21               /* Transmit End of Ring                           */
#define  GMAC_DMA_TX_DESC_TCH                      DEF_BIT_20               /* Second Address Chained                         */
#define  GMAC_DMA_TX_DESC_TTSS                     DEF_BIT_17               /* Tx Time Stamp Status                           */
#define  GMAC_DMA_TX_DESC_IHE                      DEF_BIT_16               /* IP Header Error                                */
#define  GMAC_DMA_TX_DESC_ES                       DEF_BIT_15               /* Error summary: OR of the following bits:       */
                                                                            /* UE || ED || EC || LCO || NC || LCA || FF || JT */
#define  GMAC_DMA_TX_DESC_JT                       DEF_BIT_14               /* Jabber Timeout                                 */
#define  GMAC_DMA_TX_DESC_FF                       DEF_BIT_13               /* Frame Flushed: DMA/MTL flushed the frame due   */
                                                                            /* to SW flush                                    */
#define  GMAC_DMA_TX_DESC_IPE                      DEF_BIT_12               /* Payload Checksum Error                         */
#define  GMAC_DMA_TX_DESC_LC                       DEF_BIT_11               /* Loss of Carrier: carrier lost during           */
                                                                            /* tramsmission                                   */
#define  GMAC_DMA_TX_DESC_NC                       DEF_BIT_10               /* No Carrier: no carrier signal from the         */
                                                                            /* tranceiver                                     */
#define  GMAC_DMA_TX_DESC_LCO                      DEF_BIT_09               /* Late Collision: transmission aborted due       */
                                                                            /* to collision                                   */
#define  GMAC_DMA_TX_DESC_EC                       DEF_BIT_08               /* Excessive Collision: transmission aborted      */
                                                                            /* after 16 collisions                            */
#define  GMAC_DMA_TX_DESC_VF                       DEF_BIT_07               /* VLAN Frame                                     */
#define  GMAC_DMA_TX_DESC_CC                       DEF_BIT_FIELD(4u, 3u)    /* Collision Count                                */
#define  GMAC_DMA_TX_DESC_ED                       DEF_BIT_02               /* Excessive Deferral                             */
#define  GMAC_DMA_TX_DESC_UF                       DEF_BIT_01               /* Underflow Error: late data arrival from        */
                                                                            /* the memory                                     */
#define  GMAC_DMA_TX_DESC_DB                       DEF_BIT_00               /* Deferred Bit                                   */

                                                                            /* -------------- TDES1 Register ---------------- */
#define  GMAC_DMA_TX_DESC_TBS2                     DEF_BIT_FIELD(13u, 16u)  /* Transmit Buffer2 Size                          */
#define  GMAC_DMA_TX_DESC_TBS1                     DEF_BIT_FIELD(13u,  0u)  /* Transmit Buffer1 Size                          */

                                                                            /* -------------- TDES2 Register ---------------- */
#define  GMAC_DMA_TX_DESC_B1AP                     0xFFFFFFFFu              /* Buffer1 Address Pointer                        */

                                                                            /* --------------- TDES3 Register --------------- */
#define  GMAC_DMA_TX_DESC_B2AP                     0xFFFFFFFFu              /* Buffer2 Address Pointer                        */

                                                                            /* ---- RDES0 : DMA Rx descriptor status Reg ---- */
#define  GMAC_DMA_RX_DESC_OWN                      DEF_BIT_31               /* OWN bit: descriptor is owned by DMA engine     */
#define  GMAC_DMA_RX_DESC_AFM                      DEF_BIT_30               /* DA Filter Fail for the rx frame                */
#define  GMAC_DMA_RX_DESC_FL                       DEF_BIT_FIELD(14u, 16u)  /* Receive descriptor frame len                   */
#define  GMAC_DMA_RX_DESC_ES                       DEF_BIT_15               /* Error summary: OR of the following bits:       */
                                                                            /* DE || OE || IPC || LC || RWT || RE || CE       */
#define  GMAC_DMA_RX_DESC_DE                       DEF_BIT_14               /* Desciptor error: no more descriptors for       */
                                                                            /* receive frame                                  */
#define  GMAC_DMA_RX_DESC_SAF                      DEF_BIT_13               /* SA Filter Fail for the received frame          */
#define  GMAC_DMA_RX_DESC_LE                       DEF_BIT_12               /* Frame size not matching with len field         */
#define  GMAC_DMA_RX_DESC_OE                       DEF_BIT_11               /* Overflow Error: Frame was damaged due to       */
                                                                            /* buffer overflow                                */
#define  GMAC_DMA_RX_DESC_VLAN                     DEF_BIT_10               /* VLAN Tag: received frame is a VLAN frame       */
#define  GMAC_DMA_RX_DESC_FS                       DEF_BIT_09               /* First descriptor of the frame                  */
#define  GMAC_DMA_RX_DESC_LS                       DEF_BIT_08               /* Last descriptor of the frame                   */
#define  GMAC_DMA_RX_DESC_TSA                      DEF_BIT_07               /* Advance Timestamp feature is present           */
#define  GMAC_DMA_RX_DESC_LC                       DEF_BIT_06               /* Late collision occurred during reception       */
#define  GMAC_DMA_RX_DESC_FT                       DEF_BIT_05               /* Frame type - Ethernet, otherwise 802.3         */
#define  GMAC_DMA_RX_DESC_RWT                      DEF_BIT_04               /* Receive Watchdog Timeout: watchdog timer       */
                                                                            /* expired during reception                       */
#define  GMAC_DMA_RX_DESC_RE                       DEF_BIT_03               /* Receive error: error reported by MII interface */
#define  GMAC_DMA_RX_DESC_DBE                      DEF_BIT_02               /* Dribble bit error: frame contains non int      */
                                                                            /* multiple of 8 bits                             */
#define  GMAC_DMA_RX_DESC_CE                       DEF_BIT_01               /* CRC error                                      */
#define  GMAC_DMA_RX_DESC_ESA                      DEF_BIT_00               /* Extended Status Available/Rx MAC Address       */

                                                                            /* -------------- RDES1 register ---------------- */
#define  GMAC_DMA_RX_DESC_DIC                      DEF_BIT_31               /* Disable Interrupt on Completion                */
#define  GMAC_DMA_RX_DESC_RBS2                     DEF_BIT_FIELD(13u, 16u)  /* Receive Buffer2 Size                           */
#define  GMAC_DMA_RX_DESC_RER                      DEF_BIT_15               /* Receive End of Ring                            */
#define  GMAC_DMA_RX_DESC_RCH                      DEF_BIT_14               /* Second Address Chained                         */
#define  GMAC_DMA_RX_DESC_RBS1                     DEF_BIT_FIELD(13u,  0u)  /* Receive Buffer1 Size                           */

                                                                            /* -------------- RDES2 register ---------------- */
#define  GMAC_DMA_RX_DESC_B1AP                     0xFFFFFFFFu              /* Buffer1 Address Pointer                        */

                                                                            /* -------------- RDES3 register ---------------- */
#define  GMAC_DMA_RX_DESC_B2AP                     0xFFFFFFFFu              /* Buffer2 Address Pointer                        */


/*
*********************************************************************************************************
*                                                 GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                                 MACROS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*
* Note(s) : (1) Instance specific data area structures should be defined below.  The data area
*               structure typically includes error counters and variables used to track the
*               state of the device.  Variables required for correct operation of the device
*               MUST NOT be defined globally and should instead be included within the instance
*               specific data area structure and referenced as pif->Dev_Data structure members.
*
*           (2) DMA based devices may require more than one type of descriptor.  Each descriptor
*               type should be defined below.  An example descriptor has been provided.
*
*           (3) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*/

typedef  struct  dev_desc      DEV_DESC;
typedef  struct  dev_desc_ext  DEV_DESC_EXT;

struct dev_desc {
    CPU_REG32    Status;                                        /* DMA status register.                                 */
    CPU_REG32    Len;                                           /* DMA buffer size.                                     */
    CPU_INT08U  *Addr;                                          /* DMA buffer pointer                                   */
    DEV_DESC    *Next;                                          /* DMA next descriptor pointer.                         */
};

struct dev_desc_ext {
    CPU_REG32    Status;                                        /* DMA status register.                                 */
    CPU_REG32    Len;                                           /* DMA buffer size.                                     */
    CPU_INT08U  *Addr;                                          /* DMA buffer pointer                                   */
    DEV_DESC    *Next;                                          /* DMA next descriptor pointer.                         */
    CPU_REG32    Ext_Status;                                    /* DMA extended status.                                 */
    CPU_REG32    Reserved;                                      /* Reserved.                                            */
    CPU_REG32    TTL;                                           /* DMA Transmit Timestamp Low.                          */
    CPU_REG32    TTH;                                           /* DMA Transmit Timestamp High.                         */
};

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data {
    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR      StatRxCtrRx;
    NET_CTR      StatRxCtrRxISR;
    NET_CTR      StatTxCtrTxISR;
    NET_CTR      StatRxCtrTaskSignalISR;
    NET_CTR      StatRxCtrTaskSignalSelf;
    NET_CTR      StatRxCtrTaskSignalNone;
    CPU_REG32    IntStatusLast;
#endif

#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    NET_CTR      ErrRxCtrDataAreaAlloc;
    NET_CTR      ErrRxCtrROS;
    NET_CTR      ErrRxCtrRBUS;
    NET_CTR      ErrRxCtrRPSS;
    NET_CTR      ErrRxCtrDescES;
    NET_CTR      ErrRxCtrDescOWN;
    NET_CTR      ErrRxCtrRunt;
    NET_CTR      ErrRxTxCtrFatalBusErr;
    NET_CTR      ErrRxCtrTaskSignalFail;
#endif

    DEV_DESC    *RxDescPtrStart;
    DEV_DESC    *RxDescPtrCur;


    DEV_DESC    *TxDescPtrStart;
    DEV_DESC    *TxDescPtrCur;
    DEV_DESC    *TxDescPtrComp;                                 /* See Note #3.                                         */

    CPU_INT16U   TxDescNbrAvail;

#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U   MulticastAddrHashBitCtr[64];
#endif
    CPU_ADDR     EMAC_DMABaseAddr;
    CPU_INT32U   MAC_Type;
} NET_DEV_DATA;


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
static  void  NetDev_GMAC_Init          (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_LPCxx_Enet_Init    (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_XMC4000_Enet_Init  (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_TM4C12x_Enet_Init  (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_HPS_EMAC_Init      (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_STM32Fx_EMAC_Init  (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_PHY_InitHandler    (NET_IF             *pif,
                                         CPU_INT16U          gmac_dev_type,
                                         NET_ERR            *perr);

static  void  NetDev_InitHandler        (NET_IF             *pif,
                                         CPU_INT16U          gmac_dev_type,
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

static  void  NetDev_ISR_Handler        (NET_IF             *pif,
                                         NET_DEV_ISR_TYPE    type);

static  void  NetDev_IO_Ctrl            (NET_IF             *pif,
                                         CPU_INT08U          opt,
                                         void               *p_data,
                                         NET_ERR            *perr);

static  void  NetDev_MII_Rd             (NET_IF             *pif,
                                         CPU_INT08U          phy_addr,
                                         CPU_INT08U          reg_addr,
                                         CPU_INT16U         *p_data,
                                         NET_ERR            *perr);

static  void  NetDev_MII_Wr             (NET_IF             *pif,
                                         CPU_INT08U          phy_addr,
                                         CPU_INT08U          reg_addr,
                                         CPU_INT16U          data,
                                         NET_ERR            *perr);
                                                                /* --------- FNCT'S COMMON TO DMA BASED DRV'S --------- */
static  void  NetDev_RxDescInit         (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_RxDescPtrCurInc    (NET_IF             *pif);

static  void  NetDev_TxDescInit         (NET_IF             *pif,
                                         NET_ERR            *perr);

static  void  NetDev_RxDescFreeAll      (NET_IF             *pif,
                                         NET_ERR            *perr);


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
                                                                                    /* GMAC dev API fnct ptrs :         */
const  NET_DEV_API_ETHER  NetDev_API_GMAC         = { &NetDev_GMAC_Init,            /*   Init/add                       */
                                                      &NetDev_Start,                /*   Start                          */
                                                      &NetDev_Stop,                 /*   Stop                           */
                                                      &NetDev_Rx,                   /*   Rx                             */
                                                      &NetDev_Tx,                   /*   Tx                             */
                                                      &NetDev_AddrMulticastAdd,     /*   Multicast addr add             */
                                                      &NetDev_AddrMulticastRemove,  /*   Multicast addr remove          */
                                                      &NetDev_ISR_Handler,          /*   ISR handler                    */
                                                      &NetDev_IO_Ctrl,              /*   I/O ctrl                       */
                                                      &NetDev_MII_Rd,               /*   Phy reg rd                     */
                                                      &NetDev_MII_Wr                /*   Phy reg wr                     */
                                                    };


                                                                                    /* LPCXX_ENET dev API fnct ptrs :   */
const  NET_DEV_API_ETHER  NetDev_API_LPCXX_ENET   = { &NetDev_LPCxx_Enet_Init,      /*   Init/add                       */
                                                      &NetDev_Start,                /*   Start                          */
                                                      &NetDev_Stop,                 /*   Stop                           */
                                                      &NetDev_Rx,                   /*   Rx                             */
                                                      &NetDev_Tx,                   /*   Tx                             */
                                                      &NetDev_AddrMulticastAdd,     /*   Multicast addr add             */
                                                      &NetDev_AddrMulticastRemove,  /*   Multicast addr remove          */
                                                      &NetDev_ISR_Handler,          /*   ISR handler                    */
                                                      &NetDev_IO_Ctrl,              /*   I/O ctrl                       */
                                                      &NetDev_MII_Rd,               /*   Phy reg rd                     */
                                                      &NetDev_MII_Wr                /*   Phy reg wr                     */
                                                    };


                                                                                    /* XMC4000_ENET dev API fnct ptrs : */
const  NET_DEV_API_ETHER  NetDev_API_XMC4000_ENET = { &NetDev_XMC4000_Enet_Init,    /*   Init/add                       */
                                                      &NetDev_Start,                /*   Start                          */
                                                      &NetDev_Stop,                 /*   Stop                           */
                                                      &NetDev_Rx,                   /*   Rx                             */
                                                      &NetDev_Tx,                   /*   Tx                             */
                                                      &NetDev_AddrMulticastAdd,     /*   Multicast addr add             */
                                                      &NetDev_AddrMulticastRemove,  /*   Multicast addr remove          */
                                                      &NetDev_ISR_Handler,          /*   ISR handler                    */
                                                      &NetDev_IO_Ctrl,              /*   I/O ctrl                       */
                                                      &NetDev_MII_Rd,               /*   Phy reg rd                     */
                                                      &NetDev_MII_Wr                /*   Phy reg wr                     */
                                                    };


                                                                                    /* TM4C12X_ENET dev API fnct ptrs : */
const  NET_DEV_API_ETHER  NetDev_API_TM4C12X_ENET = { &NetDev_TM4C12x_Enet_Init,    /*   Init/add                       */
                                                      &NetDev_Start,                /*   Start                          */
                                                      &NetDev_Stop,                 /*   Stop                           */
                                                      &NetDev_Rx,                   /*   Rx                             */
                                                      &NetDev_Tx,                   /*   Tx                             */
                                                      &NetDev_AddrMulticastAdd,     /*   Multicast addr add             */
                                                      &NetDev_AddrMulticastRemove,  /*   Multicast addr remove          */
                                                      &NetDev_ISR_Handler,          /*   ISR handler                    */
                                                      &NetDev_IO_Ctrl,              /*   I/O ctrl                       */
                                                      &NetDev_MII_Rd,               /*   Phy reg rd                     */
                                                      &NetDev_MII_Wr                /*   Phy reg wr                     */
                                                    };

                                                                                    /* HPS_EMAC dev API fnct ptrs :     */
const  NET_DEV_API_ETHER  NetDev_API_HPS_EMAC     = { &NetDev_HPS_EMAC_Init,        /*   Init/add                       */
                                                      &NetDev_Start,                /*   Start                          */
                                                      &NetDev_Stop,                 /*   Stop                           */
                                                      &NetDev_Rx,                   /*   Rx                             */
                                                      &NetDev_Tx,                   /*   Tx                             */
                                                      &NetDev_AddrMulticastAdd,     /*   Multicast addr add             */
                                                      &NetDev_AddrMulticastRemove,  /*   Multicast addr remove          */
                                                      &NetDev_ISR_Handler,          /*   ISR handler                    */
                                                      &NetDev_IO_Ctrl,              /*   I/O ctrl                       */
                                                      &NetDev_MII_Rd,               /*   Phy reg rd                     */
                                                      &NetDev_MII_Wr                /*   Phy reg wr                     */
                                                    };

                                                                                    /* STM32Fx_EMAC dev API fnct ptrs : */
const  NET_DEV_API_ETHER  NetDev_API_STM32Fx_EMAC = { &NetDev_STM32Fx_EMAC_Init,    /*   Init/add                       */
                                                      &NetDev_Start,                /*   Start                          */
                                                      &NetDev_Stop,                 /*   Stop                           */
                                                      &NetDev_Rx,                   /*   Rx                             */
                                                      &NetDev_Tx,                   /*   Tx                             */
                                                      &NetDev_AddrMulticastAdd,     /*   Multicast addr add             */
                                                      &NetDev_AddrMulticastRemove,  /*   Multicast addr remove          */
                                                      &NetDev_ISR_Handler,          /*   ISR handler                    */
                                                      &NetDev_IO_Ctrl,              /*   I/O ctrl                       */
                                                      &NetDev_MII_Rd,               /*   Phy reg rd                     */
                                                      &NetDev_MII_Wr                /*   Phy reg wr                     */
                                                    };


/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*
* Note(s) : (1) Device register definitions SHOULD NOT be absolute & SHOULD use the provided base address
*               within the device configuration structure (see 'net_dev_cfg.c'), as well as each device's
*               register definition structure in order to properly resolve register addresses at run-time
*               by mapping the device register definition structure onto an interface's base address.
*
*           (2) The device register definition structure MUST take into account appropriate register
*               offsets & apply reserved space as required.  The registers listed within the register
*               definition structure MUST reflect the exact ordering and data sizes illustrated in the
*               device user guide.
*
*           (3) Device registers SHOULD be declared as volatile variables so that compilers do NOT cache
*               register values but MUST perform the steps to read or write each register value for every
*               register read or write access.
*
*           (4) This set of registers are ONLY available with the Texas Instruments TM4C12x series.
*
*               An example device register structure is provided below.
*********************************************************************************************************
*/

typedef  struct  net_dev_emac {
                                                                /* ------------ ETHERNET-MAC CH0 REGISTERS ------------ */
    CPU_REG32  MAC_CONFIG;                                      /* MAC configuration.                                   */
    CPU_REG32  MAC_FRAME_FILTER;                                /* MAC frame filter                                     */
    CPU_REG32  MAC_HASHTABLE_HIGH;                              /* MAC hash table (High)                                */
    CPU_REG32  MAC_HASHTABLE_LOW;                               /* MAC hash table (Low)                                 */
    CPU_REG32  MAC_MII_ADDR;                                    /* GMII/MII address                                     */
    CPU_REG32  MAC_MII_DATA;                                    /* GMII/MII data                                        */
    CPU_REG32  MAC_FLOW_CTRL;                                   /* Flow control                                         */
    CPU_REG32  MAC_VLAN_TAG;                                    /* VLAN tag                                             */
    CPU_REG32  RSVD0;
    CPU_REG32  MAC_DEBUG;                                       /* Debug register                                       */
    CPU_REG32  MAC_RWAKE_FRFLT;                                 /* Remote wake-up frame filter                          */
    CPU_REG32  MAC_PMT_CTRL_STAT;                               /* PMT                                                  */
    CPU_REG32  LPI_CTRL_STAT;                                   /* LPI control and status                               */
    CPU_REG32  LPI_TMR_CTRL;                                    /* LPI timers control                                   */
    CPU_REG32  MAC_INTR;                                        /* Interrupt status                                     */
    CPU_REG32  MAC_INTR_MASK;                                   /* Interrupt mask                                       */
    CPU_REG32  MAC_ADDR0_HIGH;                                  /* MAC address 0 (High)                                 */
    CPU_REG32  MAC_ADDR0_LOW;                                   /* MAC address 0 (Low)                                  */
    CPU_REG32  MAC_ADDR1_HIGH;                                  /* MAC address 1 (High)                                 */
    CPU_REG32  MAC_ADDR1_LOW;                                   /* MAC address 1 (Low)                                  */
    CPU_REG32  MAC_ADDR2_HIGH;                                  /* MAC address 2 (High)                                 */
    CPU_REG32  MAC_ADDR2_LOW;                                   /* MAC address 2 (Low)                                  */
    CPU_REG32  MAC_ADDR3_HIGH;                                  /* MAC address 3 (High)                                 */
    CPU_REG32  MAC_ADDR3_LOW;                                   /* MAC address 3 (Low)                                  */
    CPU_REG32  MAC_ADDR4_HIGH;                                  /* MAC address 4 (High)                                 */
    CPU_REG32  MAC_ADDR4_LOW;                                   /* MAC address 4 (Low)                                  */
    CPU_REG32  MAC_ADDR5_HIGH;                                  /* MAC address 5 (High)                                 */
    CPU_REG32  MAC_ADDR5_LOW;                                   /* MAC address 5 (Low)                                  */
    CPU_REG32  MAC_ADDR6_HIGH;                                  /* MAC address 6 (High)                                 */
    CPU_REG32  MAC_ADDR6_LOW;                                   /* MAC address 6 (Low)                                  */
    CPU_REG32  MAC_ADDR7_HIGH;                                  /* MAC address 7 (High)                                 */
    CPU_REG32  MAC_ADDR7_LOW;                                   /* MAC address 7 (Low)                                  */
    CPU_REG32  MAC_ADDR8_HIGH;                                  /* MAC address 8 (High)                                 */
    CPU_REG32  MAC_ADDR8_LOW;                                   /* MAC address 8 (Low)                                  */
    CPU_REG32  MAC_ADDR9_HIGH;                                  /* MAC address 9 (High)                                 */
    CPU_REG32  MAC_ADDR9_LOW;                                   /* MAC address 9 (Low)                                  */
    CPU_REG32  MAC_ADDR10_HIGH;                                 /* MAC address 10 (High)                                */
    CPU_REG32  MAC_ADDR10_LOW;                                  /* MAC address 10 (Low)                                 */
    CPU_REG32  MAC_ADDR11_HIGH;                                 /* MAC address 11 (High)                                */
    CPU_REG32  MAC_ADDR11_LOW;                                  /* MAC address 11 (Low)                                 */
    CPU_REG32  MAC_ADDR12_HIGH;                                 /* MAC address 12 (High)                                */
    CPU_REG32  MAC_ADDR12_LOW;                                  /* MAC address 12 (Low)                                 */
    CPU_REG32  MAC_ADDR13_HIGH;                                 /* MAC address 13 (High)                                */
    CPU_REG32  MAC_ADDR13_LOW;                                  /* MAC address 13 (Low)                                 */
    CPU_REG32  MAC_ADDR14_HIGH;                                 /* MAC address 14 (High)                                */
    CPU_REG32  MAC_ADDR14_LOW;                                  /* MAC address 14 (Low)                                 */
    CPU_REG32  MAC_ADDR15_HIGH;                                 /* MAC address 15 (High)                                */
    CPU_REG32  MAC_ADDR15_LOW;                                  /* MAC address 15 (Low)                                 */
    CPU_REG32  RSVD1[6u];
    CPU_REG32  RGMII_STAT;                                      /* RGMII status                                         */
    CPU_REG32  RSVD2[9u];
                                                                /* -------- MMC MODULE AND STATISTICS COUNTERS -------- */
    CPU_REG32  MMC_CNTL;                                        /* MMC control                                          */
    CPU_REG32  MMC_INTR_RX;                                     /* MMC receive interrupt                                */
    CPU_REG32  MMC_INTR_TX;                                     /* MMC transmit interrupt                               */
    CPU_REG32  MMC_INTR_MASK_RX;                                /* MMC receive interrupt mask                           */
    CPU_REG32  MMC_INTR_MASK_TX;                                /* MMC transmit interrupt mask                          */
    CPU_REG32  TXOCTETCOUNT_GB;                                 /* Number of bytes transmitted in good and bad frames   */
    CPU_REG32  TXFRAMECOUNT_GB;                                 /* Number of good and bad frames transmitted            */
    CPU_REG32  TXBROADCASTFRAMES_G;                             /* Number of good broadcast frames transmitted          */
    CPU_REG32  TXMULTICASTFRAMES_G;                             /* Number of good multicast frames transmitted          */
    CPU_REG32  TX64OCTETS_GB;                                   /* Nbr of good & bad frames transmitted of 64 bytes     */
    CPU_REG32  TX65TO127OCTETS_GB;                              /* Good&Bad frames transmitted between 65 & 127 bytes   */
    CPU_REG32  TX128TO255OCTETS_GB;                             /* Good&Bad frames transmitted between 128 & 255 bytes  */
    CPU_REG32  TX256TO511OCTETS_GB;                             /* Good&Bad frames transmitted between 256 & 511 bytes  */
    CPU_REG32  TX512TO1023OCTETS_GB;                            /* Good&Bad frames transmitted between 512 & 1023 bytes */
    CPU_REG32  TX1024TOMAXOCTETS_GB;                            /* Good&Bad frames transmitted between 1024 & Max size  */
    CPU_REG32  TXUNICASTFRAMES_GB;                              /* Number of good and bad unicast frames transmitted    */
    CPU_REG32  TXMULTICASTFRAMES_GB;                            /* Number of good and bad multicast frames transmitted  */
    CPU_REG32  TXBROADCASTFRAMES_GB;                            /* Number of good and bad broadcast frames transmitted  */
    CPU_REG32  TXUNDERFLOWERROR;                                /* Number of frames aborted due to frame underflow err  */
    CPU_REG32  TXSINGLECOL_G;                                   /* Good transmitted frames after a single collision     */
    CPU_REG32  TXMULTICOL_G;                                    /* Good transmitted frames after more than single Coll. */
    CPU_REG32  TXDEFERRED;                                      /* Good transmitted frames after deferral               */
    CPU_REG32  TXLATECOL;                                       /* Frames aborted due to late collision error           */
    CPU_REG32  TXEXESSCOL;                                      /* Frames aborted due to excessive(16) collision error  */
    CPU_REG32  TXCARRIERROR;                                    /* Frames aborted due to carrier sense error            */
    CPU_REG32  TXOCTETCOUNT_G;                                  /* Bytes transmitted in good frames only                */
    CPU_REG32  TXFRAMECOUNT_G;                                  /* Good frames transmitted                              */
    CPU_REG32  TXEXECESSDEF;                                    /* Frames aborted due to excessive deferral error       */
    CPU_REG32  TXPAUSEFRAMES;                                   /* Good PAUSE frames transmitted                        */
    CPU_REG32  TXVLANFRAMES_G;                                  /* Good VLAN frames transmitted                         */
    CPU_REG32  RSVD3[2u];
    CPU_REG32  RXFRAMECOUNT_GB;                                 /* Good and bad frames received                         */
    CPU_REG32  RXOCTETCOUNT_GB;                                 /* Bytes receive in good and bad frames                 */
    CPU_REG32  RXOCTETCOUNT_G;                                  /* Bytes receive in good frames only                    */
    CPU_REG32  RXBROADCASTFRAMES_G;                             /* Good broadcast frames received                       */
    CPU_REG32  RXMULTICASTFRAMES_G;                             /* Good multicast frames received                       */
    CPU_REG32  RXCRCERROR;                                      /* Frames received with CRC error                       */
    CPU_REG32  RXALLIGNMENTERROR;                               /* Frames received with alignment(dribble) error.       */
    CPU_REG32  RXRUNTERROR;                                     /* Frames received with runt(<64 bytes & CRC err) err   */
    CPU_REG32  RXJABBERERROR;                                   /* Frames received w/ length > 1518 bytes w/ CRC err    */
    CPU_REG32  RXUNDERSIZE_G;                                   /* Frames received w. length < 64 bytes w/o any err     */
    CPU_REG32  RXOVERSIZE_G;                                    /* Frames received w/ length > max size w/o err         */
    CPU_REG32  RX64OCTETS_GB;                                   /* Good&Bad frames received with length 64 bytes        */
    CPU_REG32  RX65TO127OCTETS_GB;                              /* Good&Bad frames received between 65 & 127 bytes      */
    CPU_REG32  RX128TO255OCTETS_GB;                             /* Good&Bad frames received between 128 & 255 bytes     */
    CPU_REG32  RX256TO511OCTETS_GB;                             /* Good&Bad frames received between 256 & 511 bytes     */
    CPU_REG32  RX512TO1023OCTETS_GB;                            /* Good&Bad frames received between 512 & 1023 bytes    */
    CPU_REG32  RX1024TOMAXOCTECS_GB;                            /* Good&Bad frames received between 1024 & Max size     */
    CPU_REG32  RXUNICASTFRAMES_G;                               /* Good unicast frames received                         */
    CPU_REG32  RXLENGTHERROR;                                   /* Frames received with length error                    */
    CPU_REG32  RXOUTOFRANGETYPE;
    CPU_REG32  RXPAUSEFRAMES;                                   /* Good and valid PAUSE frames received                 */
    CPU_REG32  RXFIFOOVERFLOW;                                  /* Missed received frames due to FIFO overflow          */
    CPU_REG32  RXVLANFRAMES_GB;                                 /* Good and bad VLAN frames received                    */
    CPU_REG32  RXWATCHDOGERROR;                                 /* Frames received w/ err due to watchdog timerout err  */
    CPU_REG32  RSVD4[8u];
    CPU_REG32  MMC_IPC_INTR_MASK_RX;                            /* MMC IPC receive checksum offload interrupt mask      */
    CPU_REG32  RSVD5;
    CPU_REG32  MMC_IPC_INT_RX;                                  /* MMC receive checksum offload interrupt               */
    CPU_REG32  RSVD6;
    CPU_REG32  RXIPV4_GD_FRMS;                                  /* Good IPv4 datagrams received w/ TCP,UDP,ICMP payload */
    CPU_REG32  RXIPV4_HDRERR_FRMS;                              /* IPv4 datagrams recevied w/ header err                */
    CPU_REG32  RXIPV4_NOPAY_FRMS;
    CPU_REG32  RXIPV4_FRAG_FRMS;                                /* Good IPv4 datagrams with fragmentation               */
    CPU_REG32  RXIPV4_UDSBL_FRMS;
    CPU_REG32  RXIPV6_GD_FRMS;                                  /* Good IPv6 datagrams received w/ TCP,UDP,ICMP payload */
    CPU_REG32  RXIPV6_HDRERR_FRMS;                              /* IPv6 datagrams recevied w/ header err                */
    CPU_REG32  RXIPV6_NOPAY_FRMS;
    CPU_REG32  RXUDP_GD_FRMS;                                   /* Good IP datagrams with a good UDP payload            */
    CPU_REG32  RXUDP_ERR_FRMS;                                  /* Good IP datagrams whose UDP payload has checksum err */
    CPU_REG32  RXTCP_GD_FRMS;                                   /* Good IP datagrams with a good TCP payload            */
    CPU_REG32  RXTCP_ERR_FRMS;                                  /* Good IP datagrams whose TCP payload has checksum err */
    CPU_REG32  RXICMP_GD_FRMS;                                  /* Good IP datagrams with a good ICMP payload           */
    CPU_REG32  RXICMP_ERR_FRMS;                                 /* Good IP datagrams whose ICMP payload has checksum err*/
    CPU_REG32  RSVD7[2u];
    CPU_REG32  RXIPV4_GD_OCTETS;                                /* Bytes received in good IPv4 datagram                 */
    CPU_REG32  RXIPV4_HDRERR_OCTETS;                            /* Bytes received in IPv4 datagram with header err      */
    CPU_REG32  RXIPV4_NOPAY_OCTETS;
    CPU_REG32  RXIPV4_FRAG_OCTETS;                              /* Bytes received in fragmented IPv4 datagrams          */
    CPU_REG32  RXIPV4_UDSBL_OCTETS;
    CPU_REG32  RXIPV6_GD_OCTETS;                                /* Bytes received in good IPv6 datagram                 */
    CPU_REG32  RXIPV6_HDRERR_OCTETS;                            /* Bytes received in IPv6 datagram with header err      */
    CPU_REG32  RXIPV6_NOPAY_OCTETS;
    CPU_REG32  RXUDP_GD_OCTETS;                                 /* Bytes received in good UDP segment                   */
    CPU_REG32  RXUDP_ERR_OCTETS;                                /* Bytes received in UDP segment that had checksum err  */
    CPU_REG32  RXTCP_GD_OCTETS;                                 /* Bytes received in good TCP segment                   */
    CPU_REG32  RXTCP_ERR_OCTETS;                                /* Bytes received in TCP segment that had checksum err  */
    CPU_REG32  RXICMP_GD_OCTETS;                                /* Bytes received in good ICMP segment                  */
    CPU_REG32  RXICMP_ERR_OCTETS;                               /* Bytes received in ICMP segment that had checksum err */
    CPU_REG32  RSVD8[286u];

    CPU_REG32  MAC_TIMESTP_CTRL;                                /* Timer stamp control                                  */
    CPU_REG32  SUBSECOND_INCR;                                  /* Sub-second increment                                 */
    CPU_REG32  SECONDS;                                         /* System Time - Seconds                                */
    CPU_REG32  NANOSECONDS;                                     /* System Time - Nanoseconds                            */
    CPU_REG32  SECONDUPDATE;                                    /* System Time - Seconds update                         */
    CPU_REG32  NANOSECONDUPDATE;                                /* System Time - Nanoseconds update                     */
    CPU_REG32  ADDEND;                                          /* Time stamp addend                                    */
    CPU_REG32  TARGETSECONDS;                                   /* Target time seconds                                  */
    CPU_REG32  TARGETNANOSECONDS;                               /* Target time nanoseconds                              */
    CPU_REG32  HIGHWORD;                                        /* System Time - High word seconds                      */
    CPU_REG32  TIMESTAMPSTAT;                                   /* Time stamp status                                    */
    CPU_REG32  PPSCR;                                           /* PPC control                                          */
    CPU_REG32  ATNR;                                            /* Auxiliary time stamp-nanosecond                      */
    CPU_REG32  ATSR;                                            /* Auxiliary time stamp-seconds                         */
    CPU_REG32  RSVD9[50u];
    CPU_REG32  MAC_ADDR16_HIGH;                                 /* MAC address 16 (High)                                */
    CPU_REG32  MAC_ADDR16_LOW;                                  /* MAC address 16 (Low)                                 */
    CPU_REG32  MAC_ADDR17_HIGH;                                 /* MAC address 17 (High)                                */
    CPU_REG32  MAC_ADDR17_LOW;                                  /* MAC address 17 (Low)                                 */
    CPU_REG32  MAC_ADDR18_HIGH;                                 /* MAC address 18 (High)                                */
    CPU_REG32  MAC_ADDR18_LOW;                                  /* MAC address 18 (Low)                                 */
    CPU_REG32  MAC_ADDR19_HIGH;                                 /* MAC address 19 (High)                                */
    CPU_REG32  MAC_ADDR19_LOW;                                  /* MAC address 19 (Low)                                 */
    CPU_REG32  MAC_ADDR20_HIGH;                                 /* MAC address 20 (High)                                */
    CPU_REG32  MAC_ADDR20_LOW;                                  /* MAC address 20 (Low)                                 */
    CPU_REG32  MAC_ADDR21_HIGH;                                 /* MAC address 21 (High)                                */
    CPU_REG32  MAC_ADDR21_LOW;                                  /* MAC address 21 (Low)                                 */
    CPU_REG32  MAC_ADDR22_HIGH;                                 /* MAC address 22 (High)                                */
    CPU_REG32  MAC_ADDR22_LOW;                                  /* MAC address 22 (Low)                                 */
    CPU_REG32  MAC_ADDR23_HIGH;                                 /* MAC address 23 (High)                                */
    CPU_REG32  MAC_ADDR23_LOW;                                  /* MAC address 23 (Low)                                 */
    CPU_REG32  MAC_ADDR24_HIGH;                                 /* MAC address 24 (High)                                */
    CPU_REG32  MAC_ADDR24_LOW;                                  /* MAC address 24 (Low)                                 */
    CPU_REG32  MAC_ADDR25_HIGH;                                 /* MAC address 25 (High)                                */
    CPU_REG32  MAC_ADDR25_LOW;                                  /* MAC address 25 (Low)                                 */
    CPU_REG32  MAC_ADDR26_HIGH;                                 /* MAC address 26 (High)                                */
    CPU_REG32  MAC_ADDR26_LOW;                                  /* MAC address 26 (Low)                                 */
    CPU_REG32  MAC_ADDR27_HIGH;                                 /* MAC address 27 (High)                                */
    CPU_REG32  MAC_ADDR27_LOW;                                  /* MAC address 27 (Low)                                 */
    CPU_REG32  MAC_ADDR28_HIGH;                                 /* MAC address 28 (High)                                */
    CPU_REG32  MAC_ADDR28_LOW;                                  /* MAC address 28 (Low)                                 */
    CPU_REG32  MAC_ADDR29_HIGH;                                 /* MAC address 29 (High)                                */
    CPU_REG32  MAC_ADDR29_LOW;                                  /* MAC address 29 (Low)                                 */
    CPU_REG32  MAC_ADDR30_HIGH;                                 /* MAC address 30 (High)                                */
    CPU_REG32  MAC_ADDR30_LOW;                                  /* MAC address 30 (Low)                                 */
    CPU_REG32  MAC_ADDR31_HIGH;                                 /* MAC address 31 (High)                                */
    CPU_REG32  MAC_ADDR31_LOW;                                  /* MAC address 31 (Low)                                 */
} NET_DEV_EMAC;


typedef  struct  net_dev_edma {
                                                                /* ------------------ DMA REGISTERS ------------------- */
    CPU_REG32  DMA_BUS_MODE;                                    /* Bus Mode                                             */
    CPU_REG32  DMA_TRANS_POLL_DEMAND;                           /* Transmit poll demand                                 */
    CPU_REG32  DMA_REC_POLL_DEMAND;                             /* Receive poll demand                                  */
    CPU_REG32  DMA_REC_DES_ADDR;                                /* Receive descriptor list address                      */
    CPU_REG32  DMA_TRANS_DES_ADDR;                              /* Transmit descriptor list address                     */
    CPU_REG32  DMA_STAT;                                        /* Status                                               */
    CPU_REG32  DMA_OP_MODE;                                     /* Operation mode                                       */
    CPU_REG32  DMA_INT_EN;                                      /* Interrup enable                                      */
    CPU_REG32  DMA_MFRM_BUFOF;                                  /* Missed frame and buffer overflow counter             */
    CPU_REG32  DMA_REC_INT_WDT;                                 /* Receive interrupt watchdog timer                     */
    CPU_REG32  DMA_AXI_BUS_MODE;
    CPU_REG32  DMA_AHB_STAT;                                    /* AHB status                                           */
    CPU_REG32  RSVD12[6u];
    CPU_REG32  DMA_CURHOST_TRANS_DES;                           /* Current host transmit descriptor                     */
    CPU_REG32  DMA_CURHOST_REC_DES ;                            /* Current host receive descriptor                      */
    CPU_REG32  DMA_CURHOST_TRANS_BUF;                           /* Current host transmit buffer address                 */
    CPU_REG32  DMA_CURHOST_REC_BUF;                             /* Current host receive buffer address                  */
    CPU_REG32  RSVD13;
} NET_DEV_EDMA;


typedef  struct  net_dev_per_cfg {                              /* see Note #4                                          */
                                                                /* ----- ETHERNET PERIPHERAL & CLOCK CFG REGISTERS ---- */
    CPU_REG32  MAC_PP;                                          /* MAC peripheral property register.                    */
    CPU_REG32  MAC_PC;                                          /* MAC peripheral configuration register.               */
    CPU_REG32  MAC_CC;                                          /* MAC clock configuration register.                    */
} NET_DEV_PER_CFG;


/*
*********************************************************************************************************
*                                        NetDev_GMAC_Init()
*
* Description : Initialize generic GMAC devices.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_GMAC_Init (NET_IF   *pif,
                                NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_GMAC, perr);
}


/*
*********************************************************************************************************
*                                        NetDev_LPCxx_Enet_Init()
*
* Description : Initialize LPCxx_Enet device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_LPCxx_Enet_Init (NET_IF   *pif,
                                      NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_LPCxx_ENET, perr);
}


/*
*********************************************************************************************************
*                                       NetDev_XMC4000_Enet_Init()
*
* Description : Initialize XMC4000_Enet device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_XMC4000_Enet_Init (NET_IF   *pif,
                                      NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_XMC4000_ENET, perr);
}


/*
*********************************************************************************************************
*                                       NetDev_TM4C12x_Enet_Init()
*
* Description : Initialize TM4C12x_Enet device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_TM4C12x_Enet_Init (NET_IF   *pif,
                                        NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_TM4C12x_ENET, perr);
}

/*
*********************************************************************************************************
*                                        NetDev_HPS_EMAC_Init()
*
* Description : Initialize HPS_EMAC device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_HPS_EMAC_Init (NET_IF   *pif,
                                    NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_HPS_EMAC, perr);
}

/*
*********************************************************************************************************
*                                      NetDev_STM32Fx_EMAC_Init()
*
* Description : Initialize STM32Fx_EMAC device.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_STM32Fx_EMAC_Init (NET_IF   *pif,
                                        NET_ERR  *perr)
{
    NetDev_InitHandler(pif, NET_DEV_STM32Fx_EMAC, perr);
}


/*
*********************************************************************************************************
*                                       NetDev_PHY_InitHandler()
*
* Description : If available, initialize internal or external PHY configurations for device.
*
* Argument(s) : pif             Pointer to the interface requiring service.
*
*               gmac_dev_type   GMAC device type :
*
*                                   NET_DEV_GMAC                Generic GMAC         device.
*                                   NET_DEV_LPCxx_ENET          Generic LPCxx_ENET   device.
*                                   NET_DEV_XMC4000_ENET        Generic XMC4000_ENET device.
*                                   NET_DEV_TM4C12x_ENET        Generic TM4C12x_ENET device.
*
*               perr            Pointer to return error code.
*                                   NET_PHY_ERR_NONE            No Error.
*                                   NET_PHY_ERR_INVALID_CFG     Invalid PHY configuration.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_InitHandler().
*
* Note(s)     : (1) if EMAC_PC does not show the new configurations when using internal PHY, then perfom
*                   a reset on the ethernet PHY peripheral for the new configurations to be latched.
*
*********************************************************************************************************
*/

static  void  NetDev_PHY_InitHandler (NET_IF      *pif,
                                      CPU_INT16U   gmac_dev_type,
                                      NET_ERR     *perr)
{
    NET_PHY_CFG_ETHER  *p_phy_cfg;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_PER_CFG    *pdev_per_cfg;
    CPU_INT08U          duplex;
    CPU_INT16U          spd;
    CPU_INT32U          reg_val;


    p_phy_cfg = (NET_PHY_CFG_ETHER *)pif->Ext_Cfg;
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    spd       =  p_phy_cfg->Spd;
    duplex    =  p_phy_cfg->Duplex;

    switch (gmac_dev_type) {
        case NET_DEV_TM4C12x_ENET:
                                                                /* Overlay PER reg struct on top of PER_CFG base addr.  */
             pdev_per_cfg = (NET_DEV_PER_CFG *)(pdev_cfg->BaseAddr + NET_DEV_TM4C12x_ENET_CFG_REG_OFFSET);

             if (((spd    != NET_PHY_SPD_10 )     &&
                  (spd    != NET_PHY_SPD_100))    ||
                 ((duplex != NET_PHY_DUPLEX_HALF) &&
                  (duplex != NET_PHY_DUPLEX_FULL))) {
                                                                /* Enable Auto-negotiation and the mode supports ...    */
                 reg_val = GMAC_PC_PHY_AN_10B_100B_HF_DUPLEX;   /* ...10B-T, Half/Full duplex;100B-TX, Half/Full duplex */

             } else {
                 switch (spd) {
                     case NET_PHY_SPD_10:
                          if (duplex == NET_PHY_DUPLEX_HALF) {  /* Set 10Base-T Half Duplex.                            */
                              reg_val = GMAC_PC_PHY_FORCE_10B_H_DUPLEX;
                          } else {                              /* Set 10Base-T Full Duplex.                            */
                              reg_val = GMAC_PC_PHY_FORCE_10B_F_DUPLEX;
                          }
                          break;


                     case NET_PHY_SPD_100:
                          if (duplex == NET_PHY_DUPLEX_HALF) {  /* Set 100Base-T Half Duplex.                           */
                              reg_val = GMAC_PC_PHY_FORCE_100B_H_DUPLEX;
                          } else {                              /* Set 100Base-T Full Duplex.                           */
                              reg_val = GMAC_PC_PHY_FORCE_100B_F_DUPLEX;
                          }
                          break;


                     default:
                         *perr = NET_PHY_ERR_INVALID_CFG;
                          return;
                 }

             }

             DEF_BIT_SET(reg_val, GMAC_PC_MDIXEN);              /* Enable automatic cross-over.                         */

             DEF_BIT_CLR(pdev_per_cfg->MAC_CC, GMAC_CC_CLKEN);  /* Disable the external clock                           */

             if (p_phy_cfg->Type == NET_PHY_TYPE_INT) {         /* -------------- CONFIGURE INTERNAL PHY -------------- */
                 DEF_BIT_CLR(reg_val, GMAC_PC_PHYEXT);          /* Select internal PHY.                                 */
                 DEF_BIT_CLR(reg_val, GMAC_PC_PINTFS);          /* Select MII interface.                                */

             } else {                                           /* -------------- CONFIGURE EXTERNAL PHY -------------- */
                 DEF_BIT_SET(reg_val, GMAC_PC_PHYEXT);          /* Select external PHY.                                 */
                 if (p_phy_cfg->BusMode == NET_PHY_BUS_MODE_MII) {
                     DEF_BIT_CLR(reg_val, GMAC_PC_PINTFS);      /* Select MII interface.                                */

                 } else {
                     DEF_BIT_CLR(reg_val, GMAC_PC_PINTFS_RMII); /* Select RMII interface.                               */
                                                                /* Select & enable the ext. clock from the RMII PHY     */
                     DEF_BIT_SET(pdev_per_cfg->MAC_CC, GMAC_CC_CLKEN);
                 }
             }

             pdev_per_cfg->MAC_PC = reg_val;                    /* See Note #1.                                         */
             break;


        case NET_DEV_GMAC:
        case NET_DEV_LPCxx_ENET:
        case NET_DEV_XMC4000_ENET:
        default:
             break;
    }

   *perr =  NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_InitHandler()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Initialize required clock sources
*                   (b) Initialize external interrupt controller
*                   (c) Initialize external GPIO controller
*                   (d) Initialize driver state variables
*                   (e) Allocate memory for device DMA descriptors
*                   (f) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
*                       (2) Disable device interrupts
*                       (3) Disable device receiver and transmitter
*                       (4) Other necessary device initialization
*
* Argument(s) : pif             Pointer to the interface requiring service.
*
*               gmac_dev_type   GMAC device type :
*
*                                   NET_DEV_GMAC                Generic GMAC         device.
*                                   NET_DEV_LPCxx_ENET          Generic LPCxx_ENET   device.
*                                   NET_DEV_XMC4000_ENET        Generic XMC4000_ENET device.
*                                   NET_DEV_TM4C12x_ENET        Generic TM4C12x_ENET device.
*                                   NET_DEV_HPS_EMAC            Altera SOC HPS EMAC  device.
*                                   NET_DEV_STM32Fx_EMAC        Generic STM32Fx_EMAC device.
*
*               perr            Pointer to return error code.
*                                   NET_DEV_ERR_NONE            No Error.
*                                   NET_DEV_ERR_INIT            General initialization error.
*                                   NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*                                   NET_DEV_ERR_INVALID_CFG     Invalid device configuration.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_GMAC_Init(),
*               NetDev_LPCxx_Enet_Init().
*               NetDev_XMC4000_Enet_Init().
*               NetDev_TM4C12x_Enet_Init().
*               NetDev_HPS_EMAC_Init().
*               NetDev_STM32Fx_EMAC_Init().
*
* Note(s)     : (2) The application developer SHOULD define NetDev_CfgClk() within net_bsp.c
*                   in order to properly enable clocks for specified network interface.  In
*                   some cases, a device may require clocks to be enabled for BOTH the device
*                   and accessory peripheral modules such as GPIO.  A call to this function
*                   MAY need to occur BEFORE any device register accesses are made.  In the
*                   event that a device does NOT require any external clocks to be enabled,
*                   it is recommended that the device driver still call the NetBSP fuction
*                   which may in turn leave the section for the specific interface number
*                   empty.
*
*               (3) The application developer SHOULD define NetDev_CfgIntCtrl() within net_bsp.c
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
*               (4) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.  Micrium recommends defining and calling this NetBSP
*                   function even if no additional GPIO initialization is required.
*
*               (5) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (6) All device drivers that store instance specific data MUST declare all
*                   instance specific variables within the device data area defined above.
*
*               (7) Drivers SHOULD validate device configuration values and set *perr to
*                   NET_DEV_ERR_INVALID_CFG if unacceptible values have been specified. Fields
*                   of interest generally include, but are not limited to :
*
*                   (a) pdev_cfg->RxBufAlignOctets
*                   (b) pdev_cfg->RxBufLargeSize
*
*               (8) Many DMA devices may generate only one interrupt for several ready receive
*                   descriptors.  In order to accommodate this, it is recommended that all DMA
*                   based drivers count the number of ready receive descriptors during the
*                   receive event and signal the receive task accordingly ONLY for those
*                   NEW descriptors which have not yet been accounted for.  Each time a
*                   descriptor is processed (or discarded) the count for acknowledged and
*                   unprocessed frames should be decremented by 1.  This function initializes the
*                   acknowledged receive descriptor count to 0.
*
*              (9) Descriptors are typically required to be contiguous in memory.  Allocation of
*                   descriptors MUST occur as a single contigous block of memory.  The driver may
*                   use pointer arithmetic to sub-divide and traverse the descriptor list.
*
*              (10) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*
*              (11) Some drivers MAY require knowledge of the Phy configuration in order
*                   to properly configure the MAC with the correct Phy bus mode, speed and
*                   duplex settings.  If a driver requires access to the Phy configuration,
*                   then the driver MUST validate the pif->Phy_Cfg pointer by checking for
*                   a NULL pointer BEFORE attempting to access members of the Phy
*                   configuration structure.  Phy configuration fields of interest generally
*                   include, but are  not limited to :
*
*                   (a) pphy_cfg->Type :
*
*                       (1) NET_PHY_TYPE_INT            Phy integrated with MAC.
*                       (2) NET_PHY_TYPE_EXT            Phy externally attached to MAC.
*
*                   (b) pphy_cfg->BusMode :
*
*                       (1) NET_PHY_BUS_MODE_MII        Phy bus mode configured to MII.
*                       (2) NET_PHY_BUS_MODE_RMII       Phy bus mode configured to RMII.
*                       (3) NET_PHY_BUS_MODE_SMII       Phy bus mode configured to SMII.
*
*                   (c) pphy_cfg->Spd :
*
*                       (1) NET_PHY_SPD_0               Phy link speed unknown or NOT linked.
*                       (2) NET_PHY_SPD_10              Phy link speed configured to  10   mbit/s.
*                       (3) NET_PHY_SPD_100             Phy link speed configured to  100  mbit/s.
*                       (4) NET_PHY_SPD_1000            Phy link speed configured to  1000 mbit/s.
*                       (5) NET_PHY_SPD_AUTO            Phy link speed configured for auto-negotiation.
*
*                   (d) pphy_cfg->Duplex :
*
*                       (1) NET_PHY_DUPLEX_UNKNOWN      Phy link duplex unknown or link not established.
*                       (2) NET_PHY_DUPLEX_HALF         Phy link duplex configured to  half duplex.
*                       (3) NET_PHY_DUPLEX_FULL         Phy link duplex configured to  full duplex.
*                       (4) NET_PHY_DUPLEX_AUTO         Phy link duplex configured for auto-negotiation.
*
*              (12) When alternate descriptor is chosen without Time Stamp or Full IPC Offload feature,
*                   the descriptor size is always 4 DWORDs (DES0-DES3). This only applies to the Infineon
*                   XMC4000 family.
*********************************************************************************************************
*/

static  void  NetDev_InitHandler (NET_IF      *pif,
                                  CPU_INT16U   gmac_dev_type,
                                  NET_ERR     *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev;
    NET_DEV_EDMA       *pdev_dma;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_INT32U          input_freq;
    CPU_INT32U          mdc_div;
    CPU_INT32U          timeout;
    CPU_SIZE_T          reqd_octets;
    CPU_SIZE_T          nbytes;
    LIB_ERR             lib_err;


                                                                /* -------- OBTAIN REFERENCE TO CFGs/REGs/BSP --------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev     = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;

                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf size.                                */
    if ((pdev_cfg->RxBufLargeSize   % RX_BUF_SIZE_MULT   != 0u)) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* Rx buf's MUST be a mult of 16 octets.                */
         return;
    }
                                                                /* Validate Rx buf alignment.                           */
    if ((pdev_cfg->RxBufAlignOctets % RX_DESC_BUF_ALIGN) != 0u) {
        *perr = NET_DEV_ERR_INVALID_CFG;
         return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pdev_cfg->RxDescNbr < 2u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

    if (pdev_cfg->TxDescNbr < 2u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Rx buf size.                                */
    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )pif->Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF       *)0u,
                                     (NET_BUF_SIZE   )buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf alignment.                           */
    if ((pdev_cfg->TxBufAlignOctets % TX_DESC_BUF_ALIGN != 0u)) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* Tx buf's MUST be aligned to 1 16 BYTE boundary.      */
         return;
    }

                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* ------------- ALLOCATE DEV DATA AREA --------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4u,
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;

    pdev_data->MAC_Type = gmac_dev_type;

    switch (gmac_dev_type) {
        case NET_DEV_TM4C12x_ENET:
                                                                /* Overlay DMA reg struct on top of DMA base addr.      */
             pdev_dma = (NET_DEV_EDMA *)(pdev_cfg->BaseAddr + NET_DEV_TM4C12x_ENET_DMA_REG_OFFSET);
                                                                /* Store EMAC DMA base address.                         */
             pdev_data->EMAC_DMABaseAddr = (pdev_cfg->BaseAddr + NET_DEV_TM4C12x_ENET_DMA_REG_OFFSET);
             break;


        case NET_DEV_GMAC:
        case NET_DEV_LPCxx_ENET:
        case NET_DEV_XMC4000_ENET:
        case NET_DEV_HPS_EMAC:
        case NET_DEV_STM32Fx_EMAC:
        default:
                                                                /* Overlay DMA reg struct on top of DMA base addr.      */
             pdev_dma = (NET_DEV_EDMA *)(pdev_cfg->BaseAddr + NET_DEV_GMAC_DFLT_DMA_REG_OFFSET);

             pdev_data->EMAC_DMABaseAddr = (pdev_cfg->BaseAddr + NET_DEV_GMAC_DFLT_DMA_REG_OFFSET);
             break;
    }
                                                                /* ------------ ALLOCATE DESCRIPTOR LISTS ------------- */

    if ((gmac_dev_type != NET_DEV_HPS_EMAC    ) &&
        (gmac_dev_type != NET_DEV_STM32Fx_EMAC)) {
        nbytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);        /* Allocate Rx desc.                                    */
        pdev_data->RxDescPtrStart = (DEV_DESC *)Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                              (CPU_SIZE_T  ) pdev_cfg->RxBufAlignOctets,
                                                              (CPU_SIZE_T *)&reqd_octets,
                                                              (LIB_ERR    *)&lib_err);
        if (pdev_data->RxDescPtrStart == (void *)0) {
           *perr = NET_DEV_ERR_MEM_ALLOC;
            return;
        }

        nbytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);        /* Allocate Tx desc's.                                  */
        pdev_data->TxDescPtrStart = (DEV_DESC *)Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                              (CPU_SIZE_T  ) pdev_cfg->TxBufAlignOctets,
                                                              (CPU_SIZE_T *)&reqd_octets,
                                                              (LIB_ERR    *)&lib_err);
        if (pdev_data->TxDescPtrStart == (void *)0) {
           *perr = NET_DEV_ERR_MEM_ALLOC;
            return;
        }
    } else {
                                                                    /* ------- ALLOCATE MEMORY FOR DMA DESCRIPTORS -------- */
        nbytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC_EXT);        /* Determine block size.                                */
        Mem_PoolCreate ((MEM_POOL   *)&pdev_data->RxDescPool,       /* Pass a pointer to the mem pool to create.            */
                        (void       *) pdev_cfg->MemAddr,           /* From the dedicated memory.                           */
                        (CPU_SIZE_T  ) pdev_cfg->MemSize,           /* Dedicated area size.                                 */
                        (CPU_SIZE_T  ) 1,                           /* Allocate 1 block.                                    */
                        (CPU_SIZE_T  ) nbytes,                      /* Block size large enough to hold all Rx descriptors.  */
                        (CPU_SIZE_T  ) 32u,                         /* Block alignment (see Note #8).                       */
                        (CPU_SIZE_T *)&reqd_octets,                 /* Optional, ptr to variable to store rem nbr bytes.    */
                        (LIB_ERR    *)&lib_err);                    /* Ptr to variable to return an error code.             */
        if (lib_err != LIB_MEM_ERR_NONE) {
            *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
            return;
        }

        pdev_data->RxDescPtrStart= (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *)&pdev_data->RxDescPool,
                                                              (CPU_SIZE_T) nbytes,
                                                              (LIB_ERR  *)&lib_err);


        nbytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC_EXT);        /* Determine block size.                                */
        Mem_PoolCreate ((MEM_POOL   *)&pdev_data->TxDescPool,       /* Pass a pointer to the mem pool to create.            */
                        (void       *) pdev_cfg->MemAddr,           /* From the dedicated memory.                           */
                        (CPU_SIZE_T  ) pdev_cfg->MemSize,           /* Dedicated area size.                                 */
                        (CPU_SIZE_T  ) 1,                           /* Allocate 1 block.                                    */
                        (CPU_SIZE_T  ) nbytes,                      /* Block size large enough to hold all Tx descriptors.  */
                        (CPU_SIZE_T  ) 32u,                         /* Block alignment (see Note #8).                       */
                        (CPU_SIZE_T *)&reqd_octets,                 /* Optional, ptr to variable to store rem nbr bytes.    */
                        (LIB_ERR    *)&lib_err);                    /* Ptr to variable to return an error code.             */
        if (lib_err != LIB_MEM_ERR_NONE) {
            *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
            return;
        }

        pdev_data->TxDescPtrStart= (DEV_DESC *)Mem_PoolBlkGet((MEM_POOL *)&pdev_data->TxDescPool,
                                                              (CPU_SIZE_T) nbytes,
                                                              (LIB_ERR  *)&lib_err);
    }

                                                                /* Enable module clks (GPIO & EMAC) [see Note #2].      */
    pdev_bsp->CfgClk(pif, perr);

    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_PHY_InitHandler(pif, gmac_dev_type, perr);           /* Cfg. driver internal or external PHY if necessary.   */
    if (*perr != NET_PHY_ERR_NONE) {
        return;
    }

    pdev_dma->DMA_BUS_MODE |= GMAC_DMABMR_SWR;                  /* Reset MAC reg's to dflt val's.                       */
    pdev_dma->DMA_INT_EN    = 0u;                               /* Dis. all    int. src's.                              */
    pdev_dma->DMA_STAT      = 0xFFFFFFFFu;                      /* Clr pending int. src's.                              */
    pdev->MAC_INTR_MASK     = 0xFFFFFFFF;                       /* Disable MAC interrupts.                              */
    pdev->MMC_INTR_MASK_RX  = 0xFFFFFFFF;
    pdev->MMC_INTR_MASK_TX  = 0xFFFFFFFF;
    pdev->MMC_IPC_INTR_MASK_RX = 0xFFFFFFFF;

                                                                /* Configure ext int ctrl'r (see Note #3).              */
    pdev_bsp->CfgIntCtrl(pif, perr);

    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }

                                                                /* Configure MAC / Phy GPIO (see Note #4).              */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }
                                                                /* -------------------- CFG MAC ----------------------- */
    timeout = GMAC_SWR_TO;                                      /* Wait reset to complete.                              */
    while (((pdev_dma->DMA_BUS_MODE & GMAC_DMABMR_SWR) != 0u)  &&
           (timeout > 0u)) {
        KAL_Dly(1);
        timeout--;
    }
    if (timeout == 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Get MDC clk freq (in Hz).                            */
    input_freq = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    if (input_freq > 250000000UL) {                             /* Determine MDC clk div.                               */
        mdc_div = (CPU_INT32U)GMAC_GAR_CR_DIV122_124;
    } else if (input_freq > 150000000UL) {
        mdc_div = (CPU_INT32U)GMAC_GAR_CR_DIV102;
    } else if (input_freq > 100000000UL) {
        mdc_div = (CPU_INT32U)GMAC_GAR_CR_DIV62;
    } else if (input_freq >  60000000UL) {
        mdc_div = (CPU_INT32U)GMAC_GAR_CR_DIV42;
    } else if (input_freq >  35000000UL) {
        mdc_div = (CPU_INT32U)GMAC_GAR_CR_DIV26;
    } else {
        mdc_div = (CPU_INT32U)GMAC_GAR_CR_DIV16;
    }

    pdev->MAC_MII_ADDR &=  GMAC_GAR_CR_MASK;                    /* Clr MDC clk div bits (div 64).                       */
    pdev->MAC_MII_ADDR |=  mdc_div;                             /* Cfg MDC clk div.                                     */

    pdev->MAC_CONFIG    = (GMAC_MCR_WD         |                /* Disable watchdog.                                    */
                           GMAC_MCR_IFG_96BITS |                /* Cfg Inter-frame gap to 96 bits.                      */
                           GMAC_MCR_FES        |                /* Cfg initial MAC speed  to 100Mbps.                   */
                           GMAC_MCR_DM         |                /* Cfg initial MAC duplex to FULL.                      */
                           GMAC_MCR_DO         |                /* Disable receive own frames when in half duplex mode. */
                           GMAC_MCR_PS         |                /* Alwasys write 1 to PS bit                            */
                           GMAC_MCR_BL_10);                     /* Cfg back-off limit to MIN(n, 10).                    */

    if ((gmac_dev_type == NET_DEV_HPS_EMAC    ) ||
        (gmac_dev_type == NET_DEV_STM32Fx_EMAC)) {
        pdev->MAC_CONFIG |= GMAC_MCR_IPC;                       /* Enable checksum offloading.                          */
    }

    pdev->MAC_FRAME_FILTER = (GMAC_MFFR_HPF    |                /* Cfg Rx filter for Hash or Perfect match.             */
                              GMAC_MFFR_HMC);                   /* Enable multicast hashing.                            */

    pdev->MAC_FLOW_CTRL    =  GMAC_FCR_ZQPD;                    /* Disable automatic generation of pause frames.        */

                                                                /* -------------------- CFG DMA ----------------------- */
    pdev_dma->DMA_OP_MODE  =  GMAC_DMAOMR_DFF;                  /* Disable flushing of Rx desc buf's when RBNA.         */

    switch (gmac_dev_type) {
        case NET_DEV_GMAC:
        case NET_DEV_XMC4000_ENET:
        case NET_DEV_TM4C12x_ENET:
             pdev_dma->DMA_OP_MODE |= (GMAC_DMAOMR_RSF  |       /* Defer Rx until entire frame is within the Rx FIFO.   */
                                       GMAC_DMAOMR_TSF);        /* Defer Tx until entire frame is within the Tx FIFO.   */

             pdev_dma->DMA_BUS_MODE = (GMAC_DMABMR_AAL                 | /* First DMA transfer is 16 byte aligned.      */
                                       GMAC_DMABMR_RX_DMA_1_BEAT       | /* 1 beat max burst per Rx DMA transfer.       */
                                       GMAC_DMABMR_TX_DMA_1_BEAT       | /* 1 beat max burst per Tx DMA transfer.       */
                                      (GMAC_DMA_DESC_SKIP_LEN << 2u)   | /* desc's are contiguous in mem.               */
                                       GMAC_DMABMR_RX_TX_DMA_PRIO_1_1  | /* Rx always take precedence over Tx.          */
                                       GMAC_DMABMR_USP);                 /* Use seperate PBL for Rx & Tx burst len.     */

             if (gmac_dev_type == NET_DEV_XMC4000_ENET) {
                 pdev_dma->DMA_BUS_MODE |= GMAC_DMABMR_EDFE;    /* See Note 12.                                         */
             }
             break;

        case NET_DEV_HPS_EMAC:
             pdev_dma->DMA_BUS_MODE     = GMAC_DMABMR_AAL            |
                                          GMAC_DMABMR_PBL            |
                                          GMAC_DMABMR_USP            |
                                          GMAC_DMABMR_RX_DMA_32_BEAT |
                                          GMAC_DMABMR_TX_DMA_32_BEAT |
                                          GMAC_DMABMR_EDFE;
             pdev_dma->DMA_AXI_BUS_MODE = (7 << 16u) | (7 << 20u); /* Maximum AXI request set to 8.                      */
             pdev_dma->DMA_OP_MODE     |=  GMAC_DMAOMR_RSF |
                                           GMAC_DMAOMR_TSF |
                                           GMAC_DMAOMR_OSF;
             break;

        case NET_DEV_STM32Fx_EMAC:
             pdev_dma->DMA_OP_MODE |= (GMAC_DMAOMR_RSF |
                                       GMAC_DMAOMR_TSF |
                                       GMAC_DMAOMR_OSF);

             pdev_dma->DMA_BUS_MODE = (GMAC_DMABMR_AAL                |
                                       GMAC_DMABMR_PBL                |
                                       GMAC_DMABMR_RX_DMA_32_BEAT     |
                                       GMAC_DMABMR_TX_DMA_32_BEAT     |
                                       GMAC_DMABMR_RX_TX_DMA_PRIO_1_1 |
                                       GMAC_DMABMR_USP);
             break;

        case NET_DEV_LPCxx_ENET:
        default:
             break;
    }

   *perr = NET_DEV_ERR_NONE;
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
*                               NET_DEV_ERR_NONE                Ethernet device successfully started.
*
*                                                               - RETURNED BY NetIF_AddrHW_SetHandler() : --
*                               NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT         Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               -- RETURNED BY NetIF_DevCfgTxRdySignal() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_OS_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
*                                                               -------- RETURNED BY RxDescInit() : --------
*                               NET_BUF_ERR_NONE                Network buffer data area successfully allocated
*                                                               & initialized.
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                               NET_BUF_ERR_INVALID_IX          Invalid index.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                               maximum network buffer data area size
*                                                               available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                               buffer's data area.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               -------- RETURNED BY TxDescInit() : --------
*                               NET_DEV_ERR_NONE                Ethernet device successfully started.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (2) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, DMA based interfaces may have several
*                   frames configured for transmission before blocking is required. The number
*                   of queued transmit frames depends on the number of configured transmit
*                   descriptors.
*
*               (3) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_Ether_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (4) The device hardware address is set from one of the data sources below. Each source
*                   is listed in the order of precedence.
*
*                   (a) Device Configuration Structure       Configure a valid HW address during
*                                                                compile time.
*
*                                                            Configure either "00:00:00:00:00:00" or
*                                                                an empty string, "", in order to
*                                                                configure the HW address using using
*                                                                method (b).
*
*                   (b) NetIF_Ether_HW_AddrSet()             Call NetIF_Ether_HW_AddrSet() if the HW
*                                                                address needs to be configured via
*                                                                run-time from a different data
*                                                                source. E.g. Non auto-loading
*                                                                memory such as I2C or SPI EEPROM.
*                                                               (see Note #3).
*
*                   (c) Auto-Loading via EEPROM.             If neither options a) or b) are used,
*                                                                the IF layer will use the HW address
*                                                                obtained from the network hardware
*                                                                address registers.
*********************************************************************************************************
*/

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev;
    NET_DEV_EDMA       *pdev_dma;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
                                                                /* Overlay DMA reg struct on top of DMA base addr.      */
    pdev_dma  = (NET_DEV_EDMA      *)pdev_data->EMAC_DMABaseAddr;


#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->StatRxCtrRx             = 0u;
    pdev_data->StatRxCtrRxISR          = 0u;
    pdev_data->StatTxCtrTxISR          = 0u;
    pdev_data->StatRxCtrTaskSignalISR  = 0u;
    pdev_data->StatRxCtrTaskSignalSelf = 0u;
    pdev_data->StatRxCtrTaskSignalNone = 0u;
#endif

#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    pdev_data->ErrRxCtrDataAreaAlloc   = 0u;
    pdev_data->ErrRxCtrROS             = 0u;
    pdev_data->ErrRxCtrRBUS            = 0u;
    pdev_data->ErrRxCtrRPSS            = 0u;
    pdev_data->ErrRxCtrDescES          = 0u;
    pdev_data->ErrRxCtrDescOWN         = 0u;
    pdev_data->ErrRxCtrRunt            = 0u;
    pdev_data->ErrRxTxCtrFatalBusErr   = 0u;
    pdev_data->ErrRxCtrTaskSignalFail  = 0u;
#endif

                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #3 & #4.                                   */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0u],                            /* ... (see Note #4a).                                  */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0u],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.    */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Else get  app-configured IF layer HW MAC address, ...*/
                                                                /* ... if any (see Note #4b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0u], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0u], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any (see Note #4c).           */
            hw_addr[0u] = (pdev->MAC_ADDR0_LOW  >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1u] = (pdev->MAC_ADDR0_LOW  >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2u] = (pdev->MAC_ADDR0_LOW  >> (2u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3u] = (pdev->MAC_ADDR0_LOW  >> (3u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            hw_addr[4u] = (pdev->MAC_ADDR0_HIGH >> (0u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5u] = (pdev->MAC_ADDR0_HIGH >> (1u * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,    /* Configure IF layer to use automatically-loaded ...   */
                                    (CPU_INT08U *)&hw_addr[0u], /* ... HW MAC address.                                  */
                                    (CPU_INT08U  ) sizeof(hw_addr),
                                    (NET_ERR    *) perr);
            if (*perr != NET_IF_ERR_NONE) {                     /* No valid HW MAC address configured, return error.    */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        pdev->MAC_ADDR0_HIGH = (((CPU_INT32U)hw_addr[4u] << (0u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[5u] << (1u * DEF_INT_08_NBR_BITS)));

        pdev->MAC_ADDR0_LOW  = (((CPU_INT32U)hw_addr[0u] << (0u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[1u] << (1u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[2u] << (2u * DEF_INT_08_NBR_BITS)) |
                                ((CPU_INT32U)hw_addr[3u] << (3u * DEF_INT_08_NBR_BITS)));
    }

                                                                /* ------------------ CFG MULTICAST ------------------- */
    pdev->MAC_HASHTABLE_HIGH = 0u;                              /* Clr multicast hash filter hi bits.                   */
    pdev->MAC_HASHTABLE_LOW  = 0u;                              /* Clr multicast hash filter lo bits.                   */

#ifdef  NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif


    NetDev_RxDescInit(pif, perr);                               /* Init Rx desc's.                                      */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Init Tx desc'c.                                      */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #2.                                         */
                            pdev_cfg->TxDescNbr,
                            perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }

    pdev_data->TxDescNbrAvail = pdev_cfg->TxDescNbr;

    pdev->MAC_CONFIG      |= (GMAC_MCR_TE     |                 /* Enable MAC Tx.                                       */
                              GMAC_MCR_RE);                     /* Enable MAC Rx.                                       */

    pdev_dma->DMA_OP_MODE |= (GMAC_DMAOMR_FTF |                 /* Flush  DMA Tx FIFO.                                  */
                              GMAC_DMAOMR_ST  |                 /* Enable DMA Tx.                                       */
                              GMAC_DMAOMR_SR);                  /* Enable DMA Rx.                                       */

    pdev_dma->DMA_INT_EN   = (GMAC_DMAIER_NIE |                 /* Normal int. summary.                                 */
                              GMAC_DMAIER_RIE |                 /* Rx     int.                                          */
                              GMAC_DMAIER_TIE);                 /* Tx     int.                                          */

    pdev_dma->DMA_INT_EN  |= (GMAC_DMAIER_AIE |                 /* Abnormal int. summary.                               */
                              GMAC_DMAIER_OVE |                 /* Rx (FIFO) overflow.                                  */
                              GMAC_DMAIER_RUE |                 /* Rx buffer unavailable.                               */
                              GMAC_DMAIER_FBE);                 /* Fatal bus err.                                       */

    (void)&pdev_data;                                           /* Prevent compiler warning.                            */

   *perr = NET_DEV_ERR_NONE;
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
*
*               (3) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev;
    NET_DEV_EDMA       *pdev_dma;
    DEV_DESC           *pdesc;
    CPU_INT08U          i;
    NET_ERR             err;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */
                                                                /* Overlay DMA reg struct on top of DMA base addr.      */
    pdev_dma  = (NET_DEV_EDMA      *)pdev_data->EMAC_DMABaseAddr;

    pdev->MAC_CONFIG      &= ~GMAC_MCR_TE;                      /* Disable the MAC transmission.                        */
    pdev_dma->DMA_OP_MODE |=  GMAC_DMAOMR_FTF;                  /* Set the Flush Transmit FIFO bit.                     */
    pdev->MAC_CONFIG      &= ~GMAC_MCR_RE;                      /* Disable the MAC reception.                           */
    pdev_dma->DMA_OP_MODE &= ~GMAC_DMAOMR_ST;                   /* Disable the DMA transmission.                        */
    pdev_dma->DMA_OP_MODE &= ~GMAC_DMAOMR_SR;                   /* Disable the DMA reception.                           */


                                                                /* ------------- DISABLE & CLEAR INT'S ---------------- */
    pdev_dma->DMA_INT_EN = 0u;                                  /* Disable Rx, Tx and other supported int. src.         */
    pdev_dma->DMA_STAT   = 0xFFFFFFFFu;                         /* Clear all pending int. sources.                      */

                                                                /* --------------- FREE RX DESCRIPTORS ---------------- */
    NetDev_RxDescFreeAll(pif, &err);
   (void)&err;                                                  /* Ignore possible dealloc err (see Note #2b2).         */

                                                                /* ------------ FREE USED TX DESCRIPTORS -------------- */
    pdesc = &pdev_data->TxDescPtrStart[0u];
    for (i = 0u; i < pdev_cfg->TxDescNbr; i++) {                /* Dealloc ALL tx bufs (see Note #2a2).                 */
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->Addr, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdesc++;
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Determine which receive descriptor caused the interrupt
*                   (b) Obtain pointer to data area to replace existing data area
*                   (c) Reconfigure descriptor with pointer to new data area
*                   (d) Set return values.  Pointer to received data area and size
*                   (e) Update current receive descriptor pointer
*                   (f) Increment counters.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               p_data  Pointer to pointer to received DMA data area. The received data
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
* Note(s)     : (2) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (3) If a receive error occurs and the descriptor is invalid then the function
*                   SHOULD return 0 for the size, a NULL pointer to the data area AND an
*                   error equal to NET_DEV_ERR_RX.
*
*                   (a) If the next expected ready / valid descriptor is NOT owned by
*                       software, then there is descriptor pointer corruption and the
*                       driver should NOT increment the current receive descriptor
*                       pointer.
*
*                   (b) If the descriptor IS valid, but an error is indicated within
*                       the descriptor status bits, or length field, then the driver
*                       MUST increment the current receive descriptor pointer and discard
*                       the received frame.
*
*                   (c) If a new data area is unavailable, the driver MUST increment
*                       the current receive descriptor pointer and discard the received
*                       frame.  This will invoke the DMA to re-use the existing configured
*                       data area.
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_DATA  *pdev_data;
    NET_DEV_EDMA  *pdev_dma;
    DEV_DESC      *pdesc;
    CPU_INT08U    *pbuf_new;
    CPU_INT16S     len;
    CPU_SR_ALLOC();


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_dma  = (NET_DEV_EDMA *)pdev_data->EMAC_DMABaseAddr;    /* Overlay DMA reg struct on top of DMA base addr.      */
    pdesc     = (DEV_DESC     *)pdev_data->RxDescPtrCur;        /* Obtain ptr to next ready desc.                       */


                                                                /* ------------- CHECK FOR RECEIVE ERRORS ------------- */
    if ((pdesc->Status & GMAC_DMA_RX_DESC_OWN) != 0u) {         /* Check if the desc is owned by DMA.                   */
        CPU_CRITICAL_ENTER();
        pdev_dma->DMA_INT_EN |= (GMAC_DMAIER_RIE  |             /* Re-Enable Rx int's.                                  */
                                 GMAC_DMAIER_OVE  |             /* Rx (FIFO) overrun.                                   */
                                 GMAC_DMAIER_RUE  |             /* Rx buffer unavailable.                               */
                                 GMAC_DMAIER_RSE);              /* Rx process stopped.                                  */
        CPU_CRITICAL_EXIT();
        NET_CTR_ERR_INC(pdev_data->ErrRxCtrDescOWN);
       *size   = (CPU_INT16U  )0u;
       *p_data = (CPU_INT08U *)0u;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

    if ((pdesc->Status & GMAC_DMA_RX_DESC_ES) != 0u) {          /* If err Summary reports err's, including Rx overflow. */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard recv'd frame (see Note #3b)                  */
        NET_CTR_ERR_INC(pdev_data->ErrRxCtrDescES);
       *size   = (CPU_INT16U  )0u;
       *p_data = (CPU_INT08U *)0u;
       *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }

                                                                /* Shift and subtract 4 bytes of the CRC.               */
    len = ((pdesc->Status & GMAC_DMA_RX_DESC_FL) >> GMAC_DMA_RX_DESC_FRAME_LEN_SHIFT) - 4u;

    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {                    /* If frame is a runt.                                  */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame (see Note #3b).               */
        NET_CTR_ERR_INC(pdev_data->ErrRxCtrRunt);
       *size   = (CPU_INT16U  )0u;
       *p_data = (CPU_INT08U *)0u;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }

                                                                /* --------- OBTAIN PTR TO NEW DMA DATA AREA ---------- */
    pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0u,
                                 (NET_BUF_SIZE  *)0u,
                                 (NET_BUF_TYPE  *)0u,
                                 (NET_ERR       *)perr);


    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer.                           */
        NetDev_RxDescPtrCurInc(pif);                            /* Free cur desc.                                       */
        NET_CTR_ERR_INC(pdev_data->ErrRxCtrDataAreaAlloc);
       *size   = (CPU_INT16U  )0u;
       *p_data = (CPU_INT08U *)0u;
        return;
    }

   *size        =  len;                                         /* Return the size of the recv'd frame.                 */
   *p_data      = (CPU_INT08U *)pdesc->Addr;                    /* Return a ptr to the newly recv'd data.               */

    CPU_DCACHE_RANGE_INV(pdesc->Addr, len);                     /* Invalidate received buffer.                          */

    pdesc->Addr =  pbuf_new;                                    /* Update the desc to point to a new data area          */
    NetDev_RxDescPtrCurInc(pif);                                /* Free the current desc.                               */

    NET_CTR_STAT_INC(pdev_data->StatRxCtrRx);

    *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : (1) This function transmits the specified data :
*
*                   (a) Check if the transmitter is ready.
*                   (b) Configure the next transmit descriptor for pointer to data and data size.
*                   (c) Issue the transmit command.
*                   (d) Increment pointer to next transmit descriptor
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
* Note(s)     : (2) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (3) Care should be taken to avoid skipping transmit descriptors while selecting
*                   the next available descriptor.  Software MUST track the descriptor which
*                   is expected to generate the next transmit complete interrupt.  Skipping
*                   descriptors, unless carefully accounted for, may make it difficult to
*                   know which descriptor will complete transmission next.  Some device
*                   drivers may find it useful to adjust pdev_data->TxBufDescCompPtr after
*                   having selected the next available transmit descriptor.
*
*               (4)  The T01 descriptor bit is reserved for software use ONLY. This bit is set by software
*                    during transmission to indicate that the data ready bit for the descriptor is being set.
*                    When the DMA hardware completes transmission of the descriptor data, the ready bit is
*                    cleared and the T01 bit remains set. The transmit complete ISR handler then detects the
*                    cleared ready bit and set T01 bit and determines that the descriptor has completed
*                    transmission. The T01 bit is then cleared by the ISR handler.
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    NET_DEV_DATA  *pdev_data;
    NET_DEV_EDMA  *pdev_dma;
    DEV_DESC      *pdesc;
    CPU_SR_ALLOC();

                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_data  = (NET_DEV_DATA *)pif->Dev_Data;                 /* Obtain ptr to dev data area.                         */
    pdev_dma   = (NET_DEV_EDMA *)pdev_data->EMAC_DMABaseAddr;   /* Overlay DMA reg struct on top of DMA base addr.      */
    pdesc      = pdev_data->TxDescPtrCur;

    if ((pdesc->Status & GMAC_DMA_TX_DESC_OWN) != 0u) {         /* If cur Tx desc owned by software, then return err.   */
       *perr = NET_DEV_ERR_TX_BUSY;
        return;
    }

    CPU_DCACHE_RANGE_FLUSH(p_data, size);                       /* Flush/Clean buffer to send.                          */

    CPU_CRITICAL_ENTER();
    pdev_data->TxDescPtrCur = pdesc->Next;                      /* Increment for the next transmit                      */
    pdev_data->TxDescNbrAvail--;

    pdesc->Addr    =  p_data;                                   /* Cfg desc with Tx data area addr.                     */
    pdesc->Len     = (size & GMAC_DMA_TX_DESC_TBS1);            /* Cfg frame len: bits[12:0].                           */

    if ((pdev_data->MAC_Type != NET_DEV_HPS_EMAC    ) &&
        (pdev_data->MAC_Type != NET_DEV_STM32Fx_EMAC)) {
        pdesc->Status =  GMAC_DMA_TX_DESC_OWN |                 /* Give desc ownership to DMA.                          */
                         GMAC_DMA_TX_DESC_TCH |                 /* Second Addr Chained field points to next desc.       */
                         GMAC_DMA_TX_DESC_FS  |                 /* Desc contains first segment.                         */
                         GMAC_DMA_TX_DESC_LS  |                 /* Desc contains last  segment.                         */
                         GMAC_DMA_TX_DESC_IC;                   /* Interrupt on completion.                             */
    } else {
        pdesc->Status =  GMAC_DMA_TX_DESC_OWN |                 /* Give desc ownership to DMA.                          */
                         GMAC_DMA_TX_DESC_CO0 |                 /* Enable checksum insertion.                           */
                         GMAC_DMA_TX_DESC_CO1 |
                         GMAC_DMA_TX_DESC_TCH |                 /* Second Addr Chained field points to next desc.       */
                         GMAC_DMA_TX_DESC_FS  |                 /* Desc contains first segment.                         */
                         GMAC_DMA_TX_DESC_LS  |                 /* Desc contains last  segment.                         */
                         GMAC_DMA_TX_DESC_IC;                   /* Interrupt on completion.                             */
    }
    CPU_CRITICAL_EXIT();

    pdev_dma->DMA_STAT              =  GMAC_DMASR_TU;
    pdev_dma->DMA_TRANS_POLL_DEMAND =  1u;                      /* Inform hardware that a Tx desc has been made avail.  */


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_RxDescFreeAll()
*
* Description : (1) This function returns the descriptor memory block and descriptor data area
*                   memory blocks back to their respective memory pools :
*
*                   (a) Free Rx descriptor data areas
*                   (b) Free Rx descriptor memory block
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Stop().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;
    CPU_INT08U         *pdesc_data;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */

                                                                /* ------------- FREE RX DESC DATA AREAS -------------- */
    pdesc  = pdev_data->RxDescPtrStart;
    for (i = 0u; i < pdev_cfg->RxDescNbr; i++) {                /* Free Rx desc ring.                                   */
        pdesc_data = (CPU_INT08U *)(pdesc->Addr);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.               */
        pdesc++;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_RxDescInit()
*
* Description : This function initializes the Rx descriptor list for the specified interface.
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) Memory allocation for the descriptors and receive buffers MUST be performed BEFORE
*                   calling this function. This ensures that multiple calls to this function do NOT
*                   allocate additional memory to the interface and that the Rx descriptors may be
*                   safely re-initialized by calling this function.
*********************************************************************************************************
*/

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EDMA       *pdev_dma;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
                                                                /* Overlay DMA reg struct on top of DMA base addr.      */
    pdev_dma  = (NET_DEV_EDMA      *)pdev_data->EMAC_DMABaseAddr;


                                                                /* ------------- INIT DESCRIPTOR PTRS  ---------------- */
    pdesc                     = (DEV_DESC *)pdev_data->RxDescPtrStart;
    pdev_data->RxDescPtrStart = (DEV_DESC *)pdesc;
    pdev_data->RxDescPtrCur   = (DEV_DESC *)pdesc;

    for (i = 0u; i < pdev_cfg->RxDescNbr; i++) {                /* Init Rx descriptor ring.                             */
        pdesc->Len    = pdev_cfg->RxBufLargeSize |
                        GMAC_DMA_RX_DESC_RCH;

        pdesc->Status = GMAC_DMA_RX_DESC_OWN;                   /* DMA owns the desc.                                   */

        pdesc->Addr   = NetBuf_GetDataPtr((NET_IF        *)pif,
                                          (NET_TRANSACTION)NET_TRANSACTION_RX,
                                          (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                          (NET_BUF_SIZE   )NET_IF_IX_RX,
                                          (NET_BUF_SIZE  *)0u,
                                          (NET_BUF_SIZE  *)0u,
                                          (NET_BUF_TYPE  *)0u,
                                          (NET_ERR       *)perr);
        if (*perr != NET_BUF_ERR_NONE) {
             return;
        }

        CPU_DCACHE_RANGE_FLUSH(pdesc->Addr, pdev_cfg->RxBufLargeSize);

        if ((pdev_data->MAC_Type != NET_DEV_HPS_EMAC    ) &&
            (pdev_data->MAC_Type != NET_DEV_STM32Fx_EMAC)) {
            pdesc->Next = (DEV_DESC *)(pdesc + 1u);             /* Point to next desc in list.                          */
            pdesc++;
        } else {
            pdesc->Next = (DEV_DESC *)(pdesc + 2u);             /* Point to next desc in list.                          */
            pdesc = pdesc + 2u;
        }
    }

    if ((pdev_data->MAC_Type != NET_DEV_HPS_EMAC    ) &&
        (pdev_data->MAC_Type != NET_DEV_STM32Fx_EMAC)) {
        pdesc--;
    } else {
        pdesc = pdesc - 2u;
    }

    pdesc->Next                = (DEV_DESC *)pdev_data->RxDescPtrStart; /* Last desc points to first desc.              */
    pdev_dma->DMA_REC_DES_ADDR = (CPU_INT32U)pdev_data->RxDescPtrStart; /* Cfg DMA with addr of first desc.             */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_RxDescPtrCurInc()
*
* Description : (1) Increment current descriptor pointer to next receive descriptor :
*
*                   (a) Return ownership of current descriptor back to DMA.
*                   (b) Point to the next descriptor.
*
* Argument(s) : pif     Pointer to interface requiring service.
*               ---
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Rx().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_RxDescPtrCurInc (NET_IF  *pif)
{
    NET_DEV_EDMA  *pdev_dma;
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    NET_ERR        err;
    CPU_INT32U     int_status;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_dma  = (NET_DEV_EDMA *)pdev_data->EMAC_DMABaseAddr;    /* Overlay DMA reg struct on top of DMA base addr.      */



    pdesc                   = pdev_data->RxDescPtrCur;
    pdev_data->RxDescPtrCur = pdesc->Next;
    pdesc->Status           = GMAC_DMA_RX_DESC_OWN;             /* Return desc ownership back to hardware.              */

    CPU_MB();                                                   /* Ensure descriptor update is effective before poll.   */

    pdev_dma->DMA_REC_POLL_DEMAND = 1u;                         /* Inform hardware of available Rx desc.                */

    pdesc = pdesc->Next;                                        /* Check if there are add'l frames to Rx.               */
    if ((pdesc->Status & GMAC_DMA_RX_DESC_OWN) == 0u) {         /* Check if the desc is owned by SW.                    */
        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task for each new rdy Desc.        */
        switch (err) {
            case NET_IF_ERR_NONE:
                 NET_CTR_STAT_INC(pdev_data->StatRxCtrTaskSignalSelf);
                 break;


            case NET_IF_ERR_RX_Q_FULL:
            case NET_IF_ERR_RX_Q_SIGNAL_FAULT:
            default:
                 NET_CTR_ERR_INC(pdev_data->ErrRxCtrTaskSignalFail);
                 break;
        }
    } else {
        int_status            =  pdev_dma->DMA_STAT;            /* Clr active int. src's.                               */
        pdev_dma->DMA_STAT    =  int_status;

        pdev_dma->DMA_INT_EN |= (GMAC_DMAIER_RIE  |             /* Re-Enable Rx int's.                                  */
                                 GMAC_DMAIER_OVE  |             /* Rx (FIFO) overrun.                                   */
                                 GMAC_DMAIER_RUE  |             /* Rx buffer unavailable.                               */
                                 GMAC_DMAIER_RSE);              /* Rx process stopped.                                  */

        NET_CTR_STAT_INC(pdev_data->StatRxCtrTaskSignalNone);
    }
}


/*
*********************************************************************************************************
*                                        NetDev_TxDescInit()
*
* Description : (1) This function initializes the Tx descriptor list for the specified interface :
*
*                   (a) Obtain reference to the Tx descriptor(s) memory block
*                   (b) Initialize Tx descriptor pointers
*                   (c) Obtain Tx descriptor data areas
*                   (d) Initialize hardware registers
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE        No error
*                           NET_IF_MGR_ERR_nnnn     Various Network Interface management error codes
*                           NET_BUF_ERR_nnn         Various Network buffer error codes
* Return(s)   : none.
*
* Caller(s)   : NetDev_Start().
*
* Note(s)     : (2) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*
*               (3) All Tx descriptors are allocated as ONE memory block.  This removes the
*                   necessity to ensure that descriptor blocks are returned to the descriptor
*                   pool in the opposite order in which they were allocated; doing so would
*                   ensure that each memory block address was contiguous to the one before
*                   and after it.  If the descriptors are NOT contiguous, then software
*                   MUST NOT assign a pointer to the pool start address and use pointer
*                   arithmetic to navigate the descriptor list.  Since pointer arithmetic
*                   is a convenient way to navigate the descriptor list, ONE block is allocated
*                   and the driver uses pointer arithmetic to slice the block into descriptor
*                   sized units.
*********************************************************************************************************
*/

static  void  NetDev_TxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EDMA       *pdev_dma;
    DEV_DESC           *pdesc;
    CPU_INT16U          i;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
                                                                /* Overlay DMA reg struct on top of DMA base addr.      */
    pdev_dma  = (NET_DEV_EDMA      *)pdev_data->EMAC_DMABaseAddr;


                                                                /* -------------- INIT DESCRIPTOR PTRS  --------------- */
    pdesc                    = (DEV_DESC *)pdev_data->TxDescPtrStart;
    pdev_data->TxDescPtrComp = (DEV_DESC *)pdev_data->TxDescPtrStart;
    pdev_data->TxDescPtrCur  = (DEV_DESC *)pdev_data->TxDescPtrStart;

    for (i = 0u; i < pdev_cfg->TxDescNbr; i++) {                /* Init Tx desc ring.                                   */
        pdesc->Addr   =  0u;
        pdesc->Len    =  0u;

        pdesc->Status =  GMAC_DMA_TX_DESC_TCH |                 /* Second Addr Chained field points to next desc.       */
                         GMAC_DMA_TX_DESC_FS  |                 /* Desc contains first segment.                         */
                         GMAC_DMA_TX_DESC_LS  |                 /* Desc contains last  segment.                         */
                         GMAC_DMA_TX_DESC_IC;                   /* Interrupt on completion.                             */

        if ((pdev_data->MAC_Type != NET_DEV_HPS_EMAC    ) &&
            (pdev_data->MAC_Type != NET_DEV_STM32Fx_EMAC)) {
            pdesc->Next = (DEV_DESC *)(pdesc + 1u);             /* Point to next desc in list.                          */
            pdesc++;
        } else {
            pdesc->Next = (DEV_DESC *)(pdesc + 2u);             /* Point to next desc in list.                          */
            pdesc = pdesc + 2u;
        }
    }

    if ((pdev_data->MAC_Type != NET_DEV_HPS_EMAC    ) &&
        (pdev_data->MAC_Type != NET_DEV_STM32Fx_EMAC)) {
        pdesc--;
    } else {
        pdesc = pdesc - 2u;
    }
                                                                /* Last desc wraps to first desc.                       */
    pdesc->Next                  = (DEV_DESC *)pdev_data->TxDescPtrStart;
                                                                /* Set Tx desc list addr reg.                           */
    pdev_dma->DMA_TRANS_DES_ADDR = (CPU_INT32U)pdev_data->TxDescPtrStart;

    *perr = NET_DEV_ERR_NONE;
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
*                       (a) Perfect        filtering of ONE multicast address.
*                       (b) Imperfect hash filtering of 64  multicast addresses.
*                       (c) Promiscous non filtering.  Disable filtering of all received frames.
*
*               (2) This function implements the filtering mechanism described in 1b.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_BOOLEAN         reg_sel;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */


    crc = NetUtil_32BitCRC_CalcCpl((CPU_INT08U  *)paddr_hw,     /* Obtain CRC without compliment.                       */
                                   (CPU_INT32U   )addr_hw_len,
                                   (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    crc = NetUtil_32BitReflect(crc);                            /* Reflect CRC to produce equivalent hardware result.   */

    hash    = (crc >> 26u) & 0x3Fu;                             /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26u) & 0x1Fu;                             /* Obtain least 5 significant bits for for reg bit sel. */
    reg_sel = (crc >> 31u) & 0x01u;                             /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    (*paddr_hash_ctrs)++;                                       /* Increment hash bit reference ctr.                    */

    if (reg_sel == 0u) {                                        /* Set multicast hash reg bit.                          */
        pdev->MAC_HASHTABLE_LOW  |= (1u << bit_sel);
    } else {
        pdev->MAC_HASHTABLE_HIGH |= (1u << bit_sel);
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
#endif

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
*                       (a) Perfect        filtering of ONE multicast address.
*                       (b) Imperfect hash filtering of 64  multicast addresses.
*                       (c) Promiscous non filtering.  Disable filtering of all received frames.
*
*               (2) This function implements the filtering mechanism described in 1b.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    NET_DEV_EMAC       *pdev;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_BOOLEAN         reg_sel;


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */
    pdev      = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.      */

    crc = NetUtil_32BitCRC_CalcCpl((CPU_INT08U  *)paddr_hw,     /* Obtain CRC without compliment.                       */
                                   (CPU_INT32U   )addr_hw_len,
                                   (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    crc = NetUtil_32BitReflect(crc);                            /* Reflect CRC to produce equivalent hardware result.   */

    hash    = (crc >> 26u) & 0x3Fu;                             /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26u) & 0x1Fu;                             /* Obtain least 5 significant bits for for reg bit sel. */
    reg_sel = (crc >> 31u) & 0x01u;                             /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1u) {                                /* If hash bit reference ctr not zero.                  */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0u;                                       /* Zero hash bit references remaining.                  */

    if (reg_sel == 0u) {                                        /* Clr multicast hash reg bit.                          */
        pdev->MAC_HASHTABLE_LOW  &= ~(1u << bit_sel);
    } else {
        pdev->MAC_HASHTABLE_HIGH &= ~(1u << bit_sel);
    }

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
#endif

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_ISR_Handler()
*
* Description : This function serves as the device Interrupt Service Routine Handler. This ISR
*               handler MUST service and clear all necessary and enabled interrupt events for
*               the device.
*
* Argument(s) : pif     Pointer to interface requiring service.
*               ---
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
*                           NET_DEV_ISR_TYPE_RX
*                           NET_DEV_ISR_TYPE_TX_COMPLETE
*                           NET_DEV_ISR_TYPE_UNKNOWN
*
* Return(s)   : none.
*
* Caller(s)   : Specific first- or second-level BSP ISR handler.
*
* Note(s)     : (1) This function is called by name from the context of an ISR.
*
*               (2) In the case of an interrupt occurring prior to Network Protocol Stack initialization,
*                   the device driver should ensure that the interrupt source is cleared in order
*                   to prevent the potential for an infinite interrupt loop during system initialization.
*
*               (3) Many DMA devices generate only one interrupt event for several ready receive
*                   descriptors.  In order to accommodate this, it is recommended that all DMA based
*                   drivers count the number of ready receive descriptors during the receive event
*                   and signal the receive task for ONLY newly received descriptors which have not
*                   yet been signaled for during the last receive interrupt event.
*
*               (4) Many DMA devices generate only one interrupt event for several transmit
*                   complete descriptors.  In this case, the driver MUST determine which descriptors
*                   have completed transmission and post each descriptor data area address to
*                   the transmit deallocation task.  The code provided below assumes one
*                   interrupt per transmit event which may not necessarily be the case for all
*                   devices.
*
*               (5) Receive complete interrupts are disabled until all received frames have been
*                   processed.  This action reduces unnecessary interrupt overhead.  Rx interrupts
*                   are restored from NetDev_RxDescPtrCurInc() when the 'next' descriptor is
*                   found to be owned by the DMA.
*
*               (6) The reference manual, Ethernet DMA controller Section, states the following :
*                   (a) For any data transfer initiated by a DMA channel, if the slave replies with
*                       an error response, that DMA stops all operations and updates the error bits
*                       and the fatal bus error bit in the Status register (DMA register 5).
*                       That DMA controller can resume operation only after soft- or hard-resetting
*                       the peripheral and re-initializing the DMA.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    DEV_DESC           *pdesc;
    DEV_DESC           *pdesc_cur;
    NET_DEV_EDMA       *pdev_dma;
    NET_ERR             err;
    CPU_INT32U          int_status;
    CPU_BOOLEAN         done = DEF_NO;

   (void)&type;                                                 /* Prevent compiler warning for arg type unused.        */


                                                                /* -- OBTAIN REFERENCE TO DEVICE CFG/DATA/REGISTERS --- */
    pdev_cfg   = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
    pdev_dma  = (NET_DEV_EDMA *)pdev_data->EMAC_DMABaseAddr;    /* Overlay DMA reg struct on top of DMA base addr.      */



    pdev_dma->DMA_INT_EN &= ~(GMAC_DMAIER_NIE |                 /* Disable normal & abnormal int. summary src's to ...  */
                              GMAC_DMAIER_AIE);                 /* ... prevent int. nesting.                            */

    int_status            =   pdev_dma->DMA_STAT;               /* Clr active int. src's.                               */
    pdev_dma->DMA_STAT    =   int_status;

    if ((int_status & GMAC_DMASR_FBI) > 0u) {                   /* Fatal bus err (see Note #6).                         */
        NET_CTR_ERR_INC(pdev_data->ErrRxTxCtrFatalBusErr);
    }

    if ((int_status & GMAC_DMASR_TI) > 0u) {                     /* Tx complete?                                         */
        pdesc      = pdev_data->TxDescPtrComp;
        pdesc_cur  = pdev_data->TxDescPtrCur;
        if (pdev_data->TxDescNbrAvail < pdev_cfg->TxDescNbr) {
            while (done != DEF_YES) {
                if ((pdesc->Status & GMAC_DMA_TX_DESC_OWN) == 0u) {
                    NET_CTR_STAT_INC(pdev_data->StatTxCtrTxISR);
                    pdev_data->TxDescNbrAvail++;
                    NetIF_TxDeallocTaskPost(pdesc->Addr, &err); /* Signal Net IF that Tx resources are available.       */
                    NetIF_DevTxRdySignal(pif);
                } else {
                    break;
                }
                pdesc = pdesc->Next;
                if (pdesc == pdesc_cur) {
                    done = DEF_YES;
                }
            }
            pdev_data->TxDescPtrComp = pdesc;
        }
    }


    if ((int_status & (GMAC_DMASR_RI   |                        /* Rx int.                                              */
                       GMAC_DMASR_OVF  |                        /* Rx (FIFO) overflow.                                  */
                       GMAC_DMASR_RU   |                        /* Rx buffer unavailable.                               */
                       GMAC_DMASR_RPS)) > 0u) {                 /* Rx process stopped.                                  */

        if ((int_status & GMAC_DMASR_RI) > 0u) {                /* Rx interrupt.                                        */
            NET_CTR_STAT_INC(pdev_data->StatRxCtrRxISR);
        }

        if ((int_status & GMAC_DMASR_OVF) > 0u) {               /* Rx overflow.                                         */
            NET_CTR_ERR_INC(pdev_data->ErrRxCtrROS);
        }

        if ((int_status & GMAC_DMASR_RU) > 0u) {                /* Rx buffer unavailable.                               */
            NET_CTR_ERR_INC(pdev_data->ErrRxCtrRBUS);
        }

        if ((int_status & GMAC_DMASR_RPS) > 0u) {               /* Rx process stopped.                                  */
            NET_CTR_ERR_INC(pdev_data->ErrRxCtrRPSS);
        }

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
        pdev_data->IntStatusLast = int_status;                  /* Record last ISR status for debug.                    */
#endif

        pdev_dma->DMA_INT_EN &= ~(GMAC_DMAIER_RIE |             /* Dis. future Rx int's (see Note #5).                  */
                                  GMAC_DMAIER_OVE |             /* Rx (FIFO) overrun.                                   */
                                  GMAC_DMAIER_RUE |             /* Rx buffer unavailable.                               */
                                  GMAC_DMAIER_RSE);             /* Rx process stopped.                                  */

        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task for each new rdy Desc.        */
        switch (err) {
            case NET_IF_ERR_NONE:
                 NET_CTR_STAT_INC(pdev_data->StatRxCtrTaskSignalISR);
                 break;


            case NET_IF_ERR_RX_Q_FULL:
            case NET_IF_ERR_RX_Q_SIGNAL_FAULT:
            default:
                 pdev_dma->DMA_INT_EN |= GMAC_DMAIER_RIE;       /* Re-enable int's and signal on next Rx.               */
                 NET_CTR_ERR_INC(pdev_data->ErrRxCtrTaskSignalFail);
                 break;
        }
    }


    pdev_dma->DMA_INT_EN |= (GMAC_DMAIER_NIE |                  /* Re-Enable normal & abnormal int. src's.              */
                             GMAC_DMAIER_AIE);
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
*               data    Pointer to optional data for either sending or receiving additional function
*                       arguments or return data.
*
*               perr    Pointer to return error code.
*                           NET_IF_ERR_INVALID_IO_CTRL_OPT      Invalid option number specified.
*                           NET_ERR_FAULT_NULL_FNCT             Null interface function pointer encountered.
*
*                           NET_DEV_ERR_NONE                    IO Ctrl operation completed successfully.
*                           NET_DEV_ERR_NULL_PTR                Null argument pointer passed.
*
*                           NET_PHY_ERR_NULL_PTR                Pointer argument(s) passed NULL pointer(s).
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
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *pif,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER   *pdev_cfg;
    NET_DEV_LINK_ETHER  *plink_state;
    NET_PHY_API_ETHER   *pphy_api;
    NET_DEV_EMAC        *pdev;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */
    pdev     = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.      */

    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state      = (NET_DEV_LINK_ETHER *)p_data;
             if (plink_state == (NET_DEV_LINK_ETHER *)0) {
                *perr = NET_DEV_ERR_NULL_PTR;
                 return;
             }

             pphy_api = (NET_PHY_API_ETHER *)pif->Ext_API;

             if (pphy_api == (void *)0) {
                *perr = NET_ERR_FAULT_NULL_FNCT;
                 return;
             }
             pphy_api->LinkStateGet(pif, plink_state, perr);
             if (*perr != NET_PHY_ERR_NONE) {
                return;
             }

            *perr = NET_DEV_ERR_NONE;
             break;


        case NET_IF_IO_CTRL_LINK_STATE_UPDATE:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;

             switch (plink_state->Duplex) {                     /* Update duplex setting on device.                     */
                case NET_PHY_DUPLEX_FULL:
                                                                /* Full Duplex Mode, this will set bit 11.              */
                     pdev->MAC_CONFIG |= (CPU_INT32U)GMAC_MCR_DM;
                     break;


                case NET_PHY_DUPLEX_HALF:
                                                                /* Half Duplex Mode, this will resset bit 11.           */
                     pdev->MAC_CONFIG &= ~((CPU_INT32U)GMAC_MCR_DM);
                     break;


                default:
                    *perr =  NET_PHY_ERR_INVALID_CFG;
                     return;
             }

             switch (plink_state->Spd) {                        /* Update speed setting on device.                      */
                case NET_PHY_SPD_10:
                     pdev->MAC_CONFIG &= ~((CPU_INT32U)GMAC_MCR_FES);
                     break;


                case NET_PHY_SPD_100:
                     pdev->MAC_CONFIG |= (CPU_INT32U)GMAC_MCR_FES;
                     break;

                case NET_PHY_SPD_1000:
                    pdev->MAC_CONFIG |= (CPU_INT32U)GMAC_MCR_FES;
                    pdev->MAC_CONFIG &= (CPU_INT32U)~GMAC_MCR_PS;
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
*                                            NetDev_MII_Rd()
*
* Description : Read data over the (R)MII bus from the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NULL_PTR            Pointer argument(s) passed NULL pointer(s).
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_RD      Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various Phy functions.
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_MII_Rd (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U  *p_data,
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_EMAC       *pdev;
    CPU_INT32U          tmpreg;
    CPU_INT32U          timeout;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE REGISTERS ------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                   */
    pdev     = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.     */


    timeout  =  0u;

    tmpreg   =  pdev->MAC_MII_ADDR;                             /* Get the ETHERNET MACMIIAR value                     */
    tmpreg  &= ~GMAC_GAR_CR_MASK;                               /* Keep only the CSR Clk Range CR[2:0] bits val.       */

                                                                /* Prepare the MII address register val.               */
    tmpreg  |= (((CPU_INT32U)phy_addr << 11u) & GMAC_GAR_PA);   /* Set the PHY device addr.                            */
    tmpreg  |= (((CPU_INT32U)reg_addr <<  6u) & GMAC_GAR_GR);   /* Set the PHY reg addr.                               */
    tmpreg  &= ~GMAC_GAR_GW;                                    /* Set the read mode.                                  */
    tmpreg  |=  GMAC_GAR_GB;                                    /* Set the MII Busy bit.                               */

    pdev->MAC_MII_ADDR = tmpreg;                                /* Write the result val into the MII addr reg.         */

    do {                                                        /* Check for the Busy flag.                            */
        timeout++;
        tmpreg = pdev->MAC_MII_ADDR;
    } while ((tmpreg & GMAC_GAR_GB) && (timeout < (CPU_INT32U)PHY_RD_TO));

    if (timeout == PHY_RD_TO) {                                 /* Return ERROR in case of timeout.                    */
       *perr = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    *p_data = (CPU_INT16U)(pdev->MAC_MII_DATA);                 /* Return data reg val.                                */

    *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified Phy register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the Phy requiring service.
*
*               reg_addr    Phy register number to write to.
*
*               data        Data to write to the specified Phy register.
*
*               perr    Pointer to return error code.
*                           NET_PHY_ERR_NONE                MII write completed successfully.
*                           NET_PHY_ERR_TIMEOUT_REG_WR      Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various Phy functions.
*
* Note(s)     : (1) All functions that require device register access MUST obtain reference
*                   to the device hardware register space PRIOR to attempting to access
*                   any registers.  Register definitions SHOULD NOT be absolute and SHOULD
*                   use the provided base address within the device configuration structure,
*                   as well as the device register definition structure in order to properly
*                   resolve register addresses during run-time.
*********************************************************************************************************
*/

static  void  NetDev_MII_Wr (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   data,
                             NET_ERR     *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_EMAC       *pdev;
    CPU_INT32U          timeout;
    CPU_INT32U          tmpreg;

                                                                /* ------ OBTAIN REFERENCE TO DEVICE REGISTERS ------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                  */
    pdev     = (NET_DEV_EMAC      *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.    */


    tmpreg   = pdev->MAC_MII_ADDR;                              /* Get the ETHERNET MACMIIAR val.                     */

    tmpreg  &= ~GMAC_GAR_CR_MASK;                               /* Keep only the CSR Clk Range CR[2:0] bits val.      */

                                                                /* Prepare the MII reg addr val.                      */
    tmpreg  |= (((CPU_INT32U)phy_addr << 11u) & GMAC_GAR_PA);   /* Set the PHY device addr.                           */
    tmpreg  |= (((CPU_INT32U)reg_addr <<  6u) & GMAC_GAR_GR);   /* Set the PHY reg addr.                              */
    tmpreg  |= GMAC_GAR_GW;                                     /* Set the write mode.                                */
    tmpreg  |= GMAC_GAR_GB;                                     /* Set the MII Busy bit.                              */

    pdev->MAC_MII_DATA = data;                                  /* Give the val to the MII data reg.                  */
    pdev->MAC_MII_ADDR = tmpreg;                                /* Write the result value into the MII addr reg.      */

    timeout = 0u;                                               /* Init timeout.                                      */

    do {                                                        /* Check for the Busy flag.                           */
        timeout++;
        tmpreg = pdev->MAC_MII_ADDR;
    } while ((tmpreg & GMAC_GAR_GB) && (timeout < (CPU_INT32U)PHY_WR_TO));

    if (timeout == PHY_WR_TO) {                                 /* Return ERROR in case of timeout.                   */
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

    *perr = NET_PHY_ERR_NONE;
}


#endif  /* NET_IF_ETHER_MODULE_EN */
