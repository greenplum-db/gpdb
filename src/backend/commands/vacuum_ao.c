/*-------------------------------------------------------------------------
 *
 * vacuum_ao.c
 *	  VACUUM support for append-only tables.
 *
 *
 * Portions Copyright (c) 2016, Pivotal Inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/vacuum_ao.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aocs_compaction.h"
#include "access/appendonlywriter.h"
#include "access/appendonly_compaction.h"
#include "access/genam.h"
#include "access/xact.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "commands/vacuum.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/freespace.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/lsyscache.h"
#include "utils/pg_rusage.h"
#include "cdb/cdbappendonlyblockdirectory.h"

/*
 * State information used during the (full)
 * vacuum of indexes on append-only tables
 */
typedef struct AppendOnlyIndexVacuumState
{
	AppendOnlyVisimap visiMap;
	AppendOnlyBlockDirectory blockDirectory;
	AppendOnlyBlockDirectoryEntry blockDirectoryEntry;
} AppendOnlyIndexVacuumState;

static void vacuum_appendonly_index(Relation indexRelation,
									AppendOnlyIndexVacuumState *vacuumIndexState,
									List *updated_stats, double rel_tuple_count, bool isfull, int elevel);

static bool appendonly_tid_reaped(ItemPointer itemptr, void *state);

static void vacuum_appendonly_rel_compact(Relation aorel, bool full, int elevel, int compaction_segno, int insert_segno);
static void vacuum_appendonly_rel_drop(Relation aorel, int elevel, int compaction_segnos);
static void vacuum_appendonly_fill_stats(Relation aorel, Snapshot snapshot, int elevel,
										 BlockNumber *rel_pages, double *rel_tuples,
										 bool *relhasindex);
static void vacuum_appendonly_indexes(Relation aoRelation, VacuumStmt *vacstmt, List *updated_stats, int elevel);


/*----------
 *	ao_vacuum_rel() -- perform VACUUM for one Append-Optimized table
 *
 * This routine vacuums a single Append-Optimized table, cleans out its
 * indexes, and updates its relpages and reltuples statistics. This is
 * comparable to full_vacuum_rel() and lazy_vacuum_rel() for heap relations.
 * For AO-tables, there is little difference between a FULL and lazy vacuum.
 *
 * By this point, if we're the QD, the caller has already dispatched the
 * VACUUM to the segments. Each QE node performs the VACUUM independently,
 * and will start and commit local transactions during the phases as needed.
 * The caller has already acquired a session lock on the table, so we don't
 * need to worry about the table being dropped in-between our transactions.
 *
 * AO compaction is rather complicated.  There are four phases:
 *   - prepare phase
 *   - compaction phase
 *   - drop phase
 *   - cleanup phase
 *
 * Out of these, compaction and drop phase might repeat multiple times.
 * We go through the list of available segment files by looking up catalog,
 * and perform a compaction operation, which appends the whole segfile
 * to another available one, if the source segfile looks to be dirty enough.
 * If we find such one and perform compaction, the next step is drop. In
 * order to allow concurrent read it is required for the drop phase to
 * be a separate transaction.  We mark the segfile as an awaiting-drop
 * in the catalog, and the drop phase actually drops the segfile from the
 * disk.  There are some cases where we cannot drop the segfile immediately,
 * in which case we just skip it and leave the catalog to have awaiting-drop
 * state for this segfile.  Aside from the compaction and drop phases, the
 * rest is much simpler.  The prepare phase is to truncate unnecessary
 * blocks after the logical EOF
 *
 * After the compaction and drop phases, we update stats info in catalog.
 *
 * Cleanup phase does normal heap vacuum on auxiliary relations (toast, aoseg,
 * block directory, visimap).  That happens in the caller, not here.
 *----------
 */
void
ao_vacuum_rel(Relation onerel, VacuumStmt *vacstmt, List *updated_stats)
{
	Oid			relid = RelationGetRelid(onerel);
	int			elevel;
	TransactionId OldestXmin;
	TransactionId FreezeLimit;
	BlockNumber	relpages;
	double		reltuples;
	bool		relhasindex;
	bool		is_drop;
	List	   *compactedSegmentFileList = NIL;
	List	   *insertedSegmentFileList = NIL;
	int			compaction_segno;
	MemoryContext oldcxt;

	if (vacstmt->verbose)
		elevel = INFO;
	else
		elevel = DEBUG2;

	/*
	 * 1. Prepare phase. Scan all indexes for dead tuples, and truncate each AO segment
	 * to its EOF.
	 */
	vacuum_set_xid_limits(vacstmt->freeze_min_age, onerel->rd_rel->relisshared,
						  &OldestXmin, &FreezeLimit);

	if (Debug_appendonly_print_compaction)
		elog(LOG, "Vacuum prepare phase %s", RelationGetRelationName(onerel));

	vacuum_appendonly_indexes(onerel, vacstmt, updated_stats, elevel);
	if (RelationIsAoRows(onerel))
		AppendOnlyTruncateToEOF(onerel);
	else
		AOCSTruncateToEOF(onerel);

	/* Close the relation. We are holding a relation lock still */
	relation_close(onerel, NoLock);
	onerel = NULL;

	/* Complete the transaction opened by the caller. */
	CommitTransactionCommand();

	StartTransactionCommand();
	onerel = relation_open(relid, NoLock);

	/*
	 * Compaction and drop phases. Repeat as many times as required.
	 */
	while ((compaction_segno = SetSegnoForCompaction(onerel,
													 compactedSegmentFileList,
													 insertedSegmentFileList,
													 &is_drop)) != -1)
	{
		/*
		 * Compact this segment (unless it was compacted earlier already, and is only
		 * awaiting to be dropped).
		 */
		if (!is_drop)
		{
			int			insert_segno;

			insert_segno = SetSegnoForCompactionInsert(onerel,
													   compaction_segno,
													   compactedSegmentFileList,
													   insertedSegmentFileList);

			vacuum_appendonly_rel_compact(onerel, vacstmt->full, elevel,
										  compaction_segno,
										  insert_segno);

			relation_close(onerel, NoLock);
			onerel = NULL;
			CommitTransactionCommand();

			StartTransactionCommand();
			onerel = relation_open(relid, NoLock);

			oldcxt = MemoryContextSwitchTo(vac_context);
			compactedSegmentFileList = lappend_int(compactedSegmentFileList, compaction_segno);
			insertedSegmentFileList = lappend_int(insertedSegmentFileList, insert_segno);
			MemoryContextSwitchTo(oldcxt);
		}

		/* Drop */
		if (Debug_appendonly_print_compaction)
			elog(LOG, "drop transaction on append-only relation %s",
				 RelationGetRelationName(onerel));

		/*
		 * Upgrade our lock to AccessExclusiveLock for the drop. Upgrading a
		 * lock poses a deadlock risk, so give up if we cannot acquire the
		 * lock immediately. We'll retry dropping the segment on the next
		 * VACUUM.
		 */
		if (ConditionalLockRelationOid(relid, AccessExclusiveLock))
		{
			RegisterSegnoForCompactionDrop(relid, compaction_segno);

			vacuum_appendonly_rel_drop(onerel, elevel, compaction_segno);

			/*
			 * Release the AccessExclusiveLock, but we still hold the
			 * (weaker) session lock.
			 */
			relation_close(onerel, AccessExclusiveLock);
			onerel = NULL;
			CommitTransactionCommand();

			StartTransactionCommand();
			onerel = relation_open(relid, NoLock);
		}
	}

	/* update statistics in pg_class */
	vacuum_appendonly_fill_stats(onerel, SnapshotNow, elevel,
								 &relpages, &reltuples, &relhasindex);
	vac_update_relstats_from_list(onerel,
								  relpages, reltuples, relhasindex,
								  FreezeLimit, updated_stats);

	/* report results to the stats collector, too */
	pgstat_report_vacuum(relid, onerel->rd_rel->relisshared, true,
						 vacstmt->analyze, reltuples);

	relation_close(onerel, NoLock);
	onerel = NULL;
	CommitTransactionCommand();
}

static bool
vacuum_appendonly_index_should_vacuum(Relation aoRelation,
									  VacuumStmt *vacstmt,
									  AppendOnlyIndexVacuumState *vacuumIndexState,
									  double *rel_tuple_count)
{
	int64		hidden_tupcount;
	FileSegTotals *totals;

	Assert(RelationIsAoRows(aoRelation) || RelationIsAoCols(aoRelation));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (rel_tuple_count)
			*rel_tuple_count = 0.0;
		return false;
	}

	if (RelationIsAoRows(aoRelation))
	{
		totals = GetSegFilesTotals(aoRelation, SnapshotNow);
	}
	else
	{
		Assert(RelationIsAoCols(aoRelation));
		totals = GetAOCSSSegFilesTotals(aoRelation, SnapshotNow);
	}
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&vacuumIndexState->visiMap);

	if (rel_tuple_count)
	{
		*rel_tuple_count = (double)(totals->totaltuples - hidden_tupcount);
		Assert((*rel_tuple_count) > -1.0);
	}

	pfree(totals);

	if (hidden_tupcount > 0 || vacstmt->full)
	{
		return true;
	}
	return false;
}

/*
 * vacuum_appendonly_indexes()
 *
 * Perform a vacuum on all indexes of an append-only relation.
 *
 * The page and tuplecount information in vacrelstats are used, the
 * nindex value is set by this function.
 */
static void
vacuum_appendonly_indexes(Relation aoRelation,
						  VacuumStmt *vacstmt,
						  List *updated_stats,
						  int elevel)
{
	int			i;
	Relation   *Irel;
	int			nindexes;
	AppendOnlyIndexVacuumState vacuumIndexState;
	FileSegInfo **segmentFileInfo = NULL; /* Might be a casted AOCSFileSegInfo */
	int			totalSegfiles;

	Assert(RelationIsAoRows(aoRelation) || RelationIsAoCols(aoRelation));
	Assert(vacstmt);

	memset(&vacuumIndexState, 0, sizeof(vacuumIndexState));

	if (Debug_appendonly_print_compaction)
		elog(LOG, "Vacuum indexes for append-only relation %s",
			 RelationGetRelationName(aoRelation));

	/* Now open all indexes of the relation */
	if (vacstmt->full)
		vac_open_indexes(aoRelation, AccessExclusiveLock, &nindexes, &Irel);
	else
		vac_open_indexes(aoRelation, RowExclusiveLock, &nindexes, &Irel);

	if (RelationIsAoRows(aoRelation))
	{
		segmentFileInfo = GetAllFileSegInfo(aoRelation, SnapshotNow, &totalSegfiles);
	}
	else
	{
		Assert(RelationIsAoCols(aoRelation));
		segmentFileInfo = (FileSegInfo **)GetAllAOCSFileSegInfo(aoRelation, SnapshotNow, &totalSegfiles);
	}

	AppendOnlyVisimap_Init(
			&vacuumIndexState.visiMap,
			aoRelation->rd_appendonly->visimaprelid,
			aoRelation->rd_appendonly->visimapidxid,
			AccessShareLock,
			SnapshotNow);

	AppendOnlyBlockDirectory_Init_forSearch(&vacuumIndexState.blockDirectory,
			SnapshotNow,
			segmentFileInfo,
			totalSegfiles,
			aoRelation,
			1,
			RelationIsAoCols(aoRelation),
			NULL);

	/* Clean/scan index relation(s) */
	if (Irel != NULL)
	{
		double rel_tuple_count = 0.0;
		if (vacuum_appendonly_index_should_vacuum(aoRelation, vacstmt,
					&vacuumIndexState, &rel_tuple_count))
		{
			Assert(rel_tuple_count > -1.0);

			for (i = 0; i < nindexes; i++)
			{
				vacuum_appendonly_index(Irel[i], &vacuumIndexState, updated_stats,
										rel_tuple_count, vacstmt->full, elevel);
			}
		}
		else
		{
			/* just scan indexes to update statistic */
			for (i = 0; i < nindexes; i++)
				scan_index(Irel[i], rel_tuple_count, updated_stats, vacstmt->full);
		}
	}

	AppendOnlyVisimap_Finish(&vacuumIndexState.visiMap, AccessShareLock);
	AppendOnlyBlockDirectory_End_forSearch(&vacuumIndexState.blockDirectory);

	if (segmentFileInfo)
	{
		if (RelationIsAoRows(aoRelation))
		{
			FreeAllSegFileInfo(segmentFileInfo, totalSegfiles);
		}
		else
		{
			FreeAllAOCSSegFileInfo((AOCSFileSegInfo **)segmentFileInfo, totalSegfiles);
		}
		pfree(segmentFileInfo);
	}

	vac_close_indexes(nindexes, Irel, NoLock);
}


/*
 * Vacuums an index on an append-only table.
 *
 * This is called after an append-only segment file compaction to move
 * all tuples from the compacted segment files.
 * The segmentFileList is an
 */
static void
vacuum_appendonly_index(Relation indexRelation,
						AppendOnlyIndexVacuumState *vacuumIndexState,
						List *updated_stats,
						double rel_tuple_count,
						bool isfull,
						int elevel)
{
	Assert(RelationIsValid(indexRelation));
	Assert(vacuumIndexState);

	IndexBulkDeleteResult *stats;
	IndexVacuumInfo ivinfo;
	PGRUsage	ru0;

	pg_rusage_init(&ru0);

	ivinfo.index = indexRelation;
	ivinfo.vacuum_full = isfull;
	ivinfo.message_level = elevel;
	ivinfo.num_heap_tuples = rel_tuple_count;
	ivinfo.strategy = GetAccessStrategy(BAS_VACUUM);

	/* Do bulk deletion */
	stats = index_bulk_delete(&ivinfo, NULL, appendonly_tid_reaped,
							  (void *) vacuumIndexState);

	/* Do post-VACUUM cleanup */
	stats = index_vacuum_cleanup(&ivinfo, stats);

	if (!stats)
		return;

	/* now update statistics in pg_class */
	vac_update_relstats_from_list(indexRelation,
						stats->num_pages, stats->num_index_tuples,
						false, InvalidTransactionId, updated_stats);

	ereport(elevel,
			(errmsg("index \"%s\" now contains %.0f row versions in %u pages",
					RelationGetRelationName(indexRelation),
					stats->num_index_tuples,
					stats->num_pages),
			 errdetail("%.0f index row versions were removed.\n"
			 "%u index pages have been deleted, %u are currently reusable.\n"
					   "%s.",
					   stats->tuples_removed,
					   stats->pages_deleted, stats->pages_free,
					   pg_rusage_show(&ru0))));

	pfree(stats);
}

static bool
appendonly_tid_reaped_check_block_directory(AppendOnlyIndexVacuumState *vacuumState,
											AOTupleId *aoTupleId)
{
	if (vacuumState->blockDirectory.currentSegmentFileNum ==
			AOTupleIdGet_segmentFileNum(aoTupleId) &&
			AppendOnlyBlockDirectoryEntry_RangeHasRow(&vacuumState->blockDirectoryEntry,
				AOTupleIdGet_rowNum(aoTupleId)))
	{
		return true;
	}

	if (!AppendOnlyBlockDirectory_GetEntry(&vacuumState->blockDirectory,
		aoTupleId,
		0,
		&vacuumState->blockDirectoryEntry))
	{
		return false;
	}
	return (vacuumState->blockDirectory.currentSegmentFileNum ==
			AOTupleIdGet_segmentFileNum(aoTupleId) &&
			AppendOnlyBlockDirectoryEntry_RangeHasRow(&vacuumState->blockDirectoryEntry,
				AOTupleIdGet_rowNum(aoTupleId)));
}

/*
 * appendonly_tid_reaped()
 *
 * Is a particular tid for an appendonly reaped?
 * state should contain an integer list of all compacted
 * segment files.
 *
 * This has the right signature to be an IndexBulkDeleteCallback.
 */
static bool
appendonly_tid_reaped(ItemPointer itemptr, void *state)
{
	AOTupleId  *aoTupleId;
	AppendOnlyIndexVacuumState *vacuumState;
	bool		reaped;

	Assert(itemptr);
	Assert(state);

	aoTupleId = (AOTupleId *)itemptr;
	vacuumState = (AppendOnlyIndexVacuumState *)state;

	reaped = !appendonly_tid_reaped_check_block_directory(vacuumState,
														  aoTupleId);
	if (!reaped)
	{
		/* Also check visi map */
		reaped = !AppendOnlyVisimap_IsVisible(&vacuumState->visiMap,
		aoTupleId);
	}

	if (Debug_appendonly_print_compaction)
		elog(DEBUG3, "Index vacuum %s %d",
			 AOTupleIdToString(aoTupleId), reaped);
	return reaped;
}


/*
 * Fills in the relation statistics for an append-only relation.
 *
 *	This information is used to update the reltuples and relpages information
 *	in pg_class. reltuples is the same as "pg_aoseg_<oid>:tupcount"
 *	column and we simulate relpages by subdividing the eof value
 *	("pg_aoseg_<oid>:eof") over the defined page size.
 */
static void
vacuum_appendonly_fill_stats(Relation aorel, Snapshot snapshot, int elevel,
							 BlockNumber *rel_pages, double *rel_tuples,
							 bool *relhasindex)
{
	FileSegTotals *fstotal;
	BlockNumber nblocks;
	char	   *relname;
	double		num_tuples;
	double		totalbytes;
	double		eof;
	int64       hidden_tupcount;
	AppendOnlyVisimap visimap;

	Assert(RelationIsAoRows(aorel) || RelationIsAoCols(aorel));

	relname = RelationGetRelationName(aorel);

	/* get updated statistics from the pg_aoseg table */
	if (RelationIsAoRows(aorel))
	{
		fstotal = GetSegFilesTotals(aorel, snapshot);
	}
	else
	{
		Assert(RelationIsAoCols(aorel));
		fstotal = GetAOCSSSegFilesTotals(aorel, snapshot);
	}

	/* calculate the values we care about */
	eof = (double)fstotal->totalbytes;
	num_tuples = (double)fstotal->totaltuples;
	totalbytes = eof;
	nblocks = (uint32)RelationGuessNumberOfBlocks(totalbytes);

	AppendOnlyVisimap_Init(&visimap,
						   aorel->rd_appendonly->visimaprelid,
						   aorel->rd_appendonly->visimapidxid,
						   AccessShareLock,
						   snapshot);
	hidden_tupcount = AppendOnlyVisimap_GetRelationHiddenTupleCount(&visimap);
	num_tuples -= hidden_tupcount;
	Assert(num_tuples > -1.0);
	AppendOnlyVisimap_Finish(&visimap, AccessShareLock);

	if (Debug_appendonly_print_compaction)
		elog(LOG,
			 "Gather statistics after vacuum for append-only relation %s: "
			 "page count %d, tuple count %f",
			 relname,
			 nblocks, num_tuples);

	*rel_pages = nblocks;
	*rel_tuples = num_tuples;
	*relhasindex = aorel->rd_rel->relhasindex;

	ereport(elevel,
			(errmsg("\"%s\": found %.0f rows in %u pages.",
					relname, num_tuples, nblocks)));
	pfree(fstotal);
}

/*
 *	vacuum_appendonly_rel() -- vaccum an append-only relation
 *
 *		This procedure will be what gets executed both for VACUUM
 *		and VACUUM FULL (and also ANALYZE or any other thing that
 *		needs the pg_class stats updated).
 *
 *		The function can compact append-only segment files or just
 *		truncating the segment file to its existing eof.
 *
 *		Afterwards, the reltuples and relpages information in pg_class
 *		are updated. reltuples is the same as "pg_aoseg_<oid>:tupcount"
 *		column and we simulate relpages by subdividing the eof value
 *		("pg_aoseg_<oid>:eof") over the defined page size.
 *
 *
 *		There are txn ids, hint bits, free space, dead tuples,
 *		etc. these are all irrelevant in the append only relation context.
 */
static void
vacuum_appendonly_rel_compact(Relation aorel, bool full, int elevel, int compaction_segno, int insert_segno)
{
	char	   *relname;
	PGRUsage	ru0;

	Assert(RelationIsAoRows(aorel) || RelationIsAoCols(aorel));

	pg_rusage_init(&ru0);
	relname = RelationGetRelationName(aorel);
	ereport(elevel,
			(errmsg("vacuuming \"%s.%s\"",
					get_namespace_name(RelationGetNamespace(aorel)),
					relname)));

	if (Gp_role == GP_ROLE_DISPATCH)
		return;

	if (insert_segno == APPENDONLY_COMPACTION_SEGNO_INVALID)
	{
		if (Debug_appendonly_print_compaction)
			elog(LOG, "Vacuum pseudo-compaction phase %s", RelationGetRelationName(aorel));
	}
	else
	{
		if (Debug_appendonly_print_compaction)
			elog(LOG, "Vacuum compaction phase %s", RelationGetRelationName(aorel));
		if (RelationIsAoRows(aorel))
			AppendOnlyCompact(aorel, compaction_segno, insert_segno, full);
		else
		{
			Assert(RelationIsAoCols(aorel));
			AOCSCompact(aorel, compaction_segno, insert_segno, full);
		}
	}
}
static void
vacuum_appendonly_rel_drop(Relation aorel, int elevel, int compaction_segno)
{
	char	   *relname;
	PGRUsage	ru0;

	Assert(RelationIsAoRows(aorel) || RelationIsAoCols(aorel));

	pg_rusage_init(&ru0);
	relname = RelationGetRelationName(aorel);
	ereport(elevel,
			(errmsg("vacuuming \"%s.%s\"",
					get_namespace_name(RelationGetNamespace(aorel)),
					relname)));

	if (Gp_role == GP_ROLE_DISPATCH)
		return;

	if (Debug_appendonly_print_compaction)
		elog(LOG, "Vacuum drop phase %s", RelationGetRelationName(aorel));

	if (RelationIsAoRows(aorel))
		AppendOnlyDrop(aorel, compaction_segno);
	else
	{
		Assert(RelationIsAoCols(aorel));
		AOCSDrop(aorel, compaction_segno);
	}
}
