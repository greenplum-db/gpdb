/*-------------------------------------------------------------------------
 *
 * cdbpersistentrecovery.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpersistentrecovery.c
 *
 *-------------------------------------------------------------------------
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "postgres.h"

#include "access/clog.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/persistentfilesysobjname.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "catalog/catalog.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_class.h"
#include "catalog/pg_database.h"
#include "catalog/pg_namespace.h"
#include "catalog/gp_persistent.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbdirectopen.h"
#include "cdb/cdbdoublylinked.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentrelation.h"
#include "cdb/cdbpersistentrecovery.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/visibility_summary.h"

static bool
PersistentRecovery_RedoRelationExists(
	ItemPointer		persistentTid,
	
	int64 			persistentSerialNum,
	
	RelFileNode 	*relFileNode);	

typedef struct XactEntryData
{
	TransactionId	xid;

	XactInfoKind	infoKind;

	DoublyLinkedHead	fsObjEntryList;
} XactEntryData;
typedef XactEntryData *XactEntry;

/*
 * This hashTable is used during crash recovery. Keeps track of transactions
 * and its state plus filesystem-objects associated with those transactions
 * found during recovery. It's populated after scanning all the xlog
 * records in Pass 2 for Transaction ID Resource Manager.
 * Transaction can be found in Commit, Abort, Prepare, Abort Prepared, Commit
 * Prepared, Distributed Commit or Distributed Abort. Depending on same this
 * hashtable is populated to keep track of Committed
 * (infokind=XACT_INFOKIND_COMMIT), Aborted (infokind=XACT_INFOKIND_ABORT) and
 * Inflight (infokind=XACT_INFOKIND_NONE) transactions.
 * "fsObjHashTable" also one of the hash-tables created during recovery, keeping
 * track of persistent table changes keyed on its TID for file-system
 * object. That hash-table maintains end transaction work that’s remaining on
 * the file-system objects is used to populate and connect this xactHashTable
 * transactions to corresponding filesystem-objects touched by it.
 * With the help of same inflight transactions are scanned later to check if
 * they got PREPARED or not. This is necessary so that half-done data or “Create
 * Pending” objects would get linked to their transactions and would not get
 * aborted automatically by the recovery process. For e.g. there could be few
 * “Create Pending” objects for committed transaction in which case it needs to
 * be updated to “Created” state. On the other hand, Create Pending objects
 * associated with aborted transactions would get updated with “Aborting Create” state.
 */
static HTAB *xactHashTable = NULL;

static void
PersistentRecovery_XactHashTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(TransactionId);
	info.entrysize = sizeof(XactEntryData);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	xactHashTable = hash_create("XactEntry", 10, &info, hash_flags);
}

static XactEntry PersistentRecovery_FindOrCreateXactEntry(
	TransactionId	xid,

	bool			*found)
{
	XactEntry xactEntry;

	if (xactHashTable == NULL)
		PersistentRecovery_XactHashTableInit();

	xactEntry = 
			(XactEntry) 
					hash_search(xactHashTable,
								(void *) &xid,
								HASH_ENTER,
								found);

	if (!*found)
	{
		DoublyLinkedHead_Init(&xactEntry->fsObjEntryList);
	}

	return xactEntry;
}

#if suppress
static XactEntry PersistentRecovery_FindXactEntry(
	TransactionId	xid)
{
	XactEntry xactEntry;
	bool found;

	Assert (xactHashTable != NULL);

	xactEntry = 
			(XactEntry) 
					hash_search(xactHashTable,
								(void *) &xid,
								HASH_FIND,
								&found);
	if (!found)
		return NULL;

	return xactEntry;
}
#endif

typedef struct FsObjEntryKey
{
	ItemPointerData				persistentTid;
} FsObjEntryKey;

typedef struct FsObjEntryData
{
	FsObjEntryKey				key;

	PersistentFileSysObjName	fsObjName;

	PersistentFileSysRelStorageMgr relStorageMgr;

	int64						persistentSerialNum;
	
	PersistentFileSysState		state;

	MirroredObjectExistenceState mirrorExistenceState;

	TransactionId				xid;

	bool						updateNeeded;
	bool foundInTable;

	PersistentFileSysObjStateChangeResult stateChangeResult;
	
	DoubleLinks					xactLinks;

} FsObjEntryData;
typedef FsObjEntryData *FsObjEntry;

static char *
FsObjEntryToBuffer(char *buffer, int maxBufferLen, FsObjEntry fsObjEntry)
{
	int snprintfResult;

	if (fsObjEntry->state != PersistentFileSysState_Free)	
		snprintfResult = 
			snprintf(
				buffer,
				maxBufferLen,
				"%s: PS:'%s' MES:'%s' Xact:%u RelSMgr:'%s' PSN:" INT64_FORMAT " TID:%s",
				PersistentFileSysObjName_TypeAndObjectName(&fsObjEntry->fsObjName),
				PersistentFileSysObjState_Name(fsObjEntry->state),
				MirroredObjectExistenceState_Name(fsObjEntry->mirrorExistenceState),
				fsObjEntry->xid,
				PersistentFileSysRelStorageMgr_Name(fsObjEntry->relStorageMgr),
				fsObjEntry->persistentSerialNum,
				ItemPointerToString(&fsObjEntry->key.persistentTid));
	else
		snprintfResult = 
			snprintf(
				buffer,
				maxBufferLen,
			    "Free entry as PSN:" INT64_FORMAT ", TID:%s",
				fsObjEntry->persistentSerialNum,
				ItemPointerToString(&fsObjEntry->key.persistentTid));

	Assert(snprintfResult >= 0);
	Assert(snprintfResult < maxBufferLen);

	return buffer;
}

#define MAX_FS_OBJ_ENTRY_BUFFER 400
static char fsObjEntryBuffer[MAX_FS_OBJ_ENTRY_BUFFER];
static char fsObjEntryBuffer2[MAX_FS_OBJ_ENTRY_BUFFER];

static char *
FsObjEntryToString(FsObjEntry fsObjEntry)
{
	return FsObjEntryToBuffer(fsObjEntryBuffer, MAX_FS_OBJ_ENTRY_BUFFER, fsObjEntry);
}

static char *
FsObjEntryToString2(FsObjEntry fsObjEntry)
{
	return FsObjEntryToBuffer(fsObjEntryBuffer2, MAX_FS_OBJ_ENTRY_BUFFER, fsObjEntry);
}

static HTAB **fsObjHashTable = NULL;

static void
PersistentRecovery_FsObjHashTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	PersistentFsObjType fsObjType;

	char name[20];

	fsObjHashTable = (HTAB**)palloc(CountPersistentFsObjType * sizeof(HTAB*));

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(FsObjEntryKey);
	info.entrysize = sizeof(FsObjEntryData);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	for (fsObjType = PersistentFsObjType_First;
	     fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
 	{
 		sprintf(name, "FsObjEntry_%d", fsObjType);
		fsObjHashTable[fsObjType] = hash_create(name, 10, &info, hash_flags);
 	}

}

static FsObjEntry PersistentRecovery_FindOrCreateFsObjEntry(
	PersistentFsObjType fsObjType,
	
	ItemPointer			persistentTid,

	bool				*found)
{
	FsObjEntry fsObjEntry;

	FsObjEntryKey	key;

	if (fsObjHashTable == NULL)
		PersistentRecovery_FsObjHashTableInit();

	MemSet(&key, 0, sizeof(FsObjEntryKey));
	key.persistentTid = *persistentTid;

	fsObjEntry = 
				(FsObjEntry) 
						hash_search(fsObjHashTable[fsObjType],
									(void *) &key,
									HASH_ENTER,
									found);

	Assert(ItemPointerCompare(&fsObjEntry->key.persistentTid, persistentTid) == 0);

	if (!*found)
	{
		DoubleLinks_Init(&fsObjEntry->xactLinks);
		fsObjEntry->persistentSerialNum = 0;
		MemSet(&fsObjEntry->fsObjName, 0, sizeof(PersistentFileSysObjName));
		fsObjEntry->relStorageMgr = PersistentFileSysRelStorageMgr_None;
		fsObjEntry->xid = InvalidTransactionId;
		fsObjEntry->state = -1;
		fsObjEntry->mirrorExistenceState = -1;
		fsObjEntry->updateNeeded = false;
		fsObjEntry->foundInTable = false;
		fsObjEntry->stateChangeResult = PersistentFileSysObjStateChangeResult_StateChangeOk;
	}

	return fsObjEntry;
}

static FsObjEntry PersistentRecovery_FindFsObjEntry(
	PersistentFsObjType fsObjType,
	
	ItemPointer			persistentTid)
{
	FsObjEntry fsObjEntry;

	FsObjEntryKey	key;

	bool found;

	if (fsObjHashTable == NULL)
		return NULL;

	MemSet(&key, 0, sizeof(FsObjEntryKey));
	key.persistentTid = *persistentTid;

	fsObjEntry = 
				(FsObjEntry) 
						hash_search(fsObjHashTable[fsObjType],
									(void *) &key,
									HASH_FIND,
									&found);
	if (!found)
		return NULL;

	return fsObjEntry;
}

static void PersistentRecovery_AddEndXactFsObj(
	TransactionId 						xid,

	XactInfoKind						infoKind,

	PersistentEndXactFileSysActionInfo	*fileSysActionInfo,

	PersistentFileSysState				state)
{
	FsObjEntry 	fsObjEntry;
	FsObjEntryData prevFsObjEntry;

	bool found;

	/*
	 * Create Persistent Change entry keyed on TID.
	 */
	fsObjEntry = 
		PersistentRecovery_FindOrCreateFsObjEntry(
									fileSysActionInfo->fsObjName.type, 
									&fileSysActionInfo->persistentTid, 
									&found);
	if (found)
	{
		memcpy(&prevFsObjEntry, fsObjEntry, sizeof(FsObjEntryData));
	}

	fsObjEntry->fsObjName = fileSysActionInfo->fsObjName;

	if (fileSysActionInfo->fsObjName.type == PersistentFsObjType_RelationFile)
	{
		if (!PersistentFileSysRelStorageMgr_IsValid(fileSysActionInfo->relStorageMgr))
			elog(ERROR, "Relation storage manager for persistent '%s' for Crash Recovery is invalid (%d)",
				 PersistentFileSysObjName_TypeAndObjectName(&fileSysActionInfo->fsObjName),
				 fileSysActionInfo->relStorageMgr);

		fsObjEntry->relStorageMgr = fileSysActionInfo->relStorageMgr;
	}
	else
	{
		// Not a 'Relation File'.
		fsObjEntry->relStorageMgr = PersistentFileSysRelStorageMgr_None;
	}
		
	fsObjEntry->persistentSerialNum = fileSysActionInfo->persistentSerialNum;
	fsObjEntry->xid = xid;
	fsObjEntry->state = state;

	if (found)
	{
		if (! Disable_persistent_recovery_logging)
			elog(LOG, 
			     "%s Record (transaction %u): Overwritting %s with %s",
				 XactInfoKind_Name(infoKind),
				 xid,
				 FsObjEntryToString(&prevFsObjEntry),
				 FsObjEntryToString2(fsObjEntry));
	}
	else
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
			     "%s Record (transaction %u): Adding %s",
				 XactInfoKind_Name(infoKind),
				 xid,
				 FsObjEntryToString(fsObjEntry));
	}
}

typedef struct AoMirrorResyncEofsEntryKey
{
	ItemPointerData				persistentTid;
} AoMirrorResyncEofsEntryKey;

typedef struct AoMirrorResyncEofsEntryData
{
	AoMirrorResyncEofsEntryKey	key;

	RelFileNode					relFileNode;

	int32						segmentFileNum;

	int64						persistentSerialNum;

	TransactionId				xid;

	int64						mirrorLossEof;

	int64						mirrorNewEof;

} AoMirrorResyncEofsEntryData;
typedef AoMirrorResyncEofsEntryData *AoMirrorResyncEofsEntry;

static char *
AoMirrorResyncEofsEntryToBuffer(char *buffer, int maxBufferLen, AoMirrorResyncEofsEntry eofsEntry)
{
	int snprintfResult;

	snprintfResult =
		snprintf(
			buffer,
			maxBufferLen,
		    "AO rel %u/%u/%u segfile#:%d Xact:%u MlossEOF:" INT64_FORMAT " MnewEOF:" INT64_FORMAT " PSN:" INT64_FORMAT " TID:'%s' eofsEntAddr:%p",
			eofsEntry->relFileNode.spcNode,
			eofsEntry->relFileNode.dbNode,
			eofsEntry->relFileNode.relNode,
			eofsEntry->segmentFileNum,
			eofsEntry->xid,
			eofsEntry->mirrorLossEof,
			eofsEntry->mirrorNewEof,
			eofsEntry->persistentSerialNum,
			ItemPointerToString(&eofsEntry->key.persistentTid),
			eofsEntry);

	Assert(snprintfResult >= 0);
	Assert(snprintfResult < maxBufferLen);

	return buffer;
}

#define MAX_AO_MIRROR_RESYNC_EOFS_ENTRY_BUFFER 300
static char aoMirrorResyncEofsEntryBuffer[MAX_AO_MIRROR_RESYNC_EOFS_ENTRY_BUFFER];
static char aoMirrorResyncEofsEntryBuffer2[MAX_AO_MIRROR_RESYNC_EOFS_ENTRY_BUFFER];

static char *
AoMirrorResyncEofsEntryToString(AoMirrorResyncEofsEntry eofsEntry)
{
	return AoMirrorResyncEofsEntryToBuffer(aoMirrorResyncEofsEntryBuffer, MAX_AO_MIRROR_RESYNC_EOFS_ENTRY_BUFFER, eofsEntry);
}

static char *
AoMirrorResyncEofsEntryToString2(AoMirrorResyncEofsEntry eofsEntry)
{
	return AoMirrorResyncEofsEntryToBuffer(aoMirrorResyncEofsEntryBuffer2, MAX_AO_MIRROR_RESYNC_EOFS_ENTRY_BUFFER, eofsEntry);
}

static HTAB *recoveryAppendOnlyMirrorResyncEofsTable = NULL;

static void
PersistentRecovery_RecoveryAppendOnlyMirrorResyncEofsTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(AoMirrorResyncEofsEntryKey);
	info.entrysize = sizeof(AoMirrorResyncEofsEntryData);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	recoveryAppendOnlyMirrorResyncEofsTable = hash_create("Recovery AO EOFs", 10, &info, hash_flags);
}

static void PersistentRecovery_AddAppendOnlyMirrorResyncEofs(
	TransactionId 								xid,
	PersistentEndXactAppendOnlyMirrorResyncEofs	*eofs)
{
	AoMirrorResyncEofsEntryKey	key;

	AoMirrorResyncEofsEntry eofsEntry;

	bool found;

	AoMirrorResyncEofsEntryData prevEofsEntry;

	if (recoveryAppendOnlyMirrorResyncEofsTable == NULL)
		PersistentRecovery_RecoveryAppendOnlyMirrorResyncEofsTableInit();

	MemSet(&key, 0, sizeof(FsObjEntryKey));
	key.persistentTid = eofs->persistentTid;

	eofsEntry = 
		(AoMirrorResyncEofsEntry) 
						hash_search(recoveryAppendOnlyMirrorResyncEofsTable,
									(void *) &key,
									HASH_ENTER,
									&found);
	if (found)
	{
		memcpy(&prevEofsEntry, eofsEntry, sizeof(AoMirrorResyncEofsEntryData));
	}

	eofsEntry->relFileNode = eofs->relFileNode;
	eofsEntry->segmentFileNum = eofs->segmentFileNum;
	eofsEntry->persistentSerialNum = eofs->persistentSerialNum;
	eofsEntry->xid = xid;
	eofsEntry->mirrorLossEof = eofs->mirrorLossEof;
	eofsEntry->mirrorNewEof = eofs->mirrorNewEof;

	if (found)
	{
		if (! Disable_persistent_recovery_logging)
			elog(LOG, 
			     "Commit REDO: Overwritting %s with %s",
				 AoMirrorResyncEofsEntryToString(&prevEofsEntry),
				 AoMirrorResyncEofsEntryToString2(eofsEntry));
	}
	else
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
			     "Commit REDO: Adding %s",
				 AoMirrorResyncEofsEntryToString(eofsEntry));
	}
}

bool
PersistentRecovery_ShouldHandlePass1XLogRec(
	XLogRecPtr		*beginLoc,

	XLogRecPtr 		*lsn,
	
	XLogRecord 		*record)
{
	int					relationChangeInfoArrayCount;
	int					arrlen = ChangeTracking_GetInfoArrayDesiredMaxLength(record->xl_rmid, 
																			 record->xl_info);
	RelationChangeInfo 	relationChangeInfoArray[arrlen];
	
	if (Debug_persistent_recovery_print)
		ChangeTracking_PrintRelationChangeInfo(
											record->xl_rmid,
											record->xl_info,
											(void*)XLogRecGetData(record), 
											lsn,
											/* weAreGeneratingXLogNow */ false,
											/* printSkipIssuesOnly */ false);

	/*
	 * Gather vital peristence information from the XLOG record about relations ONLY.
	 */
	ChangeTracking_GetRelationChangeInfoFromXlog(
									  record->xl_rmid,
									  record->xl_info,
									  (void*)XLogRecGetData(record), 
									  relationChangeInfoArray,
									  &relationChangeInfoArrayCount,
									  arrlen);
	if (relationChangeInfoArrayCount == 0)
	{
		/*
		 * A non-data change XLOG record.
		 */
		return false;
	}
	else
	{
		RelFileNode *xlogRelFileNode;

		xlogRelFileNode = &relationChangeInfoArray[0].relFileNode;
		
		if(RelFileNode_IsEmpty(xlogRelFileNode))
			elog(ERROR, "Invalid RelFileNode (0,0,0)");
		
		if (GpPersistent_IsPersistentRelation(xlogRelFileNode->relNode))
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "PersistentRecovery_ShouldHandlePass1XLogRec: persistent meta-data (returning true) %u/%u/%u",
					 xlogRelFileNode->spcNode,
					 xlogRelFileNode->dbNode,
					 xlogRelFileNode->relNode);
			return true;
		}
		else
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "PersistentRecovery_ShouldHandlePass1XLogRec: regular table (returning false) %u/%u/%u",
					 xlogRelFileNode->spcNode,
					 xlogRelFileNode->dbNode,
					 xlogRelFileNode->relNode);
			return false;
		}
	}

	return false;	// Not reached.
}

void
PersistentRecovery_HandlePass2XLogRec(
	XLogRecPtr		*beginLoc,

	XLogRecPtr 		*lsn,
	
	XLogRecord 		*record)
{
	RmgrId rmid = record->xl_rmid;

	if (rmid == RM_HEAP_ID)
	{
		RelFileNode relFileNode;
		
		if (!heap_getrelfilenode(record, &relFileNode))
			elog(ERROR, "No relfilenode");

		if (relFileNode.spcNode != GLOBALTABLESPACE_OID)
			return;

		if (relFileNode.relNode != GpPersistentRelationNodeRelationId &&
			relFileNode.relNode != GpPersistentDatabaseNodeRelationId &&
			relFileNode.relNode != GpPersistentTablespaceNodeRelationId &&
			relFileNode.relNode != GpPersistentFilespaceNodeRelationId)
			return;
	}
	else if (rmid == RM_XACT_ID)
	{
		TransactionId xid;

		XactEntry xactEntry;
		bool found;
		
		PersistentEndXactRecObjects persistentObjects;
		TransactionId *subXids;
		int subXidCount;
		XactInfoKind infoKind;
		int i;
		PersistentFileSysState newState;

		/*
		 * The XLOG record is for transaction module...
		 */
		if (!xact_redo_get_info(
							record,
							&infoKind,
							&xid,
							&persistentObjects,
							&subXids,
							&subXidCount))
			return;

		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "%s Record (transaction %u)",
				 XactInfoKind_Name(infoKind),
				 xid);
		
		/*
		 * We keep track of committed, aborted, and prepared transactions so 'Create Pending'
		 * objects can be attached to these transaction entries and not get automatically
		 * aborted by Crash Recovery.
		 */
		xactEntry = 
			PersistentRecovery_FindOrCreateXactEntry(xid, &found);
		xactEntry->infoKind = infoKind;
		
		if (infoKind == XACT_INFOKIND_COMMIT)
		{
			for (i = 0; i < persistentObjects.typed.fileSysActionInfosCount; i++)
			{
				PersistentEndXactFileSysActionInfo	*fileSysActionInfo =
							&persistentObjects.typed.fileSysActionInfos[i];

				PersistentEndXactFileSysAction action;
				
				action = fileSysActionInfo->action;

				switch (action)
				{
					case PersistentEndXactFileSysAction_Create:
						newState = PersistentFileSysState_Created;
						break;
					case PersistentEndXactFileSysAction_Drop:
						newState = PersistentFileSysState_DropPending;
						break;
					case PersistentEndXactFileSysAction_AbortingCreateNeeded:
						newState = PersistentFileSysState_AbortingCreate;
						break;
					default:
						elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
							 action);
						newState = PersistentFileSysState_Free;		// Not reached.
						break;
				}
			
				PersistentRecovery_AddEndXactFsObj(
												xid,
												XACT_INFOKIND_COMMIT,
												fileSysActionInfo,
												newState);
			}

			for (i = 0; i < persistentObjects.typed.appendOnlyMirrorResyncEofsCount; i++)
			{
				PersistentEndXactAppendOnlyMirrorResyncEofs *eofs = 
							&persistentObjects.typed.appendOnlyMirrorResyncEofs[i];

				PersistentRecovery_AddAppendOnlyMirrorResyncEofs(
																xid,
																eofs);
			}
		}
		else if (infoKind == XACT_INFOKIND_ABORT)
		{
			for (i = 0; i < persistentObjects.typed.fileSysActionInfosCount; i++)
			{
				PersistentEndXactFileSysActionInfo	*fileSysActionInfo =
							&persistentObjects.typed.fileSysActionInfos[i];

				PersistentEndXactFileSysAction action;

				action = fileSysActionInfo->action;

				switch (action)
				{
					case PersistentEndXactFileSysAction_Create:
						newState = PersistentFileSysState_AbortingCreate;
						break;
					case PersistentEndXactFileSysAction_Drop:
						continue;
					case PersistentEndXactFileSysAction_AbortingCreateNeeded:
						newState = PersistentFileSysState_AbortingCreate;
						break;
					default:
						elog(ERROR, "Unexpected persistent end transaction file-system action: %d",
							 action);
						newState = PersistentFileSysState_Free;		// Not reached.
				}

				PersistentRecovery_AddEndXactFsObj(
												xid,
												XACT_INFOKIND_ABORT,
												fileSysActionInfo,
												newState);
			}
		}
		else
		{
			Assert(infoKind == XACT_INFOKIND_PREPARE);

			/*
			 * Since we don't know if this transaction will commit or abort, we
			 * only keep the transaction entry.
			 */
		}
	}

}

bool
PersistentRecovery_ShouldHandlePass3XLogRec(
	XLogRecPtr		*beginLoc,
	XLogRecPtr 		*lsn,
	XLogRecord 		*record)
{
	int					relationChangeInfoArrayCount;
	int					arrlen = ChangeTracking_GetInfoArrayDesiredMaxLength(record->xl_rmid,
																			 record->xl_info);
	RelationChangeInfo 	relationChangeInfoArray[arrlen];
	
	RelFileNode xlogRelFileNode;
	ItemPointerData xlogPersistentTid;
	int64 xlogPersistentSerialNum;

	bool exists;

	if (Debug_persistent_recovery_print)
		ChangeTracking_PrintRelationChangeInfo(
											record->xl_rmid,
											record->xl_info,
											(void*)XLogRecGetData(record), 
											lsn,
											/* weAreGeneratingXLogNow */ false,
											/* printSkipIssuesOnly */ false);

	/*
	 * Gather vital peristence information from the XLOG record about relations ONLY.
	 */
	ChangeTracking_GetRelationChangeInfoFromXlog(
									  record->xl_rmid,
									  record->xl_info,
									  (void*)XLogRecGetData(record), 
									  relationChangeInfoArray,
									  &relationChangeInfoArrayCount,
									  arrlen);

	if (relationChangeInfoArrayCount == 0)
	{
		/*
		 * Special case truncate because it is not considered a FileRep "page change"....
		 */
		if (record->xl_rmid == RM_SMGR_ID)
		{
			if (!smgrgetpersistentinfo(
									record,
									&xlogRelFileNode,
									&xlogPersistentTid,
									&xlogPersistentSerialNum))
			{
				if (Debug_persistent_recovery_print)
					elog(PersistentRecovery_DebugPrintLevel(), 
						 "PersistentRecovery_ShouldHandlePass3XLogRec: truncate %u/%u/%u, serial number " INT64_FORMAT ", TID %s",
						  xlogRelFileNode.spcNode,
						  xlogRelFileNode.dbNode,
						  xlogRelFileNode.relNode,
						  xlogPersistentSerialNum,
						  ItemPointerToString(&xlogPersistentTid));

				return true;
			}

			// Otherwise, fall through with persistent information.
		}
		else
		{
			/*
			 * A non-data change XLOG record.
			 */
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "PersistentRecovery_ShouldHandlePass3XLogRec: non-data change XLOG record");

			return true;
		}
	}
	else
	{

		xlogRelFileNode = relationChangeInfoArray[0].relFileNode;
		xlogPersistentTid = relationChangeInfoArray[0].persistentTid;
		xlogPersistentSerialNum = relationChangeInfoArray[0].persistentSerialNum;
	}
	
	if (GpPersistent_IsPersistentRelation(xlogRelFileNode.relNode))
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_ShouldHandlePass3XLogRec: skip persistent meta-data %u/%u/%u",
				 xlogRelFileNode.spcNode,
				 xlogRelFileNode.dbNode,
				 xlogRelFileNode.relNode);
		return false;
	}

	if (GpPersistent_SkipXLogInfo(xlogRelFileNode.relNode))
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_ShouldHandlePass3XLogRec: other special relation %u/%u/%u",
				 xlogRelFileNode.spcNode,
				 xlogRelFileNode.dbNode,
				 xlogRelFileNode.relNode);
		return true;
	}

	if (PersistentStore_IsZeroTid(&xlogPersistentTid))
	{
		int level = ERROR;
		if (gp_persistent_statechange_suppress_error)
		{
			level = WARNING;
		}
		elog(level, "xlog record with zero persistenTID");
	}

	/*
	 * Further qualify using the RelFileNode.
	 */
	exists = PersistentRecovery_RedoRelationExists(
												&xlogPersistentTid,
												xlogPersistentSerialNum,
												&xlogRelFileNode);

	if (exists)
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_ShouldHandlePass3XLogRec: match %u/%u/%u, serial number " INT64_FORMAT ", TID %s",
				  xlogRelFileNode.spcNode,
				  xlogRelFileNode.dbNode,
				  xlogRelFileNode.relNode,
				  xlogPersistentSerialNum,
				  ItemPointerToString(&xlogPersistentTid));
	}
	else
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_ShouldHandlePass3XLogRec: relation has been dropped or the create aborted %u/%u/%u, serial number " INT64_FORMAT ", TID %s",
				  xlogRelFileNode.spcNode,
				  xlogRelFileNode.dbNode,
				  xlogRelFileNode.relNode,
				  xlogPersistentSerialNum,
				  ItemPointerToString(&xlogPersistentTid));
	}

	return exists;
}

static void
PersistentRecovery_PrintXactAndFsObjs(void)
{
	HASH_SEQ_STATUS iterateStatus;
	XactEntry xactEntry;
	FsObjEntry fsObjEntry;

	elog(LOG, "Entering PersistentRecovery_PrintXactAndFsObjs");
	if (xactHashTable == NULL)
	{
		elog(LOG, 
			 "Entering PersistentRecovery_PrintXactAndFsObjs -- no entries");
		return;
	}
	
	hash_seq_init(&iterateStatus, xactHashTable);
	while (true)
	{
		xactEntry = (XactEntry) hash_seq_search(&iterateStatus);
		if (xactEntry == NULL)
			break;

		elog(LOG, "Transaction: %u (%s)",
			 xactEntry->xid,
			 XactInfoKind_Name(xactEntry->infoKind));

		fsObjEntry = 
			(FsObjEntry) DoublyLinkedHead_First(
									offsetof(FsObjEntryData, xactLinks),
									&xactEntry->fsObjEntryList);
		while (fsObjEntry != NULL)
		{
			elog(LOG, "FsObj: %s", FsObjEntryToString(fsObjEntry));

			fsObjEntry = 
				(FsObjEntry) DoublyLinkedHead_Next(
										offsetof(FsObjEntryData, xactLinks),
										&xactEntry->fsObjEntryList,
										fsObjEntry);
		}
	}
	elog(LOG, "Exiting PersistentRecovery_PrintXactAndFsObjs");
}

static void
PersistentRecovery_PrintAoMirrorResyncEofs(void)
{
	HASH_SEQ_STATUS iterateStatus;
	AoMirrorResyncEofsEntry eofsEntry;

	elog(LOG, "Entering PersistentRecovery_PrintAoMirrorResyncEofs");
	if (recoveryAppendOnlyMirrorResyncEofsTable == NULL)
	{
		elog(LOG, 
			 "Entering PersistentRecovery_PrintAoMirrorResyncEofs -- no entries");
		return;
	}
	
	hash_seq_init(&iterateStatus, recoveryAppendOnlyMirrorResyncEofsTable);
	while (true)
	{
		eofsEntry = (AoMirrorResyncEofsEntry) hash_seq_search(&iterateStatus);
		if (eofsEntry == NULL)
			break;

		elog(LOG, "%s", AoMirrorResyncEofsEntryToString(eofsEntry));
	}
	elog(LOG, "Exiting PersistentRecovery_PrintAoMirrorResyncEofs");
}

// -----------------------------------------------------------------------------


static void
PersistentRecovery_AddScanEntry(
	PersistentFsObjType 	fsObjType,

	ItemPointer				persistentTid,

	PersistentFileSysObjName	*fsObjName,

	PersistentFileSysRelStorageMgr relStorageMgr,

	PersistentFileSysState	state,
	
	MirroredObjectExistenceState mirrorExistenceState,

	TransactionId			parentXid,
	
	int64					persistentSerialNum)
{
	FsObjEntry fsObjEntry;
	bool found;

	/*
	 * Create Persistent Change entry.
	 */
	fsObjEntry = 
		PersistentRecovery_FindOrCreateFsObjEntry(
										fsObjType,
										persistentTid,
										&found);
	fsObjEntry->foundInTable = true;

	if (!found)
	{
		fsObjEntry->fsObjName = *fsObjName;
		fsObjEntry->relStorageMgr = relStorageMgr;
		fsObjEntry->persistentSerialNum = persistentSerialNum;
		fsObjEntry->xid = parentXid;
		fsObjEntry->state = state;
		fsObjEntry->mirrorExistenceState = mirrorExistenceState;
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
			     "Scan REDO: Add %s", FsObjEntryToString(fsObjEntry));
		return;
	}

	if (fsObjEntry->persistentSerialNum == persistentSerialNum)
	{
		if (fsObjEntry->state < state)
		{
			if (! Disable_persistent_recovery_logging)
				elog(LOG, 
				     "Scan REDO: Newer %s than %s state, transaction %u",
					 FsObjEntryToString(fsObjEntry),
					 PersistentFileSysObjState_Name(state),
					 parentXid);

			fsObjEntry->state = state;
			fsObjEntry->mirrorExistenceState = mirrorExistenceState;
			fsObjEntry->xid = parentXid;
		}
		else if (fsObjEntry->state > state)
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
				     "Scan REDO: Update %s to state '%s', transaction %u, serial number " INT64_FORMAT,
					 FsObjEntryToString(fsObjEntry),
					 PersistentFileSysObjState_Name(state),
					 parentXid,
					 persistentSerialNum);

			fsObjEntry->updateNeeded = true;
		}
		else
		{
				/*
				* During crash recovery when we drop objects, we should skip the objects whose
				* mirror existence state is MirrorDropRemains. It will be dropped during resync
				*/
				fsObjEntry->mirrorExistenceState = mirrorExistenceState;
				if (! Disable_persistent_recovery_logging)
					elog(LOG,
						"Scan REDO: PT State is same. Updated the mirror existence state for %s",
						FsObjEntryToString(fsObjEntry));
		}
	}
	else if (fsObjEntry->persistentSerialNum < persistentSerialNum)
	{
		if (! Disable_persistent_recovery_logging)
			elog(LOG, 
			     "Scan REDO: Overwriting obsolete %s with '%s', relation storage manager '%s', state '%s', transaction %u, serial number " INT64_FORMAT,
				 FsObjEntryToString(fsObjEntry),
				 PersistentFileSysObjName_TypeAndObjectName(fsObjName),
				 PersistentFileSysRelStorageMgr_Name(relStorageMgr),
				 PersistentFileSysObjState_Name(state),
				 parentXid,
				 persistentSerialNum);

		fsObjEntry->fsObjName = *fsObjName;
		fsObjEntry->relStorageMgr = relStorageMgr;
		fsObjEntry->state = state;
		fsObjEntry->mirrorExistenceState = mirrorExistenceState;
		fsObjEntry->xid = parentXid;
		fsObjEntry->persistentSerialNum = persistentSerialNum;
	}
	else
	{
		if (! Disable_persistent_recovery_logging)
			elog(LOG, 
			     "Scan REDO: Wrong state %s, persistentSerialNum " INT64_FORMAT, 
				 FsObjEntryToString(fsObjEntry),
				 persistentSerialNum);
	}
}

static int	persistentRecoveryCount = 0;

static void PersistentRecovery_ScanTuple(
	PersistentFsObjType 	fsObjType,
	ItemPointer 			persistentTid,
	int64					persistentSerialNum,
	Datum					*values)
{
	PersistentFileSysObjName	fsObjName;
	
	PersistentFileSysState	state;

	MirroredObjectExistenceState		mirrorExistenceState;

	PersistentFileSysRelStorageMgr relStorageMgr;

	TransactionId			parentXid;
	int64					serialNum;

	GpPersistent_GetCommonValues(
							fsObjType,
							values,
							&fsObjName,
							&state,
							&mirrorExistenceState,
							&parentXid,
							&serialNum);
	Assert(serialNum == persistentSerialNum);

	if (state != PersistentFileSysState_Free &&
		fsObjType == PersistentFsObjType_RelationFile)
	{
		relStorageMgr = 
			(PersistentFileSysRelStorageMgr)
							DatumGetInt16(
									values[Anum_gp_persistent_relation_node_relation_storage_manager - 1]);
		
		if (!PersistentFileSysRelStorageMgr_IsValid(relStorageMgr))
			elog(ERROR, "Relation storage manager for persistent '%s' for Crash Recovery is invalid (%d)",
				 PersistentFileSysObjName_TypeAndObjectName(&fsObjName),
				 relStorageMgr);
	}
	else
	{
		// 'Free' entry or not a 'Relation File'.
		relStorageMgr = PersistentFileSysRelStorageMgr_None;
	}
	
	PersistentRecovery_AddScanEntry(
							fsObjType,
							persistentTid,
							&fsObjName,
							relStorageMgr,
							state,
							mirrorExistenceState,
							parentXid,
							persistentSerialNum);

	persistentRecoveryCount++;
}

static void
PersistentRecovery_ScanType(
	PersistentFsObjType fsObjType)
{
	PersistentFileSysObjData *fileSysObjData;
	PersistentFileSysObjSharedData	*fileSysObjSharedData;

	PersistentStoreScan storeScan;

	Datum *values;
	 
	ItemPointerData			 persistentTid;
	int64					 persistentSerialNum;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_ScanType %s",
		     PersistentFileSysObjName_TypeName(fsObjType));

	persistentRecoveryCount = 0;

	PersistentFileSysObj_GetDataPtrs(
								fsObjType,
								&fileSysObjData,
								&fileSysObjSharedData);
		 
	values = (Datum*)palloc(fileSysObjData->storeData.numAttributes * sizeof(Datum));
		 
	PersistentStore_BeginScan(
						&fileSysObjData->storeData,
						&fileSysObjSharedData->storeSharedData,
						&storeScan);

	while (PersistentStore_GetNext(
							&storeScan,
							values,
							&persistentTid,
							&persistentSerialNum))
	{
		PersistentRecovery_ScanTuple(
								fsObjType,
								&persistentTid,
								persistentSerialNum,
								values);
	}

	pfree(values);
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_ScanType %s, count %d",
		     PersistentFileSysObjName_TypeName(fsObjType),
		     persistentRecoveryCount);
}

void
PersistentRecovery_Scan(void)
{
	PersistentFsObjType fsObjType;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_Scan");

	for (fsObjType = PersistentFsObjType_First; 
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	 	PersistentRecovery_ScanType(fsObjType);

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_Scan");
}

static void
PersistentRecovery_AttachFsObjTypeToXact(
	PersistentFsObjType fsObjType)
{
	HASH_SEQ_STATUS iterateStatus;
	FsObjEntry fsObjEntry;
	XactEntry xactEntry;
	bool found;

	Assert (fsObjHashTable != NULL);
	
	hash_seq_init(&iterateStatus, fsObjHashTable[fsObjType]);

	while (true)
	{
		fsObjEntry = 
				(FsObjEntry)
						hash_seq_search(&iterateStatus);
		if (fsObjEntry == NULL)
			break;

		if (fsObjEntry->xid == InvalidTransactionId)
			continue;

		xactEntry = 
			PersistentRecovery_FindOrCreateXactEntry(
											fsObjEntry->xid, 
											&found);
		if (!found)
			xactEntry->infoKind = XACT_INFOKIND_NONE;

		DoublyLinkedHead_AddFirst(
						offsetof(FsObjEntryData, xactLinks),
						&xactEntry->fsObjEntryList,
						fsObjEntry);
	}
}

typedef struct RecordCrashTransactionAbortRecordErrContext
{
	TransactionId 		xid;

	PersistentEndXactFileSysActionInfo	*fileSysActionInfos;
	
	int createPendingCount;
} RecordCrashTransactionAbortRecordErrContext;

static void PersistentRecovery_RecordCrashTransactionAbortRecordErrContext(void *arg)
{
	RecordCrashTransactionAbortRecordErrContext	*errContext = (RecordCrashTransactionAbortRecordErrContext*) arg;

	PersistentEndXactFileSysActionInfo	*firstFileSysActionInfo;

	Assert(errContext->createPendingCount > 0);
	firstFileSysActionInfo = &errContext->fileSysActionInfos[0];

	errcontext( 
		 "Record abort transaction record for crashed transaction %u with %d 'Create Pending' file-system objects "
		 "(first file-system object %s, persistent serial number " INT64_FORMAT ", TID %s)",
		 errContext->xid,
		 errContext->createPendingCount,
		 PersistentFileSysObjName_TypeAndObjectName(&firstFileSysActionInfo->fsObjName),
		 firstFileSysActionInfo->persistentSerialNum,
		 ItemPointerToString(&firstFileSysActionInfo->persistentTid));
}

static void
PersistentRecovery_HandlePrepareBeforeCheckpoint(void)
{
	HASH_SEQ_STATUS iterateStatus;
	XactEntry xactEntry;

	if (Debug_persistent_recovery_print)
	{
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_HandlePrepareBeforeCheckpoint");
	}

	if (xactHashTable == NULL)
	{
		if (Debug_persistent_recovery_print)
		{
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "Entering PersistentRecovery_HandlePrepareBeforeCheckpoint -- no entries");
		}
		return;
	}
	
	hash_seq_init(&iterateStatus, xactHashTable);

	while (true)
	{
		xactEntry = 
				(XactEntry)
						hash_seq_search(&iterateStatus);
		if (xactEntry == NULL)
			break;

		if (xactEntry->infoKind == XACT_INFOKIND_NONE)
		{
			XLogRecPtr preparedLoc;

			if (TwoPhaseFindRecoverPostCheckpointPreparedTransactionsMapEntry(
																		xactEntry->xid,
																		&preparedLoc,
																		"PersistentRecovery_HandlePrepareBeforeCheckpoint"))
			{
				xactEntry->infoKind = XACT_INFOKIND_PREPARE;

				if ( ! Disable_persistent_recovery_logging)
				{
					elog(LOG, 
						 "Prepare Before Checkpoint: Transaction %u (None) found and marked prepared (location %s)",
						 xactEntry->xid,
						 XLogLocationToString(&preparedLoc));
				}
			}
			else
			{
				if (Debug_persistent_recovery_print)
				{
					elog(PersistentRecovery_DebugPrintLevel(), 
						 "Prepare Before Checkpoint: Transaction %u (None) not found in checkpoint prepared list",
						 xactEntry->xid);
				}
			}
		}
		else
		{
			if (Debug_persistent_recovery_print)
			{
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "Prepare Before Checkpoint: Transaction %u (%s)",
					 xactEntry->xid,
					 XactInfoKind_Name(xactEntry->infoKind));
			}
		}
	}
	
	if (Debug_persistent_recovery_print)
	{
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_HandlePrepareBeforeCheckpoint");
	}

}

/*
 * This function basically deals with finding the transactions that were left
 * half-done (or aborted) due to the crash and cleanup the file-system objects
 * by these trasnactions.
 * The hashtable "fsObjHashTable" contains list of relation objects that
 * require a persistent table change since those were related to the transactions that
 * were either committed or aborted or prepared when the crash happened.
 * If fsObjHashTable is non-empty then it means there are filesystem
 * objects (relation/ database/ filespace/ tablespace) that needs some persistent
 * change and so iterate through all the persistent filesystem object entries in
 * fsObjHashTable and try to find information about the transaction dealing with
 * the same. For each of transaction_id corresponding to the filesystem object
 * in fsObjHashTable, search is performed in the hashtable "xactHashTable" (which consists of
 * the transactions running during the crash and their state) and try to find their
 * state (whether COMMIT, ABORT). If cannot find transaction_id in the xactHashTable
 * then its considered as a inflight (infokind = XACT_INFOKIND_NONE) transaction
 * and updated accordingly (this type of transactions are handled later in
 * function as we don’t know yet if this transactions really committed or
 * aborted).
 * Once the transaction linking is done for the entries in fsObjHashTable, it
 * finds the transactions that were PREPARED before the checkpoint record
 * and so we start scanning the xactHashTable.
 * For all the entries (or transactions) in xactHashTable that were considered
 * PREPARED (infokind=XACT_INFOKIND_NONE), search is made in another hashtable
 * “crashRecoverPostCheckpointPreparedTransactions_map_ht” which consists
 * of all pre and post checkpoint inflight transactions. If we find an entry then
 * its considered as a PREPARED (infokind=XACT_INFOKIND_NONE) transaction.
 *
 * So after attaching each filesystem object in fsObjHashTable to its transactions
 * in xactHashTable and confirming if the inflight transactions are PREPARED,
 * fresh scan is made of the xactHashTable from start.
 * There can be 3 cases:
 *     -> PREPARED transactions: For this don’t know yet what to do with the
 * transaction since don’t know if it got committed or abort post the
 * checkpoint, just have pre-checkpoint information that it is prepared
 *     -> COMMIT or ABORT transactions: This means that we had an xlog Commit or Abort
 * record and hence specific action can be taken.
 *     -> Inflight transaction: These are transactions that neither have any
 * commit/abort record in xlog nor are they listed in the checkpoint prepared
 * transactions list. Don’t know the status of such transaction and hence
 * need to examine if there are any Create Pending objects corresponding to this
 * transaction. Check performed for each of filesystem object’s persistent state linked
 * to this transaction (through xactEntry->fsObjEntryList) and if object is
 * found in Create Pending state then do conclude the following:
 * - The transaction was trying to create the object, which needs to be cleaned-up
 * - The transaction needs to be aborted
 * - Persistent state be updated to Aborting Create
 *
 * Following actions are performed based on above deductions:
 * - Note the end transaction action as “Abort”
 * - Record the crashed transaction’s abort record in XLOG
 * - Mark this file-system object as “update needed” with a flag
 * - State is updated to “Aborting Create”
 * - Entry is made in “Pass 2 recovery” hashtable
 */
void
PersistentRecovery_CrashAbort(void)
{
	PersistentFsObjType fsObjType;

	HASH_SEQ_STATUS iterateStatus;
	FsObjEntry fsObjEntry;
	XactEntry xactEntry;
	int xactCount;

	ErrorContextCallback errcontext;

	RecordCrashTransactionAbortRecordErrContext recordCrashTransactionAbortRecordErrContext;
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_CrashAbort");

	if (fsObjHashTable == NULL)
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
			     "Exiting PersistentRecovery_CrashAbort (no relation hash table)");
		return;
	}

	for (fsObjType = PersistentFsObjType_First; 
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
		PersistentRecovery_AttachFsObjTypeToXact(fsObjType);

	PersistentRecovery_HandlePrepareBeforeCheckpoint();

	if (! Disable_persistent_recovery_logging)
	{
		PersistentRecovery_PrintXactAndFsObjs();
		PersistentRecovery_PrintAoMirrorResyncEofs();
	}

	hash_seq_init(&iterateStatus, xactHashTable);

	xactCount = 0;
	while (true)
	{
		bool needsAbort;
		int createPendingCount;
		int e;
		PersistentEndXactFileSysActionInfo	*fileSysActionInfos;
		PersistentEndXactRecObjects persistentObjects;
		bool abortSuccessfullyRecorded;

		xactEntry = 
				(XactEntry)
						hash_seq_search(&iterateStatus);
		if (xactEntry == NULL)
			break;

		xactCount++;

		if (xactEntry->infoKind != XACT_INFOKIND_NONE)
		{
			Assert(xactEntry->infoKind == XACT_INFOKIND_PREPARE ||
				   xactEntry->infoKind == XACT_INFOKIND_COMMIT ||
				   xactEntry->infoKind == XACT_INFOKIND_ABORT);

			/*
			 * If it is Prepared, we don't know yet what to do with the transaction.
			 *
			 * If it is known Committed or Aborted because we found an XLOG Commit or
			 * or Abort, we may have updates that will be handled later.
			 */

			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "PersistentRecovery_CrashAbort: Skipping transaction %u (state '%s')",
					 xactEntry->xid,
					 XactInfoKind_Name(xactEntry->infoKind));

			continue;
		}

		/* If checkpoint happened after Recording COMMIT, xactHashTable won't know the Status of the xact, 
		 * since no REDO records corresponding to the same would be looked into.
		 * But its incorrect without consulting CLOG to consider ABORTing the xact based on having CREATE_PENDING entry in PT
		 * Hence should check CLOG to verify if it was COMMITTED.
		 * If we don't perform this step, the recovery later would try to mark COMMITED Xact Aborted, 
		 * seeing Create-Pending entry associated with the Xact.
		 */
		bool validStatus = false;

		XidStatus status = InRecoveryTransactionIdGetStatus(xactEntry->xid, &validStatus);
		if ((validStatus == true) && (status != 0) && (status == TRANSACTION_STATUS_COMMITTED))
		{
			xactEntry->infoKind = XACT_INFOKIND_COMMIT;

			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
						"PersistentRecovery_CrashAbort: Found transaction as COMMITTED in clog, transaction %u (state '%s')",
						xactEntry->xid,
						XactInfoKind_Name(xactEntry->infoKind));

			fsObjEntry = 
				(FsObjEntry) DoublyLinkedHead_First(
						offsetof(FsObjEntryData, xactLinks),
						&xactEntry->fsObjEntryList);

			while (fsObjEntry != NULL)
			{
				if (fsObjEntry->state == PersistentFileSysState_BulkLoadCreatePending ||
						fsObjEntry->state == PersistentFileSysState_CreatePending)
				{
					if (Debug_persistent_recovery_print)
						elog(PersistentRecovery_DebugPrintLevel(), 
								"PersistentRecovery_CrashAbort: Set state of %s to 'Created'", 
								FsObjEntryToString(fsObjEntry));

					fsObjEntry->updateNeeded = true;
					fsObjEntry->state = PersistentFileSysState_Created;
				}

				fsObjEntry = 
					(FsObjEntry) DoublyLinkedHead_Next(
							offsetof(FsObjEntryData, xactLinks),
							&xactEntry->fsObjEntryList,
							fsObjEntry);
			}
			continue;
		}

		/*
		 * We don't know the status of the transaction until we examine the
		 * persistent node states.
		 */
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_CrashAbort: Checking transaction %u (state '%s')",
				 xactEntry->xid,
				 XactInfoKind_Name(xactEntry->infoKind));

		needsAbort = false;

		fsObjEntry = 
			(FsObjEntry) DoublyLinkedHead_First(
									offsetof(FsObjEntryData, xactLinks),
									&xactEntry->fsObjEntryList);
									
		createPendingCount = 0;
		while (fsObjEntry != NULL)
		{
			if (fsObjEntry->state == PersistentFileSysState_BulkLoadCreatePending ||
				fsObjEntry->state == PersistentFileSysState_CreatePending)
			{
				needsAbort = true;
				
				if (! Disable_persistent_recovery_logging)
					elog(LOG, 
 						 "Needs-Abort REDO: (New state 'Aborting Create') %s",
 						 FsObjEntryToString(fsObjEntry));

				createPendingCount++;
			}
			else if (fsObjEntry->state != PersistentFileSysState_Free)
			{
				if (needsAbort)
					elog(ERROR, "Found persistent '%s' entry among 'Create Pending' entries",
						 FsObjEntryToString(fsObjEntry));
				break;
			}

			fsObjEntry = 
				(FsObjEntry) DoublyLinkedHead_Next(
										offsetof(FsObjEntryData, xactLinks),
										&xactEntry->fsObjEntryList,
										fsObjEntry);
		}

		if (!needsAbort)
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
					 "PersistentRecovery_CrashAbort: Skipping transaction %u due to no 'Create Pending' objects",
					 xactEntry->xid);

			Assert(createPendingCount == 0);
			continue;
		}

		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_CrashAbort: Found %d 'Create Pending' persistent file-system objects for transaction %u",
				 createPendingCount,
				 xactEntry->xid);

		fileSysActionInfos = 
			(PersistentEndXactFileSysActionInfo*)
				palloc0(createPendingCount * sizeof(PersistentEndXactFileSysActionInfo));
		
		fsObjEntry = 
			(FsObjEntry) DoublyLinkedHead_First(
									offsetof(FsObjEntryData, xactLinks),
									&xactEntry->fsObjEntryList);
		e = 0;
		while (fsObjEntry != NULL)
		{
			if (fsObjEntry->state == PersistentFileSysState_BulkLoadCreatePending ||
				fsObjEntry->state == PersistentFileSysState_CreatePending)
			{
				Assert(e < createPendingCount);

				fileSysActionInfos[e].action = PersistentEndXactFileSysAction_Create;
				fileSysActionInfos[e].fsObjName = fsObjEntry->fsObjName;
				fileSysActionInfos[e].relStorageMgr = fsObjEntry->relStorageMgr;
				fileSysActionInfos[e].persistentTid = fsObjEntry->key.persistentTid;
				fileSysActionInfos[e].persistentSerialNum = fsObjEntry->persistentSerialNum;
				e++;
			}

			fsObjEntry = 
				(FsObjEntry) DoublyLinkedHead_Next(
										offsetof(FsObjEntryData, xactLinks),
										&xactEntry->fsObjEntryList,
										fsObjEntry);
		}

		PersistentEndXactRec_Init(&persistentObjects);
		PersistentEndXactRec_AddFileSysActionInfos(
										&persistentObjects,
										EndXactRecKind_Abort,
										fileSysActionInfos, 
										createPendingCount);

		
		/* Setup error traceback support for ereport() */
		recordCrashTransactionAbortRecordErrContext.xid = xactEntry->xid;
		recordCrashTransactionAbortRecordErrContext.fileSysActionInfos = fileSysActionInfos;
		recordCrashTransactionAbortRecordErrContext.createPendingCount = createPendingCount;

		errcontext.callback = PersistentRecovery_RecordCrashTransactionAbortRecordErrContext;
		errcontext.arg = (void *) &recordCrashTransactionAbortRecordErrContext;
		errcontext.previous = error_context_stack;
		error_context_stack = &errcontext;

		elog(LOG, "Recovery Aborting Transaction %u", xactEntry->xid);

		abortSuccessfullyRecorded =
			RecordCrashTransactionAbortRecord(
									xactEntry->xid, 
									&persistentObjects);
		
		/* Pop the error context stack */
		error_context_stack = errcontext.previous;
		
		pfree(fileSysActionInfos);

		fsObjEntry = 
			(FsObjEntry) DoublyLinkedHead_First(
									offsetof(FsObjEntryData, xactLinks),
									&xactEntry->fsObjEntryList);

		while (fsObjEntry != NULL)
		{
			if (fsObjEntry->state == PersistentFileSysState_BulkLoadCreatePending ||
				fsObjEntry->state == PersistentFileSysState_CreatePending)
			{
				if (Debug_persistent_recovery_print)
					elog(PersistentRecovery_DebugPrintLevel(), 
					     "PersistentRecovery_CrashAbort: Set state of %s to 'Aborting Create'", 
					     FsObjEntryToString(fsObjEntry));

				if (abortSuccessfullyRecorded)
				{
					fsObjEntry->updateNeeded = true;
					fsObjEntry->state = PersistentFileSysState_AbortingCreate;
				}
				else
				{
					elog(WARNING, "Not setting the state of '%s' to 'Aborting Create'",
						 FsObjEntryToString(fsObjEntry));
				}
			}
			else if (fsObjEntry->state != PersistentFileSysState_Free)
				elog(ERROR, "Unexpected persistent file-system state: %d",
					 fsObjEntry->state);

			fsObjEntry = 
				(FsObjEntry) DoublyLinkedHead_Next(
										offsetof(FsObjEntryData, xactLinks),
										&xactEntry->fsObjEntryList,
										fsObjEntry);
		}

	}

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
			 "Exiting PersistentRecovery_CrashAbort (transaction count %d)", xactCount);
}


static void
PersistentRecovery_UpdateType(
	PersistentFsObjType fsObjType)
{
	HASH_SEQ_STATUS iterateStatus;
	FsObjEntry fsObjEntry;
	PersistentFileSysState	state;
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_UpdateType %s",
		     PersistentFileSysObjName_TypeName(fsObjType));

	hash_seq_init(&iterateStatus, fsObjHashTable[fsObjType]);

	persistentRecoveryCount = 0;

	while (true)
	{
		fsObjEntry = 
				(FsObjEntry)
						hash_seq_search(&iterateStatus);
		if (fsObjEntry == NULL)
			break;

		if (fsObjEntry->updateNeeded)
		{
			persistentRecoveryCount++;

			if (! Disable_persistent_recovery_logging)
			{
				elog(LOG,
				     "End-Xact-Update-Needed REDO [%d]: %s", persistentRecoveryCount, FsObjEntryToString(fsObjEntry));
			}

			state = fsObjEntry->state;
			switch (state)
			{
				case PersistentFileSysState_Created:
					PersistentFileSysObj_Created(
									&fsObjEntry->fsObjName,
									&fsObjEntry->key.persistentTid,
									fsObjEntry->persistentSerialNum,
									/* retryPossible */ false);
					break;
				
				case PersistentFileSysState_AbortingCreate:
					fsObjEntry->stateChangeResult =
						PersistentFileSysObj_MarkAbortingCreate(
									&fsObjEntry->fsObjName,
									&fsObjEntry->key.persistentTid,
									fsObjEntry->persistentSerialNum,
									/* retryPossible */ false);
					break;

				case PersistentFileSysState_DropPending:
					fsObjEntry->stateChangeResult =
						PersistentFileSysObj_MarkDropPending(
									&fsObjEntry->fsObjName,
									&fsObjEntry->key.persistentTid,
									fsObjEntry->persistentSerialNum,
									/* retryPossible */ false);
					break;
				default:
					elog(ERROR, "Unexpected persistent file-system state: %d",
						 state);
			}
		}
	}
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_UpdateType %s, count %d",
		     PersistentFileSysObjName_TypeName(fsObjType),
		     persistentRecoveryCount);
}

/*
 * This is responsible for updating the persistent state for the file-system
 * objects which was found earlier in recovery. On call to this function the hashtable
 * fsObjHashTable contains the list of relation objects that require a
 * persistent table change since those were related to the transactions that
 * were either committed or aborted or prepared when the crash happened.
 * For each of the entry in the fsObjHashTable, if the flag for “update needed”
 * is set, then will update the persistent state to one of the following
 * depending on the “state” in the entry found:
 * Created or Drop Pending – incase of committed transaction for Create or drop
 * respectively.
 * Aborting Create – incase of aborted transaction.
 */
void
PersistentRecovery_Update(void)
{
	PersistentFsObjType fsObjType;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_Update");

	if (fsObjHashTable == NULL)
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "Exiting PersistentRecovery_Update (no relation hash table)");
		return;
	}

	for (fsObjType = PersistentFsObjType_First; 
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	 	PersistentRecovery_UpdateType(fsObjType);
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
			 "Exiting PersistentRecovery_Update");
}

static void
PersistentRecovery_DropType(
	PersistentFsObjType fsObjType)
{
	HASH_SEQ_STATUS iterateStatus;
	FsObjEntry fsObjEntry;
	PersistentFileSysState	state;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_DropType %s",
		     PersistentFileSysObjName_TypeName(fsObjType));

	hash_seq_init(&iterateStatus, fsObjHashTable[fsObjType]);

	persistentRecoveryCount = 0;

	while (true)
	{
		fsObjEntry = 
				(FsObjEntry)
						hash_seq_search(&iterateStatus);
		if (fsObjEntry == NULL)
			break;

		state = fsObjEntry->state;
		
		if (state == PersistentFileSysState_AbortingCreate ||
			state == PersistentFileSysState_DropPending)
		{

			if (! fsObjEntry->foundInTable)
			{
				if (! Disable_persistent_recovery_logging)
				{
					elog(LOG, "Crash recovery skipping drop %s as tuple TID %s deleted in table",
						 FsObjEntryToString(fsObjEntry),
						 ItemPointerToString(&fsObjEntry->key.persistentTid));
				}
				continue;
			}

			if (fsObjEntry->stateChangeResult != PersistentFileSysObjStateChangeResult_StateChangeOk)
			{
				if (fsObjEntry->stateChangeResult == PersistentFileSysObjStateChangeResult_ErrorSuppressed)
				{
					elog(WARNING, 
						 "Crash recovery skipping drop for %s with State-Change result '%s'", 
						 FsObjEntryToString(fsObjEntry),
						 PersistentFileSysObjStateChangeResult_Name(fsObjEntry->stateChangeResult));
				}
				else if (! Disable_persistent_recovery_logging)
				{
					elog(LOG, 
						 "Recovery Drop: Skipping drop for %s with State-Change result '%s'", 
						 FsObjEntryToString(fsObjEntry),
						 PersistentFileSysObjStateChangeResult_Name(fsObjEntry->stateChangeResult));
				}
				continue;
			}

			if (fsObjEntry->mirrorExistenceState == MirroredObjectExistenceState_OnlyMirrorDropRemains)
			{
				/* This file-system object will be dropped during resync */
				if(Debug_persistent_recovery_print)
					elog(PersistentRecovery_DebugPrintLevel(),
						"Skipping drop for %s as only mirror drop remains", FsObjEntryToString(fsObjEntry));
				continue;
			}

			persistentRecoveryCount++;

			elog(LOG, "Recovery Drop [%d]: %s", 
				persistentRecoveryCount, FsObjEntryToString(fsObjEntry));

			if (fsObjType == PersistentFsObjType_RelationFile &&
				!PersistentFileSysRelStorageMgr_IsValid(fsObjEntry->relStorageMgr))
				elog(ERROR, "Relation storage manager for persistent '%s' for Crash Recovery is invalid (%d)",
					 PersistentFileSysObjName_TypeAndObjectName(&fsObjEntry->fsObjName),
					 fsObjEntry->relStorageMgr);
			
			PersistentFileSysObj_DropObject(
							&fsObjEntry->fsObjName,
							fsObjEntry->relStorageMgr,
							/* relationName, as don't know here */ NULL,
							&fsObjEntry->key.persistentTid,
							fsObjEntry->persistentSerialNum,
							/* ignoreNonExistence */ true,
							Debug_persistent_recovery_print,
							PersistentRecovery_DebugPrintLevel());
		}
	}

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_DropType %s, count %d",
		     PersistentFileSysObjName_TypeName(fsObjType),
		     persistentRecoveryCount);
}

/*
 * This is responsible for dropping the file-system object marked as Aborting
 * Create and Drop Pending during recovery.
 * It basically loops through the fsObjHashTable to check for entries with
 * persistent state to be either “Aborting Create” or “Drop Pending”. If it
 * finds then it proceeds with the drop of the same. Note – the entries that
 * have mirror existence state as “Only Mirror Drop Remains” (which means that
 * the object is dropped on primary but not on mirror) are not dropped in this
 * step (crash recovery). They are dropped during the filerep resync operation.
 */
void
PersistentRecovery_Drop(void)
{
	PersistentFsObjType fsObjType;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_Scan");

	for (fsObjType = PersistentFsObjType_First; 
		 fsObjType <= PersistentFsObjType_Last;
		 fsObjType++)
	 	PersistentRecovery_DropType(fsObjType);

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_Scan");
}

/*
 * This is responsible for syncing the commit and loss eofs in case of AO
 * inserts. This function uses the hashtable
 * recoveryAppendOnlyMirrorResyncEofsTable, which contains the information
 * regarding the resync eof for the mirrors. Checks for each entry in
 * recoveryAppendOnlyMirrorResyncEofsTable and corresponding entry (based on
 * persistent tid) in the fsObjHashTable, there are following cases possible for
 * this check:
 * -> If it doesn’t find entry in fsObjHashTable, then it concludes that the
 * entry in the recoveryAppendOnlyMirrorResyncEofsTable has no persistent entry
 * and the update is obsolete and thus we don’t need to update the eofs.
 * ->  If the persistent state of the object is not “Created”, then also don’t
 * need to update the eofs as it is in the non-created state (CreatePending,
 * AbortingCreate, DropPending or Free).
 * -> If the persistent serial number in fsObjHashTable is greater than the
 * one in recoveryAppendOnlyMirrorResyncEofsTable, then it concludes that the
 * entry is superseded by newer persistent entry and hence no update of eof is
 * required. (Note - If it finds the persistent serial number in fsObjHashTable
 * to be smaller than the one in recoveryAppendOnlyMirrorResyncEofsTable then it
 * complains with an error).
 * -> If finds the mirror loss eof to be equal to -1 for entry in
 * recoveryAppendOnlyMirrorResyncEofsTable, then knows that need a mirror catch
 * up while resyncing the eof. If mirror catch up is required, don’t need to
 * update the mirror loss eof (as there is no change), but if its not required
 * then need to update the mirror loss eof to the mirror new eof (as there is no
 * loss) from the entry in recoveryAppendOnlyMirrorResyncEofsTable currently
 * looping through.
 */
void
PersistentRecovery_UpdateAppendOnlyMirrorResyncEofs(void)
{
	HASH_SEQ_STATUS iterateStatus;
	AoMirrorResyncEofsEntry eofsEntry;

	FsObjEntry fsObjEntry;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_UpdateAppendOnlyMirrorResyncEofs");

	if (recoveryAppendOnlyMirrorResyncEofsTable == NULL)
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "Entering PersistentRecovery_UpdateAppendOnlyMirrorResyncEofs -- no entries");
		return;
	}
	
	persistentRecoveryCount = 0;

	hash_seq_init(&iterateStatus, recoveryAppendOnlyMirrorResyncEofsTable);

	while (true)
	{
		bool mirrorCatchupRequired;

		eofsEntry = 
			(AoMirrorResyncEofsEntry)
						hash_seq_search(&iterateStatus);
		if (eofsEntry == NULL)
			break;

		persistentRecoveryCount++;

		fsObjEntry =
				PersistentRecovery_FindFsObjEntry(
									PersistentFsObjType_RelationFile,
									&eofsEntry->key.persistentTid);
		if (fsObjEntry == NULL)
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
				     "Update Append-Only EOFs REDO: %s has no persistent entry (i.e. update is obsolete)",
					 AoMirrorResyncEofsEntryToString(eofsEntry));
			continue;
		}

		if (fsObjEntry->state != PersistentFileSysState_Created)
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
				     "Update Append-Only EOFs REDO: %s has persistent entry %s in non-Created state",
					 AoMirrorResyncEofsEntryToString(eofsEntry),
					 FsObjEntryToString(fsObjEntry));
			continue;
		}

		if (fsObjEntry->persistentSerialNum > eofsEntry->persistentSerialNum)
		{
			if (Debug_persistent_recovery_print)
				elog(PersistentRecovery_DebugPrintLevel(), 
				     "Update Append-Only EOFs REDO: %s is superceeded by newer persistent entry %s",
					 AoMirrorResyncEofsEntryToString(eofsEntry),
					 FsObjEntryToString(fsObjEntry));
			continue;
		}
		if (fsObjEntry->persistentSerialNum < eofsEntry->persistentSerialNum)
			elog(ERROR,
				 "Update Append-Only EOFs REDO: %s is newer than persistent entry %s",
				 AoMirrorResyncEofsEntryToString(eofsEntry),
				 FsObjEntryToString(fsObjEntry));
		
		if (eofsEntry->mirrorLossEof == INT64CONST(-1))
		{
			mirrorCatchupRequired = true;
		}
		else
		{
			if (eofsEntry->mirrorLossEof != eofsEntry->mirrorNewEof)
				elog(ERROR, "mirror loss EOF " INT64_FORMAT " doesn't match new EOF " INT64_FORMAT,
					 eofsEntry->mirrorLossEof,
					 eofsEntry->mirrorNewEof);
			mirrorCatchupRequired = false;
		}
		
		PersistentFileSysObj_UpdateAppendOnlyMirrorResyncEofs(
														&eofsEntry->relFileNode,
														eofsEntry->segmentFileNum,
														&eofsEntry->key.persistentTid,
														eofsEntry->persistentSerialNum,
														mirrorCatchupRequired,
														eofsEntry->mirrorNewEof,
														/* recovery */ true,
														/* flushToXLog */ false);
		
	}
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_UpdateAppendOnlyMirrorResyncEofs count %d",
		     persistentRecoveryCount);

}

typedef struct RedoRelationEntryDataKey
{

	ItemPointerData	persistentTid;

} RedoRelationEntryDataKey;

typedef struct RedoRelationEntryData
{
	RedoRelationEntryDataKey	key;

	RelFileNode		relFileNode;

	int64			persistentSerialNum;

	// UNDONE: mirrorExistence
} RedoRelationEntryData;
typedef RedoRelationEntryData *RedoRelationEntry;

void
PersistentRecovery_SerializeRedoRelationFile(
	int redoRelationFile)
{
	HASH_SEQ_STATUS iterateStatus;
	FsObjEntry fsObjEntry;

	RedoRelationEntryData redoRelationEntryData;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_SerializeRedoRelationFile");

	hash_seq_init(
			&iterateStatus, 
			fsObjHashTable[PersistentFsObjType_RelationFile]);

	persistentRecoveryCount = 0;

	while (true)
	{
		int writeLen;

		fsObjEntry = 
				(FsObjEntry)
						hash_seq_search(&iterateStatus);
		if (fsObjEntry == NULL)
			break;

		if (fsObjEntry->state == PersistentFileSysState_Free ||
			fsObjEntry->state == PersistentFileSysState_AbortingCreate ||
			fsObjEntry->state == PersistentFileSysState_DropPending)
			continue;

		Assert(fsObjEntry->state == PersistentFileSysState_BulkLoadCreatePending ||
			   fsObjEntry->state == PersistentFileSysState_CreatePending ||
			   fsObjEntry->state == PersistentFileSysState_Created);

		MemSet(&redoRelationEntryData.key, 0, sizeof(RedoRelationEntryDataKey));	// Zero out any padding.
		redoRelationEntryData.key.persistentTid = fsObjEntry->key.persistentTid;
		
		redoRelationEntryData.relFileNode = PersistentFileSysObjName_GetRelFileNode(&fsObjEntry->fsObjName);
		redoRelationEntryData.persistentSerialNum = fsObjEntry->persistentSerialNum;

		writeLen = write(redoRelationFile, &redoRelationEntryData, sizeof(RedoRelationEntryData));
		if (writeLen != sizeof(RedoRelationEntryData))
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write redo relation file : %m")));
		}
		persistentRecoveryCount++;
		
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_SerializeRedoRelationFile: %u/%u/%u, serial number " INT64_FORMAT ", TID %s",
			     redoRelationEntryData.relFileNode.spcNode,
			     redoRelationEntryData.relFileNode.dbNode,
			     redoRelationEntryData.relFileNode.relNode,
			     redoRelationEntryData.persistentSerialNum,
			     ItemPointerToString(&redoRelationEntryData.key.persistentTid));
	}
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_SerializeRedoRelationFile count %d",
		     persistentRecoveryCount);
}

static HTAB* redoRelationHashTable = NULL;

static void
PersistentRecovery_RedoRelationHashTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(RedoRelationEntryDataKey);
	info.entrysize = sizeof(RedoRelationEntryData);
	info.hash = tag_hash;

	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	redoRelationHashTable = hash_create("Redo Relation", 10, &info, hash_flags);
}

static RedoRelationEntry
PersistentRecovery_CreateRedoRelationEntry(
	RedoRelationEntry redoRelationEntry)
{
	RedoRelationEntry entry;

	bool found;

	if(RelFileNode_IsEmpty(&redoRelationEntry->relFileNode))
		elog(ERROR, "Invalid RelFileNode (0,0,0)");

	if (redoRelationHashTable == NULL)
		PersistentRecovery_RedoRelationHashTableInit();

	entry = 
			(RedoRelationEntry) 
					hash_search(redoRelationHashTable,
								(void *) &redoRelationEntry->key,
								HASH_ENTER,
								&found);

	if (found)
	{
		Assert(entry != NULL);
		elog(ERROR, 
			 "Duplicate redo relation entry: existing (%u/%u/%u, serial number " INT64_FORMAT ", TID %s), "
			 "new (%u/%u/%u, serial number " INT64_FORMAT ", TID %s)",
		     entry->relFileNode.spcNode,
		     entry->relFileNode.dbNode,
		     entry->relFileNode.relNode,
			 entry->persistentSerialNum,
		     ItemPointerToString(&entry->key.persistentTid),
			 redoRelationEntry->relFileNode.spcNode,
			 redoRelationEntry->relFileNode.dbNode,
			 redoRelationEntry->relFileNode.relNode,
		     redoRelationEntry->persistentSerialNum,
		     ItemPointerToString2(&redoRelationEntry->key.persistentTid));
	}

	entry->relFileNode = redoRelationEntry->relFileNode;
	entry->persistentSerialNum = redoRelationEntry->persistentSerialNum;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
			 "Created redo relation entry: %u/%u/%u, serial number " INT64_FORMAT ", TID %s",
			 entry->relFileNode.spcNode,
			 entry->relFileNode.dbNode,
			 entry->relFileNode.relNode,
			 entry->persistentSerialNum,
			 ItemPointerToString(&entry->key.persistentTid));

	return entry;
}

static RedoRelationEntry
PersistentRecovery_FindRedoRelationEntry(
	ItemPointer 	persistentTid)
{
	RedoRelationEntryDataKey key;

	RedoRelationEntry foundEntry;

	bool found;

	if (redoRelationHashTable == NULL)
		PersistentRecovery_RedoRelationHashTableInit();


	MemSet(&key, 0, sizeof(RedoRelationEntryDataKey));
	key.persistentTid = *persistentTid;

	foundEntry = 
			(RedoRelationEntry) 
					hash_search(redoRelationHashTable,
								(void *) &key,
								HASH_FIND,
								&found);
	if (!found)
		return NULL;

	return foundEntry;
}


void
PersistentRecovery_DeserializeRedoRelationFile(
	int redoRelationFile)
{
	RedoRelationEntryData redoRelationEntryData;

	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Entering PersistentRecovery_DeserializeRedoRelationFile");

	while(true)
	{
		int readLen;

		errno = 0;
		readLen = read(redoRelationFile, &redoRelationEntryData, sizeof(RedoRelationEntryData));

		if (readLen == 0)
			break;
		else if (readLen != sizeof(RedoRelationEntryData) && errno == 0)
			elog(ERROR, "Bad redo relation entry length (expected %d and found %d)",
			     (int)sizeof(RedoRelationEntryData), readLen);
		else if (errno != 0)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("error reading redo relation file: %m")));
		}
		
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_DeserializeRedoRelationFile: %u/%u/%u, serial number " INT64_FORMAT ", TID %s",
			     redoRelationEntryData.relFileNode.spcNode,
			     redoRelationEntryData.relFileNode.dbNode,
			     redoRelationEntryData.relFileNode.relNode,
			     redoRelationEntryData.persistentSerialNum,
			     ItemPointerToString(&redoRelationEntryData.key.persistentTid));

		if(RelFileNode_IsEmpty(&redoRelationEntryData.relFileNode))
			elog(ERROR, "Invalid RelFileNode (0,0,0)");

		PersistentRecovery_CreateRedoRelationEntry(&redoRelationEntryData);
	}
	
	if (Debug_persistent_recovery_print)
		elog(PersistentRecovery_DebugPrintLevel(), 
		     "Exiting PersistentRecovery_DeserializeRedoRelationFile count %d",
		     persistentRecoveryCount);
}

static bool
PersistentRecovery_RedoRelationExists(
	ItemPointer		persistentTid,
	
	int64 			persistentSerialNum,
	
	RelFileNode 	*relFileNode)	
{
	RedoRelationEntry redoRelationEntry;

	redoRelationEntry = PersistentRecovery_FindRedoRelationEntry(persistentTid);
	if (redoRelationEntry == NULL)
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_RedoRelationExists: TID %s is not in the currently 'Created' set (serial number " INT64_FORMAT ", %u/%u/%u)",
				 ItemPointerToString(persistentTid),
				 persistentSerialNum,
				 relFileNode->spcNode,
				 relFileNode->dbNode,
				 relFileNode->relNode);
		return false;
	}
	if (redoRelationEntry->persistentSerialNum != persistentSerialNum)
	{
		if (Debug_persistent_recovery_print)
			elog(PersistentRecovery_DebugPrintLevel(), 
				 "PersistentRecovery_RedoRelationExists: TID %s entry from the 'Created' set has different serial number (serial number " INT64_FORMAT ", %u/%u/%u) indicating XLOG for persistent obsolete file-system object (obsolete serial number " INT64_FORMAT ", %u/%u/%u)",
				 ItemPointerToString(persistentTid),
				 redoRelationEntry->persistentSerialNum,
				 redoRelationEntry->relFileNode.spcNode,
				 redoRelationEntry->relFileNode.dbNode,
				 redoRelationEntry->relFileNode.relNode,
				 persistentSerialNum,
 				 relFileNode->spcNode,
 				 relFileNode->dbNode,
 				 relFileNode->relNode);
		return false;
	}

	return true;
}

