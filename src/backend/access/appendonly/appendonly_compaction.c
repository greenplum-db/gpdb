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
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonly_compaction.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/aomd.h"
#include "access/aosegfiles.h"
#include "access/appendonly_compaction.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_appendonly_fn.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbpersistentstore.h"
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
#include "miscadmin.h"

static void AppendOnlySegmentFileTruncateToEOF(Relation aorel, int segno, int64 segeof);

/*
 * Drops a segment file.
 *
 */
static void
AppendOnlyCompaction_DropSegmentFile(Relation aorel, int segno)
{
	ItemPointerData persistentTid;
	int64		persistentSerialNum;

	if (!ReadGpRelationNode(
							aorel->rd_rel->reltablespace,
							aorel->rd_rel->relfilenode,
							segno,
							&persistentTid,
							&persistentSerialNum))
	{
		/* There is nothing to drop */
		return;
	}

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Drop segment file: segno %d", segno);

	MirroredFileSysObj_ScheduleDropAppendOnlyFile(
			&aorel->rd_node,
			segno,
			RelationGetRelationName(aorel),
			&persistentTid,
			persistentSerialNum);

	DeleteGpRelationNodeTuple(aorel, segno);
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
AppendOnlyCompaction_ShouldCompact(
								   Relation aoRelation,
								   int segno,
								   int64 segmentTotalTupcount,
								   bool isFull)
{
	bool		result;
	AppendOnlyVisimap visiMap;
	int64		hiddenTupcount;
	double		hideRatio;

	Assert(RelationIsAoRows(aoRelation) || RelationIsAoCols(aoRelation));

	if (!gp_appendonly_compaction)
	{
		ereport(LOG,
				(errmsg("Append-only compaction skipped on relation %s, segment file num %d",
						RelationGetRelationName(aoRelation),
						segno),
				 errdetail("Compaction is disabled")));
		/* Disable compaction by global guc. */
		return false;
	}

	AppendOnlyVisimap_Init(&visiMap,
						   aoRelation->rd_appendonly->visimaprelid,
						   aoRelation->rd_appendonly->visimapidxid,
						   ShareLock,
						   SnapshotNow);
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
	MirroredAppendOnlyOpen mirroredOpened;
	int32		fileSegNo;
	char		filenamepath[MAXPGPATH];

	Assert(RelationIsAoRows(aorel));

	relname = RelationGetRelationName(aorel);

	/* Open and truncate the relation segfile beyond its eof */
	MakeAOSegmentFileName(aorel, segno, -1, &fileSegNo, filenamepath);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Opening AO relation \"%s.%s\", relation id %u, relfilenode %u (physical segment file #%d, logical EOF " INT64_FORMAT ")",
		   get_namespace_name(RelationGetNamespace(aorel)),
		   relname,
		   aorel->rd_id,
		   aorel->rd_node.relNode,
		   segno,
		   segeof);

	if (OpenAOSegmentFile(aorel, filenamepath, fileSegNo, segeof, &mirroredOpened))
	{
		TruncateAOSegmentFile(&mirroredOpened, aorel, segeof, ERROR);
		CloseAOSegmentFile(&mirroredOpened);

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
AppendOnlyMoveTuple(MemTuple tuple,
					TupleTableSlot *slot,
					MemTupleBinding *mt_bind,
					AppendOnlyInsertDesc insertDesc,
					ResultRelInfo *resultRelInfo,
					EState *estate)
{
	AOTupleId  *oldAoTupleId;
	Oid			tupleOid;
	AOTupleId	newAoTupleId;

	Assert(resultRelInfo);
	Assert(slot);
	Assert(mt_bind);
	Assert(estate);

	oldAoTupleId = (AOTupleId *) slot_get_ctid(slot);
	/* Extract all the values of the tuple */
	slot_getallattrs(slot);

	tupleOid = MemTupleGetOid(tuple, mt_bind);
	appendonly_insert(insertDesc,
					  tuple,
					  &tupleOid,
					  &newAoTupleId);

	/* insert index' tuples if needed */
	if (resultRelInfo->ri_NumIndices > 0)
	{
		ExecInsertIndexTuples(slot, (ItemPointer) &newAoTupleId, estate, true);
		ResetPerTupleExprContext(estate);
	}

	elogif(Debug_appendonly_print_compaction, DEBUG5,
		   "Compaction: Moved tuple (%d," INT64_FORMAT ") -> (%d," INT64_FORMAT ")",
		   AOTupleIdGet_segmentFileNum(oldAoTupleId), AOTupleIdGet_rowNum(oldAoTupleId),
		   AOTupleIdGet_segmentFileNum(&newAoTupleId), AOTupleIdGet_rowNum(&newAoTupleId));
}

void
AppendOnlyThrowAwayTuple(
						 Relation rel,
						 MemTuple tuple,
						 TupleTableSlot *slot,
						 MemTupleBinding *mt_bind)
{
	AOTupleId  *oldAoTupleId;

	Assert(slot);
	Assert(mt_bind);

	oldAoTupleId = (AOTupleId *) slot_get_ctid(slot);
	/* Extract all the values of the tuple */
	slot_getallattrs(slot);

	if (MemTupleHasExternal(tuple, mt_bind))
	{
		toast_delete(rel, (HeapTuple) tuple, mt_bind);
	}

	elogif(Debug_appendonly_print_compaction, DEBUG5,
		   "Compaction: Throw away tuple (%d," INT64_FORMAT ")",
		   AOTupleIdGet_segmentFileNum(oldAoTupleId), AOTupleIdGet_rowNum(oldAoTupleId));
}

/*
 * Assumes that the segment file lock is already held.
 * Assumes that the segment file should be compacted.
 *
 */
static void
AppendOnlySegmentFileFullCompaction(Relation aorel,
									AppendOnlyInsertDesc insertDesc,
									FileSegInfo *fsinfo)
{
	const char *relname;
	AppendOnlyVisimap visiMap;
	AppendOnlyScanDesc scanDesc;
	TupleDesc	tupDesc;
	MemTuple	tuple;
	TupleTableSlot *slot;
	MemTupleBinding *mt_bind;
	int			compact_segno;
	int64		movedTupleCount = 0;
	ResultRelInfo *resultRelInfo;
	EState	   *estate;
	AOTupleId  *aoTupleId;
	int64		tupleCount = 0;
	int64		tuplePerPage = INT_MAX;

	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(RelationIsAoRows(aorel));
	Assert(insertDesc);

	compact_segno = fsinfo->segno;
	if (fsinfo->varblockcount > 0)
	{
		tuplePerPage = fsinfo->total_tupcount / fsinfo->varblockcount;
	}
	relname = RelationGetRelationName(aorel);

	AppendOnlyVisimap_Init(&visiMap,
						   aorel->rd_appendonly->visimaprelid,
						   aorel->rd_appendonly->visimapidxid,
						   ShareUpdateExclusiveLock,
						   SnapshotNow);

	elogif(Debug_appendonly_print_compaction,
		   LOG, "Compact AO segno %d, relation %s, insert segno %d",
		   compact_segno, relname, insertDesc->storageWrite.segmentFileNum);

	/*
	 * Todo: We need to limit the scan to one file and we need to avoid to
	 * lock the file again.
	 *
	 * We use SnapshotAny to get visible and invisible tuples.
	 */
	scanDesc = appendonly_beginrangescan(aorel,
										 SnapshotAny, SnapshotNow,
										 &compact_segno, 1, 0, NULL);

	tupDesc = RelationGetDescr(aorel);
	slot = MakeSingleTupleTableSlot(tupDesc);
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
	ExecOpenIndices(resultRelInfo);
	estate->es_result_relations = resultRelInfo;
	estate->es_num_result_relations = 1;
	estate->es_result_relation_info = resultRelInfo;

	/*
	 * Go through all visible tuples and move them to a new segfile.
	 */
	while ((tuple = appendonly_getnext(scanDesc, ForwardScanDirection, slot)) != NULL)
	{
		/* Check interrupts as this may take time. */
		CHECK_FOR_INTERRUPTS();

		aoTupleId = (AOTupleId *) slot_get_ctid(slot);
		if (AppendOnlyVisimap_IsVisible(&scanDesc->visibilityMap, aoTupleId))
		{
			AppendOnlyMoveTuple(tuple,
								slot,
								mt_bind,
								insertDesc,
								resultRelInfo,
								estate);
			movedTupleCount++;
		}
		else
		{
			/* Tuple is invisible and needs to be dropped */
			AppendOnlyThrowAwayTuple(aorel,
									 tuple,
									 slot,
									 mt_bind);
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

	SetFileSegInfoState(aorel, compact_segno, AOSEG_STATE_AWAITING_DROP);

	AppendOnlyVisimap_DeleteSegmentFile(&visiMap, compact_segno);

	/* Delete all mini pages of the segment files if block directory exists */
	if (OidIsValid(aorel->rd_appendonly->blkdirrelid))
	{
		AppendOnlyBlockDirectory_DeleteSegmentFile(aorel,
												   SnapshotNow,
												   compact_segno,
												   0);
	}

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Finished compaction: "
		   "AO segfile %d, relation %s, moved tuple count " INT64_FORMAT,
		   compact_segno, relname, movedTupleCount);

	AppendOnlyVisimap_Finish(&visiMap, NoLock);

	ExecCloseIndices(resultRelInfo);
	FreeExecutorState(estate);

	ExecDropSingleTupleTableSlot(slot);
	destroy_memtuple_binding(mt_bind);

	appendonly_endscan(scanDesc);
}

/*
 * Checks if the correct relation lock is held.
 * It does so be acquiring the lock in a no-wait mode.
 * If it didn't hold the lock before, it is released immediately.
 */
bool
HasLockForSegmentFileDrop(Relation aorel)
{
	LockAcquireResult acquireResult = LockRelationNoWait(aorel, AccessExclusiveLock);

	switch (acquireResult)
	{
		case LOCKACQUIRE_ALREADY_HELD:
			return true;
		case LOCKACQUIRE_NOT_AVAIL:
			return false;
		case LOCKACQUIRE_OK:
			UnlockRelation(aorel, AccessExclusiveLock);
#ifdef USE_ASSERT_CHECKING
			acquireResult = LockRelationNoWait(aorel, ShareUpdateExclusiveLock);
			if (acquireResult != LOCKACQUIRE_ALREADY_HELD)
			{
				elog(ERROR, "Don't hold access exclusive lock during drop");
				return false;
			}
#endif
			return false;
		default:
			Insist(false);
			return false;
	}
}

/*
 * Performs a compaction of an append-only relation.
 *
 * In non-utility mode, the compaction segment file should be marked as
 * in-use/in-compaction in the appendonlywriter.c code.
 */
void
AppendOnlyDrop(Relation aorel, int compacted_segno)
{
	const char *relname;
	int			total_segfiles;
	FileSegInfo **segfile_array;
	int			i,
				segno;

	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(RelationIsAoRows(aorel));

	relname = RelationGetRelationName(aorel);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Drop AO relation %s", relname);

	/* Get information about all the file segments we need to scan */
	segfile_array = GetAllFileSegInfo(aorel, SnapshotNow, &total_segfiles);

	for (i = 0; i < total_segfiles; i++)
	{
		segno = segfile_array[i]->segno;

		if (compacted_segno != segno)
			continue;

		/*
		 * Try to get the transaction write-lock for the Append-Only segment
		 * file.
		 *
		 * NOTE: This is a transaction scope lock that must be held until
		 * commit / abort.
		 */
		LockRelationAppendOnlySegmentFile(
										  &aorel->rd_node,
										  segfile_array[i]->segno,
										  AccessExclusiveLock,
										  false);

		/* Re-fetch under the write lock to get latest committed eof. */
		if (FileSegCanBeDropped(aorel, segno))
		{
			Assert(HasLockForSegmentFileDrop(aorel));
			AppendOnlyCompaction_DropSegmentFile(aorel, segno);
			ClearFileSegInfo(aorel, segno,
							 AOSEG_STATE_DEFAULT);
		}
	}

	if (segfile_array)
	{
		FreeAllSegFileInfo(segfile_array, total_segfiles);
		pfree(segfile_array);
	}
}

/*
 * Truncates each segment file to the AO relation to its EOF.
 * If we cannot get a lock on the segment file (because e.g. a concurrent insert)
 * the segment file is skipped.
 */
void
AppendOnlyTruncateToEOF(Relation aorel)
{
	const char *relname;
	int			total_segfiles;
	FileSegInfo **segfile_array;
	int			i,
				segno;
	LockAcquireResult acquireResult;
	FileSegInfo *fsinfo;

	Assert(RelationIsAoRows(aorel));

	relname = RelationGetRelationName(aorel);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Compact AO relation %s", relname);

	/* Get information about all the file segments we need to scan */
	segfile_array = GetAllFileSegInfo(aorel, SnapshotNow, &total_segfiles);

	for (i = 0; i < total_segfiles; i++)
	{
		segno = segfile_array[i]->segno;

		/*
		 * Try to get the transaction write-lock for the Append-Only segment
		 * file.
		 *
		 * NOTE: This is a transaction scope lock that must be held until
		 * commit / abort.
		 */
		acquireResult = LockRelationAppendOnlySegmentFile(
														  &aorel->rd_node,
														  segfile_array[i]->segno,
														  AccessExclusiveLock,
														   /* dontWait */ true);
		if (acquireResult == LOCKACQUIRE_NOT_AVAIL)
		{
			elog(DEBUG5, "truncate skips AO segfile %d, "
				 "relation %s", segfile_array[i]->segno, relname);
			continue;
		}

		/* Re-fetch under the write lock to get latest committed eof. */
		fsinfo = GetFileSegInfo(aorel, SnapshotNow, segno);

		/*
		 * This should not occur since this segfile info was found by the
		 * "all" method, but better to catch for trouble shooting (possibly
		 * index corruption?)
		 */
		if (fsinfo == NULL)
			elog(ERROR, "file seginfo for AO relation %s %u/%u/%u (segno=%u) is missing",
				 relname,
				 aorel->rd_node.spcNode,
				 aorel->rd_node.dbNode,
				 aorel->rd_node.relNode,
				 segno);

		AppendOnlySegmentFileTruncateToEOF(aorel, fsinfo->segno, fsinfo->eof);
		pfree(fsinfo);
	}

	if (segfile_array)
	{
		FreeAllSegFileInfo(segfile_array, total_segfiles);
		pfree(segfile_array);
	}
}

/*
 * Performs a compaction of an append-only relation.
 *
 * The compaction segment file should be marked as in-use/in-compaction in
 * the appendonlywriter.c code. If set, the insert_segno should also be
 * marked as in-use. When the insert segno is negative, only truncate to eof
 * operations can be executed.
 *
 * The caller is required to hold either an AccessExclusiveLock (vacuum full)
 * or a ShareLock on the relation.
 */
void
AppendOnlyCompact(Relation aorel,
				  int compaction_segno,
				  int insert_segno,
				  bool isFull)
{
	const char *relname;
	int			total_segfiles;
	FileSegInfo **segfile_array;
	AppendOnlyInsertDesc insertDesc = NULL;
	int			i,
				segno;
	FileSegInfo *fsinfo;

	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(insert_segno >= 0);

	/* We cannot compact the segment file we are inserting to. */
	Assert (insert_segno != compaction_segno);

	relname = RelationGetRelationName(aorel);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Compact AO relation %s", relname);

	/* Get information about all the file segments we need to scan */
	segfile_array = GetAllFileSegInfo(aorel, SnapshotNow, &total_segfiles);

	insertDesc = appendonly_insert_init(aorel, insert_segno, false);

	for (i = 0; i < total_segfiles; i++)
	{
		segno = segfile_array[i]->segno;

		if (compaction_segno != segno)
			continue;

		/*
		 * Try to get the transaction write-lock for the Append-Only segment
		 * file.
		 *
		 * NOTE: This is a transaction scope lock that must be held until
		 * commit / abort.
		 */
		LockRelationAppendOnlySegmentFile(
										  &aorel->rd_node,
										  segfile_array[i]->segno,
										  AccessExclusiveLock,
										  false);

		/* Re-fetch under the write lock to get latest committed eof. */
		fsinfo = GetFileSegInfo(aorel, SnapshotNow, segno);

		/*
		 * This should not occur since this segfile info was found by the
		 * "all" method, but better to catch for trouble shooting (possibly
		 * index corruption?)
		 */
		if (fsinfo == NULL)
			elog(ERROR, "file seginfo for AO relation %s %u/%u/%u (segno=%u) is missing",
				 relname,
				 aorel->rd_node.spcNode,
				 aorel->rd_node.dbNode,
				 aorel->rd_node.relNode,
				 segno);

		if (AppendOnlyCompaction_ShouldCompact(aorel,
											   fsinfo->segno, fsinfo->total_tupcount, isFull))
		{
			AppendOnlySegmentFileFullCompaction(aorel,
												insertDesc,
												fsinfo);
		}
		pfree(fsinfo);
	}

	appendonly_insert_finish(insertDesc);

	if (segfile_array)
	{
		FreeAllSegFileInfo(segfile_array, total_segfiles);
		pfree(segfile_array);
	}
}

/*
 * Returns true if the relation has no tuples.  Prepare phase of
 * compaction invokes this function on each QE.
 *
 * Examples of empty tables:
 * 1. parent of a partitioned table
 * 2. table that is created but no tuples have been inserted yet
 * 3. table from which all existing tuples are deleted and the table
 * is vacuumed.  This is a special case in which pg_aoseg_<oid> has
 * non-zero number of rows but tupcount value is zero for all rows.
 */
bool
AppendOnlyCompaction_IsRelationEmpty(Relation aorel)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	HeapTuple	tuple;
	HeapScanDesc aoscan;
	int			Anum_tupcount;
	bool		empty = true;

	Assert(RelationIsAoRows(aorel) || RelationIsAoCols(aorel));

	pg_aoseg_rel = heap_open(aorel->rd_appendonly->segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);
	aoscan = heap_beginscan(pg_aoseg_rel, SnapshotNow, 0, NULL);
	Anum_tupcount = RelationIsAoRows(aorel) ? Anum_pg_aoseg_tupcount : Anum_pg_aocs_tupcount;
	while ((tuple = heap_getnext(aoscan, ForwardScanDirection)) != NULL &&
		   empty)
	{
		if (0 < fastgetattr(tuple, Anum_tupcount,
							pg_aoseg_dsc, NULL))
			empty = false;
	}
	heap_endscan(aoscan);
	heap_close(pg_aoseg_rel, AccessShareLock);
	return empty;
}
