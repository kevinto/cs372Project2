/**************************************************************
 * *  Filename: server.c
 * *  Coded by: Kevin To
 * *  Purpose - [CS372 Project 2] Acts as a server to process FTP 
 * *			commands.
 * *
 * *            Sample command:
 * *              server <port> 
 * *
 * *              port = the port for the client to connect to.
 * *
 * ***************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h> 

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LISTEN_QUEUE 5
#define LENGTH 1024 
#define NUMBER_ALLOWED_CHARS 27 // Represents the number of different types of allowed characters
#define BUFFERLENGTH 50 // Max allowed length in a string buffer
#define FILELISTLENGTH 1024 // Max allowed length for directory list

// Global variable to track number of children
int number_children = 0;

void handleRequest(int commandSocket, char *clientHostName);
void SendFileToClient(int socket, int filePointer);
void ReceiveClientCommand(int socket, char *clientCommand, char *transferFileName, int *dataPort, char *clientHostName);
void SendFtClientFileStatus(int socket, char *serverResponse);
int GetNumCommas(char *strValue, int strLen);
int GetCommaIdx(char *strValue, int strLen, int occurance);
void OutputServerReqMsg(char *clientCommand, char *transferFileName, int dataPort);
void SendFileListToServer(int sockfd);
void SendFileToServer(int sockfd, char *transferFileName);
void GetFileList(char *returnArr);
int HostnameToIp(char *hostname, char *ip);
int startup (int argc, char *argv[]);

/**************************************************************
 * * Entry:
 * *  sig - the signal event 
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  Signal handler to clean up zombie processes
 * *
 * * Reference: http://stackoverflow.com/questions/19461744/make-parent-wait-for-all-child-processes
 * ***************************************************************/
static void wait_for_child(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
	number_children--;
}

/**************************************************************
 * * Entry:
 * *  argc - the number of arguments passed into this program
 * *  argv - a pointer to the char array of all the arguments
 * *         passed into this program
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  This is the entry point into the program.
 * *
 * ***************************************************************/
int main (int argc, char *argv[])
{
	return startup(argc, argv);
}

/**************************************************************
 * * Entry:
 * *  argc - the number of arguments passed into this program
 * *  argv - a pointer to the char array of all the arguments
 * *         passed into this program
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  Starts up the server code.
 * *
 * ***************************************************************/
int startup (int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("usage: ftserver <port>\n");
		exit(1);
	}
	
	// --------------- Section to setup socket -----------------------
	// Reference: http://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
	int sockfd, newsockfd, sin_size, pid;
	struct sockaddr_in addr_local; // client addr
	struct sockaddr_in addr_remote; // server addr
	int portNumber = atoi(argv[1]);
	struct sigaction sa;
	char cHostName[BUFFERLENGTH];
	bzero(cHostName, BUFFERLENGTH);	

	// Get the socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		fprintf(stderr, "ERROR: Failed to obtain Socket Descriptor. (errno = %d)\n", errno);
		exit(1);
	}

	// Fill the client socket address struct
	addr_local.sin_family = AF_INET; // Protocol Family
	addr_local.sin_port = htons(portNumber); // Port number
	addr_local.sin_addr.s_addr = INADDR_ANY; // AutoFill local address
	bzero(&(addr_local.sin_zero), 8); // Flush the rest of struct

	// Bind a port
	if ( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
	{
		fprintf(stderr, "ERROR: Failed to bind Port. Please select another port. (errno = %d)\n", errno);
		exit(1);
	}

	// Listen to port
	if (listen(sockfd, LISTEN_QUEUE) == -1)
	{
		fprintf(stderr, "ERROR: Failed to listen Port. (errno = %d)\n", errno);
		exit(1);
	}
	else
	{
		printf ("Server open on %d\n", portNumber);
	}

	// Set up the signal handler to clean up zombie children
	// Reference: http://www.thegeekstuff.com/2012/03/catch-signals-sample-c-code/
	sa.sa_handler = wait_for_child;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		return 1;
	}

	// Main loop that accepts and processes client connections
	while (1)
	{
		sin_size = sizeof(struct sockaddr_in);

		// Wait for any connections from the client
		if ((newsockfd = accept(sockfd, (struct sockaddr *)&addr_remote, &sin_size)) == -1)
		{
			fprintf(stderr, "ERROR: Obtaining new socket descriptor. (errno = %d)\n", errno);
			exit(1);
		}
		else
		{
			// Taken from here: http://beej.us/guide/bgnet/output/html/multipage/gethostbynameman.html
			struct hostent *he;
			struct in_addr ipv4addr;
			inet_pton(AF_INET, inet_ntoa(addr_remote.sin_addr), &ipv4addr);
			he = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
			strncpy(cHostName, he->h_name, BUFFERLENGTH);
			printf("Connection from %s\n", cHostName);
		}

		// Create child process to handle processing multiple connections
		number_children++; // Keep track of number of open children
		pid = fork();
		if (pid < 0)
		{
			perror("ERROR on fork");
			exit(1);
		}

		if (pid == 0)
		{
			// This is the client process
			close(sockfd);
			handleRequest(newsockfd, cHostName);
			exit(0);
		}
		else
		{
			close(newsockfd);
		}
	}
}

/**************************************************************
 * * Entry:
 * *  socket - the socket file descriptor
 * *  cHostName - the actual client host name
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  This code gets run in the forked process to handle the actual
 * *  server processing of the client request.
 * *
 * ***************************************************************/
void handleRequest(int commandSocket, char *cHostName)
{
	int dataPort = -1;
	char clientCommand[BUFFERLENGTH];
	bzero(clientCommand, BUFFERLENGTH);
	char transferFileName[BUFFERLENGTH];
	bzero(transferFileName, BUFFERLENGTH);
	char clientHostName[BUFFERLENGTH]; // Note: this variable is the ftserver hostname
	bzero(clientHostName, BUFFERLENGTH);
	ReceiveClientCommand(commandSocket, clientCommand, transferFileName, &dataPort, clientHostName);
	
	OutputServerReqMsg(clientCommand, transferFileName, dataPort);
	close(commandSocket);

	// Get IP address from ftclient host name
	char IP[100];
	HostnameToIp(cHostName, IP);
	
	int dataSocket;
	struct sockaddr_in remote_addr;
	
	// Get the Socket file descriptor 
	if ((dataSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("Error: Failed to obtain socket descriptor.\n");
		exit(2);
	}
	
	// Fill the socket address struct   
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(dataPort);
	inet_pton(AF_INET, IP, &remote_addr.sin_addr);
	bzero(&(remote_addr.sin_zero), 8);

	// Try to connect the client data port 
	if (connect(dataSocket, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Error: could not contact client on port %d\n", dataPort);
		exit(2);
	}

	if (strcmp(clientCommand, "-l") == 0)
	{
		printf("Sending directory contents to %s:%d.\n", clientHostName, dataPort);
		SendFileListToServer(dataSocket);
	}
	else if (strcmp(clientCommand, "-g") == 0)
	{
		// Reference: http://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c-cross-platform
		if(access(transferFileName, F_OK) != -1) 
		{
			// File exists
			SendFtClientFileStatus(dataSocket, "s");
			printf("Sending \"%s\" to %s:%d\n", transferFileName, cHostName, dataPort);
			SendFileToServer(dataSocket, transferFileName);
		} else {
			// File doesnt exist
			printf("File not found. Sending error message to %s:%d\n", cHostName, dataPort);
			SendFtClientFileStatus(dataSocket, "e");
		}
	}

	close(dataSocket);
	exit(0); // Exiting the child process.
}

/**************************************************************
 * * Entry:
 * *  hostname - the host name
 * *  ip - char array that will hold the ip address
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Gets the IP from the hostname.
 * *
 * * Reference: 
 * * http://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/
 * ***************************************************************/
int HostnameToIp(char *hostname , char *ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
    
    if ((he = gethostbyname(hostname)) == NULL) 
    {
        herror("gethostbyname");
        return 1;
    }
 
    addr_list = (struct in_addr **) he->h_addr_list;
    
    // Iterate through the network info struct to get the IP. 
    for(i = 0; addr_list[i] != NULL; i++) 
    {
        //Return the first one;
        strcpy(ip ,inet_ntoa(*addr_list[i]));
        return 0;
    }
     
    return 1;
}

/**************************************************************
 * * Entry:
 * *  sockfd - the socket to send the file list to.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the server's directory list to the client
 * *
 * ***************************************************************/
void SendFileListToServer(int sockfd)
{
	char fileList[FILELISTLENGTH];
	bzero(fileList, FILELISTLENGTH);
	GetFileList(fileList);
	
	int sendSize = FILELISTLENGTH;
	if (send(sockfd, fileList, sendSize, 0) < 0)
	{
		printf("Error: Failed to send directory list.\n");
	}
}

/**************************************************************
 * * Entry:
 * *  dirList - output for comma delimited list of file names
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Generates a comma delimited string of all the file
 * *    names in the server's directory
 * *
 * * Reference: http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
 * *
 * ***************************************************************/
void GetFileList(char *dirList)
{
	// Get host name of server to add to the beginning of the list 
    char name[50];
    gethostname(name, 50);
    strcat(dirList, name);
    strcat(dirList, ",");
    
    DIR *directoryHandle;
    struct dirent *directoryStruct;
    
    // Loop through all the files and add them to the list
    directoryHandle = opendir(".");
    if (directoryHandle)
    {
    	int currDirLen = 0;
    	int currDirListLen = 0;
        while ((directoryStruct = readdir(directoryHandle)) != NULL)
        {
            if (strcmp(directoryStruct->d_name, ".") != 0 && strcmp(directoryStruct->d_name, "..") != 0) 
            {
            	// Make sure we dont overflow our send buffer
            	currDirLen = strnlen(directoryStruct->d_name, NAME_MAX);
	    		currDirListLen = strnlen(dirList, FILELISTLENGTH);
	    		if ((currDirListLen + currDirLen) > FILELISTLENGTH)
	    		{
	    			break;
	    		}
	    		
	    		// Add the directory to the comma delimited list	
    			strcat(dirList, directoryStruct->d_name);
    			strcat(dirList, ",");
            }
        }
        
        closedir(directoryHandle);
    }
	
	// Remove the last comma 
	int lastCommaIdx = strnlen(dirList, FILELISTLENGTH) - 1;
	dirList[lastCommaIdx] = 0;
}

/**************************************************************
 * * Entry:
 * *  sockfd - the socket to send the file list to.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the requested file to the server
 * *
 * ***************************************************************/
void SendFileToServer(int sockfd, char *transferFileName)
{
	FILE * transferFilePointer;
	transferFilePointer = fopen(transferFileName, "r+");
	
	int fd = fileno(transferFilePointer);
	SendFileToClient(sockfd, fd);
	
	fclose(transferFilePointer);
}

/**************************************************************
 * * Entry:
 * *  clientCommand - char array that holds the client command
 * *  transferFileName - char array that holds the transfer file name 
 * *  dataPort - int passed by ref to hold port number
 * *
 * * Exit:
 * *  N/A
 * *
 * * Purpose:
 * *  Outputs the server request on to the console
 * *
 * ***************************************************************/
void OutputServerReqMsg(char *clientCommand, char *transferFileName, int dataPort)
{
	if (strcmp(clientCommand, "-l") == 0)
	{
		printf("List directory requested on port %d.\n", dataPort);
	}
	
	if (strcmp(clientCommand, "-g") == 0)
	{
		printf("File \"%s\" requested on port %d.\n", transferFileName, dataPort);
	}
}

/**************************************************************
 * * Entry:
 * *  socket - the socket to send the hankdshake acknoledgement from.
 * *  serverResponse - The single letter response from the server.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the file status to the client.
 * *
 * ***************************************************************/
void SendFtClientFileStatus(int socket, char *serverResponse)
{
	char sendBuffer[2]; // Send buffer
	bzero(sendBuffer, 2);
	strncpy(sendBuffer, serverResponse, 1);

	if (send(socket, sendBuffer, 1, 0) < 0)
	{
		printf("ERROR: Failed to send client the following message: %s", serverResponse);
		exit(1);
	}
}

/**************************************************************
 * * Entry:
 * *  socket - the socket to receive the client command, 
 * *           dataport, and tranfer file name (if applicable)
 * *  clientCommand - char array that holds the client command
 * *  transferFileName - char array that holds the transfer file name 
 * *  dataPort - int passed by ref to hold port number
 * *  clientHostName - char array that holds the client host name
 * *
 * * Exit:
 * *  N/A
 * *
 * * Purpose:
 * * 	Receives the command message from the client.
 * *
 * ***************************************************************/
void ReceiveClientCommand(int socket, char *clientCommand, char *transferFileName, int *dataPort, char *clientHostName)
{
	// Wait for client to send message
	char receiveBuffer[BUFFERLENGTH];
	bzero(receiveBuffer, BUFFERLENGTH);
	recv(socket, receiveBuffer, LENGTH, 0);

	// Get the comma instance in the command string
	int numCommas = GetNumCommas(receiveBuffer, BUFFERLENGTH);
	int commaIdx = GetCommaIdx(receiveBuffer, BUFFERLENGTH, 1);

	// Get the client command from the sent string	
	strncpy(clientCommand, receiveBuffer, commaIdx);
	
	// Get the data port from the sent string	
	char dataPortBuffer[BUFFERLENGTH];
	bzero(dataPortBuffer, BUFFERLENGTH);
	strncpy(dataPortBuffer, receiveBuffer + commaIdx + 1, BUFFERLENGTH);
	(*dataPort) = atoi(dataPortBuffer);

	if (numCommas == 2)
	{
		int commaIdx = GetCommaIdx(receiveBuffer, BUFFERLENGTH, 2);
		strncpy(clientHostName, receiveBuffer + commaIdx + 1, BUFFERLENGTH);
	}

	// Get file name if a third param is passed through	
	if (numCommas == 3)
	{
		// Example = "-l, 3333, filename, hostname". 
		// This code block finds the second and third comma and gets the value
		// in between
		int commaIdxStart = GetCommaIdx(receiveBuffer, BUFFERLENGTH, 2);
		int commaIdxEnd = GetCommaIdx(receiveBuffer, BUFFERLENGTH, 3);
		strncpy(transferFileName, receiveBuffer + commaIdxStart + 1, commaIdxEnd - commaIdxStart - 1);
		
		strncpy(clientHostName, receiveBuffer + commaIdxEnd + 1, BUFFERLENGTH);
	}
	
	// Clean the newline character from the client host name
	int i = 0;
	char currChar = ' ';
	while(currChar != 0)
	{
		if (i == BUFFERLENGTH)
		{
			break;
		}
		
		currChar = clientHostName[i];	
		if (currChar == '\n')
		{
			clientHostName[i] = 0;
			break;
		}
		
		i++;
	}
}

/**************************************************************
 * * Entry:
 * *  strValue - the string value you want to find a comma in.
 * *  strLen - the length of the string.
 * *  occurance - the comma occurance you want the index to.
 * *
 * * Exit:
 * *  Returns the index of the comma.
 * *
 * * Purpose:
 * * 	Gets the index of the specified comma occurance.
 * *
 * ***************************************************************/
int GetCommaIdx(char *strValue, int strLen, int occurance)
{
	int i = 0;
	int commaIdx = -1;
	int numCommas = 0;
	char currChar = ' ';
	
	while(currChar != '\n')
	{
		currChar = strValue[i++];
		if (currChar == ',')
		{
			numCommas++;
		}
		
		if (numCommas == occurance)
		{
			commaIdx = i - 1;
			break;
		}
		
		// Exit loop if we are trying to access an out of bounds index
		if (i >= strLen)
		{
			break;
		}
	}
	
	return commaIdx;
}

/**************************************************************
 * * Entry:
 * *  strValue - the string value you want to find a comma in.
 * *  strLen - the length of the string
 * *
 * * Exit:
 * *  Returns the number of commas found.
 * *
 * * Purpose:
 * * 	Gets the number of commas in a string.
 * *
 * ***************************************************************/
int GetNumCommas(char *strValue, int strLen)
{
	int i = 0;
	int numCommas = 0;
	char currChar = ' ';
	
	while(currChar != '\n')
	{
		currChar = strValue[i++];
		if (currChar == ',')
		{
			numCommas++;
		}
		
		if (i >= strLen)
		{
			break;
		}
	}
	
	return numCommas;
}

/**************************************************************
 * * Entry:
 * *  socket - the file desc for the socket
 * *  filePointer - the file desc for the transferring file
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends a file to the client.
 * *
 * ***************************************************************/
void SendFileToClient(int socket, int filePointer)
{
	char sendBuffer[LENGTH]; // Send buffer
	if (filePointer == 0)
	{
		fprintf(stderr, "ERROR: File not found on server.");
		exit(1);
	}
	
	bzero(sendBuffer, LENGTH);
	int readSize;
	while ((readSize = read(filePointer, sendBuffer, LENGTH)) > 0)
	{
		if (send(socket, sendBuffer, readSize, 0) < 0)
		{
			fprintf(stderr, "ERROR: Failed to send file.");
			exit(1);
		}
		bzero(sendBuffer, LENGTH);
	}
}
