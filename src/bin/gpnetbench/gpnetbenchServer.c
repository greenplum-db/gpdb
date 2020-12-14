#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define SERVER_APPLICATION_RECEIVE_BUF_SIZE 65536
char* receiveBuffer = NULL;

static void handleIncomingConnection(int fd);
static void usage(void);

static void
usage(void)
{
	printf("usage: gpnetbenchServer [-6] -p PORT\n");
}

int
main(int argc, char** argv)
{
	int socketFd;
	int clientFd;
	int retVal;
	int one = 1;
	bool ipv6 = false;
	struct sockaddr_in v4Addresses;
	struct sockaddr_in6 v6Addresses;
	socklen_t socket_length;
	int c;
	int serverPort = 0;
	int pid;

	while ((c = getopt (argc, argv, "6hp:")) != -1)
	{
		switch (c)
		{
			case 'p':
				serverPort = atoi(optarg);
				break;
			case '6':
				ipv6 = true;
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

	if (ipv6)
	{
		socketFd = socket(PF_INET6, SOCK_STREAM, 0);
    }
	else
	{
		socketFd = socket(PF_INET, SOCK_STREAM, 0);
	}

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

	if (ipv6)
	{
		memset(&v6Addresses, 0, sizeof(struct sockaddr_in6));
		v6Addresses.sin6_family = AF_INET6;
		v6Addresses.sin6_addr = in6addr_any;
		v6Addresses.sin6_port = htons(serverPort);
	}
	else
	{
		memset(&v4Addresses, 0, sizeof(struct sockaddr_in));
		v4Addresses.sin_family = AF_INET;
		v4Addresses.sin_addr.s_addr = htonl(INADDR_ANY);
		v4Addresses.sin_port = htons(serverPort);
	}

	if (ipv6)
	{
		retVal = bind(socketFd, (struct sockaddr *)&v6Addresses, sizeof(v6Addresses));
	}
	else
	{
		retVal = bind(socketFd, (struct sockaddr *)&v4Addresses, sizeof(v4Addresses));
	}

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

	socket_length = sizeof(v4Addresses);
	while(1)
	{
		clientFd = accept(socketFd, (struct sockaddr *)&v4Addresses, &socket_length);
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
