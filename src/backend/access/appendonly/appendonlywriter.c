/*-------------------------------------------------------------------------
 *
 * appendonlywriter.c
 *	  routines for selecting AO segment for inserts.
 *
 *
 * Note: This is also used by AOCS tables.
 *
 * Portions Copyright (c) 2008, Greenplum Inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "access/appendonlytid.h"		/* AOTupleId_MaxRowNum  */
#include "access/appendonlywriter.h"
#include "access/aocssegfiles.h"		/* AOCS */
#include "access/heapam.h"				/* heap_open */
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_authid.h"
#include "cdb/cdbvars.h"
#include "libpq-fe.h"
#include "miscadmin.h"
#include "nodes/pathnodes.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/faultinjector.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/int8.h"
#include "utils/snapmgr.h"

#define SEGFILE_CAPACITY_THRESHOLD	0.9


/*
 * Modes of operation for the choose_segno_internal() function.
 */
typedef enum
{
	/*
	 * Normal mode; select a segment to insert to, for INSERT or COPY.
	 */
	CHOOSE_MODE_WRITE,

	/*
	 * Select a segment to insert surviving rows to, when compacting
	 * another segfile in VACUUM.
	 */
	CHOOSE_MODE_COMPACTION_WRITE,

	/*
	 * Select next segment to compact.
	 */
	CHOOSE_MODE_COMPACTION_TARGET
} choose_segno_mode;

/*
 * local functions
 */
static int choose_segno_internal(Relation rel, List *avoid_segnos, choose_segno_mode mode);
static int choose_new_segfile(Relation rel, bool *used, List *avoid_segnos);
static void get_aoseg_fields(Relation rel, Relation pg_aoseg_rel, HeapTuple tuple,
							 int32 *segno, int64 *tupcount, int16 *state, int16 *formatversion);

/*
 * segfileMaxRowThreshold
 *
 * Returns the row count threshold - when a segfile more than this number of
 * rows we don't allow inserting more data into it anymore.
 */
static int64
segfileMaxRowThreshold(void)
{
	int64		maxallowed = (int64) AOTupleId_MaxRowNum;

	if (maxallowed < 0)
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("int64 out of range")));

	return (SEGFILE_CAPACITY_THRESHOLD * maxallowed);
}

typedef struct
{
	int32		segno;
	ItemPointerData ctid;
	float8		tupcount;
} candidate_segment;

/*
 * Compare candidate segments on tuple count.
 */
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
	{
		/* On tie, prefer lower-numbered segment */
		if (ca->segno < cb->segno)
			return -1;
		else
		{
			Assert(ca->segno > cb->segno);
			return 1;
		}
	}
}

/*
 * Lock an existing segfile for writing.
 *
 * The caller should ensure that the segfile is available for writing,
 * otherwise this will error out. Typical usage is to pass segno=0 on
 * a newly-created relation, e.g. in CREATE TABLE AS.
 *
 * The locking and logic for whether a segfile can be used is mostly
 * the same as in choose_segfile_internal(), but we already know which
 * segfile we want.
 */
void
LockSegnoForWrite(Relation rel, int segno)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	SysScanDesc aoscan;
	HeapTuple	tuple;
	Snapshot	snapshot;
	Snapshot	appendOnlyMetaDataSnapshot;
	Oid			segrelid;
	bool		found = false;

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("LockSegNoForWrite: Locking segno %d for append-only relation \"%s\"",
						segno, RelationGetRelationName(rel))));

	/*
	 * The algorithm below for choosing a target segment is not concurrent-safe.
	 * Grab a lock to serialize.
	 */
	LockRelationForExtension(rel, ExclusiveLock);

	appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
	GetAppendOnlyEntryAuxOids(rel->rd_id, appendOnlyMetaDataSnapshot,
							  &segrelid, NULL, NULL, NULL, NULL);
	/*
	 * Now pick a segment that is not in use, and is not over the allowed
	 * size threshold (90% full).
	 */
	pg_aoseg_rel = table_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	/*
	 * Obtain the snapshot that is taken at the beginning of the transaction.
	 * If a tuple is visible to this snapshot, and it hasn't been updated since
	 * (that's checked implicitly by heap_lock_tuple()), it's visible to any
	 * snapshot in this backend, and can be used as insertion target. We can't
	 * simply call GetTransactionSnapshot() here because it will create a new
	 * distributed snapshot for non-serializable transaction isolation level,
	 * and it may be too late.
	 */
	snapshot = GetOldestSnapshot();
	if (snapshot == NULL)
		snapshot = GetTransactionSnapshot();

	if (Debug_appendonly_print_segfile_choice)
	{
		elog(LOG, "usedByConcurrentTransaction: TransactionXmin = %u, xmin = %u, xmax = %u, myxid = %u",
			 TransactionXmin, snapshot->xmin, snapshot->xmax, GetCurrentTransactionIdIfAny());
		LogDistributedSnapshotInfo(snapshot, "Used snapshot: ");
	}

	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, snapshot, 0, NULL);
	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		int32		this_segno;
		int64		tupcount;
		int16		state;
		int16		formatversion;

		get_aoseg_fields(rel, pg_aoseg_rel, tuple,
						 &this_segno, &tupcount, &state, &formatversion);

		if (segno != this_segno)
			continue;

		if (state != AOSEG_STATE_DEFAULT)
			elog(ERROR, "segfile %d is in unexpected state %d", segno, state);

		/* If the ao segment is full, can't use it */
		if (tupcount > segfileMaxRowThreshold())
			elog(ERROR, "segfile %d is full", segno);

		/* Skip using the ao segment if not latest version (except as a compaction target) */
		if (formatversion != AORelationVersion_GetLatest())
			elog(ERROR, "segfile %d is not of the latest version", segno);

		found = true;
		break;
	}

	if (!found)
	{
		/* create it! */
		if (RelationIsAoRows(rel))
			InsertInitialSegnoEntry(rel, segno);
		else
			InsertInitialAOCSFileSegInfo(rel, segno,
										 RelationGetNumberOfAttributes(rel), segrelid);

		/* the tuple was locked by InsertInitial already */
	}
	/*
	 * If we have already used this segment in this transaction, no need
	 * to look further. We can continue to use it. We should already hold
	 * a tuple lock on the pg_aoseg row, too.
	 */
	else if (HeapTupleHeaderGetXmin(tuple->t_data) == GetCurrentTransactionId())
	{
	}
	else
	{
		/* this segno is available and not full. Try to lock it. */
		HeapTupleData locktup;
		Buffer		buf = InvalidBuffer;
		TM_FailureData hufd;
		TM_Result result;

		locktup.t_self = tuple->t_self;
		result = heap_lock_tuple(pg_aoseg_rel, &locktup,
								 GetCurrentCommandId(true),
								 LockTupleExclusive,
								 LockWaitSkip,
								 false, /* follow_updates */
								 &buf,
								 &hufd);
		if (BufferIsValid(buf))
			ReleaseBuffer(buf);
		if (result != TM_Ok)
			elog(ERROR, "could not lock segfile %d", segno);
	}

	/* OK, we have the aoseg tuple locked for us. */
	systable_endscan(aoscan);

	UnlockRelationForExtension(rel, ExclusiveLock);

	heap_close(pg_aoseg_rel, AccessShareLock);

	UnregisterSnapshot(appendOnlyMetaDataSnapshot);

	/* success! */
}

int
ChooseSegnoForWrite(Relation rel)
{
	int		chosen_segno;

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("ChooseSegnoForWrite: Choosing a segfile for relation \"%s\"",
						RelationGetRelationName(rel))));

	chosen_segno = choose_segno_internal(rel, NIL, CHOOSE_MODE_WRITE);

	if (chosen_segno == -1)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 (errmsg("could not find segment file to use for inserting into relation \"%s\"",
						 RelationGetRelationName(rel)))));
	return chosen_segno;
}

/*
 * Select a segfile to write surviving tuples to, when doing VACUUM compaction.
 */
int
ChooseSegnoForCompactionWrite(Relation rel, List *avoid_segnos)
{
	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("ChooseSegnoForCompactionWrite: Choosing a segfile for relation \"%s\"",
						RelationGetRelationName(rel))));

	return choose_segno_internal(rel, avoid_segnos, CHOOSE_MODE_COMPACTION_WRITE);
}

/*
 * Select a segfile to compact, during VACUUM.
 */
int
ChooseSegnoForCompaction(Relation rel, List *avoid_segnos)
{
	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("ChooseSegnoForCompaction: Choosing a segfile to compact in relation \"%s\"",
						RelationGetRelationName(rel))));

	return choose_segno_internal(rel, avoid_segnos, CHOOSE_MODE_COMPACTION_TARGET);
}

/*
 * Reserved segno is special: it is inserted as a regular tuple (not frozen)
 * in gp_fastsequence to leverage MVCC for cleanup in case of abort.  Reserved
 * segno should be chosen for insert when the insert command is part of the
 * same transaction that created the table.  See
 * InsertInitialFastSequenceEntries for more details.
 */
static bool
ShouldUseReservedSegno(Relation rel, choose_segno_mode mode)
{
	Relation pg_class;
	ScanKeyData scankey[1];
	SysScanDesc scan;
	HeapTuple tuple;
	TransactionId xmin;

	/*
	 * Reserved segno can only be chosen for non-vacuum cases because vacuum
	 * cannot be executed from inside a transaction.
	 */
	if (mode != CHOOSE_MODE_WRITE)
		return false;

	ScanKeyInit(&scankey[0],
				Anum_pg_class_oid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	pg_class = table_open(RelationRelationId, AccessShareLock);
	scan = systable_beginscan(pg_class, ClassOidIndexId, true,
							  NULL, 1, scankey);
	tuple = systable_getnext(scan);
	if (!tuple)
		elog(ERROR, "unable to find relation entry in pg_class for %s",
			 RelationGetRelationName(rel));
	
	xmin = HeapTupleHeaderGetXmin(tuple->t_data);
	systable_endscan(scan);
	table_close(pg_class, NoLock);

	return TransactionIdIsCurrentTransactionId(xmin);
}


/*
 * Decide which segment number should be used to write into during the COPY,
 * INSERT, or VACUUM operation we're executing. This contains the common
 * logic for all three ChooseSegno* variants.
 *
 * The rules for which segfiles can be selected and which ones are preferred
 * depend on the mode:
 *
 * - In WRITE mode, pick any existing segment, preferring tuples with lower
 *   tupcount. If they're all in use, create a new one.
 *
 * - In COMPACTION_WRITE mode, prefer existing segments with tupcount=0. If
 *   none are available, create a new segfile. If a new segfile cannot be
 *   created either, then reuse an existing segfile with non-zero tupcount.
 *
 * - In COMPACTION_TARGET mode, only existings segments with non-zero tupcount
 *   are chosen.
 *
 * If 'avoid_segnos' is non-empty, we will not choose any of those segments as
 * the target.
 *
 * The return value is a segment file number to use for inserting by each
 * segdb into its local AO table. It can be -1 no suitable existing segfile
 * was found and a new one could not be created either. The returned segfile
 * is locked for this transaction.
 */
static int
choose_segno_internal(Relation rel, List *avoid_segnos, choose_segno_mode mode)
{
	Relation	pg_aoseg_rel;
	TupleDesc	pg_aoseg_dsc;
	int			i;
	int32		chosen_segno = -1;
	candidate_segment candidates[MAX_AOREL_CONCURRENCY];
	bool		used[MAX_AOREL_CONCURRENCY];
	int			ncandidates = 0;
	SysScanDesc aoscan;
	HeapTuple	tuple;
	Snapshot	snapshot;
	Oid			segrelid;
	bool		tried_creating_new_segfile = false;

	memset(used, 0, sizeof(used));

	if (ShouldUseReservedSegno(rel, mode))
	{
		Assert(avoid_segnos == NIL);
		if (Debug_appendonly_print_segfile_choice)
			elog(LOG, "choose_segno_internal: chose RESERVED_SEGNO for wrie");

		LockSegnoForWrite(rel, RESERVED_SEGNO);
		return RESERVED_SEGNO;
	}

	/*
	 * The algorithm below for choosing a target segment is not concurrent-safe.
	 * Grab a lock to serialize.
	 */
	LockRelationForExtension(rel, ExclusiveLock);

	/*
	 * Obtain the snapshot that is taken at the beginning of the transaction.
	 * If a tuple is visible to this snapshot, and it hasn't been updated since
	 * (that's checked implicitly by heap_lock_tuple()), it's visible to any
	 * snapshot in this backend, and can be used as insertion target. We can't
	 * simply call GetTransactionSnapshot() here because it will create a new
	 * distributed snapshot for non-serializable transaction isolation level,
	 * and it may be too late.
	 */
	snapshot = GetOldestSnapshot();
	if (snapshot == NULL)
		snapshot = GetTransactionSnapshot();

	if (Debug_appendonly_print_segfile_choice)
	{
		elog(LOG, "choose_segno_internal: TransactionXmin = %u, xmin = %u, xmax = %u, myxid = %u",
			 TransactionXmin, snapshot->xmin, snapshot->xmax, GetCurrentTransactionIdIfAny());
		LogDistributedSnapshotInfo(snapshot, "Used snapshot: ");
	}

	GetAppendOnlyEntryAuxOids(rel->rd_id, NULL,
							  &segrelid, NULL, NULL, NULL, NULL);

	/*
	 * Now pick a segment that is not in use, and is not over the allowed
	 * size threshold (90% full).
	 */
	pg_aoseg_rel = heap_open(segrelid, AccessShareLock);
	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);

	/*
	 * Scan through all the pg_aoseg (or pg_aocs) entries, and make note of
	 * all "candidates".
	 */
	aoscan = systable_beginscan(pg_aoseg_rel, InvalidOid, false, snapshot, 0, NULL);
	while ((tuple = systable_getnext(aoscan)) != NULL)
	{
		int32		segno;
		int64		tupcount;
		int16		state;
		int16		formatversion;

		get_aoseg_fields(rel, pg_aoseg_rel, tuple, &segno,
						 &tupcount, &state, &formatversion);

		used[segno] = true;

		/* never write to AWAITING_DROP segments */
		if (state != AOSEG_STATE_DEFAULT)
			continue;

		/* skip over segfiles that the caller asked to avoid */
		if (list_member_int(avoid_segnos, segno))
			continue;

		if (mode != CHOOSE_MODE_COMPACTION_TARGET)
		{
			/* If the ao segment is full, skip it */
			if (tupcount > segfileMaxRowThreshold())
				continue;

			/* Skip using the ao segment if not latest version (except as a compaction target) */
			if (formatversion != AORelationVersion_GetLatest())
				continue;

			/*
			 * Historically, segment 0 was only used in utility mode.
			 * Nowadays, segment 0 is also used for CTAS and alter table
			 * rewrite commands.
			 */
			if (Gp_role != GP_ROLE_UTILITY && segno == RESERVED_SEGNO)
				continue;

			/*
			 * If we have already used this segment in this transaction, no need
			 * to look further. We can continue to use it. We should already hold
			 * a tuple lock on the pg_aoseg row, too.
			 */
			if (HeapTupleHeaderGetXmin(tuple->t_data) == GetCurrentTransactionId())
			{
				chosen_segno = segno;

				if (Debug_appendonly_print_segfile_choice)
					elog(LOG, "choose_segno_interna: chose segfile %d because it was updated earlier in the transaction already",
						 chosen_segno);
				break;
			}
		}
		else
		{
			/* If the ao segment is empty, do not choose it for compaction */
			if (tupcount == 0)
				continue;
		}

		candidates[ncandidates].segno = segno;
		candidates[ncandidates].ctid = tuple->t_self;
		candidates[ncandidates].tupcount = tupcount;
		ncandidates++;
	}
	systable_endscan(aoscan);

	/*
	 * Try to find a segment we can use among the candidates, and lock it.
	 */
	if (chosen_segno == -1)
	{

		/*
		 * Sort the candidates by tuple count, to prefer segment with fewest existing
		 * tuples. (In particular, in COMPACTION_WRITE mode, this puts all empty
		 * segfiles to the front).
		 */
		qsort((void *) candidates, ncandidates, sizeof(candidate_segment),
			  compare_candidates);

		for (i = 0; i < ncandidates; i++)
		{
			HeapTupleData locktup;
			Buffer		buf = InvalidBuffer;
			TM_FailureData hufd;
			TM_Result result;

			/*
			 * When performing VACUUM compaction, we prefer to create a new segment
			 * over reusing a non-empty segfile, as the target to write the surviving
			 * tuples to. Because if we insert to a non-empty segfile, we won't be
			 * able to compact it later in the VACUUM cycle. (Or if we do, we'll scan
			 * through all the tuples we moved onto it earlier.) So before we proceed
			 * to try locking any non-empty segments, try to create a new one.
			 */
			if (mode == CHOOSE_MODE_COMPACTION_WRITE &&
				!tried_creating_new_segfile &&
				candidates[i].tupcount > 0)
			{
				chosen_segno = choose_new_segfile(rel, used, avoid_segnos);
				tried_creating_new_segfile = true;
				if (chosen_segno != -1)
					break;
			}

			locktup.t_self = candidates[i].ctid;
			result = heap_lock_tuple(pg_aoseg_rel, &locktup,
									 GetCurrentCommandId(true),
									 LockTupleExclusive,
									 LockWaitSkip,
									 false, /* follow_updates */
									 &buf,
									 &hufd);
			if (BufferIsValid(buf))
				ReleaseBuffer(buf);
			if (result == TM_Ok)
			{
				chosen_segno = candidates[i].segno;
				if (Debug_appendonly_print_segfile_choice)
					elog(LOG, "choose_segno_internal: locked existing segfile %d", chosen_segno);
				break;
			}
			else
			{
				if (Debug_appendonly_print_segfile_choice)
					elog(LOG, "choose_segno_internal: skipped segfile %d because could not be locked",
						 candidates[i].segno);
			}
		}
	}

	/*
	 * If no existing segment could be used, create a new one.
	 */
	if (chosen_segno == -1 &&
		mode != CHOOSE_MODE_COMPACTION_TARGET &&
		!tried_creating_new_segfile)
	{
		chosen_segno = choose_new_segfile(rel, used, avoid_segnos);
	}

	UnlockRelationForExtension(rel, ExclusiveLock);

	if (Debug_appendonly_print_segfile_choice && chosen_segno != -1)
		ereport(LOG,
				(errmsg("Segno chosen for append-only relation \"%s\" is %d",
						RelationGetRelationName(rel), chosen_segno)));

	heap_close(pg_aoseg_rel, AccessShareLock);

	return chosen_segno;
}

static int
choose_new_segfile(Relation rel, bool *used, List *avoid_segnos)
{
	int		chosen_segno = -1;

	/* No segment found. Try to create a new one. */
	for (int segno = 0; segno < MAX_AOREL_CONCURRENCY; segno++)
	{
		/* Only choose seg 0 in utility mode. See above. */
		if (Gp_role != GP_ROLE_UTILITY && segno == 0)
			continue;

		if (!used[segno] && !list_member_int(avoid_segnos, segno))
		{
			chosen_segno = segno;
			break;
		}
	}

	/* If can't create a new one because MAX_AOREL_CONCURRENCY was reached */
	if (chosen_segno != -1)
	{
		if (Debug_appendonly_print_segfile_choice)
			elog(LOG, "choose_new_segfile: creating new segfile %d",
				 chosen_segno);

		if (RelationIsAoRows(rel))
			InsertInitialSegnoEntry(rel, chosen_segno);
		else
		{
			Oid segrelid;
			Snapshot appendOnlyMetaDataSnapshot;

			appendOnlyMetaDataSnapshot = RegisterSnapshot(GetCatalogSnapshot(InvalidOid));
			GetAppendOnlyEntryAuxOids(rel->rd_id, appendOnlyMetaDataSnapshot,
									  &segrelid, NULL, NULL, NULL, NULL);
			UnregisterSnapshot(appendOnlyMetaDataSnapshot);

			InsertInitialAOCSFileSegInfo(rel, chosen_segno,
										 RelationGetNumberOfAttributes(rel), segrelid);
		}
	}
	else
	{
		if (Debug_appendonly_print_segfile_choice)
			elog(LOG, "choose_new_segfile: could not create segfile, all segfiles are in use");
	}

	return chosen_segno;
}

/*
 * Helper function to extract 'segno', 'tupcount', 'state', and 'formatversion'
 * from a pg_aoseg or pg_aocs tuple.
 */
static void
get_aoseg_fields(Relation rel, Relation pg_aoseg_rel, HeapTuple tuple,
				 int32 *segno, int64 *tupcount, int16 *state, int16 *formatversion)
{
	TupleDesc	pg_aoseg_dsc = RelationGetDescr(pg_aoseg_rel);
	bool		isNull;

	if (RelationIsAoRows(rel))
	{
		*segno = DatumGetInt32(fastgetattr(tuple,
										   Anum_pg_aoseg_segno,
										   pg_aoseg_dsc, &isNull));
		Assert(!isNull);

		*tupcount = DatumGetInt64(fastgetattr(tuple,
											  Anum_pg_aoseg_tupcount,
											  pg_aoseg_dsc, &isNull));
		Assert(!isNull);

		*state = DatumGetInt16(fastgetattr(tuple,
										   Anum_pg_aoseg_state,
										   pg_aoseg_dsc, &isNull));
		Assert(!isNull);

		*formatversion = DatumGetInt16(fastgetattr(tuple,
												   Anum_pg_aoseg_formatversion,
												   pg_aoseg_dsc, &isNull));
		Assert(!isNull);
	}
	else
	{
		*segno = DatumGetInt32(fastgetattr(tuple,
										   Anum_pg_aocs_segno,
										   pg_aoseg_dsc, &isNull));
		Assert(!isNull);
		*tupcount = DatumGetInt64(fastgetattr(tuple,
											  Anum_pg_aocs_tupcount,
											  pg_aoseg_dsc, &isNull));
		Assert(!isNull);

		*state = DatumGetInt16(fastgetattr(tuple,
										   Anum_pg_aocs_state,
										   pg_aoseg_dsc, &isNull));
		Assert(!isNull);

		*formatversion = DatumGetInt16(fastgetattr(tuple,
												   Anum_pg_aocs_formatversion,
												   pg_aoseg_dsc, &isNull));
		Assert(!isNull);
	}
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
				(errmsg("AORelIncrementModCount: Incrementing modcount of aoseg entry for append-only relation %d",
						RelationGetRelid(parentrel))));

	/*
	 * It doesn't matter which segment we use, as long as the segment can be used by us
	 * (same rules as for inserting).
	 */
	segno = ChooseSegnoForWrite(parentrel);

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
