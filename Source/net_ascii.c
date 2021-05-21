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
*                                         NETWORK ASCII LIBRARY
*
* Filename : net_ascii.c
* Version  : V3.06.01
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
#define    NET_ASCII_MODULE
#include  "net_ascii.h"

#include  "net_cfg_net.h"
#ifdef  NET_IPv4_MODULE_EN
#include  "../IP/IPv4/net_ipv4.h"
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  "../IP/IPv6/net_ipv6.h"
#endif

#include  "net_util.h"
#include  <cpu.h>
#include  <cpu_core.h>
#include  <lib_def.h>
#include  <lib_ascii.h>
#include  <lib_str.h>

/*
*********************************************************************************************************
*                                        NetASCII_Str_to_MAC()
*
* Description : Convert an Ethernet MAC address ASCII string to an Ethernet MAC address.
*
* Argument(s) : p_addr_mac_ascii    Pointer to an ASCII string that contains a MAC address (see Note #1).
*
*               p_addr_mac          Pointer to a memory buffer that will receive the converted MAC address
*                                       (see Note #2) :
*
*                                       MAC address represented by ASCII string,         if NO error(s).
*
*                                       MAC address cleared to all zeros (see Note #2c), otherwise.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ASCII_ERR_NONE                  MAC address successfully converted.
*                               NET_ERR_FAULT_NULL_PTR              Argument 'paddr_mac_ascii'/'paddr_mac'
*                                                                       passed a NULL pointer.
*                               NET_ASCII_ERR_INVALID_STR_LEN       Invalid ASCII string    length.
*                               NET_ASCII_ERR_INVALID_CHAR          Invalid ASCII character.
*                               NET_ASCII_ERR_INVALID_CHAR_LEN      Invalid ASCII character length.
*                               NET_ASCII_ERR_INVALID_CHAR_SEQ      Invalid ASCII character sequence.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) RFC #1700, Section 'ETHERNET VENDOR ADDRESS COMPONENTS' states that "Ethernet
*                           addresses ... should be written hyphenated by octets (e.g., 12-34-56-78-9A-BC)".
*
*                       (2) In other words, the (Ethernet) MAC address notation separates six hexadecimal
*                           octet values by the hyphen character ('-') or by the colon character (':').
*                           Each hexadecimal value represents one octet of the MAC address starting with
*                           the most significant octet in network-order.
*
*                               MAC Address Examples :
*
*                                      MAC ADDRESS NOTATION        HEXADECIMAL EQUIVALENT
*
*                                       "00-1A-07-AC-22-09"     =     0x001A07AC2209
*                                       "76:4E:01:D2:8C:0B"     =     0x764E01D28C0B
*                                       "80-Db-fE-0b-34-52"     =     0X80DBFE0B3452
*                                        --             --              --        --
*                                        ^               ^              ^          ^
*                                        |               |              |          |
*                                       MSO             LSO            MSO        LSO
*
*                               where
*                                       MSO             Most  Significant Octet in MAC Address
*                                       LSO             Least Significant Octet in MAC Address
*
*
*                   (b) Therefore, the MAC address ASCII string MUST :
*
*                       (1) Include ONLY hexadecimal values & the hyphen character ('-') or the colon
*                           character (':') ; ALL other characters are trapped as invalid, including
*                           any leading or trailing characters.
*
*                       (2) (A) Include EXACTLY six hexadecimal values ...
*                           (B) (1) ... separated by                   ...
*                               (2) ... EXACTLY five                   ...
*                                   (a) hyphen characters or           ...
*                                   (b) colon  characters;             ...
*                           (C) ... & MUST be terminated with the NULL character.
*
*                       (3) Ensure that each hexadecimal value's number of digits does NOT exceed the
*                           maximum number of digits (2).
*
*                           (A) However, any hexadecimal value's number of digits MAY be less than the
*                               maximum number of digits.
*
*               (2) (a) The size of the memory buffer that will receive the converted MAC address MUST
*                       be greater than or equal to NET_ASCII_NBR_OCTET_ADDR_MAC.
*
*                   (b) MAC address memory buffer array accessed by octets.
*
*                   (c) MAC address memory buffer array cleared in case of any error(s).
*********************************************************************************************************
*/

void  NetASCII_Str_to_MAC (CPU_CHAR    *p_addr_mac_ascii,
                           CPU_INT08U  *p_addr_mac,
                           NET_ERR     *p_err)
{
    CPU_CHAR    *p_char_cur;
    CPU_CHAR    *p_char_prev;
    CPU_INT08U  *p_addr_cur;
    CPU_INT08U   addr_octet_val;
    CPU_INT08U   addr_octet_val_dig;
    CPU_INT08U   addr_nbr_octet;
    CPU_INT08U   addr_nbr_octet_dig;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif
                                                                        /* -------------- VALIDATE PTRS --------------- */
    if (p_addr_mac == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (p_addr_mac_ascii == (CPU_CHAR *)0) {
        Mem_Clr((void     *)p_addr_mac,                                  /* Clr rtn addr on err (see Note #2c).          */
                (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }


                                                                        /* ----------- CONVERT MAC ADDR STR ----------- */
    p_char_cur         = (CPU_CHAR   *)p_addr_mac_ascii;
    p_char_prev        = (CPU_CHAR   *)0;
    p_addr_cur         = (CPU_INT08U *)p_addr_mac;
    addr_octet_val     =  0x00u;
    addr_nbr_octet     =  0u;
    addr_nbr_octet_dig =  0u;

    while ((p_char_cur != (CPU_CHAR *)  0 ) &&                          /* Parse ALL non-NULL chars in ASCII str.       */
          (*p_char_cur != (CPU_CHAR  )'\0')) {

        switch (*p_char_cur) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                 addr_nbr_octet_dig++;                                  /* If nbr digs > max (see Note #1b3); ...       */
                 if (addr_nbr_octet_dig > NET_ASCII_CHAR_MAX_OCTET_ADDR_MAC) {
                     Mem_Clr((void     *)p_addr_mac,                    /* ... clr rtn addr  (see Note #2c)   ...       */
                             (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
                    *p_err = NET_ASCII_ERR_INVALID_CHAR_LEN;            /* ... & rtn err.                               */
                     return;
                 }

                 switch (*p_char_cur) {                                 /* Convert ASCII char into hex val.             */
                     case '0':
                     case '1':
                     case '2':
                     case '3':
                     case '4':
                     case '5':
                     case '6':
                     case '7':
                     case '8':
                     case '9':
                          addr_octet_val_dig = (CPU_INT08U) (*p_char_cur - '0');
                          break;


                     case 'A':
                     case 'B':
                     case 'C':
                     case 'D':
                     case 'E':
                     case 'F':
                          addr_octet_val_dig = (CPU_INT08U)((*p_char_cur - 'A') + 10u);
                          break;


                     case 'a':
                     case 'b':
                     case 'c':
                     case 'd':
                     case 'e':
                     case 'f':
                          addr_octet_val_dig = (CPU_INT08U)((*p_char_cur - 'a') + 10u);
                          break;


                     default:                                           /* See Note #1b1.                               */
                          Mem_Clr((void     *)p_addr_mac,               /* Clr rtn addr on err (see Note #2c).          */
                                  (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
                         *p_err = NET_ASCII_ERR_INVALID_CHAR;
                          return;
                 }
                                                                        /* Merge ASCII char into hex val.               */
                 addr_octet_val <<= DEF_NIBBLE_NBR_BITS;
                 addr_octet_val  += addr_octet_val_dig;
                 break;


            case '-':
            case ':':
                 if (p_char_prev == (CPU_CHAR *)0) {                    /* If NO prev char  (see Note #1b2B1); ...      */
                     Mem_Clr((void     *)p_addr_mac,                    /* ... clr rtn addr (see Note #2c)     ...      */
                             (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
                    *p_err = NET_ASCII_ERR_INVALID_CHAR_SEQ;            /* ... & rtn err.                               */
                     return;
                 }

                 if ((*p_char_prev == (CPU_CHAR)'-') ||                 /* If prev char a hyphen                   ...  */
                     (*p_char_prev == (CPU_CHAR)':')) {                 /* ... or       a colon (see Note #1b2B1); ...  */
                     Mem_Clr((void     *)p_addr_mac,                    /* ... clr rtn addr     (see Note #2c)     ...  */
                             (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
                    *p_err = NET_ASCII_ERR_INVALID_CHAR_SEQ;            /* ... & rtn err.                               */
                     return;
                 }

                 addr_nbr_octet++;
                 if (addr_nbr_octet >= NET_ASCII_NBR_OCTET_ADDR_MAC) {  /* If nbr octets > max (see Note #1b2A); ...    */
                     Mem_Clr((void     *)p_addr_mac,                    /* ... clr rtn addr    (see Note #2c)    ...    */
                             (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
                    *p_err = NET_ASCII_ERR_INVALID_STR_LEN;             /* ... & rtn err.                               */
                     return;
                 }

                                                                        /* Merge hex octet val into MAC addr.           */
                *p_addr_cur++        = addr_octet_val;
                 addr_octet_val     = 0x00u;
                 addr_nbr_octet_dig = 0u;
                 break;


            default:                                                    /* See Note #1b1.                               */
                 Mem_Clr((void     *)p_addr_mac,                        /* Clr rtn addr on err (see Note #2c).          */
                         (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
                *p_err = NET_ASCII_ERR_INVALID_CHAR;
                 return;
        }

        p_char_prev = p_char_cur;
        p_char_cur++;
    }

    if (p_char_cur == (CPU_CHAR *)0) {                                  /* If NULL ptr in ASCII str (see Note #1b1); .. */
        Mem_Clr((void     *)p_addr_mac,                                 /* .. clr rtn addr          (see Note #2c)   .. */
                (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
       *p_err = NET_ERR_FAULT_NULL_PTR;                                 /* .. & rtn err.                                */
        return;
    }

    if (p_char_prev == (CPU_CHAR *)0) {                                 /* If NULL        ASCII str (see Note #1b2); .. */
        Mem_Clr((void     *)p_addr_mac,                                 /* .. clr rtn addr          (see Note #2c)   .. */
                (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
       *p_err = NET_ASCII_ERR_INVALID_STR_LEN;                          /* .. & rtn err.                                */
        return;
    }

    if ((*p_char_prev == (CPU_CHAR)'-') ||                              /* If last char a hyphen                   ..   */
        (*p_char_prev == (CPU_CHAR)':')) {                              /* .. or        a colon (see Note #1b2B1); ..   */
        Mem_Clr((void     *)p_addr_mac,                                 /* .. clr rtn addr      (see Note #2c)     ..   */
                (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
       *p_err = NET_ASCII_ERR_INVALID_CHAR_SEQ;                         /* .. & rtn err.                                */
        return;
    }

    addr_nbr_octet++;
    if (addr_nbr_octet != NET_ASCII_NBR_OCTET_ADDR_MAC) {               /* If != nbr MAC addr octets (see Note #1b2A);  */
        Mem_Clr((void     *)p_addr_mac,                                 /* .. clr rtn addr           (see Note #2c)  .. */
                (CPU_SIZE_T)NET_ASCII_NBR_OCTET_ADDR_MAC);
       *p_err = NET_ASCII_ERR_INVALID_STR_LEN;                          /* .. & rtn err.                                */
        return;
    }

                                                                        /* Merge hex octet val into final MAC addr.     */
   *p_addr_cur = addr_octet_val;

   *p_err      = NET_ASCII_ERR_NONE;
}


/*
*********************************************************************************************************
*                                        NetASCII_MAC_to_Str()
*
* Description : Convert an Ethernet MAC address into an Ethernet MAC address ASCII string.
*
* Argument(s) : paddr_mac           Pointer to a memory buffer that contains the MAC address (see Note #2).
*
*               paddr_mac_ascii     Pointer to an ASCII character array that will receive the return MAC
*                                       address ASCII string from this function (see Note #1b).
*
*               hex_lower_case      Format alphabetic hexadecimal characters in lower case :
*
*                                       DEF_NO      Format alphabetic hexadecimal characters in upper case.
*                                       DEF_YES     Format alphabetic hexadecimal characters in lower case.
*
*               hex_colon_sep       Separate hexadecimal values with a colon character :
*
*                                       DEF_NO      Separate hexadecimal values with a hyphen character
*                                                       (see Note #1b1B2a).
*                                       DEF_YES     Separate hexadecimal values with a colon  character
*                                                       (see Note #1b1B2b).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ASCII_ERR_NONE                  ASCII string successfully formatted.
*                               NET_ERR_FAULT_NULL_PTR              Argument 'paddr_mac'/'paddr_mac_ascii'
*                                                                       passed a NULL pointer.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) RFC #1700, Section 'ETHERNET VENDOR ADDRESS COMPONENTS' states that "Ethernet
*                           addresses ... should be written hyphenated by octets (e.g., 12-34-56-78-9A-BC)".
*
*                       (2) In other words, the (Ethernet) MAC address notation separates six hexadecimal
*                           octet values by the hyphen character ('-') or by the colon character (':').
*                           Each hexadecimal value represents one octet of the MAC address starting with
*                           the most significant octet in network-order.
*
*                               MAC Address Examples :
*
*                                      MAC ADDRESS NOTATION        HEXADECIMAL EQUIVALENT
*
*                                       "00-1A-07-AC-22-09"     =     0x001A07AC2209
*                                       "76:4E:01:D2:8C:0B"     =     0x764E01D28C0B
*                                       "80-Db-fE-0b-34-52"     =     0X80DBFE0B3452
*                                        --             --              --        --
*                                        ^               ^              ^          ^
*                                        |               |              |          |
*                                       MSO             LSO            MSO        LSO
*
*                               where
*                                       MSO             Most  Significant Octet in MAC Address
*                                       LSO             Least Significant Octet in MAC Address
*
*
*                   (b) (1) The return MAC address ASCII string :
*
*                           (A) Formats EXACTLY six hexadecimal values ...
*                           (B) (1) ... separated by                   ...
*                               (2) ... EXACTLY five                   ...
*                                   (a) hyphen characters or           ...
*                                   (b) colon  characters;             ...
*                           (C) ... & terminated with the NULL character.
*
*                       (2) The size of the ASCII character array that will receive the returned MAC address
*                           ASCII string MUST be greater than or equal to NET_ASCII_LEN_MAX_ADDR_MAC.
*
*               (2) (a) The size of the memory buffer that contains the MAC address SHOULD be greater than
*                       or equal to NET_ASCII_NBR_OCTET_ADDR_MAC.
*
*                   (b) MAC address memory buffer array accessed by octets.
*********************************************************************************************************
*/

void  NetASCII_MAC_to_Str (CPU_INT08U   *p_addr_mac,
                           CPU_CHAR     *p_addr_mac_ascii,
                           CPU_BOOLEAN   hex_lower_case,
                           CPU_BOOLEAN   hex_colon_sep,
                           NET_ERR      *p_err)
{
    CPU_CHAR    *p_char;
    CPU_INT08U  *p_addr;
    CPU_INT08U   addr_octet_val;
    CPU_INT08U   addr_octet_dig_val;
    CPU_INT08U   addr_octet_nbr_shifts;
    CPU_INT08U   i;
    CPU_INT08U   j;

#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION(;);
    }
#endif

                                                                        /* -------------- VALIDATE PTRS --------------- */
    if (p_addr_mac == (CPU_INT08U *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
    if (p_addr_mac_ascii == (CPU_CHAR *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }


                                                                        /* ------------- CONVERT MAC ADDR ------------- */
    p_addr = p_addr_mac;
    p_char = p_addr_mac_ascii;

    for (i = NET_ASCII_NBR_OCTET_ADDR_MAC; i > 0u; i--) {               /* Parse ALL addr octets (see Note #1b1A).      */

        addr_octet_val = *p_addr;

        for (j = NET_ASCII_CHAR_MAX_OCTET_ADDR_MAC; j > 0u; j--) {      /* Parse ALL octet's hex digs.                  */
                                                                        /* Calc  cur octet's hex dig val.               */
            addr_octet_nbr_shifts = (j - 1u) * DEF_NIBBLE_NBR_BITS;
            addr_octet_dig_val    = (addr_octet_val >> addr_octet_nbr_shifts) & DEF_NIBBLE_MASK;
                                                                        /* Insert    octet's hex val ASCII dig.         */
            if (addr_octet_dig_val < 10u) {
               *p_char++ = (CPU_CHAR)(addr_octet_dig_val + '0');

            } else {
                if (hex_lower_case != DEF_YES) {
                   *p_char++ = (CPU_CHAR)((addr_octet_dig_val - 10u) + 'A');
                } else {
                   *p_char++ = (CPU_CHAR)((addr_octet_dig_val - 10u) + 'a');
                }
            }
        }

        if (i > 1u) {                                                   /* If NOT on last octet,                    ..  */
            if (hex_colon_sep != DEF_YES) {
               *p_char++ = '-';                                         /* .. insert hyphen char (see Note #1b1B2a) ..  */
            } else {
               *p_char++ = ':';                                         /* ..     or colon  char (see Note #1b1B2b).    */
            }
        }

        p_addr++;
    }

   *p_char = (CPU_CHAR)'\0';                                            /* Append NULL char (see Note #1b1C).           */

   *p_err  =  NET_ASCII_ERR_NONE;
}


/*
*********************************************************************************************************
*                                         NetASCII_Str_to_IP()
*
* Description : Convert string representation of IP address (IPv4 or IPv6) to their TCP/IP stack intern
*               representation.
*
* Argument(s) : p_addr_ip_ascii     Pointer to an ASCII string that contains a decimal IP address.
*
*               p_addr              Pointer to the variable that will received the converted IP address.
*
*               addr_max_len        Size of the variable that will received the converted IP address.
*
*                                       NET_IPv4_ADDR_SIZE
*                                       NET_IPv6_ADDR_SIZE
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*                               NET_ASCII_ERR_INVALID_CHAR_SEQ  Invalid Character sequence.
*
*                               ------------ RETURNED BY NetASCII_Str_to_IPv4() ------------
*                               See NetASCII_Str_to_IPv4() for additional return error code.
*
*                               ------------ RETURNED BY NetASCII_Str_to_IPv6() ------------
*                               See NetASCII_Str_to_IPv6() for additional return error code.
*
* Return(s)   : the IP family of the converted address, if no errors:
*                   NET_IP_ADDR_FAMILY_IPv4
*                   NET_IP_ADDR_FAMILY_IPv6
*               otherwise,
*                   NET_IP_ADDR_FAMILY_UNKNOWN
*
* Caller(s)   : Application
*
*               This function is a network protocol suite application programming interface (API) function
*               & MAY be called by application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

NET_IP_ADDR_FAMILY  NetASCII_Str_to_IP (CPU_CHAR    *p_addr_ip_ascii,
                                        void        *p_addr,
                                        CPU_INT08U   addr_max_len,
                                        NET_ERR     *p_err)
{
#ifdef NET_IPv4_MODULE_EN
    NET_IPv4_ADDR       *p_addr_ipv4;
#endif
#ifdef NET_IPv6_MODULE_EN
    NET_IPv6_ADDR       *p_addr_ipv6;
#endif
    CPU_CHAR            *p_sep;
    NET_IP_ADDR_FAMILY   addr_family;
    CPU_INT16S           result;

                                                                /* "localhost" string verfication.                      */
    result = Str_Cmp(p_addr_ip_ascii, NET_ASCII_STR_LOCAL_HOST);
    if (result == 0) {
#ifdef NET_IPv4_MODULE_EN
        addr_family = NET_IP_ADDR_FAMILY_IPv4;                  /* By default, return IPv4 local host address.          */
        p_addr_ipv4 = (NET_IPv4_ADDR *)p_addr;
       *p_addr_ipv4 = NET_IPv4_ADDR_LOCAL_HOST_ADDR;
#else
        addr_family = NET_IP_ADDR_FAMILY_IPv6;                  /* If IPv4 is not enabled, return IPv6 loopback address.*/
        p_addr_ipv6 = (NET_IPv6_ADDR *)p_addr;
       *p_addr_ipv6 = NET_IPv6_ADDR_LOOPBACK;
#endif
       *p_err = NET_ASCII_ERR_NONE;
        return (addr_family);
    }
                                                                /* ----------- DETERMINE IP ADDRESS FAMILY ------------ */
    p_sep = Str_Char(p_addr_ip_ascii, ASCII_CHAR_FULL_STOP);    /* Search for IPv4 separator '.'                        */
    if (p_sep != DEF_NULL) {
        p_sep = p_addr_ip_ascii;
        while (*p_sep != ASCII_CHAR_NULL) {
            if (ASCII_IS_DIG(*p_sep) == DEF_FALSE) {
                if (*p_sep != ASCII_CHAR_FULL_STOP) {
                    addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;
                   *p_err       = NET_ASCII_ERR_INVALID_CHAR_SEQ;
                    return (addr_family);
                }
            }
            p_sep++;
        }
        addr_family = NET_IP_ADDR_FAMILY_IPv4;
    } else {
        p_sep = Str_Char(p_addr_ip_ascii, ASCII_CHAR_COLON);    /* Search for IPv6 separator ':'                        */
        if (p_sep != DEF_NULL) {
            addr_family = NET_IP_ADDR_FAMILY_IPv6;
        } else {
            addr_family = NET_IP_ADDR_FAMILY_UNKNOWN;
           *p_err       = NET_ASCII_ERR_INVALID_CHAR_SEQ;
            return (addr_family);
        }
    }

    switch (addr_family) {
        case NET_IP_ADDR_FAMILY_IPv4:
#ifdef NET_IPv4_MODULE_EN
             p_addr_ipv4 = (NET_IPv4_ADDR *)p_addr;
            *p_addr_ipv4 = NetASCII_Str_to_IPv4(p_addr_ip_ascii, p_err);
#else
            *p_err = NET_ASCII_ERR_IP_FAMILY_NOT_PRESENT;
#endif
             break;


        case NET_IP_ADDR_FAMILY_IPv6:
#ifdef NET_IPv6_MODULE_EN
            p_addr_ipv6 = (NET_IPv6_ADDR *)p_addr;
           *p_addr_ipv6 = NetASCII_Str_to_IPv6(p_addr_ip_ascii, p_err);
#else
            *p_err = NET_ASCII_ERR_IP_FAMILY_NOT_PRESENT;
#endif
             break;


        case NET_IP_ADDR_FAMILY_UNKNOWN:
        default:
            *p_err = NET_ASCII_ERR_INVALID_CHAR_SEQ;
             break;
    }

   (void)&addr_max_len;

    return (addr_family);
}


/*
*********************************************************************************************************
*                                       NetASCII_Str_to_IPv4()
*
* Description : Convert an IPv4 address ASCII string in dotted-decimal notation to a network protocol
*                   IPv4 address in host-order.
*
* Argument(s) : p_addr_ip_ascii     Pointer to an ASCII string that contains a dotted-decimal IPv4 address
*                                       (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ASCII_ERR_NONE                  IPv4 address successfully converted.
*                               NET_ASCII_ERR_INVALID_DOT_NBR       Invalid ASCII IPv4 addr dot count.
*
*                                                                   - RETURNED BY NetASCII_Str_to_IP_Handler() : -
*                               NET_ERR_FAULT_NULL_PTR              Argument 'paddr_ip_ascii' passed a
*                                                                       NULL pointer.
*                               NET_ASCII_ERR_INVALID_STR_LEN       Invalid ASCII string    length.
*                               NET_ASCII_ERR_INVALID_CHAR          Invalid ASCII character.
*                               NET_ASCII_ERR_INVALID_CHAR_LEN      Invalid ASCII character length.
*                               NET_ASCII_ERR_INVALID_CHAR_VAL      Invalid ASCII character value.
*                               NET_ASCII_ERR_INVALID_CHAR_SEQ      Invalid ASCII character sequence.
*
* Return(s)   : Host-order IPv4 address represented by ASCII string, if NO error(s).
*
*               NET_IPv4_ADDR_NONE,                                  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) RFC #1983 states that "dotted decimal notation ... refers [to] IP addresses of the
*                           form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                           address".
*
*                       (2) In other words, the dotted-decimal IP address notation separates four decimal octet
*                           values by the dot, or period, character ('.').  Each decimal value represents one
*                           octet of the IP address starting with the most significant octet in network-order.
*
*                               IP Address Examples :
*
*                                      DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                           127.0.0.1           =       0x7F000001
*                                           192.168.1.64        =       0xC0A80140
*                                           255.255.255.0       =       0xFFFFFF00
*                                           ---         -                 --    --
*                                            ^          ^                 ^      ^
*                                            |          |                 |      |
*                                           MSO        LSO               MSO    LSO
*
*                                   where
*                                           MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                           LSO        Least Significant Octet in Dotted Decimal IP Address
*
*
*                   (b) Therefore, the dotted-decimal IP address ASCII string MUST :
*
*                       (1) Include ONLY decimal values & the dot, or period, character ('.') ; ALL other
*                           characters are trapped as invalid, including any leading or trailing characters.
*
*                       (2) (A) Include UP TO four decimal values   ...
*                           (B) (1) ... separated by                  ...
*                               (2) ... UP TO three dot characters; ...
*                           (C) ... & MUST be terminated with the NULL character.
*
*                       (3) Ensure that each decimal value's number of decimal digits, including leading
*                           zeros, does NOT exceed      the maximum number of digits (10).
*
*                           (A) However, any decimal value's number of decimal digits, including leading
*                               zeros, MAY be less than the maximum number of digits.
*
*                       (4) Ensure that each decimal value does NOT exceed the maximum value for its form:
*
*                           (1) a.b.c.d - 255.255.255.255
*                           (2) a.b.c   - 255.255.65535
*                           (3) a.b     - 255.16777215
*                           (4) a       - 4294967295
*
*               (2) To avoid possible integer arithmetic overflow, the IP address octet arithmetic result
*                   MUST be declared as an integer data type with a greater resolution -- i.e. greater
*                   number of bits -- than the IP address octet data type(s).
*********************************************************************************************************
*/
#ifdef  NET_IPv4_MODULE_EN
NET_IPv4_ADDR  NetASCII_Str_to_IPv4 (CPU_CHAR  *p_addr_ip_ascii,
                                     NET_ERR   *p_err)
{
    NET_IPv4_ADDR  ip_addr;
    CPU_INT08U     dot_nbr;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_IPv4_ADDR)0);
    }
#endif
    ip_addr = NetASCII_Str_to_IPv4_Handler(p_addr_ip_ascii,
                                          &dot_nbr,
                                           p_err);

    if (*p_err != NET_ASCII_ERR_NONE) {
         return (NET_IPv4_ADDR_NONE);
    }

    if (dot_nbr != NET_ASCII_NBR_MAX_DOT_ADDR_IP) {
       *p_err     = NET_ASCII_ERR_INVALID_DOT_NBR;
        return (NET_IPv4_ADDR_NONE);
    }

    return (ip_addr);
}
#endif


/*
*********************************************************************************************************
*                                    NetASCII_Str_to_IPv4_Handler()
*
* Description : Convert an IPv4 address ASCII string in dotted-decimal notation to a network protocol
*                   IPv4 address in host-order.
*
* Argument(s) : paddr_ip_ascii      Pointer to an ASCII string that contains a dotted-decimal IPv4 address
*                                       (see Note #1).
*
*               pdot_nbr            Pointer to the number of dot found in the ASCII string.
*
*               p_err               Pointer to variable that will receive the return error code from this function :
*
*                                       NET_ASCII_ERR_NONE                  IPv4 address successfully converted.
*                                       NET_ERR_FAULT_NULL_PTR              Argument 'paddr_ip_ascii' passed a
*                                                                       NULL pointer.
*                                       NET_ASCII_ERR_INVALID_STR_LEN       Invalid ASCII string    length.
*                                       NET_ASCII_ERR_INVALID_CHAR          Invalid ASCII character.
*                                       NET_ASCII_ERR_INVALID_CHAR_LEN      Invalid ASCII character length.
*                                       NET_ASCII_ERR_INVALID_CHAR_VAL      Invalid ASCII character value.
*                                       NET_ASCII_ERR_INVALID_CHAR_SEQ      Invalid ASCII character sequence.
*                                       NET_ASCII_ERR_INVALID_PART_LEN      Invalid ASCII part      length.
*
* Return(s)   : Host-order IPv4 address represented by ASCII string, if NO error(s).
*
*               NET_IPv4_ADDR_NONE,                                    otherwise.
*
* Caller(s)   : NetASCII_Str_to_IP(),
*               inet_aton().
*
*               This function is an INTERNAL network protocol suite function & SHOULD NOT be called by
*               application function(s).
*
* Note(s)     : (1) (a) (1) RFC #1983 states that "dotted decimal notation ... refers [to] IP addresses of the
*                           form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                           address".
*
*                       (2) In other words, the dotted-decimal IP address notation separates four decimal octet
*                           values by the dot, or period, character ('.').  Each decimal value represents one
*                           octet of the IP address starting with the most significant octet in network-order.
*
*                               IP Address Examples :
*
*                                      DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                           127.0.0.1           =       0x7F000001
*                                           192.168.1.64        =       0xC0A80140
*                                           255.255.255.0       =       0xFFFFFF00
*                                           ---         -                 --    --
*                                            ^          ^                 ^      ^
*                                            |          |                 |      |
*                                           MSO        LSO               MSO    LSO
*
*                                   where
*                                           MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                           LSO        Least Significant Octet in Dotted Decimal IP Address
*
*               (2) IEEE Std 1003.1, 2004 Edition - inet_addr, inet_ntoa - IPv4 address manipulation:
*
*                   (a) Values specified using IPv4 dotted decimal notation take one of the following forms:
*
*                       (1) a.b.c.d - When four parts are specified, each shall be interpreted
*                                     as a byte of data and assigned, from left to right,
*                                     to the four bytes of an Internet address.
*
*                       (2) a.b.c   - When a three-part address is specified, the last part shall
*                                     be interpreted as a 16-bit quantity and placed in the
*                                     rightmost two bytes of the network address. This makes
*                                     the three-part address format convenient for specifying
*                                     Class B network addresses as "128.net.host".
*
*                       (3) a.b     - When a two-part address is supplied, the last part shall be
*                                     interpreted as a 24-bit quantity and placed in the
*                                     rightmost three bytes of the network address. This makes
*                                     the two-part address format convenient for specifying
*                                      Class A network addresses as "net.host".
*
*                       (4) a       - When only one part is given, the value shall be stored
*                                     directly in the network address without any byte rearrangement.
*
*                   (b) IP Address Examples :
*
*                                      DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                           127.0.0.1           =       0x7F000001
*                                           192.168.1.64        =       0xC0A80140
*                                           192.168.320         =       0xC0A80140
*                                           192.11010368        =       0xC0A80140
*                                           3232235840          =       0xC0A80140
*                                           255.255.255.0       =       0xFFFFFF00
*                                           ---         -                 --    --
*                                            ^          ^                 ^      ^
*                                            |          |                 |      |
*                                           MSO        LSO               MSO    LSO
*
*                                   where
*                                           MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                           LSO        Least Significant Octet in Dotted Decimal IP Address
*
*               (3) Therefore, the dotted-decimal IP address ASCII string MUST :
*
*                   (a) Include ONLY decimal values & the dot, or period, character ('.') ; ALL other
*                           characters are trapped as invalid, including any leading or trailing characters.
*
*                   (b) (1) Include UP TO four decimal values   ...
*                       (2) (A) ... separated by                ...
*                           (B) ... UP TO three dot characters; ...
*                       (3) ... & MUST be terminated with the NULL character.
*
*                   (c) Ensure that each decimal value's number of decimal digits, including leading
*                       zeros, does NOT exceed      the maximum number of digits (10).
*
*                       (1) However, any decimal value's number of decimal digits, including leading
*                           zeros, MAY be less than the maximum number of digits.
*
*                   (d) Ensure that each decimal value does NOT exceed the maximum value for its form:
*
*                       (1) a.b.c.d - 255.255.255.255
*                       (2) a.b.c   - 255.255.65535
*                       (3) a.b     - 255.16777215
*                       (4) a       - 4294967295
*
*               (4) To avoid possible integer arithmetic overflow, the IP address octet arithmetic result
*                   MUST be declared as an integer data type with a greater resolution -- i.e. greater
*                   number of bits -- than the IP address octet data type(s).
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
NET_IPv4_ADDR  NetASCII_Str_to_IPv4_Handler (CPU_CHAR    *p_addr_ip_ascii,
                                             CPU_INT08U  *p_dot_nbr,
                                             NET_ERR     *p_err)
{
    CPU_INT64U      addr_parts[NET_ASCII_NBR_MAX_DEC_PARTS];
    CPU_CHAR       *p_char_cur;
    CPU_CHAR       *p_char_prev;
    NET_IPv4_ADDR   addr_ip;
    CPU_INT64U     *p_addr_part_val;                                       /* See Note #4.                                 */
    CPU_INT08U      addr_nbr_octet;
    CPU_INT08U      addr_nbr_part_dig;


#if (NET_ERR_CFG_ARG_CHK_EXT_EN == DEF_ENABLED)
    if (p_err == (NET_ERR *)0) {
        CPU_SW_EXCEPTION((NET_IPv4_ADDR)0);
    }
#endif
                                                                        /* --------------- VALIDATE PTR --------------- */
    if (p_addr_ip_ascii == (CPU_CHAR *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (NET_IPv4_ADDR_NONE);
    }


                                                                        /* ----------- CONVERT IP ADDR STR ------------ */
    p_char_cur         =  p_addr_ip_ascii;
    p_char_prev        = (CPU_CHAR *)0;
    p_addr_part_val    = &addr_parts[0];
   *p_addr_part_val    =  0u;
    addr_nbr_octet     =  0u;
    addr_nbr_part_dig  =  0u;
   *p_dot_nbr          =  0u;

    while ((p_char_cur != (CPU_CHAR *)  0 ) &&                          /* Parse ALL non-NULL chars in ASCII str.       */
          (*p_char_cur != (CPU_CHAR  )'\0')) {

        switch (*p_char_cur) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                 addr_nbr_part_dig++;                                   /* If nbr digs > max (see Note #1b3), ...       */
                 if (addr_nbr_part_dig > NET_ASCII_CHAR_MAX_PART_ADDR_IP) {
                    *p_err =  NET_ASCII_ERR_INVALID_CHAR_LEN;           /* ... rtn err.                                 */
                     return (NET_IPv4_ADDR_NONE);
                 }

                                                                        /* Convert & merge ASCII char into decimal val. */
                *p_addr_part_val *= (CPU_INT16U)10u;
                *p_addr_part_val += (CPU_INT16U)(*p_char_cur - '0');

                 if (*p_addr_part_val > NET_ASCII_VAL_MAX_PART_ADDR_IP) {   /* If octet val > max (see Note #1b4), ...  */
                    *p_err =  NET_ASCII_ERR_INVALID_CHAR_VAL;               /* ... rtn err.                             */
                     return (NET_IPv4_ADDR_NONE);
                 }
                 break;


            case '.':
                 if (p_char_prev == (CPU_CHAR *)0) {                    /* If NO prev char     (see Note #1b2B1), ...   */
                    *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;           /* ... rtn err.                                 */
                     return (NET_IPv4_ADDR_NONE);
                 }

                 if (*p_char_prev == (CPU_CHAR)'.') {                   /* If prev char a dot  (see Note #1b2B1), ...   */
                     *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;          /* ... rtn err.                                 */
                      return (NET_IPv4_ADDR_NONE);
                 }

                 addr_nbr_octet++;
                 if (addr_nbr_octet >= NET_ASCII_NBR_OCTET_ADDR_IPv4) { /* If nbr octets > max (see Note #1b2A), ...    */
                    *p_err =  NET_ASCII_ERR_INVALID_STR_LEN;            /* ... rtn err.                                 */
                     return (NET_IPv4_ADDR_NONE);
                 }

                *p_dot_nbr         += 1;
                 p_addr_part_val++;
                *p_addr_part_val    = 0x0000000000000000u;
                 addr_nbr_part_dig = 0u;
                 break;


            default:                                                    /* See Note #1b1.                               */
                *p_err =  NET_ASCII_ERR_INVALID_CHAR;
                 return (NET_IPv4_ADDR_NONE);
        }

        p_char_prev = p_char_cur;
        p_char_cur++;
    }


    if (p_char_cur  == (CPU_CHAR *)0) {                                 /* If NULL ptr in ASCII str (see Note #1b1), .. */
       *p_err =  NET_ERR_FAULT_NULL_PTR;                                /* .. rtn err.                                  */
        return (NET_IPv4_ADDR_NONE);
    }

    if (p_char_prev == (CPU_CHAR *)0) {                                 /* If NULL        ASCII str (see Note #1b2), .. */
       *p_err =  NET_ASCII_ERR_INVALID_STR_LEN;                         /* .. rtn err.                                  */
        return (NET_IPv4_ADDR_NONE);
    }

    if (*p_char_prev == (CPU_CHAR)'.') {                                /* If last char a dot (see Note #1b2B1), ..     */
        *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;                       /* .. rtn err.                                  */
         return (NET_IPv4_ADDR_NONE);
    }

    addr_ip = (NET_IPv4_ADDR)addr_parts[0];

    switch(*p_dot_nbr) {
        case 0:                                                         /* IP addr format : a       - 32 bits.          */
            if (addr_parts[0] > NET_ASCII_MASK_32_01_BIT) {             /* (See Note #3d4)                              */
               *p_err = NET_ASCII_ERR_INVALID_PART_LEN;
                return (NET_IPv4_ADDR_NONE);
            }
            *p_err = NET_ASCII_ERR_NONE;
            return (addr_ip);


        case 1:                                                         /* IP addr format : a.b     - 24 bits.          */
            if ((addr_ip       > NET_ASCII_MASK_08_01_BIT)  |           /* (See Note #3d3)                              */
                (addr_parts[1] > NET_ASCII_MASK_24_01_BIT)) {
               *p_err = NET_ASCII_ERR_INVALID_PART_LEN;
                return (NET_IPv4_ADDR_NONE);
            }
            addr_ip |= ((addr_parts[1] & NET_ASCII_MASK_08_01_BIT) << (DEF_OCTET_NBR_BITS * 3)) |
                       ((addr_parts[1] & NET_ASCII_MASK_16_09_BIT) <<  DEF_OCTET_NBR_BITS     ) |
                       ((addr_parts[1] & NET_ASCII_MASK_24_17_BIT) >>  DEF_OCTET_NBR_BITS     );
            break;


        case 2:                                                         /* IP addr format : a.b.c   - 16 bits.          */
            if ((addr_ip       > NET_ASCII_MASK_08_01_BIT)  |           /* (See Note #3d2)                              */
                (addr_parts[1] > NET_ASCII_MASK_08_01_BIT)  |
                (addr_parts[2] > NET_ASCII_MASK_16_01_BIT)) {
               *p_err = NET_ASCII_ERR_INVALID_PART_LEN;
                return (NET_IPv4_ADDR_NONE);
            }
            addr_ip |= ((addr_parts[1] & NET_ASCII_MASK_08_01_BIT) <<  DEF_OCTET_NBR_BITS     ) |
                       ((addr_parts[2] & NET_ASCII_MASK_08_01_BIT) << (DEF_OCTET_NBR_BITS * 3)) |
                       ((addr_parts[2] & 0x0000FF00) <<  DEF_OCTET_NBR_BITS     );
            break;


        case 3:                                                         /* IP addr format : a.b.c.d -  8 bits.          */
            if ((addr_ip       > NET_ASCII_MASK_08_01_BIT)  |           /* (See Note #3d1)                              */
                (addr_parts[1] > NET_ASCII_MASK_08_01_BIT)  |
                (addr_parts[2] > NET_ASCII_MASK_08_01_BIT)  |
                (addr_parts[3] > NET_ASCII_MASK_08_01_BIT)) {
               *p_err = NET_ASCII_ERR_INVALID_PART_LEN;
                return (NET_IPv4_ADDR_NONE);
            }
            addr_ip |= ((addr_parts[1] & NET_ASCII_MASK_08_01_BIT) <<  DEF_OCTET_NBR_BITS     ) |
                       ((addr_parts[2] & NET_ASCII_MASK_08_01_BIT) << (DEF_OCTET_NBR_BITS * 2)) |
                       ((addr_parts[3] & NET_ASCII_MASK_08_01_BIT) << (DEF_OCTET_NBR_BITS * 3));
            break;


        default:
           *p_err = NET_ASCII_ERR_INVALID_STR_LEN;
            return (NET_IPv4_ADDR_NONE);
    }

    addr_ip =  NET_UTIL_VAL_SWAP_ORDER_32(addr_ip);

   *p_err   =  NET_ASCII_ERR_NONE;
    return (addr_ip);
}
#endif


/*
*********************************************************************************************************
*                                        NetASCII_Str_to_IPv6()
*
* Description : Convert an IPv6 address ASCII string in common-decimal notation to a network protocol
*                   IPv6 address in host-order.
*
* Argument(s) : p_addr_ip_ascii     Pointer to an ASCII string that contains a common-decimal IPv6 address
*                                       (see Note #1).
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ASCII_ERR_NONE                  IPv6 address successfully converted.
*                               NET_ERR_FAULT_NULL_PTR              Argument 'p_addr_ip_ascii' passed a
*                                                                       NULL pointer.
*                               NET_ASCII_ERR_INVALID_STR_LEN       Invalid ASCII string    length.
*                               NET_ASCII_ERR_INVALID_CHAR          Invalid ASCII character.
*                               NET_ASCII_ERR_INVALID_CHAR_LEN      Invalid ASCII character length.
*                               NET_ASCII_ERR_INVALID_CHAR_VAL      Invalid ASCII character value.
*                               NET_ASCII_ERR_INVALID_CHAR_SEQ      Invalid ASCII character sequence.
*
* Return(s)   : Host-order IPv6 address represented by ASCII string, if NO error(s).
*
*               NET_IPv6_ADDR_NONE,                                  otherwise.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) (a)
*********************************************************************************************************
*/

#ifdef  NET_IPv6_MODULE_EN
NET_IPv6_ADDR  NetASCII_Str_to_IPv6 (CPU_CHAR  *p_addr_ip_ascii,
                                     NET_ERR   *p_err)
{
    CPU_CHAR       *p_char_cur;
    CPU_CHAR       *p_char_next;
    CPU_CHAR       *p_char_prev;
    NET_IPv6_ADDR   addr_ip;
    CPU_INT08U      addr_nbr_octet;
    CPU_INT08U      addr_nbr_colon_cur;
    CPU_INT08U      addr_nbr_colon_rem;
    CPU_INT08U      addr_nbr_colon_tot;
    CPU_BOOLEAN     addr_msb_octet;
    CPU_INT08U      addr_nbr_octet_dig;
    CPU_INT08U      addr_octet_val_dig;
    CPU_INT08U      addr_nbr_lead_zero;
    CPU_INT08U      addr_nbr_octet_zero;
    CPU_INT08U      addr_nbr_dig_end;
    CPU_INT08U      nbr_octet_zero_cnt;


    NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);

                                                                        /* --------------- VALIDATE PTR --------------- */
    if (p_addr_ip_ascii == (CPU_CHAR *)0) {
       *p_err =  NET_ERR_FAULT_NULL_PTR;
        return (addr_ip);
    }


                                                                        /* ----------- CONVERT IP ADDR STR ------------ */
    p_char_cur          = (CPU_CHAR *)p_addr_ip_ascii;
    p_char_prev         = (CPU_CHAR *)0;
    p_char_next         = (CPU_CHAR *)0;
    addr_nbr_octet      =  0u;
    addr_nbr_colon_cur  =  0u;
    addr_nbr_colon_rem  =  0u;
    addr_nbr_colon_tot  =  0u;
    addr_nbr_octet_dig  =  0u;
    addr_octet_val_dig  =  0u;
    addr_nbr_lead_zero  =  0u;
    addr_nbr_octet_zero =  0u;
    addr_nbr_dig_end    =  0u;
    addr_msb_octet      =  DEF_YES;
    nbr_octet_zero_cnt  =  0u;



    while ((p_char_cur != (CPU_CHAR *)  0 ) &&                          /* Parse ALL non-NULL chars in ASCII str.       */
          (*p_char_cur != (CPU_CHAR  )'\0')) {

        p_char_next = p_char_cur + 1u;
        switch (*p_char_cur) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                 addr_nbr_octet_dig++;                                  /* If nbr digs > max (see Note #1b3), ...       */
                 if (addr_nbr_octet_dig > NET_ASCII_CHAR_MAX_DIG_ADDR_IPv6) {
                    *p_err = NET_ASCII_ERR_INVALID_CHAR_LEN;            /* ... rtn err.                                 */
                     NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                     return (addr_ip);
                 }

                 switch (*p_char_cur) {                                 /* Convert ASCII char into hex val.             */
                     case '0':
                     case '1':
                     case '2':
                     case '3':
                     case '4':
                     case '5':
                     case '6':
                     case '7':
                     case '8':
                     case '9':
                          addr_octet_val_dig = (CPU_INT08U) (*p_char_cur - '0');
                          break;


                     case 'A':
                     case 'B':
                     case 'C':
                     case 'D':
                     case 'E':
                     case 'F':
                          addr_octet_val_dig = (CPU_INT08U)((*p_char_cur - 'A') + 10u);
                          break;


                     case 'a':
                     case 'b':
                     case 'c':
                     case 'd':
                     case 'e':
                     case 'f':
                          addr_octet_val_dig = (CPU_INT08U)((*p_char_cur - 'a') + 10u);
                          break;


                     default:                                           /* See Note #x.                               */
                          NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                         *p_err = NET_ASCII_ERR_INVALID_CHAR;
                          return (addr_ip);
                 }

                 if (addr_msb_octet == DEF_YES) {
                     addr_ip.Addr[addr_nbr_octet] =  addr_octet_val_dig;
                     addr_ip.Addr[addr_nbr_octet] <<= DEF_NIBBLE_NBR_BITS;
                     addr_msb_octet = DEF_NO;
                 } else {
                     addr_ip.Addr[addr_nbr_octet] |= addr_octet_val_dig;
                     addr_msb_octet = DEF_YES;
                     addr_nbr_octet++;
                 }

                 if (addr_nbr_octet > NET_ASCII_NBR_OCTET_ADDR_IPv6) {
                    *p_err =  NET_ASCII_ERR_INVALID_CHAR_LEN;           /* ... rtn err.                                 */
                     NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                     return (addr_ip);
                 }
                 break;


            case ':':
                 addr_nbr_colon_cur++;
                 if (p_char_prev == (CPU_CHAR *)0) {                    /* If NO prev char     (see Note #1b2B1), ...   */
                     if (p_char_next == (CPU_CHAR *)0) {
                        *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;       /* ... rtn err.                                 */
                         NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                         return (addr_ip);
                     }

                     if (*p_char_next != (CPU_CHAR)':') {
                         *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;      /* ... rtn err.                                 */
                          NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                          return (addr_ip);
                     } else {
                          break;
                     }
                 }

                 if (addr_nbr_colon_cur > NET_ASCII_CHAR_MAX_COLON_IPv6) {
                    *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;           /* .. rtn err.                                  */
                     NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                     return (addr_ip);
                 }

                 if (*p_char_prev != (CPU_CHAR)':') {
                     switch (addr_nbr_octet_dig) {
                         case 0:
                             *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;  /* .. rtn err.                                  */
                              NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                              return (addr_ip);
                              break;

                         case 1:
                              addr_ip.Addr[addr_nbr_octet +1] = addr_ip.Addr[addr_nbr_octet] >> DEF_NIBBLE_NBR_BITS;
                              addr_ip.Addr[addr_nbr_octet]    = 0x00;
                              addr_nbr_lead_zero = addr_nbr_lead_zero + 3u;
                              addr_nbr_octet     = addr_nbr_octet     + 2u;
                              addr_msb_octet     = DEF_YES;
                              break;

                         case 2:
                              addr_ip.Addr[addr_nbr_octet]    = addr_ip.Addr[addr_nbr_octet -1];
                              addr_ip.Addr[addr_nbr_octet -1] = 0x00;
                              addr_nbr_lead_zero = addr_nbr_lead_zero + 2u;
                              addr_nbr_octet     = addr_nbr_octet     + 1u;
                              break;

                         case 3:
                              addr_ip.Addr[addr_nbr_octet]    >>= DEF_NIBBLE_NBR_BITS;
                              addr_ip.Addr[addr_nbr_octet]     |= (addr_ip.Addr[addr_nbr_octet -1] << DEF_NIBBLE_NBR_BITS);
                              addr_ip.Addr[addr_nbr_octet -1] >>= DEF_NIBBLE_NBR_BITS;
                              addr_nbr_lead_zero = addr_nbr_lead_zero + 1u;
                              addr_nbr_octet     = addr_nbr_octet     + 1u;
                              addr_msb_octet     = DEF_YES;
                              break;

                         case 4:
                         default:
                              break;
                     }

                 } else {
                     if (p_char_next == (CPU_CHAR *)0) {
                        *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;       /* ... rtn err.                                 */
                         NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                         return (addr_ip);
                     }

                     if (*p_char_next == ':') {
                        *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;      /* .. rtn err.                                  */
                         NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                         return (addr_ip);
                     }

                     if (*p_char_next == '\0') {
                         addr_nbr_dig_end = 0;
                     } else {
                         addr_nbr_dig_end = 1;
                     }

                     while  ((p_char_next != (CPU_CHAR *)  0 ) &&        /* Parse ALL non-NULL chars in ASCII str.       */
                            (*p_char_next != (CPU_CHAR  )'\0')) {
                         if (*p_char_next == ':') {
                             addr_nbr_colon_rem++;
                         }
                         p_char_next++;
                     }

                     addr_nbr_colon_tot  = addr_nbr_colon_cur + addr_nbr_colon_rem;
                     addr_nbr_octet_zero = ((NET_ASCII_LEN_MAX_ADDR_IPv6 - addr_nbr_colon_tot
                                                                         - (2 *  addr_nbr_octet)
                                                                         - (4 * (addr_nbr_colon_rem       + addr_nbr_dig_end))
                                                                         - (NET_ASCII_CHAR_MAX_COLON_IPv6 - addr_nbr_colon_tot)
                                                                         -  NET_ASCII_CHAR_LEN_NUL) / 2);
                     for (nbr_octet_zero_cnt = 0u; nbr_octet_zero_cnt < addr_nbr_octet_zero; nbr_octet_zero_cnt++ ) {
                         addr_ip.Addr[addr_nbr_octet] = DEF_BIT_NONE;
                         addr_nbr_octet++;
                     }
                 }

                 if (addr_nbr_octet > NET_ASCII_NBR_OCTET_ADDR_IPv6) {  /* If nbr octets > max (see Note #1b2A), ...    */
                    *p_err =  NET_ASCII_ERR_INVALID_STR_LEN;            /* ... rtn err.                                 */
                     NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                     return (addr_ip);
                 }

                 addr_nbr_octet_dig = 0u;
                 break;


            default:                                                    /* See Note #1b1.                               */
                *p_err =  NET_ASCII_ERR_INVALID_CHAR;
                 NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
                 return (addr_ip);
        }

        p_char_prev = p_char_cur;
        p_char_cur++;
    }


    if (p_char_cur == (CPU_CHAR *)0) {                                  /* If NULL ptr in ASCII str (see Note #1b1), .. */
       *p_err =  NET_ERR_FAULT_NULL_PTR;                                /* .. rtn err.                                  */
        NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
        return (addr_ip);
    }

    if (p_char_prev == (CPU_CHAR *)0) {                                 /* If NULL ptr in ASCII str (see Note #1b1), .. */
       *p_err =  NET_ERR_FAULT_NULL_PTR;                                /* .. rtn err.                                  */
        NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
        return (addr_ip);
    }

    if ((*p_char_prev == (CPU_CHAR)':') && (addr_nbr_dig_end == 1)) {   /* If last char a colon (see Note #1b2B1), ...  */
        *p_err =  NET_ASCII_ERR_INVALID_CHAR_SEQ;                       /* .. rtn err.                                  */
         NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
         return (addr_ip);
    }

    switch (addr_nbr_octet_dig) {
        case 1:
             addr_ip.Addr[addr_nbr_octet +1] = addr_ip.Addr[addr_nbr_octet] >> DEF_NIBBLE_NBR_BITS;
             addr_ip.Addr[addr_nbr_octet]    = DEF_BIT_NONE;
             addr_nbr_lead_zero = addr_nbr_lead_zero + 3u;
             addr_nbr_octet     = addr_nbr_octet     + 2u;
             addr_msb_octet     = DEF_YES;
             break;

        case 2:
             addr_ip.Addr[addr_nbr_octet] = addr_ip.Addr[addr_nbr_octet -1];
             addr_ip.Addr[addr_nbr_octet -1] = DEF_BIT_NONE;
             addr_nbr_lead_zero = addr_nbr_lead_zero + 2u;
             addr_nbr_octet     = addr_nbr_octet     + 1u;
             break;

        case 3:
             addr_ip.Addr[addr_nbr_octet]    >>= DEF_NIBBLE_NBR_BITS;
             addr_ip.Addr[addr_nbr_octet]     |= (addr_ip.Addr[addr_nbr_octet -1] << DEF_NIBBLE_NBR_BITS);
             addr_ip.Addr[addr_nbr_octet -1] >>= DEF_NIBBLE_NBR_BITS;
             addr_nbr_lead_zero = addr_nbr_lead_zero + 1u;
             addr_nbr_octet     = addr_nbr_octet     + 1u;
             addr_msb_octet     = DEF_YES;
             break;

         case 0:
         case 4:
         default:
             break;
     }

    if (addr_nbr_octet != NET_ASCII_NBR_OCTET_ADDR_IPv6) {              /* If != nbr IPv6 addr octets (see Note #1b2A), */
       *p_err =  NET_ASCII_ERR_INVALID_STR_LEN;                         /* .. rtn err.                                  */
        NET_UTIL_IPv6_ADDR_SET_UNSPECIFIED(addr_ip);
        return (addr_ip);
    }

   *p_err      =  NET_ASCII_ERR_NONE;

    return (addr_ip);
}
#endif


/*
*********************************************************************************************************
*                                       NetASCII_IPv4_to_Str()
*
* Description : Convert a network protocol IPv4 address in host-order into an IPv4 address ASCII string
*                   in dotted-decimal notation.
*
* Argument(s) : addr_ip             IPv4 address.
*
*               p_addr_ip_ascii     Pointer to an ASCII character array that will receive the return IPv4
*                                       address ASCII string from this function (see Note #1b).
*
*               lead_zeros          Prepend leading zeros option (see Note #2) :
*
*                                       DEF_NO      Do NOT prepend leading zeros to each decimal octet value.
*                                       DEF_YES            Prepend leading zeros to each decimal octet value.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ASCII_ERR_NONE                  ASCII string successfully formatted.
*                               NET_ERR_FAULT_NULL_PTR              Argument 'paddr_ip_ascii' passed a
*                                                                       NULL pointer.
*                               NET_ASCII_ERR_INVALID_CHAR_LEN      Invalid ASCII character length.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application programming interface (API) function &
*               MAY be called by application function(s).
*
* Note(s)     : (1) (a) (1) RFC #1983 states that "dotted decimal notation ... refers [to] IPv4 addresses of the
*                           form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                           address".
*
*                       (2) In other words, the dotted-decimal IP address notation separates four decimal octet
*                           values by the dot, or period, character ('.').  Each decimal value represents one
*                           octet of the IP address starting with the most significant octet in network-order.
*
*                               IP Address Examples :
*
*                                      DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                           127.0.0.1           =       0x7F000001
*                                           192.168.1.64        =       0xC0A80140
*                                           255.255.255.0       =       0xFFFFFF00
*                                           ---         -                 --    --
*                                            ^          ^                 ^      ^
*                                            |          |                 |      |
*                                           MSO        LSO               MSO    LSO
*
*                                   where
*                                           MSO        Most  Significant Octet in Dotted Decimal IPv4 Address
*                                           LSO        Least Significant Octet in Dotted Decimal IPv4 Address
*
*
*                   (b) (1) The return dotted-decimal IPv4 address ASCII string :
*
*                           (A) Formats EXACTLY four decimal values   ...
*                           (B) (1) ... separated by                  ...
*                               (2) ... EXACTLY three dot characters; ...
*                           (C) ... & terminated with the NULL character.
*
*                       (2) The size of the ASCII character array that will receive the returned IP address
*                           ASCII string SHOULD be greater than or equal to NET_ASCII_LEN_MAX_ADDR_IP.
*
*               (2) (a) Leading zeros option prepends leading '0's prior to the first non-zero digit in each
*                       decimal octet value.  The number of leading zeros is such that the decimal octet's
*                       number of decimal digits is equal to the maximum number of digits (3).
*
*                   (b) (1) If leading zeros option DISABLED              ...
*                       (2) ... & the decimal value of the octet is zero; ...
*                       (3) ... then one digit of '0' value is formatted.
*
*                       This is NOT a leading zero; but a single decimal digit of '0' value.
*********************************************************************************************************
*/

#ifdef  NET_IPv4_MODULE_EN
void  NetASCII_IPv4_to_Str (NET_IPv4_ADDR   addr_ip,
                            CPU_CHAR       *p_addr_ip_ascii,
                            CPU_BOOLEAN     lead_zeros,
                            NET_ERR        *p_err)
{
    CPU_CHAR    *p_char;
    CPU_INT08U   base_10_val_start;
    CPU_INT08U   base_10_val;
    CPU_INT08U   addr_octet_nbr_shifts;
    CPU_INT08U   addr_octet_val;
    CPU_INT08U   addr_octet_dig_nbr;
    CPU_INT08U   addr_octet_dig_val;
    CPU_INT08U   i;


                                                                        /* -------------- VALIDATE PTR ---------------- */
    if (p_addr_ip_ascii == (CPU_CHAR *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                        /* ------------ VALIDATE NBR CHAR ------------- */
#if ((NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv4 < NET_ASCII_CHAR_MIN_OCTET   ) || \
     (NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv4 > NET_ASCII_CHAR_MAX_OCTET_08))
   *p_addr_ip_ascii = (CPU_CHAR)'\0';
   *p_err           =  NET_ASCII_ERR_INVALID_CHAR_LEN;
    return;
#endif


                                                                        /* ------------- CONVERT IP ADDR -------------- */
    p_char = p_addr_ip_ascii;


    base_10_val_start = 1u;
    for (i = 1u; i < NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv4; i++) {         /* Calc starting dig val.                       */
        base_10_val_start *= 10u;
    }


    for (i = NET_ASCII_NBR_OCTET_ADDR_IPv4; i > 0u; i--) {              /* Parse ALL addr octets (see Note #1b1A).      */
                                                                        /* Calc  cur addr octet val.                    */
        addr_octet_nbr_shifts = (i - 1u) * DEF_OCTET_NBR_BITS;
        addr_octet_val        = (CPU_INT08U)((addr_ip >> addr_octet_nbr_shifts) & DEF_OCTET_MASK);

        base_10_val           =  base_10_val_start;
        while (base_10_val > 0u) {                                      /* Parse ALL octet digs.                        */
            addr_octet_dig_nbr = addr_octet_val / base_10_val;

            if ((addr_octet_dig_nbr >  0u) ||                           /* If octet dig val > 0,                     .. */
                (base_10_val        == 1u) ||                           /* .. OR on least-sig octet dig,             .. */
                (lead_zeros == DEF_YES)) {                              /* .. OR lead zeros opt ENABLED (see Note #2),  */
                                                                        /* .. calc & insert octet val ASCII dig.        */
                 addr_octet_dig_val = (CPU_INT08U)(addr_octet_dig_nbr % 10u);
                *p_char++           = (CPU_CHAR  )(addr_octet_dig_val + '0');
            }

            base_10_val /= 10u;                                         /* Shift to next least-sig octet dig.           */
        }

        if (i > 1u) {                                                   /* If NOT on last octet, ...                    */
           *p_char++ = '.';                                             /* ... insert a dot char (see Note #1b1B2).     */
        }
    }

   *p_char = (CPU_CHAR)'\0';                                            /* Append NULL char (see Note #1b1C).           */

   *p_err  =  NET_ASCII_ERR_NONE;
}
#endif


/*
*********************************************************************************************************
*                                       NetASCII_IPv6_to_Str()
*
* Description : Convert a network protocol IPv4 address in host-order into an IPv4 address ASCII string
*                   in dotted-decimal notation.
*
* Argument(s) : paddr_ip            Pointer to IPv6 address.
*
*               paddr_ip_ascii      Pointer to an ASCII character array that will receive the return IPv6
*                                       address ASCII string from this function (see Note #1b).
*
*               lead_zeros          Prepend leading zeros option (see Note #2) :
*
*                                       DEF_NO      Do NOT prepend leading zeros to each decimal octet value.
*                                       DEF_YES            Prepend leading zeros to each decimal octet value.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               NET_ASCII_ERR_NONE                  ASCII string successfully formatted.
*                               NET_ERR_FAULT_NULL_PTR              Argument 'paddr_ip_ascii' passed a
*                                                                       NULL pointer.
*                               NET_ASCII_ERR_INVALID_CHAR_LEN      Invalid ASCII character length.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a network protocol suite application interface (API) function & MAY be
*               called by application function(s).
*
* Note(s)     : (1) (a) (1) RFC #1983 states that "dotted decimal notation ... refers [to] IP addresses of the
*                           form A.B.C.D; where each letter represents, in decimal, one byte of a four byte IP
*                           address".
*
*                       (2) In other words, the dotted-decimal IP address notation separates four decimal octet
*                           values by the dot, or period, character ('.').  Each decimal value represents one
*                           octet of the IP address starting with the most significant octet in network-order.
*
*                               IP Address Examples :
*
*                                      DOTTED DECIMAL NOTATION     HEXADECIMAL EQUIVALENT
*
*                                           127.0.0.1           =       0x7F000001
*                                           192.168.1.64        =       0xC0A80140
*                                           255.255.255.0       =       0xFFFFFF00
*                                           ---         -                 --    --
*                                            ^          ^                 ^      ^
*                                            |          |                 |      |
*                                           MSO        LSO               MSO    LSO
*
*                                   where
*                                           MSO        Most  Significant Octet in Dotted Decimal IP Address
*                                           LSO        Least Significant Octet in Dotted Decimal IP Address
*
*
*                   (b) (1) The return dotted-decimal IP address ASCII string :
*
*                           (A) Formats EXACTLY four decimal values   ...
*                           (B) (1) ... separated by                  ...
*                               (2) ... EXACTLY three dot characters; ...
*                           (C) ... & terminated with the NULL character.
*
*                       (2) The size of the ASCII character array that will receive the returned IP address
*                           ASCII string SHOULD be greater than or equal to NET_ASCII_LEN_MAX_ADDR_IP.
*
*               (2) (a) Leading zeros option prepends leading '0's prior to the first non-zero digit in each
*                       decimal octet value.  The number of leading zeros is such that the decimal octet's
*                       number of decimal digits is equal to the maximum number of digits (3).
*
*                   (b) (1) If leading zeros option DISABLED              ...
*                       (2) ... & the decimal value of the octet is zero; ...
*                       (3) ... then one digit of '0' value is formatted.
*
*                       This is NOT a leading zero; but a single decimal digit of '0' value.
*********************************************************************************************************
*/

#ifdef  NET_IPv6_MODULE_EN
void  NetASCII_IPv6_to_Str (NET_IPv6_ADDR  *p_addr_ip,
                            CPU_CHAR       *p_addr_ip_ascii,
                            CPU_BOOLEAN     hex_lower_case,
                            CPU_BOOLEAN     lead_zeros,
                            NET_ERR        *p_err)
{
    CPU_CHAR     *p_char;
    CPU_INT08U   *p_addr;
    CPU_INT08U    addr_octet_val;
    CPU_INT08U    addr_octet_dig_val;
    CPU_INT08U    addr_octet_nbr_shifts;
    CPU_INT08U    addr_nbr_octet_dig;
    CPU_INT08U    i;
    CPU_INT08U    j;
    CPU_BOOLEAN   addr_zero_lead;


                                                                        /* -------------- VALIDATE PTRS --------------- */
    if (p_addr_ip == (NET_IPv6_ADDR *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }

    if (p_addr_ip_ascii == (CPU_CHAR *)0) {
       *p_err = NET_ERR_FAULT_NULL_PTR;
        return;
    }
                                                                        /* ------------- CONVERT IP ADDR -------------- */
    p_addr             = &p_addr_ip->Addr[0];
    p_char             =  p_addr_ip_ascii;
    addr_nbr_octet_dig =  0u;
    addr_zero_lead     =  DEF_YES;

    for (i = NET_ASCII_NBR_OCTET_ADDR_IPv6; i > 0; i--) {               /* Parse ALL addr octets (see Note #1b1A).      */

        addr_octet_val = *p_addr;

        for (j = NET_ASCII_CHAR_MAX_OCTET_ADDR_IPv6; j > 0; j--) {      /* Parse ALL octet's hex digs.                  */
                                                                        /* Calc  cur octet's hex dig val.               */
            addr_octet_nbr_shifts = (j - 1) * DEF_NIBBLE_NBR_BITS;
            addr_octet_dig_val    = (addr_octet_val >> addr_octet_nbr_shifts) & DEF_NIBBLE_MASK;
                                                                        /* Insert    octet's hex val ASCII dig.         */
            addr_nbr_octet_dig++;
            if (addr_octet_dig_val < 10) {
                if ((lead_zeros         == DEF_YES) ||
                    (addr_zero_lead     == DEF_NO)  ||
                    (addr_nbr_octet_dig == NET_ASCII_CHAR_MAX_DIG_ADDR_IPv6) ||
                    (addr_octet_dig_val != DEF_BIT_NONE)) {
                   *p_char++       = (CPU_CHAR)(addr_octet_dig_val + '0');
                    addr_zero_lead = DEF_NO;
                }
            } else {
                if (hex_lower_case != DEF_YES) {
                   *p_char++ = (CPU_CHAR)((addr_octet_dig_val - 10u) + 'A');
                } else {
                   *p_char++ = (CPU_CHAR)((addr_octet_dig_val - 10u) + 'a');
                }
                addr_zero_lead = DEF_NO;
            }
        }

        if ((i % 2 != 0) &&
            (i > 1)) {                                                  /* If NOT on last octet,                    ..  */
            *p_char++          = ':';                                   /* .. insert colon char (see Note #1b1B2b).     */
             addr_zero_lead    = DEF_YES;
             addr_nbr_octet_dig = 0u;
        }

        p_addr++;
    }

   *p_char = (CPU_CHAR)'\0';                                            /* Append NULL char (see Note #1b1C).           */

   *p_err  =  NET_ASCII_ERR_NONE;
}
#endif


