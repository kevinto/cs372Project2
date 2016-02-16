/**************************************************************
 * *  Filename: otp_dec.c
 * *  Coded by: Kevin To
 * *  Purpose - Acts as the client to send work to the decryption
 * *            daemon.
 * *            Sample command:
 * *              otp_dec ciphertext key port
 * *
 * *              ciphertext = file holding the cipher text to be
 * *                          decrypted.
 * *              key = file holding they key that will be used in
 * *                    the decryption.
 * *              port = the port to connect to the daemon on.
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

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LENGTH 512

void error(const char *msg);
void ConnectToServer(char *portString, char *cipherTextFileName, char *keyFileName);
void RemoveNewLineAndAddNullTerm(char *fileName);
void CheckKeyLength(long keySize, long cipherTextSize, char *keyName);
void ScanInvalidCharacters(char *stringToCheck, int stringLength);
void SendFileToServer(int sockfd, int tempFileDesc);
void ReceiveFileFromServer(int sockfd);
int CombineTwoFiles(char *fileOneName, char *fileTwoName);
int GetTempFD();
void AddNewLineToEndOfFile(FILE *filePointer);
void OutputTempFile(int tempFileDesc);
void BufRemoveNewLineAndAddSemiColon(char *buffer, int bufferSize);
void CheckIfFileEndingValid(char *fileName);
void SendHandshakeToServer(int sockfd);
void ReceiveServerHandshakeConfirm(int sockfd, char *handshakeResponse);

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
int main(int argc, char *argv[])
{
	// If the user did not enter the correct number of parameters,
	//  display the correct message.
	if (argc != 4)
	{
		printf("usage: otp_dec ciphertext key port\n");
		exit(1);
	}

	// ------------------------------ Get the cipher text ----------------------
	// This section gets the size of the cipher text file in order to check it against
	// 	the key size. This section also saves the cipher text file into a string, so
	//	we can analyze the string for invalid characters and send it to the server.

	FILE *filePointer = fopen(argv[1], "rb");
	if (filePointer == 0)
	{
		printf("ciphertext file does not exist\n");
		exit(1);
	}

	// Find the size of the file.
	fseek(filePointer, 0, SEEK_END); // Sets the position indicator to the end of the file
	long cipherTextSize = ftell(filePointer); // Gets the file size
	fseek(filePointer, 0, SEEK_SET); // Sets the position indicator to the start of the file

	// Get the string from the file. Remember cipherTextSize includes the newline
	//  character at the end of the file.
	char *cipherTextString = malloc(cipherTextSize + 1); // Allocates memory for the string taken from the file
	fread(cipherTextString, cipherTextSize, 1, filePointer); // Get the string from the file
	fclose(filePointer);

	cipherTextString[cipherTextSize] = 0; // Null terminate the string
	RemoveNewLineAndAddNullTerm(cipherTextString);

	// ------------------------------ Get the key ----------------------
	// This section gets the size of the key file in order to check it against
	// 	the ciphertext size. This section also saves the key file into a string, so
	//	we can analyze the string for invalid characters and send it to the server.

	filePointer = fopen(argv[2], "rb");
	if (filePointer == 0)
	{
		printf("key file does not exist\n");
		exit(1);
	}

	// Find the size of the file.
	fseek(filePointer, 0, SEEK_END); // Sets the position indicator to the end of the file
	long keySize = ftell(filePointer); // Gets the file size
	fseek(filePointer, 0, SEEK_SET); // Sets the position indicator to the start of the file

	// Get the string from the file. Remember keySize includes the newline
	//  character at the end of the file.
	char *keyString = malloc(keySize + 1); // Allocates memory for the string taken from the file
	fread(keyString, keySize, 1, filePointer); // Get the string from the file
	fclose(filePointer);

	keyString[keySize] = 0; // Null terminate the string
	RemoveNewLineAndAddNullTerm(keyString);

	// Check if the key is shorter than the ciphertext
	CheckKeyLength(keySize, cipherTextSize, argv[2]);

	// Check if key or cipher text have any invalid characters
	ScanInvalidCharacters(cipherTextString, cipherTextSize);
	ScanInvalidCharacters(keyString, keySize);

	// Connect to server to send the plaintext and key. This method
	// 	also receives the server response containing the decrypted text
	ConnectToServer(argv[3], argv[1], argv[2]);

	// Free the strings
	free(cipherTextString);
	free(keyString);

	return 0;
}

/**************************************************************
 * * Entry:
 * *  N/a
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  This method does all the socket communication
 * *
 * ***************************************************************/
void ConnectToServer(char *portString, char *cipherTextFileName, char *keyFileName)
{
	int sockfd;
	struct sockaddr_in remote_addr;
	int portNumber = atoi(portString);
	char handshakeResponse[2];
	bzero(handshakeResponse, 2);

	// --------------- Section for connecting to the socket---------

	// Get the Socket file descriptor
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("Error: Failed to obtain socket descriptor.\n");
		exit(2);
	}

	// Fill the socket address struct
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(portNumber);
	inet_pton(AF_INET, "127.0.0.1", &remote_addr.sin_addr);
	bzero(&(remote_addr.sin_zero), 8);

	// Try to connect the remote
	if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("Error: could not contact otp_dec_d on port %s\n", portString);
		exit(2);
	}
	else
	{
		// printf("[otp_dec] Connected to server at port %d...ok!\n", portNumber); // For debugging
	}

	// Send initial handshake message to make sure the server exists and that
	//	we are trying to connect to the correct server
	SendHandshakeToServer(sockfd); // Send the combined file

	// Receives the server status. We will only send the plain text data if the server is
	//	willing to accept it.
	ReceiveServerHandshakeConfirm(sockfd, handshakeResponse);

	if (strcmp(handshakeResponse, "R") == 0)
	{
		// Server rejected this client because this client is unautherized to connect
		fprintf(stderr, "Error: could not contact otp_dec_d on port %s\n", portString);
		exit(1);
	}
	else if (strcmp(handshakeResponse, "S") == 0)
	{
		// Server connected successfully
		// printf("[otp_dec] Handshake successful!\n"); // For debugging
	}
	else if (strcmp(handshakeResponse, "T") == 0)
	{
		// Server is busy (too many processes running)
		printf("Error: Server rejected this client because it already has too many processes running\n");
		exit(1);
	}

	CheckIfFileEndingValid(cipherTextFileName);
	CheckIfFileEndingValid(keyFileName);

// Combine the key and ciphertext files to send to the server
	int resultTempFd = CombineTwoFiles(cipherTextFileName, keyFileName);
	// OutputTempFile(resultTempFd); // debug code, there is a problem with files that have no EOL.

	SendFileToServer(sockfd, resultTempFd); // Send the combined file

	// Receive result file from server
	ReceiveFileFromServer(sockfd);

	close (sockfd);
	// printf("[otp_dec] Connection lost.\n"); // For debugging
}

/**************************************************************
 * * Entry:
 * *  sockfd - socket to receive the handshake from.
 * *  handshakeResponse - holder for the response from the server.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Receives the handshake response from the server.
 * *
 * ***************************************************************/
void ReceiveServerHandshakeConfirm(int sockfd, char *handshakeResponse)
{
	char recvBuffer[2];
	bzero(recvBuffer, 2);

	// Wait for info that is sent from server
	recv(sockfd, recvBuffer, 1, 0);

	strncpy(handshakeResponse, recvBuffer, 1);
}

/**************************************************************
 * * Entry:
 * *  sockfd - the socket to send the handshake to.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the client's name to the server.
 * *
 * ***************************************************************/
void SendHandshakeToServer(int sockfd)
{
	char sendBuffer[LENGTH];
	bzero(sendBuffer, LENGTH);
	strncpy(sendBuffer, "otp_dec", LENGTH);

	int sendSize = 7;
	if (send(sockfd, sendBuffer, sendSize, 0) < 0)
	{
		printf("[otp_dec] Error: Failed to send initial handshake.\n");
	}
}

/**************************************************************
 * * Entry:
 * *  fileName - the file name to check.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Adds a new line to the end of the file if there are none.
 * *
 * ***************************************************************/
void CheckIfFileEndingValid(char *fileName)
{
	char readBuffer[LENGTH];
	int i;
	int foundNewLineChar = 0;
	FILE *filePointer = fopen(fileName, "rb+");

	// Read all the contents of the file checking for a new line character
	bzero(readBuffer, LENGTH);
	while (fread(readBuffer, sizeof(char), LENGTH, filePointer) > 0)
	{
		// Loop through the buffer to check for the correct characters
		for (i = 0; i < LENGTH; i++)
		{
			// Found the new line char. This file is valid. exit.
			if (readBuffer[i] == '\n')
			{
				foundNewLineChar = 1;
				break;
			}

			// Hit the end of the file.
			if (readBuffer[i] == '\0')
			{
				break;
			}
		}
		bzero(readBuffer, LENGTH);
	}

	if (!foundNewLineChar)
	{
		// New line character not found. Adding a new line character at the
		// 	end of the file. We are at the end because the previous fread
		//	call put the file pointer at the end of the file
		char newLineChar[1] = "\n";
		fwrite (newLineChar, sizeof(char), sizeof(newLineChar), filePointer);
	}

	fclose(filePointer);
}

/**************************************************************
 * * Entry:
 * *  sockfd - socket to receive the file from.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Receives the ciphertext file from the server and outputs it
 * *	to stdout.
 * *
 * ***************************************************************/
void ReceiveFileFromServer(int sockfd)
{
	char recvBuffer[LENGTH];
	bzero(recvBuffer, LENGTH);

	// Wait for info that is sent from server
	int receiveSize = 0;
	while ((receiveSize = recv(sockfd, recvBuffer, LENGTH, 0)) > 0)
	{
		// Output the decryption results
		printf("%s", recvBuffer);
		bzero(recvBuffer, LENGTH);

		// Exit out of receive loop if data chunk size is invalid
		if (receiveSize == 0 || receiveSize != 512)
		{
			break;
		}
	}
	if (receiveSize < 0)
	{
		if (errno == EAGAIN)
		{
			printf("[otp_dec] recv() timed out.\n");
		}
		else
		{
			printf("[otp_dec] recv() failed \n");
		}
	}
	// printf("Ok received from server!\n");
}

/**************************************************************
 * * Entry:
 * *  sockfd - socket to send file to.
 * *  tempFileDesc - the temp file descriptor.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Sends the temp file to the socket.
 * *
 * ***************************************************************/
void SendFileToServer(int sockfd, int tempFileDesc)
{
	char sendBuffer[LENGTH];
	bzero(sendBuffer, LENGTH);

	// printf("[otp_dec] Sending file to the Server... "); // For debugging only
	int sendSize;
	while ((sendSize = read(tempFileDesc, sendBuffer, sizeof(sendBuffer))) > 0)
	{
		if (send(sockfd, sendBuffer, sendSize, 0) < 0)
		{
			printf("[otp_dec] Error: Failed to send file.\n");
			break;
		}
		bzero(sendBuffer, LENGTH);
	}
	// printf("Ok File from Client was Sent!\n"); // For debugging only
}

/**************************************************************
 * * Entry:
 * *  fileOneName - the first file name.
 * *  fileTwoName - the second file name.
 * *
 * * Exit:
 * *  Returns a temp file descriptor
 * *
 * * Purpose:
 * * 	Combines the contents of two files into a single temp file.
 * *
 * ***************************************************************/
int CombineTwoFiles(char *fileOneName, char *fileTwoName)
{
	char readBuffer[LENGTH];
	int sizeRead = 0;
	FILE *fileOnePointer = fopen(fileOneName, "rb");
	FILE *fileTwoPointer = fopen(fileTwoName, "rb");
	int tempFD = GetTempFD();

	// Add the contents of the first file to the temp file.
	// Also added a semi-colon delimiter at the end.
	bzero(readBuffer, LENGTH);
	while ((sizeRead = fread(readBuffer, sizeof(char), LENGTH, fileOnePointer)) > 0)
	{
		BufRemoveNewLineAndAddSemiColon(readBuffer, LENGTH); // Removes new line and add null term if needed
		if (write(tempFD, readBuffer, sizeRead) == -1) // Write to the temp file
		{
			printf("[otp_dec] Error in combining ciphertext and key\n");
		}
		bzero(readBuffer, LENGTH);
	}

	// Add the contents of the second file to the temp file.
	// Also added a semi-colon delimiter at the end.
	bzero(readBuffer, LENGTH);
	while ((sizeRead = fread(readBuffer, sizeof(char), LENGTH, fileTwoPointer)) > 0)
	{
		BufRemoveNewLineAndAddSemiColon(readBuffer, LENGTH); // Removes new line and add null term if needed
		if (write(tempFD, readBuffer, sizeRead) == -1) // Write to the temp file
		{
			printf("[otp_dec] Error in combining ciphertext and key\n");
		}
		bzero(readBuffer, LENGTH);
	}

	// Reset the file pointer for the temp file
	if (-1 == lseek(tempFD, 0, SEEK_SET))
	{
		printf("File pointer reset for combined file failed\n");
	}

	// OutputTempFile(tempFD); // Used for debugging only
	fclose(fileOnePointer);
	fclose(fileTwoPointer);

	return tempFD;
}

/**************************************************************
 * * Entry:
 * *  msg - the message to print along with the error
 * *
 * * Exit:
 * *  N/a
 * *
 * * Purpose:
 * *  This is the wrapper for the error handler.
 * *
 * ***************************************************************/
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

/**************************************************************
 * * Entry:
 * *  stringValue - the string you want to transform
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * *  Removes the new line character from the string and adds a null
 * *  terminator in its place.
 * *
 * ***************************************************************/
void RemoveNewLineAndAddNullTerm(char *stringValue)
{
	size_t ln = strlen(stringValue) - 1;
	if (stringValue[ln] == '\n')
	{
		stringValue[ln] = '\0';
	}
}

/**************************************************************
 * * Entry:
 * *  keySize - the size of the key
 * *  ciphertextSize - the size of the cipher text
 * *  keyName - the key file name
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * *  Checks if key is shorter than the ciphertext. If it is, then
 * *  display the error message and exit.
 * *
 * ***************************************************************/
void CheckKeyLength(long keySize, long cipherTextSize, char *keyName)
{
	if (keySize < cipherTextSize)
	{
		printf("Error: key %s is too short\n", keyName);
		exit(1);
	}
}

/**************************************************************
 * * Entry:
 * *  stringValue - the string you want to validate
 * *  stringLength - the length of the string you want to validate
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * *  Checks if the string does not contain uppercase letters or
 * *  space. If it does not, then the program exits with an error.
 * *
 * ***************************************************************/
void ScanInvalidCharacters(char *stringValue, int stringLength)
{
	static const char possibleChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

	int i;
	for (i = 0; i < stringLength; i++)
	{
		// If there is an invalid character then exit the program
		if (strchr(possibleChars, stringValue[i]) == 0)
		{
			printf("otp_dec error: input contains bad characters\n");
			exit(1);
		}
	}
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

/**************************************************************
 * * Entry:
 * *  tempFileDesc - the temp file desc.
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Outputs the contents of the temp file
 * *
 * ***************************************************************/
void OutputTempFile(int tempFileDesc)
{
	// Do not need to close open file because the temp
	// file will auto delete at program exit.
	FILE *filePointer = fdopen(tempFileDesc, "rb");

	// Set file pointer to beginning of the file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}

	printf("test write:....\n");
	if (filePointer) {
		int c;
		while ((c = fgetc(filePointer)) != EOF)
		{
			// Print the current character if it is not the end of file
			putchar(c);
		}
	}

	// Set file pointer to beginning of the file
	if (fseek(filePointer, 0, SEEK_SET) == -1)
	{
		printf("Received file pointer reset failed\n");
	}
}

/**************************************************************
 * * Entry:
 * *  buffer -
 * *	bufferSize -
 * *
 * * Exit:
 * *  n/a
 * *
 * * Purpose:
 * * 	Removes the new line in the buffer (if it exists) and replaces
 * *	it with a semicolon.
 * *
 * ***************************************************************/
void BufRemoveNewLineAndAddSemiColon(char *buffer, int bufferSize)
{
	int i;
	for (i = 0; i < bufferSize; i++)
	{
		// Exit if we reached a null term
		if (buffer[i] == '\0')
		{
			return;
		}

		// Replace new line with semicolon
		if (buffer[i] == '\n')
		{
			buffer[i] = ';';
		}
	}
}