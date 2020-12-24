/*------------------------------------------------------------------------------
 *
 * Code dealing with the compaction of append-only tables.
 *
 * Compaction is the part of updatable append-only tables that
 * is responsible to freeing the space occupied by tuples that have
 * been deleted or updated.
 *
 * The general process is that first all visible tuples of a segment
 * file (or a set of segment files) are moved to a different segment file.
 * Afterwards in a different transaction that requires AccessExclusiveLocks
 * the compacted segment files are dropped and the eof/tupcount/varblock
 * information in pg_aoseg_<oid> are reset to 0.
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonly_compaction.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/aocs_compaction.h"
#include "access/aomd.h"
#include "access/aosegfiles.h"
#include "access/appendonly_compaction.h"
#include "access/appendonlywriter.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_appendonly_fn.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbvars.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "storage/procarray.h"
#include "storage/lmgr.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/guc.h"
#include "utils/snapmgr.h"
#include "miscadmin.h"


/*
 * Drops a segment file.
 *
 * Actually, we just truncate the segfile to 0 bytes, to reclaim the space.
 * Before GPDB 6, we used to remove the file, but with WAL replication, we
 * no longer have a convenient function to remove a single segment of a
 * relation. An empty file is as almost as good as a non-existent file. If
 * the relation is dropped later, the code in mdunlink() will remove all
 * segments, including any empty ones we've left behind.
 */
static void
AppendOnlyCompaction_DropSegmentFile(Relation aorel, int segno)
{
	char		filenamepath[MAXPGPATH];
	int32		fileSegNo;
	File		fd;

	Assert(RelationIsAoRows(aorel));

	if (Debug_appendonly_print_compaction)
		elog(LOG, "Drop segment file: segno %d", segno);

	/* Open and truncate the relation segfile */
	MakeAOSegmentFileName(aorel, segno, -1, &fileSegNo, filenamepath);

	fd = OpenAOSegmentFile(aorel, filenamepath, 0);
	if (fd >= 0)
	{
		TruncateAOSegmentFile(fd, aorel, fileSegNo, 0);
		CloseAOSegmentFile(fd);
	}
	else
	{
		/*
		 * The file we were about to drop/truncate didn't exist. That shouldn't
		 * happen, but the end result is what we wanted. Assert so that we will
		 * find out if this happens, after all, in testing. In production, we'd
		 * rather keep running.
		 */
		elog(LOG, "could not truncate segfile %s, because it does not exist", filenamepath);
		Assert(false);
	}
}

/*
 * Calculates the ratio of hidden tuples as a double between 0 and 100
 */
static double
AppendOnlyCompaction_GetHideRatio(int64 hiddenTupcount, int64 totalTupcount)
{
	double		hideRatio;

	if (hiddenTupcount == 0 || totalTupcount == 0)
	{
		return 0;
	}
	hideRatio = ((double) hiddenTupcount) / ((double) totalTupcount) * 100.0;
	return hideRatio;
}

/*
 * Returns true iff the given segment file should be compacted.
 */
bool
AppendOnlyCompaction_ShouldCompact(Relation aoRelation,
								   int segno,
								   int64 segmentTotalTupcount,
								   bool isFull,
								   Snapshot	appendOnlyMetaDataSnapshot)
{
	bool		result;
	AppendOnlyVisimap visiMap;
	int64		hiddenTupcount;
	double		hideRatio;
    Oid         visimaprelid;
    Oid         visimapidxid;

	Assert(RelationIsAppendOptimized(aoRelation));
    GetAppendOnlyEntryAuxOids(aoRelation->rd_id, appendOnlyMetaDataSnapshot,
                              NULL, NULL, NULL,
                              &visimaprelid, &visimapidxid);

	if (!gp_appendonly_compaction)
	{
		ereport(LOG,
				(errmsg("append-only compaction skipped on relation %s, segment file num %d",
						RelationGetRelationName(aoRelation),
						segno),
				 errdetail("Compaction is disabled.")));
		/* Disable compaction by global guc. */
		return false;
	}

	AppendOnlyVisimap_Init(&visiMap,
						   visimaprelid,
						   visimapidxid,
						   ShareLock,
						   appendOnlyMetaDataSnapshot);
	hiddenTupcount = AppendOnlyVisimap_GetSegmentFileHiddenTupleCount(
																	  &visiMap, segno);

	result = true;
	if (isFull && hiddenTupcount > 0)
	{
		/*
		 * if it is a full vacuum and there is any obsolete data, do a
		 * compaction
		 */
		result = true;
	}
	else
	{
		hideRatio = AppendOnlyCompaction_GetHideRatio(hiddenTupcount, segmentTotalTupcount);
		if (hideRatio <= gp_appendonly_compaction_threshold || gp_appendonly_compaction_threshold == 0)
		{
			if (hiddenTupcount > 0)
			{
				ereportif(Debug_appendonly_print_compaction, LOG,
						  (errmsg("Append-only compaction skipped on relation %s, segment file num %d, "
								  "hidden tupcount " INT64_FORMAT ", total tupcount " INT64_FORMAT ", "
								  "hide ratio %lf%%, threshold %d%%",
								  RelationGetRelationName(aoRelation),
								  segno,
								  hiddenTupcount, segmentTotalTupcount,
								  hideRatio, gp_appendonly_compaction_threshold)));
				ereport(LOG,
						(errmsg("Append-only compaction skipped on relation %s, segment file num %d",
								RelationGetRelationName(aoRelation),
								segno),
						 errdetail("Ratio of obsolete tuples below threshold (%lf%% vs %d%%)",
								   hideRatio, gp_appendonly_compaction_threshold)));
			}
			else
			{
				ereportif(Debug_appendonly_print_compaction, LOG,
						  (errmsg("Append-only compaction skipped on relation %s, segment file num %d, "
								  "hidden tupcount " INT64_FORMAT ", total tupcount " INT64_FORMAT ", "
								  "hide ratio %lf%%, threshold %d%%",
								  RelationGetRelationName(aoRelation),
								  segno,
								  hiddenTupcount, segmentTotalTupcount,
								  hideRatio, gp_appendonly_compaction_threshold)));
			}
			result = false;
		}
		elogif(Debug_appendonly_print_compaction, LOG,
			   "Schedule compaction: "
			   "segno %d, "
			   "hidden tupcount " INT64_FORMAT ", total tupcount " INT64_FORMAT ", "
			   "hide ratio %lf%%, threshold %d%%",
			   segno,
			   hiddenTupcount, segmentTotalTupcount,
			   hideRatio, gp_appendonly_compaction_threshold);
	}
	AppendOnlyVisimap_Finish(&visiMap, ShareLock);

	return result;
}

/*
 * AppendOnlySegmentFileTruncateToEOF()
 *
 * Assumes that the segment file lock is already held.
 *
 * For the segment file is truncates to the eof.
 */
static void
AppendOnlySegmentFileTruncateToEOF(Relation aorel, int segno, int64 segeof)
{
	const char *relname = RelationGetRelationName(aorel);
	File		fd;
	int32		fileSegNo;
	char		filenamepath[MAXPGPATH];

	Assert(RelationIsAoRows(aorel));

	relname = RelationGetRelationName(aorel);

	/* Open and truncate the relation segfile to its eof */
	MakeAOSegmentFileName(aorel, segno, -1, &fileSegNo, filenamepath);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Opening AO relation \"%s.%s\", relation id %u, relfilenode %u (physical segment file #%d, logical EOF " INT64_FORMAT ")",
		   get_namespace_name(RelationGetNamespace(aorel)),
		   relname,
		   aorel->rd_id,
		   aorel->rd_node.relNode,
		   segno,
		   segeof);

	fd = OpenAOSegmentFile(aorel, filenamepath, segeof);
	if (fd >= 0)
	{
		TruncateAOSegmentFile(fd, aorel, fileSegNo, segeof);
		CloseAOSegmentFile(fd);

		elogif(Debug_appendonly_print_compaction, LOG,
			   "Successfully truncated AO ROW relation \"%s.%s\", relation id %u, relfilenode %u (physical segment file #%d, logical EOF " INT64_FORMAT ")",
			   get_namespace_name(RelationGetNamespace(aorel)),
			   relname,
			   aorel->rd_id,
			   aorel->rd_node.relNode,
			   segno,
			   segeof);
	}
	else
	{
		elogif(Debug_appendonly_print_compaction, LOG,
			   "No gp_relation_node entry for AO ROW relation \"%s.%s\", relation id %u, relfilenode %u (physical segment file #%d, logical EOF " INT64_FORMAT ")",
			   get_namespace_name(RelationGetNamespace(aorel)),
			   relname,
			   aorel->rd_id,
			   aorel->rd_node.relNode,
			   segno,
			   segeof);
	}
}

static void
AppendOnlyMoveTuple(TupleTableSlot *slot,
					MemTupleBinding *mt_bind,
					AppendOnlyInsertDesc insertDesc,
					ResultRelInfo *resultRelInfo,
					EState *estate)
{
	MemTuple	tuple;
	bool		shouldFree;
	AOTupleId  *oldAoTupleId;
	AOTupleId	newAoTupleId;

	Assert(resultRelInfo);
	Assert(slot);
	Assert(mt_bind);
	Assert(estate);

	oldAoTupleId = (AOTupleId *) &slot->tts_tid;
	/* Extract all the values of the tuple */
	slot_getallattrs(slot);

	tuple = ExecFetchSlotMemTuple(slot, &shouldFree, mt_bind);
	appendonly_insert(insertDesc,
					  tuple,
					  &newAoTupleId);
	slot->tts_tid = *((ItemPointerData *) &newAoTupleId);

	/* insert index' tuples if needed */
	if (resultRelInfo->ri_NumIndices > 0)
	{
		ExecInsertIndexTuples(slot,
							  estate,
							  false, /* noDupError */
							  NULL, /* specConflict */
							  NIL /* arbiterIndexes */);
		ResetPerTupleExprContext(estate);
	}

	if (shouldFree)
		pfree(tuple);

	if (Debug_appendonly_print_compaction)
		ereport(DEBUG5,
				(errmsg("Compaction: Moved tuple (%d," INT64_FORMAT ") -> (%d," INT64_FORMAT ")",
						AOTupleIdGet_segmentFileNum(oldAoTupleId), AOTupleIdGet_rowNum(oldAoTupleId),
						AOTupleIdGet_segmentFileNum(&newAoTupleId), AOTupleIdGet_rowNum(&newAoTupleId))));
}

void
AppendOnlyThrowAwayTuple(Relation rel, TupleTableSlot *slot)
{
	AOTupleId  *oldAoTupleId;

	Assert(slot);

	oldAoTupleId = (AOTupleId *) &slot->tts_tid;
	/* Extract all the values of the tuple */
	slot_getallattrs(slot);

	/* GPDB_12_MERGE_FIXME: loop through all attributes, call toast_delete_datum()
	 * on any toasted datums, like toast_delete does for heap tuples */
#if 0
	tuple = TupGetMemTuple(slot);
	if (MemTupleHasExternal(tuple, mt_bind))
	{
		toast_delete_memtup(rel, tuple, mt_bind);
	}
#endif
	
	if (Debug_appendonly_print_compaction)
		ereport(DEBUG5,
				(errmsg("Compaction: Throw away tuple (%d," INT64_FORMAT ")",
						AOTupleIdGet_segmentFileNum(oldAoTupleId),
						AOTupleIdGet_rowNum(oldAoTupleId))));
}

/*
 * Assumes that the segment file lock is already held.
 * Assumes that the segment file should be compacted.
 *
 */
static void
AppendOnlySegmentFileFullCompaction(Relation aorel,
									AppendOnlyInsertDesc insertDesc,
									FileSegInfo *fsinfo,
									Snapshot	appendOnlyMetaDataSnapshot)
{
	const char *relname;
	AppendOnlyVisimap visiMap;
	AppendOnlyScanDesc scanDesc;
	TupleDesc	tupDesc;
	TupleTableSlot *slot;
	MemTupleBinding *mt_bind;
	int			compact_segno;
	int64		movedTupleCount = 0;
	ResultRelInfo *resultRelInfo;
	EState	   *estate;
	AOTupleId  *aoTupleId;
	int64		tupleCount = 0;
	int64		tuplePerPage = INT_MAX;
    Oid         visimaprelid;
    Oid         visimapidxid;
    Oid         blkdirrelid;

	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(RelationIsAoRows(aorel));
	Assert(insertDesc);

	compact_segno = fsinfo->segno;
	if (fsinfo->varblockcount > 0)
	{
		tuplePerPage = fsinfo->total_tupcount / fsinfo->varblockcount;
	}
	relname = RelationGetRelationName(aorel);

	GetAppendOnlyEntryAuxOids(aorel->rd_id, appendOnlyMetaDataSnapshot,
							  NULL, &blkdirrelid, NULL,
							  &visimaprelid, &visimapidxid);

	AppendOnlyVisimap_Init(&visiMap,
						   visimaprelid,
						   visimapidxid,
						   ShareUpdateExclusiveLock,
						   appendOnlyMetaDataSnapshot);

	if (Debug_appendonly_print_compaction)
		elog(LOG, "Compact AO segno %d, relation %s, insert segno %d",
			 compact_segno, relname, insertDesc->storageWrite.segmentFileNum);

	/*
	 * Todo: We need to limit the scan to one file and we need to avoid to
	 * lock the file again.
	 *
	 * We use SnapshotAny to get visible and invisible tuples.
	 */
	scanDesc = appendonly_beginrangescan(aorel,
										 SnapshotAny, appendOnlyMetaDataSnapshot,
										 &compact_segno, 1, 0, NULL);

	tupDesc = RelationGetDescr(aorel);
	slot = MakeSingleTupleTableSlot(tupDesc, &TTSOpsVirtual);
	slot->tts_tableOid = RelationGetRelid(aorel);
	mt_bind = create_memtuple_binding(tupDesc);

	/*
	 * We need a ResultRelInfo and an EState so we can use the regular
	 * executor's index-entry-making machinery.
	 */
	estate = CreateExecutorState();
	resultRelInfo = makeNode(ResultRelInfo);
	resultRelInfo->ri_RangeTableIndex = 1;	/* dummy */
	resultRelInfo->ri_RelationDesc = aorel;
	resultRelInfo->ri_TrigDesc = NULL;	/* we don't fire triggers */
	ExecOpenIndices(resultRelInfo, false);
	estate->es_result_relations = resultRelInfo;
	estate->es_num_result_relations = 1;
	estate->es_result_relation_info = resultRelInfo;

	/*
	 * Go through all visible tuples and move them to a new segfile.
	 */
	while (appendonly_getnextslot(&scanDesc->rs_base, ForwardScanDirection, slot))
	{
		/* Check interrupts as this may take time. */
		CHECK_FOR_INTERRUPTS();

		aoTupleId = (AOTupleId *) &slot->tts_tid;
		if (AppendOnlyVisimap_IsVisible(&scanDesc->visibilityMap, aoTupleId))
		{
			AppendOnlyMoveTuple(slot,
								mt_bind,
								insertDesc,
								resultRelInfo,
								estate);
			movedTupleCount++;
		}
		else
		{
			/* Tuple is invisible and needs to be dropped */
			AppendOnlyThrowAwayTuple(aorel, slot);
		}

		/*
		 * Check for vacuum delay point after approximately a var block
		 */
		tupleCount++;
		if (VacuumCostActive && tupleCount % tuplePerPage == 0)
		{
			vacuum_delay_point();
		}
	}

	MarkFileSegInfoAwaitingDrop(aorel, compact_segno);

	AppendOnlyVisimap_DeleteSegmentFile(&visiMap, compact_segno);

	/* Delete all mini pages of the segment files if block directory exists */
	if (OidIsValid(blkdirrelid))
	{
		AppendOnlyBlockDirectory_DeleteSegmentFile(aorel,
												   appendOnlyMetaDataSnapshot,
												   compact_segno,
												   0);
	}

	if (Debug_appendonly_print_compaction)
		elog(LOG, "Finished compaction: AO segfile %d, relation %s, moved tuple count " INT64_FORMAT,
			 compact_segno, relname, movedTupleCount);

	AppendOnlyVisimap_Finish(&visiMap, NoLock);

	ExecCloseIndices(resultRelInfo);
	FreeExecutorState(estate);

	ExecDropSingleTupleTableSlot(slot);
	destroy_memtuple_binding(mt_bind);

	appendonly_endscan(&scanDesc->rs_base);
}

/*
 * Recycle AWAITING_DROP segments.
 *
 * This tries to acquire an AccessExclusiveLock on the table, if it's
 * available. If it's not, no segments are dropped.
 */
void
AppendOnlyRecycleDeadSegments(Relation aorel)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	SysScanDesc aoscan;
	HeapTuple	tuple;
	Snapshot	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
	bool		got_accessexclusive_lock = false;
	TransactionId cutoff_xid = InvalidTransactionId;
	Oid			segrelid;

	Assert(RelationIsAppendOptimized(aorel));

	/*
	 * The algorithm below for choosing a target segment is not concurrent-safe.
	 * Grab a lock to serialize.
	 *
	 * INterlocks with SetSegnoInternal()
	 */
	LockRelationForExtension(aorel, ExclusiveLock);

	GetAppendOnlyEntryAuxOids(aorel->rd_id, appendOnlyMetaDataSnapshot,
							  &segrelid, NULL, NULL, NULL, NULL);
	/*
	 * Now pick a segment that is not in use, and is not over the allowed
	 * size threshold (90% full).
	 */
	pg_aoseg_rel = heap_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, appendOnlyMetaDataSnapshot, 0, NULL);
	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		bool		visible_to_all;
		int			segno;
		int16		state;
		bool		isNull;
		TransactionId xmin;

		if (RelationIsAoRows(aorel))
		{
			segno = DatumGetInt32(fastgetattr(tuple,
											  Anum_pg_aoseg_segno,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);

			state = fastgetattr(tuple, Anum_pg_aoseg_state, pg_aoseg_dsc, &isNull);
			Assert(!isNull);
		}
		else
		{
			segno = DatumGetInt32(fastgetattr(tuple,
											  Anum_pg_aocs_segno,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);

			state = DatumGetInt16(fastgetattr(tuple,
											  Anum_pg_aocs_state,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);
		}

		if (state != AOSEG_STATE_AWAITING_DROP)
			continue;

		/*
		 * Upgrade our lock to AccessExclusiveLock for the drop. Upgrading a
		 * lock poses a deadlock risk, so give up if we cannot acquire the
		 * lock immediately. We'll retry dropping the segment on the next
		 * VACUUM.
		 */
		if (!got_accessexclusive_lock)
		{
			if (!ConditionalLockRelation(aorel, AccessExclusiveLock))
			{
				if (Debug_appendonly_print_compaction)
					elog(LOG, "could not acquire AccessExclusiveLock lock on %s to recycle segno %d",
						 RelationGetRelationName(aorel), segno);
				break;
			}
			got_accessexclusive_lock = true;
		}

		/*
		 * It's in awaiting-drop state, but does everyone see it that way?
		 *
		 * Compare the tuple's xmin with the oldest-xmin horizon. We don't bother
		 * checking the xmax; we never update or lock awaiting-drop tuples, so it
		 * should not be set. Even if the tuple was update, presumably an AO
		 * segment that's in awaiting-drop state won't be resurrected, so even if
		 * someone updates or locks the tuple, it's still safe to drop.
		 */
		xmin = HeapTupleHeaderGetXmin(tuple->t_data);
		if (xmin == FrozenTransactionId)
			visible_to_all = true;
		else
		{
			if (cutoff_xid == InvalidTransactionId)
				cutoff_xid = GetOldestXmin(NULL, true);

			visible_to_all = TransactionIdPrecedes(xmin, cutoff_xid);
		}
		if (!visible_to_all)
			continue;

		/* all set! */
		if (RelationIsAoRows(aorel))
		{
			AppendOnlyCompaction_DropSegmentFile(aorel, segno);
			ClearFileSegInfo(aorel, segno);
		}
		else
		{
			AOCSCompaction_DropSegmentFile(aorel, segno);
			ClearAOCSFileSegInfo(aorel, segno);
		}
	}
	systable_endscan(aoscan);

	UnlockRelationForExtension(aorel, ExclusiveLock);

	heap_close(pg_aoseg_rel, AccessShareLock);

	UnregisterSnapshot(appendOnlyMetaDataSnapshot);
}

/*
 * Truncates each segment file in the AO relation to its EOF.
 * If we cannot get a lock on the segment file (because e.g. a concurrent insert)
 * the segment file is skipped.
 */
void
AppendOnlyTruncateToEOF(Relation aorel)
{
	const char *relname;
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	SysScanDesc aoscan;
	HeapTuple	tuple;
	Snapshot	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
	Oid			segrelid;

	Assert(RelationIsAppendOptimized(aorel));

	relname = RelationGetRelationName(aorel);

	/*
	 * The algorithm below for choosing a target segment is not concurrent-safe.
	 * Grab a lock to serialize.
	 */
	LockRelationForExtension(aorel, ExclusiveLock);

	GetAppendOnlyEntryAuxOids(aorel->rd_id, appendOnlyMetaDataSnapshot,
							  &segrelid, NULL, NULL, NULL, NULL);

	/*
	 * Now pick a segment that is not in use, and is not over the allowed
	 * size threshold (90% full).
	 */
	pg_aoseg_rel = heap_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, appendOnlyMetaDataSnapshot, 0, NULL);
	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		int			segno;
		bool		isNull;

		if (RelationIsAoRows(aorel))
		{
			segno = DatumGetInt32(fastgetattr(tuple,
											  Anum_pg_aoseg_segno,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);
		}
		else
		{
			segno = DatumGetInt32(fastgetattr(tuple,
											  Anum_pg_aocs_segno,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);
		}

		/*
		 * Is anyone writing to this segfile? Readers are OK, because they won't
		 * try to read beyond the EOF as stated in the pg_aoseg entry
		 */
		if (!pg_aoseg_tuple_could_be_updated(pg_aoseg_rel, tuple))
			continue;

		if (RelationIsAoRows(aorel))
		{
			int64		segeof;

			segeof = DatumGetInt64(fastgetattr(tuple,
											   Anum_pg_aoseg_eof,
											   pg_aoseg_dsc, &isNull));
			Assert(!isNull);
			AppendOnlySegmentFileTruncateToEOF(aorel, segno, segeof);
		}
		else
		{
			Datum		d = fastgetattr(tuple,
										Anum_pg_aocs_vpinfo,
										pg_aoseg_dsc, &isNull);
			AOCSVPInfo *vpinfo = (AOCSVPInfo *) PG_DETOAST_DATUM(d);

			AOCSSegmentFileTruncateToEOF(aorel, segno, vpinfo);

			if (DatumGetPointer(d) != (Pointer) vpinfo)
				pfree(vpinfo);
		}
	}
	systable_endscan(aoscan);
	UnlockRelationForExtension(aorel, ExclusiveLock);
	heap_close(pg_aoseg_rel, AccessShareLock);
	UnregisterSnapshot(appendOnlyMetaDataSnapshot);
}

/*
 * Performs a compaction of an append-only relation.
 *
 * The compaction segment file should be marked as in-use/in-compaction in
 * the appendonlywriter.c code.
 *
 * On exit, *insert_segno will be set to the the segment that was used as the
 * insertion target. The segfiles listed in 'avoid_segnos' will not be used
 * for insertion.
 *
 * The caller is required to hold either an AccessExclusiveLock (vacuum full)
 * or a ShareLock on the relation.
 */
void
AppendOnlyCompact(Relation aorel,
				  int compaction_segno,
				  int *insert_segno,
				  bool isFull,
				  List *avoid_segnos)
{
	const char *relname;
	AppendOnlyInsertDesc insertDesc = NULL;
	FileSegInfo *fsinfo;
	Snapshot	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));

	Assert(RelationIsAoRows(aorel));
	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);

	relname = RelationGetRelationName(aorel);

	/* Fetch under the write lock to get latest committed eof. */
	fsinfo = GetFileSegInfo(aorel, appendOnlyMetaDataSnapshot, compaction_segno, true);

	if (AppendOnlyCompaction_ShouldCompact(aorel,
										   fsinfo->segno, fsinfo->total_tupcount, isFull,
										   appendOnlyMetaDataSnapshot))
	{
		if (*insert_segno == -1)
		{
			/* get the insertion segment on first call. */
			*insert_segno = ChooseSegnoForCompactionWrite(aorel, avoid_segnos);
		}
		if (*insert_segno != -1)
		{
			insertDesc = appendonly_insert_init(aorel, *insert_segno);
			AppendOnlySegmentFileFullCompaction(aorel,
												insertDesc,
												fsinfo,
												appendOnlyMetaDataSnapshot);

			appendonly_insert_finish(insertDesc);
		}
		else
		{
			/* Could not find a target segment. Give up */
			ereport(WARNING,
					(errmsg("could not find a free segment file to use for compacting segfile %d of relation %s",
							compaction_segno, RelationGetRelationName(aorel))));
		}
	}

	pfree(fsinfo);

	UnregisterSnapshot(appendOnlyMetaDataSnapshot);
}
