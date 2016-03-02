/*-------------------------------------------------------------------------
 * ic_udp.c
 *	   Interconnect code specific to UDP transport.
 *
 * Copyright (c) 2005-2008, Greenplum
 *
 * Reviewers: jzhang, tkordas
 *-------------------------------------------------------------------------
 */

#ifdef WIN32
/*
 * Need this to get WSAPoll (poll). And it
 * has to be set before any header from the Win32 API is loaded.
 */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "postgres.h"

#include <pthread.h>

#include "nodes/execnodes.h"			/* Slice, SliceTable */
#include "nodes/pg_list.h"
#include "nodes/print.h"
#include "utils/memutils.h"
#include "utils/hsearch.h"
#include "miscadmin.h"
#include "libpq/libpq-be.h"
#include "libpq/ip.h"
#include "utils/builtins.h"
#include "utils/debugbreak.h"

#include "utils/pg_crc.h"
#include "port/pg_crc32c.h"

#include "cdb/cdbselect.h"
#include "cdb/tupchunklist.h"
#include "cdb/ml_ipc.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbicudpfaultinjection.h"

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "pgtime.h"
#include <netinet/in.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "port.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#define SHUT_RDWR SD_BOTH
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND

/* If we have old platform sdk headers, WSAPoll() might not be there */
#ifndef POLLIN
/* Event flag definitions for WSAPoll(). */

#define POLLRDNORM	0x0100
#define POLLRDBAND	0x0200
#define POLLIN		(POLLRDNORM | POLLRDBAND)
#define POLLPRI		0x0400

#define POLLWRNORM	0x0010
#define POLLOUT		(POLLWRNORM)
#define POLLWRBAND	0x0020

#define POLLERR		0x0001
#define POLLHUP		0x0002
#define POLLNVAL	0x0004

typedef struct pollfd {

	SOCKET	fd;
	SHORT	events;
	SHORT	revents;

} WSAPOLLFD, *PWSAPOLLFD, FAR *LPWSAPOLLFD;
__control_entrypoint(DllExport)
WINSOCK_API_LINKAGE
int
WSAAPI
WSAPoll(
	IN OUT LPWSAPOLLFD fdArray,
	IN ULONG fds,
	IN INT timeout
	);
#endif

#define poll WSAPoll

/*
 * Postgres normally uses it's own custom select implementation
 * on Windows, but they haven't implemented execeptfds, which
 * we use here.  So, undef this to use the normal Winsock version
 * for now
 */
#undef select
#endif

#define MAX_TRY (11)
int
timeouts[] =
{
	0,
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128,
	256,
	512,
	512 /* MAX_TRY*/
};

#define TIMEOUT(try) (try < MAX_TRY ? (timeouts[try]) : (timeouts[MAX_TRY]))

/*
 * flags definitions for flag-field of UDP-messges
 *
 * we use bit operations to test these, flags are powers of two only
 */
#define UDPIC_FLAGS_ACK			(1)
#define UDPIC_FLAGS_STOP		(2)
#define UDPIC_FLAGS_EOS			(4)
#define UDPIC_FLAGS_NAK			(8)

/* synchronization */
#define RX_THREAD_POLL_TIMEOUT (250) /* 1/4 sec in msec */

#define MAIN_THREAD_COND_TIMEOUT (250000) /* 1/4 second in usec */
#define RX_THREAD_COND_TIMEOUT (250000) /* 1/4 second in usec */

static pthread_t ic_rx_thread;
static pthread_mutex_t	rx_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool ic_rx_thread_created = true;

static volatile bool rx_thread_shutdown = false;
static volatile bool rx_thread_shutdown_complete = false;

static pthread_cond_t rx_thread_cond = PTHREAD_COND_INITIALIZER;
static volatile bool rx_thread_waiting = false;

static int	rx_buf_maxcount = 1;
static int	rx_buf_count = 0;
static char *rx_buf_freelist = NULL;

static volatile bool main_thread_waiting = false;
static volatile int main_thread_waiting_node = -1;
static volatile int main_thread_waiting_route = ANY_ROUTE;
static volatile int main_thread_waiting_query = -1; /* need to disambiguate for cursors. */

static uint32	ic_id_last_teardown=0;

static uint64	ic_stats_pkt_count=0;
static uint64	ic_stats_dropped_acks=0;
static uint64	ic_stats_retrans=0;
static uint64	ic_stats_mismatch=0;
static uint64	ic_stats_rx_overrun=0;
static uint64	ic_stats_crc_error=0;

/* Global hash table for connections. */

struct conn_htab_bin {
	volatile MotionConn *conn;
	struct conn_htab_bin *next;
};

struct conn_htab_bin	**conn_htab = NULL;
#define DEFAULT_CONN_HTAB_SIZE (Max((GpIdentity.numsegments*Gp_interconnect_hash_multiplier), 16))
int						conn_htab_size = 0;

#define CURSOR_IC_TABLE_SIZE (128)

struct cursor_ic_history_entry
{
	uint32 icId;
	uint32 cid;
	uint8 status;
	struct cursor_ic_history_entry *next;
};

uint32 cursor_ic_history_count=0;

struct cursor_ic_history_entry *cursor_ic_history_table[CURSOR_IC_TABLE_SIZE];

static void
add_cursor_ic_entry(uint32 icId, uint32 cid)
{
	struct cursor_ic_history_entry *p;
	uint8 index = icId % CURSOR_IC_TABLE_SIZE;

	p = malloc(sizeof(struct cursor_ic_history_entry));
	if (p == NULL)
	{
	}

	p->icId = icId;
	p->cid = cid;
	p->status = 1;
	p->next = cursor_ic_history_table[index];

	cursor_ic_history_table[index] = p;

	cursor_ic_history_count++;

	return;
}

static void
update_cursor_ic_entry(uint32 icId, uint8 status)
{
	struct cursor_ic_history_entry *p;
	uint8 index = icId % CURSOR_IC_TABLE_SIZE;

	for (p = cursor_ic_history_table[index]; p; p = p->next)
	{
		if (p->icId == icId)
		{
			p->status = status;
			return;
		}
	}
	/* not found */
}

static struct cursor_ic_history_entry *
get_cursor_ic_entry(uint32 icId)
{
	struct cursor_ic_history_entry *p;
	uint8 index = icId % CURSOR_IC_TABLE_SIZE;

	for (p = cursor_ic_history_table[index]; p; p = p->next)
	{
		if (p->icId == icId)
		{
			return p;
		}
	}
	/* not found */
	return NULL;
}

static void
prune_cursor_ic_entry(uint32 icId)
{
	uint8 index;

	for (index=0; index<CURSOR_IC_TABLE_SIZE; index++)
	{
		struct cursor_ic_history_entry *p, *q;

		p = cursor_ic_history_table[index];
		q = NULL;
		while (p)
		{
			/*	remove an entry if it is older than the prune-point */
			if (p->icId < icId)
			{
				struct cursor_ic_history_entry *trash;

				if (!q)
				{
					cursor_ic_history_table[index] = p->next;
				}
				else
				{
					q->next = p->next;
				}
				trash = p;
				p = trash->next; /* set up next loop */
				free(trash);

				cursor_ic_history_count--;
			}
			else
			{
				q = p;
				p = p->next;
			}
		}
	}
}

static void
purge_cursor_ic_entry(void)
{
	uint8 index;

	for (index=0; index<CURSOR_IC_TABLE_SIZE; index++)
	{
		struct cursor_ic_history_entry *trash;

		while (cursor_ic_history_table[index])
		{
			trash = cursor_ic_history_table[index];
			cursor_ic_history_table[index] = trash->next;

			free(trash);
		}
	}
}

static void
reset_main_thread_waiting()
{
	main_thread_waiting = false;
	main_thread_waiting_node = -1;
	main_thread_waiting_route = ANY_ROUTE;
	main_thread_waiting_query = -1;
}

static void
set_main_thread_waiting(int motNodeId, int route, int icId)
{
	main_thread_waiting = true;
	main_thread_waiting_node = motNodeId;
	main_thread_waiting_route = route;
	main_thread_waiting_query = icId;
}

static void getSockAddr(struct sockaddr_storage * peer, socklen_t * peer_len, const char * listenerAddr, int listenerPort);

static ChunkTransportStateEntry *startOutgoingUDPConnections(ChunkTransportState *transportStates,
															 Slice *sendSlice,
															 int *pOutgoingCount);
static void setupOutgoingUDPConnection(ChunkTransportState *transportStates,
									   ChunkTransportStateEntry *pEntry, MotionConn *conn);

static char *format_sockaddr(struct sockaddr *sa, char* buf, int bufsize);

static TupleChunkListItem RecvTupleChunkFromAnyUDP(MotionLayerState *mlStates,
												   ChunkTransportState *transportStates,
												   int16 motNodeID,
												   int16 *srcRoute);

static TupleChunkListItem RecvTupleChunkFromUDP(ChunkTransportState *transportStates,
												int16		motNodeID,
												int16		srcRoute);

static void SendEosUDP(MotionLayerState *mlStates, ChunkTransportState *transportStates,
					   int motNodeID, TupleChunkListItem tcItem);
static bool SendChunkUDP(MotionLayerState *mlStates, ChunkTransportState *transportStates,
						 ChunkTransportStateEntry *pEntry, MotionConn * conn, TupleChunkListItem tcItem, int16 motionId);

static TupleChunkListItem receiveChunksUDP(ChunkTransportState *pTransportStates, ChunkTransportStateEntry *pEntry,
										   int16 motNodeID, int16 *srcRoute, volatile MotionConn *conn, bool inTeardown);

static void doSendStopMessageUDP(ChunkTransportState *transportStates, int16 motNodeID);

static bool dispatcherAYT(void);

static void *ic_rx_thread_func(void *arg);

static void rx_handle_mismatch(struct icpkthdr *pkt, struct sockaddr_storage *peer, int peer_len);
static void aggregateStatistics(ChunkTransportStateEntry *pEntry);

static void handleShortRead(uint32 shortPacketCount, int readCount);

static void
setupUDPListeningSocket(int *listenerSocketFd, uint16 *listenerPort)
{
	int					errnoSave;
	int					fd = -1;
	int					rcvbuf;
	socklen_t			rcvbufsize=0;
	const char		   *fun;

	/*
	 * At the moment, we don't know which of IPv6 or IPv4 is wanted,
	 * or even supported, so just ask getaddrinfo...
	 *
	 * Perhaps just avoid this and try socket with AF_INET6 and AF_INT?
	 *
	 * Most implementation of getaddrinfo are smart enough to only
	 * return a particular address family if that family is both enabled,
	 * and at least one network adapter has an IP address of that family.
	 */
	struct addrinfo hints;
	struct addrinfo *addrs, *rp;
	int  s;
	struct sockaddr_storage our_addr;
	socklen_t our_addr_len;
	char service[32];
	snprintf(service,32,"%d",0);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET | AF_INET6;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;	/* For wildcard IP address */
	hints.ai_protocol = 0;			/* Any protocol */

#ifdef __darwin__
	hints.ai_family = AF_INET; /* Due to a bug in OSX Leopard, disable IPv6 for UDP interconnect on all OSX platforms */
#endif

	fun = "getaddrinfo";
	s = getaddrinfo(NULL, service, &hints, &addrs);
	if (s != 0)
		elog(ERROR,"getaddrinfo says %s",gai_strerror(s));

	/*
	 * getaddrinfo() returns a list of address structures,
	 * one for each valid address and family we can use.
	 *
	 * Try each address until we successfully bind.
	 * If socket (or bind) fails, we (close the socket
	 * and) try the next address.  This can happen if
	 * the system supports IPv6, but IPv6 is disabled from
	 * working, or if it supports IPv6 and IPv4 is disabled.
	 */

	/*
	 * If there is both an AF_INET6 and an AF_INET choice,
	 * we prefer the AF_INET6, because on UNIX it can receive either
	 * protocol, whereas AF_INET can only get IPv4.  Otherwise we'd need
	 * to bind two sockets, one for each protocol.
	 *
	 * Why not just use AF_INET6 in the hints?	That works perfect
	 * if we know this machine supports IPv6 and IPv6 is enabled,
	 * but we don't know that.
	 */

#ifndef __darwin__
#ifdef HAVE_IPV6
	if (addrs->ai_family == AF_INET && addrs->ai_next != NULL && addrs->ai_next->ai_family == AF_INET6)
	{
		/*
		 * We got both an INET and INET6 possibility, but we want to prefer the INET6 one if it works.
		 * Reverse the order we got from getaddrinfo so that we try things in our preferred order.
		 * If we got more possibilities (other AFs??), I don't think we care about them, so don't
		 * worry if the list is more that two, we just rearrange the first two.
		 */
		struct addrinfo *temp = addrs->ai_next;		/* second node */
		addrs->ai_next = addrs->ai_next->ai_next;	/* point old first node to third node if any */
		temp->ai_next = addrs;						/* point second node to first */
		addrs = temp;								/* start the list with the old second node */
		elog(DEBUG1,"Have both IPv6 and IPv4 choices");
	}
#endif
#endif

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		fun = "socket";
		/*
		 * getaddrinfo gives us all the parameters for the socket() call
		 * as well as the parameters for the bind() call.
		 */
		elog(DEBUG1,"receive socket ai_family %d ai_socktype %d ai_protocol %d",rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1)
			continue;
		elog(DEBUG1,"receive socket %d ai_family %d ai_socktype %d ai_protocol %d",fd,rp->ai_family, rp->ai_socktype, rp->ai_protocol);


		fun = "fcntl(O_NONBLOCK)";
		if (!pg_set_noblock(fd))
			continue;

		fun = "bind";
		elog(DEBUG1,"bind addrlen %d fam %d",rp->ai_addrlen,rp->ai_addr->sa_family);
		if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;					/* Success */

		close(fd);
	}

	if (rp == NULL)
	{				/* No address succeeded */
		goto error;
	}

	freeaddrinfo(addrs);		   /* No longer needed */

	/*
	 * Get our socket address (IP and Port), which we will save for others to connected to.
	 */
	MemSet(&our_addr, 0, sizeof(our_addr));
	our_addr_len = sizeof(our_addr);

	fun = "getsockname";
	if (getsockname(fd, (struct sockaddr *) &our_addr, &our_addr_len) < 0)
		goto error;

	Assert(our_addr.ss_family == AF_INET ||our_addr.ss_family == AF_INET6 );

	*listenerSocketFd = fd;
	if (our_addr.ss_family == AF_INET6)
		*listenerPort = ntohs(((struct sockaddr_in6 *)&our_addr)->sin6_port);
	else
		*listenerPort = ntohs(((struct sockaddr_in *)&our_addr)->sin_port);

	fun = "setsockopt";

	/*
	 * the max-size supported by all of our platforms appears to be 128K ?
	 *
	 * We'll try 256k first, and fall back to 128K if that doesn't work.
	 */
	rcvbuf = 256 * 1024;

	if (Gp_udp_bufsize_k != 0)
		rcvbuf = Gp_udp_bufsize_k * 1024;

retry:
	rcvbufsize = sizeof(rcvbuf);
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, rcvbufsize) < 0)
	{
		/* try a smaller buffer ? */
		if (Gp_udp_bufsize_k == 0 && rcvbuf == 256 * 1024)
		{
			rcvbuf = 128 * 1024;
			goto retry;
		}

		goto error;
	}

	fun = "getsockopt";
	rcvbufsize = sizeof(rcvbuf);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &rcvbufsize) < 0)
		goto error;

	elog(DEBUG2, "setupUDPListeningSocket: rcvbuf size %d", rcvbuf);

	return;

error:
	errnoSave = errno;
	if (fd >= 0)
		closesocket(fd);
	errno = errnoSave;
	ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("Interconnect Error: Could not set up udp listener socket."),
					errdetail("%m%s", fun)));
	return;
}

/*
 * Initialize UDP specific comms, and create rx-thread.
 */
void
InitMotionUDP(int *listenerSocketFd, uint16 *listenerPort)
{
	int pthread_err;
	int i;
	pthread_mutexattr_t m_atts;

	/* attributes of the thread we're creating */
	pthread_attr_t t_atts;

#ifdef USE_ASSERT_CHECKING
	set_test_mode();
#endif

	memset(cursor_ic_history_table, 0, sizeof(cursor_ic_history_table));

	setupUDPListeningSocket(listenerSocketFd, listenerPort);

	/* set up the connection hash table */
	conn_htab_size = DEFAULT_CONN_HTAB_SIZE;
	conn_htab = (struct conn_htab_bin **)malloc(conn_htab_size * sizeof(struct conn_htab_bin *));

	if (conn_htab == NULL)
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error: could not allocate connection hash, out of memory."),
						errdetail("%m%s", "malloc")));

	for (i=0; i < conn_htab_size; i++)
		conn_htab[i] = NULL;

	/* Set up our mutex */
	pthread_mutexattr_init(&m_atts);
	pthread_mutexattr_settype(&m_atts, PTHREAD_MUTEX_ERRORCHECK);

	pthread_mutex_init(&rx_thread_mutex, &m_atts);

	/* Start up our rx-thread */

	/* save ourselves some memory: the defaults for thread stack
	 * size are large (1M+) */
	pthread_attr_init(&t_atts);

#ifdef pg_on_solaris
	/* Solaris doesn't have PTHREAD_STACK_MIN ? */
	pthread_attr_setstacksize(&t_atts, (128*1024));
#else
	pthread_attr_setstacksize(&t_atts, Max(PTHREAD_STACK_MIN, (128*1024)));
#endif
	pthread_err = pthread_create(&ic_rx_thread, &t_atts, ic_rx_thread_func, NULL);

	pthread_attr_destroy(&t_atts);
	if (pthread_err != 0)
	{
		ic_rx_thread_created = false;
		ereport(FATAL, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("InitMotionLayerIPC: failed to create thread"),
						errdetail("pthread_create() failed with err %d", pthread_err)));
	}

	ic_rx_thread_created = true;
	return;
}

void
CleanupMotionUDP(void)
{
	int i;

	elog(DEBUG2, "udp-ic: telling receiver thread to shutdown.");

	rx_thread_shutdown = true;

	pthread_mutex_unlock(&rx_thread_mutex);

	if(ic_rx_thread_created)
		pthread_join(ic_rx_thread, NULL);

	elog(DEBUG2, "udp-ic: receiver thread shutdown.");

	purge_cursor_ic_entry();

	for (i=0; i < conn_htab_size; i++)
	{
		struct conn_htab_bin *trash;

		while (conn_htab[i] != NULL)
		{
			trash = conn_htab[i];
			conn_htab[i] = trash->next;
			free(trash);
		}
	}
	free(conn_htab);
}

/*=========================================================================
 * INLINE FUNCTIONS
 */

#ifdef USE_ASSERT_CHECKING
static inline bool
testmode_drop(int percent)
{
	if (udp_testmode &&
		(gp_udpic_dropseg == UNDEF_SEGMENT || gp_udpic_dropseg == Gp_segment))
	{
			if (random() % 100 < percent)
				return true;
	}

	return false;
}
#endif

/* MUST BE CALLED WITH rx_thread_mutex HELD! */
static inline bool
wait_on_receiver(int timeout_us)
{
	struct timespec ts;
	int wait;

	/*
	 * MPP-9910: pthread_cond_timedwait appears to be broken in OS-X 10.6.x "Snow Leopard"
	 * Let's use a different timewait function that works better on OSX (and is simpler
	 * because it uses relative time)
	 */
#ifdef __darwin__
	ts.tv_sec = 0;
	ts.tv_nsec = 1000 * timeout_us;
#else
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec;
		/*	leave in ms for this */
		ts.tv_nsec = (tv.tv_usec + timeout_us);
		if (ts.tv_nsec >= 1000000)
		{
			ts.tv_sec++;
			ts.tv_nsec -= 1000000;
		}
		ts.tv_nsec *= 1000; /* convert usec to nsec */
	}
#endif

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(DEBUG5, "waiting (timed) on route %d %s", main_thread_waiting_route,
			 (main_thread_waiting_route == ANY_ROUTE ? "(any route)" : ""));
	}

	/*
	 * we can't handle interrupts for the time we're waiting,
	 * which is why we're using timedwaits in the first place
	 */
	HOLD_INTERRUPTS();

#ifdef __darwin__
	wait = pthread_cond_timedwait_relative_np(&rx_thread_cond, &rx_thread_mutex, &ts);
#else
	wait = pthread_cond_timedwait(&rx_thread_cond, &rx_thread_mutex, &ts);
#endif

	RESUME_INTERRUPTS();

	if (wait == ETIMEDOUT)
	{
		/* condition not met */
		return false;
	}

	/* we didn't time out, condition met! */
	return true;
}

#define CONN_HASH_VALUE(icpkt) ((uint32)((((icpkt)->srcPid ^ (icpkt)->dstPid)) + (icpkt)->dstContentId))

#define CONN_HASH_MATCH(a, b) (((a)->motNodeId == (b)->motNodeId && \
								(a)->dstContentId == (b)->dstContentId && \
								(a)->srcContentId == (b)->srcContentId && \
								(a)->recvSliceIndex == (b)->recvSliceIndex && \
								(a)->sendSliceIndex == (b)->sendSliceIndex && \
								(a)->srcPid == (b)->srcPid &&			\
								(a)->dstPid == (b)->dstPid && (a)->icId == (b)->icId) ? true : false)

/*
 * Note: we want to add a connection to the hashtable if it isn't
 * already there ... so we just have to check the pointer values -- no
 * need to use CONN_HASH_MATCH() at all!
 */
static bool
conn_addhash(volatile MotionConn *conn)
{
	uint32 hashcode;
	struct conn_htab_bin *bin, *newbin;

	hashcode = CONN_HASH_VALUE(&conn->conn_info) % conn_htab_size;

	/*
	 * check for collision -- if we already have an entry for this
	 * connection, don't add another one.
	 */
	for (bin = conn_htab[hashcode]; bin != NULL; bin = bin->next)
	{
		if (bin->conn == conn)
		{
			elog(DEBUG5, "conn_addhash(): duplicate ?! node %d route %d", conn->conn_info.motNodeId, conn->route);
			return true; /* false *only* indicates memory-alloc failure. */
		}
	}

	newbin = (struct conn_htab_bin *)malloc(sizeof(struct conn_htab_bin));
	if (newbin == NULL)
	{
		return false;
	}
	newbin->conn = conn;
	newbin->next = conn_htab[hashcode];
	conn_htab[hashcode] = newbin;

	return true;
}

/*
 * Note: we want to remove a connection from the hashtable if it is
 * there ... so we just have to check the pointer values -- no need to
 * use CONN_HASH_MATCH() at all!
 */
static void
conn_delhash(volatile MotionConn *conn)
{
	uint32 hashcode;
	struct conn_htab_bin *c, *p, *trash;

	hashcode = CONN_HASH_VALUE(&conn->conn_info) % conn_htab_size;

	c = conn_htab[hashcode];

	/* find entry */
	p = NULL;
	while (c != NULL)
	{
		/* found ? */
		if (c->conn == conn)
			break;

		p = c;
		c = c->next;
	}

	/* not found ? */
	if (c == NULL)
	{
		return;
	}

	/* found the connection, remove from the chain. */
	trash = c;

	if (p == NULL)
		conn_htab[hashcode] = c->next;
	else
		p->next = c->next;

	free(trash);
	return;
}

/*
 * With the new mirroring scheme, the interconnect is no longer involved:
 * we don't have to disambiguate anymore.
 *
 * NOTE: the icpkthdr field dstListenerPort is used for disambiguation.
 * on receivers it may not match the actual port (it may have an extra bit
 * set (1<<31)).
 */
static volatile MotionConn *
findConnByHeader(struct icpkthdr *hdr)
{
	uint32 hashcode;
	struct conn_htab_bin *bin;
	volatile MotionConn *ret = NULL;

	hashcode = CONN_HASH_VALUE(hdr) % conn_htab_size;

	for (bin = conn_htab[hashcode]; bin != NULL; bin = bin->next)
	{
		if (CONN_HASH_MATCH(&bin->conn->conn_info, hdr))
		{
			ret = bin->conn;

			if (DEBUG5 >= log_min_messages)
				write_log("findConnByHeader: found. route %d state %d hashcode %d conn %p",
						  ret->route, ret->state, hashcode, ret);

			return ret;
		}
	}

	if (DEBUG5 >= log_min_messages)
		write_log("findConnByHeader: not found! (hdr->srcPid %d "
				  "hdr->srcContentId %d hdr->dstContentId %d hdr->dstPid %d sess(%d:%d) cmd(%d:%d)) hashcode %d",
				  hdr->srcPid, hdr->srcContentId, hdr->dstContentId, hdr->dstPid, hdr->sessionId,
				  gp_session_id, hdr->icId, gp_interconnect_id, hashcode);

	return NULL;
}

/*
 * This must be called with the rx_thread mutex held (maybe these locks need to be restructured a bit).
 */
static bool
conn_rehash(void)
{
	int i;
	uint32 new_hashcode;
	int new_htab_size = DEFAULT_CONN_HTAB_SIZE;
	struct conn_htab_bin		**new_conn_htab = NULL;
	struct conn_htab_bin		*bin, *newbin;

	new_conn_htab = (struct conn_htab_bin **)malloc(new_htab_size * sizeof(struct conn_htab_bin *));

	if (new_conn_htab == NULL)
	{
		elog(ERROR, "Could not resize connection hash table (old size %d, new size %d)",
			 conn_htab_size, new_htab_size);
		return false;
	}

	for (i=0; i < new_htab_size; i++)
		new_conn_htab[i] = NULL;

	/* For each bucket in the hash table */
	for (i=0; i < conn_htab_size; i++)
	{
		/* for each entry in this bucket */
		bin = conn_htab[i];

		while (bin)
		{
			volatile MotionConn *conn = bin->conn;

			new_hashcode = CONN_HASH_VALUE(&conn->conn_info) % new_htab_size;

			/* save a reference to this bin */
			newbin = bin;

			/* advance to the next bin in the old table */
			bin = bin->next;

			/* add this bin to the new table */
			newbin->next = new_conn_htab[new_hashcode];;
		}
	}

	free(conn_htab);
	conn_htab_size = new_htab_size;
	conn_htab = new_conn_htab;

	return true;
}

static void
sendAck(volatile MotionConn *conn, bool stop)
{
	int n;
	struct icpkthdr msg;

	if (stop)
		conn->conn_info.flags |= UDPIC_FLAGS_STOP;

	MemSet(&msg, 0, sizeof(msg));
	memcpy(&msg, (char *)&conn->conn_info, sizeof(msg));
	msg.flags |= UDPIC_FLAGS_ACK;

	msg.len = 0;

	if (DEBUG5 >= log_min_messages)
		write_log("sendack: flags 0x%x node %d route %d seq %d",
				  msg.flags, msg.motNodeId, conn->route, conn->conn_info.seq);

#ifdef USE_ASSERT_CHECKING
	if (testmode_drop(gp_udpic_dropacks_percent))
		return;
#endif
	Assert(UDP_listenerFd >= 0);
	Assert(conn->peer.ss_family == AF_INET ||conn->peer.ss_family == AF_INET6 );


	n = sendto(UDP_listenerFd, (const char *)&msg, sizeof(msg), 0,
			   (struct sockaddr *)&conn->peer, conn->peer_len);

	/* No need to handle EAGAIN here: no-space just means that we
	 * dropped the packet: our ordinary retransmit mechanism will
	 * handle that case*/

	if (n < sizeof(msg))
		write_log("sendack: got error %d errno %d route %d seq %d",
				  n, errno, conn->route, conn->conn_info.seq);
}

/* should be called with  rx_thread_mutex *locked* */
static void
put_rx_buffer(volatile MotionConn *conn)
{
	struct icpkthdr *buf=NULL;

	buf = (struct icpkthdr *)conn->pBuff;

	if (buf == NULL)
	{
		pthread_mutex_unlock(&rx_thread_mutex);
		elog(FATAL, "put_rx_buffer: buffer is NULL");
	}
	else if ((uint8 *)buf != conn->pkt_q[conn->pkt_q_head])
	{
		pthread_mutex_unlock(&rx_thread_mutex);
		elog(FATAL, "put_rx_buffer: buffer is not at head position.");
	}

	/* advance queue head. */
	conn->pkt_q[conn->pkt_q_head] = NULL;
	conn->pkt_q_head++;
	if (conn->pkt_q_head == Gp_interconnect_queue_depth)
		conn->pkt_q_head = 0;
	conn->pkt_q_size--;

	*(char **)buf = rx_buf_freelist;
	rx_buf_freelist = (char *)buf;

	/* now we need to pick up the next element (@ the new head).*/
	if (conn->pkt_q_size == 0)
		conn->pBuff = NULL;
	else
	{
		/* update the protocol-specific info of the newly active rx-buffer */
		conn->pBuff = conn->pkt_q[conn->pkt_q_head];
		buf = (struct icpkthdr *)conn->pBuff;

		conn->msgPos = conn->pBuff;

		conn->msgSize = buf->len;
		conn->recvBytes = conn->msgSize;
	}

	if (rx_thread_waiting)
	{
		pthread_cond_signal(&rx_thread_cond);
		elog(DEBUG2, "put_rx_buffer: waking rx thread");
	}
}

/*
 * The cdbmotion code has discarded our pointer to the motion-conn
 * structure, but has enough info to fully specify it.
 */
void
MlPutRxBuffer(ChunkTransportState *transportStates, int motNodeID, int route)
{
	ChunkTransportStateEntry	*pEntry = NULL;
	volatile MotionConn			*conn=NULL;

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	conn = pEntry->conns + route;

	pthread_mutex_lock(&rx_thread_mutex);

	if (conn->pBuff != NULL)
		put_rx_buffer(conn);
	else
		elog(LOG, "ml_put_rx_buffer: NULL pBuff");

	pthread_mutex_unlock(&rx_thread_mutex);
}

/* should be called with rx_thread_mutex *locked* */
static struct icpkthdr *
get_rx_buffer(void)
{
	struct icpkthdr *ret=NULL;

	do
	{
		if (rx_buf_freelist == NULL)
		{
			if (rx_buf_count > rx_buf_maxcount)
			{
				if (DEBUG3 >= log_min_messages)
					write_log("Interconnect ran out of rx-buffers count/max %d/%d", rx_buf_count, rx_buf_maxcount);
				break;
			}

			ret = (struct icpkthdr *)malloc(Gp_max_packet_size);

			/*
			 * Note: we return NULL if the malloc() fails -- and the
			 * main-thread will either free a buffer up, or notice
			 * that we ran out (throw an erorr) and start the
			 * teardown
			 */
			if (ret != NULL)
				rx_buf_count++;

			break;
		}

		/* we have buffers available in our freelist */
		ret = (struct icpkthdr *)rx_buf_freelist;
		rx_buf_freelist = *(char **)(rx_buf_freelist);
	} while (0);

	return ret;
}

static void setXmitSocketOptions(int txfd);
static void
setXmitSocketOptions(int txfd)
{
	int errnoSave;
	const char * fun;
	/*
	 * Note the buffer stuff below: The receive-side of our transmit socket is only used for acks.
	 *
	 * 128k of ack-space is enough for ~2000 acks if they're about 60-bytes. So that's a pretty
	 * large number -- probably good enough.
	 *
	 * For transmit outbound space, I'm not sure that we can really feed the kernel as fast as
	 * it can transmit. 128k of out-bound space is enough for 16 outbound messages at the default
	 * size. Just in case, let's try 256k.
	 *
	 * Since only one copy of this outbound-stuff is held *per process* these buffers are pretty
	 * cheap.
	 */

	{
		int					rcvbuf;
		socklen_t			rcvbufsize=0;

		fun = "getsockopt";
		rcvbufsize = sizeof(rcvbuf);
		if (getsockopt(txfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &rcvbufsize) < 0)
			goto error;

		elog(DEBUG1, "UDP-IC: xmit default rx-size %d bytes", rcvbuf);

		fun = "setsockopt";
		rcvbuf = 128 * 1024;

		if (Gp_udp_bufsize_k != 0)
			rcvbuf = Gp_udp_bufsize_k * 1024;

		rcvbufsize = sizeof(rcvbuf);
		if (setsockopt(txfd, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, rcvbufsize) < 0)
			goto error;

		elog(DEBUG1, "UDP-IC: xmit will use rx size of %d bytes", rcvbuf);
	}

	{
		int					sndbuf;
		socklen_t			sndbufsize=0;

		fun = "getsockopt";
		sndbufsize = sizeof(sndbuf);
		if (getsockopt(txfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &sndbufsize) < 0)
			goto error;

		elog(DEBUG1, "UDP-IC: xmit default tx-size %d bytes", sndbuf);

		fun = "setsockopt";
		sndbuf = 256 * 1024;
	retry:
		sndbufsize = sizeof(sndbuf);
		if (setsockopt(txfd, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, sndbufsize) < 0)
		{
			if (sndbuf == 256 * 1024)
			{
				sndbuf = 128 * 1024;
				goto retry;
			}
			goto error;
		}

		elog(DEBUG1, "UDP-IC: xmit will use tx size of %d bytes", sndbuf);
	}
	return;

error:
	errnoSave = errno;
	if (txfd >= 0)
		closesocket(txfd);
	errno = errnoSave;
	ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("Interconnect Error: Could not set up listener socket."),
					errdetail("%m%s", fun)));
}

static int getPortFromSocket(int fd);
static int getPortFromSocket(int fd)
{
	struct sockaddr_storage source_addr;
	socklen_t source_addr_len;
	MemSet(&source_addr, 0, sizeof(source_addr));
	source_addr_len = sizeof(source_addr);

	if (getsockname(fd, (struct sockaddr *) &source_addr, &source_addr_len) < 0)
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("Interconnect Error: Could not get port from socket."),
					errdetail("%m")));

	/* return the port (if we are doing this, it was picked by the system) */
	if (source_addr.ss_family == AF_INET6)
		return ntohs(((struct sockaddr_in6*)&source_addr)->sin6_port);
	else
		return ntohs(((struct sockaddr_in *)&source_addr)->sin_port);


}

/* Function startOutgoingConnections() is used to initially kick-off any outgoing
 * connections for mySlice.
 *
 * This should not be called for root slices (i.e. QD ones) since they don't
 * ever have outgoing connections.
 *
 * PARAMETERS
 *
 *	 sendSlice	- Slice that this process is a member of.
 *	 pIncIdx - index in the parent slice list of mySlice.
 *
 * RETURNS
 *	 Initialized ChunkTransportState for the Sending Motion Node Id.
 */
ChunkTransportStateEntry *
startOutgoingUDPConnections(ChunkTransportState *transportStates,
							Slice		*sendSlice,
							int			*pOutgoingCount)
{
	ChunkTransportStateEntry *pEntry;
	MotionConn *conn;
	ListCell   *cell;
	Slice	   *recvSlice;
	CdbProcess *cdbProc;

	const char		   *fun;
	int					i;
	int					errnoSave;

	*pOutgoingCount = 0;

	recvSlice = (Slice *) list_nth(transportStates->sliceTable->slices, sendSlice->parentIndex);

	adjustMasterRouting(recvSlice);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG4, "Interconnect seg%d slice%d setting up sending motion node",
			 Gp_segment, sendSlice->sliceIndex);

	pEntry = createChunkTransportState(transportStates,
									   sendSlice,
									   recvSlice,
									   list_length(recvSlice->primaryProcesses));

	Assert(pEntry && pEntry->valid);
	/*
	 * Setup a MotionConn entry for each of our outbound connections.
	 * Request a connection to each receiving backend's listening port.
	 * NB: Some mirrors could be down & have no CdbProcess entry.
	 */
	conn = pEntry->conns;

	i = 0;
	foreach(cell, recvSlice->primaryProcesses)
	{
		cdbProc = (CdbProcess *)lfirst(cell);
		if (cdbProc)
		{
			conn->cdbProc = cdbProc;
			conn->ping = palloc(Gp_max_packet_size);
			conn->pong = palloc(Gp_max_packet_size);
			conn->pBuff = conn->ping;
			conn->activeXmit = NULL;
			conn->state = mcsSetupOutgoingConnection;
			conn->route = i++;
			(*pOutgoingCount)++;
		}

		conn++;
	}

	pEntry->txport = 0;
	pEntry->txfd = 0;

	struct addrinfo hints;
	struct addrinfo *addrs, *rp;
	int s;

	char service[32];
	snprintf(service,32,"%d",0);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET | AF_INET6;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE; /* For wildcard IP address */
	hints.ai_protocol = 0; /* Any protocol */

#ifdef __darwin__
	hints.ai_family = AF_INET; /* Due to a bug in OSX Leopard, disable IPv6 for UDP interconnect on all OSX platforms */
#endif

	fun = "getaddrinfo";
	s = getaddrinfo(NULL, service, &hints, &addrs);
	if (s != 0)
		elog(ERROR,"getaddrinfo says %s",gai_strerror(s));

#ifndef __darwin__
#ifdef HAVE_IPV6
	if (addrs->ai_family == AF_INET && addrs->ai_next != NULL && addrs->ai_next->ai_family == AF_INET6)
	{
		/*
		 * We got both an INET and INET6 possibility, but we want to prefer the INET6 one if it works.
		 * Reverse the order we got from getaddrinfo so that we try things in our preferred order.
		 * If we got more possibilities (other AFs??), I don't think we care about them, so don't
		 * worry if the list is more that two, we just rearrainge the first two.
		 */
		struct addrinfo *temp = addrs->ai_next;		/* second node */
		addrs->ai_next = addrs->ai_next->ai_next;	/* point old first node to third node if any */
		temp->ai_next = addrs;						/* point second node to first */
		addrs = temp;								/* start the list with the old second node */
	}
#endif
#endif

	/*
	 * getaddrinfo() returns a list of address structures.
	 * Try each address until we successfully bind.
	 * If socket (or bind) fails, we (close the socket
	 * and) try the next address.
	 */

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		fun = "socket";
		pEntry->txfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (pEntry->txfd == -1)
			continue;
		pEntry->txfd_family = rp->ai_family;

		elog(DEBUG1,"pEntry->txfd %d socket ai_family=%d, ai_socktype %d, ai_protocol %d",pEntry->txfd,rp->ai_family,rp->ai_socktype, rp->ai_protocol);

		fun = "fcntl(O_NONBLOCK)";
		if (!pg_set_noblock(pEntry->txfd))
			continue;

		fun = "bind";
		elog(DEBUG1,"bind ai_addrlen %d, ai_fam %d",rp->ai_addrlen, rp->ai_addr->sa_family);
		if (bind(pEntry->txfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break; /* Success */

		close(pEntry->txfd);
	}

	if (rp == NULL)
	{
		/* No address succeeded */
		goto error;
	}

	freeaddrinfo(addrs); /* No longer needed */

	/* Save the txport we are using.  Just for debugging, I think */
	pEntry->txport = getPortFromSocket(pEntry->txfd);

	setXmitSocketOptions(pEntry->txfd);

	return pEntry;

error:
	errnoSave = errno;
	if (pEntry->txfd >= 0)
		closesocket(pEntry->txfd);
	errno = errnoSave;
	ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("Interconnect Error: Could not set up listener socket."),
					errdetail("%m%s", fun)));
	return NULL;

}


/*
 * Convert IP addr and port to sockaddr
 */
static void
getSockAddr(struct sockaddr_storage * peer, socklen_t * peer_len, const char * listenerAddr, int listenerPort)
{
	int			ret;
	char		portNumberStr[32];
	char	   *service;
	struct addrinfo *addrs = NULL;
	struct addrinfo hint;

	/*
	 * Get socketaddr to connect to.
	 */

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_DGRAM; /* UDP */
	hint.ai_family = AF_INET | AF_INET6; /* Allow for any family (v4, v6, even unix in the future)  */
#ifdef AI_NUMERICSERV
	hint.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;  /* Never do name resolution */
#else
	hint.ai_flags = AI_NUMERICHOST;  /* Never do name resolution */
#endif

	snprintf(portNumberStr, sizeof(portNumberStr), "%d", listenerPort);
	service = portNumberStr;

	ret = pg_getaddrinfo_all(listenerAddr, service, &hint, &addrs);
	if (ret || !addrs)
	{
		if (addrs)
			pg_freeaddrinfo_all(hint.ai_family, addrs);

		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
			errmsg("Interconnect Error: Could not parse remote listener"
				   "address: '%s' port '%d': %s", listenerAddr,listenerPort,gai_strerror(ret)),
			errdetail("getaddrinfo() unable to parse address: '%s'",
					  listenerAddr)));
		return;
	}
	/* Since we aren't using name resolution, getaddrinfo will return only 1 entry */

	elog(DEBUG1,"GetSockAddr socket ai_family %d ai_socktype %d ai_protocol %d for %s ",addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol, listenerAddr);
	memset(peer, 0, sizeof(struct sockaddr_storage));
	memcpy(peer, addrs->ai_addr, addrs->ai_addrlen);
	*peer_len = addrs->ai_addrlen;

	pg_freeaddrinfo_all(addrs->ai_family, addrs);
}
/*
 * setupOutgoingUDPConnection
 *
 * Called by SetupInterconnect when conn->state == mcsSetupOutgoingConnection.
 *
 * On return, state is:
 *		mcsSetupOutgoingConnection if failed and caller should retry.
 *		state then transitions to mcsStarted (once connection request is
 * acked by remote peer).
 */
void
setupOutgoingUDPConnection(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	CdbProcess		   *cdbProc = conn->cdbProc;
	int					n;

	Assert(conn->state == mcsSetupOutgoingConnection);
	Assert(conn->cdbProc);

	conn->wakeup_ms = 0;
	conn->remoteContentId = cdbProc->contentid;
	conn->stat_min_ack_time = ~((uint64)0);

	/* Save the information for the error message if getaddrinfo fails */
	if (strchr(cdbProc->listenerAddr,':') != 0)
		snprintf(conn->remoteHostAndPort, sizeof(conn->remoteHostAndPort),
					 "[%s]:%d", cdbProc->listenerAddr, cdbProc->listenerPort);
	else
		snprintf(conn->remoteHostAndPort, sizeof(conn->remoteHostAndPort),
			 "%s:%d", cdbProc->listenerAddr, cdbProc->listenerPort);

	/*
	 * Get socketaddr to connect to.
	 */
	getSockAddr(&conn->peer, &conn->peer_len, cdbProc->listenerAddr, cdbProc->listenerPort);


	/* Save the destination IP address */
	format_sockaddr((struct sockaddr *)&conn->peer, conn->remoteHostAndPort,
					sizeof(conn->remoteHostAndPort));



	Assert(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6 );
	if (pEntry->txfd <= 0)
	{
		/* TEMP */

		/*
		 * If we get here, we deferred creating the socket until now, because now is the
		 * first time we know the address family of the remote side.
		 *
		 * Currently, we don't use this code... We just create the socket up front, using AF_INET6
		 * if it *might* be IPv6, since we can send to either IPv4 or IPv6 addresses through that kind of socket.
		 */
		struct sockaddr saddr;

		pEntry->txfd = socket(conn->peer.ss_family, SOCK_DGRAM, 0);
		Assert(pEntry->txfd >= 0);
		elog(DEBUG1, "creating a new socket %d for tx with family %d",pEntry->txfd,(int)conn->peer.ss_family);
		pg_set_noblock(pEntry->txfd);
		setXmitSocketOptions(pEntry->txfd);
		MemSet(&saddr, 0, sizeof(saddr));
		saddr.sa_family = conn->peer.ss_family;
		if (bind(pEntry->txfd, &saddr, sizeof(saddr)) < 0)
			Assert("bind failed"==0);
		/* Save the txport we are using.  Just for debugging, I think */
		pEntry->txport = getPortFromSocket(pEntry->txfd);
		/* TEMP */
	}
	else

	{
#ifdef USE_ASSERT_CHECKING
		{
			struct sockaddr_storage source_addr;
			socklen_t source_addr_len;
			MemSet(&source_addr, 0, sizeof(source_addr));
			source_addr_len = sizeof(source_addr);


			if (getsockname(pEntry->txfd, (struct sockaddr *) &source_addr, &source_addr_len) == -1)
				Assert("getsockname failed"==0);
			Assert(pEntry->txfd_family == source_addr.ss_family);
		}
#endif
		/*
		 * If the socket was created with a different address family than the place we
		 * are sending to, we might need to do something special.
		 */
		if (pEntry->txfd_family != conn->peer.ss_family)
		{
			/*
			 * If the socket was created AF_INET6, but the address we want to send to is IPv4 (AF_INET),
			 * we might need to change the address format.  On Linux, it isn't necessary:  glibc automatically
			 * handles this.  But on MAC OSX and Solaris, we need to convert the IPv4 address to an
			 * V4-MAPPED address in AF_INET6 format.
			 */
			if (pEntry->txfd_family == AF_INET6)
			{
				struct sockaddr_storage temp;
				const struct sockaddr_in *in = (const struct sockaddr_in *)&conn->peer;
				struct sockaddr_in6 *in6_new = (struct sockaddr_in6 *)&temp;
				memset(&temp, 0, sizeof(temp));

				elog(DEBUG2, "We are inet6, remote is inet.  Converting to v4 mapped address.");

				/* Construct a V4-to-6 mapped address.  */
				temp.ss_family = AF_INET6;
				in6_new->sin6_family = AF_INET6;
				in6_new->sin6_port = in->sin_port;
				in6_new->sin6_flowinfo = 0;

				memset (&in6_new->sin6_addr, '\0', sizeof (in6_new->sin6_addr));
				//in6_new->sin6_addr.s6_addr16[5] = 0xffff;
				((uint16 *)&in6_new->sin6_addr)[5] = 0xffff;
				//in6_new->sin6_addr.s6_addr32[3] = in->sin_addr.s_addr;
				memcpy(((char *)&in6_new->sin6_addr)+12,&(in->sin_addr),4);
				in6_new->sin6_scope_id = 0;

				/* copy it back */
				memcpy(&conn->peer,&temp,sizeof(struct sockaddr_in6));
				conn->peer_len = sizeof(struct sockaddr_in6);
			}
			else
			{
				/*
				 * If we get here, something is really wrong.  We created the socket as IPv4-only (AF_INET),
				 * but the address we are trying to send to is IPv6.  It's possible we could have a V4-mapped
				 * address that we could convert to an IPv4 address, but there is currently no code path where
				 * that could happen.  So this must be an error.
				 */
				elog(ERROR, "Trying to use an IPv4 (AF_INET) socket to send to an IPv6 address");
			}
		}
	}



	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		ereport(DEBUG1, (errmsg("Interconnect connecting to seg%d slice%d %s "
								"pid=%d sockfd=%d",
								conn->remoteContentId,
								pEntry->recvSlice->sliceIndex,
								conn->remoteHostAndPort,
								conn->cdbProc->pid,
								conn->sockfd)));

	/* send connection request */
	MemSet(&conn->conn_info, 0, sizeof(conn->conn_info));
	conn->conn_info.len = 0;
	conn->conn_info.flags = 0;
	conn->conn_info.motNodeId = pEntry->motNodeId;

	conn->conn_info.recvSliceIndex = pEntry->recvSlice->sliceIndex;
	conn->conn_info.sendSliceIndex = pEntry->sendSlice->sliceIndex;
	conn->conn_info.srcContentId = Gp_segment;
	conn->conn_info.dstContentId = conn->cdbProc->contentid;

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG3, "setupOutgoingUDPConnection: node %d route %d srccontent %d dstcontent %d: %s",
			 pEntry->motNodeId, conn->route, Gp_segment, conn->cdbProc->contentid, conn->remoteHostAndPort);

	conn->conn_info.srcListenerPort = (Gp_listener_port>>16) & 0x0ffff;
	conn->conn_info.srcPid = MyProcPid;
	conn->conn_info.dstPid = conn->cdbProc->pid;
	conn->conn_info.dstListenerPort = conn->cdbProc->listenerPort;

	/* MPP-4194: add marker for mirror-disambiguation. */
	if (conn->isMirror)
		conn->conn_info.dstListenerPort |= (1<<31);

	conn->conn_info.sessionId = gp_session_id;
	conn->conn_info.icId = gp_interconnect_id;

	if (!conn_addhash(conn))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error: could not allocate UDP-IC hash, out of memory."),
						errdetail("%m %s", "malloc")));
		/* not reached */
	}

#ifdef USE_ASSERT_CHECKING
	if (testmode_drop(gp_udpic_dropxmit_percent))
		return;
#endif

	if (gp_interconnect_elide_setup && gp_interconnect_id > 1)
	{
		conn->state = mcsStarted;
		conn->msgPos = NULL;
		conn->msgSize = sizeof(conn->conn_info);
		conn->stillActive = true;
		conn->conn_info.seq = 1;
		Assert(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6 );
		return;
	}

	n = sendto(pEntry->txfd, (const char *)&conn->conn_info, sizeof(conn->conn_info), 0,
			   (struct sockaddr *)&conn->peer, conn->peer_len);

	/* we'll let this packet drop. */
	if (n < 0 && errno == EAGAIN)
	{
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG4, "setupOutgoingUDPConnection: sendto() return EAGAIN, dropped packet.");
		return;
	}

	if (n != sizeof(conn->conn_info))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error setting up outgoing "
							   "connection."),
						errdetail("%m%s", "sendto")));
	}

	return;
}								/* setupOutgoingUDPConnection */

/*
 * handleShortRead
 *		handle short read cases. If the number of short reads exceeds IC_SHORT_PACKET_LIMIT,
 *		report error. Otherwise, caller should retry.
 */
static void
handleShortRead(uint32 shortPacketCount, int readCount)
{
	/* MPP-10351: could make this a guc, but leave it as a macro for now. macro is defined in ml_ipc.h */
	if (shortPacketCount < IC_SHORT_PACKET_LIMIT)
	{
		elog(LOG, "Interconnect error waiting for peer ack, short read during recvfrom() %d", readCount);
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("Interconnect error waiting for peer ack"),
				 errdetail("short read during recvfrom() call. (%d)\n", readCount)));
		/* not reached */
	}
}

void
SetupUDPInterconnect(EState *estate)
{
	int			i, n;
	ListCell   *cell;
	Slice	   *mySlice;
	Slice	   *aSlice;
	volatile MotionConn *conn=NULL;
	int			incoming_count = 0;
	int			outgoing_count = 0;
	int			expectedTotalIncoming = 0;
	int			expectedTotalOutgoing = 0;
	int			iteration = 0;
	GpMonotonicTime startTime;
	uint64			elapsed_ms = 0;
	uint64			elapsed_timeout = 0;

	ChunkTransportStateEntry *sendingChunkTransportState = NULL;

	if (estate->interconnect_context)
	{
		elog(FATAL, "SetupUDPInterconnect: already initialized.");
	}
	else if (!estate->es_sliceTable)
	{
		elog(FATAL, "SetupUDPInterconnect: no slice table ?");
	}

	estate->interconnect_context = palloc0(sizeof(ChunkTransportState));

	/* add back-pointer for dispatch check. */
	estate->interconnect_context->estate = estate;

	/* initialize state variables */
	Assert(estate->interconnect_context->size == 0);
	estate->interconnect_context->size = CTS_INITIAL_SIZE;
	estate->interconnect_context->states = palloc0(CTS_INITIAL_SIZE * sizeof(ChunkTransportStateEntry));

	estate->interconnect_context->teardownActive = false;
	estate->interconnect_context->activated = false;
	estate->interconnect_context->incompleteConns = NIL;
	estate->interconnect_context->sliceTable = NULL;
	estate->interconnect_context->sliceId = -1;

	estate->interconnect_context->sliceTable = estate->es_sliceTable;

	estate->interconnect_context->sliceId = LocallyExecutingSliceIndex(estate);

	estate->interconnect_context->RecvTupleChunkFrom = RecvTupleChunkFromUDP;
	estate->interconnect_context->RecvTupleChunkFromAny = RecvTupleChunkFromAnyUDP;
	estate->interconnect_context->SendEos = SendEosUDP;
	estate->interconnect_context->SendChunk = SendChunkUDP;
	estate->interconnect_context->doSendStopMessage = doSendStopMessageUDP;

	mySlice = (Slice *) list_nth(estate->interconnect_context->sliceTable->slices, LocallyExecutingSliceIndex(estate));

	Assert(mySlice &&
		   IsA(mySlice, Slice) &&
		   mySlice->sliceIndex == LocallyExecutingSliceIndex(estate));

#ifdef USE_ASSERT_CHECKING
	set_test_mode();
#endif

	gp_interconnect_id = estate->interconnect_context->sliceTable->ic_instance_id;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (cursor_ic_history_count > (2 * CURSOR_IC_TABLE_SIZE))
		{
			uint32 prune_id = gp_interconnect_id - CURSOR_IC_TABLE_SIZE;

			/* only prune if we didn't underflow -- also we want the prune id to be newer than the limit (hysteresis) */
			if (prune_id < gp_interconnect_id)
			{
				prune_cursor_ic_entry(prune_id);
			}
		}
		add_cursor_ic_entry(gp_interconnect_id, gp_command_count);
	}

	pthread_mutex_lock(&rx_thread_mutex);

	/* rearrange the connection hash table, if required */
	if (conn_htab_size != DEFAULT_CONN_HTAB_SIZE)
	{
		if (!conn_rehash())
		{
			pthread_mutex_unlock(&rx_thread_mutex);

			elog(ERROR, "Could not resize connection hash table allocation failed");
			/* not reached */
		}
	}

	/* now we'll do some setup for each of our Receiving Motion Nodes. */
	foreach(cell, mySlice->children)
	{
		int			numProcs;
		int			childId = lfirst_int(cell);
		ChunkTransportStateEntry *pEntry=NULL;
		int numValidProcs = 0;

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "Setting up RECEIVING motion node %d", childId);
#endif

		aSlice = (Slice *) list_nth(estate->interconnect_context->sliceTable->slices, childId);
		numProcs = list_length(aSlice->primaryProcesses);

#ifdef AMS_VERBOSE_LOGGING
		elog(LOG, "Setup recving connections: my slice %d, childId %d",
			 mySlice->sliceIndex, childId);
#endif

		pEntry = createChunkTransportState(estate->interconnect_context, aSlice, mySlice, numProcs);

		Assert(pEntry);
		Assert(pEntry->valid);

		for (i=0; i < pEntry->numConns; i++)
		{
			conn = &pEntry->conns[i];
			conn->cdbProc = list_nth(aSlice->primaryProcesses, i);

			if (conn->cdbProc)
			{
				numValidProcs++;

				/* rx_buffer_queue */
				conn->pkt_q_size = 0;
				conn->pkt_q_head = 0;
				conn->pkt_q_tail = 0;
				conn->pkt_q = (uint8 **)malloc(Gp_interconnect_queue_depth * sizeof(char *));

				if (!conn->pkt_q)
				{
					pthread_mutex_unlock(&rx_thread_mutex);

					ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
									errmsg("Setup UDP interconnect failed: out of memory")));
					/* not reached */
				}

				/* connection header info (defining characteristics of this connection) */
				MemSet(&conn->conn_info, 0, sizeof(conn->conn_info));
				conn->route = i;

				if (gp_interconnect_elide_setup && gp_interconnect_id > 1)
				{
					conn->conn_info.seq = 1;
					conn->state = mcsStarted;
					conn->stillActive = true;

					incoming_count++;
				}

				conn->conn_info.motNodeId = pEntry->motNodeId;
				conn->conn_info.recvSliceIndex = mySlice->sliceIndex;
				conn->conn_info.sendSliceIndex = aSlice->sliceIndex;

				conn->conn_info.srcContentId = conn->cdbProc->contentid;
				conn->conn_info.dstContentId = Gp_segment;

				conn->conn_info.srcListenerPort = conn->cdbProc->listenerPort;
				conn->conn_info.srcPid = conn->cdbProc->pid;
				conn->conn_info.dstPid = MyProcPid;
				conn->conn_info.dstListenerPort = (Gp_listener_port>>16) & 0x0ffff;
				conn->conn_info.sessionId = gp_session_id;
				conn->conn_info.icId = gp_interconnect_id;

				/* If we're doing full interconnect setup*/
				if (!gp_interconnect_elide_setup || gp_interconnect_id <= 1)
				{
					estate->interconnect_context->incompleteConns = lappend(estate->interconnect_context->incompleteConns, (void *)conn);
					if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
						ereport(DEBUG1, (errmsg("Incomplete incoming interconnect connection (content/pid): %d/%d -> %d/%d",
												conn->conn_info.srcContentId, conn->conn_info.srcPid,
												conn->conn_info.dstContentId, conn->conn_info.dstPid)));
				}

				if (!conn_addhash(conn))
				{
					pthread_mutex_unlock(&rx_thread_mutex);

					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("Interconnect error: could not allocate UDP-IC hash, out of memory."),
									errdetail("%m%s", "malloc")));
					/* not reached */
				}
			}
		}

		expectedTotalIncoming += numValidProcs;

		/* let cdbmotion now how many receivers to expect. */
		setExpectedReceivers(estate->motionlayer_context, childId, numValidProcs);

		/* update the length of our rx freelist */
		rx_buf_maxcount += (pEntry->numConns * Gp_interconnect_queue_depth);
	}

	/* Initiate outgoing connections. */
	if (mySlice->parentIndex != -1)
		sendingChunkTransportState = startOutgoingUDPConnections(estate->interconnect_context, mySlice, &expectedTotalOutgoing);

	pthread_mutex_unlock(&rx_thread_mutex);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		ereport(DEBUG1, (errmsg("SetupUDPInterconnect will activate "
								"%d incoming, %d outgoing routes for gp_interconnect_id %d. "
								"Listening on ports=%d/%d sockfd=%d.",
								expectedTotalIncoming, expectedTotalOutgoing, gp_interconnect_id,
								Gp_listener_port&0x0ffff, (Gp_listener_port>>16)&0x0ffff, UDP_listenerFd)));

	gp_set_monotonic_begin_time(&startTime);

	if (interconnect_setup_timeout > 0)
		elapsed_timeout = interconnect_setup_timeout * 1000;

	/*
	 * Loop until all connections are completed or time limit is exceeded.
	 *
	 * The UDP-IC works slightly differently, above we set up the
	 * descriptors for the inbound connections, so we don't have to do
	 * any *work* how for the inbounds -- we just have to count them
	 * (so the loop below is broken in two, one for outbound and one
	 * for inbound).
	 */
	while (outgoing_count < expectedTotalOutgoing)
	{
		int		timeout;
		uint64	timeout_ms = 20*60*1000;
		struct pollfd nfd;

		/* check for cancellation */
		ML_CHECK_FOR_INTERRUPTS(false);

		iteration++;

		/* send message out, if we haven't already */
		/* loop over outbound connections sending registration messages as needed */
		n = sendingChunkTransportState ? sendingChunkTransportState->numConns : 0;

		for (i = 0; i < n; i++)
		{						/* loop to set up outgoing connections */
			conn = &sendingChunkTransportState->conns[i];

			if ((!conn->cdbProc) || (conn->state == mcsStarted))
				continue;

			/* if we're skipping setup, we don't have to bother with the wait-loop */
			if (gp_interconnect_elide_setup && gp_interconnect_id > 1)
			{
				setupOutgoingUDPConnection(estate->interconnect_context, sendingChunkTransportState, (MotionConn *)conn);
				outgoing_count++;
				continue;
			}

			/* Time to (re)connect? */
			if (conn->state == mcsSetupOutgoingConnection &&
				conn->wakeup_ms <= elapsed_ms + 1)
			{
				setupOutgoingUDPConnection(estate->interconnect_context, sendingChunkTransportState, (MotionConn *)conn);
				switch (conn->state)
				{
					case mcsSetupOutgoingConnection:
						/* Set time limit for connect() to complete. */
						conn->wakeup_ms = TIMEOUT(iteration) + elapsed_ms;
						break;
					default:
						conn->wakeup_ms = 0;
						break;
				}
			}

			if (conn->wakeup_ms > 0)
				timeout_ms = Min(timeout_ms, conn->wakeup_ms - elapsed_ms);
		}	  /* loop to set up outgoing connections */

		/* if we're not going to wait for incoming; we want to wait on acks. */
		timeout = Min(timeout_ms, ((interconnect_setup_timeout*1000) - elapsed_ms));
		timeout = Min(100, timeout);
		if (timeout < 0)
			timeout = 0;

		if (iteration > 1)
		{
			nfd.fd = sendingChunkTransportState->txfd;
			nfd.events = POLLIN;

			n = poll(&nfd, 1, timeout);
		}
		else
			n = 0;

		if (n < 0)
		{
			if (errno == EINTR)
				continue;

			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error: %m%s", "poll")));
		}
		if (n == 1 && (nfd.events & POLLIN))
		{
			uint32 short_packet_count = 0;
			for (;;)
			{
				/* read ack */
				struct sockaddr_storage peer;
				socklen_t peerlen;

				struct icpkthdr pkt;
				int read_count;

				peerlen = sizeof(peer);
				read_count = recvfrom(sendingChunkTransportState->txfd,
									  (char *)&pkt, sizeof(pkt), 0, (struct sockaddr *)&peer, &peerlen);

				if (read_count < 0)
				{
					if (errno == EWOULDBLOCK)
						break;

					if (errno == EINTR)
						continue;

					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("Interconnect error: %m%s", "recvfrom")));
				}

#ifdef USE_ASSERT_CHECKING
				if (testmode_drop(gp_udpic_fault_inject_percent))
					read_count = 0;
#endif

				if (read_count < sizeof(pkt))
				{
					handleShortRead(++short_packet_count, read_count);
					continue;
				}
				/*
				 * NOTE: we don't have a connection here, yet.
				 */
				if (pkt.flags & UDPIC_FLAGS_STOP)
				{
					elog(DEBUG1, "during setup received ack with STOP. node %d srcContent %d srcPid %d pktseq %d pkt->icId %d my->icId %d",
						 pkt.motNodeId, pkt.srcContentId, pkt.srcPid, pkt.seq, pkt.icId, gp_interconnect_id);
				}

				conn = findConnByHeader(&pkt);
				if (conn != NULL)
				{
					if (conn->state == mcsSetupOutgoingConnection)
					{
						conn->state = mcsStarted;
						conn->msgPos = NULL;
						conn->msgSize = sizeof(conn->conn_info);
						conn->stillActive = true;
						conn->conn_info.seq++;
						Assert(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6 );

						outgoing_count++;
					}
					else
						elog(DEBUG1, "state not waiting for setup node %d route %d state %d conn %p pktflags %d",
							 pkt.motNodeId, conn->route, conn->state, conn, pkt.flags);
				}
				else
				{
					char		remoteHostAndPort[128];

					format_sockaddr((struct sockaddr *) &peer, remoteHostAndPort, sizeof(remoteHostAndPort));

					elog(DEBUG3, "Got ack for unknown connection node %d command %d dstpid %d: %s",
						 pkt.motNodeId, pkt.icId, pkt.dstPid, remoteHostAndPort);
				}

				/* if we have more available, don't wait at all --
				 * we'll wait once we run out of acks */
			} /* for (;;) */
		}
		/* done with outbound (for this iteration) */

		/* The UDP interconnect doesn't *do* anything but count
		 * completed connections for the inbound side. So we can just
		 * continue, unless we've timed out. */
		if (elapsed_timeout && outgoing_count < expectedTotalOutgoing)
		{
			if (elapsed_timeout <= elapsed_ms + 20)
			{
				/* timed out */
				break;
			}
		}
		elapsed_ms = gp_get_elapsed_ms(&startTime);
	}

	/* we've either timed out, or we're done. Time to count inbound. */
	do
	{
		/* check for cancellation */
		ML_CHECK_FOR_INTERRUPTS(false);

		/* now do inbound */
		if (incoming_count < expectedTotalIncoming &&
			list_length(estate->interconnect_context->incompleteConns) != 0)
		{
			/* If the rx-thread is waiting we're never going to finish. */
			if (rx_thread_waiting)
			{
				/* deadlock, out of memory! */
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Interconnect error: UDP-IC out of memory.")));
			}

			/* We can't use foreach() because we're going to delete elements as we go */
			cell = list_head(estate->interconnect_context->incompleteConns);
			while (cell != NULL)
			{
				conn = (volatile MotionConn *)lfirst(cell);

				cell = lnext(cell);

				pthread_mutex_lock(&rx_thread_mutex);
				if (conn->state != mcsStarted)
				{
					set_main_thread_waiting(conn->conn_info.motNodeId, conn->route, gp_interconnect_id);

					if (wait_on_receiver(MAIN_THREAD_COND_TIMEOUT))
					{
						/* Got what we wanted. */
					}
					else
					{
						/* timed out waiting. */
					}

					reset_main_thread_waiting();
				}

				/* Did we get what we wanted ? */
				if (conn->state == mcsStarted)
				{
					incoming_count++;
					estate->interconnect_context->incompleteConns = list_delete_ptr(estate->interconnect_context->incompleteConns, (void *)conn);
					if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
						ereport(DEBUG1, (errmsg("Remove incomplete incoming interconnect connection (content/pid): %d/%d -> %d/%d",
												conn->conn_info.srcContentId, conn->conn_info.srcPid,
												conn->conn_info.dstContentId, conn->conn_info.dstPid)));
				}
				pthread_mutex_unlock(&rx_thread_mutex);
			}
		}

		/* Break out of select() loop if completed all connections. */
		if (outgoing_count == expectedTotalOutgoing &&
			incoming_count == expectedTotalIncoming)
			break;

		/*
		 * Been here long?	Bail if gp_interconnect_setup_timeout exceeded.
		 */
		if (interconnect_setup_timeout > 0)
		{
			int to = interconnect_setup_timeout * 1000;

			if (to <= elapsed_ms + 20)
			{
				/*
				 * If we're in verbose mode dump out at most one timed
				 * out connection.	In debug mode, dump out the
				 * details of the interconnect setup failure.
				 */
				if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
				{
					if (outgoing_count != expectedTotalOutgoing)
					{
						n = sendingChunkTransportState ? sendingChunkTransportState->numConns : 0;
						for (i = 0; i < n; i++)
						{
							conn = &sendingChunkTransportState->conns[i];

							if (conn->state == mcsStarted)
								continue;

							elog(LOG, "Outbound connection timed out (content/pid) %d/%d -> %d/%d",
								 conn->conn_info.srcContentId, conn->conn_info.srcPid,
								 conn->conn_info.dstContentId, conn->conn_info.dstPid);

							if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
								continue;
							else
								break;
						}
					}
					if (incoming_count != expectedTotalIncoming &&
						list_length(estate->interconnect_context->incompleteConns) != 0)
					{
						foreach(cell, estate->interconnect_context->incompleteConns)
						{
							conn = (MotionConn *) lfirst(cell);

							if (conn->state == mcsStarted)
								continue;

							elog(LOG, "Inbound connection timed out (content/pid) %d/%d -> %d/%d",
								 conn->conn_info.srcContentId, conn->conn_info.srcPid,
								 conn->conn_info.dstContentId, conn->conn_info.dstPid);

							if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
								continue;
							else
								break;
						}
					}
				}

				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Interconnect timeout: Unable to "
									   "complete setup of all connections "
									   "within time limit."),
								errdetail("Completed %d of %d incoming and "
										  "%d of %d outgoing connections.  "
										  "gp_interconnect_setup_timeout = %d "
										  "seconds.",
										  incoming_count, expectedTotalIncoming,
										  outgoing_count, expectedTotalOutgoing,
										  interconnect_setup_timeout)));
			}
		}

		/* Back to top of loop and look again. */
		elapsed_ms = gp_get_elapsed_ms(&startTime);
	} while (1);

	estate->interconnect_context->activated = true;

	if (gp_log_interconnect >= GPVARS_VERBOSITY_TERSE)
	{
		elapsed_ms = gp_get_elapsed_ms(&startTime);
		if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE ||
			elapsed_ms >= 0.1 * 1000 * interconnect_setup_timeout)
			elog(LOG, "SetupUDPInterconnect+" UINT64_FORMAT "ms: Activated %d incoming, "
				 "%d outgoing routes.",
				 elapsed_ms, incoming_count, outgoing_count);
	}
}

void
TeardownUDPInterconnect(ChunkTransportState *transportStates,
						MotionLayerState *mlStates,
						bool forceEOS)
{
	ChunkTransportStateEntry *pEntry=NULL;
	int			i;
	Slice	   *mySlice;
	volatile MotionConn *conn;

	if (transportStates->sliceTable == NULL)
	{
		elog(LOG, "TeardownUDPInterconnect: missing slice table.");
		return;
	}

	mySlice = (Slice *) list_nth(transportStates->sliceTable->slices, transportStates->sliceId);

	if (forceEOS)
		HOLD_INTERRUPTS();

	/* Log the start of TeardownInterconnect. */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_TERSE)
	{
		int		elevel = 0;

		if (forceEOS || !transportStates->activated)
		{
			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elevel = LOG;
			else
				elevel = DEBUG1;
		}
		else if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elevel = DEBUG4;

		if (elevel)
			ereport(elevel, (errmsg("Interconnect seg%d slice%d cleanup state: "
								 "%s; setup was %s",
								 Gp_segment, mySlice->sliceIndex,
								 forceEOS ? "force" : "normal",
								 transportStates->activated ? "completed" : "exited")));

		/* if setup did not complete, log the slicetable */
		if (!transportStates->activated &&
			gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog_node_display(DEBUG3, "local slice table", transportStates->sliceTable, true);
	}

	/* remove any "extra" incompleteConns ? (these are only used by
	 * one thread, no lock required) -- we're removing elements as we
	 * go, so don't use foreach() () */
	if (list_length(transportStates->incompleteConns) != 0)
	{
		ListCell *cell = list_head(transportStates->incompleteConns);
		while (cell != NULL)
		{
			conn = (volatile MotionConn *) lfirst(cell);

			cell = lnext(cell);

			transportStates->incompleteConns =
				list_delete_ptr(transportStates->incompleteConns, (void *)conn);
		}
	}

	/*
	 * Now "normal" connections which made it through our
	 * peer-registration step. With these we have to worry about
	 * "in-flight" data.
	 */
	if (mySlice->parentIndex != -1)
	{
		Slice	   *parentSlice;

		parentSlice = (Slice *) list_nth(transportStates->sliceTable->slices, mySlice->parentIndex);

		/* cleanup a Sending motion node. */
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG3, "Interconnect seg%d slice%d closing connections to slice%d (%d peers)",
				 Gp_segment, mySlice->sliceIndex, mySlice->parentIndex,
				 list_length(parentSlice->primaryProcesses));

		/*
		 * In the olden days, we required that the error case
		 * successfully transmit and end-of-stream message here. But
		 * the introduction of cdbdisp_check_estate_for_cancel()
		 * alleviates for the QD case, and the cross-connection of
		 * primary/mirror writer gangs in the dispatcher (propagation
		 * of cancel between them) fixes the I-S case.
		 *
		 * So the call to forceEosToPeers() is no longer required.
		 */

		/* now it is safe to remove. */
		pEntry = removeChunkTransportState(transportStates, mySlice->sliceIndex);

		closesocket(pEntry->txfd);
		pEntry->txfd = -1;
		pEntry->txfd_family = 0;

		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			conn_delhash(conn);

			conn->pBuff = NULL;
			conn->activeXmit = NULL;

			if (conn->ping)
			{
				pfree(conn->ping);
				conn->ping = NULL;
			}
			if (conn->pong)
			{
				pfree(conn->pong);
				conn->pong = NULL;
			}
		}
	}

	/* if we have issued *any* stop requests, we may have peers which
	 * we will mess up if we just start ignoring them (they depend on
	 * our ability to acknowledge their final messages) */
	if (!forceEOS)
	{
		List *pendingStop = NIL;
		ListCell   *cell;

		foreach(cell, mySlice->children)
		{
			Slice	*aSlice;
			int		childId = lfirst_int(cell);

			aSlice = (Slice *) list_nth(transportStates->sliceTable->slices, childId);

			getChunkTransportState(transportStates, aSlice->sliceIndex, &pEntry);
			Assert(pEntry);

			pthread_mutex_lock(&rx_thread_mutex);
			for (i = 0; i < pEntry->numConns; i++)
			{
				conn = pEntry->conns + i;

				/* We'll make a first pass at freeing buffer space
				 * here. (we can't wait (see below) until we've freed
				 * up some buffers -- if we do we may deadlock) */
				if (conn->pBuff != NULL)
				{
					put_rx_buffer(conn); /* as a side-effect this may wake the rx_thread */
				}

				if (conn->stillActive)
				{
					pendingStop = lappend(pendingStop, (void *)conn);
				}
			}
			pthread_mutex_unlock(&rx_thread_mutex);
		}

		elog(DEBUG3, "TeardownUDP: %d pending stops", list_length(pendingStop));

		pthread_mutex_lock(&rx_thread_mutex);

		while (list_length(pendingStop) > 0)
		{
			ListCell *cell = list_head(pendingStop);
			while (cell != NULL)
			{
				conn = (volatile MotionConn *)lfirst(cell);

				cell = lnext(cell);
				
				if (!conn->stillActive || conn->eosAcked)
				{
					/* remove from the list */
					pendingStop = list_delete_ptr(pendingStop, (void *)conn);
					conn->stillActive = false;
					continue;
				}
				
				set_main_thread_waiting(conn->conn_info.motNodeId, ANY_ROUTE,
										transportStates->sliceTable->ic_instance_id);
				
				if (wait_on_receiver(MAIN_THREAD_COND_TIMEOUT))
				{
					reset_main_thread_waiting();
					
					/* success, remove list (as above) */
					pendingStop = list_delete_ptr(pendingStop, (void *)conn);
					conn->stillActive = false;
					continue;
				}
				else
				{
					reset_main_thread_waiting();
					
					pthread_mutex_unlock(&rx_thread_mutex);
					
					/* if requested we want to bail-out to handle errors,
					 * so we use "false" */
					ML_CHECK_FOR_INTERRUPTS(false);
					
					/* if we made it here we need the lock again. */
					pthread_mutex_lock(&rx_thread_mutex);
				}
			}
		}
		
		pthread_mutex_unlock(&rx_thread_mutex);
	}

	/*
	 * cleanup all of our Receiving Motion nodes, these get closed
	 * immediately (the receiver know for real if they want to shut
	 * down -- they aren't going to be processing any more data).
	 */
	ListCell   *cell;
	foreach(cell, mySlice->children)
	{
		Slice	*aSlice;
		int		childId = lfirst_int(cell);

		aSlice = (Slice *) list_nth(transportStates->sliceTable->slices, childId);

		getChunkTransportState(transportStates, aSlice->sliceIndex, &pEntry);
		Assert(pEntry);

		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG3, "Interconnect closing connections from slice%d",
				 aSlice->sliceIndex);

		pthread_mutex_lock(&rx_thread_mutex);
		/*
		 * receivers know that they no longer care about data from
		 * below ... so we can safely discard data queued in both
		 * directions
		 */
		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			Assert((!conn->stillActive || forceEOS) && "connection is shut down while a receiver is still active");

			conn_delhash(conn);

			/* put_rx_buffer() dequeues messages and moves them to pBuff */
			while (conn->pBuff != NULL)
			{
				put_rx_buffer(conn); /* as a side-effect this may wake the rx_thread */
			}

			/* free up the packet queue */
			free(conn->pkt_q);
		}
		rx_buf_maxcount -= (pEntry->numConns * Gp_interconnect_queue_depth);
		pfree(pEntry->conns);

		/*
		 * we're inside a loop here, which is weird, but we need to
		 * make sure this stuff gets updated before we release the
		 * lock.
		 */
		ic_id_last_teardown = gp_interconnect_id;

		pthread_mutex_unlock(&rx_thread_mutex);

		/* remove it */
		removeChunkTransportState(transportStates, aSlice->sliceIndex);
	}

	pthread_mutex_lock(&rx_thread_mutex);
	/* now that we've moved active rx-buffers to the freelist, we can prune the freelist itself */
	while (rx_buf_count > rx_buf_maxcount)
	{
		struct icpkthdr *buf=NULL;

		if (rx_buf_freelist == NULL)
			elog(LOG, "freelist NULL: count %d max %d buf %p", rx_buf_count, rx_buf_maxcount, rx_buf_freelist);
		Assert(rx_buf_freelist != NULL);

		buf = (struct icpkthdr *)rx_buf_freelist;
		rx_buf_freelist = *(char **)(rx_buf_freelist);

		free(buf);
		rx_buf_count--;
	}

	if (rx_thread_waiting)
	{
		elog(LOG, "During teardownUDP, rx_thread is waiting (waking)");
		pthread_cond_signal(&rx_thread_cond);
	}

	elog((gp_interconnect_log_stats ? LOG : DEBUG1),
		 "TeardownUDPInterconnect: total pkt count " INT64_FORMAT
		 " dropped acks " INT64_FORMAT " retransmits " INT64_FORMAT
		 " conn mismatch " INT64_FORMAT " rx overrun " INT64_FORMAT
		 " crc error" INT64_FORMAT,
		 ic_stats_pkt_count, ic_stats_dropped_acks, ic_stats_retrans,
		 ic_stats_mismatch, ic_stats_rx_overrun, ic_stats_crc_error);

	ic_stats_pkt_count=0;
	ic_stats_dropped_acks=0;
	ic_stats_retrans=0;
	ic_stats_mismatch=0;
	ic_stats_rx_overrun=0;
	ic_stats_crc_error=0;

	update_cursor_ic_entry(gp_interconnect_id, 0);

	pthread_mutex_unlock(&rx_thread_mutex);

	transportStates->activated = false;
	transportStates->sliceTable = NULL;

	if (transportStates != NULL)
	{
		if (transportStates->states != NULL)
			pfree(transportStates->states);
		pfree(transportStates);
	}

	if (forceEOS)
		RESUME_INTERRUPTS();

#ifdef AMS_VERBOSE_LOGGING
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG4, "TeardownUDPInterconnect successful");
#endif
}

/**
 * Should be called from receiveChunksUDP, only for the dispatcher
 *
 * Always called with thread mutex locked.	In case of cancel request, will error out after unlocking the mutex.
 *	  If no cancel is needed then the mutex state is unchanged.
 */
static inline void
checkForCancelFromInsideReceiveChunks(ChunkTransportState *pTransportStates)
{
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(pTransportStates);
	Assert(pTransportStates->estate);

	if (cdbdisp_check_estate_for_cancel(pTransportStates->estate))
	{
		pthread_mutex_unlock(&rx_thread_mutex);

		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg(CDB_MOTION_LOST_CONTACT_STRING)));
		/* not reached */
	}
}

/* ALWAYS CALLED WITH THREAD MUTEX *LOCKED* BEFORE RETURNING, *MUST* UNLOCK. */
TupleChunkListItem
receiveChunksUDP(ChunkTransportState *pTransportStates, ChunkTransportStateEntry *pEntry,
				 int16 motNodeID, int16 *srcRoute, volatile MotionConn *conn, bool inTeardown)
{
	int			retries=0;
	bool		directed=false;
	volatile MotionConn *rxconn = NULL;
	TupleChunkListItem	tcItem=NULL;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "receivechunksUDP: motnodeid %d", motNodeID);
#endif

	Assert(pTransportStates);
	Assert(pTransportStates->sliceTable);

	/* directed receive ? */
	if (conn != NULL)
	{
		directed = true;
		*srcRoute = conn->route;
		set_main_thread_waiting(motNodeID, conn->route,
								pTransportStates->sliceTable->ic_instance_id);
	}
	else
	{
		/* non-directed receive */
		set_main_thread_waiting(motNodeID, ANY_ROUTE,
								pTransportStates->sliceTable->ic_instance_id);
	}

	/* we didn't have any data, so we've got to read it from the network. */
	for (;;)
	{
		ML_CHECK_FOR_INTERRUPTS(inTeardown);
		
		/* 1. Do we have data ready */
		if (directed && conn->pBuff)
		{
			rxconn = conn;
		}
		else if (!directed && main_thread_waiting_route != ANY_ROUTE)
		{
			rxconn = pEntry->conns + main_thread_waiting_route;
#ifdef AMS_VERBOSE_LOGGING
			elog(DEBUG3, "receiveChunksUDP: non-directed rx woke on route %d (%p)", main_thread_waiting_route, rxconn->pBuff);
#endif
		}

		aggregateStatistics(pEntry);

		if (rxconn != NULL)
		{
#ifdef AMS_VERBOSE_LOGGING
			elog(LOG, "got data with length %d", rxconn->recvBytes);
#endif
			Assert(rxconn->pBuff);

			/* successfully read into this connection's buffer. */
			tcItem = RecvTupleChunk((MotionConn *)rxconn, inTeardown);

			if (!directed)
				*srcRoute = rxconn->route;

			reset_main_thread_waiting();

			pthread_mutex_unlock(&rx_thread_mutex);

			return tcItem;
		}

		/* every 15 or 16 wait-intervals, check to see if the dispatcher should cancel */
		if (Gp_role == GP_ROLE_DISPATCH && (retries & 0x0f) == 0)
		{
			checkForCancelFromInsideReceiveChunks(pTransportStates);
		}

		retries++;

		/* 2. Wait for data to become ready */
		if (wait_on_receiver(MAIN_THREAD_COND_TIMEOUT))
			continue; /* success ! */

		/* waited a long time, might as well check for cancel now */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			checkForCancelFromInsideReceiveChunks(pTransportStates);
		}

		/* handle timeout, check for cancel */
		pthread_mutex_unlock(&rx_thread_mutex);

		ML_CHECK_FOR_INTERRUPTS(inTeardown);

		if ((retries & 0x3f) == 0)
		{
			if (!dispatcherAYT())
			{
				if (Gp_role == GP_ROLE_EXECUTE)
					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("Interconnect error segment lost contact with master (recv)")));
				else
					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("Interconnect error master lost contact with client (recv)")));
			}
		}

		if (rx_thread_waiting)
		{
			/* deadlock, out of memory! */
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error: UDP-IC out of memory.")));
		}

		/* if we made it here we need the lock again. */
		pthread_mutex_lock(&rx_thread_mutex);
	} /* for (;;) */

	/* We either got data, or get cancelled. We never make it out to
	 * here. */
	elog(FATAL, "receiveChunksUDP: not cancelled, but didn't receive.");
	return NULL; /* make GCC behave */
}

static TupleChunkListItem
RecvTupleChunkFromAnyUDP(MotionLayerState *mlStates,
						 ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 *srcRoute)
{
	ChunkTransportStateEntry	*pEntry = NULL;
	volatile MotionConn			*conn=NULL;
	int					i, index, activeCount=0;
	TupleChunkListItem	tcItem=NULL;

	if (!transportStates)
	{
		elog(FATAL, "RecvTupleChunkFromAnyUDP: missing context");
	}
	else if (!transportStates->activated)
	{
		elog(FATAL, "RecvTupleChunkFromAnyUDP: interconnect context not active!");
	}

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	index = pEntry->scanStart;

	pthread_mutex_lock(&rx_thread_mutex);
	for (i = 0; i < pEntry->numConns; i++, index++)
	{
		if (index >= pEntry->numConns)
			index = 0;

		conn = pEntry->conns + index;

		if (conn->stillActive)
			activeCount++;

		if (conn->pBuff != NULL)
		{
			tcItem = RecvTupleChunk((MotionConn *)conn, transportStates->teardownActive);
			*srcRoute = conn->route;

			pthread_mutex_unlock(&rx_thread_mutex);

			pEntry->scanStart = index + 1;

			return tcItem;
		}
	}

	/* no data pending in our queue */

#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "recvtuplechunkfromanyudp(): activeCount is %d", activeCount);
#endif
	if (activeCount == 0)
		return NULL;

	/* receiveChunksUDP() releases rx_thread_mutex as a side-effect */
	tcItem = receiveChunksUDP(transportStates, pEntry, motNodeID, srcRoute, NULL, transportStates->teardownActive);

	pEntry->scanStart = *srcRoute + 1;

	return tcItem;
}

static TupleChunkListItem
RecvTupleChunkFromUDP(ChunkTransportState *transportStates,
					  int16		motNodeID,
					  int16		srcRoute)
{
	ChunkTransportStateEntry	*pEntry = NULL;
	volatile MotionConn			*conn=NULL;
	int16				route;

	if (!transportStates)
	{
		elog(FATAL, "RecvTupleChunkFromUDP: missing context");
	}
	else if (!transportStates->activated)
	{
		elog(FATAL, "RecvTupleChunkFromUDP: interconnect context not active!");
	}

#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "recvtuplechunkfromudp().");
#endif

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "RecvTupleChunkFromUDP(motNodID=%d, srcRoute=%d)", motNodeID, srcRoute);
#endif

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	conn = pEntry->conns + srcRoute;

#ifdef AMS_VERBOSE_LOGGING
	if (!conn->stillActive)
	{
		elog(LOG, "recvtuplechunkfromudp(): connection inactive ?!");
	}
#endif

	if (!conn->stillActive)
		return NULL;

	pthread_mutex_lock(&rx_thread_mutex);
	if (conn->pBuff != NULL)
	{
		TupleChunkListItem	tcItem=NULL;

		tcItem = RecvTupleChunk((MotionConn *)conn, transportStates->teardownActive);

		pthread_mutex_unlock(&rx_thread_mutex);
		return tcItem;
	}

	/* no existing data, we've got to read a packet */
	/* receiveChunksUDP() releases rx_thread_mutex as a side-effect */
	return receiveChunksUDP(transportStates, pEntry, motNodeID, &route, conn, transportStates->teardownActive);
}

void
markUDPConnInactive(MotionConn *conn)
{
	pthread_mutex_lock(&rx_thread_mutex);
	conn->stillActive = false;
	pthread_mutex_unlock(&rx_thread_mutex);

	return;
}

static void
aggregateStatistics(ChunkTransportStateEntry *pEntry)
{
	/*
	 * We first clear the stats, and then compute new stats
	 * by aggregating the stats from each connection.
	 */
	pEntry->stat_total_ack_time = 0;
	pEntry->stat_count_acks = 0;
	pEntry->stat_max_ack_time = 0;
	pEntry->stat_min_ack_time = ~((uint64)0);
	pEntry->stat_count_resent = 0;
	pEntry->stat_max_resent = 0;
	pEntry->stat_count_dropped = 0;

	int connNo;
	for (connNo = 0; connNo < pEntry->numConns; connNo++)
	{
		MotionConn *conn = &pEntry->conns[connNo];

		pEntry->stat_total_ack_time += conn->stat_total_ack_time;
		pEntry->stat_count_acks += conn->stat_count_acks;
		pEntry->stat_max_ack_time = Max(pEntry->stat_max_ack_time, conn->stat_max_ack_time);
		pEntry->stat_min_ack_time = Min(pEntry->stat_min_ack_time, conn->stat_min_ack_time);
		pEntry->stat_count_resent += conn->stat_count_resent;
		pEntry->stat_max_resent = Max(pEntry->stat_max_resent, conn->stat_max_resent);
		pEntry->stat_count_dropped += conn->stat_count_dropped;
	}
}

/*
 * handle acks incoming from our upstream peers.
 *
 * if we receive a stop message, return true (caller will clean up).
 */
static bool
handleAcks(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry)
{
	bool ret=false;
	volatile MotionConn *ackConn;
	int n;

	struct sockaddr_storage peer;
	socklen_t peerlen;

	struct icpkthdr pkt;

	uint32 short_packet_count = 0;

	for (;;)
	{
		/* ready to read on our socket ? */
		peerlen = sizeof(peer);
		n = recvfrom(pEntry->txfd, (char *)&pkt, sizeof(pkt), 0,
					 (struct sockaddr *)&peer, &peerlen);

#ifdef USE_ASSERT_CHECKING
		if (testmode_drop(gp_udpic_fault_inject_percent))
			n = 0;
#endif

		if (n < 0)
		{
			if (errno == EWOULDBLOCK) /* had nothing to read. */
			{
				aggregateStatistics(pEntry);
				return ret;
			}

			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
			if (errno == EINTR)
				continue;

			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error waiting for peer ack"),
							errdetail("During recvfrom() call.\n")));
			/* not reached */
		}
		else if (n < sizeof(pkt))
		{
			handleShortRead(++short_packet_count, n);
			continue;
		}

		/* read packet, is this the ack we want ? */
		if (pkt.srcContentId == Gp_segment &&
			pkt.srcPid == MyProcPid &&
			pkt.srcListenerPort == ((Gp_listener_port>>16) & 0x0ffff) &&
			pkt.sessionId == gp_session_id &&
			pkt.icId == gp_interconnect_id)
		{
			/* packet is for me */

			ackConn = findConnByHeader(&pkt);

			if (ackConn == NULL)
			{
				elog(LOG, "Received ack for unknown connection (flags 0x%x)", pkt.flags);
				continue;
			}

			if (pkt.seq != ackConn->conn_info.seq)
			{
				if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
					elog(DEBUG2, "ack with bad seq?! expected %d got %d flags 0x%x", ackConn->conn_info.seq, pkt.seq, pkt.flags);

				continue;
			}

			if ((pkt.flags & UDPIC_FLAGS_NAK) && (pkt.flags & UDPIC_FLAGS_EOS))
			{
				elog(DEBUG2, "GOT NAK FOR EOS: srcpid %d dstpid %d srccontent %d dstcontent %d cmd %d flags 0x%x pktseq %d connseq %d gp_interconnect_id %d",
					 pkt.srcPid, pkt.dstPid, pkt.srcContentId, pkt.dstContentId, pkt.icId, pkt.flags, pkt.seq, ackConn->conn_info.seq, gp_interconnect_id);
			}

			if (pkt.flags & UDPIC_FLAGS_NAK)
			{
				/* this packet needs to get retried -- the receiver isn't ready to receive it yet. */
				elog(DEBUG2, "got NAK; srcpid %d dstpid %d srccontent %d dstcontent %d cmd %d flags 0x%x pktseq %d connseq %d",
					 pkt.srcPid, pkt.dstPid, pkt.srcContentId, pkt.dstContentId, pkt.icId, pkt.flags, pkt.seq, ackConn->conn_info.seq);
				continue;
			}

			/*
			 * Has this connection already received a stop request ?
			 * once we processed the first stop request, we are
			 * waiting for the ack of our ack (which will have EOS
			 * set). Ignore anything else.
			 */
			if (ackConn->stopRequested)
			{
				if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
					elog(DEBUG2, "got ack-stop ack; srcpid %d dstpid %d cmd %d flags 0x%x pktseq %d connseq %d", pkt.srcPid, pkt.dstPid, pkt.icId, pkt.flags, pkt.seq, ackConn->conn_info.seq);

				if (pkt.flags & UDPIC_FLAGS_EOS)
				{
					ackConn->conn_info.seq++;
					if (ackConn->activeXmit)
						ackConn->activeXmit = NULL;
				}
				continue;
			}

			/* haven't gotten a stop request, maybe this is one ? */
			if (pkt.flags & UDPIC_FLAGS_STOP)
			{
				if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
					elog(DEBUG2, "got ack with stop; srcpid %d dstpid %d cmd %d flags 0x%x pktseq %d connseq %d", pkt.srcPid, pkt.dstPid, pkt.icId, pkt.flags, pkt.seq, ackConn->conn_info.seq);

				ackConn->stopRequested = true;
				ackConn->conn_info.flags |= UDPIC_FLAGS_STOP;
				ret = true;

				ackConn->conn_info.seq++;
				if (ackConn->activeXmit)
					ackConn->activeXmit = NULL;

				continue;
			}

			/* this isn't a stop request. it must be a regular ack. */
			if (pkt.flags & UDPIC_FLAGS_ACK)
			{
				ackConn->conn_info.seq++;

				if (ackConn->activeXmit)
					ackConn->activeXmit = NULL;

				ackConn->stat_count_acks++;

				/* Compute the wait time for the Ack */
				uint64 waitTime = gp_get_elapsed_us(&(((MotionConn *)ackConn)->ackWaitBeginTime));
				ackConn->stat_total_ack_time += waitTime;
				ackConn->stat_max_ack_time = Max(waitTime, ackConn->stat_max_ack_time);
				ackConn->stat_min_ack_time = Min(waitTime, ackConn->stat_min_ack_time);

				if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
					elog(DEBUG5, "got good ack with seq %d node %d route %d peer %d", pkt.seq, pkt.motNodeId, ackConn->route, pkt.dstPid);
				continue;
			}
		}
		else
			elog(DEBUG4, "handleAcks: not the ack we're looking for (flags 0x%x)...mot(%d) content(%d:%d) srcpid(%d:%d) dstpid(%d) srcport(%d:%d) dstport(%d) sess(%d:%d) cmd(%d:%d)",
				 pkt.flags, pkt.motNodeId,
				 pkt.srcContentId, Gp_segment,
				 pkt.srcPid, MyProcPid,
				 pkt.dstPid,
				 pkt.srcListenerPort, ((Gp_listener_port>>16) & 0x0ffff),
				 pkt.dstListenerPort,
				 pkt.sessionId, gp_session_id,
				 pkt.icId, gp_interconnect_id);
	}

	return ret;
}

static inline void
prepareXmit(MotionConn *conn)
{
	Assert(conn != NULL);
	Assert(conn->pBuff != NULL);

	conn->conn_info.len = conn->msgSize;
	conn->conn_info.crc = 0;

	memcpy(conn->pBuff, &conn->conn_info, sizeof(conn->conn_info));

	if (gp_interconnect_full_crc)
	{
		struct icpkthdr *pkt = (struct icpkthdr *)conn->pBuff;
		pg_crc32 local_crc;

		INIT_CRC32C(local_crc);
		COMP_CRC32C(local_crc, pkt, pkt->len);
		FIN_CRC32C(local_crc);

		pkt->crc = local_crc;
	}

	gp_set_monotonic_begin_time(&conn->activeXmitTime);

	/* set active xmit */
	conn->activeXmit = conn->pBuff;
}


static void
sendOnce(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn * conn)
{
	int			n;

	Assert(conn->activeXmit);

#ifdef USE_ASSERT_CHECKING
	if (testmode_drop(gp_udpic_dropxmit_percent))
	{
		return;
	}
#endif

xmit_retry:
	n = sendto(pEntry->txfd, conn->activeXmit, conn->conn_info.len, 0,
			   (struct sockaddr *)&conn->peer, conn->peer_len);
	if (n < 0)
	{
		ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
		if (errno == EINTR)
			goto xmit_retry;

		if (errno == EAGAIN) /* no space ? not an error. */
			return;

		elog(LOG,"sendto error: txfd = %d, peer_len = %d, fam = %d", pEntry->txfd, conn->peer_len, conn->peer.ss_family);

		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error writing an outgoing packet: %m"),
						errdetail("error during sendto() call (error:%d).\n"
								  "For Remote Connection: contentId=%d at %s",
								  errno, conn->remoteContentId,
								  conn->remoteHostAndPort)));
		/* not reached */
	}

	if (n != conn->conn_info.len)
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error writing an outgoing packet"),
						errdetail("short transmit during sendto() call.\n"
								  "For Remote Connection: contentId=%d at %s",
								  conn->remoteContentId,
								  conn->remoteHostAndPort)));
		/* not reached */
	}

	return;
}

static bool
waitOnXmit(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	uint64	retry = 0;
	int		timeout;
	uint64	elapsed_ms = 0;
	bool	got_stops=false;
	int		n;
	struct pollfd nfd;
	uint64 count_resent = 0;

	/* send this packet */
	for (;;)
	{
		/* MPP-3088: don't get elapsed time on first call */
		if (retry)
			elapsed_ms = gp_get_elapsed_ms(&conn->activeXmitTime);

		timeout = TIMEOUT(retry);

		if (timeout <= elapsed_ms)
		{
			sendOnce(transportStates, pEntry, conn);

			/*
			 * Start the timer of waiting for the Ack if this is the first time
			 * this is called.
			 */
			if (!retry)
			{
				gp_set_monotonic_begin_time(&conn->ackWaitBeginTime);
			}

			if (timeout)
			{
				gp_set_monotonic_begin_time(&conn->activeXmitTime);
			}

			if (retry)
			{
				ic_stats_retrans++;
				count_resent ++;
			}

			retry++;

			if (Gp_role == GP_ROLE_EXECUTE && (retry & 0x3f) == 0)
			{
				if (!dispatcherAYT())
					ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									errmsg("Interconnect error segment lost contact with master (xmit)")));
			}
		}

		timeout = Max(1, timeout);

		nfd.fd = pEntry->txfd;
		nfd.events = POLLIN;

		n = poll(&nfd, 1, timeout);
		if (n < 0)
		{
			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
			if (errno == EINTR)
				continue;

			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error waiting for peer ack"),
							errdetail("During poll() call.\n")));
			/* not reached */
		}

		if (n == 0) /* timeout */
		{
			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
			continue;
		}

		/* got an ack to handle (possibly a stop message) */
		if (n == 1 && (nfd.events & POLLIN))
		{
			if (handleAcks(transportStates, pEntry))
				got_stops = true;

			if (!conn->activeXmit)
				break;
		}
	}

	conn->stat_count_resent += count_resent;
	conn->stat_max_resent = Max(conn->stat_max_resent, count_resent);

	return got_stops;
}

/*
 * This is a helper function for flushUDPBuffer(). We're essentially
 * acknowledging the stop message, and agreeing that the connection is
 * done. Note that as we do this we may be getting more stop messages;
 * which we have to handle too (so we re-scan our list as we get those
 * kind of responses).
 *
 * Called from flushUDPBuffer(), after noticing the return from
 * flushUDPBuffer().
 */
static void
handleStopMsgs(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, int16 motionId)
{
	int i=0;
	bool morestops=false;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG3, "handleStopMsgs: node %d", motionId);
#endif

	while (i < pEntry->numConns)
	{
		MotionConn *conn=NULL;

		conn = pEntry->conns + i;

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG3, "handleStopMsgs: node %d route %d %s %s", motionId, conn->route,
			(conn->stillActive ? "active" : "NOT active"), (conn->stopRequested ? "stop requested" : ""));
		elog(DEBUG3, "handleStopMsgs: node %d route %d msgSize %d", motionId, conn->route, conn->msgSize);
#endif

		/*
		 * MPP-2427: we're guaranteed to have recently flushed, but
		 * this might not be empty (if we got a stop on a buffer that
		 * wasn't the one we were sending) ... empty it first so the
		 * outbound buffer is empty when we get here.
		 */
		if (conn->stillActive && conn->stopRequested)
		{
			/* mark buffer empty */
			conn->tupleCount = 0;
			conn->msgSize = sizeof(conn->conn_info);

			/* now send our stop-ack EOS */
			conn->conn_info.flags |= UDPIC_FLAGS_EOS;
			conn->pBuff[conn->msgSize] = 'S';
			conn->msgSize += 1;

			prepareXmit(conn);

			/* now ready to actually send */
			elog(DEBUG3, "handleStopMsgs: node %d route %d", motionId, i);
			morestops = waitOnXmit(transportStates, pEntry, conn);

			Assert(!conn->activeXmit);

			conn->stillActive = false;
			conn->stopRequested = false;

			if (morestops)
			{
				i=0;
				continue;
			}
		}

		i++;
	}
}

/* The Function sendChunk() is used to send a tcItem to a single
 * destination. Tuples often are *very small* we aggregate in our
 * local buffer before sending into the kernel.
 *
 * PARAMETERS
 *	 conn - MotionConn that the tcItem is to be sent to.
 *	 tcItem - message to be sent.
 *	 motionId - Node Motion Id.
 */
static bool
SendChunkUDP(MotionLayerState *mlStates, ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn, TupleChunkListItem tcItem, int16 motionId)
{
	bool	gotStops=false;
	int		length=TYPEALIGN(TUPLE_CHUNK_ALIGN, tcItem->chunk_length);

	Assert(conn->msgSize > 0);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "sendChunk: msgSize %d this chunk length %d conn seq %d", conn->msgSize, tcItem->chunk_length, conn->conn_info.seq);
#endif


	if (conn->msgSize + length <= Gp_max_packet_size)
	{
		memcpy(conn->pBuff + conn->msgSize, tcItem->chunk_data, tcItem->chunk_length);
		conn->msgSize += length;

		conn->tupleCount++;

		return true;
	}
	else if (conn->activeXmit)
	{
		if (waitOnXmit(transportStates, pEntry, conn))
			gotStops = true;
	}

	Assert(!conn->activeXmit);

	/* prepare this for transmit */
	if (pEntry->sendingEos)
		conn->conn_info.flags |= UDPIC_FLAGS_EOS;

	prepareXmit(conn);

	/* active xmit, advance xmit buffer */
	conn->pBuff = (conn->pBuff == conn->ping ? conn->pong : conn->ping);

	conn->tupleCount = 0;
	conn->msgSize = sizeof(conn->conn_info);

	/* Send the packet */
	sendOnce(transportStates, pEntry, conn);

	/* Start the timer for Ack wait time */
	gp_set_monotonic_begin_time(&conn->ackWaitBeginTime);

	/* handle any acks, but don't wait */
	if (handleAcks(transportStates, pEntry))
		gotStops = true;

	/* if we got a stop */
	if (gotStops)
	{
		/* handle stop messages here */
		handleStopMsgs(transportStates, pEntry, motionId);
	}

	/* now we can copy the input to the new buffer */
	memcpy(conn->pBuff + conn->msgSize, tcItem->chunk_data, tcItem->chunk_length);
	conn->msgSize += length;

	conn->tupleCount++;

	return true;
}

/* See ml_ipc.h */
static void
SendEosUDP(MotionLayerState *mlStates,
		   ChunkTransportState *transportStates,
		   int motNodeID,
		   TupleChunkListItem tcItem)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;
	int			i;
	bool		stopRace=false;

	if (!transportStates)
	{
		elog(FATAL, "SendEosUDP: missing interconnect context.");
	}
	else if (!transportStates->activated && !transportStates->teardownActive)
	{
		elog(FATAL, "SendEosUDP: context and teardown inactive.");
	}

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG3, "Interconnect seg%d slice%d sending end-of-stream to slice%d",
			 Gp_segment, motNodeID, pEntry->recvSlice->sliceIndex);

	pEntry->sendingEos = true;

	/* we want to add our tcItem onto each of the outgoing buffers --
	 * this is guaranteed to leave things in a state where a flush is
	 * *required*.
	 */
	doBroadcast(mlStates, transportStates, pEntry, tcItem, NULL);

	/* now flush all of the buffers. */
	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;

#ifdef AMS_VERBOSE_LOGGING
		elog(LOG, "SendEosUDP: %d, flags %d %s %s %d",
			 i, conn->conn_info.flags, conn->state == mcsStarted ? "started" : "not started", conn->stillActive ? "active" : "not active", conn->stopRequested);
#endif

		if (conn->state == mcsStarted && conn->stillActive)
		{
			elog(DEBUG2, "sent eos to route %d tuplecount %d seq %d flags 0x%x stillActive %s icId %d",
				 conn->route, conn->tupleCount, conn->conn_info.seq, conn->conn_info.flags, (conn->stillActive ? "true" : "false"), conn->conn_info.icId);

			if (conn->activeXmit)
			{
				if (waitOnXmit(transportStates, pEntry, conn))
				{
					handleStopMsgs(transportStates, pEntry, motNodeID);
					if (!conn->stillActive)
						continue;
				}
			}
			Assert(!conn->activeXmit);

			/* prepare this for transmit */
			if (pEntry->sendingEos)
				conn->conn_info.flags |= UDPIC_FLAGS_EOS;

			prepareXmit(conn);

			conn->pBuff = (conn->pBuff == conn->ping ? conn->pong : conn->ping);

			conn->tupleCount = 0;
			conn->msgSize = sizeof(conn->conn_info);

			if (waitOnXmit(transportStates, pEntry, conn))
			{
				stopRace = true;
			}
			else
			{
				conn->state = mcsEosSent;
				conn->stillActive = false;
			}

			Assert(!conn->activeXmit);
		}
#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "SendEosUDP() Leaving");
#endif
	}

	if (stopRace)
		handleStopMsgs(transportStates, pEntry, motNodeID);
}

static void
doSendStopMessageUDP(ChunkTransportState *transportStates, int16 motNodeID)
{
	ChunkTransportStateEntry	*pEntry = NULL;
	volatile MotionConn			*conn = NULL;
	int			i;

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	Assert(pEntry);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG3, "Interconnect needs no more input from slice%d; notifying senders to stop.",
			 motNodeID);

	/*
	 * Note: we're only concerned with receivers here.
	 */
	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;

		pthread_mutex_lock(&rx_thread_mutex);
		if (conn->stillActive)
		{
			if (conn->conn_info.flags & UDPIC_FLAGS_EOS)
			{
				/* we have a queued packet that has EOS in it. We've acked it, so we're done */
				elog(DEBUG4, "do sendstop: already have queued EOS packet, we're done. node %d route %d", motNodeID, i);

				Assert(conn->pBuff != NULL);
				conn->stillActive = false;

				while (conn->pBuff != NULL)
				{
					put_rx_buffer(conn); /* as a side-effect this may wake the rx_thread */
				}
			}
			else
			{
				conn->stopRequested = true;
				conn->conn_info.flags |= UDPIC_FLAGS_STOP;

				/*
				 * When gp_interconnect_elide_setup is set, we don't send a handshake message at the setup time
				 * (except for the case when the query is the first one in the session, i.e.,
				 * gp_interconnect_id = 1). The peer addresses for incoming connections will not be set until
				 * the first packet has arrived. However, when the lower slice does not have data to send,
				 * the corresponding peer address for the incoming connection will never be set.
				 * We will skip sending ACKs to those connections.
				 */
#ifdef USE_ASSERT_CHECKING
				if (!gp_interconnect_elide_setup || gp_interconnect_id == 1)
				{
					if (!(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6))
					{
						elog(LOG,"UDP Interconnect bug: trying to send ack when we don't know where to send to %s", conn->remoteHostAndPort);
					}
					Assert(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6);
				}
#endif

				if (conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6)
				{
					sendAck(conn, true);

					elog(DEBUG4, "sent stop message. node %d route %d seq %d", motNodeID, i, conn->conn_info.seq);
				}
			}
		}
		pthread_mutex_unlock(&rx_thread_mutex);
	}
}

char *
format_sockaddr(struct sockaddr *sa, char* buf, int bufsize)
{
	/* Save remote host:port string for error messages. */
	if (sa->sa_family == AF_INET)
	{
		struct sockaddr_in *	sin = (struct sockaddr_in *)sa;
		uint32					saddr = ntohl(sin->sin_addr.s_addr);

		snprintf(buf, bufsize, "%d.%d.%d.%d:%d",
				 (saddr >> 24)&0xff,
				 (saddr >> 16)&0xff,
				 (saddr >> 8)&0xff,
				 saddr&0xff,
				 ntohs(sin->sin_port));
	}
#ifdef HAVE_IPV6
	else if (sa->sa_family == AF_INET6)
	{
		char remote_port[32];
		remote_port[0] = '\0';
		buf[0] = '\0';

		if (bufsize > 10)
		{
			buf[0] = '[';
			buf[1] = '\0'; /* in case getnameinfo fails */
			/*
			 * inet_ntop isn't portable.
			 * //inet_ntop(AF_INET6, &sin6->sin6_addr, buf, bufsize - 8);
			 *
			 * postgres has a standard routine for converting addresses to printable format,
			 * which works for IPv6, IPv4, and Unix domain sockets.  I've changed this
			 * routine to use that, but I think the entire format_sockaddr routine could
			 * be replaced with it.
			 */
			int ret = pg_getnameinfo_all((const struct sockaddr_storage *)sa, sizeof(struct sockaddr_in6),
							   buf+1, bufsize-10,
							   remote_port, sizeof(remote_port),
							   NI_NUMERICHOST | NI_NUMERICSERV);
			if (ret != 0)
			{
				elog(LOG,"getnameinfo returned %d: %s, and says %s port %s",ret,gai_strerror(ret),buf,remote_port);
				/*
				 * Fall back to using our internal inet_ntop routine, which really is for inet datatype
				 * This is because of a bug in solaris, where getnameinfo sometimes fails
				 * Once we find out why, we can remove this
				 */
				snprintf(remote_port,sizeof(remote_port),"%d",((struct sockaddr_in6 *)sa)->sin6_port);
				/*
				 * This is nasty: our internal inet_net_ntop takes PGSQL_AF_INET6, not AF_INET6, which
				 * is very odd... They are NOT the same value (even though PGSQL_AF_INET == AF_INET
				 */
#define PGSQL_AF_INET6	(AF_INET + 1)
				inet_net_ntop(PGSQL_AF_INET6, sa, sizeof(struct sockaddr_in6), buf+1, bufsize-10);
				elog(LOG,"Our alternative method says %s]:%s",buf,remote_port);

			}
			buf += strlen(buf);
			strcat(buf,"]");
			buf++;
		}
		snprintf(buf, 8, ":%s", remote_port);
	}
#endif
	else
		snprintf(buf, bufsize, "?host?:?port?");

	return buf;
}								/* format_sockaddr */

/*
 * Check the connection from the dispatcher to verify that it is still
 * there.
 *
 * The connection is a struct Port, stored in the global MyProcPort.
 *
 * Return true if the dispatcher connection is still alive.
 */
static bool
dispatcherAYT(void)
{
	ssize_t		ret;
	char		buf;

	if (MyProcPort->sock < 0)
		return false;

#ifndef WIN32
		ret = recv(MyProcPort->sock, &buf, 1, MSG_PEEK|MSG_DONTWAIT);
#else
		ret = recv(MyProcPort->sock, &buf, 1, MSG_PEEK|MSG_PARTIAL);
#endif

	if (ret == 0) /* socket has been closed. EOF */
		return false;

	if (ret > 0) /* data waiting on socket, it must be OK. */
		return true;

	if (ret == -1) /* error, or would be block. */
	{
		if (errno == EAGAIN || errno == EINPROGRESS)
			return true; /* connection intact, no data available */
		else
			return false;
	}
	/* not reached */

	return true;
}

static void *
ic_rx_thread_func(void *arg)
{
	struct icpkthdr *pkt=NULL;
	bool	skip_poll=false;

	gp_set_thread_sigmasks();

	for (;;)
	{
		struct pollfd nfd;
		int		timeout;
		int		n;

		if (rx_thread_shutdown)
		{
			if (DEBUG2 >= log_min_messages)
				write_log("udp-ic: rx-thread shutting down");
			break;
		}

		if (pkt == NULL)
		{
			pthread_mutex_lock(&rx_thread_mutex);

			pkt = get_rx_buffer();
			if (pkt == NULL)
			{
				struct timeval tv;
				struct timespec ts;

				if (DEBUG3 >= log_min_messages)
					write_log("no rx_buffer available");

				gettimeofday(&tv, NULL);
				ts.tv_sec = tv.tv_sec;
				/*	leave in ms for this */
				ts.tv_nsec = (tv.tv_usec + RX_THREAD_COND_TIMEOUT);
				if (ts.tv_nsec >= 1000000)
				{
					ts.tv_sec++;
					ts.tv_nsec -= 1000000;
				}
				/* convert usec to nsec */
				ts.tv_nsec *= 1000;

				/*
				 * MPP-9910: pthread_cond_timedwait appears to be
				 * broken in OS-X 10.6.x "Snow Leopard" We'll burn a
				 * lot more CPU this way, but we will not hang.
				 */
#ifdef __darwin__
				ts.tv_sec = 0;
#endif

				/*
				 * Wait on our condition variable.
				 *
				 * We do the timedwait so that we can successfully
				 * shutdown, and so we don't wind up blocking forever.
				 */
				rx_thread_waiting = true;
				pthread_cond_timedwait(&rx_thread_cond, &rx_thread_mutex, &ts);
				rx_thread_waiting = false;

				pthread_mutex_unlock(&rx_thread_mutex);
				continue;
			}

			if (DEBUG5 >= log_min_messages)
				write_log("rx_thread: got buffer %p", pkt);

			pthread_mutex_unlock(&rx_thread_mutex);
		}

		if (!skip_poll)
		{
			/* do we have inbound traffic to handle ?*/
			nfd.fd = UDP_listenerFd;
			nfd.events = POLLIN;

			timeout = RX_THREAD_POLL_TIMEOUT;

			n = poll(&nfd, 1, timeout);

			if (n < 0)
			{
				if (errno == EINTR)
				{
					continue;
				}
				write_log("Interconnect error: poll %d", errno);
				break;
			}

			if (rx_thread_shutdown)
			{
				if (DEBUG2 >= log_min_messages)
				{
					write_log("udp-ic: rx-thread shutting down");
				}
				break;
			}

			if (n == 0)
			{
				continue;
			}
		}

		if (skip_poll || (n == 1 && (nfd.events & POLLIN)))
		{
			/* we've got something interesting to read */
			/* handle incoming */
			/* ready to read on our socket */
			volatile MotionConn *conn;
			int read_count;

			struct sockaddr_storage peer;
			socklen_t peerlen;

			peerlen = sizeof(peer);
			read_count = recvfrom(UDP_listenerFd, (char *)pkt, Gp_max_packet_size, 0,
								  (struct sockaddr *)&peer, &peerlen);

			if (rx_thread_shutdown)
			{
				if (DEBUG2 >= log_min_messages)
				{
					write_log("udp-ic: rx-thread shutting down");
				}
				break;
			}

			ic_stats_pkt_count++;

			if (DEBUG5 >= log_min_messages)
				write_log("received inbound len %d", read_count);

			if (read_count < 0)
			{
				if (errno == EWOULDBLOCK || errno == EINTR)
				{
					skip_poll = false;
					continue;
				}
				write_log("Interconnect error: recvfrom (%d)", errno);
				break;
			}

			if (read_count < sizeof(pkt))
			{
				write_log("Interconnect error: short conn receive (%d)", read_count);
				continue;
			}

			/* when we get a "good" recvfrom() result, we can skip poll() until we get a bad one. */
			skip_poll = true;

			/* length must be >= 0 */
			if (pkt->len < 0)
			{
				if (DEBUG3 >= log_min_messages)
					write_log("received inbound with negative length");
				continue;
			}
			if (DEBUG5 >= log_min_messages)
				write_log("received inbound mot-id %d listener %d",
						  pkt->motNodeId, pkt->srcListenerPort);

			pthread_mutex_lock(&rx_thread_mutex);

			conn = findConnByHeader(pkt);
			if (conn != NULL)
			{
				/* Is this a connection request ? */
				if (pkt->seq == 0)
				{
					if (DEBUG5 >= log_min_messages)
						write_log("connection request from node %d route %d, seq 0",
								  pkt->motNodeId, conn->route);

					if (pkt->len > 0) /* invalid ? */
					{
						if (LOG >= log_min_messages)
							write_log("invalid packet, seq 0 pkt.len %d", pkt->len);
						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					/* fill in the peer.  Need to case away "volatile"... Ugly	*/
					memset((void *)&conn->peer, 0, sizeof(conn->peer));
					memcpy((void *)&conn->peer, &peer, peerlen);
					conn->peer_len = peerlen;

					/* MPP-4194: fill in the dstListenerPort from the peer (may have mirror marking) */
					conn->conn_info.dstListenerPort = pkt->dstListenerPort;

					if (conn->state == mcsNull)
					{
						sendAck(conn, false);

						conn->state = mcsStarted;
						conn->stillActive = true;

						conn->conn_info.seq++;
						if (DEBUG4 >= log_min_messages)
							write_log("successful conn request for (srcPid/srcListenerPort, dstPid/dstListenerPort)=(%d/%d, %d/%d)",
									  conn->conn_info.srcPid, conn->conn_info.srcListenerPort, conn->conn_info.dstPid, conn->conn_info.dstListenerPort);

						/* MPP-3088: if the main thread is waiting on
						 * inbound connections, wake it if appropriate.
						 *
						 * All such waiters will have a specific
						 * conn->route set (that is, ANY_ROUTE isn't
						 * allowed).
						 */
						if (main_thread_waiting &&
							main_thread_waiting_node == pkt->motNodeId &&
							main_thread_waiting_query == pkt->icId &&
							main_thread_waiting_route == conn->route)
						{
							/* WAKE MAIN THREAD HERE */
							pthread_cond_signal(&rx_thread_cond);
						}
					}
					else
					{
						uint32	seq;

						/* this happens when our ack has been dropped;
						 * or we didn't respond quickly enough. we
						 * send another ack (with the matching
						 * seq-number) */

						ic_stats_dropped_acks++;

						if (DEBUG3 >= log_min_messages)
						{
							write_log("duplicate connection request from node %d route %d, seq 0",
									  pkt->motNodeId, conn->route);
							if (conn->stopRequested)
							{
								write_log("duplicate conn request, stopRequested!");
							}
						}

						seq = conn->conn_info.seq;
						conn->conn_info.seq = pkt->seq;
						sendAck(conn, false);
						conn->conn_info.seq = seq;

						if (conn->state != mcsStarted)
							write_log("corrupt connection state ?");
						if (!conn->stillActive)
							write_log("corrupt stillActive flag?");
					}

					/* we'll reuse the current rx-buffer */
					pthread_mutex_unlock(&rx_thread_mutex);
					continue;
				}
				else /* pkt->seq > 0 */
				{
					if (pkt->len != read_count)
					{
						write_log("received inbound packet, short: read %d bytes, pkt->len %d", read_count, pkt->len);
						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					if (pkt->len == 0) /* no data ? */
					{
						if (DEBUG1 >= log_min_messages)
							write_log("data packet with length of ZERO, seq %d (ignoring, stop ?)",
									  pkt->seq);
						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					/*
					 * when we're not doing a full-setup on every
					 * statement, we've got to update the peer info --
					 * full setups do this at setup-time.
					 */
					if (gp_interconnect_elide_setup && pkt->seq == 1)
					{
						/* fill in the peer.  Need to cast away "volatile".  ugly */
						memset((void *)&conn->peer, 0, sizeof(conn->peer));
						memcpy((void *)&conn->peer, &peer, peerlen);
						conn->peer_len = peerlen;

						/* MPP-4194: fill in the dstListenerPort from the peer (may have mirror marking) */
						conn->conn_info.dstListenerPort = pkt->dstListenerPort;
					}

					/* data packet */
					if (pkt->flags & UDPIC_FLAGS_EOS)
					{
						if (DEBUG3 >= log_min_messages)
							write_log("received packet with EOS motid %d route %d seq %d",
									  pkt->motNodeId, conn->route, pkt->seq);
					}

					/*
					 * if we got a stop, but didn't request a stop --
					 * ignore, this is a startup blip: we must have
					 * acked with a stop -- we don't want to do
					 * anything further with the stop-message if we
					 * didn't request a stop!
					 *
					 * this is especially important when
					 * gp_interconnect_elide_setup is enabled.
					 */
					if (!conn->stopRequested && (pkt->flags & UDPIC_FLAGS_STOP))
					{
						if (pkt->flags & UDPIC_FLAGS_EOS)
						{
							write_log("non-requested stop flag, EOS! seq %d, flags 0x%x", pkt->seq, pkt->flags);
						}
						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					if (conn->stopRequested && conn->stillActive)
					{
						if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG && DEBUG5 >= log_min_messages)
							write_log("rx_thread got packet on active connection marked stopRequested. "
									  "(flags 0x%x) node %d route %d pkt seq %d conn seq %d",
									  pkt->flags, pkt->motNodeId, conn->route, pkt->seq, conn->conn_info.seq);

						/* can we update stillActive ? */
						if (DEBUG2 >= log_min_messages)
							if (!(pkt->flags & UDPIC_FLAGS_STOP) &&
								!(pkt->flags & UDPIC_FLAGS_EOS))
								write_log("stop requested but no stop flag on return packet ?!");

						if (pkt->flags & UDPIC_FLAGS_EOS)
							conn->conn_info.flags |= UDPIC_FLAGS_EOS;

						conn->conn_info.seq = pkt->seq;
						sendAck(conn, true);

						/* we only update stillActive if eos has been sent by peer. */
						if (pkt->flags & UDPIC_FLAGS_EOS)
						{
							if (DEBUG2 >= log_min_messages)
								write_log("stop requested and acknowledged by sending peer");
							conn->stillActive = false;

							/* we may need to wake a waiter */
							if (main_thread_waiting &&
								main_thread_waiting_node == pkt->motNodeId &&
								main_thread_waiting_query == pkt->icId &&
								main_thread_waiting_route == conn->route)
							{
								/* WAKE MAIN THREAD HERE */
								pthread_cond_signal(&rx_thread_cond);
							}
						}

						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					/* dropped ack ? */
					if (pkt->seq < conn->conn_info.seq)
					{
						uint32	seq;

						ic_stats_dropped_acks++;

						if (DEBUG3 >= log_min_messages)
							write_log("dropped ack ? ignored data packet w/ cmd %d conn->cmd %d node %d route %d seq %d expected %d flags 0x%x",
									  pkt->icId, conn->conn_info.icId, pkt->motNodeId,
									  conn->route, pkt->seq, conn->conn_info.seq, pkt->flags);

						seq = conn->conn_info.seq;
						conn->conn_info.seq = pkt->seq;
						sendAck(conn, false);
						conn->conn_info.seq = seq;

						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					/* sequence number is correct */
					if (!conn->stillActive)
					{
						/* peer may have dropped ack */
						if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE &&
							DEBUG1 >= log_min_messages)
							write_log("received on inactive connection node %d route %d (seq %d pkt->seq %d)",
									  pkt->motNodeId, conn->route, conn->conn_info.seq, pkt->seq);

						conn->conn_info.seq = pkt->seq;
						sendAck(conn, true);
						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					/* valid, do we have space ? */
					if (conn->pkt_q_size == Gp_interconnect_queue_depth)
					{
						/* NO RX SPACE, drop */

						/* we could wait for the other thread to clear
						 * its input queue; but for now, we drop
						 * this packet */
						if (DEBUG2 >= log_min_messages)
							write_log("no rx-space for pkt with motid %d route %d seq %d dropped",
									  pkt->motNodeId, conn->route, pkt->seq);

						ic_stats_rx_overrun++;
						conn->stat_count_dropped++;

						pthread_mutex_unlock(&rx_thread_mutex);
						continue;
					}

					/*
					 * before delivering, check the CRC of the
					 * payload. (header errors will likely have been
					 * caught above)
					 */
					if (gp_interconnect_full_crc)
					{
						pg_crc32 rx_crc, local_crc;

						rx_crc = pkt->crc;
						pkt->crc = 0;

						INIT_CRC32C(local_crc);
						COMP_CRC32C(local_crc, pkt, pkt->len);
						FIN_CRC32C(local_crc);

						if (rx_crc != local_crc)
						{
							ic_stats_crc_error++;

							/* drop */
							write_log("received network data error, dropping bad packet, user data unaffected.");
							pthread_mutex_unlock(&rx_thread_mutex);
							continue;
						}
					}

					sendAck(conn, false);

					/* valid packet, and we have space. */
					if (DEBUG5 >= log_min_messages)
						write_log("delivered data packet for node %d route %d seq %d",
								  pkt->motNodeId, conn->route, pkt->seq);

					/* point the pBuff at the head of the queue */
					if (conn->pkt_q_size == 0)
					{
						conn->pBuff = (uint8 *)pkt;
						conn->msgPos = conn->pBuff;
						conn->msgSize = pkt->len;
						conn->recvBytes = conn->msgSize;
					}

					/* put the packet at the head (advance tail) */
					conn->pkt_q[conn->pkt_q_tail] = (uint8 *)pkt;
					conn->pkt_q_tail++;
					if (conn->pkt_q_tail == Gp_interconnect_queue_depth)
						conn->pkt_q_tail = 0;

					/* advance size */
					conn->pkt_q_size++;

					conn->conn_info.seq++;

					/* we need to know about packets which we may have
					 * "in queue" which have EOS, so we don't bother
					 * with things like stop messages */
					if (pkt->flags & UDPIC_FLAGS_EOS)
					{
						conn->conn_info.flags |= UDPIC_FLAGS_EOS;
						conn->eosAcked = true;
					}

					/* Was the main thread waiting for something ? */
					if (main_thread_waiting &&
						main_thread_waiting_node == pkt->motNodeId &&
						main_thread_waiting_query == pkt->icId)
					{
						if (main_thread_waiting_route == ANY_ROUTE)
							main_thread_waiting_route = conn->route;

						/* were we either waiting for this connection or "any" */
						if (main_thread_waiting_route == conn->route)
						{
							if (DEBUG2 >= log_min_messages)
								write_log("rx thread: main_waiting waking it route %d (%p:%d)", main_thread_waiting_route, conn->pBuff, conn->pkt_q_size);

							/* WAKE MAIN THREAD HERE */
							pthread_cond_signal(&rx_thread_cond);
						}
					}

					pkt = NULL;

					pthread_mutex_unlock(&rx_thread_mutex);
					continue;
				}
			}
			else
			{
				/* else, ignore and drop, but we may need to ack it */

				/*
				 * we want to check to see if this *is* a
				 * packet from our session (previous command-id ?)
				 * which still needs some acking ?
				 */
				ic_stats_mismatch++;

				rx_handle_mismatch(pkt, &peer, peerlen);

				pthread_mutex_unlock(&rx_thread_mutex);
			}
		}

		/* pthread_yield(); */
	}

	/* clean up any thread-state here */
	rx_thread_shutdown_complete = true;

	/* nothing to return */
	return NULL;
}

/*
 * If the mismatched packet is from an old connection, we may need to
 * send an acknowledgement.
 *
 * We are called with the receiver-lock held, and we never release it.
 */
static void
rx_handle_mismatch(struct icpkthdr *pkt, struct sockaddr_storage *peer, int peer_len)
{
	/*
	 * we want to ack old packets; but *must* avoid acking connection requests:
	 *
	 *	 "ACK the past, NAK the future" explicit NAKs aren't necessary, we just don't
	 *	 want to ACK future packets, that confuses everyone.
	 */
	if (pkt->seq > 0 && pkt->sessionId == gp_session_id)
	{
		bool need_ack=false;
		uint8 ack_flags=0;

		/*
		 * The QD-backends can't use a counter, they've potentially got multiple instances (one for each active cursor)
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			struct cursor_ic_history_entry *p;

			p = get_cursor_ic_entry(pkt->icId);
			if (p)
			{
				if (p->status == 0)
				{
					need_ack = true;
				}
				else
				{
					/* not torn down yet ? how can this happen ? */
					write_log("GOT A MISMATCH PACKET WITH ID %d HISTORY THINKS IT IS ACTIVE", pkt->icId);
					return; /* ignore, no ack */
				}
			}
			else
			{
				write_log("GOT A MISMATCH PACKET WITH ID %d HISTORY HAS NO RECORD", pkt->icId);
				/* no record means that it is from the future ? */
				ack_flags = UDPIC_FLAGS_NAK;
				need_ack = true;
			}
		}
		/* The QEs get to use a simple counter. */
		else if (Gp_role == GP_ROLE_EXECUTE &&
				 (gp_interconnect_id == 0 || gp_interconnect_id >= pkt->icId))
		{
			need_ack = true;

			/*
			 * If we aren't doing the setup hand-shake, the
			 * stop message we're about to send is ambiguous.
			 * We disambiguate using a NAK flag -- our peer
			 * will continue retrying.
			 *
			 * We want to "ACK the past, but NAK the future."
			 *
			 * handleAcks() on our sender will retransmit.
			 */
			if (pkt->seq == 1 && pkt->icId > ic_id_last_teardown)
			{
				ack_flags = UDPIC_FLAGS_NAK;
			}
		}

		if (need_ack)
		{
			MotionConn dummyconn;
			char buf[128];	/* numeric IP addresses shouldn't exceed about 50 chars, but play it safe */


			memcpy(&dummyconn.conn_info, pkt, sizeof(struct icpkthdr));
			dummyconn.peer = *peer;
			dummyconn.peer_len = peer_len;

			dummyconn.conn_info.flags |= ack_flags;

			if (DEBUG4 >= log_min_messages)
				write_log("idle, acking old packet ?");

			if (DEBUG4 >= log_min_messages)
				write_log("ACKING PACKET WITH FLAGS: pkt->seq %d 0x%x pkt->icId %d last-teardown %d interconnect_id %d",
					  pkt->seq, dummyconn.conn_info.flags, pkt->icId, ic_id_last_teardown, gp_interconnect_id);

			Assert(dummyconn.peer.ss_family == AF_INET || dummyconn.peer.ss_family == AF_INET6);

			format_sockaddr((struct sockaddr *)&dummyconn.peer, buf, sizeof(buf));

			if (DEBUG4 >= log_min_messages)
				write_log("ACKING PACKET TO %s", buf);

			/* we're idle ? send an Ack to a fake conn */
			sendAck(&dummyconn, true);
		}
	}
	else
	{
		if (DEBUG4 >= log_min_messages)
			write_log("dropping packet from command-id %d seq %d (my cmd %d)", pkt->icId, pkt->seq, gp_interconnect_id);
	}
}

void
WaitInterconnectQuitUDP(void)
{
	/* In case ic thread is waiting lock */
	pthread_mutex_unlock(&rx_thread_mutex);

	rx_thread_shutdown = true;

	if(ic_rx_thread_created)
	{
		SendDummyPacket();
		pthread_join(ic_rx_thread, NULL);
	}
	ic_rx_thread_created = false;
}
