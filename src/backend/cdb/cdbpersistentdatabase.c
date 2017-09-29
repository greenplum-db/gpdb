/*-------------------------------------------------------------------------
 *
 * cdbpersistentdatabase.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpersistentdatabase.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/relfilenode.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/persistentfilesysobjname.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_database.h"
#include "catalog/gp_persistent.h"
#include "cdb/cdbsharedoidsearch.h"
#include "cdb/cdbdirectopen.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistentrelation.h"
#include "storage/ipc.h"
#include "storage/itemptr.h"
#include "storage/shmem.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/hsearch.h"


/*
 * This module is for generic relation file create and drop.
 *
 * For create, it makes the file-system create of an empty file fully transactional so
 * the relation file will be deleted even on system crash.  The relation file could be a heap,
 * index, or append-only (row- or column-store).
 */

/* This value is the gp_max_tablespaces * gp_max_databases. */
int			MaxPersistentDatabaseDirectories = 0;

typedef struct PersistentDatabaseSharedData
{

	PersistentFileSysObjSharedData fileSysObjSharedData;

	SharedOidSearchTable databaseDirSearchTable;
	/* Variable length -- MUST BE LAST */

} PersistentDatabaseSharedData;

#define PersistentDatabaseData_StaticInit {PersistentFileSysObjData_StaticInit}

typedef struct PersistentDatabaseData
{

	PersistentFileSysObjData fileSysObjData;

} PersistentDatabaseData;

/*
 * Global Variables
 */
PersistentDatabaseSharedData *persistentDatabaseSharedData = NULL;

PersistentDatabaseData persistentDatabaseData = PersistentDatabaseData_StaticInit;

static void
PersistentDatabase_VerifyInitScan(void)
{
	if (persistentDatabaseSharedData == NULL)
		elog(PANIC, "Persistent database information shared-memory not setup");

	PersistentFileSysObj_VerifyInitScan();
}

/* ----------------------------------------------------------------------------- */
/*  Scan  */
/* ----------------------------------------------------------------------------- */

static bool
PersistentDatabase_ScanTupleCallback(
									 ItemPointer persistentTid,
									 int64 persistentSerialNum,
									 Datum *values)
{
	DbDirNode	dbDirNode;

	PersistentFileSysState state;

	int64		createMirrorDataLossTrackingSessionNum;

	MirroredObjectExistenceState mirrorExistenceState;

	int32		reserved;
	TransactionId parentXid;
	int64		serialNum;

	SharedOidSearchAddResult addResult;
	DatabaseDirEntry databaseDirEntry;

	GpPersistentDatabaseNode_GetValues(
									   values,
									   &dbDirNode.tablespace,
									   &dbDirNode.database,
									   &state,
									   &createMirrorDataLossTrackingSessionNum,
									   &mirrorExistenceState,
									   &reserved,
									   &parentXid,
									   &serialNum);

	addResult =
		SharedOidSearch_Add(
							&persistentDatabaseSharedData->databaseDirSearchTable,
							dbDirNode.database,
							dbDirNode.tablespace,
							(SharedOidSearchObjHeader **) &databaseDirEntry);
	if (addResult == SharedOidSearchAddResult_NoMemory)
		elog(ERROR, "Out of shared-memory for persistent relations");
	else if (addResult == SharedOidSearchAddResult_Exists)
		elog(PANIC, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(dbDirNode.database, dbDirNode.tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));
	else
		Assert(addResult == SharedOidSearchAddResult_Ok);

	databaseDirEntry->state = state;
	databaseDirEntry->persistentSerialNum = serialNum;
	databaseDirEntry->persistentTid = *persistentTid;

	databaseDirEntry->iteratorRefCount = 0;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "PersistentDatabase_ScanTupleCallback: database %u, tablespace %u, state %s, TID %s, serial number " INT64_FORMAT,
			 dbDirNode.database,
			 dbDirNode.tablespace,
			 PersistentFileSysObjState_Name(state),
			 ItemPointerToString2(persistentTid),
			 persistentSerialNum);

	return true;
	/* Continue. */
}

/* ----------------------------------------------------------------------------- */
/*  Iterate	 */
/* ----------------------------------------------------------------------------- */

static bool iterateOnShmemExitArmed = false;

static DatabaseDirEntry dirIterateDatabaseDirEntry = NULL;

static void
PersistentDatabase_DirIterateMoveAway(
									  DatabaseDirEntry prevDatabaseDirEntry)
{
	if (prevDatabaseDirEntry == NULL)
		return;

	Assert(prevDatabaseDirEntry->iteratorRefCount > 0);
	prevDatabaseDirEntry->iteratorRefCount--;
	if (prevDatabaseDirEntry->iteratorRefCount == 0 &&
		prevDatabaseDirEntry->state == PersistentFileSysState_Free)
	{
		/*
		 * Our job is to actually free the entry.
		 */
		SharedOidSearch_Delete(
							   &persistentDatabaseSharedData->databaseDirSearchTable,
							   &prevDatabaseDirEntry->header);
	}
}

static void
PersistentDatabase_ReleaseDirIterator(void)
{

	if (dirIterateDatabaseDirEntry != NULL)
	{
		SharedOidSearch_ReleaseIterator(
										&persistentDatabaseSharedData->databaseDirSearchTable,
										(SharedOidSearchObjHeader **) &dirIterateDatabaseDirEntry);
		Assert(dirIterateDatabaseDirEntry == NULL);
	}

	PersistentDatabase_DirIterateMoveAway(dirIterateDatabaseDirEntry);
}

static void
AtProcExit_PersistentDatabase(int code, Datum arg)
{
	PersistentDatabase_ReleaseDirIterator();
}

void
PersistentDatabase_DirIterateInit(void)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	Assert(persistentDatabaseSharedData != NULL);

	PersistentDatabase_VerifyInitScan();

	PersistentDatabase_ReleaseDirIterator();

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	if (!iterateOnShmemExitArmed)
	{
		on_shmem_exit(AtProcExit_PersistentDatabase, 0);
		iterateOnShmemExitArmed = true;
	}

	dirIterateDatabaseDirEntry = NULL;

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;
}


bool
PersistentDatabase_DirIterateNext(
								  DbDirNode *dbDirNode,

								  PersistentFileSysState *state,

								  ItemPointer persistentTid,

								  int64 *persistentSerialNum)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	DatabaseDirEntry prevDatabaseDirEntry;

	Assert(persistentDatabaseSharedData != NULL);

	PersistentDatabase_VerifyInitScan();

	MemSet(dbDirNode, 0, sizeof(DbDirNode));

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	while (true)
	{
		prevDatabaseDirEntry = dirIterateDatabaseDirEntry;
		SharedOidSearch_Iterate(
								&persistentDatabaseSharedData->databaseDirSearchTable,
								(SharedOidSearchObjHeader **) &dirIterateDatabaseDirEntry);

		PersistentDatabase_DirIterateMoveAway(prevDatabaseDirEntry);

		if (dirIterateDatabaseDirEntry == NULL)
		{
			READ_PERSISTENT_STATE_ORDERED_UNLOCK;
			return false;
		}

		*state = dirIterateDatabaseDirEntry->state;
		if (*state == PersistentFileSysState_Free)
		{
			/* UNDONE: Or, PinCount > 1 */
			Assert(dirIterateDatabaseDirEntry->iteratorRefCount > 0);
			continue;
		}

		dbDirNode->tablespace = dirIterateDatabaseDirEntry->header.oid2;
		dbDirNode->database = dirIterateDatabaseDirEntry->header.oid1;

		*persistentTid = dirIterateDatabaseDirEntry->persistentTid;
		*persistentSerialNum = dirIterateDatabaseDirEntry->persistentSerialNum;

		dirIterateDatabaseDirEntry->iteratorRefCount++;
		break;
	}

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	return true;
}

void
PersistentDatabase_DirIterateClose(void)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	Assert(persistentDatabaseSharedData != NULL);

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_VerifyInitScan();

	PersistentDatabase_ReleaseDirIterator();

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;
}


bool
PersistentDatabase_DbDirExistsUnderLock(
										DbDirNode *dbDirNode)
{
	DatabaseDirEntry databaseDirEntry;

	PersistentDatabase_VerifyInitScan();

	databaseDirEntry =
		(DatabaseDirEntry)
		SharedOidSearch_Find(
							 &persistentDatabaseSharedData->databaseDirSearchTable,
							 dbDirNode->database,
							 dbDirNode->tablespace);

	return (databaseDirEntry != NULL);
}


extern void
PersistentDatabase_Reset(void)
{
	DatabaseDirEntry databaseDirEntry;

	databaseDirEntry = NULL;
	SharedOidSearch_Iterate(
							&persistentDatabaseSharedData->databaseDirSearchTable,
							(SharedOidSearchObjHeader **) &databaseDirEntry);
	while (true)
	{
		PersistentFileSysObjName fsObjName;

		DatabaseDirEntry nextDatabaseDirEntry;

		if (databaseDirEntry == NULL)
		{
			break;
		}

		PersistentFileSysObjName_SetDatabaseDir(
												&fsObjName,
												 /* tablespaceOid */ databaseDirEntry->header.oid2,
												 /* databaseOid */ databaseDirEntry->header.oid1);

		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Persistent database directory: Resetting '%s' serial number " INT64_FORMAT " at TID %s",
				 PersistentFileSysObjName_ObjectName(&fsObjName),
				 databaseDirEntry->persistentSerialNum,
				 ItemPointerToString(&databaseDirEntry->persistentTid));

		nextDatabaseDirEntry = databaseDirEntry;
		SharedOidSearch_Iterate(
								&persistentDatabaseSharedData->databaseDirSearchTable,
								(SharedOidSearchObjHeader **) &nextDatabaseDirEntry);

		SharedOidSearch_Delete(
							   &persistentDatabaseSharedData->databaseDirSearchTable,
							   &databaseDirEntry->header);

		databaseDirEntry = nextDatabaseDirEntry;
	}
}



/* ----------------------------------------------------------------------------- */
/*  Helpers  */
/* ----------------------------------------------------------------------------- */

static void
PersistentDatabase_LookupExistingDbDir(
									   DbDirNode *dbDirNode,

									   DatabaseDirEntry *databaseDirEntry)
{

	PersistentDatabase_VerifyInitScan();

	*databaseDirEntry =
		(DatabaseDirEntry)
		SharedOidSearch_Find(
							 &persistentDatabaseSharedData->databaseDirSearchTable,
							 dbDirNode->database,
							 dbDirNode->tablespace);
	if (*databaseDirEntry == NULL)
		elog(ERROR, "Persistent database entry '%s' expected to exist",
			 GetDatabasePath(dbDirNode->database, dbDirNode->tablespace));
}

/*
 * We pass in changable columns like mirrorExistenceState, parentXid, etc instead
 * of keep them in our DatabaseDirEntry to avoid stale data.
 */
static void
PersistentDatabase_AddTuple(
							DatabaseDirEntry databaseDirEntry,

							int64 createMirrorDataLossTrackingSessionNum,

							MirroredObjectExistenceState mirrorExistenceState,

							int32 reserved,

							TransactionId parentXid,

							bool flushToXLog)
 /* When true, the XLOG record for this change will be flushed to disk. */
{
	Oid			tablespaceOid = databaseDirEntry->header.oid2;
	Oid			databaseOid = databaseDirEntry->header.oid1;

	Datum		values[Natts_gp_persistent_database_node];

	GpPersistentDatabaseNode_SetDatumValues(
											values,
											tablespaceOid,
											databaseOid,
											databaseDirEntry->state,
											createMirrorDataLossTrackingSessionNum,
											mirrorExistenceState,
											reserved,
											parentXid,
											 /* persistentSerialNum */ 0);
	/* This will be set by PersistentFileSysObj_AddTuple. */

	PersistentFileSysObj_AddTuple(
								  PersistentFsObjType_DatabaseDir,
								  values,
								  flushToXLog,
								  &databaseDirEntry->persistentTid,
								  &databaseDirEntry->persistentSerialNum);
}

/* ----------------------------------------------------------------------------- */
/*  State Change  */
/* ----------------------------------------------------------------------------- */

/*
 * Indicate we intend to create a relation file as part of the current transaction.
 *
 * An XLOG IntentToCreate record is generated that will guard the subsequent file-system
 * create in case the transaction aborts.
 *
 * After 1 or more calls to this routine to mark intention about relation files that are going
 * to be created, call ~_DoPendingCreates to do the actual file-system creates.  (See its
 * note on XLOG flushing).
 */
void
PersistentDatabase_MarkCreatePending(
									 DbDirNode *dbDirNode,
 /* The tablespace and database OIDs for the create. */

									 MirroredObjectExistenceState mirrorExistenceState,

									 ItemPointer persistentTid,
 /* TID of the gp_persistent_database_node tuple for the rel file */

									 int64 *persistentSerialNum,

									 bool flushToXLog)
 /* When true, the XLOG record for this change will be flushed to disk. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	DatabaseDirEntry databaseDirEntry;
	SharedOidSearchAddResult addResult;

	PersistentFileSysObjName fsObjName;
	TransactionId topXid;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
		return;
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(
											&fsObjName,
											dbDirNode->tablespace,
											dbDirNode->database);

	topXid = GetTopTransactionId();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	databaseDirEntry =
		(DatabaseDirEntry)
		SharedOidSearch_Find(
							 &persistentDatabaseSharedData->databaseDirSearchTable,
							 dbDirNode->database,
							 dbDirNode->tablespace);
	if (databaseDirEntry != NULL)
		elog(ERROR, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

	addResult =
		SharedOidSearch_Add(
							&persistentDatabaseSharedData->databaseDirSearchTable,
							dbDirNode->database,
							dbDirNode->tablespace,
							(SharedOidSearchObjHeader **) &databaseDirEntry);
	if (addResult == SharedOidSearchAddResult_NoMemory)
		elog(ERROR, "Out of shared-memory for persistent databaseS");
	else if (addResult == SharedOidSearchAddResult_Exists)
		elog(PANIC, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));
	else
		Assert(addResult == SharedOidSearchAddResult_Ok);

	databaseDirEntry->state = PersistentFileSysState_CreatePending;

	databaseDirEntry->iteratorRefCount = 0;

	PersistentDatabase_AddTuple(
								databaseDirEntry,
								 /* createMirrorDataLossTrackingSessionNum */ 0,
								mirrorExistenceState,
								 /* reserved */ 0,
								 /* parentXid */ topXid,
								flushToXLog);

	*persistentTid = databaseDirEntry->persistentTid;
	*persistentSerialNum = databaseDirEntry->persistentSerialNum;

	/*
	 * This XLOG must be generated under the persistent write-lock.
	 */
#ifdef MASTER_MIRROR_SYNC
	mmxlog_log_create_database(dbDirNode->tablespace, dbDirNode->database);
#endif

	SIMPLE_FAULT_INJECTOR(FaultBeforePendingDeleteDatabaseEntry);

	/*
	 * MPP-18228 To make adding 'Create Pending' entry to persistent table and
	 * adding to the PendingDelete list atomic
	 */
	PendingDelete_AddCreatePendingEntryWrapper(
											   &fsObjName,
											   persistentTid,
											   *persistentSerialNum);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}

void
PersistentDatabase_AddCreated(
							  DbDirNode *dbDirNode,
 /* The tablespace and database OIDs for the create. */

							  MirroredObjectExistenceState mirrorExistenceState,

							  ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

							  bool flushToXLog)
 /* When true, the XLOG record for this change will be flushed to disk. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;
	SharedOidSearchAddResult addResult;

	int64		persistentSerialNum;

	if (!Persistent_BeforePersistenceWork())
		elog(ERROR, "We can only add to persistent meta-data when special states");

	/* Verify PersistentFileSysObj_BuildInitScan has been called. */
	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	databaseDirEntry =
		(DatabaseDirEntry)
		SharedOidSearch_Find(
							 &persistentDatabaseSharedData->databaseDirSearchTable,
							 dbDirNode->database,
							 dbDirNode->tablespace);
	if (databaseDirEntry != NULL)
		elog(ERROR, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

	addResult =
		SharedOidSearch_Add(
							&persistentDatabaseSharedData->databaseDirSearchTable,
							dbDirNode->database,
							dbDirNode->tablespace,
							(SharedOidSearchObjHeader **) &databaseDirEntry);
	if (addResult == SharedOidSearchAddResult_NoMemory)
		elog(ERROR, "Out of shared-memory for persistent databaseS");
	else if (addResult == SharedOidSearchAddResult_Exists)
		elog(PANIC, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));
	else
		Assert(addResult == SharedOidSearchAddResult_Ok);

	databaseDirEntry->state = PersistentFileSysState_Created;

	databaseDirEntry->iteratorRefCount = 0;

	PersistentDatabase_AddTuple(
								databaseDirEntry,
								 /* createMirrorDataLossTrackingSessionNum */ 0,
								mirrorExistenceState,
								 /* reserved */ 0,
								InvalidTransactionId,
								flushToXLog);

	*persistentTid = databaseDirEntry->persistentTid;
	persistentSerialNum = databaseDirEntry->persistentSerialNum;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: Add '%s' in state 'Created', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));
}

void
xlog_create_database(DbDirNode *db)
{
	DatabaseDirEntry dbe;
	SharedOidSearchAddResult addResult;

	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentDatabase_VerifyInitScan();

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	dbe = (DatabaseDirEntry) SharedOidSearch_Find(
												  &persistentDatabaseSharedData->databaseDirSearchTable,
												  db->database,
												  db->tablespace);
	if (dbe != NULL)
		elog(ERROR, "persistent database entry '%s' already exists "
			 "in state '%s'",
			 GetDatabasePath(
							 db->database,
							 db->tablespace),
			 PersistentFileSysObjState_Name(dbe->state));

	addResult = SharedOidSearch_Add(
									&persistentDatabaseSharedData->databaseDirSearchTable,
									db->database,
									db->tablespace,
									(SharedOidSearchObjHeader **) &dbe);

	if (addResult == SharedOidSearchAddResult_NoMemory)
		elog(ERROR, "out of shared-memory for persistent databases");
	else if (addResult == SharedOidSearchAddResult_Exists)
		elog(PANIC, "persistent database entry '%s' already exists in "
			 "state '%s'",
			 GetDatabasePath(
							 db->database,
							 db->tablespace),
			 PersistentFileSysObjState_Name(dbe->state));
	else
		Insist(addResult == SharedOidSearchAddResult_Ok);

	dbe->state = PersistentFileSysState_Created;

	dbe->iteratorRefCount = 0;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;
}


/* ----------------------------------------------------------------------------- */
/*  Transaction End   */
/* ----------------------------------------------------------------------------- */

/*
 * Indicate the transaction commited and the relation is officially created.
 */
void
PersistentDatabase_Created(
						   DbDirNode *dbDirNode,
 /* The tablespace and database OIDs for the created relation. */

						   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

						   int64 persistentSerialNum,
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

						   bool retryPossible)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_LookupExistingDbDir(
										   dbDirNode,
										   &databaseDirEntry);

	if (databaseDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "Persistent database entry %s expected to be in 'Create Pending' state (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

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

	databaseDirEntry->state = PersistentFileSysState_Created;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: '%s' changed state from 'Create Pending' to 'Created', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

/*
 * Iterate over gp_persistent_database_node, removing all
 * references to dbid.
 */
void
PersistentDatabase_RemoveSegment(int16 dbid, bool ismirror)
{
	Relation	rel;
	HeapScanDesc scandesc;
	HeapTuple	tuple;

	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	PersistentDatabase_VerifyInitScan();

	rel = heap_open(GpPersistentDatabaseNodeRelationId,
					AccessExclusiveLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
	{
		Form_gp_persistent_database_node form =
		(Form_gp_persistent_database_node) GETSTRUCT(tuple);
		Oid			tblspcoid = form->tablespace_oid;
		Oid			dboid = form->database_oid;
		int64		serial = form->persistent_serial_num;

		PersistentFileSysObjName fsObjName;

		/*
		 * We need to perform this table for all (database, tablespace) pairs
		 * that are defined in the persistent table.
		 */
		PersistentFileSysObjName_SetDatabaseDir(&fsObjName,
												tblspcoid,
												dboid);

		PersistentFileSysObj_RemoveSegment(&fsObjName,
										   &tuple->t_self,
										   serial,
										   dbid,
										   ismirror,
										    /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	heap_endscan(scandesc);
	heap_close(rel, NoLock);
}

void
PersistentDatabase_ActivateStandby(int16 newmaster, int16 oldmaster)
{
	Relation	rel;
	HeapScanDesc scandesc;
	HeapTuple	tuple;

	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
		elog(ERROR, "persistent table changes forbidden");

	PersistentDatabase_VerifyInitScan();

	rel = heap_open(GpPersistentDatabaseNodeRelationId,
					AccessExclusiveLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
	{
		Form_gp_persistent_database_node form =
		(Form_gp_persistent_database_node) GETSTRUCT(tuple);
		Oid			tblspcoid = form->tablespace_oid;
		Oid			dboid = form->database_oid;
		int64		serial = form->persistent_serial_num;

		PersistentFileSysObjName fsObjName;

		/*
		 * We need to perform this table for all (database, tablespace) pairs
		 * that are defined in the persistent table.
		 */
		PersistentFileSysObjName_SetDatabaseDir(&fsObjName,
												tblspcoid,
												dboid);

		PersistentFileSysObj_ActivateStandby(&fsObjName,
											 &tuple->t_self,
											 serial,
											 newmaster,
											 oldmaster,
											  /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	heap_endscan(scandesc);
	heap_close(rel, NoLock);
}

void
PersistentDatabase_AddMirrorAll(
								int16 pridbid,
								int16 mirdbid)
{
	Relation	rel;
	HeapScanDesc scandesc;
	HeapTuple	tuple;

	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "skipping persistent database mirror addition");

		return;
	}

	PersistentDatabase_VerifyInitScan();

	rel = heap_open(GpPersistentDatabaseNodeRelationId,
					AccessExclusiveLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
	{
		Form_gp_persistent_database_node form =
		(Form_gp_persistent_database_node) GETSTRUCT(tuple);
		Oid			tblspcoid = form->tablespace_oid;
		Oid			dboid = form->database_oid;
		int64		serial = form->persistent_serial_num;

		PersistentFileSysObjName fsObjName;

		PersistentFileSysObjName_SetDatabaseDir(&fsObjName,
												tblspcoid,
												dboid);

		PersistentFileSysObj_AddMirror(&fsObjName,
									   &tuple->t_self,
									   serial,
									   pridbid,
									   mirdbid,
									   NULL,
									   true,
									    /* flushToXlog */ false);
	}

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	heap_endscan(scandesc);
	heap_close(rel, NoLock);
}

/*
 * Indicate we intend to drop a relation file as part of the current transaction.
 *
 * This relation file to drop will be listed inside a commit, distributed commit, a distributed
 * prepared, and distributed commit prepared XOG records.
 *
 * For any of the commit type records, once that XLOG record is flushed then the actual
 * file-system delete will occur.  The flush guarantees the action will be retried after system
 * crash.
 */
PersistentFileSysObjStateChangeResult
PersistentDatabase_MarkDropPending(
								   DbDirNode *dbDirNode,
 /* The tablespace and database OIDs for the drop. */

								   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

								   int64 persistentSerialNum,
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

								   bool retryPossible)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return false;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_LookupExistingDbDir(
										   dbDirNode,
										   &databaseDirEntry);

	if (databaseDirEntry->state != PersistentFileSysState_CreatePending &&
		databaseDirEntry->state != PersistentFileSysState_Created)
		elog(ERROR, "Persistent database entry %s expected to be in 'Create Pending' or 'Created' state (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

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

	databaseDirEntry->state = PersistentFileSysState_DropPending;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: '%s' changed state from 'Create Pending' to 'Aborting Create', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

/*
 * Indicate we are aborting the create of a relation file.
 *
 * This state will make sure the relation gets dropped after a system crash.
 */
PersistentFileSysObjStateChangeResult
PersistentDatabase_MarkAbortingCreate(
									  DbDirNode *dbDirNode,
 /* The tablespace and database OIDs for the aborting create. */

									  ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

									  int64 persistentSerialNum,
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

									  bool retryPossible)
{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return false;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_LookupExistingDbDir(
										   dbDirNode,
										   &databaseDirEntry);

	if (databaseDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "Persistent database entry %s expected to be in 'Create Pending' (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));


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

	databaseDirEntry->state = PersistentFileSysState_AbortingCreate;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: '%s' changed state from 'Create Pending' to 'Aborting Create', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));

	return stateChangeResult;
}

static void
PersistentDatabase_DroppedVerifiedActionCallback(
												 PersistentFileSysObjName *fsObjName,

												 ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the relation. */

												 int64 persistentSerialNum,
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

												 PersistentFileSysObjVerifyExpectedResult verifyExpectedResult)
{
	DbDirNode  *dbDirNode = PersistentFileSysObjName_GetDbDirNodePtr(fsObjName);

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
			mmxlog_log_remove_database(dbDirNode->tablespace, dbDirNode->database);
#endif

			break;

		default:
			elog(ERROR, "Unexpected persistent object verify expected result: %d",
				 verifyExpectedResult);
	}
}

/*
 * Indicate we physically removed the relation file.
 */
void
PersistentDatabase_Dropped(
						   DbDirNode *dbDirNode,
 /* The tablespace and database OIDs for the dropped relation. */

						   ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

						   int64 persistentSerialNum)
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	PersistentFileSysState oldState;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_LookupExistingDbDir(
										   dbDirNode,
										   &databaseDirEntry);

	if (databaseDirEntry->state != PersistentFileSysState_DropPending &&
		databaseDirEntry->state != PersistentFileSysState_AbortingCreate)
		elog(ERROR, "Persistent database entry %s expected to be in 'Drop Pending' or 'Aborting Create' (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
										 &fsObjName,
										 persistentTid,
										 persistentSerialNum,
										 PersistentFileSysState_Free,
										  /* retryPossible */ false,
										  /* flushToXlog */ false,
										 &oldState,
										 PersistentDatabase_DroppedVerifiedActionCallback);

	databaseDirEntry->state = PersistentFileSysState_Free;

	if (databaseDirEntry->iteratorRefCount == 0)
		SharedOidSearch_Delete(
							   &persistentDatabaseSharedData->databaseDirSearchTable,
							   &databaseDirEntry->header);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: '%s' changed state from '%s' to (Free), serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 PersistentFileSysObjState_Name(oldState),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

bool
PersistentDatabase_DirIsCreated(DbDirNode *dbDirNode)
{
	READ_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	DatabaseDirEntry databaseDirEntry;
	bool		result;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before "
				 "persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
		return true;
	}

	PersistentDatabase_VerifyInitScan();

	READ_PERSISTENT_STATE_ORDERED_LOCK;

	databaseDirEntry =
		(DatabaseDirEntry)
		SharedOidSearch_Find(
							 &persistentDatabaseSharedData->databaseDirSearchTable,
							 dbDirNode->database,
							 dbDirNode->tablespace);
	result = (databaseDirEntry != NULL);
	if (result &&
		databaseDirEntry->state != PersistentFileSysState_Created &&
		databaseDirEntry->state != PersistentFileSysState_CreatePending)
		elog(ERROR, "Persistent database entry %s expected to be in 'Create Pending' or 'Created' (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

	READ_PERSISTENT_STATE_ORDERED_UNLOCK;

	return result;
}

void
PersistentDatabase_MarkJustInTimeCreatePending(
											   DbDirNode *dbDirNode,

											   MirroredObjectExistenceState mirrorExistenceState,

											   ItemPointer persistentTid,
 /* TID of the gp_persistent_database_node tuple for the rel file */

											   int64 *persistentSerialNum)

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	SharedOidSearchAddResult addResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	databaseDirEntry =
		(DatabaseDirEntry)
		SharedOidSearch_Find(
							 &persistentDatabaseSharedData->databaseDirSearchTable,
							 dbDirNode->database,
							 dbDirNode->tablespace);
	if (databaseDirEntry != NULL)
	{
		/*
		 * An existence check should have been done before calling this
		 * routine.
		 */
		elog(ERROR, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));
	}

	addResult =
		SharedOidSearch_Add(
							&persistentDatabaseSharedData->databaseDirSearchTable,
							dbDirNode->database,
							dbDirNode->tablespace,
							(SharedOidSearchObjHeader **) &databaseDirEntry);
	if (addResult == SharedOidSearchAddResult_NoMemory)
		elog(ERROR, "Out of shared-memory for persistent databases");
	else if (addResult == SharedOidSearchAddResult_Exists)
		elog(PANIC, "Persistent database entry '%s' already exists in state '%s'",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));
	else
		Assert(addResult == SharedOidSearchAddResult_Ok);

	databaseDirEntry->state = PersistentFileSysState_JustInTimeCreatePending;

	PersistentDatabase_AddTuple(
								databaseDirEntry,
								 /* createMirrorDataLossTrackingSessionNum */ 0,
								mirrorExistenceState,
								 /* reserved */ 0,
								 /* parentXid */ InvalidTransactionId,
								 /* flushToXLog */ true);


	*persistentTid = databaseDirEntry->persistentTid;
	*persistentSerialNum = databaseDirEntry->persistentSerialNum;

	/*
	 * This XLOG must be generated under the persistent write-lock.
	 *
	 * UNDONE: Just-in-time creation is a strange case.  What if we can't
	 * create the directory??
	 */
#ifdef MASTER_MIRROR_SYNC
	mmxlog_log_create_database(dbDirNode->tablespace,
							   dbDirNode->database);
#endif

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: Add '%s' in state 'Just-In-Time Create Pending', mirror existence state '%s', serial number " INT64_FORMAT " at TID %s",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 MirroredObjectExistenceState_Name(mirrorExistenceState),
			 *persistentSerialNum,
			 ItemPointerToString(persistentTid));
}

/*
 * Indicate the non-transaction just-in-time database create was successful.
 */
void
PersistentDatabase_JustInTimeCreated(
									 DbDirNode *dbDirNode,

									 ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

									 int64 persistentSerialNum)
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_LookupExistingDbDir(
										   dbDirNode,
										   &databaseDirEntry);

	if (databaseDirEntry->state != PersistentFileSysState_JustInTimeCreatePending)
		elog(ERROR, "Persistent database entry %s expected to be in 'Just-In-Time Create Pending' state (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
										 &fsObjName,
										 persistentTid,
										 persistentSerialNum,
										 PersistentFileSysState_Created,
										  /* retryPossible */ false,
										  /* flushToXlog */ false,
										  /* oldState */ NULL,
										  /* verifiedActionCallback */ NULL);

	databaseDirEntry->state = PersistentFileSysState_Created;

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: '%s' changed state from 'Just-In-Time Create Pending' to 'Created', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}

/*
 * Indicate the non-transaction just-in-time database create was NOT successful.
 */
void
PersistentDatabase_AbandonJustInTimeCreatePending(
												  DbDirNode *dbDirNode,

												  ItemPointer persistentTid,
 /* TID of the gp_persistent_rel_files tuple for the rel file */

												  int64 persistentSerialNum)
 /* Serial number for the relation.	Distinquishes the uses of the tuple. */

{
	WRITE_PERSISTENT_STATE_ORDERED_LOCK_DECLARE;

	PersistentFileSysObjName fsObjName;

	DatabaseDirEntry databaseDirEntry;

	PersistentFileSysObjStateChangeResult stateChangeResult;

	if (Persistent_BeforePersistenceWork())
	{
		if (Debug_persistent_print)
			elog(Persistent_DebugPrintLevel(),
				 "Skipping persistent database '%s' because we are before persistence work",
				 GetDatabasePath(
								 dbDirNode->database,
								 dbDirNode->tablespace));

		return;

		/*
		 * The initdb process will load the persistent table once we out of
		 * bootstrap mode.
		 */
	}

	PersistentDatabase_VerifyInitScan();

	PersistentFileSysObjName_SetDatabaseDir(&fsObjName, dbDirNode->tablespace, dbDirNode->database);

	WRITE_PERSISTENT_STATE_ORDERED_LOCK;

	PersistentDatabase_LookupExistingDbDir(
										   dbDirNode,
										   &databaseDirEntry);

	if (databaseDirEntry->state != PersistentFileSysState_JustInTimeCreatePending)
		elog(ERROR, "Persistent database entry %s expected to be in 'Just-In-Time Create Pending' state (actual state '%s')",
			 GetDatabasePath(
							 dbDirNode->database,
							 dbDirNode->tablespace),
			 PersistentFileSysObjState_Name(databaseDirEntry->state));

	stateChangeResult =
		PersistentFileSysObj_StateChange(
										 &fsObjName,
										 persistentTid,
										 persistentSerialNum,
										 PersistentFileSysState_Free,
										  /* retryPossible */ false,
										  /* flushToXlog */ false,
										  /* oldState */ NULL,
										  /* verifiedActionCallback */ NULL);

	databaseDirEntry->state = PersistentFileSysState_Free;

	if (databaseDirEntry->iteratorRefCount == 0)
		SharedOidSearch_Delete(
							   &persistentDatabaseSharedData->databaseDirSearchTable,
							   &databaseDirEntry->header);

	WRITE_PERSISTENT_STATE_ORDERED_UNLOCK;

	if (Debug_persistent_print)
		elog(Persistent_DebugPrintLevel(),
			 "Persistent database directory: Abandon '%s' in state 'Just-In-Time Create Pending', serial number " INT64_FORMAT " at TID %s (State-Change result '%s')",
			 PersistentFileSysObjName_ObjectName(&fsObjName),
			 persistentSerialNum,
			 ItemPointerToString(persistentTid),
			 PersistentFileSysObjStateChangeResult_Name(stateChangeResult));
}


/* ----------------------------------------------------------------------------- */
/*  Shmem */
/* ----------------------------------------------------------------------------- */

static Size
PersistentDatabase_SharedDataSize(void)
{
	return MAXALIGN(
					offsetof(PersistentDatabaseSharedData, databaseDirSearchTable)) +
		SharedOidSearch_TableLen(
								  /* hashSize */ 127,
								  /* freeObjectCount */ MaxPersistentDatabaseDirectories,
								  /* objectLen */ sizeof(DatabaseDirEntryData));
}

/*
 * Return the required shared-memory size for this module.
 */
Size
PersistentDatabase_ShmemSize(void)
{
	if (MaxPersistentDatabaseDirectories == 0)
		MaxPersistentDatabaseDirectories = gp_max_databases * gp_max_tablespaces;

	/* The shared-memory structure. */
	return PersistentDatabase_SharedDataSize();
}

/*
 * Initialize the shared-memory for this module.
 */
void
PersistentDatabase_ShmemInit(void)
{
	bool		found;

	/* Create the shared-memory structure. */
	persistentDatabaseSharedData =
		(PersistentDatabaseSharedData *)
		ShmemInitStruct("Persistent Database Data",
						PersistentDatabase_SharedDataSize(),
						&found);

	if (!found)
	{
		PersistentFileSysObj_InitShared(
										&persistentDatabaseSharedData->fileSysObjSharedData);

		SharedOidSearch_InitTable(
								  &persistentDatabaseSharedData->databaseDirSearchTable,
								  127,
								  MaxPersistentDatabaseDirectories,
								  sizeof(DatabaseDirEntryData));
	}

	PersistentFileSysObj_Init(
							  &persistentDatabaseData.fileSysObjData,
							  &persistentDatabaseSharedData->fileSysObjSharedData,
							  PersistentFsObjType_DatabaseDir,
							  PersistentDatabase_ScanTupleCallback);


	Assert(persistentDatabaseSharedData != NULL);
}

/*
 * Pass shared data back to the caller. See add_tablespace_data() for why we do
 * it like this.
 */
#ifdef MASTER_MIRROR_SYNC		/* annotation to show that this is just for
								 * mmsync */
void
get_database_data(dbdir_agg_state **das, char *caller)
{
	DatabaseDirEntry databaseDirEntry;

	int			maxCount;

	Assert(*das == NULL);

	mmxlog_add_database_init(das, &maxCount);

	databaseDirEntry = NULL;
	while (true)
	{
		SharedOidSearch_Iterate(
								&persistentDatabaseSharedData->databaseDirSearchTable,
								(SharedOidSearchObjHeader **) &databaseDirEntry);
		if (databaseDirEntry == NULL)
		{
			break;
		}

		mmxlog_add_database(
							das, &maxCount,
							 /* databaseoid */ databaseDirEntry->header.oid1,
							 /* tablespaceoid */ databaseDirEntry->header.oid2,
							caller);
	}
}
#endif
