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

static struct pollfd *PollFds;

void
initPollFds(int size)
{
	PollFds = (struct pollfd *) palloc0(size * sizeof(struct pollfd));
}

static bool ftsConnectStart(per_segment_info *ftsInfo);

static FtsProbeState
nextSuccessState(FtsProbeState state)
{
	switch(state)
	{
		case FTS_PROBE_SEGMENT:
			return FTS_PROBE_SUCCESS;
		case FTS_SYNCREP_SEGMENT:
			return FTS_SYNCREP_SUCCESS;
		case FTS_PROMOTE_SEGMENT:
			return FTS_PROMOTE_SUCCESS;
		default:
			elog(ERROR, "cannot determine next success state for %d", state);
	}
}

FtsProbeState
nextFailedState(FtsProbeState state)
{
	switch(state)
	{
		case FTS_PROBE_SEGMENT:
			return FTS_PROBE_FAILED;
		case FTS_SYNCREP_SEGMENT:
			return FTS_SYNCREP_FAILED;
		case FTS_PROMOTE_SEGMENT:
			return FTS_PROMOTE_FAILED;
		case FTS_PROBE_FAILED:
			return FTS_PROBE_FAILED;
		case FTS_SYNCREP_FAILED:
			return FTS_SYNCREP_FAILED;
		case FTS_PROMOTE_FAILED:
			return FTS_PROMOTE_FAILED;
		case FTS_PROBE_RETRY_WAIT:
			return FTS_PROBE_FAILED;
		case FTS_SYNCREP_RETRY_WAIT:
			return FTS_SYNCREP_FAILED;
		case FTS_PROMOTE_RETRY_WAIT:
			return FTS_PROMOTE_FAILED;
		default:
			elog(ERROR, "cannot determine next failed state for %d", state);
	}
}

static bool
allDone(fts_context *context)
{
	bool result = true;
	int i;
	for (i = 0; i < context->num_pairs && result; i++)
	{
		/*
		 * If any segment is not in a final state, return false.
		 */
		result &= (context->perSegInfos[i].state == FTS_PROBE_SUCCESS ||
				   context->perSegInfos[i].state == FTS_SYNCREP_SUCCESS ||
				   context->perSegInfos[i].state == FTS_PROMOTE_SUCCESS ||
				   context->perSegInfos[i].state == FTS_PROBE_FAILED ||
				   context->perSegInfos[i].state == FTS_SYNCREP_FAILED ||
				   context->perSegInfos[i].state == FTS_PROMOTE_FAILED);
	}
	return result;
}

/*
 * Start a libpq connection for each "per segment" object in context.  If the
 * connection is already started for an object, advance libpq state machine for
 * that object by calling PQconnectPoll().  An established libpq connection
 * (authentication complete and ready-for-query received) is identified by: (1)
 * state of the "per segment" object is any of FTS_PROBE_SEGMENT,
 * FTS_SYNCREP_SEGMENT, FTS_PROMOTE_SEGMENT and (2) PQconnectPoll() returns
 * PGRES_POLLING_OK for the connection.
 *
 * Upon failure, transition that object to a failed state.
 */
void
ftsConnect(fts_context *context)
{
	int i;
	for (i = 0; i < context->num_pairs; i++)
	{
		per_segment_info *ftsInfo = &context->perSegInfos[i];
		elog(DEBUG1, "FTS: ftsConnect (content=%d, dbid=%d) state=%d, "
			 "retry_count=%d, conn->status=%d",
			 ftsInfo->primary_cdbinfo->segindex,
			 ftsInfo->primary_cdbinfo->dbid,
			 ftsInfo->state, ftsInfo->retry_count,
			 ftsInfo->conn ? ftsInfo->conn->status : -1);
		if (ftsInfo->conn && PQstatus(ftsInfo->conn) == CONNECTION_OK)
			continue;
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SEGMENT:
			case FTS_SYNCREP_SEGMENT:
			case FTS_PROMOTE_SEGMENT:
				if (ftsInfo->conn == NULL)
				{
					AssertImply(ftsInfo->retry_count > 0,
								ftsInfo->retry_count <= gp_fts_probe_retries);
					if (!ftsConnectStart(ftsInfo))
						ftsInfo->state = nextFailedState(ftsInfo->state);
				}
				else if (ftsInfo->poll_revents & (POLLOUT | POLLIN))
				{
					switch(PQconnectPoll(ftsInfo->conn))
					{
						case PGRES_POLLING_OK:
							/*
							 * Response-state is already set and now the
							 * connection is also ready with authentication
							 * completed.  Next step should now be able to send
							 * the appropriate FTS message.
							 */
							elog(LOG, "FTS: established libpq connection "
								 "(content=%d, dbid=%d) state=%d, "
								 "retry_count=%d, conn->status=%d",
								 ftsInfo->primary_cdbinfo->segindex,
								 ftsInfo->primary_cdbinfo->dbid,
								 ftsInfo->state, ftsInfo->retry_count,
								 ftsInfo->conn->status);
							ftsInfo->poll_events = POLLOUT;
							break;

						case PGRES_POLLING_READING:
							/*
							 * The connection can now be polled for reading and
							 * if the poll() returns POLLIN in revents, data
							 * has arrived.
							 */
							ftsInfo->poll_events |= POLLIN;
							break;

						case PGRES_POLLING_WRITING:
							/*
							 * The connection can now be polled for writing and
							 * may be written to, if ready.
							 */
							ftsInfo->poll_events |= POLLOUT;
							break;

						case PGRES_POLLING_FAILED:
							ftsInfo->state = nextFailedState(ftsInfo->state);
							elog(LOG, "FTS: cannot establish libpq connection "
								 "(content=%d, dbid=%d): %s, retry_count=%d",
								 ftsInfo->primary_cdbinfo->segindex,
								 ftsInfo->primary_cdbinfo->dbid,
								 PQerrorMessage(ftsInfo->conn),
								 ftsInfo->retry_count);
							break;

						default:
							elog(ERROR, "FTS: invalid response to PQconnectPoll"
								 " (content=%d, dbid=%d): %s",
								 ftsInfo->primary_cdbinfo->segindex,
								 ftsInfo->primary_cdbinfo->dbid,
								 PQerrorMessage(ftsInfo->conn));
							break;
					}
				}
				else
					elog(DEBUG1, "FTS: ftsConnect (content=%d, dbid=%d) state=%d,"
						 " retry_count=%d, conn->status=%d pollfd.revents unset",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 ftsInfo->conn ? ftsInfo->conn->status : -1);
				break;
			case FTS_PROBE_SUCCESS:
			case FTS_SYNCREP_SUCCESS:
			case FTS_PROMOTE_SUCCESS:
			case FTS_PROBE_FAILED:
			case FTS_SYNCREP_FAILED:
			case FTS_PROMOTE_FAILED:
			case FTS_PROBE_RETRY_WAIT:
			case FTS_SYNCREP_RETRY_WAIT:
			case FTS_PROMOTE_RETRY_WAIT:
				break;
		}
	}
}

/*
 * Establish async libpq connection to a segment
 */
static bool
ftsConnectStart(per_segment_info *ftsInfo)
{
	char conninfo[1024];

	Assert(ftsInfo->poll_revents == 0);
	snprintf(conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 ftsInfo->primary_cdbinfo->hostip, ftsInfo->primary_cdbinfo->port,
			 GPCONN_TYPE_FTS);
	ftsInfo->conn = PQconnectStart(conninfo);

	if (ftsInfo->conn == NULL)
	{
		elog(ERROR, "FTS: cannot create libpq connection object, possibly out"
			 " of memory (content=%d, dbid=%d)",
			 ftsInfo->primary_cdbinfo->segindex,
			 ftsInfo->primary_cdbinfo->dbid);
	}
	if (ftsInfo->conn->status == CONNECTION_BAD)
	{
		elog(LOG, "FTS: cannot establish libpq connection to "
			 "(content=%d, dbid=%d): %s",
			 ftsInfo->primary_cdbinfo->segindex, ftsInfo->primary_cdbinfo->dbid,
			 PQerrorMessage(ftsInfo->conn));
		return false;
	}

	/*
	 * Connection started, we must wait until the socket becomes ready for
	 * writing before anything can be written on this socket.  Therefore, mark
	 * the connection to be considered for subsequent poll step.
	 */
	ftsInfo->poll_events |= POLLOUT;
	/*
	 * Start the timer.
	 */
	ftsInfo->startTime = (pg_time_t) time(NULL);

	return true;
}

static void
ftsPoll(fts_context *context)
{
	int i;
	int nfds=0;
	int nready;
	for (i = 0; i < context->num_pairs; i++)
	{
		per_segment_info *ftsInfo = &context->perSegInfos[i];
		if (ftsInfo->poll_events & (POLLIN|POLLOUT))
		{
			PollFds[nfds].fd = PQsocket(ftsInfo->conn);
			PollFds[nfds].events = ftsInfo->poll_events;
			PollFds[nfds].revents = 0;
			ftsInfo->fd_index = nfds;
			nfds++;
		}
		else
			ftsInfo->fd_index = -1; /*
									 * This socket is not considered for
									 * polling.
									 */
	}
	if (nfds == 0)
		return;

	nready = poll(PollFds, nfds, 50);
	if (nready < 0)
	{
		if (errno == EINTR)
			elog(DEBUG1, "FTS: ftsPoll() interrupted, nfds %d", nfds);
		else
			elog(ERROR, "FTS: ftsPoll() failed: nfds %d, %m", nfds);
	}
	else if (nready == 0)
	{
		elog(DEBUG1, "FTS: ftsPoll() timed out, nfds %d", nfds);
		return;
	}

	elog(DEBUG1, "FTS: ftsPoll() found %d out of %d sockets ready",
		 nready, nfds);

	/* Record poll() response poll_revents for each "per_segment" object. */
	for (i = 0; i < context->num_pairs; i++)
	{
		per_segment_info *ftsInfo = &context->perSegInfos[i];
		if (ftsInfo->poll_events & (POLLIN|POLLOUT))
		{
			Assert(PollFds[ftsInfo->fd_index].fd == PQsocket(ftsInfo->conn));
			ftsInfo->poll_revents = PollFds[ftsInfo->fd_index].revents;
			/*
			 * Reset poll_events for fds that were found ready.  Assume
			 * that at the most one bit is set in poll_events (POLLIN
			 * or POLLOUT).
			 */
			if (ftsInfo->poll_revents & ftsInfo->poll_events)
				ftsInfo->poll_events = 0;
			else if (ftsInfo->poll_revents & (POLLHUP | POLLERR))
			{
				ftsInfo->state = nextFailedState(ftsInfo->state);
				elog(LOG,
					 "FTS poll failed (revents=%d, events=%d) for "
					 "(content=%d, dbid=%d) state=%d, retry_count=%d, "
					 "libpq status=%d, asyncStatus=%d",
					 ftsInfo->poll_revents, ftsInfo->poll_events,
					 ftsInfo->primary_cdbinfo->segindex,
					 ftsInfo->primary_cdbinfo->dbid, ftsInfo->state,
					 ftsInfo->retry_count, ftsInfo->conn->status,
					 ftsInfo->conn->asyncStatus);
			}
			else if (ftsInfo->poll_revents)
			{
				ftsInfo->state = nextFailedState(ftsInfo->state);
				elog(LOG,
					 "FTS unexpected events (revents=%d, events=%d) for "
					 "(content=%d, dbid=%d) state=%d, retry_count=%d, "
					 "libpq status=%d, asyncStatus=%d",
					 ftsInfo->poll_revents, ftsInfo->poll_events,
					 ftsInfo->primary_cdbinfo->segindex,
					 ftsInfo->primary_cdbinfo->dbid, ftsInfo->state,
					 ftsInfo->retry_count, ftsInfo->conn->status,
					 ftsInfo->conn->asyncStatus);
			}
		}
		else
			ftsInfo->poll_revents = 0;
	}
}

/*
 * Send FTS query
 */
static void
ftsSend(fts_context *context)
{
	per_segment_info *ftsInfo;
	const char *message;
	int i;

	for (i = 0; i < context->num_pairs; i++)
	{
		ftsInfo = &context->perSegInfos[i];
		elog(DEBUG1, "FTS: ftsSend (content=%d, dbid=%d) state=%d, "
			 "retry_count=%d, conn->asyncStatus=%d",
			 ftsInfo->primary_cdbinfo->segindex,
			 ftsInfo->primary_cdbinfo->dbid,
			 ftsInfo->state, ftsInfo->retry_count,
			 ftsInfo->conn ? ftsInfo->conn->asyncStatus : -1);
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SEGMENT:
			case FTS_SYNCREP_SEGMENT:
			case FTS_PROMOTE_SEGMENT:
				/*
				 * The libpq connection must be ready for accepting query and
				 * the socket must be writable.
				 */
				if (PQstatus(ftsInfo->conn) != CONNECTION_OK ||
					ftsInfo->conn->asyncStatus != PGASYNC_IDLE ||
				    !(ftsInfo->poll_revents & POLLOUT))
					break;
				if (ftsInfo->state == FTS_PROBE_SEGMENT)
					message = FTS_MSG_PROBE;
				else if (ftsInfo->state == FTS_SYNCREP_SEGMENT)
					message = FTS_MSG_SYNCREP_OFF;
				else
					message = FTS_MSG_PROMOTE;
				if (PQsendQuery(ftsInfo->conn, message))
				{
					/*
					 * Message sent successfully, mark the socket to be polled
					 * for reading so we will be ready to read response when it
					 * arrives.
					 */
					ftsInfo->poll_events = POLLIN;
					elog(LOG, "FTS sent %s to (content=%d, dbid=%d), retry_count=%d",
						 message, ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid, ftsInfo->retry_count);
				}
				else
				{
					elog(LOG,
						 "FTS: failed to send %s to segment (content=%d, "
						 "dbid=%d) state=%d, retry_count=%d, "
						 "conn->asyncStatus=%d %s", message,
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 ftsInfo->conn->asyncStatus,
						 PQerrorMessage(ftsInfo->conn));
					ftsInfo->state = nextFailedState(ftsInfo->state);
				}
				break;
			default:
				/* Cannot send messages in any other state. */
				break;
		}
	}
}

/*
 * Record FTS handler's response from libpq result into probe_result
 */
static void
probeRecordResponse(per_segment_info *ftsInfo, PGresult *result)
{
	ftsInfo->result.isPrimaryAlive = true;

	int *isMirrorAlive = (int *) PQgetvalue(result, 0,
											Anum_fts_message_response_is_mirror_up);
	Assert (isMirrorAlive);
	ftsInfo->result.isMirrorAlive = *isMirrorAlive;

	int *isInSync = (int *) PQgetvalue(result, 0,
									   Anum_fts_message_response_is_in_sync);
	Assert (isInSync);
	ftsInfo->result.isInSync = *isInSync;

	int *isSyncRepEnabled = (int *) PQgetvalue(result, 0,
											   Anum_fts_message_response_is_syncrep_enabled);
	Assert (isSyncRepEnabled);
	ftsInfo->result.isSyncRepEnabled = *isSyncRepEnabled;

	int *isRoleMirror = (int *) PQgetvalue(result, 0,
										   Anum_fts_message_response_is_role_mirror);
	Assert (isRoleMirror);
	ftsInfo->result.isRoleMirror = *isRoleMirror;

	int *retryRequested = (int *) PQgetvalue(result, 0,
											 Anum_fts_message_response_request_retry);
	Assert (retryRequested);
	ftsInfo->result.retryRequested = *retryRequested;

	elog(LOG, "FTS: segment (content=%d, dbid=%d, role=%c) reported "
		 "isMirrorUp %d, isInSync %d, isSyncRepEnabled %d, "
		 "isRoleMirror %d, and retryRequested %d to the prober.",
		 ftsInfo->primary_cdbinfo->segindex,
		 ftsInfo->primary_cdbinfo->dbid,
		 ftsInfo->primary_cdbinfo->role,
		 ftsInfo->result.isMirrorAlive,
		 ftsInfo->result.isInSync,
		 ftsInfo->result.isSyncRepEnabled,
		 ftsInfo->result.isRoleMirror,
		 ftsInfo->result.retryRequested);
}

/*
 * Receive segment response
 */
static void
ftsReceive(fts_context *context)
{
	per_segment_info *ftsInfo;
	PGresult *result;
	int ntuples;
	int nfields;
	int i;

	for (i = 0; i < context->num_pairs; i++)
	{
		ftsInfo = &context->perSegInfos[i];
		elog(DEBUG1, "FTS: ftsReceive (content=%d, dbid=%d) state=%d, "
			 "retry_count=%d, conn->asyncStatus=%d",
			 ftsInfo->primary_cdbinfo->segindex,
			 ftsInfo->primary_cdbinfo->dbid,
			 ftsInfo->state, ftsInfo->retry_count,
			 ftsInfo->conn ? ftsInfo->conn->asyncStatus : -1);
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SEGMENT:
			case FTS_SYNCREP_SEGMENT:
			case FTS_PROMOTE_SEGMENT:
				/*
				 * The libpq connection must be established and a message must
				 * have arrived on the socket.
				 */
				if (PQstatus(ftsInfo->conn) != CONNECTION_OK ||
					!(ftsInfo->poll_revents & POLLIN))
					break;
				/* Read the ftsInfo that has arrived. */
				if (!PQconsumeInput(ftsInfo->conn))
				{
					elog(LOG, "FTS: failed to read from (content=%d, dbid=%d)"
						 " state=%d, retry_count=%d, conn->asyncStatus=%d %s",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 ftsInfo->conn->asyncStatus,
						 PQerrorMessage(ftsInfo->conn));
					ftsInfo->state = nextFailedState(ftsInfo->state);
					break;
				}
				/* Parse the response. */
				if (PQisBusy(ftsInfo->conn))
				{
					elog(LOG, "FTS: error parsing response from (content=%d, "
						 "dbid=%d) state=%d, retry_count=%d, "
						 "conn->asyncStatus=%d %s",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 ftsInfo->conn->asyncStatus,
						 PQerrorMessage(ftsInfo->conn));
					ftsInfo->state = nextFailedState(ftsInfo->state);
					break;
				}

				/*
				 * Response parsed, PQgetResult() should not block for I/O now.
				 */
				result = PQgetResult(ftsInfo->conn);

				if (!result || PQstatus(ftsInfo->conn) == CONNECTION_BAD)
				{
					elog(LOG, "FTS: error getting results from (content=%d, "
						 "dbid=%d) state=%d, retry_count=%d, "
						 "conn->asyncStatus=%d conn->status=%d %s",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 ftsInfo->conn->asyncStatus,
						 ftsInfo->conn->status,
						 PQerrorMessage(ftsInfo->conn));
					ftsInfo->state = nextFailedState(ftsInfo->state);
					break;
				}

				if (PQresultStatus(result) != PGRES_TUPLES_OK)
				{
					elog(LOG, "FTS: error response from (content=%d, dbid=%d)"
						 " state=%d, retry_count=%d, conn->asyncStatus=%d %s",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 ftsInfo->conn->asyncStatus,
						 PQresultErrorMessage(result));
					ftsInfo->state = nextFailedState(ftsInfo->state);
					break;
				}
				ntuples = PQntuples(result);
				nfields = PQnfields(result);
				if (nfields != Natts_fts_message_response ||
					ntuples != FTS_MESSAGE_RESPONSE_NTUPLES)
				{
					/*
					 * XXX: Investigate: including conn->asyncStatus generated
					 * a format string warning at compile time.
					 */
					elog(LOG, "FTS: invalid response from (content=%d, dbid=%d)"
						 " state=%d, retry_count=%d, expected %d tuple with "
						 "%d fields, got %d tuples with %d fields",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid,
						 ftsInfo->state, ftsInfo->retry_count,
						 FTS_MESSAGE_RESPONSE_NTUPLES,
						 Natts_fts_message_response, ntuples, nfields);
					ftsInfo->state = nextFailedState(ftsInfo->state);
					break;
				}
				/*
				 * Result received and parsed successfully.  Record it so that
				 * subsequent step processes it and transitions to next state.
				 */
				probeRecordResponse(ftsInfo, result);
				ftsInfo->state = nextSuccessState(ftsInfo->state);

				/*
				 * Reference to the result should already be stored in
				 * connection object. If it is not then free explicitly.
				 */
				if (result != ftsInfo->conn->result)
					PQclear(result);
				break;
			default:
				break;
		}
	}
}

/*
 * If retry attempts are available, transition the sgement to the start state
 * corresponding to their failure state.  If retries have exhausted, leave the
 * segment in the failure state.
 */
static void
processRetry(fts_context *context)
{
	per_segment_info *ftsInfo;
	int i;
	pg_time_t now = (pg_time_t) time(NULL);

	for (i = 0; i < context->num_pairs; i++)
	{
		ftsInfo = &context->perSegInfos[i];
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SUCCESS:
				/*
				 * Purpose of retryRequested flag is to avoid considering
				 * mirror as down prematurely.  If mirror is already marked
				 * down in configuration, there is no need to retry.
				 */
				if (!(ftsInfo->result.retryRequested &&
					  SEGMENT_IS_ALIVE(ftsInfo->mirror_cdbinfo)))
					break;
			case FTS_PROBE_FAILED:
			case FTS_SYNCREP_FAILED:
			case FTS_PROMOTE_FAILED:
				if (ftsInfo->retry_count == gp_fts_probe_retries)
				{
					elog(LOG, "FTS max (%d) retries exhausted "
						 "(content=%d, dbid=%d) state=%d",
						 ftsInfo->retry_count,
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid, ftsInfo->state);
				}
				else
				{
					ftsInfo->retry_count++;
					if (ftsInfo->state == FTS_PROBE_SUCCESS ||
						ftsInfo->state == FTS_PROBE_FAILED)
						ftsInfo->state = FTS_PROBE_RETRY_WAIT;
					else if (ftsInfo->state == FTS_SYNCREP_FAILED)
						ftsInfo->state = FTS_SYNCREP_RETRY_WAIT;
					else
						ftsInfo->state = FTS_PROMOTE_RETRY_WAIT;
					ftsInfo->retryStartTime = now;
					elog(LOG, "FTS initialized retry start time to now "
						 "(content=%d, dbid=%d) state=%d",
						 ftsInfo->primary_cdbinfo->segindex,
						 ftsInfo->primary_cdbinfo->dbid, ftsInfo->state);
					PQfinish(ftsInfo->conn);
					ftsInfo->conn = NULL;
					ftsInfo->poll_events = ftsInfo->poll_revents = 0;
					/* Reset result before next attempt. */
					memset(&ftsInfo->result, 0, sizeof(probe_result));
				}
				break;
			case FTS_PROBE_RETRY_WAIT:
			case FTS_SYNCREP_RETRY_WAIT:
			case FTS_PROMOTE_RETRY_WAIT:
				/* Wait for 1 second before making another attempt. */
				if ((int) (now - ftsInfo->retryStartTime) < 1)
					break;
				/*
				 * We have remained in retry state for over a second, it's time
				 * to make another attempt.
				 */
				elog(LOG, "FTS retrying attempt %d (content=%d, dbid=%d) "
					 "state=%d", ftsInfo->retry_count,
					 ftsInfo->primary_cdbinfo->segindex,
					 ftsInfo->primary_cdbinfo->dbid, ftsInfo->state);
				if (ftsInfo->state == FTS_PROBE_RETRY_WAIT)
					ftsInfo->state = FTS_PROBE_SEGMENT;
				else if (ftsInfo->state == FTS_SYNCREP_RETRY_WAIT)
					ftsInfo->state = FTS_SYNCREP_SEGMENT;
				else
					ftsInfo->state = FTS_PROMOTE_SEGMENT;
				break;
			default:
				break;
		}
	}
}

bool
FtsWalRepMessageSegments(fts_context *context)
{
	bool is_updated = false;
	initPollFds(context->num_pairs);

	while (!allDone(context) && FtsIsActive())
	{
		ftsConnect(context);
		ftsPoll(context);
		ftsSend(context);
		ftsReceive(context);
		processRetry(context);
		is_updated |= probeWalRepPublishUpdate(context);
	}
#ifdef USE_ASSERT_CHECKING
	int i;
	for (i = 0; i < context->num_pairs; i++)
	{
		insist_log((context->perSegInfos[i].conn == NULL),
				   "FTS: libpq connection not closed (content=%d, dbid=%d)"
				   " state=%d, retry_count=%d, conn->status=%d",
				   context->perSegInfos[i].primary_cdbinfo->segindex,
				   context->perSegInfos[i].primary_cdbinfo->dbid,
				   context->perSegInfos[i].state,
				   context->perSegInfos[i].retry_count,
				   context->perSegInfos[i].conn->status);
	}
#endif
	return is_updated;
}

/* EOF */
