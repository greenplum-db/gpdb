/*-------------------------------------------------------------------------
 *
 * dbsize_gp.c
 *
 * GPDB-specific database object size functions, and related inquiries
 *
 * Portions Copyright (c) 2017-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/utils/adt/dbsize_gp.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "libpq-fe.h"
#include "utils/builtins.h"

#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"
#include "fmgr_gp.h"

/*
 * gp_relation_filepath: get the entire file path name of the relation on all
 * segments
 */
Datum
gp_relation_filepath(PG_FUNCTION_ARGS)
{
	typedef struct Context
	{
		CdbPgResults cdb_pgresults;
		Datum qd_filepath;
		bool isnull;
		int index;
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc		tupdesc;
		MemoryContext	oldcontext;
		Oid				relid = PG_GETARG_OID(0);
		char		   *filepath_command;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context for appropriate multiple function call */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* create tupdesc for result */
		tupdesc = CreateTemplateTupleDesc(2);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "filepath",
						   TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		context = (Context *) palloc(sizeof(Context));
		context->cdb_pgresults.pg_results = NULL;
		context->cdb_pgresults.numResults = 0;
		context->index = 0;
		funcctx->user_fctx = (void *) context;

		if (!IS_QUERY_DISPATCHER() || Gp_role != GP_ROLE_DISPATCH)
			elog(ERROR,
				 "cannot use gp_relation_filepath() when not in QD mode");

		filepath_command = psprintf("SELECT filepath FROM pg_catalog.pg_relation_filepath(%u) filepath",
									relid);
		CdbDispatchCommand(filepath_command,
						   DF_CANCEL_ON_ERROR,
						   &context->cdb_pgresults);
		context->qd_filepath = DirectNullFunctionCall1(pg_relation_filepath,
													   ObjectIdGetDatum(relid),
													   &context->isnull);

		pfree(filepath_command);

		funcctx->user_fctx = (void *) context;
		MemoryContextSwitchTo(oldcontext);
	}

	/*
	 * Using SRF to return all the filepath information of the form
	 * {segment_id, filepath}
	 */
	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	while (context->index <= context->cdb_pgresults.numResults)
	{
		Datum		values[2];
		bool		nulls[2];
		HeapTuple	tuple;
		Datum		result;
		Datum		filepath;
		bool		isnull = false;
		int			seg_index;

		if (context->index == 0)
		{
			/* Setting fields representing QD's filepath */
			seg_index = GpIdentity.segindex;
			filepath = context->qd_filepath;
			isnull = context->isnull;
		}
		else
		{
			struct pg_result	*pgresult;
			ExecStatusType		resultStatus;

			/* Setting fields representing QE's filepath */
			seg_index = context->index - 1;
			pgresult = context->cdb_pgresults.pg_results[seg_index];
			resultStatus = PQresultStatus(pgresult);

			if (resultStatus != PGRES_COMMAND_OK && resultStatus != PGRES_TUPLES_OK)
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 (errmsg("could not get file path"),
						  errdetail("%s", PQresultErrorMessage(pgresult)))));
			Assert(PQntuples(pgresult) == 1);

			if (PQgetisnull(pgresult, 0, 0))
				isnull = true;
			else
				filepath = CStringGetTextDatum(PQgetvalue(pgresult, 0, 0));
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int16GetDatum(seg_index);
		if (isnull)
			nulls[1] = true;
		else
			values[1] = filepath;
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		context->index++;
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}
