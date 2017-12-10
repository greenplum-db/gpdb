/*-------------------------------------------------------------------------
 *
 * storage.c
 *	  code to create and destroy physical storage for relations
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/storage.c,v 1.6 2009/06/11 14:48:55 momjian Exp $
 *
 * NOTES
 *	  Some of this code used to be in storage/smgr/smgr.c, and the
 *	  function names still reflect that.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/visibilitymap.h"
#include "access/xact.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/storage.h"
#include "storage/freespace.h"
#include "storage/smgr.h"
#include "utils/memutils.h"
#include "utils/rel.h"

#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistentrelation.h"
#include "utils/faultinjector.h"
#include "storage/lmgr.h"
#include "storage/smgr_ao.h"

/*
 * We keep a list of all relations (represented as RelFileNode values)
 * that have been created or deleted in the current transaction.  When
 * a relation is created, we create the physical file immediately, but
 * remember it so that we can delete the file again if the current
 * transaction is aborted.	Conversely, a deletion request is NOT
 * executed immediately, but is just entered in the list.  When and if
 * the transaction commits, we can delete the physical file.
 *
 * To handle subtransactions, every entry is marked with its transaction
 * nesting level.  At subtransaction commit, we reassign the subtransaction's
 * entries to the parent nesting level.  At subtransaction abort, we can
 * immediately execute the abort-time actions for all entries of the current
 * nesting level.
 *
 * NOTE: the list is kept in TopMemoryContext to be sure it won't disappear
 * unbetimes.  It'd probably be OK to keep it in TopTransactionContext,
 * but I'm being paranoid.
 */

typedef struct PendingDelete
{
	PersistentFileSysObjName fsObjName; /* File-system object that may need to be deleted */

	PersistentFileSysRelStorageMgr relStorageMgr;

	char       *relationName;

	bool		isLocalBuf;				/* CDB: true => uses local buffer mgr */
	bool		bufferPoolBulkLoad;
	bool		dropForCommit;			/* T=delete at commit; F=delete at abort */
	bool		sameTransCreateDrop;	/* Collapsed create-delete? */
	ItemPointerData persistentTid;
	int64		persistentSerialNum;
	int			nestLevel;				/* xact nesting level of request */
	struct PendingDelete *next;			/* linked-list link */
} PendingDelete;

static PendingDelete *pendingDeletes = NULL; /* head of linked list */
static int pendingDeletesCount = 0;
static bool pendingDeletesSorted = false;
static bool pendingDeletesPerformed = true;

typedef PendingDelete *PendingDeletePtr;

static PendingDelete *
PendingDelete_AddEntry(PersistentFileSysObjName *fsObjName,
					   ItemPointer persistentTid,
					   int64 persistentSerialNum,
					   bool dropForCommit)
{
	PendingDelete *pending;

	if (!ItemPointerIsValid(persistentTid))
		elog(ERROR, "tried to delete a relation with invalid persistent TID");

	/* Add the filespace to the list of stuff to delete at abort */
	pending = (PendingDelete *)
		MemoryContextAllocZero(TopMemoryContext, sizeof(PendingDelete));

	pending->fsObjName = *fsObjName;
	pending->isLocalBuf = false;
	pending->relationName = NULL;
	pending->relStorageMgr = PersistentFileSysRelStorageMgr_None;
	pending->dropForCommit = dropForCommit;
	pending->sameTransCreateDrop = false;
	pending->nestLevel = GetCurrentTransactionNestLevel();
	pending->persistentTid = *persistentTid;
	pending->persistentSerialNum = persistentSerialNum;
	pending->next = pendingDeletes;
	pendingDeletes = pending;
	pendingDeletesCount++;
	pendingDeletesSorted = false;
	pendingDeletesPerformed = false;

	return pending;
}

static PendingDelete *
PendingDelete_AddCreatePendingEntry(PersistentFileSysObjName *fsObjName,
									ItemPointer persistentTid,
									int64 persistentSerialNum)
{
	return PendingDelete_AddEntry(fsObjName, persistentTid, persistentSerialNum,
								  /* dropForCommit */ false);
}

void
PendingDelete_AddCreatePendingRelationEntry(PersistentFileSysObjName *fsObjName,
											ItemPointer persistentTid,
											int64 *persistentSerialNum,
											PersistentFileSysRelStorageMgr relStorageMgr,
											char *relationName,
											bool isLocalBuf,
											bool bufferPoolBulkLoad)
{
	PendingDelete *pending;

	pending = PendingDelete_AddCreatePendingEntry(fsObjName,
												  persistentTid,
												  *persistentSerialNum);

	pending->relStorageMgr = relStorageMgr;
	pending->relationName = MemoryContextStrdup(TopMemoryContext, relationName);
	pending->isLocalBuf = isLocalBuf;       /*CDB*/
	pending->bufferPoolBulkLoad = bufferPoolBulkLoad;
}

/*
 * MPP-18228
 * Wrapper to call above function from cdb files
 */
void
PendingDelete_AddCreatePendingEntryWrapper(PersistentFileSysObjName *fsObjName,
										   ItemPointer persistentTid,
										   int64 persistentSerialNum)
{
       PendingDelete_AddCreatePendingEntry(fsObjName,
										   persistentTid,
										   persistentSerialNum);
}

static PendingDelete *
PendingDelete_AddDropEntry(PersistentFileSysObjName *fsObjName,
						   ItemPointer persistentTid,
						   int64 persistentSerialNum)
{
	return PendingDelete_AddEntry(fsObjName, persistentTid, persistentSerialNum,
								  /* dropForCommit */ true);
}

static inline PersistentEndXactFileSysAction
PendingDelete_Action(PendingDelete *pendingDelete)
{
	if (pendingDelete->dropForCommit)
	{
		return (pendingDelete->sameTransCreateDrop ?
				PersistentEndXactFileSysAction_AbortingCreateNeeded :
				PersistentEndXactFileSysAction_Drop);
	}
	else
		return PersistentEndXactFileSysAction_Create;
}

static void
PendingDelete_Free(PendingDelete **ele)
{
	if ((*ele)->relationName != NULL)
		pfree((*ele)->relationName);

	pfree(*ele);

	*ele = NULL;
}


/*
 * Declarations for smgr-related XLOG records
 *
 * Note: we log file creation and truncation here, but logging of deletion
 * actions is handled by xact.c, because it is part of transaction commit.
 */

/* XLOG gives us high 4 bits */
#define XLOG_SMGR_CREATE	0x10
#define XLOG_SMGR_TRUNCATE	0x20

typedef struct xl_smgr_create
{
	RelFileNode rnode;
} xl_smgr_create;

typedef struct xl_smgr_truncate
{
	BlockNumber blkno;
	RelFileNode rnode;
	ItemPointerData persistentTid;
	int64		persistentSerialNum;
} xl_smgr_truncate;


static void smgrDoDeleteActions(PendingDelete **list, int *listCount, bool forCommit);


/*
 * RelationCreateStorage
 *		Create physical storage for a relation.
 *
 * Create the underlying disk file storage for the relation. This only
 * creates the main fork; additional forks are created lazily by the
 * modules that need them.
 *
 * This function is transactional. The creation is WAL-logged, and if the
 * transaction aborts later on, the storage will be destroyed.
 */
void
RelationCreateStorage(RelFileNode rnode, bool isLocalBuf,
					  char *relationName, /* For tracing only. Can be NULL in some execution paths. */
					  MirrorDataLossTrackingState mirrorDataLossTrackingState,
					  int64 mirrorDataLossTrackingSessionNum,
					  bool *mirrorDataLossOccurred) /* FIXME: is this arg still needed? */
{
	SMgrRelation srel;

	srel = smgropen(rnode);
	smgrmirroredcreate(srel,
					   relationName,
					   mirrorDataLossTrackingState,
					   mirrorDataLossTrackingSessionNum,
					   false, /* ignoreAlreadyExists */
					   mirrorDataLossOccurred);

/*
 * Disable generation of XLOG_SMGR_CREATE until persistent tables and MMXLOG
 * records are removed from Greenplum.  Multipass crash recovery using
 * persistent tables will handle relation file creation on primary or master.
 * On Standby, MMXLOG_CREATE_FILE xlog record replayed in mmxlog_redo will take
 * care of things.  In filerep, persistent tables guide crash recovery on both,
 * primary and mirror.
 */
#if 0
	if (!isLocalBuf)
	{
		XLogRecPtr  lsn;
		XLogRecData rdata;
		xl_smgr_create xlrec;

		/*
		 * Make an XLOG entry showing the file creation.  If we abort, the
		 * file will be dropped at abort time.
		 */
		xlrec.rnode = rnode;

		rdata.data = (char *) &xlrec;
		rdata.len = sizeof(xlrec);
		rdata.buffer = InvalidBuffer;
		rdata.next = NULL;

		lsn = XLogInsert(RM_SMGR_ID, XLOG_SMGR_CREATE, &rdata);
	}
#endif
}

void
smgrcreatepending(RelFileNode *relFileNode,
				  int32 segmentFileNum,
				  PersistentFileSysRelStorageMgr relStorageMgr,
				  PersistentFileSysRelBufpoolKind relBufpoolKind,
				  MirroredObjectExistenceState mirrorExistenceState,
				  MirroredRelDataSynchronizationState relDataSynchronizationState,
				  char *relationName,
				  ItemPointer persistentTid,
				  int64 *persistentSerialNum,
				  bool isLocalBuf,
				  bool bufferPoolBulkLoad,
				  bool flushToXLog)
{
	/* Add the relation to the list of stuff to delete at abort */
	PersistentRelation_AddCreatePending(relFileNode,
										segmentFileNum,
										relStorageMgr,
										relBufpoolKind,
										bufferPoolBulkLoad,
										mirrorExistenceState,
										relDataSynchronizationState,
										relationName,
										persistentTid,
										persistentSerialNum,
										flushToXLog,
										isLocalBuf);
}


/*
 *	smgrcreatefilespacedirpending() -- Create a new filespace directory.
 */
void
smgrcreatefilespacedirpending(Oid filespaceOid,
							  int16 primaryDbId,
							  char *primaryFilespaceLocation,
							  int16 mirrorDbId,
							  char *mirrorFilespaceLocation,
							  MirroredObjectExistenceState mirrorExistenceState,
							  ItemPointer persistentTid,
							  int64 *persistentSerialNum,
							  bool flushToXLog)
{
	PersistentFilespace_MarkCreatePending(filespaceOid,
										  primaryDbId,
										  primaryFilespaceLocation,
										  mirrorDbId,
										  mirrorFilespaceLocation,
										  mirrorExistenceState,
										  persistentTid,
										  persistentSerialNum,
										  flushToXLog);
}


/*
 *	smgrcreatetablespacedirpending() -- Create a new tablespace directory.
 */
void
smgrcreatetablespacedirpending(TablespaceDirNode *tablespaceDirNode,
							   MirroredObjectExistenceState mirrorExistenceState,
							   ItemPointer persistentTid,
							   int64 *persistentSerialNum,
							   bool flushToXLog)
{
	PersistentTablespace_MarkCreatePending(tablespaceDirNode->filespace,
										   tablespaceDirNode->tablespace,
										   mirrorExistenceState,
										   persistentTid,
										   persistentSerialNum,
										   flushToXLog);
}


/*
 *	smgrcreatedbdirpending() -- Create a new database directory.
 */
void
smgrcreatedbdirpending(DbDirNode *dbDirNode,
					   MirroredObjectExistenceState mirrorExistenceState,
					   ItemPointer persistentTid,
					   int64 *persistentSerialNum,
					   bool flushToXLog)
{
	PersistentDatabase_MarkCreatePending(dbDirNode,
										 mirrorExistenceState,
										 persistentTid,
										 persistentSerialNum,
										 flushToXLog);
}


/*
 * RelationDropStorage
 *		Schedule unlinking of physical storage at transaction commit.
 */
void
RelationDropStorage(RelFileNode *relFileNode,
					int32 segmentFileNum,
					PersistentFileSysRelStorageMgr relStorageMgr,
					bool isLocalBuf,
					char *relationName,
					ItemPointer persistentTid,
					int64 persistentSerialNum)
{
	SUPPRESS_ERRCONTEXT_DECLARE;

	PersistentFileSysObjName fsObjName;

	PendingDelete *pending;


	/* IMPORANT:
	 * ----> Relcache invalidation can close an open smgr <------
	 *
	 * This routine can be called in the context of a relation and rd_smgr being used,
	 * so do not issue elog here without suppressing errcontext.  Otherwise, the heap_open
	 * inside errcontext processing may cause the smgr open to be closed...
	 */

	SUPPRESS_ERRCONTEXT_PUSH();

	PersistentFileSysObjName_SetRelationFile(&fsObjName,
											 relFileNode,
											 segmentFileNum);

	pending = PendingDelete_AddDropEntry(&fsObjName,
										 persistentTid,
										 persistentSerialNum);

	pending->relStorageMgr = relStorageMgr;
	pending->relationName = MemoryContextStrdup(TopMemoryContext, relationName);
	pending->isLocalBuf = isLocalBuf;	/*CDB*/

	if (relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly)
	{
		/*
		 * Remove pending updates for Append-Only mirror resync EOFs, too.
		 *
		 * But only at this transaction level !!!
		 */
		AppendOnlyMirrorResyncEofs_RemoveForDrop(relFileNode,
												 segmentFileNum,
												 GetCurrentTransactionNestLevel());
	}

	SUPPRESS_ERRCONTEXT_POP();

	/* IMPORANT:
	 * ----> Relcache invalidation can close an open smgr <------
	 *
	 * See above.
	 */

	/*
	 * NOTE: if the relation was created in this transaction, it will now be
	 * present in the pending-delete list twice, once with atCommit true and
	 * once with atCommit false.  Hence, it will be physically deleted at end
	 * of xact in either case (and the other entry will be ignored by
	 * smgrDoPendingDeletes, so no error will occur).  We could instead remove
	 * the existing list entry and delete the physical file immediately, but
	 * for now I'll keep the logic simple.
	 */

	// GPDB_84_MERGE_FIXME
	//RelationCloseSmgr(rel);
}

/*
 * RelationTruncate
 *		Physically truncate a relation to the specified number of blocks.
 *
 * This includes getting rid of any buffers for the blocks that are to be
 * dropped.
 */
void
RelationTruncate(Relation rel, BlockNumber nblocks, bool markPersistentAsPhysicallyTruncated)
{
	MIRROREDLOCK_BUFMGR_DECLARE;
	bool		fsm;
	bool		vm;

	// Fetch gp_persistent_relation_node information that will be added to XLOG record.
	RelationFetchGpRelationNodeForXLog(rel);

	if (markPersistentAsPhysicallyTruncated)
	{
		LockRelationForResynchronize(&rel->rd_node, AccessExclusiveLock);

		/*
		 * Fetch gp_persistent_relation_node information so we can mark the persistent entry.
		 */
		RelationFetchGpRelationNodeForXLog(rel);

		if (rel->rd_segfile0_relationnodeinfo.isPresent)
		{
			/*
			 * Since we are deleting 0, 1, or more segments files and possibly lopping off the
			 * end of new last segment file, we need to indicate to resynchronize that
			 * it should use 'Scan Incremental' only.
			 */
			PersistentRelation_MarkBufPoolRelationForScanIncrementalResync(
															&rel->rd_node,
															&rel->rd_segfile0_relationnodeinfo.persistentTid,
															rel->rd_segfile0_relationnodeinfo.persistentSerialNum);
		}
	}

	// -------- MirroredLock ----------
	// NOTE: PersistentFileSysObj_EndXactDrop acquires relation resynchronize lock before MirroredLock, too.
	MIRROREDLOCK_BUFMGR_LOCK;

	/* Open it at the smgr level if not already done */
	RelationOpenSmgr(rel);

	/* Make sure rd_targblock isn't pointing somewhere past end */
	rel->rd_targblock = InvalidBlockNumber;

	/* Truncate the FSM first if it exists */
	fsm = smgrexists(rel->rd_smgr, FSM_FORKNUM);
	if (fsm)
		FreeSpaceMapTruncateRel(rel, nblocks);

	/* Truncate the visibility map too if it exists. */
	vm = smgrexists(rel->rd_smgr, VISIBILITYMAP_FORKNUM);
	if (vm)
		visibilitymap_truncate(rel, nblocks);

	/*
	 * We WAL-log the truncation before actually truncating, which means
	 * trouble if the truncation fails. If we then crash, the WAL replay
	 * likely isn't going to succeed in the truncation either, and cause a
	 * PANIC. It's tempting to put a critical section here, but that cure
	 * would be worse than the disease. It would turn a usually harmless
	 * failure to truncate, that could spell trouble at WAL replay, into a
	 * certain PANIC.
	 */
	if (!rel->rd_istemp)
	{
		/*
		 * Make an XLOG entry showing the file truncation.
		 */
		XLogRecPtr	lsn;
		XLogRecData rdata;
		xl_smgr_truncate xlrec;

		xlrec.blkno = nblocks;
		xlrec.rnode = rel->rd_node;
		xlrec.persistentTid = rel->rd_segfile0_relationnodeinfo.persistentTid;
		xlrec.persistentSerialNum = rel->rd_segfile0_relationnodeinfo.persistentSerialNum;

		rdata.data = (char *) &xlrec;
		rdata.len = sizeof(xlrec);
		rdata.buffer = InvalidBuffer;
		rdata.next = NULL;

		lsn = XLogInsert(RM_SMGR_ID, XLOG_SMGR_TRUNCATE, &rdata);

		/*
		 * Flush, because otherwise the truncation of the main relation might
		 * hit the disk before the WAL record, and the truncation of the FSM
		 * or visibility map. If we crashed during that window, we'd be left
		 * with a truncated heap, but the FSM or visibility map would still
		 * contain entries for the non-existent heap pages.
		 */
		if (fsm || vm)
			XLogFlush(lsn);
	}

	/* Do the real work */
	smgrtruncate(rel->rd_smgr, MAIN_FORKNUM, nblocks, rel->rd_istemp);

	MIRROREDLOCK_BUFMGR_UNLOCK;
	// -------- MirroredLock ----------

	if (markPersistentAsPhysicallyTruncated)
	{
		UnlockRelationForResynchronize(&rel->rd_node, AccessExclusiveLock);
	}
}

/*
 *	smgrschedulermfilespacedir() -- Schedule removing a filespace directory at xact commit.
 *
 *		The filespace directory is marked to be removed from the store if we
 *		successfully commit the current transaction.
 */
void
smgrschedulermfilespacedir(Oid filespaceOid,
						   ItemPointer persistentTid,
						   int64 persistentSerialNum)
{
	PersistentFileSysObjName fsObjName;

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName, filespaceOid);

	PendingDelete_AddDropEntry(&fsObjName, persistentTid, persistentSerialNum);
}

/*
 *	smgrschedulermtablespacedir() -- Schedule removing a tablespace directory at xact commit.
 *
 *		The tablespace directory is marked to be removed from the store if we
 *		successfully commit the current transaction.
 */
void
smgrschedulermtablespacedir(Oid tablespaceOid,
							ItemPointer persistentTid,
							int64 persistentSerialNum)
{
	PersistentFileSysObjName fsObjName;

	PersistentFileSysObjName_SetTablespaceDir(&fsObjName, tablespaceOid);

	PendingDelete_AddDropEntry(&fsObjName,persistentTid, persistentSerialNum);
}

/*
 *	smgrschedulermdbdir() -- Schedule removing a DB directory at xact commit.
 *
 *		The database directory is marked to be removed from the store if we
 *		successfully commit the current transaction.
 */
void
smgrschedulermdbdir(DbDirNode *dbDirNode,
					ItemPointer persistentTid,
					int64 persistentSerialNum)
{
	PersistentFileSysObjName fsObjName;
	Oid			tablespace;
	Oid			database;

	tablespace = dbDirNode->tablespace;
	database = dbDirNode->database;

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName,
											tablespace,
											database);

	PendingDelete_AddDropEntry(&fsObjName,
							   persistentTid,
							   persistentSerialNum);
}




/*
 * A compare function for 2 PendingDelete.
 */
static int
PendingDelete_Compare(const PendingDelete *entry1, const PendingDelete *entry2)
{
	int			cmp;

	cmp = PersistentFileSysObjName_Compare(&entry1->fsObjName,
										   &entry2->fsObjName);
	if (cmp == 0)
	{
		/*
		 * Sort CREATE before DROP for detecting same transaction create-drops.
		 */
		if (entry1->dropForCommit == entry2->dropForCommit)
			return 0;
		else if (entry1->dropForCommit)
			return 1;
		else
			return -1;
	}
	else
		return cmp;
}

/*
 * A compare function for array of PendingDeletePtr for use with qsort.
 */
static int
PendingDeletePtr_Compare(const void *p1, const void *p2)
{
	const PendingDeletePtr *entry1Ptr = (PendingDeletePtr *) p1;
	const PendingDeletePtr *entry2Ptr = (PendingDeletePtr *) p2;
	const PendingDelete *entry1 = *entry1Ptr;
	const PendingDelete *entry2 = *entry2Ptr;

	return PendingDelete_Compare(entry1, entry2);
}

static void
smgrSortDeletesList(PendingDelete **list, int *listCount, int nestLevel)
{
	PendingDeletePtr *ptrArray;
	PendingDelete *current;
	int			i;
	PendingDelete *prev;
	int			collapseCount;

	if (*listCount == 0)
		return;

	ptrArray = (PendingDeletePtr *)
		palloc(*listCount * sizeof(PendingDeletePtr));

	i = 0;
	for (current = *list; current != NULL; current = current->next)
	{
		ptrArray[i++] = current;
	}
	Assert(i == *listCount);

	/*
	 * Sort the list.
	 *
	 * Supports the collapsing of same transaction create-deletes and to be able
	 * to process relations before database directories, etc.
	 */
	qsort(ptrArray,
		  *listCount,
		  sizeof(PendingDeletePtr),
		  PendingDeletePtr_Compare);

	/*
	 * Collapse same transaction create-drops and re-link list.
	 */
	*list = ptrArray[0];
	prev = ptrArray[0];
	collapseCount = 0;

	// Start processing elements after the first one.
	for (i = 1; i < *listCount; i++)
	{
		bool		collapse = false;

		current = ptrArray[i];

		/*
		 * Only do CREATE-DROP collapsing when both are at or below the
		 * requested transaction nest level.
		 */
		if (current->nestLevel >= nestLevel &&
			prev->nestLevel >= nestLevel &&
			(PersistentFileSysObjName_Compare(
								&prev->fsObjName,
								&current->fsObjName) == 0))
		{
			/*
			 * If there are two sequential entries for the same object, it
			 * should be a CREATE-DROP pair (XXX: why?). Sanity check that it
			 * really is. NOTE: We cannot elog(ERROR) here, because that
			 * would leave the list in an inconsistent state.
			 */
			if (prev->dropForCommit)
			{
				collapse = false;
				elog(WARNING, "Expected a CREATE for file-system object name '%s'",
					PersistentFileSysObjName_ObjectName(&prev->fsObjName));
			}
			else if (!current->dropForCommit)
			{
				collapse = false;
				elog(WARNING, "Expected a DROP for file-system object name '%s'",
					PersistentFileSysObjName_ObjectName(&current->fsObjName));
			}
			else
				collapse = true;
		}

		if (collapse)
		{
			prev->dropForCommit = true;				// Make the CREATE a DROP.
			prev->sameTransCreateDrop = true;	// Don't ignore DROP on abort.
			collapseCount++;

			if (Debug_persistent_print)
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: CREATE (transaction level %d) - DROP (transaction level %d) collapse for %s, filter transaction level %d, TID %s, serial " INT64_FORMAT,
					 current->nestLevel,
					 prev->nestLevel,
					 PersistentFileSysObjName_TypeAndObjectName(&current->fsObjName),
					 nestLevel,
					 ItemPointerToString(&current->persistentTid),
					 current->persistentSerialNum);

			PendingDelete_Free(&current);

			// Don't advance prev pointer.
		}
		else
		{
			// Re-link.
			prev->next = current;

			prev = current;
		}
	}
	prev->next = NULL;

	pfree(ptrArray);

	/*
	 * Adjust count.
	 */
	(*listCount) -= collapseCount;

#ifdef suppress
	{
		PendingDelete	*check;
		PendingDelete	*checkPrev;
		int			checkCount;

		checkPrev = NULL;
		checkCount = 0;
		for (check = *list; check != NULL; check = check->next)
		{
			checkCount++;
			if (checkPrev != NULL)
			{
				int cmp;

				cmp = PendingDelete_Compare(
										checkPrev,
										check);
				if (cmp >= 0)
					elog(ERROR, "Not sorted correctly ('%s' >= '%s')",
						 PersistentFileSysObjName_ObjectName(&checkPrev->fsObjName),
						 PersistentFileSysObjName_ObjectName(&check->fsObjName));

			}

			checkPrev = check;
		}

		if (checkCount != *listCount)
			elog(ERROR, "List count does not match (expected %d, found %d)",
			     *listCount, checkCount);
	}
#endif
}


/*
 *	smgrSubTransAbort() -- Take care of relation deletes on sub-transaction abort.
 *
 * We want to clean up a failed subxact immediately.
 */
static void
smgrSubTransAbort(void)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	PendingDelete *pending;
	PendingDelete *prev;
	PendingDelete *next;
	PendingDelete *subTransList;
	int			subTransCount;

	/*
	 * We need to complete this work, or let Crash Recovery complete it.
	 * Unlike AtEOXact_smgr, we need to start critical section here
	 * because after reorganizing the list we end up forgetting the
	 * subTransList if the code errors out.
	 */
	START_CRIT_SECTION();

	subTransList = NULL;
	subTransCount = 0;
	prev = NULL;
	for (pending = pendingDeletes; pending != NULL; pending = next)
	{
		next = pending->next;
		if (pending->nestLevel < nestLevel)
		{
			/* outer-level entries should not be processed yet */
			prev = pending;
		}
		else
		{
			if (prev)
				prev->next = next;
			else
				pendingDeletes = next;

			pendingDeletesCount--;

			// Move to sub-transaction list.
			pending->next = subTransList;
			subTransList = pending;

			subTransCount++;

			/* prev does not change */
		}
	}

	/*
	 * Sort the list in relation, database directory, tablespace, etc order.
	 * And, collapse same transaction create-deletes.
	 */
	smgrSortDeletesList(
					&subTransList,
					&subTransCount,
					nestLevel);

	pendingDeletesSorted = (nestLevel <= 1);

	/*
	 * Do abort actions for the sub-transaction's creates and deletes.
	 */
	smgrDoDeleteActions(
					&subTransList,
					&subTransCount,
					/* forCommit */ false);

	Assert(subTransList == NULL);
	Assert(subTransCount == 0);

	/*
	 * Throw away sub-transaction Append-Only mirror resync EOFs.
	 */
	smgrAppendOnlySubTransAbort();

	END_CRIT_SECTION();
}

/*
 *	AtEOXact_smgr() -- Take care of relation deletes at end of xact.
 *
 * For commit:
 *   1) Physically unlink any relations that were dropped.
 *   2) Change CreatePending relations to Created.
 *
 * ELSE for abort:
 *   1) Change CreatePending relations to DropPending
 *   2) Physicaly unlink the aborted creates.
 */
void
AtEOXact_smgr(bool isCommit)
{
	/*
	 * Sort the list in relation, database directory, tablespace, etc order.
	 * And, collapse same transaction create-deletes.
	 */
	if (!pendingDeletesSorted)
	{
		smgrSortDeletesList(
						&pendingDeletes,
						&pendingDeletesCount,
						/* nestLevel */ 0);

		pendingDeletesSorted = true;
	}

	/*
	 * We need to complete this work, or let Crash Recovery complete it.
	 */
	START_CRIT_SECTION();

	if (!pendingDeletesPerformed)
	{

		/*
		 * Do abort actions for the sub-transaction's creates and deletes.
		 */
		smgrDoDeleteActions(&pendingDeletes,
							&pendingDeletesCount,
							isCommit);

		Assert(pendingDeletes == NULL);
		Assert(pendingDeletesCount == 0);
		pendingDeletesSorted = false;
		pendingDeletesPerformed = true;
	}

	/*
	 * Update the Append-Only mirror resync EOFs.
	 */
	smgrDoAppendOnlyResyncEofs(isCommit);

	/*
	 * Free the Append-Only mirror resync EOFs hash table.
	 */
	AppendOnlyMirrorResyncEofs_HashTableRemove("AtEOXact_smgr");


	END_CRIT_SECTION();
}

static void
smgrDoDeleteActions(PendingDelete **list, int *listCount, bool forCommit)
{
	MIRRORED_LOCK_DECLARE;

	PendingDelete *current;
	int			entryIndex;

	PersistentEndXactFileSysAction action;

	bool		dropPending;
	bool		abortingCreate;

	PersistentFileSysObjStateChangeResult *stateChangeResults;

	if (*listCount == 0)
		stateChangeResults = NULL;
	else
		stateChangeResults =
				(PersistentFileSysObjStateChangeResult*)
						palloc0((*listCount) * sizeof(PersistentFileSysObjStateChangeResult));

	/*
	 * There are two situations where we get here. CommitTransaction()/AbortTransaction() or via
	 * AbortSubTransaction(). In the first case, we have already obtained the MirroredLock and
	 * CheckPointStartLock. In the second case, we have not obtained the locks, so we attempt
	 * to get them to make sure proper lock order is maintained.
	 *
	 * Normally, if a relation lock is needed, it is obtained before the MirroredLock and CheckPointStartLock,
	 * but we have not yet obtained an EXCLUSIVE LockRelationForResynchronize. This lock will be obtained in
	 * PersistentFileSysObj_EndXactDrop(). This is an exception to the normal lock ordering, which is done
	 * to reduce the time that the lock is held, thus allowing a larger window of time for filerep
	 * resynchronization to obtain the lock.
	 */

	/*
	 * We need to do the transition to 'Aborting Create' or 'Drop Pending' and perform
	 * the file-system drop while under one acquistion of the MirroredLock.  Otherwise,
	 * we could race with resynchronize's ReDrop.
	 */
	MIRRORED_LOCK;

	/*
	 * First pass does the initial State-Changes.
	 */
	entryIndex = 0;
	current = *list;
	while (true)
	{
		/*
		 * Keep adjusting the list to maintain its integrity.
		 */
		if (current == NULL)
			break;

		action = PendingDelete_Action(current);

		if (Debug_persistent_print)
		{
			if (current->relationName == NULL)
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: Do 1st delete state-change action for list entry #%d: '%s' (persistent end transaction action '%s', transaction nest level %d, persistent TID %s, persistent serial number " INT64_FORMAT ")",
					 entryIndex,
					 PersistentFileSysObjName_TypeAndObjectName(&current->fsObjName),
					 PersistentEndXactFileSysAction_Name(action),
					 current->nestLevel,
					 ItemPointerToString(&current->persistentTid),
					 current->persistentSerialNum);
			else
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: Do 1st delete state-change action for list entry #%d: '%s', relation name '%s' (persistent end transaction action '%s', transaction nest level %d, persistent TID %s, persistent serial number " INT64_FORMAT ")",
					 entryIndex,
					 PersistentFileSysObjName_TypeAndObjectName(&current->fsObjName),
					 current->relationName,
					 PersistentEndXactFileSysAction_Name(action),
					 current->nestLevel,
					 ItemPointerToString(&current->persistentTid),
					 current->persistentSerialNum);
		}

		switch (action)
		{
		case PersistentEndXactFileSysAction_Create:
			if (forCommit)
			{
				PersistentFileSysObj_Created(
								&current->fsObjName,
								&current->persistentTid,
								current->persistentSerialNum,
								/* retryPossible */ false);
			}
			else
			{
				stateChangeResults[entryIndex] =
					PersistentFileSysObj_MarkAbortingCreate(
								&current->fsObjName,
								&current->persistentTid,
								current->persistentSerialNum,
								/* retryPossible */ false);
			}
			break;

		case PersistentEndXactFileSysAction_Drop:
			if (forCommit)
			{
				stateChangeResults[entryIndex] =
					PersistentFileSysObj_MarkDropPending(
								&current->fsObjName,
								&current->persistentTid,
								current->persistentSerialNum,
								/* retryPossible */ false);
			}
			break;

		case PersistentEndXactFileSysAction_AbortingCreateNeeded:
			/*
			 * Always whether transaction commits or aborts.
			 */
			stateChangeResults[entryIndex] =
				PersistentFileSysObj_MarkAbortingCreate(
							&current->fsObjName,
							&current->persistentTid,
							current->persistentSerialNum,
							/* retryPossible */ false);
			break;

		default:
			elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
				 action);
		}

		current = current->next;
		entryIndex++;

	}

	/*
	 * Make the above State-Changes permanent.
	 */
	PersistentFileSysObj_FlushXLog();

	/*
	 * Second pass does physical drops and final State-Changes.
	 */
	entryIndex = 0;
	while (true)
	{
		/*
		 * Keep adjusting the list to maintain its integrity.
		 */
		current = *list;
		if (current == NULL)
			break;

 		Assert(*listCount > 0);
		(*listCount)--;

		*list = current->next;

		action = PendingDelete_Action(current);

		dropPending = false;		// Assume.
		abortingCreate = false;		// Assume.

		switch (action)
		{
		case PersistentEndXactFileSysAction_Create:
			if (!forCommit)
			{
				abortingCreate = true;
			}
#ifdef FAULT_INJECTOR
				FaultInjector_InjectFaultIfSet(forCommit ?
											   TransactionCommitPass1FromCreatePendingToCreated :
											   TransactionAbortPass1FromCreatePendingToAbortingCreate,
											   DDLNotSpecified,
											   "",	// databaseName
											   ""); // tableName
#endif
			break;

		case PersistentEndXactFileSysAction_Drop:
			if (forCommit)
			{
				dropPending = true;

				SIMPLE_FAULT_INJECTOR(TransactionCommitPass1FromDropInMemoryToDropPending);
			}
			break;

		case PersistentEndXactFileSysAction_AbortingCreateNeeded:
			/*
			 * Always whether transaction commits or aborts.
			 */
			abortingCreate = true;

#ifdef FAULT_INJECTOR
				FaultInjector_InjectFaultIfSet(forCommit ?
											   TransactionCommitPass1FromAbortingCreateNeededToAbortingCreate:
											   TransactionAbortPass1FromAbortingCreateNeededToAbortingCreate,
											   DDLNotSpecified,
											   "",	// databaseName
											   ""); // tableName
#endif
			break;

		default:
			elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
				 action);
		}

		if (abortingCreate || dropPending)
		{
			if (stateChangeResults[entryIndex] == PersistentFileSysObjStateChangeResult_StateChangeOk)
			{
				PersistentFileSysObj_EndXactDrop(&current->fsObjName,
												 current->relStorageMgr,
												 current->relationName,
												 &current->persistentTid,
												 current->persistentSerialNum,
												 /* ignoreNonExistence */ abortingCreate);
			}
		}

#ifdef FAULT_INJECTOR
		if (abortingCreate && !forCommit)
		{
			FaultInjector_InjectFaultIfSet(TransactionAbortPass2FromCreatePendingToAbortingCreate,
										   DDLNotSpecified,
										   "",	// databaseName
										   ""); // tableName
		}

		if (dropPending && forCommit)
		{
			FaultInjector_InjectFaultIfSet(TransactionCommitPass2FromDropInMemoryToDropPending,
										   DDLNotSpecified,
										   "",	// databaseName
										   ""); // tableName
		}

		switch (action)
		{
			case PersistentEndXactFileSysAction_Create:
				if (!forCommit)
				{
					FaultInjector_InjectFaultIfSet(TransactionAbortPass2FromCreatePendingToAbortingCreate,
												   DDLNotSpecified,
												   "",	// databaseName
												   ""); // tableName
				}
				break;

			case PersistentEndXactFileSysAction_Drop:
				if (forCommit)
				{
					FaultInjector_InjectFaultIfSet(TransactionCommitPass2FromDropInMemoryToDropPending,
												   DDLNotSpecified,
												   "",	// databaseName
												   ""); // tableName
				}
				break;

			case PersistentEndXactFileSysAction_AbortingCreateNeeded:
				FaultInjector_InjectFaultIfSet(forCommit ?
											   TransactionCommitPass2FromAbortingCreateNeededToAbortingCreate :
											   TransactionAbortPass2FromAbortingCreateNeededToAbortingCreate,
											   DDLNotSpecified,
											   "",	// databaseName
											   ""); // tableName
				break;

			default:
				break;
		}
#endif

		/* must explicitly free the list entry */
		PendingDelete_Free(&current);

		entryIndex++;

	}
	Assert(*listCount == 0);
	Assert(*list == NULL);

	PersistentFileSysObj_FlushXLog();

	MIRRORED_UNLOCK;

	if (stateChangeResults != NULL)
		pfree(stateChangeResults);
}

/*
 * smgrGetPendingFileSysWork() -- Get a list of relations that have post-commit or post-abort
 * work.
 *
 * The return value is the number of relations scheduled for termination.
 * *ptr is set to point to a freshly-palloc'd array of RelFileNodes.
 * If there are no relations to be deleted, *ptr is set to NULL.
 *
 * If haveNonTemp isn't NULL, the bool it points to gets set to true if
 * there is any non-temp table pending to be deleted; false if not.
 *
 * Note that the list does not include anything scheduled for termination
 * by upper-level transactions.
 */
int
smgrGetPendingFileSysWork(EndXactRecKind endXactRecKind,
						  PersistentEndXactFileSysActionInfo **ptr)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	int			nrels;

	PersistentEndXactFileSysActionInfo *rptr;

	PendingDelete *pending;
	int			entryIndex;

	PersistentEndXactFileSysAction action;

	Assert(endXactRecKind == EndXactRecKind_Commit ||
		   endXactRecKind == EndXactRecKind_Abort ||
		   endXactRecKind == EndXactRecKind_Prepare);

	if (!pendingDeletesSorted)
	{
		/*
		 * Sort the list in relation, database directory, tablespace, etc order.
		 * And, collapse same transaction create-deletes.
		 */
		smgrSortDeletesList(
						&pendingDeletes,
						&pendingDeletesCount,
						nestLevel);

		pendingDeletesSorted = (nestLevel <= 1);
	}

	nrels = 0;

	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		action = PendingDelete_Action(pending);

		if (pending->nestLevel >= nestLevel &&
			EndXactRecKind_NeedsAction(endXactRecKind, action))
		{
			nrels++;
		}
	}
	if (nrels == 0)
	{
		*ptr = NULL;
		return 0;
	}

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Storage Manager: Get list entries (transaction kind '%s', current transaction nest level %d)",
			 EndXactRecKind_Name(endXactRecKind),
			 nestLevel);

	rptr = (PersistentEndXactFileSysActionInfo *)
		palloc(nrels * sizeof(PersistentEndXactFileSysActionInfo));
	*ptr = rptr;
	entryIndex = 0;
	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		bool		returned;

		action = PendingDelete_Action(pending);
		returned = false;

		if (pending->nestLevel >= nestLevel &&
			EndXactRecKind_NeedsAction(endXactRecKind, action))
		{
			rptr->action = action;
			rptr->fsObjName = pending->fsObjName;
			rptr->relStorageMgr = pending->relStorageMgr;
			rptr->persistentTid = pending->persistentTid;
			rptr->persistentSerialNum = pending->persistentSerialNum;

			rptr++;
			returned = true;
		}

		if (Debug_persistent_print)
		{
			if (pending->relationName == NULL)
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: Get list entry #%d: '%s' (transaction kind '%s', returned %s, transaction nest level %d, relation storage manager '%s', persistent TID %s, persistent serial number " INT64_FORMAT ")",
					 entryIndex,
					 PersistentFileSysObjName_TypeAndObjectName(&pending->fsObjName),
					 EndXactRecKind_Name(endXactRecKind),
					 (returned ? "true" : "false"),
					 pending->nestLevel,
					 PersistentFileSysRelStorageMgr_Name(pending->relStorageMgr),
					 ItemPointerToString(&pending->persistentTid),
					 pending->persistentSerialNum);
			else
				elog(Persistent_DebugPrintLevel(),
					 "Storage Manager: Get list entry #%d: '%s', relation name '%s' (transaction kind '%s', returned %s, transaction nest level %d, relation storage manager '%s', persistent TID %s, persistent serial number " INT64_FORMAT ")",
					 entryIndex,
					 PersistentFileSysObjName_TypeAndObjectName(&pending->fsObjName),
					 pending->relationName,
					 EndXactRecKind_Name(endXactRecKind),
					 (returned ? "true" : "false"),
					 pending->nestLevel,
					 PersistentFileSysRelStorageMgr_Name(pending->relStorageMgr),
					 ItemPointerToString(&pending->persistentTid),
					 pending->persistentSerialNum);
		}
		entryIndex++;
	}
	return nrels;
}


/*
 * smgrIsPendingFileSysWork() -- Returns true if there are relations that need post-commit or
 * post-abort work.
 *
 * Note that the list does not include anything scheduled for termination
 * by upper-level transactions.
 */
bool
smgrIsPendingFileSysWork(EndXactRecKind endXactRecKind)
{
	int			nestLevel = GetCurrentTransactionNestLevel();

	PendingDelete *pending;

	PersistentEndXactFileSysAction action;

	Assert(endXactRecKind == EndXactRecKind_Commit ||
		   endXactRecKind == EndXactRecKind_Abort ||
		   endXactRecKind == EndXactRecKind_Prepare);

	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		action = PendingDelete_Action(pending);

		if (pending->nestLevel >= nestLevel &&
			EndXactRecKind_NeedsAction(endXactRecKind, action))
		{
			return true;
		}
	}

	return false;
}

/*
 *	PostPrepare_smgr -- Clean up after a successful PREPARE
 *
 * What we have to do here is throw away the in-memory state about pending
 * relation deletes.  It's all been recorded in the 2PC state file and
 * it's no longer smgr's job to worry about it.
 */
void
PostPrepare_smgr(void)
{
	PendingDelete *pending;
	PendingDelete *next;

	for (pending = pendingDeletes; pending != NULL; pending = next)
	{
		next = pending->next;
		pendingDeletes = next;

		pendingDeletesCount--;

		/* must explicitly free the list entry */
		PendingDelete_Free(&pending);
	}

	Assert(pendingDeletesCount == 0);
	pendingDeletesSorted = false;
	pendingDeletesPerformed = true;

	/*
	 * Free the Append-Only mirror resync EOFs hash table.
	 */
	AppendOnlyMirrorResyncEofs_HashTableRemove("PostPrepare_smgr");

	// UNDONE: We are passing the responsibility on to PersistentFileSysObj_PreparedEndXactAction...
}

/*
 * AtSubCommit_smgr() --- Take care of subtransaction commit.
 *
 * Reassign all items in the pending-deletes list to the parent transaction.
 */
void
AtSubCommit_smgr(void)
{
	int			nestLevel = GetCurrentTransactionNestLevel();
	PendingDelete *pending;

	for (pending = pendingDeletes; pending != NULL; pending = pending->next)
	{
		if (pending->nestLevel >= nestLevel)
		{
			pending->nestLevel = nestLevel - 1;

			if (pending->fsObjName.type == PersistentFsObjType_RelationFile &&
				pending->relStorageMgr == PersistentFileSysRelStorageMgr_AppendOnly)
			{
				/*
				 * If we are promoting a DROP of an Append-Only table, be sure to remove any
				 * pending Append-Only mirror resync EOFs updates for the NEW TRANSACTION
				 * LEVEL, too.
				 */
				AppendOnlyMirrorResyncEofs_RemoveForDrop(
													PersistentFileSysObjName_GetRelFileNodePtr(&pending->fsObjName),
													PersistentFileSysObjName_GetSegmentFileNum(&pending->fsObjName),
													pending->nestLevel);
			}
		}
	}

	AtSubCommit_smgr_appendonly();	
}

/*
 * AtSubAbort_smgr() --- Take care of subtransaction abort.
 *
 * Delete created relations and forget about deleted relations.
 * We can execute these operations immediately because we know this
 * subtransaction will not commit.
 */
void
AtSubAbort_smgr(void)
{
	smgrSubTransAbort();
}

void
smgr_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	bool		mirrorDataLossOccurred = false;

	/* Backup blocks are not used in smgr records */
	Assert(!(record->xl_info & XLR_BKP_BLOCK_MASK));

	if (info == XLOG_SMGR_CREATE)
	{
/*
 * Disable replay of XLOG_SMGR_CREATE until persistent tables and MMXLOG
 * records are removed from Greenplum.
 */
#if 0
		MirrorDataLossTrackingState mirrorDataLossTrackingState;
		int64		mirrorDataLossTrackingSessionNum;

 		xl_smgr_create *xlrec = (xl_smgr_create *) XLogRecGetData(record);
		SMgrRelation reln;

		reln = smgropen(xlrec->rnode);

		mirrorDataLossTrackingState =
			FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
				&mirrorDataLossTrackingSessionNum);
		smgrmirroredcreate(reln,
						   /* relationName */ NULL, // Ok to be NULL -- we don't know the name here.
						   mirrorDataLossTrackingState,
						   mirrorDataLossTrackingSessionNum,
						   /* ignoreAlreadyExists */ true,
						   &mirrorDataLossOccurred);
#endif
		return;
	}
	else if (info == XLOG_SMGR_TRUNCATE)
	{
		MirrorDataLossTrackingState mirrorDataLossTrackingState;
		int64		mirrorDataLossTrackingSessionNum;
		xl_smgr_truncate *xlrec = (xl_smgr_truncate *) XLogRecGetData(record);
		SMgrRelation reln;

		reln = smgropen(xlrec->rnode);

		/*
		 * Forcibly create relation if it doesn't exist (which suggests that
		 * it was dropped somewhere later in the WAL sequence).  As in
		 * XLogReadBuffer, we prefer to recreate the rel and replay the log
		 * as best we can until the drop is seen.
		 */
		mirrorDataLossTrackingState =
			FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
				&mirrorDataLossTrackingSessionNum);
		smgrmirroredcreate(reln,
						   /* relationName */ NULL, // Ok to be NULL -- we don't know the name here.
						   mirrorDataLossTrackingState,
						   mirrorDataLossTrackingSessionNum,
						   /* ignoreAlreadyExists */ true,
						   &mirrorDataLossOccurred);

		/*
		 * Pass true for allowedNotFound below to mdtruncate to cope for the case
		 * when replay of truncate redo log happen multiple times it acts as NOP, 
		 * which makes redo truncate behavior is idempotent.
		 */
		smgrtruncate(reln, MAIN_FORKNUM, xlrec->blkno, false);

		/* Also tell xlogutils.c about it */
		XLogTruncateRelation(xlrec->rnode, MAIN_FORKNUM, xlrec->blkno);

		/* Truncate FSM too */
		if (smgrexists(reln, FSM_FORKNUM))
		{
			Relation	rel = CreateFakeRelcacheEntry(xlrec->rnode);

			FreeSpaceMapTruncateRel(rel, xlrec->blkno);
			FreeFakeRelcacheEntry(rel);
		}

	}
	else
		elog(PANIC, "smgr_redo: unknown op code %u", info);
}

bool
smgrgetpersistentinfo(XLogRecord *record,
					  RelFileNode	*relFileNode,
					  ItemPointer	persistentTid,
					  int64		*persistentSerialNum)
{
	uint8		info;

	Assert (record->xl_rmid == RM_SMGR_ID);

	info = record->xl_info & ~XLR_INFO_MASK;

	if (info == XLOG_SMGR_TRUNCATE)
	{
		xl_smgr_truncate *xlrec = (xl_smgr_truncate *) XLogRecGetData(record);

		*relFileNode = xlrec->rnode;
		*persistentTid = xlrec->persistentTid;
		*persistentSerialNum = xlrec->persistentSerialNum;
		return true;
	}

	return false;
}

void
smgr_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	uint8           info = record->xl_info & ~XLR_INFO_MASK;
	char            *rec = XLogRecGetData(record);

	if (info == XLOG_SMGR_CREATE)
	{
		xl_smgr_create *xlrec = (xl_smgr_create *) rec;
		char	   *path = relpath(xlrec->rnode, MAIN_FORKNUM);

		appendStringInfo(buf, "file create: %s", path);
		pfree(path);
	}
	else if (info == XLOG_SMGR_TRUNCATE)
	{
		xl_smgr_truncate *xlrec = (xl_smgr_truncate *) rec;
		char	   *path = relpath(xlrec->rnode, MAIN_FORKNUM);

		appendStringInfo(buf, "file truncate: %s to %u blocks", path,
						 xlrec->blkno);
		pfree(path);
	}
	else
		appendStringInfo(buf, "UNKNOWN");
}
