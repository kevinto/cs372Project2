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

# Check arguments
if (not clienthelper.checkArgs(sys.argv)):
    sys.exit()    

# Setup socket connection
s = clienthelper.initContact(sys.argv)

userMessage = ""
userInput = ""
serverMessage = ""
while True:
    try:
        clienthelper.sendUserMsgToServer(s, sys.argv[3] + "\n")
        clienthelper.closeClient(s)

        serverMessage = clienthelper.receiveServerMsg(s)

        # Check if server wants to close the connection
        if "\quit\n" in serverMessage:
            clienthelper.closeClient(s)

        print serverMessage,

    except KeyboardInterrupt:
        clienthelper.closeClient(s)