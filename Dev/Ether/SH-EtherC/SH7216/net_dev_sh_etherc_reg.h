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
*                                           Renesas SH 7216
*
* Filename : net_dev_sh_etherc_reg.h
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>


/*
*********************************************************************************************************
*                                        REGISTER DEFINITIONS
*
* Note(s) : (1) The device register definition structure MUST take into account appropriate
*               register offsets and apply reserved space as required.  The registers listed
*               within the register definition structure MUST reflect the exact ordering and
*               data sizes illustrated in the device user guide. An example device register
*               structure is provided below.
*
*           (2) The device register definition structure is mapped over the corresponding base
*               address provided within the device configuration structure.  The device
*               configuration structure is provided by the application developer within
*               net_dev_cfg.c.  Refer to the Network Protocol Suite User Manual for more
*               information related to declaring device configuration structures.
*********************************************************************************************************
*/

typedef  struct  net_dev {
    CPU_REG32  EDMAC_EDMR;
    CPU_REG32  RESERVED0[0x01];
    CPU_REG32  EDMAC_EDTRR;
    CPU_REG32  RESERVED1[0x01];
    CPU_REG32  EDMAC_EDRRR;
    CPU_REG32  RESERVED2[0x01];
    CPU_REG32  EDMAC_TDLAR;
    CPU_REG32  RESERVED3[0x01];
    CPU_REG32  EDMAC_RDLAR;
    CPU_REG32  RESERVED4[0x01];
    CPU_REG32  EDMAC_EESR;
    CPU_REG32  RESERVED5[0x01];
    CPU_REG32  EDMAC_EESIPR;
    CPU_REG32  RESERVED6[0x01];
    CPU_REG32  EDMAC_TRSCER;
    CPU_REG32  RESERVED7[0x01];
    CPU_REG32  EDMAC_RMFCR;
    CPU_REG32  RESERVED8[0x01];
    CPU_REG32  EDMAC_TFTR;
    CPU_REG32  RESERVED9[0x01];
    CPU_REG32  EDMAC_FDR;
    CPU_REG32  RESERVED10[0x01];
    CPU_REG32  EDMAC_RMCR;
    CPU_REG32  RESERVED11[0x02];
    CPU_REG32  EDMAC_TFUCR;
    CPU_REG32  EDMAC_RFOCR;
    CPU_REG32  EDMAC_IOSR;
    CPU_REG32  EDMAC_FCFTR;
    CPU_REG32  RESERVED12[0x02];
    CPU_REG32  EDMAC_TRIMD;
    CPU_REG32  RESERVED13[0x12];
    CPU_REG32  EDMAC_RBWAR;
    CPU_REG32  EDMAC_RDFAR;
    CPU_REG32  RESERVED14[0x01];
    CPU_REG32  EDMAC_TBRAR;
    CPU_REG32  EDMAC_TDFAR;
    CPU_REG32  RESERVED15[0x02];
    CPU_REG32  EDMAC_EDOCR;
    CPU_REG32  RESERVED16[0x06];
    CPU_REG32  ETHERC_ECMR;
    CPU_REG32  RESERVED17[0x01];
    CPU_REG32  ETHERC_RFLR;
    CPU_REG32  RESERVED18[0x01];
    CPU_REG32  ETHERC_ECSR;
    CPU_REG32  RESERVED19[0x01];
    CPU_REG32  ETHERC_ECSIPR;
    CPU_REG32  RESERVED20[0x01];
    CPU_REG32  ETHERC_PIR;
    CPU_REG32  RESERVED21[0x01];
    CPU_REG32  ETHERC_PSR;
    CPU_REG32  RESERVED22[0x05];
    CPU_REG32  ETHERC_RDMLR;
    CPU_REG32  RESERVED23[0x03];
    CPU_REG32  ETHERC_IGPR;
    CPU_REG32  ETHERC_APR;
    CPU_REG32  ETHERC_MPR;
    CPU_REG32  RESERVED24[0x01];
    CPU_REG32  ETHERC_RFCF;
    CPU_REG32  ETHERC_TPAUSER;
    CPU_REG32  ETHERC_TPAUSECR;
    CPU_REG32  ETHERC_BCFRR;
    CPU_REG32  RESERVED25[0x14];
    CPU_REG32  ETHERC_MAHR;
    CPU_REG32  RESERVED26[0x01];
    CPU_REG32  ETHERC_MALR;
    CPU_REG32  RESERVED27[0x01];
    CPU_REG32  ETHERC_TROCR;
    CPU_REG32  ETHERC_CDCR;
    CPU_REG32  ETHERC_LCCR;
    CPU_REG32  ETHERC_CNDCR;
    CPU_REG32  RESERVED28[0x01];
    CPU_REG32  ETHERC_CEFCR;
    CPU_REG32  ETHERC_FRECR;
    CPU_REG32  ETHERC_TSFRCR;
    CPU_REG32  ETHERC_TLFRCR;
    CPU_REG32  ETHERC_RFCR;
    CPU_REG32  ETHERC_MAFCR;
} NET_DEV;

