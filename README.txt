
CREATED BY: Yahya Saad
EX4 – Event-Driven Chat Server
==Files==
chatServer.c
This file implement the behavior in an entirely event-driven manner, without the use of threads. The server will use the "select" function to check which socket descriptor is ready for reading or writing. The main socket, which receives new connections, is distinguished from other sockets that negotiate data with clients. 
README.txt 
This file; more info about the chat server and how does it work.
==How to Compile this program==
gcc chatServer.c -o chatServer
server is the executable program.
==How to run this program==
./chatServer <port>
==General information== 
-If there is no port parameter, or more than one parameter, or the port is not a number between 1 to 2^16, print the usage message.
-The usage of select() : The server will use the "select" function to check which socket descriptor is ready for reading or writing. The main socket, which receives new connections, is distinguished from other sockets that negotiate data with clients

-Opening more than 2 termenals to check the chat server by more than one user, all data implement by double linked list: add connection, remove connection and write to client functions 
==Exiting status== 
If the server ends in CTRL-C, the program catch the SIG_INT signal, clean everything and exit.

