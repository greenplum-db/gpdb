/*-------------------------------------------------------------------------
 *
 * smgr_ao.c
 *	  Tracking pending "resync EOFs" for append-only tables.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/smgr/smgr_ao.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/twophase.h"
#include "access/xact.h"
#include "cdb/cdbmirroredappendonly.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbtm.h"
#include "storage/smgr_ao.h"

static int	pendingAppendOnlyMirrorResyncIntentCount = 0;

typedef struct AppendOnlyMirrorResyncEofsKey
{
	RelFileNode relFileNode;
	int32		segmentFileNum;
	int			nestLevel;		/* Transaction nesting level. */
} AppendOnlyMirrorResyncEofsKey;

typedef struct AppendOnlyMirrorResyncEofs
{
	AppendOnlyMirrorResyncEofsKey key;

	char	   *relationName;

	ItemPointerData persistentTid;
	int64		persistentSerialNum;

	bool		didIncrementCommitCount;
	bool		isDistributedTransaction;
	char		gid[TMGIDSIZE];

	bool		mirrorCatchupRequired;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;

	int64		mirrorDataLossTrackingSessionNum;

	int64		mirrorNewEof;

} AppendOnlyMirrorResyncEofs;

static HTAB *AppendOnlyMirrorResyncEofsTable = NULL;


void
AppendOnlyMirrorResyncEofs_HashTableInit(void)
{
	HASHCTL		info;
	int			hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(AppendOnlyMirrorResyncEofsKey);
	info.entrysize = sizeof(AppendOnlyMirrorResyncEofs);
	info.hash = tag_hash;
	info.hcxt = TopMemoryContext;

	hash_flags = (HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

	AppendOnlyMirrorResyncEofsTable = hash_create("AO Mirror Resync EOFs", 10, &info, hash_flags);

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager: Append-Only mirror resync eofs hash-table created");
}

void
AppendOnlyMirrorResyncEofs_HashTableRemove(char *procName)
{
	if (AppendOnlyMirrorResyncEofsTable == NULL)
		return;

	hash_destroy(AppendOnlyMirrorResyncEofsTable);
	AppendOnlyMirrorResyncEofsTable = NULL;

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager (%s): Append-Only mirror resync eofs hash-table removed",
			 procName);

	pendingAppendOnlyMirrorResyncIntentCount = 0;
}

static void
AppendOnlyMirrorResyncEofs_InitKey(AppendOnlyMirrorResyncEofsKey *key,
								   RelFileNode *relFileNode,
								   int32 segmentFileNum,
								   int nestLevel)
{
	MemSet(key, 0, sizeof(AppendOnlyMirrorResyncEofsKey));
	key->relFileNode = *relFileNode;
	key->segmentFileNum = segmentFileNum;
	key->nestLevel = nestLevel;
}

void
AppendOnlyMirrorResyncEofs_Merge(RelFileNode *relFileNode,
								 int32 segmentFileNum,
								 int nestLevel, /* Transaction nesting level. */
								 char *relationName,
								 ItemPointer persistentTid,
								 int64 persistentSerialNum,
								 bool mirrorCatchupRequired,
								 MirrorDataLossTrackingState mirrorDataLossTrackingState,
								 int64 mirrorDataLossTrackingSessionNum,
								 int64 mirrorNewEof)
{
	int64		previousMirrorNewEof = 0;

	AppendOnlyMirrorResyncEofsKey key;
	AppendOnlyMirrorResyncEofs *entry;
	bool		found;

	if (AppendOnlyMirrorResyncEofsTable == NULL)
		AppendOnlyMirrorResyncEofs_HashTableInit();

	AppendOnlyMirrorResyncEofs_InitKey(
									   &key,
									   relFileNode,
									   segmentFileNum,
									   nestLevel);

	entry =
		(AppendOnlyMirrorResyncEofs *)
		hash_search(AppendOnlyMirrorResyncEofsTable,
					(void *) &key,
					HASH_ENTER,
					&found);

	if (!found)
	{
		entry->relationName = MemoryContextStrdup(TopMemoryContext, relationName);
		entry->persistentSerialNum = persistentSerialNum;
		entry->persistentTid = *persistentTid;
		entry->didIncrementCommitCount = false;
		entry->isDistributedTransaction = false;
		entry->gid[0] = '\0';
		entry->mirrorCatchupRequired = mirrorCatchupRequired;
		entry->mirrorDataLossTrackingState = mirrorDataLossTrackingState;
		entry->mirrorDataLossTrackingSessionNum = mirrorDataLossTrackingSessionNum;
		entry->mirrorNewEof = mirrorNewEof;
	}
	else
	{
		previousMirrorNewEof = entry->mirrorNewEof;

		/*
		 * UNDONE: What is the purpose of this IF stmt?  Shouldn't we always
		 * set the new EOF?
		 */
		if (mirrorNewEof > entry->mirrorNewEof)
			entry->mirrorNewEof = mirrorNewEof;

		/*
		 * We adopt the newer FileRep state because we accurately track the
		 * state of mirror data.  For example, the first write session might
		 * have had loss because the mirror was down.  But then the second
		 * write session discovered we were in sync and copied both the first
		 * and second write session to the mirror and flushed it.
		 */
		entry->mirrorCatchupRequired = mirrorCatchupRequired;
		entry->mirrorDataLossTrackingState = mirrorDataLossTrackingState;
		entry->mirrorDataLossTrackingSessionNum = mirrorDataLossTrackingSessionNum;
	}

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager: %s Append-Only mirror resync eofs entry: %u/%u/%u, segment file #%d, relation name '%s' (transaction nest level %d, persistent TID %s, persistent serial number " INT64_FORMAT ", "
			 "mirror data loss tracking (state '%s', session num " INT64_FORMAT "), "
			 "previous mirror new EOF " INT64_FORMAT ", input mirror new EOF " INT64_FORMAT ", saved mirror new EOF " INT64_FORMAT ")",
			 (found ? "Merge" : "New"),
			 entry->key.relFileNode.spcNode,
			 entry->key.relFileNode.dbNode,
			 entry->key.relFileNode.relNode,
			 entry->key.segmentFileNum,
			 (entry->relationName == NULL ? "<null>" : entry->relationName),
			 entry->key.nestLevel,
			 ItemPointerToString(&entry->persistentTid),
			 entry->persistentSerialNum,
			 MirrorDataLossTrackingState_Name(mirrorDataLossTrackingState),
			 mirrorDataLossTrackingSessionNum,
			 previousMirrorNewEof,
			 mirrorNewEof,
			 entry->mirrorNewEof);
}

static void
AppendOnlyMirrorResyncEofs_Remove(char *procName,
								  AppendOnlyMirrorResyncEofs *entry)
{
	Assert(AppendOnlyMirrorResyncEofsTable != NULL);

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager (%s): Remove Append-Only mirror resync eofs entry: "
			 "%u/%u/%u, segment file #%d, relation name '%s' (transaction nest level %d, persistent TID %s, persistent serial number " INT64_FORMAT ", mirror catchup required %s, saved mirror new EOF " INT64_FORMAT ")",
			 procName,
			 entry->key.relFileNode.spcNode,
			 entry->key.relFileNode.dbNode,
			 entry->key.relFileNode.relNode,
			 entry->key.segmentFileNum,
			 (entry->relationName == NULL ? "<null>" : entry->relationName),
			 entry->key.nestLevel,
			 ItemPointerToString(&entry->persistentTid),
			 entry->persistentSerialNum,
			 (entry->mirrorCatchupRequired ? "true" : "false"),
			 entry->mirrorNewEof);

	if (entry->relationName != NULL)
		pfree(entry->relationName);

	hash_search(AppendOnlyMirrorResyncEofsTable,
				(void *) &entry->key,
				HASH_REMOVE,
				NULL);
}

static void
AppendOnlyMirrorResyncEofs_Promote(AppendOnlyMirrorResyncEofs *entry,
								   int newNestLevel)
{
	Assert(AppendOnlyMirrorResyncEofsTable != NULL);

	AppendOnlyMirrorResyncEofs_Merge(&entry->key.relFileNode,
									 entry->key.segmentFileNum,
									 newNestLevel,
									 entry->relationName,
									 &entry->persistentTid,
									 entry->persistentSerialNum,
									 entry->mirrorCatchupRequired,
									 entry->mirrorDataLossTrackingState,
									 entry->mirrorDataLossTrackingSessionNum,
									 entry->mirrorNewEof);

	AppendOnlyMirrorResyncEofs_Remove("AppendOnlyMirrorResyncEofs_Promote",
									  entry);
}

void
AppendOnlyMirrorResyncEofs_RemoveForDrop(RelFileNode *relFileNode,
										 int32 segmentFileNum,
										 int nestLevel) /* Transaction nesting
														 * level. */
{
	AppendOnlyMirrorResyncEofsKey key;
	AppendOnlyMirrorResyncEofs *entry;
	bool		found;

	if (AppendOnlyMirrorResyncEofsTable == NULL)
		return;

	AppendOnlyMirrorResyncEofs_InitKey(&key,
									   relFileNode,
									   segmentFileNum,
									   nestLevel);

	entry =
		(AppendOnlyMirrorResyncEofs *)
		hash_search(AppendOnlyMirrorResyncEofsTable,
					(void *) &key,
					HASH_FIND,
					&found);
	if (found)
		AppendOnlyMirrorResyncEofs_Remove("AppendOnlyMirrorResyncEofs_RemoveForDrop",
										  entry);
}

void
smgrDoAppendOnlyResyncEofs(bool forCommit)
{
	HASH_SEQ_STATUS iterateStatus;
	AppendOnlyMirrorResyncEofs *entry;

	AppendOnlyMirrorResyncEofs *entryExample = NULL;

	int			appendOnlyMirrorResyncEofsCount;

	if (AppendOnlyMirrorResyncEofsTable == NULL)
		return;

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager: Enter Append-Only mirror resync eofs list entries (Append-Only commit work count %d)",
			 FileRepPrimary_GetAppendOnlyCommitWorkCount());

	hash_seq_init(
				  &iterateStatus,
				  AppendOnlyMirrorResyncEofsTable);

	appendOnlyMirrorResyncEofsCount = 0;
	while ((entry = hash_seq_search(&iterateStatus)) != NULL)
	{
		if (entryExample == NULL)
		{
			entryExample = entry;
		}

		if (forCommit)
		{
			PersistentFileSysObj_UpdateAppendOnlyMirrorResyncEofs(&entry->key.relFileNode,
																  entry->key.segmentFileNum,
																  &entry->persistentTid,
																  entry->persistentSerialNum,
																  entry->mirrorCatchupRequired,
																  entry->mirrorNewEof,
																   /* recovery */ false,
																   /* flushToXLog */ false);
		}
		else
		{
			/*
			 * Abort case.
			 */
			if (entry->didIncrementCommitCount)
			{
				int			systemAppendOnlyCommitWorkCount;

				LWLockAcquire(FileRepAppendOnlyCommitCountLock, LW_EXCLUSIVE);

				systemAppendOnlyCommitWorkCount =
					FileRepPrimary_FinishedAppendOnlyCommitWork(1);

				if (entry->isDistributedTransaction)
				{
					PrepareDecrAppendOnlyCommitWork(entry->gid);
				}

				if (Debug_persistent_print ||
					Debug_persistent_appendonly_commit_count_print)
					elog(Persistent_DebugPrintLevel(),
						 "Storage Manager: Append-Only Mirror Resync EOFs decrementing commit work for aborted transaction "
						 "(system count %d). "
						 "Relation %u/%u/%u, segment file #%d (persistent serial num " INT64_FORMAT ", TID %s)	",
						 systemAppendOnlyCommitWorkCount,
						 entry->key.relFileNode.spcNode,
						 entry->key.relFileNode.dbNode,
						 entry->key.relFileNode.relNode,
						 entry->key.segmentFileNum,
						 entry->persistentSerialNum,
						 ItemPointerToString(&entry->persistentTid));

				pendingAppendOnlyMirrorResyncIntentCount--;

				LWLockRelease(FileRepAppendOnlyCommitCountLock);
			}
		}

		if (Debug_persistent_print ||
			Debug_persistent_appendonly_commit_count_print)
			elog(Persistent_DebugPrintLevel(),
				 "Storage Manager: Append-Only mirror resync eofs list entry #%d: %u/%u/%u, segment file #%d, relation name '%s' "
				 "(forCommit %s, persistent TID %s, persistent serial number " INT64_FORMAT ", mirror catchup required %s,  mirror new EOF " INT64_FORMAT ")",
				 appendOnlyMirrorResyncEofsCount,
				 entry->key.relFileNode.spcNode,
				 entry->key.relFileNode.dbNode,
				 entry->key.relFileNode.relNode,
				 entry->key.segmentFileNum,
				 (entry->relationName == NULL ? "<null>" : entry->relationName),
				 (forCommit ? "true" : "false"),
				 ItemPointerToString(&entry->persistentTid),
				 entry->persistentSerialNum,
				 (entry->mirrorCatchupRequired ? "true" : "false"),
				 entry->mirrorNewEof);

		appendOnlyMirrorResyncEofsCount++;
	}

	/*
	 * If we collected Append-Only mirror resync EOFs and bumped the intent
	 * count, we need to decrement the counts as part of our end transaction
	 * work here.
	 */
	if (pendingAppendOnlyMirrorResyncIntentCount > 0)
	{
		MIRRORED_LOCK_DECLARE;

		int			oldSystemAppendOnlyCommitWorkCount;
		int			newSystemAppendOnlyCommitWorkCount;
		int			resultSystemAppendOnlyCommitWorkCount;

		if (appendOnlyMirrorResyncEofsCount != pendingAppendOnlyMirrorResyncIntentCount)
			elog(ERROR, "Pending Append-Only Mirror Resync EOFs intent count mismatch (pending %d, table count %d)",
				 pendingAppendOnlyMirrorResyncIntentCount,
				 appendOnlyMirrorResyncEofsCount);

		if (entryExample == NULL)
			elog(ERROR, "Not expecting an empty Append-Only Mirror Resync hash table when the local intent count is non-zero (%d)",
				 pendingAppendOnlyMirrorResyncIntentCount);

		MIRRORED_LOCK;
/*NOTE: When we use the MirroredLock for the whole routine, this can go. */

		LWLockAcquire(FileRepAppendOnlyCommitCountLock, LW_EXCLUSIVE);

		oldSystemAppendOnlyCommitWorkCount = FileRepPrimary_GetAppendOnlyCommitWorkCount();

		newSystemAppendOnlyCommitWorkCount =
			oldSystemAppendOnlyCommitWorkCount -
			pendingAppendOnlyMirrorResyncIntentCount;

		if (newSystemAppendOnlyCommitWorkCount < 0)
			elog(ERROR,
				 "Append-Only Mirror Resync EOFs intent count would go negative "
				 "(system count %d, entry count %d).  "
				 "Example relation %u/%u/%u, segment file #%d (persistent serial num " INT64_FORMAT ", TID %s)",
				 oldSystemAppendOnlyCommitWorkCount,
				 pendingAppendOnlyMirrorResyncIntentCount,
				 entryExample->key.relFileNode.spcNode,
				 entryExample->key.relFileNode.dbNode,
				 entryExample->key.relFileNode.relNode,
				 entryExample->key.segmentFileNum,
				 entryExample->persistentSerialNum,
				 ItemPointerToString(&entryExample->persistentTid));

		resultSystemAppendOnlyCommitWorkCount =
			FileRepPrimary_FinishedAppendOnlyCommitWork(pendingAppendOnlyMirrorResyncIntentCount);

		/*
		 * Should match since we are under FileRepAppendOnlyCommitCountLock
		 * EXCLUSIVE.
		 */
		Assert(newSystemAppendOnlyCommitWorkCount == resultSystemAppendOnlyCommitWorkCount);

		pendingAppendOnlyMirrorResyncIntentCount = 0;

		if (Debug_persistent_print ||
			Debug_persistent_appendonly_commit_count_print)
			elog(Persistent_DebugPrintLevel(),
				 "Storage Manager: Append-Only Mirror Resync EOFs intent count finishing %s work with system count %d remaining "
				 "(enter system count %d, entry count %d, result system count %d).  "
				 "Example relation %u/%u/%u, segment file #%d (persistent serial num " INT64_FORMAT ", TID %s)",
				 (forCommit ? "commit" : "abort"),
				 newSystemAppendOnlyCommitWorkCount,
				 oldSystemAppendOnlyCommitWorkCount,
				 pendingAppendOnlyMirrorResyncIntentCount,
				 resultSystemAppendOnlyCommitWorkCount,
				 entryExample->key.relFileNode.spcNode,
				 entryExample->key.relFileNode.dbNode,
				 entryExample->key.relFileNode.relNode,
				 entryExample->key.segmentFileNum,
				 entryExample->persistentSerialNum,
				 ItemPointerToString(&entryExample->persistentTid));

		LWLockRelease(FileRepAppendOnlyCommitCountLock);

		MIRRORED_UNLOCK;
	}
}

int
smgrGetAppendOnlyMirrorResyncEofs(EndXactRecKind endXactRecKind,
								  PersistentEndXactAppendOnlyMirrorResyncEofs **ptr)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	int			nentries;
	PersistentEndXactAppendOnlyMirrorResyncEofs *rptr;
	HASH_SEQ_STATUS iterateStatus;
	AppendOnlyMirrorResyncEofs *entry;
	int			entryIndex;

	if (endXactRecKind == EndXactRecKind_Abort)
	{
		/*
		 * No Append-Only Mirror Resync EOF information needed on abort.
		 */
		*ptr = NULL;
		return 0;
	}

	nentries = 0;

	if (AppendOnlyMirrorResyncEofsTable != NULL)
	{
		hash_seq_init(&iterateStatus,
					  AppendOnlyMirrorResyncEofsTable);

		while ((entry = hash_seq_search(&iterateStatus)) != NULL)
		{
			if (entry->key.nestLevel >= nestLevel)
				nentries++;
		}
	}
	if (nentries == 0)
	{
		*ptr = NULL;
		return 0;
	}

	if (Debug_persistent_print ||
		Debug_persistent_appendonly_commit_count_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager: Get Append-Only mirror resync eofs list entries (current transaction nest level %d, Append-Only commit work system count %d)",
			 nestLevel,
			 FileRepPrimary_GetAppendOnlyCommitWorkCount());

	rptr = (PersistentEndXactAppendOnlyMirrorResyncEofs *)
		palloc(nentries * sizeof(PersistentEndXactAppendOnlyMirrorResyncEofs));
	*ptr = rptr;
	entryIndex = 0;
	hash_seq_init(&iterateStatus, AppendOnlyMirrorResyncEofsTable);

	while ((entry = hash_seq_search(&iterateStatus)) != NULL)
	{
		MIRRORED_LOCK_DECLARE;

		bool		returned;
		int			resultSystemAppendOnlyCommitCount;

		returned = false;
		if (entry->key.nestLevel >= nestLevel)
		{
			MIRRORED_LOCK;

			MirroredAppendOnly_EndXactCatchup(entryIndex,
											  &entry->key.relFileNode,
											  entry->key.segmentFileNum,
											  entry->key.nestLevel,
											  entry->relationName,
											  &entry->persistentTid,
											  entry->persistentSerialNum,
											  &mirroredLockLocalVars,
											  entry->mirrorCatchupRequired,
											  entry->mirrorDataLossTrackingState,
											  entry->mirrorDataLossTrackingSessionNum,
											  entry->mirrorNewEof);

			/*
			 * See if the mirror situation for this Append-Only segment file
			 * has changed since we flushed it to disk.
			 */
			rptr->relFileNode = entry->key.relFileNode;
			rptr->segmentFileNum = entry->key.segmentFileNum;

			rptr->persistentTid = entry->persistentTid;
			rptr->persistentSerialNum = entry->persistentSerialNum;

			if (entry->mirrorCatchupRequired)
			{
				rptr->mirrorLossEof = INT64CONST(-1);
			}
			else
			{
				rptr->mirrorLossEof = entry->mirrorNewEof;
			}
			rptr->mirrorNewEof = entry->mirrorNewEof;

			rptr++;
			returned = true;

			START_CRIT_SECTION();

			LWLockAcquire(FileRepAppendOnlyCommitCountLock, LW_EXCLUSIVE);

			resultSystemAppendOnlyCommitCount =
				FileRepPrimary_IntentAppendOnlyCommitWork();

			/* Set this inside the Critical Section. */
			entry->didIncrementCommitCount = true;

			if (endXactRecKind == EndXactRecKind_Prepare)
			{
				char		gid[TMGIDSIZE];

				if (!getDistributedTransactionIdentifier(gid))
					elog(ERROR, "Unable to obtain gid during prepare");

				PrepareIntentAppendOnlyCommitWork(gid);

				entry->isDistributedTransaction = true;
				memcpy(entry->gid, gid, TMGIDSIZE);
			}

			pendingAppendOnlyMirrorResyncIntentCount++;

		}
		else
		{
			MIRRORED_LOCK;

			START_CRIT_SECTION();

			LWLockAcquire(FileRepAppendOnlyCommitCountLock, LW_EXCLUSIVE);

			resultSystemAppendOnlyCommitCount =
				FileRepPrimary_GetAppendOnlyCommitWorkCount();
		}

		if (Debug_persistent_print ||
			Debug_persistent_appendonly_commit_count_print)
		{
			if (entry->relationName == NULL)
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: Get Append-Only mirror resync eofs list entry #%d: %u/%u/%u, segment file #%d "
					 "(returned %s, result system Append-Only commit count %d, transaction nest level %d, persistent TID %s, persistent serial number " INT64_FORMAT ", mirror catchup required %s, mirror new EOF " INT64_FORMAT ")",
					 entryIndex,
					 entry->key.relFileNode.spcNode,
					 entry->key.relFileNode.dbNode,
					 entry->key.relFileNode.relNode,
					 entry->key.segmentFileNum,
					 (returned ? "true" : "false"),
					 resultSystemAppendOnlyCommitCount,
					 entry->key.nestLevel,
					 ItemPointerToString(&entry->persistentTid),
					 entry->persistentSerialNum,
					 (entry->mirrorCatchupRequired ? "true" : "false"),
					 entry->mirrorNewEof);
			else
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: Get Append-Only mirror resync eofs list entry #%d: %u/%u/%u, segment file #%d, relation name '%s' "
					 "(returned %s, result system Append-Only commit count %d, transaction nest level %d, persistent TID %s, persistent serial number " INT64_FORMAT ", mirror catchup required %s, mirror new EOF " INT64_FORMAT ")",
					 entryIndex,
					 entry->key.relFileNode.spcNode,
					 entry->key.relFileNode.dbNode,
					 entry->key.relFileNode.relNode,
					 entry->key.segmentFileNum,
					 entry->relationName,
					 (returned ? "true" : "false"),
					 resultSystemAppendOnlyCommitCount,
					 entry->key.nestLevel,
					 ItemPointerToString(&entry->persistentTid),
					 entry->persistentSerialNum,
					 (entry->mirrorCatchupRequired ? "true" : "false"),
					 entry->mirrorNewEof);
		}

		LWLockRelease(FileRepAppendOnlyCommitCountLock);

		END_CRIT_SECTION();

		MIRRORED_UNLOCK;

		entryIndex++;
	}
	return nentries;
}

/*
 * smgrIsAppendOnlyMirrorResyncEofs() -- Returns true if there Append-Only Mirror Resync
 * EOF work that needs to be done post-commit or post-abort work.
 *
 * Note that the list does not include anything scheduled for termination
 * by upper-level transactions.
 */
bool
smgrIsAppendOnlyMirrorResyncEofs(EndXactRecKind endXactRecKind)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	HASH_SEQ_STATUS iterateStatus;
	AppendOnlyMirrorResyncEofs *entry;

	if (AppendOnlyMirrorResyncEofsTable == NULL)
	{
		return false;
	}

	hash_seq_init(&iterateStatus, AppendOnlyMirrorResyncEofsTable);

	while ((entry = hash_seq_search(&iterateStatus)) != NULL)
	{
		if (entry->key.nestLevel >= nestLevel)
		{
			/* Deregister seq scan and exit early. */
			hash_seq_term(&iterateStatus);
			return true;
		}
	}

	return false;
}


bool
smgrgetappendonlyinfo(RelFileNode *relFileNode,
					  int32 segmentFileNum,
					  char *relationName,
					  bool *mirrorCatchupRequired,
					  MirrorDataLossTrackingState *mirrorDataLossTrackingState,
					  int64 *mirrorDataLossTrackingSessionNum)
{
	int			nestLevel;

	*mirrorCatchupRequired = false;
	*mirrorDataLossTrackingState = (MirrorDataLossTrackingState) -1;
	*mirrorDataLossTrackingSessionNum = 0;

	if (AppendOnlyMirrorResyncEofsTable == NULL)
		AppendOnlyMirrorResyncEofs_HashTableInit();

	/*
	 * The hash table is keyed by RelFileNode, segmentFileNum, AND transaction
	 * nesting level...
	 *
	 * So, we need to search more indirectly by walking down the transaction
	 * nesting levels.
	 */
	nestLevel = GetCurrentTransactionNestLevel();
	while (true)
	{
		AppendOnlyMirrorResyncEofsKey key;
		AppendOnlyMirrorResyncEofs *entry;
		bool		found;

		AppendOnlyMirrorResyncEofs_InitKey(&key,
										   relFileNode,
										   segmentFileNum,
										   nestLevel);

		entry =
			(AppendOnlyMirrorResyncEofs *)
			hash_search(AppendOnlyMirrorResyncEofsTable,
						(void *) &key,
						HASH_FIND,
						&found);

		if (found)
		{
			Assert(entry != NULL);
			*mirrorCatchupRequired = entry->mirrorCatchupRequired;
			*mirrorDataLossTrackingState = entry->mirrorDataLossTrackingState;
			*mirrorDataLossTrackingSessionNum = entry->mirrorDataLossTrackingSessionNum;
			return true;
		}

		if (nestLevel == 0)
			break;
		nestLevel--;
	}

	return false;
}


void
smgrAppendOnlySubTransAbort(void)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	HASH_SEQ_STATUS iterateStatus;
	AppendOnlyMirrorResyncEofs *entry;

	/*
	 * Throw away sub-transaction Append-Only mirror resync EOFs.
	 */
	hash_seq_init(&iterateStatus,
				  AppendOnlyMirrorResyncEofsTable);

	while ((entry = hash_seq_search(&iterateStatus)) != NULL)
	{
		if (entry->key.nestLevel >= nestLevel)
		{
			AppendOnlyMirrorResyncEofs_Remove("smgrSubTransAbort",
											  entry);
		}
	}
}

void
AtSubCommit_smgr_appendonly(void)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	HASH_SEQ_STATUS iterateStatus;
	AppendOnlyMirrorResyncEofs *entry;

	hash_seq_init(&iterateStatus, AppendOnlyMirrorResyncEofsTable);

	while ((entry = hash_seq_search(&iterateStatus)) != NULL)
	{
		if (entry->key.nestLevel >= nestLevel)
			AppendOnlyMirrorResyncEofs_Promote(entry, nestLevel - 1);
	}
}



void
smgrappendonlymirrorresynceofs(RelFileNode *relFileNode,
							   int32 segmentFileNum,
							   char *relationName,
							   ItemPointer persistentTid,
							   int64 persistentSerialNum,
							   bool mirrorCatchupRequired,
							   MirrorDataLossTrackingState mirrorDataLossTrackingState,
							   int64 mirrorDataLossTrackingSessionNum,
							   int64 mirrorNewEof)
{
	Assert(mirrorNewEof > 0);

	AppendOnlyMirrorResyncEofs_Merge(relFileNode,
									 segmentFileNum,
									 GetCurrentTransactionNestLevel(),
									 relationName,
									 persistentTid,
									 persistentSerialNum,
									 mirrorCatchupRequired,
									 mirrorDataLossTrackingState,
									 mirrorDataLossTrackingSessionNum,
									 mirrorNewEof);
}
