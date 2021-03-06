'''
 Filename: client.py
 Coded by: Kevin To
 Purpose - [CS372 Project 2] - File contains helper methods for the 
                               client code.
 Sample command: 
    List server's file directory:
        python ftclient.py <Host Name> <command port#> -l <data port#>
    
    Get a file from the server:
        python ftclient.py <Host Name> <command port#> -g <file name> <data port#>
'''
import socket
import sys
import struct
import time
import SocketServer
import os.path

# Purpose: Set up socket to the server
# Params:
#       argv: string array of parameters passed into client.py
# Reference: https://docs.python.org/2/library/socketserver.html 
def initiateContact(argv):
    tcpIp = sys.argv[1]
    tcpPort = int(sys.argv[2]) 
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        s.connect((tcpIp, tcpPort))
    except socket.error:
        print "Error: Failed to connect. Either host name or command port number is incorrect. Please make sure your values are correct and try again."
        sys.exit()
    return s

# Purpose: Class definition for handler that gets server data through the 
#          command port.
# Params:
#       SocketServer.BaseRequestHandler: Base class where we are modifying
#                                        the handler to get data from the server
# Reference: https://docs.python.org/2/library/socketserver.html
class MyTCPHandler(SocketServer.BaseRequestHandler):
    action = ""
    dataPort = ""
    transFileName = ""
    ftServerHostName = ""
    
    # Override the base class request handler
    def handle(self):
        self.ftServerHostName = socket.gethostbyaddr(self.client_address[0])[0]
        
        # Note: self.request is the TCP socket connected to the client
        if self.action == "-l":
            # Get dir list from server
            self.data = self.request.recv(1024).strip()
            self.outputDirList()
        else:
            # Get file from server
            
            # First command sent through will be a status command. Currently
            # the only status command is 'e' for error file not found
            status = self.request.recv(1).strip()
            if (status == "e"):
                print "%s:%d says FILE NOT FOUND" % (self.ftServerHostName, self.dataPort)
            else:
                if (os.path.isfile(self.transFileName)):
                    # Reference: http://stackoverflow.com/questions/82831/how-to-check-whether-a-file-exists-using-python
                    print "Warning: File already exists. Overwriting anyways."
                    
                try:
                    self.data = self.receiveFile()
                    self.saveTransferedFile()
                    print "File transfer complete."
                except Exception, e:
                    print "Error: Something is wrong with receiving the file. Please try again in 5 seconds."
    
    # Output the directory list to the terminal
    def outputDirList(self):
        strLen = len(self.data)
        cleanDirList = ""
        
        # For some reason the '\x00' char appears in the payload. This code 
        # will clean all that up.
        for char in self.data:
            if char != '\x00':
                cleanDirList += char
        
        # The server response of all the file names are comma delimited 
        dirList = cleanDirList.split(",")
        
        print "Receiving directory structure from %s:%d" % (dirList[0], self.dataPort)
        
        for i in range(1, len(dirList)):
            print dirList[i]
    
    # Keeps reading data from the server stream until it encounters null
    # Reference: http://stackoverflow.com/questions/27241804/sending-a-file-over-tcp-sockets-in-python
    def receiveFile(self):
        print "Receiving \"%s\" from %s:%d" % (self.transFileName, self.ftServerHostName, self.dataPort)
        
        data = ''
        foundNull = False
        total_data=[]
    
        data = self.request.recv(100)
        while data:
            total_data.append(data)
            data = self.request.recv(100)
    
        # Return string constructed from data array 
        return ''.join(total_data)
    
    # Save file contents to a file on the client folder
    def saveTransferedFile(self):
        saveFile = open(self.transFileName, "w")
        saveFile.write(self.data)
        saveFile.close()
        
# Purpose: Set up data connection to server
# Params:
#       argv: string array of parameters passed into clint.py
# Reference: https://docs.python.org/2/library/socketserver.html 
def setupDataConnection(argv):
    MyTCPHandler.action = argv[3]
    MyTCPHandler.dataPort = GetDataPort(argv)
    MyTCPHandler.transFileName = argv[4]
    
    HOST, PORT = argv[1], GetDataPort(argv)
    dataSocket = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)
    return dataSocket

# Purpose: To send the command to the server
# Params:
#		socket: Object that holds the server connection info
#		argv: Array containing command params
def makeRequest(s, argv):
    sendString = GenerateServerCommand(argv);
    
    try:
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
        return argv[3] + "," + argv[4] + "," + socket.gethostname() + "\n";
    elif len(argv) == 6:
        return argv[3] + "," + argv[5] + "," + argv[4] + "," + socket.gethostname() + "\n";
    else:
        return "";
        
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