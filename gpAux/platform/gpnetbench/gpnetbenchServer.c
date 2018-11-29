#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_APPLICATION_RECEIVE_BUF_SIZE 65536
char* receiveBuffer = NULL;

static void handleIncomingConnection(int fd);
static void usage(void);

static void
usage(void)
{
	printf("usage: gpnetbenchServer -p PORT\n");
}

int
main(int argc, char** argv)
{
	int socketFd;
	int clientFd;
	int retVal;
	int one = 1;
	struct sockaddr_in serverSocketAddress;
	socklen_t socket_length;
	int c;
	int serverPort = 0;
	int pid;
     
	while ((c = getopt (argc, argv, "hp:")) != -1)
	{
		switch (c)
		{
			case 'p':
				serverPort = atoi(optarg);
				break;
			default:
				usage();
				return 1;
		}
	}

	if (!serverPort)
	{
		fprintf(stderr, "-p port not specified\n");
		usage();
		return 1;
	}

	receiveBuffer = malloc(SERVER_APPLICATION_RECEIVE_BUF_SIZE);
	if (!receiveBuffer)
	{
		fprintf(stderr, "failed allocating memory for application receive buffer\n");
		return 1;
	}

	socketFd = socket(PF_INET, SOCK_STREAM, 0); 

	if (socketFd < 0)
	{ 
		perror("Socket creation failed");
		return 1;
	}   

	retVal = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (retVal)
	{
		perror("Could not set SO_REUSEADDR on socket");
		return 1;
	}

	memset(&serverSocketAddress, 0, sizeof(struct sockaddr_in));
	serverSocketAddress.sin_family = AF_INET;
	serverSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverSocketAddress.sin_port = htons(serverPort);

	retVal = bind(socketFd,(struct sockaddr *)&serverSocketAddress, sizeof(serverSocketAddress));
	if (retVal)
	{
		perror("Could not bind port");
		return 1;
	}

	retVal = listen(socketFd, SOMAXCONN);
  	if (retVal < 0)
	{
		perror("listen system call failed");
  		return 1;
  	}

	pid = fork();
	if (pid < 0) 
	{ 
		perror("error forking process for incoming connection");
		return 1;
	}
	if (pid > 0)
	{ 
		return 0; // we exit the parent cleanly and leave the child process open as a listening server
	}

	socket_length = sizeof(serverSocketAddress);
	while(1)
	{
		clientFd = accept(socketFd, (struct sockaddr *)&serverSocketAddress, &socket_length);
		if (clientFd < 0)
		{
			perror("error from accept call on server");
			return 1;
		}

		pid = fork();
		if (pid < 0) 
		{ 
			perror("error forking process for incoming connection");
			return 1;
		}
		if (pid == 0)
		{ 
			handleIncomingConnection(clientFd);
		}
	}

	return 0;
}

static void
handleIncomingConnection(int fd)
{
	ssize_t bytes;

	while (1)
	{
		bytes = recv(fd, receiveBuffer, SERVER_APPLICATION_RECEIVE_BUF_SIZE, 0);

		if (bytes <= 0)
		{
			// error from rev, assuming client disconnection
			// this is the end of the child process used for handling 1 client connection
			exit(0);
		}
	}
}
