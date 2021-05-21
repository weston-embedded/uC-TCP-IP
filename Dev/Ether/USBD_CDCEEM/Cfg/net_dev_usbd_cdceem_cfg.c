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
*                                  NETWORK DEVICE CONFIGURATION FILE
*
*                                            uC/USB-Device
*                                  Communication Device Class (CDC)
*                               Ethernet Emulation Model (EEM) subclass
*
* Filename : net_dev_usbd_cdceem_cfg.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) These are example configurations, which has been tested on a controlled environment,
*                Peer to Peer, and not on a real network and with a single TCP or UDP connection.
*
*            (2) Number of buffers might be updated following project/application requirements.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#define    NET_DEV_USBD_CDCEEM_CFG_MODULE

#include  <IF/net_if.h>
#include  <IF/net_if_ether.h>

#include  <usbd_cfg.h>
#include  "net_dev_usbd_cdceem_cfg.h"

#ifdef  NET_IF_ETHER_MODULE_EN

/*
*********************************************************************************************************
*                           EXAMPLE NETWORK INTERFACE / DEVICE CONFIGURATION
*
* Note(s) : (1) (a) Buffer & memory sizes & alignments configured in number of octets.
*               (b) Data bus size                      configured in number of bits.
*
*           (2) (a) All   network  buffer data area sizes MUST be configured greater than or equal to
*                   NET_BUF_DATA_SIZE_MIN.
*               (b) Large transmit buffer data area sizes MUST be configured greater than or equal to
*                   small transmit buffer data area sizes.
*               (c) Small transmit buffer data area sizes MAY need to be configured greater than or
*                   equal to the specific interface's minimum packet size.
*
*               See also 'net_buf.c       NetBuf_PoolCfgValidate()          Note #3'
*                      & 'net_if_ether.c  NetIF_Ether_BufPoolCfgValidate()  Note #2'.
*
*           (3) (a) MUST configure at least one (1) large receive  buffer.
*               (b) MUST configure at least one (1)       transmit buffer, however, zero (0) large OR
*                       zero (0) small transmit buffers MAY be configured.
*
*               See also 'net_buf.c  NetBuf_PoolCfgValidate()  Note #2'.
*
*           (4) Some processors or devices may be more efficient & may even REQUIRE that buffer data areas
*               align to specific CPU-word/octet address boundaries in order to successfully read/write
*               data from/to devices.  Therefore, it is recommended to align devices' buffer data areas to
*               the processor's or device's data bus width. Therefore, the alignment for GMAC device driver
*               MUST be only 16 bytes.
*
*               See also 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #2b'.
*
*           (5) Positive offset from base receive/transmit index, if required by device (or driver) :
*
*               (a) (1) Some device's may receive or buffer additional octets prior to the actual  received
*                       packet.  Thus an offset may be required to ignore these additional octets :
*
*                       (A) If a device does NOT receive or buffer any  additional octets prior to received
*                           packets, then the default offset of '0' SHOULD be configured.
*
*                       (B) However, if a device does receive or buffer additional octets prior to received
*                           packets, then configure the device's receive  offset with the number of
*                           additional octets.
*
*                       See also 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #2b1A'.
*
*                   (2) Some device's/driver's may require additional octets prior to the actual transmit
*                       packet.  Thus an offset may be required to reserve additional octets :
*
*                       (A) If a device/driver does NOT require any  additional octets prior to  transmit
*                           packets, then the default offset of '0' SHOULD be configured.
*
*                       (B) However, if a device/driver does require additional octets prior to  transmit
*                           packets, then configure the device's transmit offset with the number of
*                           additional octets.
*
*                       See also 'net_buf.h  NETWORK BUFFER INDEX & SIZE DEFINES  Note #2b1B'.
*
*               (b) Since each network buffer data area allocates additional octets for its configured
*                   offset(s) [see 'net_if.c  NetIF_BufPoolInit()  Note #3'], the network buffer data
*                   area size does NOT need to be increased by the number of additional offset octets.
*
*           (6) Flags to configure (optional) device features; bit-field flags logically OR'd :
*
*               (a) NET_DEV_CFG_FLAG_NONE           No device configuration flags selected.
*
*               (b) NET_DEV_CFG_FLAG_SWAP_OCTETS    Swap data octets [i.e. swap data words' high-order
*                                                       octet(s) with data words' low-order octet(s),
*                                                       & vice-versa] if required by device-to-CPU data
*                                                       bus wiring &/or CPU endian word order.
*
*           (7) Network devices with receive descriptors MUST configure the number of receive buffers
*               greater than the number of receive descriptors.
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                EXAMPLE ETHERNET DEVICE CONFIGURATION
*********************************************************************************************************
*/

                                    /* ----------------- EXAMPLE ETHERNET DEVICE A, #1 CONFIGURATION ------------------ */

NET_DEV_CFG_USBD_CDC_EEM  NetDev_Cfg_Ether_USBD_CDCEEM = {

    NET_IF_MEM_TYPE_MAIN,           /* Desired receive  buffer memory pool type :                                       */
                                    /*   NET_IF_MEM_TYPE_MAIN        buffers allocated from main memory                 */
                                    /*   NET_IF_MEM_TYPE_DEDICATED   buffers allocated from (device's) dedicated memory */
    1518u,                          /* Desired size      of device's large receive  buffers (in octets) [see Note #2].  */
      10u,                          /* Desired number    of device's large receive  buffers             [see Note #3a]. */
    sizeof(CPU_ALIGN),              /* Desired alignment of device's       receive  buffers (in octets) [see Note #4].  */
       0u,                          /* Desired offset from base receive  index, if needed   (in octets) [see Note #5a1].*/


    NET_IF_MEM_TYPE_MAIN,           /* Desired transmit buffer memory pool type :                                       */
                                    /*   NET_IF_MEM_TYPE_MAIN        buffers allocated from main memory                 */
                                    /*   NET_IF_MEM_TYPE_DEDICATED   buffers allocated from (device's) dedicated memory */
    1518u,                          /* Desired size      of device's large transmit buffers (in octets) [see Note #2].  */
       2u,                          /* Desired number    of device's large transmit buffers             [see Note #3b]. */
      60u,                          /* Desired size      of device's small transmit buffers (in octets) [see Note #2].  */
       1u,                          /* Desired number    of device's small transmit buffers             [see Note #3b]. */
    USBD_CFG_BUF_ALIGN_OCTETS,      /* Desired alignment of device's       transmit buffers (in octets) [see Note #4].  */
       2u,                          /* Desired offset from base transmit index, if needed   (in octets) [see Note #5a2].*/


    0x00000000u,                    /* Base address   of dedicated memory, if available.                                */
             0u,                    /* Size           of dedicated memory, if available (in octets).                    */


    NET_DEV_CFG_FLAG_NONE,          /* Desired option flags, if any (see Note #6).                                      */

            "00:AB:CD:EF:80:01",    /* HW address.                                                                      */

             0u                     /* USBD CDC EEM class nbr. MUST be set at runtime after call to USBD_CDC_EEM_Add(). */
};


#endif  /* NET_IF_ETHER_MODULE_EN */

