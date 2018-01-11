/*-------------------------------------------------------------------------
 *
 * netcheck.c
 *	  Implementation of network checking utilities
 *
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/netcheck.c
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "postgres.h"
#include "utils/netcheck.h"
#include "utils/timestamp.h"

#if defined(__darwin__) || defined(sun)
#include <sys/sockio.h>
#endif /* defined(__darwin__) || defined(sun) */

/*
 * ifaddrs.h is not present in Solaris;
 * need to provide partial implementation
 */
#if defined(__darwin__) || defined(__linux__)
#define HAVE_IFADDRS	1
#endif /* defined(__darwin__) || defined(linux) */

#ifdef HAVE_IFADDRS
#include <ifaddrs.h>
#endif /* HAVE_IFADDRS */


/*
 * CONSTANTS
 */

#define NET_RETRIES             (5)
#define IF_CONFIG_BUFFER_SIZE   (32 * 1024)


/*
 * STRUCTS
 */

#ifndef HAVE_IFADDRS

struct ifaddrs
{
	struct ifaddrs *ifa_next;
	char ifa_name[32];
	uint64_t ifa_flags;
	struct sockaddr	*ifa_addr;
	struct sockaddr	*ifa_netmask;
	union
	{
		struct sockaddr ifa_addr_alloc;
		struct sockaddr_in ifa_addr_v4;
		struct sockaddr_in6 ifa_addr_v6;
	};
	union
	{
		struct sockaddr ifa_netmask_alloc;
		struct sockaddr_in ifa_netmask_v4;
		struct sockaddr_in6 ifa_netmask_v6;
	};
};

#endif /* HAVE_IFADDRS */


/*
 * STATIC VARIABLES
 */

/* flag indicating if NIC is running */
static bool running = false;

/* host name to be matched to NIC */
static const char *hostTarget = NULL;

/* IPv4 and IPv6 addresses for host */
static struct sockaddr_in hostAddrV4;
static struct sockaddr_in6 hostAddrV6;

/* list of interface address descriptors */
static struct ifaddrs *ifAddresses;


#ifndef HAVE_IFADDRS

/* socket used for retrieving NIC state */
static int sock = -1;

/* interface configuration */
static struct ifconf ifconfig;

/* buffers used for storing configuration */
static char ifBuffer[IF_CONFIG_BUFFER_SIZE];
static char ifaBuffer[IF_CONFIG_BUFFER_SIZE];

/* iterator for interface address descriptors */
static struct ifaddrs *ifAddrIter;

#endif /* HAVE_IFADDRS */


/*
 * FUNCTION PROTOTYPES
 */

static void Init(const char *hostname);
static bool GetHost(void);
static bool GetIfAddresses(void);
static bool CompareIP(char *ipaddr1, char *ipaddr2, char *mask, int length);
static bool CheckRunning(void);
static void FindMatchingIf(void);
static void Release(void);


#ifndef HAVE_IFADDRS

/* provide functions from ifaddrs.h */
static int getifaddrs(struct ifaddrs **ifAddrsPtr);
static void freeifaddrs(struct ifaddrs *ifAddrStruct);

static bool OpenSocket(bool useIPv6);
static bool CloseSocket(void);
static bool GetIFConfig(void);
static bool RetrieveIfInfo(bool useIPv6);
static bool CloseSocket();

#endif /* HAVE_IFADDRS */


/*
 * check if the NIC used for routing to the given host is running
 */
bool
NetCheckNIC(const char *hostname)
{
	/* open a socket */
	for (int i = 0; i < NET_RETRIES; i++)
	{
		Init(hostname);

		if (GetHost() && GetIfAddresses())
		{
			FindMatchingIf();
			Release();

			return CheckRunning();
		}

		elog(LOG, "Retry %d to check NIC status.", i + 1);
		Release();

		/* sleep for 1 second to avoid tight loops */
		pg_usleep(USECS_PER_SEC);
	}

	elog(LOG, "Failed to check NIC status after trying %d times.", NET_RETRIES);

	return false;
}


/*
 * Initialize static variables.
 */
static void
Init(const char *hostname)
{
	hostTarget = hostname;
	running = false;

	ifAddresses = NULL;
}


/*
 * Retrieve IP address for the target host name.
 */
static bool
GetHost()
{
	struct addrinfo *hostAddresses = NULL;
	struct addrinfo *hostAddrIter = NULL;
	struct addrinfo hints;

	MemSet(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	bool foundV4 = false;
	bool foundV6 = false;

	int ret = getaddrinfo(hostTarget, NULL /*service*/, &hints, &hostAddresses);

	if (0 != ret)
	{
		elog(LOG, "Failed to lookup host name to retrieve NIC configuration: %s",
		     gai_strerror(ret));

		return false;
	}

	for (hostAddrIter = hostAddresses; hostAddrIter != NULL; hostAddrIter = hostAddrIter->ai_next)
	{
		if (AF_INET == hostAddrIter->ai_family)
		{
			Assert(!foundV4 && "expected no more than one IPv4 address for host");
			memcpy(&hostAddrV4, hostAddrIter->ai_addr, sizeof(hostAddrV4));
			foundV4 = true;
		}
		else if (AF_INET6 == hostAddrIter->ai_family)
		{
			Assert(!foundV6 && "expected no more than one IPv6 address for host");
			memcpy(&hostAddrV6, hostAddrIter->ai_addr, sizeof(hostAddrV6));
			foundV6 = true;
		}
	}

	/* release allocated resources */
	freeaddrinfo(hostAddresses);

	return true;
}


/*
 * Retrieve NIC configuration.
 */
static bool
GetIfAddresses()
{
	int ret = getifaddrs(&ifAddresses);

	if (0 != ret || NULL == ifAddresses)
	{
		elog(LOG, "Failed to retrieve NIC configuration.");
		return false;
	}

	return true;
}


/*
 * Iterate over interfaces to find the one used for routing to host address.
 */
static void
FindMatchingIf()
{
	Assert(NULL != ifAddresses);

	struct ifaddrs *ifa = NULL;
	bool found = false;

	/* iterate NICs */
	for (ifa = ifAddresses; ifa != NULL; ifa = ifa->ifa_next)
	{
		/* check if the examined interface is a NIC */
		if ((ifa->ifa_flags &
			 (IFF_UP | IFF_BROADCAST | IFF_POINTOPOINT | IFF_LOOPBACK | IFF_NOARP))
			 != (IFF_UP | IFF_BROADCAST))
		{
			continue;
		}

		/* check if the examined interface supports IPv4 or IPv6 */
		if ((AF_INET != ifa ->ifa_addr->sa_family &&
		     AF_INET6 != ifa ->ifa_addr->sa_family) ||
			ifa->ifa_netmask == NULL)
		{
			continue;
		}

		if (ifa ->ifa_addr->sa_family == AF_INET)
		{
			char nicAddr[sizeof(hostAddrV4)];
			char maskAddr[sizeof(hostAddrV4)];
			(void) memcpy(nicAddr, ifa->ifa_addr, sizeof(nicAddr));
			(void) memcpy(maskAddr, ifa->ifa_netmask, sizeof(maskAddr));

			found = CompareIP((char*) &hostAddrV4, nicAddr, maskAddr, sizeof(hostAddrV4));
		}
		else
		{
			Assert(ifa->ifa_addr->sa_family == AF_INET6);

			char nicAddr[sizeof(hostAddrV6)];
			char maskAddr[sizeof(hostAddrV6)];
			(void) memcpy(nicAddr, ifa->ifa_addr, sizeof(nicAddr));
			(void) memcpy(maskAddr, ifa->ifa_netmask, sizeof(maskAddr));

			found = CompareIP((char*) &hostAddrV6, nicAddr, maskAddr, sizeof(hostAddrV6));
		}

		if (found)
		{
			running = (ifa->ifa_flags & IFF_RUNNING) == IFF_RUNNING;
			return;
		}
	}

	Assert(!running);
	elog(LOG, "No NIC matches host name %s.", hostTarget);
}

/*
 * Compare IP addresses under mask.
 */
static bool
CompareIP(char *ipaddr1, char *ipaddr2, char *mask, int length)
{
	int i = 0;
	for (i = 0; i < length; i++)
	{
		if ((ipaddr1[i] & mask[i]) != (ipaddr2[i] & mask[i]))
		{
			return false;
		}
	}

	return true;
}

/*
 * Check if NIC is running.
 */
static bool
CheckRunning()
{
	return running;
}


/*
 * Release allocated resources.
 */
static void
Release()
{
	if (NULL != ifAddresses)
	{
		freeifaddrs(ifAddresses);
		ifAddresses = NULL;
	}
}


#ifndef HAVE_IFADDRS

/*
 * Build list of interface address descriptors
 */
int getifaddrs(struct ifaddrs **ifAddrsPtr)
{
	MemSet(ifaBuffer, 0, sizeof(ifaBuffer));
	ifAddrIter = (struct ifaddrs*) ifaBuffer;
	*ifAddrsPtr = NULL;

	if (OpenSocket(false /*useIPv6*/) &&
	    GetIFConfig() &&
	    RetrieveIfInfo(false /*useIPv6*/) &&
	    CloseSocket() &&

	    OpenSocket(true /*useIPv6*/) &&
	    GetIFConfig() &&
	    RetrieveIfInfo(true /*useIPv6*/) &&
	    CloseSocket())
	{
		*ifAddrsPtr = (struct ifaddrs*) ifaBuffer;

		return 0;
	}

	CloseSocket();

	return -1;
}


/*
 * Release list of interface address descriptors;
 * no-op since we use statically allocated buffers
 */
static void
freeifaddrs(struct ifaddrs *ifAddresses)
{}


/*
 * Open socket.
 */
static bool
OpenSocket(bool useIPv6)
{
	if (useIPv6)
	{
		sock = gp_socket(AF_INET6, SOCK_DGRAM, 0 /* protocol */);
	}
	else
	{
		sock = gp_socket(AF_INET, SOCK_DGRAM, 0 /* protocol */);
	}

	if (-1 == sock)
	{
		elog(LOG, "Failed to open socket to retrieve NIC configuration.");
		return false;
	}

	return true;
}


/*
 * Close socket.
 */
static bool
CloseSocket()
{
	(void) close(sock);

	sock = -1;

	return true;
}


/*
 * Retrieve interface configuration.
 */
static bool
GetIFConfig()
{
	MemSet(ifBuffer, 0, sizeof(ifBuffer));
	MemSet(&ifconfig, 0, sizeof(ifconfig));
	ifconfig.ifc_buf = ifBuffer;
	ifconfig.ifc_len = sizeof(ifBuffer);

	if (-1 == ioctl(sock, SIOCGIFCONF, &ifconfig))
	{
		elog(LOG, "Failed to retrieve NIC configuration.");
		return false;
	}
	return true;
}


/*
 * Retrieve information for the available NICs.
 */
static bool
RetrieveIfInfo(bool useIPv6)
{
	/*
	 * ifreq structures may be variable length;
	 * we have to step along the structure by the size of
	 * the previous structure
	 */
	struct ifreq *ifr = (struct ifreq *) ifconfig.ifc_req;
	struct ifreq *end = (struct ifreq *) ((char *) ifr + ifconfig.ifc_len);
	const char *ipvName = (useIPv6) ? "IPv6" : "IPv4";

	if (ifr == end)
	{
		elog(LOG, "Failed to retrieve NIC configuration.");
		return false;
	}

	while (ifr < end)
	{
		struct ifreq *ifrCurrent = ifr;

#ifdef __darwin__
		/* step along the array by the size of the current structure */
		ifr = (struct ifreq *)((char *)ifr + ifr->ifr_addr.sa_len + IFNAMSIZ);
#else
		ifr++;
#endif /* __darwin__ */

		if (AF_INET != ifrCurrent->ifr_addr.sa_family &&
		    AF_INET6 != ifrCurrent->ifr_addr.sa_family)
		{
			continue;
		}

		int addrLen = (ifrCurrent->ifr_addr.sa_family == AF_INET6) ?
						sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

		ifAddrIter->ifa_next = (ifAddrIter + 1);
		(void) strncpy(ifAddrIter->ifa_name, ifrCurrent->ifr_name, sizeof(ifAddrIter->ifa_name));
		(void) memcpy(&ifAddrIter->ifa_addr_alloc, &ifrCurrent->ifr_addr, addrLen);
		ifAddrIter->ifa_addr = &ifAddrIter->ifa_addr_alloc;

		/* retrieve configuration flags */
		if (-1 == ioctl(sock, SIOCGIFFLAGS, ifrCurrent))
		{
			elog(LOG, "Failed to retrieve configuration flags for NIC %s using %s.",
			     ifrCurrent->ifr_name,
			     ipvName
			     );
			continue;
		}
		ifAddrIter->ifa_flags = ifrCurrent->ifr_flags;


		/* retrieve network mask */
		if (-1 == ioctl(sock, SIOCGIFNETMASK, ifrCurrent))
		{
			elog(LOG, "Failed to retrieve network mask for NIC %s using %s.",
			     ifrCurrent->ifr_name,
			     ipvName);
			continue;
		}
		(void) memcpy(&ifAddrIter->ifa_netmask_alloc, &ifrCurrent->ifr_addr, addrLen);
		ifAddrIter->ifa_netmask = &ifAddrIter->ifa_netmask_alloc;

		/* move to next descriptor */
		ifAddrIter++;

		if ((uintptr_t) sizeof(ifaBuffer) < (uintptr_t) ifAddrIter - (uintptr_t) ifaBuffer)
		{
			elog(LOG, "Failed to retrieve configuration for all NICs, buffer capacity reached.");
			return false;
		}
	}

	/* terminate address descriptor list */
	(ifAddrIter - 1)->ifa_next = NULL;

	return true;
}


#endif /* HAVE_IFADDRS */

/* EOF */
