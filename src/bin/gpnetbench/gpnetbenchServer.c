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
static int setupListen(char hostname[], char port[], int protocol);

static void
usage(void)
{
	printf("usage: gpnetbenchServer -p PORT\n");
}

int
main(int argc, char** argv)
{
	int c, rc;
	char* serverPort = "0";

	while ((c = getopt (argc, argv, "hp:")) != -1)
	{
		switch (c)
		{
			case 'p':
				serverPort = optarg;
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

	rc = setupListen("::0", serverPort, AF_INET6);
	if (rc != 0)
		rc = setupListen("0.0.0.0", serverPort, AF_INET);

	return rc;
}

static int setupListen(char hostname[], char port[], int protocol) {
	struct addrinfo hints;
	struct addrinfo *addrs;
	int s, clientFd, clientPid;
	int one = 1;
	int fd = -1;
	int pid = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = protocol;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* Two-way, out of band connection */
	hints.ai_protocol = IPPROTO_TCP;		/* Any protocol - TCP implied for network use due to SOCK_STREAM */

	s = getaddrinfo((char *)hostname, port, &hints, &addrs);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo says %s", gai_strerror(s));
		return 1;
	}

	while (addrs != NULL)
	{
		fd = socket(addrs->ai_family, SOCK_STREAM, 0);
		if (fd < 0)
		{
			fprintf(stderr, "socket creation failed\n");
			addrs = addrs->ai_next;
			continue;
		}

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) < 0)
		{
			fprintf(stderr, "could not set SO_REUSEADDR\n");
			close(fd);
			addrs = addrs->ai_next;
			continue;
		}

		if (bind(fd, addrs->ai_addr, addrs->ai_addrlen) != -1 &&
				listen(fd, SOMAXCONN) != -1)
		{
			pid = fork();
			if (pid > 0)
			{
				return 0; // we exit the parent cleanly and leave the child process open as a listening server
			}
			else if (pid == 0)
			{
				break;
			}
			else
			{
				fprintf(stderr, "failed to fork process");
				exit(1);
			}
		}
		else
			close(fd);

		addrs = addrs->ai_next;
	}

	if (pid < 0)
	{
		fprintf(stderr, "failed to listen on port");
		return 1;
	}

	struct sockaddr_storage clientAddr;
	socklen_t clientAddrlen = sizeof(clientAddr);
	while(1)
	{
		clientFd = accept(fd, (struct sockaddr *)&clientAddr, &clientAddrlen);
		if (clientFd < 0)
		{
			perror("error from accept call on server");
			exit(1);
		}

		clientPid = fork();
		if (clientPid < 0)
		{
			perror("error forking process for incoming connection");
			exit(1);
		}

		if (clientPid == 0)
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
