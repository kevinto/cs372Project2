CS372 Project 2
Coded By: Kevin To

Prerequisites:
- This code is tested on the flip server, so make sure you run it there.
- Python, the C compiler, c make utility are required. (Available on flip)
- The client and server are tested to run on the same flip server.
- This ftp program only supports sending unix files. This means it only supports sending files that were made in a linux host. Linux files have the correct file end markers which this FTP program recognizes.
- If you want to run the server code in a different directory than the client code, move the server executable to a different directory.

Warning:
- If you get a port binding error at any point, this means that either the data port or the command port is unavailable. Please choose different port(s) and try again.

How to compile the server code:
1. Using the terminal, go to the folder containing the server code. Ensure the make file is present also.
2. Type: 
		make 

How to server executable:
1. Open two terminal windows and SSH into the same flip server. For example, flip1.engr.oregonstate.edu on port 22.
2. In both terminal windows, navigate to the folder containing the server/client code.
3. In the first terminal window, run the server code by typing (make sure you compiled the server code):
	ftserver <command port#>

Available client FTP commands:
- How to get server directory list:
	python ftclient.py <Host Name> <command port#> -l <data port#>
- How to get a file from the server:
	python ftclient.py <Host Name> <command port#> -g <file name> <data port#>
	
Notes:
- <command port#> means command port number. The command port is in charge of passing FTP commands to the server. The valid ranges are 1024 - 65535. I typically choose something like 33555.
- <data port#> means data port number. The data port is in charge of passing file data to the client. The valid ranges are 1024 - 65535. I typically choose something like 33557.
- You can use either "localhost" or the host name like flip2.engr.oregonstate.edu for the <Host Name> field.
- All references to other sources I used as information are in the source code itself. They are in the function headers.

Quick Start:
- Server (in terminal window): 
    ftserver 33555
- Client (in terminal window):
	python ftclient.py localhost 33555 -l 33557