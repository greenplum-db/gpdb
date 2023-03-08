/*-------------------------------------------------------------------------
 *
 * aocssegfiles.c
 *	  AOCS Segment files.
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/aocs/aocssegfiles.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

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
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/numeric.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "utils/datumstream.h"
#include "utils/int8.h"
#include "utils/fmgroids.h"
#include "access/aocssegfiles.h"
#include "access/aosegfiles.h"
#include "access/appendonlywriter.h"
#include "cdb/cdbaocsam.h"
#include "executor/executor.h"
#include "storage/lmgr.h"
#include "storage/procarray.h"

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"


static AOCSFileSegInfo **GetAllAOCSFileSegInfo_pg_aocsseg_rel(
									 Relation rel,
									 Relation pg_aocsseg_rel,
									 Snapshot appendOnlyMetaDataSnapshot,
									 int32 *totalseg);

void
InsertInitialAOCSFileSegInfo(Relation prel, int32 segno, int32 nvp, Oid segrelid)
{
	bool	   *nulls = palloc0(sizeof(bool) * Natts_pg_aocsseg);
	Datum	   *values = palloc0(sizeof(Datum) * Natts_pg_aocsseg);
	AOCSVPInfo *vpinfo = create_aocs_vpinfo(nvp);
	HeapTuple	segtup;
	Relation	segrel;
	Buffer		buf = InvalidBuffer;
	TM_Result	result;
	TM_FailureData hufd;
	int16		formatVersion;

	ValidateAppendonlySegmentDataBeforeStorage(segno);

	/* New segments are always created in the latest format */
	formatVersion = AOSegfileFormatVersion_GetLatest();

	segrel = heap_open(segrelid, RowExclusiveLock);

	values[Anum_pg_aocs_segno - 1] = Int32GetDatum(segno);
	values[Anum_pg_aocs_vpinfo - 1] = PointerGetDatum(vpinfo);
	values[Anum_pg_aocs_tupcount - 1] = Int64GetDatum(0);
	values[Anum_pg_aocs_varblockcount - 1] = Int64GetDatum(0);
	values[Anum_pg_aocs_formatversion - 1] = Int16GetDatum(formatVersion);
	values[Anum_pg_aocs_state - 1] = Int16GetDatum(AOSEG_STATE_DEFAULT);

	segtup = heap_form_tuple(RelationGetDescr(segrel), values, nulls);

	CatalogTupleInsertFrozen(segrel, segtup);

	/*
	 * Lock the tuple so that a concurrent insert transaction will not
	 * consider this segfile for insertion. This should succeed since
	 * we just inserted the row, and the caller is holding a lock that
	 * prevents concurrent lockers.
	 */
	result = heap_lock_tuple(segrel, segtup,
							 GetCurrentCommandId(true),
							 LockTupleExclusive,
							 LockWaitSkip,
							 false, /* follow_updates */
							 &buf,
							 &hufd);
	if (result != TM_Ok)
		elog(ERROR, "could not lock newly-inserted gp_fastsequence tuple");
	if (BufferIsValid(buf))
		ReleaseBuffer(buf);

	heap_freetuple(segtup);
	heap_close(segrel, RowExclusiveLock);

	pfree(vpinfo);
	pfree(nulls);
	pfree(values);
}

/*
 * This is a routine to extract the vpinfo underlying the untoasted datum from
 * the pg_aocsseg relation row, given the aocs relation's relnatts, into the supplied
 * AOCSFileSegInfo structure.
 *
 * Sometimes the number of columns represented in the vpinfo inside pg_aocsseg
 * the row may not match pg_class.relnatts. For instance, when we do an ADD
 * COLUMN operation, we will have lesser number of columns in the table row
 * than pg_class.relnatts.
 * On the other hand, following an aborted ADD COLUMN operation,
 * the number of columns in the table row will be
 * greater than pg_class.relnatts.
 */
static void
deformAOCSVPInfo(Relation rel, struct varlena *v, AOCSFileSegInfo *aocs_seginfo)
{
	int16 relnatts = RelationGetNumberOfAttributes(rel);
	struct varlena *dv = pg_detoast_datum(v);
	int source_size = VARSIZE(dv);
	int target_size = aocs_vpinfo_size(relnatts);


	if (source_size <= target_size)
	{
		/* The source fits into the target, simply memcpy. */
		memcpy(&aocs_seginfo->vpinfo, dv, source_size);
		Assert(aocs_seginfo->vpinfo.nEntry <= relnatts);
	}
	else
	{
		/*
		 * We have more columns represented in the vpinfo recorded inside the
		 * pg_aocsseg row, than pg_class.relnatts. Perform additional validation
		 * on these extra column entries and then simply copy over relnatts
		 * worth of entries from within the datum.
		 */
		AOCSVPInfo *vpInfo = (AOCSVPInfo *) dv;

		for (int i = relnatts; i < vpInfo->nEntry; ++i)
		{
			/*
			 * These extra entries must have be the initial frozen inserts
			 * from when InsertInitialAOCSFileSegInfo() was called during
			 * an aborted ADD COLUMN operation. Such entries should have eofs = 0,
			 * indicating that there is no data. If not, there is something
			 * seriously wrong. Yell appropriately.
			 */
			if(vpInfo->entry[i].eof > 0 || vpInfo->entry[i].eof_uncompressed > 0)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
							errmsg("For relation \"%s\" aborted column %d has non-zero eof %d or non-zero uncompressed eof %d",
								   RelationGetRelationName(rel), i, (int) vpInfo->entry[i].eof, (int) vpInfo->entry[i].eof_uncompressed)));
		}

		memcpy(&aocs_seginfo->vpinfo, dv, aocs_vpinfo_size(relnatts));
		aocs_seginfo->vpinfo.nEntry = relnatts;
	}

	if (dv != v)
		pfree(dv);
}

/*
 * GetAOCSFileSegInfo.
 *
 * Get the catalog entry for an appendonly (column-oriented) relation from the
 * pg_aocsseg_* relation that belongs to the currently used
 * AppendOnly table.
 *
 * If a caller intends to append to this (logical) file segment entry they
 * must have already locked the pg_aoseg tuple earlier, in order to guarantee
 * stability of the pg_aoseg information on this segment file and exclusive
 * right to append data to the segment file. In that case, 'locked' should be
 * passed as true.
 */
AOCSFileSegInfo *
GetAOCSFileSegInfo(Relation prel,
				   Snapshot appendOnlyMetaDataSnapshot,
				   int32 segno,
				   bool locked)
{
	int32		nvp = RelationGetNumberOfAttributes(prel);

	Relation	segrel;
	TupleDesc	tupdesc;
	SysScanDesc scan;
	HeapTuple	segtup = NULL;
	HeapTuple	fssegtup = NULL;
	int			tuple_segno = InvalidFileSegNumber;
	AOCSFileSegInfo *seginfo;
	Datum	   *d;
	bool	   *null;
	bool		isNull;
	Oid 		segrelid;

	GetAppendOnlyEntryAuxOids(prel,
					&segrelid, NULL, NULL);

	segrel = heap_open(segrelid, AccessShareLock);
	tupdesc = RelationGetDescr(segrel);

	/* Scan aoseg relation for tuple. */
	scan = systable_beginscan(segrel, InvalidOid, false, appendOnlyMetaDataSnapshot, 0, NULL);
	while ((segtup = systable_getnext(scan)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(segtup, Anum_pg_aocs_segno, tupdesc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&segtup->t_self))));

		if (segno == tuple_segno)
		{
			/* Check for duplicate aoseg entries with the same segno */
			if (fssegtup != NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("found two entries in %s with segno %d: ctid %s and ctid %s",
								RelationGetRelationName(segrel),
								segno,
								ItemPointerToString(&fssegtup->t_self),
								ItemPointerToString2(&segtup->t_self))));
			else
				fssegtup = heap_copytuple(segtup);
		}
	}

	if (!HeapTupleIsValid(fssegtup))
	{
		/* This segment file does not have an entry. */
		systable_endscan(scan);
		heap_close(segrel, AccessShareLock);

		if (locked)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("could not find segno %d for relation %s",
							segno, RelationGetRelationName(prel))));
		return NULL;
	}

	if (locked && !pg_aoseg_tuple_is_locked_by_me(fssegtup))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("segno %d for relation %s is not locked for us",
						segno, RelationGetRelationName(prel))));

	seginfo = (AOCSFileSegInfo *) palloc0(aocsfileseginfo_size(nvp));

	d = (Datum *) palloc(sizeof(Datum) * Natts_pg_aocsseg);
	null = (bool *) palloc(sizeof(bool) * Natts_pg_aocsseg);

	heap_deform_tuple(fssegtup, tupdesc, d, null);

	Assert(!null[Anum_pg_aocs_segno - 1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno - 1] == segno));
	seginfo   ->segno = segno;

	Assert(!null[Anum_pg_aocs_tupcount - 1]);
	seginfo   ->total_tupcount = DatumGetInt64(d[Anum_pg_aocs_tupcount - 1]);

	Assert(!null[Anum_pg_aocs_varblockcount - 1]);
	seginfo   ->varblockcount = DatumGetInt64(d[Anum_pg_aocs_varblockcount - 1]);

	Assert(!null[Anum_pg_aocs_modcount - 1]);
	seginfo   ->modcount = DatumGetInt64(d[Anum_pg_aocs_modcount - 1]);

	Assert(!null[Anum_pg_aocs_formatversion - 1]);
	seginfo   ->formatversion = DatumGetInt16(d[Anum_pg_aocs_formatversion - 1]);

	Assert(!null[Anum_pg_aocs_state - 1]);
	seginfo   ->state = DatumGetInt16(d[Anum_pg_aocs_state - 1]);

	Assert(!null[Anum_pg_aocs_vpinfo - 1]);
	deformAOCSVPInfo(prel,
					 (struct varlena *) DatumGetPointer(d[Anum_pg_aocs_vpinfo - 1]),
						 seginfo);

	pfree(d);
	pfree(null);

	heap_freetuple(fssegtup);
	systable_endscan(scan);
	heap_close(segrel, AccessShareLock);

	return seginfo;
}

AOCSFileSegInfo **
GetAllAOCSFileSegInfo(Relation prel,
					  Snapshot appendOnlyMetaDataSnapshot,
					  int32 *totalseg,
					  Oid *segrelidptr)
{
	Relation	pg_aocsseg_rel;
	AOCSFileSegInfo **results;
	Oid         segrelid;

	Assert(RelationIsAoCols(prel));

	GetAppendOnlyEntryAuxOids(prel,
							  &segrelid,
							  NULL, NULL);

	if (segrelid == InvalidOid)
		elog(ERROR, "could not find pg_aoseg aux table for AOCO table \"%s\"",
			 RelationGetRelationName(prel));

	if (segrelidptr != NULL)
		*segrelidptr = segrelid;

	pg_aocsseg_rel = relation_open(segrelid, AccessShareLock);

	results = GetAllAOCSFileSegInfo_pg_aocsseg_rel(prel,
												   pg_aocsseg_rel,
												   appendOnlyMetaDataSnapshot,
												   totalseg);

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
	AOCSFileSegInfo *leftSegInfo = *((AOCSFileSegInfo **) left);
	AOCSFileSegInfo *rightSegInfo = *((AOCSFileSegInfo **) right);

	if (leftSegInfo->segno < rightSegInfo->segno)
		return -1;

	if (leftSegInfo->segno > rightSegInfo->segno)
		return 1;

	return 0;
}
static AOCSFileSegInfo **
GetAllAOCSFileSegInfo_pg_aocsseg_rel(Relation rel,
									 Relation pg_aocsseg_rel,
									 Snapshot snapshot,
									 int32 *totalseg)
{

	SysScanDesc scan;
	HeapTuple	tup;

	AOCSFileSegInfo **allseg;
	AOCSFileSegInfo *aocs_seginfo;
	int			cur_seg;
	Datum	   *d;
	bool	   *null;
	int			seginfo_slot_no = AO_FILESEGINFO_ARRAY_SIZE;

	/*
	 * MPP-16407: Initialize the segment file information array, we first
	 * allocate 8 slot for the array, then array will be dynamically expanded
	 * later if necessary.
	 */
	allseg = (AOCSFileSegInfo **) palloc0(sizeof(AOCSFileSegInfo *) * seginfo_slot_no);
	d = (Datum *) palloc(sizeof(Datum) * Natts_pg_aocsseg);
	null = (bool *) palloc(sizeof(bool) * Natts_pg_aocsseg);

	cur_seg = 0;

	scan = systable_beginscan(pg_aocsseg_rel, InvalidOid, false, snapshot, 0, NULL);
	while ((tup = systable_getnext(scan)) != NULL)
	{
		/* dynamically expand space for AOCSFileSegInfo* array */
		if (cur_seg >= seginfo_slot_no)
		{
			seginfo_slot_no *= 2;
			allseg = (AOCSFileSegInfo **) repalloc(allseg, sizeof(AOCSFileSegInfo *) * seginfo_slot_no);
		}

		aocs_seginfo = (AOCSFileSegInfo *) palloc0(aocsfileseginfo_size(RelationGetNumberOfAttributes(rel)));

		allseg[cur_seg] = aocs_seginfo;

		heap_deform_tuple(tup, RelationGetDescr(pg_aocsseg_rel), d, null);

		Assert(!null[Anum_pg_aocs_segno - 1]);
		aocs_seginfo->segno = DatumGetInt32(d[Anum_pg_aocs_segno - 1]);

		Assert(!null[Anum_pg_aocs_tupcount - 1]);
		aocs_seginfo->total_tupcount = DatumGetInt64(d[Anum_pg_aocs_tupcount - 1]);

		Assert(!null[Anum_pg_aocs_varblockcount - 1]);
		aocs_seginfo->varblockcount = DatumGetInt64(d[Anum_pg_aocs_varblockcount - 1]);

		/*
		 * Modcount cannot be NULL in normal operation. However, when called
		 * from gp_aoseg_history after an upgrade, the old now invisible
		 * entries may have not set the state and the modcount.
		 */
		Assert(!null[Anum_pg_aocs_modcount - 1] || snapshot == SnapshotAny);
		if (!null[Anum_pg_aocs_modcount - 1])
			aocs_seginfo->modcount = DatumGetInt64(d[Anum_pg_aocs_modcount - 1]);

		Assert(!null[Anum_pg_aocs_formatversion - 1]);
		aocs_seginfo->formatversion = DatumGetInt16(d[Anum_pg_aocs_formatversion - 1]);

		Assert(!null[Anum_pg_aocs_state - 1] || snapshot == SnapshotAny);
		if (!null[Anum_pg_aocs_state - 1])
			aocs_seginfo->state = DatumGetInt16(d[Anum_pg_aocs_state - 1]);

		Assert(!null[Anum_pg_aocs_vpinfo - 1]);
		deformAOCSVPInfo(rel, (struct varlena *) DatumGetPointer(d[Anum_pg_aocs_vpinfo - 1]), aocs_seginfo);
		++cur_seg;
	}

	pfree(d);
	pfree(null);

	systable_endscan(scan);

	*totalseg = cur_seg;

	if (*totalseg == 0)
	{
		pfree(allseg);

		return NULL;
	}

	/*
	 * Sort allseg by the order of segment file number.
	 *
	 * Currently this is only needed when building a bitmap index to guarantee
	 * the tids are in the ascending order. But since this array is pretty
	 * small, we just sort the array for all cases.
	 */
	qsort((char *) allseg, *totalseg, sizeof(AOCSFileSegInfo *), aocsFileSegInfoCmp);

	return allseg;
}

/*
 * Summarize the pg_aocsseg metadata columns for a given AOCO relation using
 * appendOnlyMetaDataSnapshot.
 */
FileSegTotals *
GetAOCSSSegFilesTotals(Relation parentrel,
					   Snapshot appendOnlyMetaDataSnapshot)
{
	FileSegTotals 	*totals;
	AttrNumber 		num_proj_atts = RelationGetNumberOfAttributes(parentrel);
	AttrNumber 		*proj_atts = (AttrNumber *) palloc0(num_proj_atts * sizeof(AttrNumber));

	/*
	 * Construct a projection list containing all columns in the relation and
	 * then call GetAOCSSSegFilesTotalsWithProj() with it, to obtain summarized
	 * aocsseg values for all columns.
	 */
	for (AttrNumber attno = 0; attno < num_proj_atts; attno++)
		proj_atts[attno] = attno;
	totals = GetAOCSSSegFilesTotalsWithProj(parentrel,
											appendOnlyMetaDataSnapshot,
											proj_atts,
											num_proj_atts);

	pfree(proj_atts);

	return totals;
}

/*
 * Summarize the pg_aocsseg metadata columns for a given AOCO relation using
 * appendOnlyMetaDataSnapshot. However, only consider the metadata values for
 * columns that belong to the passed in projection list: "proj_atts".
 */
FileSegTotals *
GetAOCSSSegFilesTotalsWithProj(Relation parentrel,
							   Snapshot appendOnlyMetaDataSnapshot,
							   AttrNumber *proj_atts,
							   AttrNumber num_proj_atts)
{
	AOCSFileSegInfo **allseg;
	int			totalseg;
	int			s;
	AOCSVPInfo *vpinfo;
	FileSegTotals *totals;

	Assert(RelationIsValid(parentrel));
	Assert(RelationIsAoCols(parentrel));

	/*
	 * The projection list must be non-empty. If there are no columns projected,
	 * i.e. all columns must be considered, then proj_atts should be an array
	 * containing each and every column number.
	 */
	Assert(num_proj_atts > 0);
	Assert(proj_atts);

	totals = (FileSegTotals *) palloc0(sizeof(FileSegTotals));
	memset(totals, 0, sizeof(FileSegTotals));

	allseg = GetAllAOCSFileSegInfo(parentrel, appendOnlyMetaDataSnapshot, &totalseg, NULL);
	for (s = 0; s < totalseg; s++)
	{
		int			e;

		vpinfo = &((allseg[s])->vpinfo);

		for (e = 0; e < num_proj_atts; e++)
		{
			int col = proj_atts[e];
			totals->totalbytes += vpinfo->entry[col].eof;
			totals->totalbytesuncompressed += vpinfo->entry[col].eof_uncompressed;
		}
		if (allseg[s]->state != AOSEG_STATE_AWAITING_DROP)
		{
			totals->totaltuples += allseg[s]->total_tupcount;
		}
		totals->totalvarblocks += allseg[s]->varblockcount;
		totals->totalfilesegs++;
	}

	if (allseg)
	{
		FreeAllAOCSSegFileInfo(allseg, totalseg);
		pfree(allseg);
	}

	return totals;
}

/*
 * Change an pg_aoseg row from DEFAULT to AWAITING_DROP to DEFAULT.
 */
void
MarkAOCSFileSegInfoAwaitingDrop(Relation prel, int segno)
{
	Relation	segrel;
	TableScanDesc	scan;
	HeapTuple	oldtup = NULL;
	HeapTuple	newtup;
	int			tuple_segno = InvalidFileSegNumber;
	Datum		d[Natts_pg_aocsseg];
	bool		isNull;
	bool		null[Natts_pg_aocsseg] = {0,};
	bool		repl[Natts_pg_aocsseg] = {0,};
	TupleDesc	tupdesc;
	Snapshot	appendOnlyMetaDataSnapshot;
	Oid			segrelid;

	if (Debug_appendonly_print_compaction)
		elog(LOG,
			 "changing state of segfile %d of table '%s' to AWAITING_DROP",
			 segno, RelationGetRelationName(prel));

	Assert(RelationIsAoCols(prel));

	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
	GetAppendOnlyEntryAuxOids(prel,
							  &segrelid,
							  NULL, NULL);
	UnregisterSnapshot(appendOnlyMetaDataSnapshot);

	segrel = heap_open(segrelid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	/*
	 * Since we have the segment-file entry under lock (with
	 * LockRelationAppendOnlySegmentFile) we can use SnapshotNow.
	 */
	scan = table_beginscan_catalog(segrel, 0, NULL);
	while (segno != tuple_segno && (oldtup = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&oldtup->t_self))));
	}

	if (!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
							   RelationGetRelationName(prel), segno)
						));

	/*
	 * Verify that the caller locked the segment earlier. In principle, if
	 * the caller is holding an AccessExclusiveLock on the table, locking
	 * individual tuples would not be necessary, but all current callers
	 * diligently lock the tuples anyway, so we can perform this sanity check
	 * here.
	 */
	if (!pg_aoseg_tuple_is_locked_by_me(oldtup))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("cannot update pg_aocs entry for segno %d for relation %s, it is not locked for us",
						segno, RelationGetRelationName(prel))));

	d[Anum_pg_aocs_state - 1] = Int16GetDatum(AOSEG_STATE_AWAITING_DROP);
	repl[Anum_pg_aocs_state - 1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);

	pfree(newtup);

	table_endscan(scan);
	table_close(segrel, RowExclusiveLock);
}

/*
 * Reset state of an  pg_aocs row from AWAITING_DROP to DEFAULT state.
 *
 * Also clears tupcount, varblockcount, and EOFs, and sets formatversion to
 * the latest version. 'modcount' is not changed.
 *
 * The caller should have checked that the segfile is no longer needed by
 * any running transaction. It is not necessary to hold a lock on the segfile
 * row, though.
 */
void
ClearAOCSFileSegInfo(Relation prel, int segno)
{
	Relation	segrel;
	TableScanDesc	scan;
	HeapTuple	oldtup = NULL;
	HeapTuple	newtup;
	int			tuple_segno = InvalidFileSegNumber;
	Datum		d[Natts_pg_aocsseg];
	bool		isNull;
	bool		null[Natts_pg_aocsseg] = {0,};
	bool		repl[Natts_pg_aocsseg] = {0,};
	TupleDesc	tupdesc;
	int			nvp = RelationGetNumberOfAttributes(prel);
	int			i;
	AOCSVPInfo	*vpinfo = create_aocs_vpinfo(nvp);
	Oid			segrelid;
	Snapshot	appendOnlyMetaDataSnapshot;

	Assert(RelationIsAoCols(prel));

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Clear seg file info: segno %d table '%s'",
		   segno,
		   RelationGetRelationName(prel));

	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
	GetAppendOnlyEntryAuxOids(prel,
							  &segrelid,
							  NULL, NULL);
	UnregisterSnapshot(appendOnlyMetaDataSnapshot);

	segrel = heap_open(segrelid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	/*
	 * Since we have the segment-file entry under lock (with
	 * LockRelationAppendOnlySegmentFile) we can use SnapshotNow.
	 */
	scan = table_beginscan_catalog(segrel, 0, NULL);
	while (segno != tuple_segno && (oldtup = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&oldtup->t_self))));
	}

	if (!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
							   RelationGetRelationName(prel), segno)
						));

#ifdef USE_ASSERT_CHECKING
	d[Anum_pg_aocs_segno - 1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno - 1]);
	Assert(!null[Anum_pg_aocs_segno - 1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno - 1]) == segno);
#endif

	d[Anum_pg_aocs_tupcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount, tupdesc, &null[Anum_pg_aocs_tupcount - 1]);
	Assert(!null[Anum_pg_aocs_tupcount - 1]);

	d[Anum_pg_aocs_tupcount - 1] = 0;
	repl[Anum_pg_aocs_tupcount - 1] = true;

	d[Anum_pg_aocs_varblockcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_varblockcount, tupdesc, &null[Anum_pg_aocs_varblockcount - 1]);
	Assert(!null[Anum_pg_aocs_varblockcount - 1]);
	d[Anum_pg_aocs_varblockcount - 1] = 0;
	repl[Anum_pg_aocs_varblockcount - 1] = true;

	/* When the segment is later recreated, it will be in new format */
	d[Anum_pg_aocs_formatversion - 1] = Int16GetDatum(AOSegfileFormatVersion_GetLatest());
	repl[Anum_pg_aocs_formatversion - 1] = true;

	/* We do not reset the modcount here */

	for (i = 0; i < nvp; ++i)
	{
		vpinfo->entry[i].eof = 0;
		vpinfo->entry[i].eof_uncompressed = 0;
	}
	d[Anum_pg_aocs_vpinfo - 1] = PointerGetDatum(vpinfo);
	null[Anum_pg_aocs_vpinfo - 1] = false;
	repl[Anum_pg_aocs_vpinfo - 1] = true;

	d[Anum_pg_aocs_state - 1] = Int16GetDatum(AOSEG_STATE_DEFAULT);
	repl[Anum_pg_aocs_state - 1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);

	pfree(newtup);
	pfree(vpinfo);

	table_endscan(scan);
	table_close(segrel, RowExclusiveLock);
}

void
UpdateAOCSFileSegInfo(AOCSInsertDesc idesc)
{

	Relation	prel = idesc->aoi_rel;
	Relation	segrel;
	SysScanDesc scan;

	HeapTuple	oldtup = NULL;
	HeapTuple	newtup;
	int			tuple_segno = InvalidFileSegNumber;
	bool		isNull;
	Datum		d[Natts_pg_aocsseg];
	bool		null[Natts_pg_aocsseg] = {0,};
	bool		repl[Natts_pg_aocsseg] = {0,};

	TupleDesc	tupdesc;
	int			nvp = RelationGetNumberOfAttributes(prel);
	int			i;
	AOCSVPInfo *vpinfo = create_aocs_vpinfo(nvp);

	segrel = heap_open(idesc->segrelid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	/*
	 * Since we have the segment-file entry under lock (with
	 * LockRelationAppendOnlySegmentFile) we can use SnapshotNow.
	 */
	scan = systable_beginscan(segrel, InvalidOid, false, idesc->appendOnlyMetaDataSnapshot, 0, NULL);
	while (idesc->cur_segno != tuple_segno && (oldtup = systable_getnext(scan)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&oldtup->t_self))));
	}

	if (!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
							   RelationGetRelationName(prel), idesc->cur_segno)
						));

	/*
	 * Verify that the caller locked the segment earlier. In principle, if
	 * the caller is holding an AccessExclusiveLock on the table, locking
	 * individual tuples would not be necessary, but all current callers
	 * diligently lock the tuples anyway, so we can perform this sanity check
	 * here.
	 */
	if (!pg_aoseg_tuple_is_locked_by_me(oldtup))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("cannot update pg_aocs entry for segno %d for relation %s, it is not locked for us",
						idesc->cur_segno, RelationGetRelationName(prel))));

#ifdef USE_ASSERT_CHECKING
	d[Anum_pg_aocs_segno - 1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno - 1]);
	Assert(!null[Anum_pg_aocs_segno - 1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno - 1]) == idesc->cur_segno);
#endif

	d[Anum_pg_aocs_tupcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount, tupdesc, &null[Anum_pg_aocs_tupcount - 1]);
	Assert(!null[Anum_pg_aocs_tupcount - 1]);

	d[Anum_pg_aocs_tupcount - 1] += idesc->insertCount;
	repl[Anum_pg_aocs_tupcount - 1] = true;


	d[Anum_pg_aocs_varblockcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_varblockcount, tupdesc, &null[Anum_pg_aocs_varblockcount - 1]);
	Assert(!null[Anum_pg_aocs_varblockcount - 1]);
	d[Anum_pg_aocs_varblockcount - 1] += idesc->varblockCount;
	repl[Anum_pg_aocs_varblockcount - 1] = true;

	if (!idesc->skipModCountIncrement)
	{
		d[Anum_pg_aocs_modcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_modcount, tupdesc, &null[Anum_pg_aocs_modcount - 1]);
		Assert(!null[Anum_pg_aocs_modcount - 1]);
		d[Anum_pg_aocs_modcount - 1] += 1;
		repl[Anum_pg_aocs_modcount - 1] = true;
	}

	/*
	 * Lets fetch the vpinfo structure from the existing tuple in pg_aocsseg.
	 * vpinfo provides us with the end-of-file (EOF) values for each column
	 * file.
	 */
	Datum		d_tmp = fastgetattr(oldtup, Anum_pg_aocs_vpinfo, tupdesc,
									&null[Anum_pg_aocs_vpinfo - 1]);

	Assert(!null[Anum_pg_aocs_vpinfo - 1]);
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
	for (i = 0; i < nvp; ++i)
	{
		/*
		 * For CO by design due to append-only nature, new end-of-file (EOF)
		 * to be recorded in aoseg table has to be greater than currently
		 * stored EOF value, as new writes must move it forward only. If new
		 * end-of-file value is less than currently stored end-of-file
		 * something is incorrect and updating the same will yield incorrect
		 * result during reads. Hence abort the write transaction trying to
		 * update the incorrect EOF value.
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
				 "EOF " INT64_FORMAT " to be updated cannot be smaller than current EOF " INT64_FORMAT " in pg_aocsseg",
				 RelationGetRelationName(prel), prel->rd_node.relNode,
				 idesc->cur_segno, i, idesc->ds[i]->eofUncompress, oldvpinfo->entry[i].eof_uncompressed);
		}
	}

	if (dv != v)
	{
		pfree(dv);
	}

	d[Anum_pg_aocs_vpinfo - 1] = PointerGetDatum(vpinfo);
	null[Anum_pg_aocs_vpinfo - 1] = false;
	repl[Anum_pg_aocs_vpinfo - 1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);

	pfree(newtup);
	pfree(vpinfo);

	systable_endscan(scan);
	heap_close(segrel, RowExclusiveLock);
}

/*
 * Update vpinfo column of pg_aocsseg_* by adding new
 * AOCSVPInfoEntries.  One VPInfoEntry is added for each newly added
 * segfile (column).  If empty=true, add empty VPInfoEntry's having
 * eof=0.
 */
void
AOCSFileSegInfoAddVpe(Relation prel, int32 segno,
					  AOCSAddColumnDesc desc, int num_newcols, bool empty)
{
	LockAcquireResult acquireResult;

	Relation	segrel;
	SysScanDesc scan;

	AOCSVPInfo *oldvpinfo;
	AOCSVPInfo *newvpinfo;
	HeapTuple	oldtup = NULL;
	HeapTuple	newtup;
	int			tuple_segno = InvalidFileSegNumber;
	Datum		d[Natts_pg_aocsseg];
	bool		isNull;
	bool		null[Natts_pg_aocsseg] = {0,};
	bool		repl[Natts_pg_aocsseg] = {0,};

	TupleDesc	tupdesc;
	int			nvp = RelationGetNumberOfAttributes(prel);

	/* nvp is new columns + existing columns */
	int			i;
	int			j;

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
	if (acquireResult != LOCKACQUIRE_ALREADY_HELD && acquireResult != LOCKACQUIRE_ALREADY_CLEAR)
	{
		elog(ERROR, "should already have (transaction-scope) AccessExclusive"
			 " lock on relation %s, oid %d",
			 RelationGetRelationName(prel), RelationGetRelid(prel));
	}

    Oid         segrelid;
    GetAppendOnlyEntryAuxOids(prel,
                              &segrelid,
                              NULL, NULL);
	segrel = heap_open(segrelid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	/*
	 * Since we have the segment-file entry under lock (with
	 * LockRelationAppendOnlySegmentFile) we can use SnapshotNow.
	 */
	scan = systable_beginscan(segrel, InvalidOid, false, NULL, 0, NULL);
	while (segno != tuple_segno && (oldtup = systable_getnext(scan)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&oldtup->t_self))));
	}

	if (!HeapTupleIsValid(oldtup))
	{
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("AOCS rel \"%s\" segment \"%d\" does not exist",
							   RelationGetRelationName(prel), segno)
						));
	}

	d[Anum_pg_aocs_segno - 1] = fastgetattr(oldtup, Anum_pg_aocs_segno,
											tupdesc, &null[Anum_pg_aocs_segno - 1]);
	Assert(!null[Anum_pg_aocs_segno - 1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno - 1]) == segno);

	d[Anum_pg_aocs_tupcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount,
											   tupdesc,
											   &null[Anum_pg_aocs_tupcount - 1]);
	Assert(!null[Anum_pg_aocs_tupcount - 1]);

	d[Anum_pg_aocs_modcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_modcount,
											   tupdesc,
											   &null[Anum_pg_aocs_modcount - 1]);
	Assert(!null[Anum_pg_aocs_modcount - 1]);
	d[Anum_pg_aocs_modcount - 1] += 1;
	repl[Anum_pg_aocs_modcount - 1] = true;

	/* new VPInfo having VPEntries with eof=0 */
	newvpinfo = create_aocs_vpinfo(nvp);
	if (!empty)
	{
		d[Anum_pg_aocs_vpinfo - 1] =
			fastgetattr(oldtup, Anum_pg_aocs_vpinfo, tupdesc,
						&null[Anum_pg_aocs_vpinfo - 1]);
		Assert(!null[Anum_pg_aocs_vpinfo - 1]);
		struct varlena *v = (struct varlena *) DatumGetPointer(
															   d[Anum_pg_aocs_vpinfo - 1]);
		struct varlena *dv = pg_detoast_datum(v);

		Assert(VARSIZE(dv) == aocs_vpinfo_size(nvp - num_newcols));
		oldvpinfo = (AOCSVPInfo *) dv;
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
		if (dv != v)
		{
			pfree(dv);
		}
	}
	d[Anum_pg_aocs_vpinfo - 1] = PointerGetDatum(newvpinfo);
	null[Anum_pg_aocs_vpinfo - 1] = false;
	repl[Anum_pg_aocs_vpinfo - 1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);

	pfree(newtup);
	pfree(newvpinfo);

	/*
	 * Holding RowExclusiveLock lock on pg_aocsseg_* until the ALTER TABLE
	 * transaction commits/aborts.  Additionally, we are already holding
	 * AccessExclusive lock on the AOCS relation OID.
	 */
	systable_endscan(scan);
	heap_close(segrel, NoLock);
}

void
AOCSFileSegInfoAddCount(Relation prel, int32 segno,
						int64 tupadded, int64 varblockadded, int64 modcount_added)
{
	Relation	segrel;
	SysScanDesc scan;

	HeapTuple	oldtup = NULL;
	HeapTuple	newtup;
	int			tuple_segno = InvalidFileSegNumber;
	Datum		d[Natts_pg_aocsseg];
	bool		isNull;
	bool		null[Natts_pg_aocsseg] = {0,};
	bool		repl[Natts_pg_aocsseg] = {0,};

	TupleDesc	tupdesc;

    Oid         segrelid;
    GetAppendOnlyEntryAuxOids(prel,
                              &segrelid,
                              NULL, NULL);

	segrel = heap_open(segrelid, RowExclusiveLock);
	tupdesc = RelationGetDescr(segrel);

	/*
	 * Since we have the segment-file entry under lock (with
	 * LockRelationAppendOnlySegmentFile) we can use SnapshotNow.
	 */
	scan = systable_beginscan(segrel, InvalidOid, false, NULL, 0, NULL);
	while (segno != tuple_segno && (oldtup = systable_getnext(scan)) != NULL)
	{
		tuple_segno = DatumGetInt32(fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &isNull));
		if (isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid segno value NULL for tid %s",
							ItemPointerToString(&oldtup->t_self))));
	}

	if (!HeapTupleIsValid(oldtup))
		ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT),
						errmsg("AOCS table \"%s\" file segment \"%d\" does not exist",
							   RelationGetRelationName(prel), segno)
						));

	/*
	 * Verify that the caller locked the segment earlier. In principle, if
	 * the caller is holding an AccessExclusiveLock on the table, locking
	 * individual tuples would not be necessary, but all current callers
	 * diligently lock the tuples anyway, so we can perform this sanity check
	 * here.
	 */
	if (!pg_aoseg_tuple_is_locked_by_me(oldtup))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("cannot update pg_aocs entry for segno %d for relation %s, it is not locked for us",
						segno, RelationGetRelationName(prel))));

#ifdef USE_ASSERT_CHECKING
	d[Anum_pg_aocs_segno - 1] = fastgetattr(oldtup, Anum_pg_aocs_segno, tupdesc, &null[Anum_pg_aocs_segno - 1]);
	Assert(!null[Anum_pg_aocs_segno - 1]);
	Assert(DatumGetInt32(d[Anum_pg_aocs_segno - 1]) == segno);
#endif

	d[Anum_pg_aocs_tupcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_tupcount, tupdesc, &null[Anum_pg_aocs_tupcount - 1]);
	Assert(!null[Anum_pg_aocs_tupcount - 1]);

	d[Anum_pg_aocs_tupcount - 1] += tupadded;
	repl[Anum_pg_aocs_tupcount - 1] = true;

	d[Anum_pg_aocs_varblockcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_varblockcount, tupdesc, &null[Anum_pg_aocs_varblockcount - 1]);
	Assert(!null[Anum_pg_aocs_varblockcount - 1]);
	d[Anum_pg_aocs_varblockcount - 1] += varblockadded;
	repl[Anum_pg_aocs_varblockcount - 1] = true;

	d[Anum_pg_aocs_modcount - 1] = fastgetattr(oldtup, Anum_pg_aocs_modcount, tupdesc, &null[Anum_pg_aocs_modcount - 1]);
	Assert(!null[Anum_pg_aocs_modcount - 1]);
	d[Anum_pg_aocs_modcount - 1] += modcount_added;
	repl[Anum_pg_aocs_modcount - 1] = true;

	newtup = heap_modify_tuple(oldtup, tupdesc, d, null, repl);

	simple_heap_update(segrel, &oldtup->t_self, newtup);

	heap_freetuple(newtup);

	systable_endscan(scan);
	heap_close(segrel, RowExclusiveLock);
}

extern Datum aocsvpinfo_decode(PG_FUNCTION_ARGS);
Datum
aocsvpinfo_decode(PG_FUNCTION_ARGS)
{
	AOCSVPInfo *vpinfo = (AOCSVPInfo *) PG_GETARG_BYTEA_P(0);
	int			i = PG_GETARG_INT32(1);
	int			j = PG_GETARG_INT32(2);
	int64		result;

	if (i < 0 || i >= vpinfo->nEntry)
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("invalid entry for decoding aocsvpinfo")
						));

	if (j < 0 || j > 1)
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("invalid entry for decoding aocsvpinfo")
						));

	if (j == 0)
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
		Oid			aocsRelOid;

		int			relnatts;

		struct AOCSFileSegInfo **aocsSegfileArray;

		int			totalAocsSegFiles;

		int			segfileArrayIndex;

		int			columnNum;
		/* 0-based index into columns. */
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		Relation	aocsRel;
		Relation	pg_aocsseg_rel;
		Snapshot	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetLatestSnapshot());
		Oid		segrelid;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(10);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT4OID, -1, 0);
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
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "formatversion",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "state",
						   INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->aocsRelOid = aocsRelOid;

		aocsRel = heap_open(aocsRelOid, AccessShareLock);
		if (!RelationIsAoCols(aocsRel))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("'%s' is not an append-only columnar relation",
							RelationGetRelationName(aocsRel))));

		/* Remember the number of columns. */
		context->relnatts = aocsRel->rd_rel->relnatts;

		GetAppendOnlyEntryAuxOids(aocsRel,
						&segrelid,
						NULL, NULL);
		pg_aocsseg_rel = heap_open(segrelid, AccessShareLock);

		context->aocsSegfileArray = GetAllAOCSFileSegInfo_pg_aocsseg_rel(
																		 aocsRel,
																		 pg_aocsseg_rel,
																		 appendOnlyMetaDataSnapshot,
																		 &context->totalAocsSegFiles);

		heap_close(pg_aocsseg_rel, AccessShareLock);
		heap_close(aocsRel, AccessShareLock);

		/* Iteration positions. */
		context->segfileArrayIndex = 0;
		context->columnNum = 0;

		funcctx->user_fctx = (void *) context;

		UnregisterSnapshot(appendOnlyMetaDataSnapshot);
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (Context *) funcctx->user_fctx;

	/*
	 * Process each column for each segment file.
	 */
	while (true)
	{
		Datum		values[10];
		bool		nulls[10];
		HeapTuple	tuple;
		Datum		result;
		struct AOCSFileSegInfo *aocsSegfile;
		int64		eof;
		int64		eof_uncompressed;

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

		if (context->columnNum >= aocsSegfile->vpinfo.nEntry)
		{
			/*
			 * AWAITING_DROP segments might be missing information for some
			 * (newly-added) columns.
			 */
			eof = -1;
			eof_uncompressed = -1;
		}
		else
		{
			AOCSVPInfoEntry *entry;

			entry = getAOCSVPEntry(aocsSegfile, context->columnNum);
			eof = entry->eof;
			eof_uncompressed = entry->eof_uncompressed;
		}

		values[0] = Int32GetDatum(GpIdentity.segindex);
		values[1] = Int32GetDatum(aocsSegfile->segno);
		values[2] = Int16GetDatum(context->columnNum);
		values[3] = Int32GetDatum(context->columnNum * AOTupleId_MultiplierSegmentFileNum + aocsSegfile->segno);
		values[4] = Int64GetDatum(aocsSegfile->total_tupcount);
		values[5] = Int64GetDatum(eof);
		values[6] = Int64GetDatum(eof_uncompressed);
		values[7] = Int64GetDatum(aocsSegfile->modcount);
		values[8] = Int16GetDatum(aocsSegfile->formatversion);
		values[9] = Int16GetDatum(aocsSegfile->state);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		/* Indicate we emitted one column. */
		context->columnNum++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

PG_FUNCTION_INFO_V1(gp_aocsseg);

extern Datum gp_aocsseg(PG_FUNCTION_ARGS);

Datum
gp_aocsseg(PG_FUNCTION_ARGS)
{
	int			aocsRelOid = PG_GETARG_OID(0);

	return gp_aocsseg_internal(fcinfo, aocsRelOid);
}

PG_FUNCTION_INFO_V1(gp_aocsseg_history);

extern Datum gp_aocsseg_history(PG_FUNCTION_ARGS);

Datum
gp_aocsseg_history(PG_FUNCTION_ARGS)
{
	int			aocsRelOid = PG_GETARG_OID(0);

	typedef struct Context
	{
		Oid			aocsRelOid;

		int			relnatts;

		struct AOCSFileSegInfo **aocsSegfileArray;

		int			totalAocsSegFiles;

		int			segfileArrayIndex;

		int			columnNum;
		/* 0-based index into columns. */
	} Context;

	FuncCallContext *funcctx;
	Context    *context;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		Relation	aocsRel;
		Relation	pg_aocsseg_rel;
		Oid		segrelid;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(10);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id",
						   INT4OID, -1, 0);
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
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "formatversion",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "state",
						   INT2OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (Context *) palloc(sizeof(Context));
		funcctx->user_fctx = (void *) context;

		context->aocsRelOid = aocsRelOid;

		aocsRel = heap_open(aocsRelOid, AccessShareLock);
		if (!RelationIsAoCols(aocsRel))
		{
			heap_close(aocsRel, AccessShareLock);
			ereport(ERROR,
			        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				        errmsg("'%s' is not an append-only columnar relation",
				               RelationGetRelationName(aocsRel))));
		}

		/* Remember the number of columns. */
		context->relnatts = aocsRel->rd_rel->relnatts;

		GetAppendOnlyEntryAuxOids(aocsRel,
						&segrelid,
						NULL, NULL);

		pg_aocsseg_rel = heap_open(segrelid, AccessShareLock);

		context->aocsSegfileArray = GetAllAOCSFileSegInfo_pg_aocsseg_rel(
																		 aocsRel,
																		 pg_aocsseg_rel,
																		 SnapshotAny, //Get ALL tuples from pg_aocsseg_ % including aborted and in - progress ones.
																		 & context->totalAocsSegFiles);

		heap_close(pg_aocsseg_rel, AccessShareLock);
		heap_close(aocsRel, AccessShareLock);

		/* Iteration positions. */
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
		Datum		values[10];
		bool		nulls[10];
		HeapTuple	tuple;
		Datum		result;
		struct AOCSFileSegInfo *aocsSegfile;
		int64		eof;
		int64		eof_uncompressed;

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

		if (context->columnNum >= aocsSegfile->vpinfo.nEntry)
		{
			/*
			 * AWAITING_DROP segments might be missing information for some
			 * (newly-added) columns
			 */
			eof = -1;
			eof_uncompressed = -1;
		}
		else
		{
			AOCSVPInfoEntry *entry;

			entry = getAOCSVPEntry(aocsSegfile, context->columnNum);
			eof = entry->eof;
			eof_uncompressed = entry->eof_uncompressed;
		}

		values[0] = Int32GetDatum(GpIdentity.segindex);
		values[1] = Int32GetDatum(aocsSegfile->segno);
		values[2] = Int16GetDatum(context->columnNum);
		values[3] = Int32GetDatum(context->columnNum * AOTupleId_MultiplierSegmentFileNum + aocsSegfile->segno);
		values[4] = Int64GetDatum(aocsSegfile->total_tupcount);
		values[5] = Int64GetDatum(eof);
		values[6] = Int64GetDatum(eof_uncompressed);
		values[7] = Int64GetDatum(aocsSegfile->modcount);
		values[8] = Int16GetDatum(aocsSegfile->formatversion);
		values[9] = Int16GetDatum(aocsSegfile->state);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		/* Indicate we emitted one column. */
		context->columnNum++;

		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}

float8
aocol_compression_ratio_internal(Relation parentrel)
{
	StringInfoData sqlstmt;
	Relation	aosegrel;
	bool		connected = false;
	int			proc;	/* 32 bit, only holds number of segments */
	int			ret;
	int64		eof = 0;
	int64		eof_uncompressed = 0;
	float8		compress_ratio = -1;	/* the default, meaning "not
										 * available" */

	Oid			segrelid = InvalidOid;

	GetAppendOnlyEntryAuxOids(parentrel,
							  &segrelid, NULL, NULL);
	Assert(OidIsValid(segrelid));

	/*
	 * open the aoseg relation
	 */
	aosegrel = heap_open(segrelid, AccessShareLock);

	/*
	 * assemble our query string.
	 *
	 * NOTE: The aocsseg (per table) system catalog lives in the gp_aoseg
	 * namespace, too.
	 */
	initStringInfo(&sqlstmt);
	if (Gp_role == GP_ROLE_DISPATCH)
		appendStringInfo(&sqlstmt, "select vpinfo "
						 "from gp_dist_random('%s.%s')",
						 get_namespace_name(RelationGetNamespace(aosegrel)),
						 RelationGetRelationName(aosegrel));
	else
		appendStringInfo(&sqlstmt, "select vpinfo "
						 "from %s.%s",
						 get_namespace_name(RelationGetNamespace(aosegrel)),
						 RelationGetRelationName(aosegrel));

	heap_close(aosegrel, AccessShareLock);

	PG_TRY();
	{

		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("unable to obtain AO relation information from segment databases"),
					 errdetail("SPI_connect failed in get_ao_compression_ratio")));
		}
		connected = true;

		/* Do the query. */
		ret = SPI_execute(sqlstmt.data, false, 0);
		proc = (int) SPI_processed;

		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc	tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			int			i;
			HeapTuple	tuple;
			bool		isnull;
			Datum		vpinfoDatum;
			AOCSVPInfo *vpinfo;
			int			j;

			for (i = 0; i < proc; i++)
			{
				/*
				 * Each row is a binary struct vpinfo with a variable number
				 * of entries on the end.
				 */
				tuple = tuptable->vals[i];

				vpinfoDatum = heap_getattr(tuple, 1, tupdesc, &isnull);
				if (isnull)
					break;

				vpinfo = (AOCSVPInfo *) DatumGetByteaP(vpinfoDatum);

				/* CONSIDER: Better verification of vpinfo. */
				Assert(vpinfo->version == 0);
				for (j = 0; j < vpinfo->nEntry; j++)
				{
					eof += vpinfo->entry[j].eof;
					eof_uncompressed += vpinfo->entry[j].eof_uncompressed;
				}
			}

			/* guard against division by zero */
			if (eof > 0)
			{
				/* calculate the compression ratio */
				compress_ratio = (float8) eof_uncompressed / (float8) eof;

				/* format to 2 digits past the decimal point */
				compress_ratio = round(compress_ratio * 100.0) / 100.0;
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


	pfree(sqlstmt.data);

	return compress_ratio;
}

/**
 * Free up seginfo array.
 */
void
FreeAllAOCSSegFileInfo(AOCSFileSegInfo **allAOCSSegInfo, int totalSegFiles)
{
	Assert(allAOCSSegInfo);

	for (int file_no = 0; file_no < totalSegFiles; file_no++)
	{
		Assert(allAOCSSegInfo[file_no] != NULL);

		pfree(allAOCSSegInfo[file_no]);
	}
}
