/*---------------------------------------------------------------------
 *
 * gp_session_state_memory_stats.c
 *
 * The dynamically linked library created from this source can be reference by
 * creating a function in psql that references it. For example,
 *
 * CREATE FUNCTION gp_session_state_memory_entries_f()
 *	RETURNS SETOF record
 *	AS '$libdir/gp_session_state', 'gp_session_state_memory_entries'
 *	LANGUAGE C IMMUTABLE;
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    contrib/gp_internal_tools/gp_session_state_memory_stats.c
 *
 *---------------------------------------------------------------------
 */

#include "postgres.h"
#include "funcapi.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "utils/session_state.h"
#include "utils/vmem_tracker.h"
#include "miscadmin.h"

/* The number of columns as defined in gp_session_state_memory_stats view */
#define NUM_SESSION_STATE_MEMORY_ELEM 10

Datum gp_session_state_memory_entries(PG_FUNCTION_ARGS);

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(gp_session_state_memory_entries);

/*
 * Function returning memory entries for each session
 */
Datum
gp_session_state_memory_entries(PG_FUNCTION_ARGS)
{

	FuncCallContext *funcctx;
	int32 *sessionIndexPtr;

	if (SRF_IS_FIRSTCALL())
	{
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* Switch to memory context appropriate for multiple function calls */
		MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* Build a tuple descriptor for our result type. */
		TupleDesc tupdesc = CreateTemplateTupleDesc(NUM_SESSION_STATE_MEMORY_ELEM, false /* hasoid */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segid",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "sessionid",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "vmem_mb",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "runaway_status",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "qe_count",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "active_qe_count",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "dirty_qe_count",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "runaway_vmem_mb",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "runaway_command_cnt",
				INT4OID, -1 /* typmod */, 0 /* attdim */);

		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "idle_start",
				TIMESTAMPTZOID, -1 /* typmod */, 0 /* attdim */);

		Assert(NUM_SESSION_STATE_MEMORY_ELEM == 10);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		sessionIndexPtr = (int32 *) palloc(sizeof(*sessionIndexPtr));
		*sessionIndexPtr = 0;

		funcctx->user_fctx = sessionIndexPtr;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	sessionIndexPtr = (int32 *) funcctx->user_fctx;

	while (true)
	{
		if (*sessionIndexPtr >= AllSessionStateEntries->maxSession)
		{
			/* Reached the end of the entry array, we're done */
			SRF_RETURN_DONE(funcctx);
		}

		SessionState sessionState = AllSessionStateEntries->sessions[*sessionIndexPtr];
		*sessionIndexPtr = *sessionIndexPtr + 1;

		if (SessionState_IsAcquired(&sessionState))
		{
			Datum		values[NUM_SESSION_STATE_MEMORY_ELEM];
			bool		nulls[NUM_SESSION_STATE_MEMORY_ELEM];
			MemSet(nulls, 0, sizeof(nulls));

			values[0] = Int32GetDatum(Gp_segment);
			values[1] = Int32GetDatum(sessionState.sessionId);
			values[2] = Int32GetDatum(VmemTracker_ConvertVmemChunksToMB(sessionState.sessionVmem));
			values[3] = Int32GetDatum(sessionState.runawayStatus);
			values[4] = Int32GetDatum(sessionState.pinCount);
			values[5] = Int32GetDatum(sessionState.activeProcessCount);
			values[6] = Int32GetDatum(sessionState.cleanupCountdown);
			values[7] = Int32GetDatum(sessionState.sessionVmemRunaway);
			values[8] = Int32GetDatum(sessionState.commandCountRunaway);
			values[9] = TimestampTzGetDatum(sessionState.idle_start);

			HeapTuple tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
			Datum result = HeapTupleGetDatum(tuple);
			SRF_RETURN_NEXT(funcctx, result);
		}
	}
}
