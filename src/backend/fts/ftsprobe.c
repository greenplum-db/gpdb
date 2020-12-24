/*-------------------------------------------------------------------------
 *
 * ftsprobe.c
 *	  Implementation of segment probing interface
 *
 * Portions Copyright (c) 2006-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/ftsprobe.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "libpq-fe.h"
#include "libpq-int.h"
#include "access/xact.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbvars.h"
#include "postmaster/fts.h"
#include "postmaster/ftsprobe.h"
#include "postmaster/postmaster.h"
#include "utils/snapmgr.h"


static struct pollfd *PollFds;

static CdbComponentDatabaseInfo *
FtsGetPeerSegment(CdbComponentDatabases *cdbs,
				  int content, int dbid)
{
	int i;

	for (i=0; i < cdbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdbs->segment_db_info[i];

		if (segInfo->config->segindex == content && segInfo->config->dbid != dbid)
		{
			/* found it */
			return segInfo;
		}
	}

	return NULL;
}

static FtsMessageState
nextSuccessState(FtsMessageState state)
{
	FtsMessageState result = FTS_PROBE_FAILED; /* to shut up compiler */
	switch(state)
	{
		case FTS_PROBE_SEGMENT:
			result = FTS_PROBE_SUCCESS;
			break;
		case FTS_SYNCREP_OFF_SEGMENT:
			result = FTS_SYNCREP_OFF_SUCCESS;
			break;
		case FTS_PROMOTE_SEGMENT:
			result = FTS_PROMOTE_SUCCESS;
			break;
		case FTS_PROBE_RETRY_WAIT:
		case FTS_SYNCREP_OFF_RETRY_WAIT:
		case FTS_PROMOTE_RETRY_WAIT:
		case FTS_PROBE_SUCCESS:
		case FTS_SYNCREP_OFF_SUCCESS:
		case FTS_PROMOTE_SUCCESS:
		case FTS_PROBE_FAILED:
		case FTS_SYNCREP_OFF_FAILED:
		case FTS_PROMOTE_FAILED:
		case FTS_RESPONSE_PROCESSED:
			elog(ERROR, "cannot determine next success state for %d", state);
	}
	return result;
}

static FtsMessageState
nextFailedState(FtsMessageState state)
{
	FtsMessageState result = FTS_PROBE_FAILED; /* to shut up compiler */
	switch(state)
	{
		case FTS_PROBE_SEGMENT:
			result = FTS_PROBE_FAILED;
			break;
		case FTS_SYNCREP_OFF_SEGMENT:
			result = FTS_SYNCREP_OFF_FAILED;
			break;
		case FTS_PROMOTE_SEGMENT:
			result = FTS_PROMOTE_FAILED;
			break;
		case FTS_PROBE_FAILED:
			result = FTS_PROBE_FAILED;
			break;
		case FTS_SYNCREP_OFF_FAILED:
			result = FTS_SYNCREP_OFF_FAILED;
			break;
		case FTS_PROMOTE_FAILED:
			result = FTS_PROMOTE_FAILED;
			break;
		case FTS_PROBE_RETRY_WAIT:
			result = FTS_PROBE_FAILED;
			break;
		case FTS_SYNCREP_OFF_RETRY_WAIT:
			result = FTS_SYNCREP_OFF_FAILED;
			break;
		case FTS_PROMOTE_RETRY_WAIT:
			result = FTS_PROMOTE_FAILED;
			break;
		case FTS_PROBE_SUCCESS:
		case FTS_SYNCREP_OFF_SUCCESS:
		case FTS_PROMOTE_SUCCESS:
		case FTS_RESPONSE_PROCESSED:
			elog(ERROR, "cannot determine next failed state for %d", state);
	}
	return result;
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
		result = (context->perSegInfos[i].state == FTS_RESPONSE_PROCESSED);
	}
	return result;
}

/*
 * Establish async libpq connection to a segment
 */
static bool
ftsConnectStart(fts_segment_info *ftsInfo)
{
	char conninfo[1024];
	char *hostip;

	/*
	 * No events should be pending on the connection that hasn't started
	 * yet.
	 */
	Assert(ftsInfo->poll_revents == 0);
	/*
	 * The segment acting as primary should be the one to receive PROBE or
	 * SYNCREP_OFF messages.
	 */
	AssertImply(ftsInfo->state == FTS_PROBE_SEGMENT ||
				ftsInfo->state == FTS_SYNCREP_OFF_SEGMENT,
				SEGMENT_IS_ACTIVE_PRIMARY(ftsInfo->primary_cdbinfo));

	hostip = ftsInfo->primary_cdbinfo->config->hostip;
	snprintf(conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 hostip ? hostip : "", ftsInfo->primary_cdbinfo->config->port,
			 GPCONN_TYPE_FTS);
	ftsInfo->conn = PQconnectStart(conninfo);

	if (ftsInfo->conn == NULL)
	{
		elog(ERROR, "FTS: cannot create libpq connection object, possibly out"
			 " of memory (content=%d, dbid=%d)",
			 ftsInfo->primary_cdbinfo->config->segindex,
			 ftsInfo->primary_cdbinfo->config->dbid);
	}
	if (ftsInfo->conn->status == CONNECTION_BAD)
	{
		elog(LOG, "FTS: cannot establish libpq connection to "
			 "(content=%d, dbid=%d): %s",
			 ftsInfo->primary_cdbinfo->config->segindex, ftsInfo->primary_cdbinfo->config->dbid,
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
checkIfFailedDueToRecoveryInProgress(fts_segment_info *ftsInfo)
{
	if (strstr(PQerrorMessage(ftsInfo->conn), _(POSTMASTER_IN_RECOVERY_MSG)) ||
		strstr(PQerrorMessage(ftsInfo->conn), _(POSTMASTER_IN_STARTUP_MSG)))
	{
		XLogRecPtr tmpptr;
		char *ptr = strstr(PQerrorMessage(ftsInfo->conn),
				_(POSTMASTER_IN_RECOVERY_DETAIL_MSG));
		uint32		tmp_xlogid;
		uint32		tmp_xrecoff;

		if ((ptr == NULL) ||
		    sscanf(ptr, POSTMASTER_IN_RECOVERY_DETAIL_MSG " %X/%X\n",
		    &tmp_xlogid, &tmp_xrecoff) != 2)
		{
#ifdef USE_ASSERT_CHECKING
			elog(ERROR,
#else
			elog(LOG,
#endif
				"invalid in-recovery message %s "
				"(content=%d, dbid=%d) state=%d",
				PQerrorMessage(ftsInfo->conn),
				ftsInfo->primary_cdbinfo->config->segindex,
				ftsInfo->primary_cdbinfo->config->dbid,
				ftsInfo->state);
			return;
		}
		tmpptr = ((uint64) tmp_xlogid) << 32 | (uint64) tmp_xrecoff;

		/*
		 * If the xlog record returned from the primary is less than or
		 * equal to the xlog record we had saved from the last probe
		 * then we assume that recovery is not making progress. In the
		 * case of rolling panics on the primary the returned xlog
		 * location can be less than the recorded xlog location. In
		 * these cases of rolling panic or recovery hung we want to
		 * mark the primary as down.
		 */
		if (tmpptr <= ftsInfo->xlogrecptr)
		{
			elog(LOG, "FTS: detected segment is in recovery mode and not making progress (content=%d) "
				 "primary dbid=%d, mirror dbid=%d",
				 ftsInfo->primary_cdbinfo->config->segindex,
				 ftsInfo->primary_cdbinfo->config->dbid,
				 ftsInfo->mirror_cdbinfo->config->dbid);
		}
		else
		{
			ftsInfo->recovery_making_progress = true;
			ftsInfo->xlogrecptr = tmpptr;
			elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
				   "FTS: detected segment is in recovery mode replayed (%X/%X) (content=%d) "
				   "primary dbid=%d, mirror dbid=%d",
				   (uint32) (tmpptr >> 32),
				   (uint32) tmpptr,
				   ftsInfo->primary_cdbinfo->config->segindex,
				   ftsInfo->primary_cdbinfo->config->dbid,
				   ftsInfo->mirror_cdbinfo->config->dbid);
		}
	}
}

/*
 * Start a libpq connection for each "per segment" object in context.  If the
 * connection is already started for an object, advance libpq state machine for
 * that object by calling PQconnectPoll().  An established libpq connection
 * (authentication complete and ready-for-query received) is identified by: (1)
 * state of the "per segment" object is any of FTS_PROBE_SEGMENT,
 * FTS_SYNCREP_OFF_SEGMENT, FTS_PROMOTE_SEGMENT and (2) PQconnectPoll() returns
 * PGRES_POLLING_OK for the connection.
 *
 * Upon failure, transition that object to a failed state.
 */
static void
ftsConnect(fts_context *context)
{
	int i;
	for (i = 0; i < context->num_pairs; i++)
	{
		fts_segment_info *ftsInfo = &context->perSegInfos[i];
		elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
			   "FTS: ftsConnect (content=%d, dbid=%d) state=%d, "
			   "retry_count=%d, conn->status=%d",
			   ftsInfo->primary_cdbinfo->config->segindex,
			   ftsInfo->primary_cdbinfo->config->dbid,
			   ftsInfo->state, ftsInfo->retry_count,
			   ftsInfo->conn ? ftsInfo->conn->status : -1);
		if (ftsInfo->conn && PQstatus(ftsInfo->conn) == CONNECTION_OK)
			continue;
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SEGMENT:
			case FTS_SYNCREP_OFF_SEGMENT:
			case FTS_PROMOTE_SEGMENT:
				/*
				 * We always default to false.  If connect fails due to recovery in progress
				 * this variable will be set based on LSN value in error message.
				 */
				ftsInfo->recovery_making_progress = false;
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
							elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
								   "FTS: established libpq connection "
								   "(content=%d, dbid=%d) state=%d, "
								   "retry_count=%d, conn->status=%d",
								   ftsInfo->primary_cdbinfo->config->segindex,
								   ftsInfo->primary_cdbinfo->config->dbid,
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
							checkIfFailedDueToRecoveryInProgress(ftsInfo);
							elog(LOG, "FTS: cannot establish libpq connection "
								 "(content=%d, dbid=%d): %s, retry_count=%d",
								 ftsInfo->primary_cdbinfo->config->segindex,
								 ftsInfo->primary_cdbinfo->config->dbid,
								 PQerrorMessage(ftsInfo->conn),
								 ftsInfo->retry_count);
							break;

						default:
							elog(ERROR, "FTS: invalid response to PQconnectPoll"
								 " (content=%d, dbid=%d): %s",
								 ftsInfo->primary_cdbinfo->config->segindex,
								 ftsInfo->primary_cdbinfo->config->dbid,
								 PQerrorMessage(ftsInfo->conn));
							break;
					}
				}
				else
					elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
						   "FTS: ftsConnect (content=%d, dbid=%d) state=%d, "
						   "retry_count=%d, conn->status=%d pollfd.revents unset",
						   ftsInfo->primary_cdbinfo->config->segindex,
						   ftsInfo->primary_cdbinfo->config->dbid,
						   ftsInfo->state, ftsInfo->retry_count,
						   ftsInfo->conn ? ftsInfo->conn->status : -1);
				break;
			case FTS_PROBE_SUCCESS:
			case FTS_SYNCREP_OFF_SUCCESS:
			case FTS_PROMOTE_SUCCESS:
			case FTS_PROBE_FAILED:
			case FTS_SYNCREP_OFF_FAILED:
			case FTS_PROMOTE_FAILED:
			case FTS_PROBE_RETRY_WAIT:
			case FTS_SYNCREP_OFF_RETRY_WAIT:
			case FTS_PROMOTE_RETRY_WAIT:
			case FTS_RESPONSE_PROCESSED:
				break;
		}
	}
}

/*
 * Timeout is said to have occurred if greater than gp_fts_probe_timeout
 * seconds have elapsed since connection start and a response is not received.
 * Segments for which a response is received already are exempted from timeout
 * evaluation.
 */
static void
ftsCheckTimeout(fts_segment_info *ftsInfo, pg_time_t now)
{
	if (!IsFtsMessageStateSuccess(ftsInfo->state) &&
		(int) (now - ftsInfo->startTime) > gp_fts_probe_timeout)
	{
		elog(LOG,
			 "FTS timeout detected for (content=%d, dbid=%d) "
			 "state=%d, retry_count=%d,",
			 ftsInfo->primary_cdbinfo->config->segindex,
			 ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->state,
			 ftsInfo->retry_count);
		ftsInfo->state = nextFailedState(ftsInfo->state);
	}
}

static void
ftsPoll(fts_context *context)
{
	int i;
	int nfds=0;
	int nready;
	for (i = 0; i < context->num_pairs; i++)
	{
		fts_segment_info *ftsInfo = &context->perSegInfos[i];
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
		{
			elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
				   "FTS: ftsPoll() interrupted, nfds %d", nfds);
		}
		else
			elog(ERROR, "FTS: ftsPoll() failed: nfds %d, %m", nfds);
	}
	else if (nready == 0)
	{
		elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
			   "FTS: ftsPoll() timed out, nfds %d", nfds);
	}

	elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
		   "FTS: ftsPoll() found %d out of %d sockets ready",
		   nready, nfds);

	pg_time_t now = (pg_time_t) time(NULL);

	/* Record poll() response poll_revents for each "per_segment" object. */
	for (i = 0; i < context->num_pairs; i++)
	{
		fts_segment_info *ftsInfo = &context->perSegInfos[i];

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
			{
				ftsInfo->poll_events = 0;
			}
			else if (ftsInfo->poll_revents & (POLLHUP | POLLERR))
			{
				ftsInfo->state = nextFailedState(ftsInfo->state);
				elog(LOG,
					 "FTS poll failed (revents=%d, events=%d) for "
					 "(content=%d, dbid=%d) state=%d, retry_count=%d, "
					 "libpq status=%d, asyncStatus=%d",
					 ftsInfo->poll_revents, ftsInfo->poll_events,
					 ftsInfo->primary_cdbinfo->config->segindex,
					 ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->state,
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
					 ftsInfo->primary_cdbinfo->config->segindex,
					 ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->state,
					 ftsInfo->retry_count, ftsInfo->conn->status,
					 ftsInfo->conn->asyncStatus);
			}
			/* If poll timed-out above, check timeout */
			ftsCheckTimeout(ftsInfo, now);
		}
	}
}

/*
 * Send FTS query
 */
static void
ftsSend(fts_context *context)
{
	fts_segment_info *ftsInfo;
	const char *message_type;
	char message[FTS_MSG_MAX_LEN];
	int i;

	for (i = 0; i < context->num_pairs; i++)
	{
		ftsInfo = &context->perSegInfos[i];
		elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
			   "FTS: ftsSend (content=%d, dbid=%d) state=%d, "
			   "retry_count=%d, conn->asyncStatus=%d",
			   ftsInfo->primary_cdbinfo->config->segindex,
			   ftsInfo->primary_cdbinfo->config->dbid,
			   ftsInfo->state, ftsInfo->retry_count,
			   ftsInfo->conn ? ftsInfo->conn->asyncStatus : -1);
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SEGMENT:
			case FTS_SYNCREP_OFF_SEGMENT:
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
					message_type = FTS_MSG_PROBE;
				else if (ftsInfo->state == FTS_SYNCREP_OFF_SEGMENT)
					message_type = FTS_MSG_SYNCREP_OFF;
				else
					message_type = FTS_MSG_PROMOTE;

				snprintf(message, FTS_MSG_MAX_LEN, FTS_MSG_FORMAT,
						 message_type,
						 ftsInfo->primary_cdbinfo->config->dbid,
						 ftsInfo->primary_cdbinfo->config->segindex);

				if (PQsendQuery(ftsInfo->conn, message))
				{
					/*
					 * Message sent successfully, mark the socket to be polled
					 * for reading so we will be ready to read response when it
					 * arrives.
					 */
					ftsInfo->poll_events = POLLIN;
					elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
						   "FTS sent %s to (content=%d, dbid=%d), retry_count=%d",
						   message, ftsInfo->primary_cdbinfo->config->segindex,
						   ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->retry_count);
				}
				else
				{
					elog(LOG,
						 "FTS: failed to send %s to segment (content=%d, "
						 "dbid=%d) state=%d, retry_count=%d, "
						 "conn->asyncStatus=%d %s", message,
						 ftsInfo->primary_cdbinfo->config->segindex,
						 ftsInfo->primary_cdbinfo->config->dbid,
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
 * Record FTS handler's response from libpq result into fts_result
 */
static void
probeRecordResponse(fts_segment_info *ftsInfo, PGresult *result)
{
	ftsInfo->result.isPrimaryAlive = true;

	ftsInfo->result.isMirrorAlive = *PQgetvalue(result, 0,
			Anum_fts_message_response_is_mirror_up);

	ftsInfo->result.isInSync = *PQgetvalue(result, 0,
			Anum_fts_message_response_is_in_sync);

	ftsInfo->result.isSyncRepEnabled = *PQgetvalue(result, 0,
			Anum_fts_message_response_is_syncrep_enabled);

	ftsInfo->result.isRoleMirror = *PQgetvalue(result, 0,
			Anum_fts_message_response_is_role_mirror);

	ftsInfo->result.retryRequested = *PQgetvalue(result, 0,
			Anum_fts_message_response_request_retry);

	elogif(gp_log_fts >= GPVARS_VERBOSITY_DEBUG, LOG,
		   "FTS: segment (content=%d, dbid=%d, role=%c) reported "
		   "isMirrorUp %d, isInSync %d, isSyncRepEnabled %d, "
		   "isRoleMirror %d, and retryRequested %d to the prober.",
		   ftsInfo->primary_cdbinfo->config->segindex,
		   ftsInfo->primary_cdbinfo->config->dbid,
		   ftsInfo->primary_cdbinfo->config->role,
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
	fts_segment_info *ftsInfo;
	PGresult *result = NULL;
	int ntuples;
	int nfields;
	int i;

	for (i = 0; i < context->num_pairs; i++)
	{
		ftsInfo = &context->perSegInfos[i];
		elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
			   "FTS: ftsReceive (content=%d, dbid=%d) state=%d, "
			   "retry_count=%d, conn->asyncStatus=%d",
			   ftsInfo->primary_cdbinfo->config->segindex,
			   ftsInfo->primary_cdbinfo->config->dbid,
			   ftsInfo->state, ftsInfo->retry_count,
			   ftsInfo->conn ? ftsInfo->conn->asyncStatus : -1);
		switch(ftsInfo->state)
		{
			case FTS_PROBE_SEGMENT:
			case FTS_SYNCREP_OFF_SEGMENT:
			case FTS_PROMOTE_SEGMENT:
				/*
				 * The libpq connection must be established and a message must
				 * have arrived on the socket.
				 */
				if (PQstatus(ftsInfo->conn) != CONNECTION_OK ||
					!(ftsInfo->poll_revents & POLLIN))
					break;
				/* Read the response that has arrived. */
				if (!PQconsumeInput(ftsInfo->conn))
				{
					elog(LOG, "FTS: failed to read from (content=%d, dbid=%d)"
						 " state=%d, retry_count=%d, conn->asyncStatus=%d %s",
						 ftsInfo->primary_cdbinfo->config->segindex,
						 ftsInfo->primary_cdbinfo->config->dbid,
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
						 ftsInfo->primary_cdbinfo->config->segindex,
						 ftsInfo->primary_cdbinfo->config->dbid,
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
						 ftsInfo->primary_cdbinfo->config->segindex,
						 ftsInfo->primary_cdbinfo->config->dbid,
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
						 ftsInfo->primary_cdbinfo->config->segindex,
						 ftsInfo->primary_cdbinfo->config->dbid,
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
						 ftsInfo->primary_cdbinfo->config->segindex,
						 ftsInfo->primary_cdbinfo->config->dbid,
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
				break;
			default:
				/* Cannot receive response in any other state. */
				break;
		}

		/*
		 * Reference to the result should already be stored in
		 * connection object. If it is not then free explicitly.
		 */
		if (result && result != ftsInfo->conn->result)
		{
			PQclear(result);
			result = NULL;
		}
	}
}

static void
retryForFtsFailed(fts_segment_info *ftsInfo, pg_time_t now)
{
	if (ftsInfo->retry_count == gp_fts_probe_retries)
	{
		elog(LOG, "FTS max (%d) retries exhausted "
			"(content=%d, dbid=%d) state=%d",
			ftsInfo->retry_count,
			ftsInfo->primary_cdbinfo->config->segindex,
			ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->state);
		return;
	}

	ftsInfo->retry_count++;
	if (ftsInfo->state == FTS_PROBE_SUCCESS ||
		ftsInfo->state == FTS_PROBE_FAILED)
		ftsInfo->state = FTS_PROBE_RETRY_WAIT;
	else if (ftsInfo->state == FTS_SYNCREP_OFF_FAILED)
		ftsInfo->state = FTS_SYNCREP_OFF_RETRY_WAIT;
	else
		ftsInfo->state = FTS_PROMOTE_RETRY_WAIT;
	ftsInfo->retryStartTime = now;
	elogif(gp_log_fts == GPVARS_VERBOSITY_DEBUG, LOG,
		"FTS initialized retry start time to now "
		"(content=%d, dbid=%d) state=%d",
		ftsInfo->primary_cdbinfo->config->segindex,
		ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->state);

	PQfinish(ftsInfo->conn);
	ftsInfo->conn = NULL;
	ftsInfo->poll_events = ftsInfo->poll_revents = 0;
	/* Reset result before next attempt. */
	memset(&ftsInfo->result, 0, sizeof(fts_result));
}

/*
 * If retry attempts are available, transition the sgement to the start state
 * corresponding to their failure state.  If retries have exhausted, leave the
 * segment in the failure state.
 */
static void
processRetry(fts_context *context)
{
	fts_segment_info *ftsInfo;
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
				/* else, fallthrough */
			case FTS_PROBE_FAILED:
			case FTS_SYNCREP_OFF_FAILED:
			case FTS_PROMOTE_FAILED:
				retryForFtsFailed(ftsInfo, now);
				break;
			case FTS_PROBE_RETRY_WAIT:
			case FTS_SYNCREP_OFF_RETRY_WAIT:
			case FTS_PROMOTE_RETRY_WAIT:
				/* Wait for 1 second before making another attempt. */
				if ((int) (now - ftsInfo->retryStartTime) < 1)
					break;
				/*
				 * We have remained in retry state for over a second, it's time
				 * to make another attempt.
				 */
				elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
					   "FTS retrying attempt %d (content=%d, dbid=%d) "
					   "state=%d", ftsInfo->retry_count,
					   ftsInfo->primary_cdbinfo->config->segindex,
					   ftsInfo->primary_cdbinfo->config->dbid, ftsInfo->state);
				if (ftsInfo->state == FTS_PROBE_RETRY_WAIT)
					ftsInfo->state = FTS_PROBE_SEGMENT;
				else if (ftsInfo->state == FTS_SYNCREP_OFF_RETRY_WAIT)
					ftsInfo->state = FTS_SYNCREP_OFF_SEGMENT;
				else
					ftsInfo->state = FTS_PROMOTE_SEGMENT;
				break;
			default:
				break;
		}
	}
}

/*
 * Return true for segments whose response is ready to be processed.  Segments
 * whose response is already processed should have response->conn set to NULL.
 */
static bool
ftsResponseReady(fts_segment_info *ftsInfo)
{
	return (IsFtsMessageStateSuccess(ftsInfo->state) ||
			IsFtsMessageStateFailed(ftsInfo->state));
}

static bool
updateConfiguration(CdbComponentDatabaseInfo *primary,
					CdbComponentDatabaseInfo *mirror,
					char newPrimaryRole, char newMirrorRole,
					bool IsInSync, bool IsPrimaryAlive, bool IsMirrorAlive)
{
	bool UpdatePrimary = (IsPrimaryAlive != SEGMENT_IS_ALIVE(primary));
	bool UpdateMirror = (IsMirrorAlive != SEGMENT_IS_ALIVE(mirror));

	/*
	 * If probe response state is different from current state in
	 * configuration, update both primary and mirror.
	 */
	if (IsInSync != SEGMENT_IS_IN_SYNC(primary))
		UpdatePrimary = UpdateMirror = true;

	/*
	 * A mirror being promoted must be already in-sync in configuration.
	 * Update to the configuration must include mode as not-in-sync and primary
	 * status as down.
	 */
	AssertImply(newPrimaryRole == GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
				SEGMENT_IS_IN_SYNC(mirror) && !IsInSync && !IsPrimaryAlive);

	/*
	 * Primary and mirror should always have the same mode in configuration,
	 * either both reflecting in-sync or not in-sync.
	 */
	Assert(primary->config->mode == mirror->config->mode);

	bool UpdateNeeded = UpdatePrimary || UpdateMirror;
	/*
	 * Commit/abort transaction below will destroy
	 * CurrentResourceOwner.  We need it for catalog reads.
	 */
	ResourceOwner save = CurrentResourceOwner;
	if (UpdateNeeded)
	{
		StartTransactionCommand();
		GetTransactionSnapshot();

		if (UpdatePrimary)
			probeWalRepUpdateConfig(primary->config->dbid, primary->config->segindex,
									newPrimaryRole, IsPrimaryAlive,
									IsInSync);

		if (UpdateMirror)
			probeWalRepUpdateConfig(mirror->config->dbid, mirror->config->segindex,
									newMirrorRole, IsMirrorAlive,
									IsInSync);

		CommitTransactionCommand();
		CurrentResourceOwner = save;

		/*
		 * Update the status to in-memory variable as well used by
		 * dispatcher, now that changes has been persisted to catalog.
		 */
		Assert(ftsProbeInfo);
		ftsLock();
		if (IsPrimaryAlive)
			FTS_STATUS_SET_UP(ftsProbeInfo->status[primary->config->dbid]);
		else
			FTS_STATUS_SET_DOWN(ftsProbeInfo->status[primary->config->dbid]);

		if (IsMirrorAlive)
			FTS_STATUS_SET_UP(ftsProbeInfo->status[mirror->config->dbid]);
		else
			FTS_STATUS_SET_DOWN(ftsProbeInfo->status[mirror->config->dbid]);
		ftsUnlock();
	}

	return UpdateNeeded;
}

/*
 * Process resonses from primary segments:
 * (a) Transition internal state so that segments can be messaged subsequently
 * (e.g. promotion and turning off syncrep).
 * (b) Update gp_segment_configuration catalog table, if needed.
 */
static bool
processResponse(fts_context *context)
{
	bool is_updated = false;

	for (int response_index = 0;
		 response_index < context->num_pairs && FtsIsActive();
		 response_index ++)
	{
		fts_segment_info *ftsInfo = &(context->perSegInfos[response_index]);

		/*
		 * Consider segments that are in final state (success / failure) and
		 * that are not already processed.
		 */
		if (!ftsResponseReady(ftsInfo))
			continue;

		/* All retries must have exhausted before a failure is processed. */
		AssertImply(IsFtsMessageStateFailed(ftsInfo->state),
					ftsInfo->retry_count == gp_fts_probe_retries);

		CdbComponentDatabaseInfo *primary = ftsInfo->primary_cdbinfo;

		CdbComponentDatabaseInfo *mirror = ftsInfo->mirror_cdbinfo;

		bool IsPrimaryAlive = ftsInfo->result.isPrimaryAlive;
		/* Trust a response from primary only if it's alive. */
		bool IsMirrorAlive =  IsPrimaryAlive ?
			ftsInfo->result.isMirrorAlive : SEGMENT_IS_ALIVE(mirror);
		bool IsInSync = IsPrimaryAlive ?
			ftsInfo->result.isInSync : false;

		/* If primary and mirror are in sync, then both have to be ALIVE. */
		AssertImply(IsInSync, IsPrimaryAlive && IsMirrorAlive);
		/* Primary must enable syncrep as long as it thinks mirror is alive. */
		AssertImply(IsMirrorAlive && IsPrimaryAlive,
					ftsInfo->result.isSyncRepEnabled);

		switch(ftsInfo->state)
		{
			case FTS_PROBE_SUCCESS:
				Assert(IsPrimaryAlive);
				if (ftsInfo->result.isSyncRepEnabled && !IsMirrorAlive)
				{
					if (!ftsInfo->result.retryRequested)
					{
						/*
						 * Primaries that have syncrep enabled continue to block
						 * commits until FTS update the mirror status as down.
						 */
						is_updated |= updateConfiguration(
							primary, mirror,
							GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
							GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
							IsInSync, IsPrimaryAlive, IsMirrorAlive);
						/*
						 * If mirror was marked up in configuration, it must have
						 * been marked down by updateConfiguration().
						 */
						AssertImply(SEGMENT_IS_ALIVE(mirror), is_updated);
						/*
						 * Now that the configuration is updated, FTS must notify
						 * the primaries to unblock commits by sending syncrep off
						 * message.
						 */
						ftsInfo->state = FTS_SYNCREP_OFF_SEGMENT;
						elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
							   "FTS turning syncrep off on (content=%d, dbid=%d)",
							   primary->config->segindex, primary->config->dbid);
					}
					else
					{
						elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
							   "FTS skipping mirror down update for (content=%d) as retryRequested",
							   primary->config->segindex);
						ftsInfo->state = FTS_RESPONSE_PROCESSED;
					}
				}
				else if (ftsInfo->result.isRoleMirror)
				{
					/*
					 * A promote message sent previously didn't make it to the
					 * mirror.  Catalog must have been updated before sending
					 * the previous promote message.
					 */
					Assert(!IsMirrorAlive);
					Assert(!SEGMENT_IS_ALIVE(mirror));
					Assert(SEGMENT_IS_NOT_INSYNC(mirror));
					Assert(SEGMENT_IS_NOT_INSYNC(primary));
					Assert(!ftsInfo->result.isSyncRepEnabled);
					elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
						   "FTS resending promote request to (content=%d,"
						   " dbid=%d)", primary->config->segindex, primary->config->dbid);
					ftsInfo->state = FTS_PROMOTE_SEGMENT;
				}
				else
				{
					/*
					 * No subsequent state transition needed, update catalog if
					 * necessary.  The cases are mirror status found to change
					 * from down to up, mode found to change from not in-sync
					 * to in-sync or syncrep found to change from off to on.
					 */
					is_updated |= updateConfiguration(
						primary, mirror,
						GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
						GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
						IsInSync, IsPrimaryAlive, IsMirrorAlive);
					ftsInfo->state = FTS_RESPONSE_PROCESSED;
				}
				break;
			case FTS_PROBE_FAILED:
				/* Primary is down */

				/* If primary is in recovery, do not mark it down and promote mirror */
				if (ftsInfo->recovery_making_progress)
				{
					Assert(strstr(PQerrorMessage(ftsInfo->conn), _(POSTMASTER_IN_RECOVERY_MSG)) ||
						   strstr(PQerrorMessage(ftsInfo->conn), _(POSTMASTER_IN_STARTUP_MSG)));
					elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
						 "FTS: detected segment is in recovery mode and making "
						 "progress (content=%d) primary dbid=%d, mirror dbid=%d",
						 primary->config->segindex, primary->config->dbid, mirror->config->dbid);

					ftsInfo->state = FTS_RESPONSE_PROCESSED;
					break;
				}

				Assert(!IsPrimaryAlive);
				/* See if mirror can be promoted. */
				if (SEGMENT_IS_IN_SYNC(mirror))
				{
					/*
					 * Primary and mirror must have been recorded as in-sync
					 * before the probe.
					 */
					Assert(SEGMENT_IS_IN_SYNC(primary));

					/*
					 * Flip the roles and mark the failed primary as down in
					 * FTS configuration before sending promote message.
					 * Dispatcher should no longer consider the failed primary
					 * for gang creation, FTS should no longer probe the failed
					 * primary.
					 */
					is_updated |= updateConfiguration(
						primary, mirror,
						GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newPrimaryRole */
						GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newMirrorRole */
						IsInSync, IsPrimaryAlive, IsMirrorAlive);
					Assert(is_updated);

					/*
					 * Swap the primary and mirror references so that the
					 * mirror will be promoted in subsequent connect, poll,
					 * send, receive steps.
					 */
					elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
						   "FTS promoting mirror (content=%d, dbid=%d) "
						   "to be the new primary",
						   mirror->config->segindex, mirror->config->dbid);
					ftsInfo->state = FTS_PROMOTE_SEGMENT;
					ftsInfo->primary_cdbinfo = mirror;
					ftsInfo->mirror_cdbinfo = primary;
				}
				else
				{
					elog(WARNING, "FTS double fault detected (content=%d) "
						 "primary dbid=%d, mirror dbid=%d",
						 primary->config->segindex, primary->config->dbid, mirror->config->dbid);
					ftsInfo->state = FTS_RESPONSE_PROCESSED;
				}
				break;
			case FTS_SYNCREP_OFF_FAILED:
				/*
				 * Another attempt to turn off syncrep will be made in the next
				 * probe cycle.  Until then, leave the transactions waiting for
				 * syncrep.  A worse alternative is to PANIC.
				 */
				elog(WARNING, "FTS failed to turn off syncrep on (content=%d,"
					 " dbid=%d)", primary->config->segindex, primary->config->dbid);
				ftsInfo->state = FTS_RESPONSE_PROCESSED;
				break;
			case FTS_PROMOTE_FAILED:
				elog(WARNING, "FTS double fault detected (content=%d) "
					 "primary dbid=%d, mirror dbid=%d",
					 primary->config->segindex, primary->config->dbid, mirror->config->dbid);
				ftsInfo->state = FTS_RESPONSE_PROCESSED;
				break;
			case FTS_PROMOTE_SUCCESS:
				elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
					   "FTS mirror (content=%d, dbid=%d) promotion "
					   "triggerred successfully",
					   primary->config->segindex, primary->config->dbid);
				ftsInfo->state = FTS_RESPONSE_PROCESSED;
				break;
			case FTS_SYNCREP_OFF_SUCCESS:
				elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
					   "FTS primary (content=%d, dbid=%d) notified to turn "
					   "syncrep off", primary->config->segindex, primary->config->dbid);
				ftsInfo->state = FTS_RESPONSE_PROCESSED;
				break;
			default:
				elog(ERROR, "FTS invalid internal state %d for (content=%d)"
					 "primary dbid=%d, mirror dbid=%d", ftsInfo->state,
					 primary->config->segindex, primary->config->dbid, mirror->config->dbid);
				break;
		}
		/* Close connection and reset result for next message, if any. */
		memset(&ftsInfo->result, 0, sizeof(fts_result));
		PQfinish(ftsInfo->conn);
		ftsInfo->conn = NULL;
		ftsInfo->poll_events = ftsInfo->poll_revents = 0;
		ftsInfo->retry_count = 0;
	}

	return is_updated;
}

#ifdef USE_ASSERT_CHECKING
static bool
FtsIsSegmentAlive(CdbComponentDatabaseInfo *segInfo)
{
	if (SEGMENT_IS_ACTIVE_MIRROR(segInfo) && SEGMENT_IS_ALIVE(segInfo))
		return true;

	if (SEGMENT_IS_ACTIVE_PRIMARY(segInfo))
		return true;

	return false;
}
#endif

/*
 * Initialize context before a probe cycle based on cluster configuration in
 * cdbs.
 */
static void
FtsWalRepInitProbeContext(CdbComponentDatabases *cdbs, fts_context *context)
{
	context->num_pairs = cdbs->total_segments;
	context->perSegInfos = (fts_segment_info *) palloc0(
		context->num_pairs * sizeof(fts_segment_info));

	int fts_index = 0;
	int cdb_index = 0;
	for(; cdb_index < cdbs->total_segment_dbs; cdb_index++)
	{
		CdbComponentDatabaseInfo *primary = &(cdbs->segment_db_info[cdb_index]);
		if (!SEGMENT_IS_ACTIVE_PRIMARY(primary))
			continue;
		CdbComponentDatabaseInfo *mirror = FtsGetPeerSegment(cdbs,
															 primary->config->segindex,
															 primary->config->dbid);
		/*
		 * If there is no mirror under this primary, no need to probe.
		 */
		if (!mirror)
		{
			context->num_pairs--;
			continue;
		}

		/* primary in catalog will NEVER be marked down. */
		Assert(FtsIsSegmentAlive(primary));

		fts_segment_info *ftsInfo = &(context->perSegInfos[fts_index]);
		/*
		 * Initialize the response object.  Response from a segment will be
		 * processed only if ftsInfo->state is one of SUCCESS states.  If a
		 * failure is encountered in messaging a segment, its response will not
		 * be processed.
		 */
		ftsInfo->result.isPrimaryAlive = false;
		ftsInfo->result.isMirrorAlive = false;
		ftsInfo->result.isInSync = false;
		ftsInfo->result.isSyncRepEnabled = false;
		ftsInfo->result.retryRequested = false;
		ftsInfo->result.isRoleMirror = false;
		ftsInfo->result.dbid = primary->config->dbid;
		ftsInfo->state = FTS_PROBE_SEGMENT;
		ftsInfo->recovery_making_progress = false;
		ftsInfo->xlogrecptr = InvalidXLogRecPtr;

		ftsInfo->primary_cdbinfo = primary;
		ftsInfo->mirror_cdbinfo = mirror;

		Assert(fts_index < context->num_pairs);
		fts_index ++;
	}
}

static void
InitPollFds(size_t size)
{
	PollFds = (struct pollfd *) palloc0(size * sizeof(struct pollfd));
}

bool
FtsWalRepMessageSegments(CdbComponentDatabases *cdbs)
{
	bool is_updated = false;
	fts_context context;

	FtsWalRepInitProbeContext(cdbs, &context);
	InitPollFds(cdbs->total_segments);

	while (!allDone(&context) && FtsIsActive())
	{
		ftsConnect(&context);
		ftsPoll(&context);
		ftsSend(&context);
		ftsReceive(&context);
		processRetry(&context);
		is_updated |= processResponse(&context);
	}
	int i;
	if (!FtsIsActive())
	{
		for (i = 0; i < context.num_pairs; i++)
		{
			if (context.perSegInfos[i].conn)
			{
				PQfinish(context.perSegInfos[i].conn);
				context.perSegInfos[i].conn = NULL;
			}
		}
	}
#ifdef USE_ASSERT_CHECKING
	/*
	 * At the end of probe cycle, there shouldn't be any active libpq
	 * connections.
	 */
	for (i = 0; i < context.num_pairs; i++)
	{
		if (context.perSegInfos[i].conn != NULL)
			elog(ERROR,
				 "FTS libpq connection left open (content=%d, dbid=%d)"
				 " state=%d, retry_count=%d, conn->status=%d",
				 context.perSegInfos[i].primary_cdbinfo->config->segindex,
				 context.perSegInfos[i].primary_cdbinfo->config->dbid,
				 context.perSegInfos[i].state,
				 context.perSegInfos[i].retry_count,
				 context.perSegInfos[i].conn->status);
	}
#endif
	pfree(context.perSegInfos);
	pfree(PollFds);
	return is_updated;
}

/* EOF */
