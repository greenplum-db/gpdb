/*-------------------------------------------------------------------------
 *
 * cdbpersistentfilespace.c
 *
 * Copyright (c) 2009-2010, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/palloc.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/persistentfilesysobjname.h"
#include "access/transam.h"
#include "access/xlogmm.h"
#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_persistent.h"
#include "catalog/gp_segment_config.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "postmaster/postmaster.h"
#include "postmaster/primary_mirror_mode.h"
#include "storage/ipc.h"
#include "storage/itemptr.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbsharedoidsearch.h"
#include "cdb/cdbdirectopen.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentfilespace.h"

typedef struct PersistentFilespaceSharedData
{

	PersistentFileSysObjSharedData		fileSysObjSharedData;

} PersistentFilespaceSharedData;

#define PersistentFilespaceData_StaticInit {PersistentFileSysObjData_StaticInit}

typedef struct PersistentFilespaceData
{

	PersistentFileSysObjData		fileSysObjData;

} PersistentFilespaceData;

typedef struct FilespaceDirEntryKey
{
	Oid	filespaceOid;
} FilespaceDirEntryKey;

typedef struct FilespaceDirEntryData
{
	FilespaceDirEntryKey	key;

	int16	dbId1;
	char	locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen];

	int16	dbId2;
	char	locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen];

	PersistentFileSysState	state;

	int64					persistentSerialNum;
	ItemPointerData 		persistentTid;

} FilespaceDirEntryData;
typedef FilespaceDirEntryData *FilespaceDirEntry;


/*
 * Global Variables
 */
PersistentFilespaceSharedData	*persistentFilespaceSharedData = NULL;
static HTAB *persistentFilespaceSharedHashTable = NULL;

PersistentFilespaceData	persistentFilespaceData = PersistentFilespaceData_StaticInit;

static void PersistentFilespace_VerifyInitScan(void)
{
	if (persistentFilespaceSharedData == NULL)
		elog(PANIC, "Persistent filespace information shared-memory not setup");

	PersistentFileSysObj_VerifyInitScan();
}

/*
 * Return the hash entry for a filespace.
 */
static FilespaceDirEntry
PersistentFilespace_FindDirUnderLock(
	Oid			filespaceOid)
{
	bool			found;

	FilespaceDirEntry	filespaceDirEntry;

	FilespaceDirEntryKey key;

	if (persistentFilespaceSharedHashTable == NULL)
		elog(PANIC, "Persistent filespace information shared-memory not setup");

	key.filespaceOid = filespaceOid;

	filespaceDirEntry =
			(FilespaceDirEntry)
					hash_search(persistentFilespaceSharedHashTable,
								(void *) &key,
								HASH_FIND,
								&found);
	if (!found)
		return NULL;

	return filespaceDirEntry;
}

static FilespaceDirEntry
PersistentFilespace_CreateDirUnderLock(
	Oid			filespaceOid)
{
	bool			found;

	FilespaceDirEntry	filespaceDirEntry;

	FilespaceDirEntryKey key;

	if (persistentFilespaceSharedHashTable == NULL)
		elog(PANIC, "Persistent filespace information shared-memory not setup");

	key.filespaceOid = filespaceOid;

	filespaceDirEntry =
			(FilespaceDirEntry)
					hash_search(persistentFilespaceSharedHashTable,
								(void *) &key,
								HASH_ENTER_NULL,
								&found);

	if (filespaceDirEntry == NULL)
		elog(ERROR, "Out of shared-memory for persistent filespaces");

	filespaceDirEntry->state = 0;
	filespaceDirEntry->persistentSerialNum = 0;
	MemSet(&filespaceDirEntry->persistentTid, 0, sizeof(ItemPointerData));

	return filespaceDirEntry;
}

static void
PersistentFilespace_RemoveDirUnderLock(
	FilespaceDirEntry	filespaceDirEntry)
{
	FilespaceDirEntry	removeFilespaceDirEntry;

	if (persistentFilespaceSharedHashTable == NULL)
		elog(PANIC, "Persistent filespace information shared-memory not setup");

	removeFilespaceDirEntry =
				(FilespaceDirEntry)
						hash_search(persistentFilespaceSharedHashTable,
									(void *) &filespaceDirEntry->key,
									HASH_REMOVE,
									NULL);

	if (removeFilespaceDirEntry == NULL)
		elog(ERROR, "Trying to delete entry that does not exist");
}


// -----------------------------------------------------------------------------
// Scan
// -----------------------------------------------------------------------------

static bool PersistentFilespace_ScanTupleCallback(
	ItemPointer 			persistentTid,
	int64					persistentSerialNum,
	Datum					*values)
{
	Oid		filespaceOid;

	int16	dbId1;
	char	locationBlankPadded1[FilespaceLocationBlankPaddedWithNullTermLen];

	int16	dbId2;
	char	locationBlankPadded2[FilespaceLocationBlankPaddedWithNullTermLen];

	PersistentFileSysState	state;

	int64	createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState	mirrorExistenceState;

	int32					reserved;
	TransactionId			parentXid;
	int64					serialNum;

	FilespaceDirEntry filespaceDirEntry;

	GpPersistentFilespaceNode_GetValues(
									values,
									&filespaceOid,
									&dbId1,
									locationBlankPadded1,
									&dbId2,
									locationBlankPadded2,
									&state,
									&createMirrorDataLossTrackingSessionNum,
									&mirrorExistenceState,
									&reserved,
									&parentXid,
									&serialNum);

	filespaceDirEntry =
			PersistentFilespace_CreateDirUnderLock(
											filespaceOid);

	filespaceDirEntry->dbId1 = dbId1;
	memcpy(filespaceDirEntry->locationBlankPadded1, locationBlankPadded1, FilespaceLocationBlankPaddedWithNullTermLen);

	filespaceDirEntry->dbId2 = dbId2;
	memcpy(filespaceDirEntry->locationBlankPadded2, locationBlankPadded2, FilespaceLocationBlankPaddedWithNullTermLen);

	filespaceDirEntry->state = state;
	filespaceDirEntry->persistentSerialNum = serialNum;
	filespaceDirEntry->persistentTid = *persistentTid;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "PersistentFilespace_ScanTupleCallback: filespace %u, dbId1 %d, dbId2 %d, state '%s', mirror existence state '%s', TID %s, serial number " INT64_FORMAT,
			 filespaceOid,
			 dbId1,
			 dbId2,
			 PersistentFileSysObjState_Name(state),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 ItemPointerToString2(persistentTid),
			 persistentSerialNum);

	return true;	// Continue.
}

//------------------------------------------------------------------------------

extern void PersistentFilespace_LookupTidAndSerialNum(
	Oid 		filespaceOid,
				/* The filespace OID for the lookup. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_filespace_node tuple for the rel file */

	int64			*persistentSerialNum)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	FilespaceDirEntry filespaceDirEntry;

	PersistentFilespace_VerifyInitScan();

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	filespaceDirEntry =
				PersistentFilespace_FindDirUnderLock(
												filespaceOid);
	if (filespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent filespace entry %u",
			 filespaceOid);

	*persistentTid = filespaceDirEntry->persistentTid;
	*persistentSerialNum = filespaceDirEntry->persistentSerialNum;

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;
}


// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static void PersistentFilespace_AddTuple(
	FilespaceDirEntry filespaceDirEntry,

	int64			createMirrorDataLossTrackingSessionNum,

	MirroredObjectExistenceState mirrorExistenceState,

	int32			reserved,

	TransactionId 	parentXid,

	bool			flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Oid filespaceOid = filespaceDirEntry->key.filespaceOid;

	Datum values[Natts_gp_persistent_filespace_node];

	GpPersistentFilespaceNode_SetDatumValues(
										values,
										filespaceOid,
										filespaceDirEntry->dbId1,
										filespaceDirEntry->locationBlankPadded1,
										filespaceDirEntry->dbId2,
										filespaceDirEntry->locationBlankPadded2,
										filespaceDirEntry->state,
										createMirrorDataLossTrackingSessionNum,
										mirrorExistenceState,
										reserved,
										parentXid,
										/* persistentSerialNum */ 0);	// This will be set by PersistentFileSysObj_AddTuple.

	PersistentFileSysObj_AddTuple(
							PersistentFsObjType_FilespaceDir,
							values,
							flushToXLog,
							&filespaceDirEntry->persistentTid,
							&filespaceDirEntry->persistentSerialNum);
}

static void PersistentFilespace_BlankPadCopyLocation(
	char locationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen],
	char *location)
{
	int len;
	int blankPadLen;

	if (location != NULL)
	{
		len = strlen(location);
		if (len > FilespaceLocationBlankPaddedWithNullTermLen - 1)
			elog(ERROR, "Location '%s' is too long (found %d characaters -- expected no more than %d characters)",
				 location,
			     len,
			     FilespaceLocationBlankPaddedWithNullTermLen - 1);
	}
	else
		len = 0;

	if (len > 0)
		memcpy(locationBlankPadded, location, len);

	blankPadLen = FilespaceLocationBlankPaddedWithNullTermLen - 1 - len;
	if (blankPadLen > 0)
		MemSet(&locationBlankPadded[len], ' ', blankPadLen);

	locationBlankPadded[FilespaceLocationBlankPaddedWithNullTermLen - 1] = '\0';
}

void PersistentFilespace_ConvertBlankPaddedLocation(
	char 		**filespaceLocation,

	char 		*locationBlankPadded,

	bool		isPrimary)
{
	char *firstBlankChar;
	int len;

	firstBlankChar = strchr(locationBlankPadded, ' ');
	if (firstBlankChar != NULL)
	{
		len = firstBlankChar - locationBlankPadded;
		if (len == 0)
		{
			if (isPrimary)
				elog(ERROR, "Expecting non-empty primary location");

			*filespaceLocation = NULL;
			return;
		}

		*filespaceLocation = (char*)palloc(len + 1);
		memcpy(*filespaceLocation, locationBlankPadded, len);
		(*filespaceLocation)[len] = '\0';
	}
	else
	{
		/*
		 * Whole location is non-blank characters...
		 */
		len = strlen(locationBlankPadded) + 1;
		if (len != FilespaceLocationBlankPaddedWithNullTermLen)
			elog(ERROR, "Incorrect format for blank-padded filespace location");

		*filespaceLocation = pstrdup(locationBlankPadded);
	}
}

bool PersistentFilespace_TryGetPrimaryAndMirrorUnderLock(
	Oid 		filespaceOid,
				/* The filespace OID to lookup. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */

	char **mirrorFilespaceLocation)
				/* The primary filespace directory path.  Return NULL for global and base.
				 * Or, returns NULL when mirror not configured. */
{
	FilespaceDirEntry filespaceDirEntry;

	int16 primaryDbId;

	char *primaryBlankPadded = NULL;
	char *mirrorBlankPadded = NULL;

	*primaryFilespaceLocation = NULL;
	*mirrorFilespaceLocation = NULL;

#ifdef MASTER_MIRROR_SYNC
	/*
	 * Can't rely on persistent tables or memory structures on the standby so
	 * get it from the cache maintained by the master mirror sync code
	 */
	if (IsStandbyMode())
	{
		return mmxlog_filespace_get_path(
									filespaceOid,
									primaryFilespaceLocation);
	}
#endif

	/*
	 * Important to make this call AFTER we check if we are the Standby Master.
	 */
	PersistentFilespace_VerifyInitScan();

	filespaceDirEntry =
				PersistentFilespace_FindDirUnderLock(
												filespaceOid);
	if (filespaceDirEntry == NULL)
		return false;

	/*
	 * The persistent_filespace_node_table contains the paths for both the
	 * primary and mirror nodes, and the table is the same on both sides of the
	 * mirror.  When it was first created the primary put its location first,
	 * but we don't know if we were the primary when it was created or not.  To
	 * determine which path corresponds to this node we compare our dbid to the
	 * one stored in the table.
	 */
	primaryDbId = GpIdentity.dbid;
	if (filespaceDirEntry->dbId1 == primaryDbId)
	{
		/* dbid == dbid1 */
		primaryBlankPadded = filespaceDirEntry->locationBlankPadded1;
		mirrorBlankPadded = filespaceDirEntry->locationBlankPadded2;
	}
	else if (filespaceDirEntry->dbId2 == primaryDbId)
	{
		/* dbid == dbid2 */
		primaryBlankPadded = filespaceDirEntry->locationBlankPadded2;
		mirrorBlankPadded = filespaceDirEntry->locationBlankPadded1;
	}
	else
	{
		/*
		 * The dbid check above does not work for the Master Node during
		 * initial startup, because the master doesn't yet know its own dbid.
		 * To handle this we special case for the master node the master
		 * always considers the first entry as the correct location.
		 *
		 * Note: This design may need to  be reconsidered to handle standby
		 * masters!
		 */
		PrimaryMirrorMode mode;

		getPrimaryMirrorStatusCodes(&mode, NULL, NULL, NULL);

		if (mode == PMModeMaster)
		{
			/* Master node */
			primaryBlankPadded = filespaceDirEntry->locationBlankPadded1;
			mirrorBlankPadded = filespaceDirEntry->locationBlankPadded2;
		}
		else
		{
			/*
			 * Current dbid matches neither dbid in table and was not started
			 * as a master node.
			 */
			ereport(FATAL,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Unable to determine dbid for filespace lookup")));
		}
	}

	/* These should both have been populated by one of the cases above */
	Assert(primaryBlankPadded);
	Assert(mirrorBlankPadded);

	PersistentFilespace_ConvertBlankPaddedLocation(
											primaryFilespaceLocation,
											primaryBlankPadded,
											/* isPrimary */ true);

	PersistentFilespace_ConvertBlankPaddedLocation(
											mirrorFilespaceLocation,
											mirrorBlankPadded,
											/* isPrimary */ false);

	return true;
}

void PersistentFilespace_GetPrimaryAndMirrorUnderLock(
	Oid 		filespaceOid,
				/* The filespace OID to lookup. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */

	char **mirrorFilespaceLocation)
				/* The primary filespace directory path.  Return NULL for global and base.
				 * Or, returns NULL when mirror not configured. */
{
	/*
	 * Do not call PersistentFilespace_VerifyInitScan here to allow
	 * PersistentFilespace_TryGetPrimaryAndMirrorUnderLock to handle the Standby Master
	 * special case.
	 */

	if (!PersistentFilespace_TryGetPrimaryAndMirrorUnderLock(
													filespaceOid,
													primaryFilespaceLocation,
													mirrorFilespaceLocation))
	{
		elog(ERROR, "Did not find persistent filespace entry %u",
			 filespaceOid);
	}
}

void PersistentFilespace_GetPrimaryAndMirror(
	Oid 		filespaceOid,
 	            /* The filespace OID to lookup */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */

	char **mirrorFilespaceLocation)
				/* The primary filespace directory path.  Return NULL for global and base.
				 * Or, returns NULL when mirror not configured. */
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	/*
	 * Do not call PersistentFilespace_VerifyInitScan here to allow
	 * PersistentFilespace_TryGetPrimaryAndMirrorUnderLock to handle the Standby Master
	 * special case.
	 */

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	if (!PersistentFilespace_TryGetPrimaryAndMirrorUnderLock(
											filespaceOid,
											primaryFilespaceLocation,
											mirrorFilespaceLocation))
	{
		elog(ERROR, "Did not find persistent filespace entry %u",
			 filespaceOid);
	}

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;
}

// -----------------------------------------------------------------------------
// State Change
// -----------------------------------------------------------------------------

/*
 * Indicate we intend to create a filespace file as part of the current transaction.
 *
 * An XLOG IntentToCreate record is generated that will guard the subsequent file-system
 * create in case the transaction aborts.
 *
 * After 1 or more calls to this routine to mark intention about filespace files that are going
 * to be created, call ~_DoPendingCreates to do the actual file-system creates.  (See its
 * note on XLOG flushing).
 */
void PersistentFilespace_MarkCreatePending(
	Oid 		filespaceOid,
				/* The filespace where the filespace lives. */

	int16		primaryDbId,

	char 		*primaryFilespaceLocation,
				/*
				 * The primary filespace directory path.  NOT Blank padded.
				 * Just a NULL terminated string.
				 */

	int16		mirrorDbId,

	char 		*mirrorFilespaceLocation,

	MirroredObjectExistenceState mirrorExistenceState,

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			*persistentSerialNum,


	bool			flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	FilespaceDirEntry filespaceDirEntry;
	TransactionId topXid;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent filespace %u because we are before persistence work",
				 filespaceOid);

		return;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentFilespace_VerifyInitScan();

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName,filespaceOid);

	topXid = GetTopTransactionId();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	filespaceDirEntry =
				PersistentFilespace_CreateDirUnderLock(
												filespaceOid);
	Assert(filespaceDirEntry != NULL);

	filespaceDirEntry->dbId1 = primaryDbId;
	PersistentFilespace_BlankPadCopyLocation(
										filespaceDirEntry->locationBlankPadded1,
										primaryFilespaceLocation);

	filespaceDirEntry->dbId2 = mirrorDbId;
	PersistentFilespace_BlankPadCopyLocation(
										filespaceDirEntry->locationBlankPadded2,
										mirrorFilespaceLocation);

	filespaceDirEntry->state = PersistentFileSysState_CreatePending;

	PersistentFilespace_AddTuple(
							filespaceDirEntry,
							/* createMirrorDataLossTrackingSessionNum */ 0,
							mirrorExistenceState,
							/* reserved */ 0,
							/* parentXid */ topXid,
							flushToXLog);

	*persistentTid = filespaceDirEntry->persistentTid;
	*persistentSerialNum = filespaceDirEntry->persistentSerialNum;

	/*
	 * This XLOG must be generated under the persistent write-lock.
	 */
#ifdef MASTER_MIRROR_SYNC
	mmxlog_log_create_filespace(filespaceOid);
#endif

	SIMPLE_FAULT_INJECTOR(FaultBeforePendingDeleteFilespaceEntry);

	/*
	 * MPP-18228
	 * To make adding 'Create Pending' entry to persistent table and adding
	 * to the PendingDelete list atomic
	 */
	PendingDelete_AddCreatePendingEntryWrapper(
								&fsObjName,
								persistentTid,
								*persistentSerialNum);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
		     "Persistent filespace directory: Add '%s' in state 'Created', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));
}

// -----------------------------------------------------------------------------
// Transaction End
// -----------------------------------------------------------------------------

/*
 * Indicate the transaction commited and the filespace is officially created.
 */
void PersistentFilespace_Created(
	Oid 		filespaceOid,
				/* The filespace OID for the create. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum,
				/* Serial number for the filespace.	Distinquishes the uses of the tuple. */

	bool			retryPossible)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	FilespaceDirEntry filespaceDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent filespace %u because we are before "
				 "persistence work",
				 filespaceOid);
		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
		return;
	}

	PersistentFilespace_VerifyInitScan();

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName,filespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	filespaceDirEntry =
				PersistentFilespace_FindDirUnderLock(
												filespaceOid);
	if (filespaceDirEntry == NULL)
		elog(ERROR, "did not find persistent filespace entry %u",
			 filespaceOid);

	if (filespaceDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "persistent filespace entry %u expected to be in "
			 "'Create Pending' state (actual state '%s')",
			 filespaceOid,
			 PersistentFileSysObjState_Name(filespaceDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
								&fsObjName,
								persistentTid,
								persistentSerialNum,
								PersistentFileSysState_Created,
								retryPossible,
								/* flushToXlog */ false,
								/* oldState */ NULL,
								/* verifiedActionCallback */ NULL);

	filespaceDirEntry->state = PersistentFileSysState_Created;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
		     "Persistent filespace directory: '%s' changed state from 'Create Pending' to 'Created', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

/*
 * Indicate we intend to drop a filespace file as part of the current transaction.
 *
 * This filespace file to drop will be listed inside a commit, distributed commit, a distributed
 * prepared, and distributed commit prepared XOG records.
 *
 * For any of the commit type records, once that XLOG record is flushed then the actual
 * file-system delete will occur.  The flush guarantees the action will be retried after system
 * crash.
 */
PersistentFileSysObjStateChangeResult PersistentFilespace_MarkDropPending(
	Oid 		filespaceOid,
				/* The filespace OID for the drop. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum,
				/* Serial number for the filespace.	Distinquishes the uses of the tuple. */

	bool			retryPossible)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	FilespaceDirEntry filespaceDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent filespace %u because we are before persistence work",
				 filespaceOid);

		return false;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentFilespace_VerifyInitScan();

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName,filespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	filespaceDirEntry =
				PersistentFilespace_FindDirUnderLock(
												filespaceOid);
	if (filespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent filespace entry %u",
			 filespaceOid);

	if (filespaceDirEntry->state != PersistentFileSysState_CreatePending &&
		filespaceDirEntry->state != PersistentFileSysState_Created)
		elog(ERROR, "Persistent filespace entry %u expected to be in 'Create Pending' or 'Created' state (actual state '%s')",
			 filespaceOid,
			 PersistentFileSysObjState_Name(filespaceDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
								&fsObjName,
								persistentTid,
								persistentSerialNum,
								PersistentFileSysState_DropPending,
								retryPossible,
								/* flushToXlog */ false,
								/* oldState */ NULL,
								/* verifiedActionCallback */ NULL);

	filespaceDirEntry->state = PersistentFileSysState_DropPending;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
		     "Persistent filespace directory: '%s' changed state from 'Create Pending' to 'Drop Pending', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

/*
 * Indicate we are aborting the create of a filespace file.
 *
 * This state will make sure the filespace gets dropped after a system crash.
 */
PersistentFileSysObjStateChangeResult PersistentFilespace_MarkAbortingCreate(
	Oid 		filespaceOid,
				/* The filespace OID for the aborting create. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum,
				/* Serial number for the filespace.	Distinquishes the uses of the tuple. */

	bool			retryPossible)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	FilespaceDirEntry filespaceDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent filespace %u because we are before persistence work",
				 filespaceOid);

		return false;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentFilespace_VerifyInitScan();

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName,filespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	filespaceDirEntry =
				PersistentFilespace_FindDirUnderLock(
												filespaceOid);
	if (filespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent filespace entry %u",
			 filespaceOid);

	if (filespaceDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "Persistent filespace entry %u expected to be in 'Create Pending' (actual state '%s')",
			 filespaceOid,
			 PersistentFileSysObjState_Name(filespaceDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
								&fsObjName,
								persistentTid,
								persistentSerialNum,
								PersistentFileSysState_AbortingCreate,
								retryPossible,
								/* flushToXlog */ false,
								/* oldState */ NULL,
								/* verifiedActionCallback */ NULL);

	filespaceDirEntry->state = PersistentFileSysState_AbortingCreate;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
		     "Persistent filespace directory: '%s' changed state from 'Create Pending' to 'Aborting Create', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

static void
PersistentFilespace_DroppedVerifiedActionCallback(
	PersistentFileSysObjName 	*fsObjName,

	ItemPointer 				persistentTid,
			/* TID of the gp_persistent_rel_files tuple for the relation. */

	int64						persistentSerialNum,
			/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	PersistentFileSysObjVerifyExpectedResult verifyExpectedResult)
{
	Oid filespaceOid = PersistentFileSysObjName_GetFilespaceDir(fsObjName);

	switch (verifyExpectedResult)
	{
	case PersistentFileSysObjVerifyExpectedResult_DeleteUnnecessary:
	case PersistentFileSysObjVerifyExpectedResult_StateChangeAlreadyDone:
	case PersistentFileSysObjVerifyExpectedResult_ErrorSuppressed:
		break;

	case PersistentFileSysObjVerifyExpectedResult_StateChangeNeeded:
		/*
		 * This XLOG must be generated under the persistent write-lock.
		 */
#ifdef MASTER_MIRROR_SYNC
		mmxlog_log_remove_filespace(filespaceOid);
#endif

		break;

	default:
		elog(ERROR, "Unexpected persistent object verify expected result: %d",
			 verifyExpectedResult);
	}
}

/*
 * Indicate we physically removed the filespace file.
 */
void PersistentFilespace_Dropped(
	Oid 		filespaceOid,
				/* The filespace OID for the dropped filespace. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum)
				/* Serial number for the filespace.	Distinquishes the uses of the tuple. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	FilespaceDirEntry filespaceDirEntry;

	PersistentFileSysState oldState;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent filespace %u because we are before persistence work",
				 filespaceOid);

		return;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentFilespace_VerifyInitScan();

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName,filespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	filespaceDirEntry =
				PersistentFilespace_FindDirUnderLock(
												filespaceOid);
	if (filespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent filespace entry %u",
			 filespaceOid);

	if (filespaceDirEntry->state != PersistentFileSysState_DropPending &&
		filespaceDirEntry->state != PersistentFileSysState_AbortingCreate)
		elog(ERROR, "Persistent filespace entry %u expected to be in 'Drop Pending' or 'Aborting Create' (actual state '%s')",
			 filespaceOid,
			 PersistentFileSysObjState_Name(filespaceDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
								&fsObjName,
								persistentTid,
								persistentSerialNum,
								PersistentFileSysState_Free,
								/* retryPossible */ false,
								/* flushToXlog */ false,
								&oldState,
								PersistentFilespace_DroppedVerifiedActionCallback);

	filespaceDirEntry->state = PersistentFileSysState_Free;

	PersistentFilespace_RemoveDirUnderLock(filespaceDirEntry);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
		     "Persistent filespace directory: '%s' changed state from '%s' to (Free), serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 PersistentFileSysObjState_Name(oldState),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

/*
 * Add a mirror.
 */
void
PersistentFilespace_AddMirror(Oid filespace,
							  char *mirpath,
							  int16 pridbid, int16 mirdbid,
							  bool set_mirror_existence)
{
	PersistentFileSysObjName fsObjName;
	char *newpath;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	FilespaceDirEntry fde;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	PersistentFilespace_VerifyInitScan();

	PersistentFileSysObjName_SetFilespaceDir(&fsObjName, filespace);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	fde = PersistentFilespace_FindDirUnderLock(filespace);
	if (fde == NULL)
		elog(ERROR, "did not find persistent filespace entry %u",
			 filespace);

	if (fde->dbId1 == pridbid)
	{
		fde->dbId2 = mirdbid;
		PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded2,
										mirpath);
		newpath = fde->locationBlankPadded2;
	}
	else if (fde->dbId2 == pridbid)
	{
		fde->dbId1 = mirdbid;
		PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded1,
										mirpath);
		newpath = fde->locationBlankPadded1;
	}
	else
	{
		Insist(false);
	}

	PersistentFileSysObj_AddMirror(&fsObjName,
								   &fde->persistentTid,
								   fde->persistentSerialNum,
								   pridbid,
								   mirdbid,
								   (void *)newpath,
								   set_mirror_existence,
								   /* flushToXlog */ false);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

/*
 * Remove all reference to a segment from the gp_persistent_filespace_node table
 * and share memory structure.
 */
void
PersistentFilespace_RemoveSegment(int16 dbid, bool ismirror)
{
	HASH_SEQ_STATUS hstat;
	FilespaceDirEntry fde;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	hash_seq_init(&hstat, persistentFilespaceSharedHashTable);

	PersistentFilespace_VerifyInitScan();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((fde = hash_seq_search(&hstat)) != NULL)
	{
		Oid filespace = fde->key.filespaceOid;
		PersistentFileSysObjName fsObjName;

		PersistentFileSysObjName_SetFilespaceDir(&fsObjName, filespace);

		if (fde->dbId1 == dbid)
		{
			PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded1,
										"");
		}
		else if (fde->dbId2 == dbid)
		{
			PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded2,
										"");
		}

		PersistentFileSysObj_RemoveSegment(&fsObjName,
								   &fde->persistentTid,
								   fde->persistentSerialNum,
								   dbid,
								   ismirror,
								   /* flushToXlog */ false);
	}
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

/*
 * Activate a standby master by removing reference to the dead master
 * and changing our dbid to the old master's dbid
 */
void
PersistentFilespace_ActivateStandby(int16 oldmaster, int16 newmaster)
{
	HASH_SEQ_STATUS hstat;
	FilespaceDirEntry fde;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	hash_seq_init(&hstat, persistentFilespaceSharedHashTable);

	PersistentFilespace_VerifyInitScan();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((fde = hash_seq_search(&hstat)) != NULL)
	{
		Oid filespace = fde->key.filespaceOid;
		PersistentFileSysObjName fsObjName;

		PersistentFileSysObjName_SetFilespaceDir(&fsObjName, filespace);

		if (fde->dbId1 == oldmaster)
		{
			fde->dbId1 = InvalidDbid;
			fde->dbId2 = newmaster;

			/* Copy standby filespace location into new master location */
			PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded2,
										fde->locationBlankPadded1);

			PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded1,
										"");
		}
		else if (fde->dbId2 == oldmaster)
		{
			fde->dbId2 = InvalidDbid;
			fde->dbId1 = newmaster;

			/* Copy standby filespace location into new master location */
			PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded1,
										fde->locationBlankPadded2);

			PersistentFilespace_BlankPadCopyLocation(
										fde->locationBlankPadded2,
										"");
		}

		PersistentFileSysObj_ActivateStandby(&fsObjName,
								   &fde->persistentTid,
								   fde->persistentSerialNum,
								   oldmaster,
								   newmaster,
								   /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

/*
 * PersistentFilespace_LookupMirrorDbid()
 *
 * Check the gp_persistent_filespace table to identify what dbid it contains
 * that does not match the primary dbid.  If there are no filespaces currently
 * defined this check will return 0 even if there is an active mirror, because
 * the segment doesn't know any better.
 */
int16
PersistentFilespace_LookupMirrorDbid(int16 primaryDbid)
{
	HASH_SEQ_STATUS		status;
	FilespaceDirEntry   dirEntry;
	int16				mirrorDbid = 0;

	PersistentFilespace_VerifyInitScan();

	/* Start scan */
	hash_seq_init(&status, persistentFilespaceSharedHashTable);
	dirEntry = (FilespaceDirEntry) hash_seq_search(&status);
	if (dirEntry != NULL)
	{
		if (dirEntry->dbId1 == primaryDbid)
		{
			mirrorDbid = dirEntry->dbId2;
		}
		else if (dirEntry->dbId2 == primaryDbid)
		{
			mirrorDbid = dirEntry->dbId1;
		}
		else
		{
			elog(FATAL,
				 "dbid %d not found in gp_persistent_filespace_node",
				 (int) primaryDbid);
		}

		/* Terminate the scan early */
		hash_seq_term(&status);
	}

	return mirrorDbid;
}

// -----------------------------------------------------------------------------
// Shmem
// -----------------------------------------------------------------------------

static Size PersistentFilespace_SharedDataSize(void)
{
	return MAXALIGN(sizeof(PersistentFilespaceSharedData));
}

/*
 * Return the required shared-memory size for this module.
 */
Size PersistentFilespace_ShmemSize(void)
{
	Size		size;

	/* The hash table of persistent filespaces */
	size = hash_estimate_size((Size)gp_max_filespaces,
							  sizeof(FilespaceDirEntryData));

	/* The shared-memory structure. */
	size = add_size(size, PersistentFilespace_SharedDataSize());

	return size;
}

/*
 * PersistentFilespace_HashTableInit
 *
 * Create or find shared-memory hash table.
 */
static bool
PersistentFilespace_HashTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(FilespaceDirEntryKey);
	info.entrysize = sizeof(FilespaceDirEntryData);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	persistentFilespaceSharedHashTable =
						ShmemInitHash("Persistent Filespace Hash",
								   gp_max_filespaces,
								   gp_max_filespaces,
								   &info,
								   hash_flags);

	if (persistentFilespaceSharedHashTable == NULL)
		return false;

	return true;
}

/*
 * Initialize the shared-memory for this module.
 */
void PersistentFilespace_ShmemInit(void)
{
	bool found;
	bool ok;

	/* Create the shared-memory structure. */
	persistentFilespaceSharedData =
		(PersistentFilespaceSharedData *)
						ShmemInitStruct("Persistent Filespace Data",
										PersistentFilespace_SharedDataSize(),
										&found);

	if (!found)
	{
		PersistentFileSysObj_InitShared(
						&persistentFilespaceSharedData->fileSysObjSharedData);
	}

	/* Create or find our shared-memory hash table. */
	ok = PersistentFilespace_HashTableInit();
	if (!ok)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("Not enough shared memory for persistent filespace hash table")));

	PersistentFileSysObj_Init(
						&persistentFilespaceData.fileSysObjData,
						&persistentFilespaceSharedData->fileSysObjSharedData,
						PersistentFsObjType_FilespaceDir,
						PersistentFilespace_ScanTupleCallback);


	Assert(persistentFilespaceSharedData != NULL);
	Assert(persistentFilespaceSharedHashTable != NULL);
}

/*
 * Note that this can go away when we do away with master mirror sync by WAL.
 */
#ifdef MASTER_MIRROR_SYNC /* annotation to show that this is just for mmsync */
void
get_filespace_data(fspc_agg_state **fas, char *caller)
{
	HASH_SEQ_STATUS stat;
	FilespaceDirEntry fde;

	int maxCount;

	Assert(*fas == NULL);

	mmxlog_add_filespace_init(fas, &maxCount);

	hash_seq_init(&stat, persistentFilespaceSharedHashTable);

	while ((fde = hash_seq_search(&stat)) != NULL)
		mmxlog_add_filespace(
				 fas, &maxCount,
				 fde->key.filespaceOid,
				 fde->dbId1, fde->locationBlankPadded1,
                 fde->dbId2, fde->locationBlankPadded2,
                 caller);

}
#endif
