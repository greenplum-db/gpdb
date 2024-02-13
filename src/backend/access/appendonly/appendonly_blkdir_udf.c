/*------------------------------------------------------------------------------
 *
 * AppendOnly_Blkdir UDFs
 *   User-defined functions (UDF) for support of append-only block directory
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonly_blkdir_udf.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/appendonly_visimap.h"
#include "access/table.h"
#include "catalog/aoblkdir.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/cdbvars.h"
#include "funcapi.h"
#include "utils/snapmgr.h"

Datum gp_aoblkdir(PG_FUNCTION_ARGS);

/*
 * This UDF emits block directory entries for an AO/AOCO relation. It does so
 * by flattening the minipage column of ao_blkdir relations, yielding 1 minipage
 * entry / output row.
 *
 * Format:
 * tupleid | segno | columngroup_no | entry_no | first_row_no | file_offset | row_count
 *
 * This UDF also respects gp_select_invisible to report block directory entries
 * that are invisible. To determine invisible entries we can use the tupleid
 * projected here and tie it to the corresponding pg_aoblkdir tuple's xmax.
 */

Datum
gp_aoblkdir(PG_FUNCTION_ARGS)
{
	typedef struct Context
	{
		Relation 				aorel;
		SysScanDesc 			scan;
		MinipagePerColumnGroup	currMinipage;
		bool					currMinipageValid;
		int 					currMinipageEntryIdx;
		Relation				blkdirrel;
	} Context;

	Context    *context;
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	Oid			aoRelOid = PG_GETARG_OID(0);
	Tuplestorestate *tupstore;
	MemoryContext per_query_ctx;
	TupleDesc     tupdesc;
	MemoryContext oldcontext;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));

	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));

	/* Build a tuple descriptor for our result type */

	/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(7);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "tupleid",
					   TIDOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "segno",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "columngroup_no",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "entry_no",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5, "first_row_no",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 6, "file_offset",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 7, "row_count",
					   INT8OID, -1, 0);

	/*
	 * We don't require any special permission to see this function's data
	 * because nothing should be sensitive. The most critical being the slot
	 * name, which shouldn't contain anything particularly sensitive.
	 */

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	tupstore = tuplestore_begin_heap(true, false, work_mem);

	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	{
		Snapshot	sst;
		Oid			blkdirrelid;

		/* initialize Context for SRF */
		context = (Context *) palloc0(sizeof(Context));
		context->aorel = table_open(aoRelOid, AccessShareLock);
		if (!RelationStorageIsAO(context->aorel))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function not supported on non append-optimized relation")));
		sst = GetLatestSnapshot();
		GetAppendOnlyEntryAuxOids(context->aorel,
								  NULL, &blkdirrelid,
								  NULL);
		sst = gp_select_invisible ? SnapshotAny : GetLatestSnapshot();
		if (blkdirrelid == InvalidOid)
			ereport(ERROR,
					(errmsg("appendoptimized relation doesn't have a block directory"),
					 errhint("relation must have or must have had an index")));
		context->blkdirrel = table_open(blkdirrelid, AccessShareLock);
		context->scan = systable_beginscan(context->blkdirrel,
										   InvalidOid,
										   false,
										   sst,
										   0,
										   NULL);
		context->currMinipage.minipage = palloc0(minipage_size(NUM_MINIPAGE_ENTRIES));
		context->currMinipageValid = false;
		context->currMinipageEntryIdx = -1;

	}

	MemoryContextSwitchTo(oldcontext);

	for (;;)
	{
		if (!context->currMinipageValid)
		{
			Datum	minipage;
			bool	minipageNull;

			/* We need to fetch the next tuple from the blkdir relation */
			if (!systable_getnext(context->scan))
				break;

			/* deform the tuple and populate slot->values/nulls */
			slot_getallattrs(context->scan->slot);

			minipage = slot_getattr(context->scan->slot, Anum_pg_aoblkdir_minipage, &minipageNull);
			/*
			 * There should not really be any NULL values. We opt to report it
			 * instead of ERRORing out.
			 */
			context->currMinipageValid = !minipageNull;
			if (context->currMinipageValid)
			{
				/*
				 * Cache the latest scanned minipage and use it to emit the next
				 * (context->currMinipage->numMinipageEntries) rows
				 */
				copy_out_minipage(&context->currMinipage, minipage, false);
				context->currMinipageEntryIdx = 0;
			}
		}

		{
			Datum          	values[7];
			bool			nulls[7];
			TupleTableSlot 	*slot = context->scan->slot;
			Datum          	result;

			values[0] = ItemPointerGetDatum(&slot->tts_tid);
			nulls[0] = false;

			values[1] = slot_getattr(slot, Anum_pg_aoblkdir_segno, &nulls[1]);
			values[2] = slot_getattr(slot, Anum_pg_aoblkdir_columngroupno, &nulls[2]);

			/* emit minipage entry */
			if (context->currMinipageValid)
			{
				MinipagePerColumnGroup *currMinipage = &context->currMinipage;
				MinipageEntry *minipageEntry;

				Assert(context->currMinipageEntryIdx < currMinipage->numMinipageEntries);

				minipageEntry = &currMinipage->minipage->entry[context->currMinipageEntryIdx];

				values[3] = context->currMinipageEntryIdx++;
				values[4] = Int64GetDatum(minipageEntry->firstRowNum);
				values[5] = Int64GetDatum(minipageEntry->fileOffset);
				values[6] = Int64GetDatum(minipageEntry->rowCount);

				nulls[3] = false;
				nulls[4] = false;
				nulls[5] = false;
				nulls[6] = false;

				context->currMinipageValid =
					(context->currMinipageEntryIdx != currMinipage->numMinipageEntries);
			}
			else
			{
				nulls[3] = true;
				nulls[4] = true;
				nulls[5] = true;
				nulls[6] = true;
			}

			tuplestore_putvalues(tupstore, tupdesc, values, nulls);
		}


	}

	/* srf_done */
	tuplestore_donestoring(tupstore);

	table_close(context->aorel, AccessShareLock);
	systable_endscan(context->scan);
	table_close(context->blkdirrel, AccessShareLock);
	pfree(context);

	return (Datum) 0;
}
