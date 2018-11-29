/*
 * Copyright (c) 2011 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
 */

/* ---------------------------------------------------------------------
 *
 * Interface to gp_ao_co_diagnostic functions.
 *
 * This file contains functions that are wrappers around their corresponding GP
 * internal functions located in the postgres backend executable. The GP
 * internal functions can not be called directly from outside
 *
 * Internal functions can not be called directly from outside the postrgres
 * backend executable without defining them in the catalog tables. The wrapper
 * functions defined in this file are compiled and linked into an library, which
 * can then be called as a user defined function. The wrapper functions will
 * call the actual internal functions.
 *
 * ---------------------------------------------------------------------
 */

#include "postgres.h"
#include <string.h>
#include "fmgr.h"
#include "funcapi.h"

#include "cdb/cdbvars.h"
#include "access/appendonlywriter.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum
gp_aoseg_history(PG_FUNCTION_ARGS);

extern Datum
gp_aocsseg(PG_FUNCTION_ARGS);

extern Datum
gp_aoseg(PG_FUNCTION_ARGS);

extern Datum
gp_aocsseg_history(PG_FUNCTION_ARGS);

extern Datum
gp_aovisimap(PG_FUNCTION_ARGS);

extern Datum
gp_aovisimap_entry(PG_FUNCTION_ARGS);

extern Datum
gp_aovisimap_hidden_info(PG_FUNCTION_ARGS);

extern Datum
gp_remove_ao_entry_from_cache(PG_FUNCTION_ARGS);

extern Datum
gp_get_ao_entry_from_cache(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_aoseg_history_wrapper);
PG_FUNCTION_INFO_V1(gp_aoseg_wrapper);
PG_FUNCTION_INFO_V1(gp_aocsseg_wrapper);
PG_FUNCTION_INFO_V1(gp_aocsseg_history_wrapper);
PG_FUNCTION_INFO_V1(gp_aovisimap_wrapper);
PG_FUNCTION_INFO_V1(gp_aovisimap_entry_wrapper);
PG_FUNCTION_INFO_V1(gp_aovisimap_hidden_info_wrapper);
PG_FUNCTION_INFO_V1(gp_remove_ao_entry_from_cache);
PG_FUNCTION_INFO_V1(gp_get_ao_entry_from_cache);

extern Datum
gp_aoseg_history_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aoseg_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aocsseg_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aocsseg_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aocsseg_history_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aovisimap_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aovisimap_entry_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aovisimap_hidden_info_wrapper(PG_FUNCTION_ARGS);

/* ---------------------------------------------------------------------
 * Interface to gp_aoseg_history_wrapper function.
 *
 * The gp_aoseg_history_wrapper function is a wrapper around the gp_aoseg_history
 * function. It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aoseg_history(oid)
 *   RETURNS TABLE ( gp_tid tid
 *                 , gp_xmin integer
 *                 , gp_xmin_status text
 *                 , gp_xmin_commit_distrib_id text
 *                 , gp_xmax integer
 *                 , gp_xmax_status text
 *                 , gp_xmax_commit_distrib_id text
 *                 , gp_command_id integer
 *                 , gp_infomask text
 *                 , gp_update_tid tid
 *                 , gp_visibility text
 *                 , segno integer
 *                 , tupcount bigint
 *                 , eof bigint
 *                 , eof_uncompressed bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aoseg_history_wrapper' LANGUAGE C STRICT;
 *
 */
Datum
gp_aoseg_history_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aoseg_history(fcinfo);

  PG_RETURN_DATUM(returnValue);
}


/* ---------------------------------------------------------------------
 * Interface to gp_aocsseg_wrapper function.
 *
 * The gp_aocsseg_wrapper function is a wrapper around the gp_aocsseg function.
 * It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aocsseg(regclass)
 *   RETURNS TABLE ( gp_tid tid
 *                 , segno integer
 *                 , column_num smallint
 *                 , physical_segno integer
 *                 , tupcount bigint
 *                 , eof bigint
 *                 , eof_uncompressed bigint
 *                 , modcount bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aocsseg_wrapper' LANGUAGE C STRICT;
 *
 */
Datum
gp_aocsseg_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aocsseg(fcinfo);

  PG_RETURN_DATUM(returnValue);
}

/* ---------------------------------------------------------------------
 * Interface to gp_aoseg_wrapper function.
 *
 * The gp_wrapper function is a wrapper around the gp_aoseg function.
 * It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aoseg(regclass)
 *   RETURNS TABLE ( 
 *                 segno integer
 *                 , eof bigint
 *                 , tupcount bigint
 *                 , varblockcount bigint
 *                 , eof_uncompressed bigint
 *                 , modcount bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aoseg_wrapper' LANGUAGE C STRICT;
 *
 */
Datum
gp_aoseg_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aoseg(fcinfo);

  PG_RETURN_DATUM(returnValue);
}

/* ---------------------------------------------------------------------
 * Interface to gp_aocsseg_history_wrapper function.
 *
 * The gp_aocsseg_history_wrapper function is a wrapper around the gp_aocsseg_history
 * function. It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aocsseg_history(oid)
 *   RETURNS TABLE ( gp_tid tid
 *                 , gp_xmin integer
 *                 , gp_xmin_status text
 *                 , gp_xmin_distrib_id text
 *                 , gp_xmax integer
 *                 , gp_xmax_status text
 *                 , gp_xmax_distrib_id text
 *                 , gp_command_id integer
 *                 , gp_infomask text
 *                 , gp_update_tid tid
 *                 , gp_visibility text
 *                 , segno integer
 *                 , column_num smallint
 *                 , physical_segno integer
 *                 , tupcount bigint
 *                 , eof bigint
 *                 , eof_uncompressed bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aocsseg_history_wrapper' LANGUAGE C STRICT;
 *
 */
Datum
gp_aocsseg_history_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aocsseg_history(fcinfo);

  PG_RETURN_DATUM(returnValue);
}

/* 
 * Interface to gp_aovisimap_wrapper function.
 *
 * CREATE FUNCTION gp_aovisimap(regclass) RETURNS TABLE 
 * (tid tid, segno integer, row_num bigint) 
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aovisimap_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aovisimap_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aovisimap(fcinfo);

	PG_RETURN_DATUM(returnValue);
}

/* 
 * Interface to gp_aovisimap_hidden_info_wrapper function.
 *
 * CREATE FUNCTION gp_aovisimap_hidden_info(regclass) RETURNS TABLE 
 * (segno integer, hidden_tupcount bigint, total_tupcount bigint) 
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aovisimap_hidden_info_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aovisimap_hidden_info_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aovisimap_hidden_info(fcinfo);

	PG_RETURN_DATUM(returnValue);
}

/* 
 * Interface to gp_aovisimap_entry_wrapper function.
 *
 * CREATE FUNCTION gp_aovisimap_entry(regclass) RETURNS TABLE 
 * (segno integer, hidden_tupcount bigint, total_tupcount bigint) 
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aovisimap_entry_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aovisimap_entry_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aovisimap_entry(fcinfo);

	PG_RETURN_DATUM(returnValue);
}

static void not_query_dispatcher_error() {
	elog(ERROR, "ao entries are maintained only on query dispatcher");
}


/*
 * Interface to remove an entry from AppendOnlyHash cache.
 */
Datum
gp_remove_ao_entry_from_cache(PG_FUNCTION_ARGS)
{
	if (!IS_QUERY_DISPATCHER())
		not_query_dispatcher_error();

	Oid relid = PG_GETARG_OID(0);

	GpRemoveEntryFromAppendOnlyHash(relid);

	PG_RETURN_DATUM(0);
}

/*
 * Interface to obtain details of an entry from AppendOnlyHash cache.
 */
struct GetAOEntryContext
{
	int segno;
	AORelHashEntryData aoentry;
};

Datum
gp_get_ao_entry_from_cache(PG_FUNCTION_ARGS)
{
	if (!IS_QUERY_DISPATCHER())
		not_query_dispatcher_error();

	Oid relid = PG_GETARG_OID(0);
	FuncCallContext *funcctx;
	struct GetAOEntryContext *context;

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
#define NUM_ATTRS 9
		tupdesc = CreateTemplateTupleDesc(NUM_ATTRS, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segno",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "total_tupcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "tuples_added",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "inserting_transaction",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "latest_committed_inserting_dxid",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "state",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "format_version",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "is_full",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "aborted",
						   BOOLOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		context = palloc(sizeof(struct GetAOEntryContext));
		context->segno = 0;
		GpFetchEntryFromAppendOnlyHash(relid, &context->aoentry);

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
		elog(NOTICE, "transactions using relid %d: %d",
			 relid, context->aoentry.txns_using_rel);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (struct GetAOEntryContext *) funcctx->user_fctx;

	if (context->segno < MAX_AOREL_CONCURRENCY)
	{
		Datum		values[NUM_ATTRS];
		bool		nulls[NUM_ATTRS];
		HeapTuple	tuple;
		Datum		result;

		AOSegfileStatus *aosegfile =
			&(context->aoentry.relsegfiles[context->segno]);

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int16GetDatum(context->segno);
		values[1] = Int64GetDatum(aosegfile->total_tupcount);
		values[2] = Int64GetDatum(aosegfile->tupsadded);
		values[3] = TransactionIdGetDatum(aosegfile->xid);
		values[4] = TransactionIdGetDatum(aosegfile->latestWriteXid);
		values[5] = Int16GetDatum(aosegfile->state);
		values[6] = Int16GetDatum(aosegfile->formatversion);
		values[7] = BoolGetDatum(aosegfile->isfull);
		values[8] = BoolGetDatum(aosegfile->aborted);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		context->segno++;
		SRF_RETURN_NEXT(funcctx, result);
	}
	SRF_RETURN_DONE(funcctx);
}
