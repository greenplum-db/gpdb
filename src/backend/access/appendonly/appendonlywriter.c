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
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_authid.h"
#include "libpq-fe.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
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
static bool appendOnlyInsertXact = false;

/*
 * local functions
 */
static bool AOHashTableInit(void);
static AORelHashEntry AppendOnlyRelHashNew(Oid relid, bool *exists);
static AORelHashEntry AORelGetHashEntry(Oid relid);
static AORelHashEntry AORelLookupHashEntry(Oid relid);
static bool AORelCreateHashEntry(Oid relid);
static bool *GetFileSegStateInfoFromSegments(Relation parentrel);
static int64 *GetTotalTupleCountFromSegments(Relation parentrel, int segno);

/*
 * AppendOnlyWriterShmemSize -- estimate size the append only writer structures
 * will need in shared memory.
 *
 */
Size
AppendOnlyWriterShmemSize(void)
{
	Size		size;

	Insist(Gp_role == GP_ROLE_DISPATCH);

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

	Insist(Gp_role == GP_ROLE_DISPATCH);

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

	Insist(Gp_role == GP_ROLE_DISPATCH);

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(Oid);
	info.entrysize = sizeof(AORelHashEntryData);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("AOHashTableInit: Creating hash table for %d append only tables",
					  MaxAppendOnlyTables)));

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
	bool	   *awaiting_drop = NULL;

	Insist(Gp_role == GP_ROLE_DISPATCH);

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

	/* Ask segment DBs about the segfile status */
	awaiting_drop = GetFileSegStateInfoFromSegments(aorel);

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
		aoHashEntry->relsegfiles[i].latestWriteXid = InvalidDistributedTransactionId;
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

		if (awaiting_drop[segno])
		{
			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("Found segment num with awaiting drop state on at least one segment node: "
							  "relation %d, segno %d", relid, segno)));
			aoHashEntry->relsegfiles[segno].state = AWAITING_DROP_READY;
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
	if (awaiting_drop)
	{
		pfree(awaiting_drop);
	}

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("AORelCreateHashEntry: Created a hash entry for append-only "
					  "relation %d ", relid)));

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

	Insist(Gp_role == GP_ROLE_DISPATCH);

	aoentry = hash_search(AppendOnlyHash,
						  (void *) &relid,
						  HASH_REMOVE,
						  &found);

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("AORelRemoveHashEntry: Remove hash entry for inactive append-only "
					  "relation %d (found %s)",
					  relid,
					  (aoentry != NULL ? "true" : "false"))));

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
	AORelHashEntryData *aoentry;

	Insist(Gp_role == GP_ROLE_DISPATCH);

	aoentry = (AORelHashEntryData *) hash_search(AppendOnlyHash,
												 (void *) &relid,
												 HASH_FIND,
												 &found);

	if (!aoentry)
		return NULL;

	return (AORelHashEntry) aoentry;
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
		ereport(ERROR, (errmsg("expected an AO hash entry for relid %d but "
							   "found none", relid)));

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("AORelGetHashEntry: Retrieved hash entry for append-only relation "
					  "%d", relid)));

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

	Insist(Gp_role == GP_ROLE_DISPATCH);

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
					ereportif(Debug_appendonly_print_segfile_choice, LOG,
							  (errmsg("AppendOnlyRelHashNew: Appendonly Writer removing an "
									  "unused entry (rel %d) to make "
									  "room for a new one (rel %d)",
									  hentry->relid, relid)));

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
	aorelentry = (AORelHashEntryData *) hash_search(AppendOnlyHash,
													(void *) &relid,
													HASH_ENTER_NULL,
													exists);

	/* caller should check "exists" to know if relid entry exists already */
	if (*exists)
		return NULL;

	return (AORelHashEntry) aorelentry;
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
	DistributedSnapshot *ds;
	DistributedTransactionId latestWriteXid =
	segfilestat->latestWriteXid;

	/*
	 * If latestWriteXid is invalid, simple return false.
	 */
	if (!TransactionIdIsValid(latestWriteXid))
	{
		ereportif(Debug_appendonly_print_segfile_choice, LOG,
				  (errmsg("usedByConcurrentTransaction: latestWriterXid %u of segno %d not in use, so it is NOT considered concurrent",
						  latestWriteXid,
						  segno)));
		return false;
	}

	/*
	 * If the current transaction is started earlier than latestWriteXid, this
	 * segno is surely used by a concurrent transaction. So return true here.
	 */
	if (TransactionIdPrecedes(getDistributedTransactionId(),
							  latestWriteXid))
	{
		ereportif(Debug_appendonly_print_segfile_choice, LOG,
				  (errmsg("usedByConcurrentTransaction: current distributed transaction id %u preceeds latestWriterXid %x of segno %d, so it is considered concurrent",
						  getDistributedTransactionId(),
						  latestWriteXid,
						  segno)));
		return true;
	}

	/*
	 * Obtain the snapshot that is taken at the beginning of the transaction.
	 * We can't simply call GetTransactionSnapshot() here because it will
	 * create a new distributed snapshot for non-serializable transaction
	 * isolation level, and it may be too late.
	 */
	Snapshot	snapshot = NULL;

	if (!ActiveSnapshotSet())
		elog(ERROR, "could not check if dxid:%d is concurrently used, ActiveSnapshot not set",
			 latestWriteXid);

	snapshot = GetActiveSnapshot();

	if (Debug_appendonly_print_segfile_choice)
	{
		elog(LOG, "usedByConcurrentTransaction: current distributed transaction id = %x, latestWriteXid that uses segno %d is %x",
			 getDistributedTransactionId(), segno, latestWriteXid);
		if (snapshot->haveDistribSnapshot)
			LogDistributedSnapshotInfo(snapshot, "Used snapshot: ");
		else
			elog(LOG,
				 "usedByConcurrentTransaction: snapshot is not distributed, so it is NOT considered concurrent");
	}

	if (!snapshot->haveDistribSnapshot)
	{
		return false;
	}

	ds = &snapshot->distribSnapshotWithLocalMapping.ds;

	/* If latestWriterXid is invisible to me, return true. */
	if (latestWriteXid >= ds->xmax)
	{
		ereportif(Debug_appendonly_print_segfile_choice, LOG,
				  (errmsg("usedByConcurrentTransaction: latestWriterXid %x of segno %d is >= distributed snapshot xmax %u, so it is considered concurrent",
						  latestWriteXid,
						  segno,
						  ds->xmax)));
		return true;
	}

	/*
	 * If latestWriteXid is in in-progress, return true.
	 */
	int32		inProgressCount = ds->count;

	for (int inProgressNo = 0; inProgressNo < inProgressCount; inProgressNo++)
	{
		if (latestWriteXid == ds->inProgressXidArray[inProgressNo])
		{
			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("usedByConcurrentTransaction: latestWriterXid %x of segno %d is equal to distributed snapshot in-flight %u, so it is considered concurrent",
							  latestWriteXid,
							  segno,
							  ds->inProgressXidArray[inProgressNo])));
			return true;
		}
	}

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("usedByConcurrentTransaction: snapshot can see latestWriteXid %u as committed, so it is NOT considered concurrent",
					  latestWriteXid)));
	return false;
}

void
DeregisterSegnoForCompactionDrop(Oid relid, List *compactedSegmentFileList)
{
	TransactionId CurrentXid = GetTopTransactionId();
	AORelHashEntryData *aoentry;
	int			i;

	Assert(Gp_role != GP_ROLE_EXECUTE);
	if (Gp_role == GP_ROLE_UTILITY)
	{
		return;
	}

	if (compactedSegmentFileList == NIL)
	{
		return;
	}

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(relid);
	Assert(aoentry);
	aoentry->txns_using_rel++;

	for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];

		if (list_member_int(compactedSegmentFileList, i))
		{
			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("Deregister segno %d for drop "
							  "relation \"%s\" (%d)", i,
							  get_rel_name(relid), relid)));

			Assert(segfilestat->state == COMPACTED_AWAITING_DROP);
			segfilestat->xid = CurrentXid;
			appendOnlyInsertXact = true;
			segfilestat->state = COMPACTED_DROP_SKIPPED;
		}
	}

	LWLockRelease(AOSegFileLock);
	return;
}

void
RegisterSegnoForCompactionDrop(Oid relid, List *compactedSegmentFileList)
{
	TransactionId CurrentXid = GetTopTransactionId();
	AORelHashEntryData *aoentry;
	int			i;

	Assert(Gp_role != GP_ROLE_EXECUTE);
	if (Gp_role == GP_ROLE_UTILITY)
	{
		return;
	}

	if (compactedSegmentFileList == NIL)
	{
		return;
	}

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(relid);
	Assert(aoentry);
	aoentry->txns_using_rel++;

	for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];

		if (list_member_int(compactedSegmentFileList, i))
		{
			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("Register segno %d for drop "
							  "relation \"%s\" (%d)", i,
							  get_rel_name(relid), relid)));

			Assert(segfilestat->state == COMPACTED_AWAITING_DROP);
			segfilestat->xid = CurrentXid;
			appendOnlyInsertXact = true;
			segfilestat->state = DROP_USE;
		}
	}

	LWLockRelease(AOSegFileLock);
	return;
}

/*
 * SetSegnoForCompaction
 *
 * This function determines which segment to use for the next
 * compaction run.
 *
 * If a list with more than one entry is returned, all these segments should be
 * compacted. This is only returned in utility mode as the usual ways to
 * determine a segment for compaction are not available.
 * If NIL is returned, no segment should be compacted. This usually
 * means that all segments are clean or empty.
 *
 * Should only be called in DISPATCH and UTILITY mode.
 */
List *
SetSegnoForCompaction(Relation rel,
					  List *compactedSegmentFileList,
					  List *insertedSegmentFileList,
					  bool *is_drop)
{
	TransactionId CurrentXid = GetTopTransactionId();
	int			usesegno;
	int			i;
	AORelHashEntryData *aoentry;
	int64		segzero_tupcount = 0;

	Assert(Gp_role != GP_ROLE_EXECUTE);
	Assert(is_drop);
	*is_drop = false;
	if (Gp_role == GP_ROLE_UTILITY)
	{
		if (compactedSegmentFileList)
		{
			return NIL;
		}
		else
		{
			/* Compact all segments */
			List	   *compaction_segno_list = NIL;

			for (int i = 1; i < MAX_AOREL_CONCURRENCY; i++)
			{
				compaction_segno_list = lappend_int(compaction_segno_list, i);
			}
			return compaction_segno_list;
		}
	}

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("SetSegnoForCompaction: "
					  "Choosing a compaction segno for append-only "
					  "relation \"%s\" (%d)",
					  RelationGetRelationName(rel), RelationGetRelid(rel))));

	/*
	 * On the first call in a vacuum run, get an updated estimate from segno
	 * 0. We do this before aquiring the AOSegFileLock.
	 */
	if (!compactedSegmentFileList)
	{
		int64	   *total_tupcount;

		total_tupcount = GetTotalTupleCountFromSegments(rel, 0);
		segzero_tupcount = total_tupcount[0];
		pfree(total_tupcount);
	}

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(RelationGetRelid(rel));
	Assert(aoentry);

	/*
	 * On the first call in a vacuum run, set the estimated for segno 0
	 */
	if (!compactedSegmentFileList)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[0];

		segfilestat->total_tupcount = segzero_tupcount;
	}

	/* First: Always check if some segment is awaiting a drop */
	usesegno = APPENDONLY_COMPACTION_SEGNO_INVALID;
	for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];

		ereportif(Debug_appendonly_print_segfile_choice, LOG,
				  (errmsg("segment file %i for append-only relation \"%s\" (%d): state %d",
						  i, RelationGetRelationName(rel), RelationGetRelid(rel), segfilestat->state)));

		if ((segfilestat->state == AWAITING_DROP_READY ||
			 segfilestat->state == COMPACTED_AWAITING_DROP) &&
			!usedByConcurrentTransaction(segfilestat, i))
		{
			ereportif(Debug_appendonly_print_segfile_choice, LOG,
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

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("SetSegnoForCompaction: "
							  "Check segno %d for appendonly relation \"%s\" (%d): "
							  "total tupcount " INT64_FORMAT ", state %d, "
							  "in compacted list %d, in inserted list %d",
							  i, RelationGetRelationName(rel), RelationGetRelid(rel),
							  segfilestat->total_tupcount, segfilestat->state,
							  in_compaction_list, in_inserted_list)));

			/*
			 * Select when there are tuples and it is available.
			 */
			if ((segfilestat->total_tupcount > 0) &&
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
		appendOnlyInsertXact = true;

		ereportif(Debug_appendonly_print_segfile_choice, LOG,
				  (errmsg("Compaction segment chosen for append-only relation \"%s\" (%d) "
						  "is %d (tupcount " INT64_FORMAT ", txns count %d)",
						  RelationGetRelationName(rel), RelationGetRelid(rel), usesegno,
						  aoentry->relsegfiles[usesegno].total_tupcount,
						  aoentry->txns_using_rel)));
	}
	else
	{
		ereportif(Debug_appendonly_print_segfile_choice, LOG,
				  (errmsg("No compaction segment chosen for append-only relation \"%s\" (%d)",
						  RelationGetRelationName(rel), RelationGetRelid(rel))));
	}

	LWLockRelease(AOSegFileLock);

	Assert(usesegno >= 0 || usesegno == APPENDONLY_COMPACTION_SEGNO_INVALID);

	if (usesegno == APPENDONLY_COMPACTION_SEGNO_INVALID)
	{
		return NIL;
	}
	return lappend_int(NIL, usesegno);
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
 *
 * Should only be called in DISPATCH and UTILITY mode.
 */
int
SetSegnoForCompactionInsert(Relation rel,
							List *compacted_segno,
							List *compactedSegmentFileList,
							List *insertedSegmentFileList)
{
	int			i,
				usesegno = -1;
	bool		segno_chosen = false;
	AORelHashEntryData *aoentry = NULL;
	TransactionId CurrentXid = GetTopTransactionId();
	int64		min_tupcount;

	Assert(Gp_role != GP_ROLE_EXECUTE);

	if (Gp_role == GP_ROLE_UTILITY)
	{
		return RESERVED_SEGNO;
	}

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("SetSegnoForCompactionInsert: "
					  "Choosing a segno for append-only "
					  "relation \"%s\" (%d)",
					  RelationGetRelationName(rel), RelationGetRelid(rel))));

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	aoentry = AORelGetOrCreateHashEntry(RelationGetRelid(rel));
	Assert(aoentry);

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("SetSegnoForCompaction: got the hash entry for relation \"%s\" (%d).",
					  RelationGetRelationName(rel), RelationGetRelid(rel))));

	min_tupcount = INT64_MAX;
	usesegno = 0;

	/*
	 * Never use segno 0 for inserts (unless in utility mode)
	 */
	for (i = 1; i < MAX_AOREL_CONCURRENCY; i++)
	{
		AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
		bool		in_compaction_list = list_member_int(compacted_segno, i) ||
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

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("SetSegnoForCompaction: reusing segno %d for append-"
							  "only relation "
							  "%d. there are " INT64_FORMAT " tuples "
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
	appendOnlyInsertXact = true;

	Assert(usesegno >= 0);
	Assert(usesegno != RESERVED_SEGNO);

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("Segno chosen for append-only relation \"%s\" (%d) "
					  "is %d", RelationGetRelationName(rel), RelationGetRelid(rel), usesegno)));

	return usesegno;
}

/*
 * SetSegnoForWrite
 *
 * This function includes the key logic of the append only writer.
 *
 * Depending on the gp role and existingsegno make a decision on which
 * segment nubmer should be used to write into during the COPY or INSERT
 * operation we're executing.
 *
 * the return value is a segment file number to use for inserting by each
 * segdb into its local AO table.
 *
 * ROLE_DISPATCH - when this function is called on a QD the QD needs to select
 *				   a segment file number for writing data. It does so by
 *				   looking at the in-memory hash table and selecting a segment
 *				   number that the most empty across the database and is also
 *				   not currently used. Or, if we are in an explicit
 *				   transaction and inserting into the same table we use the
 *				   same segno over and over again. the passed in parameter
 *				   'existingsegno' is meaningless for this role. Also, it's
 *				   mandatory for insert coming from same transaction as create
 *				   (except CTAS or ALTER which use RESERVED_SEGNO) to use
 *				   segfile 1, currenty below logic guarantees the same as it
 *				   always checks and assigns in ascending order which file to
 *				   use for insert. This property is leveraged to pre-create
 *				   entries in gp_fastsequence (for further details check
 *				   comment for InsertInitialFastSequenceEntries()).
 *
 * ROLE_EXECUTE  - we need to use the segment file number that the QD decided
 *				   on and sent to us. it is the passed in parameter - use it.
 *
 * ROLE_UTILITY  - always use the reserved segno RESERVED_SEGNO
 *
 */
int
SetSegnoForWrite(Relation rel, int existingsegno)
{
	/* these vars are used in GP_ROLE_DISPATCH only */
	int			i,
				usesegno = -1;
	bool		segno_chosen = false;
	AORelHashEntryData *aoentry = NULL;
	TransactionId CurrentXid = GetTopTransactionId();

	switch (Gp_role)
	{
		case GP_ROLE_EXECUTE:

			Assert(existingsegno != InvalidFileSegNumber);
			Assert(existingsegno != RESERVED_SEGNO);
			return existingsegno;

		case GP_ROLE_UTILITY:

			Assert(existingsegno == InvalidFileSegNumber);
			return RESERVED_SEGNO;

		case GP_ROLE_DISPATCH:

			Assert(existingsegno == InvalidFileSegNumber);

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("SetSegnoForWrite: Choosing a segno for append-only "
							  "relation \"%s\" (%d) ",
							  RelationGetRelationName(rel), RelationGetRelid(rel))));

			LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

			aoentry = AORelGetOrCreateHashEntry(RelationGetRelid(rel));
			Assert(aoentry);
			aoentry->txns_using_rel++;

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("SetSegnoForWrite: got the hash entry for relation \"%s\" (%d). "
							  "setting txns_using_rel to %d",
							  RelationGetRelationName(rel), RelationGetRelid(rel),
							  aoentry->txns_using_rel)));

			/*
			 * Now pick the not in use segment and is not over the allowed
			 * size threshold (90% full).
			 *
			 * However, if we already picked a segno for a previous statement
			 * in this very same transaction we are still in (explicit txn) we
			 * pick the same one to insert into it again.
			 *
			 * Never use segno 0 for inserts (unless in utility mode)
			 */
			for (i = 1; i < MAX_AOREL_CONCURRENCY; i++)
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
						 * this segno is avaiable and not full. use it.
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
						aoentry->txns_using_rel--;	/* same txn. re-adjust */

						ereportif(Debug_appendonly_print_segfile_choice, LOG,
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
			appendOnlyInsertXact = true;

			Assert(usesegno >= 0);
			Assert(usesegno != RESERVED_SEGNO);

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("Segno chosen for append-only relation \"%s\" (%d) "
							  "is %d", RelationGetRelationName(rel), RelationGetRelid(rel), usesegno)));

			return usesegno;

			/* fix this for dispatch agent. for now it's broken anyway. */
		default:
			Assert(false);
			return InvalidFileSegNumber;
	}

}

/*
 * assignPerRelSegno
 *
 * For each relation that may get data inserted into it call SetSegnoForWrite
 * to assign a segno to insert into. Create a list of relid-to-segno mappings
 * and return it to the caller.
 *
 * when the all_relids list has more than one entry in it we are inserting into
 * a partitioned table. note that we assign a segno for each AO partition, even
 * if eventually no data will get inserted into it (since we can't know ahead
 * of time).
 */
List *
assignPerRelSegno(List *all_relids)
{
	ListCell   *cell;
	List	   *mapping = NIL;

	foreach(cell, all_relids)
	{
		Oid			cur_relid = lfirst_oid(cell);
		Relation	rel = heap_open(cur_relid, NoLock);

		if (RelationIsAoCols(rel) || RelationIsAoRows(rel))
		{
			SegfileMapNode *n;

			n = makeNode(SegfileMapNode);
			n->relid = cur_relid;
			n->segno = SetSegnoForWrite(rel, InvalidFileSegNumber);

			Assert(n->relid != InvalidOid);
			Assert(n->segno != InvalidFileSegNumber);

			mapping = lappend(mapping, n);

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("assignPerRelSegno: Appendonly Writer assigned segno %d to "
							  "relid %d for this write operation",
							  n->segno, n->relid)));
		}

		heap_close(rel, NoLock);
	}

	return mapping;
}

/*
 * Collects tupcount for each segfile from all segdbs by sending a query.
 * Returns an array of int64, each of whose element has the aggregated
 * total tupcount for corresponding segfile.  This fills elements for all
 * segfiles that are available in segments if segno is negative, otherwise
 * tries to fetch the tupcount for only the segfile that is passed by segno.
 */
static int64 *
GetTotalTupleCountFromSegments(Relation parentrel,
							   int segno)
{
	StringInfoData sqlstmt;
	Relation	aosegrel;
	CdbPgResults cdb_pgresults = {NULL, 0};
	int			i,
				j;
	int64	   *total_tupcount = NULL;
	Oid			save_userid;
	bool		save_secdefcxt;

	/*
	 * get the name of the aoseg relation
	 */
	aosegrel = heap_open(parentrel->rd_appendonly->segrelid, AccessShareLock);

	/*
	 * assemble our query string
	 */
	initStringInfo(&sqlstmt);
	appendStringInfo(&sqlstmt, "SELECT tupcount, segno FROM pg_aoseg.%s",
					 RelationGetRelationName(aosegrel));
	if (segno >= 0)
		appendStringInfo(&sqlstmt, " WHERE segno = %d", segno);
	heap_close(aosegrel, AccessShareLock);

	/* Allocate result array to be returned. */
	total_tupcount = palloc0(sizeof(int64) * MAX_AOREL_CONCURRENCY);

	/*
	 * Dispatch the query to segments.  Although this path is not critical to
	 * use SPI, it is desirable to avoid another MPP query here, too.
	 *
	 * Since pg_aoseg namespace is a part of catalog, we need a superuser
	 * privilege.  This is just a band-aid and we need to revisit the
	 * mechanism to synchronize pg_aoseg between master and segment.
	 */
	GetUserIdAndContext(&save_userid, &save_secdefcxt);
	SetUserIdAndContext(BOOTSTRAP_SUPERUSERID, true);

	PG_TRY();
	{
		CdbDispatchCommand(sqlstmt.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

		/* Restore userid */
		SetUserIdAndContext(save_userid, save_secdefcxt);

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			struct pg_result *pgresult = cdb_pgresults.pg_results[i];

			if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
				ereport(ERROR,
						(errmsg("failed to obtain AO total tupcount: %s (%s)",
								sqlstmt.data,
								PQresultErrorMessage(pgresult))));
			else
			{
				for (j = 0; j < PQntuples(pgresult); j++)
				{
					char	   *value;
					int64		tupcount;
					int			segno;

					/* We don't expect NULL, but sanity check. */
					if (PQgetisnull(pgresult, j, 0) == 1)
						elog(ERROR, "unexpected NULL in tupcount in results[%d]: %s",
							 i, sqlstmt.data);
					if (PQgetisnull(pgresult, j, 1) == 1)
						elog(ERROR, "unexpected NULL in segno in results[%d]: %s",
							 i, sqlstmt.data);

					value = PQgetvalue(pgresult, j, 0);
					tupcount = DatumGetFloat8(
											  DirectFunctionCall1(float8in, CStringGetDatum(value)));
					value = PQgetvalue(pgresult, j, 1);
					segno = pg_atoi(value, sizeof(int32), 0);
					total_tupcount[segno] += tupcount;
				}
			}
		}
	}
	PG_CATCH();
	{
		SetUserIdAndContext(save_userid, save_secdefcxt);
		cdbdisp_clearCdbPgResults(&cdb_pgresults);
		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(sqlstmt.data);
	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	return total_tupcount;
}

/*
 * Check if the segfiles are awaiting to be dropped by dispatching a query.
 * Returns palloc'ed an array of bool, whose element is true if the
 * corresponding segfile is marked as awaiting to be dropped in any of segdb.
 * We do this one-shot instead of interating for each segno, because the
 * dispatch cost increases as the number of segment becomes large.
 */
static bool *
GetFileSegStateInfoFromSegments(Relation parentrel)
{
	StringInfoData sqlstmt;
	Relation	aosegrel;
	CdbPgResults cdb_pgresults = {NULL, 0};
	int			i,
				j;
	bool	   *awaiting_drop;
	Oid			save_userid;
	bool		save_secdefcxt;

	Assert(RelationIsAoRows(parentrel) || RelationIsAoCols(parentrel));

	/*
	 * get the name of the aoseg relation
	 */
	aosegrel = heap_open(parentrel->rd_appendonly->segrelid, AccessShareLock);

	/*
	 * assemble our query string
	 */
	initStringInfo(&sqlstmt);
	appendStringInfo(&sqlstmt, "SELECT state, segno FROM pg_aoseg.%s",
					 RelationGetRelationName(aosegrel));
	heap_close(aosegrel, AccessShareLock);

	elogif(Debug_appendonly_print_segfile_choice, LOG,
		   "Get awaiting drop state from segments: "
		   "releation %s (%d)",
		   RelationGetRelationName(parentrel),
		   RelationGetRelid(parentrel));

	/* Allocate result array to be returned. */
	awaiting_drop = palloc0(sizeof(bool) * MAX_AOREL_CONCURRENCY);

	/*
	 * We should not use SPI here because the code path is between the
	 * initialization of slice table and query dispatch.  SPI would create
	 * another slice table and bump gp_interconnect_id, which would cause
	 * inconsistent order of execution.
	 *
	 * Since pg_aoseg namespace is a part of catalog, we need a superuser
	 * privilege.  This is just a band-aid and we need to revisit the
	 * mechanism to synchronize pg_aoseg between master and segment.
	 */
	GetUserIdAndContext(&save_userid, &save_secdefcxt);
	SetUserIdAndContext(BOOTSTRAP_SUPERUSERID, true);

	PG_TRY();
	{
		CdbDispatchCommand(sqlstmt.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

		/* Restore userid */
		SetUserIdAndContext(save_userid, save_secdefcxt);

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			struct pg_result *pgresult = cdb_pgresults.pg_results[i];

			if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
				ereport(ERROR,
						(errmsg("failed to obtain AO segfile state: %s (%s)",
								sqlstmt.data,
								PQresultErrorMessage(pgresult))));
			else
			{
				for (j = 0; j < PQntuples(pgresult); j++)
				{
					char	   *value;
					int			qe_state;
					int			segno;

					/* We don't expect NULL, but sanity check. */
					if (PQgetisnull(pgresult, j, 0) == 1)
						elog(ERROR, "unexpected NULL in state in results[%d]: %s",
							 i, sqlstmt.data);
					if (PQgetisnull(pgresult, j, 1) == 1)
						elog(ERROR, "unexpected NULL in segno in results[%d}: %s",
							 i, sqlstmt.data);
					value = PQgetvalue(pgresult, j, 0);
					qe_state = pg_atoi(value, sizeof(int32), 0);
					value = PQgetvalue(pgresult, j, 1);
					segno = pg_atoi(value, sizeof(int32), 0);

					if (segno < 0)
						elog(ERROR, "segno %d is negative", segno);
					if (segno >= MAX_AOREL_CONCURRENCY)
						elog(ERROR, "segno %d exceeds max AO concurrency", segno);

					if (qe_state == AOSEG_STATE_AWAITING_DROP)
					{
						awaiting_drop[segno] = true;
						elogif(Debug_appendonly_print_segfile_choice, LOG,
							   "Found awaiting drop segment file: "
							   "releation %s (%d), segno = %d",
							   RelationGetRelationName(parentrel),
							   RelationGetRelid(parentrel),
							   segno);
					}
				}
			}
		}
	}
	PG_CATCH();
	{
		SetUserIdAndContext(save_userid, save_secdefcxt);
		cdbdisp_clearCdbPgResults(&cdb_pgresults);
		PG_RE_THROW();
	}
	PG_END_TRY();

	pfree(sqlstmt.data);
	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	return awaiting_drop;
}

/*
 * Updates the tupcount information from the segments.
 * Should only be called in rare circumstances from the QD.
 */
void
UpdateMasterAosegTotalsFromSegments(Relation parentrel,
									Snapshot appendOnlyMetaDataSnapshot,
									List *segmentNumList,
									int64 modcount_added)
{
	ListCell   *l;
	int64	   *total_tupcount;

	Assert(RelationIsAoRows(parentrel) || RelationIsAoCols(parentrel));
	Assert(Gp_role == GP_ROLE_DISPATCH);

	/* Give -1 for segno, so that we'll have all segfile tupcount. */
	total_tupcount = GetTotalTupleCountFromSegments(parentrel, -1);

	/*
	 * We are interested in only the segfiles that were told to be updated.
	 */
	foreach(l, segmentNumList)
	{
		int			qe_segno = lfirst_int(l);
		int64		qe_tupcount = total_tupcount[qe_segno];
		int64		known_tupcount = 0;

		Assert(qe_segno >= 0);

		/*
		 * If segfile doesn't exist, assume tupcount is 0.
		 * UpdateMasterAosegTotals will create a segfile entry if necessary.
		 */
		if (RelationIsAoRows(parentrel))
		{
			FileSegInfo *fsinfo;

			fsinfo = GetFileSegInfo(parentrel,
									appendOnlyMetaDataSnapshot, qe_segno);
			if (fsinfo != NULL)
			{
				known_tupcount = fsinfo->total_tupcount;
				pfree(fsinfo);
			}
		}
		else
		{
			AOCSFileSegInfo *seginfo;

			Assert(RelationIsAoCols(parentrel));

			seginfo = GetAOCSFileSegInfo(parentrel,
										 SnapshotNow, qe_segno);

			if (seginfo !=NULL)
			{
				known_tupcount = seginfo->total_tupcount;

				pfree(seginfo);
			}
		}

		/*
		 * Check if the new tupcount is different, and update the master
		 * catalog if so.
		 */
		if (known_tupcount != qe_tupcount)
		{
			int64		tupcount_diff = qe_tupcount - known_tupcount;

			elog(DEBUG3, "UpdateMasterAosegTotalsFromSegments: updating "
				 "segno %d with tupcount " INT64_FORMAT
				 ", known tupcount " INT64_FORMAT,
				 qe_segno,
				 qe_tupcount,
				 known_tupcount);
			UpdateMasterAosegTotals(parentrel, qe_segno,
									tupcount_diff, modcount_added);
		}
	}

	pfree(total_tupcount);
}

/*
 * UpdateMasterAosegTotals
 *
 * Update the aoseg table on the master node with the updated row count of
 * the whole distributed relation. We use this information later on to keep
 * track of file 'segments' and their EOF's and decide which segno to use in
 * future writes into the table.
 */
void
UpdateMasterAosegTotals(Relation parentrel, int segno, int64 tupcount, int64 modcount_added)
{
	AORelHashEntry aoHashEntry = NULL;

	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(segno >= 0);

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
			  (errmsg("UpdateMasterAosegTotals: Updating aoseg entry for append-only relation %d "
					  "with " INT64_FORMAT " new tuples for segno %d",
					  RelationGetRelid(parentrel), (int64) tupcount, segno)));

	/* CONSIDER: We should probably get this lock even sooner. */
	LockRelationAppendOnlySegmentFile(
									  &parentrel->rd_node,
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
		UpdateFileSegInfo(parentrel, segno, 0, 0, tupcount, 0,
						  modcount_added, AOSEG_STATE_USECURRENT);
	}
	else
	{
		AOCSFileSegInfo *seginfo;

		/* AO column store */
		Assert(RelationIsAoCols(parentrel));

		seginfo = GetAOCSFileSegInfo(
									 parentrel,
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
		AOCSFileSegInfoAddCount(parentrel, segno, tupcount, 0, modcount_added);
	}

	/*
	 * Now, update num of tups added in the hash table. This pending count
	 * will get added to the total count when the transaction actually
	 * commits. (or will get discarded if it aborts). We don't need to do the
	 * same trick for the aoseg table since MVCC protects us there in case we
	 * abort.
	 */
	aoHashEntry = AORelGetHashEntry(RelationGetRelid(parentrel));
	aoHashEntry->relsegfiles[segno].tupsadded += tupcount;
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
AtCommit_AppendOnly(void)
{
	HASH_SEQ_STATUS status;
	AORelHashEntry aoentry;
	TransactionId CurrentXid;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (!appendOnlyInsertXact)
		return;

	hash_seq_init(&status, AppendOnlyHash);

	CurrentXid = GetTopTransactionIdIfAny();
	/* We should have an XID if we modified AO tables */
	Assert(CurrentXid != InvalidTransactionId);

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
			 * Was any segfile was updated in our own transaction?
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
						   segfilestat->state == PSEUDO_COMPACTION_USE ||
						   segfilestat->state == COMPACTED_DROP_SKIPPED);

					ereportif(Debug_appendonly_print_segfile_choice, LOG,
							  (errmsg("AtCommit_AppendOnly: found a segno that inserted in "
									  "our txn for table %d. Updating segno %d "
									  "tupcount: old count " INT64_FORMAT ", tups "
									  "added in this txn " INT64_FORMAT " new "
									  "count " INT64_FORMAT, aoentry->relid, i,
									  (int64) segfilestat->total_tupcount,
									  (int64) segfilestat->tupsadded,
									  (int64) segfilestat->total_tupcount +
									  (int64) segfilestat->tupsadded)));

					/* now do the in memory update */
					segfilestat->total_tupcount += segfilestat->tupsadded;
					segfilestat->tupsadded = 0;
					segfilestat->isfull =
						(segfilestat->total_tupcount > segfileMaxRowThreshold());

					Assert(!TransactionIdIsValid(segfilestat->latestWriteXid) ||
						   TransactionIdPrecedes(segfilestat->latestWriteXid,
												 getDistributedTransactionId()));

					segfilestat->latestWriteXid = getDistributedTransactionId();
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
AtAbort_AppendOnly(void)
{
	HASH_SEQ_STATUS status;
	AORelHashEntry aoentry = NULL;
	TransactionId CurrentXid = GetCurrentTransactionIdIfAny();

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (!appendOnlyInsertXact)
	{
		return;
	}

	hash_seq_init(&status, AppendOnlyHash);

	LWLockAcquire(AOSegFileLock, LW_EXCLUSIVE);

	/*
	 * for each AO table hash entry
	 */
	while ((aoentry = (AORelHashEntryData *) hash_seq_search(&status)) != NULL)
	{
		/*
		 * Only look at tables that are marked in use currently
		 */
		if (aoentry->txns_using_rel == 0)
		{
			continue;
		}
		int			i;

		/*
		 * Was any segfile updated in our own transaction?
		 */
		for (i = 0; i < MAX_AOREL_CONCURRENCY; i++)
		{
			AOSegfileStatus *segfilestat = &aoentry->relsegfiles[i];
			TransactionId InsertingXid = segfilestat->xid;

			if (InsertingXid != CurrentXid)
			{
				continue;
			}
			/* bingo! */

			Assert(segfilestat->state == INSERT_USE ||
				   segfilestat->state == COMPACTION_USE ||
				   segfilestat->state == DROP_USE ||
				   segfilestat->state == PSEUDO_COMPACTION_USE ||
				   segfilestat->state == COMPACTED_DROP_SKIPPED);

			ereportif(Debug_appendonly_print_segfile_choice, LOG,
					  (errmsg("AtAbort_AppendOnly: found a segno that inserted in our txn for "
							  "table %d. Cleaning segno %d tupcount: old "
							  "count " INT64_FORMAT " tups added in this "
							  "txn " INT64_FORMAT ", count "
							  "remains " INT64_FORMAT, aoentry->relid, i,
							  (int64) segfilestat->total_tupcount,
							  (int64) segfilestat->tupsadded,
							  (int64) segfilestat->total_tupcount)));

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
		   segfilestat->state == PSEUDO_COMPACTION_USE ||
		   segfilestat->state == COMPACTED_DROP_SKIPPED);

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
	else if (segfilestat->state == COMPACTED_DROP_SKIPPED)
	{
		segfilestat->state = AWAITING_DROP_READY;
	}
	else
	{
		Assert(false);
	}

	ereportif(Debug_appendonly_print_segfile_choice, LOG,
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

		ereportif(Debug_appendonly_print_segfile_choice, LOG,
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
AtEOXact_AppendOnly(void)
{
	HASH_SEQ_STATUS status;
	AORelHashEntry aoentry = NULL;
	TransactionId CurrentXid = GetCurrentTransactionIdIfAny();

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (!appendOnlyInsertXact)
	{
		return;
	}

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

	appendOnlyInsertXact = false;
}
