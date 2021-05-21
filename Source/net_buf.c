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
*                                      NETWORK BUFFER MANAGEMENT
*
* Filename : net_buf.c
* Version  : V3.06.01
*********************************************************************************************************
*/

#include  "net_cfg_net.h"
#ifdef  NET_BUF_MODULE_EN


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    NET_BUF_MODULE
#include  "net_buf.h"
#include  "net_cfg_net.h"

#include  "net.h"
#include  "net_conn.h"
#include  "net_tcp.h"
#include  "net_util.h"
#include  "../IF/net_if.h"

#ifdef  NET_IF_LOOPBACK_MODULE_EN
#include  "../IF/net_if_loopback.h"
#endif

#ifdef  NET_IF_ETHER_MODULE_EN
#include  "../IF/net_if_ether.h"
#endif

#ifdef  NET_IF_WIFI_MODULE_EN
#include  "../IF/net_if_wifi.h"
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

static  NET_BUF_POOLS  NetBuf_PoolsTbl[NET_IF_NBR_IF_TOT];

static  NET_BUF_QTY    NetBuf_ID_Ctr;                      /* Global buf ID ctr.                                   */




/*
*********************************************************************************************************
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static  void  NetBuf_FreeHandler(NET_BUF        *p_buf);

static  void  NetBuf_ClrHdr     (NET_BUF_HDR    *p_buf_hdr);

static  void  NetBuf_Discard    (NET_IF_NBR      if_nbr,
                                 void           *p_buf,
                                 NET_STAT_POOL  *pstat_pool);


/*
*********************************************************************************************************
*                                            NetBuf_Init()
*
* Description : (1) Initialize Network Buffer Management Module :
*
*                   (a) Initialize network buffer pools
*                   (b) Initialize network buffer ID counter
*
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Net_Init().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetBuf_Init (void)
{
    NET_BUF_POOLS  *ppool;
    NET_IF_NBR      if_nbr;
    NET_ERR         err_stat;
    LIB_ERR         err_lib;

                                                                /* ------------------ INIT BUF POOLS ------------------ */
    if_nbr =  NET_IF_NBR_BASE;
    ppool  = &NetBuf_PoolsTbl[if_nbr];
    for ( ; if_nbr < NET_IF_NBR_IF_TOT; if_nbr++) {
                                                                /* Clr net buf mem  pools.                              */
        Mem_PoolClr(&ppool->NetBufPool,     &err_lib);
        Mem_PoolClr(&ppool->RxBufLargePool, &err_lib);
        Mem_PoolClr(&ppool->TxBufLargePool, &err_lib);
        Mem_PoolClr(&ppool->TxBufSmallPool, &err_lib);
                                                                /* Clr net buf stat pools.                              */
        NetStat_PoolClr(&ppool->NetBufStatPool,     &err_stat);
        NetStat_PoolClr(&ppool->RxBufLargeStatPool, &err_stat);
        NetStat_PoolClr(&ppool->TxBufLargeStatPool, &err_stat);
        NetStat_PoolClr(&ppool->TxBufSmallStatPool, &err_stat);
        ppool++;
    }

                                                                /* ------------------ INIT BUF ID CTR ----------------- */
    NetBuf_ID_Ctr = NET_BUF_ID_INIT;
}


/*
*********************************************************************************************************
*                                          NetBuf_PoolInit()
*
* Description : (1) Allocate & initialize a network buffer pool :
*
*                   (a) Allocate   network buffer pool
*                   (b) Initialize network buffer pool statistics
*
*
* Argument(s) : if_nbr              Interface number to initialize network buffer pools.
*
*               type                Network buffer pool type :
*
*                                       NET_BUF_TYPE_BUF        Network buffer                pool.
*                                       NET_BUF_TYPE_RX_LARGE   Network buffer large receive  pool.
*                                       NET_BUF_TYPE_TX_LARGE   Network buffer large transmit pool.
*                                       NET_BUF_TYPE_TX_SMALL   Network buffer small transmit pool.
*
*               pmem_base_addr      Network buffer memory pool base address :
*
*                                       Null (0) address        Network buffers allocated from general-
*                                                                   purpose heap.
*                                       Non-null address        Network buffers allocated from specified
*                                                                   base address of dedicated memory.
*
*               mem_size            Size      of network buffer memory pool to initialize (in octets).
*
*               blk_nbr             Number    of network buffer blocks      to initialize.
*
*               blk_size            Size      of network buffer blocks      to initialize (in octets).
*
*               blk_align           Alignment of network buffer blocks      to initialize (in octets).
*
*               pmem_pool           Pointer to memory pool structure.
*
*               poctets_reqd        Pointer to a variable to ... :
*
*                                       (a) Return the number of octets required to successfully
*                                               allocate the network buffer pool, if any error(s);
*                                       (b) Return 0, otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                Network buffer pool successfully initialized.
*                               NET_BUF_ERR_POOL_MEM_ALLOC           Network buffer pool initialization failed.
*                               NET_BUF_ERR_INVALID_POOL_TYPE   Invalid network buffer pool type.
*
*                                                               --- RETURNED BY NetIF_IsValidHandler() : ----
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
*                                                               ----- RETURNED BY NetStat_PoolInit() : ------
*                               NET_ERR_FAULT_NULL_PTR           Argument(s) passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_BufPoolInit().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetBuf_PoolInit (NET_IF_NBR   if_nbr,
                       NET_BUF_TYPE type,
                       void        *pmem_base_addr,
                       CPU_SIZE_T   mem_size,
                       CPU_SIZE_T   blk_nbr,
                       CPU_SIZE_T   blk_size,
                       CPU_SIZE_T   blk_align,
                       CPU_SIZE_T  *poctets_reqd,
                       NET_ERR     *p_err)
{
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL  *pstat_pool;
    MEM_POOL       *pmem_pool;
    LIB_ERR         err_lib;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, p_err);
    if (*p_err != NET_IF_ERR_NONE) {
         return;
    }
#endif

                                                                /* ------------------ INIT BUF POOL ------------------- */
    ppool = &NetBuf_PoolsTbl[if_nbr];
    switch (type) {
        case NET_BUF_TYPE_BUF:
             pmem_pool  = &ppool->NetBufPool;
             pstat_pool = &ppool->NetBufStatPool;
             break;


        case NET_BUF_TYPE_RX_LARGE:
             pmem_pool  = &ppool->RxBufLargePool;
             pstat_pool = &ppool->RxBufLargeStatPool;
             break;


        case NET_BUF_TYPE_TX_LARGE:
             pmem_pool  = &ppool->TxBufLargePool;
             pstat_pool = &ppool->TxBufLargeStatPool;
             break;


        case NET_BUF_TYPE_TX_SMALL:
             pmem_pool  = &ppool->TxBufSmallPool;
             pstat_pool = &ppool->TxBufSmallStatPool;
             break;


        case NET_BUF_TYPE_NONE:
        default:
            *p_err = NET_BUF_ERR_INVALID_POOL_TYPE;
             return;
    }

                                                                /* Create net buf mem pool.                             */
    Mem_PoolCreate(pmem_pool,
                   pmem_base_addr,
                   mem_size,
                   blk_nbr,
                   blk_size,
                   blk_align,
                   poctets_reqd,
                  &err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
       *p_err = NET_BUF_ERR_POOL_MEM_ALLOC;
        return;
    }


                                                                /* ---------------- INIT BUF STAT POOL ---------------- */
    NetStat_PoolInit((NET_STAT_POOL   *)pstat_pool,
                     (NET_STAT_POOL_QTY)blk_nbr,
                     (NET_ERR         *)p_err);
    if (*p_err != NET_STAT_ERR_NONE) {
         return;
    }


   *p_err = NET_BUF_ERR_NONE;
}


NET_BUF_POOLS  *NetBuf_PoolsGet (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *p_pool;


    p_pool = &NetBuf_PoolsTbl[if_nbr];


    return (p_pool);
}


/*
*********************************************************************************************************
*                                       NetBuf_PoolCfgValidate()
*
* Description : (1) Validate network buffer pool configuration :
*
*                   (a) Validate configured number of network buffers
*                   (b) Validate configured size   of network buffers
*
*
* Argument(s) : pdev_cfg    Pointer to network interface's device configuration.
*               --------    Argument validated in NetIF_BufPoolInit().
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                    Network buffer pool configuration valid.
*                               NET_BUF_ERR_INVALID_POOL_TYPE       Invalid network buffer pool type.
*                               NET_BUF_ERR_INVALID_POOL_ADDR       Invalid network buffer pool address.
*                               NET_BUF_ERR_INVALID_POOL_SIZE       Invalid network buffer pool size.
*                               NET_BUF_ERR_INVALID_CFG_RX_NBR        Invalid network buffer pool number of
*                                                                       buffers configured.
*                               NET_BUF_ERR_INVALID_SIZE            Invalid network buffer data area size
*                                                                       configured.
*                               NET_BUF_ERR_INVALID_IX              Invalid offset from base index into
*                                                                       network buffer data area configured.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_BufPoolInit().
*
*               This function is an INTERNAL network protocol suite function & MUST NOT be called by
*               application function(s).
*
* Note(s)     : (2) Each network interface/device MUST configure :
*
*                   (a) At least one  (1) large receive  buffer.
*                   (b) At least one  (1)       transmit buffer; however, zero (0) large OR zero (0)
*                          small transmit buffers MAY be configured.
*
*               (3) (a) All   network  buffer data area sizes MUST be configured greater than or equal
*                       to NET_BUF_DATA_SIZE_MIN.
*
*                   (b) Large transmit buffer data area sizes MUST be configured greater than or equal
*                       to small transmit buffer data area sizes.
*********************************************************************************************************
*/

void  NetBuf_PoolCfgValidate (NET_IF_TYPE   if_type,
                              NET_DEV_CFG  *pdev_cfg,
                              NET_ERR      *p_err)
{
    NET_BUF_QTY  nbr_bufs_tx;
    CPU_INT32U   rx_buf_size_min;
    CPU_INT32U   tx_buf_size_min;


                                                                    /* -------------- VALIDATE NBR BUFS --------------- */
    if (pdev_cfg->RxBufLargeNbr < NET_BUF_NBR_RX_LARGE_MIN) {       /* Validate nbr rx bufs (see Note #2a).             */
       *p_err = NET_BUF_ERR_INVALID_CFG_RX_NBR;
        return;
    }
                                                                    /* Validate nbr tx bufs (see Note #2b).             */
    nbr_bufs_tx = pdev_cfg->TxBufLargeNbr +
                  pdev_cfg->TxBufSmallNbr;
    if (nbr_bufs_tx < NET_BUF_NBR_TX_MIN) {
       *p_err = NET_BUF_ERR_INVALID_CFG_TX_NBR;
        return;
    }

    switch (if_type) {
        case NET_IF_TYPE_LOOPBACK:
#ifdef NET_IF_LOOPBACK_MODULE_EN
             rx_buf_size_min = NET_IF_LOOPBACK_BUF_RX_LEN_MIN;
             tx_buf_size_min = NET_IF_LOOPBACK_BUF_TX_LEN_MIN;
             break;
#else
            *p_err = NET_ERR_FAULT_FEATURE_DIS;
             goto exit;
#endif


        case NET_IF_TYPE_ETHER:
#ifdef NET_IF_ETHER_MODULE_EN
             rx_buf_size_min = NET_IF_ETHER_BUF_RX_LEN_MIN;
             tx_buf_size_min = NET_IF_ETHER_BUF_TX_LEN_MIN;
             break;
#else
            *p_err = NET_ERR_FAULT_FEATURE_DIS;
             goto exit;
#endif


        case NET_IF_TYPE_WIFI:
#ifdef NET_IF_WIFI_MODULE_EN
             rx_buf_size_min = NET_IF_WIFI_BUF_RX_LEN_MIN;
             tx_buf_size_min = NET_IF_WIFI_BUF_TX_LEN_MIN;
             break;
#else
            *p_err = NET_ERR_FAULT_FEATURE_DIS;
             goto exit;
#endif


        case NET_IF_TYPE_SERIAL:
        case NET_IF_TYPE_PPP:
        case NET_IF_TYPE_NONE:
        default:
           *p_err = NET_ERR_FAULT_NOT_SUPPORTED;
            goto exit;
    }


                                                                    /* ----------- VALIDATE BUF DATA SIZES ------------ */
                                                                    /* See Note #3a.                                    */
    if (pdev_cfg->RxBufLargeSize < rx_buf_size_min) {               /* Validate large rx buf data size.                 */
       *p_err = NET_BUF_ERR_INVALID_SIZE;
        goto exit;
    }


    if (pdev_cfg->TxBufLargeNbr > 0) {                              /* If any large tx bufs cfg'd, ...                  */
        if (pdev_cfg->TxBufLargeSize < tx_buf_size_min) {           /* ... validate large tx buf size (see Note #3a).   */
           *p_err = NET_BUF_ERR_INVALID_SIZE;
            goto exit;
        }
    }

    if (pdev_cfg->TxBufSmallNbr > 0) {                              /* If any small tx bufs cfg'd, ...                  */
        if (pdev_cfg->TxBufSmallSize < tx_buf_size_min) {           /* ... validate small tx buf size (see Note #3a).   */
           *p_err = NET_BUF_ERR_INVALID_SIZE;
            goto exit;
        }
    }

    if ((pdev_cfg->TxBufLargeNbr > 0) &&                            /* If both large tx bufs                     ...    */
        (pdev_cfg->TxBufSmallNbr > 0)) {                            /* ... AND small tx bufs cfg'd,              ...    */
        if (pdev_cfg->TxBufLargeSize < pdev_cfg->TxBufSmallSize) {  /* ... validate large vs. small tx buf sizes ...    */
           *p_err = NET_BUF_ERR_INVALID_SIZE;                       /* ... (see Note #3b).                              */
            goto exit;
        }
    }


                                                                    /* --------- VALIDATE BUF DATA IX OFFSETS --------- */
                                                                    /* Validate rx buf data ix offset.                  */
                                                                    /* Validate tx buf data ix offset.                  */


   *p_err = NET_BUF_ERR_NONE;

exit:
    return;
}


/*
*********************************************************************************************************
*                                            NetBuf_Get()
*
* Description : (1) Allocate & initialize a network buffer :
*
*                   (a) Get        network buffer
*                       (1) For transmit operations, also get network buffer data area
*                   (b) Initialize network buffer
*                   (c) Update     network buffer pool statistics
*                   (d) Return pointer to buffer
*                         OR
*                       Null pointer & error code, on failure
*
*
* Argument(s) : if_nbr          Interface number to get network buffer.
*
*               transaction     Transaction type :
*
*                                   NET_TRANSACTION_RX          Receive  transaction.
*                                   NET_TRANSACTION_TX          Transmit transaction.
*
*               size            Requested buffer size  to store buffer data (see Note #2).
*
*               ix              Requested buffer index to store buffer data; MUST NOT be pre-adjusted by
*                                   interface's configured index offset(s)  [see Note #3].
*
*               pix_offset      Pointer to a variable to ... :
*
*                                   (a) Return the interface's receive/transmit index offset, if NO error(s);
*                                   (b) Return 0,                                             otherwise.
*
*               flags           Flags to select buffer options; bit-field flags logically OR'd :
*
*                                   NET_BUF_FLAG_NONE           NO buffer flags selected.
*                                   NET_BUF_FLAG_CLR_MEM        Clear buffer memory (i.e. set each buffer
*                                                                   data octet to 0x00).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                Network buffer successfully allocated &
*                                                                   initialized.
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
*                                                               ---- RETURNED BY NetBuf_GetDataPtr() : ----
*                               NET_BUF_ERR_INVALID_IX          Invalid index.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size.
*                               NET_BUF_ERR_INVALID_LEN         Invalid requested size & start index.
*
*                                                               -------- RETURNED BY NetIF_Get() : --------
*                               NET_IF_ERR_INVALID_IF           Invalid network interface number.
*
* Return(s)   : Pointer to network buffer, if NO error(s).
*
*               Pointer to NULL,           otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) 'size' of 0 octets allowed.
*
*                   See also 'NetBuf_GetDataPtr()  Note #3'.
*
*               (3) 'ix' argument automatically adjusted for interface's configured network buffer data
*                   area index offset(s) & MUST NOT be pre-adjusted by caller function(s).
*
*                   See also 'NetBuf_GetDataPtr()  Note #4'.
*
*               (4) (a) Buffer memory cleared                in NetBuf_GetDataPtr(), but ...
*                   (b) buffer flag NET_BUF_FLAG_CLR_MEM set in NetBuf_Get().
*
*                   See also 'NetBuf_GetDataPtr()  Note #7'.
*
*               (5) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*********************************************************************************************************
*/

NET_BUF  *NetBuf_Get (NET_IF_NBR        if_nbr,
                      NET_TRANSACTION   transaction,
                      NET_BUF_SIZE      size,
                      NET_BUF_SIZE      ix,
                      NET_BUF_SIZE     *pix_offset,
                      NET_BUF_FLAGS     flags,
                      NET_ERR          *p_err)
{
    NET_IF         *pif;
    NET_DEV_CFG    *pdev_cfg;
    NET_BUF        *p_buf;
    NET_BUF_HDR    *p_buf_hdr;
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL  *pstat_pool;
    MEM_POOL       *pmem_pool;
    NET_BUF_SIZE    ix_offset_unused;
    NET_BUF_SIZE    data_size;
    NET_BUF_TYPE    type;
    NET_ERR         err_stat;
    LIB_ERR         err_lib;


    if (pix_offset == (NET_BUF_SIZE *) 0) {                     /* If NOT avail, ...                                    */
        pix_offset  = (NET_BUF_SIZE *)&ix_offset_unused;        /* ... re-cfg NULL rtn ptr to unused local var.         */
       (void)&ix_offset_unused;                                 /* Prevent possible 'variable unused' warning.          */
    }
   *pix_offset = 0u;                                            /* Init ix for err (see Note #5).                       */


                                                                /* --------------------- GET BUF ---------------------- */
    pif = NetIF_Get(if_nbr, p_err);
    if (*p_err !=  NET_IF_ERR_NONE) {
         return ((NET_BUF *)0);
    }

    pdev_cfg   = (NET_DEV_CFG   *) pif->Dev_Cfg;
    ppool      = (NET_BUF_POOLS *)&NetBuf_PoolsTbl[if_nbr];
    pmem_pool  = (MEM_POOL      *)&ppool->NetBufPool;
    pstat_pool = (NET_STAT_POOL *)&ppool->NetBufStatPool;
    p_buf      = (NET_BUF       *) Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                                  (CPU_SIZE_T) sizeof(NET_BUF),
                                                  (LIB_ERR  *)&err_lib);
    if (err_lib != LIB_MEM_ERR_NONE) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NoneAvailCtr);
       *p_err =   NET_BUF_ERR_NONE_AVAIL;
        return ((NET_BUF *)0);
    }


                                                                /* --------------------- INIT BUF --------------------- */
    p_buf_hdr = &p_buf->Hdr;
    NetBuf_ClrHdr(p_buf_hdr);
    DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_USED);           /* Set buf as used.                                     */
#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    DEF_BIT_SET(p_buf_hdr->Flags, NET_BUF_FLAG_CLR_MEM);        /* Set buf data area as clr (see Note #4).              */
#endif
    NET_BUF_GET_ID(p_buf_hdr->ID);                              /* Get buf ID.                                          */
    p_buf_hdr->RefCtr   = 1u;                                   /* Set ref ctr to 1; NetBuf_Get() caller is first ref.  */
    p_buf_hdr->IF_Nbr   = if_nbr;                               /* Set buf's IF nbrs.                                   */
    p_buf_hdr->IF_NbrTx = if_nbr;
   (void)&flags;                                                /* Prevent 'variable unused' warning (see Note #6).     */

    switch (transaction) {
        case NET_TRANSACTION_RX:                                /* Cfg buf for prev'ly alloc'd rx buf data area.        */
             p_buf_hdr->Type = NET_BUF_TYPE_RX_LARGE;
             p_buf_hdr->Size = pdev_cfg->RxBufLargeSize + pdev_cfg->RxBufIxOffset;
            *pix_offset     = pdev_cfg->RxBufIxOffset;

             ;                                                  /* Ptr to rx buf data area MUST be linked by caller.    */
             break;


        case NET_TRANSACTION_TX:                                /* Get/cfg tx buf & data area.                          */
             p_buf->DataPtr = NetBuf_GetDataPtr(pif,
                                                transaction,
                                                size,
                                                ix,
                                                pix_offset,
                                               &data_size,
                                               &type,
                                                p_err);
             if (*p_err != NET_BUF_ERR_NONE) {
                  Mem_PoolBlkFree(pmem_pool,
                                  p_buf,
                                 &err_lib);
                  if (err_lib == LIB_MEM_ERR_NONE) {
                      NetStat_PoolEntryUsedDec(pstat_pool, &err_stat);
                  } else {
                      NetBuf_Discard(if_nbr,
                                     p_buf,
                                     pstat_pool);
                  }
                  return ((NET_BUF *)0);
             }

             p_buf_hdr->Type = type;
             p_buf_hdr->Size = data_size + *pix_offset;
             break;


        case NET_TRANSACTION_NONE:
        default:
             Mem_PoolBlkFree((MEM_POOL *) pmem_pool,
                             (void     *) p_buf,
                             (LIB_ERR  *)&err_lib);
             if (err_lib == LIB_MEM_ERR_NONE) {
                 NetStat_PoolEntryUsedDec(pstat_pool, &err_stat);
             } else {
                 NetBuf_Discard((NET_IF_NBR     )if_nbr,
                                (void          *)p_buf,
                                (NET_STAT_POOL *)pstat_pool);
             }
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTransactionTypeCtr);
            *p_err =   NET_ERR_INVALID_TRANSACTION;
             return ((NET_BUF *)0);
    }


                                                                /* -------------- UPDATE BUF POOL STATS --------------- */
    NetStat_PoolEntryUsedInc(pstat_pool, &err_stat);


   *p_err =  NET_BUF_ERR_NONE;

    return (p_buf);                                             /* --------------------- RTN BUF ---------------------- */
}


/*
*********************************************************************************************************
*                                         NetBuf_GetDataPtr()
*
* Description : (1) Get network buffer data area of sufficient size :
*
*                   (a) Get    network buffer data area
*                   (b) Update network buffer data area pool statistics
*                   (c) Return pointer to network buffer data area
*                         OR
*                       Null pointer & error code, on failure
*
*
* Argument(s) : pif             Pointer to interface to get network buffer data area.
*
*               transaction     Transaction type :
*
*                                   NET_TRANSACTION_RX          Receive  transaction.
*                                   NET_TRANSACTION_TX          Transmit transaction.
*
*               size            Requested buffer size  to store buffer data (see Note #3).
*
*               ix_start        Requested buffer index to store buffer data; MUST NOT be pre-adjusted by
*                                   interface's configured index offset(s)  [see Note #4].
*
*               pix_offset      Pointer to a variable to ... :
*
*                                   (a) Return the interface's receive/transmit index offset, if NO error(s);
*                                   (b) Return 0,                                             otherwise.
*
*               p_data_size     Pointer to a variable to ... :
*
*                                   (a) Return the size of the network buffer data area, if NO error(s);
*                                   (b) Return 0,                                        otherwise.
*
*               ptype           Pointer to a variable to ... :
*
*                                   (a) Return the network buffer type, if NO error(s);
*                                   (b) Return NET_BUF_TYPE_NONE,       otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                Network buffer data area successfully allocated
*                                                                   & initialized.
*                               NET_BUF_ERR_NONE_AVAIL          NO available buffer data areas to allocate.
*                               NET_BUF_ERR_INVALID_IX          Invalid index.
*                               NET_BUF_ERR_INVALID_SIZE        Invalid size; less than 0 or greater than the
*                                                                   maximum network buffer data area size
*                                                                   available.
*                               NET_BUF_ERR_INVALID_LEN         Requested size & start index overflows network
*                                                                   buffer's data area.
*                               NET_ERR_INVALID_TRANSACTION     Invalid transaction type.
*
* Return(s)   : Pointer to network buffer data area, if NO error(s).
*
*               Pointer to NULL,                     otherwise.
*
* Caller(s)   : NetBuf_Get(),
*               various device driver handler functions.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) but MAY be called by device driver handler function(s).
*
* Note(s)     : (2) 'size' & 'ix' argument check NOT required unless 'NET_BUF_SIZE's native data type
*                   'CPU_INT16U' is incorrectly configured as a signed integer in 'cpu.h'.
*
*               (3) 'size' of 0 octets allowed.
*
*               (4) 'ix_start' argument automatically adjusted for interface's configured network buffer
*                   data area index offset(s) & MUST NOT be pre-adjusted by caller function(s).
*
*               (5) Pointers to variables that return values MUST be initialized PRIOR to all other
*                   validation or function handling in case of any error(s).
*
*               (6) Network transmit buffers are allocated an appropriately-sized network buffer data area
*                   based on the total requested buffer size & index :
*
*                   (a) A small transmit buffer data area will be allocated if :
*                       (1) any small transmit  buffer data areas are available, AND ...
*                       (2) the total requested buffer size & index is less than or equal to small transmit
*                               buffers' configured data area size.
*
*                   (b) A large transmit buffer data area will be allocated if :
*                       (1)     NO  small transmit  buffer data areas are available; OR  ...
*                       (2) (A) any large transmit  buffer data areas are available, AND ...
*                           (B) the total requested buffer size & index is :
*                               (1) greater than small transmit buffers' configured data area size AND ...
*                               (2) less than or equal to large transmit buffers' configured data area size.
*
*                           See also 'NetBuf_PoolCfgValidate()  Note #3b'.
*
*               (7) Since each network buffer data area allocates additional octets for its configured
*                   offset(s) [see 'net_if.c  NetIF_BufPoolInit()  Note #3'], the network buffer data
*                   area size does NOT need to be adjusted by the number of additional offset octets.
*
*               (8) Buffer memory cleared in NetBuf_GetDataPtr() instead of in NetBuf_Free() handlers so
*                   that the data in any freed buffer data area may be inspected until that buffer data
*                   area is next allocated.
*********************************************************************************************************
*/

CPU_INT08U  *NetBuf_GetDataPtr (NET_IF           *pif,
                                NET_TRANSACTION   transaction,
                                NET_BUF_SIZE      size,
                                NET_BUF_SIZE      ix_start,
                                NET_BUF_SIZE     *pix_offset,
                                NET_BUF_SIZE     *p_data_size,
                                NET_BUF_TYPE     *ptype,
                                NET_ERR          *p_err)
{
    NET_DEV_CFG    *pdev_cfg;
    CPU_INT08U     *p_data;
    NET_STAT_POOL  *pstat_pool;
    NET_BUF_POOLS  *ppool;
    MEM_POOL       *pmem_pool;
    NET_BUF_SIZE    ix_offset;
    NET_BUF_SIZE    ix_offset_unused;
    NET_BUF_SIZE    data_size_unused;
    NET_BUF_SIZE    size_len;
    NET_BUF_SIZE    size_data;
    NET_BUF_TYPE    type;
    NET_BUF_TYPE    type_unused;
    NET_ERR         err_stat;
    LIB_ERR         err_lib;

                                                                /* Init rtn vals for err (see Note #5).                 */
    if (pix_offset  == (NET_BUF_SIZE *) 0) {                    /* If NOT avail, ...                                    */
        pix_offset   = (NET_BUF_SIZE *)&ix_offset_unused;       /* ... re-cfg NULL rtn ptr to unused local var.         */
       (void)&ix_offset_unused;                                 /* Prevent possible 'variable unused' warning.          */
    }
   *pix_offset  = 0u;

    if (p_data_size == (NET_BUF_SIZE *) 0) {                    /* If NOT avail, ...                                    */
        p_data_size  = (NET_BUF_SIZE *)&data_size_unused;       /* ... re-cfg NULL rtn ptr to unused local var.         */
       (void)&data_size_unused;                                 /* Prevent possible 'variable unused' warning.          */
    }
   *p_data_size = 0u;

    if (ptype == (NET_BUF_TYPE *) 0) {                          /* If NOT avail, ...                                    */
        ptype  = (NET_BUF_TYPE *)&type_unused;                  /* ... re-cfg NULL rtn ptr to unused local var.         */
       (void)&type_unused;                                      /* Prevent possible 'variable unused' warning.          */
    }
   *ptype = NET_BUF_TYPE_NONE;


                                                                /* ------------------ VALIDATE IX's ------------------- */
    pdev_cfg = (NET_DEV_CFG *)pif->Dev_Cfg;
    switch (transaction) {
        case NET_TRANSACTION_RX:
             ix_offset = pdev_cfg->RxBufIxOffset;
             break;


        case NET_TRANSACTION_TX:
             ix_offset = pdev_cfg->TxBufIxOffset;
             break;


        case NET_TRANSACTION_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTransactionTypeCtr);
            *p_err = NET_ERR_INVALID_TRANSACTION;
             return ((CPU_INT08U *)0);
    }

#if 0                                                           /* See Note #2.                                         */
    if (ix_start < 0) {                                         /* If neg ix's req'd/cfg'd, rtn err.                    */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return ((CPU_INT08U *)0);
    }
    if (ix_offset < 0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return ((CPU_INT08U *)0);
    }
#endif


                                                                /* ------------------ VALIDATE SIZE ------------------- */
#if 0                                                           /* See Note #2.                                         */
    if (size < 0) {                                             /* If neg size req'd, rtn err.                          */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.SizeCtr);
       *p_err = NET_BUF_ERR_INVALID_SIZE;
        return ((CPU_INT08U *)0);
    }
#endif

    size_len = size + ix_start;                                 /* Calc tot req'd size from start ix (see Note #7).     */
                                                                /* Discard possible size len ovf's.                     */
    if (size_len < size) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return ((CPU_INT08U *)0);
    }
    if (size_len < ix_start) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return ((CPU_INT08U *)0);
    }


                                                                /* ---------------- GET BUF DATA AREA ----------------- */
    ppool     = (NET_BUF_POOLS *)&NetBuf_PoolsTbl[pif->Nbr];
    p_data    = (CPU_INT08U    *) 0;
    size_data = (NET_BUF_SIZE   ) 0u;

    switch (transaction) {
        case NET_TRANSACTION_RX:
             if (size_len  <=  pdev_cfg->RxBufLargeSize) {
                 size_data  =  pdev_cfg->RxBufLargeSize;
                 type       =  NET_BUF_TYPE_RX_LARGE;
                 pmem_pool  = &ppool->RxBufLargePool;
                 pstat_pool = &ppool->RxBufLargeStatPool;

                 p_data     = (CPU_INT08U *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                                           (CPU_SIZE_T) size_len,
                                                           (LIB_ERR  *)&err_lib);

             } else {
                 NET_CTR_ERR_INC(Net_ErrCtrs.Buf.SizeCtr);
                *p_err = NET_BUF_ERR_INVALID_SIZE;
                 return ((CPU_INT08U *)0);
             }
             break;


        case NET_TRANSACTION_TX:
             if ((pdev_cfg->TxBufSmallNbr  >  0) &&             /* If small tx bufs avail         (see Note #6a1) & ..  */
                 (pdev_cfg->TxBufSmallSize >= size_len)) {      /* .. small tx buf  >= req'd size (see Note #6a2),  ..  */
                  size_data  =  pdev_cfg->TxBufSmallSize;
                  type       =  NET_BUF_TYPE_TX_SMALL;
                  pmem_pool  = &ppool->TxBufSmallPool;
                  pstat_pool = &ppool->TxBufSmallStatPool;
                                                                /* .. get a small tx buf data area.                     */
                  p_data     = (CPU_INT08U *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                                            (CPU_SIZE_T) size_len,
                                                            (LIB_ERR  *)&err_lib);
             }

             if ((p_data == (CPU_INT08U *)0)     &&             /* If small tx bufs NOT avail     (see Note #6b1); OR ..*/
                 (pdev_cfg->TxBufLargeNbr  >  0) &&             /* .. large tx bufs     avail     (see Note #6b2A) &  ..*/
                 (pdev_cfg->TxBufLargeSize >= size_len)) {      /* .. large tx buf  >= req'd size (see Note #6b2B),   ..*/
                  size_data  =  pdev_cfg->TxBufLargeSize;
                  type       =  NET_BUF_TYPE_TX_LARGE;
                  pmem_pool  = &ppool->TxBufLargePool;
                  pstat_pool = &ppool->TxBufLargeStatPool;
                                                                /* .. get a large tx buf data area.                     */
                  p_data     = (CPU_INT08U *)Mem_PoolBlkGet((MEM_POOL *) pmem_pool,
                                                            (CPU_SIZE_T) size_len,
                                                            (LIB_ERR  *)&err_lib);
             }

             if (size_len > size_data) {                        /* If tot req'd size > avail buf size, ...              */
                 NET_CTR_ERR_INC(Net_ErrCtrs.Buf.SizeCtr);
                *p_err = NET_BUF_ERR_INVALID_SIZE;              /* ... rtn err.                                         */
                 return ((CPU_INT08U *)0);
             }
             break;


        case NET_TRANSACTION_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTransactionTypeCtr);
            *p_err = NET_ERR_INVALID_TRANSACTION;
             return ((CPU_INT08U *)0);
    }


    if (p_data == (CPU_INT08U *)0) {                            /* If NO appropriately-sized data area avail, ...       */
       *p_err = NET_BUF_ERR_NONE_AVAIL;                         /* ... rtn err.                                         */
        return ((CPU_INT08U *)0);
    }


#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)                     /* Clr ALL buf data octets (see Note #8).               */
    Mem_Clr((void     *)p_data,
            (CPU_SIZE_T)size_data);
#endif


                                                                /* -------------- UPDATE BUF POOL STATS --------------- */
    NetStat_PoolEntryUsedInc(pstat_pool, &err_stat);


                                                                /* ---------------- RTN BUF DATA AREA ----------------- */
   *pix_offset  = ix_offset;
   *p_data_size = size_data;
   *ptype       = type;


   *p_err =  NET_BUF_ERR_NONE;

    return (p_data);
}


/*
*********************************************************************************************************
*                                         NetBuf_GetMaxSize()
*
* Description : Get maximum possible buffer allocation size starting at a specific buffer index.
*
* Argument(s) : if_nbr          Interface number to get maximum network buffer size.
*
*               transaction     Transaction type :
*
*                                   NET_TRANSACTION_RX          Receive  transaction.
*                                   NET_TRANSACTION_TX          Transmit transaction.
*
*               p_buf           Pointer to a network buffer.
*
*               ix_start        Requested buffer index to store buffer data.
*
* Return(s)   : Maximum buffer size for a specified network buffer or interface, if NO error(s).
*
*               0,                                                               otherwise.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Although network buffers' data area MAY be declared with an additional CPU word size
*                   (see 'net_buf.h  NETWORK BUFFER DATA TYPE  Note #2b'), this additional CPU word size
*                   does NOT increase the overall useable network buffer data area size.
*
*               (2) Since each network buffer data area allocates additional octets for its configured
*                   offset(s) [see 'net_if.c  NetIF_BufPoolInit()  Note #3'], the network buffer data
*                   area size does NOT need to be adjusted by the number of additional offset octets.
*********************************************************************************************************
*/

NET_BUF_SIZE  NetBuf_GetMaxSize (NET_IF_NBR        if_nbr,
                                 NET_TRANSACTION   transaction,
                                 NET_BUF          *p_buf,
                                 NET_BUF_SIZE      ix_start)
{
    NET_IF        *pif;
    NET_BUF_HDR   *p_buf_hdr;
    NET_DEV_CFG   *pdev_cfg;
    NET_BUF_SIZE   max_size;
    NET_ERR        err;


    max_size = 0u;

    if (p_buf != DEF_NULL) {                                /* Chk p_buf's max size.                                */
        p_buf_hdr = &p_buf->Hdr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
        switch (p_buf_hdr->Type) {
            case NET_BUF_TYPE_BUF:
            case NET_BUF_TYPE_RX_LARGE:
            case NET_BUF_TYPE_TX_LARGE:
            case NET_BUF_TYPE_TX_SMALL:
                 break;


            case NET_BUF_TYPE_NONE:
            default:
                 NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
                 return (0u);
        }
#endif

        if (ix_start < p_buf_hdr->Size) {
            max_size = p_buf_hdr->Size - ix_start;
        }

    } else {                                                    /* Else chk specific IF's cfg'd max buf size.           */
        pif = NetIF_Get(if_nbr, &err);
        if (err != NET_IF_ERR_NONE) {
            return (0u);
        }

        pdev_cfg = (NET_DEV_CFG *)pif->Dev_Cfg;
        switch (transaction) {
            case NET_TRANSACTION_RX:
                 if (pdev_cfg->RxBufLargeNbr > 0u) {
                     if (ix_start < pdev_cfg->RxBufLargeSize) {
                         max_size = pdev_cfg->RxBufLargeSize - ix_start;
                     }
                 }
                 break;


            case NET_TRANSACTION_TX:
                 if (pdev_cfg->TxBufLargeNbr > 0u) {
                     if (ix_start < pdev_cfg->TxBufLargeSize) {
                         max_size = pdev_cfg->TxBufLargeSize - ix_start;
                     }
                 } else if (pdev_cfg->TxBufSmallNbr > 0u) {
                     if (ix_start < pdev_cfg->TxBufSmallSize) {
                         max_size = pdev_cfg->TxBufSmallSize - ix_start;
                     }
                 } else {
                                                                /* Empty Else Statement                                 */
                 }
                 break;


            case NET_TRANSACTION_NONE:
            default:
                 NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTransactionTypeCtr);
                 return (0u);
        }
    }


    return (max_size);
}


/*
*********************************************************************************************************
*                                            NetBuf_Free()
*
* Description : (1) Free a network buffer :
*
*                   (a) Free network   buffer
*                   (b) Free IP option buffer                   See Note #2
*
*
* Argument(s) : p_buf       Pointer to a network buffer.
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Since any single IP packet requires only a single network buffer to receive IP
*                   options (see 'net_ip.c  NetIP_RxPktValidate()  Note #1bC'), then no more than ONE
*                   network buffer should be linked as an IP options buffer from another buffer.
*********************************************************************************************************
*/

void  NetBuf_Free (NET_BUF  *p_buf)
{
#ifdef  NET_IPv4_MODULE_EN
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF      *p_buf_ip_opt;
#endif

                                                                /* ------------------- VALIDATE PTR ------------------- */
    if (p_buf == (NET_BUF *)0) {
        return;
    }

                                                                /* ------------------ FREE NET BUF(s) ----------------- */
#ifdef  NET_IPv4_MODULE_EN
    p_buf_hdr    = &p_buf->Hdr;
    p_buf_ip_opt =  p_buf_hdr->IP_OptPtr;
#endif

    NetBuf_FreeHandler(p_buf);                                  /* Free net buf.                                        */
#ifdef  NET_IPv4_MODULE_EN
    if (p_buf_ip_opt != (NET_BUF *)0) {                         /* If avail, ...                                        */
        NetBuf_FreeHandler(p_buf_ip_opt);                       /* ... free IP opt buf (see Note #2).                   */
    }
#endif
}


/*
*********************************************************************************************************
*                                          NetBuf_FreeBuf()
*
* Description : Free a network buffer.
*
* Argument(s) : p_buf       Pointer to a network buffer.
*
*               pctr        Pointer to possible error counter.
*
* Return(s)   : Number of network buffers freed.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Buffers are NOT validated for 'Type' or 'USED' before freeing. #### NET-808
*
*                   See also 'NetBuf_FreeHandler()  Note #2'.
*
*               (2) Buffers may be referenced by multiple layers.  Therefore, the buffer's reference
*                   counter MUST be checked before freeing the buffer.
*********************************************************************************************************
*/

NET_BUF_QTY  NetBuf_FreeBuf (NET_BUF  *p_buf,
                             NET_CTR  *pctr)
{
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_QTY   nbr_freed;


    nbr_freed = 0u;

    if (p_buf != (NET_BUF *)0) {
        p_buf_hdr = &p_buf->Hdr;
        if (p_buf_hdr->RefCtr > 1) {                            /* If     buf ref'd by multiple layers (see Note #2), ..*/
            p_buf_hdr->RefCtr--;                                /* .. dec buf ref ctr.                                  */
        } else {                                                /* Else free buf.                                       */
            NetBuf_Free(p_buf);
        }

        if (pctr != (NET_CTR *)0) {                             /* If avail, ...                                        */
            NET_CTR_ERR_INC(*pctr);                             /* ... inc err ctr.                                     */
        }

        nbr_freed++;
    }

    return (nbr_freed);
}


/*
*********************************************************************************************************
*                                        NetBuf_FreeBufList()
*
* Description : Free a network buffer list.
*
*               (1) Network buffer lists are implemented as doubly-linked lists :
*
*                   (a) 'p_buf_list' points to the head of the buffer list.
*
*                   (b) Buffer's 'PrevBufPtr' & 'NextBufPtr' doubly-link each buffer in a buffer list.
*
*
*                                     ---        Head of         -------
*                                      ^       Buffer List  ---->|     |
*                                      |                         |     |
*                                      |     (see Note #1a)      |     |
*                                      |                         |     |
*                                      |                         |     |
*                                      |                         -------
*                                      |                           | ^
*                                      |                           | |
*                                      |                           v |
*                                                                -------
*                                 Buffer List                    |     |
*                                (see Note #1)                   |     |
*                                                                |     |
*                                      |                         |     |
*                                      |                         |     |
*                                      |                         -------
*                                      |                           | ^
*                                      |           NextBufPtr ---> | | <--- PrevBufPtr
*                                      |         (see Note #1b)    v |    (see Note #1b)
*                                      |                         -------
*                                      |                         |     |
*                                      v                         |     |
*                                     ---                        -------
*
*
* Argument(s) : p_buf_list   Pointer to a buffer list.
*
*               pctr        Pointer to possible error counter.
*
* Return(s)   : Number of network buffers freed.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Buffers are NOT validated for 'Type' or 'USED' before freeing. #### NET-808
*
*                   See also 'NetBuf_FreeHandler()  Note #2'.
*
*               (3) Buffers may be referenced by multiple layers.  Therefore, the buffer's reference
*                   counter MUST be checked before freeing the buffer.
*
*               (4) Buffers NOT freed are unlinked from other buffer fragment lists & compressed within
*                   their own buffer list.  Ideally, buffer fragment lists SHOULD NEVER be compressed
*                   but should be unlinked in their entirety.
*********************************************************************************************************
*/

NET_BUF_QTY  NetBuf_FreeBufList (NET_BUF  *p_buf_list,
                                 NET_CTR  *pctr)
{
    NET_BUF      *p_buf;
    NET_BUF      *p_buf_prev;
    NET_BUF      *p_buf_next;
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_HDR  *p_buf_prev_hdr;
    NET_BUF_QTY   nbr_freed;


    p_buf      = (NET_BUF   *)p_buf_list;
    p_buf_prev = (NET_BUF   *)0;
    nbr_freed  = (NET_BUF_QTY)0u;

    while (p_buf  != (NET_BUF *)0) {                            /* Free ALL bufs in buf list.                           */
        p_buf_hdr  = &p_buf->Hdr;
        p_buf_next =  p_buf_hdr->NextBufPtr;

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
        p_buf_hdr->PrevPrimListPtr = (NET_BUF *)0;
        p_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
#endif

        if (p_buf_hdr->RefCtr > 1) {                            /* If     buf ref'd by multiple layers (see Note #3), ..*/
            p_buf_hdr->RefCtr--;                                /* .. dec buf ref ctr.                                  */
            p_buf_hdr->PrevBufPtr = (NET_BUF *)p_buf_prev;
            p_buf_hdr->NextBufPtr = (NET_BUF *)0;
            if (p_buf_prev != (NET_BUF *)0) {                   /* If prev buf non-NULL, ...                            */
                p_buf_prev_hdr             = &p_buf_prev->Hdr;
                p_buf_prev_hdr->NextBufPtr =  p_buf;            /* ... set prev buf's next ptr to cur buf.              */
            }
            p_buf_prev = p_buf;                                 /* Set cur buf as new prev buf (see Note #4).           */

        } else {                                                /* Else free buf.                                       */
            NetBuf_Free(p_buf);
        }

        if (pctr != (NET_CTR *)0) {                             /* If avail, ...                                        */
            NET_CTR_ERR_INC(*pctr);                             /* ... inc err ctr.                                     */
        }

        nbr_freed++;

        p_buf = p_buf_next;
    }

    return (nbr_freed);
}


/*
*********************************************************************************************************
*                                     NetBuf_FreeBufQ_PrimList()
*
* Description : Free a network buffer queue, organized by the buffers' primary buffer lists.
*
*               (1) Network buffer queues are implemented as multiply-linked lists :
*
*                   (a) 'p_buf_q' points to the head of the buffer queue.
*
*                   (b) Buffers are multiply-linked to form a queue of buffer lists.
*
*                       In the diagram below, ... :
*
*                       (1) The top horizontal row  represents the queue of buffer lists.
*
*                       (2) Each    vertical column represents buffer fragments in the same buffer list.
*
*                       (3) Buffers' 'PrevPrimListPtr' & 'NextPrimListPtr' doubly-link each buffer list's
*                           head buffer to form the queue of buffer lists.
*
*                       (4) Buffer's 'PrevBufPtr'      & 'NextBufPtr'      doubly-link each buffer in a
*                           buffer list.
*
*
*                                                 |                                               |
*                                                 |<--------------- Buffer Queue ---------------->|
*                                                 |                (see Note #1b1)                |
*
*                                                             NextPrimListPtr
*                                                             (see Note #1b3)
*                                                                        |
*                                                                        |
*                      ---          Head of       -------       -------  v    -------       -------
*                       ^            Buffer  ---->|     |------>|     |------>|     |------>|     |
*                       |            Queue        |     |       |     |       |     |       |     |
*                       |                         |     |<------|     |<------|     |<------|     |
*                       |       (see Note #1a)    |     |       |     |  ^    |     |       |     |
*                       |                         |     |       |     |  |    |     |       |     |
*                       |                         -------       -------  |    -------       -------
*                       |                           | ^                  |      | ^
*                       |                           | |       PrevPrimListPtr   | |
*                       |                           v |       (see Note #1b3)   v |
*                       |                         -------                     -------
*                                                 |     |                     |     |
*                Fragments in the                 |     |                     |     |
*                same Buffer List                 |     |                     |     |
*                (see Note #1b2)                  |     |                     |     |
*                                                 |     |                     |     |
*                       |                         -------                     -------
*                       |                           | ^                         | ^
*                       |           NextBufPtr ---> | | <--- PrevBufPtr         | |
*                       |        (see Note #1b4)    v |   (see Note #1b4)       v |
*                       |                         -------                     -------
*                       |                         |     |                     |     |
*                       |                         |     |                     |     |
*                       |                         |     |                     -------
*                       |                         |     |
*                       v                         |     |
*                      ---                        -------
*
*
* Argument(s) : p_buf_q     Pointer to a buffer queue.
*
*               pctr        Pointer to possible error counter.
*
* Return(s)   : Number of network buffers freed.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Buffers are NOT validated for 'Type' or 'USED' before freeing. #### NET-808
*
*                   See also 'NetBuf_FreeHandler()  Note #2'.
*
*               (3) Buffers may be referenced by multiple layers.  Therefore, the buffers' reference
*                   counters MUST be checked before freeing the buffer(s).
*
*               (4) Buffers NOT freed are unlinked from other buffer fragment lists & compressed within
*                   their own buffer list.  Ideally, buffer fragment lists SHOULD NEVER be compressed
*                   but should be unlinked in their entirety.
*********************************************************************************************************
*/

NET_BUF_QTY  NetBuf_FreeBufQ_PrimList (NET_BUF  *p_buf_q,
                                       NET_CTR  *pctr)
{
    NET_BUF      *p_buf_list;
    NET_BUF      *p_buf_list_next;
    NET_BUF      *p_buf;
    NET_BUF      *p_buf_prev;
    NET_BUF      *p_buf_next;
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_QTY   nbr_freed;


    p_buf_list = p_buf_q;
    nbr_freed  = 0u;

    while (p_buf_list != (NET_BUF *)0) {                        /* Free ALL buf lists in buf Q.                         */
        p_buf_hdr                  = &p_buf_list->Hdr;
        p_buf_list_next            = (NET_BUF *)p_buf_hdr->NextPrimListPtr;
        p_buf_hdr->PrevPrimListPtr = (NET_BUF *)0;
        p_buf_hdr->NextPrimListPtr = (NET_BUF *)0;

        p_buf                      = (NET_BUF *)p_buf_list;
        p_buf_prev                 = (NET_BUF *)0;

        while (p_buf  != (NET_BUF *)0) {                        /* Free ALL bufs in buf list.                           */
            p_buf_hdr  = &p_buf->Hdr;
            p_buf_next =  p_buf_hdr->NextBufPtr;

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
            p_buf_hdr->PrevPrimListPtr = (NET_BUF *)0;
            p_buf_hdr->NextPrimListPtr = (NET_BUF *)0;
#endif

            if (p_buf_hdr->RefCtr > 1) {                        /* If     buf ref'd by multiple layers (see Note #3), ..*/
                p_buf_hdr->RefCtr--;                            /* .. dec buf ref ctr.                                  */
                p_buf_hdr->PrevBufPtr = (NET_BUF *)p_buf_prev;
                p_buf_hdr->NextBufPtr = (NET_BUF *)0;
                if (p_buf_prev != (NET_BUF *)0) {               /* If prev buf non-NULL, ...                            */
                    p_buf_hdr             = &p_buf_prev->Hdr;
                    p_buf_hdr->NextBufPtr =  p_buf;             /* ... set prev buf's next ptr to cur buf.              */
                }
                p_buf_prev = p_buf;                             /* Set cur buf as new prev buf (see Note #4).           */

            } else {                                            /* Else free buf.                                       */
                NetBuf_Free(p_buf);
            }

            if (pctr != (NET_CTR *)0) {                         /* If avail, ...                                        */
                NET_CTR_ERR_INC(*pctr);                         /* ... inc err ctr.                                     */
            }

            nbr_freed++;

            p_buf  = p_buf_next;
        }

        p_buf_list = p_buf_list_next;
    }

    return (nbr_freed);
}


/*
*********************************************************************************************************
*                                      NetBuf_FreeBufQ_SecList()
*
* Description : Free a network buffer queue, organized by the buffers' secondary buffer lists.
*
*               (1) Network buffer queues are implemented as multiply-linked lists :
*
*                   (a) 'p_buf_q' points to the head of the buffer queue.
*
*                   (b) Buffers are multiply-linked to form a queue of buffer lists.
*
*                       In the diagram below, ... :
*
*                       (1) The top horizontal row  represents the queue of buffer lists.
*
*                       (2) Each    vertical column represents buffer fragments in the same buffer list.
*
*                       (3) Buffers' 'PrevSecListPtr' & 'NextSecListPtr' doubly-link each buffer list's
*                           head buffer to form the queue of buffer lists.
*
*                       (4) Buffer's 'PrevBufPtr'     & 'NextBufPtr'     doubly-link each buffer in a
*                           buffer list.
*
*
*                                                 |                                               |
*                                                 |<--------------- Buffer Queue ---------------->|
*                                                 |                (see Note #1b1)                |
*
*                                                              NextSecListPtr
*                                                             (see Note #1b3)
*                                                                        |
*                                                                        |
*                      ---          Head of       -------       -------  v    -------       -------
*                       ^            Buffer  ---->|     |------>|     |------>|     |------>|     |
*                       |            Queue        |     |       |     |       |     |       |     |
*                       |                         |     |<------|     |<------|     |<------|     |
*                       |       (see Note #1a)    |     |       |     |  ^    |     |       |     |
*                       |                         |     |       |     |  |    |     |       |     |
*                       |                         -------       -------  |    -------       -------
*                       |                           | ^                  |      | ^
*                       |                           | |        PrevSecListPtr   | |
*                       |                           v |       (see Note #1b3)   v |
*                       |                         -------                     -------
*                                                 |     |                     |     |
*                Fragments in the                 |     |                     |     |
*                same Buffer List                 |     |                     |     |
*                (see Note #1b2)                  |     |                     |     |
*                                                 |     |                     |     |
*                       |                         -------                     -------
*                       |                           | ^                         | ^
*                       |           NextBufPtr ---> | | <--- PrevBufPtr         | |
*                       |        (see Note #1b4)    v |   (see Note #1b4)       v |
*                       |                         -------                     -------
*                       |                         |     |                     |     |
*                       |                         |     |                     |     |
*                       |                         |     |                     -------
*                       |                         |     |
*                       v                         |     |
*                      ---                        -------
*
*
* Argument(s) : p_buf_q         Pointer to a buffer queue.
*
*               pctr            Pointer to possible error counter.
*
*               pfnct_unlink    Pointer to possible unlink function.
*
* Return(s)   : Number of network buffers freed.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Buffers are NOT validated for 'Type' or 'USED' before freeing. #### NET-808
*
*                   See also 'NetBuf_FreeHandler()  Note #2'.
*
*               (3) Buffers may be referenced by multiple layers.  Therefore, the buffers' reference
*                   counters MUST be checked before freeing the buffer(s).
*
*               (4) Buffers NOT freed are unlinked from other buffer fragment lists & compressed within
*                   their own buffer list.  Ideally, buffer fragment lists SHOULD NEVER be compressed
*                   but should be unlinked in their entirety.
*
*               (5) Since buffers' unlink functions are intended to unlink a buffer from a secondary
*                   buffer queue list; the secondary buffer queue list's unlink function MUST be cleared
*                   before freeing the buffer to avoid unlinking the buffer(s) from the secondary buffer
*                   queue list multiple times.
*
*                   See also 'NetBuf_FreeHandler()  Note #3'.
*********************************************************************************************************
*/

NET_BUF_QTY  NetBuf_FreeBufQ_SecList (NET_BUF       *p_buf_q,
                                      NET_CTR       *pctr,
                                      NET_BUF_FNCT   pfnct_unlink)
{
    NET_BUF      *p_buf_list;
    NET_BUF      *p_buf_list_next;
    NET_BUF      *p_buf;
    NET_BUF      *p_buf_prev;
    NET_BUF      *p_buf_next;
    NET_BUF_HDR  *p_buf_hdr;
    NET_BUF_QTY   nbr_freed;


    p_buf_list = p_buf_q;
    nbr_freed  = 0u;

    while (p_buf_list   != (NET_BUF *)0) {                      /* Free ALL buf lists in buf Q.                         */
        p_buf_hdr        = &p_buf_list->Hdr;
        p_buf_list_next  = (NET_BUF *)p_buf_hdr->NextSecListPtr;

        p_buf            = (NET_BUF *)p_buf_list;
        p_buf_prev       = (NET_BUF *)0;

        while (p_buf    != (NET_BUF *)0) {                      /* Free ALL bufs in buf list.                           */
            p_buf_hdr    = &p_buf->Hdr;
            p_buf_next   =  p_buf_hdr->NextBufPtr;
                                                                /* Clr unlink & sec list ptrs (see Note #5).            */
            if (p_buf_hdr->UnlinkFnctPtr == (NET_BUF_FNCT)pfnct_unlink) {
                p_buf_hdr->UnlinkFnctPtr  = (NET_BUF_FNCT)0;
                p_buf_hdr->UnlinkObjPtr   = (void       *)0;
                p_buf_hdr->PrevSecListPtr = (NET_BUF    *)0;
                p_buf_hdr->NextSecListPtr = (NET_BUF    *)0;
            }

            if (p_buf_hdr->RefCtr > 1) {                        /* If     buf ref'd by multiple layers (see Note #3), ..*/
                p_buf_hdr->RefCtr--;                            /* .. dec buf ref ctr.                                  */
                p_buf_hdr->PrevBufPtr = (NET_BUF *)p_buf_prev;
                p_buf_hdr->NextBufPtr = (NET_BUF *)0;
                if (p_buf_prev != (NET_BUF *)0) {               /* If prev buf non-NULL, ...                            */
                    p_buf_hdr             = &p_buf_prev->Hdr;
                    p_buf_hdr->NextBufPtr =  p_buf;             /* ... set prev buf's next ptr to cur buf.              */
                }
                p_buf_prev = p_buf;                             /* Set cur buf as new prev buf (see Note #4).           */

            } else {                                            /* Else free buf.                                       */
                NetBuf_Free(p_buf);
            }

            if (pctr != (NET_CTR *)0) {                         /* If avail, ...                                        */
                NET_CTR_ERR_INC(*pctr);                         /* ... inc err ctr.                                     */
            }

            nbr_freed++;

            p_buf  = p_buf_next;
        }

        p_buf_list = p_buf_list_next;
    }

    return (nbr_freed);
}


/*
*********************************************************************************************************
*                                     NetBuf_FreeBufDataAreaRx()
*
* Description : Free a receive network buffer data area.
*
* Argument(s) : if_nbr      Network interface number freeing network buffer data area.
*               ------      Argument checked in NetIF_RxHandler().
*
*               p_buf_data  Pointer to network buffer data area to free.
*
* Return(s)   : none.
*
* Caller(s)   : NetIF_RxPkt(),
*               Device driver receive function(s).
*
*               This function is an INTERNAL network protocol suite function but MAY be called by
*               device driver receive function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  NetBuf_FreeBufDataAreaRx (NET_IF_NBR   if_nbr,
                                CPU_INT08U  *p_buf_data)
{
    NET_BUF_POOLS  *ppool;
    MEM_POOL       *pmem_pool;
    NET_STAT_POOL  *pstat_pool;
    NET_ERR         err;
    LIB_ERR         err_lib;

                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_buf_data == (CPU_INT08U *)0) {
        return;
    }

#if ((NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED) || \
     (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED))
   (void)NetIF_IsValidHandler(if_nbr, &err);                    /* --------------- VALIDATE NET IF NBR ---------------- */
    if (err != NET_IF_ERR_NONE) {
        return;
    }
#endif

                                                                /* -------------- FREE RX BUF DATA AREA --------------- */
    ppool      = &NetBuf_PoolsTbl[if_nbr];
    pmem_pool  = &ppool->RxBufLargePool;
    pstat_pool = &ppool->RxBufLargeStatPool;

    Mem_PoolBlkFree((MEM_POOL *) pmem_pool,
                    (void     *) p_buf_data,
                    (LIB_ERR  *)&err_lib);

    if (err_lib == LIB_MEM_ERR_NONE) {                          /* If buf data area freed to pool, ...                  */
        NetStat_PoolEntryUsedDec(pstat_pool, &err);             /* ... update buf pool stats;      ...                  */
    } else {                                                    /* ... else discard buf data area.                      */
        NetBuf_Discard((NET_IF_NBR     )if_nbr,
                       (void          *)p_buf_data,
                       (NET_STAT_POOL *)pstat_pool);
    }
}


/*
*********************************************************************************************************
*                                           NetBuf_DataRd()
*
* Description : (1) Read data from network buffer's DATA area :
*
*                   (a) Validate data read index & size
*                   (b) Read     data from buffer
*
*
* Argument(s) : p_buf       Pointer to a network buffer.
*
*               ix          Index into buffer's DATA area.
*
*               len         Number of octets to read (see Note #2).
*
*               pdest       Pointer to destination to read data into (see Note #3).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                Read from network buffer DATA area successful.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_buf'/'pdest' passed a NULL pointer.
*                               NET_BUF_ERR_INVALID_TYPE        Argument 'p_buf's TYPE is invalid or unknown.
*                               NET_BUF_ERR_INVALID_IX          Invalid index  (outside buffer's DATA area).
*                               NET_BUF_ERR_INVALID_LEN         Invalid length (outside buffer's DATA area).
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Data read of 0 octets allowed.
*
*               (3) Destination buffer size NOT validated; buffer overruns MUST be prevented by caller.
*
*               (4) 'ix' & 'len' argument check NOT required unless 'NET_BUF_SIZE's native data type
*                   'CPU_INT16U' is incorrectly configured as a signed integer in 'cpu.h'.
*
*               (5) Buffer 'Size' is NOT re-validated; validated in NetBuf_Get().
*********************************************************************************************************
*/

void  NetBuf_DataRd (NET_BUF       *p_buf,
                     NET_BUF_SIZE   ix,
                     NET_BUF_SIZE   len,
                     CPU_INT08U    *pdest,
                     NET_ERR       *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_BUF_HDR   *p_buf_hdr;
    NET_BUF_SIZE   len_data;
#endif
    CPU_INT08U    *p_data;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE BUF PTR ------------------ */
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                /* ---------------- VALIDATE BUF TYPE ----------------- */
    p_buf_hdr = &p_buf->Hdr;
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

                                                                /* ----------------- VALIDATE DEST PTR ---------------- */
    if (pdest == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- VALIDATE IX/SIZE ----------------- */
#if 0                                                           /* See Note #4.                                         */
    if (ix < 0) {                                               /* If req'd ix  < 0,    rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    if (len < 0) {                                              /* If req'd len < 0,    rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
#endif
    if (len < 1) {                                              /* If req'd len = 0,    rtn null rd (see Note #2).      */
       *p_err = NET_BUF_ERR_NONE;
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (ix >= p_buf_hdr->Size) {                                /* If req'd ix  > size, rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    len_data = ix + len;
    if (len_data > p_buf_hdr->Size) {                           /* If req'd len > size, rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
#endif

                                                                /* ------------------- RD BUF DATA -------------------- */
                                                                /* Req'd ix & len within  buf DATA area; ...            */
    p_data = &p_buf->DataPtr[ix];                                /* ... set ptr to ix into buf DATA area, ...            */
    Mem_Copy((void     *)pdest,                                 /* ... & copy len nbr DATA buf octets to dest.          */
             (void     *)p_data,
             (CPU_SIZE_T)len);

   *p_err = NET_BUF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetBuf_DataWr()
*
* Description : (1) Write data into network buffer's DATA area :
*
*                   (a) Validate data write index & size
*                   (b) Write    data into buffer
*
*
* Argument(s) : p_buf       Pointer to a network buffer.
*
*               ix          Index into buffer's DATA area.
*
*               len         Number of octets to write (see Note #2).
*
*               psrc        Pointer to data to write.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                Write to network buffer DATA area successful.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_buf'/'psrc' passed a NULL pointer.
*                               NET_BUF_ERR_INVALID_TYPE        Argument 'p_buf's TYPE is invalid or unknown.
*                               NET_BUF_ERR_INVALID_IX          Invalid index  (outside buffer's DATA area).
*                               NET_BUF_ERR_INVALID_LEN         Invalid length (outside buffer's DATA area).
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Data write of 0 octets allowed.
*
*               (3) 'ix' & 'len' argument check NOT required unless 'NET_BUF_SIZE's native data type
*                   'CPU_INT16U' is incorrectly configured as a signed integer in 'cpu.h'.
*
*               (4) Buffer 'Size' is NOT re-validated; validated in NetBuf_Get().
*********************************************************************************************************
*/

void  NetBuf_DataWr (NET_BUF       *p_buf,
                     NET_BUF_SIZE   ix,
                     NET_BUF_SIZE   len,
                     CPU_INT08U    *psrc,
                     NET_ERR       *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_BUF_HDR   *p_buf_hdr;
    NET_BUF_SIZE   len_data;
#endif
    CPU_INT08U    *p_data;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE BUF PTR ------------------ */
    if (p_buf == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                /* ---------------- VALIDATE BUF TYPE ----------------- */
    p_buf_hdr = &p_buf->Hdr;
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

                                                                /* ----------------- VALIDATE SRC PTR ----------------- */
    if (psrc == (CPU_INT08U *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
#endif

                                                                /* ----------------- VALIDATE IX/SIZE ----------------- */
#if 0                                                           /* See Note #3.                                         */
    if (ix < 0) {                                               /* If req'd ix  < 0,    rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    if (len < 0) {                                              /* If req'd len < 0,    rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
#endif
    if (len < 1) {                                              /* If req'd len = 0,    rtn null wr (see Note #2).      */
       *p_err = NET_BUF_ERR_NONE;
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    if (ix >= p_buf_hdr->Size) {                                /* If req'd ix  > size, rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    len_data = ix + len;
    if (len_data > p_buf_hdr->Size) {                           /* If req'd len > size, rtn err.                        */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
#endif

                                                                /* ------------------- WR BUF DATA -------------------- */
                                                                /* Req'd ix & len within  buf DATA area; ...            */
    p_data = &p_buf->DataPtr[ix];                               /* ... set ptr to ix into buf DATA area, ...            */
    Mem_Copy((void     *)p_data,                                /* ... & copy len nbr src octets into DATA buf.         */
             (void     *)psrc,
             (CPU_SIZE_T)len);

   *p_err = NET_BUF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                          NetBuf_DataCopy()
*
* Description : (1) Copy data from one network buffer's DATA area to another network buffer's DATA area :
*
*                   (a) Validate data copy indices & sizes
*                   (b) Copy     data between buffers
*
*
* Argument(s) : p_buf_dest  Pointer to destination network buffer.
*
*               p_buf_src   Pointer to source      network buffer.
*
*               ix_dest     Index into destination buffer's DATA area.
*
*               ix_src      Index into source      buffer's DATA area.
*
*               len         Number of octets to copy (see Note #2).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_BUF_ERR_NONE                Copy between network buffer DATA areas successful.
*                               NET_ERR_FAULT_NULL_PTR          Argument 'p_buf_dest'/'p_buf_src' passed a NULL
*                                                                   pointer.
*                               NET_BUF_ERR_INVALID_TYPE        Argument 'p_buf_dest'/'p_buf_src's TYPE is invalid
*                                                                   or unknown.
*                               NET_BUF_ERR_INVALID_IX          Invalid index  [outside buffer(s)' DATA area(s)].
*                               NET_BUF_ERR_INVALID_LEN         Invalid length [outside buffer(s)' DATA area(s)].
*
* Return(s)   : none.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (2) Data copy of 0 octets allowed.
*
*               (3) 'ix_&&&' & 'len' argument check NOT required unless 'NET_BUF_SIZE's native data type
*                   'CPU_INT16U' is incorrectly configured as a signed integer in 'cpu.h'.
*
*               (4) Buffer 'Size's are NOT re-validated; validated in NetBuf_Get().
*********************************************************************************************************
*/

void  NetBuf_DataCopy (NET_BUF       *p_buf_dest,
                       NET_BUF       *p_buf_src,
                       NET_BUF_SIZE   ix_dest,
                       NET_BUF_SIZE   ix_src,
                       NET_BUF_SIZE   len,
                       NET_ERR       *p_err)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    NET_BUF_HDR   *p_buf_hdr_dest;
    NET_BUF_HDR   *p_buf_hdr_src;
    NET_BUF_SIZE   len_data;
#endif
    CPU_INT08U    *p_data_dest;
    CPU_INT08U    *p_data_src;


#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* ---------------- VALIDATE BUF PTRS ----------------- */
    if (p_buf_dest == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_buf_src  == (NET_BUF *)0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NullPtrCtr);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

                                                                /* ---------------- VALIDATE BUF TYPES ---------------- */
    p_buf_hdr_dest = &p_buf_dest->Hdr;
    switch (p_buf_hdr_dest->Type) {
        case NET_BUF_TYPE_RX_LARGE:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }

    p_buf_hdr_src = &p_buf_src->Hdr;
    switch (p_buf_hdr_src->Type) {
        case NET_BUF_TYPE_RX_LARGE:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        default:
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
            *p_err = NET_BUF_ERR_INVALID_TYPE;
             return;
    }
#endif


                                                                /* --------------- VALIDATE IX's/SIZES ---------------- */
#if 0                                                           /* See Note #3.                                         */
                                                                /* If req'd ix's < 0,    rtn err.                       */
    if (ix_dest < 0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }
    if (ix_src  < 0) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }

    if (len < 0) {                                              /* If req'd len  < 0,    rtn err.                       */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
#endif
    if (len < 1) {                                              /* If req'd len  = 0,    rtn null copy (see Note #2).   */
       *p_err = NET_BUF_ERR_NONE;
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
                                                                /* If req'd ix's > size, rtn err.                       */
    if (ix_dest >= p_buf_hdr_dest->Size) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }
    if (ix_src  >= p_buf_hdr_src->Size ) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.IxCtr);
       *p_err = NET_BUF_ERR_INVALID_IX;
        return;
    }
                                                                /* If req'd lens > size, rtn err.                       */
    len_data = ix_dest + len;
    if (len_data > p_buf_hdr_dest->Size) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
    len_data = ix_src  + len;
    if (len_data > p_buf_hdr_src->Size ) {
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.LenCtr);
       *p_err = NET_BUF_ERR_INVALID_LEN;
        return;
    }
#endif

                                                                /* ------------------ COPY BUF DATA ------------------- */
                                                                /* Req'd ix's & len within buf DATA areas; ...          */
    p_data_dest = &p_buf_dest->DataPtr[ix_dest];                /* ... set ptrs to ix into buf DATA areas, ...          */
    p_data_src  = &p_buf_src ->DataPtr[ix_src ];
    Mem_Copy((void     *)p_data_dest,                           /* ... & copy len nbr DATA buf octets      ...          */
             (void     *)p_data_src,                            /* ... from src to dest buf.                            */
             (CPU_SIZE_T)len);

   *p_err = NET_BUF_ERR_NONE;
}


/*
*********************************************************************************************************
*                                           NetBuf_IsUsed()
*
* Description : Validate buffer in use.
*
* Argument(s) : p_buf       Pointer to object to validate as a network buffer in use.
*
* Return(s)   : DEF_YES, buffer   valid &      in use.
*
*               DEF_NO,  buffer invalid or NOT in use.
*
* Caller(s)   : various.
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_IsUsed() MUST be called with the global network lock already acquired.
*********************************************************************************************************
*/

CPU_BOOLEAN  NetBuf_IsUsed (NET_BUF  *p_buf)
{
    NET_BUF_HDR  *p_buf_hdr;
    CPU_BOOLEAN   used;

                                                                /* ------------------ VALIDATE PTR -------------------- */
    if (p_buf == (NET_BUF *)0) {
        return  (DEF_NO);
    }
                                                                /* ------------------ VALIDATE TYPE ------------------- */
    p_buf_hdr = &p_buf->Hdr;
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
        case NET_BUF_TYPE_TX_LARGE:
        case NET_BUF_TYPE_TX_SMALL:
             break;


        case NET_BUF_TYPE_NONE:
        default:
             return (DEF_NO);
    }

                                                                /* ---------------- VALIDATE BUF USED ----------------- */
    used =  DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_USED);

    return (used);
}


/*
*********************************************************************************************************
*                                        NetBuf_PoolStatGet()
*
* Description : Get network buffer statistics pool.
*
* Argument(s) : if_nbr      Interface number to get network buffer statistics.
*
* Return(s)   : Network buffer statistics pool, if NO error(s).
*
*               NULL           statistics pool, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_PoolStatGet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_PoolStatGet() blocked until network initialization completes.
*
*               (3) Return values MUST be initialized PRIOR to all other validation or function handling
*                   in case of any error(s).
*
*               (4) 'NetBufStatPool's MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

NET_STAT_POOL  NetBuf_PoolStatGet (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL   stat_pool;
    NET_ERR         err;
    CPU_SR_ALLOC();


    NetStat_PoolClr(&stat_pool, &err);                          /* Init rtn pool stat for err (see Note #3).            */

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
    Net_GlobalLockAcquire((void *)&NetBuf_PoolStatGet, &err);   /* See Note #1b.                                        */
    if (err != NET_ERR_NONE) {
        return (stat_pool);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {
        goto exit_release;
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* -------------- GET NET BUF STAT POOL --------------- */
    ppool     = &NetBuf_PoolsTbl[if_nbr];
    CPU_CRITICAL_ENTER();
    stat_pool =  ppool->NetBufStatPool;
    CPU_CRITICAL_EXIT();

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();


    return (stat_pool);
}


/*
*********************************************************************************************************
*                                    NetBuf_PoolStatResetMaxUsed()
*
* Description : Reset network buffer statistics pool's maximum number of entries used.
*
* Argument(s) : if_nbr      Interface number to reset network buffer statistics.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_PoolStatResetMaxUsed() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_PoolStatResetMaxUsed() blocked until network initialization completes.
*
*                   (a) However, since 'NetBufStatPool's are reset when network initialization
*                       completes; NO error is returned.
*********************************************************************************************************
*/

void  NetBuf_PoolStatResetMaxUsed (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_ERR         err;

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_PoolStatResetMaxUsed, &err);
    if (err != NET_ERR_NONE) {
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        goto exit_release;                                      /* ... rtn w/o err (see Note #2a).                      */
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* ------------- RESET NET BUF STAT POOL -------------- */
    ppool = &NetBuf_PoolsTbl[if_nbr];
    NetStat_PoolResetUsedMax(&ppool->NetBufStatPool, &err);

    goto exit_release;

exit_release:

                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                     NetBuf_RxLargePoolStatGet()
*
* Description : Get large receive network buffer statistics pool.
*
* Argument(s) : if_nbr      Interface number to get network buffer statistics.
*
* Return(s)   : Large receive network buffer statistics pool, if NO error(s).
*
*               NULL                         statistics pool, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_RxLargePoolStatGet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_RxLargePoolStatGet() blocked until network initialization completes.
*
*               (3) Return values MUST be initialized PRIOR to all other validation or function handling
*                   in case of any error(s).
*
*               (4) 'RxBufLargeStatPool's MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

NET_STAT_POOL  NetBuf_RxLargePoolStatGet (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL   stat_pool;
    NET_ERR         err;
    CPU_SR_ALLOC();


    NetStat_PoolClr(&stat_pool, &err);                          /* Init rtn pool stat for err (see Note #3).            */

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_RxLargePoolStatGet, &err);
    if (err != NET_ERR_NONE) {
        return (stat_pool);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {
        goto exit_release;
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* -------------- GET NET BUF STAT POOL --------------- */
    ppool     = &NetBuf_PoolsTbl[if_nbr];
    CPU_CRITICAL_ENTER();
    stat_pool =  ppool->RxBufLargeStatPool;
    CPU_CRITICAL_EXIT();

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();


    return (stat_pool);
}


/*
*********************************************************************************************************
*                                NetBuf_RxLargePoolStatResetMaxUsed()
*
* Description : Reset large receive network buffer statistics pool's maximum number of entries used.
*
* Argument(s) : if_nbr      Interface number to reset network buffer statistics.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_RxLargePoolStatResetMaxUsed() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_RxLargePoolStatResetMaxUsed() blocked until network initialization completes.
*
*                   (a) However, since 'RxBufLargeStatPool's are reset when network initialization
*                       completes; NO error is returned.
*********************************************************************************************************
*/

void  NetBuf_RxLargePoolStatResetMaxUsed (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_ERR         err;

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_RxLargePoolStatResetMaxUsed, &err);
    if (err != NET_ERR_NONE) {
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        goto exit_release;                                      /* ... rtn w/o err (see Note #2a).                      */
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* ------------- RESET NET BUF STAT POOL -------------- */
    ppool = &NetBuf_PoolsTbl[if_nbr];
    NetStat_PoolResetUsedMax(&ppool->RxBufLargeStatPool, &err);

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                     NetBuf_TxLargePoolStatGet()
*
* Description : Get large transmit network buffer statistics pool.
*
* Argument(s) : if_nbr      Interface number to get network buffer statistics.
*
* Return(s)   : Large transmit network buffer statistics pool, if NO error(s).
*
*               NULL                          statistics pool, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_TxLargePoolStatGet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_TxLargePoolStatGet() blocked until network initialization completes.
*
*               (3) Return values MUST be initialized PRIOR to all other validation or function handling
*                   in case of any error(s).
*
*               (4) 'TxBufLargeStatPool's MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

NET_STAT_POOL  NetBuf_TxLargePoolStatGet (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL   stat_pool;
    NET_ERR         err;
    CPU_SR_ALLOC();


    NetStat_PoolClr(&stat_pool, &err);                          /* Init rtn pool stat for err (see Note #3).            */

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_TxLargePoolStatGet, &err);
    if (err != NET_ERR_NONE) {
        return (stat_pool);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {
        goto exit_release;
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* -------------- GET NET BUF STAT POOL --------------- */
    ppool     = &NetBuf_PoolsTbl[if_nbr];
    CPU_CRITICAL_ENTER();
    stat_pool =  ppool->TxBufLargeStatPool;
    CPU_CRITICAL_EXIT();

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();


    return (stat_pool);
}


/*
*********************************************************************************************************
*                                NetBuf_TxLargePoolStatResetMaxUsed()
*
* Description : Reset large receive network buffer statistics pool's maximum number of entries used.
*
* Argument(s) : if_nbr      Interface number to reset network buffer statistics.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_TxLargePoolStatResetMaxUsed() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_TxLargePoolStatResetMaxUsed() blocked until network initialization completes.
*
*                   (a) However, since 'TxBufLargeStatPool's are reset when network initialization
*                       completes; NO error is returned.
*********************************************************************************************************
*/

void  NetBuf_TxLargePoolStatResetMaxUsed (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_ERR         err;

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_TxLargePoolStatResetMaxUsed, &err);
    if (err != NET_ERR_NONE) {
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        goto exit_release;                                            /* ... rtn w/o err (see Note #2a).                      */
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* ------------- RESET NET BUF STAT POOL -------------- */
    ppool = &NetBuf_PoolsTbl[if_nbr];
    NetStat_PoolResetUsedMax(&ppool->TxBufLargeStatPool, &err);

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*                                     NetBuf_TxSmallPoolStatGet()
*
* Description : Get small transmit network buffer statistics pool.
*
* Argument(s) : if_nbr      Interface number to get network buffer statistics.
*
* Return(s)   : Small transmit network buffer statistics pool, if NO error(s).
*
*               NULL                          statistics pool, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_TxSmallPoolStatGet() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_TxSmallPoolStatGet() blocked until network initialization completes.
*
*               (3) Return values MUST be initialized PRIOR to all other validation or function handling
*                   in case of any error(s).
*
*               (4) 'TxBufSmallStatPool's MUST ALWAYS be accessed exclusively in critical sections.
*********************************************************************************************************
*/

NET_STAT_POOL  NetBuf_TxSmallPoolStatGet (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL   stat_pool;
    NET_ERR         err;
    CPU_SR_ALLOC();


    NetStat_PoolClr(&stat_pool, &err);                          /* Init rtn pool stat for err (see Note #3).            */

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_TxSmallPoolStatGet, &err);
    if (err != NET_ERR_NONE) {
        return (stat_pool);
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {
        goto exit_release;
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* -------------- GET NET BUF STAT POOL --------------- */
    ppool     = &NetBuf_PoolsTbl[if_nbr];
    CPU_CRITICAL_ENTER();
    stat_pool =  ppool->TxBufSmallStatPool;
    CPU_CRITICAL_EXIT();

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();


    return (stat_pool);
}


/*
*********************************************************************************************************
*                                NetBuf_TxSmallPoolStatResetMaxUsed()
*
* Description : Reset small transmit network buffer statistics pool's maximum number of entries used.
*
* Argument(s) : if_nbr      Interface number to reset network buffer statistics.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s) [see also Note #1].
*
* Note(s)     : (1) NetBuf_TxSmallPoolStatResetMaxUsed() is called by application function(s) & ... :
*
*                   (a) MUST NOT be called with the global network lock already acquired; ...
*                   (b) MUST block ALL other network protocol tasks by pending on & acquiring the global
*                       network lock.
*
*                   This is required since an application's network protocol suite API function access is
*                   asynchronous to other network protocol tasks.
*
*               (2) NetBuf_TxSmallPoolStatResetMaxUsed() blocked until network initialization completes.
*
*                   (a) However, since 'TxBufSmallStatPool's are reset when network initialization
*                       completes; NO error is returned.
*********************************************************************************************************
*/

void  NetBuf_TxSmallPoolStatResetMaxUsed (NET_IF_NBR  if_nbr)
{
    NET_BUF_POOLS  *ppool;
    NET_ERR         err;

                                                                /* ----------------- ACQUIRE NET LOCK ----------------- */
                                                                /* See Note #1b.                                        */
    Net_GlobalLockAcquire((void *)&NetBuf_TxSmallPoolStatResetMaxUsed, &err);
    if (err != NET_ERR_NONE) {
        return;
    }

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (Net_InitDone != DEF_YES) {                              /* If init NOT complete, ...                            */
        goto exit_release;                                      /* ... rtn w/o err (see Note #2a).                      */
    }
                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        goto exit_release;
    }
#endif

                                                                /* ------------- RESET NET BUF STAT POOL -------------- */
    ppool = &NetBuf_PoolsTbl[if_nbr];
    NetStat_PoolResetUsedMax(&ppool->TxBufSmallStatPool, &err);

    goto exit_release;

exit_release:
                                                                /* ----------------- RELEASE NET LOCK ----------------- */
    Net_GlobalLockRelease();
}


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                        NetBuf_FreeHandler()
*
* Description : (1) Free a network buffer :
*
*                   (a) Configure buffer free by buffer type
*                   (b) Unlink    buffer from network layer(s)                          See Note #3
*                   (c) Clear     buffer controls
*                   (d) Free      buffer & data area back to buffer pools
*                   (e) Update    buffer pool statistics
*
*
* Argument(s) : p_buf       Pointer to a network buffer.
*               ----        Argument checked in NetBuf_Free().
*
* Return(s)   : none.
*
* Caller(s)   : NetBuf_Free().
*
* Note(s)     : (2) #### To prevent freeing a buffer already freed via auxiliary pointer(s),
*                   NetBuf_FreeHandler() checks the buffer's 'USED' flag BEFORE freeing the buffer.
*
*                   This prevention is only best-effort since any invalid duplicate  buffer frees MAY be
*                   asynchronous to potentially valid buffer gets.  Thus the invalid buffer free(s) MAY
*                   corrupt the buffer's valid operation(s).
*
*                   However, since the primary tasks of the network protocol suite are prevented from
*                   running concurrently (see 'net.h  Note #3'), it is NOT necessary to protect network
*                   buffer resources from possible corruption since no asynchronous access from other
*                   network tasks is possible.
*
*               (3) If a network buffer's unlink function is available, it is assumed that the function
*                   correctly unlinks the network buffer from any other network layer(s).
*********************************************************************************************************
*/

static  void  NetBuf_FreeHandler (NET_BUF  *p_buf)
{
#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)
    CPU_BOOLEAN     used;
#endif
    NET_IF_NBR      if_nbr;
    NET_BUF_HDR    *p_buf_hdr;
    NET_BUF_POOLS  *ppool;
    NET_STAT_POOL  *pstat_pool;
    MEM_POOL       *pmem_pool;
    NET_BUF_FNCT    unlink_fnct;
    NET_ERR         err;
    LIB_ERR         err_lib;


    p_buf_hdr = &p_buf->Hdr;

#if (NET_ERR_CFG_ARG_CHK_DBG_EN == DEF_ENABLED)                 /* ---------------- VALIDATE BUF USED ----------------- */
    used = DEF_BIT_IS_SET(p_buf_hdr->Flags, NET_BUF_FLAG_USED);
    if (used != DEF_YES) {                                      /* If buf NOT used, ...                                 */
        NET_CTR_ERR_INC(Net_ErrCtrs.Buf.NotUsedCtr);
        return;                                                 /* ... rtn but do NOT free (see Note #2).               */
    }
#endif

    if_nbr = p_buf_hdr->IF_Nbr;

                                                                /* --------------- VALIDATE NET IF NBR ---------------- */
   (void)NetIF_IsValidHandler(if_nbr, &err);
    if (err != NET_IF_ERR_NONE) {
        return;
    }

                                                                /* ------------------- CFG BUF FREE ------------------- */
    ppool = &NetBuf_PoolsTbl[if_nbr];
    switch (p_buf_hdr->Type) {
        case NET_BUF_TYPE_RX_LARGE:
             pmem_pool  = &ppool->RxBufLargePool;
             pstat_pool = &ppool->RxBufLargeStatPool;
             break;


        case NET_BUF_TYPE_TX_LARGE:
             pmem_pool  = &ppool->TxBufLargePool;
             pstat_pool = &ppool->TxBufLargeStatPool;
             break;


        case NET_BUF_TYPE_TX_SMALL:
             pmem_pool  = &ppool->TxBufSmallPool;
             pstat_pool = &ppool->TxBufSmallStatPool;
             break;


        case NET_BUF_TYPE_NONE:
        default:
             NetBuf_Discard((NET_IF_NBR     )if_nbr,
                            (NET_BUF       *)p_buf,
                            (NET_STAT_POOL *)0);
             NET_CTR_ERR_INC(Net_ErrCtrs.Buf.InvTypeCtr);
             return;
    }


                                                                /* -------------------- UNLINK BUF -------------------- */
    unlink_fnct = p_buf_hdr->UnlinkFnctPtr;
    if (unlink_fnct != (NET_BUF_FNCT)0) {                       /* If unlink fnct avail, ..                             */
        unlink_fnct(p_buf);                                     /* .. unlink buf from other layer(s) [see Note #3].     */
    }


                                                                /* ---------------------- CLR BUF --------------------- */
    DEF_BIT_CLR(p_buf_hdr->Flags, NET_BUF_FLAG_USED);           /* Set buf as NOT used.                                 */

#if (NET_DBG_CFG_MEM_CLR_EN == DEF_ENABLED)
    NetBuf_ClrHdr(p_buf_hdr);
#endif


                                                                /* -------------- FREE NET BUF DATA AREA -------------- */
    Mem_PoolBlkFree((MEM_POOL *) pmem_pool,
                    (void     *) p_buf->DataPtr,
                    (LIB_ERR  *)&err_lib);

    if (err_lib == LIB_MEM_ERR_NONE) {                          /* If buf data area freed to pool, ...                  */
        NetStat_PoolEntryUsedDec(pstat_pool, &err);             /* ... update buf pool stats;      ...                  */
    } else {                                                    /* ... else discard buf data area.                      */
        NetBuf_Discard((NET_IF_NBR     )if_nbr,
                       (void          *)p_buf->DataPtr,
                       (NET_STAT_POOL *)pstat_pool);
    }

                                                                /* ------------------- FREE NET BUF ------------------- */
    Mem_PoolBlkFree((MEM_POOL *)&ppool->NetBufPool,
                    (void     *) p_buf,
                    (LIB_ERR  *)&err_lib);

    if (err_lib == LIB_MEM_ERR_NONE) {                          /* If buf freed to pool,      ...                       */
        NetStat_PoolEntryUsedDec(&ppool->NetBufStatPool, &err); /* ... update buf pool stats; ...                       */
    } else {                                                    /* ... else discard buf.                                */
        NetBuf_Discard((NET_IF_NBR     ) if_nbr,
                       (void          *) p_buf,
                       (NET_STAT_POOL *)&ppool->NetBufStatPool);
    }
}


/*
*********************************************************************************************************
*                                           NetBuf_ClrHdr()
*
* Description : Clear network buffer header controls.
*
* Argument(s) : p_buf_hdr   Pointer to network buffer header.
*               --------    Argument validated in NetBuf_Get(),
*                                                 NetBuf_FreeHandler().
*
* Return(s)   : none.
*
* Caller(s)   : NetBuf_Get(),
*               NetBuf_FreeHandler().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  NetBuf_ClrHdr (NET_BUF_HDR  *p_buf_hdr)
{
    p_buf_hdr->Type                     =  NET_BUF_TYPE_NONE;
    p_buf_hdr->Size                     =  0u;
    p_buf_hdr->Flags                    =  NET_BUF_FLAG_NONE;

    p_buf_hdr->RefCtr                   =  0u;
    p_buf_hdr->ID                       =  NET_BUF_ID_NONE;

    p_buf_hdr->IF_Nbr                   =  NET_IF_NBR_NONE;
    p_buf_hdr->IF_NbrTx                 =  NET_IF_NBR_NONE;

    p_buf_hdr->PrevPrimListPtr          = (NET_BUF *)0;
    p_buf_hdr->NextPrimListPtr          = (NET_BUF *)0;
    p_buf_hdr->PrevSecListPtr           = (NET_BUF *)0;
    p_buf_hdr->NextSecListPtr           = (NET_BUF *)0;
    p_buf_hdr->PrevTxListPtr            = (NET_BUF *)0;
    p_buf_hdr->NextTxListPtr            = (NET_BUF *)0;
    p_buf_hdr->PrevBufPtr               = (NET_BUF *)0;
    p_buf_hdr->NextBufPtr               = (NET_BUF *)0;

    p_buf_hdr->TmrPtr                   = (NET_TMR *)0;

    p_buf_hdr->UnlinkFnctPtr            = (NET_BUF_FNCT)0;
    p_buf_hdr->UnlinkObjPtr             = (void       *)0;

    p_buf_hdr->ProtocolHdrType          =  NET_PROTOCOL_TYPE_NONE;
    p_buf_hdr->ProtocolHdrTypeIF        =  NET_PROTOCOL_TYPE_NONE;
    p_buf_hdr->ProtocolHdrTypeIF_Sub    =  NET_PROTOCOL_TYPE_NONE;
    p_buf_hdr->ProtocolHdrTypeNet       =  NET_PROTOCOL_TYPE_NONE;
    p_buf_hdr->ProtocolHdrTypeNetSub    =  NET_PROTOCOL_TYPE_NONE;
    p_buf_hdr->ProtocolHdrTypeTransport =  NET_PROTOCOL_TYPE_NONE;

    p_buf_hdr->IF_HdrIx                 =  NET_BUF_IX_NONE;
    p_buf_hdr->IF_HdrLen                =  0u;
#ifdef  NET_ARP_MODULE_EN
    p_buf_hdr->ARP_MsgIx                =  NET_BUF_IX_NONE;
    p_buf_hdr->ARP_MsgLen               =  0u;
#endif
    p_buf_hdr->IP_HdrIx                 =  NET_BUF_IX_NONE;
    p_buf_hdr->IP_HdrLen                =  0u;

    p_buf_hdr->ICMP_MsgIx               =  NET_BUF_IX_NONE;
    p_buf_hdr->ICMP_MsgLen              =  0u;
    p_buf_hdr->ICMP_HdrLen              =  0u;
#ifdef  NET_IGMP_MODULE_EN
    p_buf_hdr->IGMP_MsgIx               =  NET_BUF_IX_NONE;
    p_buf_hdr->IGMP_MsgLen              =  0u;
#endif
    p_buf_hdr->TransportHdrIx           =  NET_BUF_IX_NONE;
    p_buf_hdr->TransportHdrLen          =  0u;
    p_buf_hdr->TransportTotLen          =  0u;
    p_buf_hdr->TransportDataLen         =  0u;
    p_buf_hdr->DataIx                   =  NET_BUF_IX_NONE;
    p_buf_hdr->DataLen                  =  0u;
    p_buf_hdr->TotLen                   =  0u;

#ifdef  NET_ARP_MODULE_EN
    p_buf_hdr->ARP_AddrHW_Ptr           = (CPU_INT08U *)0;
    p_buf_hdr->ARP_AddrProtocolPtr      = (CPU_INT08U *)0;
#endif

    p_buf_hdr->IP_TotLen                =  0u;
    p_buf_hdr->IP_DataLen               =  0u;
    p_buf_hdr->IP_DatagramLen           =  0u;
    p_buf_hdr->IP_FragSizeTot           =  NET_IP_FRAG_SIZE_NONE;
    p_buf_hdr->IP_FragSizeCur           =  0u;

#ifdef  NET_IPv4_MODULE_EN
    p_buf_hdr->IP_Flags_FragOffset      =  NET_IPv4_HDR_FLAG_NONE | NET_IPv4_HDR_FRAG_OFFSET_NONE;
    p_buf_hdr->IP_ID                    =  NET_IPv4_ID_NONE;
    p_buf_hdr->IP_AddrSrc               = (NET_IPv4_ADDR)NET_IPv4_ADDR_NONE;
    p_buf_hdr->IP_AddrDest              = (NET_IPv4_ADDR)NET_IPv4_ADDR_NONE;
    p_buf_hdr->IP_AddrNextRoute         = (NET_IPv4_ADDR)NET_IPv4_ADDR_NONE;
    p_buf_hdr->IP_AddrNextRouteNetOrder = (NET_IPv4_ADDR)NET_UTIL_HOST_TO_NET_32(NET_IPv4_ADDR_NONE);
    p_buf_hdr->IP_OptPtr                = (NET_BUF   *)0;
#endif
#ifdef  NET_IPv6_MODULE_EN
    p_buf_hdr->IPv6_Flags_FragOffset    = NET_IPv6_FRAG_NONE;
    Mem_Clr(&p_buf_hdr->IPv6_AddrSrc,               NET_IPv6_ADDR_SIZE);
    Mem_Clr(&p_buf_hdr->IPv6_AddrDest,              NET_IPv6_ADDR_SIZE);
    Mem_Clr(&p_buf_hdr->IPv6_AddrNextRoute,         NET_IPv6_ADDR_SIZE);
    p_buf_hdr->IP_HdrIx                 =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_ExtHdrLen           =  0u;
    p_buf_hdr->IPv6_HopByHopHdrIx       =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_RoutingHdrIx        =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_FragHdrIx           =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_ESP_HdrIx           =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_AuthHdrIx           =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_DestHdrIx           =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_MobilityHdrIx       =  NET_BUF_IX_NONE;
    p_buf_hdr->IPv6_ID                  =  0u;
#endif

#ifdef  NET_NDP_MODULE_EN
    p_buf_hdr->NDP_AddrHW_Ptr           =  DEF_NULL;
    p_buf_hdr->NDP_AddrProtocolPtr      =  DEF_NULL;
#endif

    p_buf_hdr->TransportPortSrc         =  NET_PORT_NBR_NONE;
    p_buf_hdr->TransportPortDest        =  NET_PORT_NBR_NONE;

#ifdef  NET_TCP_MODULE_EN
    p_buf_hdr->TCP_HdrLen_Flags         =  NET_TCP_HDR_LEN_NONE | NET_TCP_HDR_FLAG_NONE;
    p_buf_hdr->TCP_SegLenInit           =  0u;
    p_buf_hdr->TCP_SegLenLast           =  0u;
    p_buf_hdr->TCP_SegLen               =  0u;
    p_buf_hdr->TCP_SegLenData           =  0u;
    p_buf_hdr->TCP_SegReTxCtr           =  0u;
    p_buf_hdr->TCP_SegSync              =  DEF_NO;
    p_buf_hdr->TCP_SegClose             =  DEF_NO;
    p_buf_hdr->TCP_SegReset             =  DEF_NO;
    p_buf_hdr->TCP_SegAck               =  DEF_NO;
    p_buf_hdr->TCP_SegAckTxd            =  DEF_NO;
    p_buf_hdr->TCP_SegAckTxReqCode      =  NET_TCP_CONN_TX_ACK_NONE;
    p_buf_hdr->TCP_SeqNbrInit           =  NET_TCP_SEQ_NBR_NONE;
    p_buf_hdr->TCP_SeqNbrLast           =  NET_TCP_SEQ_NBR_NONE;
    p_buf_hdr->TCP_SeqNbr               =  NET_TCP_SEQ_NBR_NONE;
    p_buf_hdr->TCP_AckNbr               =  NET_TCP_ACK_NBR_NONE;
    p_buf_hdr->TCP_AckNbrLast           =  NET_TCP_ACK_NBR_NONE;
    p_buf_hdr->TCP_MaxSegSize           =  NET_TCP_MAX_SEG_SIZE_NONE;
    p_buf_hdr->TCP_WinSize              =  NET_TCP_WIN_SIZE_NONE;
    p_buf_hdr->TCP_WinSizeLast          =  NET_TCP_WIN_SIZE_NONE;
    p_buf_hdr->TCP_RTT_TS_Rxd_ms        =  NET_TCP_TX_RTT_TS_NONE;
    p_buf_hdr->TCP_RTT_TS_Txd_ms        =  NET_TCP_TX_RTT_TS_NONE;
    p_buf_hdr->TCP_Flags                =  NET_TCP_FLAG_NONE;
#endif


    p_buf_hdr->Conn_ID                  =  NET_CONN_ID_NONE;
    p_buf_hdr->Conn_ID_Transport        =  NET_CONN_ID_NONE;
    p_buf_hdr->Conn_ID_App              =  NET_CONN_ID_NONE;
    p_buf_hdr->ConnType                 =  NET_CONN_TYPE_CONN_NONE;

}


/*
*********************************************************************************************************
*                                          NetBuf_Discard()
*
* Description : (1) Discard an invalid/corrupted network buffer or network buffer data area from
*                       available buffer pools :
*
*                   (a) Discard buffer from available buffer pool                       See Note #2
*                   (b) Update  buffer pool statistics                                  See Note #3
*
*               (2) Assumes buffer is invalid/corrupt & MUST be removed.  Buffer removed simply by
*                   NOT returning the buffer back to any buffer pool.
*
*
* Argument(s) : if_nbr      Interface number to discard   network buffer or network buffer data area.
*               ------      Argument checked in NetBuf_Get(),
*                                               NetBuf_FreeBufDataAreaRx(),
*                                               NetBuf_FreeHandler().
*
*               p_buf       Pointer to an invalid/corrupt network buffer or network buffer data area.
*
*               pstat_pool  Pointer to a network buffer statistics pool.
*
* Return(s)   : none.
*
* Caller(s)   : NetBuf_Get(),
*               NetBuf_FreeBufDataAreaRx(),
*               NetBuf_FreeHandler().
*
* Note(s)     : (3) (a) If the lost network buffer 'Type' is   known, then the unrecoverable buffer will
*                       be removed from the interface's appropriate buffer statistic pools.
*
*                   (b) If the lost network buffer 'Type' is unknown, then the buffer pool that lost the
*                       network buffer can NOT be determined.  Instead, a NULL statistics pool is passed
*                       & the unrecoverable buffer will remain unaccounted for in one of the interface's
*                       buffer pools.
*********************************************************************************************************
*/

static  void  NetBuf_Discard (NET_IF_NBR      if_nbr,
                              void           *p_buf,
                              NET_STAT_POOL  *pstat_pool)
{
    NET_ERR  err;

                                                                /* ------------------- DISCARD BUF -------------------- */
   (void)&if_nbr;                                               /* Prevent possible 'variable unused' warnings.         */
   (void)&p_buf;                                                 /* See Note #2.                                         */

                                                                /* --------------- UPDATE DISCARD STATS --------------- */
    if (pstat_pool != (NET_STAT_POOL *)0) {
        NetStat_PoolEntryLostInc(pstat_pool, &err);             /* See Note #3a.                                        */
    }

    NET_CTR_ERR_INC(Net_ErrCtrs.IFs.IF[if_nbr].BufLostCtr);
}

#endif  /* NET_BUF_MODULE_EN */
