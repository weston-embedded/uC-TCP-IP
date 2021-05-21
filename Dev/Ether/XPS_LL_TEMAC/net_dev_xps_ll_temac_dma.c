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
*                            Xilinx Local Link Hard TEMAC Driver (Mode DMA)
*                                    Support Both Soft and Hard DMA
*
* Filename : net_dev_xps_ll_temac_dma.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes uC/TCP-IP V2.20.00 (or more recent version) is included in the project build.
*
*            (2) This driver version does not support the extended Multicast mode.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_DEV_XPS_LL_TEMAC_MODULE

#include  <Source/net.h>
#include  <Source/net_util.h>
#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>

#include  <cpu_cache.h>
#include  "net_dev_xps_ll_temac_dma.h"

#ifdef  NET_IF_ETHER_MODULE_EN


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  TEMAC_INDIRECT_REG_ACCESS_DLY_MS                  1    /* Delay for indirect register access                           */
#define  TEMAC_INDIRECT_REG_ACCESS_TO_CNT                  4    /* Timeout count for indirect register access                   */
#define  TEMAC_MDIO_DIV_DFT                               29
#define  DMA_NO_CHANGE                                0x0000
                                                                /* Shift constants for selected masks                           */
#define  DMA_CR_IRQ_TIMEOUT_SHIFT                         24
#define  DMA_CR_IRQ_COUNT_SHIFT                           16
#define  DMA_CR_MSB_ADDR_SHIFT                            12

#define  TEMAC_EMMC_LINKSPD_10                    0x00000000    /* TEMAC_EMCFG_LINKSPD_MASK for 10 Mbit                         */
#define  TEMAC_EMMC_LINKSPD_100                   0x40000000    /* TEMAC_EMCFG_LINKSPD_MASK for 100 Mbit                        */
#define  TEMAC_EMMC_LINKSPD_1000                  0x80000000    /* TEMAC_EMCFG_LINKSPD_MASK for 1000 Mbit                       */

#define  DMA_USERAPP4_INIT_VAL                    0xFFFFFFFF    /* Constant used to init DMA_AppData4 filed in Rx DMA desc      */
#define  DMA_DESC_ALIGN                                   64    /* DMA descriptors alignment                                    */

#define  MDC_FREQ_MAX                                2500000    /* MDC Max Freq 2.5MHz                                          */

#define  MCAST_TAB_ENTRIES_NUM                             4    /* Mulicast table entries number                                */

#define  INT_DISABLE                                       0
#define  INT_ENABLE                                        1


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
*           (3) The Tail pointer is used to keep the current DMA receive chain tail position,
*               this pointer will be updated each time completed receive descriptor are processed
*               by the software.
*
*           (4) All device drivers MUST track the addresses of ALL buffers that have been
*               transmitted and not yet acknowledged through transmit complete interrupts.
*********************************************************************************************************
*/

typedef  struct  dev_desc  DEV_DESC;
                                                                /* ----------------- DMA DESCRIPTOR DATA TYPE ----------------- */
struct  dev_desc {
    CPU_REG32    DMA_NextDescAddr;                              /* DMA Next descriptor address.                                 */
    CPU_REG32    DMA_CurrBufAddr;                               /* DMA Current buffer address.                                  */
    CPU_REG32    DMA_CurrBufLen;                                /* DMA Current buffer len                                       */
    CPU_REG32    DMA_CtrlAndStatus;                             /* DMA Control and status.                                      */
    CPU_REG32    DMA_AppData1;                                  /* DMA Application specific Data                                */
    CPU_REG32    DMA_AppData2;                                  /* DMA Application specific Data                                */
    CPU_REG32    DMA_AppData3;                                  /* DMA Application specific Data                                */
    CPU_REG32    DMA_AppData4;                                  /* DMA Application specific Data                                */
    CPU_REG08    RSVD[32];                                      /* DMA Desc must be 64 bytes aligned                            */
};

                                                                /* ------------------- DEVICE INSTANCE DATA ------------------- */
typedef  struct  net_dev_data {
#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    NET_CTR      StatRxCtrRx;
    NET_CTR      StatRxCtrRxISR;
    NET_CTR      StatTxCtrTxISR;
    NET_CTR      StatRxCtrTaskSignalSelf;
    NET_CTR      StatRxCtrTaskSignalNone;
#endif

#if (NET_CTR_CFG_ERR_EN == DEF_ENABLED)
    NET_CTR      ErrRxCtrDataAreaAlloc;
    NET_CTR      ErrRxCtrTaskSignalFail;
#endif

    MEM_POOL     RxDescPool;
    MEM_POOL     TxDescPool;
    DEV_DESC    *RxBufDescPtrStart;
    DEV_DESC    *RxBufDescPtrCur;
    DEV_DESC    *RxBufDescPtrEnd;
    DEV_DESC    *RxBufDescPtrTail;                              /* See Note #3.                                                 */
    DEV_DESC    *TxBufDescPtrStart;
    DEV_DESC    *TxBufDescPtrCur;
    DEV_DESC    *TxBufDescCompPtr;                              /* See Note #4.                                                 */
    DEV_DESC    *TxBufDescPtrEnd;
    CPU_ADDR     DMA_AddrBase;
    CPU_ADDR     DMA_AddrRx;
    CPU_ADDR     DMA_AddrTx;
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

                                                                                /* -------- FNCT'S COMMON TO ALL DEV'S -------- */
static  void        NetDev_Init               (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_Start              (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_Stop               (NET_IF             *pif,
                                               NET_ERR            *perr);


static  void        NetDev_Rx                 (NET_IF             *pif,
                                               CPU_INT08U        **p_data,
                                               CPU_INT16U         *size,
                                               NET_ERR            *perr);

static  void        NetDev_Tx                 (NET_IF             *pif,
                                               CPU_INT08U         *p_data,
                                               CPU_INT16U          size,
                                               NET_ERR            *perr);

static  void        NetDev_AddrMulticastAdd   (NET_IF             *pif,
                                               CPU_INT08U         *paddr_hw,
                                               CPU_INT08U          addr_hw_len,
                                               NET_ERR            *perr);

static  void        NetDev_AddrMulticastRemove(NET_IF             *pif,
                                               CPU_INT08U         *paddr_hw,
                                               CPU_INT08U          addr_hw_len,
                                               NET_ERR            *perr);


static  void        NetDev_ISR_Handler        (NET_IF             *pif,
                                               NET_DEV_ISR_TYPE    type);

static  void        NetDev_IO_Ctrl            (NET_IF             *pif,
                                               CPU_INT08U          opt,
                                               void               *p_data,
                                               NET_ERR            *perr);


static  void        NetDev_MII_Rd             (NET_IF             *pif,
                                               CPU_INT08U          phy_addr,
                                               CPU_INT08U          reg_addr,
                                               CPU_INT16U         *p_data,
                                               NET_ERR            *perr);

static  void        NetDev_MII_Wr             (NET_IF             *pif,
                                               CPU_INT08U          phy_addr,
                                               CPU_INT08U          reg_addr,
                                               CPU_INT16U          data,
                                               NET_ERR            *perr);


                                                                                /* ----- FNCT'S COMMON TO DMA-BASED DEV'S ----- */
static  void        NetDev_RxDescInit         (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_RxDescFreeAll      (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_RxDescPtrCurInc    (NET_IF             *pif);

static  void        NetDev_TxDescInit         (NET_IF             *pif,
                                               NET_ERR            *perr);


                                                                                /* ------ FNCT'S FOR XPS_LL_TEMAC DEV'S ------- */
static  void        NetDev_SetCoalesce        (NET_IF             *pif,
                                               CPU_INT32U          dma_ch_addr,
                                               CPU_INT08U          cntr,
                                               CPU_INT08U          timer_val);


static  void        NetDev_DMA_Reset          (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_DMA_ErrRecover     (NET_IF             *pif,
                                               NET_ERR            *perr);

static  void        NetDev_DMA_EnDisChInt    (NET_IF             *pif,
                                              CPU_INT32U          dma_ch_addr,
                                              CPU_INT08U          en);


#ifdef NET_MCAST_MODULE_EN
static  void        NetDev_AddrMulticastGet   (NET_IF             *pif,
                                               CPU_INT08U         *paddr_hw,
                                               CPU_INT32U          entry,
                                               NET_ERR            *perr);
#endif


static  void        NetDev_IndirectWr_32      (NET_IF             *pif,
                                               CPU_INT32U          reg_offset,
                                               CPU_INT32U          val);

static  CPU_INT32U  NetDev_IndirectRd_32      (NET_IF             *pif,
                                               CPU_INT32U          reg_offset);


/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/

#ifdef NET_MCAST_MODULE_EN
                                                                /* Addr used to chk avail entries in multicast tbl              */
static  CPU_INT08U  Multicast_FreeHW_Addr1[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static  CPU_INT08U  Multicast_FreeHW_Addr2[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
#endif


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
                                                                                        /* XPS_LL_TEMAC dev API fnct ptrs :     */
const  NET_DEV_API_ETHER  NetDev_API_XPS_LL_TEMAC_DMA = { NetDev_Init,                  /*   Init/add                           */
                                                          NetDev_Start,                 /*   Start                              */
                                                          NetDev_Stop,                  /*   Stop                               */
                                                          NetDev_Rx,                    /*   Rx                                 */
                                                          NetDev_Tx,                    /*   Tx                                 */
                                                          NetDev_AddrMulticastAdd,      /*   Multicast addr add                 */
                                                          NetDev_AddrMulticastRemove,   /*   Multicast addr remove              */
                                                          NetDev_ISR_Handler,           /*   ISR handler                        */
                                                          NetDev_IO_Ctrl,               /*   I/O ctrl                           */
                                                          NetDev_MII_Rd,                /*   Phy reg rd                         */
                                                          NetDev_MII_Wr                 /*   Phy reg wr                         */
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
*               An example device register structure is provided below.
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  TEMAC_REG_RAF;                                   /* Reset and address filter register                            */
    CPU_REG32  TEMAC_REG_TPF;                                   /* Transmit pause frame regsiter                                */
    CPU_REG32  TEMAC_REG_IFGP;                                  /* Transmit inter-frame gap adjustement register                */
    CPU_REG32  TEMAC_REG_IS;                                    /* Interrupt status register                                    */
    CPU_REG32  TEMAC_REG_IP;                                    /* Interrupt pending register                                   */
    CPU_REG32  TEMAC_REG_IE;                                    /* Interrupt enable register                                    */
    CPU_REG32  TEMAC_REG_TTAG;                                  /* Tx VLAN TAG register                                         */
    CPU_REG32  TEMAC_REG_RTAG;                                  /* Rx VLAN TAG register                                         */
    CPU_REG32  TEMAC_REG_MSW;                                   /* Most significant word data register                          */
    CPU_REG32  TEMAC_REG_LSW;                                   /* Least significant word data register                         */
    CPU_REG32  TEMAC_REG_CTL;                                   /* Control register                                             */
    CPU_REG32  TEMAC_REG_RDY;                                   /* Ready status register                                        */
    CPU_REG32  TEMAC_REG_UAWL;                                  /* Unicast address, extended/new multicast mode register        */
    CPU_REG32  TEMAC_REG_UAWU;                                  /* Unicast address, extended/new multicast mode register        */
    CPU_REG32  TEMAC_REG_TPID0;                                 /* TPID0 register                                               */
    CPU_REG32  TEMAC_REG_TPID1;                                 /* TPID1 register                                               */
} NET_DEV;


/*
*********************************************************************************************************
*                                    TEMAC CORE REGSITERS ADDRESS
*********************************************************************************************************
*/
                                                                /* These registers are accessed indirecly through the regsiters */
                                                                /* MSW, LSW and CTL                                             */

#define  TEMAC_REG_RCW0                           0x00000200    /* Rx configuration word 0                                      */
#define  TEMAC_REG_RCW1                           0x00000240    /* Rx configuration word 1                                      */
#define  TEMAC_REG_TC                             0x00000280    /* Tx configuration                                             */
#define  TEMAC_REG_FCC                            0x000002C0    /* Flow control configuration                                   */
#define  TEMAC_REG_EMMC                           0x00000300    /* EMAC mode configuration                                      */
#define  TEMAC_REG_PHYC                           0x00000320    /* RGMII/SGMII configuration                                    */
#define  TEMAC_REG_MC                             0x00000340    /* Management configuration                                     */
#define  TEMAC_REG_UAW0                           0x00000380    /* Unicast address word 0                                       */
#define  TEMAC_REG_UAW1                           0x00000384    /* Unicast address word 1                                       */
#define  TEMAC_REG_MAW0                           0x00000388    /* Multicast address word 0                                     */
#define  TEMAC_REG_MAW1                           0x0000038C    /* Multicast address word 1                                     */
#define  TEMAC_REG_AFM                            0x00000390    /* Address Filter (promiscuous) mode                            */
#define  TEMAC_REG_TIS                            0x000003A0    /* Interrupt status                                             */
#define  TEMAC_REG_TIE                            0x000003A4    /* Interrupt enable                                             */
#define  TEMAC_REG_MIIMWD                         0x000003B0    /* MII management write data                                    */
#define  TEMAC_REG_MIIMAI                         0x000003B4    /* MII management access initiate                               */


/*
*********************************************************************************************************
*                                     REGISTER BIT DEFINITIONS
*
* Note(s) : (1) All necessary register bit definitions should be defined within this section.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       TEMAC REGISTER BIT MASKS
*********************************************************************************************************
*/
                                                                /* ----------- Reset and Address filter bit masks ------------- */
#define  TEMAC_MASK_RAF_HTRST                     0x00000001    /* TEMAC Reset                                                  */
#define  TEMAC_MASK_RAF_MCSTREJ                   0x00000002    /* Reject receive multicast destination address                 */
#define  TEMAC_MASK_RAF_BCSTREJ                   0x00000004    /* Reject receive broadcast destination address                 */
#define  TEMAC_MASK_RAF_TXVTAGMODE                0x00000018    /* Tx VLAN TAG mode                                             */
#define  TEMAC_MASK_RAF_RXVTAGMODE                0x00000060    /* Rx VLAN TAG mode                                             */
#define  TEMAC_MASK_RAF_TXVSTRPMODE               0x00000180    /* Tx VLAN STRIP mode                                           */
#define  TEMAC_MASK_RAF_RXVSTRPMODE               0x00000600    /* Rx VLAN STRIP mode                                           */
#define  TEMAC_MASK_RAF_NEWFNCENBL                0x00000800    /* New function mode                                            */
#define  TEMAC_MASK_RAF_EMULTIFLTRENBL            0x00001000    /* Multicast Filtering mode                                     */
#define  TEMAC_RAF_TXVTAGMODE_SHIFT                        3    /* Tx Tag mode shift bits                                       */
#define  TEMAC_RAF_RXVTAGMODE_SHIFT                        5    /* Rx Tag mode shift bits                                       */
#define  TEMAC_RAF_TXVSTRPMODE_SHIFT                       7    /* Tx strip mode shift bits                                     */
#define  TEMAC_RAF_RXVSTRPMODE_SHIFT                       9    /* Rx Strip mode shift bits                                     */

                                                                /* ---------- Transmit Pause Frame Register bit masks --------- */
#define  TEMAC_MASK_TPF_TPFV                      0x0000FFFF    /* Tx pause frame value                                         */

                                                                /* --- Transmit Inter-Frame Gap Adjustement Reg. bit masks ---  */
#define  TEMAC_MASK_TFGP_IFGP                     0x0000007F    /* Transmit inter-frame gap adjustment value                    */

                                                                /* -------------- Interrupt register   bit masks -------------- */
#define  TEMAC_MASK_INT_HARDACSCMPLT              0x00000001    /* Hard register access complete                                */
#define  TEMAC_MASK_INT_AUTONEG                   0x00000002    /* Auto negotiation complete                                    */
#define  TEMAC_MASK_INT_RC                        0x00000004    /* Receive complete                                             */
#define  TEMAC_MASK_INT_RXRJECT                   0x00000008    /* Receive frame rejected                                       */
#define  TEMAC_MASK_INT_RXFIFOOVR                 0x00000010    /* Receive fifo overrun                                         */
#define  TEMAC_MASK_INT_TC                        0x00000020    /* Transmit complete                                            */
#define  TEMAC_MASK_INT_RXDCM_LOCK                0x00000040    /* Rx Dcm Lock                                                  */
#define  TEMAC_MASK_INT_MGT_LOCK                  0x00000080    /* MGT clock Lock                                               */
#define  TEMAC_MASK_INT_ALL                       0x0000003F    /* All the ints                                                 */

                                                                /* INT bits that indicate receive errors                        */
#define  TEMAC_MASK_INT_RECV_ERROR               (TEMAC_MASK_INT_RXRJECT | \
                                                  TEMAC_MASK_INT_RXFIFOOVR)

                                                                /* ---------------- Control register bit masks ---------------- */
#define  TEMAC_MASK_CTL_WEN                       0x00008000    /* Write Enable                                                 */

                                                                /* ------ Receive Configuration Word 1 (RCW1) bit masks ------- */
#define  TEMAC_MASK_RCW1_RST                      0x80000000    /* Reset                                                        */
#define  TEMAC_MASK_RCW1_JUM                      0x40000000    /* Jumbo frame enable                                           */
#define  TEMAC_MASK_RCW1_FCS                      0x20000000    /* In-Band FCS enable (FCS not stripped)                        */
#define  TEMAC_MASK_RCW1_RX                       0x10000000    /* Receiver enable                                              */
#define  TEMAC_MASK_RCW1_VLAN                     0x08000000    /* VLAN frame enable                                            */
#define  TEMAC_MASK_RCW1_HD                       0x04000000    /* Half duplex mode                                             */
#define  TEMAC_MASK_RCW1_LT_DIS                   0x02000000    /* Length/type field valid check disable                        */
#define  TEMAC_MASK_RCW1_PAUSEADDR                0x0000FFFF    /* Pause frame source address                                   */

                                                                /* --------- Transmitter Configuration (TC) bit masks --------- */
#define  TEMAC_MASK_TC_RST                        0x80000000    /* Reset                                                        */
#define  TEMAC_MASK_TC_JUM                        0x40000000    /* Jumbo frame enable                                           */
#define  TEMAC_MASK_TC_FCS                        0x20000000    /* In-Band FCS enable (FCS not generated)                       */
#define  TEMAC_MASK_TC_TX                         0x10000000    /* Transmitter enable                                           */
#define  TEMAC_MASK_TC_VLAN                       0x08000000    /* VLAN frame enable                                            */
#define  TEMAC_MASK_TC_HD                         0x04000000    /* Half duplex mode                                             */
#define  TEMAC_MASK_TC_IFG                        0x02000000    /* Inter-frame gap adjustment enable                            */

                                                                /* ------------------ Ready Status bit masks ------------------ */
#define  TEMAC_MASK_RDY_FABR_RR                   0x00000001    /* Fabric read ready                                            */
#define  TEMAC_MASK_RDY_MIIM_RR                   0x00000002    /* MII management read ready                                    */
#define  TEMAC_MASK_RDY_MIIM_WR                   0x00000004    /* MII management write ready                                   */
#define  TEMAC_MASK_RDY_AF_RR                     0x00000008    /* Address filter read ready                                    */
#define  TEMAC_MASK_RDY_AF_WR                     0x00000010    /* Address filter write ready                                   */
#define  TEMAC_MASK_RDY_CFG_RR                    0x00000020    /* Configuration register read ready                            */
#define  TEMAC_MASK_RDY_CFG_WR                    0x00000040    /* Configuration register write ready                           */
#define  TEMAC_MASK_RDY_HARD_ACS_RDY              0x00010000    /* Hard register access ready                                   */

                                                                /* --------------- EMAC mode Configuration (EMMC) ------------- */
#define  TEMAC_MASK_EMMC_LINKSPEED                0xC0000000    /* Link speed                                                   */
#define  TEMAC_MASK_EMMC_RGMII                    0x20000000    /* RGMII mode enable                                            */
#define  TEMAC_MASK_EMMC_SGMII                    0x10000000    /* SGMII mode enable                                            */
#define  TEMAC_MASK_EMMC_GPCS                     0x08000000    /* 1000BaseX mode enable                                        */
#define  TEMAC_MASK_EMMC_HOST                     0x04000000    /* Host interface enable                                        */

                                                                /* ------ EMAC Management Configuration (MC) bit masks -------- */
#define  TEMAC_MASK_MC_MDIOEN                     0x00000040    /* MII management enable                                        */
#define  TEMAC_MC_CLOCK_DIVIDE_MAX                      0x3F    /* Maximum MDIO divisor                                         */

                                                                /* ---------- EMAC Unicast Address Register Word 1 ------------ */
#define  TEMAC_MASK_UAW1_UNICASTADDR              0x0000FFFF    /* Station address bits [47:32]                                 */
                                                                /* Station address bits [31:0]                                  */
                                                                /* are stored in register UAW0                                  */

                                                                /* ------ Media Independent Interface Management (MIIM) ------- */
#define  TEMAC_MASK_MIIM_REGAD                          0x1F    /* MII Phy register address (REGAD)                             */
#define  TEMAC_MASK_MIIM_PHYAD                        0x03E0    /* MII Phy address (PHYAD)                                      */
#define  TEMAC_MIIM_PHYAD_SHIFT                            5    /* MII Shift bits for PHYAD                                     */

                                                                /* --------- EMAC Multicast address word 1 register ----------- */
#define  TEMAC_MASK_MCAST_RNW                     0x00800000    /* Multicast address table register read enable                 */
#define  TEMAC_MAW1_ADDR_SHIFT                            16    /* Num. of bits to shift to select the Mcast table entry        */


/*
*********************************************************************************************************
*                                        DMA ENGINE REGISTERS
*
* Note(s) :     (1) DMA_USE_DCR: is defined in net_bsp.h depending on the hw configuration.
*                   This constant should be defined if the driver is using a Hard DMA.
*                   This constant is equivalent to Xilinx constant XPAR_XLLDMA_USE_DCR which is defined
*                   in xparameters.h file generated automatically when BSP and libraries are created
*********************************************************************************************************
*/
                                                                /* DMA_USE_DCR: see note (1)                                    */
#ifdef   DMA_USE_DCR

#define  DMA_TX_BASE_OFFSET                       0x00000000    /* TX channel registers base offset                             */
#define  DMA_RX_BASE_OFFSET                       0x00000008    /* RX channel registers base offset                             */
#define  DMA_GLB_CR_REG                           0x00000010    /* DMA control register                                         */

                                                                /* -------------- DMA TX and RX channel registers ------------- */
#define  DMA_REG_NDESC                            0x00000000    /* Next descriptor pointer                                      */
#define  DMA_REG_BUFA                             0x00000001    /* Current buffer address                                       */
#define  DMA_REG_BUFL                             0x00000002    /* Current buffer len                                           */
#define  DMA_REG_CDESC                            0x00000003    /* Current descriptor pointer                                   */
#define  DMA_REG_TDESC                            0x00000004    /* Tail descriptor pointer                                      */
#define  DMA_REG_CR                               0x00000005    /* Channel control                                              */
#define  DMA_REG_IRQ                              0x00000006    /* Interrupt register                                           */
#define  DMA_REG_SR                               0x00000007    /* Status                                                       */

#else                                                           /* Non-DCR interface is used                                    */

#define  DMA_TX_BASE_OFFSET                       0x00000000    /* TX channel registers base offset                             */
#define  DMA_RX_BASE_OFFSET                       0x00000020    /* RX channel registers base offset                             */
#define  DMA_GLB_CR_REG                           0x00000040    /* DMA control register                                         */

                                                                /* -------------- DMA TX and RX channel registers ------------- */
#define  DMA_REG_NDESC                            0x00000000    /* Next descriptor pointer                                      */
#define  DMA_REG_BUFA                             0x00000004    /* Current buffer address                                       */
#define  DMA_REG_BUFL                             0x00000008    /* Current buffer len                                           */
#define  DMA_REG_CDESC                            0x0000000C    /* Current descriptor pointer                                   */
#define  DMA_REG_TDESC                            0x00000010    /* Tail descriptor pointer                                      */
#define  DMA_REG_CR                               0x00000014    /* Channel control                                              */
#define  DMA_REG_IRQ                              0x00000018    /* Interrupt register                                           */
#define  DMA_REG_SR                               0x0000001C    /* Status                                                       */

#endif


/*
*********************************************************************************************************
*                                       DMA REGISTER BIT MASKS
*********************************************************************************************************
*/

                                                                /* ---------------- XLL DMACR Register bitmasks --------------- */
#define  DMA_MASK_CR_TX_PAUSE                     0x20000000    /* Pause TX channel                                             */
#define  DMA_MASK_CR_RX_PAUSE                     0x10000000    /* Pause RX channel                                             */
#define  DMA_MASK_CR_PLB_ERR_DIS                  0x00000020    /* Disable PLB error detection                                  */
#define  DMA_MASK_CR_RX_OVFL_ERR_DIS              0x00000010    /* Disable error when 2 or 4 bit coalesce counter overflows     */
#define  DMA_MASK_CR_TX_OVFL_ERR_DIS              0x00000008    /* Disable error when 2 or 4 bit coalesce counter overflows     */
#define  DMA_MASK_CR_TAIL_PTR_EN                  0x00000004    /* Enable use of tail pointer register                          */
#define  DMA_MASK_CR_EN_ARB_HOLD                  0x00000002    /* Enable arbitration hold                                      */
#define  DMA_MASK_CR_SW_RST                       0x00000001    /* Assert Software reset for both channels                      */

                                                                /* ----------- DMA TX and RX CR Resgister bitmasks ------------ */
#define  DMA_MASK_CR_IRQ_TIMEOUT                  0xFF000000    /* Interrupt coalesce waitbound timeout                         */
#define  DMA_MASK_CR_IRQ_COUNT                    0x00FF0000    /* Interrupt coalesce count threshold                           */
#define  DMA_MASK_CR_MSB_ADDR                     0x0000F000    /* MSB address of DMA buf. and descrip. for 36 bit addressing   */
#define  DMA_MASK_CR_APP_EN                       0x00000800    /* Application data mask enable                                 */
#define  DMA_MASK_CR_USE_1_BIT_CNT                0x00000400    /* Turn 4 and 2 bit interrupt counters into 1 bit counters      */
#define  DMA_MASK_CR_USE_INT_ON_END               0x00000200    /* Use interrupt-on-end                                         */
#define  DMA_MASK_CR_LD_IRQ_CNT                   0x00000100    /* Load IRQ_COUNT                                               */
#define  DMA_MASK_CR_IRQ_EN                       0x00000080    /* Master interrupt enable                                      */
#define  DMA_MASK_CR_IRQ_ERR_EN                   0x00000004    /* Enable error interrupt                                       */
#define  DMA_MASK_CR_IRQ_DLY_EN                   0x00000002    /* Enable coalesce delay interrupt                              */
#define  DMA_MASK_CR_IRQ_COALESCE_EN              0x00000001    /* Enable coalesce count interrupt                              */
#define  DMA_MASK_CR_IRQ_ALL_EN                   0x00000087    /* All interrupt enable bits                                    */

                                                                /* ------------ DMA TX and RX SR Resgister bitmasks ----------- */
#define  DMA_MASK_SR_IRQ_ON_END                   0x00000040    /* IRQ on end has occurred                                      */
#define  DMA_MASK_SR_STOP_ON_END                  0x00000020    /* Stop on end has occurred                                     */
#define  DMA_MASK_SR_COMPLETED                    0x00000010    /* Descriptor completed                                         */
#define  DMA_MASK_SR_SOP                          0x00000008    /* Current BD has SOP set                                       */
#define  DMA_MASK_SR_EOP                          0x00000004    /* Current BD has EOP set                                       */
#define  DMA_MASK_SR_BUSY                         0x00000002    /* Channel is busy                                              */

                                                                /* --------- DMA_TX_IRQ_REG & DMA_RX_IRQ_REG bitmasks --------- */
#define  DMA_MASK_IRQ_WRQ_EMPTY                   0x00004000    /* Write Command Queue Empty -- RX channel Only                 */
#define  DMA_MASK_IRQ_COALESCE_COUNTER            0x00003C00    /* Coalesce IRQ 4 bit counter                                   */
#define  DMA_MASK_IRQ_DLY_COUNTER                 0x00000300    /* Coalesce delay IRQ 2 bit counter                             */
#define  DMA_MASK_IRQ_PLB_RD_ERR                  0x00000010    /* PLB Read Error IRQ                                           */
#define  DMA_MASK_IRQ_PLB_WR_ERR                  0x00000008    /* PLB Write Error IRQ                                          */
#define  DMA_MASK_IRQ_ERR                         0x00000004    /* Error IRQ                                                    */
#define  DMA_MASK_IRQ_DLY                         0x00000002    /* Coalesce delay IRQ                                           */
#define  DMA_MASK_IRQ_COALESCE                    0x00000001    /* Coalesce threshold IRQ                                       */
#define  DMA_MASK_IRQ_ALL_ERR                     0x0000001C    /* All error interrupt                                          */

                                                                /* ---- DMA_BD_STSCTRL_USR0_OFFSET descriptor word bitmasks --- */
#define  DMA_MASK_BD_STSCTRL_ERR                  0x80000000    /* DMA error                                                    */
#define  DMA_MASK_BD_STSCTRL_IOE                  0x40000000    /* Interrupt on end                                             */
#define  DMA_MASK_BD_STSCTRL_SOE                  0x20000000    /* Stop on end                                                  */
#define  DMA_MASK_BD_STSCTRL_COMPLETED            0x10000000    /* DMA completed                                                */
#define  DMA_MASK_BD_STSCTRL_SOP                  0x08000000    /* Start of packet                                              */
#define  DMA_MASK_BD_STSCTRL_EOP                  0x04000000    /* End of packet                                                */
#define  DMA_MASK_BD_STSCTRL_BUSY                 0x02000000    /* DMA channel busy                                             */
#define  DMA_MASK_BD_STSCTRL                      0xFF000000    /* Status/Control field                                         */
#define  DMA_MASK_BD_STSCTRL_USR0                 0x00FFFFFF    /* User field #0                                                */


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
*                   (e) Allocate memory for device DMA descriptors
*                   (f) Initialize additional device registers
*                       (1) (R)MII mode / Phy bus type
*                       (2) Disable device interrupts
*                       (3) Disable device receiver and transmitter
*                       (4) Other necessary device initialization
*
* Argument(s) : pif     Pointer to the interface requiring service.
*
*               perr    Pointer to return error code.
*                           NET_DEV_ERR_NONE            No Error.
*                           NET_DEV_ERR_INIT            General initialization error.
*                           NET_BUF_ERR_POOL_MEM_ALLOC  Memory allocation failed.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Add() via 'pdev_api->Init()'.
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
*               (3) The application developer SHOULD define NetDev_CfgGPIO() within net_bsp.c
*                   in order to properly configure any necessary GPIO necessary for the device
*                   to operate properly.  Micrium recommends defining and calling this NetBSP
*                   function even if no additional GPIO initialization is required.
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
*                   (e) pdev_cfg->RxBufDescNbr
*                   (f) pdev_cfg->TxBufDescnbr
*
*               (8) Descriptors are typically required to be contiguous in memory.  Allocation of
*                   descriptors MUST occur as a single contigous block of memory.  The driver may
*                   use pointer arithmetic to sub-divide and traverse the descriptor list.
*
*               (9) NetDev_Init() should exit with :
*
*                   (a) All device interrupt source disabled. External interrupt controllers
*                       should however be ready to accept interrupt requests.
*                   (b) All device interrupt sources cleared.
*                   (c) Both the receiver and transmitter disabled.
*
*              (10) Some drivers MAY require knowledge of the Phy configuration in order
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
*********************************************************************************************************
*/

static  void  NetDev_Init (NET_IF   *pif,
                           NET_ERR  *perr)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_CFG_ETHER             *pdev_cfg;
    NET_DEV_DATA                  *pdev_data;
    NET_DEV                       *pdev;
    NET_BUF_SIZE                   buf_size_max;
    NET_BUF_SIZE                   buf_ix;
    CPU_SIZE_T                     reqd_octets;
    CPU_SIZE_T                     nbytes;
    LIB_ERR                        lib_err;
    CPU_INT32U                     reg_val;
    CPU_INT32U                     reg_val1;
    CPU_INT32U                     timeout_cnt;
    CPU_INT32U                     reset_mask;
    CPU_INT32U                     input_freq;
    CPU_INT32U                     mdc_div;


                                                                /* ------------ OBTAIN REFERENCE TO CFGs/REGs/BSP ------------- */
    pdev_cfg = (NET_DEV_CFG_ETHER            *)pif->Dev_Cfg;
    pdev     = (NET_DEV                      *)pdev_cfg->BaseAddr;
    pdev_bsp = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;


                                                                /* ------------------- VALIDATE DEVICE CFG -------------------- */
                                                                /* Validate Rx buf alignment.                                   */
    if ((pdev_cfg->RxBufAlignOctets % DMA_DESC_ALIGN != 0)) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* Rx DESC. MUST be aligned to DMA_DESC_ALIGN BYTE boundary.    */
         return;
    }

                                                                /* Validate Rx buf ix offset.                           */
    if (pdev_cfg->RxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Rx buf size.                                        */
    buf_ix       = NET_IF_IX_RX;
    buf_size_max = NetBuf_GetMaxSize((NET_IF_NBR     )pif->Nbr,
                                     (NET_TRANSACTION)NET_TRANSACTION_RX,
                                     (NET_BUF       *)0,
                                     (NET_BUF_SIZE   )buf_ix);
    if (buf_size_max < NET_IF_ETHER_FRAME_MAX_SIZE) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* Validate Tx buf alignment.                                   */
    if ((pdev_cfg->TxBufAlignOctets % DMA_DESC_ALIGN != 0)) {
        *perr = NET_DEV_ERR_INVALID_CFG;                        /* Tx DESC. MUST be aligned to DMA_DESC_ALIGN BYTE boundary.    */
         return;
    }

                                                                /* Validate Tx buf ix offset.                           */
    if (pdev_cfg->TxBufIxOffset != 0u) {
       *perr = NET_DEV_ERR_INVALID_CFG;
        return;
    }

                                                                /* ----------------- ALLOCATE DEV DATA AREA ------------------- */
    pif->Dev_Data = Mem_HeapAlloc((CPU_SIZE_T  ) sizeof(NET_DEV_DATA),
                                  (CPU_SIZE_T  ) 4,             /* Check if this value should be 4 or a define constant         */
                                  (CPU_SIZE_T *)&reqd_octets,
                                  (LIB_ERR    *)&lib_err);
    if (pif->Dev_Data == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }

    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;

                                                                /* Get DMA base, RX, and TX channel base address's              */
                                                                /* (see net_bsp.h). These addresses will be used to address     */
                                                                /* the DMA registers                                            */
    pdev_data->DMA_AddrBase = pdev_bsp->DMA_AddrGet();
    pdev_data->DMA_AddrTx   = pdev_bsp->DMA_AddrGet() + DMA_TX_BASE_OFFSET;
    pdev_data->DMA_AddrRx   = pdev_bsp->DMA_AddrGet() + DMA_RX_BASE_OFFSET;

                                                                /* ---------------- ALLOCATE DESCRIPTOR LISTS ----------------- */

    nbytes = pdev_cfg->RxDescNbr * sizeof(DEV_DESC);            /* Allocate Rx desc buf's.                                      */
    pdev_data->RxBufDescPtrStart = Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                 (CPU_SIZE_T  ) pdev_cfg->RxBufAlignOctets,
                                                 (CPU_SIZE_T *)&reqd_octets,
                                                 (LIB_ERR    *)&lib_err);
    if (pdev_data->RxBufDescPtrStart == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }


    nbytes = pdev_cfg->TxDescNbr * sizeof(DEV_DESC);            /* Allocate Tx desc's.                                          */
    pdev_data->TxBufDescPtrStart = Mem_HeapAlloc((CPU_SIZE_T  ) nbytes,
                                                 (CPU_SIZE_T  ) pdev_cfg->TxBufAlignOctets,
                                                 (CPU_SIZE_T *)&reqd_octets,
                                                 (LIB_ERR    *)&lib_err);
    if (pdev_data->TxBufDescPtrStart == (void *)0) {
       *perr = NET_DEV_ERR_MEM_ALLOC;
        return;
    }
                                                                /* ----------------------- TEMAC RESET ------------------------ */
    timeout_cnt  = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    reg_val      = pdev->TEMAC_REG_IS;
    reg_val     &= TEMAC_MASK_INT_MGT_LOCK;

    while (timeout_cnt > 0 && reg_val == 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        reg_val  = pdev->TEMAC_REG_IS;
        reg_val &= TEMAC_MASK_INT_MGT_LOCK;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
       *perr = NET_DEV_ERR_INIT;
        return;
    }

                                                                /* --------------- Stop the device and reset HW --------------- */
    pdev->TEMAC_REG_IE = 0;                                     /* Disable interrupts                                           */

                                                                /* Disable the receiver                                         */
    reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
    reg_val &= ~TEMAC_MASK_RCW1_RX;
    NetDev_IndirectWr_32(pif, TEMAC_REG_RCW1, reg_val);

                                                                /* Get the interrupt pending register value                     */
    reg_val = pdev->TEMAC_REG_IP;
    if (reg_val & TEMAC_MASK_INT_RXRJECT) {
        pdev->TEMAC_REG_IS = TEMAC_MASK_INT_RXRJECT;            /* Set the interrupt status register to clear the interrupt     */
    }

                                                                /* Reset the receiver                                           */
    reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
    reg_val |= TEMAC_MASK_RCW1_RST;
    NetDev_IndirectWr_32(pif, TEMAC_REG_RCW1, reg_val);

                                                                /* Reset the transmitter                                        */
    reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_TC);
    reg_val |= TEMAC_MASK_TC_RST;
    NetDev_IndirectWr_32(pif, TEMAC_REG_TC, reg_val);

                                                                /* Check if reset done                                          */
    reg_val     = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
    reg_val1    = NetDev_IndirectRd_32(pif, TEMAC_REG_TC);
    reg_val    |= reg_val1;
    reset_mask  = TEMAC_MASK_RCW1_RST | TEMAC_MASK_TC_RST;

                                                                /* loop until the reset is done                                 */
    timeout_cnt = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    while ((reg_val & reset_mask) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
        reg_val1 = NetDev_IndirectRd_32(pif, TEMAC_REG_TC);
        reg_val |= reg_val1;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
        *perr    = NET_DEV_ERR_INIT;
        return;
    }
                                                                /* Reset Hard core                                              */
    reg_val             = pdev->TEMAC_REG_RAF;
    reg_val            |= TEMAC_MASK_RAF_HTRST;
    pdev->TEMAC_REG_RAF = reg_val;
    timeout_cnt         = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
                                                                /* Check the ready hard access bit                              */
    reg_val             = pdev->TEMAC_REG_RDY;
    while (!(reg_val & TEMAC_MASK_RDY_HARD_ACS_RDY) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        reg_val        = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
       *perr = NET_DEV_ERR_INIT;
        return;
    }

    reg_val       = pdev->TEMAC_REG_IS;
    reg_val      &= TEMAC_MASK_INT_MGT_LOCK;
    timeout_cnt   = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    while (timeout_cnt > 0 && reg_val == 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        reg_val   = pdev->TEMAC_REG_IS;
        reg_val  &= TEMAC_MASK_INT_MGT_LOCK;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
       *perr = NET_DEV_ERR_INIT;
        return;
    }

                                                                /* --------------------- Set MDIO Divisor --------------------- */
                                                                /*               HOSTCLK freq                                   */
                                                                /* MDC freq = -----------------------                           */
                                                                /*            (1 + Divisor) * 2                                 */
                                                                /*                                                              */
                                                                /* HOSTCLK freq is the bus clock frequency in MHz               */
                                                                /* MDC freq is the MDIO clock freq. in MHz to the PHY           */
                                                                /* MDC freq should not exceed 2.5 MHz                           */

                                                                /* Get MDC clk input freq.                              */
    input_freq = pdev_bsp->ClkFreqGet(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        *perr  = NET_DEV_ERR_INIT;
         return;
    }
    mdc_div   = input_freq / MDC_FREQ_MAX;
    mdc_div >>= 1;
    mdc_div  -= 1;

                                                                /* Set the MDIO divisor                                         */
    NetDev_IndirectWr_32(pif, TEMAC_REG_MC, mdc_div | TEMAC_MASK_MC_MDIOEN);

                                                                /* ------------------------- DMA INIT ------------------------- */
                                                                /* Reset the DMA                                                */
    NetDev_DMA_Reset(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
        *perr  = NET_DEV_ERR_INIT;
         return;
    }


                                                                /* Configure ext int ctrl'r.                            */
    pdev_bsp->CfgIntCtrl(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
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
*                                                               - RETURNED BY NetIF_DevCfgTxRdySignal() : -
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*                               NET_OS_ERR_INIT_DEV_TX_RDY_VAL  Invalid device transmit ready signal.
*
*                                                               ---- RETURNED BY NetDev_RxDescInit() : -----
*                          !!!!
*
*                                                               ---- RETURNED BY NetDev_TxDescInit() : -----
*                          !!!!
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_IF_Start() via 'pdev_api->Start()'.
*
* Note(s)     : (2) Many DMA devices may generate only one interrupt for several ready receive
*                   descriptors.  In order to accommodate this, it is recommended that all DMA
*                   based drivers count the number of ready receive descriptors during the
*                   receive event and signal the receive task accordingly ONLY for those
*                   NEW descriptors which have not yet been accounted for.  Each time a
*                   descriptor is processed (or discarded) the count for acknowledged and
*                   unprocessed frames should be decremented by 1.  This function initializes the
*                   acknowledged receive descriptor count to 0.

*               (3) Setting the maximum number of frames queued for transmission is optional.  By
*                   default, all network interfaces are configured to block until the previous frame
*                   has completed transmission.  However, DMA based interfaces may have several
*                   frames configured for transmission before blocking is required. The number
*                   of queued transmit frames depends on the number of configured transmit
*                   descriptors.
*
*               (4) The physical hardware address should not be configured from NetDev_Init(). Instead,
*                   it should be configured from within NetDev_Start() to allow for the proper use
*                   of NetIF_Ether_HW_AddrSet(), hard coded hardware addresses from the device
*                   configuration structure, or auto-loading EEPROM's. Changes to the physical address
*                   only take effect when the device transitions from the DOWN to UP state.
*
*               (5) The device hardware address is set from one of the data sources below. Each source
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
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_CFG_ETHER             *pdev_cfg;
    NET_DEV_DATA                  *pdev_data;
    NET_DEV                       *pdev;
    CPU_INT32U                     reg_val;
    CPU_INT32U                     mac_addr_tmp;
    CPU_INT08U                     hw_addr[NET_IF_ETHER_ADDR_SIZE];
    CPU_INT08U                     hw_addr_len;
    CPU_BOOLEAN                    hw_addr_cfg;
    NET_ERR                        err;



    pdev_cfg  = (NET_DEV_CFG_ETHER            *)pif->Dev_Cfg;
    pdev      = (NET_DEV                      *)pdev_cfg->BaseAddr;
    pdev_data = (NET_DEV_DATA                 *)pif->Dev_Data;
    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;

#if (NET_CTR_CFG_STAT_EN == DEF_ENABLED)
    pdev_data->StatRxCtrRx             = 0;
    pdev_data->StatRxCtrRxISR          = 0;
    pdev_data->StatTxCtrTxISR          = 0;
    pdev_data->StatRxCtrTaskSignalSelf = 0;
    pdev_data->StatRxCtrTaskSignalNone = 0;
#endif

#if (NET_CTR_CFG_ERR_EN  == DEF_ENABLED)
    pdev_data->ErrRxCtrDataAreaAlloc   = 0;
    pdev_data->ErrRxCtrTaskSignalFail  = 0;
#endif


                                                                /* -------------------- CFG TX RDY SIGNAL --------------------- */
    NetIF_DevCfgTxRdySignal(pif,                                /* See Note #3.                                                 */
                             pdev_cfg->TxDescNbr,
                             perr);
    if (*perr != NET_IF_ERR_NONE) {
         return;
    }


                                                                /* ----------------------- CFG HW ADDR ------------------------ */
    hw_addr_cfg = DEF_NO;                                       /* See Notes #4 & #5.                                           */

    NetASCII_Str_to_MAC(pdev_cfg->HW_AddrStr,                   /* Get configured HW MAC address string, if any ...             */
                       &hw_addr[0],                             /* ... (see Note #5a).                                          */
                       &err);
    if (err == NET_ASCII_ERR_NONE) {
        NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,
                                (CPU_INT08U *)&hw_addr[0],
                                (CPU_INT08U  ) sizeof(hw_addr),
                                (NET_ERR    *)&err);
    }

    if (err == NET_IF_ERR_NONE) {                               /* If no errors, configure device    HW MAC address.            */
        hw_addr_cfg = DEF_YES;

    } else {                                                    /* Else get  app-configured IF layer HW MAC address, ...        */
                                                                /* ... if any (see Note #5b).                                   */
        hw_addr_len = sizeof(hw_addr);
        NetIF_AddrHW_GetHandler(pif->Nbr, &hw_addr[0], &hw_addr_len, &err);
        if (err == NET_IF_ERR_NONE) {
            hw_addr_cfg  = NetIF_AddrHW_IsValidHandler(pif->Nbr, &hw_addr[0], &err);
        } else {
            hw_addr_cfg  = DEF_NO;
        }

        if (hw_addr_cfg != DEF_YES) {                           /* Else attempt to get device's automatically loaded ...        */
                                                                /* ... HW MAC address, if any (see Note #5c).                   */
            mac_addr_tmp =  NetDev_IndirectRd_32(pif, TEMAC_REG_UAW0);
            hw_addr[0]   = (mac_addr_tmp >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[1]   = (mac_addr_tmp >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[2]   = (mac_addr_tmp >> (2 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[3]   = (mac_addr_tmp >> (3 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            mac_addr_tmp =  NetDev_IndirectRd_32(pif, TEMAC_REG_UAW1);
            hw_addr[4]   = (mac_addr_tmp >> (0 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;
            hw_addr[5]   = (mac_addr_tmp >> (1 * DEF_INT_08_NBR_BITS)) & DEF_INT_08_MASK;

            NetIF_AddrHW_SetHandler((NET_IF_NBR  ) pif->Nbr,    /* Configure IF layer to use automatically-loaded ...           */
                                    (CPU_INT08U *)&hw_addr[0],  /* ... HW MAC address.                                          */
                                    (CPU_INT08U  ) sizeof(hw_addr),
                                    (NET_ERR    *) perr);
            if (*perr != NET_IF_ERR_NONE) {                     /* No valid HW MAC address configured, return error.            */
                 return;
            }
        }
    }

    if (hw_addr_cfg == DEF_YES) {                               /* If necessary, set device HW MAC address.                     */
                                                                /* Set MAC bits [31: 0] in UAW0                                 */
        mac_addr_tmp  = (((CPU_INT32U)hw_addr[0] << (0 * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[1] << (1 * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[2] << (2 * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[3] << (3 * DEF_INT_08_NBR_BITS)));
        NetDev_IndirectWr_32(pif, TEMAC_REG_UAW0, mac_addr_tmp);
                                                                /* Don't change reserved bits in UAW1 register                  */
        mac_addr_tmp  =  NetDev_IndirectRd_32(pif, TEMAC_REG_UAW1);
        mac_addr_tmp &= ~TEMAC_MASK_UAW1_UNICASTADDR;
                                                                /* Set MAC bits [47:32] in UAW1                                 */
        mac_addr_tmp  = (((CPU_INT32U)hw_addr[4] << (0 * DEF_INT_08_NBR_BITS)) |
                         ((CPU_INT32U)hw_addr[5] << (1 * DEF_INT_08_NBR_BITS)));
        NetDev_IndirectWr_32(pif, TEMAC_REG_UAW1, mac_addr_tmp);
    }



#ifdef NET_MCAST_MODULE_EN
                                                                /* Allow multicast address filtering                            */
    reg_val              =  pdev->TEMAC_REG_RAF;
    reg_val             &= ~TEMAC_MASK_RAF_MCSTREJ;
    pdev->TEMAC_REG_RAF  =  reg_val;
#endif

    NetDev_RxDescInit(pif, perr);                               /* Init Rx desc's.                                              */
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }

    NetDev_TxDescInit(pif, perr);                               /* Init Tx desc'c.                                              */
    if (*perr != NET_DEV_ERR_NONE) {
        return;
    }
                                                                /* ------------------- SET TEMAC OPTIONS ---------------------- */

    reg_val = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
    reg_val          &= ~TEMAC_MASK_RCW1_JUM;                   /* Jumbo frame disabled                                         */
    reg_val          &= ~TEMAC_MASK_RCW1_FCS;                   /* In-Band FCS (frame check sequence) disabled                  */
    reg_val          &= ~TEMAC_MASK_RCW1_VLAN;                  /* VLAN frame disabled                                          */
    reg_val          &= ~TEMAC_MASK_RCW1_HD;                    /* Full duplex mode enabled                                     */
    reg_val          |=  TEMAC_MASK_RCW1_RX;                    /* Enable Temac Receiver                                        */
    NetDev_IndirectWr_32(pif, TEMAC_REG_RCW1, reg_val);

    reg_val = NetDev_IndirectRd_32(pif, TEMAC_REG_TC);
    reg_val          &= ~TEMAC_MASK_TC_JUM;
    reg_val          &= ~TEMAC_MASK_TC_FCS;
    reg_val          &= ~TEMAC_MASK_TC_VLAN;
    reg_val          &= ~TEMAC_MASK_TC_HD;
    reg_val          |=  TEMAC_MASK_TC_TX;                      /* Enable Temac Transmitter                                     */
    NetDev_IndirectWr_32(pif, TEMAC_REG_TC, reg_val);

                                                                /* Enable Temac interrupts    to handle errors                  */
    pdev->TEMAC_REG_IE = TEMAC_MASK_INT_RXRJECT   |             /* Receive frame rejected                                       */
                         TEMAC_MASK_INT_RXFIFOOVR;              /* Receive fifo overrun                                         */


                                                                /* ---------------- ENABLE DMA INTERRUPTS --------------------- */
                                                                /* Enable DMA RX interrupt.                                     */

    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_CR);

    reg_val |= DMA_MASK_CR_IRQ_EN           |                   /* Master interrupt enable                                      */
               DMA_MASK_CR_IRQ_ERR_EN       |                   /* Enable error interrupt                                       */
               DMA_MASK_CR_IRQ_DLY_EN       |                   /* Enable coalesce delay interrupt                              */
               DMA_MASK_CR_IRQ_COALESCE_EN;                     /* Enable coalesce count interrupt                              */

    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_CR, reg_val);

                                                                /* Enable DMA TX interrupt.                                     */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_CR);

    reg_val |= DMA_MASK_CR_IRQ_EN           |                   /* Master interrupt enable                                      */
               DMA_MASK_CR_IRQ_ERR_EN       |                   /* Enable error interrupt                                       */
               DMA_MASK_CR_IRQ_DLY_EN       |                   /* Enable coalesce delay interrupt                              */
               DMA_MASK_CR_IRQ_COALESCE_EN;                     /* Enable coalesce count interrupt                              */

    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_CR, reg_val);


   (void)&pdev_data;                                            /* Prevent compiler warning.                                    */

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
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_CFG_ETHER             *pdev_cfg;
    NET_DEV_DATA                  *pdev_data;
    NET_DEV                       *pdev;
    DEV_DESC                      *pdesc;
    CPU_INT08U                     i;
    NET_ERR                        err;
    CPU_INT32U                     reg_val;

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct (see Note #3).              */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.              */


                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                                 */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE BSP AREA -----------  */
                                                                /* Obtain ptr to dev bsp area                                   */
    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;

                                                                /* ------------------- DISABLE TEMAC RX & TX ------------------ */
    pdev->TEMAC_REG_IE = 0;                                     /* Disable Temac interrupts                                     */

                                                                /* Disable Temac receiver                                       */
    reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
    reg_val &= ~TEMAC_MASK_RCW1_RX;
    NetDev_IndirectWr_32(pif, TEMAC_REG_RCW1, reg_val);

                                                                /* Disable Temac Transmitter                                    */
    reg_val = NetDev_IndirectRd_32(pif, TEMAC_REG_TC);
    reg_val           &= ~TEMAC_MASK_TC_TX;
    NetDev_IndirectWr_32(pif, TEMAC_REG_TC, reg_val);

                                                                /* Get the interrupt pending register value                     */
    reg_val            = pdev->TEMAC_REG_IP;
    if (reg_val & TEMAC_MASK_INT_RXRJECT) {
        pdev->TEMAC_REG_IS = TEMAC_MASK_INT_RXRJECT;            /* set the interrupt status register to clear the interrupt     */
    }
                                                                /* -------------------- DISABLE DMA RX & TX ------------------- */
                                                                /* Disable TX channel interrupts                                */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_CR);
    reg_val &= ~DMA_MASK_CR_IRQ_ALL_EN;
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_CR, reg_val);

                                                                /* Disable RX channel interrupts                                */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_CR);
    reg_val &= ~DMA_MASK_CR_IRQ_ALL_EN;
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_CR, reg_val);

                                                                /* Clear TX channel interrupts register                         */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_IRQ);
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_IRQ, reg_val);

                                                                /* Clear RX channel interrupts register                         */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_IRQ);
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_IRQ, reg_val);

                                                                /* ------------------- FREE RX DESCRIPTORS -------------------- */
    NetDev_RxDescFreeAll(pif, perr);
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

                                                                /* ------------------ FREE USED TX DESCRIPTORS ---------------- */
    pdesc = &pdev_data->TxBufDescPtrStart[0];
    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Dealloc ALL tx bufs (see Note #2a2).                         */
        NetIF_TxDeallocTaskPost((CPU_INT08U *)pdesc->DMA_CurrBufAddr, &err);
       (void)&err;                                              /* Ignore possible dealloc err (see Note #2b2).                 */
        pdesc++;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                            NetDev_Rx()
*
* Description : (1) This function returns a pointer to the received data to the caller :
*                   (a) Decrement frame counter
*                   (b) Determine which receive descriptor caused the interrupt
*                   (c) Obtain pointer to data area to replace existing data area
*                   (d) Reconfigure descriptor with pointer to new data area
*                   (e) Set return values.  Pointer to received data area and size
*                   (f) Update current receive descriptor pointer
*                   (g) Increment statistic counters
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
*                   (b) If the descriptor IS valid, but an error is indicated within
*                       the descriptor status bits, or length field, then the driver
*                       MUST increment the current receive descriptor pointer and discard
*                       the received frame.
*                   (c) If a new data area is unavailable, the driver MUST increment
*                       the current receive descriptor pointer and discard the received
*                       frame.  This will invoke the DMA to re-use the existing configured
*                       data area.
*
*                (4) Check the DMA_AppData4 field which is initialized with DMA_USERAPP4_INIT_VAL
*                    constant and will store the length of the received frame.
*                    In RX channel case, if "EOP" flag set but its DMA_AppData4 has value
*                    DMA_USERAPP4_INIT_VAL, this means hardware has not completed updating the
*                    RX decriptor structure
*********************************************************************************************************
*/

static  void  NetDev_Rx (NET_IF       *pif,
                         CPU_INT08U  **p_data,
                         CPU_INT16U   *size,
                         NET_ERR      *perr)
{
    NET_DEV_DATA  *pdev_data;
    DEV_DESC      *pdesc;
    CPU_INT08U    *pbuf_new;
    CPU_INT16S     len;


                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                                 */

    pdesc     = pdev_data->RxBufDescPtrCur;                     /* Obtain ptr to next ready desc.                               */

                                                                /* Invalidate the current desc. to be sure to get the recent    */
                                                                /* copie that was recently updated by the DMA Engine            */
    CPU_CacheDataInvalidate(pdesc, sizeof(DEV_DESC));

    *size   = (CPU_INT16U  )0;
    *p_data = (CPU_INT08U *)0;
                                                                /* ----------------- CHECK FOR RECEIVE ERRORS ----------------- */
                                                                /* Check for the End of packet bit and receive errors           */
    if (!(pdesc->DMA_CtrlAndStatus & DMA_MASK_BD_STSCTRL_EOP) ||
         (pdesc->DMA_CtrlAndStatus & DMA_MASK_BD_STSCTRL_ERR)) {
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame (see Note #3b)                        */
        *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }


    if (pdesc->DMA_AppData4 == DMA_USERAPP4_INIT_VAL) {         /* see Note #4                                                 */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame (see Note #3b)                        */
        *perr   = (NET_ERR     )NET_DEV_ERR_RX;
        return;
    }


    len =  pdesc->DMA_AppData4  & 0x3FFF;                       /* Received Frame size                                          */
    if (len < NET_IF_ETHER_FRAME_MIN_SIZE) {                    /* If frame is a runt.                                          */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame (see Note #3b)                        */
        *perr   = (NET_ERR     )NET_DEV_ERR_INVALID_SIZE;
        return;
    }

                                                                /* -------------- OBTAIN PTR TO NEW DMA DATA AREA ------------- */
    pbuf_new = NetBuf_GetDataPtr((NET_IF        *)pif,
                                 (NET_TRANSACTION)NET_TRANSACTION_RX,
                                 (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                 (NET_BUF_SIZE   )NET_IF_IX_RX,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_SIZE  *)0,
                                 (NET_BUF_TYPE  *)0,
                                 (NET_ERR       *)perr);


    if (*perr != NET_BUF_ERR_NONE) {                            /* If unable to get a buffer.                                   */
        NetDev_RxDescPtrCurInc(pif);                            /* Discard received frame (see Note #3b)                        */
        NET_CTR_ERR_INC(pdev_data->ErrRxCtrDataAreaAlloc);
        return;
    }
                                                                /* Invalidate the cache to get the recent received data         */
    CPU_CacheDataInvalidate((CPU_VOID *)pdesc->DMA_CurrBufAddr , len);

   *size                   = len;                               /* Return the size of the recv'd frame.                         */
                                                                /* Return a ptr to the newly recv'd data.                       */
   *p_data                 = (CPU_INT08U *)pdesc->DMA_CurrBufAddr;

    pdesc->DMA_CurrBufAddr =  (CPU_REG32)pbuf_new;              /* Update the desc to point to a new data area                  */

    NET_CTR_STAT_INC(pdev_data->StatRxCtrRx);
                                                                /* Increment the current descriptor and check for other         */
    NetDev_RxDescPtrCurInc(pif);                                /* completed descriptors                                        */

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
*********************************************************************************************************
*/

static  void  NetDev_Tx (NET_IF      *pif,
                         CPU_INT08U  *p_data,
                         CPU_INT16U   size,
                         NET_ERR     *perr)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_DATA                  *pdev_data;
    DEV_DESC                      *pdesc;
    CPU_INT32U                     desc_status;
    CPU_SR_ALLOC();

                                            /* -------------- OBTAIN REFERENCE TO DEVICE BSP -------------- */
    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;   /* Obtain ptr to dev bsp.                                       */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                                 */

    pdesc     =  pdev_data->TxBufDescPtrCur;                    /* Obtain ptr to next available Tx descriptor.                  */

                                                                /* Invalidate Cache to be sure to read recent desc. status      */
    CPU_CacheDataInvalidate((CPU_VOID *)pdesc, sizeof(DEV_DESC));

    CPU_CRITICAL_ENTER();                                       /* This routine reads shared data. Disable interrupts!          */
    desc_status = pdesc->DMA_CtrlAndStatus;
    CPU_CRITICAL_EXIT();


    if (!(desc_status & DMA_MASK_BD_STSCTRL_COMPLETED)) {       /* If the current desc. is not completed, return an error       */
        *perr = NET_DEV_ERR_TX_BUSY;
        return;
    }
                                                                /* Populate the current descriptor fields                       */
    pdesc->DMA_CurrBufLen      =  size;
    pdesc->DMA_CurrBufAddr     = (CPU_REG32)p_data;
    pdesc->DMA_CtrlAndStatus   =  DMA_MASK_BD_STSCTRL_SOP  |    /* Set the Start of packet bit                                  */
                                  DMA_MASK_BD_STSCTRL_EOP;      /* Set the End of packet bit                                    */

                                                                /* Mark the current descriptor as not completed                 */
    pdesc->DMA_CtrlAndStatus  &= ~DMA_MASK_BD_STSCTRL_COMPLETED ;

                                                                /* Flush the populated desc. and the data to be transmitted.    */
                                                                /* This will allow the DMA to see the changes                   */
    CPU_CacheDataFlush((CPU_VOID *)pdesc, sizeof(DEV_DESC));
    CPU_CacheDataFlush((CPU_VOID *)pdesc->DMA_CurrBufAddr , size);

                                                                /* Update the Tail register to start processing the desc.       */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_TDESC, (CPU_INT32U)pdesc);

                                                                /* update the current descriptor with the next desc. address    */
    pdev_data->TxBufDescPtrCur = (DEV_DESC *)pdev_data->TxBufDescPtrCur->DMA_NextDescAddr;

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
*                               NET_DEV_ERR_ADDR_MULTICAST_ADD  Error adding Muticast address.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastAdd()'.
*
* Note(s)     : (1)     A multicast table entry is available if its value is either
*                       0x00.0x00.0x00.0x00.0x00.0x00 or 0xFF.0xFF.0xFF.0xFF.0xFF.0xFF
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastAdd (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT08U   addr_hw_len,
                                       NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val1;
    CPU_INT32U          reg_val2;
    CPU_INT32U          mcast_entry;
    CPU_INT08U          i;
    CPU_INT08U          l_hw_addr[6];
    CPU_BOOLEAN         free_entry;
    CPU_INT32U          rdy_val;
    CPU_INT16U          timeout_cnt;


                                                                /* ------------ OBTAIN REFERENCE TO DEVICE REGISTERS ---------- */
    pdev_cfg   = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;             /* Obtain ptr to the dev cfg struct.                            */
    pdev       = (NET_DEV           *)pdev_cfg->BaseAddr;       /* Overlay dev reg struct on top of dev base addr.              */


    free_entry = DEF_NO;                                        /* Assumes no entry is available in the Multicast table         */
                                                                /* Get the first available entry in the Multicast Table         */
                                                                /* if the multicast addr. is not already stored                 */
    for (i = 0; i < MCAST_TAB_ENTRIES_NUM; i++) {
        NetDev_AddrMulticastGet(pif, &l_hw_addr[0], i, perr);
        if (*perr != NET_DEV_ERR_NONE) {
            *perr  = NET_DEV_ERR_ADDR_MCAST_ADD;
            return;
        }
                                                                /* Check if the hw addr. is already present                     */
        if (Mem_Cmp((void *)&l_hw_addr[0],
                    (void *) paddr_hw,
                             addr_hw_len) == DEF_YES) {
            *perr = NET_DEV_ERR_NONE;                           /* Mutlicast hw addr. exists, so do nothing                     */
             return;
        }
                                                                /* Check if current Multicast table entry is available          */
                                                                /* see Note #1                                                  */
        if (free_entry == DEF_NO) {
            free_entry  = Mem_Cmp((void *)&l_hw_addr[0],
                                  (void *)&Multicast_FreeHW_Addr1[0],
                                           addr_hw_len);

            free_entry |= Mem_Cmp((void *)&l_hw_addr[0],
                                  (void *)&Multicast_FreeHW_Addr2[0],
                                           addr_hw_len);

            if (free_entry == DEF_YES) {
                mcast_entry = i;
            }
        }
    }

    if (free_entry == DEF_NO) {
        *perr   = NET_DEV_ERR_ADDR_MCAST_ADD;
        return;
    }
                                                                /* Set HW addr bits [31:0]                                      */
    reg_val1  =  paddr_hw[0]  & 0x000000FF;
    reg_val1 |=  paddr_hw[1] << 8;
    reg_val1 |=  paddr_hw[2] << 16;
    reg_val1 |=  paddr_hw[3] << 24;
                                                                /* Set HW addr bits [47:32]                                     */
    reg_val2  =  paddr_hw[4]  & 0x000000FF;
    reg_val2 |=  paddr_hw[5] << 8;
                                                                /* Make sure the RNW bit in the TEMAC_REG_MAW1 is set to 0      */
                                                                /* for write                                                    */
    reg_val2 &= ~TEMAC_MASK_MCAST_RNW;
                                                                /* Set the Mcast table entry                                    */
    reg_val2 |= (mcast_entry << TEMAC_MAW1_ADDR_SHIFT);
                                                                /* Write to Hw                                                  */
    NetDev_IndirectWr_32(pif, TEMAC_REG_MAW0, reg_val1);
                                                                /* Poll the Rdy register until the opration completes or        */
                                                                /* timeout occurs                                               */
    rdy_val     = pdev->TEMAC_REG_RDY;
    timeout_cnt = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    while (!(rdy_val & TEMAC_MASK_RDY_AF_WR) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        rdy_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
        *perr = NET_DEV_ERR_ADDR_MCAST_ADD;
         return;
    }
    NetDev_IndirectWr_32(pif, TEMAC_REG_MAW1, reg_val2);

    rdy_val     = pdev->TEMAC_REG_RDY;
    timeout_cnt = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    while (!(rdy_val & TEMAC_MASK_RDY_AF_WR) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        rdy_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
        *perr = NET_DEV_ERR_ADDR_MCAST_ADD;
         return;
    }

#else
    (void)&pif;                                                 /* Prevent 'variable unused' compiler warnings.                 */
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
*                               NET_DEV_ERR_NONE                    Hardware address successfully removed.
*                               NET_DEV_ERR_ADDR_MULTICAST_REMOVE   Error removing the Multicast Hw addr.
* Return(s)   : none.
*
* Caller(s)   : NetIF_Ether_AddrMulticastAdd() via 'pdev_api->AddrMulticastRemove()'.
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetDev_AddrMulticastRemove (NET_IF      *pif,
                                          CPU_INT08U  *paddr_hw,
                                          CPU_INT08U   addr_hw_len,
                                          NET_ERR     *perr)
{
#ifdef  NET_MCAST_MODULE_EN
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val1;
    CPU_INT32U          reg_val2;
    CPU_INT08U          mcast_entry;
    CPU_INT08U          i;
    CPU_INT08U          l_hw_addr[6];
    CPU_INT32U          rdy_val;
    CPU_INT16U          timeout_cnt;


                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                            */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.              */


                                                                /* Find the entry corresponding to the mulicast hw addr.        */
    for (i = 0; i < MCAST_TAB_ENTRIES_NUM; i++) {
        NetDev_AddrMulticastGet(pif, &l_hw_addr[0], i, perr);
        if (*perr != NET_DEV_ERR_NONE) {
            *perr   = NET_DEV_ERR_ADDR_MCAST_REMOVE;
            return;
        }
                                                                /* Check if the hw addr. is present                             */
        if (Mem_Cmp((void *)&l_hw_addr[0],
                    (void *) paddr_hw, addr_hw_len) == DEF_YES) {
            mcast_entry = i;
            break;
        }
    }
                                                                /* Do nothing If the mutlicast hw addr. was not found           */
    if (i == MCAST_TAB_ENTRIES_NUM) {
      *perr   = NET_DEV_ERR_NONE;
       return;
    }
                                                                /* Set multicast table entry to 0 to clear the hw addr          */
    reg_val1  = 0;
    reg_val2  = 0;
                                                                /* Make sure the RNW bit in the TEMAC_REG_MAW1 is set to 0      */
                                                                /* for write                                                    */
    reg_val2 &= ~TEMAC_MASK_MCAST_RNW;
                                                                /* Set the Multicat table entry address                         */
    reg_val2 |= (mcast_entry << TEMAC_MAW1_ADDR_SHIFT);

                                                                /* Write to Hw                                                  */
    NetDev_IndirectWr_32(pif, TEMAC_REG_MAW0, reg_val1);

                                                                /* Poll the Rdy register until the opration completes or        */
                                                                /* timeout occurs                                               */
    rdy_val     = pdev->TEMAC_REG_RDY;
    timeout_cnt = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    while (!(rdy_val & TEMAC_MASK_RDY_AF_WR) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        rdy_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
        *perr = NET_DEV_ERR_ADDR_MCAST_REMOVE;
         return;
    }

    NetDev_IndirectWr_32(pif, TEMAC_REG_MAW1, reg_val2);

    rdy_val     = pdev->TEMAC_REG_RDY;
    timeout_cnt = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    while (!(rdy_val & TEMAC_MASK_RDY_AF_WR) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        rdy_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
        *perr = NET_DEV_ERR_ADDR_MCAST_REMOVE;
         return;
    }

#else
    (void)&pif;                                                 /* Prevent 'variable unused' compiler warnings.                 */
    (void)&paddr_hw;
    (void)&addr_hw_len;

#endif

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_ISR_Handler()
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
* Note(s)     : (1) This function is called via function pointer from the context of an ISR.
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
*********************************************************************************************************
*/

static  void  NetDev_ISR_Handler (NET_IF            *pif,
                                  NET_DEV_ISR_TYPE   type)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_CFG_ETHER             *pdev_cfg;
    NET_DEV_DATA                  *pdev_data;
    NET_DEV                       *pdev;
    DEV_DESC                      *pdesc;
    CPU_INT08U                    *p_data;
    NET_ERR                        err;
    CPU_INT32U                     irq_status;


   (void)&type;                                                 /* Prevent compiler warning if arg 'type' not req'd.            */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg   = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;             /* Obtain ptr to the dev cfg struct.                            */
    pdev       = (NET_DEV           *)pdev_cfg->BaseAddr;       /* Overlay dev reg struct on top of dev base addr.              */

                                                                /* ------------- OBTAIN REFERENCE TO DEVICE BSP --------------- */
    pdev_bsp   = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;  /* Obtain ptr to the dev cfg struct.                            */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data  = (NET_DEV_DATA      *)pif->Dev_Data;            /* Obtain ptr to dev data area.                                 */

                                                                /* ------------------  TEMAC error interrupts ----------------- */
    irq_status = pdev->TEMAC_REG_IP;

    if (irq_status != 0) {
        pdev->TEMAC_REG_IS |= irq_status;                       /* Clear error interrupt                                        */
        return;
    }
                                                                /* ------------------ DMA TX channel interrupts --------------- */
    irq_status    = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_IRQ);

                                                                /* Acknowledge pending interrupts                               */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_IRQ, irq_status);

                                                                /* If an error occurs, reset the DMA to recover from the error */
    if (irq_status & DMA_MASK_IRQ_ALL_ERR) {
        NetDev_DMA_ErrRecover(pif, &err);
        return;
    }
                                                                /* Transmission complete interrupt                              */
    if (irq_status & (DMA_MASK_IRQ_DLY | DMA_MASK_IRQ_COALESCE)) {
        NET_CTR_STAT_INC(pdev_data->StatTxCtrTxISR);

        pdesc                     = pdev_data->TxBufDescCompPtr;
        pdesc->DMA_CtrlAndStatus  = ~DMA_MASK_BD_STSCTRL;
        pdesc->DMA_CtrlAndStatus |= DMA_MASK_BD_STSCTRL_COMPLETED;
        p_data                    = (CPU_INT08U *)pdesc->DMA_CurrBufAddr;

        NetIF_TxDeallocTaskPost(p_data, &err);
        NetIF_DevTxRdySignal(pif);                              /* Signal Net IF that Tx resources are available.               */

        pdev_data->TxBufDescCompPtr = (DEV_DESC *)pdev_data->TxBufDescCompPtr->DMA_NextDescAddr;

                                                                /* This return is important to keep. If a previous RX interrupt */
                                                                /* has occured, the RX task will run until reading all completed*/
                                                                /* descriptors while RX interrupts are disabled                 */
        return;
    }

                                                                /* ---------------- DMA RX channel interrupts ----------------- */
    irq_status    = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_IRQ);

                                                                /* Acknowledge pending interrupts                               */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_IRQ, irq_status);

                                                                /* If an error occured, reset the DMA to recover from the error */
    if (irq_status & DMA_MASK_IRQ_ALL_ERR) {
        NetDev_DMA_ErrRecover(pif, &err);
        return;
    }

                                                                /* Check if reception was done, if so, signal it to the stack   */
    if (irq_status & (DMA_MASK_IRQ_DLY | DMA_MASK_IRQ_COALESCE)) {
        NET_CTR_STAT_INC(pdev_data->StatRxCtrRxISR);
        pdesc     =  pdev_data->RxBufDescPtrCur;
        CPU_CacheDataInvalidate((CPU_VOID *)pdesc, sizeof(DEV_DESC));

        if (!(pdesc->DMA_CtrlAndStatus & DMA_MASK_BD_STSCTRL_COMPLETED)) {
            return;
        }
                                                                /* Disable DMA RX channel interrupts until all completed desc.  */
                                                                /* are read by the Receive Task NetDev_Rx() function            */
        NetDev_DMA_EnDisChInt(pif, pdev_data->DMA_AddrRx, INT_DISABLE);

        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task for each new rdy descriptor.          */

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
    }
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
    NET_DEV_LINK_ETHER  *plink_state;
    NET_PHY_API_ETHER   *pphy_api;
    CPU_INT16U           duplex;
    CPU_INT16U           spd;
    CPU_INT32U           reg_val;


    switch (opt) {                                              /* -------------- PERFORM SPECIFIED OPERATION ----------------- */
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
             duplex      =  NET_PHY_DUPLEX_UNKNOWN;
             if (plink_state->Duplex != duplex) {
                 switch (plink_state->Duplex) {                 /* Update duplex register setting on device.                    */
                    case NET_PHY_DUPLEX_FULL:                   /* The full duplex mode is set by default after Reset           */
                                                                /* Setup the TEMAC TX in Full duplex mode                       */
                         reg_val = NetDev_IndirectRd_32(pif, TEMAC_REG_TC);
                         reg_val &= ~TEMAC_MASK_TC_HD;
                         NetDev_IndirectWr_32(pif, TEMAC_REG_TC, reg_val);

                                                                /* Setup the TEMAC RX in Full duplex mode                       */
                         reg_val = NetDev_IndirectRd_32(pif, TEMAC_REG_RCW1);
                         reg_val &= ~TEMAC_MASK_RCW1_HD;
                         NetDev_IndirectWr_32(pif, TEMAC_REG_RCW1, reg_val);
                         break;

                    case NET_PHY_DUPLEX_HALF:                   /* Half duplex mode not supported (see Xilinx document DS537)   */
                         break;

                    default:
                         break;
                 }
             }

             reg_val  =  NetDev_IndirectRd_32(pif, TEMAC_REG_EMMC);
             spd      =  NET_PHY_SPD_0;
             reg_val &= ~TEMAC_MASK_EMMC_LINKSPEED;
             if (plink_state->Spd != spd) {
                 switch (plink_state->Spd) {                    /* Update speed register setting on device.                     */
                    case NET_PHY_SPD_10:
                         reg_val |= TEMAC_EMMC_LINKSPD_10;      /* 10Base-T                                                     */
                         NetDev_IndirectWr_32(pif, TEMAC_REG_EMMC, reg_val);
                         break;

                    case NET_PHY_SPD_100:
                         reg_val |= TEMAC_EMMC_LINKSPD_100;     /* 100Base-T
                                */
                         NetDev_IndirectWr_32(pif, TEMAC_REG_EMMC, reg_val);
                         break;

                    case NET_PHY_SPD_1000:
                         reg_val |= TEMAC_EMMC_LINKSPD_1000;    /* 1000Base-T                                                   */
                         NetDev_IndirectWr_32(pif, TEMAC_REG_EMMC, reg_val);
                         break;

                    default:
                         break;
                 }
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
* Description : Write data over the (R)MII bus to the specified Phy register.
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
    NET_DEV            *pdev;
    CPU_INT32U          reg_val;
    CPU_INT32U          int_en_reg;
    CPU_INT32U          timeout_cnt;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (p_data == (CPU_INT16U *)0) {
       *perr    =  NET_DEV_ERR_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                            */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.              */

                                                                /* Check if a Hard access can be done                           */
    reg_val = pdev->TEMAC_REG_RDY;
    if (!(reg_val & TEMAC_MASK_RDY_HARD_ACS_RDY)) {
        *perr    = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }                                                           /* NetDev_MII_Rd will poll for the status. The HardAcsCmplt     */
                                                                /* bit is cleared in the IE reg  so that the  application code  */
                                                                /* above doesn't also receive the interrupt.                    */
                                                                /*                                                              */
                                                                /* NetDev_MII_Rd saves the state of the IE reg  so that it can  */
                                                                /* clear the HardAcsCmplt bit and later restore the state of    */
                                                                /* the IE register. Since                                       */

     int_en_reg         = pdev->TEMAC_REG_IE;
     int_en_reg        &= ~TEMAC_MASK_INT_HARDACSCMPLT;
     pdev->TEMAC_REG_IE = int_en_reg;
                                                                /* Indirectly write the PHYAD and REGAD so we can read the PHY   */
                                                                /* register back out in the LSW register.                        */

    reg_val             = reg_addr & TEMAC_MASK_MIIM_REGAD;
    reg_val            |= ((phy_addr << TEMAC_MIIM_PHYAD_SHIFT) & TEMAC_MASK_MIIM_PHYAD);
    pdev->TEMAC_REG_LSW = reg_val;
    pdev->TEMAC_REG_CTL = TEMAC_REG_MIIMAI;

                                                                /* Wait until the value is ready to be read.                    */
    timeout_cnt     = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    do {
        reg_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
        KAL_Dly(1);
    } while (!(reg_val & TEMAC_MASK_RDY_MIIM_RR) && timeout_cnt > 0);

    if (timeout_cnt == 0) {
        *perr    = NET_PHY_ERR_TIMEOUT_REG_RD;
        return;
    }

    *p_data = pdev->TEMAC_REG_LSW;                              /* Read data                                                    */

                                                                /* clear the MIIM WST bit, and then write it back out.          */
    reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_TIS);
    reg_val &= ~TEMAC_MASK_RDY_MIIM_RR;
    NetDev_IndirectWr_32(pif, TEMAC_REG_TIS, reg_val);

    pdev->TEMAC_REG_IE = int_en_reg;                            /* Restore the state of the IE reg                              */

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
    NET_DEV            *pdev;
    CPU_INT32U          reg_val;
    CPU_INT32U          int_en_reg;
    CPU_INT32U          timeout_cnt;


                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                            */
    pdev      = (NET_DEV           *)pdev_cfg->BaseAddr;        /* Overlay dev reg struct on top of dev base addr.              */

                                                                /* Check if a Hard access can be done                           */
    reg_val = pdev->TEMAC_REG_RDY;
    if (!(reg_val & TEMAC_MASK_RDY_HARD_ACS_RDY)) {
        *perr    = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }
                                                                /* NetDev_MII_Wr will poll for the status. The HardAcsCmplt     */
                                                                /* bit is cleared in the IE reg  so that the  application code  */
                                                                /* above doesn't also receive the interrupt.                    */
                                                                /*                                                              */
                                                                /* NetDev_MII_Wr saves the state of the IE reg  so that it can  */
                                                                /* clear the HardAcsCmplt bit and later restore the state of    */
                                                                /* the IE register. Since                                       */

    int_en_reg          = pdev->TEMAC_REG_IE;
    int_en_reg         &= ~TEMAC_MASK_INT_HARDACSCMPLT;
    pdev->TEMAC_REG_IE  =  int_en_reg;
                                                                /* Indirectly write the data to the MIIMWD register, and then   */
                                                                /* indirectly write PHYAD and REGAD so the value in MIIMWD will */
                                                                /* get written to the PHY.                                      */
    NetDev_IndirectWr_32(pif, TEMAC_REG_MIIMWD, data);

    reg_val  = reg_addr & TEMAC_MASK_MIIM_REGAD;
    reg_val |= ((phy_addr << TEMAC_MIIM_PHYAD_SHIFT) & TEMAC_MASK_MIIM_PHYAD);
    NetDev_IndirectWr_32(pif, TEMAC_REG_MIIMAI, reg_val);

                                                                /* Wait until the value is ready to be read.                    */
    timeout_cnt     = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;
    do {
        reg_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
        KAL_Dly(1);
    } while (!(reg_val & TEMAC_MASK_RDY_MIIM_WR) && timeout_cnt > 0);

     if (timeout_cnt == 0) {
        *perr    = NET_PHY_ERR_TIMEOUT_REG_WR;
        return;
    }
                                                                /* clear the MIIM WST bit, and then write it back out.          */
    reg_val  = NetDev_IndirectRd_32(pif, TEMAC_REG_TIS);
    reg_val &= TEMAC_MASK_RDY_MIIM_WR;
    NetDev_IndirectWr_32(pif, TEMAC_REG_TIS, reg_val);
                                                                /* Restore the state of the IE reg                              */
    pdev->TEMAC_REG_IE =  int_en_reg;

   (void)&pdev;                                                 /* Prevent compilation warnings for variable set &  ...         */

   *perr = NET_PHY_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_RxDescInit()
*
* Description : (1) This function initializes the Rx descriptor list for the specified interface :
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Rx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
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
* Note(s)     : (2) Memory allocation for the descriptors MUST be performed BEFORE calling this
*                   function. This ensures that multiple calls to this function do NOT allocate
*                   additional memory to the interface and that the Rx descriptors may be safely
*                   re-initialized by calling this function.
*
*               (3) All Rx descriptors are allocated as ONE memory block.  This removes the
*                   necessity to ensure that descriptor blocks are returned to the descriptor
*                   pool in the opposite order in which they were allocated; doing so would
*                   ensure that each memory block address was contiguous to the one before
*                   and after it.  If the descriptors are NOT contiguous, then software
*                   MUST NOT assign a pointer to the pool start address and use pointer
*                   arithmetic to navigate the descriptor list.  Since pointer arithmetic
*                   is a convenient way to navigate the descriptor list, ONE block is allocated
*                   and the driver uses pointer arithmetic to slice the block into descriptor
*                   sized units.
*
*               (4) In RX channel case, if "EOP" flag set but its DMA_AppData4 has value
*                   DMA_USERAPP4_INIT_VAL, this means hardware has not completed updating the
*                   RX decriptor structure
*********************************************************************************************************
*/

static  void  NetDev_RxDescInit (NET_IF   *pif,
                                 NET_ERR  *perr)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_CFG_ETHER             *pdev_cfg;
    NET_DEV_DATA                  *pdev_data;
    DEV_DESC                      *pdesc;
    CPU_INT16U                     i;

                                                                /* ------------- OBTAIN REFERENCE TO DEVICE BSP --------------- */
    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;   /* Obtain ptr to the dev bsp struct                             */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                            */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA *)pif->Dev_Data;                  /* Obtain ptr to dev data area.                                 */
    pdesc     =  pdev_data->RxBufDescPtrStart;                  /* Obtain ptr to next ready desc.                               */

                                                                /* ------------------ INIT DESCRIPTOR PTRS  ------------------- */
    pdev_data->RxBufDescPtrCur  = (DEV_DESC *)pdesc;
    pdev_data->RxBufDescPtrEnd  = (DEV_DESC *)pdesc + (pdev_cfg->RxDescNbr - 1);
    pdev_data->RxBufDescPtrTail = pdev_data->RxBufDescPtrEnd;
                                                                /* -------------------- INIT RX DESCRIPTORS ------------------- */
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {
        pdesc->DMA_CurrBufLen    =  pdev_cfg->RxBufLargeSize;
        pdesc->DMA_CtrlAndStatus = ~DMA_MASK_BD_STSCTRL;
        pdesc->DMA_AppData1      =  0;
        pdesc->DMA_AppData2      =  0;
        pdesc->DMA_AppData3      =  0;
        pdesc->DMA_AppData4      =  DMA_USERAPP4_INIT_VAL;      /* See Note #4                                                  */
        pdesc->DMA_CurrBufAddr   = (CPU_REG32)NetBuf_GetDataPtr((NET_IF        *)pif,
                                                                (NET_TRANSACTION)NET_TRANSACTION_RX,
                                                                (NET_BUF_SIZE   )NET_IF_ETHER_FRAME_MAX_SIZE,
                                                                (NET_BUF_SIZE   )NET_IF_IX_RX,
                                                                (NET_BUF_SIZE  *)0,
                                                                (NET_BUF_SIZE  *)0,
                                                                (NET_BUF_TYPE  *)0,
                                                                (NET_ERR       *)perr);

        if (*perr != NET_BUF_ERR_NONE) {
             return;
        }

        if (pdesc == (pdev_data->RxBufDescPtrEnd)) {            /* Chain the last Descriptor in the list.                       */
            pdesc->DMA_NextDescAddr    = (CPU_REG32)pdev_data->RxBufDescPtrStart;
        } else {                                                /* Point to next descriptor in the list.                        */
            pdesc->DMA_NextDescAddr    = (CPU_REG32)(pdesc + 1);
        }
                                                                /* Flush the current desc. in case of data cache enabled        */
        CPU_CacheDataFlush(pdesc, sizeof(DEV_DESC));
        pdesc++;
    }
                                                                /* ----------------- INIT HARDWARE REGISTERS ------------------ */
                                                                /* Configure the DMA RX Channel Current Desc Register           */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_CDESC,
                    (CPU_INT32U)pdev_data->RxBufDescPtrStart);
                                                                /* Configure the DMA RX Channel TAIL Desc Register              */
                                                                /* This will allow the DMA RX channel to start                  */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_TDESC,
                    (CPU_INT32U)(pdev_data->RxBufDescPtrEnd));

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
* Note(s)     : (2) No mechanism exists to free a memory pool.  However, ALL receive buffers
*                   and the Rx descriptor blocks MUST be returned to their respective pools.
*********************************************************************************************************
*/

static  void  NetDev_RxDescFreeAll(NET_IF   *pif,
                                   NET_ERR  *perr)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV_DATA       *pdev_data;
    MEM_POOL           *pmem_pool;
    DEV_DESC           *pdesc;
    LIB_ERR             lib_err;
    CPU_INT16U          i;
    CPU_INT08U         *pdesc_data;


                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                            */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                                 */

                                                                /* ------------ OBTAIN REFERENCE TO RX DESC POOL -------------- */
    pmem_pool = (MEM_POOL          *)&pdev_data->RxDescPool;

                                                                /* ----------------- FREE RX DESC DATA AREAS ------------------ */
    pdesc     =  pdev_data->RxBufDescPtrStart;
    for (i = 0; i < pdev_cfg->RxDescNbr; i++) {                 /* Free Rx descriptor ring.                                     */
        pdesc_data = (CPU_INT08U *)(pdesc->DMA_CurrBufAddr);
        NetBuf_FreeBufDataAreaRx(pif->Nbr, pdesc_data);         /* Return data area to Rx data area pool.                       */
        pdesc++;
    }

                                                                /* -------------------- FREE RX DESC BLOCK -------------------- */
    Mem_PoolBlkFree( pmem_pool,                                 /* Return Rx descriptor block to Rx descriptor pool.            */
                     pdev_data->RxBufDescPtrStart,
                    &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *perr = NET_ERR_FAULT_MEM_ALLOC;
        return;
    }

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                      NetDev_RxDescPtrCurInc()
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

static  void  NetDev_RxDescPtrCurInc (NET_IF *pif)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_DATA                  *pdev_data;
    DEV_DESC                      *pdesc;
    NET_ERR                        err;


                                                                /* ----------- OBTAIN REFERENCE TO DEVICE BSP AREA ------------ */
    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;   /* Obtain ptr to dev bsp struct                                 */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                                 */
    pdesc     = pdev_data->RxBufDescPtrCur;


    pdesc->DMA_CtrlAndStatus = ~DMA_MASK_BD_STSCTRL;            /* Free the current desc. and flush cache memory if enabled     */
    pdesc->DMA_AppData4      =  DMA_USERAPP4_INIT_VAL;
    CPU_CacheDataFlush(pdesc, sizeof(DEV_DESC));

                                                                /* Update the DMA RX Channel tail register                      */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_TDESC,
   (CPU_INT32U)pdev_data->RxBufDescPtrCur);
    pdev_data->RxBufDescPtrTail = pdev_data->RxBufDescPtrCur;

                                                                /* Check if there are other completed RX frames                 */

    pdev_data->RxBufDescPtrCur  = (DEV_DESC *)pdesc->DMA_NextDescAddr;
    pdesc                       =  pdev_data->RxBufDescPtrCur;

    CPU_CacheDataInvalidate((CPU_VOID *)pdesc, sizeof(DEV_DESC));

                                                                /* Check if the desc marked as completed.                       */
    if ((pdesc->DMA_CtrlAndStatus & DMA_MASK_BD_STSCTRL_COMPLETED)) {

        NetIF_RxTaskSignal(pif->Nbr, &err);                     /* Signal Net IF RxQ Task for each new rdy Desc.                */
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
                                                                /* Re-enable DMA Rx Interrupt as it was disabled when the       */
                                                                /* previous DMA Rx int. occured. This is done to prevent        */
                                                                /* errors reading a descriptor which have been processed        */
        NetDev_DMA_EnDisChInt(pif, pdev_data->DMA_AddrRx, INT_ENABLE);
        NET_CTR_STAT_INC(pdev_data->StatRxCtrTaskSignalNone);
    }
}


/*
*********************************************************************************************************
*                                          NetDev_TxDescInit()
*
* Description : (1) This function initializes the Tx descriptor list for the specified interface :
*
*                   (a) Obtain reference to the Rx descriptor(s) memory block
*                   (b) Initialize Tx descriptor pointers
*                   (c) Obtain Rx descriptor data areas
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

static  void  NetDev_TxDescInit (NET_IF  *pif,
                                 NET_ERR *perr)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_CFG_ETHER             *pdev_cfg;
    NET_DEV_DATA                  *pdev_data;
    DEV_DESC                      *pdesc;
    CPU_INT16U                     i;

                                                                /* -------------- OBTAIN REFERENCE TO DEVICE BSP  ------------- */
    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;   /* Obtain ptr to the dev bsp struct.                            */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE REGISTERS ----------- */
    pdev_cfg  = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;              /* Obtain ptr to the dev cfg struct.                            */

                                                                /* ----------- OBTAIN REFERENCE TO DEVICE DATA AREA ----------- */
    pdev_data = (NET_DEV_DATA      *)pif->Dev_Data;             /* Obtain ptr to dev data area.                                 */

                                                                /* ------------------- INIT DESCRIPTOR PTRS  ------------------ */
    pdesc                       = (DEV_DESC *)pdev_data->TxBufDescPtrStart;
    pdev_data->TxBufDescCompPtr = (DEV_DESC *)pdev_data->TxBufDescPtrStart;
    pdev_data->TxBufDescPtrEnd  = (DEV_DESC *)pdesc + (pdev_cfg->TxDescNbr - 1);
    pdev_data->TxBufDescPtrCur  = (DEV_DESC *)pdev_data->TxBufDescPtrStart;

    for (i = 0; i < pdev_cfg->TxDescNbr; i++) {                 /* Init Tx desc ring.                                           */
        pdesc->DMA_CurrBufAddr    =  0;
        pdesc->DMA_CurrBufLen     =  0;
        pdesc->DMA_CtrlAndStatus  = ~DMA_MASK_BD_STSCTRL;
        pdesc->DMA_CtrlAndStatus |=  DMA_MASK_BD_STSCTRL_COMPLETED;
        pdesc->DMA_AppData1       =  0;
        pdesc->DMA_AppData2       =  0;
        pdesc->DMA_AppData3       =  0;
        pdesc->DMA_AppData4       =  0;
                                                                /* Chain the last Descriptor in the list                        */
        if (pdesc == (pdev_data->TxBufDescPtrEnd)) {

            pdesc->DMA_NextDescAddr    = (CPU_REG32)pdev_data->TxBufDescPtrStart;
        } else {
                                                                /* Point to next descriptor in the list.                        */
            pdesc->DMA_NextDescAddr    = (CPU_REG32)(pdesc + 1);
        }

        CPU_CacheDataFlush(pdesc, sizeof(DEV_DESC));

        pdesc++;

    }
                                                                /* ----------------- INIT HARDWARE REGISTERS ------------------ */
                                                                /* Configure the DMA TX Current Desc. Register with the Ring    */
                                                                /* start address.                                               */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_CDESC,
                     (CPU_INT32U)pdev_data->TxBufDescPtrStart);

    *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetDev_SetCoalesce()
*
* Description : Configure interrupt coalescing for the given DMA channel
*
* Argument(s) : pif             Pointer to the interface requiring service
*
*               dma_ch_addr     DMA Channel offset
*
*               cntr            The completed packet count before generating an interrupt
*                               Valid value 1 to 255 or DMA_NO_CHANGE to leave the counter value
*                               inchanged
*
*               timer_val       Dly before generating an interrupt if no activity was detected. This allow
*                               the software to read any pending completed descriptors especially if cntr
*                               (previous parameter) is greater than 1.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_DMA_Reset().
*
* Note(s)     : none
*********************************************************************************************************
*/

static void  NetDev_SetCoalesce (NET_IF      *pif,
                                 CPU_INT32U   dma_ch_addr,
                                 CPU_INT08U   cntr,
                                 CPU_INT08U   timer_val)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    CPU_INT32U                     dma_ctr_reg;


    pdev_bsp    = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;
    dma_ctr_reg = pdev_bsp->DMA_Rd32(dma_ch_addr + DMA_REG_CR);

    if (cntr != DMA_NO_CHANGE) {
        dma_ctr_reg &= ~DMA_MASK_CR_IRQ_COUNT;
        dma_ctr_reg |=  cntr << DMA_CR_IRQ_COUNT_SHIFT;
        dma_ctr_reg |=  DMA_MASK_CR_LD_IRQ_CNT;
    }

    if (timer_val != DMA_NO_CHANGE) {
        dma_ctr_reg &= ~DMA_MASK_CR_IRQ_TIMEOUT;
        dma_ctr_reg |=  timer_val << DMA_CR_IRQ_TIMEOUT_SHIFT;
        dma_ctr_reg |=  DMA_MASK_CR_LD_IRQ_CNT;
    }

    pdev_bsp->DMA_Wr32(dma_ch_addr + DMA_REG_CR, dma_ctr_reg);
}


/*
*********************************************************************************************************
*                                         NetDev_DMA_Reset()
*
* Description : Reset the DMA.
*
* Argument(s) : pif    Pointer to the interface requiring service
*
*               perr   Pointer to return error code
*
*                   NET_DEV_ERR_RST      code returned if DMA reset failed.
*                   NET_DEV_ERR_NONE     no error occured during the DMA reset
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_Init(),
*               NetDev_DMARecover().
*
* Note(s)     : none
*********************************************************************************************************
*/

static  void  NetDev_DMA_Reset (NET_IF   *pif,
                                NET_ERR  *perr)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_DATA                  *pdev_data;
    CPU_INT32U                     reg_val;
    CPU_INT32U                     timeout_cnt;


    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;
    pdev_data = (NET_DEV_DATA                 *)pif->Dev_Data;  /* Obtain ptr to the dev data area.                             */

                                                                /* Reset the DMA                                                */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrBase + DMA_GLB_CR_REG, DMA_MASK_CR_SW_RST);
    reg_val = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrBase + DMA_GLB_CR_REG);

                                                                /* wait for DMA reset completion                                */
    timeout_cnt = 2;
    while ((reg_val & DMA_MASK_CR_SW_RST) && timeout_cnt > 0) {
        KAL_Dly(2);
        reg_val = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrBase + DMA_GLB_CR_REG);
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
       *perr = NET_DEV_ERR_RST;
        return;
    }
                                                                /* ----------------- DISABLE ALL INTERRUPTS ------------------- */
                                                                /* Disable TX channel interrupts                                */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_CR);
    reg_val &= ~DMA_MASK_CR_IRQ_ALL_EN;
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_CR, reg_val);

                                                                /* Disable RX channel interrupts                                */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_CR);
    reg_val &= ~DMA_MASK_CR_IRQ_ALL_EN;
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_CR, reg_val);

                                                                /* Clear TX channel Interrupts register                         */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_IRQ);
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_IRQ, reg_val);

                                                                /* Clear RX channel Interrupts register                         */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_IRQ);
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_IRQ, reg_val);

                                                                /* Configure the DMA in normal mode and disable overflow errors */
                                                                /* for RX and TX  DMA Channels                                  */
    reg_val = DMA_MASK_CR_TAIL_PTR_EN      |
              DMA_MASK_CR_RX_OVFL_ERR_DIS  |
              DMA_MASK_CR_TX_OVFL_ERR_DIS;
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrBase + DMA_GLB_CR_REG, reg_val);

                                                                /* TX/RX Channel coalescing setting                             */
    NetDev_SetCoalesce(pif, pdev_data->DMA_AddrTx, 1, 1);
    NetDev_SetCoalesce(pif, pdev_data->DMA_AddrRx, 1, 1);

   *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                       NetDev_DMA_ErrRecover()
*
* Description : Recover from DMA error interrupts.
*
* Argument(s) : pif     Pointer to interface requiring service.
*
*               perr              Pointer to return error code
*
*                   NET_DEV_ERR_RST      Reset error code returned by NetDev_DMA_Reset()
*                   NET_DEV_ERR_NONE     no error occured during the DMA reset
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ISR_Handler().
*
* Note(s)     : none
*********************************************************************************************************
*/

static  void  NetDev_DMA_ErrRecover (NET_IF   *pif,
                                     NET_ERR  *perr)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    NET_DEV_DATA                  *pdev_data;
    CPU_INT32U                     reg_val;
    DEV_DESC                      *pdesc;
    DEV_DESC                      *pdesc_tmp;
    CPU_INT08U                    *p_data;


    NetDev_DMA_Reset(pif, perr);                                /* Reset the DMA and disable DMA interrupts                     */
    if (*perr != NET_DEV_ERR_NONE) {
         return;
    }

    pdev_bsp  = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;
    pdev_data = (NET_DEV_DATA                 *)pif->Dev_Data;
    pdesc     = (DEV_DESC                     *)pdev_data->RxBufDescPtrCur;

                                                                /* Check if there are valid completed RX descriptors            */
    CPU_CacheDataInvalidate((CPU_VOID *)pdesc, sizeof(DEV_DESC));
    while ((pdesc->DMA_CtrlAndStatus & DMA_MASK_BD_STSCTRL_COMPLETED)) {
        pdesc = (DEV_DESC*)pdesc->DMA_NextDescAddr;
        CPU_CacheDataInvalidate((CPU_VOID *)pdesc, sizeof(DEV_DESC));

        if (pdesc == pdev_data->RxBufDescPtrTail) {
            break;
        }
    }
                                                                /* Re-initialiaze the RX descriptors located between the last   */
                                                                /* completed and the tail descriptor                            */
    pdesc_tmp = pdesc;
    while (pdesc_tmp != pdev_data->RxBufDescPtrCur) {
        if (pdesc_tmp->DMA_CtrlAndStatus & DMA_MASK_BD_STSCTRL_COMPLETED) {
            pdesc_tmp->DMA_CtrlAndStatus = ~DMA_MASK_BD_STSCTRL;
            pdesc_tmp->DMA_AppData1      =  0;
            pdesc_tmp->DMA_AppData2      =  0;
            pdesc_tmp->DMA_AppData3      =  0;
            pdesc_tmp->DMA_AppData4      =  DMA_USERAPP4_INIT_VAL;
            CPU_CacheDataFlush(pdesc_tmp, sizeof(DEV_DESC));
        }
        pdesc_tmp = (DEV_DESC*)pdesc_tmp->DMA_NextDescAddr;
    }
                                                                /* Configure the DMA RX Channel Current Desc Register           */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_CDESC,
                    (CPU_INT32U)pdesc);
                                                                /* Configure the DMA RX Channel tail Desc Register              */
                                                                /* This will allow the DMA RX channel to start                  */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_TDESC,
                    (CPU_INT32U)pdev_data->RxBufDescPtrTail);
                                                                /* Configure the DMA TX Current Desc. Register with the current */
                                                                /* TX descriptor address.                                       */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_CDESC,
                     (CPU_INT32U)pdev_data->TxBufDescPtrCur);
                                                                /* The DMA TX channel is restarted, so we need to synchronize   */
                                                                /* TxBufDescCompPtr and give back the data buffer to the stack  */
    pdesc                     =  pdev_data->TxBufDescCompPtr;
    pdesc->DMA_CtrlAndStatus  = ~DMA_MASK_BD_STSCTRL;
    pdesc->DMA_CtrlAndStatus |=  DMA_MASK_BD_STSCTRL_COMPLETED;
    p_data                    = (CPU_INT08U *)pdesc->DMA_CurrBufAddr;
    NetIF_TxDeallocTaskPost(p_data, perr);
                                                                /* The DMA TX channel is restarted so we signal Net IF          */
                                                                /* that Tx resources are available.                             */
    NetIF_DevTxRdySignal(pif);
    pdev_data->TxBufDescCompPtr = (DEV_DESC *)pdev_data->TxBufDescCompPtr->DMA_NextDescAddr;

                                                                /* ---------------- ENABLE DMA INTERRUPTS --------------------- */
                                                                /* Enable DMA RX interrupt.                                     */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrRx + DMA_REG_CR);
    reg_val |= DMA_MASK_CR_IRQ_EN           |                   /* Master interrupt enable                                      */
               DMA_MASK_CR_IRQ_ERR_EN       |                   /* Enable error interrupt                                       */
               DMA_MASK_CR_IRQ_DLY_EN       |                   /* Enable coalesce delay interrupt                              */
               DMA_MASK_CR_IRQ_COALESCE_EN;                     /* Enable coalesce count                                        */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrRx + DMA_REG_CR, reg_val);

                                                                /* Enable DMA TX interrupt.                                     */
    reg_val  = pdev_bsp->DMA_Rd32(pdev_data->DMA_AddrTx + DMA_REG_CR);
    reg_val |= DMA_MASK_CR_IRQ_EN           |                   /* Master interrupt enable                                      */
               DMA_MASK_CR_IRQ_ERR_EN       |                   /* Enable error interrupt                                       */
               DMA_MASK_CR_IRQ_DLY_EN       |                   /* Enable coalesce delay interrupt                              */
               DMA_MASK_CR_IRQ_COALESCE_EN;                     /* Enable coalesce count                                        */
    pdev_bsp->DMA_Wr32(pdev_data->DMA_AddrTx + DMA_REG_CR, reg_val);

    *perr = NET_DEV_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetDev_DMA_EnDisChanInt()
*
* Description : Enable or disable a DMA channel interrupts
*
* Argument(s) : dma_ch_addr     DMA Channel address
*
*               en              indicates if interrupts should be enabled or disabled
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_ISR_Handler(),
*               NetDev_RxDescPtrCurInc().
*
* Note(s)     : none
*********************************************************************************************************
*/

static  void  NetDev_DMA_EnDisChInt   (NET_IF      *pif,
                                       CPU_INT32U   dma_ch_addr,
                                       CPU_INT08U   en)
{
    NET_DEV_BSP_XPS_LL_TEMAC_DMA  *pdev_bsp;
    CPU_INT32U                     reg_val;


    pdev_bsp = (NET_DEV_BSP_XPS_LL_TEMAC_DMA *)pif->Dev_BSP;
    reg_val  =  pdev_bsp->DMA_Rd32(dma_ch_addr + DMA_REG_CR);
    if (en > 0) {
        reg_val |=  DMA_MASK_CR_IRQ_EN;                         /* Master interrupt enable                                      */
    } else {
        reg_val &= ~DMA_MASK_CR_IRQ_EN;                         /* Master interrupt disable                                     */
    }

    pdev_bsp->DMA_Wr32(dma_ch_addr + DMA_REG_CR, reg_val);
}


/*
*********************************************************************************************************
*                                       NetDev_AddrMulticastGet()
*
* Description : Get the Multicast address stored at a given entry in the multicast table
*
* Argument(s) : pif         Pointer to an Ethernet network interface.
*               ---         Argument validated in NetIF_AddrHW_SetHandler().
*
*               paddr_hw    Pointer to hardware address.
*               --------    Argument checked   in NetIF_AddrHW_SetHandler().
*
*               entry       Multicast table entry from where the address will be read
*
*               perr        Pointer to variable that will receive the return error code from this function :
*
*                               NET_DEV_ERR_NONE                Hardware address successfully returned.
*
*                               NET_DEV_MCAST_TAB_ERR_RD        Error during Multicast table entry read.
*
* Return(s)   : none.
*
* Caller(s)   : NetDev_AddrMulticastAdd(),
                NetDev_AddrMulticastRemove().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#ifdef NET_MCAST_MODULE_EN
static  void  NetDev_AddrMulticastGet (NET_IF      *pif,
                                       CPU_INT08U  *paddr_hw,
                                       CPU_INT32U   entry,
                                       NET_ERR     *perr)
{
    NET_DEV            *pdev;
    NET_DEV_CFG_ETHER  *pdev_cfg;
    CPU_INT32U          reg_val1;
    CPU_INT32U          reg_val2;
    CPU_INT32U          rdy_reg_val;
    CPU_INT16U          timeout_cnt;


                                                                /* ------------ OBTAIN REFERENCE TO DEVICE REGISTERS ---------- */
    pdev_cfg = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;               /* Obtain ptr to the dev cfg struct.                            */
    pdev     = (NET_DEV           *)pdev_cfg->BaseAddr;         /* Overlay dev reg struct on top of dev base addr.              */


                                                                /* Prepare for read the Muticast entry table                    */
    reg_val2 = (TEMAC_MASK_MCAST_RNW | (entry << TEMAC_MAW1_ADDR_SHIFT));
    NetDev_IndirectWr_32(pif, TEMAC_REG_MAW1, reg_val2);

                                                                /* Read the hw address from TEMAC_REG_LSW and TEMAC_REG_MSW     */
    rdy_reg_val = pdev->TEMAC_REG_RDY;
    timeout_cnt = TEMAC_INDIRECT_REG_ACCESS_TO_CNT;

    while (!(rdy_reg_val & TEMAC_MASK_RDY_AF_RR) && timeout_cnt > 0) {
        KAL_Dly(TEMAC_INDIRECT_REG_ACCESS_DLY_MS);
        rdy_reg_val = pdev->TEMAC_REG_RDY;
        timeout_cnt--;
    }

    if (timeout_cnt == 0) {
        *perr   = NET_DEV_MCAST_TAB_ERR_RD;
        return;
    }
                                                                /* After requisting a read to the Multicast table, data should  */
                                                                /* be available from the LSW and MSW registers                  */
    reg_val1    = pdev->TEMAC_REG_LSW;
    reg_val2    = pdev->TEMAC_REG_MSW;

    paddr_hw[0] = (CPU_INT08U)(reg_val1);
    paddr_hw[1] = (CPU_INT08U)(reg_val1 >> 8);
    paddr_hw[2] = (CPU_INT08U)(reg_val1 >> 16);
    paddr_hw[3] = (CPU_INT08U)(reg_val1 >> 24);
    paddr_hw[4] = (CPU_INT08U)(reg_val2);
    paddr_hw[5] = (CPU_INT08U)(reg_val2 >> 8);


    *perr = NET_DEV_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       NetDev_IndirectWr_32()
*
* Description : Write TEMAC register which is indirectly accessed.
*
* Argument(s) : pif          Pointer to the interface requiring service.
*
*               reg_offset   Offset of the register to write to.
*
*               val          Value to write to register.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none
*********************************************************************************************************
*/

static  void  NetDev_IndirectWr_32 (NET_IF      *pif,
                                    CPU_INT32U   reg_offset,
                                    CPU_INT32U   val)
{

    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;

                                                                    /* --------  OBTAIN REFERENCE TO DEVICE REGISTERS --------- */
    pdev_cfg            = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;        /* Obtain ptr to the dev cfg struct                         */
    pdev                = (NET_DEV           *)pdev_cfg->BaseAddr;  /* Overlay dev reg struct on top of dev base addr.          */

    pdev->TEMAC_REG_LSW =  val;
    pdev->TEMAC_REG_CTL =  reg_offset | TEMAC_MASK_CTL_WEN;
}


/*
*********************************************************************************************************
*                                       NetDev_IndirectRd_32()
*
* Description : Read TEMAC register which is indirectly accessed.
*
* Argument(s) : pif          Pointer to the interface requiring service.
*
*               reg_offset   Offset of the register to read from.
*
* Return(s)   : Value read from register.
*
* Caller(s)   : various.
*
* Note(s)     : none
*********************************************************************************************************
*/

static  CPU_INT32U  NetDev_IndirectRd_32 (NET_IF      *pif,
                                          CPU_INT32U   reg_offset)
{
    NET_DEV_CFG_ETHER  *pdev_cfg;
    NET_DEV            *pdev;
    CPU_INT32U          reg_val;

                                                                    /* --------  OBTAIN REFERENCE TO DEVICE REGISTERS --------- */
    pdev_cfg            = (NET_DEV_CFG_ETHER *)pif->Dev_Cfg;        /* Obtain ptr to the dev cfg struct                         */
    pdev                = (NET_DEV           *)pdev_cfg->BaseAddr;  /* Overlay dev reg struct on top of dev base addr.          */

    pdev->TEMAC_REG_CTL =  reg_offset;
    reg_val             =  pdev->TEMAC_REG_LSW;

    return (reg_val);
}


#endif /* NET_IF_ETHER_MODULE_EN */

