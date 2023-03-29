/*-------------------------------------------------------------------------
 *
 * mcxtfuncs.c
 *	  Functions to show backend memory context.
 *
 * Portions Copyright (c) 1996-2020, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/mcxtfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "c.h"
#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "pgstat.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "utils/elog.h"
#include "utils/palloc.h"

/* ----------
 * An error code for segments to indicate that dispatched
 * memory context logging was not successful.
 * ----------
 */
#define ERROR_DISPATCHING_LOG_MEMORY_CONTEXT -99

/* ----------
 * The max bytes for showing identifiers of MemoryContext.
 * ----------
 */
#define MEMORY_CONTEXT_IDENT_DISPLAY_SIZE 1024

/*
 * PutMemoryContextsStatsTupleStore
 *		One recursion level for pg_get_backend_memory_contexts.
 */
static void
PutMemoryContextsStatsTupleStore(Tuplestorestate *tupstore,
								TupleDesc tupdesc, MemoryContext context,
								const char *parent, int level)
{
#define PG_GET_BACKEND_MEMORY_CONTEXTS_COLS	9

	Datum		values[PG_GET_BACKEND_MEMORY_CONTEXTS_COLS];
	bool		nulls[PG_GET_BACKEND_MEMORY_CONTEXTS_COLS];
	MemoryContextCounters stat;
	MemoryContext child;
	const char *name;
	const char *ident;

	AssertArg(MemoryContextIsValid(context));

	name = context->name;
	ident = context->ident;

	/*
	 * To be consistent with logging output, we label dynahash contexts
	 * with just the hash table name as with MemoryContextStatsPrint().
	 */
	if (ident && strcmp(name, "dynahash") == 0)
	{
		name = ident;
		ident = NULL;
	}

	/* Examine the context itself */
	memset(&stat, 0, sizeof(stat));
	(*context->methods->stats) (context, NULL, (void *) &level, &stat, true);

	memset(values, 0, sizeof(values));
	memset(nulls, 0, sizeof(nulls));

	if (name)
		values[0] = CStringGetTextDatum(name);
	else
		nulls[0] = true;

	if (ident)
	{
		int		idlen = strlen(ident);
		char		clipped_ident[MEMORY_CONTEXT_IDENT_DISPLAY_SIZE];

		/*
		 * Some identifiers such as SQL query string can be very long,
		 * truncate oversize identifiers.
		 */
		if (idlen >= MEMORY_CONTEXT_IDENT_DISPLAY_SIZE)
			idlen = pg_mbcliplen(ident, idlen, MEMORY_CONTEXT_IDENT_DISPLAY_SIZE - 1);

		memcpy(clipped_ident, ident, idlen);
		clipped_ident[idlen] = '\0';
		values[1] = CStringGetTextDatum(clipped_ident);
	}
	else
		nulls[1] = true;

	if (parent)
		values[2] = CStringGetTextDatum(parent);
	else
		nulls[2] = true;

	values[3] = Int32GetDatum(level);
	values[4] = Int64GetDatum(stat.totalspace);
	values[5] = Int64GetDatum(stat.nblocks);
	values[6] = Int64GetDatum(stat.freespace);
	values[7] = Int64GetDatum(stat.freechunks);
	values[8] = Int64GetDatum(stat.totalspace - stat.freespace);
	tuplestore_putvalues(tupstore, tupdesc, values, nulls);

	for (child = context->firstchild; child != NULL; child = child->nextchild)
	{
		PutMemoryContextsStatsTupleStore(tupstore, tupdesc,
								  child, name, level + 1);
	}
}

/*
 * pg_get_backend_memory_contexts
 *		SQL SRF showing backend memory context.
 */
Datum
pg_get_backend_memory_contexts(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not allowed in this context")));

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	PutMemoryContextsStatsTupleStore(tupstore, tupdesc,
								TopMemoryContext, NULL, 0);

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * pg_log_backend_memory_contexts
 *		Signal a backend process to log its memory contexts.
 *
 * By default, only superusers are allowed to signal to log the memory
 * contexts because allowing any users to issue this request at an unbounded
 * rate would cause lots of log messages and which can lead to denial of
 * service. Additional roles can be permitted with GRANT.
 *
 * On receipt of this signal, a backend sets the flag in the signal
 * handler, which causes the next CHECK_FOR_INTERRUPTS() to log the
 * memory contexts.
 */
Datum
pg_log_backend_memory_contexts(PG_FUNCTION_ARGS)
{
	int		pid = PG_GETARG_INT32(0);
	PGPROC		*proc = BackendPidGetProc(pid);

	/*
	 * BackendPidGetProc returns NULL if the pid isn't valid; but by the time
	 * we reach kill(), a process for which we get a valid proc here might
	 * have terminated on its own.  There's no way to acquire a lock on an
	 * arbitrary process to prevent that. But since this mechanism is usually
	 * used to debug a backend running and consuming lots of memory, that it
	 * might end on its own first and its memory contexts are not logged is
	 * not a problem.
	 */
	if (proc == NULL)
	{
		/*
		 * This is just a warning so a loop-through-resultset will not abort
		 * if one backend terminated on its own during the run.
		 */
		ereport(WARNING,
				(errmsg("PID %d is not a PostgreSQL server process", pid)));
		PG_RETURN_BOOL(false);
	}

	if (SendProcSignal(pid, PROCSIG_LOG_MEMORY_CONTEXT, proc->backendId) < 0)
	{
		/* Again, just a warning to allow loops */
		ereport(WARNING,
				(errmsg("could not send signal to process %d: %m", pid)));
		PG_RETURN_BOOL(false);
	}

	PG_RETURN_BOOL(true);
}

Datum
gp_log_backend_memory_contexts(PG_FUNCTION_ARGS)
{
	int			targetContentId;
	int			targetSessionId;
	ListCell		*dispSeg;
	ListCell		*retSeg;
	MemoryContext	per_func_ctx;
	MemoryContext	oldcontext;

	char			cmd[255];
	CdbPgResults	cdb_pgresults = {NULL, 0};
	int32			resultCount = 0;
	List			*dispatchedSegments = NIL;
	List			*returnedSegments = NIL;

	if (Gp_role == GP_ROLE_UTILITY)
	{
		ereport(ERROR,
				(errmsg("this function does not work in utility mode")));
		PG_RETURN_INT32(0);
	}

	per_func_ctx = AllocSetContextCreate(CurrentMemoryContext,
									   "gp_log_backend_memory temporary context",
									   ALLOCSET_DEFAULT_SIZES);
	oldcontext = MemoryContextSwitchTo(per_func_ctx);

	if (PG_NARGS() == 2)
	{
		targetSessionId = PG_GETARG_INT32(0);
		targetContentId = PG_GETARG_INT32(1);
	}
	else {
		targetSessionId = PG_GETARG_INT32(0);
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (PG_NARGS() == 2)
		{
			/* confirm that provided input is a valid segment */
			CdbComponentDatabases *cdbs = cdbcomponent_getCdbComponents();
			if (targetContentId < 0 || targetContentId >= cdbs->total_segments)
			{
				ereport(WARNING,
						(errmsg("\"%i\" is not a valid content ID",
								targetContentId)));
				PG_RETURN_INT16(resultCount);
			}

			sprintf(cmd, "select gp_log_backend_memory_contexts(%d,%d)",
					targetSessionId, targetContentId);
			dispatchedSegments = list_make1_int(targetContentId);
		}
		else
		{
			sprintf(cmd, "select gp_log_backend_memory_contexts(%d)", targetSessionId);
			dispatchedSegments = cdbcomponent_getCdbComponentsList();
		}
		CdbDispatchCommandToSegments(cmd, DF_NONE, dispatchedSegments, &cdb_pgresults);


		/*
		 * collect all results from dispatch, so we can check which segments
		 * didn't report success
		 */
		for (int resultno = 0; resultno < cdb_pgresults.numResults; resultno++)
		{
			struct pg_result *pgresult = cdb_pgresults.pg_results[resultno];
			if (PQresultStatus(pgresult) == PGRES_TUPLES_OK)
			{
				int retValue = pg_atoi(PQgetvalue(pgresult, 0, 0), sizeof(int), 0);
				if (retValue != ERROR_DISPATCHING_LOG_MEMORY_CONTEXT)
					returnedSegments = lappend_int(returnedSegments, retValue);
			}
		}

		/* Warn for all segments where we didn't get a positive response */
		foreach(dispSeg, dispatchedSegments)
		{
			bool foundResponse = false;
			int dispSegNo = lfirst_int(dispSeg);
			foreach(retSeg, returnedSegments)
			{
				int retSegNo = lfirst_int(retSeg);
				if (dispSegNo == retSegNo)
				{
					foundResponse = true;
					break;
				}
			}

			if (!foundResponse)
				ereport(WARNING,
					(errmsg("unable to log memory contexts for session: \"%i\", on contentID: \"%i\"",
							targetSessionId, dispSegNo)));
			else
				resultCount++;
		}

		cdbdisp_clearCdbPgResults(&cdb_pgresults);
		MemoryContextSwitchTo(oldcontext);
		PG_RETURN_INT16(resultCount);
	}
	else	/* Gp_role == EXECUTE */
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);

		int tot_backends = pgstat_fetch_stat_numbackends();
		bool errorDetected = false;
		int logsSignalled = 0;
		for (int beid = 1; beid <= tot_backends; beid++)
		{
			PgBackendStatus *beentry = pgstat_fetch_stat_beentry(beid);
			if (beentry && beentry->st_procpid >0 &&
				beentry->st_session_id == targetSessionId)
			{
				bool success = DatumGetBool(DirectFunctionCall1(pg_log_backend_memory_contexts, beentry->st_procpid));
				if (success)
					logsSignalled++;
				else
					errorDetected = true;
			}
		}
		MemoryContextSwitchTo(oldcontext);
		if (errorDetected || logsSignalled == 0)
			PG_RETURN_INT16(ERROR_DISPATCHING_LOG_MEMORY_CONTEXT);
		else
			PG_RETURN_INT16(GpIdentity.segindex);
	}
}
