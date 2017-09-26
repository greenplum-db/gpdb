/*------------------------------------------------------------------------------
 *
 * appendonly_visimap
 *   maintain a visibility bitmap for all rows in a
 *   minipage.
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonly_visimap_store.c
 *
 *------------------------------------------------------------------------------
*/
#include "postgres.h"
#include "access/genam.h"
#include "catalog/aovisimap.h"
#include "catalog/indexing.h"
#include "access/appendonly_visimap_store.h"
#include "parser/parse_oper.h"
#include "utils/lsyscache.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"

#define APPENDONLY_VISIMAP_INDEX_SCAN_KEY_NUM 2

/*
 * Frees the data allocated by the visimap store
 *
 * No function using the visibility map store should be called
 * after this function call.
 */
void
AppendOnlyVisimapStore_Finish(
							  AppendOnlyVisimapStore *visiMapStore,
							  LOCKMODE lockmode)
{
	if (visiMapStore->scanKeys)
	{
		pfree(visiMapStore->scanKeys);
		visiMapStore->scanKeys = NULL;
	}

	index_close(visiMapStore->visimapIndex, lockmode);
	heap_close(visiMapStore->visimapRelation, lockmode);
}

/*
 * Inits the visimap store.
 * The store is ready for usage after this function call.
 *
 * Assumes a zero-allocated visimap store data structure.
 * Assumes that the visimap memory context is active.
 */
void
AppendOnlyVisimapStore_Init(
							AppendOnlyVisimapStore *visiMapStore,
							Oid visimapRelid,
							Oid visimapIdxid,
							LOCKMODE lockmode,
							Snapshot snapshot,
							MemoryContext memoryContext)
{
	TupleDesc	heapTupleDesc;
	ScanKey		scanKey;

	Assert(visiMapStore);
	Assert(CurrentMemoryContext == memoryContext);
	Assert(OidIsValid(visimapRelid));
	Assert(OidIsValid(visimapIdxid));

	visiMapStore->snapshot = snapshot;
	visiMapStore->memoryContext = memoryContext;

	visiMapStore->visimapRelation = heap_open(
											  visimapRelid, lockmode);
	visiMapStore->visimapIndex = index_open(
											visimapIdxid, lockmode);

	heapTupleDesc =
		RelationGetDescr(visiMapStore->visimapRelation);
	Assert(heapTupleDesc->natts == Natts_pg_aovisimap);

	visiMapStore->scanKeys = palloc0(sizeof(ScanKeyData) * APPENDONLY_VISIMAP_INDEX_SCAN_KEY_NUM);

	/* scan key: segno */
	scanKey = visiMapStore->scanKeys;
	ScanKeyInit(scanKey,
				Anum_pg_aovisimap_segno,	/* segno */
				BTEqualStrategyNumber,
				F_INT4EQ,
				0);

	/* scan key: firstRowNum */
	scanKey++;
	ScanKeyInit(scanKey,
				Anum_pg_aovisimap_firstrownum,	/* attribute number to scan */
				BTEqualStrategyNumber,	/* strategy */
				F_INT8EQ,		/* reg proc to use */
				0);
}

/*
 * Stores the visibility map entry.
 *
 * The entry/tuple is invalidated after this function call.
 *
 * Assumes that a valid visimap entry is passed.
 * Assumes that the entry corresponds to the latest tuple
 * returned by AppendOnlyVisimapStore_find.
 *
 * Should not be called twice in the same command.
 */
void
AppendOnlyVisimapStore_Store(
							 AppendOnlyVisimapStore *visiMapStore,
							 AppendOnlyVisimapEntry *visiMapEntry)
{
	MemoryContext oldContext;
	Relation	visimapRelation;
	TupleDesc	heapTupleDesc;
	HeapTuple	tuple;
	Datum		values[Natts_pg_aovisimap];
	bool		nulls[Natts_pg_aovisimap];

	Assert(visiMapStore);
	Assert(visiMapEntry);

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map store: Store visimap entry: "
		   "(segFileNum, firstRowNum) = (%u, " INT64_FORMAT ")",
		   visiMapEntry->segmentFileNum, visiMapEntry->firstRowNum);

	oldContext = MemoryContextSwitchTo(visiMapStore->memoryContext);

	AppendOnlyVisimapEntry_Write(visiMapEntry, values,
								 nulls);

	visimapRelation = visiMapStore->visimapRelation;
	heapTupleDesc = RelationGetDescr(visimapRelation);
	tuple = heap_form_tuple(heapTupleDesc,
							values,
							nulls);

	/*
	 * Write out the visimap entry to the relation. If this visimap entry
	 * already in the relation, we update the row. Otherwise, a new row is
	 * inserted.
	 */
	if (ItemPointerIsValid(&visiMapEntry->tupleTid))
	{
		simple_heap_update(visimapRelation, &visiMapEntry->tupleTid, tuple);
	}
	else
	{
		simple_heap_insert(visimapRelation, tuple);
	}

	CatalogUpdateIndexes(visimapRelation, tuple);

	heap_freetuple(tuple);

	MemoryContextSwitchTo(oldContext);

	/* Invalidate the data after storing it. */
	ItemPointerSetInvalid(&visiMapEntry->tupleTid);
}

/**
 * Finds the visibility map entry tuple for a given
 * segmentFileNum and firstRowNum.
 *
 * Note: The firstRowNum needs to be a valid firstRowNum. It is
 * especially not the tuple id of the append-only tuple checked, updated,
 * or deleted.
 *
 * Returns true if there is such a tuple and
 * the tuple is used as current tuple.
 * Otherwise false is returned.
 *
 * Assumes that the store data structure has been initialized, but not finished.
 */
bool
AppendOnlyVisimapStore_Find(
							AppendOnlyVisimapStore *visiMapStore,
							int32 segmentFileNum,
							int64 firstRowNum,
							AppendOnlyVisimapEntry *visiMapEntry)
{
	ScanKey		scanKeys;
	IndexScanDesc indexScan;

	Assert(visiMapStore);
	Assert(visiMapEntry);
	Assert(RelationIsValid(visiMapStore->visimapRelation));
	Assert(RelationIsValid(visiMapStore->visimapIndex));

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map store: Load entry: "
		   "(segFileNum, firstRowNum) = (%u, " INT64_FORMAT ")",
		   segmentFileNum, firstRowNum);

	scanKeys = visiMapStore->scanKeys;
	scanKeys[0].sk_argument = Int32GetDatum(segmentFileNum);
	scanKeys[1].sk_argument = Int64GetDatum(firstRowNum);

	indexScan = AppendOnlyVisimapStore_BeginScan(
												 visiMapStore,
												 APPENDONLY_VISIMAP_INDEX_SCAN_KEY_NUM,
												 scanKeys);

	if (!AppendOnlyVisimapStore_GetNext(
										visiMapStore,
										indexScan,
										BackwardScanDirection,
										visiMapEntry,
										&visiMapEntry->tupleTid))
	{
		elogif(Debug_appendonly_print_visimap, LOG,
			   "Append-only visi map store: Visimap entry does not exist: "
			   "(segFileNum, firstRowNum) = (%u, " INT64_FORMAT ")",
			   segmentFileNum, firstRowNum);

		/* failed to lookup row */
		AppendOnlyVisimapStore_EndScan(visiMapStore, indexScan);
		return false;
	}
	AppendOnlyVisimapStore_EndScan(visiMapStore, indexScan);
	return true;
}

/*
 * Fetches the next entry from a visimap store index scan.
 *
 * It is the responsibility of the caller to decode the return value
 * correctly.
 *
 */
static HeapTuple
AppendOnlyVisimapStore_GetNextTuple(
									AppendOnlyVisimapStore *visiMapStore,
									IndexScanDesc indexScan,
									ScanDirection scanDirection)
{
	Assert(visiMapStore);
	Assert(RelationIsValid(visiMapStore->visimapRelation));
	Assert(RelationIsValid(visiMapStore->visimapIndex));
	Assert(indexScan);

	return index_getnext(indexScan, scanDirection);
}



/*
 * Fetches the next entry from a visimap store index scan.
 *
 * Parameter visiMapEntry may be NULL. If it is not NULL and
 * the scan returns an entry, the entry data is copied to the
 * visimapEntry.
 * Parameter tupleTid may be NULL. If it is not NULL and the scan
 * returns an entry, the (heap) tuple id is copied to the parameter.
 */
bool
AppendOnlyVisimapStore_GetNext(
							   AppendOnlyVisimapStore *visiMapStore,
							   IndexScanDesc indexScan,
							   ScanDirection scanDirection,
							   AppendOnlyVisimapEntry *visiMapEntry,
							   ItemPointerData *tupleTid)
{
	HeapTuple	tuple;
	TupleDesc	heapTupleDesc;

	Assert(visiMapStore);
	Assert(RelationIsValid(visiMapStore->visimapRelation));
	Assert(RelationIsValid(visiMapStore->visimapIndex));
	Assert(indexScan);

	tuple = AppendOnlyVisimapStore_GetNextTuple(visiMapStore, indexScan, scanDirection);
	if (tuple == NULL)
	{
		return false;
	}
	heapTupleDesc = RelationGetDescr(visiMapStore->visimapRelation);
	if (visiMapEntry)
	{
		AppendOnlyVisimapEntry_Copyout(visiMapEntry, tuple,
									   heapTupleDesc);
	}
	if (tupleTid)
	{
		ItemPointerCopy(&tuple->t_self, tupleTid);
	}
	return true;
}

/*
 * Deletes all visibility map information from a given
 * segment file.
 */
void
AppendOnlyVisimapStore_DeleteSegmentFile(
										 AppendOnlyVisimapStore *visiMapStore,
										 int segmentFileNum)
{
	ScanKeyData scanKey;
	IndexScanDesc indexScan;
	ItemPointerData tid;

	Assert(visiMapStore);
	Assert(RelationIsValid(visiMapStore->visimapRelation));
	Assert(RelationIsValid(visiMapStore->visimapIndex));

	elogif(Debug_appendonly_print_visimap, LOG,
		   "Append-only visi map store: Delete segment file: "
		   "(segFileNum) = (%u)", segmentFileNum);

	ScanKeyInit(&scanKey,
				Anum_pg_aovisimap_segno,	/* segno */
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(segmentFileNum));

	indexScan = AppendOnlyVisimapStore_BeginScan(
												 visiMapStore,
												 1,
												 &scanKey);

	while (AppendOnlyVisimapStore_GetNext(visiMapStore,
										  indexScan,
										  ForwardScanDirection,
										  NULL,
										  &tid))
	{
		simple_heap_delete(visiMapStore->visimapRelation,
						   &tid);
	}
	AppendOnlyVisimapStore_EndScan(visiMapStore, indexScan);
}

/*
 * Returns the number of hidden tuples in a given segment file
 */
int64
AppendOnlyVisimapStore_GetSegmentFileHiddenTupleCount(
													  AppendOnlyVisimapStore *visiMapStore,
													  AppendOnlyVisimapEntry *visiMapEntry,
													  int segmentFileNum)
{
	ScanKeyData scanKey;
	IndexScanDesc indexScan;
	int64		hiddenTupcount = 0;

	Assert(visiMapStore);
	Assert(visiMapEntry);
	Assert(RelationIsValid(visiMapStore->visimapRelation));
	Assert(RelationIsValid(visiMapStore->visimapIndex));

	ScanKeyInit(&scanKey,
				Anum_pg_aovisimap_segno,	/* segno */
				BTEqualStrategyNumber,
				F_INT4EQ,
				Int32GetDatum(segmentFileNum));

	indexScan = AppendOnlyVisimapStore_BeginScan(
												 visiMapStore,
												 1,
												 &scanKey);

	while (AppendOnlyVisimapStore_GetNext(visiMapStore,
										  indexScan, ForwardScanDirection,
										  visiMapEntry, NULL))
	{
		hiddenTupcount += AppendOnlyVisimapEntry_GetHiddenTupleCount(visiMapEntry);
	}
	AppendOnlyVisimapStore_EndScan(visiMapStore, indexScan);
	return hiddenTupcount;
}

/*
 * Returns the number of hidden tuples in a given releation
 */
int64
AppendOnlyVisimapStore_GetRelationHiddenTupleCount(
												   AppendOnlyVisimapStore *visiMapStore,
												   AppendOnlyVisimapEntry *visiMapEntry)
{
	IndexScanDesc indexScan;
	int64		hiddenTupcount = 0;

	Assert(visiMapStore);
	Assert(visiMapEntry);
	Assert(RelationIsValid(visiMapStore->visimapRelation));
	Assert(RelationIsValid(visiMapStore->visimapIndex));

	indexScan = AppendOnlyVisimapStore_BeginScan(
												 visiMapStore,
												 0,
												 NULL);

	while (AppendOnlyVisimapStore_GetNext(visiMapStore,
										  indexScan, ForwardScanDirection,
										  visiMapEntry, NULL))
	{
		hiddenTupcount += AppendOnlyVisimapEntry_GetHiddenTupleCount(visiMapEntry);
	}
	AppendOnlyVisimapStore_EndScan(visiMapStore, indexScan);
	return hiddenTupcount;
}

/*
 * Starts a scan over the visimap store.
 *
 * Parameter keys may be NULL iff nkeys is zero.
 */
IndexScanDesc
AppendOnlyVisimapStore_BeginScan(
								 AppendOnlyVisimapStore *visiMapStore,
								 int nkeys,
								 ScanKey keys)
{
	Assert(visiMapStore);
	Assert(RelationIsValid(visiMapStore->visimapRelation));

	return index_beginscan(
						   visiMapStore->visimapRelation,
						   visiMapStore->visimapIndex,
						   visiMapStore->snapshot,
						   nkeys,
						   keys);
}

/*
 * Ends a index scan over the visimap store.
 */
void
AppendOnlyVisimapStore_EndScan(
							   AppendOnlyVisimapStore *visiMapStore,
							   IndexScanDesc indexScan)
{
	Assert(visiMapStore);
	Assert(indexScan);

	index_endscan(indexScan);
}
