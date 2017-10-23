
/*-------------------------------------------------------------------------
 *
 * cdbdisp_async.c
 *	  Functions for asynchronous implementation of dispatching
 *	  commands to QExecutors.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbdisp_async.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include <limits.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "storage/ipc.h"		/* For proc_exit_inprogress  */
#include "tcop/tcopprot.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_async.h"
#include "cdb/cdbdispatchresult.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpq.h"
#include "miscadmin.h"

#define DISPATCH_WAIT_TIMEOUT_MSEC 2000

/*
 * Ideally, we should set timeout to zero to cancel QEs as soon as possible,
 * but considering the cost of sending cancel signal is high, we want to process
 * as many finishing QEs as possible before cancelling
 */
#define DISPATCH_WAIT_CANCEL_TIMEOUT_MSEC 100

typedef struct CdbDispatchCmdAsync
{

	/*
	 * dispatchResultPtrArray: Array[0..dispatchCount-1] of CdbDispatchResult*
	 * Each CdbDispatchResult object points to a SegmentDatabaseDescriptor
	 * that dispatcher will send the command to.
	 */
	struct CdbDispatchResult **dispatchResultPtrArray;

	/* Number of segment DBs dispatched */
	int			dispatchCount;

	/*
	 * Depending on this mode, we may send query cancel or query finish
	 * message to QE while we are waiting it to complete.  NONE means we
	 * expect QE to complete without any instruction.
	 */
	volatile DispatchWaitMode waitMode;

	/*
	 * Text information to dispatch: The format is type(1 byte) + length(size
	 * of int) + content(n bytes)
	 *
	 * For DTX command, type is 'T', it's built by function
	 * buildGpDtxProtocolCommand. For query, type is 'M', it's built by
	 * function buildGpQueryString.
	 */
	char	   *query_text;
	int			query_text_len;

} CdbDispatchCmdAsync;

static int	timeoutCounter = 0;

static void *cdbdisp_makeDispatchParams_async(int maxSlices, char *queryText, int len);

static void cdbdisp_checkDispatchResult_async(struct CdbDispatcherState *ds,
								  DispatchWaitMode waitMode);

static void cdbdisp_dispatchToGang_async(struct CdbDispatcherState *ds,
							 struct Gang *gp,
							 int sliceIndex,
							 CdbDispatchDirectDesc *dispDirect);
static void
			cdbdisp_waitDispatchFinish_async(struct CdbDispatcherState *ds);

static bool
			cdbdisp_checkForCancel_async(struct CdbDispatcherState *ds);

DispatcherInternalFuncs DispatcherAsyncFuncs =
{
	NULL,
	cdbdisp_checkForCancel_async,
	cdbdisp_makeDispatchParams_async,
	cdbdisp_checkDispatchResult_async,
	cdbdisp_dispatchToGang_async,
	cdbdisp_waitDispatchFinish_async
};


static void dispatchCommand(CdbDispatchResult *dispatchResult,
				const char *query_text,
				int query_text_len);

static void checkDispatchResult(CdbDispatcherState *ds,
					bool wait);

static bool processResults(CdbDispatchResult *dispatchResult);

static void
			signalQEs(CdbDispatchCmdAsync *pParms);

static void
			checkSegmentAlive(CdbDispatchCmdAsync *pParms);

static void
			handlePollError(CdbDispatchCmdAsync *pParms);

static void
			handlePollSuccess(CdbDispatchCmdAsync *pParms, struct pollfd *fds);

/*
 * Check dispatch result.
 * Don't wait all dispatch commands to complete.
 *
 * Return true if any connection received error.
 */
static bool
cdbdisp_checkForCancel_async(struct CdbDispatcherState *ds)
{
	Assert(ds);

	checkDispatchResult(ds, false);
	return cdbdisp_checkResultsErrcode(ds->primaryResults);
}

/*
 * Block until all data are dispatched.
 */
static void
cdbdisp_waitDispatchFinish_async(struct CdbDispatcherState *ds)
{
	const static int DISPATCH_POLL_TIMEOUT = 500;
	struct pollfd *fds;
	int			nfds,
				i;
	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;
	int			dispatchCount = pParms->dispatchCount;

	fds = (struct pollfd *) palloc(dispatchCount * sizeof(struct pollfd));

	while (true)
	{
		int			pollRet;

		nfds = 0;
		memset(fds, 0, dispatchCount * sizeof(struct pollfd));

		for (i = 0; i < dispatchCount; i++)
		{
			CdbDispatchResult *qeResult = pParms->dispatchResultPtrArray[i];
			SegmentDatabaseDescriptor *segdbDesc = qeResult->segdbDesc;
			PGconn	   *conn = segdbDesc->conn;
			int			ret;

			/* skip already completed connections */
			if (conn->outCount == 0)
				continue;

			/*
			 * call send for this connection regardless of its POLLOUT status,
			 * because it may be writable NOW
			 */
			ret = pqFlushNonBlocking(conn);

			if (ret == 0)
				continue;
			else if (ret > 0)
			{
				int			sock = PQsocket(segdbDesc->conn);

				Assert(sock >= 0);
				fds[nfds].fd = sock;
				fds[nfds].events = POLLOUT;
				nfds++;
			}
			else if (ret < 0)
			{
				pqHandleSendFailure(conn);
				char	   *msg = PQerrorMessage(conn);

				qeResult->stillRunning = false;
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("Command could not be dispatch to segment %s: %s", qeResult->segdbDesc->whoami, msg ? msg : "unknown error")));
			}
		}

		if (nfds == 0)
			break;

		/* guarantee poll() is interruptible */
		do
		{
			CHECK_FOR_INTERRUPTS();

			pollRet = poll(fds, nfds, DISPATCH_POLL_TIMEOUT);
			if (pollRet == 0)
				ELOG_DISPATCHER_DEBUG("cdbdisp_waitDispatchFinish_async(): Dispatch poll timeout after %d ms", DISPATCH_POLL_TIMEOUT);
		}
		while (pollRet == 0 || (pollRet < 0 && (SOCK_ERRNO == EINTR || SOCK_ERRNO == EAGAIN)));

		if (pollRet < 0)
			elog(ERROR, "Poll failed during dispatch");
	}

	pfree(fds);
}

/*
 * Dispatch command to gang.
 *
 * Throw out error to upper try-catch block if anything goes wrong. This function only kicks off dispatching,
 * call cdbdisp_waitDispatchFinish_async to ensure the completion
 */
static void
cdbdisp_dispatchToGang_async(struct CdbDispatcherState *ds,
							 struct Gang *gp,
							 int sliceIndex,
							 CdbDispatchDirectDesc *dispDirect)
{
	int			i;
	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;

	/*
	 * Start the dispatching
	 */
	for (i = 0; i < gp->size; i++)
	{
		CdbDispatchResult *qeResult;

		SegmentDatabaseDescriptor *segdbDesc = &gp->db_descriptors[i];

		Assert(segdbDesc != NULL);

		if (dispDirect->directed_dispatch)
		{
			/* We can direct dispatch to one segment DB only */
			Assert(dispDirect->count == 1);
			if (dispDirect->content[0] != segdbDesc->segindex)
				continue;
		}

		/*
		 * Initialize the QE's CdbDispatchResult object.
		 */
		qeResult = cdbdisp_makeResult(ds->primaryResults, segdbDesc, sliceIndex);
		if (qeResult == NULL)
		{
			/*
			 * writer_gang could be NULL if this is an extended query.
			 */
			if (ds->primaryResults->writer_gang)
				ds->primaryResults->writer_gang->dispatcherActive = true;

			elog(FATAL, "could not allocate resources for segworker communication");
		}
		pParms->dispatchResultPtrArray[pParms->dispatchCount++] = qeResult;

		dispatchCommand(qeResult, pParms->query_text, pParms->query_text_len);
	}
}

/*
 * Check dispatch result.
 *
 * Wait all dispatch work to complete, either success or fail.
 * (Set stillRunning to true when one dispatch work is completed)
 */
static void
cdbdisp_checkDispatchResult_async(struct CdbDispatcherState *ds,
								  DispatchWaitMode waitMode)
{
	Assert(ds != NULL);
	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;

	/* cdbdisp_destroyDispatcherState is called */
	if (pParms == NULL)
		return;

	/*
	 * Don't overwrite DISPATCH_WAIT_CANCEL or DISPATCH_WAIT_FINISH with
	 * DISPATCH_WAIT_NONE
	 */
	if (waitMode != DISPATCH_WAIT_NONE)
		pParms->waitMode = waitMode;

	checkDispatchResult(ds, true);

	/*
	 * It looks like everything went fine, make sure we don't miss a user
	 * cancellation?
	 *
	 * The waitMode argument is NONE when we are doing "normal work".
	 */
	if (waitMode == DISPATCH_WAIT_NONE || waitMode == DISPATCH_WAIT_FINISH)
		CHECK_FOR_INTERRUPTS();
}

/*
 * Allocates memory for a CdbDispatchCmdAsync structure and do the initialization.
 *
 * Memory will be freed in function cdbdisp_destroyDispatcherState by deleting the
 * memory context.
 */
static void *
cdbdisp_makeDispatchParams_async(int maxSlices, char *queryText, int len)
{
	int			maxResults = maxSlices * getgpsegmentCount();
	int			size = 0;

	CdbDispatchCmdAsync *pParms = palloc0(sizeof(CdbDispatchCmdAsync));

	size = maxResults * sizeof(CdbDispatchResult *);
	pParms->dispatchResultPtrArray = (CdbDispatchResult **) palloc0(size);
	pParms->dispatchCount = 0;
	pParms->waitMode = DISPATCH_WAIT_NONE;
	pParms->query_text = queryText;
	pParms->query_text_len = len;

	return (void *) pParms;
}

/*
 * Receive and process results from all running QEs.
 *
 * wait: true, wait until all dispatch works are completed.
 *       false, return immediate when there's no more data.
 *
 * Don't throw out error, instead, append the error message to
 * CdbDispatchResult.error_message.
 */
static void
checkDispatchResult(CdbDispatcherState *ds,
					bool wait)
{
	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;
	CdbDispatchResults *meleeResults = ds->primaryResults;
	SegmentDatabaseDescriptor *segdbDesc;
	CdbDispatchResult *dispatchResult;
	int			i;
	int			db_count = 0;
	int			timeout = 0;
	bool		sentSignal = false;
	struct pollfd *fds;

	db_count = pParms->dispatchCount;
	fds = (struct pollfd *) palloc(db_count * sizeof(struct pollfd));

	/*
	 * OK, we are finished submitting the command to the segdbs. Now, we have
	 * to wait for them to finish.
	 */
	for (;;)
	{
		int			sock;
		int			n;
		int			nfds = 0;

		/*
		 * bail-out if we are dying. Once QD dies, QE will recognize it
		 * shortly anyway.
		 */
		if (proc_exit_inprogress)
			break;

		/*
		 * escalate waitMode to cancel if: - user interrupt has occurred, - or
		 * an error has been reported by any QE, - in case the caller wants
		 * cancelOnError
		 */
		if ((InterruptPending || meleeResults->errcode) && meleeResults->cancelOnError)
			pParms->waitMode = DISPATCH_WAIT_CANCEL;

		/*
		 * Which QEs are still running and could send results to us?
		 */
		for (i = 0; i < db_count; i++)
		{
			dispatchResult = pParms->dispatchResultPtrArray[i];
			segdbDesc = dispatchResult->segdbDesc;

			/*
			 * Already finished with this QE?
			 */
			if (!dispatchResult->stillRunning)
				continue;

			Assert(!cdbconn_isBadConnection(segdbDesc));

			/*
			 * Add socket to fd_set if still connected.
			 */
			sock = PQsocket(segdbDesc->conn);
			Assert(sock >= 0);
			fds[nfds].fd = sock;
			fds[nfds].events = POLLIN;
			nfds++;
		}

		/*
		 * Break out when no QEs still running.
		 */
		if (nfds <= 0)
			break;

		/*
		 * Wait for results from QEs
		 *
		 * Don't wait if: - this is called from interconnect to check if
		 * there's any error.
		 *
		 * Lower the timeout if: - we need send signal to QEs.
		 */
		if (!wait)
			timeout = 0;
		else if (pParms->waitMode == DISPATCH_WAIT_NONE || sentSignal)
			timeout = DISPATCH_WAIT_TIMEOUT_MSEC;
		else
			timeout = DISPATCH_WAIT_CANCEL_TIMEOUT_MSEC;

		n = poll(fds, nfds, timeout);

		/*
		 * poll returns with an error, including one due to an interrupted
		 * call
		 */
		if (n < 0)
		{
			int			sock_errno = SOCK_ERRNO;

			if (sock_errno == EINTR)
				continue;

			elog(LOG, "handlePollError poll() failed; errno=%d", sock_errno);

			handlePollError(pParms);
			checkSegmentAlive(pParms);

			if (pParms->waitMode != DISPATCH_WAIT_NONE)
			{
				signalQEs(pParms);
				sentSignal = true;
			}

			if (!wait)
				break;
		}
		/* If the time limit expires, poll() returns 0 */
		else if (n == 0)
		{
			if (pParms->waitMode != DISPATCH_WAIT_NONE)
			{
				signalQEs(pParms);
				sentSignal = true;
			}

			if (timeoutCounter++ > (wait ? 30 : 300))
			{
				checkSegmentAlive(pParms);
				timeoutCounter = 0;
			}

			if (!wait)
				break;
		}
		/* We have data waiting on one or more of the connections. */
		else
			handlePollSuccess(pParms, fds);
	}

	pfree(fds);
}

/*
 * Helper function that actually kicks off the command on the libpq connection.
 */
static void
dispatchCommand(CdbDispatchResult *dispatchResult,
				const char *query_text,
				int query_text_len)
{
	TimestampTz beforeSend = 0;
	long		secs;
	int			usecs;

	if (DEBUG1 >= log_min_messages)
		beforeSend = GetCurrentTimestamp();

	/*
	 * Submit the command asynchronously.
	 */
	if (PQsendGpQuery_shared(dispatchResult->segdbDesc->conn, (char *) query_text, query_text_len, true) == 0)
	{
		char	   *msg = PQerrorMessage(dispatchResult->segdbDesc->conn);

		dispatchResult->stillRunning = false;
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("Command could not be dispatch to segment %s: %s",
						dispatchResult->segdbDesc->whoami, msg ? msg : "unknown error")));
	}

	if (DEBUG1 >= log_min_messages)
	{
		TimestampDifference(beforeSend, GetCurrentTimestamp(), &secs, &usecs);

		if (secs != 0 || usecs > 1000)	/* Time > 1ms? */
			elog(LOG, "time for PQsendGpQuery_shared %ld.%06d", secs, usecs);
	}

	/*
	 * We'll keep monitoring this QE -- whether or not the command was
	 * dispatched -- in order to check for a lost connection or any other
	 * errors that libpq might have in store for us.
	 */
	dispatchResult->stillRunning = true;
	dispatchResult->hasDispatched = true;

	ELOG_DISPATCHER_DEBUG("Command dispatched to QE (%s)", dispatchResult->segdbDesc->whoami);
}

/*
 * Helper function to checkDispatchResult that handles errors that occur
 * during the poll() call.
 *
 * NOTE: The cleanup of the connections will be performed by handlePollTimeout().
 */
static void
handlePollError(CdbDispatchCmdAsync *pParms)
{
	int			i;

	for (i = 0; i < pParms->dispatchCount; i++)
	{
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

		/* Skip if already finished or didn't dispatch. */
		if (!dispatchResult->stillRunning)
			continue;

		/* We're done with this QE, sadly. */
		if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
		{
			char	   *msg = PQerrorMessage(segdbDesc->conn);

			if (msg)
				elog(LOG, "Dispatcher encountered connection error on %s: %s", segdbDesc->whoami, msg);

			elog(LOG, "Dispatcher noticed bad connection in handlePollError()");

			/* Save error info for later. */
			cdbdisp_appendMessageNonThread(dispatchResult, LOG,
										   "Error after dispatch from %s: %s",
										   segdbDesc->whoami,
										   msg ? msg : "unknown error");

			PQfinish(segdbDesc->conn);
			segdbDesc->conn = NULL;
			dispatchResult->stillRunning = false;
		}
	}

	return;
}

/*
 * Receive and process results from QEs.
 */
static void
handlePollSuccess(CdbDispatchCmdAsync *pParms,
				  struct pollfd *fds)
{
	int			currentFdNumber = 0;
	int			i = 0;

	/*
	 * We have data waiting on one or more of the connections.
	 */
	for (i = 0; i < pParms->dispatchCount; i++)
	{
		bool		finished;
		int			sock;
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

		/*
		 * Skip if already finished or didn't dispatch.
		 */
		if (!dispatchResult->stillRunning)
			continue;

		ELOG_DISPATCHER_DEBUG("looking for results from %d of %d (%s)",
							  i + 1, pParms->dispatchCount, segdbDesc->whoami);

		sock = PQsocket(segdbDesc->conn);
		Assert(sock >= 0);
		Assert(sock == fds[currentFdNumber].fd);

		/*
		 * Skip this connection if it has no input available.
		 */
		if (!(fds[currentFdNumber++].revents & POLLIN))
			continue;

		ELOG_DISPATCHER_DEBUG("PQsocket says there are results from %d of %d (%s)",
							  i + 1, pParms->dispatchCount, segdbDesc->whoami);

		/*
		 * Receive and process results from this QE.
		 */
		finished = processResults(dispatchResult);

		/*
		 * Are we through with this QE now?
		 */
		if (finished)
		{
			dispatchResult->stillRunning = false;

			ELOG_DISPATCHER_DEBUG("processResults says we are finished with %d of %d (%s)",
								  i + 1, pParms->dispatchCount, segdbDesc->whoami);

			if (DEBUG1 >= log_min_messages)
			{
				char		msec_str[32];

				switch (check_log_duration(msec_str, false))
				{
					case 1:
					case 2:
						elog(LOG, "duration to dispatch result received from %d (seg %d): %s ms",
							 i + 1, dispatchResult->segdbDesc->segindex, msec_str);
						break;
				}
			}

			if (PQisBusy(dispatchResult->segdbDesc->conn))
				elog(LOG, "We thought we were done, because finished==true, but libpq says we are still busy");
		}
		else
			ELOG_DISPATCHER_DEBUG("processResults says we have more to do with %d of %d (%s)",
								  i + 1, pParms->dispatchCount, segdbDesc->whoami);
	}
}

/*
 * Send finish or cancel signal to QEs if needed.
 */
static void
signalQEs(CdbDispatchCmdAsync *pParms)
{
	int			i;
	DispatchWaitMode waitMode = pParms->waitMode;

	for (i = 0; i < pParms->dispatchCount; i++)
	{
		char		errbuf[256];
		bool		sent = false;
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];

		Assert(dispatchResult != NULL);
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

		/*
		 * Don't send the signal if - QE is finished or canceled - the signal
		 * was already sent - connection is dead
		 */

		if (!dispatchResult->stillRunning ||
			dispatchResult->wasCanceled ||
			waitMode == dispatchResult->sentSignal ||
			cdbconn_isBadConnection(segdbDesc))
			continue;

		memset(errbuf, 0, sizeof(errbuf));

		sent = cdbconn_signalQE(segdbDesc, errbuf, waitMode == DISPATCH_WAIT_CANCEL);
		if (sent)
			dispatchResult->sentSignal = waitMode;
		else
			elog(LOG, "Unable to cancel: %s",
				 strlen(errbuf) == 0 ? "cannot allocate PGCancel" : errbuf);
	}
}

/*
 * Check if any segment DB down is detected by FTS.
 *
 * Issue a FTS probe every 1 minute.
 */
static void
checkSegmentAlive(CdbDispatchCmdAsync *pParms)
{
	int			i;
	bool		forceScan = true;

	/*
	 * check the connection still valid, set 1 min time interval this may
	 * affect performance, should turn it off if required.
	 */
	for (i = 0; i < pParms->dispatchCount; i++)
	{
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

		/*
		 * Skip if already finished or didn't dispatch.
		 */
		if (!dispatchResult->stillRunning)
			continue;

		/*
		 * Skip the entry db.
		 */
		if (segdbDesc->segindex < 0)
			continue;

		ELOG_DISPATCHER_DEBUG("FTS testing connection %d of %d (%s)",
							  i + 1, pParms->dispatchCount, segdbDesc->whoami);

		if (!FtsTestConnection(segdbDesc->segment_database_info, forceScan))
		{
			char	   *msg = PQerrorMessage(segdbDesc->conn);

			dispatchResult->stillRunning = false;
			cdbdisp_appendMessageNonThread(dispatchResult, LOG,
										   "FTS detected connection lost during dispatch to %s: %s",
										   dispatchResult->segdbDesc->whoami, msg ? msg : "unknown error");

			/*
			 * Not a good idea to store into the PGconn object. Instead, just
			 * close it.
			 */
			PQfinish(segdbDesc->conn);
			segdbDesc->conn = NULL;
		}

		forceScan = false;
	}
}

/*
 * Receive and process input from one QE.
 *
 * Return true if all input are consumed or the connection went wrong.
 * Return false if there'er still more data expected.
 */
static bool
processResults(CdbDispatchResult *dispatchResult)
{
	SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
	char	   *msg;

	/*
	 * Receive input from QE.
	 */
	if (PQconsumeInput(segdbDesc->conn) == 0)
	{
		msg = PQerrorMessage(segdbDesc->conn);
		cdbdisp_appendMessageNonThread(dispatchResult, LOG,
									   "Error on receive from %s: %s",
									   segdbDesc->whoami, msg ? msg : "unknown error");
		return true;
	}

	/*
	 * If we have received one or more complete messages, process them.
	 */
	while (!PQisBusy(segdbDesc->conn))
	{
		/* loop to call PQgetResult; won't block */
		PGresult   *pRes;
		ExecStatusType resultStatus;
		int			resultIndex;

		/*
		 * PQisBusy() does some error handling, which can cause the connection
		 * to die -- we can't just continue on as if the connection is happy
		 * without checking first.
		 *
		 * For example, cdbdisp_numPGresult() will return a completely bogus
		 * value!
		 */
		if (cdbconn_isBadConnection(segdbDesc))
		{
			msg = PQerrorMessage(segdbDesc->conn);
			cdbdisp_appendMessageNonThread(dispatchResult, LOG,
										   "Connection lost when receiving from %s: %s",
										   segdbDesc->whoami, msg ? msg : "unknown error");
			return true;
		}

		/*
		 * Get one message.
		 */
		ELOG_DISPATCHER_DEBUG("PQgetResult");
		pRes = PQgetResult(segdbDesc->conn);

		/*
		 * Command is complete when PGgetResult() returns NULL. It is critical
		 * that for any connection that had an asynchronous command sent thru
		 * it, we call PQgetResult until it returns NULL. Otherwise, the next
		 * time a command is sent to that connection, it will return an error
		 * that there's a command pending.
		 */
		if (!pRes)
		{
			ELOG_DISPATCHER_DEBUG("%s -> idle", segdbDesc->whoami);
			/* this is normal end of command */
			return true;
		}

		/*
		 * Attach the PGresult object to the CdbDispatchResult object.
		 */
		resultIndex = cdbdisp_numPGresult(dispatchResult);
		cdbdisp_appendResult(dispatchResult, pRes);

		/*
		 * Did a command complete successfully?
		 */
		resultStatus = PQresultStatus(pRes);
		if (resultStatus == PGRES_COMMAND_OK ||
			resultStatus == PGRES_TUPLES_OK ||
			resultStatus == PGRES_COPY_IN ||
			resultStatus == PGRES_COPY_OUT ||
			resultStatus == PGRES_EMPTY_QUERY)
		{
			ELOG_DISPATCHER_DEBUG("%s -> ok %s",
								  segdbDesc->whoami,
								  PQcmdStatus(pRes) ? PQcmdStatus(pRes) : "(no cmdStatus)");

			if (resultStatus == PGRES_EMPTY_QUERY)
				ELOG_DISPATCHER_DEBUG("QE received empty query.");

			/*
			 * Save the index of the last successful PGresult. Can be given to
			 * cdbdisp_getPGresult() to get tuple count, etc.
			 */
			dispatchResult->okindex = resultIndex;

			/*
			 * SREH - get number of rows rejected from QE if any
			 */
			if (pRes->numRejected > 0)
				dispatchResult->numrowsrejected += pRes->numRejected;

			/*
			 * COPY FROM ON SEGMENT - get the number of rows completed by QE
			 * if any
			 */
			if (pRes->numCompleted > 0)
				dispatchResult->numrowscompleted += pRes->numCompleted;

			if (resultStatus == PGRES_COPY_IN ||
				resultStatus == PGRES_COPY_OUT)
				return true;
		}

		/*
		 * Note QE error. Cancel the whole statement if requested.
		 */
		else
		{
			/* QE reported an error */
			char	   *sqlstate = PQresultErrorField(pRes, PG_DIAG_SQLSTATE);
			int			errcode = 0;

			msg = PQresultErrorMessage(pRes);

			ELOG_DISPATCHER_DEBUG("%s -> %s %s  %s",
								  segdbDesc->whoami,
								  PQresStatus(resultStatus),
								  sqlstate ? sqlstate : "(no SQLSTATE)",
								  msg);

			/*
			 * Convert SQLSTATE to an error code (ERRCODE_xxx). Use a generic
			 * nonzero error code if no SQLSTATE.
			 */
			if (sqlstate && strlen(sqlstate) == 5)
				errcode = sqlstate_to_errcode(sqlstate);

			/*
			 * Save first error code and the index of its PGresult buffer
			 * entry.
			 */
			cdbdisp_seterrcode(errcode, resultIndex, dispatchResult);
		}
	}

	return false;				/* we must keep on monitoring this socket */
}
