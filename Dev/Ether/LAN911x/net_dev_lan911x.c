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
*                                               LAN911x
*
* Filename : net_dev_lan911x.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) Supports LAN911{5,6,7,8} Ethernet controller as described in
*
*                    Standard Microsystems Corporation's (SMSC; http://www.smsc.com)
*                    (a) LAN9115 data sheet              (SMSC LAN9115; Revision 1.1 05/17/2005)
*
*            (3) It is assumed that the LAN911x is attached to a 32 bit little-endian host processor
*                and that the data lines are NOT byte swapped.
*
*                                   -----------                 -----------
*                                   | LAN911x |                 |   CPU   |
*                                   |         |                 |         |
*                                   |         |                 |         |
*                                   |    D07- |        8        | D07-    |
*                                   |    D00  |--------/--------| D00     |
*                                   |         |                 |         |
*                                   |    D15- |        8        | D15-    |
*                                   |    D08  |--------/--------| D08     |
*                                   |         |                 |         |
*                                   -----------                 -----------
*
*            (4) #### Power Management Events are not yet implemented
*                #### PHY interrupts are not enabled (no knowledge of link state changes)
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_LAN911x_MODULE
#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>
#include  "net_dev_lan911x.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  LAN911x_CFG_TX_FIFO_SIZE                           3   /* Tx FIFO Size in kilobytes                            */
#define  LAN911x_CFG_RX_QUEUE_SIZE                         40   /* Size of receive status words queue.                  */

#define  LAN911x_INIT_RESET_RETRIES                         5
#define  LAN911x_INIT_AUTO_NEG_RETRIES                      5
#define  LAN911x_TX_ALLOC_INIT_RETRIES                      5
#define  LAN911x_CSR_RETRIES                                5

#define  IS_REV_A(x)         (((x) & 0xFFFF) == 0)

/*
*********************************************************************************************************
*                                         LAN911x REGISTERS
*
* Note(s): (1) A pointer to the device configuration MUST be available in every function
*              that utilizes the following register definitions.
*********************************************************************************************************
*/

#define  LAN91_RX_FIFO          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0000))
#define  LAN91_TX_FIFO          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0020))
#define  LAN91_RX_STATUS_FIFO   (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0040))
#define  LAN91_RX_FIFO_PEEK     (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0044))
#define  LAN91_TX_STATUS_FIFO   (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0048))
#define  LAN91_TX_FIFO_PEEK     (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x004C))
#define  LAN91_ID_REV           (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0050))
#define  LAN91_INT_CFG          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0054))
#define  LAN91_INT_STS          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0058))
#define  LAN91_INT_EN           (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x005C))
#define  LAN91_BYTE_TEST        (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0064))    // RO default 0x87654321
#define  LAN91_FIFO_INT         (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0068))
#define  LAN91_RX_CFG           (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x006C))
#define  LAN91_TX_CFG           (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0070))
#define  LAN91_HW_CFG           (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0074))
#define  LAN91_RX_DP_CTL        (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0078))
#define  LAN91_RX_FIFO_INF      (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x007C))
#define  LAN91_TX_FIFO_INF      (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0080))
#define  LAN91_PMT_CTRL         (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0084))
#define  LAN91_GPIO_CFG         (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0088))
#define  LAN91_GPT_CFG          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x008C))
#define  LAN91_GPT_CNT          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0090))
#define  LAN91_FPGA_REV         (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0094))
#define  LAN91_ENDIAN           (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x0098))    // R/W Not Affected by SW Reset
#define  LAN91_FREE_RUN         (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x009C))    // RO
#define  LAN91_RX_DROP          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00A0))    // R/WC
#define  LAN91_MAC_CSR_CMD      (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00A4))
#define  LAN91_MAC_CSR_DATA     (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00A8))    // R/W
#define  LAN91_AFC_CFG          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00AC))
#define  LAN91_E2P_CMD          (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00B0))
#define  LAN91_E2P_DATA         (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00B4))
#define  LAN91_TEST_REG_A       (*(volatile CPU_INT32U *)(void *)(pdev_cfg->BaseAddr + 0x00C0))


/*
****************************************************************************
*   TX/RX FIFO Port Register (Direct Address)
*   Offset (from Base Address)
*   and bit definitions
****************************************************************************
*/

#define TX_CMD_A_INT_ON_COMP                    (0x80000000UL)
#define TX_CMD_A_INT_BUF_END_ALGN               (0x03000000UL)
#define TX_CMD_A_INT_4_BYTE_ALGN                (0x00000000UL)
#define TX_CMD_A_INT_16_BYTE_ALGN               (0x01000000UL)
#define TX_CMD_A_INT_32_BYTE_ALGN               (0x02000000UL)
#define TX_CMD_A_INT_DATA_OFFSET                (0x001F0000UL)
#define TX_CMD_A_INT_FIRST_SEG                  (0x00002000UL)
#define TX_CMD_A_INT_LAST_SEG                   (0x00001000UL)
#define TX_CMD_A_BUF_SIZE                       (0x000007FFUL)
#define TX_CMD_B_PKT_TAG                        (0xFFFF0000UL)
#define TX_CMD_B_ADD_CRC_DISABLE                (0x00002000UL)
#define TX_CMD_B_DISABLE_PADDING                (0x00001000UL)
#define TX_CMD_B_PKT_BYTE_LENGTH                (0x000007FFUL)

#define RX_STS_RESERVED                         (0x80004301UL)
#define RX_STS_ADDR_FILTER_FAIL                 (0x40000000UL)
#define RX_STS_ES                               (0x00008000UL)
#define RX_STS_BROADCAST                        (0x00002000UL)
#define RX_STS_LENGTH_ERR                       (0x00001000UL)
#define RX_STS_RUNT                             (0x00000800UL)
#define RX_STS_MULTICAST                        (0x00000400UL)
#define RX_STS_FRAME_TOO_LONG                   (0x00000080UL)
#define RX_STS_COLLISION                        (0x00000040UL)
#define RX_STS_FRAME_TYPE                       (0x00000020UL)
#define RX_STS_RX_WD_TIMEOUT                    (0x00000010UL)
#define RX_STS_MII_ERR                          (0x00000008UL)
#define RX_STS_DRIBBLING                        (0x00000004UL)
#define RX_STS_CRC_ERR                          (0x00000002UL)

#define TX_STS_ES                               (0x00008000UL)

/*
 ****************************************************************************
*   Slave Interface Module Control and Status Register (Direct Address)
*   Offset (from Base Address)
*   and bit definitions
****************************************************************************
*/

#define ID_REV_CHIP_ID                          (0xFFFF0000UL)    // RO    default 0x011X
#define ID_REV_REV_ID                           (0x0000FFFFUL)    // RO

#define INT_CFG_INT_DEAS                        (0xFF000000UL)    // R/W
#define INT_CFG_IRQ_INT                         (0x00001000UL)    // RO
#define INT_CFG_IRQ_EN                          (0x00000100UL)    // R/W
#define INT_CFG_IRQ_POL                         (0x00000010UL)    // R/W Not Affected by SW Reset
#define INT_CFG_IRQ_TYPE                        (0x00000001UL)    // R/W Not Affected by SW Reset
#define INT_CFG_IRQ_RESERVED                    (0x00FFCEEEUL)    //Reserved bits mask

#define INT_STS_SW_INT                          (0x80000000UL)    // R/WC
#define INT_STS_TXSTOP_INT                      (0x02000000UL)    // R/WC
#define INT_STS_RXSTOP_INT                      (0x01000000UL)    // R/WC
#define INT_STS_RXDFH_INT                       (0x00800000UL)    // R/WC
#define INT_STS_RXDF_INT                        (0x00400000UL)    // R/WC
#define INT_STS_TX_IOC                          (0x00200000UL)    // R/WC
#define INT_STS_RXD_INT                         (0x00100000UL)    // R/WC
#define INT_STS_GPT_INT                         (0x00080000UL)    // R/WC
#define INT_STS_PHY_INT                         (0x00040000UL)    // RO
#define INT_STS_PME_INT                         (0x00020000UL)    // R/WC
#define INT_STS_TXSO                            (0x00010000UL)    // R/WC
#define INT_STS_RWT                             (0x00008000UL)    // R/WC
#define INT_STS_RXE                             (0x00004000UL)    // R/WC
#define INT_STS_TXE                             (0x00002000UL)    // R/WC
#define INT_STS_ERX                             (0x00001000UL)    // R/WC
#define INT_STS_TDFU                            (0x00000800UL)    // R/WC
#define INT_STS_TDFO                            (0x00000400UL)    // R/WC
#define INT_STS_TDFA                            (0x00000200UL)    // R/WC
#define INT_STS_TSFF                            (0x00000100UL)    // R/WC
#define INT_STS_TSFL                            (0x00000080UL)    // R/WC
#define INT_STS_RDFO                            (0x00000040UL)    // R/WC
#define INT_STS_RDFL                            (0x00000020UL)    // R/WC
#define INT_STS_RSFF                            (0x00000010UL)    // R/WC
#define INT_STS_RSFL                            (0x00000008UL)    // R/WC
#define INT_STS_GPIO2_INT                       (0x00000004UL)    // R/WC
#define INT_STS_GPIO1_INT                       (0x00000002UL)    // R/WC
#define INT_STS_GPIO0_INT                       (0x00000001UL)    // R/WC

#define INT_EN_SW_INT_EN                        (0x80000000UL)    // R/W
#define INT_EN_TXSTOP_INT_EN                    (0x02000000UL)    // R/W
#define INT_EN_RXSTOP_INT_EN                    (0x01000000UL)    // R/W
#define INT_EN_RXDFH_INT_EN                     (0x00800000UL)    // R/W
#define INT_EN_RXDF_INT_EN                      (0x00400000UL)    // R/W
#define INT_EN_TIOC_INT_EN                      (0x00200000UL)    // R/W
#define INT_EN_RXD_INT_EN                       (0x00100000UL)    // R/W
#define INT_EN_GPT_INT_EN                       (0x00080000UL)    // R/W
#define INT_EN_PHY_INT_EN                       (0x00040000UL)    // R/W
#define INT_EN_PME_INT_EN                       (0x00020000UL)    // R/W
#define INT_EN_TXSO_EN                          (0x00010000UL)    // R/W
#define INT_EN_RWT_EN                           (0x00008000UL)    // R/W
#define INT_EN_RXE_EN                           (0x00004000UL)    // R/W
#define INT_EN_TXE_EN                           (0x00002000UL)    // R/W
#define INT_EN_ERX_EN                           (0x00001000UL)    // R/W
#define INT_EN_TDFU_EN                          (0x00000800UL)    // R/W
#define INT_EN_TDFO_EN                          (0x00000400UL)    // R/W
#define INT_EN_TDFA_EN                          (0x00000200UL)    // R/W
#define INT_EN_TSFF_EN                          (0x00000100UL)    // R/W
#define INT_EN_TSFL_EN                          (0x00000080UL)    // R/W
#define INT_EN_RDFO_EN                          (0x00000040UL)    // R/W
#define INT_EN_RDFL_EN                          (0x00000020UL)    // R/W
#define INT_EN_RSFF_EN                          (0x00000010UL)    // R/W
#define INT_EN_RSFL_EN                          (0x00000008UL)    // R/W
#define INT_EN_GPIO2_INT                        (0x00000004UL)    // R/W
#define INT_EN_GPIO1_INT                        (0x00000002UL)    // R/W
#define INT_EN_GPIO0_INT                        (0x00000001UL)    // R/W

#define FIFO_INT_TX_AVAIL_LEVEL                 (0xFF000000UL)    // R/W
#define FIFO_INT_TX_STS_LEVEL                   (0x00FF0000UL)    // R/W
#define FIFO_INT_RX_AVAIL_LEVEL                 (0x0000FF00UL)    // R/W
#define FIFO_INT_RX_STS_LEVEL                   (0x000000FFUL)    // R/W

#define RX_CFG_RX_END_ALGN                      (0xC0000000UL)    // R/W
#define RX_CFG_RX_END_ALGN4                     (0x00000000UL)    // R/W
#define RX_CFG_RX_END_ALGN16                    (0x40000000UL)    // R/W
#define RX_CFG_RX_END_ALGN32                    (0x80000000UL)    // R/W
#define RX_CFG_RX_DMA_CNT                       (0x0FFF0000UL)    // R/W
#define RX_CFG_RX_DUMP                          (0x00008000UL)    // R/W
#define RX_CFG_RXDOFF                           (0x00001F00UL)    // R/W
#define RX_CFG_RXBAD                            (0x00000001UL)    // R/W

#define TX_CFG_TX_DMA_LVL                       (0xE0000000UL)    // R/W
#define TX_CFG_TX_DMA_CNT                       (0x0FFF0000UL)    // R/W Self Clearing
#define TX_CFG_TXS_DUMP                         (0x00008000UL)    // Self Clearing
#define TX_CFG_TXD_DUMP                         (0x00004000UL)    // Self Clearing
#define TX_CFG_TXSAO                            (0x00000004UL)    // R/W
#define TX_CFG_TX_ON                            (0x00000002UL)    // R/W
#define TX_CFG_STOP_TX                          (0x00000001UL)    // Self Clearing

#define HW_CFG_TTM                              (0x00200000UL)    // R/W
#define HW_CFG_SF                               (0x00100000UL)    // R/W
#define HW_CFG_TX_FIF_SZ                        (0x000F0000UL)    // R/W
#define HW_CFG_TR                               (0x00003000UL)    // R/W
#define HW_CFG_PHY_CLK_SEL                      (0x00000060UL)    // R/W
#define HW_CFG_PHY_CLK_SEL_INT_PHY              (0x00000000UL)    // R/W
#define HW_CFG_PHY_CLK_SEL_EXT_PHY              (0x00000020UL)    // R/W
#define HW_CFG_PHY_CLK_SEL_CLK_DIS              (0x00000040UL)    // R/W
#define HW_CFG_SMI_SEL                          (0x00000010UL)    // R/W
#define HW_CFG_EXT_PHY_DET                      (0x00000008UL)    // RO
#define HW_CFG_EXT_PHY_EN                       (0x00000004UL)    // R/W
#define HW_CFG_SRST_TO                          (0x00000002UL)    // RO
#define HW_CFG_SRST                             (0x00000001UL)    // Self Clearing

#define RX_DP_CTL_FFWD_BUSY                     (0x80000000UL)    // R/?
#define RX_DP_CTL_RX_FFWD                       (0x00000FFFUL)    // R/W

#define RX_FIFO_INF_RXSUSED                     (0x00FF0000UL)    // RO
#define RX_FIFO_INF_RXDUSED                     (0x0000FFFFUL)    // RO

#define TX_FIFO_INF_TSFREE                      (0x00FF0000UL)    // RO for PAS V.1.3
#define TX_FIFO_INF_TSUSED                      (0x00FF0000UL)    // RO for PAS V.1.4
#define TX_FIFO_INF_TDFREE                      (0x0000FFFFUL)    // RO

#define PMT_CTRL_PM_MODE                        (0x00003000UL)    // Self Clearing
#define PMT_CTRL_PM_MODE_GP                     (0x00003000UL)    // Self Clearing
#define PMT_CTRL_PM_MODE_ED                     (0x00002000UL)    // Self Clearing
#define PMT_CTRL_PM_MODE_WOL                    (0x00001000UL)    // Self Clearing
#define PMT_CTRL_PHY_RST                        (0x00000400UL)    // Self Clearing
#define PMT_CTRL_WOL_EN                         (0x00000200UL)    // R/W
#define PMT_CTRL_ED_EN                          (0x00000100UL)    // R/W
#define PMT_CTRL_PME_TYPE                       (0x00000040UL)    // R/W Not Affected by SW Reset
#define PMT_CTRL_WUPS                           (0x00000030UL)    // R/WC
#define PMT_CTRL_WUPS_NOWAKE                    (0x00000000UL)    // R/WC
#define PMT_CTRL_WUPS_ED                        (0x00000010UL)    // R/WC
#define PMT_CTRL_WUPS_WOL                       (0x00000020UL)    // R/WC
#define PMT_CTRL_WUPS_MULTI                     (0x00000030UL)    // R/WC
#define PMT_CTRL_PME_IND                        (0x00000008UL)    // R/W
#define PMT_CTRL_PME_POL                        (0x00000004UL)    // R/W
#define PMT_CTRL_PME_EN                         (0x00000002UL)    // R/W Not Affected by SW Reset
#define PMT_CTRL_READY                          (0x00000001UL)    // RO

#define GPIO_CFG_LED3_EN                        (0x40000000UL)    // R/W
#define GPIO_CFG_LED2_EN                        (0x20000000UL)    // R/W
#define GPIO_CFG_LED1_EN                        (0x10000000UL)    // R/W
#define GPIO_CFG_GPIO2_INT_POL                  (0x04000000UL)    // R/W
#define GPIO_CFG_GPIO1_INT_POL                  (0x02000000UL)    // R/W
#define GPIO_CFG_GPIO0_INT_POL                  (0x01000000UL)    // R/W
#define GPIO_CFG_EEPR_EN                        (0x00700000UL)    // R/W
#define GPIO_CFG_GPIOBUF2                       (0x00040000UL)    // R/W
#define GPIO_CFG_GPIOBUF1                       (0x00020000UL)    // R/W
#define GPIO_CFG_GPIOBUF0                       (0x00010000UL)    // R/W
#define GPIO_CFG_GPIODIR2                       (0x00000400UL)    // R/W
#define GPIO_CFG_GPIODIR1                       (0x00000200UL)    // R/W
#define GPIO_CFG_GPIODIR0                       (0x00000100UL)    // R/W
#define GPIO_CFG_GPIOD4                         (0x00000020UL)    // R/W
#define GPIO_CFG_GPIOD3                         (0x00000010UL)    // R/W
#define GPIO_CFG_GPIOD2                         (0x00000004UL)    // R/W
#define GPIO_CFG_GPIOD1                         (0x00000002UL)    // R/W
#define GPIO_CFG_GPIOD0                         (0x00000001UL)    // R/W

#define GPT_CFG_TIMER_EN                        (0x20000000UL)    // R/W
#define GPT_CFG_GPT_LOAD                        (0x0000FFFFUL)    // R/W

#define GPT_CNT_GPT_CNT                         (0x0000FFFFUL)    // RO

#define FPGA_REV_FPGA_REV                       (0x0000FFFFUL)    // RO

#define MAC_CSR_CMD_CSR_BUSY                    (0x80000000UL)    // Self Clearing
#define MAC_CSR_CMD_R_NOT_W                     (0x40000000UL)    // R/W
#define MAC_CSR_CMD_CSR_ADDR                    (0x000000FFUL)    // R/W

#define AFC_CFG_AFC_HI                          (0x00FF0000UL)    // R/W
#define AFC_CFG_AFC_LO                          (0x0000FF00UL)    // R/W
#define AFC_CFG_BACK_DUR                        (0x000000F0UL)    // R/W
#define AFC_CFG_FCMULT                          (0x00000008UL)    // R/W
#define AFC_CFG_FCBRD                           (0x00000004UL)    // R/W
#define AFC_CFG_FCADD                           (0x00000002UL)    // R/W
#define AFC_CFG_FCANY                           (0x00000001UL)    // R/W

#define E2P_CMD_EPC_BUSY                        (0x80000000UL)    // Self Clearing
#define E2P_CMD_EPC_CMD                         (0x70000000UL)    // R/W
#define E2P_CMD_EPC_CMD_READ                    (0x00000000UL)    // R/W
#define E2P_CMD_EPC_CMD_EWDS                    (0x10000000UL)    // R/W
#define E2P_CMD_EPC_CMD_EWEN                    (0x20000000UL)    // R/W
#define E2P_CMD_EPC_CMD_WRITE                   (0x30000000UL)    // R/W
#define E2P_CMD_EPC_CMD_WRAL                    (0x40000000UL)    // R/W
#define E2P_CMD_EPC_CMD_ERASE                   (0x50000000UL)    // R/W
#define E2P_CMD_EPC_CMD_ERAL                    (0x60000000UL)    // R/W
#define E2P_CMD_EPC_CMD_RELOAD                  (0x70000000UL)    // R/W
#define E2P_CMD_EPC_TIMEOUT                     (0x08000000UL)    // RO
#define E2P_CMD_E2P_PROG_GO                     (0x00000200UL)    // WO
#define E2P_CMD_E2P_PROG_DONE                   (0x00000100UL)    // RO
#define E2P_CMD_EPC_ADDR                        (0x000000FFUL)    // R/W

#define E2P_DATA_EEPROM_DATA                    (0x000000FFUL)    // R/W

#define TEST_REG_A_FR_CNT_BYP                   (0x00000008UL)    // R/W
#define TEST_REG_A_PME50M_BYP                   (0x00000004UL)    // R/W
#define TEST_REG_A_PULSE_BYP                    (0x00000002UL)    // R/W
#define TEST_REG_A_PS_BYP                       (0x00000001UL)    // R/W

#define LAN_REGISTER_EXTENT                     (0x00000100UL)

/*
****************************************************************************
*    MAC Control and Status Register (Indirect Address)
*    Offset (through the MAC_CSR CMD and DATA port)
****************************************************************************
*/

#define MAC_CR                                        (0x01UL)    // R/W
#define MAC_CR_RXALL                            (0x80000000UL)
#define MAC_CR_HBDIS                            (0x10000000UL)
#define MAC_CR_RCVOWN                           (0x00800000UL)
#define MAC_CR_LOOPBK                           (0x00200000UL)
#define MAC_CR_FDPX                             (0x00100000UL)
#define MAC_CR_MCPAS                            (0x00080000UL)
#define MAC_CR_PRMS                             (0x00040000UL)
#define MAC_CR_INVFILT                          (0x00020000UL)
#define MAC_CR_PASSBAD                          (0x00010000UL)
#define MAC_CR_HFILT                            (0x00008000UL)
#define MAC_CR_HPFILT                           (0x00002000UL)
#define MAC_CR_LCOLL                            (0x00001000UL)
#define MAC_CR_BCAST                            (0x00000800UL)
#define MAC_CR_DISRTY                           (0x00000400UL)
#define MAC_CR_PADSTR                           (0x00000100UL)
#define MAC_CR_BOLMT_MASK                       (0x000000C0UL)
#define MAC_CR_DFCHK                            (0x00000020UL)
#define MAC_CR_TXEN                             (0x00000008UL)
#define MAC_CR_RXEN                             (0x00000004UL)

#define ADDRH                                         (0x02UL)    // R/W mask 0x0000FFFFUL
#define ADDRL                                         (0x03UL)    // R/W mask 0xFFFFFFFFUL
#define HASHH                                         (0x04UL)    // R/W
#define HASHL                                         (0x05UL)    // R/W

#define MII_ACC                                       (0x06UL)    // R/W
#define MII_ACC_PHY_ADDR                        (0x0000F800UL)
#define MII_ACC_MIIRINDA                        (0x000007C0UL)
#define MII_ACC_MII_WRITE                       (0x00000002UL)
#define MII_ACC_MII_BUSY                        (0x00000001UL)

#define MII_DATA                                      (0x07UL)    // R/W mask 0x0000FFFFUL

#define FLOW                                          (0x08UL)    // R/W
#define FLOW_FCPT                               (0xFFFF0000UL)
#define FLOW_FCPASS                             (0x00000004UL)
#define FLOW_FCEN                               (0x00000002UL)
#define FLOW_FCBSY                              (0x00000001UL)

#define VLAN1                                         (0x09UL)    // R/W mask 0x0000FFFFUL
#define VLAN2                                         (0x0AUL)    // R/W mask 0x0000FFFFUL

#define WUFF                                          (0x0BUL)    // WO
#define FILTER3_COMMAND                         (0x0F000000UL)
#define FILTER2_COMMAND                         (0x000F0000UL)
#define FILTER1_COMMAND                         (0x00000F00UL)
#define FILTER0_COMMAND                         (0x0000000FUL)
#define FILTER3_ADDR_TYPE                       (0x04000000UL)
#define FILTER3_ENABLE                          (0x01000000UL)
#define FILTER2_ADDR_TYPE                       (0x00040000UL)
#define FILTER2_ENABLE                          (0x00010000UL)
#define FILTER1_ADDR_TYPE                       (0x00000400UL)
#define FILTER1_ENABLE                          (0x00000100UL)
#define FILTER0_ADDR_TYPE                       (0x00000004UL)
#define FILTER0_ENABLE                          (0x00000001UL)
#define FILTER3_OFFSET                          (0xFF000000UL)
#define FILTER2_OFFSET                          (0x00FF0000UL)
#define FILTER1_OFFSET                          (0x0000FF00UL)
#define FILTER0_OFFSET                          (0x000000FFUL)

#define FILTER3_CRC                             (0xFFFF0000UL)
#define FILTER2_CRC                             (0x0000FFFFUL)
#define FILTER1_CRC                             (0xFFFF0000UL)
#define FILTER0_CRC                             (0x0000FFFFUL)

#define WUCSR                                         (0x0CUL)    // R/W
#define WUCSR_GUE                               (0x00000200UL)
#define WUCSR_WUFR                              (0x00000040UL)
#define WUCSR_MPR                               (0x00000020UL)
#define WUCSR_WAKE_EN                           (0x00000004UL)
#define WUCSR_MPEN                              (0x00000002UL)

/*
****************************************************************************
*    Chip Specific MII Defines
*    Phy register offsets and bit definition
****************************************************************************
*/

#define LAN911X_PHY_ID                          (0x00C0001CUL)

#define PHY_BCR                               ((CPU_INT32U)0U)
#define PHY_BCR_RESET                    ((CPU_INT16U)0x8000U)
#define PHY_BCR_SPEED_SELECT             ((CPU_INT16U)0x2000U)
#define PHY_BCR_AUTO_NEG_ENABLE          ((CPU_INT16U)0x1000U)
#define PHY_BCR_POWER_DOWN               ((CPU_INT16U)0x0800U)
#define PHY_BCR_RESTART_AUTO_NEG         ((CPU_INT16U)0x0200U)
#define PHY_BCR_DUPLEX_MODE              ((CPU_INT16U)0x0100U)

#define PHY_BSR                               ((CPU_INT32U)1U)
#define PHY_BSR_LINK_STATUS              ((CPU_INT16U)0x0004U)
#define PHY_BSR_REMOTE_FAULT             ((CPU_INT16U)0x0010U)
#define PHY_BSR_AUTO_NEG_COMP            ((CPU_INT16U)0x0020U)
#define PHY_BSR_ANEG_ABILITY             ((CPU_INT16U)0x0008U)

#define PHY_ID_1                              ((CPU_INT32U)2U)
#define PHY_ID_2                              ((CPU_INT32U)3U)

#define PHY_ANEG_ADV                          ((CPU_INT32U)4U)
#define PHY_ANEG_ADV_PAUSE_OP             ((CPU_INT16U)0x0C00)
#define PHY_ANEG_ADV_ASYM_PAUSE           ((CPU_INT16U)0x0800)
#define PHY_ANEG_ADV_SYM_PAUSE            ((CPU_INT16U)0x0400)
#define PHY_ANEG_ADV_10H                  ((CPU_INT16U)0x0020)
#define PHY_ANEG_ADV_10F                  ((CPU_INT16U)0x0040)
#define PHY_ANEG_ADV_100H                 ((CPU_INT16U)0x0080)
#define PHY_ANEG_ADV_100F                 ((CPU_INT16U)0x0100)
#define PHY_ANEG_ADV_SPEED                ((CPU_INT16U)0x01E0)

#define PHY_ANEG_LPA                          ((CPU_INT32U)5U)
#define PHY_ANEG_LPA_100FDX               ((CPU_INT16U)0x0100)
#define PHY_ANEG_LPA_100HDX               ((CPU_INT16U)0x0080)
#define PHY_ANEG_LPA_10FDX                ((CPU_INT16U)0x0040)
#define PHY_ANEG_LPA_10HDX                ((CPU_INT16U)0x0020)

#define PHY_ANEG_EXP                          ((CPU_INT32U)6U)
#define PHY_ANEG_EXP_PDF                  ((CPU_INT16U)0x0010)
#define PHY_ANEG_EXP_LPNPA                ((CPU_INT16U)0x0008)
#define PHY_ANEG_EXP_NPA                  ((CPU_INT16U)0x0004)
#define PHY_ANEG_EXP_PAGE_RX              ((CPU_INT16U)0x0002)
#define PHY_ANEG_EXP_LPANEG_ABLE          ((CPU_INT16U)0x0001)

#define PHY_MODE_CTRL_STS                     ((CPU_INT32U)17) // Mode Control/Status Register
#define MODE_CTRL_STS_FASTRIP            ((CPU_INT16U)0x4000U)
#define MODE_CTRL_STS_EDPWRDOWN          ((CPU_INT16U)0x2000U)
#define MODE_CTRL_STS_LOWSQEN            ((CPU_INT16U)0x0800U)
#define MODE_CTRL_STS_MDPREBP            ((CPU_INT16U)0x0400U)
#define MODE_CTRL_STS_FARLOOPBACK        ((CPU_INT16U)0x0200U)
#define MODE_CTRL_STS_FASTEST            ((CPU_INT16U)0x0100U)
#define MODE_CTRL_STS_REFCLKEN           ((CPU_INT16U)0x0010U)
#define MODE_CTRL_STS_PHYADBP            ((CPU_INT16U)0x0008U)
#define MODE_CTRL_STS_FORCE_G_LINK       ((CPU_INT16U)0x0004U)
#define MODE_CTRL_STS_ENERGYON           ((CPU_INT16U)0x0002U)

#define PHY_INT_SRC                           ((CPU_INT32U)29)
#define PHY_INT_SRC_ENERGY_ON            ((CPU_INT16U)0x0080U)
#define PHY_INT_SRC_ANEG_COMP            ((CPU_INT16U)0x0040U)
#define PHY_INT_SRC_REMOTE_FAULT         ((CPU_INT16U)0x0020U)
#define PHY_INT_SRC_LINK_DOWN            ((CPU_INT16U)0x0010U)

#define PHY_INT_MASK                          ((CPU_INT32U)30)
#define PHY_INT_MASK_ENERGY_ON           ((CPU_INT16U)0x0080U)
#define PHY_INT_MASK_ANEG_COMP           ((CPU_INT16U)0x0040U)
#define PHY_INT_MASK_REMOTE_FAULT        ((CPU_INT16U)0x0020U)
#define PHY_INT_MASK_LINK_DOWN           ((CPU_INT16U)0x0010U)

#define PHY_SPECIAL                           ((CPU_INT32U)31)
#define PHY_SPECIAL_SPD_                 ((CPU_INT16U)0x001CU)
#define PHY_SPECIAL_SPD_10HALF           ((CPU_INT16U)0x0004U)
#define PHY_SPECIAL_SPD_10FULL           ((CPU_INT16U)0x0014U)
#define PHY_SPECIAL_SPD_100HALF          ((CPU_INT16U)0x0008U)
#define PHY_SPECIAL_SPD_100FULL          ((CPU_INT16U)0x0018U)

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

                                                                /* --------------- DEVICE INSTANCE DATA --------------- */
typedef  struct  net_dev_data_lan911x {
#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    NET_CTR         ErrRxPktEmptyCtr;
    NET_CTR         ErrRxSignalCtr;
    NET_CTR         ErrRxBufCtr;
    NET_CTR         ErrRxOvrCtr;
    NET_CTR         ErrRxQCtr;
#endif
    CPU_BOOLEAN     RxRestart;                                  /* Enable Rx auto-restart on overflow.                  */

    CPU_INT32U      TxSeqNum;
    CPU_INT08U     *TxBufPtr;

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    CPU_SIZE_T      RxQEntriesMax;
#endif
    MEM_POOL        RxQPool;
    CPU_INT32U     *RxQStart;                                   /* Ptr to start of Rx Q data                            */
    CPU_INT32U     *RxQEnd;                                     /* Ptr to end   of Rx Q data                            */
    CPU_INT32U     *RxQIn;                                      /* Ptr where next msg will be inserted  in   Rx Q       */
    CPU_INT32U     *RxQOut;                                     /* Ptr where next msg will be extracted from Rx Q       */
    CPU_SIZE_T      RxQSize;                                    /* Size of Rx Q (maximum number of entries)             */
    CPU_SIZE_T      RxQEntries;                                 /* Current number of entries in the Rx Q                */
#ifdef NET_MCAST_MODULE_EN
    CPU_INT08U      MulticastAddrHashBitCtr[64];
#endif
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

                                                                /* ------------ FNCT'S COMMON TO ALL DRV'S ------------ */
static  void         NetDev_Init               (NET_IF             *pif,
                                                NET_ERR            *perr);


static  void         NetDev_Start              (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_Stop               (NET_IF             *pif,
                                                NET_ERR            *perr);


static  void         NetDev_Rx                 (NET_IF             *pif,
                                                CPU_INT08U        **p_data,
                                                CPU_INT16U         *size,
                                                NET_ERR            *perr);

static  void         NetDev_Tx                 (NET_IF             *pif,
                                                CPU_INT08U         *p_data,
                                                CPU_INT16U          size,
                                                NET_ERR            *perr);


static  void         NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                                CPU_INT08U         *paddr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *perr);

static  void         NetDev_AddrMulticastRemove(NET_IF             *pif,
                                                CPU_INT08U         *paddr_hw,
                                                CPU_INT08U          addr_hw_len,
                                                NET_ERR            *perr);


static  void         NetDev_ISR_Handler        (NET_IF             *pif,
                                                NET_DEV_ISR_TYPE    type);

static  void         NetDev_IO_Ctrl            (NET_IF             *pif,
                                                CPU_INT08U          opt,
                                                void               *p_data,
                                                NET_ERR            *perr);


static  void         NetDev_MII_Rd             (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U         *p_data,
                                                NET_ERR            *perr);

static  void         NetDev_MII_Wr             (NET_IF             *pif,
                                                CPU_INT08U          phy_addr,
                                                CPU_INT08U          reg_addr,
                                                CPU_INT16U          data,
                                                NET_ERR            *perr);


                                                                /* ---------------- LAN911x FNCT'S -------------------- */
static  void         NetDev_RxISR_Handler      (NET_IF             *pif);

static  void         NetDev_RxPktDiscard       (NET_IF             *pif,
                                                CPU_INT16U          size);


static  CPU_INT16U   NetDev_RxPktGetSize       (NET_IF             *pif);

static  void         LAN911x_Init              (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         LAN911x_udelay            (NET_IF             *pif,
                                                CPU_INT32U          usec);

static  CPU_BOOLEAN  LAN911x_CSR_Ready         (NET_IF             *pif);

static  CPU_INT32U   LAN911x_CSR_RegRd         (NET_IF             *pif,
                                                CPU_INT32U          reg_offset);

static  void         LAN911x_CSR_RegWr         (NET_IF             *pif,
                                                CPU_INT32U          reg_offset,
                                                CPU_INT32U          val);

static  void         NetDev_Q_Init             (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_Q_Add              (NET_IF             *pif,
                                                CPU_INT32U          msg,
                                                NET_ERR            *perr);

static  CPU_INT32U   NetDev_Q_Remove           (NET_IF             *pif,
                                                NET_ERR            *perr);

static  void         NetDev_Q_Reset            (NET_IF             *pif);


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
                                                                                    /* LAN911x dev API fnct ptrs :      */
const  NET_DEV_API_ETHER  NetDev_API_LAN911x = { NetDev_Init,                       /*   Init/add                       */
                                                 NetDev_Start,                      /*   Start                          */
                                                 NetDev_Stop,                       /*   Stop                           */
                                                 NetDev_Rx,                         /*   Rx                             */
                                                 NetDev_Tx,                         /*   Tx                             */
                                                 NetDev_AddrMulticastAdd,           /*   Multicast addr add             */
                                                 NetDev_AddrMulticastRemove,        /*   Multicast addr remove          */
                                                 NetDev_ISR_Handler,                /*   ISR handler                    */
                                                 NetDev_IO_Ctrl,                    /*   I/O ctrl                       */
                                                 NetDev_MII_Rd,                     /*   Phy reg rd                     */
                                                 NetDev_MII_Wr                      /*   Phy reg wr                     */
                                               };

/*
*********************************************************************************************************
*                                            NetDev_Init()
*
* Description : (1) Initialize Network Driver Layer :
*
*                   (a) Perform Driver Layer OS initialization
*                   (b) Initialize driver status
*
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization error.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_BSP_ETHER  *pdev_bsp;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_BUF_SIZE        buf_size_max;
    NET_BUF_SIZE        buf_ix;
    CPU_SIZE_T          reqd_octets;
    LIB_ERR             lib_err;


                                                                /* ----------- OBTAIN REFERENCE TO CFG/BSP ------------ */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;
    pdev_bsp = (NET_DEV_BSP_ETHER *)pif->Dev_BSP;


                                                                /* --------------- VALIDATE DEVICE CFG ---------------- */
                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }
                                                                /* Validate Rx buf size.                                */
    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )pif->Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }


                                                                /* -------------- ALLOCATE DEV DATA AREA -------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4,
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }


    NetDev_Q_Init(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    LAN911x_Init(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


                                                                /* Init int. ctlr for the specified interface.          */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* Configure Ethernet Controller GPIO                   */
    pdev_bsp->CfgGPIO(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }


   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           LAN911x_Init()
*
* Description : (1) Initialize & start LAN911x :
*
*                   (a) Initialize Registers
*                   (b) Initialize MAC  Address
*                   (c) Initialize Auto Negotiation
*                   (d) Initialize Interrupts
*                   (e) Enable     Receiver/Transmitter
*                   (f) Initialize External Interrupt Controller    See Note #4
*
*
* Argument(s) : perr    a return error code indicating the result of initialization
*                       NET_DEV_ERR_NONE      -  success
*                       NET_DEV_ERR_NIC_OFF   -  link is down, or init failure occured
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*
* Note(s)     : (1) (a) Assumes MAC address to set has previously been initialized by
*
*                       (1) LAN911x's EEPROM            for LAN911x_MAC_ADDR_SEL_EEPROM
*                       (2) Application code            for LAN911x_MAC_ADDR_SEL_CFG
*
*               (2) Interrupts MUST be enabled ONLY after ALL network initialization is complete (see also
*                   'net.c  Net_Init()  Note #4d').
*
*               (3) This function initializes an external interrupt controller (if present) &, if ENABLED,
*                   MUST be developer-implemented in
*
*                       \<Your Product Application>\net_bsp.c
*
*                           where
*                                   <Your Product Application>    directory path for Your Product's Application
*********************************************************************************************************
*/
static  void  LAN911x_Init (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val;
    CPU_INT32U          timeout;
    CPU_INT16U          retries;
    CPU_SR_ALLOC();


                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */

    if ((LAN91_PMT_CTRL & PMT_CTRL_READY) == 0) {               /* Check if controller is in powerdown state.           */
        LAN91_BYTE_TEST = 0;                                    /* Write to BYTE_TEST takes it out of powerdown.        */
        retries = 10;
        do {
            LAN911x_udelay(pif, 10);
            reg_val  = LAN91_PMT_CTRL;
            retries--;
        } while (((reg_val & PMT_CTRL_READY) == 0) && (retries > 0));
        if ((reg_val & PMT_CTRL_READY) == 0) {                  /* Timeout waiting for PM restore.                      */
           *perr = NET_DEV_ERR_DEV_OFF;
            return;
        }
    }

    CPU_CRITICAL_ENTER();

    LAN91_INT_EN = 0;                                           /* Disable all internal interrupt sources               */

    if (!IS_REV_A(LAN91_ID_REV)) {
        retries = 1;
    } else {
        retries = 3;
    }
                                                                /* ------------ RESET MAC AND PHY HARDWARE ------------ */
    while (retries-- > 0)
    {
        LAN91_HW_CFG = HW_CFG_SRST;                             /* Perform a software reset on the LAN911x MAC          */
        reg_val      = LAN91_BYTE_TEST;                         /* Wait for read after write (HW_CFG).                  */

        timeout = 100000;                                       /* Delay for up to 1 sec on reset.                      */

        do {
            LAN911x_udelay(pif, 10);
            reg_val  = LAN91_HW_CFG;
            timeout--;
        } while (((reg_val & HW_CFG_SRST) > 0) && (timeout > 0));

        if ((reg_val & HW_CFG_SRST) > 0) {                      /* Timeout waiting for reset.                           */
            CPU_CRITICAL_EXIT();
           *perr = NET_DEV_ERR_INIT;
            return;
        }
    }

    timeout = 1000;                                             /* Check EEPROM before setting GPIO_CFG.                */
    while (((LAN91_E2P_CMD & E2P_CMD_EPC_BUSY) > 0) && (timeout > 0)) {
        LAN911x_udelay(pif, 5);
        timeout--;
    }
    if ((LAN91_E2P_CMD & E2P_CMD_EPC_BUSY) > 0){                /* Timeout waiting for EEPROM.                          */
        CPU_CRITICAL_EXIT();
       *perr = NET_DEV_ERR_INIT;
        return;
    }

                                                                /* ------------------- INIT INT REG ------------------- */
    LAN91_INT_EN  = 0;                                          /* Disable all internal interrupt sources               */
    LAN91_INT_STS = 0xFFFFFFFF;                                 /* Clear all pending interrupt sources, see note #2)    */

    LAN91_HW_CFG = (LAN911x_CFG_TX_FIFO_SIZE & 0xF) << 16;      /* Reset FIFO level and flow control settings.          */
    switch (LAN911x_CFG_TX_FIFO_SIZE) {                         /* Set automatic flow control values.                   */
                                                                /*  AFC_HI is about ((RX data FIFO size)*2/3)/64        */
                                                                /*  AFC_LO is AFC_HI/2                                  */
                                                                /*  BACK_DUR is about 5uS*(AFC_LO) rounded down         */
        case 2:                                                 /* 13440 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x008C46AF & 0xFFFFFFF0;
             break;
        case 3:                                                 /* 12480 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0082419F & 0xFFFFFFF0;
             break;
        case 4:                                                 /* 11520 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x00783C9F & 0xFFFFFFF0;
             break;
        case 5:                                                 /* 10560 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x006E374F & 0xFFFFFFF0;
             break;
        case 6:                                                 /*  9600 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0064328F & 0xFFFFFFF0;
             break;
        case 7:                                                 /*  8640 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x005A2D7F & 0xFFFFFFF0;
             break;
        case 8:                                                 /*  7680 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0050287F & 0xFFFFFFF0;
             break;
        case 9:                                                 /*  6720 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0046236F & 0xFFFFFFF0;
             break;
        case 10:                                                /*  5760 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x003C1E6F & 0xFFFFFFF0;
             break;
        case 11:                                                /*  4800 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0032195F & 0xFFFFFFF0;
             break;
                                                                /*  AFC_HI is ~1520 bytes less than RX data FIFO size   */
                                                                /*  AFC_LO is AFC_HI/2                                  */
                                                                /*  BACK_DUR is about 5uS*(AFC_LO) rounded down         */
        case 12:                                                /*  3840 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0024124F & 0xFFFFFFF0;
             break;
        case 13:                                                /*  2880 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0015073F & 0xFFFFFFF0;
             break;
        case 14:                                                /*  1920 Rx Data Fifo Size.                             */
             LAN91_AFC_CFG = 0x0006032F & 0xFFFFFFF0;
             break;


        default:
             CPU_CRITICAL_EXIT();
            *perr = NET_DEV_ERR_INIT;
             return;
    }


    reg_val  = LAN91_GPIO_CFG;                                  /* Config GPIO pins to LED output mode (multiplexed).   */
    reg_val |= GPIO_CFG_GPIOBUF2 |
               GPIO_CFG_GPIOBUF1 |
               GPIO_CFG_GPIOBUF0 |
               GPIO_CFG_LED3_EN  |
               GPIO_CFG_LED2_EN  |
               GPIO_CFG_LED1_EN;
    LAN91_GPIO_CFG = reg_val;

    reg_val  =  LAN91_INT_CFG;                                  /* Read the Interrupt Configuration Register            */
    reg_val |= (1 << 24)        |                               /* Deassert IRQ for 1*10us for edge type interrupts     */
                INT_CFG_IRQ_EN  |                               /* Set master interrupt enable bit                      */
                INT_CFG_IRQ_TYPE;                               /* Set IRQ pin push-pull driver                         */
    LAN91_INT_CFG = reg_val;                                    /* Write the new interrupt configuration value          */

    CPU_CRITICAL_EXIT();
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_ISR_Handler()
*
* Description : This function serves as a generic Interrupt Service Routine handler. This ISR handler
*               will service all necessary ISR events for the network interface controller.
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
* Caller(s)   : NetBSP_ISR_Handler().     NET BSP code for handling a group of ISR vectors.
*
* Note(s)     : (1) This function is called from the context of an ISR.
*               (2) The interface number is known at the time of BSP ISR code authoring and
*                   must be converted to a pointer to the interface from within the ISR handler
*                   instead of the lower layer BSP code that calls this ISR Handler.
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT08U         *p_data;
    CPU_INT32U          int_en;
    CPU_INT32U          int_sts;
    CPU_INT32U          active_ints;
    CPU_INT32U          reg_val;
    NET_ERR             err;


    if (type != NET_DEV_ISR_TYPE_UNKNOWN) {                     /* LAN911x must decode interrupt type from int reg.     */
        return;
    }


    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to dev cfg struct.                        */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */

                                                                /* Spurious interrupt check                             */
    if ((LAN91_INT_CFG & (INT_CFG_IRQ_INT | INT_CFG_IRQ_EN)) != (INT_CFG_IRQ_INT | INT_CFG_IRQ_EN)) {
        return;
    }

    int_en = LAN91_INT_EN;                                      /* Read the Interrupt Enable register                   */
    LAN91_INT_EN = 0;                                           /* Disable all internal interrupt sources               */

    int_sts     = LAN91_INT_STS;                                /* Read the Interrupt Status register                   */
    active_ints = int_en & int_sts;                             /* Mask out all non-enabled interrupt sources           */

    if (active_ints & INT_STS_RXSTOP_INT) {                     /* Rx Stopped                                           */
        LAN91_RX_CFG |= RX_CFG_RX_DUMP;                         /* Dump Rx FIFO                                         */

        if (pdev_data->RxRestart == 1) {
            pdev_data->RxRestart  = 0;
            reg_val = LAN91_BYTE_TEST;                          /* Wait for Rx FIFO dump.                               */
            while ((LAN91_RX_CFG & RX_CFG_RX_DUMP) > 0) {
                ;
            }

            reg_val  = LAN911x_CSR_RegRd(pif, MAC_CR);          /* Read the MAC Control Register's current value        */
            reg_val |= MAC_CR_RXEN;                             /* Set the receiver enable bit                          */
            LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);            /* Write the receiver configuration back to the MAC     */
        }

        LAN91_INT_STS = INT_STS_RXSTOP_INT;
    }

    if (active_ints & INT_STS_TXSTOP_INT) {                     /* Tx Stopped                                           */
        LAN91_TX_CFG |= TX_CFG_TXS_DUMP |                       /* Dump Tx FIFO                                         */
                        TX_CFG_TXD_DUMP;
        LAN91_INT_STS = INT_STS_TXSTOP_INT;
    }

    if (active_ints & INT_STS_RSFL) {                           /* Rx Interrupt                                         */
        NetDev_RxISR_Handler(pif);
        LAN91_INT_STS = INT_STS_RSFL;
    }

    if (active_ints & INT_STS_RDFL) {                           /* Rx Data FIFO exceeds set level (Rev A)               */
        if (IS_REV_A(LAN91_ID_REV)) {
            reg_val  = LAN911x_CSR_RegRd(pif, MAC_CR);          /* Read the MAC Control Register's current value        */
            reg_val &= ~MAC_CR_RXEN;                            /* Clear the receiver enable bit                        */
            LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);            /* Write the receiver configuration back to the MAC     */
            NET_CTR_ERR_INC(pdev_data->ErrRxOvrCtr);
            pdev_data->RxRestart = 1;                           /* Restart Rx after it has stopped completely .         */
            NetDev_Q_Reset(pif);                                /* Reset Rx status words queue.                         */
        }
        LAN91_INT_STS = INT_STS_RDFL;
    }

    if (active_ints & INT_STS_RDFO) {                           /* Rx Data FIFO exceeds set level (Not Rev A)           */
        if (!IS_REV_A(LAN91_ID_REV)) {
            reg_val  = LAN911x_CSR_RegRd(pif, MAC_CR);          /* Read the MAC Control Register's current value        */
            reg_val &= ~MAC_CR_RXEN;                            /* Clear the receiver enable bit                        */
            LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);            /* Write the receiver configuration back to the MAC     */
            NET_CTR_ERR_INC(pdev_data->ErrRxOvrCtr);
            pdev_data->RxRestart = 1;                           /* Restart Rx after it has stopped completely .         */
            NetDev_Q_Reset(pif);                                /* Reset Rx status words queue.                         */
        }
        LAN91_INT_STS = INT_STS_RDFO;
    }

    if ((active_ints & INT_STS_TSFL) > 0) {                     /* Tx Interrupt                                         */
        reg_val   = LAN91_TX_FIFO_INF;                          /* Read Tx FIFO Information reg.                        */
        reg_val  &= TX_FIFO_INF_TSUSED;                         /* Retrieve used FIFO space.                            */
        reg_val >>= 16;

        if (reg_val > 0) {
            reg_val = LAN91_TX_STATUS_FIFO;

            if ((reg_val & TX_STS_ES) != 0) {                   /* Error detected on tx packet                          */
                ;
            }

            p_data = pdev_data->TxBufPtr;
            NetIF_TxDeallocTaskPost(p_data, &err);              /* Post all transmitted buffers to deallocation task    */
            if (err == NET_IF_ERR_NONE) {
                NetIF_DevTxRdySignal(pif);                      /* Signal Net IF that Tx resources are available        */
                pdev_data->TxBufPtr = (CPU_INT08U *)0;
            }

            LAN91_INT_STS = INT_STS_TSFL;                       /* Acknowledge Tx interrupt                             */
        }
    }

    LAN91_INT_EN = int_en;                                      /* Re-Enable all internal interrupt sources             */
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
*                               NET_IF_ERR_NULL_PTR             Argument(s) passed a NULL pointer.
*                               NET_ERR_FAULT_NULL_FNCT         Invalid NULL function pointer.
*                               NET_IF_ERR_INVALID_CFG          Invalid/NULL API configuration.
*                               NET_IF_ERR_INVALID_STATE        Invalid network interface state.
*                               NET_IF_ERR_INVALID_ADDR         Invalid hardware address.
*                               NET_IF_ERR_INVALID_ADDR_LEN     Invalid hardware address length.
*
*                                                               - RETURNED BY NetIF_DevCfgTxRdySignal() : -
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

static  void  NetDev_Start (NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          reg_val;
    CPU_INT32U          hw_addr_hi;
    CPU_INT32U          hw_addr_lo;
    CPU_INT08U          hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U          hw_addr_len;
    CPU_BOOLEAN         hw_addr_cfg;
    NET_ERR             err;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)                        /* Init ctr's and state variables.                      */
    pdev_data->ErrRxPktEmptyCtr = 0;
    pdev_data->ErrRxSignalCtr   = 0;
    pdev_data->ErrRxBufCtr      = 0;
    pdev_data->ErrRxOvrCtr      = 0;
    pdev_data->ErrRxQCtr        = 0;
#endif
    pdev_data->RxRestart        = 0;
    pdev_data->TxSeqNum         = 0;
    pdev_data->TxBufPtr         = 0;


                                                                /* ---------------- CFG TX RDY SIGNAL ----------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                         */
                             pdev_cfg->TxDescNbr,
                             perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }


                                                                /* ------------------- CFG HW ADDR -------------------- */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #1 & #2.                                   */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...     */
                       &hw_addr[0],                             /* ... (see Note #2a).                                  */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.    */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Else get  app-configured IF layer HW MAC address, ...*/
                                                                /* ... if any (see Note #2b).                           */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...*/
                                                                /* ... HW MAC address, if any (see Note #2c).           */
            CPU_CRITICAL_ENTER();
            reg_val = LAN91_E2P_CMD;                            /* Read the EEPROM command register.                    */

            if ((reg_val & E2P_CMD_E2P_PROG_DONE) > 0) {        /* Chk for EEPROM with a valid MAC address.             */
                hw_addr_hi = LAN911x_CSR_RegRd(pif, ADDRH);     /* Read MAC most  significant bytes.                    */
                hw_addr_lo = LAN911x_CSR_RegRd(pif, ADDRL);     /* Read MAC least significant bytes.                    */
                CPU_CRITICAL_EXIT();

                hw_addr[0] = (hw_addr_lo >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
                hw_addr[1] = (hw_addr_lo >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
                hw_addr[2] = (hw_addr_lo >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
                hw_addr[3] = (hw_addr_lo >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

                hw_addr[4] = (hw_addr_hi >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
                hw_addr[5] = (hw_addr_hi >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

                NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,    /* Configure IF layer to use automatically-loaded   */
                                        (CPU_INT08U *)&hw_addr[0],  /* ... HW MAC address.                              */
                                        (CPU_INT08U  ) sizeof(hw_addr),
                                        (NET_ERR    *) perr);
            } else {
                 CPU_CRITICAL_EXIT();
                *perr  = NET_IF_ERR_INVALID_ADDR;
            }

            if (*perr != NET_IF_ERR_NONE) {                     /* No valid HW MAC address configured, return error.    */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.             */
        hw_addr_lo = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                      ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                      ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                      ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));

        hw_addr_hi = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                      ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));

        CPU_CRITICAL_ENTER();
        LAN911x_CSR_RegWr(pif, ADDRH, hw_addr_hi);              /* Write MAC most  significant bytes.                   */
        LAN911x_CSR_RegWr(pif, ADDRL, hw_addr_lo);              /* Write MAC least significant bytes.                   */
        CPU_CRITICAL_EXIT();
    }


                                                                /* ------------------ CFG MULTICAST ------------------- */
#ifdef NET_MCAST_MODULE_EN
                                                                /* Clr multicast addr reference bits.                   */
    Mem_Clr((void     *)       pdev_data->MulticastAddrHashBitCtr,
            (CPU_SIZE_T)sizeof(pdev_data->MulticastAddrHashBitCtr));
#endif


    CPU_CRITICAL_ENTER();

    reg_val  = LAN91_HW_CFG;                                    /* Config TX.                                           */
    reg_val &= HW_CFG_TX_FIF_SZ   |                             /* Keep TX FIFO buffer size,                            */
               HW_CFG_PHY_CLK_SEL |                             /* Keep Int/Ext Phy selection.                          */
               HW_CFG_SMI_SEL     |
               HW_CFG_EXT_PHY_EN;
                                                                /* Enable TX Store and Forward, TTM and TR bits ...     */
    reg_val |= HW_CFG_SF;                                       /* ... treated as don't care by controller.             */
    LAN91_HW_CFG = reg_val;

    reg_val  =  LAN91_FIFO_INT;                                 /* Update TX stats on every packet received.            */
    reg_val &= ~FIFO_INT_TX_AVAIL_LEVEL;
    reg_val |=  0xFFUL << 24;
    reg_val &= ~FIFO_INT_TX_STS_LEVEL;
    reg_val |=  0 << 16;
    LAN91_FIFO_INT = reg_val;
                                                                /* ------------ INITIALIZE THE TRANSMITTER ------------ */
    reg_val  = LAN911x_CSR_RegRd(pif, MAC_CR);                  /* Read the MAC Control Register                        */
    reg_val |= MAC_CR_TXEN |                                    /* Set the MAC transmitter enable bit                   */
               MAC_CR_HBDIS;
    LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);                    /* Write the value back to the MAC Control register     */

    LAN91_TX_CFG = TX_CFG_TX_ON;                                /* Enable the transmitter                               */
    LAN91_RX_CFG = 0;                                           /* Config 4-byte alignment and no data offset.          */

                                                                /* -------------- INITIALIZE THE RECEIVER ------------- */
    reg_val  =  LAN911x_CSR_RegRd(pif, MAC_CR);                 /* Read the MAC Control Register's current value        */
    reg_val &= ~MAC_CR_PRMS;                                    /* Disable promiscuous mode                             */
    reg_val &= ~MAC_CR_HFILT;                                   /* Disable hash only filtering (perfect IA addr's)      */
    reg_val |=  MAC_CR_HPFILT;                                  /* Enable multicast hash filtering (MC addr's hashed)   */
    reg_val |=  MAC_CR_RXEN;                                    /* Set the receiver enable bit                          */
    LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);                    /* Write the receiver configuration back to the MAC     */

                                                                /* Interrupt on every received packet.                  */
    reg_val  =  LAN91_FIFO_INT;
    reg_val &= ~FIFO_INT_RX_AVAIL_LEVEL;
    reg_val |=  1 << 8;
    reg_val &= ~FIFO_INT_RX_STS_LEVEL;
    reg_val |=  0 << 0;
    LAN91_FIFO_INT = reg_val;
                                                                /* ----------------- ENABLE INTERRUPTS ---------------- */
    reg_val = INT_EN_TSFL_EN       |                            /* Tx Status FIFO Level (TSFL)                          */
              INT_EN_RSFL_EN       |                            /* Rx Status FIFO Level (RSFL)                          */
              INT_EN_RXSTOP_INT_EN |                            /* Rx Stop                                              */
              INT_EN_TXSTOP_INT_EN;                             /* Tx Stop                                              */
    if (IS_REV_A(LAN91_ID_REV)) {
        reg_val |= INT_EN_RDFL_EN;                              /* Rx Data FIFO Level (RDFL)                            */
    } else {
        reg_val |= INT_EN_RDFO_EN;                              /* Rx Data FIFO Overflow (RDFO)                         */
    }
    LAN91_INT_EN  |= reg_val;
    LAN91_INT_CFG |= INT_CFG_IRQ_EN;

    CPU_CRITICAL_EXIT();

   *perr = NET_DEV_ERR_NONE;
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
* Note(s)     : none.
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

static  void  NetDev_Stop (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          reg_val;
    CPU_INT08U         *p_data;
    NET_ERR             err;
    CPU_SR_ALLOC();


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();
    pdev_data->RxRestart = 0;                                   /* Prevent Rx from being restarted.                     */

                                                                /* ---------------- DISABLE INTERRUPTS ---------------- */
    LAN91_INT_EN = INT_EN_RXSTOP_INT_EN |
                   INT_EN_TXSTOP_INT_EN;
                                                                /* --------- DISABLE TRANSMITTER AND RECEIVER --------- */
    reg_val  = LAN911x_CSR_RegRd(pif, MAC_CR);
    reg_val &= ~(MAC_CR_RXEN | MAC_CR_TXEN | MAC_CR_HBDIS);
    LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);

    LAN91_TX_CFG |= TX_CFG_STOP_TX;                             /* Stop transmitter                                     */

    NetDev_Q_Reset(pif);                                        /* Reset Rx status words queue.                         */

                                                                /* ------------- FREE USED TX DESCRIPTOR -------------- */
    p_data = pdev_data->TxBufPtr;
    if (p_data != (CPU_INT08U *)0) {
        NetIF_TxDeallocTaskPost(p_data, &err);                  /* Dealloc tx desc buf         (see Note #2a1).         */
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).         */
        pdev_data->TxBufPtr = (CPU_INT08U *)0;
    }
    CPU_CRITICAL_EXIT();

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : This function returns a pointer to the received data to the caller.
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

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
              NET_DEV_CFG_ETHER  *pdev_cfg;
              CPU_INT08U         *pbuf_new;
              CPU_INT32U         *pbuf_rx32;
              CPU_INT16U         *pbuf_rx16;
              CPU_INT08U         *pbuf_rx08;
    volatile  CPU_INT32U         *pbuf_fifo;
              CPU_INT32U          data_temp;
              CPU_INT16U          len;
              CPU_INT16U          len_dword;
              CPU_INT16U          len_rem;
              CPU_INT32U          status;


                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    status   =  NetDev_Q_Remove(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }

    len = status &    0xFFFF;
    if  ((status & 0x8000000) > 0) {
        NetDev_RxPktDiscard(pif, len);
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
       *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }

                                                                /* Request a buffer                                     */
    pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_TYPE  *)0,
                                 (NET_ERR       *)perr);

    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer, discard the frame.        */
        NetDev_RxPktDiscard(pif, len);
       *size   = (CPU_INT16U  )0;
       *p_data = (CPU_INT08U *)0;
        return;
    }

   *size   = len;
   *p_data = pbuf_new;                                          /* Return a pointer to the newly received data area     */

    pbuf_fifo = &LAN91_RX_FIFO;                                 /* Ptr to rx FIFO port (no function overhead)           */
    pbuf_rx32 = (CPU_INT32U *)pbuf_new;                         /* Ptr to rx buf.                                       */

    len_dword = len / 4;
    while (len_dword-- > 0) {
       *pbuf_rx32++ = *pbuf_fifo;                               /* Rx FIFO buf ptr automatically incremented            */
    }

    len_rem = len % 4;
    switch (len_rem) {
        case 0:
             data_temp = *pbuf_fifo;
             break;

        case 1:
             data_temp = *pbuf_fifo;
             pbuf_rx08 = (CPU_INT08U *)pbuf_rx32;
            *pbuf_rx08 = (CPU_INT08U  )data_temp & 0xFF;
             data_temp = *pbuf_fifo;
             break;

        case 2:
             data_temp = *pbuf_fifo;
             pbuf_rx16 = (CPU_INT16U *)pbuf_rx32;
            *pbuf_rx16 = (CPU_INT16U  )data_temp & 0xFFFF;
             data_temp = *pbuf_fifo;
             break;

        case 3:
             data_temp = *pbuf_fifo;
             pbuf_rx16 = (CPU_INT16U *)pbuf_rx32;
            *pbuf_rx16 = (CPU_INT16U  )data_temp & 0xFFFF;

             pbuf_rx16++;
             pbuf_rx08 = (CPU_INT08U *)pbuf_rx16;
             data_temp >>= 16;
            (void)&pbuf_rx16;

            *pbuf_rx08 = (CPU_INT08U  )data_temp & 0xFF;
             data_temp = *pbuf_fifo;
             break;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Tx()
*
* Description : This function transmits the specified data.
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

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
              NET_DEV_CFG_ETHER  *pdev_cfg;
              NET_DEV_DATA       *pdev_data;
    volatile  CPU_INT32U         *pbuf_fifo;
              CPU_INT32U         *pbuf_tx;
              CPU_INT16U          len;
              CPU_INT16U          len_dword;
              CPU_INT08U          retries;
              CPU_INT32U          seq_num;
              CPU_INT32U          reg_val;
              CPU_INT32U          cmd_a;
              CPU_INT32U          cmd_b;


                                                                /* ------- OBTAIN REFERENCE TO DEVICE CFG/DATA -------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                    */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                         */


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (((CPU_DATA)p_data & 0x3) != 0) {
       *perr = NET_DEV_ERR_INVALID_DATA_PTR;
        return;
    }
#endif

    pdev_data->TxBufPtr = p_data;

    len = (size + 3) & 0xFFFC;                                  /* Determine Tx FIFO space required for this frame.     */
                                                                /* 3 bytes added to frame size to account for overhead. */
                                                                /* Overhead includes Tx Cmd A, Tx Cmd B, and optional   */
                                                                /* offset. Bitwise anded with 0xFFFC since not all      */
                                                                /* overhead is placed into the Tx FIFO.                 */

    reg_val  = LAN91_TX_FIFO_INF;                               /* Read Tx FIFO Information register                    */
    reg_val &= TX_FIFO_INF_TDFREE;                              /* Mask out everything except available Tx space bits   */

    retries = 0;
                                                                /* Chk if Tx FIFO has enough available space            */
    while ((len >= reg_val) && (retries++ < LAN911x_TX_ALLOC_INIT_RETRIES)) {
        LAN911x_udelay(pif, 20);                                /* Wait 20us and try again                              */
        reg_val  = LAN91_TX_FIFO_INF;                           /* Read the Tx FIFO Information register                */
        reg_val &= TX_FIFO_INF_TDFREE;                          /* Mask out everything except available Tx space bits   */
    }

    if (len >= reg_val) {                                       /* Timeout waiting for Tx FIFO to have enough space.    */
       *perr = NET_DEV_ERR_TX_BUSY;                             /* Return error if Tx FIFO did not have enough space.   */
        return;
    }

    cmd_a  = size;                                              /* Cmd A data offset is (0) since buf is 32-bit aligned */
    cmd_a |= TX_CMD_A_INT_FIRST_SEG |                           /* Cfg as first frame segment, ...                      */
             TX_CMD_A_INT_LAST_SEG  |                           /* ... as last  frame segment, ...                      */
             TX_CMD_A_INT_ON_COMP;                              /* ... and interrupt on complete.                       */

    seq_num   = pdev_data->TxSeqNum;
    cmd_b     = size;                                           /* Initialize Command B with the len of the frame       */
    cmd_b    |= seq_num << 16;                                  /* Configure a sequence number for this frame           */
    pdev_data->TxSeqNum++;                                      /* Increment sequence number for next transfer          */

    LAN91_TX_FIFO = cmd_a;                                      /* Write command A                                      */
    LAN91_TX_FIFO = cmd_b;                                      /* Write command B                                      */

    pbuf_fifo = &LAN91_TX_FIFO;                                 /* Create ptr to Tx FIFO port (no function overhead)    */
    pbuf_tx   = (CPU_INT32U *)p_data;                           /* Create ptr to data buffer to be transmitted          */

    len_dword = (size + 3) / 4;                                 /* Calc number of 32 bit words to copy, with ceiling    */
    while (len_dword-- > 0) {                                   /* While there is data to be transmitted,               */
       *pbuf_fifo = *pbuf_tx++;                                 /* Load the FIFO with 4 bytes of data                   */
    }

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
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT08U          reg_val;
    CPU_BOOLEAN         reg_sel;
    CPU_SR_ALLOC();

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without compliment.                       */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    hash    = (crc >> 26) & 0x3F;                               /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26) & 0x1F;                               /* Obtain least 5 significant bits for reg bit sel.     */
    reg_sel = (crc >> 31) & 0x01;                               /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
  (*paddr_hash_ctrs)++;                                         /* Increment hash bit reference ctr.                    */

    CPU_CRITICAL_ENTER();
    if (reg_sel == 0) {                                         /* Set multicast hash reg bit.                          */
        reg_val  =  LAN911x_CSR_RegRd(pif, HASHL);
        reg_val |= (1 << bit_sel);
        LAN911x_CSR_RegWr(pif, HASHL, reg_val);                 /* Set multicast hash reg bit.                          */
    } else {
        reg_val  =  LAN911x_CSR_RegRd(pif, HASHH);
        reg_val |= (1 << bit_sel);
        LAN911x_CSR_RegWr(pif, HASHH, reg_val);                 /* Set multicast hash reg bit.                          */
    }
    CPU_CRITICAL_EXIT();

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
*
*               (3) Do NOT clear hash filter bit MAR7[7] in order to continue receiving broadcast
*                   frames.  Broadcast frames are a special case of multicast frames since
*                   they have the least significant bit of the most significant byte set
*                   to '1'.
*
*                       E.g: FF-FF-FF-FF-FF-FF
*                             ^-- F == binary 111 '1'.
*
*                       Where multicast frames have physical addresses in the form of :
*                           0x01-00-5E-XX-YY-ZZ
*                              ^- 1 == binary 000 '1'
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef NET_MCAST_MODULE_EN
    NET_DEV_DATA       *pdev_data;
    CPU_INT32U          crc;
    CPU_INT08U          bit_sel;
    CPU_INT08U          hash;
    CPU_INT08U         *paddr_hash_ctrs;
    CPU_INT08U          reg_val;
    CPU_BOOLEAN         reg_sel;
    CPU_SR_ALLOC();

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    crc = NetUtil_32BitCRC_Calc((CPU_INT08U  *)paddr_hw,        /* Obtain CRC without compliment.                       */
                                (CPU_INT32U   )addr_hw_len,
                                (NET_ERR     *)perr);

    if (*perr != NET_UTIL_ERR_NONE) {
        return;
    }

    hash    = (crc >> 26) & 0x3F;                               /* Obtain hash from 6 most significant bits of CRC.     */
    bit_sel = (crc >> 26) & 0x1F;                               /* Obtain least 5 significant bits for reg bit sel.     */
    reg_sel = (crc >> 31) & 0x01;                               /* Obtain most significant bit for reg sel.             */

    paddr_hash_ctrs = &pdev_data->MulticastAddrHashBitCtr[hash];
    if (*paddr_hash_ctrs > 1) {                                 /* If hash bit reference ctr not zero.                  */
       (*paddr_hash_ctrs)--;                                    /* Decrement hash bit reference ctr.                    */
        *perr = NET_DEV_ERR_NONE;
         return;
    }

   *paddr_hash_ctrs = 0;                                        /* Zero hash bit references remaining.                  */

    CPU_CRITICAL_ENTER();
    if (reg_sel == 0) {                                         /* Set multicast hash reg bit.                          */
        reg_val  =   LAN911x_CSR_RegRd(pif, HASHL);
        reg_val &= ~(1 << bit_sel);
        LAN911x_CSR_RegWr(pif, HASHL, reg_val);                 /* Set multicast hash reg bit.                          */
    } else {
        reg_val  =   LAN911x_CSR_RegRd(pif, HASHH);
        reg_val &= ~(1 << bit_sel);
        LAN911x_CSR_RegWr(pif, HASHH, reg_val);                 /* Set multicast hash reg bit.                          */
    }
    CPU_CRITICAL_EXIT();

#else
   (void)&pif;                                                  /* Prevent 'variable unused' compiler warnings.         */
   (void)&paddr_hw;
   (void)&addr_hw_len;
#endif

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_IO_Ctrl()
*
* Description : This function implements various hardware functions.
*
* Argument(s) : pif     Pointer to interface requiring service.
*
*               opt     Option code representing desired function to perform. The Network Protocol Suite
*                       specifies the option codes below. Additional option codes may be defined by the
*                       driver developer in the driver's header file.
*                           NET_DEV_LINK_STATE_GET
*                           NET_DEV_LINK_STATE_UPDATE
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
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_IO_Ctrl (NET_IF      *pif,
                              CPU_INT08U   opt,
                              void        *p_data,
                              NET_ERR     *perr)
{
    NET_DEV_LINK_ETHER  *plink_state;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT32U           reg_val;
    CPU_INT16U           duplex;
    CPU_SR_ALLOC();


    switch (opt) {
        case NET_IF_IO_CTRL_LINK_STATE_GET_INFO:
             plink_state = (NET_DEV_LINK_ETHER *)p_data;
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
             CPU_CRITICAL_ENTER();
             reg_val = LAN911x_CSR_RegRd(pif, MAC_CR);          /* Read MAC Control Register for current link config.   */
             if ((reg_val & MAC_CR_FDPX) > 0) {
                 duplex = NET_PHY_DUPLEX_FULL;
             } else {
                 duplex = NET_PHY_DUPLEX_HALF;
             }
             if (plink_state->Duplex != duplex) {
                 reg_val &= ~(MAC_CR_FDPX |
                              MAC_CR_RCVOWN);
                 switch (plink_state->Duplex) {                 /* Update duplex setting on device.                     */
                    case NET_PHY_DUPLEX_FULL:
                         reg_val |= MAC_CR_FDPX;                /* Set the full duplex bit                              */
                         break;
                    case NET_PHY_DUPLEX_HALF:
                         reg_val |= MAC_CR_RCVOWN;              /* Accept all frames though the address filter          */
                         break;
                 }
                 LAN911x_CSR_RegWr(pif, MAC_CR, reg_val);       /* Update the MAC with PHY link settings for duplex     */
             }
             CPU_CRITICAL_EXIT();
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
* Description : Write data over the (R)MII bus to the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               p_data      Pointer to variable to store returned register data.
*
*               perr        Pointer to return error code.
*                               NET_DEV_ERR_NULL_PTR        Pointer argument(s) passed NULL pointer(s).
*                               NET_PHY_ERR_NONE            No error.
*                               NET_PHY_ERR_TIMEOUT_REG_RD  Register read time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_MII_Rd (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U  *p_data,
                             NET_ERR     *perr)
{
    CPU_INT08U  retries;
    CPU_INT16U  ret_val;
    CPU_INT16U  phy_addr_val;
    CPU_INT16U  mii_reg_val;
    CPU_INT32U  mii_acc_val;
    CPU_INT32U  reg_val;
    CPU_SR_ALLOC();


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

    reg_val = LAN911x_CSR_RegRd(pif, MII_ACC);                  /* Check if the MII interface is ready                  */

    if ((reg_val & MII_ACC_MII_BUSY) != 0) {
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
       *p_data = 0;
        return;                                                 /* If the interface is ready, if not, return            */
    }

    phy_addr_val = (phy_addr & 0x1F) << 11;                     /* Determine the MII address of the PHY to write to     */
    mii_reg_val  = (reg_addr & 0x1F) <<  6;                     /* Specify the MII register to be read from             */
    mii_acc_val  =  phy_addr_val |                              /* MII_ACC register value                               */
                    mii_reg_val;

    CPU_CRITICAL_ENTER();
    LAN911x_CSR_RegWr(pif, MII_ACC, mii_acc_val);               /* Write the command to the MII Access register         */


    retries = 0;
    reg_val = LAN911x_CSR_RegRd(pif, MII_ACC);                  /* Read the MII_ACC register to determine MII status    */

    while (((reg_val & MII_ACC_MII_BUSY) > 0) && (retries++ < LAN911x_CSR_RETRIES)) {
        LAN911x_udelay(pif, 25);                                /* Delay 25us and try again.                            */
        reg_val = LAN911x_CSR_RegRd(pif, MII_ACC);              /* Read the MII_ACC register to determine MII status    */
    }

    if ((reg_val & MII_ACC_MII_BUSY) > 0) {
        CPU_CRITICAL_EXIT();
       *perr   = NET_PHY_ERR_TIMEOUT_REG_RD;
       *p_data = 0;
        return;
    }

    ret_val = LAN911x_CSR_RegRd(pif, MII_DATA);                 /* MII bus is ready, read MII register data             */

    CPU_CRITICAL_EXIT();

   *p_data = ret_val & 0xFFFF;
   *perr   = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_MII_Wr()
*
* Description : Write data over the (R)MII bus to the specified PHY register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               phy_addr   (R)MII bus address of the PHY requiring service.
*
*               reg_addr    PHY register number to write to.
*
*               data        Data to write to the specified PHY register.
*
*               perr        Pointer to return error code.
*                               NET_PHY_ERR_NONE            No error.
*                               NET_PHY_ERR_TIMEOUT_REG_WR  Register write time-out.
*
* Return(s)   : none.
*
* Caller(s)   : Various PHY functions.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_MII_Wr (NET_IF      *pif,
                             CPU_INT08U   phy_addr,
                             CPU_INT08U   reg_addr,
                             CPU_INT16U   data,
                             NET_ERR     *perr)
{
    CPU_INT08U  retries;
    CPU_INT16U  phy_addr_val;
    CPU_INT16U  mii_reg_val;
    CPU_INT32U  mii_acc_val;
    CPU_INT32U  reg_val;
    CPU_SR_ALLOC();


    reg_val = LAN911x_CSR_RegRd(pif, MII_ACC);                  /* Check if the MII interface is ready                  */

    if ((reg_val & MII_ACC_MII_BUSY) != 0) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;                                                 /* If the interface is ready, if not, return            */
    }

    CPU_CRITICAL_ENTER();
    LAN911x_CSR_RegWr(pif, MII_DATA, (CPU_INT32U)data);         /* Write the MII data to the MII Data register          */

    phy_addr_val = (phy_addr & 0x1F) << 11;                     /* Determine the MII address of the PHY to write to     */
    mii_reg_val  = (reg_addr & 0x1F) <<  6;                     /* Specify the MII register to be read from             */
    mii_acc_val  =  phy_addr_val |                              /* MII_ACC register value with WRITE command            */
                    mii_reg_val  |
                    MII_ACC_MII_WRITE;

    LAN911x_CSR_RegWr(pif, MII_ACC, mii_acc_val);               /* Write the command to the MII Access register         */


    retries = 0;
    reg_val = LAN911x_CSR_RegRd(pif, MII_ACC);                  /* Read the MII_ACC register to determine MII status    */

    while (((reg_val & MII_ACC_MII_BUSY) > 0) && (retries++ < LAN911x_CSR_RETRIES)) {
        LAN911x_udelay(pif, 25);                                /* Delay 25us and try again.                            */
        reg_val = LAN911x_CSR_RegRd(pif, MII_ACC);              /* Read the MII_ACC register to determine MII status    */
    }
    CPU_CRITICAL_EXIT();

    if ((reg_val & MII_ACC_MII_BUSY) > 0) {
       *perr = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetDev_RxPktGetSize()
*
* Description : (1) Get network packet size :
*
*                   (a) Read packet frame size
*                   (b) Read packet frame's control octet to check for odd packet frame size
*
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
* Return(s)   : Size, in octets, of NIC's next network packet.
*
* Caller(s)   : NetDev_Rx(), NetDev_RxPktDiscard().
*
* Note(s)     : (1) See 'LAN911x PACKET FRAMES' for packet frame format.
*               (2) Returning a size of 0 will cause the stack to discard the bad FIFO frame data
*               (3) The FCS is subtracted from the reported frame size
*********************************************************************************************************
*/

static  CPU_INT16U  NetDev_RxPktGetSize (NET_IF  *pif)
{
    NET_DEV_CFG_ETHER     *pdev_cfg;
    CPU_INT32U             rx_fifo_inf;
    CPU_INT32U             reg_val;
    CPU_INT16U             size;

                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    rx_fifo_inf = LAN91_RX_FIFO_INF;                            /* Read the Rx FIFO Information Register                */
    reg_val     = rx_fifo_inf & 0x00FF0000;                     /* Mask off the number of Rx FIFO status words present  */

    if (reg_val > 0) {                                          /* Confirm there is at least 1 frame in the Rx FIFO     */
        reg_val = LAN91_RX_STATUS_FIFO;                         /* Read Rx FIFO Status to get frame size & err list     */
        if (reg_val > 0) {                                      /* Ensure that there is a frame within the Rx FIFO      */
            if ((reg_val & RX_STS_RESERVED) != 0) {             /* Chk Reserved bits.                                   */
            } else if ((reg_val & (RX_STS_ADDR_FILTER_FAIL |    /* Chk Error bits.                                      */
                                   RX_STS_ES               |
                                   RX_STS_LENGTH_ERR       |
                                   RX_STS_RUNT             |
                                   RX_STS_FRAME_TOO_LONG   |
                                   RX_STS_COLLISION        |
                                   RX_STS_RX_WD_TIMEOUT    |
                                   RX_STS_MII_ERR          |
                                   RX_STS_DRIBBLING        |
                                   RX_STS_CRC_ERR)) != 0) {
                size  = (reg_val & 0x3FFF0000) >> 16;           /* Read size from Rx Status reg.                        */
                size |= 0x80000000;                             /* Mark size as erroneous.                              */
                return (size);                                  /* Return frame size in bytes                           */
            } else {
                size = (reg_val & 0x3FFF0000) >> 16;            /* Read size from Rx Status reg.                        */
                if (size >= NET_IF_ETHER_FRAME_CRC_SIZE) {
                    size -= NET_IF_ETHER_FRAME_CRC_SIZE;        /* Subtract Frame Check Sequence from frame size        */
                }
                return (size);                                  /* Return frame size in bytes                           */
            }
        }
    }

    return (0);                                                 /* If error occured, return invalid frame size = 0      */
}


/*
*********************************************************************************************************
*                                      NetDev_RxPktDiscard()
*
* Description : Discard received frame data from FIFO
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ISR_Handler().
*
* Note(s)     : 1) When using fast-forward to discard a frame, the Rx Status FIFO must also be read. However
*                  since the Rx Status FIFO has already been read in NetDev_RxPktGetSize(), it should not be
*                  read again here.
*********************************************************************************************************
*/

static  void  NetDev_RxPktDiscard (NET_IF      *pif,
                                   CPU_INT16U   size)
{
    NET_DEV_CFG_ETHER     *pdev_cfg;
    volatile  CPU_INT32U  *pbuf_fifo;
    CPU_INT16U             dword_size;
    CPU_INT32U             reg_val;

                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    dword_size = size / 4;                                      /* Calc number of 32-bit words to discard               */

    if (dword_size >= 4) {                                      /* If more than 4, 32-bit words, use fast-forward       */
        LAN91_RX_DP_CTL = RX_DP_CTL_FFWD_BUSY;                  /* Set FIFO Fast Forward on Rx Datapath Control reg     */
        do {
            reg_val = LAN91_RX_DP_CTL;                          /* Read the Receive Datapath Control register           */
        } while ((reg_val & RX_DP_CTL_FFWD_BUSY) > 0);          /* Wait for Rx Data FIFO Fast Forward bit to clear      */

    } else {                                                    /* If less than 4, 32-bit words, read each one          */
        pbuf_fifo  = &LAN91_RX_FIFO;                            /* Ptr to rx FIFO port (no function overhead)           */
        dword_size = (size + 3) / 4;                            /* Calc ceiling of number of 32-bit words to read       */
        while (dword_size-- > 0) {
            reg_val = *pbuf_fifo;                               /* Read 4 bytes from Rx FIFO                            */
        }
    }
}


/*
*********************************************************************************************************
*                                          LAN911x_CSR_RegRd()
*
* Description : Read data from LAN911x MAC Control and Status (CSR) register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               reg_offset  Register address offset.
*
* Return(s)   : Data read from register, or 0xFFFFFFFF on error.
*
* Caller(s)   : NetDev_Init(), NetDev_Start(), NetDev_Stop(), NetDev_IO_Ctrl(), NetDev_MII_Rd(), NetDev_MII_Wr().
*
* Note(s)     : Register ALWAYS reads 32-bit data values.
*********************************************************************************************************
*/

static  CPU_INT32U  LAN911x_CSR_RegRd (NET_IF      *pif,
                                       CPU_INT32U   reg_offset)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val;

                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    reg_val = LAN911x_CSR_Ready(pif);                           /* Check if CSR syncronization port is ready            */
    if (reg_val == DEF_FALSE) {                                 /* If port is not available (after retries), return     */
        return (0xFFFFFFFF);
    }

    LAN91_MAC_CSR_CMD = reg_offset           |                  /* Send MAC command combined with register offset       */
                        MAC_CSR_CMD_CSR_BUSY |
                        MAC_CSR_CMD_R_NOT_W;
    reg_val = LAN91_BYTE_TEST;                                  /* Force flush of previous write.                       */

    reg_val = LAN911x_CSR_Ready(pif);                           /* Check if CSR syncronization port is ready            */
    if (reg_val == DEF_FALSE) {                                 /* If port is not available (after retries), return     */
        return (0xFFFFFFFF);
    }

    reg_val = LAN91_MAC_CSR_DATA;                               /* Read CSR register data                               */

    return (reg_val);                                           /* Return CSR register data                             */
}


/*
*********************************************************************************************************
*                                          LAN911x_CSR_RegWr()
*
* Description : Write data to LAN911x MAC Control and Status (CSR) register.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               reg_offset  Register address offset.
*
*               val         Data to be written.
*
* Return(s)   : None.
*
* Caller(s)   : NetDev_Init(), NetDev_Start(), NetDev_Stop(), NetDev_IO_Ctrl(), NetDev_MII_Rd(), NetDev_MII_Wr().
*
* Note(s)     : Register writes are ALWAYS 32-bit values.
*********************************************************************************************************
*/

static  void  LAN911x_CSR_RegWr (NET_IF      *pif,
                                 CPU_INT32U   reg_offset,
                                 CPU_INT32U   val)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val;

                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    reg_val = LAN911x_CSR_Ready(pif);                           /* Check if the CSR syncronization port is ready        */
    if (reg_val == DEF_FALSE) {                                 /* If port is not available (after retries), return     */
        return;
    }

    LAN91_MAC_CSR_DATA =  val;                                  /* Write data into the CSR data reg                     */
    LAN91_MAC_CSR_CMD  = (reg_offset & 0xFF) |                  /* Perform write operation                              */
                          MAC_CSR_CMD_CSR_BUSY;
    reg_val = LAN91_BYTE_TEST;                                  /* Force flush of previous write.                       */

    reg_val = LAN911x_CSR_Ready(pif);                           /* Check if command has completed, wait and retry       */
}


/*
*********************************************************************************************************
*                                          LAN911x_CSR_Ready()
*
* Description : Check LAN911x MAC Control and Status (CSR) register syncronization port.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
* Return(s)   : DEF_TRUE (1), DEF_FALSE (0).
*
* Caller(s)   : LAN911x_CSR_RegRd(),
*               LAN911x_CSR_RegWr().
*********************************************************************************************************
*/

static  CPU_BOOLEAN  LAN911x_CSR_Ready (NET_IF  *pif)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT08U          retries;
    CPU_INT32U          reg_val;

                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    retries = 0;

    reg_val = LAN91_MAC_CSR_CMD;                                /* Read CSR CMD register to check if CSR port is busy   */
    while (((reg_val & MAC_CSR_CMD_CSR_BUSY) > 0) && (retries++ < LAN911x_CSR_RETRIES)) {
        LAN911x_udelay(pif, 25);                                /* Delay 25us and try again.                            */
        reg_val = LAN91_MAC_CSR_CMD;                            /* Read CSR CMD register to check if CSR port is busy   */
    }

    if ((reg_val & MAC_CSR_CMD_CSR_BUSY) > 0) {
        return (DEF_FALSE);                                     /* Return 0, CSR port is busy  and data was not read    */
    } else {
        return (DEF_TRUE);                                      /* Return 1, CSR port is ready and data was     read    */
    }
}


/*
*********************************************************************************************************
*                                          LAN911x_udelay()
*
* Description : Delay based on the LAN911x free-running counter.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               usec        Time to delay, in micro-seconds.
*
* Return(s)   : none.
*
* Caller(s)   : Various functions.
*********************************************************************************************************
*/

static  void  LAN911x_udelay (NET_IF  *pif, CPU_INT32U  usec)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          time_start;
    CPU_INT32U          time_curr;
    CPU_INT32U          time_delta;

                                                                /* ---------- OBTAIN REFERENCE TO DEVICE CFG ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                    */


    time_start = LAN91_FREE_RUN;
    time_delta = usec * 25;
    do {
        time_curr = LAN91_FREE_RUN;
    } while (time_curr - time_start <= time_delta);
}


/*
*********************************************************************************************************
*                                       NetDev_RxISR_Handler()
*
* Description :
*
* Argument(s) : pif         Pointer to an interface.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ISR_Handler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_RxISR_Handler (NET_IF  *pif)
{
#if ((NET_CTR_CFG_STAT_EN == DEF_ENABLED) || \
     (NET_CTR_CFG_ERR_EN  == DEF_ENABLED))
    NET_DEV_DATA  *pdev_data;
#endif
#if  (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    CPU_SIZE_T     entries;
#endif
    CPU_INT16U     size;
    NET_ERR        err;


                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
#if ((NET_CTR_CFG_STAT_EN == DEF_ENABLED) || \
     (NET_CTR_CFG_ERR_EN  == DEF_ENABLED))
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */
#endif


    size = NetDev_RxPktGetSize(pif);                            /* Get size of received packet.                         */
    if (size == 0) {
        NET_CTR_ERR_INC(pdev_data->ErrRxPktEmptyCtr);
        return;
    }

    NetDev_Q_Add(pif, (CPU_INT32U)size, &err);
    if (err != NET_DEV_ERR_NONE) {
        NET_CTR_ERR_INC(pdev_data->ErrRxQCtr);
        return;
    }

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    entries = pdev_data->RxQEntries;
    if (pdev_data->RxQEntriesMax < entries) {
        pdev_data->RxQEntriesMax = entries;
    }
#endif

    NetIF_RxTaskSignal(pif->Nbr, &err);                         /* Signal Net IF RxQ Task of new frame                  */
    switch (err) {
        case NET_IF_ERR_NONE:
            break;

        case NET_IF_ERR_RX_Q_FULL:
        case NET_IF_ERR_RX_Q_SIGNAL_FAULT:
        default:
            NET_CTR_ERR_INC(pdev_data->ErrRxSignalCtr);
            break;
    }
}


/*
*********************************************************************************************************
*                                          NetDev_Q_Init()
*
* Description : Initialize the queue for holding receive status words.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               perr        Pointer to return error code.
*                               NET_DEV_ERR_NONE            No error.
*                               NET_BUF_ERR_POOL_MEM_ALLOC  Allocation of memory pool failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init().
*********************************************************************************************************
*/

static  void  NetDev_Q_Init(NET_IF   *pif,
                            NET_ERR  *perr)
{
    NET_DEV_DATA  *pdev_data;
    MEM_POOL      *pq_pool;
    CPU_INT32U    *pq_start;
    CPU_INT32U     size;
    CPU_INT32U     nbytes;
    CPU_SIZE_T     reqd_octets;
    LIB_ERR        lib_err;

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */

                                                                /* ---------------- INITIALIZE RX QUEUE --------------- */
    size    =  LAN911x_CFG_RX_QUEUE_SIZE;
    nbytes  =  size * sizeof(CPU_INT32U);
    pq_pool = &pdev_data->RxQPool;
    Mem_PoolCreate((MEM_POOL   *) pq_pool,
                   (void       *) 0,                            /* Allocate Rx queue from main memory.                  */
                   (CPU_SIZE_T  ) 0,
                   (CPU_SIZE_T  ) 1,
                   (CPU_SIZE_T  ) nbytes,                       /* Create 1 block large enough to hold all descriptors. */
                   (CPU_SIZE_T  ) sizeof(CPU_INT32U),
                   (CPU_SIZE_T *)&reqd_octets,
                   (LIB_ERR    *)&lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }

    pq_start = (CPU_INT32U *)Mem_PoolBlkGet(pq_pool, nbytes, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_BUF_ERR_POOL_MEM_ALLOC;
    }

    pdev_data->RxQStart =  pq_start;                            /*  Initialize Rx Q.                                    */
    pdev_data->RxQEnd   = &pq_start[size];
    pdev_data->RxQSize  =  size;

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->RxQEntriesMax = 0;
#endif

    NetDev_Q_Reset(pif);                                        /* Reset Rx status words queue.                         */

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_Q_Add()
*
* Description : Add element to the end of the queue.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               msg         Message to add to the queue.
*
*               perr        Pointer to return error code.
*                               NET_DEV_ERR_NONE        No error.
*                               NET_DEV_ERR_RX          Queue full.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_RxISR_Handler().
*********************************************************************************************************
*/

static  void  NetDev_Q_Add(NET_IF      *pif,
                           CPU_INT32U   msg,
                           NET_ERR     *perr)
{
    NET_DEV_DATA  *pdev_data;
    CPU_SR_ALLOC();

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();
    if (pdev_data->RxQEntries >= pdev_data->RxQSize) {          /* Make sure queue is not full.                         */
        CPU_CRITICAL_EXIT();
       *perr = NET_DEV_ERR_RX;
        return;
    }
   *pdev_data->RxQIn++ = msg;                                   /* Insert message into queue.                           */
    pdev_data->RxQEntries++;                                    /* Update the nbr of entries in the queue.              */
    if (pdev_data->RxQIn == pdev_data->RxQEnd) {                /* Wrap IN ptr when at end of queue.                    */
        pdev_data->RxQIn  = pdev_data->RxQStart;
    }
    CPU_CRITICAL_EXIT();
   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetDev_Q_Remove()
*
* Description : Remove element from the start of the queue.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
*               perr        Pointer to return error code.
*                               NET_DEV_ERR_NONE        No error.
*                               NET_DEV_ERR_RX          Queue empty.
*
* Return(s)   : Message from the queue.
*
* Caller(s)   : NetDev_Rx().
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_Q_Remove(NET_IF   *pif,
                                    NET_ERR  *perr)
{
    NET_DEV_DATA  *pdev_data;
    CPU_INT32U     msg;
    CPU_SR_ALLOC();

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();
    if (pdev_data->RxQEntries == 0) {                           /* Chk for queue empty.                                 */
        CPU_CRITICAL_EXIT();
        msg  = 0;
       *perr = NET_DEV_ERR_RX;
        return(msg);
    }

    msg = *pdev_data->RxQOut++;                                 /* Extract oldest message from the queue.               */
    pdev_data->RxQEntries--;                                    /* Update the number of entries in the queue            */
    if (pdev_data->RxQOut == pdev_data->RxQEnd) {               /* Wrap OUT ptr when at the end of the queue.           */
        pdev_data->RxQOut  = pdev_data->RxQStart;
    }
    CPU_CRITICAL_EXIT();
   *perr = NET_DEV_ERR_NONE;
    return (msg);                                               /* Return message received.                             */
}


/*
*********************************************************************************************************
*                                         NetDev_Q_Remove()
*
* Description : Clear the contents of the queue.
*
* Argument(s) : pif         Pointer to the interface requiring service.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Q_Init(),
*               NetDev_Stop().
*********************************************************************************************************
*/

static  void  NetDev_Q_Reset(NET_IF  *pif)
{
    NET_DEV_DATA  *pdev_data;
    CPU_SR_ALLOC();

                                                                /* --------- OBTAIN REFERENCE TO DEVICE DATA ---------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                         */


    CPU_CRITICAL_ENTER();
    pdev_data->RxQIn      = pdev_data->RxQStart;
    pdev_data->RxQOut     = pdev_data->RxQStart;
    pdev_data->RxQEntries = 0;
    CPU_CRITICAL_EXIT();
}


#endif  /* NET_IF_ETHER_MODULE_EN */

