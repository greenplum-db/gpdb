/*-------------------------------------------------------------------------
 *
 * appendonlywriter.c
 *	  routines for selecting AO segment for inserts.
 *
 *
 * Note: This is also used by AOCS tables.
 *
 * Portions Copyright (c) 2008, Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonlywriter.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/appendonly_compaction.h"
#include "access/appendonlytid.h"	  /* AOTupleId_MaxRowNum  */
#include "access/appendonlywriter.h"
#include "access/aocssegfiles.h"                  /* AOCS */
#include "access/heapam.h"			  /* heap_open            */
#include "access/transam.h"
#include "access/xact.h"
#include "utils/snapmgr.h"

/*
 * local functions
 */
static int SetSegnoInternal(Relation rel, List *avoid_segnos, bool for_compaction);

#define SEGFILE_CAPACITY_THRESHOLD	0.9

/*
 * segfileMaxRowThreshold
 *
 * Returns the row count threshold - when a segfile more than this number of
 * rows we don't allow inserting more data into it anymore.
 */
static
int64  segfileMaxRowThreshold(void)
{
	int64 maxallowed = (int64)AOTupleId_MaxRowNum;

	if(maxallowed < 0)
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("int64 out of range")));

	return 	(SEGFILE_CAPACITY_THRESHOLD * maxallowed);
}

/*
 * usedByConcurrentTransaction
 *
 * Return true if segno has been used by any transactions
 * that are running concurrently with the current transaction.
 */
static bool
usedByConcurrentTransaction(TransactionId latestWriteXid, int segno)
{
	Snapshot	snapshot;
	int			i;

	/*
	 * Obtain the snapshot that is taken at the beginning of the
	 * transaction. We can't simply call GetTransactionSnapshot() here because
	 * it will create a new distributed snapshot for non-serializable
	 * transaction isolation level, and it may be too late.
	 */
	if (SerializableSnapshot == NULL)
	{
		Assert(LatestSnapshot == NULL);
		(void) GetTransactionSnapshot();
		Assert(SerializableSnapshot);
	}
	snapshot = SerializableSnapshot;

	if (Debug_appendonly_print_segfile_choice)
	{
		elog(LOG, "usedByConcurrentTransaction: TransactionXmin = %u, xmin = %u, xmax = %u, myxid = %u latestWriteXid that uses segno %d is %u",
			 TransactionXmin, snapshot->xmin, snapshot->xmax, GetCurrentTransactionIdIfAny(), segno, latestWriteXid);
		LogDistributedSnapshotInfo(snapshot, "Used snapshot: ");
	}

	/*
	 * Like in XidInSnapshot(), make a quick range check with the xmin and xmax first.
	 */
	if (TransactionIdPrecedes(latestWriteXid, snapshot->xmin))
	{
		if (Debug_appendonly_print_segfile_choice)
			ereport(LOG,
					(errmsg("usedByConcurrentTransaction: latestWriterXid %u of segno %d precedes xmin %u, so it is not considered concurrent",
							latestWriteXid,
							segno,
							snapshot->xmin)));
		return false;
	}
	if (TransactionIdFollowsOrEquals(latestWriteXid, snapshot->xmax))
	{
		if (Debug_appendonly_print_segfile_choice)
			ereport(LOG,
					(errmsg("usedByConcurrentTransaction: latestWriterXid %u of segno %d follows or is equal toxmax %u, so it is considered concurrent",
							latestWriteXid,
							segno,
							snapshot->xmax)));
		return true;
	}

	/* Check the XID array in the snapshot */
	for (i = 0; i < snapshot->xcnt; i++)
	{
		if (TransactionIdEquals(latestWriteXid, snapshot->xip[i]))
		{
			if (Debug_appendonly_print_segfile_choice)
				ereport(LOG,
						(errmsg("usedByConcurrentTransaction: latestWriterXid %u of segno %d was found in snapshot, so it is considered concurrent",
								latestWriteXid,
								segno)));

			return true;
		}
	}

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("usedByConcurrentTransaction: latestWriterXid %u of segno %d is not considered concurrent",
						latestWriteXid,
						segno)));
       return false;
}

typedef struct
{
	int32		segno;
	TransactionId xmin;
	ItemPointerData ctid;
	float8		tupcount;
} candidate_segment;

static int
compare_candidates(const void *a, const void *b)
{
	candidate_segment *ca = (candidate_segment *) a;
	candidate_segment *cb = (candidate_segment *) b;

	if (ca->tupcount < cb->tupcount)
		return -1;
	else if (ca->tupcount > cb->tupcount)
		return 1;
	else
		return 0;
}

int
SetSegnoForWrite(Relation rel, List *avoid_segnos)
{
	return SetSegnoInternal(rel, avoid_segnos, false);
}

int
SetSegnoForCompaction(Relation rel, List *avoid_segnos)
{
	return SetSegnoInternal(rel, avoid_segnos, true);
}

/*
 * SetSegnoForWrite
 *
 * This function includes the key logic of the append only writer.
 *
 * Decide which segment number should be used to write into during the COPY,
 * INSERT, or VACUUM operation we're executing.
 *
 * If 'for_compaction' is true, and no existing target segment can be used
 * as target, returns -1. (This is used by VACUUM). Otherwise, a new segment
 * is created, or if we have reached the maximum number of segments, an
 * error is thrown.
 *
 * The return value is a segment file number to use for inserting by each
 * segdb into its local AO table.
 *
 * It does so by scanning the AO segment table, looking for segments that
 * are not currently in use. The one among those that has least data, is
 * chosen. Or, if we are in an explicit transaction and inserting into the
 * same table we use the same segno over and over again.
 *
 * If 'avoid_segnos' is non-empty, we will not choose any of those segments as
 * the target.
 */
static int
SetSegnoInternal(Relation rel, List *avoid_segnos, bool for_compaction)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	int			i;
	int32		segno;
	int64		tupcount;
	int16		state;
	bool		isNull;
	int32		chosen_segno = -1;
	candidate_segment candidates[MAX_AOREL_CONCURRENCY];
	bool		used[MAX_AOREL_CONCURRENCY];
	int			ncandidates = 0;
	HeapScanDesc aoscan;
	HeapTuple	tuple;
	int16	formatversion;

	memset(used, 0, sizeof(used));

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("SetSegnoForWrite: Choosing a segno for append-only "
						"relation \"%s\"",
						RelationGetRelationName(rel))));

	/*
	 * The algorithm below for choosing a target segment is not concurrent-safe.
	 * Grab a lock to serialize.
	 */
	LockRelationForExtension(rel, ExclusiveLock);

	/*
	 * Now pick a segment that is not in use, and is not over the allowed
	 * size threshold (90% full).
	 */
	pg_aoseg_rel = heap_open(rel->rd_appendonly->segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	aoscan = heap_beginscan(pg_aoseg_rel, SnapshotNow, 0, NULL);
	while ((tuple = heap_getnext(aoscan, ForwardScanDirection)) != NULL)
	{
		if (RelationIsAoRows(rel))
		{
			segno = DatumGetInt32(fastgetattr(tuple,
											  Anum_pg_aoseg_segno,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);

			tupcount = (int64) DatumGetFloat8(fastgetattr(tuple,
														  Anum_pg_aoseg_tupcount,
														  pg_aoseg_dsc, &isNull));
			Assert(!isNull);

			state = fastgetattr(tuple, Anum_pg_aoseg_state, pg_aoseg_dsc, &isNull);
			Assert(!isNull);

			formatversion = DatumGetInt16(fastgetattr(tuple,
													  Anum_pg_aoseg_formatversion,
													  pg_aoseg_dsc, &isNull));
			Assert(!isNull);
		}
		else
		{
			segno = DatumGetInt32(fastgetattr(tuple,
											  Anum_pg_aocs_segno,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);
			tupcount = DatumGetInt64(fastgetattr(tuple,
												  Anum_pg_aocs_tupcount,
												  pg_aoseg_dsc, &isNull));
			Assert(!isNull);

			state = DatumGetInt16(fastgetattr(tuple,
											  Anum_pg_aocs_state,
											  pg_aoseg_dsc, &isNull));
			Assert(!isNull);

			formatversion = DatumGetInt16(fastgetattr(tuple,
													  Anum_pg_aocs_formatversion,
													  pg_aoseg_dsc, &isNull));
			Assert(!isNull);
		}

		used[segno] = true;

		if (state != AOSEG_STATE_DEFAULT)
			continue;

		/* If the ao segment is full, skip it */
		if (tupcount > segfileMaxRowThreshold())
			continue;

		if (list_member_int(avoid_segnos, segno))
			continue;

		/* Skip using the ao segment if not latest version */
		if (formatversion != AORelationVersion_GetLatest())
			continue;

		/*
		 * If we have already used this segment in this transaction, no need to look
		 * further. We can continue to use it
		 *
		 * If we already picked a segno for a previous statement
		 * in this very same transaction we are still in (explicit txn) we
		 * pick the same one to insert into it again.
		 */
		if (HeapTupleHeaderGetXmin(tuple->t_data) == GetCurrentTransactionId())
		{
			chosen_segno = segno;
			break;
		}

		candidates[ncandidates].segno = segno;
		candidates[ncandidates].xmin = HeapTupleHeaderGetXmin(tuple->t_data);
		candidates[ncandidates].ctid = tuple->t_self;
		candidates[ncandidates].tupcount = tupcount;
		ncandidates++;
	}
	heap_endscan(aoscan);

	/* Try to find a segment we can use among the candidates, and lock it. */
	if (chosen_segno == -1)
	{
		/*
		 * Sort the candidates by tuple count, to prefer segment with fewest existing
		 * tuples.
		 */
		qsort((void *) candidates, ncandidates, sizeof(candidate_segment),
			  compare_candidates);

		for (i = 0; i < ncandidates; i++)
		{
			if (!usedByConcurrentTransaction(candidates[i].xmin, candidates[i].segno))
			{
				/* this segno is available and not full. Try to lock it. */
				HeapTupleData locktup;
				Buffer		buf = InvalidBuffer;
				ItemPointerData update_ctid;
				TransactionId update_xmax;
				HTSU_Result result;

				locktup.t_self = candidates[i].ctid;
				result = heap_lock_tuple(pg_aoseg_rel, &locktup, &buf,
										 &update_ctid, &update_xmax,
										 GetCurrentCommandId(true),
										 LockTupleExclusive, LockTupleIfNotLocked);
				if (BufferIsValid(buf))
					ReleaseBuffer(buf);
				if (result == HeapTupleMayBeUpdated)
				{
					chosen_segno = candidates[i].segno;
					break;
				}
			}
		}
	}

	if (chosen_segno == -1 && !for_compaction)
	{
		/* No segment found. Try to create a new one. */
		for (segno = 0; segno < MAX_AOREL_CONCURRENCY; segno++)
		{
			if (!used[segno] && !list_member_int(avoid_segnos, segno))
			{
				chosen_segno = segno;

				if (RelationIsAoRows(rel))
					InsertInitialSegnoEntry(rel, chosen_segno);
				else
					InsertInitialAOCSFileSegInfo(rel, chosen_segno,
												 RelationGetNumberOfAttributes(rel));
				break;
			}
		}

		/* If can't create a new one because MAX_AOREL_CONCURRENCY was reached */
		if (chosen_segno == -1)
		{
			ereport(ERROR,
					(errmsg("could not find segment file to use for inserting into relation \"%s\".",
							RelationGetRelationName(rel))));
		}
	}

	/* OK, we have the aoseg tuple locked for us. */

	UnlockRelationForExtension(rel, ExclusiveLock);

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("Segno chosen for append-only relation \"%s\" is %d",
						RelationGetRelationName(rel), chosen_segno)));

	heap_close(pg_aoseg_rel, AccessShareLock);

	return chosen_segno;
}

/*
 * AORelIncrementModCount
 *
 * Update the modcount of an aoseg table. The modcount is used by incremental backup
 * to detect changed relations.
 */
void
AORelIncrementModCount(Relation parentrel)
{
	int			segno;

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("UpdateMasterAosegTotals: Incrementing modcount of aoseg entry for append-only relation %d",
						RelationGetRelid(parentrel))));

	/*
	 * It doesn't matter which segment we use, as long as the segment can be used by us
	 * (same rules as for inserting).
	 */
	segno = SetSegnoForWrite(parentrel, NIL);

	// CONSIDER: We should probably get this lock even sooner.
	LockRelationAppendOnlySegmentFile(
									&parentrel->rd_node,
									segno,
									AccessExclusiveLock,
									/* dontWait */ false);

	if (RelationIsAoRows(parentrel))
	{
		/*
		 * Update the master AO segment info table with correct tuple count total
		 */
		IncrementFileSegInfoModCount(parentrel, segno);
	}
	else
	{
		/* AO column store */
		AOCSFileSegInfoAddCount(parentrel, segno, 0, 0, 1);
	}
}

void ValidateAppendOnlyMetaDataSnapshot(
	Snapshot *appendOnlyMetaDataSnapshot)
{
	// Placeholder.
}
