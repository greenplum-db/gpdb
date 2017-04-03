/*-------------------------------------------------------------------------
 *
 * cdbpersistenttablespace.c
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

#include "access/persistentfilesysobjname.h"
#include "access/xlogmm.h"
#include "catalog/catalog.h"
#include "catalog/gp_persistent.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/pg_filespace.h"
#include "cdb/cdbsharedoidsearch.h"
#include "cdb/cdbdirectopen.h"
#include "cdb/cdbmirroredfilesysobj.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentfilespace.h"
#include "cdb/cdbpersistenttablespace.h"
#include "postmaster/postmaster.h"
#include "storage/itemptr.h"
#include "utils/hsearch.h"
#include "storage/shmem.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "utils/guc.h"
#include "storage/smgr.h"
#include "storage/ipc.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"

typedef struct PersistentTablespaceSharedData
{
	
	PersistentFileSysObjSharedData		fileSysObjSharedData;

} PersistentTablespaceSharedData;

#define PersistentTablespaceData_StaticInit {PersistentFileSysObjData_StaticInit}

typedef struct PersistentTablespaceData
{

	PersistentFileSysObjData		fileSysObjData;

} PersistentTablespaceData;

typedef struct TablespaceDirEntryKey
{
	Oid	tablespaceOid;
} TablespaceDirEntryKey;

typedef struct TablespaceDirEntryData
{
	TablespaceDirEntryKey	key;

	Oid						filespaceOid;

	PersistentFileSysState	state;
	int64					persistentSerialNum;
	ItemPointerData 		persistentTid;
	
} TablespaceDirEntryData;
typedef TablespaceDirEntryData *TablespaceDirEntry;


/*
 * Global Variables
 */
PersistentTablespaceSharedData	*persistentTablespaceSharedData = NULL;
static HTAB *persistentTablespaceSharedHashTable = NULL;

PersistentTablespaceData	persistentTablespaceData = PersistentTablespaceData_StaticInit;

static void PersistentTablespace_VerifyInitScan(void)
{
	if (persistentTablespaceSharedData == NULL)
		elog(PANIC, "Persistent tablespace information shared-memory not setup");

	PersistentFileSysObj_VerifyInitScan();
}

/*
 * Return the hash entry for a tablespace.
 */
static TablespaceDirEntry
PersistentTablespace_FindEntryUnderLock(
	Oid			tablespaceOid)
{
	bool			found;

	TablespaceDirEntry	tablespaceDirEntry;

	TablespaceDirEntryKey key;

	if (persistentTablespaceSharedHashTable == NULL)
		elog(PANIC, "Persistent tablespace information shared-memory not setup");

	key.tablespaceOid = tablespaceOid;

	tablespaceDirEntry = 
			(TablespaceDirEntry) 
					hash_search(persistentTablespaceSharedHashTable,
								(void *) &key,
								HASH_FIND,
								&found);
	if (!found)
		return NULL;

	return tablespaceDirEntry;
}

static TablespaceDirEntry
PersistentTablespace_CreateEntryUnderLock(
	Oid			filespaceOid,
	
	Oid 		tablespaceOid)
{
	bool			found;

	TablespaceDirEntry	tablespaceDirEntry;

	TablespaceDirEntryKey key;

	if (persistentTablespaceSharedHashTable == NULL)
		elog(PANIC, "Persistent tablespace information shared-memory not setup");

	key.tablespaceOid = tablespaceOid;

	tablespaceDirEntry = 
			(TablespaceDirEntry) 
					hash_search(persistentTablespaceSharedHashTable,
								(void *) &key,
								HASH_ENTER_NULL,
								&found);

	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Out of shared-memory for persistent tablespaces");

	tablespaceDirEntry->filespaceOid = filespaceOid;
	
	tablespaceDirEntry->state = 0;
	tablespaceDirEntry->persistentSerialNum = 0;
	MemSet(&tablespaceDirEntry->persistentTid, 0, sizeof(ItemPointerData));
	
	return tablespaceDirEntry;
}

static void
PersistentTablespace_RemoveEntryUnderLock(
	TablespaceDirEntry	tablespaceDirEntry)
{
	TablespaceDirEntry	removeTablespaceDirEntry;

	if (persistentTablespaceSharedHashTable == NULL)
		elog(PANIC, "Persistent tablespace information shared-memory not setup");

	removeTablespaceDirEntry = 
				(TablespaceDirEntry) 
						hash_search(persistentTablespaceSharedHashTable,
									(void *) &tablespaceDirEntry->key,
									HASH_REMOVE,
									NULL);

	if (removeTablespaceDirEntry == NULL)
		elog(ERROR, "Trying to delete entry that does not exist");
}

PersistentFileSysState
PersistentTablespace_GetState(
	Oid		tablespaceOid)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentFileSysState	state;

	/*
	 * pg_default and pg_global always exist, but do not have entries in
	 * gp_persistent_tablespace_node.
	 */
	if (tablespaceOid == DEFAULTTABLESPACE_OID ||
		tablespaceOid == GLOBALTABLESPACE_OID) 
	{ 
		return PersistentFileSysState_Created; 
	}

	PersistentTablespace_VerifyInitScan();

	// NOTE: Since we are not accessing data in the Buffer Pool, we don't need to
	// acquire the MirroredLock.

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_FindEntryUnderLock(
												tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	state = tablespaceDirEntry->state;

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	return state;
}

// -----------------------------------------------------------------------------
// Scan 
// -----------------------------------------------------------------------------

static bool PersistentTablespace_ScanTupleCallback(
	ItemPointer 			persistentTid,
	int64					persistentSerialNum,
	Datum					*values)
{
	Oid		filespaceOid;
	Oid		tablespaceOid;
	
	PersistentFileSysState	state;

	int64	createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState		mirrorExistenceState;

	int32					reserved;
	TransactionId			parentXid;
	int64					serialNum;
	
	TablespaceDirEntry tablespaceDirEntry;

	GpPersistentTablespaceNode_GetValues(
									values,
									&filespaceOid,
									&tablespaceOid,
									&state,
									&createMirrorDataLossTrackingSessionNum,
									&mirrorExistenceState,
									&reserved,
									&parentXid,
									&serialNum);

	tablespaceDirEntry = 
		PersistentTablespace_CreateEntryUnderLock(filespaceOid, tablespaceOid);
	
	tablespaceDirEntry->state = state;
	tablespaceDirEntry->persistentSerialNum = serialNum;
	tablespaceDirEntry->persistentTid = *persistentTid;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
			 "PersistentTablespace_ScanTupleCallback: tablespace %u, filespace %u, state %s, TID %s, serial number " INT64_FORMAT,
			 tablespaceOid,
			 filespaceOid,
			 PersistentFileSysObjState_Name(state),
			 ItemPointerToString2(persistentTid),
			 persistentSerialNum);

	return true;	// Continue.
}

//------------------------------------------------------------------------------

void PersistentTablespace_Reset(void)
{
	HASH_SEQ_STATUS stat;

	TablespaceDirEntry tablespaceDirEntry;

	hash_seq_init(&stat, persistentTablespaceSharedHashTable);

	while (true)
	{
		TablespaceDirEntry removeTablespaceDirEntry;

		PersistentFileSysObjName fsObjName;
		
		tablespaceDirEntry = hash_seq_search(&stat);
		if (tablespaceDirEntry == NULL)
			break;

		PersistentFileSysObjName_SetTablespaceDir(
										&fsObjName,
										tablespaceDirEntry->key.tablespaceOid);

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Persistent tablespace directory: Resetting '%s' serial number " INT64_FORMAT " at TID %s",
				 PersistentFileSysObjName_ObjectName(&fsObjName),
				 tablespaceDirEntry->persistentSerialNum,
				 ItemPointerToString(&tablespaceDirEntry->persistentTid));

		removeTablespaceDirEntry = 
					(TablespaceDirEntry) 
							hash_search(persistentTablespaceSharedHashTable,
										(void *) &tablespaceDirEntry->key,
										HASH_REMOVE,
										NULL);
		
		if (removeTablespaceDirEntry == NULL)
			elog(ERROR, "Trying to delete entry that does not exist");
	}
}

extern void PersistentTablespace_LookupTidAndSerialNum(
	Oid 		tablespaceOid,
				/* The tablespace OID for the lookup. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_tablespace_node tuple for the rel file */

	int64			*persistentSerialNum)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentTablespace_VerifyInitScan();

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_FindEntryUnderLock(
												tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	*persistentTid = tablespaceDirEntry->persistentTid;
	*persistentSerialNum = tablespaceDirEntry->persistentSerialNum;

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;
}

// -----------------------------------------------------------------------------
// Helpers 
// -----------------------------------------------------------------------------

static void PersistentTablespace_AddTuple(
	TablespaceDirEntry tablespaceDirEntry,

	int64			createMirrorDataLossTrackingSessionNum,

	MirroredObjectExistenceState mirrorExistenceState,

	int32			reserved,

	TransactionId 	parentXid,

	bool			flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */
{
	Oid filespaceOid = tablespaceDirEntry->filespaceOid;
	Oid tablespaceOid = tablespaceDirEntry->key.tablespaceOid;

	Datum values[Natts_gp_persistent_tablespace_node];

	GpPersistentTablespaceNode_SetDatumValues(
								values,
								filespaceOid,
								tablespaceOid,
								tablespaceDirEntry->state,
								createMirrorDataLossTrackingSessionNum,
								mirrorExistenceState,
								reserved,
								parentXid,
								/* persistentSerialNum */ 0);	// This will be set by PersistentFileSysObj_AddTuple.

	PersistentFileSysObj_AddTuple(
							PersistentFsObjType_TablespaceDir,
							values,
							flushToXLog,
							&tablespaceDirEntry->persistentTid,
							&tablespaceDirEntry->persistentSerialNum);
}

PersistentTablespaceGetFilespaces PersistentTablespace_TryGetPrimaryAndMirrorFilespaces(
	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */
	
	char **mirrorFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. 
				 * Or, returns NULL when mirror not configured. */
				 
	Oid *filespaceOid)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentTablespaceGetFilespaces result;

	*primaryFilespaceLocation = NULL;
	*mirrorFilespaceLocation = NULL;
	*filespaceOid = InvalidOid;

	if (IsBuiltinTablespace(tablespaceOid))
	{
		/*
		 * Optimize out the common cases.
		 */
		 return PersistentTablespaceGetFilespaces_Ok;
	}

#ifdef MASTER_MIRROR_SYNC
	/*
	 * Can't rely on persistent tables or memory structures on the standby so
	 * get it from the cache maintained by the master mirror sync code
	 */
	if (IsStandbyMode())
	{
		if (!mmxlog_tablespace_get_filespace(
									tablespaceOid,
									filespaceOid))
		{
			if (!Debug_persistent_recovery_print)
			{
				// Print this information when we are not doing other tracing.
				mmxlog_print_tablespaces(
									LOG,
									"Standby Get Filespace for Tablespace");
			}
			return PersistentTablespaceGetFilespaces_TablespaceNotFound;
		}

		if (!mmxlog_filespace_get_path(
									*filespaceOid,
									primaryFilespaceLocation))
		{
			if (!Debug_persistent_recovery_print)
			{
				// Print this information when we are not doing other tracing.
				mmxlog_print_filespaces(
									LOG,
									"Standby Get Filespace Location");
			}
			return PersistentTablespaceGetFilespaces_FilespaceNotFound;
		}

		return PersistentTablespaceGetFilespaces_Ok;
	}
#endif

	/*
	 * MPP-10111 - There is a point during gpexpand where we need to bring
	 * the database up to fix the filespace locations for a segment.  At
	 * this point in time the old filespace locations are wrong and we should
	 * not trust anything currently stored there.  If the guc is set we
	 * prevent the lookup of a any non builtin filespaces.
	 */
	if (gp_before_filespace_setup)
		elog(ERROR, "can not lookup tablespace location: gp_before_filespace_setup=true");

	/*
	 * Important to make this call AFTER we check if we are the Standby Master.
	 */
	PersistentTablespace_VerifyInitScan();

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry =
		PersistentTablespace_FindEntryUnderLock(
										tablespaceOid);
	if (tablespaceDirEntry == NULL)
	{
		result = PersistentTablespaceGetFilespaces_TablespaceNotFound;
	}
	else
	{
		*filespaceOid = tablespaceDirEntry->filespaceOid;

		if (!PersistentFilespace_TryGetPrimaryAndMirrorUnderLock(
													tablespaceDirEntry->filespaceOid,
													primaryFilespaceLocation,
													mirrorFilespaceLocation))
		{
			result = PersistentTablespaceGetFilespaces_FilespaceNotFound;			
		}
		else
		{
			result = PersistentTablespaceGetFilespaces_Ok;
		}
	}

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	return result;
}

void PersistentTablespace_GetPrimaryAndMirrorFilespaces(
	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */

	char **primaryFilespaceLocation,
				/* The primary filespace directory path.  Return NULL for global and base. */
	
	char **mirrorFilespaceLocation)
				/* The primary filespace directory path.  Return NULL for global and base. 
				 * Or, returns NULL when mirror not configured. */
{
	PersistentTablespaceGetFilespaces tablespaceGetFilespaces;

	Oid filespaceOid;

	/*
	 * Do not call PersistentTablepace_VerifyInitScan here to allow 
	 * PersistentTablespace_TryGetPrimaryAndMirrorFilespaces to handle the Standby Master
	 * special case.
	 */

	tablespaceGetFilespaces =
			PersistentTablespace_TryGetPrimaryAndMirrorFilespaces(
													tablespaceOid,
													primaryFilespaceLocation,
													mirrorFilespaceLocation,
													&filespaceOid);
	switch (tablespaceGetFilespaces)
	{
	case PersistentTablespaceGetFilespaces_TablespaceNotFound:
		ereport(ERROR, 
				(errcode(ERRCODE_CDB_INTERNAL_ERROR),
				 errmsg("Unable to find entry for tablespace OID = %u when getting filespace directory paths",
						tablespaceOid)));
		break;
			
	case PersistentTablespaceGetFilespaces_FilespaceNotFound:
		ereport(ERROR, 
				(errcode(ERRCODE_CDB_INTERNAL_ERROR),
				 errmsg("Unable to find entry for filespace OID = %u when forming filespace directory paths for tablespace OID = %u",
				 		filespaceOid,
						tablespaceOid)));
		break;
					
	case PersistentTablespaceGetFilespaces_Ok:
		// Go below and pass back the result.
		break;
		
	default:
		elog(ERROR, "Unexpected tablespace filespace fetch result: %d",
			 tablespaceGetFilespaces);
	}
}

Oid
PersistentTablespace_GetFileSpaceOid(Oid tablespaceOid)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	TablespaceDirEntry tablespaceDirEntry;
	Oid filespace = InvalidOid;

	if (tablespaceOid == GLOBALTABLESPACE_OID ||
		tablespaceOid == DEFAULTTABLESPACE_OID)
	{
		/*
		 * Optimize out the common cases.
		 */
		 return SYSTEMFILESPACE_OID;
	}

	PersistentTablespace_VerifyInitScan();

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry =
		PersistentTablespace_FindEntryUnderLock(
										tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	filespace = tablespaceDirEntry->filespaceOid;

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	return filespace;
}

// -----------------------------------------------------------------------------
// State Change 
// -----------------------------------------------------------------------------

/*
 * Indicate we intend to create a tablespace file as part of the current transaction.
 *
 * An XLOG IntentToCreate record is generated that will guard the subsequent file-system
 * create in case the transaction aborts.
 *
 * After 1 or more calls to this routine to mark intention about tablespace files that are going
 * to be created, call ~_DoPendingCreates to do the actual file-system creates.  (See its
 * note on XLOG flushing).
 */
void PersistentTablespace_MarkCreatePending(
	Oid 		filespaceOid,
				/* The filespace where the tablespace lives. */

	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */

	MirroredObjectExistenceState mirrorExistenceState,

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			*persistentSerialNum,


	bool			flushToXLog)
				/* When true, the XLOG record for this change will be flushed to disk. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	TablespaceDirEntry tablespaceDirEntry;
	TransactionId topXid;

	if (Persistent_BeforePersistenceWork())
	{	
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Skipping persistent tablespace %u because we are before persistence work",
				 tablespaceOid);

		return;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentTablespace_VerifyInitScan();

	PersistentFileSysObjName_SetTablespaceDir(&fsObjName,tablespaceOid);

	topXid = GetTopTransactionId();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_CreateEntryUnderLock(
												filespaceOid,
												tablespaceOid);
	Assert(tablespaceDirEntry != NULL);

	tablespaceDirEntry->state = PersistentFileSysState_CreatePending;

	PersistentTablespace_AddTuple(
							tablespaceDirEntry,
							/* createMirrorDataLossTrackingSessionNum */ 0,
							mirrorExistenceState,
							/* reserved */ 0,
							/* parentXid */ topXid,
							flushToXLog);

	*persistentTid = tablespaceDirEntry->persistentTid;
	*persistentSerialNum = tablespaceDirEntry->persistentSerialNum;
		
	/*
	 * This XLOG must be generated under the persistent write-lock.
	 */
#ifdef MASTER_MIRROR_SYNC
	mmxlog_log_create_tablespace(	
						filespaceOid,
						tablespaceOid);
#endif

	SIMPLE_FAULT_INJECTOR(FaultBeforePendingDeleteTablespaceEntry);

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
		     "Persistent tablespace directory: Add '%s' in state 'Created', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));
}

// -----------------------------------------------------------------------------
// Rebuild tablespace persistent table 'gp_persistent_tablespace_node'
// -----------------------------------------------------------------------------

void PersistentTablespace_AddCreated(
									  Oid 		filespaceOid,
									  /* The filespace where the tablespace lives. */
											
									  Oid 		tablespaceOid,
									  /* The tablespace OID to be added. */
											
									  MirroredObjectExistenceState mirrorExistenceState,
											
									  bool			flushToXLog)
									  /* When true, the XLOG record for this change will be flushed to disk. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;
	
	PersistentFileSysObjName fsObjName;
	
	ItemPointerData		persistentTid;
	int64				persistentSerialNum;	
	TablespaceDirEntry	tablespaceDirEntry;
	
	if (Persistent_BeforePersistenceWork())
	{	
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Skipping persistent tablespace %u because we are before persistence work",
				 tablespaceOid);
		
		return;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}
	
	PersistentTablespace_VerifyInitScan();
	
	PersistentFileSysObjName_SetTablespaceDir(&fsObjName,tablespaceOid);
	
	WRITE_PERSISTENT_STATE_ORDERED_LOCK;
	
	tablespaceDirEntry = 
	PersistentTablespace_CreateEntryUnderLock(
											  filespaceOid,
											  tablespaceOid);
	Assert(tablespaceDirEntry != NULL);
	
	tablespaceDirEntry->state = PersistentFileSysState_Created;
	
	PersistentTablespace_AddTuple(
								  tablespaceDirEntry,
								  /* createMirrorDataLossTrackingSessionNum */ 0,
								  mirrorExistenceState,
								  /* reserved */ 0,
								  InvalidTransactionId,
								  flushToXLog);

	persistentTid = tablespaceDirEntry->persistentTid;
	persistentSerialNum = tablespaceDirEntry->persistentSerialNum;
		
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
	
	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
		     "Persistent tablespace directory: Add '%s' in state 'Created', mirror existence state '%s', serial number " INT64_FORMAT " at TID '%s' ",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 persistentSerialNum,
			 ItemPointerToString(&persistentTid));
}

// -----------------------------------------------------------------------------
// Transaction End  
// -----------------------------------------------------------------------------

/*
 * Indicate the transaction commited and the tablespace is officially created.
 */
void PersistentTablespace_Created(
	Oid 		tablespaceOid,
				/* The tablespace OID for the create. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum,
				/* Serial number for the tablespace.	Distinquishes the uses of the tuple. */

	bool			retryPossible)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;
	
	if (Persistent_BeforePersistenceWork())
	{	
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Skipping persistent tablespace %u because we are before persistence work",
				 tablespaceOid);

		return;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentTablespace_VerifyInitScan();

	PersistentFileSysObjName_SetTablespaceDir(&fsObjName,tablespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_FindEntryUnderLock(
												tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	if (tablespaceDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "Persistent tablespace entry %u expected to be in 'Create Pending' state (actual state '%s')", 
			 tablespaceOid,
			 PersistentFileSysObjState_Name(tablespaceDirEntry->state));

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

	tablespaceDirEntry->state = PersistentFileSysState_Created;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
		     "Persistent tablespace directory: '%s' changed state from 'Create Pending' to 'Created', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

void
PersistentTablespace_RemoveSegment(int16 dbid, bool ismirror)
{
	TablespaceDirEntry tablespaceDirEntry;
	HASH_SEQ_STATUS hstat;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	hash_seq_init(&hstat, persistentTablespaceSharedHashTable);

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	PersistentTablespace_VerifyInitScan();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tablespaceDirEntry = hash_seq_search(&hstat)) != NULL)
	{
		PersistentFileSysObjName fsObjName;
		Oid tblspc = tablespaceDirEntry->key.tablespaceOid;

		tablespaceDirEntry = PersistentTablespace_FindEntryUnderLock(tblspc);

		if (tablespaceDirEntry == NULL)
			elog(ERROR, "Did not find persistent tablespace entry %u", 
				 tblspc);

        PersistentFileSysObjName_SetTablespaceDir(&fsObjName, tblspc);

	    PersistentFileSysObj_RemoveSegment(&fsObjName,
										   &tablespaceDirEntry->persistentTid,
										   tablespaceDirEntry->persistentSerialNum,
										   dbid,
										   ismirror,
										   /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

void
PersistentTablespace_ActivateStandby(int16 oldmaster, int16 newmaster)
{
	TablespaceDirEntry tablespaceDirEntry;
	HASH_SEQ_STATUS hstat;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	hash_seq_init(&hstat, persistentTablespaceSharedHashTable);

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	PersistentTablespace_VerifyInitScan();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tablespaceDirEntry = hash_seq_search(&hstat)) != NULL)
	{
		PersistentFileSysObjName fsObjName;
		Oid tblspc = tablespaceDirEntry->key.tablespaceOid;

		tablespaceDirEntry = PersistentTablespace_FindEntryUnderLock(tblspc);

		if (tablespaceDirEntry == NULL)
			elog(ERROR, "cannot find persistent tablespace entry %u", 
				 tblspc);

		PersistentFileSysObjName_SetTablespaceDir(&fsObjName, tblspc);
		PersistentFileSysObj_ActivateStandby(&fsObjName,
								   &tablespaceDirEntry->persistentTid,
								   tablespaceDirEntry->persistentSerialNum,
								   oldmaster,
								   newmaster,
								   /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

void
PersistentTablespace_AddMirrorAll(
	int16			pridbid,
	int16			mirdbid)
{
	TablespaceDirEntry tablespaceDirEntry;
	HASH_SEQ_STATUS hstat;
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	hash_seq_init(&hstat, persistentTablespaceSharedHashTable);

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	PersistentTablespace_VerifyInitScan();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tablespaceDirEntry = hash_seq_search(&hstat)) != NULL)
	{
		PersistentFileSysObjName fsObjName;
		Oid tblspc = tablespaceDirEntry->key.tablespaceOid;

		tablespaceDirEntry = PersistentTablespace_FindEntryUnderLock(tblspc);

		if (tablespaceDirEntry == NULL)
			elog(ERROR, "Did not find persistent tablespace entry %u", 
				 tblspc);

        PersistentFileSysObjName_SetTablespaceDir(&fsObjName, tblspc);

	    PersistentFileSysObj_AddMirror(&fsObjName,
    	                               &tablespaceDirEntry->persistentTid,
        	                           tablespaceDirEntry->persistentSerialNum,
									   pridbid,
            	                       mirdbid,
									   NULL,
									   true,
                    	               /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

/*
 * Indicate we intend to drop a tablespace file as part of the current transaction.
 *
 * This tablespace file to drop will be listed inside a commit, distributed commit, a distributed 
 * prepared, and distributed commit prepared XOG records.
 *
 * For any of the commit type records, once that XLOG record is flushed then the actual
 * file-system delete will occur.  The flush guarantees the action will be retried after system
 * crash.
 */
PersistentFileSysObjStateChangeResult PersistentTablespace_MarkDropPending(
	Oid 		tablespaceOid,
				/* The tablespace OID for the drop. */

	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum,
				/* Serial number for the tablespace.	Distinquishes the uses of the tuple. */

	bool			retryPossible)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;
	
	if (Persistent_BeforePersistenceWork())
	{	
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Skipping persistent tablespace %u because we are before persistence work",
				 tablespaceOid);

		return false;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentTablespace_VerifyInitScan();

	PersistentFileSysObjName_SetTablespaceDir(&fsObjName,tablespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_FindEntryUnderLock(
												tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	if (tablespaceDirEntry->state != PersistentFileSysState_CreatePending &&
		tablespaceDirEntry->state != PersistentFileSysState_Created)
		elog(ERROR, "Persistent tablespace entry %u expected to be in 'Create Pending' or 'Created' state (actual state '%s')", 
			 tablespaceOid,
			 PersistentFileSysObjState_Name(tablespaceDirEntry->state));

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

	tablespaceDirEntry->state = PersistentFileSysState_DropPending;
	
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
		     "Persistent tablespace directory: '%s' changed state from 'Create Pending' to 'Aborting Create', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

/*
 * Indicate we are aborting the create of a tablespace file.
 *
 * This state will make sure the tablespace gets dropped after a system crash.
 */
PersistentFileSysObjStateChangeResult PersistentTablespace_MarkAbortingCreate(
	Oid 		tablespaceOid,
				/* The tablespace OID for the aborting create. */
							
	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum,
				/* Serial number for the tablespace.	Distinquishes the uses of the tuple. */

	bool			retryPossible)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;
	
	if (Persistent_BeforePersistenceWork())
	{	
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Skipping persistent tablespace %u because we are before persistence work",
				 tablespaceOid);

		return false;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentTablespace_VerifyInitScan();

	PersistentFileSysObjName_SetTablespaceDir(&fsObjName,tablespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_FindEntryUnderLock(
												tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	if (tablespaceDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "Persistent tablespace entry %u expected to be in 'Create Pending' (actual state '%s')", 
			 tablespaceOid,
			 PersistentFileSysObjState_Name(tablespaceDirEntry->state));

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

	tablespaceDirEntry->state = PersistentFileSysState_AbortingCreate;
		
	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
		     "Persistent tablespace directory: '%s' changed state from 'Create Pending' to 'Aborting Create', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

static void
PersistentTablespace_DroppedVerifiedActionCallback(
	PersistentFileSysObjName 	*fsObjName,

	ItemPointer 				persistentTid,
			/* TID of the gp_persistent_rel_files tuple for the relation. */

	int64						persistentSerialNum,
			/* Serial number for the relation.	Distinquishes the uses of the tuple. */

	PersistentFileSysObjVerifyExpectedResult verifyExpectedResult)
{
	Oid tablespaceOid = PersistentFileSysObjName_GetTablespaceDir(fsObjName);

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
		mmxlog_log_remove_tablespace(tablespaceOid);
#endif
				
		break;
	
	default:
		elog(ERROR, "Unexpected persistent object verify expected result: %d",
			 verifyExpectedResult);
	}
}

/*
 * Indicate we physically removed the tablespace file.
 */
void PersistentTablespace_Dropped(
	Oid 		tablespaceOid,
				/* The tablespace OID for the dropped tablespace. */
										
	ItemPointer		persistentTid,
				/* TID of the gp_persistent_rel_files tuple for the rel file */

	int64			persistentSerialNum)
				/* Serial number for the tablespace.	Distinquishes the uses of the tuple. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	TablespaceDirEntry tablespaceDirEntry;

	PersistentFileSysState oldState;

	PersistentFileSysObjStateChangeResult stateChangeResult;
	
	if (Persistent_BeforePersistenceWork())
	{	
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(), 
				 "Skipping persistent tablespace %u because we are before persistence work",
				 tablespaceOid);

		return;	// The initdb process will load the persistent table once we out of bootstrap mode.
	}

	PersistentTablespace_VerifyInitScan();

	PersistentFileSysObjName_SetTablespaceDir(&fsObjName,tablespaceOid);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	tablespaceDirEntry = 
				PersistentTablespace_FindEntryUnderLock(
												tablespaceOid);
	if (tablespaceDirEntry == NULL)
		elog(ERROR, "Did not find persistent tablespace entry %u", 
			 tablespaceOid);

	if (tablespaceDirEntry->state != PersistentFileSysState_DropPending &&
		tablespaceDirEntry->state != PersistentFileSysState_AbortingCreate)
		elog(ERROR, "Persistent tablespace entry %u expected to be in 'Drop Pending' or 'Aborting Create' (actual state '%s')", 
			 tablespaceOid,
			 PersistentFileSysObjState_Name(tablespaceDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
								&fsObjName,
								persistentTid,
								persistentSerialNum,
								PersistentFileSysState_Free,
								/* retryPossible */ false,
								/* flushToXlog */ false,
								&oldState,
								PersistentTablespace_DroppedVerifiedActionCallback);

	tablespaceDirEntry->state = PersistentFileSysState_Free;

	PersistentTablespace_RemoveEntryUnderLock(tablespaceDirEntry);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(), 
		     "Persistent tablespace directory: '%s' changed state from '%s' to (Free), serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 PersistentFileSysObjState_Name(oldState),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

// -----------------------------------------------------------------------------
// Shmem
// -----------------------------------------------------------------------------

static Size PersistentTablespace_SharedDataSize(void)
{
	return MAXALIGN(sizeof(PersistentTablespaceSharedData));
}


/*
 * Return the required shared-memory size for this module.
 */
Size PersistentTablespace_ShmemSize(void)
{
	Size		size;

	/* The hash table of persistent tablespaces */
	size = hash_estimate_size((Size)gp_max_tablespaces,
							  sizeof(TablespaceDirEntryData));

	/* The shared-memory structure. */
	size = add_size(size, PersistentTablespace_SharedDataSize());

	return size;
}

/*
 * PersistentTablespace_HashTableInit
 *
 * Create or find shared-memory hash table.
 */
static bool
PersistentTablespace_HashTableInit(void)
{
	HASHCTL			info;
	int				hash_flags;

	/* Set key and entry sizes. */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(TablespaceDirEntryKey);
	info.entrysize = sizeof(TablespaceDirEntryData);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	persistentTablespaceSharedHashTable = 
						ShmemInitHash("Persistent Tablespace Hash",
								   gp_max_tablespaces,
								   gp_max_tablespaces,
								   &info,
								   hash_flags);

	if (persistentTablespaceSharedHashTable == NULL)
		return false;

	return true;
}
						
/*
 * Initialize the shared-memory for this module.
 */
void PersistentTablespace_ShmemInit(void)
{
	bool found;
	bool ok;

	/* Create the shared-memory structure. */
	persistentTablespaceSharedData = 
		(PersistentTablespaceSharedData *)
						ShmemInitStruct("Persistent Tablespace Data",
										PersistentTablespace_SharedDataSize(),
										&found);

	if (!found)
	{
		PersistentFileSysObj_InitShared(
						&persistentTablespaceSharedData->fileSysObjSharedData);
	}

	/* Create or find our shared-memory hash table. */
	ok = PersistentTablespace_HashTableInit();
	if (!ok)
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("Not enough shared memory for persistent tablespace hash table")));

	PersistentFileSysObj_Init(
						&persistentTablespaceData.fileSysObjData,
						&persistentTablespaceSharedData->fileSysObjSharedData,
						PersistentFsObjType_TablespaceDir,
						PersistentTablespace_ScanTupleCallback);


	Assert(persistentTablespaceSharedData != NULL);
	Assert(persistentTablespaceSharedHashTable != NULL);
}

/*
 * Pass shared data back to the caller. See add_tablespace_data() for why we do
 * it like this.
 */
#ifdef MASTER_MIRROR_SYNC /* annotation to show that this is just for mmsync */
void
get_tablespace_data(tspc_agg_state **tas, char *caller)
{
	HASH_SEQ_STATUS stat;
	TablespaceDirEntry tde;

	int maxCount;

	Assert(*tas == NULL);

	mmxlog_add_tablespace_init(tas, &maxCount);

	hash_seq_init(&stat, persistentTablespaceSharedHashTable);

	while ((tde = hash_seq_search(&stat)) != NULL)
		mmxlog_add_tablespace(
				 tas, &maxCount, 
				 tde->filespaceOid, tde->key.tablespaceOid,
				 caller);

}
#endif
