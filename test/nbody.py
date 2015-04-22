#!/usr/bin/python

import jrpc

server = None
server = jrpc.service.SocketProxy(8764) #The server's listening port
print server.run_simulation([[0,0,1], [0,1,1], [1,0,1], [1,1,1]], 0.00001, 100000)