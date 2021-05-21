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
*                                         NETWORK HEADER FILE
*
* Filename : net.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) uC/CPU V1.30
*                (b) uC/LIB V1.38.00
*
*                See also 'NETWORK INCLUDE FILES  Notes #2 & #3'.
*
*
*            (2) (a) The following network protocols are supported/implemented :
*
*                                                                        ---- LINK LAYER PROTOCOLS -----
*                    (1) (A) ARP     Address Resolution Protocol
*                                                                        --- NETWORK LAYER PROTOCOLS ---
*                    (2) (A) IPv4    Internet Protocol                    Version 4
*                        (B) IPv6    Internet Protocol                    Version 6
*                        (C) ICMPv4  Internet Control Message    Protocol Version 4
*                        (D) ICMPv6  Internet Control Message    Protocol Version 6
*                        (E) IGMP    Internet Group   Management Protocol
*                                                                        -- TRANSPORT LAYER PROTOCOLS --
*                    (3) (A) UDP     User Datagram Protocol
*                        (B) TCP     Transmission Control Protocol
*
*                (b) The following network protocols are intentionally NOT supported/implemented :
*
*                                                                        ---- LINK LAYER PROTOCOLS -----
*                    (1) (A) RARP    Reverse Address Resolution Protocol
*
*
*            (3) To protect the validity & prevent the corruption of shared network protocol resources,
*                the primary tasks of the network protocol suite are prevented from running concurrently
*                through the use of a global network lock implementing protection by mutual exclusion.
*
*                (a) The mechanism of protected mutual exclusion is irrelevant but MUST be implemented
*                    in the following two functions :
*
*                        Net_GlobalLockAcquire()   acquire access to network protocol suite
*                        Net_GlobalLockRelease()   release access to network protocol suite
*
*                (b) Since this global lock implements mutual exclusion at the network protocol suite
*                    task level, critical sections are NOT required to prevent task-level concurrency
*                    in the network protocol suite.
*
*            (4) To help debugging modules some value can be defined for internal usage:
*
*                (a) To configure the initial value of sequence numbers the following define should be
*                    added to net_cfg.h:
*
*                      #define  NET_UTIL_INIT_SEQ_NBR_0        value
*
*                (b) To remove the increment value to the TCP initial sequence number the following
*                    define should be added to net_cfg.h (See net_tcp.h Note #3):
*
*                      #define  NET_TCP_CFG_RANDOM_ISN_GEN
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This main network protocol suite header file is protected from multiple pre-processor
*               inclusion through use of the network module present pre-processor macro definition.
*
*               See also 'NETWORK INCLUDE FILES  Note #5'.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_MODULE_PRESENT                                     /* See Note #1.                                         */
#define  NET_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       NETWORK VERSION NUMBER
*
* Note(s) : (1) (a) The network protocol suite software version is denoted as follows :
*
*                       Vx.yy.zz
*
*                           where
*                                   V               denotes 'Version' label
*                                   x               denotes     major software version revision number
*                                   yy              denotes     minor software version revision number
*                                   zz              denotes sub-minor software version revision number
*
*               (b) The software version label #define is formatted as follows :
*
*                       ver = x.yyzz * 100 * 100
*
*                           where
*                                   ver             denotes software version number scaled as an integer value
*                                   x.yyzz          denotes software version number, where the unscaled integer
*                                                       portion denotes the major version number & the unscaled
*                                                       fractional portion denotes the (concatenated) minor
*                                                       version numbers
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_VERSION                                   30601u   /* See Note #1.                                         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        NETWORK INCLUDE FILES
*
* Note(s) : (1) The network protocol suite files are located in the following directories :
*
*               (a) (1) \<Your Product Application>\app_cfg.h
*                   (2)                            \net_cfg.h
*                   (3)                            \net_dev_cfg.*
*                   (4)                            \net_bsp.*
*
*               (b) \<Network Protocol Suite>\Source\net.h
*                                                   \net_*.*
*
*               (c) \<Network Protocol Suite>\Ports\<cpu>\<compiler>\net_*_a.*
*
*               (d) (1) \<Network Protocol Suite>\IF\net_if.*
*                   (2)                             \net_if_*.*
*
*               (e) \<Network Protocol Suite>\Dev\<if>\<dev>\net_dev_*.*
*
*               (f) (1)     \<Network Protocol Suite>\Secure\net_secure_*.*
*                   (2) (A) \<Network Protocol Suite>\Secure\<secure>\net_secure.*
*                       (B)                                 \<secure>\<os>\net_secure_os.*
*
*                       where
*                               <Your Product Application>      directory path for Your Product's Application
*                               <Network Protocol Suite>        directory path for network protocol suite
*                               <cpu>                           directory name for specific processor         (CPU)
*                               <compiler>                      directory name for specific compiler
*                               <os>                            directory name for specific operating system  (OS)
*                               <if>                            directory name for specific network interface (IF)
*                               <dev>                           directory name for specific network device
*                               <secure>                        directory name for specific security layer
*
*           (2) CPU-configuration software files are located in the following directories :
*
*               (a) \<CPU-Compiler Directory>\cpu_*.*
*
*               (b) \<CPU-Compiler Directory>\<cpu>\<compiler>\cpu*.*
*
*                       where
*                               <CPU-Compiler Directory>        directory path for common CPU-compiler software
*                               <cpu>                           directory name for specific processor (CPU)
*                               <compiler>                      directory name for specific compiler
*
*           (3) NO compiler-supplied standard library functions are used by the network protocol suite.
*
*               (a) Standard library functions are implemented in the custom library module(s) :
*
*                       \<Custom Library Directory>\lib_*.*
*
*                           where
*                                   <Custom Library Directory>      directory path for custom library software
*
*               (b) Network-specific library functions are implemented in the Network Utility module,
*                   'net_util.*' (see 'net_util.h  Note #1').
*
*           (4) (a) Compiler MUST be configured to include as additional include path directories :
*
*                   (1) '\<Your Product Application>\' directory                                See Note #1a
*
*                   (3) '\<Custom Library Directory>\' directory                                See Note #3a
*
*                   (4) Specific port directories :
*
*                       (A) (1) '\<CPU-Compiler Directory>\'                    directory       See Note #2a
*                           (2) '\<CPU-Compiler Directory>\<cpu>\<compiler>\'   directory       See Note #2b
*
*                       (B) '\<Network Protocol Suite>\Ports\<cpu>\<compiler>\' directory       See Note #1c
*
*                       where
*                               <Your Product Application>      directory path for Your Product's Application
*                               <Network Protocol Suite>        directory path for network protocol suite
*                               <Custom Library Directory>      directory path for custom  library software
*                               <CPU-Compiler Directory>        directory path for common  CPU-compiler software
*                               <cpu>                           directory name for specific processor         (CPU)
*                               <compiler>                      directory name for specific compiler
*                               <if>                            directory name for specific network interface (IF)
*                               <dev>                           directory name for specific network device
*********************************************************************************************************
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#include  "net_err.h"
#include  "net_type.h"



#include  <cpu.h>

#include  <lib_def.h>
#include  <lib_cfg.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef NET_MODULE
#define  NET_EXT
#else
#define  NET_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  NET_TASK_NBR_IF                                   2u
#define  NET_TASK_NBR_TMR                                  1u

#define  NET_TASK_NBR                                   (NET_TASK_NBR_IF + \
                                                         NET_TASK_NBR_TMR)


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

NET_EXT  CPU_BOOLEAN  Net_InitDone;                             /* Indicates when network initialization is complete.   */

extern   CPU_INT32U   Net_Version;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                     EXTERNAL C LANGUAGE LINKAGE
*
* Note(s) : (1) C++ compilers MUST 'extern'ally declare ALL C function prototypes & variable/object
*               declarations for correct C language linkage.
*********************************************************************************************************
*/

#ifdef  __cplusplus
extern "C" {
#endif


/*
*********************************************************************************************************
*                                             PUBLIC API
*********************************************************************************************************
*/

NET_ERR     Net_Init      (const  NET_TASK_CFG  *rx_task_cfg,   /* Network startup function.                            */
                           const  NET_TASK_CFG  *tx_task_cfg,
                           const  NET_TASK_CFG  *tmr_task_cfg);

CPU_INT16U  Net_VersionGet(       void);                        /* Get network protocol suite software version.         */

void        Net_TimeDly   (       CPU_INT32U     time_dly_sec,  /* Time delay of seconds & microseconds.                */
                                  CPU_INT32U     time_dly_us,
                                  NET_ERR       *p_err);

void        Net_InitDflt         (void);                        /* Initialize default values for configurable parameters.*/


/*
*********************************************************************************************************
*                                         INTERNAL FUNCTIONS
*********************************************************************************************************
*/

void        Net_InitCompWait     (NET_ERR       *p_err);        /* Wait  until network initialization is complete.      */

void        Net_InitCompSignal   (NET_ERR       *p_err);        /* Signal that network initialization is complete.      */

void        Net_GlobalLockAcquire(void          *p_fcnt,
                                  NET_ERR       *p_err);        /* Acquire access to network protocol suite.            */

void        Net_GlobalLockRelease(void);                        /* Release access to network protocol suite.            */

/*
*********************************************************************************************************
*                                   EXTERNAL C LANGUAGE LINKAGE END
*********************************************************************************************************
*/

#ifdef  __cplusplus
}
#endif

/*
*********************************************************************************************************
*********************************************************************************************************
*                                    NETWORK CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/


#ifndef  NET_CFG_OPTIMIZE_ASM_EN
#error  "NET_CFG_OPTIMIZE_ASM_EN        not #define'd in 'net_cfg.h'"
#error  "                         [MUST be  DEF_DISABLED]           "
#error  "                         [     ||  DEF_ENABLED ]           "

#elif  ((NET_CFG_OPTIMIZE_ASM_EN != DEF_DISABLED) && \
        (NET_CFG_OPTIMIZE_ASM_EN != DEF_ENABLED ))
#error  "NET_CFG_OPTIMIZE_ASM_EN  illegally #define'd in 'net_cfg.h'"
#error  "                         [MUST be  DEF_DISABLED]           "
#error  "                         [     ||  DEF_ENABLED ]           "
#endif


/*
*********************************************************************************************************
*                                    LIBRARY CONFIGURATION ERRORS
*********************************************************************************************************
*/

                                                                /* See 'net.h  Note #1a'.                               */
#if     (CPU_CORE_VERSION < 13000u)
#error  "CPU_CORE_VERSION      [SHOULD be >= V1.30]"
#endif


                                                                /* See 'net.h  Note #1b'.                               */
#if     (LIB_VERSION < 13800u)
#error  "LIB_VERSION           [SHOULD be >= V1.38.00]"
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* NET_MODULE_PRESENT */

