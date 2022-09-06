/*-------------------------------------------------------------------------
 *
 * xlogfuncs_gp.c
 *
 * GPDB-specific transaction log manager user interface functions
 *
 * This file contains WAL control and information functions.
 *
 * Portions Copyright (c) 2017-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/access/transam/xlogfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xlog_fn.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "funcapi.h"
#include "libpq-fe.h"

#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "utils/faultinjector.h"

/*
 * gp_create_restore_point_internal: a distributed named point for cluster restore
 */
Datum
gp_create_restore_point_internal(PG_FUNCTION_ARGS)
{

	typedef struct Context
	{
		CdbPgResults cdb_pgresults;
		XLogRecPtr	qd_restorepoint_lsn;
		int			index;
	}			Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		text	   *restore_name = PG_GETARG_TEXT_P(0);
		char	   *restore_name_str;
		char	   *restore_command;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context for appropriate multiple function call */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* create tupdesc for result */
		tupdesc = CreateTemplateTupleDesc(2, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "restore_lsn",
						   LSNOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		context = (Context *) palloc(sizeof(Context));
		context->cdb_pgresults.pg_results =
			(struct pg_result **) palloc(getgpsegmentCount() * sizeof(struct pg_result *));
		context->index = 0;
		funcctx->user_fctx = (void *) context;

		if (!IS_QUERY_DISPATCHER())
			elog(ERROR,
				 "cannot use gp_create_restore_point() when not in QD mode");

		restore_name_str = text_to_cstring(restore_name);
		restore_command =
			psprintf("SELECT pg_catalog.pg_create_restore_point(%s)", quote_literal_cstr(restore_name_str));

		/*
		 * Acquire TwophaseCommitLock in EXCLUSIVE mode. This is to ensure
		 * cluster-wide restore point consistency by blocking distributed
		 * commit prepared broadcasts from concurrent twophase transactions
		 * where a QE segment has written WAL.
		 */
		LWLockAcquire(TwophaseCommitLock, LW_EXCLUSIVE);

		SIMPLE_FAULT_INJECTOR("gp_create_restore_point_acquired_lock");

		CdbDispatchCommand(restore_command,
						   DF_NEED_TWO_PHASE | DF_CANCEL_ON_ERROR,
						   &context->cdb_pgresults);
		context->qd_restorepoint_lsn = DatumGetLSN(DirectFunctionCall1(pg_create_restore_point,
														   PointerGetDatum(restore_name)));
		LWLockRelease(TwophaseCommitLock);

		pfree(restore_command);

		funcctx->user_fctx = (void *) context;
		MemoryContextSwitchTo(oldcontext);
	}

	/*
	 * Using SRF to return all the segment LSN information of the form
	 * {segment_id, restore_lsn}
	 */
	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	while (context->index <= context->cdb_pgresults.numResults)
	{
		Datum		values[2];
		bool		nulls[2];
		HeapTuple	tuple;
		Datum		result;
		XLogRecPtr	restore_ptr;
		int			seg_index;
		uint32		hi;
		uint32		lo;

		if (context->index == 0)
		{
			/* Setting fields representing QD's restore point */
			seg_index = GpIdentity.segindex;
			restore_ptr = context->qd_restorepoint_lsn;
		}
		else
		{
			/* Setting fields representing QE's restore point */
			seg_index = context->index - 1;
			struct pg_result *pgresult = context->cdb_pgresults.pg_results[seg_index];
			ExecStatusType resultStatus = PQresultStatus(pgresult);

			if (resultStatus != PGRES_COMMAND_OK && resultStatus != PGRES_TUPLES_OK)
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 (errmsg("could not get the restore point from segment"),
						  errdetail("%s", PQresultErrorMessage(pgresult)))));
			Assert(PQntuples(pgresult) == 1);
			sscanf(PQgetvalue(pgresult, 0, 0), "%X/%X", &hi, &lo);
			restore_ptr = ((uint64) hi) << 32 | lo;
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int16GetDatum(seg_index);
		values[1] = LSNGetDatum(restore_ptr);
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		context->index++;
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

/*
 * gp_switch_wal_internal: switch WAL on all segments and return meaningful info
 */
Datum
gp_switch_wal_internal(PG_FUNCTION_ARGS)
{
	typedef struct Context
	{
		CdbPgResults cdb_pgresults;
		Datum qd_switch_lsn;
		Datum qd_switch_walfilename;
		int index;
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;
		TupleDesc		tupdesc;
		char			*switch_command;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context for appropriate multiple function call */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* create tupdesc for result */
		tupdesc = CreateTemplateTupleDesc(3, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "switch_lsn",
						   LSNOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "switch_walfilename",
						   TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		context = (Context *) palloc(sizeof(Context));
		context->cdb_pgresults.pg_results = NULL;
		context->cdb_pgresults.numResults = 0;
		context->index = 0;
		funcctx->user_fctx = (void *) context;

		if (!IS_QUERY_DISPATCHER() || Gp_role != GP_ROLE_DISPATCH)
			elog(ERROR,
				 "cannot use gp_switch_wal() when not in QD mode");

		switch_command = psprintf("SELECT switch_lsn, pg_xlogfile_name(switch_lsn) FROM pg_catalog.pg_switch_xlog() switch_lsn");
		CdbDispatchCommand(switch_command,
						   DF_NEED_TWO_PHASE | DF_CANCEL_ON_ERROR,
						   &context->cdb_pgresults);
		context->qd_switch_lsn = DatumGetLSN(DirectFunctionCall1(pg_switch_xlog, PointerGetDatum(NULL)));
		context->qd_switch_walfilename = DirectFunctionCall1(pg_xlogfile_name, context->qd_switch_lsn);

		pfree(switch_command);

		funcctx->user_fctx = (void *) context;
		MemoryContextSwitchTo(oldcontext);
	}

	/*
	 * Using SRF to return all the segment LSN information of the form
	 * {segment_id, switch_lsn, switch_walfilename}
	 */
	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	while (context->index <= context->cdb_pgresults.numResults)
	{
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;
		Datum		result;
		Datum		switch_lsn;
		Datum		switch_walfilename;
		int			seg_index;

		if (context->index == 0)
		{
			/* Setting fields representing QD's switch WAL */
			seg_index = GpIdentity.segindex;
			switch_lsn = context->qd_switch_lsn;
			switch_walfilename = context->qd_switch_walfilename;
		}
		else
		{
			struct pg_result	*pgresult;
			ExecStatusType		resultStatus;
			uint32				hi, lo;

			/* Setting fields representing QE's switch WAL */
			seg_index = context->index - 1;
			pgresult = context->cdb_pgresults.pg_results[seg_index];
			resultStatus = PQresultStatus(pgresult);

			if (resultStatus != PGRES_COMMAND_OK && resultStatus != PGRES_TUPLES_OK)
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 (errmsg("could not switch wal from segment"),
						  errdetail("%s", PQresultErrorMessage(pgresult)))));
			Assert(PQntuples(pgresult) == 1);

			sscanf(PQgetvalue(pgresult, 0, 0), "%X/%X", &hi, &lo);
			switch_lsn = LSNGetDatum(((uint64) hi) << 32 | lo);
			switch_walfilename = CStringGetTextDatum(PQgetvalue(pgresult, 0, 1));
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int16GetDatum(seg_index);
		values[1] = switch_lsn;
		values[2] = switch_walfilename;
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		context->index++;
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}
