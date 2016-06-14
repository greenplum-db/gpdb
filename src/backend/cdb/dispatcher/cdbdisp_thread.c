
/*-------------------------------------------------------------------------
 *
 * cdbdisp_thread.c
 *	  Functions for multi-thread implementation of dispatching
 *	  commands to QExecutors.
 *
 * Copyright (c) 2005-2008, Greenplum inc
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

#include "storage/ipc.h"		/* For proc_exit_inprogress  */
#include "tcop/tcopprot.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_thread.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/gp_atomic.h"

#ifndef _WIN32
#define mythread() ((unsigned long) pthread_self())
#else
#define mythread() ((unsigned long) pthread_self().p)
#endif

#define DISPATCH_WAIT_TIMEOUT_SEC 2

/*
 * Counter to indicate there are some dispatch threads running.  This will
 * be incremented at the beginning of dispatch threads and decremented at
 * the end of them.
 */
static volatile int32 RunningThreadCount = 0;

static int	getMaxThreadsPerGang(void);

static bool
shouldStillDispatchCommand(DispatchCommandParms * pParms,
						   CdbDispatchResult * dispatchResult);

static void
CollectQEWriterTransactionInformation(SegmentDatabaseDescriptor * segdbDesc,
									  CdbDispatchResult * dispatchResult);

static void
dispatchCommand(CdbDispatchResult * dispatchResult,
				const char *query_text, int query_text_len);

/* returns true if command complete */
static bool processResults(CdbDispatchResult * dispatchResult);

static DispatchWaitMode
cdbdisp_signalQE(SegmentDatabaseDescriptor * segdbDesc,
				 DispatchWaitMode waitMode);

static void *thread_DispatchCommand(void *arg);
static void thread_DispatchOut(DispatchCommandParms * pParms);
static void thread_DispatchWait(DispatchCommandParms * pParms);
static void thread_DispatchWaitSingle(DispatchCommandParms * pParms);

static void
handlePollError(DispatchCommandParms * pParms, int db_count, int sock_errno);

static void
handlePollTimeout(DispatchCommandParms * pParms,
				  int db_count, int *timeoutCounter, bool useSampling);

static void
DecrementRunningCount(void *arg);

void
cdbdisp_dispatchToGang_internal(struct CdbDispatcherState *ds,
								struct Gang *gp,
								int sliceIndex,
								CdbDispatchDirectDesc * disp_direct)
{
	struct CdbDispatchResults *dispatchResults = ds->primaryResults;
	SegmentDatabaseDescriptor *segdbDesc;
	int	i,
		max_threads,
		segdbs_in_thread_pool = 0,
		newThreads = 0;
	int	gangSize = 0;
	SegmentDatabaseDescriptor *db_descriptors;
	DispatchCommandParms *pParms = NULL;

	gangSize = gp->size;
	Assert(gangSize <= largestGangsize());
	db_descriptors = gp->db_descriptors;

	Assert(gp_connections_per_thread >= 0);
	Assert(ds->dispatchThreads != NULL);
	/*
	 * If we attempt to reallocate, there is a race here: we
	 * know that we have threads running using the
	 * dispatchCommandParamsAr! If we reallocate we
	 * potentially yank it out from under them! Don't do
	 * it!
	 */
	max_threads = getMaxThreadsPerGang();
	if (ds->dispatchThreads->dispatchCommandParmsArSize <
		(ds->dispatchThreads->threadCount + max_threads))
	{
		elog(ERROR,
			 "Attempted to reallocate dispatchCommandParmsAr while other threads still running size %d new threadcount %d",
			 ds->dispatchThreads->dispatchCommandParmsArSize,
			 ds->dispatchThreads->threadCount + max_threads);
	}

	/*
	 * Create the thread parms structures based targetSet parameter.
	 * This will add the segdbDesc pointers appropriate to the
	 * targetSet into the thread Parms structures, making sure that each thread
	 * handles gp_connections_per_thread segdbs.
	 */
	for (i = 0; i < gangSize; i++)
	{
		CdbDispatchResult *qeResult;

		segdbDesc = &db_descriptors[i];
		int	parmsIndex = 0;

		Assert(segdbDesc != NULL);

		if (disp_direct->directed_dispatch)
		{
			Assert(disp_direct->count == 1);	/* currently we allow direct-to-one dispatch, only */

			if (disp_direct->content[0] != segdbDesc->segindex)
				continue;
		}

		/*
		 * Initialize the QE's CdbDispatchResult object. 
		 */
		qeResult = cdbdisp_makeResult(dispatchResults, segdbDesc, sliceIndex);

		if (qeResult == NULL)
		{
			/*
			 * writer_gang could be NULL if this is an extended query.
			 */
			if (dispatchResults->writer_gang)
				dispatchResults->writer_gang->dispatcherActive = true;
			elog(FATAL, "could not allocate resources for segworker communication");
		}

		/*
		 * Transfer any connection errors from segdbDesc.
		 */
		if (segdbDesc->errcode || segdbDesc->error_message.len)
			cdbdisp_mergeConnectionErrors(qeResult, segdbDesc);

		parmsIndex = gp_connections_per_thread == 0 ? 0 : segdbs_in_thread_pool / gp_connections_per_thread;
		pParms = ds->dispatchThreads->dispatchCommandParmsAr + ds->dispatchThreads->threadCount + parmsIndex;
		pParms->dispatchResultPtrArray[pParms->db_count++] = qeResult;

		/*
		 * This CdbDispatchResult/SegmentDatabaseDescriptor pair will be
		 * dispatched and monitored by a thread to be started below. Only that
		 * thread should touch them until the thread is finished with them and
		 * resets the stillRunning flag. Caller must CdbCheckDispatchResult()
		 * to wait for completion.
		 */
		qeResult->stillRunning = true;

		segdbs_in_thread_pool++;
	}

	/*
	 * Compute the thread count based on how many segdbs were added into the
	 * thread pool, knowing that each thread handles gp_connections_per_thread
	 * segdbs.
	 */
	if (segdbs_in_thread_pool == 0)
		newThreads = 0;
	else if (gp_connections_per_thread == 0)
		newThreads = 1;
	else
		newThreads = 1 + (segdbs_in_thread_pool - 1) / gp_connections_per_thread;

	/*
	 * Create the threads. (which also starts the dispatching).
	 */
	for (i = 0; i < newThreads; i++)
	{
		DispatchCommandParms *pParms = &(ds->dispatchThreads->dispatchCommandParmsAr + ds->dispatchThreads->threadCount)[i];

		Assert(pParms != NULL);

		if (gp_connections_per_thread == 0)
		{
			Assert(newThreads <= 1);
			thread_DispatchOut(pParms);
		}
		else
		{
			int	pthread_err = 0;

			pParms->thread_valid = true;
			pthread_err = gp_pthread_create(&pParms->thread, thread_DispatchCommand, pParms, "dispatchToGang");

			if (pthread_err != 0)
			{
				int	j;

				pParms->thread_valid = false;

				/*
				 * Error during thread create (this should be caused by
				 * resource constraints). If we leave the threads running,
				 * they'll immediately have some problems -- so we need to
				 * join them, and *then* we can issue our FATAL error
				 */
				pParms->waitMode = DISPATCH_WAIT_CANCEL;

				for (j = 0; j < ds->dispatchThreads->threadCount + (i - 1); j++)
				{
					DispatchCommandParms *pParms;

					pParms = &ds->dispatchThreads->dispatchCommandParmsAr[j];

					pParms->waitMode = DISPATCH_WAIT_CANCEL;
					pParms->thread_valid = false;
					pthread_join(pParms->thread, NULL);
				}

				ereport(FATAL,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("could not create thread %d of %d", i + 1, newThreads),
						 errdetail ("pthread_create() failed with err %d", pthread_err)));
			}
		}

	}

	ds->dispatchThreads->threadCount += newThreads;
	elog(DEBUG4, "dispatchToGang: Total threads now %d",
		 ds->dispatchThreads->threadCount);
}

void
CdbCheckDispatchResult_internal(struct CdbDispatcherState *ds,
								struct SegmentDatabaseDescriptor ***failedSegDB,
								int *numOfFailed, DispatchWaitMode waitMode)
{
	int	i;
	int	j;
	int	nFailed = 0;
	DispatchCommandParms *pParms;
	CdbDispatchResult *dispatchResult;
	SegmentDatabaseDescriptor *segdbDesc;

	Assert(ds != NULL);

	if (failedSegDB)
		*failedSegDB = NULL;
	if (numOfFailed)
		*numOfFailed = 0;

	/*
	 * No-op if no work was dispatched since the last time we were called.
	 */
	if (!ds->dispatchThreads || ds->dispatchThreads->threadCount == 0)
	{
		elog(DEBUG5, "CheckDispatchResult: no threads active");
		return;
	}

	/*
	 * Wait for threads to finish.
	 */
	for (i = 0; i < ds->dispatchThreads->threadCount; i++)
	{
		pParms = &ds->dispatchThreads->dispatchCommandParmsAr[i];
		Assert(pParms != NULL);

		/*
		 * Does caller want to stop short?
		 */
		switch (waitMode)
		{
			case DISPATCH_WAIT_CANCEL:
			case DISPATCH_WAIT_FINISH:
				pParms->waitMode = waitMode;
				break;
			default:
				break;
		}

		if (gp_connections_per_thread == 0)
		{
			thread_DispatchWait(pParms);
		}
		else
		{
			elog(DEBUG4, "CheckDispatchResult: Joining to thread %d of %d",
				 i + 1, ds->dispatchThreads->threadCount);

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
						 ds->dispatchThreads->threadCount, pthread_err,
						 (unsigned long) mythread());
			}
		}
		HOLD_INTERRUPTS();
		pParms->thread_valid = false;
		MemSet(&pParms->thread, 0, sizeof(pParms->thread));
		RESUME_INTERRUPTS();

		/*
		 * Examine the CdbDispatchResult objects containing the results
		 * from this thread's QEs.
		 */
		for (j = 0; j < pParms->db_count; j++)
		{
			dispatchResult = pParms->dispatchResultPtrArray[j];

			if (dispatchResult == NULL)
			{
				elog(LOG, "CheckDispatchResult: result object is NULL ? skipping.");
				continue;
			}

			if (dispatchResult->segdbDesc == NULL)
			{
				elog(LOG, "CheckDispatchResult: result object segment descriptor is NULL ? skipping.");
				continue;
			}

			segdbDesc = dispatchResult->segdbDesc;

			/*
			 * segdbDesc error message is unlikely here, but check anyway.
			 */
			if (segdbDesc->errcode || segdbDesc->error_message.len)
				cdbdisp_mergeConnectionErrors(dispatchResult, segdbDesc);

			/*
			 * Log the result
			 */
			if (DEBUG2 >= log_min_messages)
				cdbdisp_debugDispatchResult(dispatchResult, DEBUG2, DEBUG3);

			/*
			 * Notify FTS to reconnect if connection lost or never connected.
			 */
			if (failedSegDB && PQstatus(segdbDesc->conn) == CONNECTION_BAD)
			{
				/*
				 * Allocate storage.  Caller should pfree() it.
				 */
				if (!*failedSegDB)
					*failedSegDB = palloc(sizeof(**failedSegDB) * (2 * getgpsegmentCount() + 1));

				/*
				 * Append to broken connection list.
				 */
				(*failedSegDB)[nFailed++] = segdbDesc;
				(*failedSegDB)[nFailed] = NULL;

				if (numOfFailed)
					*numOfFailed = nFailed;
			}

			/*
			 * Zap our SegmentDatabaseDescriptor ptr because it may be
			 * invalidated by the call to FtsHandleNetFailure() below.
			 * Anything we need from there, we should get before this.
			 */
			dispatchResult->segdbDesc = NULL;

		}
	}

	/*
	 * reset thread state (will be destroyed later on in finishCommand)
	 */
	ds->dispatchThreads->threadCount = 0;

	/*
	 * It looks like everything went fine, make sure we don't miss a
	 * user cancellation?
	 *
	 * The waitMode argument is NONE when we are doing "normal work".
	 */
	if (waitMode == DISPATCH_WAIT_NONE || waitMode == DISPATCH_WAIT_FINISH)
		CHECK_FOR_INTERRUPTS();
}

/*
 * Synchronize threads to finish for this process to die.  Dispatching
 * threads need to acknowledge that we are dying, otherwise the main
 * thread will cleanup memory contexts which could cause process crash
 * while the threads are touching stale pointers.  Threads will check
 * proc_exit_inprogress and immediately stops once it's found to be true.
 */
void
cdbdisp_waitThreads(void)
{
	int	i,
		max_retry;
	long interval = 10 * 1000;	/* 10 msec */

	/*
	 * Just in case to avoid to be stuck in the final stage of process
	 * lifecycle, insure by setting time limit.  If it exceeds, it probably
	 * means some threads are stuck and not progressing, in which case
	 * we can go ahead and cleanup things anyway.  The duration should be
	 * longer than the select timeout in thread_DispatchWait.
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
CdbDispatchCmdThreads *
cdbdisp_makeDispatchThreads(int maxSlices)
{
	int	maxThreadsPerGang = getMaxThreadsPerGang();

	/*
	 * the maximum number of command parameter blocks we'll possibly need is
	 * * one for each slice on the primary gang. Max sure that we
	 * * have enough -- once we've created the command block we're stuck with it
	 * * for the duration of this statement (including CDB-DTM ).
	 * * X 2 for good measure ?
	 */
	int	maxThreads = maxThreadsPerGang * 4 * Max(maxSlices, 5);

	int	maxConn = gp_connections_per_thread == 0 ? largestGangsize() : gp_connections_per_thread;
	int	size = 0;
	int	i = 0;
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
	}

	return dThreads;
}

static void
thread_DispatchOut(DispatchCommandParms * pParms)
{
	CdbDispatchResult *dispatchResult;
	int	i,
		db_count = pParms->db_count;

	/*
	 * The pParms contains an array of SegmentDatabaseDescriptors
	 * to send commands through to.
	 */
	for (i = 0; i < db_count; i++)
	{
		dispatchResult = pParms->dispatchResultPtrArray[i];

		/*
		 * Don't use elog, it's not thread-safe
		 */
		if (DEBUG5 >= log_min_messages)
		{
			if (dispatchResult->segdbDesc->conn)
			{
				write_log
					("thread_DispatchCommand working on %d of %d commands.  asyncStatus %d",
					 i + 1, db_count,
					 dispatchResult->segdbDesc->conn->asyncStatus);
			}
		}

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
				write_log
					(" We thought we were done, because !shouldStillDispatchCommand(), but libpq says we are still busy");
			if (PQstatus(dispatchResult->segdbDesc->conn) == CONNECTION_BAD)
				write_log
					(" We thought we were done, because !shouldStillDispatchCommand(), but libpq says the connection died?");
		}
		else
		{
			/*
			 * Kick off the command over the libpq connection.
			 * * If unsuccessful, proceed anyway, and check for lost connection below.
			 */
			if (PQisBusy(dispatchResult->segdbDesc->conn))
			{
				write_log
					("Trying to send to busy connection %s  %d %d asyncStatus %d",
					 dispatchResult->segdbDesc->whoami, i, db_count,
					 dispatchResult->segdbDesc->conn->asyncStatus);
			}

			if (PQstatus(dispatchResult->segdbDesc->conn) == CONNECTION_BAD)
			{
				char *msg;

				msg = PQerrorMessage(dispatchResult->segdbDesc->conn);

				write_log
					("Dispatcher noticed a problem before query transmit: %s (%s)",
					 msg ? msg : "unknown error",
					 dispatchResult->segdbDesc->whoami);

				/*
				 * Save error info for later.
				 */
				cdbdisp_appendMessage(dispatchResult, LOG,
									  ERRCODE_GP_INTERCONNECTION_ERROR,
									  "Error before transmit from %s: %s",
									  dispatchResult->segdbDesc->whoami,
									  msg ? msg : "unknown error");

				PQfinish(dispatchResult->segdbDesc->conn);
				dispatchResult->segdbDesc->conn = NULL;
				dispatchResult->stillRunning = false;

				continue;
			}
#ifdef USE_NONBLOCKING
			/*
			 * In 2000, Tom Lane said:
			 * "I believe that the nonblocking-mode code is pretty buggy, and don't
			 *	recommend using it unless you really need it and want to help debug
			 *	it.."
			 *
			 * Reading through the code, I'm not convinced the situation has
			 * improved in 2007... I still see some very questionable things
			 * about nonblocking mode, so for now, I'm disabling it.
			 */
			PQsetnonblocking(dispatchResult->segdbDesc->conn, TRUE);
#endif

			dispatchCommand(dispatchResult, pParms->query_text,
							pParms->query_text_len);
		}
	}

#ifdef USE_NONBLOCKING

	/*
	 * Is everything sent?	Well, if the network stack was too busy, and we are using
	 * nonblocking mode, some of the sends
	 * might not have completed.  We can't use SELECT to wait unless they have
	 * received their work, or we will wait forever.	Make sure they do.
	 */

	{
		bool allsent = true;

		/*
		 * debug loop to check to see if this really is needed
		 */
		for (i = 0; i < db_count; i++)
		{
			dispatchResult = pParms->dispatchResultPtrArray[i];
			if (!dispatchResult->stillRunning
				|| !dispatchResult->hasDispatched)
				continue;
			if (PQstatus(dispatchResult->segdbDesc->conn) == CONNECTION_BAD)
				continue;
			if (dispatchResult->segdbDesc->conn->outCount > 0)
			{
				write_log("Yes, extra flushing is necessary %d", i);
				break;
			}
		}

		/*
		 * Check to see if any needed extra flushing.
		 */
		for (i = 0; i < db_count; i++)
		{
			int	flushResult;

			dispatchResult = pParms->dispatchResultPtrArray[i];
			if (!dispatchResult->stillRunning
				|| !dispatchResult->hasDispatched)
				continue;
			if (PQstatus(dispatchResult->segdbDesc->conn) == CONNECTION_BAD)
				continue;
			/*
			 * If data remains unsent, send it.  Else we might be waiting for the
			 * result of a command the backend hasn't even got yet.
			 */
			flushResult = PQflush(dispatchResult->segdbDesc->conn);
			/*
			 * First time, go through the loop without waiting if we can't 
			 * flush, in case we are using multiple network adapters, and
			 * other connections might be able to flush
			 */
			if (flushResult > 0)
			{
				allsent = false;
				write_log("flushing didn't finish the work %d", i);
			}

		}

		/*
		 * our first attempt at doing more flushes didn't get everything out,
		 * so we need to continue to try.
		 */

		for (i = 0; i < db_count; i++)
		{
			dispatchResult = pParms->dispatchResultPtrArray[i];
			while (PQisnonblocking(dispatchResult->segdbDesc->conn))
			{
				PQflush(dispatchResult->segdbDesc->conn);
				PQsetnonblocking(dispatchResult->segdbDesc->conn, FALSE);
			}
		}

	}
#endif
}

static void
thread_DispatchWait(DispatchCommandParms * pParms)
{
	SegmentDatabaseDescriptor *segdbDesc;
	CdbDispatchResult *dispatchResult;
	int	i,
		db_count = pParms->db_count;
	int	timeoutCounter = 0;

	/*
	 * OK, we are finished submitting the command to the segdbs.
	 * Now, we have to wait for them to finish.
	 */
	for (;;)
	{
		int	sock;
		int	n;
		int	nfds = 0;
		int	cur_fds_num = 0;

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

			/*
			 * Add socket to fd_set if still connected.
			 */
			sock = PQsocket(segdbDesc->conn);
			if (sock >= 0 && PQstatus(segdbDesc->conn) != CONNECTION_BAD)
			{
				pParms->fds[nfds].fd = sock;
				pParms->fds[nfds].events = POLLIN;
				nfds++;
				Assert(nfds <= pParms->nfds);
			}

			/*
			 * Lost the connection.
			 */
			else
			{
				char *msg = PQerrorMessage(segdbDesc->conn);

				/*
				 * Save error info for later.
				 */
				cdbdisp_appendMessage(dispatchResult, DEBUG1,
									  ERRCODE_GP_INTERCONNECTION_ERROR,
									  "Lost connection to %s.  %s",
									  segdbDesc->whoami, msg ? msg : "");

				/*
				 * Free the PGconn object.
				 */
				PQfinish(segdbDesc->conn);
				segdbDesc->conn = NULL;
				dispatchResult->stillRunning = false;
			}
		}

		/*
		 * Break out when no QEs still running.
		 */
		if (nfds <= 0)
			break;

		/*
		 * bail-out if we are dying.  We should not do much of cleanup
		 * as the main thread is waiting on this thread to finish.	Once
		 * QD dies, QE will recognize it shortly anyway.
		 */
		if (proc_exit_inprogress)
			break;

		/*
		 * Wait for results from QEs. Block here until input is available.
		 */
		n = poll(pParms->fds, nfds, DISPATCH_WAIT_TIMEOUT_SEC * 1000);

		if (n < 0)
		{
			int	sock_errno = SOCK_ERRNO;

			if (sock_errno == EINTR)
				continue;

			handlePollError(pParms, db_count, sock_errno);
			continue;
		}

		if (n == 0)
		{
			handlePollTimeout(pParms, db_count, &timeoutCounter, true);
			continue;
		}

		cur_fds_num = 0;
		/*
		 * We have data waiting on one or more of the connections.
		 */
		for (i = 0; i < db_count; i++)
		{
			bool finished;

			dispatchResult = pParms->dispatchResultPtrArray[i];
			segdbDesc = dispatchResult->segdbDesc;

			/*
			 * Skip if already finished or didn't dispatch. 
			 */
			if (!dispatchResult->stillRunning)
				continue;

			if (DEBUG4 >= log_min_messages)
				write_log("looking for results from %d of %d", i + 1,
						  db_count);

			/*
			 * Skip this connection if it has no input available.
			 */
			sock = PQsocket(segdbDesc->conn);
			if (sock >= 0)
			{
				/*
				 * The fds array is shorter than conn array, so the following
				 * match method will use this assumtion.
				 */
				Assert(sock == pParms->fds[cur_fds_num].fd);
			}
			if (sock >= 0 && (sock == pParms->fds[cur_fds_num].fd))
			{
				cur_fds_num++;
				if (!(pParms->fds[cur_fds_num - 1].revents & POLLIN))
					continue;
			}

			if (DEBUG4 >= log_min_messages)
				write_log("PQsocket says there are results from %d", i + 1);
			/*
			 * Receive and process results from this QE.
			 */
			finished = processResults(dispatchResult);

			/*
			 * Are we through with this QE now?
			 */
			if (finished)
			{
				if (DEBUG4 >= log_min_messages)
					write_log
						("processResults says we are finished with %d:  %s",
						 i + 1, segdbDesc->whoami);
				dispatchResult->stillRunning = false;
				if (DEBUG1 >= log_min_messages)
				{
					char msec_str[32];

					switch (check_log_duration(msec_str, false))
					{
						case 1:
						case 2:
							write_log
								("duration to dispatch result received from thread %d (seg %d): %s ms",
								 i + 1, dispatchResult->segdbDesc->segindex,
								 msec_str);
							break;
					}
				}
				if (PQisBusy(dispatchResult->segdbDesc->conn))
					write_log
						("We thought we were done, because finished==true, but libpq says we are still busy");

			}
			else if (DEBUG4 >= log_min_messages)
				write_log("processResults says we have more to do with %d: %s",
						  i + 1, segdbDesc->whoami);
		}
	}
}

static void
thread_DispatchWaitSingle(DispatchCommandParms * pParms)
{
	SegmentDatabaseDescriptor *segdbDesc;
	CdbDispatchResult *dispatchResult;
	char *msg = NULL;

	/*
	 * Assert() cannot be used in threads
	 */
	if (pParms->db_count != 1)
		write_log("Bug... thread_dispatchWaitSingle called with db_count %d",
				  pParms->db_count);

	dispatchResult = pParms->dispatchResultPtrArray[0];
	segdbDesc = dispatchResult->segdbDesc;

	if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
	{
		msg = PQerrorMessage(segdbDesc->conn);

		/*
		 * Save error info for later.
		 */
		cdbdisp_appendMessage(dispatchResult, DEBUG1,
							  ERRCODE_GP_INTERCONNECTION_ERROR,
							  "Lost connection to %s.  %s",
							  segdbDesc->whoami, msg ? msg : "");

		/*
		 * Free the PGconn object.
		 */
		PQfinish(segdbDesc->conn);
		segdbDesc->conn = NULL;
		dispatchResult->stillRunning = false;
	}
	else
	{
		PQsetnonblocking(segdbDesc->conn, FALSE);

		for (;;)
		{
			PGresult *pRes;
			ExecStatusType resultStatus;
			int	resultIndex = cdbdisp_numPGresult(dispatchResult);

			if (DEBUG4 >= log_min_messages)
				write_log("PQgetResult, resultIndex = %d", resultIndex);
			/*
			 * Get one message.
			 */
			pRes = PQgetResult(segdbDesc->conn);

			CollectQEWriterTransactionInformation(segdbDesc, dispatchResult);

			/*
			 * Command is complete when PGgetResult() returns NULL. It is critical
			 * that for any connection that had an asynchronous command sent thru
			 * it, we call PQgetResult until it returns NULL. Otherwise, the next
			 * time a command is sent to that connection, it will return an error
			 * that there's a command pending.
			 */
			if (!pRes)
			{
				if (DEBUG4 >= log_min_messages)
				{
					/*
					 * Don't use elog, it's not thread-safe
					 */
					write_log("%s -> idle", segdbDesc->whoami);
				}
				break;
			}

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
				resultStatus == PGRES_COPY_IN ||
				resultStatus == PGRES_COPY_OUT)
			{
				/*
				 * Save the index of the last successful PGresult. Can be given to
				 * cdbdisp_getPGresult() to get tuple count, etc.
				 */
				dispatchResult->okindex = resultIndex;

				if (DEBUG3 >= log_min_messages)
				{
					/*
					 * Don't use elog, it's not thread-safe
					 */
					char *cmdStatus = PQcmdStatus(pRes);

					write_log("%s -> ok %s",
							  segdbDesc->whoami,
							  cmdStatus ? cmdStatus : "(no cmdStatus)");
				}

				if (resultStatus == PGRES_COPY_IN ||
					resultStatus == PGRES_COPY_OUT)
					return;
			}

			/*
			 * Note QE error.  Cancel the whole statement if requested.
			 */
			else
			{
				char *sqlstate = PQresultErrorField(pRes, PG_DIAG_SQLSTATE);
				int	errcode = 0;

				msg = PQresultErrorMessage(pRes);

				if (DEBUG2 >= log_min_messages)
				{
					/*
					 * Don't use elog, it's not thread-safe
					 */
					write_log("%s -> %s %s  %s",
							  segdbDesc->whoami,
							  PQresStatus(resultStatus),
							  sqlstate ? sqlstate : "(no SQLSTATE)",
							  msg ? msg : "");
				}

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


		if (DEBUG4 >= log_min_messages)
			write_log("processResultsSingle says we are finished with :  %s",
					  segdbDesc->whoami);
		dispatchResult->stillRunning = false;
		if (DEBUG1 >= log_min_messages)
		{
			char msec_str[32];

			switch (check_log_duration(msec_str, false))
			{
				case 1:
				case 2:
					write_log
						("duration to dispatch result received from thread (seg %d): %s ms",
						 dispatchResult->segdbDesc->segindex, msec_str);
					break;
			}
		}
		if (PQisBusy(dispatchResult->segdbDesc->conn))
			write_log
				("We thought we were done, because finished==true, but libpq says we are still busy");
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
 *		 elog is NOT thread-safe.  Developers should instead use something like:
 *
 *	if (DEBUG3 >= log_min_messages)
 *			write_log("my brilliant log statement here.");
 *
 * NOTE: In threads, we cannot use palloc, because it's not thread safe.
 */
static void *
thread_DispatchCommand(void *arg)
{
	DispatchCommandParms *pParms = (DispatchCommandParms *) arg;

	gp_set_thread_sigmasks();

	/*
	 * Mark that we are runnig a new thread.  The main thread will check
	 * it to see if there is still alive one.  Let's do this after we block
	 * signals so that nobody will intervent and mess up the value.
	 * (should we actually block signals before spawning a thread, as much
	 * like we do in fork??)
	 */
	pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &RunningThreadCount, 1);

	/*
	 * We need to make sure the value will be decremented once the thread
	 * finishes.  Currently there is not such case but potentially we could
	 * have pthread_exit or thread cancellation in the middle of code, in
	 * which case we would miss to decrement value if we tried to do this
	 * without the cleanup callback facility.
	 */
	pthread_cleanup_push(DecrementRunningCount, NULL);
	{
		thread_DispatchOut(pParms);
		/*
		 * thread_DispatchWaitSingle might have a problem with interupts
		 */
		if (pParms->db_count == 1 && false)
			thread_DispatchWaitSingle(pParms);
		else
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
static void
dispatchCommand(CdbDispatchResult * dispatchResult,
				const char *query_text, int query_text_len)
{
	SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
	PGconn *conn = segdbDesc->conn;
	TimestampTz beforeSend = 0;
	long secs;
	int	usecs;

	if (DEBUG1 >= log_min_messages)
		beforeSend = GetCurrentTimestamp();

	/*
	 * Submit the command asynchronously.
	 */
	if (PQsendGpQuery_shared(conn, (char *) query_text, query_text_len) == 0)
	{
		char *msg = PQerrorMessage(segdbDesc->conn);

		if (DEBUG3 >= log_min_messages)
			write_log("PQsendMPPQuery_shared error %s %s",
					  segdbDesc->whoami, msg ? msg : "");

		/*
		 * Note the error.
		 */
		cdbdisp_appendMessage(dispatchResult, LOG,
							  ERRCODE_GP_INTERCONNECTION_ERROR,
							  "Command could not be sent to segment db %s;  %s",
							  segdbDesc->whoami, msg ? msg : "");
		PQfinish(conn);
		segdbDesc->conn = NULL;
		dispatchResult->stillRunning = false;
	}

	if (DEBUG1 >= log_min_messages)
	{
		TimestampDifference(beforeSend, GetCurrentTimestamp(), &secs, &usecs);

		if (secs != 0 || usecs > 1000)	/* Time > 1ms? */
			write_log("time for PQsendGpQuery_shared %ld.%06d", secs, usecs);
	}

	dispatchResult->hasDispatched = true;
	/*
	 * We'll keep monitoring this QE -- whether or not the command
	 * was dispatched -- in order to check for a lost connection
	 * or any other errors that libpq might have in store for us.
	 */
}

/*
 * Helper function to thread_DispatchCommand that handles errors that occur
 * during the poll() call.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *		 thread_DispatchCommand absolutely no elog'ing.
 *		 The cleanup of the connections will be performed by handlePollTimeout().
 */
static void
handlePollError(DispatchCommandParms * pParms, int db_count, int sock_errno)
{
	int	i;
	int	forceTimeoutCount;

	if (LOG >= log_min_messages)
	{
		/*
		 * Don't use elog, it's not thread-safe
		 */
		write_log("handlePollError poll() failed; errno=%d", sock_errno);
	}

	/*
	 * Based on the select man page, we could get here with
	 * errno == EBADF (bad descriptor), EINVAL (highest descriptor negative or negative timeout)
	 * or ENOMEM (out of memory).
	 * This is most likely a programming error or a bad system failure, but we'll try to 
	 * clean up a bit anyhow.
	 *
	 * We *can* get here as a result of some hardware issues. the timeout code
	 * knows how to clean up if we've lost contact with one of our peers.
	 *
	 * We should check a connection's integrity before calling PQisBusy().
	 */
	for (i = 0; i < db_count; i++)
	{
		CdbDispatchResult *dispatchResult = pParms->dispatchResultPtrArray[i];

		/*
		 * Skip if already finished or didn't dispatch. 
		 */
		if (!dispatchResult->stillRunning)
			continue;

		/*
		 * We're done with this QE, sadly. 
		 */
		if (PQstatus(dispatchResult->segdbDesc->conn) == CONNECTION_BAD)
		{
			char *msg;

			msg = PQerrorMessage(dispatchResult->segdbDesc->conn);
			if (msg)
				write_log("Dispatcher encountered connection error on %s: %s",
						  dispatchResult->segdbDesc->whoami, msg);

			write_log
				("Dispatcher noticed bad connection in handlePollError()");

			/*
			 * Save error info for later.
			 */
			cdbdisp_appendMessage(dispatchResult, LOG,
								  ERRCODE_GP_INTERCONNECTION_ERROR,
								  "Error after dispatch from %s: %s",
								  dispatchResult->segdbDesc->whoami,
								  msg ? msg : "unknown error");

			PQfinish(dispatchResult->segdbDesc->conn);
			dispatchResult->segdbDesc->conn = NULL;
			dispatchResult->stillRunning = false;
		}
	}

	forceTimeoutCount = 60;		/* anything bigger than 30 */
	handlePollTimeout(pParms, db_count, &forceTimeoutCount, false);

	return;

	/*
	 * No point in trying to cancel the other QEs with select() broken.
	 */
}

/*
 * Helper function to thread_DispatchCommand that handles timeouts that occur
 * during the poll() call.
 *
 * NOTE: since this is called via a thread, the same rules apply as to
 *		 thread_DispatchCommand absolutely no elog'ing.
 */
static void
handlePollTimeout(DispatchCommandParms * pParms,
				  int db_count, int *timeoutCounter, bool useSampling)
{
	CdbDispatchResult *dispatchResult;
	CdbDispatchResults *meleeResults;
	SegmentDatabaseDescriptor *segdbDesc;
	int i;

	/*
	 * Are there any QEs that should be canceled?
	 */
	for (i = 0; i < db_count; i++)
	{
		DispatchWaitMode waitMode;

		dispatchResult = pParms->dispatchResultPtrArray[i];
		if (dispatchResult == NULL)
			continue;
		segdbDesc = dispatchResult->segdbDesc;
		meleeResults = dispatchResult->meleeResults;

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
		 * However, escalate it to cancel if:
		 *	 - user interrupt has occurred,
		 *	 - or I'm told to send cancel,
		 *	 - or an error has been reported by another QE,
		 *	 - in case the caller wants cancelOnError and it was not canceled
		 */
		if ((InterruptPending ||
			 pParms->waitMode == DISPATCH_WAIT_CANCEL ||
			 meleeResults->errcode) &&
			(meleeResults->cancelOnError && !dispatchResult->wasCanceled))
			waitMode = DISPATCH_WAIT_CANCEL;

		/*
		 * Finally, don't send the signal if
		 *	 - no action needed (NONE)
		 *	 - the signal was already sent
		 *	 - connection is dead
		 */
		if (waitMode != DISPATCH_WAIT_NONE &&
			waitMode != dispatchResult->sentSignal &&
			PQstatus(segdbDesc->conn) != CONNECTION_BAD)
		{
			dispatchResult->sentSignal = cdbdisp_signalQE(segdbDesc, waitMode);
		}
	}

	/*
	 * check the connection still valid, set 1 min time interval
	 * this may affect performance, should turn it off if required.
	 */
	if ((*timeoutCounter)++ > 30)
	{
		*timeoutCounter = 0;

		for (i = 0; i < db_count; i++)
		{
			dispatchResult = pParms->dispatchResultPtrArray[i];
			segdbDesc = dispatchResult->segdbDesc;

			if (DEBUG5 >= log_min_messages)
				write_log("checking status %d of %d %s stillRunning %d",
						  i + 1, db_count, segdbDesc->whoami,
						  dispatchResult->stillRunning);

			/*
			 * Skip if already finished or didn't dispatch. 
			 */
			if (!dispatchResult->stillRunning)
				continue;

			/*
			 * If we hit the timeout, and the query has already been
			 * cancelled we'll try to re-cancel here.
			 *
			 * XXX we may not need this anymore.  It might be harmful
			 * rather than helpful, as it creates another connection.
			 */
			if (dispatchResult->sentSignal == DISPATCH_WAIT_CANCEL &&
				PQstatus(segdbDesc->conn) != CONNECTION_BAD)
			{
				dispatchResult->sentSignal =
					cdbdisp_signalQE(segdbDesc, DISPATCH_WAIT_CANCEL);
			}

			/*
			 * Skip the entry db.
			 */
			if (segdbDesc->segindex < 0)
				continue;

			if (DEBUG5 >= log_min_messages)
				write_log("testing connection %d of %d     %s stillRunning %d",
						  i + 1, db_count, segdbDesc->whoami,
						  dispatchResult->stillRunning);

			if (!FtsTestConnection(segdbDesc->segment_database_info, false))
			{
				/*
				 * Note the error.
				 */
				cdbdisp_appendMessage(dispatchResult, DEBUG1,
									  ERRCODE_GP_INTERCONNECTION_ERROR,
									  "Lost connection to one or more segments - fault detector checking for segment failures. (%s)",
									  segdbDesc->whoami);

				/*
				 * Not a good idea to store into the PGconn object. Instead,
				 * just close it.
				 */
				PQfinish(segdbDesc->conn);
				segdbDesc->conn = NULL;

				/*
				 * This connection is hosed.
				 */
				dispatchResult->stillRunning = false;
			}
		}
	}

}

static int
getMaxThreadsPerGang(void)
{
	int	maxThreads = 0;

	if (gp_connections_per_thread == 0)
		maxThreads = 1; /* one, not zero, because we need to allocate one param block */
	else
		maxThreads = 1 + (largestGangsize() - 1) / gp_connections_per_thread;
	return maxThreads;
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
shouldStillDispatchCommand(DispatchCommandParms * pParms,
						   CdbDispatchResult * dispatchResult)
{
	SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
	CdbDispatchResults *gangResults = dispatchResult->meleeResults;

	/*
	 * Don't dispatch to a QE that is not connected. Note, that PQstatus() correctly
	 * * handles the case where segdbDesc->conn is NULL, and we *definitely* want to
	 * * produce an error for that case.
	 */
	if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
	{
		char *msg = PQerrorMessage(segdbDesc->conn);

		/*
		 * Save error info for later.
		 */
		cdbdisp_appendMessage(dispatchResult, LOG,
							  ERRCODE_GP_INTERCONNECTION_ERROR,
							  "Lost connection to %s.  %s",
							  segdbDesc->whoami, msg ? msg : "");

		if (DEBUG4 >= log_min_messages)
		{
			/*
			 * Don't use elog, it's not thread-safe
			 */
			write_log("Lost connection: %s", segdbDesc->whoami);
		}

		/*
		 * Free the PGconn object at once whenever we notice it's gone bad. 
		 */
		PQfinish(segdbDesc->conn);
		segdbDesc->conn = NULL;
		dispatchResult->stillRunning = false;

		return false;
	}

	/*
	 * Don't submit if already encountered an error. The error has already
	 * been noted, so just keep quiet.
	 */
	if (pParms->waitMode == DISPATCH_WAIT_CANCEL || gangResults->errcode)
	{
		if (gangResults->cancelOnError)
		{
			dispatchResult->wasCanceled = true;

			if (Debug_cancel_print || DEBUG4 >= log_min_messages)
			{
				/*
				 * Don't use elog, it's not thread-safe
				 */
				write_log("Error cleanup in progress; command not sent to %s",
						  segdbDesc->whoami);
			}
			return false;
		}
	}

	/*
	 * Don't submit if client told us to cancel. The cancellation request has
	 * already been noted, so hush.
	 */
	if (InterruptPending && gangResults->cancelOnError)
	{
		dispatchResult->wasCanceled = true;
		if (Debug_cancel_print || DEBUG4 >= log_min_messages)
			write_log("Cancellation request pending; command not sent to %s",
					  segdbDesc->whoami);
		return false;
	}

	return true;
}

static bool
processResults(CdbDispatchResult * dispatchResult)
{
	SegmentDatabaseDescriptor *segdbDesc = dispatchResult->segdbDesc;
	char *msg;
	int	rc;

	/*
	 * PQisBusy() has side-effects
	 */
	if (DEBUG5 >= log_min_messages)
	{
		write_log("processResults.  isBusy = %d", PQisBusy(segdbDesc->conn));

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
	if (DEBUG4 >= log_min_messages && PQisBusy(segdbDesc->conn))
		write_log("PQisBusy");

	/*
	 * If we have received one or more complete messages, process them.
	 */
	while (!PQisBusy(segdbDesc->conn))
	{
		/* loop to call PQgetResult; won't block */
		PGresult *pRes;
		ExecStatusType resultStatus;
		int	resultIndex;

		/*
		 * PQisBusy() does some error handling, which can
		 * cause the connection to die -- we can't just continue on as
		 * if the connection is happy without checking first.
		 * 
		 * For example, cdbdisp_numPGresult() will return a completely
		 * bogus value!
		 */
		if (PQstatus(segdbDesc->conn) == CONNECTION_BAD
			|| segdbDesc->conn->sock == -1)
		{
			goto connection_error;
		}

		resultIndex = cdbdisp_numPGresult(dispatchResult);

		if (DEBUG4 >= log_min_messages)
			write_log("PQgetResult");
		/*
		 * Get one message.
		 */
		pRes = PQgetResult(segdbDesc->conn);

		CollectQEWriterTransactionInformation(segdbDesc, dispatchResult);

		/*
		 * Command is complete when PGgetResult() returns NULL. It is critical
		 * that for any connection that had an asynchronous command sent thru
		 * it, we call PQgetResult until it returns NULL. Otherwise, the next
		 * time a command is sent to that connection, it will return an error
		 * that there's a command pending.
		 */
		if (!pRes)
		{
			if (DEBUG4 >= log_min_messages)
			{
				/*
				 * Don't use elog, it's not thread-safe
				 */
				write_log("%s -> idle", segdbDesc->whoami);
			}
			/* this is normal end of command */
			return true;
		} /* end of results */


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

			if (DEBUG3 >= log_min_messages)
			{
				/*
				 * Don't use elog, it's not thread-safe
				 */
				char	   *cmdStatus = PQcmdStatus(pRes);

				write_log("%s -> ok %s",
						  segdbDesc->whoami,
						  cmdStatus ? cmdStatus : "(no cmdStatus)");
			}

			/*
			 * SREH - get number of rows rejected from QE if any
			 */
			if (pRes->numRejected > 0)
				dispatchResult->numrowsrejected += pRes->numRejected;

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

			if (DEBUG2 >= log_min_messages)
			{
				/*
				 * Don't use elog, it's not thread-safe
				 */
				write_log("%s -> %s %s  %s",
						  segdbDesc->whoami,
						  PQresStatus(resultStatus),
						  sqlstate ? sqlstate : "(no SQLSTATE)",
						  msg ? msg : "");
			}

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

	return false; /* we must keep on monitoring this socket */

connection_error:
	msg = PQerrorMessage(segdbDesc->conn);

	if (msg)
		write_log("Dispatcher encountered connection error on %s: %s",
				  segdbDesc->whoami, msg);

	/*
	 * Save error info for later.
	 */
	cdbdisp_appendMessage(dispatchResult, LOG,
						  ERRCODE_GP_INTERCONNECTION_ERROR,
						  "Error on receive from %s: %s",
						  segdbDesc->whoami, msg ? msg : "unknown error");

	/*
	 * Can't recover, so drop the connection. 
	 */
	PQfinish(segdbDesc->conn);
	segdbDesc->conn = NULL;
	dispatchResult->stillRunning = false;

	return true; /* connection is gone! */
}

static void
CollectQEWriterTransactionInformation(SegmentDatabaseDescriptor * segdbDesc,
									  CdbDispatchResult * dispatchResult)
{
	PGconn *conn = segdbDesc->conn;

	if (conn && conn->QEWriter_HaveInfo)
	{
		dispatchResult->QEIsPrimary = true;
		dispatchResult->QEWriter_HaveInfo = true;
		dispatchResult->QEWriter_DistributedTransactionId = conn->QEWriter_DistributedTransactionId;
		dispatchResult->QEWriter_CommandId = conn->QEWriter_CommandId;
		if (conn && conn->QEWriter_Dirty)
		{
			dispatchResult->QEWriter_Dirty = true;
		}
	}
}

/*
 * Send cancel/finish signal to still-running QE through libpq.
 * waitMode is either CANCEL or FINISH.  Returns true if we successfully
 * sent a signal (not necessarily received by the target process).
 */
static DispatchWaitMode
cdbdisp_signalQE(SegmentDatabaseDescriptor * segdbDesc,
				 DispatchWaitMode waitMode)
{
	char errbuf[256];
	PGcancel *cn = PQgetCancel(segdbDesc->conn);
	int	ret = 0;

	if (cn == NULL)
		return DISPATCH_WAIT_NONE;

	/*
	 * PQcancel uses some strcpy/strcat functions; let's
	 * clear this for safety.
	 */
	MemSet(errbuf, 0, sizeof(errbuf));

	if (Debug_cancel_print || DEBUG4 >= log_min_messages)
		write_log("Calling PQcancel for %s", segdbDesc->whoami);

	/*
	 * Send query-finish, unless the client really wants to cancel the
	 * query.  This could happen if cancel comes after we sent finish.
	 */
	if (waitMode == DISPATCH_WAIT_CANCEL)
		ret = PQcancel(cn, errbuf, 256);
	else if (waitMode == DISPATCH_WAIT_FINISH)
		ret = PQrequestFinish(cn, errbuf, 256);
	else
		write_log("unknown waitMode: %d", waitMode);

	if (ret == 0 && (Debug_cancel_print || LOG >= log_min_messages))
		write_log("Unable to cancel: %s", errbuf);

	PQfreeCancel(cn);

	return (ret != 0 ? waitMode : DISPATCH_WAIT_NONE);
}
