#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>

#include <dlfcn.h>

/* Probability of each connect() or send() call to be disrupted */
static double connect_fail_prob = 0.01;
static double send_fail_prob = 0.01;

/* from FLAKY_PORT environment variable */
static int flaky_port = -1;

/* currently open connection for which we might cause send()s to fail */
static int flaky_fd = -1;

static int (*connect_real)(int, const  struct sockaddr*, socklen_t) = NULL;
static ssize_t (*send_real)(int sockfd, const void *buf, size_t len, int flags) = NULL;

static void reset_socket(int fd);

/*
 * Wrapper for connect() library function. Checks if the connection is being
 * attempted to the port specified by FLAKY_PORT environment variable, and
 * if so, refuses it with some probability.
 */
int
connect(int sockfd,  const  struct sockaddr *serv_addr, socklen_t addrlen)
{
	const unsigned char *c;
	int			port;
	int			is_flaky_port;
	int			retval;

	if (flaky_port == -1)
	{
		char *s = getenv("FLAKY_PORT");

		if (s == NULL)
		{
			fprintf(stderr, "FLAKY_PORT not set\n");
			flaky_port = 0;
		}
		else
		{
			flaky_port = atoi(s);
			fprintf(stderr, "FLAKY_PORT is %d\n", flaky_port);
		}
	}

	if (!connect_real)
		connect_real = dlsym(RTLD_NEXT,"connect");

	if (serv_addr->sa_family==AF_INET)
	{
		c = (unsigned char *) serv_addr->sa_data;
		port = 256*c[0]+c[1];

		is_flaky_port = port == flaky_port;
	}
	else
		is_flaky_port = 0;

	/* If the port matches, fail the call with given probability */
	if (is_flaky_port)
	{
		if (random() < connect_fail_prob * RAND_MAX)
		{
			fprintf(stderr,"FLAKY: connect() denied to port %d\n", port);

			shutdown(flaky_fd, SHUT_RDWR);
			
			return ECONNREFUSED;
		}
		else
		{
			fprintf(stderr,"FLAKY: connect() allowed to port %d\n", port);
		}
	}

	retval = connect_real(sockfd,serv_addr,addrlen);

	if (is_flaky_port)
		flaky_fd = sockfd;
	return retval;
}

/*
 * Wrapper for send() library function. Returns ECONNRESET and shuts down
 * the connection, with some probability.
 */
ssize_t
send(int sockfd, const void *buf, size_t len, int flags)
{
	if (!send_real)
		send_real = dlsym(RTLD_NEXT,"send");

	if (sockfd == flaky_fd)
	{
		if (random() < send_fail_prob * RAND_MAX)
		{
			fprintf(stderr,"FLAKY: made send() of %d bytes to fail\n", (int) len);
			
			reset_socket(sockfd);
			
			return ECONNRESET;
		}
		else
		{
			fprintf(stderr,"FLAKY: send() of %d bytes called\n", (int) len);
		}
	}

	return send_real(sockfd, buf, len, flags);
}

/*
 * Close a socket, and replace the fd with an unconnected socket.
 *
 * We use this to forcibly disconnect a connection on simulated send()
 * failure. Simply calling close() on the socket is not nice, as the caller
 * might react badly to EBADF. Also, if someone opens a new file or socket,
 * it might be assigned the same fd, confusing things further.
 *
 * shutdown() also doesn't quite work. poll() on a socket that we've shut
 * down doesn't return POLLERR, so the caller might hang, waiting for it
 * to become writeable.
 */
static void
reset_socket(int fd)
{
	int			newfd;

	newfd = socket(AF_INET, SOCK_STREAM, 0);
	dup2(newfd, fd);
}
