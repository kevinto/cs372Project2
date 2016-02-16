'''
 Filename: client.py
 Coded by: Kevin To
 Purpose - [CS372 Project 1] - File contains helper methods for the 
                               client code. 
'''
import socket
import sys
import struct
import time


# Purpose: Close server connection and exit client
# Params:
#		socket: Object that holds the server connection info
def closeClient(s):
    try:
        s.send("quit\n")
        s.close()
        sys.exit()
    except:
        print "Server disconnected..."
        sys.exit()

# Purpose: Set up connection to server
# Params:
#		argv: string array of parameters passed into clint.py
# Reference: http://www.bogotobogo.com/python/python_network_programming_server_client.php
def initContact(argv):
	tcpIp = sys.argv[1]
	tcpPort = int(sys.argv[2]) 
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.connect((tcpIp, tcpPort))
	return s


# Purpose: To wait for the server message to come
# Params:
#		socket: Object that holds the server connection info
#		handle: String containing the client user name
def sendUserMsgToServer(s, handle):
	# Get user message
    userInput = raw_input(handle)
    userMessage = handle + userInput + "\n"

    try:
        # Check if user wants to close connection
        if userInput == "\quit" :
            s.send("quit\n")
            s.close()
            sys.exit()

        s.send(userMessage)
    except:
        print "Server disconnected..."
        sys.exit()

# Purpose: To wait for the server message to come
# Params:
#		s: Object that holds the server connection info
# Reference: http://www.binarytides.com/receive-full-data-with-the-recv-socket-function-in-python/
def receiveServerMsg(s):
    # Set socket to non-blocking. This enables the while loop below
    # to keep calling recv to monitor for any data received. This fixes
    # the problem where we are only receiving partial messages from the 
    # server. We will only stop receiving when we receive a null terminator.
    s.setblocking(0)
    
    data=''
    foundNull = False 
    total_data=[]

    while 1:
    	# Only exit loop if there is data and we found a null terminator
        if total_data and foundNull:
            break
         
        try:
            data = s.recv(8192)
            if data:
            	# Found data, append to array
                total_data.append(data)

                # Check for null termination
                if '\n' in data:
                    foundNull = True

            # Check if connection is terminated from client end
            # Reference: http://stackoverflow.com/questions/5686490/detect-socket-hangup-without-sending-or-receiving
            if len(data) == 0:
            	return "\quit\n"
        except:
            pass

    # Return string constructed from data array 
    return ''.join(total_data)

# Purpose: Check the arguments that are passed into client.py
# Params:
#		argv: string array of parameters passed into clint.py
def checkArgs(argv):
	# Check that the correct number of arguments are sent in
	if len(argv) != 3:
		print 'Error: Server Hostname and port number required'
		return False

	port = 0
	try:
		# Convert port param into a number
		port = int(argv[2])
	except:
		print 'Error: Port has to be a number'

	# Check if the port is within an acceptable numeric range
	if port < 1024 or 65535 < port:
		print 'Error: Port has to be in the range 1024 to 65525'
		return False

	return True