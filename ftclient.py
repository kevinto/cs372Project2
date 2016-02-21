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

handle = ""
userMessage = ""
userInput = ""
serverMessage = ""
while True:
	try:
		# Get user handle
		if not handle:
			handle = raw_input("Enter your handle: ")
			handle += "> "

		clienthelper.sendUserMsgToServer(s, handle)
		clienthelper.closeClient(s)

		serverMessage = clienthelper.receiveServerMsg(s)

		# Check if server wants to close the connection
		if "\quit\n" in serverMessage:
			clienthelper.closeClient(s)

		print serverMessage,

	except KeyboardInterrupt:
		clienthelper.closeClient(s)