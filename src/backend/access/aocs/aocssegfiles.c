/* 
 * AOCS Segment files.
 *
 * Copyright (c) 2009, Greenplum Inc.
 */

#include "postgres.h"

#include "cdb/cdbappendonlystorage.h"
#include "access/aomd.h"
#include "access/heapam.h"
#include "access/genam.h"
#include "access/hio.h"
#include "access/multixact.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/valid.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/pg_appendonly.h"
#include "catalog/namespace.h"
#include "catalog/indexing.h"
#include "catalog/gp_fastsequence.h"
#include "cdb/cdbvars.h"
#include "executor/spi.h"
#include "nodes/makefuncs.h"
#include "utils/acl.h"
#include "utils/syscache.h"
#include "utils/numeric.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "utils/datumstream.h"
#include "utils/fmgroids.h"
#include "access/aocssegfiles.h"
#include "access/aosegfiles.h"
#include "access/appendonlywriter.h"
#include "cdb/cdbaocsam.h"
#include "executor/executor.h"

#include "utils/debugbreak.h"
#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"

static void PrintPgaocssegAndGprelationNodeEntries(AOCSFileSegInfo **allseginfo,
												int totalsegs,
												bool *segmentFileNumMap);

static void CheckCOConsistencyWithGpRelationNode(Snapshot snapshot,
												Relation rel,
												int totalsegs,
												AOCSFileSegInfo ** allseginfo);

AOCSFileSegInfo *
NewAOCSFileSegInfo(int4 segno, int4 nvp)
{
	AOCSFileSegInfo *seginfo;

	seginfo = (AOCSFileSegInfo *) palloc0(aocsfileseginfo_size(nvp));
	seginfo->segno = segno;
	seginfo->vpinfo.nEntry = nvp;
	seginfo->state = AOSEG_STATE_DEFAULT;

	return seginfo;
}

void InsertInitialAOCSFileSegInfo(Oid segrelid, int4 segno, int4 nvp)
{
    bool *nulls = palloc0(sizeof(bool) * Natts_pg_aocsseg);
    Datum *values = palloc0(sizeof(Datum) * Natts_pg_aocsseg);
    AOCSVPInfo *vpinfo = create_aocs_vpinfo(nvp);
    HeapTuple segtup;
	Relation segrel;

    segrel = heap_open(segrelid, RowExclusiveLock);

	InsertFastSequenceEntry(segrelid,
							(int64)segno,
							0);

    values[Anum_pg_aocs_segno-1] = Int32GetDatum(segno);
    values[Anum_pg_aocs_vpinfo-1] = PointerGetDatum(vpinfo);
	values[Anum_pg_aocs_tupcount-1] = Int64GetDatum(0);
	values[Anum_pg_aocs_varblockcount-1] = Int64GetDatum(0);
	values[Anum_pg_aocs_state-1] = Int16GetDatum(AOSEG_STATE_DEFAULT);

    segtup = heap_form_tuple(RelationGetDescr(segrel), values, nulls);

    frozen_heap_insert(segrel, segtup);
    CatalogUpdateIndexes(segrel, segtup);

    heap_freetuple(segtup);
    heap_close(segrel, RowExclusiveLock);

    pfree(vpinfo);
    pfree(nulls);
    pfree(values);
}

/* 
 * GetAOCSFileSegInfo.
 * 
 * Get the catalog entry for an appendonly (column-oriented) relation from the
 * pg_aocsseg_* relation that belongs to the currently used
 * AppendOnly table.
 *
 * If a caller intends to append to this (logical) file segment entry they must
 * already hold a relation Append-Only segment file (transaction-scope) lock (tag 
 * LOCKTAG_RELATION_APPENDONLY_SEGMENT_FILE) in order to guarantee
 * stability of the pg_aoseg information on this segment file and exclusive right
 * to append data to the segment file.
 */
AOCSFileSegInfo *
GetAOCSFileSegInfo(
	Relation			prel,
	AppendOnlyEntry		*aoEntry,
	Snapshot			appendOnlyMetaDataSnapshot,
	int32				segno)
{
	int32 nvp = RelationGetNumberOfAttributes(prel);

	Relation segrel;
	Relation segidx;
	ScanKeyData scankey;
	IndexScanDesc scan;
	HeapTuple segtup;

	AOCSFileSegInfo *seginfo;
	Datum *d;
	bool *null;

	segrel = heap_open(aoEntry->segrelid, AccessShareLock);
	segidx = index_open(aoEntry->segidxid, AccessShareLock);

	ScanKeyInit(&scankey, (AttrNumber)Anum_pg_aocs_segno, BTEqualStrategyNumber, F_OIDEQ, Int32GetDatum(segno));

	scan = index_beginscan(segrel, segidx, appendOnlyMetaDataSnapshot, 1, &scankey);

	segtup = index_getnext(scan, ForwardScanDirection);

	if(!HeapTupleIsValid(segtup))
	{
		/* This segment file does not have an entry. */
		index_endscan(scan);
		index_close(segidx, AccessShareLock);
		heap_close(segrel, AccessShareLock);
		return NULL;
	}

	/* Close the index */
	segtup = heap_copytuple(segtup);
	
	index_endscan(scan);
	index_close(segidx, AccessShareLock);

	Assert(HeapTupleIsValid(segtup));

	seginfo = (AOCSFileSegInfo *) palloc0(aocsfileseginfo_size(nvp));

	d = (Datum *) palloc(sizeof(Datum) * Natts_pg_aocsseg);
	null = (bool *) palloc(sizeof(bool) * Natts_pg_aocsseg); 

	heap_deform_tuple(segtup, RelationGetDescr(segrel), d, null);

	Assert(!null[Anum_pg_aocs_segno - 1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno - 1] == segno));
	seginfo->segno = segno;

	Assert(!null[Anum_pg_aocs_tupcount - 1]);
	seginfo->total_tupcount = DatumGetInt64(d[Anum_pg_aocs_tupcount - 1]);

	Assert(!null[Anum_pg_aocs_varblockcount - 1]);
	seginfo->varblockcount = DatumGetInt64(d[Anum_pg_aocs_varblockcount - 1]);

	Assert(!null[Anum_pg_aocs_modcount - 1]);
	seginfo->modcount = DatumGetInt64(d[Anum_pg_aocs_modcount - 1]);

	Assert(!null[Anum_pg_aocs_state - 1]);
	seginfo->state = DatumGetInt16(d[Anum_pg_aocs_state - 1]);

	Assert(!null[Anum_pg_aocs_vpinfo - 1]);
	{
		struct varlena *v = (struct varlena *) DatumGetPointer(d[Anum_pg_aocs_vpinfo - 1]);
		struct varlena *dv = pg_detoast_datum(v);

		Assert(VARSIZE(dv) == aocs_vpinfo_size(nvp));
		memcpy(&seginfo->vpinfo, dv, aocs_vpinfo_size(nvp));
		if(dv!=v)
			pfree(dv);
	}

	pfree(d);
	pfree(null);

	heap_close(segrel, AccessShareLock);

	return seginfo;
}

AOCSFileSegInfo **GetAllAOCSFileSegInfo(Relation prel,
										AppendOnlyEntry *aoEntry,
										Snapshot appendOnlyMetaDataSnapshot,
										int32 *totalseg)
{
    Relation			pg_aocsseg_rel;
	AOCSFileSegInfo		**results;

	Assert(RelationIsAoCols(prel));
	
	pg_aocsseg_rel = relation_open(aoEntry->segrelid, AccessShareLock);

	results = GetAllAOCSFileSegInfo_pg_aocsseg_rel(
											RelationGetNumberOfAttributes(prel),
											RelationGetRelationName(prel),
											aoEntry,
											pg_aocsseg_rel,
											appendOnlyMetaDataSnapshot,
											totalseg);

	CheckCOConsistencyWithGpRelationNode(appendOnlyMetaDataSnapshot,
											prel,
											*totalseg,
											results);

    heap_close(pg_aocsseg_rel, AccessShareLock);

	return results;
}

/*
 * The comparison routine that sorts an array of AOCSFileSegInfos
 * in the ascending order of the segment number.
 */
static int
aocsFileSegInfoCmp(const void *left, const void *right)
{
	AOCSFileSegInfo *leftSegInfo = *((AOCSFileSegInfo **)left);
	AOCSFileSegInfo *rightSegInfo = *((AOCSFileSegInfo **)right);
	
	if (leftSegInfo->segno < rightSegInfo->segno)
		return -1;
	
	if (leftSegInfo->segno > rightSegInfo->segno)
		return 1;
	
	return 0;
}

AOCSFileSegInfo **GetAllAOCSFileSegInfo_pg_aocsseg_rel(
										int numOfColumns,
										char *relationName,
										AppendOnlyEntry *aoEntry,
										Relation pg_aocsseg_rel,
										Snapshot snapshot,
										int32 *totalseg)
{

    int32 nvp = numOfColumns;

    HeapScanDesc scan;
    HeapTuple tup;

    AOCSFileSegInfo **allseg;
    AOCSFileSegInfo *seginfo;
    int cur_seg;
    Datum *d;
    bool *null;
	int seginfo_slot_no = AO_FILESEGINFO_ARRAY_SIZE;

	Assert(aoEntry != NULL);

	/* MPP-16407:
	 * Initialize the segment file information array, we first allocate 8 slot for the array,
	 * then array will be dynamically expanded later if necessary.
	 */
    allseg = (AOCSFileSegInfo **) palloc0(sizeof(AOCSFileSegInfo*) * seginfo_slot_no);
    d = (Datum *) palloc(sizeof(Datum) * Natts_pg_aocsseg);
    null = (bool *) palloc(sizeof(bool) * Natts_pg_aocsseg);

    cur_seg = 0;

    scan = heap_beginscan(pg_aocsseg_rel, snapshot, 0, NULL);
    while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
		/* dynamically expand space for AOCSFileSegInfo* array */
		if (cur_seg >= seginfo_slot_no)
		{
			seginfo_slot_no *= 2;
			allseg = (AOCSFileSegInfo **) repalloc(allseg, sizeof(AOCSFileSegInfo*) * seginfo_slot_no);
		}
		
		seginfo = (AOCSFileSegInfo *) palloc0(aocsfileseginfo_size(nvp));

		allseg[cur_seg] = seginfo;

		GetTupleVisibilitySummary(
								tup,
								&seginfo->tupleVisibilitySummary);

		heap_deform_tuple(tup, RelationGetDescr(pg_aocsseg_rel), d, null);

		Assert(!null[Anum_pg_aocs_segno - 1]);
		seginfo->segno = DatumGetInt32(d[Anum_pg_aocs_segno - 1]);

		Assert(!null[Anum_pg_aocs_tupcount - 1]);
		seginfo->total_tupcount = DatumGetInt64(d[Anum_pg_aocs_tupcount - 1]);

		Assert(!null[Anum_pg_aocs_varblockcount - 1]);
		seginfo->varblockcount = DatumGetInt64(d[Anum_pg_aocs_varblockcount - 1]);

		/*
		 * Modcount cannot be NULL in normal operation. However, when
		 * called from gp_aoseg_history after an upgrade, the old now invisible
		 * entries may have not set the state and the modcount.
		 */
		Assert(!null[Anum_pg_aocs_modcount - 1] || snapshot == SnapshotAny);
		if (!null[Anum_pg_aocs_modcount - 1])
			seginfo->modcount = DatumGetInt64(d[Anum_pg_aocs_modcount - 1]);

		Assert(!null[Anum_pg_aocs_state - 1] || snapshot == SnapshotAny);
		if (!null[Anum_pg_aocs_state - 1])
			seginfo->state = DatumGetInt16(d[Anum_pg_aocs_state - 1]);

		Assert(!null[Anum_pg_aocs_vpinfo - 1]);
        {
            struct varlena *v = (struct varlena *) DatumGetPointer(d[Anum_pg_aocs_vpinfo - 1]);
            struct varlena *dv = pg_detoast_datum(v);

            /* 
             * VARSIZE(dv) may be less than aocs_vpinfo_size, in case of
             * add column, we try to do a ctas from old table to new table.
             */
            Assert(VARSIZE(dv) <= aocs_vpinfo_size(nvp));

            memcpy(&seginfo->vpinfo, dv, VARSIZE(dv));
            if(dv!=v)
                pfree(dv);
        }
        ++cur_seg;
    }

    pfree(d);
    pfree(null);

    heap_endscan(scan);

	*totalseg = cur_seg;

	if (*totalseg == 0)
	{
		pfree(allseg);
		
		return NULL;
	}
	
	/*
	 * Sort allseg by the order of segment file number.
	 *
	 * Currently this is only needed when building a bitmap index to guarantee the tids
	 * are in the ascending order. But since this array is pretty small, we just sort
	 * the array for all cases.
	 */
	qsort((char *)allseg, *totalseg, sizeof(AOCSFileSegInfo *), aocsFileSegInfoCmp);

    return allseg;
}

FileSegTotals *
GetAOCSSSegFilesTotals(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot)
{
	AOCSFileSegInfo **allseg;
	int totalseg;
	int s;
	AOCSVPInfo *vpinfo;
	AppendOnlyEntry *aoEntry = NULL;
	FileSegTotals *totals;

	Assert(RelationIsValid(parentrel));
	Assert(RelationIsAoCols(parentrel));

	totals = (FileSegTotals *) palloc0(sizeof(FileSegTotals));
	memset(totals, 0, sizeof(FileSegTotals));

	aoEntry = GetAppendOnlyEntry(RelationGetRelid(parentrel), appendOnlyMetaDataSnapshot);
	Assert(aoEntry != NULL);

	allseg = GetAllAOCSFileSegInfo(parentrel, aoEntry, appendOnlyMetaDataSnapshot, &totalseg);
	for (s = 0; s < totalseg; s++)
	{
		int32 nEntry;
		int e;

		vpinfo = &((allseg[s])->vpinfo);
		nEntry = vpinfo->nEntry;

		for (e = 0; e < nEntry; e++)
		{
			totals->totalbytes += vpinfo->entry[e].eof;
			totals->totalbytesuncompressed = vpinfo->entry[e].eof_uncompressed;
		}
		if (allseg[s]->state != AOSEG_STATE_AWAITING_DROP)
		{
			totals->totaltuples += allseg[s]->total_tupcount;
		}
		totals->totalvarblocks += allseg[s]->varblockcount;
		totals->totalfilesegs ++;
	}

	pfree(aoEntry);
	
	if (allseg)
	{
		FreeAllAOCSSegFileInfo(allseg, totalseg);
		pfree(allseg);
	}

	return totals;
}

/*
 * GetAOCSTotalBytes
 *
 * Get the total bytes for a specific AOCS table from the pg_aocsseg table on this local segdb.
 */
int64
GetAOCSTotalBytes(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot)
{
	AOCSFileSegInfo **allseg;
	int totalseg;
	int64 result;
	int s;
	AOCSVPInfo *vpinfo;
	AppendOnlyEntry *aoEntry = NULL;

	aoEntry = GetAppendOnlyEntry(RelationGetRelid(parentrel), appendOnlyMetaDataSnapshot);
	Assert(aoEntry != NULL);

	result = 0;
	allseg = GetAllAOCSFileSegInfo(parentrel, aoEntry, appendOnlyMetaDataSnapshot, &totalseg);
	for (s = 0; s < totalseg; s++)
	{
		int32 nEntry;
		int e;
		
		vpinfo = &((allseg[s])->vpinfo);
		nEntry = vpinfo->nEntry;

		for (e = 0; e < nEntry; e++)
			result += vpinfo->entry[e].eof;
	}

	pfree(aoEntry);
	
	if (allseg)
	{
		FreeAllAOCSSegFileInfo(allseg, totalseg);
		pfree(allseg);
	}

	return result;
}

void
SetAOCSFileSegInfoState(Relation prel,
		AppendOnlyEntry *aoEntry,
		int segno,
		FileSegInfoState newState)
{
	LockAcquireResult acquireResult;
	Relation segrel;
	Relation segidx;
	ScanKeyData key;
	IndexScanDesc scan;
	HeapTuple oldtup;
	HeapTuple newtup;
	Datum d[Natts_pg_aocsseg];
	bool null[Natts_pg_aocsseg] = {0, };
	bool repl[Natts_pg_aocsseg] = {0, };
	TupleDesc tupdesc; 

	elogif(Debug_appendonly_print_compaction, LOG,
			"Set segfile info state: segno %d, table '%s', new state %d",
			segno,
			RelationGetRelationName(prel),
			newState);
	/*
	 * Since we have the segment-file entry under lock (with LockRelationAppendOnlySegmentFile)
	 * we can use SnapshotNow.
	 */
	Snapshot usesnapshot = SnapshotNow;

	Assert(aoEntry != NULL);
	Assert(RelationIsAoCols(prel));
	Assert(newState > AOSEG_STATE_USECURRENT && newState <= AOSEG_STATE_AWAITING_DROP);

	/*
	 * Verify we already have the write-lock!
	 */
	acquireResult = LockRelationAppendOnlySegmentFile(
												&prel->rd_node,
												segno,
												AccessExclusiveLock,
												/* dontWait */ false);
	if (acquireResult != LOCKACQUIRE_ALREADY_HELD)
	{
		elog(ERROR, "Should already have the (transaction-scope) write-lock on Append-Only segment file #%d, "
					 "relation %s", segno, RelationGetRelationName(prel));
	}

	segrel = heap_open(aoEntry->segrelid, RowExclusiveLock);
	segidx = index_open(aoEntry->segidxid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	ScanKeyInit(&key, (AttrNumber)Anum_pg_aocs_segno, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(segno)); 
	scan = index_beginscan(segrel, segidx, usesnapshot, 1, &key);

	oldtup = index_getnext(scan, ForwardScanDirection);

	if(!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
					errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
						RelationGetRelationName(prel), segno)
					));

#ifdef USE_ASSERT_CHECKING
	d[Anum_pg_aocs_segno-1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno-1]);
	Assert(!null[Anum_pg_aocs_segno-1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno-1]) == segno);
#endif

	d[Anum_pg_aocs_state - 1] = Int16GetDatum(newState);
	repl[Anum_pg_aocs_state - 1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);
	CatalogUpdateIndexes(segrel, newtup);

	pfree(newtup);

	index_endscan(scan);
	index_close(segidx, RowExclusiveLock);
	heap_close(segrel, RowExclusiveLock);
}

void
ClearAOCSFileSegInfo(Relation prel, AppendOnlyEntry *aoEntry, int segno, FileSegInfoState newState)
{
	LockAcquireResult acquireResult;
	Relation segrel;
	Relation segidx;
	ScanKeyData key;
	IndexScanDesc scan;
	HeapTuple oldtup;
	HeapTuple newtup;
	Datum d[Natts_pg_aocsseg];
	bool null[Natts_pg_aocsseg] = {0, };
	bool repl[Natts_pg_aocsseg] = {0, };
	TupleDesc tupdesc; 
	int nvp = RelationGetNumberOfAttributes(prel);
	int i;
	AOCSVPInfo *vpinfo = create_aocs_vpinfo(nvp);

	/*
	 * Since we have the segment-file entry under lock (with LockRelationAppendOnlySegmentFile)
	 * we can use SnapshotNow.
	 */
	Snapshot usesnapshot = SnapshotNow;

	Assert(aoEntry != NULL);
	Assert(RelationIsAoCols(prel));
	Assert(newState >= AOSEG_STATE_USECURRENT && newState <= AOSEG_STATE_AWAITING_DROP);

	elogif(Debug_appendonly_print_compaction, LOG,
			"Clear seg file info: segno %d table '%s'",
			segno,
			RelationGetRelationName(prel));
	
	/*
	 * Verify we already have the write-lock!
	 */
	acquireResult = LockRelationAppendOnlySegmentFile(
												&prel->rd_node,
												segno,
												AccessExclusiveLock,
												/* dontWait */ false);
	if (acquireResult != LOCKACQUIRE_ALREADY_HELD)
	{
		elog(ERROR, "Should already have the (transaction-scope) write-lock on Append-Only segment file #%d, "
					 "relation %s", segno, RelationGetRelationName(prel));
	}

	segrel = heap_open(aoEntry->segrelid, RowExclusiveLock);
	segidx = index_open(aoEntry->segidxid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	ScanKeyInit(&key, (AttrNumber)Anum_pg_aocs_segno, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(segno)); 
	scan = index_beginscan(segrel, segidx, usesnapshot, 1, &key);

	oldtup = index_getnext(scan, ForwardScanDirection);

	if(!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
					errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
						RelationGetRelationName(prel), segno)
					));

#ifdef USE_ASSERT_CHECKING
	d[Anum_pg_aocs_segno-1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno-1]);
	Assert(!null[Anum_pg_aocs_segno-1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno-1]) == segno);
#endif

	d[Anum_pg_aocs_tupcount-1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount, tupdesc, &null[Anum_pg_aocs_tupcount-1]);
	Assert(!null[Anum_pg_aocs_tupcount-1]);

	d[Anum_pg_aocs_tupcount-1] = 0;
	repl[Anum_pg_aocs_tupcount-1] = true;


	d[Anum_pg_aocs_varblockcount-1] = fastgetattr(oldtup, Anum_pg_aocs_varblockcount, tupdesc, &null[Anum_pg_aocs_varblockcount-1]);
	Assert(!null[Anum_pg_aocs_varblockcount-1]);
	d[Anum_pg_aocs_varblockcount-1] = 0;
	repl[Anum_pg_aocs_varblockcount-1] = true;

	/* We do not reset the modcount here */

	for(i=0; i<nvp; ++i)
	{
		vpinfo->entry[i].eof = 0;
		vpinfo->entry[i].eof_uncompressed = 0;
	}
	d[Anum_pg_aocs_vpinfo-1] = PointerGetDatum(vpinfo);
	null[Anum_pg_aocs_vpinfo-1] = false;
	repl[Anum_pg_aocs_vpinfo-1] = true;

	if (newState > 0)
	{
		d[Anum_pg_aocs_state - 1] = Int16GetDatum(newState);
		repl[Anum_pg_aocs_state - 1] = true;
	}

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);
	CatalogUpdateIndexes(segrel, newtup);

	pfree(newtup);
	pfree(vpinfo);

	index_endscan(scan);
	index_close(segidx, RowExclusiveLock);
	heap_close(segrel, RowExclusiveLock);
}

void 
UpdateAOCSFileSegInfo(AOCSInsertDesc idesc)
{
	LockAcquireResult acquireResult;
	
	Relation prel = idesc->aoi_rel;
	Relation segrel;
	Relation segidx;

	ScanKeyData key;
	IndexScanDesc scan;

	HeapTuple oldtup;
	HeapTuple newtup;
	Datum d[Natts_pg_aocsseg];
	bool null[Natts_pg_aocsseg] = {0, };
	bool repl[Natts_pg_aocsseg] = {0, };

	TupleDesc tupdesc; 
	int nvp = RelationGetNumberOfAttributes(prel);
	int i;
	AOCSVPInfo *vpinfo = create_aocs_vpinfo(nvp);
	AppendOnlyEntry *aoEntry = idesc->aoEntry;

	/*
	 * Since we have the segment-file entry under lock (with LockRelationAppendOnlySegmentFile)
	 * we can use SnapshotNow.
	 */
	Snapshot usesnapshot = SnapshotNow;

	Assert(aoEntry != NULL);

	/*
	 * Verify we already have the write-lock!
	 */
	acquireResult = LockRelationAppendOnlySegmentFile(
												&prel->rd_node,
												idesc->cur_segno,
												AccessExclusiveLock,
												/* dontWait */ false);
	if (acquireResult != LOCKACQUIRE_ALREADY_HELD)
	{
		elog(ERROR, "Should already have the (transaction-scope) write-lock on Append-Only segment file #%d, "
					 "relation %s", idesc->cur_segno, RelationGetRelationName(prel));
	}

	segrel = heap_open(aoEntry->segrelid, RowExclusiveLock);
	segidx = index_open(aoEntry->segidxid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	ScanKeyInit(&key, 1, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(idesc->cur_segno)); 
	scan = index_beginscan(segrel, segidx, usesnapshot, 1, &key);

	oldtup = index_getnext(scan, ForwardScanDirection);

	if(!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
					errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
						RelationGetRelationName(prel), idesc->cur_segno)
					));

#ifdef USE_ASSERT_CHECKING
	d[Anum_pg_aocs_segno-1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno-1]);
	Assert(!null[Anum_pg_aocs_segno-1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno-1]) == idesc->cur_segno);
#endif

	d[Anum_pg_aocs_tupcount-1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount, tupdesc, &null[Anum_pg_aocs_tupcount-1]);
	Assert(!null[Anum_pg_aocs_tupcount-1]);

	d[Anum_pg_aocs_tupcount-1] += idesc->insertCount;
	repl[Anum_pg_aocs_tupcount-1] = true;


	d[Anum_pg_aocs_varblockcount-1] = fastgetattr(oldtup, Anum_pg_aocs_varblockcount, tupdesc, &null[Anum_pg_aocs_varblockcount-1]);
	Assert(!null[Anum_pg_aocs_varblockcount-1]);
	d[Anum_pg_aocs_varblockcount-1] += idesc->varblockCount;
	repl[Anum_pg_aocs_varblockcount-1] = true;

	d[Anum_pg_aocs_modcount-1] = fastgetattr(oldtup, Anum_pg_aocs_modcount, tupdesc, &null[Anum_pg_aocs_modcount-1]);
	Assert(!null[Anum_pg_aocs_modcount-1]);
	d[Anum_pg_aocs_modcount-1] += 1;
	repl[Anum_pg_aocs_modcount-1] = true;

	/*
	 * Lets fetch the vpinfo structure from the existing tuple in pg_aocsseg.
	 * vpinfo provides us with the end-of-file (EOF) values for each column file.
	 */
	Datum d_tmp = fastgetattr(oldtup, Anum_pg_aocs_vpinfo, tupdesc,
						&null[Anum_pg_aocs_vpinfo-1]);
	Assert(!null[Anum_pg_aocs_vpinfo-1]);
	struct varlena *v = (struct varlena *) DatumGetPointer(d_tmp);
	struct varlena *dv = pg_detoast_datum(v);
	Assert(VARSIZE(dv) == aocs_vpinfo_size(nvp));
	AOCSVPInfo *oldvpinfo = (AOCSVPInfo *) dv;

	/*
	 * Number of columns fetched from vpinfo should match number of attributes
	 * for relation.
	 */
	Assert(nvp == oldvpinfo->nEntry);

	/* Check and update EOF value for each column file. */
	for(i=0; i<nvp; ++i)
	{
		/*
		 * For CO by design due to append-only nature,
		 * new end-of-file (EOF) to be recorded in aoseg table has to be greater
		 * than currently stored EOF value, as new writes must move it forward only.
		 * If new end-of-file value is less than currently stored end-of-file
		 * something is incorrect and updating the same will yeild incorrect result
		 * during reads. Hence abort the write transaction trying to update the
		 * incorrect EOF value.
		 */

		if (oldvpinfo->entry[i].eof <= idesc->ds[i]->eof)
		{
			vpinfo->entry[i].eof = idesc->ds[i]->eof;
		}
		else
		{
			elog(ERROR, "Unexpected compressed EOF for relation %s, relfilenode %u, segment file %d coln %d. "
						"EOF " INT64_FORMAT " to be updated cannot be smaller than current EOF " INT64_FORMAT " in pg_aocsseg",
						RelationGetRelationName(prel), prel->rd_node.relNode,
						idesc->cur_segno, i, idesc->ds[i]->eof, oldvpinfo->entry[i].eof);
		}

		if (oldvpinfo->entry[i].eof_uncompressed <= idesc->ds[i]->eofUncompress)
		{
			vpinfo->entry[i].eof_uncompressed = idesc->ds[i]->eofUncompress;
		}
		else
		{
			elog(ERROR, "Unexpected EOF for relation %s, relfilenode %u, segment file %d coln %d. "
						"EOF " INT64_FORMAT " to be updated cannot be smaller than current EOF "INT64_FORMAT" in pg_aocsseg",
						RelationGetRelationName(prel), prel->rd_node.relNode,
						idesc->cur_segno, i, idesc->ds[i]->eofUncompress, oldvpinfo->entry[i].eof_uncompressed);
		}
	}

	if(dv!=v)
	{
		pfree(dv);
	}

	d[Anum_pg_aocs_vpinfo-1] = PointerGetDatum(vpinfo);
	null[Anum_pg_aocs_vpinfo-1] = false;
	repl[Anum_pg_aocs_vpinfo-1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);
	CatalogUpdateIndexes(segrel, newtup);

	pfree(newtup);
	pfree(vpinfo);

	index_endscan(scan);
	index_close(segidx, RowExclusiveLock);
	heap_close(segrel, RowExclusiveLock);
}

/*
 * Update vpinfo column of pg_aocsseg_* by adding new
 * AOCSVPInfoEntries.  One VPInfoEntry is added for each newly added
 * segfile (column).  If empty=true, add empty VPInfoEntry's having
 * eof=0.
 */
void AOCSFileSegInfoAddVpe(Relation prel, AppendOnlyEntry *aoEntry, int32 segno,
						   AOCSAddColumnDesc desc, int num_newcols, bool empty)
{
	LockAcquireResult acquireResult;

	Relation segrel;
	Relation segidx;

	ScanKeyData key;
	IndexScanDesc scan;

	AOCSVPInfo *oldvpinfo;
	AOCSVPInfo *newvpinfo;
	HeapTuple oldtup;
	HeapTuple newtup;
	Datum d[Natts_pg_aocsseg];
	bool null[Natts_pg_aocsseg] = {0, };
	bool repl[Natts_pg_aocsseg] = {0, };

	TupleDesc tupdesc; 
	int nvp = RelationGetNumberOfAttributes(prel);
	/* nvp is new columns + existing columns */
	int i;
	int j;

	if (Gp_role == GP_ROLE_UTILITY)
	{
		elog(ERROR, "cannot add column in utility mode, relation %s, segno %d",
			 RelationGetRelationName(prel), segno);
	}
	if (empty && Gp_role != GP_ROLE_DISPATCH)
	{
		elog(LOG, "Adding empty VPEntries for relation %s, segno %d",
			 RelationGetRelationName(prel), segno);
	}

	acquireResult = LockRelationNoWait(prel, AccessExclusiveLock);
	if (acquireResult != LOCKACQUIRE_ALREADY_HELD)
	{
		elog(ERROR, "should already have (transaction-scope) AccessExclusive"
			 " lock on relation %s, oid %d",
			 RelationGetRelationName(prel), RelationGetRelid(prel));
	}

	segrel = heap_open(aoEntry->segrelid, RowExclusiveLock);
	segidx = index_open(aoEntry->segidxid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	ScanKeyInit(&key, 1, BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(segno));
	/*
	 * Since we have the segment-file entry under lock (with
	 * LockRelationAppendOnlySegmentFile) we can use SnapshotNow.
	 */
	scan = index_beginscan(segrel, segidx, SnapshotNow, 1, &key);

	oldtup = index_getnext(scan, ForwardScanDirection);

	if(!HeapTupleIsValid(oldtup))
	{
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("AOCS rel \"%s\" segment \"%d\" does not exist",
						RelationGetRelationName(prel), segno)
						));
	}

	d[Anum_pg_aocs_segno-1] = fastgetattr(oldtup, Anum_pg_aocs_segno,
										  tupdesc, &null[Anum_pg_aocs_segno-1]);
	Assert(!null[Anum_pg_aocs_segno-1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno-1]) == segno);

	d[Anum_pg_aocs_tupcount-1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount,
											 tupdesc,
											 &null[Anum_pg_aocs_tupcount-1]);
	Assert(!null[Anum_pg_aocs_tupcount-1]);

	d[Anum_pg_aocs_modcount-1] = fastgetattr(oldtup, Anum_pg_aocs_modcount,
											 tupdesc,
											 &null[Anum_pg_aocs_modcount-1]);
	Assert(!null[Anum_pg_aocs_modcount-1]);
	d[Anum_pg_aocs_modcount-1] += 1;
	repl[Anum_pg_aocs_modcount-1] = true;

	/* new VPInfo having VPEntries with eof=0 */
	newvpinfo = create_aocs_vpinfo(nvp);
	if (!empty)
	{
		d[Anum_pg_aocs_vpinfo-1] =
				fastgetattr(oldtup, Anum_pg_aocs_vpinfo, tupdesc,
							&null[Anum_pg_aocs_vpinfo-1]);
		Assert(!null[Anum_pg_aocs_vpinfo-1]);
		struct varlena *v = (struct varlena *) DatumGetPointer(
				d[Anum_pg_aocs_vpinfo-1]);
		struct varlena *dv = pg_detoast_datum(v);
		Assert(VARSIZE(dv) == aocs_vpinfo_size(nvp - num_newcols));
		oldvpinfo = (AOCSVPInfo *)dv;
		Assert(oldvpinfo->nEntry + num_newcols == nvp);
		/* copy existing columns' eofs to new vpinfo */
		for (i = 0; i < oldvpinfo->nEntry; ++i)
		{	
			newvpinfo->entry[i].eof = oldvpinfo->entry[i].eof;
			newvpinfo->entry[i].eof_uncompressed =
					oldvpinfo->entry[i].eof_uncompressed;
		}
		/* eof for new segfiles come next */
		for (i = oldvpinfo->nEntry, j = 0; i < nvp; ++i, ++j)
		{
			newvpinfo->entry[i].eof = desc->dsw[j]->eof;
			newvpinfo->entry[i].eof_uncompressed =
					desc->dsw[j]->eofUncompress;
		}
		if(dv!=v)
		{
			pfree(dv);
		}
	}
	d[Anum_pg_aocs_vpinfo-1] = PointerGetDatum(newvpinfo);
	null[Anum_pg_aocs_vpinfo-1] = false;
	repl[Anum_pg_aocs_vpinfo-1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);
	CatalogUpdateIndexes(segrel, newtup);

	pfree(newtup);
	pfree(newvpinfo);

	index_endscan(scan);
	/*
	 * Holding RowExclusiveLock lock on pg_aocsseg_* until the ALTER
	 * TABLE transaction commits/aborts.  Additionally, we are already
	 * holding AccessExclusive lock on the AOCS relation OID.
	 */
	index_close(segidx, NoLock);
	heap_close(segrel, NoLock);
}

void AOCSFileSegInfoAddCount(Relation prel, AppendOnlyEntry *aoEntry, 
		int32 segno, int64 tupadded, int64 varblockadded, int64 modcount_added)
{
	LockAcquireResult acquireResult;
	
    Relation segrel;
    Relation segidx;

    ScanKeyData key;
    IndexScanDesc scan;

    HeapTuple oldtup;
    HeapTuple newtup;
    Datum d[Natts_pg_aocsseg];
    bool null[Natts_pg_aocsseg] = {0, };
    bool repl[Natts_pg_aocsseg] = {0, };

    TupleDesc tupdesc; 

	/*
	 * Since we have the segment-file entry under lock (with LockRelationAppendOnlySegmentFile)
	 * we can use SnapshotNow.
	 */
    Snapshot usesnapshot = SnapshotNow;


	Assert(aoEntry != NULL);

	/*
	 * Verify we already have the write-lock!
	 */
	acquireResult = LockRelationAppendOnlySegmentFile(
												&prel->rd_node,
												segno,
												AccessExclusiveLock,
												/* dontWait */ false);
	if (acquireResult != LOCKACQUIRE_ALREADY_HELD)
	{
		elog(ERROR, "Should already have the (transaction-scope) write-lock on Append-Only segment file #%d, "
					 "relation %s", segno, RelationGetRelationName(prel));
	}

    segrel = heap_open(aoEntry->segrelid, RowExclusiveLock);
    segidx = index_open(aoEntry->segidxid, RowExclusiveLock);

    tupdesc = RelationGetDescr(segrel);

    ScanKeyInit(&key, 1, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(segno));
    scan = index_beginscan(segrel, segidx, usesnapshot, 1, &key);

    oldtup = index_getnext(scan, ForwardScanDirection);

    if(!HeapTupleIsValid(oldtup))
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
                    errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
                        RelationGetRelationName(prel), segno)
                    ));

#ifdef USE_ASSERT_CHECKING
    d[Anum_pg_aocs_segno-1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno-1]);
    Assert(!null[Anum_pg_aocs_segno-1]);
    Assert(DatumGetInt32(d[Anum_pg_aocs_segno-1]) == segno);
#endif

    d[Anum_pg_aocs_tupcount-1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount, tupdesc, &null[Anum_pg_aocs_tupcount-1]);
    Assert(!null[Anum_pg_aocs_tupcount-1]);

    d[Anum_pg_aocs_tupcount-1] += tupadded;
    repl[Anum_pg_aocs_tupcount-1] = true;

    d[Anum_pg_aocs_varblockcount-1] = fastgetattr(oldtup, Anum_pg_aocs_varblockcount, tupdesc, &null[Anum_pg_aocs_varblockcount-1]);
    Assert(!null[Anum_pg_aocs_varblockcount-1]);
    d[Anum_pg_aocs_varblockcount-1] += varblockadded;
    repl[Anum_pg_aocs_tupcount-1] = true;

    d[Anum_pg_aocs_modcount-1] = fastgetattr(oldtup, Anum_pg_aocs_modcount, tupdesc, &null[Anum_pg_aocs_modcount-1]);
    Assert(!null[Anum_pg_aocs_modcount-1]);
    d[Anum_pg_aocs_modcount-1] += modcount_added;
    repl[Anum_pg_aocs_modcount-1] = true;

    newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

    simple_heap_update(segrel, &oldtup->t_self, newtup);
    CatalogUpdateIndexes(segrel, newtup);

    heap_freetuple(newtup);

    index_endscan(scan);
    index_close(segidx, RowExclusiveLock);
    heap_close(segrel, RowExclusiveLock);
}

extern Datum aocsvpinfo_decode(PG_FUNCTION_ARGS);
Datum aocsvpinfo_decode(PG_FUNCTION_ARGS)
{
    AOCSVPInfo *vpinfo = (AOCSVPInfo *) PG_GETARG_BYTEA_P(0);
    int i = PG_GETARG_INT32(1);
    int j = PG_GETARG_INT32(2);
    int64 result;

    if (i<0 || i>=vpinfo->nEntry)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("invalid entry for decoding aocsvpinfo")
                    ));

    if (j<0 || j>1)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("invalid entry for decoding aocsvpinfo")
                    ));

    if (j==0)
        result = vpinfo->entry[i].eof;
    else
        result = vpinfo->entry[i].eof_uncompressed;

    PG_RETURN_INT64(result);
}

PG_MODULE_MAGIC;



static Datum
gp_aocsseg_internal(PG_FUNCTION_ARGS, Oid aocsRelOid)
{
	typedef struct Context
	{
		Oid		aocsRelOid;

		int		relnatts;
		
		struct AOCSFileSegInfo **aocsSegfileArray;

		int		totalAocsSegFiles;

		int		segfileArrayIndex;

		int		columnNum;
						// 0-based index into columns.
	} Context;
	
	FuncCallContext *funcctx;
	Context *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		Relation aocsRel;
		AppendOnlyEntry *aoEntry;
		Relation pg_aocsseg_rel;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function
		 * calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(9, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "gp_tid",
						   TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "column_num",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "physical_segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "tupcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "eof",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "eof_uncompressed",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "modcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "state",
						   INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->aocsRelOid = aocsRelOid;

		aocsRel = heap_open(aocsRelOid, NoLock);
		if(!RelationIsAoCols(aocsRel))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("'%s' is not an append-only columnar relation",
							RelationGetRelationName(aocsRel))));

		// Remember the number of columns.
		context->relnatts = aocsRel->rd_rel->relnatts;

		aoEntry = GetAppendOnlyEntry(aocsRelOid, SnapshotNow);
		
		pg_aocsseg_rel = heap_open(aoEntry->segrelid, NoLock);
		
		context->aocsSegfileArray = GetAllAOCSFileSegInfo_pg_aocsseg_rel(
														aocsRel->rd_rel->relnatts,
														RelationGetRelationName(aocsRel), 
														aoEntry, 
														pg_aocsseg_rel,
														SnapshotNow, 
														&context->totalAocsSegFiles);

		heap_close(pg_aocsseg_rel, NoLock);
		heap_close(aocsRel, NoLock);

		// Iteration positions.
		context->segfileArrayIndex = 0;
		context->columnNum = 0;

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	/*
	 * Process each column for each segment file.
	 */
	while (true)
	{
		Datum		values[9];
		bool		nulls[9];
		HeapTuple	tuple;
		Datum		result;

		struct AOCSFileSegInfo *aocsSegfile;

		AOCSVPInfoEntry *entry;
		
		if (context->segfileArrayIndex >= context->totalAocsSegFiles)
		{
			break;
		}

		if (context->columnNum >= context->relnatts)
		{
			/*
			 * Finished with the current segment file.
			 */
			context->segfileArrayIndex++;
			if (context->segfileArrayIndex >= context->totalAocsSegFiles)
			{
				continue;
			}

			context->columnNum = 0;
		}
		
		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		aocsSegfile = context->aocsSegfileArray[context->segfileArrayIndex];

		entry = getAOCSVPEntry(aocsSegfile, context->columnNum);

		values[0] = ItemPointerGetDatum(&aocsSegfile->tupleVisibilitySummary.tid);
		values[1] = Int32GetDatum(aocsSegfile->segno);
		values[2] = Int16GetDatum(context->columnNum);
		values[3] = Int32GetDatum(context->columnNum * AOTupleId_MultiplierSegmentFileNum + aocsSegfile->segno);
		values[4] = Int64GetDatum(aocsSegfile->total_tupcount);
		values[5] = Int64GetDatum(entry->eof);
		values[6] = Int64GetDatum(entry->eof_uncompressed);
		values[7] = Int64GetDatum(aocsSegfile->modcount);
		values[8] = Int16GetDatum(aocsSegfile->state);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		// Indicate we emitted one column.
		context->columnNum++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

PG_FUNCTION_INFO_V1(gp_aocsseg);

extern Datum
gp_aocsseg(PG_FUNCTION_ARGS);

Datum
gp_aocsseg(PG_FUNCTION_ARGS)
{
	int aocsRelOid = PG_GETARG_OID(0);
	return gp_aocsseg_internal(fcinfo, aocsRelOid);
}

PG_FUNCTION_INFO_V1(gp_aocsseg_name);

extern Datum
gp_aocsseg_name(PG_FUNCTION_ARGS);

/*
 * UDF to get aocsseg information by relation name
 */
Datum
gp_aocsseg_name(PG_FUNCTION_ARGS)
{
    int aocsRelOid;
	RangeVar		*parentrv;
	text			*relname = PG_GETARG_TEXT_P(0);

	parentrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
	aocsRelOid = RangeVarGetRelid(parentrv, false);

	return gp_aocsseg_internal(fcinfo, aocsRelOid);
}

PG_FUNCTION_INFO_V1(gp_aocsseg_history);

extern Datum
gp_aocsseg_history(PG_FUNCTION_ARGS);

Datum
gp_aocsseg_history(PG_FUNCTION_ARGS)
{
    int aocsRelOid = PG_GETARG_OID(0);

	typedef struct Context
	{
		Oid		aocsRelOid;

		int		relnatts;
		
		struct AOCSFileSegInfo **aocsSegfileArray;

		int		totalAocsSegFiles;

		int		segfileArrayIndex;

		int		columnNum;
						// 0-based index into columns.
	} Context;
	
	FuncCallContext *funcctx;
	Context *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		Relation aocsRel;
		AppendOnlyEntry *aoEntry;
		Relation pg_aocsseg_rel;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function
		 * calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(19, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "gp_tid",
						   TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "gp_xmin",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "gp_xmin_status",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "gp_xmin_distrib_id",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "gp_xmax",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "gp_xmax_status",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "gp_xmax_distrib_id",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "gp_command_id",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "gp_infomask",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "gp_update_tid",
						   TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 11, "gp_visibility",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 12, "segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 13, "column_num",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 14, "physical_segno",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 15, "tupcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 16, "eof",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 17, "eof_uncompressed",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 18, "modcount",
						   INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 19, "state",
						   INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->aocsRelOid = aocsRelOid;

		aocsRel = heap_open(aocsRelOid, NoLock);
		if(!RelationIsAoCols(aocsRel))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("'%s' is not an append-only columnar relation",
							RelationGetRelationName(aocsRel))));

		// Remember the number of columns.
		context->relnatts = aocsRel->rd_rel->relnatts;

		aoEntry = GetAppendOnlyEntry(aocsRelOid, SnapshotNow);
		
		pg_aocsseg_rel = heap_open(aoEntry->segrelid, NoLock);
		
		context->aocsSegfileArray = GetAllAOCSFileSegInfo_pg_aocsseg_rel(
														RelationGetNumberOfAttributes(aocsRel),
														RelationGetRelationName(aocsRel), 
														aoEntry, 
														pg_aocsseg_rel,
														SnapshotAny,	// Get ALL tuples from pg_aocsseg_% including aborted and in-progress ones. 
														&context->totalAocsSegFiles);

		heap_close(pg_aocsseg_rel, NoLock);
		heap_close(aocsRel, NoLock);

		// Iteration positions.
		context->segfileArrayIndex = 0;
		context->columnNum = 0;

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	/*
	 * Process each column for each segment file.
	 */
	while (true)
	{
		Datum		values[19];
		bool		nulls[19];
		HeapTuple	tuple;
		Datum		result;

		struct AOCSFileSegInfo *aocsSegfile;

		AOCSVPInfoEntry *entry;
		
		if (context->segfileArrayIndex >= context->totalAocsSegFiles)
		{
			break;
		}

		if (context->columnNum >= context->relnatts)
		{
			/*
			 * Finished with the current segment file.
			 */
			context->segfileArrayIndex++;
			if (context->segfileArrayIndex >= context->totalAocsSegFiles)
			{
				continue;
			}

			context->columnNum = 0;
		}
		
		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		aocsSegfile = context->aocsSegfileArray[context->segfileArrayIndex];

		entry = getAOCSVPEntry(aocsSegfile, context->columnNum);

		GetTupleVisibilitySummaryDatums(
								&values[0],
								&nulls[0],
								&aocsSegfile->tupleVisibilitySummary);

		values[11] = Int32GetDatum(aocsSegfile->segno);
		values[12] = Int16GetDatum(context->columnNum);
		values[13] = Int32GetDatum(context->columnNum * AOTupleId_MultiplierSegmentFileNum + aocsSegfile->segno);
		values[14] = Int64GetDatum(aocsSegfile->total_tupcount);
		values[15] = Int64GetDatum(entry->eof);
		values[16] = Int64GetDatum(entry->eof_uncompressed);
		values[17] = Int64GetDatum(aocsSegfile->modcount);
		values[18] = Int16GetDatum(aocsSegfile->state);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		// Indicate we emitted one column.
		context->columnNum++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

Datum
gp_update_aocol_master_stats_internal(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot)
{
	StringInfoData	sqlstmt;
	Relation		aosegrel;
	bool			connected = false;
	char			aoseg_relname[NAMEDATALEN];
	int				proc; /* 32 bit, only holds number of segments */
	int				ret;
	float8			total_count = 0;
	MemoryContext	oldcontext = CurrentMemoryContext;
    int32			nvp = RelationGetNumberOfAttributes(parentrel);
	AppendOnlyEntry *aoEntry = GetAppendOnlyEntry(RelationGetRelid(parentrel), appendOnlyMetaDataSnapshot);

	Assert(aoEntry != NULL);
	
	/*
	 * get the name of the aoseg relation
	 */
	aosegrel = heap_open(aoEntry->segrelid, AccessShareLock);
	snprintf(aoseg_relname, NAMEDATALEN, "%s", RelationGetRelationName(aosegrel));
	heap_close(aosegrel, AccessShareLock);

	/*
	 * assemble our query string
	 */
	initStringInfo(&sqlstmt);
	appendStringInfo(&sqlstmt, "select segno,sum(tupcount) "
					"from gp_dist_random('pg_aoseg.%s') "
					"group by (segno)", aoseg_relname);


	PG_TRY();
	{

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
							errmsg("Unable to obtain AO relation information from segment databases."),
							errdetail("SPI_connect failed in gp_update_ao_master_stats")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, false, 0);
		proc = (int) SPI_processed;


		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			int i;

			/*
			 * Iterate through each result tuple
			 */
			for (i = 0; i < proc; i++)
			{
				HeapTuple	tuple = tuptable->vals[i];
				AOCSFileSegInfo *aocsfsinfo = NULL;
				int			qe_segno;
				int64		qe_tupcount;
				char		*val_segno;
				char		*val_tupcount;
				MemoryContext	cxt_save;

				/*
				 * Get totals from QE's for a specific segment
				 */
				val_segno = SPI_getvalue(tuple, tupdesc, 1);
				val_tupcount = SPI_getvalue(tuple, tupdesc, 2);

				/* use our own context so that SPI won't free our stuff later */
				cxt_save = MemoryContextSwitchTo(oldcontext);

				/*
				 * Convert to desired data type
				 */
				qe_segno = pg_atoi(val_segno, sizeof(int32), 0);
				qe_tupcount = DatumGetInt64(DirectFunctionCall1(int8in,
																CStringGetDatum(val_tupcount)));

				total_count += qe_tupcount;

				/*
				 * Get the numbers on the QD for this segment
				 */
					
				
				// CONSIDER: For integrity, we should lock ALL segment files first before 
				// executing the query.  And, the query of the segments (the SPI_execute)
				// and the update (UpdateFileSegInfo) should be in the same transaction.
				//
				// If there are concurrent Append-Only inserts, we can end up with
				// the wrong answer...
				//
				// NOTE: This is a transaction scope lock that must be held until commit / abort.
				//
				LockRelationAppendOnlySegmentFile(
												&parentrel->rd_node,
												qe_segno,
												AccessExclusiveLock,
												/* dontWait */ false);
					
				aocsfsinfo = GetAOCSFileSegInfo(parentrel, aoEntry, appendOnlyMetaDataSnapshot, qe_segno);
				if (aocsfsinfo == NULL)
				{
					InsertInitialAOCSFileSegInfo(aoEntry->segrelid, qe_segno, nvp);

					aocsfsinfo = NewAOCSFileSegInfo(qe_segno, nvp);
				}

				/*
				 * check if numbers match.
				 * NOTE: proper way is to use int8eq() but since we
				 * don't expect any NAN's in here better do it directly
				 */
				if(aocsfsinfo->total_tupcount != qe_tupcount)
				{
					int64	diff = qe_tupcount - aocsfsinfo->total_tupcount;

					elog(DEBUG3, "gp_update_ao_master_stats: updating "
						"segno %d with tupcount %d", qe_segno,
						(int)qe_tupcount);

					/*
					 * QD tup count !=  QE tup count. update QD count by
					 * passing in the diff (may be negative sometimes).
					 */
					AOCSFileSegInfoAddCount(parentrel, aoEntry, qe_segno, diff, 0, 1);
				}
				else
					elog(DEBUG3, "gp_update_ao_master_stats: no need to "
						"update segno %d. it is synced", qe_segno);

				pfree(aocsfsinfo);

				MemoryContextSwitchTo(cxt_save);

				/*
				 * TODO: if an entry exists for this rel in the AO hash table
				 * need to also update that entry in shared memory. Need to
				 * figure out how to do this safely when concurrent operations
				 * are in progress. note that if no entry exists we are ok.
				 *
				 * At this point this doesn't seem too urgent as we generally
				 * only expect this function to update segno 0 only and the QD
				 * never cares about segment 0 anyway.
				 */
			}
		}

		connected = false;

		SPI_finish();
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(aoEntry);
		
	pfree(sqlstmt.data);

	PG_RETURN_FLOAT8(total_count);
}

Datum
aocol_compression_ratio_internal(Relation parentrel)
{
	StringInfoData	sqlstmt;
	Relation		aosegrel;
	bool			connected = false;
	char			aocsseg_relname[NAMEDATALEN];
	int				proc; /* 32 bit, only holds number of segments */
	int				ret;
	int64			eof = 0;
	int64			eof_uncompressed = 0;
	float8			compress_ratio = -1; /* the default, meaning "not available" */

	MemoryContext	oldcontext = CurrentMemoryContext;
	Oid segrelid = InvalidOid;

	GetAppendOnlyEntryAuxOids(RelationGetRelid(parentrel), SnapshotNow,
							  &segrelid, NULL, NULL, NULL, NULL, NULL);
	Assert(OidIsValid(segrelid));

	/*
	 * get the name of the aoseg relation
	 */
	aosegrel = heap_open(segrelid, AccessShareLock);
	snprintf(aocsseg_relname, NAMEDATALEN, "%s", RelationGetRelationName(aosegrel));
	heap_close(aosegrel, AccessShareLock);

	/*
	 * assemble our query string.
	 *
	 * NOTE: The aocsseg (per table) system catalog lives in the gp_aoseg namespace, too.
	 */
	initStringInfo(&sqlstmt);
	if (Gp_role == GP_ROLE_DISPATCH)
		appendStringInfo(&sqlstmt, "select vpinfo "
								"from gp_dist_random('pg_aoseg.%s')",
									aocsseg_relname);
	else
		appendStringInfo(&sqlstmt, "select vpinfo "
								"from pg_aoseg.%s",
									aocsseg_relname);

	PG_TRY();
	{

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
							errmsg("Unable to obtain AO relation information from segment databases."),
							errdetail("SPI_connect failed in get_ao_compression_ratio")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, false, 0);
		proc = (int) SPI_processed;


		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc		tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable*	tuptable = SPI_tuptable;
			int				i;
			HeapTuple		tuple;
			bool			isnull;
			Datum			vpinfoDatum;
			AOCSVPInfo		*vpinfo;
			int				j;
			MemoryContext	cxt_save;

			for (i = 0; i < proc; i++)
			{
				/*
				 * Each row is a binary struct vpinfo with a variable number of entries
				 * on the end.
				 */
				tuple = tuptable->vals[i];
				
				vpinfoDatum = heap_getattr(tuple, 1, tupdesc, &isnull);
				if (isnull)
					break;

				vpinfo = (AOCSVPInfo*)DatumGetByteaP(vpinfoDatum);
				
				// CONSIDER: Better verification of vpinfo.
				Assert(vpinfo->version == 0);
				for (j = 0; j < vpinfo->nEntry; j++)
				{
					eof += vpinfo->entry[j].eof;
					eof_uncompressed += vpinfo->entry[j].eof_uncompressed;
				}
			}

			/* use our own context so that SPI won't free our stuff later */
			cxt_save = MemoryContextSwitchTo(oldcontext);

			/* guard against division by zero */
			if (eof > 0)
			{
				char  buf[8];

				/* calculate the compression ratio */
				float8 compress_ratio_raw = 
						((float8)eof_uncompressed) /
												((float8)eof);

				/* format to 2 digits past the decimal point */
				sprintf(buf, "%.2f", compress_ratio_raw);

				/* format to 2 digit decimal precision */
				compress_ratio = DatumGetFloat8(DirectFunctionCall1(float8in,
												CStringGetDatum(buf)));
			}

			MemoryContextSwitchTo(cxt_save);

		}

		connected = false;
		SPI_finish();
	}

	/* Clean up in case of error. */
	PG_CATCH();
	{
		if (connected)
			SPI_finish();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();


	pfree(sqlstmt.data);

	PG_RETURN_FLOAT8(compress_ratio);
}

/**
 * Free up seginfo array.
 */
void
FreeAllAOCSSegFileInfo(AOCSFileSegInfo **allAOCSSegInfo, int totalSegFiles)
{
	Assert(allAOCSSegInfo);

	for(int file_no = 0; file_no < totalSegFiles; file_no++)
	{
		Assert(allAOCSSegInfo[file_no] != NULL);

		pfree(allAOCSSegInfo[file_no]);
	}
}


void
PrintPgaocssegAndGprelationNodeEntries(AOCSFileSegInfo **allseginfo, int totalsegs, bool *segmentFileNumMap)
{
	char segnumArray[600];
	char delimiter[5] = " ";
	char tmp[10] = {0};
	memset(segnumArray, 0, sizeof(segnumArray));

	char		*head = segnumArray;
	const char	*tail = segnumArray + sizeof(segnumArray);

	for (int i = 0 ; i < totalsegs; i++)
	{
		snprintf(tmp, sizeof(tmp), "%d", allseginfo[i]->segno);

		if (strlen(tmp) + strlen(delimiter) >= (tail - head))
			break;

		head += strlcpy(head, tmp, tail - head);
		head += strlcpy(head, delimiter, tail - head);
	}
	elog(LOG, "pg_aocsseg segno entries: %s", segnumArray);

	memset(segnumArray, 0, sizeof(segnumArray));
	head = segnumArray;

	for (int i = 0; i < AOTupleId_MaxSegmentFileNum; i++)
	{
		if (segmentFileNumMap[i] == true)
		{
			snprintf(tmp, sizeof(tmp), "%d", i);

			if (strlen(tmp) + strlen(delimiter) >= (tail - head))
				break;

			head += strlcpy(head, tmp, tail - head);
			head += strlcpy(head, delimiter, tail - head);
		}
	}
	elog(LOG, "gp_relation_node segno entries: %s", segnumArray);
}

void
CheckCOConsistencyWithGpRelationNode( Snapshot snapshot, Relation rel, int totalsegs, AOCSFileSegInfo ** allseginfo)
{
	GpRelationNodeScan gpRelationNodeScan;
	int segmentFileNum = 0;
	ItemPointerData persistentTid;
	int64 persistentSerialNum = 0;
	int segmentCount = 0;
	Relation gp_relation_node;

	if (!gp_appendonly_verify_eof)
	{
		return;
	}

	/*
	 * gp_relation_node always has a zero as its first entry. Hence we use Segment File number
	 * plus one in order to accomodate the zero.
	 */
	const int num_gp_relation_node_entries = AOTupleId_MaxSegmentFileNum + 1;
	bool *segmentFileNumMap = (bool*) palloc0( sizeof(bool) * num_gp_relation_node_entries);

	gp_relation_node = heap_open(GpRelationNodeRelationId, AccessShareLock);
	GpRelationNodeBeginScan(
					snapshot,
					gp_relation_node,
					rel->rd_id,
					rel->rd_rel->relfilenode,
					&gpRelationNodeScan);
	while ((NULL != GpRelationNodeGetNext(
						&gpRelationNodeScan,
						&segmentFileNum,
						&persistentTid,
						&persistentSerialNum)))
	{
		if (!segmentFileNumMap[segmentFileNum % num_gp_relation_node_entries])
		{
			segmentFileNumMap[segmentFileNum % num_gp_relation_node_entries] = true;
			segmentCount++;
		}

		if (segmentCount > totalsegs + 1)
		{
			elog(ERROR, "gp_relation_node (%d) has more entries than pg_aocsseg (%d) for relation %s",
				segmentCount,
				totalsegs,
				RelationGetRelationName(rel));
		}
	}

	GpRelationNodeEndScan(&gpRelationNodeScan);
	heap_close(gp_relation_node, AccessShareLock);

	for (int i = 0, j = 1; j < num_gp_relation_node_entries; j++)
	{
		if (segmentFileNumMap[j])
		{
			while (i < totalsegs && allseginfo[i]->segno != j)
			{
				i++;
			}

			if (i == totalsegs)
			{
				PrintPgaocssegAndGprelationNodeEntries(allseginfo, totalsegs, segmentFileNumMap);
				elog(ERROR, "Missing gp_relation_node entry %d in pg_aocsseg for relation %s",
					j,
					RelationGetRelationName(rel));
			}
		}
	}

	pfree(segmentFileNumMap);
}

