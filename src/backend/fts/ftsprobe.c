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
#include "libpq-fe.h"
#include "libpq-int.h"

#include "cdb/cdbgang.h"		/* gp_pthread_create */
#include "libpq/ip.h"
#include "postmaster/fts.h"
#include "postmaster/ftsprobe.h"

#include "executor/spi.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

/* mutex used for pthread synchronization in parallel probing */
static pthread_mutex_t worker_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Establish async libpq connection to a segment
 */
static bool
ftsConnect(CdbComponentDatabaseInfo *dbInfo,
		   FtsConnectionInfo *ftsInfo)
{
	char conninfo[1024];

	snprintf(conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 dbInfo->hostip, dbInfo->port, GPCONN_TYPE_FTS);
	ftsInfo->conn = PQconnectdb(conninfo);

	if (ftsInfo->conn == NULL)
	{
		write_log("FTS: cannot create libpq connection object, possibly out of memory"
				  " (content=%d, dbid=%d)", ftsInfo->segmentId, ftsInfo->dbId);
		return false;
	}
	if (ftsInfo->conn->status == CONNECTION_BAD)
	{
		write_log("FTS: cannot establish libpq connection to (content=%d, dbid=%d): %s",
				  ftsInfo->segmentId, ftsInfo->dbId,
				  ftsInfo->conn->errorMessage.data);
		return false;
	}
	return true;
}

/*
 * Send FTS query
 */
static bool
ftsSend(FtsConnectionInfo *ftsInfo)
{
	if (!PQsendQuery(ftsInfo->conn, ftsInfo->message))
	{
		write_log("FTS: failed to send query '%s' to (content=%d, dbid=%d): "
				  "connection status %d, %s",
				  FTS_MSG_PROBE, ftsInfo->segmentId, ftsInfo->dbId,
				  ftsInfo->conn->status, ftsInfo->conn->errorMessage.data);
		return false;
	}
	return true;
}

/*
 * Record FTS handler's response from libpq result into probe_result
 */
static void
probeRecordResponse(FtsConnectionInfo *ftsInfo, PGresult *result)
{
	ftsInfo->result->isPrimaryAlive = true;

	int *isMirrorAlive = (int *) PQgetvalue(result, 0,
											Anum_fts_message_response_is_mirror_up);
	Assert (isMirrorAlive);
	ftsInfo->result->isMirrorAlive = *isMirrorAlive;

	int *isInSync = (int *) PQgetvalue(result, 0,
									   Anum_fts_message_response_is_in_sync);
	Assert (isInSync);
	ftsInfo->result->isInSync = *isInSync;

	int *isSyncRepEnabled = (int *) PQgetvalue(result, 0,
											   Anum_fts_message_response_is_syncrep_enabled);
	Assert (isSyncRepEnabled);
	ftsInfo->result->isSyncRepEnabled = *isSyncRepEnabled;

	int *isRoleMirror = (int *) PQgetvalue(result, 0,
										   Anum_fts_message_response_is_role_mirror);
	Assert (isRoleMirror);
	ftsInfo->result->isRoleMirror = *isRoleMirror;

	int *retryRequested = (int *) PQgetvalue(result, 0,
											 Anum_fts_message_response_request_retry);
	Assert (retryRequested);
	ftsInfo->result->retryRequested = *retryRequested;

	write_log("FTS: segment (content=%d, dbid=%d, role=%c) reported isMirrorUp %d, isInSync %d, isSyncRepEnabled %d, isRoleMirror %d, and retryRequested %d to the prober.",
			  ftsInfo->segmentId, ftsInfo->dbId, ftsInfo->role,
			  ftsInfo->result->isMirrorAlive,
			  ftsInfo->result->isInSync,
			  ftsInfo->result->isSyncRepEnabled,
			  ftsInfo->result->isRoleMirror,
			  ftsInfo->result->retryRequested);
}

/*
 * Receive segment response
 */
static bool
ftsReceive(FtsConnectionInfo *ftsInfo)
{
	/* last non-null result that will be returned */
	PGresult   *lastResult = NULL;

	/*
	 * Could directly use PQexec() here instead of loop and have very simple
	 * version, only hurdle is PQexec() (and PQgetResult()) has infinite wait
	 * coded in pqWait() and hence doesn't allow for checking the timeout
	 * (SLA) for FTS probe.
	 *
	 * It would have been really easy if pqWait() could have been taken
	 * timeout as connection object property instead of having -1 hard-coded,
	 * that way could have easily used the PQexec() interface.
	 */
	for (;;)
	{
		PGresult   *tmpResult = NULL;
		/*
		 * Receive data until PQgetResult is ready to get the result without
		 * blocking.
		 */
		while (PQisBusy(ftsInfo->conn))
		{
			if (!probePollIn(ftsInfo))
			{
				return false;	/* either timeout or error happened */
			}

			if (PQconsumeInput(ftsInfo->conn) == 0)
			{
				write_log("FTS: error reading probe response from "
						  "libpq socket (content=%d, dbid=%d): %s",
						  ftsInfo->segmentId, ftsInfo->dbId,
						  PQerrorMessage(ftsInfo->conn));
				return false;	/* trouble */
			}
		}

		/*
		 * Emulate the PQexec()'s behavior of returning the last result when
		 * there are many. Since walsender will never generate multiple
		 * results, we skip the concatenation of error messages.
		 */
		tmpResult = PQgetResult(ftsInfo->conn);
		if (tmpResult == NULL)
			break;				/* response received */

		PQclear(lastResult);
		lastResult = tmpResult;

		if (PQstatus(ftsInfo->conn) == CONNECTION_BAD)
		{
			write_log("FTS: error reading probe response from "
					  "libpq socket (content=%d, dbid=%d): %s",
					  ftsInfo->segmentId, ftsInfo->dbId,
					  PQerrorMessage(ftsInfo->conn));
			return false;	/* trouble */
		}
	}

	if (PQresultStatus(lastResult) != PGRES_TUPLES_OK)
	{
		PQclear(lastResult);
		write_log("FTS: error reading probe response from "
				  "libpq socket (content=%d, dbid=%d): %s",
				  ftsInfo->segmentId, ftsInfo->dbId,
				  PQerrorMessage(ftsInfo->conn));
		return false;
	}

	if (PQnfields(lastResult) != Natts_fts_message_response ||
		PQntuples(lastResult) != FTS_MESSAGE_RESPONSE_NTUPLES)
	{
		int			ntuples = PQntuples(lastResult);
		int			nfields = PQnfields(lastResult);

		PQclear(lastResult);
		write_log("FTS: invalid probe response from (content=%d, dbid=%d):"
				  "Expected %d tuple with %d field, got %d tuples with %d fields",
				  ftsInfo->segmentId, ftsInfo->dbId, FTS_MESSAGE_RESPONSE_NTUPLES,
				  Natts_fts_message_response, ntuples, nfields);
		return false;
	}

	/*
	 * FTS_MSG_SYNCREP_OFF response only needs an ack for now. In future
	 * iterations, we could parse that response to detect the case when mirror
	 * has come back up in-sync when previously thought not in-sync from probe
	 * response. In that situation, we should force another probe to update
	 * the gp_segment_configuration to avoid waiting the fts probe interval.
	 */
	if (strcmp(ftsInfo->message, FTS_MSG_PROBE) == 0)
	{
		probeRecordResponse(ftsInfo, lastResult);
		if (ftsInfo->result->retryRequested)
		{
			write_log("FTS: primary asked to retry the probe for (content=%d, dbid=%d)",
					  ftsInfo->segmentId, ftsInfo->dbId);
			return false;
		}
	}
	/* Primary must have syncrep disabled in response to SYNCREP_OFF message. */
	AssertImply(strcmp(ftsInfo->message, FTS_MSG_SYNCREP_OFF) == 0,
				PQgetvalue(lastResult, 0, Anum_fts_message_response_is_syncrep_enabled) != NULL &&
				*(PQgetvalue(lastResult, 0, Anum_fts_message_response_is_syncrep_enabled)) == false);

	return true;
}

static void
ftsSegmentHelper(CdbComponentDatabaseInfo *dbInfo,
				 FtsConnectionInfo *ftsInfo)
{
	/*
	 * probe segment: open socket -> connect -> send probe msg -> receive response;
	 * on any error the connection is shut down, the socket is closed
	 * and the probe process is restarted;
	 * this is repeated until no error occurs (response is received) or timeout expires;
	 */

	int retryCnt = 0;
	/* reset timer */
	gp_set_monotonic_begin_time(&ftsInfo->startTime);
	while (retryCnt < gp_fts_probe_retries && FtsIsActive() && (
			   !ftsConnect(dbInfo, ftsInfo) ||
			   !ftsSend(ftsInfo) ||
			   !ftsReceive(ftsInfo)))
	{
		PQfinish(ftsInfo->conn);
		ftsInfo->conn = NULL;

		/* sleep for 1 second to avoid tight loops */
		pg_usleep(USECS_PER_SEC);
		retryCnt++;

		write_log("FTS: retry %d to probe segment (content=%d, dbid=%d).",
				  retryCnt - 1, ftsInfo->segmentId, ftsInfo->dbId);

		/* reset timer */
		gp_set_monotonic_begin_time(&ftsInfo->startTime);
	}

	if (retryCnt == gp_fts_probe_retries)
	{
		write_log("FTS: failed to probe segment (content=%d, dbid=%d) after trying %d time(s), "
				  "maximum number of retries reached.",
				  ftsInfo->segmentId,
				  ftsInfo->dbId,
				  retryCnt);
	}

	if (ftsInfo->conn)
	{
		PQfinish(ftsInfo->conn);
		ftsInfo->conn = NULL;
	}
}

static void
messageWalRepSegment(probe_response_per_segment *response)
{
	Assert(response);
	CdbComponentDatabaseInfo *segment_db_info = response->segment_db_info;
	Assert(segment_db_info);

	/* setup probe descriptor */
	FtsConnectionInfo ftsInfo;
	memset(&ftsInfo, 0, sizeof(FtsConnectionInfo));
	ftsInfo.segmentId = segment_db_info->segindex;
	ftsInfo.dbId = segment_db_info->dbid;
	ftsInfo.role = segment_db_info->role;
	ftsInfo.mode = segment_db_info->mode;
	ftsInfo.result = &(response->result);
	ftsInfo.message = response->message;

	ftsSegmentHelper(segment_db_info, &ftsInfo);
}

static void *
messageWalRepSegmentFromThread(void *arg)
{
	fts_context *context = (fts_context *)arg;

	Assert(context);

	int number_of_probed_segments = 0;

	while(number_of_probed_segments < context->num_primary_segments)
	{
		/*
		 * Look for the unprocessed context
		 */
		int response_index = number_of_probed_segments;

		pthread_mutex_lock(&worker_thread_mutex);
		while(response_index < context->num_primary_segments)
		{
			if (!context->responses[response_index].isScheduled)
			{
				context->responses[response_index].isScheduled = true;
				break;
			}
			number_of_probed_segments++;
			response_index++;
		}
		pthread_mutex_unlock(&worker_thread_mutex);

		/*
		 * If probed all the segments, we are done.
		 */
		if (response_index == context->num_primary_segments)
			break;

		/*
		 * If FTS is shutdown, abort.
		 */
		if (!FtsIsActive())
			break;

		/* now let's probe the primary. */
		probe_response_per_segment *response = &context->responses[response_index];
		AssertImply(strcmp(response->message, FTS_MSG_PROMOTE) != 0,
					SEGMENT_IS_ACTIVE_PRIMARY(response->segment_db_info));
		messageWalRepSegment(response);
	}

	return NULL;
}

void
FtsWalRepMessageSegments(fts_context *context)
{
	int workers = gp_fts_probe_threadcount;
	pthread_t *threads = NULL;
	Assert(context);

	threads = (pthread_t *)palloc(workers * sizeof(pthread_t));

	for(int i = 0; i < workers; i++)
	{
		int ret = gp_pthread_create(&threads[i], messageWalRepSegmentFromThread, context, "FtsWalRepMessageSegments");

		if (ret)
		{
			ereport(ERROR, (errmsg("FTS: failed to create probing thread")));
		}
	}

	for(int i = 0; i < workers; i++)
	{
		int ret = pthread_join(threads[i], NULL);

		if (ret)
		{
			ereport(ERROR, (errmsg("FTS: failed to join probing thread")));
		}
	}
}

/*
 * Check if probe timeout has expired
 */
bool
probeTimeout(FtsConnectionInfo *ftsInfo, const char* calledFrom)
{
	uint64 elapsed_ms = gp_get_elapsed_ms(&ftsInfo->startTime);
	const uint64 debug_elapsed_ms = 5000;

	if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE && elapsed_ms > debug_elapsed_ms)
	{
		write_log("FTS: probe '%s' elapsed time: " UINT64_FORMAT " ms for segment (content=%d, dbid=%d).",
		          calledFrom, elapsed_ms, ftsInfo->segmentId, ftsInfo->dbId);
	}

	/* If connection takes more than the gp_fts_probe_timeout, we fail. */
	if (elapsed_ms > (uint64)gp_fts_probe_timeout * 1000)
	{
		write_log("FTS: failed to probe segment (content=%d, dbid=%d) due to timeout expiration, "
				  "probe elapsed time: " UINT64_FORMAT " ms.",
				  ftsInfo->segmentId,
				  ftsInfo->dbId,
				  elapsed_ms);

		return true;
	}

	return false;
}

/*
 * Wait for a socket to become available for reading.
 */
bool
probePollIn(FtsConnectionInfo *ftsInfo)
{
	struct pollfd nfd;
	nfd.fd = PQsocket(ftsInfo->conn);
	nfd.events = POLLIN;
	int ret;

	do
	{
		ret = poll(&nfd, 1, 1000);

		if (ret > 0)
		{
			return true;
		}

		if (ret == -1 && errno != EAGAIN && errno != EINTR)
		{
			ftsInfo->probe_errno = errno;
			write_log("FTS: pollin error on libpq socket (content=%d, dbid=%d): %s",
					  ftsInfo->segmentId, ftsInfo->dbId, errmessage(ftsInfo));

			return false;
		}
	} while (!probeTimeout(ftsInfo, "probePollIn"));

	write_log("FTS: pollin timeout waiting for libpq socket to become "
					  "available (content=%d, dbid=%d)", ftsInfo->segmentId,
			  ftsInfo->dbId);

	return false;
}

/*
 * strerror() is not threadsafe.  Therefore, prober threads must use
 * the correct strerror_r() available on the platform to obtain error
 * message corresponding to errno.
 */
char *errmessage(FtsConnectionInfo *ftsInfo)
{
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	/* POSIX compliant version (seen on OSX 10.9) */
	int ret = strerror_r(ftsInfo->probe_errno,
						 ftsInfo->errmsg, PROBE_ERR_MSG_LEN);
	if (ret != 0)
		ftsInfo->errmsg[0] = '\0';
#else
	/* GNU version (seen on RHEL 6.2) */
	char *msg = strerror_r(ftsInfo->probe_errno,
						   ftsInfo->errmsg, PROBE_ERR_MSG_LEN);
	if (msg == NULL)
		ftsInfo->errmsg[0] = '\0';
	else
	{
		strncpy(ftsInfo->errmsg, msg, PROBE_ERR_MSG_LEN-1);
		/* In case strncpy doesn't null-terminate errmsg */
		ftsInfo->errmsg[PROBE_ERR_MSG_LEN-1] = '\0';
	}
#endif
	return ftsInfo->errmsg;
}


/* EOF */
