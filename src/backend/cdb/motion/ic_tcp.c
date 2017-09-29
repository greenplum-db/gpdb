/*-------------------------------------------------------------------------
 * ic_tcp.c
 *	   Interconnect code specific to TCP transport.
 *
 * Portions Copyright (c) 2005-2008, Greenplum, Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/ic_tcp.c
 *
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

#include "nodes/execnodes.h"	/* Slice, SliceTable */
#include "nodes/pg_list.h"
#include "nodes/print.h"
#include "miscadmin.h"
#include "libpq/libpq-be.h"
#include "libpq/ip.h"
#include "utils/builtins.h"
#include "utils/debugbreak.h"

#include "cdb/cdbselect.h"
#include "cdb/tupchunklist.h"
#include "cdb/ml_ipc.h"
#include "cdb/cdbvars.h"

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/in.h>

/*
 * backlog for listen() call: it is important that this be something like a
 * good match for the maximum number of QEs. Slow insert performance will
 * result if it is too low.
 */
#define CONNECT_RETRY_MS	4000
#define CONNECT_AGGRESSIVERETRY_MS	500

/* listener backlog is calculated at listener-creation time */
int			listenerBacklog = 128;

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

#define POLLRDNORM  0x0100
#define POLLRDBAND  0x0200
#define POLLIN      (POLLRDNORM | POLLRDBAND)
#define POLLPRI     0x0400

#define POLLWRNORM  0x0010
#define POLLOUT     (POLLWRNORM)
#define POLLWRBAND  0x0020

#define POLLERR     0x0001
#define POLLHUP     0x0002
#define POLLNVAL    0x0004

typedef struct pollfd
{

	SOCKET		fd;
	SHORT		events;
	SHORT		revents;

}			WSAPOLLFD, *PWSAPOLLFD, FAR * LPWSAPOLLFD;

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
 * Postgres normally uses its own custom select implementation
 * on Windows, but they haven't implemented execeptfds, which
 * we use here.  So, undef this to use the normal Winsock version
 * for now
 */
#undef select
#endif

/* our timeout value for select() and other socket operations. */
static struct timeval tval;

static inline MotionConn *
getMotionConn(ChunkTransportStateEntry *pEntry, int iConn)
{
	Assert(pEntry);
	Assert(pEntry->conns);
	Assert(iConn < pEntry->numConns + pEntry->numPrimaryConns);

	return pEntry->conns + iConn;
}

static ChunkTransportStateEntry *startOutgoingConnections(ChunkTransportState *transportStates,
						 Slice *sendSlice,
						 int *pOutgoingCount);

static void format_fd_set(StringInfo buf, int nfds, mpp_fd_set *fds, char *pfx, char *sfx);
static char *format_sockaddr(struct sockaddr *sa, char *buf, int bufsize);

static void setupOutgoingConnection(ChunkTransportState *transportStates,
						ChunkTransportStateEntry *pEntry, MotionConn *conn);
static void updateOutgoingConnection(ChunkTransportState *transportStates,
						 ChunkTransportStateEntry *pEntry, MotionConn *conn, int errnoSave);
static void sendRegisterMessage(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn);
static bool readRegisterMessage(ChunkTransportState *transportStates,
					MotionConn *conn);
static MotionConn *acceptIncomingConnection(void);

static void flushInterconnectListenerBacklog(void);

static void waitOnOutbound(ChunkTransportStateEntry *pEntry);

static TupleChunkListItem RecvTupleChunkFromAnyTCP(MotionLayerState *mlStates,
						 ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 *srcRoute);

static TupleChunkListItem RecvTupleChunkFromTCP(ChunkTransportState *transportStates,
					  int16 motNodeID,
					  int16 srcRoute);

static void SendEosTCP(MotionLayerState *mlStates, ChunkTransportState *transportStates,
		   int motNodeID, TupleChunkListItem tcItem);

static bool SendChunkTCP(MotionLayerState *mlStates, ChunkTransportState *transportStates,
			 ChunkTransportStateEntry *pEntry, MotionConn *conn, TupleChunkListItem tcItem, int16 motionId);

static bool flushBuffer(MotionLayerState *mlStates, ChunkTransportState *transportStates,
			ChunkTransportStateEntry *pEntry, MotionConn *conn, int16 motionId);

static void doSendStopMessageTCP(ChunkTransportState *transportStates, int16 motNodeID);

/*
 * setupTCPListeningSocket
 */
static void
setupTCPListeningSocket(int backlog, int *listenerSocketFd, uint16 *listenerPort)
{
	int			errnoSave;
	int			fd = -1;
	const char *fun;

	*listenerSocketFd = -1;
	*listenerPort = 0;

	struct sockaddr_storage addr;
	socklen_t	addrlen;

	struct addrinfo hints;
	struct addrinfo *addrs,
			   *rp;
	int			s;
	char		service[32];
	char		myname[128];
	char	   *localname = NULL;

	/*
	 * we let the system pick the TCP port here so we don't have to manage
	 * port resources ourselves.  So set the port to 0 (any port)
	 */
	snprintf(service, 32, "%d", 0);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* TCP socket */
	hints.ai_flags = AI_PASSIVE;	/* For wildcard IP address */
	hints.ai_protocol = 0;		/* Any protocol */

	/*
	 * We use INADDR_ANY if we don't have a valid address for ourselves (e.g.
	 * QD local connections tend to be AF_UNIX, or on 127.0.0.1 -- so bind
	 * everything)
	 */
	if (Gp_role == GP_ROLE_DISPATCH ||
		(MyProcPort->laddr.addr.ss_family != AF_INET &&
		 MyProcPort->laddr.addr.ss_family != AF_INET6))
		localname = NULL;		/* We will listen on all network adapters */
	else
	{
		/*
		 * Restrict what IP address we will listen on to just the one that was
		 * used to create this QE session.
		 */
		getnameinfo((const struct sockaddr *) &(MyProcPort->laddr.addr), MyProcPort->laddr.salen,
					myname, sizeof(myname),
					NULL, 0, NI_NUMERICHOST);
		hints.ai_flags |= AI_NUMERICHOST;
		localname = myname;
		elog(DEBUG1, "binding to %s only", localname);
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			ereport(DEBUG4, (errmsg("binding listener %s", localname)));
	}

	s = getaddrinfo(localname, service, &hints, &addrs);
	if (s != 0)
		elog(ERROR, "getaddrinfo says %s", gai_strerror(s));

	/*
	 * getaddrinfo() returns a list of address structures, one for each valid
	 * address and family we can use.
	 *
	 * Try each address until we successfully bind. If socket (or bind) fails,
	 * we (close the socket and) try the next address.  This can happen if the
	 * system supports IPv6, but IPv6 is disabled from working, or if it
	 * supports IPv6 and IPv4 is disabled.
	 */


	/*
	 * If there is both an AF_INET6 and an AF_INET choice, we prefer the
	 * AF_INET6, because on UNIX it can receive either protocol, whereas
	 * AF_INET can only get IPv4.  Otherwise we'd need to bind two sockets,
	 * one for each protocol.
	 *
	 * Why not just use AF_INET6 in the hints?  That works perfect if we know
	 * this machine supports IPv6 and IPv6 is enabled, but we don't know that.
	 */

#ifdef HAVE_IPV6
	if (addrs->ai_family == AF_INET && addrs->ai_next != NULL && addrs->ai_next->ai_family == AF_INET6)
	{
		/*
		 * We got both an INET and INET6 possibility, but we want to prefer
		 * the INET6 one if it works. Reverse the order we got from
		 * getaddrinfo so that we try things in our preferred order. If we got
		 * more possibilities (other AFs??), I don't think we care about them,
		 * so don't worry if the list is more that two, we just rearrange the
		 * first two.
		 */
		struct addrinfo *temp = addrs->ai_next; /* second node */

		addrs->ai_next = addrs->ai_next->ai_next;	/* point old first node to
													 * third node if any */
		temp->ai_next = addrs;	/* point second node to first */
		addrs = temp;			/* start the list with the old second node */
	}
#endif

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		/*
		 * getaddrinfo gives us all the parameters for the socket() call as
		 * well as the parameters for the bind() call.
		 */

		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == -1)
			continue;

		/*
		 * we let the system pick the TCP port here so we don't have to manage
		 * port resources ourselves.
		 */

		if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;				/* Success */

		close(fd);
		fd = -1;
	}

	fun = "bind";
	if (fd == -1)
		goto error;

	/* Make socket non-blocking. */
	fun = "fcntl(O_NONBLOCK)";
	if (!pg_set_noblock(fd))
		goto error;

	fun = "listen";
	if (listen(fd, backlog) < 0)
		goto error;

	/* Get the listening socket's port number. */
	fun = "getsockname";
	addrlen = sizeof(addr);
	if (getsockname(fd, (struct sockaddr *) &addr, &addrlen) < 0)
		goto error;

	/* Give results to caller. */
	*listenerSocketFd = fd;

	/* display which port was chosen by the system. */
	if (addr.ss_family == AF_INET6)
		*listenerPort = ntohs(((struct sockaddr_in6 *) &addr)->sin6_port);
	else
		*listenerPort = ntohs(((struct sockaddr_in *) &addr)->sin_port);

	freeaddrinfo(addrs);
	return;

error:
	errnoSave = errno;
	if (fd >= 0)
		closesocket(fd);
	errno = errnoSave;
	freeaddrinfo(addrs);
	ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("Interconnect Error: Could not set up tcp listener socket."),
					errdetail("%s: %m", fun)));
}								/* setupListeningSocket */

/*
 * Initialize TCP specific comms.
 */
void
InitMotionTCP(int *listenerSocketFd, uint16 *listenerPort)
{
	tval.tv_sec = 0;
	tval.tv_usec = 500000;

#ifdef pg_on_solaris
	listenerBacklog = Min(1024, Max(GpIdentity.numsegments * 4, listenerBacklog));
#endif

	setupTCPListeningSocket(listenerBacklog, listenerSocketFd, listenerPort);

	return;
}

/* cleanup any TCP-specific comms info */
void
CleanupMotionTCP(void)
{
	/* nothing to do. */
	return;
}

/* Function readPacket() is used to read in the next packet from the given
 * MotionConn.
 *
 * This call blocks until the packet is read in, and is part of a
 * global scheme where senders block until the entire message is sent, and
 * receivers block until the entire message is read.  Both use non-blocking
 * socket calls so that we can handle any PG interrupts.
 *
 * Note, that for speed we want to read a message all in one go,
 * header and all. A consequence is that we may read in part of the
 * next message, which we've got to keep track of ... recvBytes holds
 * the byte-count of the unprocessed messages.
 *
 * PARAMETERS
 *	 conn - MotionConn to read the packet from.
 *
 */
/* static inline void */
void
readPacket(MotionConn *conn, ChunkTransportState *transportStates)
{
	int			n,
				bytesRead = conn->recvBytes;
	bool		gotHeader = false,
				gotPacket = false;
	mpp_fd_set	rset;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "readpacket: (fd %d) (max %d) outstanding bytes %d", conn->sockfd, Gp_max_packet_size, conn->recvBytes);
#endif

	/* do we have a complete message waiting to be processed ? */
	if (conn->recvBytes >= PACKET_HEADER_SIZE)
	{
		memcpy(&conn->msgSize, conn->msgPos, sizeof(uint32));
		gotHeader = true;
		if (conn->recvBytes >= conn->msgSize)
		{
#ifdef AMS_VERBOSE_LOGGING
			elog(DEBUG5, "readpacket: returning previously read data (%d)", conn->recvBytes);
#endif
			return;
		}
	}

	/*
	 * partial message waiting in recv buffer! Move to head of buffer:
	 * eliminate the slack (which will always be at the beginning) in the
	 * buffer
	 */
	if (conn->recvBytes != 0)
		memmove(conn->pBuff, conn->msgPos, conn->recvBytes);

	conn->msgPos = conn->pBuff;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "readpacket: %s on previous call msgSize %d", gotHeader ? "got header" : "no header", conn->msgSize);
#endif

	while (!gotPacket && bytesRead < Gp_max_packet_size)
	{
		/* see if user canceled and stuff like that */
		ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

		/*
		 * we read at the end of the buffer, we've eliminated any slack above
		 */
		if ((n = recv(conn->sockfd, conn->pBuff + bytesRead,
					  Gp_max_packet_size - bytesRead, 0)) < 0)
		{
			if (errno == EINTR)
				continue;
			if (errno == EWOULDBLOCK)
			{
				int			retry = 0;

				do
				{
					struct timeval timeout = tval;

					/* check for the QD cancel for every 2 seconds */
					if (retry++ > 4)
					{
						retry = 0;

						/* check to see if the dispatcher should cancel */
						if (Gp_role == GP_ROLE_DISPATCH)
						{
							checkForCancelFromQD(transportStates);
						}

					}

					/* see if user canceled and stuff like that */
					ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

					MPP_FD_ZERO(&rset);
					MPP_FD_SET(conn->sockfd, &rset);
					n = select(conn->sockfd + 1, (fd_set *) &rset, NULL, NULL, &timeout);
					if (n == 0 || (n < 0 && errno == EINTR))
						continue;
					else if (n < 0)
					{
						ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
										errmsg("Interconnect error reading an incoming packet."),
										errdetail("%s from seg%d at %s: %m",
												  "select",
												  conn->remoteContentId,
												  conn->remoteHostAndPort)));
					}

				}
				while (n < 1);
			}
			else
			{
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Interconnect error reading an incoming packet."),
								errdetail("%s from seg%d at %s: %m",
										  "read",
										  conn->remoteContentId,
										  conn->remoteHostAndPort)));
			}
		}
		else if (n == 0)
		{
#ifdef AMS_VERBOSE_LOGGING
			elog(DEBUG5, "readpacket(); breaking in while (fd %d) recvBytes %d msgSize %d", conn->sockfd, conn->recvBytes, conn->msgSize);
			print_connection(transportStates, conn->sockfd, "interconnect error on");
#endif
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error: connection closed prematurely."),
							errdetail("from Remote Connection: contentId=%d at %s",
									  conn->remoteContentId, conn->remoteHostAndPort)));
			break;
		}
		else
		{
			bytesRead += n;

			if (!gotHeader && bytesRead >= PACKET_HEADER_SIZE)
			{
				/* got the header */
				memcpy(&conn->msgSize, conn->msgPos, sizeof(uint32));
				gotHeader = true;
			}
			conn->recvBytes = bytesRead;

			if (gotHeader && bytesRead >= conn->msgSize)
				gotPacket = true;
		}
	}
#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "readpacket: got %d bytes", conn->recvBytes);
#endif
}

static void
flushIncomingData(int fd)
{
	static char trash[8192];
	int			bytes;

	/*
	 * If we're in TeardownInterconnect, we should only have to call recv() a
	 * couple of times to empty out our socket buffers
	 */
	do
	{
		bytes = recv(fd, trash, sizeof(trash), 0);
	} while (bytes > 0);
}

/* Function startOutgoingConnections() is used to initially kick-off any outgoing
 * connections for mySlice.
 *
 * This should not be called for root slices (i.e. QD ones) since they don't
 * ever have outgoing connections.
 *
 * PARAMETERS
 *
 *  sendSlice - Slice that this process is member of.
 *  pIncIdx - index in the parent slice list of myslice.
 *
 * RETURNS
 *	 Initialized ChunkTransportState for the Sending Motion Node Id.
 */
static ChunkTransportStateEntry *
startOutgoingConnections(ChunkTransportState *transportStates,
						 Slice *sendSlice,
						 int *pOutgoingCount)
{
	ChunkTransportStateEntry *pEntry;
	MotionConn *conn;
	ListCell   *cell;
	Slice	   *recvSlice;
	CdbProcess *cdbProc;

	*pOutgoingCount = 0;

	recvSlice = (Slice *) list_nth(transportStates->sliceTable->slices, sendSlice->parentIndex);

	adjustMasterRouting(recvSlice);

	if (gp_interconnect_aggressive_retry)
	{
		if ((list_length(recvSlice->children) * sendSlice->numGangMembersToBeActive) > listenerBacklog)
			transportStates->aggressiveRetry = true;
	}
	else
		transportStates->aggressiveRetry = false;

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG4, "Interconnect seg%d slice%d setting up sending motion node (aggressive retry is %s)",
			 Gp_segment, sendSlice->sliceIndex,
			 (transportStates->aggressiveRetry ? "active" : "inactive"));

	pEntry = createChunkTransportState(transportStates,
									   sendSlice,
									   recvSlice,
									   list_length(recvSlice->primaryProcesses));

	/*
	 * Setup a MotionConn entry for each of our outbound connections. Request
	 * a connection to each receiving backend's listening port. NB: Some
	 * mirrors could be down & have no CdbProcess entry.
	 */
	conn = pEntry->conns;

	foreach(cell, recvSlice->primaryProcesses)
	{
		cdbProc = (CdbProcess *) lfirst(cell);
		if (cdbProc)
		{
			conn->cdbProc = cdbProc;
			conn->pBuff = palloc(Gp_max_packet_size);
			conn->state = mcsSetupOutgoingConnection;
			(*pOutgoingCount)++;
		}
		conn++;
	}

	return pEntry;
}								/* startOutgoingConnections */


/*
 * setupOutgoingConnection
 *
 * Called by SetupInterconnect when conn->state == mcsSetupOutgoingConnection.
 *
 * On return, state is:
 *      mcsSetupOutgoingConnection if failed and caller should retry.
 *      mcsConnecting if non-blocking connect() is pending.  Caller should
 *          send registration message when socket becomes write-ready.
 *      mcsSendRegMsg or mcsStarted if connect() completed successfully.
 */
static void
setupOutgoingConnection(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	CdbProcess *cdbProc = conn->cdbProc;

	int			sockopt;
	int			n;

	int			ret;
	char		portNumberStr[32];
	char	   *service;
	struct addrinfo *addrs = NULL;
	struct addrinfo hint;

	Assert(conn->cdbProc);
	Assert(conn->state == mcsSetupOutgoingConnection);

	conn->wakeup_ms = 0;
	conn->remoteContentId = cdbProc->contentid;

	/*
	 * record the destination IP addr and port for error messages. Since the
	 * IP addr might be IPv6, it might have ':' embedded, so in that case, put
	 * '[]' around it so we can see that the string is an IP and port
	 * (otherwise it might look just like an IP).
	 *
	 * Should we really put this info into these fields? Later we replace this
	 * with the source IP and port (after the bind call).
	 */
	if (strchr(cdbProc->listenerAddr, ':') != 0)
		snprintf(conn->remoteHostAndPort, sizeof(conn->remoteHostAndPort),
				 "[%s]:%d", cdbProc->listenerAddr, cdbProc->listenerPort);
	else
		snprintf(conn->remoteHostAndPort, sizeof(conn->remoteHostAndPort),
				 "%s:%d", cdbProc->listenerAddr, cdbProc->listenerPort);

	/* Might be retrying due to connection failure etc.  Close old socket. */
	if (conn->sockfd >= 0)
	{
		closesocket(conn->sockfd);
		conn->sockfd = -1;
	}

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_family = AF_UNSPEC; /* Allow for IPv4 or IPv6  */
#ifdef AI_NUMERICSERV
	hint.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;	/* Never do name
														 * resolution */
#else
	hint.ai_flags = AI_NUMERICHOST; /* Never do name resolution */
#endif

	snprintf(portNumberStr, sizeof(portNumberStr), "%d", cdbProc->listenerPort);
	service = portNumberStr;

	ret = pg_getaddrinfo_all(cdbProc->listenerAddr, service, &hint, &addrs);
	if (ret || !addrs)
	{
		if (addrs)
			pg_freeaddrinfo_all(hint.ai_family, addrs);

		ereport(ERROR,
				(errmsg("could not translate host addr \"%s\", port \"%d\" to address: %s",
						cdbProc->listenerAddr, cdbProc->listenerPort, gai_strerror(ret))));

		return;
	}

	/*
	 * Since we aren't using name resolution, getaddrinfo will return only 1
	 * entry
	 */

	/*
	 * Create a socket.  getaddrinfo() returns the parameters needed by
	 * socket()
	 */
	conn->sockfd = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
	if (conn->sockfd < 0)
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error setting up outgoing "
							   "connection."),
						errdetail("%s: %m", "socket")));

	/* make socket non-blocking BEFORE we connect. */
	if (!pg_set_noblock(conn->sockfd))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error setting up outgoing "
							   "connection."),
						errdetail("%s: %m", "fcntl(O_NONBLOCK)")));
	}

	/* Allow bind() to succeed even if the port is in TIME_WAIT state. */
	sockopt = 1;
	if (setsockopt(conn->sockfd, SOL_SOCKET, SO_REUSEADDR,
				   (void *) &sockopt, sizeof(sockopt)) < 0)
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error setting up outgoing "
							   "connection."),
						errdetail("%s: %m", "setsockopt(SO_REUSEADDR)")));


	/*
	 * To help with fault tolerance, try to use the same LAN adapter as was
	 * used to connect the session to us.
	 *
	 * Bind that IP address to the socket.   But, we must insure that the
	 * source address is the same address family as the destination address.
	 *
	 * The is especially important to check on the QD, since local sessions
	 * are often AF_UNIX, and even if TCP, they could be either v6 or v4.
	 *
	 */


	struct sockaddr_storage saddr;
	int			saddr_len = addrs->ai_addrlen;

	memset(&saddr, 0, sizeof(saddr));

	if (MyProcPort->laddr.addr.ss_family == addrs->ai_family)
	{
		/* We need to copy the address so we can clear the port */

		memcpy(&saddr, &MyProcPort->laddr.addr, MyProcPort->laddr.salen);
		if (saddr.ss_family == AF_INET)
			((struct sockaddr_in *) &saddr)->sin_port = 0;	/* pick any available
															 * outbound port */
		else if (saddr.ss_family == AF_INET6)
			((struct sockaddr_in6 *) &saddr)->sin6_port = 0;


		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		{
			char		debugmsg[128];

			inet_ntop(saddr.ss_family, (struct sockaddr *) &saddr, debugmsg, sizeof(debugmsg));
			ereport(DEBUG4, (errmsg("bind outbound  %s", debugmsg)));
		}
	}
	else
	{
		/*
		 * bind to the "any ip address" "any port".  Not really necessary to
		 * use bind() but this allows us to get the system assigned source IP
		 * and port without waiting until after the connect() call.
		 */
		saddr.ss_family = addrs->ai_family;

		/*
		 * We could set the address to INADDR_ANY or INADDR_ANY6 but those are
		 * just zeros anyway
		 */
	}

	if (bind(conn->sockfd, (struct sockaddr *) &saddr, saddr_len) < 0)
	{
		char		debugmsg[128];

		inet_ntop(saddr.ss_family, (struct sockaddr *) &saddr, debugmsg, sizeof(debugmsg));
		/* Should never get EADDRINUSE because we know it's our port. */
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error setting up outgoing "
							   "connection."),
						errdetail("Could not bind to local addr %s. %m",
								  debugmsg)));
	}

	/*
	 * Get the source IP address and Port
	 */
	format_sockaddr((struct sockaddr *) &saddr, conn->remoteHostAndPort,
					sizeof(conn->remoteHostAndPort));


	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		ereport(DEBUG1, (errmsg("Interconnect connecting to seg%d slice%d %s "
								"pid=%d sockfd=%d",
								conn->remoteContentId,
								pEntry->recvSlice->sliceIndex,
								conn->remoteHostAndPort,
								conn->cdbProc->pid,
								conn->sockfd)));

	/*
	 * Initiate the connection.
	 */
	for (;;)
	{							/* connect() EINTR retry loop */
		ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

		n = connect(conn->sockfd, addrs->ai_addr, addrs->ai_addrlen);

		/* Non-blocking socket never connects immediately, but check anyway. */
		if (n == 0)
		{
			sendRegisterMessage(transportStates, pEntry, conn);
			pg_freeaddrinfo_all(hint.ai_family, addrs);
			return;
		}

		/* Retry if a signal was received. */
		if (errno == EINTR)
			continue;

		/* Normal case: select() will tell us when connection is made. */
		if (errno == EINPROGRESS ||
			errno == EWOULDBLOCK)
		{
			conn->state = mcsConnecting;
			pg_freeaddrinfo_all(hint.ai_family, addrs);
			return;
		}

		pg_freeaddrinfo_all(hint.ai_family, addrs);
		/* connect() failed.  Log the error.  Caller should retry. */
		updateOutgoingConnection(transportStates, pEntry, conn, errno);
		return;
	}							/* connect() EINTR retry loop */
}								/* setupOutgoingConnection */


/*
 * updateOutgoingConnection
 *
 * Called when connect() succeeds or fails.
 */
static void
updateOutgoingConnection(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn, int errnoSave)
{
	socklen_t	sizeoferrno = sizeof(errnoSave);

	/* Get errno value indicating success or failure. */
	if (errnoSave == -1 &&
		getsockopt(conn->sockfd, SOL_SOCKET, SO_ERROR,
				   (void *) &errnoSave, &sizeoferrno))
	{
		/* getsockopt failed */
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect could not connect to seg%d %s",
							   conn->remoteContentId,
							   conn->remoteHostAndPort),
						errdetail("%s sockfd=%d: %m",
								  "getsockopt(SO_ERROR)",
								  conn->sockfd)));
	}

	switch (errnoSave)
	{
			/* Success!  Advance to next state. */
		case 0:
			sendRegisterMessage(transportStates, pEntry, conn);
			return;
		default:
			errno = errnoSave;
			ereport(LOG, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						  errmsg("Interconnect could not connect to seg%d %s "
								 "pid=%d; "
								 "will retry. %s: %m",
								 conn->remoteContentId,
								 conn->remoteHostAndPort,
								 conn->cdbProc->pid,
								 "connect")));
			break;
	}

	/* Tell caller to close the socket and try again. */
	conn->state = mcsSetupOutgoingConnection;
}								/* updateOutgoingConnection */

/* Function sendRegisterMessage() used to send a Register message to the
 * remote destination on the other end of the provided conn.
 *
 * PARAMETERS
 *
 *	 pEntry - ChunkTransportState.
 *	 conn	- MotionConn to send message out on.
 *
 * Called by SetupInterconnect when conn->state == mcsSetupOutgoingConnection.
 *
 * On return, state is:
 *      mcsSendRegMsg if registration message has not been completely sent.
 *          Caller should retry when socket becomes write-ready.
 *      mcsStarted if registration message has been sent.  Caller can start
 *          sending data.
 */
static void
sendRegisterMessage(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	int			bytesToSend;
	int			bytesSent;

	if (conn->state != mcsSendRegMsg)
	{
		RegisterMessage *regMsg = (RegisterMessage *) conn->pBuff;
		struct sockaddr_storage localAddr;
		socklen_t	addrsize;

		Assert(conn->cdbProc &&
			   conn->pBuff &&
			   sizeof(*regMsg) <= Gp_max_packet_size);
		Assert(pEntry->recvSlice &&
			   pEntry->sendSlice &&
			   IsA(pEntry->recvSlice, Slice) &&
			   IsA(pEntry->sendSlice, Slice));

		/* Save local host and port for log messages. */
		addrsize = sizeof(localAddr);
		if (getsockname(conn->sockfd, (struct sockaddr *) &localAddr, &addrsize))
		{
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error after making connection."),
							errdetail("%s sockfd=%d remote=%s: %m",
									  "getsockname",
									  conn->sockfd,
									  conn->remoteHostAndPort)));
		}
		format_sockaddr((struct sockaddr *) &localAddr, conn->localHostAndPort,
						sizeof(conn->localHostAndPort));

		if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
			ereport(LOG, (errmsg("Interconnect sending registration message "
								 "to seg%d slice%d %s pid=%d "
								 "from seg%d slice%d %s sockfd=%d",
								 conn->remoteContentId,
								 pEntry->recvSlice->sliceIndex,
								 conn->remoteHostAndPort,
								 conn->cdbProc->pid,
								 Gp_segment,
								 pEntry->sendSlice->sliceIndex,
								 conn->localHostAndPort,
								 conn->sockfd)));

		regMsg->msgBytes = sizeof(*regMsg);
		regMsg->recvSliceIndex = pEntry->recvSlice->sliceIndex;
		regMsg->sendSliceIndex = pEntry->sendSlice->sliceIndex;

		regMsg->srcContentId = Gp_segment;
		regMsg->srcListenerPort = Gp_listener_port & 0x0ffff;
		regMsg->srcPid = MyProcPid;
		regMsg->srcSessionId = gp_session_id;
		regMsg->srcCommandCount = gp_interconnect_id;


		conn->state = mcsSendRegMsg;
		conn->msgPos = conn->pBuff;
		conn->msgSize = sizeof(*regMsg);
	}

	/* Send as much as we can. */
	for (;;)
	{
		bytesToSend = conn->pBuff + conn->msgSize - conn->msgPos;
		bytesSent = send(conn->sockfd, conn->msgPos, bytesToSend, 0);
		if (bytesSent == bytesToSend)
			break;
		else if (bytesSent >= 0)
			conn->msgPos += bytesSent;
		else if (errno == EWOULDBLOCK)
			return;				/* call me again to send the rest */
		else if (errno == EINTR)
			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
		else
		{
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error writing registration "
								   "message to seg%d at %s",
								   conn->remoteContentId,
								   conn->remoteHostAndPort),
							errdetail("%s pid=%d sockfd=%d local=%s: %m",
									  "write",
									  conn->cdbProc->pid,
									  conn->sockfd,
									  conn->localHostAndPort)));
		}
	}

	/* Sent it all. */
	conn->state = mcsStarted;
	conn->msgPos = NULL;
	conn->msgSize = PACKET_HEADER_SIZE;
	conn->stillActive = true;
}								/* sendRegisterMessage */


/* Function readRegisterMessage() reads a "Register" message off of the conn
 * and places it in the right MotionLayerEntry conn slot based on the contents
 * of the register message.
 *
 * PARAMETERS
 *
 *	 conn - MotionConn to read the register messagefrom.
 *
 * Returns true if message has been received; or false if caller must retry
 * when socket becomes read-ready.
 */
static bool
readRegisterMessage(ChunkTransportState *transportStates,
					MotionConn *conn)
{
	int			bytesToReceive;
	int			bytesReceived;
	int			iconn;
	RegisterMessage *regMsg;
	RegisterMessage msg;
	MotionConn *newConn;
	ChunkTransportStateEntry *pEntry = NULL;
	CdbProcess *cdbproc;

	/* Get ready to receive the Register message. */
	if (conn->state != mcsRecvRegMsg)
	{
		conn->state = mcsRecvRegMsg;
		conn->msgSize = sizeof(*regMsg);
		conn->msgPos = conn->pBuff;

		Assert(conn->pBuff &&
			   sizeof(*regMsg) <= Gp_max_packet_size);
	}

	/* Receive all that is available, up to the expected message size. */
	for (;;)
	{
		bytesToReceive = conn->pBuff + conn->msgSize - conn->msgPos;
		bytesReceived = recv(conn->sockfd, conn->msgPos, bytesToReceive, 0);
		if (bytesReceived == bytesToReceive)
			break;
		else if (bytesReceived > 0)
			conn->msgPos += bytesReceived;
		else if (bytesReceived == 0)
		{
			elog(LOG, "Interconnect error reading register message from %s: connection closed",
				 conn->remoteHostAndPort);

			/* maybe this peer is already retrying ? */
			goto old_conn;
		}
		else if (errno == EWOULDBLOCK)
			return false;		/* call me again to receive the rest */
		else if (errno == EINTR)
			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
		else
		{
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error reading register message "
								   "from %s", conn->remoteHostAndPort),
							errdetail("%s sockfd=%d local=%s: %m",
									  "read",
									  conn->sockfd,
									  conn->localHostAndPort)));
		}
	}

	/*
	 * Got the whole message.  Convert fields to native byte order.
	 */
	regMsg = (RegisterMessage *) conn->pBuff;
	msg.msgBytes = regMsg->msgBytes;
	msg.recvSliceIndex = regMsg->recvSliceIndex;
	msg.sendSliceIndex = regMsg->sendSliceIndex;

	msg.srcContentId = regMsg->srcContentId;
	msg.srcListenerPort = regMsg->srcListenerPort;
	msg.srcPid = regMsg->srcPid;
	msg.srcSessionId = regMsg->srcSessionId;
	msg.srcCommandCount = regMsg->srcCommandCount;

	/* Check for valid message format. */
	if (msg.msgBytes != sizeof(*regMsg))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error reading register message "
							   "from %s: format not recognized",
							   conn->remoteHostAndPort),
						errdetail("msgBytes=%d expected=%d sockfd=%d local=%s",
								  msg.msgBytes, (int) sizeof(*regMsg),
								  conn->sockfd, conn->localHostAndPort)));
	}

	/* get rid of old connections first */
	if (msg.srcSessionId != gp_session_id ||
		msg.srcCommandCount < gp_interconnect_id)
	{
		/*
		 * This is an old connection, which can be safely ignored. We get this
		 * kind of stuff for cases in which one gang participating in the
		 * interconnect exited a query before calling SetupInterconnect().
		 * Later queries wind up receiving their registration messages.
		 */
		elog(LOG, "Received invalid, old registration message: "
			 "will ignore ('expected:received' session %d:%d ic-id %d:%d)",
			 gp_session_id, msg.srcSessionId,
			 gp_interconnect_id, msg.srcCommandCount);

		goto old_conn;
	}

	/* Verify that the message pertains to one of our receiving Motion nodes. */
	if (msg.sendSliceIndex > 0 &&
		msg.sendSliceIndex <= transportStates->size &&
		msg.recvSliceIndex == transportStates->sliceId &&
		msg.srcContentId >= -1)
	{
		/* this is a good connection */
	}
	else
	{
		/* something is wrong */
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error: Invalid registration "
							   "message received from %s.",
							   conn->remoteHostAndPort),
						errdetail("sendSlice=%d recvSlice=%d srcContentId=%d "
								  "srcPid=%d srcListenerPort=%d "
								  "srcSessionId=%d srcCommandCount=%d motnode=%d",
								  msg.sendSliceIndex, msg.recvSliceIndex,
								  msg.srcContentId, msg.srcPid,
								  msg.srcListenerPort, msg.srcSessionId,
								  msg.srcCommandCount, msg.sendSliceIndex)));
	}

	/*
	 * Find state info for the specified Motion node.  The sender's slice
	 * number equals the motion node id.
	 */
	getChunkTransportState(transportStates, msg.sendSliceIndex, &pEntry);
	Assert(pEntry);

	/*
	 * Find and verify the CdbProcess node for the sending process.
	 */
	if (list_length(pEntry->sendSlice->primaryProcesses) == 1)
		iconn = 0;
	else
		iconn = msg.srcContentId;

	cdbproc = (CdbProcess *) list_nth(pEntry->sendSlice->primaryProcesses, iconn);

	if (msg.srcContentId != cdbproc->contentid ||
		msg.srcListenerPort != cdbproc->listenerPort ||
		msg.srcPid != cdbproc->pid)
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error: Invalid registration "
							   "message received from %s.",
							   conn->remoteHostAndPort),
						errdetail("sendSlice=%d srcContentId=%d "
								  "srcPid=%d srcListenerPort=%d",
								  msg.sendSliceIndex, msg.srcContentId,
								  msg.srcPid, msg.srcListenerPort)));
	}

	/*
	 * Allocate MotionConn slot corresponding to sender's position in the
	 * sending slice's CdbProc list.
	 */
	newConn = getMotionConn(pEntry, iconn);

	if (newConn->sockfd != -1 ||
		newConn->state != mcsNull)
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error: Duplicate registration "
							   "message received from %s.",
							   conn->remoteHostAndPort),
						errdetail("Already accepted registration from %s for "
								  "sendSlice=%d srcContentId=%d "
								  "srcPid=%d srcListenerPort=%d",
								  newConn->remoteHostAndPort,
								  msg.sendSliceIndex, msg.srcContentId,
								  msg.srcPid, msg.srcListenerPort)));
	}

	/* message looks good */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
	{
		ereport(LOG, (errmsg("Interconnect seg%d slice%d sockfd=%d accepted "
							 "registration message from seg%d slice%d %s pid=%d",
							 Gp_segment,
							 msg.recvSliceIndex,
							 conn->sockfd,
							 msg.srcContentId,
							 msg.sendSliceIndex,
							 conn->remoteHostAndPort,
							 msg.srcPid)));
	}

	/* Copy caller's temporary MotionConn to its assigned slot. */
	*newConn = *conn;

	newConn->cdbProc = cdbproc;
	newConn->remoteContentId = msg.srcContentId;

	/*
	 * The caller's MotionConn object is no longer valid.
	 */
	MemSet(conn, 0, sizeof(*conn));
	conn->state = mcsNull;

	/*
	 * Prepare to begin reading tuples.
	 */
	newConn->state = mcsStarted;
	newConn->msgPos = NULL;
	newConn->msgSize = 0;
	newConn->stillActive = true;

	MPP_FD_SET(newConn->sockfd, &pEntry->readSet);

	if (newConn->sockfd > pEntry->highReadSock)
		pEntry->highReadSock = newConn->sockfd;

#ifdef AMS_VERBOSE_LOGGING
	dumpEntryConnections(DEBUG4, pEntry);
#endif

	/* we've completed registration of this connection */
	return true;

old_conn:
	shutdown(conn->sockfd, SHUT_RDWR);
	closesocket(conn->sockfd);
	conn->sockfd = -1;

	pfree(conn->pBuff);
	conn->pBuff = NULL;

	/*
	 * this connection is done, but with sockfd == -1 isn't a "success"
	 */
	return true;
}								/* readRegisterMessage */


/*
 * acceptIncomingConnection
 *
 * accept() a connection request that is pending on the listening socket.
 * Returns a newly palloc'ed MotionConn object; or NULL if the listening
 * socket does not have any pending connection requests.
 */
static MotionConn *
acceptIncomingConnection(void)
{
	int			newsockfd;
	socklen_t	addrsize;
	MotionConn *conn;
	struct sockaddr_storage remoteAddr;
	struct sockaddr_storage localAddr;

	/*
	 * Accept a connection.
	 */
	for (;;)
	{							/* loop until success or EWOULDBLOCK */
		MemSet(&remoteAddr, 0, sizeof(remoteAddr));
		addrsize = sizeof(remoteAddr);
		newsockfd = accept(TCP_listenerFd, (struct sockaddr *) &remoteAddr, &addrsize);
		if (newsockfd >= 0)
			break;

		switch (errno)
		{
			case EINTR:
				/* A signal arrived.  Loop to retry the accept(). */
				break;

			case EWOULDBLOCK:
				/* Connection request queue is empty.  Normal return. */
				return NULL;

			case EBADF:
			case EFAULT:
			case EINVAL:
#ifndef _WIN32
			case ENOTSOCK:
#endif
			case EOPNOTSUPP:
				/* Shouldn't get these errors unless there is a bug. */
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Interconnect error on listener port %d",
									   Gp_listener_port),
								errdetail("%s sockfd=%d: %m",
										  "accept",
										  TCP_listenerFd)));
				break;			/* not reached */
			case ENOMEM:
			case ENFILE:
			case EMFILE:
			case ENOBUFS:
				/* Out of resources. */
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Interconnect error on listener port %d",
									   Gp_listener_port),
								errdetail("%s sockfd=%d: %m",
										  "accept",
										  TCP_listenerFd)));
				break;			/* not reached */
			default:
				/* Network problem, connection aborted, etc.  Continue. */
				ereport(LOG, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							  errmsg("Interconnect connection request not "
									 "completed on listener port %d",
									 Gp_listener_port),
							  errdetail("%s sockfd=%d: %m",
										"accept",
										TCP_listenerFd)));
		}						/* switch (errno) */
	}							/* loop until success or EWOULDBLOCK */

	/*
	 * Create a MotionConn object to hold the connection state.
	 */
	conn = palloc0(sizeof(MotionConn));
	conn->sockfd = newsockfd;
	conn->pBuff = palloc(Gp_max_packet_size);
	conn->msgSize = 0;
	conn->recvBytes = 0;
	conn->msgPos = 0;
	conn->tupleCount = 0;
	conn->stillActive = false;
	conn->state = mcsAccepted;
	conn->remoteContentId = -2;

	/* Save remote and local host:port strings for error messages. */
	format_sockaddr((struct sockaddr *) &remoteAddr, conn->remoteHostAndPort,
					sizeof(conn->remoteHostAndPort));
	addrsize = sizeof(localAddr);
	if (getsockname(newsockfd, (struct sockaddr *) &localAddr, &addrsize))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error after accepting connection."),
						errdetail("%s sockfd=%d remote=%s: %m",
								  "getsockname",
								  newsockfd,
								  conn->remoteHostAndPort)));
	}
	format_sockaddr((struct sockaddr *) &localAddr, conn->localHostAndPort,
					sizeof(conn->localHostAndPort));

	/* make socket non-blocking */
	if (!pg_set_noblock(newsockfd))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error after accepting connection."),
						errdetail("%s sockfd=%d remote=%s local=%s: %m",
								  "fcntl(O_NONBLOCK)",
								  newsockfd,
								  conn->remoteHostAndPort,
								  conn->localHostAndPort)));
	}

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG4, "Interconnect got incoming connection "
			 "from remote=%s to local=%s sockfd=%d",
			 conn->remoteHostAndPort, conn->localHostAndPort, newsockfd);

	return conn;
}								/* acceptIncomingConnection */

/* See ml_ipc.h */
void
SetupTCPInterconnect(EState *estate)
{
	int			i,
				index,
				n;
	ListCell   *cell;
	Slice	   *mySlice;
	Slice	   *aSlice;
	MotionConn *conn;
	int			incoming_count = 0;
	int			outgoing_count = 0;
	int			expectedTotalIncoming = 0;
	int			expectedTotalOutgoing = 0;
	int			iteration = 0;
	GpMonotonicTime startTime;
	StringInfoData logbuf;
	uint64		elapsed_ms = 0;

	/* we can have at most one of these. */
	ChunkTransportStateEntry *sendingChunkTransportState = NULL;

	if (estate->interconnect_context)
	{
		elog(FATAL, "SetupTCPInterconnect: already initialized.");
	}
	else if (!estate->es_sliceTable)
	{
		elog(FATAL, "SetupTCPInterconnect: no slice table ?");
	}

	estate->interconnect_context = palloc0(sizeof(ChunkTransportState));

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

	estate->interconnect_context->RecvTupleChunkFrom = RecvTupleChunkFromTCP;
	estate->interconnect_context->RecvTupleChunkFromAny = RecvTupleChunkFromAnyTCP;
	estate->interconnect_context->SendEos = SendEosTCP;
	estate->interconnect_context->SendChunk = SendChunkTCP;
	estate->interconnect_context->doSendStopMessage = doSendStopMessageTCP;

	mySlice = (Slice *) list_nth(estate->interconnect_context->sliceTable->slices, LocallyExecutingSliceIndex(estate));

	Assert(estate->es_sliceTable &&
		   IsA(mySlice, Slice) &&
		   mySlice->sliceIndex == LocallyExecutingSliceIndex(estate));

	gp_interconnect_id = estate->interconnect_context->sliceTable->ic_instance_id;

	gp_set_monotonic_begin_time(&startTime);

	/* Initiate outgoing connections. */
	if (mySlice->parentIndex != -1)
		sendingChunkTransportState = startOutgoingConnections(estate->interconnect_context, mySlice, &expectedTotalOutgoing);

	/* now we'll do some setup for each of our Receiving Motion Nodes. */
	foreach(cell, mySlice->children)
	{
		int			totalNumProcs,
					activeNumProcs;
		int			childId = lfirst_int(cell);

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "Setting up RECEIVING motion node %d", childId);
#endif

		aSlice = (Slice *) list_nth(estate->interconnect_context->sliceTable->slices, childId);

		/*
		 * If we're using directed-dispatch we have dummy primary-process
		 * entries, so we count the entries.
		 */
		activeNumProcs = 0;
		totalNumProcs = list_length(aSlice->primaryProcesses);
		for (i = 0; i < totalNumProcs; i++)
		{
			CdbProcess *cdbProc;

			cdbProc = list_nth(aSlice->primaryProcesses, i);
			if (cdbProc)
				activeNumProcs++;
		}

		(void) createChunkTransportState(estate->interconnect_context, aSlice, mySlice, totalNumProcs);

		/* let cdbmotion now how many receivers to expect. */
		setExpectedReceivers(estate->motionlayer_context, childId, activeNumProcs);

		expectedTotalIncoming += activeNumProcs;
	}

	if (expectedTotalIncoming > listenerBacklog)
		ereport(WARNING, (errmsg("SetupTCPInterconnect: too many expected incoming connections(%d), Interconnect setup might possibly fail", expectedTotalIncoming),
						  errhint("Try enlarging the gp_interconnect_tcp_listener_backlog GUC value and OS net.core.somaxconn parameter")));

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		ereport(DEBUG1, (errmsg("SetupInterconnect will activate "
								"%d incoming, %d outgoing routes.  "
								"Listening on port=%d sockfd=%d.",
								expectedTotalIncoming, expectedTotalOutgoing,
								Gp_listener_port, TCP_listenerFd)));

	/*
	 * Loop until all connections are completed or time limit is exceeded.
	 */
	while (outgoing_count < expectedTotalOutgoing ||
		   incoming_count < expectedTotalIncoming)
	{							/* select() loop */
		struct timeval timeout;
		mpp_fd_set	rset,
					wset,
					eset;
		int			highsock = -1;
		uint64		timeout_ms = 20 * 60 * 1000;
		int			outgoing_fail_count = 0;

		iteration++;

		MPP_FD_ZERO(&rset);
		MPP_FD_ZERO(&wset);
		MPP_FD_ZERO(&eset);

		/* Expecting any new inbound connections? */
		if (incoming_count < expectedTotalIncoming)
		{
			if (TCP_listenerFd < 0)
			{
				elog(FATAL, "SetupTCPInterconnect: bad listener");
			}

			MPP_FD_SET(TCP_listenerFd, &rset);
			highsock = TCP_listenerFd;
		}

		/* Inbound connections awaiting registration message */
		foreach(cell, estate->interconnect_context->incompleteConns)
		{
			conn = (MotionConn *) lfirst(cell);

			if (conn->state != mcsRecvRegMsg || conn->sockfd < 0)
			{
				elog(FATAL, "SetupTCPInterconnect: incomplete connection bad state or bad fd");
			}

			MPP_FD_SET(conn->sockfd, &rset);
			highsock = Max(highsock, conn->sockfd);
		}

		/* Outgoing connections */
		outgoing_count = 0;
		n = sendingChunkTransportState ? sendingChunkTransportState->numConns : 0;

		for (i = 0; i < n; i++)
		{
			index = i;

			conn = &sendingChunkTransportState->conns[index];

			/* Time to cancel incomplete connect() and retry? */
			if (conn->state == mcsConnecting &&
				conn->wakeup_ms > 0 &&
				conn->wakeup_ms <= elapsed_ms + 20)
			{
				ereport(LOG, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							  errmsg("Interconnect timeout: Connection "
									 "to seg%d %s from local port %s was not "
									 "complete after " UINT64_FORMAT
									 "ms " UINT64_FORMAT " elapsed.  Will retry.",
									 conn->remoteContentId,
									 conn->remoteHostAndPort,
									 conn->localHostAndPort,
									 conn->wakeup_ms, (elapsed_ms + 20))));
				conn->state = mcsSetupOutgoingConnection;
			}

			/* Time to connect? */
			if (conn->state == mcsSetupOutgoingConnection &&
				conn->wakeup_ms <= elapsed_ms + 20)
			{
				setupOutgoingConnection(estate->interconnect_context, sendingChunkTransportState, conn);
				switch (conn->state)
				{
					case mcsSetupOutgoingConnection:
						/* Retry failed connection after awhile. */
						conn->wakeup_ms = (iteration - 1) * 1000 + elapsed_ms;
						break;
					case mcsConnecting:
						/* Set time limit for connect() to complete. */
						if (estate->interconnect_context->aggressiveRetry)
							conn->wakeup_ms = CONNECT_AGGRESSIVERETRY_MS + elapsed_ms;
						else
							conn->wakeup_ms = CONNECT_RETRY_MS + elapsed_ms;
						break;
					default:
						conn->wakeup_ms = 0;
						break;
				}
			}

			/* What events are we watching for? */
			switch (conn->state)
			{
				case mcsNull:
					break;
				case mcsSetupOutgoingConnection:
					outgoing_fail_count++;
					break;
				case mcsConnecting:
					if (conn->sockfd < 0)
					{
						elog(FATAL, "SetupTCPInterconnect: bad fd, mcsConnecting");
					}

					MPP_FD_SET(conn->sockfd, &wset);
					MPP_FD_SET(conn->sockfd, &eset);
					highsock = Max(highsock, conn->sockfd);
					break;
				case mcsSendRegMsg:
					if (conn->sockfd < 0)
					{
						elog(FATAL, "SetupTCPInterconnect: bad fd, mcsSendRegMsg");
					}
					MPP_FD_SET(conn->sockfd, &wset);
					highsock = Max(highsock, conn->sockfd);
					break;
				case mcsStarted:
					outgoing_count++;
					break;
				default:
					elog(FATAL, "SetupTCPInterconnect: bad connection state");
			}

			if (conn->wakeup_ms > 0)
				timeout_ms = Min(timeout_ms, conn->wakeup_ms - elapsed_ms);
		}						/* loop to set up outgoing connections */

		/* Break out of select() loop if completed all connections. */
		if (outgoing_count == expectedTotalOutgoing &&
			incoming_count == expectedTotalIncoming)
			break;

		/*
		 * Been here long?  Bail if gp_interconnect_setup_timeout exceeded.
		 */
		if (interconnect_setup_timeout > 0)
		{
			int			to = interconnect_setup_timeout * 1000;

			if (to <= elapsed_ms + 20)
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
										  interconnect_setup_timeout)
								));
			/* don't wait for more than 500ms */
			timeout_ms = Min(500, Min(timeout_ms, to - elapsed_ms));
		}

		/*
		 * If no socket events to wait for, loop to retry after a pause.
		 */
		if (highsock < 0)
		{
			if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE &&
				(timeout_ms > 0 || iteration > 2))
				ereport(LOG, (errmsg("SetupInterconnect+" UINT64_FORMAT
									 "ms:   pause " UINT64_FORMAT "ms   "
									 "outgoing_fail=%d iteration=%d",
									 elapsed_ms, timeout_ms,
									 outgoing_fail_count, iteration)
							  ));

			/* Shouldn't be in this loop unless we have some work to do. */
			if (outgoing_fail_count <= 0)
			{
				elog(FATAL, "SetupInterconnect: invalid outgoing count");
			}

			/* Wait until earliest wakeup time or overall timeout. */
			if (timeout_ms > 0)
			{
				ML_CHECK_FOR_INTERRUPTS(estate->interconnect_context->teardownActive);
				pg_usleep(timeout_ms * 1000);
				ML_CHECK_FOR_INTERRUPTS(estate->interconnect_context->teardownActive);
			}

			/* Back to top of loop and look again. */
			elapsed_ms = gp_get_elapsed_ms(&startTime);
			continue;
		}

		/*
		 * Wait for socket events.
		 *
		 * In order to handle errors at intervals less than the full timeout
		 * length, we limit our select(2) wait to a maximum of 500ms.
		 */
		if (timeout_ms > 0)
		{
			timeout.tv_sec = timeout_ms / 1000; /* 0 */
			timeout.tv_usec = (timeout_ms - (timeout.tv_sec * 1000)) * 1000;
			Assert(timeout_ms == timeout.tv_sec * 1000 + timeout.tv_usec / 1000);
		}
		else
			timeout.tv_sec = timeout.tv_usec = 0;

		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		{
			initStringInfo(&logbuf);

			format_fd_set(&logbuf, highsock + 1, &rset, "r={", "} ");
			format_fd_set(&logbuf, highsock + 1, &wset, "w={", "} ");
			format_fd_set(&logbuf, highsock + 1, &eset, "e={", "}");

			elapsed_ms = gp_get_elapsed_ms(&startTime);

			ereport(DEBUG1, (errmsg("SetupInterconnect+" UINT64_FORMAT
									"ms:   select()  "
									"Interest: %s.  timeout=" UINT64_FORMAT "ms "
									"outgoing_fail=%d iteration=%d",
									elapsed_ms, logbuf.data, timeout_ms,
									outgoing_fail_count, iteration)));
			pfree(logbuf.data);
			MemSet(&logbuf, 0, sizeof(logbuf));
		}

		ML_CHECK_FOR_INTERRUPTS(estate->interconnect_context->teardownActive);
		n = select(highsock + 1, (fd_set *) &rset, (fd_set *) &wset, (fd_set *) &eset, &timeout);
		ML_CHECK_FOR_INTERRUPTS(estate->interconnect_context->teardownActive);

		elapsed_ms = gp_get_elapsed_ms(&startTime);

		/*
		 * Log the select() if requested.
		 */
		if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
		{
			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG ||
				n != expectedTotalIncoming + expectedTotalOutgoing)
			{
				int			elevel = (n == expectedTotalIncoming + expectedTotalOutgoing)
				? DEBUG1 : LOG;
				int			errnoSave = errno;

				initStringInfo(&logbuf);
				if (n > 0)
				{
					appendStringInfo(&logbuf, "result=%d  Ready: ", n);
					format_fd_set(&logbuf, highsock + 1, &rset, "r={", "} ");
					format_fd_set(&logbuf, highsock + 1, &wset, "w={", "} ");
					format_fd_set(&logbuf, highsock + 1, &eset, "e={", "}");
				}
				else
					appendStringInfoString(&logbuf, n < 0 ? "error" : "timeout");
				ereport(elevel, (errmsg("SetupInterconnect+" UINT64_FORMAT "ms:   select()  %s",
										elapsed_ms, logbuf.data)));
				pfree(logbuf.data);
				MemSet(&logbuf, 0, sizeof(logbuf));
				errno = errnoSave;
			}
		}

		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error: %s: %m", "select")));
		}

		/*
		 * check our connections that are accepted'd but no register message.
		 * we don't know which motion node these apply to until we actually
		 * receive the REGISTER message.  this is why they are all in a single
		 * list.
		 *
		 * NOTE: we don't use foreach() here because we want to trim from the
		 * list as we go.
		 *
		 * We used to bail out of the while loop when incoming_count hit
		 * expectedTotalIncoming, but that causes problems if some connections
		 * are left over -- better to just process them here.
		 */
		cell = list_head(estate->interconnect_context->incompleteConns);
		while (n > 0 && cell != NULL)
		{
			conn = (MotionConn *) lfirst(cell);

			/*
			 * we'll get the next cell ready now in case we need to delete the
			 * cell that corresponds to our MotionConn
			 */
			cell = lnext(cell);

			if (MPP_FD_ISSET(conn->sockfd, &rset))
			{
				n--;
				if (readRegisterMessage(estate->interconnect_context, conn))
				{
					/*
					 * We're done with this connection (either it is bogus
					 * (and has been dropped), or we've added it to the
					 * appropriate hash table)
					 */
					estate->interconnect_context->incompleteConns = list_delete_ptr(estate->interconnect_context->incompleteConns, conn);

					/* is the connection ready ? */
					if (conn->sockfd != -1)
						incoming_count++;

					if (conn->pBuff)
						pfree(conn->pBuff);
					/* Free temporary MotionConn storage. */
					pfree(conn);
				}
			}
		}

		/*
		 * Someone tickling our listener port?  Accept pending connections.
		 */
		if (MPP_FD_ISSET(TCP_listenerFd, &rset))
		{
			n--;
			while ((conn = acceptIncomingConnection()) != NULL)
			{
				/*
				 * get the connection read for a subsequent call to
				 * ReadRegisterMessage()
				 */
				conn->state = mcsRecvRegMsg;
				conn->msgSize = sizeof(RegisterMessage);
				conn->msgPos = conn->pBuff;
				conn->remapper = CreateTupleRemapper();

				estate->interconnect_context->incompleteConns = lappend(estate->interconnect_context->incompleteConns, conn);
			}
		}

		/*
		 * Check our outgoing connections.
		 */
		i = 0;
		while (n > 0 &&
			   outgoing_count < expectedTotalOutgoing &&
			   i < sendingChunkTransportState->numConns)
		{						/* loop to check outgoing connections */
			conn = &sendingChunkTransportState->conns[i++];
			switch (conn->state)
			{
				case mcsConnecting:
					/* Has connect() succeeded or failed? */
					if (MPP_FD_ISSET(conn->sockfd, &wset) ||
						MPP_FD_ISSET(conn->sockfd, &eset))
					{
						n--;
						updateOutgoingConnection(estate->interconnect_context, sendingChunkTransportState, conn, -1);
						switch (conn->state)
						{
							case mcsSetupOutgoingConnection:
								/* Failed.  Wait awhile before retrying. */
								conn->wakeup_ms = (iteration - 1) * 1000 + elapsed_ms;
								break;
							case mcsSendRegMsg:
								/* Connected, but reg msg not fully sent. */
								conn->wakeup_ms = 0;
								break;
							case mcsStarted:
								/* Connected, sent reg msg, ready to rock. */
								outgoing_count++;
								break;
							default:
								elog(FATAL, "SetupInterconnect: bad outgoing state");
						}
					}
					break;

				case mcsSendRegMsg:
					/* Ready to continue sending? */
					if (MPP_FD_ISSET(conn->sockfd, &wset))
					{
						n--;
						sendRegisterMessage(estate->interconnect_context, sendingChunkTransportState, conn);
						if (conn->state == mcsStarted)
							outgoing_count++;
					}
					break;

				default:
					break;
			}

		}						/* loop to check outgoing connections */

		/* By now we have dealt with all the events reported by select(). */
		if (n != 0)
			elog(FATAL, "SetupInterconnect: extra select events.");
	}							/* select() loop */

	/*
	 * if everything really got setup properly then we shouldn't have any
	 * incomplete connections.
	 *
	 * XXX: In some cases (when the previous query got 'fast-track cancelled'
	 * because of an error during setup) we can wind up with connections here
	 * which ought to have been cleaned up. These connections should be closed
	 * out here. It would obviously be better if we could avoid these
	 * connections in the first place!
	 */
	if (list_length(estate->interconnect_context->incompleteConns) != 0)
	{
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG2, "Incomplete connections after known connections done, cleaning %d",
				 list_length(estate->interconnect_context->incompleteConns));

		while ((cell = list_head(estate->interconnect_context->incompleteConns)) != NULL)
		{
			conn = (MotionConn *) lfirst(cell);

			if (conn->sockfd != -1)
			{
				flushIncomingData(conn->sockfd);
				shutdown(conn->sockfd, SHUT_WR);
				closesocket(conn->sockfd);
				conn->sockfd = -1;
			}

			estate->interconnect_context->incompleteConns = list_delete_ptr(estate->interconnect_context->incompleteConns, conn);

			if (conn->pBuff)
				pfree(conn->pBuff);
			pfree(conn);
		}
	}

	estate->interconnect_context->activated = true;

	if (gp_log_interconnect >= GPVARS_VERBOSITY_TERSE)
	{
		elapsed_ms = gp_get_elapsed_ms(&startTime);
		if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE ||
			elapsed_ms >= 0.1 * 1000 * interconnect_setup_timeout)
			elog(LOG, "SetupInterconnect+" UINT64_FORMAT "ms: Activated %d incoming, "
				 "%d outgoing routes.",
				 elapsed_ms, incoming_count, outgoing_count);
	}
}								/* SetupInterconnect */

/* TeardownInterconnect() function is used to cleanup interconnect resources that
 * were allocated during SetupInterconnect().  This function should ALWAYS be
 * called after SetupInterconnect to avoid leaking resources (like sockets)
 * even if SetupInterconnect did not complete correctly.  As a result, this
 * function must complete successfully even if SetupInterconnect didn't.
 *
 * SetupInterconnect() always gets called under the ExecutorState MemoryContext.
 * This context is destroyed at the end of the query and all memory that gets
 * allocated under it is free'd.  We don't have have to worry about pfree() but
 * we definitely have to worry about socket resources.
 *
 * If forceEOS is set, we force end-of-stream notifications out any send-nodes,
 * and we wrap that send in a PG_TRY/CATCH block because the goal is to reduce
 * confusion (and if we're being shutdown abnormally anyhow, let's not start
 * adding errors!).
 */
void
TeardownTCPInterconnect(ChunkTransportState *transportStates,
						MotionLayerState *mlStates,
						bool forceEOS, bool hasError)
{
	ListCell   *cell;
	ChunkTransportStateEntry *pEntry = NULL;
	int			i;
	Slice	   *mySlice;
	MotionConn *conn;

	if (transportStates->sliceTable == NULL)
	{
		elog(LOG, "TeardownUDPInterconnect: missing slice table.");
		return;
	}

	/*
	 * if we're already trying to clean up after an error -- don't allow
	 * signals to interrupt us
	 */
	if (forceEOS)
		HOLD_INTERRUPTS();

	mySlice = (Slice *) list_nth(transportStates->sliceTable->slices, transportStates->sliceId);

	/* Log the start of TeardownInterconnect. */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_TERSE)
	{
		int			elevel = 0;

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

	/*
	 * phase 1 mark all sockets (senders and receivers) with shutdown(2),
	 * start with incomplete connections (if any).
	 */

	/*
	 * The incompleteConns list is only used as a staging area for MotionConns
	 * during by SetupInterconnect().  So we only expect to have entries here
	 * if SetupInterconnect() did not finish correctly.
	 *
	 * NOTE: we don't use foreach() here because we want to trim from the list
	 * as we go.
	 */
	if (transportStates->incompleteConns &&
		gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG2, "Found incomplete conn. length %d", list_length(transportStates->incompleteConns));

	/*
	 * These are connected inbound peers that we haven't dealt with quite yet
	 */
	while ((cell = list_head(transportStates->incompleteConns)) != NULL)
	{
		MotionConn *conn = (MotionConn *) lfirst(cell);

		/* they're incomplete, so just slam them shut. */
		if (conn->sockfd != -1)
		{
			flushIncomingData(conn->sockfd);
			shutdown(conn->sockfd, SHUT_WR);
			closesocket(conn->sockfd);
			conn->sockfd = -1;
		}

		/* free up the tuple remapper */
		if (conn->remapper)
		{
			DestroyTupleRemapper(conn->remapper);
			conn->remapper = NULL;
		}

		/*
		 * The list operations are kind of confusing (see list.c), we could
		 * alternatively write the following line as:
		 *
		 * incompleteConns = list_delete_cell(incompleteConns, cell, NULL); or
		 * incompleteConns = list_delete_first(incompleteConns); or
		 * incompleteConns = list_delete_ptr(incompleteConns, conn)
		 */
		transportStates->incompleteConns = list_delete(transportStates->incompleteConns, conn);
	}

	list_free(transportStates->incompleteConns);
	transportStates->incompleteConns = NIL;

	/*
	 * Now "normal" connections which made it through our peer-registration
	 * step. With these we have to worry about "in-flight" data.
	 */
	if (mySlice->parentIndex != -1)
	{
		/* cleanup a Sending motion node. */
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG3, "Interconnect seg%d slice%d closing connections to slice%d",
				 Gp_segment, mySlice->sliceIndex, mySlice->parentIndex);

		getChunkTransportState(transportStates, mySlice->sliceIndex, &pEntry);

		if (forceEOS && !hasError)
			forceEosToPeers(mlStates, transportStates, mySlice->sliceIndex);

		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;
			if (conn->sockfd >= 0)
				shutdown(conn->sockfd, SHUT_WR);

			/* free up the tuple remapper */
			if (conn->remapper)
			{
				DestroyTupleRemapper(conn->remapper);
				conn->remapper = NULL;
			}
		}
	}

	/*
	 * cleanup all of our Receiving Motion nodes, these get closed immediately
	 * (the receiver know for real if they want to shut down -- they aren't
	 * going to be processing any more data).
	 */
	foreach(cell, mySlice->children)
	{
		Slice	   *aSlice;
		int			childId = lfirst_int(cell);

		aSlice = (Slice *) list_nth(transportStates->sliceTable->slices, childId);

		getChunkTransportState(transportStates, aSlice->sliceIndex, &pEntry);

		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG3, "Interconnect closing connections from slice%d",
				 aSlice->sliceIndex);

		/*
		 * receivers know that they no longer care about data from below ...
		 * so we can safely discard data queued in both directions
		 */
		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			if (conn->sockfd >= 0)
			{
				flushIncomingData(conn->sockfd);
				shutdown(conn->sockfd, SHUT_WR);

				closesocket(conn->sockfd);
				conn->sockfd = -1;

				/* free up the tuple remapper */
				if (conn->remapper)
				{
					DestroyTupleRemapper(conn->remapper);
					conn->remapper = NULL;
				}

			}
		}
		removeChunkTransportState(transportStates, aSlice->sliceIndex);
		pfree(pEntry->conns);
	}

	/*
	 * phase 2: wait on all sockets for completion, when complete call close
	 * and free (if required)
	 */
	if (mySlice->parentIndex != -1)
	{
		/* cleanup a Sending motion node. */
		getChunkTransportState(transportStates, mySlice->sliceIndex, &pEntry);

		if (!forceEOS)
			waitOnOutbound(pEntry);

		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			if (conn->sockfd >= 0)
			{
				closesocket(conn->sockfd);
				conn->sockfd = -1;
			}
		}
		pEntry = removeChunkTransportState(transportStates, mySlice->sliceIndex);
	}

	/*
	 * If there are clients waiting on our listener; we *must* disconnect
	 * them; otherwise we'll be out of sync with the client (we may accept
	 * them on a subsequent query!)
	 */
	if (TCP_listenerFd != -1)
		flushInterconnectListenerBacklog();

	transportStates->activated = false;
	transportStates->sliceTable = NULL;

	if (transportStates->states != NULL)
		pfree(transportStates->states);
	pfree(transportStates);

	if (forceEOS)
		RESUME_INTERRUPTS();

#ifdef AMS_VERBOSE_LOGGING
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG4, "TeardownInterconnect successful");
#endif
}

#ifdef AMS_VERBOSE_LOGGING
void
print_connection(ChunkTransportState *transportStates, int fd, const char *msg)
{
	struct sockaddr_in local,
				remote;

	socklen_t	len;
	int			errlevel = transportStates->teardownActive ? LOG : ERROR;

	len = sizeof(remote);
	if (getpeername(fd, (struct sockaddr *) &remote, &len) < 0)
	{
		elog(errlevel, "print_connection(%d, %s): can't get peername err: %m",
			 fd, msg);
	}

	len = sizeof(local);
	if (getsockname(fd, (struct sockaddr *) &local, &len) < 0)
	{
		elog(errlevel, "print_connection(%d, %s): can't get localname err: %m",
			 fd, msg);
	}

	elog(DEBUG2, "%s: w/ports (%d/%d)",
		 msg, ntohs(local.sin_port), ntohs(remote.sin_port));
}
#endif

void
format_fd_set(StringInfo buf, int nfds, mpp_fd_set *fds, char *pfx, char *sfx)
{
	int			i;

	appendStringInfoString(buf, pfx);
	for (i = 1; i < nfds; i++)
	{
		if (MPP_FD_ISSET(i, fds))
			appendStringInfo(buf, "%d,", i);
	}

	if (buf->len > 0 &&
		buf->data[buf->len - 1] == ',')
		truncateStringInfo(buf, buf->len - 1);

	appendStringInfoString(buf, sfx);
}

char *
format_sockaddr(struct sockaddr *sa, char *buf, int bufsize)
{
	/* Save remote host:port string for error messages. */
	if (sa->sa_family == AF_INET)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *) sa;
		uint32		saddr = ntohl(sin->sin_addr.s_addr);

		snprintf(buf, bufsize, "%d.%d.%d.%d:%d",
				 (saddr >> 24) & 0xff,
				 (saddr >> 16) & 0xff,
				 (saddr >> 8) & 0xff,
				 saddr & 0xff,
				 ntohs(sin->sin_port));
	}
#ifdef HAVE_IPV6
	else if (sa->sa_family == AF_INET6)
	{
		char		remote_port[32];

		if (bufsize > 10)
		{
			buf[0] = '[';

			/*
			 * inet_ntop isn't portable. //inet_ntop(AF_INET6,
			 * &sin6->sin6_addr, buf, bufsize - 8);
			 *
			 * postgres has a standard routine for converting addresses to
			 * printable format, which works for IPv6, IPv4, and Unix domain
			 * sockets.  I've changed this routine to use that, but I think
			 * the entire format_sockaddr routine could be replaced with it.
			 */
			int			ret = pg_getnameinfo_all((const struct sockaddr_storage *) sa, sizeof(struct sockaddr_storage),
												 buf + 1, bufsize - 10,
												 remote_port, sizeof(remote_port),
												 NI_NUMERICHOST | NI_NUMERICSERV);

			if (ret != 0)
			{
				elog(LOG, "getnameinfo returned %d: %s, and says %s port %s", ret, gai_strerror(ret), buf, remote_port);

				/*
				 * Fall back to using our internal inet_ntop routine, which
				 * really is for inet datatype This is because of a bug in
				 * solaris, where getnameinfo sometimes fails Once we find out
				 * why, we can remove this
				 */
				snprintf(remote_port, sizeof(remote_port), "%d", ((struct sockaddr_in6 *) sa)->sin6_port);

				/*
				 * This is nasty: our internal inet_net_ntop takes
				 * PGSQL_AF_INET6, not AF_INET6, which is very odd... They are
				 * NOT the same value (even though PGSQL_AF_INET == AF_INET
				 */
#define PGSQL_AF_INET6	(AF_INET + 1)
				inet_net_ntop(PGSQL_AF_INET6, sa, sizeof(struct sockaddr_in6), buf + 1, bufsize - 10);
				elog(LOG, "Our alternative method says %s]:%s", buf, remote_port);

			}
			buf += strlen(buf);
			strcat(buf, "]");
			buf++;
		}
		snprintf(buf, 8, ":%s", remote_port);
	}
#endif
	else
		snprintf(buf, bufsize, "?host?:?port?");

	return buf;
}								/* format_sockaddr */

static void
flushInterconnectListenerBacklog(void)
{
	int			pendingConn,
				newfd,
				i;
	mpp_fd_set	rset;
	struct timeval timeout;

	do
	{
		MPP_FD_ZERO(&rset);
		MPP_FD_SET(TCP_listenerFd, &rset);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		pendingConn = select(TCP_listenerFd + 1, (fd_set *) &rset, NULL, NULL, &timeout);
		if (pendingConn > 0)
		{
			for (i = 0; i < pendingConn; i++)
			{
				struct sockaddr_storage remoteAddr;
				struct sockaddr_storage localAddr;
				char		remoteHostAndPort[64];
				char		localHostAndPort[64];
				socklen_t	addrsize;

				addrsize = sizeof(remoteAddr);
				newfd = accept(TCP_listenerFd, (struct sockaddr *) &remoteAddr, &addrsize);
				if (newfd < 0)
				{
					ereport(DEBUG3, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									 errmsg("Interconnect error while clearing incoming connections."),
									 errdetail("%s sockfd=%d: %m", "accept", newfd)));
					continue;
				}

				if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
				{
					/* Get remote and local host:port strings for message. */
					format_sockaddr((struct sockaddr *) &remoteAddr, remoteHostAndPort,
									sizeof(remoteHostAndPort));
					addrsize = sizeof(localAddr);
					if (getsockname(newfd, (struct sockaddr *) &localAddr, &addrsize))
					{
						ereport(LOG, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
									  errmsg("Interconnect error while clearing incoming connections."),
									  errdetail("%s sockfd=%d remote=%s: %m",
												"getsockname",
												newfd,
												remoteHostAndPort)));
					}
					else
					{
						format_sockaddr((struct sockaddr *) &localAddr, localHostAndPort,
										sizeof(localHostAndPort));
						ereport(DEBUG2, (errmsg("Interconnect clearing incoming connection "
												"from remote=%s to local=%s.  sockfd=%d.",
												remoteHostAndPort, localHostAndPort,
												newfd)));
					}
				}

				/* make socket non-blocking */
				if (!pg_set_noblock(newfd))
				{
					elog(LOG, "During incoming queue flush, could not set non-blocking.");
				}
				else
				{
					/* shutdown this socket */
					flushIncomingData(newfd);
				}

				shutdown(newfd, SHUT_WR);
				closesocket(newfd);
			}
		}
		else if (pendingConn < 0 && errno != EINTR)
		{
			ereport(LOG, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						  errmsg("Interconnect error during listener cleanup."),
						  errdetail("%s sockfd=%d: %m", "select", TCP_listenerFd)));
		}

		/*
		 * now we either loop through for another check (on EINTR or if we
		 * cleaned one client) or we're done
		 */
	}
	while (pendingConn != 0);
}

/*
 * Wait for our peer to close the socket (at which point our select(2)
 * will tell us that the socket is ready to read, and the socket-read
 * will only return 0.
 *
 * This works without the select, but burns tons of CPU doing nothing
 * useful.
 *
 * ----
 * The way it used to work, is we used CHECK_FOR_INTERRUPTS(), and
 * wrapped it in PG_TRY: We *must* return locally; otherwise
 * TeardownInterconnect() can't exit cleanly. So we wrap our
 * cancel-detection checks for interrupts with a PG_TRY block.
 *
 * By swallowing the non-local return on cancel, we lose the "cancel"
 * state (CHECK_FOR_INTERRUPTS() clears QueryCancelPending()). So we
 * should just check QueryCancelPending here ... and avoid calling
 * CHECK_FOR_INTERRUPTS().
 *
 * ----
 *
 * Now we just check explicitly for interrupts (which is, as far as I
 * can tell, the only interrupt-driven state change we care
 * about). This should give us notification of ProcDiePending and
 * QueryCancelPending
 */
static void
waitOnOutbound(ChunkTransportStateEntry *pEntry)
{
	MotionConn *conn;

	struct timeval timeout;
	mpp_fd_set	waitset,
				curset;
	int			maxfd = -1;
	int			i,
				n,
				conn_count = 0;

	MPP_FD_ZERO(&waitset);

	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;

		if (conn->sockfd >= 0)
		{
			MPP_FD_SET(conn->sockfd, &waitset);
			if (conn->sockfd > maxfd)
				maxfd = conn->sockfd;
			conn_count++;
		}
	}

	for (;;)
	{
		int			saved_err;

		if (conn_count == 0)
			return;

		if (InterruptPending)
		{
#ifdef AMS_VERBOSE_LOGGING
			elog(DEBUG3, "waitOnOutbound(): interrupt pending fast-track");
#endif
			return;
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;

		memcpy(&curset, &waitset, sizeof(mpp_fd_set));

		n = select(maxfd + 1, (fd_set *) &curset, NULL, NULL, &timeout);
		if (n == 0 || (n < 0 && errno == EINTR))
		{
			continue;
		}
		else if (n < 0)
		{
			saved_err = errno;

			if (InterruptPending)
				return;

			/*
			 * Something unexpected, but probably not horrible warn and return
			 */
			elog(LOG, "TeardownTCPInterconnect: waitOnOutbound select errno=%d", saved_err);
			break;
		}

		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			if (conn->sockfd >= 0 && MPP_FD_ISSET(conn->sockfd, &curset))
			{
				int			count;
				char		buf;

				/* ready to read. */
				count = recv(conn->sockfd, &buf, sizeof(buf), 0);
				if (count == 0) /* done ! */
				{
					MPP_FD_CLR(conn->sockfd, &waitset);
					/* we may have finished */
					conn_count--;
					continue;
				}
				if (count > 0 || (count < 0 && errno == EAGAIN))
					continue;
				/* Some other kind of error happened */
				if (errno == EINTR)
					continue;

				/*
				 * Something unexpected, but probably not horrible warn and
				 * return
				 */
				MPP_FD_CLR(conn->sockfd, &waitset);
				/* we may have finished */
				conn_count--;
				elog(LOG, "TeardownTCPInterconnect: waitOnOutbound %s: %m", "recv");
				continue;
			}
		}
	}

	return;
}

static void
doSendStopMessageTCP(ChunkTransportState *transportStates, int16 motNodeID)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;
	int			i;
	char		m = 'S';
	ssize_t		written;

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

		if (conn->sockfd >= 0 &&
			MPP_FD_ISSET(conn->sockfd, &pEntry->readSet))
		{
			/* someone is trying to send stuff to us, let's stop 'em */
			while ((written = send(conn->sockfd, &m, sizeof(m), 0)) < 0)
			{
				if (errno == EINTR)
				{
					ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
					continue;
				}
				else
					break;
			}

			if (written != sizeof(m))
			{
				/*
				 * how can this happen ? the kernel buffer should be empty in
				 * the send direction
				 */
				elog(LOG, "SendStopMessage: failed on write.  %m");
			}
		}
		/* CRITICAL TO AVOID DEADLOCK */
		DeregisterReadInterest(transportStates, motNodeID, i,
							   "no more input needed");
	}
}

static TupleChunkListItem
RecvTupleChunkFromTCP(ChunkTransportState *transportStates,
					  int16 motNodeID,
					  int16 srcRoute)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "RecvTupleChunkFrom(motNodID=%d, srcpIncIdx %d srcRoute=%d)", motNodeID, srcpInc, srcRoute);
#endif

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	conn = pEntry->conns + srcRoute;

	return RecvTupleChunk(conn, transportStates);
}

static TupleChunkListItem
RecvTupleChunkFromAnyTCP(MotionLayerState *mlStates,
						 ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 *srcRoute)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionNodeEntry *pMNEntry;
	MotionConn *conn;
	TupleChunkListItem tcItem;
	mpp_fd_set	rset;
	int			n,
				i,
				index;
	bool		skipSelect = false;



#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "RecvTupleChunkFromAny(motNodeId=%d)", motNodeID);
#endif

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	pMNEntry = getMotionNodeEntry(mlStates, motNodeID, "RecvTupleChunkFromAny");

	int			retry = 0;

	do
	{
		/* Every 2 seconds */
		if (retry++ > 4)
		{
			retry = 0;
			/* check to see if the dispatcher should cancel */
			if (Gp_role == GP_ROLE_DISPATCH)
				checkForCancelFromQD(transportStates);
		}

		struct timeval timeout = tval;

		/* make sure we check for these. */
		ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

		memcpy(&rset, &pEntry->readSet, sizeof(mpp_fd_set));

		/*
		 * since we may have data in a local buffer, we may be able to
		 * short-circuit the select() call (and if we don't do this we may
		 * wait when we have data ready, since it has already been read)
		 */
		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			if (conn->sockfd >= 0 &&
				MPP_FD_ISSET(conn->sockfd, &rset) &&
				conn->recvBytes != 0)
			{
				/* we have data on this socket, let's short-circuit our select */
				MPP_FD_ZERO(&rset);
				MPP_FD_SET(conn->sockfd, &rset);

				skipSelect = true;
			}
		}
		if (skipSelect)
			break;

		n = select(pEntry->highReadSock + 1, (fd_set *) &rset, NULL, NULL, &timeout);
		pMNEntry->sel_rd_wait += (tval.tv_sec - timeout.tv_sec) * 1000000 + (tval.tv_usec - timeout.tv_usec);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg("Interconnect error receiving an incoming packet."),
							errdetail("%s: %m", "select")));
		}
#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "RecvTupleChunkFromAny() select() returned %d ready sockets", n);
#endif
	} while (n < 1);

	/*
	 * We scan the file descriptors starting from where we left off in the
	 * last call (don't continually poll the first when others may be ready!).
	 */
	index = pEntry->scanStart;
	for (i = 0; i < pEntry->numConns; i++, index++)
	{
		/*
		 * avoid division ? index = ((scanStart + i) % pEntry->numConns);
		 */
		if (index >= pEntry->numConns)
			index = 0;

		conn = pEntry->conns + index;

#ifdef AMS_VERBOSE_LOGGING
		if (!conn->stillActive)
		{
			elog(LOG, "RecvTupleChunkFromAny: trying to read on inactive socket %d", conn->sockfd);
		}
#endif

		if (conn->sockfd >= 0 &&
			MPP_FD_ISSET(conn->sockfd, &rset))
		{
#ifdef AMS_VERBOSE_LOGGING
			elog(DEBUG5, "RecvTupleChunkFromAny() (fd %d) %d/%d", conn->sockfd, motNodeID, index);
#endif
			tcItem = RecvTupleChunk(conn, transportStates);

			*srcRoute = index;

			/*
			 * advance start point (avoid doing division/modulus operation
			 * here)
			 */
			pEntry->scanStart = index + 1;

			return tcItem;
		}
	}

	/* we should never ever get here... */
	elog(FATAL, "RecvTupleChunkFromAnyTCP: didn't receive, and didn't get cancelled");
	return NULL;				/* keep the compiler happy */
}

/* See ml_ipc.h */
static void
SendEosTCP(MotionLayerState *mlStates,
		   ChunkTransportState *transportStates,
		   int motNodeID,
		   TupleChunkListItem tcItem)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;
	int			i;

	if (!transportStates)
	{
		elog(FATAL, "SendEosTCP: missing interconnect context.");
	}
	else if (!transportStates->activated && !transportStates->teardownActive)
	{
		elog(FATAL, "SendEosTCP: context and teardown inactive.");
	}

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG3, "Interconnect seg%d slice%d sending end-of-stream to slice%d",
			 Gp_segment, motNodeID, pEntry->recvSlice->sliceIndex);

	/*
	 * we want to add our tcItem onto each of the outgoing buffers -- this is
	 * guaranteed to leave things in a state where a flush is *required*.
	 */
	doBroadcast(mlStates, transportStates, pEntry, tcItem, NULL);

	/* now flush all of the buffers. */
	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;

		if (conn->sockfd >= 0 && conn->state == mcsStarted)
			flushBuffer(mlStates, transportStates, pEntry, conn, motNodeID);

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "SendEosTCP() Leaving");
#endif
	}

	return;
}

static bool
flushBuffer(MotionLayerState *mlStates, ChunkTransportState *transportStates,
			ChunkTransportStateEntry *pEntry, MotionConn *conn, int16 motionId)
{
	char	   *sendptr;
	MotionNodeEntry *pMNEntry;
	int			n,
				sent = 0;
	mpp_fd_set	wset;
	mpp_fd_set	rset;

#ifdef AMS_VERBOSE_LOGGING
	{
		struct timeval snapTime;

		gettimeofday(&snapTime, NULL);
		elog(DEBUG5, "----sending chunk @%s.%d time is %d.%d",
			 __FILE__, __LINE__, (int) snapTime.tv_sec, (int) snapTime.tv_usec);
	}
#endif

	pMNEntry = getMotionNodeEntry(mlStates, motionId, "flushBuffer");

	/* first set header length */
	*(uint32 *) conn->pBuff = conn->msgSize;

	/* now send message */
	sendptr = (char *) conn->pBuff;
	sent = 0;
	do
	{
		struct timeval timeout;

		/* check for stop message before sending anything  */
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		MPP_FD_ZERO(&rset);
		MPP_FD_SET(conn->sockfd, &rset);

		/*
		 * since timeout = 0, select returns imediately and no time is wasted
		 * waiting trying to send data on the network
		 */
		n = select(conn->sockfd + 1, (fd_set *) &rset, NULL, NULL, &timeout);
		/* handle errors at the write call, below */
		if (n > 0 && MPP_FD_ISSET(conn->sockfd, &rset))
		{
#ifdef AMS_VERBOSE_LOGGING
			print_connection(transportStates, conn->sockfd, "stop from");
#endif
			/* got a stop message */
			conn->stillActive = false;
			return false;
		}

		if ((n = send(conn->sockfd, sendptr + sent, conn->msgSize - sent, 0)) < 0)
		{
			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
			if (errno == EINTR)
				continue;
			if (errno == EWOULDBLOCK)
			{
				do
				{
					timeout = tval;

					ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

					MPP_FD_ZERO(&rset);
					MPP_FD_ZERO(&wset);
					MPP_FD_SET(conn->sockfd, &wset);
					MPP_FD_SET(conn->sockfd, &rset);
					n = select(conn->sockfd + 1, (fd_set *) &rset, (fd_set *) &wset, NULL, &timeout);
					pMNEntry->sel_wr_wait += (tval.tv_sec - timeout.tv_sec) * 1000000 + (tval.tv_usec - timeout.tv_usec);
					if (n < 0)
					{
						if (errno == EINTR)
							continue;

						/*
						 * if we got an error in teardown, ignore it: treat it
						 * as a stop message
						 */
						if (transportStates->teardownActive)
						{
#ifdef AMS_VERBOSE_LOGGING
							print_connection(transportStates, conn->sockfd, "stop from");
#endif
							conn->stillActive = false;
							return false;
						}
						ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
										errmsg("Interconnect error writing an outgoing packet: %m"),
										errdetail("error during select() call (error:%d).\n"
												  "For Remote Connection: contentId=%d at %s",
												  errno, conn->remoteContentId,
												  conn->remoteHostAndPort)));
					}

					/*
					 * as a sender... if there is something to read... it must
					 * mean its a StopSendingMessage.  we don't even bother to
					 * read it.
					 */
					if (MPP_FD_ISSET(conn->sockfd, &rset) || transportStates->teardownActive)
					{
#ifdef AMS_VERBOSE_LOGGING
						print_connection(transportStates, conn->sockfd, "stop from");
#endif
						conn->stillActive = false;
						return false;
					}
				} while (n < 1);
			}
			else
			{
				/*
				 * if we got an error in teardown, ignore it: treat it as a
				 * stop message
				 */
				if (transportStates->teardownActive)
				{
#ifdef AMS_VERBOSE_LOGGING
					print_connection(transportStates, conn->sockfd, "stop from");
#endif
					conn->stillActive = false;
					return false;
				}
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Interconnect error writing an outgoing packet"),
								errdetail("error during send() call (error:%d).\n"
										  "For Remote Connection: contentId=%d at %s",
										  errno, conn->remoteContentId,
										  conn->remoteHostAndPort)));
			}
		}
		else
		{
			sent += n;
		}
	} while (sent < conn->msgSize);

	conn->tupleCount = 0;
	conn->msgSize = PACKET_HEADER_SIZE;

	return true;
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
SendChunkTCP(MotionLayerState *mlStates, ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn, TupleChunkListItem tcItem, int16 motionId)
{
	int			length = TYPEALIGN(TUPLE_CHUNK_ALIGN, tcItem->chunk_length);

	Assert(conn->msgSize > 0);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "sendChunk: msgSize %d this chunk length %d", conn->msgSize, tcItem->chunk_length);
#endif

	if (conn->msgSize + length > Gp_max_packet_size)
	{
		if (!flushBuffer(mlStates, transportStates, pEntry, conn, motionId))
			return false;
	}

	memcpy(conn->pBuff + conn->msgSize, tcItem->chunk_data, tcItem->chunk_length);
	conn->msgSize += length;

	conn->tupleCount++;
	return true;
}
