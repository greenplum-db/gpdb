/*-------------------------------------------------------------------------
 *
 * gp_transaction_log.c
 *		Set-returning function to view gp_transaction_log table.
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
#include "access/clog.h"
#include "access/distributedlog.h"
#include "access/transam.h"
#include "cdb/cdbvars.h"                /* Gp_segment */

Datum		gp_transaction_log(PG_FUNCTION_ARGS);

/*
 * pgdatabasev - produce a view of gp_transaction_log that combines 
 * information from the local clog and the distributed log.
 */
Datum
gp_transaction_log(PG_FUNCTION_ARGS)
{
	typedef struct Context
	{
		TransactionId		indexXid;
	} Context;
	
	FuncCallContext *funcctx;
	Context *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function
		 * calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		/* this had better match gp_distributed_xacts view in system_views.sql */
		tupdesc = CreateTemplateTupleDesc(4, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "dbid",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "transaction",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "status",
						   TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->indexXid = ShmemVariableCache->nextXid;
												// Start with last possible + 1.

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	/*
	 * Go backwards until we don't find a clog log page
	 */
	while (true)
	{
		XidStatus 	status;
		char 		*statusStr = NULL;
		Datum		values[4];
		bool		nulls[4];
		HeapTuple	tuple;
		Datum		result;

		if (context->indexXid < FirstNormalTransactionId)
			break;
		
		if (!CLOGScanForPrevStatus(&context->indexXid,
								   &status))
			break;

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int16GetDatum((int16)Gp_segment);
		values[1] = Int16GetDatum((int16)GpIdentity.dbid);
		values[2] = TransactionIdGetDatum(context->indexXid);

		if (status == TRANSACTION_STATUS_IN_PROGRESS)
			statusStr = "InProgress";
		else if (status == TRANSACTION_STATUS_COMMITTED)
			statusStr = "Committed";
		else if (status == TRANSACTION_STATUS_ABORTED)
			statusStr = "Aborted";
		else if (status == TRANSACTION_STATUS_SUB_COMMITTED)
			statusStr = "SubXactCommitted";
		else
			elog(ERROR, "Unexpected transaction status %d",
			     status);

		values[3] = CStringGetTextDatum(statusStr);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}


