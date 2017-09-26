/*------------------------------------------------------------------------------
 *
 * AppendOnlyVisimap UDFs
 *   User-defined functions (UDF) for support of append-only visimap
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonly_visimap_udf.c
 *
 *------------------------------------------------------------------------------
*/
#include "postgres.h"
#include "access/appendonly_visimap.h"
#include "access/appendonly_visimap_entry.h"
#include "access/appendonly_visimap_store.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly_fn.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "funcapi.h"
#include "utils/builtins.h"


extern Datum gp_aovisimap(PG_FUNCTION_ARGS);
extern Datum gp_aovisimap_name(PG_FUNCTION_ARGS);

static Datum
gp_aovisimap_internal(PG_FUNCTION_ARGS, Oid aoRelOid)
{
	Datum		values[3];
	bool		nulls[3];
	HeapTuple	tuple;
	Datum		result;

	typedef struct Context
	{
		Relation	aorel;
		AppendOnlyVisimapScan visiMapScan;
		AOTupleId	aoTupleId;

	} Context;

	FuncCallContext *funcctx;
	Context    *context;

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
		tupdesc = CreateTemplateTupleDesc(3, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "tid",
						   TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "row_num",
						   INT8OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc0(sizeof(Context));

		context->aorel = heap_open(aoRelOid, AccessShareLock);
		if (!(RelationIsAoRows(context->aorel) || RelationIsAoCols(context->aorel)))
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Function not supported on relation")));
		}

		AppendOnlyVisimapScan_Init(&context->visiMapScan,
								   context->aorel->rd_appendonly->visimaprelid,
								   context->aorel->rd_appendonly->visimapidxid,
								   AccessShareLock,
								   SnapshotNow);
		AOTupleIdInit_Init(&context->aoTupleId);

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	while (true)
	{
		if (!AppendOnlyVisimapScan_GetNextInvisible(
													&context->visiMapScan,
													&context->aoTupleId))
		{
			break;
		}
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		values[0] = ItemPointerGetDatum((ItemPointer) &context->aoTupleId);
		values[1] = Int32GetDatum(AOTupleIdGet_segmentFileNum(&context->aoTupleId));
		values[2] = Int64GetDatum(AOTupleIdGet_rowNum(&context->aoTupleId));

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}

	AppendOnlyVisimapScan_Finish(&context->visiMapScan, AccessShareLock);
	heap_close(context->aorel, AccessShareLock);
	pfree(context);
	funcctx->user_fctx = NULL;
	SRF_RETURN_DONE(funcctx);
}

Datum
gp_aovisimap(PG_FUNCTION_ARGS)
{
	Oid			relid;

	relid = PG_GETARG_OID(0);
	return gp_aovisimap_internal(fcinfo, relid);
}

Datum
gp_aovisimap_name(PG_FUNCTION_ARGS)
{
	RangeVar   *parentrv;
	text	   *relname = PG_GETARG_TEXT_P(0);
	Oid			relid;

	parentrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
	relid = RangeVarGetRelid(parentrv, false);

	return gp_aovisimap_internal(fcinfo, relid);
}


extern Datum gp_aovisimap_hidden_info(PG_FUNCTION_ARGS);
extern Datum gp_aovisimap_hidden_info_name(PG_FUNCTION_ARGS);

static Datum
gp_aovisimap_hidden_info_internal(PG_FUNCTION_ARGS, Oid aoRelOid)
{
	Datum		values[3];
	bool		nulls[3];
	HeapTuple	tuple;
	Datum		result;

	typedef struct Context
	{
		AppendOnlyVisimap visiMap;

		Relation	parentRelation;

		FileSegInfo **appendonlySegfileInfo;
		AOCSFileSegInfo **aocsSegfileInfo;
		int			segfile_info_total;

		int			i;
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

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
		tupdesc = CreateTemplateTupleDesc(3, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "hidden_tupcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "total_tupcount",
						   INT8OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc0(sizeof(Context));

		context->parentRelation = heap_open(aoRelOid, AccessShareLock);
		if (!(RelationIsAoRows(context->parentRelation) || RelationIsAoCols(context->parentRelation)))
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Function not supported on relation")));
		}

		if (RelationIsAoRows(context->parentRelation))
		{
			context->appendonlySegfileInfo = GetAllFileSegInfo(
															   context->parentRelation,
															   SnapshotNow,
															   &context->segfile_info_total);
		}
		else
		{
			Assert(RelationIsAoCols(context->parentRelation));
			context->aocsSegfileInfo = GetAllAOCSFileSegInfo(context->parentRelation,
															 SnapshotNow, &context->segfile_info_total);
		}
		context->i = 0;

		AppendOnlyVisimap_Init(&context->visiMap,
							   context->parentRelation->rd_appendonly->visimaprelid,
							   context->parentRelation->rd_appendonly->visimapidxid,
							   AccessShareLock,
							   SnapshotNow);

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	while (context->i < context->segfile_info_total)
	{
		int64		tupcount;
		int			segno;

		if (context->appendonlySegfileInfo)
		{
			FileSegInfo *fsinfo = context->appendonlySegfileInfo[context->i];

			tupcount = fsinfo->total_tupcount;
			segno = fsinfo->segno;
		}
		else if (context->aocsSegfileInfo)
		{
			AOCSFileSegInfo *fsinfo = context->aocsSegfileInfo[context->i];

			tupcount = fsinfo->total_tupcount;
			segno = fsinfo->segno;
		}
		else
		{
			Insist(false);
		}

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		values[0] = Int32GetDatum(segno);
		values[1] = Int64GetDatum(AppendOnlyVisimap_GetSegmentFileHiddenTupleCount(
																				   &context->visiMap, segno));
		values[2] = Int64GetDatum(tupcount);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		context->i++;
		SRF_RETURN_NEXT(funcctx, result);
	}

	AppendOnlyVisimap_Finish(&context->visiMap, AccessShareLock);
	if (context->appendonlySegfileInfo)
	{
		FreeAllSegFileInfo(context->appendonlySegfileInfo, context->segfile_info_total);
		pfree(context->appendonlySegfileInfo);
		context->appendonlySegfileInfo = NULL;
	}
	if (context->aocsSegfileInfo)
	{
		FreeAllAOCSSegFileInfo(context->aocsSegfileInfo, context->segfile_info_total);
		pfree(context->aocsSegfileInfo);
		context->aocsSegfileInfo = NULL;

	}
	heap_close(context->parentRelation, AccessShareLock);
	pfree(context);
	funcctx->user_fctx = NULL;

	SRF_RETURN_DONE(funcctx);
}

Datum
gp_aovisimap_hidden_info(PG_FUNCTION_ARGS)
{
	Oid			aoRelOid = PG_GETARG_OID(0);

	return gp_aovisimap_hidden_info_internal(fcinfo, aoRelOid);
}

Datum
gp_aovisimap_hidden_info_name(PG_FUNCTION_ARGS)
{
	RangeVar   *parentrv;
	text	   *relname = PG_GETARG_TEXT_P(0);
	Oid			relid;

	parentrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
	relid = RangeVarGetRelid(parentrv, false);

	return gp_aovisimap_hidden_info_internal(fcinfo, relid);
}

extern Datum gp_aovisimap_entry(PG_FUNCTION_ARGS);
extern Datum gp_aovisimap_entry_name(PG_FUNCTION_ARGS);

static void
gp_aovisimap_encode_bitmap(char *bitmapBuffer, Bitmapset *bms)
{
	int			i;
	int			last = -1;

	Assert(bitmapBuffer);

	memset(bitmapBuffer, '0', APPENDONLY_VISIMAP_MAX_RANGE + 1);
	bitmapBuffer[APPENDONLY_VISIMAP_MAX_RANGE] = 0;

	i = bms_first_from(bms, 0);
	while (i >= 0)
	{
		last = i;
		Assert(i < APPENDONLY_VISIMAP_MAX_RANGE);
		bitmapBuffer[i] = '1';
		i = bms_first_from(bms, i + 1);
	}
	bitmapBuffer[last + 1] = 0;
}

static Datum
gp_aovisimap_entry_internal(PG_FUNCTION_ARGS, Oid aoRelOid)
{
	Datum		values[4];
	bool		nulls[4];
	HeapTuple	tuple;
	Datum		result;

	typedef struct Context
	{
		AppendOnlyVisimap visiMap;

		Relation	parentRelation;

		IndexScanDesc indexScan;

		text	   *bitmapBuffer;
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

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
		tupdesc = CreateTemplateTupleDesc(4, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "first_row_num",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "hidden_tupcount",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "bitmap",
						   TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc0(sizeof(Context));

		context->parentRelation = heap_open(aoRelOid, AccessShareLock);
		if (!(RelationIsAoRows(context->parentRelation) || RelationIsAoCols(context->parentRelation)))
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Function not supported on relation")));
		}

		AppendOnlyVisimap_Init(&context->visiMap,
							   context->parentRelation->rd_appendonly->visimaprelid,
							   context->parentRelation->rd_appendonly->visimapidxid,
							   AccessShareLock,
							   SnapshotNow);

		context->indexScan = AppendOnlyVisimapStore_BeginScan(&
															  context->visiMap.visimapStore, 0, NULL);

		context->bitmapBuffer = palloc0(VARHDRSZ + APPENDONLY_VISIMAP_MAX_RANGE + 1);

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	if (AppendOnlyVisimapStore_GetNext(&context->visiMap.visimapStore,
									   context->indexScan,
									   ForwardScanDirection,
									   &context->visiMap.visimapEntry,
									   NULL))
	{
		AppendOnlyVisimapEntry *visimapEntry = &context->visiMap.visimapEntry;

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		values[0] = Int32GetDatum(visimapEntry->segmentFileNum);
		values[1] = Int64GetDatum(visimapEntry->firstRowNum);
		values[2] = Int32GetDatum(
								  (int32) AppendOnlyVisimapEntry_GetHiddenTupleCount(visimapEntry));

		gp_aovisimap_encode_bitmap(VARDATA(context->bitmapBuffer),
								   visimapEntry->bitmap);
		SET_VARSIZE(context->bitmapBuffer, APPENDONLY_VISIMAP_MAX_RANGE);
		values[3] = PointerGetDatum(context->bitmapBuffer);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}

	AppendOnlyVisimapStore_EndScan(&context->visiMap.visimapStore,
								   context->indexScan);
	AppendOnlyVisimap_Finish(&context->visiMap, AccessShareLock);
	heap_close(context->parentRelation, AccessShareLock);

	pfree(context->bitmapBuffer);
	pfree(context);
	funcctx->user_fctx = NULL;

	SRF_RETURN_DONE(funcctx);
}

Datum
gp_aovisimap_entry(PG_FUNCTION_ARGS)
{
	Oid			aoRelOid = PG_GETARG_OID(0);

	return gp_aovisimap_entry_internal(fcinfo, aoRelOid);
}

Datum
gp_aovisimap_entry_name(PG_FUNCTION_ARGS)
{
	RangeVar   *parentrv;
	text	   *relname = PG_GETARG_TEXT_P(0);
	Oid			relid;

	parentrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
	relid = RangeVarGetRelid(parentrv, false);

	return gp_aovisimap_entry_internal(fcinfo, relid);
}
