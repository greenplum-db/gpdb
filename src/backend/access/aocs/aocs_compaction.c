/*------------------------------------------------------------------------------
 *
 * Code dealing with the compaction of append-only tables.
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/aocs/aocs_compaction.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/aosegfiles.h"
#include "access/aomd.h"
#include "access/aocs_compaction.h"
#include "access/appendonly_compaction.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_appendonly_fn.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbpersistentstore.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/guc.h"
#include "miscadmin.h"

/**
 * Drops a segment file.
 *
 */
static void
AOCSCompaction_DropSegmentFile(Relation aorel,
							   int segno)
{
	ItemPointerData persistentTid;
	int64		persistentSerialNum;
	int			pseudoSegNo;
	int			col;

	Assert(RelationIsAoCols(aorel));

	for (col = 0; col < RelationGetNumberOfAttributes(aorel); col++)
	{
		pseudoSegNo = (col * AOTupleId_MultiplierSegmentFileNum) + segno;

		if (!ReadGpRelationNode(
								aorel->rd_rel->reltablespace,
								aorel->rd_rel->relfilenode,
								pseudoSegNo,
								&persistentTid,
								&persistentSerialNum))
		{
			/* There is nothing to drop */
			return;
		}

		elogif(Debug_appendonly_print_compaction, LOG,
			   "Drop segment file: "
			   "segno %d",
			   pseudoSegNo);

		MirroredFileSysObj_ScheduleDropAppendOnlyFile(
													  &aorel->rd_node,
													  pseudoSegNo,
													  RelationGetRelationName(aorel),
													  &persistentTid,
													  persistentSerialNum);

		DeleteGpRelationNodeTuple(aorel,
								  pseudoSegNo);
	}
}

/*
 * AOCSSegmentFileTruncateToEOF()
 *
 * Assumes that the segment file lock is already held.
 *
 * For the segment file is truncates to the eof.
 */
static void
AOCSSegmentFileTruncateToEOF(Relation aorel,
							 AOCSFileSegInfo *fsinfo)
{
	const char *relname = RelationGetRelationName(aorel);
	int			segno;
	int			j;

	Assert(fsinfo);
	Assert(RelationIsAoCols(aorel));

	segno = fsinfo->segno;
	relname = RelationGetRelationName(aorel);

	for (j = 0; j < fsinfo->vpinfo.nEntry; ++j)
	{
		int64		segeof;
		char		filenamepath[MAXPGPATH];
		AOCSVPInfoEntry *entry;
		MirroredAppendOnlyOpen mirroredOpened;
		int32		fileSegNo;

		entry = getAOCSVPEntry(fsinfo, j);
		segeof = entry->eof;

		/* Open and truncate the relation segfile beyond its eof */
		MakeAOSegmentFileName(aorel, segno, j, &fileSegNo, filenamepath);

		elogif(Debug_appendonly_print_compaction, LOG,
			   "Opening AO COL relation \"%s.%s\", relation id %u, relfilenode %u column #%d, logical segment #%d (physical segment file #%d, logical EOF " INT64_FORMAT ")",
			   get_namespace_name(RelationGetNamespace(aorel)),
			   relname,
			   aorel->rd_id,
			   aorel->rd_node.relNode,
			   j,
			   segno,
			   fileSegNo,
			   segeof);

		if (OpenAOSegmentFile(aorel, filenamepath, fileSegNo, segeof, &mirroredOpened))
		{
			TruncateAOSegmentFile(&mirroredOpened, aorel, segeof, ERROR);
			CloseAOSegmentFile(&mirroredOpened);

			elogif(Debug_appendonly_print_compaction, LOG,
				   "Successfully truncated AO COL relation \"%s.%s\", relation id %u, relfilenode %u column #%d, logical segment #%d (physical segment file #%d, logical EOF " INT64_FORMAT ")",
				   get_namespace_name(RelationGetNamespace(aorel)),
				   relname,
				   aorel->rd_id,
				   aorel->rd_node.relNode,
				   j,
				   segno,
				   fileSegNo,
				   segeof);
		}
		else
		{
			elogif(Debug_appendonly_print_compaction, LOG,
				   "No gp_relation_node entry for AO COL relation \"%s.%s\", relation id %u, relfilenode %u column #%d, logical segment #%d (physical segment file #%d, logical EOF " INT64_FORMAT ")",
				   get_namespace_name(RelationGetNamespace(aorel)),
				   relname,
				   aorel->rd_id,
				   aorel->rd_node.relNode,
				   j,
				   segno,
				   fileSegNo,
				   segeof);
		}
	}
}

/*
 * Truncates each segment file to the AOCS relation to its EOF.
 * If we cannot get a lock on the segment file (because e.g. a concurrent insert)
 * the segment file is skipped.
 */
void
AOCSTruncateToEOF(Relation aorel)
{
	const char *relname;
	int			total_segfiles;
	AOCSFileSegInfo **segfile_array;
	int			i,
				segno;
	LockAcquireResult acquireResult;
	AOCSFileSegInfo *fsinfo;

	Assert(RelationIsAoCols(aorel));

	relname = RelationGetRelationName(aorel);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Compact AO relation %s", relname);

	/* Get information about all the file segments we need to scan */
	segfile_array = GetAllAOCSFileSegInfo(aorel, SnapshotNow, &total_segfiles);

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
		fsinfo = GetAOCSFileSegInfo(aorel, SnapshotNow, segno);

		/*
		 * This should not occur since this segfile info was found by the
		 * "all" method, but better to catch for trouble shooting (possibly
		 * index corruption?)
		 */
		if (fsinfo == NULL)
			elog(ERROR, "file seginfo for AOCS relation %s %u/%u/%u (segno=%u) is missing",
				 relname,
				 aorel->rd_node.spcNode,
				 aorel->rd_node.dbNode,
				 aorel->rd_node.relNode,
				 segno);

		AOCSSegmentFileTruncateToEOF(aorel, fsinfo);
		pfree(fsinfo);
	}

	if (segfile_array)
	{
		FreeAllAOCSSegFileInfo(segfile_array, total_segfiles);
		pfree(segfile_array);
	}
}

static void
AOCSMoveTuple(TupleTableSlot *slot,
			  AOCSInsertDesc insertDesc,
			  ResultRelInfo *resultRelInfo,
			  EState *estate)
{
	AOTupleId  *oldAoTupleId;
	AOTupleId	newAoTupleId;

	Assert(resultRelInfo);
	Assert(slot);
	Assert(estate);

	oldAoTupleId = (AOTupleId *) slot_get_ctid(slot);
	/* Extract all the values of the tuple */
	slot_getallattrs(slot);

	(void) aocs_insert_values(insertDesc,
							  slot_get_values(slot),
							  slot_get_isnull(slot),
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

/*
 * Assumes that the segment file lock is already held.
 * Assumes that the segment file should be compacted.
 */
static bool
AOCSSegmentFileFullCompaction(Relation aorel,
							  AOCSInsertDesc insertDesc,
							  AOCSFileSegInfo *fsinfo)
{
	const char *relname;
	AppendOnlyVisimap visiMap;
	AOCSScanDesc scanDesc;
	TupleDesc	tupDesc;
	TupleTableSlot *slot;
	int			compact_segno;
	int64		movedTupleCount = 0;
	ResultRelInfo *resultRelInfo;
	MemTupleBinding *mt_bind;
	EState	   *estate;
	bool	   *proj;
	int			i;
	AOTupleId  *aoTupleId;
	int64		tupleCount = 0;
	int64		tuplePerPage = INT_MAX;

	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(RelationIsAoCols(aorel));
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
						   ShareLock,
						   SnapshotNow);

	elogif(Debug_appendonly_print_compaction,
		   LOG, "Compact AO segfile %d, relation %sd",
		   compact_segno, relname);

	proj = palloc0(sizeof(bool) * RelationGetNumberOfAttributes(aorel));
	for (i = 0; i < RelationGetNumberOfAttributes(aorel); ++i)
	{
		proj[i] = true;
	}
	scanDesc = aocs_beginrangescan(aorel,
								   SnapshotNow, SnapshotNow,
								   &compact_segno, 1, NULL, proj);

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

	aocs_getnext(scanDesc, ForwardScanDirection, slot);
	while (!TupIsNull(slot))
	{
		CHECK_FOR_INTERRUPTS();

		aoTupleId = (AOTupleId *) slot_get_ctid(slot);
		if (AppendOnlyVisimap_IsVisible(&scanDesc->visibilityMap, aoTupleId))
		{
			AOCSMoveTuple(
						  slot,
						  insertDesc,
						  resultRelInfo,
						  estate);
			movedTupleCount++;
		}
		else
		{
			MemTuple	tuple = TupGetMemTuple(slot);

			/* Tuple is invisible and needs to be dropped */
			AppendOnlyThrowAwayTuple(aorel,
									 tuple,
									 slot,
									 mt_bind);
		}

		/*
		 * Check for vacuum delay point after approximatly a var block
		 */
		tupleCount++;
		if (VacuumCostActive && tupleCount % tuplePerPage == 0)
		{
			vacuum_delay_point();
		}

		aocs_getnext(scanDesc, ForwardScanDirection, slot);

	}

	SetAOCSFileSegInfoState(aorel, compact_segno,
							AOSEG_STATE_AWAITING_DROP);

	AppendOnlyVisimap_DeleteSegmentFile(&visiMap,
										compact_segno);

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

	aocs_endscan(scanDesc);
	pfree(proj);

	return true;
}


/*
 * Performs a compaction of an append-only AOCS relation.
 *
 * The compaction segment file should be marked as in-use/in-compaction
 * in the appendonlywriter.c code.
 */
void
AOCSDrop(Relation aorel, int compacted_segno)
{
	const char *relname;
	int			total_segfiles;
	AOCSFileSegInfo **segfile_array;
	int			i,
				segno;
	LockAcquireResult acquireResult;

	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(RelationIsAoCols(aorel));

	relname = RelationGetRelationName(aorel);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Drop AOCS relation %s", relname);

	/* Get information about all the file segments we need to scan */
	segfile_array = GetAllAOCSFileSegInfo(aorel,
										  SnapshotNow, &total_segfiles);

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
		acquireResult = LockRelationAppendOnlySegmentFile(
														  &aorel->rd_node,
														  segfile_array[i]->segno,
														  AccessExclusiveLock,
														   /* dontWait */ true);
		if (acquireResult == LOCKACQUIRE_NOT_AVAIL)
		{
			elog(DEBUG5, "drop skips AOCS segfile %d, "
				 "relation %s", segfile_array[i]->segno, relname);
			continue;
		}

		if (AOCSFileSegCanBeDropped(aorel, segno))
		{
			Assert(HasLockForSegmentFileDrop(aorel));
			AOCSCompaction_DropSegmentFile(aorel, segno);
			ClearAOCSFileSegInfo(aorel, segno, AOSEG_STATE_DEFAULT);
		}
		else
		{
			/* drop not planned, try at least eof truncation */
			AOCSFileSegInfo *fsinfo;

			fsinfo = GetAOCSFileSegInfo(aorel, SnapshotNow, segno);
			AOCSSegmentFileTruncateToEOF(aorel, fsinfo);
			pfree(fsinfo);
		}
	}

	if (segfile_array)
	{
		FreeAllAOCSSegFileInfo(segfile_array, total_segfiles);
		pfree(segfile_array);
	}
}


/*
 * Performs a compaction of an append-only relation in column-orientation.
 *
 * The compaction segment file should be marked as in-use/in-compaction in the
 * appendonlywriter.c code. If set, the insert_segno should also be marked as
 * in-use. When the insert segno is negative, only truncate to eof operations
 * can be executed.
 *
 * The caller is required to hold either an AccessExclusiveLock (vacuum full)
 * or a ShareLock on the relation.
 */
void
AOCSCompact(Relation aorel,
			int compaction_segno,
			int insert_segno,
			bool isFull)
{
	const char *relname;
	int			total_segfiles;
	AOCSFileSegInfo **segfile_array;
	AOCSInsertDesc insertDesc = NULL;
	int			i,
				segno;
	LockAcquireResult acquireResult;
	AOCSFileSegInfo *fsinfo;

	Assert(RelationIsAoCols(aorel));
	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);
	Assert(insert_segno >= 0);

	/* We cannot compact the segment file we are inserting to. */
	Assert(compaction_segno != insert_segno);

	relname = RelationGetRelationName(aorel);

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Compact AO relation %s", relname);

	/* Get information about all the file segments we need to scan */
	segfile_array = GetAllAOCSFileSegInfo(aorel, SnapshotNow, &total_segfiles);

	if (insert_segno >= 0)
	{
		insertDesc = aocs_insert_init(aorel, insert_segno, false);
	}

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
		acquireResult = LockRelationAppendOnlySegmentFile(
														  &aorel->rd_node,
														  segfile_array[i]->segno,
														  AccessExclusiveLock,
														   /* dontWait */ true);
		if (acquireResult == LOCKACQUIRE_NOT_AVAIL)
		{
			elog(DEBUG5, "compaction skips AOCS segfile %d, "
				 "relation %s", segfile_array[i]->segno, relname);
			continue;
		}

		/* Re-fetch under the write lock to get latest committed eof. */
		fsinfo = GetAOCSFileSegInfo(aorel, SnapshotNow, segno);

		/*
		 * This should not occur since this segfile info was found by the
		 * "all" method, but better to catch for trouble shooting (possibly
		 * index corruption?)
		 */
		if (fsinfo == NULL)
			elog(ERROR, "file seginfo for AOCS relation %s %u/%u/%u (segno=%u) is missing",
				 relname,
				 aorel->rd_node.spcNode,
				 aorel->rd_node.dbNode,
				 aorel->rd_node.relNode,
				 segno);

		if (AppendOnlyCompaction_ShouldCompact(aorel,
											   fsinfo->segno, fsinfo->total_tupcount, isFull))
		{
			AOCSSegmentFileFullCompaction(aorel, insertDesc, fsinfo);
		}
		else
		{
			/* compaction is skipped: Try eof truncation */
			AOCSSegmentFileTruncateToEOF(aorel, fsinfo);
		}

		pfree(fsinfo);
	}

	if (insertDesc != NULL)
		aocs_insert_finish(insertDesc);

	if (segfile_array)
	{
		FreeAllAOCSSegFileInfo(segfile_array, total_segfiles);
		pfree(segfile_array);
	}

}
