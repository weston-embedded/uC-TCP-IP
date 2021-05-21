/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                               EXAMPLE
*
*                                         MULTICAST ECHO SERVER
*
* Filename : mcast_echo_server.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This example shows how to create a UDP Multicast echo server using IPv4 string address.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <lib_def.h>
#include  <cpu.h>

#include  <Source/net_sock.h>
#include  <Source/net_util.h>
#include  <Source/net_ascii.h>

#include  <IP/IPv4/net_ipv4.h>
#include  <IP/IPv4/net_igmp.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                              FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

void  MCastEchoServer (NET_IF_NBR     if_nbr,
                       CPU_CHAR      *p_group_addr,
                       NET_PORT_NBR   port)
{
    NET_IPv4_ADDR       addr;
    NET_SOCK_ID         sock_id;
    NET_SOCK_ADDR_IPv4  addr_local;
    NET_ERR             net_err;



    addr = NetASCII_Str_to_IPv4(p_group_addr, &net_err);        /* Convert Group IP address.                            */

    NetIGMP_HostGrpJoin(if_nbr, addr, &net_err);                /* Join the multicast group.                            */
    if (net_err != NET_IGMP_ERR_NONE) {
        return;
    }



    sock_id = NetSock_Open(NET_SOCK_FAMILY_IP_V4,               /* Open an UDP socket.                                  */
                           NET_SOCK_TYPE_DATAGRAM,
                           NET_SOCK_PROTOCOL_UDP,
                          &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        return;
    }

                                                                        /* Set Socket address structure.                */
    addr_local.AddrFamily = NET_SOCK_ADDR_FAMILY_IP_V4;                 /* IPv4.                                        */
    addr_local.Addr       = NET_UTIL_HOST_TO_NET_32(NET_IPv4_ADDR_ANY); /* Any IPv4 addresses.                          */
    addr_local.Port       = NET_UTIL_HOST_TO_NET_16(port);              /* Multicast Port to listen on.                 */


    NetSock_Bind(                    sock_id,                   /* Bind the socket.                                     */
                 (NET_SOCK_ADDR   *)&addr_local,
                                     sizeof(addr_local),
                                    &net_err);
    if (net_err != NET_SOCK_ERR_NONE) {
        goto exit_close;
    }


    while(1) {
        CPU_CHAR           buf[1472u];
        NET_SOCK_ADDR      sock_addr;
        NET_SOCK_ADDR_LEN  sock_addr_len = sizeof(sock_addr);
        CPU_INT32S         data_len;


        data_len = NetSock_RxDataFrom(sock_id,                  /* Receive Data from the multicast group.               */
                                      buf,
                                      1472u,
                                      NET_SOCK_FLAG_NONE,
                                     &sock_addr,
                                     &sock_addr_len,
                                      DEF_NULL,
                                      DEF_NULL,
                                      DEF_NULL,
                                     &net_err);
        switch (net_err) {
            case NET_SOCK_ERR_NONE:
                 NetSock_TxDataTo(sock_id,                      /* Transmit received data to the sender.                */
                                  buf,
                                  data_len,
                                  NET_SOCK_FLAG_NONE,
                                 &sock_addr,
                                  sock_addr_len,
                                 &net_err);
                 break;

            case NET_SOCK_ERR_RX_Q_EMPTY:
                 break;

            default:
                goto exit_close;
        }
    }


exit_close:
    NetSock_Close(sock_id, &net_err);                           /* Close the socket.                                    */

    return;
}

