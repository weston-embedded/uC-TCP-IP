#*
#********************************************************************************************************
#                                            EXAMPLE CODE
#
#               This file is provided as an example on how to use Micrium products.
#
#               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
#               your application products.  Example code may be used as is, in whole or in
#               part, or may be used as a reference only. This file can be modified as
#               required to meet the end-product requirements.
#
#********************************************************************************************************
#*


 #!/usr/bin/env python

import asyncio
import time
import socket
import struct

class EchoServerClientProtocol(asyncio.Protocol):
    def connection_made(self, transport):
        self.transport = transport

        peername = transport.get_extra_info('peername')
        print('Connection from {}'.format(peername))

        sock = transport.get_extra_info('socket')
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))


    def data_received(self, data):
        message = data.decode()
        print('Data received: {!r}'.format(message))

        print('Send: {!r}'.format(message))
        self.transport.write(data)

        time.sleep(0.02)

        print('Close the client socket')
        self.transport.close()


loop = asyncio.get_event_loop()
# Each client connection will create a new protocol instance
coro = loop.create_server(EchoServerClientProtocol, '0.0.0.0', 10001)
server = loop.run_until_complete(coro)

# Serve requests until CTRL+c is pressed
print('Serving on {}'.format(server.sockets[0].getsockname()))
try:
    loop.run_forever()
except KeyboardInterrupt:
    pass

# Close the server
server.close()
loop.run_until_complete(server.wait_closed())
loop.close()
