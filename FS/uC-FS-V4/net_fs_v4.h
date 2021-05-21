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
*                                   NETWORK FILE SYSTEM PORT LAYER
*
*                                             uC/FS V4.xx
*
* Filename : net_fs_v4.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) uC/FS  V4.07.04
*                (b) uC/Clk V3.09.04       if uC/FS V4.04 (or more recent) is included
*
*            (2) Should be compatible with the following TCP/IP application versions (or more recent):
*
*                (a) uC/FTPc  V1.93.01
*                (b) uC/FTPs  V1.95.02
*                (c) uC/HTTPs V1.98.00
*                (d) uC/TFTPc V1.92.01
*                (e) uC/TFTPs V1.91.02
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*/

#ifndef  NET_FS_V4_MODULE_PRESENT
#define  NET_FS_V4_MODULE_PRESENT


/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  "../net_fs.h"

#include  <Source/clk.h>
#include  <Source/fs.h>
#include  <Source/fs_api.h>
#include  <Source/fs_dev.h>
#include  <Source/fs_dir.h>
#include  <Source/fs_entry.h>
#include  <Source/fs_file.h>
#include  <Source/fs_partition.h>
#include  <Source/fs_vol.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  NET_FS_FILE_BUF_EN     DEF_ENABLED


/*
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*/

extern  const  NET_FS_API  NetFS_API_FS_V4;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*/

#if (FS_CFG_WORKING_DIR_EN != DEF_ENABLED )
    #error  "FS_CFG_WORKING_DIR_EN      illegally #define'd in 'fs_cfg.h'"
    #error  "                           [MUST be  DEF_ENABLED]           "
#endif


                                                                /* ---------------- NET_FS_FILE_BUF_EN ---------------- */
#ifndef  NET_FS_FILE_BUF_EN
#error  "NET_FS_FILE_BUF_EN                           not #define'd in 'net_fs_v4.h'            "
#error  "                                       [MUST be  DEF_DISABLED]                         "
#error  "                                       [     ||  DEF_ENABLED ]                         "

#elif  ((NET_FS_FILE_BUF_EN != DEF_DISABLED) && \
        (NET_FS_FILE_BUF_EN != DEF_ENABLED ))
#error  "NET_FS_FILE_BUF_EN                     illegally #define'd in 'net_fs_v4.h'            "
#error  "                                       [MUST be  DEF_DISABLED]                         "
#error  "                                       [     ||  DEF_ENABLED ]                         "
#endif


/*
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*/

#endif                                                          /* End of net fs V4 module include.                     */


