//Name:				Connie McClung
//Course:			CS372 Fall 2018
//Assignment:		Project 2
//Program Name:		ftserver.c
//Last modified:	11/26/2018
//Description:		program to run an ftp server that receives client requests on an control connection,
//					then opens a data transfer connection to transmit requested data.
//					Serves directory listing and text file transfer requests
//					Control connection runs until sigint received.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>//socket functions
#include <netinet/in.h>//sockaddr_in structure
#include <netdb.h> 
#include <arpa/inet.h>//in_addr structure
#include <sys/wait.h> //handle child processes with waitpid()
#include <signal.h>//handle sigint
#include <dirent.h> // for listing directory

#define MAXMSGSIZE 200001 //max bytes allowed in command msg and associated server error reply
#define MAXARGS 3
#define MAXFILESIZE 200001 //big to accomodate large files
#define BACKLOG 5

//function declarations
void *get_in_addr(struct sockaddr *socketAddress);
int sendAllBytes(int socketfd, char* msgbuffer);
int startUp(char *port);
int handleRequest(int newfd, char **cmdArgumentArray, char* filename, char* portNumber);
void processDirectoryListRequest(char* buffer);


//	Function name: catchSigInt()
//	Description: function to receive Ctrl+C sig, display exit message and shut down program
//				uses code from CS344 lectures and my assignments from that class
//	Precondition: sig handler registered in main
//  Postcondition: exit message displayed, program exits
//
void catchSigInt(int signo)
{
	char* notice = "SIGINT received. Exiting program.\n"; //message to display when sigint received
	printf("\n%s", notice);fflush(stdout);
	exit(0); //exit gracefully.
}

//	Function name: get_in_addr()
//	Description: function to get sockaddr, IPv4 or IPv6 from
//				https://beej.us/guide/bgnet/html/single/bgnet.html#ipaddrs2
//	Precondition:
//  Postcondition:
//
void *get_in_addr(struct sockaddr *socketAddress)
{
	if (socketAddress->sa_family == AF_INET)//if IPv4, return correct struct 
	{
		return &(((struct sockaddr_in*)socketAddress)->sin_addr);
	}
	//else return struct for IPv6
	return &(((struct sockaddr_in6*)socketAddress)->sin6_addr);
}


//	Function name: sendAllBytes()
//	Description: function to ensure all bytes in message buffer are written to socket
//		based on sendall() function code at https://beej.us/guide/bgnet/html/multi/advanced.html
//		takes socket fd and string message and loops send until all bytes are sent or error is returned
//	Precondition: server socket created and bound to port, client connection accepted, message buffer contains message
//  Postcondition: all bytes in message buffer are pushed into socket 
//	
int sendAllBytes(int socketfd, char* msgbuffer)
{
	int bytesSent;
	int totalBytesSent = 0;
	int bytesRemaining = strlen(msgbuffer);
	while (totalBytesSent < strlen(msgbuffer)) //loop until all all bytes in message have been pushed to socket
	{
		bytesSent = send(socketfd, msgbuffer + totalBytesSent, bytesRemaining, 0);
		if (bytesSent == -1) //if error in send, exit
		{
			perror("Server: send failure");
			exit(1);
		}
		totalBytesSent += bytesSent;
		bytesRemaining -= bytesSent;
	}
	if (bytesSent == -1)
	{
		return -1; //return -1 to calling function if send unsuccessful
	}
	else
	{
		return 0; //else return 0 for success
	}
}

//	Function name: startUp()
//	Description: function to resolve server ip address and bind specified port to server socket 
//				takes input port for either control or transfer connection, incidates by message which type
//				uses code and examples explained at https://beej.us/guide/bgnet/html/multi/clientserver.html
//	Precondition: char* port contains port number to connect on, type contains 'c' for control connection, 't' for transfer connection
//	Postcondition: address information for server machine obtained, socket created, port bound to socket, listening on port
//
//
int startUp(char *port) 
{
	int socketfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int returnValue;
	int yes = 1; //for setsockopt, modeled after beej code referenced above
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;//use current IP

	if ((returnValue = getaddrinfo(NULL,port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(returnValue));
		return 1;
	}
	
	//loop thorugh all results in servinfo and bind to first possible
	for (p = servinfo; p != NULL; p->ai_next)
	{
		if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("Server: socket creation error");
			continue;
		}
		//users setsockopt to prevent "address in use" message
		if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(1);
		}
		if (bind(socketfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(socketfd);
			perror("Server:bind");
			continue;
		}
		break;
	}
	freeaddrinfo(servinfo); //servinfo struct no longer needed
	if (p == NULL)
	{
		fprintf(stderr, "Server error: failed to bind \n");
		exit(1);
	}
	if (listen(socketfd, BACKLOG) == -1)
	{
		perror("Listen");
		exit(1);
	}
	return socketfd; //return created socket to calling function
}

//	Function name: startDataConnection()
//	Description: gets address info for hostname, creates socket and makes connection request to destination port
//				uses code and examples explained at https://beej.us/guide/bgnet/html/multi/clientserver.html
//
//	Precondition: port and hostname are known
//	Postcondition: socket created and connection made to destination host at supplied portnumber
//
//
int startDataConnection(char *port, char *host)
{
	int socketfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int returnValue;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int newconnectfd;

	//get client addrinfo
	if ((returnValue = getaddrinfo(host, port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "Data transfer socket getaddrinfo: %s\n", gai_strerror(returnValue));
		return 1;
	}
	//create socket
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((socketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("Data transfer socket creation error");
			continue;
		}
		if (connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) //if unsuccessful connect
		{
			close(socketfd);
			perror("Data transfer socket:connect error");
			continue;
		}
		break;
	}
	//error handling
	if (p == NULL)
	{
		fprintf(stderr, "Server error: failed to make data connection to client on port %s \n", port);
		return 2;
	}

	//printf("\nServer is now connected to client port %s for data transfer...\n", port);fflush(stdout);
	freeaddrinfo(servinfo);
	return socketfd;
}

//	Function name: processFileTransfer()
//	Description: this function receives a string buffer and filename, opens the file if available in current directory,
//				and copies file contents to buffer			
//				function uses concepts and code from CS344 lectures and code I wrote for an assignment in that class.
//	Precondition: buffer has been allocated, filename string is passed, file exists in current directory
//	Postcondition: file contents are copied to buffer and null-terminated, ready for sending across socket
//
void processFileTransfer(char *buffer, char *filename)
{
	//open requested text file and copy to buffer
	//printf("Filename: %s\n", filename);fflush(stdout);
	FILE *textfd = fopen(filename, "r");//open text file for reading
	
	memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize buffer befire use
	if (!textfd)//check for successful file open
	{
		memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
		strcpy(buffer, "FILE NOT FOUND"); //error message to send to client if file not found
		return;
	}

	//read file contents into buffer - uses code found at http://www.fundza.com/c4serious/fileIO_reading_all/
	fseek(textfd, 0L, SEEK_END); //find out how long the file is
	int fileLength = ftell(textfd);
	//printf("\nFile contains %d chars\n", fileLength);fflush(stdout);
	fseek(textfd, 0L, SEEK_SET); //reset to start of file
	memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
	//printf("Buffer contents before call to filetransfer: %s\n", buffer);fflush(stdout);
	fread(buffer, sizeof(char), fileLength, textfd); //read all chars in file into buffer

	buffer[fileLength + 1] = '\0'; //null-terminate
	//printf("\nFile contents: %s\n", buffer);fflush(stdout);
	fclose(textfd); //close file after contents are copied to buffer
	return;
}

//	Function name: processDirectoryListRequest()
//	Description: function to walk a directory structure and write all file names to a buffer 
//				based on lecture code from CS344 and concepts in https://www.geeksforgeeks.org/c-program-list-files-sub-directorie-directory/
//	Precondition: buffer has been allocated and passed to this function, current directory contains files
//	Postcondition: current working directory filenames are written to buffer 
//
void processDirectoryListRequest(char *buffer)
{
	memset(buffer, '\0', sizeof(MAXFILESIZE)); //clear buffer before writing to it
	
	//pointers to DIR object and dirent struct
	DIR* dirToList; 
	struct dirent *fileInDir; 

	//open directory, error if not able to open
	dirToList = opendir(".");	
	if (dirToList == NULL)
	{
		memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
		strcpy(buffer, "Server Says: Error - could not open directory!\n");
		exit(1);
	}
	//walk directory and add each file name to buffer
	while ((fileInDir = readdir(dirToList)) != NULL)
	{
		//skip current and parent directory markers
		if (!strcmp(fileInDir->d_name, ".")) 
		{
			continue;
		}
		if (!strcmp(fileInDir->d_name, ".."))
		{
			continue;
		}
		strcat(buffer, fileInDir->d_name); //add each filename to buffer
		strcat(buffer, "\n"); //put each filename on a newline
	}
	strcat(buffer, "\n"); //newline at end
	closedir(dirToList); //close DIR
	buffer[strlen(buffer) - 1] = '\0'; //null-terminate
	return;
}	


//	Function name: handleRequest()
//	Description:	function to receive client command on passed socket, parse command string into an array
//					and determine appropriate response.
//	Precondition: buffer has been allocated and passed to this function, current directory contains files
//	Postcondition: current working directory filenames are written to buffer 
//
int handleRequest(int newfd, char **cmdArgumentArray, char* filename, char* portNumber)
{
	int bytesReceived;
	int i;
	int command;
	int port;

	char clientMsg[MAXMSGSIZE];
	char serverReply[MAXMSGSIZE];

	memset(clientMsg, '\0', MAXMSGSIZE); //initalize
	memset(serverReply, '\0', MAXMSGSIZE); //initalize
	memset(filename, '\0', sizeof(filename));//initialize
	memset(portNumber, '\0', sizeof(portNumber));//initialize

	//receive command from client	
	int valid = 0;
	while (!valid){
		bytesReceived = recv(newfd, clientMsg, MAXMSGSIZE - 1, 0);
		if (bytesReceived == -1)
		{
			perror("receive error");
			exit(1); //should this be return 1; instead?
		}	
		clientMsg[bytesReceived] ='\0';//null terminate received message string
		//printf("Client sent this command: %s\n", clientMsg);
		
		//PARSE CLIENT MESSAGE
		//uses concepts from lectures and code from a project I did in CS344
		int argArraySize = 0; //update array size counter after each string is added, this will be the number of arguments
		char* cmdArgumentArray[MAXARGS];//there should not be more than 3 arguments passed 
		char* argEntry = strtok(clientMsg, " \n"); //get first string separated by delimiter
		while (argEntry)
		{
			cmdArgumentArray[argArraySize] = argEntry;
			//advance to next parsed string
			argEntry = strtok(NULL, " \n");
			argArraySize++;//increment array size
		}

		//depending on command, send back transfer port and command type (for -l, -g, or invalid)
		if (strcmp(cmdArgumentArray[0], "-l") == 0)
		{
			printf("\nList directory requested on port %s. \n", cmdArgumentArray[1]);fflush(stdout);
			strcpy(portNumber, cmdArgumentArray[1]);
			valid = 1;
			command = 1; //1 indicates command was -l
			return command;
		}
		else if (strcmp(cmdArgumentArray[0], "-g") == 0)
		{
			strcpy(filename, cmdArgumentArray[1]);
			printf("\nFile \"%s\" requested on port %s.\n", filename, cmdArgumentArray[2]);fflush(stdout);
			strcpy(portNumber, cmdArgumentArray[2]);
			valid = 1;
			command = 2;//2 indicates command was -g
			return command;
		}
		else
		{
			printf("\nClient sent invalid command.\n");fflush(stdout);
			command = -1; //-1 indicates client sent an invalid command
			return command;
		}
	}
	return command;
}



int main(int argc, char *argv[])
{
	int socketfd; // listen on fd
	int newfd; // new connection for accept
	char **cmdArgumentArray; // pointer to array of strings containing parsed client command
	char buffer[200001]; //buffer for sending and receiving messages and files
	char filename[512]; //string to get filename to transfer

	int bytesReceived;
	char transferPort[5]; //port
	int transfd;
	//int newtransfd;
	char startupType;
	char clientHostName[1024];
	char service[20];

	//signal handling setup
	// SIGINT 		
	struct sigaction actionSigInt = { 0 };//initialize struct
	actionSigInt.sa_handler = catchSigInt;
	sigfillset(&actionSigInt.sa_mask);
	sigaction(SIGINT, &actionSigInt, NULL);//register signal handler for SIGINT
	actionSigInt.sa_flags = 0; //not setting any flags

	memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
	memset(filename, '\0', sizeof(filename)); //initialize

	struct sockaddr_storage client_addr; //connecting client's address info
	socklen_t sin_size; //size of connecting client's address
	char s[INET6_ADDRSTRLEN]; //for inetop to show connecting client's address

	//VALIDATE COMMAND-LINE PARAMETERS
	//check for correct number of arguments
	if (argc != 2) { fprintf(stderr,"USAGE: %s <port#>\n", argv[0]); exit(0); } 
	if (atoi(argv[1]) < 1024 || atoi(argv[1]) > 65535)
	{
		fprintf(stderr, "Invalid control port\n"); exit(0);
	}
	while (1)
	{
		memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
		memset(filename, '\0', sizeof(filename)); //initialize
		//launch server on specified port and wait for connections
		socketfd = startUp(argv[1]); // 
		printf("\nServer control connection open on port %s\n", argv[1]);fflush(stdout);

		sin_size = sizeof client_addr; //
		//accept a client connection
		if ((newfd = accept(socketfd, (struct sockaddr *)&client_addr, &sin_size)) == -1)
		{
			perror("accept");
			exit(1);
		}

		inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
		getnameinfo((struct sockaddr *)&client_addr, sin_size, clientHostName, sizeof clientHostName, service, sizeof service, 0);
		printf("\nConnection from %s.\n", clientHostName);


		int command = handleRequest(newfd, cmdArgumentArray, filename, transferPort);
		//TEST FOR -L OR -G COMMAND 
		//IF -L
		if (command == 1) //client asked for directory listing
		{
			memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
			processDirectoryListRequest(buffer); //put directory list in buffer
			transfd = startDataConnection(transferPort, clientHostName); //connect to client's transfer port
			if (sendAllBytes(transfd, buffer) == -1)//transfer data
			{
				perror("Directory send");
			} 
			printf("\nSending directory contents to %s:%s\n", clientHostName, transferPort); fflush(stdout);
			memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
			close(transfd);//close data transfer connection
		}
		//IF -G
		else if (command == 2)
		{
			memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
			processFileTransfer(buffer, filename);
			transfd = startDataConnection(transferPort, clientHostName);
			if (strcmp(buffer, "FILE NOT FOUND") == 0)
			{
				memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
				strcpy(buffer, "FILE NOT FOUND\n");
				printf("\nFile not found. Sending error message to %s:%s\n", clientHostName, transferPort);
				if (sendAllBytes(transfd, buffer) == -1)
				{
					perror("File transfer message send");
				}
				memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
				close(transfd);//close data transfer connection
			}
			else
			{
				if (sendAllBytes(transfd, buffer) == -1)
				{
					perror("File transfer send");
				}
				printf("\nSending \"%s\" to %s:%s\n", filename, clientHostName, transferPort); fflush(stdout);
				memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
				close(transfd);//close data transfer connection
			}
		}
		else if (command == -1)
		{
			//send error message to client for invalid command
			memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
			strcpy(buffer, "Server Says: Error - invalid command. Terminating connection...\0");
			if (sendAllBytes(newfd, buffer) == -1)
			{
				perror("Invalid command error send");
			}
			printf("\nSending error message to client for invalid command\n");fflush(stdout);
			//invalid command error sent on original control connection
			memset(buffer, '\0', sizeof(MAXFILESIZE)); //initialize strings before use
		}
		close(newfd);
		close(socketfd);
	}

	exit(0);

}	
