/*-------------------------------------------------------------------------
 *
 * appendonlywriter.c
 *	  routines to support AO concurrent writes via shared memory structures.
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
#include "access/appendonlytid.h"	/* AOTupleId_MaxRowNum  */
#include "access/appendonlywriter.h"
#include "access/aocssegfiles.h"	/* AOCS */
#include "access/heapam.h"		/* heap_open            */
#include "access/transam.h"		/* InvalidTransactionId */
#include "access/xact.h"
#include "catalog/pg_authid.h"
#include "gp-libpq-fe.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"

#include "cdb/cdbvars.h"		/* Gp_role              */
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbutil.h"
#include "pg_config.h"

/*
 * GUC variables
 */
int			MaxAppendOnlyTables;	/* Max # of tables */

/*
 * Global Variables
 */
static HTAB *AppendOnlyHash;	/* Hash of AO tables */
AppendOnlyWriterData *AppendOnlyWriter;
static TransactionId appendOnlyInsertXact = InvalidTransactionId;

/*
 * local functions
 */
static bool AOHashTableInit(void);
static AORelHashEntry AppendOnlyRelHashNew(Oid relid, bool *exists);
static AORelHashEntry AORelGetHashEntry(Oid relid);
static AORelHashEntry AORelLookupHashEntry(Oid relid);
static bool AORelCreateHashEntry(Oid relid);

/*
 * AppendOnlyWriterShmemSize -- estimate size the append only writer structures
 * will need in shared memory.
 *
 */
Size
AppendOnlyWriterShmemSize(void)
{
	Size		size;

	/* The hash of append only relations */
	size = hash_estimate_size((Size) MaxAppendOnlyTables,
							  sizeof(AORelHashEntryData));

	/* The writer structure. */
	size = add_size(size, sizeof(AppendOnlyWriterData));

	/* Add a safety margin */
	size = add_size(size, size / 10);

	return size;
}

/*
 * InitAppendOnlyWriter -- initialize the AO writer hash in shared memory.
 *
 * The AppendOnlyHash table has no data in it as yet.
 */
void
InitAppendOnlyWriter(void)
{
	bool		found;
	bool		ok;

	/* Create the writer structure. */
	AppendOnlyWriter = (AppendOnlyWriterData *)
		ShmemInitStruct("Append Only Writer Data",
						sizeof(AppendOnlyWriterData),
						&found);

	if (found && AppendOnlyHash)
		return;					/* We are already initialized and have a hash
								 * table */

	/* Specify that we have no AO rel information yet. */
	AppendOnlyWriter->num_existing_aorels = 0;

	/* Create AppendOnlyHash (empty at this point). */
	ok = AOHashTableInit();
	if (!ok)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("not enough shared memory for append only writer")));

	ereport(DEBUG1, (errmsg("initialized append only writer")));

	return;
}

/*
 * AOHashTableInit
 *
 * initialize the hash table of AO rels and their fileseg entries.
 */
static bool
AOHashTableInit(void)
{
	HASHCTL		info;
	int			hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(Oid);
	info.entrysize = sizeof(AORelHashEntryData);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	AppendOnlyHash = ShmemInitHash("Append Only Hash",
								   MaxAppendOnlyTables,
								   MaxAppendOnlyTables,
								   &info,
								   hash_flags);

	if (!AppendOnlyHash)
		return false;

	return true;
}


/*
 * AORelCreateEntry -- initialize the elements for an AO relation entry.
 *					   if an entry already exists, return successfully.
 * Notes:
 *	It is expected that the appropriate lightweight lock is held before
 *	calling this - unless we are the startup process.
 */
static bool
AORelCreateHashEntry(Oid relid)
{
	bool		exists = false;
	FileSegInfo **allfsinfo = NULL;
	AOCSFileSegInfo **aocsallfsinfo = NULL;
	int			i;
	int			total_segfiles = 0;
	AORelHashEntry aoHashEntry = NULL;
	Relation	aorel;

	/*
	 * Momentarily release the AOSegFileLock so we can safely access the
	 * system catalog (i.e. without risking a deadlock).
	 */
	LWLockRelease(AOSegFileLock);

	/*
	 * Now get all the segment files information for this relation from the QD
	 * aoseg table. then update our segment file array in this hash entry.
	 */
	aorel = heap_open(relid, RowExclusiveLock);

	/*
	 * Use SnapshotNow since we have an exclusive lock on the relation.
	 */
	if (RelationIsAoRows(aorel))
	{
		allfsinfo = GetAllFileSegInfo(aorel, SnapshotNow, &total_segfiles);
	}
	else
	{
		Assert(RelationIsAoCols(aorel));
		aocsallfsinfo = GetAllAOCSFileSegInfo(aorel, SnapshotNow, &total_segfiles);
	}

	heap_close(aorel, RowExclusiveLock);

	/*
	 * Re-acquire AOSegFileLock.
	 *
	 * Note: Another session may have raced-in and created it.
	 */
	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoHashEntry = AppendOnlyRelHashNew(relid, &exists);

	/* Does an entry for this relation exists already? exit early */
	if (exists)
	{
		if (allfsinfo)
		{
			FreeAllSegFileInfo(allfsinfo, total_segfiles);
			pfree(allfsinfo);
		}
		if (aocsallfsinfo)
		{
			FreeAllAOCSSegFileInfo(aocsallfsinfo, total_segfiles);
			pfree(aocsallfsinfo);
		}
		return true;
	}

	/*
	 * If the new aoHashEntry pointer is NULL, then we are out of allowed # of
	 * AO tables used for write concurrently, and don't have any unused
	 * entries to free
	 */
	if (!aoHashEntry)
	{
		Assert(AppendOnlyWriter->num_existing_aorels >= MaxAppendOnlyTables);
		return false;
	}


	Insist(aoHashEntry->relid == relid);
	aoHashEntry->txns_using_rel = 0;

	/*
	 * Initialize all segfile array to zero
	 */
	for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
	{
		aoHashEntry->relsegfiles[i].state = AVAILABLE;
		aoHashEntry->relsegfiles[i].xid = InvalidTransactionId;
		aoHashEntry->relsegfiles[i].latestWriteXid = FrozenTransactionId;
		aoHashEntry->relsegfiles[i].isfull = false;
		aoHashEntry->relsegfiles[i].total_tupcount = 0;
		aoHashEntry->relsegfiles[i].tupsadded = 0;
		aoHashEntry->relsegfiles[i].aborted = false;
		aoHashEntry->relsegfiles[i].formatversion = AORelationVersion_GetLatest();
	}

	/*
	 * update the tupcount and formatVersion of each 'segment' file in the
	 * append only hash according to the information in the pg_aoseg table.
	 */
	for (i = 0; i < total_segfiles; i++)
	{
		int			segno;
		int64		total_tupcount;
		int16		formatversion;

		if (allfsinfo)
		{
			segno = allfsinfo[i]->segno;
			total_tupcount = allfsinfo[i]->total_tupcount;
			formatversion = allfsinfo[i]->formatversion;
		}
		else
		{
			Assert(aocsallfsinfo);
			segno = aocsallfsinfo[i]->segno;
			total_tupcount = aocsallfsinfo[i]->total_tupcount;
			formatversion = aocsallfsinfo[i]->formatversion;
		}

		aoHashEntry->relsegfiles[segno].total_tupcount = total_tupcount;
		aoHashEntry->relsegfiles[segno].formatversion = formatversion;
	}

	/* record the fact that another hash entry is now taken */
	AppendOnlyWriter->num_existing_aorels++;

	if (allfsinfo)
	{
		FreeAllSegFileInfo(allfsinfo, total_segfiles);
		pfree(allfsinfo);
	}
	if (aocsallfsinfo)
	{
		FreeAllAOCSSegFileInfo(aocsallfsinfo, total_segfiles);
		pfree(aocsallfsinfo);
	}

	return true;
}

/*
 * AORelRemoveEntry -- remove the hash entry for a given relation.
 *
 * Notes
 *	The append only lightweight lock (AOSegFileLock) *must* be held for
 *	this operation.
 */
bool
AORelRemoveHashEntry(Oid relid)
{
	bool		found;
	void	   *aoentry;

	aoentry = hash_search(AppendOnlyHash,
						  (void *) &relid,
						  HASH_REMOVE,
						  &found);

	if (aoentry == NULL)
		return false;

	AppendOnlyWriter->num_existing_aorels--;

	return true;
}

/*
 * AORelLookupEntry -- return the AO hash entry for a given AO relation,
 *					   it exists.
 */
static AORelHashEntry
AORelLookupHashEntry(Oid relid)
{
	bool		found;

	return (AORelHashEntry) hash_search(AppendOnlyHash,
										(void *) &relid,
										HASH_FIND,
										&found);
}

/*
 * AORelGetEntry -- Same as AORelLookupEntry but here the caller is expecting
 *					an entry to exist. We error out if it doesn't.
 */
static AORelHashEntry
AORelGetHashEntry(Oid relid)
{
	AORelHashEntryData *aoentry = AORelLookupHashEntry(relid);

	if (!aoentry)
		ereport(ERROR,
				(errmsg("expected an AO hash entry for relid %d but found none",
						relid)));

	return (AORelHashEntry) aoentry;
}

/*
 * Gets or creates the AORelHashEntry.
 *
 * Assumes that the AOSegFileLock is acquired.
 * The AOSegFileLock will still be acquired when this function returns, expect
 * if it errors out.
 */
static AORelHashEntryData *
AORelGetOrCreateHashEntry(Oid relid)
{

	AORelHashEntryData *aoentry = NULL;

	aoentry = AORelLookupHashEntry(relid);
	if (aoentry != NULL)
	{
		return aoentry;
	}

	/*
	 * We need to create a hash entry for this relation.
	 *
	 * However, we need to access the pg_appendonly system catalog table, so
	 * AORelCreateHashEntry will carefully release the AOSegFileLock, gather
	 * the information, and then re-acquire AOSegFileLock.
	 */
	if (!AORelCreateHashEntry(relid))
	{
		LWLockRelease(AOSegFileLock);
		ereport(ERROR, (errmsg("can't have more than %d different append-only "
							   "tables open for writing data at the same time. "
							   "if tables are heavily partitioned or if your "
							   "workload requires, increase the value of "
							   "max_appendonly_tables and retry",
							   MaxAppendOnlyTables)));
	}

	/* get the hash entry for this relation (must exist) */
	aoentry = AORelGetHashEntry(relid);
	return aoentry;
}

/*
 * AppendOnlyRelHashNew -- return a new (empty) aorel hash object to initialize.
 *
 * Notes
 *	The appendonly lightweight lock (AOSegFileLock) *must* be held for
 *	this operation.
 */
static AORelHashEntry
AppendOnlyRelHashNew(Oid relid, bool *exists)
{
	AORelHashEntryData *aorelentry = NULL;

	/*
	 * We do not want to exceed the max number of allowed entries since we
	 * don't drop entries when we're done with them (so we could reuse them).
	 * Note that the hash table will allow going past max_size (it's not a
	 * hard limit) so we do the check ourselves.
	 *
	 * Therefore, check for this condition first and deal with it if exists.
	 */
	if (AppendOnlyWriter->num_existing_aorels >= MaxAppendOnlyTables)
	{
		/* see if there's already an existing entry for this relid */
		aorelentry = AORelLookupHashEntry(relid);

		/*
		 * already have an entry for this rel? reuse it. don't have? try to
		 * drop an unused entry and create our entry there
		 */
		if (aorelentry)
		{
			*exists = true;		/* reuse */
			return NULL;
		}
		else
		{
			HASH_SEQ_STATUS status;
			AORelHashEntryData *hentry;

			Assert(AppendOnlyWriter->num_existing_aorels == MaxAppendOnlyTables);

			/*
			 * loop through all entries looking for an unused one
			 */
			hash_seq_init(&status, AppendOnlyHash);

			while ((hentry = (AORelHashEntryData *) hash_seq_search(&status)) != NULL)
			{
				if (hentry->txns_using_rel == 0)
				{
					/* remove this unused entry */
					/* TODO: remove the LRU entry, not just any unused one */
					AORelRemoveHashEntry(hentry->relid);
					hash_seq_term(&status);

					/* we now have room for a new entry, create it */
					aorelentry = (AORelHashEntryData *) hash_search(AppendOnlyHash,
																	(void *) &relid,
																	HASH_ENTER_NULL,
																	exists);

					Insist(aorelentry);
					return (AORelHashEntry) aorelentry;
				}

			}

			/*
			 * No room for a new entry. give up.
			 */
			return NULL;
		}

	}

	/*
	 * We don't yet have a full hash table. Create a new entry if not exists
	 */
	return (AORelHashEntry) hash_search(AppendOnlyHash,
										(void *) &relid,
										HASH_ENTER_NULL,
										exists);
}

#define SEGFILE_CAPACITY_THRESHOLD	0.9

/*
 * segfileMaxRowThreshold
 *
 * Returns the row count threshold - when a segfile more than this number of
 * rows we don't allow inserting more data into it anymore.
 */
static
int64
segfileMaxRowThreshold(void)
{
	int64		maxallowed = (int64) AOTupleId_MaxRowNum;

	if (maxallowed < 0)
		ereport(ERROR,
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				 errmsg("int64 out of range")));

	return (SEGFILE_CAPACITY_THRESHOLD * maxallowed);
}

/*
 * usedByConcurrentTransaction
 *
 * Return true if segno has been used by any transactions
 * that are running concurrently with the current transaction.
 */
static bool
usedByConcurrentTransaction(AOSegfileStatus *segfilestat, int segno)
{
	TransactionId latestWriteXid = segfilestat->latestWriteXid;
	Snapshot	snapshot;
	int			i;

	/*
	 * Obtain the snapshot that is taken at the beginning of the transaction.
	 * We can't simply call GetTransactionSnapshot() here because it will
	 * create a new distributed snapshot for non-serializable transaction
	 * isolation level, and it may be too late.
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

void
RegisterSegnoForCompactionDrop(Oid relid, int compacted_segno)
{
	TransactionId CurrentXid = GetTopTransactionId();
	AORelHashEntryData *aoentry;
	AOSegfileStatus *segfilestat;

	Assert(compacted_segno >= 0);
	Assert(compacted_segno < MAX_AOREL_CONCURRENCY);

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(relid);
	Assert(aoentry);
	aoentry->txns_using_rel++;

	segfilestat = &aoentry->relsegfiles[compacted_segno];

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("Register segno %d for drop "
						"relation \"%s\" (%d)", compacted_segno,
						get_rel_name(relid), relid)));

	Assert(segfilestat->state == COMPACTED_AWAITING_DROP);
	segfilestat->xid = CurrentXid;
	Assert(appendOnlyInsertXact == InvalidOid || appendOnlyInsertXact == CurrentXid);
	appendOnlyInsertXact = CurrentXid;
	segfilestat->state = DROP_USE;

	LWLockRelease(AOSegFileLock);
	return;
}

/*
 * SetSegnoForCompaction
 *
 * This function determines which segment to use for the next
 * compaction run.
 *
 * If -1 (APPENDONLY_COMPACTION_SEGNO_INVALID) is returned, no segment
 * should be compacted. This usually means that all segments are clean
 * or empty.
 */
int
SetSegnoForCompaction(Relation rel,
					  List *compactedSegmentFileList,
					  List *insertedSegmentFileList,
					  bool *is_drop)
{
	TransactionId CurrentXid = GetTopTransactionId();
	int			usesegno;
	int			i;
	AORelHashEntryData *aoentry;

	*is_drop = false;

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("SetSegnoForCompaction: "
						"Choosing a compaction segno for append-only "
						"relation \"%s\" (%d)",
				RelationGetRelationName(rel), RelationGetRelid(rel))));

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(RelationGetRelid(rel));
	Assert(aoentry);

	/* First: Always check if some segment is awaiting a drop */
	usesegno = APPENDONLY_COMPACTION_SEGNO_INVALID;
	for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];

		if ((segfilestat->state == AWAITING_DROP_READY ||
			 segfilestat->state == COMPACTED_AWAITING_DROP) &&
			!usedByConcurrentTransaction(segfilestat, i))
		{
			if (Debug_appendonly_print_segfile_choice)
				ereport(LOG,
						(errmsg("Found segment awaiting drop for append-only relation \"%s\" (%d)",
						RelationGetRelationName(rel), RelationGetRelid(rel))));

			*is_drop = true;
			usesegno = i;
			break;
		}
	}

	if (usesegno <= 0)
	{
		for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
		{
			AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
			bool		in_compaction_list = list_member_int(compactedSegmentFileList, i);
			bool		in_inserted_list = list_member_int(insertedSegmentFileList, i);

			/*
			 * Select when there are tuples and it is available.
			 */
			if (segfilestat->total_tupcount > 0 &&
				segfilestat->state == AVAILABLE &&
				!usedByConcurrentTransaction(segfilestat, i) &&
				!in_compaction_list &&
				!in_inserted_list)
			{
				usesegno = i;
				break;
			}
		}
	}

	if (usesegno != APPENDONLY_COMPACTION_SEGNO_INVALID)
	{
		/* mark this segno as in use */
		if (aoentry->relsegfiles[usesegno].xid != CurrentXid)
		{
			aoentry->txns_using_rel++;
		}
		if (*is_drop)
		{
			aoentry->relsegfiles[usesegno].state = PSEUDO_COMPACTION_USE;
		}
		else
		{
			aoentry->relsegfiles[usesegno].state = COMPACTION_USE;

		}
		aoentry->relsegfiles[usesegno].xid = CurrentXid;
		Assert(appendOnlyInsertXact == InvalidOid || appendOnlyInsertXact == CurrentXid);
		appendOnlyInsertXact = CurrentXid;

		if (Debug_appendonly_print_segfile_choice)
			ereport(LOG,
					(errmsg("Compaction segment chosen for append-only relation \"%s\" (%d) "
							"is %d (tupcount " INT64_FORMAT ", txns count %d)",
							RelationGetRelationName(rel), RelationGetRelid(rel), usesegno,
							aoentry->relsegfiles[usesegno].total_tupcount,
							aoentry->txns_using_rel)));
	}
	else
	{
		if (Debug_appendonly_print_segfile_choice)
			ereport(LOG,
					(errmsg("No compaction segment chosen for append-only relation \"%s\" (%d)",
							RelationGetRelationName(rel), RelationGetRelid(rel))));
	}

	LWLockRelease(AOSegFileLock);

	Assert(usesegno >= 0 || usesegno == APPENDONLY_COMPACTION_SEGNO_INVALID);

	return usesegno;
}

/*
 * SetSegnoForCompactionInsert
 *
 * This function determines into which segment a compaction
 * run should write into.
 *
 * Currently, it always selects a non-used, least-filled segment.
 *
 * Note that this code does not manipulate aoentry->txns_using_rel
 * as it has before been set by SetSegnoForCompaction.
 */ 
int
SetSegnoForCompactionInsert(Relation rel,
							int compacted_segno,
							List *compactedSegmentFileList,
							List *insertedSegmentFileList)
{
	int			i,
				usesegno = -1;
	bool		segno_chosen = false;
	AORelHashEntryData *aoentry = NULL;
	TransactionId CurrentXid = GetTopTransactionId();
	int64		min_tupcount;

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("SetSegnoForCompactionInsert: "
						"Choosing a segno for append-only relation \"%s\" (%d)", 
						RelationGetRelationName(rel), RelationGetRelid(rel))));

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(RelationGetRelid(rel));
	Assert(aoentry);

	min_tupcount = INT64_MAX;
	usesegno = 0;

	/* 
	 * Never use segno 0 for inserts (it used to be used in utility mode in
	 * GPDB version 5 and below, but not anymore. )
	 */
	for (i = 1; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
		bool		in_compaction_list = (compacted_segno == i) ||
			list_member_int(compactedSegmentFileList, i);

		if (segfilestat->total_tupcount < min_tupcount &&
			segfilestat->state == AVAILABLE &&
			segfilestat->formatversion == AORelationVersion_GetLatest() &&
			!usedByConcurrentTransaction(segfilestat, i) &&
			!in_compaction_list)
		{
			min_tupcount = segfilestat->total_tupcount;
			usesegno = i;
			segno_chosen = true;
		}

		if (!in_compaction_list && segfilestat->xid == CurrentXid)
		{
			/* we already used this segno in our txn. use it again */
			usesegno = i;
			segno_chosen = true;

			if (Debug_appendonly_print_segfile_choice)
				ereport(LOG,
						(errmsg("SetSegnoForCompaction: reusing segno %d for append-only relation "
								"%u. there are " INT64_FORMAT " tuples "
								"added to it from previous operations "
								"in this not yet committed txn.",
								usesegno, RelationGetRelid(rel),
								(int64) segfilestat->tupsadded)));
			break;
		}
	}

	if (!segno_chosen)
	{
		LWLockRelease(AOSegFileLock);
		ereport(ERROR, (errmsg("could not find segment file to use for "
							   "inserting into relation %s (%d).",
							   RelationGetRelationName(rel), RelationGetRelid(rel))));
	}

	Insist(usesegno != RESERVED_SEGNO);

	/* mark this segno as in use */
	aoentry->relsegfiles[usesegno].state = INSERT_USE;
	aoentry->relsegfiles[usesegno].xid = CurrentXid;

	LWLockRelease(AOSegFileLock);
	Assert(appendOnlyInsertXact == InvalidOid || appendOnlyInsertXact == CurrentXid);
	appendOnlyInsertXact = CurrentXid;

	Assert(usesegno >= 0);
	Assert(usesegno != RESERVED_SEGNO);

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("Segno chosen for append-only relation \"%s\" (%u) "
						"is %d", RelationGetRelationName(rel), RelationGetRelid(rel), usesegno)));

	return usesegno;
}

/*
 * SetSegnoForWrite
 *
 * This function includes the key logic of the append only writer.
 *
 * Decide which segment number should be used to write into during the
 * COPY or INSERT operation we're executing.
 *
 * The return value is a segment file number to use for inserting by each
 * segdb into its local AO table.
 *
 * It does so by looking at the in-memory hash table and selecting a segment number
 * that the most empty across the database and is also not currently used. Or, if
 * we are in an explicit transaction and inserting into the same table we use the
 * same segno over and over again.
 */
int
SetSegnoForWrite(Relation rel)
{
	int			i,
				usesegno = -1;
	bool		segno_chosen = false;
	AORelHashEntryData *aoentry = NULL;
	TransactionId CurrentXid = GetTopTransactionId();

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("SetSegnoForWrite: Choosing a segno for append-only "
						"relation \"%s\" (%d) ",
						RelationGetRelationName(rel), RelationGetRelid(rel))));

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(RelationGetRelid(rel));
	Assert(aoentry);
	aoentry->txns_using_rel++;

	/*
	 * Now pick the not in use segment and is not over the
	 * allowed size threshold (90% full).
	 *
	 * However, if we already picked a segno for a previous statement
	 * in this very same transaction we are still in (explicit txn) we
	 * pick the same one to insert into it again.
	 *
	 * Never use segno 0 for inserts (it used to be used in utility mode in
	 * GPDB version 4.3 and below, but not anymore. )
	 */
	for (i = 1 ; i < MAX_AOREL_CONCURRENCY ; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];

		if (!segfilestat->isfull)
		{
			if (segfilestat->state == AVAILABLE &&
				segfilestat->formatversion == AORelationVersion_GetLatest() &&
				!segno_chosen &&
				!usedByConcurrentTransaction(segfilestat, i))
			{
				/*
				 * this segno is available and not full. use it.
				 *
				 * Notice that we don't break out of the loop quite
				 * yet. We still need to check the rest of the segnos,
				 * if our txn is already using one of them. see below.
				 */
				usesegno = i;
				segno_chosen = true;
			}

			if (segfilestat->xid == CurrentXid)
			{
				/* we already used this segno in our txn. use it again */
				usesegno = i;
				segno_chosen = true;
				aoentry->txns_using_rel--; /* same txn. re-adjust */

				if (Debug_appendonly_print_segfile_choice)
					ereport(LOG,
							(errmsg("SetSegnoForWrite: reusing segno %d for append-"
									"only relation "
									"%d. there are " INT64_FORMAT " tuples "
									"added to it from previous operations "
									"in this not yet committed txn. decrementing"
									"txns_using_rel back to %d",
									usesegno, RelationGetRelid(rel),
									(int64) segfilestat->tupsadded,
									aoentry->txns_using_rel)));
				break;
			}
		}
	}

	if(!segno_chosen)
	{
		LWLockRelease(AOSegFileLock);
		ereport(ERROR,
				(errmsg("could not find segment file to use for inserting into relation %s (%d).",
						RelationGetRelationName(rel), RelationGetRelid(rel))));
	}

	Assert(usesegno >= 0);
	Assert(usesegno != RESERVED_SEGNO);

	/* mark this segno as in use */
	aoentry->relsegfiles[usesegno].state = INSERT_USE;
	aoentry->relsegfiles[usesegno].xid = CurrentXid;

	LWLockRelease(AOSegFileLock);
	Assert(appendOnlyInsertXact == InvalidOid || appendOnlyInsertXact == CurrentXid);
	appendOnlyInsertXact = CurrentXid;

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				(errmsg("Segno chosen for append-only relation \"%s\" (%d) "
						"is %d", RelationGetRelationName(rel), RelationGetRelid(rel), usesegno)));

	return usesegno;
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
	segno = SetSegnoForWrite(parentrel);

	/* CONSIDER: We should probably get this lock even sooner. */
	LockRelationAppendOnlySegmentFile(&parentrel->rd_node,
									  segno,
									  AccessExclusiveLock,
									   /* dontWait */ false);

	if (RelationIsAoRows(parentrel))
	{
		FileSegInfo *fsinfo;

		/* get the information for the file segment we inserted into */

		/*
		 * Since we have an exclusive lock on the segment-file entry, we can
		 * use SnapshotNow.
		 */
		fsinfo = GetFileSegInfo(parentrel, SnapshotNow, segno);
		if (fsinfo == NULL)
		{
			InsertInitialSegnoEntry(parentrel, segno);
		}
		else
		{
			pfree(fsinfo);
		}

		/*
		 * Update the master AO segment info table with correct tuple count
		 * total
		 */
		IncrementFileSegInfoModCount(parentrel, segno);
	}
	else
	{
		AOCSFileSegInfo *seginfo;

		/* AO column store */
		Assert(RelationIsAoCols(parentrel));

		seginfo = GetAOCSFileSegInfo(parentrel,
									 SnapshotNow,
									 segno);
		if (seginfo == NULL)
		{
			InsertInitialAOCSFileSegInfo(parentrel, segno,
										 RelationGetNumberOfAttributes(parentrel));
		}
		else
		{
			pfree(seginfo);
		}
		AOCSFileSegInfoAddCount(parentrel, segno, 0, 0, 1);
	}
}

/*
 * Pre-commit processing for append only tables.
 *
 * Update the tup count of all hash entries for all AO table writes that
 * were executed in this transaction.
 *
 * Note that we do NOT free the segno entry quite yet! we must do it later on,
 * in AtEOXact_AppendOnly
 *
 * NOTE: if you change this function, take a look at AtAbort_AppendOnly and
 * AtEOXact_AppendOnly as well.
 */
void
AtCommit_AppendOnly(TransactionId CurrentXid)
{
	HASH_SEQ_STATUS status;
	AORelHashEntry	aoentry;

	if (appendOnlyInsertXact == InvalidTransactionId || CurrentXid == InvalidTransactionId)
		return;

	hash_seq_init(&status, AppendOnlyHash);

	/* We should have an XID if we modified AO tables */
	Assert(CurrentXid == appendOnlyInsertXact);

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	/*
	 * for each AO table hash entry
	 */
	while ((aoentry = (AORelHashEntryData *) hash_seq_search(&status)) != NULL)
	{
		/*
		 * Only look at tables that are marked in use currently
		 */
		if (aoentry->txns_using_rel > 0)
		{
			int			i;

			/*
			 * Was any segfile updated in our own transaction?
			 */
			for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
			{
				AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
				TransactionId InsertingXid = segfilestat->xid;

				if (InsertingXid == CurrentXid)
				{
					/* bingo! */
					Assert(segfilestat->state == INSERT_USE ||
						   segfilestat->state == COMPACTION_USE ||
						   segfilestat->state == DROP_USE ||
						   segfilestat->state == PSEUDO_COMPACTION_USE);

					if (Debug_appendonly_print_segfile_choice)
						ereport(LOG,
								(errmsg("AtCommit_AppendOnly: found a segno that inserted in "
										"our txn for table %d. Updating segno %d "
										"tupcount: old count " INT64_FORMAT ", tups "
										"added in this txn " INT64_FORMAT " new "
										"count " INT64_FORMAT, aoentry->relid, i,
										(int64) segfilestat->total_tupcount,
										(int64) segfilestat->tupsadded,
										(int64) segfilestat->total_tupcount +
										(int64) segfilestat->tupsadded )));

					/* now do the in memory update */
					segfilestat->total_tupcount += segfilestat->tupsadded;
					segfilestat->tupsadded = 0;
					segfilestat->isfull =
						(segfilestat->total_tupcount > segfileMaxRowThreshold());

					segfilestat->latestWriteXid = CurrentXid;
				}
			}
		}
	}

	LWLockRelease(AOSegFileLock);
}

/*
 * Abort processing for append only tables.
 *
 * Zero-out the tupcount of all hash entries for all AO table writes that
 * were executed in this transaction.
 *
 * Note that we do NOT free the segno entry quite yet! we must do it later on,
 * in AtEOXact_AppendOnly
 *
 * NOTE: if you change this function, take a look at AtCommit_AppendOnly and
 * AtEOXact_AppendOnly as well.
 */
void
AtAbort_AppendOnly(TransactionId CurrentXid)
{
	HASH_SEQ_STATUS status;
	AORelHashEntry aoentry = NULL;

	if (appendOnlyInsertXact == InvalidTransactionId || CurrentXid == InvalidTransactionId)
		return;

	/* We should have an XID if we modified AO tables */
	Assert(CurrentXid == appendOnlyInsertXact);

	hash_seq_init(&status, AppendOnlyHash);

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	/*
	 * for each AO table hash entry
	 */
	while ((aoentry = (AORelHashEntryData *) hash_seq_search(&status)) != NULL)
	{
		int			i;

		/*
		 * Only look at tables that are marked in use currently
		 */
		if (aoentry->txns_using_rel == 0)
			continue;

		/*
		 * Was any segfile updated in our own transaction?
		 */
		for (i = 0 ; i < MAX_AOREL_CONCURRENCY; i++)
		{
			AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
			TransactionId InsertingXid = segfilestat->xid;

			if (InsertingXid != CurrentXid)
				continue;

			/* bingo! */

			Assert(segfilestat->state == INSERT_USE ||
				   segfilestat->state == COMPACTION_USE ||
				   segfilestat->state == DROP_USE ||
				   segfilestat->state == PSEUDO_COMPACTION_USE);

			if (Debug_appendonly_print_segfile_choice)
			{
				ereport(LOG,
						(errmsg("AtAbort_AppendOnly: found a segno that inserted in our txn for "
								"table %d. Cleaning segno %d tupcount: old "
								"count " INT64_FORMAT " tups added in this "
								"txn " INT64_FORMAT ", count "
								"remains " INT64_FORMAT, aoentry->relid, i,
								(int64) segfilestat->total_tupcount,
								(int64) segfilestat->tupsadded,
								(int64) segfilestat->total_tupcount)));
			}

			/* now do the in memory cleanup. tupcount not touched */
			segfilestat->tupsadded = 0;
			segfilestat->aborted = true;
			/* state transitions are done by AtEOXact_AppendOnly and friends */
		}
	}

	LWLockRelease(AOSegFileLock);
}

/*
 * Assumes that AOSegFileLock is held.
 * Transitions from state 3, 4, or 4 to the correct next state based
 * on the abort/commit of the transaction.
 *
 */
static void
AtEOXact_AppendOnly_StateTransition(AORelHashEntry aoentry, int segno,
									AOSegfileStatus *segfilestat)
{
	AOSegfileState oldstate;

	Assert(segfilestat);
	Assert(segfilestat->state == INSERT_USE ||
		   segfilestat->state == COMPACTION_USE ||
		   segfilestat->state == DROP_USE ||
		   segfilestat->state == PSEUDO_COMPACTION_USE);

	oldstate = segfilestat->state;
	if (segfilestat->state == INSERT_USE)
	{
		segfilestat->state = AVAILABLE;
	}
	else if (segfilestat->state == DROP_USE)
	{
		if (!segfilestat->aborted)
		{
			segfilestat->state = AVAILABLE;
		}
		else
		{
			segfilestat->state = AWAITING_DROP_READY;
		}
	}
	else if (segfilestat->state == COMPACTION_USE)
	{
		if (!segfilestat->aborted)
		{
			segfilestat->state = COMPACTED_AWAITING_DROP;
		}
		else
		{
			segfilestat->state = AVAILABLE;
		}
	}
	else if (segfilestat->state == PSEUDO_COMPACTION_USE)
	{
		if (!segfilestat->aborted)
		{
			segfilestat->state = COMPACTED_AWAITING_DROP;
		}
		else
		{
			segfilestat->state = AWAITING_DROP_READY;
		}
	}
	else
	{
		Assert(false);
	}

	if (Debug_appendonly_print_segfile_choice)
		ereport(LOG,
				  (errmsg("Segment file state transition for "
						  "table %d, segno %d: %d -> %d",
						  aoentry->relid, segno,
						  oldstate, segfilestat->state)));
	segfilestat->xid = InvalidTransactionId;
	segfilestat->aborted = false;
}

/*
 * Assumes that AOSegFileLock is held.
 */
static void
AtEOXact_AppendOnly_Relation(AORelHashEntry aoentry, TransactionId currentXid)
{
	int			i;
	bool		entry_updated = false;

	/*
	 * Only look at tables that are marked in use currently
	 */
	if (aoentry->txns_using_rel == 0)
	{
		return;
	}

	/*
	 * Was any segfile updated in our own transaction?
	 */
	for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
		TransactionId InsertingXid = segfilestat->xid;

		if (InsertingXid != currentXid)
		{
			continue;
		}
		/* bingo! */

		AtEOXact_AppendOnly_StateTransition(aoentry, i, segfilestat);
		entry_updated = true;
	}

	/* done with this AO table entry */
	if (entry_updated)
	{
		aoentry->txns_using_rel--;

		if (Debug_appendonly_print_segfile_choice)
			ereport(LOG,
					(errmsg("AtEOXact_AppendOnly: updated txns_using_rel, it is now %d",
							aoentry->txns_using_rel)));
	}

	if (test_AppendOnlyHash_eviction_vs_just_marking_not_inuse)
	{
		/*
		 * If no transaction is using this entry, it can be removed if
		 * hash-table gets full. So perform the same here if the above GUC is
		 * set.
		 */
		if (aoentry->txns_using_rel == 0)
		{
			AORelRemoveHashEntry(aoentry->relid);
		}
	}
}

/*
 * End of txn processing for append only tables.
 *
 * This must be called after the AtCommit or AtAbort functions above did their
 * job already updating the tupcounts in shared memory. We now find the relevant
 * entries again and mark them not in use, so that the AO writer could allocate
 * them to future txns.
 *
 * NOTE: if you change this function, take a look at AtCommit_AppendOnly and
 * AtAbort_AppendOnly as well.
 */
void
AtEOXact_AppendOnly(TransactionId CurrentXid)
{
	HASH_SEQ_STATUS status;
	AORelHashEntry aoentry = NULL;

	if (appendOnlyInsertXact == InvalidTransactionId || CurrentXid == InvalidTransactionId)
		return;

	/* We should have an XID if we modified AO tables */
	Assert(CurrentXid == appendOnlyInsertXact);

	hash_seq_init(&status, AppendOnlyHash);

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	/*
	 * for each AO table hash entry
	 */
	while ((aoentry = (AORelHashEntryData *) hash_seq_search(&status)) != NULL)
	{
		AtEOXact_AppendOnly_Relation(aoentry, CurrentXid);
	}

	LWLockRelease(AOSegFileLock);

	appendOnlyInsertXact = InvalidTransactionId;
}

void
ValidateAppendOnlyMetaDataSnapshot(
								   Snapshot *appendOnlyMetaDataSnapshot)
{
	/* Placeholder. */
}
