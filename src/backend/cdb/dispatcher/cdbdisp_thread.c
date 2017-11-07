
/*-------------------------------------------------------------------------
 *
 * cdbdisp_thread.c
 *	  Functions for multi-thread implementation of dispatching
 *	  commands to QExecutors.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbdisp_thread.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include <pthread.h>
#include <limits.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "storage/ipc.h"		/* For proc_exit_inprogress */
#include "tcop/tcopprot.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_thread.h"
#include "cdb/cdbdispatchresult.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpq.h"
#include "miscadmin.h"
#include "port/atomics.h"

#ifndef _WIN32
#define mythread() ((unsigned long) pthread_self())
#else
#define mythread() ((unsigned long) pthread_self().p)
#endif

#define DISPATCH_WAIT_TIMEOUT_SEC 2



/*
 * Parameter structure for the DispatchCommand threads
 */
typedef struct DispatchCommandParms
{
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

	/*
	 * db_count: The number of segdbs that this thread is responsible for
	 * dispatching the command to. Equals the count of segdbDescPtrArray
	 * below.
	 */
	int			db_count;


	/*
	 * dispatchResultPtrArray: Array[0..db_count-1] of CdbDispatchResult* Each
	 * CdbDispatchResult object points to a SegmentDatabaseDescriptor that
	 * this thread is responsible for dispatching the command to.
	 */
	struct CdbDispatchResult **dispatchResultPtrArray;

	/*
	 * Depending on this mode, we may send query cancel or query finish
	 * message to QE while we are waiting it to complete.  NONE means we
	 * expect QE to complete without any instruction.
	 */
	volatile DispatchWaitMode waitMode;

	/*
	 * pollfd supports for libpq
	 */
	int			nfds;
	struct pollfd *fds;

	/*
	 * The pthread_t thread handle.
	 */
	pthread_t	thread;
	bool		thread_valid;

} DispatchCommandParms;

/*
 * Keeps state of all the dispatch command threads.
 */
typedef struct CdbDispatchCmdThreads
{
	struct DispatchCommandParms *dispatchCommandParmsAr;
	int			dispatchCommandParmsArSize;
	int			threadCount;
} CdbDispatchCmdThreads;

/*
 * Counter to indicate there are some dispatch threads running. This will
 * be incremented at the beginning of dispatch threads and decremented at
 * the end of them.
 */
static volatile int32 RunningThreadCount = 0;

static int	getMaxThreadsPerGang(void);

static bool shouldStillDispatchCommand(DispatchCommandParms *pParms,
						   CdbDispatchResult *dispatchResult);

static bool dispatchCommand(CdbDispatchResult *dispatchResult,
				const char *query_text, int query_text_len);

/* returns true if command complete */
static bool processResults(CdbDispatchResult *dispatchResult);

static void *thread_DispatchCommand(void *arg);
static void thread_DispatchOut(DispatchCommandParms *pParms);
static void thread_DispatchWait(DispatchCommandParms *pParms);

static void cdbdisp_checkCancel(DispatchCommandParms *pParms);
static void cdbdisp_checkSegmentDBAlive(DispatchCommandParms *pParms);
static void handlePollSuccess(DispatchCommandParms *pParms);

static void
			DecrementRunningCount(void *arg);

static void
			cdbdisp_waitThreads(void);

static bool
			cdbdisp_shouldCancel(struct CdbDispatcherState *ds);

static void *cdbdisp_makeDispatchThreads(int maxSlices, char *queryText, int queryTextLen);

static void CdbCheckDispatchResult_internal(struct CdbDispatcherState *ds,
								DispatchWaitMode waitMode);

static void cdbdisp_dispatchToGang_internal(struct CdbDispatcherState *ds,
								struct Gang *gp,
								int sliceIndex,
								CdbDispatchDirectDesc *dispDirect);

DispatcherInternalFuncs DispatcherSyncFuncs =
{
	cdbdisp_waitThreads,
	cdbdisp_shouldCancel,
	NULL,
	cdbdisp_makeDispatchThreads,
	CdbCheckDispatchResult_internal,
	cdbdisp_dispatchToGang_internal,
	NULL
};

/*
 * Initialize dispatcher thread parameters.
 *
 * return the number of threads needed for this gang.
 */
static int
cdbdisp_initDispatchParmsForGang(struct CdbDispatcherState *ds,
								 struct Gang *gp,
								 int sliceIndex,
								 CdbDispatchDirectDesc *disp_direct)
{
	CdbDispatchCmdThreads *pThreads = NULL;
	int			segdbsToDispatch = 0;
	int			i = 0;


	Assert(ds->dispatchParams != NULL);
	pThreads = (CdbDispatchCmdThreads *) ds->dispatchParams;

	/*
	 * Create the thread parms structures based targetSet parameter. This will
	 * add the segdbDesc pointers appropriate to the targetSet into the thread
	 * Parms structures, making sure that each thread handles
	 * gp_connections_per_thread segdbs.
	 */
	for (i = 0; i < gp->size; i++)
	{
		CdbDispatchResult *qeResult = NULL;
		DispatchCommandParms *pParms = NULL;
		int			parmsIndex = 0;
		SegmentDatabaseDescriptor *segdbDesc = &gp->db_descriptors[i];

		Assert(segdbDesc != NULL);

		if (disp_direct->directed_dispatch)
		{
			/* currently we allow direct-to-one dispatch, only */
			Assert(disp_direct->count == 1);
			if (disp_direct->content[0] != segdbDesc->segindex)
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

		parmsIndex = segdbsToDispatch / gp_connections_per_thread;
		pParms = pThreads->dispatchCommandParmsAr + pThreads->threadCount + parmsIndex;
		pParms->dispatchResultPtrArray[pParms->db_count++] = qeResult;

		/*
		 * This CdbDispatchResult/SegmentDatabaseDescriptor pair will be
		 * dispatched and monitored by a thread to be started below. Only that
		 * thread should touch them until the thread is finished with them and
		 * resets the stillRunning flag. Caller must CdbCheckDispatchResult()
		 * to wait for completion.
		 */
		qeResult->stillRunning = true;
		segdbsToDispatch++;
	}

	/*
	 * Compute the thread count based on how many segdbs were added into the
	 * thread pool, knowing that each thread handles gp_connections_per_thread
	 * segdbs.
	 */
	Assert(segdbsToDispatch > 0);
	return 1 + (segdbsToDispatch - 1) / gp_connections_per_thread;
}

void
cdbdisp_dispatchToGang_internal(struct CdbDispatcherState *ds,
								struct Gang *gp,
								int sliceIndex,
								CdbDispatchDirectDesc *disp_direct)
{
	int			i = 0;
	CdbDispatchCmdThreads *pThreads = (CdbDispatchCmdThreads *) ds->dispatchParams;
	int			threadStartIndex = pThreads->threadCount;
	int			newThreads = cdbdisp_initDispatchParmsForGang(ds, gp, sliceIndex, disp_direct);

	/*
	 * Create the threads. (which also starts the dispatching).
	 */
	for (i = 0; i < newThreads; i++)
	{
		int			pthread_err = 0;
		DispatchCommandParms *pParms = pThreads->dispatchCommandParmsAr + threadStartIndex + i;

		Assert(pParms != NULL);

		pParms->thread_valid = true;

		pthread_err = gp_pthread_create(&pParms->thread, thread_DispatchCommand, pParms, "dispatchToGang");
		if (pthread_err != 0)
		{
			int			j;

			pParms->thread_valid = false;

			/*
			 * Error during thread create (this should be caused by resource
			 * constraints). If we leave the threads running, they'll
			 * immediately have some problems -- so we need to join them, and
			 * *then* we can issue our FATAL error
			 */
			pParms->waitMode = DISPATCH_WAIT_CANCEL;

			for (j = 0; j < threadStartIndex + i; j++)
			{
				DispatchCommandParms *pParms;

				pParms = &pThreads->dispatchCommandParmsAr[j];

				pParms->waitMode = DISPATCH_WAIT_CANCEL;
				pParms->thread_valid = false;
				pthread_join(pParms->thread, NULL);
			}

			ereport(FATAL, (errcode(ERRCODE_INTERNAL_ERROR),
							errmsg("could not create thread %d of %d", i + 1, newThreads),
							errdetail("pthread_create() failed with err %d", pthread_err)));
		}
		pThreads->threadCount++;
	}

	ELOG_DISPATCHER_DEBUG("dispatchToGang: Total threads now %d", pThreads->threadCount);
}

void
CdbCheckDispatchResult_internal(struct CdbDispatcherState *ds,
								DispatchWaitMode waitMode)
{
	CdbDispatchCmdThreads *pThreads = NULL;
	int			i;
	int			j;
	int			threadCount;

	Assert(ds != NULL);
	pThreads = (CdbDispatchCmdThreads *) ds->dispatchParams;

	/*
	 * No-op if no work was dispatched since the last time we were called.
	 */
	if (pThreads == NULL || pThreads->threadCount == 0)
	{
		ELOG_DISPATCHER_DEBUG("CheckDispatchResult: no threads active");
		return;
	}

	/*
	 * Wait for threads to finish.
	 */

	threadCount = pThreads->threadCount;
	for (i = 0; i < threadCount; i++)
	{
		DispatchCommandParms *pParms = &pThreads->dispatchCommandParmsAr[i];

		Assert(pParms != NULL);

		/*
		 * Don't overwrite DISPATCH_WAIT_CANCEL or DISPATCH_WAIT_FINISH with
		 * DISPATCH_WAIT_NONE
		 */
		if (waitMode != DISPATCH_WAIT_NONE)
			pParms->waitMode = waitMode;

		ELOG_DISPATCHER_DEBUG("CheckDispatchResult: Joining to thread %d of %d", i + 1, threadCount);

		if (pParms->thread_valid)
		{
			int			pthread_err = 0;

			pthread_err = pthread_join(pParms->thread, NULL);
			if (pthread_err != 0)
				elog(FATAL,
					 "CheckDispatchResult: pthread_join failed on thread %d (%lu) of %d (returned %d attempting to join to %lu)",
					 i + 1,
#ifndef _WIN32
					 (unsigned long) pParms->thread,
#else
					 (unsigned long) pParms->thread.p,
#endif
					 pThreads->threadCount, pthread_err,
					 (unsigned long) mythread());
		}

		HOLD_INTERRUPTS();
		pParms->thread_valid = false;
		MemSet(&pParms->thread, 0, sizeof(pParms->thread));
		RESUME_INTERRUPTS();

		/*
		 * Examine the CdbDispatchResult objects containing the results from
		 * this thread's QEs.
		 */
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
		{
			for (j = 0; j < pParms->db_count; j++)
			{
				CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[j];

				cdbdisp_debugDispatchResult(dispatchResult);
			}
		}

		pThreads->threadCount--;
	}

	Assert(pThreads->threadCount == 0);

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
 * Synchronize threads to finish for this process to die. Dispatching
 * threads need to acknowledge that we are dying, otherwise the main
 * thread will cleanup memory contexts which could cause process crash
 * while the threads are touching stale pointers. Threads will check
 * proc_exit_inprogress and immediately stops once it's found to be true.
 */
void
cdbdisp_waitThreads(void)
{
	int			i,
				max_retry;
	long		interval = 10 * 1000;	/* 10 msec */

	/*
	 * Just in case to avoid to be stuck in the final stage of process
	 * lifecycle, insure by setting time limit. If it exceeds, it probably
	 * means some threads are stuck and not progressing, in which case we can
	 * go ahead and cleanup things anyway. The duration should be longer than
	 * the select timeout in thread_DispatchWait.
	 */
	max_retry = (DISPATCH_WAIT_TIMEOUT_SEC + 10) * 1000000L / interval;

	/*
	 * This is supposed to be called after the flag is set.
	 */
	Assert(proc_exit_inprogress);

	for (i = 0; i < max_retry; i++)
	{
		if (RunningThreadCount == 0)
			break;
		pg_usleep(interval);
	}
}

/*
 * cdbdisp_makeDispatchThreads:
 * Allocates memory for a CdbDispatchCmdThreads structure and the memory
 * needed inside. Do the initialization.
 * Will be freed in function cdbdisp_destroyDispatcherState by deleting the
 * memory context.
 */
void *
cdbdisp_makeDispatchThreads(int maxSlices, char *queryText, int queryTextLen)
{
	int			maxThreadsPerGang = getMaxThreadsPerGang();
	int			maxThreads = maxThreadsPerGang * maxSlices;

	int			maxConn = gp_connections_per_thread;
	int			size = 0;
	int			i = 0;
	CdbDispatchCmdThreads *dThreads = palloc0(sizeof(*dThreads));

	size = maxThreads * sizeof(DispatchCommandParms);
	dThreads->dispatchCommandParmsAr = (DispatchCommandParms *) palloc0(size);
	dThreads->dispatchCommandParmsArSize = maxThreads;
	dThreads->threadCount = 0;

	for (i = 0; i < maxThreads; i++)
	{
		DispatchCommandParms *pParms = &dThreads->dispatchCommandParmsAr[i];

		pParms->nfds = maxConn;
		MemSet(&pParms->thread, 0, sizeof(pthread_t));

		size = maxConn * sizeof(CdbDispatchResult *);
		pParms->dispatchResultPtrArray = (CdbDispatchResult **) palloc0(size);

		size = sizeof(struct pollfd) * maxConn;
		pParms->fds = (struct pollfd *) palloc0(size);
		pParms->query_text = queryText;
		pParms->query_text_len = queryTextLen;
	}

	return (void *) dThreads;
}

/*
 * Dispatch the command to all segment DBs.
 */
static void
thread_DispatchOut(DispatchCommandParms *pParms)
{
	CdbDispatchResult *dispatchResult;
	int			i,
				db_count = pParms->db_count;

	/*
	 * The pParms contains an array of SegmentDatabaseDescriptors to send
	 * commands through to.
	 */
	for (i = 0; i < db_count; i++)
	{
		dispatchResult = pParms->dispatchResultPtrArray[i];

		Assert(dispatchResult->segdbDesc != NULL &&
			   dispatchResult->segdbDesc->conn != NULL);
		WRITE_LOG_DISPATCHER_DEBUG("thread_DispatchOut working on %d of %d commands. asyncStatus %d",
								   i + 1, db_count, dispatchResult->segdbDesc->conn->asyncStatus);

		dispatchResult->hasDispatched = false;
		dispatchResult->sentSignal = DISPATCH_WAIT_NONE;
		dispatchResult->wasCanceled = false;

		if (!shouldStillDispatchCommand(pParms, dispatchResult))
		{
			/*
			 * Don't dispatch if cancellation pending or no connection.
			 */
			dispatchResult->stillRunning = false;
			if (PQisBusy(dispatchResult->segdbDesc->conn))
				write_log("We thought we were done, because !shouldStillDispatchCommand(), "
						  "but libpq says we are still busy");
			continue;
		}

		/*
		 * Kick off the command over the libpq connection. If unsuccessful,
		 * proceed anyway, and check for lost connection below.
		 */
		if (dispatchCommand(dispatchResult, pParms->query_text, pParms->query_text_len))
		{
			dispatchResult->hasDispatched = true;

			/*
			 * We'll keep monitoring this QE -- whether or not the command was
			 * dispatched -- in order to check for a lost connection or any
			 * other errors that libpq might have in store for us.
			 */
			WRITE_LOG_DISPATCHER_DEBUG("Command dispatched to segment db (%s)",
									   dispatchResult->segdbDesc->whoami);
		}
		else
		{
			dispatchResult->stillRunning = false;
			write_log("Command could not be sent to segment db (%s)",
					  dispatchResult->segdbDesc->whoami);
		}
	}
}

/*
 * Loops all still running connections, receive and process results
 * from each connection.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *       thread_DispatchCommand absolutely no elog'ing.
 */
static void
handlePollSuccess(DispatchCommandParms *pParms)
{
	int			cur_fds_num = 0;
	int			i = 0;

	/*
	 * We have data waiting on one or more of the connections.
	 */
	for (i = 0; i < pParms->db_count; i++)
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

		WRITE_LOG_DISPATCHER_DEBUG("looking for results from %d of %d (%s)",
								   i + 1, pParms->db_count, segdbDesc->whoami);

		sock = PQsocket(segdbDesc->conn);
		Assert(sock >= 0);
		Assert(sock == pParms->fds[cur_fds_num].fd);

		/*
		 * Skip this connection if it has no input available.
		 */
		if (!(pParms->fds[cur_fds_num++].revents & POLLIN))
			continue;

		WRITE_LOG_DISPATCHER_DEBUG("PQsocket says there are results from  %d of %d (%s)",
								   i + 1, pParms->db_count, segdbDesc->whoami);

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

			WRITE_LOG_DISPATCHER_DEBUG("processResults says we are finished with %d of %d (%s)",
									   i + 1, pParms->db_count, segdbDesc->whoami);

			if (DEBUG1 >= log_min_messages)
			{
				char		msec_str[32];

				switch (check_log_duration(msec_str, false))
				{
					case 1:
					case 2:
						write_log("duration to dispatch result received from thread %d (seg %d): %s ms",
								  i + 1, dispatchResult->segdbDesc->segindex, msec_str);
						break;
				}
			}

			if (PQisBusy(dispatchResult->segdbDesc->conn))
				write_log("We thought we were done, because finished==true, but libpq says we are still busy");
		}
		else
		{
			WRITE_LOG_DISPATCHER_DEBUG("processResults says we have more to do with %d of %d (%s)",
									   i + 1, pParms->db_count, segdbDesc->whoami);
		}
	}
}

static void
thread_DispatchWait(DispatchCommandParms *pParms)
{
	int			timeoutCounter = 0;

	/*
	 * OK, we are finished submitting the command to the segdbs. Now, we have
	 * to wait for them to finish.
	 */
	for (;;)
	{
		int			i;
		int			n = 0;
		int			nfds = 0;
		int			db_count = pParms->db_count;

		/*
		 * Which QEs are still running and could send results to us?
		 */
		for (i = 0; i < db_count; i++)
		{
			int			sock;
			CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];
			SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

			/*
			 * Already finished with this QE?
			 */
			if (!dispatchResult->stillRunning)
				continue;

			if (cdbconn_isBadConnection(segdbDesc))
			{
				char	   *msg = PQerrorMessage(segdbDesc->conn);

				cdbdisp_appendMessage(dispatchResult, LOG,
									  "Lost connection to %s: %s.",
									  segdbDesc->whoami, msg ? msg : "unknown error");
				dispatchResult->stillRunning = false;
				continue;
			}

			/*
			 * Add socket to fd_set if still connected.
			 */
			sock = PQsocket(segdbDesc->conn);
			Assert(sock >= 0);
			pParms->fds[nfds].fd = sock;
			pParms->fds[nfds].events = POLLIN;
			nfds++;
			Assert(nfds <= pParms->nfds);
		}

		/*
		 * Break out when no QEs still running.
		 */
		if (nfds <= 0)
			break;

		/*
		 * bail-out if we are dying. We should not do much of cleanup as the
		 * main thread is waiting on this thread to finish.	Once QD dies, QE
		 * will recognize it shortly anyway.
		 */
		if (proc_exit_inprogress)
			break;

		/*
		 * Wait for results from QEs. Block here until input is available.
		 */
		n = poll(pParms->fds, nfds, DISPATCH_WAIT_TIMEOUT_SEC * 1000);

		/*
		 * poll returns with an error, including one due to an interrupted
		 * call
		 */
		if (n < 0)
		{
			int			sock_errno = SOCK_ERRNO;

			if (sock_errno == EINTR)
				continue;

			write_log("handlePollError poll() failed; errno=%d", sock_errno);

			cdbdisp_checkCancel(pParms);
			cdbdisp_checkSegmentDBAlive(pParms);
		}
		/* If the time limit expires, poll() returns 0 */
		else if (n == 0)
		{
			cdbdisp_checkCancel(pParms);
			if (timeoutCounter++ > 30)
			{
				cdbdisp_checkSegmentDBAlive(pParms);
				timeoutCounter = 0;
			}
		}
		/* We have data waiting on one or more of the connections. */
		else
			handlePollSuccess(pParms);
	}
}

/*
 * Cleanup routine for the dispatching thread.	This will indicate the thread
 * is not running any longer.
 */
static void
DecrementRunningCount(void *arg)
{
	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) &RunningThreadCount, 1);
}

/*
 * thread_DispatchCommand is the thread proc used to dispatch the command to one or more of the qExecs.
 *
 * NOTE: This function MUST NOT contain elog or ereport statements. (or most any other backend code)
 *		 elog is NOT thread-safe. Developers should instead use something like:
 *
 * NOTE: In threads, we cannot use palloc, because it's not thread safe.
 */
static void *
thread_DispatchCommand(void *arg)
{
	DispatchCommandParms *pParms = (DispatchCommandParms *) arg;

	gp_set_thread_sigmasks();

	/*
	 * Mark that we are runnig a new thread. The main thread will check it to
	 * see if there is still alive one. Let's do this after we block signals
	 * so that nobody will intervent and mess up the value. (should we
	 * actually block signals before spawning a thread, as much like we do in
	 * fork??)
	 */
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &RunningThreadCount, 1);

	/*
	 * We need to make sure the value will be decremented once the thread
	 * finishes. Currently there is not such case but potentially we could
	 * have pthread_exit or thread cancellation in the middle of code, in
	 * which case we would miss to decrement value if we tried to do this
	 * without the cleanup callback facility.
	 */
	pthread_cleanup_push(DecrementRunningCount, NULL);
	{
		thread_DispatchOut(pParms);
		thread_DispatchWait(pParms);
	}
	pthread_cleanup_pop(1);

	return (NULL);
}

/*
 * Helper function to thread_DispatchCommand that actually kicks off the
 * command on the libpq connection.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *		 thread_DispatchCommand absolutely no elog'ing.
 */
static bool
dispatchCommand(CdbDispatchResult *dispatchResult,
				const char *query_text, int query_text_len)
{
	TimestampTz beforeSend = 0;
	long		secs;
	int			usecs;

	if (DEBUG1 >= log_min_messages)
		beforeSend = GetCurrentTimestamp();

	/*
	 * Submit the command asynchronously.
	 */
	if (PQsendGpQuery_shared(dispatchResult->segdbDesc->conn, (char *) query_text, query_text_len, false) == 0)
		return false;

	if (DEBUG1 >= log_min_messages)
	{
		TimestampDifference(beforeSend, GetCurrentTimestamp(), &secs, &usecs);

		if (secs != 0 || usecs > 1000)	/* Time > 1ms? */
			write_log("time for PQsendGpQuery_shared %ld.%06d", secs, usecs);
	}

	return true;
}

/*
 * Helper function to thread_DispatchCommand that handles timeouts and
 * error that occur during the poll() call.
 *
 * This function loops all still running connections and send cancel or
 * finish signal if needed.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *       thread_DispatchCommand absolutely no elog'ing.
 */
static void
cdbdisp_checkCancel(DispatchCommandParms *pParms)
{
	int			i;

	/*
	 * Are there any QEs that should be canceled?
	 */
	for (i = 0; i < pParms->db_count; i++)
	{
		DispatchWaitMode waitMode;
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];

		Assert(dispatchResult != NULL);
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
		CdbDispatchResults *meleeResults = dispatchResult->meleeResults;

		/*
		 * Already finished with this QE?
		 */
		if (!dispatchResult->stillRunning)
			continue;

		waitMode = DISPATCH_WAIT_NONE;

		/*
		 * Send query finish to this QE if QD is already done.
		 */
		if (pParms->waitMode == DISPATCH_WAIT_FINISH)
			waitMode = DISPATCH_WAIT_FINISH;

		/*
		 * However, escalate it to cancel if: - user interrupt has occurred, -
		 * or I'm told to send cancel, - or an error has been reported by
		 * another QE, - in case the caller wants cancelOnError and it was not
		 * canceled
		 */
		if ((InterruptPending || pParms->waitMode == DISPATCH_WAIT_CANCEL || meleeResults->errcode) &&
			(meleeResults->cancelOnError && !dispatchResult->wasCanceled))
			waitMode = DISPATCH_WAIT_CANCEL;

		/*
		 * Finally, don't send the signal if - no action needed (NONE) - the
		 * signal was already sent - connection is dead
		 */
		if (waitMode != DISPATCH_WAIT_NONE &&
			waitMode != dispatchResult->sentSignal &&
			!cdbconn_isBadConnection(segdbDesc))
		{
			char		errbuf[256];
			bool		sent;

			memset(errbuf, 0, sizeof(errbuf));

			if (Debug_cancel_print || gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
				write_log("Calling PQcancel for %s", segdbDesc->whoami);

			sent = cdbconn_signalQE(segdbDesc, errbuf, waitMode == DISPATCH_WAIT_CANCEL);
			if (sent)
				dispatchResult->sentSignal = waitMode;
			else
				write_log("Unable to cancel: %s", strlen(errbuf) == 0 ? "cannot allocate PGCancel" : errbuf);
		}
	}
}

/*
 * Helper function to thread_DispatchCommand that handles timeouts and
 * error that occur during the poll() call.
 *
 * This function loops check all still running connections to see if the
 * segment DB is still alive.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *       thread_DispatchCommand absolutely no elog'ing.
 */
static void
cdbdisp_checkSegmentDBAlive(DispatchCommandParms *pParms)
{
	int			i;

	/*
	 * check the connection still valid, set 1 min time interval this may
	 * affect performance, should turn it off if required.
	 */
	for (i = 0; i < pParms->db_count; i++)
	{
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];
		SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;

		WRITE_LOG_DISPATCHER_DEBUG("checking status %d of %d %s stillRunning %d",
								   i + 1, pParms->db_count, segdbDesc->whoami, dispatchResult->stillRunning);

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

		WRITE_LOG_DISPATCHER_DEBUG("testing connection %d of %d %s stillRunning %d",
								   i + 1, pParms->db_count, segdbDesc->whoami, dispatchResult->stillRunning);

		if (!FtsTestConnection(segdbDesc->segment_database_info, false))
		{
			cdbdisp_appendMessage(dispatchResult, LOG,
								  "Lost connection to %s. FTS detected segment failures.",
								  segdbDesc->whoami);

			dispatchResult->stillRunning = false;
		}
	}
}

static int
getMaxThreadsPerGang(void)
{
	return 1 + (largestGangsize() - 1) / gp_connections_per_thread;
}

/*
 * Helper function to thread_DispatchCommand that decides if we should dispatch
 * to this segment database.
 *
 * (1) don't dispatch if there is already a query cancel notice pending.
 * (2) make sure our libpq connection is still good.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *		 thread_DispatchCommand absolutely no elog'ing.
 */
static bool
shouldStillDispatchCommand(DispatchCommandParms *pParms,
						   CdbDispatchResult *dispatchResult)
{
	SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
	CdbDispatchResults *gangResults = dispatchResult->meleeResults;

	if (cdbconn_isBadConnection(segdbDesc))
	{
		char	   *msg = PQerrorMessage(segdbDesc->conn);

		/*
		 * Save error info for later.
		 */
		cdbdisp_appendMessage(dispatchResult, LOG,
							  "Lost connection to %s.  %s",
							  segdbDesc->whoami, msg ? msg : "");

		return false;
	}

	/*
	 * Don't submit if already encountered an error. The error has already
	 * been noted, so just keep quiet.
	 */
	if ((pParms->waitMode == DISPATCH_WAIT_CANCEL || gangResults->errcode) &&
		gangResults->cancelOnError)
	{
		dispatchResult->wasCanceled = true;
		if (Debug_cancel_print || gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			write_log("Error cleanup in progress; command not sent to %s",
					  segdbDesc->whoami);
		return false;
	}

	/*
	 * Don't submit if client told us to cancel. The cancellation request has
	 * already been noted, so hush.
	 */
	if (InterruptPending && gangResults->cancelOnError)
	{
		dispatchResult->wasCanceled = true;
		if (Debug_cancel_print || gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			write_log("Cancellation request pending; command not sent to %s",
					  segdbDesc->whoami);
		return false;
	}

	return true;
}

/*
 * Receive and process results from QE.
 *
 * Return true if the command is completed or the connection is gone.
 */
static bool
processResults(CdbDispatchResult *dispatchResult)
{
	SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
	char	   *msg;
	int			rc;

	/*
	 * PQisBusy() has side-effects
	 */
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
	{
		write_log("processResults. isBusy = %d", PQisBusy(segdbDesc->conn));

		if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
			goto connection_error;
	}

	/*
	 * Receive input from QE.
	 */
	rc = PQconsumeInput(segdbDesc->conn);

	/*
	 * If PQconsumeInput fails, we're hosed.
	 */
	if (rc == 0)
	{							/* handle PQconsumeInput error */
		goto connection_error;
	}

	/*
	 * PQisBusy() has side-effects
	 */
	if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG && PQisBusy(segdbDesc->conn))
		write_log("PQisBusy");

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
		if (PQstatus(segdbDesc->conn) == CONNECTION_BAD
			|| segdbDesc->conn->sock == -1)
		{
			goto connection_error;
		}

		resultIndex = cdbdisp_numPGresult(dispatchResult);

		WRITE_LOG_DISPATCHER_DEBUG("PQgetResult");

		/*
		 * Get one message.
		 */
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
			WRITE_LOG_DISPATCHER_DEBUG("%s -> idle", segdbDesc->whoami);

			/* this is normal end of command */
			return true;
		}						/* end of results */


		/*
		 * Attach the PGresult object to the CdbDispatchResult object.
		 */
		cdbdisp_appendResult(dispatchResult, pRes);

		/*
		 * Did a command complete successfully?
		 */
		resultStatus = PQresultStatus(pRes);
		if (resultStatus == PGRES_COMMAND_OK ||
			resultStatus == PGRES_TUPLES_OK ||
			resultStatus == PGRES_COPY_IN || resultStatus == PGRES_COPY_OUT)
		{

			/*
			 * Save the index of the last successful PGresult. Can be given to
			 * cdbdisp_getPGresult() to get tuple count, etc.
			 */
			dispatchResult->okindex = resultIndex;

			WRITE_LOG_DISPATCHER_DEBUG("%s -> ok %s", segdbDesc->whoami,
									   PQcmdStatus(pRes) ? PQcmdStatus(pRes) : "(no cmdStatus)");

			/*
			 * SREH - get number of rows rejected from QE if any
			 */
			if (pRes->numRejected > 0)
				dispatchResult->numrowsrejected += pRes->numRejected;

			/*
			 * COPY FROM ON SEGMENT - get number of rows completed by QE if
			 * any
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

			WRITE_LOG_DISPATCHER_DEBUG("%s -> %s %s  %s",
									   segdbDesc->whoami,
									   PQresStatus(resultStatus),
									   sqlstate ? sqlstate : "(no SQLSTATE)", msg ? msg : "");

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

connection_error:
	msg = PQerrorMessage(segdbDesc->conn);

	/*
	 * Save error info for later.
	 */
	cdbdisp_appendMessage(dispatchResult, LOG,
						  "Error on receive from %s: %s",
						  segdbDesc->whoami, msg ? msg : "unknown error");

	return true;				/* connection is gone! */
}

static bool
cdbdisp_shouldCancel(struct CdbDispatcherState *ds)
{
	Assert(ds);
	return cdbdisp_checkResultsErrcode(ds->primaryResults);
}
