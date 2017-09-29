/*-------------------------------------------------------------------------
 *
 * cdbdistributedxacts.c
 *		Set-returning function to view gp_distributed_xacts table.
 *
 * IDENTIFICATION
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbtm.h"

Datum		gp_distributed_xacts__(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_distributed_xacts__);
/*
 * pgdatabasev - produce a view of gp_distributed_xacts to include transient state
 */
Datum
gp_distributed_xacts__(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	TMGALLXACTSTATUS *allDistributedXactStatus;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		/* this had better match gp_distributed_xacts view in system_views.sql */
		tupdesc = CreateTemplateTupleDesc(5, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "distributed_xid",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "distributed_id",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "state",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "gp_session_id",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "xmin_distributed_snapshot",
						   XIDOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		getAllDistributedXactStatus(&allDistributedXactStatus);
		funcctx->user_fctx = (void *) allDistributedXactStatus;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	allDistributedXactStatus = (TMGALLXACTSTATUS *) funcctx->user_fctx;

	while (true)
	{
		TMGXACTSTATUS *distributedXactStatus;

		Datum		values[6];
		bool		nulls[6];
		HeapTuple	tuple;
		Datum		result;

		if (!getNextDistributedXactStatus(allDistributedXactStatus,
										  &distributedXactStatus))
			break;

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = TransactionIdGetDatum(distributedXactStatus->gxid);
		values[1] = CStringGetTextDatum(distributedXactStatus->gid);
		values[2] = CStringGetTextDatum(DtxStateToString(distributedXactStatus->state));

		values[3] = UInt32GetDatum(distributedXactStatus->sessionId);
		values[4] = TransactionIdGetDatum(distributedXactStatus->xminDistributedSnapshot);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}
