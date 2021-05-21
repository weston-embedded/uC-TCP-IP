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
*                                           TCP ECHO SERVER
*
* Filename : tcp_server.c
* Version  : V3.06.01
*********************************************************************************************************
* Note(s)  : (1) This example shows how to create a TCP server that accept IPv4 or IPv6 connections.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <Source/net_cfg_net.h>
#include  <Source/net_sock.h>
#include  <Source/net_app.h>
#include  <Source/net_util.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  TCP_SERVER_PORT  10001
#define  RX_BUF_SIZE         15


/*
*********************************************************************************************************
*                                         App_TCP_ServerIPv4()
*
* Description : TCP Echo server:
*
*                   (a) Open a socket.
*                   (b) Configure socket's address.
*                   (c) Bind that socket.
*                   (d) Receive data on the socket.
*                   (e) Transmit to source the data received.
*                   (f) Close socket on fatal fault error.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if  defined(NET_IPv4_MODULE_EN) && \
     defined(NET_SOCK_TYPE_STREAM_MODULE_EN)
void  App_TCP_ServerIPv4 (void)
{
    NET_SOCK_ID          sock_listen;
    NET_SOCK_ID          sock_child;
    NET_SOCK_ADDR_IPv4   server_sock_addr_ip;
    NET_SOCK_ADDR_IPv4   client_sock_addr_ip;
    NET_SOCK_ADDR_LEN    client_sock_addr_ip_size;
    NET_SOCK_RTN_CODE    rx_size;
    NET_SOCK_RTN_CODE    tx_size;
    NET_SOCK_DATA_SIZE   tx_rem;
    CPU_CHAR             rx_buf[RX_BUF_SIZE];
    CPU_INT32U           addr_any = NET_IPv4_ADDR_ANY;
    CPU_INT08U          *p_buf;
    CPU_BOOLEAN          fault_err;
    NET_ERR              err;


                                                                /* ----------------- OPEN IPV4 SOCKET ----------------- */
    sock_listen = NetSock_Open(NET_SOCK_PROTOCOL_FAMILY_IP_V4,
                               NET_SOCK_TYPE_STREAM,
                               NET_SOCK_PROTOCOL_TCP,
                              &err);
    if (err != NET_SOCK_ERR_NONE) {
        return;
    }


                                                                /* ------------ CONFIGURE SOCKET'S ADDRESS ------------ */
    NetApp_SetSockAddr((NET_SOCK_ADDR *)&server_sock_addr_ip,
                                         NET_SOCK_ADDR_FAMILY_IP_V4,
                                         TCP_SERVER_PORT,
                       (CPU_INT08U    *)&addr_any,
                                         NET_IPv4_ADDR_SIZE,
                                        &err);
    switch (err) {
        case NET_APP_ERR_NONE:
            break;


        case NET_APP_ERR_FAULT:
        case NET_APP_ERR_NONE_AVAIL:
        case NET_APP_ERR_INVALID_ARG:
        default:
            NetSock_Close(sock_listen, &err);
            return;
    }


                                                                /* ------------------- BIND SOCKET -------------------- */
    NetSock_Bind(                  sock_listen,
                 (NET_SOCK_ADDR *)&server_sock_addr_ip,
                                   NET_SOCK_ADDR_SIZE,
                                  &err);
    if (err != NET_SOCK_ERR_NONE) {
        NetSock_Close(sock_listen, &err);
        return;
    }


                                                                /* ------------------ LISTEN SOCKET ------------------- */
    NetSock_Listen(sock_listen, 1, &err);
    if (err != NET_SOCK_ERR_NONE) {
        NetSock_Close(sock_listen, &err);
        return;
    }

    fault_err = DEF_NO;

    while (fault_err == DEF_NO) {
        client_sock_addr_ip_size = NET_SOCK_ADDR_IPv4_SIZE;

                                                                /* ---------- ACCEPT NEW INCOMING CONNECTION ---------- */
        sock_child = NetSock_Accept(                  sock_listen,
                                    (NET_SOCK_ADDR *)&client_sock_addr_ip,
                                                     &client_sock_addr_ip_size,
                                                     &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 do {
                                                                /* ----- WAIT UNTIL RECEIVING DATA FROM A CLIENT ------ */
                     client_sock_addr_ip_size = sizeof(client_sock_addr_ip);
                     rx_size = NetSock_RxDataFrom(                  sock_child,
                                                                    rx_buf,
                                                                    RX_BUF_SIZE,
                                                                    NET_SOCK_FLAG_NONE,
                                                  (NET_SOCK_ADDR *)&client_sock_addr_ip,
                                                                   &client_sock_addr_ip_size,
                                                                    DEF_NULL,
                                                                    DEF_NULL,
                                                                    DEF_NULL,
                                                                   &err);
                     switch (err) {
                         case NET_SOCK_ERR_NONE:
                              tx_rem =  rx_size;
                              p_buf  = (CPU_INT08U *)rx_buf;
                                                                /* ----- TRANSMIT THE DATA RECEIVED TO THE CLIENT ----- */
                              do {
                                  tx_size = NetSock_TxDataTo(                  sock_child,
                                                             (void           *)p_buf,
                                                                               tx_rem,
                                                                               NET_SOCK_FLAG_NONE,
                                                             (NET_SOCK_ADDR *)&client_sock_addr_ip,
                                                                               client_sock_addr_ip_size,
                                                                              &err);
                                  tx_rem -= tx_size;
                                  p_buf   = (CPU_INT08U *)(p_buf + tx_size);

                              } while (tx_rem > 0);
                              break;

                         case NET_SOCK_ERR_RX_Q_EMPTY:
                         case NET_ERR_FAULT_LOCK_ACQUIRE:
                              break;

                         default:
                             fault_err = DEF_YES;
                             break;
                     }

                 } while (fault_err == DEF_NO);


                                                                /* ---------------- CLOSE CHILD SOCKET ---------------- */
                 NetSock_Close(sock_child, &err);
                 if (err != NET_SOCK_ERR_NONE) {
                     fault_err = DEF_YES;
                 }
                 break;

            case NET_SOCK_ERR_NONE_AVAIL:
            case NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT:
                 break;

            default:
                 fault_err = DEF_YES;
                 break;
        }
    }

                                                                /* ------------- FATAL FAULT SOCKET ERROR ------------- */
    NetSock_Close(sock_listen, &err);                           /* This function should be reached only when a fatal ...*/
                                                                /* fault error has occurred.                            */
}
#endif



/*
*********************************************************************************************************
*                                         App_TCP_ServerIPv6()
*
* Description : TCP Echo server:
*
*                   (a) Open a socket.
*                   (b) Configure socket's address.
*                   (c) Bind that socket.
*                   (d) Receive data on the socket.
*                   (e) Transmit to source the data received.
*                   (f) Close socket on fatal fault error.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : none.
*
* Note(s)     : none.
*********************************************************************************************************
*/
#if  defined(NET_IPv6_MODULE_EN) && \
     defined(NET_SOCK_TYPE_STREAM_MODULE_EN)
void  App_TCP_ServerIPv6 (void)
{
    NET_SOCK_ID          sock_listen;
    NET_SOCK_ID          sock_child;
    NET_SOCK_ADDR_IPv6   server_sock_addr_ip;
    NET_SOCK_ADDR_IPv6   client_sock_addr_ip;
    NET_SOCK_ADDR_LEN    client_sock_addr_ip_size;
    NET_SOCK_RTN_CODE    rx_size;
    NET_SOCK_RTN_CODE    tx_size;
    NET_SOCK_DATA_SIZE   tx_rem;
    CPU_CHAR             rx_buf[RX_BUF_SIZE];
    CPU_INT08U          *p_buf;
    CPU_BOOLEAN          fault_err;
    NET_ERR              err;



                                                                /* ----------------- OPEN IPV6 SOCKET ----------------- */
    sock_listen = NetSock_Open(NET_SOCK_PROTOCOL_FAMILY_IP_V6,
                               NET_SOCK_TYPE_STREAM,
                               NET_SOCK_PROTOCOL_TCP,
                              &err);
    if (err != NET_SOCK_ERR_NONE) {
        return;
    }



                                                                /* ------------ CONFIGURE SOCKET'S ADDRESS ------------ */
    NetApp_SetSockAddr((NET_SOCK_ADDR *)&server_sock_addr_ip,
                                         NET_SOCK_ADDR_FAMILY_IP_V6,
                                         TCP_SERVER_PORT,
                       (CPU_INT08U    *)&NET_IPv6_ADDR_ANY,
                                         NET_IPv6_ADDR_SIZE,
                                        &err);
    switch (err) {
        case NET_APP_ERR_NONE:
            break;


        case NET_APP_ERR_FAULT:
        case NET_APP_ERR_NONE_AVAIL:
        case NET_APP_ERR_INVALID_ARG:
        default:
            NetSock_Close(sock_listen, &err);
            return;
    }


                                                                /* ------------------- BIND SOCKET -------------------- */
    NetSock_Bind(                  sock_listen,
                 (NET_SOCK_ADDR *)&server_sock_addr_ip,
                                   NET_SOCK_ADDR_SIZE,
                                  &err);
    if (err != NET_SOCK_ERR_NONE) {
        NetSock_Close(sock_listen, &err);
        return;
    }


                                                                /* ------------------ LISTEN SOCKET ------------------- */
    NetSock_Listen(sock_listen, 1, &err);
    if (err != NET_SOCK_ERR_NONE) {
        NetSock_Close(sock_listen, &err);
        return;
    }

    fault_err = DEF_NO;

    while (fault_err == DEF_NO) {
                                                                /* --------- ACCEPT NEW INCOMMING CONNECTION ---------- */
        sock_child = NetSock_Accept(                  sock_listen,
                                    (NET_SOCK_ADDR *)&client_sock_addr_ip,
                                                     &client_sock_addr_ip_size,
                                                     &err);
        switch (err) {
            case NET_SOCK_ERR_NONE:
                 do {
                                                                /* ---- WAIT UNTIL RECEIVING DATA FROM THE CLIENT ----- */
                     client_sock_addr_ip_size = sizeof(client_sock_addr_ip);
                     rx_size = NetSock_RxDataFrom(                  sock_child,
                                                                    rx_buf,
                                                                    RX_BUF_SIZE,
                                                                    NET_SOCK_FLAG_NONE,
                                                  (NET_SOCK_ADDR *)&client_sock_addr_ip,
                                                                   &client_sock_addr_ip_size,
                                                                    DEF_NULL,
                                                                    DEF_NULL,
                                                                    DEF_NULL,
                                                                   &err);
                     switch (err) {
                         case NET_SOCK_ERR_NONE:
                              tx_rem =  rx_size;
                              p_buf  = (CPU_INT08U *)rx_buf;
                                                                /* ----- TRANSMIT THE DATA RECEIVED TO THE CLIENT ----- */
                              do {
                                  tx_size = NetSock_TxDataTo(                  sock_child,
                                                             (void           *)p_buf,
                                                                               tx_rem,
                                                                               NET_SOCK_FLAG_NONE,
                                                             (NET_SOCK_ADDR *)&client_sock_addr_ip,
                                                                               client_sock_addr_ip_size,
                                                                              &err);
                                  tx_rem -=  tx_size;
                                  p_buf   = (CPU_INT08U *)(p_buf + tx_size);

                              } while (tx_rem > 0);
                              break;

                         case NET_SOCK_ERR_RX_Q_EMPTY:
                         case NET_ERR_FAULT_LOCK_ACQUIRE:
                              break;

                         default:
                             fault_err = DEF_YES;
                             break;
                     }

                 } while (fault_err == DEF_NO);


                                                                /* ---------------- CLOSE CHILD SOCKET ---------------- */
                 NetSock_Close(sock_child, &err);
                 if (err != NET_SOCK_ERR_NONE) {
                     fault_err = DEF_YES;
                 }
                 break;

            case NET_SOCK_ERR_NONE_AVAIL:
            case NET_SOCK_ERR_CONN_SIGNAL_TIMEOUT:
                 break;

            default:
                 fault_err = DEF_YES;
                 break;
        }
    }

                                                                /* ------------- FATAL FAULT SOCKET ERROR ------------- */
    NetSock_Close(sock_listen, &err);                           /* This function should be reached only when a fatal ...*/
                                                                /* fault error has occurred.                            */
}
#endif
