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
# Check arguments
if (not clienthelper.checkArgs(sys.argv)):
    sys.exit()

# Setup command socket connection
commandSocket = clienthelper.initiateContact(sys.argv)

# Setup data socket connection
dataSocket = clienthelper.setupDataConnection(sys.argv)

# serverMessage = ""
# while True:
try:
    clienthelper.makeRequest(commandSocket, sys.argv)
    dataSocket.handle_request()
    
    # serverMessage = clienthelper.receiveServerMsg(s)
    # print serverMessage,
    
    # Clean up sockets
    commandSocket.close()
    dataSocket.server_close()
    sys.exit()
    
except KeyboardInterrupt:
    # Clean up sockets
    commandSocket.close()
    dataSocket.server_close()
    sys.exit()