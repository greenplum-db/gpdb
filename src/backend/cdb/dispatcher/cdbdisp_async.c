/*-------------------------------------------------------------------------
 *
 * cdbdisp_async.c
 *	  Functions for asynchronous implementation of dispatching
 *	  commands to QExecutors.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbdisp_async.c
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

#include "pgstat.h"
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
#include "commands/sequence.h"
#include "access/xact.h"
#include "utils/timestamp.h"
#define DISPATCH_WAIT_TIMEOUT_MSEC 2000

/*
 * Ideally, we should set timeout to zero to cancel QEs as soon as possible,
 * but considering the cost of sending cancel signal is high, we want to process
 * as many finishing QEs as possible before cancelling
 */
#define DISPATCH_WAIT_CANCEL_TIMEOUT_MSEC 100

/*
 * DISPATCH_NO_WAIT means return immediate when there's no more data,
 * DISPATCH_WAIT_UNTIL_FINISH means wait until all dispatch works are completed.
 */
#define DISPATCH_NO_WAIT 0
#define DISPATCH_WAIT_UNTIL_FINISH -1

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
	 * When waitMode is set to DISPATCH_WAIT_ACK_ROOT,
	 * the expected acknowledge message from QE should be specified.
	 * This field stores the expected acknowledge message.
	 */
	const char	*ackMessage;

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

static void *cdbdisp_makeDispatchParams_async(int maxSlices, int largestGangSize, char *queryText, int len);

static bool cdbdisp_checkAckMessage_async(struct CdbDispatcherState *ds, const char *message,
									int timeout_sec);

static void cdbdisp_checkDispatchResult_async(struct CdbDispatcherState *ds,
								  DispatchWaitMode waitMode);

static void cdbdisp_dispatchToGang_async(struct CdbDispatcherState *ds,
							 struct Gang *gp,
							 int sliceIndex);
static void	cdbdisp_waitDispatchFinish_async(struct CdbDispatcherState *ds);

static bool	cdbdisp_checkForCancel_async(struct CdbDispatcherState *ds);
static int *cdbdisp_getWaitSocketFds_async(struct CdbDispatcherState *ds, int *nsocks);

DispatcherInternalFuncs DispatcherAsyncFuncs =
{
	cdbdisp_checkForCancel_async,
	cdbdisp_getWaitSocketFds_async,
	cdbdisp_makeDispatchParams_async,
	cdbdisp_checkAckMessage_async,
	cdbdisp_checkDispatchResult_async,
	cdbdisp_dispatchToGang_async,
	cdbdisp_waitDispatchFinish_async
};


static void dispatchCommand(CdbDispatchResult *dispatchResult,
				const char *query_text,
				int query_text_len);

static void checkDispatchResult(CdbDispatcherState *ds, int timeout_sec);

static bool processResults(CdbDispatchResult *dispatchResult);

static void
			signalQEs(CdbDispatchCmdAsync *pParms);

static void
			checkSegmentAlive(CdbDispatchCmdAsync *pParms);

static void
			handlePollError(CdbDispatchCmdAsync *pParms);

static void
			handlePollSuccess(CdbDispatchCmdAsync *pParms, WaitEvent *revents, int nready);

static bool
			checkAckMessage(CdbDispatchResult *dispatchResult, const char *message);

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

	checkDispatchResult(ds, DISPATCH_NO_WAIT);
	return cdbdisp_checkResultsErrcode(ds->primaryResults);
}

/*
 * Return all FDs to wait for, after dispatching.
 *
 * nsocks is the returned socket fds number (as an output param):
 *
 * Return value is the array of waiting socket fds.
 * It's be palloced in this function, so caller need to pfree it.
 */
static int *
cdbdisp_getWaitSocketFds_async(struct CdbDispatcherState *ds, int *nsocks)
{
	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;
	int			i;
	int			*fds = NULL;

	Assert(ds);

	*nsocks = 0;
	if (proc_exit_inprogress)
		return NULL;

	fds = palloc(pParms->dispatchCount * sizeof(int));

	/*
	 * This should match the logic in cdbdisp_checkForCancel_async(). In
	 * particular, when cdbdisp_checkForCancel_async() is called, it must
	 * process any incoming data from the socket we return here, or we
	 * will busy wait.
	 */
	for (i = 0; i < pParms->dispatchCount; i++)
	{
		CdbDispatchResult *dispatchResult;
		SegmentDatabaseDescriptor *segdbDesc;

		dispatchResult = pParms->dispatchResultPtrArray[i];
		segdbDesc = dispatchResult->segdbDesc;

		Assert(!cdbconn_isBadConnection(segdbDesc));

		(fds)[(*nsocks)++] = PQsocket(segdbDesc->conn);
	}

	return fds;
}

/*
 * Block until all data are dispatched.
 */
static void
cdbdisp_waitDispatchFinish_async(struct CdbDispatcherState *ds)
{
	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;
	int				dispatchCount = pParms->dispatchCount;
	const static int DISPATCH_POLL_TIMEOUT = 500;
	int			i;

	/*
	 * DispWaitSet's lifecycle is in the whole QD process,
	 * so alloc it in the TopMemoryContext (rather than DispatcherContext)
	 */
	ResetWaitEventSet(&DispWaitSet, TopMemoryContext, dispatchCount);

	WaitEvent 		*revents = palloc(sizeof(WaitEvent) * dispatchCount);
	int 			*added = palloc0(sizeof(int) * dispatchCount);
	while (true)
	{
		int			pollRet;
		bool		allFlushed = true;

		for (i = 0; i < dispatchCount; i++)
		{
			CdbDispatchResult *qeResult = pParms->dispatchResultPtrArray[i];
			SegmentDatabaseDescriptor *segdbDesc = qeResult->segdbDesc;
			PGconn	   *conn = segdbDesc->conn;
			int			ret = 0;

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
				/* add segment sock to the waitset */
				if (!added[i])
				{
					int 	sock = PQsocket(conn);
					Assert(sock >= 0);
					AddWaitEventToSet(DispWaitSet, WL_SOCKET_WRITEABLE, sock, NULL, NULL);
					added[i] = 1;
				}

				allFlushed = false;
			}
			else if (ret < 0)
			{
				/* error message should be set up already */
				char	   *msg = PQerrorMessage(conn);

				qeResult->stillRunning = false;
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Command could not be dispatch to segment %s: %s", qeResult->segdbDesc->whoami, msg ? msg : "unknown error")));
			}
		}

		if (allFlushed)
			break;

		/* guarantee poll() is interruptible */
		do
		{
			CHECK_FOR_INTERRUPTS();

			pollRet = WaitEventSetWait(DispWaitSet, DISPATCH_POLL_TIMEOUT, revents, dispatchCount, WAIT_EVENT_DISP_FINISH);
			Assert(pollRet >= 0);
			if (pollRet == 0)
				ELOG_DISPATCHER_DEBUG("cdbdisp_waitDispatchFinish_async(): Dispatch poll timeout after %d ms", DISPATCH_POLL_TIMEOUT);
		}
		while (pollRet == 0);
	}
	pfree(revents);
	pfree(added);
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
							 int sliceIndex)
{
	int			i;

	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;

	/*
	 * Start the dispatching
	 */
	for (i = 0; i < gp->size; i++)
	{
		CdbDispatchResult *qeResult;

		SegmentDatabaseDescriptor *segdbDesc = gp->db_descriptors[i];

		Assert(segdbDesc != NULL);

		/*
		 * Initialize the QE's CdbDispatchResult object.
		 */
		qeResult = cdbdisp_makeResult(ds->primaryResults, segdbDesc, sliceIndex);
		if (qeResult == NULL)
		{
			elog(FATAL, "could not allocate resources for segworker communication");
		}
		pParms->dispatchResultPtrArray[pParms->dispatchCount++] = qeResult;

		dispatchCommand(qeResult, pParms->query_text, pParms->query_text_len);
	}
}

/*
 * Check the specified acknowledge messages from QEs.
 *
 * Check all dispatch connections to get expected acknowledge message.
 * Return true if all required QEs' acknowledge messages have been received.
 *
 * message: specifies the expected ACK message to check.
 * timeout_sec: the second that the dispatcher waits for the ack messages at most.
 *              0 means checking immediately, and -1 means waiting until all ack
 *              messages are received.
 */
static bool
cdbdisp_checkAckMessage_async(struct CdbDispatcherState *ds, const char *message,
							  int timeout_sec)
{
	DispatchWaitMode prevWaitMode;
	CdbDispatchCmdAsync *pParms;
	bool receivedAll = true;

	Assert(ds);

	pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;
	/* If cdbdisp_destroyDispatcherState is called */
	if (pParms == NULL || message == NULL)
		return false;

	pParms->ackMessage = message;
	prevWaitMode = pParms->waitMode;
	pParms->waitMode = DISPATCH_WAIT_ACK_ROOT;

	for (int i = 0; i < pParms->dispatchCount; i++)
		pParms->dispatchResultPtrArray[i]->receivedAckMsg = false;

	checkDispatchResult(ds, timeout_sec);

	for (int i = 0; i < pParms->dispatchCount; i++)
	{
		if (!pParms->dispatchResultPtrArray[i]->receivedAckMsg &&
			pParms->dispatchResultPtrArray[i]->stillRunning)
		{
			receivedAll = false;
			break;
		}
	}

	pParms->waitMode = prevWaitMode;
	pParms->ackMessage = NULL;

	return receivedAll;
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

	checkDispatchResult(ds, DISPATCH_WAIT_UNTIL_FINISH);
}

/*
 * Allocates memory for a CdbDispatchCmdAsync structure and do the initialization.
 *
 * Memory will be freed in function cdbdisp_destroyDispatcherState by deleting the
 * memory context.
 */
static void *
cdbdisp_makeDispatchParams_async(int maxSlices, int largestGangSize, char *queryText, int len)
{
	int			maxResults = maxSlices * largestGangSize;
	int			size = 0;

	CdbDispatchCmdAsync *pParms = palloc0(sizeof(CdbDispatchCmdAsync));

	size = maxResults * sizeof(CdbDispatchResult *);
	pParms->dispatchResultPtrArray = (CdbDispatchResult **) palloc0(size);
	pParms->dispatchCount = 0;
	pParms->waitMode = DISPATCH_WAIT_NONE;
	pParms->ackMessage = NULL;
	pParms->query_text = queryText;
	pParms->query_text_len = len;

	return (void *) pParms;
}

/*
 * Receive and process results from all running QEs.
 * timeout_sec: the second that the dispatcher waits for the ack messages at most.
 *              DISPATCH_NO_WAIT(0): return immediate when there's no more data.
 *              DISPATCH_WAIT_UNTIL_FINISH(-1): wait until all dispatch works are completed.
 */
static void
checkDispatchResult(CdbDispatcherState *ds, int timeout_sec)
{
	int			i;
	int			timeout = 0;
	bool		sentSignal = false;
	uint8 ftsVersion = 0;
	struct timeval start_ts, now;
	int64		diff_us;

	CdbDispatchCmdAsync *pParms = (CdbDispatchCmdAsync *) ds->dispatchParams;
	CdbDispatchResults *meleeResults = ds->primaryResults;
	SegmentDatabaseDescriptor *segdbDesc;
	CdbDispatchResult *dispatchResult;

	int 	db_count = pParms->dispatchCount;
	ResetWaitEventSet(&DispWaitSet, TopMemoryContext, db_count);
	int 	*added = palloc0(db_count * sizeof(int));
	WaitEvent *revents = palloc(sizeof(WaitEvent) * db_count);

	/*
	 * OK, we are finished submitting the command to the segdbs. Now, we have
	 * to wait for them to finish.
	 */
	gettimeofday(&start_ts, NULL);
	for (;;)
	{
		int			n;
		int			nfds = 0;
		int			ack_count = 0;
		PGconn		*conn;

		/*
		 * bail-out if we are dying. Once QD dies, QE will recognize it
		 * shortly anyway.
		 */
		if (proc_exit_inprogress)
			break;

		/*
		 * Current loop might last for the long time so check on interrupts.
		 */
		CHECK_FOR_INTERRUPTS();

		/*
		 * escalate waitMode to cancel if:
		 * - user cancel request has occurred,
		 * - or an error has been reported by any QE,
		 * - in case the caller wants cancelOnError
		 */
		if ((CancelRequested() || meleeResults->errcode) && meleeResults->cancelOnError)
			pParms->waitMode = DISPATCH_WAIT_CANCEL;

		/*
		 * Which QEs are still running and could send results to us?
		 */
		for (i = 0; i < db_count; i++)
		{
			dispatchResult = pParms->dispatchResultPtrArray[i];
			segdbDesc = dispatchResult->segdbDesc;
			conn = segdbDesc->conn;

			if (pParms->waitMode == DISPATCH_WAIT_ACK_ROOT &&
				checkAckMessage(dispatchResult, pParms->ackMessage))
			{
				ack_count++;
				continue;
			}

			/*
			 * Already finished with this QE?
			 */
			if (!dispatchResult->stillRunning)
				continue;

			Assert(!cdbconn_isBadConnection(segdbDesc));

			/*
			 * Flush out buffer in case some commands are not fully
			 * dispatched to QEs, this can prevent QD from polling
			 * on such QEs forever.
			 */
			if (conn->outCount > 0)
			{
				/*
				 * Don't error out here, let following poll() routine to
				 * handle it.
				 */
				if (pqFlush(conn) < 0)
					elog(LOG, "Failed flushing outbound data to %s:%s",
						 segdbDesc->whoami, PQerrorMessage(conn));
			}

#ifdef FAULT_INJECTOR
			/* inject invalid sock to simulate an pqFlush() error */
			static int saved_sock = -1;
			if (FaultInjector_InjectFaultIfSet("inject_invalid_sock_for_checkDispatchResult",
						DDLNotSpecified,
						"" /* databaseName */,
						"" /* tableName */) == FaultInjectorTypeSkip)
			{
				if (i == 0 && saved_sock == -1)
				{
					saved_sock = conn->sock;
					conn->sock = -1;
					strlcpy(conn->errorMessage.data, "inject invalid sock\n", conn->errorMessage.maxlen);
					conn->errorMessage.len = strlen(conn->errorMessage.data);
					i--;
				}
			}
#endif

			/*
			 * When the connection was broken, the previous pqFlush() set:
			 * 			sock = -1 and status = CONNECTION_BAD
			 * it will cause an infinite hang when poll() it later, so need to skip it here
			 */
			if (cdbconn_isBadConnection(segdbDesc))
			{
				elog(WARNING, "Connection (%s) is broken, PQerrorMessage:%s",
					segdbDesc->whoami, PQerrorMessage(conn));
				dispatchResult->stillRunning = false;
#ifdef FAULT_INJECTOR
				/* restore the saved sock */
				if (i == -1)
				{
					conn->sock = saved_sock;
					conn->errorMessage.data[0] = '\0';
					conn->errorMessage.len = 0;
					dispatchResult->stillRunning = true;
				}
#endif
				continue;
			}

			/* add segment sock to the waitset */
			if (!added[i])
			{
				int 	sock = PQsocket(conn);
				long 	ev_userdata = i; /* the index "i" as the event's userdata */
				Assert(sock >= 0);
				AddWaitEventToSet(DispWaitSet, WL_SOCKET_READABLE, sock, NULL, (void *)ev_userdata);
				added[i] = 1;
			}
			nfds++;
		}

		/*
		 * Break out when no QEs still running or required QEs acked.
		 */
		if (nfds <= 0 ||
			(pParms->waitMode == DISPATCH_WAIT_ACK_ROOT && ack_count == ds->rootGangSize))
			break;

		/*
		 * Wait for results from QEs
		 *
		 * Don't wait if: - this is called from interconnect to check if
		 * there's any error.
		 *
		 * Lower the timeout if: - we need send signal to QEs.
		 */
		if (timeout_sec == 0)
			timeout = 0;
		else if (pParms->waitMode == DISPATCH_WAIT_NONE ||
				 pParms->waitMode == DISPATCH_WAIT_ACK_ROOT ||
				 sentSignal)
			timeout = DISPATCH_WAIT_TIMEOUT_MSEC;
		else
			timeout = DISPATCH_WAIT_CANCEL_TIMEOUT_MSEC;

		n = WaitEventSetWait(DispWaitSet, timeout, revents, db_count, WAIT_EVENT_DISP_RESULT);

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

			/*
			 * Since an error was detected for the segment, request
			 * FTS to perform a probe before checking the segment
			 * state.
			 */
			FtsNotifyProber();
			checkSegmentAlive(pParms);

			if (pParms->waitMode != DISPATCH_WAIT_NONE &&
				pParms->waitMode != DISPATCH_WAIT_ACK_ROOT)
			{
				signalQEs(pParms);
				sentSignal = true;
			}

			gettimeofday(&now, NULL);
			diff_us = (now.tv_sec - start_ts.tv_sec) * 1000000;
			diff_us += (int) now.tv_usec - (int) start_ts.tv_usec;
			if (timeout_sec >= 0 && diff_us >= timeout_sec * 1000000L)
				break;
		}
		/* If the time limit expires, poll() returns 0 */
		else if (n == 0)
		{
			if (pParms->waitMode != DISPATCH_WAIT_NONE &&
				pParms->waitMode != DISPATCH_WAIT_ACK_ROOT)
			{
				signalQEs(pParms);
				sentSignal = true;
			}

			/*
			 * This code relies on FTS being triggered at regular
			 * intervals. Iff FTS detects change in configuration
			 * then check segment state. FTS probe is not triggered
			 * explicitly in this case because this happens every
			 * DISPATCH_WAIT_TIMEOUT_MSEC.
			 */
			if (ftsVersion == 0 || ftsVersion != getFtsVersion())
			{
				ftsVersion = getFtsVersion();
				checkSegmentAlive(pParms);
			}

			gettimeofday(&now, NULL);
			diff_us = (now.tv_sec - start_ts.tv_sec) * 1000000;
			diff_us += (int) now.tv_usec - (int) start_ts.tv_usec;
			if (timeout_sec >= 0 && diff_us >= timeout_sec * 1000000L)
				break;
		}
		/* We have data waiting on one or more of the connections. */
		else
			handlePollSuccess(pParms, revents, n);
	} /* for (;;) */

	pfree(revents);
	pfree(added);
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

	forwardQENotices();

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
 * Helper function to check whether specified acknowledge message has been
 * received.
 *
 * Check whether the current required acknowledge message is already received
 * in the ackPGNotifies queue.
 */
static bool
checkAckMessage(CdbDispatchResult *dispatchResult, const char *message)
{
	bool received = false;
	PGnotify* ackNotifies = (PGnotify *) dispatchResult->ackPGNotifies;

	if (!message)
		elog(ERROR, "Notify ACK message is required.");

	if (dispatchResult->receivedAckMsg)
		return true;

	while (ackNotifies)
	{
		if (strcmp(ackNotifies->extra, message) == 0)
		{
			received = true;
			dispatchResult->receivedAckMsg = true;
			break;
		}
		ackNotifies = ackNotifies->next;
	}
	return received;
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

		if (pParms->waitMode == DISPATCH_WAIT_ACK_ROOT &&
				 dispatchResult->receivedAckMsg)
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
	forwardQENotices();

	return;
}

/*
 * Receive and process results from QEs.
 */
static void
handlePollSuccess(CdbDispatchCmdAsync *pParms,
				  WaitEvent *revents, int nready)
{
	int			i = 0;

	/*
	 * We have data waiting on one or more of the connections.
	 */
	for (i = 0; i < nready; i++)
	{
		long pos = (long)(revents[i].user_data); /* original position */
		bool		finished;
		int			sock;
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[pos];
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

		/*
		 * Skip if already finished or didn't dispatch.
		 */
		if (!dispatchResult->stillRunning)
			continue;

		if (pParms->waitMode == DISPATCH_WAIT_ACK_ROOT &&
				 dispatchResult->receivedAckMsg)
			continue;

		ELOG_DISPATCHER_DEBUG("looking for results from %ld of %d (%s)",
							  pos + 1, pParms->dispatchCount, segdbDesc->whoami);

		sock = PQsocket(segdbDesc->conn);
		Assert(sock >= 0);
		Assert(sock == revents[i].fd);

		/*
		 * Skip this connection if it has no input available.
		if (!(fds[currentFdNumber++].revents & POLLIN))
			continue;
		 */

		ELOG_DISPATCHER_DEBUG("PQsocket says there are results from %ld of %d (%s)",
							  pos + 1, pParms->dispatchCount, segdbDesc->whoami);

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

			ELOG_DISPATCHER_DEBUG("processResults says we are finished with %ld of %d (%s)",
								  pos + 1, pParms->dispatchCount, segdbDesc->whoami);

			if (DEBUG1 >= log_min_messages)
			{
				char		msec_str[32];

				switch (check_log_duration(msec_str, false))
				{
					case 1:
					case 2:
						elog(LOG, "duration to dispatch result received from %ld (seg %d): %s ms",
							 pos + 1, dispatchResult->segdbDesc->segindex, msec_str);
						break;
				}
			}

			if (PQisBusy(dispatchResult->segdbDesc->conn))
				elog(DEBUG1, "did not receive query results on libpq connection %s",
					 dispatchResult->segdbDesc->whoami);
		}
		else
			ELOG_DISPATCHER_DEBUG("processResults says we have more to do with %ld of %d (%s)",
								  pos + 1, pParms->dispatchCount, segdbDesc->whoami);
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
			(pParms->waitMode == DISPATCH_WAIT_ACK_ROOT &&
			 dispatchResult->receivedAckMsg) ||
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
 */
static void
checkSegmentAlive(CdbDispatchCmdAsync *pParms)
{
	int			i;

	/*
	 * check the connection still valid
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

		if (FtsIsSegmentDown(segdbDesc->segment_database_info))
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
	}
}

static inline void
send_sequence_response(PGconn *conn, Oid oid, int64 last, int64 cached, int64 increment, bool overflow, bool error)
{
	if (pqPutMsgStart(SEQ_NEXTVAL_QUERY_RESPONSE, false, conn) < 0)
		elog(ERROR, "Failed to send sequence response: %s", PQerrorMessage(conn));
	pqPutInt(oid, 4, conn);
	pqPutInt(last >> 32, 4, conn);
	pqPutInt(last, 4, conn);
	pqPutInt(cached >> 32, 4, conn);
	pqPutInt(cached, 4, conn);
	pqPutInt(increment >> 32, 4, conn);
	pqPutInt(increment, 4, conn);
	pqPutc(overflow ? SEQ_NEXTVAL_TRUE : SEQ_NEXTVAL_FALSE, conn);
	pqPutc(error ? SEQ_NEXTVAL_TRUE : SEQ_NEXTVAL_FALSE, conn);
	if (pqPutMsgEnd(conn) < 0)
		elog(ERROR, "Failed to send sequence response: %s", PQerrorMessage(conn));
	if (pqFlush(conn) < 0)
		elog(ERROR, "Failed to send sequence response: %s", PQerrorMessage(conn));
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
	forwardQENotices();

	/*
	 * If we have received one or more complete messages, process them.
	 */
	while (!PQisBusy(segdbDesc->conn))
	{
		/* loop to call PQgetResult; won't block */
		PGresult   *pRes;
		ExecStatusType resultStatus;
		int			resultIndex;

		forwardQENotices();

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

		if (segdbDesc->conn->wrote_xlog)
		{
			MarkTopTransactionWriteXLogOnExecutor();

			/*
			 * Reset the worte_xlog here. Since if the received pgresult not process
			 * the xlog write message('x' message sends from QE in ReadyForQuery),
			 * the value may still refer to previous dispatch statement. Which may
			 * always mark current top transaction has wrote xlog on executor.
			 */
			segdbDesc->conn->wrote_xlog = false;
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
			 * COPY FROM - get the number of rows completed by QE
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

	forwardQENotices();

	PGnotify *qnotifies = PQnotifies(segdbDesc->conn);
	while(qnotifies && elog_geterrcode() == 0)
	{
		if (strcmp(qnotifies->relname, CDB_NOTIFY_NEXTVAL) == 0)
		{
			/*
			 * If there was nextval request then respond back on this libpq
			 * connection with the next value. Check and process nextval
			 * message only if QD has not already hit the error. Since QD could
			 * have hit the error while processing the previous nextval_qd()
			 * request itself and since full error handling is not complete yet
			 * (ex: releasing all the locks, etc.), shouldn't attempt to call
			 * nextval_qd() again.
			 */

			CHECK_FOR_INTERRUPTS();

			int64 last;
			int64 cached;
			int64 increment;
			bool overflow;
			Oid dbid;
			Oid seq_oid;

			if (sscanf(qnotifies->extra, "%u:%u", &dbid, &seq_oid) != 2)
				elog(ERROR, "invalid nextval message");

			if (dbid != MyDatabaseId)
				elog(ERROR, "nextval message database id:%u doesn't match my database id:%u",
					 dbid, MyDatabaseId);

			PG_TRY();
			{
				nextval_qd(seq_oid, &last, &cached, &increment, &overflow);
			}
			PG_CATCH();
			{
				send_sequence_response(segdbDesc->conn, seq_oid, last, cached, increment, overflow, true /* error */);
				PG_RE_THROW();
			}
			PG_END_TRY();
			/* respond back on this libpq connection with the next value */
			send_sequence_response(segdbDesc->conn, seq_oid, last, cached, increment, overflow, false /* error */);
		}
		else if (strcmp(qnotifies->relname, CDB_NOTIFY_ENDPOINT_ACK) == 0)
		{
			qnotifies->next = (struct pgNotify *) dispatchResult->ackPGNotifies;
			dispatchResult->ackPGNotifies = (struct PGnotify *) qnotifies;

			/* Don't free the notify here since it in queue now */
			qnotifies = NULL;
		}
		else
		{
			/* Got an unknown PGnotify, just record it in log */
			if (qnotifies->relname)
				elog(LOG, "got an unknown notify message : %s", qnotifies->relname);
		}

		if (qnotifies)
			PQfreemem(qnotifies);
		qnotifies = PQnotifies(segdbDesc->conn);
	}

	forwardQENotices();

	return false;				/* we must keep on monitoring this socket */
}
