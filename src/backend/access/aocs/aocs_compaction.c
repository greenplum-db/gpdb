/*------------------------------------------------------------------------------
 *
 * Code dealing with the compaction of append-only tables.
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/aocs/aocs_compaction.c
 *
 *------------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/aosegfiles.h"
#include "access/aomd.h"
#include "access/aocs_compaction.h"
#include "access/appendonly_compaction.h"
#include "access/appendonlywriter.h"
#include "catalog/catalog.h"
#include "catalog/gp_fastsequence.h"
#include "catalog/indexing.h"
#include "catalog/pg_appendonly.h"
#include "catalog/pg_attribute_encoding.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbvars.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "storage/lmgr.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/guc.h"
#include "miscadmin.h"
#include "commands/progress.h"
#include "pgstat.h"

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
void
AOCSCompaction_DropSegmentFile(Relation aorel, int segno, AOVacuumRelStats *vacrelstats)
{
	int			col;

	Assert(RelationIsAoCols(aorel));

	for (col = 0; col < RelationGetNumberOfAttributes(aorel); col++)
	{
		char		filenamepath[MAXPGPATH];
		int			pseudoSegNo;
		File		fd;
		/* Filenum for the col */
		FileNumber  filenum = GetFilenumForAttribute(RelationGetRelid(aorel), col + 1);

		/* Open and truncate the relation segfile */
		MakeAOSegmentFileName(aorel, segno, filenum, &pseudoSegNo, filenamepath);

		elogif(Debug_appendonly_print_compaction, LOG,
			   "Drop segment file: "
			   "segno %d",
			   pseudoSegNo);

		fd = OpenAOSegmentFile(filenamepath, 0);
		if (fd >= 0)
		{
			TruncateAOSegmentFile(fd, aorel, pseudoSegNo, 0, vacrelstats);
			CloseAOSegmentFile(fd);
		}
		else
		{
			/*
			 * The file we were about to drop/truncate didn't exist. That's normal,
			 * for example, if a column is added with ALTER TABLE ADD COLUMN.
			 */
			elog(DEBUG1, "could not truncate segfile %s, because it does not exist", filenamepath);
		}
	}
}

/*
 * AOCSSegmentFileTruncateToEOF()
 *
 * Truncates the files for all columns of logical segfile 'segno' to
 * the EOF values from 'vpinfo'. The caller is responsible for locking so
 * that a concurrent backend doesn't write to the segfile while we truncate
 * it.
 *
 * This is used to clean up space left behind by aborted or crashed
 * transactions.
 */
void
AOCSSegmentFileTruncateToEOF(Relation aorel, int segno, AOCSVPInfo *vpinfo, AOVacuumRelStats *vacrelstats)
{
	const char *relname = RelationGetRelationName(aorel);
	int			j;

	Assert(RelationIsAoCols(aorel));

	for (j = 0; j < vpinfo->nEntry; ++j)
	{
		int64		segeof;
		char		filenamepath[MAXPGPATH];
		AOCSVPInfoEntry *entry;
		File		fd;
		int32		fileSegNo;
		/* Filenum for the column */
		FileNumber  filenum = GetFilenumForAttribute(RelationGetRelid(aorel), j + 1);

		entry = &vpinfo->entry[j];
		segeof = entry->eof;

		/* Open and truncate the relation segfile to its eof */
		MakeAOSegmentFileName(aorel, segno, filenum, &fileSegNo, filenamepath);

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

		fd = OpenAOSegmentFile(filenamepath, segeof);
		if (fd >= 0)
		{
			TruncateAOSegmentFile(fd, aorel, fileSegNo, segeof, vacrelstats);
			CloseAOSegmentFile(fd);

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

	oldAoTupleId = (AOTupleId *) &slot->tts_tid;
	/* Extract all the values of the tuple */
	slot_getallattrs(slot);

	(void) aocs_insert_values(insertDesc,
							  slot->tts_values,
							  slot->tts_isnull,
							  &newAoTupleId);
	memcpy(&slot->tts_tid, &newAoTupleId, sizeof(ItemPointerData));

	/* insert index' tuples if needed */
	if (resultRelInfo->ri_NumIndices > 0)
	{
		ExecInsertIndexTuples(slot, estate, false, false, NIL);
		ResetPerTupleExprContext(estate);
	}

	elogif(Debug_appendonly_print_compaction, DEBUG5,
		   "Compaction: Moved tuple (%d," INT64_FORMAT ") -> (%d," INT64_FORMAT ")",
		   AOTupleIdGet_segmentFileNum(oldAoTupleId), AOTupleIdGet_rowNum(oldAoTupleId),
		   AOTupleIdGet_segmentFileNum(&newAoTupleId), AOTupleIdGet_rowNum(&newAoTupleId));
}

/*
 * Subroutine of AOCSCompact().
 */
static bool
AOCSSegmentFileFullCompaction(Relation aorel,
							  AOCSInsertDesc insertDesc,
							  AOCSFileSegInfo *fsinfo,
							  Snapshot snapshot,
							  AOVacuumRelStats *vacrelstats)
{
	const char *relname;
	AppendOnlyVisimap visiMap;
	AOCSScanDesc scanDesc;
	TupleDesc	tupDesc;
	TupleTableSlot *slot;
	int			compact_segno;
	ResultRelInfo *resultRelInfo;
	MemTupleBinding *mt_bind;
	EState	   *estate;
	int64		moved_tupleCount = 0;
	int64		tuplePerPage = INT_MAX;
	int64		curr_num_dead_tuples = 0;
	int64		prev_num_dead_tuples = 0;
	int64		curr_heap_blks_scanned = 0;
	int64		prev_heap_blks_scanned = 0;

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
						   insertDesc->visimaprelid,
						   ShareLock,
						   snapshot);

	elogif(Debug_appendonly_print_compaction,
		   LOG, "Compact AO segfile %d, relation %sd",
		   compact_segno, relname);

	scanDesc = aocs_beginrangescan(aorel,
								   snapshot, snapshot,
								   &compact_segno, 1);

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
	 * We don't want uniqueness checks to be performed while "insert"ing tuples
	 * to a destination segfile during AOCSMoveTuple(). This is to ensure that
	 * we can avoid spurious conflicts between the moved tuple and the original
	 * tuple.
	 */
	estate->gp_bypass_unique_check = true;

	while (aocs_getnext(scanDesc, ForwardScanDirection, slot))
	{
		CHECK_FOR_INTERRUPTS();

		/*
 		 * AppendOnlyVisimap_IsVisible() has already been called in aocs_getnext().
		 */
		Assert(AppendOnlyVisimap_IsVisible(&scanDesc->visibilityMap,
										   (AOTupleId *) &slot->tts_tid));
		AOCSMoveTuple(slot,
					  insertDesc,
					  resultRelInfo,
					  estate);

		/*
		 * Check for vacuum delay point after approximatly a var block
		 */
		moved_tupleCount++;
		if (VacuumCostActive && moved_tupleCount % tuplePerPage == 0)
		{
			vacuum_delay_point();
		}

		/*
		 * Report that we are now scanning and compacting segment files.
		 */
		curr_num_dead_tuples = scanDesc->cur_seg_row + 1 - moved_tupleCount;
		if (curr_num_dead_tuples > prev_num_dead_tuples)
		{
			pgstat_progress_update_param(PROGRESS_VACUUM_NUM_DEAD_TUPLES,
										 vacrelstats->num_dead_tuples + curr_num_dead_tuples);
			prev_num_dead_tuples = curr_num_dead_tuples;
		}

		curr_heap_blks_scanned = RelationGuessNumberOfBlocksFromSize(scanDesc->totalBytesRead);
		if (curr_heap_blks_scanned > prev_heap_blks_scanned)
		{
			pgstat_progress_update_param(PROGRESS_VACUUM_HEAP_BLKS_SCANNED,
										 curr_heap_blks_scanned);
			prev_heap_blks_scanned = curr_heap_blks_scanned;
		}
	}
	/* Accumulate total number dead tuples */
	vacrelstats->num_dead_tuples += scanDesc->cur_seg_row - moved_tupleCount;

	MarkAOCSFileSegInfoAwaitingDrop(aorel, compact_segno);

	AppendOnlyVisimap_DeleteSegmentFile(&visiMap,
										compact_segno);

	/* Delete all mini pages of the segment files if block directory exists */
	if (OidIsValid(insertDesc->blkdirrelid))
	{
		AppendOnlyBlockDirectory_DeleteSegmentFile(aorel,
												   snapshot,
												   compact_segno,
												   0);
	}

	elogif(Debug_appendonly_print_compaction, LOG,
		   "Finished compaction: "
		   "AO segfile %d, relation %s, moved tuple count " INT64_FORMAT,
		   compact_segno, relname, moved_tupleCount);

	AppendOnlyVisimap_Finish(&visiMap, NoLock);

	ExecCloseIndices(resultRelInfo);
	FreeExecutorState(estate);

	ExecDropSingleTupleTableSlot(slot);
	destroy_memtuple_binding(mt_bind);

	aocs_endscan(scanDesc);

	return true;
}

/*
 * Performs a compaction of an append-only relation in column-orientation.
 *
 * The compaction segment file should be locked for this transaction in
 * the appendonlywriter.c code.
 *
 * On exit, *insert_segno will be set to the segment that was used as the
 * insertion target. The segfiles listed in 'avoid_segnos' will not be used
 * for insertion.
 *
 * The caller is required to hold either an AccessExclusiveLock (vacuum full)
 * or a ShareLock on the relation.
 */
void
AOCSCompact(Relation aorel,
			int compaction_segno,
			int *insert_segno,
			bool isFull,
			List *avoid_segnos,
			AOVacuumRelStats *vacrelstats)
{
	const char *relname;
	AOCSInsertDesc insertDesc = NULL;
	AOCSFileSegInfo *fsinfo;
	Snapshot	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));

	Assert(RelationIsAoCols(aorel));
	Assert(Gp_role == GP_ROLE_EXECUTE || Gp_role == GP_ROLE_UTILITY);

	relname = RelationGetRelationName(aorel);
	elogif(Debug_appendonly_print_compaction, LOG,
		   "Compact AO relation %s", relname);

	/* Fetch under the write lock to get latest committed eof. */
	fsinfo = GetAOCSFileSegInfo(aorel, appendOnlyMetaDataSnapshot, compaction_segno, true);

	if (AppendOnlyCompaction_ShouldCompact(aorel,
										   compaction_segno, fsinfo->total_tupcount, isFull,
										   appendOnlyMetaDataSnapshot))
	{
		if (*insert_segno == -1)
		{
			/* get the insertion segment on first call. */
			*insert_segno = ChooseSegnoForCompactionWrite(aorel, avoid_segnos);
		}

		if (*insert_segno != -1)
		{
			/*
			 * Note: since we don't know how many rows will actually be inserted
			 * we provide the default number of rows to bump gp_fastsequence by.
			 */
			insertDesc = aocs_insert_init(aorel, *insert_segno, NUM_FAST_SEQUENCES);

			AOCSSegmentFileFullCompaction(aorel,
										  insertDesc,
										  fsinfo,
										  appendOnlyMetaDataSnapshot,
										  vacrelstats);

			insertDesc->skipModCountIncrement = true;
			aocs_insert_finish(insertDesc);
		}
		else
		{
			/* FIXME: Could not find a target segment. What now? */
		}
	}

	pfree(fsinfo);

	UnregisterSnapshot(appendOnlyMetaDataSnapshot);
}
