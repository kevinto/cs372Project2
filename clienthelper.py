'''
 Filename: client.py
 Coded by: Kevin To
 Purpose - [CS372 Project 2] - File contains helper methods for the 
                               client code. 
'''
import socket
import sys
import struct
import time
import SocketServer

# Purpose: Close server connection and exit client
# Params:
#		socket: Object that holds the server connection info
def closeClient(s):
    try:
        s.close()
        sys.exit()
    except:
        print "Server disconnected..."
        sys.exit()

# Purpose: Set up connection to server
# Params:
#       argv: string array of parameters passed into clint.py
# Reference: http://www.bogotobogo.com/python/python_network_programming_server_client.php
def initCommandSocket(argv):
    tcpIp = sys.argv[1]
    tcpPort = int(sys.argv[2]) 
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((tcpIp, tcpPort))
    return s

# Purpose: Set up connection to server
# Params:
#       argv: string array of parameters passed into clint.py
# Reference: https://docs.python.org/2/library/socketserver.html 
def initiateContact(argv):
    tcpIp = sys.argv[1]
    tcpPort = int(sys.argv[2]) 
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((tcpIp, tcpPort))
    return s

# Purpose: To send the command to the server
# Params:
#		socket: Object that holds the server connection info
#		argv: Array containing command params
def makeRequest(s, argv):
    sendString = GenerateServerCommand(argv);
    
    try:
        print "Sending: " + sendString 
        s.send(sendString)
    except:
        print "Error: Server disconnected..."
        sys.exit()

# Purpose: Generates server command string based on the sys args
#          passed in.
# Params:
#		socket: Object that holds the server connection info
#		argv: Array containing command params
def GenerateServerCommand(argv):
    if len(argv) == 5:
        return argv[3] + "," + argv[4] + "\n";
    elif len(argv) == 6:
        return argv[3] + "," + argv[5] + "," + argv[4] + "\n";
    else:
        return "";
        
# Purpose: To wait for the server message to come
# Params:
#       s: Object that holds the server connection info
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
#        argv: string array of parameters passed into clint.py
def checkArgs(argv):
    # Check that the correct number of arguments are sent in
    if len(argv) != 5 and len(argv) != 6:
        print 'Error: Valid commands can be either of the following:'
        print 'ftclient <hostname> <server port> <command> <data port>'
        print 'ftclient <hostname> <server port> <command> <file name> <data port>'
        return False

    dataPort = 0
    if len(argv) == 5:
        dataPort = argv[4]

    if len(argv) == 6:
        dataPort = argv[5]

    serverPort = 0
    try:
        # Convert port param into a number
        serverPort = int(argv[2])
        dataPortNumber = int(dataPort)
    except:
        print 'Error: One or both ports are not valid numbers'

    # Check if the server port is within an acceptable numeric range
    if serverPort < 1024 or 65535 < serverPort:
        print 'Error: Server Port has to be in the range 1024 to 65525'
        return False

    # Check if the data port is within an acceptable numeric range
    if dataPortNumber < 1024 or 65535 < dataPortNumber:
        print 'Error: Data Port has to be in the range 1024 to 65525'
        return False

    # Check if supported commands are sent in
    if (argv[3] != '-l') and (argv[3] != '-g'):
        print 'Error: Only -l and -g commands are supported'
        return False

    return True
    
# Purpose: Gets the data port from the command line args
# Params:
#		argv: Array containing command params
def GetDataPort(argv):
    if len(argv) == 5:
        return int(argv[4]);
    elif len(argv) == 6:
        return int(argv[5]);
    else:
        return -1;