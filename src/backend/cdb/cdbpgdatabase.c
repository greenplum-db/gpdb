/*-------------------------------------------------------------------------
 *
 * cdbpgdatabase.c
 *		Set-returning function to view pgdatabase table.
 *
 * IDENTIFICATION
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "cdb/cdbutil.h"

Datum		gp_pgdatabase__(PG_FUNCTION_ARGS);

/* Working status for pg_prepared_xact */
typedef struct
{
	CdbComponentDatabases *cdb_component_dbs;
	int			currIdx;
} Working_State;

PG_FUNCTION_INFO_V1(gp_pgdatabase__);

/*
 * pgdatabasev - produce a view of pgdatabase to include transient state
 */
Datum
gp_pgdatabase__(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	Working_State *mystatus;
	CdbComponentDatabases *cdb_dbs;

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
		/* this had better match pg_prepared_xacts view in	system_views.sql */
		tupdesc = CreateTemplateTupleDesc(5, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "dbid",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "isprimary",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "content",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "valid",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "definedprimary",
						   BOOLOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		mystatus = (Working_State *) palloc(sizeof(Working_State));
		funcctx->user_fctx = (void *) mystatus;

		mystatus->cdb_component_dbs = cdbcomponent_getCdbComponents();
		mystatus->currIdx = 0;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	mystatus = (Working_State *) funcctx->user_fctx;
	cdb_dbs = mystatus->cdb_component_dbs;

	while (cdb_dbs != NULL && mystatus->currIdx < cdb_dbs->total_segment_dbs + cdb_dbs->total_entry_dbs)
	{
		Datum		values[6];
		bool		nulls[6];
		HeapTuple	tuple;
		Datum		result;
		CdbComponentDatabaseInfo *db;

		if (mystatus->currIdx < cdb_dbs->total_entry_dbs)
			db = &cdb_dbs->entry_db_info[mystatus->currIdx];
		else
			db = &cdb_dbs->segment_db_info[mystatus->currIdx - cdb_dbs->total_entry_dbs];
		mystatus->currIdx++;

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = UInt16GetDatum(db->config->dbid);
		values[1] = BoolGetDatum(SEGMENT_IS_ACTIVE_PRIMARY(db));
		values[2] = UInt16GetDatum(db->config->segindex);

		values[3] = BoolGetDatum(false);
		if (db->config->status == GP_SEGMENT_CONFIGURATION_STATUS_UP)
		{
			if (db->config->mode == GP_SEGMENT_CONFIGURATION_MODE_INSYNC ||
				db->config->mode == GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC)
			{
				values[3] = BoolGetDatum(true);
			}
		}
		values[4] = BoolGetDatum(db->config->preferred_role ==
								 GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}
