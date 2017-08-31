/*-------------------------------------------------------------------------
 *
 * simexsys.c
 * 		Implementation of system call wrappers with integrated ES injection
 *
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/simexsys.c
 *
 *-------------------------------------------------------------------------
 */

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/poll.h>

#include "postgres.h"
#include "utils/simex.h"

#define ERR_RETURN_VAL   -1         /* returned value when simulating system call error */
#define NET_ERRNO        ENOBUFS    /* errno value describing injected error */


/* static functions */
#ifdef USE_TEST_UTILS
static bool injectErrorNet(void);
#endif /* USE_TEST_UTILS */


/*
 * Wrapper for socket()
 */
int
gp_socket(int socket_family, int socket_type, int protocol)
{
#ifdef USE_TEST_UTILS
	if (injectErrorNet())
	{
		return ERR_RETURN_VAL;
	}
#endif /* USE_TEST_UTILS */

	return socket(socket_family, socket_type, protocol);
}


/*
 * Wrapper for connect()
 */
int
gp_connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
#ifdef USE_TEST_UTILS
	if (injectErrorNet())
	{
		return ERR_RETURN_VAL;
	}
#endif /* USE_TEST_UTILS */

	return connect(socket, address, address_len);
}


/*
 * Wrapper for poll()
 */
int
gp_poll(struct pollfd *fds, uint32 nfds, int timeout)
{
#ifdef USE_TEST_UTILS
	if (injectErrorNet())
	{
		return ERR_RETURN_VAL;
	}
#endif /* USE_TEST_UTILS */

	return poll(fds, nfds, timeout);
}


/*
 * Wrapper for send()
 */
int
gp_send(int socket, const void *buffer, size_t length, int flags)
{
#ifdef USE_TEST_UTILS
	if (injectErrorNet())
	{
		return ERR_RETURN_VAL;
	}
#endif /* USE_TEST_UTILS */

	return send(socket, buffer, length, flags);
}


/*
 * Wrapper for recv()
 */
int
gp_recv(int socket, void *buffer, size_t length, int flags)
{
#ifdef USE_TEST_UTILS
	if (injectErrorNet())
	{
		return ERR_RETURN_VAL;
	}
#endif /* USE_TEST_UTILS */

	return recv(socket, buffer, length, flags);
}


#ifdef USE_TEST_UTILS
/*
 * Check for simulation of network error
 */
static bool
injectErrorNet()
{
	if (gp_simex_init &&
		gp_simex_run &&
		gp_simex_class == SimExESClass_SysError)
	{
		SimExESSubClass subclass = SimEx_CheckInject();
		if (subclass == SimExESSubClass_SysError_Net)
		{
			errno = NET_ERRNO;
			return true;
		}
	}
	return false;
}
#endif /* USE_TEST_UTILS */


/* EOF */
