/*
*********************************************************************************************************
*
*                                   NETWORK SECURITY HARDWARE LAYER
*
*                                            SEGGER emSSL
*
* Filename : net_secure_emssl_hw.c
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

#include "net_secure_emssl_hw.h"
#include "../../../net_secure_emssl.h"
#include  <stm32f7xx_hal.h>
#include  <stm32f7xx_hal_rcc_ex.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

void  NetSecure_EmSSL_HW_RNG_Init   (void);
void  NetSecure_EmSSL_HW_HASH_Init  (void);
void  NetSecure_EmSSL_HW_CRYPTO_Init(void);


/*
*********************************************************************************************************
*                                    NetSecure_EmSSL_HW_RNG_Init()
*
* Description : Configures the hardware random number generator for emSSL to use.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetSecure_EmSSL_HW_RNG_Init (void)
{
#if defined(RNG)
    __HAL_RCC_RNG_CLK_ENABLE();
    __HAL_RCC_RNG_FORCE_RESET();
    __HAL_RCC_RNG_RELEASE_RESET();
                                                                /* Install external random number generator.            */
    CRYPTO_RNG_InstallEx(&CRYPTO_RNG_HW_STM32_RNG, &CRYPTO_RNG_HW_STM32_RNG);
#endif
}


/*
*********************************************************************************************************
*                                   NetSecure_EmSSL_HW_HASH_Init()
*
* Description : Configures the hardware hashing unit for emSSL to use.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetSecure_EmSSL_HW_HASH_Init (void)
{
#if defined(HASH)
    __HAL_RCC_HASH_CLK_ENABLE();
    __HAL_RCC_HASH_FORCE_RESET();
    __HAL_RCC_HASH_RELEASE_RESET();
#endif
}


/*
*********************************************************************************************************
*                                  NetSecure_EmSSL_HW_CRYPTO_Init()
*
* Description : Configures the hardware cryptoengine for emSSL to use.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetSecure_EmSSL_HW_CRYPTO_Init (void)
{
#if defined(CRYP)
                                                                /* Turn on clocks to the CRYP accelerator and reset it. */
    __HAL_RCC_CRYP_CLK_ENABLE();
    __HAL_RCC_CRYP_FORCE_RESET();
    __HAL_RCC_CRYP_RELEASE_RESET();

                                                                /* Install hardware assistance.                         */
    CRYPTO_AES_Install(&CRYPTO_CIPHER_AES_HW_STM32_CRYP,  NULL);
    CRYPTO_TDES_Install(&CRYPTO_CIPHER_TDES_HW_STM32_CRYP, NULL);
#endif
}
