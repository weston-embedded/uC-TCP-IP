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
*                                           Renesas H8S2472
*
* Filename : net_dev_h8s2472_isr.c
* Version  : V3.06.01
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
**                                         Global Functions
*********************************************************************************************************
*********************************************************************************************************
*/

#pragma asm
        .IMPORT _OSTCBCur
        .IMPORT _OSIntExit
        .IMPORT _OSIntNesting
        .IMPORT _NetBSP_ENET_ISR_Handler
#pragma endasm

/*
*********************************************************************************************************
*                                               MACROS
*********************************************************************************************************
*/

#pragma asm
          .MACRO   PUSHALL
          PUSH.L    ER0
          PUSH.L    ER1
          PUSH.L    ER2
          PUSH.L    ER3
          PUSH.L    ER4
          PUSH.L    ER5
          PUSH.L    ER6
          .ENDM

          .MACRO   POPALL
          POP.L     ER6
          POP.L     ER5
          POP.L     ER4
          POP.L     ER3
          POP.L     ER2
          POP.L     ER1
          POP.L     ER0
          .ENDM
#pragma endasm

/*
*********************************************************************************************************
*                                             NetDEV_ISR()
*
* Description : EtherC ISR, calls the ISR handler :
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : This is an ISR.

*********************************************************************************************************
*/

#pragma noregsave NetDev_ISR
#pragma interrupt (NetDev_ISR(vect=119))
void  NetDev_ISR (void)
{
#pragma asm
          PUSHALL

_NetDev_ISR1:
          MOV.B    @_OSIntNesting, R6L
          INC.B    R6L
          MOV.B    R6L, @_OSIntNesting
          CMP.B    #1,R6L
          BNE      _NetDev_ISR1_1

          MOV.L    @_OSTCBCur, ER6
          MOV.L    ER7, @ER6

_NetDev_ISR1_1:
          JSR      @_NetBSP_ENET_ISR_Handler
          JSR      @_OSIntExit

          POPALL
#pragma endasm
}

