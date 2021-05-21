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
*                                     NETWORK SECURITY PORT LAYER
*
*                                            SEGGER emSSL
*
* Filename : net_secure_emssl_cfg.h
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes the following versions (or more recent) of software modules are included in
*                the project build :
*
*                (a) SEGGER emSSL V2.54a
*                (b) uC/Clk V3.09
*
*                See also 'net.h  Note #1'.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  NET_SECURE_EMSSL_CFG_MODULE_PRESENT
#define  NET_SECURE_EMSSL_CFG_MODULE_PRESENT

#ifdef   NET_SECURE_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define EMSSL_CFG_MIN_FOOTPRINT_EN             DEF_ENABLED      /* Minimal configuration.                              */
                                                                /* Install DHE cipher suite. Key agreement is slow ... */
#define EMSSL_CFG_DHE_RSA_CIPHER_SUITE         DEF_DISABLED     /* ... so it should be left disabled by default.       */
#define EMSSL_CFG_ECDHE_ECDSA_CIPHER_SUITE_EN  DEF_ENABLED      /* Install ECDHE-ECDSA cipher suites.                  */
#define EMSSL_CFG_ECDHE_RSA_CIPHER_SUITE_EN    DEF_ENABLED      /* Install ECDHE-RSA cipher suites.                    */
#define EMSSL_CFG_ECDH_ECDSA_CIPHER_SUITE_EN   DEF_ENABLED      /* Install ECDH-ECDSA cipher suites.                   */
#define EMSSL_CFG_ECDH_RSA_CIPHER_SUITE_EN     DEF_ENABLED      /* Install ECDH-RSA cipher suites.                     */
#define EMSSL_CFG_RSA_CIPHER_SUITE_EN          DEF_ENABLED      /* Install static RSA cipher suites.                   */
                                                                /* Install insecure NULL cipher suites for debugging...*/
#define EMSSL_CFG_NULL_CIPHER_SUITE_EN         DEF_DISABLED     /* ...purposes. No block encipherment.                 */

#if (NET_SECURE_CFG_HW_CRYPTO_EN == DEF_ENABLED)
#include  <net_secure_emssl_hw.h>
#endif

#endif
#endif