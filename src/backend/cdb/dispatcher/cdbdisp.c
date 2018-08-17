
/*-------------------------------------------------------------------------
 *
 * cdbdisp.c
 *	  Functions to dispatch commands to QExecutors.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbdisp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include <limits.h>

#include "storage/ipc.h"		/* For proc_exit_inprogress */
#include "tcop/tcopprot.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdisp_thread.h"
#include "cdb/cdbdisp_async.h"
#include "cdb/cdbdispatchresult.h"
#include "executor/execUtils.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"

typedef struct dispatcher_handle_t
{
	struct CdbDispatcherState *dispatcherState;

	ResourceOwner owner;	/* owner of this handle */
	struct dispatcher_handle_t *next;
	struct dispatcher_handle_t *prev;
} dispatcher_handle_t;

static dispatcher_handle_t *open_dispatcher_handles;
static bool dispatcher_resowner_callback_registered;
static void dispatcher_abort_callback(ResourceReleasePhase phase,
								   bool isCommit,
								   bool isTopLevel,
								   void *arg);
static void cleanup_dispatcher_handle(dispatcher_handle_t *h);

static dispatcher_handle_t *find_dispatcher_handle(CdbDispatcherState *ds);
static dispatcher_handle_t *allocate_dispatcher_handle(void);
static void destroy_dispatcher_handle(dispatcher_handle_t *h);

/*
 * default directed-dispatch parameters: don't direct anything.
 */
CdbDispatchDirectDesc default_dispatch_direct_desc = {false, 0, {0}};

static void cdbdisp_clearGangActiveFlag(CdbDispatcherState *ds);

static DispatcherInternalFuncs *pDispatchFuncs = NULL;

/*
 * cdbdisp_dispatchToGang:
 * Send the strCommand SQL statement to the subset of all segdbs in the cluster
 * specified by the gang parameter. cancelOnError indicates whether an error
 * occurring on one of the qExec segdbs should cause all still-executing commands to cancel
 * on other qExecs. Normally this would be true. The commands are sent over the libpq
 * connections that were established during cdblink_setup.	They are run inside of threads.
 * The number of segdbs handled by any one thread is determined by the
 * guc variable gp_connections_per_thread.
 *
 * The caller must provide a CdbDispatchResults object having available
 * resultArray slots sufficient for the number of QEs to be dispatched:
 * i.e., resultCapacity - resultCount >= gp->size.	This function will
 * assign one resultArray slot per QE of the Gang, paralleling the Gang's
 * db_descriptors array. Success or failure of each QE will be noted in
 * the QE's CdbDispatchResult entry; but before examining the results, the
 * caller must wait for execution to end by calling cdbdisp_checkDispatchResult().
 *
 * The CdbDispatchResults object owns some malloc'ed storage, so the caller
 * must make certain to free it by calling cdbdisp_destroyDispatcherState().
 *
 * When dispatchResults->cancelOnError is false, strCommand is to be
 * dispatched to every connected gang member if possible, despite any
 * cancellation requests, QE errors, connection failures, etc.
 *
 * NB: This function should return normally even if there is an error.
 * It should not longjmp out via elog(ERROR, ...), ereport(ERROR, ...),
 * PG_THROW, CHECK_FOR_INTERRUPTS, etc.
 */
void
cdbdisp_dispatchToGang(struct CdbDispatcherState *ds,
					   struct Gang *gp,
					   int sliceIndex,
					   CdbDispatchDirectDesc *disp_direct)
{
	struct CdbDispatchResults *dispatchResults = ds->primaryResults;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(gp && gp->size > 0);
	Assert(dispatchResults && dispatchResults->resultArray);

	if (dispatchResults->writer_gang)
	{
		/*
		 * Are we dispatching to the writer-gang when it is already busy ?
		 */
		if (gp == dispatchResults->writer_gang)
		{
			if (dispatchResults->writer_gang->dispatcherActive)
			{
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("query plan with multiple segworker groups is not supported"),
						 errhint("likely caused by a function that reads or modifies data in a distributed table")));
			}

			dispatchResults->writer_gang->dispatcherActive = true;
		}
	}

	/*
	 * WIP: will use a function pointer for implementation later, currently
	 * just use an internal function to move dispatch thread related code into
	 * a separate file.
	 */
	(pDispatchFuncs->dispatchToGang) (ds, gp, sliceIndex, disp_direct);
}

/*
 * For asynchronous dispatcher, we have to wait all dispatch to finish before we move on to query execution,
 * otherwise we may get into a deadlock situation, e.g, gather motion node waiting for data,
 * while segments waiting for plan. This is skipped in threaded dispatcher as data is sent in blocking style.
 */
void
cdbdisp_waitDispatchFinish(struct CdbDispatcherState *ds)
{
	if (pDispatchFuncs->waitDispatchFinish != NULL)
		(pDispatchFuncs->waitDispatchFinish) (ds);
}

/*
 * cdbdisp_checkDispatchResult:
 *
 * Waits for completion of threads launched by cdbdisp_dispatchToGang().
 *
 * QEs that were dispatched with 'cancelOnError' true and are not yet idle
 * will be canceled/finished according to waitMode.
 */
void
cdbdisp_checkDispatchResult(struct CdbDispatcherState *ds,
					   DispatchWaitMode waitMode)
{
	PG_TRY();
	{
		(pDispatchFuncs->checkResults) (ds, waitMode);
	}
	PG_CATCH();
	{
		cdbdisp_clearGangActiveFlag(ds);
		PG_RE_THROW();
	}
	PG_END_TRY();

	cdbdisp_clearGangActiveFlag(ds);

	if (log_dispatch_stats)
		ShowUsage("DISPATCH STATISTICS");

	if (DEBUG1 >= log_min_messages)
	{
		char		msec_str[32];

		switch (check_log_duration(msec_str, false))
		{
			case 1:
			case 2:
				ereport(LOG,
						(errmsg("duration to dispatch result received from all QEs: %s ms", msec_str)));
				break;
		}
	}
}

/*
 * cdbdisp_getDispatchResults:
 *
 * Block until all QEs return results or report errors.
 *
 * Return Values:
 *   Return NULL If one or more QEs got Error. In that case, *qeErrors contains
 *   a list of ErrorDatas.
 */
struct CdbDispatchResults *
cdbdisp_getDispatchResults(struct CdbDispatcherState *ds, ErrorData **qeError)
{
	int			errorcode;

	if (!ds || !ds->primaryResults)
	{
		/*
		 * Fallback in case we have no dispatcher state.  Since the caller is
		 * likely to output the errors on NULL return, add an error message to
		 * aid debugging.
		 */
		*qeError = ereport_and_return(ERROR,
									  (errcode(ERRCODE_INTERNAL_ERROR),
									   errmsg("no dispatcher state")));
		return NULL;
	}

	/* check if any error reported */
	errorcode = ds->primaryResults->errcode;

	if (errorcode)
	{
		cdbdisp_dumpDispatchResults(ds->primaryResults, qeError);
		return NULL;
	}

	return ds->primaryResults;
}

/*
 * CdbDispatchHandleError
 *
 * When caller catches an error, the PG_CATCH handler can use this
 * function instead of cdbdisp_finishCommand to wait for all QEs
 * to finish, and report QE errors if appropriate.
 * This function should be called only from PG_CATCH handlers.
 *
 * This function doesn't cleanup dispatcher state, dispatcher state
 * will be destroyed as part of the resource owner cleanup.
 *
 * On return, the caller is expected to finish its own cleanup and
 * exit via PG_RE_THROW().
 */
void
CdbDispatchHandleError(struct CdbDispatcherState *ds)
{
	int		qderrcode;
	bool		useQeError = false;
	ErrorData *error = NULL;

	/*
	 * If cdbdisp_dispatchToGang() wasn't called, don't wait.
	 */
	if (!ds || !ds->primaryResults)
		return;

	/*
	 * Request any remaining commands executing on qExecs to stop. We need to
	 * wait for the threads to finish. This allows for proper cleanup of the
	 * results from the async command executions. Cancel any QEs still
	 * running.
	 */
	cdbdisp_cancelDispatch(ds);

	/*
	 * When a QE stops executing a command due to an error, as a consequence
	 * there can be a cascade of interconnect errors (usually "sender closed
	 * connection prematurely") thrown in downstream processes (QEs and QD).
	 * So if we are handling an interconnect error, and a QE hit a more
	 * interesting error, we'll let the QE's error report take precedence.
	 */
	qderrcode = elog_geterrcode();
	if (qderrcode == ERRCODE_GP_INTERCONNECTION_ERROR)
	{
		bool		qd_lost_flag = false;
		char	   *qderrtext = elog_message();

		if (qderrtext
			&& strcmp(qderrtext, CDB_MOTION_LOST_CONTACT_STRING) == 0)
			qd_lost_flag = true;

		if (ds->primaryResults && ds->primaryResults->errcode)
		{
			if (qd_lost_flag
				&& ds->primaryResults->errcode == ERRCODE_GP_INTERCONNECTION_ERROR)
				useQeError = true;
			else if (ds->primaryResults->errcode != ERRCODE_GP_INTERCONNECTION_ERROR)
				useQeError = true;
		}
	}

	if (useQeError)
	{
		/*
		 * Throw the QE's error, catch it, and fall thru to return normally so
		 * caller can finish cleaning up. Afterwards caller must exit via
		 * PG_RE_THROW().
		 */
		MemoryContext oldcontext;

		/*
		 * During abort processing, we are running in ErrorContext. Avoid
		 * doing these heavy things in ErrorContext. (There's one particular
		 * issue: these calls use CopyErrorData(), which asserts that we
		 * are not in ErrorContext.)
		 */
		oldcontext = MemoryContextSwitchTo(CurTransactionContext);

		PG_TRY();
		{
			cdbdisp_getDispatchResults(ds, &error);
			if (error != NULL)
				ReThrowError(error);
		}
		PG_CATCH();
		{
		}						/* nop; fall thru */
		PG_END_TRY();

		MemoryContextSwitchTo(oldcontext);
	}

	cdbdisp_destroyDispatcherState(ds);
}



/*
 * Allocate memory and initialize CdbDispatcherState.
 *
 * Call cdbdisp_destroyDispatcherState to free it.
 *
 *	 maxSlices: max number of slices of the query/command.
 */
CdbDispatcherState *
cdbdisp_makeDispatcherState(void)
{
	dispatcher_handle_t *handle = allocate_dispatcher_handle();
	return handle->dispatcherState;
}

void *
cdbdisp_makeDispatchParams(int maxSlices,
						  char *queryText,
						  int queryTextLen)
{
	return (pDispatchFuncs->makeDispatchParams) (maxSlices, queryText, queryTextLen);
}

/*
 * Free memory in CdbDispatcherState
 *
 * Free dispatcher memory context.
 */
void
cdbdisp_destroyDispatcherState(CdbDispatcherState *ds)
{
	if (!ds)
		return;

	CdbDispatchResults *results = ds->primaryResults;
	dispatcher_handle_t *h = find_dispatcher_handle(ds);

	if (results != NULL && results->resultArray != NULL)
	{
		int			i;

		for (i = 0; i < results->resultCount; i++)
		{
			cdbdisp_termResult(&results->resultArray[i]);
		}
		results->resultArray = NULL;
	}

	ds->dispatchParams = NULL;
	ds->primaryResults = NULL;

	if (h != NULL)
		destroy_dispatcher_handle(h);
}

void
cdbdisp_cancelDispatch(CdbDispatcherState *ds)
{
	cdbdisp_checkDispatchResult(ds, DISPATCH_WAIT_CANCEL);
}

/*
 * Clear our "active" flags; so that we know that the writer gangs are busy -- and don't stomp on
 * internal dispatcher structures.
 */
static void
cdbdisp_clearGangActiveFlag(CdbDispatcherState *ds)
{
	if (ds && ds->primaryResults && ds->primaryResults->writer_gang)
	{
		ds->primaryResults->writer_gang->dispatcherActive = false;
	}
}

bool
cdbdisp_checkForCancel(CdbDispatcherState *ds)
{
	if (pDispatchFuncs == NULL || pDispatchFuncs->checkForCancel == NULL)
		return false;
	return (pDispatchFuncs->checkForCancel) (ds);
}

/*
 * Return a file descriptor to wait for events from the QEs after dispatching
 * a query.
 *
 * This is intended for use with cdbdisp_checkForCancel(). First call
 * cdbdisp_getWaitSocketFd(), and wait on that socket to become readable
 * e.g. with select() or poll(). When it becomes readable, call
 * cdbdisp_checkForCancel() to process the incoming data, and repeat.
 *
 * XXX: This returns only one fd, but we might be waiting for results from
 * multiple QEs. In that case, this returns arbitrarily one of them. You
 * should still have a timeout, and call cdbdisp_checkForCancel()
 * periodically, to process results from the other QEs.
 */
int
cdbdisp_getWaitSocketFd(CdbDispatcherState *ds)
{
	if (pDispatchFuncs == NULL || pDispatchFuncs->getWaitSocketFd == NULL)
		return -1;
	return (pDispatchFuncs->getWaitSocketFd) (ds);
}

void
cdbdisp_onProcExit(void)
{
	if (pDispatchFuncs != NULL && pDispatchFuncs->procExitCallBack != NULL)
		(pDispatchFuncs->procExitCallBack) ();
}

void
cdbdisp_setAsync(bool async)
{
	if (async)
		pDispatchFuncs = &DispatcherAsyncFuncs;
	else
		pDispatchFuncs = &DispatcherSyncFuncs;
}

dispatcher_handle_t *
allocate_dispatcher_handle(void)
{
	dispatcher_handle_t	*h;

	if (DispatcherContext == NULL)
		DispatcherContext = AllocSetContextCreate(TopMemoryContext,
												 "Dispatch Context",
												 ALLOCSET_DEFAULT_MINSIZE,
												 ALLOCSET_DEFAULT_INITSIZE,
												 ALLOCSET_DEFAULT_MAXSIZE);


	h = MemoryContextAllocZero(DispatcherContext, sizeof(dispatcher_handle_t));

	h->dispatcherState = MemoryContextAllocZero(DispatcherContext, sizeof(CdbDispatcherState));
	h->owner = CurrentResourceOwner;
	h->next = open_dispatcher_handles;
	h->prev = NULL;
	if (open_dispatcher_handles)
		open_dispatcher_handles->prev = h;
	open_dispatcher_handles = h;

	if (!dispatcher_resowner_callback_registered)
	{
		RegisterResourceReleaseCallback(dispatcher_abort_callback, NULL);
		dispatcher_resowner_callback_registered = true;
	}
	return h;
}

static void
destroy_dispatcher_handle(dispatcher_handle_t *h)
{
	h->dispatcherState = NULL;

	/* unlink from linked list first */
	if (h->prev)
		h->prev->next = h->next;
	else
		open_dispatcher_handles = h->next;
	if (h->next)
		h->next->prev = h->prev;

	pfree(h);

	if (open_dispatcher_handles == NULL)
		MemoryContextReset(DispatcherContext);
}

static dispatcher_handle_t *
find_dispatcher_handle(CdbDispatcherState *ds)
{
	dispatcher_handle_t *head = open_dispatcher_handles;
	while (head != NULL)
	{
		if (head->dispatcherState == ds)
			return head;
		head = head->next;
	}
	return NULL;
}

static void
cleanup_dispatcher_handle(dispatcher_handle_t *h)
{
	if (h->dispatcherState == NULL)
	{
		destroy_dispatcher_handle(h);
		return;
	}

	cdbdisp_cancelDispatch(h->dispatcherState);
	cdbdisp_destroyDispatcherState(h->dispatcherState);
}

static void
dispatcher_abort_callback(ResourceReleasePhase phase,
					   bool isCommit,
					   bool isTopLevel,
					   void *arg)
{
	dispatcher_handle_t *curr;
	dispatcher_handle_t *next;

	if (phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	next = open_dispatcher_handles;
	while (next)
	{
		curr = next;
		next = curr->next;

		if (curr->owner == CurrentResourceOwner)
		{
			if (isCommit)
				elog(WARNING, "dispatcher reference leak: %p still referenced", curr);

			cleanup_dispatcher_handle(curr);
		}
	}
}
