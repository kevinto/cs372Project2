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
#define LENGTH 512
#define NUMBER_ALLOWED_CHARS 27 // Represents the number of different types of allowed characters
#define BUFFERLENGTH 50 // Max allowed length in a string buffer
#define FILELISTLENGTH 1024 // Max allowed length for directory list

// Global variable to track number of children
int number_children = 0;

void ProcessConnection(int socket);
int GetTempFD();
void ReceiveClientFile(int socket, FILE *tempFilePointer);
void SendFileToClient(int socket, int tempFilePointer);
void AddNewLineToEndOfFile(FILE *filePointer);
int GetSizeOfPlaintext(FILE *filePointer);
int GetSizeOfKeyText(FILE *filePointer);
void SavePlainTextToString(char *plainTextString, int plainTextSize, FILE *filePointer);
void SaveKeyTextToString(char *keyTextString, int keyTextSize, FILE *filePointer);
void EncyptText(char *plainTextString, int plainTextSize, char *keyTextString, int keyTextSize, char *cipherText);
int GetCharToNumberMapping(char character);
char GetNumberToCharMapping(int number);
void ReceiveClientCommand(int socket, char *clientCommand, char *transferFileName, int *dataPort, char *clientHostName);
void SendClientHandshakeResponse(int socket, char *serverResponse);
int GetNumCommas(char *strValue, int strLen);
int GetCommaIdx(char *strValue, int strLen, int occurance);
void OutputServerReqMsg(char *clientCommand, char *transferFileName, int dataPort);
void SendFileListToServer(int sockfd);
void SendFileToServer(int sockfd, char *transferFileName);
void GetFileList(char *returnArr, int arrLen);

// Signal handler to clean up zombie processes
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
	if (argc < 2)
	{
		printf("usage: server <port>\n");
		exit(1);
	}
	
	// --------------- Section to setup socket -----------------------

	int sockfd, newsockfd, sin_size, pid;
	struct sockaddr_in addr_local; // client addr
	struct sockaddr_in addr_remote; // server addr
	int portNumber = atoi(argv[1]);
	struct sigaction sa;

	// Get the socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		fprintf(stderr, "ERROR: Failed to obtain Socket Descriptor. (errno = %d)\n", errno);
		exit(1);
	}
	else
	{
		// printf("[otp_enc_d] Obtaining socket descriptor successfully.\n"); // For debugging only
	}

	// Fill the client socket address struct
	addr_local.sin_family = AF_INET; // Protocol Family
	addr_local.sin_port = htons(portNumber); // Port number
	addr_local.sin_addr.s_addr = INADDR_ANY; // AutoFill local address
	bzero(&(addr_local.sin_zero), 8); // Flush the rest of struct

	// Bind a port
	if ( bind(sockfd, (struct sockaddr*)&addr_local, sizeof(struct sockaddr)) == -1 )
	{
		fprintf(stderr, "ERROR: Failed to bind Port. (errno = %d)\n", errno);
		exit(1);
	}
	else
	{
		// printf("[otp_enc_d] Binded tcp port %d in addr 127.0.0.1 sucessfully.\n", portNumber); // For debugging only
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
			printf("Connection from %s\n", he->h_name);
		}

		// Remove THIS
		// close(sockfd);
		// exit(1);

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
			ProcessConnection(newsockfd);
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
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  This code gets run in the forked process to handle the actual
 * *  server processing of the client request.
 * *
 * ***************************************************************/
void ProcessConnection(int commandSocket)
{
	int dataPort = -1;
	char clientCommand[BUFFERLENGTH];
	bzero(clientCommand, BUFFERLENGTH);
	char transferFileName[BUFFERLENGTH];
	bzero(transferFileName, BUFFERLENGTH);
	char clientHostName[BUFFERLENGTH];
	bzero(clientHostName, BUFFERLENGTH);
	ReceiveClientCommand(commandSocket, clientCommand, transferFileName, &dataPort, clientHostName);

	// For debugging	
	// printf("clientCommand: %s\n", clientCommand);	
	// printf("transferFileName: %s\n", transferFileName);	
	// printf("dataport: %d\n", dataPort);	
	// printf("clientHostName: %s\n", clientHostName);	
	
	OutputServerReqMsg(clientCommand, transferFileName, dataPort);
	close(commandSocket);
	
	// printf("Server terminated child\n");
	// exit(0); // Exiting the child process.
	
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
	inet_pton(AF_INET, "127.0.0.1", &remote_addr.sin_addr);
	bzero(&(remote_addr.sin_zero), 8);

	// Try to connect the remote 
	if (connect(dataSocket, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Error: could not contact client on port %d\n", dataPort);
		exit(2);
	}
	else
	{
		// printf("Connected to server at port %d...ok!\n", dataPort); // For debugging
	}

	if (strcmp(clientCommand, "-l") == 0)
	{
		printf("Sending directory contents to %s:%d.\n", clientHostName, dataPort);
		SendFileListToServer(dataSocket);
	}
	else if (strcmp(clientCommand, "-g") == 0)
	{
		SendFileToServer(dataSocket, transferFileName);
	}

	// Receives the server status. We will only send the plain text data if the server is 
	//	willing to accept it.
	// ReceiveServerHandshakeConfirm(dataSocket, handshakeResponse);
	close(dataSocket);
	printf("Server terminated child\n");
	exit(0); // Exiting the child process.
}

/**************************************************************
 * * Entry:
 * *  sockfd - the socket to send the file list to.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the client's name to the server.
 * *
 * ***************************************************************/
void SendFileListToServer(int sockfd)
{
	char sendBuffer[LENGTH];
	bzero(sendBuffer, LENGTH);
	strncpy(sendBuffer, "otp_enc", LENGTH);

	char fileList[FILELISTLENGTH];
	bzero(fileList, FILELISTLENGTH);
	GetFileList(fileList, FILELISTLENGTH);
	
	int sendSize = 7;
	if (send(sockfd, sendBuffer, sendSize, 0) < 0)
	{
		printf("Error: Failed to send initial handshake.\n");
	}
}

/**************************************************************
 * * Entry:
 * *  sockfd - the socket to send the file list to.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the client's name to the server.
 * *
 * * Reference: http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
 * *
 * ***************************************************************/
void GetFileList(char *dirList, int arrLen)
{
	// Get host name of server to send
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
	
    // printf("dirList: %s\n", dirList);
}

/**************************************************************
 * * Entry:
 * *  sockfd - the socket to send the file list to.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the client's name to the server.
 * *
 * ***************************************************************/
void SendFileToServer(int sockfd, char *transferFileName)
{
	char sendBuffer[LENGTH];
	bzero(sendBuffer, LENGTH);
	strncpy(sendBuffer, "fil_na", LENGTH);
	// strncpy(sendBuffer, "otp_dec", LENGTH);

	int sendSize = 7;
	if (send(sockfd, sendBuffer, sendSize, 0) < 0)
	{
		printf("Error: Failed to send initial handshake.\n");
	}
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
 * * 	Sends a response to the client's initial handshake message
 * *
 * ***************************************************************/
void SendClientHandshakeResponse(int socket, char *serverResponse)
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
	
	// Clean the newline character
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

// -----------------------------------------------------------------------------

/**************************************************************
 * * Entry:
 * *  plainTextString - the plaintext string
 * *	plainTextSize - the plaintext size 
 * *	keyTextString - the key string
 * *	keyTextSize - the key size
 * *	cipherText - the ciper text, returned by ref
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Gets the cipher text from the given plaintext and key
 * *
 * ***************************************************************/
void EncyptText(char *plainTextString, int plainTextSize, char *keyTextString, int keyTextSize, char *cipherText)
{
	int i;
	char currEncChar;
	int currEncCharNumber;
	int currPlainTextNumber;
	int currKeyTextNumber;
	for (i = 0; i < plainTextSize; i++)
	{
		// Get the number mappings
		currPlainTextNumber = GetCharToNumberMapping(plainTextString[i]);
		currKeyTextNumber = GetCharToNumberMapping(keyTextString[i]);

		// Get the number after encyption
		currEncCharNumber = (currPlainTextNumber + currKeyTextNumber) % NUMBER_ALLOWED_CHARS;

		// Get the character from the encryption number
		currEncChar = GetNumberToCharMapping(currEncCharNumber);

		cipherText[i] = currEncChar;
		// For debugging
		// printf("current enc number : %d\n", currEncCharNumber);
		// printf("current character : %c\n", currEncChar);
	}
}

/**************************************************************
 * * Entry:
 * *  number - an int number 
 * *
 * * Exit:
 * *  Returns the character that is mapped to the number. Returns 
 * *	'e' if it is an unsupported number.
 * *
 * * Purpose:
 * * 	Gets the character for the specified number. The number range
 * *	is between 0 and 27, inclusive.
 * *
 * ***************************************************************/
char GetNumberToCharMapping(int number)
{
	static const char possibleChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

	// Return if the number is out of bounds
	if (number < 0 || 27 < number)
	{
		// Lower case 'e' means that there was an invalid number
		return 'e';
	}

	return possibleChars[number];
}

/**************************************************************
 * * Entry:
 * *  character - a character
 * *
 * * Exit:
 * *  Returns the number that is mapped to the character. Returns 
 * *	-1 if it is an invalid character.
 * *
 * * Purpose:
 * * 	Gets the number that maps to the specified character
 * *
 * ***************************************************************/
int GetCharToNumberMapping(char character)
{
	static const char possibleChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

	int i;
	for (i = 0; i < NUMBER_ALLOWED_CHARS; i++)
	{
		if (character == possibleChars[i])
		{
			return i;
		}
	}

	return -1;
}

/**************************************************************
 * * Entry:
 * *  keyTextString - the key string
 * * 	keyTextSize	- the size of the key string
 * *	filePointer - the file containing the key string
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Saves the key from the file into the specified string.
 * *
 * ***************************************************************/
void SaveKeyTextToString(char *keyTextString, int keyTextSize, FILE *filePointer)
{
	char readBuffer[LENGTH];
	int i;
	int fileTracker = 0;
	int stringTracker = 0;
	int foundFirstSemiColon = 0;

	// Set file pointer to the start of the file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}

	// Count the number of characters
	bzero(readBuffer, LENGTH);
	while (fread(readBuffer, sizeof(char), LENGTH, filePointer) > 0)
	{
		// Loop through the buffer to count characters
		for (i = 0; i < LENGTH; i++)
		{
			// Check if we found the first semicolon
			if (!foundFirstSemiColon && readBuffer[i] == ';')
			{
				foundFirstSemiColon = 1;
				continue;
			}

			if (foundFirstSemiColon)
			{
				keyTextString[stringTracker] = readBuffer[i]; // Copy the file contents to the string
				stringTracker++;
				fileTracker++;
			}

			// Exit loop if we reached the end of the key length
			if (fileTracker == (keyTextSize))
			{
				break;
			}

		}
		bzero(readBuffer, LENGTH);
	}
}

/**************************************************************
 * * Entry:
 * *  plainTextString - the plaintext string
 * * 	plainTextSize	- the size of the plaintext string
 * *	filePointer - the file containing the plaintext string
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Saves the plaintext from the file into the specified string.
 * *
 * ***************************************************************/
void SavePlainTextToString(char *plainTextString, int plainTextSize, FILE *filePointer)
{
	char readBuffer[LENGTH];
	int i;
	int fileTracker = 0;
	int stringTracker = 0;

	// Set file pointer to the start of the file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}

	// Count the number of characters
	bzero(readBuffer, LENGTH);
	while (fread(readBuffer, sizeof(char), LENGTH, filePointer) > 0)
	{
		// Loop through the buffer to count characters
		for (i = 0; i < LENGTH; i++)
		{
			// Exit loop if we reached the end of the plain text portion of the file
			if (fileTracker == (plainTextSize))
			{
				break;
			}

			plainTextString[stringTracker] = readBuffer[i]; // Copy the file contents to the string
			stringTracker++;
			fileTracker++;
		}
		bzero(readBuffer, LENGTH);
	}
}

/**************************************************************
 * * Entry:
 * *  filePointer - The file pointer to the file containing the 
 * *								plaintext and key, semi-colon delimited.
 * *
 * * Exit:
 * *  Returns the key size as an int.
 * *
 * * Purpose:
 * * 	Gets the key size from the file containing both the 
 * *	plaintext and key.
 * *
 * ***************************************************************/
int GetSizeOfKeyText(FILE *filePointer)
{
	char readBuffer[LENGTH];
	int i;
	int keyTextSize = 0;
	int foundFirstSemiColon = 0;
	int foundLastSemiColon = 0;

	// Set file pointer to the start of the file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}

	// Count the number of characters
	bzero(readBuffer, LENGTH);
	while (fread(readBuffer, sizeof(char), LENGTH, filePointer) > 0)
	{
		// Loop through the buffer to count characters
		for (i = 0; i < LENGTH; i++)
		{
			// Found the first semi-colon delimiter.
			if ((foundFirstSemiColon == 0) && readBuffer[i] == ';')
			{
				foundFirstSemiColon = 1;
				continue;
			}

			// Count the characters after the first semi-colon
			if (foundFirstSemiColon)
			{
				// Found the file end semicolon. Exit.
				if (readBuffer[i] == ';')
				{
					foundLastSemiColon = 1;
					break;
				}

				keyTextSize++;
			}
		}
		bzero(readBuffer, LENGTH);

		if (foundLastSemiColon)
		{
			// Found the last semi-colon, exit out of the loop
			break;
		}
	}

	return keyTextSize;
}

/**************************************************************
 * * Entry:
 * *  filePointer - The file pointer to the file containing the 
 * *								plaintext and key, semi-colon delimited.
 * *
 * * Exit:
 * *  Returns the plaintext size as an int.
 * *
 * * Purpose:
 * * 	Gets the plaintext size from the file containing both the 
 * *	plaintext and key.
 * *
 * ***************************************************************/
int GetSizeOfPlaintext(FILE *filePointer)
{
	char readBuffer[LENGTH];
	int i;
	int fileSize = 0;
	int foundFirstSemiColon = 0;

	// Set file pointer to the start of the file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}

	// Count the number of characters
	bzero(readBuffer, LENGTH);
	while (fread(readBuffer, sizeof(char), LENGTH, filePointer) > 0)
	{
		// Loop through the buffer to count characters
		for (i = 0; i < LENGTH; i++)
		{
			// Found the semi-colon delimiter. Break from loop
			if (readBuffer[i] == ';')
			{
				foundFirstSemiColon = 1;
				break;
			}

			fileSize++; // Keep track of the file size
		}
		bzero(readBuffer, LENGTH);

		if (foundFirstSemiColon)
		{
			// Found the delimiter. Break out of the loop
			break;
		}
	}

	return fileSize;
}

/**************************************************************
 * * Entry:
 * *  socket - the file desc for the socket
 * *	tempFilePointer - the file desc for the temp file
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the contents of the temp file to the client.
 * *
 * ***************************************************************/
void SendFileToClient(int socket, int tempFilePointer)
{
	char sendBuffer[LENGTH]; // Send buffer
	// printf("[otp_enc_d] Sending received file to the Client..."); // For debugging only
	if (tempFilePointer == 0)
	{
		// fprintf(stderr, "ERROR: File %s not found on server. (errno = %d)\n", fs_name, errno);
		fprintf(stderr, "ERROR: File temp received not found on server.");
		exit(1);
	}

	bzero(sendBuffer, LENGTH);
	int readSize;
	while ((readSize = read(tempFilePointer, sendBuffer, LENGTH)) > 0)
	{
		if (send(socket, sendBuffer, readSize, 0) < 0)
		{
			// fprintf(stderr, "ERROR: Failed to send file %s. (errno = %d)\n", fs_name, errno);
			fprintf(stderr, "ERROR: Failed to send file temp received.");
			exit(1);
		}
		bzero(sendBuffer, LENGTH);
	}
	// printf("Ok sent to client!\n"); // For debugging only
}

/**************************************************************
 * * Entry:
 * *  socket - the file desc for the socket
 * *	tempFilePointer - the file desc for the temp file
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Gets the file from the client and puts it into the temp file.
 * *
 * ***************************************************************/
void ReceiveClientFile(int socket, FILE *tempFilePointer)
{
	char receiveBuffer[LENGTH]; // Receiver buffer
	bzero(receiveBuffer, LENGTH); // Clear out the buffer

	// Loop the receiver until all file data is received
	int bytesReceived = 0;
	while ((bytesReceived = recv(socket, receiveBuffer, LENGTH, 0)) > 0)
	{
		int bytesWrittenToFile = fwrite(receiveBuffer, sizeof(char), bytesReceived, tempFilePointer);
		if (bytesWrittenToFile < bytesReceived)
		{
			printf("[otp_enc_d] File write failed on server.\n");
		}
		bzero(receiveBuffer, LENGTH);
		if (bytesReceived == 0 || bytesReceived != 512)
		{
			break;
		}
	}
	if (bytesReceived < 0)
	{
		if (errno == EAGAIN)
		{
			printf("recv() timed out.\n");
		}
		else
		{
			fprintf(stderr, "recv() failed due to errno = %d\n", errno);
			exit(1);
		}
	}
	// printf("Ok received from client!\n"); // For debugging only
}

/**************************************************************
 * * Entry:
 * *  N/a
 * *
 * * Exit:
 * *  Returns the temp file descriptor
 * *
 * * Purpose:
 * * 	Gets the temp file descriptor. This temp file will clean it  
 * *  self up at the program end.
 * *
 * ***************************************************************/
int GetTempFD()
{
	char tempFileNameBuffer[32];
	char buffer[24];
	int filedes;

	// Zero out the buffers
	bzero(tempFileNameBuffer, sizeof(tempFileNameBuffer));
	bzero(buffer, sizeof(buffer));

	// Set up temp template
	strncpy(tempFileNameBuffer, "/tmp/myTmpFile-XXXXXX", 21);
	// strncpy(buffer, "Hello World", 11); // Need for test only

	errno = 0;
	// Create the temporary file, this function will replace the 'X's
	filedes = mkstemp(tempFileNameBuffer);

	// Call unlink so that whenever the file is closed or the program exits
	// the temporary file is deleted
	unlink(tempFileNameBuffer);

	if (filedes < 1)
	{
		printf("\n Creation of temp file failed with error [%s]\n", strerror(errno));
		return 1;
	}

	return filedes;
}

/**************************************************************
 * * Entry:
 * *  filePointer - the file pointer to an opened writeable file
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  Adds a new line to the end of a file.
 * *
 * ***************************************************************/
void AddNewLineToEndOfFile(FILE *filePointer)
{
	char newlineBuffer[1] = "\n";

	// Set the file pointer to the end of the file
	if (fseek(filePointer, 0, SEEK_END) == -1)
	{
		printf("Received file pointer reset failed\n");
	}

	// Write the newline char to the end of the file
	fwrite(newlineBuffer, sizeof(char), 1, filePointer);

	// Set file pointer to the start of the temp file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}
}