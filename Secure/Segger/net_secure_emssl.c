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
* Filename : net_secure_emssl.c
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
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_SECURE_MODULE

#ifndef NET_SECURE_EMSSL_HW_MODULE_PRESENT
#include  <lib_math.h>
#endif

#include  "net_secure_emssl.h"
#include  "../net_secure.h"
#include  "../../Source/net_cfg_net.h"
#include  <net_secure_emssl_cfg.h>
#include  "../../Source/net_bsd.h"
#include  "../../Source/net_conn.h"
#include  "../../Source/net_tcp.h"
#include  <lib_str.h>
#include  <KAL/kal.h>
#include  <Source/clk.h>
#include  <lib_mem.h>
#include  <lib_str.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef   NET_SECURE_MODULE_EN
#define  NET_SECURE_EMSSL_MODULE


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Calculate RAM usage as recommended by SEGGER manual. */
#define  NET_SECURE_SSL_CONN_NBR_MAX          (NET_SECURE_CFG_MAX_NBR_SOCK_SERVER + NET_SECURE_CFG_MAX_NBR_SOCK_CLIENT)
#define  NET_SECURE_SSL_MEM_SIZE              (((700u + 500u) + (2u * 16u * 1024u)) * NET_SECURE_SSL_CONN_NBR_MAX + 512u)
#define  NET_SECURE_SSL_MIN_MEM_SIZE          (34u * 1024u)


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static            int   NetSecure_emSSL_Rx    (       int            sock_id,
                                                      char          *p_data_buf,
                                                      int            data_buf_len,
                                                      int            flags);

static            int   NetSecure_emSSL_Tx    (       int            sock_id,
                                               const  char          *p_data,
                                                      int            data_len,
                                                      int            flags);

static            int   NetSecure_GetCert     (       SSL_SESSION   *p_session,
                                                      CPU_INT32U     index,
                                               const  CPU_INT08U   **p_data,
                                                      CPU_INT32U    *p_data_len);

static            int   NetSecure_GetPrivKey  (       SSL_SESSION   *p_session,
                                               const  CPU_INT08U   **p_data,
                                                      CPU_INT32U    *p_data_len);

static  unsigned  long  NetSecure_GetClk      (void);

#ifndef  NET_SECURE_EMSSL_HW_MODULE_PRESENT
static            void  NetSecure_GetRandomNbr(       CPU_INT08U    *p_nbr,
                                                      CPU_INT32U     data_len);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

extern         CPU_CHAR            *Mem_Heap;

static         KAL_SEM_HANDLE       emSSL_SessionLockHandle;
static         KAL_SEM_HANDLE       emSSL_HWCryptoSemHandle;
static         CPU_SIZE_T          *SSL_StoreMemPtr;
static         MEM_DYN_POOL         NetSecure_SessionPool;
static         MEM_DYN_POOL         NetSecure_ServerDescPool;
static         MEM_DYN_POOL         NetSecure_ClientDescPool;


static  const  SSL_TRANSPORT_API    Micrium_TCP_Transport = {
    NetSecure_emSSL_Tx,
    NetSecure_emSSL_Rx,
    NetSecure_GetClk
};

static  const  SSL_CERTIFICATE_API  Micrium_CertAPI = {
    DEF_NULL,
    NetSecure_GetCert,
    NetSecure_GetPrivKey,
};

#ifndef  NET_SECURE_EMSSL_HW_MODULE_PRESENT
static  const  CRYPTO_RNG_API       NetSecure_LibMemRand_API = {
    0,
    NetSecure_GetRandomNbr,
    0,
    0,
};
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

typedef  struct  emssl_data {
#if (EMSSL_MUTUAL_AUTH_EN == DEF_ENABLED)
    const  CPU_INT08U                        *ClientCertPtr;
    const  CPU_INT08U                        *ClientPrivKeyPtr;
           CPU_INT16U                         ClientCertLen;
           CPU_INT16U                         ClientPrivKeyLen;
#endif
    const  CPU_INT08U                        *ServerCertPtr;
    const  CPU_INT08U                        *ServerPrivKeyPtr;
           CPU_INT16U                         ServerCertLen;
           CPU_INT16U                         ServerPrivKeyLen;
} NET_SECURE_EMSSL_DATA;


typedef  struct  emssl_session {
           CPU_CHAR                          *CommonNamePtr;
           NET_SOCK_SECURE_UNTRUSTED_REASON   UntrustedReason;
           NET_SOCK_SECURE_TRUST_FNCT         TrustCallbackFnctPtr;
           NET_SECURE_EMSSL_DATA             *DataPtr;
           NET_SOCK_SECURE_TYPE               Type;
           SSL_SESSION                        SessionCtx;
} NET_SECURE_EMSSL_SESSION;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                    EMSSL CONFIGURATION FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           SSL_X_Config()
*
* Description : Configures emSSL to use the specified algorithms.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : SSL_Init().
*
* Note(s)     : (1) Minimal configuration set by default for TLS-RSA-WITH-AES-128-GCM-SHA256.
*********************************************************************************************************
*/

void  SSL_X_Config (void)
{                                                               /* Add memory to emSSL.                                 */
    SSL_MEM_Add(SSL_StoreMemPtr, NET_SECURE_SSL_MEM_SIZE);
                                                                /* Install cipher suites.                               */
#if (EMSSL_CFG_MIN_FOOTPRINT_EN == DEF_ENABLED)                 /* See Note 1.                                          */
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_128_GCM_SHA256);
    SSL_MAC_Add(&SSL_MAC_SHA256_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_128_GCM_API);
    SSL_SIGNATURE_VERIFY_Add(&SSL_SIGNATURE_VERIFY_RSA_API);
    SSL_SIGNATURE_SIGN_Add(&SSL_SIGNATURE_SIGN_RSA_API);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA256_WITH_RSA_ENCRYPTION);
    SSL_PROTOCOL_Add(&SSL_PROTOCOL_TLS1v2_API);
#else
#if (EMSSL_CFG_DHE_RSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_3DES_EDE_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_SEED_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_128_CCM);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_128_CCM_8);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_256_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_256_CCM);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_256_CCM_8);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_AES_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_ARIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_ARIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_ARIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_ARIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
#endif
#if (EMSSL_CFG_ECDHE_ECDSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_128_CCM);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_128_CCM_8);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_256_CCM);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_256_CCM_8);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_RC4_128_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
#endif
#if (EMSSL_CFG_ECDHE_RSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_AES_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_AES_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_AES_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_AES_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_ARIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_ARIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_RC4_128_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
#endif

#if (EMSSL_CFG_ECDH_ECDSA_CIPHER_SUITE_EN == DEF_ENABLED)
#if (EMSSL_CFG_NULL_CIPHER_SUITE_EN == DEF_ENABLED)
     SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_NULL_SHA);
#endif
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_RC4_128_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_AES_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_AES_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_AES_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_AES_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_AES_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_AES_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384);
#endif
#if (EMSSL_CFG_ECDH_RSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_3DES_EDE_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_AES_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_AES_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_AES_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_AES_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_AES_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_AES_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_ARIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_ARIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_ARIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_ARIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_ECDH_RSA_WITH_RC4_128_SHA);
#endif
#if (EMSSL_CFG_RSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_3DES_EDE_CBC_SHA);        /* Mandatory TLS 1.1 cipher suite [TLS1v1] s. 9         */
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_SEED_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_128_CBC_SHA256);      /* Mandatory TLS 1.2 cipher suite [TLS1v2] s. 9         */
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_128_CCM);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_256_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_256_CCM);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_AES_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_CAMELLIA_128_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_CAMELLIA_256_CBC_SHA);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_CAMELLIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_CAMELLIA_256_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_CAMELLIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_CAMELLIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_ARIA_128_CBC_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_ARIA_256_CBC_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_ARIA_128_GCM_SHA256);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_ARIA_256_GCM_SHA384);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_RC4_128_MD5);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_RC4_128_SHA);
#if (EMSSL_CFG_NULL_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_NULL_MD5);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_NULL_SHA);
    SSL_SUITE_Add(&SSL_SUITE_RSA_WITH_NULL_SHA256);
#endif
#endif
                                                                /* Add MAC algorithms.                                  */
    SSL_MAC_Add(&SSL_MAC_MD5_API);
    SSL_MAC_Add(&SSL_MAC_SHA_API);
    SSL_MAC_Add(&SSL_MAC_SHA224_API);
    SSL_MAC_Add(&SSL_MAC_SHA256_API);
    SSL_MAC_Add(&SSL_MAC_SHA384_API);
    SSL_MAC_Add(&SSL_MAC_SHA512_API);
                                                                /* Add bulk ciphers.                                    */
    SSL_CIPHER_Add(&SSL_CIPHER_AES_128_GCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_256_GCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_ARIA_256_GCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_ARIA_128_GCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_CAMELLIA_256_GCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_CAMELLIA_128_GCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_128_CCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_256_CCM_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_128_CCM_8_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_256_CCM_8_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_128_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_AES_256_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_ARIA_256_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_ARIA_128_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_CAMELLIA_256_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_CAMELLIA_128_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_3DES_EDE_CBC_API);               /* 3DES no longer considered secure. Not recommended.   */
    SSL_CIPHER_Add(&SSL_CIPHER_SEED_CBC_API);
    SSL_CIPHER_Add(&SSL_CIPHER_RC4_128_API);
    SSL_CIPHER_Add(&SSL_CIPHER_CHACHA20_POLY1305_API);
                                                                /* Add public key signature verifiers.                  */
    SSL_SIGNATURE_VERIFY_Add(&SSL_SIGNATURE_VERIFY_RSA_API);
    SSL_SIGNATURE_VERIFY_Add(&SSL_SIGNATURE_VERIFY_ECDSA_API);

#if (EMSSL_CFG_DHE_RSA_CIPHER_SUITE_EN   == DEF_ENABLED || \
     EMSSL_CFG_ECDHE_RSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SIGNATURE_SIGN_Add(&SSL_SIGNATURE_SIGN_RSA_API);
#endif

#if (EMSSL_CFG_ECDHE_ECDSA_CIPHER_SUITE_EN == DEF_ENABLED)
    SSL_SIGNATURE_SIGN_Add(&SSL_SIGNATURE_SIGN_ECDSA_API);
#endif
                                                                /* Add advertised signature algorithms.                 */
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA_WITH_RSA_ENCRYPTION);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA224_WITH_RSA_ENCRYPTION);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA256_WITH_RSA_ENCRYPTION);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA384_WITH_RSA_ENCRYPTION);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA512_WITH_RSA_ENCRYPTION);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA_WITH_ECDSA);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA224_WITH_ECDSA);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA256_WITH_ECDSA);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA384_WITH_ECDSA);
    SSL_SIGNATURE_ALGORITHM_Add(SSL_SIGNATURE_SHA512_WITH_ECDSA);

                                                                /* Add elliptic curves.                                 */
    SSL_CURVE_Add(&SSL_CURVE_Curve25519);
    SSL_CURVE_Add(&SSL_CURVE_secp521r1);
    SSL_CURVE_Add(&SSL_CURVE_secp384r1);
    SSL_CURVE_Add(&SSL_CURVE_secp256r1);
    SSL_CURVE_Add(&SSL_CURVE_secp224r1);
    SSL_CURVE_Add(&SSL_CURVE_secp192r1);
    SSL_CURVE_Add(&SSL_CURVE_brainpoolP512r1);
    SSL_CURVE_Add(&SSL_CURVE_brainpoolP384r1);
    SSL_CURVE_Add(&SSL_CURVE_brainpoolP256r1);

                                                                /* Add TLS protocol versions.                           */
    SSL_PROTOCOL_Add(&SSL_PROTOCOL_TLS1v0_API);
    SSL_PROTOCOL_Add(&SSL_PROTOCOL_TLS1v1_API);
    SSL_PROTOCOL_Add(&SSL_PROTOCOL_TLS1v2_API);
#endif
}


/*
*********************************************************************************************************
*                                          CRYPTO_X_Config()
*
* Description : Installs crypto implementation and available hardware.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : CRYPTO_Init().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  CRYPTO_X_Config (void)
{
                                                                /* Install software hashes and ciphers.                 */
    CRYPTO_MD5_Install     (&CRYPTO_HASH_MD5_SW,        DEF_NULL);
    CRYPTO_SHA1_Install    (&CRYPTO_HASH_SHA1_SW,       DEF_NULL);
    CRYPTO_SHA224_Install  (&CRYPTO_HASH_SHA224_SW,     DEF_NULL);
    CRYPTO_SHA256_Install  (&CRYPTO_HASH_SHA256_SW,     DEF_NULL);
    CRYPTO_SHA512_Install  (&CRYPTO_HASH_SHA512_SW,     DEF_NULL);
    CRYPTO_AES_Install     (&CRYPTO_CIPHER_AES_SW,      DEF_NULL);
    CRYPTO_SEED_Install    (&CRYPTO_CIPHER_SEED_SW,     DEF_NULL);
    CRYPTO_ARIA_Install    (&CRYPTO_CIPHER_ARIA_SW,     DEF_NULL);
    CRYPTO_CAMELLIA_Install(&CRYPTO_CIPHER_CAMELLIA_SW, DEF_NULL);


#ifdef  NET_SECURE_EMSSL_HW_MODULE_PRESENT
    NET_SECURE_HW_ACCEL_INIT();                                 /* Enable the available hardware accelerator(s).        */
#else
    CRYPTO_RNG_Install(&NetSecure_LibMemRand_API);              /* Install RNG API that uses uC/LIB's Math_Rand() funct.*/
#endif
                                                                /* Install small modular exponentiation functions.      */
    CRYPTO_MPI_SetPublicModExp(CRYPTO_MPI_ModExp_Basic_Fast);
    CRYPTO_MPI_SetPrivateModExp(CRYPTO_MPI_ModExp_Basic_Fast);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                   MICRIUM uC/OS WRAPPER FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            SSL_OS_Init()
*
* Description : Initializes emSSL session access semaphore
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : SSL_Init().
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  SSL_OS_Init (void)
{
    RTOS_ERR  kal_err;


    emSSL_SessionLockHandle = KAL_SemCreate((const CPU_CHAR *)"emSSL Session Sem",
                                                               DEF_NULL,
                                                              &kal_err);
    if (kal_err != RTOS_ERR_NONE) {
        SSL_TRACE_DBG(("Could not create SSL session semaphore."));
        CPU_SW_EXCEPTION(;);
    }
}


/*
*********************************************************************************************************
*                                           SSL_OS_Unlock()
*
* Description : Increments the emSSL session access semaphore counter.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  SSL_OS_Unlock (void)
{
    RTOS_ERR  kal_err;


    KAL_SemPend(emSSL_SessionLockHandle, KAL_OPT_PEND_BLOCKING, KAL_TIMEOUT_INFINITE, &kal_err);
    if (kal_err != RTOS_ERR_NONE) {
        CPU_SW_EXCEPTION(;);
    }
}


/*
*********************************************************************************************************
*                                            SSL_OS_Lock()
*
* Description : Decrements the emSSL session access semaphore counter.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  SSL_OS_Lock (void)
{
    RTOS_ERR  kal_err;


    KAL_SemPost(emSSL_SessionLockHandle, KAL_OPT_POST_NONE, &kal_err);
    if (kal_err != RTOS_ERR_NONE) {
        CPU_SW_EXCEPTION(;);
    }
}


/*
*********************************************************************************************************
*                                          CRYPTO_OS_Init()
*
* Description : Initializes emSSL HW access semaphore.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : CRYPTO_Init().
*
* Note(s)     : This semaphore protects access to the hardware resource.
*********************************************************************************************************
*/

void  CRYPTO_OS_Init (void)
{
    RTOS_ERR  kal_err;


    kal_err = RTOS_ERR_NONE;
    emSSL_HWCryptoSemHandle = KAL_SemCreate((const CPU_CHAR *)"emSSL HW Crypto Lock",
                                                               DEF_NULL,
                                                              &kal_err);
    if (kal_err != RTOS_ERR_NONE) {
        SSL_TRACE_DBG(("Could not create SSL HW Crypto semaphore."));
        CPU_SW_EXCEPTION(;);
    }
}


/*
*********************************************************************************************************
*                                          CRYPTO_OS_Claim()
*
* Description : Increment emSSL HW access semaphore counter.
*
* Argument(s) : Unit     Hardware device number.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  CRYPTO_OS_Claim (unsigned int Unit)
{
    RTOS_ERR  kal_err;


    KAL_SemPost(emSSL_HWCryptoSemHandle, KAL_OPT_POST_NONE, &kal_err);
    if (kal_err != RTOS_ERR_NONE) {
        CPU_SW_EXCEPTION(;);
    }
}


/*
*********************************************************************************************************
*                                         CRYPTO_OS_Unclaim()
*
* Description : Decrement emSSL HW access semaphore counter.
*
* Argument(s) : Unit     Hardware device number.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  CRYPTO_OS_Unclaim (unsigned int Unit)
{
    RTOS_ERR  kal_err;


    KAL_SemPend(emSSL_HWCryptoSemHandle, KAL_OPT_PEND_BLOCKING, KAL_TIMEOUT_INFINITE, &kal_err);
    if (kal_err != RTOS_ERR_NONE) {
        CPU_SW_EXCEPTION(;);
    }
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                    uC/TCPIP TRANSPORT FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        NetSecure_emSSL_Rx()
*
* Description : Receives encrypted data from the socket receive queue.
*
* Argument(s) : sock_id          Socket descriptor/handle identifier of socket to receive data.
*
*               p_data_buf       Buffer that will store the received data.
*
*               data_buf_len     Length of p_data_buf.
*
*               flags            Socket flags.
*
* Return(s)   : Number of positive data octets received, if NO error(s)
*
*                0,                                      if socket connection closed
*
*               -1,                                      otherwise
*
* Caller(s)   : _SSL_IO_Recv()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  int  NetSecure_emSSL_Rx (int    sock_id,
                                 char  *p_data_buf,
                                 int    data_buf_len,
                                 int    flags)
{
    NET_SOCK_RTN_CODE   ret_err;
    NET_ERR             net_err;
    CPU_BOOLEAN         block;
    NET_SOCK           *p_sock;


    ret_err = NET_SOCK_BSD_ERR_RX;
    p_sock  = NetSock_GetObj(sock_id);

    if ((p_sock == DEF_NULL) || (p_sock->SecureSession == (NET_SECURE_EMSSL_SESSION *)0)) {
        return (ret_err);
    }
                                                                /* Make sure socket is configured to block.             */
    block = DEF_BIT_IS_CLR(p_sock->Flags, NET_SOCK_FLAG_SOCK_NO_BLOCK);
    DEF_BIT_CLR(p_sock->Flags, NET_SOCK_FLAG_SOCK_NO_BLOCK);

    ret_err = NetSock_RxDataHandlerStream( sock_id,
                                           p_sock,
                                           p_data_buf,
                                           data_buf_len,
                                           flags,
                                           0u,
                                           0u,
                                          &net_err);
    if (!block) {
        DEF_BIT_SET(p_sock->Flags, NET_SOCK_FLAG_SOCK_NO_BLOCK);
    }

    return (ret_err);
}


/*
*********************************************************************************************************
*                                        NetSecure_emSSL_Tx()
*
* Description : Sends encrypted data from the upper emSSL stack.
*
* Argument(s) : sock_id          Socket descriptor/handle identifier of socket to send data.
*
*               p_data_buf       Buffer that contains the data.
*
*               data_buf_len     Length of p_data_buf.
*
*               flags            Socket flags.
*
* Return(s)   : Number of positive data octets transmitted, if NO error(s)
*
*                0,                                      if socket connection closed
*
*               -1,                                      otherwise
*
* Caller(s)   : _SSL_IO_Send()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  int  NetSecure_emSSL_Tx (       int    sock_id,
                                 const  char  *p_data_buf,
                                        int    data_buf_len,
                                        int    flags)
{
    CPU_INT32S                 result;
    NET_SOCK_RTN_CODE          ret_err;
    NET_ERR                    net_err;
    NET_SOCK                  *p_sock;
    NET_SECURE_EMSSL_SESSION  *p_session;


    ret_err   = NET_SOCK_BSD_ERR_TX;
    p_sock    = NetSock_GetObj(sock_id);
    p_session = p_sock->SecureSession;

    if ((p_sock == DEF_NULL) || (&(p_session->SessionCtx) == (SSL_SESSION *)0)) {
        return (ret_err);
    }

    if ((p_sock->State != NET_SOCK_STATE_CONN     )  &&         /* Data cannot be sent if socket is closed or closing.  */
        (p_sock->State != NET_SOCK_STATE_CONN_DONE)) {
        return (ret_err);
    }

    result = NetSock_TxDataHandlerStream(        sock_id,
                                                 p_sock,
                                         (void *)p_data_buf,
                                                 data_buf_len,
                                                 flags,
                                                &net_err);

    return (result);
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                     NETWORK SECURITY FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      NetSecure_CA_CertInstall()
*
* Description : Install certificate authority's certificate.
*
* Argument(s) : p_ca_cert      Pointer to CA certificate.
*
*               ca_cert_len    Certificate length.
*
*               fmt            Certificate format:
*
*                                  NET_SOCK_SECURE_CERT_KEY_FMT_PEM
*                                  NET_SOCK_SECURE_CERT_KEY_FMT_DER
*
*               p_err          Pointer to variable that will receive the return error code from this function :
*
*                                  NET_SECURE_ERR_NONE          Certificate   successfully installed.
*                                  NET_SECURE_ERR_INSTALL       Certificate installation   failed.
*                                  NET_ERR_FAULT_NOT_SUPPORTED  Certificate has invalid format.
*
* Return(s)   : DEF_OK,
*
*               DEF_FAIL.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #2].
*
* Note(s)     : (1) Net_Secure_CA_CertInstall() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) emSSL only supports DER format. PEM-formatted certificates should first be converted
*                   using SEGGER's PrintCert.exe command line utility. Please consult the reference manual.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetSecure_CA_CertInstall (const  void                 *p_ca_cert,
                                              CPU_INT32U            ca_cert_len,
                                              NET_SECURE_CERT_FMT   fmt,
                                              NET_ERR              *p_err)
{
    CPU_BOOLEAN  rtn_val;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)                 /* ---------------- VALIDATE CERT LEN ----------------- */
    if (ca_cert_len > NET_SECURE_CFG_MAX_CA_CERT_LEN) {
       *p_err = NET_SECURE_ERR_INSTALL;
        rtn_val = DEF_FAIL;
        goto exit_fault;
    }

    if (p_ca_cert == (SSL_ROOT_CERTIFICATE *)DEF_NULL) {
       *p_err = NET_SECURE_ERR_INSTALL;
        rtn_val = DEF_FAIL;
        goto exit_fault;
    }
#endif

    Net_GlobalLockAcquire((void *)&NetSecure_CA_CertInstall, p_err);
    if (*p_err != NET_ERR_NONE) {
         rtn_val = DEF_FAIL;
         goto exit_fault;
    }

    switch (fmt) {
        case NET_SOCK_SECURE_CERT_KEY_FMT_DER:
             SSL_ROOT_CERTIFICATE_Add((SSL_ROOT_CERTIFICATE *)p_ca_cert);
            *p_err   = NET_SECURE_ERR_NONE;
             rtn_val = DEF_OK;
             break;


        case NET_SOCK_SECURE_CERT_KEY_FMT_PEM:
        case NET_SOCK_SECURE_CERT_KEY_FMT_NONE:
        default:
            *p_err   = NET_ERR_FAULT_NOT_SUPPORTED;
             rtn_val = DEF_FAIL;
             break;
    }

    Net_GlobalLockRelease();

exit_fault:
    return (rtn_val);
}


/*
*********************************************************************************************************
*                                            NetSecure_Log()
*
* Description : Log the given string.
*
* Argument(s) : p_str    Pointer to string to log.
*
* Return(s)   : none.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetSecure_Log (CPU_CHAR  *p_str)
{
    SSL_TRACE_DBG(((CPU_CHAR *)p_str));
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           NetSecure_Init()
*
* Description : (1) Initialize security port :
*
*                   (a) Initialize security memory pools
*                   (b) Initialize CA descriptors
*                   (c) Initialize emSSL
*
*
* Argument(s) : p_err    Pointer to variable that will receive the return error code from this function :
*
*                            LIB_MEM_ERR_NONE                No error initializing resources for emSSL.
*                            NET_SECURE_ERR_INIT_POOL        Could not create memory pool(s).
*                            LIB_MEM_ERR_HEAP_EMPTY          No more memory available on heap.
*
*                            And:
*                            Error Codes returned by Mem_DynPoolCreate() and Mem_HeapAlloc().
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetSecure_Init (NET_ERR  *p_err)
{
    LIB_ERR  lib_err;


    SSL_TRACE_DBG(("%s: Start\n", __FUNCTION__));
   *p_err = NET_SECURE_ERR_NONE;

    if (NET_SECURE_SSL_CONN_NBR_MAX > 0u) {
        Mem_DynPoolCreate("SSL Session pool",
                          &NetSecure_SessionPool,
                           DEF_NULL,
                           sizeof(NET_SECURE_EMSSL_SESSION),
                           sizeof(CPU_ALIGN),
                           0u,
                           NET_SECURE_SSL_CONN_NBR_MAX,
                          &lib_err);
        if (lib_err != LIB_MEM_ERR_NONE) {
            SSL_TRACE_DBG(("Mem_DynPoolCreate() returned an error"));
           *p_err = NET_SECURE_ERR_INIT_POOL;
            return;
        }
                                                                /* Allocate Heap space for emSSL.                       */
        SSL_StoreMemPtr = Mem_HeapAlloc( NET_SECURE_SSL_MEM_SIZE,
                                         sizeof(CPU_SIZE_T),
                                         DEF_NULL,
                                        &lib_err);
        if (lib_err != LIB_MEM_ERR_NONE) {
            SSL_TRACE_DBG(("Mem_HeapAlloc() returned an error"));
           *p_err = NET_SECURE_ERR_INIT_POOL;
            return;
        }
    } else {
        SSL_TRACE_DBG(("Invalid number of sessions in net_cfg.h"));
       *p_err = NET_SECURE_ERR_INIT_POOL;
        return;
    }

                                                                /* Create mem pool of descriptors. One block per socket.*/
    if (NET_SECURE_CFG_MAX_NBR_SOCK_CLIENT > 0u) {
        Mem_DynPoolCreate("SSL Client Desc pool",
                          &NetSecure_ClientDescPool,
                           DEF_NULL,
                           sizeof(NET_SECURE_EMSSL_DATA),
                           sizeof(CPU_ALIGN),
                           0u,
                           NET_SECURE_CFG_MAX_NBR_SOCK_CLIENT,
                          &lib_err);
        if (lib_err != LIB_MEM_ERR_NONE) {
            SSL_TRACE_DBG(("Mem_DynPoolCreate() returned an error"));
           *p_err = NET_SECURE_ERR_INIT_POOL;
            return;
        }
    }

    if (NET_SECURE_CFG_MAX_NBR_SOCK_SERVER > 0u) {
        Mem_DynPoolCreate("SSL Server Desc pool",
                          &NetSecure_ServerDescPool,
                           DEF_NULL,
                           sizeof(NET_SECURE_EMSSL_DATA),
                           sizeof(CPU_ALIGN),
                           0u,
                           NET_SECURE_CFG_MAX_NBR_SOCK_SERVER,
                          &lib_err);
        if (lib_err != LIB_MEM_ERR_NONE) {
            SSL_TRACE_DBG(("Mem_DynPoolCreate() returned an error"));
           *p_err = NET_SECURE_ERR_INIT_POOL;
            return;
        }
    }

    SSL_Init();                                                 /* Initialize emSSL stack.                              */
}


/*
*********************************************************************************************************
*                                        NetSecure_InitSession()
*
* Description : Initialize a new secure session.
*
* Argument(s) : p_sock    Pointer to the accepted/connected socket.
*               ------    Argument checked in NetSock_CfgSecure(),
*                                             NetSecure_SockAccept().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_SECURE_ERR_NONE         Secure session     available.
*                             NET_SECURE_ERR_NOT_AVAIL    Secure session NOT available.
*
* Return(s)   : none.
*
* Caller(s)   : NetSock_CfgSecure().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) By default, emSSL enables TLS versions 1.0 through 1.2 when negotiating connections.
*                   However, if the user wishes to restrict the TLS versions, (s)he can use the
*                   SSL_SESSION_SetProtocolRange() API call.
*
*               (2) If the session pointer is NOT null, the session memory block is assumed to have been
*                   allocated and validated by NetSecure_Accept() and thus NetSecure_InitSession() would
*                   only need to prepare it with SSL_SESSION_Prepare().
*********************************************************************************************************
*/

void  NetSecure_InitSession (NET_SOCK  *p_sock,
                             NET_ERR   *p_err)
{
    NET_SECURE_EMSSL_SESSION  *p_blk;
    LIB_ERR                    lib_err;


    SSL_TRACE_DBG(("%s: Start\n", __FUNCTION__));
                                                                /* Get SSL session object. See Note 2.                  */
    p_blk = Mem_DynPoolBlkGet(&NetSecure_SessionPool, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_SECURE_ERR_NOT_AVAIL;
        return;
    }

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    SSL_MEMSET(&p_blk->SessionCtx, 0, sizeof(p_blk->SessionCtx));
#endif

    p_blk->DataPtr         = (void *)DEF_NULL;
    p_blk->Type            =  NET_SOCK_SECURE_TYPE_NONE;
    p_blk->UntrustedReason =  NET_SOCK_SECURE_UNKNOWN;
    p_sock->SecureSession  =  p_blk;

    SSL_TRACE_DBG(("%s: Normal exit\n", __FUNCTION__));

    SSL_SESSION_Prepare(&p_blk->SessionCtx,
                         p_sock->ID,
                        &Micrium_TCP_Transport);

    SSL_SESSION_SetCertificateAPI(&p_blk->SessionCtx, &Micrium_CertAPI);

#if (EMSSL_MUTUAL_AUTH_EN == DEF_ENABLED)
    SSL_CLIENT_ConfigMutualAuth();
#endif

   *p_err = NET_SECURE_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetSecure_GetCert()
*
* Description : Gets the server socket's certificate in DER format from the session data descriptor.
*
* Argument(s) : p_session     Pointer to an emSSL session.
*
*               index         Index of the certificate to return.
*
*               p_data        Pointer to a memory location where the private key will be stored.
*
*               p_data_len    Pointer to a variable that will store the size of the private key.
*
* Return(s)   : 0, if the private key was successfully obtained.
*
*              -1, otherwise.
*
* Caller(s)   : _SSL_COMMON_SendCertificate()
*
* Note(s)     : (1) The socket associated with 'p_session' must have been previously configured by
*                   the NetSecure_SockCertKeyCfg() API.
*********************************************************************************************************
*/

static  int  NetSecure_GetCert (       SSL_SESSION   *p_session,
                                       CPU_INT32U     index,
                                const  CPU_INT08U   **p_data,
                                       CPU_INT32U    *p_data_len)
{
    NET_SOCK                  *p_sock;
    NET_SECURE_EMSSL_SESSION  *p_session_desc;


    if (p_session == (void *)0) {
        return (-1);
    }

    p_sock = NetSock_GetObj(p_session->Socket);

    if (p_sock == (void *)0) {
        return (-1);
    }

    p_session_desc = (NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession;

    if (index == 0u) {                                          /* emSSL allows for just one self-signed certificate.   */
       *p_data     = p_session_desc->DataPtr->ServerCertPtr;
       *p_data_len = p_session_desc->DataPtr->ServerCertLen;
        return (0);
    } else {
       *p_data     = 0u;
       *p_data_len = 0u;
        return (-1);
    }
}


/*
*********************************************************************************************************
*                                        NetSecure_GetPrivKey()
*
* Description : Gets the server socket's private key from the session data descriptor.
*
* Argument(s) : p_session     Pointer to an emSSL session.
*
*               p_data        Pointer to a memory location where the private key will be stored.
*
*               p_data_len    Pointer to a variable that will store the size of the private key.
*
* Return(s)   : 0, if the private key was successfully obtained.
*
*              -1, otherwise.
*
* Caller(s)   : _SSL_CLIENT_SendCertificateVerify()
*               _SSL_SERVER_SendServerKeyExchange_SignRSA()
*               _SSL_SERVER_SendServerKeyExchange_SignECDSA()
*               _SSL_SERVER_ProcessClientKeyExchange_RSA()
*               _SSL_SERVER_ProcessClientKeyExchange_ECDH()
*
* Note(s)     : (1) The socket associated with 'p_session' must have been previously configured by
*                   the NetSecure_SockCertKeyCfg() API.
*********************************************************************************************************
*/

static  int  NetSecure_GetPrivKey (       SSL_SESSION   *p_session,
                                   const  CPU_INT08U   **p_data,
                                          CPU_INT32U    *p_data_len)
{
    NET_SOCK                  *p_sock;
    NET_SECURE_EMSSL_SESSION  *p_session_desc;


    if (p_session == (void *)0) {
        return (-1);
    }

    p_sock = NetSock_GetObj(p_session->Socket);

    if (p_sock == (void *)0) {
        return (-1);
    }

    p_session_desc = (NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession;
   *p_data         = p_session_desc->DataPtr->ServerPrivKeyPtr;
   *p_data_len     = p_session_desc->DataPtr->ServerPrivKeyLen;

    return (0);
}


/*
*********************************************************************************************************
*                                          NetSecure_GetClk()
*
* Description : Gets the UNIX-formatted timestamp from uC/Clk
*
* Argument(s) : none.
*
* Return(s)   : UNIX-formatted timestamp (in seconds) as reported by uC/Clk.
*
*               0 otherwise.
*
* Caller(s)   : _SSL_COMMON_ProcessCertificate()
*               _SSL_CLIENT_SendClientHello()
*
* Note(s)     : (1) Be sure to include and initialize the uC/Clk module in the project with the current
*                   date and time. Use uC/SNTPc to update the timestamp if maintained in software and not
*                   via an RTC.
*********************************************************************************************************
*/

static  unsigned  long  NetSecure_GetClk (void)
{
#if (CLK_CFG_UNIX_EN == DEF_ENABLED)
  CPU_BOOLEAN  is_valid;
  CLK_TS_SEC   unix_time = 0u;


  is_valid = Clk_GetTS_Unix(&unix_time);
  return ((is_valid) ? ((unsigned long)unix_time) : 0u);
#else
  return (0u);
#endif
}


/*
*********************************************************************************************************
*                                     NetSecure_GetRandomNbr()
*
* Description : Gets a pseudo-random number for emSSL to use.
*
* Argument(s) : p_nbr     Generated pseudo-random number.
*
*               data_len  Length of data.
*
* Return(s)   : none.
*
* Caller(s)   : CRYPTO_RNG_Get().
*
* Note(s)     : Math_Init() must be called at some point before uC/TCPIP's initialization.
*********************************************************************************************************
*/

#ifndef  NET_SECURE_EMSSL_HW_MODULE_PRESENT
static  void  NetSecure_GetRandomNbr (CPU_INT08U  *p_nbr,
                                      CPU_INT32U   data_len)
{
    if (p_nbr && data_len) {
        while (data_len > 0u) {
           *p_nbr += (Math_Rand() + data_len);
            data_len --;
        }
    }
}
#endif


/*
*********************************************************************************************************
*                                      NetSecure_SockCertKeyCfg()
*
* Description : Configure server secure socket's certificate and key from buffers:
*
*
* Argument(s) : p_sock           Pointer to the server's socket to configure certificate and key.
*               ------           Argument checked in NetSock_CfgSecure(),
*                                                    NetSock_CfgSecureServerCertKeyInstall().
*
*               sock_type        Type of secure socket
*
*                                NET_SOCK_SECURE_TYPE_CLIENT        Client socket.
*                                NET_SOCK_SECURE_TYPE_SERVER        Server socket.
*
*               pbuf_cert        Pointer to the certificate         buffer to install.
*               ------           Argument checked in NetSock_CfgSecure(),
*                                                    NetSock_CfgSecureServerCertKeyInstall().
*
*               buf_cert_size    Size    of the certificate         buffer to install.
*
*               pbuf_key         Pointer to the key                 buffer to install.
*               ------           Argument checked in NetSock_CfgSecure(),
*                                                    NetSock_CfgSecureServerCertKeyInstall().
*
*               buf_key_size     Size    of the key                 buffer to install.
*
*               fmt              Format  of the certificate and key buffer to install.
*
*                                    NET_SECURE_INSTALL_FMT_PEM      Certificate and Key format is PEM.
*                                    NET_SECURE_INSTALL_FMT_DER      Certificate and Key format is DER.
*
*               cert_chain       Certificate point to a chain of certificate.
*
*                                    DEF_YES     Certificate points to a chain  of certificate.
*                                    DEF_NO      Certificate points to a single    certificate.
*
*               p_err            Pointer to variable that will receive the return error code from this function :
*
*                                    NET_SOCK_ERR_NONE                       Server socket's certificate and key successfully
*                                                                              installed.
*                                    NET_SOCK_ERR_NULL_PTR                   Invalid pointer.

*                                    NET_SECURE_ERR_INSTALL                  Certificate and/or Key NOT successfully installed
*                                                                              or invalid certificate and key format.
*
*                                    NET_SECURE_ERR_HANDSHAKE                SSL Handshake Cannot Be Performed.
*
* Return(s)   : DEF_OK,   Server socket's certificate and key successfully configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetSock_CfgSecureServerCertKeyInstall().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) NetSock_CfgSecureServerCertKeyBuf() is called by application function(s) & ... :
*
*                   (a) MUST be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) PEM-formatted certificates are not supported by emSSL. The 'fmt' parameter should always
*                   be NET_SECURE_INSTALL_FMT_DER.
*
*               (3) The 'cert_chain' parameter is unused by emSSL. If a user wishes to install a certificate
*                   that was issued by another certificate in the store then simply install it by calling
*                   NetSecure_CA_CertInstall(); emSSL handles the chain internally.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetSecure_SockCertKeyCfg (       NET_SOCK                       *p_sock,
                                              NET_SOCK_SECURE_TYPE            sock_type,
                                       const  CPU_INT08U                     *p_buf_cert,
                                              CPU_SIZE_T                      buf_cert_size,
                                       const  CPU_INT08U                     *p_buf_key,
                                              CPU_SIZE_T                      buf_key_size,
                                              NET_SOCK_SECURE_CERT_KEY_FMT    fmt,
                                              CPU_BOOLEAN                     cert_chain,
                                              NET_ERR                        *p_err)
{
    NET_SECURE_EMSSL_SESSION  *p_session_desc;
    LIB_ERR                    lib_err;
    CPU_SR_ALLOC();


   (void)cert_chain;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if ((buf_cert_size >  NET_SECURE_CFG_MAX_CERT_LEN) ||
        (buf_cert_size == 0u)) {
       *p_err = NET_SECURE_ERR_INSTALL;
        return (DEF_FAIL);
    }

    if ((buf_key_size >  NET_SECURE_CFG_MAX_KEY_LEN) ||
        (buf_key_size == 0u)) {
       *p_err = NET_SECURE_ERR_INSTALL;
        return (DEF_FAIL);
    }

    if (fmt != NET_SOCK_SECURE_CERT_KEY_FMT_DER) {              /* See Note 2.                                          */
       *p_err = NET_SECURE_ERR_INSTALL;
        return (DEF_FAIL);
    }
#endif

    p_session_desc = (NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession;
   *p_err          =  NET_SECURE_ERR_NONE;

    if (p_session_desc == (void *)0) {
       *p_err = NET_SOCK_ERR_NULL_PTR;
        return (DEF_FAIL);
    }

    CPU_CRITICAL_ENTER();
    p_session_desc->Type = sock_type;

    switch (sock_type) {
        case NET_SOCK_SECURE_TYPE_SERVER:
             if (p_session_desc->DataPtr) {
                *p_err = NET_SECURE_ERR_INSTALL;
                 break;
             }
             p_session_desc->DataPtr = Mem_DynPoolBlkGet(&NetSecure_ServerDescPool, &lib_err);
             if (lib_err != LIB_MEM_ERR_NONE) {
                *p_err = NET_SECURE_ERR_INSTALL;
                 break;
             }
             p_session_desc->DataPtr->ServerCertPtr    = ((SSL_ROOT_CERTIFICATE *)p_buf_cert)->pData->pCertDER;
             p_session_desc->DataPtr->ServerPrivKeyPtr = p_buf_key;
             p_session_desc->DataPtr->ServerCertLen    = buf_cert_size;
             p_session_desc->DataPtr->ServerPrivKeyLen = buf_key_size;
             break;


        case NET_SOCK_SECURE_TYPE_CLIENT:
             if (p_session_desc->DataPtr == DEF_NULL) {
                 p_session_desc->DataPtr = Mem_DynPoolBlkGet(&NetSecure_ClientDescPool, &lib_err);
                 if (lib_err != LIB_MEM_ERR_NONE) {
                    *p_err = NET_SECURE_ERR_INSTALL;
                     break;
                 }
             }
#if (EMSSL_MUTUAL_AUTH_EN == DEF_ENABLED)
             p_session_desc->DataPtr->ClientCertPtr    = ((SSL_ROOT_CERTIFICATE *)p_buf_cert)->pData->pCertDER;
             p_session_desc->DataPtr->ClientPrivKeyPtr = p_buf_key;
             p_session_desc->DataPtr->ClientCertLen    = buf_cert_size;
             p_session_desc->DataPtr->ClientPrivKeyLen = buf_key_size;
#else
            *p_err = NET_SECURE_ERR_INSTALL;
#endif
             break;


        default:
            *p_err = NET_SECURE_ERR_INSTALL;
             break;
    }
    CPU_CRITICAL_EXIT();
    if (*p_err != NET_SECURE_ERR_NONE) {
        return (DEF_FAIL);
    }

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                    NetSecure_ClientCommonNameSet()
*
* Description : Configure client secure socket's common name.
*
* Argument(s) : p_sock           Pointer to the client's socket to configure common name.
*               ------           Argument checked in NetSock_CfgSecure(),
*                                                    NetSock_CfgSecureClientCommonName().
*
*               p_common_name    Pointer to the common name.
*
*               p_err            Pointer to variable that will receive the return error code from this function :
*
*                                    NET_SOCK_ERR_NONE               Close notify alert successfully transmitted.
*                                    NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*                                    NET_SECURE_ERR_NOT_AVAIL
*
* Return(s)   : DEF_OK,   Client socket's common name successfully configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetSock_CfgSecureClientCommonName().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     :  (1) NetSecure_ClientCommonNameSet() is called by application function(s) & ... :
*
*                   (a) MUST be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetSecure_ClientCommonNameSet (NET_SOCK  *p_sock,
                                            CPU_CHAR  *p_common_name,
                                            NET_ERR   *p_err)
{
    NET_SECURE_EMSSL_SESSION  *p_session;


    p_session = (NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession;
   *p_err     = NET_SOCK_ERR_NONE;

    if (p_session == (NET_SECURE_EMSSL_SESSION *)0) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
        return (DEF_FAIL);
    }
    p_session->CommonNamePtr = p_common_name;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                  NetSecure_ClientTrustCallBackSet()
*
* Description : Configure client secure socket's trust callback function.
*
* Argument(s) : p_sock             Pointer to the client's socket to configure trust call back function.
*               ------             Argument checked in NetSock_CfgSecure(),
*                                                      NetSock_CfgSecureClientTrustCallBack().
*
*               p_callback_fnct    Pointer to the trust call back function
*               ---------------    Argument checked in NetSock_CfgSecureClientTrustCallBack(),
*
*               p_err              Pointer to variable that will receive the return error code from this function :
*
*                                      NET_SOCK_ERR_NONE               Close notify alert successfully transmitted.
*                                      NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*
* Return(s)   : DEF_OK,   Client socket's trust call back function successfully configured.
*
*               DEF_FAIL, otherwise.
*
* Caller(s)   : NetSock_CfgSecureClientTrustCallBack().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     :  (1) NetSecure_ClientTrustCallBackSet() is called by application function(s) & ... :
*
*                   (a) MUST be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock (see 'net.h  Note #3').
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetSecure_ClientTrustCallBackSet (NET_SOCK                    *p_sock,
                                               NET_SOCK_SECURE_TRUST_FNCT   p_callback_fnct,
                                               NET_ERR                     *p_err)
{
    NET_SECURE_EMSSL_SESSION  *p_session;


    p_session = (NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession;
   *p_err     = NET_SOCK_ERR_NONE;

    if (p_session == (NET_SECURE_EMSSL_SESSION *)0) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
        return (DEF_FAIL);
    }
    p_session->TrustCallbackFnctPtr = p_callback_fnct;

    return (DEF_OK);
}


/*
*********************************************************************************************************
*                                         NetSecure_SockClose()
*
* Description : (1) Close the secure socket :
*
*                   (a) Get & validate the SSL session of the socket to close
*                   (b) Transmit close notify alert to the peer
*                   (c) Free the SSL session buffer
*
*
* Argument(s) : p_sock    Pointer to a socket.
*               ------    Argument checked in NetSock_Close().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_SOCK_ERR_NONE               Secure session successfully closed.
*                             NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*
* Return(s)   : none.
*
* Caller(s)   : NetSock_CloseSockHandler()
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetSecure_SockClose (NET_SOCK  *p_sock,
                           NET_ERR   *p_err)
{
    LIB_ERR                 lib_err;
    SSL_SESSION            *p_session;
    NET_SECURE_EMSSL_DATA  *p_data;
    NET_SOCK_SECURE_TYPE    sock_type;


    sock_type =  (((NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession)->Type);
    p_data    =  (((NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession)->DataPtr);
    p_session = &(((NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession)->SessionCtx);
   *p_err     =  NET_SOCK_ERR_NONE;
    lib_err   =  LIB_MEM_ERR_NONE;

    switch (sock_type) {
        case NET_SOCK_SECURE_TYPE_SERVER:
             if (p_data != DEF_NULL) {
                 Mem_DynPoolBlkFree(&NetSecure_ServerDescPool, p_data, &lib_err);
             } else {
                 lib_err = LIB_MEM_ERR_NULL_PTR;
             }
             break;


        case NET_SOCK_SECURE_TYPE_CLIENT:
             if (p_data != DEF_NULL) {
                 Mem_DynPoolBlkFree(&NetSecure_ClientDescPool, p_data, &lib_err);
             } else {
                 lib_err = LIB_MEM_ERR_NULL_PTR;
             }
             break;


        case NET_SOCK_SECURE_TYPE_NONE:
        default:
             if (p_data == DEF_NULL) {
                 break;
             }
             return;
    }

    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
    } else {
        ((NET_SECURE_EMSSL_SESSION  *)p_sock->SecureSession)->Type                 = NET_SOCK_SECURE_TYPE_NONE;
        ((NET_SECURE_EMSSL_SESSION  *)p_sock->SecureSession)->DataPtr              = DEF_NULL;
        ((NET_SECURE_EMSSL_SESSION  *)p_sock->SecureSession)->TrustCallbackFnctPtr = DEF_NULL;
    }

    if (p_session->State == SSL_CONNECTED) {                    /* Disconnect the session on the appropriate state.     */
        SSL_SESSION_Disconnect(p_session);
    }
    Mem_DynPoolBlkFree(&NetSecure_SessionPool, p_sock->SecureSession, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
    } else {
        p_sock->SecureSession = DEF_NULL;
    }
}


/*
*********************************************************************************************************
*                                      NetSecure_SockCloseNotify()
*
* Description : Transmit the close notify alert to the peer through a SSL session.
*
* Argument(s) : p_sock    Pointer to a socket.
*               ------    Argument checked in NetSock_Close().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_SOCK_ERR_NONE               Close notify alert successfully transmitted.
*                             NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*
* Return(s)   : none.
*
* Caller(s)   : NetSock_Close(),
*               NetSecure_SockClose().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) If the server decides to close the connection, it SHOULD send a close notify
*                   alert to the connected peer prior to perform the socket close operations.
*
*               (2) (a) This function will be called twice during a socket close process but the
*                       close notify alert will only transmitted during the first call.
*
*               (3) This function implementation is not required for emSSL. The SSL core takes care of
*                   sending out alert messages before closing a connection.
*
*********************************************************************************************************
*/

void  NetSecure_SockCloseNotify (NET_SOCK  *p_sock,
                                 NET_ERR   *p_err)
{
   *p_err = NET_SOCK_ERR_NONE;                                  /* Not Implemented, emSSL does this automatically.      */
}


/*
*********************************************************************************************************
*                                         NetSecure_SockConn()
*
* Description : (1) Connect a socket to a remote host through an encrypted SSL handshake :
*
*                   (a) Get & validate the SSL session of the connected socket
*                   (b) Initialize the     SSL connect.
*                   (c) Perform            SSL handshake.
*
*
* Argument(s) : p_sock    Pointer to a connected socket.
*               ------    Argument checked in NetSock_Conn().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_SOCK_ERR_NONE               Secure socket     successfully connected.
*                             NET_SECURE_ERR_HANDSHAKE        Secure socket NOT successfully connected.
*                             NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*
* Return(s)   : none.
*
* Caller(s)   : NetSock_Conn().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (1) emSSL reports a SSL_ERROR_SOCKET_ERROR for multiple reasons, including when a self-signed
*                   certificate is encountered in a connection attmept. For this reason, this port will NOT
*                   send the trust callback a NET_SOCK_SECURE_SELF_SIGNED reason for distrust and will send
*                   the generic NET_SOCK_SECURE_UNTRUSTED_BY_CA instead.
*
*               (2) SSL_ERROR_GetText() may be used by trust callback function to decode the reason why the
*                   connection attempt failed.
*********************************************************************************************************
*/

void  NetSecure_SockConn (NET_SOCK  *p_sock,
                          NET_ERR   *p_err)
{
    CPU_INT32S                         result;
    SSL_SESSION                       *p_session_ctx;
    NET_SECURE_EMSSL_SESSION          *p_session;
    NET_SOCK_SECURE_UNTRUSTED_REASON   reason;
    LIB_ERR                            lib_err;


   *p_err          =  NET_SOCK_ERR_NONE;
    p_session      =  ((NET_SECURE_EMSSL_SESSION  *)p_sock->SecureSession);
    p_session_ctx  = &(p_session->SessionCtx);
    result         = -1;

    if (p_session == (NET_SECURE_EMSSL_SESSION *)0) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
        return;
    }

    if (p_session_ctx == (SSL_SESSION *)0) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
        return;
    }

    if (p_session->Type == NET_SOCK_SECURE_TYPE_NONE) {
        p_session->DataPtr = Mem_DynPoolBlkGet(&NetSecure_ClientDescPool, &lib_err);
        if (lib_err != LIB_MEM_ERR_NONE) {
           *p_err = NET_SECURE_ERR_NULL_PTR;
            return;
        }
        p_session->Type = NET_SOCK_SECURE_TYPE_CLIENT;
        result          = SSL_SESSION_Connect(p_session_ctx, p_session->CommonNamePtr);

        if (p_session_ctx->ConnectionEnd != SSL_CONNECTION_END_CLIENT) {
           *p_err = NET_SECURE_ERR_HANDSHAKE;
            return;
        }
    }
    if (result < 0) {
        if (p_session->TrustCallbackFnctPtr != DEF_NULL) {
            switch (result) {
                case  SSL_ERROR_RECEIVED_CERTIFICATE_EXPIRED:
                      reason = NET_SOCK_SECURE_EXPIRE_DATE;
                      break;


                case  SSL_ERROR_RECEIVED_CERTIFICATE_REVOKED:
                      reason = NET_SOCK_SECURE_INVALID_DATE;
                      break;


                case  SSL_ERROR_RECEIVED_INTERNAL_ERROR:
                      reason = NET_SOCK_SECURE_UNKNOWN;
                      break;


                default:
                      reason = NET_SOCK_SECURE_UNTRUSTED_BY_CA;
                      break;
            }

            p_session->UntrustedReason = reason;
            p_session->TrustCallbackFnctPtr(&result, reason);
        }
       *p_err = NET_SECURE_ERR_HANDSHAKE;
    }
}


/*
*********************************************************************************************************
*                                        NetSecure_SockAccept()
*
* Description : (1) Return a new secure socket accepted from a listen socket :
*
*                   (a) Get & validate SSL session of listening socket
*                   (b) Initialize     SSL session of accepted  socket              See Note #2
*                   (c) Initialize     SSL accept
*                   (d) Perform        SSL handshake
*
*
* Argument(s) : p_sock_listen    Pointer to a listening socket.
*               -------------    Argument validated in NetSock_Accept().
*
*               p_sock_accept    Pointer to an accepted socket.
*               -------------    Argument checked   in NetSock_Accept().
*
*               p_err            Pointer to variable that will receive the return error code from this function :
*
*                                    NET_SOCK_ERR_NONE               Secure socket      successfully accepted.
*                                    NET_SECURE_ERR_HANDSHAKE        Secure socket  NOT successfully accepted.
*                                    NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*                                    NET_SECURE_ERR_NOT_AVAIL        Secure session NOT available.
*
* Return(s)   : none.
*
* Caller(s)   : NetSock_Accept().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) The SSL session of the listening socket has already been validated.  The session
*                   pointer of the accepted socket is also assumed to be valid.
*
*               (3) The listening SSL session is not initialized with the context information.  Then,
*                   the quiet shutdown option SHOULD be set to avoid trying to send encrypted data on
*                   the listening session.
*********************************************************************************************************
*/

void  NetSecure_SockAccept (NET_SOCK  *p_sock_listen,
                            NET_SOCK  *p_sock_accept,
                            NET_ERR   *p_err)
{
    CPU_INT32S                 result;
    NET_SECURE_EMSSL_SESSION  *p_session_accept;
    NET_SECURE_EMSSL_SESSION  *p_session_listen;
    LIB_ERR                    lib_err;


    if (NetSecure_SessionPool.BlkAllocCnt == NetSecure_SessionPool.BlkQtyMax) {
        SSL_TRACE_DBG(("Error: NO SSL sessions available\n"));
       *p_err = NET_SECURE_ERR_NOT_AVAIL;
        return;
    }

    if (NetSecure_ServerDescPool.BlkAllocCnt == NetSecure_ServerDescPool.BlkQtyMax) {
        SSL_TRACE_DBG(("Error: NO server sessions available\n"));
       *p_err = NET_SECURE_ERR_NOT_AVAIL;
        return;
    }

    NetSecure_InitSession(p_sock_accept, p_err);                /* Initialize session context.                          */
    if (*p_err != NET_SECURE_ERR_NONE) {
        SSL_TRACE_DBG(("Error: NO session available\n"));
       *p_err = NET_SECURE_ERR_NOT_AVAIL;
        return;
    }

    p_session_accept = ((NET_SECURE_EMSSL_SESSION *)p_sock_accept->SecureSession);
    p_session_listen = ((NET_SECURE_EMSSL_SESSION *)p_sock_listen->SecureSession);

                                                                /* Get SSL server session object.                       */
    p_session_accept->DataPtr = Mem_DynPoolBlkGet(&NetSecure_ServerDescPool, &lib_err);
    if (lib_err != LIB_MEM_ERR_NONE) {
       *p_err = NET_SECURE_ERR_NOT_AVAIL;
        return;
    }
    p_session_accept->Type                      = NET_SOCK_SECURE_TYPE_SERVER;
    p_session_accept->DataPtr->ServerCertPtr    = p_session_listen->DataPtr->ServerCertPtr;
    p_session_accept->DataPtr->ServerCertLen    = p_session_listen->DataPtr->ServerCertLen;
    p_session_accept->DataPtr->ServerPrivKeyPtr = p_session_listen->DataPtr->ServerPrivKeyPtr;
    p_session_accept->DataPtr->ServerPrivKeyLen = p_session_listen->DataPtr->ServerPrivKeyLen;

#if (EMSSL_MUTUAL_AUTH_EN == DEF_ENABLED)
    Mem_Copy((void *)p_session_accept->DataPtr->ClientCertPtr,
             (void *)p_session_listen->DataPtr->ClientCertPtr,
                     p_session_listen->DataPtr->ClientCertLen);

    Mem_Copy((void *)p_session_accept->DataPtr->ClientPrivKeyPtr,
             (void *)p_session_listen->DataPtr->ClientPrivKeyPtr,
                     p_session_listen->DataPtr->ClientPrivKeyLen);
#endif

    result = SSL_SESSION_Accept(&(p_session_accept->SessionCtx));
    if (result < 0) {
       *p_err = NET_SECURE_ERR_NOT_AVAIL;
        return;
    }

    if (p_session_accept->SessionCtx.ConnectionEnd != SSL_CONNECTION_END_SERVER) {
       *p_err = NET_SECURE_ERR_HANDSHAKE;
        return;
    }

   *p_err = NET_SOCK_ERR_NONE;
}


/*
*********************************************************************************************************
*                                     NetSecure_SockRxDataHandler()
*
* Description : Receive clear data through a secure socket :
*
*                   (a) Get & validate the SSL session of the receiving socket
*                   (b) Receive the data
*
*
* Argument(s) : p_sock          Pointer to a receive socket.
*               ------          Argument checked in NetSock_RxDataHandler().
*
*               p_data_buf      Pointer to an application data buffer that will receive the socket's
*               ----------         received data.
*
*                               Argument checked in NetSock_RxDataHandler().
*
*               data_buf_len    Size of the   application data buffer (in octets).
*               ------------    Argument checked in NetSock_RxDataHandler().
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_SOCK_ERR_NONE               Clear data successfully received.
*                                   NET_SOCK_ERR_RX_Q_CLOSED        Socket receive queue closed.
*                                   NET_ERR_FAULT_LOCK_ACQUIRE      Error acquiring global network lock
*
*                                   NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*                                   NET_ERR_RX                      Receive error.
*
* Return(s)   : Number of positive data octets received, if NO error(s).
*
*               NET_SOCK_BSD_ERR_RX,                         otherwise.
*
* Caller(s)   : NetSock_RxDataHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_SOCK_RTN_CODE  NetSecure_SockRxDataHandler (NET_SOCK    *p_sock,
                                                void        *p_data_buf,
                                                CPU_INT16U   data_buf_len,
                                                NET_ERR     *p_err)
{
    CPU_INT32S          result;
    CPU_BOOLEAN         rx_q_closed;
    NET_SOCK_RTN_CODE   ret_err;
    NET_CONN_ID         tcp_conn_id;
    NET_SOCK_ID         sock_id;
    NET_ERR             net_err;
    SSL_SESSION        *p_session;
    CPU_BOOLEAN         block;


    sock_id     = p_sock->ID;
    rx_q_closed = DEF_FALSE;
    ret_err     = NET_SOCK_BSD_ERR_RX;
   *p_err       = NET_SOCK_ERR_NONE;

    if (p_sock->SecureSession == (SSL_SESSION *)0) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
        return (ret_err);
    }

    p_session = &(((NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession)->SessionCtx);
                                                                /* Only call SSL_SESSION_Receive() if handshake success.*/
    if (p_session->State == SSL_CONNECTED) {
        result = SSL_SESSION_Receive(p_session, p_data_buf, data_buf_len);
    } else if (p_session->State == SSL_CLOSED) {
       *p_err  = NET_ERR_RX;
        result = NET_SOCK_BSD_ERR_RX;
    } else {
        block = DEF_BIT_IS_CLR(p_sock->Flags, NET_SOCK_FLAG_SOCK_NO_BLOCK);
        DEF_BIT_CLR(p_sock->Flags, NET_SOCK_FLAG_SOCK_NO_BLOCK);

        result = NetSock_RxDataHandlerStream( sock_id,
                                              p_sock,
                                              p_data_buf,
                                              data_buf_len,
                                              NET_SOCK_FLAG_NONE,
                                              0u,
                                              0u,
                                             &net_err);
        if (!block) {
            DEF_BIT_SET(p_sock->Flags, NET_SOCK_FLAG_SOCK_NO_BLOCK);
        }
    }
    if (result < 0) {
        tcp_conn_id =  NetConn_Tbl[p_sock->ID_Conn].ID_Transport;
        rx_q_closed = (NetTCP_ConnTbl[tcp_conn_id].RxQ_State == NET_TCP_RX_Q_STATE_CLOSED);
       *p_err       = (rx_q_closed) ? (NET_SOCK_ERR_RX_Q_CLOSED) : (NET_ERR_RX);
    } else {
        ret_err = result;
    }

    return (ret_err);
}


/*
*********************************************************************************************************
*                                    NetSecure_SockRxIsDataPending()
*
* Description : Is data pending in SSL receive queue.
*
* Argument(s) : p_sock    Pointer to a receive socket.
*               ------    Argument checked in NetSock_IsAvailRxStream().
*
*               p_err     Pointer to variable that will receive the return error code from this function :
*
*                             NET_SOCK_ERR_NONE               Return values is valid.
*                             NET_SOCK_ERR_FAULT              Fault.
*
* Return(s)   : DEF_YES, If data is pending.
*
*               DEF_NO,  Otherwise
*
* Caller(s)   : NetSock_IsAvailRxStream().
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if (NET_SOCK_CFG_SEL_EN == DEF_ENABLED)
CPU_BOOLEAN  NetSecure_SockRxIsDataPending (NET_SOCK  *p_sock,
                                            NET_ERR   *p_err)
{
    SSL_SESSION  *p_session;


    p_session = &(((NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession)->SessionCtx);
   *p_err     =  NET_SOCK_ERR_NONE;

    if (p_session == (SSL_SESSION *)0) {
       *p_err = NET_SOCK_ERR_FAULT;
        return (DEF_NO);
    }

    return (p_session->UnreadLen == 0u) ? DEF_NO : DEF_YES;
}
#endif


/*
*********************************************************************************************************
*                                     NetSecure_SockTxDataHandler()
*
* Description : Transmit clear data through a secure socket :
*
*                   (a) Get & validate the SSL session of the transmitting socket
*                   (b) Transmit the data
*
*
* Argument(s) : p_sock          Pointer to a transmit socket.
*               ------          Argument checked in NetSock_TxDataHandler().
*
*               p_data_buf      Pointer to application data to transmit.
*               ----------      Argument checked in NetSock_TxDataHandler().
*
*               data_buf_len    Length of  application data to transmit (in octets).
*               ------------    Argument checked in NetSock_TxDataHandler().
*
*               p_err           Pointer to variable that will receive the return error code from this function :
*
*                                   NET_SOCK_ERR_NONE               Clear data   successfully transmitted.
*                                   NET_SECURE_ERR_NULL_PTR         Secure session pointer is NULL.
*                                   NET_ERR_TX                      Transmit error.
*
* Return(s)   : Number of positive data octets transmitted, if NO error(s).
*
*               NET_SOCK_BSD_ERR_TX,                            otherwise.
*
* Caller(s)   : NetSock_TxDataHandler().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

NET_SOCK_RTN_CODE  NetSecure_SockTxDataHandler (NET_SOCK    *p_sock,
                                                void        *p_data_buf,
                                                CPU_INT16U   data_buf_len,
                                                NET_ERR     *p_err)
{
    CPU_INT32S          result;
    SSL_SESSION        *p_session;
    NET_SOCK_RTN_CODE   ret_err;
    NET_SOCK_ID         sock_id;
    NET_ERR             net_err;


    ret_err   =  NET_SOCK_BSD_ERR_TX;
   *p_err     =  NET_SOCK_ERR_NONE;
    sock_id   =  p_sock->ID;
    p_session = &(((NET_SECURE_EMSSL_SESSION *)p_sock->SecureSession)->SessionCtx);

    if (p_sock->SecureSession == (NET_SECURE_EMSSL_SESSION *)0) {
       *p_err = NET_SECURE_ERR_NULL_PTR;
        return (ret_err);
    }
                                                                /* Only use SSL_SESSION_Send() if handshake succeeded.  */
    if (p_session->State == SSL_CONNECTED) {
        result = SSL_SESSION_Send(p_session,
                                  p_data_buf,
                                  data_buf_len);
    } else {
        result = NetSock_TxDataHandlerStream( sock_id,
                                              p_sock,
                                              p_data_buf,
                                              data_buf_len,
                                              NET_SOCK_FLAG_NONE,
                                             &net_err);
    }

    if (result < 0) {
       *p_err = NET_ERR_TX;
    } else {
        ret_err = result;
    }

    return (ret_err);
}

#endif /* NET_SECURE_MODULE_EN */
