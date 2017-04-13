/*-------------------------------------------------------------------------
 *
 * gp_fastsequence.c
 *    routines to maintain a light-weight sequence table.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/gp_fastsequence.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/gp_fastsequence.h"
#include "catalog/indexing.h"
#include "utils/relcache.h"
#include "utils/fmgroids.h"
#include "access/genam.h"
#include "access/htup.h"
#include "access/heapam.h"
#include "utils/syscache.h"
#include "access/appendonlywriter.h"

static void insert_or_update_fastsequence(
	Relation gp_fastsequence_rel,
	HeapTuple oldTuple,
	TupleDesc tupleDesc,
	Oid objid,
	int64 objmod,
	int64 newLastSequence);

/*
 * gp_fastsequence is used to generate and keep track of row numbers for AO
 * and CO tables. Row numbers for AO/CO tables act as a component to form TID,
 * stored in index tuples and used during index scans to lookup intended
 * tuple. Hence this number must be monotonically incrementing value. Also
 * should not rollback irrespective of insert/update transaction aborting for
 * AO/CO table, as reusing row numbers even across aborted transactions would
 * yield wrong results for index scans. Also, entries in gp_fastsequence must
 * only exist for lifespan of the corresponding table.
 *
 * Given those special needs, this function inserts 1 initial rows to
 * fastsequence for segfile 1. Only segfile 1 can be used to insert tuples within
 * same transaction creating the table hence initial entry is only created for
 * these. Entries for rest of segfiles will get created with frozenXids during
 * inserts. These entries are inserted while creating the AO/CO table to
 * leverage MVCC to clear out gp_fastsequence entries incase of
 * aborts/failures. All future calls to insert_or_update_fastsequence() for
 * objmod 0 or objmod 1 will perform inplace updates to these tuples.
 */
void
InsertInitialFastSequenceEntries(Oid objid)
{
	Relation gp_fastsequence_rel;
	TupleDesc tupleDesc;
	Datum *values;
	bool *nulls;
	HeapTuple tuple = NULL;

	/*
	 * Open and lock the gp_fastsequence catalog table.
	 */
	gp_fastsequence_rel = heap_open(FastSequenceRelationId, RowExclusiveLock);
	tupleDesc = RelationGetDescr(gp_fastsequence_rel);

	values = palloc0(sizeof(Datum) * tupleDesc->natts);
	nulls = palloc0(sizeof(bool) * tupleDesc->natts);

	values[Anum_gp_fastsequence_objid - 1] = ObjectIdGetDatum(objid);
	values[Anum_gp_fastsequence_last_sequence - 1] = Int64GetDatum(0);

	/* Insert entry for segfile 1 */
	values[Anum_gp_fastsequence_objmod - 1] = Int64GetDatum(1);
	tuple = heaptuple_form_to(tupleDesc, values, nulls, NULL, NULL);
	simple_heap_insert(gp_fastsequence_rel, tuple);
	CatalogUpdateIndexes(gp_fastsequence_rel, tuple);
	heap_freetuple(tuple);

	heap_close(gp_fastsequence_rel, RowExclusiveLock);
}

/*
 * InsertFastSequenceEntry
 *
 * Insert a new fast sequence entry for a given object. If the given
 * object already exists in the table, this function replaces the old
 * entry with a fresh initial value.
 */
void
InsertFastSequenceEntry(Oid objid, int64 objmod, int64 lastSequence)
{
	Relation gp_fastsequence_rel;
	ScanKeyData scankey[2];
	SysScanDesc scan;
	TupleDesc tupleDesc;
	HeapTuple tuple = NULL;
	
	/*
	 * Open and lock the gp_fastsequence catalog table.
	 */
	gp_fastsequence_rel = heap_open(FastSequenceRelationId, RowExclusiveLock);
	tupleDesc = RelationGetDescr(gp_fastsequence_rel);
	
	/* SELECT * FROM gp_fastsequence WHERE objid = :1 AND objmod = :2 FOR UPDATE */
	ScanKeyInit(&scankey[0],
				Anum_gp_fastsequence_objid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(objid));
	ScanKeyInit(&scankey[1],
				Anum_gp_fastsequence_objmod,
				BTEqualStrategyNumber, F_INT8EQ,
				Int64GetDatum(objmod));
	scan = systable_beginscan(gp_fastsequence_rel, FastSequenceObjidObjmodIndexId, true,
							  SnapshotNow, 2, scankey);

	tuple = systable_getnext(scan);
	insert_or_update_fastsequence(gp_fastsequence_rel,
						tuple,
						tupleDesc,
						objid,
						objmod,
						lastSequence);
	systable_endscan(scan);

	/*
	 * gp_fastsequence table locking for AO inserts uses bottom up approach
	 * meaning the locks are first acquired on the segments and later on the
	 * master.
	 * Hence, it is essential that we release the lock here to avoid
	 * any form of master-segment resource deadlock. E.g. A transaction
	 * trying to reindex gp_fastsequence has acquired a lock on it on the
	 * master but is blocked on the segment as another transaction which
	 * is an insert operation has acquired a lock first on segment and is
	 * trying to acquire a lock on the Master. Deadlock!
	 */
	heap_close(gp_fastsequence_rel, RowExclusiveLock);
}

/*
 * insert or update the existing fast sequence number for (objid, objmod).
 *
 * If such an entry exists in the table, it is provided in oldTuple. This tuple
 * is updated with the new value. Otherwise, a new tuple is inserted into the
 * table.
 */
static void
insert_or_update_fastsequence(Relation gp_fastsequence_rel,
					HeapTuple oldTuple,
					TupleDesc tupleDesc,
					Oid objid,
					int64 objmod,
					int64 newLastSequence)
{
	Datum *values;
	bool *nulls;
	HeapTuple newTuple;

	values = palloc0(sizeof(Datum) * tupleDesc->natts);
	nulls = palloc0(sizeof(bool) * tupleDesc->natts);

	/*
	 * If such a tuple does not exist, insert a new one.
	 */
	if (!HeapTupleIsValid(oldTuple))
	{
		values[Anum_gp_fastsequence_objid - 1] = ObjectIdGetDatum(objid);
		values[Anum_gp_fastsequence_objmod - 1] = Int64GetDatum(objmod);
		values[Anum_gp_fastsequence_last_sequence - 1] = Int64GetDatum(newLastSequence);

		newTuple = heaptuple_form_to(tupleDesc, values, nulls, NULL, NULL);

		frozen_heap_insert(gp_fastsequence_rel, newTuple);
		CatalogUpdateIndexes(gp_fastsequence_rel, newTuple);

		heap_freetuple(newTuple);
	}
	else
	{
#ifdef USE_ASSERT_CHECKING
		Oid oldObjid;
		int64 oldObjmod;
		bool isNull;
		
		oldObjid = heap_getattr(oldTuple, Anum_gp_fastsequence_objid, tupleDesc, &isNull);
		Assert(!isNull);
		oldObjmod = heap_getattr(oldTuple, Anum_gp_fastsequence_objmod, tupleDesc, &isNull);
		Assert(!isNull);
		Assert(oldObjid == objid && oldObjmod == objmod);
#endif

		values[Anum_gp_fastsequence_objid - 1] = ObjectIdGetDatum(objid);
		values[Anum_gp_fastsequence_objmod - 1] = Int64GetDatum(objmod);
		values[Anum_gp_fastsequence_last_sequence - 1] = Int64GetDatum(newLastSequence);

		newTuple = heap_form_tuple(tupleDesc, values, nulls);
		newTuple->t_data->t_ctid = oldTuple->t_data->t_ctid;
		newTuple->t_self = oldTuple->t_self;
		if (tupleDesc->tdhasoid)
			HeapTupleSetOid(newTuple, HeapTupleGetOid(oldTuple));
		heap_inplace_update(gp_fastsequence_rel, newTuple);

		heap_freetuple(newTuple);
	}
	
	pfree(values);
	pfree(nulls);
}

/*
 * GetFastSequences
 *
 * Get a list of consecutive sequence numbers. The starting sequence
 * number is the maximal value between 'lastsequence' + 1 and minSequence.
 * The length of the list is given.
 *
 * If there is not such an entry for objid in the table, create
 * one here.
 *
 * The existing entry for objid in the table is updated with a new
 * lastsequence value.
 */
int64 GetFastSequences(Oid objid, int64 objmod,
					   int64 minSequence, int64 numSequences)
{
	Relation gp_fastsequence_rel;
	ScanKeyData scankey[2];
	SysScanDesc scan;
	TupleDesc tupleDesc;
	HeapTuple tuple;
	int64 firstSequence = minSequence;
	Datum lastSequenceDatum;
	int64 newLastSequence;

	gp_fastsequence_rel = heap_open(FastSequenceRelationId, RowExclusiveLock);
	tupleDesc = RelationGetDescr(gp_fastsequence_rel);

	/*
	 * SELECT * FROM gp_fastsequence
	 * WHERE objid = :1 AND objmod = :2
	 * FOR UPDATE
	 */
	ScanKeyInit(&scankey[0],
				Anum_gp_fastsequence_objid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(objid));
	ScanKeyInit(&scankey[1],
				Anum_gp_fastsequence_objmod,
				BTEqualStrategyNumber, F_INT8EQ,
				Int64GetDatum(objmod));
	scan = systable_beginscan(gp_fastsequence_rel, FastSequenceObjidObjmodIndexId, true,
							  SnapshotNow, 2, scankey);

	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
	{
		newLastSequence = firstSequence + numSequences - 1;
	}
	else
	{
		bool isNull;

		lastSequenceDatum = heap_getattr(tuple, Anum_gp_fastsequence_last_sequence,
										tupleDesc, &isNull);
		
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got an invalid lastsequence number: NULL")));
		
		if (DatumGetInt64(lastSequenceDatum) + 1 > firstSequence)
			firstSequence = DatumGetInt64(lastSequenceDatum) + 1;
		newLastSequence = firstSequence + numSequences - 1;
	}

	insert_or_update_fastsequence(gp_fastsequence_rel, tuple, tupleDesc,
						objid, objmod, newLastSequence);

	systable_endscan(scan);
		
	/* Refer to the comment at the end of InsertFastSequenceEntry. */
	heap_close(gp_fastsequence_rel, RowExclusiveLock);

	return firstSequence;
}

/*
 * RemoveFastSequenceEntry
 *
 * Remove all entries associated with the given object id.
 *
 * If the given objid is an invalid OID, this function simply
 * returns.
 *
 * It is okay for the given valid objid to have no entries in
 * gp_fastsequence.
 */
void
RemoveFastSequenceEntry(Oid objid)
{
	Relation	rel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;

	if (!OidIsValid(objid))
		return;

	rel = heap_open(FastSequenceRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_gp_fastsequence_objid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(objid));

	sscan = systable_beginscan(rel, FastSequenceObjidObjmodIndexId, true,
							   SnapshotNow, 1, &scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(rel, &tuple->t_self);
	}

	systable_endscan(sscan);
	heap_close(rel, RowExclusiveLock);
}
