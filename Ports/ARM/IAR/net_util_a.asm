;********************************************************************************************************
;                                              uC/TCP-IP
;                                      The Embedded TCP/IP Suite
;
;                    Copyright 2004-2021 Silicon Laboratories Inc. www.silabs.com
;
;                                 SPDX-License-Identifier: APACHE-2.0
;
;               This software is subject to an open source license and is distributed by
;                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
;                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
;
;********************************************************************************************************


;********************************************************************************************************
;
;                                       NETWORK UTILITY LIBRARY
;
;                                                ARM
;                                            IAR Compiler
;
; Filename : net_util_a.asm
; Version  : V3.06.01
;********************************************************************************************************
; Note(s)  : (1) Assumes ARM CPU mode configured for Little Endian.
;********************************************************************************************************


;********************************************************************************************************
;                                          PUBLIC FUNCTIONS
;********************************************************************************************************

        PUBLIC  NetUtil_16BitSumDataCalcAlign_32


;********************************************************************************************************
;                                     CODE GENERATION DIRECTIVES
;********************************************************************************************************

        RSEG CODE:CODE:NOROOT(2)
        CODE32


;********************************************************************************************************
;                                 NetUtil_16BitSumDataCalcAlign_32()
;
; Description : Calculate 16-bit sum on 32-bit word-aligned data.
;
; Argument(s) : pdata_32    Pointer  to 32-bit word-aligned data (see Note #2).
;
;               size        Size of data.
;
; Return(s)   : 16-bit sum (see Notes #1 & #3).
;
; Caller(s)   : NetUtil_16BitSumDataCalc().
;
;               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
;               application function(s).
;
; Note(s)     : (1) Computes the sum of consecutive 16-bit values.
;
;               (2) Since many word-aligned processors REQUIRE  that multi-octet words be located on word-
;                   aligned addresses, sum calculation REQUIREs that 32-bit words are accessed on addresses
;                   that are multiples of 4 octets.
;
;               (3) The 16-bit sum MUST be returned in Big Endian/Network order.
;
;                   See 'net_util.c  NetUtil_16BitSumDataCalc()  Note #5b'.
;
;                   (a) Assumes Little Endian CPU Mode (see 'net_util_a.asm  Note #1') thus requiring the
;                       16-bit octets of the 32-bit data to be swapped.
;
;                       #### However, the 16-bit octets COULD be swapped after the 16-bit sum is fully
;                       calculated.
;
;               (4) (a) A "straightforward" assembly implementation would do the following for each
;                       32-bit word:
;
;                       (1) Extract the lower  16-bit half-word from the 32-bit word.
;                       (2) Swap    the lower  16-bit half-word's bytes.
;                       (3) Add     the lower  16-bit half-word to the sum.
;                       (4) Extract the higher 16-bit half-word from the 32-bit word.
;                       (5) Swap    the higher 16-bit half-word's bytes.
;                       (6) Add     the higher 16-bit half-word to the sum.
;
;                       If the initial 32-bit word were 0x11223344, then 0x2211 and 0x4433 would be the
;                       two 16-bit half-words that eventually get added to the sum.
;
;                   (b) A faster assembly implementation which can accelerate this process would do the
;                       following for each 32-bit word:
;
;                       (1) Rotate the 32-bit word right 8 bits.
;                       (2) Extract the lower  16-bit half-word from the rotated 32-bit word.
;                       (3) Add     the lower  16-bit half-word to the sum.
;                       (4) Extract the higher 16-bit half-word from the rotated 32-bit word.
;                       (5) Add     the higher 16-bit half-word to the sum.
;
;                       If the initial 32-bit word were 0x11223344, then 0x4411 and 0x2233 would be the
;                       two 16-bit half-words that eventually get added to the sum.  Notice these
;                       half-words are equal to those formed in the straightforward implementation with
;                       the lower octets swapped.  Since the algorithm does not care about the order in
;                       which the bytes are accumulated (only the position of the bytes), this does not
;                       affect the outcome.
;********************************************************************************************************
; CPU_INT32U  NetUtil_16BitSumDataCalcAlign_32 (void        *pdata_32,   @       ==>  R0
;                                               CPU_INT32U   size)       @       ==>  R1
;                                                                        @  sum  ==>  R2

NetUtil_16BitSumDataCalcAlign_32:
        STMFD       SP!, {R2-R12}

        MOV         R2, #0
        B           NetUtil_16BitSumDataCalcAlign_32_0

NetUtil_16BitSumDataCalcAlign_32_1:
        LDMIA       R0!, {R5-R12}                           ; Calc sum of sixteen 16-bit words ...
                                                            ; ... using eight 32-bit CPU regs.
        MOV         R3,  R5,  ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R6,  ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R7,  ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R8,  ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R9,  ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R10, ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R11, ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        MOV         R3,  R12, ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        SUB         R1, R1, #(4*8*1)

NetUtil_16BitSumDataCalcAlign_32_0:
        CMP         R1, #(4*8*1)                            ; end of loop
        BCS         NetUtil_16BitSumDataCalcAlign_32_1

        B           NetUtil_16BitSumDataCalcAlign_32_2



NetUtil_16BitSumDataCalcAlign_32_3:
        LDMIA       R0!, {R5}                               ; Calc sum of two 16-bit words ...
                                                            ; ... using   one 32-bit CPU reg.

        MOV         R3,  R5,  ROR #8
        MOV         R4,  R3,  LSL #16
        ADD         R2,  R2,  R4,  LSR #16
        ADD         R2,  R2,  R3,  LSR #16

        SUB         R1, R1, #(4*1*1)

NetUtil_16BitSumDataCalcAlign_32_2:

        CMP         R1, #(4*1*1)                            ; end of loop
        BCS         NetUtil_16BitSumDataCalcAlign_32_3

        MOV         R0, R2
        LDMFD       SP!, {R2-R12}
        BX          LR                                      ; return


        END

