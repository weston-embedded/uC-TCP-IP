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
*                                       NETWORK UTILITY LIBRARY
*
*                                              AVR32 UC3
*                                            GNU Compiler
*
* Filename : net_util_a.asm
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) Assumes AVR32 CPU mode configured for Big Endian.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                          PUBLIC FUNCTIONS
*********************************************************************************************************
*/

        .global  NetUtil_16BitSumDataCalcAlign_32


/*
*********************************************************************************************************
*                                     CODE GENERATION DIRECTIVES
*********************************************************************************************************
*/

        .section .text, "ax"


/*
*********************************************************************************************************
*                                 NetUtil_16BitSumDataCalcAlign_32()
*
* Description : Calculate 16-bit sum on 32-bit word-aligned data.
*
* Argument(s) : pdata_32    Pointer  to 32-bit word-aligned data (see Note #2).
*
*               size        Size of data.
*
* Return(s)   : 16-bit sum (see Notes #1 & #3).
*
* Caller(s)   : NetUtil_16BitSumDataCalc().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) Computes the sum of consecutive 16-bit values.
*
*               (2) Since many word-aligned processors REQUIRE  that multi-octet words be located on word-
*                   aligned addresses, sum calculation REQUIREs that 32-bit words are accessed on addresses
*                   that are multiples of 4 octets.
*
*               (3) The 16-bit sum MUST be returned in Big Endian/Network order.
*
*                   See 'net_util.c  NetUtil_16BitSumDataCalc()  Note #5b'.
*********************************************************************************************************
*/
/*
CPU_INT32U  NetUtil_16BitSumDataCalcAlign_32 (void        *pdata_32,   @       ==>  R0
                                              CPU_INT32U   size)       @       ==>  R1
                                                                       @  sum  ==>  R2
*/

NetUtil_16BitSumDataCalcAlign_32:
        PUSHM   R0-R7, LR

        MOV     R10, 0
        SUB     R11, 4*9
        BRLT    NetUtil_16BitSumDataCalcAlign_32_Chk_Rem

NetUtil_16BitSumDataCalcAlign_32_36:
        LDM     R12++, R0-R8                                    /* Load nine 32-bit registers.                          */

        BFEXTU  R9,  R0,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R0,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R1,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R1,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R2,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R2,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R3,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R3,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R4,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R4,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R5,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R5,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R6,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R6,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R7,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R7,  16, 16
        ADD     R10, R9
        BFEXTU  R9,  R8,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R8,  16, 16
        ADD     R10, R9

        SUB     R11, 4*9
        BRGE    NetUtil_16BitSumDataCalcAlign_32_36

NetUtil_16BitSumDataCalcAlign_32_Chk_Rem:
        NEG     R11
        ADD     PC, PC, R11 << 2                                /* Jump to remaining position.                          */

        NOP
        NOP
        NOP
        NOP
        NOP
        NOP

        .rept   8
        LD.W    R0,  R12
        SUB     R12, -4
        BFEXTU  R9,  R0,   0, 16
        ADD     R10, R9
        BFEXTU  R9,  R0,  16, 16
        ADD     R10, R9
        .endr

        MOV     R12, R10
        POPM    R0-R7, PC

        .end

