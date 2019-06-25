#!/usr/bin/env python3 
#Name:			Connie McClung 
#Course:		CS372 Fall 2018
#Assignment:	Project 2
#Program:		ftclient.py
#Last modified: 11/26/2018
#Description 	This program instantiates an ftp client that connects to server at specified server port and 
#				accepts connection at specified data tranfer port 

#import modules
import socket
import sys
import os #to check if a file already exists in current directory

MAXSIZE = 200001

# sendAllBytes()
# send function to ensure all data is sent to socket by looping until bytes sent = string length
# using concepts explained at https://docs.python.org/3/howto/sockets.html#socket-howto
# Precondition: socket is created and connected, message string exists
# Postcondition: all bytes in message string are pushed to socket
def sendAllBytes(clientMessage, socket):
	# ensure all data is sent to socket
	# using concepts explained at https://docs.python.org/3/howto/sockets.html#socket-howto
	totalBytesSent = 0
	bytesLeft = len(clientMessage.encode())
	while totalBytesSent < len(clientMessage.encode()):
		bytesSent = socket.send(clientMessage.encode()[totalBytesSent:])
		if bytesSent == 0:
			raise RuntimeError("Socket connection broken")
		totalBytesSent += bytesSent
		bytesLeft -= bytesSent
		
# receiveAllBytes()
# function to ensure all data is received from socket by looping 
# I saw this method described at https://stackoverflow.com/questions/17667903/python-socket-receive-large-amount-of-data
# Precondition: socket is created and connected, host at end of connection has sent data
# Postcondition: all bytes sent via socket are received and written to data
def receiveAllBytes(connection):
	data = ''
	partialMsg = None
	while partialMsg != '':
		partialMsg = connection.recv(4096).decode() #recommended receive buffer size from python.org
		data += partialMsg
	return data

# initiateContact()
# function to create socket and connect to server at host and port passed in as parameters
# uses socket concepts from https://docs.python.org/3/library/socket.html
# Precondition: host and port are provided, socket module imported
# Postcondition: socket is created and connected to host at port number
def initiateContact(host, port):
	ftClient =  socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
	ftClient.connect((host, port))
	return ftClient
	
# makeRequest()
# function to send client command to server
# uses socket concepts from https://docs.python.org/3/library/socket.html
# Precondition: command message has been created, socket is created and connected, ready for send
# Postcondition:  message is transmitted to host
def makeRequest(clientMessage, ftClient):
	sendAllBytes(clientMessage, ftClient)
	#print("sent request to client")


# receiveData()
# function to accept server connection on specified port
# and receive directory listing or text file transfer
# if filename exists in current directory, file is not transfered.
# uses socket concepts from https://docs.python.org/3/library/socket.html
# Precondition: host, port,command, and filename (if file transfer) are known
# Postcondition: directory listing is printed to screen, file is written to current directory if not duplicate
def receiveData(transferPort, host, port, command, filename):
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as ftClientTransfer:
		ftClientTransfer.bind(('',transferPort))
		#print("bound\n")
		ftClientTransfer.listen(1)
		#print("listening\n")
		dataConnection, otherAddress = ftClientTransfer.accept()
		#print("accepted\n")
		dataString = receiveAllBytes(dataConnection)
		if (command == "-l"):
			print("\nReceiving directory structure from " + host + ":" + str(port) + "\n")
			if not dataString:
				print("\nError in data\n")
				sys.exit(1)
			print(dataString)
		elif (command == "-g"):
		#uses code from 
			exists = os.path.isfile(filename)
			if exists:	
				print("\n" + filename + " already exists in directory! File not saved.\n\nGoodbye!\n")
				sys.exit(1)
			elif ("FILE NOT FOUND" in dataString):
				print("\n"+host + ":" + str(port) + " says FILE NOT FOUND\n")
			else:
				print("\nReceiving " + filename + " from " + host +":" + str(port) + "\n")
				with open(filename, "w+") as outputFile: 
						outputFile.write(dataString)
				print("\nFile transfer complete\n")
		ftClientTransfer.close()

				
def main():
	#validate commandline args and assign to variables
	if len(sys.argv) < 5:
		print("\nUsage: "+ sys.argv[0] + " servername"+ " server_portnumber" + " command" + " filename(if command -g)" + " transfer_port\n");
		sys.exit(1)
	

	#assign args to variables for passing to functions.	
	host = sys.argv[1]
	port = int(sys.argv[2])
	command = sys.argv[3]
	transferPort = int(sys.argv[-1])
	
	#validate port numbers  
	if (port < 1024 or port > 65535):
		print("\nError: invalid server port number\n")
		sys.exit(1)
	if (transferPort <1024 or transferPort > 65535):
		print("\nError: invalid transfer port number\n")
		sys.exit(1)
	
	# validate that command is either -l or -g
	# server also checks for invalid command, but it's quicker to check for it here without sending across TCP
	if (command != "-l") and (command != "-g"):
		print("\nError - invalid command. Terminating program...\n")
		sys.exit(1)
	# if command is valid, command string is constructed to send to server
	if len(sys.argv) == 5:
		clientCommand = sys.argv[3] + " " +sys.argv[4]
		filename = ''
	elif len(sys.argv) == 6:
		clientCommand = sys.argv[3] + " " + sys.argv[4] + " " + sys.argv[5]
		filename = sys.argv[4]
	#print("Command to send to server: " + clientCommand)
	
	
	ftClient = initiateContact(host,port) # make control connection
	makeRequest(clientCommand, ftClient)  # send command to server
	receiveData(transferPort, host, port, command, filename) # receive server data and do client-side processing
	

		
	print("Goodbye!\n")
				
	sys.exit(0)

	
# call the main() function to begin the program
if __name__ == '__main__':
    main()


		
