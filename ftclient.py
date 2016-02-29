'''
 Filename: client.py
 Coded by: Kevin To
 Purpose - [CS372 Project 2] - File contains the ftp client code
           written in python.
'''
#!/usr/bin/env python

import socket
import sys
import struct
import time
import clienthelper
import SocketServer

# Handler for the server code to recieve data from ftserver
class MyTCPHandler(SocketServer.BaseRequestHandler):
    """
    The request handler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """

    def handle(self):
        # self.request is the TCP socket connected to the client
        self.data = self.request.recv(1024).strip()
        print "{} wrote:".format(self.client_address[0])
        print self.data
        # just send back the same data, but upper-cased
        # self.request.sendall(self.data.upper())

# Check arguments
if (not clienthelper.checkArgs(sys.argv)):
    sys.exit()

# Setup socket connection
commandSocket = clienthelper.initCommandSocket(sys.argv)


# serverMessage = ""
# while True:
try:
    # Create the server
    HOST, PORT = sys.argv[1], clienthelper.GetDataPort(sys.argv)
    dataSocket = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)
    
    clienthelper.sendCommandToServer(commandSocket, sys.argv)
    dataSocket.handle_request()
    
    # serverMessage = clienthelper.receiveServerMsg(s)
    # print serverMessage,
    
    # Clean up sockets
    commandSocket.close()
    dataSocket.server_close()
    sys.exit()
    
except KeyboardInterrupt:
    commandSocket.close()
    dataSocket.server_close()
    sys.exit()