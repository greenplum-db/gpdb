/*-------------------------------------------------------------------------
 *
 * ftsprobe.c
 *	  Implementation of segment probing interface
 *
 * Portions Copyright (c) 2006-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/ftsprobe.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include <pthread.h>
#include <limits.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "cdb/cdbgang.h"		/* gp_pthread_create */
#include "libpq/ip.h"
#include "postmaster/fts.h"

#include "executor/spi.h"
#include "postmaster/primary_mirror_mode.h"

#include "cdb/ml_ipc.h" /* gettime_elapsed_ms */

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifdef USE_TEST_UTILS
#include "utils/simex.h"
#endif /* USE_TEST_UTILS */

/*
 * CONSTANTS
 */

#ifdef USE_SEGWALREP
#define PROBE_RESPONSE_LEN  (4)         /* size of segment response message */
#else
#define PROBE_RESPONSE_LEN  (20)         /* size of segment response message */
#endif
#define PROBE_ERR_MSG_LEN   (256)        /* length of error message for errno */

/*
 * MACROS
 */

#define SYS_ERR_TRANSIENT(errno) (errno == EINTR || errno == EAGAIN)


/*
 * STRUCTURES
 */

typedef struct threadWorkerInfo
{
	uint8 *scan_status;
} threadWorkerInfo;


typedef struct ProbeConnectionInfo
{
	int16 dbId;                          /* the dbid of the segment */
	int16 segmentId;                     /* content indicator: -1 for master, 0, ..., n-1 for segments */
	char role;                           /* primary ('p'), mirror ('m') */
	char mode;                           /* sync ('s'), resync ('r'), change-tracking ('c') */

	int	fd;                              /* socket file descriptor */
	struct sockaddr_storage  saddr;      /* socket descriptor */
	struct addrinfo *addrs;              /* socket address info */
	struct addrinfo hint;                /* socket address info hint */
	int saddr_len;
	const char *hostIp;                  /* segment ip */
	int port;                            /* segment postmaster port */

	GpMonotonicTime startTime;           /* probe start timestamp */
	char response[PROBE_RESPONSE_LEN];   /* buffer to store segment response to probe */

	bool isSocketOpened;                 /* flag indicating if socket is opened */
	bool isConnected;                    /* flag indicating if connection has been established */

	char segmentStatus;                  /* probed segment status */

	int16 probe_errno;                   /* saved errno from the latest system call */
	char errmsg[PROBE_ERR_MSG_LEN];      /* message returned by strerror() */
} ProbeConnectionInfo;

typedef struct ProbeMsg
{
	uint32 packetlen;
	PrimaryMirrorTransitionPacket payload;
} ProbeMsg;


/*
 * STATIC VARIABLES
 */

/* mutex used for pthread synchronization in parallel probing */
static pthread_mutex_t worker_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

/* struct holding segment configuration */
static CdbComponentDatabases *cdb_component_dbs = NULL;

/* one byte of status for each segment */
static uint8 *scan_status;

/*
 * FUNCTION PROTOTYPES
 */

static void *probeSegmentFromThread(void *cdb_component_dbs);

static char probeSegment(CdbComponentDatabaseInfo *dbInfo);

static bool probeMarkSocketNonBlocking(ProbeConnectionInfo *probeInfo);
static bool probeGetIpAddr(ProbeConnectionInfo *probeInfo);
static bool probeOpenSocket(ProbeConnectionInfo *probeInfo);
static bool probeConnect(ProbeConnectionInfo *probeInfo);
static bool probePoll(ProbeConnectionInfo *probeInfo, bool incoming);
static bool probeSend(ProbeConnectionInfo *probeInfo);
static bool probeReceive(ProbeConnectionInfo *probeInfo);
static bool probeProcessResponse(ProbeConnectionInfo *probeInfo);
static bool probeTimeout(ProbeConnectionInfo *probeInfo, const char* calledFrom);
static void probeClose(ProbeConnectionInfo *probeInfo);


/*
 * strerror() is not threadsafe.  Therefore, prober threads must use
 * the correct strerror_r() available on the platform to obtain error
 * message corresponding to errno.
 */
static char *errmessage(ProbeConnectionInfo *probeInfo)
{
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	/* POSIX compliant version (seen on OSX 10.9) */
	int ret = strerror_r(probeInfo->probe_errno,
						 probeInfo->errmsg, PROBE_ERR_MSG_LEN);
	if (ret != 0)
		probeInfo->errmsg[0] = '\0';
#else
	/* GNU version (seen on RHEL 6.2) */
	char *msg = strerror_r(probeInfo->probe_errno,
						   probeInfo->errmsg, PROBE_ERR_MSG_LEN);
	if (msg == NULL)
		probeInfo->errmsg[0] = '\0';
	else
	{
		strncpy(probeInfo->errmsg, msg, PROBE_ERR_MSG_LEN-1);
		/* In case strncpy doesn't null-terminate errmsg */
		probeInfo->errmsg[PROBE_ERR_MSG_LEN-1] = '\0';
	}
#endif
	return probeInfo->errmsg;
}

/*
 * probe segments to check if they are alive and if any failure has occurred
 */
void
FtsProbeSegments(CdbComponentDatabases *dbs, uint8 *probeRes)
{
	int i;

	threadWorkerInfo worker_info;
	int workers = gp_fts_probe_threadcount;
	pthread_t *threads = NULL;

	cdb_component_dbs = dbs;
	scan_status = probeRes;
	if (cdb_component_dbs == NULL || scan_status == NULL)
	{
		elog(ERROR, "FTS: segment configuration has not been loaded to shared memory");
	}

	/* reset probe results */
	memset(scan_status, 0, cdb_component_dbs->total_segment_dbs * sizeof(scan_status[0]));

	/* figure out which segments to include in the scan. */
	for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];

		if (FtsIsSegmentAlive(segInfo))
		{
			/* mark segment for probing */
			scan_status[segInfo->dbid] = PROBE_SEGMENT;
		}
		else
		{
			/* consider segment dead */
			scan_status[segInfo->dbid] = PROBE_DEAD;
		}
	}

	worker_info.scan_status = scan_status;

	threads = (pthread_t *)palloc(workers * sizeof(pthread_t));
	for (i = 0; i < workers; i++)
	{
#ifdef USE_ASSERT_CHECKING
		int ret =
#endif /* USE_ASSERT_CHECKING */
		gp_pthread_create(&threads[i], probeSegmentFromThread, &worker_info, "probeSegments");

		Assert(ret == 0 && "FTS: failed to create probing thread");
	}

	/* we have nothing left to do but wait */
	for (i = 0; i < workers; i++)
	{
#ifdef USE_ASSERT_CHECKING
		int ret =
#endif /* USE_ASSERT_CHECKING */
		pthread_join(threads[i], NULL);

		Assert(ret == 0 && "FTS: failed to join probing thread");
	}

	pfree(threads);
	threads = NULL;

	/* if we're shutting down, just exit. */
	if (!FtsIsActive())
		return;

	if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "FTS: probe results for all segments:");
		for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
		{
			CdbComponentDatabaseInfo *segInfo = NULL;

			segInfo = &cdb_component_dbs->segment_db_info[i];

			elog(LOG, "segment dbid %d status 0x%x.", segInfo->dbid, scan_status[segInfo->dbid]);
		}
	}
}


/*
 * This is called from several different threads: ONLY USE THREADSAFE FUNCTIONS INSIDE.
 */
static char
probeSegment(CdbComponentDatabaseInfo *dbInfo)
{
	Assert(dbInfo != NULL);

	/* setup probe descriptor */
	ProbeConnectionInfo probeInfo;
	memset(&probeInfo, 0, sizeof(ProbeConnectionInfo));
	probeInfo.segmentId = dbInfo->segindex;
	probeInfo.dbId = dbInfo->dbid;
	probeInfo.role = dbInfo->role;
	probeInfo.mode = dbInfo->mode;
	probeInfo.hostIp = dbInfo->hostip;
	probeInfo.port = dbInfo->port;
	probeInfo.segmentStatus = PROBE_DEAD;

	/* set probe start timestamp */
	gp_set_monotonic_begin_time(&probeInfo.startTime);
	int retryCnt = 1;

	/*
	 * probe segment: open socket -> connect -> send probe msg -> receive response;
	 * on any error the connection is shut down, the socket is closed
	 * and the probe process is restarted;
	 * this is repeated until no error occurs (response is received) or timeout expires;
	 */
	while (!probeGetIpAddr(&probeInfo) ||
	       !probeOpenSocket(&probeInfo) ||
	       !probeMarkSocketNonBlocking(&probeInfo) ||
	       !probeConnect(&probeInfo) ||
	       !probeSend(&probeInfo) ||
	       !probeReceive(&probeInfo) ||
	       !probeProcessResponse(&probeInfo))
	{
		probeClose(&probeInfo);

		Assert(probeInfo.segmentStatus == PROBE_DEAD);

		/* check if FTS is active */
		if (!FtsIsActive())
		{
			/* the returned value will be ignored */
			probeInfo.segmentStatus = PROBE_ALIVE;
			break;
		}

		/*
		 * if maximum number of retries was reached,
		 * report segment as non-responsive (dead)
		 */
		if (retryCnt == gp_fts_probe_retries)
		{
			write_log("FTS: failed to probe segment (content=%d, dbid=%d) after trying %d time(s), "
					  "maximum number of retries reached.",
					  probeInfo.segmentId,
					  probeInfo.dbId,
					  retryCnt);
			break;
		}

		/* sleep for 1 second to avoid tight loops */
		pg_usleep(USECS_PER_SEC);
		retryCnt++;

		write_log("FTS: retry %d to probe segment (content=%d, dbid=%d).",
				  retryCnt - 1, probeInfo.segmentId, probeInfo.dbId);

		/* reset timer */
		gp_set_monotonic_begin_time(&probeInfo.startTime);
	}

	/* segment response to probe was received, close connection  */
	probeClose(&probeInfo);

	return probeInfo.segmentStatus;
}


/*
 * Mark socket as non-blocking
 */
static bool
probeMarkSocketNonBlocking(ProbeConnectionInfo *probeInfo)
{
	Assert(probeInfo != NULL);
	Assert(probeInfo->isSocketOpened);
	Assert(!probeInfo->isConnected);
	Assert(probeInfo->fd > 0);

	/* mark socket as non-blocking */
	if (!pg_set_noblock(probeInfo->fd))
	{
		write_log("FTS: failed to mark socket as non-blocking to connect to segment (content=%d, dbid=%d).",
		          probeInfo->segmentId, probeInfo->dbId);
		return false;
	}
	return true;
}


/*
 * Get segment IP address and port.
 */
static bool
probeGetIpAddr(ProbeConnectionInfo *probeInfo)
{

	Assert(probeInfo != NULL);
	Assert(!probeInfo->isSocketOpened);
	Assert(!probeInfo->isConnected);

	MemSet(&probeInfo->saddr, 0, sizeof(probeInfo->saddr));
	probeInfo->addrs = NULL;

	/*
	 * Get an sockaddr that has the right address and port in it.
	 * We get passed in the IP address (IPv4 or IPv6), not the name, so
	 * we don't need to worry about name resolution.
	 */

	/* initialize hint structure */
	MemSet(&probeInfo->hint, 0, sizeof(probeInfo->hint));
	probeInfo->hint.ai_socktype = SOCK_STREAM; /* TCP */
	probeInfo->hint.ai_family = AF_UNSPEC;	/* Allow for any family */
#ifdef AI_NUMERICSERV
	probeInfo->hint.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;  /* Never do name resolution */
#else
	probeInfo->hint.ai_flags = AI_NUMERICHOST;  /* Never do name resolution */
#endif

	char portNumberStr[32];
	snprintf(portNumberStr, sizeof(portNumberStr), "%d", probeInfo->port);

	int ret = pg_getaddrinfo_all(probeInfo->hostIp, portNumberStr, &probeInfo->hint, &probeInfo->addrs);
	if (ret != 0 || probeInfo->addrs == NULL)
	{
		if (probeInfo->addrs != NULL)
		{
			pg_freeaddrinfo_all(probeInfo->hint.ai_family, probeInfo->addrs);
		}

		write_log("FTS: failed to get IP address to connect to segment (content=%d, dbid=%d) \"%s\", port \"%d\" to address: %s.",
						probeInfo->segmentId, probeInfo->dbId,
						probeInfo->hostIp, probeInfo->port, gai_strerror(ret));
		return false;
	}

	Assert(probeInfo->addrs->ai_next == NULL);

	/* since we aren't using name resolution, we retrieve exactly one address */
	memcpy(&probeInfo->saddr, probeInfo->addrs->ai_addr, probeInfo->addrs->ai_addrlen);
	probeInfo->saddr_len = probeInfo->addrs->ai_addrlen;

	return true;
}


/*
 * Open socket
 */
static bool
probeOpenSocket(ProbeConnectionInfo *probeInfo)
{
	Assert(probeInfo != NULL);
	Assert(probeInfo->addrs != NULL);
	Assert(!probeInfo->isSocketOpened);
	Assert(!probeInfo->isConnected);
	Assert(probeInfo->fd == 0);
	probeInfo->isSocketOpened = true;
	struct addrinfo *addrs = probeInfo->addrs;

	/* open socket */
	if ((probeInfo->fd = gp_socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol)) < 0)
	{
		probeInfo->probe_errno = errno;
        write_log("FTS: failed to open socket to connect to segment "
                  "(content=%d, dbid=%d), errno=%d (%s)",
                  probeInfo->segmentId, probeInfo->dbId, probeInfo->probe_errno,
                  errmessage(probeInfo));
		probeInfo->isSocketOpened = false;
		probeInfo->fd = 0;
	}

	pg_freeaddrinfo_all(probeInfo->hint.ai_family, addrs);

	return probeInfo->isSocketOpened;
}


/*
 * Establish connection to segment
 */
static bool
probeConnect(ProbeConnectionInfo *probeInfo)
{
	Assert(probeInfo != NULL);
	Assert(probeInfo->isSocketOpened);
	Assert(!probeInfo->isConnected);
	Assert(probeInfo->fd > 0);

	int res = 0;
	probeInfo->isConnected = true;

	while ((res = gp_connect(probeInfo->fd, (struct sockaddr *) &probeInfo->saddr, probeInfo->saddr_len)) == -1)
	{
		/* check for transient error */
		if (SYS_ERR_TRANSIENT(errno) && !probeTimeout(probeInfo, "connect") && FtsIsActive())
		{
			continue;
		}

		/*
		 * check for EINPROGRESS error;
		 * socket is non-blocking, connection may not be established immediately
		 */
		if (errno != EINPROGRESS)
		{
			probeInfo->probe_errno = errno;
			write_log("FTS: failed to connect to segment (content=%d, dbid=%d),"
					  " errno %d (%s).", probeInfo->segmentId, probeInfo->dbId,
					  probeInfo->probe_errno, errmessage(probeInfo));
			probeInfo->isConnected = false;
		}

		break;
	}

	/* check for errors */
	return probeInfo->isConnected;
}


/*
 * Poll connection to check if it is ready for sending
 */
static bool
probePoll(ProbeConnectionInfo *probeInfo, bool incoming)
{
	Assert(probeInfo != NULL);
	Assert(probeInfo->isSocketOpened);
	Assert(probeInfo->isConnected);
	Assert(probeInfo->fd > 0);

	int	timeoutMs = 1000;
	int event = (incoming ? POLLIN : POLLOUT);
	struct pollfd nfd;
	nfd.fd = probeInfo->fd;
	nfd.events = event;

	int res = 0;

	while (FtsIsActive() &&
	       (res = gp_poll(&nfd, 1, timeoutMs)) <= 0 &&
		   !probeTimeout(probeInfo, "poll"))
	{
		/* check for transient error */
		if (res != 0 && !SYS_ERR_TRANSIENT(errno))
		{
			probeInfo->probe_errno = errno;
			const char *operation = (incoming ? "receiving from" : "sending to");
			write_log("FTS: failed to poll connection before %s segment "
					  "(content=%d, dbid=%d), errno %d (%s).", operation,
					  probeInfo->segmentId, probeInfo->dbId,
					  probeInfo->probe_errno, errmessage(probeInfo));
			break;
		}
	}

	/* if we success, one file descriptor (socket) is ready for sending/receiving */
	Assert (res <= 1);
	return (res == 1 && ((nfd.events & event) == event));
}


/*
 * Send the status request-startup-packet
 */
static bool
probeSend(ProbeConnectionInfo *probeInfo)
{
	Assert(probeInfo != NULL);
	Assert(probeInfo->isSocketOpened);
	Assert(probeInfo->isConnected);
	Assert(probeInfo->fd > 0);

	uint32 bytesSent = 0;

	/* prepare message to send to segment */
	ProbeMsg msg;
	msg.packetlen = htonl((uint32) sizeof(msg));
	msg.payload.protocolCode = (MsgType) htonl(PRIMARY_MIRROR_TRANSITION_QUERY_CODE);
	msg.payload.dataLength = 0;

	while (bytesSent < sizeof(msg) &&
	       !probeTimeout(probeInfo, "send") &&
	       probePoll(probeInfo, false /*incoming*/))
	{
		int res = gp_send(probeInfo->fd, ((char*) &msg) + bytesSent, sizeof(msg) - bytesSent, 0);
		if (res < 0)
		{
			/* check for transient error */
			if (!SYS_ERR_TRANSIENT(errno))
			{
				probeInfo->probe_errno = errno;
				write_log("FTS: failed to send request to segment "
						  "(dbid=%d, content=%d), errno %d (%s).",
						  probeInfo->dbId, probeInfo->segmentId,
						  probeInfo->probe_errno, errmessage(probeInfo));
				break;
			}
		}
		else
		{
			bytesSent += res;
		}
	}

	Assert(bytesSent <= sizeof(msg));
	return (bytesSent == sizeof(msg));
}


/*
 * Receive segment response
 */
static bool
probeReceive(ProbeConnectionInfo *probeInfo)
{
	Assert(probeInfo != NULL);
	Assert(probeInfo->isSocketOpened);
	Assert(probeInfo->isConnected);
	Assert(probeInfo->fd > 0);

	uint32 bytesReceived = 0;

	while (bytesReceived < sizeof(probeInfo->response) &&
	       !probeTimeout(probeInfo, "receive") &&
	       probePoll(probeInfo, true /*incoming*/))
	{
		int res = gp_recv
				(
				probeInfo->fd,
				((char*) &probeInfo->response) + bytesReceived,
				sizeof(probeInfo->response) - bytesReceived,
				0
				)
				;

		if (gp_log_fts > GPVARS_VERBOSITY_VERBOSE)
		{
			write_log("FTS: read %d offset %d remainder %ld from segment (dbid=%d, content=%d).",
			           res, bytesReceived, (long)(sizeof(probeInfo->response) - bytesReceived), probeInfo->dbId, probeInfo->segmentId);
		}

		if (res == 0)
		{
			write_log("FTS: failed to receive data, segment (dbid=%d, content=%d) closed connection unexpectedly.",
			          probeInfo->dbId, probeInfo->segmentId);

			break;
		}

		if (res < 0)
		{
			/* check for transient error */
			if (!SYS_ERR_TRANSIENT(errno))
			{
				probeInfo->probe_errno = errno;
				write_log("FTS: failed to receive response from segment "
						  "(dbid=%d, content=%d), errno %d (%s).",
			              probeInfo->dbId, probeInfo->segmentId,
						  probeInfo->probe_errno, errmessage(probeInfo));
				break;
			}
		}

		if (res > 0)
		{
			bytesReceived += res;
		}
	}

	Assert(bytesReceived <= sizeof(probeInfo->response));
	return (bytesReceived == sizeof(probeInfo->response));
}


/*
 * Close connection and socket
 */
static void
probeClose(ProbeConnectionInfo *probeInfo)
{
	/* close connection */
	if (probeInfo->isConnected)
	{
		Assert(probeInfo->isSocketOpened);
		Assert(probeInfo->fd > 0);

		(void) shutdown(probeInfo->fd, SHUT_RDWR);
	}

	/* close socket */
	if (probeInfo->isSocketOpened)
	{
		(void) close(probeInfo->fd);
	}

	probeInfo->isConnected = false;
	probeInfo->isSocketOpened = false;
	probeInfo->fd = 0;
}


/*
 * Check if probe timeout has expired
 */
static bool
probeTimeout(ProbeConnectionInfo *probeInfo, const char* calledFrom)
{
	uint64 elapsed_ms = gp_get_elapsed_ms(&probeInfo->startTime);
	const uint64 debug_elapsed_ms = 5000;

	if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE && elapsed_ms > debug_elapsed_ms)
	{
		write_log("FTS: probe '%s' elapsed time: " UINT64_FORMAT " ms for segment (dbid=%d, content=%d).",
		          calledFrom, elapsed_ms, probeInfo->dbId, probeInfo->segmentId);
	}

	/* If connection takes more than the gp_fts_probe_timeout, we fail. */
	if (elapsed_ms > (uint64)gp_fts_probe_timeout * 1000)
	{
		write_log("FTS: failed to probe segment (content=%d, dbid=%d) due to timeout expiration, "
				  "probe elapsed time: " UINT64_FORMAT " ms.",
				  probeInfo->segmentId,
				  probeInfo->dbId,
				  elapsed_ms);

		return true;
	}

	return false;
}


/*
 * Process segment response
 */
static bool
probeProcessResponse(ProbeConnectionInfo *probeInfo)
{
	Assert(probeInfo->segmentStatus == PROBE_DEAD);

#ifndef USE_SEGWALREP
	PrimaryMirrorMode role;
	SegmentState_e state;
	DataState_e mode;
	FaultType_e fault;
#endif
	uint32 bufInt;

	memcpy(&bufInt, probeInfo->response, 4);
	if (ntohl(bufInt) != PROBE_RESPONSE_LEN)
	{
		write_log("FTS: probe protocol violation got length %d.", ntohl(bufInt));
		return false;
	}

#ifdef USE_SEGWALREP
	/*
	 * This is where the message sent by the primary through
	 * sendPrimaryMirrorTransitionQuery is processed. If the message is
	 * changed, this processing should be updated to reflect the change along
	 * with the PROBE_RESPONSE_LEN.
	 */
	probeInfo->segmentStatus = PROBE_ALIVE;

	write_log("FTS: segment (dbid=%d, content=%d) reported segmentstatus %x to the prober.",
			  probeInfo->dbId, probeInfo->segmentId, probeInfo->segmentStatus);
#else
	memcpy(&bufInt, probeInfo->response + 4, sizeof(bufInt));
	role = (PrimaryMirrorMode)ntohl(bufInt);

	memcpy(&bufInt, probeInfo->response + 8, sizeof(bufInt));
	state = (SegmentState_e)ntohl(bufInt);

	memcpy(&bufInt, probeInfo->response + 12, sizeof(bufInt));
	mode = (DataState_e)ntohl(bufInt);

	memcpy(&bufInt, probeInfo->response + 16, sizeof(bufInt));
	fault = (FaultType_e)ntohl(bufInt);

	if (gp_log_fts > GPVARS_VERBOSITY_VERBOSE)
	{
		write_log("FTS: probe result for dbid=%d, content=%d: %s %s %s %s.",
				  probeInfo->dbId, probeInfo->segmentId,
				  getMirrorModeLabel(role),
				  getSegmentStateLabel(state),
				  getDataStateLabel(mode),
				  getFaultTypeLabel(fault));
	}

	/* segment responded to probe, mark it as alive */
	probeInfo->segmentStatus = PROBE_ALIVE;

	/* check if segment has completed re-synchronizing */
	if (mode == DataStateInSync && probeInfo->mode == 'r')
	{
		probeInfo->segmentStatus |= PROBE_RESYNC_COMPLETE;
	}
	else
	{
		/* check for inconsistent segment state */
		char probeRole = getRole(role);
		char probeMode = getMode(mode);
		if ((probeRole != probeInfo->role || probeMode != probeInfo->mode))
		{
			write_log("FTS: segment (dbid=%d, content=%d) has not reached new state yet, "
			          "expected state: ('%c','%c'), "
			          "reported state: ('%c','%c').",
					  probeInfo->dbId,
					  probeInfo->segmentId,
					  probeInfo->role,
					  probeInfo->mode,
					  probeRole,
					  probeMode);
		}
	}

	/* check if segment reported a fault */
	if (state == SegmentStateFault)
	{
		/* get fault type - this will be used to decide the next segment state */
		switch(fault)
		{
			case FaultTypeNet:
				probeInfo->segmentStatus |= PROBE_FAULT_NET;
				break;

			case FaultTypeMirror:
				probeInfo->segmentStatus |= PROBE_FAULT_MIRROR;
				break;

			case FaultTypeDB:
			case FaultTypeIO:
				probeInfo->segmentStatus |= PROBE_FAULT_CRASH;
				break;

			default:
				Assert(!"Unexpected segment fault type");
		}
		write_log("FTS: segment (dbid=%d, content=%d) reported fault %s segmentstatus %x to the prober.",
				  probeInfo->dbId, probeInfo->segmentId, getFaultTypeLabel(fault), probeInfo->segmentStatus);
	}
#endif

	return true;
}


/*
 * Function called by probing thread;
 * iteratively picks next segment pair to probe until all segments are probed;
 * probes primary; probes mirror if primary reports crash fault or does not respond
 */
static void *
probeSegmentFromThread(void *arg)
{
	threadWorkerInfo *worker_info;
	int i;

	worker_info = (threadWorkerInfo *) arg;

	i = 0;

	for (;;)
	{
		CdbComponentDatabaseInfo *primary = NULL;
		CdbComponentDatabaseInfo *mirror = NULL;

		char probe_result_primary = PROBE_DEAD;
		char probe_result_mirror = PROBE_DEAD;

		/*
		 * find untested primary, mark primary and mirror "tested" and unlock.
		 */
		pthread_mutex_lock(&worker_thread_mutex);
		for (; i < cdb_component_dbs->total_segment_dbs; i++)
		{
			primary = &cdb_component_dbs->segment_db_info[i];

			/* check segments in pairs of primary-mirror */
			if (!SEGMENT_IS_ACTIVE_PRIMARY(primary))
			{
				continue;
			}

			if (PROBE_CHECK_FLAG(worker_info->scan_status[primary->dbid], PROBE_SEGMENT))
			{
				/* prevent re-checking this pair */
				worker_info->scan_status[primary->dbid] &= ~PROBE_SEGMENT;

				mirror = FtsGetPeerSegment(primary->segindex, primary->dbid);

				/* check if mirror is marked for probing */
				if (mirror != NULL &&
					PROBE_CHECK_FLAG(worker_info->scan_status[mirror->dbid], PROBE_SEGMENT))
				{
					worker_info->scan_status[mirror->dbid] &= ~PROBE_SEGMENT;
				}
				else
				{
					mirror = NULL;
				}

				break;
			}
		}
		pthread_mutex_unlock(&worker_thread_mutex);

		/* check if all segments were probed */
		if (i == cdb_component_dbs->total_segment_dbs || primary == NULL)
		{
			break;
		}

		/* if we've gotten a pause or shutdown request, we ignore probe results. */
		if (!FtsIsActive())
		{
			break;
		}

		/* probe primary */
		probe_result_primary = probeSegment(primary);
		Assert(!PROBE_CHECK_FLAG(probe_result_primary, PROBE_SEGMENT));

		if ((probe_result_primary & PROBE_ALIVE) == 0 && gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		{
			write_log("FTS: primary (dbid=%d, content=%d, status 0x%x) didn't respond to probe.",
			          primary->dbid, primary->segindex, probe_result_primary);
		}

		if (mirror != NULL)
		{
			/* assume mirror is alive */
			probe_result_mirror = PROBE_ALIVE;

			/* probe mirror only if primary is dead or has a crash/network fault */
			if (!PROBE_CHECK_FLAG(probe_result_primary, PROBE_ALIVE) ||
				PROBE_CHECK_FLAG(probe_result_primary, PROBE_FAULT_CRASH) ||
				PROBE_CHECK_FLAG(probe_result_primary, PROBE_FAULT_NET))
			{
				/* probe mirror */
				probe_result_mirror = probeSegment(mirror);
				Assert(!PROBE_CHECK_FLAG(probe_result_mirror, PROBE_SEGMENT));

				if ((probe_result_mirror & PROBE_ALIVE) == 0 && gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
				{
					write_log("FTS: mirror (dbid=%d, content=%d, status 0x%x) didn't respond to probe.",
					          mirror->dbid, mirror->segindex, probe_result_mirror);
				}
			}
		}

		/* update results */
		pthread_mutex_lock(&worker_thread_mutex);
		worker_info->scan_status[primary->dbid] = probe_result_primary;
		if (mirror != NULL)
		{
			worker_info->scan_status[mirror->dbid] = probe_result_mirror;
		}
		pthread_mutex_unlock(&worker_thread_mutex);
	}

	return NULL;
}


/* EOF */
