#Connie McClung 
#CS372 Fall 2018
#Project 2
#README.md

Simple FTP server in C with Python client to request and receive directory listing and file transmission; handles duplicate filenames and additional client connections


To compile ftserver.c, type:

gcc -o ftserver ftserver.c


To run compiled ftserver program, type:

./ftserver  <server-port-number>


To run ftclient.py, type:

python3 ./ftclient.py <server-host-name> <server-port-number> <command> <filename(if command == -g)> <data-port-number>
		